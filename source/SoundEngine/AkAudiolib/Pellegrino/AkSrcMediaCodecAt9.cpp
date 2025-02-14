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
#include "AkSrcMediaCodecAt9.h"
#include "AkATRAC9.h"
#include "AkAjmScheduler.h"

extern AkPlatformInitSettings g_PDSettings;

#define AK_AJM_INSTANCE_FLAGS(__nchan__) (SCE_AJM_INSTANCE_FLAG_MAX_CHANNEL(__nchan__) | SCE_AJM_INSTANCE_FLAG_FORMAT(SCE_AJM_FORMAT_ENCODING_S16)) | SCE_AJM_INSTANCE_FLAG_RESAMPLE

IAkSrcMediaCodec* CreateATRAC9Codec(const AK::SrcMedia::Header * in_pHeader )
{
	return AkNew(AkMemID_Processing, CAkSrcMediaCodecAt9());
}

IAkSoftwareCodec* CreateATRAC9SrcMedia(void* in_pCtx)
{
	return AkNew(AkMemID_Processing, CAkSrcMedia((CAkPBI*)in_pCtx, CreateATRAC9Codec));
}

using namespace AK::SrcMedia::Stream;

CAkSrcMediaCodecAt9::CAkSrcMediaCodecAt9()
	: m_pAjmInstance(nullptr)
	, m_pPreviousInputBuffer(nullptr)
	, m_PreviousInputBufferSize(0)
	, m_uLoopCnt(0)
	, m_bStopLooping(false)
{
}

IAkSrcMediaCodec::Result CAkSrcMediaCodecAt9::Init(const AK::SrcMedia::Header & in_header, AK::SrcMedia::CodecInfo & out_codec, AkUInt16 uLoopCnt)
{
	if (in_header.FormatInfo.pFormat->wFormatTag != AK_WAVE_FORMAT_AT9)
	{
		return AK_FileFormatMismatch;
	}

	AKASSERT(in_header.FormatInfo.pFormat);
	AKASSERT(in_header.FormatInfo.uFormatSize == sizeof(At9WaveFormatExtensible));
	AKASSERT(in_header.FormatInfo.pFormat->cbSize == (sizeof(At9WaveFormatExtensible) - sizeof(WaveFormatEx)));
	At9WaveFormatExtensible * pFmt = static_cast<At9WaveFormatExtensible*>(in_header.FormatInfo.pFormat);

	//Setup format on the PBI
	out_codec.format.SetAll(
		AK_CORE_SAMPLERATE,
		pFmt->GetChannelConfig(),
		AK_AJM_OUTPUT_SAMPLE_SIZE * 8,
		AK_AJM_OUTPUT_SAMPLE_SIZE * pFmt->nChannels,
		AK_INT,
		AK_INTERLEAVED);

	out_codec.uSourceSampleRate = pFmt->nSamplesPerSec;
	out_codec.uTotalSamples = pFmt->at9TotalSamplesPerChannel;
	out_codec.uAttributes = AK_CODEC_ATTR_HARDWARE | AK_CODEC_ATTR_PITCH;
	if (g_PDSettings.bHwCodecLowLatencyMode)
		out_codec.uAttributes |= AK_CODEC_ATTR_HARDWARE_LL;

	// Init state.
	m_uNumChannels = pFmt->GetChannelConfig().uNumChannels;
	m_uSrcSampleRate = pFmt->nSamplesPerSec;
	m_uDataOffset = in_header.uDataOffset;
	m_wSamplesPerBlock = pFmt->wSamplesPerBlock;
	m_nBlockAlign = pFmt->nBlockAlign;
	m_uAt9EncoderDelaySamples = pFmt->at9DelaySamplesInputOverlapEncoder;
	m_uLoopCnt = uLoopCnt;
	memcpy(m_At9ConfigData, pFmt->at9ConfigData, sizeof(m_At9ConfigData));

	AkUInt32 uPCMLoopEnd = in_header.uPCMLoopEnd == 0 ? pFmt->at9TotalSamplesPerChannel : in_header.uPCMLoopEnd;

	out_codec.heuristics.uLoopStart = in_header.uDataOffset + AkMin(in_header.uDataSize, (AkUInt32)(in_header.uPCMLoopStart / m_wSamplesPerBlock) * m_nBlockAlign);
	out_codec.heuristics.uLoopEnd = in_header.uDataOffset + AkMin(in_header.uDataSize, (AkUInt32)(((uPCMLoopEnd + m_uAt9EncoderDelaySamples) / m_wSamplesPerBlock) + 1) * m_nBlockAlign);

	AK_AJM_DEBUG_PRINTF("[AJM %p] Header Parsed; TotalFrames %d; PCM Loops [%d, %d]; File Loops [%d, %d]\n", this, out_codec.uTotalSamples, in_header.uPCMLoopStart, uPCMLoopEnd, out_codec.heuristics.uLoopStart, out_codec.heuristics.uLoopEnd);

	// Update stream buffering settings.
	out_codec.heuristics.fThroughput = pFmt->nAvgBytesPerSec / 1000.f;	// Throughput (bytes/ms)
	out_codec.uMinimalBufferSize = 1;

	AK::SrcMedia::ResamplingRamp::Init(&m_Ramp, m_uSrcSampleRate);
	return CreateDecoder();
}

void CAkSrcMediaCodecAt9::Term()
{
	DestroyDecoder();
}

IAkSrcMediaCodec::Result CAkSrcMediaCodecAt9::PrepareNextBuffer(AK::SrcMedia::Stream::State * pStream, const AK::SrcMedia::Position::State &position, const PitchInfo &pitch)
{
	AK_INSTRUMENT_SCOPE("CAkSrcMediaCodecAt9::PrepareNextBuffer");

	// Nothing to do if the current batch is still in flight.
	if (m_pAjmInstance->BatchStatus == AK::Ajm::Batch::Status::InProgress)
		return AK_Success;

	IAkSrcMediaCodec::Result eResult = ProcessDecodeBatch(pStream);
	if (eResult == AK_Fail)
		return eResult;

	AK::SrcMedia::ResamplingRamp::SetRampTargetPitch(&m_Ramp, pitch.fPitch);
	if (pitch.eType == PitchShiftType_LinearConstant)
		m_Ramp.fLastRatio = m_Ramp.fTargetRatio;

	AkUInt32 uResamplerFrames = m_pAjmInstance->Resampler.Info.sResampleInfo.iNumSamples > 0 ? m_pAjmInstance->Resampler.Info.sResampleInfo.iNumSamples : 0;

	// We will not fetch a buffer that starts a new loop range unless we're done decoding the current range
	bool bAllowStmLoop = false;

	// Are we done decoding the current range?
	if (m_pAjmInstance->State == AK::Ajm::Instance::InstanceState::Uninitialized)
	{
		// Since we're just getting started, don't ramp up to the pitch; start there immediately
		m_Ramp.fLastRatio = m_Ramp.fTargetRatio;
		AK::Ajm::SetRange(m_pAjmInstance, m_pAjmInstance->RangeStart, (m_uLoopCnt != LOOPING_ONE_SHOT ? position.uPCMLoopEnd + 1 : position.uTotalSamples) - position.uCurSample, 0);
	}
	else if (m_pAjmInstance->State == AK::Ajm::Instance::InstanceState::IdleEndOfRange)
	{
		AK_AJM_DEBUG_PRINTF("[AJM %p] Completed range [%d, %d].\n", this, m_pAjmInstance->RangeStart, m_pAjmInstance->RangeLength);
		if (m_uLoopCnt != LOOPING_ONE_SHOT)
		{
			AKASSERT(m_PreviousInputBufferSize == 0 && pStream->uSizeLeft == 0); // At the end of a range, all previous input should have been consumed.
			if (m_uLoopCnt > 1) --m_uLoopCnt;
			AK::Ajm::SetRange(m_pAjmInstance, position.uPCMLoopStart, (m_uLoopCnt != LOOPING_ONE_SHOT ? position.uPCMLoopEnd + 1 : position.uTotalSamples) - position.uPCMLoopStart, uResamplerFrames);
			AK_AJM_DEBUG_PRINTF("[AJM %p] Looping back to decode range [%d, %d].\n", this, m_pAjmInstance->RangeStart, m_pAjmInstance->RangeLength);
			bAllowStmLoop = true;
		}
		else if (m_pAjmInstance->RangeProgress == m_pAjmInstance->RangeLength)
		{
			AK_AJM_DEBUG_PRINTF("[AJM %p] is done.\n", this);
			// Signal NoMoreData so that the next source in the VPL can start decoding in advance
			return AK_NoMoreData;
		}
	}

	AkUInt32 uOutputFramesLeft = AK::SrcMedia::ResamplingRamp::MaxOutputFrames(&m_Ramp, m_pAjmInstance->RangeLength - m_pAjmInstance->RangeProgress);

	// Determine how many samples we want to decode on next batch
	AkUInt32 uFramesToRefill = AK::Ajm::SpaceAvailable(m_pAjmInstance) / FrameSize();
	if (uFramesToRefill == 0)
	{
		AK_AJM_DEBUG_PRINTF("[AJM %p] Nothing to decode (no space in output buffer).\n", this);
		return AK_Success; // Buffer is already full, nothing to decode
	}

	bool bWillStartNewRange = m_pAjmInstance->State == AK::Ajm::Instance::InstanceState::Uninitialized || m_pAjmInstance->State == AK::Ajm::Instance::InstanceState::IdleStartOfRange;

	// Determine size of input required for this batch
	AkUInt32 uInputSize = 0;
	AkUInt32 uSrcFramesToDecode = AK::SrcMedia::ResamplingRamp::MaxSrcFrames(&m_Ramp, uFramesToRefill);

	// Always feed more samples to allow resampler to fill its internal buffer
	if (uResamplerFrames < AK_AJM_RESAMPLER_EXTRA_FRAMES)
		uSrcFramesToDecode += AK_AJM_RESAMPLER_EXTRA_FRAMES - uResamplerFrames;

	// Skip the encoder delay frames and the beginning part of the block before our range start point
	if (bWillStartNewRange)
		uSrcFramesToDecode += FramesToSkip();

	// Read from input stream to gather as many bytes as possible to send to AJM
	AKRESULT eStmResult = GatherStreamBuffers(pStream, uSrcFramesToDecode, bAllowStmLoop, uInputSize);
	bool bIsLastInputBuffer = eStmResult == AK_NoMoreData;
	if (eStmResult == AK_Fail || (uInputSize == 0 && !bIsLastInputBuffer))
	{
		AK_AJM_DEBUG_PRINTF("[AJM %p] Nothing to decode. Stream result: %d; Input size: %d.\n", this, eStmResult, uInputSize);
		return eStmResult;
	}
	
	// Make certain we never send more data than what AJM can handle
	uInputSize = AkMin(uInputSize, SCE_AJM_DEC_AT9_MAX_INPUT_BUFFER_SIZE);

	// Add this instance's jobs to the next batch
	m_pAjmInstance->JobFlags = 0;
	if (m_pAjmInstance->State == AK::Ajm::Instance::InstanceState::Uninitialized)
	{
		AK::Ajm::AddInitializeAT9Job(m_pAjmInstance);
		AK_AJM_DEBUG_PRINTF("[AJM %p] Initialize(%x)\n", this, *(AkUInt32*)m_pAjmInstance->AT9InitParams.uiConfigData);
	}
	if (bWillStartNewRange || m_bStopLooping)
	{
		AK::Ajm::AddSetGaplessDecodeJob(m_pAjmInstance, FramesToSkip(), m_pAjmInstance->RangeLength);
		AK_AJM_DEBUG_PRINTF("[AJM %p] SetGaplessDecode(%d, %d, %d)\n", this, m_pAjmInstance->GaplessDecode.Params.uiSkipSamples, m_pAjmInstance->GaplessDecode.Params.uiTotalSamples, m_pAjmInstance->GaplessDecode.reset);
	}
	{
		// The AJM resample keeps some decoded frames internally. To maximize quality, we only flush them out when the decoder is on its very last decode batch
		uint32_t flags = 0;
		if (bIsLastInputBuffer) flags |= SCE_AJM_RESAMPLE_FLAG_END;
		// Reset must occur on the first decode batch after a seek
		if ((m_pAjmInstance->Resampler.Flags & SCE_AJM_RESAMPLE_FLAG_RESET) != 0) flags |= SCE_AJM_RESAMPLE_FLAG_RESET;

		AkUInt32 uExpectedRampLength = AkMin(uFramesToRefill, uOutputFramesLeft);

		AkReal32 rampStart = AK::SrcMedia::ResamplingRamp::RampStart(&m_Ramp);
		AkReal32 rampStep = AK::SrcMedia::ResamplingRamp::RampStep(&m_Ramp, uExpectedRampLength);
		AK::Ajm::AddSetResampleParameters(m_pAjmInstance, rampStart, rampStep, flags);
		AK::Ajm::AddGetResampleInfo(m_pAjmInstance);
		AK_AJM_DEBUG_PRINTF("[AJM %p] SetResampleParameters(%f, %f, %u)\n", this, m_pAjmInstance->Resampler.RatioStart, m_pAjmInstance->Resampler.RatioStep, m_pAjmInstance->Resampler.Flags);
	}
	if (m_PreviousInputBufferSize > 0)
	{
		AkUInt32 fstBufferSize = AkMin(m_PreviousInputBufferSize, uInputSize);
		AkUInt32 sndBufferSize = AkMin(pStream->uSizeLeft, uInputSize - fstBufferSize);
		AK::Ajm::AddDecodeJob(m_pAjmInstance, { m_pPreviousInputBuffer, fstBufferSize }, { pStream->pNextAddress, sndBufferSize });
		AK_AJM_DEBUG_PRINTF("[AJM %p] Decoding [%lu,%zu] + [%u,%zu] onto [%u,%zu]\n",
			this,
			pStream->uCurFileOffset - m_pAjmInstance->InputDescriptors[0].szSize, m_pAjmInstance->InputDescriptors[0].szSize,
			pStream->uCurFileOffset, m_pAjmInstance->InputDescriptors[1].szSize,
			m_pAjmInstance->WriteHead, m_pAjmInstance->OutputDescriptors[0].szSize
		);
	}
	else
	{
		AkUInt32 fstBufferSize = AkMin(pStream->uSizeLeft, uInputSize);
 		AK::Ajm::AddDecodeJob(m_pAjmInstance, { pStream->pNextAddress, fstBufferSize }, { nullptr, 0 });
		AK_AJM_DEBUG_PRINTF("[AJM %p] Decoding [%u,%zu] onto [%u,%zu]\n",
			this,
			pStream->uCurFileOffset, m_pAjmInstance->InputDescriptors[0].szSize,
			m_pAjmInstance->WriteHead, m_pAjmInstance->OutputDescriptors[0].szSize
		);
	}
	AKASSERT(m_pAjmInstance->OutputDescriptors[0].szSize > 0);
	return bIsLastInputBuffer ? AK_NoMoreData : AK_Success;
}

IAkSrcMediaCodec::Result CAkSrcMediaCodecAt9::GetBuffer(AK::SrcMedia::Stream::State * pStream, AkUInt16 uMaxFrames, BufferInfo & out_buffer)
{
	AK_INSTRUMENT_SCOPE("CAkSrcMediaCodecAt9::GetBuffer");

	IAkSrcMediaCodec::Result eResult;

	// Make sure the batch is not still in progress at this point
	CAkGlobalAjmScheduler::Get().WaitForJobs();

	// Then check the result
	eResult = ProcessDecodeBatch(pStream);
	if (eResult == AK_Fail)
		return eResult;

	// Copy decoded frames, if any
	if (AK::Ajm::HasBuffer(m_pAjmInstance))
	{
		AK::Ajm::GetBuffer(m_pAjmInstance, out_buffer.Buffer, out_buffer.uSrcFrames);

		AK_AJM_DEBUG_PRINTF("[AJM %p] Transfered [%u,%u] to pipeline. %d buffers are ready.\n", this, out_buffer.uSrcFrames, out_buffer.Buffer.uValidFrames, m_pAjmInstance->uReadCount);
		eResult = AK_DataReady;
	}
	else
	{
		AK_AJM_DEBUG_PRINTF("[AJM %p] No buffer to transfer!\n", this);
		eResult = AK_NoDataReady;
	}

	return eResult;
}

void CAkSrcMediaCodecAt9::ReleaseBuffer(AK::SrcMedia::Stream::State * pStream)
{
	if (m_pAjmInstance)
	{
		AK::Ajm::ReleaseBuffer(m_pAjmInstance);
		AK_AJM_DEBUG_PRINTF("[AJM %p] Released buffer. %d buffers are free.\n", this, m_pAjmInstance->uWriteCount);
	}
}

void CAkSrcMediaCodecAt9::VirtualOn()
{
	DestroyDecoder();
}

IAkSrcMediaCodec::Result CAkSrcMediaCodecAt9::VirtualOff()
{
	return CreateDecoder();
}

IAkSrcMediaCodec::Result CAkSrcMediaCodecAt9::FindClosestFileOffset(AkUInt32 in_uDesiredSample, SeekInfo & out_SeekInfo)
{
	AkUInt32 blockIndex = in_uDesiredSample / m_wSamplesPerBlock;
	out_SeekInfo.uFileOffset = blockIndex * m_nBlockAlign;
	out_SeekInfo.uPCMOffset = in_uDesiredSample; // SetGaplessDecode makes up the difference. We do NOT want the rest of the code to know about this.
	out_SeekInfo.uSkipLength = 0; // Will be computed live when we prepare the next batch
	return AK_Success;
}

IAkSrcMediaCodec::Result CAkSrcMediaCodecAt9::Seek(AK::SrcMedia::Stream::State* pStream, const SeekInfo & seek, AkUInt32 uNumFrames, AkUInt16 uLoopCnt)
{
	AKASSERT(m_pAjmInstance != nullptr);
	if (m_PreviousInputBufferSize > 0)
	{
		m_PreviousInputBufferSize = 0;
		m_pPreviousInputBuffer = nullptr;
	}
	AK::Ajm::Seek(m_pAjmInstance);
	AK::Ajm::SetRange(m_pAjmInstance, seek.uPCMOffset, uNumFrames, 0);
	m_pAjmInstance->Resampler.Flags = SCE_AJM_RESAMPLE_FLAG_RESET;
	m_uLoopCnt = uLoopCnt;
	AK_AJM_DEBUG_PRINTF("[AJM %p] Seeking to PCM sample %d; file offset %d; length %d\n", this, seek.uPCMOffset, seek.uFileOffset, uNumFrames);
	return AK_Success;
}

void CAkSrcMediaCodecAt9::StopLooping(const AK::SrcMedia::Position::State &position)
{
	m_uLoopCnt = position.uLoopCnt;
	if (m_uLoopCnt == LOOPING_ONE_SHOT)
	{
		m_pAjmInstance->RangeLength = position.uTotalSamples - position.uPCMLoopStart;
		m_bStopLooping = true;
		AK_AJM_DEBUG_PRINTF("[AJM %p] will stop looping; decode range extended to [%d, %d].\n", this, m_pAjmInstance->RangeStart, m_pAjmInstance->RangeLength);
	}
}

IAkSrcMediaCodec::Result CAkSrcMediaCodecAt9::CreateDecoder()
{
	AKRESULT eResult = CAkGlobalAjmScheduler::Get().CreateInstance(
		SCE_AJM_CODEC_DEC_AT9,
		AK_AJM_INSTANCE_FLAGS(m_uNumChannels),
		BufferSize(),
		&m_pAjmInstance);
	if (eResult == AK_MaxReached)
	{
		return Result(AK_Fail, AK::Monitor::ErrorCode_HwVoiceLimitReached);
	}
	else if (eResult == AK_DeviceNotReady)
	{
		return Result(AK_Fail, AK::Monitor::ErrorCode_HwVoiceInitFailed);
	}
	else if (eResult != AK_Success)
	{
		return eResult;
	}
	AK_AJM_DEBUG_PRINTF("[AJM %p] Created instance #%d\n", this, m_pAjmInstance->InstanceId);

	memcpy(m_pAjmInstance->AT9InitParams.uiConfigData, m_At9ConfigData, sizeof(m_pAjmInstance->AT9InitParams.uiConfigData));
	m_pPreviousInputBuffer = nullptr;
	m_PreviousInputBufferSize = 0;

	return AK_Success;
}

void CAkSrcMediaCodecAt9::DestroyDecoder()
{
	if (m_pAjmInstance)
	{
		AK_AJM_DEBUG_PRINTF("[AJM %p] Marked instance #%d for destruction\n", this, m_pAjmInstance->InstanceId);
		CAkGlobalAjmScheduler::Get().DestroyInstance(m_pAjmInstance);
		m_pAjmInstance = nullptr;
	}
}

IAkSrcMediaCodec::Result CAkSrcMediaCodecAt9::ProcessDecodeBatch(AK::SrcMedia::Stream::State * pStream)
{
	AK_INSTRUMENT_SCOPE("CAkSrcMediaCodecAt9::ProcessDecodeBatch");

	AK::Ajm::Batch::Status eStatus = m_pAjmInstance->BatchStatus;

	AKASSERT(eStatus != AK::Ajm::Batch::Status::InProgress);

	// React to overall batch status here
	switch (eStatus)
	{
	case AK::Ajm::Batch::Status::Invalid:
		return AK_NoDataReady;

	case AK::Ajm::Batch::Status::Failure:
		// Keep the batch status to ensure the voice is killed
		return Result(AK_Fail, AK::Monitor::ErrorCode_ATRAC9DecodeFailed);

	case AK::Ajm::Batch::Status::Cancelled:
		// Batch status can be discarded
		m_pAjmInstance->BatchStatus = AK::Ajm::Batch::Status::Invalid;
		return AK_NoDataReady;

	case AK::Ajm::Batch::Status::Success:
		// Make sure to only process the results once
		m_pAjmInstance->BatchStatus = AK::Ajm::Batch::Status::Invalid;
		// Continue to process
		break;

	default:
		AKASSERT(!"Unknown batch status, please define policy for this new status here.");
	}

#if defined(AK_AJM_DEBUG_HASH)
	auto out = m_pAjmInstance->OutputDescriptors;
	auto s1 = AkMin(m_pAjmInstance->DecodeResult.sStream.iSizeProduced, out[0].szSize);
	auto s2 = m_pAjmInstance->DecodeResult.sStream.iSizeProduced - s1;
	AK_AJM_DEBUG_PRINTF(
		"[AJM %p] Output buffers after decode: [%u,%zu,%llu] + [%u,%zu,%llu]\n", this,
			m_pAjmInstance->WriteHead, s1, HashBuffer(out[0].pAddress, s1),
			0, s2, HashBuffer(out[1].pAddress, s2)
		);
#endif

	bool bLastRange = m_uLoopCnt == LOOPING_ONE_SHOT;

	// Are the jobs in the batch successful?
	AKRESULT eResult = AK::Ajm::RecordJobResults(m_pAjmInstance, bLastRange);
	if (eResult != AK_Success)
		return Result(AK_Fail, AK::Monitor::ErrorCode_ATRAC9DecodeFailed);

	m_bStopLooping = false;

	AK_AJM_DEBUG_PRINTF("[AJM %p] Produced %d bytes and advanced input by %d bytes. %d buffers are ready.\n", this, m_pAjmInstance->DecodeResult.sStream.iSizeProduced, m_pAjmInstance->DecodeResult.sStream.iSizeConsumed, m_pAjmInstance->uReadCount);
	AKASSERT(m_pAjmInstance->DecodeResult.sStream.iSizeProduced % FrameSize() == 0);

	// Remove the resampler reset flag and record the new last ratio
	m_pAjmInstance->Resampler.Flags &= ~SCE_AJM_RESAMPLE_FLAG_RESET;
	m_Ramp.fLastRatio = m_Ramp.fTargetRatio;

	// Record number of input bytes consumed in the stream source
	int32_t sc = m_pAjmInstance->DecodeResult.sStream.iSizeConsumed;
	if (m_PreviousInputBufferSize > 0)
	{
		AkUInt32 psc = AkMin(m_PreviousInputBufferSize, sc);
		sc -= psc;
		m_PreviousInputBufferSize -= psc;
		m_pPreviousInputBuffer += psc;
		if (m_PreviousInputBufferSize == 0)
		{
			m_pPreviousInputBuffer = nullptr;
			ReleaseStreamBuffer(pStream);
		}
	}
	ConsumeData(pStream, sc);
	if (sc > 0 && pStream->uSizeLeft == 0)
	{
		ReleaseStreamBuffer(pStream);
	}

	return AK_Success;
}

AKRESULT CAkSrcMediaCodecAt9::GatherStreamBuffers(AK::SrcMedia::Stream::State * pStream, AkUInt32 uSrcFramesToDecode, bool bAllowLoop, AkUInt32 &out_uInputSize)
{
	// Up to two input buffers can be gathered (Previous and Current)
	// AJM allows split input buffers, so there is no need to stitch them

	AkUInt32 uAvailableInput = m_PreviousInputBufferSize + pStream->uSizeLeft;

	// WG-49802 AJM consumes input in increments not aligned on ATRAC9 block size.
	// First gather the remainder of the block we're currently in.
	// We count this block as containing zero samples (not true, but otherwise impossible to predict)
	AkUInt32 uNextBlockSize = m_nBlockAlign - (pStream->uCurFileOffset - m_uDataOffset) % m_nBlockAlign;
	AkUInt32 uMaxInputSize = ((uSrcFramesToDecode + m_wSamplesPerBlock - 1) / m_wSamplesPerBlock) * m_nBlockAlign + uNextBlockSize;

	// Count available ATRAC9 blocks
	AKRESULT eStmResult = AK_DataReady;
	AkUInt32 uInputSize = 0;
	while (uInputSize < uMaxInputSize)
	{
		if (uAvailableInput < uNextBlockSize)
		{
			// Must stream in more blocks
			if (HasNoMoreStreamData(pStream))
			{
				// WG-49802 For the very last buffer, just feed the rest of the input to AJM, even if it doesn't align with the ATRAC9 block size
				uInputSize += uAvailableInput;
				uAvailableInput -= uAvailableInput;
				eStmResult = AK_NoMoreData;
				break;
			}
			// Only loop the stream when allowed
			if (!bAllowLoop && NextFetchWillLoop(pStream))
			{
				// WG-49802 For the last buffer of the loop, just feed the rest of the input to AJM, even if it doesn't align with the ATRAC9 block size
				uInputSize += uAvailableInput;
				uAvailableInput -= uAvailableInput;
				eStmResult = uInputSize > 0 ? AK_DataReady : AK_NoDataReady;
				break;
			}
			// Move current stream buffer to previous slot to make room for the next one
			if (m_PreviousInputBufferSize == 0 && pStream->uSizeLeft > 0)
			{
				AK_AJM_DEBUG_PRINTF("[AJM %p] Saving input buffer [%u,%u]\n", this, pStream->uCurFileOffset, pStream->uSizeLeft);
				m_pPreviousInputBuffer = (AkUInt8*)pStream->pNextAddress;
				m_PreviousInputBufferSize = pStream->uSizeLeft;
				ConsumeData(pStream, pStream->uSizeLeft);
			}
			// If the stream still has data, then AJM must first consume the previous input buffer before we can fetch a new one
			if (pStream->uSizeLeft > 0)
			{
				eStmResult = AK_DataReady;
				break;
			}
			// Everything is clear to get a new buffer
			eStmResult = FetchStreamBuffer(pStream);
			if (eStmResult != AK_DataReady)
			{
				AK_AJM_DEBUG_PRINTF("[AJM %p] Could not fetch new input buffer: %d\n", this, eStmResult);
				break;
			}
			AK_AJM_DEBUG_PRINTF("[AJM %p] Fetched new input buffer [%u,%u]\n", this, pStream->uCurFileOffset, pStream->uSizeLeft);
			uAvailableInput += pStream->uSizeLeft;
		}
		// Add a block
		uInputSize += uNextBlockSize;
		uAvailableInput -= uNextBlockSize;

		// Next block should be a full block
		uNextBlockSize = m_nBlockAlign;
	}
	out_uInputSize = uInputSize;
	return eStmResult;
}
