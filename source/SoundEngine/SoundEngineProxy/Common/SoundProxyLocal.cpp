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

#include "SoundProxyLocal.h"
#include "AkSound.h"
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


SoundProxyLocal::SoundProxyLocal( AkUniqueID in_id )
{
	CAkIndexable* pIndexable = AK::SoundEngine::GetIndexable( in_id, AkIdxType_AudioNode );

	SetIndexable( pIndexable != NULL ? pIndexable : CAkSound::Create( in_id ) );
}

SoundProxyLocal::~SoundProxyLocal()
{
}

void SoundProxyLocal::SetSource(AkUniqueID in_sourceID, AkUInt32 in_PluginID, const AkOSChar* in_szFileName, const struct CacheID & in_guidCacheID)
{
	CAkSound* pIndexable = static_cast<CAkSound*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		AkFileID fileID = AK::WWISESOUNDENGINE_DLL::CacheIDToFileID(in_guidCacheID);
		pIndexable->SetSource(in_sourceID, in_PluginID, in_szFileName, fileID);
	}
}

void SoundProxyLocal::SetSource( AkUniqueID in_sourceID )
{
	CAkSound* pIndexable = static_cast<CAkSound*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetSource( in_sourceID );
	}
}

void SoundProxyLocal::IsZeroLatency( bool in_bIsZeroLatency )
{
	CAkSound* pIndexable = static_cast<CAkSound*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->IsZeroLatency(in_bIsZeroLatency);
	}
}

void SoundProxyLocal::SetNonCachable( bool in_bNonCachable )
{
	CAkSound* pIndexable = static_cast<CAkSound*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetNonCachable(in_bNonCachable);
	}
}

void SoundProxyLocal::SetEnvelope( AkUniqueID in_sourceID, AkFileParser::EnvelopePoint * in_pEnvelope, AkUInt32 in_uNumPoints, AkReal32 in_fMaxEnvValue )
{
	CAkSound* pIndexable = static_cast<CAkSound*>( GetIndexable() );
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
				in_fMaxEnvValue);
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

void SoundProxyLocal::SetChannelConfigOverride(AkUniqueID in_sourceID, AkInt64 in_iVal)
{
	CAkSound* pIndexable = static_cast<CAkSound*>(GetIndexable());
	if (pIndexable)
	{
		CAkFunctionCritical SpaceSetAsCritical;

		AK::WWISESOUNDENGINE_DLL::g_setAnalysis[in_sourceID].SetChannelConfigOverride(in_iVal);
	}
}

#endif
#endif // #ifndef AK_OPTIMIZED
