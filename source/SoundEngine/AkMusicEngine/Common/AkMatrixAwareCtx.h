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

#ifndef _MATRIX_AWARE_CTX_H_
#define _MATRIX_AWARE_CTX_H_

#include "AkMusicCtx.h"

struct AkMusicSeekInfo;
struct AkSegmentInfo;
class CAkMusicPackedHistory;
class CAkMusicPlaybackHistory;
class AkMusicTransDestRule;
class CAkScheduleWindow;
class CAkMatrixSequencer;
class CAkMusicSegment;
class CAkMusicNode;

// -------------------------------------------------------------------
// Name: AkCutoffInfo
// Desc: Structure for chain cutoff information. Chain cutoff is the 
//		process by which items (segments) in a chain are scheduled but not 
//		played: When performing a transition from a segment, the content of
//		following segments (pre-entries, basically) should not be heard.
// -------------------------------------------------------------------
struct AkCutoffInfo
{
	AkCutoffInfo() : iCutoffTime( 0 ), bCutoff( false ) {}

	// During context processing, merge the value of the cutoff info that is passed by parents 
	// with that of the local context. The resulting value is the smallest of both values,
	// if cutoff is required.
	void Merge( const AkCutoffInfo & in_otherCutoffInfo )
	{
		if ( in_otherCutoffInfo.MustCutoff() )
			Set( in_otherCutoffInfo.CutoffTime() );
	}

	// Set the time after which scheduled item should not be heard.
	void Set( AkInt64 in_iCutoffTime )
	{
		if ( !bCutoff )
		{
			iCutoffTime = in_iCutoffTime;
			bCutoff = true;
		}
		else if ( in_iCutoffTime < iCutoffTime )
			iCutoffTime = in_iCutoffTime;
	}
	
	// Cancel cutoff.
	inline void Reset()
	{
		bCutoff = false;
	}

	// Time conversion of cutoff value from parent time to local time.
	inline void ParentToLocalTime( AkUInt64 in_uTimeOffset )
	{
		iCutoffTime -= in_uTimeOffset;
	}
	
	inline AkInt64 CutoffTime() const
	{
		return iCutoffTime;
	}
	
	inline bool MustCutoff() const
	{
		return bCutoff;
	}
	
private:
	AkInt64 iCutoffTime;
	bool	bCutoff;
};

// -------------------------------------------------------------------
// Name: CAkMatrixAwareCtx
// Desc: Base class for high-level music contexts. 
// -------------------------------------------------------------------
class CAkMatrixAwareCtx : public CAkMusicCtx
{
public:
	CAkMatrixAwareCtx(
        CAkMusicCtx *   in_parent = NULL        // Parent context. NULL if this is a top-level context.
        );
    virtual ~CAkMatrixAwareCtx();

    AKRESULT Init(
        CAkRegisteredObj *  in_pGameObj,
        UserParams &    in_rUserparams,
		bool in_bPlayDirectly
        );

	// Get the duration while this context will be silent. It corresponds to the look-ahead duration 
	// of this context that is strictly due to streaming.
	AkUInt32 GetSilentDuration();
	
	// Sequencer access.
	inline CAkMatrixSequencer * Sequencer() { return m_pSequencer; }

	// Get sync time of this context in the time coordinates of its parent.
	inline AkInt64 SyncTime() { return m_iLocalTime; }

	// Set sync time of this context in the time coordinates of its parent.
	inline void Schedule( AkInt64 in_iSyncTime ) { AKASSERT( in_iSyncTime >= 0 ); m_iLocalTime = in_iSyncTime; }

	// Get segment node of first segment of this context. Should be called before context starts playing.
	// Returns NULL if <nothing>.
	CAkMusicSegment * GetFirstSegmentNode(
		CAkMusicNode ** out_ppParentNode = NULL // Returned parent node. Pass NULL to ignore.
		);

	//
	// Overridable methods.
	// ----------------------------------------------

	// Called by parent (switches): completes the initialization.
	// Default implementation does nothing.
	virtual void EndInit();

	// Functions used to get/set playback history.  Playback history must be managed by top level music switch container only!
	virtual bool CanKeepPlaybackHistory() const { return false; }
	virtual bool SetPlaybackHistory( AkUniqueID in_musicNodeId, AkTimeMs in_timeLeftOff, AkUniqueID in_playlistId ) { return false; }
	virtual bool SetPlaybackHistory( AkUniqueID in_musicNodeId, const CAkMusicPackedHistory& in_history ) { return false; }
	virtual const CAkMusicPlaybackHistory* GetPlaybackHistory( AkUniqueID in_musicNodeId ) { return NULL; }

	// Process a music context at each frame. The top-level context is processed, which
	// in turn calls Process() on its children, and so on. Each level passes the current time 
	// and cutoff info in its own time coordinates, and the first thing a context must do is
	// to convert them into local time values.
	// Derived classes should call ProcessEpilogue() at the end.
    virtual void Process(
		AkInt64			in_iCurrentTime,		// Current time in the time coordinates of the caller (parent).
		AkUInt32		in_uNumSamples,			// Number of samples to process.
		AkCutoffInfo &	in_cutoffInfo			// Downstream chain cutoff info. Modified inside.
		) = 0;

	// Prepares the context according to the destination transition rule, if specified. 
	// Otherwise, prepares it at position in_iMinStartPosition. The returned look-ahead time 
	// corresponds to the time needed before reaching the synchronization point.
	// Returns the required look-ahead time if successful.
	// Returns CtxPrepare_Success if preparing was successful, CtxPrepare_SegmentChanged if the starting segment was changed
	// (normally implies re-evaluating transition rule), CtxPrepare_Failure in case of a fatal error.
	enum CtxPrepareResult
	{
		CtxPrepare_Failure,
		CtxPrepare_Success,
		CtxPrepare_SegmentChanged		
	};
	virtual CtxPrepareResult Prepare(
		const AkMusicTransDestRule * in_pRule,	// Transition destination (arrival) rule. NULL means "anywhere", in any segment.
		const CAkMusicPlaybackHistory * in_pPlaybackHistory,	// Playback history, used when transitioning to a playlist we've played before, and synching to the last played segment, or somewhere close
		AkMusicSeekInfo & io_seekingInfo,		// Data for seeking within the context. Actual seeking location may placed later according to what the rule prescribes. Seek offset will be "consumed" and returned. 
		AkInt32 & out_iRequiredLookAheadTime,	// Returned required look-ahead time.
		AkUniqueID & out_uSelectedCue,			// Returned cue this context was prepared to. AK_INVALID_UNIQUE_ID if not on a cue.
		AkUniqueID	in_uCueHashForMatchSrc = 0,	// Cue which was used in the source segment. Used only if rule is bDestMatchCueName.
		bool in_bWrapInSegment = false			// TEMP Wrap desired seek position in first segment if true (legacy behavior for SameTime transition).
		) = 0;

	// Context-specific delegates for CAkScheduleWindow. 
	// Contexts implement the behavior by which a scheduling window is moved from one segment
	// to another. They must ultimately set the appropriate scheduled item and its length.
	// If io_window had just been instantiated and its current scheduled item is not already set, 
	// either the active item or the first scheduled item should be returned (depends on CAkScheduleWindow::FirstItemOnly()).
	virtual void GetNextScheduleWindow( 
		CAkScheduleWindow & io_window,		// Current schedule window, returned as next schedule window. 
		bool in_bDoNotGrow = false			// If true, chains are not asked to grow. "False" is typically used for scheduling, whereas "True" is used to check the content of a chain.
		) = 0;

	// Call this to refresh the current window's length.
	virtual void RefreshWindow(
		CAkScheduleWindow & io_window
		) = 0;

	// Inspect context that has not started playing yet and determine the time when it will start playing,
	// relative to its sync time (out_iPlayTime, typically <= 0). Additionnally, compute the time when it 
	// will start playing and will be audible (out_iPlayTimeAudible, always >= out_iPlayTime).
	virtual void QueryLookAheadInfo(
		AkInt64 &	out_iPlayTime,			// Returned time of earliest play command (relative to context).
		AkInt64 &	out_iPlayTimeAudible	// Returned time of earliest play command that becomes audible. Always >= out_iPlayTime. Note that it may not refer to the same play action as above.
		) = 0;

    // For Music Renderer's music contexts look-up: concrete contexts must return their own node.
    virtual CAkMusicNode * Node() = 0;

	// Query for transition reversal. 
	virtual bool CanRestartPlaying() = 0;

	// Stop a context based on how much time it has played already. Should be stopped immediately
	// if it has not been playing yet, or faded out by the amount of time it has been playing.
	virtual void CancelPlayback(
		AkInt64 in_iCurrentTime		// Current time in the time coordinates of the caller (parent).
		) = 0;

	// Interface for parent switch context: trigger switch change that was delayed because of parent transition.
	virtual void PerformDelayedSwitchChange() = 0;

	// Change playback position if this is supported by the context.
	// Default implementation does not support this.
	virtual AKRESULT SeekTimeAbsolute( 
		AkTimeMs & io_position,	// Seek position, in ms, relative to Entry Cue (if applicable). Returned as effective position.
		bool in_bSnapToCue		// True if final position needs to be on a cue (Exit cue excluded).
		);
	virtual AKRESULT SeekPercent( 
		AkReal32 & io_fPercent,	// Seek position, in percentage, between Entry and Exit Cues ([0,1]). Returned as effective position.
		bool in_bSnapToCue		// True if final position needs to be on a cue (Exit cue excluded).
		);

	// Music object queries.
	// Returns the playing info of the current active segment.
	virtual AKRESULT GetPlayingSegmentInfo(
		AkSegmentInfo &	out_segmentInfo		// Returned segment info.
		);

protected:
	//
	// CAkMusicCtx
	//
	// Override OnStopped(): Need to notify sequencer in case this context is the top-level.
	virtual void OnStopped();

#ifndef AK_OPTIMIZED
	// Override OnPaused() and OnResumed(): Wwise objects pause/resume notifications.
	virtual void OnPaused();
	virtual void OnResumed();
public:
	AkUniqueID	 NodeID();
#endif

protected:

	// Services.
	// ---------------------------------------------- 

	bool IsTopLevel() { return ( NULL == m_pParentCtx ); }

	// Process prologue/epilogue.
	// Derived classes should call this function at the begin and end of their Process() method.
	inline void ProcessPrologue(
		AkInt64	&		io_iCurrentTime,	// Current time in the time coordinates of the caller (parent).
		AkUInt32 &		io_uNumSamples,		// Number of samples to process.
		AkCutoffInfo &	io_cutoffInfo		// Downstream chain cutoff info. Modified inside.
		)
	{
		io_iCurrentTime = ParentToLocalTime( io_iCurrentTime );
		io_cutoffInfo.ParentToLocalTime( m_iLocalTime );
		CAkMusicCtx::ProcessPrologue( io_uNumSamples );
	}
	
	// Handles global processing (stingers) if applicable, and stopping after last frame. 
	void ProcessEpilogue(
		AkInt64			in_iCurrentTime,		// Current local time.
		AkUInt32		in_uNumSamples			// Number of samples to process.
		);
	
	// Time coordinate conversions.
	//
	inline AkInt64 GlobalToLocalTime( AkInt64 in_iTime ) { return in_iTime - GetAbsoluteTimeOffset(); }
	inline AkInt64 ParentToLocalTime( AkInt64 in_iTime ) { return in_iTime - m_iLocalTime; }

public:
//protected:
    // Shared Segment Sequencer.
    // TODO (LX) Enforce do not set sequencer elsewhere than from Music Renderer. Enforce const.
    void SetSequencer( 
        CAkMatrixSequencer * in_pSequencer
        );
	void SetRegisteredNotif(
		AkUInt32 in_uCallbackFlags
		);

private:

	// Climb context hierarchy to compute absolute time offset of this context.
	inline AkInt64 GetAbsoluteTimeOffset()
	{
		AkInt64 iAbsoluteTimeOffset = m_iLocalTime;
		if ( Parent() )
			iAbsoluteTimeOffset += ((CAkMatrixAwareCtx*)Parent())->GetAbsoluteTimeOffset();
		return iAbsoluteTimeOffset;
	}

private:
	// Sequencer. Shared across all contexts of hierarchy.
    CAkMatrixSequencer *m_pSequencer;

	// Time offset of this context relative to its parent. From the point of view of its parent,
	// this value corresponds to this context's sync time.
	AkUInt64			m_iLocalTime;

	// ListBare usage reserved
public:
	CAkMatrixAwareCtx *	pNextTopLevelSibling;
};

#endif //_MATRIX_AWARE_CTX_H_
