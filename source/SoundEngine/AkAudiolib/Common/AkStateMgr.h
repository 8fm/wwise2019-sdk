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

//////////////////////////////////////////////////////////////////////
//
// AkStateMgr.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _STATE_MGR_H_
#define _STATE_MGR_H_

#include <AK/Tools/Common/AkKeyArray.h>
#include "AkList2.h"
#include "AkPreparationAware.h"
#include "PrivateStructures.h"
#include <AK/Tools/Common/AkListBare.h>
#include <AK/Tools/Common/AkListBareLight.h>
#include <AK/Tools/Common/AkLock.h>
#include "AkStateTypes.h"
#include "ITransitionable.h"

class CAkState;
class CAkStateAware;
class CAkSwitchAware;
class CAkRegisteredObj;
class CAkTransition;

// none is a reserved name in Wwise and no name can be named none, except the none state.
#define AK_HASH_STATE_NONE 748895195 // This is the hash of "none" by GetIDFromString( "none" )


//Struct defining a transition between two states
struct AkStateTransition
{
	AkStateID		StateFrom;
	AkStateID		StateTo;
	bool operator<(const AkStateTransition& in_sCompareTo) const
	{
		if(StateFrom == in_sCompareTo.StateFrom)
		{
			return StateTo < in_sCompareTo.StateTo;
		}
		return StateFrom < in_sCompareTo.StateFrom;
	}
	bool operator==(const AkStateTransition& in_sCompareTo) const
	{
		return (StateFrom == in_sCompareTo.StateFrom && StateTo == in_sCompareTo.StateTo);
	}
};

struct AkStateLink
{
	class CAkState*	pState;
};

struct AkStateValue
{
	AkReal32 fValue;
	CAkTransition* pTransition;
};

typedef AkPropBundle<AkStateValue, AkStatePropertyId> AkStateValues;
typedef CAkKeyArray<AkStateID, AkStateLink> AkMapStates;

class AkStateGroupChunk AK_FINAL : public ITransitionable
{
public:
	AkStateGroupChunk * m_pNextInGroup; // in AkStateGroupInfo::members
	AkStateGroupChunk * m_pNextInNode; // in AkParameterNodeBase::states
	CAkStateAware * m_pOwner;
	AkStateValues m_values;

	AkMapStates		m_mapStates;	// Map of States available for this sound

	const AkStateGroupID	m_ulStateGroup;
	AkStateID		m_ulActualState;  // Actual state of the ParameterNode

	AkUInt8			m_eStateSyncType; // AkSyncType

	inline AkStateGroupChunk( CAkStateAware* in_pOwner, AkStateGroupID in_ulStateGroup )
		:m_pNextInGroup( NULL )
		,m_pNextInNode( NULL )
		,m_pOwner( in_pOwner )
		,m_ulStateGroup( in_ulStateGroup )
		,m_ulActualState( AK_HASH_STATE_NONE )
		,m_eStateSyncType( SyncTypeImmediate )
	{
	}

	~AkStateGroupChunk();	

	inline void SetOwner( CAkStateAware* in_pOwner ) { m_pOwner = in_pOwner; }

	inline void SetActualState(  AkStateID in_ulStateID ) { m_ulActualState = in_ulStateID; }

	inline CAkState* GetState() { return GetState( m_ulActualState ); }
	CAkState* GetState( AkStateID in_StateTypeID );

	AKRESULT AddState(
		AkUniqueID in_ulStateInstanceID,
		AkStateID in_ulStateID,
		bool in_bNotify = true
		);

	void RemoveState(
		AkStateID in_ulStateID
		);

	void FlushStateTransitions();

	virtual void TransUpdateValue(
		AkIntPtr in_eTargetType,
		AkReal32 in_fValue,
		bool in_bIsTerminated
		);
};

template <class T>
struct AkListBareLightNextInGroup
{
	/// Default policy.
	static AkForceInline T *& Get( T * in_pItem ) 
	{
		return in_pItem->m_pNextInGroup;
	}
};

template <class T>
struct AkListBareLightNextInNode
{
	/// Default policy.
	static AkForceInline T *& Get( T * in_pItem ) 
	{
		return in_pItem->m_pNextInNode;
	}
};

//Struct defining all the info associated to a channel
class AkStateGroupInfo
{
public:
	AkTimeMs		lDefaultTransitionTime;
	AkStateID		ActualState;

	typedef AkListBareLight<AkStateGroupChunk, AkListBareLightNextInGroup> MemberList;
	MemberList members;

	CAkKeyArray<AkStateTransition, AkTimeMs> mapTransitions;

	AkStateGroupInfo():lDefaultTransitionTime(0),ActualState(AK_HASH_STATE_NONE){}//struct constructor

	void Term();
};

class IAkTriggerAware
{
public:
	virtual void Trigger( AkTriggerID in_triggerID ) = 0;
};

//Class representing the state manager
class CAkStateMgr
{	
	friend class AkMonitor;// for profiling purpose.

public:

	class PreparationStateItem
	{
		friend class AkMonitor;// for profiling purpose.
	public:
		PreparationStateItem * pNextItem; // For CAkURenderer::m_listCtxs

	public:
		PreparationStateItem( AkUInt32 in_GroupID )
			:pNextItem(NULL)
			,m_GroupID( in_GroupID )
		{
		}

		~PreparationStateItem()
		{
			m_PreparationList.Term();
		}
		
		void Add( CAkPreparationAware* in_pToBeAdded )
		{
			AKASSERT( m_PreparationList.FindEx( in_pToBeAdded ) == m_PreparationList.End() );
			m_PreparationList.AddFirst( in_pToBeAdded );
		}

		void Remove( CAkPreparationAware* in_pToBeRemoved )
		{
			m_PreparationList.Remove( in_pToBeRemoved );
		}

		CAkPreparedContent* GetPreparedcontent()
		{
			return &m_PreparedContent;
		}

		AkUInt32 GroupID(){ return m_GroupID; }

		AKRESULT Notify( AkUInt32 in_GameSyncID, bool in_bIsActive );

		// U_NEXTITEM Policy for PreparationList. Needs to be public.
		template <class T>
		struct AkListBareNextItemPrepare
		{
			static AkForceInline T *& Get( T * in_pItem ) 
			{
				return in_pItem->pNextItemPrepare;
			}
		};
	private:

		CAkPreparedContent m_PreparedContent;

		AkUInt32 m_GroupID;

		// List of people to be notified when changes occur
		typedef AkListBare<CAkPreparationAware, AkListBareNextItemPrepare> PreparationList;
		PreparationList m_PreparationList;
	};

public:
	CAkStateMgr();
	~CAkStateMgr();

	AKRESULT Init();

	void Term();

	// Add a StateGroup ID in the list
	AKRESULT AddStateGroup(
		AkStateGroupID in_ulStateGroupID	//StateGroup ID
		);

	// Add a member to the channel
	// an element having state must be associated to a channel to have it working
	AKRESULT AddStateGroupMember(
		AkStateGroupID in_ulStateGroupID,	//StateGroup ID
		AkStateGroupChunk* in_pMember//Audionode to be referenced as a member
		);
#ifndef AK_OPTIMIZED
	// Remove a StateGroup ID in the list
	AKRESULT RemoveStateGroup(
		AkStateGroupID in_ulStateGroupID	//StateGroup ID
		);
#endif

	// Remove a member from the specified channel
	AKRESULT RemoveStateGroupMember(
		AkStateGroupID in_ulStateGroupID,	//StateGroup ID
		AkStateGroupChunk* in_pMember//Audionode to be removed from the member list
		);

	// Add a state transition to the specified state group
	AKRESULT AddStateTransition(
		AkStateGroupID in_ulStateGroupID,	//StateGroup ID
		AkStateID in_ulStateID1,		//State type "FROM"
		AkStateID in_ulStateID2,		//State type "TO"
		AkTimeMs lTransitionTime,		//Transition time in ms
		bool in_bIsShared = false	//Is the transition valid in both ways
		);

	// Remove a state transition from the specified state group
	AKRESULT RemoveStateTransition(
		AkStateGroupID in_ulStateGroupID,	//StateGroup ID
		AkStateID in_ulStateID1,		//State type "FROM"
		AkStateID in_ulStateID2,		//State type "TO"
		bool in_bIsShared = false	//Is the transition to be removed in both ways
		);

	// Clear all the specific transitions of a given channel transition from the specified state group
	AKRESULT ClearStateTransition(
		AkStateGroupID in_ulStateGroupID	//StateGroup ID
		);

	// Set the default transition time of the specified state group
	AKRESULT SetdefaultTransitionTime(
		AkStateGroupID in_ulStateGroupID,	//StateGroup ID
		AkTimeMs lTransitionTime		//Transition time in ms
		);

	// Get the state of the specified state group
	//
	// Return - AkStateID - ID of the ActualState
	AkStateID GetState(
		AkStateGroupID in_ulStateGroupID
	);

	// Get the state of the specified state group
	//
	// Return - whether the state group exists
	//        - out_ulStateID - ID of ActualState
	bool GetState(
		AkStateGroupID in_ulStateGroupID,
		AkStateID& out_ulStateID
		);

	// Reset all states to none and stop all actual states transitions
#ifndef AK_OPTIMIZED
	AKRESULT ResetAllStates();
#endif

	AKRESULT RemoveAllStateGroups( bool in_bIsFromClearBanks );

	AKRESULT RegisterSwitch( CAkSwitchAware* in_pSwitchCntr, AkStateGroupID in_ulStateGroup);

	AKRESULT UnregisterSwitch( CAkSwitchAware* in_pSwitchCntr );

	AKRESULT RegisterTrigger( IAkTriggerAware* in_pTrigerAware, AkTriggerID in_triggerID, CAkRegisteredObj* in_GameObj );
    AKRESULT RegisterTrigger( IAkTriggerAware* in_pTrigerAware, CAkRegisteredObj* in_GameObj );

	AKRESULT UnregisterTrigger( IAkTriggerAware* in_pTrigerAware, AkTriggerID in_Trigger );
    AKRESULT UnregisterTrigger( IAkTriggerAware* in_pTrigerAware );

	void Trigger( AkTriggerID in_Trigger, CAkRegisteredObj* in_GameObj );

	// Set the state of the specified state group
	void SetStateInternal(
		AkStateGroupID in_ulStateGroupID,	//StateGroup ID
		AkStateID in_ulStateID,				//State type_ID
		bool in_bSkipTransitionTime = false,
        bool in_bSkipExtension = false      // true: skips call to state handler extension.
		);

	
	// Add / remove functionnality to add entries in m_PreparationGroups.
	// Do not forget to allocatee and free the items in those list.
	// This is a grow only list.
	PreparationStateItem* GetPreparationItem( AkUInt32 in_ulGroup, AkGroupType in_eGroupType );

	AKRESULT PrepareGameSync( 
		AkGroupType in_eGroupType, 
		AkUInt32 in_uGroup, 
		AkUInt32 in_uGameSyncID, 
		bool in_bIsActive 
		);

private:

	AKRESULT UpdateSwitches( AkStateGroupID in_ulStateGroup, AkStateID in_StateFrom, AkStateID in_StateTo );

	struct RegisteredSwitch
	{
		CAkSwitchAware* pSwitch;
		AkStateGroupID  ulStateGroup;
	};

	struct RegisteredTrigger
	{
		IAkTriggerAware*	pTriggerAware;
		AkTriggerID			triggerID;
		CAkRegisteredObj*	gameObj;
	};

	typedef CAkKeyArray<AkStateGroupID, AkStateGroupInfo*> AkListStateGroups;
	AkListStateGroups m_StateGroups;//List of channels actually available

	typedef CAkList2< RegisteredSwitch, const RegisteredSwitch&, AkAllocAndFree> AkListRegisteredSwitch;
	AkListRegisteredSwitch m_listRegisteredSwitch;
	bool m_bNotifyingSwitches;
	bool m_bUnregisteredSwitches;

	typedef CAkList2< RegisteredTrigger, const RegisteredTrigger&, AkAllocAndFree> AkListRegisteredTrigger;
	AkListRegisteredTrigger m_listRegisteredTrigger;

	typedef AkListBare<PreparationStateItem> PreparationGroups;
	void TermPreparationGroup( PreparationGroups& in_rGroup );

	CAkLock m_PrepareGameSyncLock;

	PreparationGroups m_PreparationGroupsStates;
	PreparationGroups m_PreparationGroupsSwitches;
};

extern CAkStateMgr* g_pStateMgr;

#endif
