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
// AkActionPlayEvent.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkActionPlayEvent.h"
#include "AkAudioLibIndex.h"
#include "AkParameterNode.h"
#include "AkMonitor.h"
#include "AkEvent.h"
#include "AkAudioMgr.h"


CAkActionPlayEvent::CAkActionPlayEvent(AkActionType in_eActionType, AkUniqueID in_ulID) 
: CAkAction(in_eActionType, in_ulID)
{
}

CAkActionPlayEvent::~CAkActionPlayEvent()
{
	
}

AKRESULT CAkActionPlayEvent::Execute( AkPendingAction * in_pAction )
{
	AKRESULT eResult = AK_Success;

	CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( m_ulElementID );
	if(pEvent)
	{		
		CAkAudioMgr::ExecuteEvent(pEvent, in_pAction->GameObj(), in_pAction->GameObjID(), in_pAction->UserParam.PlayingID(), AK_INVALID_PLAYING_ID, in_pAction->UserParam.CustomParam());
		pEvent->Release();
	}
	else
	{
		CAkCntrHist HistArray;
		MONITOR_OBJECTNOTIF( in_pAction->UserParam.PlayingID(), in_pAction->GameObjID(), in_pAction->UserParam.CustomParam(), AkMonitorData::NotificationReason_PlayFailed, HistArray, m_ulElementID, false, 0 );
		eResult = AK_IDNotFound;
	}
	return eResult;
}

CAkActionPlayEvent* CAkActionPlayEvent::Create(AkActionType in_eActionType, AkUniqueID in_ulID)
{
	CAkActionPlayEvent*	pActionPlayEvent = AkNew(AkMemID_Event,CAkActionPlayEvent(in_eActionType, in_ulID));
	if( pActionPlayEvent )
	{
		if( pActionPlayEvent->Init() != AK_Success )
		{
			pActionPlayEvent->Release();
			pActionPlayEvent = NULL;
		}
	}

	return pActionPlayEvent;
}

AKRESULT CAkActionPlayEvent::SetActionParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	return AK_Success;
}
