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

#include <ajm.h>

#include "AkSettings.h"

static const AkUInt16 AKAT9_RINGBUFFER_NUM_BLOCKS = 4;
static const AkUInt16 AKAT9_MAX_DECODER_INPUT_BUFFERS = 2;

#define SamplesToOutputBytes( _a ) _a * uNumChannels * sizeof(AkUInt16)
#define SampleToBufferIndex( _a ) _a * uNumChannels

struct sAkDecodeResult
{		
	SceAjmSidebandResult		sResult;
	SceAjmSidebandStream		sStream;
	SceAjmSidebandFormat		sFormat;
	SceAjmSidebandMFrame		sMFrame;
};

class CAkSkipDecodedFrames
{
public:
	CAkSkipDecodedFrames();
	
	AkUInt32 				SetSkipFrames(AkUInt32 in_uSkipStartIndex, AkUInt32 in_uSkipLen, bool in_bReadBeforeSkip);
	void					Reset();

	AkUInt32				uSkipStartBufferSamples;
	AkUInt32				uSkipSamplesLength;
	AkUInt8					bReadBeforeSkip:1;
};

class CAkDecodingRingbuffer
{
public:
	CAkDecodingRingbuffer();
	
	bool					Create(AkUInt32 in_numSamples, AkUInt32 in_numChannels);
	AKRESULT				Grow(AkUInt32 in_numSamples, AkUInt32 in_numChannels);
	void					Destroy();
	void					Reset();
	AkForceInline AkUInt32	CalcFreeSpace() const {	return m_TotalSamples - m_RingbufferUsedSample; }
	AkForceInline AkUInt32	CalcUsedSpace() const { return m_RingbufferUsedSample; }
	AkForceInline AkInt32	CalcAvailableFrames() const { return m_RingbufferUsedSample - (m_SamplesBeingConsumed + m_SkipFrames.uSkipSamplesLength + m_SkipFramesStart.uSkipSamplesLength); }
	AkForceInline bool		ReadBeforeSkipRequired( AkInt32 in_skipStartIndex ) const { return (in_skipStartIndex % m_TotalSamples >= m_ReadSampleIndex) && (in_skipStartIndex % m_TotalSamples < m_ReadSampleIndex + m_RingbufferUsedSample); }

	AkInt32					SkipFramesIfNeeded(const AkUInt32 in_uReadLength);

	AkForceInline void		ReleasePreviousDataIfNeeded()
	{ 
		if (m_pPreviousData != NULL)
		{
			AkFree(AkMemID_Processing, m_pPreviousData);
			m_pPreviousData = NULL;
		}
	}


	AkUInt16*				GetWriteBuffer(AkUInt32 in_uOffset)
	{
		//AKASSERT(in_uOffset < m_ReadSampleIndex);
		return m_pData + in_uOffset;
	}

	AkUInt16*				GetReadBuffer(AkUInt32 in_uOffset)
	{
		return m_pData + in_uOffset;
	}

	AkUInt32				m_WriteSampleIndex;
	AkUInt32				m_ReadSampleIndex;
	AkUInt32				m_SamplesBeingConsumed;
	AkInt32					m_RingbufferUsedSample;
	AkUInt32				m_TotalSamples;

	CAkSkipDecodedFrames	m_SkipFramesStart;
	CAkSkipDecodedFrames	m_SkipFrames;


private:
	AkUInt16*				m_pData;
	AkUInt16*				m_pPreviousData;
};