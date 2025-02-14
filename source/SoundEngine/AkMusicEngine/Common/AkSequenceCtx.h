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
// AkSequenceCtx.h
//
// Music RanSeq Container Context.
//
//////////////////////////////////////////////////////////////////////
#ifndef _SEQUENCE_CTX_H_
#define _SEQUENCE_CTX_H_

#include "AkSegmentChain.h"
#include "AkRSIterator.h"

class CAkSequenceCtx : public CAkChainCtx
{
public:
    CAkSequenceCtx(
        CAkMusicRanSeqCntr *in_pSequenceNode,
        CAkMusicCtx *       in_pParentCtx
        );
    virtual ~CAkSequenceCtx();

    // Initializes the context. Automatically performs a one-segment look-ahead.
    // Returns first segment that was scheduled in the sequencer.
	// IMPORTANT. OnStops itself on failure.
    AKRESULT Init(
        CAkRegisteredObj *  in_GameObject,
        UserParams &        in_rUserparams,
		bool				in_bPlayDirectly
        );
	// Called by parent (switches): completes the initialization.
	// Sequences set their iterator ready.
	virtual void EndInit();


    // Matrix Aware Context implementation.
    // ----------------------------------------------------------

    // For Music Renderer's music contexts look-up.
    virtual CAkMusicNode * Node();

    CAkMusicRanSeqCntr * SequenceNode() { return m_pSequenceNode; }
    // ----------------------------------------------------------
    
    // CAkChainCtx implementation
    // ----------------------------------------------------------
	// Schedule a new item and append it to the chain.
    virtual bool Grow();

	// Chain is grown until something has been scheduled at in_iDesiredPosition, and there are 
	// at least AK_MINIMAL_PLAYLIST_LOOK_AHEAD items ahead of this position.
	virtual void UpdateChainLength(
		AkInt64 in_iDesiredPosition			// Desired position.
		);

	// Put the chain in a state where it will not be able to grow anymore, after a fatal error, 
	// and return the last valid item (NULL if none): set the iterator to invalid.
	virtual CAkScheduledItem * HandleFatalError();

	// Jump to element in playlist, described by playlist ID.
	/// REVIEW Will also alternatively accept a full playlist state/iterator.
	virtual AKRESULT PlaylistJump(
		AkUniqueID in_uJumpToID,						// Desired playlist ID. 
		const AkMusicTransDestRule * in_pRule,			// Transition rule used to get to the desired playlist item (may be null)
		const CAkMusicPlaybackHistory * in_pPlaybackHistory	// Playback history, used when transitioning to a playlist we've played before, and synching to the last played segment, or somewhere close
	);

	virtual void NotifyItemSync( CAkScheduledItem* in_pItem );
	virtual void NotifyItemFlush( CAkScheduledItem* in_pItem );

	// Returns false if the playlist iterator was made invalid because of a memory failure.
	virtual bool IsValid();

#ifndef AK_OPTIMIZED
	// Function to retrieve the play target (the node which was played, resulting in a music PBI)
	virtual AkUniqueID GetPlayTargetID( AkUniqueID in_playTargetID ) const;
#endif

private:

	// Searches parent ctx for playback history of this node.
	const CAkMusicPlaybackHistory* PlaybackHistory() const;

	// Have we reached the end of the playlist?
	bool EndOfPlaylistReached();

    // Returns new the first new item that was scheduled.
    CAkScheduledItem * ScheduleNextSegment( 
		bool & out_bPlayPreEntry				// True if rule required playing first item's pre-entry.
		);

	// Used to schedule nothing when resuming an ended playlist
	CAkScheduledItem * ScheduleNoSegment();

    // Appends a bucket to the chain following a music transition rule.
    CAkScheduledItem * AppendItem( 
        const AkMusicTransitionRule & in_rule,  // Transition rule between source bucket (in_pSrcBucket) and next one.
        CAkScheduledItem * in_pSrcItem,			// Item after which a new item is scheduled (NULL if first).
        AkUniqueID in_nextID,                   // Segment ID of the bucket to append to the chain.
        AkUniqueID in_playlistItemID            // Playlist item ID of the bucket to append to the chain.
        );

	// Initializes playlist iterator so that it points to the top-level node of the playlist that corresponds
    // to the in_uJumpToIdx. Flushes sequencer look-ahead if necessary.
    // Can only be called before the sequence starts playing.
    // Returns the new first segment bucket (of the sequence).
	CAkScheduledItem * JumpToSegment(
        AkUniqueID in_uJumpToID,				// Top-level playlist jump index.
		const CAkMusicPlaybackHistory* in_pHistory	// Playlist playback history
        );

	CAkScheduledItem * ResumePlayback(
		AkJumpToSelType in_eJumpType,
		const CAkMusicPlaybackHistory* in_pHistory	// Playlist playback history
	);

	// Grow.
	CAkScheduledItem * _Grow();

protected:
	virtual void OnLastFrame( AkUInt32 in_uNumSamples );

	virtual void SetPBIFade(void * in_pOwner, AkReal32 in_fFadeRatio);

private:
    CAkMusicRanSeqCntr *        m_pSequenceNode;
    
    // Current playlist iterator
	AkRSIterator				m_iterator;

	bool						m_bIsChainValid;	// False if growth of chain has failed because of a memory failure.
};


#endif //_SEQUENCE_CTX_H_
