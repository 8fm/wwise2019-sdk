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
// AkMusicTrack.h
//
// Class for music track node.
// The music track is not a music node. It cannot be played, and does
// not implement a context creation method. However it is an active
// parent because it has children sounds, and propagates notifications
// to them. Only MusicSegment can be a parent of MusicTrack.
//
//////////////////////////////////////////////////////////////////////
#ifndef _MUSIC_TRACK_H_
#define _MUSIC_TRACK_H_

#include "AkSoundBase.h"
#include "AkMusicStructs.h"
#include "AkSource.h"
#include "AkMidiClipCtx.h"

// -------------------------------------------------------------------
// Values that may be needed for lookahead calculations
// -------------------------------------------------------------------

// Look-ahead time boost for AAC on iOS.
// AAC sources have a very long setup time on iOS, that has nothing to do with streaming.
// Let's increase it without the user's consent (AAC is not recommended with interactive music).
// Note that as opposed to the XMA compensation, this is just a look-ahead time boost and not a compensation;
// we want playback to remain synchronized the way it is right now.
#ifdef AK_IOS
#define MUSIC_TRACK_AAC_LOOK_AHEAD_TIME_BOOST	((AkUInt32)((AkReal32)AkAudioLibSettings::g_pipelineCoreFrequency * 400.f / 1000.f))	// 400 ms
#endif

// -------------------------------------------------------------------
// Structures.
// -------------------------------------------------------------------
struct AkTrackSrc
{
	AkUInt32	uSubTrackIndex;		// Subtrack index.
	AkUniqueID	srcID;				// Source ID.
	AkUniqueID	eventID;			// Event ID.
    AkUInt32    uClipStartPosition;	// Clip start position, relative to beginning of the track.
    AkUInt32    uClipDuration;		// Clip duration.
	AkUInt32    uSrcDuration;		// Source's original duration.
    AkInt32     iSourceTrimOffset;	// Source offset at the beginning of the clip (positive, < OriginalSourceDuration).
};

// Clip automation objects, stored in an array in tracks.
class CAkClipAutomation
{
public:
	// IMPORTANT: Do NOT free conversion table in destructor. CAkClipAutomation objects are stored in an 
	// array, which moves items efficiently by performing shallow copy. Users are responsible to free the 
	// table (with UnsetCurve()) when removing/destroying this object.
	CAkClipAutomation() {}
	CAkClipAutomation( const CAkClipAutomation & in_other )
	{
		// Shallow copy.
		m_uClipIndex		= in_other.m_uClipIndex;
		m_eAutoType			= in_other.m_eAutoType;
		m_tableAutomation	= in_other.m_tableAutomation;
	}
	~CAkClipAutomation() {}

	AKRESULT Set( 
		AkUInt32				in_uClipIndex,
		AkClipAutomationType	in_eAutoType,
		AkRTPCGraphPoint		in_arPoints[], 
		AkUInt32				in_uNumPoints 
		)
	{
		ClearCurve();

		m_uClipIndex	= in_uClipIndex;
		m_eAutoType		= in_eAutoType;

		if ( in_uNumPoints > 0 )
		{
			AKRESULT eResult = m_tableAutomation.Set(in_arPoints, in_uNumPoints, AkCurveScaling_None);
			if (eResult == AK_Success )
			{
				// Iterate through table to convert point time (x) in samples.
				for ( AkUInt32 uPoint = 0; uPoint < m_tableAutomation.Size(); uPoint++ )
					m_tableAutomation.Point(uPoint).From = (AkReal32)AkTimeConv::SecondsToSamples( m_tableAutomation.Point(uPoint).From );
				m_tableAutomation.RecomputeCoefficients();
			}
			return eResult;
		}
		return AK_Success;
	}

	inline void ClearCurve()
	{
		m_tableAutomation.Unset();
	}

	// Get the automation value at the given time.
	inline AkReal32 GetValue( 
		AkInt32 iTime 	// Current time relative to clip start.
		)
	{
		AKASSERT(IsValid());
		return m_tableAutomation.Convert( (AkReal32)iTime );
	}

	inline AkUInt32	ClipIndex() const { return m_uClipIndex; }
	inline AkClipAutomationType	Type() const { return m_eAutoType; }
	inline bool IsValid() const {
		return (m_tableAutomation.IsInitialized());
	}

protected:
	
	// Key.
	AkUInt32				m_uClipIndex;
	AkClipAutomationType	m_eAutoType;

	// Automation table.
	CAkConversionTable m_tableAutomation;
};

// -------------------------------------------------------------------
// Extension to the Source object containing music specific information
// -------------------------------------------------------------------
class CAkMusicSource : public CAkSource
{
public:

    CAkMusicSource()
        :m_uStreamingLookAhead(0)
    {
    }

    inline AkUInt32 StreamingLookAhead()
    {
        return m_uStreamingLookAhead;
    }

    void StreamingLookAhead( AkUInt32 in_uStmLookAhead )
    {
		// FIXME Fix offline.
		if( ! IsMidi() )
			m_uStreamingLookAhead = in_uStmLookAhead;
    }

private:
    AkUInt32				m_uStreamingLookAhead;	// Streaming look-ahead (in samples, at the native frame rate).
};

// -------------------------------------------------------------------
// Name: TrackSwitchInfo
// Desc: Portion of track node used only for a switch track.
// -------------------------------------------------------------------
struct TrackSwitchInfo
{
public:
	TrackSwitchInfo()
		: m_uGroupID( AK_INVALID_UNIQUE_ID )
		, m_uDefaultSwitch( AK_INVALID_UNIQUE_ID )
		, m_eGroupType( AkGroupType_Switch )
	{}

	~TrackSwitchInfo() { m_arSwitchAssoc.Term(); }

	typedef AkArray<AkUInt32, const AkUInt32&, ArrayPoolDefault> TrackSwitchAssoc;

	inline const TrackSwitchAssoc& GetSwitchAssoc() const { return m_arSwitchAssoc; }
	inline AkUInt32 GetSwitchGroup() const { return m_uGroupID; }
	inline AkGroupType GetGroupType() const { return m_eGroupType; }
	inline AkUInt32 GetDefaultSwitch() const { return m_uDefaultSwitch; }
	inline const AkMusicTrackTransRule& GetTransRule() const { return m_transRule; }

	inline void SetSwitchGroup( AkUInt32 in_uSwitchGroup ) { m_uGroupID = in_uSwitchGroup; }
	inline void SetGroupType( AkGroupType in_eGroupType ) { m_eGroupType = in_eGroupType; }
	inline void SetDefaultSwitch( AkUInt32 in_uSwitch ) { m_uDefaultSwitch = in_uSwitch; }
	AKRESULT SetSwitchAssoc( AkUInt32 in_uNumAssoc, AkUInt32* in_puAssoc );
	AKRESULT SetTransRule( AkWwiseMusicTrackTransRule& in_transRule );

	AKRESULT SetSwitchParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize );
	AKRESULT SetTransParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize );

private:
	AkUInt32				m_uGroupID;			// May either be a state group or a switch group
	AkUInt32				m_uDefaultSwitch;	// Default value, to be used if none is available, 
	AkGroupType				m_eGroupType;		// Is it binded to state or to switches

	TrackSwitchAssoc		m_arSwitchAssoc;
	AkMusicTrackTransRule	m_transRule;
};

// -------------------------------------------------------------------
// Name: CAkMusicTrack
// Desc: Track audio node.
// -------------------------------------------------------------------
class CAkMusicTrack : public CAkSoundBase
{
public:
    // Thread safe version of the constructor.
	static CAkMusicTrack * Create(
        AkUniqueID in_ulID = 0
        );

	AKRESULT SetInitialValues( AkUInt8* pData, AkUInt32 ulDataSize, CAkUsageSlot* in_pUsageSlot, bool in_bIsPartialLoadOnly );

	// Return the node category.
	virtual AkNodeCategory NodeCategory();

    // Play the specified node
    // NOT IMPLEMENTED.
    //
    // Return - AKRESULT - Ak_Success if succeeded
	virtual AKRESULT PlayInternal( AkPBIParams& in_rPBIParams );

	virtual void ExecuteAction( ActionParams& in_rAction );
	virtual void ExecuteActionNoPropagate(ActionParams& in_rAction);
	virtual void ExecuteActionExcept( ActionParamsExcept& in_rAction );

    // Wwise specific interface.
    // -----------------------------------------
	void SetOverride( AkMusicOverride in_eOverride, bool in_bValue );
	void SetFlag( AkMusicFlag in_eFlag, bool in_bValue );

	AKRESULT AddPlaylistItem(
		AkTrackSrcInfo &in_srcInfo
		);

	AKRESULT SetPlayList(
		AkUInt32		in_uNumPlaylistItem,
		AkTrackSrcInfo* in_pArrayPlaylistItems,
		AkUInt32		in_uNumSubTrack
		);

	AKRESULT AddClipAutomation(
		AkUInt32				in_uClipIndex,
		AkClipAutomationType	in_eAutomationType,
		AkRTPCGraphPoint		in_arPoints[], 
		AkUInt32				in_uNumPoints 
		);

	AKRESULT SetSwitchGroup(
		AkUInt32	in_ulGroup,
		AkGroupType	in_eGroupType
		);

	AKRESULT SetDefaultSwitch(
		AkUInt32	in_ulSwitch
		);

	AKRESULT SetSwitchAssoc(
		AkUInt32	in_ulNumAssoc,
		AkUInt32*	in_pulAssoc
		);

	AKRESULT SetTransRule(
		AkWwiseMusicTrackTransRule&	in_transRule
		);

    AKRESULT AddSource(
		AkUniqueID      in_srcID,
		AkPluginID      in_pluginID,
        const AkOSChar* in_pszFilename,
		AkFileID		in_uCacheID
        );

	AKRESULT AddSource( 
		AkUniqueID in_srcID, 
		AkUInt32 in_pluginID, 
		AkMediaInformation in_MediaInfo
		);

	AKRESULT AddPluginSource( 
		AkUniqueID	in_srcID
		);

	bool HasBankSource();

	bool SourceLoaded(){ return !m_arSrcInfo.IsEmpty(); }

	// WAL-only 
    void RemoveAllSources();

	void IsZeroLatency( bool in_bIsZeroLatency );

	void SetNonCachable( bool in_bNonCachable );

	void LookAheadTime( AkTimeMs in_LookAheadTime );

	virtual AkObjectCategory Category();

	// Like ParameterNodeBase's, but does not check parent.
	bool GetStateSyncTypes( AkStateGroupID in_stateGroupID, CAkStateSyncArray* io_pSyncTypes );

    // Interface for Contexts
    // ----------------------

	CAkMusicSource* GetSourcePtr( AkUniqueID in_SourceID );

	// Note: AkTrackSrcInfo is the internal representation of audio source positions in tracks: in samples.
    // Loop count is stored in the Sound object.
	typedef AkArray<AkTrackSrc, const AkTrackSrc&, ArrayPoolDefault> TrackPlaylist;
	const TrackPlaylist & GetTrackPlaylist(){ return m_arTrackPlaylist; }

	AKRESULT SetMusicTrackType( AkMusicTrackType in_eType );
	inline AkMusicTrackType GetMusicTrackType() const { return m_eTrackType; }
	inline AkUInt32 GetNumSubTracks() const { return m_uNumSubTrack; }
	AkUInt16 GetNextRS();

	CAkClipAutomation * GetClipAutomation(
		AkUInt32				in_uClipIndex,
		AkClipAutomationType	in_eAutomationType
		)
	{
		ClipAutomationArray::Iterator it = m_arClipAutomation.FindByKey( in_uClipIndex, in_eAutomationType );
		if ( it != m_arClipAutomation.End() )
			return &(*it);
		return NULL;
	}		

	void GetMidiTargetNode( bool& r_bOverrideParent, AkUniqueID& r_uMidiTargetId, bool & r_bIsMidiTargetBus );
	void GetMidiTempoSource( bool& r_bOverrideParent, AkMidiTempoSource& r_eTempoSource );

	inline const TrackSwitchInfo* GetTrackSwitchInfo() const { AKASSERT( m_pSwitchInfo ); return m_pSwitchInfo; }

	AkUInt32 GetDefaultSwitch();

	inline const AkMusicTrackTransRule& GetTransRule() const { AKASSERT( m_pSwitchInfo ); return m_pSwitchInfo->GetTransRule(); }

	virtual AKRESULT PrepareData();
	virtual void UnPrepareData();

	virtual void GatherSounds(AkSoundArray& io_aActiveSounds, AkSoundArray& io_aInactiveSounds, AkGameSyncArray& io_aGameSyncs, bool in_bIsActive, CAkRegisteredObj* in_pGameObj, AkUInt32 in_uUpdateGameSync, AkUInt32 in_uNewGameSyncValue);

	// Functions used by segment/track ctx during playback
	AkInt32 ComputeMinSrcLookAhead( AkInt32 in_iPosition );	// Segment position, relative to pre-entry.

	// Notify the children PBI that a mute variation occurred
	virtual void MuteNotification(
		AkReal32		in_fMuteRatio,		// New muting ratio
		AkMutedMapItem& in_rMutedItem,		// Instigator's unique key
		bool			in_bIsFromBus = false
		);

	// Notify the children PBI that a mute variation occurred
	virtual void MuteNotification(
		AkReal32 in_fMuteRatio,				// New muting ratio
		CAkRegisteredObj *	in_pGameObj,	// Target Game Object
		AkMutedMapItem& in_rMutedItem,		// Instigator's unique key
		bool in_bPrioritizeGameObjectSpecificItems
		);


	void AddActiveMidiClip(CAkMidiClipCtx* in_pClip)		
	{
		AKASSERT(m_ActiveMidiClips.FindEx(in_pClip) == m_ActiveMidiClips.End());
		m_ActiveMidiClips.AddFirst(in_pClip);
	}
	
	void RemoveActiveMidiClip(CAkMidiClipCtx* in_pClip)	
	{
		m_ActiveMidiClips.Remove(in_pClip);
	}

protected:
    CAkMusicTrack( 
        AkUniqueID in_ulID
        );
    virtual ~CAkMusicTrack();

    AKRESULT Init() { return CAkSoundBase::Init(); }

	// Function used to load from soundbanks.
	AKRESULT SetSwitchParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize );
	AKRESULT SetTransParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize );

	// Helper: remove all sources without checking if the play count is above 0.
	void RemoveAllSourcesNoCheck();

    // Array of source descriptors.
    typedef CAkKeyArray<AkUniqueID, CAkMusicSource*> SrcInfoArray;
    SrcInfoArray    m_arSrcInfo;

	// Array of clip automation.
	class ClipAutomationArray : public AkArray<CAkClipAutomation, const CAkClipAutomation&,ArrayPoolDefault>
	{
	public:
		Iterator FindByKey( AkUInt32 in_uClipIndex, AkClipAutomationType in_eAutomationType )
		{
			Iterator it = Begin();
			while ( it != End() )
			{
				if ( (*it).ClipIndex() == in_uClipIndex && (*it).Type() == in_eAutomationType )
					break;
				++it;
			}
			return it;
		}

		// Override Term(): Free curve tables before destroying (see note in CAkClipAutomation).
		void Term()
		{
			if ( m_pItems )
			{
				for ( Iterator it = Begin(), itEnd = End(); it != itEnd; ++it )
				{
					(*it).ClearCurve();
					(*it).~CAkClipAutomation();
				}
				ArrayPoolDefault::Free( m_pItems );
				m_pItems = 0;
				m_uLength = 0;
				m_ulReserved = 0;
			}
		}
	protected:
		void RemoveAll();	// Undefined.
	};
	ClipAutomationArray m_arClipAutomation;

	AkUInt32		m_uNumSubTrack;

	// Classified by index of the sub track
	
	TrackPlaylist			m_arTrackPlaylist;
	AkInt32					m_iLookAheadTime;
	AkMusicTrackType		m_eTrackType;
	AkUInt16				m_SequenceIndex;

	TrackSwitchInfo*		m_pSwitchInfo;

	AkUInt8			m_bOverrideParentMidiTarget :1;
	AkUInt8			m_bOverrideParentMidiTempo :1;

	AkUInt8			m_bMidiTargetTypeBus :1;

	typedef	AkListBareLight<CAkMidiClipCtx> MidiClipList;
	MidiClipList m_ActiveMidiClips;
};

#endif
