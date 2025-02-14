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
// AkDynamicSequence.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include "AkAudioMgr.h"
#include "AkPlayingMgr.h"
#include "AkDynamicSequence.h"
#include "AkAudioLibIndex.h"
#include "AkDynamicSequencePBI.h"
#include "AkParameterNode.h"
#include "AkQueuedMsg.h"

CAkDynamicSequence::CAkDynamicSequence( AkPlayingID in_playingID, AK::SoundEngine::DynamicSequence::DynamicSequenceType in_eDynamicSequenceType )
	: CAkPBIAware( in_playingID ) // ID of dynamic sequence is its playing ID
	, m_eState( State_Stopped )
	, m_pGameObj( NULL )
	, m_bClosed( false )
	, m_eDynamicSequenceType( in_eDynamicSequenceType )
	, m_ulPauseCount(0)
	, m_ulPauseTime(0)
	, m_ulTicksPaused(0)
	, m_uSequenceID( AK_INVALID_SEQUENCE_ID )
	
{
	m_playingItem.audioNodeID = AK_INVALID_UNIQUE_ID;
	m_playingItem.pCustomInfo = NULL;

	m_queuedItem.audioNodeID = AK_INVALID_UNIQUE_ID;
	m_queuedItem.pCustomInfo = NULL;

	m_userParams.SetPlayingID(in_playingID);
}

CAkDynamicSequence::~CAkDynamicSequence()
{
	m_playList.Term();
	if ( m_pGameObj )
		m_pGameObj->Release();
}

CAkDynamicSequence* CAkDynamicSequence::Create( AkPlayingID in_PlayingID, AK::SoundEngine::DynamicSequence::DynamicSequenceType in_eDynamicSequenceType )
{
	CAkDynamicSequence* pAkDynamicSequence = AkNew(AkMemID_Object, CAkDynamicSequence( in_PlayingID, in_eDynamicSequenceType ) );

	if( pAkDynamicSequence && pAkDynamicSequence->Init() != AK_Success )
	{
		pAkDynamicSequence->Release();
		pAkDynamicSequence = NULL;
	}

	return pAkDynamicSequence;
}

AKRESULT CAkDynamicSequence::Init()
{
	AddToIndex();
	return AK_Success;
}

void CAkDynamicSequence::AddToIndex()
{
	AKASSERT( g_pIndex );
	g_pIndex->m_idxDynamicSequences.SetIDToPtr(this);
}

void CAkDynamicSequence::RemoveFromIndex()
{
	AKASSERT(g_pIndex);
	g_pIndex->m_idxDynamicSequences.RemoveID(ID());
}

AkUInt32 CAkDynamicSequence::AddRef() 
{ 
	AkAutoLock<CAkLock> IndexLock( g_pIndex->m_idxDynamicSequences.GetLock() ); 
    return ++m_lRef; 
} 

AkUInt32 CAkDynamicSequence::Release() 
{ 
    AkAutoLock<CAkLock> IndexLock( g_pIndex->m_idxDynamicSequences.GetLock() ); 
    AkInt32 lRef = --m_lRef; 
    AKASSERT( lRef >= 0 ); 
    if ( !lRef ) 
    {
		RemoveFromIndex();
		AkDelete(AkMemID_Object, this );
    } 
    return lRef; 
}

CAkPBI* CAkDynamicSequence::CreatePBI( CAkSoundBase* in_pSound, 
									   CAkSource* in_pSource, 
									   AkPBIParams& in_rPBIParams, 
									   const PriorityInfoCurrent& in_rPriority
									   ) const
{
	return AkNew( AkMemID_Object, CAkDynamicSequencePBI( 
		in_rPBIParams,
		in_pSound,
		in_pSource,
		in_rPriority,
		m_eDynamicSequenceType) );
}

void CAkDynamicSequence::SetGameObject( CAkRegisteredObj* in_pGameObj )
{
	AKASSERT( !m_pGameObj );
	m_pGameObj = in_pGameObj;
	if ( m_pGameObj )
		m_pGameObj->AddRef();
}

AkUniqueID CAkDynamicSequence::_GetNextToPlay( AkTimeMs & out_delay, void*& out_rpCustomParam )
{
	//Check if we've been properly initialized.
	if ( !m_pGameObj )
		return AK_INVALID_UNIQUE_ID;

	AkAutoLock<CAkLock> lockPlaylist( m_lockPlaylist );
	
	m_queuedItem.audioNodeID = AK_INVALID_UNIQUE_ID;
	m_queuedItem.pCustomInfo = NULL;
	
	if ( m_eState != State_Stopped )
	{
		while ( !m_playList.IsEmpty() )
		{
			const AK::SoundEngine::DynamicSequence::PlaylistItem & item = m_playList[0];

			out_rpCustomParam = item.pCustomInfo;
			if ( item.audioNodeID == AK_INVALID_UNIQUE_ID )
			{
				CAkCntrHist HistArray;
				MONITOR_OBJECTNOTIF( m_userParams.PlayingID(), m_pGameObj->ID(), m_userParams.CustomParam(), AkMonitorData::NotificationReason_PlayFailed, HistArray, AK_INVALID_UNIQUE_ID, false, 0 );
				MONITOR_ERROREX( AK::Monitor::ErrorCode_NothingToPlay, m_userParams.PlayingID(), m_pGameObj->ID(), AK_INVALID_UNIQUE_ID, false );
				m_playList.Erase( 0 );

				g_pPlayingMgr->NotifyEndOfDynamicSequenceItem( GetPlayingID(), AK_INVALID_UNIQUE_ID, out_rpCustomParam );
			}
			else
			{
				m_queuedItem = item;				

				out_delay = item.msDelay;
				m_playList.Erase( 0 );
				break;
			}
		}

		// Usually, this code would go right after the code that has called GetNextToPlay.
		// But to prevent race condition ( m_eState should not be set to State_Waiting outside the playlist lock),
		// We manage the Waiting state here.
		if ( m_queuedItem.audioNodeID == AK_INVALID_UNIQUE_ID )
			m_eState = State_Waiting;
	}

	return m_queuedItem.audioNodeID;
}

AkUniqueID CAkDynamicSequence::GetNextToPlay( AkTimeMs & out_delay, void*& out_rpCustomParam, UserParams& out_rUserParam )
{
	m_playingItem = m_queuedItem;
	AkUniqueID id = _GetNextToPlay( out_delay, out_rpCustomParam );	

	out_rUserParam.SetExternalSources(m_queuedItem.GetExternalSources());
	return id;
}

AKRESULT CAkDynamicSequence::Play( AkTimeMs in_uTransitionDuration, AkCurveInterpolation in_eFadeCurve )
{
	if ( ( m_eState == State_Stopped || m_eState == State_Waiting ) && m_ulPauseCount == 0 )
	{
		//m_playingItem.audioNodeID = AK_INVALID_UNIQUE_ID;

		m_eState = State_Playing;

		AKRESULT result = AK_Fail;

		// If the play of the next item failed, play the following one.
		// Repeat until there is an element to play or no more element to play in the list

		AkTimeMs delay = 0;

		do
		{
			void* pCustomInfo = NULL;;
			AkUniqueID nextToPlay = _GetNextToPlay( delay, pCustomInfo );
			m_userParams.SetExternalSources(m_queuedItem.GetExternalSources());

			result = nextToPlay == AK_INVALID_UNIQUE_ID ? AK_Success : _PlayNode( nextToPlay, delay, in_uTransitionDuration, in_eFadeCurve );
			if( result != AK_Success )
			{
				g_pPlayingMgr->NotifyEndOfDynamicSequenceItem( GetPlayingID(), nextToPlay, pCustomInfo );
			}

		} while( result != AK_Success );
	}

	return AK_Success;
}

AKRESULT CAkDynamicSequence::Stop( AkTimeMs in_uTransitionDuration, AkCurveInterpolation in_eFadeCurve )
{
	// Reset sequence ID to avoid trouble when play happens before minimum transition time.
	m_uSequenceID = AK_INVALID_SEQUENCE_ID;

	// Stop must proceed even if the state is Stopped, just in case the last time it has been stopped it was a break.
	// It will avoid sound being played after a stop all.
	return _Stop( AK_StopImmediate, in_uTransitionDuration, in_eFadeCurve );
}

AKRESULT CAkDynamicSequence::Break()
{
	// Reset sequence ID to avoid trouble when play happens before end of currently playing sound.
	m_uSequenceID = AK_INVALID_SEQUENCE_ID;

	if ( m_eState != State_Stopped )
		return _Stop( AK_StopAfterCurrentItem, 0, AkCurveInterpolation_Linear );

	return AK_Success;
}

AKRESULT CAkDynamicSequence::_Stop( AkDynamicSequenceStopMode in_eStopMode, AkTimeMs in_uTransitionDuration, AkCurveInterpolation in_eFadeCurve )
{
	m_eState = State_Stopped;
	m_ulPauseCount = 0;

	g_pAudioMgr->ClearPendingItems( m_userParams.PlayingID() );

	if ( m_queuedItem.audioNodeID != AK_INVALID_UNIQUE_ID && ( in_eStopMode == AK_StopImmediate ) )
	{
		CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef( m_queuedItem.audioNodeID, AkNodeType_Default );
		if ( pNode )
		{
			pNode->Stop( m_pGameObj, m_userParams.PlayingID(), in_uTransitionDuration, in_eFadeCurve );
			pNode->Release();
		}

		{
			AkAutoLock<CAkLock> lockPlaylist(m_lockPlaylist);
			m_queuedItem.audioNodeID = AK_INVALID_UNIQUE_ID;
			m_queuedItem.pCustomInfo = NULL;
		}
	}

	if ( m_playingItem.audioNodeID != AK_INVALID_UNIQUE_ID )
	{		
		CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef( m_playingItem.audioNodeID, AkNodeType_Default );
		if ( pNode )
		{
			if ( in_eStopMode == AK_StopImmediate )
			{
				pNode->Stop( m_pGameObj, m_userParams.PlayingID(), in_uTransitionDuration, in_eFadeCurve );
			}
			else
			{
				pNode->PlayToEnd( m_pGameObj, pNode, m_userParams.PlayingID() );
			}
			pNode->Release();	
		}
	}

	return AK_Success;
}

void CAkDynamicSequence::_StopNoPropagation()
{
	m_eState = State_Stopped;
	m_ulPauseCount = 0;
	{
		AkAutoLock<CAkLock> lockPlaylist(m_lockPlaylist);
		m_queuedItem.audioNodeID = AK_INVALID_UNIQUE_ID;
		m_queuedItem.pCustomInfo = NULL;
	}
}

AKRESULT CAkDynamicSequence::Pause( AkTimeMs in_uTransitionDuration, AkCurveInterpolation in_eFadeCurve )
{
	if ( m_eState == State_Stopped )
		return AK_Success;
		
	++m_ulPauseCount;

	if ( m_ulPauseCount == 1 )
	{
		m_ulPauseTime = g_pAudioMgr->GetBufferTick();

		g_pAudioMgr->PausePendingItems(m_userParams.PlayingID());

		if ( m_playingItem.audioNodeID != AK_INVALID_UNIQUE_ID )
		{		
			CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef( m_playingItem.audioNodeID, AkNodeType_Default );

			if ( pNode )
			{
				pNode->Pause( m_pGameObj, m_userParams.PlayingID(), in_uTransitionDuration, in_eFadeCurve );
				pNode->Release();
			}
		}

		if ( m_queuedItem.audioNodeID != AK_INVALID_UNIQUE_ID )
		{
			CAkParameterNodeBase* pNode  = g_pIndex->GetNodePtrAndAddRef( m_queuedItem.audioNodeID, AkNodeType_Default );

			if ( pNode )
			{
				pNode->Pause( m_pGameObj, m_userParams.PlayingID(), in_uTransitionDuration, in_eFadeCurve );
				pNode->Release();
			}
		}
	}

	return AK_Success;
}

void CAkDynamicSequence::_PauseNoPropagation()
{
	if (m_eState != State_Stopped)
	{
		++m_ulPauseCount;
		if (m_ulPauseCount == 1)
		{
			m_ulPauseTime = g_pAudioMgr->GetBufferTick();
		}
	}
}

AKRESULT CAkDynamicSequence::Resume( AkTimeMs in_uTransitionDuration, AkCurveInterpolation in_eFadeCurve )
{
	if ( m_ulPauseCount == 0 )
		return AK_Success;
		
	if ( !--m_ulPauseCount )
	{
		if ( m_eState != State_Playing )
		{
			Play( in_uTransitionDuration, in_eFadeCurve );
		}

		g_pAudioMgr->ResumePausedPendingItems( m_userParams.PlayingID() );

		if ( m_playingItem.audioNodeID != AK_INVALID_UNIQUE_ID )
		{		
			CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef( m_playingItem.audioNodeID, AkNodeType_Default );

			if ( pNode ) 
			{
				pNode->Resume( m_pGameObj, m_userParams.PlayingID(), in_uTransitionDuration, in_eFadeCurve );
				pNode->Release();
			}
		}

		if ( m_queuedItem.audioNodeID != AK_INVALID_UNIQUE_ID )
		{
			CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef( m_queuedItem.audioNodeID, AkNodeType_Default );

			if ( pNode )
			{
				pNode->Resume( m_pGameObj, m_userParams.PlayingID(), in_uTransitionDuration, in_eFadeCurve );
				pNode->Release();
			}
		}

		{
			AkAutoLock<CAkLock> lockPlaylist(m_lockPlaylist);
			m_ulTicksPaused += g_pAudioMgr->GetBufferTick() - m_ulPauseTime;
			m_ulPauseTime = 0;
		}
	}

	return AK_Success;
}

AKRESULT CAkDynamicSequence::Seek( const AkQueuedMsg_DynamicSequenceSeek& in_Seek )
{
	if( m_eState == State_Stopped )
		return AK_Success;

	if( !m_pGameObj )
		return AK_Fail;

	SeekActionParams l_Params;
	l_Params.bIsFromBus = false;
	l_Params.bIsMasterResume = false;
	l_Params.transParams.eFadeCurve = AkCurveInterpolation_Linear;
	l_Params.pGameObj = m_pGameObj;
	l_Params.playingID = m_userParams.PlayingID();
	l_Params.transParams.TransitionTime = 0;
	l_Params.bIsMasterCall = false;
	l_Params.bIsSeekRelativeToDuration = in_Seek.bIsSeekRelativeToDuration;
	l_Params.bSnapToNearestMarker = in_Seek.bSnapToNearestMarker;
	if (l_Params.bIsSeekRelativeToDuration)
		l_Params.fSeekPercent = in_Seek.fSeekPercent;
	else
		l_Params.iSeekTime = in_Seek.iSeekTime;
	l_Params.eType = ActionParamType_Seek;

	CAkParameterNodeBase* pNode = NULL;
	if (m_playingItem.audioNodeID != AK_INVALID_UNIQUE_ID)	
		 pNode = g_pIndex->GetNodePtrAndAddRef(m_playingItem.audioNodeID, AkNodeType_Default);		
	else if( m_queuedItem.audioNodeID != AK_INVALID_UNIQUE_ID )	
		pNode = g_pIndex->GetNodePtrAndAddRef(m_queuedItem.audioNodeID, AkNodeType_Default);			

	if (pNode)
	{
		pNode->ExecuteAction(l_Params);
		pNode->Release();
	}
	else
	{
		return AK_IDNotFound;
	}	

	return AK_Success;
}

void CAkDynamicSequence::GetQueuedItem(AkUniqueID & out_audioNodeID, void *& out_pCustomInfo)
{
	AkAutoLock<CAkLock> lockPlaylist(m_lockPlaylist);
	out_audioNodeID = m_queuedItem.audioNodeID;
	out_pCustomInfo = m_queuedItem.pCustomInfo;
}

void CAkDynamicSequence::GetPauseTimes(AkUInt32 &out_uTime, AkUInt32 &out_uDuration)
{
	AkAutoLock<CAkLock> lockPlaylist(m_lockPlaylist);
	out_uTime = m_ulPauseTime;
	out_uDuration = m_ulTicksPaused;
	m_ulTicksPaused = 0;
}

void CAkDynamicSequence::_ResumeNoPropagation()
{
	if ( m_ulPauseCount != 0 )
	{
		if (!--m_ulPauseCount)
		{
			if (m_eState != State_Playing)
				Play(0, AkCurveInterpolation_Linear);

			{
				AkAutoLock<CAkLock> lockPlaylist(m_lockPlaylist);
				m_ulTicksPaused += g_pAudioMgr->GetBufferTick() - m_ulPauseTime;
				m_ulPauseTime = 0;
			}
		}
	}
}

void CAkDynamicSequence::ResumeWaiting()
{
	if ( m_eState == State_Waiting && !m_playList.IsEmpty() )
	{
		Play( 0, AkCurveInterpolation_Linear );
	}
}

AKRESULT CAkDynamicSequence::_PlayNode( AkUniqueID in_nodeID, AkTimeMs in_delay, AkTimeMs in_uTransitionDuration, AkCurveInterpolation in_eFadeCurve )
{
	AKASSERT(g_pIndex);

	//Check if we've been properly initialized.
	if ( !m_pGameObj )
		return AK_Fail;

	AKRESULT eResult = AK_Success;

	CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef( in_nodeID, AkNodeType_Default );

	if( pNode )
	{
		// Transition Parameters
		TransParams	Tparameters;
		Tparameters.TransitionTime = in_uTransitionDuration;
		Tparameters.eFadeCurve = in_eFadeCurve;

		// Continuation List
		ContParams continuousParams;
		continuousParams.spContList.Attach( CAkContinuationList::Create() );
		if ( !continuousParams.spContList )
		{
			pNode->Release();
			return AK_Fail;
		}

		// PBI Params
		AkPBIParams pbiParams;
        
		pbiParams.eType = AkPBIParams::DynamicSequencePBI;
        pbiParams.pInstigator = this;
		pbiParams.userParams = m_userParams;
		pbiParams.ePlaybackState = PB_Playing;
		pbiParams.uFrameOffset = AkTimeConv::MillisecondsToSamples( in_delay );
        pbiParams.bIsFirst = true;

		pbiParams.pGameObj = m_pGameObj;
		pbiParams.playTargetID = in_nodeID;

		pbiParams.pTransitionParameters = &Tparameters;
        pbiParams.pContinuousParams = &continuousParams;

		// Only Acquire a dynamic Sequence ID in SA mode.
		if( m_eDynamicSequenceType == AK::SoundEngine::DynamicSequence::DynamicSequenceType_SampleAccurate )
		{
			// We must generate the sequence ID here, and not let the continuous PBI do it itself.
			// We cannot do it in the constructor as the constructor is in the game thread. (GetNewSequenceID is not thread safe)
			// so we do it here, the first time it is required.
			while( m_uSequenceID == AK_INVALID_SEQUENCE_ID ) // While and not If to pad the int32 overflow.
			{
				m_uSequenceID = CAkPBI::GetNewSequenceID();
			}
		}

		pbiParams.sequenceID = m_uSequenceID;

		eResult = static_cast<CAkParameterNode*>(pNode)->Play( pbiParams );

		pNode->Release();
	}
	else
	{
		CAkCntrHist HistArray;
		MONITOR_OBJECTNOTIF( m_userParams.PlayingID(), m_pGameObj->ID(), m_userParams.CustomParam(), AkMonitorData::NotificationReason_PlayFailed, HistArray, in_nodeID, false, 0 );
		MONITOR_ERROR_PARAM( AK::Monitor::ErrorCode_SelectedNodeNotAvailable, in_nodeID, m_userParams.PlayingID(), m_pGameObj->ID(), ID(), false );
		eResult = AK_IDNotFound;
	}

	return eResult;
}

void CAkDynamicSequence::UnlockPlaylist()
{
	bool bIsWaiting = m_eState == State_Waiting;

	m_lockPlaylist.Unlock();

	if ( bIsWaiting )
	{
		// Queue a message for resuming waiting
		AkQueuedMsg *pItem = g_pAudioMgr->ReserveQueue(QueuedMsgType_DynamicSequenceCmd, AkQueuedMsg::Sizeof_DynamicSequenceCmd() );
		AddRef();
		pItem->dynamicsequencecmd.pDynamicSequence = this;
		pItem->dynamicsequencecmd.eCommand = AkQueuedMsg_DynamicSequenceCmd::ResumeWaiting;
		g_pAudioMgr->FinishQueueWrite();
	}
}

void CAkDynamicSequence::AllExec( ActionParamType in_eType, CAkRegisteredObj * in_pGameObj )
{
	// In this function, we simply change the state of the DynamicSequence.
	// We do not have to notify the playing nodes, as they will be notified by the normal process of the incoming event.
	// Making it this way will allow keeping transition time specified in the events/actions as well as maintaining the state of the Dynamic sequence.
	if( m_pGameObj == in_pGameObj || in_pGameObj == NULL )
	{
		switch( in_eType )
		{
		case ActionParamType_Stop:		
			_StopNoPropagation();
			break;

		case ActionParamType_Pause:
			_PauseNoPropagation();
			break;

		case ActionParamType_Resume:	
			_ResumeNoPropagation();
			break;

		default:
			break;
		}
	}
}

AK::SoundEngine::DynamicSequence::PlaylistItem::PlaylistItem()
{
	audioNodeID = 0;
	msDelay = 0;
	pCustomInfo = NULL;
	pExternalSrcs = NULL;
}

AK::SoundEngine::DynamicSequence::PlaylistItem::PlaylistItem( const PlaylistItem& in_rCopy )
{
	pExternalSrcs = NULL;
	*this = in_rCopy;
}

AK::SoundEngine::DynamicSequence::PlaylistItem::~PlaylistItem()
{
	if (pExternalSrcs)
		pExternalSrcs->Release();
}

AKRESULT AK::SoundEngine::DynamicSequence::PlaylistItem::SetExternalSources( AkUInt32 in_nExternalSrc, AkExternalSourceInfo* in_pExternalSrc )
{
	if (pExternalSrcs)
		pExternalSrcs->Release();

	pExternalSrcs = NULL;
	if (in_nExternalSrc > 0)
		pExternalSrcs = AkExternalSourceArray::Create(in_nExternalSrc, in_pExternalSrc);
	else
		return AK_Success;
		
	return pExternalSrcs == NULL ? AK_InsufficientMemory : AK_Success;
}

AK::SoundEngine::DynamicSequence::PlaylistItem& AK::SoundEngine::DynamicSequence::PlaylistItem::operator=( const PlaylistItem& in_rCopy )
{
	if (pExternalSrcs)
		pExternalSrcs->Release();

	audioNodeID = in_rCopy.audioNodeID;
	msDelay = 	in_rCopy.msDelay;
	pCustomInfo = in_rCopy.pCustomInfo;
	pExternalSrcs = in_rCopy.pExternalSrcs;

	if (pExternalSrcs)
		pExternalSrcs->AddRef();

	return *this;
}
