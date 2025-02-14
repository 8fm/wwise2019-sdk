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

#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include "AkSrcMedia.h"
#include "AkAjm.h"
#include "../OpusCommon.h"

class CAkAjmWemOpus : public IAkSrcMediaCodec
{
public:
	CAkAjmWemOpus();
	~CAkAjmWemOpus() { Term(); }

	virtual Result Init(const AK::SrcMedia::Header& in_header, AK::SrcMedia::CodecInfo& out_codec, AkUInt16 uLoopCnt) override;
	virtual Result Warmup(AK::SrcMedia::Stream::State* pStream) override { return AK_Success; };
	virtual void Term() override;

	virtual Result PrepareNextBuffer(AK::SrcMedia::Stream::State* pStream, const AK::SrcMedia::Position::State& position, const PitchInfo& pitch) override;
	virtual Result GetBuffer(AK::SrcMedia::Stream::State* pStream, AkUInt16 uMaxFrames, BufferInfo& out_buffer) override;
	virtual void ReleaseBuffer(AK::SrcMedia::Stream::State* pStream) override;

	virtual void VirtualOn() override;
	virtual Result VirtualOff() override;

	virtual Result FindClosestFileOffset(AkUInt32 in_uDesiredSample, SeekInfo& out_SeekInfo) override;
	virtual Result Seek(AK::SrcMedia::Stream::State* pStream, const SeekInfo& seek, AkUInt32 uNumFramesInRange, AkUInt16 uLoopCnt) override;

	virtual void StopLooping(const AK::SrcMedia::Position::State& position) override;

private:

	Result CreateDecoder();
	void DestroyDecoder();

	AKRESULT AllocatePacketData();
	void FreePacketData();

	IAkSrcMediaCodec::Result ProcessDecodeBatch(AK::SrcMedia::Stream::State* pStream);
	AKRESULT GatherPackets(AK::SrcMedia::Stream::State* pStream, AkUInt32 uNumPackets);
	void RecordConsumedInput(AkUInt32 uConsumed);

	void ResolveSeekPacket(AkUInt32 uDesiredSample, AkUInt32& out_uPacketIndex, AkUInt32& out_uSamplesToSkip) const
	{
		AkUInt32 uDelayTarget = uDesiredSample + m_uCodecDelay;
		AkUInt32 uPacketIndex = uDelayTarget / m_uSamplesPerPacket;

		AkUInt32 uSkipSamples = uDelayTarget - (uPacketIndex * m_uSamplesPerPacket);

		// Keep going back until preroll is satisfied.
		AkUInt32 uPreroll = (AkUInt32)(OPUS_PREROLL_MS * m_uSampleRate / 1000.0f);
		while (uSkipSamples < uPreroll && uPacketIndex > 0)
		{
			uPacketIndex--;
			uSkipSamples += m_uSamplesPerPacket;
		}
		out_uPacketIndex = uPacketIndex;
		out_uSamplesToSkip = uSkipSamples;
	}

	inline AkUInt32 BufferSize() const
	{
		return AK_NUM_VOICE_REFILL_FRAMES * FrameSize();
	}
	inline AkUInt8 FrameSize() const
	{
		return AK_AJM_OUTPUT_SAMPLE_SIZE * m_uNumChannels;
	}

	AK::Ajm::Instance* m_pAjmInstance;
	AK::SrcMedia::ResamplingRamp::State m_Ramp;
	AkUInt16 m_uLoopCnt;    // Codec loop count, which is NOT the same as the voice loop count or stream loop count (due to buffering)

	AkUInt32 m_uSkipSamples;
	AkUInt32 m_uCurPacket;
	struct {
		AkUInt8* pData;
		AkUInt32 uAllocated; // Allocated size of pData
		AkUInt32 uGathered;  // Number of bytes copied from input stream, including a possibly incomplete packet at the end
		AkUInt32 uReady;     // Number of bytes containing full packets
	} m_PacketData;

	AK::SrcMedia::ConstantFrameSeekTable m_SeekTable;

	AkUInt32 m_uNumChannels;
	AkUInt32 m_uSampleRate;
	AkUInt32 m_uLoopStartPacketIndex;
	AkUInt32 m_uLoopStartSkipSamples;
	AkUInt16 m_uSamplesPerPacket;
	AkUInt16 m_uCodecDelay;
	AkUInt8  m_uMappingFamily;

	bool m_bStopLooping : 1;
};