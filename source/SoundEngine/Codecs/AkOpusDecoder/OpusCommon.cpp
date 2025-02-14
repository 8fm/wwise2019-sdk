/***********************************************************************
The content of this file includes source code for the sound engine
portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
Two Source Code" as defined in the Source Code Addendum attached
with this file.  Any use of the Level Two Source Code shall be
subject to the terms and conditions outlined in the Source Code
Addendum and the End User License Agreement for Wwise(R).

Version:   Build: 
Copyright (c) 2006-2020 Audiokinetic Inc.
***********************************************************************/

#include "AkFileParserBase.h"
#include "OpusCommon.h"
#include "AkCommon.h"

bool AK::Opus::ValidateFormat(WaveFormatOpusWEM* pFormat)
{
	AkChannelConfig cfg = pFormat->GetChannelConfig();
	int mapping;
	int coupled;
	ChannelConfigToMapping(cfg, mapping, coupled);
	return
		pFormat->uCodecVersion == 1 &&
		pFormat->nChannels == cfg.uNumChannels &&
		pFormat->uMappingFamily == mapping &&
		pFormat->uSeekTableSize > 0;
}


void AK::Opus::ChannelConfigToMapping(const AkChannelConfig& in_cfg, int& out_mapping, int& out_coupled)
{
	out_mapping = 255; // https://tools.ietf.org/html/rfc7845#section-5.1.1
	out_coupled = 0;
	AkUInt32 channelConfigType = in_cfg.eConfigType;
	if (channelConfigType == AK_ChannelConfigType_Standard)
	{
		AkChannelMask uMask = in_cfg.uChannelMask;
		switch (uMask)
		{
			// As per RFC 7845 5.1.1.1: Only Mono and Stereo (Left, Right)
		case AK_SPEAKER_SETUP_MONO:
		case AK_SPEAKER_SETUP_STEREO:
			out_mapping = 0;
			break;
			// As per RFC 7845 5.1.1.2:
			// 3 channels: linear surround(left, center, right).
			// 4 channels: quadraphonic (front left, front right, rear left, rear right).
			// 5 channels: 5.0 surround (front left, front center, front right, rear left, rear right).
			// 6 channels: 5.1 surround (front left, front center, front right, rear left, rear right, LFE).
			// 7 channels: 6.1 surround (front left, front center, front right, side left, side right, rear center, LFE).
			// 8 channels: 7.1 surround (front left, front center, front right, side left, side right, rear left, rear right, LFE).
		case AK_SPEAKER_SETUP_3STEREO:
		case AK_SPEAKER_SETUP_4:
		case AK_SPEAKER_SETUP_5:
		case AK_SPEAKER_SETUP_5POINT1:
		// case AK_SPEAKER_SETUP_6POINT1 omitted: not the same configuration as opus-6.1
		case AK_SPEAKER_SETUP_7POINT1:
			out_mapping = 1;
			break;
		default:
			break; // Leave default mapping family (255)
		}

		switch (uMask)
		{
			// As per RFC 7845 5.1.1.1: Only Mono and Stereo (Left, Right)		
		case AK_SPEAKER_SETUP_STEREO:
		case AK_SPEAKER_SETUP_3STEREO:
			out_coupled = 1;
			break;
		case AK_SPEAKER_SETUP_4:
		case AK_SPEAKER_SETUP_5:
		case AK_SPEAKER_SETUP_5POINT1:
			out_coupled = 2;
			break;
		// case AK_SPEAKER_SETUP_6POINT1 omitted: not the same configuration as opus-6.1
		case AK_SPEAKER_SETUP_7POINT1:
			out_coupled = 3;
			break;
		default:
			out_coupled = 0; // mapping family 255 has no coupling
			break;
		}
	}
	// A note re: Ambisonics: mapping 2 and 3 are not supported by Sony, so we'll encode with mapping 255.  Less efficient, but cross-platform.
	// See https://tools.ietf.org/html/rfc8486#section-3.1
}

void AK::Opus::WriteChannelMapping(AkUInt32 uNumChannels, unsigned char* in_pMapping, unsigned char* out_pMapping)
{
	if (in_pMapping == nullptr)
	{
		for (int c = 0; c < (int)uNumChannels; c++)
		{
			out_pMapping[c] = c;
		}
	}
	else
	{
		memcpy(out_pMapping, in_pMapping, uNumChannels);
	}
	for (int c = (int)uNumChannels; c < 255; c++)
	{
		out_pMapping[c] = 255;
	}
}

