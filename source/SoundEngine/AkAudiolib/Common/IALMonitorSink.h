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
// IALMonitorSink.h
//
// gduford
//
//////////////////////////////////////////////////////////////////////
#ifndef _IALMONITORSINK_H_
#define _IALMONITORSINK_H_

namespace AkMonitorData
{
	struct MonitorDataItem;
}

namespace AK
{
    class IALMonitorSink
    {
    public:
	    virtual void MonitorNotification( const AkMonitorData::MonitorDataItem& in_rMonitorItem, bool in_bAccumulate = false ) = 0;
		virtual void FlushAccumulated() = 0;
    };

	class IMonitorDataItemSink
	{
	public:
		virtual void HandleDataItem(AkTimeMs in_timeStamp, const AkMonitorData::MonitorDataItem& in_rMonitorItem) = 0;
	};
}

#endif	// _IALMONITORSINK_H_
