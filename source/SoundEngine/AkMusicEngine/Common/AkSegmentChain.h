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


#ifndef _SEGMENT_CHAIN_H_
#define _SEGMENT_CHAIN_H_

#include <AK/Tools/Common/AkListBare.h>
#include "AkMatrixAwareCtx.h"
#include "AkMatrixSequencer.h"

class CAkChainCtx;

//-----------------------------------------------------------------------------
// Name: AkScheduledChain
// Desc: Extends list bare of of scheduled items. Provides a self-contained iterator.
//-----------------------------------------------------------------------------
class AkScheduledChain : public AkListBare<CAkScheduledItem>
{
public:
	AkScheduledChain() {}
	~AkScheduledChain() {}

	// Iterator having a reference to its owner.
	struct SelfContainedIter : public AkListBare<CAkScheduledItem>::Iterator
	{
		SelfContainedIter() : pCtx( NULL ) { pItem = NULL; }
		SelfContainedIter( CAkChainCtx * in_pOwner ) : pCtx( in_pOwner ) {}

		inline CAkChainCtx * Owner() const { return pCtx; }
		inline bool IsSet() const { return pItem != NULL; }
		inline void Reset() { pCtx = NULL; pItem = NULL; }

		// Get the position of an item within its context.
		AkInt64 GetScheduledItemTime() const;

		// Convert time (relative to owner context) into a position relative to the pointed segment's
		// entry cue.
		AkInt64 CtxTimeToSegmentPosition(
			AkInt64		in_iTime			// Time, relative to the owner context.
			) const;
			
		// Convert given position of the segment pointed by this scheduled item into a time value,
		// relative to the owner context.
		AkInt64 SegmentPositionToCtxTime(
			AkInt64		in_iSegmentPosition	// Position of segment pointed by this scheduled item (relative to entry cue).
			) const; 

		CAkChainCtx * pCtx;
	};

	inline SelfContainedIter BeginSelfContained( CAkChainCtx * in_pOwner )
	{
		SelfContainedIter returnedIt( in_pOwner );
		returnedIt.pItem = m_pFirst;
		return returnedIt;
	}
};

//-----------------------------------------------------------------------------
// Name: CAkChainCtx
// Desc: Base class for sequencer-aware contexts that represent chain of 
//		 scheduled items. Associated to playlists, single segments and <nothing>. 
//		 It is the leaf of a given branch stack.
//-----------------------------------------------------------------------------
class CAkChainCtx : public CAkMatrixAwareCtx
{
	friend struct AkScheduledChain::SelfContainedIter;
public:
	CAkChainCtx(
        CAkMusicCtx *   in_parent = NULL        // Parent context. NULL if this is a top-level context.
        );
    virtual ~CAkChainCtx();

	//
	// Interface for low-level SegmentCtx.
	//
	AkInt32 GetSegmentPosition( CAkScheduledItem * in_pItem );

	//
	// Interface for scheduling windows.
	//
	// Returns false if the chain is compromised and scheduling logic should not take for granted
	// that the chain will be able to grow and/or finish with an item containing <nothing>.
	virtual bool IsValid() = 0;

protected:

	// 
	// CAkMusicCtx.
	// 
	// Override OnStopped(): Need to flush chain and release references.
	virtual void OnStopped();

	// 
	// CAkMatrixAwareCtx implementation.
	// 

    // Sequencer processing.
    virtual void Process(
		AkInt64			in_iCurrentTime,		// Current time in the time coordinates of the caller (parent).
		AkUInt32		in_uNumSamples,			// Number of samples to process.
		AkCutoffInfo&	in_cutoffInfo			// Downstream chain cutoff info. Modified inside.
		);

	// Prepares the context according to the destination transition rule, if specified. 
	// Otherwise, prepares it at position in_iMinStartPosition and grows the chain in order to honor 
	// the start position. The returned look-ahead time depends on all currently scheduled items 
	// (the number of scheduled items depends on the concrete context policy - for e.g. playlists schedule 3 items ahead).
	// Returns the required look-ahead time. 
	// Returns CtxPrepare_Success if preparing was successful, CtxPrepare_SegmentChanged if the starting segment was changed
	// (normally implies re-evaluating transition rule), CtxPrepare_Failure in case of a fatal error.
	virtual CtxPrepareResult Prepare(
		const AkMusicTransDestRule * in_pRule,	// Transition destination (arrival) rule. NULL means "anywhere", in any segment.
		const CAkMusicPlaybackHistory * in_pPlaybackHistory,	// Playback history, used when transitioning to a playlist we've played before, and synching to the last played segment, or somewhere close
		AkMusicSeekInfo & io_seekingInfo,		// Data for seeking within the context. Actual seeking location may placed later according to what the rule prescribes. Seek offset will be "consumed" and returned. 
		AkInt32 & out_iRequiredLookAheadTime,	// Returned required look-ahead time.
		AkUniqueID & out_uSelectedCue,			// Returned cue this context was prepared to. AK_INVALID_UNIQUE_ID if not on a cue.
		AkUniqueID	in_uCueHashForMatchSrc = 0,	// Cue which was used in the source segment. Used only if rule is bDestMatchCueName.
		bool in_bWrapInSegment = false			// TEMP Wrap desired seek position in first segment if true (legacy behavior for SameTime transition).
		);

	// Tree walking.
	virtual void GetNextScheduleWindow( 
		CAkScheduleWindow & io_window,		// Current schedule window, returned as next schedule window. If io_window had just been instantiated, the active window would be returned.
		bool in_bDoNotGrow = false			// If true, chains are not asked to grow. "False" is typically used for scheduling, whereas "True" is used to check the content of a chain.
		);

	// Call this to refresh the current window's length.
	virtual void RefreshWindow(
		CAkScheduleWindow & io_window
		);

	// Inspect context that has not started playing yet and determine the time when it will start playing,
	// relative to its sync time (out_iPlayTime, typically <= 0). Additionnally, compute the time when it 
	// will start playing and will be audible (out_iPlayTimeAudible, always >= out_iPlayTime).
	virtual void QueryLookAheadInfo(
		AkInt64 &	out_iPlayTime,			// Returned time of earliest play command (relative to context).
		AkInt64 &	out_iPlayTimeAudible	// Returned time of earliest play command that becomes audible. Always >= out_iPlayTime. Note that it may not refer to the same play action as above.
		);

	// Query for transition reversal. 
	virtual bool CanRestartPlaying();

	// Stop a context based on how much time it has played already. Should be stopped immediately
	// if it has not been playing yet, or faded out by the amount of time it has been playing.
	virtual void CancelPlayback(
		AkInt64 in_iCurrentTime		// Current time in the time coordinates of the caller (parent).
		);

	// Interface for parent switch context: trigger switch change that was delayed because of parent transition.
	// Chain contexts do not respond to this message.
	virtual void PerformDelayedSwitchChange();
	
	// 
    // Virtual methods to override.
	// 
	// Grow chain: called when the chain is required to grow (schedule another item).
	// Return true if a new item was added, false otherwise.
	// Default implementation does nothing, returns false.
    virtual bool Grow();

	// Update chain length: context is notified that it should (possibly) schedule items
	// so that something is scheduled at (at least) in_iDesiredPosition (relative to this context).
	// Default implementation does nothing.
	virtual void UpdateChainLength(
		AkInt64 in_iDesiredPosition			// Desired position.
		);

	// Put the chain in a state where it will not be able to grow anymore, after a fatal error, 
	// and return the last valid item (NULL if none).
	virtual CAkScheduledItem * HandleFatalError();

	// Jump to element in playlist, described by playlist ID.
	/// REVIEW Will also alternatively accept a full playlist state/iterator.
	// NOTE: This function is always called within temporary addref.
	virtual AKRESULT PlaylistJump(
		AkUniqueID in_uJumpToID,						// Desired playlist item ID. 
		const AkMusicTransDestRule * in_pRule,			// Transition rule used to get to the desired playlist item (may be null)
		const CAkMusicPlaybackHistory * in_pPlaybackHistory	// Playback history, used when transitioning to a playlist we've played before, and synching to the last played segment, or somewhere close
		) 
	{
		AKASSERT( !"Not implemented for this object type" );
		return AK_NotImplemented;
	}

	virtual void NotifyItemSync( CAkScheduledItem* /*in_pItem*/ ) {}
	virtual void NotifyItemFlush( CAkScheduledItem* /*in_pItem*/ ) {}

	//
	// Services.
	//
	// Create a new item and append it to the chain.
    CAkScheduledItem * EnqueueItem(
        AkUInt64		in_uTimeOffset,	// Sync time offset relative to this context.
        CAkSegmentCtx * in_pSegment		// Low-level segment context. Can be NULL (terminating chain).
        );

	// Flush all scheduled items of the chain: stop, release references, re-schedule cancelled actions.
    void Flush();

	// Flush first scheduled item (segment). Assumes that there is at least one.
	// NOTE: This function is always called within temporary addref.
	void FlushFirstItem();

	// Compute segment starting position based on a sync position and a transition rule. Used to query segment for look-ahead time. 
	// Actual start position depends on fade offset (if there is a non-zero fade) and if pre-entry should be played.
	inline AkInt32 ComputeSegmentStartPosition( AkUInt32 in_uSyncPos, AkTimeMs in_transTime, AkInt32 in_iFadeOffset, bool in_bPlayPreEntry, AkInt32 in_iPreEntryDuration )
	{
		AkInt32 iStartPosition = in_uSyncPos;
		if ( in_transTime > 0 )
		{
			iStartPosition += in_iFadeOffset;
			if ( iStartPosition < 0 && !in_bPlayPreEntry )
				iStartPosition = 0;	// Clamp at entry cue if pre-entry not played. Must not query segment in pre-entry; returned look-ahead time would be wrong.
		}
		else if ( in_bPlayPreEntry )
			iStartPosition -= in_iPreEntryDuration;

		return iStartPosition;
	}
	
protected:
    
	// Chain of scheduled items.
	AkScheduledChain	m_chain;

	// Local time to/from scheduled item chain time conversions. Scheduled items are offset from
	// local time when chain contexts are prepared.
	inline AkInt64 LocalToChainTime( AkInt64 in_iLocalTime ) const { return in_iLocalTime + m_uItemsTimeOffset; }
	inline AkInt64 ChainToLocalTime( AkInt64 in_iChainTime ) const { return in_iChainTime - m_uItemsTimeOffset; }

	// The following conversions are only accessible by this class and the chain iterator.
private:

	// Get the scheduled time of a segment relative to this context's sync time.
	inline AkInt64 GetScheduledItemTime( 
		CAkScheduledItem *	in_pItem			// Scheduled segment.
		) const
	{
		return ChainToLocalTime( in_pItem->Time() );
	}

	// Convert time (relative to this context) into a position relative to the given segment's
	// entry cue.
	inline AkInt64 CtxTimeToSegmentPosition(
		CAkScheduledItem *	in_pItem,			// Scheduled segment.
		AkInt64				in_iTime			// Time, relative to the owner context.
		) const
	{
		return in_iTime - ChainToLocalTime( in_pItem->Time() );
	}
		
	// Convert position (relative to the given scheduled segment) into a time value,
	// relative to this context's sync time.
	inline AkInt64 SegmentPositionToCtxTime(
		CAkScheduledItem *	in_pItem,			// Scheduled segment.
		AkInt64				in_iSegmentPosition	// Position of segment pointed by this scheduled item (relative to entry cue).
		) const
	{
		return in_iSegmentPosition + ChainToLocalTime( in_pItem->Time() );
	}

private:
	// Delete all items. All segment item should be NULL (that is, properly destroyed following segment ctx notification).
    void ClearChain();

private:

	// Offset of scheduled items' time relative to this context. Always >= 0.
	// A non-zero value means that the items are shifted earlier compared to this context. For example,
	// m_uItemsTimeOffset = half of first segment's duration means that the context was prepared 
	// in such a way that its sync point occurs in the middle of the first segment. 
	AkUInt32 m_uItemsTimeOffset;
};

#endif //_SEGMENT_CHAIN_H_
