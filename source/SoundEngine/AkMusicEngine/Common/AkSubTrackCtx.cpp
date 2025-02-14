/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

//////////////////////////////////////////////////////////////////////
//
// AkSubTrackCtx.cpp
//
// Sub-track context.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkSubTrackCtx.h"
#include "AkMusicRenderer.h"
#include "AkMatrixSequencer.h"
#include "AkSegmentCtx.h"
#include "AkMidiClipCtx.h"
#include "AkAudioMgr.h"
#include "AkEvent.h"
#include "AkProfile.h"

extern AkPlatformInitSettings g_PDSettings;

// -------------------------------------------------------------------
// CAkSubTrackCtx
// -------------------------------------------------------------------
CAkSubTrackCtx* CAkSubTrackCtx::Create( CAkMusicCtx* in_pParentCtx, CAkMusicTrack* in_pTrackNode, AkUInt32 in_uSubTrack )
{
	return AkNew(AkMemID_Object, CAkSubTrackCtx( in_pParentCtx, in_pTrackNode, in_uSubTrack ) );
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
CAkSubTrackCtx::CAkSubTrackCtx( CAkMusicCtx* in_pParentCtx, CAkMusicTrack* in_pTrackNode, AkUInt32 in_uSubTrack )
	 : CAkMusicCtx( in_pParentCtx )
	 , m_pTrackNode( in_pTrackNode )
	 , m_uSubTrack( in_uSubTrack )
	 , m_iAudibleTime( 0 )
	 , m_bSequencerInitDone( false )
	 , m_bStopReleaseDone( false )
{
	if( m_pTrackNode )
		m_pTrackNode->AddRef();
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
CAkSubTrackCtx::~CAkSubTrackCtx()
{
	m_sequencer.Term();

	if( m_pTrackNode )
		m_pTrackNode->Release();
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void CAkSubTrackCtx::Init( CAkRegisteredObj* in_GameObject, UserParams& in_rUserparams, bool in_bPlayDirectly )
{
	CAkMusicCtx::Init( in_GameObject, in_rUserparams, in_bPlayDirectly );
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
CAkSegmentCtx* CAkSubTrackCtx::SegmentCtx() const
{
	AKASSERT( Parent() );
	return static_cast<CAkSegmentCtx*>( Parent() );
}

// -------------------------------------------------------------------
// Sets sub-track's audible time, relative to pre-entry.
// -------------------------------------------------------------------
void CAkSubTrackCtx::Prepare( AkInt32 in_iAudibleTime ) // Time is relative to pre-entry
{
	m_iAudibleTime = in_iAudibleTime;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void CAkSubTrackCtx::OnPlayed()
{
	CAkMusicCtx::OnPlayed();

	AkModulatorTriggerParams params;
	params.pGameObj			= SegmentCtx()->GameObjectPtr();
	params.playingId		= SegmentCtx()->PlayingID();
	params.eTriggerMode		= AkModulatorTriggerParams::TriggerMode_FirstPlay;
	Track()->TriggerModulators( params, &m_ModulatorData );

	// Schedule audio clips playback in this segment.
	if( ! m_bSequencerInitDone )
	{
		ScheduleSequencerCmds();
		m_bSequencerInitDone = true;
	}
}

// -------------------------------------------------------------------
// Override MusicCtx OnStopped: Need to flush actions to release children.
// -------------------------------------------------------------------
void CAkSubTrackCtx::OnStopped()
{
	Flush();
	CAkMusicCtx::OnStopped();

	AKASSERT( SegmentCtx() );
	SegmentCtx()->RemoveReferences( this );

	// Decrement ref count if it hasn't already been done (i.e. OnStopped may be called more than once).
	if( ! m_bStopReleaseDone )
	{
		m_bStopReleaseDone = true;
		Release();
	}
}

// -------------------------------------------------------------------
// Override MusicCtx OnPaused on some platforms: Stop all sounds and prepare to restart them on resume (WG-19814).
// -------------------------------------------------------------------
void CAkSubTrackCtx::OnPaused()
{
	// Execute pause, even if we are going to stop all our children, in order to keep the Transport happy.
	CAkMusicCtx::OnPaused();

#ifdef AK_STOP_MUSIC_ON_PAUSE
	// Stop all children (with min time transition).
	// Note: Children of segment contexts are MusicPBIs. They cannot be destroyed from within OnStopped().
	ChildrenCtxList::Iterator it = m_listChildren.Begin( );
	while ( it != m_listChildren.End( ) )
	{
#ifndef AK_OPTIMIZED
		static_cast<CAkMusicPBI*>(*it)->MonitorPaused();
#endif
		CAkChildCtx* pChildCtx = static_cast<CAkChildCtx*>(*it);
		++it;
		pChildCtx->OnLastFrame( AK_NO_IN_BUFFER_STOP_REQUESTED );
	}
	Flush();
#endif	// AK_STOP_MUSIC_ON_PAUSE
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
#ifdef AK_STOP_MUSIC_ON_PAUSE
void CAkSubTrackCtx::OnResumed()
{
	CAkMusicCtx::OnResumed();
}
#endif	// AK_STOP_MUSIC_ON_PAUSE

#ifndef AK_OPTIMIZED
// -------------------------------------------------------------------
// Catch MusicCtx OnEditDirty(): Stop and reschedule all audio clips.
// -------------------------------------------------------------------
void CAkSubTrackCtx::OnEditDirty()
{
	AKASSERT( false );
}
#endif

// ----------------------------------------------------------------------------------------------
// Flush sequencer commands and automation.
// ----------------------------------------------------------------------------------------------
void CAkSubTrackCtx::Flush()
{
	while( !m_listAutomation.IsEmpty() )
	{
		AkMusicAutomation* pAutomation = m_listAutomation.First();
		m_listAutomation.RemoveFirst();
		AkDelete(AkMemID_Object, pAutomation );
	}
	m_sequencer.Flush();
}

// ----------------------------------------------------------------------------------------------
// Instantiate and play MusicPBIs in current time window [in_iNow, in_iNow+in_uNumSamples[, 
// schedule and executes associated stop commands.
// Called by segment ctx to process the next frame.
// ----------------------------------------------------------------------------------------------
void CAkSubTrackCtx::Process(
		AkInt32		in_iTime,			// Current time (in samples) relative to the pre-entry.
		AkUInt32	in_uNumSamples,		// Time window size.
		AkReal32	in_fPlaybackSpeed )	// Playback speed.
{
	ProcessPrologue( in_uNumSamples	);

	// Execute all pending actions that should occur in the next audio frame.
	ExecuteSequencerCmds( in_iTime, in_uNumSamples, in_fPlaybackSpeed );

	// Process automation.
	// Note: Automation values are computed for the _end_ of the processing window.
	AkInt32 iClipTime = in_iTime + in_uNumSamples;
	AutomationList::Iterator it = m_listAutomation.Begin();
	for( ; it != m_listAutomation.End(); ++it )
	{
		(*it)->Apply( iClipTime );
	}

	ProcessEpilogue();
}

// ----------------------------------------------------------------------------------------------
// Schedules clip commands for playback at m_iAudibleTime.
// ----------------------------------------------------------------------------------------------
void CAkSubTrackCtx::ScheduleSequencerCmds()
{
	AKASSERT( m_pTrackNode );

	const CAkMusicTrack::TrackPlaylist & playList = m_pTrackNode->GetTrackPlaylist();
	AkUInt32 uNumClips = playList.Length();

	for( AkUInt32 uClip = 0; uClip < uNumClips; ++uClip )
	{
		if( m_uSubTrack == playList[uClip].uSubTrackIndex )
		{
			const AkTrackSrc & srcInfo = playList[uClip];
			if( CAkMusicSource * pSrc = m_pTrackNode->GetSourcePtr( srcInfo.srcID ) )
			{
				AkSrcTypeInfo * pSrcTypeInfo = pSrc->GetSrcTypeInfo();
				AKASSERT( pSrcTypeInfo );

				// Compute source's look ahead.
				// Look ahead is the source's look-ahead, if it is streaming, and has no prefetched data or
				// play position will not be 0.
				AkUInt32 uSrcLookAhead = 0;
				if ( ( pSrcTypeInfo->mediaInfo.Type == SrcTypeFile ) &&
					( !pSrc->IsZeroLatency() || m_iAudibleTime > (AkInt32)srcInfo.uClipStartPosition || srcInfo.iSourceTrimOffset != 0 ) )
				{
					uSrcLookAhead = pSrc->StreamingLookAhead();
				}

#ifdef AK_IOS
				// Look-ahead time boost for iOS AAC. This is NOT a codec-specific latency compensation.
				if ( CODECID_FROM_PLUGINID( pSrcTypeInfo->dwID ) == AKCODECID_AAC 
					&& !AK_PERF_OFFLINE_RENDERING )
				{
					uSrcLookAhead += MUSIC_TRACK_AAC_LOOK_AHEAD_TIME_BOOST;
				}
#endif
#ifdef AK_XMA_SUPPORTED
				if ( CODECID_FROM_PLUGINID( pSrcTypeInfo->dwID ) == AKCODECID_XMA )
				{
					uSrcLookAhead += AK_NUM_VOICE_REFILL_FRAMES;
					AkUInt32 uMinLookahead = AK_IM_HW_CODEC_MIN_LOOKAHEAD_XMA * AK_NUM_VOICE_REFILL_FRAMES;
					if ( uSrcLookAhead < uMinLookahead )
						uSrcLookAhead = uMinLookahead;
				}
#endif

#ifdef AK_ATRAC9_SUPPORTED
				if ( CODECID_FROM_PLUGINID( pSrcTypeInfo->dwID ) == AKCODECID_ATRAC9 )
				{
					uSrcLookAhead += AK_NUM_VOICE_REFILL_FRAMES;
					AkUInt32 uMinLookahead = AK_IM_HW_CODEC_MIN_LOOKAHEAD_ATRAC9 * AK_NUM_VOICE_REFILL_FRAMES;
					if ( uSrcLookAhead < uMinLookahead )
						uSrcLookAhead = uMinLookahead;
				}
#endif
#ifdef AK_IM_HW_CODEC_MIN_LOOKAHEAD_OPUS_WEM
				if (CODECID_FROM_PLUGINID(pSrcTypeInfo->dwID) == AKCODECID_AKOPUS_WEM)
				{
					uSrcLookAhead += AK_NUM_VOICE_REFILL_FRAMES;
					AkUInt32 uMinLookahead = AK_IM_HW_CODEC_MIN_LOOKAHEAD_OPUS_WEM * AK_NUM_VOICE_REFILL_FRAMES;
					if (uSrcLookAhead < uMinLookahead)
						uSrcLookAhead = uMinLookahead;
				}
#endif
#ifdef AK_NX
				if (CODECID_FROM_PLUGINID(pSrcTypeInfo->dwID) == AKCODECID_OPUSNX)
				{
					if (srcInfo.iSourceTrimOffset != 0)  // when seeking, compensate for CAkSrcOpusBase::GetNominalPacketPreroll
						uSrcLookAhead += AK_NUM_VOICE_REFILL_FRAMES*4;
					else
						uSrcLookAhead += AK_NUM_VOICE_REFILL_FRAMES;
				}
#endif

				// Start everything that exist after current position (taking segment's look-ahead into account).
				AkInt32 iStopAt = (AkInt32)( srcInfo.uClipStartPosition + srcInfo.uClipDuration );
				if ( m_iAudibleTime < iStopAt )
				{
					// Should be played.

					// Compute action time and source offset (start position).
					// If effective position is before clip start, then the source offset is the 
					// trim offset and the action time is the effective play position. 
					// Otherwise, the source offset is the trim offset plus the offset between the segment
					// position and the clip start position (wrapped over the source's duration),
					// and the action time is the segment position (now).

					AkInt32 iActionTime;
					AkInt32 iSourceOffset;
					AkInt32 iClipDuration;
					if ( m_iAudibleTime > (AkInt32)srcInfo.uClipStartPosition )
					{
						iActionTime = m_iAudibleTime - (AkInt32)uSrcLookAhead;
						iSourceOffset = ( ( m_iAudibleTime - (AkInt32)srcInfo.uClipStartPosition ) + srcInfo.iSourceTrimOffset ) % srcInfo.uSrcDuration;
						iClipDuration = (AkInt32)srcInfo.uClipDuration - (m_iAudibleTime - (AkInt32)srcInfo.uClipStartPosition);
					}
					else
					{
						iActionTime = (AkInt32)srcInfo.uClipStartPosition - (AkInt32)uSrcLookAhead;
						iSourceOffset = srcInfo.iSourceTrimOffset;
						iClipDuration = (AkInt32)srcInfo.uClipDuration;

						// Special case to compensate for XMA latency: shift action start time if and only
						// if source offset is 0 (seeking XMA is sample-accurate).
					}
					AKASSERT( iSourceOffset >= 0 );

					AkMusicActionPlayClip* pAction = AddSequencerPlayCmd( iActionTime, srcInfo, iClipDuration, iSourceOffset, uSrcLookAhead );
					if ( pAction )
					{
						// Query track for clip automation.
						if ( !pSrcTypeInfo->IsMidi() )
						{
							pAction->AttachClipAutomation( m_pTrackNode, uClip, AutomationType_Volume, srcInfo.uClipStartPosition );
							pAction->AttachClipAutomation( m_pTrackNode, uClip, AutomationType_LPF, srcInfo.uClipStartPosition );
							pAction->AttachClipAutomation( m_pTrackNode, uClip, AutomationType_HPF, srcInfo.uClipStartPosition );
							pAction->AttachClipAutomation( m_pTrackNode, uClip, AutomationType_FadeIn, srcInfo.uClipStartPosition );
							pAction->AttachClipAutomation( m_pTrackNode, uClip, AutomationType_FadeOut, srcInfo.uClipStartPosition );
						}
					}
				}
			}

			if ( srcInfo.eventID != AK_INVALID_UNIQUE_ID )
			{
				AkInt32 iStopAt = (AkInt32)( srcInfo.uClipStartPosition + srcInfo.uClipDuration );
				if ( m_iAudibleTime <= iStopAt )
				{
					AddSequencerPostEventCmd( srcInfo.uClipStartPosition, srcInfo.eventID );
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------
void CAkSubTrackCtx::ExecuteSequencerCmds(
	AkInt32		in_iTime,			// Current time (relative to pre-entry).
	AkUInt32	in_uNumSamples,		// Time window size.
	AkReal32	in_fPlaybackSpeed )	// Playback speed.
{
	AkMusicAction * pAction;
	// Get next action.
	while ( m_sequencer.PopImminentAction( in_iTime, in_uNumSamples, pAction ) == AK_DataReady )
	{
		// Execute action and destroy it.
		AkUInt32 uWindowOffset = pAction->Time() - in_iTime;
		switch ( pAction->Type() )
		{
		case MusicActionTypePostEvent:
			{
				AkMusicActionPostEvent* pActionPostEvent = static_cast<AkMusicActionPostEvent*>( pAction );

				CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( pActionPostEvent->eventID );
				CAkRegisteredObj* pGameObj = SegmentCtx()->GameObjectPtr();
				UserParams& UserParam = SegmentCtx()->GetUserParams();

				if ( pEvent )
				{
					// Send the notification before any resulting from the actions in the event
					MONITOR_EVENTTRIGGERED(UserParam.PlayingID(), pActionPostEvent->eventID, pGameObj->ID(), UserParam.CustomParam());
					CAkAudioMgr::ExecuteEvent( pEvent, pGameObj, pGameObj->ID(), UserParam.PlayingID(), AK_INVALID_PLAYING_ID, UserParam.CustomParam(), uWindowOffset );
					pEvent->Release();
				}
				else
				{
					MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_EventIDNotFound, pActionPostEvent->eventID, UserParam.PlayingID(), pGameObj->ID(), pActionPostEvent->eventID, false);
				}
			}
			break;
		case MusicActionTypeStop:
			{
				AkMusicActionStopClip* pActionStop = static_cast<AkMusicActionStopClip*>(pAction);
				pActionStop->pTargetPBI->_Stop( uWindowOffset );

				// Apply and destroy all automation related to this PBI if applicable.
				if ( pActionStop->bHasAutomation )
				{
					AutomationList::IteratorEx it = m_listAutomation.BeginEx();
					while ( it != m_listAutomation.End() )
					{
						if ( (*it)->pPBI == pActionStop->pTargetPBI )
						{
							AkMusicAutomation * pAutomation = (*it);
							// Note: Automation values are computed for the _end_ of the processing window.
							pAutomation->Apply( in_iTime + in_uNumSamples );
							it = m_listAutomation.Erase( it );
							AkDelete(AkMemID_Object, pAutomation );
						}
						else
							++it;
					}
				}
			}
			break;

		case MusicActionTypePlay:
			{
				AkMusicActionPlayClip* pActionPlay = static_cast<AkMusicActionPlayClip*>(pAction);
				const AkTrackSrc & srcInfo = pActionPlay->TrackSrc();
				CAkChildCtx* pSrcCtx = NULL;

				if ( CreatePlayCtx( pActionPlay, srcInfo, uWindowOffset, in_fPlaybackSpeed, pSrcCtx ) )
				{
					if ( !IsLastFrame() )
					{
						// Enqueue an explicit Stop action.
						// Action must occur in 
						// source duration + begin_trim_offset + end_trim_offset
						AkMusicActionStopClip* pActionStop = AddSequencerStopCmd( (srcInfo.uClipStartPosition + srcInfo.uClipDuration), pSrcCtx );
						if ( pActionStop )
						{
							// Register automation to segment context.
							while ( AkMusicAutomation * pAutomation = pActionPlay->PopAutomation() )
							{
								pAutomation->pPBI = static_cast< CAkMusicPBI* >(pSrcCtx);
								pActionStop->bHasAutomation = true;
								m_listAutomation.AddFirst( pAutomation );
								if ( pAutomation->pAutomationData->Type() == AutomationType_FadeIn )
								{
									// Fade in.
									// Adjust audio frame and source offsets to the closest frame boundary. 
									static_cast< CAkMusicPBI* >(pSrcCtx)->FixStartTimeForFadeIn();
								}
								else if ( pAutomation->pAutomationData->Type() == AutomationType_FadeOut )
								{
									// Fade out. Set flag on music PBI so that it bypasses its intra-frame stopping mechanism.
									static_cast< CAkMusicPBI* >(pSrcCtx)->SetFadeOut();
								}
							}
						}
						else
						{
							// Cannot schedule a stop action for this PBI: do not play it.
							pSrcCtx->_Stop( 0 );
						}
					}
					else
					{
						// We played a PBI after OnLastFrame() was processed. Execute _Stop now.
						// NOTE: Get number of samples in last frame directly from CAkMusicCtx:
						// if it is set to AK_NO_IN_BUFFER_STOP_REQUESTED, we want to pass it as is
						// to our music PBI in order to have a smooth stop.
						pSrcCtx->_Stop( GetNumSamplesInLastFrame() );

						// Apply automation now and destroy.
						while ( AkMusicAutomation * pAutomation = pActionPlay->PopAutomation() )
						{
							pAutomation->pPBI = static_cast< CAkMusicPBI* >(pSrcCtx);
							// Note: Automation values are computed for the _end_ of the processing window.
							pAutomation->Apply( in_iTime + in_uNumSamples );
							AkDelete(AkMemID_Object, pAutomation );
						}
					}
				}
			}
			break;

		default:
			AKASSERT( !"Invalid action" );
		}
		AkDelete(AkMemID_Object, pAction );
	}
}

// ----------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------
bool CAkSubTrackCtx::CreatePlayCtx(
			  AkMusicActionPlayClip*	in_pActionPlay,
			  const AkTrackSrc&			in_rSrcInfo,
			  const AkUInt32			in_uWindowOffset,
			  const AkReal32			in_fPlaybackSpeed,
			  CAkChildCtx*&				out_pSrcCtx )
{
	AKASSERT( m_pTrackNode );
	AKRESULT eResult = AK_Fail;

	// Trans params: No transition for sources that start playing in NextFrame processing.
	TransParams transParams;

	const AkTrackSrc& rSrcInfo = in_pActionPlay->TrackSrc();
	CAkMusicSource* pSource = m_pTrackNode->GetSourcePtr( rSrcInfo.srcID );
	if ( pSource )
	{		
		CAkSegmentCtx* pSegmentCtx = SegmentCtx();
		AKASSERT( pSegmentCtx );

		if ( ! pSource->IsMidi() )
		{
			CAkMusicPBI* pMusicPBI;
			eResult = CAkMusicRenderer::Play( this,
				m_pTrackNode,
				pSource,
				pSegmentCtx->GameObjectPtr(),
				transParams,
				pSegmentCtx->GetUserParams(),
				pSegmentCtx->PlayDirectly(),
				&rSrcInfo,
				in_pActionPlay->SourceOffset(),
				in_pActionPlay->PlayOffset() + in_uWindowOffset,
				in_fPlaybackSpeed,
				pMusicPBI );

			out_pSrcCtx = pMusicPBI;
		}
		else
		{
			CAkMidiClipCtx* pMidiCtx;
			CAkMidiClipMgr* pMidiMgr = static_cast<CAkMatrixAwareCtx*>( pSegmentCtx->Parent() )->Sequencer()->GetMidiClipMgr();

			AKASSERT( in_pActionPlay->PlayOffset() == 0 );

			eResult = pMidiMgr->AddClipCtx(
				this,
				m_pTrackNode,
				pSource,
				pSegmentCtx->GameObjectPtr(),
				transParams,
				pSegmentCtx->GetUserParams(),
				&rSrcInfo,
				in_pActionPlay->PlayDuration(),
				in_pActionPlay->SourceOffset(),
				in_uWindowOffset,
				pMidiCtx );
			out_pSrcCtx = pMidiCtx;
		}
	}

	return eResult == AK_Success;
}

// ----------------------------------------------------------------------------------------------
// Called when PBI destruction occurred from Lower engine without the higher-level hierarchy knowing it.
// Remove all references to this target from sequencer.
// ----------------------------------------------------------------------------------------------
void CAkSubTrackCtx::RemoveReferences( CAkChildCtx* in_pCtx )
{
	// Remove all references from the action scheduler.
	m_sequencer.ClearTargetCtx( in_pCtx );

	// Remove all references from the automation list.
	AutomationList::IteratorEx it = m_listAutomation.BeginEx();
	while( it != m_listAutomation.End() )
	{
		if ( (*it)->pPBI == in_pCtx )
		{
			AkMusicAutomation* pAutomation = (*it);
			it = m_listAutomation.Erase( it );
			AkDelete(AkMemID_Object, pAutomation );
		}
		else
			++it;
	}
}

// ----------------------------------------------------------------------------------------------
// Computes source offset (in file) and frame offset (in pipeline time, relative to now) required to make a given source
// restart (become physical) sample-accurately.
// Returns false if restarting is not possible because of the timing constraints.
// ----------------------------------------------------------------------------------------------
bool CAkSubTrackCtx::GetSourceInfoForPlaybackRestart(
	const CAkMusicPBI* in_pCtx,		// Context which became physical.
	AkInt32& out_iLookAhead,		// Returned required look-ahead time (frame offset).
	AkInt32& out_iSourceOffset )	// Returned required source offset ("core" sample-accurate offset in source).
{
	// This context can be playing, about to stop, or even already stopped in the case of platforms which
	// may take several lower engine frames before stopping after being told so by behavioral engines.
	// Music PBIs should not bother becoming physical if the segment context is stopping.
	AKASSERT( !IsIdle() );
	if ( IsStopping() )
		return false;

	// Should still be connected to segment, and segment should still have an owner!
	CAkSegmentCtx* pSegmentCtx = static_cast<CAkSegmentCtx*>( Parent() );
	AKASSERT( pSegmentCtx && pSegmentCtx->GetOwner() );

	// Compute and set new source offset and look-ahead time.
	CAkMusicSource* pSrc = static_cast<CAkMusicSource*>( in_pCtx->GetSource() );
	AKASSERT( pSrc || !"PBI has no source" );

	AkSrcTypeInfo* pSrcTypeInfo = pSrc->GetSrcTypeInfo();
	AKASSERT( pSrcTypeInfo );

	// Get required look-ahead for this source at position iSourceOffset.
	// Look ahead is the source's look-ahead, if it is streaming (can't use prefetch data here... TODO Enable in Lower Engine).
	if ( pSrcTypeInfo->mediaInfo.Type == SrcTypeFile )
		out_iLookAhead = pSrc->StreamingLookAhead();
	else
		out_iLookAhead = 0;

	// Get source's track info.
	const AkTrackSrc* pSrcInfo = ((CAkMusicPBI*)in_pCtx)->GetSrcInfo();

	// XMA and AAC latency compensation: use bigger look-ahead to compute source offset.
#ifdef AK_XMA_SUPPORTED
	if ( CODECID_FROM_PLUGINID( pSrcTypeInfo->dwID ) == AKCODECID_XMA )
	{
		out_iLookAhead += AK_NUM_VOICE_REFILL_FRAMES;
		AkInt32 iMinLookahead = AK_IM_HW_CODEC_MIN_LOOKAHEAD_XMA * (AkInt32)AK_NUM_VOICE_REFILL_FRAMES;
		if ( out_iLookAhead < iMinLookahead )
			out_iLookAhead = iMinLookahead;
	}
#endif
#ifdef AK_IOS
	if ( CODECID_FROM_PLUGINID( pSrcTypeInfo->dwID ) == AKCODECID_AAC
		&& !AK_PERF_OFFLINE_RENDERING )
	{
		out_iLookAhead += MUSIC_TRACK_AAC_LOOK_AHEAD_TIME_BOOST;
	}
#endif
#ifdef AK_ATRAC9_SUPPORTED
	if ( CODECID_FROM_PLUGINID( pSrcTypeInfo->dwID ) == AKCODECID_ATRAC9 )
	{
		out_iLookAhead += AK_NUM_VOICE_REFILL_FRAMES;
		AkUInt32 uMinLookahead = AK_IM_HW_CODEC_MIN_LOOKAHEAD_ATRAC9 * AK_NUM_VOICE_REFILL_FRAMES;
		if (out_iLookAhead < uMinLookahead)
			out_iLookAhead = uMinLookahead;
	}
#endif
#ifdef AK_IM_HW_CODEC_MIN_LOOKAHEAD_OPUS_WEM
	if (CODECID_FROM_PLUGINID(pSrcTypeInfo->dwID) == AKCODECID_AKOPUS_WEM)
	{
		out_iLookAhead += AK_NUM_VOICE_REFILL_FRAMES;
		AkUInt32 uMinLookahead = AK_IM_HW_CODEC_MIN_LOOKAHEAD_OPUS_WEM * AK_NUM_VOICE_REFILL_FRAMES;
		if (out_iLookAhead < uMinLookahead)
			out_iLookAhead = uMinLookahead;
	}
#endif
#ifdef AK_NX
	if (CODECID_FROM_PLUGINID(pSrcTypeInfo->dwID) == AKCODECID_OPUSNX)
	{
		if (pSrcInfo->iSourceTrimOffset != 0) // when seeking, compensate for CAkSrcOpusBase::GetNominalPacketPreroll
			out_iLookAhead += AK_NUM_VOICE_REFILL_FRAMES * 4;
		else
			out_iLookAhead += AK_NUM_VOICE_REFILL_FRAMES;
	}
#endif

	// Round up look ahead time to AK_NUM_VOICE_REFILL_FRAMES, so that voice restart coincides with 
	// an audio frame (fade ins can only be smooth when applied to a full audio frame).
	out_iLookAhead = ( ( out_iLookAhead + AK_NUM_VOICE_REFILL_FRAMES - 1 ) / (AkUInt32)AK_NUM_VOICE_REFILL_FRAMES ) * AK_NUM_VOICE_REFILL_FRAMES;

	// Compute absolute segment position.
	// Note: Handling of virtual voices occur _after_ processing for next frame, so the engine's time has been
	// ticked already. Need to go back in time (-AK_NUM_VOICE_REFILL_FRAMES).
	// TODO: Centralize all ticks to one generic call at the end of an audio frame.
	AkUInt32 uTimeWindowSize = pSegmentCtx->GetTimeWindowSize();
	AkInt32 iAbsoluteSegmentPosition = pSegmentCtx->GetClipPosition() - uTimeWindowSize;

	// Compute source offset according to current segment position, and 
	AkInt32 iClipStartOffset = iAbsoluteSegmentPosition - ( (AkInt32)pSrcInfo->uClipStartPosition );

	// Check restart position (taking the required look-ahead time into account) against scheduled stop position.
	AkInt32 iStopAt = (AkInt32)( pSrcInfo->uClipStartPosition + pSrcInfo->uClipDuration );
	if ( iAbsoluteSegmentPosition + out_iLookAhead >= iStopAt )
		return false;	// Restarting will occur after scheduled stop.

	// Compute source offset: translate clip start offset into source offset, taking its required look-ahead time into account.
	out_iSourceOffset = ( iClipStartOffset + pSrcInfo->iSourceTrimOffset + out_iLookAhead ) % pSrcInfo->uSrcDuration;
	if ( out_iSourceOffset < 0 ) 
	{
		AKASSERT( !"The look-ahead time should have compensated for the negative segment position" );
		out_iSourceOffset = 0;
	}

	return true;
}

// ----------------------------------------------------------------------------------------------
// Broadcast trigger modulators request to child contexts.
// ----------------------------------------------------------------------------------------------
void CAkSubTrackCtx::OnTriggerModulators()
{
	// Trigger the modulators
	AkModulatorTriggerParams params;
	params.pGameObj			= SegmentCtx()->GameObjectPtr();
	params.playingId		= SegmentCtx()->PlayingID();
	params.eTriggerMode		= AkModulatorTriggerParams::TriggerMode_ParameterUpdated;
	Track()->TriggerModulators( params, &m_ModulatorData );
}

void CAkSubTrackCtx::SetPBIFade(void * in_pOwner, AkReal32 in_fFadeRatio)
{
	AkDeltaMonitorObjBrace brace(m_pTrackNode->key);
	CAkMusicCtx::SetPBIFade(in_pOwner, in_fFadeRatio);
}


#ifndef AK_OPTIMIZED
// ----------------------------------------------------------------------------------------------
// Function to retrieve the play target (the node which was played, resulting in a music PBI)
// ----------------------------------------------------------------------------------------------
AkUniqueID CAkSubTrackCtx::GetPlayTargetID( AkUniqueID in_playTargetID ) const
{
	if( Parent() )
		in_playTargetID = Parent()->GetPlayTargetID( in_playTargetID );
	return in_playTargetID;
}
#endif
