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
#include "AkActionResume.h"
#include "AkAudioMgr.h"
#include "AkAudioLibIndex.h"
#include "AkParameterNode.h"
#include "AkBanks.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>

extern CAkAudioMgr* g_pAudioMgr;

CAkActionResume::CAkActionResume(AkActionType in_eActionType, AkUniqueID in_ulID) 
: CAkActionActive(in_eActionType, in_ulID)
{
}

CAkActionResume::~CAkActionResume()
{

}

CAkActionResume* CAkActionResume::Create(AkActionType in_eActionType, AkUniqueID in_ulID)
{
	CAkActionResume* pActionResume = AkNew(AkMemID_Event,CAkActionResume(in_eActionType, in_ulID));
	if( pActionResume )
	{
		if( pActionResume->Init() != AK_Success )
		{
			pActionResume->Release();
			pActionResume = NULL;
		}
	}

	return pActionResume;
}

AKRESULT CAkActionResume::Execute( AkPendingAction * in_pAction )
{
	AKASSERT(g_pAudioMgr);

	AKRESULT eResult = AK_Success;

	CAkRegisteredObj * pGameObj = in_pAction->GameObj();

	switch(CAkAction::ActionType())
	{
		case AkActionType_Resume_E_O:
		case AkActionType_Resume_E:
			{
				CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
				pTargetNode.Attach( GetAndRefTarget() );
				if( pTargetNode )
				{
					eResult = Exec( ActionParamType_Resume, pGameObj, in_pAction->TargetPlayingID );
					g_pAudioMgr->ResumePausedPendingAction( pTargetNode, pGameObj, m_bIsMasterResume, in_pAction->TargetPlayingID );
				}
			}
			break;

		case AkActionType_Resume_ALL_O:
		case AkActionType_Resume_ALL:
			AllExecExcept( ActionParamType_Resume, pGameObj, in_pAction->TargetPlayingID );
			g_pAudioMgr->ResumePausedPendingActionAllExcept( pGameObj, &m_listElementException, m_bIsMasterResume, in_pAction->TargetPlayingID );
			break;

		default:
			AKASSERT(!"Should not happen, unsupported condition");
			break;
	}
	return eResult;
}

AKRESULT CAkActionResume::SetActionSpecificParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	AkUInt8 byBitVector = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );

	IsMasterResume( GETBANKDATABIT( byBitVector, BANK_BITPOS_ACTION_RESUME_MASTERRESUME ) != 0 );
	ApplyToStateTransitions( GETBANKDATABIT( byBitVector, BANK_BITPOS_ACTION_RESUME_APPLYTOSTATETRANSITIONS ) != 0 );
	ApplyToDynamicSequence( GETBANKDATABIT( byBitVector, BANK_BITPOS_ACTION_RESUME_APPLYTODYNAMICSEQUENCE ) != 0 );

	return AK_Success;
}

void CAkActionResume::SetActionActiveParams( ActionParams& io_params ) const
{
	io_params.bIsMasterResume = m_bIsMasterResume;
	io_params.bApplyToStateTransitions = m_bApplyToStateTransitions;
	io_params.bApplyToDynamicSequence = m_bApplyToDynamicSequence;
}
