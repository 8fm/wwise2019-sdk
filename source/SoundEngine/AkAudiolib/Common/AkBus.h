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
// AkBus.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _BUS_H_
#define _BUS_H_

#include "AkActiveParent.h"
#include "AkKeyList.h"
#include <AK/Tools/Common/AkLock.h>

class CAkDuckItem;
extern AkInitSettings g_settings;
class AkDevice;

typedef AkArray<CAkBus*, CAkBus*> AkBusArray;

// class corresponding to a bus
//
// Author:  alessard
class CAkBus: public CAkActiveParent<CAkParameterNodeBase>
{
public:

	enum AkDuckingState
	{
		DuckState_OFF,
		DuckState_ON,
		DuckState_PENDING//Waiting for the notification before unducking
#define DUCKING_STATE_NUM_STORAGE_BIT 3
	};

	struct AkDuckInfo
	{
		AkVolumeValue DuckVolume;
		AkTimeMs FadeOutTime;
		AkTimeMs FadeInTime;
		AkCurveInterpolation FadeCurve;
		AkPropID TargetProp;
		bool operator ==(AkDuckInfo& in_Op)
		{
			return ( (DuckVolume == in_Op.DuckVolume) 
				&& (FadeOutTime == in_Op.FadeOutTime) 
				&& (FadeInTime == in_Op.FadeInTime) 
				&& (FadeCurve == in_Op.FadeCurve) 
				&& (TargetProp == in_Op.TargetProp)
				);
		}
	};

	//Thread safe version of the constructor
	static CAkBus* Create(AkUniqueID in_ulID = 0);

	// Check if the specified child can be connected
    //
    // Return - bool -	AK_NotCompatible
	//					AK_Succcess
	//					AK_MaxReached
    virtual AKRESULT CanAddChild(
        CAkParameterNodeBase * in_pAudioNode  // Audio node ID to connect on
        );

	using CAkActiveParent<CAkParameterNodeBase>::ParentBus;
	virtual void Parent(CAkParameterNodeBase* in_pParent);
	using CAkActiveParent<CAkParameterNodeBase>::Parent;

	virtual AKRESULT AddChildInternal( CAkParameterNodeBase* in_pChild );

	virtual AKRESULT AddChild( WwiseObjectIDext in_ulID );

	virtual void RemoveChild(
        CAkParameterNodeBase* in_pChild
		);

	virtual void RemoveChild( WwiseObjectIDext in_ulID );

	virtual void AdjustCount(CAkParameterNodeBase*& io_pParent){}

	virtual AkNodeCategory NodeCategory();	

	virtual void ExecuteAction( ActionParams& in_rAction );

	virtual void ExecuteActionExcept( ActionParamsExcept& in_rAction );

	virtual void ExecuteActionExceptParentCheck( ActionParamsExcept& in_rAction ){ ExecuteActionExcept( in_rAction ); }

	virtual void PlayToEnd(CAkRegisteredObj * in_pGameObj, CAkParameterNodeBase* in_NodePtr, AkPlayingID in_PlayingID /* = AK_INVALID_PLAYING_ID */);

	virtual void ForAllPBI( 
		AkForAllPBIFunc in_funcForAll,
		const AkRTPCKey & in_rtpcKey,
		void * in_pCookie );

	virtual void PropagatePositioningNotification(
		AkReal32			in_RTPCValue,	// 
		AkRTPC_ParameterID	in_ParameterID,	// RTPC ParameterID, must be a Positioning ID.
		CAkRegisteredObj *		in_GameObj,		// Target Game Object
		AkRTPCExceptionChecker*	in_pExceptCheck = NULL
		);

	virtual AKRESULT GetAudioParameters(
		AkSoundParams&	out_Parameters,	// Set of parameter to be filled		
		AkMutedMap&			io_rMutedMap,			// Map of muted elements
		const AkRTPCKey&	in_rtpcKey,				// Key to retrieve rtpc parameters
		AkPBIModValues*		io_pRanges,				// Range structure to be filled (or NULL if not required)
		AkModulatorsToTrigger* in_pTriggerModulators,	// Trigger modulators (or NULL if not required)
		bool				in_bDoBusCheck = true,
		CAkParameterNodeBase*   in_pStopAtNode = NULL
		);

	void GetNonMixingBusParameters(
		AkSoundParams &io_Parameters,  // Set of parameter to be filled
		const AkRTPCKey & in_rtpcKey   // Key to retrieve rtpc parameters
		);
	void GetMixingBusParameters(
		AkSoundParams&	out_Parameters,	// Set of parameter to be filled
		const AkRTPCKey&	in_rtpcKey				// Key to retrieve rtpc parameters
		);

	virtual void TriggerModulators( const AkModulatorTriggerParams& in_params, CAkModulatorData* out_pModPBIData, bool in_bStopAtMixingBus ) const;	

	// Set a runtime property value (SIS)
	virtual void SetAkProp(
		AkPropID in_eProp, 
		CAkRegisteredObj * in_pGameObj,		// Game object associated to the action
		AkValueMeaning in_eValueMeaning,	// Target value meaning
		AkReal32 in_fTargetValue = 0,		// Target value
		AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,
		AkTimeMs in_lTransitionTime = 0
		);

	using CAkActiveParent<CAkParameterNodeBase>::SetAkProp;

	// Mute the element
	virtual void Mute(
		CAkRegisteredObj *	in_pGameObj,
		AkCurveInterpolation		in_eFadeCurve = AkCurveInterpolation_Linear,
		AkTimeMs		in_lTransitionTime = 0
		);
	
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
		);

	virtual void SetAkProp( 
		AkPropID in_eProp, 
		AkReal32 in_fValue, 
		AkReal32 in_fMin, 
		AkReal32 in_fMax 
		);

	virtual void ParamNotification( NotifParams& in_rParams );

	virtual void MuteNotification(
		AkReal32 in_fMuteLevel,
		AkMutedMapItem& in_rMutedItem,
		bool			in_bIsFromBus = false
		);

	using CAkActiveParent<CAkParameterNodeBase>::MuteNotification; //Removes a warning Vita compiler.

	virtual void NotifyBypass(
		AkUInt32 in_bitsFXBypass,
		AkUInt32 in_uTargetMask,
		CAkRegisteredObj * in_GameObj,
		AkRTPCExceptionChecker* in_pExceptCheck = NULL
		);

	void SetRecoveryTime( AkUInt32 in_RecoveryTime );

	void SetMaxDuckVolume( AkReal32 in_fMaxDuckVolume );

	AKRESULT AddDuck(
		AkUniqueID in_BusID,
		AkVolumeValue in_DuckVolume,
		AkTimeMs in_FadeOutTime,
		AkTimeMs in_FadeInTime,
		AkCurveInterpolation in_eFadeCurve,
		AkPropID in_TargetProp
		);

	AKRESULT RemoveDuck(
		AkUniqueID in_BusID
		);

	AKRESULT RemoveAllDuck();

	void Duck(
		AkUniqueID in_BusID,
		AkVolumeValue in_DuckVolume,
		AkTimeMs in_FadeOutTime,
		AkCurveInterpolation in_eFadeCurve,
		AkPropID in_PropID
		);

	void Unduck(
		AkUniqueID in_BusID,
		AkTimeMs in_FadeInTime,
		AkCurveInterpolation in_eFadeCurve,
		AkPropID in_PropID
		);

	void PauseDuck(
		AkUniqueID in_BusID
		);

	virtual void ApplyMaxNumInstances( 
		AkUInt16 in_u16MaxNumInstance
		);

	virtual AKRESULT IncrementPlayCount( 
		CounterParameters& io_params,
		bool in_bNewInstance,
		bool in_bSwitchingOutputbus = false
		);

	virtual void DecrementPlayCount( 
		CounterParameters& io_params
		);

	virtual bool IncrementActivityCount( AkUInt16 in_flagForwardToBus = AK_ForwardToBusType_ALL );
	virtual void DecrementActivityCount( AkUInt16 in_flagForwardToBus = AK_ForwardToBusType_ALL );

	bool IsOrIsChildOf( CAkParameterNodeBase * in_pNodeToTest );

	void DuckNotif();

	bool HasEffect();
	bool IsMixingBus();

	bool IsTopBus();	

	inline AkUniqueID GetDeviceShareset() 
	{
		if(m_idDeviceShareset == AK_INVALID_DEVICE_ID)
		{
			CAkBus* pBus = this;
			do
			{
				pBus = static_cast<CAkBus*>(pBus->ParentBus());
			} while (pBus && pBus->m_idDeviceShareset == AK_INVALID_DEVICE_ID);

			if (pBus)
			{
				m_idDeviceShareset = pBus->m_idDeviceShareset;
#ifdef WWISE_AUTHORING
				m_idDevice = pBus->m_idDevice;
#endif
			}
		}
		return m_idDeviceShareset;
	}

	static void ExecuteMasterBusAction( ActionParams& in_params );
	static void ExecuteMasterBusActionExcept( ActionParamsExcept& in_params );

	void CheckDuck();

	AkVolumeValue GetDuckedVolume( AkPropID in_eProp );

	void ChannelConfig( AkChannelConfig in_channelConfig );
	inline AkChannelConfig ChannelConfig() { return m_channelConfig; }

	// Gets the Next Mixing Bus associated to this node
	//
	// RETURN - CAkBus* - The next mixing bus pointer.
	//						NULL if no mixing bus found
	virtual CAkBus* GetMixingBus();

	virtual void UpdateFx(
		AkUInt32 in_uFXIndex
		);

	virtual void GetFX(
		AkUInt32	in_uFXIndex,
		AkFXDesc&	out_rFXInfo,
		CAkRegisteredObj *	in_GameObj = NULL
		);

	virtual void GetFXDataID(
		AkUInt32	in_uFXIndex,
		AkUInt32	in_uDataIndex,
		AkUInt32&	out_rDataID
		);

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

	virtual void SetStateProperties(
		AkUInt32 in_uProperties,
		AkStatePropertyUpdate* in_pProperties,
		bool in_bNotify
		);

	virtual AkStateGroupChunk* AddStateGroup(
		AkStateGroupID in_ulStateGroupID,
		bool in_bNotify = true
		);

	virtual void RemoveStateGroup(
		AkStateGroupID in_ulStateGroupID,
		bool in_bNotify = true
		);

	virtual AKRESULT UpdateStateGroups(
		AkUInt32 in_uGroups,
		AkStateGroupUpdate* in_pGroups,
		AkStateUpdate* in_pUpdates
		);

	AkInt16 GetBypassAllFX(CAkRegisteredObj * in_GameObjPtr);

	inline bool HasMixerPlugin() { return ( m_pMixerPlugin && m_pMixerPlugin->plugin.id != AK_INVALID_UNIQUE_ID ); }
	
	void GetMixerPlugin(
		AkFXDesc&	out_rFXInfo
		);

	void GetMixerPluginDataID(
		AkUInt32	in_uDataIndex,
		AkUInt32&	out_rDataID
		);

	// WAL setter for mixer plugin
	virtual AKRESULT SetMixerPlugin( 
		AkUniqueID in_uID,						// Unique id of CAkFxShareSet or CAkFxCustom
		bool in_bShareSet,						// Shared?
		SharedSetOverride in_eSharedSetOverride	// For example, Proxy cannot override FX set by API.
		);

	virtual AKRESULT SetInitialValues(AkUInt8* in_pData, AkUInt32 in_ulDataSize);
	virtual AKRESULT SetInitialFxParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize, bool in_bPartialLoadOnly );

	bool GetStateSyncTypes( AkStateGroupID in_stateGroupID, CAkStateSyncArray* io_pSyncTypes );

	virtual AkUInt16 Children()
	{
		return static_cast<AkUInt16>( m_mapChildId.Length() + m_mapBusChildId.Length() );
	}
	
	virtual void PositioningChangeNotification(
		AkReal32			in_RTPCValue,	// Value
		AkRTPC_ParameterID	in_ParameterID,	// RTPC ParameterID, must be a positioning ID.
		CAkRegisteredObj *		in_GameObj,		// Target Game Object
		AkRTPCExceptionChecker*	in_pExceptCheck = NULL
		)
	{
		PropagatePositioningNotification( in_RTPCValue, in_ParameterID, in_GameObj, in_pExceptCheck );
	}

	// HDR
	//
	inline void SetHdrCompressorDirty()
	{ 
		// Force recomputation of coefficients and clear memory, and re-cache in lower engine.
		m_bHdrGainComputerDirty = true;
		m_bHdrReleaseTimeDirty = true;	
	}	
	
	void SetBusDevice(AkUniqueID in_idDeviceShareset);

	// Searches through the top busses that use the oldSharesetId as their device, and sets them to the newSharesetId
	static void ReplaceTopBusSharesets(AkUniqueID in_oldSharesetId, AkUniqueID in_newSharesetId);

	static AkForceInline CAkBus* GetMainBus()
	{ 
		if (!s_TopBusses.IsEmpty()) 
			return s_TopBusses[0]; 
		return NULL;
	}

	inline AKRESULT SetMasterBus()
	{
		AKRESULT eResult = AK_Success;
		if (!s_TopBusses.Exists(this) && s_TopBusses.AddLast(this) == NULL)
			eResult = AK_InsufficientMemory;
		return eResult;
	}
	
#ifndef AK_OPTIMIZED
	// Set up the global variables for accessing the top of the bus hierarchy. 
	inline void SetMasterBus(AkUniqueID in_idDeviceShareset, AkUInt32 in_idDevice)
	{ 
		SetMasterBus();	//Can ignore error, only in Authoring which has plenty of memory.	
		SetBusDevice(in_idDeviceShareset);
#ifdef WWISE_AUTHORING
		m_idDevice = in_idDevice;
#endif					
	}
#ifdef WWISE_AUTHORING
	inline AkUInt32 GetTestDeviceID() { return m_idDevice; }
#endif
#endif

	inline void SetHdrReleaseMode( bool in_bHdrReleaseModeExponential ) 
	{ 
		m_bHdrReleaseModeExponential = in_bHdrReleaseModeExponential;
		m_bHdrReleaseTimeDirty = true;	// Force recomputation of coefficients and clear memory.
	}

	// Returns true if values were dirty.
	inline bool GetHdrGainComputer(
		AkReal32 & out_fHdrThreshold,
		AkReal32 & out_fRatio
		)
	{
		// Can change because of live edit. Will be honored on next play. AKASSERT( IsHdrBus() );
		GetPropAndRTPCExclusive( out_fHdrThreshold, AkPropID_HDRBusThreshold, AkRTPCKey() );
		GetPropAndRTPCExclusive( out_fRatio, AkPropID_HDRBusRatio, AkRTPCKey() );
		bool bGainComputerDirty = m_bHdrGainComputerDirty;
		m_bHdrGainComputerDirty = false;
		return bGainComputerDirty;
	}

	// Returns true if values were dirty.
	inline bool GetHdrBallistics(
		AkReal32 & out_fReleaseTime,
		bool & out_bReleaseModeExponential		
		)
	{
		GetPropAndRTPCExclusive( out_fReleaseTime, AkPropID_HDRBusReleaseTime, AkRTPCKey() );
		out_bReleaseModeExponential = m_bHdrReleaseModeExponential;
		bool bReleaseTimeDirty = m_bHdrReleaseTimeDirty;
		m_bHdrReleaseTimeDirty = false;
		return bReleaseTimeDirty;
	}

	AkUniqueID GetGameParamID();
	void ClampWindowTop(AkReal32 &in_fWindowTop);

#ifndef AK_OPTIMIZED
	virtual void MonitoringSolo( bool in_bSolo );
	virtual void MonitoringMute( bool in_bMute );

	inline static bool IsMonitoringSoloActive_bus() { return ( g_uSoloCount_bus > 0 ); }
	inline static bool IsMonitoringMuteSoloActive_bus() { return ( g_uSoloCount_bus > 0 || g_uMuteCount_bus > 0 ); }

	virtual void InvalidatePositioning();

#endif

	// --- CAkRTPCSubscriberNode overrides--- //
	
	// CAkRTPCSubscriberNode
	virtual void PushParamUpdate( AkRTPC_ParameterID in_ParamID, const AkRTPCKey& in_rtpcKey, AkReal32 in_fValue, AkReal32 in_fDeltaValue, AkRTPCExceptionChecker* in_pExceptCheck );

	virtual void NotifyParamChanged( bool in_bLiveEdit, AkRTPC_ParameterID in_rtpcID );
	virtual void NotifyParamsChanged( bool in_bLiveEdit, const AkRTPCBitArray& in_bitArray );

	CAkRTPCSubscriberNode& GetDuckerNode() {return m_DuckerNode;}

	virtual void ClearLimiters();

#ifndef AK_OPTIMIZED
	virtual void UpdateRTPC();
	virtual void ResetRTPC();
#endif

protected:


	class ChildrenIterator
	{
	public:
		ChildrenIterator( AkMapChildID& in_First, AkMapChildID& in_Second )
			:m_pCurrentlist( &in_First )
			,m_pSecondlist( &in_Second )
		{
			m_iter = in_First.Begin();
			SwitchIfEnd();
		}

		inline ChildrenIterator& operator++()
		{
			++m_iter;
			SwitchIfEnd();
			return *this;
		}

		inline bool End()
		{
			 return m_iter == m_pCurrentlist->End();
		}

		/// Operator *.
		inline CAkParameterNodeBase* operator*()
		{
			return (*m_iter);
		}

	private:

		inline void SwitchIfEnd()
		{
			// Switch to next list.
			if( m_iter == m_pCurrentlist->End() )
			{
				if( m_pCurrentlist != m_pSecondlist )
				{
					m_pCurrentlist = m_pSecondlist;
					m_iter = m_pCurrentlist->Begin();
				}
			}
		}

		AkMapChildID::Iterator m_iter;
		AkMapChildID* m_pCurrentlist;
		AkMapChildID* m_pSecondlist;
	};

	virtual CAkSIS* GetSIS( CAkRegisteredObj * in_GameObj = NULL );
	virtual void RecalcNotification( bool in_bLiveEdit, bool in_bLog = false );

	// Constructors
    CAkBus(AkUniqueID in_ulID);

	//Destructor
    virtual ~CAkBus();

	void StartDucking();
	void StopDucking();

	virtual AKRESULT SetInitialParams(AkUInt8*& pData, AkUInt32& ulDataSize );

	void StartDuckTransitions(CAkDuckItem*		in_pDuckItem,
								AkReal32		in_fTargetValue,
								AkValueMeaning	in_eValueMeaning,
								AkCurveInterpolation	in_eFadeCurve,
								AkTimeMs		in_lTransitionTime,
								AkPropID		in_ePropID);

	AKRESULT RequestDuckNotif();

	void UpdateDuckedBus();

	struct MixerPluginChunk : public SharedSetOverrideBox
	{
		MixerPluginChunk()
		{
			plugin.id = AK_INVALID_UNIQUE_ID;
			plugin.bShareSet = true;
		}

		FXStruct plugin;
	};
	MixerPluginChunk * m_pMixerPlugin;

	AkMapChildID m_mapBusChildId; // List of nodes connected to this one

	AkUInt32 m_RecoveryTime; // Recovery time in output samples
	AkChannelConfig m_channelConfig;

	AkReal32 m_fMaxDuckVolume;

	typedef CAkKeyList<AkUniqueID, AkDuckInfo, AkAllocAndFree> AkToDuckList;
	AkToDuckList m_ToDuckList;

	typedef CAkKeyList<AkUniqueID, CAkDuckItem, AkAllocAndFree> AkDuckedVolumeList;
	AkDuckedVolumeList m_DuckedVolumeList;
	AkDuckedVolumeList m_DuckedBusVolumeList;

	// Keeps track of parameter targets to update via duckers.
	CAkRTPCSubscriberNode m_DuckerNode;

	AkUniqueID m_idDeviceShareset;	//Device type this bus is outputing into. (TODO remove from s_TopBus)

	AkUInt8/*AkDuckingState*/	m_eDuckingState :DUCKING_STATE_NUM_STORAGE_BIT;
	AkUInt8						m_bHdrReleaseModeExponential	:1;
	AkUInt8						m_bHdrReleaseTimeDirty			:1;
	AkUInt8						m_bHdrGainComputerDirty			:1;	

#ifdef WWISE_AUTHORING
	AkUInt32					m_idDevice;
#endif

public:
	static void MuteBackgroundMusic();
	static void UnmuteBackgroundMusic();
	static bool IsBackgroundMusicMuted();
	
// For Wwise purpose, those two functions are exposed, 
	void SetAsBackgroundMusicBus();
	void UnsetAsBackgroundMusicBus();

private:
	void GetControlBusParams(AkSoundParams &io_Params, const AkRTPCKey& in_rtpcKey);
	void BackgroundMusic_Mute();
	void BackgroundMusic_Unmute();

	AkUInt8 m_bIsBackgroundMusicBus		:1;
	static bool s_bIsBackgroundMusicMuted;	

	static AkBusArray s_BGMBusses;
	static AkBusArray s_TopBusses;
	static bool s_bIsSharesetInitialized;
	static CAkLock m_BackgroundMusicLock;
};

#endif //_BUS_H_
