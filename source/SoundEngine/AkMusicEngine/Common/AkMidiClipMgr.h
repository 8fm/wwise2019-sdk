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
// AkMidiClipMgr.h
//
// Base context class for all parent contexts.
// Propagates commands to its children. Implements child removal.
//
// NOTE: Only music contexts are parent contexts, so this class is
// defined here. Move to AkAudioEngine if other standard nodes use them.
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_MIDI_CLIP_MGR_H_
#define _AK_MIDI_CLIP_MGR_H_

#include <AK/Tools/Common/AkListBare.h>
#include <AK/Tools/Common/AkArray.h>
#include <AK/MusicEngine/Common/AkMusicEngine.h>
#include "AkMusicStructs.h"
#include "AkMidiBaseMgr.h"
#include "AkMidiClipCtx.h"

class CAkMusicCtx;
class CAkMusicTrack;
class CAkMusicSource;
class CAkRegisteredObj;
class CAkMidiClipCtx;

//-------------------------------------------------------------------
// Class CAkMidiClipMgr: Singleton. Manages top-level music contexts,
// routes game actions to music contexts, acts as an external Behavioral 
// Extension and State Handler, and as a music PBI factory.
//-------------------------------------------------------------------
class CAkMidiClipMgr : public CAkMidiBaseMgr
{
public:

	CAkMidiClipMgr();
    ~CAkMidiClipMgr();

	AKRESULT AddClipCtx(
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
		);

	void NextFrame( AkUInt32 in_uNumSamples, AkReal32 in_fPlaybackSpeed );


	void StopNoteIfUsingData(CAkUsageSlot* in_pUsageSlot, bool in_bFindOtherSlot);

protected:

	virtual void UpdateOnFirstNote( MidiFrameEventList& in_list, AkMidiFrameEvent* in_pEvent );
};

#endif
