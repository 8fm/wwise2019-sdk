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

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include "../AkCodecWemOpus.h"
#include "AkAjmWemOpus.h"

IAkSrcMediaCodec* CreateCodecWemOpus(const AK::SrcMedia::Header* in_pHeader)
{
	if (in_pHeader->FormatInfo.pFormat->nChannels <= 8)
	{
		return AkNew(AkMemID_Processing, CAkAjmWemOpus());
	}
	return AkNew(AkMemID_Processing, CAkCodecWemOpus());
}
