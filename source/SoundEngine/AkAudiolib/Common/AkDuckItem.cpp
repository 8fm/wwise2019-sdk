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
// AkDuckItem.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkDuckItem.h"
#include "AkTransition.h"
#include "AkTransitionManager.h"
#include "AkBus.h"
#include "AkMonitor.h"

AKRESULT CAkDuckItem::Term()
{
	AKASSERT(g_pTransitionManager);
	if(m_pvVolumeTransition)	{
		
		g_pTransitionManager->RemoveTransitionUser(m_pvVolumeTransition, this);
		m_pvVolumeTransition = NULL;
	}
	return AK_Success;
}


AKRESULT CAkDuckItem::Init(CAkBus* in_pBusNode)
{
	AKASSERT(in_pBusNode);
	m_pvVolumeTransition = NULL;
	m_pBusNode = in_pBusNode;
	m_EffectiveVolumeOffset = 0;
	m_clearOnTransitionTermination = false;
	return AK_Success;
}

void CAkDuckItem::TransUpdateValue(AkIntPtr in_target, AkReal32 in_fValue, bool in_bIsTerminated)
{
	AkPropID eTargetProp = (AkPropID) in_target;

	AkReal32 fPreviousVolume = m_pBusNode->GetDuckedVolume( eTargetProp );

	// set the new value
	m_EffectiveVolumeOffset = in_fValue;

	AkReal32 fNextVolume = m_pBusNode->GetDuckedVolume( eTargetProp );

	AkReal32 fNotified = fNextVolume - fPreviousVolume;	
	
	if(in_bIsTerminated)
	{
		if (m_clearOnTransitionTermination)
		{
			m_EffectiveVolumeOffset = 0.0f; // Otherwise it'll always be providing a small offset
		}
		m_pBusNode->CheckDuck();
		m_pvVolumeTransition = NULL;
		m_clearOnTransitionTermination = false;				
	}

	if( fNotified != 0.0f )
	{
		AkDeltaMonitorObjBrace brace(m_pBusNode->key);
		m_pBusNode->GetDuckerNode().PushParamUpdate_All( g_AkPropRTPCID[in_target], fNextVolume, fNotified );		
	}
}
