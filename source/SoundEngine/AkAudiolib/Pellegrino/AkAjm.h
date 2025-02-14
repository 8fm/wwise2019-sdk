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
#include <ajm/opus_decoder.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>

//#define AK_AJM_DEBUG 1
//#define AK_AJM_DEBUG_HASH 1

#if defined(AK_AJM_DEBUG)
#define AK_AJM_DEBUG_PRINTF(...) printf(__VA_ARGS__)
#if defined(AK_AJM_DEBUG_HASH)
#include <AK/Tools/Common/AkFNVHash.h>
static AK::Hash64::HashType HashBuffer(void * ptr, size_t sz)
{
	if (sz == 0) return 0;
	AK::FNVHash64 hasher;
	return hasher.Compute(ptr, sz);
}
#endif
#else
#define AK_AJM_DEBUG_PRINTF(...)
#endif

#define AK_AJM_JOB_INITIALIZE_AT9          (1 << 0)
#define AK_AJM_JOB_SET_GAPLESS_DECODE      (1 << 1)
#define AK_AJM_JOB_SET_RESAMPLE_PARAMETERS (1 << 2)
#define AK_AJM_JOB_DECODE_SPLIT            (1 << 3)
#define AK_AJM_JOB_GET_RESAMPLE_INFO       (1 << 4)
#define AK_AJM_JOB_INITIALIZE_OPUS         (1 << 5)
#define AK_AJM_JOBS_PENDING                (1 << 15)

#define AK_AJM_OUTPUT_SAMPLE_SIZE       (sizeof(AkUInt16))

// How many buffers will be decoded in advance
// This increases latency when reacting to pitch change but is necessary due to AJM overhead
// Firing off an AJM batch is an expensive operation. It cannot be performed every audio loop iteration.
#define AK_AJM_NUM_BUFFERS 6

// AJM cannot decode loopend + loopstart in a single decode operation.
// Thus two buffers are required to avoid source starvation when looping
#define AK_AJM_MIN_BUFFERS 2

// Keeping buffers aligned to this value avoids copying them in/out of AJM memory
#define AK_AJM_BUFFER_ALIGNMENT 64

// The AJM resampler has an internal buffer that can keep this many audio frames in memory
#define AK_AJM_RESAMPLER_EXTRA_FRAMES 256

namespace AK
{
	AKRESULT SceStatusToAkResult(int status);

namespace Ajm
{
	struct Batch
	{
		enum class Status
		{
			Invalid,
			InProgress,
			Success,
			Failure,
			Cancelled
		};
		SceAjmBatchId id;
		Status status;
		SceAjmBatchError error;
	};

	struct Instance
	{
		enum class InstanceState {
			Uninitialized,
			IdleStartOfRange,
			IdleInRange,
			IdleEndOfRange,
		};

		AkUInt32 JobFlags;
		SceAjmInstanceId InstanceId;
		Batch::Status BatchStatus;
		union {
			SceAjmDecAt9InitializeParameters AT9InitParams;
			SceAjmDecOpusInitializeParameters OpusInitParams;
		};
		SceAjmInitializeResult InitResult;
		struct {
			SceAjmSidebandGaplessDecode Params;
			int reset;
			SceAjmSetGaplessDecodeResult Result;
		} GaplessDecode;
		struct {
			float RatioStart;
			float RatioStep;
			uint32_t Flags;
			SceAjmSidebandResult Result;
			SceAjmSidebandResampleResult Info;
		} Resampler;
		SceAjmBuffer InputDescriptors[2];
		SceAjmBuffer OutputDescriptors[2];
		SceAjmDecodeResult DecodeResult;

		// Range information
		AkUInt32 RangeStart;
		AkInt32  RangeProgress;
		AkUInt32 RangeLength;

		InstanceState State;

		// Simple ring buffer to hold output samples
		AkUInt8 * pBuffer;
		AkUInt32 BufferSize;
		AkUInt32 ReadHead;
		AkUInt32 WriteHead;

		struct MetaRecord {
			size_t   uValidSize;   // Number of valid bytes in buffer
			AkUInt16 uSrcFrames; // Number of source media frames contained in this buffer (they may be resampled)
		};
		MetaRecord BufferMeta[AK_AJM_NUM_BUFFERS]; // Information about each decoded audio buffer
		AkUInt8 uWriteCount; // How many buffers are ready to be written from the WriteHead
		AkUInt8 uReadCount;  // How many buffers are ready to be read from the ReadHead
	};
	
	AKRESULT CreateInstance(SceAjmContextId ctx, SceAjmCodecType type, uint32_t flags, AkUInt32 bufferSize, Instance ** out_pInstance);
	void DestroyInstance(SceAjmContextId ctx, Instance * pInstance);
	void SetRange(Instance * pInstance, AkUInt32 start, AkUInt32 length, AkUInt32 remainder);
	void ClearBuffer(Instance * pInstance);
	AKRESULT RecordJobResults(Instance * pInstance, bool bLastRange);

	inline AkUInt32 SpaceAvailable(Instance * pInstance)
	{
		if (0 == pInstance->uWriteCount) return 0;
		// It's possible for the current buffer to have been partly written to in the last batch
		// This is most likely to happen when we are looping back
		AkUInt32 uCurrentBufferOffset = pInstance->BufferMeta[pInstance->WriteHead].uValidSize;
		return pInstance->BufferSize * pInstance->uWriteCount - uCurrentBufferOffset;
	}

	inline bool HasBuffer(Instance * pInstance)
	{
		return pInstance->uReadCount > 0;
	}

	void GetBuffer(Instance * pInstance, AkAudioBuffer &out_buffer, AkUInt16 &out_uSrcFrames);

	inline void ReleaseBuffer(Instance * pInstance)
	{
		if (pInstance->uWriteCount < AK_AJM_NUM_BUFFERS)
			pInstance->uWriteCount++;
	}

	void Seek(Instance * pInstance);
	void AddInitializeAT9Job(Instance * pInstance);
	void AddInitializeOpusJob(Instance* pInstance);
	void AddSetGaplessDecodeJob(Instance * pInstance, AkUInt32 skip, AkUInt32 total);
	void AddSetResampleParameters(Instance * pInstance, float ratioStart, float ratioStep, uint32_t flags);
	void AddDecodeJob(Instance * pInstance, SceAjmBuffer input1, SceAjmBuffer input2);
	void AddGetResampleInfo(Instance * pInstance);
}
}

class CAkAjmBatchBuilder
{
public:
	CAkAjmBatchBuilder();
	~CAkAjmBatchBuilder();

	AKRESULT Init(size_t batchSize);
	void Term();

	void AddJobs(AK::Ajm::Instance * pInstance);

	AKRESULT StartBatch(SceAjmContextId contextId, const unsigned int priority, AK::Ajm::Batch &out_batch);
	AKRESULT Reset(size_t newBatchSize);

private:
	AKRESULT GrowBuffer(size_t newBatchSize);

	SceAjmBatchInfo m_batchInfo;
	void * m_batchBuffer;
	size_t m_batchSize;
	bool m_batchCreated;
};

