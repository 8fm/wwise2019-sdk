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

#include "AkSrcMedia.h"
#include "AkAjm.h"
#include "AkAjmScheduler.h"

IAkSoftwareCodec* CreateATRAC9SrcMedia(void* in_pCtx);

class CAkSrcMediaCodecAt9 : public IAkSrcMediaCodec
{
public:
	CAkSrcMediaCodecAt9();

	virtual Result Init(const AK::SrcMedia::Header & in_header, AK::SrcMedia::CodecInfo & out_codec, AkUInt16 uLoopCnt) override;
	virtual void Term() override;

	virtual Result Warmup(AK::SrcMedia::Stream::State* pStream) override { return AK_Success; } // no-op

	virtual Result PrepareNextBuffer(AK::SrcMedia::Stream::State* pStream, const AK::SrcMedia::Position::State &position, const PitchInfo &pitch) override;
	virtual Result GetBuffer(AK::SrcMedia::Stream::State * pStream, AkUInt16 uMaxFrames, BufferInfo & out_buffer) override;
	virtual void ReleaseBuffer(AK::SrcMedia::Stream::State * pStream) override;

	virtual void VirtualOn() override;
	virtual Result VirtualOff() override;

	virtual Result FindClosestFileOffset(AkUInt32 in_uDesiredSample, SeekInfo & out_SeekInfo) override;
	virtual Result Seek(AK::SrcMedia::Stream::State* pStream, const SeekInfo & seek, AkUInt32 uNumFrames, AkUInt16 uLoopCnt) override;

	virtual void StopLooping(const AK::SrcMedia::Position::State &position) override;

private:
	IAkSrcMediaCodec::Result CreateDecoder();
	void DestroyDecoder();

	IAkSrcMediaCodec::Result ProcessDecodeBatch(AK::SrcMedia::Stream::State * pStream);

	AKRESULT GatherStreamBuffers(AK::SrcMedia::Stream::State* pStream, AkUInt32 uSrcFramesToDecode, bool bAllowLoop, AkUInt32& out_uInputSize);

	inline AkUInt32 BufferSize() const
	{
		return AK_NUM_VOICE_REFILL_FRAMES * FrameSize();
	}
	inline AkUInt32 FramesToSkip() const
	{
		return (m_pAjmInstance->RangeStart % m_wSamplesPerBlock) + m_uAt9EncoderDelaySamples;
	}
	inline AkUInt8 FrameSize() const
	{
		return AK_AJM_OUTPUT_SAMPLE_SIZE * m_uNumChannels;
	}

	// Stream info
	AkUInt32 m_uNumChannels;
	AkUInt32 m_uSrcSampleRate;
	AkUInt32 m_uDataOffset;
	AkUInt16 m_wSamplesPerBlock;
	AkUInt16 m_nBlockAlign;
	AkUInt32 m_uAt9EncoderDelaySamples;
	AkUInt8 m_At9ConfigData[SCE_AJM_DEC_AT9_CONFIG_DATA_SIZE];

	AK::Ajm::Instance * m_pAjmInstance;

	AkUInt8 * m_pPreviousInputBuffer;
	AkUInt32 m_PreviousInputBufferSize;

	AK::SrcMedia::ResamplingRamp::State m_Ramp;
	AkUInt16 m_uLoopCnt;    // Codec loop count, which is NOT the same as the voice loop count or stream loop count (due to buffering)

	bool m_bStopLooping :1;
};
