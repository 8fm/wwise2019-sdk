/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

#include "stdafx.h"
#include "AkSrcMedia.h"

#include "AkSrcFileServices.h"
#include "AkSrcMediaCodecPCM.h"
#include "AkVPLSrcCbxNode.h"
#include "AkPositionRepository.h"

CAkSrcMedia::CAkSrcMedia(CAkPBI * pCtx, AkCreateSrcMediaCodecCallback pCodecFactory)
	: IAkSoftwareCodec(pCtx)
	, m_Position{}
	, m_StreamState{}
	, m_Envelope{}
	, m_pCodecFactory(pCodecFactory)
	, m_pCodec(nullptr)
	, m_uDataOffset(0)
	, m_uSourceSampleRate(0)
	, m_uCodecAttributes(0)
	, m_eOrdering(AK::SrcMedia::ChannelOrdering_Auto)
	, m_bHeaderFromBank(false)
	, m_bCodecVirtual(false)
	, m_bVoiceVirtual(false)
{
	AKASSERT(pCodecFactory != nullptr);
}

AKRESULT CAkSrcMedia::StartStream(AkUInt8 * in_pBuffer, AkUInt32 ulBufferSize)
{
	IAkSrcMediaCodec::Result eResult;

	EnterPreBufferingState();

	m_pCtx->CalcEffectiveParams();

	if (m_StreamState.pStream == nullptr)
	{
		// First, create stream
		AkSrcTypeInfo * pSrcTypeInfo = m_pCtx->GetSrcTypeInfo();
		AkUInt32 type = pSrcTypeInfo->mediaInfo.Type;
		AKASSERT(type == SrcTypeMemory || type == SrcTypeFile);
		AK::SrcMedia::Stream::InitParams params{};
		params.pSrcType = m_pCtx->GetSrcTypeInfo();
		params.uLoopCnt = m_pCtx->GetLooping();
		params.priority = m_pCtx->GetPriority();
		if (params.pSrcType->mediaInfo.Type == SrcTypeMemory || m_pCtx->IsPrefetched())
		{
			m_pCtx->GetDataPtr(params.pBuffer, params.uBufferSize);
		}
		eResult = AK::SrcMedia::Stream::Init(&m_StreamState, params);
		if (eResult != AK_Success)
			return eResult;
	}

	if (m_pCodec == nullptr)
	{
		// Fetch first buffer
		AkUInt8 * pBuffer = nullptr;
		AkUInt32 uSize = 0;
		eResult = AK::SrcMedia::Stream::ReadStreamBuffer(&m_StreamState, &pBuffer, &uSize);
		if (eResult == AK_NoDataReady)
			return AK_FormatNotReady; // Pipeline will re-try later
		if (eResult != AK_DataReady && eResult != AK_NoMoreData)
			return AK_Fail;

		// Parse the header
		AK::SrcMedia::Header header;
		eResult = ParseHeader(pBuffer, uSize, header);
		if (eResult != AK_Success)
			return eResult;

		// Initialize the producer based on header data
		eResult = InitCodec(pBuffer, uSize, header);
		if (eResult != AK_Success)
			return eResult;

		// Commit that first buffer now that file loop points are properly computed
		eResult = AK::SrcMedia::Stream::CommitStreamBuffer(&m_StreamState, pBuffer, uSize);
		if (eResult != AK_Success)
			return eResult;

		// "Consume" header.
		AKASSERT(m_StreamState.uSizeLeft >= header.uDataOffset || !"Header must be entirely contained within first stream buffer");
		AK::SrcMedia::Stream::ConsumeData(&m_StreamState, header.uDataOffset);
	}

	// Allow codec to warm up with additional stream data and return AK_FormatNotReady
	eResult = m_pCodec->Warmup(&m_StreamState);
	MonitorCodecError(eResult);
	if (eResult != AK_Success)
		return eResult;

	// Seek to source offset if necessary
	if (m_pCtx->RequiresSourceSeek())
	{
		// Compute the ideal PCM offset to seek to
		AkUInt32 uSourceOffset;
		AkUInt16 uLoopCnt;
		GetSourceOffset(uSourceOffset, uLoopCnt);

		eResult = SeekTo(uSourceOffset, uLoopCnt);
		if (eResult != AK_Success)
			return eResult;
	}

	// Allow HW codecs to prepare their first buffer immediately.
	PrepareNextBuffer();

	return AK_Success;
}

void CAkSrcMedia::StopStream()
{
	if (m_pCodec != nullptr)
	{
		m_pCodec->Term();
		AkDelete(AkMemID_Processing, m_pCodec);
		m_pCodec = nullptr;
	}

	if (m_pAnalysisData && !m_bHeaderFromBank)
	{
		AkFree(AkMemID_Processing, m_pAnalysisData);
		m_pAnalysisData = nullptr;
	}

	m_Markers.Free();

	AK::SrcMedia::Stream::Term(&m_StreamState);
}

void CAkSrcMedia::GetBuffer(AkVPLState & io_state)
{
	IAkSrcMediaCodec::Result eResult;
	// Update priority on the stream
	m_StreamState.priority = m_pCtx->GetPriority();

	AkAudioFormat fmt = m_pCtx->GetMediaFormat();

	// Obtain a decoded buffer from the codec
	IAkSrcMediaCodec::BufferInfo buffer;
	buffer.Buffer.AttachInterleavedData(nullptr, 0, 0, fmt.channelConfig);
	buffer.uSrcFrames = 0;
	eResult = m_pCodec->GetBuffer(&m_StreamState, io_state.MaxFrames(), buffer);
	AKASSERT(eResult == AK_DataReady || eResult == AK_NoMoreData || eResult == AK_NoDataReady || eResult == AK_Fail);
	MonitorCodecError(eResult);

	if (eResult == AK_Fail)
	{
		io_state.uValidFrames = 0;
		io_state.eState = eResult;
		io_state.result = eResult;
		return;
	}

	if (buffer.Buffer.uValidFrames == 0)
	{
		AKASSERT(eResult != AK_NoMoreData); // Codec ran out of samples before returning all samples in the file!
		io_state.uValidFrames = 0;
		io_state.result = eResult;
	}
	else
	{
		AKASSERT(buffer.Buffer.GetChannelConfig() == fmt.channelConfig);
		AKASSERT(buffer.uSrcFrames > 0);

		LeavePreBufferingState();

		io_state.AttachInterleavedData(buffer.Buffer.GetInterleavedData(), buffer.Buffer.MaxFrames(), buffer.Buffer.uValidFrames);

		// Setup pipeline buffer properly; we have some frames to send out
		io_state.posInfo.uSampleRate = m_pCtx->GetMediaFormat().uSampleRate;
		io_state.posInfo.uStartPos = m_Position.uCurSample;
		io_state.posInfo.uFileEnd = m_Position.uTotalSamples;

		AkUInt32 uMaxFrames = buffer.uSrcFrames;
		while (uMaxFrames > 0)
		{
			AkUInt32 uClamped = uMaxFrames;
			AK::SrcMedia::Position::ClampFrames(&m_Position, uClamped);

			// Markers and position info.
			SaveMarkersForBuffer(io_state, m_Position.uCurSample, uClamped);

			// Internal position
			bool bLoop = false;
			io_state.result = AK::SrcMedia::Position::Forward(&m_Position, uClamped, bLoop);

			uMaxFrames -= uClamped;
		}
	}
}

void CAkSrcMedia::ReleaseBuffer()
{
	m_pCodec->ReleaseBuffer(&m_StreamState);
}

void CAkSrcMedia::PrepareNextBuffer()
{
	if (!m_bVoiceVirtual && !m_bCodecVirtual)
	{
		AKASSERT(m_pCodec);
		// Asynchronous codecs can start decoding next buffer now.
		IAkSrcMediaCodec::Result eResult = PrepareCodecNextBuffer();
		// We don't report non-success to the pipeline here, just log it
		MonitorCodecError(eResult);
		if (eResult == AK_NoMoreData)
		{
			// This codec is not going to decode anything else.
			// This means the next source in the VPL can start decoding right away.
			SetPrepareNextSource();
		}
	}
}

AKRESULT CAkSrcMedia::TimeSkip(AkUInt32 & io_uFrames)
{
	AKRESULT eResult;

	AKASSERT(m_bVoiceVirtual);

	// Store current sample position for markers and io_uFrames adjustment.
	AkUInt32 uCurrSample = m_Position.uCurSample;

	AK::SrcMedia::Position::ClampFrames(&m_Position, io_uFrames);

	bool bLoop;
	eResult = AK::SrcMedia::Position::Forward(&m_Position, io_uFrames, bLoop);

#if AK_VOICE_BUFFER_POSITION_TRACKING
	char msg[256];
	sprintf(msg, "[p:%p] TimeSkip %i / %i(%i) \n", this, m_Position.uCurSample, m_Position.uTotalSamples, m_Position.uPCMLoopEnd);
	AKPLATFORM::OutputDebugMsg(msg);
#endif

	// Post markers
	m_Markers.TimeSkipMarkers(m_pCtx, uCurrSample, io_uFrames, m_Position.uTotalSamples);

	// Update position
	UpdatePositionInfo(uCurrSample, 1.0f);

	return eResult;
}

void CAkSrcMedia::VirtualOn(AkVirtualQueueBehavior eBehavior)
{
	// Note: Stopping the stream will likely let the stream manager flush all the buffers
	// that we don't currently own.
	m_StreamState.pStream->Stop();

	// In elapsed and beginning mode, release resources.
	// In resume mode, don't touch anything.
	if (eBehavior == AkVirtualQueueBehavior_FromElapsedTime
		|| eBehavior == AkVirtualQueueBehavior_FromBeginning)
	{
		if (m_StreamState.uSizeLeft != 0)
		{
			AK::SrcMedia::Stream::ReleaseStreamBuffer(&m_StreamState);
			m_StreamState.pNextAddress = NULL;
			m_StreamState.uSizeLeft = 0;
		}

		m_pCodec->VirtualOn();
		m_bCodecVirtual = true;
	}
	m_bVoiceVirtual = true;

	if (AK_EXPECT_FALSE(m_pCtx->GetRegisteredNotif() & AK_EnableGetSourceStreamBuffering))
	{
		AkBufferingInformation bufferingInfo;
		AK::SrcFileServices::GetBuffering(m_StreamState.pStream, m_StreamState.uSizeLeft, bufferingInfo);
		g_pPositionRepository->UpdateBufferingInfo(m_pCtx->GetPlayingID(), this, bufferingInfo);
	}
}

AKRESULT CAkSrcMedia::VirtualOff(AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset)
{
	m_bVoiceVirtual = false;
	if (eBehavior == AkVirtualQueueBehavior_Resume)
	{
		EnterPreBufferingState();
	}
	else
	{
		IAkSrcMediaCodec::Result eResult;
		AkUInt32 uSeekSample = 0;
		AkUInt16 uLoopCnt = 0;
		if (eBehavior == AkVirtualQueueBehavior_FromElapsedTime)  // Do not release/seek when last buffer.
		{
			// Set stream position now for next read.
			if (!in_bUseSourceOffset)
			{
				AKASSERT(m_Position.uCurSample < m_Position.uTotalSamples);
				uSeekSample = m_Position.uCurSample;
				uLoopCnt = m_Position.uLoopCnt;
			}
			else
			{
				GetSourceOffset(uSeekSample, uLoopCnt);
			}
		}
		else if (eBehavior == AkVirtualQueueBehavior_FromBeginning)
		{
			uSeekSample = 0;
			uLoopCnt = m_pCtx ? m_pCtx->GetLooping() : LOOPING_ONE_SHOT;
		}

		eResult = m_pCodec->VirtualOff();
		MonitorCodecError(eResult);
		if (eResult != AK_Success)
		{
			return eResult;
		}
		m_bCodecVirtual = false;

		eResult = SeekTo(uSeekSample, uLoopCnt);
		if (eResult != AK_Success)
		{
			// We failed to seek, most chances are we failed because we had no seek table
			// Default seeking at the beginning.
			eResult = SeekTo(0, uLoopCnt);
		}
		if (eResult != AK_Success)
			return eResult;
	}

	return m_StreamState.pStream->Start();
}

AkReal32 CAkSrcMedia::GetDuration() const
{
	return AK::SrcMedia::Duration(m_Position.uTotalSamples, m_uSourceSampleRate, m_Position.uPCMLoopStart, m_Position.uPCMLoopEnd, m_pCtx->GetLooping());
}

AKRESULT CAkSrcMedia::StopLooping()
{
	AKRESULT eResult = AK::SrcMedia::Stream::StopLooping(&m_StreamState);
	if (eResult != AK_Success)
		return eResult;

	if (AK::SrcMedia::Stream::NextFetchWillLoop(&m_StreamState))
	{
		// We missed the boat due to stream delays; we'll have to loop an extra time.
		m_Position.uLoopCnt = 2;
	}
	else
	{
		m_Position.uLoopCnt = 1;
	}
	m_pCodec->StopLooping(m_Position);

	return AK_Success;
}

AKRESULT CAkSrcMedia::Seek()
{
	AKRESULT eResult = CAkVPLSrcNode::Seek();
	// Ignore return code and do this anyway.
	UpdatePositionInfo(m_Position.uCurSample, 0.0f);
	return eResult;
}

AKRESULT CAkSrcMedia::ChangeSourcePosition()
{
	AKASSERT(m_pCtx->RequiresSourceSeek());

	// Compute the ideal PCM offset to seek to
	AkUInt32 uSourceOffset;
	AkUInt16 uLoopCnt;
	GetSourceOffset(uSourceOffset, uLoopCnt);

	return SeekTo(uSourceOffset, uLoopCnt);
}

AkReal32 CAkSrcMedia::GetPitch() const
{
	return ((m_uCodecAttributes & AK_CODEC_ATTR_PITCH) && !m_bVoiceVirtual) != 0 ? 0.0f : m_pCtx->GetEffectiveParams().Pitch();
}

AkReal32 CAkSrcMedia::GetAnalyzedEnvelope(AkUInt32 in_uBufferedFrames)
{
	// Note: in_uBufferedFrames may be larger than uCurSample after looping.
	const AkUInt32 uCurSample = (in_uBufferedFrames <= m_Position.uCurSample) ? (m_Position.uCurSample - in_uBufferedFrames) : 0;
	return AK::SrcMedia::EnvelopeAnalyzer::GetEnvelope(&m_Envelope, m_pAnalysisData, uCurSample);
}

AKRESULT CAkSrcMedia::RelocateMedia(AkUInt8 * in_pNewMedia, AkUInt8 * in_pOldMedia)
{
	return AK::SrcMedia::Stream::RelocateMedia(&m_StreamState, in_pNewMedia, in_pOldMedia);
}

AkChannelMappingFunc CAkSrcMedia::GetChannelMappingFunc()
{
	switch (m_eOrdering)
	{
	case AK::SrcMedia::ChannelOrdering_Pipeline:
		return AkPipelineChannelMappingFunc;
	case AK::SrcMedia::ChannelOrdering_Wave:
		return AkWaveChannelMappingFunc;
	case AK::SrcMedia::ChannelOrdering_Vorbis:
		return AkVorbisChannelMappingFunc;
	default:
		AKASSERT(m_eOrdering == AK::SrcMedia::ChannelOrdering_Auto);
		return nullptr;
	}
}

AKRESULT CAkSrcMedia::ParseHeader(AkUInt8 * in_pBuffer, AkUInt32 in_uBufferSize, AK::SrcMedia::Header & out_header)
{
	out_header.AnalysisData.uDataSize = 0;
	out_header.SeekInfo.uSeekChunkSize = 0;
	AKRESULT eResult = AkFileParser::Parse(
		in_pBuffer,              // Data buffer.
		in_uBufferSize,          // Buffer size.
		out_header.FormatInfo,       // WAVE format info.
		&m_Markers,                  // Markers.
		&out_header.uPCMLoopStart,   // Beginning of loop.
		&out_header.uPCMLoopEnd,     // End of loop (inclusive).
		&out_header.uDataSize,       // Data size.
		&out_header.uDataOffset,     // Offset to data.
		&out_header.AnalysisData,    // Analysis data.
		&out_header.SeekInfo);       // Seek table info

    if ( eResult != AK_Success )
    {
        MONITOR_SOURCE_ERROR( AkFileParser::ParseResultToMonitorMessage( eResult ), m_pCtx );
        return eResult;
    }

	m_bHeaderFromBank = m_StreamState.bIsMemoryStream || AK::SrcMedia::Stream::HasPrefetch(&m_StreamState);

	// Validate header
	if (out_header.uPCMLoopEnd > 0 && out_header.uPCMLoopEnd <= out_header.uPCMLoopStart)
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_InvalidAudioFileHeader, m_pCtx );
		return AK_InvalidFile;
	}

	// Store analysis data
	AK::SrcMedia::EnvelopeAnalyzer::Init(&m_Envelope);
	if (out_header.AnalysisData.uDataSize > 0)
	{
		if (m_bHeaderFromBank)
		{
			// Assign directly
			m_pAnalysisData = out_header.AnalysisData.pData;
		}
		else
		{
			m_pAnalysisData = (AkFileParser::AnalysisData*)AkAlloc(AkMemID_Processing, out_header.AnalysisData.uDataSize);
			if (!m_pAnalysisData)
				return AK_InsufficientMemory;

			AkMemCpy(m_pAnalysisData, out_header.AnalysisData.pData, out_header.AnalysisData.uDataSize);
		}
	}
	return AK_Success;
}

AKRESULT CAkSrcMedia::InitCodec(AkUInt8 * in_pBuffer, AkUInt32 in_uBufferSize, AK::SrcMedia::Header &io_header)
{
	IAkSrcMediaCodec::Result eResult;

	m_pCodec = m_pCodecFactory(&io_header);
	if (m_pCodec == nullptr)
		return AK_InsufficientMemory;

	AkUInt16 uLoopCnt = m_pCtx->GetLooping();
	AK::SrcMedia::CodecInfo codec{};
	m_StreamState.pStream->GetHeuristics(codec.heuristics);
	eResult = m_pCodec->Init(io_header, codec, uLoopCnt);
	MonitorCodecError(eResult);
	if (eResult != AK_Success)
	{
		return eResult;
	}

	// Validate codec info
	AkUInt32 uEndOfData = io_header.uDataOffset + io_header.uDataSize;
	if ( codec.uTotalSamples == 0 ||
		 codec.heuristics.uLoopStart > uEndOfData || 
		 codec.heuristics.uLoopEnd > uEndOfData ||
		 codec.heuristics.fThroughput == 0 ||
		 codec.uSourceSampleRate == 0 )
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_InvalidAudioFileHeader, m_pCtx );
		return AK_InvalidFile;
	}

	// Set the detected media format on the PBI
	m_pCtx->SetMediaFormat(codec.format);

	if (io_header.uPCMLoopEnd == 0)
	{
		io_header.uPCMLoopEnd = codec.uTotalSamples - 1;
	}

	// Update stream heuristics based on codec info
	m_StreamState.pStream->SetHeuristics(codec.heuristics);
	m_StreamState.pStream->SetMinimalBufferSize(codec.uMinimalBufferSize);
	AK::SrcMedia::Stream::UpdateFileSize(&m_StreamState, io_header.uDataOffset + io_header.uDataSize);
	AK::SrcMedia::Stream::UpdateLooping(&m_StreamState, codec.heuristics.uLoopStart, codec.heuristics.uLoopEnd, uLoopCnt);

	AK::SrcMedia::Position::Init(&m_Position, io_header, codec, uLoopCnt);
	m_uSourceSampleRate = codec.uSourceSampleRate;
	m_uDataOffset = io_header.uDataOffset;
	m_uCodecAttributes = codec.uAttributes;
	m_eOrdering = codec.eOrdering;

	return AK_Success;
}

void CAkSrcMedia::GetSourceOffset(AkUInt32 & out_uPosition, AkUInt16 & out_uLoopCnt) const
{
	// Get the seek position from PBI (absolute, with looping unwrapped).
	AkUInt32 uAbsoluteSourceOffset;
	bool bSnapToMarker;
	if (m_pCtx->IsSeekRelativeToDuration())
	{
		// Special case infinite looping.
		AkReal32 uSourceDuration = (m_pCtx->GetLooping() != LOOPING_INFINITE) ? GetDuration() : AK::SrcMedia::DurationNoLoop(m_Position.uTotalSamples, m_uSourceSampleRate);
		uAbsoluteSourceOffset = m_pCtx->GetSeekPosition(m_uSourceSampleRate, uSourceDuration, bSnapToMarker);
	}
	else
	{
		uAbsoluteSourceOffset = m_pCtx->GetSeekPosition(m_uSourceSampleRate, bSnapToMarker);
	}

	// Convert the absolute seeking value into relative value from the beginning
	// of the file, adjusting the loop count.
	AK::SrcMedia::AbsoluteToRelativeSourceOffset(
		uAbsoluteSourceOffset,
		m_Position.uPCMLoopStart,
		m_Position.uPCMLoopEnd,
		m_pCtx->GetLooping(),
		out_uPosition,
		out_uLoopCnt);

	// Snap value to marker if applicable.
	if (bSnapToMarker)
	{
		const AkAudioMarker * pMarker = m_Markers.GetClosestMarker(out_uPosition);
		if (pMarker)
		{
			out_uPosition = pMarker->dwPosition;
			// Now that we've found the real position, we need to pass it again through the conversion
			// in case the marker was over, or slightly after the loop point.
			AK::SrcMedia::AbsoluteToRelativeSourceOffset(
				out_uPosition,
				m_Position.uPCMLoopStart,
				m_Position.uPCMLoopEnd,
				out_uLoopCnt,
				out_uPosition,
				out_uLoopCnt);
		}
		else
		{
			// No marker: cannot snap to anything.
			MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_SeekNoMarker, m_pCtx);
		}
	}
}

void CAkSrcMedia::SaveMarkersForBuffer( 
	AkPipelineBuffer & io_buffer, 
	AkUInt32 in_ulBufferStartPos,
	AkUInt32 in_ulNumFrames
	)
{
	if ( m_pCtx )
	{
		CAkVPLSrcCbxNode * pCbx = m_pCtx->GetCbx();
		if (pCbx)
		{
			m_Markers.SaveMarkersForBuffer(
				pCbx->GetPendingMarkersQueue(),
				m_pCtx,
				io_buffer,
				in_ulBufferStartPos,
				in_ulNumFrames);
		}
	}
}

void CAkSrcMedia::UpdatePositionInfo(AkUInt32 uCurrSample, AkReal32 fLastRate)
{
	if (m_pCtx->GetRegisteredNotif() & AK_EnableGetSourcePlayPosition)
	{
		AkBufferPosInformation bufferPosInfo;

		bufferPosInfo.uSampleRate = m_pCtx->GetMediaFormat().uSampleRate;
		bufferPosInfo.uStartPos = uCurrSample;
		bufferPosInfo.uFileEnd = m_Position.uTotalSamples;
		bufferPosInfo.fLastRate = fLastRate;

		g_pPositionRepository->UpdatePositionInfo(m_pCtx->GetPlayingID(), &bufferPosInfo, this);
	}
}

AKRESULT CAkSrcMedia::SeekTo(AkUInt32 uSeekOffset, AkUInt16 uLoopCnt)
{
	if (uSeekOffset >= m_Position.uTotalSamples)
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_SeekAfterEof, m_pCtx );
		return AK_Fail;
	}

	AKRESULT eSeekResult;

	// Resolve the point in the media file most suitable for seeking to uSourceOffset
	IAkSrcMediaCodec::SeekInfo seek;
	eSeekResult = m_pCodec->FindClosestFileOffset(uSeekOffset, seek);
	if (eSeekResult != AK_Success)
		return AK_Fail;

	AKASSERT(uSeekOffset >= seek.uPCMOffset);

	AkUInt32 uRealOffset = m_uDataOffset + seek.uFileOffset;
	
	// Seek the file stream to the position determined by the codec
	eSeekResult = AK::SrcMedia::Stream::Seek(&m_StreamState, uRealOffset, uLoopCnt);
	if (eSeekResult != AK_Success && eSeekResult != AK_PartialSuccess)
		return eSeekResult;
		
	if (eSeekResult == AK_Success)
	{
		// Flush streamed data.
		AK::SrcMedia::Stream::ReleaseStreamBuffer(&m_StreamState);
		m_StreamState.uSizeLeft = 0;
		m_StreamState.pNextAddress = nullptr;
	}

	// Reset prebuffering status.
	EnterPreBufferingState();

	// Push difference between desired and actual sample position to PBI. The pitch node will consume it.
	m_pCtx->SetSourceOffsetRemainder(uSeekOffset - seek.uPCMOffset);

	// Update internal state
	m_Position.uCurSample = seek.uPCMOffset;
	m_Position.uLoopCnt = uLoopCnt;

	if (!m_bCodecVirtual)
	{
		IAkSrcMediaCodec::Result eCodecResult;
		// Notify codec that we will indeed seek
		AkUInt32 uFramesInRange = (uLoopCnt == LOOPING_ONE_SHOT ? m_Position.uTotalSamples : m_Position.uPCMLoopEnd + 1) - seek.uPCMOffset;
		eCodecResult = m_pCodec->Seek(&m_StreamState, seek, uFramesInRange, uLoopCnt);
		MonitorCodecError(eCodecResult);
		if (eCodecResult != AK_Success)
		{
			return AK_Fail;
		}

		PrepareNextBuffer();
	}

	return AK_Success;
}

AKRESULT CAkSrcMedia::PrepareCodecNextBuffer()
{
	IAkSrcMediaCodec::PitchInfo pitch;
	pitch.fPitch = m_pCtx->GetEffectiveParams().Pitch();
	pitch.eType = m_pCtx->GetPitchShiftType();
	return m_pCodec->PrepareNextBuffer(&m_StreamState, m_Position, pitch);
}
