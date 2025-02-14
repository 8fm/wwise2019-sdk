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

// Implement constant power mixdown of a number of input channel.
// Center and LFE channel are adjustable but others are taken full scale.
// Constant power implies sum_i=1_N(ChanGain^2) = 1
// E.g. using 5.1 configuration (CG = Constant power channel gain)
// CG^2 (L) + CG^2 (R) + (CenterLevel*CG)^2 (C) + (LFELevel*CG)^2 (LFE) + CG^2 (LS) + CG^2 (RS)

#include "AkDownMixUtils.h"
#include <AK/Tools/Common/AkAssert.h>
#include <math.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>
#include "MixStereoWidth.h"
#include "Mix2Interp.h"

static const AkReal32 s_fToStereo[][2] =
{
	{ 1.f, 0.f },									// FL
	{ 0.f, 1.f },									// FR
#ifdef AK_LFECENTER
	{ 0.5f, 0.5f },									// FC
	{ 0.f, 0.f },									// LFE
#endif
#ifdef AK_REARCHANNELS
	{ 1.f, 0.f },									// BL
	{ 0.f, 1.f },									// BR
	{ 0.f, 0.f },									// N/A
	{ 0.f, 0.f },									// N/A
	{ 0.5f, 0.5f },									// BC
	{ 1.f, 0.f },									// SL
	{ 0.f, 1.f },									// SR
#ifdef AK_71AUDIO
	{ 0.5f, 0.5f },									// Top
	{ 1.f, 0.f },									// HFL
	{ 0.5f, 0.5f },									// HFC
	{ 0.f, 1.f },									// HFR
	{ 1.f, 0.f },									// HBL
	{ 0.f, 0.f },									// HBC
	{ 1.f, 0.f },									// HBR
#endif	// AK_71AUDIO
#endif	// AK_REARCHANNELS
};

namespace DSP
{		
	void AkDownMix(AkAudioBuffer * in_pBuffer, AkAudioBuffer * out_pDownMixBuffer,
		AkReal32 in_fPrevInputCenterLevel, AkReal32 in_fInputCenterLevel,
		AkReal32 in_fPrevInputLFELevel, AkReal32 in_fInputLFELevel,
		AkReal32 in_fPrevInputStereoWidth, AkReal32 in_fInputStereoWidth
		)
	{
		// TODO: Make this code more flexible and consider output channel mask to take the right down-mixing decisions
		AkChannelConfig inChannelCfg = in_pBuffer->GetChannelConfig();
		AkChannelMask uInChannelMask = inChannelCfg.uChannelMask;

		// With standard configs, IR (downmix) config always a subset of input config (in terms of channel identifiers).
		// With ambisonics: if input is ambisonics, take W. If input is standard, downmix to mono first and then copy onto each channel.

		const AkUInt32 uNumFrames = in_pBuffer->uValidFrames;
		if (in_pBuffer->GetChannelConfig().eConfigType == AK_ChannelConfigType_Standard
			&& in_pBuffer->GetChannelConfig().uNumChannels > 1 )
		{
			AkReal32 fPrevChanGain, fChanGain;

			// Compute channel gains
#define AK_ALL_CHANNELS_EXCEPT_CENTER_AND_LFE ( AK_SUPPORTED_STANDARD_CHANNEL_MASK & (~AK_SPEAKER_FRONT_CENTER) & (~AK_SPEAKER_LOW_FREQUENCY) )
			AkReal32 fUnscaled = (AkReal32)AK::GetNumNonZeroBits(uInChannelMask & AK_ALL_CHANNELS_EXCEPT_CENTER_AND_LFE);
			AkReal32 fPrevCenter, fCenter;
			fPrevCenter = fCenter = 0.f;
			AkReal32 fPrevLFE, fLFE;
			fPrevLFE = fLFE = 0.f;
			if (uInChannelMask & AK_SPEAKER_FRONT_CENTER)
			{
				if ((uInChannelMask & AK_SPEAKER_SETUP_3_0) == AK_SPEAKER_SETUP_3_0)
				{
					fPrevCenter = in_fPrevInputCenterLevel*in_fPrevInputCenterLevel;
					fCenter = in_fInputCenterLevel*in_fInputCenterLevel;
				}
				else
				{
					fUnscaled += 1.f;
				}
			}
			if (uInChannelMask & AK_SPEAKER_LOW_FREQUENCY)
			{
				fPrevLFE = in_fPrevInputLFELevel*in_fPrevInputLFELevel;
				fLFE = in_fInputLFELevel*in_fInputLFELevel;
			}

			fPrevChanGain = sqrtf(1.f / (fUnscaled + fPrevCenter + fPrevLFE));
			fChanGain = sqrtf(1.f / (fUnscaled + fCenter + fLFE));
			
			// Downmixing to stereo (mono when there is only one channel in both input and IR).
			// Allocate for stereo.
			AkUInt32 uMxSize = AK::SpeakerVolumes::Matrix::GetRequiredSize(in_pBuffer->NumChannels(), 2);

			AK::SpeakerVolumes::MatrixPtr mx = (AK::SpeakerVolumes::MatrixPtr)AkAlloca(uMxSize);
			AK::SpeakerVolumes::MatrixPtr mxPrev = (AK::SpeakerVolumes::MatrixPtr)AkAlloca(uMxSize);

			AkChannelConfig cfgDownMix;
			if (in_pBuffer->GetChannelConfig().eConfigType == AK_ChannelConfigType_Standard
				&& in_pBuffer->GetChannelConfig().RemoveLFE().uNumChannels >= 2)
			{
				cfgDownMix.SetStandard(AK_SPEAKER_SETUP_STEREO);

				// Compute downmix matrix of input to stereo for standard config.
				AkUInt32 uIn = 0;
				AkUInt32 uIdxDownMixRow = 0;
				AkChannelMask uChannelBit = AK_SPEAKER_FRONT_LEFT;
				AkChannelMask uInChannelMaskNoLFE = uInChannelMask & ~AK_SPEAKER_LOW_FREQUENCY;
				while (uChannelBit <= uInChannelMaskNoLFE)
				{
					if (uChannelBit & uInChannelMaskNoLFE)
					{
						AK::SpeakerVolumes::VectorPtr pOutRow = AK::SpeakerVolumes::Matrix::GetChannel(mx, uIn, 2);
						AK::SpeakerVolumes::VectorPtr pOutRowPrev = AK::SpeakerVolumes::Matrix::GetChannel(mxPrev, uIn, 2);
						pOutRow[0] = s_fToStereo[uIdxDownMixRow][0] * fChanGain;
						pOutRow[1] = s_fToStereo[uIdxDownMixRow][1] * fChanGain;
						pOutRowPrev[0] = s_fToStereo[uIdxDownMixRow][0] * fPrevChanGain;
						pOutRowPrev[1] = s_fToStereo[uIdxDownMixRow][1] * fPrevChanGain;
						++uIn;
					}
					++uIdxDownMixRow;
					uChannelBit <<= 1;
				}

				// Apply input-side stereo width
				AkReal32 fGain1, fGain2, fPrevGain1, fPrevGain2;
				DSP::ComputeLRMixGains(in_fPrevInputStereoWidth, in_fInputStereoWidth, fGain1, fGain2, fPrevGain1, fPrevGain2);

				// Multiply mix matrices with transpose of width matrices [fGain1 fGain2; fGain2 fGain1] (symmetrical)
				AkUInt32 uNumChannelsInNoLFE = in_pBuffer->GetChannelConfig().RemoveLFE().uNumChannels;
				for (AkUInt32 uChanIn = 0; uChanIn < uNumChannelsInNoLFE; uChanIn++)
				{
					AK::SpeakerVolumes::VectorPtr pChanIn = AK::SpeakerVolumes::Matrix::GetChannel(mx, uChanIn, 2);
					AkReal32 fLeft = fGain1 * pChanIn[0] + fGain2 * pChanIn[1];
					AkReal32 fRight = fGain2 * pChanIn[0] + fGain1 * pChanIn[1];
					pChanIn[0] = fLeft;
					pChanIn[1] = fRight;

					AK::SpeakerVolumes::VectorPtr pPrevChanIn = AK::SpeakerVolumes::Matrix::GetChannel(mxPrev, uChanIn, 2);
					AkReal32 fPrevLeft = fPrevGain1 * pPrevChanIn[0] + fPrevGain2 * pPrevChanIn[1];
					AkReal32 fPrevRight = fPrevGain2 * pPrevChanIn[0] + fPrevGain1 * pPrevChanIn[1];
					pPrevChanIn[0] = fPrevLeft;
					pPrevChanIn[1] = fPrevRight;
				}

				// Fix center.
				if ((uInChannelMask & AK_SPEAKER_SETUP_3_0) == AK_SPEAKER_SETUP_3_0)
				{
					AK::SpeakerVolumes::VectorPtr pChan = AK::SpeakerVolumes::Matrix::GetChannel(mx, 2, 2);
					AK::SpeakerVolumes::VectorPtr pChanPrev = AK::SpeakerVolumes::Matrix::GetChannel(mxPrev, 2, 2);
					pChan[0] *= in_fInputCenterLevel;
					pChanPrev[0] *= in_fPrevInputCenterLevel;
					pChan[1] *= in_fInputCenterLevel;
					pChanPrev[1] *= in_fPrevInputCenterLevel;
				}
				// Fix LFE.
				if (uInChannelMask & AK_SPEAKER_LOW_FREQUENCY)
				{
					AKASSERT(uInChannelMask != AK_SPEAKER_SETUP_0POINT1);
					AK::SpeakerVolumes::VectorPtr pLfe = AK::SpeakerVolumes::Matrix::GetChannel(mx, in_pBuffer->NumChannels() - 1, 2);
					AK::SpeakerVolumes::VectorPtr pLfePrev = AK::SpeakerVolumes::Matrix::GetChannel(mxPrev, in_pBuffer->NumChannels() - 1, 2);
					pLfe[0] = in_fInputLFELevel;
					pLfePrev[0] = in_fPrevInputLFELevel;
					pLfe[1] = in_fInputLFELevel;
					pLfePrev[1] = in_fPrevInputLFELevel;
				}
			}
			else
			{
				cfgDownMix.SetStandard(AK_SPEAKER_SETUP_MONO);

				for (AkUInt32 uChan = 0; uChan < in_pBuffer->NumChannels(); uChan++)
				{
					AK::SpeakerVolumes::Matrix::GetChannel(mx, uChan, 1)[0] = fChanGain;
					AK::SpeakerVolumes::Matrix::GetChannel(mxPrev, uChan, 1)[0] = fPrevChanGain;
				}

				// Fix center.
				if ((uInChannelMask & AK_SPEAKER_SETUP_3_0) == AK_SPEAKER_SETUP_3_0)
				{
					AK::SpeakerVolumes::VectorPtr pChan = AK::SpeakerVolumes::Matrix::GetChannel(mx, 2, 1);
					AK::SpeakerVolumes::VectorPtr pChanPrev = AK::SpeakerVolumes::Matrix::GetChannel(mxPrev, 2, 1);
					pChan[0] *= in_fInputCenterLevel;
					pChanPrev[0] *= in_fPrevInputCenterLevel;
				}

				// Fix LFE.
				if (uInChannelMask & AK_SPEAKER_LOW_FREQUENCY
					&& uInChannelMask != AK_SPEAKER_SETUP_0POINT1)	// <- preserve old behavior
				{
					AK::SpeakerVolumes::VectorPtr pLfe = AK::SpeakerVolumes::Matrix::GetChannel(mx, in_pBuffer->NumChannels() - 1, 1);
					AK::SpeakerVolumes::VectorPtr pLfePrev = AK::SpeakerVolumes::Matrix::GetChannel(mxPrev, in_pBuffer->NumChannels() - 1, 1);
					pLfe[0] *= in_fInputLFELevel;
					pLfePrev[0] *= in_fPrevInputLFELevel;
				}
			}

			if (uInChannelMask != AK_SPEAKER_SETUP_0POINT1)
			{
				for (AkUInt32 uChan = 0; uChan < out_pDownMixBuffer->NumChannels(); uChan++)
				{
					memset(out_pDownMixBuffer->GetChannel(uChan), 0, uNumFrames * sizeof(AkReal32));
				}

				AkUInt32 uChanOut = 0;
				do
				{
					// LX Optimize have "copy with gain"
					memset(out_pDownMixBuffer->GetChannel(uChanOut), 0, uNumFrames * sizeof(AkReal32));
					
					AkUInt32 uChanIn = 0;
					do
					{
						Mix2Interp(
							out_pDownMixBuffer->GetChannel(uChanOut),
							in_pBuffer->GetChannel(uChanIn),
							1.f,
							1.f,
							AK::SpeakerVolumes::Matrix::GetChannel(mxPrev, uChanIn, cfgDownMix.uNumChannels)[uChanOut],
							AK::SpeakerVolumes::Matrix::GetChannel(mx, uChanIn, cfgDownMix.uNumChannels)[uChanOut],
							uNumFrames);
					} while (++uChanIn < in_pBuffer->NumChannels());
				} while (++uChanOut < cfgDownMix.uNumChannels);
			}
			else
			{
				// 0.1: LFE volume dealt with at upmix stage.
				AKASSERT(cfgDownMix.uNumChannels == 1);
				memcpy(out_pDownMixBuffer->GetChannel(0), in_pBuffer->GetLFE(), uNumFrames * sizeof(AkReal32));
			}

			if (out_pDownMixBuffer->GetChannelConfig().eConfigType == AK_ChannelConfigType_Standard
				&& out_pDownMixBuffer->GetChannelConfig().uNumChannels > cfgDownMix.uNumChannels)
			{
				if (cfgDownMix.uNumChannels == 1)
				{
					// Copy our downmixed channel to all channels of our downmix buffer.
					for (AkUInt32 uChan = 1; uChan < out_pDownMixBuffer->NumChannels(); uChan++)
					{
						memcpy(out_pDownMixBuffer->GetChannel(uChan), out_pDownMixBuffer->GetChannel(0), uNumFrames * sizeof(AkReal32));
					}
				}
				else if (cfgDownMix.uNumChannels == 2)
				{
					// Replicate the left channel of our downmix to all left channels of the downmix buffer, and the right channel to all right channels of the downmix buffer.
					AKASSERT(out_pDownMixBuffer->GetChannelConfig().eConfigType == AK_ChannelConfigType_Standard);
					AkUInt32 uIdxSL = ((out_pDownMixBuffer->GetChannelConfig().uChannelMask & AK_SPEAKER_SETUP_3_0) == AK_SPEAKER_SETUP_3_0) ? 3 : 2;
					if ((out_pDownMixBuffer->GetChannelConfig().uChannelMask & AK_SPEAKER_SETUP_3_0) == AK_SPEAKER_SETUP_3_0)
					{
						/// LX Optimize: Gain not changing over time
						Mix2Interp(out_pDownMixBuffer->GetChannel(2), out_pDownMixBuffer->GetChannel(0), 1.f, 1.f, 0.5f, 0.5f, uNumFrames);
						Mix2Interp(out_pDownMixBuffer->GetChannel(2), out_pDownMixBuffer->GetChannel(1), 1.f, 1.f, 0.5f, 0.5f, uNumFrames);
					}
					if (AK::HasSurroundChannels(out_pDownMixBuffer->GetChannelConfig().uChannelMask))
					{
						memcpy(out_pDownMixBuffer->GetChannel(uIdxSL), out_pDownMixBuffer->GetChannel(0), uNumFrames * sizeof(AkReal32));
						memcpy(out_pDownMixBuffer->GetChannel(uIdxSL + 1), out_pDownMixBuffer->GetChannel(1), uNumFrames * sizeof(AkReal32));
					}
					if (AK::HasSideAndRearChannels(out_pDownMixBuffer->GetChannelConfig().uChannelMask))
					{
						memcpy(out_pDownMixBuffer->GetChannel(uIdxSL + 2), out_pDownMixBuffer->GetChannel(0), uNumFrames * sizeof(AkReal32));
						memcpy(out_pDownMixBuffer->GetChannel(uIdxSL + 3), out_pDownMixBuffer->GetChannel(1), uNumFrames * sizeof(AkReal32));
					}
					/// LX TODO Height channels (?)
					if (AK::HasLFE(out_pDownMixBuffer->GetChannelConfig().uChannelMask))
					{
						Mix2Interp(out_pDownMixBuffer->GetLFE(), out_pDownMixBuffer->GetChannel(0), 1.f, 1.f, 0.5f, 0.5f, uNumFrames);
						Mix2Interp(out_pDownMixBuffer->GetLFE(), out_pDownMixBuffer->GetChannel(1), 1.f, 1.f, 0.5f, 0.5f, uNumFrames);
					}
				}
			}
		}
		else
		{
			// Non standard configs simply take first channel. Ambisonics: it is W.
			memcpy(out_pDownMixBuffer->GetChannel(0), in_pBuffer->GetChannel(0), uNumFrames * sizeof(AkReal32));

			if (out_pDownMixBuffer->GetChannelConfig().eConfigType == AK_ChannelConfigType_Ambisonic)
			{
				// No input spread with ambisonics.
				for (AkUInt32 uChan = 1; uChan < out_pDownMixBuffer->NumChannels(); uChan++)
				{
					memcpy(out_pDownMixBuffer->GetChannel(uChan), in_pBuffer->GetChannel(0), uNumFrames * sizeof(AkReal32));
				}
			}
			else
			{
				// Further channels are mixed in with input spread.
#define AK_CONV_VERB_MAXSTEREOWIDTH (180.f)		
				AkReal32 fPrevDir = in_fPrevInputStereoWidth / AK_CONV_VERB_MAXSTEREOWIDTH;
				AkReal32 fDir = in_fInputStereoWidth / AK_CONV_VERB_MAXSTEREOWIDTH;

				for (AkUInt32 uChan = 1; uChan < out_pDownMixBuffer->NumChannels(); uChan++)
				{
					// LX Optimize have "copy with gain"
					memset(out_pDownMixBuffer->GetChannel(uChan), 0, uNumFrames * sizeof(AkReal32));

					Mix2Interp(
						out_pDownMixBuffer->GetChannel(uChan),
						in_pBuffer->GetChannel(uChan),						
						0,
						0,
						fPrevDir,
						fDir,
						uNumFrames);
				}
			}
		}
	}

} // namespace DSP
