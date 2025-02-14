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
// AkSwitchTrackInfo.cpp
//
// Switch track context.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkSwitchTrackInfo.h"
#include "AkSegmentCtx.h"

// -------------------------------------------------------------------
// -------------------------------------------------------------------
CAkSwitchTrackInfo* CAkSwitchTrackInfo::Create( CAkSegmentCtx* in_pSegmentCtx, CAkMusicTrack* in_pTrackNode )
{
	return AkNew(AkMemID_Object, CAkSwitchTrackInfo( in_pSegmentCtx, in_pTrackNode ) );
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void CAkSwitchTrackInfo::Destroy( CAkSwitchTrackInfo* in_pSwitchTrackCtx )
{
	return AkDelete(AkMemID_Object, in_pSwitchTrackCtx );
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
bool CAkSwitchTrackInfo::IsSubTrackActive( AkUInt16 in_uSubTrackIdx ) const
{
	ActiveSubTrackArray::Iterator it = m_arActiveSubTrack.Begin();
	for( ; it != m_arActiveSubTrack.End(); ++it )
	{
		if( in_uSubTrackIdx == *it )
			return true;
	}
	return false;
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void CAkSwitchTrackInfo::UpdateActiveSubTrack()
{
	// Make sure array is empty
	m_arActiveSubTrack.Term();

	// Update list
	if( const TrackSwitchInfo* pSwitchInfo = m_pTrackNode->GetTrackSwitchInfo() )
	{
		const TrackSwitchInfo::TrackSwitchAssoc& rSwitchAssoc = pSwitchInfo->GetSwitchAssoc();
		TrackSwitchInfo::TrackSwitchAssoc::Iterator it = rSwitchAssoc.Begin();
		for( AkUInt16 uSubTrackIdx = 0; it != rSwitchAssoc.End(); ++it, ++uSubTrackIdx )
		{
			if( m_idSwitchState == *it )
				m_arActiveSubTrack.AddLast( uSubTrackIdx );
		}
	}
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void CAkSwitchTrackInfo::InitSwitch()
{
	// Determine which switch must be used
	if( const TrackSwitchInfo* pSwitchInfo = m_pTrackNode->GetTrackSwitchInfo() )
	{
		if( m_pSegmentCtx )
		{
			AkRTPCKey rtpcKey( m_pSegmentCtx->GameObjectPtr(), m_pSegmentCtx->PlayingID() );
			m_idSwitchState = GetSwitchToUse( rtpcKey, pSwitchInfo->GetSwitchGroup(), pSwitchInfo->GetGroupType() );
		}
		else
			m_idSwitchState = AK_INVALID_UNIQUE_ID;

		// Consult track to further determine which switch to use
		if ( m_idSwitchState == AK_INVALID_UNIQUE_ID )
			m_idSwitchState = m_pTrackNode->GetDefaultSwitch();

		// Update list of active sub-tracks
		UpdateActiveSubTrack();
	}
}

// -------------------------------------------------------------------
// -------------------------------------------------------------------
void CAkSwitchTrackInfo::SetSwitch( AkUInt32 in_Switch, const AkRTPCKey& in_rtpcKey, AkRTPCExceptionChecker* in_pExCheck )
{
	CAkRegisteredObj* pGameObj = m_pSegmentCtx->GameObjectPtr();
	if (m_pTrackNode->GetTrackSwitchInfo() != NULL && AkGroupType_Switch == m_pTrackNode->GetTrackSwitchInfo()->GetGroupType())
	{
		if ( (in_rtpcKey.GameObj() != NULL && in_rtpcKey.GameObj() != pGameObj) || (in_pExCheck && in_pExCheck->IsException(pGameObj)) )
				return;
	}

	if( in_Switch != m_idSwitchState )
	{
		// Determine what clips are now active
		m_idPrevSwitchState = m_idSwitchState;
		m_idSwitchState = in_Switch;
		UpdateActiveSubTrack();

		// Stop all scheduled track clips
		m_pSegmentCtx->RescheduleSwitchTrack( this );
	}
}
