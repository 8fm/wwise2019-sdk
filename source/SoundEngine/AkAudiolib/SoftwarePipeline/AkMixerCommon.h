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
// AkMixerInternal.h
//
// Simple header file to contain material common to AkMixer and SIMD variants of AkMixer
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_MIXER_INTERNAL_H_
#define _AK_MIXER_INTERNAL_H_

#include "AkCommon.h"

// Interleaving helpers.
struct AkSpeakerVolumesN
{
	AkReal32 fVolume;
	AkReal32 fVolumeDelta;

	AkSpeakerVolumesN(AkReal32 fPrevVol, AkReal32 fNextVol, AkReal32 fOneOverNumSamples)
	{
		fVolume = fPrevVol;
		fVolumeDelta = (fNextVol - fPrevVol) * fOneOverNumSamples;
	}
};

// Helper functions to perform gain and interleave

// GainN; i.e. in_pSrc and in_pDest have the same number (N) of channels
void ApplyGainN(AkAudioBuffer* in_pInputBuffer, AkAudioBuffer* in_pOutputBuffer, AkRamp in_gain);
void ApplyGainNInt16(AkAudioBuffer* in_pInputBuffer, AkAudioBuffer* in_pOutputBuffer, AkRamp in_gain);
void ApplyGainN_V4F32(AkAudioBuffer* in_pInputBuffer, AkAudioBuffer* in_pOutputBuffer, AkRamp in_gain);
void ApplyGainNInt16_V4F32(AkAudioBuffer* in_pInputBuffer, AkAudioBuffer* in_pOutputBuffer, AkRamp in_gain);

// InterleaveXY; Generic gain/interleave
void ApplyGainAndInterleaveXY(AkAudioBuffer* in_pInputBuffer, AkAudioBuffer* in_pOutputBuffer, AkRamp in_gain);
void ApplyGainAndInterleaveXYInt16(AkAudioBuffer* in_pInputBuffer, AkAudioBuffer* in_pOutputBuffer, AkRamp in_gain);

// Mono, InterleaveStereo, Interleave51, Interleave71; specializations to convert to and from specific (but typical) channel configs
void ApplyGainMono_V4F32(AkAudioBuffer* in_pInputBuffer, AkAudioBuffer* in_pOutputBuffer, AkRamp in_gain);
void ApplyGainAndInterleaveStereo_V4F32(AkAudioBuffer* in_pInputBuffer, AkAudioBuffer* in_pOutputBuffer, AkRamp in_gain);
void ApplyGainAndInterleaveStereoInt16_V4F32(AkAudioBuffer* in_pInputBuffer, AkAudioBuffer* in_pOutputBuffer, AkRamp in_gain);
void ApplyGainAndInterleave51_V4F32(AkAudioBuffer* in_pInputBuffer, AkAudioBuffer* in_pOutputBuffer, AkRamp in_gain);
void ApplyGainAndInterleave51FromStereo_V4F32(AkAudioBuffer* in_pInputBuffer, AkAudioBuffer* in_pOutputBuffer, AkRamp in_gain);
void ApplyGainAndInterleave71_V4F32(AkAudioBuffer* in_pInputBuffer, AkAudioBuffer* in_pOutputBuffer, AkRamp in_gain);
void ApplyGainAndInterleave71From51_V4F32(AkAudioBuffer* in_pInputBuffer, AkAudioBuffer* in_pOutputBuffer, AkRamp in_gain);
void ApplyGainAndInterleave71FromStereo_V4F32(AkAudioBuffer* in_pInputBuffer, AkAudioBuffer* in_pOutputBuffer, AkRamp in_gain);

#endif
