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
// IALMonitor.h
//
// gduford
//
//////////////////////////////////////////////////////////////////////
#ifndef _IALMONITOR_H_
#define _IALMONITOR_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include "AkMonitorData.h"

namespace AK
{
    class IALMonitorSink;
	class IMonitorDataItemSink;

	class IALWatches
	{
	protected:
		virtual ~IALWatches(){}

	public:
		virtual void SetMeterWatches(AkMonitorData::MeterWatch* in_pWatches, AkUInt32 in_uiWatchCount) = 0;
	};
	
	class IALMonitor
		: public IALWatches
    {
	protected:
		virtual ~IALMonitor(){}

    public:
	    virtual void Register( IALMonitorSink* in_pMonitorSink, AkMonitorData::MaskType in_whatToMonitor ) = 0;
	    virtual void Unregister( IALMonitorSink* in_pMonitorSink ) = 0;
	};

	// Authoring-side interface
	class IMonitor
		: public IALWatches
	{
	protected:
		virtual ~IMonitor(){}

	public:
		virtual void Monitor(IMonitorDataItemSink* in_pMonitorSink, AkMonitorData::MaskType in_whatToMonitor) = 0;
		virtual AkReal32 GetFreqRatio() = 0;
		virtual AkUInt32 GetCoreSampleRate() = 0;
		virtual AkUInt32 GetFramesPerBuffer() = 0;
		virtual AkTimeMs GetTimeStamp() = 0;
	};
}

#endif	// _IALMONITOR_H_
