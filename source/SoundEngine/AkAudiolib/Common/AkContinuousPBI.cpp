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
// AkContinuousPBI.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkContinuousPBI.h"
#include "AkTransitionManager.h"
#include "AkPathManager.h"
#include "AkAudioMgr.h"
#include "AkActionPlayAndContinue.h" 
#include "AkMultiPlayNode.h"         
#include "AkRanSeqCntr.h"            
#include "AkSoundBase.h"             

extern CAkAudioMgr*		g_pAudioMgr;
extern AkInitSettings	g_settings; 

#define MINIMAL_LENGTH_FOR_CROSSFADE 50.0f //in milliseconds

CAkContinuousPBI::CAkContinuousPBI( AkPBIParams&				in_params,
									CAkSoundBase*				in_pSound,		
								    CAkSource*					in_pSource,
									const PriorityInfoCurrent&	in_rPriority
									)
:CAkPBI( in_params,
		 in_pSound,		
		 in_pSource,
		 in_rPriority,
		 0
		 )
,m_spContList(in_params.pContinuousParams->spContList)
,m_fTransitionTime(0.0f)
,m_ulNextElementToPlay(AK_INVALID_UNIQUE_ID)
,m_eTransitionMode(Transition_Disabled)
,m_bIsFirstPlay(in_params.bIsFirst)
,m_bIsContinuousPaused(false)
,m_bIsNextPrepared(false)
{
	AKASSERT( m_pInstigator );
	m_pInstigator->AddRef();

	m_ulPauseCount = in_params.pContinuousParams->ulPauseCount;

	AKASSERT ( g_pTransitionManager );
	AKASSERT ( g_pPathManager );
	// have we got any ?
	if( m_PBTrans.pvPSTrans == NULL )
	{
		// no, then grab this one
		m_PBTrans.pvPSTrans = in_params.pContinuousParams->pPlayStopTransition;
		// Setting it to NULL means we took it, important
		in_params.pContinuousParams->pPlayStopTransition = NULL;
	}

	// have we got any ?
	if( m_PBTrans.pvPRTrans == NULL)
	{
		// no, then grab this one
		m_PBTrans.pvPRTrans = in_params.pContinuousParams->pPauseResumeTransition;
		// Setting it to NULL means we took it, important
		in_params.pContinuousParams->pPauseResumeTransition = NULL;
	}

	m_PlayHistoryForNextToPlay.Init();

	PrepareNextPlayHistory( in_params.playHistory );
}

AKRESULT CAkContinuousPBI::Init()
{
	AKRESULT eResult = CAkPBI::Init();
	if( eResult == AK_Success )
	{
		AKRESULT	Result;//NOT TO BE RETURNED, error handled internally
		if( m_PBTrans.pvPSTrans )
		{
			Result = g_pTransitionManager->AddTransitionUser( m_PBTrans.pvPSTrans, this );
			if( Result == AK_Success )
			{
				if( !g_pTransitionManager->IsTerminated( m_PBTrans.pvPSTrans ) )
				{
					MonitorFade( AkMonitorData::NotificationReason_Fade_Started, UNKNOWN_FADE_TIME );
				}
				else
				{
					if( m_PBTrans.pvPSTrans->IsFadingOut() )
					{
						m_eInitialState = PBI_InitState_Stopped;
					}
					g_pTransitionManager->RemoveTransitionUser( m_PBTrans.pvPSTrans, this );

					m_PBTrans.pvPSTrans = NULL;
				}
			}
			else 
			{
				// Error handling.
				//
				// We were specified a transition play-stop, but we were unable to Add ourself to the transition manager. So we cannot honor transition no matter what.
				// Since AddTransitionUser returned an error, it means the system does not have enough memory to add our pointer reference...
				// I am pretty sure, 99.9% the transition is OK since we are most likely being called by someone who have a reference on the transition.
				// But technically, we should not be allowed to access m_PBTrans.pvPSTrans anymore... but the Action is currently holding reference on it... but it would be a hack to allow us to use it.
				// Since we don't have enough memory to allocate 4 bytes... we will not have enough to play it anyway, so better just set it stopped anyway as better play nothing than leaking one sound when in low memory condition.

				//if( m_PBTrans.pvPSTrans && m_PBTrans.pvPSTrans->IsFadingOut() )
				{
					m_eInitialState = PBI_InitState_Stopped;
				}
				m_PBTrans.pvPSTrans = NULL;
			}
		}
		if( m_PBTrans.pvPRTrans )
		{
			Result = g_pTransitionManager->AddTransitionUser( m_PBTrans.pvPRTrans, this );
			if(Result == AK_Success)
			{
				if( !g_pTransitionManager->IsTerminated( m_PBTrans.pvPRTrans ) )
				{
					MonitorFade( AkMonitorData::NotificationReason_Fade_Started, UNKNOWN_FADE_TIME );
				}
				else
				{
					if( m_PBTrans.pvPRTrans->IsFadingOut() )
					{
						if( m_eInitialState == PBI_InitState_Playing )
						{
							m_eInitialState = PBI_InitState_Paused;
						}
					}
					g_pTransitionManager->RemoveTransitionUser( m_PBTrans.pvPRTrans, this );
					
					m_PBTrans.pvPRTrans = NULL;
				}
			}
			else
			{	
				m_PBTrans.pvPRTrans = NULL;
			}
		}
		if( m_eInitialState == PBI_InitState_Playing && m_ulPauseCount != 0 && !m_PBTrans.pvPRTrans )
		{
			m_eInitialState = PBI_InitState_Paused;
		}
	}
	return eResult;
}

CAkContinuousPBI::~CAkContinuousPBI()
{
	AKASSERT( m_pInstigator );
	m_pInstigator->Release();
}

void CAkContinuousPBI::Term( bool in_bFailedToInit )
{
	AKASSERT( g_pAudioMgr );

	DecrementPlayCount();

	PrepareNextToPlay( false );

	// g_pAudioMgr may not exist anymore on termination of the audiolib
	if( !HasNextToPlay() && g_pAudioMgr)
	{
		// It is possible that an action owns this PBI, should be removed from the list
		g_pAudioMgr->ClearCrossFadeOccurence(this);
	}
	if( HasNextToPlay() && !m_bTerminatedByStop )//nothing to do if no suite or if was killed
	{
		// WG-15809
		// XFades must not continue if the fade out to stop was requested.
        if( 
            !m_bWasPreStopped 
            || 
            (m_eTransitionMode != Transition_CrossFadeAmp && m_eTransitionMode != Transition_CrossFadePower)
            )
        {
		    CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef( m_ulNextElementToPlay, AkNodeType_Default );
		    if(pNode)
		    {
			    // create the action we need
			    CAkActionPlayAndContinue* pAction = CAkActionPlayAndContinue::Create( AkActionType_PlayAndContinue, 0, m_spContList );
			    if(pAction)
			    {
				    /*if(m_bIsContinuousPaused)
				    {
					    ++m_ulPauseCount;
					    m_bIsContinuousPaused = false;
				    }*/
				    pAction->SetPauseCount( m_ulPauseCount );
				    pAction->SetHistory( m_PlayHistoryForNextToPlay );

					WwiseObjectID wwiseId( pNode->ID() );
				    pAction->SetElementID( wwiseId );
                    pAction->SetInstigator( m_pInstigator );

					pAction->SetPlayDirectly( m_bPlayDirectly );
#ifndef AK_OPTIMIZED
					pAction->SetPlayTargetID( GetPlayTargetID() );
#endif

					MidiEventActionType eAction = GetSound()->GetMidiNoteOffAction();
					pAction->AssignMidi( eAction, m_pMidiNote, m_midiEvent );

					pAction->AssignModulator( GetModulatorData() );
					if( m_eTransitionMode == Transition_SampleAccurate )
						pAction->SetSAInfo(GetSequenceID()); //This is in case of a break on a sample accurate sound, the second one in line will skip the playback and should try to stitch the next one on termination.

				    AkPendingAction* pPendingAction = AkNew(AkMemID_Object, AkPendingAction( GetGameObjectPtr() ) );
				    if( pPendingAction )
				    {

					    // copy the transitions we have
					    if(
						    pAction->SetPlayStopTransition( m_PBTrans.pvPSTrans, pPendingAction ) == AK_Success
						    &&
						    pAction->SetPauseResumeTransition( m_PBTrans.pvPRTrans, pPendingAction ) == AK_Success
						    )
					    {
							AKRESULT eResult = AK_Success;

						    pAction->SetPathInfo(GetPathInfo());
						    if( m_bPlayFailed || in_bFailedToInit )
						    {
							    //WG-2352: avoid freeze on loop
							    //WG-4724: Delay must be exactly the size of a
							    //         buffer to avoid sample accurate glitches
							    //         and Buffer inconsistencies

							    pAction->SetIsFirstPlay( m_bIsFirstPlay );
    							
							    AkInt32 iDelay = AkTimeConv::MillisecondsToSamples( m_fTransitionTime );
								AkUInt32 uMinWaitSamplesAfterPlayFailed = ( AK_WAIT_BUFFERS_AFTER_PLAY_FAILED + g_settings.uContinuousPlaybackLookAhead ) * AK_NUM_VOICE_REFILL_FRAMES;
							    if( (m_eTransitionMode != Transition_Delay )
									|| iDelay < (AkInt32)uMinWaitSamplesAfterPlayFailed )
								{
								    iDelay = uMinWaitSamplesAfterPlayFailed;
								}

							    eResult = pAction->SetAkProp( AkPropID_DelayTime, iDelay, 0, 0 );
						    }
						    else if(m_eTransitionMode == Transition_Delay)
						    {
							    eResult = pAction->SetAkProp( AkPropID_DelayTime, AkTimeConv::MillisecondsToSamples( m_fTransitionTime ), 0, 0 );
						    }

							if ( eResult == AK_Success ) 
							{
								pPendingAction->pAction = pAction;
								pPendingAction->UserParam = m_UserParams;

								g_pAudioMgr->EnqueueOrExecuteAction( pPendingAction );
								if( m_fTransitionTime && m_ulPauseCount && !m_PBTrans.pvPRTrans )
								{
									g_pAudioMgr->PausePending( pPendingAction );
								}
							}
							else
							{
							    AkDelete(AkMemID_Object, pPendingAction );
							}
					    }
					    else
					    {
						    AkDelete(AkMemID_Object, pPendingAction );
					    }
				    }
				    // we are done with these
				    // Must not term m_pContinuationList here, releasing pAction will
				    pAction->Release();
			    }

			    if( m_bNeedNotifyEndReached )
			    {
				    m_bIsNotifyEndReachedContinuous = true;
			    }

			    pNode->Release();
		    }
        }
	}

	m_spContList = NULL;

	if( m_bNeedNotifyEndReached && m_bIsNotifyEndReachedContinuous )
	{
		Monitor(AkMonitorData::NotificationReason_EndReachedAndContinue);
		m_bNeedNotifyEndReached = false;
	}

	CAkPBI::Term( in_bFailedToInit );
}

//Seeking
// Disabled with transitions that involve playing more than one PBI at a time.
void CAkContinuousPBI::SeekTimeAbsolute( AkTimeMs in_iPosition, bool in_bSnapToMarker )
{
	if ( CanSeek() )
	{
		CAkPBI::SeekTimeAbsolute( in_iPosition, in_bSnapToMarker );
	}
	else
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_CannotSeekContinuous, this );
	}
}
void CAkContinuousPBI::SeekPercent( AkReal32 in_fPercent, bool in_bSnapToMarker )
{
	if ( CanSeek() )
	{
		CAkPBI::SeekPercent( in_fPercent, in_bSnapToMarker );
	}
	else
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_CannotSeekContinuous, this );
	}
}

bool CAkContinuousPBI::CanSeek()
{
	// Check if there is an invalid transition type in any continuous container
	// type in the hierarchy.
	bool bAllowFirstOnly = false;
	CAkParameterNodeBase * pNode = m_pParamNode->Parent();
	while ( pNode )
	{
		if ( pNode->NodeCategory() == AkNodeCategory_RanSeqCntr )
		{
			CAkRanSeqCntr * pCntr = static_cast<CAkRanSeqCntr*>( pNode );
			if ( pCntr->TransitionMode() == Transition_CrossFadeAmp
				|| pCntr->TransitionMode() == Transition_CrossFadePower	
				|| pCntr->TransitionMode() == Transition_TriggerRate )
			{
				// Cannot seek if one of the parents has such a transition mode.
				return false;
			}
			else if ( pCntr->TransitionMode() == Transition_SampleAccurate )
				bAllowFirstOnly = true;
		}
		pNode = pNode->Parent();
	}

	// No invalid transition mode in the hierarchy. However, there might be a restriction
	// to seek only if this is the first sound.
	return ( !bAllowFirstOnly || m_bIsFirstPlay );
}

void CAkContinuousPBI::PrepareNextPlayHistory( PlayHistory& in_rPlayHistory )
{
	// Create the History of the next sound to play
	
	m_PlayHistoryForNextToPlay = in_rPlayHistory;
	AkUInt32& ulrCount = m_PlayHistoryForNextToPlay.HistArray.uiArraySize;

	while( ulrCount )
	{
		if( m_PlayHistoryForNextToPlay.IsContinuous( ulrCount -1 ) )
		{
			break;
		}
		else
		{
			--ulrCount;
		}
	}
}

void CAkContinuousPBI::PrepareNextToPlay( bool in_bIsPreliminary )
{
	if( !m_bIsNextPrepared && m_spContList )
	{
		while( !m_spContList->m_listItems.IsEmpty() )
		{
			CAkContinueListItem& item = m_spContList->m_listItems.Last();
			if( !( item.m_pMultiPlayNode ) )
			{
				AkUniqueID uSelectedNodeID_UNUSED;
				AkUInt16 wPositionSelected;
				CAkParameterNodeBase * pNode = item.m_pContainer->GetNextToPlayContinuous( GetGameObjectPtr(), wPositionSelected, uSelectedNodeID_UNUSED, item.m_pContainerInfo, item.m_LoopingInfo );
				if(pNode)
				{
					m_PlayHistoryForNextToPlay.HistArray.aCntrHist[m_PlayHistoryForNextToPlay.HistArray.uiArraySize - 1] = wPositionSelected;
					m_ulNextElementToPlay = pNode->ID();
					m_eTransitionMode = item.m_pContainer->TransitionMode();
					if( m_eTransitionMode == Transition_CrossFadeAmp
						|| m_eTransitionMode == Transition_CrossFadePower
						|| m_eTransitionMode == Transition_Delay )
					{
						m_fTransitionTime = item.m_pContainer->TransitionTime( AkRTPCKey( GetGameObjectPtr(), GetPlayingID() ) );
					}
					else
					{
						m_fTransitionTime = 0.0f;
					}

					// Exit used if next was found
					m_bIsNextPrepared = true;
					pNode->Release();
					return;
				}
				else
				{
					m_PlayHistoryForNextToPlay.RemoveLast();

					while(m_PlayHistoryForNextToPlay.HistArray.uiArraySize
						&& !m_PlayHistoryForNextToPlay.IsContinuous( m_PlayHistoryForNextToPlay.HistArray.uiArraySize-1 ) )
					{
						m_PlayHistoryForNextToPlay.RemoveLast();
					}
					m_spContList->m_listItems.RemoveLast();
				}
			}
			else // Encountered a switch block
			{
				if( in_bIsPreliminary )
				{
					return;
				}
				CAkSmartPtr<CAkContinuationList> l_spContList;
				item.m_pMultiPlayNode->ContGetList( item.m_pAlternateContList, l_spContList );
				m_spContList->m_listItems.RemoveLast();
				if( l_spContList )
				{
					// I am the chosen one, flush the old ContList and use the new one

					// Lets take the new list and lets continue playing!!!
					m_spContList = l_spContList;
				}
				else
				{
					// We are not the continuous one, so terminate normally with no next
					m_spContList = NULL;
					return;
				}
			}
		}

		m_spContList = NULL;
	}

	m_bIsNextPrepared = true;	
	// Exit used if next was not found
}

void CAkContinuousPBI::SetEstimatedLength( AkReal32 in_fEstimatedLength )
{
	if( m_bWasStopped || m_bWasPreStopped )
	{
		return;
	}

	PrepareNextToPlay( true );

	if (m_eTransitionMode != Transition_CrossFadeAmp
		&& m_eTransitionMode != Transition_CrossFadePower)
		return;

	if( in_fEstimatedLength == 0 )
	{
		MONITOR_ERRORMSG_PLAYINGID(AKTEXT("Cross-Fade transition ignored: Estimated length not available. Make sure sounds with Cross-Fade transitions virtual mode are set to \"Play from elapsed time\" and that only sounds with finite duration are used."), GetPlayingID() );
	}
	if (in_fEstimatedLength < MINIMAL_LENGTH_FOR_CROSSFADE || !HasNextToPlay() )
	{
		return;
	}

	// create the action we need
	CAkActionPlayAndContinue* pAction = CAkActionPlayAndContinue::Create( AkActionType_PlayAndContinue, 0, m_spContList );
	if(pAction)
	{
		AkPendingAction* pPendingAction = AkNew(AkMemID_Object, AkPendingAction( GetGameObjectPtr() ) );
		if( pPendingAction )
		{
			pAction->SetPauseCount( m_ulPauseCount );
			pAction->SetHistory(m_PlayHistoryForNextToPlay);
			WwiseObjectID wwiseId( m_ulNextElementToPlay );
			pAction->SetElementID( wwiseId );
            pAction->SetInstigator( m_pInstigator );
			pAction->SetPlayDirectly( m_bPlayDirectly );
#ifndef AK_OPTIMIZED
			pAction->SetPlayTargetID( GetPlayTargetID() );
#endif

			MidiEventActionType eAction = GetSound()->GetMidiNoteOffAction();
			pAction->AssignMidi( eAction, m_pMidiNote, m_midiEvent );

			pAction->AssignModulator( GetModulatorData() );
			
			// copy the transitions we have
			// only pause/resume, since play/Stop will be used for the cross fade
			pAction->SetPauseResumeTransition( m_PBTrans.pvPRTrans, pPendingAction );
			pAction->SetPathInfo(GetPathInfo());

			//Max transition time equals half of the length, avoiding having multiple instances all at once
			AkReal32 fTransitionTime = AkMath::Min( m_fTransitionTime, in_fEstimatedLength / 2.0f );

			AkInt32 iDelaySamples = AkTimeConv::MillisecondsToSamples(in_fEstimatedLength - fTransitionTime);

			pAction->SetFadeBack( this, (AkTimeMs) fTransitionTime );

			if ( pAction->SetAkProp( AkPropID_DelayTime, iDelaySamples, 0, 0 ) == AK_Success )
			{
				pPendingAction->pAction = pAction;
				pPendingAction->UserParam = m_UserParams;

				g_pAudioMgr->EnqueueOrExecuteAction( pPendingAction );
				if( (iDelaySamples >= AK_NUM_VOICE_REFILL_FRAMES) && m_ulPauseCount )
				{
					g_pAudioMgr->PausePending( pPendingAction );
				}
			}
			else
			{
				AkDelete(AkMemID_Object, pPendingAction );
			}
		}
		// we are done with these
		pAction->Release();
		m_bIsNotifyEndReachedContinuous = true;
	}

	m_spContList = NULL;

	m_ulNextElementToPlay = AK_INVALID_UNIQUE_ID;
}

void CAkContinuousPBI::PrepareSampleAccurateTransition()
{
	if( m_bWasStopped )
	{
		return;
	}

	PrepareNextToPlay( true );

	if( (m_eTransitionMode != Transition_SampleAccurate) || !HasNextToPlay() )
	{
		return;
	}

	// create the action we need
	CAkActionPlayAndContinue* pAction = CAkActionPlayAndContinue::Create( AkActionType_PlayAndContinue, 0, m_spContList );
	if(pAction)
	{
		AkPendingAction* pPendingAction = AkNew(AkMemID_Object, AkPendingAction( GetGameObjectPtr() ) );
		if( pPendingAction )
		{
            pAction->SetPauseCount( m_ulPauseCount );
            pAction->SetHistory( m_PlayHistoryForNextToPlay );
			WwiseObjectID wwiseId( m_ulNextElementToPlay );
            pAction->SetElementID( wwiseId );
            pAction->SetInstigator( m_pInstigator );
			pAction->SetPlayDirectly( m_bPlayDirectly );
#ifndef AK_OPTIMIZED
			pAction->SetPlayTargetID( GetPlayTargetID() );
#endif

			MidiEventActionType eAction = GetSound()->GetMidiNoteOffAction();
			pAction->AssignMidi( eAction, m_pMidiNote, m_midiEvent );

			pAction->AssignModulator( GetModulatorData() );

			// copy the transitions we have
			if(
				pAction->SetPlayStopTransition( m_PBTrans.pvPSTrans, pPendingAction ) == AK_Success
				&&
				pAction->SetPauseResumeTransition( m_PBTrans.pvPRTrans, pPendingAction ) == AK_Success
				)
			{
				pAction->SetPathInfo(GetPathInfo());

				if( m_bWasPaused )
				{
					// here set the next to start as paused
					pAction->StartAsPaused();
				}

				pAction->SetSAInfo( GetSequenceID() );

				pPendingAction->pAction = pAction;
				pPendingAction->UserParam = m_UserParams;

				g_pAudioMgr->EnqueueOrExecuteAction(pPendingAction);
			}
			else
			{
				AkDelete(AkMemID_Object, pPendingAction );
			}
		}

		// we are done with these
		pAction->Release();
		m_bIsNotifyEndReachedContinuous = true;
	}

	m_spContList = NULL;

	m_ulNextElementToPlay = AK_INVALID_UNIQUE_ID;
}

void CAkContinuousPBI::_Stop(AkPBIStopMode in_eStopMode /*= AkPBIStopMode_Normal*/, bool in_bHasNotStarted /*= false*/)
{
	m_ulNextElementToPlay = AK_INVALID_UNIQUE_ID;
	m_bIsNextPrepared = true;

	m_spContList = NULL;

	CAkPBI::_Stop( in_eStopMode, in_bHasNotStarted );
}

void CAkContinuousPBI::PlayToEnd( CAkParameterNodeBase * in_pNode )
{
	// 1 - Tell to not launch next
	// probably includes clearing information for next(maybe deleting it)
	// and setting next to none

	//Don't change future playback if it is targetting a node that should not be modified by this playToEnd command

	CAkParameterNodeBase * pNextToPlay = NULL;
	if ( HasNextToPlay() )
		pNextToPlay = g_pIndex->GetNodePtrAndAddRef( m_ulNextElementToPlay, AkNodeType_Default );

	if(		!m_bIsNextPrepared 
		||	!HasNextToPlay() 
		||	(static_cast<CAkParameterNode*>( pNextToPlay )->IsOrIsChildOf( in_pNode ) )
		)
	{
		m_ulNextElementToPlay = AK_INVALID_UNIQUE_ID;
		m_bIsNextPrepared = false;

		if( m_spContList )
		{
			while( !m_spContList->m_listItems.IsEmpty() )
			{
				CAkContinueListItem& item = m_spContList->m_listItems.Last();
				if( !( item.m_pMultiPlayNode ) )
				{
					if( item.m_pContainer->IsOrIsChildOf( in_pNode ) )
					{
						m_PlayHistoryForNextToPlay.RemoveLast();
						while(m_PlayHistoryForNextToPlay.HistArray.uiArraySize
						&& !m_PlayHistoryForNextToPlay.IsContinuous( m_PlayHistoryForNextToPlay.HistArray.uiArraySize-1 ) )
						{
							m_PlayHistoryForNextToPlay.RemoveLast();
						}
						m_spContList->m_listItems.RemoveLast();
					}
					else
					{
						break;
					}
				}
				else // Encountered a switch block
				{
					CAkSmartPtr<CAkContinuationList> l_spContList;
					item.m_pMultiPlayNode->ContGetList( item.m_pAlternateContList, l_spContList );
					m_spContList->m_listItems.RemoveLast();
					if( l_spContList )
					{
						// I am the chosen one, flush the old ContList and use the new one

						// Lets take the new list and lets continue playing!!!
						m_spContList = l_spContList;
					}
				}
			}
			if( m_spContList->m_listItems.Length() == 0 )
			{
				m_spContList = NULL;
			}
		}
	}

	if ( pNextToPlay )
		pNextToPlay->Release();

	// 2 - Then call the native Play to end, which will shorten the duration of the sound if possible
	// (will set loop count to last loop if possible)
	
	CAkPBI::PlayToEnd( in_pNode );
}

void CAkContinuousPBI::SetPauseStateForContinuous(bool in_bIsPaused)
{
	m_bIsContinuousPaused = in_bIsPaused;
}

#ifndef AK_OPTIMIZED

void CAkContinuousPBI::Monitor(AkMonitorData::NotificationReason in_Reason, bool in_bUpdateCount )
{
	if(in_Reason == AkMonitorData::NotificationReason_Play)
	{
		if(!m_bIsFirstPlay)
		{
			in_Reason = AkMonitorData::NotificationReason_PlayContinue;
		}
	}

	CAkPBI::Monitor(in_Reason, in_bUpdateCount);
}

void CAkContinuousPBI::_StopAndContinue(
		AkUniqueID			in_ItemToPlay,
		AkUInt16				in_PlaylistPosition,
		CAkContinueListItem* in_pContinueListItem
		)
{
	AKASSERT(g_pIndex);
	if( m_spContList )
	{
		AKASSERT( !m_spContList->m_listItems.IsEmpty() );
		CAkContinueListItem & itemFirst = *(m_spContList->m_listItems.Begin());

		in_pContinueListItem->m_LoopingInfo = itemFirst.m_LoopingInfo;

		if( in_pContinueListItem->m_LoopingInfo.bIsEnabled )
		{
			if( itemFirst.m_pContainerInfo )
			{
				CAkSequenceInfo* pSeqInfo = static_cast<CAkSequenceInfo*>( itemFirst.m_pContainerInfo );
				if( pSeqInfo->m_i16LastPositionChosen == 0 && pSeqInfo->m_bIsForward )
				{
					in_pContinueListItem->m_LoopingInfo.lLoopCount += 1;
				}
			}
		}
		m_spContList = NULL;
	}
	/*else
	{
		in_pContinueListItem->m_LoopingInfo.bIsEnabled = false;
		in_pContinueListItem->m_LoopingInfo.bIsInfinite = false;
		in_pContinueListItem->m_LoopingInfo.lLoopCount = 1;
	}*/
	m_spContList.Attach( CAkContinuationList::Create() );
	if ( m_spContList )
	{
		if( m_spContList->m_listItems.AddLast(*in_pContinueListItem) == NULL )
		{
			m_spContList = NULL;
		}
	}
	
	m_PlayHistoryForNextToPlay.Init();
	m_PlayHistoryForNextToPlay.Add( in_PlaylistPosition, true);
	m_ulNextElementToPlay = in_ItemToPlay;

	m_bIsNextPrepared = true;

	//We don't want it to crossfade nor delay, we want it to change right away
	m_eTransitionMode = Transition_Disabled;

	CAkPBI::_Stop( AkPBIStopMode_StopAndContinue, false );
}

#endif
