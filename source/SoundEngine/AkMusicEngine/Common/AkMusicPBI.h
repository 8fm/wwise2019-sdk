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
// AkMusicPBI.h
//
// Enhances the PBI by keeping a pointer to a parent (music) context. 
// Removes itself from parent in destructor.
// Also, handles transitions of a multi-level context hierarchy:
// checks if it is registered to a transition of one of its ascendant
// before creating one (a extra step before PBI::_Play/_Stop).
//
//////////////////////////////////////////////////////////////////////
#ifndef _MUSIC_PBI_H_
#define _MUSIC_PBI_H_

#include "AkPBI.h"
#include "AkMusicCtx.h"

class CAkSubTrackCtx;
struct AkTrackSrc;

// IMPORTANT: CAkPBI must be the first base class specified, because the Upper Renderer AkDestroys
// them through a pointer to a CAkPBI.
class CAkMusicPBI : public CAkPBI,
                    public CAkChildCtx
{

    friend class CAkMusicRenderer;

public:

    virtual ~CAkMusicPBI();

	virtual AKRESULT Init();

	virtual void Term( bool in_bFailedToInit );

	//Seeking
	// Disabled on Music PBI. Seeking is always performed by Segment Ctx.
	virtual void SeekTimeAbsolute( AkTimeMs in_iPosition, bool in_bSnapToMarker );
	virtual void SeekPercent( AkReal32 in_fPercent, bool in_bSnapToMarker );

	// Stop the PBI (the PBI is then destroyed)
	virtual void _Stop( AkUInt32 in_uStopOffset );

	// Apply an automation value of a given type.
	void SetAutomationValue( AkClipAutomationType in_eAutomationType, AkReal32 in_fValue );

	void FixStartTimeForFadeIn();
	inline void SetFadeOut() { m_bNeedsFadeOut = true; }

	virtual AkRTPCBitArray GetTargetedParamsSet() { return AkRTPCBitArray( RTPC_MUSIC_PBI_PARAMS_BITFIELD ); }

protected:

    // Child context implementation.
    // ------------------------------------------

	// Called at the end of each audio frame. Clean up is carried ob here, including the call to OnStopped() 
	// if this was this context's last frame.
	virtual void OnFrameEnd() {}

	// Notify context that this will be the last processing frame, propagated from a high-level context Stop().
	// Stop offset is set on the PBI. Lower engine will stop it in_uNumSamples after the beginning of the audio frame.
	virtual void OnLastFrame( 
		AkUInt32 in_uNumSamples			// Number of samples left to process before stopping. 
        );								// 0 if immediate, AK_NO_IN_BUFFER_STOP_REQUESTED if it should correspond to an audio frame, whatever its size (mininum transition time).
	
	// Pause context playback, propagated from a high-level context _Pause().
	virtual void OnPaused();

	// Resume context playback, propagated from a high-level context _Resume().
	virtual void OnResumed();

    // Fade management. Set fade level ratios.
    virtual void SetPBIFade( 
        void * in_pOwner,
        AkReal32 in_fFadeRatio
        );

	// Broadcast generic property change to child contexts.
	virtual void OnPropertyChange(
		AkPropID in_eProp,		// Property.
		AkReal32 in_fValue		// New value.
		);

	// Broadcast trigger modulators request to child contexts.
	virtual void OnTriggerModulators() {}

	// Special refcounting methods for parent contexts. After VirtualAddRef(), a child context must not be destroyed
	// until VirtualRelease() is called.
	virtual void VirtualAddRef();
	virtual void VirtualRelease();

#ifndef AK_OPTIMIZED
	virtual void OnEditDirty();
#endif

    // Overriden methods from PBIs.
    // ---------------------------------------------

	// Override PBI _stop: skip command when posted because sound has not already started playing.
	// Interactive music sounds need to play even if they did not start playing (WG-19938).
	virtual void _Stop( AkPBIStopMode in_eStopMode = AkPBIStopMode_Normal, bool in_bHasNotStarted = false );

    //Must be overriden to force interactive music to ignore pitch from busses
	virtual void RefreshParameters(AkInitialSoundParams* in_pInitialParams);

public:
	virtual AkUInt32 GetAndClearStopOffset()
	{
		/// REVIEW Promote to base class, un-virtualize
		AkUInt32 ulStopOffset = m_ulStopOffset;
		m_ulStopOffset = AK_NO_IN_BUFFER_STOP_REQUESTED;
		return ( !m_bNeedsFadeOut ) ? ulStopOffset : AK_NO_IN_BUFFER_STOP_REQUESTED;
	}

public:
	const AkTrackSrc * GetSrcInfo() const { return m_pSrcInfo; }

#ifndef AK_OPTIMIZED
	inline void MonitorPaused() { Monitor(AkMonitorData::NotificationReason_Paused); }
#endif

private:
	inline CAkSubTrackCtx* SubTrackCtx();

    // Only the Music Renderer can create/_Play music PBIs.

	// Overridden from base PBI.
	virtual AkCtxVirtualHandlingResult NotifyVirtualOff( AkVirtualQueueBehavior in_eBehavior );
    
    CAkMusicPBI(
		AkPBIParams&		in_params,
        CAkMusicCtx *		in_parent,
        CAkSoundBase*		in_pSound,			// Pointer to the sound.
		CAkSource*			in_pSource,			// Pointer to the source.
		const AkTrackSrc *	in_pSrcInfo,		// Pointer to track's source playlist item.
		PriorityInfoCurrent	in_Priority,
		AkUInt32			in_uSourceOffset,
		AkReal32			in_fSpeed			// Playback speed.
        );

	void SetMuteMapEntry(void * in_pOwner, AkReal32 in_fRatio);

protected:
	const AkTrackSrc *	m_pSrcInfo;		// Pointer to track's source playlist item.

	AkLPFType			m_LPFAutomationOffset;		// LPF Automation. NOTE: currently it is the only non-RTPC'd offset to LPF. Convert to a "mute map" if required (update in CalculateEffectiveLPF).
	AkLPFType			m_HPFAutomationOffset;		// LPF Automation. NOTE: currently it is the only non-RTPC'd offset to LPF. Convert to a "mute map" if required (update in CalculateEffectiveLPF).

#ifdef _DEBUG
	AkUInt32			m_uRefCount;	// Debug variable, to ensure that this is not destroyed while VirtualAddRef() was called.
#endif
	AkUInt32			m_bNeedsFadeOut :1;	// True if intra-frame stopping should be bypassed with fade out instead.
};

#endif
