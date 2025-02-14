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
// AkSegmentCtx.cpp
//
// Segment context.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkSegmentCtx.h"
#include "AkMusicSegment.h"
#include "AkSegmentChain.h"
#include "AkSubTrackCtx.h"

// ----------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------
CAkSegmentCtx::CAkSegmentCtx(
    CAkMusicSegment *   in_pSegmentNode,
    CAkMusicCtx *       in_pParentCtx      
    )
    :CAkMusicCtx( in_pParentCtx )
    ,m_pSegmentNode( in_pSegmentNode )
    ,m_pOwner( NULL )
    ,m_iAudibleTime( 0 )
	,m_bClipsScheduled( false )
#ifndef AK_OPTIMIZED
	,m_PlaylistItemID( AK_INVALID_UNIQUE_ID )
#endif
{
	if( m_pSegmentNode )
		m_pSegmentNode->AddRef();
}

// ----------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------
CAkSegmentCtx::~CAkSegmentCtx()
{
	// Term sequencer
    m_sequencer.Term();

	// All children should have been deleted/detached
	AKASSERT( m_listChildren.IsEmpty() );

	// Destroy all switch track ctx
	while( ! m_listSwitchTrack.IsEmpty() )
	{
		CAkSwitchTrackInfo* pSwitchCtx = m_listSwitchTrack.First();
		m_listSwitchTrack.RemoveFirst();
		CAkSwitchTrackInfo::Destroy( pSwitchCtx );
	}

	// Release segment node
	if( m_pSegmentNode )
		m_pSegmentNode->Release();
}

// ----------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------
AKRESULT CAkSegmentCtx::Init(
    CAkRegisteredObj *  in_GameObject,
    UserParams &        in_rUserparams,
	bool				in_bPlayDirectly
    )
{
    CAkMusicCtx::Init( in_GameObject, in_rUserparams, in_bPlayDirectly );

	// Create ctx used to monitor switch changes
	CreateSwitchTrackCtx();

    return AK_Success;
}

// ----------------------------------------------------------------------------------------------
//	Create ctx used to monitor switch changes.
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::CreateSwitchTrackCtx()
{
	AkUInt32 uNumTracks = m_pSegmentNode->NumTracks();

	for ( AkUInt32 uTrack = 0; uTrack < uNumTracks; ++uTrack )
	{
		CAkMusicTrack* pTrack = m_pSegmentNode->Track( (AkUInt16)uTrack );
		AkMusicTrackType eTrackType = pTrack->GetMusicTrackType();

		// Create a sub-track ctx for all active sub-tracks.  For switch tracks,
		// must also create a siwtch track ctx to manage switch change notifications.
		if( eTrackType == AkMusicTrackType_Switch )
		{
			CAkSwitchTrackInfo* pSwitchCtx = CAkSwitchTrackInfo::Create( this, pTrack );
			if( pSwitchCtx )
			{
				// Initialize and add to list
				pSwitchCtx->Init();
				m_listSwitchTrack.AddFirst( pSwitchCtx );
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------
//	Create ctx used to manage clip playback.
// ----------------------------------------------------------------------------------------------
CAkSubTrackCtx* CAkSegmentCtx::GetOrCreateSubTrackCtx( CAkMusicTrack* in_pTrack, AkUInt32 in_uSubTrack )
{
	// Check list of children for a context on the desired sub-track.
	// The child must not be stopping.
	ChildrenCtxList::Iterator it = m_listChildren.Begin();
	for( ; it != m_listChildren.End(); ++it )
	{
		CAkSubTrackCtx* pSubTrackCtx = static_cast<CAkSubTrackCtx*>(*it);
		if( pSubTrackCtx->Track() == in_pTrack && pSubTrackCtx->SubTrack() == in_uSubTrack && ! pSubTrackCtx->IsStopping() )
			return pSubTrackCtx;
	}
	CAkSubTrackCtx* pSubTrackCtx = CAkSubTrackCtx::Create( this, in_pTrack, in_uSubTrack );
	if( pSubTrackCtx )
	{
		pSubTrackCtx->AddRef();
		pSubTrackCtx->Init( GameObjectPtr(), GetUserParams(), PlayDirectly() );
	}
	return pSubTrackCtx;
}

// ----------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------
CAkSwitchTrackInfo* CAkSegmentCtx::GetSwitchTrackInfo( CAkMusicTrack* in_pTrack )
{
	SwitchTrackInfoList::Iterator it = m_listSwitchTrack.Begin();
	for( ; it != m_listSwitchTrack.End(); ++it )
	{
		if( (*it)->Track() == in_pTrack )
			return *it;
	}
	return NULL;
}

// ----------------------------------------------------------------------------------------------
// Perform(): 
// Instantiate and play MusicPBIs in current time window [in_iNow, in_iNow+in_uNumSamples[, 
// schedule and executes associated stop commands.
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::Process(
	AkInt32		in_iTime,			// Current time (in samples) relative to the Entry Cue.
	AkUInt32	in_uNumSamples,		// Time window size.
	AkReal32	in_fPlaybackSpeed	// Playback speed.
	)
{
	ProcessPrologue( in_uNumSamples	);

	// Execute all pending actions that should occur in the next audio frame.
	ExecuteSequencerCmds( in_iTime, in_uNumSamples, in_fPlaybackSpeed );

	// Call process on all sub-tracks
	AkInt32 iClipTime = SegmentTimeToClipData( in_iTime );
	ChildrenCtxList::Iterator it = m_listChildren.Begin();
	while( it != m_listChildren.End() )
	{
		CAkSubTrackCtx* pSubTrackCtx = static_cast<CAkSubTrackCtx*>(*it);
		++it;
		if( pSubTrackCtx->RequiresProcessing() )
			pSubTrackCtx->Process( iClipTime, in_uNumSamples, in_fPlaybackSpeed );
	}

	ProcessEpilogue();
}

// ----------------------------------------------------------------------------------------------
// Context commands
//
// Initialize context for playback.
// Prepare the context for playback: set initial context position.
// Audible playback will start at position in_iSourceOffset (relative to Entry Cue).
// Returns the exact amount of time (samples) that will elapse between call to _Play() and 
// beginning of playback at position in_iSourceOffset.
// ----------------------------------------------------------------------------------------------
AkInt32 CAkSegmentCtx::Prepare(
    AkInt32 in_iSourceOffset	// Position in samples, at the native sample rate, relative to the Entry Cue.
    )
{
	// The time when this context should be audible is in_iSourceOffset -> m_iAudibleTime.
	// ScheduleAudioClips() uses m_iAudibleTime to compute appropriate play actions based on clip data.
    m_iAudibleTime = in_iSourceOffset;

	// Get segment position relative to pre-entry
	AkInt32 iPosition = SegmentTimeToClipData( in_iSourceOffset );

	// Compute minimum lookahead for each track.
	AkInt32 iLookAhead = 0;
	AkUInt32 uNumTracks = m_pSegmentNode->NumTracks();
	for ( AkUInt32 uTrack = 0; uTrack < uNumTracks; ++uTrack )
	{
		CAkMusicTrack* pTrack = m_pSegmentNode->Track( (AkUInt16)uTrack );
		AkInt32 iTempLookAhead = pTrack->ComputeMinSrcLookAhead( iPosition );
		iLookAhead = ( iTempLookAhead > iLookAhead ) ? iTempLookAhead : iLookAhead;
	}

	return iLookAhead;
}

// ----------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::OnPlayed()
{
	CAkMusicCtx::OnPlayed();

	

	// Schedule sub-track playback in this segment.
	ScheduleSequencerCmds();

	NotifyAction( AkMonitorData::NotificationReason_MusicSegmentStarted );
}

// ----------------------------------------------------------------------------------------------
// Override MusicCtx OnStopped: Need to flush actions to release children.
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::OnStopped()
{
	AddRef();

	// Flush sequencer and automation.
	Flush();

	if ( m_pOwner )
	{
		// Notify music segment ended (once - IsPlaying() is checked therein).
		NotifyAction( AkMonitorData::NotificationReason_MusicSegmentEnded );

		// Notify owner.
		m_pOwner->Detach();
	}

	CAkMusicCtx::OnStopped();

	Release();
}

// ----------------------------------------------------------------------------------------------
// Override MusicCtx OnPaused on some platforms: Stop all sounds and prepare to restart them on resume (WG-19814).
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::OnPaused()
{
	// Execute pause, even if we are going to stop all our children, in order to keep the Transport happy.
	CAkMusicCtx::OnPaused();

	if ( m_pOwner )
		m_pOwner->OnPaused();

#ifdef AK_STOP_MUSIC_ON_PAUSE
	// Stop all children (with min time transition).
	// Note: Children of segment contexts are MusicPBIs. They cannot be destroyed from within OnStopped().
	ChildrenCtxList::Iterator it = m_listChildren.Begin( );
    while( it != m_listChildren.End( ) )
	{
		CAkChildCtx* pChildCtx = static_cast<CAkChildCtx*>(*it);
		++it;
		pChildCtx->OnLastFrame( AK_NO_IN_BUFFER_STOP_REQUESTED );
	}
	Flush();
#endif	// AK_STOP_MUSIC_ON_PAUSE
}

#ifdef AK_STOP_MUSIC_ON_PAUSE
// ----------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::OnResumed()
{
	// Restart all clips at the current position + look-ahead time.
	// Avoid doing this if not playing: OnResumed() is broadcasted to everyone, even if they haven't 
	// started playing yet.
	if ( IsPlaying()
		&& IsPaused() )	// Also, OnResumed() is called even if we weren't paused.
	{
		RescheduleSequencerCmdsNow();
	}

	CAkMusicCtx::OnResumed();
}
#endif	// AK_STOP_MUSIC_ON_PAUSE

#ifndef AK_OPTIMIZED
// ----------------------------------------------------------------------------------------------
// Catch MusicCtx OnEditDirty(): Stop and reschedule all audio clips.
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::OnEditDirty()
{
	ChildrenCtxList::Iterator it = m_listChildren.Begin( );
	while( it != m_listChildren.End( ) )
	{
		CAkChildCtx* pChildCtx = static_cast<CAkChildCtx*>(*it);
		++it;
		pChildCtx->OnLastFrame( 0 );
	}

	Flush();

	if ( IsPlaying() )
	{
		MONITOR_ERRORMESSAGE( AK::Monitor::ErrorCode_MusicClipsRescheduledAfterTrackEdit );
		RescheduleSequencerCmdsNow();
	}
}
#endif

// ----------------------------------------------------------------------------------------------
// Flush sequencer commands.
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::Flush()
{
	m_sequencer.Flush();
}

// ----------------------------------------------------------------------------------------------
// Called by sub-track ctx when stopped.
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::RemoveReferences( CAkSubTrackCtx* in_pSubTrack )
{
	// Remove all references from the action scheduler.
	m_sequencer.ClearSubTrackCtx( in_pSubTrack );
}

// ----------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::ScheduleSequencerCmds()
{
	// Clips will be scheduled once we exit.
	m_bClipsScheduled = true;

	// Get current segment position, relative to entry-cue.
	AkInt32 iSegmentPosition = ((CAkChainCtx*)Parent())->GetSegmentPosition( m_pOwner );

	// Get time when we want segment to start playing.
	AkInt32 iSchedulePosition = m_iAudibleTime;

	// Only need to schedule play commands, scheduled at the current segment position.
	// Stop commands are only used to schedule sub-track changes due to a switch change.
	if( m_pSegmentNode )
	{
		AkMusicFade fadeParams;
		fadeParams.transitionTime = 0;

		AkUInt32 uNumTracks = m_pSegmentNode->NumTracks();

		for ( AkUInt32 uTrack = 0; uTrack < uNumTracks; ++uTrack )
		{
			CAkMusicTrack* pTrack = m_pSegmentNode->Track( (AkUInt16)uTrack );
			AkUInt32 uSubTrack = 0;

			AkMusicTrackType eTrackType = pTrack->GetMusicTrackType();

			// Create a sub-track ctx for all active sub-tracks.  For switch tracks,
			// must also create a siwtch track ctx to manage switch change notifications.
			if( eTrackType != AkMusicTrackType_Switch )
			{
				uSubTrack = pTrack->GetNextRS();
				AddSequencerPlayCmd( iSegmentPosition, pTrack, uSubTrack, fadeParams, iSchedulePosition );
			}
			else
			{
				CAkSwitchTrackInfo* pSwitchCtx = GetSwitchTrackInfo( pTrack );
				if( pSwitchCtx )
				{
					// Create sub-track ctx for each active sub-track
					for( uSubTrack = 0; uSubTrack < pTrack->GetNumSubTracks(); ++uSubTrack )
					{
						if( pSwitchCtx->IsSubTrackActive( (AkUInt16)uSubTrack ) )
						{
							AddSequencerPlayCmd( iSegmentPosition, pTrack, uSubTrack, fadeParams, iSchedulePosition );
						}
					}
				}

				// Send switch track notification
				AddSwitchTrackNotif( iSegmentPosition, pTrack );
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------
// Re-schedule all tracks to play as soon as possible from current time.
// Note: All sub-track ctx should have been destroyed.
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::RescheduleSequencerCmdsNow()
{
	// Compute the global (worst case) look-ahead time at the current position, then prepare
	// the context at this at this new position. Re-preparing until it converges.
	AkInt32 iSegmentPosition = ((CAkChainCtx*)Parent())->GetSegmentPosition( m_pOwner );
	AkInt32 iLookAheadTime = Prepare( iSegmentPosition );
	if ( iLookAheadTime > 0 )
	{
		AkInt32 iNewLookAheadTime = Prepare( iSegmentPosition + iLookAheadTime );
		while ( iNewLookAheadTime > iLookAheadTime )
		{
			iLookAheadTime = iNewLookAheadTime;
			iNewLookAheadTime = Prepare( iSegmentPosition + iLookAheadTime );
		}
	}

	// Re-schedule audio clips at the new prepared position.
	ScheduleSequencerCmds();
}

// ----------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::ExecuteSequencerCmds(
	AkInt32		in_iTime,			// Current time (relative to entry cue).
	AkUInt32	in_uNumSamples,		// Time window size.
	AkReal32	in_fPlaybackSpeed	// Playback speed.
	)
{
	AkMusicAction* pAction;

	while( m_sequencer.PopImminentAction( in_iTime, in_uNumSamples, pAction ) == AK_DataReady )
	{
		AkUInt32 uWindowOffset = pAction->Time() - in_iTime;
		switch( pAction->Type() )
		{
		case MusicActionTypePlay:
			{
				AkMusicActionPlaySubTrack* pActionPlay = static_cast<AkMusicActionPlaySubTrack*>(pAction);

				// One last check to make sure the play cmd is needed (i.e. make sure there isn't already an
				// active ctx for the sub-track)
				if( IsSubTrackPlayCmdNeeded( pActionPlay->Track(), pActionPlay->SubTrack() ) )
				{
					// Create sub-track ctx
					CAkSubTrackCtx* pSubTrackCtx = GetOrCreateSubTrackCtx( pActionPlay->Track(), pActionPlay->SubTrack() );
					if( pSubTrackCtx )
					{
						AKASSERT( !pSubTrackCtx->IsStopping() );

						AkInt32 iClipAudibleTime = SegmentTimeToClipData( pActionPlay->SegmentPosition() );
						pSubTrackCtx->Prepare( iClipAudibleTime );
						AkMusicFade fadeParams( pActionPlay->FadeParams() );
						fadeParams.iFadeOffset = pActionPlay->SegmentPosition() - pActionPlay->Time();
						pSubTrackCtx->_Play( fadeParams );

						// If we're about to end then flag the new child (must check sub-track ctx as well 'cause it may not be new)
						if( IsLastFrame() )
						{
							TransParams transParams;
							pSubTrackCtx->_Stop( transParams, GetNumSamplesInLastFrame() );
						}
					}
				}
			}
			break;
		case MusicActionTypeStop:
			{
				AkMusicActionStopSubTrack* pActionStop = static_cast<AkMusicActionStopSubTrack*>(pAction);
				CAkSubTrackCtx* pSubTrackCtx = pActionStop->SubTrackCtx();

				TransParams transParams;
				transParams.eFadeCurve = pActionStop->FadeParams().eFadeCurve;
				transParams.TransitionTime = pActionStop->FadeParams().transitionTime;

				pSubTrackCtx->_Stop( transParams, uWindowOffset );
			}
			break;
		case MusicActionTypeNotif:
			{
				AkMusicActionSwitchNotif* pActionNotif = static_cast<AkMusicActionSwitchNotif*>(pAction);
				ResolveSwitchTrackNotif( pActionNotif );
			}
			break;
		default:
			break; // MusicActionTypePostEvent handled by CAkSubTrackCtx::ExecuteSequencerCmds
		}
		AkDelete(AkMemID_Object, pAction );
	}
}

// ----------------------------------------------------------------------------------------------
// Called by CAkSwitchTrackInfo following a switch change.
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::RescheduleSwitchTrack( CAkSwitchTrackInfo* in_pSwitchInfo )
{
	// If we don't have an owner then we must bail (end of track condition)
	// If our clips have not yet been scheduled then there's nothing to re-schedule
	if( ! m_pOwner || ! m_bClipsScheduled )
		return;

	// Get current time (relative to entry-cue)
	AkInt32 iSegmentTime = static_cast< CAkChainCtx* >( Parent() )->GetSegmentPosition( m_pOwner );

	// Get pointer to music track node
	CAkMusicTrack* pTrack = in_pSwitchInfo->Track();

	// Get transition rule
	const AkMusicTrackTransRule& rTransRule = pTrack->GetTransRule();
	if( rTransRule.eSyncType == SyncTypeExitNever )
		return;

	// If we're before the start of segment playback then just reschedule as before (i.e. from scratch)
	if ( ! IsPlaying() )
	{
		Flush();
		ScheduleSequencerCmds();
		return;
	}

	// Start looking at the earliest possible exit sync position.
	AkInt32 iSrcFadeOffset = -AkTimeConv::MillisecondsToSamples( rTransRule.srcFadeParams.transitionTime ) + rTransRule.srcFadeParams.iFadeOffset;
	AkInt32 iDestFadeOffset = rTransRule.destFadeParams.iFadeOffset;

	AkInt32 iExitSyncPos = AkMax( iSegmentTime, iSegmentTime - iSrcFadeOffset );
	iExitSyncPos = AkMax( iExitSyncPos, iSegmentTime - iDestFadeOffset );

	// Entry cue is minimum synch point.
	iExitSyncPos = AkMax( 0, iExitSyncPos );

	// Will need to remember lookahead calculated below
	AkInt32 iLookAhead = 0;

	// Look for the first transition point where we can transition correctly (i.e. respect lookahead times)
	while( true )
	{
		// Find next exit position
		AkUInt32 uExitSyncPos = (AkUInt32)iExitSyncPos;
		AkUniqueID idCueFilterHash = rTransRule.uCueFilterHash;
		AKRESULT eResult = SegmentNode()->GetExitSyncPos( uExitSyncPos, (AkSyncType)rTransRule.eSyncType, idCueFilterHash, false, uExitSyncPos );
		if( eResult != AK_Success )
			return;
		iExitSyncPos = (AkInt32)uExitSyncPos;

		// Determine minimum source lookahead time required
		AkInt32 iPosition = SegmentTimeToClipData( iExitSyncPos );
		iLookAhead = pTrack->ComputeMinSrcLookAhead( iPosition );
		iPosition = iSegmentTime - iDestFadeOffset + iLookAhead;
		if( iPosition <= iExitSyncPos )
			break;

		// Look for the next exit sync position.  Do this by moving over our trial
		// position by the amount by which the test above failed.
		iExitSyncPos += iPosition - iExitSyncPos;
	}
	// Don't bother rescheduling if transition point is on or after exit cue
	if( iExitSyncPos >= (AkInt32)SegmentNode()->ActiveDuration() )
		return;

	// Calculate play & stop times.
	AkInt32 iStopTime = iExitSyncPos + iSrcFadeOffset;
	AkInt32 iPlayTime = iExitSyncPos + iDestFadeOffset - iLookAhead;
	AKASSERT( iStopTime >= iSegmentTime && iPlayTime >= iSegmentTime );

	// Remove commands on the same track:
	//	- remove play commands on inactive sub-tracks
	//	- remove stop commands on active sub-tracks
	CleanupSequencerCmdsOnSwitch( in_pSwitchInfo, iStopTime, iPlayTime );

	// Schedule a stop command for each playing, unactive sub-track.
	ScheduleSequencerStopCmdsOnSwitch( in_pSwitchInfo, iStopTime, rTransRule.srcFadeParams );

	// Schedule a play command for each active sub-track.
	ScheduleSequencerPlayCmdsOnSwitch( in_pSwitchInfo, iPlayTime, iPlayTime + iLookAhead, rTransRule.destFadeParams );

	// Send switch track notification
	AddSwitchTrackNotif( iExitSyncPos, pTrack );
}

// ----------------------------------------------------------------------------------------------
// Remove sequencer commands on given track:
//	- remove play commands on inactive sub-tracks
//	- remove stop commands on active sub-tracks
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::CleanupSequencerCmdsOnSwitch( CAkSwitchTrackInfo* in_pSwitchInfo, AkInt32 in_iStopActionTime, AkInt32 in_iPlayActionTime )
{
	AkInt32 iMinActionTime = AkMin( in_iStopActionTime, in_iPlayActionTime );

	CAkMusicSubTrackSequencer::IteratorEx it = m_sequencer.BeginEx();
	while( it != m_sequencer.End() )
	{
		AkMusicAction* pAction = *it;

		switch( pAction->Type() )
		{
		case MusicActionTypePlay:
			{
				if( pAction->Time() >= iMinActionTime )
				{
					AkMusicActionPlaySubTrack* pActionPlay = static_cast<AkMusicActionPlaySubTrack*>(*it);
					if( pActionPlay->Track() == in_pSwitchInfo->Track() && ! in_pSwitchInfo->IsSubTrackActive( (AkUInt16)pActionPlay->SubTrack() ) )
					{
						it = m_sequencer.Erase( it );
						AkDelete(AkMemID_Object, pActionPlay );
						continue;
					}
				}
			}
			break;
		case MusicActionTypeStop:
			{
				if( pAction->Time() >= iMinActionTime )
				{
					AkMusicActionStopSubTrack* pActionStop = static_cast<AkMusicActionStopSubTrack*>(*it);
					if( pActionStop->SubTrackCtx()->Track() == in_pSwitchInfo->Track() && in_pSwitchInfo->IsSubTrackActive( (AkUInt16)pActionStop->SubTrackCtx()->SubTrack() ) )
					{
						it = m_sequencer.Erase( it );
						AkDelete(AkMemID_Object, pActionStop );
						continue;
					}
				}
			}
			break;
		case MusicActionTypeNotif:
			break;
		default:
			AKASSERT( false && !"Invalid sequencer command" );
		}

		++it;
	}
}

// ----------------------------------------------------------------------------------------------
// Schedules a stop command on all playing, unactive sub-tracks.
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::ScheduleSequencerStopCmdsOnSwitch( CAkSwitchTrackInfo* in_pSwitchInfo, AkInt32 in_iActionTime, const AkMusicFade& in_rFadeParams )
{
	ChildrenCtxList::Iterator itst = m_listChildren.Begin();
	while( itst != m_listChildren.End() )
	{
		CAkSubTrackCtx* pSubTrackCtx = static_cast<CAkSubTrackCtx*>(*itst);
		++itst;

		// If on same track, track is not stopping, and sub-track is unactive, then must stop!
		if( ! pSubTrackCtx->IsStopping() && pSubTrackCtx->Track() == in_pSwitchInfo->Track() )
		{
			if( ! in_pSwitchInfo->IsSubTrackActive( (AkUInt16)pSubTrackCtx->SubTrack() ) )
			{
				// Make sure we don't already have a stop cmd scheduled
				AkMusicActionStopSubTrack* pStopAction = NULL;
				CAkMusicSubTrackSequencer::IteratorEx ita = m_sequencer.BeginEx();
				while( ita != m_sequencer.End() )
				{
					AkMusicActionStopSubTrack* pTempStop = static_cast<AkMusicActionStopSubTrack*>(*ita);
					if( (*ita)->Type() == MusicActionTypeStop && pTempStop->SubTrackCtx() == pSubTrackCtx )
					{
						if( pTempStop->Time() <= in_iActionTime )
						{
							pStopAction = pTempStop;
							break;
						}
						ita = m_sequencer.Erase( ita );
						AkDelete(AkMemID_Object, pTempStop );
					}
					else
						++ita;
				}

				// Add new stop command if we don't already have one
				if( ! pStopAction )
					AddSequencerStopCmd( in_iActionTime, pSubTrackCtx, in_rFadeParams );
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------
// Schedules a play command on active sub-tracks.
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::ScheduleSequencerPlayCmdsOnSwitch( CAkSwitchTrackInfo* in_pSwitchInfo, AkInt32 in_iActionTime, AkInt32 in_iSegmentTime, const AkMusicFade& in_rFadeParams )
{
	// Check each active sub-track
	const CAkSwitchTrackInfo::ActiveSubTrackArray& arActiveSubTrack = in_pSwitchInfo->ActiveSubtracks();
	CAkSwitchTrackInfo::ActiveSubTrackArray::Iterator it = arActiveSubTrack.Begin();
	for( ; it != arActiveSubTrack.End(); ++it )
	{
		// Make sure we don't already have a play cmd in the sequencer
		AkUInt32 uSubTrack = *it;
		AkMusicActionPlaySubTrack* pPlayAction = NULL;
		CAkMusicSubTrackSequencer::IteratorEx ita = m_sequencer.BeginEx();
		while( ita != m_sequencer.End() )
		{
			AkMusicActionPlaySubTrack* pTempPlay = static_cast<AkMusicActionPlaySubTrack*>(*ita);
			if( (*ita)->Type() == MusicActionTypePlay && pTempPlay->Track() == in_pSwitchInfo->Track() && pTempPlay->SubTrack() == uSubTrack )
			{
				if( pTempPlay->Time() <= in_iActionTime )
				{
					pPlayAction = pTempPlay;
					break;
				}
				ita = m_sequencer.Erase( ita );
				AkDelete(AkMemID_Object, pTempPlay );
			}
			else
				++ita;
		}

		// Add new play command if we don't already have one
		if( ! pPlayAction && IsSubTrackPlayCmdNeeded( in_pSwitchInfo->Track(), uSubTrack ) )
			AddSequencerPlayCmd( in_iActionTime, in_pSwitchInfo->Track(), uSubTrack, in_rFadeParams, in_iSegmentTime );
	}
}

// ----------------------------------------------------------------------------------------------
// Determines if the given sub-track really requires a play cmd.  A play command is needed if:
//	1) the sub-track is not already playing,
//	2) the sub-track is playing but has a transition on it (may be stopping).
// ----------------------------------------------------------------------------------------------
bool CAkSegmentCtx::IsSubTrackPlayCmdNeeded( CAkMusicTrack* in_pTrack, AkUInt32 in_uSubTrack )
{
	bool bNeedsPlayCmd = true;

	// Check children ctx for sub-track.
	ChildrenCtxList::Iterator it = m_listChildren.Begin();
	for( ; it != m_listChildren.End(); ++it )
	{
		CAkSubTrackCtx* pSubTrackCtx = static_cast<CAkSubTrackCtx*>(*it);
		if( pSubTrackCtx->Track() == in_pTrack && pSubTrackCtx->SubTrack() == in_uSubTrack )
		{
			if( ! pSubTrackCtx->IsStopping() && ! pSubTrackCtx->HasStopTrans() )
			{
				bNeedsPlayCmd = false;
				break;
			}
		}
	}

	return bNeedsPlayCmd;
}

// ----------------------------------------------------------------------------------------------
// Convert clip data time (relative to beginning of pre-entry) to segment time (relative to entry cue).
// ----------------------------------------------------------------------------------------------
AkInt32 CAkSegmentCtx::ClipDataToSegmentTime(
	AkInt32 in_iClipDataTime )		// Time, relative to beginning of pre-entry.
{
	return in_iClipDataTime - m_pSegmentNode->PreEntryDuration();
}

// ----------------------------------------------------------------------------------------------
// Convert segment time (relative to entry cue) to clip data time (relative to beginning of pre-entry).
// ----------------------------------------------------------------------------------------------
AkInt32 CAkSegmentCtx::SegmentTimeToClipData(
	AkInt32 in_iSegmentTime )		// Time, relative to entry cue.
{
	return in_iSegmentTime + m_pSegmentNode->PreEntryDuration();
}

// ----------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------
CAkRegisteredObj * CAkSegmentCtx::GameObjectPtr()
{ 
    AKASSERT( Parent() );
    return static_cast<CAkMatrixAwareCtx*>(Parent())->Sequencer()->GameObjectPtr(); 
}

// ----------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------
AkPlayingID CAkSegmentCtx::PlayingID()
{ 
    AKASSERT( Parent() );
    return static_cast<CAkMatrixAwareCtx*>(Parent())->Sequencer()->PlayingID(); 
}

// ----------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------
UserParams & CAkSegmentCtx::GetUserParams()
{ 
    AKASSERT( Parent() );
    return static_cast<CAkMatrixAwareCtx*>(Parent())->Sequencer()->GetUserParams(); 
}

bool CAkSegmentCtx::PlayDirectly()
{
	AKASSERT( Parent() );
	return static_cast<CAkMatrixAwareCtx*>(Parent())->Sequencer()->PlayDirectly();
}

// ----------------------------------------------------------------------------------------------
// Called by children (sub-track ctx) to get current segment position, relative to pre-entry.
// ----------------------------------------------------------------------------------------------
AkInt32 CAkSegmentCtx::GetClipPosition()
{
	return SegmentTimeToClipData( static_cast<CAkChainCtx*>( Parent() )->GetSegmentPosition( m_pOwner ) );
}

// ----------------------------------------------------------------------------------------------
// Called by children (sub-track ctx) to get frame size.
// ----------------------------------------------------------------------------------------------
AkInt32 CAkSegmentCtx::GetTimeWindowSize()
{
	return static_cast<CAkChainCtx*>( Parent() )->Sequencer()->CurTimeWindowSize();
}

// ----------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::NotifyAction( AkMonitorData::NotificationReason in_eReason )
{
#ifndef AK_OPTIMIZED
	if( !IsIdle() && GetPlayListItemID() != AK_INVALID_UNIQUE_ID )
	{
		// We should always have an assoicated segment node.  However, this is kept because of WG-21868.
		// Probable cause is a change through WAL during playback.
		AKASSERT( SegmentNode() && SegmentNode()->Parent() && SegmentNode()->Parent()->NodeCategory() == AkNodeCategory_MusicRanSeqCntr );
		if( SegmentNode() && SegmentNode()->Parent() && SegmentNode()->Parent()->NodeCategory() == AkNodeCategory_MusicRanSeqCntr )
		{
			// At this point, SegmentNode()->Parent() is a MusicRanSeqCntr
			MONITOR_MUSICOBJECTNOTIF( PlayingID(), GameObjectPtr()->ID(), GetUserParams().CustomParam(), in_eReason, SegmentNode()->Parent()->ID(), GetPlayListItemID() );
		}
	}
#endif
}

// ----------------------------------------------------------------------------------------------
// Adds a switch track notification to the music action sequencer.
// Also checks for other notifications on the same track, and reverts when needed.
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::AddSwitchTrackNotif( AkInt32 in_iSegmentPosition, CAkMusicTrack* in_pTrack )
{
	// Find the associated switch track ctx
	CAkSwitchTrackInfo* pSwitchTrackInfo = GetSwitchTrackInfo( in_pTrack );
	if( pSwitchTrackInfo )
	{
		// Remove all notifications for transitions that will occur on the given track, at a later time
		CAkMusicSubTrackSequencer::IteratorEx it = m_sequencer.BeginEx();
		while( it != m_sequencer.End() )
		{
			AkMusicActionSwitchNotif* pAction = static_cast<AkMusicActionSwitchNotif*>(*it);
			if( (*it)->Type() == MusicActionTypeNotif
				&& (*it)->Time() >= in_iSegmentPosition
				&& pAction->SwitchTrackInfo() == pSwitchTrackInfo )
			{
				it = m_sequencer.Erase( it );

				MONITOR_MUSICTRACKTRANS(
					GetUserParams().PlayingID(),
					GameObjectPtr()->ID(),
					AkMonitorData::NotificationReason_MusicTrackTransitionReverted,
					pSwitchTrackInfo->Track()->ID(),
					pSwitchTrackInfo->Track()->GetTrackSwitchInfo()->GetSwitchGroup(),
					pAction->PrevSwitchStateID(),
					pAction->NewSwitchStateID(),
					0 );

				AkDelete(AkMemID_Object, pAction );
			}
			else
				++it;
		}

		// Get current time (relative to entry-cue)
		AkInt32 iSegmentTime = static_cast< CAkChainCtx* >( Parent() )->GetSegmentPosition( m_pOwner );
		AkTimeMs fTimeToExit = AkTimeConv::SamplesToMilliseconds( in_iSegmentPosition - iSegmentTime );

		// Send a notificatoin for shceduled transition
		MONITOR_MUSICTRACKTRANS(
			GetUserParams().PlayingID(),
			GameObjectPtr()->ID(),
			AkMonitorData::NotificationReason_MusicTrackTransitionScheduled,
			pSwitchTrackInfo->Track()->ID(),
			pSwitchTrackInfo->Track()->GetTrackSwitchInfo()->GetSwitchGroup(),
			pSwitchTrackInfo->PrevSwitchState(),
			pSwitchTrackInfo->SwitchState(),
			fTimeToExit );

		// Add notification for later transition notifications (reverted or resolved)
		AkMusicActionSwitchNotif* pAction = AkNew(AkMemID_Object, AkMusicActionSwitchNotif(
			in_iSegmentPosition,
			pSwitchTrackInfo,
			pSwitchTrackInfo->PrevSwitchState(),
			pSwitchTrackInfo->SwitchState() ) );
		if( pAction )
			m_sequencer.ScheduleAction( pAction );
	}
}

// ----------------------------------------------------------------------------------------------
// Sends a switch track resolved notification.
// ----------------------------------------------------------------------------------------------
void CAkSegmentCtx::ResolveSwitchTrackNotif( AkMusicActionSwitchNotif* in_pNotif )
{
	MONITOR_MUSICTRACKTRANS(
		GetUserParams().PlayingID(),
		GameObjectPtr()->ID(),
		AkMonitorData::NotificationReason_MusicTrackTransitionResolved,
		in_pNotif->SwitchTrackInfo()->Track()->ID(),
		in_pNotif->SwitchTrackInfo()->Track()->GetTrackSwitchInfo()->GetSwitchGroup(),
		in_pNotif->PrevSwitchStateID(),
		in_pNotif->NewSwitchStateID(),
		0 );
}

void CAkSegmentCtx::SetPBIFade(void * in_pOwner, AkReal32 in_fFadeRatio)
{
	AkDeltaMonitorObjBrace brace(m_pSegmentNode->key);
	CAkMusicCtx::SetPBIFade(in_pOwner, in_fFadeRatio);
}

#ifndef AK_OPTIMIZED
// ----------------------------------------------------------------------------------------------
// Function to retrieve the play target (the node which was played, resulting in a music PBI)
// ----------------------------------------------------------------------------------------------
AkUniqueID CAkSegmentCtx::GetPlayTargetID( AkUniqueID in_playTargetID ) const
{
	if( m_pSegmentNode )
		in_playTargetID = m_pSegmentNode->ID();
	if( Parent() )
		in_playTargetID = Parent()->GetPlayTargetID( in_playTargetID );
	return in_playTargetID;
}
#endif
