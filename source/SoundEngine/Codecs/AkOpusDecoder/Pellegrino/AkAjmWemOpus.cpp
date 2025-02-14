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

#include "AkAjmWemOpus.h"
#include "AkAjmScheduler.h"

#include <AK/Tools/Common/AkRingBuffer.h>

extern AkPlatformInitSettings g_PDSettings;

#define AK_AJM_INSTANCE_FLAGS(__nchan__) (SCE_AJM_INSTANCE_FLAG_MAX_CHANNEL(__nchan__) | SCE_AJM_INSTANCE_FLAG_FORMAT(SCE_AJM_FORMAT_ENCODING_S16)) | SCE_AJM_INSTANCE_FLAG_RESAMPLE

CAkAjmWemOpus::CAkAjmWemOpus()
	: m_pAjmInstance(nullptr)
	, m_uLoopCnt(0)
	, m_uSkipSamples(0)
	, m_uCurPacket(0)
	, m_uNumChannels(0)
	, m_uSampleRate(0)
	, m_uLoopStartPacketIndex(0)
	, m_uLoopStartSkipSamples(0)
	, m_uSamplesPerPacket(0)
	, m_uCodecDelay(0)
	, m_uMappingFamily(0)
	, m_bStopLooping(false)
{
}

IAkSrcMediaCodec::Result CAkAjmWemOpus::Init(const AK::SrcMedia::Header& in_header, AK::SrcMedia::CodecInfo& out_codec, AkUInt16 uLoopCnt)
{
	if (in_header.FormatInfo.pFormat->wFormatTag != AK_WAVE_FORMAT_OPUS_WEM ||
		in_header.FormatInfo.uFormatSize < sizeof(WaveFormatOpusWEM))
	{
		return AK_FileFormatMismatch;
	}

	// Format
	WaveFormatOpusWEM* pFmt = (WaveFormatOpusWEM*)in_header.FormatInfo.pFormat;
	if (!AK::Opus::ValidateFormat(pFmt))
	{
		return AK_InvalidFile;
	}

	AkChannelConfig channelConfig = pFmt->GetChannelConfig();
	out_codec.format.SetAll(
		AK_CORE_SAMPLERATE,
		channelConfig,
		AK_AJM_OUTPUT_SAMPLE_SIZE * 8,
		AK_AJM_OUTPUT_SAMPLE_SIZE * pFmt->nChannels,
		AK_INT,
		AK_INTERLEAVED);

	out_codec.uSourceSampleRate = OPUS_RATE;
	out_codec.uTotalSamples = pFmt->dwTotalPCMFrames;

	// Throughput.
	out_codec.heuristics.fThroughput = (AkReal32)pFmt->nAvgBytesPerSec / 1000.f;
	out_codec.uMinimalBufferSize = 1;

	// Attributes.
	out_codec.uAttributes = AK_CODEC_ATTR_HARDWARE | AK_CODEC_ATTR_PITCH;
	if (g_PDSettings.bHwCodecLowLatencyMode)
		out_codec.uAttributes |= AK_CODEC_ATTR_HARDWARE_LL;

	// Channel config
	if (pFmt->uMappingFamily == 1)
	{
		out_codec.eOrdering = AK::SrcMedia::ChannelOrdering::ChannelOrdering_Vorbis;
	}
	else if (channelConfig.eConfigType == AK_ChannelConfigType_Standard)
	{
		out_codec.eOrdering = AK::SrcMedia::ChannelOrdering::ChannelOrdering_Wave;
	}

	// Copy data required for operation and coming off of virtual
	m_uSamplesPerPacket = pFmt->wSamplesPerBlock;
	m_uSampleRate = pFmt->nSamplesPerSec;
	m_uCodecDelay = pFmt->uCodecDelay;
	m_uNumChannels = channelConfig.uNumChannels;
	m_uMappingFamily = pFmt->uMappingFamily;
	m_uSkipSamples = (AkUInt32)pFmt->uCodecDelay;
	m_uLoopCnt = uLoopCnt;

	// Seek table.
	AKRESULT res = m_SeekTable.Init(pFmt->uSeekTableSize, in_header.SeekInfo.pSeekTable, m_uSamplesPerPacket);
	if (res != AK_Success)
		return res;

	AK::SrcMedia::ResamplingRamp::Init(&m_Ramp, m_uSampleRate);

	// Loop points
	// We resolve uLoopStart in order to pre-roll for higher quality.
	ResolveSeekPacket(in_header.uPCMLoopStart, m_uLoopStartPacketIndex, m_uLoopStartSkipSamples);
	out_codec.heuristics.uLoopStart = in_header.uDataOffset + m_SeekTable.GetPacketDataOffsetFromIndex(m_uLoopStartPacketIndex);

	// Loop end doesn't need pre-roll, we don't seek there. We just need the index of the first packet FOLLOWING the one containing the loop point
	AkUInt32 uPCMLoopEnd = in_header.uPCMLoopEnd > 0 ? in_header.uPCMLoopEnd : pFmt->dwTotalPCMFrames - 1;
	AkUInt32 uLoopEndPacketIndex = (uPCMLoopEnd + m_uCodecDelay) / m_uSamplesPerPacket;
	out_codec.heuristics.uLoopEnd = in_header.uDataOffset + m_SeekTable.GetPacketDataOffsetFromIndex(uLoopEndPacketIndex + 1);

	// Allocate enough space to accomodate enough packets to fill up the buffer
	res = AllocatePacketData();
	if (res != AK_Success)
		return res;

	AK_AJM_DEBUG_PRINTF("[AJM %p] Opus Header Parsed; TotalFrames %d; PCM Loops [%d, %d]; File Loops [%d, %d]\n", this, out_codec.uTotalSamples, in_header.uPCMLoopStart, in_header.uPCMLoopEnd, out_codec.heuristics.uLoopStart, out_codec.heuristics.uLoopEnd);

	return CreateDecoder();
}

void CAkAjmWemOpus::Term()
{
	DestroyDecoder();
	m_SeekTable.Term();
	FreePacketData();
}

IAkSrcMediaCodec::Result CAkAjmWemOpus::PrepareNextBuffer(AK::SrcMedia::Stream::State* pStream, const AK::SrcMedia::Position::State& position, const PitchInfo& pitch)
{
	AK_INSTRUMENT_SCOPE("CAkAjmWemOpus::PrepareNextBuffer");

	// Nothing to do if the current batch is still in flight.
	if (m_pAjmInstance->BatchStatus == AK::Ajm::Batch::Status::InProgress)
		return AK_Success;

	IAkSrcMediaCodec::Result eResult = ProcessDecodeBatch(pStream);
	if (eResult == AK_Fail)
		return eResult;

	AK::SrcMedia::ResamplingRamp::SetRampTargetPitch(&m_Ramp, pitch.fPitch);
	if (pitch.eType == PitchShiftType_LinearConstant)
		m_Ramp.fLastRatio = m_Ramp.fTargetRatio;

	bool bResetResampler = (m_pAjmInstance->Resampler.Flags & SCE_AJM_RESAMPLE_FLAG_RESET) != 0;
	AkUInt32 uResamplerFrames = !bResetResampler && m_pAjmInstance->Resampler.Info.sResampleInfo.iNumSamples > 0 ? m_pAjmInstance->Resampler.Info.sResampleInfo.iNumSamples : 0;

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
			AKASSERT(pStream->uSizeLeft == 0 && m_PacketData.uReady == 0 && m_PacketData.uGathered == 0); // At the end of a range, all previous input should have been consumed.
			if (m_uLoopCnt > 1) --m_uLoopCnt;
			AK::Ajm::SetRange(m_pAjmInstance, position.uPCMLoopStart, (m_uLoopCnt != LOOPING_ONE_SHOT ? position.uPCMLoopEnd + 1 : position.uTotalSamples) - position.uPCMLoopStart, uResamplerFrames);
			AK_AJM_DEBUG_PRINTF("[AJM %p] Looping back to decode range [%d, %d].\n", this, m_pAjmInstance->RangeStart, m_pAjmInstance->RangeLength);
			m_uSkipSamples = m_uLoopStartSkipSamples;
			m_uCurPacket = m_uLoopStartPacketIndex;
			bAllowStmLoop = true;
		}
		else if (m_pAjmInstance->RangeProgress == m_pAjmInstance->RangeLength)
		{
			AK_AJM_DEBUG_PRINTF("[AJM %p] is done.\n", this);
			// Signal NoMoreData so that the next source in the VPL can start decoding in advance
			return AK_NoMoreData;
		}
	}

	AkUInt32 uSrcFramesLeft = m_pAjmInstance->RangeLength - m_pAjmInstance->RangeProgress;
	AkUInt32 uOutputFramesLeft = AK::SrcMedia::ResamplingRamp::MaxOutputFrames(&m_Ramp, uSrcFramesLeft);

	// Determine how many samples we want to decode on next batch
	AkUInt32 uFramesToRefill = AK::Ajm::SpaceAvailable(m_pAjmInstance) / FrameSize();
	if (uFramesToRefill == 0) return AK_Success; // Buffer is already full, nothing to decode

	bool bWillStartNewRange = m_pAjmInstance->State == AK::Ajm::Instance::InstanceState::Uninitialized || m_pAjmInstance->State == AK::Ajm::Instance::InstanceState::IdleStartOfRange;
	bool bIsLastDecodeBatch = m_uLoopCnt == LOOPING_ONE_SHOT && uFramesToRefill >= uOutputFramesLeft;

	// Determine size of input required for this batch
	AkUInt32 uSrcFramesToDecode = AK::SrcMedia::ResamplingRamp::MaxSrcFrames(&m_Ramp, uFramesToRefill);

	// Always feed more samples to allow resampler to fill its internal buffer
	if (uResamplerFrames < AK_AJM_RESAMPLER_EXTRA_FRAMES)
		uSrcFramesToDecode += AK_AJM_RESAMPLER_EXTRA_FRAMES - uResamplerFrames;

	// Cap this total to the number of src frames left in the range that have not been decoded
	AKASSERT(uSrcFramesLeft >= uResamplerFrames);
	uSrcFramesToDecode = AkMin(uSrcFramesToDecode, uSrcFramesLeft - uResamplerFrames);

	// Skip the encoder delay frames and the beginning part of the block before our range start point
	if (bWillStartNewRange)
		uSrcFramesToDecode += m_uSkipSamples;

	AkUInt32 uPacketsToGather = (uSrcFramesToDecode + m_uSamplesPerPacket - 1) / m_uSamplesPerPacket;

	// Read from input stream to gather as many bytes as possible to send to AJM
	AKRESULT eStmResult = GatherPackets(pStream, uPacketsToGather);
	if (eStmResult == AK_Fail || eStmResult == AK_InsufficientMemory)
		return eStmResult;

	if (m_PacketData.uReady == 0 && !bIsLastDecodeBatch)
		return AK_NoDataReady; // No data to decode

	// Add this instance's jobs to the next batch
	m_pAjmInstance->JobFlags = 0;
	if (m_pAjmInstance->State == AK::Ajm::Instance::InstanceState::Uninitialized)
	{
		AK::Ajm::AddInitializeOpusJob(m_pAjmInstance);
		AK_AJM_DEBUG_PRINTF("[AJM %p] Initialize(%d, %d, %d)\n", this, m_pAjmInstance->OpusInitParams.uiMappingFamily, m_pAjmInstance->OpusInitParams.uiNumChannels, m_pAjmInstance->OpusInitParams.uiSampleRate);
	}
	if (bWillStartNewRange || m_bStopLooping)
	{
		AK::Ajm::AddSetGaplessDecodeJob(m_pAjmInstance, m_uSkipSamples, m_pAjmInstance->RangeLength);
		AK_AJM_DEBUG_PRINTF("[AJM %p] SetGaplessDecode(%d, %d, %d)\n", this, m_pAjmInstance->GaplessDecode.Params.uiSkipSamples, m_pAjmInstance->GaplessDecode.Params.uiTotalSamples, m_pAjmInstance->GaplessDecode.reset);
	}
	{
		// The AJM resample keeps some decoded frames internally. To maximize quality, we only flush them out when the decoder is on its very last decode batch
		uint32_t flags = 0;
		if (bIsLastDecodeBatch) flags |= SCE_AJM_RESAMPLE_FLAG_END;
		// Reset must occur on the first decode batch after a seek
		if (bResetResampler) flags |= SCE_AJM_RESAMPLE_FLAG_RESET;

		AkUInt32 uExpectedRampLength = AkMin(uFramesToRefill, uOutputFramesLeft);

		AkReal32 rampStart = AK::SrcMedia::ResamplingRamp::RampStart(&m_Ramp);
		AkReal32 rampStep = AK::SrcMedia::ResamplingRamp::RampStep(&m_Ramp, uExpectedRampLength);
		AK::Ajm::AddSetResampleParameters(m_pAjmInstance, rampStart, rampStep, flags);
		AK::Ajm::AddGetResampleInfo(m_pAjmInstance);
		AK_AJM_DEBUG_PRINTF("[AJM %p] SetResampleParameters(%f, %f, %u)\n", this, m_pAjmInstance->Resampler.RatioStart, m_pAjmInstance->Resampler.RatioStep, m_pAjmInstance->Resampler.Flags);
	}
	AK::Ajm::AddDecodeJob(m_pAjmInstance, { m_PacketData.pData, m_PacketData.uReady }, { nullptr, 0 });
	AK_AJM_DEBUG_PRINTF("[AJM %p] Decoding [%lu,%zu] + [%u,%zu] onto [%u,%zu]\n",
		this,
		pStream->uCurFileOffset - m_pAjmInstance->InputDescriptors[0].szSize, m_pAjmInstance->InputDescriptors[0].szSize,
		pStream->uCurFileOffset, m_pAjmInstance->InputDescriptors[1].szSize,
		m_pAjmInstance->WriteHead, m_pAjmInstance->OutputDescriptors[0].szSize
		);
	AKASSERT(m_pAjmInstance->OutputDescriptors[0].szSize > 0);
	return bIsLastDecodeBatch ? AK_NoMoreData : AK_Success;
}

IAkSrcMediaCodec::Result CAkAjmWemOpus::GetBuffer(AK::SrcMedia::Stream::State* pStream, AkUInt16 uMaxFrames, BufferInfo& out_buffer)
{
	AK_INSTRUMENT_SCOPE("CAkAjmWemOpus::GetBuffer");

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

void CAkAjmWemOpus::ReleaseBuffer(AK::SrcMedia::Stream::State* pStream)
{
	if (m_pAjmInstance)
	{
		AK::Ajm::ReleaseBuffer(m_pAjmInstance);
		AK_AJM_DEBUG_PRINTF("[AJM %p] Released buffer. %d buffers are free.\n", this, m_pAjmInstance->uWriteCount);
	}
}

void CAkAjmWemOpus::VirtualOn()
{
	DestroyDecoder();
	FreePacketData();
}

IAkSrcMediaCodec::Result CAkAjmWemOpus::VirtualOff()
{
	AKRESULT eResult = AllocatePacketData();
	if (eResult != AK_Success) return eResult;
	return CreateDecoder();
}

IAkSrcMediaCodec::Result CAkAjmWemOpus::FindClosestFileOffset(AkUInt32 in_uDesiredSample, SeekInfo& out_SeekInfo)
{
	AkUInt32 uPacketIndex, uSkipSamples;
	ResolveSeekPacket(in_uDesiredSample, uPacketIndex, uSkipSamples);
	out_SeekInfo.uPCMOffset = in_uDesiredSample;
	out_SeekInfo.uFileOffset = m_SeekTable.GetPacketDataOffsetFromIndex(uPacketIndex);
	out_SeekInfo.uSkipLength = uSkipSamples;
	m_uCurPacket = uPacketIndex;
	return AK_Success;
}

IAkSrcMediaCodec::Result CAkAjmWemOpus::Seek(AK::SrcMedia::Stream::State* pStream, const SeekInfo& seek, AkUInt32 uNumFramesInRange, AkUInt16 uLoopCnt)
{
	AKASSERT(m_pAjmInstance != nullptr);
	AK::Ajm::Seek(m_pAjmInstance);
	AK::Ajm::SetRange(m_pAjmInstance, seek.uPCMOffset, uNumFramesInRange, 0);
	m_pAjmInstance->Resampler.Flags = SCE_AJM_RESAMPLE_FLAG_RESET;
	m_uSkipSamples = seek.uSkipLength;
	m_uLoopCnt = uLoopCnt;
	m_PacketData.uReady = m_PacketData.uGathered = 0;
	AK_AJM_DEBUG_PRINTF("[AJM %p] Seeking to PCM sample %d; file offset %d; skip %d, length %d\n", this, seek.uPCMOffset, seek.uFileOffset, seek.uSkipLength, uNumFramesInRange);
	return AK_Success;
}

void CAkAjmWemOpus::StopLooping(const AK::SrcMedia::Position::State& position)
{
	m_uLoopCnt = position.uLoopCnt;
	if (m_uLoopCnt == LOOPING_ONE_SHOT)
	{
		m_pAjmInstance->RangeLength = position.uTotalSamples - position.uPCMLoopStart;
		m_bStopLooping = true;
		AK_AJM_DEBUG_PRINTF("[AJM %p] will stop looping; decode range extended to [%d, %d].\n", this, m_pAjmInstance->RangeStart, m_pAjmInstance->RangeLength);
	}
}

IAkSrcMediaCodec::Result CAkAjmWemOpus::CreateDecoder()
{
	AkUInt32 uBufferSize = AK_NUM_VOICE_REFILL_FRAMES * AK_AJM_OUTPUT_SAMPLE_SIZE * m_uNumChannels;
	AKRESULT eResult = CAkGlobalAjmScheduler::Get().CreateInstance(
		SCE_AJM_CODEC_OPUS_DEC,
		AK_AJM_INSTANCE_FLAGS(m_uNumChannels),
		uBufferSize,
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

	m_pAjmInstance->OpusInitParams.uiMappingFamily = m_uMappingFamily;
	m_pAjmInstance->OpusInitParams.uiNumChannels = m_uNumChannels;
	m_pAjmInstance->OpusInitParams.uiSampleRate = m_uSampleRate;

	return AK_Success;
}

void CAkAjmWemOpus::DestroyDecoder()
{
	if (m_pAjmInstance)
	{
		AK_AJM_DEBUG_PRINTF("[AJM %p] Marked instance #%d for destruction\n", this, m_pAjmInstance->InstanceId);
		CAkGlobalAjmScheduler::Get().DestroyInstance(m_pAjmInstance);
		m_pAjmInstance = nullptr;
	}
}

AKRESULT CAkAjmWemOpus::AllocatePacketData()
{
	// Use the first packet size as a guide to allocate 
	m_PacketData.uAllocated = ((AK_AJM_NUM_BUFFERS * AK_NUM_VOICE_REFILL_FRAMES + m_uSamplesPerPacket - 1) / m_uSamplesPerPacket) * m_SeekTable.GetPacketSize(0);
	m_PacketData.pData = (AkUInt8*)AkMalign(AkMemID_Processing | AkMemType_Device, m_PacketData.uAllocated, AK_AJM_BUFFER_ALIGNMENT);
	m_PacketData.uGathered = 0;
	m_PacketData.uReady = 0;

	return m_PacketData.pData == nullptr ? AK_InsufficientMemory : AK_Success;
}

void CAkAjmWemOpus::FreePacketData()
{
	if (m_PacketData.pData)
	{
		AkFalign(AkMemID_Processing | AkMemType_Device, m_PacketData.pData);
		m_PacketData.pData = nullptr;
		m_PacketData.uGathered = m_PacketData.uReady = m_PacketData.uAllocated = 0;
	}
}

IAkSrcMediaCodec::Result CAkAjmWemOpus::ProcessDecodeBatch(AK::SrcMedia::Stream::State* pStream)
{
	AK_INSTRUMENT_SCOPE("CAkAjmWemOpus::ProcessDecodeBatch");

	AK::Ajm::Batch::Status eStatus = m_pAjmInstance->BatchStatus;

	AKASSERT(eStatus != AK::Ajm::Batch::Status::InProgress);

	// React to overall batch status here
	switch (eStatus)
	{
	case AK::Ajm::Batch::Status::Invalid:
		return AK_NoDataReady;

	case AK::Ajm::Batch::Status::Failure:
		// Keep the batch status to ensure the voice is killed
		return Result(AK_Fail, AK::Monitor::ErrorCode_OpusDecodeError);

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
		return Result(AK_Fail, AK::Monitor::ErrorCode_OpusDecodeError);

	m_bStopLooping = false;

	AK_AJM_DEBUG_PRINTF("[AJM %p] Produced %d bytes and advanced input by %d bytes. %d buffers are ready.\n", this, m_pAjmInstance->DecodeResult.sStream.iSizeProduced, m_pAjmInstance->DecodeResult.sStream.iSizeConsumed, m_pAjmInstance->uReadCount);
	AKASSERT(m_pAjmInstance->DecodeResult.sStream.iSizeProduced % FrameSize() == 0);

	// Remove the resampler reset flag and record the new last ratio
	m_pAjmInstance->Resampler.Flags &= ~SCE_AJM_RESAMPLE_FLAG_RESET;
	m_Ramp.fLastRatio = m_Ramp.fTargetRatio;

	// Record number of input bytes consumed in the stream source
	RecordConsumedInput(m_pAjmInstance->DecodeResult.sStream.iSizeConsumed);

	return AK_Success;
}

AKRESULT CAkAjmWemOpus::GatherPackets(AK::SrcMedia::Stream::State * pStream, AkUInt32 uNumPackets)
{
	// First count the packets already ready
	AkUInt32 i = 0;
	while (uNumPackets > 0 && i < m_PacketData.uReady)
	{
		uNumPackets--;
		AkUInt16 uPacketSize = *(AkUInt16*)(m_PacketData.pData + i);
		i += uPacketSize + 2;
	}
	// Then gather any new packet
	while (uNumPackets > 0)
	{
		if (pStream->uSizeLeft == 0)
		{
			// If we don't have data unconsumed and we will never get any more we are done decoding
			if (HasNoMoreStreamData(pStream))
				return AK_NoMoreData;

			// See if we can get more data from stream manager.
			ReleaseStreamBuffer(pStream);
			AKRESULT eStmResult = FetchStreamBuffer(pStream);
			if (eStmResult != AK_DataReady)
				return eStmResult;
		}

		AKASSERT(pStream->uSizeLeft > 0);

		AkUInt16 uPacketDataSize = m_SeekTable.GetPacketSize(m_uCurPacket);
		AkUInt32 uPacketSize = uPacketDataSize + 2;
		AkUInt32 uIncompleteData = m_PacketData.uGathered - m_PacketData.uReady;
		AkUInt32 uSizeToGather = uPacketSize - uIncompleteData;

		AKASSERT(uSizeToGather > 0);

		// First, check if there's enough space for this packet in our buffer
		if (m_PacketData.uAllocated - m_PacketData.uGathered < uSizeToGather)
		{
			// Need a larger buffer; realloc for the number of packets left to gather
			// In 2020.1 this will be optimized to use the new AkReallocAligned macro.
			AkUInt32 uNewAlloc = m_PacketData.uAllocated + (uPacketSize * uNumPackets);
			void * pNewData = (AkUInt8*)AkMalign(AkMemID_Processing | AkMemType_Device, uNewAlloc, AK_AJM_BUFFER_ALIGNMENT);
			if (pNewData == nullptr)
				return AK_InsufficientMemory;
			memcpy(pNewData, m_PacketData.pData, AkMin(uNewAlloc, m_PacketData.uAllocated));
			m_PacketData.pData = (AkUInt8*)pNewData;
			m_PacketData.uAllocated = uNewAlloc;
		}

		AKASSERT(m_PacketData.uAllocated - m_PacketData.uGathered >= uSizeToGather);

		// If we are writing a new packet, write the two-byte header required by AJM first.
		if (uIncompleteData == 0)
		{
			*(AkUInt16*)(m_PacketData.pData + m_PacketData.uGathered) = uPacketDataSize;
			m_PacketData.uGathered += 2;
			uIncompleteData += 2;
			uSizeToGather -= 2;
		}

		// Copy what we have of the packet data
		AkUInt32 uCopySize = AkMin(pStream->uSizeLeft, uSizeToGather);
		memcpy(m_PacketData.pData + m_PacketData.uGathered, pStream->pNextAddress, uCopySize);
		ConsumeData(pStream, uCopySize);
		m_PacketData.uGathered += uCopySize;

		if (uCopySize == uSizeToGather)
		{
			// We copied an entire packet; record it.
			m_PacketData.uReady += uPacketSize;
			uNumPackets--;
			m_uCurPacket++;
		}
	}
	return AK_DataReady;
}

void CAkAjmWemOpus::RecordConsumedInput(AkUInt32 uConsumed)
{
	AKASSERT(uConsumed <= m_PacketData.uReady);
	if (uConsumed > 0)
	{
		memmove(m_PacketData.pData, m_PacketData.pData + uConsumed, m_PacketData.uGathered - uConsumed); // XXX TODO use a ringbuffer instead.
		m_PacketData.uReady -= uConsumed;
		m_PacketData.uGathered -= uConsumed;
	}
}
