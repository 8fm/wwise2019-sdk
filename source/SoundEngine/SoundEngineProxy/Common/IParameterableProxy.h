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

#include "IObjectProxy.h"

#include "AkRTPC.h"
#include "AkParameterNodeBase.h"

class IParameterableProxy : virtual public IObjectProxy
{
	DECLARE_BASECLASS( IObjectProxy );
public:
	virtual void AddChild( WwiseObjectIDext in_id ) = 0;
	virtual void RemoveChild( WwiseObjectIDext in_id ) = 0;

	virtual void SetAkProp( AkPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax ) = 0;
	virtual void SetAkProp( AkPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax ) = 0;

	virtual void PriorityApplyDistFactor( bool in_bApplyDistFactor ) = 0;
	virtual void PriorityOverrideParent( bool in_bOverrideParent ) = 0;

	virtual void SetStateProperties( AkUInt32 in_uStateProperties, AkStatePropertyUpdate* in_pStateProperties ) = 0;
	virtual void AddStateGroup( AkStateGroupID in_stateGroupID ) = 0;
	virtual void RemoveStateGroup( AkStateGroupID in_stateGroupID ) = 0;
	virtual void UpdateStateGroups(AkUInt32 in_uGroups, AkStateGroupUpdate* in_pGroups, AkStateUpdate* in_pUpdates) = 0;	
	virtual void AddState( AkStateGroupID in_stateGroupID, AkUniqueID in_stateInstanceID, AkStateID in_stateID ) = 0;
	virtual void RemoveState( AkStateGroupID in_stateGroupID, AkStateID in_stateID ) = 0;
	virtual void UseState( bool in_bUseState ) = 0;
	virtual void SetStateSyncType( AkStateGroupID in_StateGroupID, AkUInt32/*AkSyncType*/ in_eSyncType ) = 0;
	

	virtual void SetFX( AkUInt32 in_uFXIndex, AkUniqueID in_uID, bool in_bShareSet ) = 0;
	virtual void UpdateEffects( AkUInt32 in_uCount, AkEffectUpdate* in_pUpdates ) = 0;

	virtual void BypassAllFX( bool in_bBypass ) = 0;
	virtual void BypassFX( AkUInt32 in_uFXIndex, bool in_bBypass ) = 0;

	virtual void SetRTPC( AkRtpcID in_RTPC_ID, AkRtpcType in_RTPCType, AkRtpcAccum in_RTPCAccum, AkRTPC_ParameterID in_ParamID, AkUniqueID in_RTPCCurveID, AkCurveScaling in_eScaling, AkRTPCGraphPoint* in_pArrayConversion = NULL, AkUInt32 in_ulConversionArraySize = 0 ) = 0;
	virtual void UnsetRTPC( AkRTPC_ParameterID in_ParamID, AkUniqueID in_RTPCCurveID ) = 0;

	virtual void MonitoringSolo( bool in_bSolo ) = 0;
	virtual void MonitoringMute( bool in_bMute ) = 0;

	virtual void SetAuxBusSend(AkUniqueID in_AuxBusID, AkUInt32 in_ulIndex) = 0;
	virtual void SetUseGameAuxSends(bool in_bUse) = 0;

	virtual void PosSetPositioningOverride(bool in_bOverride) = 0;
	virtual void PosSetPanningType(AkSpeakerPanningType in_ePannerType) = 0;
	virtual void SetListenerRelativeRouting(bool in_bHasListenerRelativeRouting) = 0;

	virtual void PosSetSpatializationMode(Ak3DSpatializationMode in_eMode) = 0;
	virtual void PosSet3DPositionType(Ak3DPositionType in_e3DPositionType) = 0;
	virtual void PosSetAttenuationID(AkUniqueID in_AttenuationID) = 0;
	virtual void PosEnableAttenuation(bool in_bEnableAttenuation) = 0;

	virtual void PosSetHoldEmitterPosAndOrient(bool in_bHoldEmitterPosAndOrient) = 0;
	virtual void PosSetHoldListenerOrient(bool in_bHoldListenerOrient) = 0;

	virtual void PosSetPathMode(AkPathMode in_ePathMode) = 0;
	virtual void PosSetIsLooping(bool in_bIsLooping) = 0;
	virtual void PosSetTransition(AkTimeMs in_TransitionTime) = 0;

	virtual void PosSetPath(
		AkPathVertex*           in_pArayVertex,
		AkUInt32                 in_ulNumVertices,
		AkPathListItemOffset*   in_pArrayPlaylist,
		AkUInt32                 in_ulNumPlaylistItem
		) = 0;

	virtual void PosUpdatePathPoint(
		AkUInt32 in_ulPathIndex,
		AkUInt32 in_ulVertexIndex,
		const AkVector& in_ptPosition,
		AkTimeMs in_DelayToNext
		) = 0;

	virtual void PosSetPathRange(AkUInt32 in_ulPathIndex, AkReal32 in_fXRange, AkReal32 in_fYRange, AkReal32 in_fZRange) = 0;

	virtual void RemoveOutputBus() = 0;
	virtual void SetMixerPlugin(AkUniqueID in_uID, bool in_bShareSet) = 0;
	virtual void SetOverrideAttachmentParams(bool in_bOverride) = 0;

	// Spatial Audio
	virtual void SetEnableDiffraction(bool in_bEnable) = 0;
	virtual void SetReflectionsAuxBus(AkUniqueID in_AuxBusID) = 0;
	virtual void SetOverrideReflectionsAuxBus(bool in_bEnable) = 0;

	enum MethodIDs
	{
		MethodAddChild  = __base::LastMethodID,
		MethodRemoveChild,
		MethodSetAkPropF,
		MethodSetAkPropI,
		MethodPriorityApplyDistFactor,
		MethodPriorityOverrideParent,
		MethodSetStateProperties,
		MethodAddStateGroup,
		MethodRemoveStateGroup,
		MethodUpdateStateGroups,
		MethodAddState,
		MethodRemoveState,
		MethodUseState,
		MethodSetStateSyncType,

		MethodSetFX,
		MethodBypassAllFX,
		MethodBypassFX,
		MethodUpdateEffects,

		MethodSetRTPC,
		MethodUnsetRTPC,

		MethodMonitoringSolo,
		MethodMonitoringMute,

		MethodSetAuxBusSend,
		MethodSetUseGameAuxSends,

		MethodPosSetPositioningOverride,
		MethodPosSetPanningType,
		MethodSetListenerRelativeRouting,

		MethodPosSetSpatializationMode,
		MethodPosSet3DPositionType,
		MethodPosSetAttenuationID,
		MethodPosEnableAttenuation,

		MethodPosSetHoldEmitterPosAndOrient,
		MethodPosSetHoldListenerOrient,

		MethodPosSetPathMode,
		MethodPosSetIsLooping,
		MethodPosSetTransition,

		MethodPosSetPath,
		MethodPosUpdatePathPoint,
		MethodPosSetPathRange,

		MethodSetMixerPlugin,
		MethodSetOverrideAttachmentParams,

		MethodRemoveOutputBus,

		// Spatial Audio
		MethodSetEnableDiffraction,
		MethodSetReflectionsAuxBus,
		MethodSetOverrideReflectionsAuxBus,

		LastMethodID
	};
};

namespace ParameterableProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1
		<
		IParameterableProxy::MethodPosSetPositioningOverride,
		bool
		> PosSetPositioningOverride;

	typedef ObjectProxyCommandData::CommandData1
		<
		IParameterableProxy::MethodPosSetPanningType,
		AkUInt32
		> PosSetPanningType;

	typedef ObjectProxyCommandData::CommandData1
		<
		IParameterableProxy::MethodSetListenerRelativeRouting,
		bool
		> SetListenerRelativeRouting;

}

namespace ParameterableProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterableProxy::MethodAddChild, 
		WwiseObjectIDext // AkUniqueID and type
	> AddChild;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterableProxy::MethodRemoveChild, 
		WwiseObjectIDext // AkUniqueID and type
	> RemoveChild;

	typedef ObjectProxyCommandData::CommandData4
	< 
		IParameterableProxy::MethodSetAkPropF,
		AkUInt32, // AkPropID
		AkReal32,
		AkReal32,
		AkReal32
	> SetAkPropF;

	typedef ObjectProxyCommandData::CommandData4
	< 
		IParameterableProxy::MethodSetAkPropI,
		AkUInt32, // AkPropID
		AkInt32,
		AkInt32,
		AkInt32
	> SetAkPropI;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterableProxy::MethodPriorityApplyDistFactor, 
		bool 
	> PriorityApplyDistFactor;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterableProxy::MethodPriorityOverrideParent, 
		bool 
	> PriorityOverrideParent;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterableProxy::MethodAddStateGroup, 
		AkStateGroupID 
	> AddStateGroup;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterableProxy::MethodRemoveStateGroup, 
		AkStateGroupID 
	> RemoveStateGroup;

	struct SetStateProperties : public ObjectProxyCommandData::CommandData0<IParameterableProxy::MethodSetStateProperties>
	{
		SetStateProperties();
		SetStateProperties(IObjectProxy * in_pProxy);
		~SetStateProperties();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_uProperties;
		AkStatePropertyUpdate * m_pProperties;		

	private:

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData0<IParameterableProxy::MethodSetStateProperties> );
	};

	struct UpdateStateGroups : public ObjectProxyCommandData::CommandData0<IParameterableProxy::MethodUpdateStateGroups>
	{
		UpdateStateGroups();
		UpdateStateGroups(IObjectProxy * in_pProxy);
		~UpdateStateGroups();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_uGroups;
		AkStateGroupUpdate * m_pGroups;		
		AkStateUpdate *m_pStates;

	private:

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData0<IParameterableProxy::MethodUpdateStateGroups> );
	};
	
	typedef ObjectProxyCommandData::CommandData3
	< 
		IParameterableProxy::MethodAddState,
		AkStateGroupID,
		AkUniqueID, 
		AkStateID 
	> AddState;
	
	typedef ObjectProxyCommandData::CommandData2
	< 
		IParameterableProxy::MethodRemoveState,
		AkStateGroupID,
		AkStateID 
	> RemoveState;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterableProxy::MethodUseState, 
		bool
	> UseState;

	typedef ObjectProxyCommandData::CommandData2
	< 
		IParameterableProxy::MethodSetStateSyncType,
		AkStateGroupID,
		AkUInt32
	> SetStateSyncType;

	typedef ObjectProxyCommandData::CommandData3
	<
		IParameterableProxy::MethodSetFX,
		AkUInt32,
		AkUniqueID,
		bool
	> SetFX;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterableProxy::MethodBypassAllFX, 
		bool
	> BypassAllFX;

	typedef ObjectProxyCommandData::CommandData2
	< 
		IParameterableProxy::MethodBypassFX, 
		AkUInt32,
		bool
	> BypassFX;

	struct UpdateEffects : public ObjectProxyCommandData::CommandData0<IParameterableProxy::MethodUpdateEffects>
	{
		UpdateEffects();
		UpdateEffects(IObjectProxy * in_pProxy);
		~UpdateEffects();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkEffectUpdate *m_pUpdates;
		AkUInt32 m_uCount;

	private:

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData0<IParameterableProxy::MethodUpdateEffects> );
	};

	struct SetRTPC : public ObjectProxyCommandData::CommandData, public ProxyCommandData::CurveData<AkRTPCGraphPoint>
	{
		SetRTPC();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkRtpcID m_RTPCID;
		AkRtpcType m_RTPCType;
		AkRtpcAccum m_RTPCAccum;
		AkRTPC_ParameterID m_paramID;
		AkUniqueID m_RTPCCurveID;

	private:

		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	typedef ObjectProxyCommandData::CommandData2
	<
		IParameterableProxy::MethodUnsetRTPC,
		AkUInt32,
		AkUniqueID
	> UnsetRTPC;
	
	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterableProxy::MethodMonitoringSolo, 
		bool 
	> MonitoringSolo;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterableProxy::MethodMonitoringMute, 
		bool 
	> MonitoringMute;

	typedef ObjectProxyCommandData::CommandData2
	<
		IParameterableProxy::MethodSetMixerPlugin,
		AkUniqueID,
		bool
	> SetMixerPlugin;

	typedef ObjectProxyCommandData::CommandData1
	<
		IParameterableProxy::MethodSetOverrideAttachmentParams,
		bool
	> SetOverrideAttachmentParams;

	typedef ObjectProxyCommandData::CommandData0
		<
		IParameterableProxy::MethodRemoveOutputBus
		> RemoveOutputBus;

	typedef ObjectProxyCommandData::CommandData0
		<
		IParameterableProxy::MethodRemoveOutputBus
		> RemoveOutputBus;

	typedef ObjectProxyCommandData::CommandData1
		<
		IParameterableProxy::MethodSetUseGameAuxSends,
		bool
		> SetUseGameAuxSends;

	typedef ObjectProxyCommandData::CommandData2
		<
		IParameterableProxy::MethodSetAuxBusSend,
		AkUniqueID,
		AkUInt32
		> SetAuxBusSend;

	typedef ObjectProxyCommandData::CommandData1
		<
		IParameterableProxy::MethodSetEnableDiffraction,
		bool
		> SetEnableDiffraction;

	typedef ObjectProxyCommandData::CommandData1
		<
		IParameterableProxy::MethodSetReflectionsAuxBus,
		AkUniqueID
		> SetReflectionsAuxBus;

	typedef ObjectProxyCommandData::CommandData1
		<
		IParameterableProxy::MethodSetOverrideReflectionsAuxBus,
		bool
		> SetOverrideReflectionsAuxBus;
}

#endif // #ifndef AK_OPTIMIZED
