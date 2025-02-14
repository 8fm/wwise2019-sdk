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
// AkSIS.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkSIS.h"
#include "AkTransition.h"
#include "AkTransitionManager.h"
#include "AkParameterNode.h"
#include "AkMonitor.h"
#include "AkMutedMap.h"
#include "AkRegisteredObj.h"
#include "AkRTPCSubscriber.h"

CAkSIS::~CAkSIS()
{
	for ( AkSISValues::Iterator it = m_values.Begin(), itEnd = m_values.End(); it != itEnd; ++it )
	{
		if ( (*it).pTransition )
			g_pTransitionManager->RemoveTransitionUser( (*it).pTransition, this );
	}
}

AkSISValue * CAkSIS::GetSISValue( AkPropID in_ePropID, AkReal32 in_fDefault )
{
	AkSISValue * pSISValue = m_values.FindProp( in_ePropID );
	if (!pSISValue && m_pParamNode->GetSISNode().EnableParam(g_AkPropRTPCID[in_ePropID]) == AK_Success)
	{
		pSISValue = m_values.AddAkProp(in_ePropID);
		if (!pSISValue)
			return NULL;
	
		pSISValue->fValue = in_fDefault;
		pSISValue->pTransition = NULL;
	}

	return pSISValue;
}

void CAkSIS::TransUpdateValue( AkIntPtr in_eTarget, AkReal32 in_fValue, bool in_bIsTerminated )
{
	AkDeltaMonitorObjBrace brace(m_pParamNode->key);

	AkPropID eTargetProp = (AkPropID) in_eTarget;

	AkSISValue * pValue = m_values.FindProp( eTargetProp );
	AKASSERT( pValue );

	if ( eTargetProp != AkPropID_MuteRatio )
	{ 
		m_pParamNode->GetSISNode().PushParamUpdate_Scoped( g_AkPropRTPCID[ eTargetProp ], m_pGameObj, in_fValue, in_fValue - pValue->fValue );
	}
	else
	{
		if( in_bIsTerminated && in_fValue == AK_UNMUTED_RATIO )
		{
			MONITOR_PARAMCHANGED(
				AkMonitorData::NotificationReason_Unmuted, 
				m_pParamNode->ID(),
				m_pParamNode->IsBusCategory(),
				m_pGameObj ? m_pGameObj->ID() : AK_INVALID_GAME_OBJECT
				);
		}

		AkMutedMapItem item;
        item.m_bIsPersistent = false;
		item.m_bIsGlobal = (m_pGameObj == NULL);
		item.m_Identifier = m_pParamNode;
		item.m_eReason = AkDelta_Mute;
							
		// object wise ?
		if( m_pGameObj != NULL )
		{
			m_pParamNode->MuteNotification( in_fValue, m_pGameObj, item );
		}
		else
		{
			m_pParamNode->MuteNotification( in_fValue, item );
		}					
	}

	pValue->fValue = in_fValue;
	if ( in_bIsTerminated )
		pValue->pTransition = NULL;
}
