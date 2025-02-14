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
#include "AkSrcFileOpus.h"
#include "AkCodecWemOpus.h"
#include <AK/SoundEngine/Common/IAkPlugin.h>

// Plugin mechanism. Dynamic create function whose address must be registered to the FX manager.
static IAkSoftwareCodec* CreateOpusSrcPlugin(void* in_pCtx)
{
	return AkNew(AkMemID_Processing, CAkSrcFileOpus((CAkPBI*)in_pCtx));
}

static IAkFileCodec* CreateOpusFileCodec()
{
	return AkNew(AkMemID_Processing, CAkOpusFileCodec());
}

AkCodecDescriptor g_AkOggOpusCodecDescriptor{
	CreateOpusSrcPlugin,
	CreateOpusSrcPlugin,
	CreateOpusFileCodec,
	nullptr
};

AK::PluginRegistration AkOggOpusDecoderRegistration(AKCOMPANYID_AUDIOKINETIC, AKCODECID_AKOPUS, g_AkOggOpusCodecDescriptor);

#if defined(AK_WEM_OPUS_HW_SUPPORTED)
// Platform-specific definition of the codec-creation function
IAkSrcMediaCodec* CreateCodecWemOpus(const AK::SrcMedia::Header* in_pHeader);
#else
IAkSrcMediaCodec* CreateCodecWemOpus(const AK::SrcMedia::Header* in_pHeader)
{
	return AkNew(AkMemID_Processing, CAkCodecWemOpus());
}
#endif

static IAkSoftwareCodec* CreateAkSrcMediaWemOpus(void* in_pCtx)
{
	return AkNew(AkMemID_Processing, CAkSrcMedia((CAkPBI*)in_pCtx, CreateCodecWemOpus));
}

static IAkFileCodec* CreateAkFileCodecOpus()
{
	return AkNew(AkMemID_Processing, CAkFileCodecWemOpus());
}

AkCodecDescriptor g_AkWemOpusCodecDescriptor{
	CreateAkSrcMediaWemOpus,
	CreateAkSrcMediaWemOpus,
	CreateAkFileCodecOpus,
	nullptr
};

AK::PluginRegistration AkWemOpusDecoderRegistration(AKCOMPANYID_AUDIOKINETIC, AKCODECID_AKOPUS_WEM, g_AkWemOpusCodecDescriptor);

void *AK_malloc(size_t in_size)
{
	return AkAlloc(AkMemID_Processing, in_size);
}

void *AK_calloc(size_t in_size, size_t in_count)
{
	return AkAlloc(AkMemID_Processing, in_size*in_count);
}

void *AK_realloc(void* p, size_t in_size)
{	
	return AkRealloc(AkMemID_Processing, p, in_size);
}

void AK_free(void* p)
{
	if(p != NULL)
		AkFree(AkMemID_Processing, p);
}