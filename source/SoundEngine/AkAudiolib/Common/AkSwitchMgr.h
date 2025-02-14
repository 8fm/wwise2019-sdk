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
// AkSwitchMgr.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _SWITCH_MGR_H_
#define _SWITCH_MGR_H_

#include "AkRTPC.h"
#include "AkRTPCKey.h"
#include <AK/Tools/Common/AkHashList.h>
#include "AkRegisteredObj.h"
#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkKeyArray.h>
#include "AkMonitor.h"

class CAkSwitchAware;
class AkRTPCExceptionChecker;
class CAkModulatorData;


struct AkSwitchSubscriptionKey
{
	CAkSwitchAware*		pSubscriber;	// Cast to appropriate interface/class depending on eType

	AkSwitchSubscriptionKey( CAkSwitchAware* in_pSubscriber ) : pSubscriber( in_pSubscriber ) {}
	AkSwitchSubscriptionKey() {}

	bool operator == ( const AkSwitchSubscriptionKey & in_other ) const
	{
		return pSubscriber == in_other.pSubscriber ;
	}
};

inline AkHashType AkHash(AkSwitchSubscriptionKey in_key) { return (AkHashType)(AkUIntPtr)in_key.pSubscriber; }


// CAkRTPCMgr Class
// Unique per instance of Audiolib, the RTPC manager owns the real 
// time parameters and notified those who wants to be
class CAkSwitchMgr
{	
public:

	friend class AkMonitor;

	CAkSwitchMgr();
	~CAkSwitchMgr();

	AKRESULT Init();
	AKRESULT Term();

	AKRESULT SubscribeSwitch(
		CAkSwitchAware*		in_pSubscriber,
		AkSwitchGroupID		in_switchGroup
		);

	void UnSubscribeSwitch( CAkSwitchAware* in_pSubscriber );

	void ResetSwitches( CAkRegisteredObj* in_pGameObj = NULL );

	AKRESULT AddSwitchRTPC(
		AkSwitchGroupID		in_switchGroup, 
		AkRtpcID			in_rtpcID,
		AkRtpcType			in_rtpcType,
		AkSwitchGraphPoint*	in_pGraphPts, //NULL if none
		AkUInt32			in_numGraphPts //0 if none
		);

	void RemoveSwitchRTPC( AkSwitchGroupID in_switchGroup );

	inline AKRESULT SetSwitchInternal( AkSwitchGroupID in_switchGroup, AkSwitchStateID in_switchState, CAkRegisteredObj* in_pGameObj );
	AkSwitchStateID GetSwitch( AkSwitchGroupID in_switchGroup, const AkRTPCKey& in_rtpcKey );
	bool GetSwitch( AkSwitchGroupID in_switchGroup, const AkRTPCKey& in_rtpcKey, AkSwitchStateID& out_switchID );

	void UnregisterGameObject( CAkRegisteredObj* in_pGameObj );

	inline void SetSwitchFromRTPCMgr( void* in_pSubscriber, const AkRTPCKey& in_rtpcKey, AkReal32 in_rtpcValue, AkRTPCExceptionChecker* in_pExCheck );
	void TriggerModulator( const CAkSwitchAware* in_pSubscriber, const struct AkModulatorTriggerParams& in_params, CAkModulatorData* out_pModPBIData );

private:

	AKRESULT _SubscribeSwitch(
		CAkSwitchAware*		in_pSubscriber,
		AkSwitchGroupID		in_switchGroup
		);

	void _UnSubscribeSwitch( CAkSwitchAware* in_pSubscriber );

	struct AkSwitchSubscription
	{
		// Make sure that the subscriber is actually still subscribed before accessing.
		CAkSwitchAware* GetSubscriber() {return bSubscribed ? key.pSubscriber : NULL;}

		AkSwitchSubscription(): bSubscribed(true) {}
		~AkSwitchSubscription() {}

		AkSwitchSubscriptionKey key;
		AkSwitchSubscription* pNextItem;
		AkSwitchGroupID switchGroup;
		bool bSubscribed; // <- IMPORTANT: Check this variable before dereferencing the key.pSubscriber!  For convenience use GetSubscriber() above.
	};

	struct AkSubsPtrGetKey
	{
		/// Default policy.
		static AkForceInline AkSwitchSubscription*& Get( AkSwitchSubscription*& in_item ) 
		{
			return in_item;
		}
	};

	typedef AkSortedKeyArray<AkSwitchSubscription*, AkSwitchSubscription*, ArrayPoolDefault, AkSubsPtrGetKey> AkSubscriptionArray;

	class AkSwitchEntry
	{
		friend class AkMonitor;
	public:
		AkSwitchEntry( AkSwitchGroupID in_switchGroup )
			: key( in_switchGroup )
			, rtpcID( AK_INVALID_RTPC_ID )
			, rtpcType( AkRtpcType_GameParameter )
		{}
		~AkSwitchEntry();

	public:
		// For AkHashListBare
		AkSwitchGroupID key;
		AkSwitchEntry* pNextItem;

	public:
		AKRESULT AddRTPC( AkRtpcID in_rtpcID, AkRtpcType in_rtpcType, AkSwitchGraphPoint* in_pGraphPts, AkUInt32 in_numGraphPts );
		void RemoveRTPC();
		void RemoveGameObject( CAkRegisteredObj* in_pGameObj );
		void RemoveValues() { values.RemoveAll( AkRTPCKey() ); }
		bool SetSwitchInternal( AkSwitchStateID in_switchState, CAkRegisteredObj* in_pGameObj );
		void SetSwitchFromRTPCMgr( const AkRTPCKey& in_rtpcKey, AkUInt32 in_rtpcValue, AkRTPCExceptionChecker* in_pExCheck );
		AkSwitchStateID GetSwitch( const AkRTPCKey& in_rtpcKey );
		bool IsObsolete() { return subscriptions.IsEmpty() && values.IsEmpty() && rtpcID == AK_INVALID_RTPC_ID; }
		bool HasRTPC() const { return rtpcID != AK_INVALID_RTPC_ID; }
		bool HasModulator() const { return rtpcID != AK_INVALID_RTPC_ID && rtpcType == AkRtpcType_Modulator; }

	private:
		typedef AkArray< AkSwitchStateID, AkSwitchStateID&, AkArrayAllocatorNoAlign<AkMemID_Structure> > AkSwitchStateArray;

		// Used for mapped RTPC (if any)
		AkSwitchStateArray	states;
		AkRtpcID			rtpcID;
		AkRtpcType			rtpcType;

	public:
		AkSubscriptionArray subscriptions;
	private:
		AkRTPCValueTree values;
	};

	AkSwitchEntry* GetSwitchEntry( AkSwitchGroupID in_switchGroup );
	void RemoveSubscriptionFromEntry( AkSwitchSubscription* in_pSubscription );

	void ExecuteSubsActions();
	bool IsSwitching() { return m_iSwitching > 0; }

	typedef AkHashListBare< AkSwitchGroupID, AkSwitchEntry > AkMapSwitchEntries;
	AkMapSwitchEntries m_mapEntries;

	typedef AkHashListBare< AkSwitchSubscriptionKey, AkSwitchSubscription > AkMapSwitchSubscriptions;
	AkMapSwitchSubscriptions m_mapSubscriptions;

	// A struct for keeping track of delayed reg/unregistration ations that need to be executed
	//	if Register()/Unregister() is called while switch changing is taking place.
	struct SubsAction
	{ 
		enum AddOrRemove { ADD, REMOVE };

		SubsAction(): addOrRemove(ADD), pSubscriber(NULL), switchGroup(0) {}
		SubsAction(	AddOrRemove in_addRemove, 
					CAkSwitchAware* in_pSubscriber, 
					AkSwitchGroupID in_switchGroup ): 
			addOrRemove(in_addRemove), 
			pSubscriber(in_pSubscriber), 
			switchGroup(in_switchGroup) {}

		AddOrRemove			addOrRemove;
		CAkSwitchAware*		pSubscriber;
		AkSwitchGroupID		switchGroup;
	};
	typedef AkArray< SubsAction, const SubsAction&, AkHybridAllocator< sizeof( SubsAction ) * 16 > > SubsActionArray;
	
	SubsActionArray m_delayedSubsActions;

	//A value > 0 indicates that a switch change is taking place.  
	//	To read, access through IsSwitching().  To write, access through SwitchingInThisScope struct.
	AkInt32 m_iSwitching;

	// Scoped class for incrementing/decrementing m_iSwitching.
	struct SwitchingInThisScope
	{
		SwitchingInThisScope();
		~SwitchingInThisScope();
	};
	friend struct SwitchingInThisScope;
};



void CAkSwitchMgr::SetSwitchFromRTPCMgr( void* in_pSubscriber, const AkRTPCKey& in_rtpcKey, AkReal32 in_rtpcValue, AkRTPCExceptionChecker* in_pExCheck )
{
	SwitchingInThisScope s;

	if( in_pSubscriber )
	{
		AkSwitchEntry* pEntry = reinterpret_cast<AkSwitchEntry*>( in_pSubscriber );
		pEntry->SetSwitchFromRTPCMgr( in_rtpcKey, (AkUInt32)in_rtpcValue, in_pExCheck );
	}
}

AKRESULT CAkSwitchMgr::SetSwitchInternal( AkSwitchGroupID in_switchGroup, AkSwitchStateID in_switchState, CAkRegisteredObj* in_pGameObj )
{
	SwitchingInThisScope s;
	AKRESULT eResult = AK_InsufficientMemory;

	MONITOR_SWITCHCHANGED( in_switchGroup, in_switchState,
		in_pGameObj ? in_pGameObj->ID() : AK_INVALID_GAME_OBJECT );

	AkSwitchEntry* pEntry = GetSwitchEntry( in_switchGroup );
	if( pEntry )
	{
		if( pEntry->SetSwitchInternal( in_switchState, in_pGameObj ) )
			eResult = AK_Success;
	}

	return eResult;
}


extern AKSOUNDENGINE_API CAkSwitchMgr* g_pSwitchMgr;



#endif //_SWITCH_MGR_H_
