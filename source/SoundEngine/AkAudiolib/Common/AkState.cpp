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
// AkState.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkState.h"
#include "AkAudioLibIndex.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include "AkStateAware.h"
#include "AkStateMgr.h"

extern CAkStateMgr* g_pStateMgr;

CAkState::CAkState( AkUniqueID in_ulID )
:CAkIndexable(in_ulID)
,m_pParentToNotify(NULL)
{
}

CAkState::~CAkState()
{
}

CAkState* CAkState::Create( AkUniqueID in_ulID )
{
	CAkState* pState = AkNew(AkMemID_Structure, CAkState(in_ulID) );
	if( pState )
	{
		if( pState->Init() != AK_Success )
		{
			pState->Release();
			pState = NULL;
		}
	}
	return pState;
}

AKRESULT CAkState::SetInitialValues(AkUInt8* in_pData, AkUInt32 in_ulDataSize)
{
	AKRESULT eResult = AK_Success;

	//Read ID
	// We don't care about the ID, just skip it
	SKIPBANKDATA( AkUInt32, in_pData, in_ulDataSize );

	eResult = m_props.SetInitialParams( in_pData, in_ulDataSize );

	CHECKBANKDATASIZE( in_ulDataSize, eResult );

	return eResult;
}

void CAkState::AddToIndex()
{
	g_pIndex->m_idxCustomStates.SetIDToPtr( this );
}

void CAkState::RemoveFromIndex()
{
	g_pIndex->m_idxCustomStates.RemoveID( ID() );
}

AkUInt32 CAkState::AddRef() 
{ 
	AkAutoLock<CAkLock> IndexLock( g_pIndex->m_idxCustomStates.GetLock() ); 
	return ++m_lRef;
} 

AkUInt32 CAkState::Release() 
{
	AkAutoLock<CAkLock> IndexLock( g_pIndex->m_idxCustomStates.GetLock() ); 
	AkInt32 lRef = --m_lRef; 
	AKASSERT( lRef >= 0 ); 
	if ( !lRef ) 
	{ 
		RemoveFromIndex(); 
		AkDelete( AkMemID_Structure, this ); 
	} 
	return lRef; 
}

void CAkState::InitNotificationSystem( CAkStateAware * in_pNode )
{
	m_pParentToNotify = in_pNode;
}

void CAkState::TermNotificationSystem()
{
	m_pParentToNotify = NULL;
}

void CAkState::NotifyParent()
{
	if( m_pParentToNotify )
		m_pParentToNotify->NotifyStateParametersModified();
}

void CAkState::SetAkProp( AkStatePropertyId in_eProp, AkReal32 in_fValue, AkReal32 in_fDefault )
{
	AkReal32 fProp = m_props.GetAkProp( in_eProp, in_fDefault );
	if( fProp != in_fValue )
	{
		m_props.SetAkProp( in_eProp, in_fValue );
		NotifyParent();
	}
}
