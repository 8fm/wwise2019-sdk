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
#ifndef AK_OPTIMIZED
#ifndef PROXYCENTRAL_CONNECTED

#include "ALMonitorProxyLocal.h"

#include "AkAudioLib.h"
#include "IALMonitor.h"
#include "AkMonitorData.h"
#include "AkCritical.h"

ALMonitorProxyLocal::ALMonitorProxyLocal( IALMonitorSubProxyHolder * in_pHolder )
	: m_pHolder( in_pHolder )
	, m_bRegistered( false )
{
	AKASSERT( m_pHolder );
}

ALMonitorProxyLocal::~ALMonitorProxyLocal()
{
}

void ALMonitorProxyLocal::Monitor( AkMonitorData::MaskType in_uWhatToMonitor )
{
    AK::SoundEngine::GetMonitor()->Register( this, in_uWhatToMonitor );
	m_bRegistered = in_uWhatToMonitor != 0;
}

void ALMonitorProxyLocal::SetMeterWatches( AkMonitorData::MeterWatch* in_pWatches, AkUInt32 in_uiWatchCount )
{
	CAkFunctionCritical SpaceSetAsCritical;
	AK::SoundEngine::GetMonitor()->SetMeterWatches( in_pWatches, in_uiWatchCount );
}

void ALMonitorProxyLocal::MonitorNotification( const AkMonitorData::MonitorDataItem& in_rMonitorItem, bool in_bAccumulate )
{
	size_t sizeItem = AkMonitorData::RealSizeof( in_rMonitorItem );
	AkMonitorData::MonitorDataItem * pOurItem = (AkMonitorData::MonitorDataItem *) malloc( sizeItem );
	memcpy( pOurItem, &in_rMonitorItem, sizeItem );

	m_pHolder->MonitorNotification( pOurItem );
}

void ALMonitorProxyLocal::FlushAccumulated()
{
}

#endif // PROXYCENTRAL_CONNECTED
#endif // AK_OPTIMIZED
