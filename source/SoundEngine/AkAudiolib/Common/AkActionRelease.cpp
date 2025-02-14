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
// AkActionRelease.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkActionRelease.h"
#include "AkAudioMgr.h"
#include "AkAudioLibIndex.h"
#include "AkParameterNode.h"

CAkActionRelease::CAkActionRelease(AkActionType in_eActionType, AkUniqueID in_ulID) 
: CAkActionActive(in_eActionType, in_ulID)
{
}

CAkActionRelease::~CAkActionRelease()
{
	
}

CAkActionRelease* CAkActionRelease::Create(AkActionType in_eActionType, AkUniqueID in_ulID)
{
	CAkActionRelease*	pActionRelease = AkNew(AkMemID_Event,CAkActionRelease(in_eActionType, in_ulID));
	if( pActionRelease )
	{
		if( pActionRelease->Init() != AK_Success )
		{
			pActionRelease->Release();
			pActionRelease = NULL;
		}
	}

	return pActionRelease;
}

AKRESULT CAkActionRelease::Execute( AkPendingAction * in_pAction )
{
	AKRESULT eResult = AK_Fail;

	CAkRegisteredObj * pGameObj = in_pAction->GameObj();

	switch(CAkAction::ActionType())
	{
	case AkActionType_Release:
	case AkActionType_Release_O:
		{
			CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
			pTargetNode.Attach( GetAndRefTarget() );
			if( pTargetNode )
			{
				eResult = Exec( ActionParamType_Release, pGameObj, in_pAction->TargetPlayingID );
			}
		}
		break;

	default:
		AKASSERT(!"Should not happen, unsupported Release condition");
		break;
	}
	return eResult;
}


AKRESULT CAkActionRelease::SetActionParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	return AK_Success;
}
