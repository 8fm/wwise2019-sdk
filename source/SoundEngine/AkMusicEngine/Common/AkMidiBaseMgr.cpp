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
// AkMidiBaseMgr.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkMidiBaseMgr.h"
#include "AkMidiBaseCtx.h"
#include "AkMidiNoteCtx.h"
#include "AkPlayingMgr.h"
#include "AkBankMgr.h"
#include "AkMusicCtx.h"
#include "AkMusicRenderer.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CAkMidiBaseMgr::CAkMidiBaseMgr()
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CAkMidiBaseMgr::~CAkMidiBaseMgr()
{
	CleanupTargetInfo();

	AKASSERT( m_mapTargetInfo.IsEmpty() );
	m_mapTargetInfo.Term();

	AKASSERT( m_listNoteOffs.IsEmpty() );
	CleanupNoteOffs( false );
	m_listNoteOffs.Term();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void CAkMidiBaseMgr::AttachCtx( CAkMidiBaseCtx* in_pCtx )
{
	m_listMidiCtx.AddFirst( in_pCtx );
}

void CAkMidiBaseMgr::DetachCtx( CAkMidiBaseCtx* in_pCtx )
{
	m_listMidiCtx.Remove( in_pCtx );
}

//-----------------------------------------------------------------------------
// Name: ScheduleMidiEvents
// Desc: Go through list of add events and schedule needed actions.
//-----------------------------------------------------------------------------
void CAkMidiBaseMgr::ScheduleMidiEvents( MidiFrameEventList& in_listEvents, AkUInt32 in_uNumSamples )
{
	while( ! in_listEvents.IsEmpty() )
	{
		// Careful: UpdateOnFirstNote may remove items from the list.
		// So, play it safe!  Don't keep an iterator, just use the
		// first item in the list for each iteration of the loop.
		AkMidiFrameEvent* pEvent = *in_listEvents.Begin();
		in_listEvents.RemoveFirst();

		if ( pEvent->m_bFirstNote && ! pEvent->m_bWeakEvent )
			UpdateOnFirstNote( in_listEvents, pEvent );

		ScheduleMidiEvent( pEvent->m_pMidiCtx, pEvent->m_midiEvent, pEvent->m_frameOffset, false );

		AkDelete(AkMemID_Object, pEvent );
	}

	// Now process all note events in one shot!
	UpdateMidiNotes( in_uNumSamples );
}

//-----------------------------------------------------------------------------
// Name: ScheduleMidiEvent
// Desc: Schedule action required for event.
//-----------------------------------------------------------------------------
void CAkMidiBaseMgr::ScheduleMidiEvent(
	CAkMidiBaseCtx*			in_pMidiCtx,
	const AkMidiEventEx&	in_rEvent,
	const AkUInt32			in_uFrameOffset,
	bool					in_bPairWithCtx,
	bool					in_bMonitorEvent )	// true
{
	AKASSERT( in_pMidiCtx );

	if( in_bMonitorEvent )
	{
		MONITOR_MIDIEVENT( in_rEvent, in_pMidiCtx->GetGameObj()->ID(), in_pMidiCtx->GetTrackID(), in_pMidiCtx->MidiTargetId() );
	}

	if (in_pMidiCtx->GetUserParams().PlayingID() != AK_INVALID_PLAYING_ID )
		g_pPlayingMgr->NotifyMidiEvent( in_pMidiCtx->GetUserParams().PlayingID(), in_rEvent );

	// Get the target ctx, it will be needed throughout
	AKASSERT( in_pMidiCtx->MidiTargetNode() );
	TargetInfo* pTargetInfo = m_mapTargetInfo.Exists( in_pMidiCtx->MidiTargetNode() );
	if ( ! pTargetInfo )
	{
		pTargetInfo = m_mapTargetInfo.Set( in_pMidiCtx->MidiTargetNode(), TargetInfo() );
		if ( ! pTargetInfo )
			return;
	}

	CAkParameterNodeBase* pTarget = in_pMidiCtx->MidiTargetNode();

	// Only schedule non-note events when targeting a bus
	if( ( pTarget->NodeCategory() != AkNodeCategory_Bus &&
		  pTarget->NodeCategory() != AkNodeCategory_AuxBus )
	    ||
		( in_rEvent.IsCcEvent() ) )
	{
		CAkMidiNoteEvent * pNoteCtx = AkNew(AkMemID_Object, CAkMidiNoteEvent( in_pMidiCtx, pTarget ) );
		if ( pNoteCtx != NULL )
		{
			// Schedule the event
			bool bScheduled = false;

			if ( pNoteCtx->ScheduleMidiEvent( in_rEvent, in_uFrameOffset ) )
			{
				if ( in_rEvent.IsNoteOff() )
					bScheduled = PairNoteOffInList( pTargetInfo->m_listMidiNotes, pNoteCtx, in_bPairWithCtx );
				else
					bScheduled = true;
			}

			if ( ! bScheduled )
			{
				//TBD: there could be stuck notes if this happens.
				pNoteCtx->Release();
				return;
			}

			if ( !in_rEvent.IsNoteOn() && !in_rEvent.IsNoteOff() )
				CcScheduleMidiEvents( *pTargetInfo, pNoteCtx, in_uFrameOffset );

			AddToMidiNoteList( pTargetInfo->m_listMidiNotes, pNoteCtx );
		}
	}
}

//-----------------------------------------------------------------------------
// Name: PairNoteOffInList
// Desc: Looks for/generates a list for the given target.  If a list is found,
//		 must match with an unmatched note on before adding to the list.
//-----------------------------------------------------------------------------
bool CAkMidiBaseMgr::PairNoteOffInList( MidiNoteList& in_rNoteList, CAkMidiNoteEvent* in_pNoteCtx, bool in_bPairWithCtx )
{
	AKASSERT( in_pNoteCtx->IsNoteOff() );

	// Check for old events on same chan+note
	MidiNoteList::Iterator it = in_rNoteList.Begin();
	for ( ; it != in_rNoteList.End(); ++it )
	{
		AKASSERT( *it );
		if ( ! in_bPairWithCtx || (*it)->GetMidiCtx() == in_pNoteCtx->GetMidiCtx() )
		{
			if ( in_pNoteCtx->PairWithExistingNote( *it ) )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Insert in ordered list, according to frame offset
//-----------------------------------------------------------------------------
void CAkMidiBaseMgr::AddToMidiNoteList( MidiNoteList& in_rNoteList, CAkMidiNoteEvent* in_pNoteCtx )
{
	MidiNoteList::IteratorEx it = in_rNoteList.BeginEx();
	for ( ; it != in_rNoteList.End(); ++it )
	{
		if ( (*it)->GetFrameOffset() > in_pNoteCtx->GetFrameOffset() )
			break;
	}
	in_rNoteList.Insert( it, in_pNoteCtx );
}

//-----------------------------------------------------------------------------
// Name: CleanupActions
// Desc: Cleanup of midi action list.  This is mostly for the stop actions,
//		 which can only be performed within execution frame (frame in which the
//		 stop actually takes place).
//-----------------------------------------------------------------------------
void CAkMidiBaseMgr::CleanupActions( CAkMidiBaseCtx* in_pMidiCtx )
{
	AKASSERT( in_pMidiCtx );
	TargetInfo* pTargetInfo = GetTargetInfo( in_pMidiCtx->MidiTargetNode() );
	if ( ! pTargetInfo )
		return;

	// Schedule sustain-pedal-off event on all channels!
	for ( AkUInt32 i = 0; i < AK_MIDI_MAX_NUM_CHANS; ++i )
	{
		AkMidiEventEx midiEvent;
		midiEvent.MakeSustainPedalOff( i );
		ScheduleMidiEvent( in_pMidiCtx, midiEvent, 0, false, false );
	}

	// Manually set sustain pedals to off; this is not an error nor overkill, the
	// step above and this one are both required!
	pTargetInfo->m_usSustainPedals = 0;

	// Go through midi notes list:
	// - if a play action has been executed, ensure the note-off is scheduled
	// - set the frame offset for all actions to 0
	// - execute all actions
	MidiNoteList& midiNoteList = pTargetInfo->m_listMidiNotes;
	MidiNoteList::Iterator it = midiNoteList.Begin();
	for ( ; it != midiNoteList.End(); ++it )
	{
		CAkMidiNoteEvent* pNoteCtx = *it;
		if ( pNoteCtx->GetMidiCtx() == in_pMidiCtx )
		{
			pNoteCtx->SetFrameOffset( 0 );

			if ( pNoteCtx->IsNoteOn() && ! pNoteCtx->NoteOffScheduled() )
			{
				AkMidiEventEx midiEvent = pNoteCtx->GetMidiEvent();
				midiEvent.byType = AK_MIDI_EVENT_TYPE_NOTE_OFF;
				ScheduleMidiEvent( in_pMidiCtx, midiEvent, pNoteCtx->GetFrameOffset(), true, false );
			}
		}
	}

	// Now process all note events in one shot!
	UpdateMidiNotes( 0 );

	// Re-stop all note-offs with NO FADE!
	// NOTE: this must be done last, or else note-offs created above might
	//		stay in note-off list, because OnFrame may never be called again!
	CleanupNoteOffs( in_pMidiCtx );
}

void CAkMidiBaseMgr::KillAllNotes( CAkMidiBaseCtx* in_pMidiCtx )
{
	KillNotes( in_pMidiCtx, AK_NUM_VOICE_REFILL_FRAMES, AK_INVALID_MIDI_CHANNEL );
}



//-----------------------------------------------------------------------------
// Name: KillNotes
// Desc: Schedule a note-off for each note-on before in_uNumSamples, for the
//		 given target/channel .
//-----------------------------------------------------------------------------
void CAkMidiBaseMgr::KillNotes( CAkMidiBaseCtx* in_pMidiCtx, AkUInt32 in_uNumSamples, AkMidiChannelNo in_uChannel )
{
	AKASSERT( in_pMidiCtx );
	TargetInfo* pTargetInfo = GetTargetInfo( in_pMidiCtx->MidiTargetNode() );
	if ( ! pTargetInfo )
		return;

	// Go through midi notes list:
	// - if a play action has been executed, ensure the note-off is scheduled
	// - set the frame offset for all actions to 0
	// - execute all actions
	MidiNoteList& midiNoteList = pTargetInfo->m_listMidiNotes;
	MidiNoteList::Iterator it = midiNoteList.Begin();
	for ( ; it != midiNoteList.End(); ++it )
	{
		CAkMidiNoteEvent* pNoteCtx = *it;
		if ( pNoteCtx->GetMidiCtx() == in_pMidiCtx )
		{
			if ( pNoteCtx->GetMidiEvent().byChan == in_uChannel || in_uChannel == AK_INVALID_MIDI_CHANNEL )
			{
				if ( pNoteCtx->IsNoteOn() && ! pNoteCtx->NoteOffScheduled() && pNoteCtx->GetFrameOffset() <= (AkInt32)in_uNumSamples )
				{
					AkMidiEventEx midiEvent = pNoteCtx->GetMidiEvent();
					midiEvent.byType = AK_MIDI_EVENT_TYPE_NOTE_OFF;
					ScheduleMidiEvent( in_pMidiCtx, midiEvent, pNoteCtx->GetFrameOffset(), true, false );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Update actions of all pending notes
//-----------------------------------------------------------------------------
void CAkMidiBaseMgr::UpdateMidiNotes( AkUInt32 in_uElapsedFrames )
{
	// Check the note list of each target and execute actions
	TargetInfoMap::Iterator im = m_mapTargetInfo.Begin();
	for ( ; im != m_mapTargetInfo.End(); ++im )
	{
		TargetInfo& rTargetInfo = (*im).item;
		MidiNoteList& listMidiNotes = rTargetInfo.m_listMidiNotes;
		MidiNoteList::IteratorEx it = listMidiNotes.BeginEx();
		while ( it != listMidiNotes.End() )
		{
			CAkMidiNoteEvent* pMidiNote = *it;

			bool bSustainPedal = ( rTargetInfo.m_usSustainPedals & (1 << pMidiNote->GetMidiEvent().byChan) ) != 0;
			bool bAllowEventExecution = ( !bSustainPedal || !pMidiNote->GetMidiEvent().IsNoteOff() );

			if ( pMidiNote->Update( in_uElapsedFrames, bAllowEventExecution ) )
			{
				if ( !pMidiNote->IsNoteOn() && !pMidiNote->IsNoteOff() )
					CcUpdateMidiNotes( rTargetInfo, pMidiNote );

				it = listMidiNotes.Erase( it );

				if( pMidiNote->IsNoteOff() )
				{
					pMidiNote->GetNoteState()->SetNoteStateAware( this );
					m_listNoteOffs.AddLast( pMidiNote ); // keep note-off until no PBIs are attached
				}
				else
					pMidiNote->Release();
			}
			else
				++it;
		}
	}

	// Cleanup finished note-offs
	CleanupNoteOffs( true );
}

//-----------------------------------------------------------------------------
// Cleanup of note-off list.
//-----------------------------------------------------------------------------
void CAkMidiBaseMgr::CleanupNoteOffs( CAkMidiBaseCtx* in_pMidiCtx )
{
	MidiNoteList::IteratorEx ino = m_listNoteOffs.BeginEx();
	while( ino != m_listNoteOffs.End() )
	{
		CAkMidiNoteEvent* pNoteCtx = *ino;
		CAkMidiNoteState* pNoteState = pNoteCtx->GetNoteState();
		if( pNoteCtx->GetMidiCtx() == in_pMidiCtx )
		{
			ino = m_listNoteOffs.Erase(ino);
			pNoteState->SetNoteStateAware( NULL );
			pNoteCtx->StopPBIsNoFade();
#ifndef AK_OPTIMIZED
			// MidiNoteState must release MidiBaseCtx to ensure MidiBaseCtx may die gracefully, in the same frame
			// (active modulators will keep a ref to MidiNoteState, keeping MidiBaseCtx alive until the next frame)
			pNoteState->ReleaseMidiCtx();
#endif	
			pNoteCtx->Release();
		}
		else
			++ino;
	}
}

void CAkMidiBaseMgr::CleanupNoteOffs( bool in_bCheckFinished )
{
	MidiNoteList::IteratorEx ino = m_listNoteOffs.BeginEx();
	while( ino != m_listNoteOffs.End() )
	{
		CAkMidiNoteEvent* pNoteCtx = *ino;
		CAkMidiNoteState* pNoteState = pNoteCtx->GetNoteState();
		if( ! in_bCheckFinished || pNoteState->IsFinished() )
		{
			ino = m_listNoteOffs.Erase( ino );
			pNoteState->SetNoteStateAware( NULL );
			pNoteCtx->StopPBIsNoFade();
#ifndef AK_OPTIMIZED
			// MidiNoteState must release MidiBaseCtx to ensure MidiBaseCtx may die gracefully, in the same frame
			// (active modulators will keep a ref to MidiNoteState, keeping MidiBaseCtx alive until the next frame)
			pNoteState->ReleaseMidiCtx();
#endif	
			pNoteCtx->Release();
		}
		else
			++ino;
	}
}

//-----------------------------------------------------------------------------
// Called when a note-state is finished.
//-----------------------------------------------------------------------------
void CAkMidiBaseMgr::OnNoteStateFinished( CAkMidiNoteState* in_pNote )
{
	MidiNoteList::IteratorEx ino = m_listNoteOffs.BeginEx();
	for( ; ino != m_listNoteOffs.End(); ++ino )
	{
		CAkMidiNoteEvent* pNoteCtx = *ino;
		if( pNoteCtx->GetNoteState() == in_pNote )
		{
			m_listNoteOffs.Erase( ino );
			in_pNote->SetNoteStateAware( NULL );
			pNoteCtx->Release();
			// May be deleted after this; DO NOT refer to any members!!!
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Deal with Cc event during MIDI notes update
//-----------------------------------------------------------------------------
void CAkMidiBaseMgr::CcUpdateMidiNotes( TargetInfo& io_rTargetInfo, CAkMidiNoteEvent* in_pMidiNote )
{
	const AkMidiEventEx& rEvent = in_pMidiNote->GetMidiEvent();
	CAkMidiBaseCtx* pMidiCtx = in_pMidiNote->GetMidiCtx();

	// If the MIDI target is a bus, then the game object and playing ID must NOT be part of the RTPC key!
	AkRTPCKey rtpcKey(	pMidiCtx->IsMidiTargetBus() ? NULL : pMidiCtx->GetGameObj(),
						AK_INVALID_PLAYING_ID,			// <- The reason we do not set this, is to support live playing of keyboard though the authoring. 
														//		Without any special previsions each note played from a keyboard will have a new playing ID.  
														//		Also see PlayInternal() in AkSound.cpp where we set the velocity, etc.
						rEvent.GetNoteAndChannel(),
						pMidiCtx->MidiTargetId() );

	if ( in_pMidiNote->IsCcEvent() )
	{
		//Handle "special" CC's first.
		if ( rEvent.Cc.byCc == AK_MIDI_CC_ALL_CONTROLLERS_OFF )
		{
			io_rTargetInfo.m_usSustainPedals = 0;

			TransParams transParams;
			transParams.TransitionTime = (AkTimeMs)0;
			AK_DELTA_MONITOR_STACK_UPDATE_SCOPE;	//SetMidiParameterValue will log RTPC changes, not valid in previous braces.
			for ( AkRtpcID ctrlr = AssignableMidiControl_Start; ctrlr < AssignableMidiControl_Max; ++ctrlr )
			{
				g_pRTPCMgr->ResetRTPCValue( ctrlr, rtpcKey, transParams );
			}
		}
		else if ( rEvent.Cc.byCc == AK_MIDI_CC_ALL_NOTES_OFF || rEvent.Cc.byCc == AK_MIDI_CC_ALL_SOUND_OFF )
		{
			// Do nothing! This is handled during event scheduling.
		}
		else
		{
			// Standard CC's assigned by user
			AK_DELTA_MONITOR_STACK_UPDATE_SCOPE;	//SetMidiParameterValue will log RTPC changes, not valid in previous braces.
			g_pRTPCMgr->SetMidiParameterValue((AkAssignableMidiControl)(rEvent.Cc.byCc + 1), (AkReal32)rEvent.Cc.byValue, rtpcKey);
		
			if ( rEvent.Cc.byCc == AK_MIDI_CC_HOLD_PEDAL )
			{
				// Update target ctx sustain pedals
				io_rTargetInfo.m_usSustainPedals &= ~(1 << rEvent.byChan);
				AkUInt16 bitField = (rEvent.Cc.byValue >= 64) ? 1 : 0;
				io_rTargetInfo.m_usSustainPedals |= ( bitField << rEvent.byChan);
			}
		}
	}
	else if ( in_pMidiNote->IsPitchBend() )
	{
		AkReal32 fPitchBend = (AkReal32)(((AkUInt16)rEvent.PitchBend.byValueMsb << 7) | ((AkUInt16)rEvent.PitchBend.byValueLsb));
		fPitchBend *= 128.f/16384.f;

		AK_DELTA_MONITOR_STACK_UPDATE_SCOPE;	//SetMidiParameterValue will log RTPC changes, not valid in previous braces.
		g_pRTPCMgr->SetMidiParameterValue( (AssignableMidiControl_PitchBend), fPitchBend, rtpcKey );
	}
}


//-----------------------------------------------------------------------------
// Deal with Cc event during MIDI notes scheduling
//-----------------------------------------------------------------------------
void CAkMidiBaseMgr::CcScheduleMidiEvents( TargetInfo& /*io_rTargetInfo*/, CAkMidiNoteEvent* in_pMidiNote, AkUInt32 in_uFrameOffset )
{
	if ( in_pMidiNote->IsCcEvent() )
	{
		const AkMidiEventEx& rEvent = in_pMidiNote->GetMidiEvent();

		//Handle "special" CC's first.
		if ( rEvent.Cc.byCc == AK_MIDI_CC_ALL_NOTES_OFF || rEvent.Cc.byCc == AK_MIDI_CC_ALL_SOUND_OFF )
		{
			CAkMidiBaseCtx* pMidiCtx = in_pMidiNote->GetMidiCtx();
			KillNotes( pMidiCtx, in_uFrameOffset, rEvent.byChan );
		}
	}
}

//-----------------------------------------------------------------------------
// Removes any lingering target info.
//-----------------------------------------------------------------------------
void CAkMidiBaseMgr::CleanupTargetInfo()
{
	TargetInfoMap::Iterator im = m_mapTargetInfo.Begin();
	while( im != m_mapTargetInfo.End() )
	{
		TargetInfo& rTargetInfo = (*im).item;
		if ( rTargetInfo.m_listMidiNotes.IsEmpty() )
			im = m_mapTargetInfo.Erase( im );
		else
			++im;
	}
}

//-----------------------------------------------------------------------------
// Name: OnPaused
// Desc: Called on pause action.
//-----------------------------------------------------------------------------
void CAkMidiBaseMgr::OnPaused( CAkMidiBaseCtx* in_pMidiCtx )
{
	// Look in list of currently playing notes (note-off not executed)
	TargetInfo* pTargetInfo = GetTargetInfo( in_pMidiCtx->MidiTargetNode() );
	if ( pTargetInfo )
	{
		MidiNoteList::Iterator it = pTargetInfo->m_listMidiNotes.Begin();
		for ( ; it != pTargetInfo->m_listMidiNotes.End(); ++it )
		{
			CAkMidiNoteEvent* pNoteCtx = *it;
			if ( pNoteCtx->GetMidiCtx() == in_pMidiCtx )
				pNoteCtx->Pause( in_pMidiCtx->GetGameObj(), in_pMidiCtx->GetUserParams() );
		}
	}

	// Look in list of note-off pending notes
	MidiNoteList::Iterator ino = m_listNoteOffs.Begin();
	for( ; ino != m_listNoteOffs.End(); ++ino )
	{
		CAkMidiNoteEvent* pNoteCtx = *ino;
		if( pNoteCtx->GetMidiCtx() == in_pMidiCtx )
			pNoteCtx->Pause( in_pMidiCtx->GetGameObj(), in_pMidiCtx->GetUserParams() );
	}
}

//-----------------------------------------------------------------------------
// Name: OnResumed
// Desc: Called on un-pause action.
//-----------------------------------------------------------------------------
void CAkMidiBaseMgr::OnResumed( CAkMidiBaseCtx* in_pMidiCtx )
{
	TargetInfo* pTargetInfo = GetTargetInfo( in_pMidiCtx->MidiTargetNode() );
	if ( pTargetInfo )
	{
		MidiNoteList::Iterator it = pTargetInfo->m_listMidiNotes.Begin();
		for ( ; it != pTargetInfo->m_listMidiNotes.End(); ++it )
		{
			CAkMidiNoteEvent* pNoteCtx = *it;
			if( pNoteCtx->GetMidiCtx() == in_pMidiCtx )
				pNoteCtx->Resume( in_pMidiCtx->GetGameObj(), in_pMidiCtx->GetUserParams() );
		}
	}

	// Look in list of note-off pending notes
	MidiNoteList::Iterator ino = m_listNoteOffs.Begin();
	for( ; ino != m_listNoteOffs.End(); ++ino )
	{
		CAkMidiNoteEvent* pNoteCtx = *ino;
		if( pNoteCtx->GetMidiCtx() == in_pMidiCtx )
			pNoteCtx->Resume( in_pMidiCtx->GetGameObj(), in_pMidiCtx->GetUserParams() );
	}
}
