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
// AkActionSetGameParameter.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkActionSetGameParameter.h"
#include "AkRegisteredObj.h"
#include "AkRTPCMgr.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include "AkAudioMgr.h"

CAkActionSetGameParameter::CAkActionSetGameParameter(AkActionType in_eActionType, AkUniqueID in_ulID) 
: CAkActionSetValue( in_eActionType, in_ulID )
, m_eValueMeaning( AkValueMeaning_Offset )
, m_bBypassInternalTransition( false )
{
}

CAkActionSetGameParameter::~CAkActionSetGameParameter()
{
}

CAkActionSetGameParameter* CAkActionSetGameParameter::Create( AkActionType in_eActionType, AkUniqueID in_ulID )
{
	CAkActionSetGameParameter* pActionSetRTPC = AkNew(AkMemID_Event, CAkActionSetGameParameter( in_eActionType, in_ulID ) );
	if( pActionSetRTPC )
	{
		if( pActionSetRTPC->Init() != AK_Success )
		{
			pActionSetRTPC->Release();
			pActionSetRTPC = NULL;
		}
	}

	return pActionSetRTPC;
}

void CAkActionSetGameParameter::SetValue(
	const AkReal32 in_Value, //Target value of the action
	const AkValueMeaning in_eValueMeaning, //Meaning of the target value
	const AkReal32 in_RangeMin,
	const AkReal32 in_RangeMax
	)
{
	RandomizerModifier::SetModValue( m_TargetValue, in_Value, in_RangeMin, in_RangeMax);
	m_eValueMeaning = in_eValueMeaning;
}

void CAkActionSetGameParameter::BypassGameParameterInternalTransition( bool in_bBypassGameParameterInternalTransition )
{
	m_bBypassInternalTransition = in_bBypassGameParameterInternalTransition;
}

void CAkActionSetGameParameter::ExecSetValue(CAkParameterNodeBase* )
{
	TransParams transParams( GetTransitionTime(), CurveType(), m_bBypassInternalTransition );
	g_pRTPCMgr->SetRTPCInternal( ElementID(), RandomizerModifier::GetModValue( m_TargetValue ), AkRTPCKey(), transParams, m_eValueMeaning );
}
void CAkActionSetGameParameter::ExecSetValue(CAkParameterNodeBase*, CAkRegisteredObj * in_pGameObj)
{
	TransParams transParams( GetTransitionTime(), CurveType(), m_bBypassInternalTransition );
	g_pRTPCMgr->SetRTPCInternal( ElementID(), RandomizerModifier::GetModValue( m_TargetValue ), in_pGameObj, transParams, m_eValueMeaning );
}

void CAkActionSetGameParameter::ExecResetValue(CAkParameterNodeBase*)
{
	TransParams transParams( GetTransitionTime(), CurveType(), m_bBypassInternalTransition );
	g_pRTPCMgr->ResetRTPCValue(ElementID(), AkRTPCKey(), transParams);
}
void CAkActionSetGameParameter::ExecResetValue(CAkParameterNodeBase*, CAkRegisteredObj * in_pGameObj)
{
	TransParams transParams( GetTransitionTime(), CurveType(), m_bBypassInternalTransition );
	g_pRTPCMgr->ResetRTPCValue(ElementID(), in_pGameObj, transParams);
}

void CAkActionSetGameParameter::ExecResetValueExcept(CAkParameterNodeBase*)
{
	AKASSERT( !"Not implemented" );
}

void CAkActionSetGameParameter::ExecResetValueExcept(CAkParameterNodeBase*, CAkRegisteredObj * )
{
	AKASSERT( !"Not implemented" );
}

AKRESULT CAkActionSetGameParameter::SetActionSpecificParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	AkUInt8 bBypassTransition = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize);
	BypassGameParameterInternalTransition( bBypassTransition != 0 );

	AkUInt8 TargetValueMeaning = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );

	AkReal32 ValBase = READBANKDATA( AkReal32, io_rpData, io_rulDataSize );

	AkReal32 ValMin = READBANKDATA( AkReal32, io_rpData, io_rulDataSize );

	AkReal32 ValMax = READBANKDATA( AkReal32, io_rpData, io_rulDataSize );

	RandomizerModifier::SetModValue( m_TargetValue, ValBase, ValMin, ValMax );
	m_eValueMeaning = AkValueMeaning( TargetValueMeaning );

	return AK_Success;
}
