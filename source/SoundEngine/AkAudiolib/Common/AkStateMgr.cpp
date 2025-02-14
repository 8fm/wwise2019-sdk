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
// AkStateMgr.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkStateMgr.h"
#include "AkState.h"
#include "AkAudioLib.h"
#include "AkTransitionManager.h"
#include "AkParameterNodeBase.h"
#include "AkSwitchAware.h"
#include <AK/Tools/Common/AkFNVHash.h>

#define AK_REGISTERED_LIST_SIZE		8

extern AkExternalStateHandlerCallback g_pExternalStateHandler;

void AkStateGroupInfo::Term()
{
	mapTransitions.Term();
	members.Term();
}

AkStateGroupChunk::~AkStateGroupChunk()
{
	for (AkStateValues::Iterator it = m_values.Begin(), itEnd = m_values.End(); it != itEnd; ++it)
	{
		AKASSERT(it.pValue->pTransition == NULL);	//There should be NO transitions left over at this point!

		//But if there is, cleanup.  (This is a patch to avoid crashing later on, but the assert scenario should be investigated further)
		if (it.pValue->pTransition)
		{
			g_pTransitionManager->RemoveTransitionUser(it.pValue->pTransition, this);
			it.pValue->pTransition = NULL;
			m_pOwner->StateTransitionRemoved();
		}
	}
	m_mapStates.Term();
}

AKRESULT CAkStateMgr::PreparationStateItem::Notify( AkUInt32 in_GameSyncID, bool in_bIsActive )
{
	AKRESULT eResult = AK_Success;
	for( PreparationList::Iterator iter = m_PreparationList.Begin(); iter != m_PreparationList.End(); ++iter )
	{
		eResult = iter.pItem->ModifyActiveState( in_GameSyncID, in_bIsActive );
		if( eResult != AK_Success )
		{
			if( in_bIsActive )// do not remove the if, even if there is an assert after.
			{
				// Must cancel what has been done up to now.
				for( PreparationList::Iterator iterFlush = m_PreparationList.Begin(); iter != iterFlush; ++iterFlush )
				{
					iterFlush.pItem->ModifyActiveState( in_GameSyncID, false );
				}
			}
			else
			{
				AKASSERT( !"Automatic data preparation system :: Unhandled situation" );
			}
			break;
		}
	}
	return eResult;
}

CAkStateMgr::CAkStateMgr()
	: m_bNotifyingSwitches( false )
	, m_bUnregisteredSwitches( false )
{
}

CAkStateMgr::~CAkStateMgr()
{
}

#if defined(AK_ENABLE_ASSERTS)
static inline AkUInt32 HashName( const char * in_pString, size_t in_strlen )
{
	AK::FNVHash32 MainHash;
	return MainHash.Compute( (const unsigned char *) in_pString, (unsigned int)in_strlen );
}
#endif

AKRESULT CAkStateMgr::Init()
{
	AKRESULT eResult = AK_Success;

	// Be absolutely sure that the hash value of "None" is as expected!
	AKASSERT( HashName( "none", 4 ) == AK_HASH_STATE_NONE );

	eResult = m_listRegisteredSwitch.Reserve( AK_REGISTERED_LIST_SIZE );
	if( eResult == AK_Success )
	{
		eResult = m_listRegisteredTrigger.Reserve( AK_REGISTERED_LIST_SIZE );
	}
	return eResult;
}

void CAkStateMgr::Term()
{
	RemoveAllStateGroups( false );

	m_StateGroups.Term();
	m_listRegisteredSwitch.Term();
	m_listRegisteredTrigger.Term();

	TermPreparationGroup( m_PreparationGroupsStates );
	TermPreparationGroup( m_PreparationGroupsSwitches );
}

void CAkStateMgr::TermPreparationGroup( PreparationGroups& in_rGroup )
{
	PreparationGroups::IteratorEx iter = in_rGroup.BeginEx();
	while( iter != in_rGroup.End() )
	{
		PreparationStateItem* pItem = iter.pItem;
		iter = in_rGroup.Erase( iter );
		AkDelete(AkMemID_Structure, pItem);
	}
	in_rGroup.Term();
}

AKRESULT CAkStateMgr::AddStateGroup(AkStateGroupID in_ulStateGroupID)
{
	AKRESULT eResult = AK_Success;

	AKASSERT(in_ulStateGroupID);//zero is an invalid ID

	AkStateGroupInfo** l_ppStateGrpInfo = m_StateGroups.Exists(in_ulStateGroupID);
	if( l_ppStateGrpInfo )
	{
		return AK_Success;
	}
	AkStateGroupInfo* l_pStateGroupInfo = AkNew( AkMemID_Structure, AkStateGroupInfo );
	if( !l_pStateGroupInfo )
	{
		return AK_Fail;
	}

	if( !m_StateGroups.Set( in_ulStateGroupID, l_pStateGroupInfo ) )
	{
		eResult = AK_Fail;
	}
	if( eResult != AK_Success )
	{
		l_pStateGroupInfo->Term();
		AkDelete(AkMemID_Structure, l_pStateGroupInfo );
	}

	return eResult;
}

AKRESULT CAkStateMgr::AddStateGroupMember(AkStateGroupID in_ulStateGroupID, AkStateGroupChunk* in_pMember)
{
	AKASSERT(in_pMember);
	AkStateGroupInfo** l_ppStateGrpInfo = m_StateGroups.Exists(in_ulStateGroupID);
	if( l_ppStateGrpInfo )
	{
		(*l_ppStateGrpInfo)->members.AddFirst( in_pMember );
		return AK_Success;
	}
	else
	{
		return AK_InvalidStateGroup;
	}
}

AKRESULT CAkStateMgr::SetdefaultTransitionTime(AkStateGroupID in_ulStateGroupID,AkTimeMs lTransitionTime)
{
	AkStateGroupInfo** l_ppStateGrpInfo = m_StateGroups.Exists(in_ulStateGroupID);
	if( l_ppStateGrpInfo )
	{
		(*l_ppStateGrpInfo)->lDefaultTransitionTime = lTransitionTime;
		return AK_Success;
	}
	else
	{
		return AK_InvalidStateGroup;
	}
}

#ifndef AK_OPTIMIZED
AKRESULT CAkStateMgr::RemoveStateGroup(AkStateGroupID in_ulStateGroupID)
{
	AkStateGroupInfo** l_ppStateGrpInfo = m_StateGroups.Exists(in_ulStateGroupID);
	if( l_ppStateGrpInfo )
	{
        AkStateGroupInfo* pStateGroupInfo = *l_ppStateGrpInfo;

        // Set the actual state of this channel's member to None.
		while( !pStateGroupInfo->members.IsEmpty() )
	    {
			AkStateGroupChunk* pMember = pStateGroupInfo->members.First();
			pMember->m_pOwner->RemoveStateGroup( in_ulStateGroupID );	
	    }

		pStateGroupInfo->Term();
		AkDelete(AkMemID_Structure, pStateGroupInfo );
		m_StateGroups.Unset( in_ulStateGroupID );

		return AK_Success;
	}
	else
	{
		return AK_InvalidStateGroup;
	}
}
#endif

AKRESULT CAkStateMgr::RemoveStateGroupMember(AkStateGroupID in_ulStateGroupID, AkStateGroupChunk* in_pMember)
{
	AKASSERT(in_pMember);

	AkStateGroupInfo** l_ppStateGrpInfo = m_StateGroups.Exists( in_ulStateGroupID );
	if( l_ppStateGrpInfo )
	{
		(*l_ppStateGrpInfo)->members.Remove( in_pMember );
		return AK_Success;
	}
	else
	{
		return AK_InvalidStateGroup;
	}
}

AKRESULT CAkStateMgr::AddStateTransition(AkStateGroupID in_ulStateGroupID, AkStateID in_ulStateID1, AkStateID in_ulStateID2, AkTimeMs lTransitionTime,bool in_bIsShared/*= false*/)
{
	AKASSERT(in_ulStateGroupID);
	AkStateGroupInfo** l_ppStateGrpInfo = m_StateGroups.Exists( in_ulStateGroupID );
	if( l_ppStateGrpInfo )
	{
		AkStateTransition localTransition;
		localTransition.StateFrom = in_ulStateID1;
		localTransition.StateTo = in_ulStateID2;
		if( !(*l_ppStateGrpInfo)->mapTransitions.Set( localTransition, lTransitionTime ) )
		{
			return AK_Fail;
		}
		if(in_bIsShared)
		{
			localTransition.StateFrom = in_ulStateID2;
			localTransition.StateTo = in_ulStateID1;
			if( !(*l_ppStateGrpInfo)->mapTransitions.Set( localTransition, lTransitionTime ) )
			{
				return AK_Fail;
			}
		}
		return AK_Success;
	}
	else
	{
		return AK_InvalidStateGroup;
	}
}

AKRESULT CAkStateMgr::RemoveStateTransition(AkStateGroupID in_ulStateGroupID, AkStateID in_ulStateID1, AkStateID in_ulStateID2, bool in_bIsShared/*= false*/)
{
	AKASSERT(in_ulStateGroupID);

	AkStateGroupInfo** l_ppStateGrpInfo = m_StateGroups.Exists( in_ulStateGroupID );
	if( l_ppStateGrpInfo )
	{
		AkStateTransition localTransition;
		localTransition.StateFrom = in_ulStateID1;
		localTransition.StateTo = in_ulStateID2;
		(*l_ppStateGrpInfo)->mapTransitions.Unset( localTransition );
		if(in_bIsShared)
		{
			localTransition.StateFrom = in_ulStateID2;
			localTransition.StateTo = in_ulStateID1;
			(*l_ppStateGrpInfo)->mapTransitions.Unset( localTransition );
		}
		return AK_Success;
	}
	else
	{
		return AK_InvalidStateGroup;
	}
}

AKRESULT CAkStateMgr::ClearStateTransition(AkStateGroupID in_ulStateGroupID)
{
	AKASSERT(in_ulStateGroupID);
	AkStateGroupInfo** l_ppStateGrpInfo = m_StateGroups.Exists( in_ulStateGroupID );
	if( l_ppStateGrpInfo )
	{
		(*l_ppStateGrpInfo)->mapTransitions.RemoveAll();
		return AK_Success;
	}
	else
	{
		return AK_InvalidStateGroup;
	}
}

void CAkStateMgr::SetStateInternal(AkStateGroupID in_ulStateGroupID, AkStateID in_ulStateID, bool in_bSkipTransitionTime /*= false */, bool in_bSkipExtension /*= false */ )
{
	AkStateGroupInfo*		pStateGroupInfo = NULL;
	
	AKASSERT(g_pTransitionManager);
	AkStateGroupInfo** l_ppStateGrpInfo = m_StateGroups.Exists( in_ulStateGroupID );
	if( !l_ppStateGrpInfo )
	{
		MONITOR_ERROR( AK::Monitor::ErrorCode_InvalidGroupID);
		return;
	}

	pStateGroupInfo = *l_ppStateGrpInfo;
	AKASSERT(pStateGroupInfo);
	
	if ( !in_bSkipExtension )
		UpdateSwitches( in_ulStateGroupID, pStateGroupInfo->ActualState, in_ulStateID );

	// in_bSkipTransitionTime means "force state change Now". Thus we don't call the extension handler.
    if ( !in_bSkipExtension && g_pExternalStateHandler && !in_bSkipTransitionTime )
    {
        if ( g_pExternalStateHandler( in_ulStateGroupID, in_ulStateID ) )
        {
            // External State Handler returned true, therefore it processed the state change. Leave now.
            return;
        }
    }
    
	// let's see if we have anything defined for this transition
	AkStateTransition ThisTransition;
	ThisTransition.StateFrom = pStateGroupInfo->ActualState;
	ThisTransition.StateTo = in_ulStateID;

	AkTimeMs transTime = 0;
	if(!in_bSkipTransitionTime)
	{
		if (ThisTransition.StateFrom == ThisTransition.StateTo)
			return; // in_bSkipTransitionTime was not set and we are requested to stay on same state, no need to do anything.

		AkTimeMs* pTime = pStateGroupInfo->mapTransitions.Exists(ThisTransition);
		if( pTime )
		{
			transTime = *pTime;
		}
		else
		{
			transTime = pStateGroupInfo->lDefaultTransitionTime;
		}
	}

	// go through the members list and start transitions when needed
	for( AkStateGroupInfo::MemberList::Iterator iter = pStateGroupInfo->members.Begin(); iter != pStateGroupInfo->members.End(); ++iter )
	{
		AkStateGroupChunk* pStateGroupChunk = *iter;

		CAkStateAware* pOwner = pStateGroupChunk->m_pOwner;

		// set new state
		pStateGroupChunk->SetActualState( in_ulStateID );

		CAkState* pNextState = pStateGroupChunk->GetState( in_ulStateID );
		const AkStatePropBundle propsEmpty;
		const AkStatePropBundle & propsNext = pNextState ? pNextState->Props() : propsEmpty;

		// Iterate through pOwner's vector of managed properties
		const StatePropertyArray* pStateProps = pOwner->GetStateProperties();
		if ( pStateProps )
		{
			for ( AkUInt32 i = 0; i < pStateProps->Length(); ++i )
			{
				AkStatePropertyId iProp = (*pStateProps)[i].propertyId;
				AkRtpcAccum rtpcAccum = (AkRtpcAccum)(*pStateProps)[i].accumType;
				bool inDb = (*pStateProps)[i].inDb != 0;

				AkIntPtr eTarget = (*pStateProps)[i].ToUInt();
				AkTimeMs propTime = ( rtpcAccum == AkRtpcAccum_Boolean ) ? 0 : transTime;

				AkStateValue * pChunkValue = pStateGroupChunk->m_values.FindProp( iProp );

				AkReal32 fDefault = ( rtpcAccum == AkRtpcAccum_Multiply ) ? 1.f : 0.f;
				AkReal32 fNext = propsNext.GetAkProp( iProp, fDefault );
				if ( pChunkValue && pChunkValue->pTransition )
				{
					g_pTransitionManager->ChangeParameter(
						pChunkValue->pTransition,
						eTarget,
						fNext,
						propTime,
						AkCurveInterpolation_Linear,
						AkValueMeaning_Default );
				}
				else
				{
					AkReal32 fPrev = pChunkValue ? pChunkValue->fValue : fDefault;
					if ( fPrev != fNext )
					{
						if ( !pChunkValue )
						{
							pChunkValue = pStateGroupChunk->m_values.AddAkProp( iProp );
							if( pChunkValue )
							{
								// MUST be initialized before calling AddTransitionToList
								// WG-25244
								// The callback using pChunkValue can be called while calling AddTransitionToList.
								// TODO_STATEPROPS: Assuming float
								pChunkValue->fValue = 0.0f;
								pChunkValue->pTransition = NULL;
							}
						}

						if ( propTime == 0.f )
						{
							// No need to create a transition, simply update the value immediately
							AkDeltaMonitorUpdateBrace updateBrace(AkDelta_State);
							pStateGroupChunk->TransUpdateValue( eTarget, fNext, true );
						}
						else
						{
							// initialise transition parameters
							TransitionParameters trans(
								pStateGroupChunk,
								eTarget,
								fPrev,
								fNext,
								propTime,
								AkCurveInterpolation_Linear, // fade curve is assumed to be linear for all
								AkDelta_State,	//State transitions are logged when they are applied.
								inDb,
								true );

							CAkTransition* pTransition = g_pTransitionManager->AddTransitionToList( trans, true, CAkTransitionManager::TC_State );
					
							if ( pChunkValue )
							{
								pChunkValue->pTransition = pTransition;
								if ( pTransition )
								{
									if ( !pOwner->StateTransitionAdded() )
									{
										AkDeltaMonitorUpdateBrace updateBrace( trans.eChangeReason );
										// Could not properly handle the transition
										// Skip Transition, simulate end of transition, and go to target value
										pStateGroupChunk->TransUpdateValue( eTarget, fNext, true );
										// Remove user from the transition user (will also destroy the transition because there was no other user).
										g_pTransitionManager->RemoveTransitionUser( pTransition, pStateGroupChunk );
									}
								}
								else
								{
									AkDeltaMonitorUpdateBrace updateBrace( trans.eChangeReason );
									pChunkValue->fValue = fNext;
									pStateGroupChunk->TransUpdateValue( eTarget, fNext, true );
								}
							}
							else
							{
								if ( pTransition )
									g_pTransitionManager->RemoveTransitionUser( pTransition, pStateGroupChunk );
							}
						}
					}
				}
			}
		}
	}

    pStateGroupInfo->ActualState = in_ulStateID;

	if ( ThisTransition.StateFrom != ThisTransition.StateTo )
	{
		MONITOR_STATECHANGED(in_ulStateGroupID, ThisTransition.StateFrom, ThisTransition.StateTo);
	}
}

bool CAkStateMgr::GetState(AkStateGroupID in_ulStateGroupID, AkStateID& out_ulStateID)
{
	AkStateGroupInfo** l_ppStateGrpInfo = m_StateGroups.Exists( in_ulStateGroupID );
	if(l_ppStateGrpInfo)
	{
		out_ulStateID = (*l_ppStateGrpInfo)->ActualState;
		return true;
	}

	out_ulStateID = AK_HASH_STATE_NONE;
	return false;
}

AkStateID CAkStateMgr::GetState(AkStateGroupID in_ulStateGroupID)
{
	AkStateID ulStateID;
	if ( ! GetState( in_ulStateGroupID, ulStateID ) )
		ulStateID = 0;
	// Normally want to use AK_HASH_STATE_NONE, but afraid of behavior change, so keep going with 0!
	return ulStateID;
}

#ifndef AK_OPTIMIZED
AKRESULT CAkStateMgr::ResetAllStates()
{
	//TODO(alessard) to be revised...
	AKRESULT eResult = AK_Success;

	//Tells everybody to set stat to none without transition time
	for( AkListStateGroups::Iterator iter = m_StateGroups.Begin(); iter != m_StateGroups.End(); ++iter )
	{
		AK::SoundEngine::SetState( (*iter).key, 0, true, false ); // true to skip transitiontime
	}

	// This should theorically not pass trough the index, it shall use the Master bus 
	// notification system once it is available
	// But for now, let's use the indexes.
	AKASSERT( g_pIndex );

	{
		CAkIndexItem<CAkParameterNodeBase*>& l_rIdx = g_pIndex->m_idxAudioNode;
		{//Bracket for autolock
			AkAutoLock<CAkLock> IndexLock( l_rIdx.m_IndexLock );

			CAkIndexItem<CAkParameterNodeBase*>::AkMapIDToPtr::Iterator iter = l_rIdx.m_mapIDToPtr.Begin();
			while( iter != l_rIdx.m_mapIDToPtr.End() )
			{
				static_cast<CAkParameterNodeBase*>( *iter )->UseState( true );
				++iter;
			}
		}
	}

	{
		CAkIndexItem<CAkParameterNodeBase*>&l_rIdx = g_pIndex->m_idxBusses;
		{//Bracket for autolock
			AkAutoLock<CAkLock> IndexLock( l_rIdx.m_IndexLock );

			CAkIndexItem<CAkParameterNodeBase*>::AkMapIDToPtr::Iterator iter = l_rIdx.m_mapIDToPtr.Begin();
			while( iter != l_rIdx.m_mapIDToPtr.End() )
			{
				static_cast<CAkParameterNodeBase*>( *iter )->UseState( true );
				++iter;
			}
		}
	}

	return eResult;
}

#endif

AKRESULT CAkStateMgr::RemoveAllStateGroups( bool in_bIsFromClearBanks )
{
	AkListStateGroups::Iterator iter = m_StateGroups.Begin();
	while( iter != m_StateGroups.End() )
    {
		AkStateGroupInfo* pInfo = (*iter).item;
		if( !in_bIsFromClearBanks || pInfo->members.IsEmpty() )
		{
			pInfo->Term();
			AkDelete(AkMemID_Structure, pInfo );
			iter = m_StateGroups.EraseSwap( iter );
		}
		else
		{
			++iter;
		}
    }
	return AK_Success;
}

AKRESULT CAkStateMgr::RegisterSwitch( CAkSwitchAware* in_pSwitchCntr, AkStateGroupID in_ulStateGroup )
{
	AKRESULT eResult = AK_Success;
	AKASSERT( in_pSwitchCntr );
	if(in_pSwitchCntr)
	{
		CAkStateMgr::RegisteredSwitch l_RS;
		l_RS.pSwitch = in_pSwitchCntr;
		l_RS.ulStateGroup = in_ulStateGroup;
		if ( !m_listRegisteredSwitch.AddLast( l_RS ) )
		{
			eResult = AK_Fail;
		}
	}
	else
	{
		eResult = AK_InvalidParameter;
	}

	return eResult;
}

AKRESULT CAkStateMgr::UnregisterSwitch( CAkSwitchAware* in_pSwitchCntr )
{
	AKRESULT eResult = AK_Success;

	AkListRegisteredSwitch::IteratorEx iter = m_listRegisteredSwitch.BeginEx();
	while( iter != m_listRegisteredSwitch.End() )
	{
		if( (*iter).pSwitch == in_pSwitchCntr )
		{
			if ( !m_bNotifyingSwitches )
			{
				iter = m_listRegisteredSwitch.Erase( iter );
			}
			else
			{
				(*iter).pSwitch = NULL;
				++iter;
				m_bUnregisteredSwitches = true;
			}
		}
		else
		{
			++iter;
		}
	}

	return eResult;
}

CAkStateMgr::PreparationStateItem* CAkStateMgr::GetPreparationItem( AkUInt32 in_ulGroup, AkGroupType in_eGroupType )
{
	AkAutoLock<CAkLock> gate( m_PrepareGameSyncLock );// to pad monitoring recaps.

	PreparationGroups* pPreparationGroup;
	if( in_eGroupType == AkGroupType_State )
	{
		pPreparationGroup = &m_PreparationGroupsStates;
	}
	else
	{
		AKASSERT( in_eGroupType == AkGroupType_Switch );
		pPreparationGroup = &m_PreparationGroupsSwitches;
	}
	
	PreparationGroups::IteratorEx iter = pPreparationGroup->BeginEx();
	while( iter != pPreparationGroup->End() )
	{
		if( in_ulGroup == iter.pItem->GroupID() )
			return iter.pItem;
		++iter;
	}

	PreparationStateItem* pPreparationStateItem = AkNew(AkMemID_Structure, PreparationStateItem(in_ulGroup));
	if (pPreparationStateItem)
	{
		pPreparationGroup->AddFirst(pPreparationStateItem);
	}

	return pPreparationStateItem;
}

AKRESULT CAkStateMgr::PrepareGameSync( 
		AkGroupType in_eGroupType, 
		AkUInt32 in_uGroupID, 
		AkUInt32 in_uGameSyncID, 
		bool in_bIsActive 
		)
{
	CAkStateMgr::PreparationStateItem* pPreparationStateItem = GetPreparationItem( in_uGroupID, in_eGroupType );
	if( !pPreparationStateItem )
	{
		// If is was going to be removed, not finding it is not an issue, return success.
		if( in_bIsActive )
			return AK_InsufficientMemory;
		else
			return AK_Success;
	}

	CAkPreparedContent::ContentList& rContentList = pPreparationStateItem->GetPreparedcontent()->m_PreparableContentList;

	CAkPreparedContent::ContentList::Iterator iter = rContentList.FindEx( in_uGameSyncID );

	if( iter != rContentList.End() )
	{
		if( in_bIsActive )
			return AK_Success; // nothing to do, everything is already alright.
		rContentList.EraseSwap( iter );
	}
	else
	{
		if( !in_bIsActive )
			return AK_Success; // nothing to do, everything is already alright.
		rContentList.AddLast( in_uGameSyncID );
	}

	AKRESULT eResult = pPreparationStateItem->Notify( in_uGameSyncID, in_bIsActive );
	if( eResult != AK_Success )
	{
		// If this assert pops. it means that we encountered an error while trying to free loaded memory.
		AKASSERT( in_bIsActive && "Unhandled situation, PrepareGameSync(...) failed to free allocated data" );
		rContentList.EraseSwap( iter );
	}
	else
	{
		MONITOR_GAMESYNC( in_uGroupID, in_uGameSyncID, in_bIsActive, in_eGroupType );
	}

	return eResult;
}

AKRESULT CAkStateMgr::UpdateSwitches( AkStateGroupID in_ulStateGroup, AkStateID in_StateFrom, AkStateID in_StateTo )
{
	AKRESULT eResult = AK_Success;

	if( in_StateFrom != in_StateTo )
	{
		AKASSERT( !m_bNotifyingSwitches );
		m_bNotifyingSwitches = true;

		for( AkListRegisteredSwitch::Iterator iter = m_listRegisteredSwitch.Begin(); iter != m_listRegisteredSwitch.End(); ++iter )
		{
			RegisteredSwitch& l_rRegSw = *iter;
			if( l_rRegSw.ulStateGroup == in_ulStateGroup )
			{
				if ( l_rRegSw.pSwitch )
				{
					l_rRegSw.pSwitch->SetSwitch( in_StateTo, AkRTPCKey(), NULL );
				}
			}
		}

		if ( m_bUnregisteredSwitches )
		{
			for( AkListRegisteredSwitch::IteratorEx iter = m_listRegisteredSwitch.BeginEx(); iter != m_listRegisteredSwitch.End(); )
			{
				if ( (*iter).pSwitch == NULL )
					iter = m_listRegisteredSwitch.Erase( iter );
				else
					++iter;
			}

			m_bUnregisteredSwitches = false;
		}

		m_bNotifyingSwitches = false;
	}

	return eResult;
}

AKRESULT CAkStateMgr::RegisterTrigger( IAkTriggerAware* in_pTrigerAware, AkTriggerID in_triggerID, CAkRegisteredObj* in_GameObj )
{
	AKASSERT( in_pTrigerAware );

	RegisteredTrigger	l_RT;
	l_RT.pTriggerAware	= in_pTrigerAware;
	l_RT.triggerID		= in_triggerID;
	l_RT.gameObj		= in_GameObj;

	if ( !m_listRegisteredTrigger.AddLast( l_RT ) )
		return AK_Fail;

	return AK_Success;
}

AKRESULT CAkStateMgr::UnregisterTrigger( IAkTriggerAware* in_pTrigerAware, AkTriggerID in_Trigger )
{
	AkListRegisteredTrigger::IteratorEx iter = m_listRegisteredTrigger.BeginEx();
	while( iter != m_listRegisteredTrigger.End() )
	{
		if( (*iter).pTriggerAware == in_pTrigerAware && (*iter).triggerID == in_Trigger )
		{
			iter = m_listRegisteredTrigger.Erase( iter );
			break;
		}
		else
		{
			++iter;
		}
	}

	return AK_Success;
}

AKRESULT CAkStateMgr::RegisterTrigger( IAkTriggerAware* in_pTrigerAware, CAkRegisteredObj* in_GameObj )
{
	AKASSERT( in_pTrigerAware );

	RegisteredTrigger	l_RT;
	l_RT.pTriggerAware	= in_pTrigerAware;
	l_RT.triggerID		= AK_INVALID_UNIQUE_ID;
	l_RT.gameObj		= in_GameObj;

	if ( !m_listRegisteredTrigger.AddLast( l_RT ) )
		return AK_Fail;

	return AK_Success;
}

AKRESULT CAkStateMgr::UnregisterTrigger( IAkTriggerAware* in_pTrigerAware )
{
    AkListRegisteredTrigger::IteratorEx iter = m_listRegisteredTrigger.BeginEx();
	while( iter != m_listRegisteredTrigger.End() )
	{
		if( (*iter).pTriggerAware == in_pTrigerAware )
		{
			iter = m_listRegisteredTrigger.Erase( iter );
		}
		else
		{
			++iter;
		}
	}

	return AK_Success;
}

void CAkStateMgr::Trigger( AkTriggerID in_Trigger, CAkRegisteredObj* in_GameObj )
{
	for( AkListRegisteredTrigger::Iterator iter = m_listRegisteredTrigger.Begin(); iter != m_listRegisteredTrigger.End(); ++iter )
	{
		RegisteredTrigger& l_rRegTrigger = *iter;
		if( l_rRegTrigger.triggerID == AK_INVALID_UNIQUE_ID || l_rRegTrigger.triggerID == in_Trigger )
		{
			if( in_GameObj == NULL || l_rRegTrigger.gameObj == in_GameObj )
			{
				l_rRegTrigger.pTriggerAware->Trigger( in_Trigger );
			}
		}
	}
}
