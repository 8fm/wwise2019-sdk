/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

//////////////////////////////////////////////////////////////////////
//
// AkMusicTrack.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkMusicTrack.h"
#include "AkMusicRenderer.h"
#include "AkBus.h"
#include "AkBankMgr.h"
#include "AkEvent.h"
#include "AkProfile.h"

extern AkPlatformInitSettings g_PDSettings;

CAkMusicTrack::CAkMusicTrack( 
    AkUniqueID in_ulID
    )
	: CAkSoundBase(in_ulID)
	, m_uNumSubTrack( 0 )
	, m_iLookAheadTime( 0 )
	, m_eTrackType( AkMusicTrackType_Normal )
	, m_SequenceIndex( (AkUInt16) -1 )
	, m_pSwitchInfo( NULL )
	, m_bOverrideParentMidiTarget( 0 )
	, m_bOverrideParentMidiTempo( 0 )
{
}

CAkMusicTrack::~CAkMusicTrack()
{
	AKASSERT(m_ActiveMidiClips.IsEmpty());
	m_ActiveMidiClips.Term();

    RemoveAllSourcesNoCheck();

    m_arSrcInfo.Term();
	m_arTrackPlaylist.Term();
	m_arClipAutomation.Term();

	if( m_pSwitchInfo )
		AkDelete(AkMemID_Structure, m_pSwitchInfo );
}

// Thread safe version of the constructor.
CAkMusicTrack * CAkMusicTrack::Create(
    AkUniqueID in_ulID
    )
{
	CAkMusicTrack * pTrack = AkNew(AkMemID_Structure, CAkMusicTrack(in_ulID));
    if( pTrack )
	{
        if( pTrack->Init() != AK_Success )
		{
			pTrack->Release();
			pTrack = NULL;
		}
	}
    return pTrack;
}

AKRESULT CAkMusicTrack::SetInitialValues( AkUInt8* in_pData, AkUInt32 in_ulDataSize, CAkUsageSlot*, bool in_bIsPartialLoadOnly )
{	
	AKOBJECT_TYPECHECK(AkNodeCategory_MusicTrack);

	AKRESULT eResult = AK_Success;

	//Read ID
	// We don't care about the ID, just read/skip it
	SKIPBANKDATA( AkUInt32, in_pData, in_ulDataSize );

	// Read overrides
	AkUInt8 uFlags = READBANKDATA( AkUInt8, in_pData, in_ulDataSize );
	m_bOverrideParentMidiTempo	=	(uFlags >> AkMusicNode_FlagBitPos_OverrideMidiTempo) & 1;
	m_bOverrideParentMidiTarget	=	(uFlags >> AkMusicNode_FlagBitPos_OverrideMidiTarget) & 1;
	m_bMidiTargetTypeBus =			(uFlags >> AkMusicNode_FlagBitPos_MidiTargetTypeBus) & 1;

	// Read sources
	AkUInt32 numSources = READBANKDATA( AkUInt32, in_pData, in_ulDataSize );
	for( AkUInt32 i = 0; i < numSources; ++i )
	{
		//Read Source info
		if( eResult == AK_Success )
		{
			AkBankSourceData oSourceInfo;
			eResult = CAkBankMgr::LoadSource(in_pData, in_ulDataSize, oSourceInfo);
			if (eResult != AK_Success)
				return eResult;

			if (oSourceInfo.m_pParam == NULL)
			{
				//This is a file source
				eResult = AddSource( oSourceInfo.m_MediaInfo.sourceID, oSourceInfo.m_PluginID, oSourceInfo.m_MediaInfo );
			}
			else
			{
				//This is a plugin
				eResult = AddPluginSource( oSourceInfo.m_MediaInfo.sourceID );
			}

			if( eResult != AK_Success )
				return AK_Fail;
		}
	}

	// Read playlist
	AkUInt32 numPlaylistItem = READBANKDATA( AkUInt32, in_pData, in_ulDataSize );
	if( numPlaylistItem )
	{
		AkTrackSrcInfo* pPlaylist = ( AkTrackSrcInfo* )AkMalign(AkMemID_Object, numPlaylistItem*sizeof(AkTrackSrcInfo), AK_64B_OS_STRUCT_ALIGN );
		if( !pPlaylist )
			return AK_InsufficientMemory;

		for( AkUInt32 i = 0; i < numPlaylistItem; ++i )
		{
			pPlaylist[i].trackID			= READBANKDATA( AkUInt32, in_pData, in_ulDataSize );
			pPlaylist[i].sourceID			= READBANKDATA( AkUniqueID, in_pData, in_ulDataSize );
			pPlaylist[i].eventID			= READBANKDATA( AkUniqueID, in_pData, in_ulDataSize );
			pPlaylist[i].fPlayAt			= READBANKDATA( AkReal64, in_pData, in_ulDataSize );
			pPlaylist[i].fBeginTrimOffset	= READBANKDATA( AkReal64, in_pData, in_ulDataSize );
			pPlaylist[i].fEndTrimOffset		= READBANKDATA( AkReal64, in_pData, in_ulDataSize );
			pPlaylist[i].fSrcDuration		= READBANKDATA( AkReal64, in_pData, in_ulDataSize );
		}
		AkUInt32 numSubTrack = READBANKDATA( AkUInt32, in_pData, in_ulDataSize );
		eResult = SetPlayList( numPlaylistItem, pPlaylist, numSubTrack );
		AkFalign(AkMemID_Object, pPlaylist);
	}

	if( eResult != AK_Success )
		return eResult;

	//Read automation data
	m_arClipAutomation.Term();
	AkUInt32 numClipAutomationItem = READBANKDATA( AkUInt32, in_pData, in_ulDataSize );
	if ( numClipAutomationItem )
	{
		if ( m_arClipAutomation.Reserve( numClipAutomationItem ) != AK_Success )
			return AK_Fail;

		for( AkUInt32 i = 0; i < numClipAutomationItem; ++i )
		{
			CAkClipAutomation * pClipAutomation = m_arClipAutomation.AddLast();
			AKASSERT( pClipAutomation );	//reserved
			AkUInt32				uClipIndex	= READBANKDATA( AkUInt32, in_pData, in_ulDataSize );
			AkClipAutomationType	eAutoType	= (AkClipAutomationType)READBANKDATA( AkUInt32, in_pData, in_ulDataSize );
			AkUInt32				uNumPoints	= READBANKDATA( AkUInt32, in_pData, in_ulDataSize );
			if ( uNumPoints )
			{
				if ( pClipAutomation->Set( uClipIndex, eAutoType, (AkRTPCGraphPoint*)in_pData, uNumPoints ) != AK_Success )
					return AK_Fail;
				
				const size_t sizeofAkRTPCGraphPoint = sizeof( AkReal32 ) + sizeof( AkReal32 ) + sizeof( AkInt32 );
				SKIPBANKBYTES( uNumPoints * sizeofAkRTPCGraphPoint, in_pData, in_ulDataSize );
			}
		}
	}

	//ReadParameterNode
	eResult = SetNodeBaseParams( in_pData, in_ulDataSize, in_bIsPartialLoadOnly );


	// !!! WARNING !!! 
	// Very important not to put anything between this and the following bail-out point!!
	// See in_bIsPartialLoadOnly condition that follows (see Alex for more info)
	// !!! WARNING !!! 


	if( in_bIsPartialLoadOnly )
	{
		//Partial load has been requested, probable simply replacing the actual source created by the Wwise on load bank.
		return eResult;
	}

	if( eResult == AK_Success )
	{
		eResult = SetMusicTrackType( ( AkMusicTrackType )READBANKDATA( AkUInt8, in_pData, in_ulDataSize ) );
		if( eResult == AK_Success && GetMusicTrackType() == AkMusicTrackType_Switch )
		{
			eResult = SetSwitchParams( in_pData, in_ulDataSize );
			if( eResult == AK_Success )
				eResult = SetTransParams( in_pData, in_ulDataSize );
		}
	}

	if( eResult == AK_Success )
	{
		LookAheadTime( READBANKDATA( AkTimeMs, in_pData, in_ulDataSize ) );
	}

	CHECKBANKDATASIZE( in_ulDataSize, eResult );

	return eResult;
}

AKRESULT CAkMusicTrack::SetSwitchParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	return m_pSwitchInfo->SetSwitchParams( io_rpData, io_rulDataSize );
}

AKRESULT CAkMusicTrack::SetTransParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	return m_pSwitchInfo->SetTransParams( io_rpData, io_rulDataSize );
}

// Return the node category.
AkNodeCategory CAkMusicTrack::NodeCategory()
{
    return AkNodeCategory_MusicTrack;
}

AKRESULT CAkMusicTrack::PlayInternal( AkPBIParams& )
{
    AKASSERT( !"Cannot play music tracks" );
	return AK_NotImplemented;
}

void CAkMusicTrack::ExecuteAction( ActionParams& in_rAction )
{
}

void CAkMusicTrack::ExecuteActionNoPropagate(ActionParams& in_rAction)
{
	CheckPauseTransition(in_rAction);
}

void CAkMusicTrack::ExecuteActionExcept( ActionParamsExcept& in_rAction )
{
	CheckPauseTransition(in_rAction);
}

// Wwise specific interface.
// -----------------------------------------
AKRESULT CAkMusicTrack::AddPlaylistItem(
		AkTrackSrcInfo &in_srcInfo
		)
{
	AkReal64 fClipDuration = in_srcInfo.fSrcDuration + in_srcInfo.fEndTrimOffset - in_srcInfo.fBeginTrimOffset;
	// Note: the UI sometimes pushes negative (or ~0) source duration. If it happens, ignore this playlist item.
	// The only exception is music event cues, which have a valid event ID.
	if ((in_srcInfo.fSrcDuration <= 0 || fClipDuration <= 0) && in_srcInfo.eventID == AK_INVALID_UNIQUE_ID)
		return AK_Success;


	AkTrackSrc * pPlaylistRecord = m_arTrackPlaylist.AddLast();
	if ( !pPlaylistRecord )
		return AK_Fail;

	pPlaylistRecord->uSubTrackIndex =	in_srcInfo.trackID;
	pPlaylistRecord->srcID =			in_srcInfo.sourceID;
	pPlaylistRecord->eventID =			in_srcInfo.eventID;

	AKASSERT( in_srcInfo.fPlayAt + in_srcInfo.fBeginTrimOffset > - ((AkReal64)(1000.f/(AkReal64)AK_CORE_SAMPLERATE)) );
	pPlaylistRecord->uClipStartPosition =	AkTimeConv::MillisecondsToSamples( in_srcInfo.fPlayAt + in_srcInfo.fBeginTrimOffset );

    pPlaylistRecord->uClipDuration =	AkTimeConv::MillisecondsToSamples( fClipDuration );

	pPlaylistRecord->uSrcDuration =		AkTimeConv::MillisecondsToSamples( in_srcInfo.fSrcDuration );

	AkInt32 iBeginTrimOffset = AkTimeConv::MillisecondsToSamples( in_srcInfo.fBeginTrimOffset );
	pPlaylistRecord->iSourceTrimOffset = pPlaylistRecord->uSrcDuration > 0 ?  iBeginTrimOffset % (AkInt32)pPlaylistRecord->uSrcDuration : 0;
	AKASSERT( abs( (int)pPlaylistRecord->iSourceTrimOffset ) <= (AkInt32)pPlaylistRecord->uSrcDuration );

	if ( pPlaylistRecord->iSourceTrimOffset < 0 )
		pPlaylistRecord->iSourceTrimOffset += pPlaylistRecord->uSrcDuration;
	AKASSERT( pPlaylistRecord->iSourceTrimOffset >= 0 );
	
	return AK_Success;
}

AKRESULT CAkMusicTrack::SetPlayList(
		AkUInt32		in_uNumPlaylistItem,
		AkTrackSrcInfo* in_pArrayPlaylistItems,
		AkUInt32		in_uNumSubTrack
		)
{
	NOTIFY_EDIT_DIRTY();
		
	m_arTrackPlaylist.Term();

	m_uNumSubTrack = in_uNumSubTrack;

	if ( m_arTrackPlaylist.Reserve( in_uNumPlaylistItem ) != AK_Success )
		return AK_Fail;

	for( AkUInt32 i = 0; i < in_uNumPlaylistItem; ++i )
	{
		// Add playlist item. Must succeed because we just reserved memory for the array.
		AKVERIFY( AddPlaylistItem( in_pArrayPlaylistItems[i] ) == AK_Success );
	}
	return AK_Success;
}

AKRESULT CAkMusicTrack::AddClipAutomation(
	AkUInt32				in_uClipIndex,
	AkClipAutomationType	in_eAutomationType,
	AkRTPCGraphPoint		in_arPoints[], 
	AkUInt32				in_uNumPoints 
	)
{
	ClipAutomationArray::Iterator it = m_arClipAutomation.FindByKey( in_uClipIndex, in_eAutomationType );
	if ( it != m_arClipAutomation.End() )
	{
		// Free automation curve (see note in CAkClipAutomation).
		(*it).ClearCurve();
		m_arClipAutomation.EraseSwap( it );

		NOTIFY_EDIT_DIRTY();
	}

	if ( in_uNumPoints > 0 )
	{
		CAkClipAutomation * pAutomation = m_arClipAutomation.AddLast();
		if ( pAutomation )
		{
			NOTIFY_EDIT_DIRTY();
			return pAutomation->Set( in_uClipIndex, in_eAutomationType, in_arPoints, in_uNumPoints );
		}
	}
	return AK_Success;
}

AKRESULT CAkMusicTrack::AddSource( 
	AkUniqueID      in_srcID,
	AkPluginID      in_pluginID,
    const AkOSChar* in_pszFilename,
	AkFileID		in_uCacheID
    )
{
	CAkMusicSource** ppSource = m_arSrcInfo.Exists( in_srcID );
	if( ppSource )
	{
		//Already there, if the source is twice in the same playlist, it is useless to copy it twice.
		return AK_Success;
	}
	else
	{
		ppSource = m_arSrcInfo.Set( in_srcID );
	}
    if ( ppSource )
    {   
		*ppSource = AkNew(AkMemID_Structure, CAkMusicSource() );
		if(*ppSource)
		{
			(*ppSource)->SetSource( in_srcID, in_pluginID, in_pszFilename, in_uCacheID, 
				true,		// Use filename string
				false );	// External sources not yet supported in music tracks.
			if( ! (*ppSource)->IsMidi() )
				(*ppSource)->StreamingLookAhead( m_iLookAheadTime );
		}
		else
		{
			m_arSrcInfo.Unset( in_srcID );
		}
    }
	return ( ppSource && *ppSource ) ? AK_Success : AK_Fail;
}

AKRESULT CAkMusicTrack::AddSource( 
		AkUniqueID in_srcID, 
		AkUInt32 in_pluginID, 
		AkMediaInformation in_MediaInfo
		)
{
    CAkMusicSource** ppSource = m_arSrcInfo.Exists( in_srcID );
	if( ppSource )
	{
		//Already there, if the source is twice in the same playlist, it is useless to copy it twice.
		return AK_Success;
	}
	else
	{
		ppSource = m_arSrcInfo.Set( in_srcID );
	}
    if ( ppSource )
    {   
		*ppSource = AkNew(AkMemID_Structure, CAkMusicSource() );
		if(*ppSource)
		{
			(*ppSource)->SetSource( in_pluginID, in_MediaInfo );
            (*ppSource)->StreamingLookAhead( m_iLookAheadTime );
		}
		else
		{
			m_arSrcInfo.Unset( in_srcID );
		}
    }
	return ( ppSource && *ppSource ) ? AK_Success : AK_Fail;
}

AKRESULT CAkMusicTrack::AddPluginSource( 
		AkUniqueID	in_srcID )
{
	CAkMusicSource** ppSource = m_arSrcInfo.Set( in_srcID );
    if ( ppSource )
    {   
		*ppSource = AkNew(AkMemID_Structure, CAkMusicSource() );
		if(*ppSource)
		{
			(*ppSource)->SetSource( in_srcID );
		}
		else
		{
			m_arSrcInfo.Unset( in_srcID );
		}
    }
	return ( ppSource && *ppSource ) ? AK_Success : AK_Fail;
}


bool CAkMusicTrack::HasBankSource()
{ 
	for( SrcInfoArray::Iterator iter = m_arSrcInfo.Begin(); iter != m_arSrcInfo.End(); ++iter )
	{
		if( iter.pItem->item->HasBankSource() )
			return true;
	}
	return false;
}

void CAkMusicTrack::RemoveAllSources()
{
	// WG-20999: Cannot remove sources while editing if we're playing. Sources will "leak",
	// in that if a new source with the same ID is added, it will be ignored, and other sources
	// could exist for a while although they are not needed. This is an authoring issue only, since
	// this function is always only called by the WAL through the local proxy.
	if ( !IsPlaying() )
		RemoveAllSourcesNoCheck();
}

void CAkMusicTrack::RemoveAllSourcesNoCheck()
{
	m_uNumSubTrack = 0;
	m_arTrackPlaylist.RemoveAll(); // FIXME: Should only remove sources from the playlist.
	for( SrcInfoArray::Iterator iter = m_arSrcInfo.Begin(); iter != m_arSrcInfo.End(); ++iter )
	{
		AkDelete(AkMemID_Structure, iter.pItem->item );
	}
	m_arSrcInfo.RemoveAll();
}

void CAkMusicTrack::IsZeroLatency( bool in_bIsZeroLatency )
{
	for( SrcInfoArray::Iterator iter = m_arSrcInfo.Begin(); iter != m_arSrcInfo.End(); ++iter )
	{
		iter.pItem->item->IsZeroLatency( in_bIsZeroLatency );
	}
}

void CAkMusicTrack::SetNonCachable( bool in_bNonCachable )
{
	for( SrcInfoArray::Iterator iter = m_arSrcInfo.Begin(); iter != m_arSrcInfo.End(); ++iter )
	{
		iter.pItem->item->SetNonCachable( in_bNonCachable );
	}
}

void CAkMusicTrack::LookAheadTime( AkTimeMs in_LookAheadTime )
{
	m_iLookAheadTime = AkTimeConv::MillisecondsToSamples( in_LookAheadTime * CAkMusicRenderer::StreamingLookAheadRatio() );
	for( SrcInfoArray::Iterator iter = m_arSrcInfo.Begin(); iter != m_arSrcInfo.End(); ++iter )
	{
		if( ! iter.pItem->item->IsMidi() )
			iter.pItem->item->StreamingLookAhead( m_iLookAheadTime );
	}
}

AkObjectCategory CAkMusicTrack::Category()
{
	return ObjCategory_Track;
}

// Like ParameterNodeBase's, but does not check parent.
bool CAkMusicTrack::GetStateSyncTypes( AkStateGroupID in_stateGroupID, CAkStateSyncArray* io_pSyncTypes )
{
	if( CheckSyncTypes( in_stateGroupID, io_pSyncTypes ) )
		return true;
	
	if( ParentBus() )
	{
		if( static_cast<CAkBus*>( ParentBus() )->GetStateSyncTypes( in_stateGroupID, io_pSyncTypes ) )
		{
			return true;
		}
	}
	return false;
}

// Interface for Contexts
// ----------------------

CAkMusicSource* CAkMusicTrack::GetSourcePtr( AkUniqueID SourceID )
{
	CAkMusicSource ** ppSource = m_arSrcInfo.Exists( SourceID );
	if( ppSource )
		return *ppSource;
	return NULL;
}

AKRESULT CAkMusicTrack::SetMusicTrackType( AkMusicTrackType in_eType )
{
	AKRESULT eResult = AK_Success;
	if( in_eType != m_eTrackType )
	{
		if( m_pSwitchInfo )
		{
			AkDelete(AkMemID_Structure, m_pSwitchInfo );
			m_pSwitchInfo = NULL;
		}
		if( in_eType == AkMusicTrackType_Switch )
		{
			m_pSwitchInfo = AkNew(AkMemID_Structure, TrackSwitchInfo );
			if( m_pSwitchInfo == NULL )
				eResult = AK_InsufficientMemory;
		}
		m_eTrackType = in_eType;
	}
	return eResult;
}

AkUInt16 CAkMusicTrack::GetNextRS()
{
	AkUInt16 uIndex = 0;
	switch( m_eTrackType )
	{
	case AkMusicTrackType_Normal:
		break;
		
	case AkMusicTrackType_Random:
		if( m_uNumSubTrack )
			uIndex = (AkUInt16)( AKRANDOM::AkRandom() % m_uNumSubTrack );
		break;

	case AkMusicTrackType_Sequence:
		++m_SequenceIndex;
		if( m_SequenceIndex >= m_uNumSubTrack )
		{
			m_SequenceIndex = 0;
		}
		uIndex = m_SequenceIndex;
		break;
	
	case AkMusicTrackType_Switch:
		break;

	default:
		AKASSERT( !"Unknown MusicTrackType" );
	}
	return uIndex;
}

AKRESULT CAkMusicTrack::PrepareData()
{
	// Part 1
	AKRESULT eResult = AK_Success;
	for( SrcInfoArray::Iterator iter = m_arSrcInfo.Begin(); iter != m_arSrcInfo.End(); ++iter )
	{
		eResult = iter.pItem->item->PrepareData();
		if( eResult != AK_Success )
		{
			// Undo what has been prepared up to now in Part 1
			for( SrcInfoArray::Iterator iterFlush = m_arSrcInfo.Begin(); iterFlush != iter; ++iterFlush )
			{
				iterFlush.pItem->item->UnPrepareData();
			}
			break;
		}
	}

	if(eResult == AK_Success)
	{
		// Part 2
		for (CAkMusicTrack::TrackPlaylist::Iterator iter = m_arTrackPlaylist.Begin(); iter != m_arTrackPlaylist.End(); ++iter)
		{
			CAkBankMgr::AkBankQueueItem bankLoadItemStub;
			bankLoadItemStub.eType = CAkBankMgr::QueueItemPrepareEvent;
			bankLoadItemStub.bankLoadFlag = AkBankLoadFlag_None;
			bankLoadItemStub.bankID = AK_INVALID_BANK_ID;
			bankLoadItemStub.callbackInfo.pCookie = 0;
			bankLoadItemStub.callbackInfo.pfnBankCallback = NULL;

			if (iter.pItem->eventID != AK_INVALID_UNIQUE_ID)
			{
				eResult = g_pBankManager->PrepareEvent(bankLoadItemStub, iter.pItem->eventID);

				if (eResult != AK_Success)
				{
					// Undo what has been prepared up to now in Part 2.
					for (CAkMusicTrack::TrackPlaylist::Iterator iterFlush = m_arTrackPlaylist.Begin(); iterFlush != iter; ++iterFlush)
					{
						g_pBankManager->UnprepareEvent(iterFlush.pItem->eventID);
					}
					
					// Revert Part 1 Entirely since we had failure in part 2
					for (SrcInfoArray::Iterator iterFlush = m_arSrcInfo.Begin(); iterFlush != m_arSrcInfo.End(); ++iterFlush)
					{
						iterFlush.pItem->item->UnPrepareData();
					}
					break; // bail out we had an error and reverted all we did in this function so far.
				}
			}
		}
	}
	
	return eResult;
}

void CAkMusicTrack::UnPrepareData()
{
	for( SrcInfoArray::Iterator iter = m_arSrcInfo.Begin(); iter != m_arSrcInfo.End(); ++iter )
	{
		iter.pItem->item->UnPrepareData();
	}

	for (CAkMusicTrack::TrackPlaylist::Iterator iter = m_arTrackPlaylist.Begin(); iter != m_arTrackPlaylist.End(); ++iter)
	{
		g_pBankManager->UnprepareEvent(iter.pItem->eventID);
	}
}

void CAkMusicTrack::GetMidiTargetNode( bool & r_bOverrideParent, AkUniqueID & r_uMidiTargetId, bool & r_bIsMidiTargetBus )
{
	r_bOverrideParent = m_bOverrideParentMidiTarget != 0;
	r_uMidiTargetId = (AkUniqueID)m_props.GetAkProp( AkPropID_MidiTargetNode, (AkInt32)AK_INVALID_UNIQUE_ID ).iValue;
	r_bIsMidiTargetBus = m_bMidiTargetTypeBus != 0;
}

void CAkMusicTrack::GetMidiTempoSource( bool & r_bOverrideParent, AkMidiTempoSource & r_eTempoSource )
{
	r_bOverrideParent = m_bOverrideParentMidiTempo != 0;
	r_eTempoSource = (AkMidiTempoSource)m_props.GetAkProp( AkPropID_MidiTempoSource, (AkInt32)AkMidiTempoSource_Hierarchy ).iValue;
}

void CAkMusicTrack::SetOverride( AkMusicOverride in_eOverride, bool in_bValue )
{
	AkUInt8 uValue = in_bValue ? 1 : 0;
	switch ( in_eOverride )
	{
	case AkMusicOverride_MidiTempo: m_bOverrideParentMidiTempo = uValue; break;
	case AkMusicOverride_MidiTarget: m_bOverrideParentMidiTarget = uValue; break;
	default: break;
	}
}

void CAkMusicTrack::SetFlag( AkMusicFlag in_eFlag, bool in_bValue )
{
	AkUInt8 uValue = in_bValue ? 1 : 0;
	switch ( in_eFlag )
	{
	case AkMusicFlag_MidiTargetIsBus: m_bMidiTargetTypeBus = uValue; break;
	}
}

AKRESULT CAkMusicTrack::SetSwitchGroup( AkUInt32 in_uGroupId, AkGroupType in_eGroupType )
{
	if( m_pSwitchInfo )
	{
		m_pSwitchInfo->SetSwitchGroup( in_uGroupId );
		m_pSwitchInfo->SetGroupType( in_eGroupType );
	}
	return AK_Success;
}

AKRESULT CAkMusicTrack::SetDefaultSwitch( AkUInt32 in_uSwitch )
{
	if( m_pSwitchInfo )
		m_pSwitchInfo->SetDefaultSwitch( in_uSwitch );
	return AK_Success;
}

AKRESULT CAkMusicTrack::SetSwitchAssoc( AkUInt32 in_ulNumAssoc, AkUInt32* in_pulAssoc )
{
	if( m_pSwitchInfo )
		return m_pSwitchInfo->SetSwitchAssoc( in_ulNumAssoc, in_pulAssoc );

	return AK_Success;
}

AKRESULT CAkMusicTrack::SetTransRule( AkWwiseMusicTrackTransRule& in_transRule )
{
	if( m_pSwitchInfo )
		return m_pSwitchInfo->SetTransRule( in_transRule );

	return AK_Success;
}

AkUInt32 CAkMusicTrack::GetDefaultSwitch()
{
	if( m_pSwitchInfo )
		return m_pSwitchInfo->GetDefaultSwitch();
	return AK_INVALID_UNIQUE_ID;
}


//------------------------------------------------------------------------------------
// TrackSwitchInfo
//------------------------------------------------------------------------------------

AKRESULT TrackSwitchInfo::SetSwitchParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	// Read switch info
	SetGroupType( (AkGroupType)READBANKDATA( AkUInt8, io_rpData, io_rulDataSize ) );
	SetSwitchGroup( READBANKDATA( AkUInt32, io_rpData, io_rulDataSize ) );
	SetDefaultSwitch( READBANKDATA( AkUInt32, io_rpData, io_rulDataSize ) );

	m_arSwitchAssoc.Term();
	AkUInt32 numSwitchAssoc = READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );

	if ( m_arSwitchAssoc.Reserve( numSwitchAssoc ) != AK_Success )
		return AK_Fail;
	for( AkUInt32 i = 0; i < numSwitchAssoc; ++i )
	{
		AkUInt32* pulSwitchAssoc = m_arSwitchAssoc.AddLast();
		AKASSERT( pulSwitchAssoc );	//reserved
		*pulSwitchAssoc = READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
	}

	return AK_Success;
}

AKRESULT TrackSwitchInfo::SetTransParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	// Read trans info
	m_transRule.srcFadeParams.transitionTime	= READBANKDATA( AkInt32, io_rpData, io_rulDataSize );
	m_transRule.srcFadeParams.eFadeCurve		= (AkCurveInterpolation)READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
	m_transRule.srcFadeParams.iFadeOffset		= AkTimeConv::MillisecondsToSamples( READBANKDATA( AkInt32, io_rpData, io_rulDataSize ) );
	m_transRule.eSyncType						= READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
	m_transRule.uCueFilterHash					= READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );

	m_transRule.destFadeParams.transitionTime	= READBANKDATA( AkInt32, io_rpData, io_rulDataSize );
	m_transRule.destFadeParams.eFadeCurve		= (AkCurveInterpolation)READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
	m_transRule.destFadeParams.iFadeOffset		= AkTimeConv::MillisecondsToSamples( READBANKDATA( AkInt32, io_rpData, io_rulDataSize ) );

	return AK_Success;
}

AKRESULT TrackSwitchInfo::SetSwitchAssoc( AkUInt32 in_uNumAssoc, AkUInt32* in_puAssoc )
{
	m_arSwitchAssoc.RemoveAll();
	if( in_uNumAssoc != 0 )
	{
		if( ! m_arSwitchAssoc.Resize( in_uNumAssoc ) )
			return AK_InsufficientMemory;

		for( AkUInt32 i = 0; i < in_uNumAssoc; ++i )
			m_arSwitchAssoc[i] = in_puAssoc[i];
	}

	return AK_Success;
}

AKRESULT TrackSwitchInfo::SetTransRule( AkWwiseMusicTrackTransRule& in_transRule )
{
	m_transRule.eSyncType = in_transRule.eSrcSyncType;
	m_transRule.uCueFilterHash = (AkUniqueID)in_transRule.uSrcCueFilterHash;
	m_transRule.srcFadeParams = in_transRule.srcFade;
	m_transRule.srcFadeParams.iFadeOffset = AkTimeConv::MillisecondsToSamples( m_transRule.srcFadeParams.iFadeOffset );
	m_transRule.destFadeParams = in_transRule.destFade;
	m_transRule.destFadeParams.iFadeOffset = AkTimeConv::MillisecondsToSamples( m_transRule.destFadeParams.iFadeOffset );

	return AK_Success;
}


void CAkMusicTrack::GatherSounds(	AkSoundArray& io_aActiveSounds, 
									AkSoundArray& io_aInactiveSounds, 
									AkGameSyncArray& io_aGameSyncs, 
									bool in_bIsActive, 
									CAkRegisteredObj* in_pGameObj, 
									AkUInt32 in_uUpdateGameSync, 
									AkUInt32 in_uNewGameSyncValue)
{
	for( SrcInfoArray::Iterator iter = m_arSrcInfo.Begin(); iter != m_arSrcInfo.End(); ++iter )
	{
		AkSrcTypeInfo* pSrcInf = iter.pItem->item->GetSrcTypeInfo();
		if (pSrcInf->mediaInfo.Type == SrcTypeFile )
		{
			if (in_bIsActive)
			{
				io_aActiveSounds.AddLast(pSrcInf);
			}
			else
			{
				io_aInactiveSounds.AddLast(pSrcInf);
			}
		}
	}
}

// -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
// Computes minimum lookahead time required to play track at request segment position.
// -*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
AkInt32 CAkMusicTrack::ComputeMinSrcLookAhead(
	AkInt32 in_iPosition )	// Segment position, relative to pre-entry.
{
	AkInt32 iLookAhead = 0;

	// Get info from all sources on the required subtrack.
	CAkMusicTrack::TrackPlaylist::Iterator it = m_arTrackPlaylist.Begin();
	while( it != m_arTrackPlaylist.End() )
	{
		// Must look at all sub-tracks to get worst case (WG-22521) !
		{
			const AkTrackSrc & srcInfo = (*it);
			CAkMusicSource * pSrc = GetSourcePtr( srcInfo.srcID );

			if( pSrc )
			{
				AkSrcTypeInfo * pSrcTypeInfo = pSrc->GetSrcTypeInfo();
				AKASSERT( pSrcTypeInfo );

				// Get source's relative start position. If < 0, make 0.
				AkInt32 iRelativeStartPos = srcInfo.uClipStartPosition - in_iPosition;

				// Get required look-ahead for this source at position in_iPosition.
				// Look ahead is the source's look-ahead, if it is streaming, and has no prefetched data or
				// play position will not be 0.
				AkInt32 iSrcRelLookAhead = 0;
				if ( ( pSrcTypeInfo->mediaInfo.Type == SrcTypeFile ) &&
					( !pSrc->IsZeroLatency() || iRelativeStartPos < 0 || srcInfo.iSourceTrimOffset != 0 ) )
				{
					iSrcRelLookAhead = pSrc->StreamingLookAhead();
				}

				// Special case to compensate for XMA and AAC latency.
#ifdef AK_XMA_SUPPORTED
				if ( CODECID_FROM_PLUGINID( pSrcTypeInfo->dwID ) == AKCODECID_XMA )
				{
					iSrcRelLookAhead += AK_NUM_VOICE_REFILL_FRAMES;
					AkInt32 iMinLookahead = AK_IM_HW_CODEC_MIN_LOOKAHEAD_XMA * (AkInt32)AK_NUM_VOICE_REFILL_FRAMES;
					if ( iSrcRelLookAhead < iMinLookahead )
						iSrcRelLookAhead = iMinLookahead;
				}
#endif
#ifdef AK_IOS
				if ( CODECID_FROM_PLUGINID( pSrcTypeInfo->dwID ) == AKCODECID_AAC 
					&& !AK_PERF_OFFLINE_RENDERING )
				{
					iSrcRelLookAhead += MUSIC_TRACK_AAC_LOOK_AHEAD_TIME_BOOST;
				}
#endif
#ifdef AK_ATRAC9_SUPPORTED
				if ( CODECID_FROM_PLUGINID( pSrcTypeInfo->dwID ) == AKCODECID_ATRAC9 )
				{
					iSrcRelLookAhead += AK_NUM_VOICE_REFILL_FRAMES;
					AkUInt32 uMinLookahead = AK_IM_HW_CODEC_MIN_LOOKAHEAD_ATRAC9 * AK_NUM_VOICE_REFILL_FRAMES;
					if ( iSrcRelLookAhead < uMinLookahead )
						iSrcRelLookAhead = uMinLookahead;
				}
#endif
#ifdef AK_IM_HW_CODEC_MIN_LOOKAHEAD_OPUS_WEM
				if (CODECID_FROM_PLUGINID(pSrcTypeInfo->dwID) == AKCODECID_AKOPUS_WEM)
				{
					iSrcRelLookAhead += AK_NUM_VOICE_REFILL_FRAMES;
					AkUInt32 uMinLookahead = AK_IM_HW_CODEC_MIN_LOOKAHEAD_OPUS_WEM * AK_NUM_VOICE_REFILL_FRAMES;
					if (iSrcRelLookAhead < uMinLookahead)
						iSrcRelLookAhead = uMinLookahead;
				}
#endif
#ifdef AK_NX
				if (CODECID_FROM_PLUGINID(pSrcTypeInfo->dwID) == AKCODECID_OPUSNX)
				{
					if (srcInfo.iSourceTrimOffset != 0)  // when seeking, compensate for CAkSrcOpusBase::GetNominalPacketPreroll
						iSrcRelLookAhead += AK_NUM_VOICE_REFILL_FRAMES*4;
					else
						iSrcRelLookAhead += AK_NUM_VOICE_REFILL_FRAMES;
				}
#endif

				if ( iRelativeStartPos < 0 )
					iRelativeStartPos = 0;

				iSrcRelLookAhead -= iRelativeStartPos;

				if ( iLookAhead < iSrcRelLookAhead )
					iLookAhead = iSrcRelLookAhead;
			}
			// Music Event Cues have srcID == 0 and eventID != 0. We need to check for something that doesn't fit those criteria.
			else if (srcInfo.srcID || !srcInfo.eventID)
			{
				MONITOR_ERROR( AK::Monitor::ErrorCode_SelectedChildNotAvailable );
			}
		}
		++it;
	}

	return iLookAhead;
}


void CAkMusicTrack::MuteNotification(AkReal32 in_fMuteRatio, AkMutedMapItem& in_rMutedItem, bool in_bIsFromBus )
{
	if (!in_bIsFromBus)
	{
		for (MidiClipList::Iterator it = m_ActiveMidiClips.Begin(); it != m_ActiveMidiClips.End(); ++it)
			(*it)->MuteNotification(in_fMuteRatio, in_rMutedItem, false );
	}

	CAkSoundBase::MuteNotification(in_fMuteRatio, in_rMutedItem, in_bIsFromBus );
}

void CAkMusicTrack::MuteNotification( AkReal32 in_fMuteRatio, CAkRegisteredObj * in_pGameObj, AkMutedMapItem& in_rMutedItem, bool in_bPrioritizeGameObjectSpecificItems /* = false */ )
{
	for (MidiClipList::Iterator it = m_ActiveMidiClips.Begin(); it != m_ActiveMidiClips.End(); ++it)
	{
		CAkMidiClipCtx* pCtx = *it;
		if(pCtx->GetGameObj() == in_pGameObj)
			pCtx->MuteNotification( in_fMuteRatio, in_rMutedItem, in_bPrioritizeGameObjectSpecificItems );
	}

	CAkSoundBase::MuteNotification( in_fMuteRatio, in_pGameObj, in_rMutedItem, in_bPrioritizeGameObjectSpecificItems );
}
