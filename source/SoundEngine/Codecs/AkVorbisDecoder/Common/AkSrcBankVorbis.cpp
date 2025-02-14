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
#include "AkSrcBankVorbis.h"
#include "AkMonitor.h"
#include "AkVorbisCodebookMgr.h"

// Constructor
CAkSrcBankVorbis::CAkSrcBankVorbis( CAkPBI * in_pCtx ) 
: CAkSrcBaseEx( in_pCtx )
, m_pucData( NULL )		// current data pointer
, m_pucDataStart( NULL )// start of audio data
, m_pOutputBuffer( NULL )
, m_uOutputBufferSize( 0 )
{
	// do this here as well as it is legal to be StopStream'ed
	// without having been StartStream'ed
	InitVorbisState();
}

// Destructor
CAkSrcBankVorbis::~CAkSrcBankVorbis()
{
	AKASSERT(!m_pOutputBuffer);
	if ( m_VorbisState.TremorInfo.VorbisDSPState.csi )
		g_VorbisCodebookMgr.ReleaseCodebook( m_VorbisState );
}

// GetBuffer
void CAkSrcBankVorbis::GetBuffer( AkVPLState & io_state )
{
	// The stream should not start before all headers are decoded
	AKASSERT( m_VorbisState.TremorInfo.ReturnInfo.eDecoderState >= PACKET_STREAM );

	// Trim input data to loop end if necessary
	m_VorbisState.TremorInfo.uInputDataSize =  GetMaxInputDataSize();
	m_VorbisState.TremorInfo.bNoMoreInputPackets = true;

	DecodeVorbis(&m_VorbisState.TremorInfo, m_VorbisState.VorbisInfo.uMaxPacketSize, m_pucData, m_pOutputBuffer, m_uOutputBufferSize);
	
	io_state.result = m_VorbisState.TremorInfo.ReturnInfo.eDecoderStatus;

	if ( AK_EXPECT_FALSE( io_state.result == AK_Fail ) )
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_VorbisDecodeError, m_pCtx );
		return;
	}

	// Advance m_pucData based on how many packets were consumed
	m_pucData += m_VorbisState.TremorInfo.ReturnInfo.uInputBytesConsumed;
	m_VorbisState.TremorInfo.uRequestedFrames = m_VorbisState.TremorInfo.ReturnInfo.uFramesProduced;

	// Prepare buffer for VPL, update PCM position, handle end of loop/file, post markers and position info.
	SubmitBufferAndUpdate( 
		m_pOutputBuffer,
		(AkUInt16)m_VorbisState.TremorInfo.ReturnInfo.uFramesProduced, 
		m_VorbisState.uSampleRate,
		m_VorbisState.TremorInfo.channelConfig, 
		io_state );
}

// ReleaseBuffer
void CAkSrcBankVorbis::ReleaseBuffer()
{
	m_VorbisState.TremorInfo.ReturnInfo.uFramesProduced = 0;
	// Preserve output buffer allocation to avoid allocator churn
	// FreeOutputBuffer();
}

// StartStream
AKRESULT CAkSrcBankVorbis::StartStream( AkUInt8 * pvBuffer, AkUInt32 ulBufferSize )
{
    AKASSERT( m_markers.m_hdrMarkers.uNumMarkers == 0 && m_markers.m_pMarkers == NULL ); 

	if ( pvBuffer == NULL )
	{
		return AK_Fail;
	}

	AkFileParser::FormatInfo fmtInfo;
	AkFileParser::AnalysisDataChunk analysisDataChunk;
	AKRESULT eResult = AkFileParser::Parse( pvBuffer,			// Data buffer.
											ulBufferSize,		// Buffer size.
											fmtInfo,			// Returned audio format info.
											&m_markers,			// Markers.
											&m_uPCMLoopStart,	// Beginning of loop.
											&m_uPCMLoopEnd,		// End of loop (inclusive).
											&m_uDataSize,		// Data size.
											&m_uDataOffset,		// Offset to data.
											&analysisDataChunk,	// Analysis info.
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

	if ( m_pCtx )
		m_pCtx->SetMediaFormat(format);

	if ( analysisDataChunk.uDataSize > 0 )
		m_pAnalysisData = analysisDataChunk.pData;

	m_pucDataStart = pvBuffer + m_uDataOffset;

	m_uTotalSamples = pFmt->vorbisHeader.dwTotalPCMFrames;
	
	m_VorbisState.VorbisInfo = pFmt->vorbisHeader;

	m_VorbisState.uSampleRate = pFmt->nSamplesPerSec;
	m_VorbisState.TremorInfo.channelConfig = channelConfig;

	// Fix loop start and loop end for no SMPL chunk
	if ( m_uPCMLoopEnd == 0 )
	{	
		m_uPCMLoopEnd = m_uTotalSamples-1;
	}

	// Verify data buffer consistency.
	if ( m_uPCMLoopEnd < m_uPCMLoopStart 
		|| m_uPCMLoopEnd >= m_uTotalSamples
		|| ulBufferSize != (m_uDataOffset + m_uDataSize) )
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_InvalidAudioFileHeader, m_pCtx );
		return AK_Fail;
	}

	m_pucData = m_pucDataStart;
	LoopInit();

	eResult = DecodeVorbisHeader();

	if( eResult == AK_Success )
	{
		AkUInt32 uSrcOffsetRemainder = 0;

		if( m_pCtx && m_pCtx->RequiresSourceSeek() )
		{
			eResult = SeekToNativeOffset();
			uSrcOffsetRemainder = m_pCtx->GetSourceOffsetRemainder();
			m_uCurSample += uSrcOffsetRemainder;
			m_pCtx->SetSourceOffsetRemainder( 0 );
		}

		VorbisDSPRestart( uSrcOffsetRemainder );
	}

	return eResult;
}

bool CAkSrcBankVorbis::SupportMediaRelocation() const
{
	return true;
}

AKRESULT CAkSrcBankVorbis::RelocateMedia( AkUInt8* in_pNewMedia, AkUInt8* in_pOldMedia )
{
	AkUIntPtr uMemoryOffset = (AkUIntPtr)in_pNewMedia - (AkUIntPtr)in_pOldMedia;
	m_pucData = m_pucData + uMemoryOffset;
	m_pucDataStart = m_pucDataStart + uMemoryOffset;

	return AK_Success;
}

AKRESULT CAkSrcBankVorbis::Seek( AkUInt32 in_SourceOffset )
{
	// Only allow seek index when source context is null
	AKASSERT( m_pCtx == NULL );

	// Compute new file offset and seek stream.
	AkUInt32 uSrcOffsetRemainder = 0;
	AKRESULT eResult = SeekToNativeOffset( in_SourceOffset, uSrcOffsetRemainder );
	m_uCurSample += uSrcOffsetRemainder;
	VorbisDSPRestart( uSrcOffsetRemainder );
	return eResult;
}

// StopStream
void CAkSrcBankVorbis::StopStream()
{
	TermVorbisState();

	ReleaseBuffer();
	if ( m_VorbisState.pSeekTable )
	{
		AkFree( AkMemID_Processing, m_VorbisState.pSeekTable );
		m_VorbisState.pSeekTable = NULL;
	}
	FreeOutputBuffer();

	CAkSrcBaseEx::StopStream();
}

AKRESULT CAkSrcBankVorbis::StopLooping()
{
	m_VorbisState.TremorInfo.VorbisDSPState.state.extra_samples_end = m_VorbisState.VorbisInfo.uLastGranuleExtra;
	return CAkSrcBaseEx::StopLooping();
}

void CAkSrcBankVorbis::VirtualOn( AkVirtualQueueBehavior eBehavior )
{
	if ( eBehavior == AkVirtualQueueBehavior_FromBeginning || eBehavior == AkVirtualQueueBehavior_FromElapsedTime )
	{
		vorbis_dsp_clear(&m_VorbisState.TremorInfo.VorbisDSPState);
		FreeOutputBuffer();
	}
	CAkSrcBaseEx::VirtualOn(eBehavior);
}

// VirtualOff
AKRESULT CAkSrcBankVorbis::VirtualOff( AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset )
{
	if ( eBehavior == AkVirtualQueueBehavior_FromBeginning || eBehavior == AkVirtualQueueBehavior_FromElapsedTime )
	{
		if (vorbis_dsp_init(&m_VorbisState.TremorInfo.VorbisDSPState, m_VorbisState.TremorInfo.VorbisDSPState.channels) == -1)
			return AK_Fail;	//Could not allocate the buffers again.  Kill this voice.
	}

	AKRESULT eResult = AK_Success;
	AkUInt32 uSrcOffsetRemainder = 0;

	AKASSERT( m_VorbisState.TremorInfo.ReturnInfo.eDecoderState >= PACKET_STREAM );

	if ( eBehavior == AkVirtualQueueBehavior_FromElapsedTime )
	{
		if ( !in_bUseSourceOffset )
		{
			VirtualSeek( m_uCurSample );
		}
		else
		{
			eResult = SeekToNativeOffset();	// Use source offset specified by behavioral engine.
		}

		uSrcOffsetRemainder = m_pCtx->GetSourceOffsetRemainder();
		m_pCtx->SetSourceOffsetRemainder( 0 );
		m_uCurSample += uSrcOffsetRemainder;
	}
	else if ( eBehavior == AkVirtualQueueBehavior_FromBeginning ) 
	{
		// Setup completed go to data directly
		m_pucData = m_pucDataStart + m_VorbisState.VorbisInfo.dwVorbisDataOffset;
		LoopInit();
	}
	else
	{
		// Nothing to do for resume mode
		return AK_Success;
	}

	VorbisDSPRestart( uSrcOffsetRemainder );

	return eResult;
}

// VirtualSeek - Determine where to seek
AKRESULT CAkSrcBankVorbis::VirtualSeek( AkUInt32 & io_uSeekPosition )
{
#ifndef AK_OPTIMIZED
	if ( !HasSeekTable() && ( m_uTotalSamples > AK_VORBIS_SEEK_TABLE_WARN_SIZE ) )
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_VorbisSeekTableRecommended, m_pCtx );
	}
#endif

	AkUInt32 uFileOffset;
	VorbisSeek( m_VorbisState, io_uSeekPosition, io_uSeekPosition, uFileOffset );
	m_pucData = m_pucDataStart + uFileOffset;
	return AK_Success;
}

//================================================================================
// Decoding of seek table and 3 Vorbis headers
//================================================================================
AKRESULT CAkSrcBankVorbis::DecodeVorbisHeader()
{
	AKASSERT( m_VorbisState.TremorInfo.ReturnInfo.eDecoderState == UNINITIALIZED ); 

	AKRESULT eResult = InitVorbisInfo();
	if( eResult != AK_Success )
	{
		return eResult;
	}

	// Note: Always copy seek table if it exists.
	// Read all seek table items
	if ( m_VorbisState.VorbisInfo.dwSeekTableSize > 0 )
	{
		AKASSERT( m_VorbisState.pSeekTable != NULL );	
		AkMemCpy( m_VorbisState.pSeekTable, m_pucData, m_VorbisState.VorbisInfo.dwSeekTableSize ); 	
		m_pucData += m_VorbisState.VorbisInfo.dwSeekTableSize;
	}

	// Read Vorbis header packets

	// Get the next packet
	ogg_packet Packet;
	UnpackPacket(m_pucData,m_pucData+sizeof(OggPacketHeader),false,Packet);
	m_pucData += sizeof(OggPacketHeader) + Packet.buffer.size;
	
	// Synthesize Vorbis header
	m_VorbisState.TremorInfo.VorbisDSPState.csi = g_VorbisCodebookMgr.Decodebook(m_VorbisState, m_pCtx, &Packet);
	if(!m_VorbisState.TremorInfo.VorbisDSPState.csi)
		return AK_Fail;

	// Only get here once codebook header is parsed, complete Vorbis setup
	// Initialize global decoder state
	AkInt32 iResult = vorbis_dsp_init( &(m_VorbisState.TremorInfo.VorbisDSPState), m_VorbisState.TremorInfo.channelConfig.uNumChannels );
	if ( iResult )
	{
		// DO NOT ASSERT! Can fail because of failed _ogg_malloc(). AKASSERT( !"Failure initializing Vorbis decoder." );
		return AK_Fail;
	}
	
	m_VorbisState.TremorInfo.ReturnInfo.eDecoderState = PACKET_STREAM;

	return AK_Success;
}

void CAkSrcBankVorbis::InitVorbisState( )
{
	memset(&m_VorbisState, 0, sizeof(AkVorbisSourceState));
}

void CAkSrcBankVorbis::TermVorbisState( )
{
	vorbis_dsp_clear( &m_VorbisState.TremorInfo.VorbisDSPState );
}

AKRESULT CAkSrcBankVorbis::InitVorbisInfo()
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

void CAkSrcBankVorbis::LoopInit()
{
	m_uCurSample = 0;

	// Init state.
	ResetLoopCnt();
}

AkUInt32 CAkSrcBankVorbis::GetMaxInputDataSize()
{
	if ( !DoLoop() )
	{
		return (AkUInt32)( m_pucDataStart + m_uDataSize - m_pucData );
	}
	else
	{
		// Seek table is inserted in front of encoded data so we need to offset by SeekTable size
		return (AkUInt32) ( m_pucDataStart + m_VorbisState.VorbisInfo.LoopInfo.dwLoopEndPacketOffset + m_VorbisState.VorbisInfo.dwSeekTableSize - m_pucData );
	}
}

void CAkSrcBankVorbis::FreeOutputBuffer()
{
	if (m_pOutputBuffer)
	{
		AkFalign(AkMemID_Processing, m_pOutputBuffer);
		m_pOutputBuffer = NULL;
		m_uOutputBufferSize = 0;
	}
}

AKRESULT CAkSrcBankVorbis::SeekToNativeOffset( AkUInt32 in_SourceOffset, AkUInt32& out_SourceOffsetRemainder )
{
	AKASSERT( m_VorbisState.TremorInfo.ReturnInfo.eDecoderState >= PACKET_STREAM );

	// Note: Force using 64 bit calculation to avoid uint32 overflow.
	AkUInt64 uSourceOffset = in_SourceOffset;

	if ( uSourceOffset >= m_uTotalSamples )
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_SeekAfterEof, m_pCtx );
		return AK_Fail;
	}

	AkUInt32 uRealOffset = (AkUInt32)uSourceOffset;
	VirtualSeek( uRealOffset );
	m_uCurSample = uRealOffset;

	// Store error in output.
	AKASSERT( (int)uSourceOffset - (int)uRealOffset >= 0 );
	out_SourceOffsetRemainder = (AkUInt32)uSourceOffset - uRealOffset;

	return AK_Success;
}

// Helper: Seek to source offset obtained from PBI (number of samples at the pipeline's sample rate).
AKRESULT CAkSrcBankVorbis::SeekToNativeOffset()
{
	AKASSERT( m_VorbisState.TremorInfo.ReturnInfo.eDecoderState >= PACKET_STREAM );

	// Note: Force using 64 bit calculation to avoid uint32 overflow.
	AkUInt64 uSourceOffset = GetSourceOffset();

	if ( uSourceOffset >= m_uTotalSamples )
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_SeekAfterEof, m_pCtx );
		return AK_Fail;
	}

	AkUInt32 uRealOffset = (AkUInt32)uSourceOffset;
	VirtualSeek( uRealOffset );
	m_uCurSample = uRealOffset;
	
	// Store error in PBI.
	AKASSERT( (int)uSourceOffset - (int)uRealOffset >= 0 );
	m_pCtx->SetSourceOffsetRemainder( (AkUInt32)uSourceOffset - uRealOffset );

	return AK_Success;
}

AKRESULT CAkSrcBankVorbis::ChangeSourcePosition()
{
	// Compute new file offset and seek stream.
	// Note: The seek time is stored in the PBI's source offset. However it represents the absolute
	// time from the beginning of the sound, taking the loop region into account.

	AKRESULT eResult = SeekToNativeOffset();
	AkUInt32 uSrcOffsetRemainder = m_pCtx->GetSourceOffsetRemainder();
	m_pCtx->SetSourceOffsetRemainder( 0 );
	m_uCurSample += uSrcOffsetRemainder;
	VorbisDSPRestart( uSrcOffsetRemainder );
	return eResult;
}

// Override OnLoopComplete() handler: "restart DSP" (set primimg frames) and fix decoder status
// if it's a loop end, and place input pointer.
AKRESULT CAkSrcBankVorbis::OnLoopComplete(
	bool in_bEndOfFile		// True if this was the end of file, false otherwise.
	)
{
	// IMPORTANT: Call base first. VorbisDSPRestart() checks the loop count, so it has to be updated first.
	AKRESULT eResult = CAkSrcBaseEx::OnLoopComplete( in_bEndOfFile );
	if ( !in_bEndOfFile )
	{
		m_pucData = m_pucDataStart + m_VorbisState.VorbisInfo.LoopInfo.dwLoopStartPacketOffset + m_VorbisState.VorbisInfo.dwSeekTableSize;
		
		VorbisDSPRestart( m_VorbisState.VorbisInfo.LoopInfo.uLoopBeginExtra );

		// Vorbis reported no more data due to end-of-loop; reset the state.
		AKASSERT( m_VorbisState.TremorInfo.ReturnInfo.eDecoderStatus == AK_NoMoreData );
		m_VorbisState.TremorInfo.ReturnInfo.eDecoderStatus = AK_DataReady;
	}
	return eResult;
}

CAkVorbisGrainCodec::CAkVorbisGrainCodec()
	: m_Node(nullptr)
{
}

AKRESULT CAkVorbisGrainCodec::Init(AkUInt8 * in_pBuffer, AkUInt32 in_pBufferSize)
{
	AKRESULT eResult = m_Node.StartStream(in_pBuffer, in_pBufferSize);
	if (eResult != AK_Success)
	{
		// WG-45470 Make sure to free the seek table by calling StopStream()
		m_Node.StopStream();
		return eResult;
	}
	m_Node.SetInfiniteLooping();
	return eResult;
}

void CAkVorbisGrainCodec::GetBuffer(AkAudioBuffer & io_state)
{
	AkVPLState tempState((AkVPLState&)io_state);
	m_Node.GetBuffer(tempState);
	io_state = (AkAudioBuffer)tempState;
}

void CAkVorbisGrainCodec::ReleaseBuffer()
{
	m_Node.ReleaseBuffer();
}

AKRESULT CAkVorbisGrainCodec::SeekTo(AkUInt32 in_SourceOffset)
{
	return m_Node.Seek(in_SourceOffset);
}

void CAkVorbisGrainCodec::Term()
{
	m_Node.Term(CtxDestroyReasonFinished); // Term calls StopStream()
}
