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
// AkPBI.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _PBI_H_
#define _PBI_H_

#include "AkBehavioralCtx.h"
#include "AkMutedMap.h"
#include "AkMonitorData.h"
#include "AkRTPC.h"
#include "AkCntrHistory.h"
#include "AkSource.h"
#include "AkTransportAware.h"
#include "ITransitionable.h"
#include "AkMidiStructs.h"

class CAkBus;
class CAkSoundBase;
class CAkRegisteredObj;
class CAkTransition;
class CAkPath;
class CAkPBIAware;
class CAkLimiter;
class CAkMidiNoteState;
class CAkVPLSrcCbxNode;
struct ActionParams;

class CAkUsageSlot;

extern AkReal32 g_fVolumeThreshold;
extern AkReal32 g_fVolumeThresholdDB;

#define AK_INVALID_SEQUENCE_ID 0

enum AkCtxState
{
	CtxStateStop		= 0,
	CtxStatePause		= 1,
	CtxStateResume		= 2,
	CtxStatePlay		= 3,
	CtxStateToDestroy	= 4
#define CTXSTATE_NUM_STORAGE_BIT 4
};

enum AkCtxDestroyReason
{
	CtxDestroyReasonFinished = 0,
	CtxDestroyReasonPlayFailed
};

enum AkCtxVirtualHandlingResult
{
	VirtualHandling_NotHandled,
	VirtualHandling_RequiresSeeking,
	VirtualHandling_ShouldStop
};

enum KickFrom
{
	KickFrom_Stopped,
	KickFrom_OverNodeLimit,
	KickFrom_OverGlobalLimit
};

enum AkPitchShiftType
{
	PitchShiftType_LinearInterpolate,
	PitchShiftType_LinearConstant
};

AkForceInline bool HandleInvalidFloat(AkReal32 in_float)
{
	if (!AkMath::IsValidFloatInput(in_float))
	{
		// WG-28394
		// This assert means invalid format floating point values were detected in the system.
		// This is usually caused by invalid user input (illegal floating point values)or memory corruption.
		MONITOR_ERRORMSG(AKTEXT("Invalid Floating point value Detected : non-finite(or NaN) priority."));	
		AKASSERT(!"Invalid Floating point value Detected : non-finite(or NaN) priority.");
		return true;
	}
	return false;
}


struct AkPBIModValues
{
	AkVolumeValue	VolumeOffset;
	AkVolumeValue	MakeUpGainOffset;
	AkPitchValue	PitchOffset;
	AkLPFType		LPFOffset;
	AkLPFType		HPFOffset;

	AkPBIModValues()
		:VolumeOffset(0)
		, MakeUpGainOffset(0)
		, PitchOffset(0)
		, LPFOffset(0)
		, HPFOffset(0)
	{}
};

struct AkPriorityStruct
{	
	AkPriorityStruct() :
		priority(AK_DEFAULT_PRIORITY)
	{}

	AkForceInline AkReal32 GetPriority() const
	{
		return priority;
	} 

	AkForceInline bool operator==(const AkPriorityStruct& other)
	{
		return priority == other.priority
			&& pipelineID == other.pipelineID
			&& seqID == other.seqID;
	}

	AkForceInline bool operator!=(const AkPriorityStruct& other)
	{
		return priority != other.priority
			|| pipelineID != other.pipelineID
			|| seqID != other.seqID;
	}

	void SetPriority(AkReal32 in_priority)
	{
		if (HandleInvalidFloat(in_priority))
			return;

		priority = in_priority;
	}

private:
	AkReal32 priority;

public:
	AkPipelineID pipelineID;
	AkUniqueID seqID;
};

struct PriorityInfo
{
	PriorityInfo()
		:priority(AK_DEFAULT_PRIORITY)
		,distanceOffset(0.0f)
	{}

	AkForceInline AkReal32 GetPriority() const
	{
		return priority;
	}

	void SetPriority(AkReal32 in_priority)
	{
		if (HandleInvalidFloat(in_priority))
			return;
		
		priority = in_priority;
	}

	AkForceInline AkReal32 GetDistanceOffset() const
	{
		return distanceOffset;
	}

	void SetDistanceOffset(AkReal32 in_distanceOffset)
	{
		if (HandleInvalidFloat(in_distanceOffset))
			return;
		
		distanceOffset = in_distanceOffset;
	}

private:
	AkReal32 priority;			// Priority 0-100.
	AkReal32 distanceOffset;	// Offset to priority at max radius if m_bPriorityApplyDistFactor.
};

struct PriorityInfoCurrent
{
	PriorityInfoCurrent()
	{
		currentPriority.SetPriority(AK_DEFAULT_PRIORITY);
	}

	PriorityInfoCurrent( PriorityInfo in_PriorityInfo )
		:priorityInfo ( in_PriorityInfo )
	{
		SetCurrent(in_PriorityInfo.GetPriority());
	}

	AkForceInline AkReal32 GetCurrent() const { return currentPriority.GetPriority(); }
	AkForceInline AkReal32 BasePriority() const { return priorityInfo.GetPriority(); }
	AkForceInline AkReal32 DistanceOffset()const { return priorityInfo.GetDistanceOffset(); }
	void ResetBase( AkReal32 in_newBase )
	{
		priorityInfo.SetPriority(in_newBase);
	}
	void Reset( const PriorityInfo& in_priorityInfo )
	{
		priorityInfo = in_priorityInfo;
	}

	void SetCurrent( AkReal32 in_priority )
	{
		currentPriority.SetPriority(AkMin(AK_MAX_PRIORITY, AkMax(AK_MIN_PRIORITY, in_priority)));
	}

	void SetPipelineID( AkPipelineID in_pipelineID )
	{
		currentPriority.pipelineID = in_pipelineID;
	}

	void SetSeqID( AkUniqueID in_seqID )
	{
		currentPriority.seqID = in_seqID;
	}

	AkForceInline AkPipelineID GetPipelineID() const
	{
		return currentPriority.pipelineID;
	}

	AkForceInline AkUniqueID GetSeqID() const
	{
		return currentPriority.seqID;
	}

	AkForceInline AkPriorityStruct& GetPriorityKey()
	{
		return currentPriority;
	}

private:
	AkPriorityStruct currentPriority;
	PriorityInfo priorityInfo;
};

struct AkInitialSoundParams
{
	AkInitialSoundParams(const AkInitialSoundParams& in_copy): pParamsValidFromNode(in_copy.pParamsValidFromNode), pCtrlBusUsed(in_copy.pCtrlBusUsed)
	{
		DoCopy(in_copy);
	}

	const AkInitialSoundParams& operator= (const AkInitialSoundParams& in_copy )
	{
		if (&in_copy != this)
		{
			DoTerm();
			DoCopy(in_copy);
		}
		return *this;
	}

	AkInitialSoundParams(){	Clear();	}
	~AkInitialSoundParams() { DoTerm(); }

	void Clear()
	{	
		soundParams.ClearEx();
		mutedMap.RemoveAll();
		ranges = AkPBIModValues();
		pParamsValidFromNode = NULL;
		pCtrlBusUsed = NULL;
	}

	//Initial params passed to the PBI, to speed up calculation of multiple pbi's with common hierarchies.
	AkSoundParams			soundParams;
	AkMutedMap				mutedMap;
	AkPBIModValues			ranges;
	AkModulatorsToTrigger	modulators;

	CAkParameterNodeBase*	pParamsValidFromNode;

	CAkBus*					pCtrlBusUsed;

private:
	inline void DoTerm()
	{
		mutedMap.Term();
		modulators.Term();
	}

	inline void DoCopy(const AkInitialSoundParams& in_copy)
	{
		pParamsValidFromNode = in_copy.pParamsValidFromNode;
		pCtrlBusUsed = in_copy.pCtrlBusUsed;
		soundParams = in_copy.soundParams;
		ranges = in_copy.ranges;
		AkUInt32 uNumMutedItems = in_copy.mutedMap.Length();
		if (uNumMutedItems > 0 && mutedMap.Resize(uNumMutedItems))
		{
			for (AkUInt32 i = 0; i < uNumMutedItems; ++i)
				mutedMap[i] = in_copy.mutedMap[i];
		}
		AkUInt32 uModulatorCount = in_copy.modulators.Length();
		if (uModulatorCount > 0 && modulators.Resize(uModulatorCount))
		{
			for (AkUInt32 i = 0; i < uModulatorCount; ++i)
				modulators[i] = in_copy.modulators[i];
		}
	}
};

struct DelayContinuousParams
{
	DelayContinuousParams()
	: uNextSelectedNodeID(AK_INVALID_UNIQUE_ID)
	, bSkipDelay(false)
	{}

	AkUniqueID uNextSelectedNodeID;
	bool	bSkipDelay : 1;
};

struct AkPBIParams
{
	enum ePBIType
	{
		PBI,
		ContinuousPBI,
		DynamicSequencePBI
	};

	AkPBIParams()
		: pInstigator( NULL )
		, sequenceID( AK_INVALID_SEQUENCE_ID )
		, playTargetID( AK_INVALID_UNIQUE_ID )
		, pMidiNoteState( NULL )
		, bMidiCheckParent(true)
		, bIsFirst(true)
		, bPlayDirectly(false)
		, bNeedsFadeIn(false)
	{
		playHistory.Init();
	}
	
	AkPBIParams( PlayHistory& in_rPlayHist )
		: pInstigator( NULL )
		, playHistory(in_rPlayHist)
		, sequenceID( AK_INVALID_SEQUENCE_ID )
		, playTargetID( AK_INVALID_UNIQUE_ID )
		, pMidiNoteState( NULL )
		, bMidiCheckParent(true)
		, bIsFirst(true)
		, bPlayDirectly(false)
		, bNeedsFadeIn(false)
	{

	}

	AkForceInline const AkMidiEventEx& GetMidiEvent() const { return midiEvent; }
	AkUniqueID GetMidiTargetID() const;

	void PopulateInitialSoundParams(CAkParameterNodeBase* in_pFromNode);

#ifndef AK_OPTIMIZED
	void SetPlayTargetID( const AkUniqueID in_playTargetID )
	{
		if( playTargetID == AK_INVALID_UNIQUE_ID )
			playTargetID = in_playTargetID;
	}
#endif

	ePBIType			eType;
	CAkPBIAware*		pInstigator;
	CAkRegisteredObj*		pGameObj;
	TransParams*		pTransitionParameters;
	UserParams			userParams;
	PlayHistory			playHistory;
	AkPlaybackState		ePlaybackState;
	AkUInt32			uFrameOffset;

	// continuous specific member
	ContParams*			pContinuousParams;
	AkUniqueID			sequenceID;

	AkUniqueID			playTargetID;	// ID of original played item, which led to the PBI

	AkMidiEventEx		midiEvent;
	CAkMidiNoteState*	pMidiNoteState;
	
	AkInitialSoundParams initialSoundParams;

	DelayContinuousParams delayParams;

	bool				bMidiCheckParent : 1;
	bool				bIsFirst : 1;
	bool				bPlayDirectly : 1;
	bool				bNeedsFadeIn : 1;
};

typedef AkArray< CAkLimiter*, CAkLimiter*, AkHybridAllocator< sizeof( CAkLimiter* ) > > AkLimiters;

// class corresponding to a Playback instance
//
// Author:  alessard
class CAkPBI : public CAkTransportAware,
			   public ITransitionable,
			   public CAkBehavioralCtx,
				public CAkScopedRtpcObj

{
public:
	CAkPBI * pNextItem; // For CAkURenderer::m_listCtxs

public:

    // Constructor
	CAkPBI( AkPBIParams&				in_params,
			CAkSoundBase*				in_pSound,			// Pointer to the sound.
			CAkSource*					in_pSource,
			const PriorityInfoCurrent&	in_rPriority,
			AkUInt32					in_uSourceOffset
	);

    //Destructor
	virtual ~CAkPBI();

	virtual AKRESULT Init();
	
	AKRESULT InitAttenuation( AkPathInfo* in_pPathInfo );
	
	virtual void Term( bool in_bFailedToInit);

	void UpdateLimiters();

	virtual void TransUpdateValue(
		AkIntPtr in_eTarget,	// Transition target type
		AkReal32 in_fValue,				// New Value
		bool in_bIsTerminated			// Is it the end of the transition
		);

	virtual void SetEstimatedLength( AkReal32 in_fEstimatedLength );

	// Play the PBI
	void _InitPlay();

	void ProcessCommand( ActionParams& in_rAction );

	AKRESULT _Play( TransParams & in_transParams, bool in_bPaused, bool in_bForceIgnoreSync = false );

	// Stop the PBI (the PBI is then destroyed)
	//
	virtual void _Stop( AkPBIStopMode in_eStopMode = AkPBIStopMode_Normal, bool in_bHasNotStarted = false );

	// Stop the PBI (the PBI is then destroyed)
	//
	void _Stop( 
		const TransParams & in_transParams,	// Fade parameters
		bool in_bUseMinTransTime			// If true, ensures that a short fade is applied if TransitionTime is 0. 
											// If false, the caller needs to ensure that the lower engine will stop the PBI using another method... (stop offset anyone?)
		);

#ifndef AK_OPTIMIZED
	virtual void _StopAndContinue(
		AkUniqueID			in_ItemToPlay,
		AkUInt16				in_PlaylistPosition,
		CAkContinueListItem* in_pContinueListItem
		);
#endif

public:
	//Pause the PBI
	//
	void _Pause( TransParams & in_transParams );

	//Resume the PBI
	//
	void _Resume( TransParams & in_transParams, bool in_bIsMasterResume );

	virtual void PlayToEnd( CAkParameterNodeBase * in_pNode );

	//Seeking
	virtual void SeekTimeAbsolute( AkTimeMs in_iPosition, bool in_bSnapToMarker );
	virtual void SeekPercent( AkReal32 in_fPercent, bool in_bSnapToMarker );

	// Get instigator
	inline CAkPBIAware* GetInstigatorPtr() const { return m_pInstigator; }

	void SetCbx(CAkVPLSrcCbxNode * in_pCbx) { m_pCbx = in_pCbx; }
	CAkVPLSrcCbxNode * GetCbx() const { return m_pCbx; }

	//CAkRTPCTarget inteface
	virtual void UpdateTargetParam( AkRTPC_ParameterID in_eParam, AkReal32 in_fValue, AkReal32 in_fDelta );
	virtual void NotifyParamChanged(bool in_bLiveEdit, AkRTPC_ParameterID in_rtpcID) { RecalcNotification(in_bLiveEdit); }
	virtual void NotifyParamsChanged(bool in_bLiveEdit, const AkRTPCBitArray& in_bitArray) { RecalcNotification(in_bLiveEdit); }

	// Notify the PBI that the Mute changed
	void MuteNotification(
		AkReal32 in_fMuteRatio, //New mute ratio
		AkMutedMapItem& in_rMutedItem, //Element where mute changed
		bool in_bPrioritizeGameObjectSpecificItems = false
		);

	void NotifyBypass(
		AkUInt32 in_bitsFXBypass,
		AkUInt32 in_uTargetMask = 0xFFFFFFFF
		);

	void UpdateBypass(
		AkRTPC_ParameterID in_rtpcId,
		AkInt16 in_delta
		);

	//Calculate the Muted/Faded effective volume
	virtual void CalculateMutedEffectiveVolume();

	// Get Sound.
	CAkSoundBase*	GetSound() const { return (CAkSoundBase*)m_pParamNode; }

	// get the current play stop transition
	CAkTransition* GetPlayStopTransition();

	// get the current pause resume transition
	CAkTransition*	GetPauseResumeTransition();

    // direct access to Mute Map
    AKRESULT        SetMuteMapEntry( 
        AkMutedMapItem & in_key,
        AkReal32 in_fFadeRatio
        );
  
	virtual void	SetPauseStateForContinuous(bool in_bIsPaused);

	// Prepare the Sample accurate next sound if possible and available.
	virtual void	PrepareSampleAccurateTransition();

	inline void			GetDataPtr( AkUInt8 *& out_pBuffer, AkUInt32 & out_uDataSize ) const
	{ 
		out_pBuffer = m_pDataPtr; 
		out_uDataSize = m_uDataSize; 
	}

	inline bool HasLoadedMedia() const { return m_pDataPtr != NULL;	}

	// IAkAudioCtx interface implementation.
	void				Destroy( AkCtxDestroyReason in_eReason );

	void				Play( AkReal32 in_fDuration );
	void				Stop();
	void				Pause();
	void				Resume();
	void				NotifAddedAsSA();

	bool                WasPaused() { return m_bWasPaused; }
	bool				WasStopped() { return m_bWasStopped && m_ulStopOffset == AK_NO_IN_BUFFER_STOP_REQUESTED; }

	void				ProcessContextNotif( AkCtxState in_eState, AkCtxDestroyReason in_eDestroyReason = CtxDestroyReasonFinished, AkReal32 in_fEstimatedLength = 0.0f );

	AkUniqueID			GetSequenceID() const { return m_PriorityInfoCurrent.GetSeqID(); }
	AkUniqueID			GetInstigatorID() const;
#ifndef AK_OPTIMIZED
	AkUniqueID			GetPlayTargetID() const { return m_playTargetID; }
#endif
	
	AkUniqueID			GetBusOutputProfilingID();
	void				UpdatePriority( AkReal32 in_NewPriority );
	AkForceInline AkReal32 GetPriorityFloat() const
	{
		AKASSERT( m_PriorityInfoCurrent.GetCurrent() >= AK_MIN_PRIORITY && m_PriorityInfoCurrent.GetCurrent() <= AK_MAX_PRIORITY );
		return m_PriorityInfoCurrent.GetCurrent(); 
	}
	AkForceInline AkPriority	GetPriority() const
	{ 
		return (AkPriority)GetPriorityFloat();
	}
	AkPriority			ComputePriorityWithDistance(AkReal32 in_fDistance);

	inline bool			IsPrefetched() const { return m_pSource->IsZeroLatency(); }
	inline AkUInt16		GetLooping() const { return m_LoopCount; }
	
	AkReal32			GetLoopStartOffsetSeconds() const;
	AkReal32			GetLoopEndOffsetSeconds() const; //Seconds from the end of the file
	
	AkUInt32			GetLoopStartOffsetFrames() const;
	AkUInt32			GetLoopEndOffsetFrames() const; //Frames from the end of the file
	
	AkReal32			GetLoopCrossfadeSeconds() const;
	void				LoopCrossfadeCurveShape( AkCurveInterpolation& out_eCrossfadeUpType,  
													AkCurveInterpolation& out_eCrossfadeDownType) const;

	void				GetTrimSeconds(AkReal32& out_fBeginTrim, AkReal32& out_fEndTrim) const;

	void GetSourceFade(	AkReal32& out_fBeginFadeOffsetSec, AkCurveInterpolation& out_eBeginFadeCurveType, 
					AkReal32& out_fEndFadeOffsetSec, AkCurveInterpolation& out_eEndFadeCurveType  );

	const AkAudioFormat& GetMediaFormat() const { return m_sMediaFormat; }
	void				SetMediaFormat(const AkAudioFormat& in_rFormat ) 
	{ 
		m_sMediaFormat = in_rFormat; 
	}
	AkBelowThresholdBehavior GetVirtualBehavior( AkVirtualQueueBehavior& out_Behavior );
	bool ShouldChangeVirtualBehaviourForInterruption();

	void				VirtualPositionUpdate();

	AkForceInline AkSrcTypeInfo *	GetSrcTypeInfo() const { return m_pSource->GetSrcTypeInfo(); }
	AkForceInline void GetSrcType(AkSrcType& out_srcType, AkUInt32& out_uCodecId) const
	{
		
		AkSrcTypeInfo * pSrcType = GetSrcTypeInfo();
		out_srcType = (AkSrcType)pSrcType->mediaInfo.Type;
		out_uCodecId = pSrcType->dwID;		
	}

	inline AkReal32 GetMakeupGaindB() const { return (m_EffectiveParams.MakeUpGain() + m_Ranges.MakeUpGainOffset); }
	inline bool NormalizeLoudness() const { return m_EffectiveParams.bNormalizeLoudness; }

	AkForceInline AkInt16 GetBypassAllFX() { return m_EffectiveParams.iBypassAllFX; }

	AkForceInline CAkSource* GetSource() const { return m_pSource; }

	AkForceInline bool	WasKicked(){ return m_bWasKicked; }
	void				Kick( KickFrom in_eIsForMemoryThreshold );

	AkForceInline AkPlayingID GetPlayingID() const { return m_UserParams.PlayingID(); };

	virtual AkCtxVirtualHandlingResult	NotifyVirtualOff( AkVirtualQueueBehavior in_eBehavior );

	bool IsUsingThisSlot( const CAkUsageSlot* in_pSlotToCheck );
	bool IsUsingThisSlot( const AkUInt8* in_pData );

	bool FindAlternateMedia( const CAkUsageSlot* in_pSlotToCheck );

	// Seeking / source offset management
	inline bool RequiresSourceSeek() const { return m_bSeekDirty; }
	inline bool IsSeekRelativeToDuration() const { return m_bSeekRelativeToDuration; }
	
	// Returns seek position in samples (at the source's sample rate).
	// NOTE: IsSeekRelativeToDuration() must be false to use this method.
	inline AkUInt32 GetSeekPosition( AkUInt32 in_uSampleRateSource, bool & out_bSnapToMarker )
	{
		AKASSERT( !IsSeekRelativeToDuration() );		
		out_bSnapToMarker = m_bSnapSeekToMarker;
		// Note: Force using 64 bit calculation to avoid uint32 overflow.
		AkUInt32 uNativeSampleRate = AK_CORE_SAMPLERATE;
		return (AkUInt32)( (AkUInt64)m_uSeekPosition * in_uSampleRateSource / uNativeSampleRate );
	}

	// Returns seek position in samples (at the source's sample rate). 
	// It is computed using the seeking percentage, so the actual source duration must be given.
	// NOTE: IsSeekRelativeToDuration() must be true to use this method.
	inline AkUInt32 GetSeekPosition( AkUInt32 in_uSampleRateSource, AkReal32 in_fSourceDuration, bool & out_bSnapToMarker )
	{
		AKASSERT( IsSeekRelativeToDuration() );
		out_bSnapToMarker = m_bSnapSeekToMarker;
		return (AkUInt32)( m_fSeekPercent * in_fSourceDuration * in_uSampleRateSource / 1000.f );
	}

	// IMPORTANT: SetSourceOffsetRemainder() is intended to be used by the lower engine ONLY. Every time a source uses the 
	// source offset, it has to clear or push the error value to the PBI using this method (the pitch node
	// handles the error). It has the effect of clearing the dirty flag. 
	// On the other hand, behavioral engines (PBI and derived classes) must set the source offset using
	// SetNewSeekXX() (which sets the dirty flag).
	// todo: rename SeekCompleted( remainderOffset ) to emphase its importance!
	inline void SetSourceOffsetRemainder( AkUInt32 in_uSourceOffset )
	{ 
		m_uSeekPosition = in_uSourceOffset; 
		m_bSeekDirty = false; 
		m_bSeekRelativeToDuration = false; 
		m_bSnapSeekToMarker	= false;
	}

	inline AkUInt32 GetSourceOffsetRemainder() 
	{
		// Ignore seeking error if a new seek is pending.
		if ( !m_bSeekDirty )
			return m_uSeekPosition;
		return 0;
	}

	// Look-ahead time / subframe offset management.
	// 
	// Before running.
	// Use this function to scale the frame size with playback speed before consuming the frame offset.
	AkForceInline AkInt32 ScaleFrameSize( AkUInt32 in_uFrameSize ) { return AkMath::Round( in_uFrameSize * m_fPlaybackSpeed ); }
	
	// Get current frame offset. 
	// IMPORTANT: This is the frame offset at normal playback rate.
	// It is the responsibility of the client to scale other time values (using 
	// ScaleFrameSize() above) before modifying or comparing with this value.
	AkForceInline AkInt32 GetFrameOffset() { return m_iFrameOffset; }

	// IMPORTANT: Be careful to scale the operand (typ. frame size) before calling this.
    AkForceInline void ConsumeFrameOffset( AkInt32 in_iSamplesConsumed ) 
    { 
		// Stop consuming frame offset once it drops below zero (late) to prevent underflow.
		if ( AK_EXPECT_FALSE( m_iFrameOffset >= 0 ) )
			m_iFrameOffset -= in_iSamplesConsumed;
    }

	AkForceInline void AddDelayFrames( AkUInt32 in_uNumFrames ) { m_iFrameOffset += in_uNumFrames; }

	// While/after processing (for padding).
	// Get frame offset before it was consumed. Call this when you need to know the frame offset value before
	// it was consumed in CAkVPLSrcCbxNode::StartRun().
	AkForceInline AkInt32 GetFrameOffsetBeforeFrame() 
	{
		// Deduce size consumed during this frame from data type and current playback speed.
		AkUInt32 uFrameSize = AK_NUM_VOICE_REFILL_FRAMES; 
		return AkMath::Round( ( m_iFrameOffset + ( uFrameSize * m_fPlaybackSpeed ) ) / m_fPlaybackSpeed );
	}

	AkForceInline bool RequiresPreBuffering() const { return m_bRequiresPreBuffering; }

	void GetFXDataID( AkUInt32 in_uFXIndex, AkUInt32 in_uDataIndex, AkUInt32& out_rDataID );

	void UpdateFx(
		AkUInt32	   	in_uFXIndex
		);

	void UpdateSource(
		const AkUInt8* in_pOldDataPtr
	);

	// Used for queries.
	AkForceInline AkReal32 GetMaxDistance(){ return m_fMaxDistance; }

#ifndef AK_OPTIMIZED
	// Notify the monitor with the specified reason
	virtual void Monitor(
		AkMonitorData::NotificationReason in_Reason,			// Reason for the notification
		bool in_bUpdateCount = true
		);

	// Quarry the AM hierarchy to determine status of monitoring mute.
	void RefreshMonitoringMuteSolo();
#else
	inline void Monitor(
		AkMonitorData::NotificationReason,
		bool /*in_bUpdateCount*/ = true
		)
	{/*No implementation in Optimized version*/}
#endif
	
	void Virtualize();
	void Devirtualize( bool in_bAllowKick = true );
	inline bool VoiceNotLimited(){ return (m_bIsForcedToVirtualizeForLimiting || m_bIsVirtual || m_bIsExemptedFromLimiter); }
	inline bool IsVirtual(){ return m_bIsVirtual; }
	inline bool IsExemptedFromLimiter(){ return m_bIsExemptedFromLimiter; }

	inline void WakeUpVoice(){ m_bIsExemptedFromLimiter = false; }// Hardware voice was assigned, cannot bypass the limiter anymore.

	inline bool NeedsFadeIn() { return m_bNeedsFadeIn; }

	inline bool IsLoudnessNormalizationEnabled() 
	{ 
		CalcEffectiveParams();
		return GetEffectiveParams().bNormalizeLoudness;
	}

	inline AkPitchShiftType GetPitchShiftType() { return m_ePitchShiftType; }
	
	void ForceVirtualize( KickFrom in_eReason );
	void ForceVirtualize()// Called by the Wii, watch out when playing with this code.
	{
		m_bIsForcedToVirtualizeForLimiting = true;
	}

	void ForceDevirtualize()
	{
		m_bIsForcedToVirtualizeForLimiting = false; 
	}
	
	void VirtualizeForInterruption();
	void DevirtualizeForInterruption();

	AkForceInline AkPriorityStruct& GetPriorityKey() { return m_PriorityInfoCurrent.GetPriorityKey(); }	

	AkUniqueID GetMidiTargetID() const { return m_rtpcKey.MidiTargetID(); }
	inline const AkMidiEventEx& GetMidiEvent(){ return m_midiEvent; }
	inline CAkMidiNoteState* GetMidiNoteState() const {	return m_pMidiNote; }
	//Used for modulator trigger behavior.
	inline bool IsFirstInSequence() const { return m_bFisrtInSequence; }

	void SkipDecrementPlayCount(){ m_bWasPlayCountDecremented = true; };

	virtual void GetModulatorTriggerParams(AkModulatorTriggerParams& out_params, bool in_bFirstPlay = true);
	void TriggerModulators(const AkModulatorsToTrigger& in_modulators, bool in_bFirstPlay);

	void GetFX( AkUInt32 in_uFXIndex, AkFXDesc & out_rFXInfo );

	// Use GetVirtualBehavior instead of GetUnsafeUnderThresholdBehavior unless you are very sure it was already Cached.
	AkBelowThresholdBehavior GetUnsafeUnderThresholdBehavior(){ return (AkBelowThresholdBehavior)m_eCachedBelowThresholdBehavior; }	

protected:
	enum PBIInitialState
	{
		PBI_InitState_Playing,
		PBI_InitState_Paused,
		PBI_InitState_Stopped
#define PBIINITSTATE_NUM_STORAGE_BIT 3
	};

	// Overridable methods.
	virtual void RefreshParameters(AkInitialSoundParams* in_pInitialSoundParams);

	//Pause the PBI
	//
	//Return - AKRESULT - AK_Success if succeeded
	virtual void _Pause( bool in_bIsFromTransition = false );

	//Resume the PBI
	//
	//Return - AKRESULT - AK_Success if succeeded
	virtual void _Resume();

	// Set raw frame offset. Only for derived classes!
	AkForceInline void SetFrameOffset( AkInt32 in_iFrameOffset ) { m_iFrameOffset = in_iFrameOffset; }

	// Internal use only
	// Notify the monitor with the specified reason
	virtual void MonitorFade(
		AkMonitorData::NotificationReason in_Reason,			// Reason for the notification
		AkTimeMs in_FadeTime
		);

	void CreateTransition(
		bool in_bIsPlayStopTransition,		// true if it is a PlayStop transition, false if it is PauseResume
		AkIntPtr in_transitionTarget,		// Transition target type
		TransParams in_transParams          // Transition parameters.
		);

	// Prepare PBI for stopping with minimum transition time. Note: also used in PBI subclasses.
	void StopWithMinTransTime();

	void DecrementPlayCount();

    void RemoveAllVolatileMuteItems();


	// Seeking, from PBI and derived classes only (behavioral engine).
	inline void SetNewSeekPosition( AkUInt32 in_uSeekPosition, bool in_bSnapSeekToMarker ){ m_uSeekPosition = in_uSeekPosition; m_bSeekRelativeToDuration = false; SetNewSeekFlags( in_bSnapSeekToMarker ); }
	inline void SetNewSeekPercent( AkReal32 in_fSeekPercent, bool in_bSnapSeekToMarker ){ m_fSeekPercent = in_fSeekPercent; m_bSeekRelativeToDuration = true; SetNewSeekFlags( in_bSnapSeekToMarker ); }

	// 3D game defined positioning-dependent priority offset.
	virtual void UpdatePriorityWithDistance(CAkAttenuation* in_pAttenuation, AkReal32 in_fMinDistance);
	AkReal32 ComputePriorityOffset(AkReal32 in_fDistance, CAkAttenuation::AkAttenuationCurve* in_pVolumeDryCurve);
	
#ifndef AK_OPTIMIZED
	virtual void ReportSpatialAudioErrorCodes();
#endif

// Members
	CAkUsageSlot*		m_pUsageSlot;

	AkMutedMap			m_mapMutedNodes;
	AkPBIModValues		m_Ranges;
 
	UserParams			m_UserParams;				// User Parameters.

	PlaybackTransition	m_PBTrans;
	
	CAkPBIAware*		m_pInstigator;				// Instigator of PBI.

	CAkSource*			m_pSource;					// Associated Source
	CAkVPLSrcCbxNode* m_pCbx;					// Combiner pointer	

	AkAudioFormat		m_sMediaFormat;				// Audio format of the source.
	
	AkReal32			m_fPlaybackSpeed;			// Interactive music rate. Currently translates into a pitch shift.

	AkReal32			m_fPlayStopFadeRatio;
	AkReal32			m_fPauseResumeFadeRatio;

	CAkCntrHist			m_CntrHistArray;

	// Seek position (expressed in absolute time or percentage, according to m_bSeekRelativeToDuration).
	union
	{
		AkUInt32			m_uSeekPosition;		// Seek offset in Native samples.
		AkReal32			m_fSeekPercent;			// Seek offset expressed in percentage of duration ([0,1]).
	};

	AkInt16				m_LoopCount;

// enums
	AkUInt8/*PBIInitialState*/	m_eInitialState			:PBIINITSTATE_NUM_STORAGE_BIT;
	AkUInt8/*AkCtxState*/		m_State					:CTXSTATE_NUM_STORAGE_BIT;
	
	AkUInt8						m_eCachedVirtualQueueBehavior	:VIRTUAL_QUEUE_BEHAVIOR_NUM_STORAGE_BIT;
	AkUInt8						m_eCachedBelowThresholdBehavior :BELOW_THRESHOLD_BEHAVIOR_NUM_STORAGE_BIT;
	AkUInt8						m_bVirtualBehaviorCached		:1;

// bools stored on one bit.

	AkUInt8				m_bNeedNotifyEndReached :1;
	AkUInt8				m_bIsNotifyEndReachedContinuous :1;

	AkUInt8				m_bTerminatedByStop		:1;
	AkUInt8				m_bPlayFailed			:1;
	
	//This flag is used to avoid sending commands that would be invalid anyway after sending a stop.
	AkUInt8				m_bWasStopped			:1;
	AkUInt8				m_bWasPreStopped		:1;
	AkUInt8				m_bWasPaused			:1;

	AkUInt8				m_bInitPlayWasCalled	:1;

	AkUInt8				m_bWasKicked			:1;
	AkUInt8				m_eWasKickedForMemory	:3;
	AkUInt8				m_bWasPlayCountDecremented :1;

	AkUInt8				m_bRequiresPreBuffering :1;	// True for PBIs that should not starve on start up (e.g. music).

	AkUInt8             m_bSeekDirty				:1;
	AkUInt8             m_bSeekRelativeToDuration	:1;	// When true, seeking is expressed as a percentage of the whole duration, and m_fSeekPercent must be used
	AkUInt8             m_bSnapSeekToMarker			:1;

	AkUInt8				m_bIsExemptedFromLimiter	:1; // Sample accurate voices do count as one voices in the limiter. Exempted till they become main voice.
	AkUInt8				m_bIsVirtual				:1;
	AkUInt8				m_bNeedsFadeIn				:1;

	AkPitchShiftType	m_ePitchShiftType			:3;

	AkUInt8				m_bFisrtInSequence			:1;
	
	PriorityInfoCurrent m_PriorityInfoCurrent;

	AkUInt32			m_ulPauseCount;

	AkInt32			    m_iFrameOffset;

	AkUInt8 *			m_pDataPtr;
	AkUInt32			m_uDataSize;
	

#ifndef AK_OPTIMIZED
	AkUniqueID			m_playTargetID;	// ID of original played item, which led to this PBI	
#endif

	AkMidiEventEx			m_midiEvent; //The MIDI event that triggered this PBI
	CAkMidiNoteState*		m_pMidiNote; 

public:
	void SetStopOffset( AkUInt32 in_ulStopOffset ) { m_ulStopOffset = in_ulStopOffset; }
	inline AkUInt32 GetStopOffset() const { return m_ulStopOffset; }
	virtual AkUInt32 GetAndClearStopOffset()
	{
		AkUInt32 ulStopOffset = m_ulStopOffset;
		m_ulStopOffset = AK_NO_IN_BUFFER_STOP_REQUESTED;
		return ulStopOffset;
	}

	static AkUniqueID GetNewSequenceID(){ return m_CalSeqID++; }
	
	AkLimiters m_LimiterArray;


	void ClearLimiters();

protected:

	AkUInt32		m_ulStopOffset;

	virtual void OnPathAdded(CAkPath* pPath);

private:

	void AssignMidiNoteState( CAkMidiNoteState* in_pNoteState );

	inline void SetNewSeekFlags( bool in_bSnapSeekToMarker )
	{ 
		m_bSnapSeekToMarker = in_bSnapSeekToMarker; 
		m_bSeekDirty = true; 
	}


	static AkUniqueID	m_CalSeqID;

#if defined (_DEBUG)
	void DebugCheckLimiters();
#endif

};

#endif
