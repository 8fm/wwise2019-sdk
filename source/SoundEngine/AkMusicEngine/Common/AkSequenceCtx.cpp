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
// AkSequenceCtx.cpp
//
// Sequence context.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkSequenceCtx.h"
#include "AkMusicRanSeqCntr.h"
#include "AkSegmentCtx.h"
#include "AkMusicSegment.h"
#include "AkMusicStructs.h"
#include "AkMatrixSequencer.h"
#include "AkMusicPlaybackHistory.h"
#include "AkMonitor.h"

#define AK_MINIMAL_PLAYLIST_LOOK_AHEAD	(3)

CAkSequenceCtx::CAkSequenceCtx(
    CAkMusicRanSeqCntr *in_pSequenceNode,
    CAkMusicCtx *       in_pParentCtx
    )
:CAkChainCtx( in_pParentCtx )
,m_pSequenceNode( in_pSequenceNode )
,m_bIsChainValid( true )
{
	if( m_pSequenceNode )
		m_pSequenceNode->AddRef();
}

CAkSequenceCtx::~CAkSequenceCtx()
{
	// IMPORTANT: Term() m_iterator before releasing the audio node.
	// Releasing the audio node may result in its destruction, and m_iterator
	// may hold references to objects that exist within the audio node...
	m_iterator.Term();

	if( m_pSequenceNode )
		m_pSequenceNode->Release();
}

void CAkSequenceCtx::OnLastFrame( AkUInt32 in_uNumSamples )
{
	if ( EndOfPlaylistReached() && m_listChildren.Begin() == m_listChildren.End() )
	{
		// No more playing segments, and no more scheduled playbacks
		CAkMusicPackedHistory history( m_iterator.PackIterator() );
		if ( NULL != history.Data() && NULL != Parent() && NULL != m_pSequenceNode )
		{
			CAkMatrixAwareCtx* pParent = static_cast<CAkMatrixAwareCtx*>( Parent() );
			pParent->SetPlaybackHistory( m_pSequenceNode->ID(), history );
		}
	}

	CAkChainCtx::OnLastFrame( in_uNumSamples );
}


// Initializes the context. Automatically performs a one-segment look-ahead.
// Returns first segment that was scheduled in the sequencer.
AKRESULT CAkSequenceCtx::Init(
    CAkRegisteredObj *  in_GameObject,
    UserParams &        in_rUserparams,
	bool				in_bPlayDirectly
    )
{
	AKRESULT eResult = CAkChainCtx::Init( in_GameObject, in_rUserparams, in_bPlayDirectly );
    if ( eResult == AK_Success )
    {
		// Check for existing iterator history
		const CAkMusicPlaybackHistory* history = PlaybackHistory();

		// Initialize the iterator
		eResult = m_iterator.Init( m_pSequenceNode, Sequencer()->GetUserParams().PlayingID(), history );
		if ( eResult == AK_Success )
		{
			// Print playlist items deterministically into scheduler until we have reached the requested look-ahead.
			bool bPlayPreEntry;
			CAkScheduledItem * pItem = ScheduleNextSegment( bPlayPreEntry );

			// Protect caller if there was an error enqueueing the first segment.
			if ( pItem && 
					pItem->SegmentCtx() )
			{
				// Prepare the sequence to start playing in the first segment either at entry cue 
				// or at pre-entry, depending on playlist transition rules. 
				// This is where it will start playing by default (if it is top-level).
				// Parent contexts may re-prepare us otherwise.
				AkInt32 iStartPosition = ( bPlayPreEntry ) ? -pItem->SegmentCtx()->SegmentNode()->PreEntryDuration() : 0;
				AkUniqueID uCueDest;	// ignored.
				AkInt32 iLookAheadDuration;
				AkMusicSeekInfo seekInfo = AkMusicSeekInfo( iStartPosition );
				CtxPrepareResult ePrepareResult = Prepare( NULL, NULL, seekInfo, iLookAheadDuration, uCueDest );
				eResult = ( ePrepareResult != CtxPrepare_Failure ) ? AK_Success : AK_Fail;

				// Schedule to reach its sync point in "look-ahead duration".
				// Parent may re-schedule this context at will.
				Schedule( iLookAheadDuration );
			}
			else
			{
				eResult = AK_Fail;
			}
		}
    }

    return eResult;
}

// Called by parent (switches): completes the initialization.
// Sequences set their iterator ready.
void CAkSequenceCtx::EndInit()
{
	m_iterator.EndInit();
}

const CAkMusicPlaybackHistory* CAkSequenceCtx::PlaybackHistory() const
{
	if ( Parent() )
		return static_cast<CAkMatrixAwareCtx*>( Parent() )->GetPlaybackHistory( m_pSequenceNode->ID() );
	return NULL;
}

bool CAkSequenceCtx::Grow()
{
	return _Grow() != NULL;
}

AKRESULT CAkSequenceCtx::PlaylistJump(
	AkUniqueID in_uJumpToID,						// Desired playlist ID. 
	const AkMusicTransDestRule * in_pRule,			// Transition rule used to get to the desired playlist item (may be null)
	const CAkMusicPlaybackHistory * in_pHistory	// Playback history, used when transitioning to a playlist we've played before, and synching to the last played segment, or somewhere close
)
{
	// Determine if we'll be using the playback history
	AkJumpToSelType jumpType = (AkJumpToSelType)(
		in_pRule ? in_pRule->eJumpToType : AkJumpToSelType_SpecificItem
		);

	CAkScheduledItem* pScheduled = NULL;

	// We either jump to a specific segment, or resume from last segment (...or the next one after that)
	if (
		( jumpType == AkJumpToSelType_LastPlayedSegment || jumpType == AkJumpToSelType_NextSegment )
		&& in_pHistory && in_pHistory->m_playlistHistory.Size() > 0
		)
	{
		pScheduled = ResumePlayback( jumpType, in_pHistory );
	}
	else
	{
		pScheduled = JumpToSegment( in_uJumpToID, in_pHistory );
	}

	return pScheduled ? AK_Success : AK_Fail;
}

CAkScheduledItem * CAkSequenceCtx::_Grow()
{
	// Grow if the end of the sequence was not reached yet, and if the last bucket is not
	// inhibated (a sequence cannot grow while it is "inhibated").
    CAkScheduledItem * pNewItem = NULL;
	AKASSERT( !m_chain.IsEmpty() );

    if ( m_iterator.IsValid() )
    {
		CAkScheduledItem * pLastItem = m_chain.Last();
		bool bPlayPreEntry; // unused
		pNewItem = ScheduleNextSegment( bPlayPreEntry );

		// The bucket returned could be the same as the previous LsatBucket
		// if there was a fatal error (that is, no new bucket was created).
		if ( pNewItem == pLastItem )
		{
			// There was an error.
			pNewItem = NULL;
		}
    }
    
    return pNewItem;
}

// Chain is grown until something has been scheduled at in_iDesiredPosition, and there are 
// at least AK_MINIMAL_PLAYLIST_LOOK_AHEAD items ahead of this position.
void CAkSequenceCtx::UpdateChainLength(
	AkInt64 in_iDesiredPosition			// Desired position.
	)
{
	AkUInt32 uSegmentCounter = 0;
	AkUInt32 uBucketLookAhead = 0;

	// Have at least enough items up to in_iDesiredPosition.
	AkScheduledChain::Iterator it = m_chain.Begin();
	AkScheduledChain::Iterator itPrev = it;
	while ( it != m_chain.End() 
			&& (*it)->Time() < in_iDesiredPosition )
	{
		itPrev = it;
		++it;
	}

	if ( it == m_chain.End() )
	{
		// Need to grow until we reach the desired position.
		it = itPrev;
		if ( it == m_chain.End() )
			return;	// ungrowable.

		while ( (AkInt64)(*it)->Time() < in_iDesiredPosition )
		{
			if ( !_Grow() )
				return;

			if( ++uSegmentCounter > MAX_SRC_SEGMENT_LOOK_AHEAD )
				goto break_infinite_loop;

			itPrev = it;
			++it;
			AKASSERT( it != m_chain.End() );
		}
	}

	// Ensure we have AK_MINIMAL_PLAYLIST_LOOK_AHEAD items after this.
	while ( uBucketLookAhead < AK_MINIMAL_PLAYLIST_LOOK_AHEAD )
	{
		++uBucketLookAhead;
		if ( it != m_chain.End() )
		{
			// Not the end of the chain. Check next item.
			itPrev = it;
			++it;
		}
		else
		{
			it = itPrev;
			if ( !_Grow() )
				return;

			if( ++uSegmentCounter > MAX_SRC_SEGMENT_LOOK_AHEAD )
				goto break_infinite_loop;

			// Grow succeeded. Point "it" at this new last segment, back up in "itPrev", increment.
			++it;
			itPrev = it;
			++it;
		}
	}

	return;

break_infinite_loop:

	// Fixing WG-14679: Detecting infinite or almost infinite loops.
	// Hard limit of MAX_SRC_SEGMENT_LOOK_AHEAD segment at the same frame.
	// Preventing infinite loops.
	{
		MONITOR_ERROR( AK::Monitor::ErrorCode_TooManySimultaneousMusicSegments );
		HandleFatalError();
	}
}

// For Music Renderer's music contexts look-up.
CAkMusicNode * CAkSequenceCtx::Node()
{
    return m_pSequenceNode;
}

// Initializes playlist iterator so that it points to the top-level node of the playlist that corresponds
// to the in_uJumpToIdx. Flushes sequencer look-ahead if necessary.
// Can only be called before the sequence starts playing.
// IMPORTANT: AddRef temporarily around calls to this function.
// Returns NULL if there was a fatal error.
CAkScheduledItem * CAkSequenceCtx::JumpToSegment(
    AkUniqueID in_uJumpToID,
	const CAkMusicPlaybackHistory* in_pHistory
    )
{
    AKASSERT( !IsPlaying() );

    // NOTE. in_uJumpToID is 0 when not set in UI. 
    if ( in_uJumpToID == 0 )
    {
		AKASSERT( m_chain.Begin() != m_chain.End() );
		return *m_chain.Begin();
	}

	// Must be addref'd from outside when calling this function. 
    // Otherwise, flushing all buckets would result in self-destruction.
	AKASSERTD( GetRefCount() > NumChildren() );
        
    // Flush segment(s) previously scheduled.
    Flush();

    // Prepare iterator.
	if ( m_iterator.JumpTo( in_uJumpToID, in_pHistory ) != AK_Success )
		return HandleFatalError();

	bool bPlayPreEntry; // unused
	CAkScheduledItem* pScheduled = ScheduleNextSegment( bPlayPreEntry );

	// Check content of first item after scheduling. It is illegal to have a sequence that starts 
	// with an empty item. This can occur if there was not enough memory.
	if ( pScheduled && !pScheduled->SegmentCtx() )
	{
		Flush();
		pScheduled = NULL;
	}

	return pScheduled;
}

CAkScheduledItem * CAkSequenceCtx::ResumePlayback(
	AkJumpToSelType in_eJumpType,
	const CAkMusicPlaybackHistory* in_pHistory
	)
{
	AKASSERT( AkJumpToSelType_LastPlayedSegment == in_eJumpType
		|| AkJumpToSelType_NextSegment == in_eJumpType );

	// Need to put an end to our current iterator
	m_iterator.Term();

	// Setup the iterator with the provided history
	if ( ! m_iterator.UnpackIterator( in_pHistory->m_playlistHistory ) )
		return HandleFatalError();

	// Must be addref'd from outside when calling this function. 
	// Otherwise, flushing all buckets would result in self-destruction.
	AKASSERTD( GetRefCount() > NumChildren() );

	// Flush segment(s) previously scheduled.
	Flush();

	// Now, are we gonna use the previously playing segment, or the one after
	bool bPlayPreEntry; // unused
	bool bPlaylistDone = EndOfPlaylistReached();

	CAkScheduledItem* pScheduled = NULL;

	if ( AkJumpToSelType_NextSegment == in_eJumpType && !bPlaylistDone )
	{
		m_iterator.SkipToNextSegment();
		bPlaylistDone = EndOfPlaylistReached();
	}

	if ( bPlaylistDone )
		return ScheduleNoSegment();

	pScheduled = ScheduleNextSegment( bPlayPreEntry );

	// Check content of first item after scheduling. It is illegal to have a sequence that starts 
	// with an empty item. This can occur if there was not enough memory.
	if ( !bPlaylistDone && pScheduled && !pScheduled->SegmentCtx() )
	{
		Flush();
		pScheduled = NULL;
	}

	return pScheduled;
}

bool CAkSequenceCtx::EndOfPlaylistReached()
{
	if ( ! m_iterator.IsValid() )
		return true;
	
	AkUniqueID nextID = m_iterator.GetCurrentSegment();
	CAkScheduledItem* pItem = m_chain.Last();

	return ( !pItem || !pItem->SegmentCtx() ) && nextID == AK_MUSIC_TRANSITION_RULE_ID_NONE;
}

// Returns new the first new item that was scheduled.
CAkScheduledItem * CAkSequenceCtx::ScheduleNextSegment( 
	bool & out_bPlayPreEntry 
	)
{
	out_bPlayPreEntry = false;

	if ( IsStopping() )
		return NULL;	// This operation is potentially dangerous if this context is stopping.

    // Get next segment to be played from Playlist.

	if ( !m_iterator.IsValid() )// reached end of PlayList
		return NULL;

	AkUniqueID nextID = m_iterator.GetCurrentSegment();
	AkUniqueID playlistItemID = m_iterator.GetPlaylistItem();

	m_iterator.MoveToNextSegment();

    // Get source context.
    CAkScheduledItem * pSrcItem = m_chain.Last();

	// Be careful.  If we have an item in the chain and it doesn't have a sequence ctx,
	// then it's meant to be the last, and nothing else should be scheduled!  Bail out now!
	// This change was made mostly to address WG-44930.
	if ( pSrcItem && !pSrcItem->SegmentCtx() )
		return NULL;

    // Query transition rule.
    // (Handle <NONE>)
    AkUniqueID curID = ( pSrcItem ) ? pSrcItem->SegmentCtx()->SegmentNode()->ID() : AK_MUSIC_TRANSITION_RULE_ID_NONE;
    const AkMusicTransitionRule & rule = m_pSequenceNode->GetTransitionRule( curID, nextID );

    if ( rule.pTransObj )
    {
        // There is a transition segment. Schedule it, and then schedule the "real" next one.
        // Create a new rule, using the original source rule and the Entry parameters of the transition segment.
        AkMusicTransitionRule transRule;
#ifndef AK_OPTIMIZED
		transRule.index = rule.index;
#endif
        transRule.srcRule = rule.srcRule;
        // Not used. transRule.srcID
        transRule.pTransObj = NULL;
        ///
        transRule.destRule.fadeParams = rule.pTransObj->fadeInParams;
        transRule.destRule.uCueFilterHash = AK_INVALID_UNIQUE_ID;
        transRule.destRule.eEntryType = EntryTypeEntryMarker;
        transRule.destRule.bPlayPreEntry = rule.pTransObj->bPlayPreEntry;
		transRule.destRule.bDestMatchSourceCueName = false;
		out_bPlayPreEntry = rule.pTransObj->bPlayPreEntry;
        // Not used. transRule.destID
        ///
        pSrcItem = AppendItem( transRule, pSrcItem, rule.pTransObj->segmentID, AK_INVALID_UNIQUE_ID );
		// Being unable to append an empty item is a fatal error.
        if ( pSrcItem )
		{
			// However, for a transition segment, enqueueuing a transition item that is empty is also
			// a fatal error.
			if ( !pSrcItem->SegmentCtx() )
			{
				return HandleFatalError();
			}

			// Now, append the "real" next bucket.
			// Compute a rule using the original destination rule, and the Exit parameters of the transition segment.
			///
			transRule.srcRule.fadeParams = rule.pTransObj->fadeOutParams;
			transRule.srcRule.eSyncType = SyncTypeExitMarker;
			transRule.srcRule.bPlayPostExit = rule.pTransObj->bPlayPostExit;
			// Not used. transRule.srcID = rule.srcID;
			///
			transRule.destRule = rule.destRule;
			// Not used. transRule.destID

			AppendItem( transRule, pSrcItem, nextID, playlistItemID );
		}

        // Note that we return the first bucket that was scheduled, not the last one.
        return pSrcItem;
    }
    else
	{
		out_bPlayPreEntry = rule.destRule.bPlayPreEntry;
        return AppendItem( rule, pSrcItem, nextID, playlistItemID );
	}
}

CAkScheduledItem * CAkSequenceCtx::ScheduleNoSegment()
{
	// IMPORTANT: this function should only be called when resuming an already finished playlist
	AKASSERT( IsIdle() && EndOfPlaylistReached() );

	AkUniqueID nextID = AK_MUSIC_TRANSITION_RULE_ID_NONE;
	AkUniqueID playlistItemID = AK_MUSIC_TRANSITION_RULE_ID_NONE;

	// Get source context.
	CAkScheduledItem * pSrcItem = m_chain.Last();

	AkUniqueID curID = ( pSrcItem ) ? pSrcItem->SegmentCtx()->SegmentNode()->ID() : AK_MUSIC_TRANSITION_RULE_ID_NONE;
	const AkMusicTransitionRule & rule = m_pSequenceNode->GetTransitionRule( curID, nextID );

	return AppendItem( rule, pSrcItem, nextID, playlistItemID );
}

// Appends a bucket to the chain following a music transition rule.
CAkScheduledItem * CAkSequenceCtx::AppendItem( 
    const AkMusicTransitionRule & in_rule,  // Transition rule between source bucket (in_pSrcBucket) and next one.
    CAkScheduledItem * in_pSrcItem,			// Item after which a new item is scheduled (NULL if first).
    AkUniqueID in_nextID,                   // Segment ID of the bucket to append to the chain.
    AkUniqueID in_playlistItemID            // Playlist item ID of the bucket to append to the chain.
    )
{
    AKASSERT( !in_rule.pTransObj );

	AkUInt64 uCurrentChainTime = 0;
    AkInt32 iSyncOffset = 0;

    // Source (if not <nothing>):
	if ( in_pSrcItem )
    {
		uCurrentChainTime = in_pSrcItem->Time();

        // Attach stop action for active segment.

        const AkMusicTransSrcRule & srcRule = in_rule.srcRule;
        AKASSERT( srcRule.eSyncType != SyncTypeImmediate ||
                  !"Sequence containers do not support Immediate transitions" );

        // Get time to sync.
		AkUInt32 uSyncPosition;
		AkUniqueID uCueFilter = srcRule.uCueFilterHash;	// ignored.
        CAkMusicSegment * pSrcSegmentNode = static_cast<CAkMusicSegment*>(in_pSrcItem->SegmentCtx()->SegmentNode());
        AKRESULT eResult = pSrcSegmentNode->GetExitSyncPos( 
            0,  // Minimal time-to-sync constraint related to source: 0, will be truncated if too long.
            (AkSyncType)srcRule.eSyncType,
			uCueFilter,
            true,  // skip entry marker.
            uSyncPosition );
        AKASSERT( eResult == AK_Success );
		iSyncOffset = uSyncPosition;



        // Prepare and attach Stop action.
		AkInt32 iFadeOutDuration = srcRule.fadeParams.transitionTime;
        AkInt32 iStopOffset = srcRule.fadeParams.iFadeOffset -
                              AkTimeConv::MillisecondsToSamples( iFadeOutDuration );
        
		// Deal with fades that are too long.
		if ( iStopOffset < -iSyncOffset )
		{
			iStopOffset = -iSyncOffset;
			iFadeOutDuration = AkTimeConv::SamplesToMilliseconds( srcRule.fadeParams.iFadeOffset - iStopOffset );
		}

		// Special case: sharp stop with post exit: push the stop offset to the end of the post-exit.
        if ( 0 == iStopOffset 
			&& 0 == iFadeOutDuration 
			&& srcRule.bPlayPostExit )
        {
            iStopOffset = pSrcSegmentNode->PostExitDuration();
        }

        iStopOffset += pSrcSegmentNode->ActiveDuration();
        
        in_pSrcItem->AttachStopCmd( iFadeOutDuration, 
                                    srcRule.fadeParams.eFadeCurve, 
                                    iStopOffset );
    }

    // Instantiate next segment.
    CAkSegmentCtx * pNextCtx = NULL;
    const AkMusicTransDestRule & destRule = in_rule.destRule;
    AkUInt32 uEntrySyncPos = 0;
    if ( in_nextID != AK_MUSIC_TRANSITION_RULE_ID_NONE )
    {
        // Destination: 

        // Get next segment.
        CAkMusicSegment * pNextSegment = static_cast<CAkMusicSegment*>(g_pIndex->GetNodePtrAndAddRef( in_nextID, AkNodeType_Default ));
        
		if ( pNextSegment )
		{
			AkUniqueID uSelectedCue;	// ignored.
			pNextSegment->GetEntrySyncPos( destRule, 0, AK_INVALID_UNIQUE_ID, uSelectedCue, uEntrySyncPos );

			// Create a context for next segment.
			pNextCtx = pNextSegment->CreateLowLevelSegmentCtxAndAddRef(
				this,
				Sequencer()->GameObjectPtr(),
				Sequencer()->GetUserParams(),
				Sequencer()->PlayDirectly() );

			if ( pNextCtx )
			{
				pNextCtx->SetPlayListItemID( in_playlistItemID );
			}
			else
			{
				// If the next segment context cannot be created, we will enqueue a NULL bucket in the chain.
				// This sequence will be considered as finished.
				// If it is under a switch context, it will remain until there is a switch change. Otherwise,
				// it is a top-level and it will auto-clean (stop).
				// Set the iterator as invalid so that it does not try to grow again.
				m_iterator.SetAsInvalid();
			}

			pNextSegment->Release();
		}
		else
			m_iterator.SetAsInvalid();
    }

	// Notify segment scheduling.
	{
		AkMonitorData::NotificationReason eNotifReason = ( m_pSequenceNode && m_pSequenceNode->Parent() && ! in_pSrcItem ) ?
		AkMonitorData::NotificationReason_MusicPlaylistSegmentScheduled :
		AkMonitorData::NotificationReason_MusicPlaylistTransitionScheduled ;

		MONITOR_MUSICPLAYLISTTRANS(
			Sequencer()->PlayingID(),
			Sequencer()->GameObjectPtr()->ID(),
			eNotifReason,
			in_rule.index,
			m_pSequenceNode->ID(), 
			in_pSrcItem ? static_cast<CAkMusicSegment*>(in_pSrcItem->SegmentCtx()->SegmentNode())->ID() : AK_INVALID_UNIQUE_ID, 
			in_nextID,
			(AkTimeMs)AkMath::Round64( 1000 * (AkReal64)(uCurrentChainTime + iSyncOffset ) / (AkReal64)AK_CORE_SAMPLERATE ) );
	}

    // Append new sequenced segment.
    CAkScheduledItem * pNextItem = EnqueueItem( uCurrentChainTime + iSyncOffset, pNextCtx );

	// Release new segment context (only) once it has been enqueued in the chain.
	if ( pNextCtx )
		pNextCtx->Release();

    if ( !pNextItem )
	{
		// Cannot enqueue a new bucket. This is a fatal error.
		return HandleFatalError();
	}


    // Attach Play on next segment if applicable.
    if ( pNextCtx )
    {
		AKASSERT( destRule.eEntryType == EntryTypeEntryMarker );
		AkInt32 iStartPosition = ComputeSegmentStartPosition( 
			uEntrySyncPos, 
			destRule.fadeParams.transitionTime, 
			destRule.fadeParams.iFadeOffset, 
			destRule.bPlayPreEntry, 
			pNextCtx->SegmentNode()->PreEntryDuration() );

		// Prepare segment.
        // Set segment's source offset, get required look-ahead.
        // iPlayOffset is the offset between the EntrySync position and where Play should start
		// (positive when play should start _before_ the EntrySync position).
        AkInt32 iPlayOffset = pNextItem->Prepare( uEntrySyncPos, iStartPosition );
				
		// Prepare Play action.

        // Handle look-ahead that cannot be met.
#ifndef AK_OPTIMIZED
		if ( iPlayOffset > iSyncOffset 
			&& in_pSrcItem )
		{
            // Might not have enough time to honor look-ahead. Post log.
            MONITOR_ERROR( AK::Monitor::ErrorCode_TooLongSegmentLookAhead );
        }
#endif

		// Set fade in. The rule's fade offset is relative to the sync position, whereas the item's 
		// fade offset must be relative to when the play command occurs.
        pNextItem->SetFadeIn( 
			destRule.fadeParams.transitionTime, 
			destRule.fadeParams.eFadeCurve,
			iPlayOffset + destRule.fadeParams.iFadeOffset );
    }

    return pNextItem;
}

CAkScheduledItem * CAkSequenceCtx::HandleFatalError()
{
	// Set iterator as invalid.
	m_iterator.SetAsInvalid();
	m_bIsChainValid = false;
	return CAkChainCtx::HandleFatalError();
}

void CAkSequenceCtx::NotifyItemSync( CAkScheduledItem* in_pItem )
{
	MONITOR_MUSICPLAYLISTTRANS(
		Sequencer()->PlayingID(),
		Sequencer()->GameObjectPtr()->ID(),
		AkMonitorData::NotificationReason_MusicPlaylistTransitionSynched,
		0,
		m_pSequenceNode->ID(), 
		AK_INVALID_UNIQUE_ID, 
		in_pItem->SegmentID(),
		0 );

	// Notify the scheduler that next item is playing
	CAkMusicPackedHistory history = m_iterator.NotifySegmentPlay();
	if ( NULL != history.Data() && NULL != Parent() )
	{
		CAkMatrixAwareCtx* pParent = static_cast<CAkMatrixAwareCtx*>( Parent() );
		pParent->SetPlaybackHistory( m_pSequenceNode->ID(), history );
	}
}

void CAkSequenceCtx::NotifyItemFlush( CAkScheduledItem* in_pItem )
{
	AkUniqueID segmentID = in_pItem->SegmentID();
	if( segmentID != AK_INVALID_UNIQUE_ID )
	{
		MONITOR_MUSICPLAYLISTTRANS(
			Sequencer()->PlayingID(),
			Sequencer()->GameObjectPtr()->ID(),
			AkMonitorData::NotificationReason_MusicPlaylistTransitionFlushed,
			0,
			m_pSequenceNode->ID(), 
			AK_INVALID_UNIQUE_ID, 
			segmentID,
			0 );
	}
}

// Returns false if the playlist iterator was made invalid because of a memory failure.
bool CAkSequenceCtx::IsValid()
{
	return m_bIsChainValid;
}

void CAkSequenceCtx::SetPBIFade(void * in_pOwner, AkReal32 in_fFadeRatio)
{
	AkDeltaMonitorObjBrace brace(m_pSequenceNode->key);
	CAkMusicCtx::SetPBIFade(in_pOwner, in_fFadeRatio);
}

#ifndef AK_OPTIMIZED
// ----------------------------------------------------------------------------------------------
// Function to retrieve the play target (the node which was played, resulting in a music PBI)
// ----------------------------------------------------------------------------------------------
AkUniqueID CAkSequenceCtx::GetPlayTargetID( AkUniqueID in_playTargetID ) const
{
	if( m_pSequenceNode )
		in_playTargetID = m_pSequenceNode->ID();
	if( Parent() )
		in_playTargetID = Parent()->GetPlayTargetID( in_playTargetID );
	return in_playTargetID;
}
#endif
