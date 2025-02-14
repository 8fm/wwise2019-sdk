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
#pragma once

#ifndef AK_OPTIMIZED
#ifndef PROXYCENTRAL_CONNECTED

#include "ObjectProxyLocal.h"
#include "IParameterableProxy.h"

class ParameterableProxyLocal : public ObjectProxyLocal
								, virtual public IParameterableProxy
{
public:
	virtual ~ParameterableProxyLocal();

	// IParameterableProxy members
	virtual void AddChild( WwiseObjectIDext in_id );
	virtual void RemoveChild( WwiseObjectIDext in_id );

	virtual void SetAkProp( AkPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax );
	virtual void SetAkProp( AkPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax );

	virtual void PriorityApplyDistFactor( bool in_bApplyDistFactor );
	virtual void PriorityOverrideParent( bool in_bOverrideParent );

	virtual void SetStateProperties( AkUInt32 in_uStateProperties, AkStatePropertyUpdate* in_pStateProperties );
	virtual void AddStateGroup( AkStateGroupID in_stateGroupID );
	virtual void RemoveStateGroup( AkStateGroupID in_stateGroupID );
	virtual void UpdateStateGroups(AkUInt32 in_uGroups, AkStateGroupUpdate* in_pGroups, AkStateUpdate* in_pUpdates);
	virtual void AddState( AkStateGroupID in_stateGroupID, AkUniqueID in_stateInstanceID, AkStateID in_stateID );
	virtual void RemoveState( AkStateGroupID in_stateGroupID, AkStateID in_stateID );
	virtual void UseState( bool in_bUseState );
	virtual void SetStateSyncType( AkStateGroupID in_StateGroupID, AkUInt32/*AkSyncType*/ in_eSyncType );

	virtual void SetFX( AkUInt32 in_uFXIndex, AkUniqueID in_uID, bool in_bShareSet );
	virtual void UpdateEffects( AkUInt32 in_uCount, AkEffectUpdate* in_pUpdates );

	virtual void BypassAllFX( bool in_bBypass );
	virtual void BypassFX( AkUInt32 in_uFXIndex, bool in_bBypass );

	virtual void SetRTPC( AkRtpcID in_RTPC_ID, AkRtpcType in_RTPCType, AkRtpcAccum in_RTPCAccum, AkRTPC_ParameterID in_ParamID, AkUniqueID in_RTPCCurveID, AkCurveScaling in_eScaling, AkRTPCGraphPoint* in_pArrayConversion = NULL, AkUInt32 in_ulConversionArraySize = 0 );
	virtual void UnsetRTPC( AkRTPC_ParameterID in_ParamID, AkUniqueID in_RTPCCurveID );

	virtual void MonitoringSolo( bool in_bSolo );
	virtual void MonitoringMute( bool in_bMute );

	virtual void SetAuxBusSend(AkUniqueID in_AuxBusID, AkUInt32 in_ulIndex);
	virtual void SetUseGameAuxSends(bool in_bUse);

	virtual void PosSetPositioningOverride(bool in_bOverride);
	virtual void PosSetPanningType(AkSpeakerPanningType in_ePannerType);
	virtual void SetListenerRelativeRouting(bool in_bHasListenerRelativeRouting);
	
	virtual void PosSetSpatializationMode(Ak3DSpatializationMode in_eMode);
	virtual void PosSet3DPositionType(Ak3DPositionType in_e3DPositionType);
	virtual void PosSetAttenuationID(AkUniqueID in_AttenuationID);
	virtual void PosEnableAttenuation(bool in_bEnableAttenuation);

	virtual void PosSetHoldEmitterPosAndOrient(bool in_bHoldEmitterPosAndOrient);
	virtual void PosSetHoldListenerOrient(bool in_bHoldListenerOrient);

	virtual void PosSetPathMode(AkPathMode in_ePathMode);
	virtual void PosSetIsLooping(bool in_bIsLooping);
	virtual void PosSetTransition(AkTimeMs in_TransitionTime);

	virtual void PosSetPath(
		AkPathVertex*           in_pArayVertex,
		AkUInt32                 in_ulNumVertices,
		AkPathListItemOffset*   in_pArrayPlaylist,
		AkUInt32                 in_ulNumPlaylistItem
		);

	virtual void PosUpdatePathPoint(
		AkUInt32 in_ulPathIndex,
		AkUInt32 in_ulVertexIndex,
		const AkVector& in_ptPosition,
		AkTimeMs in_DelayToNext
		);

	virtual void PosSetPathRange(AkUInt32 in_ulPathIndex, AkReal32 in_fXRange, AkReal32 in_fYRange, AkReal32 in_fZRange);

	virtual void SetMixerPlugin( AkUniqueID in_uID, bool in_bShareSet );
	virtual void SetOverrideAttachmentParams( bool in_bOverride );

	virtual void RemoveOutputBus();

	// Spatial Audio
	virtual void SetEnableDiffraction(bool in_bEnable);
	virtual void SetReflectionsAuxBus(AkUniqueID in_AuxBusID);
	virtual void SetOverrideReflectionsAuxBus(bool in_bOverride);
};

#endif
#endif // #ifndef AK_OPTIMIZED
