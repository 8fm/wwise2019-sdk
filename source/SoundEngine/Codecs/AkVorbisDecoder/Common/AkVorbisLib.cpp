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
#include "AkSrcBankVorbis.h"
#include "AkSrcFileVorbis.h"
#include <AK/SoundEngine/Common/IAkPlugin.h>

// Plugin mechanism. Dynamic create function whose address must be registered to the FX manager.
IAkSoftwareCodec* CreateVorbisSrcPlugin(void* in_pCtx)
{
	return AkNew(AkMemID_Processing, CAkSrcFileVorbis((CAkPBI*)in_pCtx));
}

IAkFileCodec * CreateVorbisFileCodec()
{
	return AkNew(AkMemID_Processing, CAkVorbisFileCodec());
}

IAkGrainCodec * CreateVorbisGrainCodec()
{
	return AkNew(AkMemID_Processing, CAkVorbisGrainCodec());
}

AkCodecDescriptor g_VorbisCodecDescriptor{
	CreateVorbisSrcPlugin,
	CreateVorbisSrcPlugin,
	CreateVorbisFileCodec,
	CreateVorbisGrainCodec
};

AK::PluginRegistration AkVorbisDecoderRegistration(AKCOMPANYID_AUDIOKINETIC, AKCODECID_VORBIS, g_VorbisCodecDescriptor);
