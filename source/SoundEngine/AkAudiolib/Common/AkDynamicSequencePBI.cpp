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
// AkDynamicSequencePBI.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkDynamicSequencePBI.h"
#include "AkDynamicSequence.h"
#include "AkPlayingMgr.h"
#include "AkLEngineCmds.h"                            // for CAkLEngineCmds
#include "AkParameterNode.h"                          // for CAkParameterNode

// Constructor
CAkDynamicSequencePBI::CAkDynamicSequencePBI(
											AkPBIParams&				in_params,
											CAkSoundBase*				in_pSound,			// Sound associated to the PBI (NULL if none).	
											CAkSource*					in_pSource,
											const PriorityInfoCurrent&	in_rPriority,
											AK::SoundEngine::DynamicSequence::DynamicSequenceType in_eDynamicSequenceType
											)
: CAkContinuousPBI( in_params
				   ,in_pSound
				   ,in_pSource
				   ,in_rPriority
				   )
, m_bRequestNextFromDynSeq( true )
, m_eDynamicSequenceType( in_eDynamicSequenceType )
{
	CAkDynamicSequence* pDynamicSequence = static_cast<CAkDynamicSequence*>( m_pInstigator );
	m_startingNode = pDynamicSequence->GetQueuedItem().audioNodeID;
	m_pDynSecCustomInfo = pDynamicSequence->GetQueuedItem().pCustomInfo;
}


CAkDynamicSequencePBI::~CAkDynamicSequencePBI( void )
{
}

void CAkDynamicSequencePBI::Term( bool in_bFailedToInit )
{
	if( !in_bFailedToInit )
	{
		// If the PBI failed to init, we must not notify there, the caller will take care of the notification in case of error on playback.
		g_pPlayingMgr->NotifyEndOfDynamicSequenceItem( GetPlayingID(), m_startingNode, m_pDynSecCustomInfo );
	}

	CAkContinuousPBI::Term( in_bFailedToInit );
}

void CAkDynamicSequencePBI::PrepareNextToPlay( bool in_bIsPreliminary )
{
	CAkContinuousPBI::PrepareNextToPlay( in_bIsPreliminary );

	if ( m_bIsNextPrepared && !m_bWasStopped )
	{
		if ( HasNextToPlay() )
		{
			m_bRequestNextFromDynSeq = false;
		}
		else if ( m_bRequestNextFromDynSeq  )
		{
			if( !in_bIsPreliminary || m_eDynamicSequenceType == AK::SoundEngine::DynamicSequence::DynamicSequenceType_SampleAccurate )
			{
				m_bRequestNextFromDynSeq = false;
				CAkDynamicSequence* pDynamicSequence = static_cast<CAkDynamicSequence*>( m_pInstigator );
				
				while( 1 )
				{
					void* pCustomInfo = NULL;
					AkTimeMs delay = 0;
					AkUniqueID nextElement = pDynamicSequence->GetNextToPlay( delay, pCustomInfo, m_UserParams );

					if ( nextElement == AK_INVALID_UNIQUE_ID )
						break; // nothing next, we're out.

					// We Have something new to play next.
					if( PlayNextElement( nextElement, delay ) == AK_Success )
					{
						// successful, we can leave
						break;
					}

					// Failed to play the next in line, try the one after it.
					// but we still have to notify for the user get the skipped item callback.
					g_pPlayingMgr->NotifyEndOfDynamicSequenceItem( GetPlayingID(), nextElement, pCustomInfo );
				}
			}
		}
	}
}

AKRESULT CAkDynamicSequencePBI::PlayNextElement( AkUniqueID in_nextElementID, AkTimeMs in_delay )
{
	CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef( in_nextElementID, AkNodeType_Default );
	if ( !pNode )
		return AK_Fail;
		
	ContParams	Cparameters( m_PBTrans.pvPSTrans, m_PBTrans.pvPRTrans, GetPathInfo(), m_ulPauseCount );
	
	Cparameters.spContList.Attach( CAkContinuationList::Create() );

	TransParams	Tparameters;

	Tparameters.TransitionTime = 0;
	Tparameters.eFadeCurve = AkCurveInterpolation_Constant;

	AkPBIParams pbiParams;
    
	pbiParams.eType = AkPBIParams::DynamicSequencePBI;
    pbiParams.pInstigator = m_pInstigator;
	pbiParams.userParams = m_UserParams;
	pbiParams.ePlaybackState = PB_Playing;
	pbiParams.uFrameOffset = AkTimeConv::MillisecondsToSamples( in_delay );
    pbiParams.bIsFirst = false;

	pbiParams.pGameObj = GetGameObjectPtr();
	pbiParams.playTargetID = in_nextElementID;

	pbiParams.pTransitionParameters = &Tparameters;
    pbiParams.pContinuousParams = &Cparameters;
    pbiParams.pMidiNoteState = m_pMidiNote;
	pbiParams.sequenceID = GetSequenceID();
	
	AKRESULT eResult = static_cast<CAkParameterNode*>(pNode)->Play( pbiParams );

	if( m_bNeedNotifyEndReached )
	{
		m_bIsNotifyEndReachedContinuous = true;
	}

	pNode->Release();

	CAkLEngineCmds::IncrementSyncCount();//We must increment it there, to ensure it will be processed.

	return eResult;
}
