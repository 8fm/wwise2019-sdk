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
// AkBusCallbackMgr.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include "AkBusCallbackMgr.h"
#include <AK/Tools/Common/AkAutoLock.h>
#include "AkLEngine.h"

CAkBusCallbackMgr::CAkBusCallbackMgr()
{
}

CAkBusCallbackMgr::~CAkBusCallbackMgr()
{
	m_ListCallbacks.Term();
	m_ListMeteringCallbacks.Term();
}

AKRESULT CAkBusCallbackMgr::SetVolumeCallback( AkUniqueID in_busID, AkBusCallbackFunc in_pfnCallback )
{
	{
		AkAutoLock<CAkLock> gate(m_csLock);
		if ( in_pfnCallback )
		{
			AkBusCallbackFunc* pItem = m_ListCallbacks.Exists( in_busID );
			if( !pItem )
			{
				pItem = m_ListCallbacks.Set( in_busID );
				if( !pItem )
					return AK_InsufficientMemory;
			}
			*pItem = in_pfnCallback;
		}
		else
		{
			m_ListCallbacks.Unset( in_busID );
		}
	}
	CAkLEngine::EnableVolumeCallback( in_busID, in_pfnCallback != NULL );

	return AK_Success;
}

AKRESULT CAkBusCallbackMgr::SetMeteringCallback( AkUniqueID in_busID, AkBusMeteringCallbackFunc in_pfnCallback, AkMeteringFlags in_eMeteringFlags )
{
	{
		AkAutoLock<CAkLock> gate(m_csLock);
		if ( in_pfnCallback || in_eMeteringFlags == AK_NoMetering )
		{
			MeterCallbackInfo * pItem = m_ListMeteringCallbacks.Exists( in_busID );
			if( !pItem )
			{
				pItem = m_ListMeteringCallbacks.Set( in_busID );
				if( !pItem )
					return AK_InsufficientMemory;
				pItem->Clear();
			}
			pItem->pCallback = in_pfnCallback;
			pItem->eFlags = in_eMeteringFlags;
		}
		else
		{
			m_ListMeteringCallbacks.Unset( in_busID );
		}
	}
	CAkLEngine::EnableMeteringCallback( in_busID, ( in_pfnCallback != NULL ) ? in_eMeteringFlags : AK_NoMetering );

	return AK_Success;
}

bool CAkBusCallbackMgr::DoVolumeCallback( AkUniqueID in_busID, AkSpeakerVolumeMatrixCallbackInfo& in_rCallbackInfo )
{
	AkAutoLock<CAkLock> gate(m_csLock);
	AkBusCallbackFunc* pItem = m_ListCallbacks.Exists( in_busID );
	if( pItem )
	{
		(*pItem)(&in_rCallbackInfo);
		return true;
	}
	return false;
}

bool CAkBusCallbackMgr::DoMeteringCallback( AkUniqueID in_busID, AK::IAkMetering * in_pMetering, AkChannelConfig in_channelConfig )
{
	AkAutoLock<CAkLock> gate(m_csLock);
	MeterCallbackInfo* pItem = m_ListMeteringCallbacks.Exists( in_busID );
	if( pItem )
	{
		// Note: only passing flags that were asked for via the API.
		( pItem->pCallback )( in_pMetering, in_channelConfig, pItem->eFlags );
		return true;
	}
	return false;
}

bool CAkBusCallbackMgr::IsVolumeCallbackEnabled( AkUniqueID in_busID )
{
	AkAutoLock<CAkLock> gate(m_csLock);
	AkBusCallbackFunc* pItem = m_ListCallbacks.Exists( in_busID );
	return pItem?true:false;
}

AkMeteringFlags CAkBusCallbackMgr::IsMeteringCallbackEnabled( AkUniqueID in_busID )
{
	AkAutoLock<CAkLock> gate(m_csLock);
	MeterCallbackInfo* pItem = m_ListMeteringCallbacks.Exists( in_busID );
	return ( pItem ) ? pItem->eFlags : AK_NoMetering;
}
