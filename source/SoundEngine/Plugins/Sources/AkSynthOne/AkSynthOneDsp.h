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

#ifndef _AK_SYNTHONE_DSP_H_
#define _AK_SYNTHONE_DSP_H_

#include "AkSynthOneParams.h"
#include "BiquadFilter.h"
#include "AkColoredNoise.h"
#include "AkMidiStructs.h"

// Determine how processing will be performed
#if defined(AKSIMD_V4F32_SUPPORTED)
	#define SYNTHONE_SIMD
#else
	#define SYNTHONE_SCALAR
#endif

// Oscillator SNR is governed by table size and sampling increment rounding technique.
// Since we use linear interpolation on the sampling increment the SNR is given by
// 12 * (log2(TABLESIZE) - 1) thus 96dB for a size of 512
#define OSC_WAVETABLESIZE			(512)
#define OSC_WAVETABLESIZEMASK		(OSC_WAVETABLESIZE-1)

// Random number generation defines
static const AkReal32 ONEOVERMAXRANDVAL	= ( 1.f / 0x80000000 ); // 2^31

// At 48 kHz output -> maximum fundamental frequency allowed is 12000
// With 15 harmonics we must able to represent signal up to 15 * 12000 = 180000 to avoid aliased signal
// 180000 / 48 000 corresponds to a 3.75 x oversampling requirement
// Overshoot to avoid critical sampling
#define OSCOVERSAMPLING		4

// Using an array of 2nd order Butterworth filter to get steeper slope
// Each section is -12 dB/octave
#define NUMFILTERSECTIONS	3

// Used to determine if a module should be processed or not (under this level equals inaudible)
#define LEVELCUTOFF			(0.000016f)

//-----------------------------------------------------------------------------
// Class used to ramp RTPC'd params.  Ramping is always done over one frame!
//-----------------------------------------------------------------------------
class ValueRamp
{
public:

	// Constructor method.
	ValueRamp()
		: m_fStep( 0.f )	// Signed increment
		, m_fTarget( 0.f )	// Target gain for ramping
		, m_fCurrent( 0.f )	// Current interpolated value
	{}

	// Destructor method.
	~ValueRamp() {}

	// Initial parameter interpolation ramp setup.
	inline void RampSetup( 
		AkReal32 fStart,	// Start value of ramp
		AkReal32 fTarget,	// End value of ramp
		AkUInt32 uNumStep )	// Number of Tick() calls to reach target
	{
		if( m_fTarget != fTarget )
		{
			if( uNumStep != 0 )
			{
				m_fCurrent = fStart;
				m_fTarget = fTarget;
				AkReal32 fDiff = m_fTarget - m_fCurrent;
				m_fStep = fDiff / uNumStep;
			}
			else
			{
				m_fTarget = fTarget;
				m_fCurrent = fTarget;
				m_fStep = 0.f;
			}
		}
	}

	inline void RampDone()
	{
		m_fCurrent = m_fTarget;
		m_fStep = 0.f;
	}

	// Process a single interpolation frame.
	AkForceInline AkReal32 Step()
	{
		m_fCurrent += m_fStep;
		return m_fCurrent;
	}

	// Helpers
	AkForceInline AkReal32 GetTarget() const { return m_fTarget; }
	AkForceInline AkReal32 GetCurrent() const { return m_fCurrent; }
	AkForceInline AkReal32 GetIncrement() const { return m_fStep; }

private:

	AkReal32	m_fStep;		// Signed increment
	AkReal32	m_fTarget;		// Target for interpolation ramp
	AkReal32	m_fCurrent;		// Current interpolated value

} AK_ALIGN_DMA;

//-----------------------------------------------------------------------------
// Oscillator processing context.
//-----------------------------------------------------------------------------
enum AkSynthOneOsc
{
	AkSynthOneOsc1 = 0,
	AkSynthOneOsc2
};

struct AkSynthOneOscProc
{
	AkSynthOneOscProc()
		: pWavTable( NULL )
		, eWavType( AkSynthOneWaveType_Sine )
		, fPhase( 0.f )
		, bInvert( false )
	{}

	void Reset() { fPhase = 0.f; }

	AkSynthOneOsc eOsc;
	AkReal32* pWavTable;
	AkSynthOneWaveType eWavType;
	AkReal32 fPhase;
	ValueRamp transRamp;
	ValueRamp levelRamp;
	ValueRamp pwmRamp;
	bool bInvert;

} AK_ALIGN_DMA;

//-----------------------------------------------------------------------------
// SynthOne implementation (source effect).
//-----------------------------------------------------------------------------
class CAkSynthOneDsp
{
public:

    CAkSynthOneDsp();
    ~CAkSynthOneDsp();

	// Functions called by CAkSynthOne equivalents.
	void Init(CAkSynthOneParams* in_pParams, AK::IAkSourcePluginContext * in_pSourceCtx, AkReal32 in_fSampleRate, const AkMidiEventEx& in_midiEvent);
	void Reset( CAkSynthOneParams* in_pParams );

	void PreProcess( CAkSynthOneParams* in_pParams, AkUInt32 in_uNumSample );
	void RampSetup( CAkSynthOneParams* in_pParams, AkUInt32 in_uNumSample );
	void OscSetup( CAkSynthOneParams* in_pParams );
	void NoiseSetup( CAkSynthOneParams* in_pParams, AkReal32 in_fSampleRate );

	// Frame processing.
	bool Process( AkUInt32 in_uNumSamples, AkSampleType* out_pBuf, AkReal32* in_pScratch );
	void RampDone();
	void PostProcess();

private:

#ifdef SYNTHONE_SIMD

	//-----------------------------------------------------------------------------
	// SIMD processing path
	template < typename FmPolicy, typename PwmPolicy >
	void SimdProcOscX( const AkUInt32 in_uNumSample, const AkReal32 in_fBaseFreq, AkSynthOneOscProc& io_oscProc, AkReal32* AK_RESTRICT out_pOsc, AkReal32* AK_RESTRICT in_pFm, AkReal32* AK_RESTRICT in_pScratch );

	//-----------------------------------------------------------------------------
	void SimdProcOsc( const AkUInt32 in_uNumSample, const AkReal32 in_fBaseFreq, AkSynthOneOscProc& io_oscProc, AkReal32* out_pOsc, AkReal32* in_pFm, AkReal32* in_pScratch );
	void SimdProcOscFm( const AkUInt32 in_uNumSample, const AkReal32 in_fBaseFreq, AkSynthOneOscProc& io_oscProc, AkReal32* out_pOsc, AkReal32* in_pFm, AkReal32* in_pScratch );
	void SimdProcOscPwm( const AkUInt32 in_uNumSample, const AkReal32 in_fBaseFreq, AkSynthOneOscProc& io_oscProc, AkReal32* out_pOsc, AkReal32* in_pFm, AkReal32* in_pScratch );
	void SimdProcOscFmPwm( const AkUInt32 in_uNumSample, const AkReal32 in_fBaseFreq, AkSynthOneOscProc& io_oscProc, AkReal32* out_pOsc, AkReal32* in_pFm, AkReal32* in_pScratch );

	typedef void (CAkSynthOneDsp::*FnSimdOscProc)( const AkUInt32 in_uNumSample, const AkReal32 in_fBaseFreq, AkSynthOneOscProc& io_oscProc, AkReal32* out_pOsc, AkReal32* in_pFm, AkReal32* in_pScratch );

	FnSimdOscProc GetSimdOscProcFn( const AkSynthOneOscProc& in_oscProc );

	//-----------------------------------------------------------------------------
	template < typename SampleCbxPolicy >
	void CombineSamples( AkUInt32 in_uNumSample, AkReal32* AK_RESTRICT in_pOsc1, AkReal32* AK_RESTRICT in_pOsc2, AkReal32* AK_RESTRICT out_pBuf );
	void SimdProcOsc( AkUInt32 in_uNumSample, AkReal32* out_pBuf, AkReal32* in_pScratch );

	//-----------------------------------------------------------------------------
	void BypassNoise( AkUInt32 in_uNumSample, AkReal32* AK_RESTRICT io_pBuf );
	void GenNoiseBuf( AkUInt32 in_uNumSample, AkReal32* AK_RESTRICT out_pRand );
	void ProcessNoise( AkUInt32 in_uNumSample, AkReal32* AK_RESTRICT io_pBuf, AkReal32* AK_RESTRICT in_pRand );

	void FinalOutput( AkUInt32 in_uNumSample, AkReal32* AK_RESTRICT io_pBuf, AkReal32* AK_RESTRICT in_pScratch );

	void SimdProcDsp( AkUInt32 in_uNumSamples, AkReal32* out_pBuf, AkReal32* in_pScratch );

	//-----------------------------------------------------------------------------

#else

	//-----------------------------------------------------------------------------
	// Friend class used for processing
	friend class CAkSynthOneScalar;

	//-----------------------------------------------------------------------------
	// Performs oscillator synthesis
	AkForceInline void ScalarProcDsp( AkUInt32 in_uNumSamples, AkSampleType* out_pBuf, AkReal32* in_pScratch );

	//-----------------------------------------------------------------------------

#endif // SYNTHONE_SIMD

	//-----------------------------------------------------------------------------
	bool ProcessDsp( AkUInt32 in_uNumSamples, AkSampleType* out_pBuf, AkReal32* in_pScratch );

	//-----------------------------------------------------------------------------
	// Functions used to setup processing BEFORE main job is started
	AkForceInline void ClipFrequencyToValidRange( AkReal32& io_fFreqValue );
	AkForceInline AkReal32 GetFrequencyToUse();
	AkForceInline AkReal32 GetMidiNoteFrequency();

	//-----------------------------------------------------------------------------
	// Functions used in both PPU & SPU
	AkForceInline bool IsOverSampling() const { return m_bOverSampling; }
	AkForceInline bool IsMixingMode() const { return m_bMixingMode; }

	//-----------------------------------------------------------------------------

    // Internal state variables.
	AkReal32			m_fSampleRate;			// Sample rate
	AkReal32			m_fInvSampleRate;		// 1 / Sample rate
	AkReal32			m_fOversampledRate;		// Oversampling rate
    AkReal32			m_fInvOversampledRate;	// 1 / Oversampling rate
	AkReal32			m_fOscMaxFreq;			// Oscillator maximum frequency (sampling rate dependent)

	bool				m_bOverSampling;
	bool				m_bMixingMode;

	AkSynthOneOscProc	m_procOsc1;
	AkSynthOneOscProc	m_procOsc2;

	// Ramps for RTPCed values
	ValueRamp m_levelRampNoise;
	ValueRamp m_levelRampOutput;
	ValueRamp m_fmAmountRamp;

	typedef DSP::BiquadFilterMonoSIMD SynthOneFilter;
	SynthOneFilter		m_bqFilters[NUMFILTERSECTIONS];			// Butterworth low pass filters

	// Noise generation params
	AkSynthOneNoiseType		m_noiseColor;
	DSP::CAkColoredNoise	m_noiseGen;

	// Various parameters used during procesing
	AkReal32		m_fBaseFreq;	// Base frequency for each oscillator, before transposition

	// MIDI note used for frequency configuration (eFreqMode = AkSynthOneFrequencyMode_MidiNote).
	AkMidiEventEx	m_midiEvent;
	AK::IAkSourcePluginContext * m_pSourceCtx;
} AK_ALIGN_DMA;

#endif // _AK_SYNTHONE_DSP_H_
