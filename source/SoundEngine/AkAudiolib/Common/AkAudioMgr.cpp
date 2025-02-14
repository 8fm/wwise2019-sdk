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
// CAkAudioMgr.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkAudioMgr.h"
#include "AkAudioLib.h"
#include "AkSound.h"

#include "Ak3DListener.h"
#include "AkActionStop.h"
#include "AkLEngine.h"
#include "AkURenderer.h"
#include "AkTransitionManager.h"
#include "AkPathManager.h"
#include "AkPlayingMgr.h"
#include "AkQueuedMsg.h"
#include "AkEffectsMgr.h"
#include "AkSwitchMgr.h"
#include "AkRegistryMgr.h"
#include "AkEvent.h"
#include "AkStreamCacheMgmt.h"
#include "AkDynamicSequence.h"

#include "AkProfile.h"
#include "AkAudioLibTimer.h"

#include "AkBankMgr.h"
#include "AkOutputMgr.h"

#include "AkMidiDeviceMgr.h"
#include "../../AkMemoryMgr/Common/AkMemoryMgrBase.h"

#include "ICommunicationCentral.h"
#include <AK/Comm/AkCommunication.h>

#include "AkCommon.h"
#include "AkCustomPluginDataStore.h"

#ifdef AK_PS4
	#include "AkACPManager.h"
#endif

extern AkPlatformInitSettings g_PDSettings;
extern AkInitSettings g_settings;

#ifndef AK_OPTIMIZED
extern AK::Comm::ICommunicationCentral * g_pCommCentral;
#endif

extern BehavioralExtensionArray g_aBehavioralExtensions[AkGlobalCallbackLocation_Num];
extern AkExternalUnloadBankHandlerCallback g_pExternalUnloadBankHandlerCallback;

void _CallGlobalExtensions(AkGlobalCallbackLocation in_eLocation);

extern AkInitSettings					g_settings;

AkTimeMs	AkMonitor::m_ThreadTime = 0;

#define AK_MIN_NUM_PENDING_ITEMS 32
#define AK_MIN_NUM_PAUSED_PENDING_ITEMS 32

#define DEF_MSG_OBJECT_SIZE( _NAME_, _OBJECT_ ) \
AkUInt16 AkQueuedMsg::_NAME_() { return BASE_AKQUEUEMSGSIZE + sizeof( _OBJECT_ ); }

AkUInt16 AkQueuedMsg::Sizeof_GameObjMultiPositionBase() 
{
	return BASE_AKQUEUEMSGSIZE + offsetof( AkQueuedMsg_GameObjMultiplePosition, aMultiPosition );
}

AkUInt16 AkQueuedMsg::Sizeof_GameObjMultiObstructionBase()
{
	return BASE_AKQUEUEMSGSIZE + offsetof( AkQueuedMsg_GameObjMultipleObstruction, pObstructionOcclusionValues );
}

static bool GetListeners(AkQueuedMsg_ListenerIDs& in_listenerIDMsg, AkListenerSet& out_listeners)
{
	if (out_listeners.Reserve(in_listenerIDMsg.uNumListeners))
	{
		for (AkUInt16 i = 0; i < in_listenerIDMsg.uNumListeners; ++i)
			out_listeners.Set(in_listenerIDMsg.aListenerIDs[i].gameObjID);
		return true;
	}
	return false;
}


AkUInt16 AkQueuedMsg::Sizeof_EndOfList() 
{
	return BASE_AKQUEUEMSGSIZE;
}

AkUInt16 AkQueuedMsg::Sizeof_EventPostMIDIBase()
{
	return BASE_AKQUEUEMSGSIZE + offsetof( AkQueuedMsg_EventPostMIDI, aPosts );
}

DEF_MSG_OBJECT_SIZE( Sizeof_Event,						AkQueuedMsg_Event )
DEF_MSG_OBJECT_SIZE( Sizeof_Rtpc,						AkQueuedMsg_Rtpc )
DEF_MSG_OBJECT_SIZE( Sizeof_RtpcWithTransition,			AkQueuedMsg_RtpcWithTransition )
DEF_MSG_OBJECT_SIZE( Sizeof_State,						AkQueuedMsg_State )
DEF_MSG_OBJECT_SIZE( Sizeof_Switch,						AkQueuedMsg_Switch )
DEF_MSG_OBJECT_SIZE( Sizeof_Trigger,					AkQueuedMsg_Trigger )
DEF_MSG_OBJECT_SIZE( Sizeof_RegisterGameObj,			AkQueuedMsg_RegisterGameObj )
DEF_MSG_OBJECT_SIZE( Sizeof_UnregisterGameObj,			AkQueuedMsg_UnregisterGameObj )
DEF_MSG_OBJECT_SIZE( Sizeof_GameObjPosition,			AkQueuedMsg_GameObjPosition )
DEF_MSG_OBJECT_SIZE( Sizeof_GameObjActiveControllers,	AkQueuedMsg_GameObjActiveControllers )
DEF_MSG_OBJECT_SIZE( Sizeof_DefaultActiveListeners,		AkQueuedMsg_DefaultActiveListeners)
DEF_MSG_OBJECT_SIZE( Sizeof_ResetListeners,				AkQueuedMsg_ResetListeners)
DEF_MSG_OBJECT_SIZE( Sizeof_ListenerSpatialization, 	AkQueuedMsg_ListenerSpatialization )
DEF_MSG_OBJECT_SIZE( Sizeof_GameObjEnvValues,			AkQueuedMsg_GameObjEnvValues )
DEF_MSG_OBJECT_SIZE( Sizeof_GameObjDryLevel,			AkQueuedMsg_GameObjDryLevel )
DEF_MSG_OBJECT_SIZE( Sizeof_GameObjObstruction, 		AkQueuedMsg_GameObjObstruction )
DEF_MSG_OBJECT_SIZE( Sizeof_ResetSwitches,				AkQueuedMsg_ResetSwitches )
DEF_MSG_OBJECT_SIZE( Sizeof_ResetRTPC,					AkQueuedMsg_ResetRTPC )
DEF_MSG_OBJECT_SIZE( Sizeof_ResetRTPCValue,				AkQueuedMsg_ResetRTPCValue )
DEF_MSG_OBJECT_SIZE( Sizeof_ResetRTPCValueWithTransition,	AkQueuedMsg_ResetRtpcValueWithTransition )
DEF_MSG_OBJECT_SIZE( Sizeof_SetOutputVolume,	AkQueuedMsg_SetOutputVolume )
DEF_MSG_OBJECT_SIZE( Sizeof_OpenDynamicSequence,		AkQueuedMsg_OpenDynamicSequence )
DEF_MSG_OBJECT_SIZE( Sizeof_DynamicSequenceCmd, 		AkQueuedMsg_DynamicSequenceCmd )
DEF_MSG_OBJECT_SIZE( Sizeof_DynamicSequenceSeek,		AkQueuedMsg_DynamicSequenceSeek)
DEF_MSG_OBJECT_SIZE( Sizeof_KillBank,					AkQueuedMsg_KillBank )
DEF_MSG_OBJECT_SIZE( Sizeof_StopAll,					AkQueuedMsg_StopAll )
DEF_MSG_OBJECT_SIZE( Sizeof_StopPlayingID,				AkQueuedMsg_StopPlayingID )
DEF_MSG_OBJECT_SIZE( Sizeof_PlayingIDAction,			AkQueuedMsg_PlayingIDAction )
DEF_MSG_OBJECT_SIZE( Sizeof_EventAction,				AkQueuedMsg_EventAction )
DEF_MSG_OBJECT_SIZE( Sizeof_EventStopMIDI,				AkQueuedMsg_EventStopMIDI )
DEF_MSG_OBJECT_SIZE( Sizeof_LockUnlockStreamCache,		AkQueuedMsg_LockUnlockStreamCache )
DEF_MSG_OBJECT_SIZE( Sizeof_GameObjScalingFactor,		AkQueuedMsg_GameObjScalingFactor )
DEF_MSG_OBJECT_SIZE( Sizeof_Seek,						AkQueuedMsg_Seek )
DEF_MSG_OBJECT_SIZE( Sizeof_PlaySourcePlugin,		    AkQueuedMsg_SourcePluginAction )
DEF_MSG_OBJECT_SIZE( Sizeof_StartStopCapture,			AkQueuedMsg_StartStopCapture )
DEF_MSG_OBJECT_SIZE( Sizeof_AddOutputCaptureMarker,		AkQueuedMsg_AddOutputCaptureMarker )
DEF_MSG_OBJECT_SIZE( Sizeof_SetEffect,					AkQueuedMsg_SetEffect )
DEF_MSG_OBJECT_SIZE( Sizeof_SetBusConfig,				AkQueuedMsg_SetBusConfig)
DEF_MSG_OBJECT_SIZE( Sizeof_SetPanningRule,				AkQueuedMsg_SetPanningRule )
DEF_MSG_OBJECT_SIZE( Sizeof_SetSpeakerAngles,			AkQueuedMsg_SetSpeakerAngles )
DEF_MSG_OBJECT_SIZE( Sizeof_SetProcessMode,				AkQueuedMsg_SetProcessMode )
DEF_MSG_OBJECT_SIZE( Sizeof_SetRandomSeed,				AkQueuedMsg_SetRandomSeed )
DEF_MSG_OBJECT_SIZE( Sizeof_MuteBackgroundMusic,		AkQueuedMsg_MuteBackgroundMusic )
DEF_MSG_OBJECT_SIZE( Sizeof_PluginCustomGameData,		AkQueuedMsg_PluginCustomGameData )
DEF_MSG_OBJECT_SIZE( Sizeof_Suspend,					AkQueuedMsg_Suspend )
DEF_MSG_OBJECT_SIZE( Sizeof_InitSinkPlugin,				AkQueuedMsg_InitSinkPlugin )
DEF_MSG_OBJECT_SIZE( Sizeof_ApiExtension,				AkQueuedMsg_ApiExtension )
DEF_MSG_OBJECT_SIZE( Sizeof_SetBusDevice,				AkQueuedMsg_SetBusDevice )
DEF_MSG_OBJECT_SIZE( Sizeof_ReplaceOutput,				AkQueuedMsg_ReplaceOutput )

///////////////////////////////////////
//Helpers
inline bool CheckObjAndPlayingID(  const CAkRegisteredObj* in_pObjSearchedFor, AkPlayingID in_PlayingIDSearched, AkPendingAction* pActualPendingAction )
{
	return CheckObjAndPlayingID( in_pObjSearchedFor, pActualPendingAction->GameObj(), in_PlayingIDSearched, pActualPendingAction->UserParam.PlayingID() );
}
//////////////////////////////////////


AKRESULT AkLockLessMsgQueue::Init( AkMemPoolId in_PoolId, AkUInt32 in_ulSize )
{
	m_pStart = (AkUInt8 *)AkAlloc( in_PoolId, in_ulSize );
	if (!m_pStart)
		return AK_Fail;

	m_pRead = m_pStart;
	m_pWrite = m_pStart;

	m_pEnd = m_pStart + in_ulSize;

	m_uQueueSize = in_ulSize;

	return AK_Success;
}

void AkLockLessMsgQueue::Term( AkMemPoolId in_PoolId )
{
	if ( m_pStart )
	{
		AkFree( in_PoolId, m_pStart );
		m_pStart = NULL;
	}
}

bool AkLockLessMsgQueue::NeedWraparound()
{
	return (m_pRead + Sizeof_QueueWrapAround) > m_pEnd;
}

AkQueuedMsg * CAkAudioMgr::ReserveForWrite(AkUInt32 &io_size)
{
	//Signals the fact that the queue is being modified.  That means that RenderAudio can't finish until this write is completed.
	AkAtomicInc32(&m_uMsgQueueWriters);

	AkQueuedMsg* pRet = (AkQueuedMsg*)m_MsgQueue.ReserveForWrite(io_size);

	if (!pRet)
	{
		// only decrement when returning NULL, otherwise it will be decrement by the message destructor.
		AkAtomicDec32(&m_uMsgQueueWriters);
	}
	return pRet;
}

void * AkLockLessMsgQueue::ReserveForWrite(AkUInt32 &io_size)
{
	io_size += 3;
	io_size &= ~0x03;

tryAgain:
	AkUInt8* pWriteBegin = m_pWrite;
	AkUInt8* pWriteEnd = pWriteBegin;
	AkUInt8* pRead = m_pRead; // ensure consistency if m_pRead changes while we're doing stuff

	if ( pRead > pWriteBegin ) // simple case : contiguous free space
	{
		// NOTE: in theory the empty space goes to m_pEnd if pRead == m_pVirtualEnd, but
		// this breaks our lock-free model.
		if (io_size + 4 < (AkUIntPtr)(pRead - pWriteBegin))
		{
			pWriteEnd += io_size;
			if (!AkAtomicCasPtr((AkAtomicPtr*)&m_pWrite, pWriteEnd, pWriteBegin))
				goto tryAgain;
			
			return pWriteBegin;
		}
	}
	else
	{
		if (io_size + 4 < (AkUIntPtr)(m_pEnd - pWriteBegin)) // fits in the remaining space before the end ?
		{
			pWriteEnd += io_size;
			if (!AkAtomicCasPtr((AkAtomicPtr*)&m_pWrite, pWriteEnd, pWriteBegin))
				goto tryAgain;

			return pWriteBegin;
		}

		if (io_size + 4 < (AkUIntPtr)(pRead - m_pStart)) // fits in the space before the read ptr ?
		{
			pWriteEnd = m_pStart + io_size;
			if (!AkAtomicCasPtr((AkAtomicPtr*)&m_pWrite, pWriteEnd, pWriteBegin))
				goto tryAgain;

			//Write the tag saying the rest of the buffer is unused.
			AKASSERT(pWriteBegin <= m_pEnd);
			if(pWriteBegin + Sizeof_QueueWrapAround <= m_pEnd)
			{
				AkQueuedMsg *pMsg = (AkQueuedMsg *)pWriteBegin;
				pMsg->type = QueuedMsgType_QueueWrapAround;
				pMsg->size = Sizeof_QueueWrapAround;
			}
			//If not enough space, the message loop will detect it anyway.
			return m_pStart;
		}
	}
	
	return NULL;
}

//-----------------------------------------------------------------------------
// Name: CAkAudioMgr
// Desc: Constructor.
//
// Parameters:
//	None.
//
// Return: 
//	None.
//-----------------------------------------------------------------------------
CAkAudioMgr::CAkAudioMgr()
	:
#ifndef AK_OPTIMIZED
	m_MsgQueueSizeFilled(0),
#endif
	m_uBufferTick(0),
	m_iEndOfListCount(0),
	m_timeLastBuffer(0),
	m_timeThisBuffer(0),
	m_uCallsWithoutTicks(0),
	m_uMsgQueueWriters(0),
	m_fFractionalBuffer(0.f)
{
}

//-----------------------------------------------------------------------------
// Name: ~CAkAudioMgr
// Desc: Destructor.
//
// Parameters:
//	None.
//
// Return: 
//	None.
//-----------------------------------------------------------------------------
CAkAudioMgr::~CAkAudioMgr()
{
	AKASSERT(m_MsgQueueHandlers.IsEmpty()); //Forgort to call UnregisterMsgQueueHandler??
	m_MsgQueueHandlers.Term();
}

AKRESULT CAkAudioMgr::Init()
{
	// Calculate the number of blocks we will create in the command queue
	AKRESULT eResult = m_MsgQueue.Init(AkMemID_SoundEngine, g_settings.uCommandQueueSize);
	
	if( eResult == AK_Success )
	{
		eResult = m_mmapPending.Reserve( AK_MIN_NUM_PENDING_ITEMS );
	}
	if( eResult == AK_Success )
	{
		eResult = m_mmapPausedPending.Reserve(AK_MIN_NUM_PAUSED_PENDING_ITEMS);
	}
	StampTimeLastBuffer();
	return eResult;
}

void CAkAudioMgr::Term()
{
	RemoveAllPreallocAndReferences();
	RemoveAllPausedPendingAction();
	RemoveAllPendingAction();

	m_MsgQueue.Term(AkMemID_SoundEngine);
	m_mmapPending.Term();
	m_mmapPausedPending.Term();
}

//-----------------------------------------------------------------------------
// Name: RenderAudio
// Desc: Render Upper Audio Engine.
//-----------------------------------------------------------------------------

#define MAX_SPINS_BITS 7

void CAkAudioMgr::WaitOnMsgQueuewriters()
{
	//Spin-wait to make sure that other threads have finished writing their messages in their reservations.  This should be very quick.
	AkUInt32 spins = 0;
	while ( AkAtomicLoad32( &m_uMsgQueueWriters ) > 0 )
	{
		AkSleep(spins >> MAX_SPINS_BITS);//Will sleep 0 ms (thread yield) for 128 spins, then sleep 1 ms (real thread suspend).
		spins++;
	}
	AK_ATOMIC_FENCE_FULL_BARRIER();
}

AKRESULT CAkAudioMgr::RenderAudio(bool in_bAllowSyncRender)
{
	AK_INSTRUMENT_SCOPE("CAkAudioMgr::RenderAudio");

	bool bProcess = !m_MsgQueue.IsEmpty(); // do not unnecessarily wake up the audio thread if no events have been enqueued.
	if (bProcess)
	{
		//Reserve right away to make sure that any other thread logging a new message will go AFTER.		
		ReserveQueue(QueuedMsgType_EndOfList, AkQueuedMsg::Sizeof_EndOfList());
		FinishQueueWrite();

		WaitOnMsgQueuewriters();

		AkAtomicInc32(&m_iEndOfListCount);
	}

	//In offline mode, process a frame even if there isn't any events.	
	if (AK_PERF_OFFLINE_RENDERING || (!g_settings.bUseLEngineThread && in_bAllowSyncRender))
	{
		Perform();
		AK_IF_PERF_OFFLINE_RENDERING({
			AkMonitor::Instance()->DispatchNotification();
		})
	}
	else if (bProcess)
		m_audioThread.WakeupEventsConsumer();

	return AK_Success;
}

void CAkAudioMgr::IncrementBufferTick()
{ 
	++m_uBufferTick; 
#ifndef AK_OPTIMIZED
	AkPerf::PostTimers();
	AkMonitor::Monitor_EndOfFrame();
	AkMonitor::SetThreadTime( (AkTimeMs) ( (AkReal64) m_uBufferTick * 1000.0 * AK_NUM_VOICE_REFILL_FRAMES / AK_CORE_SAMPLERATE ) );
	AkMonitor::Monitor_TimeStamp();
#endif
}

AkQueuedMsg * CAkAudioMgr::ReserveQueue( AkUInt16 in_eType, AkUInt32 in_uSize )
{
	AkQueuedMsg * pData = ReserveForWrite( in_uSize );
	
	// Our message queue is full; drain it and wait for the audio thread
	// to tell us that we can re-enqueue this message
	while (pData == NULL)
	{
		if (AK_EXPECT_FALSE(in_uSize > m_MsgQueue.GetMaxQueueSize()))
		{
			MONITOR_ERROR(AK::Monitor::ErrorCode_CommandTooLarge);
			return NULL;
		}

		MONITOR_ERROR(AK::Monitor::ErrorCode_CommandQueueFull);

		{
			CAkFunctionCritical SpaceSetAsCritical;

			pData = ReserveForWrite(in_uSize);// Yes, try Again, because we have been waiting ont the lock "SpaceSetAsCritical" and it could have been solved by another thread, and we should not keep on blocking the audio thread, we need to release this lock ASAP.

			if (!pData)// if another thread handled the queue full, no need to wait.
			{
				WaitOnMsgQueuewriters();

				bool bDummy;
				ProcessMsgQueue(true, bDummy ) ;

				pData = ReserveForWrite(in_uSize);
			}
		}
	}	

	pData->type = in_eType;
	pData->size = (AkUInt16)in_uSize;

	return pData;
}

AkUInt32 CAkAudioMgr::ComputeFramesToRender()
{	
	AkUInt32 l_uNumBufferToFill = CAkLEngine::GetNumBufferNeededAndSubmit();

	AK_IF_PERF_OFFLINE_RENDERING({
		l_uNumBufferToFill = 1;
	})
	AK_ELSE_PERF_OFFLINE_RENDERING({
		if(CAkOutputMgr::AllSlaves())
		{
			if(!CAkOutputMgr::RenderIsActive())
				l_uNumBufferToFill = 1;	//Render one frame per manual call to RenderAudio.
			else		
			{
				// The Sink was just reinitialized, it means that there is no buffer to fill.
				// m_timeLastBuffer is set to now after Sink ReInit to not take account the
				// init time for the hardware timeout handling
				if (m_timeLastBuffer > m_timeThisBuffer)
					return 0;

				const AkReal32 msPerBuffer = (1000.0f / ((AkReal32)AK_CORE_SAMPLERATE / AK_NUM_VOICE_REFILL_FRAMES));
				AkReal32 dElapsed = (AkReal32)AKPLATFORM::Elapsed(m_timeThisBuffer, m_timeLastBuffer);
				AkReal32 dNumBufferToFill = dElapsed / msPerBuffer + m_fFractionalBuffer;
				l_uNumBufferToFill = (AkUInt32)(dNumBufferToFill);
				if(l_uNumBufferToFill > 0)
					m_fFractionalBuffer = dNumBufferToFill - l_uNumBufferToFill;

				if(l_uNumBufferToFill > (AkUInt32)AK_CORE_SAMPLERATE / AK_NUM_VOICE_REFILL_FRAMES) //If we need to render too many frames to keep up with time, it will do the opposite (take too much time).  Bite the bullet.
					l_uNumBufferToFill = 1;	
			}
		}
		else
		{
			m_fFractionalBuffer = 0.f;
		}
	})
	return l_uNumBufferToFill;
}

bool CAkAudioMgr::m_bPipelineDirty( false );

void CAkAudioMgr::Perform()
{	
	AK_INSTRUMENT_SCOPE("CAkAudioMgr::Perform");
	
	g_csMain.Lock();

#ifndef AK_OPTIMIZED
	if (AkMonitor::NeedsDeltaRecap())
		AkMonitor::RecapParamDelta();
#endif

	AkAudiolibTimer::Item * pTimerItemEventMgr = AK_START_TIMER(0, AkAudiolibTimer::uTimerEventMgrThread, 0);
	AKPLATFORM::PerformanceCounter(&m_timeThisBuffer);

	_CallGlobalExtensions( AkGlobalCallbackLocation_Begin );

	AkUInt32 l_uNumBufferToFill = ComputeFramesToRender();

	bool bHasTicked = (l_uNumBufferToFill > 0);
	if (bHasTicked)
	{
		// reset time last buffer only if it was not reset already after frame start
		if (m_timeLastBuffer < m_timeThisBuffer)
			m_timeLastBuffer = m_timeThisBuffer;

		m_uCallsWithoutTicks = 0;
	}
	else
		HandleLossOfHardwareResponse();

#ifdef AK_PS4
	if (!bHasTicked)
	{
		//There are no buffers to submit this go round, but we may have decoding jobs to kick off.
		//  Only SubmitDecodeBatch() if Poll() returns true (ie the last batch is finished), because otherwise SubmitDecodeBatch() will block.
		if ( CAkACPManager::Get().Poll() && CAkACPManager::Get().NeedAnotherPass() )
			CAkACPManager::Get().SubmitDecodeBatch();
	}
#endif
	do
	{
		if ( l_uNumBufferToFill )
			_CallGlobalExtensions( AkGlobalCallbackLocation_PreProcessMessageQueueForRender );

		// Process events from the main queue.
		bool bDeviceSwitch = false;
		bool bMessagesProcessed = ProcessMsgQueue(false, bDeviceSwitch);

		if (AK_EXPECT_FALSE(bDeviceSwitch || CAkOutputMgr::IsResetMainDeviceSet()))
		{
			//If a WakeupFromSuspend occured, the number of frames to render will be different.
			l_uNumBufferToFill = ComputeFramesToRender();
		}
	
		// Process events that are pending.
		bool bPendingActionsProcessed = ProcessPendingList();

		if (bMessagesProcessed || bPendingActionsProcessed)
		{
			CAkListener::OnBeginFrame();

			_CallGlobalExtensions(AkGlobalCallbackLocation_PostMessagesProcessed);
			
			g_pRegistryMgr->UpdateGameObjectPositions();
		}

		// Execute all new "play sound" low-level commands. If we've been awaken because of a RenderAudio(),
		// it is beneficial to perform this now in order to be ready to play at the next audio frame.
		if ( CAkLEngineCmds::ProcessPlayCmdsNeeded() )
		{
#ifdef AK_PS4
			if ( l_uNumBufferToFill > 0 ||				// If there are buffers to fill, we must process play commands.
				CAkACPManager::Get().Poll() )			// If not, check to see if we are waiting on jobs.  If we are, then delay the processing of play commands because
														//		starting an ATRAC9 voice requires that we wait for the batch decode jobs to be done.
#endif
				CAkLEngineCmds::ProcessPlayCommands();
		}
	
		if ( l_uNumBufferToFill == 0 )
			break;

		_CallGlobalExtensions( AkGlobalCallbackLocation_BeginRender );

		// Here, the +1 forces the one buffer look ahead.
		// We must tell the transitions where we want to be on next buffer, not where we are now
		AkUInt32 l_ActualBufferTick = GetBufferTick() + 1;

		g_pTransitionManager->ProcessTransitionsList( l_ActualBufferTick );
		g_pPathManager->ProcessPathsList( l_ActualBufferTick );

		CAkLEngineCmds::ProcessAllCommands();

		g_pModulatorMgr->ProcessModulators();

		CAkLEngine::Perform();

		AkPipelineBufferBase::ClearFreeListBuckets();

		CAkURenderer::PerformContextNotif();

		_CallGlobalExtensions(AkGlobalCallbackLocation_EndRender);

		--l_uNumBufferToFill;

#ifndef AK_OPTIMIZED
		AkResourceMonitor::Callback();

		if (AK::MemoryMgr::IsFrameCheckEnabled())
		{
			AK::MemoryMgr::CheckForOverwrite();
		}
		AK_PERF_TICK_AUDIO();
#endif

		AK_STOP_TIMER(pTimerItemEventMgr);
		IncrementBufferTick();
		pTimerItemEventMgr = AK_START_TIMER(0, AkAudiolibTimer::uTimerEventMgrThread, 0);
	}
	while ( true );

	AK::IAkStreamMgr::Get()->SignalAllDevices();

	_CallGlobalExtensions( AkGlobalCallbackLocation_End );

	AK_STOP_TIMER(pTimerItemEventMgr);

#ifndef AK_OPTIMIZED
	// Process communications
	if (bHasTicked)
	{
		DoProcessCommunication();
	}
#endif // !(defined AK_OPTIMIZED)

	g_csMain.Unlock();
}

void CAkAudioMgr::DoProcessCommunication()
{
#ifndef AK_OPTIMIZED
	if (g_pCommCentral && !AK_PERF_OFFLINE_RENDERING)
	{
		AKRESULT eResult = g_pCommCentral->Process();
		// if commCentral hit an error then reset comms so it can recover
		if (eResult == AK_Fail)
		{
			AK::Comm::Reset();
		}
	}
#endif
}

void CAkAudioMgr::ProcessCommunication()
{
#ifndef AK_OPTIMIZED
	g_csMain.Lock();

	DoProcessCommunication();

	g_csMain.Unlock();
#endif
}

//Game Object ID is passed only for Monitoring purposes.  If in_pGameObj is null, we can still report the GO ID. 
void CAkAudioMgr::ExecuteEvent( CAkEvent* in_pEvent, CAkRegisteredObj * in_pGameObj, AkGameObjectID in_GO, AkPlayingID in_playingID, AkPlayingID in_TargetPlayingID, const AkCustomParamType& in_rCustomParam, AkUInt32 in_uiFrameOffset)
{
	for( CAkEvent::AkActionList::Iterator iter = in_pEvent->m_actions.Begin(); iter != in_pEvent->m_actions.End(); ++iter )
	{
		CAkAction* pAction = *iter;
		AKASSERT( pAction );

		AkPendingAction* pThisAction = NULL;

		if ( ACTION_TYPE_USE_OBJECT & pAction->ActionType() )
		{
			if(!in_pGameObj)
			{
				MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_Event, in_playingID, in_GO, in_pEvent->ID(), false);
			}
			else if(!in_pGameObj->HasComponent<CAkEmitter>())
			{
				MONITOR_ERROREX(AK::Monitor::ErrorCode_GameObjectIsNotEmitterEvent, in_playingID, in_GO, in_pEvent->ID(), false);
			}
			else
			{
				pThisAction = AkNew(AkMemID_Object, AkPendingAction(in_pGameObj));
			}
		}
		else
		{
			pThisAction = AkNew(AkMemID_Object, AkPendingAction(NULL));
		}

		if( pThisAction )
		{
			pThisAction->TargetPlayingID = in_TargetPlayingID;
			pThisAction->pAction = pAction;
			pThisAction->UserParam.Init(in_playingID, in_rCustomParam);

			g_pAudioMgr->EnqueueOrExecuteAction( pThisAction, in_uiFrameOffset);
		}
	}
}

void CAkAudioMgr::ProcessAllActions( CAkEvent* in_pEvent, AkQueuedMsg_EventAction& in_rEventAction, CAkRegisteredObj * in_pGameObj )
{
	// Find each action play in the list, and execute the action on their target.
	for( CAkEvent::AkActionList::Iterator iter = in_pEvent->m_actions.Begin(); iter != in_pEvent->m_actions.End(); ++iter )
	{
		CAkAction* pAction = *iter;
		AKASSERT( pAction );
		
		AkActionType aType = pAction->ActionType();
		switch( aType )
		{
		case AkActionType_Play:
			{
				CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
				pTargetNode.Attach( pAction->GetAndRefTarget() );

				if( pTargetNode )
				{
					ProcessCustomAction( 
						pTargetNode, 
						in_pGameObj, 
						in_rEventAction.eActionToExecute, 
						in_rEventAction.uTransitionDuration,
						in_rEventAction.eFadeCurve,
						in_rEventAction.TargetPlayingID
						);
				}
			}
			break;

		case AkActionType_PlayEvent:
			{
				CAkEvent* pSubEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( pAction->ElementID() );
				if( pSubEvent )
				{
					ProcessAllActions( pSubEvent, in_rEventAction, in_pGameObj );
					pSubEvent->Release();
				}
			}
			//No break here intentionnal. Fallthrough.
		default:
			switch( in_rEventAction.eActionToExecute )
			{
			case AK::SoundEngine::AkActionOnEventType_Stop:
			case AK::SoundEngine::AkActionOnEventType_Break:
				g_pAudioMgr->StopAction( pAction->ID(), in_rEventAction.TargetPlayingID );
				break;

			case AK::SoundEngine::AkActionOnEventType_Pause:
				g_pAudioMgr->PauseAction( pAction->ID(), in_rEventAction.TargetPlayingID );
				break;

			case AK::SoundEngine::AkActionOnEventType_Resume:
				g_pAudioMgr->ResumeAction( pAction->ID(), in_rEventAction.TargetPlayingID );
				break;
			case AK::SoundEngine::AkActionOnEventType_ReleaseEnvelope:
				break;
			default:
				AKASSERT(!"Unsupported AkActionOnEventType");
				break;
			}
			break;
		}
	}
}

void CAkAudioMgr::ProcessAllActions( CAkEvent* in_pEvent, AkQueuedMsg_EventPostMIDI& in_rPostMIDI, CAkRegisteredObj* in_pGameObj )
{
	// Find each action play in the list, and post MIDI on the target.
	for( CAkEvent::AkActionList::Iterator iter = in_pEvent->m_actions.Begin(); iter != in_pEvent->m_actions.End(); ++iter )
	{
		CAkAction* pAction = *iter;
		AKASSERT( pAction );
		
		AkActionType aType = pAction->ActionType();
		switch( aType )
		{
		case AkActionType_Play:
			{
				CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
				pTargetNode.Attach( pAction->GetAndRefTarget() );

				if( pTargetNode )
				{
					AkNodeCategory eCategory = pTargetNode->NodeCategory();

					if( eCategory == AkNodeCategory_RanSeqCntr
						|| eCategory == AkNodeCategory_Sound
						|| eCategory == AkNodeCategory_SwitchCntr
						|| eCategory == AkNodeCategory_LayerCntr )
					{
						PostMIDIEvent( pTargetNode, in_pGameObj, in_rPostMIDI );
					}
				}
			}
			break;

		case AkActionType_PlayEvent:
			{
				CAkEvent* pSubEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( pAction->ElementID() );
				if( pSubEvent )
				{
					ProcessAllActions( pSubEvent, in_rPostMIDI, in_pGameObj );
					pSubEvent->Release();
				}
			}
			break;

		default:
			break;
		}
	}
}

void CAkAudioMgr::ProcessAllActions( CAkEvent* in_pEvent, AkQueuedMsg_EventStopMIDI& in_rStopMIDI, CAkRegisteredObj* in_pGameObj )
{
	// It is possible that no event was specified
	if( ! in_pEvent )
	{
		PostMIDIStop( NULL, in_pGameObj );
	}
	else
	{
		// Find each action play in the list, and post MIDI on the target.
		for( CAkEvent::AkActionList::Iterator iter = in_pEvent->m_actions.Begin(); iter != in_pEvent->m_actions.End(); ++iter )
		{
			CAkAction* pAction = *iter;
			AKASSERT( pAction );
		
			AkActionType aType = pAction->ActionType();
			switch( aType )
			{
			case AkActionType_Play:
				{
					CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
					pTargetNode.Attach( pAction->GetAndRefTarget() );

					if( pTargetNode )
					{
						PostMIDIStop( pTargetNode, in_pGameObj );
					}
				}
				break;

			case AkActionType_PlayEvent:
				{
					CAkEvent* pSubEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( pAction->ElementID() );
					if( pSubEvent )
					{
						ProcessAllActions( pSubEvent, in_rStopMIDI, in_pGameObj );
						pSubEvent->Release();
					}
				}
				break;

			default:
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Name: ProcessMsgQueue
// Desc: Process the message queue.
//
// Parameters:
//	in_bDrainMsgQueue : if true, force processing messages regardless of m_iEndOfListCount
//-----------------------------------------------------------------------------
bool CAkAudioMgr::ProcessMsgQueue( bool in_bDrainMsgQueue, bool &out_bDeviceSwitch )
{
	AK_INSTRUMENT_SCOPE("CAkAudioMgr::ProcessMsgQueue");

	out_bDeviceSwitch = false;
	AKASSERT(g_pIndex);

	AK_ATOMIC_FENCE_FULL_BARRIER();

	AkUInt32 uUsageSize = m_MsgQueue.GetUsageSize();
	
#ifndef AK_OPTIMIZED
	// how much of the queue has been filled
	AkUInt32 uMsgQueueSizeFilled = uUsageSize;
	if (m_MsgQueueSizeFilled < uMsgQueueSizeFilled)
	{
		m_MsgQueueSizeFilled = uMsgQueueSizeFilled;
	}
#endif

	// Can't process until the writer has finished one frame (m_iEndOfListCount > 0), unless we are draining.
	if( uUsageSize == 0
		|| (!in_bDrainMsgQueue && m_iEndOfListCount == 0 ) 
		|| (in_bDrainMsgQueue && uUsageSize <= m_MsgQueue.GetMaxQueueSize() / 2 ) )
	{
		return false;
	}

	//When draining the queue, we must avoid processing the whole buffer to avoid issues chasing the write pointer. (see assert in case QueuedMsgType_EndOfList)
	//We simply need to freeup *some* memory for the new messages, not the whole memory.
	bool bReachedEnd = false;
	AkUInt32 uDrainedBytes = 0;
	AkUInt32 uBytesToDrain = uUsageSize / 2;

	do
	{
		AkQueuedMsg * pItem = (AkQueuedMsg *) m_MsgQueue.BeginRead();
		if (!m_MsgQueue.NeedWraparound())
		{
			MONITOR_API_FUNCTION_CALL(pItem);
			switch(pItem->type)
			{
			default:
				AKASSERT(!"Invalid msg queue item type!");
				break;

			case QueuedMsgType_QueueWrapAround:
				//This message tells that the rest of the queue buffer is unused (the next message was too large to fit in)
				m_MsgQueue.EndRead(m_MsgQueue.GetMaxQueueSize());	//Force wrap around.
				continue;

			case QueuedMsgType_Invalid:
				break;		//Skip normally.  This message reservation was cancelled and is unused.

			case QueuedMsgType_EndOfList:
				//Can only process the next frame when it is complete.
				//Note, end the msg processing when reaching the end of a frame, even when in "drain mode". 
				if(m_iEndOfListCount == 0)
					return true; // Make sure drain does not consume end-of-list prematurely: leave it in the message queue.
				bReachedEnd = AkAtomicDec32(&m_iEndOfListCount) == 0;
				break;

			case QueuedMsgType_Event:
			{
				MONITOR_EVENTTRIGGERED(pItem->event.PlayingID, pItem->event.pEvent->ID(), pItem->event.gameObjID, pItem->event.CustomParam);
				
				CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(pItem->event.gameObjID);

				ExecuteEvent(pItem->event.pEvent, pGameObj, pItem->event.gameObjID, pItem->event.PlayingID, pItem->event.TargetPlayingID, pItem->event.CustomParam);

				if(pGameObj)
				{
					pGameObj->Release();
				}
				g_pPlayingMgr->RemoveItemActiveCount(pItem->event.PlayingID);

				pItem->event.pEvent->Release();

				if(pItem->event.CustomParam.pExternalSrcs)
					pItem->event.CustomParam.pExternalSrcs->Release();
			}
			break;

			case QueuedMsgType_SourcePluginAction:
			{
				UserParams local_userParams;
				local_userParams.SetPlayingID(pItem->sourcePluginAction.PlayingID);

				AkUInt32 ulPluginID = CAkEffectsMgr::GetMergedID(
					AkPluginTypeSource,
					pItem->sourcePluginAction.CompanyID,
					pItem->sourcePluginAction.PluginID
					);

				// Adding bit meaning this object wa dynamically created without the Wwise project.
				AkUniqueID soundID = ulPluginID | AK_SOUNDENGINE_RESERVED_BIT;

				switch(pItem->sourcePluginAction.ActionType)
				{
				case AkSourcePluginActionType_Play:
				{
					CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(pItem->sourcePluginAction.gameObjID);
					if(pGameObj)// Avoid performing if the GameObject was specified but does not exist.
					{
						//Create a dummy effect
						CAkFxCustom* pFx = static_cast<CAkFxCustom*>(g_pIndex->m_idxFxCustom.GetPtrAndAddRef(soundID));
						if(!pFx)
						{
							pFx = CAkFxCustom::Create(soundID);
							if(pFx)
							{
								pFx->SetFX(ulPluginID, NULL, 0);
							}
						}
						else
						{
							pFx->Release();
						}

						CAkSound* pSound = static_cast<CAkSound*>(g_pIndex->GetNodePtrAndAddRef(soundID, AkNodeType_Default));
						if(!pSound && pFx)
						{
							pSound = CAkSound::Create(soundID);
							if(pSound)
							{
								pSound->SetSource(pFx->ID());
							}
						}
						else
						{
							pSound->Release();// release only if GetPtrAndAddRef was called.
						}
						PlaySourceInput(soundID, pGameObj, local_userParams);
						pGameObj->Release();
					}
					else
					{
						MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_SourcePluginAction, pItem->sourcePluginAction.PlayingID, pItem->sourcePluginAction.gameObjID, AK_INVALID_UNIQUE_ID, false);
					}	
				}
				break;

				case AkSourcePluginActionType_Stop:
				{
					CAkSound* pSound = static_cast<CAkSound*>(g_pIndex->GetNodePtrAndAddRef(soundID, AkNodeType_Default));

					if(pSound)
					{
						ActionParams l_Params;
						l_Params.bIsFromBus = false;
						l_Params.bIsMasterResume = false;
						l_Params.transParams.eFadeCurve = AkCurveInterpolation_Linear;
						l_Params.pGameObj = NULL;
						l_Params.playingID = pItem->sourcePluginAction.PlayingID;
						l_Params.transParams.TransitionTime = 0;
						l_Params.bIsMasterCall = false;
						l_Params.eType = ActionParamType_Stop;

						pSound->ExecuteAction(l_Params);
						pSound->Release();
					}
				}
				break;
				}

			}
			break;

			case QueuedMsgType_EventAction:
			{
				AkQueuedMsg_EventAction& rEventAction = pItem->eventAction;

				CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(rEventAction.gameObjID);
				if((pGameObj && pGameObj->HasComponent<CAkEmitter>()) || rEventAction.gameObjID == AK_INVALID_GAME_OBJECT)// Avoid performing if the GameObject was specified but does not exist.
				{
					ProcessAllActions(rEventAction.pEvent, rEventAction, pGameObj);
				}
				else
				{
					if(!pGameObj)
					{
						MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_EventAction, AK_INVALID_PLAYING_ID, pItem->eventAction.gameObjID, pItem->eventAction.eventID, false);
					}
					else
					{
						MONITOR_ERROREX(AK::Monitor::ErrorCode_GameObjectIsNotEmitterEvent, AK_INVALID_PLAYING_ID, pItem->eventAction.gameObjID, pItem->eventAction.eventID, false);
					}
				}

				if(pGameObj)
				{
					pGameObj->Release();
				}

				pItem->eventAction.pEvent->Release();
			}
			break;

			case QueuedMsgType_EventPostMIDI:
			{
				AkQueuedMsg_EventPostMIDI& rPostMIDI = pItem->postMIDI;

				CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(rPostMIDI.gameObjID);
				if((pGameObj && pGameObj->HasComponent<CAkEmitter>()) || rPostMIDI.gameObjID == AK_INVALID_GAME_OBJECT)// Avoid performing if the GameObject was specified but does not exist.
				{
					ProcessAllActions(rPostMIDI.pEvent, rPostMIDI, pGameObj);
				}
				else
				{
					if(!pGameObj)
					{
						MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_EventPostMIDI, AK_INVALID_PLAYING_ID, rPostMIDI.gameObjID, rPostMIDI.pEvent->ID(), false);
					}
					else
					{
						MONITOR_ERROREX(AK::Monitor::ErrorCode_GameObjectIsNotEmitterEvent, AK_INVALID_PLAYING_ID, rPostMIDI.gameObjID, rPostMIDI.pEvent->ID(), false);
					}
				}

				if(pGameObj)
				{
					pGameObj->Release();
				}

				rPostMIDI.pEvent->Release();
			}
			break;

			case QueuedMsgType_EventStopMIDI:
			{
				AkQueuedMsg_EventStopMIDI& rStopMIDI = pItem->stopMIDI;

				CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(rStopMIDI.gameObjID);
				if((pGameObj && pGameObj->HasComponent<CAkEmitter>()) || rStopMIDI.gameObjID == AK_INVALID_GAME_OBJECT)// Avoid performing if the GameObject was specified but does not exist.
				{
					ProcessAllActions(rStopMIDI.pEvent, rStopMIDI, pGameObj);
				}
				else
				{
					if(!pGameObj)
					{
						MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_EventStopMIDI, AK_INVALID_PLAYING_ID, rStopMIDI.gameObjID, rStopMIDI.pEvent ? rStopMIDI.pEvent->ID() : AK_INVALID_UNIQUE_ID, false);
					}
					else
					{
						MONITOR_ERROREX(AK::Monitor::ErrorCode_GameObjectIsNotEmitterEvent, AK_INVALID_PLAYING_ID, rStopMIDI.gameObjID, rStopMIDI.pEvent ? rStopMIDI.pEvent->ID() : AK_INVALID_UNIQUE_ID, false);
					}

				}

				if(pGameObj)
				{
					pGameObj->Release();
				}

				if(rStopMIDI.pEvent)
					rStopMIDI.pEvent->Release();
			}
			break;

			case QueuedMsgType_LockUnlockStreamCache:
			{
				CAkRegisteredObj * pGameObj = NULL;
				if(pItem->lockUnlockStreamCache.gameObjID != AK_INVALID_GAME_OBJECT)
				{
					pGameObj = g_pRegistryMgr->GetObjAndAddref(pItem->lockUnlockStreamCache.gameObjID);
					if(pGameObj == NULL)
					{
						//Bail out
						MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_LockUnlockStreamCache, AK_INVALID_PLAYING_ID, pItem->seek.gameObjID, pItem->seek.eventID, false);
						break;
					}
				}

				AKASSERT(pItem->lockUnlockStreamCache.gameObjID == AK_INVALID_GAME_OBJECT || pGameObj != NULL);

				// Lock or Unlock
				if(pItem->lockUnlockStreamCache.Lock)
					PinFilesInStreamCache(pItem->lockUnlockStreamCache.pEvent, pGameObj, pItem->lockUnlockStreamCache.ActivePriority, pItem->lockUnlockStreamCache.InactivePriority);
				else
					UnpinFilesInStreamCache(pItem->lockUnlockStreamCache.pEvent, pGameObj);

				pItem->lockUnlockStreamCache.pEvent->Release();

				if(pGameObj)
					pGameObj->Release();

				break;
			}
			case QueuedMsgType_Seek:
			{
				CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(pItem->seek.gameObjID);
				if((pGameObj && pGameObj->HasComponent<CAkEmitter>()) || pItem->seek.gameObjID == AK_INVALID_GAME_OBJECT)// Avoid performing if the GameObject was specified but does not exist.
				{
					AkQueuedMsg_Seek & rSeek = pItem->seek;
					// Find each action play in the list, and execute the action on their target.
					for(CAkEvent::AkActionList::Iterator iter = rSeek.pEvent->m_actions.Begin(); iter != rSeek.pEvent->m_actions.End(); ++iter)
					{
						CAkAction* pAction = *iter;
						AKASSERT(pAction);

						AkActionType aType = pAction->ActionType();
						if(aType == AkActionType_Play)
						{
							CAkActionPlay* pActionPlay = static_cast<CAkActionPlay*>(pAction);
							CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef(pActionPlay->ElementID(), AkNodeType_Default);

							if(pNode)
							{
								SeekActionParams l_Params;
								l_Params.bIsFromBus = false;
								l_Params.bIsMasterResume = false;
								l_Params.transParams.eFadeCurve = AkCurveInterpolation_Linear;
								l_Params.pGameObj = pGameObj;
								l_Params.playingID = rSeek.playingID;
								l_Params.transParams.TransitionTime = 0;
								l_Params.bIsMasterCall = false;
								l_Params.bIsSeekRelativeToDuration = pItem->seek.bIsSeekRelativeToDuration;
								l_Params.bSnapToNearestMarker = pItem->seek.bSnapToMarker;
								l_Params.iSeekTime = rSeek.iPosition;
								l_Params.eType = ActionParamType_Seek;

								// Note: seeking of pending actions is useless, counterintuitive and thus not supported

								pNode->ExecuteAction(l_Params);
								pNode->Release();
							}
						}
					}
				}
				else
				{
					if(!pGameObj)
					{
						MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_Seek, AK_INVALID_PLAYING_ID, pItem->seek.gameObjID, pItem->seek.eventID, false);
					}
					else
					{
						MONITOR_ERROREX(AK::Monitor::ErrorCode_GameObjectIsNotEmitterEvent, AK_INVALID_PLAYING_ID, pItem->seek.gameObjID, pItem->seek.eventID, false);
					}

				}

				if(pGameObj)
				{
					pGameObj->Release();
				}

				pItem->seek.pEvent->Release();
			}
			break;

			case QueuedMsgType_RTPC:
				/// WARNING: Do not forget to keep QueuedMsgType_RTPCWithTransition in sync.
			{
				TransParams transParams;
				if(pItem->rtpc.gameObjID == AK_INVALID_GAME_OBJECT)
				{
					g_pRTPCMgr->SetRTPCInternal(pItem->rtpc.ID, pItem->rtpc.Value, AkRTPCKey(), transParams);
				}
				else
				{
					CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(pItem->rtpc.gameObjID);
					if(pGameObj)
					{
						g_pRTPCMgr->SetRTPCInternal(pItem->rtpc.ID, pItem->rtpc.Value, pGameObj, pItem->rtpc.PlayingID, transParams);
						pGameObj->Release();
					}
					else
					{
						MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_RTPC, pItem->rtpc.PlayingID, pItem->rtpc.gameObjID, AK_INVALID_UNIQUE_ID, false);
					}
				}
			}
			break;

			case QueuedMsgType_RTPCWithTransition:

				if(pItem->rtpc.gameObjID == AK_INVALID_GAME_OBJECT)
				{
					g_pRTPCMgr->SetRTPCInternal(pItem->rtpcWithTransition.ID, pItem->rtpcWithTransition.Value, AkRTPCKey(), pItem->rtpcWithTransition.transParams);
				}
				else
				{
					CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(pItem->rtpc.gameObjID);
					if(pGameObj)
					{
						g_pRTPCMgr->SetRTPCInternal(pItem->rtpcWithTransition.ID, pItem->rtpcWithTransition.Value, pGameObj, pItem->rtpc.PlayingID, pItem->rtpcWithTransition.transParams);
						pGameObj->Release();
					}
					else
					{
						MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_RTPCWithTransition, AK_INVALID_PLAYING_ID, pItem->rtpcWithTransition.gameObjID, AK_INVALID_UNIQUE_ID, false);
					}
				}
				break;

			case QueuedMsgType_State:
				g_pStateMgr->SetStateInternal(
					pItem->setstate.StateGroupID,
					pItem->setstate.StateID,
					pItem->setstate.bSkipTransition != 0,
					pItem->setstate.bSkipExtension != 0);
				break;

			case QueuedMsgType_Switch:
			{
				CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(pItem->setswitch.gameObjID);
				if(pGameObj)
				{
					g_pSwitchMgr->SetSwitchInternal(
						pItem->setswitch.SwitchGroupID,
						pItem->setswitch.SwitchStateID,
						pGameObj);
					pGameObj->Release();
				}
				else
				{
					MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_Switch, AK_INVALID_PLAYING_ID, pItem->setswitch.gameObjID, AK_INVALID_UNIQUE_ID, false);
				}
			}
			break;

			case QueuedMsgType_Trigger:
			{
				if(pItem->trigger.gameObjID == AK_INVALID_GAME_OBJECT)
					g_pStateMgr->Trigger(pItem->trigger.TriggerID, NULL);
				else
				{
					CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(pItem->trigger.gameObjID);
					if(pGameObj)
					{
						g_pStateMgr->Trigger(pItem->trigger.TriggerID, pGameObj);
						pGameObj->Release();
					}
					else
					{
						MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_Trigger, AK_INVALID_PLAYING_ID, pItem->trigger.gameObjID, AK_INVALID_UNIQUE_ID, false);
					}
				}
			}
			break;

			case QueuedMsgType_RegisterGameObj:
			{
				g_pRegistryMgr->RegisterObject(pItem->reggameobj.gameObjID, pItem->reggameobj.pMonitorData);
			}
			break;

			case QueuedMsgType_UnregisterGameObj:
				if(pItem->unreggameobj.gameObjID == AK_INVALID_GAME_OBJECT)
					g_pRegistryMgr->UnregisterAll(NULL);
				else
					g_pRegistryMgr->UnregisterObject(pItem->unreggameobj.gameObjID);
				break;

			case QueuedMsgType_GameObjPosition:
			{
				AkChannelEmitter emitter;
				emitter.position = pItem->gameobjpos.Position;
				emitter.uInputChannels = AK_SPEAKER_SETUP_ALL_SPEAKERS;
				if(AK_EXPECT_FALSE(g_pRegistryMgr->SetPosition(
					pItem->gameobjpos.gameObjID,
					&emitter,
					1,
					AK::SoundEngine::MultiPositionType_SingleSource
					) != AK_Success))
				{
					MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_GameObjPosition, AK_INVALID_PLAYING_ID, pItem->gameobjpos.gameObjID, AK_INVALID_UNIQUE_ID, false);
				}
				break;
			}
			case QueuedMsgType_GameObjMultiPosition:
				if(AK_EXPECT_FALSE(g_pRegistryMgr->SetPosition(
					pItem->gameObjMultiPos.gameObjID,
					pItem->gameObjMultiPos.aMultiPosition,
					pItem->gameObjMultiPos.uNumPositions,
					pItem->gameObjMultiPos.eMultiPositionType
					) != AK_Success))
				{
					MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_GameObjMultiPosition, AK_INVALID_PLAYING_ID, pItem->gameObjMultiPos.gameObjID, AK_INVALID_UNIQUE_ID, false);
				}
				break;

			case QueuedMsgType_GameObjActiveListeners:
			{
				AkAutoTermListenerSet listeners;
				GetListeners(pItem->gameobjactlist, listeners);

				g_pRegistryMgr->UpdateListeners(pItem->gameobjactlist.gameObjID, listeners, (AkListenerOp)pItem->gameobjactlist.uOp);
			}
			break;

			case QueuedMsgType_DefaultActiveListeners:
			{
				AkAutoTermListenerSet listeners;
				GetListeners(pItem->defaultactlist, listeners);
				
				g_pRegistryMgr->UpdateDefaultListeners( listeners, (AkListenerOp)pItem->defaultactlist.uOp );
			}
			break;


			case QueuedMsgType_ResetListeners:
			{
				g_pRegistryMgr->ResetListenersToDefault(pItem->resetListeners.gameObjID);
			}
			break;

			case QueuedMsgType_SetOutputVolume:
			{
				AkDevice* pDevice = CAkOutputMgr::GetDevice(pItem->setoutputvolume.idDevice);
				if(pDevice)
				{
					pDevice->SetNextVolume(pItem->setoutputvolume.fVolume);
				}
				else
				{
					MONITOR_ERRORMSG2(AKTEXT("Error! Could not set volume for output device."), AKTEXT(""));
				}
			}
			break;

			case QueuedMsgType_ListenerSpatialization:
			{
				AkChannelConfig channelConfig;
				channelConfig.Deserialize(pItem->listspat.uChannelConfig);
				if(CAkListener::SetListenerSpatialization(
					pItem->listspat.listener.gameObjID,
					pItem->listspat.bSpatialized != 0,
					channelConfig,
					pItem->listspat.bSetVolumes != 0 ? (AK::SpeakerVolumes::VectorPtr)pItem->listspat.pVolumes : NULL) == AK_InvalidParameter)
				{
					MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_ListenerSpatialization, AK_INVALID_PLAYING_ID, pItem->listspat.listener.gameObjID, AK_INVALID_UNIQUE_ID, false);
				}
				if(pItem->listspat.pVolumes)
					AK::SpeakerVolumes::Vector::Free((AK::SpeakerVolumes::VectorPtr&)pItem->listspat.pVolumes, AkMemID_Object);
			}
			break;

			case QueuedMsgType_GameObjEnvValues:
				g_pRegistryMgr->SetGameObjectAuxSendValues(
					pItem->gameobjenvvalues.gameObjID,
					pItem->gameobjenvvalues.pEnvValues,
					pItem->gameobjenvvalues.uNumValues);
				break;

			case QueuedMsgType_GameObjDryLevel:
				g_pRegistryMgr->SetGameObjectOutputBusVolume(
					pItem->gameobjdrylevel.emitter.gameObjID,
					pItem->gameobjdrylevel.listener.gameObjID,
					pItem->gameobjdrylevel.fValue);
				break;

			case QueuedMsgType_GameObjScalingFactor:
				if(AK_EXPECT_FALSE(g_pRegistryMgr->SetGameObjectScalingFactor(
					pItem->gameobjscalingfactor.gameObjID,
					pItem->gameobjscalingfactor.fValue) != AK_Success))
				{
					MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_GameObjScalingFactor, AK_INVALID_PLAYING_ID, pItem->gameobjscalingfactor.gameObjID, AK_INVALID_UNIQUE_ID, false);
				}
				break;

			case QueuedMsgType_GameObjObstruction:
				g_pRegistryMgr->SetObjectObstructionAndOcclusion(
					pItem->gameobjobstr.emitter.gameObjID,
					pItem->gameobjobstr.listener.gameObjID,
					pItem->gameobjobstr.fObstructionLevel,
					pItem->gameobjobstr.fOcclusionLevel);			
				break;

			case QueuedMsgType_GameObjMultipleObstruction:
				AKASSERT(pItem->gameobjmultiobstr.pObstructionOcclusionValues != NULL);
				g_pRegistryMgr->SetMultipleObstructionAndOcclusion(
					pItem->gameobjmultiobstr.emitter.gameObjID,
					pItem->gameobjmultiobstr.listener.gameObjID,
					pItem->gameobjmultiobstr.pObstructionOcclusionValues,
					pItem->gameobjmultiobstr.uNumObstructionOcclusionValues);
				break;

#ifndef AK_OPTIMIZED
			case QueuedMsgType_ResetSwitches:
				if(pItem->resetswitches.gameObjID == AK_INVALID_GAME_OBJECT)
					g_pSwitchMgr->ResetSwitches();
				else
				{
					CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(pItem->resetswitches.gameObjID);
					if(pGameObj)
					{
						g_pSwitchMgr->ResetSwitches(pGameObj);
						pGameObj->Release();
					}
					else
					{
						MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_ResetSwitches, AK_INVALID_PLAYING_ID, pItem->resetswitches.gameObjID, AK_INVALID_UNIQUE_ID, false);
					}
				}
				break;

			case QueuedMsgType_ResetRTPC:
				if(pItem->resetrtpc.gameObjID == AK_INVALID_GAME_OBJECT)
					g_pRTPCMgr->ResetRTPC(NULL, pItem->resetrtpc.PlayingID);
				else
				{
					CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(pItem->resetrtpc.gameObjID);
					if(pGameObj)
					{
						g_pRTPCMgr->ResetRTPC(pGameObj, pItem->resetrtpc.PlayingID);
						pGameObj->Release();
					}
					else
					{
						MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_ResetRTPC, pItem->resetrtpc.PlayingID, pItem->resetrtpc.gameObjID, AK_INVALID_UNIQUE_ID, false);
					}
				}
				break;
#endif

			case QueuedMsgType_ResetRTPCValue:
				/// WARNING: Do not forget to keep QueuedMsgType_ResetRTPCValueWithTransition in sync.
				if(pItem->resetrtpcvalue.gameObjID == AK_INVALID_GAME_OBJECT)
				{
					// No transition.
					TransParams transParams;
					g_pRTPCMgr->ResetRTPCValue(pItem->resetrtpcvalue.ParamID, NULL, AK_INVALID_PLAYING_ID, transParams);
				}
				else
				{
					CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(pItem->resetrtpcvalue.gameObjID);
					if(pGameObj)
					{
						// No transition.
						TransParams transParams;
						g_pRTPCMgr->ResetRTPCValue(pItem->resetrtpcvalue.ParamID, pGameObj, pItem->resetrtpcvalue.PlayingID, transParams);
						pGameObj->Release();
					}
					else
					{
						MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_ResetRTPCValue, AK_INVALID_PLAYING_ID, pItem->resetrtpcvalue.gameObjID, AK_INVALID_UNIQUE_ID, false);
					}
				}
				break;

			case QueuedMsgType_ResetRTPCValueWithTransition:
				if(pItem->resetrtpcvalue.gameObjID == AK_INVALID_GAME_OBJECT)
					g_pRTPCMgr->ResetRTPCValue(pItem->resetrtpcvalueWithTransition.ParamID, NULL, AK_INVALID_PLAYING_ID, pItem->resetrtpcvalueWithTransition.transParams);
				else
				{
					CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(pItem->resetrtpcvalue.gameObjID);
					if(pGameObj)
					{
						g_pRTPCMgr->ResetRTPCValue(pItem->resetrtpcvalueWithTransition.ParamID, pGameObj, pItem->resetrtpcvalue.PlayingID, pItem->resetrtpcvalueWithTransition.transParams);
						pGameObj->Release();
					}
					else
					{
						MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_ResetRTPCValueWithTransition, AK_INVALID_PLAYING_ID, pItem->resetrtpcvalueWithTransition.gameObjID, AK_INVALID_UNIQUE_ID, false);
					}
				}
				break;

				///////////////////////////////////////
				// Dynamic Sequence Related messages
				/////////////////////////////////////
			case QueuedMsgType_OpenDynamicSequence:
			{
				CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(pItem->opendynamicsequence.gameObjID);
				if(pGameObj)
				{
					pItem->opendynamicsequence.pDynamicSequence->SetGameObject(pGameObj);
					pGameObj->Release();
				}
				else if(pItem->opendynamicsequence.gameObjID != AK_INVALID_GAME_OBJECT)
				{
					MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_OpenDynamicSequence, pItem->opendynamicsequence.PlayingID, pItem->opendynamicsequence.gameObjID, AK_INVALID_UNIQUE_ID, false);
				}
			}
			break;

			case QueuedMsgType_DynamicSequenceCmd:
				switch(pItem->dynamicsequencecmd.eCommand)
				{
				case AkQueuedMsg_DynamicSequenceCmd::Play:
					pItem->dynamicsequencecmd.pDynamicSequence->Play(pItem->dynamicsequencecmd.uTransitionDuration, pItem->dynamicsequencecmd.eFadeCurve);
					break;

				case AkQueuedMsg_DynamicSequenceCmd::Pause:
					pItem->dynamicsequencecmd.pDynamicSequence->Pause(pItem->dynamicsequencecmd.uTransitionDuration, pItem->dynamicsequencecmd.eFadeCurve);
					break;

				case AkQueuedMsg_DynamicSequenceCmd::Resume:
					pItem->dynamicsequencecmd.pDynamicSequence->Resume(pItem->dynamicsequencecmd.uTransitionDuration, pItem->dynamicsequencecmd.eFadeCurve);
					break;

				case AkQueuedMsg_DynamicSequenceCmd::ResumeWaiting:
					pItem->dynamicsequencecmd.pDynamicSequence->ResumeWaiting();
					break;

				case AkQueuedMsg_DynamicSequenceCmd::Stop:
					pItem->dynamicsequencecmd.pDynamicSequence->Stop(pItem->dynamicsequencecmd.uTransitionDuration, pItem->dynamicsequencecmd.eFadeCurve);
					break;

				case AkQueuedMsg_DynamicSequenceCmd::Break:
					pItem->dynamicsequencecmd.pDynamicSequence->Break();
					break;				

				case AkQueuedMsg_DynamicSequenceCmd::Close:
					pItem->dynamicsequencecmd.pDynamicSequence->Close();

					// Ok, we can not get rid of the playing id for which we add an active count in the OpenDynamicSequence
					g_pPlayingMgr->RemoveItemActiveCount(pItem->dynamicsequencecmd.pDynamicSequence->GetPlayingID());

					// Release for the creation by the OpenDynamicSequence message
					pItem->dynamicsequencecmd.pDynamicSequence->Release();
					break;
				}

				// Release for enqueue item				
				pItem->dynamicsequencecmd.pDynamicSequence->Release();
				break;
			case QueuedMsgType_DynamicSequenceSeek:
				pItem->dynamicsequencecmd.pDynamicSequence->Seek(pItem->dynamicsequenceSeek);
				pItem->dynamicsequencecmd.pDynamicSequence->Release();
				break;

			case QueuedMsgType_StopAll:
			{
				AkGameObjectID gameObjectID = pItem->stopAll.gameObjID;

				AkActionType actionType = gameObjectID == AK_INVALID_GAME_OBJECT ? AkActionType_Stop_ALL : AkActionType_Stop_ALL_O;
				CAkActionStop* pStopAction = CAkActionStop::Create(actionType);
				if(pStopAction)
				{
					if(gameObjectID != AK_INVALID_GAME_OBJECT)
					{
						CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(gameObjectID);
						if(pGameObj)
						{
							AkPendingAction PendingAction(pGameObj);
							pStopAction->Execute(&PendingAction);
							pGameObj->Release();
						}
						else
						{
							MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_StopAll, AK_INVALID_PLAYING_ID, gameObjectID, AK_INVALID_UNIQUE_ID, false);
						}
					}
					else
					{
						AkPendingAction PendingAction(NULL);// dummy with no game object.
						pStopAction->Execute(&PendingAction);
					}
					pStopAction->Release();
				}
			}
			break;

			case QueuedMsgType_StopPlayingID:
			{
				ClearPendingItems(pItem->stopEvent.playingID);

				ActionParams l_Params;
				l_Params.bIsFromBus = false;
				l_Params.bIsMasterResume = false;
				l_Params.transParams.eFadeCurve = pItem->stopEvent.eFadeCurve;
				l_Params.eType = ActionParamType_Stop;
				l_Params.pGameObj = NULL;
				l_Params.playingID = pItem->stopEvent.playingID;
				l_Params.transParams.TransitionTime = pItem->stopEvent.uTransitionDuration;
				l_Params.bIsMasterCall = false;

				CAkBus::ExecuteMasterBusAction(l_Params);
			}
			break;

			case QueuedMsgType_PlayingIDAction:
			{
				ActionParams l_Params;
				switch (pItem->playingIDAction.eActionToExecute)
				{
				case AK::SoundEngine::AkActionOnEventType_Stop:
					l_Params.eType = ActionParamType_Stop;
					ClearPendingItems(pItem->playingIDAction.playingID);
					break;
				case AK::SoundEngine::AkActionOnEventType_Pause:
					l_Params.eType = ActionParamType_Pause;
					PausePendingItems(pItem->playingIDAction.playingID);
					break;
				case AK::SoundEngine::AkActionOnEventType_Resume:
					l_Params.eType = ActionParamType_Resume;
					ResumePausedPendingItems(pItem->playingIDAction.playingID);
					break;
				case AK::SoundEngine::AkActionOnEventType_Break:
					l_Params.eType = ActionParamType_Break;
					ClearPendingItems(pItem->playingIDAction.playingID);
					break;
				case AK::SoundEngine::AkActionOnEventType_ReleaseEnvelope:
					l_Params.eType = ActionParamType_Release;
					break;
				}

				l_Params.bIsFromBus = false;
				l_Params.bIsMasterResume = false;
				l_Params.transParams.eFadeCurve = pItem->playingIDAction.eFadeCurve;
				l_Params.pGameObj = NULL;
				l_Params.playingID = pItem->playingIDAction.playingID;
				l_Params.transParams.TransitionTime = pItem->playingIDAction.uTransitionDuration;
				l_Params.bIsMasterCall = false;

				CAkBus::ExecuteMasterBusAction(l_Params);
			}
			break;

			case QueuedMsgType_KillBank:
			{
				CAkUsageSlot* pUsageSlot = pItem->killbank.pUsageSlot;
				AKASSERT(pUsageSlot);

				if(g_pExternalUnloadBankHandlerCallback)
				{
					g_pExternalUnloadBankHandlerCallback(pUsageSlot);
				}
				CAkURenderer::StopAllPBIs(pUsageSlot);
				pUsageSlot->Release(false);
			}
			break;
			case QueuedMsgType_AddOutput:
			{
				AkOutputSettings* pSettings = (AkOutputSettings*)pItem->addOutput.pSettings;
				AkAutoTermListenerSet listeners;
				GetListeners(pItem->addOutput, listeners);

				if(CAkOutputMgr::AddOutputDevice(*pSettings, listeners) != AK_Success)
				{
					MONITOR_ERRORMSG2(AKTEXT("Error! Initialization of output device failed."), AKTEXT(""));
				}

				break;
			}
			case QueuedMsgType_RemoveOutput:
			{
				CAkOutputMgr::RemoveOutputDevice(pItem->removeOutput.idOutput);
				break;
			}
			case QueuedMsgType_ReplaceOutput:
			{
				if (CAkOutputMgr::ReplaceDevice(*(AkOutputSettings*)(&pItem->replaceOutput.pSettings), pItem->replaceOutput.idOutput) == AK_Success)
				{
					// Since the Sink Init can be lengthy,
					// we set the time last buffer to after a successful init to prevent timeout
					StampTimeLastBuffer();
				}
				out_bDeviceSwitch = true;
				break;
			}

			case QueuedMsgType_SetEffect:
			{
				CAkParameterNodeBase * pNode = g_pIndex->GetNodePtrAndAddRef(pItem->setEffect.audioNodeID, pItem->setEffect.eNodeType);
				if(pNode)
				{
					AkNodeCategory eCat = pNode->NodeCategory();
					if(eCat == AkNodeCategory_Bus
						|| eCat == AkNodeCategory_AuxBus
						|| eCat == AkNodeCategory_ActorMixer
						|| eCat == AkNodeCategory_RanSeqCntr
						|| eCat == AkNodeCategory_Sound
						|| eCat == AkNodeCategory_SwitchCntr
						|| eCat == AkNodeCategory_LayerCntr
						|| eCat == AkNodeCategory_MusicTrack
						|| eCat == AkNodeCategory_MusicSegment
						|| eCat == AkNodeCategory_MusicRanSeqCntr
						|| eCat == AkNodeCategory_MusicSwitchCntr)
					{
						CAkParameterNodeBase * pParamNode = (CAkParameterNodeBase *)pNode;

						if(pParamNode->SetFX(pItem->setEffect.uFXIndex, pItem->setEffect.shareSetID, true, SharedSetOverride_SDK) != AK_Success)
						{
							MONITOR_ERRORMSG(AKTEXT("SetEffect: set effect failed"));
						}
					}
					else
					{
						MONITOR_ERRORMSG(AKTEXT("SetEffect: audio node type not supported"));
					}
					pNode->Release();
				}
				else
				{
					MONITOR_ERRORMSG(AKTEXT("SetEffect: audio node not found"));
				}
			}
			break;

			case QueuedMsgType_SetMixer:
			{
				CAkParameterNodeBase * pNode = g_pIndex->GetNodePtrAndAddRef(pItem->setEffect.audioNodeID, pItem->setEffect.eNodeType);
				if(pNode)
				{
					AkNodeCategory eCat = pNode->NodeCategory();
					if(eCat == AkNodeCategory_Bus
						|| eCat == AkNodeCategory_AuxBus)
					{
						CAkParameterNodeBase * pParamNode = (CAkParameterNodeBase *)pNode;

						if(pParamNode->SetMixerPlugin(pItem->setEffect.shareSetID, true, SharedSetOverride_SDK) != AK_Success)
						{
							MONITOR_ERRORMSG(AKTEXT("SetMixer: SetMixerPlugin failed"));
						}
					}
					else
					{
						MONITOR_ERRORMSG(AKTEXT("SetMixer: audio node type not supported"));
					}
					pNode->Release();
				}
				else
				{
					MONITOR_ERRORMSG(AKTEXT("SetMixer: audio node not found"));
				}
			}
			break;

			case QueuedMsgType_SetBusConfig:
			{
				CAkParameterNodeBase * pNode = g_pIndex->GetNodePtrAndAddRef(pItem->setBusConfig.audioNodeID, AkNodeType_Bus);
				if(pNode)
				{
					AKASSERT(pNode->NodeCategory() == AkNodeCategory_Bus || pNode->NodeCategory() == AkNodeCategory_AuxBus);
					CAkBus * pBus = (CAkBus*)pNode;
					AkChannelConfig channelConfig;
					channelConfig.Deserialize(pItem->setBusConfig.uChannelConfig);
					if(!pBus->IsTopBus())
						pBus->ChannelConfig(channelConfig);
					else
					{
						MONITOR_ERRORMSG(AKTEXT("SetBusConfig failed: cannot set master bus config"));
					}
					pNode->Release();
				}
				else
				{
					MONITOR_ERRORMSG(AKTEXT("SetBusConfig: Bus not found"));
				}
			}
			break;

			case QueuedMsgType_SetPanningRule:
				CAkLEngine::SetPanningRule(pItem->setPanningRule.idOutput, pItem->setPanningRule.panRule);
				break;

			case QueuedMsgType_SetSpeakerAngles:
			{
				AKASSERT(pItem->setSpeakerAngles.pfSpeakerAngles);
				AkDevice * pDevice = CAkOutputMgr::GetDevice(pItem->setSpeakerAngles.idOutput);
				if(pDevice)
				{
					if(pDevice->SetSpeakerAngles(pItem->setSpeakerAngles.pfSpeakerAngles, pItem->setSpeakerAngles.uNumAngles, pItem->setSpeakerAngles.fHeightAngle) != AK_Success)
					{
						MONITOR_ERRORMSG(AKTEXT("SetSpeakerAngles failed"));
					}
				}
				else
				{
					MONITOR_ERRORMSG(AKTEXT("SetSpeakerAngles: Device not found"));
				}
				AkFree(AkMemID_Object, pItem->setSpeakerAngles.pfSpeakerAngles);
			}
			break;

			case QueuedMsgType_StartStopOutputCapture:
			{
				if(pItem->outputCapture.szFileName != (AkUIntPtr)NULL)
				{
					CAkOutputMgr::StartOutputCapture(pItem->outputCapture.szFileName);
					AkFree(AkMemID_Object, pItem->outputCapture.szFileName);
				}
				else
				{
					CAkOutputMgr::StopOutputCapture();
				}
			}
			break;

			case QueuedMsgType_AddOutputCaptureMarker:
			{
				CAkOutputMgr::AddOutputCaptureMarker(pItem->captureMarker.szMarkerText);

				AkFree(AkMemID_Object, pItem->captureMarker.szMarkerText);
			}
			break;

			case QueuedMsgType_SetRandomSeed:
				AKRANDOM::AkRandomInit(pItem->randomSeed.uSeed);
				break;

			case QueuedMsgType_MuteBackgroundMusic:
				if(pItem->muteBGM.bMute)
					CAkBus::MuteBackgroundMusic();
				else
					CAkBus::UnmuteBackgroundMusic();
				break;

			case QueuedMsgType_PluginCustomGameData:
				AkCustomPluginDataStore::SetPluginCustomGameData(pItem->pluginCustomGameData.busID, pItem->pluginCustomGameData.busObjectID, pItem->pluginCustomGameData.eType, pItem->pluginCustomGameData.uCompanyID, pItem->pluginCustomGameData.uPluginID, pItem->pluginCustomGameData.pData, pItem->pluginCustomGameData.uSizeInBytes, true);
				break;

			case QueuedMsgType_SuspendOrWakeup:
				if(CAkOutputMgr::SetDeviceSuspended(pItem->suspend.bSuspend != 0, pItem->suspend.bRender != 0, pItem->suspend.uDelay) == AK_Success)
					out_bDeviceSwitch = true;
#if defined(AK_COMM_DISABLE_ON_SUSPEND) && !defined(AK_OPTIMIZED)
				 if (!AK_PERF_OFFLINE_RENDERING && g_pCommCentral != nullptr)
				 {
				 	if (pItem->suspend.bSuspend != 0)
						g_pCommCentral->Suspend();
				 	else
						g_pCommCentral->Resume();
				 }
#endif
				break;
			case QueuedMsgType_InitSinkPlugin:
			{
				if (CAkOutputMgr::InitMainDevice(*(AkOutputSettings*)(&pItem->initSink)) == AK_Success) {
					// Since the Sink Init can be lengthy,
					// we set the time last buffer to after a successful init to prevent timeout
					StampTimeLastBuffer();
				}
				out_bDeviceSwitch = true;
			}
			break;
			case QueuedMsgType_ApiExtension:
			{
				AkMsgQueueHandlers* pHandler = m_MsgQueueHandlers.Exists(pItem->apiExtension.uID);
				if(pHandler != NULL)
					(pHandler->fcnExec)((void*)((&pItem->apiExtension) + 1), pItem->size - sizeof(AkQueuedMsg_ApiExtension));
			}
			break;		
			case QueuedMsgType_SetBusDevice:
			{
				CAkBus* pBus = (CAkBus*)(g_pIndex->GetNodeIndex(AkNodeType_Bus).GetPtrAndAddRef(pItem->setBusDevice.idBus));
				if(pBus)
				{
					pBus->SetBusDevice(pItem->setBusDevice.idDevice);
					pBus->Release();
				}
				break;
			}
			}

			AkUInt16 uMsgSize = pItem->size;
			m_MsgQueue.EndRead( uMsgSize );

			if ( in_bDrainMsgQueue )
			{
				uDrainedBytes += uMsgSize;
				bReachedEnd |= uDrainedBytes >= uBytesToDrain;
			}
		
			// For each event processed, increment the synchronization count.
			// for RTPC and switches too, since they can cause new playback to occur.
			CAkLEngineCmds::IncrementSyncCount();
		}
		else
		{
			m_MsgQueue.EndRead(m_MsgQueue.GetMaxQueueSize());	//Force wrap around.
		}
	}
	while( !bReachedEnd );

	return true;
}
//-----------------------------------------------------------------------------
// Name: ProcessPendingList
// Desc: figure out if some of those pending are ready to run.
//
// Parameters:
//	None.
//
// Return: 
//	None.
//----------------------------------------------------------------------------- 
bool CAkAudioMgr::ProcessPendingList()
{
	bool bPendingActionsProcessed = false;

	AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
	while( iter != m_mmapPending.End() )
	{
		// is it time to go ?
		if( (*iter).key <= m_uBufferTick)
		{
			AkPendingAction* pPendingAction = (*iter).item;
			m_mmapPending.Erase( iter );

			NotifyDelayEnded( pPendingAction );

			ProcessAction( pPendingAction );
			bPendingActionsProcessed = true;

			//increment the sync count for pending events too, so that they don't sync with next events.
			CAkLEngineCmds::IncrementSyncCount();
			
			iter = m_mmapPending.BeginEx();
		}
		else
        {
			break;
        }
	}

	return bPendingActionsProcessed;
}

//-----------------------------------------------------------------------------
// Name: EnqueueOrExecuteAction
// Desc: Enqueue or execute an action.
//
// Parameters:
//	AkPendingAction* in_pActionItem : Action to enqueue.
//
// Return: 
//	None.
//-----------------------------------------------------------------------------
void CAkAudioMgr::EnqueueOrExecuteAction( AkPendingAction* in_pActionItem, AkUInt32 in_uiFrameOffset)
{
	AKASSERT(in_pActionItem);
	AKASSERT(in_pActionItem->pAction);

	g_pPlayingMgr->AddItemActiveCount(in_pActionItem->UserParam.PlayingID());

	// make sure it won't get thrown away
	in_pActionItem->pAction->AddRef();

	AkUInt32 delayS = in_pActionItem->pAction->GetDelayTime() + in_uiFrameOffset;
	AkUInt32 delayTicks = delayS / AK_NUM_VOICE_REFILL_FRAMES;

	in_pActionItem->LaunchTick = m_uBufferTick;
	in_pActionItem->LaunchFrameOffset = delayS - delayTicks * AK_NUM_VOICE_REFILL_FRAMES;

	if( delayTicks == 0 )
	{
		ProcessAction( in_pActionItem );
	}
	else
	{
		// Special case for Play and Continue actions.
		// Post ahead of time to lower engine to reduce latency due to streaming.
		if ( in_pActionItem->pAction->ActionType() == AkActionType_PlayAndContinue )
		{
			AkUInt32 uNumFramesLookAhead = AkMin( delayTicks, g_settings.uContinuousPlaybackLookAhead );
			delayTicks -= uNumFramesLookAhead;

			in_pActionItem->LaunchFrameOffset = delayS - delayTicks * AK_NUM_VOICE_REFILL_FRAMES;

			if ( delayTicks == 0 )
			{
				// Action should be processed now after all. Process and bail out.
				ProcessAction( in_pActionItem ); 
				return;
			}
		}
		in_pActionItem->LaunchTick += delayTicks;
		if( m_mmapPending.Insert( in_pActionItem->LaunchTick, in_pActionItem ) == AK_Success )
		{
			MONITOR_ACTIONDELAYED( in_pActionItem->UserParam.PlayingID(), in_pActionItem->pAction->ID(), in_pActionItem->GameObjID(), delayS * 1000 / AK_CORE_SAMPLERATE, in_pActionItem->UserParam.CustomParam() );
			NotifyDelayStarted( in_pActionItem );
		}
		else
		{
			FlushAndCleanPendingAction( in_pActionItem );
		}
	}
}

void CAkAudioMgr::FlushAndCleanPendingAction( AkPendingAction* in_pPendingAction )
{
	in_pPendingAction->pAction->Release();
	AkDelete(AkMemID_Object, in_pPendingAction);
}

//-----------------------------------------------------------------------------
// Name: ProcessActionQueue
// Desc: Process the action queue.
//
// Parameters:
//	None.
//
// Return: 
//	None.
//-----------------------------------------------------------------------------
void CAkAudioMgr::ProcessAction( AkPendingAction * in_pAction ) 
{
	AKASSERT( in_pAction->pAction );

	MONITOR_ACTIONTRIGGERED( in_pAction->UserParam.PlayingID(), in_pAction->pAction->ID(), in_pAction->GameObjID(), in_pAction->UserParam.CustomParam() );

	//Various actions will access RTPC values.  If these values are bound to built in parameters (distance, etc) we need to make sure that
	//	the cached position is updated, because this step is usually done after processing the message queue.
	if (in_pAction->GameObj())
	{
		CAkEmitter* pRegObj = in_pAction->GameObj()->GetComponent<CAkEmitter>();
		if (pRegObj != NULL)
			pRegObj->UpdateCachedPositions();
	}

	// execute it
	in_pAction->pAction->Execute( in_pAction );

	// Important to do it AFTER calling execute, that will avoid having the Playing mgr thinking that the Playing ID is dead.
	if( in_pAction->UserParam.PlayingID() )
	{
		g_pPlayingMgr->RemoveItemActiveCount( in_pAction->UserParam.PlayingID() );
	}

	// we don't care anymore about that one
	in_pAction->pAction->Release();

	// get rid of it
	AkDelete(AkMemID_Object, in_pAction);
}

//-----------------------------------------------------------------------------
// Name: ProcessCustomAction
//-----------------------------------------------------------------------------
void CAkAudioMgr::ProcessCustomAction( 
		CAkParameterNodeBase* ptargetNode, 
		CAkRegisteredObj * in_pGameObj, 
		AkUInt32 in_ActionToExecute,
		AkTimeMs in_uTransitionDuration,
		AkCurveInterpolation in_eFadeCurve,
		AkPlayingID	in_PlayingID
	)
{
	if( ptargetNode )
	{
		ActionParams l_Params;
		l_Params.bIsFromBus = false;
		l_Params.bIsMasterResume = false;
		l_Params.transParams.eFadeCurve = in_eFadeCurve;
		l_Params.pGameObj = in_pGameObj;
		l_Params.playingID = in_PlayingID;
		l_Params.transParams.TransitionTime = in_uTransitionDuration;
		l_Params.bIsMasterCall = false;
		l_Params.targetNodePtr = ptargetNode;

		switch( in_ActionToExecute )
		{
		case AK::SoundEngine::AkActionOnEventType_Stop:
			g_pAudioMgr->StopPendingAction( ptargetNode, in_pGameObj, in_PlayingID );
			l_Params.eType = ActionParamType_Stop;
			break;

		case AK::SoundEngine::AkActionOnEventType_Pause:
			g_pAudioMgr->PausePendingAction( ptargetNode, in_pGameObj, true, in_PlayingID );
			l_Params.eType = ActionParamType_Pause;
			break;

		case AK::SoundEngine::AkActionOnEventType_Resume:
			g_pAudioMgr->ResumePausedPendingAction( ptargetNode, in_pGameObj, false, in_PlayingID );
			l_Params.eType = ActionParamType_Resume;
			break;

		case AK::SoundEngine::AkActionOnEventType_Break:
			g_pAudioMgr->BreakPendingAction( ptargetNode, in_pGameObj, in_PlayingID );
			l_Params.eType = ActionParamType_Break;
			break;

		case AK::SoundEngine::AkActionOnEventType_ReleaseEnvelope:
			l_Params.eType = ActionParamType_Release;
			break;

		default:
			AKASSERT( !"Unexpected custom action type" );
			return;
		};

		ptargetNode->ExecuteAction( l_Params );
	}
}

void CAkAudioMgr::PostMIDIEvent( 
		CAkParameterNodeBase* in_pTargetNode, 
		CAkRegisteredObj* in_pGameObj, 
		AkQueuedMsg_EventPostMIDI& in_rPostMIDI
	)
{
	if( in_pTargetNode && in_pGameObj )
	{
		CAkMidiDeviceMgr* pMidiMgr = CAkMidiDeviceMgr::Get();
		if( pMidiMgr )
		{
			pMidiMgr->PostEvents( in_pTargetNode->ID(), in_pGameObj->ID(), in_rPostMIDI.aPosts, in_rPostMIDI.uNumPosts );
		}
	}
}

void CAkAudioMgr::PostMIDIStop( 
		CAkParameterNodeBase* in_pTargetNode, 
		CAkRegisteredObj* in_pGameObj
	)
{
	CAkMidiDeviceMgr* pMidiMgr = CAkMidiDeviceMgr::Get();
	if( pMidiMgr )
	{
		pMidiMgr->StopAll(
					in_pTargetNode ? in_pTargetNode->ID() : AK_INVALID_UNIQUE_ID,
					in_pGameObj ? in_pGameObj->ID() : AK_INVALID_GAME_OBJECT
					);
	}
}

void CAkAudioMgr::PlaySourceInput( AkUniqueID in_Target, CAkRegisteredObj* in_pGameObj, UserParams in_userParams )
{
    CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef( in_Target, AkNodeType_Default );

	if(pNode)
	{
		TransParams	Tparameters;

		Tparameters.TransitionTime = 0;
		Tparameters.eFadeCurve = AkCurveInterpolation_Linear;

		AkPBIParams pbiParams;
        
		pbiParams.eType = AkPBIParams::PBI;
        pbiParams.pInstigator = pNode;
        pbiParams.userParams = in_userParams;
		pbiParams.ePlaybackState = PB_Playing;
		pbiParams.uFrameOffset = 0;
        pbiParams.bIsFirst = true;

		pbiParams.pGameObj = in_pGameObj;

		pbiParams.pTransitionParameters = &Tparameters;
        pbiParams.pContinuousParams = NULL;
        pbiParams.sequenceID = AK_INVALID_SEQUENCE_ID;

		static_cast<CAkParameterNode*>(pNode)->Play( pbiParams );

		pNode->Release();
	}
	else
	{
		CAkCntrHist HistArray;
		MONITOR_OBJECTNOTIF( in_userParams.PlayingID(), in_pGameObj->ID(), in_userParams.CustomParam(), AkMonitorData::NotificationReason_PlayFailed, HistArray, in_Target, false, 0 );
        MONITOR_ERROREX( AK::Monitor::ErrorCode_SelectedNodeNotAvailablePlay, in_userParams.PlayingID(), in_pGameObj->ID(), in_Target, false );
	}
}

//-----------------------------------------------------------------------------
// Name: PausePendingAction
//-----------------------------------------------------------------------------
void CAkAudioMgr::PausePendingAction( CAkParameterNodeBase * in_pNodeToTarget, 
										  CAkRegisteredObj * in_GameObj,
										  bool		in_bIsMasterOnResume,
										  AkPlayingID in_PlayingID)
{
	AkPendingAction*	pThisAction = NULL;
	CAkAction*			pAction		= NULL;

	// scan'em all

	AkMultimapPausedPending::Iterator iterPaused = m_mmapPausedPending.Begin();
	while( iterPaused != m_mmapPausedPending.End() )
	{
		pThisAction = (*iterPaused).item;
		pAction = pThisAction->pAction;

		CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
		pTargetNode.Attach( pAction->GetAndRefTarget() );

		// is it ours ? we do pause Resume actions only if we are a master pause
		if(
			(!in_pNodeToTarget || IsElementOf( in_pNodeToTarget, pTargetNode ) )
			&& ( ( (pAction->ActionType() & ACTION_TYPE_ACTION) != ACTION_TYPE_RESUME) || in_bIsMasterOnResume )
			&& CheckObjAndPlayingID( in_GameObj, in_PlayingID, pThisAction )
			&& ( pAction->ActionType() != AkActionType_Duck )
			)
		{
			++(pThisAction->ulPauseCount);
		}

		++iterPaused;
	}

	AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
	while( iter != m_mmapPending.End() )
	{
		pThisAction = (*iter).item;
		pAction = pThisAction->pAction;
		CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
		pTargetNode.Attach( pAction->GetAndRefTarget() );

		// is it ours ? we do pause Resume actions only if we are a master pause
		if(
			(!in_pNodeToTarget || IsElementOf( in_pNodeToTarget, pTargetNode ) )
			&& ( ( (pAction->ActionType() & ACTION_TYPE_ACTION) != ACTION_TYPE_RESUME) || in_bIsMasterOnResume )
			&& CheckObjAndPlayingID( in_GameObj, in_PlayingID, pThisAction )
			&& ( pAction->ActionType() != AkActionType_Duck )
			)
		{
			InsertAsPaused( pAction->ElementID(), pThisAction );
			iter = m_mmapPending.Erase( iter );
		}
		else
		{
			++iter;
		}
	}
}

//-----------------------------------------------------------------------------
// Name: PausePendingAction
//-----------------------------------------------------------------------------
void CAkAudioMgr::PausePendingAction( CAkAction* in_pAction )
{
	AkPendingAction*	pThisAction = NULL;
	CAkAction*			pAction		= NULL;

	// scan'em all

	AkMultimapPausedPending::Iterator iterPaused = m_mmapPausedPending.Begin();
	while( iterPaused != m_mmapPausedPending.End() )
	{
		pThisAction = (*iterPaused).item;
		pAction = pThisAction->pAction;

		CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
		pTargetNode.Attach( pAction->GetAndRefTarget() );

		// is it ours ? we do pause Resume actions only if we are a master pause
		if( pAction == in_pAction )
		{
			++(pThisAction->ulPauseCount);
		}

		++iterPaused;
	}

	AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
	while( iter != m_mmapPending.End() )
	{
		pThisAction = (*iter).item;
		pAction = pThisAction->pAction;
		CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
		pTargetNode.Attach( pAction->GetAndRefTarget() );

		// is it ours ? we do pause Resume actions only if we are a master pause
		if( pAction == in_pAction )
		{
			InsertAsPaused( pAction->ElementID(), pThisAction );
			iter = m_mmapPending.Erase( iter );
		}
		else
		{
			++iter;
		}
	}
}
//--------------------------------------------------------------------------------------------
// Name: PausePendingItem
// Desc: move actions affecting an audionode (in_ulAudioNodeID) pending in the paused pending.
//
// Parameters:
//	AkUniqueID	 in_ulElementID		   :
//	CAkRegisteredObj * in_GameObj		       :
//-----------------------------------------------------------------------------
void CAkAudioMgr::PausePendingItems( AkPlayingID in_PlayingID )
{
	AkPendingAction*	pThisAction = NULL;
	CAkAction*			pAction		= NULL;

	// scan'em all

	AkMultimapPausedPending::Iterator iterPaused = m_mmapPausedPending.Begin();
	while( iterPaused != m_mmapPausedPending.End() )
	{
		pThisAction = (*iterPaused).item;

		// is it ours ? we do pause Resume actions only if they match our playing id
		if(	   ( pThisAction->UserParam.PlayingID() == in_PlayingID )
			&& ( pAction->ActionType() != AkActionType_Duck ) )
		{
			++(pThisAction->ulPauseCount);
		}

		++iterPaused;
	}

	AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
	while( iter != m_mmapPending.End() )
	{
		pThisAction = (*iter).item;
		pAction = pThisAction->pAction;

		// is it ours ? we do pause Resume actions only if they match our playing id
		if(	   ( pThisAction->UserParam.PlayingID() == in_PlayingID )
			&& ( pAction->ActionType() != AkActionType_Duck ) )
		{
			InsertAsPaused( pAction->ElementID(), pThisAction );
			iter = m_mmapPending.Erase( iter );
		}
		else
		{
			++iter;
		}
	}
}

void CAkAudioMgr::InsertAsPaused( AkUniqueID in_ElementID, AkPendingAction* in_pPendingAction, AkUInt32 in_ulPauseCount )
{
	in_pPendingAction->PausedTick = m_uBufferTick;
	in_pPendingAction->ulPauseCount = in_ulPauseCount;

	// shove the action in the PausedPending list
	AKRESULT eResult = m_mmapPausedPending.Insert( in_ElementID, in_pPendingAction );

	if( eResult == AK_Success )
	{
		CAkCntrHist HistArray;
		MONITOR_OBJECTNOTIF( in_pPendingAction->UserParam.PlayingID(), in_pPendingAction->GameObjID(), in_pPendingAction->UserParam.CustomParam(), AkMonitorData::NotificationReason_Paused, HistArray, in_pPendingAction->pAction->ID(), false, 0 );
	}
	else
	{
		MONITOR_ERRORMSG_PLAYINGID(AKTEXT("Pending action was destroyed because a critical memory allocation failed."), in_pPendingAction->UserParam.PlayingID());
		NotifyDelayAborted( in_pPendingAction, false );
		FlushAndCleanPendingAction( in_pPendingAction );
	}
}

void CAkAudioMgr::TransferToPending( AkPendingAction* in_pPendingAction )
{
	// offset the launch time by the pause duration
	in_pPendingAction->LaunchTick += ( m_uBufferTick - in_pPendingAction->PausedTick );

	// shove it action in the Pending list
	AKRESULT eResult = m_mmapPending.Insert( in_pPendingAction->LaunchTick, in_pPendingAction );

	if( eResult == AK_Success )
	{
		// Here we must send the resume notification anyway!, to balance the pause notification.
		CAkCntrHist HistArray;
		MONITOR_OBJECTNOTIF( in_pPendingAction->UserParam.PlayingID(), in_pPendingAction->GameObjID(), in_pPendingAction->UserParam.CustomParam(), AkMonitorData::NotificationReason_Resumed, HistArray, in_pPendingAction->pAction->ID(), false, 0 );
	}
	else
	{
		MONITOR_ERRORMSG_PLAYINGID(AKTEXT("Pending action was destroyed because a critical memory allocation failed."), in_pPendingAction->UserParam.PlayingID());
		NotifyDelayAborted( in_pPendingAction, true );
		FlushAndCleanPendingAction( in_pPendingAction );
	}
}

AKRESULT CAkAudioMgr::MidiNoteOffExecuted( CAkAction* in_pAction, 
										  CAkParameterNodeBase * in_pMidiTargetNode, 
										  MidiEventActionType in_eMidiAction )
{
	if (in_eMidiAction == MidiEventActionType_Break || in_eMidiAction == MidiEventActionType_Stop)
	{
		CAkAction* pAction;
		AkPendingAction* pPendingAction;
		{
			AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
			while( iter != m_mmapPending.End() )
			{
				pPendingAction = (*iter).item;
				pAction = pPendingAction->pAction;
				if( pAction == in_pAction )
				{	
					AKASSERT( pAction->ActionType() == AkActionType_PlayAndContinue );

					bool l_bFlush = true;

					if (in_eMidiAction == MidiEventActionType_Break)
						l_bFlush = static_cast<CAkActionPlayAndContinue*>( pPendingAction->pAction )->BreakToNode( in_pMidiTargetNode, pPendingAction->GameObj(), pPendingAction );

					if( l_bFlush )
					{
						NotifyDelayAborted( pPendingAction, false /* = was paused*/  );
						iter = FlushPendingItem( pPendingAction, m_mmapPending, iter );
					}
					else
					{
						++iter;
					}
				}
				else
				{
					++iter;
				}
			}
		}

		{
			AkMultimapPausedPending::IteratorEx iter = m_mmapPausedPending.BeginEx();
			while( iter != m_mmapPausedPending.End() )
			{
				pPendingAction = (*iter).item;
				pAction = pPendingAction->pAction;
				if( pAction == in_pAction )
				{
					AKASSERT( pAction->ActionType() == AkActionType_PlayAndContinue );
					bool l_bFlush = static_cast<CAkActionPlayAndContinue*>( pPendingAction->pAction )->BreakToNode( in_pMidiTargetNode, pPendingAction->GameObj(), pPendingAction );
					if( l_bFlush )
					{
						NotifyDelayAborted( pPendingAction, true /* = was paused*/ );
						iter = FlushPendingItem( pPendingAction, m_mmapPausedPending, iter );
					}
					else
					{
						++iter;
					}
				}
				else
				{
					++iter;
				}
			}
		}
	}

	return AK_Success;
}

//-----------------------------------------------------------------------------
// Name: BreakPendingAction
// Desc: Break pending actions.
//-----------------------------------------------------------------------------
AKRESULT CAkAudioMgr::BreakPendingAction( CAkParameterNodeBase * in_pNodeToTarget, 
										  CAkRegisteredObj * in_GameObj,
										  AkPlayingID in_PlayingID )
{
	AKRESULT	eResult = AK_Success;

	CAkAction* pAction;
	AkPendingAction* pPendingAction;
	{
		AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
		while( iter != m_mmapPending.End() )
		{
			pPendingAction = (*iter).item;
			pAction = pPendingAction->pAction;
			CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
			pTargetNode.Attach( pAction->GetAndRefTarget() );
			if(
				(!in_pNodeToTarget || IsElementOf( in_pNodeToTarget, pTargetNode ) )
				&& 
				CheckObjAndPlayingID( in_GameObj, in_PlayingID, pPendingAction )
				)
			{	
				bool l_bFlush = false;
				switch( pAction->ActionType() )
				{
				case AkActionType_PlayAndContinue:
					l_bFlush = static_cast<CAkActionPlayAndContinue*>( pPendingAction->pAction )->BreakToNode( in_pNodeToTarget, pPendingAction->GameObj(), pPendingAction );
					break;
				case AkActionType_Play:
					l_bFlush = true;
					break;
				default:
					break;
				}

				if( l_bFlush )
				{
					NotifyDelayAborted( pPendingAction, false );
					iter = FlushPendingItem( pPendingAction, m_mmapPending, iter );
				}
				else
				{
					++iter;
				}
			}
			else
			{
				++iter;
			}
		}
	}

	{
		AkMultimapPausedPending::IteratorEx iter = m_mmapPausedPending.BeginEx();
		while( iter != m_mmapPausedPending.End() )
		{
			pPendingAction = (*iter).item;
			pAction = pPendingAction->pAction;
			CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
			pTargetNode.Attach( pAction->GetAndRefTarget() );
			if(
				( !in_pNodeToTarget || IsElementOf( in_pNodeToTarget,pTargetNode ) )
				&& 
				CheckObjAndPlayingID( in_GameObj, in_PlayingID, pPendingAction )
				&& 
				( pAction->ActionType() == AkActionType_PlayAndContinue ) 
				)
			{
				bool l_bFlush = static_cast<CAkActionPlayAndContinue*>( pPendingAction->pAction )->BreakToNode( in_pNodeToTarget, pPendingAction->GameObj(), pPendingAction );
				if( l_bFlush )
				{
					NotifyDelayAborted( pPendingAction, true );
					iter = FlushPendingItem( pPendingAction, m_mmapPausedPending, iter );
				}
				else
				{
					++iter;
				}
			}
			else
			{
				++iter;
			}
		}
	}

	return eResult;
}

//-----------------------------------------------------------------------------
// Name: StopPendingAction
// Desc: Stop pending actions.
//
// Parameters:
// AkUniqueID   in_TargetID			  :
// CAkRegisteredObj * in_GameObj            :
//
// Return: 
//	AK_Success : Succeeded.
//  AK_Fail    : Failed.
//-----------------------------------------------------------------------------
AKRESULT CAkAudioMgr::StopPendingAction(  CAkParameterNodeBase * in_pNodeToTarget, 
										 CAkRegisteredObj * in_GameObj,
										 AkPlayingID in_PlayingID  )
{
	AKRESULT	eResult = AK_Success;

	CAkAction* pAction;
	AkPendingAction* pPendingAction;
	{
		AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
		while( iter != m_mmapPending.End() )
		{
			pPendingAction = (*iter).item;
			pAction = pPendingAction->pAction;
			CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
			pTargetNode.Attach( pAction->GetAndRefTarget() );
			if(
				(!in_pNodeToTarget || IsElementOf( in_pNodeToTarget, pTargetNode ) )
				&& 
				CheckObjAndPlayingID( in_GameObj, in_PlayingID, pPendingAction )
				&& 
				( pAction->ActionType() != AkActionType_Duck )
				)
			{
				NotifyDelayAborted( pPendingAction, false );

				iter = FlushPendingItem( pPendingAction, m_mmapPending, iter );
			}
			else
			{
				++iter;
			}
		}
	}

	{
		AkMultimapPausedPending::IteratorEx iter = m_mmapPausedPending.BeginEx();
		while( iter != m_mmapPausedPending.End() )
		{
			pPendingAction = (*iter).item;
			pAction = pPendingAction->pAction;
			CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
			pTargetNode.Attach( pAction->GetAndRefTarget() );
			if(
				( !in_pNodeToTarget || IsElementOf( in_pNodeToTarget, pTargetNode ) )
				&& 
				CheckObjAndPlayingID( in_GameObj, in_PlayingID, pPendingAction )
				&& 
				( pAction->ActionType() != AkActionType_Duck )
				)
			{
				NotifyDelayAborted( pPendingAction, true );

				iter = FlushPendingItem( pPendingAction, m_mmapPausedPending, iter );
			}
			else
			{
				++iter;
			}
		}
	}

	return eResult;
}

//-----------------------------------------------------------------------------
// Name: PausePendingActionAllExcept
// Desc:
//
// Parameters:
// CAkRegisteredObj *		in_GameObj				:
// ExceptionList*	in_pExceptionList		:
//-----------------------------------------------------------------------------
void CAkAudioMgr::PausePendingActionAllExcept(	CAkRegisteredObj *	in_GameObj, 
												ExceptionList*		in_pExceptionList,
												bool				in_bIsMasterOnResume,
												AkPlayingID in_PlayingID )
{
	AkPendingAction*	pThisAction = NULL;
	CAkAction*			pAction		= NULL;

	AkMultimapPausedPending::Iterator iterPaused = m_mmapPausedPending.Begin();
	while( iterPaused != m_mmapPausedPending.End() )
	{
		pThisAction = (*iterPaused).item;
		pAction = pThisAction->pAction;

		// is it ours ? we don't pause pending resumes or we'll get stuck
		if(
			(( (pAction->ActionType() & ACTION_TYPE_ACTION) != ACTION_TYPE_RESUME) || in_bIsMasterOnResume )
			&& CheckObjAndPlayingID( in_GameObj, in_PlayingID, pThisAction )
			&& !IsAnException( pAction, in_pExceptionList )
			&& ( pAction->ActionType() != AkActionType_Duck )
			)
		{	
			++(pThisAction->ulPauseCount);
		}
		++iterPaused;
	}

	// scan'em all
	AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
	while( iter != m_mmapPending.End() )
	{
		pThisAction = (*iter).item;
		pAction = pThisAction->pAction;

		// is it ours ? we don't pause pending resumes or we'll get stuck
		if(
			(( (pAction->ActionType() & ACTION_TYPE_ACTION) != ACTION_TYPE_RESUME) || in_bIsMasterOnResume )
			&& CheckObjAndPlayingID( in_GameObj, in_PlayingID, pThisAction )
			&& !IsAnException( pAction, in_pExceptionList )
			&& ( pAction->ActionType() != AkActionType_Duck )
			)
		{	
			InsertAsPaused( pAction->ElementID(), pThisAction );
			iter = m_mmapPending.Erase( iter );
		}
		else
		{
			++iter;
		}
	}
}

//-----------------------------------------------------------------------------
// Name: StopPendingActionAllExcept
// Desc:
//
// Parameters:
// CAkRegisteredObj *	  in_GameObj		    :
// ExceptionList& in_pExceptionList      :
//
// Return: 
//	AK_Success : Succeeded.
//  AK_Fail    : Failed.
//-----------------------------------------------------------------------------
AKRESULT CAkAudioMgr::StopPendingActionAllExcept( CAkRegisteredObj * in_GameObj,
												  ExceptionList* in_pExceptionList,
												  AkPlayingID in_PlayingID )
{
	AKRESULT	eResult = AK_Success;

	AkPendingAction* pPendingAction;
	{
		AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
		while( iter != m_mmapPending.End() )
		{
			pPendingAction = (*iter).item;
			if(
				CheckObjAndPlayingID( in_GameObj, in_PlayingID, pPendingAction )
				&& !IsAnException( pPendingAction->pAction, in_pExceptionList )
				&& ( pPendingAction->pAction->ActionType() != AkActionType_Duck )
				)
			{
				NotifyDelayAborted( pPendingAction, false );

				iter = FlushPendingItem( pPendingAction, m_mmapPending, iter );
			}
			else
			{
				++iter;
			}
		}
	}

	{
		AkMultimapPausedPending::IteratorEx iter = m_mmapPausedPending.BeginEx();
		while( iter != m_mmapPausedPending.End() )
		{
			pPendingAction = (*iter).item;
			if(
				CheckObjAndPlayingID( in_GameObj, in_PlayingID, pPendingAction )
				&& !IsAnException( pPendingAction->pAction, in_pExceptionList )
				&& ( pPendingAction->pAction->ActionType() != AkActionType_Duck )
				)
			{
				NotifyDelayAborted( pPendingAction, true );

				iter = FlushPendingItem( pPendingAction, m_mmapPausedPending, iter );
			}
			else
			{
				++iter;
			}
		}
	}

	return eResult;
}

//-----------------------------------------------------------------------------
// Name: ResumePausedPendingAction
// Desc: move paused pending actions(in_ulElementID) in the pending list
//
// Parameters:
//	AkUniqueID		in_ulElementID		  :
//	CAkRegisteredObj *	in_GameObj			  :
//-----------------------------------------------------------------------------
void CAkAudioMgr::ResumePausedPendingAction( CAkParameterNodeBase * in_pNodeToTarget, 
												 CAkRegisteredObj *	in_GameObj,
												 bool			in_bIsMasterOnResume,
												 AkPlayingID in_PlayingID )
{
	AkPendingAction* pThisAction;

	// if we've got it then move it

	AkMultimapPausedPending::IteratorEx iter = m_mmapPausedPending.BeginEx();
	while( iter != m_mmapPausedPending.End() )
	{
		pThisAction = (*iter).item;
		CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
		pTargetNode.Attach( pThisAction->pAction->GetAndRefTarget() );
		if( (!in_pNodeToTarget || IsElementOf( in_pNodeToTarget, pTargetNode ))
			&& CheckObjAndPlayingID( in_GameObj, in_PlayingID, pThisAction )
			)
		{
			if( in_bIsMasterOnResume || pThisAction->ulPauseCount == 0 )
			{
				TransferToPending( pThisAction );
				iter = m_mmapPausedPending.Erase( iter );
			}
			else
			{
				--(pThisAction->ulPauseCount);
				++iter;
			}
		}
		else
		{
			++iter;
		}
	}

	ResumeNotPausedPendingAction( in_pNodeToTarget, in_GameObj, in_bIsMasterOnResume, in_PlayingID );
}

//-----------------------------------------------------------------------------
// Name: ResumePausedPendingAction
//-----------------------------------------------------------------------------
void CAkAudioMgr::ResumePausedPendingAction( CAkAction* in_pAction )
{
	AkPendingAction* pThisAction;

	// if we've got it then move it

	AkMultimapPausedPending::IteratorEx iter = m_mmapPausedPending.BeginEx();
	while( iter != m_mmapPausedPending.End() )
	{
		pThisAction = (*iter).item;
		CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
		pTargetNode.Attach( pThisAction->pAction->GetAndRefTarget() );
		if( pThisAction->pAction == in_pAction )
		{
			if( pThisAction->ulPauseCount == 0 )
			{
				TransferToPending( pThisAction );
				iter = m_mmapPausedPending.Erase( iter );
			}
			else
			{
				--(pThisAction->ulPauseCount);
				++iter;
			}
		}
		else
		{
			++iter;
		}
	}

	ResumeNotPausedPendingAction( in_pAction );
}

void CAkAudioMgr::ResumePausedPendingItems( AkPlayingID in_playingID )
{
	AkPendingAction* pThisAction;

	// if we've got it then move it
	
	AkMultimapPausedPending::IteratorEx iter = m_mmapPausedPending.BeginEx();
	while( iter != m_mmapPausedPending.End() )
	{
		pThisAction = (*iter).item;
		if( pThisAction->UserParam.PlayingID() == in_playingID )
		{
			if( pThisAction->ulPauseCount == 0 )
			{
				TransferToPending( pThisAction );
				iter = m_mmapPausedPending.Erase( iter );
			}
			else
			{
				--(pThisAction->ulPauseCount);
				++iter;
			}
		}
		else
		{
			++iter;
		}
	}
}

void CAkAudioMgr::ResumeNotPausedPendingAction( CAkParameterNodeBase * in_pNodeToTarget,
												CAkRegisteredObj *	in_GameObj,
												bool				/*in_bIsMasterOnResume*/,
												AkPlayingID in_PlayingID)
{
	//in_bIsMasterOnResume here is unused for now, but will be useful when supporting pause counting
	AkPendingAction* pThisAction;

	// if we've got it then move it
	AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
	while( iter != m_mmapPending.End() )
	{
		pThisAction = (*iter).item;
		CAkAction* pAction = pThisAction->pAction;
		CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
		pTargetNode.Attach( pAction->GetAndRefTarget() );
		if( (!in_pNodeToTarget || IsElementOf( in_pNodeToTarget, pTargetNode ))
			&& CheckObjAndPlayingID( in_GameObj, in_PlayingID, pThisAction )
			)
		{	 
			if( pAction->ActionType() == AkActionType_PlayAndContinue )
			{
				CAkActionPlayAndContinue* pActionPAC = static_cast<CAkActionPlayAndContinue*>( pAction );
				pActionPAC->Resume();
			}
		}
		++iter;
	}
}

void CAkAudioMgr::ResumeNotPausedPendingAction( CAkAction* in_pAction )
{
	//in_bIsMasterOnResume here is unused for now, but will be useful when supporting pause counting
	AkPendingAction* pThisAction;

	// if we've got it then move it
	AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
	while( iter != m_mmapPending.End() )
	{
		pThisAction = (*iter).item;
		CAkAction* pAction = pThisAction->pAction;
		CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
		pTargetNode.Attach( pAction->GetAndRefTarget() );
		if( pAction == in_pAction )
		{	 
			if( pAction->ActionType() == AkActionType_PlayAndContinue )
			{
				CAkActionPlayAndContinue* pActionPAC = static_cast<CAkActionPlayAndContinue*>( pAction );
				pActionPAC->Resume();
			}
		}
		++iter;
	}
}

//-----------------------------------------------------------------------------
// Name: ResumePausedPendingActionAllExcept
// Desc: Execute behavioural side and lower engine side.
//
// Parameters:
// CAkRegisteredObj *		in_GameObj			  :
// ExceptionList&	in_pExceptionList      :
//-----------------------------------------------------------------------------
void CAkAudioMgr::ResumePausedPendingActionAllExcept(CAkRegisteredObj * in_GameObj, 
														 ExceptionList* in_pExceptionList,
														 bool			in_bIsMasterOnResume,
														 AkPlayingID in_PlayingID )
{
	AkPendingAction*	pThisAction = NULL;

	// if we've got it then move it
	AkMultimapPausedPending::IteratorEx iter = m_mmapPausedPending.BeginEx();
	while( iter != m_mmapPausedPending.End() )
	{
		pThisAction = (*iter).item;
		if(
			CheckObjAndPlayingID( in_GameObj, in_PlayingID, pThisAction )
			&& !IsAnException( pThisAction->pAction, in_pExceptionList )
			)
		{	
			if( in_bIsMasterOnResume || pThisAction->ulPauseCount == 0 )
			{
				TransferToPending( pThisAction );
				iter = m_mmapPausedPending.Erase( iter );
			}
			else
			{
				--(pThisAction->ulPauseCount);
				++iter;
			}
		}
		else
		{
			++iter;
		}
	}
	g_pAudioMgr->ResumeNotPausedPendingActionAllExcept( in_GameObj, in_pExceptionList, in_bIsMasterOnResume, in_PlayingID );
}

void CAkAudioMgr::ResumeNotPausedPendingActionAllExcept(CAkRegisteredObj * in_GameObj, 
														ExceptionList*  in_pExceptionList,
														bool			/*in_bIsMasterOnResume*/,
														AkPlayingID in_PlayingID)
{
	//in_bIsMasterOnResume here is unused for now, but will be useful when supporting pause counting

	AkPendingAction*	pThisAction = NULL;

	// if we've got it then move it
	AkMultimapPending::Iterator iter = m_mmapPending.Begin();
	while( iter != m_mmapPending.End() )
	{
		pThisAction = (*iter).item;
		CAkAction* pAction = pThisAction->pAction;
		if(
			CheckObjAndPlayingID( in_GameObj, in_PlayingID, pThisAction )
			&&
			!IsAnException( pAction, in_pExceptionList )
			)
		{	 
			if( pAction->ActionType() == AkActionType_PlayAndContinue )
			{
				CAkActionPlayAndContinue* pActionPAC = static_cast<CAkActionPlayAndContinue*>( pAction );
				pActionPAC->Resume();
			}
		}
		++iter;
	}
}
//-----------------------------------------------------------------------------
// Name: RemovePendingAction
//-----------------------------------------------------------------------------
void CAkAudioMgr::RemovePendingAction( CAkParameterNodeBase * in_pNodeToTarget )
{
	AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
	while( iter != m_mmapPending.End() )
	{
		AkPendingAction* pPending = (*iter).item;
		CAkAction* pAction = pPending->pAction;
		CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
		pTargetNode.Attach( pAction->GetAndRefTarget() );
		if( 
			IsElementOf( in_pNodeToTarget, pTargetNode )
			&& ( pAction->ActionType() != AkActionType_Duck )
			)
		{
			NotifyDelayAborted( pPending, false );

			iter = FlushPendingItem( pPending, m_mmapPending, iter );
		}
		else
		{
			++iter;
		}
	}
}

//-----------------------------------------------------------------------------
// Name: RemovePausedPendingAction
//-----------------------------------------------------------------------------
void CAkAudioMgr::RemovePausedPendingAction( CAkParameterNodeBase * in_pNodeToTarget )
{
	AkMultimapPausedPending::IteratorEx iter = m_mmapPausedPending.BeginEx();
	while( iter != m_mmapPausedPending.End() )
	{
		AkPendingAction* pPending = (*iter).item;
		CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
		pTargetNode.Attach(pPending->pAction->GetAndRefTarget() );
		if( IsElementOf( in_pNodeToTarget, pTargetNode ) )
		{
			NotifyDelayAborted( pPending, true );

			iter = FlushPendingItem( pPending, m_mmapPausedPending, iter );
		}
		else
		{
			++iter;
		}
	}
}

//-----------------------------------------------------------------------------
// Name: RemoveAllPausedPendingAction
// Desc:
//
// Parameters:
//	None.
//
// Return: 
//	None.
//-----------------------------------------------------------------------------
void CAkAudioMgr::RemoveAllPausedPendingAction()
{
	AkMultimapPausedPending::IteratorEx iter = m_mmapPausedPending.BeginEx();
	while( iter != m_mmapPausedPending.End() )
	{
		AkPendingAction* pPending = (*iter).item;

		NotifyDelayAborted( pPending, true );

		iter = FlushPendingItem( pPending, m_mmapPausedPending, iter );
	}
}

//-----------------------------------------------------------------------------
// Name: RemoveAllPendingAction
// Desc:
//
// Parameters:
//	None.
//
// Return: 
//	None.
//-----------------------------------------------------------------------------
void CAkAudioMgr::RemoveAllPendingAction()
{
	AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
	while( iter != m_mmapPending.End() )
	{
		AkPendingAction* pPending = (*iter).item;
		
		NotifyDelayAborted( pPending, false );

		iter = FlushPendingItem( pPending, m_mmapPending, iter );
	}
}

//-----------------------------------------------------------------------------
// Name: RemoveAllPreallocAndReferences
// Desc: Message queue clean-up before destroying the Audio Manager.
//		 Free command queue's pre-allocated and pre-referenced items (that is, that were allocated 
//		 or referenced by the game thread, in AkAudioLib). 
//
// Parameters:
//	None.
//
// Return: 
//	None.
//-----------------------------------------------------------------------------
void CAkAudioMgr::RemoveAllPreallocAndReferences()
{
	while(!m_MsgQueue.IsEmpty())
	{
		AkQueuedMsg * pItem = (AkQueuedMsg *) m_MsgQueue.BeginRead();
		AKASSERT( pItem );

		switch ( pItem->type )
		{
		case QueuedMsgType_RegisterGameObj:
			if ( pItem->reggameobj.pMonitorData )
			{
				MONITOR_FREESTRING( pItem->reggameobj.pMonitorData );
			}
			break;

		case QueuedMsgType_Event:
			AKASSERT(pItem->event.pEvent);
			g_pPlayingMgr->RemoveItemActiveCount( pItem->event.PlayingID );
			pItem->event.pEvent->Release();
			if (pItem->event.CustomParam.pExternalSrcs != (AkUIntPtr)NULL)
				pItem->event.CustomParam.pExternalSrcs->Release();
			break;

		case QueuedMsgType_Seek:
			AKASSERT( pItem->seek.pEvent );
			pItem->seek.pEvent->Release();
			break;

		case QueuedMsgType_EventAction:
			AKASSERT(pItem->eventAction.pEvent);
			pItem->eventAction.pEvent->Release();
			break;

		case QueuedMsgType_EventPostMIDI:
			AKASSERT( pItem->postMIDI.pEvent );
			pItem->postMIDI.pEvent->Release();
			break;

		case QueuedMsgType_EventStopMIDI:
			if( pItem->stopMIDI.pEvent )
				pItem->stopMIDI.pEvent->Release();
			break;

		case QueuedMsgType_DynamicSequenceCmd:
			AKASSERT( pItem->dynamicsequencecmd.pDynamicSequence );
			if( AkQueuedMsg_DynamicSequenceCmd::Close == pItem->dynamicsequencecmd.eCommand )
			{
				g_pPlayingMgr->RemoveItemActiveCount( pItem->dynamicsequencecmd.pDynamicSequence->GetPlayingID() );
				pItem->dynamicsequencecmd.pDynamicSequence->Release();// Yes, we must release twice.
			}
			pItem->dynamicsequencecmd.pDynamicSequence->Release();
			break;
		
		case QueuedMsgType_ApiExtension:
			{
				AkMsgQueueHandlers* pHandler = m_MsgQueueHandlers.Exists(pItem->apiExtension.uID);
				if (pHandler != NULL)
					(pHandler->fcnRelease)((void*)((&pItem->apiExtension) + 1), pItem->size - sizeof(AkQueuedMsg_ApiExtension));
			}
			break;
		case QueuedMsgType_AddOutputCaptureMarker:
			AkFree(AkMemID_Object, pItem->captureMarker.szMarkerText);
			pItem->captureMarker.szMarkerText = NULL;
			break;

		// TODO: Add cases for messages that need special cleanup here.
		// (alessard) my guess is that QueuedMsgType_ListenerSpatialization and QueuedMsgType_SetSpeakerAngles are leaking memory when terminating the sound engine while messages were not processed.
		// This is not critical but could cause asserts to some users.

		case QueuedMsgType_QueueWrapAround:
			//This message tells that the rest of the queue buffer is unused (the next message was too large to fit in)
			m_MsgQueue.EndRead(m_MsgQueue.GetMaxQueueSize());	//Force wrap around.
			continue;
		
		}

		m_MsgQueue.EndRead( pItem->size );
	}
}

//-----------------------------------------------------------------------------
// Name: NotifyDelayStarted
// Desc:
//
// Parameters:
//	AkPendingAction* in_pPending : Point to a pending action.
//
// Return: 
//	None.
//-----------------------------------------------------------------------------
void CAkAudioMgr::NotifyDelayStarted(AkPendingAction* in_pPending)
{
	NotifyDelay( in_pPending, AkMonitorData::NotificationReason_Delay_Started, false );
}

//-----------------------------------------------------------------------------
// Name: NotifyDelayAborted
// Desc:
//
// Parameters:
//	AkPendingAction* in_pPending : Point to a pending action.
//
// Return: 
//	None.
//-----------------------------------------------------------------------------
void CAkAudioMgr::NotifyDelayAborted( AkPendingAction* in_pPending, bool in_bWasPaused )
{
	NotifyDelay(in_pPending, AkMonitorData::NotificationReason_Delay_Aborted, in_bWasPaused );
	g_pPlayingMgr->RemoveItemActiveCount( in_pPending->UserParam.PlayingID() );
}

//-----------------------------------------------------------------------------
// Name: NotifyDelayEnded
// Desc:
//
// Parameters:
//	AkPendingAction* in_pPending : Point to a pending action.
//
// Return: 
//	None.
//-----------------------------------------------------------------------------
void CAkAudioMgr::NotifyDelayEnded(AkPendingAction* in_pPending, bool in_bWasPaused )
{
	NotifyDelay(in_pPending, AkMonitorData::NotificationReason_Delay_Ended, in_bWasPaused );
}

//-----------------------------------------------------------------------------
// Name: NotifyDelay
// Desc: 
//
// Parameters:
//	AkPendingAction*				  in_pPending : Point to a pending action.
//	AkMonitorData::NotificationReason in_Reason   :
//
// Return: 
//	None.
//-----------------------------------------------------------------------------
void CAkAudioMgr::NotifyDelay(AkPendingAction* in_pPending, 
								AkMonitorData::NotificationReason in_Reason,
								bool in_bWasPaused )
{
	CAkCntrHist HistArray;
	if( in_bWasPaused )
	{
		MONITOR_OBJECTNOTIF( in_pPending->UserParam.PlayingID(), in_pPending->GameObjID(), in_pPending->UserParam.CustomParam(), AkMonitorData::NotificationReason_Pause_Aborted, HistArray, in_pPending->pAction->ID(), false, 0 );
	}

	switch ( in_pPending->pAction->ActionType() )
	{
	case AkActionType_Duck:
		// WG-4697: Don't notify if the action is a ducking action. 
		break;
	case AkActionType_PlayAndContinue:
		if(!static_cast<CAkActionPlayAndContinue*>(in_pPending->pAction)->NeedNotifyDelay() )
		{
			//Do not notify in the case of a cross-fade
			if(in_Reason == AkMonitorData::NotificationReason_Delay_Aborted)
			{
				in_Reason = AkMonitorData::NotificationReason_ContinueAborted;
			}
			else
			{
				break;
			}
		}
		//no break here
	case AkActionType_Play:
		static_cast<CAkActionPlay*>(in_pPending->pAction)->GetHistArray( HistArray );

		//no break here
	default:
		MONITOR_OBJECTNOTIF( in_pPending->UserParam.PlayingID(), in_pPending->GameObjID(), in_pPending->UserParam.CustomParam(), in_Reason, HistArray, in_pPending->pAction->ID(), false, 0 );
		break;
	}
}

//-----------------------------------------------------------------------------
// Name: GetActionMatchingPlayingID
// Desc:
//
// Parameters:
//	AkPlayingID in_PlayingID
//
// Return: 
//	AkPendingAction * : Pointer to pending action.
//-----------------------------------------------------------------------------
AkPendingAction* CAkAudioMgr::GetActionMatchingPlayingID(AkPlayingID in_PlayingID)
{
	for( AkMultimapPending::Iterator iter = m_mmapPending.Begin(); iter != m_mmapPending.End(); ++iter )
	{
		if( (*iter).item->UserParam.PlayingID() == in_PlayingID )
		{
			return (*iter).item;
		}
	}
	for( AkMultimapPausedPending::Iterator iter = m_mmapPausedPending.Begin(); iter != m_mmapPausedPending.End(); ++iter )
	{
		if( (*iter).item->UserParam.PlayingID() == in_PlayingID)
		{
			return (*iter).item;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Name: StopAction
// Desc:
//
// Return: 
//	AK_Success : Succeeded.
//  AK_Fail    : Failed.
//-----------------------------------------------------------------------------
AKRESULT CAkAudioMgr::StopAction(AkUniqueID in_ActionID, AkPlayingID in_PlayingID /*= AK_INVALID_PLAYING_ID*/)
{
	AKRESULT	eResult = AK_Success;

	AkPendingAction* pPending;

	for( AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx(); iter != m_mmapPending.End(); )
	{
		pPending = (*iter).item;

		if( pPending->pAction->ID() == in_ActionID && (in_PlayingID==AK_INVALID_PLAYING_ID||pPending->UserParam.PlayingID()==in_PlayingID) )
		{
			NotifyDelayAborted( pPending, false );
			iter = FlushPendingItem( pPending, m_mmapPending, iter );
		}
		else
			++iter;
	}

	for( AkMultimapPausedPending::IteratorEx iter = m_mmapPausedPending.BeginEx(); iter != m_mmapPausedPending.End(); )
	{
		pPending = (*iter).item;

		if( pPending->pAction->ID() == in_ActionID && (in_PlayingID==AK_INVALID_PLAYING_ID||pPending->UserParam.PlayingID()==in_PlayingID))
		{
			NotifyDelayAborted( pPending, true );
			iter = FlushPendingItem( pPending, m_mmapPausedPending, iter );
		}
		else
			++iter;
	}

	return eResult;
}

//-----------------------------------------------------------------------------
// Name: PauseAction
// Desc:
//
// Return: 
//	AK_Success : Succeeded.
//  AK_Fail    : Failed.
//-----------------------------------------------------------------------------
AKRESULT CAkAudioMgr::PauseAction(AkUniqueID in_ActionID, AkPlayingID in_PlayingID /*= AK_INVALID_PLAYING_ID*/)
{
	AKRESULT eResult = AK_Success;

	AkPendingAction*	pThisAction;
	CAkAction*			pAction;

	AkMultimapPausedPending::Iterator iterPause = m_mmapPausedPending.Begin();
	while( iterPause != m_mmapPausedPending.End() )
	{
		pThisAction = (*iterPause).item;
		pAction = pThisAction->pAction;
		AKASSERT(pAction);

		// is it ours ? we don't pause pending resumes or we'll get stuck
		if(in_ActionID == pAction->ID()&& (in_PlayingID==AK_INVALID_PLAYING_ID||pThisAction->UserParam.PlayingID()==in_PlayingID))
		{	
			++(pThisAction->ulPauseCount);
		}
		++iterPause;
	}

	// scan'em all
	AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
	while( iter != m_mmapPending.End() )
	{
		pThisAction = (*iter).item;
		pAction = pThisAction->pAction;
		AKASSERT(pAction);

		// is it ours ? we don't pause pending resumes or we'll get stuck
		if(in_ActionID == pAction->ID()&& (in_PlayingID==AK_INVALID_PLAYING_ID||pThisAction->UserParam.PlayingID()==in_PlayingID))
		{	
			InsertAsPaused( pAction->ElementID(), pThisAction );
			iter = m_mmapPending.Erase( iter );
		}
		else
		{
			++iter;
		}
	}
	return eResult;
}

//-----------------------------------------------------------------------------
// Name: ResumeAction
// Desc:
//
// Return: 
//	AK_Success : Succeeded.
//  AK_Fail    : Failed.
//-----------------------------------------------------------------------------
AKRESULT CAkAudioMgr::ResumeAction(AkUniqueID in_ActionID, AkPlayingID in_PlayingID /*= AK_INVALID_PLAYING_ID*/)
{
	AKRESULT eResult = AK_Success;
	AkPendingAction*	pThisAction;

	AkMultimapPausedPending::IteratorEx iter = m_mmapPausedPending.BeginEx();
	while( iter != m_mmapPausedPending.End() )
	{
		pThisAction = (*iter).item;
		if( pThisAction->pAction->ID() == in_ActionID && (in_PlayingID==AK_INVALID_PLAYING_ID||pThisAction->UserParam.PlayingID()==in_PlayingID) )
		{
			if( pThisAction->ulPauseCount == 0 )
			{
				TransferToPending( pThisAction );
				iter = m_mmapPausedPending.Erase( iter );
			}
			else
			{
				--(pThisAction->ulPauseCount);
				++iter;
			}
		}
		else
		{
			++iter;
		}
	}
	return eResult;
}

//-----------------------------------------------------------------------------
// Name: ClearPendingItems
// Desc:
//
// Parameters:
// AkPlayingID in_PlayingID
//
// Return: 
//	None.
//-----------------------------------------------------------------------------
void CAkAudioMgr::ClearPendingItems( AkPlayingID in_PlayingID )
{
	AkPendingAction* pPendingAction;
	{
		AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
		while( iter != m_mmapPending.End() )
		{
			pPendingAction = (*iter).item;
			if( pPendingAction->UserParam.PlayingID() == in_PlayingID )
			{
				NotifyDelayAborted( pPendingAction, false );

				iter = FlushPendingItem( pPendingAction, m_mmapPending, iter );
			}
			else
			{
				++iter;
			}
		}
	}
	{
		AkMultimapPausedPending::IteratorEx iter = m_mmapPausedPending.BeginEx();
		while( iter != m_mmapPausedPending.End() )
		{
			pPendingAction = (*iter).item;
			if( pPendingAction->UserParam.PlayingID() == in_PlayingID)
			{
				NotifyDelayAborted( pPendingAction, true );

				iter = FlushPendingItem( pPendingAction, m_mmapPausedPending, iter );
			}
			else
			{
				++iter;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Name: ClearPendingItemsExemptOne
// Desc:
//
// Parameters:
//	AkPlayingID in_PlayingID
//
// Return: 
//	None.
//-----------------------------------------------------------------------------
void CAkAudioMgr::ClearPendingItemsExemptOne(AkPlayingID in_PlayingID)
{
	bool bExempt = true;
	AkPendingAction* pPendingAction;
	{
		AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx();
		while( iter != m_mmapPending.End() )
		{
			pPendingAction = (*iter).item;
			if( pPendingAction->UserParam.PlayingID() == in_PlayingID )
			{
				if(bExempt)
				{
					NotifyDelayEnded( pPendingAction );
					g_pPlayingMgr->RemoveItemActiveCount( pPendingAction->UserParam.PlayingID() );
					bExempt = false;
				}
				else
				{
					NotifyDelayAborted( pPendingAction, false );
				}
				iter = FlushPendingItem( pPendingAction, m_mmapPending, iter );
			}
			else
			{
				++iter;
			}
		}
	}
	{
		AkMultimapPausedPending::IteratorEx iter = m_mmapPausedPending.BeginEx();
		while( iter != m_mmapPausedPending.End() )
		{
			pPendingAction = (*iter).item;
			if(pPendingAction->UserParam.PlayingID() == in_PlayingID)
			{
				if(bExempt)
				{
					// This is a special situation, the notification use true as optionnal flag, 
					// telling that the sound is unpaused and continuated in one shot
					NotifyDelayEnded( pPendingAction, true );
					g_pPlayingMgr->RemoveItemActiveCount( pPendingAction->UserParam.PlayingID() );
					bExempt = false;
				}
				else
				{
					NotifyDelayAborted( pPendingAction, true );
				}

				iter = FlushPendingItem( pPendingAction, m_mmapPausedPending, iter );
			}
			else
			{
				++iter;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Name: ClearCrossFadeOccurence
// Desc:
//
// Parameters:
//	 CAkPBI* in_pPBIToCheck :
//
// Return: 
//	None.
//-----------------------------------------------------------------------------
void CAkAudioMgr::ClearCrossFadeOccurence( CAkContinuousPBI* in_pPBIToCheck )
{
	CAkAction* pAction;
	//do not use in_pPBIToCheck here, only for comparaison purpose.
	for( AkMultimapPending::Iterator iter = m_mmapPending.Begin(); iter != m_mmapPending.End(); ++iter )
	{
		pAction = (*iter).item->pAction;
		if( pAction->ActionType() == AkActionType_PlayAndContinue )
		{
			static_cast<CAkActionPlayAndContinue*>( pAction )->UnsetFadeBack( in_pPBIToCheck );
		}
	}
	for( AkMultimapPausedPending::Iterator iter = m_mmapPausedPending.Begin(); iter != m_mmapPausedPending.End(); ++iter )
	{
		pAction = (*iter).item->pAction;
		if(pAction->ActionType() == AkActionType_PlayAndContinue)
		{
			static_cast<CAkActionPlayAndContinue*>( pAction )->UnsetFadeBack( in_pPBIToCheck );
		}
	}
}

//-----------------------------------------------------------------------------
// Name: IsAnException
// Desc:
//
// Parameters:
//	None.
//
// Return: 
//	true  : An exception.
//	false : Not an exception.
//-----------------------------------------------------------------------------
bool CAkAudioMgr::IsAnException( CAkAction* in_pAction, ExceptionList* in_pExceptionList )
{
	AKASSERT(in_pAction);

	if( !in_pExceptionList )
	{
		return false;
	}

	bool l_bCheckedBus = false;

	CAkParameterNodeBase* pBusNode = NULL;

	if(in_pAction->ElementID())
	{
		CAkParameterNodeBase* pNode = in_pAction->GetAndRefTarget();
		CAkParameterNodeBase* pNodeInitial = pNode;

		while(pNode != NULL)
		{
			for( ExceptionList::Iterator iter = in_pExceptionList->Begin(); iter != in_pExceptionList->End(); ++iter )
			{
				WwiseObjectID wwiseId( pNode->ID(), pNode->IsBusCategory() );
				if( *iter == wwiseId )
				{
					pNodeInitial->Release();
					return true;
				}
			}

			if( !l_bCheckedBus )
			{
				pBusNode = pNode->ParentBus();
				if(pBusNode)
				{
					l_bCheckedBus = true;
				}
			}

			pNode = pNode->Parent();
		}
		while(pBusNode != NULL)
		{
			for( ExceptionList::Iterator iter = in_pExceptionList->Begin(); iter != in_pExceptionList->End(); ++iter )
			{
				WwiseObjectID wwiseId( pBusNode->ID(), pBusNode->IsBusCategory() );
				if( (*iter) == wwiseId )
				{
					pNodeInitial->Release();
					return true;
				}
			}

			pBusNode = pBusNode->ParentBus();
		}

		if ( pNodeInitial )
			pNodeInitial->Release();
	}
	return false;
}

//-----------------------------------------------------------------------------
// Name: IsElementOf
// Desc:
//
// Parameters:
// AkUniqueID in_TargetID	:
// AkUniqueID in_IDToCheck	:
//
// Return: 
//	true  : is element of.
//	false : not an element of.
//-----------------------------------------------------------------------------
bool CAkAudioMgr::IsElementOf( CAkParameterNodeBase * in_pNodeToTarget, CAkParameterNodeBase * in_pNodeToCheck )
{
	bool bIsElementOf = false;

	if(in_pNodeToTarget && in_pNodeToCheck)
	{
		if(in_pNodeToTarget == in_pNodeToCheck)//most situations
		{
			bIsElementOf = true;
		}
		else
		{
			CAkParameterNodeBase* pBus = in_pNodeToCheck->ParentBus();

			for( in_pNodeToCheck = in_pNodeToCheck->Parent(); in_pNodeToCheck ; in_pNodeToCheck = in_pNodeToCheck->Parent() )
			{
				if( in_pNodeToTarget == in_pNodeToCheck )
				{
					bIsElementOf = true;
					break;
				}
				if( pBus == NULL )
				{
					pBus = in_pNodeToCheck->ParentBus();
				}
			}
			if( !bIsElementOf )
			{
				//checking bus
				for( /*noinit*/ ; pBus ; pBus = pBus->ParentBus() )
				{
					if( in_pNodeToTarget == pBus )
					{
						bIsElementOf = true;
						break;
					}
				}
			}		
		}
	}

	return bIsElementOf;
}

//Start the AudioThread
AKRESULT CAkAudioMgr::Start()
{
	AKRANDOM::AkRandomInit();
	
    AKRESULT res = AK_Fail;
	if (g_settings.bUseLEngineThread && !AK_PERF_OFFLINE_RENDERING)
	{
		AKASSERT(m_audioThread.GetAudioThreadID() == 0);

        res = m_audioThread.Start();
	}
	else
	{
		CAkLEngine::OnThreadStart();
		res = AK_Success;
	}
#ifdef AK_IOS
    CAkLEngine::RegisterNotifications();
#endif
    RenderAudio(false);
	return res;
}

//Stop the AudioThread 
void CAkAudioMgr::Stop()
{
	if (g_settings.bUseLEngineThread && !AK_PERF_OFFLINE_RENDERING)
	{
		m_audioThread.Stop();
	}
	else
	{
		CAkLEngine::OnThreadEnd();
	}
}

AkPendingAction::AkPendingAction( CAkRegisteredObj * in_pGameObj )
	: TargetPlayingID( AK_INVALID_PLAYING_ID )
	, pGameObj( in_pGameObj )
{
	if ( pGameObj )
		pGameObj->AddRef();
}

AkPendingAction::~AkPendingAction()
{
	if ( pGameObj )
		pGameObj->Release();
}

AkGameObjectID AkPendingAction::GameObjID() 
{ 
	return pGameObj ? pGameObj->ID() : AK_INVALID_GAME_OBJECT; 
}

void AkPendingAction::TransUpdateValue( AkIntPtr in_eTarget, AkReal32, bool in_bIsTerminated )
{
	AKASSERT( g_pAudioMgr );
	if( pAction->ActionType() == AkActionType_PlayAndContinue )
	{
		CAkActionPlayAndContinue* pActionPAC = static_cast<CAkActionPlayAndContinue*>( pAction );
		AKASSERT(g_pTransitionManager);	 

		TransitionTargets eTarget = (TransitionTargets)in_eTarget;
		switch( eTarget )
		{
		case TransTarget_Stop:
		case TransTarget_Play:
			if(in_bIsTerminated)
			{
				pActionPAC->m_PBTrans.pvPSTrans = NULL;

				if( eTarget == TransTarget_Stop )
				{
					g_pAudioMgr->StopPending( this );
				}
			}
			break;
		case TransTarget_Pause:
		case TransTarget_Resume:
			if(in_bIsTerminated)
			{
				pActionPAC->m_PBTrans.pvPRTrans = NULL;

				if( eTarget == TransTarget_Pause )
				{
					g_pAudioMgr->PausePending( this );
				}

				pActionPAC->SetPauseCount( 0 );
			}
			break;
		default:
			AKASSERT(!"Unsupported data type");
			break;
		}
	}
	else
	{
		// Should not happen, only PlayAndContinue Actions can have transitions.
		AKASSERT( pAction->ActionType() == AkActionType_PlayAndContinue );
	}
}

AKRESULT CAkAudioMgr::PausePending( AkPendingAction* in_pPA )
{
	AKRESULT	eResult = AK_Success;

	if( in_pPA )
	{
		AkPendingAction*	pThisAction = NULL;
		CAkAction*			pAction		= NULL;

		// scan'em all
		for( AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx(); iter != m_mmapPending.End(); ++iter )
		{
			pThisAction = (*iter).item;
			pAction = pThisAction->pAction;

			if( pThisAction == in_pPA )
			{	
				AkUInt32 l_ulPauseCount = 0;

				if( pAction->ActionType() == AkActionType_PlayAndContinue )
				{
					l_ulPauseCount = static_cast<CAkActionPlayAndContinue*>(pAction)->GetPauseCount() - 1;
					static_cast<CAkActionPlayAndContinue*>(pAction)->SetPauseCount( 0 );
				}

				InsertAsPaused( pAction->ElementID(), pThisAction, l_ulPauseCount );
				m_mmapPending.Erase( iter );
				return eResult;
			}
		}
		
		// scan'em all
		for( AkMultimapPausedPending::Iterator iter = m_mmapPausedPending.Begin(); iter != m_mmapPausedPending.End(); ++iter )
		{
			pThisAction = (*iter).item;
			pAction = pThisAction->pAction;

			if( pThisAction == in_pPA )
			{	
				if( pAction->ActionType() == AkActionType_PlayAndContinue )
				{
					(pThisAction->ulPauseCount) += static_cast<CAkActionPlayAndContinue*>(pAction)->GetPauseCount();
				}
				else
				{
					++(pThisAction->ulPauseCount);
				}
				return eResult;
			}
		}
	}
	return eResult;
}

AKRESULT CAkAudioMgr::StopPending( AkPendingAction* in_pPA )
{
	AKRESULT	eResult = AK_Success;

	if( in_pPA )
	{		
		for( AkMultimapPausedPending::IteratorEx iter = m_mmapPausedPending.BeginEx(); iter != m_mmapPausedPending.End(); ++iter )
		{
			if( in_pPA == (*iter).item )
			{
				NotifyDelayAborted( in_pPA, true );
				FlushPendingItem( in_pPA, m_mmapPausedPending, iter );
				break;
			}
		}
		
		for( AkMultimapPending::IteratorEx iter = m_mmapPending.BeginEx(); iter != m_mmapPending.End(); ++iter )
		{
			if( in_pPA == (*iter).item )
			{
				NotifyDelayAborted( in_pPA, false );
				FlushPendingItem( in_pPA, m_mmapPending, iter );
				break;
			}
		}
	}
	return eResult;
}

#ifndef AK_OPTIMIZED
void CAkAudioMgr::InvalidatePendingPaths(AkUniqueID in_idElement)
{
	AkMultimapPending::Iterator it = m_mmapPending.Begin();
	while(it != m_mmapPending.End())
	{
		AkPendingAction* pAction = (*it).item;
		if( pAction->pAction->ActionType() == AkActionType_PlayAndContinue )
		{
			CAkActionPlayAndContinue *pPlayAndContinue = static_cast<CAkActionPlayAndContinue*>( pAction->pAction );
			if (in_idElement == pPlayAndContinue->GetPathInfo()->PathOwnerID)
			{
				g_pPathManager->RemovePathFromList(pPlayAndContinue->GetPathInfo()->pPBPath);
				pPlayAndContinue->GetPathInfo()->pPBPath = NULL;
				pPlayAndContinue->GetPathInfo()->PathOwnerID = AK_INVALID_UNIQUE_ID;
			}
		}
		++it;
	}

	AkMultimapPausedPending::Iterator it2 = m_mmapPausedPending.Begin();
	while(it2 != m_mmapPausedPending.End())
	{
		AkPendingAction* pAction = (*it2).item;
		if( pAction->pAction->ActionType() == AkActionType_PlayAndContinue )
		{
			CAkActionPlayAndContinue *pPlayAndContinue = static_cast<CAkActionPlayAndContinue*>( pAction->pAction );
			if (in_idElement == pPlayAndContinue->GetPathInfo()->PathOwnerID)
			{
				g_pPathManager->RemovePathFromList(pPlayAndContinue->GetPathInfo()->pPBPath);
				pPlayAndContinue->GetPathInfo()->pPBPath = NULL;
				pPlayAndContinue->GetPathInfo()->PathOwnerID = AK_INVALID_UNIQUE_ID;
			}
		}
		++it2;
	}
}
#endif

void CAkAudioMgr::HandleLossOfHardwareResponse()
{	
	m_uCallsWithoutTicks++;
			
	//How much time has passed since the last tick?
	// WG-23727: Use configurable timeout to avoid crashes from various iOS hardware.
	AkUInt32 maxUnresponsitveFrames = g_settings.uMaxHardwareTimeoutMs / AK_MS_PER_BUFFER_TICK;
	if (m_uCallsWithoutTicks > maxUnresponsitveFrames && CAkOutputMgr::RenderIsActive())
	{
		if (AKPLATFORM::Elapsed(m_timeThisBuffer, m_timeLastBuffer) > g_settings.uMaxHardwareTimeoutMs )
		{
			//The HW hasn't requested samples for too long, assume it is dead.
			CAkOutputMgr::StartSilentMode(true);
			StampTimeLastBuffer();
			m_uCallsWithoutTicks = 0;
			MONITOR_ERRORMSG("Hardware audio subsystem stopped responding.  Silent mode is enabled.");
		}
	}
}

void CAkAudioMgr::StampTimeLastBuffer()
{
	AKPLATFORM::PerformanceCounter(&m_timeLastBuffer);
}

AKRESULT CAkAudioMgr::SuspendWakeup(bool in_bSuspend, bool in_bRender, AkUInt32 in_uDelay)
{
    AkQueuedMsg *pItem = g_pAudioMgr->ReserveQueue(QueuedMsgType_SuspendOrWakeup, AkQueuedMsg::Sizeof_Suspend());
	pItem->suspend.bSuspend = in_bSuspend;
    pItem->suspend.bRender = in_bRender;
	pItem->suspend.uDelay = in_uDelay;
    FinishQueueWrite();
    return RenderAudio(false);
}

void CAkAudioMgr::InitSinkPlugin(AkOutputSettings& in_settings)
{
	AKASSERT(sizeof(AkOutputSettings) == sizeof(AkQueuedMsg_InitSinkPlugin));
	AkQueuedMsg *pItem = g_pAudioMgr->ReserveQueue(QueuedMsgType_InitSinkPlugin, AkQueuedMsg::Sizeof_InitSinkPlugin());	
	memcpy(&pItem->initSink, &in_settings, sizeof(AkOutputSettings));
	FinishQueueWrite();	
}
