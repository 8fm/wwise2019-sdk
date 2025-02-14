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

#include <ajm.h>
#include <AK/Tools/Common/AkListBare.h>
#include <AK/Tools/Common/AkLock.h>

class CAkSrcBankAt9;
class CAkSrcFileAt9;

class CAkACPManager
{
public:
	CAkACPManager();
	~CAkACPManager();

	AKRESULT Init();
	void Term();
	
	AKRESULT Register(CAkSrcBankAt9* pACPSrc);
	AKRESULT Register(CAkSrcFileAt9* pACPSrc);
	void Unregister(CAkSrcBankAt9* pACPSrc);
	void Unregister(CAkSrcFileAt9* pACPSrc);

	// Returns true if after calling RunDecoderBatch(), some voices need another batch to produces the amount of data needed for the frame.
    inline bool NeedAnotherPass(){ return m_bNeedAnotherPass; }

	void SubmitDecodeBatch();

    //Wait up to in_uTimeoutMs for the current decode batch to be finished.
    //  in_uTimeoutMs - pass 0 to poll, or SCE_AJM_WAIT_INFINITE to wait forever.
    //  returns true if batch was completed.
	bool Wait(unsigned int in_uTimeoutMs = SCE_AJM_WAIT_INFINITE);
    inline bool Poll() {return Wait(0);}

	// A batch job has been submitted.
	bool IsBusy(){return m_batchId != SCE_AJM_BATCH_INVALID;}
	
#ifndef AK_OPTIMIZED
	AkReal32 GetDSPUsage(){return m_stats.sEngine.fUsageBatch;}
private:
	 SceAjmGetStatisticsResult m_stats;
#endif

public:
	static inline CAkACPManager& Get() { return s_ACPManager; }
private:
	static CAkACPManager		s_ACPManager;

	SceAjmContextId m_AjmContextId;
	
	AkListBare<CAkSrcBankAt9, AkListBareNextItem, AkCountPolicyNoCount> m_ACPBankSrc;
	AkListBare<CAkSrcFileAt9, AkListBareNextItem, AkCountPolicyNoCount> m_ACPFileSrc;
	
	uint8_t* m_pAcpBatchBuffer;
	SceAjmBatchInfo m_batchInfo;
	volatile SceAjmBatchId m_batchId;

	CAkLock m_lockWait;

    bool m_bNeedAnotherPass;
};