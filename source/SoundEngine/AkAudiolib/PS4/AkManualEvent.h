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

#pragma once
#include <AK/Tools/Common/AkAssert.h>

class AkManualEvent
{
public:

	AkManualEvent()
	:m_bSignaled(false)	// Initially not signaled.
	{
		ScePthreadMutexattr	mutex_attr;
		AKVERIFY(!scePthreadMutexattrInit( &mutex_attr ));	
		AKVERIFY(!scePthreadMutexattrSetprotocol(&mutex_attr, SCE_PTHREAD_PRIO_INHERIT));
		AKVERIFY(!scePthreadMutexInit( &m_mutex, &mutex_attr, NULL));
		AKVERIFY(!scePthreadMutexattrDestroy( &mutex_attr ));
		AKVERIFY(!scePthreadCondInit(&m_condition, NULL, ""));
	}

	~AkManualEvent()
	{
		AKVERIFY(!scePthreadCondDestroy(&m_condition));
		AKVERIFY(!scePthreadMutexDestroy(&m_mutex));
	}

	inline AKRESULT Init()
	{
		m_bSignaled = false;	// Initially not signaled.
		return AK_Success;
	}

	inline void Wait()
	{
		scePthreadMutexLock(&m_mutex);
		if(!m_bSignaled)
			scePthreadCondWait(&m_condition,&m_mutex);
		scePthreadMutexUnlock(&m_mutex);
	}

	inline void Signal()
	{
		scePthreadMutexLock(&m_mutex);
		m_bSignaled = true;
		scePthreadCondBroadcast(&m_condition);
		scePthreadMutexUnlock(&m_mutex);
	}

	inline void Reset()
	{
		scePthreadMutexLock(&m_mutex);
		m_bSignaled = false;
		scePthreadMutexUnlock(&m_mutex);
	}

private:
	ScePthreadMutex m_mutex;
	ScePthreadCond m_condition;
	bool m_bSignaled;
};