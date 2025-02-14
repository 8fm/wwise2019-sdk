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

#include "stdafx.h"
#include "AkAjm.h"
#include <AK/Tools/Common/AkObject.h>

#define AK_RETURN_IF_NOT_SCE_OK(__var__) if (SCE_OK != (__var__)) return AK::SceStatusToAkResult(__var__)

namespace AK
{
	AKRESULT SceStatusToAkResult(int status)
	{
		switch (status) {
		case SCE_OK:
			return AK_Success;
		case SCE_AJM_ERROR_OUT_OF_MEMORY: // Fallthrough
		case SCE_AJM_ERROR_OUT_OF_RESOURCES:
			return AK_MaxReached;
		case SCE_AJM_ERROR_MALFORMED_BATCH:
			return AK_InvalidParameter;
		case SCE_AJM_ERROR_INVALID_BATCH: // Fallthrough
		case SCE_AJM_ERROR_INVALID_CONTEXT:
			return AK_IDNotFound;
		default:
			return AK_Fail;
		}
	}
}

CAkAjmBatchBuilder::CAkAjmBatchBuilder()
	: m_batchBuffer(NULL)
	, m_batchSize(0)
	, m_batchCreated(false)
{
}

CAkAjmBatchBuilder::~CAkAjmBatchBuilder()
{
	AKASSERT(m_batchBuffer == nullptr);
}

AKRESULT CAkAjmBatchBuilder::Init(size_t batchSize)
{
	m_batchBuffer = AkAlloc(AkMemID_Processing, batchSize);
	AKRESULT eResult = m_batchBuffer ? AK_Success : AK_InsufficientMemory;
	if (eResult == AK_Success)
	{
		m_batchSize = batchSize;
		return Reset(m_batchSize);
	}
	return eResult;
}

void CAkAjmBatchBuilder::Term()
{
	if (m_batchBuffer)
	{
		AkFree(AkMemID_Processing, m_batchBuffer);
		m_batchBuffer = NULL;
	}
}

#define INSTANCE_HAS_FLAG(f) ((pInstance->JobFlags & f) == f)

void CAkAjmBatchBuilder::AddJobs(AK::Ajm::Instance * pInstance)
{
	AKASSERT(!m_batchCreated);

	if (INSTANCE_HAS_FLAG(AK_AJM_JOB_INITIALIZE_AT9))
	{
		AK_AJM_DEBUG_PRINTF("[AJM] sceAjmBatchJobInitialize(%d)\n", pInstance->InstanceId);
		AKVERIFY(SCE_OK == sceAjmBatchJobInitialize(
			&m_batchInfo,
			pInstance->InstanceId,
			&pInstance->AT9InitParams,
			sizeof(pInstance->AT9InitParams),
			&pInstance->InitResult));
	}
	if (INSTANCE_HAS_FLAG(AK_AJM_JOB_INITIALIZE_OPUS))
	{
		AK_AJM_DEBUG_PRINTF("[AJM] sceAjmBatchJobInitialize(%d)\n", pInstance->InstanceId);
		AKVERIFY(SCE_OK == sceAjmBatchJobInitialize(
			&m_batchInfo,
			pInstance->InstanceId,
			&pInstance->OpusInitParams,
			sizeof(pInstance->OpusInitParams),
			&pInstance->InitResult));
	}
	if (INSTANCE_HAS_FLAG(AK_AJM_JOB_SET_GAPLESS_DECODE))
	{
		AK_AJM_DEBUG_PRINTF("[AJM] sceAjmBatchJobSetGaplessDecode(%d, %d, %d, %d)\n", pInstance->InstanceId, pInstance->GaplessDecode.Params.uiSkipSamples, pInstance->GaplessDecode.Params.uiTotalSamples, pInstance->GaplessDecode.reset);
		AKVERIFY(SCE_OK == sceAjmBatchJobSetGaplessDecode(
			&m_batchInfo,
			pInstance->InstanceId,
			&pInstance->GaplessDecode.Params,
			pInstance->GaplessDecode.reset,
			&pInstance->GaplessDecode.Result));
	}
	if (INSTANCE_HAS_FLAG(AK_AJM_JOB_SET_RESAMPLE_PARAMETERS))
	{
		AK_AJM_DEBUG_PRINTF("[AJM] sceAjmBatchJobSetResampleParametersEx(%d, %f, %f, %d)\n", pInstance->InstanceId, pInstance->Resampler.RatioStart, pInstance->Resampler.RatioStep, pInstance->Resampler.Flags);
		AKVERIFY(SCE_OK == sceAjmBatchJobSetResampleParametersEx(
			&m_batchInfo,
			pInstance->InstanceId,
			pInstance->Resampler.RatioStart,
			pInstance->Resampler.RatioStep,
			pInstance->Resampler.Flags,
			&pInstance->Resampler.Result));
	}
	if (INSTANCE_HAS_FLAG(AK_AJM_JOB_DECODE_SPLIT))
	{
		size_t numInputs = pInstance->InputDescriptors[1].szSize == 0 ? 1 : 2;
		size_t numOutputs = pInstance->OutputDescriptors[1].szSize == 0 ? 1 : 2;

		auto in = pInstance->InputDescriptors;
		auto out = pInstance->OutputDescriptors;

		sceAjmBatchJobDecodeSplit(
			&m_batchInfo,
			pInstance->InstanceId,
			pInstance->InputDescriptors, numInputs,
			pInstance->OutputDescriptors, numOutputs,
			&pInstance->DecodeResult);
#if defined(AK_AJM_DEBUG_HASH)
		AK_AJM_DEBUG_PRINTF(
			"[AJM] sceAjmBatchJobDecodeSplit(%d, [%u,%zu,%llx], [%u,%zu,%llx], [%u,%zu], [%u,%zu])\n",
			pInstance->InstanceId,
			pInstance->ReadHead, in[0].szSize, HashBuffer(in[0].pAddress, in[0].szSize),
			0, in[1].szSize, HashBuffer(in[1].pAddress, in[1].szSize),
			pInstance->WriteHead, out[0].szSize,
			0, out[1].szSize
		);
#else
		AK_AJM_DEBUG_PRINTF(
			"[AJM] sceAjmBatchJobDecodeSplit(%d, [%u,%zu], [%u,%zu], [%u,%zu], [%u,%zu])\n",
			pInstance->InstanceId,
			pInstance->ReadHead, in[0].szSize,
			0, in[1].szSize,
			pInstance->WriteHead, out[0].szSize,
			0, out[1].szSize
		);
#endif
	}
	if (INSTANCE_HAS_FLAG(AK_AJM_JOB_GET_RESAMPLE_INFO))
	{
		AK_AJM_DEBUG_PRINTF("[AJM] sceAjmBatchJobGetResampleInfo(%d)\n", pInstance->InstanceId);
		AKVERIFY(SCE_OK == sceAjmBatchJobGetResampleInfo(
			&m_batchInfo,
			pInstance->InstanceId,
			&pInstance->Resampler.Info));
	}
}

AKRESULT CAkAjmBatchBuilder::StartBatch(SceAjmContextId contextId, unsigned int priority, AK::Ajm::Batch &out_batch)
{
	AK_INSTRUMENT_SCOPE("CAkAjmBatchBuilder::StartBatch");
	AKASSERT(!m_batchCreated);
	m_batchCreated = true;
	int err = sceAjmBatchStart(contextId, &m_batchInfo, priority, &out_batch.error, &out_batch.id);
	AK_AJM_DEBUG_PRINTF("[AJM] sceAjmBatchStart(%d, %d) => %d, %d, %d\n", contextId, priority, err, out_batch.error.iErrorCode, out_batch.id);
	return AK::SceStatusToAkResult(err);
}

AKRESULT CAkAjmBatchBuilder::Reset(size_t newBatchSize)
{
	if (m_batchSize < newBatchSize)
	{
		AKRESULT eResult = GrowBuffer(newBatchSize);
		if (eResult != AK_Success)
			return eResult;
	}
	int r = sceAjmBatchInitialize(m_batchBuffer, m_batchSize, &m_batchInfo);
	m_batchCreated = false;
	return AK::SceStatusToAkResult(r);
}

AKRESULT CAkAjmBatchBuilder::GrowBuffer(size_t newBatchSize)
{
	m_batchBuffer = AkRealloc(AkMemID_Processing, m_batchBuffer, newBatchSize);
	if (m_batchBuffer == nullptr)
	{
		return AK_InsufficientMemory;
	}
	m_batchSize = newBatchSize;
	return AK_Success;
}

AKRESULT AK::Ajm::CreateInstance(SceAjmContextId ctx, SceAjmCodecType type, uint32_t flags, AkUInt32 bufferSize, Instance ** out_pInstance)
{
	AKASSERT(bufferSize % AK_AJM_BUFFER_ALIGNMENT == 0);
	Instance * pInstance = AkNew(AkMemID_Processing, Instance());
	if (!pInstance)
	{
		return AK_InsufficientMemory;
	}

	pInstance->InstanceId = 0;
	pInstance->JobFlags = 0;
	pInstance->BatchStatus = Batch::Status::Invalid;
	pInstance->RangeStart = 0;
	pInstance->RangeProgress = 0;
	pInstance->RangeLength = 0;
	pInstance->uReadCount = 0;
	pInstance->uWriteCount = AK_AJM_NUM_BUFFERS;
	pInstance->ReadHead = 0;
	pInstance->WriteHead = 0;
	pInstance->State = Instance::InstanceState::Uninitialized;
	pInstance->BufferSize = bufferSize;

	pInstance->pBuffer = (AkUInt8*)AkMalign(AkMemID_Processing | AkMemType_Device, pInstance->BufferSize * AK_AJM_NUM_BUFFERS, AK_AJM_BUFFER_ALIGNMENT);
	if (!pInstance->pBuffer)
	{
		AkDelete(AkMemID_Processing, pInstance);
		return AK_InsufficientMemory;
	}

	int result = sceAjmInstanceCreate(
		ctx,
		type,
		flags,
		&pInstance->InstanceId);
	if (result != SCE_OK)
	{
		AkDelete(AkMemID_Processing, pInstance->pBuffer);
		AkDelete(AkMemID_Processing, pInstance);
		switch (result) {
		case SCE_AJM_ERROR_OUT_OF_MEMORY: // Fallthrough
		case SCE_AJM_ERROR_OUT_OF_RESOURCES:
			return AK_MaxReached;
		default:
			return AK_DeviceNotReady;
		}
	}
	AK_AJM_DEBUG_PRINTF("[AJM] sceAjmInstanceCreate(%d, %d, %d) => %d\n", ctx, type, flags, pInstance->InstanceId);
	*out_pInstance = pInstance;
	return AK_Success;
}

void AK::Ajm::DestroyInstance(SceAjmContextId ctx, Instance * pInstance)
{
	if (pInstance->InstanceId != 0)
	{
		sceAjmInstanceDestroy(ctx, pInstance->InstanceId);
		AK_AJM_DEBUG_PRINTF("[AJM] sceAjmInstanceDestroy(%d, %d)\n", ctx, pInstance->InstanceId);
	}
	if (pInstance->pBuffer)
	{
		AkFalign(AkMemID_Processing | AkMemType_Device, pInstance->pBuffer);
		pInstance->pBuffer = nullptr;
	}
	AkDelete(AkMemID_Processing, pInstance);
}

void AK::Ajm::SetRange(Instance * pInstance, AkUInt32 start, AkUInt32 length, AkUInt32 remainder)
{
	// When setting a new range, we must "accomodate space" for any samples held up in resampler
	pInstance->RangeStart = start;
	pInstance->RangeProgress = 0 - remainder;
	pInstance->RangeLength = length;
	if (pInstance->State != Instance::InstanceState::Uninitialized)
	{
		pInstance->State = Instance::InstanceState::IdleStartOfRange;
	}
}

void AK::Ajm::ClearBuffer(Instance * pInstance)
{
	pInstance->WriteHead = 0;
	pInstance->ReadHead = 0;
	pInstance->uReadCount = 0;
	AkZeroMemSmall(pInstance->BufferMeta, sizeof(pInstance->BufferMeta));
}

AKRESULT AK::Ajm::RecordJobResults(Instance * pInstance, bool bLastRange)
{
	AK_RETURN_IF_NOT_SCE_OK(pInstance->InitResult.sResult.iResult);
	AK_RETURN_IF_NOT_SCE_OK(pInstance->GaplessDecode.Result.sResult.iResult);
	AK_RETURN_IF_NOT_SCE_OK(pInstance->DecodeResult.sResult.iResult & ~(SCE_AJM_RESULT_PARTIAL_INPUT | SCE_AJM_RESULT_NOT_ENOUGH_ROOM));

	bool bAtEnd = pInstance->DecodeResult.sStream.uiTotalDecodedSamples == pInstance->RangeLength;

	// Calculate number of source frames produced
	AkInt32 uSamplesInResampler = pInstance->Resampler.Info.sResampleInfo.iNumSamples > 0 ? pInstance->Resampler.Info.sResampleInfo.iNumSamples : 0;
	AkInt64 uTotalReceivedSamples = pInstance->DecodeResult.sStream.uiTotalDecodedSamples - uSamplesInResampler;
	AKASSERT((AkInt32)uTotalReceivedSamples >= pInstance->RangeProgress);
	AkInt32 uNumFramesProduced = uTotalReceivedSamples - pInstance->RangeProgress;

	// Calculate number of bytes produced
	size_t uNumBytesProduced = pInstance->DecodeResult.sStream.iSizeProduced;
	double fFramesPerBytes = (double)uNumFramesProduced / uNumBytesProduced;

	// Distribute both values proportionally in the available output buffers
	while (true)
	{
		// Figure out how many bytes were written to this buffer and how many source frames this corresponds to
		AKASSERT(pInstance->BufferSize > pInstance->BufferMeta[pInstance->WriteHead].uValidSize);
		size_t uSpaceLeft = pInstance->BufferSize - pInstance->BufferMeta[pInstance->WriteHead].uValidSize;
		size_t uSizeAdded = AkMin(uNumBytesProduced, uSpaceLeft);
		AkUInt16 uFramesAdded = uSizeAdded * fFramesPerBytes;

		pInstance->BufferMeta[pInstance->WriteHead].uValidSize += uSizeAdded;
		pInstance->BufferMeta[pInstance->WriteHead].uSrcFrames += uFramesAdded;

		uNumBytesProduced -= uSizeAdded;
		uNumFramesProduced -= uFramesAdded;
		pInstance->RangeProgress += uFramesAdded;

		if (uNumBytesProduced == 0)
		{
			// This is the last buffer to process for this batch; record the remaining source frames caused by fp rounding error in this one
			pInstance->BufferMeta[pInstance->WriteHead].uSrcFrames += uNumFramesProduced;
			pInstance->RangeProgress += uNumFramesProduced;
			uNumFramesProduced = 0;

			if (uSpaceLeft == uSizeAdded || (bLastRange && bAtEnd && uSamplesInResampler == 0))
			{
				// The very last buffer in the stream will be marked as ready even if incomplete
				pInstance->uReadCount++;
				pInstance->uWriteCount--;
				pInstance->WriteHead = (pInstance->WriteHead + 1) % AK_AJM_NUM_BUFFERS;
			}

			break;
		}

		AKASSERT(uSpaceLeft == uSizeAdded);

		// Move on to the next - it must be empty!
		pInstance->uReadCount++;
		pInstance->uWriteCount--;
		pInstance->WriteHead = (pInstance->WriteHead + 1) % AK_AJM_NUM_BUFFERS;
		AKASSERT(pInstance->BufferMeta[pInstance->WriteHead].uValidSize == 0);
		AKASSERT(pInstance->BufferMeta[pInstance->WriteHead].uSrcFrames == 0);
	}
	AKASSERT(pInstance->uReadCount <= AK_AJM_NUM_BUFFERS);
	AKASSERT(pInstance->uWriteCount <= AK_AJM_NUM_BUFFERS);
	AKASSERT(uNumFramesProduced == 0);
	AKASSERT(uNumBytesProduced == 0);

	if (bAtEnd)
	{
		AKASSERT(pInstance->RangeProgress + uSamplesInResampler == pInstance->RangeLength);
		pInstance->State = AK::Ajm::Instance::InstanceState::IdleEndOfRange;
	}
	else
	{
		AKASSERT(pInstance->RangeProgress + uSamplesInResampler < pInstance->RangeLength);
		pInstance->State = AK::Ajm::Instance::InstanceState::IdleInRange;
	}
	return AK_Success;
}

void AK::Ajm::GetBuffer(Instance * pInstance, AkAudioBuffer & out_buffer, AkUInt16 & out_uSrcFrames)
{
	AKASSERT(pInstance->uReadCount > 0);

	auto record = pInstance->BufferMeta[pInstance->ReadHead];
	AkUInt16 uFrameSize = AK_AJM_OUTPUT_SAMPLE_SIZE * out_buffer.NumChannels();
	AKASSERT(record.uValidSize % uFrameSize == 0);
	AkUInt32 uValidFrames = record.uValidSize / uFrameSize;
	void * pOutputBuffer = pInstance->pBuffer + (pInstance->ReadHead * pInstance->BufferSize);

	out_buffer.AttachInterleavedData(pOutputBuffer, uValidFrames, uValidFrames);
	out_uSrcFrames = record.uSrcFrames;

	pInstance->BufferMeta[pInstance->ReadHead].uSrcFrames = 0;
	pInstance->BufferMeta[pInstance->ReadHead].uValidSize = 0;

	pInstance->ReadHead = (pInstance->ReadHead + 1) % AK_AJM_NUM_BUFFERS;
	pInstance->uReadCount--;
}

void AK::Ajm::Seek(Instance * pInstance)
{
	pInstance->BatchStatus = Batch::Status::Invalid;
	pInstance->JobFlags = 0;
	ClearBuffer(pInstance);
}

void AK::Ajm::AddInitializeAT9Job(Instance * pInstance)
{
	pInstance->JobFlags |= AK_AJM_JOB_INITIALIZE_AT9;
}

void AK::Ajm::AddInitializeOpusJob(Instance* pInstance)
{
	pInstance->JobFlags |= AK_AJM_JOB_INITIALIZE_OPUS;
}

void AK::Ajm::AddSetGaplessDecodeJob(Instance * pInstance, AkUInt32 skip, AkUInt32 total)
{
	pInstance->JobFlags |= AK_AJM_JOB_SET_GAPLESS_DECODE;
	pInstance->GaplessDecode.Params.uiSkipSamples = skip;
	pInstance->GaplessDecode.Params.uiTotalSamples = total;
	pInstance->GaplessDecode.reset = pInstance->State == AK::Ajm::Instance::InstanceState::IdleStartOfRange ? 1 : 0;
}

void AK::Ajm::AddSetResampleParameters(Instance * pInstance, float ratioStart, float ratioStep, uint32_t flags)
{
	pInstance->JobFlags |= AK_AJM_JOB_SET_RESAMPLE_PARAMETERS;
	pInstance->Resampler.RatioStart = ratioStart;
	pInstance->Resampler.RatioStep = ratioStep;
	pInstance->Resampler.Flags = flags;
}

void AK::Ajm::AddDecodeJob(Instance * pInstance, SceAjmBuffer input1, SceAjmBuffer input2)
{
	// Setup input
	pInstance->InputDescriptors[0] = input1;
	pInstance->InputDescriptors[1] = input2;

	// Setup output
	size_t refillSize = SpaceAvailable(pInstance);
	size_t wholeBufferSize = pInstance->BufferSize * AK_AJM_NUM_BUFFERS;
	size_t uWriteOffset = pInstance->WriteHead * pInstance->BufferSize + pInstance->BufferMeta[pInstance->WriteHead].uValidSize;
	size_t spaceAtEnd = wholeBufferSize - uWriteOffset;
	size_t firstBufferSize = AkMin(refillSize, spaceAtEnd);
	size_t remainder = refillSize - firstBufferSize;
	AKASSERT(firstBufferSize + remainder <= wholeBufferSize);
	pInstance->OutputDescriptors[0].pAddress = pInstance->pBuffer + uWriteOffset;
	pInstance->OutputDescriptors[0].szSize = firstBufferSize;
	pInstance->OutputDescriptors[1].pAddress = pInstance->pBuffer;
	pInstance->OutputDescriptors[1].szSize = remainder;

	// AJM buffer overruns are EXTREMELY difficult to debug because they can occur any time an active batch is being processed!
	// Never remove these asserts!
	AKASSERT(pInstance->OutputDescriptors[0].pAddress >= pInstance->pBuffer);
	AKASSERT(pInstance->OutputDescriptors[1].pAddress >= pInstance->pBuffer);
	AKASSERT((AkUInt8*)pInstance->OutputDescriptors[0].pAddress + pInstance->OutputDescriptors[0].szSize <= (AkUInt8*)pInstance->pBuffer + pInstance->BufferSize * AK_AJM_NUM_BUFFERS);
	AKASSERT((AkUInt8*)pInstance->OutputDescriptors[1].pAddress + pInstance->OutputDescriptors[1].szSize <= (AkUInt8*)pInstance->pBuffer + pInstance->BufferSize * AK_AJM_NUM_BUFFERS);

	// Enable flag
	pInstance->JobFlags |= AK_AJM_JOB_DECODE_SPLIT;
}

void AK::Ajm::AddGetResampleInfo(Instance * pInstance)
{
	pInstance->JobFlags |= AK_AJM_JOB_GET_RESAMPLE_INFO;
}
