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
// AkActionPause.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkActionPause.h"
#include "AkAudioMgr.h"
#include "AkBanks.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>

extern CAkAudioMgr* g_pAudioMgr;

CAkActionPause::CAkActionPause(AkActionType in_eActionType, AkUniqueID in_ulID) 
: CAkActionActive(in_eActionType, in_ulID)
, m_bPausePendingResume( true )
{
}

CAkActionPause::~CAkActionPause()
{

}

CAkActionPause* CAkActionPause::Create(AkActionType in_eActionType, AkUniqueID in_ulID)
{
	CAkActionPause* pActionPause = AkNew(AkMemID_Event,CAkActionPause(in_eActionType, in_ulID));
	if( pActionPause )
	{
		if( pActionPause->Init() != AK_Success )
		{
			pActionPause->Release();
			pActionPause = NULL;
		}
	}

	return pActionPause;
}

AKRESULT CAkActionPause::Execute( AkPendingAction * in_pAction )
{
	AKASSERT(g_pAudioMgr);

	AKRESULT eResult = AK_Success;

	CAkRegisteredObj * pGameObj = in_pAction->GameObj();

	switch(CAkAction::ActionType())
	{
		case AkActionType_Pause_E_O:
		case AkActionType_Pause_E:
			{
				CAkParameterNodeBase* pNode = GetAndRefTarget();
				if( pNode )
				{
					eResult = Exec( ActionParamType_Pause, pGameObj, in_pAction->TargetPlayingID );
					g_pAudioMgr->PausePendingAction( pNode, pGameObj, m_bPausePendingResume, in_pAction->TargetPlayingID );
					pNode->Release();
				}
			}
			break;

		case AkActionType_Pause_ALL_O:
		case AkActionType_Pause_ALL:
			AllExecExcept( ActionParamType_Pause, pGameObj, in_pAction->TargetPlayingID );
			g_pAudioMgr->PausePendingActionAllExcept( pGameObj, &m_listElementException, m_bPausePendingResume, in_pAction->TargetPlayingID );
			break;

		default:
			AKASSERT(!"Should not happen, unsupported condition");
			break;
	}

	return eResult;
}

AKRESULT CAkActionPause::SetActionSpecificParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	AkUInt8 byBitVector = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
	IncludePendingResume( GETBANKDATABIT( byBitVector, BANK_BITPOS_ACTION_PAUSE_PAUSEDELAYEDRESUMEACTION ) != 0 );
	ApplyToStateTransitions( GETBANKDATABIT( byBitVector, BANK_BITPOS_ACTION_PAUSE_APPLYTOSTATETRANSITIONS ) != 0 );
	ApplyToDynamicSequence( GETBANKDATABIT( byBitVector, BANK_BITPOS_ACTION_PAUSE_APPLYTODYNAMICSEQUENCE ) != 0 );

	return AK_Success;
}

void CAkActionPause::SetActionActiveParams( ActionParams& io_params ) const
{
	io_params.bApplyToStateTransitions = m_bApplyToStateTransitions;
	io_params.bApplyToDynamicSequence = m_bApplyToDynamicSequence;
}
