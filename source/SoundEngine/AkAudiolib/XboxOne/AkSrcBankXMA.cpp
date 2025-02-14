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

#include "AkSrcBankXMA.h"
#include "AkFileParserBase.h"
#include "AudiolibDefs.h"
#include "AkMonitor.h"
#include <stdio.h>
#include "AkPlayingMgr.h"
#include "AkLEngine.h"
#include "AkProfile.h"

IAkSoftwareCodec* CreateXMABankPlugin(void* in_pCtx)
{
	return AkNew(AkMemID_Processing, CAkSrcBankXMA((CAkPBI*)in_pCtx));
}


CAkSrcBankXMA::CAkSrcBankXMA( CAkPBI * in_pCtx )
	: CAkSrcXMABase<CAkSrcBaseEx>( in_pCtx )
{
}

CAkSrcBankXMA::~CAkSrcBankXMA()
{
}

void CAkSrcBankXMA::StopStream()
{
	//StopStream() is called before deleting this object, so we need to make sure DecodingFinished() is called before doing so.
	//	That is accomplished by WaitForHwVoices().
	FreeHardwareVoices();
	WaitForHardwareVoices();
	CAkSrcBaseEx::StopStream();
}

void CAkSrcBankXMA::GetBuffer( AkVPLState & io_state )
{
	AkACPMgr::Get().WaitForFlowgraphs();

	// Update source if required (once per Wwise frame).
	if ( UpdateSource( io_state ) == AK_Fail )
	{
		io_state.result = AK_Fail;
		return;
	}
	
	// Handle starvation.
	if ( m_uNumShapeFramesAvailable == 0 )
	{
		// The pipeline is trying to pull too much data at a time.
		io_state.result = AK_NoDataReady;
		return;
	}

	AKASSERT( GetShapeDmaFramesAvailable( m_pHwVoice[0]->pDmaContext[0] ) > 0 );

	const AkAudioFormat &rFormat = m_pCtx->GetMediaFormat();

	// Adjust number of samples produced for the pipeline (includes SRC).
	AKASSERT(m_uNumShapeFramesAvailable > 0);
	AKASSERT(m_uNumShapeFramesGranted == 0);
	m_uNumShapeFramesGranted = 1;
	AkUInt16 uNumSamplesRefill = (AkUInt16)( m_uNumShapeFramesGranted * SHAPE_FRAME_SIZE );
	AkReal32* pCurrentDmaBuffer = m_pDmaBuffer + m_pHwVoice[0]->uDmaReadPointer * SHAPE_FRAME_SIZE * rFormat.GetNumChannels();

	if (rFormat.HasLFE())
	{
		// LFE should follow left, right, center
		AkUInt32 uMask = (rFormat.channelConfig.uChannelMask & AK_SPEAKER_FRONT_LEFT) |
			(rFormat.channelConfig.uChannelMask & AK_SPEAKER_FRONT_RIGHT) |
			(rFormat.channelConfig.uChannelMask & AK_SPEAKER_FRONT_CENTER);

		// move LFE last
		XMAHELPERS::MoveLFEBuffer(pCurrentDmaBuffer, uNumSamplesRefill, (AkUInt16)AK::GetNumNonZeroBits( uMask ), (AkUInt16)rFormat.GetNumChannels()-1);
	}

	io_state.AttachInterleavedData( 
		pCurrentDmaBuffer,	// use our own dma read pointer because the other is updated asynchronously
		uNumSamplesRefill, 
		uNumSamplesRefill,
		rFormat.channelConfig );

	io_state.posInfo.uSampleRate = m_uSourceSampleRate;
	io_state.posInfo.uStartPos = m_uCurSample;
	io_state.posInfo.uFileEnd = m_uTotalSamples;

	ConsumeDmaFrames();

	// End of source?
	// Wait until all SHAPE frames previously produced are passed to the pipeline (including this one) before returning NoMoreData.
	io_state.result = ( !m_bEndOfSource || ValidDmaFrames() > 0 ) ? AK_DataReady : AK_NoMoreData;
}

void CAkSrcBankXMA::VirtualOn( AkVirtualQueueBehavior eBehavior )
{
    // "FromBeginning" & "FromElapsedTime": flush everything. "Resume": stand still.
	if ( eBehavior == AkVirtualQueueBehavior_FromBeginning 
        || eBehavior == AkVirtualQueueBehavior_FromElapsedTime )
	{
		// Do the same thing as CAkSrcBankXMA::StopStream(), but do not wait for the flowgraphs. We do not want to stall waiting for the harware everytime we go virtual. 
		FreeHardwareVoices();
		CAkSrcBaseEx::StopStream();
    }
}

AKRESULT CAkSrcBankXMA::VirtualOff( AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset )
{
	// "FromBeginning" & "FromElapsedTime": restart stream at proper position.
	// "Resume": just re-enter prebuffering state.
	if ( eBehavior == AkVirtualQueueBehavior_Resume )
	{
		EnterPreBufferingState();
		return AK_Success;
	}
	else if ( eBehavior == AkVirtualQueueBehavior_FromBeginning )
	{
		m_uCurSample = 0;
		ResetLoopCnt();
	}

	//We should not have already allocated the HW voices.
	AKASSERT(m_eHwVoiceState != HwVoiceState_Allocated);
	
	// Init XMA and place either to source offset, or to current PCM offset.
	WaitForHardwareVoices();
	return InitXMA(in_bUseSourceOffset);
}

AKRESULT CAkSrcBankXMA::SeekToSourceOffset( 
	AkXMA2WaveFormat & in_xmaFmt, 
	DWORD * in_pSeekTable, 
	AkUInt8 * in_pData, 
	AkUInt32 in_uNbStreams,
	DWORD * out_dwBitOffset, 
	DWORD & out_dwSubframesSkip )
{
	AkUInt32 uSourceOffset;
	if ( m_pCtx->RequiresSourceSeek() )
	{
		uSourceOffset = GetSourceOffset();
		if ( uSourceOffset >= in_xmaFmt.SamplesEncoded )
		{
			MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_SeekAfterEof, m_pCtx );
			return AK_Fail;
		}
	}
	else
	{
		// Seek at beginning of the file.
		uSourceOffset = 0;
	}

	AkUInt32 uRealOffset = uSourceOffset;
	if ( FindDecoderPosition( uRealOffset, in_xmaFmt, in_pSeekTable, in_pData, in_uNbStreams, out_dwBitOffset, out_dwSubframesSkip ) != AK_Success )
		return AK_Fail;			

	m_uCurSample = uRealOffset;
	AKASSERT( uSourceOffset >= uRealOffset );
	/// m_pCtx->SetSourceOffsetRemainder( uSourceOffset - uRealOffset );
	/// NOTE: SHAPE XMA does not support sample accurate looping from pitch node. This would require decoding one more SHAPE
	/// buffer in order to be ready for these extra consumed samples, and would be inefficient. 
	m_pCtx->SetSourceOffsetRemainder( 0 );

	return AK_Success;
}

AKRESULT CAkSrcBankXMA::InitXMA(
	bool in_bUseSourceOffset
	)
{
	// TODO Do not re-execute parsing.
	AkUInt8 * pBuffer;
    AkUInt32 uBufferSize;
    m_pCtx->GetDataPtr( pBuffer, uBufferSize );
    if ( pBuffer == NULL )
		return AK_Fail;

	// Do not parse markers if they were already parsed.
	CAkMarkers * pMarkers = ( m_markers.Count() == 0 ) ? &m_markers : NULL;

	AkFileParser::FormatInfo fmtInfo;
	AkFileParser::SeekInfo seekInfo;
	AkFileParser::AnalysisDataChunk analysisDataChunk;
	AKRESULT eResult = AkFileParser::Parse( pBuffer,			// Data buffer
											uBufferSize,		// Buffer size
											fmtInfo,			// Returned audio format info.
											pMarkers,			// Markers.
											&m_uPCMLoopStart,   // Beginning of loop offset.
											&m_uPCMLoopEnd,		// End of loop offset.
											&m_uDataSize,		// Data size.
											&m_uDataOffset,		// Offset to data.
											&analysisDataChunk,	// Analysis info.
											&seekInfo, 			// Format specific seek table.
											true);

    if ( eResult != AK_Success )
    {
        MONITOR_SOURCE_ERROR( AkFileParser::ParseResultToMonitorMessage( eResult ), m_pCtx );
		return AK_InvalidFile;
    }
	
	AKASSERT(ApuIsVirtualAddressValid((pBuffer + m_uDataOffset), SHAPE_XMA_INPUT_BUFFER_ALIGNMENT));

	if(	(m_uDataSize > SHAPE_XMA_MAX_INPUT_BUFFER_SIZE)			||
		(m_uDataSize & SHAPE_XMA_INPUT_BUFFER_SIZE_MASK)		||
		(m_uDataOffset & SHAPE_XMA_INPUT_BUFFER_SIZE_MASK) ) 
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_InvalidXMAData, m_pCtx );
		return AK_InvalidFile;
	}

	if ( (m_uDataSize - m_uDataOffset) > uBufferSize )
	{
		//This probably means that we are trying to play a whole file, but only have prefetch media loaded.  This will choke SHAPE.
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_SelectedMediaNotAvailable, m_pCtx );
		return AK_InvalidFile;
	}

	// XMA expects an AkXMA2WaveFormat
	AKASSERT( fmtInfo.uFormatSize == sizeof( AkXMA2WaveFormat ) );
	AkXMA2WaveFormat * pXMAfmt = (AkXMA2WaveFormat*)fmtInfo.pFormat;

	//Setup format on the PBI

	AkUInt32 dwChannelMask = pXMAfmt->ChannelMask;	
	const WORD uChannels = pXMAfmt->wfx.nChannels;

	// SRC is performed within this source; store sample rate of source, and show 48KHz to the pipeline.
	m_uSourceSampleRate = pXMAfmt->wfx.nSamplesPerSec;

	AkAudioFormat format;
	AkChannelConfig cfg;
	cfg.SetStandard( dwChannelMask ); // supports standard configs only
	format.SetAll(
		AK_CORE_SAMPLERATE, 
		cfg,
		32, 
		sizeof(AkReal32) * uChannels, 
		AK_FLOAT,								
		AK_NONINTERLEAVED);					
	m_pCtx->SetMediaFormat(format);

	if ( analysisDataChunk.uDataSize > 0 )
		m_pAnalysisData = analysisDataChunk.pData;

	AKASSERT( !( m_uDataSize & 2047 ) ); // Data should be a multiple of 2K

	m_uTotalSamples = pXMAfmt->SamplesEncoded;

    // XMA2? then keep PCM loop points (data boundaries otherwise).
    if ( pXMAfmt->LoopLength )
    {
        m_uPCMLoopStart = pXMAfmt->LoopBegin;
        m_uPCMLoopEnd   = pXMAfmt->LoopBegin + pXMAfmt->LoopLength;
    }
    else
    {
        m_uPCMLoopStart = 0;
        m_uPCMLoopEnd   = pXMAfmt->SamplesEncoded - 1;
    }

	// Seek to appropriate place.
	m_uNumXmaStreams = (uChannels + 1) / 2;
	DWORD dwBitOffset[AK_MAX_XMASTREAMS];
	DWORD dwSubframesSkip;
	if ( in_bUseSourceOffset )
		eResult = SeekToSourceOffset( *pXMAfmt, (DWORD*)seekInfo.pSeekTable, pBuffer, m_uNumXmaStreams, dwBitOffset, dwSubframesSkip );
	else
		eResult = FindDecoderPosition( m_uCurSample, *pXMAfmt, (DWORD*)seekInfo.pSeekTable, pBuffer, m_uNumXmaStreams, dwBitOffset, dwSubframesSkip );
	if ( eResult != AK_Success )
		return eResult;

	// XMA
	SHAPE_XMA_SAMPLE_RATE eSampleRate;
	switch ( pXMAfmt->wfx.nSamplesPerSec )
	{
	case 24000:
		eSampleRate = SHAPE_XMA_SAMPLE_RATE_24K;
		break;
	case 32000:
		eSampleRate = SHAPE_XMA_SAMPLE_RATE_32K;
		break;
	case 44100:
		eSampleRate = SHAPE_XMA_SAMPLE_RATE_44_1K;
		break;
	case 48000:
	default:
		eSampleRate = SHAPE_XMA_SAMPLE_RATE_48K;
		break;
	}

	eResult = RegisterHardwareVoices(this);
	if (eResult != AK_Success)
		return eResult;

	// Init XMA decoder / voice.
	for (AkUInt8 i = 0; i < m_uNumXmaStreams; i++)
	{
		AkUInt32 uStreamChannels = 2;
		if ((uChannels & 0x1) != 0 && i == (m_uNumXmaStreams - 1))
		{
			uStreamChannels = 1;
		}

		//For non-streamed voices, always try to produce AK_NUM_DMA_FRAMES_PER_BUFFER frames of data.
		m_pHwVoice[i]->uMaxFlowgraphsToRun = AK_NUM_DMA_FRAMES_PER_BUFFER;

		// SRC
		/// REVIEW Is there a problem with this?
		HRESULT hr = SetShapeSrcCommand( m_pHwVoice[i]->pSrcContext, SHAPE_SRC_COMMAND_TYPE_START );
		AKVERIFY( SUCCEEDED( hr ) );

        AKASSERT( !m_pHwVoice[i]->IsXmaCtxEnable() );

		hr = SetShapeXmaInputBuffer0(m_pHwVoice[i]->pXmaContext, (char *) pBuffer + m_uDataOffset);
		AKVERIFY( SUCCEEDED( hr ) );

		// NOTE: Set to same as buffer0 to avoid hardware hangs in some cases, according to MS.
		hr = SetShapeXmaInputBuffer1(m_pHwVoice[i]->pXmaContext, (char *) pBuffer + m_uDataOffset);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaInputBuffer0Size(m_pHwVoice[i]->pXmaContext, m_uDataSize);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaInputBuffer1Size(m_pHwVoice[i]->pXmaContext, 0);
		AKVERIFY( SUCCEEDED( hr ) );

		SetDecoderLooping( GetLoopCnt(), i );

		hr = SetShapeXmaLoopSubframeEnd(m_pHwVoice[i]->pXmaContext, pXMAfmt->loopData[i].SubframeData >> 4);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaLoopSubframeSkip(m_pHwVoice[i]->pXmaContext, ( pXMAfmt->loopData[i].SubframeData & 15 ) );
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaNumSubframesToDecode(m_pHwVoice[i]->pXmaContext, 1);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaNumSubframesToSkip(m_pHwVoice[i]->pXmaContext, dwSubframesSkip);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaNumChannels(m_pHwVoice[i]->pXmaContext, uStreamChannels);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaSampleRate(m_pHwVoice[i]->pXmaContext, eSampleRate);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaInputBufferReadOffset(m_pHwVoice[i]->pXmaContext, dwBitOffset[i] );
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaOutputBufferReadOffset(m_pHwVoice[i]->pXmaContext, 0);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaLoopStartOffset(m_pHwVoice[i]->pXmaContext, pXMAfmt->loopData[i].LoopStart);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaLoopEndOffset(m_pHwVoice[i]->pXmaContext, pXMAfmt->loopData[i].LoopEnd);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaPacketMetadata(m_pHwVoice[i]->pXmaContext, 0);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaCurrentInputBuffer(m_pHwVoice[i]->pXmaContext, 0);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaInputBuffer0Valid(m_pHwVoice[i]->pXmaContext, true);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaInputBuffer1Valid(m_pHwVoice[i]->pXmaContext, false);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaOutputBufferValid(m_pHwVoice[i]->pXmaContext, true );
		AKVERIFY( SUCCEEDED( hr ) );

		// DMA
		for ( AkUInt32 j = 0; j < uStreamChannels; ++j )
		{
			hr = SetShapeDmaReadPointer( m_pHwVoice[i]->pDmaContext[j], 0 );
			AKVERIFY( SUCCEEDED( hr ) );

			hr = SetShapeDmaWritePointer( m_pHwVoice[i]->pDmaContext[j], 0 );
			AKVERIFY( SUCCEEDED( hr ) );

			hr = SetShapeDmaBufferFull( m_pHwVoice[i]->pDmaContext[j], false );
			AKVERIFY( SUCCEEDED( hr ) );

			hr = SetShapeDmaAudioBuffer( m_pHwVoice[i]->pDmaContext[j], m_pDmaBuffer );
			AKVERIFY( SUCCEEDED( hr ) );

			hr = SetShapeDmaNumFrames( m_pHwVoice[i]->pDmaContext[j], AK_NUM_DMA_FRAMES_IN_OUTPUT_BUFFER );
			AKVERIFY( SUCCEEDED( hr ) );

			hr = SetShapeDmaFloatSamples( m_pHwVoice[i]->pDmaContext[j], true );
			AKVERIFY( SUCCEEDED( hr ) );
			
			hr = SetShapeDmaNumChannels( m_pHwVoice[i]->pDmaContext[j], uChannels );
			AKVERIFY( SUCCEEDED( hr ) );

			hr = SetShapeDmaChannel( m_pHwVoice[i]->pDmaContext[j], (i*2)+j ); // 2 channel per stream, expect last one which can be either 1 or 2
			AKVERIFY( SUCCEEDED( hr ) );
		}
	}

	// We just set up the decoder (because of playback start or seeking), so we need to reset the buffering status.
	EnterPreBufferingState();

	m_bEndOfSource = false;

    return eResult;
}

AKRESULT CAkSrcBankXMA::StartStream( AkUInt8 *, AkUInt32 )
{
	if (!AkACPMgr::Get().IsInitialized())
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_XMACreateDecoderLimitReached, m_pCtx );
		return AK_Fail;
	}

	// Init XMA and place to source offset.
	m_uCurSample = 0;
    return InitXMA( true );
}

// Note: HW context must exist.
AKRESULT CAkSrcBankXMA::FindDecoderPosition( AkUInt32& io_uSampleOffset, XMA2WAVEFORMATEX& in_xmaFmt, DWORD * in_pSeekTable, AkUInt8* pData, AkUInt32 in_uNbStreams, DWORD * out_dwBitOffset, DWORD & out_dwSubframesSkip )
{
	// Find block in seek table.
	int i = 0;
	AKASSERT( io_uSampleOffset <= in_xmaFmt.SamplesEncoded );
    while ( io_uSampleOffset > in_pSeekTable[i] )
        ++i;
    
	// Now, navigate through XMA data to find correct offset (will round down to 128 samples boundary).
	
	// Start parsing XMA stream from nearest block.
	AkUInt32 uBlockOffset = i*in_xmaFmt.BytesPerBlock;	// in bytes.

	AkUInt32 uDesiredSampleOffsetFromBlock = io_uSampleOffset - ( i ? in_pSeekTable[i-1] : 0 );
    
	bool bDataOutOfBounds = false; 
    if ( AK::XMAHELPERS::FindDecodePosition( pData + m_uDataOffset + uBlockOffset, m_uDataSize - uBlockOffset, uDesiredSampleOffsetFromBlock, in_uNbStreams, out_dwBitOffset, out_dwSubframesSkip, bDataOutOfBounds ) == AK_Success )
    {
		if (bDataOutOfBounds)
		{
			return AK_Fail;
		}

		// Add block offset (in bits) to bit offset computed from address passed to FindDecodePosition().
		for (AkUInt32 idx = 0; idx < in_uNbStreams; idx++)
		{
			out_dwBitOffset[idx] += 8 * uBlockOffset;
		}

		// uDesiredSampleOffsetFromBlock is returned as the remaining samples to be skipped after the nearest
		// subframe where the decoder has been placed. 
		// Return accurate sample offset to which we seeked to.
		io_uSampleOffset -= uDesiredSampleOffsetFromBlock;
		AKASSERT( io_uSampleOffset % XMA_SAMPLES_PER_SUBFRAME == 0 );

		return AK_Success;
    }

	return AK_Fail;
}

AKRESULT CAkSrcBankXMA::ChangeSourcePosition()
{	
	AKRESULT res = AK_Success;
	if (m_eHwVoiceState == HwVoiceState_Allocated)
	{
		//To seek into an active voice, destroy and re-init the HW voice.	
		FreeHardwareVoices();
		WaitForHardwareVoices();
		res = InitXMA(true);
	}
	else if (m_pCtx->RequiresSourceSeek())
	{
		//Virtual voices will start in the correct position when they become real.
		//When InitXMA() is called later, FindDecoderPosition() will set m_uCurSample to the precise sample required by the decoder. 
		m_uCurSample = AkMin( GetSourceOffset(), m_uTotalSamples );
	}

	return res;
}

// IO/buffering status notifications. 
// In-memory XMA may starve because of the XMA decoder. 
void CAkSrcBankXMA::NotifySourceStarvation()
{
	MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_XMADecoderSourceStarving, m_pCtx );
}

