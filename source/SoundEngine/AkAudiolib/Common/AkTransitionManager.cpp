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
// AkTransitionManager.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include <AK/Tools/Common/AkAutoLock.h>
#include "AkTransitionManager.h"
#include "AkMath.h"
#include "AudiolibDefs.h"
#include "AkTransition.h"
#include "AkInterpolation.h"
#include "AkAudioMgr.h"

//====================================================================================================
//====================================================================================================
void CAkTransitionManager::Term()
{
	// anyone got lost in here ?
	TermList( m_ActiveTransitionsList_Fade );
	TermList( m_ActiveTransitionsList_State );
}

void CAkTransitionManager::TermList( AkTransitionList& in_rTransitionList )
{
	if(!in_rTransitionList.IsEmpty())
	{
		for( AkTransitionList::Iterator iter = in_rTransitionList.Begin(); iter != in_rTransitionList.End(); ++iter )
		{
			CAkTransition* pTransition = *iter;
			// force transition to end to ensure clients are detached (WG-19176)
			pTransition->ComputeTransition( pTransition->m_uStartTimeInBufferTick + pTransition->m_uDurationInBufferTick );
			// get rid of the transition
			pTransition->Term();
			AkDelete( AkMemID_Object, pTransition );
		}
	}
	in_rTransitionList.Term();
}

//====================================================================================================
// add a new one to the list of those to be processed
//====================================================================================================
CAkTransition* CAkTransitionManager::AddTransitionToList(const TransitionParameters& in_Params,bool in_bStart,AkTransitionCategory in_eTransitionCategory)
{	
	AkTransitionList* pTransitionList = (in_eTransitionCategory == TC_State) ? &m_ActiveTransitionsList_State : &m_ActiveTransitionsList_Fade;

	CAkTransition* pThisTransition = NULL;

	// get a new one
	pThisTransition = AkNew(AkMemID_Object, CAkTransition );

	if(pThisTransition != NULL)
	{
		// fill it in and check that it was ok
		if(pThisTransition->InitParameters( in_Params, g_pAudioMgr->GetBufferTick() ) != AK_Fail)
		{
			// add it to the active list
			if( pTransitionList->AddLast( pThisTransition ) )
			{
				if(in_bStart)
				{
					// start it
					pThisTransition->m_eState = CAkTransition::Running;
				}
			}
			else
			{
				pThisTransition->Term();
				AkDelete(AkMemID_Object, pThisTransition );
				pThisTransition = NULL;
			}
		}
		// couldn't fill it in for some reason, get rid of it
		else
		{
			pThisTransition->Term();
			AkDelete(AkMemID_Object, pThisTransition );
			pThisTransition = NULL;
		}
	}

	if( pThisTransition == NULL )
	{
		AkDeltaMonitorUpdateBrace updateBrace(in_Params.eChangeReason);

		AKASSERT( in_Params.pUser );
		// We ended up in a situation where a memory allocation failure most likely occured
		// That caused an impossibility to Create a requested transition.
		// Fallback on faking the end of the transition to avoid the command being lost.(WG-23935)
		in_Params.pUser->TransUpdateValue( 
			in_Params.eTarget, 
			in_Params.fTargetValue, 
			true //Completed/bDone
			);
	}
	return pThisTransition;
}

void CAkTransitionManager::PauseAllStateTransitions()
{
	PauseAllTransitionInList( m_ActiveTransitionsList_State );
}

void CAkTransitionManager::ResumeAllStateTransitions()
{
	ResumeAllTransitionInList( m_ActiveTransitionsList_State );
}

//====================================================================================================
//====================================================================================================
void CAkTransitionManager::PauseAllTransitionInList( AkTransitionList& in_rTransitionList )
{
	if(!in_rTransitionList.IsEmpty())
	{
		for( AkTransitionList::Iterator iter = in_rTransitionList.Begin(); iter != in_rTransitionList.End(); ++iter )
		{
			CAkTransition* pTransition = *iter;
			pTransition->Pause();
		}
	}
}

//====================================================================================================
//====================================================================================================
void CAkTransitionManager::ResumeAllTransitionInList( AkTransitionList& in_rTransitionList )
{
	if(!in_rTransitionList.IsEmpty())
	{
		for( AkTransitionList::Iterator iter = in_rTransitionList.Begin(); iter != in_rTransitionList.End(); ++iter )
		{
			CAkTransition* pTransition = *iter;
			pTransition->Resume();
		}
	}
}

//====================================================================================================
// adds a given user to a given multi user transition
//====================================================================================================
AKRESULT CAkTransitionManager::AddTransitionUser(CAkTransition* in_pTransition,ITransitionable* in_pUser)
{
	AKASSERT( m_ActiveTransitionsList_Fade.Exists(in_pTransition) || m_ActiveTransitionsList_State.Exists(in_pTransition) );

	AKRESULT eResult = AK_Fail;

	if(!in_pTransition->m_UsersList.Exists(in_pUser))
	{
		// can we add a new one ?
		if(in_pTransition->m_UsersList.AddLast(in_pUser) )
		{
			eResult = AK_Success;
		}
	}

	return eResult;
}

bool CAkTransitionManager::IsTerminated( CAkTransition* in_pTransition )
{
	AKASSERT( m_ActiveTransitionsList_Fade.Exists(in_pTransition) || m_ActiveTransitionsList_State.Exists(in_pTransition) );

	return ( in_pTransition->m_eState == CAkTransition::Done );
}

//====================================================================================================
// removes a given user from a given multi user transition
//====================================================================================================
AKRESULT CAkTransitionManager::RemoveTransitionUser(CAkTransition* in_pTransition,ITransitionable* in_pUser)
{
	AKASSERT( m_ActiveTransitionsList_Fade.Exists(in_pTransition) || m_ActiveTransitionsList_State.Exists(in_pTransition) );

	// assume we don't have this user
	AKRESULT eResult = AK_Fail;

	// look for our user in the list
	CAkTransition::AkTransitionUsersList::Iterator iter = in_pTransition->m_UsersList.FindEx( in_pUser );
	if ( iter != in_pTransition->m_UsersList.End() )
	{
		eResult = AK_Success;

		// remove it
		in_pTransition->m_UsersList.EraseSwap( iter );
		if( in_pTransition->m_UsersList.IsEmpty() )
		{
			RemoveTransitionFromList( in_pTransition );
		}
	}

	return eResult;
}
//====================================================================================================
// flags a transition for removal
//====================================================================================================
void CAkTransitionManager::RemoveTransitionFromList(CAkTransition* in_pTransition)
{
	AKASSERT( m_ActiveTransitionsList_Fade.Exists(in_pTransition) || m_ActiveTransitionsList_State.Exists(in_pTransition) );

	// then remove it
	in_pTransition->m_eState = CAkTransition::ToRemove;
}
//====================================================================================================
// flags a transition for pause
//====================================================================================================
void CAkTransitionManager::Pause(CAkTransition* in_pTransition)
{
	AKASSERT( m_ActiveTransitionsList_Fade.Exists(in_pTransition) || m_ActiveTransitionsList_State.Exists(in_pTransition) );

	AKASSERT(in_pTransition != NULL);

	in_pTransition->Pause();
}
//====================================================================================================
// flags a transition for un-pause
//====================================================================================================
void CAkTransitionManager::Resume(CAkTransition* in_pTransition)
{
	AKASSERT( m_ActiveTransitionsList_Fade.Exists(in_pTransition) || m_ActiveTransitionsList_State.Exists(in_pTransition) );

	AKASSERT(in_pTransition != NULL);

	in_pTransition->Resume();
}
//====================================================================================================
// changes the target and duration values
//====================================================================================================
void CAkTransitionManager::ChangeParameter(CAkTransition*		in_pTransition,
											AkIntPtr			in_eTarget,
											AkReal32			in_NewTarget,
											AkTimeMs			in_NewDuration,
											AkCurveInterpolation in_eCurveType,
											AkValueMeaning		in_eValueMeaning)
{
	AKASSERT( m_ActiveTransitionsList_Fade.Exists(in_pTransition) || m_ActiveTransitionsList_State.Exists(in_pTransition) );

	AKASSERT(in_pTransition != NULL);
	
	AkIntPtr l_oldTarget = in_pTransition->m_eTarget;

	// set the new target type
	in_pTransition->m_eTarget = in_eTarget;

	in_pTransition->UpdateFilteringCoeff( in_NewDuration );

//----------------------------------------------------------------------------------------------------
// process dB values
//----------------------------------------------------------------------------------------------------
	if(in_pTransition->m_bdBs)
	{
		in_pTransition->m_fStartValue = AkMath::dBToLin( in_pTransition->m_fCurrentValue );

		// is our new ending point an offset ?
		if(in_eValueMeaning == AkValueMeaning_Offset)
		{
			AkReal32 linTarget = AkMath::dBToLin( in_NewTarget );
			in_pTransition->m_fTargetValue *= linTarget;
			in_pTransition->m_fFinalValue = AkMath::FastLinTodB( in_pTransition->m_fTargetValue );
		}
		// nope it's not, fill in the union with whatever we got
		else
		{
			in_pTransition->m_fFinalValue = in_NewTarget;
			AkReal32 linTarget = AkMath::dBToLin( in_NewTarget );
			in_pTransition->m_fTargetValue = linTarget;
		}
	}
//----------------------------------------------------------------------------------------------------
// process linear values
//----------------------------------------------------------------------------------------------------
	else
	{
		in_pTransition->m_fStartValue = in_pTransition->m_fCurrentValue;

		// is our new ending point an offset ?
		if(in_eValueMeaning == AkValueMeaning_Offset)
		{
			in_pTransition->m_fTargetValue += in_NewTarget;
			in_pTransition->m_fFinalValue = in_pTransition->m_fTargetValue;
		}
		// nope it's not, fill in the union with whatever we got
		else
		{
			in_pTransition->m_fTargetValue = in_NewTarget;
			in_pTransition->m_fFinalValue = in_pTransition->m_fTargetValue;
		}
	}

	// WG-13494: want to use the reciprocal curve when fading out. Except for S-Curves.
	bool bFadeIn = ( in_pTransition->m_fStartValue < in_pTransition->m_fTargetValue );
	in_pTransition->m_eFadeCurve = ( bFadeIn || ( in_eCurveType == AkCurveInterpolation_InvSCurve ) || ( in_eCurveType == AkCurveInterpolation_SCurve ) ) ?
		in_eCurveType : (AkCurveInterpolation) ( AkCurveInterpolation_LastFadeCurve - in_eCurveType );

	// Update time settings.
	AkUInt32 uLastTickUpdated = g_pAudioMgr->GetBufferTick();
	AkUInt32 uNewDurationTicks = CAkTransition::Convert( in_NewDuration );

	// Same target in a Play/Stop/Pause/Resume transition ? 
	if ( ( l_oldTarget == in_pTransition->m_eTarget )
		&& ( in_eTarget & ( TransTarget_Play | TransTarget_Stop | TransTarget_Pause | TransTarget_Resume ) ) )
	{
		AkUInt32 uElapsedTicks = uLastTickUpdated - in_pTransition->m_uStartTimeInBufferTick;
		AkUInt32 uRemainingDurationTicks = in_pTransition->m_uDurationInBufferTick - uElapsedTicks;
		in_pTransition->m_uDurationInBufferTick = AkMin( uNewDurationTicks, uRemainingDurationTicks );
	}
	else
	{
		in_pTransition->m_uDurationInBufferTick = uNewDurationTicks;
	}

	// this is our new start time
	in_pTransition->m_uStartTimeInBufferTick = uLastTickUpdated;
	in_pTransition->m_uLastBufferTickUpdated = uLastTickUpdated;
}
//====================================================================================================
// does what it takes to get things moving
//====================================================================================================
void CAkTransitionManager::ProcessTransitionsList( AkUInt32 in_CurrentBufferTick )
{
	ProcessTransitionsList( in_CurrentBufferTick, m_ActiveTransitionsList_Fade );
	ProcessTransitionsList( in_CurrentBufferTick, m_ActiveTransitionsList_State );
}
//====================================================================================================
// Helper, called for each transition list
//====================================================================================================
void CAkTransitionManager::ProcessTransitionsList( AkUInt32 in_CurrentBufferTick, AkTransitionList& in_rTransitionList )
{
	CAkTransition*	pThisTransition;
//----------------------------------------------------------------------------------------------------
// process the active transitions
//----------------------------------------------------------------------------------------------------

	unsigned int uTransitionIndex = 0; // WG-31524 - Crash in CAkTransitionManager::ProcessTransitionsList
									   // we cannot use an array iterator here, some codepath are possibly adding transitions
									   // to this list and the array may be reallocated on Grow.
	while ( uTransitionIndex < in_rTransitionList.Length() )
	{
		pThisTransition = in_rTransitionList[uTransitionIndex];

//----------------------------------------------------------------------------------------------------
// should be removed ? if yes then it should not be processed
//----------------------------------------------------------------------------------------------------
		if(pThisTransition->m_eState == CAkTransition::ToRemove)
		{
			// get rid of the transition
			pThisTransition->Term();
			AkDelete(AkMemID_Object, pThisTransition );

			// remove the list entry
			in_rTransitionList.Erase(uTransitionIndex);
		}
		else
		{
			switch(pThisTransition->m_eState)
			{
			case CAkTransition::ToPause:
				// remember when this happened
				pThisTransition->m_uLastBufferTickUpdated = in_CurrentBufferTick;
				pThisTransition->m_eState = CAkTransition::Paused;
				break;
			case CAkTransition::ToResume:
				// erase the time spent in pause mode
				pThisTransition->m_uStartTimeInBufferTick += in_CurrentBufferTick - pThisTransition->m_uLastBufferTickUpdated;

				pThisTransition->m_eState = CAkTransition::Running;
				break;
			default:
				break;
			}
//----------------------------------------------------------------------------------------------------
// now let's update those who need to
//----------------------------------------------------------------------------------------------------
			if(pThisTransition->m_eState == CAkTransition::Running)
			{
				// step it and let me know what to do
				if(pThisTransition->ComputeTransition( in_CurrentBufferTick )) // TRUE == transition completed 
				{
					// this one is done
					pThisTransition->Term();
					in_rTransitionList.Erase(uTransitionIndex);
					AkDelete(AkMemID_Object, pThisTransition );
				}
				else
				{
					++uTransitionIndex;
				}
			}
			else
			{
				++uTransitionIndex;
			}
		}
	}
}

// gets the current and maximum number of transitions
void CAkTransitionManager::GetTransitionsUsage( 
		AkUInt16& out_ulNumFadeTransitionsUsed,
		AkUInt16& out_ulNumStateTransitionsUsed
		)
{
	out_ulNumFadeTransitionsUsed = (AkUInt16)m_ActiveTransitionsList_Fade.Length();
	out_ulNumStateTransitionsUsed = (AkUInt16)m_ActiveTransitionsList_State.Length();
}
