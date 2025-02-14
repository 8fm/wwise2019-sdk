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
// AkSwitchAware.h
// Basic interface and implementation for containers that are 
// switch/state dependent.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkSwitchAware.h"
#include "AkSwitchMgr.h"
#include "AkStateMgr.h"
#include "AkRTPCKey.h"

CAkSwitchAware::CAkSwitchAware()
{
}

CAkSwitchAware::~CAkSwitchAware()
{
}

void CAkSwitchAware::UnsubscribeSwitches()
{
    g_pSwitchMgr->UnSubscribeSwitch( this );
	g_pStateMgr->UnregisterSwitch( this );
}

AKRESULT CAkSwitchAware::SubscribeSwitch( 
    AkUInt32 in_ulGroup, 
    AkGroupType in_eGroupType 
    )
{
    AKRESULT	eResult = AK_Success;

	AKASSERT( g_pSwitchMgr && g_pStateMgr );

	UnsubscribeSwitches();

	if( in_ulGroup )
	{
		switch( in_eGroupType )
		{
		case AkGroupType_Switch:
			eResult = g_pSwitchMgr->SubscribeSwitch( this, in_ulGroup );
			break;
		case AkGroupType_State:
			eResult = g_pStateMgr->RegisterSwitch( this, in_ulGroup );
			break;
		default:
			AKASSERT( !"Unknown AkGroupType" );
			eResult = AK_InvalidParameter;
			break;
		}
	}

	return eResult;
}

AkSwitchStateID CAkSwitchAware::GetSwitchToUse( 
	const AkRTPCKey& in_rtpcKey,
    AkUInt32 in_ulGroup, 
    AkGroupType in_eGroupType 
    )
{
	AKASSERT( g_pSwitchMgr && g_pStateMgr );

	// Get the switch to use
	AkSwitchStateID ulSwitchState;
	if( in_eGroupType == AkGroupType_State)
	{
		ulSwitchState = g_pStateMgr->GetState( in_ulGroup );
	}
	else
	{
		AKASSERT( in_eGroupType == AkGroupType_Switch );
		ulSwitchState = g_pSwitchMgr->GetSwitch( in_ulGroup, in_rtpcKey );
	}

	return ulSwitchState;
}
