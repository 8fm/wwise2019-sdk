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
// AkActionBypassFX.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkActionBypassFX.h"
#include "AkAudioMgr.h"
#include "AkRegistryMgr.h"

CAkActionBypassFX::CAkActionBypassFX( AkActionType in_eActionType, AkUniqueID in_ulID ) 
: CAkActionExcept( in_eActionType, in_ulID )
, m_bIsBypass( false )
, m_uTargetMask( 0xFFFFFFFF )
{
}

CAkActionBypassFX::~CAkActionBypassFX()
{
}

AKRESULT CAkActionBypassFX::Execute( AkPendingAction * in_pAction )
{
	CAkParameterNodeBase* pNode = NULL;

	CAkRegisteredObj * pGameObj = in_pAction->GameObj();

	switch( ActionType() )
	{
	case AkActionType_BypassFX_M:
	case AkActionType_BypassFX_O:
		pNode = GetAndRefTarget();
		if( pNode )
		{
			pNode->BypassFX( m_bIsBypass ? m_uTargetMask : 0, m_uTargetMask, pGameObj );
			pNode->Release();
		}
		break;

	case AkActionType_ResetBypassFX_M:
	case AkActionType_ResetBypassFX_O:
		pNode = GetAndRefTarget();
		if( pNode )
		{
			pNode->ResetBypassFX( m_uTargetMask, pGameObj );
			pNode->Release();
		}
		break;

	case AkActionType_ResetBypassFX_ALL:
		{

			ResetBypassFXAEHelper( g_pRegistryMgr->GetModifiedElementList() );

			for (CAkModifiedNodes::tList::Iterator it = CAkModifiedNodes::List().Begin(); it != CAkModifiedNodes::List().End(); ++it)
			{
				CAkModifiedNodes* pModeNodes = ((CAkModifiedNodes*)*it);
				ResetBypassFXAEHelper(pModeNodes->GetModifiedElementList());
			}
		}
		break;

	case AkActionType_ResetBypassFX_ALL_O:
		{
			CAkModifiedNodes* pModNodes = in_pAction->GameObj()->GetComponent<CAkModifiedNodes>();
			if (pModNodes)
			{
				for (AkListNode::Iterator iter = pModNodes->GetModifiedElementList()->Begin(); iter != pModNodes->GetModifiedElementList()->End(); ++iter)
				{
					pNode = g_pIndex->GetNodePtrAndAddRef( *iter );
					if( pNode )
					{
						bool l_bIsException = false;
						for( ExceptionList::Iterator itExcept = m_listElementException.Begin(); itExcept != m_listElementException.End(); ++itExcept )
						{
							WwiseObjectID wwiseId( pNode->ID(), pNode->IsBusCategory() );
							if( (*itExcept) == wwiseId )
							{
								l_bIsException = true;
								break;
							}
						}
						if( !l_bIsException )
						{
							pNode->ResetBypassFX( m_uTargetMask, pGameObj );
						}
						pNode->Release();
					}
				}
			}
		}
		break;

	default:
		break;
	}

	return AK_Success;
}

void CAkActionBypassFX::ResetBypassFxAllHelper( const AkListNode* in_pListID )
{
	if( in_pListID )
	{
		for( AkListNode::Iterator iter = in_pListID->Begin(); iter != in_pListID->End(); ++iter )
		{
			CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef( *iter );
			if( pNode )
			{
				pNode->ResetBypassFX( m_uTargetMask );
				pNode->Release();
			}
		}
	}
}

void CAkActionBypassFX::ResetBypassFXAEHelper( const AkListNode* in_pListID )
{
	if( in_pListID )
	{
		for( AkListNode::Iterator iter = in_pListID->Begin(); iter != in_pListID->End(); ++iter )
		{
			CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef( *iter );
			if(pNode)
			{
				bool l_bIsException = false;
				for( ExceptionList::Iterator itExcept = m_listElementException.Begin(); itExcept != m_listElementException.End(); ++itExcept )
				{
					WwiseObjectID wwiseId( pNode->ID(), pNode->IsBusCategory() );
					if( (*itExcept) == wwiseId )
					{
						l_bIsException = true;
						break;
					}
				}
				if( !l_bIsException )
				{
					pNode->ResetBypassFX( m_uTargetMask );
				}
				pNode->Release();
			}
		}
	}
}

CAkActionBypassFX* CAkActionBypassFX::Create( AkActionType in_eActionType, AkUniqueID in_ulID )
{
	CAkActionBypassFX* pActionBypassFX = AkNew(AkMemID_Event, CAkActionBypassFX( in_eActionType, in_ulID ) );
	if( pActionBypassFX )
	{
		if( pActionBypassFX->Init() != AK_Success )
		{
			pActionBypassFX->Release();
			pActionBypassFX = NULL;
		}
	}
	return pActionBypassFX;
}

void CAkActionBypassFX::Bypass( const bool in_bIsBypass )
{
	m_bIsBypass = in_bIsBypass;
}

void CAkActionBypassFX::SetBypassTarget( bool in_bTargetAll, AkUInt32 in_uTargetMask )
{
	m_uTargetMask = in_uTargetMask;
	if ( in_bTargetAll )
		m_uTargetMask |= ( 1 << AK_NUM_EFFECTS_BYPASS_ALL_FLAG );
}

AKRESULT CAkActionBypassFX::SetActionParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	AkUInt8 l_cVal = READBANKDATA(AkUInt8, io_rpData, io_rulDataSize);
	m_bIsBypass = l_cVal != 0;

	m_uTargetMask = READBANKDATA(AkUInt8, io_rpData, io_rulDataSize);

	return SetExceptParams( io_rpData, io_rulDataSize );
}
