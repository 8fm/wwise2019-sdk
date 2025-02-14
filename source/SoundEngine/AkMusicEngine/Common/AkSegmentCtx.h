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
// AkSegmentCtx.h
//
// Segment context.
//
//////////////////////////////////////////////////////////////////////
#ifndef _SEGMENT_CTX_H_
#define _SEGMENT_CTX_H_

#include "AkMusicCtx.h"
#include "AkSwitchTrackInfo.h"
#include "AkContextualMusicSequencer.h"
#include "AkMonitorData.h"

class CAkMusicSegment;
class CAkScheduledItem;

// -------------------------------------------------------------------
// Name: CAkSegmentCtx
// Desc: Low-level segment context. Never used alone, always in a 
//		 chain of scheduled items. Responsible for managing playback of 
//		 music PBIs.
// -------------------------------------------------------------------
class CAkSegmentCtx : public CAkMusicCtx
{
public:
    CAkSegmentCtx(
        CAkMusicSegment *   in_pSegmentNode,
        CAkMusicCtx *       in_pParentCtx
        );
    virtual ~CAkSegmentCtx();

    AKRESULT Init(
        CAkRegisteredObj *  in_GameObject,
        UserParams &        in_rUserparams,
		bool				in_bPlayDirectly
        );

    // Set owner.
    inline void SetOwner( CAkScheduledItem* in_pOwner ) { m_pOwner = in_pOwner; }
	inline CAkScheduledItem* GetOwner() const { return m_pOwner; }

    //
	// Context commands
	//
    // Initialize context for playback.
    // Prepare the context for playback at the given position.
    // Audible playback will start at position in_iSourceOffset (relative to EntryCue).
    // Returns the exact amount of time (samples) that will elapse between call to _Play() and 
    // beginning of playback at position in_iPosition.
	AkInt32 Prepare(
		AkInt32		in_iPosition		// Desired position in samples relative to the Entry Cue.
        );

	// Process: 
    // Instantiate and play MusicPBIs in current time window [in_iNow, in_iNow+in_uNumSamples[, 
	// schedule and executes associated stop commands.
    void Process(
		AkInt32		in_iTime,			// Current time (in samples) relative to the Entry Cue.
		AkUInt32	in_uNumSamples,		// Time window size.
		AkReal32	in_fPlaybackSpeed	// Playback speed.
		);

	// Get time when this segment becomes audible (relative to its entry cue).
	inline AkInt32 GetAudibleTime() { return m_iAudibleTime; }

    // Get remaining time during which the segment is silent. 
	// If returned value is negative, silent time is completely elapsed.
    inline AkInt32 GetRemainingSilentTime(
		AkInt32		in_iTime			// Current time (in samples) relative to the Entry Cue.
		)
	{ 
		return m_iAudibleTime - in_iTime; 
	}

	// Called by CAkSwitchTrackInfo following a switch change.
	void RescheduleSwitchTrack( CAkSwitchTrackInfo* in_pSwitchInfo );

	// Called by sub-track ctx when stopped.
    void RemoveReferences( CAkSubTrackCtx* in_pSubTrack );

    // Non-virtual counterpart, when user knows it is a segment.
    inline CAkMusicSegment * SegmentNode() const { return m_pSegmentNode; }

	// Functions used by children (track ctx).
	AkInt32 GetClipPosition();
	AkInt32 GetTimeWindowSize();

	// Access to ascendents' shared objects.
	CAkRegisteredObj *  GameObjectPtr();
	AkPlayingID		    PlayingID();
	UserParams &		GetUserParams();
	bool				PlayDirectly();

#ifndef AK_OPTIMIZED
	// Function to retrieve the play target (the node which was played, resulting in a music PBI)
	virtual AkUniqueID GetPlayTargetID( AkUniqueID in_playTargetID ) const;
#endif

protected:

	// 
	// CAkMusicCtx
	// 
	// Override MusicCtx OnPlayed(): Need to schedule audio clips.
    virtual void OnPlayed();
	
	// Override MusicCtx OnStopped(): Need to flush actions to release children and to notify for cursor.
    virtual void OnStopped();

	// Override MusicCtx OnPaused/Resumed on some platforms: Stop all sounds and prepare to restart them on resume.
	virtual void OnPaused();
#ifdef AK_STOP_MUSIC_ON_PAUSE
	virtual void OnResumed();
#endif	/// AK_STOP_MUSIC_ON_PAUSE

#ifndef AK_OPTIMIZED
	// Catch MusicCtx OnEditDirty(): Stop and reschedule all audio clips.
	virtual void OnEditDirty();
#endif

	virtual void SetPBIFade(void * in_pOwner, AkReal32 in_fFadeRatio);
	

private:
	void CreateSwitchTrackCtx();
	CAkSwitchTrackInfo* GetSwitchTrackInfo( CAkMusicTrack* in_pTrack );
	CAkSubTrackCtx* GetOrCreateSubTrackCtx( CAkMusicTrack* in_pTrack, AkUInt32 in_uSubTrack );

	// Schedule sub-track play commands.
	void ScheduleSequencerCmds();

	// Schedule sub-track play commands from current segment position.
	void RescheduleSequencerCmdsNow();

	// Execute sub-track commands.
	void ExecuteSequencerCmds(
		AkInt32		in_iTime,				// Current time (relative to entry cue).
		AkUInt32	in_uNumSamples,			// Time window size.
		AkReal32	in_fPlaybackSpeed );	// Playback speed.

	// Functions for cmd sequencer updates on switch change.
	void CleanupSequencerCmdsOnSwitch( CAkSwitchTrackInfo* in_pSwitchInfo, AkInt32 in_iStopActionTime, AkInt32 in_iPlayActionTime );
	void ScheduleSequencerStopCmdsOnSwitch( CAkSwitchTrackInfo* in_pSwitchInfo, AkInt32 in_iActionTime, const AkMusicFade& in_rFadeParams );
	void ScheduleSequencerPlayCmdsOnSwitch( CAkSwitchTrackInfo* in_pSwitchInfo, AkInt32 in_iActionTime, AkInt32 in_iSegmentTime, const AkMusicFade& in_rFadeParams );

	// Determines if the given sub-track really requires a play cmd.
	bool IsSubTrackPlayCmdNeeded( CAkMusicTrack* in_pTrack, AkUInt32 in_uSubTrack );

	// Simple helpers to add cmds to sequencer.
	inline void AddSequencerPlayCmd( AkInt32 in_iActionTime, CAkMusicTrack* in_pTrack, AkUInt32 in_uSubTrack, const AkMusicFade& in_fadeParams, AkInt32 in_iSegmentPosition )
	{
		AkMusicActionPlaySubTrack* pAction = AkNew( AkMemID_Object, AkMusicActionPlaySubTrack(
			in_iActionTime, in_pTrack, in_uSubTrack, in_fadeParams, in_iSegmentPosition ) );
		if( pAction )
			m_sequencer.ScheduleAction( pAction );
	}

	inline void AddSequencerStopCmd( AkInt32 in_iActionTime, CAkSubTrackCtx* in_pTrackCtx, const AkMusicFade& in_fadeParams )
	{
		AkMusicActionStopSubTrack* pAction = AkNew(AkMemID_Object, AkMusicActionStopSubTrack(
			in_iActionTime, in_pTrackCtx, in_fadeParams ) );
		if( pAction )
			m_sequencer.ScheduleAction( pAction );
	}

	// Convert clip data time (relative to beginning of pre-entry) to segment time (relative to entry cue).
	AkInt32 ClipDataToSegmentTime(
		AkInt32		in_iClipDataTime	// Time, relative to beginning of pre-entry.
		);

	// Convert segment time (relative to entry cue) to clip data time (relative to beginning of pre-entry).
	AkInt32 SegmentTimeToClipData(
		AkInt32		in_iSegmentTime	// Time, relative to entry cue.
		);

	// Flush sequencer commands and automation.
	void Flush();

	// Updates list of pending switch track transitions
	void AddSwitchTrackNotif( AkInt32 in_iSegmentPosition, CAkMusicTrack* in_pTrack );
	void ResolveSwitchTrackNotif( AkMusicActionSwitchNotif* in_pNotif );

private:

    CAkMusicSubTrackSequencer m_sequencer;

    CAkMusicSegment*	m_pSegmentNode;    
    CAkScheduledItem*	m_pOwner;

	SwitchTrackInfoList		m_listSwitchTrack;

    // Time when this segment context will be audible, relative to its entry cue.
	// It is set by Prepare() as the desired segment start position. 
	// Audio clips are scheduled in such a way that this context is silent before m_iAudibleTime is reached.
    AkInt32			m_iAudibleTime;

	// Whether we've already scheduled our clips
	bool			m_bClipsScheduled;

	void NotifyAction( AkMonitorData::NotificationReason in_eReason );

public:
	inline AkUniqueID GetPlayListItemID(){ return m_PlaylistItemID; }
	inline void SetPlayListItemID( AkUniqueID in_playlistItemID ){ m_PlaylistItemID = in_playlistItemID; }
private:
	AkUniqueID		m_PlaylistItemID;
};

#endif
