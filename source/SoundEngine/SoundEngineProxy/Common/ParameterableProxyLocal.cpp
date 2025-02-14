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
#ifndef PROXYCENTRAL_CONNECTED



#include "ParameterableProxyLocal.h"
#include "AkParameterNodeBase.h"
#include "AkCritical.h"
#include "AkAudioLib.h"
#include "AkURenderer.h"

ParameterableProxyLocal::~ParameterableProxyLocal()
{
}

void ParameterableProxyLocal::AddChild( WwiseObjectIDext in_id )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->AddChild( in_id );
	}
}

void ParameterableProxyLocal::RemoveChild( WwiseObjectIDext in_id )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->RemoveChild( in_id );
	}
}

void ParameterableProxyLocal::SetAkProp( AkPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax )
{		
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		AkDeltaMonitor::OpenLiveEditBrace(pIndexable->key);		
		pIndexable->SetAkProp( in_eProp, in_fValue, in_fMin, in_fMax );
		AkDeltaMonitor::CloseLiveEditBrace();
	}
	
}

void ParameterableProxyLocal::SetAkProp( AkPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetAkProp( in_eProp, in_iValue, in_iMin, in_iMax );
	}
}

void ParameterableProxyLocal::PriorityApplyDistFactor( bool in_bApplyDistFactor )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetPriorityApplyDistFactor( in_bApplyDistFactor );
	}
}

void ParameterableProxyLocal::PriorityOverrideParent( bool in_bOverrideParent )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetPriorityOverrideParent( in_bOverrideParent );
	}
}

void ParameterableProxyLocal::SetStateProperties( AkUInt32 in_uStateProperties, AkStatePropertyUpdate* in_pStateProperties )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetStateProperties( in_uStateProperties, in_pStateProperties );
	}
}

void ParameterableProxyLocal::AddStateGroup( AkStateGroupID in_stateGroupID )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->AddStateGroup( in_stateGroupID );
	}
}

void ParameterableProxyLocal::RemoveStateGroup( AkStateGroupID in_stateGroupID )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		AkDeltaMonitor::LogStateUnset(pIndexable, in_stateGroupID);
		pIndexable->RemoveStateGroup( in_stateGroupID );
	}
}

void ParameterableProxyLocal::UpdateStateGroups(AkUInt32 in_uGroups, AkStateGroupUpdate* in_pGroups, AkStateUpdate* in_pUpdates)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->UpdateStateGroups( in_uGroups, in_pGroups, in_pUpdates);
	}
}

void ParameterableProxyLocal::AddState( AkStateGroupID in_stateGroupID, AkUniqueID in_stateInstanceID, AkStateID in_stateID )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->AddState( in_stateGroupID, in_stateInstanceID, in_stateID );
	}
}

void ParameterableProxyLocal::RemoveState( AkStateGroupID in_stateGroupID, AkStateID in_stateID )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->RemoveState( in_stateGroupID, in_stateID );
	}
}

void ParameterableProxyLocal::UseState( bool in_bUseState )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->UseState( in_bUseState );
	}
}

void ParameterableProxyLocal::SetStateSyncType( AkStateGroupID in_StateGroupID, AkUInt32/*AkSyncType*/ in_eSyncType )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetStateSyncType( in_StateGroupID, in_eSyncType );
	}
}

void ParameterableProxyLocal::SetFX( AkUInt32 in_uFXIndex, AkUniqueID in_uID, bool in_bShareSet )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetFX( in_uFXIndex, in_uID, in_bShareSet, SharedSetOverride_Proxy );
	}
}


void ParameterableProxyLocal::BypassAllFX( bool in_bBypass )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->MainBypassFX( ( in_bBypass ? 1 : 0 ) << AK_NUM_EFFECTS_BYPASS_ALL_FLAG, 1 << AK_NUM_EFFECTS_BYPASS_ALL_FLAG );
	}
}

void ParameterableProxyLocal::BypassFX( AkUInt32 in_uFXIndex, bool in_bBypass )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->MainBypassFX( ( in_bBypass ? 1 : 0 ) << in_uFXIndex, 1 << in_uFXIndex );
	}
}

void ParameterableProxyLocal::UpdateEffects( AkUInt32 in_uCount, AkEffectUpdate* in_pUpdates )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->UpdateEffects( in_uCount, in_pUpdates, SharedSetOverride_Proxy );
	}
}

void ParameterableProxyLocal::SetRTPC( AkRtpcID in_RTPC_ID, AkRtpcType in_RTPCType, AkRtpcAccum in_RTPCAccum, AkRTPC_ParameterID in_ParamID, AkUniqueID in_RTPCCurveID, AkCurveScaling in_eScaling, AkRTPCGraphPoint* in_pArrayConversion /*= NULL*/, AkUInt32 in_ulConversionArraySize /*= 0*/ )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->SetRTPC( in_RTPC_ID, in_RTPCType, in_RTPCAccum, in_ParamID, in_RTPCCurveID, in_eScaling, in_pArrayConversion, in_ulConversionArraySize );
	}
}

void ParameterableProxyLocal::UnsetRTPC( AkRTPC_ParameterID in_ParamID, AkUniqueID in_RTPCCurveID )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->UnsetRTPC( in_ParamID, in_RTPCCurveID );
	}
}

void ParameterableProxyLocal::MonitoringSolo( bool in_bSolo )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->MonitoringSolo( in_bSolo );
	}
}

void ParameterableProxyLocal::MonitoringMute( bool in_bMute )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->MonitoringMute( in_bMute );
	}
}

void ParameterableProxyLocal::SetAuxBusSend(AkUniqueID in_AuxBusID, AkUInt32 in_ulIndex)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		pIndexable->SetAuxBusSend(in_AuxBusID, in_ulIndex);
	}
}

void ParameterableProxyLocal::SetUseGameAuxSends(bool in_bUse)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		return pIndexable->SetUseGameAuxSends(in_bUse);
	}
}

void ParameterableProxyLocal::PosSetPositioningOverride(bool in_bOverride)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->PosSetPositioningOverride(in_bOverride);
	}
}

void ParameterableProxyLocal::PosSetPanningType(AkSpeakerPanningType in_ePannerType)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->PosSetPanningType(in_ePannerType);
	}
}

void ParameterableProxyLocal::SetListenerRelativeRouting(bool in_bHasListenerRelativeRouting)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetListenerRelativeRouting(in_bHasListenerRelativeRouting);
	}
}


void ParameterableProxyLocal::PosSetSpatializationMode(Ak3DSpatializationMode in_eMode)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->PosSetSpatializationMode(in_eMode);
	}
}

void ParameterableProxyLocal::PosSet3DPositionType(Ak3DPositionType in_e3DPositionType)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->PosSet3DPositionType(in_e3DPositionType);
	}
}

void ParameterableProxyLocal::PosSetAttenuationID(AkUniqueID in_AttenuationID)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->PosSetAttenuationID(in_AttenuationID);
	}
}

void ParameterableProxyLocal::PosEnableAttenuation(bool in_bEnableAttenuation)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->PosEnableAttenuation(in_bEnableAttenuation);
	}
}

void ParameterableProxyLocal::PosSetHoldEmitterPosAndOrient(bool in_bHoldEmitterPosAndOrient)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->PosSetHoldEmitterPosAndOrient(in_bHoldEmitterPosAndOrient);
	}
}

void ParameterableProxyLocal::PosSetHoldListenerOrient(bool in_bHoldListenerOrient)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->PosSetHoldListenerOrient(in_bHoldListenerOrient);
	}
}

void ParameterableProxyLocal::PosSetPathMode(AkPathMode in_ePathMode)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->PosSetPathMode(in_ePathMode);
	}
}

void ParameterableProxyLocal::PosSetIsLooping(bool in_bIsLooping)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->PosSetIsLooping(in_bIsLooping);
	}
}

void ParameterableProxyLocal::PosSetTransition(AkTimeMs in_TransitionTime)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->PosSetTransition(in_TransitionTime);
	}
}

void ParameterableProxyLocal::PosSetPath(
	AkPathVertex*           in_pArayVertex,
	AkUInt32                 in_ulNumVertices,
	AkPathListItemOffset*   in_pArrayPlaylist,
	AkUInt32                 in_ulNumPlaylistItem
	)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical; // WG-4418 eliminate potential problems
		pIndexable->PosSetPath(in_pArayVertex, in_ulNumVertices, in_pArrayPlaylist, in_ulNumPlaylistItem);
	}
}

void ParameterableProxyLocal::PosUpdatePathPoint(
	AkUInt32 in_ulPathIndex,
	AkUInt32 in_ulVertexIndex,
	const AkVector& in_ptPosition,
	AkTimeMs in_DelayToNext
	)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical; // WG-4418 eliminate potential problems

		pIndexable->PosUpdatePathPoint(in_ulPathIndex, in_ulVertexIndex, in_ptPosition, in_DelayToNext);
	}
}

void ParameterableProxyLocal::PosSetPathRange(AkUInt32 in_ulPathIndex, AkReal32 in_fXRange, AkReal32 in_fYRange, AkReal32 in_fZRange)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->PosSetPathRange(in_ulPathIndex, in_fXRange, in_fYRange, in_fZRange);
	}
}

void ParameterableProxyLocal::SetMixerPlugin( AkUniqueID in_uID, bool in_bShareSet )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetMixerPlugin( in_uID, in_bShareSet, SharedSetOverride_Proxy );
	}
}

void ParameterableProxyLocal::SetOverrideAttachmentParams( bool in_bOverride )
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetOverrideAttachmentParams( in_bOverride );
	}
}

void ParameterableProxyLocal::RemoveOutputBus()
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->RemoveOutputBus();
	}
}

// Spatial Audio
void ParameterableProxyLocal::SetEnableDiffraction(bool in_bEnable)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetEnableDiffraction(in_bEnable);
	}
}

void ParameterableProxyLocal::SetReflectionsAuxBus(AkUniqueID in_AuxBusID)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetReflectionsAuxBus(in_AuxBusID);
	}
}

void ParameterableProxyLocal::SetOverrideReflectionsAuxBus(bool in_bOverride)
{
	CAkParameterNodeBase* pIndexable = static_cast<CAkParameterNodeBase*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetOverrideReflectionsAuxBus(in_bOverride);
	}
}

#endif
#endif
