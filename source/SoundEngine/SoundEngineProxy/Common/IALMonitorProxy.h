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
#pragma once

#ifndef AK_OPTIMIZED

#include "IALMonitor.h"
#include "CommandData.h"

class IALMonitorProxy : public AK::IALMonitor
{
public:
	virtual bool StartMonitoring() = 0;
	virtual void StopMonitoring() = 0;
};

class IMonitorProxy : public AK::IMonitor
{
public:
	virtual bool StartMonitoring() = 0;
	virtual void StopMonitoring() = 0;
};

namespace ALMonitorProxyCommandData
{
	struct CommandData : public ProxyCommandData::CommandData
	{
		CommandData();
		CommandData( AkUInt16 in_methodID );

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		DECLARE_BASECLASS( ProxyCommandData::CommandData );
	};

	struct Monitor : public CommandData
	{
		Monitor();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkMonitorData::MaskType m_uWhatToMonitor;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetMeterWatches : public CommandData
	{
		SetMeterWatches();
		~SetMeterWatches();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_uiWatchCount;
		AkMonitorData::MeterWatch* m_pWatches;

	private:
		DECLARE_BASECLASS( CommandData );
	};

}

#endif // #ifndef AK_OPTIMIZED
