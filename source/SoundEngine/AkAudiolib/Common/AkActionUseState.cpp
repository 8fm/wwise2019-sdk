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
// AkActionUseState.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkActionUseState.h"
#include "AkAudioLibIndex.h"
#include "AkParameterNode.h"

CAkActionUseState::CAkActionUseState(AkActionType in_eActionType, AkUniqueID in_ulID) 
: CAkAction(in_eActionType, in_ulID)
{
}

CAkActionUseState::~CAkActionUseState()
{

}
AKRESULT CAkActionUseState::Execute( AkPendingAction * )
{
	AKASSERT(g_pIndex);
	CAkParameterNodeBase* pNode = GetAndRefTarget();
	if(pNode)
	{
		pNode->UseState(CAkAction::ActionType() == AkActionType_UseState_E);
		pNode->Release();
	}
	return AK_Success;
}

CAkActionUseState* CAkActionUseState::Create(AkActionType in_eActionType, AkUniqueID in_ulID)
{
	CAkActionUseState* pActionUseState = AkNew(AkMemID_Event,CAkActionUseState(in_eActionType, in_ulID));
	if( pActionUseState )
	{
		if( pActionUseState->Init() != AK_Success )
		{
			pActionUseState->Release();
			pActionUseState = NULL;
		}
	}

	return pActionUseState;
}

void CAkActionUseState::UseState(bool in_bUseState)
{
	ActionType(in_bUseState?AkActionType_UseState_E:AkActionType_UnuseState_E);
}
