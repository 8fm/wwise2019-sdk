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
// AkPBI.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkPBI.h"
#include "Ak3DListener.h"
#include "AkBankMgr.h"
#include "AkTransitionManager.h"
#include "AkPlayingMgr.h"
#include "AkURenderer.h"
#include "AkBus.h"
#include "AkLEngineCmds.h"
#include "AkMidiNoteCtx.h"
#include "AkEffectsMgr.h"
#include "AkPathManager.h"
#include "AkRegistryMgr.h"                          // for CAkRegistryMgr, CheckObjAndPlayingID

// Global initialized flag to know when spatial audio is in use.
extern bool g_bSpatialAudioInitialized;

//Constants for AkSoundParams.
const AkUInt32 AkSoundParams::s_PropToIdx[AkPropID_NUM] =
{
	0,							//AkPropID_Volume
	AkSoundParams_PROPCOUNT,	//AkPropID_LFE
	1,							//AkPropID_Pitch
	2,							//AkPropID_LPF
	3,							//AkPropID_HPF
	4,							//AkPropID_BusVolume
	5,							//AkPropID_MakeUpGain
	AkSoundParams_PROPCOUNT,	//AkPropID_Priority
	AkSoundParams_PROPCOUNT,	//AkPropID_PriorityDistanceOffs
	AkSoundParams_PROPCOUNT,	//(AkPropID)9
	AkSoundParams_PROPCOUNT,	//(AkPropID)10
	AkSoundParams_PROPCOUNT,	//AkPropID_MuteRatio
	AkSoundParams_PROPCOUNT,	//AkPropID_PAN_LR
	AkSoundParams_PROPCOUNT,	//AkPropID_PAN_FR
	AkSoundParams_PROPCOUNT,	//AkPropID_CenterPCT
	AkSoundParams_PROPCOUNT,	//AkPropID_DelayTime
	AkSoundParams_PROPCOUNT,	//AkPropID_TransitionTime
	AkSoundParams_PROPCOUNT,	//AkPropID_Probability
	AkSoundParams_PROPCOUNT,	//AkPropID_DialogueMode
	6	,						//AkPropID_UserAuxSendVolume0
	7	,						//AkPropID_UserAuxSendVolume1
	8	,						//AkPropID_UserAuxSendVolume2
	9	,						//AkPropID_UserAuxSendVolume3
	10	,						//AkPropID_GameAuxSendVolume
	11	,						//AkPropID_OutputBusVolume
	12	,						//AkPropID_OutputBusHPF
	13	,						//AkPropID_OutputBusLPF
	AkSoundParams_PROPCOUNT,	//AkPropID_HDRBusThreshold
	AkSoundParams_PROPCOUNT,	//AkPropID_HDRBusRatio
	AkSoundParams_PROPCOUNT,	//AkPropID_HDRBusReleaseTime
	AkSoundParams_PROPCOUNT,	//AkPropID_HDRBusGameParam
	AkSoundParams_PROPCOUNT,	//AkPropID_HDRBusGameParamMin
	AkSoundParams_PROPCOUNT,	//AkPropID_HDRBusGameParamMax
	14,							//AkPropID_HDRActiveRange
	AkSoundParams_PROPCOUNT,	//AkPropID_LoopStart
	AkSoundParams_PROPCOUNT,	//AkPropID_LoopEnd
	AkSoundParams_PROPCOUNT,	//AkPropID_TrimInTime
	AkSoundParams_PROPCOUNT,	//AkPropID_TrimOutTime
	AkSoundParams_PROPCOUNT,	//AkPropID_FadeInTime
	AkSoundParams_PROPCOUNT,	//AkPropID_FadeOutTime
	AkSoundParams_PROPCOUNT,	//AkPropID_FadeInCurve
	AkSoundParams_PROPCOUNT,	//AkPropID_FadeOutCurve
	AkSoundParams_PROPCOUNT,	//AkPropID_LoopCrossfadeDuratio
	AkSoundParams_PROPCOUNT,	//AkPropID_CrossfadeUpCurve
	AkSoundParams_PROPCOUNT,	//AkPropID_CrossfadeDownCurve
	AkSoundParams_PROPCOUNT,	//AkPropID_MidiTrackingRootNote
	AkSoundParams_PROPCOUNT,	//AkPropID_MidiPlayOnNoteType
	AkSoundParams_PROPCOUNT,	//AkPropID_MidiTransposition
	AkSoundParams_PROPCOUNT,	//AkPropID_MidiVelocityOffset
	AkSoundParams_PROPCOUNT,	//AkPropID_MidiKeyRangeMin
	AkSoundParams_PROPCOUNT,	//AkPropID_MidiKeyRangeMax
	AkSoundParams_PROPCOUNT,	//AkPropID_MidiVelocityRangeMin
	AkSoundParams_PROPCOUNT,	//AkPropID_MidiVelocityRangeMax
	AkSoundParams_PROPCOUNT,	//AkPropID_MidiChannelMask
	AkSoundParams_PROPCOUNT,	//AkPropID_PlaybackSpeed
	AkSoundParams_PROPCOUNT,	//AkPropID_MidiTempoSource
	AkSoundParams_PROPCOUNT,	//AkPropID_MidiTargetNode
	AkSoundParams_PROPCOUNT,	//AkPropID_AttachedPluginFXID
	AkSoundParams_PROPCOUNT,	//AkPropID_Loop
	AkSoundParams_PROPCOUNT,	//AkPropID_InitialDelay
	15,							//AkPropID_UserAuxSendLPF0
	16,							//AkPropID_UserAuxSendLPF1
	17,							//AkPropID_UserAuxSendLPF2
	18,							//AkPropID_UserAuxSendLPF3
	19,							//AkPropID_UserAuxSendHPF0
	20,							//AkPropID_UserAuxSendHPF1
	21,							//AkPropID_UserAuxSendHPF2
	22,							//AkPropID_UserAuxSendHPF3
	23,							//AkPropID_GameAuxSendLPF
	24,							//AkPropID_GameAuxSendHPF
	AkSoundParams_PROPCOUNT,	//AkPropID_AttenuationID
	AkSoundParams_PROPCOUNT,	//AkPropID_PositioningTypeBlend
	25							//AkPropID_ReflectionBusVolume
};

//-----------------------------------------------------------------------------
// External variables.
//-----------------------------------------------------------------------------
extern CAkRegistryMgr*	g_pRegistryMgr;

AkUniqueID CAkPBI::m_CalSeqID = AK_INVALID_SEQUENCE_ID + 1;

#define MIN_NUM_MUTED_ITEM_IN_LIST 4


CAkPBI::CAkPBI(	AkPBIParams&				in_params,
				CAkSoundBase*				in_pSound,
			    CAkSource*					in_pSource,
				const PriorityInfoCurrent&	in_rPriority,
				AkUInt32					in_uSourceOffset
				)
				: CAkBehavioralCtx(in_params.pGameObj, in_pSound, in_params.bPlayDirectly)
	, m_pUsageSlot( NULL )
	, m_UserParams( in_params.userParams )
	, m_pInstigator( in_params.pInstigator )
	, m_pSource( in_pSource )
	, m_pCbx( NULL )
	, m_fPlaybackSpeed( AK_DEFAULT_PLAYBACK_SPEED )
	, m_fPlayStopFadeRatio( AK_UNMUTED_RATIO )
	, m_fPauseResumeFadeRatio( AK_UNMUTED_RATIO )
	, m_uSeekPosition( in_uSourceOffset )
	, m_LoopCount( LOOPING_ONE_SHOT )
	, m_eInitialState( PBI_InitState_Playing )
	, m_State( CtxStateStop )
	, m_eCachedVirtualQueueBehavior( AkVirtualQueueBehavior_FromBeginning )
	, m_eCachedBelowThresholdBehavior( AkBelowThresholdBehavior_ContinueToPlay )
	, m_bVirtualBehaviorCached( false )
	, m_bNeedNotifyEndReached( false )
	, m_bIsNotifyEndReachedContinuous( false )
	, m_bTerminatedByStop( false )
	, m_bPlayFailed( false )
	, m_bWasStopped( false )
	, m_bWasPreStopped( false )
	, m_bWasPaused( false )
	, m_bInitPlayWasCalled( false )
	, m_bWasKicked( false )
	, m_eWasKickedForMemory( KickFrom_OverNodeLimit )
	, m_bWasPlayCountDecremented( false )
	, m_bRequiresPreBuffering( true )	// True: standard context require pre-buffering if streaming. Note: Zero-latency streams skip pre-buffering at the source level.
	, m_bSeekDirty( in_uSourceOffset != 0 )
	, m_bSeekRelativeToDuration( false )
	, m_bSnapSeekToMarker( false )
	, m_bIsExemptedFromLimiter(false)
	, m_bIsVirtual( false )
	, m_bNeedsFadeIn( in_params.bNeedsFadeIn )
	, m_ePitchShiftType( PitchShiftType_LinearInterpolate )
	, m_bFisrtInSequence( in_params.bIsFirst )
	, m_PriorityInfoCurrent( in_rPriority )
	, m_ulPauseCount( 0 )
	, m_iFrameOffset( in_params.uFrameOffset )
	, m_pDataPtr( NULL )
	, m_uDataSize( 0 )
#ifndef AK_OPTIMIZED
	, m_playTargetID( in_params.playTargetID )
#endif
	, m_midiEvent( in_params.midiEvent )
	, m_pMidiNote( NULL )
	, m_ulStopOffset( AK_NO_IN_BUFFER_STOP_REQUESTED )
{
	AKASSERT(in_params.pGameObj->HasComponent<CAkEmitter>());

	if (in_params.sequenceID == AK_INVALID_SEQUENCE_ID)
	{
		//generate one
		m_PriorityInfoCurrent.SetSeqID( GetNewSequenceID() );
	}
	else
	{
		m_PriorityInfoCurrent.SetSeqID(in_params.sequenceID);
		// This voice is meant to be the sequence of another voice and should be considered Exempted from Limitations.
		// This basically gives this voice the exclusive right to play even if exceeding the limiters limits.
		// This is true until this voice gets the go to be a limited voice
		m_bIsExemptedFromLimiter = true;
	}

#ifndef AK_OPTIMIZED
	if( m_playTargetID == AK_INVALID_UNIQUE_ID && m_pInstigator )
		m_playTargetID = m_pInstigator->ID();	
#endif
	
	m_PriorityInfoCurrent.SetPipelineID(m_PipelineID);

	/// LX REVIEW Why is this the default value? Why is there a default value?
	/// Try Anonymous( 1 )
	m_sMediaFormat.channelConfig.SetStandard( AK_SPEAKER_FRONT_CENTER );

	m_CntrHistArray = in_params.playHistory.HistArray;
	
	AkMidiNoteChannelPair noteAndChannel = m_midiEvent.GetNoteAndChannel();
	m_rtpcKey.MidiChannelNo() = noteAndChannel.channel;
	m_rtpcKey.MidiNoteNo() = noteAndChannel.note;

	if( m_midiEvent.IsValid() && m_pInstigator )
		m_rtpcKey.MidiTargetID() = m_pInstigator->ID();

	m_rtpcKey.PlayingID() = GetPlayingID();
	m_rtpcKey.PBI() = this;

	if (m_midiEvent.IsValid() && m_midiEvent.IsNoteOn())
	{
		//Set MIDI parameters here that need to be unique per-PBI.
		AkRTPCKey keyForMidiParameters = m_rtpcKey;
		keyForMidiParameters.PlayingID() = AK_INVALID_PLAYING_ID; //exception to support playing with authoring tool.
		g_pRTPCMgr->SetMidiParameterValue(AssignableMidiControl_Velocity, (AkReal32)m_midiEvent.NoteOnOff.byVelocity, keyForMidiParameters);
	}

	AssignMidiNoteState( in_params.pMidiNoteState );
}

AKRESULT CAkPBI::Init()
{
	CAkBehavioralCtx::Init();

	InitSpatialAudioVoice();

	if (!m_bPlayDirectly)
	{
		m_LimiterArray.AddLast(&CAkURenderer::GetGlobalLimiter());
		for (AkLimiters::Iterator it = m_LimiterArray.Begin(); it != m_LimiterArray.End(); ++it)
		{
			(*it)->Add(this);
		}
	}

	AKRESULT eResult = AK_Fail;
	if (m_UserParams.PlayingID())
	{
		AKASSERT(g_pPlayingMgr);
		eResult = g_pPlayingMgr->SetPBI(m_UserParams.PlayingID(), this, &m_uRegisteredNotif);
	}
	if (eResult == AK_Success)
	{
		m_pSource->LockDataPtr((void*&)m_pDataPtr, m_uDataSize, m_pUsageSlot);
	}

	return eResult;
}

#ifndef AK_OPTIMIZED
void CAkPBI::ReportSpatialAudioErrorCodes()
{
	if (g_bSpatialAudioInitialized &&
		m_BasePosParams.bHasListenerRelativeRouting &&
		!GetGameObjectPtr()->HasComponent(GameObjComponentIdx_SpatialAudioRoom) &&
		m_posParams.settings.m_e3DPositionType == AK_3DPositionType_ListenerWithAutomation)
	{
		const AkSoundParams& params = GetEffectiveParams();
		if (params.reflectionsAuxBus != AK_INVALID_AUX_ID)
		{
			MONITOR_ERROREX(AK::Monitor::ErrorCode_SpatialAudio_ListenerAutomationNotSupported, GetPlayingID(), GetGameObjectPtr()->ID(), GetSoundID(), false);
			//MONITOR_ERROREX(AK::Monitor::ErrorCode_SpatialAudio_ListenerAutomationNotSupported, AK_INVALID_PLAYING_ID, GetGameObjectPtr()->ID(), GetSoundID(), false);
		}
	}
}
#endif

CAkPBI::~CAkPBI()
{
	AKASSERT( m_pMidiNote == NULL );
	m_LimiterArray.Term();
}

void CAkPBI::Term( bool /*in_bFailedToInit*/ )
{
	AKASSERT(m_pParamNode);
	AKASSERT(g_pTransitionManager);
	AKASSERT(g_pPathManager);

	DecrementPlayCount();

	if( m_PBTrans.pvPSTrans )
	{
		Monitor(AkMonitorData::NotificationReason_Fade_Aborted);
		g_pTransitionManager->RemoveTransitionUser( m_PBTrans.pvPSTrans, this );
	}
	if( m_PBTrans.pvPRTrans )
	{
		Monitor(AkMonitorData::NotificationReason_Fade_Aborted);
		g_pTransitionManager->RemoveTransitionUser( m_PBTrans.pvPRTrans, this );
	}

	if( m_bNeedNotifyEndReached )
	{
		Monitor(AkMonitorData::NotificationReason_EndReached);
		m_bNeedNotifyEndReached = false;
	}

	if(m_UserParams.PlayingID())
	{
		g_pPlayingMgr->Remove(m_UserParams.PlayingID(),this);
	}

	CAkScopedRtpcObj::Term( m_rtpcKey );

	m_mapMutedNodes.Term();

	GetSound()->RemovePBI(this);

	// Must be done BEFORE releasing m_pParamNode, as the source ID is required to free the data.
	if ( m_pDataPtr )
	{
		m_pSource->UnLockDataPtr();
		m_pDataPtr = NULL;
	}
	if ( m_pUsageSlot )
	{
		m_pUsageSlot->Release( false );
		m_pUsageSlot = NULL;
	}
	
	bool bSourceIsExternal = m_pSource->IsExternallySupplied();

	if (m_pMidiNote != NULL)
	{
		m_pMidiNote->DetachPBI(this);
		m_pMidiNote->Release();
		m_pMidiNote = NULL;
	}

	if ( bSourceIsExternal )
	{
		//External sources are cloned when used so we can change their properties with real values.
		//Release the cloned source.
		
		AkDelete(AkMemID_Object, m_pSource);
	}

	CAkBehavioralCtx::Term(false);
}

void CAkPBI::UpdateLimiters()
{
	// Only need to update the PBIs that were made dirty
	if ( m_LimiterArray.Length() == 0 && !m_bPlayDirectly )
	{
		CounterParameters params;
		params.pGameObj = GetGameObjectPtr();
		params.pLimiters = &m_LimiterArray;
		GetSound()->IncrementPlayCount( params, false );


		m_LimiterArray.AddLast(&CAkURenderer::GetGlobalLimiter());
		for (AkLimiters::Iterator it = m_LimiterArray.Begin(); it != m_LimiterArray.End(); ++it)
		{
			(*it)->Add(this);
			if ( m_bIsVirtual )
				(*it)->IncrementVirtual();
		}
	}
}

void CAkPBI::_InitPlay()
{
	// Generate the next loop count.
    AKASSERT( GetSound() != NULL );
	m_LoopCount = GetSound()->Loop();

	//Ensure this function is not called twice
	AKASSERT( !m_bInitPlayWasCalled );
	if(!m_bInitPlayWasCalled)
	{
		m_bInitPlayWasCalled = true; // Must be done BEFORE calling AkMonitorData

		StartPath();
	}
}



void CAkPBI::ProcessCommand( ActionParams& in_rAction )
{
	if( CheckObjAndPlayingID( in_rAction.pGameObj, GetGameObjectPtr(), in_rAction.playingID, GetPlayingID() ) )
	{
		switch( in_rAction.eType )
		{
		case ActionParamType_Stop :
			_Stop( in_rAction.transParams, true );
			break;

		case ActionParamType_Pause :
			_Pause( in_rAction.transParams );
			break;

		case ActionParamType_Resume :
			_Resume( in_rAction.transParams, in_rAction.bIsMasterResume );
			break;

		case ActionParamType_Release :
			ReleaseModulators();
			break;

		case ActionParamType_Break:
			PlayToEnd( in_rAction.targetNodePtr );
			break;

		default:
			AKASSERT(!"ERROR: Command not defined.");
			break;
		}
	}
}

AKRESULT CAkPBI::_Play( TransParams & in_transParams, bool in_bPaused, bool in_bForceIgnoreSync )
{
	AKRESULT eResult;

    // Start transition if applicable.
    if( in_transParams.TransitionTime != 0 )
	{
		m_fPlayStopFadeRatio = AK_MUTED_RATIO;
		CreateTransition( true, 
					 	TransTarget_Play, 
						in_transParams );
	}

	if( in_bPaused == true || m_eInitialState == PBI_InitState_Paused )
	{
		m_bWasPaused = true;
		eResult = CAkLEngineCmds::EnqueueAction( AkLECmd::Type_PlayPause, this );
		
		if( m_PBTrans.pvPSTrans )
		{
			g_pTransitionManager->Pause( m_PBTrans.pvPSTrans );
		}

		PausePath(true);		
		
		m_ModulatorData.Pause();
	}
	else
	{
		eResult = CAkLEngineCmds::EnqueueAction( AkLECmd::Type_Play, this );
	}

	if ( eResult == AK_Success )
	{
		if( m_eInitialState == PBI_InitState_Stopped )
		{
			_Stop();
		}

		if( in_bForceIgnoreSync )
		{
			// especially useful for IM, avoid making IM playback pending one on each others.
			CAkLEngineCmds::IncrementSyncCount();
		}
	}

	return eResult;
}

void CAkPBI::_Stop( AkPBIStopMode in_eStopMode /*= AkPBIStopMode_Normal*/, bool /*in_bHasNotStarted = false*/)
{
	if(!m_bWasStopped)
	{
		m_bWasStopped = true;

		if( in_eStopMode == AkPBIStopMode_Normal || in_eStopMode == AkPBIStopMode_StopAndContinueSequel )
		{
			if( m_PBTrans.pvPSTrans )
			{
				Monitor(AkMonitorData::NotificationReason_Fade_Aborted);
				g_pTransitionManager->RemoveTransitionUser( m_PBTrans.pvPSTrans, this );
				m_PBTrans.pvPSTrans = NULL;
			}
			if( m_PBTrans.pvPRTrans )
			{
				Monitor(AkMonitorData::NotificationReason_Fade_Aborted);
				g_pTransitionManager->RemoveTransitionUser( m_PBTrans.pvPRTrans, this );
				m_PBTrans.pvPRTrans = NULL;
			}

			if(m_PathInfo.pPBPath != NULL)
			{
				// if continous then the path got rid of the played flags
				if(m_PathInfo.pPBPath->IsContinuous())
				{
					AkPathState* pPathState = GetSound()->GetPathState();
					pPathState->pbPlayed = NULL;
					pPathState->ulCurrentListIndex = 0;
				}
				g_pPathManager->RemovePathUser(m_PathInfo.pPBPath,this);
				m_PathInfo.pPBPath = NULL;
				m_PathInfo.PathOwnerID = AK_INVALID_UNIQUE_ID;
			}

			if( m_bWasPaused )
			{
				if (m_ulStopOffset == 0)
					m_ulStopOffset = AK_NO_IN_BUFFER_STOP_REQUESTED;
				Monitor(AkMonitorData::NotificationReason_Pause_Aborted);
			}

			if( m_bIsNotifyEndReachedContinuous || in_eStopMode == AkPBIStopMode_StopAndContinueSequel )
			{
				Monitor(AkMonitorData::NotificationReason_StoppedAndContinue, m_bNeedNotifyEndReached );
			}
			else
			{
				AkMonitorData::NotificationReason reason;

				if( !m_bWasKicked )
					reason = AkMonitorData::NotificationReason_Stopped;
				else
				{
					switch( m_eWasKickedForMemory )
					{
					case KickFrom_Stopped:
						reason = AkMonitorData::NotificationReason_Stopped;
						break;
					case KickFrom_OverNodeLimit:
						reason = AkMonitorData::NotificationReason_StoppedLimit;
						break;
					case KickFrom_OverGlobalLimit:
						reason = AkMonitorData::NotificationReason_StoppedGlobalLimit;
						break;
                    default:
                        reason = AkMonitorData::NotificationReason_None;
                        break;
					}
				}

				Monitor( reason, m_bNeedNotifyEndReached );
			}

			m_bNeedNotifyEndReached = false;
			m_bTerminatedByStop = true;
		}
	}
}

void CAkPBI::_Stop( 
	const TransParams & in_transParams,	// Fade parameters
	bool in_bUseMinTransTime			// If true, ensures that a short fade is applied if TransitionTime is 0. 
										// If false, the caller needs to ensure that the lower engine will stop the PBI using another method... (stop offset anyone?) 
	)
{
	if( m_bWasPaused || ( m_PBTrans.pvPRTrans && m_PBTrans.pvPRTrans->IsFadingOut() ) )
	{
		// If we are paused or going to be, we stop right away.
		_Stop();
	}
	else
	{
		m_bWasPreStopped = true;
		if( in_transParams.TransitionTime != 0 )
		{
			CreateTransition(true, TransTarget_Stop, in_transParams );			 
		}
		else if( m_State == CtxStateStop )//That mean we are not yet started, just let it be stopped without the glitch caussed by the minimal transition time
		{
			_Stop( AkPBIStopMode_Normal, true );	// flag "has not started playing".
		}
		else
		{
			if( m_PBTrans.pvPSTrans )
			{
				// Force actual transition to use new duration of 0 ms
				g_pTransitionManager->ChangeParameter(	static_cast<CAkTransition*>( m_PBTrans.pvPSTrans ), 
														TransTarget_Stop, 
														AK_MUTED_RATIO,
														0,//no transition time
														AkCurveInterpolation_Linear,
														AkValueMeaning_Default );
			}
			else if ( in_bUseMinTransTime )
			{
				StopWithMinTransTime();
			}
			else
			{
				// Otherwise, the caller needs to ensure that the lower engine will stop the PBI using another method... (stop offset anyone?) 
				// Currently, the only other way of stopping is to specify a stop offset.
				AKASSERT( GetStopOffset() != AK_NO_IN_BUFFER_STOP_REQUESTED );
			}
		}
	}
}

// Prepare PBI for stopping with minimum transition time. Note: also used in PBI subclasses.
void CAkPBI::StopWithMinTransTime()
{
	//Set volume to 0 and flag to generate ramp in LEngine.
	m_fPlayStopFadeRatio = AK_MUTED_RATIO;
	// Set fFadeRatio to min directly instead of calling ComputeEffectiveVolumes
	m_EffectiveParams.fFadeRatio = AK_MUTED_RATIO;
	// Set kicked flag to make sure the PBI is no longer counted as an active voice
	if ( ! m_bWasKicked )
	{
		m_bWasKicked = true;
		m_eWasKickedForMemory = KickFrom_Stopped;
	}
	_Stop( AkPBIStopMode_Normal );
}

#ifndef AK_OPTIMIZED
void CAkPBI::_StopAndContinue(
		AkUniqueID			/*in_ItemToPlay*/,
		AkUInt16			/*in_PlaylistPosition*/,
		CAkContinueListItem* /*in_pContinueListItem*/
		)
{
	_Stop();
}
#endif


void CAkPBI::_Pause(bool in_bIsFromTransition /*= false*/)
{
	if(!m_bWasStopped && !m_bWasPaused)
	{		
		m_bWasPaused = true;
		m_fPauseResumeFadeRatio = AK_MUTED_RATIO;
		// Set fFadeRatio to min directly instead of calling ComputeEffectiveVolumes
		m_EffectiveParams.fFadeRatio = AK_MUTED_RATIO;
		AKASSERT(g_pTransitionManager);

		// In the case of transition, it is the combiner node that checks the WasStopped flag
		// once the last transition buffer has been processed.
		if(!in_bIsFromTransition)
		{
			CAkLEngineCmds::EnqueueAction( AkLECmd::Type_Pause, this );
		}
		if( m_PBTrans.pvPSTrans )
		{
			g_pTransitionManager->Pause( m_PBTrans.pvPSTrans );
		}

		PausePath(true);

		m_ModulatorData.Pause();
	}
}

void CAkPBI::_Pause( TransParams & in_transParams )
{
	AkDeltaMonitorUpdateBrace brace(AkDelta_Pause, GetSoundID());

	++m_ulPauseCount;

	AKASSERT( m_ulPauseCount != 0 );//Just in case we were at -1 unsigned	
	if( in_transParams.TransitionTime != 0 )
	{		
		CreateTransition(false, TransTarget_Pause, in_transParams );	
	}
	else if( m_State == CtxStateStop )
	{	// either we were stopped or not yet started, do not use minimal transition time	
		AkDeltaMonitor::LogUpdate(AkPropID_MuteRatio, AK_MUTED_RATIO, GetSoundID(), GetPipelineID());
		_Pause();
	}
	else
	{	
		if( m_PBTrans.pvPRTrans )
		{
			// Force actual transition to use new duration of 0 ms
			g_pTransitionManager->ChangeParameter(	static_cast<CAkTransition*>( m_PBTrans.pvPRTrans ), 
													TransTarget_Pause, 
													AK_MUTED_RATIO,
													0,//no transition time
													AkCurveInterpolation_Linear,
													AkValueMeaning_Default );
		}
		else
		{
			AkDeltaMonitor::LogUpdate(AkPropID_MuteRatio, AK_MUTED_RATIO, GetSoundID(), GetPipelineID());
			_Pause( true );
		}
	}	
}

void CAkPBI::_Resume()
{
	if(!m_bWasStopped)
	{
		if(m_bWasPaused == true)
		{
			PausePath(false);

			m_ModulatorData.Resume();

			m_bWasPaused = false;
			AKASSERT(g_pTransitionManager);
			CAkLEngineCmds::EnqueueAction( AkLECmd::Type_Resume, this );

			if( m_PBTrans.pvPSTrans )
			{
				g_pTransitionManager->Resume( m_PBTrans.pvPSTrans );
			}
		}
	}
}

void CAkPBI::_Resume( TransParams & in_transParams, bool in_bIsMasterResume )
{
	AkDeltaMonitorUpdateBrace brace(AkDelta_Pause, GetSoundID());

	if( in_bIsMasterResume || m_ulPauseCount <= 1)
	{
		m_ulPauseCount = 0;

		_Resume();

		if( in_transParams.TransitionTime != 0 )
		{
			// Use given transition time
			CreateTransition( false, TransTarget_Resume, in_transParams );
		}
		else if( m_PBTrans.pvPRTrans )
		{
			// Force actual transition to use new duration of 0 ms
			g_pTransitionManager->ChangeParameter(	static_cast<CAkTransition*>( m_PBTrans.pvPRTrans ), 
													TransTarget_Resume, 
													AK_UNMUTED_RATIO,
													0,//no transition time
													AkCurveInterpolation_Linear,
													AkValueMeaning_Default );
		}
		else
		{
			// no transition created, using minimal transition time
			AkDeltaMonitor::LogUpdate(AkPropID_MuteRatio, AK_UNMUTED_RATIO, GetSoundID(), GetPipelineID());

			m_fPauseResumeFadeRatio = AK_UNMUTED_RATIO;
			CalculateMutedEffectiveVolume();
		}
	}
	else
	{
		--m_ulPauseCount;
	}
}

void CAkPBI::PlayToEnd( CAkParameterNodeBase * )
{
	m_LoopCount = 1;

	CAkLEngineCmds::EnqueueAction( AkLECmd::Type_StopLooping, this );
}

void CAkPBI::SeekTimeAbsolute( AkTimeMs in_iPosition, bool in_bSnapToMarker )
{
	// Negative seek positions are not supported on sounds.
	AKASSERT( in_iPosition >= 0 );

	// Set source offset and notify lower engine.
	SetNewSeekPosition( AkTimeConv::MillisecondsToSamples( in_iPosition ), in_bSnapToMarker );
	CAkLEngineCmds::EnqueueAction( AkLECmd::Type_Seek, this );
	MONITOR_OBJECTNOTIF(m_UserParams.PlayingID(), GetGameObjectPtr()->ID(), m_UserParams.CustomParam(), AkMonitorData::NotificationReason_Seek, m_CntrHistArray, m_pParamNode->ID(), false, in_iPosition );
}

void CAkPBI::SeekPercent( AkReal32 in_fPercent, bool in_bSnapToMarker )
{
	// Out of bound percentage not accepted.
	AKASSERT( in_fPercent >= 0 && in_fPercent <= 1 );

	// Set source offset and notify lower engine.
	SetNewSeekPercent( in_fPercent, in_bSnapToMarker );
	CAkLEngineCmds::EnqueueAction( AkLECmd::Type_Seek, this );
	AkReal32 fPercent = in_fPercent*100;
	MONITOR_OBJECTNOTIF(m_UserParams.PlayingID(), GetGameObjectPtr()->ID(), m_UserParams.CustomParam(), AkMonitorData::NotificationReason_SeekPercent, m_CntrHistArray, m_pParamNode->ID(), false, *(AkTimeMs*)&fPercent );
}

void CAkPBI::UpdateTargetParam( AkRTPC_ParameterID in_eParam, AkReal32 in_fValue, AkReal32 in_fDelta )
{	
	AkDeltaMonitor::OpenRTPCTargetBrace(GetSoundID(), GetPipelineID());

	switch( in_eParam )
	{
	case RTPC_Priority:
		{
			// Known issue:
			// There is one situation where this will not work properly.
			// It is if the base RTPC + distance offset were totalling an invalid number and this was them modified using RTPCs.
			// If this occur, there may be a slight offset in the distance attenuation factor calculation.
			// We actually ignore this error, since this error will be corrected on next iteration of the 3D calculation if it applies.
			// Fixing this would cause a lot of processing for a simple RTPC update.
			// The fix could be:
			// re-calculate the distance offset priority here ( and access all the associated parameters from here, which may take some time )
			// or keep the last non-clamped calculated offset as a member of the PBI class.( Which looks a bit heavy due to the fact that this is not supposed to happen if the user did draw consistent graphs. )
			//
			AkReal32 fNewBaseValue = m_PriorityInfoCurrent.BasePriority() + in_fDelta;
			AkReal32 fNewCurrent = fNewBaseValue + m_PriorityInfoCurrent.BasePriority() - m_PriorityInfoCurrent.GetCurrent();
			UpdatePriority( fNewCurrent );
			m_PriorityInfoCurrent.ResetBase(fNewBaseValue);
		}
		break;

	case RTPC_BypassFX0:
	case RTPC_BypassFX1:
	case RTPC_BypassFX2:
	case RTPC_BypassFX3:
	case RTPC_BypassAllFX:
		UpdateBypass( in_eParam, (AkInt16)in_fDelta );
		break;
	case RTPC_BusVolume:
		//Transform to OutputBusVolume.
		AkDeltaMonitor::LogUpdate(AkPropID_BusVolume, in_fValue);
		m_EffectiveParams.Accumulate(AkPropID_OutputBusVolume, in_fDelta, AkDelta_None);
		break;
	default:
		CAkBehavioralCtx::UpdateTargetParam(in_eParam, in_fValue, in_fDelta);
		break;
	}	

	AkDeltaMonitor::CloseRTPCTargetBrace();

}

void CAkPBI::MuteNotification( AkReal32 in_fMuteRatio, AkMutedMapItem& in_rMutedItem, bool in_bPrioritizeGameObjectSpecificItems)
{
	if (m_mapMutedNodes.MuteNotification(in_fMuteRatio, in_rMutedItem, in_bPrioritizeGameObjectSpecificItems))
	{
		AkDeltaMonitor::LogUpdate(AkPropID_MuteRatio, in_fMuteRatio, GetSoundID(), GetPipelineID());
	}
	CalculateMutedEffectiveVolume();
}

// direct access to Mute Map. Applies only to persistent items.
AKRESULT CAkPBI::SetMuteMapEntry( 
    AkMutedMapItem & in_key,
    AkReal32 in_fFadeRatio
    )
{
    AKASSERT( in_key.m_bIsPersistent );

    AKRESULT eResult;
    if ( in_fFadeRatio != AK_UNMUTED_RATIO )
        eResult = ( m_mapMutedNodes.Set( in_key, in_fFadeRatio ) ) ? AK_Success : AK_Fail;
    else
    {
        m_mapMutedNodes.Unset( in_key );
        eResult = AK_Success;
    }
    CalculateMutedEffectiveVolume();
    return eResult;
}

void CAkPBI::RemoveAllVolatileMuteItems()
{
    AkMutedMap::Iterator iter = m_mapMutedNodes.Begin();
    while ( iter != m_mapMutedNodes.End() )
	{
        if ( !((*iter).key.m_bIsPersistent) )
            iter = m_mapMutedNodes.EraseSwap( iter );
        else
            ++iter;
	}   
}

void CAkPBI::CalculateMutedEffectiveVolume()
{
	//TODO AkDeltaMonitor Check if all muted paths are taken in account
	//TODO AkDeltaMonitor Check if play/stop fade and pause/resume need to be reported

	// PlayStop and PauseResume fade ratios as well as ratios of the MutedMap are [0,1].
	AkReal32 l_fRatio = 1.0f;
	for( AkMutedMap::Iterator iter = m_mapMutedNodes.Begin(); iter != m_mapMutedNodes.End(); ++iter )
	{
		l_fRatio *= (*iter).item;
	}
	l_fRatio *= m_fPlayStopFadeRatio;
	l_fRatio *= m_fPauseResumeFadeRatio;
	l_fRatio = AkMath::Max( l_fRatio, AK_MUTED_RATIO );	// Clamp to 0 because transitions may result in ratios slightly below 0.
	//AKASSERT( l_fRatio <= 1.f );	// Small overshoots may be accepted. Could also drop a term in taylor approximations of interpolation curves...
	
	m_EffectiveParams.fFadeRatio = l_fRatio;	
	m_bFadeRatioDirty = false;
}

void CAkPBI::NotifyBypass(
	AkUInt32 in_bitsFXBypass,
	AkUInt32 in_uTargetMask /* = 0xFFFFFFFF */ )
{
	if ( m_pCbx 
		&& ( in_uTargetMask & ~( 1 << AK_NUM_EFFECTS_BYPASS_ALL_FLAG ) ) )
	{
		m_pCbx->SetFxBypass( in_bitsFXBypass, in_uTargetMask );
	}

	if (in_uTargetMask & (1 << AK_NUM_EFFECTS_BYPASS_ALL_FLAG))
	{
		AkInt16 bypass = (in_bitsFXBypass & (1 << AK_NUM_EFFECTS_BYPASS_ALL_FLAG)) ? 1 : 0;
		m_EffectiveParams.iBypassAllFX = bypass;
	}
}

void CAkPBI::UpdateBypass(
	AkRTPC_ParameterID in_rtpcId,
	AkInt16 in_delta )
{
	if ( in_rtpcId != RTPC_BypassAllFX )
	{
		if ( m_pCbx )
		{
			AkUInt32 fx = in_rtpcId - RTPC_BypassFX0;
			m_pCbx->UpdateBypass( fx, in_delta );
		}
	}
	else
	{
		m_EffectiveParams.iBypassAllFX += in_delta;
	}
}

void CAkPBI::TransUpdateValue(AkIntPtr in_eTarget, AkReal32 in_fValue, bool in_bIsTerminated)
{	
	AkDeltaMonitorObjBrace brace(GetSoundID());
	AkDeltaMonitor::LogUpdate(AkPropID_MuteRatio, in_fValue, GetSoundID(), GetPipelineID());

	TransitionTargets eTarget = (TransitionTargets) in_eTarget;
	switch(eTarget)
	{
	case TransTarget_Stop:
	case TransTarget_Play:
		if (in_bIsTerminated)
		{
			m_PBTrans.pvPSTrans = NULL;
			Monitor(AkMonitorData::NotificationReason_Fade_Completed);

			if (eTarget == TransTarget_Stop)
			{
				_Stop(AkPBIStopMode_Normal, true);
			}
		}				
		m_fPlayStopFadeRatio = in_fValue;
		break;
	case TransTarget_Pause:
	case TransTarget_Resume:
		if(in_bIsTerminated)
		{
			m_PBTrans.pvPRTrans = NULL;
			Monitor(AkMonitorData::NotificationReason_Fade_Completed);

			if( eTarget == TransTarget_Pause )
			{
				_Pause(true);
			}			
		}			
		m_fPauseResumeFadeRatio = in_fValue;		
		break;
	default:
		AKASSERT(!"Unsupported data type");
		break;
	}	

	m_bFadeRatioDirty = true;
}


void CAkPBI::CreateTransition(bool in_bIsPlayStopTransition, AkIntPtr in_transitionTarget, TransParams in_transParams )
{
	AKASSERT(g_pTransitionManager);
	CAkTransition* prTransition = in_bIsPlayStopTransition?m_PBTrans.pvPSTrans:m_PBTrans.pvPRTrans;

	AkReal32 fTargetValue = AK_UNMUTED_RATIO;
	TransitionTargets eTarget = (TransitionTargets)in_transitionTarget;
	AkDeltaType eChangeReason = TransTarget_Pause == eTarget || TransTarget_Resume == eTarget ? AkDelta_Pause : AkDelta_Fade;
	if( TransTarget_Pause == eTarget || eTarget == TransTarget_Stop )
	{
		fTargetValue = AK_MUTED_RATIO;
	}

	if(!prTransition)
	{
		TransitionParameters Params(
			this,
			in_transitionTarget,
			in_bIsPlayStopTransition?m_fPlayStopFadeRatio:m_fPauseResumeFadeRatio,
			fTargetValue,
			in_transParams.TransitionTime,
			in_transParams.eFadeCurve,
			eChangeReason,
			false,
			true );
		prTransition = g_pTransitionManager->AddTransitionToList(Params);

		if(in_bIsPlayStopTransition)
		{
			m_PBTrans.pvPSTrans = prTransition;
			m_bNeedsFadeIn = ((TransitionTargets)in_transitionTarget == TransTarget_Play);
		}
		else
		{
			m_PBTrans.pvPRTrans = prTransition;
		}

		MonitorFade( AkMonitorData::NotificationReason_Fade_Started, in_transParams.TransitionTime );

		if( !prTransition )
		{
			// TODO : Should send a warning to tell the user that the transition is being skipped because 
			// the max num of transition was reached, or that the transition manager refused to do 
			// it for any other reason.

			// Forcing the end of transition right now.
			TransUpdateValue( Params.eTarget, Params.fTargetValue, true );
		}
	}
	else
	{
		g_pTransitionManager->ChangeParameter(
			static_cast<CAkTransition*>(prTransition),
			in_transitionTarget,
			fTargetValue,
			in_transParams.TransitionTime,
			in_transParams.eFadeCurve,
			AkValueMeaning_Default);
	}
}

void CAkPBI::DecrementPlayCount()
{
	if( m_bIsVirtual )
		Devirtualize( false );

	if( m_bWasPlayCountDecremented == false )
	{
		m_bWasPlayCountDecremented = true;

		for (AkLimiters::Iterator it = m_LimiterArray.Begin(); it != m_LimiterArray.End(); ++it)
		{
			(*it)->Remove( this );
		}
		m_LimiterArray.RemoveAll();

		CounterParameters counterParams;
		counterParams.pGameObj = GetGameObjectPtr();
		GetSound()->DecrementPlayCount( counterParams );

	}
}

// Returns true if the Context may jump to virtual voices, false otherwise.
AkBelowThresholdBehavior CAkPBI::GetVirtualBehavior( AkVirtualQueueBehavior& out_Behavior )
{
	if( !m_bVirtualBehaviorCached )
	{
		m_bVirtualBehaviorCached = true;
		AkBelowThresholdBehavior eBelowThresholdBehavior = GetSound()->GetVirtualBehavior( out_Behavior );

		if ( eBelowThresholdBehavior == AkBelowThresholdBehavior_KillIfOneShotElseVirtual )
		{
			if ( GetSound()->IsInfiniteLooping( m_pInstigator ) )
				eBelowThresholdBehavior = AkBelowThresholdBehavior_SetAsVirtualVoice;
			else
				eBelowThresholdBehavior = AkBelowThresholdBehavior_KillVoice;
		}

		//Set the cache values;
		m_eCachedVirtualQueueBehavior = out_Behavior;
		m_eCachedBelowThresholdBehavior = eBelowThresholdBehavior;

		return eBelowThresholdBehavior;
	}
	else
	{
		out_Behavior = (AkVirtualQueueBehavior)m_eCachedVirtualQueueBehavior;
		return (AkBelowThresholdBehavior)m_eCachedBelowThresholdBehavior;
	}
}

void CAkPBI::MonitorFade( AkMonitorData::NotificationReason in_Reason, AkTimeMs in_TransitionTime )
{
	AkUniqueID id = 0;
	if(m_pParamNode)
	{
		id = m_pParamNode->ID();
	}
	MONITOR_OBJECTNOTIF(m_UserParams.PlayingID(), GetGameObjectPtr()->ID(), m_UserParams.CustomParam(), in_Reason, m_CntrHistArray, id, false, in_TransitionTime );
}

#ifndef AK_OPTIMIZED
void CAkPBI::Monitor(AkMonitorData::NotificationReason in_Reason, bool in_bUpdateCount )
{
	AkUniqueID id = 0;
	if( m_pParamNode )
	{
		id = m_pParamNode->ID();
	}
	if( !m_bInitPlayWasCalled )
	{
		AKASSERT( !( in_Reason == AkMonitorData::NotificationReason_StoppedAndContinue ) );//Should not happen
		AKASSERT( !( in_Reason == AkMonitorData::NotificationReason_EndReachedAndContinue ) );//Should not happen

		//If the PBI was not initialized, it must be considered as a PlayFailed since it never started...
		if( in_Reason == AkMonitorData::NotificationReason_Stopped 
			|| in_Reason == AkMonitorData::NotificationReason_EndReached
			)
		{
			in_Reason = AkMonitorData::NotificationReason_ContinueAborted;
		}
		else if( in_Reason == AkMonitorData::NotificationReason_StoppedAndContinue
			|| in_Reason == AkMonitorData::NotificationReason_EndReachedAndContinue
			)
		{
			return;
		}
	}

	if ( in_bUpdateCount )
		in_Reason = (AkMonitorData::NotificationReason) ( in_Reason | AkMonitorData::NotificationReason_PlayCountBit );

	MONITOR_OBJECTNOTIF(m_UserParams.PlayingID(), GetGameObjectPtr()->ID(), m_UserParams.CustomParam(), in_Reason, m_CntrHistArray, id, false, 0 );
}
#endif
//====================================================================================================
// get the current play stop transition
//====================================================================================================
CAkTransition* CAkPBI::GetPlayStopTransition()
{
	return m_PBTrans.pvPSTrans;
}

//====================================================================================================
// get the current pause resume transition
//====================================================================================================
CAkTransition* CAkPBI::GetPauseResumeTransition()
{
	return m_PBTrans.pvPRTrans;
}

void CAkPBI::SetPauseStateForContinuous(bool)
{
	//empty function for CAkPBI, overriden by continuous PBI
}

void CAkPBI::SetEstimatedLength( AkReal32 )
{
	//nothing to do, implemented only for continuousPBI
}

void CAkPBI::PrepareSampleAccurateTransition()
{
	//nothing to do, implemented only for continuousPBI
}

void CAkPBI::Destroy( AkCtxDestroyReason in_eReason )
{
	// IMPORTANT: Clear Cbx now because other commands may refer to this PBI, 
	// during the same audio frame, between now and when the PBI is actually destroyed.
	m_pCbx = NULL;

	CAkURenderer::EnqueueContextNotif( this, CtxStateToDestroy, in_eReason );
}

void CAkPBI::Play( AkReal32 in_fDuration )
{
	m_State = CtxStatePlay;
	Monitor(AkMonitorData::NotificationReason_Play);
	CAkURenderer::EnqueueContextNotif( this, CtxStatePlay, CtxDestroyReasonFinished, in_fDuration );
}

void CAkPBI::Stop()
{
	m_State = CtxStateStop;
}

void CAkPBI::Pause()
{
	m_State = CtxStatePause;
	Monitor(AkMonitorData::NotificationReason_Paused);
}

void CAkPBI::Resume()
{
	m_State = CtxStatePlay;
	Monitor(AkMonitorData::NotificationReason_Resumed);
}

void CAkPBI::NotifAddedAsSA()
{
	m_State = CtxStatePlay;
}

void CAkPBI::ProcessContextNotif( AkCtxState in_eState, AkCtxDestroyReason in_eDestroyReason, AkReal32 in_fEstimatedLength )
{
	switch( in_eState )
	{
	case CtxStatePlay:
		m_bNeedNotifyEndReached = true;
		PrepareSampleAccurateTransition();
		SetEstimatedLength( in_fEstimatedLength );
		break;
	case CtxStateToDestroy:
		if( in_eDestroyReason == CtxDestroyReasonPlayFailed )
		{
			m_bNeedNotifyEndReached = false;
			m_bPlayFailed = true;
			Monitor(AkMonitorData::NotificationReason_PlayFailed);
		}
		break;
	default:
		AKASSERT( !"Unexpected Context notification" );
		break;
	}
}

AkUniqueID CAkPBI::GetInstigatorID() const
{
	if( m_pInstigator )
		return m_pInstigator->ID();
	return AK_INVALID_UNIQUE_ID;
}


AkUniqueID CAkPBI::GetBusOutputProfilingID()
{
	CAkBus* pBusOutputProfiling = GetOutputBusPtr();
	return pBusOutputProfiling ? pBusOutputProfiling->ID() : AK_INVALID_UNIQUE_ID;
}

void CAkPBI::GetModulatorTriggerParams(AkModulatorTriggerParams& out_params, bool in_bFirstPlay)
{
	out_params.midiEvent = m_midiEvent;
	out_params.pMidiNoteState = m_pMidiNote;
	out_params.midiTargetId = GetMidiTargetID();
	out_params.pGameObj = GetGameObjectPtr();
	out_params.uFrameOffset = m_iFrameOffset;
	out_params.playingId = m_UserParams.PlayingID();
	out_params.pPbi = this;
	out_params.eTriggerMode = m_bFisrtInSequence ?
		(in_bFirstPlay ?
		AkModulatorTriggerParams::TriggerMode_FirstPlay :
		AkModulatorTriggerParams::TriggerMode_ParameterUpdated
		) :
		AkModulatorTriggerParams::TriggerMode_SubsequentPlay;
}

void CAkPBI::TriggerModulators(const AkModulatorsToTrigger& in_modulators, bool in_bFirstPlay)
{
	// Get rid of all xfrms in modulator data; the list will be updated.
	m_ModulatorData.ClearModulationSources();

	if (in_modulators.Length() > 0)
	{
		AkModulatorTriggerParams params;
		CAkPBI::GetModulatorTriggerParams(params, in_bFirstPlay);
		g_pModulatorMgr->Trigger( in_modulators, params, &GetModulatorData() );
	}
}

void CAkPBI::GetFX( AkUInt32 in_uFXIndex, AkFXDesc & out_rFXInfo )
{
	if ( !m_bPlayDirectly )
	{
		m_pParamNode->GetFX( in_uFXIndex, out_rFXInfo, GetGameObjectPtr() );
	}
	else
	{
		out_rFXInfo.pFx = NULL;
	}
}

void CAkPBI::RefreshParameters(AkInitialSoundParams* in_pInitialSoundParams)
{		
	OpenSoundBrace();

	CAkParameterNodeBase* pGetParamsUpToNodes = NULL; //NULL means up to top.
	bool bDoBusCheck = true;
	
	CAkBus* pCtrlBus = GetControlBus();
	if (in_pInitialSoundParams && pCtrlBus == in_pInitialSoundParams->pCtrlBusUsed /*<- Params not useful if for another bus*/ && !m_bPlayDirectly)
	{
#ifdef AK_DELTAMONITOR_ENABLED
		// Using precomputed params, tell to fetch them from parent
		if (in_pInitialSoundParams->pParamsValidFromNode != NULL)
		{
			AkDeltaMonitor::LogUsePrecomputedParams(in_pInitialSoundParams->pParamsValidFromNode->ID());
		}
#endif

		if (pCtrlBus)
			bDoBusCheck = false;
		pGetParamsUpToNodes = in_pInitialSoundParams->pParamsValidFromNode;
		m_EffectiveParams = in_pInitialSoundParams->soundParams;
		m_Ranges = in_pInitialSoundParams->ranges;

		AkUInt32 uNumMutedItems = in_pInitialSoundParams->mutedMap.Length();
		if (uNumMutedItems > 0 && m_mapMutedNodes.Resize(uNumMutedItems))
		{
			for (AkUInt32 i = 0; i < uNumMutedItems; ++i)
				m_mapMutedNodes[i] = in_pInitialSoundParams->mutedMap[i];
		}
	}
	else
	{
		m_EffectiveParams.ClearEx();

		// Make sure we start with an empty map before calling
		// GetAudioParameters(), to avoid keeping obsolete entries.
		// Note: Only volatile map items are removed. Persistent items are never obsolete.
		RemoveAllVolatileMuteItems();

		if (pCtrlBus)
		{
			pCtrlBus->GetNonMixingBusParameters(m_EffectiveParams, AkRTPCKey());
		}
	}
	
	if ( !m_bPlayDirectly )
	{
		//Transfer AkPropID_BusVolume to AkPropID_OutputBusVolume
		//Log only for Mixing busses.
		if (pCtrlBus && pCtrlBus->IsMixingBus())
			AkDeltaMonitor::LogDelta(AkDelta_BaseValue, AkPropID_OutputBusVolume, m_EffectiveParams.BusVolume());

		m_EffectiveParams.AddNoLogOutputBusVolume(m_EffectiveParams.BusVolume());
		m_EffectiveParams.SetNoLogBusVolume(0.f);

		GetSound()->UpdateBaseParams( GetRTPCKey(), &m_BasePosParams, m_p3DAutomation );

		if ( m_pCbx )
			m_pCbx->RefreshBypassFx();

		///////////////////////////////////
		// Must reset previous Audio parameters so newly added Aux busses can be added.
		m_Prev2DParams.Invalidate();
		///////////////////////////////////
		m_EffectiveParams.Request.EnableDefaultProps();		
		if (!IsHDR())
			m_EffectiveParams.Request.UnsetBit(AkPropID_HDRActiveRange);

		AkModulatorsToTrigger* pModulators = NULL;
		if ( in_pInitialSoundParams )
		{
			pModulators = &in_pInitialSoundParams->modulators;
		}

		AkModulatorsToTrigger modulators;
		if (m_bRefreshAllAfterRtpcChange)
			pModulators = &modulators;

		if ( pGetParamsUpToNodes != m_pParamNode )
		{
			///////////////////////////////////////////////////////////////////////////////
			// These need to be reinitialized at every call to GetAudioParameters().
			// see WG-30986
			m_EffectiveParams.ResetServicedFlags();
			///////////////////////////////////////////////////////////////////////////////
			m_pParamNode->GetAudioParameters(
				m_EffectiveParams,				
				m_mapMutedNodes,
				GetRTPCKey(),
				(!m_bGetAudioParamsCalled ? &m_Ranges : NULL), //include ranges only on first update
				pModulators,
				bDoBusCheck,//bus check
				pGetParamsUpToNodes
				);
		}

#ifdef AK_DELTAMONITOR_ENABLED
		if (m_bGetAudioParamsCalled)
		{
			//As stated, the range modifiers (randomizers) are not recomputed when refreshing parameters.
			//Log them though, we need them to appear in this update.  Minor issue: this is the collapsed randomizers.
			AkDeltaMonitor::LogDelta(AkDelta_RangeMod, AkPropID_Volume, m_Ranges.VolumeOffset);
			AkDeltaMonitor::LogDelta(AkDelta_RangeMod, AkPropID_LPF, m_Ranges.LPFOffset);
			AkDeltaMonitor::LogDelta(AkDelta_RangeMod, AkPropID_HPF, m_Ranges.HPFOffset);
		}
#endif

		//Add cumulated randomizers.  They are already reported to the profiler.
		m_EffectiveParams.AddNoLogVolume(m_Ranges.VolumeOffset);
		m_EffectiveParams.AddNoLogPitch(m_Ranges.PitchOffset);
		m_EffectiveParams.AddNoLogLPF(m_Ranges.LPFOffset);
		m_EffectiveParams.AddNoLogHPF(m_Ranges.HPFOffset);
		
		CAkPBI::CalculateMutedEffectiveVolume();		

		PriorityInfo priorityInfo = m_pParamNode->GetPriority( GetGameObjectPtr() );

		if ( priorityInfo.GetPriority() != m_PriorityInfoCurrent.BasePriority() || priorityInfo.GetDistanceOffset() != m_PriorityInfoCurrent.DistanceOffset() )
		{
			m_PriorityInfoCurrent.Reset( priorityInfo );
			UpdatePriority( priorityInfo.GetPriority() );
		}

		// Update modulators if they have changed during a live edit.  
		//	Note that we do not trigger them here under normal triggering circumstances, but CAkURenderer explicity calls TriggerModulators().
		//	This allows us to avoid triggering modulators if we are under threshold and will be killed.
		if (m_bRefreshAllAfterRtpcChange)
		{
			TriggerModulators( *pModulators, !m_bGetAudioParamsCalled );
			m_pParamNode->GetPositioningParams(GetRTPCKey(), m_BasePosParams, m_posParams);
			m_bRefreshAllAfterRtpcChange = false;
		}
		modulators.Term();
	}
	m_bGetAudioParamsCalled = true;
	m_bAreParametersValid = true;

	SpatialAudioParamsUpdated();

	CloseSoundBrace();
}

void CAkPBI::UpdatePriority( AkReal32 in_NewPriority )
{
	in_NewPriority = AkMath::Min(AK_MAX_PRIORITY, AkMax(AK_MIN_PRIORITY, in_NewPriority));
	// First update all the lists, and only then change the priority.
	// The priority is the key to find the object and must not be changed before things have been reordered.
	AkReal32 fLastPriority = GetPriorityFloat();
	if( fLastPriority != in_NewPriority )
	{
		for (AkLimiters::Iterator it = m_LimiterArray.Begin(); it != m_LimiterArray.End(); ++it)
		{
			(*it)->Update(in_NewPriority, this);
		}

		m_PriorityInfoCurrent.SetCurrent( in_NewPriority );
	}
}

void CAkPBI::UpdatePriorityWithDistance( 
	CAkAttenuation* in_pAttenuation, AkReal32 in_fMinDistance
	)
{
	// Priority changes based on distance to listener
	AKASSERT(in_pAttenuation);
	CAkAttenuation::AkAttenuationCurve* pVolumeDryCurve = in_pAttenuation->GetCurve( AttenuationCurveID_VolumeDry );
	if (pVolumeDryCurve)
	{
		AkReal32 priorityTemp = m_PriorityInfoCurrent.BasePriority();
		priorityTemp += ComputePriorityOffset(in_fMinDistance, pVolumeDryCurve);
		UpdatePriority(priorityTemp);
	}
}

AkPriority CAkPBI::ComputePriorityWithDistance(
	AkReal32 in_fDistance
	)
{
	AkReal32 priorityTemp = m_PriorityInfoCurrent.BasePriority();

	// Priority changes based on distance to listener

	//Get curve max radius
	// IMPORTANT Here, we scale the automation radius based on the attenuation's max distance, regardless of whether attenuation is enabled or not. A sign that "max distance" is misplaced...
	CAkAttenuation* pAttenuation = m_posParams.GetRawAttenuation();
	if (pAttenuation)
	{
		CAkAttenuation::AkAttenuationCurve* pVolumeDryCurve = pAttenuation->GetCurve(AttenuationCurveID_VolumeDry);
		if (pVolumeDryCurve)
		{
			priorityTemp += ComputePriorityOffset(in_fDistance, pVolumeDryCurve);
			priorityTemp = AkMath::Min(AK_MAX_PRIORITY, AkMax(AK_MIN_PRIORITY, priorityTemp));
		}
	}
	
	return (AkPriority)priorityTemp;
}

AkReal32 CAkPBI::ComputePriorityOffset(
	AkReal32 in_fDistance,
	CAkAttenuation::AkAttenuationCurve* in_pVolumeDryCurve
	)
{
	AKASSERT(in_pVolumeDryCurve);
	AkReal32 distanceOffsetTemp = m_PriorityInfoCurrent.DistanceOffset();
	AkReal32 priorityOffset;
	if (distanceOffsetTemp == 0.0f)
		priorityOffset = 0.f;
	else
	{
		AkReal32 fMaxDistance = in_pVolumeDryCurve->GetLastPoint().From;
		if (in_fDistance < fMaxDistance && fMaxDistance > 0)
			priorityOffset = in_fDistance / fMaxDistance * distanceOffsetTemp;
		else
			priorityOffset = distanceOffsetTemp;
	}
	return priorityOffset;
}

// Call this instead of ComputeVolumeData when "virtualized".
void CAkPBI::VirtualPositionUpdate()
{
	AKASSERT( IsForcedVirtualized() );

	// Make sure 2d volumes are recalculated when we come back.
	m_Prev2DParams.Invalidate();
	
	// Compute distance and update priority if there is an attenuation.
	// IMPORTANT Here, we scale the automation radius based on the attenuation's max distance, regardless of whether attenuation is enabled or not. A sign that "max distance" is misplaced...
	CAkAttenuation * pAttenuation = m_posParams.GetRawAttenuation();
	if (pAttenuation)
	{
		/// REVIEW Only done when not Hold, or worse, if !m_bGameObjPositionCached, is very suspicious.
		if( !m_posParams.settings.m_bHoldEmitterPosAndOrient || !m_bGameObjPositionCached )
		{
			const AkSoundPositionRef & posEntry = GetEmitter()->GetPosition();
			AkReal32 fMinDistance = CAkURenderer::GetMinDistance(posEntry, GetGameObjectPtr()->GetListeners()) / GetEmitter()->GetScalingFactor();
				
			UpdatePriorityWithDistance(pAttenuation, fMinDistance);
		}
	}

	// Get automation running
	if (m_p3DAutomation && m_PathInfo.pPBPath)
	{
		Ak3DAutomationParams & r3DParams = m_p3DAutomation->GetParams();

		// this one might have been changed so pass it along
		bool bIsLooping = r3DParams.m_bIsLooping;
		m_PathInfo.pPBPath->SetIsLooping(bIsLooping);

		if (bIsLooping
			&& m_PathInfo.pPBPath->IsContinuous()
			&& m_PathInfo.pPBPath->IsIdle())
		{
			StartPath();
		}
	}
}	

void CAkPBI::Kick( KickFrom in_eIsForMemoryThreshold )
{
	if ( ! m_bWasKicked )
	{
		m_eWasKickedForMemory = in_eIsForMemoryThreshold;
		m_bWasKicked = true;
	}
    TransParams transParams;
	_Stop( transParams, true );
}

AkCtxVirtualHandlingResult CAkPBI::NotifyVirtualOff( AkVirtualQueueBehavior )
{
	// Instances of the actor-mixer hierarchy don't handle VirtualOff behaviors in any special way.
	return VirtualHandling_NotHandled;
}

void CAkPBI::GetFXDataID( AkUInt32 in_uFXIndex, AkUInt32 in_uDataIndex, AkUInt32& out_rDataID )
{
	GetSound()->GetFXDataID( in_uFXIndex, in_uDataIndex, out_rDataID );
}

void CAkPBI::UpdateFx(
	AkUInt32	   	in_uFXIndex
	)
{
	if ( m_bPlayDirectly )
		return;

	if ( m_pCbx )
	{
		m_pCbx->UpdateFx( in_uFXIndex );
	}
}

void CAkPBI::UpdateSource(
	const AkUInt8* in_pOldDataPtr
)
{
	if (m_bPlayDirectly)
		return;

	if (m_pCbx)
	{
		m_pCbx->UpdateSource(in_pOldDataPtr);
	}
}

bool CAkPBI::IsUsingThisSlot( const CAkUsageSlot* in_pSlotToCheck )
{
	bool bRet = false;

	if( m_pUsageSlot == in_pSlotToCheck )
	{
		if( !FindAlternateMedia( in_pSlotToCheck ) )
		{
			// Found something using this data and could not change it successfully, must stop it!
			bRet = true;
		}
	}
	
	if( !bRet )
	{
		// We still have to check for potential effects that could be using this slot using Plugin Media.
		if( m_pCbx )
		{
			bRet = m_pCbx->IsUsingThisSlot( in_pSlotToCheck );
		}
	}

	return bRet;
}

bool CAkPBI::IsUsingThisSlot( const AkUInt8* in_pData )
{
	bool bRet = false;

	// We still have to check for potential effects that could be using this slot using Plugin Media.

	if( m_pCbx )
	{
		bRet = m_pCbx->IsUsingThisSlot( in_pData );
	}

	return bRet;
}

bool CAkPBI::FindAlternateMedia( const CAkUsageSlot* in_pSlotToCheck )
{
	CAkVPLSrcNode* pSrcCtx = NULL;
	if( m_pCbx )
	{
		for( AkUInt32 i = 0; i < MAX_NUM_SOURCES; ++i )
		{
			CAkVPLSrcNode** ppSources =  m_pCbx->GetSources();
			// Must find which source is the good one as we may be the sample accurate pending one.
			if( ppSources[i] && ppSources[i]->GetContext() == this )
			{
				pSrcCtx = ppSources[i];
			}
		}
	}

	AkUInt8* pData = NULL;
	AkUInt32 uSize = m_uDataSize;
	CAkUsageSlot* pSlot = NULL;

	if (pSrcCtx && pSrcCtx->SupportMediaRelocation())
	{
		m_pSource->LockDataPtr((void*&)pData, uSize, pSlot);	//Fetch the NEW data pointer for the same media ID.
																//Attempt to detect mismatching media in duplicate banks
		if (pData && uSize == m_uDataSize && pSrcCtx->RelocateMedia(pData, m_pDataPtr) == AK_Success)
		{
			if (pSrcCtx->MustRelocatePitchInputBufferOnMediaRelocation())
			{
				if (m_pCbx->GetContext() == this)// WG-34504 & WG-32540 // Only the CURRENT context should order to make the correction on the Pitch node input buffer. (sample accurate were causing double offset)
					m_pCbx->RelocateMedia(pData, m_pDataPtr);
			}
			if (pSrcCtx->MustRelocateAnalysisDataOnMediaRelocation())
			{
				pSrcCtx->RelocateAnalysisData(pData, m_pDataPtr);
			}
			// Here Release old data and do swap in PBI to.
			if (m_pDataPtr)
			{
				m_pSource->UnLockDataPtr();
			}
			if (m_pUsageSlot)
			{
				m_pUsageSlot->Release(false);
			}
			m_pDataPtr = pData;
			m_pUsageSlot = pSlot;

			return true;
		}
	}

	if (pData && uSize != m_uDataSize)
	{
		//If possible, report the NEW bank.  The OLD bank we had was just reported as an "unload" notification.
		AkUInt32 newBankID = pSlot ? pSlot->key.bankID : 0;
		MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_MediaDuplicationLength, newBankID, GetPlayingID(), GetGameObjectPtr()->ID(), GetParameterNode()->ID(), false);
	}

	//It is quite unexpected that a call to RelocateMedia would fail, but just in case we still need to release.
	if (pData)
		m_pSource->UnLockDataPtr();
	if (pSlot)
		pSlot->Release(false);

	return false;
}

void CAkPBI::ForceVirtualize( KickFrom in_eReason )
{
	AkVirtualQueueBehavior _unused; 
	switch( GetVirtualBehavior( _unused ) )
	{
	case AkBelowThresholdBehavior_KillVoice:
		Kick( in_eReason );
		break;

	case AkBelowThresholdBehavior_SetAsVirtualVoice:
		ForceVirtualize();
		break;

	case AkBelowThresholdBehavior_ContinueToPlay:
		// Ignoring the order to go virtual
		break;

	case AkBelowThresholdBehavior_KillIfOneShotElseVirtual:
		break;
	}
}

void CAkPBI::Virtualize()
{
	if( !m_bIsVirtual )
	{
		m_bIsVirtual = true;

		for ( AkLimiters::Iterator it = m_LimiterArray.Begin(); it != m_LimiterArray.End(); ++it )
		{
			(*it)->IncrementVirtual();
		}

		AkVirtualQueueBehavior bhvr;
		if ( (GetVirtualBehavior(bhvr) == AkBelowThresholdBehavior_SetAsVirtualVoice) && (bhvr != AkVirtualQueueBehavior_FromElapsedTime) )
			CAkURenderer::IncrementDangerousVirtualCount();

		CAkURenderer::IncrementVirtualCount();
	}
}

void CAkPBI::Devirtualize( bool in_bAllowKick )
{
	if( m_bIsVirtual )
	{
		m_bIsVirtual = false;

		for ( AkLimiters::Iterator it = m_LimiterArray.Begin(); it != m_LimiterArray.End(); ++it )
		{
			(*it)->DecrementVirtual();
		}

		AkVirtualQueueBehavior bhvr;
		if ( (GetVirtualBehavior(bhvr) == AkBelowThresholdBehavior_SetAsVirtualVoice) && (bhvr != AkVirtualQueueBehavior_FromElapsedTime) )
			CAkURenderer::DecrementDangerousVirtualCount();

		CAkURenderer::DecrementVirtualCount();
	}
}
	
#ifndef AK_OPTIMIZED
void CAkPBI::RefreshMonitoringMuteSolo()
{
	if (GetCbx())
	{
		// Retrieve mute/solo state from this node in the bus hierarchy.
		bool bNodeSolo = false, bNodeMute = false;
		m_pParamNode->GetMonitoringMuteSoloState(bNodeSolo, bNodeMute);

		if (m_pMidiNote && m_pMidiNote->m_pSourceCtx)
			m_pMidiNote->m_pSourceCtx->GetMonitoringMuteSoloState(bNodeSolo, bNodeMute);

		// Retrieve control bus mute/solo state.  This will only be applied to the direct output (not aux sends).
		bool bCtrlBusSolo = false, bCtrlBusMute = false;
		m_pParamNode->GetCtrlBusMonitoringMuteSoloState(bCtrlBusSolo, bCtrlBusMute);

		GetCbx()->SetMonitoringMuteSolo_DirectOnly( bCtrlBusMute, bCtrlBusSolo );

		//Retrieve mute/solo state from game object.
		bool bGameObjSolo = GetGameObjectPtr()->IsSoloExplicit();
		bool bGameObjMute = GetGameObjectPtr()->IsMute();
		
		bool bOverallSolo = !CAkParameterNodeBase::IsBusMonitoringSoloActive()	// If bus solo is active, we just inherit the propagated passive solo/mute state from the bus update
				&&
			(	(bGameObjSolo && (!CAkParameterNodeBase::IsVoiceMonitoringSoloActive() || bNodeSolo))
				|| 
				(bNodeSolo && (!g_pRegistryMgr->IsMonitoringSoloActive() || bGameObjSolo))
				);

		bool bOverallMute = bNodeMute || bGameObjMute || (!bNodeSolo && CAkParameterNodeBase::IsVoiceMonitoringSoloActive());

		GetCbx()->SetMonitoringMuteSolo_Voice(bOverallMute, bOverallSolo);
	}
}
#endif

AkReal32 CAkPBI::GetLoopStartOffsetSeconds() const
{
	return GetSound()->LoopStartOffset();
}

 //Seconds from the loop end to the end of the file
AkReal32 CAkPBI::GetLoopEndOffsetSeconds() const
{
	return GetSound()->LoopEndOffset();
}

AkUInt32 CAkPBI::GetLoopStartOffsetFrames() const
{
	AKASSERT( GetMediaFormat().uSampleRate != 0 );
	return (AkUInt32)floor( GetSound()->LoopStartOffset() * (AkReal32)GetMediaFormat().uSampleRate + 0.5f );
}

 //Seconds from the loop end to the end of the file
AkUInt32 CAkPBI::GetLoopEndOffsetFrames() const
{
	AKASSERT( GetMediaFormat().uSampleRate != 0 );
	return (AkUInt32)floor( GetSound()->LoopEndOffset() * (AkReal32)GetMediaFormat().uSampleRate + 0.5f );
}

AkReal32 CAkPBI::GetLoopCrossfadeSeconds() const
{
	return GetSound()->LoopCrossfadeDuration();
}

void CAkPBI::LoopCrossfadeCurveShape( AkCurveInterpolation& out_eCrossfadeUpType,  AkCurveInterpolation& out_eCrossfadeDownType) const
{
	GetSound()->LoopCrossfadeCurveShape(out_eCrossfadeUpType, out_eCrossfadeDownType);
}

void CAkPBI::GetTrimSeconds(AkReal32& out_fBeginTrim, AkReal32& out_fEndTrim) const
{
	GetSound()->GetTrim( out_fBeginTrim, out_fEndTrim );
}

void CAkPBI::GetSourceFade(	AkReal32& out_fBeginFadeOffsetSec, AkCurveInterpolation& out_eBeginFadeCurveType, 
					AkReal32& out_fEndFadeOffsetSec, AkCurveInterpolation& out_eEndFadeCurveType  )
{
	GetSound()->GetFade(	out_fBeginFadeOffsetSec, out_eBeginFadeCurveType, 
					out_fEndFadeOffsetSec, out_eEndFadeCurveType  );
}

void CAkPBI::AssignMidiNoteState( CAkMidiNoteState* in_pNoteState )
{ 
	AKASSERT(m_pMidiNote == NULL);
	m_pMidiNote = in_pNoteState; 
	if (m_pMidiNote != NULL)
	{
		m_pMidiNote->AddRef();
		MidiEventActionType eAction = GetSound()->GetMidiNoteOffAction();
		m_pMidiNote->AttachPBI(eAction,this);
	}
}

AkUniqueID AkPBIParams::GetMidiTargetID() const
{
	return ( midiEvent.IsValid() && pInstigator ) ? pInstigator->ID() : AK_INVALID_UNIQUE_ID;
}

void AkPBIParams::PopulateInitialSoundParams(CAkParameterNodeBase* in_pFromNode)
{
	AKASSERT(in_pFromNode != NULL);

	if (initialSoundParams.pParamsValidFromNode != in_pFromNode && 
		!midiEvent.IsValid() 
		/* There are few reasons why pre-fetching midi params is not supported: 
				(1) The note is filtered/transformed as we walk down the hierarchy, so we cant assume the children will share the same RTPC key. 
				(2) Several RTPCs are set for velocity, note, and frequency in CAkSound::PlayInternal().  This has not happened yet.
		*/
		)
	{
		AkRTPCKey rtpcKey;
		rtpcKey.GameObj() = pGameObj;
		rtpcKey.PlayingID() = userParams.PlayingID();
		bool bDoBusCheck = true;
		//rtpcKey.PBI() = ???  <- we can't fill out this field yet, as the pbi has not been created.

		CAkBus* pControlBus = in_pFromNode->GetControlBus();
		if (initialSoundParams.pCtrlBusUsed != NULL)
		{
			if (initialSoundParams.pCtrlBusUsed != pControlBus)
			{
				// If the control bus is different, it means it was overridden and in this case the parameters we have are useless.
				initialSoundParams.Clear();
			}
			else
			{
				// outpus bus is the same. Don't collect its params.
				bDoBusCheck = false;
			}
		}

		///////////////////////////////////////////////////////////////////////////////
		// These need to be reinitialized at every call to PopulateInitialSoundParams().
		// see WG-30986
		initialSoundParams.soundParams.ResetServicedFlags();
		///////////////////////////////////////////////////////////////////////////////

		AkDeltaMonitor::OpenPrecomputedParamsBrace(in_pFromNode->ID());
		if (bDoBusCheck == false)
		{
			// Using precomputed params, tell to fetch them from parent
			AkDeltaMonitor::LogUsePrecomputedParams(initialSoundParams.pParamsValidFromNode->ID());
		}

		initialSoundParams.soundParams.Request.EnableDefaultProps();
		in_pFromNode->GetAudioParameters(initialSoundParams.soundParams, initialSoundParams.mutedMap, rtpcKey, &initialSoundParams.ranges, &initialSoundParams.modulators, bDoBusCheck, initialSoundParams.pParamsValidFromNode);

		
		initialSoundParams.pParamsValidFromNode = in_pFromNode;
		if (initialSoundParams.pCtrlBusUsed == NULL && pControlBus != NULL)
		{
			pControlBus->GetNonMixingBusParameters(initialSoundParams.soundParams, AkRTPCKey());
			initialSoundParams.pCtrlBusUsed = pControlBus;
		}
		AkDeltaMonitor::ClosePrecomputedParamsBrace();
	}
}

void CAkPBI::VirtualizeForInterruption()
{
	m_bIsVirtualizedForInterruption = true;
	if(ShouldChangeVirtualBehaviourForInterruption())
	{
		// No need to set m_bVirtualBehaviorCached = true here, it's done in GetVirtualBehavior.
		m_eCachedBelowThresholdBehavior = AkBelowThresholdBehavior_SetAsVirtualVoice;
		m_eCachedVirtualQueueBehavior = AkVirtualQueueBehavior_FromElapsedTime;

		if(m_pCbx)
		{
			for(AkMixConnectionList::Iterator itConnection = m_pCbx->BeginConnection(); itConnection != m_pCbx->EndConnection(); ++itConnection)
				(*itConnection)->SetPrevSilent(true);
		}
    }
}

void CAkPBI::DevirtualizeForInterruption()
{
	m_bIsVirtualizedForInterruption = false;
}

bool CAkPBI::ShouldChangeVirtualBehaviourForInterruption()
{
	AkVirtualQueueBehavior out_Behavior;
	AkBelowThresholdBehavior res = GetVirtualBehavior(out_Behavior);
	return res == AkBelowThresholdBehavior_ContinueToPlay;
}

void CAkPBI::ClearLimiters()
{
	for (AkLimiters::Iterator it = m_LimiterArray.Begin(); it != m_LimiterArray.End(); ++it)
	{
		(*it)->Remove(this);
		if (m_bIsVirtual)
			(*it)->DecrementVirtual();
	}
	m_LimiterArray.RemoveAll();
}

void CAkPBI::OnPathAdded(CAkPath* pPBPath)
{
	//Set parameters of the path that are used to identify it to the profiler.
	pPBPath->SetSoundUniqueID(m_pParamNode->ID());
	pPBPath->SetPlayingID(m_UserParams.PlayingID());
}