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

#include "AkAjm.h"
#include <AK/Tools/Common/AkArray.h>

#define AK_AJM_DECODER_BATCH_SIZE (SCE_AJM_JOB_INITIALIZE_SIZE + SCE_AJM_JOB_SET_GAPLESS_DECODE_SIZE + SCE_AJM_JOB_SET_RESAMPLE_PARAMETERS_SIZE + SCE_AJM_JOB_GET_RESAMPLE_INFO_SIZE + SCE_AJM_JOB_DECODE_SPLIT_SIZE(4))

class CAkAjmScheduler
{
public:
	CAkAjmScheduler();

	AKRESULT Init();
	void Term();

	SceAjmContextId GetContextId() const;

	AKRESULT CreateInstance(SceAjmCodecType type, uint32_t flags, AkUInt32 bufferSize, AK::Ajm::Instance ** out_pInstance);
	void DestroyInstance(AK::Ajm::Instance * pInstance);

	AKRESULT SubmitJobs();
	void WaitForJobs();
	void CancelJobs();

private:

	struct Entry {
		AK::Ajm::Instance * instance;
		bool destroyed;
		const Entry& operator=(AK::Ajm::Instance * pInstance)
		{
			AKASSERT(pInstance != nullptr && pInstance->InstanceId > 0);
			instance = pInstance;
			destroyed = false;
			return *this;
		}
		bool operator==(AK::Ajm::Instance * pInstance)
		{
			AKASSERT(pInstance != nullptr && pInstance->InstanceId > 0);
			return instance->InstanceId == pInstance->InstanceId;
		}
	};
	typedef AkArray<Entry, AK::Ajm::Instance *, ArrayPoolLEngineDefault> DecoderList;

	void UpdateBatchStatus(int err);

	SceAjmContextId m_context;
	DecoderList m_decoders;

	CAkAjmBatchBuilder m_builder;
	AK::Ajm::Batch m_batch;
};

class CAkGlobalAjmScheduler
{
public:
	static AKRESULT Init();
	static void Term();

	static CAkAjmScheduler & Get();
private:
	static CAkAjmScheduler s_manager;
};
