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
// AkActionPlay.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkActionPlay.h"
#include "AkAudioMgr.h"
#include "AkParameterNode.h"

CAkActionPlay::CAkActionPlay(AkActionType in_eActionType, AkUniqueID in_ulID) 
: CAkAction(in_eActionType, in_ulID)
, m_bankID(AK_INVALID_BANK_ID)
{
}

CAkActionPlay::~CAkActionPlay()
{
	
}

AKRESULT CAkActionPlay::Execute( AkPendingAction * in_pAction )
{
	AKRESULT eResult = AK_Success;

	AkPropValue * pProbability = m_props.FindProp( AkPropID_Probability );
	if ( AK_EXPECT_FALSE( pProbability != NULL ) )
	{
		if( ( pProbability->fValue == 0.0f
			|| (AKRANDOM::AkRandom() / (AkReal64)AKRANDOM::AK_RANDOM_MAX) * 100 > pProbability->fValue))
			return AK_Success;
	}

	CAkParameterNodeBase* pNode = GetAndRefTarget();

	if(pNode)
	{
		TransParams	Tparameters;

		Tparameters.TransitionTime = GetTransitionTime();
		Tparameters.eFadeCurve = (AkCurveInterpolation)m_eFadeCurve;

		AkPBIParams pbiParams;
        
		pbiParams.eType = AkPBIParams::PBI;
        pbiParams.pInstigator = pNode;
		pbiParams.userParams = in_pAction->UserParam;
		pbiParams.ePlaybackState = PB_Playing;
		pbiParams.uFrameOffset = in_pAction->LaunchFrameOffset;
        pbiParams.bIsFirst = true;
		pbiParams.bPlayDirectly = in_pAction->pAction->PlayDirectly();
		pbiParams.pGameObj = in_pAction->GameObj();
#ifndef AK_OPTIMIZED
		pbiParams.playTargetID = m_ulElementID;
#endif

		pbiParams.pTransitionParameters = &Tparameters;
        pbiParams.pContinuousParams = NULL;
        pbiParams.sequenceID = AK_INVALID_SEQUENCE_ID;

		eResult = static_cast<CAkParameterNode*>(pNode)->Play( pbiParams );

		pNode->Release();
	}
	else
	{
		CAkCntrHist HistArray;
		MONITOR_OBJECTNOTIF( in_pAction->UserParam.PlayingID(), in_pAction->GameObjID(), in_pAction->UserParam.CustomParam(), AkMonitorData::NotificationReason_PlayFailed, HistArray, m_ulElementID, false, 0 );
		MONITOR_ERROREX( AK::Monitor::ErrorCode_SelectedNodeNotAvailablePlay, in_pAction->UserParam.PlayingID(), in_pAction->GameObjID(), m_ulElementID, false );
		eResult = AK_IDNotFound;
	}
	return eResult;
}

CAkActionPlay* CAkActionPlay::Create(AkActionType in_eActionType, AkUniqueID in_ulID)
{
	CAkActionPlay*	pActionPlay = AkNew(AkMemID_Event,CAkActionPlay(in_eActionType, in_ulID));
	if( pActionPlay )
	{
		if( pActionPlay->Init() != AK_Success )
		{
			pActionPlay->Release();
			pActionPlay = NULL;
		}
	}

	return pActionPlay;
}

void CAkActionPlay::GetHistArray( AkCntrHistArray& out_rHistArray )
{
	//we don't have any so we give away a clean copy
	out_rHistArray.Init();
}

AKRESULT CAkActionPlay::SetActionParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	AkUInt8 ucFadeCurveType = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );

	m_eFadeCurve = (AkCurveInterpolation)ucFadeCurveType;

	SetBankID( READBANKDATA( AkBankID, io_rpData, io_rulDataSize ) );

	m_bWasLoadedFromBank = true;

	return AK_Success;
}
