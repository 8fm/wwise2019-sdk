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
// AkMidiBaseMgr.h
//
// Base context class for all parent contexts.
// Propagates commands to its children. Implements child removal.
//
// NOTE: Only music contexts are parent contexts, so this class is
// defined here. Move to AkAudioEngine if other standard nodes use them.
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_MIDI_BASE_MGR_H_
#define _AK_MIDI_BASE_MGR_H_

#include <AK/Tools/Common/AkListBare.h>
#include <AK/Tools/Common/AkArray.h>
#include <AK/MusicEngine/Common/AkMusicEngine.h>
#include "AkMidiBaseCtx.h"
#include "AkMidiNoteCtx.h"

class CAkMusicCtx;
class CAkMusicTrack;
class CAkMusicSource;
class CAkRegisteredObj;
class CAkMidiBaseCtx;
class CAkMidiNoteEvent;


//-------------------------------------------------------------------
// Class CAkMidiBaseMgr: Singleton. Manages top-level music contexts,
// routes game actions to music contexts, acts as an external Behavioral 
// Extension and State Handler, and as a music PBI factory.
//-------------------------------------------------------------------
class CAkMidiBaseMgr : public CAkMidiNoteStateAware
{
public:

	CAkMidiBaseMgr();
    ~CAkMidiBaseMgr();

	void AddMidiEvent( CAkMidiBaseCtx* in_pMidiCtx, const AkMidiEventEx & in_event, AkUInt32 in_uFrameOffset );
	void CleanupActions( CAkMidiBaseCtx* in_pMidiCtx );

	void OnPaused( CAkMidiBaseCtx* in_pMidiCtx );
	void OnResumed( CAkMidiBaseCtx* in_pMidiCtx );

	void AttachCtx( CAkMidiBaseCtx* in_pCtx );
	void DetachCtx( CAkMidiBaseCtx* in_pCtx );

	virtual void OnNoteStateFinished( CAkMidiNoteState* in_pNote );

	void KillAllNotes( CAkMidiBaseCtx* in_pMidiCtx );

protected:

	typedef AkListBareLight< CAkMidiBaseCtx > MidiCtxList;
	MidiCtxList m_listMidiCtx;

	typedef AkListBare< CAkMidiNoteEvent > MidiNoteList;
	struct TargetInfo
	{
		TargetInfo() : m_usSustainPedals( 0 ) {}
		MidiNoteList	m_listMidiNotes;
		AkUInt16		m_usSustainPedals; // Bitfield for sustain pedal on each channel
	};
	typedef CAkKeyArray< CAkParameterNodeBase*, TargetInfo > TargetInfoMap;
	TargetInfoMap m_mapTargetInfo;

	// Keep a list of note-off events.  The list will be checked each frame.  If a note-off
	// event is flushed if it has no attached PBIs.
	MidiNoteList m_listNoteOffs;

	// Get ctx from map, using target node.  It must exist.
	inline TargetInfo* GetTargetInfo( CAkParameterNodeBase* in_pTargetNode )
	{
		// Check if key exists
		AKASSERT( in_pTargetNode );
		return m_mapTargetInfo.Exists( in_pTargetNode );
	}

	void CleanupTargetInfo();

	void ScheduleMidiEvents( MidiFrameEventList& in_listEvents, AkUInt32 in_uNumSamples );
	void ScheduleMidiEvent( CAkMidiBaseCtx* in_pMidiCtx, const AkMidiEventEx & in_event, AkUInt32 in_uFrameOffset, bool in_bPairWithCtx, bool in_bMonitorEvent = true );
	virtual void UpdateOnFirstNote( MidiFrameEventList& in_list, AkMidiFrameEvent* in_pEvent ) = 0;
	void AddToMidiNoteList( MidiNoteList& in_listMidiNotes, CAkMidiNoteEvent* in_pNoteCtx );
	bool PairNoteOffInList( MidiNoteList& in_rNoteList, CAkMidiNoteEvent* in_pNoteCtx, bool in_bPairWithCtx );
	void UpdateMidiNotes( AkUInt32 in_uElapsedFrames );
	void CleanupNoteOffs( CAkMidiBaseCtx* in_pMidiCtx );
	void CleanupNoteOffs( bool in_bCheckFinished );

	void CcUpdateMidiNotes( TargetInfo& io_rTargetInfo, CAkMidiNoteEvent* in_pMidiNote );
	void CcScheduleMidiEvents( TargetInfo& io_rTargetInfo, CAkMidiNoteEvent* in_pMidiNote, AkUInt32 in_uFrameOffset );
	void KillNotes( CAkMidiBaseCtx* in_pMidiCtx, AkUInt32 in_uNumSamples, AkMidiChannelNo in_uChannel );
};

#endif
