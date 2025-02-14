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
// AkAudioMgr.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _AUDIO_MGR_H_
#define _AUDIO_MGR_H_

#include "AkMultiKeyList.h"
#include "AkAudioThread.h"
#include "AkParameterNodeBase.h" //MidiEventActionType
#include "PrivateStructures.h"

class CAkEvent;
struct AkCustomParamType;
struct AkOutputSettings;
struct AkQueuedMsg;

class CAkContinuousPBI;
class CAkRegisteredObj;
struct AkQueuedMsg_EventAction;
struct AkQueuedMsg_EventPostMIDI;
struct AkQueuedMsg_EventStopMIDI;

#define COMMAND_QUEUE_SIZE			(1024 * 256)

typedef void(*AkMsgQueueHandler)(void* pMsg, AkUInt32 uSize);

class AkLockLessMsgQueue
{
public:
	AkLockLessMsgQueue()
		: m_pRead( NULL )
		, m_pWrite( NULL )
		, m_pStart( NULL )
		, m_pEnd( NULL )
	{
	}

	~AkLockLessMsgQueue()
	{
		AKASSERT( m_pStart == NULL );
	}

	AKRESULT Init( AkMemPoolId in_PoolId, AkUInt32 in_ulSize );
	void Term( AkMemPoolId in_PoolId );

	bool IsEmpty()
	{
		return m_pRead == m_pWrite;
	}

	void * BeginRead()
	{
		return m_pRead;
	}

	void EndRead( AkUInt32 in_ulSize )
	{
		in_ulSize += 3;
		in_ulSize &= ~0x03; // Data is always aligned to 4 bytes.

		AkUInt8* nextRead = m_pRead + in_ulSize;
		if (nextRead < m_pEnd)
			m_pRead = nextRead;
		else
			m_pRead = m_pStart;
	}

	// Get the next message int the queue without changing the position of the read pointer. 
	void PeekNextMsg(AkUInt8 ** in_pPosition, AkUInt32 in_size)
	{
		// Note : only moves the in_pPosition ptr forward and doesn't handle wrap around. 
		in_size += 3;
		in_size &= ~0x03;
		*in_pPosition = *in_pPosition + in_size;
	}

	inline bool NeedWraparound();

	//Reserves space in the queue.
	inline void * ReserveForWrite(AkUInt32 &io_size);

	AkUInt32 GetMaxQueueSize() { return m_uQueueSize; }
	AkUInt32 GetUsageSize() { return (AkUInt32)(m_pRead > m_pWrite ? m_uQueueSize-(m_pRead-m_pWrite):(m_pWrite-m_pRead)); }

private:
	AkUInt8 * volatile m_pRead;			// Read position in buffer
	AkUInt8 * volatile m_pWrite;		// Write position in buffer

	AkUInt8 * volatile m_pStart;		// Memory start of buffer
	AkUInt8 * volatile m_pEnd;			// Memory end of buffer
	AkUInt32 m_uQueueSize;
};

// all what we need to start an action at some later time
struct AkPendingAction : public ITransitionable 
							
{
	CAkAction*			pAction;				// the action itself
	AkUInt32			LaunchTick;				// when it should run
	AkUInt32			LaunchFrameOffset;		// frame offset
	AkUInt32			PausedTick;				// when it got paused
	UserParams			UserParam;
	AkUInt32			ulPauseCount;			// Pause Count
	AkPlayingID			TargetPlayingID;

	AkPendingAction( CAkRegisteredObj * in_pGameObj );
	virtual ~AkPendingAction();

	AkGameObjectID GameObjID();
	CAkRegisteredObj * GameObj() { return pGameObj; }

	virtual void TransUpdateValue( AkIntPtr in_eTargetType, AkReal32 in_fValue, bool in_bIsTerminated );

private:
	CAkRegisteredObj	* pGameObj;	// Game object pointer, made private to enforce refcounting
};

class CAkAudioMgr
{
	friend class CAkAudioThread;

public:
	CAkAudioMgr();
	~CAkAudioMgr();

	AKRESULT	Init();
	void		Term();

	//Start the AudioThread
	AKRESULT Start();

	//Stop the AudioThread 
	void Stop();

	//Reserves space in the message queue for a message.
	struct AkQueuedMsg* ReserveQueue( AkUInt16 in_eType, AkUInt32 in_uSize );
	void FinishQueueWrite() { AkAtomicDec32(&m_uMsgQueueWriters); }

	//Enqueues an End-of-list item if there are events to process and wakes up audio thread.
	AKRESULT RenderAudio( bool in_bAllowSyncRender );
	
	AkForceInline void WakeupEventsConsumer() { return m_audioThread.WakeupEventsConsumer(); }

	inline void IncrementBufferTick();

	AkUInt32 GetBufferTick(){ return m_uBufferTick; }

	template< typename T >
	typename T::IteratorEx FlushPendingItem( AkPendingAction* in_pPA, T & in_List, typename T::IteratorEx in_Iter )
	{
		typename T::IteratorEx itNext = in_List.Erase( in_Iter );
		in_pPA->pAction->Release();
		AkDelete(AkMemID_Object, in_pPA);
		return itNext;
	}

	AKRESULT MidiNoteOffExecuted( CAkAction* in_pAction, CAkParameterNodeBase * in_pMidiTargetNode, MidiEventActionType in_eMidiAction );

	AKRESULT BreakPendingAction( CAkParameterNodeBase * in_pNodeToTarget, CAkRegisteredObj * in_GameObj, AkPlayingID in_PlayingID );

	AKRESULT StopPendingAction(  CAkParameterNodeBase * in_pNodeToTarget, CAkRegisteredObj * in_GameObj, AkPlayingID in_PlayingID );

	AKRESULT StopPendingActionAllExcept( CAkRegisteredObj * in_GameObj, ExceptionList* in_pExceptionList, AkPlayingID in_PlayingID );

	// move actions from the pending list to the paused pending list
	void PausePendingAction(CAkParameterNodeBase * in_pNodeToTarget, CAkRegisteredObj * in_GameObj, bool in_bIsMasterOnResume, AkPlayingID in_PlayingID );
	void PausePendingAction( CAkAction* in_pAction );
	void PausePendingItems( AkPlayingID in_PlayingID );

	// move actions from the paused pending list to the pending list
	void ResumePausedPendingAction( CAkParameterNodeBase * in_pNodeToTarget, CAkRegisteredObj * in_GameObj, bool in_bIsMasterOnResume, AkPlayingID in_PlayingID );
	void ResumePausedPendingAction( CAkAction* in_pAction );
	void ResumePausedPendingItems( AkPlayingID in_playingID );

	// Cancel eventual pause transition in the pending list( not paused pending)
	void ResumeNotPausedPendingAction(CAkParameterNodeBase * in_pNodeToTarget, CAkRegisteredObj * in_GameObj, bool in_bIsMasterOnResume, AkPlayingID in_PlayingID );
	void ResumeNotPausedPendingAction( CAkAction* in_pAction );

	void PausePendingActionAllExcept( CAkRegisteredObj * in_GameObj, ExceptionList* in_pExceptionList, bool in_bIsMasterOnResume, AkPlayingID in_PlayingID );

	void ResumePausedPendingActionAllExcept( CAkRegisteredObj * in_GameObj, ExceptionList* in_pExceptionList, bool in_bIsMasterOnResume, AkPlayingID in_PlayingID );

	// Cancel eventual pause transition in the pending list( not paused pending)
	void ResumeNotPausedPendingActionAllExcept( CAkRegisteredObj * in_GameObj, ExceptionList* in_pExceptionList, bool in_bIsMasterOnResume, AkPlayingID in_PlayingID );

	// remove some actions from the pending list
	void RemovePendingAction( CAkParameterNodeBase * in_pNodeToTarget );

	// remove some actions from the paused pending list
	void RemovePausedPendingAction( CAkParameterNodeBase * in_pNodeToTarget );

	AKRESULT PausePending( AkPendingAction* in_pPA );
	AKRESULT StopPending( AkPendingAction* in_pPA );

	// figure out if anything has to come out of this list
	bool ProcessPendingList();

	void RemoveAllPausedPendingAction();
	void RemoveAllPendingAction();
	void RemoveAllPreallocAndReferences();

	void InsertAsPaused( AkUniqueID in_ElementID, AkPendingAction* in_pPendingAction, AkUInt32 in_ulPauseCount = 0 );
	void TransferToPending( AkPendingAction* in_pPendingAction );

	// This function return a pointer to the first action that is pending that corresponds to the
	// specified Playing ID. This is used to Recycle the content of a playing instance that
	// currently do not have PBI.
	//
	// Return - AkPendingAction* - pointer to the first action that is pending that corresponds to the
	// specified Playing ID. NULL if not found.
	AkPendingAction* GetActionMatchingPlayingID(
		AkPlayingID in_PlayingID	// Searched Playing ID
		);

	// Force the destruction of pending items associated to the specified PlayingID.
	void ClearPendingItems(
		AkPlayingID in_PlayingID
		);

	// Force the destruction of pending items associated to the specified PlayingID, but one of them do not return Abort
	void ClearPendingItemsExemptOne(
		AkPlayingID in_PlayingID
		);

	void ClearCrossFadeOccurence(
		CAkContinuousPBI* in_pPBIToCheck
		);

	// Execute an action or enqueue it in the pending queue depending if there is a delay
	// in_uiFrameOffset will be added to the delay - initially used to achieve sample accurate execution of events from music event cues
	void EnqueueOrExecuteAction(
		AkPendingAction* in_pActionItem, 
		AkUInt32 in_uiFrameOffset = 0
		);

	AKRESULT StopAction(AkUniqueID in_ActionID, AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID);
	AKRESULT PauseAction(AkUniqueID in_ActionID, AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID);
	AKRESULT ResumeAction(AkUniqueID in_ActionID,AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID);

	// the total size of the queue (in bytes)
	AkUInt32 GetActualQueueSize() {return m_MsgQueue.GetUsageSize();}
	AkUInt32 GetMaximumMsgSize() {return m_MsgQueue.GetMaxQueueSize();}
	
#ifndef AK_OPTIMIZED
	// 0.0 -> 1.0 (returns the greatest value since the previous query)
	AkUInt32 GetMaxSizeQueueFilled()
	{
		AkUInt32 tmp = m_MsgQueueSizeFilled; m_MsgQueueSizeFilled = 0; return tmp;
	}

	AkUInt32 GetMaxQueueSize()
	{
		return  m_MsgQueue.GetMaxQueueSize();
	}

	void InvalidatePendingPaths(AkUniqueID in_idElement);
#endif

	AkThreadID GetThreadID() { return m_audioThread.GetAudioThreadID(); }
	CAkAudioThread & GetAudioThread() { return m_audioThread; }

	AKRESULT SuspendWakeup(bool in_bSuspend, bool in_bRender, AkUInt32 in_uDelay);
	void InitSinkPlugin(AkOutputSettings& in_settings);
	void Perform();
	void ProcessCommunication();

	void ProcessAllActions(CAkEvent* in_pEvent, AkQueuedMsg_EventAction& in_rEventAction, CAkRegisteredObj* in_pGameObj);
	void ProcessAllActions(CAkEvent* in_pEvent, AkQueuedMsg_EventPostMIDI& in_rPostMIDI, CAkRegisteredObj* in_pGameObj);
	void ProcessAllActions(CAkEvent* in_pEvent, AkQueuedMsg_EventStopMIDI& in_rStopMIDI, CAkRegisteredObj* in_pGameObj);

	static bool m_bPipelineDirty;

private:	

	//Reserves space in the queue.
	inline AkQueuedMsg * ReserveForWrite(AkUInt32 &io_size);

	AkUInt32 ComputeFramesToRender();	

	// Add an action in the delayed action queue
	
	static void FlushAndCleanPendingAction( 
		AkPendingAction* in_pPendingAction 
		);

	void MonitorApiCalls();

	// the messages we get from the game side
	AkLockLessMsgQueue m_MsgQueue;

#ifndef AK_OPTIMIZED
	// how much of the queue is filled
	AkUInt32 m_MsgQueueSizeFilled;
#endif

	// the things that are not ready to be done
	typedef CAkMultiKeyList<AkUInt32, AkPendingAction*, AkAllocAndKeep> AkMultimapPending;
	AkMultimapPending m_mmapPending;

	// the things that are not ready to be done that got paused
	typedef CAkMultiKeyList<AkUniqueID, AkPendingAction*, AkAllocAndKeep> AkMultimapPausedPending;
	AkMultimapPausedPending m_mmapPausedPending;

	// Time actually in use by the Audio Mgr
	AkUInt32 m_uBufferTick;
public:
	AkAtomic32 m_iEndOfListCount; // Number of EndOfList items in command queue.

	// Take all the Events in the Event queue until it reaches 
	// the End Of List flag and process them all
	bool ProcessMsgQueue( bool in_bDrainMsgQueue, bool &out_bDeviceSwitch );
	
	// in_uiFrameOffset will be added to an action's delay - initially used to achieve sample accurate execution of events from music event cues
	static void ExecuteEvent( CAkEvent* in_pEvent, CAkRegisteredObj * in_pGameObj, AkGameObjectID in_GO, AkPlayingID in_playingID, AkPlayingID in_TargetPlayingID, const AkCustomParamType& in_rCustomParam, AkUInt32 in_uiFrameOffset = 0 );

	// execute those ready
	void ProcessAction( AkPendingAction * in_pAction );

	// execute a specific custom action
	void ProcessCustomAction( 
		CAkParameterNodeBase* ptargetNode, 
		CAkRegisteredObj * in_pGameObj, 
		AkUInt32 in_ActionToExecute,
		AkTimeMs in_uTransitionDuration,
		AkCurveInterpolation in_eFadeCurve,
		AkPlayingID	in_PlayingID
		);

	// post MIDI event to target
	void PostMIDIEvent( 
		CAkParameterNodeBase* in_pTargetNode, 
		CAkRegisteredObj * in_pGameObj, 
		AkQueuedMsg_EventPostMIDI & in_rPostMIDI
		);

	// post MIDI stop event to target(s)
	void PostMIDIStop( 
		CAkParameterNodeBase* in_pTargetNode, 
		CAkRegisteredObj* in_pGameObj
		);

    // play a specified target
	void PlaySourceInput(
		AkUniqueID in_Target, 
		CAkRegisteredObj * in_pGameObj,
        UserParams in_userParams
		);

	// Monitor notif
	void NotifyDelayStarted(
		AkPendingAction* in_pPending//Pointer to the pending action that was delayed
		);

	// Monitor notif
	void NotifyDelayAborted(
		AkPendingAction* in_pPending, //Pointer to the pending action that was aborted
		bool in_bWasPaused
		);

	// Monitor notif
	void NotifyDelayEnded(
		AkPendingAction* in_pPending, //Pointer to the pending action that just ended
		bool in_bWasPaused = false
		);

	void NotifyDelay(
		AkPendingAction* in_pPending, 
		AkMonitorData::NotificationReason in_Reason,
		bool in_bWasPaused
		);

	void WaitOnMsgQueuewriters();

	//Internal tool, allow to know it the specified action should be excepted by the call.
	bool IsAnException( CAkAction* in_pAction, ExceptionList* in_pExceptionList );

	bool IsElementOf( CAkParameterNodeBase * in_pNodeToTarget, CAkParameterNodeBase * in_pNodeToCheck );	
	
	void HandleLossOfHardwareResponse();	

	//Useful to prevent device timeout for lenghty operations
	//(e.g. sink init) executed during the frame
	void StampTimeLastBuffer();

	void DoProcessCommunication();

//----------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------

	CAkAudioThread	m_audioThread;
	AkInt64			m_timeLastBuffer;
	AkInt64			m_timeThisBuffer;
	AkUInt32		m_uCallsWithoutTicks;
	AkAtomic32		m_uMsgQueueWriters;
	AkReal32		m_fFractionalBuffer;

	struct AkMsgQueueHandlers
	{
		AkMsgQueueHandlers() : fcnExec(NULL), fcnRelease(NULL) {}
		AkMsgQueueHandler fcnExec;
		AkMsgQueueHandler fcnRelease;
	};

	//Use this version if the message requires a special release function to free memory, clean up, etc.
	AKRESULT RegisterMsgQueueHandler(AkMsgQueueHandler in_FcnExecute, AkMsgQueueHandler in_FcnReleaseData, AkUInt32 in_Id)
	{
		AkMsgQueueHandlers* ppFcn = m_MsgQueueHandlers.Set(in_Id);
		if (ppFcn)
		{
			(*ppFcn).fcnExec = in_FcnExecute;
			(*ppFcn).fcnRelease = in_FcnReleaseData;
			return AK_Success;
		}
		return AK_Fail;
	}

	AKRESULT RegisterMsgQueueHandler(AkMsgQueueHandler in_Fcn, AkUInt32 in_Id)
	{
		return RegisterMsgQueueHandler(in_Fcn, NULL, in_Id);
	}

	void UnregisterMsgQueueHandler(AkUInt32 in_Id)
	{
		m_MsgQueueHandlers.Unset(in_Id);
	}

	CAkKeyArray< AkUInt32, AkMsgQueueHandlers > m_MsgQueueHandlers;
};

extern CAkAudioMgr* g_pAudioMgr;
#endif
