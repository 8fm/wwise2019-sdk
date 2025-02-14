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
#include "AkSrcFileOpus.h"
#include "opusfile.h"
#include "opusfile/src/internal.h"
#include "OpusCommon.h"
#include "AkCommon.h"

#ifdef XAPU_WORKS
#include "GX/HWDecoder.h"
#endif

namespace AK
{
	// Deinterleave floating point samples (used in conjunction with some N-channel routines)
	void Opus_DeinterleaveAndRemap(AkReal32 * in_pSamples, AkPipelineBuffer* io_pIOBuffer)
	{
		AKASSERT(io_pIOBuffer->GetChannelConfig().eConfigType == AK_ChannelConfigType_Standard);


		const AkUInt32 uFramesToCopy = io_pIOBuffer->uValidFrames;
		const AkUInt32 uNumChannels = io_pIOBuffer->NumChannels();
		for (AkUInt32 i = 0; i < uNumChannels; ++i)
		{
			AkReal32 * AK_RESTRICT pIn = in_pSamples + i;
			AkReal32 * AK_RESTRICT pOut = (AkReal32 * AK_RESTRICT)(io_pIOBuffer->GetChannel(AkVorbisChannelMappingFunc(io_pIOBuffer->GetChannelConfig(), i)));
			for (AkUInt32 j = 0; j < uFramesToCopy; ++j)
			{
				*pOut++ = *pIn;
				pIn += uNumChannels;
			}
		}
	}

	void Opus_RemapAndConvertInt16(AkReal32 * in_pSamples, AkAudioBuffer * io_pOutBuf)
	{
		AkUInt16 *pBuf = (AkUInt16*)io_pOutBuf->GetInterleavedData();
		AkUInt32 uNumFrames = io_pOutBuf->uValidFrames;
		AkUInt32 uNumChannels = io_pOutBuf->NumChannels();
		for (AkUInt32 i = 0; i < uNumFrames; i ++)
		{
			for (AkUInt8 y = 0; y < uNumChannels; y++)
			{
				AkUInt32 di = i * uNumChannels + AkVorbisChannelMappingFunc(io_pOutBuf->GetChannelConfig(), y);
				AkUInt32 si = i * uNumChannels + y;
				pBuf[di] = FLOAT_TO_INT16(in_pSamples[si]);
			}
		}
	}

	void Opus_ConvertInt16(AkReal32 * in_pSamples, AkAudioBuffer * io_pOutBuf)
	{
		AkUInt16 *pBuf = (AkUInt16*)io_pOutBuf->GetInterleavedData();
		AkUInt32 uNumFrames = io_pOutBuf->uValidFrames;
		AkUInt32 uNumChannels = io_pOutBuf->NumChannels();
		for (AkUInt32 i = 0; i < uNumFrames * uNumChannels; i++)
		{
			pBuf[i] = FLOAT_TO_INT16(in_pSamples[i]);
		}
	}

	void Opus_DeinterleaveStd(AkReal32 * in_pSamples, AkPipelineBuffer* io_pIOBuffer)
	{
		const AkChannelConfig uChannelConfig = io_pIOBuffer->GetChannelConfig();
		AKASSERT(uChannelConfig.eConfigType == AK_ChannelConfigType_Standard);

		const AkUInt32 uFramesToCopy = io_pIOBuffer->uValidFrames;
		const AkUInt32 uNumChannels = io_pIOBuffer->NumChannels();
		for (AkUInt32 i = 0; i < uNumChannels; ++i)
		{
			AkReal32 * AK_RESTRICT pIn = in_pSamples + i;
			AkReal32 * AK_RESTRICT pOut = (AkReal32 * AK_RESTRICT)(io_pIOBuffer->GetChannel(AkAudioBuffer::StandardToPipelineIndex(uChannelConfig, i)));
			for (AkUInt32 j = 0; j < uFramesToCopy; ++j)
			{
				*pOut++ = *pIn;
				pIn += uNumChannels;
			}
		}
	}

	void Opus_Deinterleave(AkReal32 * in_pSamples, AkPipelineBuffer* io_pIOBuffer)
	{
		const AkUInt32 uFramesToCopy = io_pIOBuffer->uValidFrames;
		const AkUInt32 uNumChannels = io_pIOBuffer->NumChannels();
		for (AkUInt32 i = 0; i < uNumChannels; ++i)
		{
			AkReal32 * AK_RESTRICT pIn = in_pSamples + i;
			AkReal32 * AK_RESTRICT pOut = (AkReal32 * AK_RESTRICT)(io_pIOBuffer->GetChannel(i));
			for (AkUInt32 j = 0; j < uFramesToCopy; ++j)
			{
				*pOut++ = *pIn;
				pIn += uNumChannels;
			}
		}
	}
}

//Disable seeking for now.  When enabled, we must support the fact that the seek table is at the end of the file.
const OpusFileCallbacks CAkSrcFileOpus::c_OpusFileCB= { read_opus, NULL/*seek_opus*/, tell_opus, close_opus };

CAkSrcFileOpus::CAkSrcFileOpus(CAkPBI * in_pCtx)
	:CAkSrcFileBase(in_pCtx)
	, m_pOpusFile(NULL)
	, m_uSeekedSample(SEEK_UNINITIALIZED)
	, m_uOpusFileStart(0)
	, m_bVirtual(true)
	, m_pOutputBuffer(NULL)
	, m_uOutputBufferSize(0)
{
}

CAkSrcFileOpus::~CAkSrcFileOpus()
{
	if(m_pOpusFile)
	{
		op_free(m_pOpusFile);
		m_pOpusFile = NULL;
	}

	AKASSERT(!m_pOutputBuffer);
}

void CAkSrcFileOpus::GetBuffer(AkVPLState & io_state)
{
	// Check prebuffering status.
	AKRESULT eResult = IsPrebufferingReady();
	if(AK_EXPECT_FALSE(eResult != AK_DataReady))
	{
		io_state.result = eResult;
		return;
	}	

	//If we didn't manage to complete the seek in the SeekStream call because of missing data, retry here.
	if(m_uSeekedSample != SEEK_UNINITIALIZED)
	{
		io_state.result = SeekHelper();		
		if(m_ulSizeLeft == 0 || io_state.result == AK_NoDataReady)
		{
			io_state.result = AK_NoDataReady;
			return;
		}
	}

retryDecode:
	float * pInterleavedSamples = NULL;
	int iPacketDuration = 0;
	int samplesRead = AK_op_read_float_no_copy(m_pOpusFile, &pInterleavedSamples, &iPacketDuration);

	if (samplesRead == OP_EFAULT)
	{
		io_state.result = AK_Fail;
		return;
	}
	
	if(samplesRead == OP_EREAD)
	{
		AKASSERT(m_ulSizeLeft == 0);

		if(m_pOpusFile->os.e_o_s)
		{
			m_bIsLastStmBuffer = true;
			io_state.uValidFrames = 0;
			io_state.result = AK_NoMoreData;
			return;
		}
		// See if we can get more data from stream manager.
		ReleaseStreamBuffer();
		AKRESULT eStmResult = FetchStreamBuffer();
		if(eStmResult != AK_DataReady)
		{
			io_state.result = eStmResult;
			return;
		}
		goto retryDecode;
	}
	else if(samplesRead == OP_HOLE)
	{		
		//This should not happen with our files. 
		//However it isn't a fatal error.  The decoder can recover from the hole and continue on the next packet.  
		//Much better than killing the sound.
		AKASSERT(!"Opus Hole detected.");
		MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_OpusDecodeError, m_pCtx);
		goto retryDecode;
	}
	else if (samplesRead < 0)
	{
		//Internal link numbers don't match (link number is defined in the header)
		//Can happen if the header is in prefetch area.
		if(samplesRead == OP_EBADLINK)
		{
			MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_InvalidAudioFileHeader, m_pCtx);
		}
		else
		{
			MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_OpusDecodeError, m_pCtx);
		}
		io_state.result = AK_Fail;
		return;
	}
	else if(samplesRead == 0)
	{ 			
		m_bIsLastStmBuffer = true;
		io_state.uValidFrames = 0;
		io_state.result = AK_NoMoreData;
		return;
	}		
	
	io_state.result = AK_DataReady;
	if (io_state.NumChannels() > 1)
	{
		AkUInt32 uOutputBufferSize = io_state.NumChannels() * iPacketDuration * sizeof(AkReal32);
		if (AK_EXPECT_FALSE(!m_pOutputBuffer || uOutputBufferSize<m_uOutputBufferSize))
		{
			if (m_pOutputBuffer)
				AkFalign(AkMemID_Processing, m_pOutputBuffer);

			m_uOutputBufferSize = uOutputBufferSize;
			m_pOutputBuffer = (AkReal32 *)AkMalign(AkMemID_Processing, m_uOutputBufferSize, AK_BUFFER_ALIGNMENT);
			if (AK_EXPECT_FALSE(!m_pOutputBuffer))
			{
				io_state.result = AK_Fail;
				return;
			}
		}

		io_state.AttachInterleavedData(m_pOutputBuffer, iPacketDuration, samplesRead, io_state.GetChannelConfig());
		if (m_pOpusFile->links[0].head.mapping_family == 1)
			Opus_DeinterleaveAndRemap(pInterleavedSamples, &io_state);
		else if (io_state.GetChannelConfig().eConfigType == AK_ChannelConfigType_Standard)
			Opus_DeinterleaveStd(pInterleavedSamples, &io_state);
		else
			Opus_Deinterleave(pInterleavedSamples, &io_state);
	}
		else
	{
		io_state.AttachInterleavedData(pInterleavedSamples, iPacketDuration, samplesRead, io_state.GetChannelConfig());
	}
		
	if(DoLoop() && samplesRead + m_uCurSample >= m_uPCMLoopEnd)
	{
		samplesRead = m_uPCMLoopEnd - m_uCurSample + 1;
	}

	SubmitBufferAndUpdate(
		(float*)io_state.GetContiguousDeinterleavedData(),
		samplesRead,
		OPUS_RATE,
		io_state.GetChannelConfig(),
		io_state);

	io_state.AttachInterleavedData(io_state.GetContiguousDeinterleavedData(), iPacketDuration, samplesRead, io_state.GetChannelConfig());
}

void CAkSrcFileOpus::ReleaseBuffer()
{
	// Preserve output buffer allocation to avoid allocator churn
	// FreeOutputBuffer();
}

void CAkSrcFileOpus::StopStream()
{
#ifdef XAPU_WORKS
	m_pHWContext->Disconnect();
#endif
	if(m_pOpusFile)
	{
		op_free(m_pOpusFile);
		m_pOpusFile = NULL;
	}

	FreeOutputBuffer();

	CAkSrcFileBase::StopStream();
}

AKRESULT CAkSrcFileOpus::StartStream(AkUInt8 * in_pBuffer, AkUInt32 ulBufferSize)
{	
	AKRESULT eResult = AK_Success;
	if(m_bFormatReady)
	{
		// Check streaming status if header has already been parsed.
		return IsInitialPrebufferingReady();
	}	
	
	if(!m_pStream)
	{
		// Specify default buffer constraints.By default, no constraint is necessary.But it is obviously harder
		// for any stream manager implementation to increase the minimum buffer size at run time than to decrease
		// it, so we set it to a "worst case" value.
		AkAutoStmBufSettings bufSettings;
		bufSettings.uBufferSize = 0;		// No constraint.
		bufSettings.uMinBufferSize = AK_WORST_CASE_MIN_STREAM_BUFFER_SIZE;
		bufSettings.uBlockSize = 0;

		eResult = CreateStream(bufSettings, 0);
		if(AK_EXPECT_FALSE(eResult != AK_Success))
			return eResult;

		// Get data from bank if stream sound is prefetched.
		// If it is, open stream paused, set position to end of prefetched data, then resume.		
		if(m_pCtx->IsPrefetched())
		{
			m_pCtx->GetDataPtr(m_pNextAddress, m_ulSizeLeft);
			m_bIsReadingPrefecth = (m_pNextAddress != NULL && m_ulSizeLeft != 0);
			m_bSkipBufferRelease = m_bIsReadingPrefecth;
		}
	}
	
	if(AK_Success == eResult && !m_bIsReadingPrefecth)
	{
		// Start IO now, if we don't have prefetch
		eResult = m_pStream->Start();
		if(AK_EXPECT_FALSE(eResult != AK_Success))
			return eResult;
	}

	eResult = _ProcessFirstBuffer();	
	if(eResult == AK_Success && m_bIsReadingPrefecth)
		eResult = m_pStream->Start();	

	if(eResult == AK_Success)
		eResult = IsInitialPrebufferingReady();

	return eResult;
}


// Process newly acquired buffer from stream: 
// Update offset in file, 
// Reset pointers for client GetBuffer(),
// Deal with looping: update count, file position, SetStreamPosition() to loop start.
// Sets bLastStmBuffer flag.
AKRESULT CAkSrcFileOpus::ProcessStreamBuffer(AkUInt8 * in_pBuffer, bool in_bIsReadingPrefecth)
{
	AKASSERT(m_pStream);

	// Update offset in file.
	
	m_uCurFileOffset = m_ulFileOffset;
	m_ulFileOffset += m_ulSizeLeft;

	// Set next pointer.
	AKASSERT(in_pBuffer != NULL);
	m_pNextAddress = in_pBuffer;
	m_bIsReadingPrefecth = in_bIsReadingPrefecth;

	// "Consume" correction due to seeking on block boundary. This will move m_pNextAddress, m_uCurFileOffset and m_ulSizeLeft in their respective directions.
	ConsumeData(m_uiCorrection);

	// Will we hit the end?
	if(m_ulFileOffset >= m_uDataOffset + m_uDataSize)
	{	
		m_bIsLastStmBuffer = true;	
	}
	else
	{
		// If it will not loop inside that buffer, reset the offset.
		m_uiCorrection = 0;
	}
	return AK_Success;
}

// Overriden from base class because Opus decoder buffers a lot of data, spanning more than one buffer.
AKRESULT CAkSrcFileOpus::_ProcessFirstBuffer()
{
	EnterPreBufferingState();

	AKRESULT eResult = AK_FormatNotReady;	
	m_bIsLastStmBuffer = false;
	if(!m_bIsReadingPrefecth)
	{
		do
		{			
			eResult = m_pStream->GetBuffer( (void*&)m_pNextAddress, m_ulSizeLeft, AK_PERF_OFFLINE_RENDERING);
			if(eResult == AK_NoDataReady)
			{
				// Not ready. Leave.
				return AK_FormatNotReady;
			}
			else if(eResult != AK_DataReady && eResult != AK_NoMoreData)
			{
				// IO error.
				return AK_Fail;
			}
			AKASSERT(m_pNextAddress);
			eResult = ParseHeader(m_pNextAddress);		
			// If the header consumes the entire streaming buffer we could have m_pNextAddress == NULL.
			if (m_pNextAddress)
			{
				ProcessStreamBuffer(m_pNextAddress, false);
			}
		} while(eResult == AK_FormatNotReady);
	}
	else
	{
		// If we read from prefetch, just parse, it is all in memory
		eResult = ParseHeader(m_pNextAddress);
	}

	if(eResult != AK_Success)
		return eResult;

	AKASSERT(m_uTotalSamples && m_uDataSize);
	if(m_pOpusFile)
	{
		//Tell the decoder it can seek.  We open without seek to avoid seeking at the end, which it does naturally.
		m_pOpusFile->callbacks.seek = seek_opus;
		m_pOpusFile->seekable = 1;
	}	
	m_bVirtual = false;

	if(m_pCtx->RequiresSourceSeek() && GetSourceOffset() != m_uCurSample)
	{		
		eResult = SeekToSourceOffset();
	}
	else if(m_bIsReadingPrefecth)
	{
		//Seek to the end of the prefetch
		SetStreamPosition(m_uCurFileOffset);		
	}

	m_pStream->SetMinimalBufferSize(1);

	m_bFormatReady = true;
	return eResult;
}

void CAkSrcFileOpus::FreeOutputBuffer()
{
	if (m_pOutputBuffer)
	{
		AkFalign(AkMemID_Processing, m_pOutputBuffer);
		m_pOutputBuffer = NULL;
		m_uOutputBufferSize = 0;
	}
}

AKRESULT CAkSrcFileOpus::ParseHeader(AkUInt8 * in_pBuffer)
{	
	int opusErr = 0;	
	if(m_pOpusFile)
	{
		//We streamed part of the header+first buffers before. Stream the new buffer in the opus decoder.
		m_pNextAddress = in_pBuffer;
		int opusErr = AK_continue_open(m_pOpusFile);
		if(opusErr != 0)
		{
			if(opusErr == OP_EREAD)
				return AK_FormatNotReady;

			m_pOpusFile = NULL;
			MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_UnkownOpusError, m_pCtx);
			return AK_Fail;
		}
		
		return AK_Success;
	}

	AkFileParser::FormatInfo fmtInfo;
	AkFileParser::AnalysisDataChunk analysisDataChunk;
	AKRESULT eResult = AkFileParser::Parse(
		in_pBuffer,			// Data buffer.
		m_ulSizeLeft,		// Buffer size.
		fmtInfo,			// Returned audio format info.
		&m_markers,			// Markers.
		&m_uPCMLoopStart,	// Beginning of loop.
		&m_uPCMLoopEnd,		// End of loop (inclusive).
		&m_uDataSize,		// Data size.
		&m_uDataOffset,		// Offset to data.
		&analysisDataChunk,	// Analysis data.
		NULL);				// (Format-specific) seek table info (pass NULL is not expected).

	if(eResult != AK_Success)
	{
		MONITOR_SOURCE_ERROR(AkFileParser::ParseResultToMonitorMessage(eResult), m_pCtx);
		return eResult;
	}

	//Check if the file is of the expected format
	if(fmtInfo.pFormat->wFormatTag != AK_WAVE_FORMAT_OPUS)
	{
		MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_FileFormatMismatch, m_pCtx);
		return AK_InvalidFile;
	}

	// Opus expects a WaveFormatOpus
	WaveFormatOpus * pFmt = (WaveFormatOpus *)fmtInfo.pFormat;
	AKASSERT(fmtInfo.uFormatSize == sizeof(WaveFormatOpus));

	//Setup format on the PBI
	AkChannelConfig channelConfig = pFmt->GetChannelConfig();
	AkAudioFormat format;

	format.SetAll(
		OPUS_RATE,	//Yes, Opus is hardcoded to 48k output, as per documentation
		channelConfig,
		32,
		pFmt->nChannels * sizeof(AkReal32),
		AK_FLOAT,
		AK_NONINTERLEAVED);

	m_pCtx->SetMediaFormat(format);

	// Store analysis data if it was present. 
	// Note: StoreAnalysisData may fail, but it is not critical.
	if(analysisDataChunk.uDataSize > 0)
		StoreAnalysisData(analysisDataChunk);

	m_uTotalSamples = pFmt->dwTotalPCMFrames;	

	// Fix loop start and loop end for no SMPL chunk
	if((m_uPCMLoopStart == 0) && (m_uPCMLoopEnd == 0))
	{
		m_uPCMLoopEnd = m_uTotalSamples - 1;
	}

	m_uOpusFileStart = m_uDataOffset;

	//Cache these variables, they are in the stream buffer, which may get cleared.
	opus_int64 end_offset = pFmt->dwEndGranuleOffset;
	opus_int64 pcm_end = pFmt->dwEndGranulePCM;
	AkReal32 fThroughput = fmtInfo.pFormat->nAvgBytesPerSec/1000.f;	

	//Advance the read pointer to the first byte of the Opus file
	m_pNextAddress = in_pBuffer + m_uDataOffset;
	m_uCurFileOffset = m_uDataOffset;
	m_ulSizeLeft -= m_uDataOffset;

	m_pOpusFile = AK_op_open_callbacks(this, &c_OpusFileCB, NULL, 0, &opusErr);
	if (m_pOpusFile)
	{
		//Keep information to help seeking.  The current algo in opusfile does a bissection on the file 
		//but starts by seeking at the end to get these two variables.  We avoid that seek as it is very disturbing to the Stream Mgr.
		m_pOpusFile->links[0].end_offset = end_offset;
		m_pOpusFile->links[0].pcm_end = pcm_end;
		m_pOpusFile->end = m_uDataSize;

		AkAutoStmHeuristics heuristics;
		m_pStream->GetHeuristics(heuristics);
		GetStreamLoopHeuristic(DoLoop(), heuristics);
		heuristics.priority = m_pCtx->GetPriority();	// Priority.
		heuristics.fThroughput = fThroughput;
		m_pStream->SetMinTargetBufferSize(65536); // #define OP_CHUNK_SIZE     (65536) WG-39680
		m_pStream->SetHeuristics(heuristics);
		m_pStream->SetMinimalBufferSize(1);

#ifdef XAPU_WORKS
		if (m_pHWContext->Connect(pFmt->nChannels) != AK_Success)
		{
			//MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_UnkownOpusError, m_pCtx);
			return AK_Fail;
		}
				
		op_set_decode_callback(m_pOpusFile, HWWorkContext::Decode, m_pHWContext);
#endif

	}
	if(opusErr == OP_EREAD)	
		return AK_FormatNotReady;
				
	switch(opusErr)
	{
	case 0: break;	//Success;
	case OP_ENOTFORMAT: MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_FileFormatMismatch, m_pCtx); eResult = AK_InvalidFile; break;
	case OP_EBADHEADER: MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_InvalidAudioFileHeader, m_pCtx); eResult = AK_InvalidFile; break;
	default: MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_UnkownOpusError, m_pCtx); eResult = AK_Fail; break;
	}	

	if(m_pOpusFile == NULL)
	{
		MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_UnkownOpusError, m_pCtx);
		return AK_Fail;
	}

	return eResult;
}

AKRESULT CAkSrcFileOpus::FindClosestFileOffset(
	AkUInt32 in_uDesiredSample,		// Desired sample position in file.
	AkUInt32 & out_uSeekedSample,	// Returned sample where file position was set.
	AkUInt32 & out_uFileOffset		// Returned file offset where file position was set.
	)
{
	return AK_NotImplemented;
}

void CAkSrcFileOpus::VirtualOn(AkVirtualQueueBehavior eBehavior)
{
	CAkSrcFileBase::VirtualOn(eBehavior);

	// In elapsed and beginning mode, release our streaming buffer.
	// In resume mode, don't touch anything.
	if(eBehavior == AkVirtualQueueBehavior_FromElapsedTime || eBehavior == AkVirtualQueueBehavior_FromBeginning)
	{
		op_decode_clear(m_pOpusFile);		
		ogg_sync_clear(&m_pOpusFile->oy);
		FreeOutputBuffer();
	}
	m_bVirtual = true;
}	

AKRESULT CAkSrcFileOpus::VirtualOff(AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset)
{
	m_bVirtual = false;
	AKRESULT res = CAkSrcFileBase::VirtualOff(eBehavior, in_bUseSourceOffset);	
	m_bVirtual = res != AK_Success;	//If error, we're still virtual.
	return res;
}

AKRESULT CAkSrcFileOpus::OnLoopComplete(bool in_bEndOfFile /* True if this was the end of file, false otherwise. */)
{	
	if(DoLoop() && !in_bEndOfFile)
	{
		m_uSeekedSample = m_uPCMLoopStart;
		SeekHelper();
	}
	return CAkSrcBaseEx::OnLoopComplete(in_bEndOfFile);
}

AKRESULT CAkSrcFileOpus::SeekStream( AkUInt32 in_uDesiredSample, AkUInt32 & out_uSeekedSample )
{	
	out_uSeekedSample = AkMin(in_uDesiredSample, m_uTotalSamples);
	if(in_uDesiredSample >= m_uTotalSamples)
		return AK_NoMoreData;	
		
	m_uSeekedSample = out_uSeekedSample;
		
	// Reset stream-side loop count.
	m_uStreamLoopCnt = GetLoopCnt();

	if(m_bVirtual)
		return AK_Success;

	//Opus needs to read from stream to seek.  So the stream must be started at this point.
	m_pStream->Start();

	AKRESULT res = SeekHelper();
	if(res == AK_NoDataReady)
		return AK_Success;
			
	return res;
}

AKRESULT CAkSrcFileOpus::SeekHelper()
{
	AKRESULT res = AK_DataReady;
	while(res == AK_DataReady)
	{
		int err = op_pcm_seek(m_pOpusFile, m_uSeekedSample);
		if(err == OP_EREAD)
		{
			m_bIsLastStmBuffer = false;
			ReleaseStreamBuffer();
			res = FetchStreamBuffer();
		}
		else
		{
			if(err != 0)
			{
				//Internal link numbers don't match (link number is defined in the header)
				//Can happen if the header is in prefetch area.
				if(err == OP_EBADLINK)
				{
					MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_InvalidAudioFileHeader, m_pCtx);
				}
				else
				{
					MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_UnkownOpusError, m_pCtx);
				}
				return AK_Fail;					
			}
			res = AK_Success;
			m_uSeekedSample = SEEK_UNINITIALIZED;
		}
	}
	return res;
}

AKRESULT CAkSrcFileOpus::ChangeSourcePosition()
{
	//Contrary to base class, do not release the buffer after seeking.  Opus reader will have buffered part of the audio data at that point.
	return SeekToSourceOffset();
}

int CAkSrcFileOpus::read_opus(void *_stream, unsigned char *_ptr, int _nbytes)
{
	CAkSrcFileOpus* pThis = (CAkSrcFileOpus*)_stream;
	return pThis->Read(_ptr, _nbytes);
}


int CAkSrcFileOpus::Read(unsigned char *_ptr, int _nbytes)
{
	if(m_ulSizeLeft == 0)
	{
		ReleaseStreamBuffer();		
		if(HasNoMoreStreamData() || FetchStreamBuffer() != AK_DataReady)
			return OP_EREAD;				
	}

	AKASSERT(m_pNextAddress);
	if(m_pNextAddress == NULL)
		return OP_EREAD;

	int actualBytes = AkMin((AkUInt32)_nbytes, m_ulSizeLeft);
	memcpy(_ptr, m_pNextAddress, actualBytes);
	CAkSrcFileBase::ConsumeData(actualBytes);		
	
	return actualBytes;
}

int CAkSrcFileOpus::seek_opus(void *_stream, opus_int64 _offset, int _whence)
{
	CAkSrcFileOpus* pThis = (CAkSrcFileOpus*)_stream;
	if(_offset == 0 && _whence == SEEK_CUR)
		return pThis->m_uCurFileOffset;

	return pThis->SeekOpus(_offset, _whence);
}

int CAkSrcFileOpus::SeekOpus(opus_int64 _offset, int _whence)
{
	AKASSERT(SEEK_SET == AK_MoveBegin);
	AKASSERT(SEEK_CUR == AK_MoveCurrent);
	AKASSERT(SEEK_END == AK_MoveEnd);	

	//Check if we are seeking in the current memory block.
	AkInt64 realOffset = 0;
	if(_whence == SEEK_CUR && _offset > 0)
	{
		if( _offset < m_ulSizeLeft)
		{
			ConsumeData((AkUInt32)_offset);
			return 0;
		}		
	}
	else if(_whence == SEEK_SET && _offset > 0)
	{		
		_offset += m_uOpusFileStart;//Correct the file offset to be relative to the beginning of the opus file

		//If we have prefetch, try to seek in it.
		if (m_pCtx->IsPrefetched())
		{
			AkUInt8 *pData;
			AkUInt32 uSize;
			m_pCtx->GetDataPtr(pData, uSize);
			if (pData && _offset < uSize && (m_pNextAddress < pData || m_pNextAddress >= pData + uSize))
			{
				if (m_pStream->SetPosition(uSize, (AkMoveMethod)SEEK_SET, &realOffset) == AK_Success)
				{
					ReleaseStreamBuffer();
					m_pNextAddress = pData;
					m_ulSizeLeft = uSize;
					m_ulFileOffset = uSize;
					m_uCurFileOffset = 0;
					m_bIsReadingPrefecth = true;
					m_bSkipBufferRelease = true;
					m_bIsLastStmBuffer = false;
				}
			}
		}

		if(_offset >= m_uCurFileOffset && _offset < m_uCurFileOffset + m_ulSizeLeft)
		{
			ConsumeData((AkUInt32)_offset - m_uCurFileOffset);
			return 0;
		}
	}
	else if(_whence == SEEK_END)
	{
		AkUInt32 uFileSize = m_uDataSize + m_uDataOffset;
		AkUInt32 uAbsolute = uFileSize - (AkUInt32)_offset;
		if(uAbsolute >= m_uCurFileOffset && uAbsolute < m_uCurFileOffset + m_ulSizeLeft)
		{
			ConsumeData(uAbsolute - m_uCurFileOffset);
			return 0;
		}		
	}
		
	AKRESULT eResult = m_pStream->SetPosition(_offset, (AkMoveMethod)_whence, &realOffset);
	if(eResult == AK_Success)
	{
		// Keep track of offset caused by unbuffered IO constraints.
		// Set file offset to true value.
		if(_whence == SEEK_SET)
		{
			m_uiCorrection = (AkUInt32)_offset - (AkUInt32)realOffset;
			m_ulFileOffset = (AkUInt32)realOffset;
		}
		else if(_whence == SEEK_END)
		{
			m_uiCorrection = 0;			
			m_ulFileOffset = m_uDataSize+m_uDataOffset - (AkUInt32)realOffset;
		}
		else if(_whence == SEEK_CUR)
		{
			m_uiCorrection = m_ulFileOffset + (AkUInt32)_offset - (AkUInt32)realOffset;
			m_ulFileOffset = m_ulFileOffset + (AkUInt32)realOffset;
		}

		ResetStreamingAfterSeek();

		ReleaseStreamBuffer();
		m_ulSizeLeft = 0;
		m_pNextAddress = NULL;
		return 0;
	}
	return -1;
}

opus_int64 CAkSrcFileOpus::tell_opus(void *_stream)
{
	CAkSrcFileOpus* pThis = (CAkSrcFileOpus*)_stream;
	return pThis->m_ulFileOffset + pThis->m_ulSizeLeft;//pThis->m_pStream->GetPosition(NULL);
}

int CAkSrcFileOpus::close_opus(void *_stream)
{
	//Nothing to do, we'll close our own stream internally.
	return 0;
}

AKRESULT CAkOpusFileCodec::DecodeFile(AkUInt8 * pDst, AkUInt32 uDstSize, AkUInt8 * pSrc, AkUInt32 uSrcSize, AkUInt32 &out_uSizeWritten)
{
	// Decode the whole buffer in one shot
	AKRESULT eResult;
	AkUInt32 uPCMLoopStart, uPCMLoopEnd, uDataSize, uDataOffset;
	AkFileParser::FormatInfo fmtInfo;
	AkFileParser::AnalysisDataChunk analysisDataChunk;

	out_uSizeWritten = 0;
	eResult = AkFileParser::Parse(
		pSrc,
		uSrcSize,
		fmtInfo,
		NULL,
		&uPCMLoopStart,
		&uPCMLoopEnd,
		&uDataSize,
		&uDataOffset,
		&analysisDataChunk,
		NULL);
	if (eResult != AK_Success)
	{
		return eResult;
	}
	if(fmtInfo.pFormat->wFormatTag != AK_WAVE_FORMAT_OPUS)
	{
		return AK_InvalidFile;
	}
	int opusErr = 0;
	OggOpusFile * opusFile = op_open_memory((const unsigned char *)pSrc + uDataOffset, uSrcSize - uDataOffset, &opusErr);
	switch (opusErr)
	{
	case 0:
		break;
	case OP_ENOTFORMAT:
		return AK_InvalidFile;
	case OP_EBADHEADER:
		return AK_InvalidFile;
	default:
		return AK_Fail;
	}

	AkUInt8 uNumChannels = fmtInfo.pFormat->nChannels;
	AkChannelConfig channelConfig = fmtInfo.pFormat->GetChannelConfig();
	AkUInt16 * pBuf = (AkUInt16*)pDst;
	AkUInt16 * pBufEnd = pBuf + (uDstSize / 2); // Counted as units of int16 samples
	bool bContinue = true;
	while (bContinue)
	{
		float * pInterleavedSamples;
		int iPacketDuration;

		int samplesRead = AK_op_read_float_no_copy(opusFile, &pInterleavedSamples, &iPacketDuration);
		if (samplesRead == 0)
		{
			// We are done.
			eResult = AK_Success;
			bContinue = false;
		}
		else if (samplesRead < 0)
		{
			eResult = AK_Fail;
			bContinue = false;
		}
		else
		{
			AkUInt32 progress = samplesRead * uNumChannels;
			AKASSERT(pBuf + progress <= pBufEnd);
			AkAudioBuffer outBuffer;
			outBuffer.AttachInterleavedData(pBuf, samplesRead, samplesRead, channelConfig);
			if (uNumChannels > 1)
			{
				AK::Opus_RemapAndConvertInt16(pInterleavedSamples, &outBuffer);
			}
			else
			{
				AK::Opus_ConvertInt16(pInterleavedSamples, &outBuffer);
			}
			pBuf += progress;
			out_uSizeWritten += progress * sizeof(AkUInt16);
		}
	}
	op_free(opusFile);
	return eResult;
}
