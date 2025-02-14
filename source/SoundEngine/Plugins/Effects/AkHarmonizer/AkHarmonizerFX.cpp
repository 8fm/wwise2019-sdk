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

#include "AkHarmonizerFX.h"
#include <math.h>
#include <AK/Tools/Common/AkAssert.h>
#include "AkHarmonizerDSPProcess.h"
#include <AK/AkWwiseSDKVersion.h>

// Plugin mechanism. Dynamic create function whose address must be registered to the FX manager.
AK::IAkPlugin* CreateAkHarmonizerFX( AK::IAkPluginMemAlloc * in_pAllocator )
{
	return AK_PLUGIN_NEW( in_pAllocator, CAkHarmonizerFX( ) );
}


// Constructor.
CAkHarmonizerFX::CAkHarmonizerFX() 
	: m_pParams(NULL)
	, m_pAllocator(NULL)
{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
	m_FXInfo.DryDelay = NULL;
#endif
}

// Destructor.
CAkHarmonizerFX::~CAkHarmonizerFX()
{

}

// Initializes and allocate memory for the effect
AKRESULT CAkHarmonizerFX::Init(	AK::IAkPluginMemAlloc * in_pAllocator,	// Memory allocator interface.
									AK::IAkEffectPluginContext * in_pFXCtx,	// FX Context
									AK::IAkPluginParam * in_pParams,		// Effect parameters.
									AkAudioFormat &	in_rFormat				// Required audio input format.
									)
{
	m_pParams = (CAkHarmonizerFXParams*)(in_pParams);
	m_pAllocator = in_pAllocator;
	m_FXInfo.uTotalNumChannels = in_rFormat.GetNumChannels();

	// Initial computations
	m_pParams->GetParams( &m_FXInfo.Params );
	m_FXInfo.PrevParams = m_FXInfo.Params;
	m_FXInfo.uSampleRate = in_rFormat.uSampleRate;

	ComputeNumProcessedChannels( in_rFormat.channelConfig );
	ComputeWetPathEnabled( in_rFormat.channelConfig );
	AKRESULT eResult = InitPitchVoices();
	if ( eResult != AK_Success )
		return eResult;
	eResult = InitDryDelay();
	if ( eResult != AK_Success )
		return eResult;

	m_pParams->m_ParamChangeHandler.ResetAllParamChanges();

	AK_PERF_RECORDING_RESET();

	return eResult;
}

void CAkHarmonizerFX::ComputeNumProcessedChannels( AkChannelConfig in_channelConfig )
{
	switch ( m_FXInfo.Params.eInputType )
	{
	case AKINPUTTYPE_ASINPUT:
		m_FXInfo.configProcessed = m_FXInfo.Params.bProcessLFE ? in_channelConfig : in_channelConfig.RemoveLFE();
		break;
	case AKINPUTTYPE_CENTER:
		m_FXInfo.configProcessed.SetStandard( ( m_FXInfo.Params.bProcessLFE ? AK_SPEAKER_SETUP_1_1_CENTER : AK_SPEAKER_SETUP_1_0_CENTER ) & in_channelConfig.uChannelMask );
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
	case AKINPUTTYPE_LEFTONLY:
		m_FXInfo.configProcessed.SetStandard((m_FXInfo.Params.bProcessLFE ? AK_SPEAKER_SETUP_2_1 : AK_SPEAKER_SETUP_2_0) & in_channelConfig.uChannelMask & (AK_SPEAKER_FRONT_LEFT | AK_SPEAKER_LOW_FREQUENCY));
		break;
	}
}

void CAkHarmonizerFX::ComputeWetPathEnabled( AkChannelConfig in_channelConfig )
{
	bool bHasOneVoiceEnabled = false;
	for (AkUInt32 i = 0; i < AKHARMONIZER_NUMVOICES; i++ )
	{	
		if ( m_FXInfo.Params.Voice[i].bEnable )
			bHasOneVoiceEnabled = true;
	}
	// At least one named channel of the signal config and config to be processed must match, or it is identical in case of anonymous configs.
	m_FXInfo.bWetPathEnabled = bHasOneVoiceEnabled && (((in_channelConfig.uChannelMask & m_FXInfo.configProcessed.uChannelMask) != 0) || in_channelConfig == m_FXInfo.configProcessed);
}

AKRESULT CAkHarmonizerFX::InitPitchVoices()
{
	for (AkUInt32 i = 0; i < AKHARMONIZER_NUMVOICES; i++ )
	{
		if ( m_FXInfo.Params.Voice[i].bEnable )
		{	
			AKRESULT eResult = m_FXInfo.PhaseVocoder[i].Init( 
				m_pAllocator, 
				m_FXInfo.configProcessed.uNumChannels,
				m_FXInfo.uSampleRate,
				m_FXInfo.Params.uWindowSize,
				false );	
			if ( eResult != AK_Success )
				return eResult;

			if ( m_FXInfo.Params.Voice[i].Filter.eFilterType != AKFILTERTYPE_NONE && m_FXInfo.configProcessed.uNumChannels )
			{
				eResult = m_FXInfo.Filter[i].Init( m_pAllocator, m_FXInfo.configProcessed.uNumChannels, true );
				if ( eResult != AK_Success )
					return eResult;
				m_FXInfo.Filter[i].ComputeCoefs(	
					(DSP::FilterType)(m_FXInfo.Params.Voice[i].Filter.eFilterType-1),
					(AkReal32) m_FXInfo.uSampleRate,
					m_FXInfo.Params.Voice[i].Filter.fFilterFrequency,
					m_FXInfo.Params.Voice[i].Filter.fFilterGain,
					m_FXInfo.Params.Voice[i].Filter.fFilterQFactor );
			}
		}
	}

	return AK_Success;
}

void CAkHarmonizerFX::TermPitchVoices()
{
	for (AkUInt32 i = 0; i < AKHARMONIZER_NUMVOICES; i++ )
	{
		m_FXInfo.Filter[i].Term( m_pAllocator );
		m_FXInfo.PhaseVocoder[i].Term( m_pAllocator );
	}
}

void CAkHarmonizerFX::ResetPitchVoices()
{
	for ( AkUInt32 i = 0; i < AKHARMONIZER_NUMVOICES; i++ )
	{
		if ( m_FXInfo.Params.Voice[i].bEnable )
		{	
			m_FXInfo.PhaseVocoder[i].Reset();
			m_FXInfo.PhaseVocoder[i].ResetInputFill();
			m_FXInfo.Filter[i].Reset();
		}
	}
}

AKRESULT CAkHarmonizerFX::InitDryDelay()
{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
	if ( m_FXInfo.uTotalNumChannels )
	{
		m_FXInfo.DryDelay = (::DSP::CDelayLight *) AK_PLUGIN_ALLOC( m_pAllocator, sizeof( ::DSP::CDelayLight ) * m_FXInfo.uTotalNumChannels );
		if ( m_FXInfo.DryDelay == NULL )
			return AK_InsufficientMemory;
		for ( AkUInt32 j = 0; j < m_FXInfo.uTotalNumChannels; j++ )
			AkPlacementNew( m_FXInfo.DryDelay + j ) ::DSP::CDelayLight();
	}
#endif

	if ( m_FXInfo.Params.bSyncDry && m_FXInfo.bWetPathEnabled )
	{
		const AkUInt32 uDryDelay = m_FXInfo.Params.uWindowSize;
		for ( AkUInt32 j = 0; j < m_FXInfo.uTotalNumChannels; j++ )
		{
			AKRESULT eResult = m_FXInfo.DryDelay[j].Init( m_pAllocator, uDryDelay );
			if ( eResult != AK_Success )
				return eResult;
		}
	}
	return AK_Success;
}

void CAkHarmonizerFX::TermDryDelay()
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

void CAkHarmonizerFX::ResetDryDelay()
{
	for ( AkUInt32 j = 0; j < m_FXInfo.uTotalNumChannels; j++ )
		m_FXInfo.DryDelay[j].Reset( );
}

// Terminates.
AKRESULT CAkHarmonizerFX::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	TermPitchVoices();
	TermDryDelay();
	AK_PLUGIN_DELETE( in_pAllocator, this );
	return AK_Success;
}

// Reset or seek to start.
AKRESULT CAkHarmonizerFX::Reset( )
{
	ResetPitchVoices();
	ResetDryDelay();
	return AK_Success;
}


// Effect info query.
AKRESULT CAkHarmonizerFX::GetPluginInfo( AkPluginInfo & out_rPluginInfo )
{
	out_rPluginInfo.eType = AkPluginTypeEffect;
	out_rPluginInfo.bIsInPlace = true;
	out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
	return AK_Success;
}

void CAkHarmonizerFX::Execute( AkAudioBuffer * io_pBuffer )
{
	// Compute changes if parameters have changed
	m_pParams->GetParams( &m_FXInfo.Params );

	if ( m_pParams->m_ParamChangeHandler.HasAnyChanged() )
	{	
		// These values cannot support RTPC because this could invalidate the constant global data flow requirement during the changes
		// These losses in input buffering could never be compensated and this would in turn lead to starvation
		// of the algorithm, which is handled through... infinite looping. Consider using an out-of-place node
		// if RTPC on pitch is required.
		if ( m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_VOICE1PITCH ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_VOICE2PITCH ) )
		{
			TermPitchVoices();
			AKRESULT eResult = InitPitchVoices(); 
			if ( eResult != AK_Success )
				return; // passthrough
			ResetPitchVoices();
		}

		if ( m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_WINDOWSIZE ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_VOICE1ENABLE ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_VOICE2ENABLE ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_INPUT ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_PROCESSLFE ) )
		{
			TermPitchVoices();
			TermDryDelay();
			ComputeNumProcessedChannels( io_pBuffer->GetChannelConfig() );
			ComputeWetPathEnabled( io_pBuffer->GetChannelConfig() );
			AKRESULT eResult = InitPitchVoices();
			if ( eResult != AK_Success )
				return; // passthrough
			eResult = InitDryDelay();
			if ( eResult != AK_Success )
				return; // passthrough
			ResetPitchVoices();
			ResetDryDelay();
		}

		if ( m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_SYNCDRY ) )
		{
			TermDryDelay( );
			AKRESULT eResult = InitDryDelay( ); 
			if ( eResult != AK_Success )
				return;
			ResetDryDelay();
		}

		if ( m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_VOICE1FILTERTYPE ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_VOICE1FILTERFREQUENCY ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_VOICE1FILTERGAIN ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_VOICE1FILTERQFACTOR ) )
		{
			if ( m_FXInfo.Params.Voice[0].Filter.eFilterType != AKFILTERTYPE_NONE )
			{
				if (!m_FXInfo.Filter[0].IsInitialized())
				{
					AKRESULT eResult = m_FXInfo.Filter[0].Init( m_pAllocator, m_FXInfo.configProcessed.uNumChannels );
					if ( eResult != AK_Success )
						return;
				}

				m_FXInfo.Filter[0].ComputeCoefs(	
					(DSP::FilterType)(m_FXInfo.Params.Voice[0].Filter.eFilterType-1),
					(AkReal32) m_FXInfo.uSampleRate,
					m_FXInfo.Params.Voice[0].Filter.fFilterFrequency,
					m_FXInfo.Params.Voice[0].Filter.fFilterGain,
					m_FXInfo.Params.Voice[0].Filter.fFilterQFactor );
			}
			else
			{
				 m_FXInfo.Filter[0].Term( m_pAllocator );
			}
		}

		if ( m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_VOICE2FILTERTYPE ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_VOICE2FILTERFREQUENCY ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_VOICE2FILTERGAIN ) ||
			 m_pParams->m_ParamChangeHandler.HasChanged( AKHARMONIZERPARAMID_VOICE2FILTERQFACTOR ) )
		{
			if ( m_FXInfo.Params.Voice[1].Filter.eFilterType != AKFILTERTYPE_NONE )
			{
				if (!m_FXInfo.Filter[1].IsInitialized())
				{
					AKRESULT eResult = m_FXInfo.Filter[1].Init( m_pAllocator, m_FXInfo.configProcessed.uNumChannels );
					if ( eResult != AK_Success )
						return;
				}

				m_FXInfo.Filter[1].ComputeCoefs(	
					(DSP::FilterType)(m_FXInfo.Params.Voice[1].Filter.eFilterType-1),
					(AkReal32) m_FXInfo.uSampleRate,
					m_FXInfo.Params.Voice[1].Filter.fFilterFrequency,
					m_FXInfo.Params.Voice[1].Filter.fFilterGain,
					m_FXInfo.Params.Voice[1].Filter.fFilterQFactor );
			}
			else
			{
				m_FXInfo.Filter[1].Term( m_pAllocator );
			}
		}
	}
	m_pParams->m_ParamChangeHandler.ResetAllParamChanges();

	// Temporary storage: Pitch out buffer + wet channel accumulator buffer 
	AkUInt32 uVoiceMgmtBufferSize = 2 * io_pBuffer->MaxFrames() * sizeof(AkReal32);
	// Temp storage for phase vocoder
	AkUInt32 uPhaseVocoderTempBufferSize = 0;
	// All voices have same requirement, processed sequentially in same memory
	for ( AkUInt32 i = 0; i < AKHARMONIZER_NUMVOICES; i++ )
	{
		if ( m_FXInfo.Params.Voice[i].bEnable )
			uPhaseVocoderTempBufferSize = m_FXInfo.PhaseVocoder[i].GetTDStorageSize();
	}

	AK_PERF_RECORDING_START( "Harmonizer", 25, 100 );

	// Temporary storage: Voice mgmt + Phase vocoder time domain window
	AkUInt32 uTotalTempStorageSize = uVoiceMgmtBufferSize + uPhaseVocoderTempBufferSize;
	AkReal32 * pfTempStorage  = (AkReal32*) AK_PLUGIN_ALLOC( m_pAllocator, uTotalTempStorageSize );
	if ( !pfTempStorage )
		return; // Pipeline cannot handle errors here, just play dry signal when out of memory

	AkHarmonizerDSPProcess( io_pBuffer, m_FXInfo, pfTempStorage );

	if ( pfTempStorage )
		AK_PLUGIN_FREE( m_pAllocator, pfTempStorage );
	
	AK_PERF_RECORDING_STOP( "Harmonizer", 25, 100 );
}

