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
#ifndef AK_OPTIMIZED

#include "TrackProxyConnected.h"
#include "AkAudioLib.h"
#include "AkMusicTrack.h"
#include "CommandDataSerializer.h"
#include "ITrackProxy.h"

TrackProxyConnected::TrackProxyConnected( AkUniqueID in_id )
{
	CAkIndexable* pIndexable = AK::SoundEngine::GetIndexable( in_id, AkIdxType_AudioNode );
	if ( !pIndexable )
		pIndexable = CAkMusicTrack::Create( in_id );

	SetIndexable( pIndexable );
}

TrackProxyConnected::~TrackProxyConnected()
{
}

void TrackProxyConnected::HandleExecute( AkUInt16 in_uMethodID, CommandDataSerializer& in_rSerializer, CommandDataSerializer& out_rReturnSerializer )
{
	CAkMusicTrack * pTrack = static_cast<CAkMusicTrack *>( GetIndexable() );

	switch( in_uMethodID )
	{
	case ITrackProxy::MethodAddSource:
		{	
			//no remote set source
			break;
		}
	case ITrackProxy::MethodRemoveAllSources:
		{	
			//no remote set source
			break;
		}
	case ITrackProxy::MethodSetPlayList:
		{
			//no remote set playlist due to source collapse by SB generator.
			TrackProxyCommandData::SetPlayList setPlayList;
			if( in_rSerializer.Get( setPlayList ) )
				pTrack->SetPlayList( setPlayList.m_uNumPlaylistItem, setPlayList.m_pArrayPlaylistItems, setPlayList.m_uNumSubTrack );
			break;
		}

	case ITrackProxy::MethodAddClipAutomation:
		{
			TrackProxyCommandData::AddClipAutomation addClipAutomation;
			if( in_rSerializer.Get( addClipAutomation ) )
				pTrack->AddClipAutomation( addClipAutomation.m_uClipIndex, addClipAutomation.m_eAutomationType, addClipAutomation.m_pArrayPoints, addClipAutomation.m_uNumPoints );

			break;
		}

	case ITrackProxy::MethodIsStreaming:
		{
			TrackProxyCommandData::IsStreaming isStreaming;
			if( in_rSerializer.Get( isStreaming ) )
			{
//				pTrack->IsStreaming( isStreaming.m_param1 );
			}

			break;
		}

	case ITrackProxy::MethodIsZeroLatency:
		{
			TrackProxyCommandData::IsZeroLatency isZeroLatency;
			if( in_rSerializer.Get( isZeroLatency ) )
				pTrack->IsZeroLatency( isZeroLatency.m_param1 );

			break;
		}
	case ITrackProxy::MethodSetNonCachable:
		{
			TrackProxyCommandData::SetNonCachable isNonCachable;
			if( in_rSerializer.Get( isNonCachable ) )
				pTrack->SetNonCachable( isNonCachable.m_param1 );

			break;
		}
	case ITrackProxy::MethodLookAheadTime:
		{
			TrackProxyCommandData::LookAheadTime lookAheadTime;
			if( in_rSerializer.Get( lookAheadTime ) )
				pTrack->LookAheadTime( lookAheadTime.m_param1 );

			break;
		}
	case ITrackProxy::MethodSetMusicTrackType:
		{
			TrackProxyCommandData::SetMusicTrackType setMusicTrackType;
			if( in_rSerializer.Get( setMusicTrackType ) )
				pTrack->SetMusicTrackType( (AkMusicTrackType) setMusicTrackType.m_param1 );

			break;
		}
	case ITrackProxy::MethodSetSwitchGroup:
		{
			TrackProxyCommandData::SetSwitchGroup setSwitchGroup;
			if( in_rSerializer.Get( setSwitchGroup ) )
				pTrack->SetSwitchGroup( setSwitchGroup.m_ulGroup, setSwitchGroup.m_eGroupType );

			break;
		}
	case ITrackProxy::MethodSetDefaultSwitch:
		{
			TrackProxyCommandData::SetDefaultSwitch setDefaultSwitch;
			if( in_rSerializer.Get( setDefaultSwitch ) )
				pTrack->SetDefaultSwitch( setDefaultSwitch.m_ulSwitch );

			break;
		}
	case ITrackProxy::MethodSetSwitchAssoc:
		{
			TrackProxyCommandData::SetSwitchAssoc setSwitchAssoc;
			if( in_rSerializer.Get( setSwitchAssoc ) )
				pTrack->SetSwitchAssoc( setSwitchAssoc.m_ulNumAssoc, setSwitchAssoc.m_pulAssoc );

			break;
		}
	case ITrackProxy::MethodSetTransRule:
		{
			TrackProxyCommandData::SetTransRule setTransRule;
			if( in_rSerializer.Get( setTransRule ) )
				pTrack->SetTransRule( setTransRule.m_transRule );

			break;
		}
	case ITrackProxy::MethodSetOverride:
		{
			TrackProxyCommandData::SetOverride setOverride;
			if (in_rSerializer.Get( setOverride ))
				pTrack->SetOverride( (AkMusicOverride)setOverride.m_param1, setOverride.m_param2 != 0 );
			break;
		}
	case ITrackProxy::MethodSetFlag:
		{
			TrackProxyCommandData::SetFlag setFlag;
			if (in_rSerializer.Get( setFlag ))
				pTrack->SetFlag( (AkMusicFlag)setFlag.m_param1, setFlag.m_param2 != 0 );
			break;
		}

	default:
		__base::HandleExecute( in_uMethodID, in_rSerializer, out_rReturnSerializer );
	}
}
#endif // #ifndef AK_OPTIMIZED
