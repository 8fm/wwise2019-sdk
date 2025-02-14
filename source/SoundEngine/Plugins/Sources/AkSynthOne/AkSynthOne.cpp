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

//#define AK_ENABLE_PERF_RECORDING
#include "AkSynthOneParams.h"
#include "AkSynthOne.h"

#include <AK/Tools/Common/AkAssert.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/AkWwiseSDKVersion.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//-----------------------------------------------------------------------------
// Name: CreateSynthOne
// Desc: Plugin mechanism. Dynamic create function whose address must be 
//       registered to the FX manager.
//-----------------------------------------------------------------------------
AK::IAkPlugin* CreateAkSynthOne(AK::IAkPluginMemAlloc * in_pAllocator)
{
	return AK_PLUGIN_NEW( in_pAllocator, CAkSynthOne() );
}

//-----------------------------------------------------------------------------
// Name: CAkSynthOne
// Desc: Constructor.
//-----------------------------------------------------------------------------
CAkSynthOne::CAkSynthOne()
	: m_pParams( NULL )
	, m_pAllocator( NULL )
	, m_pSourceCtx( NULL )
	, m_fSampleRate( 0.f )
	, m_uScratchSize( 0 )
	, m_pScratchBuf( NULL )
{
}

//-----------------------------------------------------------------------------
// Name: ~CAkSynthOne
// Desc: Destructor.
//-----------------------------------------------------------------------------
CAkSynthOne::~CAkSynthOne()
{
}

//-----------------------------------------------------------------------------
// Name: Init
// Desc: Initialize tone generator 
//-----------------------------------------------------------------------------
AKRESULT CAkSynthOne::Init(	AK::IAkPluginMemAlloc *			in_pAllocator,		// Memory allocator interface.
							AK::IAkSourcePluginContext *	in_pSourceCtx,		// Source FX context
							AK::IAkPluginParam *			in_pParams,			// Effect parameters.
							AkAudioFormat &					io_rFormat			// Supported audio output format.
						  )
{
	m_pAllocator = in_pAllocator;
	m_pSourceCtx = in_pSourceCtx;

	// Set parameters access.
	AKASSERT( NULL != in_pParams );
	m_pParams = reinterpret_cast<CAkSynthOneParams*>(in_pParams);
	if( m_pParams == NULL )
		return AK_Fail;

	// Output format set to Mono native by default.
	// Output to native format STRONGLY recommended but other formats are supported.

	// Save audio format internally
	m_bOverSampling = m_pParams->GetOverSampling();
	m_fSampleRate = static_cast<AkReal32>( io_rFormat.uSampleRate );

	// Adjust MIDI event if using MIDI note for frequency, but MIDI event is invalid.
	m_midiEvent = m_pSourceCtx->GetMidiEvent();
	if( ! m_midiEvent.IsValid() ||
		! m_midiEvent.IsNoteOnOff() )
	{
		// Set to note C3 (note 48)
		m_midiEvent.MakeNoteOff();
		m_midiEvent.SetNoteNumber( 48 );
	}

	// Check validity of wave types
	if( m_pParams->m_Params.eOsc1Waveform != AkSynthOneWaveType_Sine &&
		m_pParams->m_Params.eOsc1Waveform != AkSynthOneWaveType_Triangle &&
		m_pParams->m_Params.eOsc1Waveform != AkSynthOneWaveType_Square &&
		m_pParams->m_Params.eOsc1Waveform != AkSynthOneWaveType_Sawtooth )
	{
		AKASSERT( !"Invalid osc1 waveform." );
	}

	if( m_pParams->m_Params.eOsc2Waveform != AkSynthOneWaveType_Sine &&
		m_pParams->m_Params.eOsc2Waveform != AkSynthOneWaveType_Triangle &&
		m_pParams->m_Params.eOsc2Waveform != AkSynthOneWaveType_Square &&
		m_pParams->m_Params.eOsc2Waveform != AkSynthOneWaveType_Sawtooth )
	{
		AKASSERT( !"Invalid osc2 waveform." );
	}

	// Check validity of noise type
	if ( m_pParams->m_Params.eNoiseType != AkSynthOneNoiseType_Pink &&
		 m_pParams->m_Params.eNoiseType != AkSynthOneNoiseType_White &&
		 m_pParams->m_Params.eNoiseType != AkSynthOneNoiseType_Red &&
		 m_pParams->m_Params.eNoiseType != AkSynthOneNoiseType_Purple )
	{
		 AKASSERT( !"Invalid noise type." );
	}

	// Reset DSP processing context
	m_SynthOneDsp.Init(m_pParams, in_pSourceCtx, m_fSampleRate, m_midiEvent);

	AK_PERF_RECORDING_RESET();

	return AK_Success;
}

//-----------------------------------------------------------------------------
// Name: Term.
// Desc: Term. The effect must destroy itself herein
//-----------------------------------------------------------------------------
AKRESULT CAkSynthOne::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AK_PLUGIN_DELETE( in_pAllocator, this );
	return AK_Success;
}

//-----------------------------------------------------------------------------
// Name: Reset
// Desc: Reset or seek to start (looping).
//-----------------------------------------------------------------------------
AKRESULT CAkSynthOne::Reset( void )
{
	m_SynthOneDsp.Reset( m_pParams );
	return AK_Success;
}

//-----------------------------------------------------------------------------
// Name: GetEffectType
// Desc: Effect information query.
//-----------------------------------------------------------------------------
AKRESULT CAkSynthOne::GetPluginInfo( AkPluginInfo & out_rPluginInfo )
{
	out_rPluginInfo.eType = AkPluginTypeSource;
	out_rPluginInfo.bIsInPlace = true;
	out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
	return AK_Success;
}

//-----------------------------------------------------------------------------
// Name: AllocateScratchBuf
// Desc: Allocates memory for scratch buffer.
//-----------------------------------------------------------------------------
bool CAkSynthOne::AllocateScratchBuf( AkUInt32 in_uNumSample, bool in_bIsOverSampling )
{
	AkUInt32 uNumSample = m_bOverSampling ?
		in_uNumSample * OSCOVERSAMPLING : in_uNumSample;

#ifdef SYNTHONE_SIMD
	m_uScratchSize = 4 * uNumSample * sizeof(AkReal32);
#else
	m_uScratchSize = 2 * uNumSample * sizeof(AkReal32);
#endif

	m_pScratchBuf = (AkReal32*)AK_PLUGIN_ALLOC( m_pAllocator, m_uScratchSize );
	if( ! m_pScratchBuf )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Name: ReleaseScratchBuf
// Desc: Releases memory for scratch buffer.
//-----------------------------------------------------------------------------
void CAkSynthOne::ReleaseScratchBuf()
{
	if( m_pScratchBuf != NULL )
	{
		AK_PLUGIN_FREE( m_pAllocator, m_pScratchBuf );
		m_pScratchBuf = NULL;
	}

	m_uScratchSize = 0;
}

//-----------------------------------------------------------------------------
// Name: Execute
// Desc: Source effect processing.
//-----------------------------------------------------------------------------
void CAkSynthOne::Execute(	AkAudioBuffer * io_pBufferOut )
{
	bool bRc = false;

	// Figure out if we can produce a full buffer before the end of the sound
	AkUInt32 uNumSample = io_pBufferOut->MaxFrames();	// Infinite, always produce maximum

	// Allocate scratch buffer
	if( ! AllocateScratchBuf( io_pBufferOut->MaxFrames(), m_bOverSampling ) )
	{
		io_pBufferOut->uValidFrames = 0;
		return;
	}

	// Main processing
	if( uNumSample != 0 )
	{
		AK_PERF_RECORDING_START( "SynthOne", 25, 30 );

		m_SynthOneDsp.PreProcess( m_pParams, uNumSample );
		bRc = m_SynthOneDsp.Process( uNumSample, io_pBufferOut->GetChannel(0), m_pScratchBuf );

		// Notify buffers of updated production
		io_pBufferOut->uValidFrames = bRc ? (AkUInt16)uNumSample : 0;
		io_pBufferOut->eState = AK_DataReady;	// still more to go

		AK_PERF_RECORDING_STOP( "SynthOne", 25, 30 );
	}

	// Release scratch buffer
	ReleaseScratchBuf();
}
