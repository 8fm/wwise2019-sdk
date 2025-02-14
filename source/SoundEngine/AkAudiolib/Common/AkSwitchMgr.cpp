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
// AkSwitchMgr.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkSwitchMgr.h"
#include "AkModulatorMgr.h"
#include "AkMonitor.h"
#include "AkSwitchAware.h"

#define DEFAULT_SWITCH_TYPE 0


CAkSwitchMgr::CAkSwitchMgr(): m_iSwitching(0)
{
}

CAkSwitchMgr::~CAkSwitchMgr()
{
}

AKRESULT CAkSwitchMgr::Init()
{
	if (!m_mapSubscriptions.Init(AK_HASH_SIZE_VERY_SMALL) || !m_mapEntries.Init(AK_HASH_SIZE_VERY_SMALL))
		return AK_InsufficientMemory;

	return AK_Success;
}

AKRESULT CAkSwitchMgr::Term()
{
	for( AkMapSwitchEntries::IteratorEx it = m_mapEntries.BeginEx(); it != m_mapEntries.End(); )
	{
		AkSwitchEntry* pEntry = *it;
		it = m_mapEntries.Erase( it );

		AkDelete(AkMemID_Structure, pEntry );
	}

	for( AkMapSwitchSubscriptions::IteratorEx it = m_mapSubscriptions.BeginEx(); it != m_mapSubscriptions.End(); )
	{
		AkSwitchSubscription* pSubscription = *it;
		it = m_mapSubscriptions.Erase( it );

		RemoveSubscriptionFromEntry( pSubscription );
		AkDelete(AkMemID_Structure, pSubscription );
	}

	m_mapEntries.Term();
	m_mapSubscriptions.Term();
	m_delayedSubsActions.Term();

	return AK_Success;
}

void CAkSwitchMgr::RemoveSubscriptionFromEntry( AkSwitchSubscription* in_pSubscription )
{
	AkSwitchEntry* pEntry = m_mapEntries.Exists( in_pSubscription->switchGroup );
	if ( pEntry )
	{
		pEntry->subscriptions.Unset( in_pSubscription );
	}
}

AKRESULT CAkSwitchMgr::SubscribeSwitch( CAkSwitchAware* in_pSubscriber, AkSwitchGroupID in_switchGroup )
{
	if (!IsSwitching())
	{
		return _SubscribeSwitch(in_pSubscriber,in_switchGroup);
	}
	else
	{
		return m_delayedSubsActions.AddLast( SubsAction( SubsAction::ADD, in_pSubscriber, in_switchGroup) ) != NULL ? AK_Success : AK_Fail;
	}
}

AKRESULT CAkSwitchMgr::_SubscribeSwitch( CAkSwitchAware* in_pSubscriber, AkSwitchGroupID in_switchGroup )
{
	AKASSERT( !IsSwitching() );
	AKASSERT(in_pSubscriber);

	AKRESULT eResult = AK_InsufficientMemory;

	if( in_pSubscriber )
	{
		AkSwitchSubscriptionKey key(in_pSubscriber);

		AkSwitchSubscription* pSubscription = m_mapSubscriptions.Exists( key );
		if( pSubscription )
		{
			// Look for entry and remove subscription from it
			RemoveSubscriptionFromEntry( pSubscription );

			// Subscribe to new switch group
			pSubscription->switchGroup = in_switchGroup;
		}
		else
		{
			pSubscription = AkNew(AkMemID_Structure, AkSwitchSubscription());
			if ( pSubscription )
			{
				pSubscription->key = key;
				pSubscription->switchGroup = in_switchGroup;
				m_mapSubscriptions.Set( pSubscription );
			}
		}

		if( pSubscription )
		{
			AkSwitchEntry* pEntry = GetSwitchEntry( in_switchGroup );
			if( pEntry && pEntry->subscriptions.Add( pSubscription ) )
				eResult = AK_Success;
		}

		if( eResult != AK_Success )
		{
			m_mapSubscriptions.Unset( key );
			if ( pSubscription )
			{
				RemoveSubscriptionFromEntry( pSubscription );
				AkDelete(AkMemID_Structure, pSubscription );
			}
		}
	}

	return eResult;
}

void CAkSwitchMgr::UnSubscribeSwitch( CAkSwitchAware* in_pSubscriber )
{
	_UnSubscribeSwitch( in_pSubscriber );
	
	if (IsSwitching())
	{
		m_delayedSubsActions.AddLast( SubsAction( SubsAction::REMOVE, in_pSubscriber, 0) );
	}
}

void CAkSwitchMgr::_UnSubscribeSwitch( CAkSwitchAware* in_pSubscriber )
{
	AKASSERT(in_pSubscriber);

	AkSwitchSubscriptionKey key(in_pSubscriber);

	AkSwitchSubscription* pSubscription = m_mapSubscriptions.Exists( key );
	if( pSubscription )
	{
		if (!IsSwitching())
		{
			m_mapSubscriptions.Unset( key );
			RemoveSubscriptionFromEntry( pSubscription );
			AkDelete(AkMemID_Structure, pSubscription );
		}
		else
		{
			// We can not modify the map if we are in the middle of switching.  Mark for clean up later.
			pSubscription->bSubscribed = false;
		}
	}
}

void CAkSwitchMgr::ExecuteSubsActions()
{
	for ( SubsActionArray::Iterator it = m_delayedSubsActions.Begin(); it != m_delayedSubsActions.End(); ++it )
	{
		SubsAction& ss = *it;
		if ( ss.addOrRemove == SubsAction::ADD )
		{
			_SubscribeSwitch( ss.pSubscriber, ss.switchGroup );
		}
		else if ( ss.addOrRemove == SubsAction::REMOVE )
		{
			_UnSubscribeSwitch( ss.pSubscriber );
		}
	}

	m_delayedSubsActions.RemoveAll();
}

CAkSwitchMgr::AkSwitchEntry* CAkSwitchMgr::GetSwitchEntry( AkSwitchGroupID in_switchGroup )
{
	AkSwitchEntry* pEntry = m_mapEntries.Exists( in_switchGroup );
	if ( pEntry )
		return pEntry;

	pEntry = AkNew(AkMemID_Structure, AkSwitchEntry(in_switchGroup));
	if (!pEntry)
		return NULL;

	m_mapEntries.Set( pEntry );

	return pEntry;
}

AKRESULT CAkSwitchMgr::AddSwitchRTPC(
								   AkSwitchGroupID		in_switchGroup, 
								   AkRtpcID				in_rtpcID,
								   AkRtpcType			in_rtpcType,
								   AkSwitchGraphPoint*	in_pGraphPts, //NULL if none
								   AkUInt32				in_numGraphPts //0 if none
								   )
{
	AKASSERT( !IsSwitching() );

	AKRESULT eResult = AK_Success;

	AkSwitchEntry* pEntry = GetSwitchEntry( in_switchGroup );
	if( pEntry )
	{
		// Release previous resources if an RTPC was already associated
		pEntry->RemoveRTPC();

		// Allocate required memory
		if( in_numGraphPts )
			eResult = pEntry->AddRTPC( in_rtpcID, in_rtpcType, in_pGraphPts, in_numGraphPts );
	}
	else
		eResult = AK_InsufficientMemory;

	return eResult;
}

void CAkSwitchMgr::RemoveSwitchRTPC( AkSwitchGroupID in_switchGroup )
{
	AKASSERT(!IsSwitching());

	AkSwitchEntry* pEntry = GetSwitchEntry( in_switchGroup );
	if( pEntry )
		pEntry->RemoveRTPC();
}

void CAkSwitchMgr::ResetSwitches( CAkRegisteredObj* in_pGameObj )
{
	SwitchingInThisScope s;

	AkMapSwitchEntries::IteratorEx it = m_mapEntries.BeginEx();
	while( it != m_mapEntries.End() )
	{
		AkSwitchEntry* pEntry = *it;
		++it;

		if( pEntry )
		{
			pEntry->RemoveValues();

			AkSubscriptionArray::Iterator is = pEntry->subscriptions.Begin();
			for( ; is != pEntry->subscriptions.End(); ++is )
			{
				CAkSwitchAware* pSubscriber = (*is)->GetSubscriber();
				if (pSubscriber)
					pSubscriber->SetSwitch( DEFAULT_SWITCH_TYPE, in_pGameObj );
			}

			if( pEntry->IsObsolete() )
			{
				m_mapEntries.Unset( pEntry->key );
				AkDelete(AkMemID_Structure, pEntry );
			}
		}
	}
}

bool CAkSwitchMgr::GetSwitch( AkSwitchGroupID in_switchGroup, const AkRTPCKey& in_rtpcKey, AkSwitchStateID& out_switchID )
{
	// Look for entry.
	AkSwitchEntry* pEntry = m_mapEntries.Exists( in_switchGroup );
	if( pEntry )
	{
		out_switchID = pEntry->GetSwitch( in_rtpcKey );
		return true;
	}

	out_switchID = AK_DEFAULT_SWITCH_STATE;
	return false;
}

AkSwitchStateID CAkSwitchMgr::GetSwitch( AkSwitchGroupID in_switchGroup, const AkRTPCKey& in_rtpcKey )
{
	AkSwitchStateID switchId;
	GetSwitch( in_switchGroup, in_rtpcKey, switchId );
	return switchId;
}

void CAkSwitchMgr::UnregisterGameObject( CAkRegisteredObj* in_pGameObj )
{
	AKASSERT( in_pGameObj != NULL );
	if( in_pGameObj == NULL )
		return;

	//Clear its specific switches
	AkMapSwitchEntries::IteratorEx it = m_mapEntries.BeginEx();
	for( ; it != m_mapEntries.End(); ++it )
	{
		AkSwitchEntry* pEntry = *it;
		pEntry->RemoveGameObject( in_pGameObj );
	}
}

void CAkSwitchMgr::TriggerModulator( const CAkSwitchAware* in_pSubscriber, const AkModulatorTriggerParams& in_params, CAkModulatorData* out_pModPBIData )
{
	AKASSERT(in_pSubscriber);

	AkSwitchSubscriptionKey key(const_cast<CAkSwitchAware*>( in_pSubscriber ));

	AkSwitchSubscription* pSubscription = m_mapSubscriptions.Exists( key );
	if( pSubscription && pSubscription->bSubscribed )
	{
		AkSwitchEntry* pEntry = m_mapEntries.Exists( pSubscription->switchGroup );
		if( pEntry && pEntry->HasModulator() )
		{
			AkModulatorSubscriberInfo subsrInfo;
			subsrInfo.pSubscriber = pEntry;
			subsrInfo.eSubscriberType = CAkRTPCMgr::SubscriberType_SwitchGroup;
			g_pModulatorMgr->Trigger(subsrInfo, in_params, out_pModPBIData);
		}
	}
}

CAkSwitchMgr::AkSwitchEntry::~AkSwitchEntry()
{
	RemoveGameObject( NULL );
	RemoveRTPC();
	values.Term();
	subscriptions.Term();
}

void CAkSwitchMgr::AkSwitchEntry::RemoveGameObject( CAkRegisteredObj * in_pGameObj )
{
	values.Unset( AkRTPCKey(in_pGameObj) );
}

void CAkSwitchMgr::AkSwitchEntry::RemoveRTPC()
{
	if( rtpcID != AK_INVALID_RTPC_ID )
	{
		g_pRTPCMgr->UnregisterSwitchGroup( this );
		rtpcID = AK_INVALID_RTPC_ID;
		states.Term();
	}
}

AKRESULT CAkSwitchMgr::AkSwitchEntry::AddRTPC( AkRtpcID in_rtpcID, AkRtpcType in_rtpcType, AkSwitchGraphPoint* in_pGraphPts, AkUInt32 in_numGraphPts )
{
	AKASSERT( in_pGraphPts && in_numGraphPts );

	AKRESULT eResult = states.Reserve( in_numGraphPts );

	if( eResult == AK_Success )
	{
		rtpcID = in_rtpcID;
		rtpcType = in_rtpcType;

		AkRTPCGraphPoint* pRtpcGraphPts = (AkRTPCGraphPoint*)AkAlloc( AkMemID_Object, in_numGraphPts * sizeof(AkRTPCGraphPoint) );
		if( pRtpcGraphPts )
		{
			for( AkUInt32 i = 0; i < in_numGraphPts; ++i )
			{
				pRtpcGraphPts[i].From = in_pGraphPts[i].From;
				pRtpcGraphPts[i].To = (AkReal32)i;
				pRtpcGraphPts[i].Interp = in_pGraphPts[i].Interp;
				states.AddLast( in_pGraphPts[i].To );
			}

			eResult = g_pRTPCMgr->RegisterSwitchGroup( this, in_rtpcID, in_rtpcType, pRtpcGraphPts, in_numGraphPts );
			AkFree(AkMemID_Object, pRtpcGraphPts );
		}
		else
			eResult = AK_InsufficientMemory;
	}

	if( eResult != AK_Success )
	{
		rtpcID = AK_INVALID_RTPC_ID;
		states.Term();
	}

	return eResult;
}

bool CAkSwitchMgr::AkSwitchEntry::SetSwitchInternal( AkSwitchStateID in_switchState, CAkRegisteredObj* in_pGameObj )
{
	AkRTPCValue* pValue = values.Set( AkRTPCKey( in_pGameObj ) );
	if( pValue )
		pValue->uValue = (AkUInt32)in_switchState;

	// Update all subscribers if not on an RTPC
	if( rtpcID == AK_INVALID_RTPC_ID )
	{
		AkSubscriptionArray::Iterator it = subscriptions.Begin();
		for( ; it != subscriptions.End(); ++it )
		{
			CAkSwitchAware* pSubscriber = (*it)->GetSubscriber();
			if (pSubscriber)
				pSubscriber->SetSwitch( in_switchState, in_pGameObj );
		}
	}

	return pValue != NULL;
}

void CAkSwitchMgr::AkSwitchEntry::SetSwitchFromRTPCMgr( const AkRTPCKey& in_rtpcKey, AkUInt32 in_rtpcValue, AkRTPCExceptionChecker* in_pExCheck )
{
	AKASSERT( rtpcID != AK_INVALID_RTPC_ID );

	if( in_rtpcValue < states.Length() )
	{
		AkSubscriptionArray::Iterator it = subscriptions.Begin();
		for( ; it != subscriptions.End(); ++it )
		{
			CAkSwitchAware* pSubscriber = (*it)->GetSubscriber();
			if (pSubscriber)
				pSubscriber->SetSwitch( states[ in_rtpcValue ], in_rtpcKey.GameObj(), in_pExCheck );
		}
	}
}

AkSwitchStateID CAkSwitchMgr::AkSwitchEntry::GetSwitch( const AkRTPCKey& in_rtpcKey )
{
	AkSwitchStateID switchState = AK_DEFAULT_SWITCH_STATE;
	AkRTPCKey matchKey = in_rtpcKey;

	// Check if mapped to an RTPC
	if( rtpcID != AK_INVALID_RTPC_ID )
	{
		AkUInt32 rtpcValue = (AkUInt32)g_pRTPCMgr->GetRTPCConvertedValue( this, matchKey );
		if( rtpcValue < states.Length() )
			switchState = states[ rtpcValue ];
	}
	else
	{
		// Look in values
		AkRTPCValue* pValue = values.FindBestMatch( matchKey );
		if( pValue )
			switchState = pValue->uValue;
	}

	return switchState;
}

CAkSwitchMgr::SwitchingInThisScope::SwitchingInThisScope()
{ 
	AKASSERT(g_pSwitchMgr->m_iSwitching >= 0); 
	g_pSwitchMgr->m_iSwitching++; 
}

CAkSwitchMgr::SwitchingInThisScope::~SwitchingInThisScope()
{ 
	AKASSERT(g_pSwitchMgr->m_iSwitching > 0); 
	g_pSwitchMgr->m_iSwitching--;
	if (g_pSwitchMgr->m_iSwitching == 0)
	{
		g_pSwitchMgr->ExecuteSubsActions();
	}
}
