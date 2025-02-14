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
#pragma once

#include <AK/SoundEngine/Common/AkSoundEngine.h>

#define OPUS_PREROLL_MS 80             // RFC 7845 recommends an 80ms pre-roll https://tools.ietf.org/html/rfc7845#section-4.6
#define OPUS_RATE 48000
#define SEEK_UNINITIALIZED 0xFFFFFFFF
#define AK_OPUS_MAX_STREAMS 255

struct WaveFormatOpus : public WaveFormatExtensible
{
	AkUInt32 dwTotalPCMFrames;			// Total number of encoded PCM frames
	AkUInt32 dwEndGranuleOffset;		// First byte of the last granule, in offset relative to the opus file start
	AkUInt32 dwEndGranulePCM;			// First PCM offet of the last granule.	
};

#pragma pack (push)
#pragma pack(1)
struct WaveFormatOpusWEM : public WaveFormatExtensible
{
	AkUInt32 dwTotalPCMFrames;			// Total number of encoded PCM frames
	AkUInt32 uSeekTableSize;			// Number of entries. 
	AkInt16  uCodecDelay;               // Number of samples to skip from start of stream
	AkUInt8  uCodecVersion;             // Version of WEM Opus media format
	AkUInt8  uMappingFamily;
};
#pragma pack (pop)

namespace AK
{
	namespace Opus
	{
		bool ValidateFormat(WaveFormatOpusWEM* pFormat);
		void ChannelConfigToMapping(const AkChannelConfig &in_cfg, int& out_mapping, int& out_coupled);
		void WriteChannelMapping(AkUInt32 uNumChannels, unsigned char * in_pMapping, unsigned char* out_pMapping);
	}
}
