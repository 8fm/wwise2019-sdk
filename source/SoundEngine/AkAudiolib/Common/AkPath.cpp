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
// AkPath.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkPath.h"
#include "AkTransitionManager.h"
#include "Ak3DListener.h"
#include "AkBehavioralCtx.h"             // for CAkBehavioralCtx
#include "AkRandom.h"

//====================================================================================================
//====================================================================================================
CAkPath::CAkPath()
{
	m_eState = Idle;				// not active
	m_pPathsList = NULL;			// no play list
	m_pCurrentList = NULL;			// not playing any
	m_ulListSize = 0;				// empty list
	m_PathMode = AkStepSequence;	// default
	m_bWasStarted = false;			// initially not started
	m_bIsLooping = false;
	m_uPotentialUsers = 0;
	m_uNumUsers = 0;
	m_StartTime = 0;
	m_EndTime = 0;
	m_Duration = 0;
	m_TimePaused = 0;
	m_ulSoundUniqueID = AK_INVALID_UNIQUE_ID;
	m_playingID = AK_INVALID_PLAYING_ID;
	m_LastPositionNotified.Zero();
}
//====================================================================================================
//====================================================================================================
CAkPath::~CAkPath()
{
}

//====================================================================================================
//====================================================================================================
void CAkPath::Term()
{
	AKASSERT(g_pTransitionManager);

	m_eState = Idle;
	m_PBIsList.Term();

	// if continuous then the state was not saved and the sound won't free the played flags
	if(m_PathMode & AkPathContinuous)
	{
		// get rid of the path played flags if any
		m_pbPlayed = NULL;
	}

	m_NoFollowOrientationRotationMatrices.Term();	
}
//====================================================================================================
// start moving along our path
//====================================================================================================
AKRESULT CAkPath::Start( AkUInt32 in_CurrentBufferTick )
{
	AKASSERT(g_pTransitionManager);
	AKRESULT	eResult;

	// assume no list
	eResult = AK_Fail;

	// have we got a play list ?
	if(m_pCurrentList != NULL)
	{
		// assume none
		eResult = AK_PathNoVertices;
		m_bWasStarted = true;

		// any vertex ? // copy these vertices
		if( m_pCurrentList->iNumVertices > 0 )
        {
			m_uCurrentVertex = 0;

			eResult = AK_Success;

			// get the starting vertex
			AkPathVertex & From = m_pCurrentList->pVertices[ m_uCurrentVertex++ ];

			// set start position
			m_StartPosition.X = From.Vertex.X;
			m_StartPosition.Y = From.Vertex.Y;
			m_StartPosition.Z = From.Vertex.Z;

			// taking that much time
			m_Duration = CAkPath::Convert( From.Duration );

			// any destination vertex ?
			if( m_uCurrentVertex < m_pCurrentList->iNumVertices )
			{
				// get the destination vertex
				AkVector To = m_pCurrentList->pVertices[ m_uCurrentVertex ].Vertex;

				if (m_StartPosition.X != To.X ||  
					m_StartPosition.Y != To.Y ||
					m_StartPosition.Z != To.Z ||
					m_pCurrentList->iNumVertices > 2)
				{
					RandomizePosition(m_StartPosition);
					RandomizePosition(To);
				}
				else
				{
					//If 2 points are at the same place, randomize only once and copy to avoid movement.
					RandomizePosition(m_StartPosition);
					To.X = m_StartPosition.X;
					To.Y = m_StartPosition.Y;
					To.Z = m_StartPosition.Z;
				}

				// compute direction
				m_Direction.X = To.X - m_StartPosition.X;
				m_Direction.Y = To.Y - m_StartPosition.Y;
				m_Direction.Z = To.Z - m_StartPosition.Z;
			}
			else
			{
				// we're not moving
				m_Direction.X = 0.0f;
				m_Direction.Y = 0.0f;
				m_Direction.Z = 0.0f;
			}

			// send the start position to the users
			UpdateStartPosition();

			// start moving
			m_StartTime = in_CurrentBufferTick;
			m_EndTime = in_CurrentBufferTick + m_Duration;
			m_fa = 1.0f / m_Duration;
			m_fb = -( m_StartTime * m_fa );
			// update state
			m_eState = Running;

			// PhM : insert path started notification here
			MONITOR_PATH_EVENT(m_playingID,m_ulSoundUniqueID,AkMonitorData::AkPathEvent_ListStarted,m_ulCurrentListIndex);
		}
	}

	return eResult;
}
//====================================================================================================
// stop moving the sound
//====================================================================================================
void CAkPath::Stop()
{

	// update the state
	m_eState = Idle;
}
//====================================================================================================
//====================================================================================================
void CAkPath::Pause( AkUInt32 in_CurrentBufferTick )
{
	m_TimePaused = in_CurrentBufferTick;
	// update the state
	m_eState = Paused;
}
//====================================================================================================
//====================================================================================================
void CAkPath::Resume( AkUInt32 in_CurrentBufferTick )
{
	// erase the time spent in pause mode
	AkUInt32 TimeDelta = in_CurrentBufferTick - m_TimePaused;
	m_StartTime += TimeDelta;
	m_EndTime += TimeDelta;
	m_fb = -( m_StartTime * m_fa );

	// update the state
	m_eState = Running;
}
//====================================================================================================
// start moving toward next one
//====================================================================================================
void CAkPath::NextVertex()
{
	AKASSERT( m_pCurrentList );

	// get the starting vertex

    // REVIEW: This condition is meant to prevent crashes when the SECOND vertex could not be added to the
    // list. Wwise always packages at least 2 vertices even if there is only one. But out-of-memory conditions
    // can still occur, so this check has to be done anyway. TODO: Package only one vertex when path lenght is 1.
    if ( m_uCurrentVertex < m_pCurrentList->iNumVertices )
    {
	    AkPathVertex & To = m_pCurrentList->pVertices[ m_uCurrentVertex++ ];

	    // -- The start position must be set even if there in no more vertex
	    // -- If there is no more vertex, it will also stand as the last notified position, required for Continuous mode containers sharing the same path
	    // -- When the transition reaches the end, that position will be given as unique position to every other sounds sharing this path.
		m_StartPosition.X = To.Vertex.X;
	    m_StartPosition.Y = To.Vertex.Y;
	    m_StartPosition.Z = To.Vertex.Z;
		RandomizePosition(m_StartPosition);

	    m_Duration = CAkPath::Convert( To.Duration );
    }

	bool bHasNextOne = true;
	bool bStartNewList = false;
	// not enough to start a new move ?
	if(m_uCurrentVertex >= m_pCurrentList->iNumVertices )
	{
		// no more vertices, look for another list
		if(GetNextPathList() == AK_Success)
		{
			bStartNewList = true;
		}
		else
		{
			m_eState = Idle;
			bHasNextOne = false;
		}
	}

	// can we keep moving on ?
	if(bHasNextOne)
	{
	    AkVector To = m_pCurrentList->pVertices[ m_uCurrentVertex ].Vertex;
		RandomizePosition(To);

		// compute direction
		m_Direction.X = To.X - m_StartPosition.X;
		m_Direction.Y = To.Y - m_StartPosition.Y;
		m_Direction.Z = To.Z - m_StartPosition.Z;

		// start moving
		m_StartTime = m_EndTime;
		m_EndTime += m_Duration;
		m_fa = 1.0f / m_Duration;
		m_fb = -( m_StartTime * m_fa );

		if(bStartNewList)
		{
			// new one started
			MONITOR_PATH_EVENT(m_playingID,m_ulSoundUniqueID,AkMonitorData::AkPathEvent_ListStarted,m_ulCurrentListIndex);
		}
	}
}
//====================================================================================================
//====================================================================================================
bool CAkPath::IsRunning()
{
	return (m_eState == Running);
}
//====================================================================================================
//====================================================================================================
bool CAkPath::IsIdle()
{
	return (m_eState == Idle);
}
//====================================================================================================
// update the positions
//====================================================================================================
void CAkPath::UpdatePosition( AkUInt32 in_CurrentBufferTick )
{
	// compute the current value
	AkReal32 fdP = m_fa * in_CurrentBufferTick + m_fb;

	fdP = AkMath::Min(fdP,1.0f);
	fdP = AkMath::Max(fdP,0.0f);

	// compute the new position
	AkVector	Position;
	Position.X = m_StartPosition.X + fdP * m_Direction.X;
	Position.Y = m_StartPosition.Y + fdP * m_Direction.Y;
	Position.Z = m_StartPosition.Z + fdP * m_Direction.Z;

	AkVector	DeltaPosition;
	DeltaPosition.X = Position.X - m_LastPositionNotified.X;
	DeltaPosition.Y = Position.Y - m_LastPositionNotified.Y;
	DeltaPosition.Z = Position.Z - m_LastPositionNotified.Z;

	// send the new stuff to the users
	for( AkPBIList::Iterator iter = m_PBIsList.Begin(); iter != m_PBIsList.End(); ++iter )
	{
			AKASSERT(*iter);
			// set the sound's position
			(*iter)->Get3DAutomation()->SetPositionFromPath( DeltaPosition );
	}

	m_LastPositionNotified = Position;

	// are we done with this one ?
	if( in_CurrentBufferTick >= m_EndTime )
	{
		// go to next one
		NextVertex();
	}
}
//====================================================================================================
// sets the play list that is going to be used
//====================================================================================================
AKRESULT CAkPath::SetPathsList(AkPathListItem*	in_pPathList,
							   AkUInt32			in_ulListSize,
							   AkPathMode		in_PathMode,
							   bool				in_bIsLooping,
							   AkPathState*		in_pState)
{
	AKRESULT eResult = AK_Fail;

	// is it ok to set the list ?
	if(m_eState == Idle)
	{
		m_pPathsList = in_pPathList;
		m_ulListSize = (AkUInt16) in_ulListSize;
		m_PathMode = in_PathMode;
		m_bIsLooping = in_bIsLooping;
//----------------------------------------------------------------------------------------------------
// we have got played flags in the saved state
//----------------------------------------------------------------------------------------------------
		if(!in_pState->pbPlayed.IsNull())
		{
			// use these
			m_pbPlayed = in_pState->pbPlayed;	
			m_ulCurrentListIndex = (AkUInt16) in_pState->ulCurrentListIndex;
			// set the current one
			AKASSERT(m_ulCurrentListIndex < m_ulListSize);
			m_pCurrentList = m_pPathsList + m_ulCurrentListIndex;
		}
//----------------------------------------------------------------------------------------------------
// we have not got played flags in the saved state
//----------------------------------------------------------------------------------------------------
		else
		{
			m_ulCurrentListIndex = 0;
			m_pCurrentList = in_pPathList;
			m_pbPlayed = AkSharedPathState::Create(in_ulListSize);
			if (m_pbPlayed.IsNull())
				return AK_Fail;

			// should we randomly pick the first one ?
			if(in_PathMode & AkPathRandom)
			{
				PickRandomList();
			}
		}
		// we're all set
		eResult = AK_Success;
	}

	return eResult;
}
//====================================================================================================
// looks for the next path in the playlist, makes it current and adds its vertices
//====================================================================================================
AKRESULT CAkPath::GetNextPathList()
{
	AKRESULT	eResult = AK_NoMoreData;
	bool		bLooped;

	// have we got a list already ?
	if(m_pCurrentList != NULL)
	{
		// next one is random
		if(m_PathMode & AkPathRandom)
		{
			bLooped = PickRandomList();
		}
		// next one is sequence
		else
		{
			bLooped = PickSequenceList();
		}

		// should we continue ?
		if(m_PathMode & AkPathContinuous)
		{
			//  should we keep going ?
			if(!bLooped || (bLooped && m_bIsLooping))
			{
				m_uCurrentVertex = 0;
                eResult = AK_Success;
			}
		}
	}

	return eResult;
}
//====================================================================================================
//====================================================================================================
bool CAkPath::PickSequenceList()
{
	// assume some more to play
	bool	bLooped = false;

	// move to next one, is it ok ?
	if(++m_ulCurrentListIndex < m_ulListSize)
	{
		// go to the next list
		// m_pCurrentList modification is not allowed in step mode, see bug WG-3846
		if( m_PathMode & AkPathContinuous )
		{
			++m_pCurrentList;
		}
	}
	else
	{
		// back to first list
		// m_pCurrentList modification is not allowed in step mode, see bug WG-3846
		if( m_PathMode & AkPathContinuous )
		{
			m_pCurrentList = m_pPathsList;
		}
		m_ulCurrentListIndex = 0;
		bLooped = true;
	}
	return bLooped;
}
//====================================================================================================
// random : loop = all of them played at least once.
//====================================================================================================
bool CAkPath::PickRandomList()
{
	// pick next one
	m_ulCurrentListIndex = AKRANDOM::AkRandom() % m_ulListSize;

	// set the one we picked as current
	// m_pCurrentList modification is not allowed in step mode, see bug WG-3846
	if( m_PathMode & AkPathContinuous )
	{
		m_pCurrentList = m_pPathsList + m_ulCurrentListIndex;
	}

	// set this one has played
	bool bLooped = false;
	if( !m_pbPlayed.IsNull() )
	{
		// if all have been played once reset flags
		bLooped = m_pbPlayed->AllPlayed();
		if(bLooped)
			m_pbPlayed->ClearPlayedFlags();

		(*m_pbPlayed)[m_ulCurrentListIndex] = true;
	}

	return bLooped;
}
//====================================================================================================
//====================================================================================================

void CAkPath::RandomizePosition( AkVector& in_rPosition )
{
	in_rPosition.X += ((AkReal32)AKRANDOM::AkRandom() / (AKRANDOM::AK_RANDOM_MAX/2) - 1.0f) * m_pCurrentList->fRangeX;
	in_rPosition.Z += ((AkReal32)AKRANDOM::AkRandom() / (AKRANDOM::AK_RANDOM_MAX/2) - 1.0f) * m_pCurrentList->fRangeY;
	in_rPosition.Y += ((AkReal32)AKRANDOM::AkRandom() / (AKRANDOM::AK_RANDOM_MAX/2) - 1.0f) * m_pCurrentList->fRangeZ;
}
//====================================================================================================
//====================================================================================================

void CAkPath::SetSoundUniqueID( AkUniqueID in_soundUniqueID )
{
	m_ulSoundUniqueID = in_soundUniqueID;
}

void CAkPath::SetPlayingID( AkPlayingID in_playingID )
{
	m_playingID = in_playingID;
}

void CAkPath::SetIsLooping(bool in_bIsLooping)
{
	m_bIsLooping = in_bIsLooping;
}

void CAkPath::UpdateStartPosition()
{
	for( AkPBIList::Iterator iter = m_PBIsList.Begin(); iter != m_PBIsList.End(); ++iter )
	{
		AKASSERT(*iter);
		// set the 3D position
		(*iter)->Get3DAutomation()->SetPositionFromPath( m_StartPosition );
	}

	m_LastPositionNotified = m_StartPosition;
}

void CAkPath::InitRotationMatricesForNoFollowMode(const AkListenerSet& in_Listeners)
{
	for (AkListenerSet::Iterator it = in_Listeners.Begin(); it != in_Listeners.End(); ++it)
	{
		InitRotationMatrixForListener(*it);
	}
}

AkReal32 *CAkPath::InitRotationMatrixForListener( AkGameObjectID in_uListenerId )
{
	// Grab and copy the current listener orientation.
	AkReal32* pInverse= (AkReal32*)m_NoFollowOrientationRotationMatrices.Set(in_uListenerId);
	if (pInverse == NULL)
		return NULL;

	AkReal32 *pMat = CAkListener::GetListenerMatrix(in_uListenerId);
	if (pMat == NULL)
	{
		m_NoFollowOrientationRotationMatrices.Unset(in_uListenerId);
		return NULL;
	}

	memcpy(pInverse, pMat, 9 * sizeof(AkReal32));
	return pInverse;
}
