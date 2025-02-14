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
// AkContextualMusicSequencer.h
//
// Action sequencer for music contexts.
// Holds a list of pending musical actions, stamped with sample-based
// timing. 
// For example, a sequence context would enqueue actions on children 
// nodes that are scheduled to play or stop in the near future.
//
//////////////////////////////////////////////////////////////////////
#ifndef _CTX_MUSIC_SEQUENCER_H_
#define _CTX_MUSIC_SEQUENCER_H_

#include <AK/Tools/Common/AkListBare.h>
#include "AkMusicStructs.h"
#include "AkMusicTrack.h"
#include "AkMusicPBI.h"

class CAkChildCtx;
class CAkSwitchTrackInfo;

//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-

struct AkMusicAutomation
{
	AkMusicAutomation( CAkClipAutomation * in_pAutomationData, AkInt32 in_iTimeStart )
	: pAutomationData( in_pAutomationData )
	, pPBI( NULL )
	, iTimeStart( in_iTimeStart )
	{}

	inline void Apply( 
		AkInt32 in_iCurrentClipTime	)	// Current time relative to clip data (beginning of pre-entry).
	{
		pPBI->SetAutomationValue( 
			pAutomationData->Type(), 
			pAutomationData->GetValue( in_iCurrentClipTime - iTimeStart ) );
	}

	CAkClipAutomation *	pAutomationData;
	CAkMusicPBI	*		pPBI;
	AkInt32				iTimeStart;
	AkMusicAutomation *	pNextLightItem;
};

//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-

enum AkMusicActionType
{
	MusicActionTypePlay,
	MusicActionTypeStop,
	MusicActionTypeNotif,
	MusicActionTypePostEvent,
};

//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-

class AkMusicAction
{
public:
	virtual ~AkMusicAction() {}

	virtual AkMusicActionType Type() = 0;
	inline AkInt32 Time() { return m_iTime; }
	inline void Time( AkInt32 in_iTime ) { m_iTime = in_iTime; }

	AkMusicAction*	pNextItem;

protected:
	AkMusicAction( AkInt32 in_iTime )
		: m_iTime( in_iTime )
	{}

    AkInt32	m_iTime;
};

//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-

class AkMusicActionPlaySubTrack : public AkMusicAction
{
public:
	virtual AkMusicActionType Type() { return MusicActionTypePlay; }

	inline CAkMusicTrack* Track() { return m_pTrackNode; }
	inline AkUInt32 SubTrack() { return m_uSubTrack; }
	inline AkMusicFade& FadeParams() { return m_fadeParams; }
	inline AkInt32 SegmentPosition() { return m_iSegmentPosition; }

	AkMusicActionPlaySubTrack( AkInt32 in_iActionTime, CAkMusicTrack* in_pTrackNode, AkUInt32 in_uSubTrack, const AkMusicFade& in_fadeParams, int in_iSegmentPosition )
		: AkMusicAction( in_iActionTime )
		, m_pTrackNode( in_pTrackNode )
		, m_uSubTrack( in_uSubTrack )
		, m_fadeParams( in_fadeParams )
		, m_iSegmentPosition( in_iSegmentPosition )
	{
		AKASSERT( in_pTrackNode );
		in_pTrackNode->AddRef();
	}

	virtual ~AkMusicActionPlaySubTrack()
	{
		AKASSERT( m_pTrackNode );
		m_pTrackNode->Release();
	}

protected:
	CAkMusicTrack*	m_pTrackNode;
	AkUInt32		m_uSubTrack;
	AkMusicFade		m_fadeParams;
	AkInt32			m_iSegmentPosition;	// Time at which to schedule the segment playback, relative to entry-cue
};

class AkMusicActionStopSubTrack : public AkMusicAction
{
public:
	virtual ~AkMusicActionStopSubTrack() {}
	virtual AkMusicActionType Type() { return MusicActionTypeStop; }

	inline CAkSubTrackCtx* SubTrackCtx() { return m_pSubTrackCtx; }
	inline AkMusicFade& FadeParams() { return m_fadeParams; }

	AkMusicActionStopSubTrack( AkInt32 in_iTime, CAkSubTrackCtx* in_pSubTrackCtx, const AkMusicFade& in_fadeParams )
		: AkMusicAction( in_iTime )
		, m_pSubTrackCtx( in_pSubTrackCtx )
		, m_fadeParams( in_fadeParams )
	{}

protected:
	CAkSubTrackCtx*	m_pSubTrackCtx;
	AkMusicFade		m_fadeParams;
};

class AkMusicActionSwitchNotif : public AkMusicAction
{
public:
	virtual ~AkMusicActionSwitchNotif() {}
	virtual AkMusicActionType Type() { return MusicActionTypeNotif; }

	inline CAkSwitchTrackInfo* SwitchTrackInfo() { return m_pSwitchTrackInfo; }
	inline AkSwitchStateID PrevSwitchStateID() { return m_prevSwitchID; }
	inline AkSwitchStateID NewSwitchStateID() { return m_newSwitchID; }

	AkMusicActionSwitchNotif( AkInt32 in_iTime, CAkSwitchTrackInfo* in_pSwitchTrackInfo, AkSwitchStateID in_prevSwitchID, AkSwitchStateID in_newSwitchID )
		: AkMusicAction( in_iTime )
		, m_pSwitchTrackInfo( in_pSwitchTrackInfo )
		, m_prevSwitchID( in_prevSwitchID )
		, m_newSwitchID( in_newSwitchID )
	{}

protected:
	CAkSwitchTrackInfo*	m_pSwitchTrackInfo;
	AkSwitchStateID		m_prevSwitchID;
	AkSwitchStateID		m_newSwitchID;
};

//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-

class AkMusicActionPlayClip : public AkMusicAction
{
public:
	AkMusicActionPlayClip( 
		AkInt32				in_iTime,			// Action scheduled time.
		const AkTrackSrc&	in_rTrackSrc,		// Track source info.
		AkUInt32			in_uPlayDuration,	// Play Duration.
		AkUInt32			in_uSourceOffset,	// Source start position.
		AkUInt32			in_uPlayOffset		// Lower engine source start look-ahead.
		)
	: AkMusicAction( in_iTime )
	, m_rTrackSrc( in_rTrackSrc )
	, m_uPlayDuration( in_uPlayDuration )
	, m_uSourceOffset( in_uSourceOffset )
	, m_uPlayOffset( in_uPlayOffset )
	{}

	virtual ~AkMusicActionPlayClip() 
	{
		while ( !m_listAutomation.IsEmpty() )
		{
			AkMusicAutomation * pAutomation = m_listAutomation.First();
			m_listAutomation.RemoveFirst();
			AkDelete(AkMemID_Object, pAutomation );
		}
	}

	virtual AkMusicActionType Type() { return MusicActionTypePlay; }

	inline const AkTrackSrc & TrackSrc() { return m_rTrackSrc; }
	inline AkUInt32	PlayDuration() { return m_uPlayDuration; }
	inline void PlayDuration( AkUInt32 in_uPlayDuration ) { m_uPlayDuration = in_uPlayDuration; }
	inline AkUInt32	SourceOffset() { return m_uSourceOffset; }
	inline AkUInt32	PlayOffset() { return m_uPlayOffset; }

	// Query track for automation data and attach to action if applicable.
	inline void AttachClipAutomation(
		CAkMusicTrack*			in_pTrack,
		AkUInt32				in_uClipIndex,
		AkClipAutomationType	in_eType,
		AkInt32					in_iTimeStart
		)
	{
		CAkClipAutomation * pAutomationData = in_pTrack->GetClipAutomation( in_uClipIndex, in_eType );
		if (pAutomationData && pAutomationData->IsValid())
		{
			AkMusicAutomation * pAutomation = AkNew(AkMemID_Object, AkMusicAutomation( pAutomationData, in_iTimeStart ) );
			if ( pAutomation )
				m_listAutomation.AddFirst( pAutomation );
		}
	}

	inline AkMusicAutomation * PopAutomation()
	{
		AkMusicAutomation * pAutomation = m_listAutomation.First();
		if ( pAutomation )
			m_listAutomation.RemoveFirst();
		return pAutomation;
	}

protected:
	const AkTrackSrc &	m_rTrackSrc;	// Track source info.
	AkUInt32			m_uPlayDuration;// Duration of play time
	AkUInt32			m_uSourceOffset;// Source start position.
	AkUInt32			m_uPlayOffset;	// Lower engine source start look-ahead.
	typedef AkListBareLight<AkMusicAutomation> AutomationList;
	AutomationList		m_listAutomation;
};

class AkMusicActionStopClip : public AkMusicAction
{
public:
	AkMusicActionStopClip( 
		AkInt32				in_iTime,		// Action scheduled time.
		CAkChildCtx*		in_pTargetPBI	// Target PBI.
		) 
	: AkMusicAction( in_iTime )
	, pTargetPBI( in_pTargetPBI )
	, bHasAutomation(false)
	{}

	virtual AkMusicActionType Type() { return MusicActionTypeStop; }

	CAkChildCtx * 	    pTargetPBI;		// Target PBI.
	bool				bHasAutomation;
};

class AkMusicActionPostEvent : public AkMusicAction
{
public:
	AkMusicActionPostEvent( AkInt32	in_iTime, AkUniqueID in_eventID )
		: AkMusicAction( in_iTime )
		, eventID( in_eventID )
	{} 

	virtual AkMusicActionType Type() { return MusicActionTypePostEvent; }

	AkUniqueID eventID;
};

//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-

class CAkMusicActionSequencer : public AkListBare<AkMusicAction>
{
public:
    CAkMusicActionSequencer();
    virtual ~CAkMusicActionSequencer();

    // Schedule an action.
    void ScheduleAction( 
        AkMusicAction * in_pAction );		// Action to be scheduled. Allocated by user.

    // Returns AK_NoMoreData when there is no action to be executed in next frame (out_action is invalid).
    // Otherwise, returns AK_DataReady.
    // NOTE: When actions are dequeued with this method, they are still referenced. Caller needs to
    // release them explicitly.
    AKRESULT PopImminentAction(
		AkInt32 in_iNow,					// Current time (samples).
		AkInt32 in_iFrameDuration,			// Number of samples to process.
        AkMusicAction*& out_pAction );		// Returned action. Freed by user.

    // Remove all actions from sequencer (ref counted targets are released).
    void Flush();
};

//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-

class CAkMusicSubTrackSequencer : public CAkMusicActionSequencer
{
public:
	// Stop the given:
	//  - if the track has a play action, simply remove it
	//  - if the track has a stop action, reschedule it for now
	void ClearSubTrackCtx( CAkSubTrackCtx* in_pSubTrackCtx );
};

//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-

class CAkMusicClipSequencer : public CAkMusicActionSequencer
{
public:
	// Removes from sequencer and frees all actions that reference the specified PBI. 
	void ClearTargetCtx( CAkChildCtx* in_pTarget );
};


#endif //_CTX_MUSIC_SEQUENCER_H_
