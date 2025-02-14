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
// AkActionEvent.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkActionEvent.h"
#include "AkAudioMgr.h"
#include "AkEvent.h"

CAkActionEvent::CAkActionEvent(AkActionType in_eActionType, AkUniqueID in_ulID)
: CAkAction(in_eActionType, in_ulID)
, m_ulTargetEventID( 0 )
{
}

CAkActionEvent::~CAkActionEvent()
{
}

AKRESULT CAkActionEvent::Execute( AkPendingAction * in_pAction )
{
	AKRESULT eResult = AK_Success;
	AKASSERT(g_pIndex);

	CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( m_ulTargetEventID );
	
	if( pEvent )
	{
		CAkRegisteredObj * pGameObj = in_pAction->GameObj();

		CAkEvent::AkActionList::Iterator iter = pEvent->m_actions.Begin();
		while( iter != pEvent->m_actions.End() )
		{
			CAkAction* pAction = *iter;
			AkActionType aType = pAction->ActionType();
			if( aType == AkActionType_Play )
			{
				CAkParameterNodeBase* pNode = pAction->GetAndRefTarget();
				if ( pNode )
				{
					if(m_eActionType == AkActionType_StopEvent)
					{
						g_pAudioMgr->StopPendingAction( pNode, pGameObj, AK_INVALID_PLAYING_ID );
						pNode->Stop( pGameObj );
					}
					else if(m_eActionType == AkActionType_PauseEvent)
					{
						g_pAudioMgr->PausePendingAction( pNode, pGameObj, true, AK_INVALID_PLAYING_ID );
						pNode->Pause( pGameObj );
					}
					else
					{
						g_pAudioMgr->ResumePausedPendingAction( pNode, pGameObj, false, AK_INVALID_PLAYING_ID );
						pNode->Resume( pGameObj );
					}
					pNode->Release();
				}
			}
			else
			{
				if(m_eActionType == AkActionType_StopEvent)
				{
					g_pAudioMgr->StopAction( pAction->ID() );
				}
				else if(m_eActionType == AkActionType_PauseEvent)
				{
					g_pAudioMgr->PauseAction( pAction->ID() );
				}
				else
				{
					g_pAudioMgr->ResumeAction( pAction->ID() );
				}
			}
			++iter;
		}
		pEvent->Release();
	}
	return eResult;
}

void CAkActionEvent::SetElementID( WwiseObjectIDext in_Id )
{
	m_ulTargetEventID = in_Id.id;
}

CAkActionEvent* CAkActionEvent::Create(AkActionType in_eActionType, AkUniqueID in_ulID)
{
	CAkActionEvent*	pActionEvent = AkNew(AkMemID_Event,CAkActionEvent(in_eActionType, in_ulID));
	if( pActionEvent )
	{
		if( pActionEvent->Init() != AK_Success )
		{
			pActionEvent->Release();
			pActionEvent = NULL;
		}
	}

	return pActionEvent;
}
