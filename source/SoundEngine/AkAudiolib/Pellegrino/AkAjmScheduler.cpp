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

#include "AkAjmScheduler.h"
#include "AkMonitor.h"

// Allocate for five concurrent AJM voices
#define AK_AJM_INITIAL_BATCH_SIZE (AK_AJM_DECODER_BATCH_SIZE * 5)

CAkAjmScheduler::CAkAjmScheduler()
	: m_context()
	, m_decoders()
	, m_builder()
	, m_batch{ 0, AK::Ajm::Batch::Status::Invalid }
{
}

AKRESULT CAkAjmScheduler::Init()
{
	int result = sceAjmInitialize(SCE_AJM_INITIALIZE_FLAG_OUTPUT_ZERO_COPY, &m_context);
	if (result != SCE_OK)
	{
		MONITOR_ERROR(AK::Monitor::ErrorCode_HwVoicesSystemInitFailed);
		return AK::SceStatusToAkResult(result);
	}
	result = sceAjmModuleRegister(m_context, SCE_AJM_CODEC_DEC_AT9, 0);
	if (result != SCE_OK)
	{
		MONITOR_ERROR(AK::Monitor::ErrorCode_HwVoicesSystemInitFailed);
		Term();
		return AK::SceStatusToAkResult(result);
	}
	result = sceAjmModuleRegister(m_context, SCE_AJM_CODEC_DEC_OPUS, 0);
	if (result != SCE_OK)
	{
		MONITOR_ERROR(AK::Monitor::ErrorCode_HwVoicesSystemInitFailed);
		Term();
		return AK::SceStatusToAkResult(result);
	}
	AKRESULT eResult = m_builder.Init(AK_AJM_INITIAL_BATCH_SIZE);
	if (eResult != AK_Success)
	{
		MONITOR_ERROR(AK::Monitor::ErrorCode_HwVoicesSystemInitFailed);
		Term();
	}
	return eResult;
}

void CAkAjmScheduler::Term()
{
	CancelJobs();
	// All lingering decoders should already be marked for destruction. Clean them up here.
	for (DecoderList::Iterator it = m_decoders.Begin(); it != m_decoders.End(); ++it)
	{
		Entry& entry = *it;
		AKASSERT(entry.destroyed);
		AK::Ajm::Instance * pInstance = (*it).instance;
		if (pInstance)
		{
			AK::Ajm::DestroyInstance(m_context, pInstance);
		}
	}
	m_builder.Term();
	m_decoders.Term();
	if (m_context > 0)
	{
		AKVERIFY(sceAjmModuleUnregister(m_context, SCE_AJM_CODEC_DEC_OPUS) == SCE_OK);
		AKVERIFY(sceAjmModuleUnregister(m_context, SCE_AJM_CODEC_DEC_AT9) == SCE_OK);
		AKVERIFY(sceAjmFinalize(m_context) == SCE_OK);
		m_context = 0;
	}
}

SceAjmContextId CAkAjmScheduler::GetContextId() const
{
	return m_context;
}

AKRESULT CAkAjmScheduler::CreateInstance(SceAjmCodecType type, uint32_t flags, AkUInt32 bufferSize, AK::Ajm::Instance ** out_pInstance)
{
	if (m_context <= 0) return AK_DeviceNotReady;
	AK::Ajm::Instance * pInstance;
	AKRESULT eResult = AK::Ajm::CreateInstance(m_context, type, flags, bufferSize, &pInstance);
	if (eResult != AK_Success) return eResult;
	AKASSERT(pInstance != nullptr);
	AKASSERT(m_decoders.FindEx(pInstance) == m_decoders.End());

	Entry * entry = m_decoders.AddLast(pInstance);
	if (!entry)
	{
		AK::Ajm::DestroyInstance(m_context, pInstance);
		return AK_InsufficientMemory;
	}
	*out_pInstance = pInstance;

	return AK_Success;
}

void CAkAjmScheduler::DestroyInstance(AK::Ajm::Instance * pInstance)
{
	if (m_context <= 0) return;
	DecoderList::Iterator it = m_decoders.FindEx(pInstance);
	if (it != m_decoders.End())
	{
		// Delay destruction until batch is finished to avoid write-after-free errors
		(*it).destroyed = true;
	}
}

AKRESULT CAkAjmScheduler::SubmitJobs()
{
	if (m_context <= 0) return AK_DeviceNotReady;
	AK_INSTRUMENT_SCOPE("CAkAjmScheduler::SubmitJobs");

	// Previous batch may not have been resolved yet.
	// This can happen if all HW voices in a batch have been stopped or gone virtual.
	// Make sure that there are no two batches queued at the same time!
	WaitForJobs();

	AKRESULT eResult = m_builder.Reset(AK_AJM_DECODER_BATCH_SIZE * m_decoders.Length());
	if (eResult != AK_Success)
		return eResult;

	AkUInt32 uMinReadCount = AK_AJM_NUM_BUFFERS;
	for (DecoderList::Iterator it = m_decoders.Begin(); it != m_decoders.End(); ++it)
	{
		Entry& entry = *it;
		if (entry.destroyed)
		{
			AK::Ajm::Instance * pInstance = (*it).instance;
			if (pInstance)
			{
				AK::Ajm::DestroyInstance(m_context, pInstance);
			}
			m_decoders.Erase(it);
			--it;
		}
		else if ((entry.instance->JobFlags & ~AK_AJM_JOBS_PENDING) != 0)
		{
			AKASSERT(entry.instance->BatchStatus != AK::Ajm::Batch::Status::InProgress);
			m_builder.AddJobs(entry.instance);
			entry.instance->JobFlags = AK_AJM_JOBS_PENDING;
			uMinReadCount = AkMin(uMinReadCount, entry.instance->uReadCount);
		}
	}

	// Since starting/waiting on an AJM batch is very expensive, we only do it
	// when one of the current AJM voices nears starvation. When this happens,
	// the other voices will also participate even if not about to starve,
	// thus synchronizing all current voices for future batches
	if (uMinReadCount <= AK_AJM_MIN_BUFFERS)
	{
		AKASSERT(m_batch.status != AK::Ajm::Batch::Status::InProgress);
		AKRESULT eStartResult = m_builder.StartBatch(m_context, SCE_AJM_PRIORITY_GAME_DEFAULT, m_batch);
		if (eStartResult != AK_Success)
		{
			m_batch.status = AK::Ajm::Batch::Status::Invalid;
			MONITOR_ERROR(AK::Monitor::ErrorCode_HwVoicesDecodeBatchFailed);
		}
		else
		{
			m_batch.status = AK::Ajm::Batch::Status::InProgress;
		}
	}
	else
	{
		m_batch.status = AK::Ajm::Batch::Status::Invalid;
	}
	for (DecoderList::Iterator it = m_decoders.Begin(); it != m_decoders.End(); ++it)
	{
		Entry& entry = *it;
		if (entry.instance->JobFlags == AK_AJM_JOBS_PENDING)
		{
			entry.instance->BatchStatus = m_batch.status;
			entry.instance->JobFlags = 0;
		}
	}
	return eResult;
}

void CAkAjmScheduler::WaitForJobs()
{
	if (m_context <= 0) return;
	if (m_batch.status == AK::Ajm::Batch::Status::InProgress)
	{
		AK_INSTRUMENT_SCOPE("CAkAjmScheduler::WaitForJobs");
		int err = sceAjmBatchWait(m_context, m_batch.id, SCE_AJM_WAIT_INFINITE, &m_batch.error);
		AK_AJM_DEBUG_PRINTF("[AJM] sceAjmBatchWait(%d, %d, %d) => %d, %d\n", m_context, m_batch.id, SCE_AJM_WAIT_INFINITE, err, m_batch.error.iErrorCode);
		if (err != SCE_OK)
		{
			MONITOR_ERROR(AK::Monitor::ErrorCode_HwVoicesDecodeBatchFailed);
		}
		UpdateBatchStatus(err);
	}
}

void CAkAjmScheduler::CancelJobs()
{
	if (m_context <= 0) return;
	if (m_batch.status == AK::Ajm::Batch::Status::InProgress)
	{
		AK_INSTRUMENT_SCOPE("CAkAjmScheduler::CancelJobs");
		int r = sceAjmBatchCancel(m_context, m_batch.id);
		AK_AJM_DEBUG_PRINTF("[AJM] sceAjmBatchCancel(%d, %d) => %d\n", m_context, m_batch.id, r);
		AKVERIFY(r == SCE_OK);
		// We MUST wait for the batch to finish cancelling. Otherwise AJM memory may still be busy when we try to Term() the Sound Engine.
		r = sceAjmBatchWait(m_context, m_batch.id, SCE_AJM_WAIT_INFINITE, &m_batch.error);
		AK_AJM_DEBUG_PRINTF("[AJM] sceAjmBatchWait(%d, %d, %d) => %d, %d\n", m_context, m_batch.id, SCE_AJM_WAIT_INFINITE, r, m_batch.error.iErrorCode);
		AKVERIFY(SCE_OK == r || SCE_AJM_ERROR_CANCELLED == r);
		UpdateBatchStatus(SCE_AJM_ERROR_CANCELLED);
	}
}

void CAkAjmScheduler::UpdateBatchStatus(int err)
{
	switch (err)
	{
	case SCE_AJM_ERROR_CANCELLED:
		m_batch.status = AK::Ajm::Batch::Status::Cancelled;
		break;
	case SCE_AJM_ERROR_IN_PROGRESS:
		m_batch.status = AK::Ajm::Batch::Status::InProgress;
		break;
	case SCE_OK:
		m_batch.status = AK::Ajm::Batch::Status::Success;
		break;
	default:
		m_batch.status = AK::Ajm::Batch::Status::Failure;
	}
	// Record the batch status in each voice that participated in the batch.
	// This allows the voice to process the results in a future frame.
	// For example, voices for Interactive Music will start decoding ~4 frames before
	// the first buffer is actually needed.
	for (DecoderList::Iterator it = m_decoders.Begin(); it != m_decoders.End(); ++it)
	{
		Entry& entry = *it;
		if (entry.instance->BatchStatus == AK::Ajm::Batch::Status::InProgress)
		{
			entry.instance->BatchStatus = m_batch.status;
		}
	}
}

CAkAjmScheduler CAkGlobalAjmScheduler::s_manager;

AKRESULT CAkGlobalAjmScheduler::Init()
{
	return s_manager.Init();
}

void CAkGlobalAjmScheduler::Term()
{
	s_manager.Term();
}

CAkAjmScheduler& CAkGlobalAjmScheduler::Get()
{
	return s_manager;
}
