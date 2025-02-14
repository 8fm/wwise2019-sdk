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

//////////////////////////////////////////////////////////////////////
//
// AkMixer.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkMath.h"
#include "AkMixer.h"
#include "AkMixerCommon.h"

void PipelineToStandardOffsets( AkUInt32 in_uNumSamples, AkChannelConfig srcConfig, AkUInt32 * out_arChannelOffsets )
{
	// Take ordering into account when channel config is standard.
#ifdef AK_LFECENTER
	if ( srcConfig.HasLFE() )
	{
		AkUInt32 uDestLFE = ( srcConfig.HasCenter() ) ? 3 : 2;
		// Before LFE.
		AkUInt32 uDest = 0;
		while ( uDest < uDestLFE )
		{
			// Remap the channels to wav specification
			out_arChannelOffsets[uDest]=in_uNumSamples*uDest;
			uDest++;
		}
		// LFE
		out_arChannelOffsets[uDestLFE]=in_uNumSamples*(srcConfig.uNumChannels-1);
		// After LFE
		uDest = uDestLFE + 1;
		while ( uDest < srcConfig.uNumChannels )
		{
			// Remap the channels to wav specification
			out_arChannelOffsets[uDest]=in_uNumSamples*(uDest-1);
			uDest++;
		}
	}
	else
#endif
	{	
		for( AkUInt32 c = 0; c < srcConfig.uNumChannels; c++ )
		{
			out_arChannelOffsets[c]=in_uNumSamples*c;
		}
	}
}

// Not directly used; kept around for reference. See ApplyGainNInt16_V4F32
void ApplyGainNInt16(
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain)
{
	AkUInt16 usMaxFrames = in_pInputBuffer->MaxFrames();
	const AkReal32* AK_RESTRICT pSrc = (const AkReal32*)in_pInputBuffer->GetInterleavedData();
	AkInt16* AK_RESTRICT pDest = (AkInt16*)in_pOutputBuffer->GetInterleavedData();
	AkChannelConfig channelConfig = in_pOutputBuffer->GetChannelConfig();

	// Convert pSrc to pDest and apply gain
	AkReal32 fGainDelta = (in_gain.fNext - in_gain.fPrev) / (AkReal32)usMaxFrames;
	for (size_t c = 0; c < channelConfig.uNumChannels; c++)
	{
		AkReal32 fGain = in_gain.fPrev;
		for (size_t i = 0; i < usMaxFrames; i++)
		{
			*pDest = FLOAT_TO_INT16(*pSrc * fGain);
			pSrc++;
			pDest++;
			fGain += fGainDelta;
		}
	}
}

// Not directly used; kept around for reference. See ApplyGainN_V4F32
void ApplyGainN(
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain)
{
	AkUInt16 usMaxFrames = in_pInputBuffer->MaxFrames();
	const AkReal32* AK_RESTRICT pSrc = (const AkReal32*)in_pInputBuffer->GetInterleavedData();
	AkReal32* AK_RESTRICT pDest = (AkReal32*)in_pOutputBuffer->GetInterleavedData();
	AkChannelConfig channelConfig = in_pOutputBuffer->GetChannelConfig();

	// Convert pSrc to pDest and apply gain
	AkReal32 fGainDelta = (in_gain.fNext - in_gain.fPrev) / (AkReal32)usMaxFrames;
	for (size_t c = 0; c < channelConfig.uNumChannels; c++)
	{
		AkReal32 fGain = in_gain.fPrev;
		for (size_t i = 0; i < usMaxFrames; i++)
		{
			*pDest = (*pSrc * fGain);
			pSrc++;
			pDest++;
			fGain += fGainDelta;
		}
	}
}

void ApplyGainAndInterleaveXYInt16(
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain)
{
	AkUInt16 usMaxFrames = in_pInputBuffer->MaxFrames();
	const AkReal32* AK_RESTRICT pSrc = (const AkReal32*)in_pInputBuffer->GetInterleavedData();
	AkInt16* AK_RESTRICT pDest = (AkInt16*)in_pOutputBuffer->GetInterleavedData();
	AkChannelConfig srcConfig = in_pInputBuffer->GetChannelConfig();
	AkChannelConfig destConfig = in_pOutputBuffer->GetChannelConfig();
	
	// Compute channel offsets at once.
	AkUInt32 * channelOffsets = (AkUInt32 *)AkAlloca( srcConfig.uNumChannels * sizeof(size_t) );
	PipelineToStandardOffsets( usMaxFrames, srcConfig, channelOffsets );
	
	// Interleave the data
	AkReal32 fGain = in_gain.fPrev;
	AkReal32 fDelta = ( in_gain.fNext - in_gain.fPrev ) / (AkReal32)usMaxFrames;

	AKASSERT(srcConfig.uNumChannels <= destConfig.uNumChannels);
	if (srcConfig.uNumChannels == destConfig.uNumChannels)
	{
		for (size_t i = 0; i < usMaxFrames; i++)
		{
			for (size_t c = 0; c < srcConfig.uNumChannels; c++)
			{
				*pDest = FLOAT_TO_INT16(pSrc[channelOffsets[c] + i] * fGain);
				pDest++;
			}
			fGain += fDelta;
		}
	}
	else
	{
		for (size_t i = 0; i < usMaxFrames; i++)
		{
			size_t c = 0;
			for (; c < srcConfig.uNumChannels; c++)
			{
				*pDest = FLOAT_TO_INT16(pSrc[channelOffsets[c] + i] * fGain);
				pDest++;
			}
			for (; c < destConfig.uNumChannels; c++)
			{
				*pDest = FLOAT_TO_INT16(0);
				pDest++;
			}
			fGain += fDelta;
		}
	}
}

void ApplyGainAndInterleaveXY(
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain)
{
	AkUInt16 usMaxFrames = in_pInputBuffer->MaxFrames();
	const AkReal32* AK_RESTRICT pSrc = (const AkReal32*)in_pInputBuffer->GetInterleavedData();
	AkReal32* AK_RESTRICT pDest = (AkReal32*)in_pOutputBuffer->GetInterleavedData();
	AkChannelConfig srcConfig = in_pInputBuffer->GetChannelConfig();
	AkChannelConfig destConfig = in_pOutputBuffer->GetChannelConfig();
	
	// Compute channel offsets at once.
	AkUInt32 * channelOffsets = (AkUInt32 *)AkAlloca( srcConfig.uNumChannels * sizeof(size_t) );
	PipelineToStandardOffsets( usMaxFrames, srcConfig, channelOffsets );
	
	// Interleave the data
	AkReal32 fGain = in_gain.fPrev;
	AkReal32 fDelta = (in_gain.fNext - in_gain.fPrev) / (AkReal32)usMaxFrames;

	AKASSERT(srcConfig.uNumChannels <= destConfig.uNumChannels);
	if (srcConfig.uNumChannels == destConfig.uNumChannels)
	{
		for (size_t i = 0; i < usMaxFrames; i++)
		{
			for (size_t c = 0; c < srcConfig.uNumChannels; c++)
			{
				*pDest = pSrc[channelOffsets[c] + i] * fGain;
				pDest++;
			}
			fGain += fDelta;
		}
	}
	else
	{
		// Interleave the data
		for (size_t i = 0; i < usMaxFrames; i++)
		{
			size_t c = 0;
			for (; c < srcConfig.uNumChannels; c++)
			{
				*pDest = pSrc[channelOffsets[c] + i] * fGain;
				pDest++;
			}
			for (; c < destConfig.uNumChannels; c++)
			{
				*pDest = 0;
				pDest++;
			}
			fGain += fDelta;
		}
	}
}
