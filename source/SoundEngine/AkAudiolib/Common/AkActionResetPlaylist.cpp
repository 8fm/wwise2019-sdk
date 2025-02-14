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
// AkActionResetPlaylist.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkActionResetPlaylist.h"
#include "AkAudioMgr.h"
#include "AkAudioLibIndex.h"
#include "AkParameterNode.h"
#include "AkRanSeqCntr.h"

extern CAkAudioMgr* g_pAudioMgr;

CAkActionResetPlaylist::CAkActionResetPlaylist(AkActionType in_eActionType, AkUniqueID in_ulID)
: CAkActionActive(in_eActionType, in_ulID)
{
}

CAkActionResetPlaylist::~CAkActionResetPlaylist()
{

}

CAkActionResetPlaylist* CAkActionResetPlaylist::Create(AkActionType in_eActionType, AkUniqueID in_ulID)
{
	CAkActionResetPlaylist* pActionResetPlaylist = AkNew(AkMemID_Event, CAkActionResetPlaylist(in_eActionType, in_ulID));
	if (pActionResetPlaylist)
	{
		if (pActionResetPlaylist->Init() != AK_Success)
		{
			pActionResetPlaylist->Release();
			pActionResetPlaylist = NULL;
		}
	}

	return pActionResetPlaylist;
}

AKRESULT CAkActionResetPlaylist::Execute(AkPendingAction * in_pAction)
{
	AKRESULT eResult = AK_Success;

	CAkRegisteredObj * pGameObj = NULL;

	switch(CAkAction::ActionType())
	{
		case AkActionType_ResetPlaylist_E_O:
			pGameObj = in_pAction->GameObj();

			// No break here, fallthrough.

		case AkActionType_ResetPlaylist_E:
			{
				CAkParameterNodeBase* pTargetNode = GetAndRefTarget();
				if( pTargetNode )
				{
					if (pTargetNode->NodeCategory() == AkNodeCategory_RanSeqCntr)
					{
						((CAkRanSeqCntr*)pTargetNode)->SafeResetSpecificInfo(pGameObj);
					}
					pTargetNode->Release();
				}
			}
			break;

		default:
			AKASSERT(!"Should not happen, unsupported condition");
			break;
	}
	return eResult;
}

AKRESULT CAkActionResetPlaylist::SetActionSpecificParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize)
{
	// no params yet on this action.
	return AK_Success;
}
