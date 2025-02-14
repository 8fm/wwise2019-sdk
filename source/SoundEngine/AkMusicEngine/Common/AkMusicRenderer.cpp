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
// AkMusicRenderer.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkMusicRenderer.h"
#include "AkURenderer.h"
#include "AkPlayingMgr.h"
#include "AkAudioLib.h"
#include "AkRegistryMgr.h"
#include "AkMatrixSequencer.h"
#include "AkMusicBank.h"
#include "ProxyMusic.h"
#include "AkMusicTransAware.h"
#include "AkBankMgr.h"
#include "AkSegmentInfoRepository.h"


//-----------------------------------------------------------------------------
// Default values defines.
//-----------------------------------------------------------------------------
#define DEFAULT_STREAMING_LOOK_AHEAD_RATIO	(1.f);	// Multiplication factor for all streaming look-ahead heuristic values.


// Define interface
namespace AK
{
	namespace MusicEngine
	{
		AKRESULT Init(
			AkMusicSettings *	in_pSettings	///< Initialization settings (can be NULL, to use the default values)
			)
		{
			if ( CAkMusicRenderer::Init( in_pSettings ) == AK_Success )
			{
				SoundEngine::RegisterGlobalCallback( CAkMusicRenderer::ProcessNextFrame, AkGlobalCallbackLocation_BeginRender, NULL, AkPluginTypeGlobalExtension, AKCOMPANYID_AUDIOKINETIC, AKEXTENSIONID_INTERACTIVEMUSIC );
				SoundEngine::RegisterGlobalCallback( CAkMusicRenderer::DestroyMusicRenderer, AkGlobalCallbackLocation_Term);
				SoundEngine::AddExternalStateHandler( CAkMusicRenderer::SetState );
				SoundEngine::AddExternalBankHandler( AkMusicBank::LoadBankItem );
				SoundEngine::AddExternalUnloadBankHandler( AkMusicBank::UnloadBankSlot );
#ifndef AK_OPTIMIZED
				SoundEngine::AddExternalProfileHandler( CAkMusicRenderer::HandleProfiling );

				AK::ProxyMusic::Init();
#endif
				return AK_Success;
			}
			return AK_Fail;
		}

		/// Get the default values of the initialization settings of the music engine.
		/// \sa
		/// - \ref soundengine_integration_init_advanced
		/// - AK::MusicEngine::Init()
		void GetDefaultInitSettings(
            AkMusicSettings &	out_settings	///< Returned default platform-independent sound engine settings
		    )
		{
			out_settings.fStreamingLookAheadRatio = DEFAULT_STREAMING_LOOK_AHEAD_RATIO;
		}

		void Term()
		{
			// Termination now done in bLastCall of global callback.

			SoundEngine::UnregisterGlobalCallback( CAkMusicRenderer::ProcessNextFrame );
			SoundEngine::UnregisterGlobalCallback( CAkMusicRenderer::DestroyMusicRenderer );
		}

		AKRESULT GetPlayingSegmentInfo(
			AkPlayingID		in_PlayingID,		// Playing ID returned by AK::SoundEngine::PostEvent()
			AkSegmentInfo &	out_segmentInfo,	// Position of the music segment (in ms) associated with that playing ID. The position is relative to the Entry Cue.
			bool			in_bExtrapolate		// Position is extrapolated based on time elapsed since last sound engine update.
			)
		{
			// Music position repository is owned by the music renderer.
			return CAkMusicRenderer::GetPlayingSegmentInfo( in_PlayingID, out_segmentInfo, in_bExtrapolate );
		}

	} // namespace MusicEngine
} // namespace AK

//------------------------------------------------------------------
// Defines.
//------------------------------------------------------------------
#define PENDING_STATE_CHANGES_MIN_ITEMS     (4)

//------------------------------------------------------------------
// Global variables.
//------------------------------------------------------------------
CAkMusicRenderer::MatrixAwareCtxList CAkMusicRenderer::m_listCtx;
CAkSegmentInfoRepository CAkMusicRenderer::m_segmentInfoRepository;
CAkMusicRenderer::PendingStateChanges CAkMusicRenderer::m_queuePendingStateChanges;
// Global music settings.
AkMusicSettings CAkMusicRenderer::m_musicSettings;
bool CAkMusicRenderer::m_bMustNotify = false;
bool CAkMusicRenderer::m_bLiveEdit = false;
#ifndef AK_OPTIMIZED
bool CAkMusicRenderer::m_bEditDirty = false;
#endif

void CAkMusicRenderer::Destroy()
{
	StopAll();

	CAkMusicTransAware::TermPanicTransitionRule();

    m_listCtx.Term();
	m_segmentInfoRepository.Term();
    m_queuePendingStateChanges.Term();
}

AKRESULT CAkMusicRenderer::Init(
	AkMusicSettings *	in_pSettings
	)
{
	// Store user settings.
	if ( in_pSettings )
	{
		m_musicSettings = *in_pSettings;
	}
	else
	{
		// Use defaults.
		AK::MusicEngine::GetDefaultInitSettings( m_musicSettings );
	}

	m_segmentInfoRepository.Init();

	m_bMustNotify = false;
	m_bLiveEdit = false;

#ifndef AK_OPTIMIZED
	m_bEditDirty = false;
#endif
	
	return m_queuePendingStateChanges.Reserve( PENDING_STATE_CHANGES_MIN_ITEMS );
}

void CAkMusicRenderer::ClearMusicPBIAndDecrement( CAkSoundBase* in_pSound, CAkMusicPBI*& io_pPBI, bool in_bCalledIncrementPlayCount, CAkRegisteredObj * in_pGameObj )
{
	CAkPBI* pbi = io_pPBI;
	CAkURenderer::ClearPBIAndDecrement( in_pSound, pbi, in_bCalledIncrementPlayCount, in_pGameObj );
	io_pPBI = NULL;
}

// Similar to URenderer::Play().
// Creates a Music PBI (a PBI that can be a child of a high-level context) and assigns a parent.
// Returns it
// Uses the parent's transition, the parent's game object
AKRESULT CAkMusicRenderer::Play( 
    CAkMusicCtx *		io_pParentCtx,
	CAkSoundBase*		in_pSound,
	CAkSource *			in_pSource,
    CAkRegisteredObj *	in_pGameObj,
    TransParams &		in_transParams,
    UserParams&			in_rUserparams,
	bool				in_bPlayDirectly,
	const AkTrackSrc *	in_pSrcInfo,		// Pointer to track's source playlist item.
	AkUInt32			in_uSourceOffset,	// Start position of source (in samples, at the native sample rate).
    AkUInt32			in_uFrameOffset,    // Frame offset for look-ahead and LEngine sample accuracy.
	AkReal32			in_fPlaybackSpeed,	// Playback rate.
    CAkMusicPBI *&		out_pPBI            // TEMP: Created PBI is needed to set the transition from outside.
    )
{
	AKRESULT eResult = AK_Fail;
	out_pPBI = NULL;
	bool bCalledIncrementPlayCount = false;
	bool bPlayingIDIncremented = false;
	AkMonitorData::NotificationReason eReason;
	AkBelowThresholdBehavior eBelowThresholdBehavior;
	bool bAllowedToPlayIfUnderThreshold = CAkURenderer::GetVirtualBehaviorAction(in_pSound, NULL, eBelowThresholdBehavior);

	AkReal32 fMaxRadius;
	PriorityInfoCurrent priority = CAkURenderer::_CalcInitialPriority( in_pSound, in_pGameObj, fMaxRadius );

	AKRESULT eValidateLimitsResult = CAkURenderer::ValidateLimits( priority.GetCurrent(), eReason );
	if( eValidateLimitsResult == AK_Fail )
	{
		eResult = AK_PartialSuccess;// That mean is not an error, the sound did not play simply because the max num instance made it abort.
	}
	else
	{
		PlayHistory history;
    	history.Init();

		AkPBIParams params;
		params.pGameObj = in_pGameObj;
		params.userParams = in_rUserparams;
		params.playHistory = history;
		params.uFrameOffset = in_uFrameOffset;
		params.bPlayDirectly = in_bPlayDirectly;

#ifndef AK_OPTIMIZED
		params.playTargetID = io_pParentCtx ? io_pParentCtx->GetPlayTargetID( AK_INVALID_UNIQUE_ID ) : AK_INVALID_UNIQUE_ID;
#endif

		out_pPBI = AkNew( AkMemID_Object, 
			CAkMusicPBI( params,
						 io_pParentCtx,
						 in_pSound,
						 in_pSource,
						 in_pSrcInfo,  
						 priority,
						 in_uSourceOffset,
						 in_fPlaybackSpeed
						 ) );

		bool bIsInitiallyUnderThreshold = false;
		if( out_pPBI )
		{
			eResult = out_pPBI->PreInit(fMaxRadius, NULL, bAllowedToPlayIfUnderThreshold, eReason, &params.initialSoundParams, bIsInitiallyUnderThreshold);
		}
		else
		{
			eResult = AK_Fail;
		}

		if( eResult == AK_Success )
		{ 
			bool bAllowKicking = !bIsInitiallyUnderThreshold || eBelowThresholdBehavior == AkBelowThresholdBehavior_ContinueToPlay;
			eResult = CAkURenderer::IncrementPlayCountAndInit(in_pSound, in_pGameObj, priority.GetCurrent(), eValidateLimitsResult, bAllowedToPlayIfUnderThreshold, eReason, out_pPBI, bAllowKicking);
			bCalledIncrementPlayCount = true;
			bPlayingIDIncremented = (eResult == AK_Success);
		}

		if( eResult == AK_Success )
		{
			// Trigger the modulators
			AkModulatorTriggerParams modParams;
			modParams.pGameObj = in_pGameObj;
			modParams.playingId = out_pPBI->GetPlayingID();
			modParams.pPbi = out_pPBI;
			modParams.uFrameOffset = in_uFrameOffset;
			modParams.eTriggerMode = AkModulatorTriggerParams::TriggerMode_SubsequentPlay;

			in_pSound->TriggerModulators(modParams, &out_pPBI->GetModulatorData());

			Play( out_pPBI, in_transParams/*, in_ePlaybackState*/ );
		}
	}

	///////////////////////////
	// ERROR Handling
	///////////////////////////

	if( eResult != AK_Success )
	{
		if (!bPlayingIDIncremented && in_rUserparams.PlayingID())
		{			
			//Make sure the PlayingManager can manage the Event count properly.  Normally this should have been done in IncrementPlayCount, 
			//but in case of error it could have been skipped.
			AkUInt32 uDontCare;
			g_pPlayingMgr->SetPBI( in_rUserparams.PlayingID(), out_pPBI, &uDontCare );		
		}
		ClearMusicPBIAndDecrement( in_pSound, out_pPBI, bCalledIncrementPlayCount, in_pGameObj );

		if( eResult != AK_PartialSuccess )
		{
			eReason = AkMonitorData::NotificationReason_PlayFailed;
			MONITOR_ERROREX( AK::Monitor::ErrorCode_PlayFailed, in_rUserparams.PlayingID(), in_pGameObj->ID(), in_pSound->ID(), false );
		}

		PlayHistory playHistory;
		playHistory.Init();
		in_pSound->MonitorNotif( 
			eReason,
			in_pGameObj->ID(),
			in_rUserparams,
			playHistory );
	}

	return eResult;
} // Play


void CAkMusicRenderer::Play(	
    CAkMusicPBI *   in_pContext, 
    TransParams &   in_transParams
    /*,
	AkPlaybackState	in_ePlaybackState*/
	)
{
	// Add PBI context to Upper Renderer's list.
    CAkURenderer::EnqueueContext( in_pContext );
	
    in_pContext->_InitPlay();
    
	bool l_bPaused = false;
    /* TODO
	// Check if the play command is actually a play-pause.
	if( in_ePlaybackState == PB_Paused )
	{
		l_bPaused = true;
	}
    */

	in_pContext->_Play( in_transParams, l_bPaused, true );
}

// Stops all top-level contexts.
bool CAkMusicRenderer::StopAll()
{
	bool bWasSomethingStopped = false;
    // Look among our top-level children.
	// Note. Contexts may dequeue themselves while being stopped
	MatrixAwareCtxList::Iterator it = m_listCtx.Begin();
	while ( it != m_listCtx.End() )
	{
		// Cache our pointer on the stack, as it could self-destruct inside OnStopped().
		CAkMatrixAwareCtx * pCtx = (*it);
		++it;
		TransParams transParams;
		pCtx->_Stop( transParams, 0 );	// Stop immediately.
		bWasSomethingStopped = true;
	}

	return bWasSomethingStopped;
}


// Game triggered actions (stop/pause/resume).
void CAkMusicRenderer::Stop(	
    CAkMusicNode *      in_pNode,
    CAkRegisteredObj *  in_pGameObj,
    TransParams &       in_transParams,
	AkPlayingID			in_playingID
    )
{
    // Look among our top-level children.
    // Note. Contexts may dequeue themselves while being stopped.
    MatrixAwareCtxList::Iterator it = m_listCtx.Begin();
	while ( it != m_listCtx.End() )
    {
		// Cache our pointer on the stack, as it could self-destruct inside OnStopped().
        CAkMatrixAwareCtx * pCtx = (*it);
		++it;

        if( pCtx->Node() == in_pNode )
		{
			if( CheckObjAndPlayingID( in_pGameObj, pCtx->Sequencer()->GameObjectPtr(), in_playingID, pCtx->Sequencer()->PlayingID() ) )
			{
				pCtx->_Stop( in_transParams );
			}
		}
    }
}

void CAkMusicRenderer::Pause(	
    CAkMusicNode *      in_pNode,
    CAkRegisteredObj *  in_pGameObj,
    TransParams &       in_transParams,
	AkPlayingID			in_playingID
    )
{
    // Look among our top-level children.
    MatrixAwareCtxList::Iterator it = m_listCtx.Begin();
	while ( it != m_listCtx.End() )
    {
        if( (*it)->Node() == in_pNode )
		{
			if( CheckObjAndPlayingID( in_pGameObj, (*it)->Sequencer()->GameObjectPtr(), in_playingID, (*it)->Sequencer()->PlayingID() ) )
			{
                (*it)->_Pause( in_transParams );
			}
		}
        ++it;
	} 
}

void CAkMusicRenderer::Resume(	
    CAkMusicNode *      in_pNode,
    CAkRegisteredObj *  in_pGameObj,
    TransParams &       in_transParams,
    bool                in_bMasterResume,    // REVIEW
	AkPlayingID			in_playingID
    )
{
    // Look among our top-level children.
    MatrixAwareCtxList::Iterator it = m_listCtx.Begin();
	while ( it != m_listCtx.End() )
    {
        if( (*it)->Node() == in_pNode )
		{
			if( CheckObjAndPlayingID( in_pGameObj, (*it)->Sequencer()->GameObjectPtr(), in_playingID, (*it)->Sequencer()->PlayingID() ) )
			{
                (*it)->_Resume( in_transParams, in_bMasterResume );
			}
		}
        ++it;
	}
}

void CAkMusicRenderer::SeekTimeAbsolute(	
    CAkMusicNode *		in_pNode,
    CAkRegisteredObj *  in_pGameObj,
	AkPlayingID			in_playingID,
	AkTimeMs			in_iSeekTime,
	bool				in_bSnapToCue
	)
{
	// Search segment contexts among our top-level children.
    MatrixAwareCtxList::Iterator it = m_listCtx.Begin();
	while ( it != m_listCtx.End() )
    {
		// Cache pointer in case destruction occurs while iterating in the list.
		CAkMatrixAwareCtx * pCtx = (*it);
		++it;
        if( pCtx->Node() == in_pNode )
		{
			if( !in_pGameObj || pCtx->Sequencer()->GameObjectPtr() == in_pGameObj )
			{
				if( in_playingID == AK_INVALID_PLAYING_ID || pCtx->Sequencer()->PlayingID() == in_playingID )
				{
					if ( pCtx->SeekTimeAbsolute( in_iSeekTime, in_bSnapToCue ) == AK_Success )
					{
						UserParams & userParams = pCtx->Sequencer()->GetUserParams();
						g_pPlayingMgr->NotifyMusicPlayStarted( userParams.PlayingID() );
						MONITOR_OBJECTNOTIF(userParams.PlayingID(), pCtx->Sequencer()->GameObjectPtr()->ID(), userParams.CustomParam(), AkMonitorData::NotificationReason_Seek, CAkCntrHist(), pCtx->Node()->ID(), false, in_iSeekTime );
					}
					else
					{
						MONITOR_ERRORMSG_PLAYINGID("Music Renderer: Seeking failed", in_playingID);
					}
				}
			}
		}
	} 
}

void CAkMusicRenderer::SeekPercent(	
    CAkMusicNode *		in_pNode,
    CAkRegisteredObj *  in_pGameObj,
	AkPlayingID			in_playingID,
	AkReal32			in_fSeekPercent,
	bool				in_bSnapToCue
	)
{
	// Search segment contexts among our top-level children.
    MatrixAwareCtxList::Iterator it = m_listCtx.Begin();
	while ( it != m_listCtx.End() )
    {
		// Cache pointer in case destruction occurs while iterating in the list.
		CAkMatrixAwareCtx * pCtx = (*it);
		++it;
        if( pCtx->Node() == in_pNode )
		{
			if( !in_pGameObj || pCtx->Sequencer()->GameObjectPtr() == in_pGameObj )
			{
				if( in_playingID == AK_INVALID_PLAYING_ID || pCtx->Sequencer()->PlayingID() == in_playingID )
				{
					if ( pCtx->SeekPercent( in_fSeekPercent, in_bSnapToCue ) == AK_Success )
					{
						UserParams & userParams = pCtx->Sequencer()->GetUserParams();
						g_pPlayingMgr->NotifyMusicPlayStarted( userParams.PlayingID() );
						AkReal32 fPercent = in_fSeekPercent*100;
						MONITOR_OBJECTNOTIF(userParams.PlayingID(), pCtx->Sequencer()->GameObjectPtr()->ID(), userParams.CustomParam(), AkMonitorData::NotificationReason_SeekPercent, CAkCntrHist(), pCtx->Node()->ID(), false, *(AkTimeMs*)&fPercent );
					}
					else
					{
						MONITOR_ERRORMSG_PLAYINGID("Music Renderer: Seeking failed", in_playingID);
					}
				}
			}
		}
	} 
}

void CAkMusicRenderer::StopNoteIfUsingData(CAkUsageSlot* in_pSlot, bool in_bFindOtherSlot)
{
	for (MatrixAwareCtxList::Iterator it = m_listCtx.Begin(); it != m_listCtx.End(); ++it)
	{
		CAkMatrixAwareCtx* pCtx = (*it);
		CAkMatrixSequencer* pSeq = pCtx->Sequencer();

		if(pSeq)
		{
			CAkMidiClipMgr* pClipMgr = pSeq->GetMidiClipMgr();
			pClipMgr->StopNoteIfUsingData(in_pSlot, in_bFindOtherSlot);
		}
	}
}

void CAkMusicRenderer::MusicNotification(bool in_bLiveEdit)
{
	// Put off notification until next frame
	m_bMustNotify = true;
	m_bLiveEdit = in_bLiveEdit;
}

void CAkMusicRenderer::DoMusicNotification()
{
	if( m_bMustNotify )
	{
		// Just set all top-level nodes dirty.
		MatrixAwareCtxList::Iterator it = m_listCtx.Begin();
		while ( it != m_listCtx.End() )
		{
			(*it)->Sequencer()->SetParametersDirty( m_bLiveEdit );
			++it;
		}
		m_bMustNotify = false;
		m_bLiveEdit = false;
	}
}

// Add/Remove Top-Level Music Contexts (happens when created from MusicNode::Play()).
AKRESULT CAkMusicRenderer::AddChild( 
    CAkMatrixAwareCtx * in_pMusicCtx,
    UserParams &        in_rUserparams,
    CAkRegisteredObj *  in_pGameObj,
	bool				in_bPlayDirectly
    )
{
	AKRESULT eResult = AK_Fail;
    // Create and enqueue a top-level sequencer.
    CAkMatrixSequencer * pSequencer = AkNew(AkMemID_Object, CAkMatrixSequencer( in_pMusicCtx, in_rUserparams, in_pGameObj, in_bPlayDirectly ) );
    if ( pSequencer )
    {
        CAkMusicNode * pNode = in_pMusicCtx->Node();
		if ( pNode && pNode->IncrementActivityCount() )
		{
			pNode->AddToURendererActiveNodes();
			m_listCtx.AddFirst( in_pMusicCtx );

            // TODO Enforce do not set sequencer elsewhere than here.
            in_pMusicCtx->SetSequencer( pSequencer );

            // We generated a Top-Level context and sequencer:
            // Register/Add ref to the Playing Mgr, so that it keeps the playing ID alive.
            if ( in_rUserparams.PlayingID() )
            {
				// IMPORTANT: Initialize callback flags because PlayingMgr does not set them if the entry
				// does not exist.
				AkUInt32 uCallbackFlags = 0;

                AKASSERT( g_pPlayingMgr );
                eResult = g_pPlayingMgr->SetPBI( in_rUserparams.PlayingID(), in_pMusicCtx, &uCallbackFlags );

				in_pMusicCtx->SetRegisteredNotif( uCallbackFlags );

				if ( uCallbackFlags & AK_EnableGetMusicPlayPosition )
				{
					if ( m_segmentInfoRepository.CreateEntry( in_rUserparams.PlayingID() ) != AK_Success )
					{
						// Failed creating an entry in the repository. Go on but ignore segment info queries.
						in_pMusicCtx->SetRegisteredNotif( uCallbackFlags & ~AK_EnableGetMusicPlayPosition );
					}
				}
				g_pPlayingMgr->NotifyMusicPlayStarted( in_rUserparams.PlayingID() );
            }
   		}
        else
        {
			if (pNode)
			{
				pNode->DecrementActivityCount();
				pNode->RemoveFromURendererActiveNodes();
			}

			// Destroy sequencer now if it wasn't assigned to the context yet. Otherwise, let contexts
			// remove themselves normally from the renderer.
            AkDelete(AkMemID_Object, pSequencer );
        }
    }

    return eResult;
}
void CAkMusicRenderer::RemoveChild( 
    CAkMatrixAwareCtx * in_pMusicCtx
    )
{
    // Note: This call may fail if the context was never added to the renderer's children, because 
	// CAkMusicRenderer::AddChild() failed (because no memory).
    m_listCtx.Remove( in_pMusicCtx );
    
	// Note: The context may not have a sequencer if it was not created because of an out-of-memory condition.
    CAkMatrixSequencer * pSequencer = in_pMusicCtx->Sequencer();
	if( pSequencer )
	{
		// Notify Playing Mgr.
		AKASSERT(g_pPlayingMgr);
		if ( pSequencer->PlayingID() )
		{
			if ( in_pMusicCtx->GetRegisteredNotif() & AK_EnableGetMusicPlayPosition )
				m_segmentInfoRepository.RemoveEntry( pSequencer->PlayingID() );

			g_pPlayingMgr->Remove( pSequencer->PlayingID(), in_pMusicCtx );
			if (in_pMusicCtx->Node())
			{
				in_pMusicCtx->Node()->DecrementActivityCount();
				in_pMusicCtx->Node()->RemoveFromURendererActiveNodes();
			}
		}
	
		AkDelete(AkMemID_Object, pSequencer );
	}
}

// Music Audio Loop interface.
void CAkMusicRenderer::DestroyMusicRenderer(AK::IAkGlobalPluginContext *, AkGlobalCallbackLocation, void *)
{
	CAkMusicRenderer::Destroy();
}

void CAkMusicRenderer::ProcessNextFrame(AK::IAkGlobalPluginContext *, AkGlobalCallbackLocation, void *)
{
	WWISE_SCOPED_PROFILE_MARKER("CAkMusicRenderer::ProcessNextFrame");

	// Refresh context hierarchies if sensitive live editing was performed.
#ifndef AK_OPTIMIZED
	if ( IsEditDirty() )
	{
		MatrixAwareCtxList::Iterator it = m_listCtx.Begin();
		while ( it != m_listCtx.End() )
		{
			(*it)->OnEditDirty();
			++it;
		}
		ClearEditDirty();
	}
#endif 

	// Propogate music notification
	DoMusicNotification();

    // Perform top-level segment sequencers.
	MatrixAwareCtxList::Iterator it = m_listCtx.Begin();
	while ( it != m_listCtx.End() )
    {
		// Cache our pointer on the stack in case dequeueing occurs from within Execute().
        CAkMatrixAwareCtx * pCtx = (*it);
		++it;

		CAkMatrixSequencer * pSequencer = pCtx->Sequencer();

		// Store current segment info before preparing context for next audio frame.
		if ( pCtx->GetRegisteredNotif() & AK_EnableGetMusicPlayPosition )
		{
			AkSegmentInfo segmentInfo;
			if ( pCtx->GetPlayingSegmentInfo( segmentInfo ) == AK_Success )
				m_segmentInfoRepository.UpdateSegmentInfo( pSequencer->PlayingID(), segmentInfo );
		}
		
		// Execute logic for next audio frame.
		pSequencer->Execute( AK_NUM_VOICE_REFILL_FRAMES );
	}
}

// Music objects queries.
AKRESULT CAkMusicRenderer::GetPlayingSegmentInfo(
	AkPlayingID		in_PlayingID,		// Playing ID returned by AK::SoundEngine::PostEvent()
	AkSegmentInfo &	out_segmentInfo,	// Position of the music segment (in ms) associated with that playing ID. The position is relative to the Entry Cue.
	bool			in_bExtrapolate		// Position is extrapolated based on time elapsed since last sound engine update.
	)
{
	return m_segmentInfoRepository.GetSegmentInfo( in_PlayingID, out_segmentInfo, in_bExtrapolate ); 
}

//
// States management.
//

// Returns true if state change was handled (delayed) by the Music Renderer. False otherwise.
bool CAkMusicRenderer::SetState( 
    AkStateGroupID     in_stateGroupID, 
    AkStateID          in_stateID
    )
{
    // Query top-level context sequencers that need to handle state change by delaying it.

    CAkMatrixAwareCtx * pChosenCtx = NULL;

    // Values for chosen context.
    AkInt64 iChosenRelativeSyncTime;  
    AkUInt32 uChosenSegmentLookAhead;

    if ( GetDelayedStateChangeData(
            in_stateGroupID,
            pChosenCtx,
            iChosenRelativeSyncTime,
            uChosenSegmentLookAhead ) <= 0 )
    {
        // Either a context requires an immediate change, or no one is registered to this state group.
        // Return now as "not handled".        
        return false;
    }


    //
    // Process delayed state change.
    // 
    AKASSERT( pChosenCtx );

    // Reserve a spot for pending state change.
    AkStateChangeRecord * pNewStateChange = m_queuePendingStateChanges.AddFirst();
    if ( !pNewStateChange )
    {
        // No memory. Return without handling delayed state change.
        // Invalidate all pending state changes that relate to this state group.
        PendingStateChangeIter it = m_queuePendingStateChanges.Begin();
    	InvalidateOlderPendingStateChanges( it, in_stateGroupID );
        return false;
    }

    
    // Delegate handling of delayed state change sequencing to the appropriate context sequencer.
    if ( pChosenCtx->Sequencer()->ProcessDelayedStateChange( pNewStateChange, 
                                                             uChosenSegmentLookAhead, 
                                                             iChosenRelativeSyncTime ) == AK_Success )
    {
        // Setup the rest of the state change record.
        pNewStateChange->stateGroupID   = in_stateGroupID;
        pNewStateChange->stateID        = in_stateID;
        pNewStateChange->bWasPosted     = false;
		pNewStateChange->bIsReferenced	= true;

        // Return True ("handled") unless a context required an immediate change.
        return true;
    }
    else
    {
        // Failed handling delayed state change.
        // Remove the record, return False ("not handled").
        AKVERIFY( m_queuePendingStateChanges.RemoveFirst() == AK_Success );
        return false;
    }
}

// Execute a StateChange music action.
void CAkMusicRenderer::PerformDelayedStateChange(
    void *             in_pCookie
    )
{
    // Find pending state change in queue. 
    PendingStateChangeIterEx it;
	// Post state change if required (if was not already posted).
	FindPendingStateChange( in_pCookie, it );
	(*it).bIsReferenced = false;
    if ( !(*it).bWasPosted )
    {
        (*it).bWasPosted = true;
		
        AkStateGroupID stateGroupID = (*it).stateGroupID;
        //
        // Set state on sound engine, with flag "skip call to state handler extension".
        // 
        AKVERIFY( AK::SoundEngine::SetState( 
            stateGroupID, 
            (*it).stateID, 
            false, 
            true ) == AK_Success ); 

		// Invalidate all older pending state changes (for this StateGroup ID).
        InvalidateOlderPendingStateChanges( ++it, stateGroupID );
    }
	// else State change is obsolete.
	
    
    // Clean up queue.
    CleanPendingStateChanges();
}

// Notify Renderer whenever a StateChange music action needs to be rescheduled.
void CAkMusicRenderer::RescheduleDelayedStateChange(
    void *              in_pCookie
    )
{
    // Find pending state change in queue. 
    PendingStateChangeIterEx it;    
    FindPendingStateChange( in_pCookie, it );
	if ( !(*it).bWasPosted )
	{
		AkStateGroupID stateGroupID = (*it).stateGroupID;

		// Values for chosen context.
		AkInt64 iChosenRelativeSyncTime;  
		AkUInt32 uChosenSegmentLookAhead;
		CAkMatrixAwareCtx * pChosenCtx = NULL;

		if ( GetDelayedStateChangeData(
				stateGroupID, 
				pChosenCtx,
				iChosenRelativeSyncTime,
				uChosenSegmentLookAhead ) <= 0 )
		{
			// Either a context requires an immediate change, or no one is registered to this state group.
			CancelDelayedStateChange( stateGroupID, it );
			return;
		}

		AKASSERT( pChosenCtx );

		//
		// Process delayed state change.
		// 
	    
		// Delegate handling of delayed state change sequencing to the appropriate context sequencer.
		if ( pChosenCtx->Sequencer()->ProcessDelayedStateChange( in_pCookie, 
																 uChosenSegmentLookAhead, 
																 iChosenRelativeSyncTime ) != AK_Success )
		{
			// Failed handling delayed state change.
			// Set state on sound engine now, remove the record.
			CancelDelayedStateChange( stateGroupID, it );
		}
	}
	else 
	{
		// State change was obsolete anyway, so mark it as "not referenced" and clean now.
		(*it).bIsReferenced = false;
		CleanPendingStateChanges();
	}
}


// Helpers.

// Set state on sound engine now, with flag "skip call to state handler extension".
// Clean pending delayed state change list.
void CAkMusicRenderer::CancelDelayedStateChange( 
    AkStateGroupID     in_stateGroupID, 
    PendingStateChangeIterEx & in_itPendingStateChg
    )
{
    //
    // Set state on sound engine now, with flag "skip call to state handler extension".
    // 
    AKVERIFY( AK::SoundEngine::SetState( 
        in_stateGroupID, 
        (*in_itPendingStateChg).stateID, 
        false, 
        true ) == AK_Success ); 

    // Invalidate record, clean.
    (*in_itPendingStateChg).bWasPosted = true;
	(*in_itPendingStateChg).bIsReferenced = false;
    InvalidateOlderPendingStateChanges( in_itPendingStateChg, in_stateGroupID );
    CleanPendingStateChanges();
}

// Query top-level context sequencers that need to handle state change by delaying it.
// Returns the minimal absolute delay for state change. Value <= 0 means "immediate".
AkInt64 CAkMusicRenderer::GetDelayedStateChangeData(
    AkStateGroupID          in_stateGroupID, 
    CAkMatrixAwareCtx *&    out_pChosenCtx,
    AkInt64 &               out_iChosenRelativeSyncTime,
    AkUInt32 &              out_uChosenSegmentLookAhead
    )
{
    AkInt64 iEarliestAbsoluteDelay = 0;
    out_pChosenCtx = NULL;

    MatrixAwareCtxList::Iterator itCtx = m_listCtx.Begin();
	while ( itCtx != m_listCtx.End() )
    {
		if ( (*itCtx)->IsPlaying()
			&& !(*itCtx)->IsPaused() )
		{
			AkInt64 iRelativeSyncTime;  // state change time relative to segment
			AkUInt32 uSegmentLookAhead;
			bool bStateGroupUsed = false;

			AkInt64 iAbsoluteDelay = (*itCtx)->Sequencer()->QueryStateChangeDelay( in_stateGroupID,
																				   bStateGroupUsed,
																				   uSegmentLookAhead,
																				   iRelativeSyncTime );
			if ( bStateGroupUsed
				&& ( !out_pChosenCtx || iAbsoluteDelay < iEarliestAbsoluteDelay )
				)
			{
				// This context requires a state change that should occur the earliest.
				iEarliestAbsoluteDelay = iAbsoluteDelay;

				out_iChosenRelativeSyncTime = iRelativeSyncTime;
				out_uChosenSegmentLookAhead = uSegmentLookAhead;
				out_pChosenCtx = (*itCtx);
			}
		}
        ++itCtx;
    }

    // NOTE. Since delayed processing always occurs one frame later, we substract one frame size out of the returned
    // delay. (Delays smaller than one frame will be considered as immediate).
    iEarliestAbsoluteDelay -= AK_NUM_VOICE_REFILL_FRAMES;
    return iEarliestAbsoluteDelay;
}

void CAkMusicRenderer::FindPendingStateChange( 
    void * in_pCookie,
    PendingStateChangeIterEx & out_iterator
    )
{
    out_iterator = m_queuePendingStateChanges.BeginEx();
    while ( out_iterator != m_queuePendingStateChanges.End() )
    {
        if ( &(*out_iterator) == in_pCookie )
        {
            // Found.
            break;
        }
        ++out_iterator;
    }

    // Must have been found.
    AKASSERT( out_iterator != m_queuePendingStateChanges.End() );
}

void CAkMusicRenderer::CleanPendingStateChanges()
{
    PendingStateChangeIterEx it = m_queuePendingStateChanges.BeginEx();
    while ( it != m_queuePendingStateChanges.End() )
    {
        // Dequeue if required (if ref count is 0).
        if ( !(*it).bIsReferenced )
        {
        	AKASSERT( (*it).bWasPosted );
            it = m_queuePendingStateChanges.Erase( it );
        }
        else
            ++it;
    }
}

void CAkMusicRenderer::InvalidateOlderPendingStateChanges( 
    PendingStateChangeIter & in_iterator,
    AkStateGroupID           in_stateGroupID
    )
{
    while ( in_iterator != m_queuePendingStateChanges.End() )
    {
        // Find next (older) pending state change with that same StateGroup ID.
        if ( (*in_iterator).stateGroupID == in_stateGroupID )
        {
            // Invalidate.
            (*in_iterator).bWasPosted = true;
        }
        ++in_iterator;
    }
}

#ifndef AK_OPTIMIZED
void CAkMusicRenderer::HandleProfiling()
{
	/*
	Get and post :
		SegmentID, 
		PlayingID 
		and position (in double 64 in ms)
	*/

	// We must first count them to make the initial allocation (only if required)
	AkUInt16 uNumPlayingIM = 0;
	MatrixAwareCtxList::Iterator it = m_listCtx.Begin();
	while ( it != m_listCtx.End() )
	{
		CAkMatrixAwareCtx * pCtx = (*it);
		++it;
		if( pCtx->Node()->NodeCategory() == AkNodeCategory_MusicSegment 
			&& pCtx->Sequencer()->GetUserParams().CustomParam().ui32Reserved & AK_EVENTFROMWWISE_RESERVED_BIT )
		{
			++uNumPlayingIM;
		}
	}
	if( uNumPlayingIM )
	{
		// We do have something to monitor, so let's gather the info.
		AkInt32 sizeofItem = SIZEOF_MONITORDATA_TO( segmentPositionData.positions )
						+ uNumPlayingIM * sizeof( AkMonitorData::SegmentPositionData );

		AkProfileDataCreator creator( sizeofItem );
		if ( !creator.m_pData )
			return;

		creator.m_pData->eDataType = AkMonitorData::MonitorDataSegmentPosition;

		creator.m_pData->segmentPositionData.numPositions = uNumPlayingIM;

		uNumPlayingIM = 0;
		it = m_listCtx.Begin();
		while ( it != m_listCtx.End() )
		{
			CAkMatrixAwareCtx * pCtx = (*it);
			++it;
			CAkMatrixSequencer* pSequencer = pCtx->Sequencer();
			if( pCtx->Node()->NodeCategory() == AkNodeCategory_MusicSegment 
			&& ( pSequencer->GetUserParams().CustomParam().ui32Reserved & AK_EVENTFROMWWISE_RESERVED_BIT )
			&& pCtx->IsPlaying() )
			{	
				AkMonitorData::SegmentPositionData& l_rdata = creator.m_pData->segmentPositionData.positions[ uNumPlayingIM ];
				AkInt32 iCurSegmentPosition = pSequencer->GetCurSegmentPosition();//in samples
				if( iCurSegmentPosition <= 0 )
				{
					l_rdata.f64Position = 0;//negative stands for not started yet, so we pass 0.
				}
				else
				{
					l_rdata.f64Position =	AkTimeConv::SamplesToSeconds( iCurSegmentPosition )*1000;
				}
				l_rdata.playingID =		pSequencer->PlayingID();
				l_rdata.segmentID =		pCtx->Node()->ID();
				l_rdata.customParam =	pSequencer->GetUserParams().CustomParam();

				++uNumPlayingIM;
			}
		}
	}
}
#endif
