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
// AkMixer.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_MIXER_H_
#define _AK_MIXER_H_

#include "AkCommon.h"

class CAkSrcLpHpFilter;
class AkAudioBuffer;

namespace AkMixer
{
	// Must be called at init to use ApplyGain functions
	void InitMixerFunctTable();

	AKSOUNDENGINE_API void ApplyGainAndInterleave(
		AkAudioBuffer* in_pInputBuffer,
		AkAudioBuffer* in_pOutputBuffer,
		AkRamp in_gain,
		bool in_convertToInt16);

	AKSOUNDENGINE_API void ApplyGain(
		AkAudioBuffer* in_pInputBuffer,
		AkAudioBuffer* in_pOutputBuffer,
		AkRamp in_gain,
		bool in_convertToInt16);
	
	AKSOUNDENGINE_API void MixNinNChannels(
		AkAudioBuffer *	in_pInputBuffer,
		AkAudioBuffer *	in_pOutputBuffer,
		AkRamp & in_attenuation,
		AK::SpeakerVolumes::ConstMatrixPtr in_mxPrevVolumes,
		AK::SpeakerVolumes::ConstMatrixPtr in_mxNextVolumes,
		AkReal32 in_fOneOverNumFrames,
		AkUInt16 in_uNumFrames);

	AKSOUNDENGINE_API void MixOneInNChannels(
		AkReal32 * AK_RESTRICT in_pInChannel,
		AkAudioBuffer *	in_pOutputBuffer,
		AkChannelConfig in_channelConfigOutNoLfe, // We do not mix into the output LFE, so we don't consider it. Pass in in_pOutputBuffer->GetChannelConfig with no LFE
		AkRamp & in_attenuation,
		AK::SpeakerVolumes::ConstVectorPtr in_pPrevVolumes,
		AK::SpeakerVolumes::ConstVectorPtr in_pNextVolumes,
		AkReal32 in_fOneOverNumFrames,
		AkUInt16 in_uNumFrames);

	AKSOUNDENGINE_API void MixFilterNinNChannels(
		AkAudioBuffer *	in_pInputBuffer,
		AkAudioBuffer *	in_pOutputBuffer,
		AkRamp & in_attenuation,
		AK::SpeakerVolumes::ConstMatrixPtr in_mxPrevVolumes,
		AK::SpeakerVolumes::ConstMatrixPtr in_mxNextVolumes,
		AkReal32 in_fOneOverNumFrames,
		AkUInt16 in_uNumFrames,
		CAkSrcLpHpFilter* in_BQF);

	AKSOUNDENGINE_API void MixBypassNinNChannels(
		AkAudioBuffer *	in_pInputBuffer,
		AkAudioBuffer *	in_pOutputBuffer,
		AkRamp & in_attenuation,
		AK::SpeakerVolumes::ConstMatrixPtr in_mxPrevVolumes,
		AK::SpeakerVolumes::ConstMatrixPtr in_mxNextVolumes,
		AkReal32 in_fOneOverNumFrames,
		AkUInt16 in_uNumFrames,
		CAkSrcLpHpFilter* in_BQF);

	// Assumes in_uNumSamples % 8 == 0.
	AKSOUNDENGINE_API void MixChannelSIMD(
		AkReal32*	in_pSourceData,
		AkReal32*	in_pDestData,
		AkReal32	in_fVolume,
		AkReal32	in_fVolumeDelta,
		AkUInt32	in_uNumSamples);
};
#endif
