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
#include "AkSrcMediaCodecPCM.h"

using namespace AK::SrcMedia::Stream;

CAkSrcMediaCodecPCM::CAkSrcMediaCodecPCM()
	: m_pStitchBuffer( nullptr )
	, m_uNumBytesBuffered( 0 )
	, m_uSizeToRelease( 0 )
	, m_uBlockAlign( 0 )
{
}

IAkSrcMediaCodec::Result CAkSrcMediaCodecPCM::Init(const AK::SrcMedia::Header &in_header, AK::SrcMedia::CodecInfo &out_codec, AkUInt16 uLoopCnt)
{
	if (in_header.FormatInfo.pFormat->wFormatTag != AK_WAVE_FORMAT_EXTENSIBLE)
	{
		return AK_FileFormatMismatch;
	}
	AKASSERT( in_header.FormatInfo.uFormatSize >= sizeof( WaveFormatExtensible ) );
	AKASSERT( in_header.uDataOffset % 4 == 0 );

	// Format
	WaveFormatExtensible *pFmt = in_header.FormatInfo.pFormat;
	out_codec.format.SetAll(
		pFmt->nSamplesPerSec,
		pFmt->GetChannelConfig(),
		pFmt->wBitsPerSample,
		pFmt->nBlockAlign,
		AK_INT,
		AK_INTERLEAVED);

	out_codec.uSourceSampleRate = pFmt->nSamplesPerSec;

	out_codec.uTotalSamples = in_header.uDataSize / pFmt->nBlockAlign;

	// Loop points. If not looping or ulLoopEnd is 0 (no loop points),
	// set loop points to data chunk boundaries.
	if (0 == in_header.uPCMLoopEnd)
	{
		// Loop start = start of data.
		out_codec.heuristics.uLoopStart = in_header.uDataOffset;
		// Loop end = end of data.
		out_codec.heuristics.uLoopEnd = in_header.uDataOffset + in_header.uDataSize;
		
		// Fix PCM LoopEnd:
		AKASSERT( ( (out_codec.heuristics.uLoopEnd - in_header.uDataOffset ) / pFmt->nBlockAlign ) >= 1 );
	}
	else
	{	
		// If LoopEnd is non zero, then it is in sample frames and the loop should include the end sample.
		// Convert to bytes offset, from beginning of FILE.
		AkUInt32 uiBlockAlign = pFmt->nBlockAlign;

		out_codec.heuristics.uLoopStart = in_header.uDataOffset + uiBlockAlign*in_header.uPCMLoopStart;
		out_codec.heuristics.uLoopEnd = in_header.uDataOffset + uiBlockAlign*(in_header.uPCMLoopEnd+1);
	}

	// Throughput.
	out_codec.heuristics.fThroughput = (AkReal32) ( pFmt->nBlockAlign * pFmt->nSamplesPerSec ) / 1000.f;
	out_codec.uMinimalBufferSize = pFmt->nBlockAlign;

	// Attributes.
	out_codec.uAttributes = AK_CODEC_ATTR_RELOCATE_PITCH;

	m_uBlockAlign = pFmt->nBlockAlign;

	return AK_Success;
}

void CAkSrcMediaCodecPCM::Term()
{
	if ( m_pStitchBuffer )
	{
		AkFree( AkMemID_Processing, m_pStitchBuffer );
		m_pStitchBuffer = nullptr;
		m_uNumBytesBuffered = 0;
	}
}

IAkSrcMediaCodec::Result CAkSrcMediaCodecPCM::GetBuffer(AK::SrcMedia::Stream::State* pStream, AkUInt16 in_uMaxFrames, BufferInfo &out_buffer)
{
	AKRESULT eResult;

	// See if we need to get more data from stream manager.
	if (pStream->uSizeLeft == 0)
	{
		AKASSERT(!HasNoMoreStreamData(pStream));
		eResult = FetchStreamBuffer(pStream);
		if (AK_EXPECT_FALSE(eResult != AK_DataReady))
		{
			return eResult;
		}
	}

	// Deal with NoMoreData return flag.
	AkUInt16 usBlockAlign = m_uBlockAlign;
	// Number of whole sample frames.
	AkUInt32 uFramesLeft = pStream->uSizeLeft / usBlockAlign;

	// At this point, some data is ready, or there is no more data.
	// Maybe loop end or data chunk sizes were invalid.
	if (pStream->uSizeLeft == 0 && HasNoMoreStreamData(pStream))
	{
		AKASSERT(!"Invalid loop back boundary. Wrong values in file header? Source failure.");
		return AK_Fail;
	}

	// Give what the client wants, or what we have.
	AkUInt16 uMaxFrames = (AkUInt16)AkMin(in_uMaxFrames, uFramesLeft);

	void * pSubmitData = NULL;

	// Might need to buffer streamed data if previous sample frame was split between
	// 2 streaming buffers (this should never happen in mono or stereo).
	if (0 == m_uNumBytesBuffered)
	{
		// Using streamed buffer directly: free any previously allocated stitch buffer.
		if (m_pStitchBuffer)
		{
			AkFree(AkMemID_Processing, m_pStitchBuffer);
			m_pStitchBuffer = NULL;
		}

		// Submit streaming buffer directly.
		pSubmitData = pStream->pNextAddress;

		m_uSizeToRelease = uMaxFrames * usBlockAlign;

		// Check if data left after this frame represents less than one whole sample frame.
		// In such a case, buffer it.
		AKASSERT(pStream->uSizeLeft >= m_uSizeToRelease);
		AkUInt32 uSizeLeftAfterRelease = pStream->uSizeLeft - m_uSizeToRelease;
		if (uSizeLeftAfterRelease < usBlockAlign
			&& uSizeLeftAfterRelease > 0)
		{
			// This case should NEVER occur for a memory-type stream
			AKASSERT(!pStream->bIsMemoryStream);
			AKASSERT(!m_pStitchBuffer);
			m_pStitchBuffer = (AkUInt8*)AkAlloc(AkMemID_Processing, usBlockAlign);

			if (m_pStitchBuffer)
			{
				m_uNumBytesBuffered = (AkUInt16)uSizeLeftAfterRelease;
				AKPLATFORM::AkMemCpy(m_pStitchBuffer, pStream->pNextAddress + m_uSizeToRelease, m_uNumBytesBuffered);

				// Increment m_uSizeToRelease a bit so that pointers updates in ReleaseBuffer()
				// take it into account.
				m_uSizeToRelease += m_uNumBytesBuffered;
			}
			else
			{
				// Cannot allocate stitch buffer: This error is unrecoverable. 
				return AK_Fail;
			}
		}
	}
	else
	{
		// This case should NEVER occur for a memory-type stream
		AKASSERT(!pStream->bIsMemoryStream);
		AKASSERT(m_pStitchBuffer);
		AKASSERT(m_uNumBytesBuffered < usBlockAlign);
		AkUInt32 uNumBytesToCopy = usBlockAlign - m_uNumBytesBuffered;
		if (uNumBytesToCopy > pStream->uSizeLeft)
		{
			AKASSERT(!"Streaming granularity is smaller than the size of a sample frame");
			return AK_Fail;
		}

		AKASSERT(m_uNumBytesBuffered + uNumBytesToCopy == usBlockAlign);
		AKPLATFORM::AkMemCpy(m_pStitchBuffer + m_uNumBytesBuffered, pStream->pNextAddress, uNumBytesToCopy);

		// Now that we prepared our stitch buffer for output, recompute data size presented to the pipeline.
		uMaxFrames = 1;

		// Reset state. 
		m_uSizeToRelease = (AkUInt16)uNumBytesToCopy;
		m_uNumBytesBuffered = 0;

		// Submit stitch buffer.
		pSubmitData = m_pStitchBuffer;
	}

	out_buffer.Buffer.AttachInterleavedData(pSubmitData, uMaxFrames, uMaxFrames);
	out_buffer.uSrcFrames = uMaxFrames;

	return uMaxFrames > 0 ? AK_DataReady : AK_NoDataReady;
}

void CAkSrcMediaCodecPCM::ReleaseBuffer(AK::SrcMedia::Stream::State* pStream)
{
	AKASSERT( m_uSizeToRelease <= pStream->uSizeLeft || !"Invalid released data size" );

	ConsumeData(pStream, m_uSizeToRelease);
	m_uSizeToRelease = 0;	

	if ( pStream->uSizeLeft == 0 )
		ReleaseStreamBuffer(pStream);
}

IAkSrcMediaCodec::Result CAkSrcMediaCodecPCM::FindClosestFileOffset(AkUInt32 in_uDesiredSample, SeekInfo & out_SeekInfo)
{
	// IMPORTANT: Never access BlockAlign from PBI format!
	out_SeekInfo.uPCMOffset = in_uDesiredSample;
	out_SeekInfo.uFileOffset = in_uDesiredSample * m_uBlockAlign;
	return AK_Success;
}

IAkSrcMediaCodec::Result CAkSrcMediaCodecPCM::Seek(AK::SrcMedia::Stream::State* pStream, const SeekInfo & seek, AkUInt32 uNumFrames, AkUInt16 uLoopCnt)
{
	m_uSizeToRelease = 0;
	if ( m_pStitchBuffer )
	{
		AkFree( AkMemID_Processing, m_pStitchBuffer );
		m_pStitchBuffer = nullptr;
		m_uNumBytesBuffered = 0;
	}
	return AK_Success;
}

void CAkSrcMediaCodecPCM::VirtualOn()
{
	// Free stitch buffer if playback will be reset
	if ( m_pStitchBuffer )
	{
		AkFree( AkMemID_Processing, m_pStitchBuffer );
		m_pStitchBuffer = NULL;
		m_uNumBytesBuffered = 0;
	}
}
