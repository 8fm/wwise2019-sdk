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
// AkPositionRepository.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkCommon.h"
#include "AkPositionRepository.h"
#include "AkVPLSrcNode.h"
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/Tools/Common/AkAutoLock.h>

//-----------------------------------------------------------------------------
// Defines.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Name: Init
// Desc: Initialize CAkPositionRepository.
//
// Return: Ak_Success
//-----------------------------------------------------------------------------
AKRESULT CAkPositionRepository::Init()
{
	return m_mapPosInfo.Reserve( AK_POSITION_REPOSITORY_MIN_NUM_INSTANCES );
}

//-----------------------------------------------------------------------------
// Name: Term
// Desc: Terminates CAkPositionRepository.
//-----------------------------------------------------------------------------
void CAkPositionRepository::Term()
{
	m_mapPosInfo.Term();
	m_mapBufferingInfo.Term();
}

//-----------------------------------------------------------------------------
// Name: SetRate
// Desc: Update the last rate for a given ID, useful when sounds gets paused/resumed.
//
//-----------------------------------------------------------------------------
void CAkPositionRepository::SetRate( AkPlayingID in_PlayingID, CAkVPLSrcNode * in_pNode, AkReal32 in_fNewRate )
{
	AkAutoLock<CAkLock> gate(m_lock);
	AkPositionKey key = { in_PlayingID, in_pNode };
	AkPositionInfo* pPosInfo = m_mapPosInfo.Exists( key );
	if( pPosInfo )
	{
		pPosInfo->timeUpdated = m_i64LastTimeUpdated;
		pPosInfo->bufferPosInfo.fLastRate = in_fNewRate;
	}
}

//-----------------------------------------------------------------------------
// Name: AddSource
// Desc: Register a playing ID _with a cookie (source)_ for position information.
//-----------------------------------------------------------------------------
void CAkPositionRepository::AddSource( AkPlayingID in_PlayingID, CAkVPLSrcNode * in_pNode )
{
	AkAutoLock<CAkLock> gate(m_lock);
	AkPositionKey key = { in_PlayingID, in_pNode };
	if( m_mapPosInfo.Exists( key ) )
	{
		// There is already an entry for this playing ID. The caller must be another sound 
		// in the same event. Its position is ignored.
		return;
	}	

	MapStruct<AkPositionKey, AkPositionInfo> * pNewEntry = m_mapPosInfo.AddLast();
	if ( pNewEntry )
	{
		pNewEntry->key = key;

		// Initialize uSampleRate. As long as it is 1, the structure's content is not used/read.
		pNewEntry->item.bufferPosInfo.Clear();
	}
}

//-----------------------------------------------------------------------------
// Name: RemoveSource
// Desc: Unregister a playing ID for position information only if cookie (source)_ matches.
//-----------------------------------------------------------------------------
void CAkPositionRepository::RemoveSource( AkPlayingID in_PlayingID, CAkVPLSrcNode * in_pNode )
{
	AkAutoLock<CAkLock> gate(m_lock);
	AkPositionKey key = { in_PlayingID, in_pNode };
	CAkKeyArray<AkPositionKey, AkPositionInfo>::Iterator it = m_mapPosInfo.FindEx( key );
	if ( it != m_mapPosInfo.End() )
	{		
		m_mapPosInfo.Erase( it );
	}
}

//-----------------------------------------------------------------------------
// Name: UpdatePositionInfo
// Desc: Updates the position information associated with a PlayingID.
//-----------------------------------------------------------------------------
void CAkPositionRepository::UpdatePositionInfo( AkPlayingID in_PlayingID, AkBufferPosInformation* in_pPosInfo, CAkVPLSrcNode * in_pNode )
{
	AkAutoLock<CAkLock> gate(m_lock);

	AkPositionKey key = { in_PlayingID, in_pNode };
	AkPositionInfo* pPosInfo = m_mapPosInfo.Exists( key );
	
	if( !pPosInfo )
	{
		pPosInfo = m_mapPosInfo.Set( key );
		if( !pPosInfo )
			return;

		UpdateTime();
	}	
	
	pPosInfo->bufferPosInfo = *in_pPosInfo;
	pPosInfo->timeUpdated = m_i64LastTimeUpdated;
}

AKRESULT CAkPositionRepository::GetCurrPositions(AkPlayingID in_PlayingID, AkSourcePosition* out_puPositions, AkUInt32 * io_pcPositions, bool in_bExtrapolate)
{
	AkAutoLock<CAkLock> gate(m_lock);

	AkUInt32 uMaxPositions = *io_pcPositions;
	if (uMaxPositions == 0)
	{
		// The user requested to count only.
		out_puPositions = NULL;
		uMaxPositions = 0xFFFFFFFF;
	}
	AkUInt32 iPosition = 0;

	for (CAkKeyArray<AkPositionKey, AkPositionInfo>::Iterator it = m_mapPosInfo.Begin(); it != m_mapPosInfo.End() && iPosition < uMaxPositions; ++it)
	{
		if ((*it).key.playingID == in_PlayingID)
		{
			const AkPositionInfo & posInfo = (*it).item;
			if (posInfo.bufferPosInfo.uSampleRate == 1) // If sample rate == 1, the entry has been added but no update has been performed yet. (see AkBufferPosInformation::Clear)
				continue;

			if (out_puPositions == NULL)
			{
				++iPosition;
				continue;
			}

			AkReal32 fPosition = (posInfo.bufferPosInfo.uStartPos*1000.f) / posInfo.bufferPosInfo.uSampleRate;
			AkUInt32 uEndFileTime = (AkUInt32)((posInfo.bufferPosInfo.uFileEnd*1000.f) / posInfo.bufferPosInfo.uSampleRate);

			if (in_bExtrapolate)
			{
				//extrapolation using timer
				AkInt64 CurrTime;
				AKPLATFORM::PerformanceCounter(&CurrTime);
				AkReal32 fElapsed = AKPLATFORM::Elapsed(CurrTime, posInfo.timeUpdated);

				fPosition += (fElapsed * posInfo.bufferPosInfo.fLastRate);
			}

			AkUInt32 uPosition = (AkUInt32)fPosition;
			uPosition = AkMin(uPosition, uEndFileTime);

			out_puPositions[iPosition].audioNodeID = (*it).key.pNode->GetContext()->GetSoundID();
			out_puPositions[iPosition].mediaID = (*it).key.pNode->GetContext()->GetSrcTypeInfo()->mediaInfo.sourceID;
			out_puPositions[iPosition].msTime = uPosition;

			++iPosition;
		}
	}

	*io_pcPositions = iPosition;
	return iPosition == 0 ? AK_Fail : AK_Success;
}

#ifdef AK_ENABLE_ASSERTS

void CAkPositionRepository::VerifyNoPlayingID( AkPlayingID in_PlayingID )
{
	AkAutoLock<CAkLock> gate(m_lock);
	for (CAkKeyArray<AkPositionKey, AkPositionInfo>::Iterator it = m_mapPosInfo.Begin(); it != m_mapPosInfo.End(); ++it)
	{
		AKASSERT((*it).key.playingID != in_PlayingID);
	}
}

#endif

//-----------------------------------------------------------------------------
// Stream buffering section.
//-----------------------------------------------------------------------------
void CAkPositionRepository::RemovePlayingIDForBuffering( AkPlayingID in_PlayingID )
{
	AkAutoLock<CAkLock> gate(m_lock);
	m_mapBufferingInfo.Unset( in_PlayingID );
}

void CAkPositionRepository::UpdateBufferingInfo(AkPlayingID in_PlayingID, CAkVPLSrcNode * in_pNode, AkBufferingInformation & in_bufferingInfo)
{
	AkAutoLock<CAkLock> gate(m_lock);

	AkBufferingInfo* pBufferingInfo = m_mapBufferingInfo.Exists( in_PlayingID );	
	if( !pBufferingInfo )
	{
		pBufferingInfo = m_mapBufferingInfo.Set( in_PlayingID );
		if( !pBufferingInfo )
			return;

		pBufferingInfo->bufferingInfo = in_bufferingInfo;
	}
	else
	{
		if (pBufferingInfo->pNode == in_pNode)
		{
			// progress update for a given node.
			pBufferingInfo->bufferingInfo = in_bufferingInfo;
		}
		else
		{
			// Another stream has already pushed its buffering status. Merge with this new stream:
			// - Ignore failures.
			// - Take the smallest buffering amount (unless it is in a failure state).
			// - AK_Success supersedes AK_NoMoreData.
			if ( in_bufferingInfo.eBufferingState != AK_Fail )
			{
				if ( in_bufferingInfo.uBuffering < pBufferingInfo->bufferingInfo.uBuffering )
					pBufferingInfo->bufferingInfo.uBuffering = in_bufferingInfo.uBuffering;
				if ( pBufferingInfo->bufferingInfo.eBufferingState != AK_Success )
					pBufferingInfo->bufferingInfo.eBufferingState = in_bufferingInfo.eBufferingState;
			}
		}
	}
	pBufferingInfo->pNode = in_pNode;
}

AKRESULT CAkPositionRepository::GetBuffering( AkPlayingID in_PlayingID, AkTimeMs & out_buffering, AKRESULT & out_eStatus )
{
	AkAutoLock<CAkLock> gate(m_lock);
	AkBufferingInfo* pBufferingInfo = m_mapBufferingInfo.Exists( in_PlayingID );
	if( !pBufferingInfo )
		return AK_Fail;

	out_buffering = pBufferingInfo->bufferingInfo.uBuffering;
	out_eStatus = pBufferingInfo->bufferingInfo.eBufferingState;
	return AK_Success;
}

