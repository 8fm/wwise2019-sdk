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

//////////////////////////////////////////////////////////////////////
//
// AkMeterFX.cpp
//
// Meter processing FX implementation.
// 
//
// Copyright 2010 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#include "AkMeterFX.h"
#include "AkMeterFXHelpers.h"
#include "AkMeterManager.h"
#include <AK/AkWwiseSDKVersion.h>

// Plugin mechanism. Dynamic create function whose address must be registered to the FX manager.
AK::IAkPlugin* CreateAkMeterFX( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AK::IAkPlugin* pPlugin = AK_PLUGIN_NEW( in_pAllocator, CAkMeterFX( ) );
	return pPlugin;
}

// Constructor.
CAkMeterFX::CAkMeterFX()
	: m_pParams( NULL )
	, m_pCtx( NULL )
	, m_pAllocator( NULL )
	, m_pMeterManager( NULL )
	, m_bTerminated( false )
	, m_fDownstreamGain( 1.f )
	, m_bFirstDownstreamGain( true )
{
}

// Destructor.
CAkMeterFX::~CAkMeterFX()
{
}

// Initializes and allocate memory for the effect
AKRESULT CAkMeterFX::Init(	
	AK::IAkPluginMemAlloc * in_pAllocator,		// Memory allocator interface.
	AK::IAkEffectPluginContext * in_pFXCtx,		// FX Context
	AK::IAkPluginParam * in_pParams,			// Effect parameters.
	AkAudioFormat &	in_rFormat					// Required audio input format.
	)
{
	m_pParams = static_cast<CAkMeterFXParams*>(in_pParams);
	m_pAllocator = in_pAllocator;
	m_pCtx = in_pFXCtx;
	m_state.uSampleRate = in_rFormat.uSampleRate;

	m_pMeterManager = CAkMeterManager::Instance(in_pAllocator, in_pFXCtx->GlobalContext());
	if ( !m_pMeterManager )
		return AK_Fail;

	m_pMeterManager->Register( this );

	// Copy parameters to members for meter manager processing (which might occur after m_pParams is deleted)
	m_fMin = m_pParams->RTPC.fMin;
	m_uGameParamID = m_pParams->NonRTPC.uGameParamID;
	m_uGameObjectID = AK_INVALID_GAME_OBJECT;
	if (m_pParams->NonRTPC.eScope == AkMeterScope_GameObject && m_pCtx->GetGameObjectInfo() != NULL)
		m_uGameObjectID = m_pCtx->GetGameObjectInfo()->GetGameObjectID();
	m_uNodeID = m_pCtx->GetNodeID();
	m_eMode = m_pParams->NonRTPC.eMode;

	AK_PERF_RECORDING_RESET();

	return AK_Success;
}

// Terminates.
AKRESULT CAkMeterFX::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	if ( m_pMeterManager )
	{
		m_bTerminated = true;
	}
	else // if we didn't successfully acquire meter manager, manager can't kill us.
	{
		AK_PLUGIN_DELETE( in_pAllocator, this );
	}

	return AK_Success;
}

// Reset
AKRESULT CAkMeterFX::Reset()
{
	m_state.fHoldTime = 0.0f;
	m_state.fReleaseTarget = m_pParams->RTPC.fMin;
	m_state.fLastValue = m_pParams->RTPC.fMin;
	for ( int i = 0; i < HOLD_MEMORY_SIZE; ++i )
		m_state.fHoldMemory[ i ] = m_pParams->RTPC.fMin;

	return AK_Success;
}

// Effect info query.
AKRESULT CAkMeterFX::GetPluginInfo( AkPluginInfo & out_rPluginInfo )
{
	out_rPluginInfo.eType = AkPluginTypeEffect;
	out_rPluginInfo.bIsInPlace = true;
	out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
	return AK_Success;
}

void CAkMeterFX::Execute( AkAudioBuffer * io_pBuffer ) 
{
	AkReal32 * pfInChannel = NULL;
#ifndef AK_OPTIMIZED
	char * pMonitorData = NULL;
	int sizeofData = 0;
	if ( m_pCtx->CanPostMonitorData() )
	{
		sizeofData = sizeof( AkUInt32 ) * 1 
			+ sizeof( AkReal32 ) * io_pBuffer->NumChannels()
			+ sizeof( AkReal32 );
		pMonitorData = (char *) AkAlloca( sizeofData );

		AkChannelConfig channelConfig = io_pBuffer->GetChannelConfig();
		*((AkUInt32 *) pMonitorData ) = channelConfig.Serialize();

		pfInChannel = (AkReal32 *) ( pMonitorData + sizeof( AkUInt32 ) );

	}
#endif

	// Get applied gain ramp
	AkReal32 fGain = 1.f;
	AkReal32 fGainInc = 0.f;

	if( m_pParams->NonRTPC.bApplyDownstreamVolume && io_pBuffer->uValidFrames )
	{
		if( m_bFirstDownstreamGain )
		{
			m_bFirstDownstreamGain = false;
			m_fDownstreamGain = m_pCtx->GetDownstreamGain();
			fGain = m_fDownstreamGain;
		}
		else
		{
			fGain = m_fDownstreamGain;
			m_fDownstreamGain = m_pCtx->GetDownstreamGain();
		}
		fGainInc = (m_fDownstreamGain - fGain) / io_pBuffer->uValidFrames;
	}

	AK_PERF_RECORDING_START( "Meter", 25, 30 );

	AkReal32 fValue = AkMeterGetValue( io_pBuffer, m_pParams, pfInChannel, fGain, fGainInc );
	AkMeterApplyBallistics( fValue, io_pBuffer->MaxFrames(), m_pParams, &m_state );

	// Although we don't modify the output, the meter ballistics do have a tail
	bool bPreStop = io_pBuffer->eState == AK_NoMoreData;
	if ( bPreStop )
	{	
		bool bTailEndReached = ( m_state.fLastValue <= m_pParams->RTPC.fMin );
		if ( !bTailEndReached )
		{
			io_pBuffer->ZeroPadToMaxFrames();
			io_pBuffer->eState = AK_DataReady;
		}
	}

	// Copy parameters to members for meter manager processing (which might occur after m_pParams is deleted)
	m_fMin = m_pParams->RTPC.fMin;
	m_eMode = m_pParams->NonRTPC.eMode;
	m_uNodeID = m_pCtx->GetNodeID();

	AkGameObjectID uGameObjectID = AK_INVALID_GAME_OBJECT;
	if (m_pParams->NonRTPC.eScope == AkMeterScope_GameObject && m_pCtx->GetGameObjectInfo() != NULL)
		uGameObjectID = m_pCtx->GetGameObjectInfo()->GetGameObjectID();

	// If the scope or game parame has changed due to a live update, we need to re-register it.
	if (m_uGameObjectID != uGameObjectID || m_uGameParamID != m_pParams->NonRTPC.uGameParamID)
	{
		m_uGameObjectID = uGameObjectID;
		m_uGameParamID = m_pParams->NonRTPC.uGameParamID;

		m_pMeterManager->Unregister(this);
		m_pMeterManager->Register(this);
	}

	AK_PERF_RECORDING_STOP( "Meter", 25, 30 );

#ifndef AK_OPTIMIZED
	if ( pfInChannel )
	{
		pfInChannel[io_pBuffer->NumChannels()] = m_state.fLastValue;

		m_pCtx->PostMonitorData( pMonitorData, sizeofData );
	}
#endif
}
