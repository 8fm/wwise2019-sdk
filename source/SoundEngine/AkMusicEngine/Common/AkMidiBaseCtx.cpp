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
// AkMidiBaseCtx.cpp
//
// Manages the playout context of a midi source.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkMidiBaseCtx.h"
#include "AkMidiBaseMgr.h"

CAkMidiBaseCtx::CAkMidiBaseCtx
	(
		CAkRegisteredObj*	in_pGameObj,
		TransParams &		in_rTransParams,
		UserParams &		in_rUserParams,
		AkUniqueID			in_uidRootTarget
	)
: pNextLightItem( NULL )
, m_pGameObj( in_pGameObj )
, m_UserParams( in_rUserParams )
, m_TransParams( in_rTransParams )
, m_uRootTargetId( in_uidRootTarget )
, m_pRootTargetNode( NULL )
, m_bIsRootTargetBus( false )
, m_lRef( 1 )
, m_bMuted(false)
{
	if( m_pGameObj )
		m_pGameObj->AddRef();
}

CAkMidiBaseCtx::~CAkMidiBaseCtx()
{
	if ( m_pGameObj )
		m_pGameObj->Release();

	if ( m_pRootTargetNode )
		m_pRootTargetNode->Release();
}

AKRESULT CAkMidiBaseCtx::Init()
{
	// Determine the target for midi data
	return ResolveMidiTarget() ? AK_Success : AK_Fail;
}

//-----------------------------------------------------------------------------
// Name: AddMidiEvent
// Desc: Simply add a new event to a pending MIDI event list.  The list will
//		 be treated later by a call to ScheduleMidiEvents
//-----------------------------------------------------------------------------
void CAkMidiBaseCtx::AddMidiEvent(
	MidiFrameEventList&		in_listEvents,
	const AkMidiEventEx&	in_rEvent,
	const AkUInt32			in_uFrameOffset,
	const AkUInt32			in_uEventIdx, // 0
	const bool				in_bWeakEvent, // false
	const bool				in_bFirstEvent ) // false
{
	// Filter out the meta events
	if( !in_rEvent.IsTypeOk() )
		return;

	if ( m_bMuted )
		return;

	AkMidiFrameEvent* pEvent = AkNew(AkMemID_Object, AkMidiFrameEvent( this, in_rEvent, in_uFrameOffset, in_uEventIdx, in_bWeakEvent, in_bFirstEvent ) );
	if ( pEvent )
	{
		// Insert in ordered list, by frame offset
		MidiFrameEventList::IteratorEx it = in_listEvents.BeginEx();
		for ( ; it != in_listEvents.End(); ++it )
		{
			AkMidiFrameEvent* pItem = *it;
			if ( pItem->m_frameOffset > pEvent->m_frameOffset )
				break; // respect chronological order of events.
			if ( pItem->m_frameOffset == pEvent->m_frameOffset && pItem->m_bWeakEvent && ! pEvent->m_bWeakEvent )
				break; // weak events should be processed after non-weak events.
			if ( pItem->m_frameOffset == pEvent->m_frameOffset && ! pItem->m_bFirstNote && pEvent->m_bFirstNote )
				break; // first note events should be processed first (may affect events that follow in list).
		}
		in_listEvents.Insert( it, pEvent );
	}
}

//-----------------------------------------------------------------------------
// Name: CleanupActions
//-----------------------------------------------------------------------------
void CAkMidiBaseCtx::CleanupActions()
{
	CAkMidiBaseMgr* pMidiMgr = GetMidiMgr();
	if ( m_pRootTargetNode && pMidiMgr )
		pMidiMgr->CleanupActions( this );
}

//-----------------------------------------------------------------------------
// Name: AddRef
//-----------------------------------------------------------------------------
AkUInt32 CAkMidiBaseCtx::AddRef()
{
	return ++m_lRef;
}

//-----------------------------------------------------------------------------
// Name: Release
//-----------------------------------------------------------------------------
AkUInt32 CAkMidiBaseCtx::Release()
{
	AKASSERT( m_lRef > 0 );
    AkInt32 iRef = --m_lRef;
    if ( m_lRef == 0 )
    {
        AkDelete(AkMemID_Object, this );
    }
	return iRef;
}
