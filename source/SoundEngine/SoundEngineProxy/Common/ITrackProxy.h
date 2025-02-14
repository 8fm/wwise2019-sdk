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

#include "IParameterNodeProxy.h"
#include "AkMusicStructs.h"

struct AkTrackSrcInfo;

class ITrackProxy : virtual public IParameterNodeProxy
{
	DECLARE_BASECLASS( IParameterNodeProxy );
public:

	virtual AKRESULT AddSource(
		AkUniqueID      in_srcID,
        AkPluginID      in_pluginID,
		const AkOSChar* in_pszFilename,
		const struct CacheID &	in_guidCacheID
        ) = 0;

	virtual AKRESULT AddPluginSource( 
		AkUniqueID	in_srcID 
		) = 0;

	virtual void SetPlayList(
		AkUInt32		in_uNumPlaylistItem,
		AkTrackSrcInfo* in_pArrayPlaylistItems,
		AkUInt32		in_uNumSubTrack
		) = 0;

	virtual void AddClipAutomation(
		AkUInt32				in_uClipIndex,
		AkClipAutomationType	in_eAutomationType,
		AkRTPCGraphPoint		in_arPoints[], 
		AkUInt32				in_uNumPoints 
		) = 0;

	virtual void SetSwitchGroup( 
		AkUInt32	in_ulGroup, 
		AkGroupType	in_eGroupType 
		) = 0;

	virtual void SetDefaultSwitch( 
		AkUInt32	in_ulSwitch
		) = 0;

	virtual void SetSwitchAssoc( 
		AkUInt32	in_ulNumAssoc,
		AkUInt32*	in_pulAssoc
		) = 0;

	virtual void SetTransRule(
		AkWwiseMusicTrackTransRule&	in_transRule
		) = 0;

    virtual void RemoveAllSources() = 0;

	virtual void IsStreaming( bool in_bIsStreaming ) = 0;

	virtual void IsZeroLatency( bool in_bIsZeroLatency ) = 0;

	virtual void LookAheadTime( AkTimeMs in_LookAheadTime ) = 0;

	virtual void SetMusicTrackType( AkUInt32 /*AkMusicTrackType*/ in_eType ) = 0;

	virtual void SetEnvelope( AkUniqueID in_sourceID, AkFileParser::EnvelopePoint * in_pEnvelope, AkUInt32 in_uNumPoints, AkReal32 in_fMaxEnvValue ) = 0;
	virtual void SetChannelConfigOverride(AkUniqueID in_sourceID, AkInt64 in_iVal) = 0;

	virtual void LoadMidiMedia( const AkOSChar* in_pszSourceFile, AkUniqueID in_uMidiId, AkUInt32 in_uHash ) = 0;
	virtual void ReleaseAllMidiMedia() = 0;

	virtual void SetOverride( AkUInt8 in_eOverride, AkUInt8 in_uValue ) = 0;

	virtual void SetFlag( AkUInt8 in_eFlag, AkUInt8 in_uValue ) = 0;

	enum MethodIDs
	{
		MethodAddSource = __base::LastMethodID,
		MethodAddPluginSource,
		MethodSetPlayList,
		MethodAddClipAutomation,
		MethodRemoveAllSources,
		MethodIsStreaming,
		MethodIsZeroLatency,
		MethodSetNonCachable,
		MethodLookAheadTime,
		MethodSetMusicTrackType,
		MethodSetEnvelope,
		MethodSetChannelConfigOverride,
		MethodSetSwitchGroup,
		MethodSetDefaultSwitch,
		MethodSetSwitchAssoc,
		MethodSetTransRule,
		MethodSetOverride,
		MethodSetFlag,

		LastMethodID
	};
};

namespace TrackProxyCommandData
{
	struct SetPlayList : public ObjectProxyCommandData::CommandData
	{
		SetPlayList();
		~SetPlayList();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32		m_uNumPlaylistItem;
		AkTrackSrcInfo* m_pArrayPlaylistItems;
		AkUInt32		m_uNumSubTrack;


	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct AddClipAutomation : public ObjectProxyCommandData::CommandData
	{
		AddClipAutomation();
		~AddClipAutomation();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32				m_uClipIndex;
		AkClipAutomationType	m_eAutomationType;
		AkRTPCGraphPoint *		m_pArrayPoints;
		AkUInt32				m_uNumPoints;

	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct SetSwitchGroup : public ObjectProxyCommandData::CommandData
	{
		SetSwitchGroup();
		~SetSwitchGroup();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_ulGroup;
		AkGroupType m_eGroupType;

	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct SetDefaultSwitch : public ObjectProxyCommandData::CommandData
	{
		SetDefaultSwitch();
		~SetDefaultSwitch();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_ulSwitch;

	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct SetSwitchAssoc : public ObjectProxyCommandData::CommandData
	{
		SetSwitchAssoc();
		~SetSwitchAssoc();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_ulNumAssoc;
		AkUInt32* m_pulAssoc;

	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	struct SetTransRule : public ObjectProxyCommandData::CommandData
	{
		SetTransRule();
		~SetTransRule();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseMusicTrackTransRule m_transRule;

	private:
		DECLARE_BASECLASS( ObjectProxyCommandData::CommandData );
	};

	typedef ObjectProxyCommandData::CommandData2
	< 
		ITrackProxy::MethodSetOverride,
		AkUInt8,
		AkUInt8
	> SetOverride;

	typedef ObjectProxyCommandData::CommandData2
		< 
		ITrackProxy::MethodSetFlag,
		AkUInt8,
		AkUInt8
		> SetFlag;

	typedef ObjectProxyCommandData::CommandData1
	< 
		ITrackProxy::MethodIsStreaming,
		bool
	> IsStreaming;

	typedef ObjectProxyCommandData::CommandData1
	< 
		ITrackProxy::MethodIsZeroLatency,
		bool
	> IsZeroLatency;

	typedef ObjectProxyCommandData::CommandData1
	< 
		ITrackProxy::MethodSetNonCachable,
		bool
	> SetNonCachable;

	typedef ObjectProxyCommandData::CommandData1
	< 
		ITrackProxy::MethodLookAheadTime,
		AkTimeMs
	> LookAheadTime;

	typedef ObjectProxyCommandData::CommandData1
	< 
		ITrackProxy::MethodSetMusicTrackType,
		AkUInt32
	> SetMusicTrackType;
}

#endif // #ifndef AK_OPTIMIZED
