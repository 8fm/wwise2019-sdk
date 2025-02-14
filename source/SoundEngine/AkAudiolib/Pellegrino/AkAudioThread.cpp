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
#include "AkRandom.h"
#include "AkLEngine.h"

using namespace AKPLATFORM;

extern AkInitSettings g_settings;
extern AkPlatformInitSettings g_PDSettings;

AkThread		CAkAudioThread::m_hEventMgrThread;
AkThreadID		CAkAudioThread::m_hEventMgrThreadID;

CAkAudioThread::CAkAudioThread():
	m_bStopThread(false) 
{
	AkClearEvent(m_eventProcess);
	AkClearThread(&m_hEventMgrThread);
}

void CAkAudioThread::WakeupEventsConsumer()
{
	if(m_eventProcess != NULL)
		AkSignalEvent( m_eventProcess );
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

	// 1,000,000 to convert from seconds to uSec
	// 1 / 4.0 to wait up to 4 times between each major tick
	SceKernelUseconds defaultTickWaitTime = (SceKernelUseconds)(1000000.0 * (AK_NUM_VOICE_REFILL_FRAMES / AK_CORE_SAMPLERATE) * (1 / 4.0));
	AKASSERT( g_pAudioMgr );
	do
    {
		g_pAudioMgr->Perform();


		// wait for eventProcess to be flagged - or otherwise wait for the requisite quantum of time between ticks
		SceKernelUseconds usWait = CAkOutputMgr::IsSuspended() ? UINT_MAX : defaultTickWaitTime;
		int ret = sceKernelWaitEventFlag(
			pAudioThread->m_eventProcess,
			1,
			SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_ALL,
			SCE_NULL,
			&usWait);
		AKVERIFY(ret == SCE_OK || ret == SCE_KERNEL_ERROR_ETIMEDOUT);
	}
	while ( !pAudioThread->m_bStopThread );

	CAkLEngine::OnThreadEnd();
	
	AK::MemoryMgr::TermForThread();
    AkExitThread( AK_RETURN_THREAD_OK );
}

AKRESULT CAkAudioThread::Start()
{	
	AKRESULT ret = AkCreateNamedEvent( m_eventProcess, "AudioThread" );
	
	if ( ret != AK_Success ) 
	{
		AkClearEvent(m_eventProcess);
		return AK_Fail;
	}
	
	m_bStopThread	= false;
	
	if(g_settings.bUseLEngineThread)
	{
		//Create the EventManagerThread
		AkCreateThread(EventMgrThreadFunc,					// Start address
			this,								// Parameter
			g_PDSettings.threadLEngine,			// Properties 
			&m_hEventMgrThread,					// AkHandle
			"AK::EventManager");				// Debug name

		// is the thread ok ?
		if(!AkIsValidThread(&m_hEventMgrThread))
			return AK_Fail;
	}
	
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
	
	AkDestroyEvent( m_eventProcess );
	
	AkCreateNamedEvent( m_eventProcess, "CAkAudioThread::Stop" );
}
