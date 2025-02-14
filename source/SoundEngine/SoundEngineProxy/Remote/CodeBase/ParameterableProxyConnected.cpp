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

#include "ParameterableProxyConnected.h"
#include "AkParameterNodeBase.h"
#include "CommandDataSerializer.h"
#include "IParameterableProxy.h"
#include "IParameterNodeProxy.h"

ParameterableProxyConnected::ParameterableProxyConnected()
{
}

ParameterableProxyConnected::~ParameterableProxyConnected()
{
}

void ParameterableProxyConnected::HandleExecute( AkUInt16 in_uMethodID, CommandDataSerializer& in_rSerializer, CommandDataSerializer& /*out_rReturnSerializer*/ )
{
	CAkParameterNodeBase * pNode = static_cast<CAkParameterNodeBase *>( GetIndexable() );

	switch( in_uMethodID )
	{
	case IParameterableProxy::MethodAddChild:
		{
			ParameterableProxyCommandData::AddChild addChild;
			if( in_rSerializer.Get( addChild ) )
				pNode->AddChild( addChild.m_param1 );
			break;
		}

	case IParameterableProxy::MethodRemoveChild:
		{
			ParameterableProxyCommandData::RemoveChild removeChild;
			if( in_rSerializer.Get( removeChild ) )
				pNode->RemoveChild( removeChild.m_param1 );
			break;
		}

	case IParameterableProxy::MethodSetAkPropF:
		{
			ParameterableProxyCommandData::SetAkPropF setAkProp;
			if (in_rSerializer.Get(setAkProp))
			{
				AkDeltaMonitor::OpenLiveEditBrace(pNode->key);
				pNode->SetAkProp((AkPropID)setAkProp.m_param1, setAkProp.m_param2, setAkProp.m_param3, setAkProp.m_param4);
				AkDeltaMonitor::CloseLiveEditBrace();
			}
			break;
		}

	case IParameterableProxy::MethodSetAkPropI:
		{
			ParameterableProxyCommandData::SetAkPropI setAkProp;
			if( in_rSerializer.Get( setAkProp ) )
					pNode->SetAkProp( (AkPropID) setAkProp.m_param1, setAkProp.m_param2, setAkProp.m_param3, setAkProp.m_param4 );
			break;
		}

	case IParameterableProxy::MethodPriorityApplyDistFactor:
		{
			ParameterableProxyCommandData::PriorityApplyDistFactor priorityApplyDistFactor;
			if( in_rSerializer.Get( priorityApplyDistFactor ) )
					pNode->SetPriorityApplyDistFactor( priorityApplyDistFactor.m_param1 );
			break;
		}

	case IParameterableProxy::MethodPriorityOverrideParent:
		{
			ParameterableProxyCommandData::PriorityOverrideParent priorityOverrideParent;
			if( in_rSerializer.Get( priorityOverrideParent ) )
					pNode->SetPriorityOverrideParent( priorityOverrideParent.m_param1 );
			break;
		}

	case IParameterableProxy::MethodAddStateGroup:
		{
			ParameterableProxyCommandData::AddStateGroup addStateGroup;
			if( in_rSerializer.Get( addStateGroup ) )
					pNode->AddStateGroup( addStateGroup.m_param1 );
			break;
		}

	case IParameterableProxy::MethodRemoveStateGroup:
		{
			ParameterableProxyCommandData::RemoveStateGroup removeStateGroup;
			if (in_rSerializer.Get(removeStateGroup))
			{
				AkDeltaMonitor::LogStateUnset(pNode, removeStateGroup.m_param1);
				pNode->RemoveStateGroup(removeStateGroup.m_param1);
			}
			break;
		}
	case IParameterableProxy::MethodUpdateStateGroups:
		{
			ParameterableProxyCommandData::UpdateStateGroups updateStateGroups;
			if( in_rSerializer.Get( updateStateGroups ) )
				pNode->UpdateStateGroups( updateStateGroups.m_uGroups, updateStateGroups.m_pGroups, updateStateGroups.m_pStates );
			break;
		}
	case IParameterableProxy::MethodSetStateProperties:
		{
			ParameterableProxyCommandData::SetStateProperties setStateProperties;
			if( in_rSerializer.Get( setStateProperties ) )
				pNode->SetStateProperties( setStateProperties.m_uProperties, setStateProperties.m_pProperties );
			break;
		}

	case IParameterableProxy::MethodAddState:
		{
			ParameterableProxyCommandData::AddState addState;
			if( in_rSerializer.Get( addState ) )
					pNode->AddState( addState.m_param1, addState.m_param2, addState.m_param3 );

			break;
		}

	case IParameterableProxy::MethodRemoveState:
		{
			ParameterableProxyCommandData::RemoveState removeState;
			if( in_rSerializer.Get( removeState ) )
					pNode->RemoveState( removeState.m_param1, removeState.m_param2 );

			break;
		}

	case IParameterableProxy::MethodUseState:
		{
			ParameterableProxyCommandData::UseState useState;
			if( in_rSerializer.Get( useState ) )
					pNode->UseState( useState.m_param1 );
			break;
		}

	case IParameterableProxy::MethodSetStateSyncType:
		{
			ParameterableProxyCommandData::SetStateSyncType setStateSyncType;
			if( in_rSerializer.Get( setStateSyncType ) )
					pNode->SetStateSyncType( setStateSyncType.m_param1, setStateSyncType.m_param2 );

			break;
		}

	case IParameterableProxy::MethodSetFX:
		{
			ParameterableProxyCommandData::SetFX setFX;
			if( in_rSerializer.Get( setFX ) )
				pNode->SetFX( setFX.m_param1, setFX.m_param2, setFX.m_param3, SharedSetOverride_Proxy );

			break;
		}

	case IParameterableProxy::MethodBypassAllFX:
		{
			ParameterableProxyCommandData::BypassAllFX bypassAllFX;
			if( in_rSerializer.Get( bypassAllFX ) )
			{
					AkUInt32 uBits = ( bypassAllFX.m_param1 ? 1 : 0 ) << AK_NUM_EFFECTS_BYPASS_ALL_FLAG;
					pNode->MainBypassFX( uBits, 1 << AK_NUM_EFFECTS_BYPASS_ALL_FLAG );
			}

			break;
		}

	case IParameterableProxy::MethodBypassFX:
		{
			ParameterableProxyCommandData::BypassFX bypassFX;
			if( in_rSerializer.Get( bypassFX ) )
			{
				AkUInt32 uBits = ( bypassFX.m_param2 ? 1 : 0 ) << bypassFX.m_param1;
				pNode->BypassFX( uBits, 1 << bypassFX.m_param1 );
			}

			break;
		}

	case IParameterableProxy::MethodUpdateEffects:
		{
			ParameterableProxyCommandData::UpdateEffects update;
			if( in_rSerializer.Get( update ) )
				pNode->UpdateEffects( update.m_uCount, update.m_pUpdates, SharedSetOverride_Proxy );

			break;
		}


	case IParameterableProxy::MethodSetRTPC:
		{
			ParameterableProxyCommandData::SetRTPC setRTPC;
			if( in_rSerializer.Get( setRTPC ) )
					pNode->SetRTPC( setRTPC.m_RTPCID, setRTPC.m_RTPCType, setRTPC.m_RTPCAccum, setRTPC.m_paramID, setRTPC.m_RTPCCurveID, setRTPC.m_eScaling, setRTPC.m_pArrayConversion, setRTPC.m_ulConversionArraySize );

			break;
		}

	case IParameterableProxy::MethodUnsetRTPC:
		{
			ParameterableProxyCommandData::UnsetRTPC unsetRTPC;
			if( in_rSerializer.Get( unsetRTPC ) )
					pNode->UnsetRTPC( (AkRTPC_ParameterID) unsetRTPC.m_param1, unsetRTPC.m_param2 );

			break;
		}

	case IParameterableProxy::MethodMonitoringSolo:
		{
			ParameterableProxyCommandData::MonitoringSolo monitoringSolo;
			if( in_rSerializer.Get( monitoringSolo ) )
					pNode->MonitoringSolo( monitoringSolo.m_param1 );
			break;
		}

	case IParameterableProxy::MethodMonitoringMute:
		{
			ParameterableProxyCommandData::MonitoringMute monitoringMute;
			if( in_rSerializer.Get( monitoringMute ) )
					pNode->MonitoringMute( monitoringMute.m_param1 );
			break;
		}
	
	case IParameterNodeProxy::MethodSetAuxBusSend:
		{
			ParameterableProxyCommandData::SetAuxBusSend setAuxBusSend;
			if( in_rSerializer.Get( setAuxBusSend ) )
				pNode->SetAuxBusSend( (AkUniqueID) setAuxBusSend.m_param1, (AkUInt32) setAuxBusSend.m_param2 );

			break;
		}
	case IParameterNodeProxy::MethodSetUseGameAuxSends:
		{
			ParameterableProxyCommandData::SetUseGameAuxSends setUseGameAuxSends;
			if( in_rSerializer.Get( setUseGameAuxSends ) )
				pNode->SetUseGameAuxSends( (bool) setUseGameAuxSends.m_param1 );

			break;
		}

	case IParameterableProxy::MethodPosSetPositioningOverride:
	{
		ParameterableProxyCommandData::PosSetPositioningOverride posSetPositioningOverride;
		if (in_rSerializer.Get(posSetPositioningOverride))
			pNode->PosSetPositioningOverride(posSetPositioningOverride.m_param1);

		break;
	}

	case IParameterableProxy::MethodPosSetPanningType:
		{
			ParameterableProxyCommandData::PosSetPanningType posSetPanningType;
			if (in_rSerializer.Get(posSetPanningType))
				pNode->PosSetPanningType((AkSpeakerPanningType)posSetPanningType.m_param1);

			break;
		}

	case IParameterableProxy::MethodSetListenerRelativeRouting:
		{
			ParameterableProxyCommandData::SetListenerRelativeRouting setListenerRelativeRouting;
			if (in_rSerializer.Get(setListenerRelativeRouting))
				pNode->SetListenerRelativeRouting(setListenerRelativeRouting.m_param1);

			break;
		}
	case IParameterNodeProxy::MethodPosSetSpatializationMode:
		{
			ParameterNodeProxyCommandData::PosSetSpatializationMode posSetSpatializationModeParams;
			if (in_rSerializer.Get(posSetSpatializationModeParams))
				pNode->PosSetSpatializationMode((Ak3DSpatializationMode)posSetSpatializationModeParams.m_param1);

			break;
		}
	case IParameterNodeProxy::MethodPosSet3DPositionType:
	{
		ParameterNodeProxyCommandData::PosSet3DPositionType posSet3DPositionType;
		if (in_rSerializer.Get(posSet3DPositionType))
			pNode->PosSet3DPositionType((Ak3DPositionType)posSet3DPositionType.m_param1);

		break;
	}
	case IParameterNodeProxy::MethodPosSetAttenuationID:
		{
			ParameterNodeProxyCommandData::PosSetAttenuationID SetAttenuationIDParams;
			if( in_rSerializer.Get( SetAttenuationIDParams ) )
					pNode->PosSetAttenuationID( SetAttenuationIDParams.m_param1 );

			break;
		}
	case IParameterNodeProxy::MethodPosEnableAttenuation:
	{
		ParameterNodeProxyCommandData::PosEnableAttenuation posEnableAttenuation;
		if (in_rSerializer.Get(posEnableAttenuation))
			pNode->PosEnableAttenuation(posEnableAttenuation.m_param1);

		break;
	}

	case IParameterNodeProxy::MethodPosSetHoldEmitterPosAndOrient:
		{
			ParameterNodeProxyCommandData::PosSetHoldEmitterPosAndOrient posSetHoldEmitterPosAndOrient;
			if (in_rSerializer.Get(posSetHoldEmitterPosAndOrient))
				pNode->PosSetHoldEmitterPosAndOrient(posSetHoldEmitterPosAndOrient.m_param1);

			break;
		}

	case IParameterNodeProxy::MethodPosSetHoldListenerOrient:
		{
			ParameterNodeProxyCommandData::PosSetHoldListenerOrient posSetHoldListenerOrient;
			if (in_rSerializer.Get(posSetHoldListenerOrient))
				pNode->PosSetHoldListenerOrient(posSetHoldListenerOrient.m_param1);

			break;
		}

	case IParameterNodeProxy::MethodPosSetPathMode:
		{
			ParameterNodeProxyCommandData::PosSetPathMode posSetPathMode;
			if( in_rSerializer.Get( posSetPathMode ) )
					pNode->PosSetPathMode( (AkPathMode) posSetPathMode.m_param1 );

			break;
		}

	case IParameterNodeProxy::MethodPosSetIsLooping:
		{
			ParameterNodeProxyCommandData::PosSetIsLooping posSetIsLooping;
			if( in_rSerializer.Get( posSetIsLooping ) )
					pNode->PosSetIsLooping( posSetIsLooping.m_param1 );

			break;
		}

	case IParameterNodeProxy::MethodPosSetTransition:
		{
			ParameterNodeProxyCommandData::PosSetTransition posSetTransition;
			if( in_rSerializer.Get( posSetTransition ) )
					pNode->PosSetTransition( posSetTransition.m_param1 );

			break;
		}

	case IParameterNodeProxy::MethodPosSetPath:
		{
			ParameterNodeProxyCommandData::PosSetPath posSetPath;
			if( in_rSerializer.Get( posSetPath ) )
					pNode->PosSetPath( posSetPath.m_pArrayVertex, posSetPath.m_ulNumVertices, posSetPath.m_pArrayPlaylist, posSetPath.m_ulNumPlaylistItem );

			break;
		}

	case IParameterNodeProxy::MethodPosUpdatePathPoint:
		{
			ParameterNodeProxyCommandData::PosUpdatePathPoint posUpdatePathPoint;
			if( in_rSerializer.Get( posUpdatePathPoint ) )
					pNode->PosUpdatePathPoint( posUpdatePathPoint.m_param1, posUpdatePathPoint.m_param2, posUpdatePathPoint.m_param3, posUpdatePathPoint.m_param4 );

			break;
		}

	case IParameterNodeProxy::MethodPosSetPathRange:
		{
			ParameterNodeProxyCommandData::PosSetPathRange posRange;
			if( in_rSerializer.Get( posRange ) )
					pNode->PosSetPathRange( posRange.m_param1, posRange.m_param2, posRange.m_param3, posRange.m_param4 );

			break;
		}

	case IParameterableProxy::MethodSetMixerPlugin:
		{
			ParameterableProxyCommandData::SetMixerPlugin setMixerPlugin;
			if( in_rSerializer.Get( setMixerPlugin ) )
					pNode->SetMixerPlugin( setMixerPlugin.m_param1, setMixerPlugin.m_param2, SharedSetOverride_Proxy );

			break;
		}

	case IParameterableProxy::MethodSetOverrideAttachmentParams:
		{
			ParameterableProxyCommandData::SetOverrideAttachmentParams setOverrideAttachmentParams;
			if( in_rSerializer.Get( setOverrideAttachmentParams ) )
					pNode->SetOverrideAttachmentParams( setOverrideAttachmentParams.m_param1 );

			break;
		}

	case IParameterableProxy::MethodRemoveOutputBus:
		{
			pNode->RemoveOutputBus();
			break;
		}

	case IParameterableProxy::MethodSetEnableDiffraction:
		{
			ParameterableProxyCommandData::SetEnableDiffraction data;
			if (in_rSerializer.Get(data))
				pNode->SetEnableDiffraction(data.m_param1);
			break;
		}

	case IParameterableProxy::MethodSetReflectionsAuxBus:
		{
			ParameterableProxyCommandData::SetReflectionsAuxBus data;
			if (in_rSerializer.Get(data))
				pNode->SetReflectionsAuxBus(data.m_param1);
			break;
		}

	case IParameterableProxy::MethodSetOverrideReflectionsAuxBus:
		{
			ParameterableProxyCommandData::SetOverrideReflectionsAuxBus data;
			if (in_rSerializer.Get(data))
				pNode->SetOverrideReflectionsAuxBus(data.m_param1);
			break;
		}

	default:
		AKASSERT( false );
	}
}
#endif // #ifndef AK_OPTIMIZED
