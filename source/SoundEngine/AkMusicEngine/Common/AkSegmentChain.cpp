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

#include "stdafx.h"
#include "AkSegmentChain.h"
#include "AkMusicSegment.h"
#include "AkScheduleWindow.h"

#define NULL_ITEM_TIME AK_UINT_MAX

AkInt64 AkScheduledChain::SelfContainedIter::GetScheduledItemTime() const
{
	AkInt64 iItemTime = Owner()->GetScheduledItemTime( *(*this) );
	// Note: Item time may be negative, but from the point of view of the outside,
	// everything behind a context sync time is considered as look-ahead.
	if ( iItemTime < 0 )
		iItemTime = 0;
	return iItemTime;
}

// Convert time (relative to owner context) into a position relative to the pointed segment's
// entry cue.
AkInt64 AkScheduledChain::SelfContainedIter::CtxTimeToSegmentPosition(
	AkInt64		in_iTime			// Time, relative to the owner context.
	) const
{
	return Owner()->CtxTimeToSegmentPosition( *(*this), in_iTime );
}
	
// Convert given position of the segment pointed by this scheduled item into a time value,
// relative to the owner context.
AkInt64 AkScheduledChain::SelfContainedIter::SegmentPositionToCtxTime(
	AkInt64		in_iSegmentPosition	// Position of segment pointed by this scheduled item (relative to entry cue).
	) const 
{
	return Owner()->SegmentPositionToCtxTime( *(*this), in_iSegmentPosition );
}


CAkChainCtx::CAkChainCtx(
    CAkMusicCtx *   in_parent	// Parent context. NULL if this is a top-level context.
    )
:CAkMatrixAwareCtx( in_parent )
,m_uItemsTimeOffset( 0 )
{
}

CAkChainCtx::~CAkChainCtx()
{
	ClearChain();
    m_chain.Term();
}

// Interface for SegmentCtx.
AkInt32 CAkChainCtx::GetSegmentPosition( CAkScheduledItem * in_pItem )
{
	AKASSERT( in_pItem );

	// Create a scheduling window and make it point to in_pItem. 
	CAkScheduleWindow window( this, true );
	while ( window.GetScheduledItem() != in_pItem )
	{
		GetNextScheduleWindow( window, true );
	}
	AKASSERT( window.IsValid() );
	return window.ToSegmentPosition( GlobalToLocalTime( Sequencer()->Now() ) );
}

// Override OnStopped(): Need to flush chain and release references.
void CAkChainCtx::OnStopped()
{
	AddRef();
	
	// Flush all scheduled items. Release low-level segment contexts.
	Flush();

	CAkMatrixAwareCtx::OnStopped();

	Release();
}

// Delete all items. All segment item should be NULL (that is, properly destroyed following segment ctx notification).
void CAkChainCtx::ClearChain()
{
    while ( !m_chain.IsEmpty() )
    {
        CAkScheduledItem * pItem = m_chain.First();
        AKVERIFY( m_chain.RemoveFirst() == AK_Success );

        pItem->Destroy();
    }
}

// Create a new item and append it to the chain.
CAkScheduledItem * CAkChainCtx::EnqueueItem(
	AkUInt64		in_uTimeOffset,	// Sync time offset relative to this context.
	CAkSegmentCtx * in_pSegment		// Low-level segment context. Can be NULL (terminating chain).
    )
{
    CAkScheduledItem * pNewSequencedItem = AkNew(AkMemID_Object, CAkScheduledItem( in_uTimeOffset, in_pSegment ) );
    if ( pNewSequencedItem )
        m_chain.AddLast( pNewSequencedItem );
    return pNewSequencedItem;
}

// Sequencer processing.
void CAkChainCtx::Process(
	AkInt64			in_iCurrentTime,		// Current time in the time coordinates of the caller (parent).
	AkUInt32		in_uNumSamples,			// Number of samples to process.
	AkCutoffInfo &	in_cutoffInfo			// Downstream chain cutoff info. Modified inside.
	)
{
	ProcessPrologue( in_iCurrentTime, in_uNumSamples, in_cutoffInfo );

	// Get values in terms of scheduled item chain times.
	AkInt64 iCurItemTime = LocalToChainTime( in_iCurrentTime );
	AkInt64 iMaxEnabledItemTime = LocalToChainTime( in_cutoffInfo.CutoffTime() );

	// Process all scheduled items in chain for the desired number of samples.
	// Grow chain if required.
	// Clean old items.
	
	UpdateChainLength( iCurItemTime );

	bool bCanCollectItems = true;
	AkScheduledChain::IteratorEx it = m_chain.BeginEx();
	while ( it != m_chain.End() )
    {
		// Notify if item is about to start; use time in parent coordinates.
		AkInt64 itTime = (*it)->Time();

        // Process item: executes attached actions and runs low-level segment contexts.
		bool bWasPlayPending = (*it)->IsPlaybackPending();

		bool bSkip = in_cutoffInfo.MustCutoff() && ( itTime >= iMaxEnabledItemTime );
		(*it)->Process( iCurItemTime, in_uNumSamples, Sequencer()->GetPlaybackSpeed(), bSkip );

		if ( bWasPlayPending && ! (*it)->IsPlaybackPending() )
			NotifyItemSync( *it );

		// Garbage collect old items.
		if ( !bCanCollectItems )
			++it;
		else
		{
			AkScheduledChain::Iterator itNext = it;
			++itNext;
			if ( (*it)->CanDestroy()
				&& itNext != m_chain.End() 
				&& iCurItemTime > (*itNext)->Time() )
			{
				CAkScheduledItem * pItem = (*it);
				it = m_chain.Erase( it );
				pItem->Destroy();
			}
			else
			{
				// Stop trying to collect items as soon as one of them has been declared uncollectable.
				bCanCollectItems = false;
				++it;
			}
		}
    }

	ProcessEpilogue( in_iCurrentTime, in_uNumSamples );
}

// Tree walking.
void CAkChainCtx::GetNextScheduleWindow( 
	CAkScheduleWindow & io_window,		// Current schedule window, returned as next schedule window. If io_window had just been instantiated, the active window would be returned.
	bool in_bDoNotGrow					// If true, chains are not asked to grow. "False" is typically used for scheduling, whereas "True" is used to check the content of a chain.
	)
{
	AkScheduledChain::SelfContainedIter itNext( this );
	AkScheduledChain::SelfContainedIter it = io_window.GetScheduledItemIterator();

	if ( it != m_chain.End() )
	{
		// Has a scheduled item. Context must be this one. Return next item.
		AKASSERT( it.Owner() == this );
		// Must not have been called again if length was already infinite.
		AKASSERT( !io_window.IsDurationInfinite() );

		itNext = it;
		++itNext;

		// If length is not infinite, itNext must exist.
		if ( itNext == m_chain.End() )
		{
			io_window.Invalidate();
			return;	// Infinite start position ( this context must be stopping).
		}
	}
	else
	{
		// First time this chain is queried. Get the first segment.
		it = itNext = m_chain.BeginSelfContained( this );
		if ( it == m_chain.End() )
			return;	// Infinite start position ( this context must be stopping).
		
		if ( !io_window.FirstItemOnly() )
		{
			// Place the window over the item corresponding to the current time.

			// Get current time in items chain.
			AkInt64 iCurrentTime = GlobalToLocalTime( Sequencer()->Now() );

			// Move iterator to item corresponding to current time.
			++itNext;
			while ( itNext != m_chain.End() 
					&& GetScheduledItemTime( *itNext ) < iCurrentTime )
			{
				it = itNext;
				++itNext;
			}
			itNext = it;
		}
	}

	bool bLastItem = false;
	AkUInt64 uDuration = 0;

	// Check following item in order to compute length.
	AkScheduledChain::Iterator itFollow = itNext;
	
	++itFollow;

	// Grow if necessary.
	if ( itFollow == m_chain.End() )
	{
		// Try growing again. If we can't, it's the last item of the chain.
		// In "no grow" mode, just assume it is the end of the chain.
		if ( in_bDoNotGrow || !Grow() )
			bLastItem = true;
		else
		{
			itFollow = itNext;
			++itFollow;
			AKASSERT( itFollow != m_chain.End() );
		}
	}
	
	// Compute window length.
	if ( !bLastItem )
	{
		// From the point of view of a window, nothing exists behind the context's sync point.
		AkInt64 iTimeStart = GetScheduledItemTime( *itNext );
		if ( iTimeStart < 0 ) iTimeStart = 0;
		AkInt64 iTimeEnd = GetScheduledItemTime( *itFollow );
		if ( iTimeEnd < 0 ) iTimeEnd = 0;
		uDuration = iTimeEnd - iTimeStart;
	}	
	io_window.SetScheduledItem( itNext );
	io_window.SetDuration( uDuration, bLastItem );
}

// Call this to refresh the current window's length.
void CAkChainCtx::RefreshWindow(
	CAkScheduleWindow & io_window
	)
{
	AkScheduledChain::SelfContainedIter it = io_window.GetScheduledItemIterator();

	// Has a scheduled item. Context must be this one.
	AKASSERT( it != m_chain.End() 
			&& it.Owner() == this );
	
	bool bLastItem = false;
	AkUInt64 uDuration = 0;

	// Check following item in order to recompute length.
	AkScheduledChain::Iterator itFollow = it;
	++itFollow;

	// Grow if necessary.
	if ( itFollow == m_chain.End() )
	{
		// Try growing again. If we can't, it's the last item of the chain.
		if ( !Grow() )
			bLastItem = true;
		else
		{
			itFollow = it;
			++itFollow;
			AKASSERT( itFollow != m_chain.End() );
		}
	}
	
	// Compute window length.
	if ( !bLastItem )
	{
		// From the point of view of a window, nothing exists behind the context's sync point.
		AkInt64 iTimeStart = GetScheduledItemTime( *it);
		if ( iTimeStart < 0 ) iTimeStart = 0;
		AkInt64 iTimeEnd = GetScheduledItemTime( *itFollow );
		if ( iTimeEnd < 0 ) iTimeEnd = 0;
		uDuration = ( iTimeEnd - iTimeStart );
	}
	io_window.SetDuration( uDuration, bLastItem );
}

// Interface for parent switch context: trigger switch change that was delayed because of parent transition.
void CAkChainCtx::PerformDelayedSwitchChange()
{
}

// Flush all scheduled items of the chain: stop, release references, re-schedule cancelled actions.
void CAkChainCtx::Flush()
{
    while ( !m_chain.IsEmpty() )
    {
        CAkScheduledItem * pItem = m_chain.First();
		NotifyItemFlush( pItem );
        pItem->OnStopped();
        AKVERIFY( m_chain.RemoveFirst() == AK_Success );
        pItem->Destroy();
    }
}

// Flush first scheduled item (segment). Assumes that there is at least one.
// NOTE: This function is always called within temporary addref.
void CAkChainCtx::FlushFirstItem()
{
	CAkScheduledItem * pItem = m_chain.First();
	AKASSERT( pItem );
    pItem->OnStopped();
    AKVERIFY( m_chain.RemoveFirst() == AK_Success );
    pItem->Destroy();
}

// Does not grow (default implementation).
bool CAkChainCtx::Grow()
{
    return false;
}

// Does nothing (default implementation).
void CAkChainCtx::UpdateChainLength(
	AkInt64 // Desired position.
	)
{
}

// Prepares the context according to the destination transition rule, if specified. 
// Otherwise, prepares it at position in_iMinStartPosition and grows the chain in order to honor 
// the start position. The returned look-ahead time depends on all currently scheduled items 
// (the number of scheduled items depends on the concrete context policy - playlists schedule 3 items ahead).
// Returns the required look-ahead time. 
// Returns CtxPrepare_Success if preparing was successful, CtxPrepare_SegmentChanged if the starting segment was changed
// (normally implies re-evaluating transition rule), CtxPrepare_Failure in case of a fatal error.
CAkMatrixAwareCtx::CtxPrepareResult CAkChainCtx::Prepare(
	const AkMusicTransDestRule * in_pRule,	// Transition destination (arrival) rule. NULL means "anywhere", in any segment.
	const CAkMusicPlaybackHistory * in_pPlaybackHistory,	// Playback history, used when transitioning to a playlist we've played before, and synching to the last played segment, or somewhere close
	AkMusicSeekInfo & io_seekingInfo,		// Data for seeking within the context. Actual seeking location may placed later according to what the rule prescribes. Seek offset will be "consumed" and returned. 
	AkInt32 & out_iRequiredLookAheadTime,	// Returned required look-ahead time.
	AkUniqueID & out_uSelectedCue,			// Returned cue this context was prepared to. AK_INVALID_UNIQUE_ID if not on a cue.
	AkUniqueID	in_uCueHashForMatchSrc,		// Cue which was used in the source segment. Used only if rule is bDestMatchCueName.
	bool in_bWrapInSegment					// TEMP Wrap desired seek position in first segment if true (legacy behavior for SameTime transition).
	)
{
	AkScheduledChain::Iterator it = m_chain.Begin();
	AKASSERT( it != m_chain.End() );

	if ( io_seekingInfo.RequiresInterSegmentSeek() )
	{
		// Jump to appropriate segment, clear it from seek request, and return with CtxPrepare_SegmentChanged code.
		// Jumping: Surround by AddRef/Release as this will temporarily clear off all children.
		AddRef();
		if ( PlaylistJump( io_seekingInfo.uJumpTo, in_pRule, in_pPlaybackHistory ) != AK_Success )
		{
			// Fatal error, failed jumping to segment. Stop ctx before releasing the reference.
			_Cancel();
			Release();
			return CtxPrepare_Failure;
		}
		Release();

		io_seekingInfo.ClearInterSegmentSeek();
		return CtxPrepare_SegmentChanged;
	}

	AkInt32 iRequiredLookAhead;
	AkUInt32 uSyncTime = 0;
	AkInt32 iStartPosition = 0;

	// If a rule is specified, get Entry position from first item's segment node (if it exists).
	if ( in_pRule )
	{
		bool bPlayPreEntry = false;
		if ( (*it)->SegmentCtx() )
		{
			CAkMusicSegment * pFirstSegmentNode = (*it)->SegmentCtx()->SegmentNode();
			AKASSERT( pFirstSegmentNode );

			AkUInt32 uSegmentDuration = pFirstSegmentNode->ActiveDuration();
			AkUInt32 uDesiredEntryPosition = io_seekingInfo.seekInSegment.GetSeekPosition( uSegmentDuration );

			if ( in_bWrapInSegment )
			{
				// Wrap around destination's duration.
				if ( uSegmentDuration > 0 )
					uDesiredEntryPosition = uDesiredEntryPosition % uSegmentDuration;
				else
					uDesiredEntryPosition = 0;
			}
			pFirstSegmentNode->GetEntrySyncPos( 
				*in_pRule, 
				uDesiredEntryPosition, 
				in_uCueHashForMatchSrc, 
				out_uSelectedCue, 
				uSyncTime );

			// Clamp sync time to segment duration.
			if ( uSyncTime > uSegmentDuration )
				uSyncTime = uSegmentDuration;

			// Consume time off desired start position.
			AkUInt32 uConsumedSegmentTime = AkMin( uSyncTime, uDesiredEntryPosition );
			io_seekingInfo.seekInSegment.SetAbsolute( uDesiredEntryPosition - uConsumedSegmentTime );

			// Synching in on or beyond Exit Cue (except Entry Cue of 0-length segments)?
			if (uSyncTime >= uSegmentDuration
				&& (uSegmentDuration > 0 || uSyncTime > 0))
			{
				// Flush first item (chain will start at next segment) and bail out.
				AddRef();
				FlushFirstItem();
				AKASSERT(m_chain.First() || !"Should at least be an empty item remaining");
				UpdateChainLength(uSyncTime);
				Release();
				if (m_chain.First()->SegmentCtx())
					return CtxPrepare_SegmentChanged;
				else
				{
					// Otherwise we are seeking to nothing (end of chain).
					// This is not supported (would require rethinking context refcounting).
#ifndef AK_OPTIMIZED
					AkMonitor::Monitor_PostCodeWithParam(AK::Monitor::ErrorCode_SeekAfterEndOfPlaylist, AK::Monitor::ErrorLevel_Error, AK_INVALID_UNIQUE_ID, Sequencer()->PlayingID(), Sequencer()->GameObjectPtr()->ID(), NodeID(), false);
#endif
					return CtxPrepare_Failure;
				}
			}

			// Play pre-entry?
			// Pre-entry can only be played if the entry position is the Entry Cue (0).
			// Note: No pre-entry with same time transitions.
			bPlayPreEntry = in_pRule->bPlayPreEntry 
							&& uSyncTime == 0 
							&& in_pRule->eEntryType != EntryTypeSameTime;

			// Segment start position is the sync position, modulated with pre-entry and fade offset.
			iStartPosition = ComputeSegmentStartPosition( uSyncTime, in_pRule->fadeParams.transitionTime, in_pRule->fadeParams.iFadeOffset, bPlayPreEntry, pFirstSegmentNode->PreEntryDuration() );
		}
		else
		{
			// otherwise item is empty, sync and start positions are 0 (start will be ignored).
			// Consume time off desired start position.
			io_seekingInfo.seekInSegment.SetAbsolute( 0 );
		}
	}
	else
	{
		// Position is always absolute in this path (don't know why).
		iStartPosition = io_seekingInfo.seekInSegment.iSeekPosition;

		// iMinStartPosition is the position to which we are going to seek,
		// but sync position is never before the entry cue.
		uSyncTime = ( iStartPosition > 0 ) ? iStartPosition : 0;

		// Consume time off desired start position.
		io_seekingInfo.seekInSegment.SetAbsolute( 0 );

		/// TODO WG-16446. In order to implement seeking in playlist containers, 
		/// increment iterator until the correct segment is found, and flush old ones.
	}

	// Prepare the first item.
	iRequiredLookAhead = (*it)->Prepare( uSyncTime, iStartPosition );
	
	// Sync time is relative to the first segment. Make it relative to the chain before updating the chain.
	uSyncTime += AkTimeConv::ToShortRange( (*it)->Time() );
	m_uItemsTimeOffset = uSyncTime;

	UpdateChainLength( uSyncTime );

	// Walk through chain and return global look-ahead time (before first item sync).
	++it;
	while ( it != m_chain.End() )
	{
		// Note: This sequence has not started playing yet. We can assume its sync offsets are well within 32 bits.
		AkInt32 iLookAheadTime = -(*it)->GetPlayTime() - ( (AkInt32)(*it)->Time() - (AkInt32)uSyncTime );
		if ( iLookAheadTime > iRequiredLookAhead )
			iRequiredLookAhead = iLookAheadTime;
		++it;
	}

	out_iRequiredLookAheadTime = iRequiredLookAhead;
	return CtxPrepare_Success;
}

// Inspect context that has not started playing yet and determine the time when it will start playing,
// relative to its sync time (out_iPlayTime, typically <= 0). Additionnally, compute the time when it 
// will start playing and will be audible (out_iPlayTimeAudible, always >= out_iPlayTime).
void CAkChainCtx::QueryLookAheadInfo(
	AkInt64 &	out_iPlayTime,			// Returned time of earliest play command (relative to context).
	AkInt64 &	out_iPlayTimeAudible	// Returned time of earliest play command that becomes audible. Always >= out_iPlayTime. Note that it may not refer to the same play action as above.
	)
{
	AkScheduledChain::Iterator it = m_chain.Begin();
	AKASSERT( it != m_chain.End() );

	if ( !(*it)->SegmentCtx() )
	{
		out_iPlayTime = 0;
		out_iPlayTimeAudible = 0;
		return;
	}

	// Walk through chain and get both earliest play time and earliest play time not considering streaming
	// look-ahead. They may not be related to the same segment.
	AkInt64 iEarliestPlayTime = (*it)->GetPlayTime() + (*it)->Time();
	AkInt64 iEarliestPlayTimeAudible = (*it)->SegmentCtx()->GetAudibleTime() + (*it)->Time();
	++it;
	while ( it != m_chain.End() 
			&& (*it)->SegmentCtx() )	// Break if item is empty.
	{	
		AkInt64 iAbsolutePlayTime = (*it)->GetPlayTime() + (*it)->Time();
		if ( iAbsolutePlayTime < iEarliestPlayTime )
			iEarliestPlayTime = iAbsolutePlayTime;
		AkInt64 iAbsolutePlayTimeAudible = (*it)->SegmentCtx()->GetAudibleTime() + (*it)->Time();
		if ( iAbsolutePlayTimeAudible < iEarliestPlayTimeAudible )
			iEarliestPlayTimeAudible = iAbsolutePlayTimeAudible;
		++it;
	}

	// Convert returned values to local time.
	out_iPlayTime = ChainToLocalTime( iEarliestPlayTime );
	out_iPlayTimeAudible = ChainToLocalTime( iEarliestPlayTimeAudible );
}

// A chain context can restart playing if none of its scheduled item has been skipped.
bool CAkChainCtx::CanRestartPlaying()
{
	// If this context is in its last frame, it is unrecoverable.
	if ( IsPlaying() || IsIdle() )
	{
		AkScheduledChain::Iterator it = m_chain.Begin();
		while ( it != m_chain.End() )
		{
			if ( (*it)->WasPlaybackSkipped() )
				return false;
			++it;
		}
		return true;
	}
	return false;
}

// Stop a context based on how much time it has played already. Should be stopped immediately
// if it has not been playing yet, or faded out by the amount of time it has been playing.
void CAkChainCtx::CancelPlayback(
	AkInt64 in_iCurrentTime		// Current time in the time coordinates of the caller (parent).
	)
{
	AddRef();
	if ( IsIdle() )
		OnStopped();
	else if ( IsPlaying() )
	{
		// Convert into local time values.
		in_iCurrentTime = ParentToLocalTime( in_iCurrentTime );

		// Convert time in terms of scheduled item chain times.
		AkInt64 iCurItemTime = LocalToChainTime( in_iCurrentTime );
		
		AkScheduledChain::Iterator it = m_chain.Begin();
		while ( it != m_chain.End() )
		{
			(*it)->CancelPlayback( iCurItemTime );
			++it;
		}
	}
	Release();
}

CAkScheduledItem * CAkChainCtx::HandleFatalError()
{
	return m_chain.Last();
}
