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
// AkMusicPBI.cpp
//
// Enhances the PBI by keeping a pointer to a parent (music) context. 
// Removes itself from parent in destructor.
// Also, handles transitions of a multi-level context hierarchy:
// checks if it is registered to a transition of one of its ascendant
// before creating one (a extra step before PBI::_Play/_Stop).
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkMusicPBI.h"
#include "AkSubTrackCtx.h"

CAkMusicPBI::CAkMusicPBI(
	AkPBIParams&		in_params,
    CAkMusicCtx*		in_parent,
    CAkSoundBase*		in_pSound,			// Pointer to the sound.
	CAkSource*			in_pSource,			// Pointer to the source.
	const AkTrackSrc *	in_pSrcInfo,		// Pointer to track's source playlist item.
	PriorityInfoCurrent	in_Priority,
	AkUInt32			in_uSourceOffset,
	AkReal32			in_fSpeed			// Playback speed.	
    )
:CAkPBI( in_params,
		 in_pSound,
		 in_pSource,
         in_Priority,
		 in_uSourceOffset
		 )
,CAkChildCtx( in_parent )
,m_pSrcInfo( in_pSrcInfo )
, m_LPFAutomationOffset(0)
, m_HPFAutomationOffset(0)
#ifdef _DEBUG
, m_uRefCount(0)
#endif
, m_bNeedsFadeOut(false)
{
	// Override pre-buffering flag set by base class.
	m_bRequiresPreBuffering = false;

	// Override pitch shift type. Music changes pitch without interpolation in order to follow playback speed tightly.
	m_ePitchShiftType = PitchShiftType_LinearConstant;

	// Override rate.
	m_fPlaybackSpeed = in_fSpeed;
}

CAkMusicPBI::~CAkMusicPBI()
{
	AKASSERTD( m_uRefCount == 0 );	// Debug variable.
}

void CAkMusicPBI::Term( bool in_bFailedToInit )
{
	if (SubTrackCtx())
	{
		// Notify sub-track ctx
		SubTrackCtx()->RemoveReferences(this);
	}

    Disconnect();

	CAkPBI::Term( in_bFailedToInit );
}

AKRESULT CAkMusicPBI::Init()
{
    AKASSERT( Parent() || !"A Music PBI HAS to have a parent" );
    
	Connect();

    return CAkPBI::Init();
}

//Seeking
// Disabled on Music PBI. Seeking is always performed by Segment Ctx.
void CAkMusicPBI::SeekTimeAbsolute( AkTimeMs, bool )
{
	AKASSERT( !"Cannot seek Music PBI directly" );
}
void CAkMusicPBI::SeekPercent( AkReal32, bool )
{
	AKASSERT( !"Cannot seek Music PBI directly" );
}

void CAkMusicPBI::_Stop( AkPBIStopMode in_eStopMode, bool in_bHasNotStarted )
{
	// Skip _Stop if a stop offset was set on a sound that started playing in the same frame.
	if ( !in_bHasNotStarted || m_ulStopOffset == AK_NO_IN_BUFFER_STOP_REQUESTED )
		CAkPBI::_Stop( in_eStopMode );
}

void CAkMusicPBI::SetAutomationValue( AkClipAutomationType in_eAutomationType, AkReal32 in_fValue )
{
	OpenSoundBrace();
	switch ( in_eAutomationType )
	{
	case AutomationType_Volume:
		// NOTE: Volume automation uses a slot in the "mute map" in order to save memory. 
		// It is handled as a dB scale in the UI, and therefore transformed using AkMath::Prescaling_ToLin_dB().
		// Here we need to use it directly as a fade ratio, so instead of relying on the automatic curve
		// conversion, m_tableAutomation.Set() uses "AkCurveScaling_None" and we simplify the math:
		// scaled_val = 20*log(prescaled_val+1)
		// fade_val = 10^(scaled_val/20) = 10^log(prescaled_val+1) = prescaled_val+1
		// IMPORTANT: Keep in sync with AkMath::Prescaling_ToLin_dB().
		in_fValue += 1.f;
		AkDeltaMonitor::LogDelta(AkDelta_MusicSegmentEnvelope, AkPropID_Volume, in_fValue, 1.0f);
		SetMuteMapEntry((AkUInt8*)this + (AkUIntPtr)in_eAutomationType, in_fValue);
		break;

	case AutomationType_FadeIn:
	case AutomationType_FadeOut:		
		AkDeltaMonitor::LogDelta(AkDelta_Fade, AkPropID_Volume, in_fValue, 1.0f);
		SetMuteMapEntry((AkUInt8*)this + (AkUIntPtr)in_eAutomationType, in_fValue);
		break;
	case AutomationType_LPF:
		AkDeltaMonitor::LogDelta(AkDelta_MusicSegmentEnvelope, AkPropID_LPF, in_fValue, 0.0f);
		m_EffectiveParams.AddNoLogLPF(in_fValue - m_LPFAutomationOffset);
		m_LPFAutomationOffset = in_fValue;		
		break;
	case AutomationType_HPF:
		AkDeltaMonitor::LogDelta(AkDelta_MusicSegmentEnvelope, AkPropID_HPF, in_fValue, 0.0f);
		m_EffectiveParams.AddNoLogHPF(in_fValue - m_HPFAutomationOffset);
		m_HPFAutomationOffset = in_fValue;		
		break;
	default:
		AKASSERT( !"Invalid automation type" );
	}
	CloseSoundBrace();
}

// Child context implementation.
// ------------------------------------------

// Notify context that this will be the last processing frame, propagated from a high-level context Stop().
// Stop offset is set on the PBI. Lower engine will stop it in_uNumSamples after the beginning of the audio frame.
void CAkMusicPBI::OnLastFrame( 
	AkUInt32 in_uNumSamples			// Number of samples left to process before stopping. 
    )
{
    // Set stop offset only if smaller than previous value.
	if ( in_uNumSamples != AK_NO_IN_BUFFER_STOP_REQUESTED )
	{
		// Scale with playback speed.
		in_uNumSamples = AkMath::Round( in_uNumSamples / m_fPlaybackSpeed );
		if ( in_uNumSamples < m_ulStopOffset )
			SetStopOffset( in_uNumSamples );
	}

	// Stop abruptly (no transition). Do not use minimum transition time if m_ulStopOffset is not 
	// AK_NO_IN_BUFFER_STOP_REQUESTED; the lower engine will stop this PBI using the stop offset.
	TransParams transParams;
	CAkPBI::_Stop( transParams, m_ulStopOffset == AK_NO_IN_BUFFER_STOP_REQUESTED || m_bNeedsFadeOut );
}

// Pause context playback, propagated from a high-level context _Pause().
void CAkMusicPBI::OnPaused()
{
    // Enqueue pause command to lower engine, through upper renderer.
    _Pause( true );
}

// Resume context playback, propagated from a high-level context _Resume().
void CAkMusicPBI::OnResumed()
{
    // Enqueue pause command to lower engine, through upper renderer.
    _Resume();

    // Note. PBIs mute their m_cPauseResumeFade in _Pause(), but do not unmute it in _Resume().
    // Force it.
    m_fPauseResumeFadeRatio = AK_UNMUTED_RATIO;
    CalculateMutedEffectiveVolume();
}

// Special refcounting methods for parent contexts. After VirtualAddRef(), a child context must not be destroyed
// until VirtualRelease() is called.
void CAkMusicPBI::VirtualAddRef()
{
	// Does nothing: We take for granted that the URenderer does not destroy us if this was called.
	// TODO: Implement true refcounting if this changes.
#ifdef _DEBUG
	// Debug variable
	++m_uRefCount;
#endif
}
void CAkMusicPBI::VirtualRelease()
{
	// Does nothing.
#ifdef _DEBUG
	// Debug variable
	AKASSERT( m_uRefCount > 0 );
	--m_uRefCount;
#endif
}

#ifndef AK_OPTIMIZED
void CAkMusicPBI::OnEditDirty()
{
	// This notification is never propagated at this level.
	AKASSERT( !"Unhandled notification" );
}
#endif


// OVERRIDEN METHODS FROM PBIs.
// ---------------------------------------------

// Stop the PBI.
void CAkMusicPBI::_Stop( 
    AkUInt32 in_uStopOffset )
{
	OnLastFrame( in_uStopOffset );
}


// Fade management. Propagate fades down to PBIs muted map
void CAkMusicPBI::SetPBIFade(void * in_pOwner, AkReal32 in_fFadeRatio)
{
	AkDeltaMonitor::LogUpdate(AkPropID_MuteRatio, in_fFadeRatio, GetSoundID(), GetPipelineID());
	SetMuteMapEntry(in_pOwner, in_fFadeRatio);
}

void CAkMusicPBI::SetMuteMapEntry(void * in_pOwner, AkReal32 in_fRatio)
{
    AkMutedMapItem mutedMapItem;
    mutedMapItem.m_bIsPersistent = true;
    mutedMapItem.m_bIsGlobal = false;
    mutedMapItem.m_Identifier = in_pOwner;
	mutedMapItem.m_eReason = AkDelta_Fade;	
	CAkPBI::SetMuteMapEntry( mutedMapItem, in_fRatio);
}

// Broadcast generic property change to child contexts.
void CAkMusicPBI::OnPropertyChange(
	AkPropID in_eProp,		// Property.
	AkReal32 in_fValue		// New value.
	)
{
	if ( in_eProp == AkPropID_PlaybackSpeed )
	{
		m_fPlaybackSpeed = in_fValue;
		m_bAreParametersValid = false;
	}
}

void CAkMusicPBI::FixStartTimeForFadeIn() 
{ 
	// Do not fix start time if the source is streaming and was not supposed to seek already. This may prevent us from using prefetch, if applicable, and in general
	// it is inefficient to seek a stream even by just a little. Let's assume that a fade-in that starts in the middle of a frame will sound acceptable.
	// REVIEW: Make this an option?
	if ( RequiresSourceSeek() 
		|| !IsPrefetched() )
	{
		AkInt32 iOldFrameOffset = GetFrameOffset();
		AKASSERT( iOldFrameOffset >= 0 );
		AkInt32 iFrameSize = ScaleFrameSize( AK_NUM_VOICE_REFILL_FRAMES );
		AkInt32 iSubFrameOffset = iOldFrameOffset % iFrameSize;

		if ( iSubFrameOffset > iFrameSize / 2 
			|| iSubFrameOffset > (AkInt32)m_uSeekPosition )
		{
			AkInt32 iFrameOffsetIncrement = iFrameSize - iSubFrameOffset;			
			SetFrameOffset( iOldFrameOffset + iFrameOffsetIncrement );
			SetNewSeekPosition( m_uSeekPosition + iFrameOffsetIncrement, false );
		}
		else if ( iSubFrameOffset > 0 )
		{
			SetFrameOffset( iOldFrameOffset - iSubFrameOffset );
			AKASSERT( (AkInt32)m_uSeekPosition >= iSubFrameOffset );
			SetNewSeekPosition( m_uSeekPosition - iSubFrameOffset, false );
		}
	}
	m_bNeedsFadeIn = true;
}

AkCtxVirtualHandlingResult CAkMusicPBI::NotifyVirtualOff( AkVirtualQueueBehavior in_eBehavior )
{
	// Interactive music only has "From Elapsed Time option". Replace this assert with an if() if it changes.
	AKASSERT( in_eBehavior == AkVirtualQueueBehavior_FromElapsedTime );

	AkInt32 iLookAheadTime;
	AkInt32 iSourceOffset;
	if ( SubTrackCtx()->GetSourceInfoForPlaybackRestart(
		this,				// Context which became physical.
		iLookAheadTime,		// Returned required frame offset (sample-accurate look-ahead time).
		iSourceOffset ) )	// Returned required source offset ("core" sample-accurate offset in source).
	{
		// Look-ahead time should be a multiple of the frame size so that volume ramps sound smooth.
		SetFrameOffset( iLookAheadTime );
		SetNewSeekPosition( iSourceOffset, false );
		return VirtualHandling_RequiresSeeking;
	}
	else
	{
		// Source cannot restart before scheduled stop time.
		return VirtualHandling_ShouldStop;
	}
}

void CAkMusicPBI::RefreshParameters(AkInitialSoundParams* in_pInitialParams)
{
	OpenSoundBrace();
	CAkPBI::RefreshParameters(in_pInitialParams);
		
	// Scale pitch to respect playback speed.
	// IMPORTANT: We can do this here, and in SetPlaybackSpeed, because there is no pitch in music
	// nodes and no playback speed in actor-mixers...
	m_EffectiveParams.SetNoLogPitch(((1200.f / LOGTWO) * log10f(m_fPlaybackSpeed)));

	//Re-apply automation
	m_EffectiveParams.AddToLPF(m_LPFAutomationOffset, AkDelta_MusicSegmentEnvelope);
	m_EffectiveParams.AddToHPF(m_HPFAutomationOffset, AkDelta_MusicSegmentEnvelope);
	CloseSoundBrace();
}

CAkSubTrackCtx* CAkMusicPBI::SubTrackCtx()
{
	AKASSERT( Parent() );
	return static_cast<CAkSubTrackCtx*>(Parent());
}
