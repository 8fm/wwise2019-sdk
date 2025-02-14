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

#include <AK/Tools/Common/AkAssert.h>
#include "AkTremoloFX.h"
#include "LFO.h"
#include <AK/AkWwiseSDKVersion.h>

// Plugin mechanism. Dynamic create function whose address must be registered to the FX manager.
AK::IAkPlugin* CreateAkTremoloFX( AK::IAkPluginMemAlloc * in_pAllocator )
{
	return AK_PLUGIN_NEW( in_pAllocator, CAkTremoloFX() );
}

// Constructor.
CAkTremoloFX::CAkTremoloFX() 
: m_pSharedParams( 0 )
, m_pAllocator( 0 )
, m_pFXCtx( NULL )
{
	AKPLATFORM::AkMemSet( &m_FXInfo.Params, 0, sizeof(m_FXInfo.Params) );
	AKPLATFORM::AkMemSet( &m_FXInfo.PrevParams, 0, sizeof(m_FXInfo.PrevParams) );
}

// Destructor.
CAkTremoloFX::~CAkTremoloFX()
{
	if ( m_pAllocator )
		 m_lfo.Release( m_pAllocator );
}

// Initializes and allocate memory for the effect
AKRESULT CAkTremoloFX::Init(	AK::IAkPluginMemAlloc * in_pAllocator,		// Memory allocator interface.
								AK::IAkEffectPluginContext * in_pFXCtx,		// FX Context
								AK::IAkPluginParam * in_pParams,			// Effect parameters.
								AkAudioFormat &	in_rFormat					// Required audio input format.
							   )
{	
	// get parameter state
	m_pSharedParams = static_cast<CAkTremoloFXParams*>(in_pParams);
	m_pSharedParams->GetParams( &m_FXInfo.Params );
	m_FXInfo.PrevParams = m_FXInfo.Params;
	m_pFXCtx = in_pFXCtx;

	m_FXInfo.uSampleRate = in_rFormat.uSampleRate;
	m_pAllocator = in_pAllocator;

	AKRESULT eResult = SetupLFO( in_rFormat.channelConfig );

	AK_PERF_RECORDING_RESET();

	return eResult;
}

AKRESULT CAkTremoloFX::SetupLFO( AkChannelConfig in_channelConfig )
{
	// Exclude Center channel if applicable.
	if ( !m_FXInfo.Params.NonRTPC.bProcessCenter && ((in_channelConfig.uChannelMask & AK_SPEAKER_SETUP_3_0) == AK_SPEAKER_SETUP_3_0) )
		in_channelConfig = in_channelConfig.RemoveCenter();

	// Exclude LFE if applicable.
	if ( !m_FXInfo.Params.NonRTPC.bProcessLFE )
		in_channelConfig = in_channelConfig.RemoveLFE();

	return m_lfo.Setup(
		m_pAllocator,
		m_pFXCtx->GlobalContext(),
		in_channelConfig,
		m_FXInfo.uSampleRate,
		m_FXInfo.Params.RTPC.modParams );
}

AKRESULT CAkTremoloFX::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AK_PLUGIN_DELETE( in_pAllocator, this );
	return AK_Success;
}


// Effect info query.
AKRESULT CAkTremoloFX::GetPluginInfo( AkPluginInfo & out_rPluginInfo )
{
	out_rPluginInfo.eType = AkPluginTypeEffect;
	out_rPluginInfo.bIsInPlace = true;
	out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
	return AK_Success;
}

// parameter updates from wwise
void CAkTremoloFX::LiveParametersUpdate( AkAudioBuffer* io_pBuffer )
{
	// Handle live changes
	if ( (m_FXInfo.PrevParams.NonRTPC.bProcessLFE != m_FXInfo.Params.NonRTPC.bProcessLFE) || (m_FXInfo.PrevParams.NonRTPC.bProcessCenter != m_FXInfo.Params.NonRTPC.bProcessCenter) )
	{
		AkChannelConfig config = io_pBuffer->GetChannelConfig();
		SetupLFO( config );
	}
}

// parameter updates from the game engine
void CAkTremoloFX::RTPCParametersUpdate()
{
	m_lfo.SetParams(
		m_FXInfo.uSampleRate,
		m_FXInfo.Params.RTPC.modParams.lfoParams
		);
}


// main processing routine
void CAkTremoloFX::Execute( AkAudioBuffer * io_pBuffer )
{
	m_pSharedParams->GetParams( &m_FXInfo.Params );

	// Support live change of non-RTPC parameters when connected to Wwise
	if(m_FXInfo.Params.NonRTPC.bHasChanged)
	{
		LiveParametersUpdate( io_pBuffer );
	}

	if ( m_lfo.GetNumChannels() == 0 ) // Necessary to do this after LiveParametersUpdate.
		return;

	if(m_FXInfo.Params.RTPC.bHasChanged)
	{
		// RTPC parameter updates
		RTPCParametersUpdate();
	}

	AkChannelConfig config = io_pBuffer->GetChannelConfig();

	// Skip LFE if required.
	if ( !m_FXInfo.Params.NonRTPC.bProcessLFE )
		config = config.RemoveLFE();

	AK_PERF_RECORDING_START( "Tremolo", 25, 30 );
	
	// Skip center channel if required.
	bool bSkipCenterChannel = ( !m_FXInfo.Params.NonRTPC.bProcessCenter && ((config.uChannelMask & AK_SPEAKER_SETUP_3_0) == AK_SPEAKER_SETUP_3_0) );

	// Prepare variables for ramp.
	const AkUInt32 uNumFrames = io_pBuffer->uValidFrames;

	const AkReal32 fDepth = m_FXInfo.Params.RTPC.fModDepth;
	const AkReal32 fPrevDepth = m_FXInfo.PrevParams.RTPC.fModDepth;

	const AkReal32 fOutputGain = m_FXInfo.Params.RTPC.fOutputGain;
	const AkReal32 fPrevOutputGain = m_FXInfo.PrevParams.RTPC.fOutputGain;
	const AkReal32 fOutInc = ( fOutputGain - fPrevOutputGain ) / uNumFrames;
	const AkReal32 fPWM = m_FXInfo.Params.RTPC.modParams.lfoParams.fPWM;

	for(AkUInt32 i = 0, iLfo = 0; i < config.uNumChannels; ++i)
	{
		// Skip center channel.
		if ( bSkipCenterChannel && i == AK_IDX_SETUP_3_CENTER )
			continue;

		TremoloOutputPolicy oOutputParams;
		oOutputParams.fGain = fOutputGain;
		oOutputParams.fGainInc = fOutInc;

		m_lfo.GetChannel( iLfo++ ).ProduceBuffer(io_pBuffer->GetChannel(i), uNumFrames, fDepth, fPrevDepth, fPWM, oOutputParams);
	}

	m_FXInfo.PrevParams = m_FXInfo.Params;

	AK_PERF_RECORDING_STOP( "Tremolo", 25, 30 );
}

AKRESULT CAkTremoloFX::Reset()
{
	return AK_Success;
}
