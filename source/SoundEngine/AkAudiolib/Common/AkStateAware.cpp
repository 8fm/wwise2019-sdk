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
// AkParameterNodeBase.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "AkStateAware.h"
#include "AkState.h"
#include "AkAudioLibIndex.h"
#include "AkStateMgr.h"
#include "AkTransitionManager.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include "AkParameters.h" //for AkPropDescriptor

CAkStateAware::CAkStateAware()
	: m_bUseState( 1 )
{}

CAkStateAware::~CAkStateAware()
{}

void CAkStateAware::SetStateProperties( AkUInt32 in_uProperties, AkStatePropertyUpdate* in_pProperties, bool in_bNotify )
{
	if ( ! EnsureStateData() )
		return;

	// Notify all current targets immediately of property changes because we'll forget the properties we're losing
	NotifyStateParamTargets();

	StatePropertyArray& stateProps = *GetStateProperties();

	// If state properties have not chaged then no need to do anything
	if ( stateProps.Length() == in_uProperties )
	{
		AkUInt32 i = 0;
		for ( ; i < in_uProperties; ++i )
		{
			if ( stateProps[i].propertyId != (AkStatePropertyId)in_pProperties[i].propertyId
				|| stateProps[i].accumType != in_pProperties[i].accumType )
				break;
		}
		if ( i == in_uProperties )
			return;
	}

	stateProps.Term();

	if ( in_uProperties > 0 && stateProps.Resize( in_uProperties ) )
	{
		for ( AkUInt32 i = 0; i < in_uProperties; ++i )
		{
			stateProps[i].propertyId = (AkStatePropertyId)in_pProperties[i].propertyId;
			stateProps[i].accumType = (AkUInt8)in_pProperties[i].accumType;
			stateProps[i].inDb = in_pProperties[i].inDb ? 1 : 0;
		}
	}

	if (in_bNotify)
		NotifyStateParametersModified();
}

bool CAkStateAware::HasState( AkRtpcID in_uProperty )
{
	StateGroupChunkList* chunks = GetStateChunks();
	return chunks != NULL
		&& ! chunks->IsEmpty()
		&& IsStateProperty( in_uProperty );
}

bool CAkStateAware::IsStateProperty( AkRtpcID in_uProperty )
{
	if ( StatePropertyArray* pStateProps = GetStateProperties() )
	{
		for ( AkUInt32 i = 0; i < pStateProps->Length(); ++i )
		{
			if ( (*pStateProps)[i].propertyId == (AkStatePropertyId)in_uProperty )
				return true;
		}
	}
	return false;
}

AkStateGroupChunk* CAkStateAware::AddStateGroup( AkStateGroupID in_ulStateGroupID, bool in_bNotify )
{
	// Now this function simply adds and does not replace, a call with 0 would be suspiscious.
	AKASSERT( in_ulStateGroupID );

	if ( ! EnsureStateData() )
		return NULL;

	AkStateGroupChunk* pChunk = GetStateGroupChunk( in_ulStateGroupID );
	if( pChunk )
		return pChunk;

	pChunk = AkNew(AkMemID_Structure, AkStateGroupChunk( this, in_ulStateGroupID ) );
	if ( !pChunk )
		return NULL;

	AKRESULT eResult = g_pStateMgr->AddStateGroupMember( in_ulStateGroupID, pChunk );
	if( eResult != AK_Success )
	{
		AkDelete(AkMemID_Structure, pChunk );
		return NULL;
	}

	StateGroupChunkList* pStateChunks = GetStateChunks();
	pStateChunks->AddFirst( pChunk );

	pChunk->m_ulActualState = g_pStateMgr->GetState( in_ulStateGroupID );

	if (in_bNotify)
		NotifyStateParametersModified();

	return pChunk;
}

void CAkStateAware::RemoveStateGroup( AkStateGroupID in_ulStateGroupID, bool in_bNotify )
{
	StateGroupChunkList* pStateChunks = GetStateChunks();
	if ( pStateChunks )
	{
		for ( StateGroupChunkList::IteratorEx it = pStateChunks->BeginEx(); it != pStateChunks->End(); ++it )
		{
			if ( (*it)->m_ulStateGroup == in_ulStateGroupID )
			{
				pStateChunks->Erase( it );
				g_pStateMgr->RemoveStateGroupMember( in_ulStateGroupID, *it );

				while( (*it)->m_mapStates.Length() > 0 )
					(*it)->RemoveState( (*it)->m_mapStates.Begin().pItem->key );

				AKASSERT( (*it)->m_mapStates.IsEmpty() );
				AkDelete(AkMemID_Structure, *it );
				if (in_bNotify)
					NotifyStateParametersModified();
				return;
			}
		}
	}
}

void CAkStateAware::RemoveStateGroups(bool in_bNotify)
{
	StateGroupChunkList* pStateChunks = GetStateChunks();
	if ( pStateChunks )
	{
		if ( !pStateChunks->IsEmpty() )
		{
			do
			{
				AkStateGroupChunk * pStateGroupChunk = pStateChunks->First();
				pStateChunks->RemoveFirst();

				g_pStateMgr->RemoveStateGroupMember( pStateGroupChunk->m_ulStateGroup, pStateGroupChunk );

				AkMapStates& mapStates = pStateGroupChunk->m_mapStates;
				while( mapStates.Length() > 0 )
					pStateGroupChunk->RemoveState( mapStates.Begin().pItem->key );

				AkDelete(AkMemID_Structure, pStateGroupChunk );
			}
			while ( !pStateChunks->IsEmpty() );

			if (in_bNotify)
				NotifyStateParametersModified();
		}
	}
}

AkStateGroupChunk* CAkStateAware::GetStateGroupChunk( AkStateGroupID in_ulStateGroupID )
{
	StateGroupChunkList* pStateChunks = GetStateChunks();
	if ( pStateChunks )
	{
		for ( StateGroupChunkList::Iterator it = pStateChunks->Begin(); it != pStateChunks->End(); ++it )
		{
			if ( (*it)->m_ulStateGroup == in_ulStateGroupID )
				return *it;
		}
	}

	return NULL;
}

AKRESULT CAkStateAware::AddState( AkStateGroupID in_ulStateGroupID, AkUniqueID in_ulStateInstanceID, AkStateID in_ulStateID)
{
	AkStateGroupChunk* pChunk = GetStateGroupChunk( in_ulStateGroupID );
	AKASSERT( pChunk );
	if( pChunk )
		return pChunk->AddState( in_ulStateInstanceID, in_ulStateID );

	return AK_Fail;
}

void CAkStateAware::RemoveState( AkStateGroupID in_ulStateGroupID, AkStateID in_ulStateID )
{
	AkStateGroupChunk* pChunk = GetStateGroupChunk( in_ulStateGroupID );
	if( pChunk )
	{
		pChunk->RemoveState( in_ulStateID );
		NotifyStateParametersModified();
	}
}

AKRESULT CAkStateAware::UpdateStateGroups(AkUInt32 in_uGroups, AkStateGroupUpdate* in_pGroups, AkStateUpdate* in_pStates)
{
	if ( ! EnsureStateData() )
		return AK_InsufficientMemory;

	AKRESULT res = AK_Success;
	AKRESULT resFinal = AK_Success;
	bool bNotify = false;

	StateGroupChunkList* pStateChunks = GetStateChunks();

	//Gather all the group IDs so we can detect which ones to delete.
	AkUInt32 uGroupCount = 0;
	for ( StateGroupChunkList::Iterator it = pStateChunks->Begin(); it != pStateChunks->End(); ++it )
		uGroupCount++;

	AkUniqueID *pGroupsToDelete = NULL;
	if ( uGroupCount > 0 )
	{
		pGroupsToDelete = (AkUniqueID*)AkAlloca(sizeof(AkUniqueID)*uGroupCount);
		if (pGroupsToDelete == NULL)
			return AK_InsufficientMemory;

		for ( StateGroupChunkList::Iterator it = pStateChunks->Begin(); it != pStateChunks->End(); ++it )
		{
			*pGroupsToDelete = it.pItem->m_ulStateGroup;
			pGroupsToDelete++;
		}
		pGroupsToDelete -= uGroupCount;
	}

	//Go through all recieved groups
	for ( ; in_uGroups > 0; in_uGroups--, in_pGroups++ )
	{
		AkStateGroupChunk* pChunk = NULL;

		AkUInt32 iGroup = 0;
		for ( ; pGroupsToDelete != NULL && iGroup < uGroupCount && pGroupsToDelete[iGroup] != in_pGroups->ulGroupID; iGroup++ )
			/*Empty, searching the right slot*/;

		if ( iGroup != uGroupCount )
		{
			pGroupsToDelete[iGroup] = pGroupsToDelete[uGroupCount-1];	//This group is used, don't delete it.  //Swap element with last of list.
			uGroupCount--;

			//Check if the sync state type changed.
			pChunk = GetStateGroupChunk(in_pGroups->ulGroupID);
			bNotify |= (pChunk->m_eStateSyncType != (AkUInt8)in_pGroups->eSyncType);	
		}
		else
		{
			bNotify = true;					//New group, we'll need to notify.

			pChunk = AddStateGroup(in_pGroups->ulGroupID);
			if (pChunk == NULL)
			{
				resFinal = AK_InsufficientMemory;
				break;
			}
		}
		pChunk->m_eStateSyncType = (AkUInt8)in_pGroups->eSyncType;

		//Find which states were deleted.
		AkUniqueID *pStatesToDelete = (AkUniqueID*)AkAlloca(sizeof(AkUniqueID)*pChunk->m_mapStates.Reserved());

		AkUInt32 uPreviousStatesCount = 0;
		AkUniqueID *pState = pStatesToDelete;
		for ( AkMapStates::Iterator it = pChunk->m_mapStates.Begin(); it != pChunk->m_mapStates.End(); ++it )
		{
			*pState = it.pItem->item.pState->ID();	
			pState++;
			uPreviousStatesCount++;
		}

		for ( AkUInt32 i = 0; i < in_pGroups->ulStateCount; i++, in_pStates++ )
		{
			//Find if this custom state was used already.
			AkUInt32 iState = 0;
			for ( ; iState < uPreviousStatesCount && pStatesToDelete[iState] != in_pStates->ulStateInstanceID; iState++ )
				/*Empty, searching the right slot*/;

			if ( iState != uPreviousStatesCount )
			{
				//This group is used, don't delete it.  
				pStatesToDelete[iState] = pStatesToDelete[uPreviousStatesCount-1];	//Swap element with last of list.
				uPreviousStatesCount--;
			}
			else
			{
				bNotify = true;	//New group, we'll need to notify.

				res = pChunk->AddState( in_pStates->ulStateInstanceID, in_pStates->ulStateID, false );
				if (res != AK_Success)
					resFinal = res;
			}
		}

		//Delete states that disappeared
		bNotify |= uPreviousStatesCount > 0;
		for ( AkUInt32 iState = 0; iState < uPreviousStatesCount; iState++ )
			pChunk->RemoveState( pStatesToDelete[iState] );
	}

	//Delete unused groups.
	bNotify |= uGroupCount > 0;
	for ( AkUInt32 iGroup = 0; iGroup < uGroupCount; iGroup++ )
		RemoveStateGroup( pGroupsToDelete[iGroup], false );

	if (bNotify)
		NotifyStateParametersModified();

	return resFinal;
}

void CAkStateAware::NotifyStateParametersModified()
{
	StateGroupChunkList* pStateChunks = GetStateChunks();
	if ( ! pStateChunks )
		return;

	for ( StateGroupChunkList::Iterator iter = pStateChunks->Begin(); iter != pStateChunks->End(); ++iter )
	{
		AkStateGroupChunk* pStateGroupChunk = *iter;

		CAkState* pState = pStateGroupChunk->GetState();
		if( pState )
		{
			for ( AkStatePropBundle::Iterator it = pState->Props().Begin(), itEnd = pState->Props().End(); it != itEnd; ++it )
			{
				AkReal32 fProp = *it.pValue;

				AkStateValue * pChunkValue = pStateGroupChunk->m_values.FindProp( (AkPropID) *it.pID );
				if ( pChunkValue )
				{
					if ( pChunkValue->pTransition )
					{
						g_pTransitionManager->ChangeParameter(
										pChunkValue->pTransition,
										*it.pID,
										fProp,
										0,
										AkCurveInterpolation_Linear,
										AkValueMeaning_Default);
					}
					else
					{
						pChunkValue->fValue = fProp;
					}
				}
				else
				{
					pChunkValue = pStateGroupChunk->m_values.AddAkProp( (AkPropID) *it.pID );
					if ( pChunkValue )
					{
						pChunkValue->fValue = fProp;
						pChunkValue->pTransition = NULL;
					}
				}
			}
		}
		else
		{
			pStateGroupChunk->FlushStateTransitions();// Only on the stateGroup.
		}
	}

	UpdateStateParamTargets();
}

void CAkStateAware::UseState( bool in_bUseState )
{
	m_bUseState = in_bUseState ? 1 : 0;
	NotifyStateParamTargets();
}


AKRESULT CAkStateAware::ReadStateChunk( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	AkUInt32 ulNumStateProps = READVARIABLESIZEBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
	if ( ulNumStateProps != 0 )
	{
		if ( ! EnsureStateData() )
			return AK_InsufficientMemory;

		StatePropertyArray& stateProps = *GetStateProperties();
		if ( ! stateProps.Resize( ulNumStateProps ) )
			return AK_InsufficientMemory;

		for ( AkUInt32 propIdx = 0; propIdx < ulNumStateProps; ++propIdx )
		{
			stateProps[ propIdx ].propertyId = (AkStatePropertyId)READVARIABLESIZEBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
			stateProps[ propIdx ].accumType = (AkUInt8)READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
			stateProps[ propIdx ].inDb = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize ) ? 1 : 0;
		}
	}

	AkUInt32 ulNumStateGroups = READVARIABLESIZEBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
	if ( ulNumStateGroups != 0 && ! EnsureStateData() )
		return AK_InsufficientMemory;

	for ( AkUInt32 groupIdx = 0 ; groupIdx < ulNumStateGroups ; ++groupIdx )
	{
		AkStateGroupID ulStateGroupID = READBANKDATA( AkStateGroupID, io_rpData, io_rulDataSize );
		AKASSERT( ulStateGroupID );

		AkStateGroupChunk* pChunk = AddStateGroup( ulStateGroupID );
		if ( ! pChunk )
			return AK_Fail;

		pChunk->m_eStateSyncType = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );

		// Read Num States
		AkUInt32 ulNumStates = READVARIABLESIZEBANKDATA( AkUInt16, io_rpData, io_rulDataSize );

		for ( AkUInt32 i = 0 ; i < ulNumStates ; ++i )
		{
			AkUInt32 state = READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
			AkUInt32 iD = READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );

			AKRESULT eResult = pChunk->AddState( iD, state );
			if ( eResult != AK_Success )
				return eResult;
		}
	}

	// Use State is hardcoded to true, not read from banks
	UseState( true );

	return AK_Success;
}


void CAkStateAware::SetStateSyncType( AkStateGroupID in_stateGroupID, AkUInt32/*AkSyncType*/ in_eSyncType )
{
	AkStateGroupChunk* pChunk = AddStateGroup( in_stateGroupID );
	if( pChunk )
	{
		pChunk->m_eStateSyncType = (AkUInt8)in_eSyncType;
	}
}

bool CAkStateAware::CheckSyncTypes( AkStateGroupID in_stateGroupID, CAkStateSyncArray* io_pSyncTypes )
{
	AkStateGroupChunk* pStateGroupChunk = GetStateGroupChunk( in_stateGroupID );
	if( pStateGroupChunk )
	{
		AkSyncType syncType = (AkSyncType)pStateGroupChunk->m_eStateSyncType;
		if( syncType == SyncTypeImmediate )
		{
			io_pSyncTypes->RemoveAllSync();
            io_pSyncTypes->GetStateSyncArray().AddLast( syncType );
			return true;
		}
		else
		{
			bool bFound = false;
			for( StateSyncArray::Iterator iter = io_pSyncTypes->GetStateSyncArray().Begin(); iter != io_pSyncTypes->GetStateSyncArray().End(); ++iter )
			{
				if( *iter == syncType )
				{
					bFound = true;
					break;
				}
			}
			if( !bFound )
			{
				io_pSyncTypes->GetStateSyncArray().AddLast( syncType );
			}
		}
	}
	return false;
}

void CAkStateAware::FlushStateTransitions()
{
	StateGroupChunkList* pStateChunks = GetStateChunks();
	if ( pStateChunks )
	{
		for ( StateGroupChunkList::Iterator iter = pStateChunks->Begin(); iter != pStateChunks->End(); ++iter )
			(*iter)->FlushStateTransitions();
	}
}

void CAkStateAware::PauseTransitions( bool in_bPause )
{
	// should we pause ?
	StateGroupChunkList* pStateChunks = GetStateChunks();
	if ( pStateChunks )
	{
		for( StateGroupChunkList::Iterator iter = pStateChunks->Begin(); iter != pStateChunks->End(); ++iter )
		{
			AkStateGroupChunk* pStateGroupChunk = *iter;

			for ( AkStateValues::Iterator it = pStateGroupChunk->m_values.Begin(), itEnd = pStateGroupChunk->m_values.End(); it != itEnd; ++it )
			{
				if ( it.pValue->pTransition )
				{
					if( in_bPause )
						g_pTransitionManager->Pause( it.pValue->pTransition );
					else
						g_pTransitionManager->Resume( it.pValue->pTransition );
				}
			}
		}
	}
}

void CAkStateAware::PushStateParamUpdate( AkStatePropertyId in_propertyId, AkRtpcAccum in_rtpcAccum, AkStateGroupID in_stateGroup, AkReal32 in_oldValue, AkReal32 in_newValue, bool in_transitionDone )
{}




//====================================================================================================
// AkStateGroupChunk
//====================================================================================================


CAkState* AkStateGroupChunk::GetState( AkStateID in_StateTypeID )
{
	AkStateLink* l_pStateLink = m_mapStates.Exists( in_StateTypeID );
	if( l_pStateLink )
		return ( *l_pStateLink ).pState;

	return NULL;
}

AKRESULT AkStateGroupChunk::AddState( AkUniqueID in_ulStateInstanceID, AkStateID in_ulStateID, bool in_bNotify )
{
	CAkState* pState = g_pIndex->m_idxCustomStates.GetPtrAndAddRef( in_ulStateInstanceID );

	AkStateLink* l_pStateLink = m_mapStates.Exists( in_ulStateID );
	if( l_pStateLink )
	{
		if( pState == l_pStateLink->pState )// If we are replacing it by the exact same state.
		{
			// Nothing to Do.
			if( pState )
				pState->Release();

			return AK_Success;
		}

		// Remove the existing one.
		l_pStateLink->pState->TermNotificationSystem();
		l_pStateLink->pState->Release();

		m_mapStates.Unset( in_ulStateID );
	}

	AKASSERT( in_ulStateID );

	if(!pState)
	{
		return AK_InvalidInstanceID;
	}

	AkStateLink Link;
	Link.pState = pState;
	//Link.ulStateID = in_ulStateInstanceID;

	if ( m_mapStates.Set( in_ulStateID, Link ) )
	{
		pState->InitNotificationSystem( m_pOwner );

		if (in_bNotify)
			m_pOwner->NotifyStateParametersModified();
		return AK_Success;
	}
	else
	{
		pState->Release();
		return AK_InsufficientMemory;
	}
}

void AkStateGroupChunk::RemoveState(
		AkStateID in_ulStateID //SwitchState
		)
{
	AkStateLink* l_pStateLink = m_mapStates.Exists(in_ulStateID);
	if( !l_pStateLink )
		return;

	l_pStateLink->pState->TermNotificationSystem();
	l_pStateLink->pState->Release();

	m_mapStates.Unset( in_ulStateID );
}

void AkStateGroupChunk::FlushStateTransitions()
{
	for ( AkStateValues::Iterator it = m_values.Begin(), itEnd = m_values.End(); it != itEnd; ++it )
	{
		if ( it.pValue->pTransition )
		{
			g_pTransitionManager->RemoveTransitionUser( it.pValue->pTransition, this );
			it.pValue->pTransition = NULL;
			m_pOwner->StateTransitionRemoved();
		}
	}
}

void AkStateGroupChunk::TransUpdateValue(AkIntPtr in_eTarget, AkReal32 in_fValue, bool in_bIsTerminated)
{
	AkStatePropertyInfo info;
	info.FromUInt( (AkUInt32)in_eTarget );

	AkReal32 fDefault = (info.accumType == AkRtpcAccum_Multiply) ? 1.f : 0.f;
	AkStateValue * pChunkValue = m_values.FindProp( info.propertyId );
	AkReal32 fPrevious = pChunkValue ? pChunkValue->fValue : fDefault;

	bool bTransitionDone = false;
	if( pChunkValue )
	{
		pChunkValue->fValue = in_fValue;

		if( in_bIsTerminated && pChunkValue->pTransition )
		{
			pChunkValue->pTransition = NULL;
			bTransitionDone = true;
		}
	}

	if (m_pOwner->UseState())
	{
		AkDeltaMonitor::LogDriver(m_ulStateGroup, m_ulActualState);
		m_pOwner->PushStateParamUpdate(info.propertyId, (AkRtpcAccum)info.accumType, m_ulStateGroup, fPrevious, in_fValue, bTransitionDone);		
	}

	if ( bTransitionDone )
	{
		m_pOwner->StateTransitionRemoved();
	}

	// DO NOT ADD ANYTHING, m_pOwner may have been destroyed
}
