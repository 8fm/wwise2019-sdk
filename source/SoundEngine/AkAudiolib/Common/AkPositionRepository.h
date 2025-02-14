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
// AkPositionRepository.h
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_POSITION_REPOSITORY_H_
#define _AK_POSITION_REPOSITORY_H_

#include <AK/Tools/Common/AkKeyArray.h>
#include <AK/Tools/Common/AkLock.h>

#define AK_POSITION_REPOSITORY_MIN_NUM_INSTANCES 8

class CAkVPLSrcNode;

struct AkPositionInfo
{
	AkInt64					timeUpdated;	//Last time information was updated
	AkBufferPosInformation	bufferPosInfo;	//Buffer position information
};

struct AkBufferingInfo
{
	CAkVPLSrcNode *			pNode;			//Allows discriminating between sources per playing id
	AkBufferingInformation	bufferingInfo;	//Stream buffering information
};

class CAkPositionRepository
{
public:
	//initialization
	AKRESULT Init();
	void Term();

	//Public Methods
	void AddSource( AkPlayingID in_PlayingID, CAkVPLSrcNode * in_pNode );
	void RemoveSource( AkPlayingID in_PlayingID, CAkVPLSrcNode * in_pNode );

	void UpdatePositionInfo( AkPlayingID in_PlayingID, AkBufferPosInformation* in_pPosInfo, CAkVPLSrcNode * in_pNode );
	
	AKRESULT GetCurrPositions(AkPlayingID in_PlayingID, struct AkSourcePosition* out_puPositions, AkUInt32 * io_pcPositions, bool in_bExtrapolate);
	void RemovePlayingIDForBuffering( AkPlayingID in_PlayingID );
	void UpdateBufferingInfo(AkPlayingID in_PlayingID, CAkVPLSrcNode * in_pNode, AkBufferingInformation & in_bufferingInfo);
	AKRESULT GetBuffering( AkPlayingID in_PlayingID, AkTimeMs & out_buffering, AKRESULT & out_eStatus );

	// Update the timer, should be called once per frame.
	inline void UpdateTime()
	{
		if( !m_mapPosInfo.IsEmpty() )
			AKPLATFORM::PerformanceCounter( &m_i64LastTimeUpdated );
	}

	// SetRate, mostly used for paused/resumed sounds.
	void SetRate( AkPlayingID in_PlayingID, CAkVPLSrcNode * in_pNode, AkReal32 in_fNewRate );

#ifdef AK_ENABLE_ASSERTS
	void VerifyNoPlayingID(AkPlayingID in_PlayingID);
#endif

private:
	struct AkPositionKey
	{
		AkPlayingID playingID;
		CAkVPLSrcNode * pNode;

		bool operator==(const AkPositionKey& other) const
		{
			return playingID == other.playingID
				&& pNode == other.pNode;
		}
	};

	CAkKeyArray<AkPositionKey, AkPositionInfo> m_mapPosInfo;
	CAkKeyArray<AkPlayingID, AkBufferingInfo> m_mapBufferingInfo;

	CAkLock m_lock;
	AkInt64 m_i64LastTimeUpdated;
};

extern CAkPositionRepository* g_pPositionRepository;

#endif //_AK_POSITION_REPOSITORY_H_
