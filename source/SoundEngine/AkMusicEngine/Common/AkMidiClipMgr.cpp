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
// AkMidiMgr.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkMidiClipMgr.h"
#include "AkMidiNoteCtx.h"

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CAkMidiClipMgr::CAkMidiClipMgr()
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CAkMidiClipMgr::~CAkMidiClipMgr()
{
	AKASSERT( m_listMidiCtx.IsEmpty() );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

AKRESULT CAkMidiClipMgr::AddClipCtx
(
	CAkMusicCtx *		io_pParentCtx,
	CAkMusicTrack *		in_pTrack,
	CAkMusicSource *	in_pSource,
	CAkRegisteredObj *	in_pGameObj,
	TransParams &		in_rTransParams,
	UserParams &		in_rUserParams,
	const AkTrackSrc *	in_pSrcInfo,		// Pointer to track's source playlist item.
	AkUInt32			in_uPlayDuration,	// Total number of samples the playback will last.
	AkUInt32			in_uSourceOffset,	// Start position of source (in samples, at the native sample rate).
	AkUInt32			in_uFrameOffset,	// Frame offset for look-ahead and LEngine sample accuracy.
	CAkMidiClipCtx *&	out_pCtx			// TEMP: Created ctx is needed to set the transition from outside.
)
{
    // Check parameters.
    AKASSERT( in_pTrack != NULL );
    if( in_pTrack == NULL )
        return AK_InvalidParameter;

	out_pCtx = AkNew
		(
			AkMemID_Object,
			CAkMidiClipCtx
				(
					io_pParentCtx,
					this,
					in_pTrack,
					in_pSource,
					in_pGameObj,
					in_rTransParams,
					in_rUserParams,
					in_pSrcInfo,
					in_uPlayDuration,
					in_uSourceOffset,
					in_uFrameOffset
				)
		);

	if( out_pCtx != NULL )
	{
		if( out_pCtx->Init() == AK_Success )
		{
			return AK_Success;
		}
		else
		{
			out_pCtx->Release();
			out_pCtx = NULL;
		}
	}

	return AK_Fail;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void CAkMidiClipMgr::NextFrame( AkUInt32 in_uNumSamples, AkReal32 in_fPlaybackSpeed )
{
	// Cycle through all midi track ctx
	MidiFrameEventList listEvents;
	MidiCtxList::Iterator it = m_listMidiCtx.Begin();
	while( it != m_listMidiCtx.End() )
	{
		CAkMidiClipCtx* pCtx = static_cast<CAkMidiClipCtx*>( *it );
		++it; // Move iterator first because the item may be removed from the list
		pCtx->OnFrame( listEvents, in_uNumSamples );
	}

	// Scale the frame offset of all events back to normal playing speed.
	MidiFrameEventList::Iterator il = listEvents.Begin();
	for ( ; il != listEvents.End(); ++il )
	{
		(*il)->m_frameOffset = (AkUInt32)AkMath::Round( (*il)->m_frameOffset / in_fPlaybackSpeed );
	}

	// Schedule each event.
	ScheduleMidiEvents( listEvents, in_uNumSamples );
}

//-----------------------------------------------------------------------------
// Name: UpdateOnFirstNote
// Desc: Look for weak MIDI ctx on the same target as the event.  For each
//		 such MIDI ctx:
//			- delete all its weak events in the list following this event
//			- force a stop to all outstanding notes
//-----------------------------------------------------------------------------
void CAkMidiClipMgr::UpdateOnFirstNote(
	MidiFrameEventList& in_list,
	AkMidiFrameEvent* in_pEvent )
{
	AKASSERT( in_pEvent->m_bFirstNote );
	AKASSERT( !in_pEvent->m_bWeakEvent );
	AKASSERT( in_pEvent->m_pMidiCtx );

	// Look for weak MIDI ctx on the same target.
	CAkParameterNodeBase* pTargetNode = in_pEvent->m_pMidiCtx->MidiTargetNode();
	TargetInfo* pTargetCtx = GetTargetInfo( pTargetNode );
	if ( pTargetCtx )
	{
		MidiCtxList::Iterator icl = m_listMidiCtx.Begin();
		while ( icl != m_listMidiCtx.End() )
		{
			CAkMidiClipCtx* pMidiCtx = static_cast<CAkMidiClipCtx*>( *icl );
			++icl;

			if ( pMidiCtx && pMidiCtx->IsWeak() &&
				pMidiCtx != in_pEvent->m_pMidiCtx &&
				pMidiCtx->MidiTargetNode() == pTargetNode )
			{
				// Make sure the ctx remains alive while we need it!
				pMidiCtx->AddRef();

				// Remove all events in the list following this event.
				MidiFrameEventList::IteratorEx iel = in_list.BeginEx();
				for ( ; iel != in_list.End(); )
				{
					if ( (*iel)->m_pMidiCtx == pMidiCtx )
					{
						AkMidiFrameEvent* pToDel = *iel;
						iel = in_list.Erase( iel );
						AkDelete(AkMemID_Object, pToDel );
					}
					else
						++iel;
				}

				// Schedule a sustain-pedal-off event (for new note-offs in current frame).
				for ( AkUInt32 i = 0; i < AK_MIDI_MAX_NUM_CHANS; ++i )
				{
					AkMidiEventEx midiEvent;
					midiEvent.MakeSustainPedalOff( i );
					ScheduleMidiEvent( pMidiCtx, midiEvent, in_pEvent->m_frameOffset, false );
				}

				// Manually kill the sustain pedals (for old note-offs that are sustained).
				pTargetCtx->m_usSustainPedals = 0;

				// Schedule note-offs for all outstanding note-ons.
				MidiNoteList& midiNoteList = pTargetCtx->m_listMidiNotes;
				MidiNoteList::IteratorEx inl = midiNoteList.BeginEx();
				for ( ; inl != midiNoteList.End(); ++inl )
				{
					CAkMidiNoteEvent* pNoteCtx = *inl;
					if ( pNoteCtx->GetMidiCtx() == pMidiCtx )
					{
						if ( pNoteCtx->IsNoteOn() && ! pNoteCtx->NoteOffScheduled() )
						{
							AkMidiEventEx midiEvent = pNoteCtx->GetMidiEvent();
							midiEvent.byType = AK_MIDI_EVENT_TYPE_NOTE_OFF;
							ScheduleMidiEvent( pMidiCtx, midiEvent, in_pEvent->m_frameOffset, true );
						}
					}
				}

				// If the ctx must die, then it now has our blessing.
				pMidiCtx->Release();
				pMidiCtx = NULL;
			}
		}
	}

	// Get list of all MIDI CC events that precede this event and schedule each to execute
	// at the same time as this event.
	MidiFrameEventList listCC;
	CAkMidiClipCtx* pMidiCtx = static_cast<CAkMidiClipCtx*>( in_pEvent->m_pMidiCtx );
	pMidiCtx->GetCCEvents( listCC, in_pEvent->m_frameOffset, in_pEvent->m_eventIdx );

	while ( ! listCC.IsEmpty() )
	{
		AkMidiFrameEvent* pCc = *listCC.Begin();
		listCC.RemoveFirst();

		if( ! in_pEvent->m_midiEvent.IsCcEvent() ||
			! in_pEvent->m_midiEvent.IsSameChannel( pCc->m_midiEvent ) ||
			! in_pEvent->m_midiEvent.IsSameCc( pCc->m_midiEvent ) )
		{
			ScheduleMidiEvent( pMidiCtx, pCc->m_midiEvent, pCc->m_frameOffset, false );
		}
		AkDelete(AkMemID_Object, pCc );
	}
}

void CAkMidiClipMgr::StopNoteIfUsingData(CAkUsageSlot* in_pUsageSlot, bool in_bFindOtherSlot)
{
	// Call stop for each ctx using the memory slot
	for ( MidiCtxList::Iterator it = m_listMidiCtx.Begin(); it != m_listMidiCtx.End(); )
	{
		CAkMidiClipCtx* pCtx = static_cast<CAkMidiClipCtx*>(*it);
		++it;
		if (pCtx)
		{
			if (pCtx->IsUsingThisSlot(in_pUsageSlot))
			{
				if (!in_bFindOtherSlot || !pCtx->RelocateMedia(in_pUsageSlot))
				{
					((CAkChildCtx*) pCtx)->_Stop( AK_NO_IN_BUFFER_STOP_REQUESTED );

					// Very important!!  Do not access the ctx anymore, as it may have died...
				}
			}
		}
	}
}
