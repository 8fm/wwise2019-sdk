/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

#include "stdafx.h"
#ifndef AK_OPTIMIZED

#include "RendererProxyLocal.h"

#include "AkAudioLib.h"
#include "AkAudioLibIndex.h"
#include "AkEnvironmentsMgr.h"
#include "AkSwitchMgr.h"
#include "AkStateMgr.h"
#include "AkCritical.h"
#include "AkMonitor.h"
#include "AkPlayingMgr.h"
#include "AkRegistryMgr.h"

extern AK::Comm::ICommunicationCentral * g_pCommCentral;

RendererProxyLocal::RendererProxyLocal()
{
}

RendererProxyLocal::~RendererProxyLocal()
{
}

AkPlayingID RendererProxyLocal::PostEvent( AkUniqueID in_eventID, AkWwiseGameObjectID in_gameObjectPtr, unsigned int in_cookie, AkUInt32 in_uFlags, AkTimeMs in_iSeekOffset ) const
{
	AkCustomParamType customParam;
	customParam.customParam = in_cookie;
	customParam.ui32Reserved = in_cookie ? AK_EVENTWITHCOOKIE_RESERVED_BIT : 0;
	customParam.ui32Reserved |= AK_EVENTFROMWWISE_RESERVED_BIT;
	customParam.pExternalSrcs = NULL;
	AKASSERT( AK::SoundEngine::IsInitialized() );
	AkPlayingID playingID =  AK::SoundEngine::PostEvent( in_eventID, (AkGameObjectID)in_gameObjectPtr, AK_WWISE_MARKER_NOTIFICATION_FLAGS | in_uFlags, NULL, NULL, &customParam );
	if (in_iSeekOffset)
	{
		SeekOnEvent(in_eventID, in_gameObjectPtr, in_iSeekOffset, false, playingID);
	}
	return playingID;
}

AKRESULT RendererProxyLocal::ExecuteActionOnEvent( AkUniqueID in_eventID, AK::SoundEngine::AkActionOnEventType in_ActionType, AkWwiseGameObjectID in_gameObjectID /* = AK_INVALID_GAME_OBJECT */, AkTimeMs in_uTransitionDuration /* = 0 */, AkCurveInterpolation in_eFadeCurve /* = AkCurveInterpolation_Linear */)
{
    if( AK::SoundEngine::IsInitialized() )
	{
		return AK::SoundEngine::ExecuteActionOnEvent( in_eventID, in_ActionType, (AkGameObjectID)in_gameObjectID, in_uTransitionDuration, in_eFadeCurve );
	}
	return AK_Fail;
}

void RendererProxyLocal::SetNetworkControllerName(const char* in_pszNetworkAppName) const
{
	if (AK::SoundEngine::IsInitialized() && g_pCommCentral)
	{
		g_pCommCentral->SetNetworkControllerName(in_pszNetworkAppName);
	}
}

bool RendererProxyLocal::SeekOnEvent(AkUniqueID in_eventID, AkWwiseGameObjectID in_gameObjectID, AkTimeMs in_iPosition, bool in_bSeekToNearestMarker, AkPlayingID in_PlayingID) const
{
	if (AK::SoundEngine::IsInitialized())
	{
		AK::SoundEngine::SeekOnEvent(in_eventID, (AkGameObjectID)in_gameObjectID, in_iPosition, in_bSeekToNearestMarker, in_PlayingID);
		return true;
	}
	return false;
}

bool RendererProxyLocal::SeekOnEvent(AkUniqueID in_eventID, AkWwiseGameObjectID in_gameObjectID, AkReal32 in_fPercent, bool in_bSeekToNearestMarker, AkPlayingID in_PlayingID) const
{
	if (AK::SoundEngine::IsInitialized())
	{
		AK::SoundEngine::SeekOnEvent(in_eventID, (AkGameObjectID)in_gameObjectID, in_fPercent, in_bSeekToNearestMarker, in_PlayingID);
		return true;
	}
	return false;
}

AKRESULT RendererProxyLocal::RegisterGameObj( AkWwiseGameObjectID in_GameObj, const char* in_pszObjectName /*= "" */ )
{
    if( AK::SoundEngine::IsInitialized() )
	{
		if (in_GameObj >= AkGameObjectID_ReservedStart)
		{
			// Internal game object registration must go directly through the registry manager because the API filters out this range.
			CAkFunctionCritical SpaceSetAsCritical;
			void* pMonitorData = AkMonitor::Monitor_AllocateGameObjNameString((AkGameObjectID)in_GameObj, in_pszObjectName);
			return g_pRegistryMgr->RegisterObject((AkGameObjectID)in_GameObj, pMonitorData);
		}
		else
		{
			// For external game objects, it is safer to go through the message queue.  This guarantees order of operations wrt other API calls and ray computation update.
			return AK::SoundEngine::RegisterGameObj((AkGameObjectID)in_GameObj, in_pszObjectName);
		}
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::UnregisterGameObj( AkWwiseGameObjectID in_GameObj )
{
	if( AK::SoundEngine::IsInitialized() )
	{
		AkGameObjectID gameObjectID = in_GameObj;
		if (gameObjectID == AK_INVALID_GAME_OBJECT)
			return AK::SoundEngine::UnregisterAllGameObj();
		else
			return AK::SoundEngine::UnregisterGameObj(gameObjectID);
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::MonitoringMuteGameObj(AkWwiseGameObjectID in_GameObj, bool in_bState, bool in_bNotify)
{
	if (AK::SoundEngine::IsInitialized())
	{
		CAkFunctionCritical SpaceSetAsCritical;
		AKRESULT result = g_pRegistryMgr->MonitoringMuteGameObj((AkGameObjectID)in_GameObj, in_bState);
		return result;
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::MonitoringSoloGameObj(AkWwiseGameObjectID in_GameObj, bool in_bState, bool in_bNotify)
{
	if (AK::SoundEngine::IsInitialized())
	{
		CAkFunctionCritical SpaceSetAsCritical;
		AKRESULT result = g_pRegistryMgr->MonitoringSoloGameObj((AkGameObjectID)in_GameObj, in_bState);
		return result;
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::SetDefaultListeners(AkWwiseGameObjectID* in_pListenerIDs, AkUInt32 in_uNumListeners)
{
	if (AK::SoundEngine::IsInitialized())
	{		
		for (AkUInt32 i = 0; i < in_uNumListeners; ++i)
			AK::SoundEngine::AddDefaultListener((AkGameObjectID)in_pListenerIDs[i]);	//Only add to the game.  Never SET the default listeners.  Otherwise it might bug the game's listeners setup!
		return AK_Success;
	}
	return AK_Fail;
}
	
AKRESULT RendererProxyLocal::SetListeners(AkWwiseGameObjectID in_GameObjectID, AkWwiseGameObjectID* in_pListenerIDs, AkUInt32 in_uNumListeners)
{
	if( AK::SoundEngine::IsInitialized() )
	{
		AkGameObjectID* pListenerIDs = (AkGameObjectID*) AkAlloca(sizeof(AkGameObjectID)*in_uNumListeners);
		for (AkUInt32 i = 0; i < in_uNumListeners; ++i)
			pListenerIDs[i] = (AkGameObjectID)in_pListenerIDs[i];
		return AK::SoundEngine::SetListeners((AkGameObjectID)in_GameObjectID, pListenerIDs, in_uNumListeners);
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::SetAttenuationScalingFactor( AkWwiseGameObjectID in_GameObjectID, AkReal32 in_fAttenuationScalingFactor )
{
    if( AK::SoundEngine::IsInitialized() )
	{
		return AK::SoundEngine::SetScalingFactor( (AkGameObjectID)in_GameObjectID, in_fAttenuationScalingFactor );
	}
	return AK_Fail;
}

void RendererProxyLocal::SetRelativePosition( const AkSoundPosition & in_rPosition )
{
    if( AK::SoundEngine::IsInitialized() )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		g_pRegistryMgr->SetRelativePosition( in_rPosition );
	}
}

AKRESULT RendererProxyLocal::SetPosition( AkWwiseGameObjectID in_GameObj, const AkSoundPosition& in_rPosition )
{
    if( AK::SoundEngine::IsInitialized() )
	{
		return AK::SoundEngine::SetPosition( (AkGameObjectID)in_GameObj, in_rPosition );
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::SetMultiplePositions( AkWwiseGameObjectID in_GameObjectID, const AkSoundPosition * in_pPositions, AkUInt16 in_NumPositions, AK::SoundEngine::MultiPositionType in_eMultiPositionType /* = MultiPositionType_MultiDirections */ )
{
    if( AK::SoundEngine::IsInitialized() )
	{
		return AK::SoundEngine::SetMultiplePositions( (AkGameObjectID)in_GameObjectID, in_pPositions, in_NumPositions, in_eMultiPositionType );
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::SetListenerPosition(const AkListenerPosition& in_rPosition, AkWwiseGameObjectID in_ulIndex /*= 0*/)
{
    if( AK::SoundEngine::IsInitialized() )
	{
		return AK::SoundEngine::SetPosition((AkGameObjectID)in_ulIndex, in_rPosition);
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::SetListenerScalingFactor( AkWwiseGameObjectID in_ulIndex, AkReal32 in_fAttenuationScalingFactor )
{
    if( AK::SoundEngine::IsInitialized() )
	{
		return AK::SoundEngine::SetScalingFactor( (AkGameObjectID)in_ulIndex, in_fAttenuationScalingFactor);
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::SetListenerSpatialization( AkWwiseGameObjectID in_uIndex, bool in_bSpatialized, AkChannelConfig in_channelConfig, AK::SpeakerVolumes::VectorPtr in_pVolumeOffsets /*= NULL */)
{
	if( AK::SoundEngine::IsInitialized() )
	{
		// IMPORTANT: Move volumes to array allocated on the stack with proper size. Volume vector size depends on the platform. Do not use Vector::Copy().
		AK::SpeakerVolumes::VectorPtr pVolumeVector = NULL;
		if ( in_pVolumeOffsets )
		{
			AkUInt32 uVectorSize = AK::SpeakerVolumes::Vector::GetRequiredSize( in_channelConfig.uNumChannels );
			pVolumeVector = (AK::SpeakerVolumes::VectorPtr)AkAlloca( uVectorSize );
			for ( AkUInt32 uElement = 0; uElement < in_channelConfig.uNumChannels; uElement++ )
			{
				pVolumeVector[uElement] = in_pVolumeOffsets[uElement];
			}
		}
		return AK::SoundEngine::SetListenerSpatialization( (AkGameObjectID)in_uIndex, in_bSpatialized, in_channelConfig, pVolumeVector);
	}
	return AK_Fail;
}

void RendererProxyLocal::SetRTPC( AkRtpcID in_RTPCid, AkReal32 in_Value, AkWwiseGameObjectID in_GameObj /*= AK_INVALID_GAME_OBJECT*/, bool in_bBypassInternalValueInterpolation /*= false*/ )
{
    if( AK::SoundEngine::IsInitialized() )
	{
		AK::SoundEngine::SetRTPCValue( in_RTPCid, in_Value, (AkGameObjectID)in_GameObj, 0, AkCurveInterpolation_Linear, in_bBypassInternalValueInterpolation );
	}
}

void RendererProxyLocal::SetDefaultRTPCValue( AkRtpcID in_RTPCid, AkReal32 in_defaultValue )
{
	if( AK::SoundEngine::IsInitialized() && g_pRTPCMgr )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		g_pRTPCMgr->SetDefaultParamValue(in_RTPCid, in_defaultValue);
	}
}

void RendererProxyLocal::SetRTPCRamping( AkRtpcID in_RTPCid, int in_rampingType, AkReal32 in_fRampUp, AkReal32 in_fRampDown )
{
	if( AK::SoundEngine::IsInitialized() && g_pRTPCMgr )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		g_pRTPCMgr->SetRTPCRamping(in_RTPCid, (AkTransitionRampingType)in_rampingType, in_fRampUp, in_fRampDown );
	}
}

void RendererProxyLocal::BindRTPCToBuiltInParam( AkRtpcID in_RTPCid, AkBuiltInParam in_eBindToParam )
{
	if( AK::SoundEngine::IsInitialized() && g_pRTPCMgr )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		g_pRTPCMgr->RemoveBuiltInParamBindings( in_RTPCid );
		
		if ( in_eBindToParam != BuiltInParam_None )
		{
			g_pRTPCMgr->AddBuiltInParamBinding(in_eBindToParam, in_RTPCid);
		}
	}
}


void RendererProxyLocal::SetPanningRule( AkPanningRule in_panningRule )
{
    if( AK::SoundEngine::IsInitialized() )
	{
		// Set panning rule for main device.
		AK::SoundEngine::SetPanningRule( in_panningRule );
	}
}

AKRESULT RendererProxyLocal::ResetRTPC( AkWwiseGameObjectID in_GameObjectPtr )
{
    if( AK::SoundEngine::IsInitialized() )
	{
		return AK::SoundEngine::ResetRTPC( (AkGameObjectID)in_GameObjectPtr );
	}
	return AK_Fail;
}

void RendererProxyLocal::ResetRTPCValue(  AkRtpcID in_RTPCid, AkWwiseGameObjectID in_GameObjectPtr, bool in_bBypassInternalValueInterpolation /*= false*/ )
{
    if( AK::SoundEngine::IsInitialized() )
	{
		AK::SoundEngine::ResetRTPCValue( in_RTPCid, (AkGameObjectID)in_GameObjectPtr, 0, AkCurveInterpolation_Linear, in_bBypassInternalValueInterpolation );
	}
}

AKRESULT RendererProxyLocal::SetSwitch( AkSwitchGroupID in_SwitchGroup, AkSwitchStateID in_SwitchState, AkWwiseGameObjectID in_GameObj /*= AK_INVALID_GAME_OBJECT*/ )
{
    if( AK::SoundEngine::IsInitialized() )
	{
		return AK::SoundEngine::SetSwitch( in_SwitchGroup, in_SwitchState, (AkGameObjectID)in_GameObj );
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::ResetSwitches( AkWwiseGameObjectID in_GameObjectPtr )
{
    if( AK::SoundEngine::IsInitialized() )
	{
		return AK::SoundEngine::ResetSwitches( (AkGameObjectID)in_GameObjectPtr );
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::PostTrigger( AkTriggerID in_Trigger, AkWwiseGameObjectID in_GameObj  )
{
    if( AK::SoundEngine::IsInitialized() )
	{
		return AK::SoundEngine::PostTrigger( in_Trigger, (AkGameObjectID)in_GameObj );
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::ResetAllStates()
{
    if( AK::SoundEngine::IsInitialized() )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		return g_pStateMgr->ResetAllStates();
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::ResetRndSeqCntrPlaylists()
{
    if( AK::SoundEngine::IsInitialized() )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		return g_pIndex->ResetRndSeqCntrPlaylists();
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::SetGameObjectAuxSendValues(AkWwiseGameObjectID in_gameObjectID, AkMonitorData::AuxSendValue* in_aEnvironmentValues, AkUInt32 in_uNumEnvValues)
{
	if( AK::SoundEngine::IsInitialized() )
	{
		AkAuxSendValue* pEnvironmentValues = (AkAuxSendValue*)alloca(sizeof(AkAuxSendValue)*in_uNumEnvValues);
		for (AkUInt32 i = 0; i < in_uNumEnvValues; ++i)
		{
			pEnvironmentValues[i].listenerID = (AkGameObjectID)in_aEnvironmentValues[i].listenerID; //AkWwiseGameObjectID -> AkGameObjectID
			pEnvironmentValues[i].auxBusID = in_aEnvironmentValues[i].auxBusID;
			pEnvironmentValues[i].fControlValue = in_aEnvironmentValues[i].fControlValue;
		}

		return AK::SoundEngine::SetGameObjectAuxSendValues((AkGameObjectID)in_gameObjectID, pEnvironmentValues, in_uNumEnvValues);
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::SetGameObjectOutputBusVolume(AkWwiseGameObjectID in_emitterID, AkWwiseGameObjectID in_listenerID, AkReal32 in_fControlValue)
{
	if( AK::SoundEngine::IsInitialized() )
	{
		return AK::SoundEngine::SetGameObjectOutputBusVolume((AkGameObjectID)in_emitterID, (AkGameObjectID)in_listenerID, in_fControlValue);
	}
	return AK_Fail;
}
	
AKRESULT RendererProxyLocal::SetObjectObstructionAndOcclusion(AkWwiseGameObjectID in_EmitterID, AkWwiseGameObjectID in_ListenerID, AkReal32 in_fObstructionLevel, AkReal32 in_fOcclusionLevel)
{
	if( AK::SoundEngine::IsInitialized() )
	{
		return AK::SoundEngine::SetObjectObstructionAndOcclusion((AkGameObjectID)in_EmitterID, (AkGameObjectID)in_ListenerID, in_fObstructionLevel, in_fOcclusionLevel);
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::SetMultipleObstructionAndOcclusion(AkWwiseGameObjectID in_EmitterID, AkWwiseGameObjectID in_ListenerID, AkObstructionOcclusionValues* in_ObstructionOcclusionValue, AkUInt32 in_uNumObstructionOcclusion)
{
	if (AK::SoundEngine::IsInitialized())
	{
		return AK::SoundEngine::SetMultipleObstructionAndOcclusion((AkGameObjectID)in_EmitterID, (AkGameObjectID)in_ListenerID, in_ObstructionOcclusionValue, in_uNumObstructionOcclusion);
	}

	return AK_Fail;
}

AKRESULT RendererProxyLocal::SetObsOccCurve( int in_curveXType, int in_curveYType, AkUInt32 in_uNumPoints, AkRTPCGraphPoint * in_apPoints, AkCurveScaling in_eScaling )
{
	if( g_pEnvironmentMgr )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		return g_pEnvironmentMgr->SetObsOccCurve( (CAkEnvironmentsMgr::eCurveXType)in_curveXType, 
												  (CAkEnvironmentsMgr::eCurveYType)in_curveYType, 
												  in_uNumPoints, in_apPoints, in_eScaling );
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::SetObsOccCurveEnabled( int in_curveXType, int in_curveYType, bool in_bEnabled )
{
	if( g_pEnvironmentMgr )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		g_pEnvironmentMgr->SetCurveEnabled( (CAkEnvironmentsMgr::eCurveXType)in_curveXType, 
										    (CAkEnvironmentsMgr::eCurveYType)in_curveYType, 
											in_bEnabled );
		return AK_Success;
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::AddSwitchRTPC( AkSwitchGroupID in_switchGroup, AkRtpcID in_RTPC_ID, AkRtpcType in_RTPC_Type, AkSwitchGraphPoint* in_pArrayConversion, AkUInt32 in_ulConversionArraySize )
{
	if( g_pSwitchMgr )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		return g_pSwitchMgr->AddSwitchRTPC( in_switchGroup, in_RTPC_ID, in_RTPC_Type, in_pArrayConversion, in_ulConversionArraySize );
	}
	return AK_Fail;

}

void RendererProxyLocal::RemoveSwitchRTPC( AkSwitchGroupID in_switchGroup )
{
	if( g_pSwitchMgr )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		return g_pSwitchMgr->RemoveSwitchRTPC( in_switchGroup );
	}
}

void RendererProxyLocal::SetVolumeThreshold( AkReal32 in_VolumeThreshold )
{
	AK::SoundEngine::SetVolumeThresholdInternal( in_VolumeThreshold, AK::SoundEngine::AkCommandPriority_WwiseApp );
}

void RendererProxyLocal::SetMaxNumVoicesLimit( AkUInt16 in_maxNumberVoices )
{
	AK::SoundEngine::SetMaxNumVoicesLimitInternal( in_maxNumberVoices, AK::SoundEngine::AkCommandPriority_WwiseApp );
}

void RendererProxyLocal::SetMaxNumDangerousVirtVoicesLimit( AkUInt16 in_maxNumberVoices )
{
	AK::SoundEngine::SetMaxNumDangerousVirtVoicesLimitInternal( in_maxNumberVoices, AK::SoundEngine::AkCommandPriority_WwiseApp );
}

AKRESULT RendererProxyLocal::PostMsgMonitor( const AkUtf16* in_pszMessage )
{
	if( AkMonitor::Get() )
	{
		size_t stringSize = AkUtf16StrLen(in_pszMessage) +1;
		AkOSChar* in_pzConverted = (AkOSChar*)AkAlloc(AkMemID_Profiler, sizeof(AkOSChar) * stringSize );
		
		if( in_pzConverted == NULL)
		{
			return AK_Fail;
		}
		
		AK_UTF16_TO_OSCHAR( in_pzConverted, in_pszMessage, stringSize );
		
		AkMonitor::Monitor_PostString(in_pzConverted, AK::Monitor::ErrorLevel_Message, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT);
		AkFree(AkMemID_Profiler, in_pzConverted );
		return AK_Success;
	}
	return AK_Fail;
}

AKRESULT RendererProxyLocal::PostMsgMonitor( const char* in_pszMessage )
{
	AKRESULT eResult = AK_Fail;
	AkUtf16 wideString[ AK_MAX_PATH ];	
	if( AK_CHAR_TO_UTF16( wideString, in_pszMessage, AK_MAX_PATH ) > 0)
		eResult = PostMsgMonitor( wideString );

	return eResult;
}

void RendererProxyLocal::StopAll( AkWwiseGameObjectID in_GameObjPtr )
{
	if( AK::SoundEngine::IsInitialized() )
	{
		AK::SoundEngine::StopAll( (AkGameObjectID)in_GameObjPtr );
	}
}

void RendererProxyLocal::StopPlayingID( AkPlayingID in_playingID, AkTimeMs in_uTransitionDuration , AkCurveInterpolation in_eFadeCurve )
{
	if( AK::SoundEngine::IsInitialized() )
	{
		AK::SoundEngine::ExecuteActionOnPlayingID(AK::SoundEngine::AkActionOnEventType_Stop, in_playingID, in_uTransitionDuration, in_eFadeCurve );
	}
}

void RendererProxyLocal::ExecuteActionOnPlayingID(AK::SoundEngine::AkActionOnEventType in_ActionType, AkPlayingID in_playingID, AkTimeMs in_uTransitionDuration, AkCurveInterpolation in_eFadeCurve)
{
	if (AK::SoundEngine::IsInitialized())
	{
		AK::SoundEngine::ExecuteActionOnPlayingID(in_ActionType, in_playingID, in_uTransitionDuration, in_eFadeCurve);
	}
}

void RendererProxyLocal::SetLoudnessFrequencyWeighting( AkLoudnessFrequencyWeighting in_eLoudnessFrequencyWeighting )
{
	if( AK::SoundEngine::IsInitialized() )
	{
		AK::SoundEngine::SetLoudnessFrequencyWeighting( in_eLoudnessFrequencyWeighting );
	}
}

void RendererProxyLocal::AddOutput(AkOutputSettings &in_rSetting, const AkGameObjectID* in_pListenerIDs, AkUInt32 in_uNumListeners)
{
	if(!AK::SoundEngine::IsInitialized())
		return;

	CAkFunctionCritical SpaceSetAsCritical;
	AK::SoundEngine::AddOutput(in_rSetting, NULL, in_pListenerIDs, in_uNumListeners);
}

void RendererProxyLocal::RegisterPlugin(const char *in_szPlugin)
{
	if(!AK::SoundEngine::IsInitialized())
		return;
	
	AkOSChar *szOSName;
	CONVERT_CHAR_TO_OSCHAR(in_szPlugin, szOSName);
	AK::SoundEngine::RegisterPluginDLL(szOSName);		
}

#endif // #ifndef AK_OPTIMIZED
