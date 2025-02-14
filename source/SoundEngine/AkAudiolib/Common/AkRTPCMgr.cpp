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
// AkRTPCMgr.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkRTPCMgr.h"
#include "AkSwitchCntr.h"
#include "AkDefault3DParams.h"
#include "AkMonitor.h"
#include "AkEvent.h"
#include "AkAudioLib.h"
#include "AkRegisteredObj.h"
#include "AkLayer.h"
#include "AkLayerCntr.h"
#include "AkBehavioralCtx.h"
#include "AkTransitionManager.h"
#include "IAkRTPCSubscriberPlugin.h"
#include "AkModulator.h"
#include "AkModulatorMgr.h"
#include "AkSwitchMgr.h"
#include "AkRTPCKey.h"

//AK_PROFILE_CREATE_DOMAIN( D, L"RTPC" );

inline AkHashType AkHash(CAkRegisteredObj *  in_obj) { return (AkHashType)(AkUIntPtr)in_obj; }

bool CurrentValue::GetModulatorValue( AkRtpcID in_RTPC_ID, AkRTPCKey& io_rtpcKey, AkReal32& out_value )
{
	return g_pModulatorMgr->GetCurrentModulatorValue( in_RTPC_ID, io_rtpcKey, out_value );
}

bool CurrentValue::ModulatorSupportsAutomatedParams(AkRtpcID in_RTPC_ID)
{
	return g_pModulatorMgr->SupportsAutomatedParams(in_RTPC_ID);
}

// CAkRTPCTransition

CAkRTPCMgr::CAkRTPCTransition::CAkRTPCTransition( AkRTPCEntry * in_pOwner, const AkRTPCKey& in_rtpcKey ) 
	: m_pTransition ( NULL )
	, m_pOwner( in_pOwner )
	, m_rtpcKey( in_rtpcKey )
	, m_bRemoveEntryWhenDone( false )
{
}

CAkRTPCMgr::CAkRTPCTransition::~CAkRTPCTransition()
{
	if ( m_pTransition )
		g_pTransitionManager->RemoveTransitionUser( m_pTransition, this );
}

void CAkRTPCMgr::CAkRTPCTransition::TransUpdateValue(AkIntPtr, AkReal32 in_unionValue, bool in_bIsTerminated)
{
	bool bRemoveEntry = m_bRemoveEntryWhenDone && in_bIsTerminated;
	// Note: We could have cached the value entry to avoid looking it up at each frame, but since they
	// are stored in a sorted key array, we can't...
	bool bCheckExceptions = false;
	AkRTPCValue *pValueEntry = NULL, *pParentValueEntry = NULL;
	pValueEntry = m_pOwner->FindExactValue( m_rtpcKey, &pParentValueEntry, &bCheckExceptions );
	m_pOwner->ApplyRTPCValue( pValueEntry, pParentValueEntry, in_unionValue, m_rtpcKey, bRemoveEntry, bCheckExceptions );
	
	if ( in_bIsTerminated )
	{
		// Clean up transition.
		m_pOwner->transitions.Remove( this );
		m_pTransition = NULL;
		AkDelete(AkMemID_Object, this );
	}
}

AKRESULT CAkRTPCMgr::CAkRTPCTransition::Start( 
	AkReal32 in_fStartValue,
	AkReal32 in_fTargetValue,
	TransParamsBase & in_transParams,
	bool in_bRemoveEntryWhenDone 
	)
{
	TransitionParameters params(
		this,
		TransTarget_RTPC,
		in_fStartValue,
		in_fTargetValue,
		in_transParams.TransitionTime,
		in_transParams.eFadeCurve,
		AkDelta_RTPC,
		false,
		false );
	if( m_pOwner->rampingType == AkTransitionRampingType_FilteringOverTime && !in_transParams.bBypassInternalValueInterpolation )
	{
		params.bUseFiltering = true;
		in_transParams.TransitionTime = (AkTimeMs)( (in_fStartValue > in_fTargetValue ? m_pOwner->fRampDown : m_pOwner->fRampUp)*1000 );
	}
	m_bRemoveEntryWhenDone = in_bRemoveEntryWhenDone;// Must be set _Before_ AddTransitionToList as AddTransitionToList can call update directly.

	m_pTransition = g_pTransitionManager->AddTransitionToList( params );

	return ( m_pTransition ) ? AK_Success : AK_Fail;
}

void CAkRTPCMgr::CAkRTPCTransition::Update(
	AkReal32 in_fNewTargetValue,
	TransParamsBase & in_transParams,
	bool in_bRemoveEntryWhenDone 
	)
{
	g_pTransitionManager->ChangeParameter(
		m_pTransition,
		TransTarget_RTPC,
		in_fNewTargetValue,
		in_transParams.TransitionTime,
		in_transParams.eFadeCurve,
		AkValueMeaning_Default	// Absolute
		);

	// Update the object's behavior at the outcome of the transition. Think about a transitioning reset during a transitionning set...
	m_bRemoveEntryWhenDone = in_bRemoveEntryWhenDone;
}

AkReal32 CAkRTPCMgr::CAkRTPCTransition::GetTargetValue()
{ 
	AKASSERT( m_pTransition ); 
	return m_pTransition->GetTargetValue(); 
}


// AkRTPCEntry

CAkRTPCMgr::AkRTPCEntry::~AkRTPCEntry()
{
	SCOPE_CHECK_TREE( values, AkRTPCValue );
	RemoveValue( AkRTPCKey() );
	values.Term();
	activeSubscriptions.Term();
    dormantSubscriptions.Term();
}

// Remove RTPC value for specified game object.
// Important: If in_pGameObj is NULL, ALL values are removed.
// IMPORTANT: Subscribers are not notified.
void CAkRTPCMgr::AkRTPCEntry::RemoveValue( const AkRTPCKey& in_key )
{
	if ( !in_key.AnyFieldValid() )
	{
		AkRTPCTransitions::IteratorEx it = transitions.BeginEx();
		while ( it != transitions.End() )
		{
			it = DestroyTransition( it );
		}
	}
	else
	{
		AkRTPCTransitions::IteratorEx it;
		FindTransition( in_key, it );
		if ( it != transitions.End() )
		{
			it = DestroyTransition( it );
		}
	}

	values.Unset( in_key );
	SCOPE_CHECK_TREE( values, AkRTPCValue );
}

void CAkRTPCMgr::AkRTPCEntry::RemoveMatchingValues( const AkRTPCKey& in_matchKey )
{
	if ( !in_matchKey.AnyFieldValid() ) //Remove all.
	{
		AkRTPCTransitions::IteratorEx it = transitions.BeginEx();
		while ( it != transitions.End() )
		{
			it = DestroyTransition( it );
		}
	}
	else //Remove matching transitions
	{
		AkRTPCTransitions::IteratorEx it;
		FindMatchingTransition( in_matchKey, it );
		while ( it != transitions.End() )
		{
			it = DestroyTransition( it );
			FindMatchingTransition( in_matchKey, it );
		}
	}

	values.RemoveAll( in_matchKey );
	SCOPE_CHECK_TREE( values, AkRTPCValue );
}


// Helpers.

// Returns valid iterator if found. Otherwise, iterator equals transitions.End(). 
void CAkRTPCMgr::AkRTPCEntry::FindTransition( const AkRTPCKey& in_rtpcKey, CAkRTPCMgr::AkRTPCTransitions::IteratorEx & out_iter )
{
    out_iter = transitions.BeginEx();
	while ( out_iter != transitions.End() )
	{
		if ( (*out_iter)->RTPCKey() == in_rtpcKey )
			return;
		++out_iter;
	}
}

// Returns valid iterator if found. Otherwise, iterator equals transitions.End(). 
void CAkRTPCMgr::AkRTPCEntry::FindMatchingTransition( const AkRTPCKey& in_rtpcKey, CAkRTPCMgr::AkRTPCTransitions::IteratorEx & out_iter )
{
	out_iter = transitions.BeginEx();
	while ( out_iter != transitions.End() )
	{
		if ( (*out_iter)->RTPCKey().MatchValidFields( in_rtpcKey ) )
			return;
		++out_iter;
	}
}

bool CAkRTPCMgr::AkRTPCEntry::DoesValueHaveTransition( const AkRTPCKey& in_rtpcKey )
{
	CAkRTPCMgr::AkRTPCTransitions::IteratorEx iter;
	FindTransition( in_rtpcKey, iter );
	return iter != transitions.End();
}

void CAkRTPCMgr::AkRTPCEntry::Move( AkRTPCSubscription* in_pSubscription, AkRTPCSubscriptions& from, AkRTPCSubscriptions& to )
{
	AKASSERT(from.Reserved() == to.Reserved());
	AKASSERT(from.Length() + to.Length() <= to.Reserved());

    // Arrays are pre-sized, no allocations will take place here.
    AkRTPCSubscriptions::Iterator itFound = from.FindEx(in_pSubscription);
    if (itFound != from.End())
    {
        from.Unset( in_pSubscription );
        AKASSERT( to.FindEx(in_pSubscription) == to.End() );
        AKVERIFY( to.Add( in_pSubscription ) != NULL );
    }
}

CAkRTPCMgr::CAkRTPCMgr()
{
}

CAkRTPCMgr::~CAkRTPCMgr()
{
}

AKRESULT CAkRTPCMgr::Init()
{
	if (!m_RTPCEntries.Init(AK_HASH_SIZE_VERY_SMALL) || !m_RTPCSubscribers.Init(AK_HASH_SIZE_VERY_SMALL))
		return AK_InsufficientMemory;

	SetMidiParameterDefaultValue(AssignableMidiControl_PitchBend, AK_MIDI_PITCH_BEND_DEFAULT);

	return AK_Success;
}

AKRESULT CAkRTPCMgr::Term()
{
	for( AkMapRTPCEntries::IteratorEx it = m_RTPCEntries.BeginEx(); it != m_RTPCEntries.End(); )
	{
		AkRTPCEntry * pEntry = *it;
		it = m_RTPCEntries.Erase( it );

		AkDelete(AkMemID_Object, pEntry );
	}

	for( AkMapRTPCSubscribers::IteratorEx it = m_RTPCSubscribers.BeginEx(); it != m_RTPCSubscribers.End(); )
	{
		AkRTPCSubscription * pSubscription = *it;
		it = m_RTPCSubscribers.Erase( it );

		RemoveReferencesToSubscription( pSubscription );
		AkDelete(AkMemID_Object, pSubscription );
	}

	m_RTPCEntries.Term();
	m_RTPCSubscribers.Term();

	for ( AkUInt32 i=0; i<BuiltInParam_Max; ++i)
	{
		m_BuiltInParamBindings[i].Term();
	}

	return AK_Success;
}

AKRESULT CAkRTPCMgr::RegisterLayer( CAkLayer* in_pLayer, AkRtpcID in_rtpcID, AkRtpcType in_rtpcType )
{
	return SubscribeRTPC(in_pLayer, in_rtpcID, in_rtpcType, AkRtpcAccum_Exclusive, RTPC_MaxNumRTPC, 0, AkCurveScaling_None, NULL, 0, AkRTPCKey(), SubscriberType_CAkCrossfadingLayer);
}

void CAkRTPCMgr::UnregisterLayer( CAkLayer* in_pLayer, AkRtpcID in_rtpcID )
{
	UnSubscribeRTPC(in_pLayer, RTPC_MaxNumRTPC);
}

AKRESULT CAkRTPCMgr::RegisterSwitchGroup( void* in_pSubscriber, AkRtpcID in_rtpcID, AkRtpcType in_rtpcType, AkRTPCGraphPoint* in_pGraphPts, AkUInt32 in_numGraphPts )
{
	return SubscribeRTPC( in_pSubscriber, in_rtpcID, in_rtpcType, AkRtpcAccum_Exclusive, RTPC_MaxNumRTPC, 0, AkCurveScaling_None, in_pGraphPts, in_numGraphPts, AkRTPCKey(), SubscriberType_SwitchGroup );
}

void CAkRTPCMgr::UnregisterSwitchGroup( void* in_pSubscriber )
{
	UnSubscribeRTPC( in_pSubscriber, RTPC_MaxNumRTPC );
}

AKRESULT CAkRTPCMgr::SetRTPCInternal( 
	AkRtpcID in_RTPCid, 
	AkReal32 in_Value, 
	CAkRegisteredObj * pGameObj,
	AkPlayingID in_playingID,
	TransParamsBase & in_transParams,
	AkValueMeaning in_eValueMeaning		// AkValueMeaning_Independent (absolute) or AkValueMeaning_Offset (relative)
	)
{
	AkRTPCKey rtpcKey;
	rtpcKey.GameObj() = pGameObj;
	rtpcKey.PlayingID() = in_playingID;

	return SetRTPCInternal(
			in_RTPCid, 
			in_Value, 
			rtpcKey,
			in_transParams,
			in_eValueMeaning	);
}


AKRESULT CAkRTPCMgr::SetRTPCInternal(AkRtpcID in_RTPCid,
										AkReal32 in_Value, 
										const AkRTPCKey& in_rtpcKey,
										TransParamsBase & in_transParams,
										AkValueMeaning in_eValueMeaning,		// AkValueMeaning_Independent (absolute) or AkValueMeaning_Offset (relative)
										bool in_bBypassInternalRampingIfFirst   // For Built-in WG-29455 BuiltIn must not interpolate on initial playback.
									 )
{
	AkRTPCValue *pOldValue = NULL, *pParentValue = NULL;
	bool bNeedExceptions = false;

	AkRTPCEntry * pEntry = m_RTPCEntries.Exists( in_RTPCid );
	if ( pEntry )
	{
		pOldValue = pEntry->FindExactValue( in_rtpcKey, &pParentValue, &bNeedExceptions );
		if (pOldValue == NULL && in_bBypassInternalRampingIfFirst)
		{
			// For Built-in WG-29455 BuiltIn must not interpolate on initial playback.
			// if there was no exact matching previous value, do not ramp from default.
			in_transParams.bBypassInternalValueInterpolation = true;
		}
	}
	else
	{
		pOldValue = NULL;

		pEntry = AkNew(AkMemID_Object, AkRTPCEntry(in_RTPCid));
		if ( !pEntry )
			return AK_Fail;

		m_RTPCEntries.Set( pEntry );
		pEntry->FindExactValue( in_rtpcKey, &pParentValue, &bNeedExceptions );
	}

	if ( in_eValueMeaning == AkValueMeaning_Offset )
	{
		// Relative change: compute new absolute value based on current _target_ value
		// (that is, the transition's target value if applicable, the current value otherwise).
		in_Value += pEntry->GetCurrentTargetValue( pOldValue, pParentValue, in_rtpcKey );
	}

	return pEntry->SetRTPC( pOldValue, pParentValue, in_Value, in_rtpcKey, in_transParams, false, bNeedExceptions );
}

AKRESULT CAkRTPCMgr::AkRTPCEntry::SetRTPC(
	AkRTPCValue * in_pValueEntry, // Pass in the value entry of the game object map. MUST be == ValueExists( pGameObj ). NULL if there is none.
	AkRTPCValue * in_pParentValueEntry, // a fallback value at a wider scope
	AkReal32 in_fNewValue, 
	const AkRTPCKey& in_ValKey,
	TransParamsBase & in_transParams,
	bool in_bUnsetWhenDone,		// Remove game object's specific RTPC Value once value is set.
	bool in_bCheckExceptions
	)
{
	SCOPE_CHECK_TREE( values, AkRTPCValue );

	AKASSERT( in_pValueEntry == FindExactValue( in_ValKey, NULL, NULL ) );

	bool bRequiresUpdate = ( !in_pValueEntry || in_pValueEntry->fValue != in_fNewValue );

	AkReal32 fStartValue = 0.f;
	if( bRequiresUpdate )
	{
		fStartValue = GetCurrentValue( in_pValueEntry, in_pParentValueEntry );
	}

	if( bRequiresUpdate && rampingType != AkTransitionRampingType_None && !(in_transParams.bBypassInternalValueInterpolation) )
	{
		AkTimeMs minTransitionTime = 0;

		if( rampingType == AkTransitionRampingType_SlewRate )//1 is currently slew rate, use enum plz...
		{
			//fRampUp and fRampDown is in units per sec while TransitionTime is in AkTimeMs.
			if( fStartValue < in_fNewValue )
			{
				if( fRampUp != 0 )
					minTransitionTime = (AkTimeMs)(((in_fNewValue - fStartValue)/fRampUp)*1000.f);
			}
			else //should not be == as bRequiresUpdate is false.
			{
				if( fRampDown != 0 )
					minTransitionTime = (AkTimeMs)(((fStartValue - in_fNewValue)/fRampDown)*1000.f);
			}
		}
		else if( rampingType == AkTransitionRampingType_FilteringOverTime )
		{
			//fRampUp and fRampDown is in seconds while TransitionTime is in AkTimeMs.
			if( fStartValue < in_fNewValue )
				minTransitionTime = (AkTimeMs)(fRampUp*1000.f);
			else //should not be == as bRequiresUpdate is false.
				minTransitionTime = (AkTimeMs)(fRampDown*1000.f);
		}
		in_transParams.TransitionTime = AkMax( in_transParams.TransitionTime, minTransitionTime );
	}

	if ( in_transParams.TransitionTime > 0 && bRequiresUpdate )
	{
		AKRESULT res = AK_Success;

		// Track the existence of this transition.  The value entry does not exist yet,	but it will get created on the next update of the transition manager.   
		// We need to flag this transition for removal, in case the scoped object (eg game obj) is deleted before then.
		if (!in_pValueEntry)
			res = CAkScopedRtpcObj::AddedNewRtpcValue(key, in_ValKey);

		// Requires a transition. Create or change transition target.

		// Create or modify transition. If a transition exists after returning from this function
		// (returns true), bail out: RTPC updates will be executed from the transition handler. 
		// If a transition does not exist, for any reason, proceed. We might want to set the value 
		// immediately, notify subscribers and/or unset the entry.
		if ( res == AK_Success && CreateOrModifyTransition( in_ValKey, fStartValue, in_fNewValue, in_transParams, in_bUnsetWhenDone ) )
			return AK_Success;
	}
	else
	{
		// Does not require a transition, or no update is required. Destroy current transition if applicable.
		AkRTPCTransitions::IteratorEx itTrans;
		FindTransition( in_ValKey, itTrans );
		if ( itTrans != transitions.End() )
			DestroyTransition( itTrans );
	}

	if ( !bRequiresUpdate && !in_bUnsetWhenDone )
		return AK_Success;

	// If we did not bail out before, the RTPC value must be applied now. Otherwise, the transition handler will do it.
	// Also, pass in_bUnsetWhenDone directly so that the RTPC value gets unset at once.	
	AkDeltaMonitor::OpenUpdateBrace(in_ValKey.GameObj() == NULL ? AkDelta_RTPCGlobal : AkDelta_RTPC);
	ApplyRTPCValue( in_pValueEntry, in_pParentValueEntry, in_fNewValue, in_ValKey, in_bUnsetWhenDone, in_bCheckExceptions );
	AkDeltaMonitor::CloseUpdateBrace();
	return AK_Success;
}

// Returns the current target value if there is a transition, or the current value otherwise.
// Argument in_pValueEntry can be NULL: means that there is no specific value currently 
// assigned to the desired game object.
AkReal32 CAkRTPCMgr::AkRTPCEntry::GetCurrentTargetValue( AkRTPCValue * in_pValueEntry, AkRTPCValue * in_pParentValueEntry, const AkRTPCKey& in_rtpcKey )
{
	if ( in_pValueEntry )
	{
		// An entry exists for this game object (which can be the global NULL game object).
		AkRTPCTransitions::IteratorEx itTrans;
		FindTransition( in_rtpcKey.GameObj(), itTrans );
		if ( itTrans != transitions.End() )
			return (*itTrans)->GetTargetValue();
	}
	// No transition found on this value entry. Return current (or default) value.
	return GetCurrentValue( in_pValueEntry, in_pParentValueEntry );
}

// Returns the default value if current value does not exist.
AkReal32 CAkRTPCMgr::AkRTPCEntry::GetCurrentValue( AkRTPCValue * in_pValueEntry, AkRTPCValue * in_pParentValueEntry  )
{
	return in_pValueEntry != NULL ? in_pValueEntry->fValue 
				: ( in_pParentValueEntry != NULL ? in_pParentValueEntry->fValue 
						: fDefaultValue );
}

AKRESULT CAkRTPCMgr::AkRTPCEntry::AddSubscription(AkRTPCSubscription* pSubscription, bool in_bIsActive)
{
	AKRESULT eResult = AK_Success;	
	if(!dormantSubscriptions.Exists(pSubscription) && !activeSubscriptions.Exists(pSubscription))
	{
		// Make sure enough memory is allocated in both arrays.  We make each arrays contain exactly enough room for all the subscriptions because a subscription can only exist in one of the two arrays.
		AkUInt32 uSumLength = activeSubscriptions.Length() + dormantSubscriptions.Length() + 1;
		if ((activeSubscriptions.Reserved() >= uSumLength || activeSubscriptions.GrowArray( (2*uSumLength) - activeSubscriptions.Reserved())) &&
			(dormantSubscriptions.Reserved() >= uSumLength || dormantSubscriptions.GrowArray( (2*uSumLength) - dormantSubscriptions.Reserved())))
		{
			AkRTPCSubscriptions& addToArray = (in_bIsActive ? activeSubscriptions : dormantSubscriptions);
			AKVERIFY(addToArray.Add(pSubscription) != NULL);
			AKASSERT(activeSubscriptions.Reserved() == dormantSubscriptions.Reserved());
			AKASSERT(activeSubscriptions.Length() + dormantSubscriptions.Length() <= dormantSubscriptions.Reserved());
			eResult = AK_Success;
		}
		else
		{
			eResult = AK_InsufficientMemory;
		}
	}

	return eResult;
}

//Remove the reference to the subscription from this AkRTPCEntry's map, *only if* no additional curves 
//	remain that reference this RTPC Id.. 
bool CAkRTPCMgr::AkRTPCEntry::RemoveSubscriptionIfNoCurvesRemain(AkRTPCSubscription* in_pSubscription)
{
	for (RTPCCurveArray::Iterator iterCurves = in_pSubscription->Curves.Begin(); iterCurves != in_pSubscription->Curves.End(); ++iterCurves)
	{
		if (iterCurves.pItem->RTPC_ID == this->key)
		{
			// Bail out! There is an existing curve that references this subscription.
			return false;
		}
	}
	activeSubscriptions.Unset(in_pSubscription);
	dormantSubscriptions.Unset(in_pSubscription);
	return true;
}

void CAkRTPCMgr::AkRTPCEntry::RemoveSubscription(AkRTPCSubscription* in_pSubscription)
{
	activeSubscriptions.Unset(in_pSubscription);
	dormantSubscriptions.Unset(in_pSubscription);
}

// Creates or modifies a game parameter transition. 
// Returns false if creation fails, or if no transition is required whatsoever (transition is 
// cleaned up inside). In such a case, the caller should apply the RTPC value immediately. 
// Otherwise, RTPC updates are handled by the transition.
bool CAkRTPCMgr::AkRTPCEntry::CreateOrModifyTransition(
	const AkRTPCKey& in_rtpcKey, 
	AkReal32 in_fStartValue,
	AkReal32 in_fTargetValue,
	TransParamsBase & in_transParams,
	bool in_bRemoveEntryWhenDone
	)
{
	// Do not use a transition if target value equals start value. If one exists, destroy it.
	bool bRequiresTransition = ( in_fStartValue != in_fTargetValue );

	AkRTPCTransitions::IteratorEx itTrans;
	FindTransition( in_rtpcKey, itTrans );
	if ( itTrans != transitions.End() )
	{
		// Transition exists. 
		if ( bRequiresTransition )
		{
			// A transition is required. Modify it.
			(*itTrans)->Update( in_fTargetValue, in_transParams, in_bRemoveEntryWhenDone );
			return true;
		}
		else
		{
			// Transition is not required. Destroy it.
			DestroyTransition( itTrans );
		}			
	}
	else
	{
		// Transition does not exist. Create one if required.
		if ( bRequiresTransition )
		{
			// Create transition. Store in_bRemoveEntryWhenDone: the handler will unset the value when target is reached.
			CAkRTPCTransition * pTrRTPC = AkNew(AkMemID_Object, CAkRTPCTransition( this, in_rtpcKey ) );
			if ( pTrRTPC )
			{
				if ( pTrRTPC->Start( 
						in_fStartValue,
						in_fTargetValue,
						in_transParams,
						in_bRemoveEntryWhenDone ) == AK_Success )
				{
					// Successfully created transition. RTPC updates will occur from the transition handler.
					transitions.AddFirst( pTrRTPC );
					return true;
				}
				else
				{
					// Calling Start which returned failure DID delete the CAkRTPCTransition "pTrRTPC" object already.
					// Accessing pTrRTPC is therefore illegal ( and unnecessary ).
				}
			}
		}
	}

	// Transition was not be created.
	return false;
}

// Notifies the subscribers and manages the map of values.
void CAkRTPCMgr::AkRTPCEntry::ApplyRTPCValue( 
	AkRTPCValue * in_pValueEntry, 
	AkRTPCValue * in_pParentValueEntry,
	AkReal32 in_NewValue, 
	const AkRTPCKey& in_ValKey,
	bool in_bUnsetValue,		// If true, the game object's specific RTPC entry is removed instead of being stored.
	bool in_bCheckExceptions
	)
{
	AkReal32 fCurrentVal = GetCurrentValue(in_pValueEntry, in_pParentValueEntry);

	if (in_pValueEntry == NULL && !in_bUnsetValue)
	{
		if (CAkScopedRtpcObj::AddedNewRtpcValue(key, in_ValKey) == AK_Success)
		{
			in_pValueEntry = values.Set(in_ValKey);
			SCOPE_CHECK_TREE(values, AkRTPCValue);
		}
	}

	if (in_pValueEntry != NULL)
	{
		// Push the update to the listeners and store it. 
		// Only push the update if we are able to save it in a in_pValueEntry.

		if (in_bUnsetValue)
		{
			// Unset value (reset)
			values.Unset(in_ValKey);
			SCOPE_CHECK_TREE(values, AkRTPCValue);
		}
		else
		{
			in_pValueEntry->fValue = in_NewValue;
		}

		if ( fCurrentVal != in_NewValue )
			NotifyRTPCChange(fCurrentVal, in_NewValue, in_ValKey, in_bCheckExceptions);
	}
}

void CAkRTPCMgr::AkRTPCEntry::NotifyRTPCChange( 
	AkReal32 in_OldValue, 
	AkReal32 in_NewValue, 
	const AkRTPCKey& in_ValKey,
	bool in_bCheckExceptions )
{
	AkRtpcID idRTPC = key;

	AkRTPCExceptionChecker_<AkRTPCValueTree> exChcker( in_ValKey, values );
	AkRTPCExceptionChecker* pExceptionsForSubsc = NULL;
	if ( in_bCheckExceptions ) 
		pExceptionsForSubsc = &exChcker;
	
	AkUInt32 uNumActive = activeSubscriptions.Length();
	if (uNumActive > 0)
	{
		// Copy the active subscriptions to a temp array on the stack.  
		// PushUpdate can call into functions that play sounds (eg. continuous switch containers driven by RTPC)
		//	and they may activate subscriptions, thereby modifying the activeSubscriptions list.

		size_t uAllocSize = uNumActive * sizeof(AkRTPCSubscription*);
		AkRTPCSubscription** pSubs = (AkRTPCSubscription**)alloca(uAllocSize);
		memcpy(pSubs, &activeSubscriptions[0], uAllocSize);

		AkDeltaMonitor::LogDriver(idRTPC, in_NewValue);

		for (AkRTPCSubscription** pEnd = pSubs + uNumActive; pSubs != pEnd; ++pSubs)
		{
			(*pSubs)->PushUpdate(idRTPC, in_OldValue, in_NewValue, in_ValKey, pExceptionsForSubsc, &values);
		}		
	}
}

// Evaluate the subscription's curves that are based on idRTPC, add their values
AkReal32 CAkRTPCMgr::AkRTPCSubscription::ConvertCurves(AkRtpcID idRTPC, AkReal32 in_NewValue) const
{
	AkReal32 l_value = 0.f;

	for( RTPCCurveArray::Iterator iterCurves = Curves.Begin(); iterCurves != Curves.End(); ++iterCurves )
	{
		if( iterCurves.pItem->RTPC_ID == idRTPC )
		{
			l_value += iterCurves.pItem->ConversionTable.Convert( in_NewValue );
		}
	}

	return l_value;
}

void CAkRTPCMgr::AkRTPCSubscription::GetParamValue(
	AkRtpcID in_rtpcId,
	AkReal32 in_fRtpc,
	AkReal32& out_fValue ) const
{
	if ( AK_EXPECT_FALSE( eAccum == AkRtpcAccum_Multiply ) )
		_GetParamValue<AccumulateMult>(in_rtpcId, in_fRtpc, out_fValue );
	else
		_GetParamValue<AccumulateAdd>(in_rtpcId, in_fRtpc, out_fValue );
}

void CAkRTPCMgr::AkRTPCSubscription::GetParamValues(
	AkRtpcID in_rtpcId,
	AkReal32 in_newRtpc,
	AkReal32 in_oldRtpc,
	AkReal32& out_newValue,
	AkReal32& out_oldValue ) const
{
	if ( AK_EXPECT_FALSE( eAccum == AkRtpcAccum_Multiply ) )
		_GetParamValues<AccumulateMult>(in_rtpcId, in_newRtpc, in_oldRtpc, out_newValue, out_oldValue );
	else
		_GetParamValues<AccumulateAdd>(in_rtpcId, in_newRtpc, in_oldRtpc, out_newValue, out_oldValue );
}

void CAkRTPCMgr::AkRTPCSubscription::PushUpdate(
					AkRtpcID idRTPC, 
					AkReal32 in_fOldValue,
					AkReal32 in_fNewValue, 
					const AkRTPCKey& in_rtpcKey,
					AkRTPCExceptionChecker* in_exChecker, 
					AkRTPCValueTree* values ) const
{
    AKASSERT( key.pSubscriber );
	if (!key.pSubscriber)
		return; // WG-30323 Crash in AkRTPCSubscription(10 Error reports).( We did another fix but we never had a repro, so we padded this crash.)	

    if( eType == SubscriberType_CAkParameterNodeBase )
    {
        AKASSERT( key.pSubscriber );
        AkReal32 l_value, l_oldValue;

        CAkRTPCSubscriberNode* pTgt = reinterpret_cast<CAkRTPCSubscriberNode*>(key.pSubscriber);

        AKASSERT ( pTgt->HasActiveTargets() ); //Should not be in the active list, if there are no active targets.
        
		GetParamValues( idRTPC, in_fNewValue, in_fOldValue, l_value, l_oldValue );

		// Update even if l_value == l_oldValue to correctly monitor RTPCs
		AkDeltaMonitorObjBrace deltaBrace(static_cast<CAkParameterNodeBase*>(pTgt)->key);
		pTgt->PushParamUpdate( key.ParamID, in_rtpcKey, l_value, l_value - l_oldValue, in_exChecker  );
    }
    else if ( eType == SubscriberType_Plugin )
    {
        if(	TargetKey.MatchValidFields(in_rtpcKey) && 
            ( in_exChecker == NULL || !in_exChecker->IsException(TargetKey)  ) )
        {
			AKASSERT( key.pSubscriber );

			AkReal32 oldValue, newValue;
			GetParamValues( idRTPC, in_fNewValue, in_fOldValue, newValue, oldValue );

			// Update even if oldValue == newValue to correctly monitor RTPCs
			IAkRTPCSubscriberPlugin* pSubscriber = reinterpret_cast<IAkRTPCSubscriberPlugin*>(key.pSubscriber);
#ifndef AK_OPTIMIZED
			AkDeltaMonitorObjBrace deltaBrace(pSubscriber->GetContextID());
#endif
			pSubscriber->UpdateTargetRtpcParam(key.ParamID, idRTPC, eAccum, newValue);
        }
    }
    else if( eType == SubscriberType_PBI )
    {
        if(	TargetKey.MatchValidFields(in_rtpcKey) && 
            ( in_exChecker == NULL || !in_exChecker->IsException(TargetKey)  ) )
        {
            AkReal32 l_value = ConvertCurves(idRTPC,in_fNewValue);
            // Notify the registered user of the change
            static_cast<CAkBehavioralCtx*>( key.pSubscriber )->SetParam( static_cast<AkPluginParamID>(key.ParamID), &l_value, sizeof(l_value) );
        }
    }
    else if ( eType == SubscriberType_Modulator )
    {
        AkReal32 l_value = ConvertCurves(idRTPC,in_fNewValue);
        reinterpret_cast<CAkModulator*>( key.pSubscriber )->SetParamFromRTPCMgr( 
            key.ParamID, l_value, in_rtpcKey );
    }
    else if ( eType == SubscriberType_SwitchGroup )
    {
        AKASSERT( Curves.Length() == 1 && Curves.Begin().pItem->RTPC_ID == idRTPC );
        CAkConversionTable& convTable = Curves.Begin().pItem->ConversionTable;
        AkReal32 newValue = convTable.Convert( in_fNewValue );
        AkReal32 oldValue = convTable.Convert( in_fOldValue );
		
		// Update even if oldValue == newValue to correctly monitor RTPCs

		//ANTI Delta brace.  
		//While it is worthwhile to have all RTPC changes in one packet, when a Switch container is affected, sounds may start.  
		//So Post what we have and let the normal sound-startup parameter collection go.
		AkDeltaMonitor::CloseMultiTargetBrace();
		g_pSwitchMgr->SetSwitchFromRTPCMgr(key.pSubscriber, in_rtpcKey, newValue, in_exChecker);
		AkDeltaMonitor::OpenMultiTargetBrace(in_rtpcKey.GameObj() == NULL ? AkDelta_RTPCGlobal : AkDelta_RTPC, idRTPC, in_fNewValue);
    }
    else
    {
		AKASSERT(eType == SubscriberType_CAkLayer || eType == SubscriberType_CAkCrossfadingLayer);

        CAkLayer* pLayer = reinterpret_cast<CAkLayer*>(key.pSubscriber);
        const CAkLayerCntr * pOwner = pLayer->GetOwner();
		if (pOwner)
		{
			if (AK_EXPECT_FALSE(pOwner->IsPlaying() || (pOwner->IsActive() && pOwner->IsContinuousValidation())))
			{
				AkDeltaMonitorObjBrace deltaBrace(pLayer->key);
				if (key.ParamID == RTPC_MaxNumRTPC)	//Used for the crossfade RTPC of Blend containers
				{
					pLayer->OnRTPCChanged(in_rtpcKey, in_fOldValue, in_fNewValue, in_exChecker);					
				}
				else if (pLayer->IsPlaying())
				{
					AkReal32 newValue = ConvertCurves(idRTPC, in_fNewValue);
					AkReal32 oldValue = ConvertCurves(idRTPC, in_fOldValue);

					// Update even if oldValue == newValue to correctly monitor RTPCs
					pLayer->SetParamComplexFromRTPCManager(
						(void *) this,
						key.ParamID,
						idRTPC,
						newValue,
						oldValue,
						in_rtpcKey,
						in_exChecker
						);
				}
			}
		}
    }	
}

void CAkRTPCMgr::RemoveReferencesToSubscription( AkRTPCSubscription * in_pSubscription )
{
	bool bFound = false;
	for( RTPCCurveArray::Iterator it = in_pSubscription->Curves.Begin(); it != in_pSubscription->Curves.End(); ++it )
	{
		AkRTPCEntry * pEntry = m_RTPCEntries.Exists( (*it).RTPC_ID );
		if ( pEntry )
		{
            pEntry->RemoveSubscription( in_pSubscription );
			bFound = true;
		}

		(*it).ConversionTable.Unset();
	}
	in_pSubscription->Curves.RemoveAll();

	if (!bFound)
	{
		//The RTPC doesn't have any curves.  Search in the whole lot even if it's longer.
		AkMapRTPCEntries::Iterator it = m_RTPCEntries.Begin();
		for(; it != m_RTPCEntries.End(); ++it)
		{
			it.pItem->RemoveSubscription(in_pSubscription);
		}
	}
	
	g_pModulatorMgr->RemoveSubscription( in_pSubscription );
}

// Called for RTPCs that only have one curve (e.g. switch groups)
AkReal32 CAkRTPCMgr::GetRTPCConvertedValue(
	void*					in_pSubscriber,
	const AkRTPCKey&		in_rtpcKey
	)
{
	AkRTPCSubscriptionKey key;
	key.pSubscriber = in_pSubscriber;
	key.ParamID = RTPC_MaxNumRTPC;

	AkRTPCSubscription* pSubscription = m_RTPCSubscribers.Exists( key );
	AKASSERT( ! pSubscription || pSubscription->Curves.Length() == 1 );
	if ( pSubscription && pSubscription->Curves.Length() != 0 )
	{
		AKASSERT( pSubscription->eAccum == AkRtpcAccum_Exclusive );

		AkReal32 rtpcValue;
		bool bIsAutomatedParam;
		AkRTPCKey localKey( in_rtpcKey );

		RTPCCurve* pCurve = pSubscription->Curves.Begin().pItem;
		if (AK_EXPECT_FALSE(!GetRTPCValue<CurrentValue>( pCurve->RTPC_ID, pSubscription->key.ParamID, pSubscription->eType, localKey, rtpcValue, bIsAutomatedParam )))
			rtpcValue = GetDefaultValue( pCurve->RTPC_ID );

		AKASSERT( !bIsAutomatedParam );
		if (AK_EXPECT_FALSE(!bIsAutomatedParam))
			return pCurve->ConversionTable.Convert( rtpcValue );
	}

	return DEFAULT_RTPC_VALUE;
}

///////////////////////////////////////////////////////////////////////////
// Subscription Functions
///////////////////////////////////////////////////////////////////////////

AKRESULT CAkRTPCMgr::SubscribeRTPC(
		void*						in_pSubscriber,
		AkRtpcID					in_RTPC_ID,
		AkRtpcType					in_RTPCType,
		AkRtpcAccum					in_RTPCAccum,
		AkRTPC_ParameterID			in_ParamID,		//# of the param that must be notified on change
		AkUniqueID					in_RTPCCurveID,		
		AkCurveScaling				in_eScaling,
		AkRTPCGraphPoint*			in_pArrayConversion,
		AkUInt32					in_ulConversionArraySize,
		const AkRTPCKey&			in_rtpcKey,
		SubscriberType				in_eType,
        bool                        in_isActive
		)
{
	AKASSERT(in_pSubscriber);

	//Suppose parameters are wrongs
	AKRESULT eResult = AK_InvalidParameter;

	if(in_pSubscriber)
	{
		AkRTPCSubscriptionKey key;
		key.pSubscriber = in_pSubscriber;
		key.ParamID = in_ParamID;

		AkRTPCSubscription* pSubscription = m_RTPCSubscribers.Exists( key );
		if ( pSubscription )
		{
			AkRtpcID toReplaceRtpcId = AK_INVALID_RTPC_ID;

			// Remove existing entry for the specified curve
			for( RTPCCurveArray::Iterator iterCurves = pSubscription->Curves.Begin(); iterCurves != pSubscription->Curves.End(); ++iterCurves )
			{
				if( iterCurves.pItem->RTPCCurveID == in_RTPCCurveID )
				{
					toReplaceRtpcId = iterCurves.pItem->RTPC_ID;
					
					//Erase the curve before removing subscription references.
					iterCurves.pItem->ConversionTable.Unset();
					pSubscription->Curves.Erase( iterCurves );
					
					AkRTPCEntry * pEntry = m_RTPCEntries.Exists(toReplaceRtpcId);
					if (pEntry)
					{
						//Unset the subscription if there are no more curves with the same RTPC Id.
						pEntry->RemoveSubscriptionIfNoCurvesRemain(pSubscription);
					}

					break;
				}
			}

			// If the RTPC is replacing a modulator, remove the subscription reference in the modulator mgr.
			if (toReplaceRtpcId != AK_INVALID_RTPC_ID)
				g_pModulatorMgr->RemoveSubscription( pSubscription, toReplaceRtpcId );
		}
		else
		{
			// Create a new subscription
			pSubscription = AkNew(AkMemID_Object, AkRTPCSubscription());
			if (!pSubscription)
			{
				eResult = AK_InsufficientMemory;
			}
			else
			{
				pSubscription->key.pSubscriber = in_pSubscriber;
				pSubscription->key.ParamID = in_ParamID;
				pSubscription->eType = in_eType;
				pSubscription->eAccum = in_RTPCAccum;
				pSubscription->TargetKey = in_rtpcKey;
				m_RTPCSubscribers.Set( pSubscription );
			}
		}

		if ( pSubscription )
		{
			if( in_pArrayConversion && in_ulConversionArraySize )
			{
				RTPCCurve* pNewCurve = pSubscription->Curves.AddLast();
				if ( ! pNewCurve )
				{
					eResult = AK_InsufficientMemory;
				}
				else
				{
					pNewCurve->RTPC_ID = in_RTPC_ID;
					pNewCurve->RTPCCurveID = in_RTPCCurveID;
					eResult = pNewCurve->ConversionTable.Set( in_pArrayConversion, in_ulConversionArraySize, in_eScaling );
					if ( eResult != AK_Success )
						pSubscription->Curves.RemoveLast();
				}
			}
			else if (in_eType == SubscriberType_CAkCrossfadingLayer && in_ParamID == RTPC_MaxNumRTPC)	//This case is for the RTPC controlling the crossfade in blends.
				eResult = AK_Success;

			if ( eResult == AK_Success )
			{
				if ( in_RTPCType == AkRtpcType_Modulator )
				{
					eResult = g_pModulatorMgr->AddSubscription( in_RTPC_ID, pSubscription );
				}
				else
				{
					AkRTPCEntry * pEntry = GetRTPCEntry( in_RTPC_ID );
					if ( pEntry )
						eResult = pEntry->AddSubscription(pSubscription, in_isActive);
					else
						eResult = AK_InsufficientMemory;
				}
			}
		}

		if( eResult == AK_Success )
		{
			UpdateSubscription(*pSubscription, in_RTPC_ID );
		}
		else if ( pSubscription && pSubscription->Curves.IsEmpty() )
		{
			m_RTPCSubscribers.Unset( key );
			RemoveReferencesToSubscription( pSubscription );
			AkDelete(AkMemID_Object, pSubscription );
		}
	}

	return eResult;
}

CAkRTPCMgr::AkRTPCSubscription* CAkRTPCMgr::GetSubscription(
	const void*					in_pSubscriber,
	AkUInt32				in_ParamID
	)
{
	AkRTPCSubscriptionKey key;
	key.pSubscriber = const_cast<void*>(in_pSubscriber);
	key.ParamID = (AkRTPC_ParameterID) in_ParamID;

	return m_RTPCSubscribers.Exists( key );
}


void CAkRTPCMgr::UnSubscribeRTPC( void* in_pSubscriber, AkUInt32 in_ParamID, AkUniqueID in_RTPCCurveID, bool* out_bMoreCurvesRemaining /* = NULL */ )
{
	AkRTPCSubscriptionKey key;
	key.pSubscriber = in_pSubscriber;
	key.ParamID = (AkRTPC_ParameterID) in_ParamID;

	AkRTPCSubscription* pSubscription = m_RTPCSubscribers.Exists( key );
	if ( pSubscription )
	{
		if ( out_bMoreCurvesRemaining )
			*out_bMoreCurvesRemaining = !pSubscription->Curves.IsEmpty();

		for( RTPCCurveArray::Iterator iterCurves = pSubscription->Curves.Begin(); iterCurves != pSubscription->Curves.End(); ++iterCurves )
		{
			if( iterCurves.pItem->RTPCCurveID == in_RTPCCurveID )
			{
				//Save the rtpc id to pass to modulator mgr
				AkRtpcID rtpcIdToRemove = iterCurves.pItem->RTPC_ID;
				
				//Remove curve from subscription before removing references to subscriptions (in CAkModulator and AkRTPCEntry)
				iterCurves.pItem->ConversionTable.Unset();
				pSubscription->Curves.Erase( iterCurves );

				//Find the related RTPC entry; remove the subscription reference if there are no other curves with the same RTPC ID.
				AkRTPCEntry * pEntry = m_RTPCEntries.Exists(rtpcIdToRemove);
				if (pEntry)
					pEntry->RemoveSubscriptionIfNoCurvesRemain(pSubscription);

				g_pModulatorMgr->RemoveSubscription(pSubscription, rtpcIdToRemove);

				if ( pSubscription->Curves.IsEmpty() )
				{
					if ( out_bMoreCurvesRemaining )
						*out_bMoreCurvesRemaining = false;

					m_RTPCSubscribers.Unset( key );
					// Note: No need to remove references to subscription. We just did. 
					AkDelete(AkMemID_Object, pSubscription );
				}

				break;
			}
		}
		
		// Found the subscription but not the curve
		return;
	}

	if ( out_bMoreCurvesRemaining )
		*out_bMoreCurvesRemaining = false;
}

void CAkRTPCMgr::ActivateSubscription(const void* in_pSubscriber, AkRTPCBitArray in_paramSet)
{
    for ( AkUInt32 iBit = 0; !in_paramSet.IsEmpty(); ++iBit )
    {
        AkRTPCSubscriptionKey subsKey;
        subsKey.pSubscriber = (void*)in_pSubscriber;
        if ( in_paramSet.IsSet( iBit ) )
        {
            subsKey.ParamID = (AkRTPC_ParameterID)iBit;
            AkRTPCSubscription* pSubscription = m_RTPCSubscribers.Exists( subsKey );
            if (pSubscription)
            {
                for( RTPCCurveArray::Iterator iterCurves = pSubscription->Curves.Begin(); iterCurves != pSubscription->Curves.End(); ++iterCurves )
                {
                    AkRTPCEntry* pEntry = m_RTPCEntries.Exists( iterCurves.pItem->RTPC_ID );
					if (pEntry)
						pEntry->ActivateSubscription( pSubscription );
                }
            }

            in_paramSet.UnsetBit( iBit );
        }
    }
}

void CAkRTPCMgr::DeactivateSubscription(const void* in_pSubscriber, AkRTPCBitArray in_paramSet)
{
    for ( AkUInt32 iBit = 0; !in_paramSet.IsEmpty(); ++iBit )
    {
        AkRTPCSubscriptionKey subsKey;
        subsKey.pSubscriber = (void*)in_pSubscriber;
        if ( in_paramSet.IsSet( iBit ) )
        {
            subsKey.ParamID = (AkRTPC_ParameterID)iBit;
            AkRTPCSubscription* pSubscription = m_RTPCSubscribers.Exists( subsKey );
            if (pSubscription)
            {
                for( RTPCCurveArray::Iterator iterCurves = pSubscription->Curves.Begin(); iterCurves != pSubscription->Curves.End(); ++iterCurves )
                {
                    AkRTPCEntry* pEntry = m_RTPCEntries.Exists( iterCurves.pItem->RTPC_ID );
                    if (pEntry)
						pEntry->DeactivateSubscription( pSubscription );
                }
            }

            in_paramSet.UnsetBit( iBit );
        }
    }
}

void CAkRTPCMgr::UnSubscribeRTPC( void* in_pSubscriber, AkUInt32 in_ParamID )
{
	AkRTPCSubscriptionKey key;
	key.pSubscriber = in_pSubscriber;
	key.ParamID = (AkRTPC_ParameterID) in_ParamID;

	AkRTPCSubscription* pSubscription = m_RTPCSubscribers.Exists( key );
	if ( pSubscription )
	{
		m_RTPCSubscribers.Unset( key );
		RemoveReferencesToSubscription( pSubscription );
		AkDelete(AkMemID_Object, pSubscription );
	}
}

void CAkRTPCMgr::UnSubscribeRTPC( void* in_pSubscriber )
{
	AkRTPCSubscription::List toDelete;
	AkMapRTPCSubscribers::IteratorEx iterRTPCSubs = m_RTPCSubscribers.BeginEx();
	while( iterRTPCSubs != m_RTPCSubscribers.End() )
    {
		AkRTPCSubscription * pSubscription = *iterRTPCSubs;

		if( pSubscription->key.pSubscriber == in_pSubscriber )
		{
			iterRTPCSubs = m_RTPCSubscribers.Erase( iterRTPCSubs );
			toDelete.AddFirst(pSubscription);
		}
		else
		{
			++iterRTPCSubs;
		}
	}

	//Need to perform in a seconds pass because RemoveReferencesToSubscription() can end up calling back into RTPC Mgr
	//	and modifying the m_RTPCSubscribers map.
	while (!toDelete.IsEmpty())
	{
		AkRTPCSubscription * pSubscription = toDelete.First();
		AKVERIFY( toDelete.RemoveFirst() == AK_Success );
		RemoveReferencesToSubscription( pSubscription );
		AkDelete(AkMemID_Object, pSubscription );
	}
}

void CAkRTPCMgr::RemovedScopedRtpcObj( AkRtpcID in_RTPCid, const AkRTPCKey& in_rtpcKey )
{
	AkRTPCSubscription::List toDelete;

	AkRTPCEntry * pEntry = m_RTPCEntries.Exists(in_RTPCid);
	if (pEntry)
	{
        // REVIEW: iterating through these subscriptions lists can potentially be slow if there are many subscriptions.

		AkRTPCSubscriptions::Iterator iterRTPCSubs = pEntry->activeSubscriptions.Begin();
		while( iterRTPCSubs != pEntry->activeSubscriptions.End() )
		{
			AkRTPCSubscription & rSubscription = **iterRTPCSubs;
			if(rSubscription.TargetKey == in_rtpcKey)
			{
				iterRTPCSubs = pEntry->activeSubscriptions.Erase( iterRTPCSubs );
				toDelete.AddFirst(&rSubscription);
			}
			else
				++iterRTPCSubs;
		}

        iterRTPCSubs = pEntry->dormantSubscriptions.Begin();
        while( iterRTPCSubs != pEntry->dormantSubscriptions.End() )
        {
			AkRTPCSubscription & rSubscription = **iterRTPCSubs;
			if(rSubscription.TargetKey == in_rtpcKey)
            {
                iterRTPCSubs = pEntry->dormantSubscriptions.Erase( iterRTPCSubs );
                toDelete.AddFirst(&rSubscription);
            }
            else
                ++iterRTPCSubs;
        }

		pEntry->RemoveMatchingValues( in_rtpcKey );
	}

	
	while (!toDelete.IsEmpty())
	{
		AkRTPCSubscription * pSubscription = toDelete.First();
		AKVERIFY( toDelete.RemoveFirst() == AK_Success );
		m_RTPCSubscribers.Unset(pSubscription->key);
		g_pModulatorMgr->RemoveSubscription( pSubscription );
		AkDelete(AkMemID_Object, pSubscription );
	}
}

void CAkRTPCMgr::UpdateSubscription(
	AkRTPCSubscription& in_rSubscription,
	AkRtpcID in_RtpcID // = AK_INVALID_UNIQUE_ID
	)
{
	bool bLiveEdit = ( in_RtpcID != AK_INVALID_UNIQUE_ID );

	if( in_rSubscription.eType == SubscriberType_CAkParameterNodeBase )
	{
		reinterpret_cast<CAkRTPCSubscriberNode*>(in_rSubscription.key.pSubscriber)->NotifyParamChanged(bLiveEdit, in_rSubscription.key.ParamID);
	}
	else if ( in_rSubscription.eType == SubscriberType_Plugin )
	{
		IAkRTPCSubscriberPlugin* pSubscriber = reinterpret_cast<IAkRTPCSubscriberPlugin*>( in_rSubscription.key.pSubscriber );
	
#ifndef AK_OPTIMIZED
		// Open an update brace so we can log the RTPC update in PluginRTPCSub::UpdateTargetParam
		AkDeltaMonitor::OpenUpdateBrace(AkDelta_RTPC, pSubscriber->GetContextID());

		// Log the RTPC value first
		AkRTPCKey key;
		bool isAutomated = false;
		AkReal32 rtpcValue = 0.0f;
		GetRTPCValue<CurrentValue>(in_RtpcID, in_rSubscription.key.ParamID, in_rSubscription.eType, key, rtpcValue, isAutomated);
		AkDeltaMonitor::LogDriver(in_RtpcID, rtpcValue);
#endif

		if ( in_RtpcID == AK_INVALID_UNIQUE_ID )
		{
			AkReal32 fValue = ( in_rSubscription.eAccum == AkRtpcAccum_Multiply ) ? AccumulateMult::BaseValue() : AccumulateAdd::BaseValue();
			pSubscriber->NotifyRtpcParamChanged( in_rSubscription.key.ParamID, in_RtpcID, in_rSubscription.eAccum, fValue, true );
		}
		else
		{
			AkReal32 fValue = ( in_rSubscription.eAccum == AkRtpcAccum_Multiply )
				? GetRTPCConvertedValue<AccumulateMult,CurrentValue>( &in_rSubscription, in_rSubscription.TargetKey, in_RtpcID )
				: GetRTPCConvertedValue<AccumulateAdd,CurrentValue>( &in_rSubscription, in_rSubscription.TargetKey, in_RtpcID );

			pSubscriber->NotifyRtpcParamChanged( in_rSubscription.key.ParamID, in_RtpcID, in_rSubscription.eAccum, fValue, false );
		}

#ifndef AK_OPTIMIZED
		AkDeltaMonitor::CloseUpdateBrace(pSubscriber->GetContextID());
#endif		
	}
	else if( in_rSubscription.eType == SubscriberType_PBI )
	{
		AkReal32 rtpcValue = GetRTPCConvertedValue<AccumulateAdd,CurrentValue>( &in_rSubscription, in_rSubscription.TargetKey );

		static_cast<CAkBehavioralCtx*>( in_rSubscription.key.pSubscriber )->SetParam( 
				static_cast<AkPluginParamID>( in_rSubscription.key.ParamID ), 
				&rtpcValue,
				sizeof( rtpcValue )
				);
	}
	else if( in_rSubscription.eType == SubscriberType_Modulator )
	{
		AkReal32 rtpcValue = GetRTPCConvertedValue<AccumulateAdd,CurrentValue>( &in_rSubscription, in_rSubscription.TargetKey );

		static_cast<CAkModulator*>( in_rSubscription.key.pSubscriber )->SetParamFromRTPCMgr( 
			in_rSubscription.key.ParamID,
			rtpcValue,
			in_rSubscription.TargetKey
			);
	}
	else if( in_rSubscription.eType == SubscriberType_SwitchGroup )
	{
		AkReal32 rtpcValue = GetRTPCConvertedValue<AccumulateAdd, CurrentValue>(&in_rSubscription, in_rSubscription.TargetKey);
		AkRTPCEntry* pEntry = m_RTPCEntries.Exists(in_RtpcID);
		if (pEntry)
		{
			AkRTPCExceptionChecker_<AkRTPCValueTree> exChcker(in_rSubscription.TargetKey, pEntry->GetValueTree());
			g_pSwitchMgr->SetSwitchFromRTPCMgr(in_rSubscription.key.pSubscriber, in_rSubscription.TargetKey, rtpcValue, &exChcker);
		}
		else
		{
			g_pSwitchMgr->SetSwitchFromRTPCMgr(in_rSubscription.key.pSubscriber, in_rSubscription.TargetKey, rtpcValue, NULL);
		}
	}
	else
	{
		AKASSERT(in_rSubscription.eType == SubscriberType_CAkLayer || in_rSubscription.eType == SubscriberType_CAkCrossfadingLayer);

		// Simply tell playing instances to recalc, way faster and way simplier than starting a serie of endless notifications
		reinterpret_cast<CAkLayer*>(in_rSubscription.key.pSubscriber)->RecalcNotification( bLiveEdit );
	}
}

void CAkRTPCMgr::ResetRTPC( CAkRegisteredObj * pGameObj, AkPlayingID in_playingID )
{
	AkRTPCKey key;
	key.GameObj() = pGameObj;
	key.PlayingID() = in_playingID;

	// Reset all values and then update everybody...
	for ( AkMapRTPCEntries::Iterator it = m_RTPCEntries.Begin(); it != m_RTPCEntries.End(); ++it )
		(*it)->RemoveValue( key );

	for( AkMapRTPCSubscribers::Iterator iter = m_RTPCSubscribers.Begin(); iter != m_RTPCSubscribers.End(); ++iter )
	{
		UpdateSubscription( **iter );
	}
}

CAkRTPCMgr::AkRTPCEntry * CAkRTPCMgr::GetRTPCEntry( AkRtpcID in_RTPCid )
{
	AkRTPCEntry * pEntry = m_RTPCEntries.Exists( in_RTPCid );
	if ( pEntry )
		return pEntry;

	pEntry = AkNew(AkMemID_Object, AkRTPCEntry(in_RTPCid));
	if (!pEntry)
		return NULL;

	m_RTPCEntries.Set( pEntry );

	return pEntry;
}

void CAkRTPCMgr::SetDefaultParamValue( AkRtpcID in_RTPCid, AkReal32 in_fValue )
{
	AkRTPCEntry * pEntry = GetRTPCEntry( in_RTPCid );
	if ( pEntry )
		pEntry->fDefaultValue = in_fValue;
}

void CAkRTPCMgr::SetRTPCRamping( AkRtpcID in_RTPCid, AkTransitionRampingType in_rampingType, AkReal32 in_fRampUp, AkReal32 in_fRampDown )
{
	AkRTPCEntry * pEntry = GetRTPCEntry( in_RTPCid );
	if ( pEntry )
	{
		pEntry->rampingType = in_rampingType;
		pEntry->fRampUp = in_fRampUp;
		pEntry->fRampDown = in_fRampDown;
	}
}

AkReal32 CAkRTPCMgr::GetDefaultValue( AkRtpcID in_RTPCid, bool* out_pbWasFound )
{
	AkRTPCEntry * pEntry = m_RTPCEntries.Exists( in_RTPCid );
	if( out_pbWasFound )
	{
		*out_pbWasFound = (pEntry != NULL);
	}
	return pEntry ? pEntry->fDefaultValue : 0.0f;
}

void CAkRTPCMgr::ResetRTPCValue(AkRtpcID in_RTPCid, CAkRegisteredObj * pGameObj, AkPlayingID in_playingID, TransParamsBase & in_transParams)
{
	AkRTPCKey key;
	key.GameObj() = pGameObj;
	key.PlayingID() = in_playingID;
	ResetRTPCValue(in_RTPCid, key, in_transParams);
}

void CAkRTPCMgr::ResetRTPCValue(AkRtpcID in_RTPCid, const AkRTPCKey& in_rtpcKey, TransParamsBase & in_transParams)
{
	//Remove the local value of the parameter for this game object.  It will default back
	//to the default value automatically, if there is no global value.
	bool bCheckExceptions = true;
	AkRTPCEntry * pEntry = m_RTPCEntries.Exists( in_RTPCid );
	if ( pEntry )
	{
		// Remove any existing transitions
		bool bHasTrans = pEntry->DoesValueHaveTransition( in_rtpcKey );

		AkRTPCValue * pFallbackValue = NULL;
		AkRTPCValue * pValue = pEntry->FindExactValue( in_rtpcKey, &pFallbackValue, &bCheckExceptions );
		if ( !pValue && !bHasTrans ) // No previous value ? No change.
			return;

		AkReal32 fNewValue;

		if ( pFallbackValue )
			fNewValue = pFallbackValue->fValue;
		else
			fNewValue = pEntry->fDefaultValue;
		
		AKVERIFY( pEntry->SetRTPC(
			pValue,
			pFallbackValue,
			fNewValue, 
			in_rtpcKey,
			in_transParams,
			true,				// Remove game object's specific RTPC Value once target value is reached.
			bCheckExceptions
			) == AK_Success || !"Reset RTPC must succeed" );
	}
}

AKRESULT CAkRTPCMgr::SetMidiParameterValue(
							   AkAssignableMidiControl in_eMidiParm, 
							   AkReal32 in_fValue, 
							   const AkRTPCKey& in_rtpcKey
							   )
{
	AKASSERT( in_eMidiParm >= AssignableMidiControl_Start && in_eMidiParm < AssignableMidiControl_Max );
	TransParams transParams;
	transParams.TransitionTime = (AkTimeMs)0;

	return SetRTPCInternal((AkRtpcID)(in_eMidiParm), in_fValue, in_rtpcKey, transParams, AkValueMeaning_Independent);
}

void CAkRTPCMgr::SetMidiParameterDefaultValue(
									  AkAssignableMidiControl in_eMidiParm, 
									  AkReal32 in_DefaultValue
									  )
{
	AKASSERT( in_eMidiParm >= AssignableMidiControl_Start && in_eMidiParm < AssignableMidiControl_Max );
	AkRTPCEntry * pEntry = GetRTPCEntry( (AkRtpcID)(in_eMidiParm) );
	if ( pEntry )
		pEntry->fDefaultValue = in_DefaultValue;
}

void CAkRTPCMgr::AddBuiltInParamBinding( AkBuiltInParam in_builtInParamIdx, AkRtpcID in_RTPCid )
{
	AKASSERT(in_builtInParamIdx >= BuiltInParam_Start && in_builtInParamIdx < BuiltInParam_Max);
	AkRtpcIdArray& rtpcArr = m_BuiltInParamBindings[in_builtInParamIdx];

	RemoveBuiltInParamBindings(in_RTPCid);

	AkRtpcID* pRtpcId = rtpcArr.AddLast(in_RTPCid);
	if (pRtpcId != NULL)
	{
		*pRtpcId = in_RTPCid;
	}
}

void CAkRTPCMgr::RemoveBuiltInParamBindings( AkRtpcID in_RTPCid )
{
	for ( AkUInt32 paramIdx = BuiltInParam_Start; paramIdx < BuiltInParam_Max; ++paramIdx )
	{
		AkRtpcIdArray& rtpcArr = m_BuiltInParamBindings[paramIdx];

		for ( AkRtpcIdArray::Iterator it = rtpcArr.Begin(); it != rtpcArr.End(); ++it )
		{
			if ( *it == in_RTPCid )
			{
				rtpcArr.EraseSwap(it);
				break;
			}
		}
	}
}

void CAkRTPCMgr::SetBuiltInParamValue( AkBuiltInParam in_builtInParamIdx, const AkRTPCKey& in_key, AkReal32 in_fValue )
{
	AKASSERT(in_builtInParamIdx > 0 && in_builtInParamIdx < BuiltInParam_Max);
	AkRtpcIdArray& rtpcArr = m_BuiltInParamBindings[in_builtInParamIdx];

	for ( AkRtpcIdArray::Iterator it = rtpcArr.Begin(); it != rtpcArr.End(); ++it )
	{
		TransParams tp;
		SetRTPCInternal(*it, in_fValue, in_key, tp, AkValueMeaning_Independent, true);
	}
}

AkUInt32 GetNumberOfRTPC(AkUInt8*& io_rpData)
{
	AkUInt8* pData = io_rpData;
	AkUInt32 uSize = 0;
	return READBANKDATA(AkUInt16, pData, uSize);
}
