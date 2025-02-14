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

#include "stdafx.h"
#include "AkAudioMix.h"

AKRESULT AkAudioMix::Allocate( AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut )
{
	AKRESULT res = AK_Success;

	AkUInt32 uNewMemSize = GetRequiredSize( in_uNumChannelsIn, in_uNumChannelsOut );
	if (uNewMemSize != uAllocdMemSize)
	{
		if (uAllocdMemSize > 0)
			Free();

		pMemory = AK::SpeakerVolumes::HeapAlloc(uNewMemSize, AkMemID_Processing);
		if (pMemory)
		{
			uAllocdMemSize = uNewMemSize;
			pNext = (AK::SpeakerVolumes::MatrixPtr)pMemory;
			pPrevious = (AK::SpeakerVolumes::MatrixPtr)((AkUInt8*)pMemory + uNewMemSize / 2);
			res = AK_Success;
		}
		else
		{
			res = AK_Fail;
		}
	}

	return res;
}

void AkAudioMix::Free()
{
	AK::SpeakerVolumes::HeapFree(pMemory, AkMemID_Processing);
	pMemory= NULL;
	uAllocdMemSize = 0;
	pNext = NULL;
	pPrevious = NULL;
}
