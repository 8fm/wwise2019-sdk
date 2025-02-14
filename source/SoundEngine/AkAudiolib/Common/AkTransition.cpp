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
// AkTransition.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkTransition.h"
#include "AudiolibDefs.h"
#include "AkInterpolation.h"
#include "AkMath.h"
#include "AkTransitionManager.h"
#include "AkDeltaMonitor.h"

//====================================================================================================
//====================================================================================================
CAkTransition::CAkTransition()
{
	m_eTarget = TransTarget_Undefined;
	m_bdBs = false;
	m_eState = Idle;
	m_fTimeRatio = 0.0f;
	m_fReleaseCoef = 0.f;
	m_bIsFiltering = false;
	m_eChangeReason = AkDelta_None;
}
//====================================================================================================
//====================================================================================================
CAkTransition::~CAkTransition()
{
}
//====================================================================================================
//====================================================================================================
void CAkTransition::Term()
{
	// get rid of our user's lists
	m_UsersList.Term();
}

void CAkTransition::UpdateFilteringCoeff( AkTimeMs in_TransitionDuration )
{
	if( m_bIsFiltering && in_TransitionDuration )
	{
		// 0.2f stands for a target of 99.5% at the end of the transition time, will then stick to target value.
		m_fReleaseCoef = expf( -AK_NUM_VOICE_REFILL_FRAMES / ( DEFAULT_NATIVE_FREQUENCY * ( 0.2f *in_TransitionDuration/1000.f) ) );
	}
}

bool CAkTransition::IsFadingOut()
{ 
	return m_eTarget == TransTarget_Pause || m_eTarget == TransTarget_Stop;
}
//====================================================================================================
// steps transition returns TRUE when the transition has completed
//====================================================================================================
bool CAkTransition::ComputeTransition( AkUInt32 in_CurrentBufferTick )
{
	AkReal32	fResult;		// will get sent to the user to update the the target parameter
	bool		bDone = false;	// tells the manager to drop completed transitions

	if ( in_CurrentBufferTick >= m_uStartTimeInBufferTick + m_uDurationInBufferTick )
	{
		bDone = true;
		fResult = m_fFinalValue;
	}
	else
	{
		if( m_bIsFiltering && m_fReleaseCoef != 0 )
		{
			fResult = m_fReleaseCoef * (m_fCurrentValue-m_fFinalValue) + m_fFinalValue;
		}
		else
		{
			if ( in_CurrentBufferTick <= m_uStartTimeInBufferTick )
			{
				m_fTimeRatio = 0.0f;
			}
			else
			{
				AkUInt32 uElapsedTick = in_CurrentBufferTick - m_uStartTimeInBufferTick;
				m_fTimeRatio = (AkReal32) uElapsedTick / (AkReal32) m_uDurationInBufferTick;
			}

			// compute new value
			fResult = AkInterpolation::InterpolateNoCheck(
				m_fTimeRatio,
				m_fStartValue,
				m_fTargetValue,
				m_eFadeCurve );
		}

		if ( m_bdBs ) 
			fResult = AkMath::FastLinTodB( fResult );
	}

	
	m_fCurrentValue = fResult;

	AkDeltaMonitor::OpenUpdateBrace(m_eChangeReason);
	// send the new stuff to the users if any
	for (AkTransitionUsersList::Iterator iter = m_UsersList.Begin(); iter != m_UsersList.End(); ++iter)
	{
		AKASSERT(*iter);
		// call this one
		(*iter)->TransUpdateValue(m_eTarget, fResult, bDone);
	}
	AkDeltaMonitor::CloseUpdateBrace();	

	return bDone;
}
//====================================================================================================
// fill in a transition
//====================================================================================================
AKRESULT CAkTransition::InitParameters( const TransitionParameters& in_Params, AkUInt32 in_CurrentBufferTick )
{
	AKRESULT l_eResult = AK_Success;

	m_eTarget = in_Params.eTarget;

	m_bdBs = in_Params.bdBs;
	m_bIsFiltering = in_Params.bUseFiltering;

	// must keep supplied target value as final value!!!
	m_fFinalValue = in_Params.fTargetValue;
	m_fCurrentValue = in_Params.fStartValue;

    AkReal32 startValue = in_Params.fStartValue;
    AkReal32 targetValue = in_Params.fTargetValue;

    // convert dBs to linear first, this saves two m_tableDBtoReal->Get() per transition in the process loop
	if(in_Params.bdBs)
	{
		startValue = AkMath::dBToLin(startValue);
		targetValue = AkMath::dBToLin(targetValue);
	}

	m_fStartValue = startValue;
	m_fTargetValue = targetValue;

	// WG-13494: want to use the reciprocal curve when fading out. Except for S-Curves.
	m_eFadeCurve = ( !in_Params.bUseReciprocalCurve || ( startValue < targetValue ) || ( in_Params.eFadeCurve == AkCurveInterpolation_InvSCurve ) || ( in_Params.eFadeCurve == AkCurveInterpolation_SCurve ) ) ?
		in_Params.eFadeCurve : (AkCurveInterpolation) ( AkCurveInterpolation_LastFadeCurve - in_Params.eFadeCurve );

	m_uStartTimeInBufferTick = in_CurrentBufferTick;
	m_uDurationInBufferTick = CAkTransition::Convert( in_Params.lDuration );
	m_fTimeRatio = 0.0f;
	m_eChangeReason = in_Params.eChangeReason;

	UpdateFilteringCoeff( in_Params.lDuration );

	// as we may be called to start a sticky transition the user might already be in here
	if(!m_UsersList.Exists(in_Params.pUser))
	{
		// add the user to the list
		if( !m_UsersList.AddLast( in_Params.pUser ) )
		{
			return AK_Fail;
		}
	}
	return l_eResult;
}
