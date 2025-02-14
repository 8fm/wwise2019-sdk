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
#include "AkSrcFileADPCM.h"
#include "AkADPCMCodec.h"
#include "AkFileParserBase.h"
#include "AkMonitor.h"
#include "PtADPCM_Decode.h"

CAkSrcFileADPCM::CAkSrcFileADPCM( CAkPBI * in_pCtx )
	: CAkSrcFileBase( in_pCtx )
	, m_pOutBuffer( NULL )
	, m_pExtraBlock( NULL )
	, m_wExtraSize( 0 )
	, m_uSamplesPerBlock(0)
{
}

CAkSrcFileADPCM::~CAkSrcFileADPCM()
{	
	if ( m_pExtraBlock )
	{
		AkFree( AkMemID_Processing, m_pExtraBlock );
	}

	if (m_pOutBuffer)
	{
		AkFalign(AkMemID_Processing, m_pOutBuffer);
		m_pOutBuffer = NULL;
	}

}

void CAkSrcFileADPCM::GetBuffer( AkVPLState & io_state )
{
    AKASSERT( m_pStream != NULL );

    AKRESULT eResult = IsPrebufferingReady();
	if ( AK_EXPECT_FALSE( eResult != AK_DataReady ) )
	{
		io_state.result = eResult;
		return;
	};

    // See if we need to get more data from stream manager.
    if ( m_ulSizeLeft == 0 )
    {
		AKASSERT( !HasNoMoreStreamData() );	// If there really was no more data, the source should already have returned AK_NoMoreData.
		eResult = FetchStreamBuffer();
		if ( AK_EXPECT_FALSE( eResult != AK_DataReady ) )
		{
			io_state.result = eResult;
			return;
		}
    }

	const AkAudioFormat& rFormat = m_pCtx->GetMediaFormat();
	AkUInt32 uBlockAlign = rFormat.GetBlockAlign();
	AkUInt32 uNumChannels = rFormat.channelConfig.uNumChannels;

    // At that point, some data is ready, or there is no more data.
    // Maybe loop end or data chunk sizes were invalid.
    AKASSERT( m_ulSizeLeft > 0 || !"No more data: GetBuffer should not have been called" );

	// Allocate output buffer for decoded data

	
	AKASSERT( io_state.MaxFrames() >= m_uSamplesPerBlock );
	AkUInt16 uMaxFrames = AK_NUM_VOICE_REFILL_FRAMES;
	if (m_pOutBuffer == NULL)
	{
		m_pOutBuffer = (AkUInt8 *)AkMalign(AkMemID_Processing, AK_NUM_VOICE_REFILL_FRAMES * uBlockAlign, AK_BUFFER_ALIGNMENT);
		if (!m_pOutBuffer)
		{
			io_state.result = AK_Fail;
			return;
		}
	}

	AkPrefetchZeroAligned(m_pOutBuffer, AK_NUM_VOICE_REFILL_FRAMES * uBlockAlign);

	AkUInt32 ulPCMBlockAlign = uBlockAlign * m_uSamplesPerBlock;

	AkUInt8 * pOutBuffer = m_pOutBuffer;
	
	AkUInt32 uChannelBlockSize = m_uInputBlockSize / uNumChannels;
	
	// Might need to process one compressed block split between input buffers

	if ( m_wExtraSize )
	{
		AKASSERT( m_pExtraBlock );
		AKPLATFORM::AkMemCpy( m_pExtraBlock + m_wExtraSize, m_pNextAddress, m_uInputBlockSize - m_wExtraSize );

		if (m_uFormatTag == AK_WAVE_FORMAT_ADPCM)
		{
			for (AkUInt32 iChan = 0; iChan < uNumChannels; ++iChan)
				CAkADPCMCodec::Decode(m_pExtraBlock + iChan * uChannelBlockSize, pOutBuffer + iChan * sizeof(AkInt16), 1, m_uInputBlockSize, uNumChannels);
		}
		else
		{
			for (AkUInt32 iChan = 0; iChan < uNumChannels; ++iChan)
				PtADPCM::DecodeBlocks((AkInt16 *)pOutBuffer + iChan, m_pExtraBlock + iChan * uChannelBlockSize, m_uSamplesPerBlock, m_uInputBlockSize, uNumChannels);
		}
			
		ConsumeData( m_uInputBlockSize - m_wExtraSize );

		m_wExtraSize = 0;

		pOutBuffer += ulPCMBlockAlign;
		uMaxFrames -= m_uSamplesPerBlock;
	}

	// Now convert this input buffer
	AkUInt32 uSizeLeftFrames = m_ulSizeLeft / m_uInputBlockSize;
	AkUInt32 uNumBlocks = uMaxFrames / m_uSamplesPerBlock;
	AkUInt32 ulADPCMBlocksToProcess = AkMin( uSizeLeftFrames, uNumBlocks);

	if (m_uFormatTag == AK_WAVE_FORMAT_ADPCM)
	{
		for (AkUInt32 iChan = 0; iChan < uNumChannels; ++iChan)
			CAkADPCMCodec::Decode(m_pNextAddress + iChan * uChannelBlockSize, pOutBuffer + iChan * sizeof(AkInt16), ulADPCMBlocksToProcess, m_uInputBlockSize, uNumChannels);
	}
	else
	{
		for (AkUInt32 iChan = 0; iChan < uNumChannels; ++iChan)
			PtADPCM::DecodeBlocks((AkInt16 *)pOutBuffer + iChan, m_pNextAddress + iChan * uChannelBlockSize, ulADPCMBlocksToProcess*m_uSamplesPerBlock, m_uInputBlockSize, uNumChannels);
	}

	pOutBuffer += ulADPCMBlocksToProcess * ulPCMBlockAlign;

	uMaxFrames = (AkUInt16)(( pOutBuffer - m_pOutBuffer ) / uBlockAlign);

    ConsumeData( ulADPCMBlocksToProcess * m_uInputBlockSize );

    // If there is less than a whole compressed block left in the input buffer, 
	// keep what's left and request the next one, as we cannot process this bit by itself.

    if ( m_ulSizeLeft < m_uInputBlockSize )
    {
		if ( !m_pExtraBlock )
		{
			m_pExtraBlock = (AkUInt8*)AkAlloc(AkMemID_Processing, m_uInputBlockSize);
			if ( AK_EXPECT_FALSE( !m_pExtraBlock ) )
			{
				io_state.result = AK_Fail;
				return;
			}
		}

		m_wExtraSize = (AkUInt16) m_ulSizeLeft;
		AKPLATFORM::AkMemCpy( m_pExtraBlock, m_pNextAddress, m_wExtraSize );

		ConsumeData( m_ulSizeLeft );

        ReleaseStreamBuffer();
    }

	// Prepare buffer for VPL, update PCM position, handle end of loop/file, post markers and position info.
	SubmitBufferAndUpdate( m_pOutBuffer, uMaxFrames, rFormat.uSampleRate, rFormat.channelConfig, io_state );
}

void CAkSrcFileADPCM::StopStream()
{
	if (m_pOutBuffer)
	{
		AkFalign(AkMemID_Processing, m_pOutBuffer);
		m_pOutBuffer = NULL;
	}

	CAkSrcFileBase::StopStream();
}

void CAkSrcFileADPCM::VirtualOn( AkVirtualQueueBehavior eBehavior )
{
	CAkSrcFileBase::VirtualOn( eBehavior );

	if ( eBehavior == AkVirtualQueueBehavior_FromBeginning 
		|| eBehavior == AkVirtualQueueBehavior_FromElapsedTime )
	{
		// Streaming buffers are released in CAkSrcFileBase::VirtualOn() in these two modes:
		// Reset buffered input.
		m_wExtraSize = 0;
	}
}

AKRESULT CAkSrcFileADPCM::ParseHeader(
	AkUInt8 * in_pBuffer	// Buffer to parse
	)
{
    AKASSERT( m_pStream );

    // Got the first buffer. Parse.
	AkFileParser::FormatInfo fmtInfo;
	AkFileParser::AnalysisDataChunk analysisDataChunk;
	AKRESULT eResult = AkFileParser::Parse( in_pBuffer,			// Data buffer.
											m_ulSizeLeft,		// Buffer size.
											fmtInfo,			// Returned audio format info.
											&m_markers,			// Markers.
											&m_uPCMLoopStart,	// Beginning of loop.
											&m_uPCMLoopEnd,		// End of loop (inclusive).
											&m_uDataSize,		// Data size.
											&m_uDataOffset,		// Offset to data.
											&analysisDataChunk,	// Analysis data.
											NULL );				// (Format-specific) seek table info (pass NULL is not expected).

    if ( eResult != AK_Success )
    {
        MONITOR_SOURCE_ERROR( AkFileParser::ParseResultToMonitorMessage( eResult ), m_pCtx );
        return eResult;
    }

	m_uFormatTag = fmtInfo.pFormat->wFormatTag;

	//Check if the file is of the expected format
	if (m_uFormatTag != AK_WAVE_FORMAT_ADPCM && m_uFormatTag != AK_WAVE_FORMAT_PTADPCM)
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_FileFormatMismatch, m_pCtx );
		return AK_InvalidFile;
	}

	WaveFormatExtensible * pFmt = fmtInfo.pFormat;
	AKASSERT( fmtInfo.uFormatSize == sizeof( WaveFormatExtensible ) );

	//Setup format on the PBI	
	AkAudioFormat format;
	format.SetAll(
		pFmt->nSamplesPerSec, 
		pFmt->GetChannelConfig(), 
		16, 
		2 * pFmt->nChannels, 
		AK_INT,								
		AK_INTERLEAVED);					

	m_pCtx->SetMediaFormat(format);

	// Store analysis data if it was present. 
	// Note: StoreAnalysisData may fail, but it is not critical.
	if ( analysisDataChunk.uDataSize > 0 )
		StoreAnalysisData( analysisDataChunk );

	m_uInputBlockSize = pFmt->nBlockAlign;

	if (m_uFormatTag == AK_WAVE_FORMAT_ADPCM)
	{
		m_uSamplesPerBlock = ADPCM_SAMPLES_PER_BLOCK;
		AKASSERT(CAkADPCMCodec::IsValidImaAdpcmFormat(*pFmt));

	}
	else // PtADPCM
	{
		m_uSamplesPerBlock = PtADPCM::SamplePerBlock;
		AKASSERT(PtADPCM::IsValidPtAdpcmFormat(*pFmt));
	}

	m_uTotalSamples = m_uDataSize * m_uSamplesPerBlock / pFmt->nBlockAlign;

    // Set loop points.
    AkUInt32 ulEndOfData = m_uDataOffset + m_uDataSize;

    // Loop points. If not looping or ulLoopEnd is 0 (no loop points),
    // set loop points to data chunk boundaries.
    if ( 0 == m_uPCMLoopEnd 
		|| !DoLoop() )
    {
        // Loop start = start of data.
        m_ulLoopStart   = m_uDataOffset;
        // Loop end = end of data.
        m_ulLoopEnd    = ulEndOfData;
        
        // Fix PCM Loop End.
        AKASSERT( m_uDataSize > 0 
        		&& ( m_uDataSize / m_uInputBlockSize * m_uSamplesPerBlock ) >= 1 );
		m_uPCMLoopEnd = m_uDataSize / m_uInputBlockSize * m_uSamplesPerBlock - 1;
    }
    else
    {	
		// If LoopEnd is non zero, then it is in sample frames and the loop should include the end sample.
        // Convert to bytes offset, from beginning of FILE.

		// Find the block number that the loop points corresponds to.
		AKASSERT( (m_uPCMLoopStart % m_uSamplesPerBlock ) == 0 ); 
		AKASSERT( ((m_uPCMLoopEnd + 1) % m_uSamplesPerBlock ) == 0 ); 
		m_ulLoopStart = m_uDataOffset + m_uInputBlockSize * (m_uPCMLoopStart / m_uSamplesPerBlock );
		m_ulLoopEnd = m_uDataOffset + m_uInputBlockSize * ((m_uPCMLoopEnd+1) / m_uSamplesPerBlock );

        // Verify loop. Invalid if loop start or loop end passed the last sample of the file, or if loop end before loop start.
        if ( m_uPCMLoopEnd < m_uPCMLoopStart ||
			 m_ulLoopStart > ulEndOfData || 
             m_ulLoopEnd > ulEndOfData )
        {
	        MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_InvalidAudioFileHeader, m_pCtx );
            return AK_InvalidFile;
        }
    }

    // Update stream heuristics.
	AkAutoStmHeuristics heuristics;
    m_pStream->GetHeuristics( heuristics );

	//One more sanity check on the loop points
	if ( m_uPCMLoopEnd <= m_uPCMLoopStart || m_uPCMLoopStart > m_uTotalSamples || m_uPCMLoopEnd >= m_uTotalSamples)
		return AK_Fail;

    // Throughput.
    heuristics.fThroughput = (AkReal32) pFmt->nSamplesPerSec * pFmt->nBlockAlign / ( 1000.f * m_uSamplesPerBlock );

    // Looping.
    if ( DoLoop() )
    {
        heuristics.uLoopStart = m_ulLoopStart;
        heuristics.uLoopEnd = m_ulLoopEnd;
    }

    // Priority.
    heuristics.priority = m_pCtx->GetPriority();

    m_pStream->SetHeuristics( heuristics );

	// Update stream buffering settings.
	// Cannot receive less than an ADPCM block.
	return m_pStream->SetMinimalBufferSize(m_uInputBlockSize);
}

// Finds the closest offset in file that corresponds to the desired sample position.
// Returns the file offset (in bytes, relative to the beginning of the file) and its associated sample position.
// Returns AK_Fail if the codec is unable to seek.
AKRESULT CAkSrcFileADPCM::FindClosestFileOffset( 
	AkUInt32 in_uDesiredSample,		// Desired sample position in file.
	AkUInt32 & out_uSeekedSample,	// Returned sample where file position was set.
	AkUInt32 & out_uFileOffset		// Returned file offset where file position was set.
	)
{
	AkUInt32 l_numADPCMBlock = in_uDesiredSample / m_uSamplesPerBlock;
	out_uSeekedSample = l_numADPCMBlock * m_uSamplesPerBlock;
	out_uFileOffset = m_uDataOffset + l_numADPCMBlock * m_uInputBlockSize;
	return AK_Success;
}


AKRESULT CAkSrcFileADPCM::ChangeSourcePosition()
{
	m_wExtraSize = 0;
	return CAkSrcFileBase::ChangeSourcePosition();
}

