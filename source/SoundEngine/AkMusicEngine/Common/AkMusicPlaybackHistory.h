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

#ifndef _MUSIC_PLAYBACK_HISTORY_H_
#define _MUSIC_PLAYBACK_HISTORY_H_

#include <AK/SoundEngine/Common/AkCommonDefs.h>


class CAkMusicPackedHistory
{
private:
	struct Packed
	{
		AkInt32 m_refCount;
		AkUInt8 m_data[1];
	};

public:
	CAkMusicPackedHistory()
		: packed(NULL)
		, size(0)
		, poolId(AK_INVALID_POOL_ID)
	{}
	CAkMusicPackedHistory( const CAkMusicPackedHistory& in_other )
		: packed(NULL)
		, size(0)
	{
		Copy( in_other );
	}
	~CAkMusicPackedHistory()
	{
		Release();
	}
	const CAkMusicPackedHistory& operator=( const CAkMusicPackedHistory& in_other )
	{
		Copy( in_other );
		return *this;
	}
	void Copy( const CAkMusicPackedHistory& in_other )
	{
		Release();

		packed = in_other.packed;
		size = in_other.size;
		poolId = in_other.poolId;

		AddRef();
	}
	// !!Note!! can only attach to memory blocks which can be casted using the struct Packed above
	//		That is, the first 32 bits of the block must be the ref count!
	void Attach( const AkUInt8* in_packed, AkUInt32 in_size, AkMemPoolId in_poolId )
	{
		AKASSERT( in_packed != NULL && in_size != 0 && in_poolId != AK_INVALID_POOL_ID );
		packed = (Packed*)in_packed;
		size = in_size;
		poolId = in_poolId;
	}

	AkUInt32 Size() const { return size; }
	AkUInt8* Data() const { return reinterpret_cast<AkUInt8*>(packed); }

private:
	void AddRef()
	{
		if ( packed )
			++packed->m_refCount;
	}
	void Release()
	{
		if ( packed && --packed->m_refCount == 0 )
			AkFree( poolId, packed );
		packed = NULL;
		size = 0;
	}

	Packed* packed;
	AkUInt32 size;
	AkMemPoolId poolId;
};


class CAkMusicPlaybackHistory
{
public:
	CAkMusicPlaybackHistory()
		: m_uTimeLeftOff( 0 )
		, m_playlistId( AK_INVALID_UNIQUE_ID )
	{}

	CAkMusicPlaybackHistory( const CAkMusicPlaybackHistory& in_other );
	~CAkMusicPlaybackHistory();

	const CAkMusicPlaybackHistory& operator=( const CAkMusicPlaybackHistory& in_other );

	void SetPlaylistHistory( const CAkMusicPackedHistory& in_playlistHistory );

	AkTimeMs				m_uTimeLeftOff;
	AkUniqueID				m_playlistId;
	CAkMusicPackedHistory	m_playlistHistory;
};

#endif //_MUSIC_PLAYBACK_HISTORY_H_
