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
// AkInterpolation.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _INTERPOLATION_H_
#define _INTERPOLATION_H_

#include "AkPrivateTypes.h"
#include "AkMath.h"

namespace AkInterpolation
{
	// Interpolate and output reals. Note that AkCurveInterpolation_Constant is not supported.
	inline AkReal32 InterpolateNoCheck(AkReal32 in_fTimeRatio, AkReal32 in_fInitialVal, AkReal32 in_fTargetVal, AkCurveInterpolation in_eFadeCurve)
	{
		switch(in_eFadeCurve)
		{
		case AkCurveInterpolation_Linear:
			return in_fTimeRatio * ( in_fTargetVal - in_fInitialVal ) + in_fInitialVal;
		case AkCurveInterpolation_Exp1:
			//return pow( in_fTimeRatio, 1.41f ) * ( in_fTargetVal - in_fInitialVal ) + in_fInitialVal;
			return ( in_fTimeRatio + 1 ) * in_fTimeRatio * 0.5f * ( in_fTargetVal - in_fInitialVal ) + in_fInitialVal;
		case AkCurveInterpolation_SineRecip:
			//return cos( PIOVERTWO * in_fTimeRatio ) * ( in_fInitialVal - in_fTargetVal ) + in_fTargetVal;
			// Using a 7th order MacLaurin series of cos with minimax economization over range [0,PI/2]. Error <= 6.7e-6
			{
				AkReal32 x = PIOVERTWO * in_fTimeRatio;
				AkReal32 x2 = x * x;
				return ( 0.999993295282167f + x2 * ( -4.99912439712246e-1f + x2 * ( 4.14877480454292e-2f + x2 * -1.27120948569655e-3f ) ) )
					* ( in_fInitialVal - in_fTargetVal ) + in_fTargetVal;
			}
		case AkCurveInterpolation_Exp3:
			return ( in_fTimeRatio * in_fTimeRatio * in_fTimeRatio * ( in_fTargetVal - in_fInitialVal ) ) + in_fInitialVal;
		case AkCurveInterpolation_Log1:
			//return pow( 1 - in_fTimeRatio, 1.41f ) * ( in_fInitialVal - in_fTargetVal ) + in_fTargetVal; 
			return ( ( in_fTimeRatio - 3 ) * in_fTimeRatio ) * 0.5f * ( in_fInitialVal - in_fTargetVal ) + in_fInitialVal;
		case AkCurveInterpolation_Sine:
			//return sin( PIOVERTWO * in_fTimeRatio ) * ( in_fTargetVal - in_fInitialVal ) + in_fInitialVal;
			// Using a 7th order MacLaurin series of sin with minimax economization over range [0,PI/2]. Error <= 5.9e-7
			/// REVIEW: Could reduce order and obtain precision ~6e-5.
			{
				//return AkMath::FastSin(PIOVERTWO * in_fTimeRatio) * (in_fTargetVal - in_fInitialVal) + in_fInitialVal;

				//Straight FastSin, given the reduced range of 0 to PI/2 (no Abs calls necessary)
				AkReal32 x = PIOVERTWO * in_fTimeRatio;
				AKASSERT(x >= 0 && x <= PI/2);
				static const float B = 4 / PI;
				static const float C = -4 / (PI*PI);
				static const float P = 0.225f;
				static const float Pm = 1.f-0.225f;

				float y = B * x + C * x * x;

				//  const float Q = 0.775;
				//return P * (y * y - y) + y;   // Q * y + P * y * abs(y)
				//return P * y*y - P*y +y
				return (P * y*y + Pm*y) * (in_fTargetVal - in_fInitialVal) + in_fInitialVal;			
			}
		case AkCurveInterpolation_Log3:
			{
				AkReal32 fFlippedTimeRatio = 1 - in_fTimeRatio;
				return fFlippedTimeRatio * fFlippedTimeRatio * fFlippedTimeRatio * ( in_fInitialVal - in_fTargetVal ) + in_fTargetVal; 
			}
		case AkCurveInterpolation_SCurve:
			{
				// return ( in_fTargetVal - in_fInitialVal ) * ( 1 - cos( PI * in_fTimeRatio ) ) * 0.5f + in_fInitialVal;
				// Using a 7th order MacLaurin series of (1-cos)/2 with minimax economization over range [0,PI]. Error <= 7e-4
				AkReal32 x = PI * in_fTimeRatio;
				AkReal32 x2 = x * x;
				return ( 6.967021361369841e-4f + x2 * ( 2.476747878488185e-1f + x2 * ( -1.961384016191180e-2f + x2 * 4.848339972837020e-4f ) ) )
					* ( in_fTargetVal - in_fInitialVal ) + in_fInitialVal;
			}
		case AkCurveInterpolation_InvSCurve:
			{
				/*
				if(in_fTimeRatio <= 0.5f)
					return ( ( in_fTargetVal - in_fInitialVal ) * sin( PI * in_fTimeRatio ) * 0.5f ) + in_fInitialVal;
				else
					return ( in_fTargetVal - in_fInitialVal ) * ( 1 - sin( PI * in_fTimeRatio ) * 0.5f ) + in_fInitialVal;
					*/
				// Using a 7th order MacLaurin series of sin(x)/2 with minimax economization over range [0,PI/2]. Error <= 5.9e-7
				if ( in_fTimeRatio <= 0.5f )
				{
					AkReal32 x = PI * in_fTimeRatio;
					AkReal32 x2 = x * x;			
					return ( x * ( 0.499998307954001f + x2 * ( -8.33241419094755e-2f + x2 * ( 4.153162613579945e-3f + x2 * -9.181826988473399e-5f ) ) ) )
						* ( in_fTargetVal - in_fInitialVal ) + in_fInitialVal;
				}
				else
				{
					AkReal32 x = PI - PI * in_fTimeRatio;
					AkReal32 x2 = x * x;			
					return ( 1.f - x * ( 0.499998307954001f + x2 * ( -8.33241419094755e-2f + x2 * ( 4.153162613579945e-3f + x2 * -9.181826988473399e-5f ) ) ) )
						* ( in_fTargetVal - in_fInitialVal ) + in_fInitialVal;
				}
			}
		default:
			AKASSERT(!"Fade curve not supported");
		}
		return 0;
	}

	AkForceInline AkReal32 InterpolateNoCheck2(AkReal32 in_fTimeRatio, AkReal32 in_fInitialVal, AkReal32 in_fTargetVal, AkReal32 in_fHeight, AkCurveInterpolation in_eFadeCurve)
	{		
		switch (in_eFadeCurve)
		{	
		case AkCurveInterpolation_Exp1:
			//return pow( in_fTimeRatio, 1.41f ) * ( in_fTargetVal - in_fInitialVal ) + in_fInitialVal;			
			return (in_fTimeRatio + 1) * in_fTimeRatio * in_fHeight + in_fInitialVal;
		case AkCurveInterpolation_SineRecip:
			//return cos( PIOVERTWO * in_fTimeRatio ) * ( in_fInitialVal - in_fTargetVal ) + in_fTargetVal;
			// Using a 7th order MacLaurin series of cos with minimax economization over range [0,PI/2]. Error <= 6.7e-6
		{			
			AkReal32 x = in_fTimeRatio;
			AkReal32 x2 = x * x;
			return (0.999993295282167f + x2 * (-4.99912439712246e-1f + x2 * (4.14877480454292e-2f + x2 * -1.27120948569655e-3f)))
				* (-in_fHeight) + in_fTargetVal;
		}
		case AkCurveInterpolation_Exp3:
			return (in_fTimeRatio * in_fTimeRatio * in_fTimeRatio * in_fHeight) + in_fInitialVal;
		case AkCurveInterpolation_Log1:
			//return pow( 1 - in_fTimeRatio, 1.41f ) * ( in_fInitialVal - in_fTargetVal ) + in_fTargetVal; 			
			return ((in_fTimeRatio - 3) * in_fTimeRatio) * in_fHeight + in_fInitialVal;
		case AkCurveInterpolation_Sine:			
		{
			//return AkMath::FastSin(PIOVERTWO * in_fTimeRatio) * in_fHeight + in_fInitialVal;

			//Straight FastSin, given the reduced range of 0 to PI/2 (no Abs calls necessary)			
			AkReal32 x = in_fTimeRatio;
			AKASSERT(x >= 0 && x <= PI / 2);
			static const float B = 4 / PI;
			static const float C = -4 / (PI*PI);
			static const float P = 0.225f;
			static const float Pm = 1.f - 0.225f;

			float y = B * x + C * x * x;			
			return (P * y*y + Pm*y) * in_fHeight + in_fInitialVal;
		}
		case AkCurveInterpolation_Log3:
		{
			AkReal32 fFlippedTimeRatio = 1 - in_fTimeRatio;
			return fFlippedTimeRatio * fFlippedTimeRatio * fFlippedTimeRatio * (-in_fHeight) + in_fTargetVal;
		}
		case AkCurveInterpolation_SCurve:
		{
			// return ( in_fTargetVal - in_fInitialVal ) * ( 1 - cos( PI * in_fTimeRatio ) ) * 0.5f + in_fInitialVal;
			// Using a 7th order MacLaurin series of (1-cos)/2 with minimax economization over range [0,PI]. Error <= 7e-4			
			AkReal32 x =  in_fTimeRatio;
			AkReal32 x2 = x * x;			
			return (6.967021361369841e-4f + x2 * (2.476747878488185e-1f + x2 * (-1.961384016191180e-2f + x2 * 4.848339972837020e-4f))) * in_fHeight + in_fInitialVal;
		}
		case AkCurveInterpolation_InvSCurve:
		{		
			// Using a 7th order MacLaurin series of sin(x)/2 with minimax economization over range [0,PI/2]. Error <= 5.9e-7
			if (in_fTimeRatio <= 0.5f*PI)
			{				
				AkReal32 x = in_fTimeRatio;
				AkReal32 x2 = x * x;
				return (x * (0.499998307954001f + x2 * (-8.33241419094755e-2f + x2 * (4.153162613579945e-3f + x2 * -9.181826988473399e-5f))))
					* in_fHeight + in_fInitialVal;
			}
			else
			{				
				AkReal32 x = PI - in_fTimeRatio;
				AkReal32 x2 = x * x;
				return (1.f - x * (0.499998307954001f + x2 * (-8.33241419094755e-2f + x2 * (4.153162613579945e-3f + x2 * -9.181826988473399e-5f))))
					* in_fHeight + in_fInitialVal;
			}
		}
		default:
			AKASSERT(!"Fade curve not supported");
		}
		return 0;
	}
}

#endif

