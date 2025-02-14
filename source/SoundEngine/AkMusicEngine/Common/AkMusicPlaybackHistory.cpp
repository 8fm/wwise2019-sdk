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

#include "stdafx.h"
#include <AK/Tools/Common/AkObject.h>
#include "AkMusicPlaybackHistory.h"

CAkMusicPlaybackHistory::~CAkMusicPlaybackHistory()
{}

CAkMusicPlaybackHistory::CAkMusicPlaybackHistory( const CAkMusicPlaybackHistory& in_other )
	: m_uTimeLeftOff( in_other.m_uTimeLeftOff )
	, m_playlistId( in_other.m_playlistId )
	, m_playlistHistory( in_other.m_playlistHistory )
{}

const CAkMusicPlaybackHistory& CAkMusicPlaybackHistory::operator=( const CAkMusicPlaybackHistory& in_other )
{
	m_uTimeLeftOff = in_other.m_uTimeLeftOff;
	m_playlistId = in_other.m_playlistId;
	m_playlistHistory = in_other.m_playlistHistory;
	return *this;
}

void CAkMusicPlaybackHistory::SetPlaylistHistory( const CAkMusicPackedHistory& in_playlistHistory )
{
	m_playlistHistory = in_playlistHistory;
}
