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

#include "opus.h"
#include "opus_multistream.h"

#include "AkSrcMedia.h"
#include "OpusCommon.h"

#include "AkCodecWemOpus.h"

#include <stdio.h>

using namespace AK::SrcMedia::Stream;

// These channel mappings are used to configure the Opus decoder library to convert from the internal stream channel order (with coupled streams coming first) to standard Vorbis channel order.
static unsigned char AK_Opus_wav_permute_matrix_decode[8][8] =
{
	{0}, /* 1.0 mono   */
	{0, 1}, /* 2.0 stereo */
	{0, 2, 1}, /* 3.0 channel ('wide') stereo */
	{0, 1, 2, 3}, /* 4.0 discrete quadraphonic */
	{0, 4, 1, 2, 3}, /* 5.0 surround */
	{0, 4, 1, 2, 3, 5}, /* 5.1 surround */
	{0, 6, 1, 2, 3, 4, 5}, /* 6.1 surround */
	{0, 6, 1, 2, 3, 4, 5, 7} /* 7.1 surround (classic theater 8-track) */
};

CAkCodecWemOpus::CAkCodecWemOpus()
	: m_uSamplesPerPacket(0)
	, m_pOpusDecoder(nullptr)
	, m_uSampleRate(0)
	, m_uCodecDelay(0)
	, m_uChannelConfig(0)
	, m_uMappingFamily(0)
	, m_uLoopStartPacketIndex(0)
	, m_uLoopStartSkipSamples(0)
	, m_Position{}
	, m_uSkipSamples(0)
	, m_uCurPacket(0)
{
	m_OutputBuffer.Clear();
}

IAkSrcMediaCodec::Result CAkCodecWemOpus::Init(const AK::SrcMedia::Header& in_header, AK::SrcMedia::CodecInfo& out_codec, AkUInt16 uLoopCnt)
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
		OPUS_RATE,
		channelConfig,
		32,
		pFmt->nChannels * sizeof(AkReal32),
		AK_FLOAT,
		AK_NONINTERLEAVED);

	out_codec.uSourceSampleRate = OPUS_RATE;
	out_codec.uTotalSamples = pFmt->dwTotalPCMFrames;

	// Throughput.
	out_codec.heuristics.fThroughput = (AkReal32)pFmt->nAvgBytesPerSec / 1000.f;
	out_codec.uMinimalBufferSize = 1;

	// Attributes.
	out_codec.uAttributes = 0;

	// Channel ordering.
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
	m_uChannelConfig = pFmt->uChannelConfig;
	m_uMappingFamily = pFmt->uMappingFamily;
	m_uSkipSamples = (AkUInt32)pFmt->uCodecDelay;

	// Seek table.
	AKRESULT res = m_SeekTable.Init(pFmt->uSeekTableSize, in_header.SeekInfo.pSeekTable, m_uSamplesPerPacket);
	if (res != AK_Success)
		return res;	

	AK::SrcMedia::Position::Init(&m_Position, in_header, out_codec, uLoopCnt);
	if (m_Position.uPCMLoopEnd == 0) m_Position.uPCMLoopEnd = pFmt->dwTotalPCMFrames - 1;

	// Loop points
	// We resolve uLoopStart in order to pre-roll for higher quality.
	ResolveSeekPacket(in_header.uPCMLoopStart, m_uLoopStartPacketIndex, m_uLoopStartSkipSamples);
	out_codec.heuristics.uLoopStart = in_header.uDataOffset + m_SeekTable.GetPacketDataOffsetFromIndex(m_uLoopStartPacketIndex);

	// Loop end doesn't need pre-roll, we don't seek there. We just need the index of the first packet FOLLOWING the one containing the loop point
	AkUInt32 uPCMLoopEnd = in_header.uPCMLoopEnd > 0 ? in_header.uPCMLoopEnd : pFmt->dwTotalPCMFrames - 1;
	AkUInt32 uLoopEndPacketIndex = (uPCMLoopEnd + m_uCodecDelay) / m_uSamplesPerPacket;
	out_codec.heuristics.uLoopEnd = in_header.uDataOffset + m_SeekTable.GetPacketDataOffsetFromIndex(uLoopEndPacketIndex + 1);

	return AllocateResources();
}

void CAkCodecWemOpus::Term()
{
	m_Stitcher.FreeStitchBuffer();
	FreeResources();
	m_SeekTable.Term();		
}

IAkSrcMediaCodec::Result CAkCodecWemOpus::GetBuffer(AK::SrcMedia::Stream::State* pStream, AkUInt16 uMaxFrames, BufferInfo& out_buffer)
{
	Result eResult;
	do
	{
		eResult = ProcessPacket(pStream, out_buffer);
		// Keep consuming packets until we're done skipping frames
	} while (eResult == AK_DataNeeded);
	return eResult;
}

IAkSrcMediaCodec::Result CAkCodecWemOpus::FindClosestFileOffset(AkUInt32 in_uDesiredSample, SeekInfo& out_SeekInfo)
{
	AkUInt32 uPacketIndex, uSkipSamples;
	ResolveSeekPacket(in_uDesiredSample, uPacketIndex, uSkipSamples);
	out_SeekInfo.uPCMOffset = in_uDesiredSample;
	out_SeekInfo.uFileOffset = m_SeekTable.GetPacketDataOffsetFromIndex(uPacketIndex);
	out_SeekInfo.uSkipLength = uSkipSamples;
	m_uCurPacket = uPacketIndex;
	return AK_Success;
}

IAkSrcMediaCodec::Result CAkCodecWemOpus::Seek(AK::SrcMedia::Stream::State* pStream, const SeekInfo& seek, AkUInt32 uNumFramesInRange, AkUInt16 uLoopCnt)
{
	m_Position.uCurSample = seek.uPCMOffset;
	m_Position.uLoopCnt = uLoopCnt;
	m_uSkipSamples = seek.uSkipLength;
	return AK_Success;
}

void CAkCodecWemOpus::StopLooping(const AK::SrcMedia::Position::State& position)
{
	AKASSERT(position.uCurSample == m_Position.uCurSample);
	m_Position.uLoopCnt = position.uLoopCnt;
}


// This is the function that writes the pcm output as a de-interleaved buffer
static void ak_opus_deinterleave_channel_out_float(
	void *dst,
	int dst_stride,
	int dst_channel,
	const float *src,
	int src_stride,
	int frame_size,
	void *user_data
)
{
	AkUInt32 uSkipSamples = *(AkUInt32*)user_data;
	float *float_dst = (float*)dst;

	if (src != NULL)
	{
		for (int i = uSkipSamples; i < frame_size; i++)
			float_dst[dst_channel*frame_size + (i - uSkipSamples)] = src[i*src_stride];
	}
	else
	{
		for (int i = uSkipSamples; i < frame_size; i++)
			float_dst[dst_channel*frame_size + (i - uSkipSamples)] = 0;
	}
}

IAkSrcMediaCodec::Result CAkCodecWemOpus::ProcessPacket(AK::SrcMedia::Stream::State* pStream, BufferInfo& out_buffer)
{
	void* pPacket = nullptr;
	AkUInt32 uPacketSize = 0;

	// First, try to gather a full packet
	m_Stitcher.m_PacketState.uNextPacketSize = m_SeekTable.GetPacketSize(m_uCurPacket);
	AKRESULT eResult = m_Stitcher.GetNextPacket(pStream, pPacket, uPacketSize);
	if (eResult != AK_DataReady)
		return eResult;
	AKASSERT(pPacket != nullptr);
	AKASSERT(uPacketSize > 0);
	m_uCurPacket++;

	int samples = opus_multistream_decode_native(
		m_pOpusDecoder,
		(AkUInt8*)pPacket,
		uPacketSize,
		m_OutputBuffer.GetInterleavedData(),
		ak_opus_deinterleave_channel_out_float,
		m_uSamplesPerPacket,
		0,
		0,
		&m_uSkipSamples);
	if (samples < 0)
	{
		return Result(AK_Fail, AK::Monitor::ErrorCode_OpusDecodeError);
	}

	m_Stitcher.FreeStitchBuffer();

	AKASSERT(samples > 0);

	// Record skipped samples
	AkUInt32 uSkippedSamples = AkMin((AkUInt32)samples, m_uSkipSamples);
	m_uSkipSamples -= uSkippedSamples;

	AkUInt32 uValidFrames = samples - uSkippedSamples;
	if (uValidFrames == 0)
		return AK_DataNeeded; // Skipped the entire packet; we need another one

	// Cut samples beyond the loop end point
	AK::SrcMedia::Position::ClampFrames(&m_Position, uValidFrames);

	out_buffer.Buffer.AttachInterleavedData(
		m_OutputBuffer.GetInterleavedData(),
		samples, // Very important to specify this number as uMaxFrames, because it is the actual stride of the de-interleaved buffer
		uValidFrames);

	out_buffer.uSrcFrames = uValidFrames;
	bool bLoop;
	AK::SrcMedia::Position::Forward(&m_Position, uValidFrames, bLoop);
	if (bLoop)
	{
		// We will be seeking back to loop start on the next GetBuffer()
		// Don't forget to skip over the usual 80 ms pre-roll
		m_uSkipSamples = m_uLoopStartSkipSamples;
		m_uCurPacket = m_uLoopStartPacketIndex;
	}
	return AK_DataReady;
}

IAkSrcMediaCodec::Result CAkCodecWemOpus::AllocateResources()
{
	AkChannelConfig channelConfig;
	channelConfig.Deserialize(m_uChannelConfig);

	m_OutputBuffer.SetChannelConfig(channelConfig);
	m_OutputBuffer.SetRequestSize(m_uSamplesPerPacket);
	AKRESULT eResult = m_OutputBuffer.GetCachedBuffer();
	if (eResult != AK_Success)
	{
		Term();
		return eResult;
	}

	int uMappingFamily;
	int uCoupledStreams;
	AK::Opus::ChannelConfigToMapping(channelConfig, uMappingFamily, uCoupledStreams);
	AKASSERT(m_uMappingFamily == uMappingFamily);

	int uStreamCount = channelConfig.uNumChannels - uCoupledStreams;

	unsigned char mapping[255];
	AK::Opus::WriteChannelMapping(channelConfig.uNumChannels, uMappingFamily == 1 ? AK_Opus_wav_permute_matrix_decode[channelConfig.uNumChannels-1] : nullptr, mapping);
	m_pOpusDecoder = (OpusMSDecoder*)AkAlloc(AkMemID_Processing, opus_multistream_decoder_get_size(uStreamCount, uCoupledStreams));
	if (!m_pOpusDecoder)
	{
		Term();
		return AK_InsufficientMemory;
	}

	int error = opus_multistream_decoder_init(
		m_pOpusDecoder,
		m_uSampleRate,
		channelConfig.uNumChannels,
		uStreamCount,
		uCoupledStreams,
		mapping);

	if (error != OPUS_OK)
	{
		FreeResources();
		return AK_Fail;
	}
	return AK_Success;
}

void CAkCodecWemOpus::FreeResources()
{
	if (m_pOpusDecoder)
	{
		AkFree(AkMemID_Processing, m_pOpusDecoder);
		m_pOpusDecoder = nullptr;
	}
	if (m_OutputBuffer.HasData())
	{
		m_OutputBuffer.ReleaseCachedBuffer();
	}
}

void AkOpusInterleaveConvertInt16AndRemap(AkAudioBuffer * pBufIn, AkAudioBuffer * pBufOut)
{
	const AkUInt32 uFramesToCopy = pBufIn->uValidFrames;
	const AkUInt32 uNumChannels = pBufIn->NumChannels();
	for ( AkUInt32 i = 0; i < uNumChannels; ++i )
	{
		AkReal32 * AK_RESTRICT pIn = (AkReal32 * AK_RESTRICT)(pBufIn->GetChannel(AkVorbisChannelMappingFunc(pBufIn->GetChannelConfig(), i)));
		AkInt16 * AK_RESTRICT pOut = (AkInt16 * AK_RESTRICT)( pBufOut->GetInterleavedData() ) + i;
		for ( AkUInt32 j = 0; j < uFramesToCopy; ++j )
		{
			*pOut = FLOAT_TO_INT16(*pIn);
			pOut += uNumChannels;
			pIn++;
		}
	}
}

void AkOpusInterleaveConvertInt16(AkAudioBuffer *pBufIn, AkAudioBuffer * pBufOut)
{
	const AkUInt32 uFramesToCopy = pBufIn->uValidFrames;
	const AkUInt32 uNumChannels = pBufIn->NumChannels();
	for ( AkUInt32 i = 0; i < uNumChannels; ++i )
	{
		AkReal32 * AK_RESTRICT pIn = (AkReal32 * AK_RESTRICT)(pBufIn->GetChannel(i));
		AkInt16 * AK_RESTRICT pOut = (AkInt16 * AK_RESTRICT)( pBufOut->GetInterleavedData() ) + i;
		for ( AkUInt32 j = 0; j < uFramesToCopy; ++j )
		{
			*pOut = FLOAT_TO_INT16(*pIn);
			pOut += uNumChannels;
			pIn++;
		}
	}
}

AKRESULT CAkFileCodecWemOpus::DecodeFile(AkUInt8* pDst, AkUInt32 uDstSize, AkUInt8* pSrc, AkUInt32 uSrcSize, AkUInt32& out_uSizeWritten)
{
	AKRESULT eResult;
	AK::SrcMedia::Header header;

	// Step 1: Parse header
	eResult = AkFileParser::Parse(
		pSrc,
		uSrcSize,
		header.FormatInfo,
		NULL,
		&header.uPCMLoopStart,
		&header.uPCMLoopEnd,
		&header.uDataSize,
		&header.uDataOffset,
		&header.AnalysisData,
		&header.SeekInfo);

	if (eResult != AK_Success)
		return eResult;

	// Step 2: Initialize the SrcMedia stream
	AK::SrcMedia::Stream::State stream;
	AK::SrcMedia::Stream::InitParams streamParams;
	AkSrcTypeInfo srcTypeInfo;
	srcTypeInfo.mediaInfo.Type = SrcTypeMemory;
	streamParams.pBuffer = pSrc;
	streamParams.uBufferSize = uSrcSize;
	streamParams.priority = AK_DEFAULT_PRIORITY;
	streamParams.pSrcType = &srcTypeInfo;
	streamParams.uLoopCnt = LOOPING_ONE_SHOT;
	eResult = AK::SrcMedia::Stream::Init(&stream, streamParams);
	if (eResult != AK_Success)
		return eResult;

	// Step 3: Instantiate, initialize, and warm-up the codec
	CAkCodecWemOpus* pCodec = AkNew(AkMemID_Processing, CAkCodecWemOpus());
	if (!pCodec)
	{
		AK::SrcMedia::Stream::Term(&stream);
		return AK_InsufficientMemory;
	}

	AK::SrcMedia::CodecInfo codec{};
	stream.pStream->GetHeuristics(codec.heuristics);
	eResult = pCodec->Init(header, codec, LOOPING_ONE_SHOT);
	if (eResult != AK_Success)
	{
		AkFree(AkMemID_Processing, pCodec);
		AK::SrcMedia::Stream::Term(&stream);
		return eResult;
	}

	// Step 4: Decode loop
	AkUInt16* pWrite = (AkUInt16*)pDst;
	AkUInt16* pWriteEnd = (AkUInt16*)(pDst + uDstSize);
	while (true)
	{
		IAkSrcMediaCodec::BufferInfo bufferIn;
		AkAudioBuffer bufferOut;
		bufferIn.Buffer.AttachInterleavedData(nullptr, 0, 0, codec.format.channelConfig);
		eResult = pCodec->ProcessPacket(&stream, bufferIn);

		if (eResult == AK_Fail)
		{
			break;
		}

		// Process produced samples
		if (bufferIn.Buffer.uValidFrames > 0)
		{
			AkUInt32 uCopySamples = AkMin((AkUInt32)(pWriteEnd - pWrite), (AkUInt32)bufferIn.Buffer.uValidFrames * codec.format.channelConfig.uNumChannels);
			bufferOut.AttachInterleavedData(pWrite, bufferIn.Buffer.uValidFrames, bufferIn.Buffer.uValidFrames);

			if (codec.format.channelConfig.eConfigType == AK_ChannelConfigType_Standard && codec.format.GetNumChannels() >= 2)
			{
				AkOpusInterleaveConvertInt16AndRemap(&bufferIn.Buffer, &bufferOut);
			}
			else
			{
				AkOpusInterleaveConvertInt16(&bufferIn.Buffer, &bufferOut);
			}
			pWrite += uCopySamples;
		}

		if (pWrite == pWriteEnd)
		{
			eResult = AK_Success;
			break;
		}
	}

	pCodec->Term();
	AkFree(AkMemID_Processing, pCodec);
	AK::SrcMedia::Stream::Term(&stream);
	return eResult;
}
