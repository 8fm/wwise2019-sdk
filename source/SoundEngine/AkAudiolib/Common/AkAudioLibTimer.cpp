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
// AkAudiolibTimer.cpp
//
// alessard
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkAudioLibTimer.h"

namespace AkAudiolibTimer
{
	ItemArray items;
	AkAtomic32 iItemIdx = 0;
	Item * StartTimer(AkUInt32 in_uThreadIdx, AkUInt32 in_uPluginID, AkUInt32 in_uPipelineID)
	{
		AkInt32 iNextIdx = AkAtomicInc32(&iItemIdx);
		AkInt32 iCurIdx = iNextIdx - 1;
		if (iCurIdx < (AkInt32)items.Length()) // There's no lockless way to grow this array; see ResetTimers
		{
			Item * pItem = &items[iCurIdx];
			AKPLATFORM::PerformanceCounter(&pItem->iStartTick);
			pItem->uThreadIdx = in_uThreadIdx;
			pItem->uPluginID = in_uPluginID;
			pItem->uPipelineID = in_uPipelineID;
			pItem->uExtraID = 0;

			return pItem;
		}
		else
		{
			return NULL;
		}
	}

	void StopTimer(Item * in_pItem)
	{
		if (in_pItem)
			AKPLATFORM::PerformanceCounter(&in_pItem->iStopTick);
	}
}

namespace AK
{
    AkReal32 g_fFreqRatio;
}
