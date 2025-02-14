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


#include "stdafx.h"
#include "AkMidiParseSe.h"

#include <math.h>

#include <AK/Tools/Common/AkObject.h>
#include "../../AkAudiolib/Common/AkMidiParseLib.h"



AkMidiParseSe::AkMidiParseSe()
	: m_pBuffer( NULL )
	, m_uiBufferSize( 0 )
	, m_fTempo( 0 )
	, m_uEventIdx( 0 )
	, m_uEventTime( 0 )
	, m_uParseTicks( 0 )
	, m_fParseFrac( 0.f )
	, m_bParsed( false )
	, m_bValid( false )
	, m_bLoopOk( true )
{
}

AkMidiParseSe::~AkMidiParseSe()
{
}



//=============================================================================
// PUBLIC FUNCTIONS
//=============================================================================



//-----------------------------------------------------------------------------
// Name: SetBuffer
// Desc: Takes buffer pointer and size, and performs a quick parse to ensure
//		everything is kosher.
//-----------------------------------------------------------------------------
AKRESULT AkMidiParseSe::SetBuffer( AkUInt8* in_pbyBuffer, AkUInt32 in_uiBufferSize )
{
	// Nothing is valid anymore
	m_bParsed = false;
	m_bValid = false;

	// Check input params
	if ( ! in_pbyBuffer || in_uiBufferSize == 0 )
		return AK_Fail;

	// Latch inputs
	m_pBuffer = in_pbyBuffer;
	m_uiBufferSize = in_uiBufferSize;

	// Perform quick parse on the buffer
	QuickParse();

	// Reset the seek function
	if ( m_bValid )
		m_bValid = ResetParse();

	// If the quick parse did not pass then forget the buffer
	if ( ! m_bValid )
	{
		m_pBuffer = NULL;
		m_uiBufferSize = 0;
		return AK_Fail;
	}

	return m_bValid ? AK_Success : AK_Fail;
}

void AkMidiParseSe::RelocateMedia(AkUInt8* in_pbyNewLocation)
{
	if (m_pBuffer)
	{

		AkInt64 iDelta = (AkUIntPtr)in_pbyNewLocation - (AkUIntPtr)m_pBuffer;
		
		if (m_Track.pbyBegin)
			m_Track.pbyBegin += iDelta;
		if (m_Track.pbyEnd)
			m_Track.pbyEnd += iDelta;
		if (m_Track.pbySeekEvent)
			m_Track.pbySeekEvent += iDelta;

		m_pBuffer = in_pbyNewLocation;
	}
}

//-----------------------------------------------------------------------------
// Name: SetTempo
//-----------------------------------------------------------------------------
void AkMidiParseSe::SetTempo( const AkReal32 in_fTempo )
{
	if( in_fTempo != 0.0f )
		m_fTempo = in_fTempo;
	_CalcMsPerTick();
}

//-----------------------------------------------------------------------------
// Name: SetLoopOk
//-----------------------------------------------------------------------------
void AkMidiParseSe::SetLoopOk( const bool in_bLoopOk )
{
	m_bLoopOk = in_bLoopOk;
}

//-----------------------------------------------------------------------------
// Name: QuickParse
// Desc: Given a MIDI buffer:
//		- Validates contents of midi header and track chunk header(s),
//		- Keeps pointers to beginning and end of tracks,
//		- May remove meta and sys-ex events, if required.
//-----------------------------------------------------------------------------
void AkMidiParseSe::QuickParse()
{
	// Check if this is necessary
	if ( m_bParsed || !m_pBuffer || m_uiBufferSize == 0 )
		return;

	// Check parameters
	AkUInt8* pbyBufBegin = m_pBuffer;
	AkUInt8* pbyBufEnd = pbyBufBegin + m_uiBufferSize;
	AkUInt8* pbyParse = m_pBuffer;

	// No longer have a midi buffer associated ... for now
	m_bParsed = true;
	m_bValid = false;

	m_MidiHdr.Clear();

	m_MidiHdr.uiTicksPerBeat = ((AkUInt16)pbyParse[0] << 8) | ((AkUInt16)pbyParse[1] << 0);
	if ( m_MidiHdr.uiTicksPerBeat == 0 )
		return;
	pbyParse += 2;

	AkUInt32 uTempo =	(AkUInt32)pbyParse[0] << 0 |
						(AkUInt32)pbyParse[1] << 8 |
						(AkUInt32)pbyParse[2] << 16 |
						(AkUInt32)pbyParse[3] << 24 ;
	m_fTempo = *(AkReal32*)&uTempo;
	_CalcMsPerTick();
	pbyParse += 4;

	// Roughly parse the track
	{
		m_Track.Clear();

		// Make sure we're still within the provided buffer
		m_Track.pbyBegin	= pbyParse;
		m_Track.pbyEnd		= pbyBufEnd;
	}

	// We have a valid MIDI file!
	m_bValid = true;
}

//-----------------------------------------------------------------------------
// Name: SeekToTime
// Desc: Seeks events until reached either:
//		- the exact requested time,
//		- the event just before exceeding the requested time
// Note:
//		- the requested time must be from 0 to duration of original clip
//-----------------------------------------------------------------------------
AKRESULT AkMidiParseSe::SeekToTime( AkReal32 in_fTime )
{
	AKASSERT( m_bValid );
	if ( ! m_bValid )
		return AK_Fail;

	ResetParse();

	// Get total number of ticks equivalent to seek time
	_SetTickCntAfterTime( in_fTime );

	// Seek until the first event past the given time
	while ( HaveMore() )
	{
		if ( GetTimeOfNext() >= GetParseTicks() )
			return AK_Success;

		MoveToNext();
	}

	return AK_Fail;
}

//-----------------------------------------------------------------------------
// Name: SeekToStart
// Desc: Sets current parsing position: event index and time to event.
//-----------------------------------------------------------------------------
void AkMidiParseSe::SeekToStart()
{
	AKASSERT( m_bParsed && m_bValid );
	ResetParse();
}

//-----------------------------------------------------------------------------
// Name: GetCurrentPos
// Desc: Gets current parsing position.
//-----------------------------------------------------------------------------
void AkMidiParseSe::GetCurrentPos( MidiParsePtr& out_parsePtr ) const
{
	AKASSERT( m_bParsed && m_bValid );
	out_parsePtr.m_uParseTicks = m_uParseTicks;
	out_parsePtr.m_fParseFrac = m_fParseFrac;
	out_parsePtr.m_uEventIdx = m_uEventIdx;
	out_parsePtr.m_uEventTime = m_uEventTime;
}

//-----------------------------------------------------------------------------
// Name: SetCurrentPos
// Desc: Sets current parsing position.
//-----------------------------------------------------------------------------
void AkMidiParseSe::SetCurrentPos( const MidiParsePtr& in_parsePtr )
{
	AKASSERT( m_bParsed && m_bValid );
	
	ResetParse();

	// Seek until requested event.
	for ( unsigned i = 0; i < in_parsePtr.m_uEventIdx; ++i )
		MoveToNext();

	// Set the seek time delta.
	m_uEventTime = in_parsePtr.m_uEventTime;

	// Set the total number of ticks counted upto this point.
	m_uParseTicks = in_parsePtr.m_uParseTicks;
	m_fParseFrac = in_parsePtr.m_fParseFrac;

	// If we haven't found our event then something is wrong!!
	AKASSERT( GetEventIdx() == in_parsePtr.m_uEventIdx );
}

//-----------------------------------------------------------------------------
// Name: MoveEventWindow
// Desc: Returns an event, if any, that occurs in the given time interval.
//		 Parameter io_fWindowTime is updated to reflect how much time is left
//		 in the window (0 if no event was found).
// Note: A call to this function must be preceeded by either another call to
//		 this function, or a call to SeekToTime.
//-----------------------------------------------------------------------------
void AkMidiParseSe::MoveEventWindow( AkReal32 in_rfWindowTime, WindowEventList& out_listEvents )
{
	AKASSERT( m_bParsed && m_bValid );

	// Ensure event list is empty
	AKASSERT( out_listEvents.IsEmpty() );

	// Remember current parse tick count
	AkUInt32 uStartTickCnt = GetParseTicks();

	// Set parse tick count to after window
	_SetTickCntAfterTime( in_rfWindowTime );

	// Get events until we've reached the tick count above
	while ( HaveMore() && GetTimeOfNext() < GetParseTicks() )
	{
		AkMidiEventEx midiEvent;

		// Check if the next event is worthy of the user; if so, then add to list.
		if ( IsWantedEvent( midiEvent ) )
		{
			AkMidiWindowEvent* pWndEvent = AkNew(AkMemID_Object, AkMidiWindowEvent(midiEvent));
			if ( pWndEvent != NULL )
			{
				// Return time of event, relative to start of requested interval
				pWndEvent->fWindowDelta = _TicksToMs( GetTimeOfNext() - uStartTickCnt );

				// Return the event idx, relative to the start of the file
				pWndEvent->uEventIdx = GetEventIdx();

				// Add to list
				out_listEvents.AddLast( pWndEvent );
			}
		}

		// Move on to the next event.
		bool bMovedToNext = MoveToNext();

		// Return false if there's nothing else waiting for us.
		if ( ! bMovedToNext )
			break;
	}
}

//-----------------------------------------------------------------------------
// Name: GetNextEvent
// Desc: Gets next event that may be useful to user.
//-----------------------------------------------------------------------------
bool AkMidiParseSe::GetNextEvent( AkMidiEventEx& out_rEvent, AkUInt32& out_uEventIdx )
{
	AKASSERT( m_bParsed && m_bValid );

	// We should be looking for only one event, but there is the possibility of a tempo
	// event, which we want to keep track of, but not return
	while ( true )
	{
		// Check if the next event is worthy of the user.
		bool bEventIsGood = IsWantedEvent( out_rEvent );

		// Get event index.
		out_uEventIdx = GetEventIdx();

		// Move on to the next event.
		bool bMovedToNext = MoveToNext();

		// Return event if we've found something.
		if ( bEventIsGood )
			break;

		// Return false if there's nothing else waiting for us.
		if ( ! bMovedToNext )
			return false;
	}

	return true;
}



//=============================================================================
// PRIVATE FUNCTIONS
//=============================================================================



//-----------------------------------------------------------------------------
// Name: ResetParse
// Desc: Reset context to start parsing at start of file.
//-----------------------------------------------------------------------------
bool AkMidiParseSe::ResetParse()
{
	bool bReset = ResetSeek( &m_Track, 1 );

	m_uEventIdx = 0;
	m_uEventTime = m_Track.uiSeekDelta;
	m_uParseTicks = 0;
	m_fParseFrac = 0.f;

	return bReset;
}

//-----------------------------------------------------------------------------
// Name: MoveToNext
// Desc: Seek to next event, updating member variables according to the
//		 current event.
//-----------------------------------------------------------------------------
bool AkMidiParseSe::MoveToNext()
{
	if ( ! m_bValid )
		return false;

	// Check if we stopped looping
	if ( ! m_Track.pbySeekEvent && ! m_bLoopOk )
		return false;

	// Move on to next event
	SeekNextEvent( &m_Track );

	// Loop back to the start of the track if necessary
	if ( ! m_Track.pbySeekEvent && m_bLoopOk )
	{
		// Seek back to first event
		ResetSeek( &m_Track, 1 );
	}

	// Update total tick count
	m_uEventTime += GetEventDelta();

	// Update event count
	m_uEventIdx++;

	return true;
}

//-----------------------------------------------------------------------------
// Name: GetWantedEvent
// Desc: Gets event info only if the event is of interest to the user
//		 (i.e. note-on, note-off or cc).
//-----------------------------------------------------------------------------
bool AkMidiParseSe::IsWantedEvent( AkMidiEventEx& out_rEvent )
{
	if ( ! m_bValid || ! m_Track.pbySeekEvent )
		return false;

	if ( IsChanEvent( &m_Track ) )
	{
		// Generate an event
		out_rEvent.byType = GetEventType( &m_Track );
		out_rEvent.byChan = GetEventChan( &m_Track );
		out_rEvent.Gen.byParam1 = m_Track.abySeekExtract[1];
		out_rEvent.Gen.byParam2 = m_Track.abySeekExtract[2];

		// Convert note on with velocity 0 to note off
		if ( out_rEvent.byType == AK_MIDI_EVENT_TYPE_NOTE_ON && out_rEvent.NoteOnOff.byVelocity == 0 )
			out_rEvent.byType = AK_MIDI_EVENT_TYPE_NOTE_OFF;

		return true;
	}

	return false;
}




//======================================================================================
// Helpers
//======================================================================================

void AkMidiParseSe::_CalcMsPerTick()
{
	m_fMsPerTick = AK_MILLISECONDS_PER_MINUTE / (m_fTempo * m_MidiHdr.uiTicksPerBeat);
	m_fTicksPerMs = (m_fTempo * m_MidiHdr.uiTicksPerBeat) / AK_MILLISECONDS_PER_MINUTE;
}

AkReal32 AkMidiParseSe::_TicksToMs( AkUInt32 in_uTicks )
{
	return m_fMsPerTick * in_uTicks;
}

void AkMidiParseSe::_SetTickCntAfterTime( AkReal32 in_fTime )
{
	AkReal32 fTimeAfter = m_fTicksPerMs * in_fTime + m_fParseFrac;
	AkReal32 fTimeFloor = floorf( fTimeAfter );
	m_fParseFrac = fTimeAfter - fTimeFloor;
	SetParseTicks( m_uParseTicks + (AkUInt32)fTimeFloor );
}
