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
// AkTransition.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _TRANSITION_H_
#define _TRANSITION_H_

#include "ITransitionable.h"
#include <AK/Tools/Common/AkArray.h>
#include "AudiolibDefs.h"
#include "AkSettings.h"
#include "AkPrivateTypes.h"

enum AkTransitionRampingType
{
	AkTransitionRampingType_None = 0,
	AkTransitionRampingType_SlewRate = 1,
	AkTransitionRampingType_FilteringOverTime = 2
};
//----------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------
class CAkTransition
{
// the one that will get things moving
	friend	class CAkTransitionManager;

public:
	CAkTransition();
	~CAkTransition();

	// Getters.
	inline AkReal32 GetTargetValue() { return m_fTargetValue; }

	// cleans up before leaving
	void Term();

    // applies a time offset to transition (changes transition's current time)
    // non-reversible, apply after TransitionMgr's creation.
    void Offset(
        AkInt32 in_iOffset  // time offset (in audio frames -buffer ticks-)
        ) 
    { 
		if ( in_iOffset >= 0 
			||  m_uStartTimeInBufferTick > (AkUInt32) -in_iOffset )
		{
	        m_uStartTimeInBufferTick += in_iOffset; 
		}
		else 
		{
			m_uStartTimeInBufferTick = 0;
		}
    }

	// computes next value according to current time
	bool	ComputeTransition( AkUInt32 in_CurrentBufferTick );

	static AkForceInline AkUInt32 Convert( AkTimeMs in_ValueInTime )
	{
		/*
		AkUInt32 l_ValueInNumBuffer = in_ValueInTime/AK_MS_PER_BUFFER_TICK;
		l_ValueInNumBuffer += in_ValueInTime%AK_MS_PER_BUFFER_TICK ? 1 : 0;

		return l_ValueInNumBuffer;
		*/

		// Approximation is nearly as accurate and way faster.
		return ( in_ValueInTime + AK_MS_PER_BUFFER_TICK - 1 ) / AK_MS_PER_BUFFER_TICK;
	}

	void UpdateFilteringCoeff( AkTimeMs in_TransitionDuration );

	bool IsFadingOut();

private:

	void Pause()
	{
		// is it pause-able ?
		if(m_eState == CAkTransition::Running)
		{
			// then pause it
			m_eState = CAkTransition::ToPause;
		}
		else if (m_eState == CAkTransition::ToResume)
		{
			// we were already RESUMED within this frame. We can revert it.
			m_eState = CAkTransition::Paused;
		}
	}

	void Resume()
	{
		// is it resume-able
		if(m_eState == CAkTransition::Paused)
		{
			// then resume it
			m_eState = CAkTransition::ToResume;
		}
		else if (m_eState == CAkTransition::ToPause)
		{
			// we were already PAUSED within this frame. We can revert it.
			m_eState = CAkTransition::Running;
		}
	}

	// fill it in
	AKRESULT InitParameters( const class TransitionParameters& in_Params, AkUInt32 in_CurrentBufferTick );

	// which one should be used
	AkIntPtr	m_eTarget;

	// Initial value.
	// Will be useful to calculate the interpolation of non-linear curves.{AkUInt32, AkReal32, etc}
	AkReal32	m_fStartValue;

	// Target Volume of the fade {AkUInt32, AkReal32, etc}
	AkReal32	m_fTargetValue;

	// Current value, used for parameter changing
	AkReal32	m_fCurrentValue;

	// Value supplied when transition is done; must keep supplied TargetValue to not introduce DbToLin errors
	// IS NOT THE SAME AS m_fTargetValue (in the case of m_bdBs == true )
	AkReal32	m_fFinalValue;

	// Start time of the fade in Buffer ticks
	AkUInt32	m_uStartTimeInBufferTick;

	// Time it will take in Buffer ticks
	AkUInt32	m_uDurationInBufferTick;

	// how far it is in the duration
	AkReal32	m_fTimeRatio;

	// Last time it was processed, used for paused transitions
	AkUInt32	m_uLastBufferTickUpdated;

	// our transition's possible states
	enum State
	{
		Idle		= 0,	// created
		Running		= 1,	// transitioning some value
		ToPause		= 2,	// pause request
		Paused		= 3,	// transitioning suspended
		ToResume	= 4,	// resume request
		Done		= 5,	// transition ended
		ToRemove	= 6		// remove from active list request
	};

	// list of those we have to call
	typedef AkArray<ITransitionable*, ITransitionable*,ArrayPoolDefault> AkTransitionUsersList;
	AkTransitionUsersList	m_UsersList;

	// Curve to use for the transition
	AkCurveInterpolation			m_eFadeCurve;

	// our transition's current state
	State		m_eState;

	// values are dBs
	bool		m_bdBs :1;

	bool		m_bIsFiltering :1;

	AkReal32	m_fReleaseCoef;			// Release one-pole filter coefficient.

	AkDeltaType m_eChangeReason;
};
//----------------------------------------------------------------------------------------------------
// 
//----------------------------------------------------------------------------------------------------
class TransitionParameters
{
public:
	TransitionParameters( 
		ITransitionable*	in_pUser,
		AkIntPtr			in_eTarget,
		AkReal32			in_fStartValue,
		AkReal32			in_fTargetValue,
		AkTimeMs			in_lDuration,
		AkCurveInterpolation in_eFadeCurve,
		AkDeltaType			in_eChangeReason,
		bool				in_bdBs,		
		bool				in_bUseReciprocalCurve = true,
		bool				in_bUseFiltering = false )
		: pUser( in_pUser )
		, eTarget( in_eTarget )
		, fStartValue( in_fStartValue )
		, fTargetValue( in_fTargetValue )
		, lDuration( in_lDuration )
		, eFadeCurve( in_eFadeCurve )
		, eChangeReason(in_eChangeReason)
		, bdBs( in_bdBs )		
		, bUseReciprocalCurve( in_bUseReciprocalCurve )
		, bUseFiltering( in_bUseFiltering )
	{}
	~TransitionParameters() {}

	ITransitionable*	pUser;			// who will update the value
	AkIntPtr			eTarget;		// what's in the union
	AkReal32			fStartValue;	// where we are starting from
	AkReal32			fTargetValue;	// where we	are going to
	AkTimeMs			lDuration;		// how long it will take
	AkCurveInterpolation eFadeCurve;	// what shape to use
	AkDeltaType			eChangeReason;
	bool				bdBs;			// start and target are dB values	
	bool				bUseReciprocalCurve;	// use reciprocal curve when fading out
	bool				bUseFiltering;	// use reciprocal curve when fading out

private:
	TransitionParameters();
};
#endif
