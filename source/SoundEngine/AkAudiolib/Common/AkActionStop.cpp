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
// AkActionStop.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkActionStop.h"
#include "AkAudioMgr.h"
#include "AkBanks.h"
#include "AkURenderer.h"
#include "AkAudioLibTimer.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>

extern CAkAudioMgr* g_pAudioMgr;

CAkActionStop::CAkActionStop(AkActionType in_eActionType, AkUniqueID in_ulID) 
: CAkActionActive(in_eActionType, in_ulID)
, m_bApplyToStateTransitions(false)
, m_bApplyToDynamicSequence(false)
{
}

CAkActionStop::~CAkActionStop()
{

}

CAkActionStop* CAkActionStop::Create(AkActionType in_eActionType, AkUniqueID in_ulID)
{
	CAkActionStop* pActionStop = AkNew(AkMemID_Event,CAkActionStop(in_eActionType, in_ulID));
	if( pActionStop )
	{
		if( pActionStop->Init() != AK_Success )
		{
			pActionStop->Release();
			pActionStop = NULL;
		}
	}

	return pActionStop;
}

AKRESULT CAkActionStop::Execute( AkPendingAction * in_pAction )
{
	AKASSERT(g_pAudioMgr);

	AKRESULT eResult = AK_Success;

	CAkRegisteredObj * pGameObj = in_pAction->GameObj();

	switch(CAkAction::ActionType())
	{
		case AkActionType_Stop_E:
		case AkActionType_Stop_E_O:
			{
				CAkSmartPtr<CAkParameterNodeBase> pTargetNode;
				pTargetNode.Attach( GetAndRefTarget() );
				if( pTargetNode )
				{
					eResult = Exec( ActionParamType_Stop, pGameObj, in_pAction->TargetPlayingID );
					g_pAudioMgr->StopPendingAction( pTargetNode, pGameObj, in_pAction->TargetPlayingID );
				}
			}
			break;

		case AkActionType_Stop_ALL:
		case AkActionType_Stop_ALL_O:
		{
			if (m_listElementException.Length() > 0)
			{
				AllExecExcept(ActionParamType_Stop, pGameObj, in_pAction->TargetPlayingID);
			}
			else
			{
				ActionParams lParams;
				lParams.bIsFromBus = false;
				lParams.bIsMasterCall = pGameObj ? false : true;
				lParams.transParams.eFadeCurve = (AkCurveInterpolation)m_eFadeCurve;
				lParams.eType = ActionParamType_Stop;
				lParams.pGameObj = pGameObj;
				lParams.playingID = in_pAction->TargetPlayingID;
				lParams.transParams.TransitionTime = GetTransitionTime();
				SetActionActiveParams(lParams);
				ExecOnDynamicSequences(lParams);
				CAkURenderer::StopAll(lParams);
			}
			
			g_pAudioMgr->StopPendingActionAllExcept(pGameObj, &m_listElementException, in_pAction->TargetPlayingID);
			break;
		}
		default:
			AKASSERT(!"Should not happen, unsupported Stop condition");
			break;
	}
	return eResult;
}

AKRESULT CAkActionStop::SetActionSpecificParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	AkUInt8 byBitVector = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );

	ApplyToStateTransitions( GETBANKDATABIT( byBitVector, BANK_BITPOS_ACTION_STOP_APPLYTOSTATETRANSITIONS ) != 0 );
	ApplyToDynamicSequence( GETBANKDATABIT( byBitVector, BANK_BITPOS_ACTION_STOP_APPLYTODYNAMICSEQUENCE ) != 0 );

	return AK_Success;
}

void CAkActionStop::SetActionActiveParams( ActionParams& io_params ) const
{
	io_params.bApplyToStateTransitions = m_bApplyToStateTransitions;
	io_params.bApplyToDynamicSequence = m_bApplyToDynamicSequence;
}
