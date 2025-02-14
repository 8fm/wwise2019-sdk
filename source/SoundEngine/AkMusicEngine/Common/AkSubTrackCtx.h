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
// AkTrackCtx.h
//
// Track context.
//
//////////////////////////////////////////////////////////////////////
#ifndef _SUB_TRACK_CTX_H_
#define _SUB_TRACK_CTX_H_

#include "AkContextualMusicSequencer.h"
#include "AkModulatorPBIData.h"                        // for CAkModulatorData
#include "AkMusicCtx.h"                                // for CAkMusicCtx

class CAkSegmentCtx;

// -------------------------------------------------------------------
// Name: CAkSubTrackCtx
// Desc: Child of CAkSegmentCtx, made to manage clip playback of
//			a sub-track.
// -------------------------------------------------------------------
class CAkSubTrackCtx : public CAkMusicCtx
{
public:

	CAkSubTrackCtx( CAkMusicCtx* in_pParentCtx, CAkMusicTrack* in_pTrackNode, AkUInt32 in_uSubTrack );
	virtual ~CAkSubTrackCtx();

	static CAkSubTrackCtx* Create( CAkMusicCtx* in_pParentCtx, CAkMusicTrack* in_pTrackNode, AkUInt32 in_uSubTrack );
	void Init( CAkRegisteredObj* in_GameObject, UserParams& in_rUserparams, bool in_bPlayDirectly );
	void Prepare( AkInt32 in_iAudibleTime ); // Time is relative to pre-entry

	inline CAkMusicTrack* Track() const { return m_pTrackNode; }
	inline AkUInt32 SubTrack() const { return m_uSubTrack; }


	// Access to modulator resources.
	inline CAkModulatorData& ModulatorPBIData() { return m_ModulatorData; }

	// Request to trigger all associated modulators (called during live edit only).
	virtual void OnTriggerModulators();

#ifndef AK_OPTIMIZED
	// Function to retrieve the play target (the node which was played, resulting in a music PBI)
	virtual AkUniqueID GetPlayTargetID( AkUniqueID in_playTargetID ) const;
#endif

private:

	CAkSegmentCtx* SegmentCtx() const;

public:

	// ----------------------------------------------------------------------------------------------
	// PBI notifications.
	// ----------------------------------------------------------------------------------------------

	// Called when PBI destruction occurred from Lower engine without the higher-level hierarchy knowing it.
	// Remove all references to this target from sequencer.
	void RemoveReferences( CAkChildCtx* in_pCtx );

	// Computes source offset (in file) and frame offset (in pipeline time, relative to now) required to make a given source
	// restart (become physical) sample-accurately.
	// Returns false if restarting is not possible because of the timing constraints.
	bool GetSourceInfoForPlaybackRestart(
		const CAkMusicPBI* in_pCtx,		// Context which became physical.
		AkInt32& out_iLookAhead,		// Returned required look-ahead time (frame offset).
		AkInt32& out_iSourceOffset );	// Returned required source offset ("core" sample-accurate offset in source).

	// ----------------------------------------------------------------------------------------------
	// Functions called from segment ctx.
	// ----------------------------------------------------------------------------------------------
	void Process(
		AkInt32		in_iTime,				// Current time (in samples) relative to the pre-entry.
		AkUInt32	in_uNumSamples,			// Time window size.
		AkReal32	in_fPlaybackSpeed );	// Playback speed.

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

	void Flush();

	virtual void SetPBIFade(void * in_pOwner, AkReal32 in_fFadeRatio);

private:
	void ScheduleSequencerCmds();
	void ExecuteSequencerCmds(
		AkInt32		in_iTime,				// Current time (relative to pre-entry).
		AkUInt32	in_uNumSamples,			// Time window size.
		AkReal32	in_fPlaybackSpeed );	// Playback speed.

	bool CreatePlayCtx(
		AkMusicActionPlayClip*	in_pActionPlay,
		const AkTrackSrc&		in_rSrcInfo,
		const AkUInt32			in_uWindowOffset,
		const AkReal32			in_fPlaybackSpeed,
		CAkChildCtx*&			out_pSrcCtx );

	inline AkMusicActionPlayClip* AddSequencerPlayCmd( AkInt32 in_iActionTime, const AkTrackSrc& in_rTrackSrc, AkUInt32 in_uPlayDuration, AkUInt32 in_uSourceOffset, AkUInt32 in_uPlayOffset )
	{
		AkMusicActionPlayClip* pAction = AkNew(AkMemID_Object, AkMusicActionPlayClip(
			in_iActionTime, in_rTrackSrc, in_uPlayDuration, in_uSourceOffset, in_uPlayOffset ) );
		if( pAction )
			m_sequencer.ScheduleAction( pAction );
		return pAction;
	}

	inline AkMusicActionStopClip* AddSequencerStopCmd( AkInt32 in_iActionTime, CAkChildCtx* in_pTargetPBI )
	{
		AkMusicActionStopClip* pAction = AkNew(AkMemID_Object, AkMusicActionStopClip( in_iActionTime, in_pTargetPBI ) );
		if( pAction )
			m_sequencer.ScheduleAction( pAction );
		return pAction;
	}

	inline AkMusicActionPostEvent* AddSequencerPostEventCmd( AkInt32 in_iActionTime, AkUniqueID in_eventID )
	{
		AkMusicActionPostEvent* pAction = AkNew(AkMemID_Object, AkMusicActionPostEvent( in_iActionTime, in_eventID ) );
		if( pAction )
			m_sequencer.ScheduleAction( pAction );
		return pAction;
	}

	CAkMusicClipSequencer m_sequencer;

private:
	typedef AkListBareLight<AkMusicAutomation> AutomationList;
	AutomationList		m_listAutomation;

protected:
	CAkMusicTrack*	m_pTrackNode;
	AkUInt32		m_uSubTrack;

	CAkModulatorData m_ModulatorData;

public:
	CAkSubTrackCtx* pNextLightItem;

private:
	// Time when this sub-track context will be audible, relative to its pre-entry.
	// It is set by Prepare() as the desired sub-track start position. 
	// Clips are scheduled in such a way that this context is silent before m_iAudibleTime is reached.
	AkInt32			m_iAudibleTime;
	AkUInt8			m_bSequencerInitDone:1;
	AkUInt8			m_bStopReleaseDone:1;
};

#endif //_SUB_TRACK_CTX_H_
