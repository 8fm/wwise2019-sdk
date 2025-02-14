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
#ifndef __AK_MIDI_PARSELIB_H__
#define __AK_MIDI_PARSELIB_H__

#include <AK/Tools/Common/AkPlatformFuncs.h>
#include "AkMidiStructs.h"

//-----------------------------------------------------------------------------
// Name: DecodeVariableLength
// Desc: Converts the MIDI variable length value to 32 bit value.  Note that a
//       variable length value must not be use more than 4 bytes, otherwise
//       the value ~0 is returned.
//-----------------------------------------------------------------------------
static inline void DecodeVariableLength
	(
		AkUInt8*	&in_rpbyParse,
		AkUInt32	&out_rulDeltaTime
	)
{
	AkUInt32	ulByte, i;

	out_rulDeltaTime = 0;

	for ( i = 0; i < 4; i++ )
	{
		out_rulDeltaTime <<= 7;
		ulByte = (AkUInt32)*in_rpbyParse++;
		out_rulDeltaTime |= ulByte & 0x7f;
		if ( (ulByte >> 7) == 0 )
			break;
	}
	if ( i == 4 )
		out_rulDeltaTime = ~0;
}

//-----------------------------------------------------------------------------
// Quick functions used by parsing function
//-----------------------------------------------------------------------------
static inline bool IsRunStatus( const AkMidiTrack * const in_pTrack )
{
	// Event type is determined by MSB nibble
	return (in_pTrack->pbySeekEvent[0] & 0x80) == 0x00;
}

static inline bool IsChanEvent( const AkMidiTrack * in_pTrack )
{
	return	(in_pTrack->abySeekExtract[0] & 0x80) == 0x80 &&
			(in_pTrack->abySeekExtract[0] & 0xf0) != 0xf0;
}

static inline AkUInt8 GetChanEvent( const AkMidiTrack * in_pTrack )
{
	return in_pTrack->abySeekExtract[0];
}

static inline bool IsNoteOn( const AkMidiTrack * in_pTrack )
{
	return (in_pTrack->abySeekExtract[0] & 0xf0) == 0x90 && in_pTrack->abySeekExtract[2] != 0;
}

static inline bool IsNoteOff( const AkMidiTrack * in_pTrack )
{
	return ( (in_pTrack->abySeekExtract[0] & 0xf0) == 0x80 ) ||
		   ( (in_pTrack->abySeekExtract[0] & 0xf0) == 0x90 && in_pTrack->abySeekExtract[2] == 0 );
}

static inline bool IsCc( const AkMidiTrack * in_pTrack )
{
	return (in_pTrack->abySeekExtract[0] & 0xf0) == 0xb0;
}

static inline AkUInt8 GetCcType( const AkMidiTrack * const in_pTrack )
{
	return in_pTrack->abySeekExtract[1];
}

static inline AkUInt8 GetCcValue( const AkMidiTrack * const in_pTrack )
{
	return in_pTrack->abySeekExtract[2];
}

static inline AkUInt8 GetEventType( const AkMidiTrack * in_pTrack )
{
	if ( (in_pTrack->abySeekExtract[0] & 0xf0) != 0xf0 )
		return in_pTrack->abySeekExtract[0] & 0xf0;
	else
		return in_pTrack->abySeekExtract[0];
}

static inline AkUInt8 GetEventNote( const AkMidiTrack * in_pTrack )
{
	return in_pTrack->abySeekExtract[1];
}

static inline AkUInt8 GetEventChan( const AkMidiTrack * in_pTrack )
{
	return in_pTrack->abySeekExtract[0] & 0x0f;
}

static inline bool IsMetaEvent( const AkMidiTrack * in_pTrack )
{
	return ( in_pTrack->abySeekExtract[0] == 0xff );
}

static inline AkUInt8 GetMetaType( const AkMidiTrack * const in_pTrack )
{
	return in_pTrack->abySeekExtract[1];
}

static inline bool IsTempoEvent( const AkMidiTrack * in_pTrack )
{
	return IsMetaEvent( in_pTrack ) && GetMetaType( in_pTrack ) == AK_MIDI_META_EVENT_TYPE_SET_TEMPO;
}

static inline AkUInt32 GetMetaTempo( const AkMidiTrack * const in_pTrack )
{
	return	((AkUInt32)in_pTrack->abySeekExtract[3] << 16) |
			((AkUInt32)in_pTrack->abySeekExtract[4] << 8)  |
			((AkUInt32)in_pTrack->abySeekExtract[5] << 0)  ;
}

static inline bool IsEndOfTrackEvent( const AkMidiTrack * in_pTrack )
{
	return IsMetaEvent( in_pTrack ) && GetMetaType( in_pTrack ) == AK_MIDI_META_EVENT_TYPE_END_OF_TRACK;
}

static inline bool IsSysexEvent( const AkMidiTrack * in_pTrack )
{
	return ( in_pTrack->abySeekExtract[0] == 0xf0 || in_pTrack->abySeekExtract[0] == 0xf7 );
}

//-----------------------------------------------------------------------------
// Name: GetEventSize
// Desc: Moves pointer past the current event; sets pointer to NULL on error.
//-----------------------------------------------------------------------------
static inline AkUInt32 GetEventSize( AkMidiTrack * const in_pTrack )
{
	// This is either: system-exlusive, meta, or unknown
	if ( !IsChanEvent( in_pTrack ) )
	{
		if ( IsMetaEvent( in_pTrack ) || IsSysexEvent( in_pTrack ) )
		{
			// Skip over event type
			AkUInt8 * pbyEvent = &in_pTrack->abySeekExtract[1];

			// Skip meta event type
			if ( IsMetaEvent( in_pTrack ) )
				pbyEvent++;

			// Get event data length
			AkUInt32 ulEventSize;
			DecodeVariableLength( pbyEvent, ulEventSize );
			if ( ulEventSize == ~0 || (pbyEvent + ulEventSize) < pbyEvent )
				return ~0;

			AkUInt32 uiLeadSize = (AkUInt32)(pbyEvent - in_pTrack->abySeekExtract);
			return ulEventSize + uiLeadSize;
		}
		else
		{
			// Unknown event type
		}
	}
	else
	{
		// This is a channel/midi event
		AkUInt8	byEventType = GetEventType( in_pTrack );
		AkUInt8	byCcType = GetCcType( in_pTrack );

		if ( byEventType == AK_MIDI_EVENT_TYPE_INVALID )
			return ~0;
		if ( byEventType == AK_MIDI_EVENT_TYPE_CONTROLLER && byCcType >= AK_MIDI_NUM_VALID_CC )
			return ~0;

		// Skip over event params
		if ( byEventType == AK_MIDI_EVENT_TYPE_PROGRAM_CHANGE ||
			 byEventType == AK_MIDI_EVENT_TYPE_CHANNEL_AFTERTOUCH )
			return 2;
		else
			return 3;
	}

	return ~0;
}

//-----------------------------------------------------------------------------
// Name: SkipOverEvent
// Desc: Moves pointer past the current event; sets pointer to NULL on error.
//-----------------------------------------------------------------------------
static inline void SkipOverEvent( AkMidiTrack * const in_pTrack )
{

	// Get event length
	AkUInt32 ulEventSize = GetEventSize( in_pTrack );
	if ( ulEventSize == ~0 )
	{
		in_pTrack->pbySeekEvent = NULL;
		return;
	}

	// Must reduce size by 1 if a channel event in running status
	if ( IsChanEvent( in_pTrack ) && IsRunStatus( in_pTrack ) )
		--ulEventSize;

	// Skip past event and check bounds
	in_pTrack->pbySeekEvent += ulEventSize;
	if ( in_pTrack->pbySeekEvent > in_pTrack->pbyEnd )
		in_pTrack->pbySeekEvent = NULL;
}

//-----------------------------------------------------------------------------
// Name: VarLenEncode
// Desc: Writes value in variable length
//-----------------------------------------------------------------------------
static inline void VarLenEncode
		(
			AkUInt32	in_uValue,
			AkUInt8		*&out_rpbyOut
		)
{
	in_uValue <<= 4;
	if ( in_uValue != 0 )
	{
		AkUInt32 i;
		for ( i = 0; i < 4; i++ )
		{
			if ( (in_uValue >> 25) != 0 )
				break;
			in_uValue <<= 7;
		}
		for ( ; i < 4; i++ )
		{
			*out_rpbyOut++ = (AkUInt8)(in_uValue >> 25) | (i == 3 ? 0x00 : 0x80);
			in_uValue <<= 7;
		}
	}
	else
	{
		*out_rpbyOut++ = 0;
	}
}

//-----------------------------------------------------------------------------
// Name: ExtractEvent
// Desc: Get full event in an array; that is, remove running status
//-----------------------------------------------------------------------------
static inline void ExtractEvent( AkMidiTrack * in_pTrack )
{
	AKASSERT( in_pTrack );

	// Make sure inputs are legit
	memset( in_pTrack->abySeekExtract, 0, sizeof in_pTrack->abySeekExtract );
	if ( ! in_pTrack->pbySeekEvent )
		return;

	// Look for either a channel or tempo event.  Any other event we just
	// copy the max number of bytes we can
	AkUInt8 * pbyExtract = in_pTrack->abySeekExtract;
	AkUInt8 byExtractSize = sizeof in_pTrack->abySeekExtract;
	if ( ((*in_pTrack->pbySeekEvent & 0x80) == 0x00) && in_pTrack->byRunStatus != AK_MIDI_EVENT_TYPE_INVALID )
	{
		*pbyExtract++ = in_pTrack->byRunStatus;
		byExtractSize--;
	}

	// Make sure we never copy data that's outside of the current track
	if ( in_pTrack->pbySeekEvent + byExtractSize > in_pTrack->pbyEnd )
		byExtractSize = (AkUInt8)(in_pTrack->pbyEnd - in_pTrack->pbySeekEvent);

	memcpy( pbyExtract, in_pTrack->pbySeekEvent, byExtractSize );
}

//-----------------------------------------------------------------------------
// Name: ClearSeek
// Desc: Seek is done for the current track
//-----------------------------------------------------------------------------
static void ClearSeek( AkMidiTrack * in_pTrack )
{
	in_pTrack->pbySeekEvent = NULL;
	memset( in_pTrack->abySeekExtract, 0, sizeof in_pTrack->abySeekExtract );
	in_pTrack->byRunStatus = AK_MIDI_EVENT_TYPE_INVALID;
	in_pTrack->uiSeekDelta = ~0;
}

//-----------------------------------------------------------------------------
// Name: ResetSeek
// Desc: Resets all members used for seek functions
//-----------------------------------------------------------------------------
static inline bool ResetSeek( AkMidiTrack * in_pTracks, const AkUInt32 in_uiNumTracks )
{
	// Have every track point to its first event
	for ( AkUInt32 uiTrackIdx = 0; uiTrackIdx < in_uiNumTracks; uiTrackIdx++ )
	{
		AkMidiTrack *pTrack = &in_pTracks[ uiTrackIdx ];
		pTrack->byRunStatus = AK_MIDI_EVENT_TYPE_INVALID;
		pTrack->pbySeekEvent = pTrack->pbyBegin;
		pTrack->uiSeekDelta = ~0;

		if ( pTrack->pbySeekEvent )
		{
			DecodeVariableLength( pTrack->pbySeekEvent, pTrack->uiSeekDelta );
			if ( pTrack->uiSeekDelta == ~0 || pTrack->pbySeekEvent >= pTrack->pbyEnd )
				return false;
			ExtractEvent( pTrack );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Name: SeekNextEvent
// Desc: Sets the internal event pointer to the next event, and returns a
//       pointer to the unformatted data.
//-----------------------------------------------------------------------------
static bool SeekNextEvent
	(
		AkMidiTrack	*in_pTrack
	)
{
	AKASSERT( ! in_pTrack->pbySeekEvent || (in_pTrack->pbySeekEvent >= in_pTrack->pbyBegin && in_pTrack->pbySeekEvent < in_pTrack->pbyEnd) );

	// No need to go on if done seeking on this track
	if ( ! in_pTrack->pbySeekEvent )
		return true;

	// Skip over event
	SkipOverEvent( in_pTrack );
	if ( ! in_pTrack->pbySeekEvent || in_pTrack->pbySeekEvent > in_pTrack->pbyEnd )
	{
		ClearSeek( in_pTrack );
		return false;
	}

	// Keep track of running status (skipped event is still in extract)
	if ( IsChanEvent( in_pTrack ) )
		in_pTrack->byRunStatus = in_pTrack->abySeekExtract[0];
	else
		in_pTrack->byRunStatus = AK_MIDI_EVENT_TYPE_INVALID;

	// Check for end of seeking on this track
	if ( in_pTrack->pbySeekEvent == in_pTrack->pbyEnd )
	{
		ClearSeek( in_pTrack );
	}
	else
	{
		// Get time delta for next event
		DecodeVariableLength( in_pTrack->pbySeekEvent, in_pTrack->uiSeekDelta );
		if ( in_pTrack->uiSeekDelta == ~0 || in_pTrack->pbySeekEvent >= in_pTrack->pbyEnd )
		{
			ClearSeek( in_pTrack );
			return false;
		}
		ExtractEvent( in_pTrack );
	}

	return true;
}

#endif //__AK_MIDI_PARSELIB_H__
