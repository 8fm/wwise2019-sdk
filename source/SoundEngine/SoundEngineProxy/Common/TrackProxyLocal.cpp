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
#ifndef PROXYCENTRAL_CONNECTED

#include "TrackProxyLocal.h"
#include "AkMusicTrack.h"
#include "AkAudioLib.h"
#include "AkCritical.h"
#include "../../../../../Authoring/source/WwiseSoundEngine/WwiseCacheID.h"

namespace AK
{
	namespace WWISESOUNDENGINE_DLL
	{
		extern GlobalAnalysisSet g_setAnalysis;
	}
}

TrackProxyLocal::TrackProxyLocal( AkUniqueID in_id )
{
	CAkIndexable* pIndexable = AK::SoundEngine::GetIndexable( in_id, AkIdxType_AudioNode );

	SetIndexable( pIndexable != NULL ? pIndexable : CAkMusicTrack::Create( in_id ) );
}

TrackProxyLocal::~TrackProxyLocal()
{
	ReleaseAllMidiMedia();
}

AKRESULT TrackProxyLocal::AddSource(
		AkUniqueID      in_srcID,
		AkPluginID      in_pluginID,
        const AkOSChar* in_pszFilename,
		const struct CacheID &	in_guidCacheID
		)
{
	CAkMusicTrack* pIndexable = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		const AkOSChar* pszFileName = (CODECID_FROM_PLUGINID( in_pluginID ) == AKCODECID_MIDI) ? NULL : in_pszFilename;

		AkFileID fileID = AK::WWISESOUNDENGINE_DLL::CacheIDToFileID(in_guidCacheID);
		return pIndexable->AddSource(in_srcID, in_pluginID, pszFileName, fileID);
	}
	return AK_Fail;
}

AKRESULT TrackProxyLocal::AddPluginSource( 
		AkUniqueID	in_srcID
		)
{
	CAkMusicTrack* pIndexable = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		return pIndexable->AddPluginSource( in_srcID );
	}
	return AK_Fail;
}

void TrackProxyLocal::SetPlayList(
		AkUInt32		in_uNumPlaylistItem,
		AkTrackSrcInfo* in_pArrayPlaylistItems,
		AkUInt32		in_uNumSubTrack
		)
{
	CAkMusicTrack* pIndexable = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetPlayList( in_uNumPlaylistItem, in_pArrayPlaylistItems, in_uNumSubTrack );
	}
}

void TrackProxyLocal::AddClipAutomation(
	AkUInt32				in_uClipIndex,
	AkClipAutomationType	in_eAutomationType,
	AkRTPCGraphPoint		in_arPoints[], 
	AkUInt32				in_uNumPoints 
	)
{
	CAkMusicTrack* pIndexable = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->AddClipAutomation( in_uClipIndex, in_eAutomationType, in_arPoints, in_uNumPoints );
	}
}

void TrackProxyLocal::SetSwitchGroup( 
	AkUInt32	in_ulGroup, 
	AkGroupType	in_eGroupType 
	)
{
	CAkMusicTrack* pIndexable = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetSwitchGroup( in_ulGroup, in_eGroupType );
	}
}

void TrackProxyLocal::SetDefaultSwitch( 
	 AkUInt32	in_ulSwitch
	 )
{
	CAkMusicTrack* pIndexable = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetDefaultSwitch( in_ulSwitch );
	}
}

void TrackProxyLocal::SetSwitchAssoc( 
	AkUInt32	in_ulNumAssoc,
	AkUInt32*	in_pulAssoc
	)
{
	CAkMusicTrack* pIndexable = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetSwitchAssoc( in_ulNumAssoc, in_pulAssoc );
	}
}

void TrackProxyLocal::RemoveAllSources()
{
	CAkMusicTrack* pIndexable = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->RemoveAllSources();
	}
}

void TrackProxyLocal::IsStreaming( bool /*in_bIsStreaming*/ )
{
	CAkMusicTrack* pIndexable = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pIndexable )
	{
		//pIndexable->IsStreaming(in_bIsStreaming);
	}
}

void TrackProxyLocal::IsZeroLatency( bool in_bIsZeroLatency )
{
	CAkMusicTrack* pIndexable = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->IsZeroLatency(in_bIsZeroLatency);
	}
}

void TrackProxyLocal::LookAheadTime( AkTimeMs in_LookAheadTime )
{
	CAkMusicTrack* pIndexable = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->LookAheadTime( in_LookAheadTime );
	}
}

void TrackProxyLocal::SetMusicTrackType( AkUInt32 /*AkMusicTrackType*/ in_eType )
{
	CAkMusicTrack* pIndexable = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetMusicTrackType( (AkMusicTrackType) in_eType );
	}
}

void TrackProxyLocal::SetEnvelope( AkUniqueID in_sourceID, AkFileParser::EnvelopePoint * in_pEnvelope, AkUInt32 in_uNumPoints, AkReal32 in_fMaxEnvValue )
{
	CAkMusicTrack* pIndexable = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		AK::WWISESOUNDENGINE_DLL::GlobalAnalysisSet::iterator it = AK::WWISESOUNDENGINE_DLL::g_setAnalysis.find( in_sourceID );
		if ( it != AK::WWISESOUNDENGINE_DLL::g_setAnalysis.end() )
		{
			// Already there. Update and notify sources.
			(*it).second.SetEnvelope(
				in_pEnvelope, 
				in_uNumPoints, 
				in_fMaxEnvValue );
			(*it).second.NotifyObservers();
		}
		else
		{
			// New envelope. 
			AK::WWISESOUNDENGINE_DLL::g_setAnalysis.emplace(
				std::piecewise_construct,
				std::forward_as_tuple(in_sourceID),
				std::forward_as_tuple(in_pEnvelope, in_uNumPoints, in_fMaxEnvValue));
		}
	}
}

void TrackProxyLocal::SetChannelConfigOverride(AkUniqueID in_sourceID, AkInt64 in_iVal)
{
	CAkMusicTrack* pIndexable = static_cast<CAkMusicTrack*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;

		AK::WWISESOUNDENGINE_DLL::g_setAnalysis[in_sourceID].SetChannelConfigOverride(in_iVal);
	}
}

void TrackProxyLocal::SetOverride( AkUInt8 in_eOverride, AkUInt8 in_uValue )
{
	CAkMusicTrack* pMusicTrack = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pMusicTrack )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pMusicTrack->SetOverride((AkMusicOverride)in_eOverride, in_uValue != 0);
	}
}

void TrackProxyLocal::SetFlag( AkUInt8 in_eFlag, AkUInt8 in_uValue )
{
	CAkMusicTrack* pMusicTrack = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pMusicTrack )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pMusicTrack->SetFlag((AkMusicFlag)in_eFlag, in_uValue != 0);
	}
}

void TrackProxyLocal::LoadMidiMedia( const AkOSChar* in_pszSourceFile, AkUniqueID in_uMidiId, AkUInt32 in_uHash )
{
	// Check if media isn't already loaded
	RefMediaVector::iterator it = m_vecRefedMedia.begin();
	for( ; it != m_vecRefedMedia.end(); ++it )
	{
		if( *it == in_uMidiId )
			return;
	}

	// Load the file
	AK::SoundEngine::LoadMediaFileSync( in_uMidiId, in_pszSourceFile, AK::SoundEngine::LoadMediaFile_Load );

	// Add to vector
	m_vecRefedMedia.push_back( in_uMidiId );
}

void TrackProxyLocal::ReleaseAllMidiMedia()
{
	RefMediaVector::iterator it = m_vecRefedMedia.begin();
	for( ; it != m_vecRefedMedia.end(); ++it )
	{
		AK::SoundEngine::LoadMediaFileSync( *it, (AkOSChar*)NULL, AK::SoundEngine::LoadMediaFile_Unload );
	}
	m_vecRefedMedia.clear();
}
void TrackProxyLocal::SetTransRule( AkWwiseMusicTrackTransRule& in_transRule )
{
	CAkMusicTrack* pIndexable = static_cast<CAkMusicTrack*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetTransRule( in_transRule );
	}
}

#endif
#endif // #ifndef AK_OPTIMIZED
