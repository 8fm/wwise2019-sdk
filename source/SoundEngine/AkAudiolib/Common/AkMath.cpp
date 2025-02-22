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
// AkMath.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h" 

#include "AkMath.h"
#include "AkRandom.h"

namespace AkMath
{
	//====================================================================================================
	//====================================================================================================
	void Normalise(AkVector& io_Vector )
	{
		AkReal32 fMagnitude = Magnitude(io_Vector);

		// PhM : this prevents from dividing by 0,
		// but does not prevent from getting huge values
		if(fMagnitude != 0.0f)
		{
			io_Vector.X /= fMagnitude;
			io_Vector.Y /= fMagnitude;
			io_Vector.Z /= fMagnitude;
		}
	}
	//====================================================================================================
	//====================================================================================================
	AkReal32 Distance(const AkVector& Position1, const AkVector& Position2)
	{
		AkVector	Distance;

		Distance.X = Position1.X - Position2.X;
		Distance.Y = Position1.Y - Position2.Y;
		Distance.Z = Position1.Z - Position2.Z;

		return Magnitude(Distance);
	}
	//====================================================================================================
	// interpolates between |in_fLowerX and |in_fUpperX at position in_fX
	//                      |in_fLowerY     |in_fUpperY
	//====================================================================================================
	AkReal32 Interpolate(AkReal32 in_fLowerX,AkReal32 in_fLowerY,
								AkReal32 in_fUpperX,AkReal32 in_fUpperY,
								AkReal32 in_fX)
	{
		AkReal32 fInterpolated;

#ifdef XBOX360_DISABLED //this code doesn't work when in_fUpperX == in_fLowerX
		// are we below the min ?
		// fpDest = fpRegA >= 0 ? fpRegB : fpRegA
		AkReal32 fx = (AkReal32) __fsel(in_fX - in_fLowerX, in_fX, in_fLowerX);

		// are we above the max ?
		// fpDest = fpRegA >= 0 ? fpRegB : fpRegA
		fx = (AkReal32) __fsel(in_fUpperX - fx, fx, in_fUpperX);

		// compute this anyway
		AkReal32 fA = ( fx - in_fLowerX ) / ( in_fUpperX - in_fLowerX );
		fInterpolated = in_fLowerY + ( fA * (in_fUpperY - in_fLowerY) );
#else
		// are we below the min ?
		if(in_fX <= in_fLowerX)
		{
			fInterpolated = in_fLowerY;
		}
		// are we above the mas ?
		else if(in_fX >= in_fUpperX)
		{
			fInterpolated = in_fUpperY;
		}
		// we're in between
		else
		{
			AkReal32 fA = ( in_fX - in_fLowerX ) / ( in_fUpperX - in_fLowerX );
			fInterpolated = in_fLowerY + ( fA * (in_fUpperY - in_fLowerY) );
		}
#endif

		return fInterpolated;
	}
	//====================================================================================================
	// interpolates between |0.0f       and |1.0f       at position in_fX
	//                      |in_fLowerY     |in_fUpperY
	//====================================================================================================
	AkReal32 NormalisedInterpolate(AkReal32 in_fLowerY,
											AkReal32 in_fUpperY,
											AkReal32 in_fX)
	{
		AkReal32 fInterpolated;

		// are we below the min ?
		if(in_fX <= 0.0f)
		{
			fInterpolated = in_fLowerY;
		}
		// are we above the mas ?
		else if(in_fX >= 1.0f)
		{
			fInterpolated = in_fUpperY;
		}
		// we're in between
		else
		{
			fInterpolated = in_fLowerY + ( in_fX * (in_fUpperY - in_fLowerY) );
		}

		return fInterpolated;
	}

	//====================================================================================================
	// RangeRange returns a random float value between in_fMin and in_fMax
	//====================================================================================================
	AkReal32 RandRange(AkReal32 in_fMin, AkReal32 in_fMax )
	{
		// Note: RAND_MAX is in stdlib.h and RAND_MAX value may be platform independent
		// There may be a need in the future depending on target platforms to adapt this 
		// code. For the moment being all supported platforms should have no problem with the following.

		// Ensure max provided is above the minimum
		if (in_fMax < in_fMin)
		{
			in_fMax = in_fMin;
		}
		
		// Get an integer in range (0,RAND_MAX)
		AkReal32 fRandVal = AKRANDOM::AkRandom() / static_cast<AkReal32>(AKRANDOM::AK_RANDOM_MAX);
		return ( fRandVal * (in_fMax - in_fMin) + in_fMin );
	}
}

