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
#include "AkMixer.h"
#include "AkMixerCommon.h"
#include "AkSrcLpFilter.h"

//////////////////////////////////////////////////////////////////////////

namespace AkMixer
{

enum ApplyGainSpeakerPreset
{
	ApplyGainSpeaker_Mono = 0,
	ApplyGainSpeaker_Stereo,
	ApplyGainSpeaker_51,
	ApplyGainSpeaker_71,
	ApplyGainSpeaker_Other,
	ApplyGainSpeaker_End
};

typedef void(*ApplyGainFuncPtr) (
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain);

// Mixer function table indices are, e.g.,
// MixerFuncTable[SrcSpeakerConfig][DestSpeakerConfig][ConvertToInt16];
static ApplyGainFuncPtr ApplyGainFuncTable[ApplyGainSpeaker_End][ApplyGainSpeaker_End][2];

ApplyGainSpeakerPreset GetApplyGainSpeakerPreset(AkChannelMask channelMask)
{
	ApplyGainSpeakerPreset preset = ApplyGainSpeaker_End;
	switch (channelMask)
	{
	case AK_SPEAKER_SETUP_MONO:		preset = ApplyGainSpeaker_Mono; break;
	case AK_SPEAKER_SETUP_STEREO:	preset = ApplyGainSpeaker_Stereo; break;
	case AK_SPEAKER_SETUP_5POINT1:	preset = ApplyGainSpeaker_51; break;
	case AK_SPEAKER_SETUP_7POINT1:	preset = ApplyGainSpeaker_71; break;
	default:						preset = ApplyGainSpeaker_Other; break;
	}
	return preset;
}

void InitMixerFunctTable()
{
	// Set baseline values first
	for (AkUInt16 i = 0; i < ApplyGainSpeaker_End; ++i)
	{
		for (AkUInt16 j = 0; j < ApplyGainSpeaker_End; ++j)
		{
			ApplyGainFuncTable[i][j][0] = ApplyGainAndInterleaveXY;
			ApplyGainFuncTable[i][j][1] = ApplyGainAndInterleaveXYInt16;
		}
	}

	// Set manual presets otherwise
	ApplyGainFuncTable[ApplyGainSpeaker_Mono][ApplyGainSpeaker_Mono][0] = ApplyGainMono_V4F32;
	ApplyGainFuncTable[ApplyGainSpeaker_Mono][ApplyGainSpeaker_Mono][1] = ApplyGainNInt16_V4F32;

	ApplyGainFuncTable[ApplyGainSpeaker_Stereo][ApplyGainSpeaker_Stereo][0] = ApplyGainAndInterleaveStereo_V4F32;
	ApplyGainFuncTable[ApplyGainSpeaker_Stereo][ApplyGainSpeaker_Stereo][1] = ApplyGainAndInterleaveStereoInt16_V4F32;

	ApplyGainFuncTable[ApplyGainSpeaker_51]	[ApplyGainSpeaker_51][0] = ApplyGainAndInterleave51_V4F32;
	ApplyGainFuncTable[ApplyGainSpeaker_Stereo][ApplyGainSpeaker_51][0] = ApplyGainAndInterleave51FromStereo_V4F32;

	ApplyGainFuncTable[ApplyGainSpeaker_71][ApplyGainSpeaker_71][0] = ApplyGainAndInterleave71_V4F32;
	ApplyGainFuncTable[ApplyGainSpeaker_51][ApplyGainSpeaker_71][0] = ApplyGainAndInterleave71From51_V4F32;
	ApplyGainFuncTable[ApplyGainSpeaker_Stereo][ApplyGainSpeaker_71][0] = ApplyGainAndInterleave71FromStereo_V4F32;
}

void ApplyGainAndInterleave(
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain,
	bool in_convertToInt16)
{
	AKASSERT(in_pInputBuffer->NumChannels() <= in_pOutputBuffer->NumChannels());

	// Determine appropriate indices for mixer table
	ApplyGainSpeakerPreset srcPreset = GetApplyGainSpeakerPreset(in_pInputBuffer->GetChannelConfig().uChannelMask);
	ApplyGainSpeakerPreset destPreset = GetApplyGainSpeakerPreset(in_pOutputBuffer->GetChannelConfig().uChannelMask);
	AkUInt32 convertToInt16 = in_convertToInt16 ? 1 : 0;

	// Execute the function
	ApplyGainFuncTable[srcPreset][destPreset][convertToInt16](in_pInputBuffer, in_pOutputBuffer, in_gain);
}

void ApplyGain(
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain,
	bool in_convertToInt16)
{
	AKASSERT(in_pInputBuffer->GetChannelConfig() == in_pOutputBuffer->GetChannelConfig());
	if (in_convertToInt16)
	{
		ApplyGainNInt16_V4F32(in_pInputBuffer, in_pOutputBuffer, in_gain);
	}
	else
	{
		ApplyGainN_V4F32(in_pInputBuffer, in_pOutputBuffer, in_gain);
	}
}

//////////////////////////////////////////////////////////////////////////

void MixNinNChannels(
	AkAudioBuffer *	in_pInputBuffer,
	AkAudioBuffer *	in_pOutputBuffer,
	AkRamp & in_attenuation,
	AK::SpeakerVolumes::ConstVectorPtr in_vPrevVolumes,
	AK::SpeakerVolumes::ConstVectorPtr in_vNextVolumes,
	AkReal32 in_fOneOverNumFrames,
	AkUInt16 in_uNumFrames
)
{
	// We process full band channels in and out with matrixing, and then perform a single channel
	// mix from LFE in to LFE out (if applicable). 
	AkChannelConfig outputConfigNoLfe = in_pOutputBuffer->GetChannelConfig().RemoveLFE();
	AkUInt32 uNumFullbandChannelsIn = in_pInputBuffer->GetChannelConfig().RemoveLFE().uNumChannels;
	AkUInt32 uNumChannelsOut = in_pOutputBuffer->NumChannels(); // Important: Use the real number of output channels in order to address input channels correctly.
	unsigned int uChannel = 0;
	while (uChannel < uNumFullbandChannelsIn)
	{
		MixOneInNChannels(
			in_pInputBuffer->GetChannel(uChannel),
			in_pOutputBuffer,
			outputConfigNoLfe,
			in_attenuation,
			AK::SpeakerVolumes::Matrix::GetChannel(in_vPrevVolumes, uChannel, uNumChannelsOut),
			AK::SpeakerVolumes::Matrix::GetChannel(in_vNextVolumes, uChannel, uNumChannelsOut),
			in_fOneOverNumFrames,
			in_uNumFrames
		);
		++uChannel;
	}

	// LFE.
#ifdef AK_LFECENTER
	if (in_pInputBuffer->HasLFE() && in_pOutputBuffer->HasLFE())
	{
		// uChannel already points at input LFE.
		AkUInt32 uIdxLfeOut = in_pOutputBuffer->GetChannelConfig().uNumChannels - 1;
		AkSpeakerVolumesN volumeLFE(
			AK::SpeakerVolumes::Matrix::GetChannel(in_vPrevVolumes, uChannel, uNumChannelsOut)[uIdxLfeOut] * in_attenuation.fPrev,
			AK::SpeakerVolumes::Matrix::GetChannel(in_vNextVolumes, uChannel, uNumChannelsOut)[uIdxLfeOut] * in_attenuation.fNext,
			in_fOneOverNumFrames);
		MixChannelSIMD(
			in_pInputBuffer->GetLFE(),
			in_pOutputBuffer->GetLFE(),
			volumeLFE.fVolume,
			volumeLFE.fVolumeDelta,
			in_uNumFrames
		);
	}
#endif

	// set the number of output bytes
	in_pOutputBuffer->uValidFrames = in_uNumFrames;
}

void MixOneInNChannels(
	AkReal32 * AK_RESTRICT in_pInChannel,
	AkAudioBuffer *	in_pOutputBuffer,
	AkChannelConfig in_channelConfigOutNoLfe, // We do not mix into the output LFE, so we don't consider it. Pass in in_pOutputBuffer->GetChannelConfig with no LFE
	AkRamp & in_attenuation,
	AK::SpeakerVolumes::ConstVectorPtr in_pPrevVolumes,
	AK::SpeakerVolumes::ConstVectorPtr in_pNextVolumes,
	AkReal32 in_fOneOverNumFrames,
	AkUInt16 in_uNumFrames
)
{
	unsigned int uOutChannel = 0;
	while (uOutChannel < in_channelConfigOutNoLfe.uNumChannels)
	{
		AkReal32 fPrev = in_pPrevVolumes[uOutChannel] * in_attenuation.fPrev;
		AkReal32 fNext = in_pNextVolumes[uOutChannel] * in_attenuation.fNext;
		MixChannelSIMD(
			in_pInChannel,
			in_pOutputBuffer->GetChannel(uOutChannel),
			fPrev,
			(fNext - fPrev) * in_fOneOverNumFrames,
			in_uNumFrames);
		++uOutChannel;
	}
}

#define MAX_BUFFER_STACK_ALLOC 32768

void MixFilterNinNChannels(
	AkAudioBuffer *	in_pInputBuffer,
	AkAudioBuffer *	in_pOutputBuffer,
	AkRamp & in_attenuation,
	AK::SpeakerVolumes::ConstVectorPtr in_vPrevVolumes,
	AK::SpeakerVolumes::ConstVectorPtr in_vNextVolumes,
	AkReal32 in_fOneOverNumFrames,
	AkUInt16 in_uNumFrames,
	CAkSrcLpHpFilter* in_BQF)
{
	AkAudioBuffer outOfPlaceBuffer;
	AkUInt32 size = sizeof(AkSampleType) * in_uNumFrames * in_BQF->GetChannelConfig().uNumChannels + AK_SIMD_ALIGNMENT;
	AkUInt8* pTempBuffer = NULL;
	if (size <= MAX_BUFFER_STACK_ALLOC)	//Can't assume that all configs fit on stack.  But it is faster to do so.
	{
		pTempBuffer = (AkUInt8*)AkAlloca(size);
		pTempBuffer = (AkUInt8*)(((AkUIntPtr)pTempBuffer + (AK_SIMD_ALIGNMENT - 1)) & ~(AK_SIMD_ALIGNMENT - 1));
	}
	else
	{
		pTempBuffer = (AkUInt8*)AkMalign(AkMemID_Processing, size, AK_SIMD_ALIGNMENT);
	}

	if (pTempBuffer)
	{
		outOfPlaceBuffer.AttachInterleavedData(pTempBuffer, in_uNumFrames, in_uNumFrames, in_BQF->GetChannelConfig());
		in_BQF->ExecuteOutOfPlace(in_pInputBuffer, &outOfPlaceBuffer);
		MixNinNChannels(&outOfPlaceBuffer, in_pOutputBuffer, in_attenuation, in_vPrevVolumes, in_vNextVolumes, in_fOneOverNumFrames, in_uNumFrames);
	}
	else
	{
		//No memory for biquad.  At least mix the buffer.
		MixNinNChannels(in_pInputBuffer, in_pOutputBuffer, in_attenuation, in_vPrevVolumes, in_vNextVolumes, in_fOneOverNumFrames, in_uNumFrames);
	}

	in_pOutputBuffer->uValidFrames = in_uNumFrames;
	in_BQF->CheckBypass();

	if (pTempBuffer != NULL && size > MAX_BUFFER_STACK_ALLOC)
		AkFalign(AkMemID_Processing, pTempBuffer);
}

void MixBypassNinNChannels(
	AkAudioBuffer *	in_pInputBuffer,
	AkAudioBuffer *	in_pOutputBuffer,
	AkRamp & in_attenuation,
	AK::SpeakerVolumes::ConstVectorPtr in_vPrevVolumes,
	AK::SpeakerVolumes::ConstVectorPtr in_vNextVolumes,
	AkReal32 in_fOneOverNumFrames,
	AkUInt16 in_uNumFrames,
	CAkSrcLpHpFilter* in_BQF
)
{
	// We process full band channels in and out with matrixing, and then perform a single channel
	// mix from LFE in to LFE out (if applicable). 
	AkChannelConfig outputConfigNoLfe = in_pOutputBuffer->GetChannelConfig().RemoveLFE();
	AkUInt32 uNumFullbandChannelsIn = in_pInputBuffer->GetChannelConfig().RemoveLFE().uNumChannels;
	AkUInt32 uNumChannelsOut = in_pOutputBuffer->NumChannels(); // Important: Use the real number of output channels in order to address input channels correctly.
	unsigned int uChannel = 0;
	while (uChannel < uNumFullbandChannelsIn)
	{
		in_BQF->BypassMono(in_pInputBuffer, uChannel);

		MixOneInNChannels(
			in_pInputBuffer->GetChannel(uChannel),
			in_pOutputBuffer,
			outputConfigNoLfe,
			in_attenuation,
			AK::SpeakerVolumes::Matrix::GetChannel(in_vPrevVolumes, uChannel, uNumChannelsOut),
			AK::SpeakerVolumes::Matrix::GetChannel(in_vNextVolumes, uChannel, uNumChannelsOut),
			in_fOneOverNumFrames,
			in_uNumFrames
		);
		++uChannel;
	}

	// LFE.
#ifdef AK_LFECENTER
	if (in_pInputBuffer->HasLFE() && in_pOutputBuffer->HasLFE())
	{
		// uChannel already points at input LFE.
		in_BQF->BypassMono(in_pInputBuffer, uChannel);

		AkUInt32 uIdxLfeOut = in_pOutputBuffer->GetChannelConfig().uNumChannels - 1;
		AkSpeakerVolumesN volumeLFE(
			AK::SpeakerVolumes::Matrix::GetChannel(in_vPrevVolumes, uChannel, uNumChannelsOut)[uIdxLfeOut] * in_attenuation.fPrev,
			AK::SpeakerVolumes::Matrix::GetChannel(in_vNextVolumes, uChannel, uNumChannelsOut)[uIdxLfeOut] * in_attenuation.fNext,
			in_fOneOverNumFrames);
		MixChannelSIMD(
			in_pInputBuffer->GetLFE(),
			in_pOutputBuffer->GetLFE(),
			volumeLFE.fVolume,
			volumeLFE.fVolumeDelta,
			in_uNumFrames
		);
	}
#endif

	// set the number of output bytes
	in_pOutputBuffer->uValidFrames = in_uNumFrames;
}

}