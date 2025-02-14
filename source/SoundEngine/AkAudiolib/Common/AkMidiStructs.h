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

#ifndef __AK_MIDI_STRUCTS_H__
#define __AK_MIDI_STRUCTS_H__

#include <AK/SoundEngine/Common/AkMidiTypes.h>


//-----------------------------------------------------------------------------
// Constants.
//-----------------------------------------------------------------------------

// List of meta event types
#define AK_MIDI_META_EVENT_TYPE_SEQUENCE_NUMBER			0x00
#define AK_MIDI_META_EVENT_TYPE_TEXT_EVENT				0x01
#define AK_MIDI_META_EVENT_TYPE_COPYRIGHT_NOTICE		0x02
#define AK_MIDI_META_EVENT_TYPE_SEQUENCE_TRACK_NAME		0x03
#define AK_MIDI_META_EVENT_TYPE_INSTRUMENT_NAME			0x04
#define AK_MIDI_META_EVENT_TYPE_LYRICS					0x05
#define AK_MIDI_META_EVENT_TYPE_MARKER					0x06
#define AK_MIDI_META_EVENT_TYPE_CUE_POINT				0x07
#define AK_MIDI_META_EVENT_TYPE_CHANNEL_PREFIX			0x20
#define AK_MIDI_META_EVENT_TYPE_END_OF_TRACK			0x2f
#define AK_MIDI_META_EVENT_TYPE_SET_TEMPO				0x51
#define AK_MIDI_META_EVENT_TYPE_SMPTE_OFFSET			0x54
#define AK_MIDI_META_EVENT_TYPE_TIME_SIGNATURE			0x58
#define AK_MIDI_META_EVENT_TYPE_KEY_SIGNATURE			0x59
#define AK_MIDI_META_EVENT_TYPE_SEQUENCER_SPECIFIC		0x7f

// Useful defines
#define AK_MICROSECONDS_PER_MINUTE			60000000
#define AK_MILLISECONDS_PER_MINUTE			60000
#define AK_MIDI_DEFAULT_TEMPO_BPM			120

#define AK_MIDI_PITCH_BEND_DEFAULT			64

#define AK_MIDI_NUM_VALID_CC				128
#define AK_MIDI_MAX_NUM_NOTES				128
#define AK_MIDI_MAX_NUM_CHANS				16

#define AK_MIDI_VELOCITY_MAX_VAL			127

#define AK_MIDI_BIGGEST_EVENT_POSSIBLE		(4+6) // biggest time + biggest event


//-----------------------------------------------------------------------------
// Macros.
//-----------------------------------------------------------------------------

// Convert 32-bit MIDI event to 8-bit components
#define AK_MIDI_EVENT_GET_TYPE( in_dwEvent ) (AkUInt8)((in_dwEvent >> 0) & 0xf0)
#define AK_MIDI_EVENT_GET_CHANNEL( in_dwEvent ) (AkUInt8)((in_dwEvent >> 0) & 0x0f)
#define AK_MIDI_EVENT_GET_PARAM1( in_dwEvent ) (AkUInt8)(in_dwEvent >> 8)
#define AK_MIDI_EVENT_GET_PARAM2( in_dwEvent ) (AkUInt8)(in_dwEvent >> 16)


//-----------------------------------------------------------------------------
// Structs
//-----------------------------------------------------------------------------

struct AkMidiNoteChannelPair
{
	AkMidiNoteChannelPair(): note(AK_INVALID_MIDI_NOTE), channel(AK_INVALID_MIDI_CHANNEL) {}

	bool operator == ( const AkMidiNoteChannelPair& in_rhs ) const { return note == in_rhs.note && channel == in_rhs.channel; }
	bool operator < ( const AkMidiNoteChannelPair& in_rhs ) const { return channel < in_rhs.channel || (channel == in_rhs.channel && note < in_rhs.note ); }
	bool operator > ( const AkMidiNoteChannelPair& in_rhs ) const { return channel > in_rhs.channel || (channel == in_rhs.channel && note > in_rhs.note ); }

	bool IsChannelValid() const	{ return channel != AK_INVALID_MIDI_CHANNEL; }
	bool IsNoteValid() const 	{ return note != AK_INVALID_MIDI_NOTE; }

	AkMidiNoteNo		note;
	AkMidiChannelNo		channel;
};

struct AkMidiEventEx : public AkMIDIEvent
{
	AkMidiEventEx& operator=( const AkMIDIEvent& in_event )
	{
		AkMIDIEvent* pCast = (AkMIDIEvent*)this;

		if( pCast == &in_event )
			return *this;

		*pCast = in_event;
		return *this;
	}

	inline AkMidiEventEx()
	{
		byType = AK_MIDI_EVENT_TYPE_INVALID;
		byChan = AK_INVALID_MIDI_CHANNEL;
	}

	inline AkMidiEventEx( AkMIDIEvent& in_event )
		: AkMIDIEvent( in_event )
	{}

	inline bool IsValid() const { return byType != AK_MIDI_EVENT_TYPE_INVALID; }
	inline void MakeInvalid()	{ *this = AkMidiEventEx(); }

	inline bool IsTypeOk() const { return ( (byType & 0x80) == 0x80 ) && ( (byType & 0xf0) != 0xf0 ); }

	inline bool IsSameChannel ( const AkMidiEventEx& in_other ) const 
	{
		return byChan == in_other.byChan; 
	}

	inline bool IsNoteEvent () const
	{
		return byType == AK_MIDI_EVENT_TYPE_NOTE_ON ||
			byType == AK_MIDI_EVENT_TYPE_NOTE_OFF ||
			byType == AK_MIDI_EVENT_TYPE_NOTE_AFTERTOUCH;
	}

	inline bool IsCcEvent() const
	{
		return byType == AK_MIDI_EVENT_TYPE_CONTROLLER;
	}

	inline bool IsSameCc( const AkMidiEventEx& in_other ) const
	{ 
		return IsCcEvent() && in_other.IsCcEvent() && 
			Cc.byCc == in_other.Cc.byCc; 
	}

	inline bool IsSameNote ( const AkMidiEventEx& in_other ) const
	{ 
		return IsNoteEvent() && in_other.IsNoteEvent() && 
			NoteOnOff.byNote == in_other.NoteOnOff.byNote; 
	}

	inline bool IsSameChannelAndNote ( const AkMidiEventEx& in_other ) const
	{
		return IsSameChannel( in_other ) && IsSameNote( in_other );
	}

	inline bool IsNoteOn() const
	{
		return ( byType == AK_MIDI_EVENT_TYPE_NOTE_ON && NoteOnOff.byVelocity > 0 );
	}

	inline bool IsNoteOff() const
	{
		return byType == AK_MIDI_EVENT_TYPE_NOTE_OFF || 
			(byType == AK_MIDI_EVENT_TYPE_NOTE_ON && NoteOnOff.byVelocity == 0 );
	}

	inline bool IsNoteOnOff() const
	{
		return IsNoteOn() || IsNoteOff();
	}

	bool IsPitchBend() const
	{
		return byType == AK_MIDI_EVENT_TYPE_PITCH_BEND;
	}

	inline AkMidiNoteChannelPair GetNoteAndChannel() const
	{
		AkMidiNoteChannelPair noteAndCh;
		noteAndCh.channel = byChan;
		noteAndCh.note =  IsNoteEvent() ? NoteOnOff.byNote : AK_INVALID_MIDI_NOTE;
		return noteAndCh;
	}

	inline void MakeSustainPedalOff( AkUInt32 in_uChan )
	{
		byType = AK_MIDI_EVENT_TYPE_CONTROLLER;
		byChan = (AkUInt8)in_uChan;
		Cc.byCc = AK_MIDI_CC_HOLD_PEDAL;
		Cc.byValue = 0;
	}

	inline void MakeNoteOn()
	{
		byType = AK_MIDI_EVENT_TYPE_NOTE_ON;
		NoteOnOff.byVelocity = 0;
	}

	inline void MakeNoteOff()
	{
		byType = AK_MIDI_EVENT_TYPE_NOTE_OFF;
		NoteOnOff.byVelocity = 0;
	}

	inline void SetNoteNumber( const AkMidiNoteNo in_note )
	{
		//AKASSERT( IsNoteOnOff() );
		NoteOnOff.byNote = in_note;
	}

	inline AkMidiNoteNo GetNoteNumber() const
	{
		//AKASSERT( IsNoteOnOff() );
		return (AkMidiNoteNo)NoteOnOff.byNote;
	}
};


// struct to hold header chunk info
struct AkMidiHdr
{
	void Clear()
	{
		uiFileFormat = 0;
		uiNumTracks = 0;
		uiTicksPerBeat = 0;
	}

	AkMidiHdr()
	{
		Clear();
	}

	AkUInt32		uiFileFormat;		// Format of MIDI file
	AkUInt32		uiNumTracks;		// Number of track chunks in file
	AkUInt32		uiTicksPerBeat;		// Time division in ticks per 1/4 note; valid only if not 0
};


// Struct to hold track chunk info
struct AkMidiTrack
{
	AkMidiTrack()
		: byTrackNum(0)
		, pbyBegin(0)
		, pbyEnd(0)
		, pbySeekEvent(0)
		, uiSeekDelta(0)
		, byRunStatus(AK_MIDI_EVENT_TYPE_INVALID)
	{}

	void Clear()
	{
		byTrackNum = 0;
		pbyBegin = NULL;
		pbyEnd = NULL;
		pbySeekEvent = NULL;
		uiSeekDelta = 0;
		byRunStatus = AK_MIDI_EVENT_TYPE_INVALID;
	}

	AkUInt8		byTrackNum;

	AkUInt8		*pbyBegin;		// Points to first byte of track
	AkUInt8		*pbyEnd;		// Points to first byte after track

	AkUInt8		*pbySeekEvent;		// Points to first byte of event seek (actual event, not time delta)
	AkUInt32	uiSeekDelta;		// Delta time left before event *pbySeek is the next event in track merge
	AkUInt8		abySeekExtract[6];	// Seek event extracted

	AkUInt8		byRunStatus;	// Running status when parsing
};


#endif
