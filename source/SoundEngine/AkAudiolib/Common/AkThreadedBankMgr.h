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
// AkThreadedBankMgr.h
//
//////////////////////////////////////////////////////////////////////

#ifndef _THREADED_BANK_MGR_H_
#define _THREADED_BANK_MGR_H_

#include "AkBankMgr.h"

class CAkBankMemMgr;
class CAkParameterNode;
class CAkUsageSlot;
class CAkThreadedBankMgr;

// The Threaded version of the Bank Manager
//		Manage bank slots allocation
//		Loads and unloads banks
class CAkThreadedBankMgr: public CAkBankMgr
{
public:
	
	// Constructor and Destructor
	CAkThreadedBankMgr();
	virtual ~CAkThreadedBankMgr();

	virtual AKRESULT Init();
	virtual void Term();

	virtual AKRESULT QueueBankCommand( CAkBankMgr::AkBankQueueItem in_Item );

	virtual AKRESULT InitSyncOp(AK::SoundEngine::AkSyncCaller& in_syncLoader);
	virtual AKRESULT WaitForSyncOp(AK::SoundEngine::AkSyncCaller& in_syncLoader, AKRESULT in_eResult);
	
	virtual void StopThread();

	virtual void CancelCookie( void* in_pCookie ){ m_CallbackMgr.CancelCookie( in_pCookie ); }

	static AkThreadID GetThreadID() {return m_idThread;}

	virtual AKRESULT KillSlot(CAkUsageSlot * in_pUsageSlot, AkBankCallbackFunc in_pCallBack, void* in_pCookie){ return KillSlotAsync(in_pUsageSlot, in_pCallBack, in_pCookie); }

private:

// threading
    static AK_DECLARE_THREAD_ROUTINE( BankThreadFunc );
	AKRESULT StartThread();

	AKRESULT ExecuteCommand();

	static AkThread	m_BankMgrThread;
	static AkThreadID m_idThread;

	AkEvent m_eventQueue;
	bool	m_bStopThread;

	CAkLock m_queueLock;

	typedef CAkList2<AkBankQueueItem, const AkBankQueueItem&, AkAllocAndFree> AkBankQueue;
	AkBankQueue m_BankQueue;

};

#endif



