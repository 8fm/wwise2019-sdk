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

#include "IParameterableProxy.h"
#include "AkSharedEnum.h"

struct AkPathVertex;
struct AkPathListItemOffset;

class IParameterNodeProxy : virtual public IParameterableProxy
{
	DECLARE_BASECLASS( IParameterableProxy );
public:
	
	virtual void OverrideFXParent( bool in_bIsFXOverrideParent ) = 0;

	virtual void SetBelowThresholdBehavior( AkBelowThresholdBehavior in_eBelowThresholdBehavior ) = 0;
	virtual void SetMaxNumInstancesOverrideParent( bool in_bOverride ) = 0;
	virtual void SetVVoicesOptOverrideParent( bool in_bOverride ) = 0;
	virtual void SetMaxNumInstances( AkUInt16 in_u16MaxNumInstance ) = 0;
	virtual void SetIsGlobalLimit( bool in_bIsGlobalLimit ) = 0;
	virtual void SetMaxReachedBehavior( bool in_bKillNewest ) = 0;
	virtual void SetOverLimitBehavior( bool in_bUseVirtualBehavior ) = 0;
	virtual void SetVirtualQueueBehavior( AkVirtualQueueBehavior in_eBehavior ) = 0;
	
	virtual void SetOverrideGameAuxSends( bool in_bOverride ) = 0;
	virtual void SetOverrideUserAuxSends( bool in_bOverride ) = 0;
	virtual void SetOverrideMidiEventsBehavior( bool in_bOverride ) = 0;
	virtual void SetOverrideMidiNoteTracking( bool in_bOverride ) = 0;
	virtual void SetEnableMidiNoteTracking( bool in_bEnable ) = 0;
	virtual void SetIsMidiBreakLoopOnNoteOff( bool in_bBreak ) = 0;

	virtual void SetOverrideHdrEnvelope( bool in_bOverride ) = 0;
	virtual void SetOverrideAnalysis( bool in_bOverride ) = 0;
	virtual void SetNormalizeLoudness( bool in_bNormalizeLoudness ) = 0;
	virtual void SetEnableEnvelope( bool in_bEnableEnvelope ) = 0;

	enum MethodIDs
	{
		MethodOverrideFXParent = __base::LastMethodID,

		MethodSetBelowThresholdBehavior,
		MethodSetMaxNumInstancesOverrideParent,
		MethodSetVVoicesOptOverrideParent,
		MethodSetMaxNumInstances,
		MethodSetIsGlobalLimit,
		MethodSetMaxReachedBehavior,
		MethodSetOverLimitBehavior,
		MethodSetVirtualQueueBehavior,
		
		MethodSetOverrideGameAuxSends,
		MethodSetOverrideUserAuxSends,

		MethodSetOverrideHdrEnvelope,
		MethodSetOverrideAnalysis,
		MethodSetNormalizeLoudness,
		MethodSetEnableEnvelope,

		MethodSetOverrideMidiEventsBehavior,
		MethodSetOverrideMidiNoteTracking,
		MethodSetEnableMidiNoteTracking,
		MethodSetIsMidiBreakLoopOnNoteOff,

		LastMethodID
	};
};

namespace ParameterNodeProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1
	< 
	IParameterNodeProxy::MethodPosSetSpatializationMode,
	AkUInt32
	> PosSetSpatializationMode;

	typedef ObjectProxyCommandData::CommandData1
		<
		IParameterNodeProxy::MethodPosSet3DPositionType,
		AkUInt32
		> PosSet3DPositionType;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodPosSetAttenuationID,
		AkUniqueID
	> PosSetAttenuationID;

	typedef ObjectProxyCommandData::CommandData1
	<
		IParameterNodeProxy::MethodPosEnableAttenuation,
		bool
	> PosEnableAttenuation;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodPosSetHoldEmitterPosAndOrient,
		bool
	> PosSetHoldEmitterPosAndOrient;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodPosSetHoldListenerOrient,
		bool
	> PosSetHoldListenerOrient;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodPosSetPathMode,
		AkUInt32
	> PosSetPathMode;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodPosSetIsLooping,
		bool
	> PosSetIsLooping;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodPosSetTransition,
		AkTimeMs
	> PosSetTransition;

	struct PosSetPath : public ObjectProxyCommandData::CommandData
	{
		PosSetPath();
		~PosSetPath();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkPathVertex* m_pArrayVertex;
		AkUInt32 m_ulNumVertices;
		AkPathListItemOffset* m_pArrayPlaylist;
		AkUInt32 m_ulNumPlaylistItem;

	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	typedef ObjectProxyCommandData::CommandData4
	< 
		IParameterNodeProxy::MethodPosUpdatePathPoint,
		AkUInt32,
		AkUInt32,
		AkVector,
		AkTimeMs
	> PosUpdatePathPoint;

	typedef ObjectProxyCommandData::CommandData4
	< 
		IParameterNodeProxy::MethodPosSetPathRange,
		AkUInt32,
		AkReal32,
		AkReal32,
		AkReal32
	> PosSetPathRange;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodOverrideFXParent,
		bool
	> OverrideFXParent;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodSetBelowThresholdBehavior,
		AkUInt32
	> SetBelowThresholdBehavior;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodSetMaxNumInstancesOverrideParent,
		bool
	> SetMaxNumInstancesOverrideParent;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodSetVVoicesOptOverrideParent,
		bool
	> SetVVoicesOptOverrideParent;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodSetMaxNumInstances,
		AkUInt16
	> SetMaxNumInstances;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodSetIsGlobalLimit,
		bool
	> SetIsGlobalLimit;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodSetMaxReachedBehavior,
		bool
	> SetMaxReachedBehavior;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodSetOverLimitBehavior,
		bool
	> SetOverLimitBehavior;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodSetVirtualQueueBehavior,
		AkUInt32
	> SetVirtualQueueBehavior;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodSetOverrideGameAuxSends,
		bool
	> SetOverrideGameAuxSends;

	typedef ObjectProxyCommandData::CommandData1
		< 
		IParameterNodeProxy::MethodSetOverrideMidiEventsBehavior,
		bool
		> SetOverrideMidiEventsBehavior;

	typedef ObjectProxyCommandData::CommandData1
		< 
		IParameterNodeProxy::MethodSetOverrideMidiNoteTracking,
		bool
		> SetOverrideMidiNoteTracking;

	typedef ObjectProxyCommandData::CommandData1
		< 
		IParameterNodeProxy::MethodSetEnableMidiNoteTracking,
		bool
		> SetEnableMidiNoteTracking;

	typedef ObjectProxyCommandData::CommandData1
		< 
		IParameterNodeProxy::MethodSetIsMidiBreakLoopOnNoteOff,
		bool
		> SetIsMidiBreakLoopOnNoteOff;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodSetOverrideUserAuxSends,
		bool
	> SetOverrideUserAuxSends;
	
	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodSetOverrideHdrEnvelope,
		bool
	> SetOverrideHdrEnvelope;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodSetOverrideAnalysis,
		bool
	> SetOverrideAnalysis;
	
	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodSetNormalizeLoudness,
		bool
	> SetNormalizeLoudness;
	
	typedef ObjectProxyCommandData::CommandData1
	< 
		IParameterNodeProxy::MethodSetEnableEnvelope,
		bool
	> SetEnableEnvelope;
}

#endif // #ifndef AK_OPTIMIZED
