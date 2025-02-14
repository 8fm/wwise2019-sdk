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

#include "AkPitchShifterFX.h"
#include <math.h>
#include <AK/Tools/Common/AkAssert.h>
#include "AkPitchShifterDSPProcess.h"
#include <AK/AkWwiseSDKVersion.h>

// Plugin mechanism. Dynamic create function whose address must be registered to the FX manager.
AK::IAkPlugin* CreateAkPitchShifterFX( AK::IAkPluginMemAlloc * in_pAllocator )
{
	return AK_PLUGIN_NEW( in_pAllocator, CAkPitchShifterFX( ) );
}


// Constructor.
CAkPitchShifterFX::CAkPitchShifterFX() 
	: m_pParams(NULL)
	, m_pAllocator(NULL)
{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
	m_FXInfo.DryDelay = NULL;
#endif

}

// Destructor.
CAkPitchShifterFX::~CAkPitchShifterFX()
{

}

// Initializes and allocate memory for the effect
AKRESULT CAkPitchShifterFX::Init(	AK::IAkPluginMemAlloc * in_pAllocator,	// Memory allocator interface.
									AK::IAkEffectPluginContext * in_pFXCtx,	// FX Context
									AK::IAkPluginParam * in_pParams,		// Effect parameters.
									AkAudioFormat &	in_rFormat				// Required audio input format.
									)
{
	m_pParams = (CAkPitchShifterFXParams*)(in_pParams);
	m_pAllocator = in_pAllocator;
	m_FXInfo.uTotalNumChannels = in_rFormat.GetNumChannels();

	// Initial computations
	m_pParams->GetParams( &m_FXInfo.Params );
	m_FXInfo.PrevParams = m_FXInfo.Params;
	m_FXInfo.uSampleRate = in_rFormat.uSampleRate;

	AkChannelConfig channelConfig = in_rFormat.channelConfig; 
	ComputeNumProcessedChannels( channelConfig );
	ComputeTailLength();
	AKRESULT eResult = InitPitchVoice();
	if ( eResult != AK_Success )
		return eResult;
	eResult = InitDryDelay();
	if ( eResult != AK_Success )
		return eResult;

	m_pParams->m_ParamChangeHandler.ResetAllParamChanges();

	AK_PERF_RECORDING_RESET();

	return AK_Success;
}

void CAkPitchShifterFX::ComputeNumProcessedChannels( AkChannelConfig in_channelConfig )
{
	switch ( m_FXInfo.Params.eInputType )
	{
	case AKINPUTTYPE_ASINPUT:
		m_FXInfo.configProcessed = m_FXInfo.Params.bProcessLFE ? in_channelConfig : in_channelConfig.RemoveLFE();
		break;
	case AKINPUTTYPE_CENTER:
		m_FXInfo.configProcessed.SetStandard((m_FXInfo.Params.bProcessLFE ? AK_SPEAKER_SETUP_1_1_CENTER : AK_SPEAKER_SETUP_1_0_CENTER) & in_channelConfig.uChannelMask);
		break;
	case AKINPUTTYPE_STEREO:
		m_FXInfo.configProcessed.SetStandard((m_FXInfo.Params.bProcessLFE ? AK_SPEAKER_SETUP_2_1 : AK_SPEAKER_SETUP_2_0) & in_channelConfig.uChannelMask);
		break;
	case AKINPUTCHANNELTYPE_3POINT0:
		m_FXInfo.configProcessed.SetStandard((m_FXInfo.Params.bProcessLFE ? AK_SPEAKER_SETUP_3_1 : AK_SPEAKER_SETUP_3_0) & in_channelConfig.uChannelMask);
		break;
	case AKINPUTTYPE_4POINT0:
		m_FXInfo.configProcessed.SetStandard((m_FXInfo.Params.bProcessLFE ? AK_SPEAKER_SETUP_4_1 : AK_SPEAKER_SETUP_4_0) & in_channelConfig.uChannelMask);
		break;
	case AKINPUTTYPE_5POINT0:
		m_FXInfo.configProcessed.SetStandard((m_FXInfo.Params.bProcessLFE ? AK_SPEAKER_SETUP_5_1 : AK_SPEAKER_SETUP_5_0) & in_channelConfig.uChannelMask);
		break;
	}
}

void CAkPitchShifterFX::ComputeTailLength()
{
	m_FXInfo.uTailLength = (AkUInt32)(m_FXInfo.Params.fDelayTime*0.001f*m_FXInfo.uSampleRate);
}

AKRESULT CAkPitchShifterFX::InitPitchVoice()
{
	if ( m_FXInfo.configProcessed.uNumChannels > 0 )
	{
		AKRESULT eResult = m_FXInfo.PitchShifter.Init( m_pAllocator, m_FXInfo.Params.fDelayTime, m_FXInfo.configProcessed.uNumChannels, m_FXInfo.uSampleRate );
		if ( eResult != AK_Success )
			return eResult;
		m_FXInfo.PitchShifter.SetPitchFactor( m_FXInfo.Params.Voice.fPitchFactor );

		if ( m_FXInfo.Params.Voice.Filter.eFilterType != AKFILTERTYPE_NONE )
		{
			eResult = m_FXInfo.Filter.Init( m_pAllocator, m_FXInfo.configProcessed.uNumChannels, true );
			if ( eResult != AK_Success )
				return eResult;

			m_FXInfo.Filter.ComputeCoefs(	
				(::DSP::FilterType)(m_FXInfo.Params.Voice.Filter.eFilterType-1),
				(AkReal32) m_FXInfo.uSampleRate,
				m_FXInfo.Params.Voice.Filter.fFilterFrequency,
				m_FXInfo.Params.Voice.Filter.fFilterGain,
				m_FXInfo.Params.Voice.Filter.fFilterQFactor );
		}
	}

	return AK_Success;
}

void CAkPitchShifterFX::TermPitchVoice()
{
	if ( m_FXInfo.configProcessed.uNumChannels > 0 )
	{
		m_FXInfo.PitchShifter.Term( m_pAllocator );
	}

	if ( m_FXInfo.Filter.IsInitialized() )
	{
		m_FXInfo.Filter.Term( m_pAllocator );
	}
}

void CAkPitchShifterFX::ResetPitchVoice()
{
	if ( m_FXInfo.configProcessed.uNumChannels > 0 )
	{
		m_FXInfo.PitchShifter.Reset();
		m_FXInfo.Filter.Reset();
	}
}

AKRESULT CAkPitchShifterFX::InitDryDelay()
{
	const AkUInt32 uDryDelay = m_FXInfo.uTailLength/2;
	if ( m_FXInfo.Params.bSyncDry )
	{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
		m_FXInfo.DryDelay = (::DSP::CDelayLight *) AK_PLUGIN_ALLOC( m_pAllocator, sizeof( ::DSP::CDelayLight ) * m_FXInfo.uTotalNumChannels );
		if ( m_FXInfo.DryDelay == NULL )
			return AK_InsufficientMemory;
#endif
		for ( AkUInt32 j = 0; j < m_FXInfo.uTotalNumChannels; j++ )
		{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
			AkPlacementNew( m_FXInfo.DryDelay + j ) ::DSP::CDelayLight();
#endif
			AKRESULT eResult = m_FXInfo.DryDelay[j].Init( m_pAllocator, uDryDelay );
			if ( eResult != AK_Success )
				return eResult;
		}
	}
	return AK_Success;
}

void CAkPitchShifterFX::TermDryDelay()
{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
	if ( m_FXInfo.DryDelay )
#endif
	{
		for ( AkUInt32 j = 0; j < m_FXInfo.uTotalNumChannels; j++ )
		{
			m_FXInfo.DryDelay[j].Term( m_pAllocator );
#ifndef AK_VOICE_MAX_NUM_CHANNELS
			m_FXInfo.DryDelay[j].~CDelayLight();
#endif
		}
#ifndef AK_VOICE_MAX_NUM_CHANNELS
		AK_PLUGIN_FREE( m_pAllocator, m_FXInfo.DryDelay );
		m_FXInfo.DryDelay = NULL;
#endif
	}
}

void CAkPitchShifterFX::ResetDryDelay()
{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
	if ( m_FXInfo.DryDelay )
#endif
	{
		for ( AkUInt32 j = 0; j < m_FXInfo.uTotalNumChannels; j++ )
			m_FXInfo.DryDelay[j].Reset( );
	}
}

// Terminates.
AKRESULT CAkPitchShifterFX::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	TermPitchVoice();
	TermDryDelay();
	AK_PLUGIN_DELETE( in_pAllocator, this );
	return AK_Success;
}

// Reset or seek to start.
AKRESULT CAkPitchShifterFX::Reset( )
{
	ResetPitchVoice();
	ResetDryDelay();
	return AK_Success;
}


// Effect info query.
AKRESULT CAkPitchShifterFX::GetPluginInfo( AkPluginInfo & out_rPluginInfo )
{
	out_rPluginInfo.eType = AkPluginTypeEffect;
	out_rPluginInfo.bIsInPlace = true;
	out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
	return AK_Success;
}


void CAkPitchShifterFX::Execute( AkAudioBuffer * io_pBuffer )
{
	// Compute changes if parameters have changed
	m_pParams->GetParams( &m_FXInfo.Params );

	if ( m_pParams->m_ParamChangeHandler.HasAnyChanged() )
	{
		if ( m_pParams->m_ParamChangeHandler.HasChanged( AKPITCHSHIFTERPARAMID_DELAYTIME ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKPITCHSHIFTERPARAMID_INPUT ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKPITCHSHIFTERPARAMID_PROCESSLFE ) )
		{
			TermPitchVoice();
			TermDryDelay();
			ComputeTailLength();
			ComputeNumProcessedChannels( io_pBuffer->GetChannelConfig() );
			AKRESULT eResult = InitPitchVoice();
			if ( eResult != AK_Success )
				return; // passthrough
			eResult = InitDryDelay();
			if ( eResult != AK_Success )
				return; // passthrough
			ResetPitchVoice();
			ResetDryDelay();
		}

		if ( m_pParams->m_ParamChangeHandler.HasChanged( AKPITCHSHIFTERPARAMID_SYNCDRY ) )
		{
			TermDryDelay( );
			AKRESULT eResult = InitDryDelay( ); 
			if ( eResult != AK_Success )
				return;
			ResetDryDelay();
		}

		if ( m_pParams->m_ParamChangeHandler.HasChanged( AKPITCHSHIFTERPARAMID_PITCH ) )
		{
			m_FXInfo.PitchShifter.SetPitchFactor( m_FXInfo.Params.Voice.fPitchFactor );
		}

		if ( m_pParams->m_ParamChangeHandler.HasChanged( AKPITCHSHIFTERPARAMID_FILTERTYPE ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKPITCHSHIFTERPARAMID_FILTERFREQUENCY ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKPITCHSHIFTERPARAMID_FILTERGAIN ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKPITCHSHIFTERPARAMID_FILTERQFACTOR ) )
		{
			if ( m_FXInfo.Params.Voice.Filter.eFilterType != AKFILTERTYPE_NONE )
			{
				if ( !m_FXInfo.Filter.IsInitialized() )
				{
					AKRESULT eResult = m_FXInfo.Filter.Init( m_pAllocator, m_FXInfo.configProcessed.uNumChannels, true );
					if ( eResult != AK_Success )
						return;
				}

				m_FXInfo.Filter.ComputeCoefs(	
					(DSP::FilterType)(m_FXInfo.Params.Voice.Filter.eFilterType-1),
					(AkReal32) m_FXInfo.uSampleRate,
					m_FXInfo.Params.Voice.Filter.fFilterFrequency,
					m_FXInfo.Params.Voice.Filter.fFilterGain,
					m_FXInfo.Params.Voice.Filter.fFilterQFactor );
			}
			else
			{
				m_FXInfo.Filter.Term( m_pAllocator );
			}
		}
	}

	m_pParams->m_ParamChangeHandler.ResetAllParamChanges();

	AK_PERF_RECORDING_START( "PitchShifter", 25, 30 );

	AkReal32 * pfBufferStorage = NULL;
	void * pfTwoPassStorage = NULL;
	if ( m_FXInfo.configProcessed.uNumChannels > 0 )
	{
		// Allocate temporary storage for wet path
		pfBufferStorage = (AkReal32*) AK_PLUGIN_ALLOC( m_pAllocator, io_pBuffer->MaxFrames() * sizeof(AkReal32) );
		if ( !pfBufferStorage )
			return; // Pipeline cannot handle errors here, just play dry signal when out of memory

#ifdef AKDELAYPITCHSHIFT_USETWOPASSALGO
		// Allocate temporary storage for 2-pass computations to avoid LHS
		pfTwoPassStorage  = (void*) AK_PLUGIN_ALLOC( m_pAllocator, io_pBuffer->MaxFrames() * (2*sizeof(AkReal32)+4*sizeof(AkInt32)) );
		if ( !pfTwoPassStorage )
			return; // Pipeline cannot handle errors here, just play dry signal when out of memory
#endif
	}
	
	AkPitchShifterDSPProcess( io_pBuffer, m_FXInfo, pfBufferStorage, pfTwoPassStorage );

	if ( pfBufferStorage )
		AK_PLUGIN_FREE( m_pAllocator, pfBufferStorage );
	if ( pfTwoPassStorage )
		AK_PLUGIN_FREE( m_pAllocator, pfTwoPassStorage );
	
	AK_PERF_RECORDING_STOP( "PitchShifter", 25, 30 );
}
