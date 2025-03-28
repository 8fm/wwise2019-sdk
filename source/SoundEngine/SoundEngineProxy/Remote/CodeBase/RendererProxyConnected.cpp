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

#include "stdafx.h"
#ifndef AK_OPTIMIZED

#include "RendererProxyConnected.h"
#include "CommandDataSerializer.h"
#include "IRendererProxy.h"

void RendererProxyConnected::HandleExecute( CommandDataSerializer& in_rSerializer, CommandDataSerializer& out_rReturnSerializer )
{
	RendererProxyCommandData::CommandData cmdData;

	{
		CommandDataSerializer::AutoSetDataPeeking peekGate( in_rSerializer );
		in_rSerializer.Get( cmdData );
	}

	switch( cmdData.m_methodID )
	{
	case AK::Comm::IRendererProxy::MethodSetNetworkControllerName:
		{
			RendererProxyCommandData::SetNetworkControllerName setNetworkControllerName;
			if (in_rSerializer.Get(setNetworkControllerName))
				m_localProxy.SetNetworkControllerName(setNetworkControllerName.m_pszNetworkAppName);
			break;
		}
	case AK::Comm::IRendererProxy::MethodPostEvent:
		{
			RendererProxyCommandData::PostEvent postEvent;
			if( in_rSerializer.Get( postEvent ) )
				out_rReturnSerializer.Put( m_localProxy.PostEvent( postEvent.m_eventID, postEvent.m_gameObjectPtr, postEvent.m_cookie, postEvent.m_flags, postEvent.m_seekOffset ) );

			break;
		}

	case AK::Comm::IRendererProxy::MethodExecuteActionOnEvent:
		{
			RendererProxyCommandData::ExecuteActionOnEvent executeActionOnEvent;
			if( in_rSerializer.Get( executeActionOnEvent ) )
				m_localProxy.ExecuteActionOnEvent( executeActionOnEvent.m_eventID, executeActionOnEvent.m_eActionType, executeActionOnEvent.m_gameObjectID, executeActionOnEvent.m_uTransitionDuration, executeActionOnEvent.m_eFadeCurve );

			break;
		}

	case AK::Comm::IRendererProxy::MethodSeekOnEvent_MS:
		{
			RendererProxyCommandData::SeekOnEvent_MS seekOnEvent;
			if (in_rSerializer.Get(seekOnEvent))
				m_localProxy.SeekOnEvent(seekOnEvent.m_eventID, seekOnEvent.m_gameObjectID, seekOnEvent.m_iPosition, seekOnEvent.m_bSeekToNearestMarker, seekOnEvent.m_PlayingID);

			break;
		}

	case AK::Comm::IRendererProxy::MethodSeekOnEvent_PCT:
	{
		RendererProxyCommandData::SeekOnEvent_PCT seekOnEvent;
		if (in_rSerializer.Get(seekOnEvent))
			m_localProxy.SeekOnEvent(seekOnEvent.m_eventID, seekOnEvent.m_gameObjectID, seekOnEvent.m_fPercent, seekOnEvent.m_bSeekToNearestMarker, seekOnEvent.m_PlayingID);

		break;
	}

	case AK::Comm::IRendererProxy::MethodRegisterGameObject:
		{
			RendererProxyCommandData::RegisterGameObj registerGameObject;
			if( in_rSerializer.Get( registerGameObject ) )
				m_localProxy.RegisterGameObj( registerGameObject.m_gameObjectPtr, registerGameObject.m_pszObjectName );

			break;
		}

	case AK::Comm::IRendererProxy::MethodUnregisterGameObject:
		{
			RendererProxyCommandData::UnregisterGameObj unregisterGameObject;
			if( in_rSerializer.Get( unregisterGameObject ) )
				m_localProxy.UnregisterGameObj( unregisterGameObject.m_gameObjectPtr );

			break;
		}

	case AK::Comm::IRendererProxy::MethodSetDefaultListeners:
		{
			RendererProxyCommandData::SetDefaultListeners SetDefaultListeners;
			if (in_rSerializer.Get(SetDefaultListeners))
				m_localProxy.SetDefaultListeners(SetDefaultListeners.m_pListeners, SetDefaultListeners.m_uNumListeners);

			break;
		}

	case AK::Comm::IRendererProxy::MethodSetListeners:
		{
			RendererProxyCommandData::SetListeners SetListeners;
			if( in_rSerializer.Get( SetListeners ) )
				m_localProxy.SetListeners( SetListeners.m_gameObjectID, SetListeners.m_pListeners, SetListeners.m_uNumListeners );

			break;
		}

	case AK::Comm::IRendererProxy::MethodSetAttenuationScalingFactor:
		{
			RendererProxyCommandData::SetAttenuationScalingFactor setAttenuationScalingFactor;
			if( in_rSerializer.Get( setAttenuationScalingFactor ) )
				m_localProxy.SetAttenuationScalingFactor( setAttenuationScalingFactor.m_gameObjectID, setAttenuationScalingFactor.m_fAttenuationScalingFactor );

			break;
		}

	case AK::Comm::IRendererProxy::MethodSetRelativePosition:
		{
			RendererProxyCommandData::SetRelativePosition setRelativePosition;
			if( in_rSerializer.Get( setRelativePosition ) )
				m_localProxy.SetRelativePosition( setRelativePosition.m_position );

			break;
		}

	case AK::Comm::IRendererProxy::MethodSetPosition:
		{
			RendererProxyCommandData::SetPosition setPosition;
			if( in_rSerializer.Get( setPosition ) )
				m_localProxy.SetPosition( setPosition.m_gameObjectPtr, setPosition.m_position );

			break;
		}

	case AK::Comm::IRendererProxy::MethodSetListenerPosition:
		{
			RendererProxyCommandData::SetListenerPosition setListenerPosition;
			if( in_rSerializer.Get( setListenerPosition ) )
				m_localProxy.SetListenerPosition( setListenerPosition.m_position, setListenerPosition.m_ulIndex );

			break;
		}

	case AK::Comm::IRendererProxy::MethodSetListenerScalingFactor:
		{
			RendererProxyCommandData::SetListenerScalingFactor setListenerScalingFactor;
			if( in_rSerializer.Get( setListenerScalingFactor ) )
				m_localProxy.SetListenerScalingFactor( setListenerScalingFactor.m_ulIndex, setListenerScalingFactor.m_fAttenuationScalingFactor );

			break;
		}

	case AK::Comm::IRendererProxy::MethodSetListenerSpatialization:
		{
			RendererProxyCommandData::SetListenerSpatialization setListenerSpatialization;
			if( in_rSerializer.Get( setListenerSpatialization ) )
			{
				AK::SpeakerVolumes::VectorPtr pSpeakerVolume = setListenerSpatialization.m_bUseVolumeOffsets? setListenerSpatialization.m_pVolumeOffsets : NULL;
	
				m_localProxy.SetListenerSpatialization( setListenerSpatialization.m_uIndex, setListenerSpatialization.m_bSpatialized, setListenerSpatialization.m_channelConfig, pSpeakerVolume );
			}
			break;
		}

	case AK::Comm::IRendererProxy::MethodSetPanningRule:
		{
			// This might create some inconsistencies
			//RendererProxyCommandData::SetPanningRule setPanningRule;
			//if( in_rSerializer.Get( setPanningRule ) )
			//	m_localProxy.SetPanningRule( setPanningRule.m_panningRule );
			//
			break;
		}

	case AK::Comm::IRendererProxy::MethodSetMultiplePositions:
		{
			RendererProxyCommandData::SetMultiplePositions setMultiplePositions;
			if( in_rSerializer.Get( setMultiplePositions ) )
				m_localProxy.SetMultiplePositions( setMultiplePositions.m_gameObjectPtr, setMultiplePositions.m_pPositions, (AkUInt16)setMultiplePositions.m_numPositions, setMultiplePositions.m_eMultiPositionType );

			break;
		}

	case AK::Comm::IRendererProxy::MethodSetRTPC:
		{
			RendererProxyCommandData::SetRTPC setRTPC;
			if( in_rSerializer.Get( setRTPC ) )
				m_localProxy.SetRTPC( setRTPC.m_RTPCid, setRTPC.m_value, setRTPC.m_gameObj, setRTPC.m_bBypassInternalValueInterpolation );

			break;
		}

	case AK::Comm::IRendererProxy::MethodSetDefaultRTPCValue:
		{
			RendererProxyCommandData::SetDefaultRTPCValue setDefaultRTPCValue;
			if( in_rSerializer.Get( setDefaultRTPCValue ) )
				m_localProxy.SetDefaultRTPCValue( setDefaultRTPCValue.m_RTPCid, setDefaultRTPCValue.m_defaultValue );

			break;
		}

	case AK::Comm::IRendererProxy::MethodSetRTPCRamping:
		{
			RendererProxyCommandData::SetRTPCRamping setRTPCRamping;
			if( in_rSerializer.Get( setRTPCRamping ) )
				m_localProxy.SetRTPCRamping( setRTPCRamping.m_RTPCid, setRTPCRamping.m_rampingType, setRTPCRamping.m_fRampUp, setRTPCRamping.m_fRampDown );

			break;
		}

	case AK::Comm::IRendererProxy::MethodRTPCBindToBuiltInParam:
		{
			RendererProxyCommandData::RTPCBindToBuiltInParam bindToBuiltInParam;
			if( in_rSerializer.Get( bindToBuiltInParam ) )
				m_localProxy.BindRTPCToBuiltInParam( bindToBuiltInParam.m_RTPCid, bindToBuiltInParam.m_bindToParam );

			break;
		}

	case AK::Comm::IRendererProxy::MethodResetRTPC:
		{
			RendererProxyCommandData::ResetRTPC resetRTPC;
			if( in_rSerializer.Get( resetRTPC ) )
				m_localProxy.ResetRTPC( resetRTPC.m_gameObj );

			break;
		}

	case AK::Comm::IRendererProxy::MethodResetRTPCValue:
		{
			RendererProxyCommandData::ResetRTPCValue resetRTPC;
			if( in_rSerializer.Get( resetRTPC ) )
				m_localProxy.ResetRTPCValue( resetRTPC.m_RTPCid, resetRTPC.m_gameObj, resetRTPC.m_bBypassInternalValueInterpolation );

			break;
		}

	case AK::Comm::IRendererProxy::MethodSetSwitch:
		{
			RendererProxyCommandData::SetSwitch setSwitch;
			if( in_rSerializer.Get( setSwitch ) )
				m_localProxy.SetSwitch( setSwitch.m_switchGroup, setSwitch.m_switchState, setSwitch.m_gameObj );

			break;
		}

	case AK::Comm::IRendererProxy::MethodPostTrigger:
		{
			RendererProxyCommandData::PostTrigger trigger;
			if( in_rSerializer.Get( trigger ) )
				m_localProxy.PostTrigger( trigger.m_trigger, trigger.m_gameObj );

			break;
		}

	case AK::Comm::IRendererProxy::MethodResetSwitches:
		{
			RendererProxyCommandData::ResetSwitches resetSwitches;
			if( in_rSerializer.Get( resetSwitches ) )
				m_localProxy.ResetSwitches( resetSwitches.m_gameObj );

			break;
		}

	case AK::Comm::IRendererProxy::MethodResetAllStates:
		{
			RendererProxyCommandData::ResetAllStates resetAllStates;
			if( in_rSerializer.Get( resetAllStates ) )
				m_localProxy.ResetAllStates();

			break;
		}

	case AK::Comm::IRendererProxy::MethodResetRndSeqCntrPlaylists:
		{
			RendererProxyCommandData::ResetRndSeqCntrPlaylists resetRndSeqCntrPlaylists;
			if( in_rSerializer.Get( resetRndSeqCntrPlaylists ) )
				m_localProxy.ResetRndSeqCntrPlaylists();

			break;
		}

	case AK::Comm::IRendererProxy::MethodSetGameObjectAuxSendValues:
		{
			RendererProxyCommandData::SetGameObjectAuxSendValues setGameObjectEnvironmentsValues;
			if( in_rSerializer.Get( setGameObjectEnvironmentsValues ) )
				m_localProxy.SetGameObjectAuxSendValues( setGameObjectEnvironmentsValues.m_gameObjectID, setGameObjectEnvironmentsValues.m_aEnvironmentValues, setGameObjectEnvironmentsValues.m_uNumEnvValues );

			break;
		}
	case AK::Comm::IRendererProxy::MethodSetGameObjectOutputBusVolume:
		{
			RendererProxyCommandData::SetGameObjectOutputBusVolume setGameObjectDryLevelValue;
			if( in_rSerializer.Get( setGameObjectDryLevelValue ) )
				m_localProxy.SetGameObjectOutputBusVolume(setGameObjectDryLevelValue.m_emitterID, setGameObjectDryLevelValue.m_listenerID, setGameObjectDryLevelValue.m_fControlValue);

			break;
		}
	case AK::Comm::IRendererProxy::MethodSetObjectObstructionAndOcclusion:
		{
			RendererProxyCommandData::SetObjectObstructionAndOcclusion setObjectObstructionAndOcclusion;
			if( in_rSerializer.Get( setObjectObstructionAndOcclusion ) )
				m_localProxy.SetObjectObstructionAndOcclusion( setObjectObstructionAndOcclusion.m_EmitterID, setObjectObstructionAndOcclusion.m_ListenerID, setObjectObstructionAndOcclusion.m_fObstructionLevel, setObjectObstructionAndOcclusion.m_fOcclusionLevel );

			break;
		}
	case AK::Comm::IRendererProxy::MethodSetMultipleObstructionAndOcclusion:
		{
			RendererProxyCommandData::SetMultipleObstructionAndOcclusion setMultipleObstructionAndOcclusion;
			if (in_rSerializer.Get(setMultipleObstructionAndOcclusion))
				m_localProxy.SetMultipleObstructionAndOcclusion(setMultipleObstructionAndOcclusion.m_EmitterID, setMultipleObstructionAndOcclusion.m_ListenerID, setMultipleObstructionAndOcclusion.m_fObstructionOcclusionValues, setMultipleObstructionAndOcclusion.m_uNumOcclusionObstruction);

			break;
		}
	case AK::Comm::IRendererProxy::MethodSetObsOccCurve:
		{
			RendererProxyCommandData::SetObsOccCurve setObsOccCurve;
			if( in_rSerializer.Get( setObsOccCurve ) )
				m_localProxy.SetObsOccCurve( setObsOccCurve.m_curveXType, setObsOccCurve.m_curveYType, (AkUInt16)setObsOccCurve.m_ulConversionArraySize, setObsOccCurve.m_pArrayConversion, setObsOccCurve.m_eScaling );

			break;
		}

	case AK::Comm::IRendererProxy::MethodSetObsOccCurveEnabled:
		{
			RendererProxyCommandData::SetObsOccCurveEnabled setObsOccCurveEnabled;
			if( in_rSerializer.Get( setObsOccCurveEnabled ) )
				m_localProxy.SetObsOccCurveEnabled( setObsOccCurveEnabled.m_curveXType, setObsOccCurveEnabled.m_curveYType, setObsOccCurveEnabled.m_bEnabled );

			break;
		}

	case AK::Comm::IRendererProxy::MethodAddSwitchRTPC:
		{
			RendererProxyCommandData::AddSwitchRTPC addSwitchRTPC;
			if( in_rSerializer.Get( addSwitchRTPC ) )
				m_localProxy.AddSwitchRTPC( addSwitchRTPC.m_uSwitchGroup, addSwitchRTPC.m_RTPC_ID, addSwitchRTPC.m_RTPC_Type, addSwitchRTPC.m_pArrayConversion, addSwitchRTPC.m_uArraySize );
			break;
		}
		
	case AK::Comm::IRendererProxy::MethodRemoveSwitchRTPC:
		{
			RendererProxyCommandData::RemoveSwitchRTPC removeSwitchRTPC;
			if( in_rSerializer.Get( removeSwitchRTPC ) )
				m_localProxy.RemoveSwitchRTPC( removeSwitchRTPC.m_uSwitchGroup );
			break;
		}
		
	case AK::Comm::IRendererProxy::MethodSetVolumeThreshold:
		{
			RendererProxyCommandData::SetVolumeThreshold setVolumeThreshold;
			if( in_rSerializer.Get( setVolumeThreshold ) )
				m_localProxy.SetVolumeThreshold( setVolumeThreshold.m_fVolumeThreshold );
			break;
		}

	case AK::Comm::IRendererProxy::MethodSetMaxNumVoicesLimit:
		{
			RendererProxyCommandData::SetMaxNumVoicesLimit setNumVoiceGlobalLimit;
			if( in_rSerializer.Get( setNumVoiceGlobalLimit ) )
				m_localProxy.SetMaxNumVoicesLimit( setNumVoiceGlobalLimit.m_maxNumberVoices );
			break;
		}

	case AK::Comm::IRendererProxy::MethodSetMaxNumDangerousVirtVoicesLimit:
	{
		RendererProxyCommandData::SetMaxNumDangerousVirtVoicesLimit setNumVirtVoiceGlobalLimit;
		if ( in_rSerializer.Get( setNumVirtVoiceGlobalLimit ) )
			m_localProxy.SetMaxNumDangerousVirtVoicesLimit( setNumVirtVoiceGlobalLimit.m_maxNumberVoices );
		break;
	}

	case AK::Comm::IRendererProxy::MethodPostMsgMonitor:
		{
			RendererProxyCommandData::PostMsgMonitor postMsgMonitor;
			if( in_rSerializer.Get( postMsgMonitor ) )
				m_localProxy.PostMsgMonitor( postMsgMonitor.m_pszMessage );
			break;
		}
	case AK::Comm::IRendererProxy::MethodStopAll:
		{
			RendererProxyCommandData::StopAll stopAll;
			if( in_rSerializer.Get( stopAll ) )
				m_localProxy.StopAll( stopAll.m_GameObjPtr );
			break;
		}
	case AK::Comm::IRendererProxy::MethodStopPlayingID:
		{
			RendererProxyCommandData::StopPlayingID stopPlayingID;
			if( in_rSerializer.Get( stopPlayingID ) )
				m_localProxy.StopPlayingID( stopPlayingID.m_playingID, stopPlayingID.m_uTransitionDuration, stopPlayingID.m_eFadeCurve );
			break;
		}
	case AK::Comm::IRendererProxy::MethodExecuteActionOnPlayingID:
	{
		RendererProxyCommandData::ExecuteActionOnPlayingID executeActionOnPlayingID;
		if (in_rSerializer.Get(executeActionOnPlayingID))
			m_localProxy.ExecuteActionOnPlayingID(executeActionOnPlayingID.m_eActionType, executeActionOnPlayingID.m_playingID, executeActionOnPlayingID.m_uTransitionDuration, executeActionOnPlayingID.m_eFadeCurve);
		break;
	}
	case AK::Comm::IRendererProxy::MethodRegisterPlugin:
		{
			RendererProxyCommandData::RegisterPlugin regPlugin;
			if(in_rSerializer.Get(regPlugin))
				m_localProxy.RegisterPlugin(regPlugin.m_szPlugin);
			break;
		}
	case AK::Comm::IRendererProxy::MethodMonitoringMuteGameObj:
	{
		RendererProxyCommandData::MonitoringMuteGameObj monitoringMuteGameObj;
		if (in_rSerializer.Get(monitoringMuteGameObj))
			m_localProxy.MonitoringMuteGameObj(monitoringMuteGameObj.m_gameObjectPtr, monitoringMuteGameObj.m_bMute);
		break;
	}

	case AK::Comm::IRendererProxy::MethodMonitoringSoloGameObj:
	{
		RendererProxyCommandData::MonitoringSoloGameObj monitoringSoloGameObj;
		if (in_rSerializer.Get(monitoringSoloGameObj))
			m_localProxy.MonitoringSoloGameObj(monitoringSoloGameObj.m_gameObjectPtr, monitoringSoloGameObj.m_bSolo);
		break;
	}
	default:
		AKASSERT( !"Unsupported command." );
	}
	in_rSerializer.Reset();
}
#endif // #ifndef AK_OPTIMIZED
