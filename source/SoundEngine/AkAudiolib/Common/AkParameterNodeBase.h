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
// AkParameterNodeBase.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _BASE_PARAMETER_NODE_H_
#define _BASE_PARAMETER_NODE_H_

#include "ActivityChunk.h"
#include "AkPBIAware.h"
#include "AkSIS.h"
#include "AkParamNodeStateAware.h"
#include "AkRTPCSubscriber.h"

struct AkObjectInfo;
struct AkPositioningInfo;


enum AkNodeCategory
{
	AkNodeCategory_Bus				= 0,	// The node is a bus
	AkNodeCategory_ActorMixer		= 1,	// The node is an actro-mixer
	AkNodeCategory_RanSeqCntr		= 2,	// The node is a Ran/Seq container
	AkNodeCategory_Sound			= 3,	// The node is a sound
	AkNodeCategory_SwitchCntr		= 4,	// The node is a Switch container
	AkNodeCategory_LayerCntr		= 5,	// The node is a Layer container


    AkNodeCategory_MusicTrack		= 6,	// The node is a Music Track
    AkNodeCategory_MusicSegment		= 7,	// The node is a Music Segment
    AkNodeCategory_MusicRanSeqCntr	= 8,	// The node is a Music Multi-Sequence container
    AkNodeCategory_MusicSwitchCntr	= 9,	// The node is a Music Switch container
	AkNodeCategory_AuxBus			= 10,	// The node is an Auxilliary bus node

	AkNodeCategory_None				= 1000
};

enum MidiPlayOnNoteType
{
	MidiPlayOnNoteType_NoteOn	= 1,
	MidiPlayOnNoteType_NoteOff	= 2
};

enum MidiEventActionType
{
	MidiEventActionType_NoAction	= 0,
	MidiEventActionType_Play		= 1,
	MidiEventActionType_Break		= 2,
	MidiEventActionType_Stop		= 3 // only used internally
};

enum SharedSetOverride
{
	// Here order matters. The higher, the most important...
	SharedSetOverride_Bank = 0,
	SharedSetOverride_Proxy = 1,
	SharedSetOverride_SDK = 2
};

class SharedSetOverrideBox
{
public:
	SharedSetOverrideBox()
		: m_eSharedSetOverride( SharedSetOverride_Bank )// Bank is lowest priority
	{}

	bool AcquireOverride( const SharedSetOverride in_writer )
	{
		if( m_eSharedSetOverride > in_writer )
		{
			// You are not allowed to modify this object Effect list when connecting and live editing, most likely because the SDK API call overrided what you are trying to change. 
			return false;
		}
		else
		{
			m_eSharedSetOverride = in_writer;
			return true;
		}
	}

private:
	SharedSetOverride m_eSharedSetOverride;
};

struct NotifParams
{
	AkRTPC_ParameterID			eType;
	AkRTPCKey					rtpcKey;
	bool						bIsFromBus;
	AkRTPCExceptionChecker*		pExceptCheck;
	AkReal32					fValue;
	AkReal32					fDelta;
};

struct AkEffectUpdate
{
	AkUniqueID ulEffectID;
	AkUInt8 uiIndex;
	bool bShared;
};

typedef void( *AkForAllPBIFunc )(
	CAkPBI * in_pPBI,
	const AkRTPCKey & in_rtpcKey,
	void * in_pCookie 
	);

#define AK_ForwardToBusType_Normal (1)
#define AK_ForwardToBusType_ALL (AK_ForwardToBusType_Normal)

struct CounterParameters
{
	CounterParameters()
		: fPriority( AK_MIN_PRIORITY )
		, pGameObj( NULL )
		, pLimiters( NULL )
		, uiFlagForwardToBus( AK_ForwardToBusType_ALL )
		, ui16NumKicked( 0 )
		, bMaxConsidered( false )
		, bAllowKick( true )
	{}

	AkReal32 fPriority;
	CAkRegisteredObj* pGameObj;
	AkLimiters* pLimiters;
	AkUInt16 uiFlagForwardToBus;
	AkUInt16 ui16NumKicked;
	bool bMaxConsidered;
	bool bAllowKick;
};


struct AkGameSync
{
	AkGameSync(): m_eGroupType(AkGroupType_State), m_ulGroup(0) {}
	AkGameSync(AkGroupType in_eGroupType, AkUInt32 in_ulGroup ): m_eGroupType(in_eGroupType), m_ulGroup(in_ulGroup) {}

	AkGroupType m_eGroupType;
	AkUInt32 m_ulGroup;
};

typedef AkArray<AkGameSync, AkGameSync, ArrayPoolDefault> AkGameSyncArray;

typedef AkArray<AkSrcTypeInfo*, AkSrcTypeInfo*, ArrayPoolDefault> AkSoundArray;

class CAkParameterNodeBase: public CAkPBIAware, public CAkParamNodeStateAware, public CAkRTPCSubscriberNode
{
protected:
	CAkParameterNodeBase(AkUniqueID in_ulID = 0);
	
public:
	CAkParameterNodeBase * pNextItem; // For CAkURenderer::m_listActiveNodes
    virtual ~CAkParameterNodeBase();
	virtual void OnPreRelease(){FlushStateTransitions();}//( WG-19178 )

	AKRESULT Init()
	{ 
		//Initializing m_bIsBusCategory here and storing it inside a member.
		//must be kept as a member here as the destructor needs to know its type to call RemoveChild on parent and RemoveFromIndex.
		AkNodeCategory eCategory = NodeCategory();
		m_bIsBusCategory = 
			   eCategory == AkNodeCategory_Bus
			|| eCategory == AkNodeCategory_AuxBus;

		AddToIndex();
		return AK_Success; 
	}

	// Adds the Node in the General indes
	void AddToIndex();

	// Removes the Node from the General indes
	void RemoveFromIndex();

	virtual AkUInt32 AddRef();
	virtual AkUInt32 Release();

	// Set the parent of the AudioNode 
    //
    // Return - CAkParameterNodeBase* - true if Element is playing, else false
	virtual void Parent(CAkParameterNodeBase* in_pParent);

	CAkParameterNodeBase* Parent() const { return m_pParentNode; }

	virtual void ParentBus(CAkParameterNodeBase* in_pParentBus);

	CAkParameterNodeBase* ParentBus() const 
	{
		AKASSERT( m_pBusOutputNode == NULL || (m_overriddenParams.IsSet(RTPC_OutputBusVolume) && m_overriddenParams.IsSet(RTPC_OutputBusLPF) && m_overriddenParams.IsSet(RTPC_OutputBusHPF) ));
		AKASSERT( m_pBusOutputNode != NULL || !(m_overriddenParams.IsSet(RTPC_OutputBusVolume) || m_overriddenParams.IsSet(RTPC_OutputBusLPF) || m_overriddenParams.IsSet(RTPC_OutputBusHPF) ));
		
		return m_pBusOutputNode; 
	}

	// Notify the Children that the game object was unregistered
	virtual void Unregister(
		CAkRegisteredObj * in_pGameObj
		);

	virtual AKRESULT AddChildInternal( CAkParameterNodeBase* in_pChild );

	AKRESULT AddChildByPtr( CAkParameterNodeBase* in_pChild )
	{
		in_pChild->AddRef(); //AddChildInternal Will hold the ref and release in case of error.
		return AddChildInternal( in_pChild );
	}

	virtual AKRESULT AddChild( WwiseObjectIDext in_ulID )
	{
		if(!in_ulID.id)
		{
			return AK_InvalidID;
		}
		
		CAkParameterNodeBase* pAudioNode = g_pIndex->GetNodePtrAndAddRef( in_ulID.id, AkNodeType_Default );
		if ( !pAudioNode )
			return AK_IDNotFound;

		return AddChildInternal( pAudioNode );
	}

    virtual void RemoveChild(
        CAkParameterNodeBase* in_pChild  // Child to remove
		);

	virtual void RemoveChild( WwiseObjectIDext /*in_ulID*/ ){}

	virtual void RemoveOutputBus();

	virtual void GetChildren( AkUInt32& io_ruNumItems, AkObjectInfo* out_aObjectInfos, AkUInt32& index_out, AkUInt32 iDepth );

	virtual void GatherSounds( AkSoundArray& io_aActiveSounds, AkSoundArray& io_aInactiveSounds, AkGameSyncArray& io_aGameSyncs, bool in_bIsActive, CAkRegisteredObj* in_pGameObj, AkUInt32 in_uUpdateGameSync, AkUInt32 in_uNewGameSyncValue ){};

	//Return the Node Category
	//
	// Return - AkNodeCategory - Type of the node
	virtual AkNodeCategory NodeCategory() = 0;

	bool IsBusCategory() const
	{
		return m_bIsBusCategory;
	}

	// IsPlayable main purpose is to help identify easy to see dead end playback.
	// In case of doubt, return true.
	// Sounds should check if they have a source.
	virtual bool IsPlayable(){ return true; }

	void Stop( 
		CAkRegisteredObj * in_pGameObj = NULL, 
		AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID, 
		AkTimeMs in_uTransitionDuration = 0,
		AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear );

	void Pause( 
		CAkRegisteredObj * in_pGameObj = NULL, 
		AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID, 
		AkTimeMs in_uTransitionDuration = 0,
		AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear );

	void Resume( 
		CAkRegisteredObj * in_pGameObj = NULL, 
		AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID, 
		AkTimeMs in_uTransitionDuration = 0,
		AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear );

	virtual void ExecuteAction( ActionParams& in_rAction ) = 0;
	virtual void ExecuteActionNoPropagate(ActionParams& in_rAction) = 0;
	virtual void ExecuteActionExcept( ActionParamsExcept& in_rAction ) = 0;
	virtual void ExecuteActionExceptParentCheck( ActionParamsExcept& in_rAction ) = 0;

	virtual void PlayToEnd( CAkRegisteredObj * in_pGameObj, CAkParameterNodeBase* in_pNodePtr, AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID) = 0;

	virtual void ParamNotification( NotifParams& in_rParams ) = 0;

	virtual void Notification(
		AkRTPC_ParameterID in_ParamID, 
		AkReal32 in_fDelta,						// Param variation
		AkReal32 in_fValue,						// Param value
		const AkRTPCKey& in_rtpcKey = AkRTPCKey(),				
		AkRTPCExceptionChecker* in_pExceptCheck = NULL
		);

	void PriorityNotification(
		AkReal32 in_Priority,					// Priority variation
		CAkRegisteredObj * in_pGameObj = NULL,	// Target Game Object
		AkRTPCExceptionChecker* in_pExceptCheck = NULL
		);


	// Notify the children PBI that a mute variation occured
	virtual void MuteNotification(
		AkReal32		in_fMuteRatio,	// New muting ratio
		AkMutedMapItem& in_rMutedItem,	// Node identifier (instigator's unique key)
		bool			in_bIsFromBus = false
		)=0;

	// Notify the children PBI that a mute variation occured
	virtual void MuteNotification(
		AkReal32 in_fMuteRatio,			// New muting ratio
		CAkRegisteredObj * in_pGameObj,	// Target Game Object
		AkMutedMapItem& in_rMutedItem,	// Node identifier (instigator's unique key)
		bool in_bPrioritizeGameObjectSpecificItems = false
		)=0;

	// Notify the children PBI that a change int the positioning parameters occured from RTPC
	virtual void PropagatePositioningNotification(
		AkReal32			in_RTPCValue,	// Value
		AkRTPC_ParameterID	in_ParameterID,	// RTPC ParameterID, must be a positioning ID.
		CAkRegisteredObj *		in_GameObj,		// Target Game Object
		AkRTPCExceptionChecker*	in_pExceptCheck = NULL
		)=0;

	// Music notifications. Does nothing by default.
	virtual void MusicNotification(
		AkReal32			/*in_RTPCValue*/,	// Value
		AkRTPC_ParameterID	/*in_ParameterID*/,	// RTPC ParameterID, must be a positioning ID.
		CAkRegisteredObj *	/*in_GameObj*/		// Target Game Object
		) {}

	void ClearLimitersAndMakeDirty();
	virtual void ClearLimiters() = 0;

#ifndef AK_OPTIMIZED
	virtual void UpdateRTPC();
	virtual void ResetRTPC();
#endif

	void SetListenerRelativeRouting(bool in_bHasListenerRelativeRouting);
	
	bool HasListenerRelativeRouting() const;

	inline AkSpeakerPanningType GetPannerType() { return (AkSpeakerPanningType)m_ePannerType; }

	virtual void ForAllPBI( 
		AkForAllPBIFunc in_funcForAll,
		const AkRTPCKey & in_rtpcKey,
		void * in_pCookie ) = 0;

	virtual void NotifyBypass(
		AkUInt32 in_bitsFXBypass,
		AkUInt32 in_uTargetMask,
		CAkRegisteredObj * in_pGameObj = NULL,
		AkRTPCExceptionChecker* in_pExceptCheck = NULL
		) = 0;

#ifndef AK_OPTIMIZED
	void InvalidateAllPaths();
	virtual void InvalidatePositioning(); // Live Editing
#endif

	bool PositioningInfoOverrideParent() const { return (m_overriddenParams & RTPC_POSITIONING_BITFIELD) != 0; }
	void SetPositioningInfoOverrideParent(bool in_bSet);

	virtual bool GetMaxRadius(AkReal32 & /*out_fRadius*/);

	// Get2DParams returns true if using RTPC
	void Get2DParams( const AkRTPCKey& in_rtpcKey, BaseGenParams* in_pBasePosParams );
	void Get3DPanning( const AkRTPCKey& in_rtpcKey, AkVector & out_posPan );

	static bool IsException( CAkParameterNodeBase* in_pNode, ExceptionList& in_rExceptionList );

	// Gets the Next Mixing Bus associated to this node
	//
	// RETURN - CAkBus* - The next mixing bus pointer.
	//						NULL if no mixing bus found
	virtual CAkBus* GetMixingBus();

	CAkBus* GetControlBus();

	virtual void UpdateFx(
		AkUInt32	   	in_uFXIndex
		) = 0;

	// Used to increment/decrement the playcount used for notifications and ducking
	virtual AKRESULT IncrementPlayCount(
		CounterParameters& io_params,
		bool in_bNewInstance,
		bool in_bSwitchingOutputbus = false
		) = 0;

	virtual void DecrementPlayCount(
		CounterParameters& io_params
		) = 0;

	virtual bool IncrementActivityCount( AkUInt16 in_flagForwardToBus = AK_ForwardToBusType_ALL );
	virtual void DecrementActivityCount( AkUInt16 in_flagForwardToBus = AK_ForwardToBusType_ALL );

	virtual AKRESULT PrepareData() = 0;
	virtual void UnPrepareData() = 0;

	static AKRESULT PrepareNodeData( AkUniqueID in_NodeID );
	static void UnPrepareNodeData( AkUniqueID in_NodeID );

	virtual void TriggerModulators( const AkModulatorTriggerParams& in_params, CAkModulatorData* out_pModPBIData, bool in_bDoBusCheck = true ) const;

	//	Fill the AkModulatorParamXfrmArray struct with parameters and their transformations.
	//		Used by the modulators to to apply audio-rate modulation.
	//
	// Return - AKRESULT - AK_Success if succeeded
	AKRESULT GetModulatorParamXfrms(
		AkModulatorParamXfrmArray&	io_paramsXforms,
		AkRtpcID					in_modulatorID,		
		const CAkRegisteredObj *	in_GameObject,
		bool						in_bDoBusCheck = true
		) const;

	virtual AKRESULT GetAudioParameters(
		AkSoundParams&	out_Parameters,			// Set of parameter to be filled		
		AkMutedMap&			io_rMutedMap,			// Map of muted elements
		const AkRTPCKey&	in_rtpcKey,				// Key to retrieve rtpc parameters
		AkPBIModValues*		io_pRanges,				// Range structure to be filled (or NULL if not required)
		AkModulatorsToTrigger* in_pTriggerModulators,			// Params to trigger modulators (or NULL if not required)
		bool				in_bDoBusCheck = true,
		CAkParameterNodeBase*	in_pStopAtNode = NULL
		) = 0;

	// Set a runtime property value (SIS)
	virtual void SetAkProp(
		AkPropID in_eProp, 
		CAkRegisteredObj * in_pGameObj,		// Game object associated to the action
		AkValueMeaning in_eValueMeaning,	// Target value meaning
		AkReal32 in_fTargetValue = 0,		// Target value
		AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,
		AkTimeMs in_lTransitionTime = 0
		) = 0;

	// Reset a runtime property value (SIS)
	virtual void ResetAkProp(
		AkPropID in_eProp, 
		AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,
		AkTimeMs in_lTransitionTime = 0
		);

	// Mute the element
	virtual void Mute(
		CAkRegisteredObj *	in_pGameObj,
		AkCurveInterpolation		in_eFadeCurve = AkCurveInterpolation_Linear,
		AkTimeMs		in_lTransitionTime = 0
		) = 0;
	
	// Unmute the element for the specified game object
	virtual void Unmute(
		CAkRegisteredObj *	in_pGameObj,					//Game object associated to the action
		AkCurveInterpolation		in_eFadeCurve = AkCurveInterpolation_Linear,
		AkTimeMs		in_lTransitionTime = 0
		);

	// Un-Mute the element(per object and main)
	virtual void UnmuteAll(
		AkCurveInterpolation		in_eFadeCurve = AkCurveInterpolation_Linear,
		AkTimeMs		in_lTransitionTime = 0
		) = 0;

	// starts a transition on a given SIS
	void StartSISTransition(
		CAkSIS * in_pSIS,
		AkPropID in_ePropID,
		AkReal32 in_fTargetValue,
		AkValueMeaning in_eValueMeaning,
		AkCurveInterpolation in_eFadeCurve,
		AkTimeMs in_lTransitionTime );

	// starts a mute transition on a given SIS
	void StartSisMuteTransitions(CAkSIS*		in_pSIS,
								AkReal32		in_fTargetValue,
								AkCurveInterpolation		in_eFadeCurve,
								AkTimeMs		in_lTransitionTime);

	virtual void RecalcNotification(bool in_bLiveEdit, bool in_bLog = false) {};
	virtual void RecalcNotificationWithID( bool /*in_bLiveEdit*/, AkRTPC_ParameterID /*in_rtpcID*/ ) {}
	virtual void RecalcNotificationWithBitArray( bool /*in_bLiveEdit*/, const AkRTPCBitArray& /*in_rtpcID*/ ) {}

	void SetMaxReachedBehavior( bool in_bKillNewest );
	void SetOverLimitBehavior( bool in_bUseVirtualBehavior );
	void SetMaxNumInstances( AkUInt16 in_u16MaxNumInstance );
	void SetIsGlobalLimit( bool in_bIsGlobalLimit );
	virtual void ApplyMaxNumInstances( AkUInt16 in_u16MaxNumInstance ) = 0;
	void SetVVoicesOptOverrideParent( bool in_bOverride );
	void SetOverrideMidiEventsBehavior( bool in_bOverride );
	void SetOverrideMidiNoteTracking( bool in_bOverride );
	void SetEnableMidiNoteTracking( bool in_bEnable );
	void SetIsMidiBreakLoopOnNoteOff( bool in_bBreak );

	void SetOverrideAttachmentParams( bool in_bOverride );

	PriorityInfo GetPriority( CAkRegisteredObj * in_GameObjPtr );

	virtual void SetAkProp( AkPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax );
	virtual void SetAkProp( AkPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax );
	virtual AkPropValue * FindCustomProp( AkUInt32 in_uPropID );

	void SetPriorityApplyDistFactor( bool in_bApplyDistFactor );
	void SetPriorityOverrideParent( bool in_bOverrideParent );

	// Set or replace an FX in the Node.
	// The emplacement is important
	AKRESULT SetFX( 
		AkUInt32 in_uFXIndex,				// Position of the FX in the array
		AkUniqueID in_uID,					// Unique id of CAkFxShareSet or CAkFxCustom
		bool in_bShareSet,					// Shared?
		SharedSetOverride in_eSharedSetOverride	// For example, Proxy cannot override FX set by API.
		);

	AKRESULT UpdateEffects(AkUInt32 in_uCount, AkEffectUpdate* in_pUpdates, SharedSetOverride in_eSharedSetOverride );

	// WAL setter for mixer plugin
	virtual AKRESULT SetMixerPlugin( AkUniqueID, bool, SharedSetOverride ) { AKASSERT( !"Only on bus" ); return AK_Fail; }
	
	AKRESULT RenderedFX(
		AkUInt32		in_uFXIndex,
		bool			in_bRendered
		);

	AKRESULT MainBypassFX(
		AkUInt32		in_bitsFXBypass,
		AkUInt32        in_uTargetMask = 0xFFFFFFFF
		);

	virtual void ResetFXBypass( 
		AkUInt32 in_bitsFXBypass,
		AkUInt32 in_uTargetMask = 0xFFFFFFFF
		);

	void BypassFX(
		AkUInt32			in_bitsFXBypass,
		AkUInt32			in_uTargetMask,
		CAkRegisteredObj *	in_pGameObj = NULL,
		bool			in_bIsFromReset = false
		);

	void ResetBypassFX(
		AkUInt32			in_uTargetMask,
		CAkRegisteredObj *	in_pGameObj = NULL
		);

	virtual void GetFX(
		AkUInt32	in_uFXIndex,
		AkFXDesc&	out_rFXInfo,
		CAkRegisteredObj *	in_GameObj = NULL
		) = 0;

	virtual void GetFXDataID(
		AkUInt32	in_uFXIndex,
		AkUInt32	in_uDataIndex,
		AkUInt32&	out_rDataID
		) = 0;

	void GetAttachedPropFX( AkFXDesc& out_rFXInfo );

	// Register a given parameter to an RTPC ID
	virtual AKRESULT SetRTPC(
		AkRtpcID			in_RTPC_ID,
		AkRtpcType			in_RTPCType,
		AkRtpcAccum			in_RTPCAccum,
		AkRTPC_ParameterID	in_ParamID,
		AkUniqueID			in_RTPCCurveID,
		AkCurveScaling		in_eScaling,
		AkRTPCGraphPoint*	in_pArrayConversion = NULL,		// NULL if none
		AkUInt32			in_ulConversionArraySize = 0,	// 0 if none
		bool				in_bNotify = true
		);

	virtual void UnsetRTPC(
		AkRTPC_ParameterID in_ParamID,
		AkUniqueID in_RTPCCurveID
		);

	AKRESULT SetAuxBusSend(AkUniqueID in_AuxBusID, AkUInt32 in_ulIndex);
	void SetOverrideGameAuxSends(bool in_bOverride);
	void SetUseGameAuxSends(bool in_bUse);
	void SetOverrideUserAuxSends(bool in_bOverride);

	// Spatial Audio
	void SetEnableDiffraction(bool in_bEnable);
	void SetReflectionsAuxBus(AkUniqueID in_AuxBusID);
	void SetOverrideReflectionsAuxBus(bool in_bOverride);
	bool GetOverrideReflectionsAuxBus() const {	return (m_overriddenParams & RTPC_SPATIAL_AUDIO_BITFIELD) != 0; }

	void RegisterParameterTarget(CAkParameterTarget* in_pTarget, const AkRTPCBitArray& in_paramsRequested, bool in_bPropagateToBusHier);
	void UnregisterParameterTarget(CAkParameterTarget* in_pTarget, const AkRTPCBitArray& in_paramsRequested, bool in_bPropagateToBusHier);

	bool GetStateSyncTypes( AkStateGroupID in_stateGroupID, CAkStateSyncArray* io_pSyncTypes, bool in_bBusChecked = false );

	virtual bool ParamOverriden( AkRTPC_ParameterID /* in_ParamID */ )
	{
		return false;
	}

	virtual bool ParamMustNotify( AkRTPC_ParameterID /* in_ParamID */ )
	{
		return true;
	}

	// HDR
	inline bool IsHdrBus() const 
	{ 
		return (m_overriddenParams & RTPC_HDR_BUS_PARAMS_BITFIELD) != 0;
	}
	inline bool IsInHdrHierarchy()
	{
		if ( IsHdrBus() )
			return true;
		else if ( ParentBus() )
			return ParentBus()->IsInHdrHierarchy();
		else if ( Parent() )
			return Parent()->IsInHdrHierarchy();
		else
			return false;
	}
	void SetHdrBus( bool in_bIsHdrBus ) 
	{ 
		m_overriddenParams.Mask(RTPC_HDR_BUS_PARAMS_BITFIELD, in_bIsHdrBus);
	}

	// Monitoring solo/mute (profiling only)
#ifndef AK_OPTIMIZED
	virtual void MonitoringSolo( bool in_bSolo );
	virtual void MonitoringMute( bool in_bMute );

	void _MonitoringSolo( bool in_bSolo, AkUInt32& io_ruSoloCount );
	void _MonitoringMute( bool in_bMute, AkUInt32& io_ruMuteCount );

	inline static void ResetMonitoringMuteSolo() 
	{ 
		g_uSoloCount = 0;
		g_uMuteCount = 0;
		g_uSoloCount_bus = 0;
		g_uMuteCount_bus = 0;
		g_bIsMonitoringMuteSoloDirty = false; 
	}
	inline static void SetMonitoringMuteSoloDirty() { g_bIsMonitoringMuteSoloDirty = true; }
	static bool IsRefreshMonitoringMuteSoloNeeded();	// Call this within global lock to know if Renderer should refresh solo/mutes.

	inline static bool IsVoiceMonitoringSoloActive() { return ( g_uSoloCount > 0 ); }
	inline static bool IsVoiceMonitoringMuteSoloActive() { return ( g_uSoloCount > 0 || g_uMuteCount > 0 ); }

	inline static bool IsBusMonitoringSoloActive() { return (g_uSoloCount_bus > 0); }
	inline static bool IsBusMonitoringMuteSoloActive() { return (g_uSoloCount_bus > 0 || g_uMuteCount_bus > 0); }

	inline static bool IsAnyMonitoringSoloActive() { return IsVoiceMonitoringSoloActive() || IsBusMonitoringSoloActive(); }
	inline static bool IsAnyMonitoringMuteSoloActive() { return IsVoiceMonitoringMuteSoloActive() || IsBusMonitoringMuteSoloActive(); }

	// Fetch monitoring mute/solo state from hierarchy, starting from leaves.
	void GetMonitoringMuteSoloState( 
		bool & io_bSolo,	// Pass false. Bit is OR'ed against each node of the signal flow.
		bool & io_bMute		// Pass false. Bit is OR'ed against each node of the signal flow.
		);
	void GetCtrlBusMonitoringMuteSoloState(
		bool & io_bSolo,	// Pass false. Bit is OR'ed against each node of the signal flow.
		bool & io_bMute		// Pass false. Bit is OR'ed against each node of the signal flow.
		);
#endif

	CAkRTPCSubscriberNode& GetSISNode(){return m_SISNode;}

	AkUInt32 GetDepth();

	//////////////////////////////////////////////////////////////////////////////
	//Positioning information setting
	//////////////////////////////////////////////////////////////////////////////

	void PosSetPanningType(AkSpeakerPanningType in_ePannerType);

#ifndef AK_OPTIMIZED
	// WAL entry point for positioning override. 
	void PosSetPositioningOverride(bool in_bOverride);
	void PosSetSpatializationMode(Ak3DSpatializationMode in_eMode);
	void PosSet3DPositionType(Ak3DPositionType in_e3DPositionType);
	void PosSetAttenuationID(AkUniqueID in_AttenuationID);
	void PosEnableAttenuation(bool in_bEnableAttenuation);
#endif

	void PosSetHoldEmitterPosAndOrient(bool in_bHoldEmitterPosAndOrient);
	void PosSetHoldListenerOrient(bool in_bHoldListenerOrient);

	AKRESULT PosSetPathMode(AkPathMode in_ePathMode);
	AKRESULT PosSetIsLooping(bool in_bIsLooping);
	AKRESULT PosSetTransition(AkTimeMs in_TransitionTime);

	AKRESULT PosSetPath(
		AkPathVertex*           in_pArrayVertex,
		AkUInt32                 in_ulNumVertices,
		AkPathListItemOffset*   in_pArrayPlaylist,
		AkUInt32                 in_ulNumPlaylistItem
		);

#ifndef AK_OPTIMIZED
	void PosUpdatePathPoint(
		AkUInt32 in_ulPathIndex,
		AkUInt32 in_ulVertexIndex,
		AkVector in_newPosition,
		AkTimeMs in_DelayToNext
		);
#endif

	void PosSetPathRange(AkUInt32 in_ulPathIndex, AkReal32 in_fXRange, AkReal32 in_fYRange, AkReal32 in_fZRange);

	AKRESULT Ensure3DAutomationAllocated();
	void Free3DAutomation();

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	AkPathState* GetPathState();
	
	// Allocate 3D parameter blob if required.
	void Get3DAutomationParams(
		CAk3DAutomationParams*& io_rp3DParams
		);

	AKRESULT GetStatic3DParams(AkPositioningInfo& out_rPosInfo);

	void UpdateBaseParams(
		const AkRTPCKey& in_rtpcKey,
		BaseGenParams * io_pBasePosParams,
		CAk3DAutomationParams * io_p3DParams
		);

	void GetPositioningParams(
		const AkRTPCKey& in_rtpcKey,
		BaseGenParams & out_basePosParams,
		AkPositioningParams & out_posParams
		);

	CAk3DAutomationParams* Get3DCloneForObject();

	AkInt16 GetBypassFX(AkUInt32 in_uFXIndex, CAkRegisteredObj * in_pGameObj);

protected:

	void FreePathInfo();

	// Entry point for positioning change notifications. A node must not notify unless positioning 
	// is defined at its own level. See WG-22498.
	virtual void PositioningChangeNotification(
		AkReal32			in_RTPCValue,	// Value
		AkRTPC_ParameterID	in_ParameterID,	// RTPC ParameterID, must be a positioning ID.
		CAkRegisteredObj *		in_GameObj,		// Target Game Object
		AkRTPCExceptionChecker*	in_pExceptCheck = NULL
		)
	{
		if ( PositioningInfoOverrideParent() )
			PropagatePositioningNotification( in_RTPCValue, in_ParameterID, in_GameObj, in_pExceptCheck );
	}

	virtual AKRESULT SetInitialParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize ) = 0;
	virtual AKRESULT SetInitialFxParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize, bool in_bPartialLoadOnly ) = 0;

	AKRESULT SetNodeBaseParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize, bool in_bPartialLoadOnly );
	AKRESULT SetPositioningParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize );
	virtual AKRESULT SetAuxParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize );
	virtual AKRESULT SetAdvSettingsParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize );

	virtual CAkSIS* GetSIS( CAkRegisteredObj * in_GameObj );

	void ApplyTransitionsOnProperties(AkSoundParams &io_Parameters, AkMutedMap& io_rMutedMap, const AkRTPCKey& in_rtpcKey);	
	AkInt16 _GetBypassAllFX(CAkRegisteredObj * in_GameObjPtr);

	// Helper
	AKRESULT GetNewResultCodeForVirtualStatus( AKRESULT in_prevError, AKRESULT in_NewError )
	{
		switch( in_NewError )
		{
		case AK_Success:
			break;

		case AK_MustBeVirtualized:
			if( in_prevError == AK_Success )
				in_prevError = AK_MustBeVirtualized;
			break;

		default:
			// Take the new error code
			in_prevError = in_NewError;
			break;
		}

		return in_prevError;
	}
	
	void ApplyAllSIS(const CAkSIS & in_rSIS, AkMutedMap& io_rMutedMap, AkSoundParams &io_Parameters);

//members
protected:

	CAkRTPCSubscriberNode m_SISNode;
	// consider merging m_pGlobalSIS into m_pMapSIS
	CAkSIS*			m_pGlobalSIS;

	typedef CAkKeyArray<CAkRegisteredObj *, CAkSIS*> AkMapSIS;
	AkMapSIS* m_pMapSIS;		// Map of specific parameters associated to an object

	struct FXStruct
	{
		AkUniqueID id;
		bool bRendered;
		bool bShareSet;
	};

	struct FXChunk : public SharedSetOverrideBox
	{
		FXChunk();
		~FXChunk();

		FXStruct aFX[ AK_NUM_EFFECTS_PER_OBJ ];
		AkUInt8 bitsMainFXBypass; // original bypass params | 0-3 is effect-specific, 4 is bypass all
	};

	FXChunk * m_pFXChunk;

	CAk3DAutomationParamsEx* m_p3DAutomationParams;

public:
	bool PriorityOverrideParent() const { return m_overriddenParams.IsSet(RTPC_Priority);	}
	bool MaxNumInstIgnoreParent() const { return m_bIgnoreParentMaxNumInst; }
	void SetMaxNumInstIgnoreParent(bool in_bSet) { 
		ClearLimitersAndMakeDirty();
		m_bIgnoreParentMaxNumInst = in_bSet;
	}

	AkUInt16	 	GetMaxNumInstances( CAkRegisteredObj * in_GameObjPtr = NULL );
	bool			IsMaxNumInstancesActivated(){ return GetMaxNumInstances() != 0; }

protected:

	AkForceInline bool HasRtpcOrState( AkRTPC_ParameterID in_rtpcId )
	{
		return HasRTPC( in_rtpcId ) || HasState( in_rtpcId );
	}

	template < typename Accumulator, typename Monitor >
	AkForceInline AkReal32 _GetRTPCAndState( AkRTPC_ParameterID in_rtpcId, const AkRTPCKey& in_rtpcKey)
	{
		AkReal32 fValue = Accumulator::BaseValue();

		if (HasState(in_rtpcId))
			Accumulator::Accumulate(fValue, GetStateParamValue<Accumulator, Monitor>(in_rtpcId));
		if( HasRTPC( in_rtpcId ) )
			Accumulator::Accumulate( fValue, GetRTPCConvertedValue<Monitor>( in_rtpcId, in_rtpcKey ) );

		return fValue;
	}

	template < typename Accumulator >
	AkForceInline AkReal32 _GetRTPCAndState(AkRTPC_ParameterID in_rtpcId, const AkRTPCKey& in_rtpcKey)
	{
		return _GetRTPCAndState<Accumulator, AkDeltaMonitorDisabled>(in_rtpcId, in_rtpcKey);
	}

	template < typename Accumulator, typename Monitor >
	AkForceInline void _GetPropAndRTPCAndState(AkReal32& out_value, AkPropID in_propId, const AkRTPCKey& in_rtpcKey)
	{		
		AkReal32 fValue = m_props.GetAkProp(in_propId, g_AkPropDefault[in_propId]).fValue;
		Accumulator::Accumulate( out_value, m_props.GetAkProp( in_propId, g_AkPropDefault[in_propId] ).fValue);
		Monitor::LogBaseValue(key, in_propId, fValue);

		AkRTPC_ParameterID rtpcId = g_AkPropRTPCID[in_propId];
		Accumulator::Accumulate(out_value, _GetRTPCAndState<Accumulator, Monitor>(rtpcId, in_rtpcKey));
	}

	template < typename Accumulator >
	AkForceInline void _GetPropAndRTPCAndState(AkReal32& out_value, AkPropID in_propId, const AkRTPCKey& in_rtpcKey)
	{
		_GetPropAndRTPCAndState<Accumulator, AkDeltaMonitorDisabled>(out_value, in_propId, in_rtpcKey);
	}

	AkForceInline void _GetPropAndRTPCAndState(AkReal32& out_value, AkPropID in_propId, const AkRTPCKey& in_rtpcKey)
	{
		_GetPropAndRTPCAndState<AccumulateAdd, AkDeltaMonitorDisabled>(out_value, in_propId, in_rtpcKey);
	}

	template < typename Accumulator >
	AkForceInline void _GetPropAndRTPCAndState( AkInt32& out_value, AkPropID in_propId, const AkRTPCKey& in_rtpcKey )
	{
		Accumulator::Accumulate( out_value, m_props.GetAkProp( in_propId, g_AkPropDefault[in_propId]).iValue );

		AkRTPC_ParameterID rtpcId = g_AkPropRTPCID[ in_propId ];
		AkReal32 fValue = _GetRTPCAndState<Accumulator>( rtpcId, in_rtpcKey );

		Accumulator::Accumulate( out_value, (AkInt32)( (fValue > 0.0f) ? (fValue + 0.5f) : (fValue - 0.5f) ) );
	}
	
	// out_fValue will be offset
	AkForceInline void GetPropAndRTPCAndState(AkInt32& out_value, AkPropID in_propId, const AkRTPCKey& in_rtpcKey)
	{
		_GetPropAndRTPCAndState<AccumulateAdd>(out_value, in_propId, in_rtpcKey);
	}

	AkForceInline void GetPropAndRTPCAndState(AkReal32& out_value, AkPropID in_propId, const AkRTPCKey& in_rtpcKey)
	{
		_GetPropAndRTPCAndState<AccumulateAdd>(out_value, in_propId, in_rtpcKey);
	}

	AkForceInline void GetPropAndRTPCAndStateMult(AkReal32& out_value, AkPropID in_propId, const AkRTPCKey& in_rtpcKey)
	{
		_GetPropAndRTPCAndState<AccumulateMult>(out_value, in_propId, in_rtpcKey);
	}

	AkForceInline void GetRTPCAndState(AkReal32& out_value, AkRTPC_ParameterID in_rtpcId, const AkRTPCKey& in_rtpcKey)
	{
		out_value = _GetRTPCAndState<AccumulateAdd>(in_rtpcId, in_rtpcKey);
	}

	AkForceInline AkReal32 GetRTPCAndState(AkRTPC_ParameterID in_rtpcId, const AkRTPCKey& in_rtpcKey)
	{
		return _GetRTPCAndState<AccumulateAdd>(in_rtpcId, in_rtpcKey);
	}

protected:
	void GetPropAndRTPCAndState(AkSoundParams& io_params, const AkRTPCKey& in_rtpcKey);
	
	// out_fValue will be overriden exclusively
	AkForceInline void GetPropAndRTPCExclusive( AkReal32& out_fValue, AkPropID in_propId, const AkRTPCKey& in_rtpcKey )
	{
		AkRTPC_ParameterID rtpcId = g_AkPropRTPCID[ in_propId ];
		if( HasRTPC( rtpcId ) )
		{
			out_fValue = GetRTPCConvertedValue( rtpcId, in_rtpcKey  );
		}
		else
		{
			out_fValue = m_props.GetAkProp( in_propId, g_AkPropDefault[in_propId]).fValue;
		}
	}

	AkForceInline void GetModulatorXfrmForPropID( AkModulatorParamXfrmArray& io_xforms, AkPropID in_propId, AkRtpcID in_modulatorID, const CAkRegisteredObj * in_GameObjPtr ) const
	{
		AkRTPC_ParameterID rtpcId = g_AkPropRTPCID[ in_propId ];
		if( HasRTPC( rtpcId ) )
			GetModulatorParamXfrm(io_xforms, rtpcId, in_modulatorID, in_GameObjPtr );
	}

	/////////////////////////////////////////////////////

	bool EnableActivityChunk();
	void DisableActivityChunk(); // Delete activity chunk if ChunkIsUseless

	bool OnNewActivityChunk();

	bool IsActivityChunkEnabled() const
	{
		return m_pActivityChunk != NULL;
	}

	void SafeDisconnectActivityChunk();
	void DeleteActivityChunk();
	
	AkInt16 GetPlayCount() const
	{ 
		if( IsActivityChunkEnabled() )
			return GetActivityChunk()->GetPlayCount();
		else
			return 0; 
	}

	AkInt16 GetRoutedToBusPlayCount() const
	{
		if (IsActivityChunkEnabled())
			return GetActivityChunk()->GetRoutedToBusPlayCount();
		else
			return 0;
	}

	AkInt16 GetActivityCount() const
	{ 
		if( IsActivityChunkEnabled() )
			return GetActivityChunk()->GetActivityCount();
		else
			return 0; 
	}

	AkInt16 GetRoutedToBusActivityCount() const
	{
		if (IsActivityChunkEnabled())
			return GetActivityChunk()->GetRoutedToBusActivityCount();
		else
			return 0;
	}

	AkUInt16 GetPlayCountValid()
	{ 
		if( IsActivityChunkEnabled() )
			return GetActivityChunk()->GetPlayCountValid();
		else
			return 0; 
	}

	AkUInt16 GetVirtualCountValid()
	{
		if( IsActivityChunkEnabled() )
			return GetActivityChunk()->GetVirtualCountValid();
		else
			return 0; 
	}

	AKRESULT IncrementPlayCountValue(bool in_bIsRoutedToBus);
	void DecrementPlayCountValue(bool in_bIsRoutedToBus);
	bool IncrementActivityCountValue(bool in_bIsRoutedToBus);
	void DecrementActivityCountValue(bool in_bIsRoutedToBus);

	void UpdateMaxNumInstanceGlobal( AkUInt16 in_u16LastMaxNumInstanceForRTPC )
	{
		if( IsActivityChunkEnabled() )
			return GetActivityChunk()->UpdateMaxNumInstanceGlobal(in_u16LastMaxNumInstanceForRTPC );
	}

public:
	void AddPBI( CAkBehavioralCtx* in_pPBI )
	{
		if (IsActivityChunkEnabled())
			GetActivityChunk()->m_listPBI.AddFirst( in_pPBI );
	}

	void RemovePBI(CAkBehavioralCtx* in_pPBI)
	{
		if( IsActivityChunkEnabled() )
		{
			GetActivityChunk()->m_listPBI.Remove( in_pPBI );

			if( GetActivityChunk()->ChunkIsUseless() )
			{
				DeleteActivityChunk();
			}
		}
	}

	bool IsGlobalLimit()
	{
		if( IsActivityChunkEnabled() )
			return GetActivityChunk()->IsGlobalLimit();
		return true;
	}

protected:

	AKRESULT ProcessGlobalLimiter( CounterParameters& io_params, bool in_bNewInstance );
	AKRESULT KickIfOverGlobalLimit( CounterParameters& io_params, AkUInt16 in_u16Max );
	void CheckAndDeleteActivityChunk();

	AKRESULT ProcessGameObjectLimiter( CounterParameters& io_params, bool in_bNewInstance );
	AKRESULT KickIfOverLimit( StructMaxInst*& in_pPerObjCount, CounterParameters& io_params, AkUInt16& in_u16Max );
	AKRESULT CreateGameObjectLimiter( CAkRegisteredObj* in_pGameObj, StructMaxInst*& out_pPerObjCount, AkUInt16 in_u16Max );
	void CheckAndDeleteGameObjLimiter( CAkRegisteredObj* in_pGameObj );

	bool IsMidiNoteTracking( AkInt32& out_iRootNote ) const;
	bool IsMidiBreakLoopOnNoteOff() const;


public:
	bool IsPlaying() const { return ( GetPlayCount() > 0 ); } // This function was NOT created as an helper to get information about if something is playing, 
														// but instead have been created to avoid forwarding uselessly notifications trough the tree.

	bool IsActiveOrPlaying() const { return IsPlaying() || IsActive(); }
	bool IsActive() const { return GetActivityCount() > 0; }

	bool DoesKillNewest(){ return m_bKillNewest; }

	virtual bool IsInfiniteLooping( CAkPBIAware* in_pInstigator );
	/////////////////////////////////////////////////////

	AkActivityChunk*	GetActivityChunk(){return m_pActivityChunk;}
	const AkActivityChunk*	GetActivityChunk() const {return m_pActivityChunk;}

	void AddToURendererActiveNodes();
	void RemoveFromURendererActiveNodes();

	struct AuxChunk
	{
		AuxChunk();

		AkUniqueID aAux[AK_NUM_USER_AUX_SEND_PER_OBJ];
	};

private:
	AkActivityChunk*	m_pActivityChunk;

protected:
	CAkParameterNodeBase*	m_pParentNode;		// Hirc Parent (optional), bus always have NULL
	CAkParameterNodeBase*	m_pBusOutputNode;	// Bus Parent (optional)
	
	AuxChunk * m_pAuxChunk;

	AkPropBundle<AkPropValue> m_props;

	AkOverridableParamsBitArray m_overriddenParams;

	AkUniqueID m_reflectionsAuxBus;

	
	AkPositioningSettings m_posSettings;

	bool GetOverrideUserAuxSends() const
	{
		return (m_overriddenParams & RTPC_USER_AUX_SEND_PARAM_BITFIELD) != 0;
	}

	bool GetOverrideGameAuxSends() const
	{
		return (m_overriddenParams & RTPC_GAME_AUX_SEND_PARAPM_BITFIELD) != 0;
	}

	AkUInt16		m_u16MaxNumInstance : 10;				// Zero being no max.

	AkUInt8			m_bKillNewest					:1;
	AkUInt8			m_bUseVirtualBehavior			:1;
	AkUInt8			m_bIsVVoicesOptOverrideParent	:1;
	AkUInt8			m_bOverrideAttachmentParams		:1;
	AkUInt8			m_bIsGlobalLimit				:1;

	AkUInt8			m_bPriorityApplyDistFactor	: 1;
	AkUInt8			m_bPlaceHolder				: 1;
	AkUInt8			m_bIsBusCategory			: 1;
	AkUInt8			m_bUseGameAuxSends			: 1;

	AkUInt8			m_bHasListenerRelativeRouting : 1;	// 

	// Positioning.
	AkUInt8/*AkSpeakerPanningType*/	m_ePannerType : AK_PANNER_NUM_STORAGE_BITS;
	
	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	// Settings from CAkParameterNode
	// Putting it in the base class allows saving significant memory
	// while having no real drawback.
	AkUInt8					m_bOverrideMidiEventsBehavior		:1;
	AkUInt8					m_bOverrideMidiNoteTracking			:1;
	AkUInt8					m_bEnableMidiNoteTracking			:1;
	AkUInt8					m_bMidiBreakLoopOnNoteOff			:1;
	////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////
	// Settings from CAkSwitchCntr
	// Putting it in the base class allows saving significant memory
	// while having no real drawback.
	AkUInt8					m_bIsContinuousValidation			:1;	// Is the validation continuous
	////////////////////////////////////////////////////////////////////////

	AkUInt8					m_bIgnoreParentMaxNumInst			:1;
	// Monitoring mute/solo.
#ifndef AK_OPTIMIZED
	AkUInt8			m_bIsSoloed	:1;		// UI explicitly set this node to SOLO.
	AkUInt8			m_bIsMuted	:1;		// UI explicitly set this node to MUTE.
	static AkUInt32	g_uSoloCount;		// Total number of nodes set to SOLO.
	static AkUInt32	g_uMuteCount;		// Total number of nodes set to MUTE.
	static AkUInt32	g_uSoloCount_bus;		// Total number of nodes set to SOLO.
	static AkUInt32	g_uMuteCount_bus;		// Total number of nodes set to MUTE.
	static bool		g_bIsMonitoringMuteSoloDirty;	// Set to true when property changes, to force an update by the renderer.
#endif

	struct Counts
	{
		Counts( AkInt16 activityCount, AkInt16 playCount)
			: m_activityCount(activityCount)
			, m_playCount(playCount)
		{}
		
		AkInt16 m_activityCount;
		AkInt16 m_playCount;
	};

	virtual void AdjustCount( CAkParameterNodeBase*& io_pParent );
	bool IncrementAllCountBus( CAkParameterNodeBase* in_pBus, Counts counts );
	bool IncrementAllCountParentBus( Counts counts );
	void DecrementAllCountBus( CAkParameterNodeBase* in_pBus, Counts counts );
	void DecrementAllCountCurrentNodeBus( Counts counts );

	void IncrementRoutedToBusPlayCount(AkInt16 in_count);
	void IncrementRoutedToBusActivityCount(AkInt16 in_count);
	void DecrementRoutedToBusPlayCount(AkInt16 in_count);
	void DecrementRoutedToBusActivityCount(AkInt16 in_count);
};

#define AKOBJECT_TYPECHECK(__Expected) \
	if (NodeCategory() != __Expected)	\
	{	\
		g_pBankManager->ReportDuplicateObject(key, __Expected, NodeCategory());	\
		return AK_DuplicateUniqueID;	\
	}

#endif // _PARAMETER_NODE_BASE_H_
