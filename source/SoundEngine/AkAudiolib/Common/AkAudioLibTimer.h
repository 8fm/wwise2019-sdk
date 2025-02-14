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
// AkAudiolibTimer.h
//
// alessard
//
//////////////////////////////////////////////////////////////////////
#ifndef _AKAUDIOLIBTIMER_H_
#define _AKAUDIOLIBTIMER_H_

#include <AK/Tools/Common/AkLock.h>
#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkAutoLock.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>

// NOTE: g_fFreqRatio was moved to namespace AK (for publication purposes).
// It is available in all builds.
namespace AK
{
    extern AkReal32 g_fFreqRatio;
}

namespace AkAudiolibTimer
{
	static const AkUInt32 uTimerEventMgrThread = AKMAKECLASSID(AkPluginTypeGlobalExtension, AKCOMPANYID_AUDIOKINETIC, AKEXTENSIONID_EVENTMGRTHREAD);

	struct Item
	{
		AkInt64 iStartTick;
		AkInt64 iStopTick;
		AkUInt32 uThreadIdx;
		AkPluginID uPluginID;
		AkPipelineID uPipelineID;
		AkUInt32 uExtraID;
	};

#define AKTIMERARRAY_GROWBY 1024
#define AKTIMERARRAY_GROWTHRESH 512 // Grow when fewer free items than

	typedef AkArray<Item, const Item &, ArrayPoolDefault> ItemArray;
	extern ItemArray items;

	extern AkAtomic32 iItemIdx;

#ifndef AK_OPTIMIZED

	inline void InitTimers()
	{
		AKPLATFORM::UpdatePerformanceFrequency();
	}

	inline void TermTimers()
	{
		items.Term();
	}

	inline void ResetTimers()
	{
		if ((AkInt32)items.Length() - AKTIMERARRAY_GROWTHRESH <= iItemIdx)
			items.Resize((AkUInt32)((iItemIdx + AKTIMERARRAY_GROWTHRESH + AKTIMERARRAY_GROWBY) / AKTIMERARRAY_GROWBY) * AKTIMERARRAY_GROWBY); // So that next frame will have the right capacity
		iItemIdx = 0;
	}

	extern Item * StartTimer(AkUInt32 in_uThreadIdx, AkPluginID in_uPluginID, AkPipelineID in_uPipelineID);
	extern void StopTimer(Item * in_pItem);
#endif //AK_OPTIMIZED
}

#define AK_START_TIMER_AUDIO()		AkAudiolibTimer::timerAudio.Start()

#ifndef AK_OPTIMIZED
#define AK_INIT_TIMERS()			AkAudiolibTimer::InitTimers()	// call it once at init time
#define AK_TERM_TIMERS()			AkAudiolibTimer::TermTimers()   // call it once at term time

#define AK_STOP_TIMER_AUDIO()		AkAudiolibTimer::timerAudio.Stop()

#define AK_START_TIMER( in_uThreadIdx, in_uPluginID, in_uPipelineID ) AkAudiolibTimer::StartTimer( in_uThreadIdx, in_uPluginID, in_uPipelineID )
#define AK_STOP_TIMER( in_pItem )  AkAudiolibTimer::StopTimer( in_pItem )

#else

// In release, do not create profiling timers, but compute global frequency ratio.
#define AK_INIT_TIMERS()            AKPLATFORM::UpdatePerformanceFrequency()
#define AK_TERM_TIMERS()

#define AK_STOP_TIMER_AUDIO()	

#define AK_START_TIMER( in_uThreadIdx, in_uPluginID, in_uPipelineID ) NULL
#define AK_STOP_TIMER( in_pItem )

#define AK_INCREMENT_TIMER_COUNT()
#define AK_DECREMENT_TIMER_COUNT()

#endif

#endif //_AKAUDIOLIBTIMER_H_
