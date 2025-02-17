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
// AkAudioThread.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkAudioThread.h"
#include "AkAudioMgr.h"
#include "AkLEngine.h"
#include "AkOutputMgr.h"
#include <mutex>

using namespace AKPLATFORM;

extern AkInitSettings g_settings;
extern AkPlatformInitSettings g_PDSettings;

AkThread		CAkAudioThread::m_hEventMgrThread;
AkThreadID		CAkAudioThread::m_hEventMgrThreadID;

CAkAudioThread::CAkAudioThread()
	:m_eventProcess( CAkLEngine::GetProcessEvent() )
	,m_bStopThread(false)
{
}

void CAkAudioThread::WakeupEventsConsumer()
{
	if ( m_eventProcess != NULL )
		::SetEvent( m_eventProcess );
}

AkEvent executeEventPerformEvent;

void executeEventPerform()
{
	g_pAudioMgr->Perform();
	AkSignalEvent( executeEventPerformEvent );
}


//-----------------------------------------------------------------------------
// Name: EventMgrThreadFunc
// Desc: Audio loop
//-----------------------------------------------------------------------------
AK_DECLARE_THREAD_ROUTINE(CAkAudioThread::EventMgrThreadFunc)
{
	AK_INSTRUMENT_THREAD_START( "CAkAudioThread::EventMgrThreadFunc" );

	m_hEventMgrThreadID = AKPLATFORM::CurrentThread();

	// get our info from the parameter
	CAkAudioThread* pAudioThread = AK_GET_THREAD_ROUTINE_PARAMETER_PTR( CAkAudioThread );

	CAkLEngine::OnThreadStart();

	AKASSERT(g_pAudioMgr);

	DWORD dwWaitRes = WAIT_TIMEOUT;
	do
	{
		{
			switch ( dwWaitRes )
			{
				case WAIT_OBJECT_0:			//ThreadProcessEvent
				case WAIT_TIMEOUT:			// Default Time Out
					g_pAudioMgr->Perform();
					break;
				default:
					AKASSERT( !"Unexpected event received on main thread" );
			}
		}

		DWORD msWait = CAkOutputMgr::IsSuspended() ? INFINITE : AK_PC_WAIT_TIME;
#ifdef AK_USE_UWP_API
		dwWaitRes = ::WaitForSingleObjectEx(pAudioThread->m_eventProcess, msWait, FALSE);
#else
		dwWaitRes = ::WaitForSingleObject(pAudioThread->m_eventProcess, msWait);
#endif
    }
	while ( !pAudioThread->m_bStopThread );

	CAkLEngine::OnThreadEnd();

	AK::MemoryMgr::TermForThread();
	AkExitThread( AK_RETURN_THREAD_OK );
}

AKRESULT CAkAudioThread::Start()
{
	m_bStopThread = false;	

	//Create the EventManagerThread
	AkCreateThread(	EventMgrThreadFunc,					// Start address
					this,								// Parameter
					g_PDSettings.threadLEngine,			// Properties 
					&m_hEventMgrThread,					// AkHandle
					"AK::EventManager" );				// Debug name
	// is the thread ok ?
	if ( !AkIsValidThread(&m_hEventMgrThread) )
		return AK_Fail;

	return AK_Success;
}

void CAkAudioThread::Stop()
{
	m_bStopThread = true;
	if ( AkIsValidThread( &m_hEventMgrThread ) )
	{
		WakeupEventsConsumer();
		AkWaitForSingleThread( &m_hEventMgrThread );
		AkCloseThread(&m_hEventMgrThread);
		AkClearThread(&m_hEventMgrThread);
		m_hEventMgrThreadID = 0;
	}	
}
