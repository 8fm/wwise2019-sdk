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
#include "AkSrcFileVorbis.h"
#include "AkMonitor.h"
#include "AkProfile.h"
#include "AkVorbisCodebookMgr.h"
#include "AkVorbisCodec.h"

// Constructor
CAkSrcFileVorbis::CAkSrcFileVorbis( CAkPBI * in_pCtx ) 
: CAkSrcFileBase( in_pCtx )
, m_bStitching (false)
, m_pOutputBuffer(NULL)
, m_uOutputBufferSize(0)
{
	m_pOggPacketData = NULL;
	m_uPacketDataGathered = 0;
	m_uPacketHeaderGathered = 0;

	InitVorbisState();
}

// Destructor
CAkSrcFileVorbis::~CAkSrcFileVorbis()
{
	ReleaseBuffer();
	if ( m_VorbisState.TremorInfo.VorbisDSPState.csi )
		g_VorbisCodebookMgr.ReleaseCodebook( m_VorbisState );
}

// GetBuffer
void CAkSrcFileVorbis::GetBuffer( AkVPLState & io_state )
{
	// The stream should not start before all headers are decoded
	AKASSERT( m_VorbisState.TremorInfo.ReturnInfo.eDecoderState >= PACKET_STREAM );

	// Check prebuffering status.
	AKRESULT eResult = IsPrebufferingReady();
	if ( AK_EXPECT_FALSE( eResult != AK_DataReady ) )
	{
		io_state.result = eResult;
		return;
	}	

	do 
	{
		//Erase previous state.
		m_VorbisState.TremorInfo.ReturnInfo.eDecoderStatus = AK_NoDataReady;
		m_VorbisState.TremorInfo.ReturnInfo.uFramesProduced = 0;
		m_VorbisState.TremorInfo.ReturnInfo.uInputBytesConsumed = 0;	

		eResult = GetNextPacket();	//GetNextPacket ensures that there is at least one full packet.  There may be more data to process though.
		if (eResult == AK_DataReady)
		{
			m_VorbisState.TremorInfo.uInputDataSize = CurrentPacketSize() + sizeof(OggPacketHeader);
			m_VorbisState.TremorInfo.bNoMoreInputPackets = Stream_NextFetchWillLoop() || HasNoMoreStreamData();
			if (m_bStitching)
				m_VorbisState.TremorInfo.bNoMoreInputPackets = m_VorbisState.TremorInfo.bNoMoreInputPackets && m_ulSizeLeft == 0;	//It is the last packet only if there is nothing left in the stream buffer.
			else
				m_VorbisState.TremorInfo.uInputDataSize += m_ulSizeLeft;	//We have more data anyway.  Give it.				

			DecodeVorbis(&m_VorbisState.TremorInfo, m_VorbisState.VorbisInfo.uMaxPacketSize, m_pOggPacketData, m_pOutputBuffer, m_uOutputBufferSize);

			io_state.result = m_VorbisState.TremorInfo.ReturnInfo.eDecoderStatus;

			if(io_state.result != AK_Fail && !m_bStitching && m_VorbisState.TremorInfo.ReturnInfo.uInputBytesConsumed != 0)
				ConsumeData(m_VorbisState.TremorInfo.ReturnInfo.uInputBytesConsumed - (CurrentPacketSize() + sizeof(OggPacketHeader)));		//The first packet is always consumed by GetNextPacket

			FreeStitchBuffer();						
		}
		else
		{		
			io_state.result = eResult;	
		}
	} while (eResult == AK_DataReady && (m_VorbisState.TremorInfo.ReturnInfo.eDecoderStatus == AK_DataNeeded || m_VorbisState.TremorInfo.ReturnInfo.eDecoderStatus == AK_NoDataReady));
	
	// Prepare buffer for VPL, update PCM position, handle end of loop/file, post markers and position info.
	SubmitBufferAndUpdateVorbis( io_state );
}

// Override SubmitBufferAndUpdate(): Check decoder status, post error message if applicable
// and restart DSP if loop is resolved.
void CAkSrcFileVorbis::SubmitBufferAndUpdateVorbis( AkVPLState & io_state )
{	
	if(io_state.result == AK_DataReady || io_state.result == AK_NoDataReady || io_state.result == AK_NoMoreData)
	{				
		// Prepare buffer for VPL, update PCM position, handle end of loop/file, post markers and position info.
		SubmitBufferAndUpdate( 
			m_pOutputBuffer,
			m_VorbisState.TremorInfo.ReturnInfo.uFramesProduced, 
			m_VorbisState.uSampleRate,
			m_VorbisState.TremorInfo.channelConfig, 
			io_state );

		if ( AK_EXPECT_FALSE( io_state.result == AK_NoDataReady && m_pNextAddress ) ) // Handle decoder starvation: check note below about this condition.
		{
			// The _decoder_ returned no data ready, although streaming data is ready.

			// IMPORTANT: Check m_pNextAddress, not m_ulSizeLeft! m_ulSizeLeft has already been consumed.
			// What we are interested in now is whether we did send streamed data or not. GetStreamBuffer() 
			// had starved, then m_pNextAddress would also be NULL.

			if ( m_VorbisState.TremorInfo.ReturnInfo.uInputBytesConsumed )
			{
				// Change return code to AK_DataReady to force the pipeline to ask us again.
				io_state.result = AK_DataReady;
			}
			else
			{
				// WG-18965: Decoder is not able to consume what is available, and won't be getting any more.
				// Kill this voice to avoid endless ping-pong between source and pitch nodes.
				MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_VorbisDecodeError, m_pCtx );
				io_state.result = AK_Fail;
			}
		}
	}
	else
	{
		io_state.result = AK_Fail;
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_VorbisDecodeError, m_pCtx );
	}
}

// Override OnLoopComplete() handler: "restart DSP" (set primimg frames) and fix decoder status 
// if it's a loop end.
AKRESULT CAkSrcFileVorbis::OnLoopComplete(
	bool in_bEndOfFile		// True if this was the end of file, false otherwise.
	)
{
	// IMPORTANT: Call base first. VorbisDSPRestart() checks the loop count, so it has to be updated first.
	AKRESULT eResult = CAkSrcFileBase::OnLoopComplete( in_bEndOfFile );
	if ( !in_bEndOfFile )
	{
		VorbisDSPRestart( m_VorbisState.VorbisInfo.LoopInfo.uLoopBeginExtra );

		// Vorbis reported no more data due to end-of-loop; reset the state.
		AKASSERT( m_VorbisState.TremorInfo.ReturnInfo.eDecoderStatus == AK_NoMoreData );
		m_VorbisState.TremorInfo.ReturnInfo.eDecoderStatus = AK_DataReady;
	}
	return eResult;
}

// ReleaseBuffer
void CAkSrcFileVorbis::ReleaseBuffer()
{
	m_VorbisState.TremorInfo.ReturnInfo.uFramesProduced = 0;
	// Preserve output buffer allocation to avoid allocator churn
	// FreeOutputBuffer();
}

// StartStream
AKRESULT CAkSrcFileVorbis::StartStream( AkUInt8 * in_pBuffer, AkUInt32 ulBufferSize )
{
	if ( m_bFormatReady )
	{
		// Check streaming status if header has already been parsed.
		return IsInitialPrebufferingReady();
	}
	else if ( m_pStream && m_VorbisState.TremorInfo.ReturnInfo.eDecoderState < PACKET_STREAM )
	{
		// Try process first buffer if stream is already created 
		AKRESULT eResult = ProcessFirstBuffer();
		if ( eResult == AK_Success )
			eResult = IsInitialPrebufferingReady();
		return eResult;
	}

	// Specify buffer constraints. By default, no constraint is necessary. But it is obviously harder
	// for any stream manager implementation to increase the minimum buffer size at run time than to decrease
	// it, so we set it to a "worst case" value.
	// Note: With Vorbis, it sometimes happens that the max packet size is larger than 2K.
	AkAutoStmBufSettings bufSettings;
	bufSettings.uBufferSize		= 0;		// No constraint.
	bufSettings.uMinBufferSize	= 0;
	bufSettings.uBlockSize		= 0;
	AKRESULT eResult = CreateStream( bufSettings, 0 );
	if ( AK_EXPECT_FALSE( eResult != AK_Success ) )
		return eResult;

	bool bUsePrefetchedData;
	eResult = HandlePrefetch( bUsePrefetchedData );
	if ( eResult == AK_Success )
	{
		// Start IO.
        eResult = m_pStream->Start();
		if ( AK_EXPECT_FALSE( eResult != AK_Success ) )
			return eResult;

		// If the header was not parsed from bank data, attempt to fetch and process the first stream buffer now.
		if ( !bUsePrefetchedData )
		{
			eResult = ProcessFirstBuffer();
			if ( eResult == AK_Success )
				eResult = IsInitialPrebufferingReady();
		}
		else
		{
			// If prefetch is used, the full header and setup information should be in the bank 
			LoopInit();
			eResult = DecodeVorbisHeader();
			AKASSERT( eResult != AK_FormatNotReady );
			if ( eResult != AK_Success )
				return eResult;

			VorbisDSPRestart( 0 );
		
			AKASSERT( !IsPreBuffering() );	// This is normally reset in SeekStream(), which is not used here. If we ever do, set m_bWaitForCompleteBuffering to false.
		}
	}

	return eResult;
}

// StopStream
void CAkSrcFileVorbis::StopStream()
{
	TermVorbisState();

	ReleaseBuffer();

	if ( m_VorbisState.pSeekTable )
	{
		AkFree( AkMemID_Processing, m_VorbisState.pSeekTable );
		m_VorbisState.pSeekTable = NULL;
	}

	FreeStitchBuffer();	
	FreeOutputBuffer();

	CAkSrcFileBase::StopStream();
}

AKRESULT CAkSrcFileVorbis::StopLooping()
{
	AKRESULT eResult = CAkSrcFileBase::StopLooping();
	// Fix the amount of samples to trim out of the loop end to that of the end of file, IF AND ONLY IF
	// streaming has not looped already. In the latter case, this value is normally fixed when looping
	// is resolved (WG-18602).
	if ( !Stream_NextFetchWillLoop() )
		m_VorbisState.TremorInfo.VorbisDSPState.state.extra_samples_end = m_VorbisState.VorbisInfo.uLastGranuleExtra;

	return eResult;
}

// VirtualOn
void CAkSrcFileVorbis::VirtualOn( AkVirtualQueueBehavior eBehavior )
{
	CAkSrcFileBase::VirtualOn( eBehavior );

	if ( eBehavior == AkVirtualQueueBehavior_FromBeginning || eBehavior == AkVirtualQueueBehavior_FromElapsedTime )
	{
		vorbis_dsp_clear(&m_VorbisState.TremorInfo.VorbisDSPState);
		FreeStitchBuffer();
		FreeOutputBuffer();
	}
}

// VirtualOff
AKRESULT CAkSrcFileVorbis::VirtualOff( AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset )
{
	AKRESULT eResult = CAkSrcFileBase::VirtualOff( eBehavior, in_bUseSourceOffset );

	AKASSERT( m_VorbisState.TremorInfo.ReturnInfo.eDecoderState >= PACKET_STREAM );
	
	if ( eBehavior == AkVirtualQueueBehavior_FromBeginning || eBehavior == AkVirtualQueueBehavior_FromElapsedTime )
	{
		if (vorbis_dsp_init(&m_VorbisState.TremorInfo.VorbisDSPState, m_VorbisState.TremorInfo.VorbisDSPState.channels) == -1)
			return AK_Fail;	//Could not allocate the buffers again.  Kill this voice.
		AkUInt32 uSrcOffsetRemainder = m_pCtx->GetSourceOffsetRemainder();
		m_pCtx->SetSourceOffsetRemainder( 0 );
		m_uCurSample += uSrcOffsetRemainder;

		VorbisDSPRestart( uSrcOffsetRemainder );
	}

	return eResult;
}

//================================================================================
// Decoding of seek table and 3 Vorbis headers
//================================================================================
AKRESULT CAkSrcFileVorbis::DecodeVorbisHeader( )
{
	// Try to get the setup, comment and codebook headers and set up the Vorbis decoder
	// Any error while decoding header is fatal.
	while( m_VorbisState.TremorInfo.ReturnInfo.eDecoderState < PACKET_STREAM ) 
	{
		// Exit if no data left
		if ( m_ulSizeLeft == 0 )
		{
			return AK_FormatNotReady;
		}

		// Read seek table
		if ( m_VorbisState.TremorInfo.ReturnInfo.eDecoderState < SEEKTABLEINTIALIZED )
		{
			if ( m_VorbisState.uSeekTableSizeRead < m_VorbisState.VorbisInfo.dwSeekTableSize )
			{
				AkUInt32 uCopySize = AkMin( m_ulSizeLeft, m_VorbisState.VorbisInfo.dwSeekTableSize - m_VorbisState.uSeekTableSizeRead );
				
				// Read all seek table items
				// Note: Always copy seek table if it exists.
				AKASSERT( m_VorbisState.pSeekTable != NULL && m_VorbisState.VorbisInfo.dwSeekTableSize > 0 );
				AkMemCpy( (AkUInt8*)m_VorbisState.pSeekTable + m_VorbisState.uSeekTableSizeRead, m_pNextAddress, uCopySize ); 

				m_VorbisState.uSeekTableSizeRead += uCopySize;
				ConsumeData( uCopySize );
			}

			if ( m_VorbisState.uSeekTableSizeRead == m_VorbisState.VorbisInfo.dwSeekTableSize )
				m_VorbisState.TremorInfo.ReturnInfo.eDecoderState = SEEKTABLEINTIALIZED;
		}

		// Read Vorbis header packets
		while ( m_VorbisState.TremorInfo.ReturnInfo.eDecoderState == SEEKTABLEINTIALIZED ) 
		{
			// Get the next packet
			ogg_packet OggPacket;
			AKRESULT eResult = GetNextPacket();
			if ( eResult == AK_NoDataReady ) 
			{
				return AK_FormatNotReady;
			}
			else if ( eResult == AK_Fail || eResult == AK_NoMoreData || eResult == AK_InsufficientMemory )
			{
				return AK_Fail;
			}
			else
			{
				AKASSERT( eResult == AK_DataReady );

				UnpackPacket(m_pOggPacketData, m_pOggPacketData+sizeof(OggPacketHeader), false, OggPacket);

				m_VorbisState.TremorInfo.VorbisDSPState.csi = g_VorbisCodebookMgr.Decodebook(m_VorbisState, m_pCtx, &OggPacket);
				if(!m_VorbisState.TremorInfo.VorbisDSPState.csi )
					return AK_Fail;
			
				m_VorbisState.TremorInfo.ReturnInfo.eDecoderState = PACKET_STREAM;
				
				FreeStitchBuffer();
			}
		}
	}

	// Only get here once all three headers are parsed, complete Vorbis setup
	AKASSERT( m_VorbisState.TremorInfo.ReturnInfo.eDecoderState == PACKET_STREAM );

	// Initialize global decoder state
	AkInt32 iResult = vorbis_dsp_init(&m_VorbisState.TremorInfo.VorbisDSPState, m_VorbisState.TremorInfo.channelConfig.uNumChannels );
	if ( iResult )
	{
		// DO NOT ASSERT! Can fail because of failed _ogg_malloc(). AKASSERT( !"Failure initializing Vorbis decoder." );
		return AK_Fail;
	}

    return AK_Success;
}

void CAkSrcFileVorbis::InitVorbisState( )
{
	memset(&m_VorbisState, 0, sizeof(AkVorbisSourceState));
}

void CAkSrcFileVorbis::TermVorbisState( )
{
	vorbis_dsp_clear( &m_VorbisState.TremorInfo.VorbisDSPState );
}

AKRESULT CAkSrcFileVorbis::InitVorbisInfo()
{
	if ( m_VorbisState.VorbisInfo.dwSeekTableSize )
	{
		// Allocate seek table
		m_VorbisState.pSeekTable = (AkVorbisSeekTableItem *) AkAlloc( AkMemID_Processing, m_VorbisState.VorbisInfo.dwSeekTableSize );
		if ( m_VorbisState.pSeekTable == NULL )
			return AK_InsufficientMemory;
	}

	m_VorbisState.TremorInfo.ReturnInfo.eDecoderState = INITIALIZED;

	return AK_Success;
}

//================================================================================
//================================================================================
AKRESULT CAkSrcFileVorbis::ProcessFirstBuffer()
{
	EnterPreBufferingState();
	
	AKASSERT( m_ulSizeLeft == 0 );
	AkUInt8 * pBuffer;
    AKRESULT eResult = m_pStream->GetBuffer(
                            (void*&)pBuffer,	// Address of granted data space.
                            m_ulSizeLeft,		// Size of granted data space.
                            AK_PERF_OFFLINE_RENDERING );

	if ( eResult == AK_NoDataReady )
    {
        // Not ready. Leave.
        return AK_FormatNotReady;
    }
    else if ( eResult != AK_DataReady && eResult != AK_NoMoreData )
    {
        // IO error.
        return AK_Fail;
    }

    if ( m_VorbisState.TremorInfo.ReturnInfo.eDecoderState == UNINITIALIZED )
	{
		// Parse header. 
		eResult = ParseHeader( pBuffer );
		if ( AK_EXPECT_FALSE( eResult != AK_Success ) )
			return eResult;

		AKASSERT( m_uTotalSamples && m_uDataSize );

		LoopInit();

		// Process buffer
		eResult = ProcessStreamBuffer( pBuffer );
		if ( AK_EXPECT_FALSE( eResult != AK_Success ) )
			return eResult;

		// "Consume" header.
		AKASSERT( m_ulSizeLeft >= m_uDataOffset || !"Header must be entirely contained within first stream buffer" );
		ConsumeData( m_uDataOffset );
	}
	else
	{
		// Process buffer
		eResult = ProcessStreamBuffer( pBuffer );
		if ( AK_EXPECT_FALSE( eResult != AK_Success ) )
			return eResult;
	}

	AKASSERT( m_VorbisState.TremorInfo.ReturnInfo.eDecoderState < PACKET_STREAM );

	// Need to setup headers first
	eResult = DecodeVorbisHeader(); 
	if ( eResult != AK_Success )
	{
		if ( eResult == AK_FormatNotReady && m_ulSizeLeft == 0 )
		{
			ReleaseStreamBuffer();
			m_pNextAddress = NULL;
		}
		return eResult;
	}

	AkUInt32 uSrcOffsetRemainder = 0;

	if( m_pCtx->RequiresSourceSeek() )
	{
		eResult = SeekToSourceOffset();

		// Flush streamed input.
		if ( m_ulSizeLeft != 0 )
		{
			ReleaseStreamBuffer();
			m_pNextAddress = NULL;
			m_ulSizeLeft = 0;
		}

		uSrcOffsetRemainder = m_pCtx->GetSourceOffsetRemainder();
		m_uCurSample += uSrcOffsetRemainder;
		m_pCtx->SetSourceOffsetRemainder( 0 );
	}

	VorbisDSPRestart( uSrcOffsetRemainder );

	m_bFormatReady = true;
	return eResult;
}
//================================================================================
// ParseHeader
// Parse header information
//================================================================================
AKRESULT CAkSrcFileVorbis::ParseHeader( 
	AkUInt8 * in_pBuffer	// Buffer to parse
	)
{
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

	//Check if the file is of the expected format
	if (fmtInfo.pFormat->wFormatTag != AK_WAVE_FORMAT_VORBIS)
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_FileFormatMismatch, m_pCtx );
		return AK_InvalidFile;
	}

	// Vorbis expects a WaveFormatVorbis
	WaveFormatVorbis * pFmt = (WaveFormatVorbis *)fmtInfo.pFormat;
	AKASSERT( fmtInfo.uFormatSize == sizeof( WaveFormatVorbis ) );

	//Setup format on the PBI
	AkChannelConfig channelConfig = pFmt->GetChannelConfig();
	AkAudioFormat format;

	format.SetAll(
		pFmt->nSamplesPerSec, 
		channelConfig, 
		32, 
		pFmt->nChannels * sizeof(AkReal32),
		AK_FLOAT,							
		AK_NONINTERLEAVED);		

	m_pCtx->SetMediaFormat(format);

	// Store analysis data if it was present. 
	// Note: StoreAnalysisData may fail, but it is not critical.
	if ( analysisDataChunk.uDataSize > 0 )
		StoreAnalysisData( analysisDataChunk );

	m_uTotalSamples = pFmt->vorbisHeader.dwTotalPCMFrames;
	
	m_VorbisState.VorbisInfo = pFmt->vorbisHeader;
	m_VorbisState.VorbisInfo.uMaxPacketSize += 8;	//Need 8 bytes to ensure no 64 bit reads go beyond the buffer allocated by the streaming mgr.

	m_VorbisState.TremorInfo.channelConfig = channelConfig;
	m_VorbisState.uSampleRate = pFmt->nSamplesPerSec;

	// Fix loop start and loop end for no SMPL chunk
	if((m_uPCMLoopStart == 0) && (m_uPCMLoopEnd == 0))
	{	
		m_uPCMLoopEnd = m_uTotalSamples-1;
	}

	// Loop points. If not looping or ulLoopEnd is 0 (no loop points),
    // set loop points to data chunk boundaries.
    if ( DoLoop() )
    {   
		// NOTE: Disregard loop points contained in Fmt chunk and use VorbisInfo instead
		m_ulLoopStart = m_VorbisState.VorbisInfo.LoopInfo.dwLoopStartPacketOffset + m_uDataOffset + m_VorbisState.VorbisInfo.dwSeekTableSize;
		m_ulLoopEnd = m_VorbisState.VorbisInfo.LoopInfo.dwLoopEndPacketOffset + m_uDataOffset + m_VorbisState.VorbisInfo.dwSeekTableSize;
    }
	else
	{
		m_ulLoopStart = m_uDataOffset + m_VorbisState.VorbisInfo.dwVorbisDataOffset; // VorbisDataOffset == seek table + vorbis header
		m_ulLoopEnd = m_uDataOffset + m_uDataSize;
	}

	AKASSERT( m_pStream );
	// Update stream heuristics.
	AkAutoStmHeuristics heuristics;
    m_pStream->GetHeuristics( heuristics );
	GetStreamLoopHeuristic( DoLoop(), heuristics );
	heuristics.fThroughput = pFmt->nAvgBytesPerSec / 1000.f;	// Throughput (bytes/ms)
    heuristics.priority = m_pCtx->GetPriority();	// Priority.
    m_pStream->SetHeuristics( heuristics );

	eResult = InitVorbisInfo();
	if ( eResult != AK_Success )
		return eResult;

	// Update stream buffering settings.
	return m_pStream->SetMinimalBufferSize( 1 );
}

// Finds the closest offset in file that corresponds to the desired sample position.
// Returns the file offset (in bytes, relative to the beginning of the file) and its associated sample position.
AKRESULT CAkSrcFileVorbis::FindClosestFileOffset( 
	AkUInt32 in_uDesiredSample,		// Desired sample position in file.
	AkUInt32 & out_uSeekedSample,	// Returned sample where file position was set.
	AkUInt32 & out_uFileOffset		// Returned file offset where file position was set.
	)
{
#ifndef AK_OPTIMIZED
	if ( !HasSeekTable() && ( m_uTotalSamples > AK_VORBIS_SEEK_TABLE_WARN_SIZE ) )
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_VorbisSeekTableRecommended, m_pCtx );
	}
#endif

	VorbisSeek( m_VorbisState, in_uDesiredSample, out_uSeekedSample, out_uFileOffset );
	out_uFileOffset += m_uDataOffset;

	return AK_Success;
}

void CAkSrcFileVorbis::FreeStitchBuffer()
{
	if (m_bStitching && m_pOggPacketData)
	{
		AkFalign( AkMemID_Processing, m_pOggPacketData );
		m_pOggPacketData = NULL;
		m_bStitching = false;
		m_uPacketHeaderGathered = 0;
		m_uPacketDataGathered = 0;
	}
}

void CAkSrcFileVorbis::FreeOutputBuffer()
{
	if (m_pOutputBuffer)
	{
		AkFalign(AkMemID_Processing, m_pOutputBuffer);
		m_pOutputBuffer = NULL;
		m_uOutputBufferSize = 0;
	}
}

//GetNextPacket ensures that there is at least one full packet.  There may be more data to process though.
AKRESULT CAkSrcFileVorbis::GetNextPacket()
{
	while ( true )
	{
		AKASSERT( m_pStream != NULL );

		if ( m_ulSizeLeft == 0 )
		{
			// If we don't have data unconsumed and we will never get any more we are done decoding
			if ( HasNoMoreStreamData() )
				return AK_NoMoreData;

			// See if we can get more data from stream manager.
			ReleaseStreamBuffer();
			AKRESULT eStmResult = FetchStreamBuffer();
			if ( eStmResult != AK_DataReady )
				return eStmResult;			
		}

		bool bHeaderReady = false;	

		// Gather header
		if ( m_uPacketHeaderGathered < sizeof(OggPacketHeader) )
		{
			if ( m_ulSizeLeft > 0 )
			{
				// Accumulate data into packet header
				AkUInt32 uCopySize = AkMin( m_ulSizeLeft, sizeof(OggPacketHeader)-m_uPacketHeaderGathered );
				if (uCopySize != sizeof(OggPacketHeader))
				{					
					//We'll need to stitch, allocate a stitch buffer.
					if (!m_bStitching)
					{
						//Our Vorbis version reads chunks of 64 bits.  uMaxPacketSize was already increased by 7 bytes.
						AkUInt32 uAlignedSize = (m_VorbisState.VorbisInfo.uMaxPacketSize + sizeof(OggPacketHeader));	
						m_pOggPacketData = (AkUInt8*)AkMalign(AkMemID_Processing, uAlignedSize, 8);
						if ( m_pOggPacketData == NULL )
						{
							return AK_InsufficientMemory;
						}
						m_bStitching = true;
					}
					AkMemCpy( m_pOggPacketData+m_uPacketHeaderGathered, m_pNextAddress, uCopySize );					
				}				
				else
					m_pOggPacketData = m_pNextAddress;

				m_uPacketHeaderGathered += uCopySize;
				ConsumeData( uCopySize );
				bHeaderReady = m_uPacketHeaderGathered == sizeof(OggPacketHeader);
			}
		}
		else
		{
			bHeaderReady = true;
		}

		// Gather packet data
		if ( bHeaderReady )
		{					
			if (m_uPacketDataGathered == 0 && m_ulSizeLeft < (AkUInt32)CurrentPacketSize() + 8 && !m_bStitching)
			{
				//We'll need to stitch, allocate a stitch buffer.
				//Our Vorbis version reads chunks of 64 bits.  Make sure we have an extra 8 bytes as scratch memory
				AkUInt32 uAlignedSize = CurrentPacketSize() + sizeof(OggPacketHeader) + 8;
				AkUInt8* pStitch = (AkUInt8*)AkMalign(AkMemID_Processing, uAlignedSize, 8);				
				if ( pStitch == NULL )
				{
					return AK_InsufficientMemory;
				}
				AkMemCpy( pStitch, m_pOggPacketData, sizeof(OggPacketHeader) );

				m_pOggPacketData = pStitch;
				m_bStitching = true;
			}

			AKASSERT( m_uPacketDataGathered <= CurrentPacketSize() );
			if ( m_uPacketDataGathered < CurrentPacketSize() && m_ulSizeLeft > 0 )
			{
				// Accumulate data into packet
				AkUInt32 uCopySize = AkMin( m_ulSizeLeft, CurrentPacketSize()-m_uPacketDataGathered );

				if (m_bStitching)
					AkMemCpy( m_pOggPacketData+m_uPacketDataGathered+sizeof(OggPacketHeader), m_pNextAddress, uCopySize );

				m_uPacketDataGathered += uCopySize;
				ConsumeData( uCopySize );
			}
		}

		if ( m_uPacketHeaderGathered == sizeof(OggPacketHeader) && m_uPacketDataGathered == CurrentPacketSize() )
		{
			// Prepare for next GetNextPacket() 
			m_uPacketHeaderGathered = 0;
			m_uPacketDataGathered = 0;
			return AK_DataReady;	// Packet ready
		}
		// Not a full packet yet, keep gathering
	}
}

void CAkSrcFileVorbis::LoopInit()
{
	m_uCurSample = 0;

	// Init state.
	ResetLoopCnt();
}

AKRESULT CAkSrcFileVorbis::ChangeSourcePosition()
{
	AKRESULT eResult = CAkSrcFileBase::ChangeSourcePosition();

	if ( eResult == AK_Success )
	{
		FreeStitchBuffer();
		AkUInt32 uSrcOffsetRemainder = m_pCtx->GetSourceOffsetRemainder();
		m_pCtx->SetSourceOffsetRemainder( 0 );
		m_uCurSample += uSrcOffsetRemainder;

		VorbisDSPRestart( uSrcOffsetRemainder );
	}

	return eResult;
}

AKRESULT CAkVorbisFileCodec::DecodeFile(AkUInt8 * pDst, AkUInt32 uDstSize, AkUInt8 * pSrc, AkUInt32 uSrcSize, AkUInt32 &out_uSizeWritten)
{
	out_uSizeWritten = 0;

	AkFileParser::FormatInfo fmtInfo;
	AkFileParser::AnalysisDataChunk analysisDataChunk;
	AkUInt32 uPCMLoopStart, uPCMLoopEnd, uDataSize, uDataOffset;
	AKRESULT eResult = AkFileParser::Parse( pSrc,
											uSrcSize,
											fmtInfo,
											NULL,
											&uPCMLoopStart,
											&uPCMLoopEnd,
											&uDataSize,
											&uDataOffset,
											&analysisDataChunk,
											NULL );

	if ( eResult != AK_Success )
	{
		return eResult;
	}

	//Check if the file is of the expected format
	if (fmtInfo.pFormat->wFormatTag != AK_WAVE_FORMAT_VORBIS)
	{
		return AK_InvalidFile;
	}

	WaveFormatVorbis * pFmt = (WaveFormatVorbis *)fmtInfo.pFormat;
	AKASSERT( fmtInfo.uFormatSize == sizeof( WaveFormatVorbis ) );

	AkChannelConfig channelConfig = pFmt->GetChannelConfig();
	AkVorbisSourceState	vorbisState;
	vorbisState.VorbisInfo = pFmt->vorbisHeader;
	vorbisState.uSampleRate = pFmt->nSamplesPerSec;
	vorbisState.TremorInfo.channelConfig = channelConfig;

	pSrc += uDataOffset;
	uSrcSize -= uDataOffset;

	if ( vorbisState.VorbisInfo.dwSeekTableSize )
	{
		pSrc += vorbisState.VorbisInfo.dwSeekTableSize;
		uSrcSize -= vorbisState.VorbisInfo.dwSeekTableSize;
	}

	vorbisState.TremorInfo.ReturnInfo.eDecoderState = INITIALIZED;

	// Get the next packet, which contains the codebook
	ogg_packet Packet;
	UnpackPacket(pSrc,pSrc+sizeof(OggPacketHeader),false,Packet);
	pSrc += sizeof(OggPacketHeader) + Packet.buffer.size;
	uSrcSize -= sizeof(OggPacketHeader) + Packet.buffer.size;

	// Synthesize Vorbis header
	vorbisState.TremorInfo.VorbisDSPState.csi = g_VorbisCodebookMgr.Decodebook(vorbisState, NULL, &Packet);
	if (!vorbisState.TremorInfo.VorbisDSPState.csi)
	{
		return AK_Fail;
	}

	// Initialize global decoder state
	AkInt32 iResult = vorbis_dsp_init( &(vorbisState.TremorInfo.VorbisDSPState), vorbisState.TremorInfo.channelConfig.uNumChannels );
	if ( iResult )
	{
		return AK_Fail;
	}

	vorbisState.TremorInfo.ReturnInfo.eDecoderState = PACKET_STREAM;
	vorbis_dsp_restart(&vorbisState.TremorInfo.VorbisDSPState, 0, vorbisState.VorbisInfo.uLastGranuleExtra);

	AkUInt32 totalFramesDecoded = 0;
	AkUInt32 uNumChannels = fmtInfo.pFormat->nChannels;
	AkUInt16* pDstPCM = (AkUInt16*)pDst;
	AkUInt32 uDstPCMSize = uDstSize / 2;
	AkReal32* pBuf = NULL;
	AkUInt32 uBufSize = 0;
	bool bContinue = true;
	while (bContinue)
	{
		AKASSERT( vorbisState.TremorInfo.ReturnInfo.eDecoderState >= PACKET_STREAM );

		vorbisState.TremorInfo.uInputDataSize = uSrcSize;
		vorbisState.TremorInfo.bNoMoreInputPackets = true;

		vorbisState.TremorInfo.ReturnInfo.uFramesProduced = 0;
		DecodeVorbis(&vorbisState.TremorInfo, vorbisState.VorbisInfo.uMaxPacketSize, pSrc, pBuf, uBufSize);

		eResult = vorbisState.TremorInfo.ReturnInfo.eDecoderStatus;
		if (eResult == AK_Fail)
		{
			bContinue = false;
		}
		else
		{
			AkUInt32 framesProduced = vorbisState.TremorInfo.ReturnInfo.uFramesProduced;
			AkUInt32 progress = framesProduced * uNumChannels;
			AKASSERT(progress <= uDstPCMSize);

			for (size_t i = 0; i < framesProduced; i++)
			{
				for (size_t c = 0; c < uNumChannels; c++)
				{
					pDstPCM[i * uNumChannels + c] = FLOAT_TO_INT16(pBuf[i + c * framesProduced]);
				}
			}

			vorbisState.TremorInfo.uRequestedFrames = framesProduced;
			pDstPCM += progress;
			uDstPCMSize -= progress;
			pSrc += vorbisState.TremorInfo.ReturnInfo.uInputBytesConsumed;
			uSrcSize -= vorbisState.TremorInfo.ReturnInfo.uInputBytesConsumed;
			out_uSizeWritten += progress * sizeof(AkUInt16);

			totalFramesDecoded += framesProduced;
			if (totalFramesDecoded == pFmt->vorbisHeader.dwTotalPCMFrames)
			{
				bContinue = false;
				eResult = AK_Success;
			}
		}
	}

	vorbis_dsp_clear( &vorbisState.TremorInfo.VorbisDSPState );
	if (pBuf)
	{
		AkFalign(AkMemID_Processing, pBuf);
	}
	if (vorbisState.TremorInfo.VorbisDSPState.csi)
	{
		g_VorbisCodebookMgr.ReleaseCodebook(vorbisState);
	}
	return eResult;
}
