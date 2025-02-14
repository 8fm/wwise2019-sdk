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
// AkActionActive.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkActionActive.h"
#include "AkBus.h"
#include "AkDynamicSequence.h"

CAkActionActive::CAkActionActive(AkActionType in_eActionType, AkUniqueID in_ulID) 
: CAkActionExcept(in_eActionType, in_ulID)
{
}

CAkActionActive::~CAkActionActive()
{
}

AKRESULT CAkActionActive::SetActionParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	AkUInt8 ucFadeCurveType = READBANKDATA(AkUInt8, io_rpData, io_rulDataSize );

	m_eFadeCurve = (AkCurveInterpolation)ucFadeCurveType;

	AKRESULT eResult = SetActionSpecificParams(io_rpData, io_rulDataSize);

	if( eResult == AK_Success )
		eResult = SetExceptParams( io_rpData, io_rulDataSize );

	return eResult;
}

AKRESULT CAkActionActive::Exec( ActionParamType in_eType, CAkRegisteredObj * in_pGameObj, AkPlayingID in_TargetPlayingID )
{
	CAkParameterNodeBase* pNode = GetAndRefTarget();
	if(pNode)
	{
		ActionParams l_Params;
		l_Params.bIsFromBus = false;
		l_Params.bIsMasterCall = false;
		l_Params.transParams.eFadeCurve = (AkCurveInterpolation)m_eFadeCurve;
		l_Params.eType = in_eType;
		l_Params.pGameObj = in_pGameObj;
		l_Params.playingID = in_TargetPlayingID;
		l_Params.transParams.TransitionTime = GetTransitionTime();

		SetActionActiveParams( l_Params );

		pNode->ExecuteAction( l_Params );

		pNode->Release();

		return AK_Success;
	}
	else
	{
		return AK_IDNotFound;
	}
}

void CAkActionActive::ExecOnDynamicSequences(ActionParams in_params)
{
	if (in_params.bApplyToDynamicSequence)
	{
		AKASSERT(g_pIndex);
		CAkIndexItem<CAkDynamicSequence*>& l_rIdx = g_pIndex->m_idxDynamicSequences;

		CAkDynamicSequence ** pSequences = NULL;
		int cSequences = 0;

		{
			// Dynamic Sequences need to be collected inside of the index lock because creation occurs from the game thread. Destruction occurs from
			// the audio thread, so we're safe to let go of the lock for the actual operation.
			AkAutoLock<CAkLock> IndexLock(l_rIdx.m_IndexLock);
			if (l_rIdx.m_mapIDToPtr.Length())
			{
				pSequences = (CAkDynamicSequence **)AkAlloca(l_rIdx.m_mapIDToPtr.Length()*sizeof(CAkDynamicSequence *));
				for (CAkIndexItem<CAkDynamicSequence*>::AkMapIDToPtr::Iterator iter = l_rIdx.m_mapIDToPtr.Begin(); iter != l_rIdx.m_mapIDToPtr.End(); ++iter)
				{
					pSequences[cSequences++] = static_cast<CAkDynamicSequence*>(*iter);
				}
			}
		}

		for (int i = 0; i < cSequences; ++i)
			pSequences[i]->AllExec(in_params.eType, in_params.pGameObj); // WG-33382 this needs to be done outside of the index lock!
	}
}

void CAkActionActive::AllExecExcept( ActionParamType in_eType, CAkRegisteredObj * in_pGameObj, AkPlayingID in_TargetPlayingID )
{
	ActionParamsExcept l_Params;
	l_Params.bIsFromBus = false;
	l_Params.bIsMasterCall = in_pGameObj ? false : true;
    l_Params.transParams.eFadeCurve = (AkCurveInterpolation)m_eFadeCurve;
	l_Params.eType = in_eType;
	l_Params.pGameObj = in_pGameObj;
	l_Params.playingID = in_TargetPlayingID;
	l_Params.pExeceptionList = &m_listElementException;
	l_Params.transParams.TransitionTime = GetTransitionTime();

	SetActionActiveParams( l_Params );

	ExecOnDynamicSequences(l_Params);

	CAkBus::ExecuteMasterBusActionExcept(l_Params);

}
