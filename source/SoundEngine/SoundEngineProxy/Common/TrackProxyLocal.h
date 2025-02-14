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
#pragma once

#ifndef AK_OPTIMIZED
#ifndef PROXYCENTRAL_CONNECTED

#include "ParameterNodeProxyLocal.h"
#include "ITrackProxy.h"

#include <vector>
#include <map>

class CAkMusicTrack;

class TrackProxyLocal : public ParameterNodeProxyLocal
						, virtual public ITrackProxy
{
public:
	TrackProxyLocal( AkUniqueID in_id );
	virtual ~TrackProxyLocal();

	// ITrackProxy members
	virtual AKRESULT AddSource(
		AkUniqueID      in_srcID,
        AkPluginID      in_pluginID,
		const AkOSChar* in_pszFilename,
		const struct CacheID &	in_guidCacheID
        );

	virtual AKRESULT AddPluginSource( 
		AkUniqueID	in_srcID 
		);

	virtual void SetPlayList(
		AkUInt32		in_uNumPlaylistItem,
		AkTrackSrcInfo* in_pArrayPlaylistItems,
		AkUInt32		in_uNumSubTrack
		);

	virtual void AddClipAutomation(
		AkUInt32				in_uClipIndex,
		AkClipAutomationType	in_eAutomationType,
		AkRTPCGraphPoint		in_arPoints[], 
		AkUInt32				in_uNumPoints 
		);

	virtual void SetSwitchGroup( 
		AkUInt32	in_ulGroup, 
		AkGroupType	in_eGroupType 
		);

	virtual void SetDefaultSwitch( 
		AkUInt32	in_ulSwitch
		);

	virtual void SetSwitchAssoc( 
		AkUInt32	in_ulNumAssoc,
		AkUInt32*	in_pulAssoc
		);

	virtual void SetTransRule(
		AkWwiseMusicTrackTransRule&	in_transRule
		);

    virtual void RemoveAllSources();

	virtual void IsStreaming( bool in_bIsStreaming );

	virtual void IsZeroLatency( bool in_bIsZeroLatency );

	virtual void LookAheadTime( AkTimeMs in_LookAheadTime );

	virtual void SetMusicTrackType( AkUInt32 /*AkMusicTrackType*/ in_eRSType );

	virtual void SetEnvelope( AkUniqueID in_sourceID, AkFileParser::EnvelopePoint * in_pEnvelope, AkUInt32 in_uNumPoints, AkReal32 in_fMaxEnvValue );
	virtual void SetChannelConfigOverride(AkUniqueID in_sourceID, AkInt64 in_iVal);

	virtual void LoadMidiMedia( const AkOSChar* in_pszSourceFile, AkUniqueID in_uMidiId, AkUInt32 in_uHash );
	virtual void ReleaseAllMidiMedia();

	virtual void SetOverride( AkUInt8 in_eOverride, AkUInt8 in_uValue );
	virtual void SetFlag( AkUInt8 in_eFlag, AkUInt8 in_uValue );

private:
	typedef std::vector< AkUniqueID > RefMediaVector;
	RefMediaVector m_vecRefedMedia;
};

#endif
#endif // #ifndef AK_OPTIMIZED
