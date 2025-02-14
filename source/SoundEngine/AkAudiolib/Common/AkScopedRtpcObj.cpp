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
// AkScopedRtpcObj.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkScopedRtpcObj.h"
#include "AkModulatorMgr.h"
#include "AkPlayingMgr.h"
#include "AkRegisteredObj.h"
#include "AkPBI.h"

AKRESULT CAkScopedRtpcObj::AddedNewRtpcValue( AkRtpcID in_rtpcId, const AkRTPCKey& in_ValKey )
{
	bool result = true;
	if ( in_ValKey.PBI() != NULL )
		result = in_ValKey.PBI()->AddedNewRtpcValue(in_rtpcId);
	else if (in_ValKey.PlayingID() != AK_INVALID_PLAYING_ID )
		result = g_pPlayingMgr->AddedNewRtpcValue( in_ValKey.PlayingID(), in_rtpcId );
	else if ( in_ValKey.GameObj() != NULL )
		result = in_ValKey.GameObj()->AddedNewRtpcValue(in_rtpcId);
	return result ? AK_Success : AK_Fail;
}

AKRESULT CAkScopedRtpcObj::AddedNewModulatorCtx( AkRtpcID in_rtpcId, const AkRTPCKey& in_ValKey )
{
	bool result = true;
	if ( in_ValKey.PBI() != NULL )
		result = in_ValKey.PBI()->AddedNewModulatorCtx(in_rtpcId);
	else if (in_ValKey.PlayingID() != AK_INVALID_PLAYING_ID )
		result = g_pPlayingMgr->AddedNewModulatorCtx( in_ValKey.PlayingID(), in_rtpcId );
	else if ( in_ValKey.GameObj() != NULL )
		result = in_ValKey.GameObj()->AddedNewModulatorCtx(in_rtpcId);
	return result ? AK_Success : AK_Fail;
}

#ifdef RTPC_OBJ_SCOPE_SAFETY
void CAkScopedRtpcObj::CheckRtpcKey( const AkRTPCKey& in_rtpcKey )
{
	if ( in_rtpcKey.PBI() != NULL )
	{
		AKASSERT(in_rtpcKey.PBI()->m_Safety == 0xAAAAAAAA);
	}
	
	if (in_rtpcKey.PlayingID() != AK_INVALID_PLAYING_ID )
	{
		AKASSERT( g_pPlayingMgr->IsActive(in_rtpcKey.PlayingID()) );
	}
	
	if ( in_rtpcKey.GameObj() != NULL )
	{
		AKASSERT(in_rtpcKey.GameObj()->m_Safety == 0xAAAAAAAA);
	}
}
#endif

CAkScopedRtpcObj::CAkScopedRtpcObj()
#ifdef RTPC_OBJ_SCOPE_SAFETY
: m_Safety(0xAAAAAAAA)
#endif
{
}

CAkScopedRtpcObj::~CAkScopedRtpcObj()
{
	AKASSERT(m_Modulators.IsEmpty());
	AKASSERT(m_RtpcVals.IsEmpty());

#ifdef RTPC_OBJ_SCOPE_SAFETY
	AKASSERT(m_Safety == 0xBBBBBBBB);
	m_Safety = 0xDDDDDDDD;
#endif
}

void CAkScopedRtpcObj::Term(const AkRTPCKey& in_rtpcKey)
{
	for ( AkRtpcIDSet::Iterator it = m_Modulators.Begin(); it != m_Modulators.End(); ++it )
	{
		g_pModulatorMgr->RemovedScopedRtpcObj( *it, in_rtpcKey );
	}
	m_Modulators.Term();

	for ( AkRtpcIDSet::Iterator it = m_RtpcVals.Begin(); it != m_RtpcVals.End(); ++it )
	{
		g_pRTPCMgr->RemovedScopedRtpcObj( *it, in_rtpcKey );
	}
	m_RtpcVals.Term();

#ifdef RTPC_OBJ_SCOPE_SAFETY
	m_Safety = 0xBBBBBBBB;
#endif
}
	
