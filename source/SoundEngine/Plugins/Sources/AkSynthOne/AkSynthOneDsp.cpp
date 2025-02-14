/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2019 Audiokinetic Inc.
 ***********************************************************************/

#include "AkSynthOneParams.h"
#include "AkSynthOne.h"

#include "AkWaveTables.h"

#include <AK/Tools/Common/AkAssert.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "IAkGlobalPluginContextEx.h"


//-----------------------------------------------------------------------------
// Defines.
//-----------------------------------------------------------------------------

static const AkReal32 FILTERCUTFREQ = 18000.f;	// Oversampling low pass filter cutoff frequency

// Type of audio samples
#define AUDIO_SAMPLE_TYPE_REAL

// Wavetable synthesis overview:
// In wavetable synthesis, we store a single period of the necessary waveform type in a table of values
// computed once at effect initialization time. After that we just need to sequentially go through the
// table and retrieve the necessary (precomputed) amplitude value (table lookup synthesis). The rate at 
// which the table is scan determines the fundamental frequency of the oscillator. To achieve higher
// frequencies we just skip over some values (effectively downsampling the signal) and the opposite for
// lower frequencies. The rate at which we scan the wavetable will change if we perform a frequency sweep
// and thus we need to be able to convert an instantaneous frequency desired into a wave table sampling
// increment. This relation is given below.

// Sampling increment relation to oscillator frequency:
// SI = N * f0
//			--
//			fs
// where SI is the sampling increment (how many samples to jump each time in the wave table)
// f0 is the desired oscillator fundamental frequency
// fs is the sampling frequency
// N is the size of the wave table used
// Obviously SI will not necessarily land on an integer value and we must thus extrapolate the
// imaginary sample at that floating point location. In this implementation, this is done through
// linear interpolation. This technique (along with the table size) governs the overall oscillator
// SNR, documented elsewhere.

// Waveform generation recipes:
// Because frequency may change at any time we cannot band-limit the wave. We thus choose to add a fixed number
// of harmonics. An oversampled intermediary representation that is filtered prior to decimation is used.
// Because of Fourier's theory stating that all signals may be
// constructed as a sum of sinusoids at proper amplitude, frequency, and phase we use some common recipes
// to build the necessary waveforms

// Square wave -> All odd harmonics with amplitude 1/k (k is the harmonic number)
// Triangle wave -> All odd harmonics with amplitude 1/k^2 (k is the harmonic number)
// Sawtooth wave -> All harmonics with amplitude 1/k (k is the harmonic number)


//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------
DSP::CAkColoredNoise::AkNoiseColor GetColoredNoise( AkSynthOneNoiseType in_eSynthOneNoise )
{
	DSP::CAkColoredNoise::AkNoiseColor eColoredNoise = DSP::CAkColoredNoise::NOISECOLOR_WHITE;
	switch( in_eSynthOneNoise )
	{
	case AkSynthOneNoiseType_Red: eColoredNoise = DSP::CAkColoredNoise::NOISECOLOR_RED; break;
	case AkSynthOneNoiseType_Pink: eColoredNoise = DSP::CAkColoredNoise::NOISECOLOR_PINK; break;
	case AkSynthOneNoiseType_White: eColoredNoise = DSP::CAkColoredNoise::NOISECOLOR_WHITE; break;
	case AkSynthOneNoiseType_Purple: eColoredNoise = DSP::CAkColoredNoise::NOISECOLOR_PURPLE; break;
	default: AKASSERT( false );
	}

	return eColoredNoise;
}


//-----------------------------------------------------------------------------
// Name: CAkSynthOneDsp
// Desc: Constructor.
//-----------------------------------------------------------------------------
CAkSynthOneDsp::CAkSynthOneDsp()
	: m_fSampleRate( 0.f )
	, m_fInvSampleRate( 0.f )
	, m_fOversampledRate( 0.f )
	, m_fInvOversampledRate( 0.f )
	, m_fOscMaxFreq( 0.f )
	, m_bOverSampling( true )
	, m_noiseColor( AkSynthOneNoiseType_White )
	, m_pSourceCtx(NULL)
{
	m_procOsc1.eOsc = AkSynthOneOsc1;
	m_procOsc2.eOsc = AkSynthOneOsc2;

	AKPLATFORM::AkMemSet( &m_bqFilters, 0, sizeof m_bqFilters );
}

//-----------------------------------------------------------------------------
// Name: ~CAkSynthOneDsp
// Desc: Destructor.
//-----------------------------------------------------------------------------
CAkSynthOneDsp::~CAkSynthOneDsp()
{
}

//-----------------------------------------------------------------------------
// Initialize tone generator 
//-----------------------------------------------------------------------------
void CAkSynthOneDsp::Init(CAkSynthOneParams* in_pParams, AK::IAkSourcePluginContext * in_pSourceCtx, AkReal32 in_fSampleRate, const AkMidiEventEx& in_midiEvent)
{
	m_pSourceCtx = in_pSourceCtx;

	// Output format set to Mono native by default.
	// Output to native format STRONGLY recommended but other formats are supported.

	// Save audio format internally
	m_bOverSampling = in_pParams->GetOverSampling();
	m_fSampleRate = in_fSampleRate;
	m_fInvSampleRate = 1.f / m_fSampleRate;
	m_fOversampledRate = m_fSampleRate * (IsOverSampling() ? OSCOVERSAMPLING : 1);
	m_fInvOversampledRate = 1.f / m_fOversampledRate;

	// Nyquist frequency
	m_fOscMaxFreq = AkMin( m_fSampleRate/2.f, SYNTHONE_FREQUENCY_MAX );

	// Adjust MIDI event if using MIDI note for frequency, but MIDI event is invalid.
	m_midiEvent = in_midiEvent;
	if( ! m_midiEvent.IsValid() ||
		! m_midiEvent.IsNoteOnOff() )
	{
		// Set to note C3 (note 48)
		m_midiEvent.MakeNoteOff();
		m_midiEvent.SetNoteNumber( 48 );
	}

	// Initialize pink noise variables
	m_noiseColor = in_pParams->m_Params.eNoiseType;
	m_noiseGen.Init(GetColoredNoise(m_noiseColor), (AkUInt32)m_fSampleRate, static_cast<AK::IAkGlobalPluginContextEx *>(in_pSourceCtx->GlobalContext())->Random());

	// Reset filter memory and set coefficients
	for( unsigned int i = 0; i < NUMFILTERSECTIONS; i++ )
	{
		//The cutoff frequency was computed for 48kHz.  Transform it for the current sample rate.
		m_bqFilters[i].ComputeCoefs( DSP::FilterType_LowPass, m_fOversampledRate, FILTERCUTFREQ * m_fSampleRate/48000.f );
	}
}

//-----------------------------------------------------------------------------
// Helper to reset ramps.
//-----------------------------------------------------------------------------
AkForceInline void _RampReset( ValueRamp& io_ramp, AkReal32 in_fValue )
{
	io_ramp.RampSetup( in_fValue, in_fValue, 0 );
}

//-----------------------------------------------------------------------------
// Reset or seek to start (looping).
//-----------------------------------------------------------------------------
void CAkSynthOneDsp::Reset( CAkSynthOneParams* in_pParams )
{
	// Reset oscillator counters
	m_procOsc1.Reset();
	m_procOsc2.Reset();

	// Level ramp initialization
	if( in_pParams )
	{
		_RampReset( m_procOsc1.levelRamp, in_pParams->GetLevel( AkSynthOneSel_Osc1 ) );
		_RampReset( m_procOsc2.levelRamp, in_pParams->GetLevel( AkSynthOneSel_Osc2 ) );
		_RampReset( m_levelRampNoise, in_pParams->GetLevel( AkSynthOneSel_Noise ) );
		_RampReset( m_levelRampOutput, in_pParams->GetLevel( AkSynthOneSel_Output ) );

		_RampReset( m_procOsc1.transRamp, in_pParams->GetTranspose( AkSynthOneSel_Osc1 ) );
		_RampReset( m_procOsc2.transRamp, in_pParams->GetTranspose( AkSynthOneSel_Osc2 ) );

		_RampReset( m_procOsc1.pwmRamp, in_pParams->GetPwm( AkSynthOneSel_Osc1 ) );
		_RampReset( m_procOsc2.pwmRamp, in_pParams->GetPwm( AkSynthOneSel_Osc2 ) );

		_RampReset( m_fmAmountRamp, in_pParams->GetFmAmount() );
	}

	// Reset filter memory and set coefficients
	for ( unsigned int i = 0; i < NUMFILTERSECTIONS; i++ )
		m_bqFilters[i].Reset();
}

//-----------------------------------------------------------------------------
// Ramp setup helpers
//-----------------------------------------------------------------------------
AkForceInline void _RampSetup( ValueRamp& io_ramp, AkReal32 in_fParam, AkUInt32 in_uSteps )
{
	io_ramp.RampSetup( io_ramp.GetCurrent(), in_fParam, in_uSteps );
}

//-----------------------------------------------------------------------------
// Setup value ramps for shared parameters.
//-----------------------------------------------------------------------------
void CAkSynthOneDsp::RampSetup( CAkSynthOneParams* in_pParams, AkUInt32 in_uNumSample )
{
	// Setup level ramps on one frames

	AkUInt32 uRampSteps = in_uNumSample;

	_RampSetup( m_levelRampNoise, in_pParams->GetLevel( AkSynthOneSel_Noise ), uRampSteps );
	_RampSetup( m_levelRampOutput, in_pParams->GetLevel( AkSynthOneSel_Output ), uRampSteps );

	// The following ramps are on one oversampled frame

	uRampSteps = in_uNumSample * (IsOverSampling() ? OSCOVERSAMPLING : 1);

	// Oscillator output level ramps
	_RampSetup( m_procOsc1.levelRamp, in_pParams->GetLevel( AkSynthOneSel_Osc1 ), uRampSteps );
	_RampSetup( m_procOsc2.levelRamp, in_pParams->GetLevel( AkSynthOneSel_Osc2 ), uRampSteps );

	// Translation ramps
	_RampSetup( m_procOsc1.transRamp, in_pParams->GetTranspose( AkSynthOneSel_Osc1 ), uRampSteps );
	_RampSetup( m_procOsc2.transRamp, in_pParams->GetTranspose( AkSynthOneSel_Osc2 ), uRampSteps );

	// PWM ramps
	_RampSetup( m_procOsc1.pwmRamp, in_pParams->GetPwm( AkSynthOneSel_Osc1 ), uRampSteps );
	_RampSetup( m_procOsc2.pwmRamp, in_pParams->GetPwm( AkSynthOneSel_Osc2 ), uRampSteps );

	// FM ramp (also one frame)
	_RampSetup( m_fmAmountRamp, in_pParams->GetFmAmount(), uRampSteps );
}

//-----------------------------------------------------------------------------
// Setup value ramps for shared parameters.
//-----------------------------------------------------------------------------
void CAkSynthOneDsp::RampDone()
{
	m_levelRampNoise.RampDone();
	m_levelRampOutput.RampDone();

	// Oscillator output level ramps
	m_procOsc1.levelRamp.RampDone();
	m_procOsc2.levelRamp.RampDone();

	// Translation ramps
	m_procOsc1.transRamp.RampDone();
	m_procOsc2.transRamp.RampDone();

	// PWM ramps
	m_procOsc1.pwmRamp.RampDone();
	m_procOsc2.pwmRamp.RampDone();

	// FM ramp (also one frame)
	m_fmAmountRamp.RampDone();
}

//-----------------------------------------------------------------------------
// Get frequency of associated MIDI note.
AkReal32 CAkSynthOneDsp::GetMidiNoteFrequency()
{
	AKASSERT( m_midiEvent.IsNoteOnOff() );
	return powf( 2, ((AkReal32)m_midiEvent.GetNoteNumber() - 69) / 12 ) * 440;
}

//-----------------------------------------------------------------------------
// Clip frequency value to valid range
void CAkSynthOneDsp::ClipFrequencyToValidRange( AkReal32& io_fFreqValue )
{
	// Ensure start frequency is in valid range
	if ( io_fFreqValue < SYNTHONE_FREQUENCY_MIN )
	{
		io_fFreqValue = SYNTHONE_FREQUENCY_MIN;
	}
	else if ( io_fFreqValue >= m_fOscMaxFreq )
	{
		// Just below Nyquist
		io_fFreqValue = m_fOscMaxFreq - 1;
	}
}

//-----------------------------------------------------------------------------
// Setup variables used for oscillator processing.
//	MUST NOT BE CALLED BY SPU job!!
//-----------------------------------------------------------------------------
void CAkSynthOneDsp::OscSetup( CAkSynthOneParams* in_pParams )
{
	AKASSERT( in_pParams );
	if( ! in_pParams )
		return;

	//------------------------------------------------------------------
	// Get base frequency to use
	m_fBaseFreq = SYNTHONE_FREQUENCY_MIN;

	AkSynthOneFrequencyMode eFreqMode = in_pParams->m_Params.eFreqMode;

	if( eFreqMode == AkSynthOneFrequencyMode_Specify )
		m_fBaseFreq = in_pParams->GetBaseFrequency();
	else if( eFreqMode == AkSynthOneFrequencyMode_MidiNote )
		m_fBaseFreq = GetMidiNoteFrequency();
	else
		AKASSERT( false );

	ClipFrequencyToValidRange( m_fBaseFreq );

	//------------------------------------------------------------------
	// Is in mixing mode or ring mode?
	m_bMixingMode = ( in_pParams->m_Params.eOpMode == AkSynthOneOperationMode_Mix );

	//------------------------------------------------------------------
	// Determine if oscillator samples are inverted
	m_procOsc1.bInvert = in_pParams->GetOscInvert( AkSynthOneSel_Osc1 );
	m_procOsc2.bInvert = in_pParams->GetOscInvert( AkSynthOneSel_Osc2 );

	//------------------------------------------------------------------
	// Determine which wave table to use
	m_procOsc1.eWavType = in_pParams->m_Params.eOsc1Waveform;
	m_procOsc2.eWavType = in_pParams->m_Params.eOsc2Waveform;
}

//-----------------------------------------------------------------------------
// Setup variables used for oscillator processing.
//	MUST NOT BE CALLED BY SPU job!!
//-----------------------------------------------------------------------------
void CAkSynthOneDsp::NoiseSetup( CAkSynthOneParams* in_pParams, AkReal32 in_fSampleRate )
{
	AkSynthOneNoiseType eSynthOneNoise = in_pParams->m_Params.eNoiseType;

	if( eSynthOneNoise != m_noiseColor )
	{
		m_noiseColor = eSynthOneNoise;
		m_noiseGen.Init(GetColoredNoise(m_noiseColor), (AkUInt32)in_fSampleRate, static_cast<AK::IAkGlobalPluginContextEx *>(m_pSourceCtx->GlobalContext())->Random());
	}
}

//-----------------------------------------------------------------------------
// Prologue to Process function call
//-----------------------------------------------------------------------------
void CAkSynthOneDsp::PreProcess( CAkSynthOneParams* in_pParams, AkUInt32 in_uNumSample )
{
	// Setup for processing
	AKASSERT( in_pParams );
	if( in_pParams )
	{
		// Setup static params
		OscSetup( in_pParams );

		// Setup ramps values for RTPC'd params
		RampSetup( in_pParams, in_uNumSample );

		// Noise setup
		NoiseSetup( in_pParams, m_fSampleRate );
	}
}

//-----------------------------------------------------------------------------
// Source effect processing.
//-----------------------------------------------------------------------------
bool CAkSynthOneDsp::Process( AkUInt32 in_uNumSamples, AkSampleType* out_pBuf, AkReal32* in_pScratch )
{
	bool rc = false;

	// Main processing
	if( in_uNumSamples != 0 && out_pBuf && in_pScratch )
	{
		// Produce buffer
		rc = ProcessDsp( in_uNumSamples, out_pBuf, in_pScratch );
	}

	// Update context regardless
	PostProcess();

	return rc;
}

//-----------------------------------------------------------------------------
// Epilogue to Process function call
//-----------------------------------------------------------------------------
void CAkSynthOneDsp::PostProcess()
{
	RampDone();
}

//-----------------------------------------------------------------------------
#include "AkSynthOne.inl"
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
void SelectWavTable( AkSynthOneOscProc& io_procOsc )
{
	// Fill wavetable depending on waveform type and set proper execute function
	switch ( io_procOsc.eWavType )
	{
	case AkSynthOneWaveType_Sine: io_procOsc.pWavTable = fSineTable; break;
	case AkSynthOneWaveType_Triangle: io_procOsc.pWavTable = fTriangleTable; break;
	case AkSynthOneWaveType_Square: io_procOsc.pWavTable = fSquareTable; break;
	case AkSynthOneWaveType_Sawtooth: io_procOsc.pWavTable = fSawtoothTable; break;
	default: AKASSERT(!"Unknown oscillator 1 wave type");
	}
}

//-----------------------------------------------------------------------------
// Produce buffer samples.
AkNoInline bool CAkSynthOneDsp::ProcessDsp( AkUInt32 in_uNumSamples, AkSampleType* out_pBuf, AkReal32* in_pScratch )
{
	// Select wav tables to use
	SelectWavTable( m_procOsc1 );
	SelectWavTable( m_procOsc2 );

	if( !m_procOsc1.pWavTable || !m_procOsc2.pWavTable )
		return false;

#ifdef SYNTHONE_SIMD
	SimdProcDsp( in_uNumSamples, out_pBuf, in_pScratch );
#else
	ScalarProcDsp( in_uNumSamples, out_pBuf, in_pScratch );
#endif

	return true;
}

