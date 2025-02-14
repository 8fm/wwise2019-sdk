/***********************************************************************
The content of this file includes source code for the sound engine
portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
Two Source Code" as defined in the Source Code Addendum attached
with this file.  Any use of the Level Two Source Code shall be
subject to the terms and conditions outlined in the Source Code
Addendum and the End User License Agreement for Wwise(R).

Version:   Build: 
Copyright (c) 2006-2020 Audiokinetic Inc.
***********************************************************************/

#pragma once

#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include "AkSrcMedia.h"
#include "OpusCommon.h"

struct OpusMSDecoder;

class CAkCodecWemOpus : public IAkSrcMediaCodec
{
public:

	CAkCodecWemOpus();
	~CAkCodecWemOpus() { Term(); }

	virtual Result Init(const AK::SrcMedia::Header& in_header, AK::SrcMedia::CodecInfo& out_codec, AkUInt16 uLoopCnt) override;

	virtual void Term() override;

	virtual Result Warmup(AK::SrcMedia::Stream::State* pStream) override
	{
		// Nothing to warm-up
		return AK_Success;
	}

	virtual Result PrepareNextBuffer(AK::SrcMedia::Stream::State* pStream, const AK::SrcMedia::Position::State& position, const PitchInfo& pitch) override
	{
		// Nothing to prepare
		return AK_Success;
	}

	virtual Result GetBuffer(AK::SrcMedia::Stream::State* pStream, AkUInt16 uMaxFrames, BufferInfo& out_buffer) override;

	virtual void ReleaseBuffer(AK::SrcMedia::Stream::State* pStream) override
	{
		// Nothing to release
	}

	virtual void VirtualOn() override
	{
		m_Stitcher.FreeStitchBuffer();
		FreeResources();
	}

	virtual Result VirtualOff() override
	{
		return AllocateResources();
	}

	virtual Result FindClosestFileOffset(AkUInt32 in_uDesiredSample, SeekInfo& out_SeekInfo) override;

	virtual Result Seek(AK::SrcMedia::Stream::State* pStream, const SeekInfo& seek, AkUInt32 uNumFramesInRange, AkUInt16 uLoopCnt) override;

	virtual void StopLooping(const AK::SrcMedia::Position::State& position) override;

	Result ProcessPacket(AK::SrcMedia::Stream::State* pStream, BufferInfo& out_buffer);

private:

	Result AllocateResources();

	void FreeResources();

	void ResolveSeekPacket(AkUInt32 uDesiredSample, AkUInt32 &out_uPacketIndex, AkUInt32 &out_uSamplesToSkip) const
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

	typedef AK::SrcMedia::PacketStitcher<AK::SrcMedia::ManualStitcherState> OpusPacketStitcher;

	AkUInt16       m_uSamplesPerPacket;
	OpusMSDecoder* m_pOpusDecoder;

	// Buffers
	AkPipelineBuffer m_OutputBuffer;

	// Seek table
	AK::SrcMedia::ConstantFrameSeekTable m_SeekTable;	

	// Parameters for Opus decoder
	AkUInt32  m_uSampleRate;
	AkUInt16  m_uCodecDelay;
	AkUInt32  m_uChannelConfig;
	AkUInt8   m_uMappingFamily;
	AkUInt32  m_uLoopStartPacketIndex;
	AkUInt32  m_uLoopStartSkipSamples;

	// Codec State
	AK::SrcMedia::Position::State m_Position;
	OpusPacketStitcher            m_Stitcher;
	AkUInt32                      m_uSkipSamples;
	AkUInt32                      m_uCurPacket;
};

class CAkFileCodecWemOpus : public IAkFileCodec
{
public:
	virtual ~CAkFileCodecWemOpus() {}

	virtual AKRESULT DecodeFile(AkUInt8* pDst, AkUInt32 uDstSize, AkUInt8* pSrc, AkUInt32 uSrcSize, AkUInt32& out_uSizeWritten) override;
};