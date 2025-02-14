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
// AkThreadedBankMgr.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include "AkThreadedBankMgr.h"
#include "AkAudioLib.h"

extern AkPlatformInitSettings g_PDSettings; //For thread properties.

#define AK_BANK_DEFAULT_LIST_SIZE 10

AkThread CAkThreadedBankMgr::m_BankMgrThread;
AkThreadID CAkThreadedBankMgr::m_idThread = 0;


CAkThreadedBankMgr::CAkThreadedBankMgr()
	: m_bStopThread( false )
{
	AkClearEvent( m_eventQueue );
	AkClearThread(&m_BankMgrThread);
}

CAkThreadedBankMgr::~CAkThreadedBankMgr()
{
}

AKRESULT CAkThreadedBankMgr::Init()
{
	AKRESULT eResult = CAkBankMgr::Init();
	if(eResult == AK_Success)
	{
		eResult = m_BankQueue.Reserve( AK_BANK_DEFAULT_LIST_SIZE );
	}
	if(eResult == AK_Success)
	{
		eResult = StartThread();
	}

	return eResult;
}

void CAkThreadedBankMgr::Term()
{
	while( m_BankQueue.Length() != 0 )
	{
		CAkThreadedBankMgr::AkBankQueueItem Item = m_BankQueue.First();
		m_BankQueue.RemoveFirst();
		Item.Term();
	}
	m_BankQueue.Term();

	CAkBankMgr::Term();
}

//====================================================================================================
//====================================================================================================
AKRESULT CAkThreadedBankMgr::StartThread()
{
	if(AkIsValidThread(&m_BankMgrThread))
	{
		AKASSERT( !"Wrong thread trying to start another thread." );
		return AK_Fail;
	}

	m_bStopThread = false;

	if ( AkCreateEvent( m_eventQueue ) != AK_Success )
	{
		AKASSERT( !"Could not create event required to start BankMgr thread." );
		return AK_Fail;
	}

    AkCreateThread(	BankThreadFunc,		// Start address
					this,				// Parameter
					g_PDSettings.threadBankManager, // Thread properties 
					&m_BankMgrThread,
					"AK::BankManager" );	// Name
	if ( !AkIsValidThread(&m_BankMgrThread) )
	{
		AKASSERT( !"Could not create bank manager thread" );
		return AK_Fail;
	}
	return AK_Success;
}
//====================================================================================================
//====================================================================================================
void CAkThreadedBankMgr::StopThread()
{
	m_bStopThread = true;
	if ( AkIsValidThread( &m_BankMgrThread ) )
	{
		// stop the eventMgr thread
		AkSignalEvent( m_eventQueue );
		AkWaitForSingleThread( &m_BankMgrThread );

		AkCloseThread( &m_BankMgrThread );
	}
	AkDestroyEvent( m_eventQueue );
}
//====================================================================================================
//====================================================================================================
AK_DECLARE_THREAD_ROUTINE( CAkThreadedBankMgr::BankThreadFunc )
{
	AK_THREAD_INIT_CODE(g_PDSettings.threadBankManager);
	CAkThreadedBankMgr& rThis = *AK_GET_THREAD_ROUTINE_PARAMETER_PTR(CAkThreadedBankMgr);

	AK_INSTRUMENT_THREAD_START( "CAkThreadedBankMgr::BankThreadFunc" );

	m_idThread = AKPLATFORM::CurrentThread();

	while(true)
	{
		AkWaitForEvent( rThis.m_eventQueue );

		if ( rThis.m_bStopThread )
			break;
			
		// Something in the queue!
		rThis.ExecuteCommand();
	}

	AK::MemoryMgr::TermForThread();
	AkExitThread(AK_RETURN_THREAD_OK);
}

AKRESULT CAkThreadedBankMgr::QueueBankCommand( AkBankQueueItem in_Item  )
{
	AkAutoLock<CAkLock> gate(m_queueLock);

	AKRESULT eResult = AK_Success;
	if( in_Item.callbackInfo.pfnBankCallback )
		eResult = m_CallbackMgr.AddCookie( in_Item.callbackInfo.pCookie );

	if(eResult == AK_Success)
	{
		eResult = m_BankQueue.AddLast( in_Item ) ? AK_Success : AK_Fail;
		if( in_Item.callbackInfo.pfnBankCallback && eResult != AK_Success )
			m_CallbackMgr.RemoveOneCookie( in_Item.callbackInfo.pCookie );
	}
	if(eResult == AK_Success)
	{
		BankMonitorNotification( in_Item );

		AkSignalEvent( m_eventQueue );
	}
	else
	{
		// Queuing failed.  Terminate queue item.
		in_Item.Term();
	}

	return eResult;
}

AKRESULT CAkThreadedBankMgr::InitSyncOp(AK::SoundEngine::AkSyncCaller& in_syncLoader)
{
	return in_syncLoader.Init();
}

AKRESULT CAkThreadedBankMgr::WaitForSyncOp(AK::SoundEngine::AkSyncCaller& in_syncLoader, AKRESULT in_eResult)
{
	return in_syncLoader.Wait(in_eResult);
}


AKRESULT CAkThreadedBankMgr::ExecuteCommand()
{
	AKRESULT eResult = AK_Success;
	while( true )
	{
		m_queueLock.Lock();
		if( m_BankQueue.Length() )
		{
			CAkThreadedBankMgr::AkBankQueueItem Item = m_BankQueue.First();
			m_BankQueue.RemoveFirst();
			m_queueLock.Unlock();

			CAkBankMgr::ExecuteCommand(Item);

			Item.Term();
		}
		else //Yes, the queue may be empty even if the semaphore was signaled, because a cancellation may have occurred
		{
			m_queueLock.Unlock();
			break; //exit while( true ) loop
		}
	}

	return eResult;
}

