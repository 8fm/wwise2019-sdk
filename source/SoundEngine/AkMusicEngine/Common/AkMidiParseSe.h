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

#ifndef __AK_MIDI_PARSE_SE_H__
#define __AK_MIDI_PARSE_SE_H__

#include "AkMidiStructs.h"

#include <AK/Tools/Common/AkListBare.h>


//-----------------------------------------------------------------------------
// Used to return list of timed events
//-----------------------------------------------------------------------------
struct AkMidiWindowEvent : public AkMidiEventEx
{
	AkMidiWindowEvent( const AkMidiEventEx& in_rEvent )
		: AkMidiEventEx( in_rEvent )
		, fWindowDelta( 0.f )
		, uEventIdx( 0 )
		, pNextItem( NULL )
	{}

	AkReal32			fWindowDelta;	// Time delta from beginning of window
	AkUInt32			uEventIdx;		// Idx of event (used by parser)
	AkMidiWindowEvent*	pNextItem;
};
typedef AkListBare< AkMidiWindowEvent > WindowEventList;



//-----------------------------------------------------------------------------
// Parser class
//-----------------------------------------------------------------------------
class AkMidiParseSe
{
public:
	
	AkMidiParseSe();
    virtual ~AkMidiParseSe();

	AKRESULT SetBuffer( AkUInt8* in_pbyBuffer, AkUInt32 in_uiBufferSize );
	void RelocateMedia(AkUInt8* in_pbyNewLocation);
	AKRESULT SeekToTime( AkReal32 in_fTime );
	void SeekToStart();
	void MoveEventWindow( AkReal32 in_rfWindowTime, WindowEventList& out_listEvents );
	bool GetNextEvent( AkMidiEventEx& out_rEvent, AkUInt32& out_uEventIdx );

	void SetTempo( const AkReal32 in_fTempo );
	void SetLoopOk( const bool in_bLoopOk );

	// Parse pointer struct for setting parsing position
	struct MidiParsePtr
	{
		AkUInt32	m_uEventIdx;
		AkUInt32	m_uEventTime;
		AkUInt32	m_uParseTicks;
		AkReal32	m_fParseFrac;
	};

	// Following functions are used to save and restore current parsing position.
	void GetCurrentPos( MidiParsePtr& out_parsePtr ) const;
	void SetCurrentPos( const MidiParsePtr& in_parsePtr );

private:

	// Parsing functions
	void QuickParse();
	bool ResetParse();

	AkForceInline void _CalcMsPerTick();
	AkForceInline AkReal32 _TicksToMs( AkUInt32 in_uTicks );
	AkForceInline void _SetTickCntAfterTime( AkReal32 in_fTime );

	AkForceInline bool HaveMore() const;

	AkForceInline AkUInt32 GetTimeOfNext() const;
	AkForceInline AkUInt32 GetEventIdx() const;
	AkForceInline void SetEventIdx( AkUInt32 in_uEventIdx );
	AkForceInline void SetParseTicks( AkUInt32 in_uTicks );
	AkForceInline AkUInt32 GetParseTicks() const;
	AkForceInline void SetParseFrac( AkReal32 in_fFrac );
	AkForceInline AkReal32 GetParseFrac() const;
	AkForceInline AkUInt32 GetEventDelta() const;

	bool MoveToNext();
	bool IsWantedEvent( AkMidiEventEx& out_rEvent );

	AkUInt8*	m_pBuffer;
	AkUInt32	m_uiBufferSize;

	AkMidiHdr		m_MidiHdr;
	AkMidiTrack		m_Track;

	// Calls to GetEventInWindow are cumulative.  That is, the time window
	// used to fetch the next event keeps moving forward.  This member
	// keeps track of how much "window time" has accumulated since the last
	// event was found.  Thus, essentially, this member is a time offset
	// from the previous event found.
	//AkReal32	m_fTimeAfterSeek;

	AkReal32	m_fTempo;	// Target tempo for parsing
	AkReal32	m_fMsPerTick;
	AkReal32	m_fTicksPerMs;

	AkUInt32	m_uEventIdx; // Idx of next event (keeps increasing upon file wrap)
	AkUInt32	m_uEventTime; // Absolute time of next event, in ticks (keeps increasing upon file wrap)

	AkUInt32	m_uParseTicks; // Absolute time of parse, in ticks (keeps increasing upon file wrap)
	AkReal32	m_fParseFrac; // Fractional part of parse time, used to reduce drift from subsequent calls

	AkUInt32	m_bParsed :1;
	AkUInt32	m_bValid :1;
	AkUInt32	m_bLoopOk :1;
};



//-----------------------------------------------------------------------------
// Name: HaveMore
// Desc: Is there another event?
//-----------------------------------------------------------------------------
bool AkMidiParseSe::HaveMore() const
{
	return m_Track.pbySeekEvent != NULL || m_bLoopOk;
}

//-----------------------------------------------------------------------------
// Name: GetTimeToNext
// Desc: Return time to next event.
//-----------------------------------------------------------------------------
AkUInt32 AkMidiParseSe::GetTimeOfNext() const
{
	return m_uEventTime;
}

//-----------------------------------------------------------------------------
// Name: GetEventIdx
// Desc: Return index of next event.
//-----------------------------------------------------------------------------
AkUInt32 AkMidiParseSe::GetEventIdx() const
{
	return m_uEventIdx;
}

//-----------------------------------------------------------------------------
// Name: SetEventIdx
// Desc: Set index of next event.
//-----------------------------------------------------------------------------
void AkMidiParseSe::SetEventIdx( AkUInt32 in_uEventIdx )
{
	m_uEventIdx = in_uEventIdx;
}

//-----------------------------------------------------------------------------
// Name: SetParseTicks
// Desc: Sets total number of ticks counted during parsing upto current position.
//-----------------------------------------------------------------------------
void AkMidiParseSe::SetParseTicks( AkUInt32 in_uTicks )
{
	m_uParseTicks = in_uTicks;
}

//-----------------------------------------------------------------------------
// Name: GetParseTicks
// Desc: Gets total number of ticks counted during parsing upto current position.
//-----------------------------------------------------------------------------
AkUInt32 AkMidiParseSe::GetParseTicks() const
{
	return m_uParseTicks;
}

//-----------------------------------------------------------------------------
// Name: SetParseFrac
// Desc: Fractional part of ticks counter kept during parsing to reduce drift.
//-----------------------------------------------------------------------------
void AkMidiParseSe::SetParseFrac( AkReal32 in_fFrac )
{
	m_fParseFrac = in_fFrac;
}

//-----------------------------------------------------------------------------
// Name: GetParseFrac
// Desc: Fractional part of ticks counter kept during parsing to reduce drift.
//-----------------------------------------------------------------------------
AkReal32 AkMidiParseSe::GetParseFrac() const
{
	return m_fParseFrac;
}

//-----------------------------------------------------------------------------
// Name: GetEventDelta
// Desc: Get event delta, in ticks.
//-----------------------------------------------------------------------------
AkUInt32 AkMidiParseSe::GetEventDelta() const
{
	return m_Track.uiSeekDelta;
}


#endif
