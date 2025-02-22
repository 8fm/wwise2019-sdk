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
// AkActionTrigger.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkActionTrigger.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include "AkAudioMgr.h"
#include "AkStateMgr.h"

CAkActionTrigger::CAkActionTrigger(AkActionType in_eActionType, AkUniqueID in_ulID) 
: CAkAction(in_eActionType, in_ulID)
{
}

CAkActionTrigger::~CAkActionTrigger()
{

}

AKRESULT CAkActionTrigger::Execute( AkPendingAction * in_pAction )
{
	g_pStateMgr->Trigger( m_ulElementID, in_pAction->GameObj() );
	return AK_Success;
}

CAkActionTrigger* CAkActionTrigger::Create( AkActionType in_eActionType, AkUniqueID in_ulID )
{
	CAkActionTrigger* pActionTrigger = AkNew(AkMemID_Event, CAkActionTrigger( in_eActionType, in_ulID ) );
	if( pActionTrigger )
	{
		if( pActionTrigger->Init() != AK_Success )
		{
			pActionTrigger->Release();
			pActionTrigger = NULL;
		}
	}

	return pActionTrigger;
}

