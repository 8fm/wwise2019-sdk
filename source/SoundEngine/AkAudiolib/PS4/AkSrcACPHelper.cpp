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
#include "AkCommon.h"

#include "AkSrcACPHelper.h"

CAkSkipDecodedFrames::CAkSkipDecodedFrames() :
uSkipStartBufferSamples(0)
, uSkipSamplesLength(0)
, bReadBeforeSkip(false)
{
}

AkUInt32 CAkSkipDecodedFrames::SetSkipFrames(AkUInt32 in_uSkipStartIndex, AkUInt32 in_uSkipLen, bool in_bReadBeforeSkip)
{
	if (uSkipStartBufferSamples == in_uSkipStartIndex && 
		uSkipSamplesLength == in_uSkipLen )
	{
		return in_uSkipLen;
	}
	AKASSERT(uSkipSamplesLength == 0);

#ifdef AT9_STREAM_DEBUG_OUTPUT
		char msg[256];
		sprintf( msg, "-- -- -- -- SetSkipFrames %p in_uSkipStartIndex:%i, in_uSkipLen:%i \n", this, in_uSkipStartIndex, in_uSkipLen);
		AKPLATFORM::OutputDebugMsg( msg );
#endif

	uSkipStartBufferSamples = in_uSkipStartIndex;
	uSkipSamplesLength = in_uSkipLen;

	// Setting skipdata while valid data is already located there in the ringbuffer, read once before skipping
	bReadBeforeSkip = in_bReadBeforeSkip;

	return in_uSkipLen;
}

void CAkSkipDecodedFrames::Reset()
{
	uSkipStartBufferSamples = 0;
	uSkipSamplesLength = 0;
	bReadBeforeSkip = false;
}

CAkDecodingRingbuffer::CAkDecodingRingbuffer() :
m_WriteSampleIndex(0)
, m_ReadSampleIndex(0)
, m_SamplesBeingConsumed(0)
, m_RingbufferUsedSample(0)
, m_TotalSamples(0)
, m_pData(NULL)
, m_pPreviousData(NULL)
{
}

bool CAkDecodingRingbuffer::Create(AkUInt32 in_numSamples, AkUInt32 in_numChannels)
{
	m_pData = (AkUInt16*)AkAlloc( AkMemID_Processing, in_numSamples * in_numChannels * sizeof( AkUInt16 ) ); 
	m_TotalSamples = in_numSamples;

	return m_pData != NULL;
}

AKRESULT CAkDecodingRingbuffer::Grow(AkUInt32 in_newSize, AkUInt32 in_numChannels)
{
	AKASSERT(m_pData != NULL);
	AKASSERT(in_newSize > m_TotalSamples);

	AkUInt16 sample_width = in_numChannels * sizeof( AkUInt16 ); // width of one sample
	AkUInt16* new_pData = (AkUInt16*)AkAlloc( AkMemID_Processing, in_newSize * sample_width ); 
	if (new_pData == NULL)
	{
		return AK_Fail;
	}

	AKPLATFORM::AkMemSet( new_pData, 0, (in_newSize) * sample_width );

	if (m_RingbufferUsedSample > 0)
	{
		if (m_ReadSampleIndex < m_WriteSampleIndex)
		{
			// Straigtfoward case
			// |-----<Read*****Write>-----|
			// to
			// |-----<Read*****Write>----------------|
			AKPLATFORM::AkMemCpy( new_pData, m_pData, m_TotalSamples * sample_width );
		}
		else
		{
			AkUInt16 growSamples = in_newSize - m_TotalSamples;
			// |*****Write>-----<Read******|
			// to
			// |*****Write>---------------<Read******|
			AKPLATFORM::AkMemCpy( new_pData, m_pData, m_WriteSampleIndex * sample_width );

			AkUInt16 new_ReadSampleIndex = m_ReadSampleIndex + growSamples;

			AKPLATFORM::AkMemCpy(
				new_pData + (new_ReadSampleIndex * in_numChannels),
				m_pData + (m_ReadSampleIndex * in_numChannels),
				(m_TotalSamples-m_ReadSampleIndex) * sample_width );

			// If we have skipping samples in the the data section between read pointer and end of ring buffer, we need to move it
			if (m_SkipFrames.uSkipSamplesLength != 0 && m_SkipFrames.uSkipStartBufferSamples > m_ReadSampleIndex)
			{
				m_SkipFrames.uSkipStartBufferSamples += growSamples;
			}
			m_ReadSampleIndex = new_ReadSampleIndex;
		}
	}

	// Data is being consumed by the pipeline, keep it till the next release buffer
	if (m_SamplesBeingConsumed > 0)
	{
		AKASSERT(m_pPreviousData == NULL);
		m_pPreviousData = m_pData;
	}
	else
	{
		AkFree(AkMemID_Processing, m_pData);
	}

	m_pData = new_pData;
	m_TotalSamples = in_newSize;

	return AK_Success;
}

void CAkDecodingRingbuffer::Destroy()
{
	if (m_pData != NULL)
	{
		AkFree( AkMemID_Processing, m_pData ); 
		m_pData = NULL;
	}

	if (m_pPreviousData != NULL)
	{
		AkFree(AkMemID_Processing, m_pPreviousData);
		m_pPreviousData = NULL;
	}
}

void CAkDecodingRingbuffer::Reset()
{
	m_WriteSampleIndex = 0;
	m_ReadSampleIndex = 0;
	m_SamplesBeingConsumed = 0;
	m_RingbufferUsedSample = 0;
	m_SkipFramesStart.Reset();
	m_SkipFrames.Reset();
}

AkInt32 CAkDecodingRingbuffer::SkipFramesIfNeeded(const AkUInt32 in_uReadLength)
{
	AkInt32 framesToRead = in_uReadLength;
	if ( m_SkipFramesStart.uSkipSamplesLength != 0 )
	{
		if ( m_ReadSampleIndex == m_SkipFramesStart.uSkipStartBufferSamples )
		{
#ifdef AT9_STREAM_DEBUG_OUTPUT
			char msg[256];
			sprintf( msg, "-- -- -- -- [%p] Skipping A. %i \n", this, m_SkipFramesStart.uSkipSamplesLength);
			AKPLATFORM::OutputDebugMsg( msg );
#endif
			if (m_SkipFramesStart.bReadBeforeSkip)
			{
				m_SkipFramesStart.bReadBeforeSkip = false;
				return framesToRead;
			}
			// skip at the begining
			m_ReadSampleIndex = ((m_ReadSampleIndex + m_SkipFramesStart.uSkipSamplesLength) % m_TotalSamples);
			
			m_RingbufferUsedSample -= m_SkipFramesStart.uSkipSamplesLength;

			m_SkipFramesStart.uSkipStartBufferSamples = 0;
			m_SkipFramesStart.uSkipSamplesLength = 0;

			if (m_RingbufferUsedSample < framesToRead)
			{
				framesToRead = m_RingbufferUsedSample;
			}
		}
		else if( m_ReadSampleIndex < m_SkipFramesStart.uSkipStartBufferSamples && m_ReadSampleIndex + framesToRead >  m_SkipFramesStart.uSkipStartBufferSamples )
		{
#ifdef AT9_STREAM_DEBUG_OUTPUT
			char msg[256];
			sprintf( msg, "-- -- -- -- [%p] Skipping B. %i \n", this, m_SkipFramesStart.uSkipSamplesLength);
			AKPLATFORM::OutputDebugMsg( msg );
#endif
			if (m_SkipFramesStart.bReadBeforeSkip)
			{
				m_SkipFramesStart.bReadBeforeSkip = false;
				return framesToRead;
			}
			 // skip at the end
			framesToRead = ( m_SkipFramesStart.uSkipStartBufferSamples - m_ReadSampleIndex );

			m_SamplesBeingConsumed += m_SkipFramesStart.uSkipSamplesLength;

			m_SkipFramesStart.uSkipStartBufferSamples = 0;
			m_SkipFramesStart.uSkipSamplesLength = 0;
		}
	}
	
	if ( m_SkipFrames.uSkipSamplesLength != 0 )
	{
		if ( m_ReadSampleIndex == m_SkipFrames.uSkipStartBufferSamples )
		{
#ifdef AT9_STREAM_DEBUG_OUTPUT
			char msg[256];
			sprintf( msg, "-- -- -- -- [%p] Skipping C. %i \n", this, m_SkipFrames.uSkipSamplesLength);
			AKPLATFORM::OutputDebugMsg( msg );
#endif

			if (m_SkipFrames.bReadBeforeSkip)
			{
				m_SkipFrames.bReadBeforeSkip = false;
				return framesToRead;
			}

			// skip at the begining
			m_ReadSampleIndex = ((m_ReadSampleIndex + m_SkipFrames.uSkipSamplesLength) % m_TotalSamples);

			m_RingbufferUsedSample -= m_SkipFrames.uSkipSamplesLength;

			m_SkipFrames.uSkipStartBufferSamples = 0;
			m_SkipFrames.uSkipSamplesLength = 0;	

			if (m_RingbufferUsedSample < framesToRead)
			{
				framesToRead = m_RingbufferUsedSample;
			}
		}
		else if( m_ReadSampleIndex < m_SkipFrames.uSkipStartBufferSamples && m_ReadSampleIndex + framesToRead >  m_SkipFrames.uSkipStartBufferSamples )
		{
#ifdef AT9_STREAM_DEBUG_OUTPUT
			char msg[256];
			sprintf( msg, "-- -- -- -- [%p] Skipping D. %i \n", this, m_SkipFrames.uSkipSamplesLength);
			AKPLATFORM::OutputDebugMsg( msg );
#endif
			if (m_SkipFrames.bReadBeforeSkip)
			{
				m_SkipFrames.bReadBeforeSkip = false;
				return framesToRead;
			}

			// skip at the end
			framesToRead = ( m_SkipFrames.uSkipStartBufferSamples - m_ReadSampleIndex );

			m_SamplesBeingConsumed += m_SkipFrames.uSkipSamplesLength;

			m_SkipFrames.uSkipStartBufferSamples = 0;
			m_SkipFrames.uSkipSamplesLength = 0;
		}
	}
	
	return framesToRead;
}
