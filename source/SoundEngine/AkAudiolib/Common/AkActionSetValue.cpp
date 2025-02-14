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
// AkActionSetValue.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkActionSetValue.h"
#include "AkAudioMgr.h"
#include "AkRegistryMgr.h"

CAkActionSetValue::CAkActionSetValue(AkActionType in_eActionType, AkUniqueID in_ulID) 
: CAkActionExcept(in_eActionType, in_ulID)
{
}

CAkActionSetValue::~CAkActionSetValue()
{

}

AKRESULT CAkActionSetValue::Execute( AkPendingAction * in_pAction )
{
	AKASSERT(g_pIndex);
	AKASSERT(g_pRegistryMgr);

	CAkParameterNodeBase* pNode = NULL;

	switch(ActionType())
	{
		case AkActionType_SetVolume_O:
		case AkActionType_SetPitch_O:
		case AkActionType_Mute_O:
		case AkActionType_SetLPF_O:
		case AkActionType_SetHPF_O:
		case AkActionType_SetBusVolume_O:
			pNode = GetAndRefTarget();
			if(pNode)
			{
				ExecSetValue(pNode, in_pAction->GameObj());
				pNode->Release();
			}
			break;
		case AkActionType_SetVolume_M:
		case AkActionType_SetPitch_M:
		case AkActionType_Mute_M:
		case AkActionType_SetLPF_M:
		case AkActionType_SetHPF_M:
		case AkActionType_SetBusVolume_M:
			pNode = GetAndRefTarget();
			if(pNode)
			{
				ExecSetValue(pNode);
				pNode->Release();
			}
			break;
		case AkActionType_ResetVolume_O:
		case AkActionType_ResetPitch_O:
		case AkActionType_Unmute_O:
		case AkActionType_ResetLPF_O:
		case AkActionType_ResetHPF_O:
		case AkActionType_ResetBusVolume_O:
			pNode = GetAndRefTarget();
			if(pNode)
			{
				ExecResetValue(pNode,in_pAction->GameObj());
				pNode->Release();
			}
			break;
		case AkActionType_ResetVolume_M:
		case AkActionType_ResetPitch_M:
		case AkActionType_Unmute_M:
		case AkActionType_ResetLPF_M:
		case AkActionType_ResetHPF_M:
		case AkActionType_ResetBusVolume_M:
			pNode = GetAndRefTarget();
			if(pNode)
			{
				ExecResetValue(pNode);
				pNode->Release();
			}
			break;
		case AkActionType_ResetVolume_ALL:
		case AkActionType_ResetPitch_ALL:
		case AkActionType_Unmute_ALL:
		case AkActionType_ResetLPF_ALL:
		case AkActionType_ResetHPF_ALL:
		case AkActionType_ResetBusVolume_ALL:
			{
				ResetAEHelper( g_pRegistryMgr->GetModifiedElementList() );

				for (CAkModifiedNodes::tList::Iterator it = CAkModifiedNodes::List().Begin(); it != CAkModifiedNodes::List().End(); ++it)
				{
					CAkModifiedNodes* pModNodes = ((CAkModifiedNodes*)*it);
					ResetAEHelper(pModNodes->GetModifiedElementList());
				}

				break;
			}
		case AkActionType_ResetVolume_ALL_O:
		case AkActionType_ResetPitch_ALL_O:
		case AkActionType_Unmute_ALL_O:
		case AkActionType_ResetLPF_ALL_O:
		case AkActionType_ResetHPF_ALL_O:
			{
				CAkModifiedNodes* pModNodes = in_pAction->GameObj()->GetComponent<CAkModifiedNodes>();
				if (pModNodes)
				{
					for (AkListNode::Iterator iter = pModNodes->GetModifiedElementList()->Begin(); iter != pModNodes->GetModifiedElementList()->End(); ++iter)
					{
						pNode = g_pIndex->GetNodePtrAndAddRef( *iter );
						if( pNode )
						{
							ExecResetValueExcept( pNode, in_pAction->GameObj() );
							pNode->Release();
						}
					}
				}
				break;
			}
		case AkActionType_SetGameParameter_O:
		case AkActionType_SetGameParameter:
			ExecSetValue(NULL, in_pAction->GameObj());
			break;
		case AkActionType_ResetGameParameter_O:
		case AkActionType_ResetGameParameter:
			ExecResetValue(NULL,in_pAction->GameObj());
			break;
		default:
			AKASSERT(!"Unknown or unsupported Action Type Requested");
			break;
	}
	return AK_Success;
}

void CAkActionSetValue::ResetAEHelper( const AkListNode* in_pListID )
{
	if( in_pListID )
	{
		for( AkListNode::Iterator iter = in_pListID->Begin(); iter != in_pListID->End(); ++iter )
		{
			CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef( *iter );
			if( pNode )
			{
				ExecResetValueExcept( pNode );
				pNode->Release();
			}
		}
	}
}

AKRESULT CAkActionSetValue::SetActionParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	AkUInt8 ucFadeCurveType = READBANKDATA(AkUInt8, io_rpData, io_rulDataSize );

	m_eFadeCurve = (AkCurveInterpolation)ucFadeCurveType;

	AKRESULT eResult = SetActionSpecificParams(io_rpData, io_rulDataSize);

	if( eResult == AK_Success )
		eResult = SetExceptParams( io_rpData, io_rulDataSize );

	return eResult;
}
