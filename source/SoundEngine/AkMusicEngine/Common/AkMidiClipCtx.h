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
// AkMidiClipCtx.h
//
// Manages the playout context of a midi source.
//
//////////////////////////////////////////////////////////////////////
#ifndef _MIDI_CLIP_CTX_H_
#define _MIDI_CLIP_CTX_H_

#include "AkMidiBaseCtx.h"
#include "AkChildCtx.h"
#include "AkMidiParseSe.h"
#include "AkMutedMap.h"
#include "AkRTPCSubscriber.h"

struct AkTrackSrc;
class CAkMusicTrack;
class CAkSubTrackCtx;
class CAkMidiClipMgr;
class CAkMusicSource;
class CAkUsageSlot;


class CAkMidiClipCtx	: public CAkMidiBaseCtx
						, public CAkChildCtx
						, public CAkParameterTarget
{
	friend class CAkMidiClipMgr;

public:

	virtual ~CAkMidiClipCtx();

	virtual AKRESULT Init();
	virtual AkUniqueID GetTrackID() const;
	bool IsUsingThisSlot(const CAkUsageSlot* in_pMediaSlot) const;
	bool RelocateMedia(CAkUsageSlot* in_pMediaSlot);

#ifndef AK_OPTIMIZED
	virtual void GetMonitoringMuteSoloState( bool &out_bSolo, bool &out_bMute );
#endif

	void MuteNotification(	AkReal32			in_fMuteRatio, 
		AkMutedMapItem&		in_rMutedItem,
		bool				in_bPrioritizeGameObjectSpecificItems);

#ifndef AK_OPTIMIZED
	virtual AkUniqueID GetPlayTargetID( AkUniqueID in_playTargetID ) const;
#endif

	// CAkParameterTarget overrides.
	// We register for RTPC_PlaybackSpeed update, however they are handled in CAkMusicNode, so we do not do anything here.
	virtual void UpdateTargetParam(AkRTPC_ParameterID in_eParam, AkReal32 in_fValue, AkReal32 in_fDelta) {}
	virtual void NotifyParamChanged(bool in_bLiveEdit, AkRTPC_ParameterID in_rtpcID) {}
	virtual void NotifyParamsChanged(bool in_bLiveEdit, const AkRTPCBitArray& in_bitArray) {}
	virtual AkRTPCBitArray GetTargetedParamsSet(){ return 1ull << RTPC_PlaybackSpeed; }

protected:

    CAkMidiClipCtx(
        CAkMusicCtx *		in_parent,
		CAkMidiClipMgr *	in_pMidiMgr,
		CAkMusicTrack*		in_pTrack,
		CAkMusicSource*		in_pSource,
		CAkRegisteredObj*	in_pGameObj,		// Game object and channel association.
		TransParams &		in_rTransParams,
		UserParams &		in_rUserParams,
		const AkTrackSrc*	in_pSrcInfo,		// Pointer to track's source playlist item.
		AkUInt32			in_uPlayDuration,
		AkUInt32			in_uSourceOffset,
		AkUInt32			in_uFrameOffset
        );

	virtual bool ResolveMidiTarget();
	AkReal32 GetTargetTempo();
	void GetCCEvents( MidiFrameEventList& in_listEvents, AkUInt32 in_uFrameOffset, AkUInt32 in_uEventIdx );
	
	// Override OnLastFrame: scale number of samples with current playback speed.
	AkUInt32 GetFrameOffset() const { return m_uFrameOffset; }

	// Called at the end of each audio frame. Clean up is carried ob here, including the call to OnStopped() 
	// if this was this context's last frame.
	virtual void OnFrameEnd() {}

	virtual void OnLastFrame( AkUInt32 in_uNumSamples );
	void OnFrame( MidiFrameEventList& in_listEvents, AkUInt32 in_uNumSamples );

	// Management of stop offset.
	void SetStopOffset( AkUInt32 in_ulStopOffset ){ m_uStopOffset = in_ulStopOffset; }

	// Setting WasStopped flag means the ctx is almost dead, release once now,
	// and when all active notes are done, the ctx will die peacefully.
	void SetWasStopped() { if( ! m_bWasStopped ) { m_bWasStopped = true; Release(); } }

	// Abstract functions that are of no use!
	virtual void SetPBIFade( void * /*in_pOwner*/, AkReal32 /*in_fFadeRatio*/ ) {}

	// Called by MidiMgr, if true then ctx can only generate weak events.
	inline bool IsWeak() { return m_bWasStopped; }

	// Stop context playback, propagated from a high-level context _Pause().
	virtual void _Stop( AkUInt32 in_uStopOffset );

	// Pause context playback, propagated from a high-level context _Pause().
	virtual void OnPaused();

	// Resume context playback, propagated from a high-level context _Resume().
	virtual void OnResumed();

	// Ignore fades is used for immediate stops.
	void SetAbsoluteStop( bool in_bAbsolute ) { m_bAbsoluteStop = in_bAbsolute; }
	virtual bool GetAbsoluteStop() const { return m_bAbsoluteStop; }

	// CAkChildCtx virtual functions that we don't want
	virtual void OnPropertyChange( AkPropID, AkReal32 ) {}
	virtual void OnTriggerModulators() {}

	// From CAkMidiBaseCtx
	virtual CAkMidiBaseMgr* GetMidiMgr() const;

	CAkMidiClipMgr*		m_pMidiMgr;

	CAkMusicTrack*		m_pTrack;
	CAkMusicSource*		m_pSource;
	const AkTrackSrc *	m_pSrcInfo;		// Pointer to track's source playlist item.

	CAkUsageSlot*		m_pUsageSlot;
	AkUInt8*			m_pDataPtr;
	AkUInt32			m_uDataSize;

	AkMidiParseSe		m_MidiParse;

	AkMutedMap			m_mapMutedNodes;

	AkUInt32	m_uSourceOffset;
	AkUInt32	m_uFrameOffset;
	AkUInt32	m_uStopOffset;
	AkUInt32	m_bWasPaused :1;
	AkUInt32	m_bWasStopped :1;
	AkUInt32	m_bFirstNote :1;
	AkUInt32	m_bAbsoluteStop :1;
	
protected:

	// Special refcounting methods for parent contexts. After VirtualAddRef(), a child context must not be destroyed
	// until VirtualRelease() is called.
	virtual void VirtualAddRef() { AddRef(); }
	virtual void VirtualRelease() { Release(); }

	CAkSubTrackCtx* SubTrackCtx() const;

#ifndef AK_OPTIMIZED
	virtual void OnEditDirty()
	{
		// This notification is never propagated at this level.
		AKASSERT( !"Unhandled notification" );
	}
#endif

private:
	void RefreshMusicParameters() const;
	bool CheckIsMuted();

public: 
	//for AkListBareLight to track active midi clips in AkMusicTrack.
	CAkMidiClipCtx* pNextLightItem;
};


#endif
