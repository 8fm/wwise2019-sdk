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
// CAkMidiClipCtx.cpp
//
// Manages the playout context of a midi source.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkMidiClipCtx.h"
#include "AkMidiClipMgr.h"
#include "AkSegmentCtx.h"
#include "AkMatrixAwareCtx.h"
#include "AkMatrixSequencer.h"
#include "AkBankMgr.h"
#include "AkSubTrackCtx.h"
#include "AkURenderer.h"

CAkMidiClipCtx::CAkMidiClipCtx
	(
		CAkMusicCtx *		in_parent,
		CAkMidiClipMgr *	in_pMidiMgr,
		CAkMusicTrack*		in_pTrack,
		CAkMusicSource*		in_pSource,
		CAkRegisteredObj*		in_pGameObj,		// Game object and channel association.
		TransParams &		in_rTransParams,
		UserParams &		in_rUserParams,
		const AkTrackSrc*	in_pSrcInfo,		// Pointer to track's source playlist item.
		AkUInt32			/*in_uPlayDuration*/,
		AkUInt32			in_uSourceOffset,
		AkUInt32			in_uFrameOffset
	)
: CAkMidiBaseCtx( in_pGameObj, in_rTransParams, in_rUserParams, AK_INVALID_UNIQUE_ID )
, CAkChildCtx( in_parent )
, CAkParameterTarget( AkRTPCKey() )
, m_pMidiMgr( in_pMidiMgr )
, m_pTrack( in_pTrack )
, m_pSource( in_pSource )
, m_pSrcInfo( in_pSrcInfo )
, m_pUsageSlot( NULL )
, m_pDataPtr( NULL )
, m_uDataSize( 0 )
, m_MidiParse( )
, m_uSourceOffset( in_uSourceOffset )
, m_uFrameOffset( in_uFrameOffset )
, m_uStopOffset( AK_NO_IN_BUFFER_STOP_REQUESTED )
, m_bWasPaused( false )
, m_bWasStopped( false )
, m_bFirstNote( true )
, m_bAbsoluteStop( false )
{
	AKASSERT( m_pMidiMgr );

	m_pTrack->AddRef();
}

CAkMidiClipCtx::~CAkMidiClipCtx()
{
	if ( m_pDataPtr )
		m_pSource->UnLockDataPtr();

	if ( m_pUsageSlot )
		m_pUsageSlot->Release( false );

	m_pTrack->DecrementActivityCount();
	m_pTrack->RemoveFromURendererActiveNodes();
	m_pTrack->RemoveActiveMidiClip( this );
	m_pTrack->Release();

	// Detach from midi mgr.  Do this before any other destructive operations!!
	if( m_pMidiMgr )
		m_pMidiMgr->DetachCtx( this );

	if (SubTrackCtx())
	{
		SubTrackCtx()->RemoveReferences( this );
	}

	// Disconnect from parent music ctx.  This MUST be done last, as it can
	// lead to a chain reaction of deletes!
	Disconnect();

	m_mapMutedNodes.Term();
}

AKRESULT CAkMidiClipCtx::Init()
{
	AKRESULT eResult = CAkMidiBaseCtx::Init();

    AKASSERT( Parent() || !"A MIDI Ctx HAS to have a parent" );
	Connect();

	if (!m_pTrack->IncrementActivityCount())
		return AK_Fail;

	m_pTrack->AddToURendererActiveNodes();
	RegisterParamTarget( m_pTrack );

	RefreshMusicParameters();

	// Do not move!  Even though the result is obtained much sooner,
	// we must still try to increment the activity count (and other things)
	if ( eResult != AK_Success )
		return AK_Fail;

	m_pSource->LockDataPtr( (void*&)m_pDataPtr, m_uDataSize, m_pUsageSlot );
	if ( m_pDataPtr && m_uDataSize != 0 )
	{
		if ( m_MidiParse.SetBuffer( m_pDataPtr, m_uDataSize ) != AK_Success )
		{
			MONITOR_ERROR(AK::Monitor::ErrorCode_MediaNotLoaded);
			return AK_Fail;
		}
		m_MidiParse.SetTempo( GetTargetTempo() );
		AkReal32 fSourceOffset = (AkReal32)AkTimeConv::SamplesToMilliseconds( m_uSourceOffset );
		if ( m_MidiParse.SeekToTime( fSourceOffset ) != AK_Success )
			return AK_Fail;
		m_MidiParse.SetLoopOk( m_pTrack->Loop() == AkLoopVal_NotLooping ? false : true );
	}
	else
	{
		AkSrcTypeInfo* srcType = m_pSource->GetSrcTypeInfo();
		if ( srcType )
		{
			MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_MediaNotLoaded, srcType->mediaInfo.sourceID, AK_INVALID_PLAYING_ID, GetGameObj() ? GetGameObj()->ID() : AK_INVALID_GAME_OBJECT, srcType->mediaInfo.sourceID, false);
		}
		else
		  MONITOR_ERROR(AK::Monitor::ErrorCode_MediaNotLoaded);
		return AK_Fail;
	}

	// Attach to midi mgr
	m_pMidiMgr->AttachCtx( this );


	m_pTrack->AddActiveMidiClip(this);

	AkRTPCKey rtpcKey;
	rtpcKey.GameObj() = m_pGameObj;
	rtpcKey.PlayingID() = m_UserParams.PlayingID();

	//REVIEW is the only reason to do this to get the mute state?  There are much better ways to do that...
	AkSoundParams musicHeirarchyParams; // <- unused, atm.
	AkPBIModValues ranges; //<--unused	
	musicHeirarchyParams.Request.ClearBits(); // <- no selected params	//REVIEW WHY? 
	m_pTrack->GetAudioParameters(
		musicHeirarchyParams, 		
		m_mapMutedNodes, 
		rtpcKey, 
		NULL, /* <- don't include range*/
		NULL, /* <- dont gather modulators */
		false, /*<- don't check bus*/
		NULL );

	m_bMuted = CheckIsMuted();

    return AK_Success;
}

void CAkMidiClipCtx::RefreshMusicParameters() const
{
	if ( CAkSubTrackCtx* pSubTrackCtx = SubTrackCtx() )
	{
		if ( CAkSegmentCtx* pSegmentCtx = static_cast<CAkSegmentCtx*>(pSubTrackCtx->Parent()) )
		{
			if ( CAkMatrixAwareCtx* pMatrixCtx = static_cast<CAkMatrixAwareCtx*>( pSegmentCtx->Parent() ) )
			{
				if ( CAkMatrixSequencer* pSequencer = pMatrixCtx->Sequencer() )
					pSequencer->SetParametersDirty( false );
			}
		}
	}
}

CAkMidiBaseMgr* CAkMidiClipCtx::GetMidiMgr() const
{
	return m_pMidiMgr;
}

AkUniqueID CAkMidiClipCtx::GetTrackID() const
{
	return m_pTrack != NULL ? m_pTrack->ID() : AK_INVALID_UNIQUE_ID ;
}


bool CAkMidiClipCtx::IsUsingThisSlot(const CAkUsageSlot* in_pMediaSlot) const
{
	return m_pUsageSlot == in_pMediaSlot;
}

bool CAkMidiClipCtx::RelocateMedia(CAkUsageSlot* in_pMediaSlot)
{
	CAkUsageSlot* pUsageSlot = NULL;
	if (!g_pBankManager)
		return false;
	
	//relocate the media if you can
	AkUInt8* pTrialData = NULL;
	AkUInt32 uTrialDataSize;

	//Call the lock with some trial data first. If pUsageSlot is not null, then it worked. Do it again for real.
	m_pSource->LockDataPtr((void*&)pTrialData, uTrialDataSize, pUsageSlot);
	if (pUsageSlot)
	{
		m_pSource->UnLockDataPtr();
		//release the old data and reassign
		CAkUsageSlot* pTempSlot = m_pUsageSlot;

		m_pUsageSlot = pUsageSlot;

		m_MidiParse.RelocateMedia(pTrialData);

		m_pDataPtr = pTrialData;
		m_uDataSize = uTrialDataSize;

		if (pTempSlot)
			pTempSlot->Release(false);

		return true;
	}


	return false;
}


#ifndef AK_OPTIMIZED
void CAkMidiClipCtx::GetMonitoringMuteSoloState( bool &out_bSolo, bool &out_bMute )
{
	m_pTrack->GetMonitoringMuteSoloState(out_bSolo, out_bMute);
}
#endif

void CAkMidiClipCtx::_Stop( AkUInt32 in_uStopOffset )
{
	OnLastFrame( in_uStopOffset );
}

//-----------------------------------------------------------------------------
// Name: OnPaused
// Desc: Called on pause action.
//-----------------------------------------------------------------------------
void CAkMidiClipCtx::OnPaused()
{
	if( m_pMidiMgr && !m_bWasPaused )
		m_pMidiMgr->OnPaused( this );
	m_bWasPaused = true;
}

//-----------------------------------------------------------------------------
// Name: OnResumed
// Desc: Called on un-pause action.
//-----------------------------------------------------------------------------
void CAkMidiClipCtx::OnResumed()
{
	if( m_pMidiMgr && m_bWasPaused )
		m_pMidiMgr->OnResumed( this );
	m_bWasPaused = false;
}

void CAkMidiClipCtx::MuteNotification(	
		AkReal32			in_fMuteRatio, 
		AkMutedMapItem&		in_rMutedItem,
		bool				in_bPrioritizeGameObjectSpecificItems
		) 
{
	m_mapMutedNodes.MuteNotification(in_fMuteRatio, in_rMutedItem, in_bPrioritizeGameObjectSpecificItems);
	m_bMuted = CheckIsMuted();
	if (m_bMuted)
	{
		m_pMidiMgr->KillAllNotes(this); 
	}
}

// Notify context that this will be the last processing frame, propagated from a high-level context Stop().
// Stop offset is set on the PBI. Lower engine will stop it in_uNumSamples after the beginning of the audio frame.

void CAkMidiClipCtx::OnLastFrame( AkUInt32 in_uNumSamples ) // Number of samples left to process before stopping. 
{
	// This function may delete the object; make sure that only happens at the end!
	AddRef();

	// Set stop offset only if smaller than previous value.
	if ( in_uNumSamples != AK_NO_IN_BUFFER_STOP_REQUESTED )
	{
		if ( in_uNumSamples < m_uStopOffset )
			SetStopOffset( in_uNumSamples );
	}
	else
	{
		SetStopOffset( 0 );
		SetAbsoluteStop( true );
		CleanupActions();
		SetWasStopped();
	}

	UnregisterParamTarget();

	// If this ctx must die then it has our blessing.
	Release();

	// Don't add any code after release (ctx may have died)!!
}

//-----------------------------------------------------------------------------
// Name: OnFrame
// Desc: Get vector of midi events found within the given time window
//-----------------------------------------------------------------------------
void CAkMidiClipCtx::OnFrame( MidiFrameEventList& in_listEvents, AkUInt32 in_uNumSamples ) // Number of samples to process. 
{
	// Must not continue if we've been paused.
	if ( m_bWasPaused )
		return;

	// This function may delete the object; make sure that only happens at the end!
	AddRef();

	// Consume frame offset first (TODO: must check for frame offset bigger than one frame
	// if we want to support MIDI file look-ahead).
	AkUInt32 uFrameOffset = m_uFrameOffset;
	AKASSERT( uFrameOffset <= in_uNumSamples );
	in_uNumSamples -= uFrameOffset;
	m_uFrameOffset = 0;

	// Local copies of members used for stopping.
	bool bWasStopped = m_bWasStopped;
	AkUInt32 uStopOffset = m_uStopOffset;
	if ( uStopOffset != AK_NO_IN_BUFFER_STOP_REQUESTED )
	{
		uStopOffset = (uStopOffset >= uFrameOffset)
			? uStopOffset - uFrameOffset
			: 0;
		// WG-40607: We now handle an offset of in_uNumSamples to catch an action on the last frame
		AKASSERT( uStopOffset <= in_uNumSamples );
	}

	// Convert in ms for MIDI data consumption.
	AkReal32 fTimeWindow = AkTimeConv::SamplesToMsReal( in_uNumSamples ); 

	// Get all events within the given time window!
	if ( fTimeWindow != 0.f )
	{
		WindowEventList listWndEvents;
		m_MidiParse.MoveEventWindow( fTimeWindow, listWndEvents );

		// Go through list of returned events and pass on those events that need to be.
		while ( ! listWndEvents.IsEmpty() )
		{
			// Remove from list.
			AkMidiWindowEvent* pWndEvent = *listWndEvents.Begin();
			listWndEvents.RemoveFirst();

			// Convert window offset from ms to samples.
			AkUInt32 uOffsetSamples = (AkUInt32)AkTimeConv::MillisecondsToSamples( pWndEvent->fWindowDelta );

			// Check if exceeded stop offset.  Once stop offset is reached, all note-on events are blocked.
			// Furthermore, all non-blocked events are tagged as weak events.  The weak flag is used by the MIDI mgr
			// for MIDI transitions.
			if ( AK_EXPECT_FALSE( uStopOffset != AK_NO_IN_BUFFER_STOP_REQUESTED ) )
			{
				bWasStopped = ( uOffsetSamples >= uStopOffset );
			}

			// Keep event if allowed.
			if ( ! pWndEvent->IsNoteOn() || ! bWasStopped )
			{
				uOffsetSamples += uFrameOffset;
				AddMidiEvent( in_listEvents, *pWndEvent, uOffsetSamples, pWndEvent->uEventIdx, m_bWasStopped, m_bFirstNote );
				m_bFirstNote = false;
			}

			// Release event memory.
			AkDelete(AkMemID_Object, pWndEvent );
		}
	}

	// Check if stop offset has been reached.
	if ( uStopOffset != AK_NO_IN_BUFFER_STOP_REQUESTED )
	{
		SetStopOffset( 0 );
		SetWasStopped();
	}

	// If this ctx must die then it has our blessing.
	Release();

	// Don't add any code after release (ctx may have died)!!
}

//-----------------------------------------------------------------------------
// Name: GetCCEvents
// Desc: Gets all CC events preceding event in_uEventIdx.  Duplicate CC events
//		 are discarded (only the most recent event for a given CC is kept).
//-----------------------------------------------------------------------------
void CAkMidiClipCtx::GetCCEvents( MidiFrameEventList& in_listEvents, AkUInt32 in_uFrameOffset, AkUInt32 in_uEventIdx )
{
	// Get current parsing position.
	AkMidiParseSe::MidiParsePtr parsePtr;
	m_MidiParse.GetCurrentPos( parsePtr );

	// Seek to the beginning of the file.
	m_MidiParse.SeekToStart();

	// Get all events upto the given event; only keep the CC events.
	for ( AkUInt32 uEventCnt = 0; uEventCnt != in_uEventIdx; ++uEventCnt )
	{
		AkUInt32 uNewIdx;
		AkMidiEventEx newEvent;
		if ( m_MidiParse.GetNextEvent( newEvent, uNewIdx ) )
		{
			if ( uNewIdx > in_uEventIdx )
				break;
			if ( newEvent.IsCcEvent() )
			{
				// Remove all events in list with the same CC.
				MidiFrameEventList::IteratorEx it = in_listEvents.BeginEx();
				while ( it != in_listEvents.End() )
				{
					if ( (*it)->m_midiEvent.IsSameCc( newEvent ) )
					{
						AkMidiFrameEvent* pToDel = *it;
						it = in_listEvents.Erase( it );
						AkDelete(AkMemID_Object, pToDel );
					}
					else
						++it;
				}

				// Add the new event.
				AddMidiEvent( in_listEvents, newEvent, in_uFrameOffset, uNewIdx, false, false );
			}
		}
		else
			break;
	}

	// Restore the parsing position.
	m_MidiParse.SetCurrentPos( parsePtr );
}

//-----------------------------------------------------------------------------
// Name: ResolveMidiTarget
// Desc: Go through music hiearchy, starting from track, to determine the root
//		 target for all generated events.
//-----------------------------------------------------------------------------
bool CAkMidiClipCtx::ResolveMidiTarget()
{
	// If we already had a target then release it
	if ( m_pRootTargetNode )
	{
		m_pRootTargetNode->Release();
		m_pRootTargetNode = NULL;
		m_uRootTargetId = AK_INVALID_UNIQUE_ID;
	}

	AKASSERT( m_uRootTargetId == AK_INVALID_UNIQUE_ID );

	// First get the track's target info
	bool bOverrideParent;
	bool bIsMidiTargetBus;
	AkUniqueID uMidiTargetId;
	m_pTrack->GetMidiTargetNode( bOverrideParent, uMidiTargetId, bIsMidiTargetBus );

	// If we don't have a midi target yet, then look at all parent music nodes
	CAkMusicNode* pMusicNode = static_cast<CAkMusicNode*>( m_pTrack->Parent() );

	while ( pMusicNode && !bOverrideParent )
	{
		pMusicNode->GetMidiTargetNode( bOverrideParent, uMidiTargetId, bIsMidiTargetBus );
		pMusicNode = static_cast<CAkMusicNode*>( pMusicNode->Parent() );
	}

	// If we do have a target then get a reference to it
	m_bIsRootTargetBus = bIsMidiTargetBus;
	m_uRootTargetId = uMidiTargetId;
	if ( m_uRootTargetId != AK_INVALID_UNIQUE_ID )
	{
		if( bIsMidiTargetBus )
			m_pRootTargetNode = g_pIndex->GetNodePtrAndAddRef( m_uRootTargetId, AkNodeType_Bus );
		else
			m_pRootTargetNode = g_pIndex->GetNodePtrAndAddRef( m_uRootTargetId, AkNodeType_Default );

		if ( ! m_pRootTargetNode )
			m_uRootTargetId = AK_INVALID_UNIQUE_ID;
	}
	else
		m_pRootTargetNode = NULL;

	MONITOR_MIDITARGET(
		m_UserParams.PlayingID(),
		m_pGameObj->ID(),
		( m_uRootTargetId != AK_INVALID_UNIQUE_ID ) ?
			AkMonitorData::NotificationReason_MidiTargetResolved : AkMonitorData::NotificationReason_MidiTargetUnresolved,
		m_pTrack->ID(),
		uMidiTargetId,
		0 );

	return m_pRootTargetNode != NULL;
}

//-----------------------------------------------------------------------------
// Name: GetTargetTempo
// Desc: Determines the target tempo for the clip:
//			(1) Determine if the tempo is determined by the hierarchy or file,
//			(2) If the hierarchy, get the tempo of the overriding music node.
//-----------------------------------------------------------------------------
AkReal32 CAkMidiClipCtx::GetTargetTempo()
{
	AkReal32 fTargetTempo = 0.0;

	// Start with the track and determine who decides the tempo source.
	AkMidiTempoSource eTempoSource;
	bool bIsOverride;
	m_pTrack->GetMidiTempoSource( bIsOverride, eTempoSource );

	// If we don't have an override, look at music node ancestors.
	CAkMusicNode* pMusicNode = static_cast<CAkMusicNode*>( m_pTrack->Parent() );
	while ( ! bIsOverride && pMusicNode )
	{
		pMusicNode->GetMidiTempoSource( bIsOverride, eTempoSource );
		pMusicNode = static_cast<CAkMusicNode*>( pMusicNode->Parent() );
	}

	// If the target tempo is determined by the hiearchy, start looking...
	if ( eTempoSource == AkMidiTempoSource_Hierarchy )
	{
		bIsOverride = false;
		pMusicNode = static_cast<CAkMusicNode*>( m_pTrack->Parent() );
		while ( ! bIsOverride && pMusicNode )
		{
			pMusicNode->GetMidiTargetTempo( bIsOverride, fTargetTempo );
			pMusicNode = static_cast<CAkMusicNode*>( pMusicNode->Parent() );
		}
	}
	else
	{
		AKASSERT( eTempoSource == AkMidiTempoSource_File );
	}

	return fTargetTempo;
}

CAkSubTrackCtx* CAkMidiClipCtx::SubTrackCtx() const
{
	AKASSERT( Parent() );
	return static_cast<CAkSubTrackCtx*>(Parent());
}


bool CAkMidiClipCtx::CheckIsMuted()
{
	AkReal32 l_fRatio = 1.0f;
	for( AkMutedMap::Iterator iter = m_mapMutedNodes.Begin(); iter != m_mapMutedNodes.End(); ++iter )
	{
		l_fRatio *= (*iter).item;
	}

	return l_fRatio == 0.f;
}

#ifndef AK_OPTIMIZED
// ----------------------------------------------------------------------------------------------
// Function to retrieve the play target (the node which was played, resulting in a music PBI)
// ----------------------------------------------------------------------------------------------
AkUniqueID CAkMidiClipCtx::GetPlayTargetID( AkUniqueID in_playTargetID ) const
{
	if( Parent() )
		in_playTargetID = Parent()->GetPlayTargetID( in_playTargetID );
	return in_playTargetID;
}
#endif
