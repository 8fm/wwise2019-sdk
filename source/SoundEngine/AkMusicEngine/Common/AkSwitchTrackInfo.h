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
// AkSwitchTrackCtx.h
//
// Switch track context.
//
//////////////////////////////////////////////////////////////////////
#ifndef _SWITCH_TRACK_INFO_H_
#define _SWITCH_TRACK_INFO_H_

#include "AkMusicTrack.h"
#include "AkSwitchAware.h"

class AkRTPCExceptionChecker;
class CAkSegmentCtx;

class CAkSwitchTrackInfo : public CAkSwitchAware
{
public:
	static CAkSwitchTrackInfo* Create( CAkSegmentCtx* in_pSegmentCtx, CAkMusicTrack* in_pTrackNode );
	static void Destroy( CAkSwitchTrackInfo* in_pSwitchTrackCtx );

	CAkSwitchTrackInfo( CAkSegmentCtx* in_pSegmentCtx, CAkMusicTrack* in_pTrackNode )
		: m_pSegmentCtx( in_pSegmentCtx )
		, m_pTrackNode( in_pTrackNode )
		, m_idSwitchState( AK_INVALID_UNIQUE_ID )
		, m_idPrevSwitchState( AK_INVALID_UNIQUE_ID )
	{
		AKASSERT( m_pTrackNode && m_pTrackNode->GetMusicTrackType() == AkMusicTrackType_Switch );
		m_pTrackNode->AddRef();
	}

	virtual ~CAkSwitchTrackInfo()
	{
		UnsubscribeSwitches();
		m_arActiveSubTrack.Term();
		m_pTrackNode->Release();
		m_pTrackNode = NULL;
	}

	CAkMusicTrack* Track() const { return m_pTrackNode; }

	typedef AkArray< AkUInt16, AkUInt16, ArrayPoolDefault > ActiveSubTrackArray;

private:

	CAkSegmentCtx* m_pSegmentCtx;
	CAkMusicTrack* m_pTrackNode;

	ActiveSubTrackArray	m_arActiveSubTrack;

	AkSwitchStateID	m_idSwitchState;
	AkSwitchStateID	m_idPrevSwitchState;

public:

	void Init()
	{
		if( const TrackSwitchInfo* pSwitchInfo = m_pTrackNode->GetTrackSwitchInfo() )
		{
			AKRESULT eResult = SubscribeSwitch( pSwitchInfo->GetSwitchGroup(), pSwitchInfo->GetGroupType() );
			if( eResult == AK_Success )
				InitSwitch();
		}
	}

	bool IsSubTrackActive( AkUInt16 in_uSubTrackIdx ) const;
	const ActiveSubTrackArray& ActiveSubtracks() const { return m_arActiveSubTrack; }
	void UpdateActiveSubTrack();

	void InitSwitch();
	virtual void SetSwitch( 
		AkUInt32 in_Switch, 
		const AkRTPCKey& in_rtpcKey,
		AkRTPCExceptionChecker* in_pExCheck = NULL );

	AkForceInline AkSwitchStateID SwitchState() { return m_idSwitchState; }
	AkForceInline AkSwitchStateID PrevSwitchState() { return m_idPrevSwitchState; }

public:

	// Next item in AkListBareLight
	CAkSwitchTrackInfo* pNextLightItem;
};

typedef AkListBareLight<CAkSwitchTrackInfo> SwitchTrackInfoList;

#endif
