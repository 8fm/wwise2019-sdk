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
// AkConversionTable.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _CONVERSION_TABLE_H_
#define _CONVERSION_TABLE_H_

#include "AkRTPC.h"
#include "AkInterpolation.h"
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/Tools/Common/AkObject.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>
#include <float.h>
#include "AkMath.h"


class CAkConversionTable
{
public:

	//Constructor
	CAkConversionTable()		
	{
		Init();
	}

	AkForceInline void Init()
	{
		m_pArrayGraphPoints = NULL;
		m_ulArraySize = 0;
		m_eScaling = AkCurveScaling_None;
		m_idx = 1;
	}

	AkForceInline AkReal32 Convert( AkReal32 in_valueToConvert ) const
	{		
		AkUInt32 uIdx = m_idx;
		AkReal32 fVal = ConvertInternal(in_valueToConvert, uIdx);
		m_idx = uIdx;
		return fVal;
	}

	AkForceInline AkReal32 ConvertProgressive( AkReal32 in_valueToConvert, AkUInt32& io_index )
	{		
		io_index++;	//For the external world, the pre/post segments don't exist.
		AkReal32 fVal = ConvertInternal( in_valueToConvert, io_index);			
		io_index--;
		return fVal;
	}	

	//Convert the input value into the output value, applying conversion if the converter is available
#define POLY_MASK ((1 << AkCurveInterpolation_Linear) | (1 << AkCurveInterpolation_Constant))

	AkReal32 ConvertInternal(AkReal32 in_valueToConvert, AkUInt32& io_index) const
	{
		AKASSERT(m_pArrayGraphPoints && m_ulArraySize);
		AkReal32 l_returnedValue;

		/*Check the cached index first!
		Discussion on the advantage of caching the last index used:
		Keeping the last used index seems like a bad idea since a curve can be used on multiple Game Objects that don't have the same RTPC value.
		So subsequent calls to Convert would "dirty" the cache and make it miss, rendering this caching inefficient.  This is partly true.
		I studied 3 methods, for an equal probability of input values:
		a) Not caching, starting at the beginning
		b) Not caching, starting at the Middle segment
		c) Caching.

		Did some simulations with 5 segments (an average size curve).
		Option a) yields a cost of C(n,k)/N average search loops (i.e for a N=5, avg cost = 3 iterations)
		Option B) yields a cost of C(n-1,k)/N average search loops (i.e for a N=5, avg cost = 2.2 iterations)
		Option c) is harder to evaluate as the search point and initial point distance can vary between 0 and 4 but with a skewed distribution.
		This makes the avg cost at 2.6 iterations.

		So why pick an inferior option?  Because of expected usage patterns.
		+ Global RTPCs share a singular value across all objects.
		+ RTPC used on a single object are unique and also benefit from the cache.
		+ Segments aren't of equal length. Long segments will encompass a larger swath of RTPC values, making each index not of equal probability.

		So, cached index it is, until proven otherwise.
		*/

		const AkRTPCGraphPoint* AK_RESTRICT pFirst = m_pArrayGraphPoints + io_index;
		const AkRTPCGraphPoint* AK_RESTRICT pSecond = pFirst + 1;

		if (in_valueToConvert >= pFirst->From && in_valueToConvert < pSecond->From)
		{
		retry:
			// You will get here if:
			//    * You evaluate between 2 points					
			Coefficients& rCoefs = ((Coefficients*)(m_pArrayGraphPoints + m_ulArraySize))[io_index];
			AkReal32 fA = (in_valueToConvert - pFirst->From);
			if ((1 << pFirst->Interp) & POLY_MASK)
			{
				// Polynomial interpolation between the two points (const, lin). Could do quad (Exp1 & Lin1) too, but const and lin are the most used by far.
				l_returnedValue = fA * rCoefs.fDivisor + pFirst->To;
			}
			else
			{
				l_returnedValue = AkInterpolation::InterpolateNoCheck2(
					fA * rCoefs.fDivisor,
					static_cast<AkReal32>(pFirst->To),
					static_cast<AkReal32>(pSecond->To),
					rCoefs.fHeight,
					pFirst->Interp);
			}

			return ApplyCurveScaling(l_returnedValue);
		}
		else
		{
			if (in_valueToConvert < pFirst->From)
			{
				while (io_index > 0 && in_valueToConvert < pFirst->From)
				{
					pFirst--;
					io_index--;
				}
				pSecond = pFirst + 1;
			}
			else if (in_valueToConvert >= pSecond->From)
			{
				while ((io_index+1) < m_ulArraySize && in_valueToConvert >= pSecond->From)
				{
					pSecond++;
					io_index++;
				}
				pFirst = pSecond - 1;
			}

			goto retry;
		}

		return l_returnedValue;
	}

	

	
	AkForceInline AkReal32 ApplyCurveScaling( AkReal32 in_rVal ) const
	{
		AkReal32 l_returnedValue = in_rVal;

		switch( m_eScaling )
		{
		case AkCurveScaling_None:
			break;

		case AkCurveScaling_dB:
			l_returnedValue = static_cast<AkReal32>( AkMath::ScalingFromLin_dB( static_cast<AkReal32>( l_returnedValue ) ) );
			break;

		case AkCurveScaling_Log:
			l_returnedValue = static_cast<AkReal32>( AkMath::ScalingFromLog( static_cast<AkReal32>( l_returnedValue ) ) );
			break;

		case AkCurveScaling_dBToLin:
			l_returnedValue = static_cast<AkReal32>( AkMath::dBToLin( static_cast<AkReal32>( l_returnedValue ) ) );
			break;

		default:
			AKASSERT( ! "Unknown scaling mode!" );
		}

		return l_returnedValue;
	}

	AKRESULT Clone(AK::IAkPluginMemAlloc* in_pAllocator, const CAkConversionTable & in_rClone)
	{
		Unset(in_pAllocator);
		if (!in_rClone.IsInitialized())		
			return AK_Success;		
		
		m_ulArraySize = in_rClone.m_ulArraySize;
		m_eScaling = in_rClone.m_eScaling;
		m_idx = 1;

		AkUInt32 uSize = m_ulArraySize * (sizeof(AkRTPCGraphPoint) + sizeof(Coefficients));
		m_pArrayGraphPoints = (AkRTPCGraphPoint*)AK_PLUGIN_ALLOC(in_pAllocator, uSize);
		if (m_pArrayGraphPoints)
		{
			AKPLATFORM::AkMemCpy(m_pArrayGraphPoints, in_rClone.m_pArrayGraphPoints, uSize);
			return AK_Success;
		}
		
		return AK_InsufficientMemory;
	}

	AKRESULT Clone(const CAkConversionTable & in_rClone)
	{
		Unset();
		if (!in_rClone.IsInitialized())		
			return AK_Success;		

		m_ulArraySize = in_rClone.m_ulArraySize;
		m_eScaling = in_rClone.m_eScaling;
		m_idx = 1;

		AkUInt32 uSize = m_ulArraySize * (sizeof(AkRTPCGraphPoint) + sizeof(Coefficients));
		m_pArrayGraphPoints = (AkRTPCGraphPoint*)AkAlloc(AkMemID_Object, uSize);
		if (m_pArrayGraphPoints)
		{
			AKPLATFORM::AkMemCpy(m_pArrayGraphPoints, in_rClone.m_pArrayGraphPoints, uSize);
			return AK_Success;
		}

		return AK_InsufficientMemory;
	}

	inline AKRESULT Set(
		AK::IAkPluginMemAlloc*		in_pAllocator,
		AkRTPCGraphPoint*			in_pArrayConversion,
		AkUInt32					in_ulConversionArraySize,
		AkCurveScaling				in_eScaling
		)
	{
		AKRESULT eResult = AK_InvalidParameter;
		Unset(in_pAllocator);
		AKASSERT(!m_pArrayGraphPoints);

		if (in_pArrayConversion && in_ulConversionArraySize)
		{
			m_idx = 1;
			m_ulArraySize = in_ulConversionArraySize + 2;
			AkUInt32 uSize = m_ulArraySize * (sizeof(AkRTPCGraphPoint) + sizeof(Coefficients));
			m_pArrayGraphPoints = (AkRTPCGraphPoint*)AK_PLUGIN_ALLOC(in_pAllocator, uSize);
			if (m_pArrayGraphPoints)
			{
				CopyPoints(in_pArrayConversion, in_ulConversionArraySize, in_eScaling);
				eResult = AK_Success;
			}
			else
			{
				eResult = AK_InsufficientMemory;
				m_ulArraySize = 0;
			}
		}

		return eResult;
	}

	inline AKRESULT Set(
		AkRTPCGraphPoint*			in_pArrayConversion,
		AkUInt32					in_ulConversionArraySize,
		AkCurveScaling				in_eScaling)
	{
		AKRESULT eResult = AK_InvalidParameter;
		Unset();
		AKASSERT(!m_pArrayGraphPoints);

		if (in_pArrayConversion && in_ulConversionArraySize)
		{
			m_idx = 1;
			m_ulArraySize = in_ulConversionArraySize + 2;
			AkUInt32 uSize = m_ulArraySize * (sizeof(AkRTPCGraphPoint) + sizeof(Coefficients));
			m_pArrayGraphPoints = (AkRTPCGraphPoint*)AkAlloc(AkMemID_Object, uSize);
			if (m_pArrayGraphPoints)
			{
				CopyPoints(in_pArrayConversion, in_ulConversionArraySize, in_eScaling);
				eResult = AK_Success;
			}
			else
			{
				eResult = AK_InsufficientMemory;
				m_ulArraySize = 0;
			}
		}

		return eResult;
	}	

	void CopyPoints(AkRTPCGraphPoint* in_pArrayConversion, AkUInt32	in_ulConversionArraySize, AkCurveScaling in_eScaling)
	{
		m_eScaling = in_eScaling;

		AKPLATFORM::AkMemCpy(m_pArrayGraphPoints + 1, in_pArrayConversion, in_ulConversionArraySize * sizeof(AkRTPCGraphPoint));

		m_pArrayGraphPoints[0].Interp = AkCurveInterpolation_Constant;
		m_pArrayGraphPoints[0].From = -FLT_MAX;
		// Copy the value "To" from the secont element (copied previouly) instead of in_pArrayConversion[0] since its address
		// may be an odd vallue which cause an invallid memory access on 32 bits platforms
		m_pArrayGraphPoints[0].To = m_pArrayGraphPoints[1].To;

		m_pArrayGraphPoints[m_ulArraySize - 1].Interp = AkCurveInterpolation_Constant;
		m_pArrayGraphPoints[m_ulArraySize - 1].From = FLT_MAX;
		// Copy the value "To" from the secont last element (copied previouly) instead of in_pArrayConversion[in_ulConversionArraySize - 1]
		// since the address of in_pArrayConversion may be an odd vallue which cause an invallid memory access on 32 bits platforms
		m_pArrayGraphPoints[m_ulArraySize - 1].To = m_pArrayGraphPoints[m_ulArraySize - 2].To;

		RecomputeCoefficients();
	}

	void RecomputeCoefficients()
	{
		Coefficients *pCoefs = (Coefficients *)(m_pArrayGraphPoints + m_ulArraySize);
		//Compute vertical delta and divisor for horizontal normalization.
		//For some functions, include constants too, where easily applicable to save some operations.
		for (AkUInt32 i = 0; i < m_ulArraySize; i++)
		{
			pCoefs[i].fHeight = m_pArrayGraphPoints[i + 1].To - m_pArrayGraphPoints[i].To;
			pCoefs[i].fDivisor = 1.0f / (m_pArrayGraphPoints[i + 1].From - m_pArrayGraphPoints[i].From);
			if (m_pArrayGraphPoints[i].Interp == AkCurveInterpolation_Constant)
				pCoefs[i].fDivisor = 0;
			else if (m_pArrayGraphPoints[i].Interp == AkCurveInterpolation_Linear)
				pCoefs[i].fDivisor *= (m_pArrayGraphPoints[i + 1].To - m_pArrayGraphPoints[i].To);
			else if (m_pArrayGraphPoints[i].Interp == AkCurveInterpolation_Sine || m_pArrayGraphPoints[i].Interp == AkCurveInterpolation_SineRecip)
				pCoefs[i].fDivisor *= PIOVERTWO;
			else if (m_pArrayGraphPoints[i].Interp == AkCurveInterpolation_SCurve || m_pArrayGraphPoints[i].Interp == AkCurveInterpolation_InvSCurve)
				pCoefs[i].fDivisor *= PI;
			else if (m_pArrayGraphPoints[i].Interp == AkCurveInterpolation_Log1)
				pCoefs[i].fHeight *= -0.5f;
			else if (m_pArrayGraphPoints[i].Interp == AkCurveInterpolation_Exp1)
				pCoefs[i].fHeight *= 0.5f;
		}
	}
		

	// REVIEW: this should eventually be done at soundbank generation time.
	void Linearize()
	{
		if ( m_eScaling == AkCurveScaling_None )
		{
			m_eScaling = AkCurveScaling_dBToLin;
		}
		else if ( m_eScaling == AkCurveScaling_dB )
		{
			for( AkUInt32 i = 0; i < m_ulArraySize; ++i )
			{
				AKASSERT( m_pArrayGraphPoints[ i ].To <= 0.0f );
				m_pArrayGraphPoints[ i ].To = m_pArrayGraphPoints[ i ].To + 1;
			}

			m_eScaling = AkCurveScaling_None;
			RecomputeCoefficients();
		}
		else
		{
			AKASSERT( false && "Scaling type cannot be linearized!" );
		}
	}
	
	void Unset(AK::IAkPluginMemAlloc* in_pAllocator)
	{
		if (m_pArrayGraphPoints)
		{
			AK_PLUGIN_FREE(in_pAllocator, m_pArrayGraphPoints);
			m_pArrayGraphPoints = NULL;
		}

		m_ulArraySize = 0;
		m_eScaling = AkCurveScaling_None;
	}
	
	void Unset()
	{
		if(m_pArrayGraphPoints)
		{
			AkFree(AkMemID_Object, m_pArrayGraphPoints);
			m_pArrayGraphPoints = NULL;
		}

		m_ulArraySize = 0;
		m_eScaling = AkCurveScaling_None;
	}

	AkReal32 GetMidValue()
	{
		AkReal32 l_DefaultVal = 0;
		if( m_pArrayGraphPoints && m_ulArraySize)
		{
			AkReal32 l_LowerBound = m_pArrayGraphPoints[1].From;
			AkReal32 l_HigherBound = m_pArrayGraphPoints[m_ulArraySize-2].From;
			l_DefaultVal = ( l_LowerBound + l_HigherBound )* 0.5f;
		}
		return l_DefaultVal;
	}

	void GetLine(AkReal32& out_fSlope, AkReal32& out_fIntercept)
	{
		//TBD: Could relax these assertions...
		AKASSERT(m_ulArraySize == 4);

		AkRTPCGraphPoint firstPoint( m_pArrayGraphPoints[1] );
		AkRTPCGraphPoint secondPoint( m_pArrayGraphPoints[2] );

		firstPoint.To = ApplyCurveScaling( firstPoint.To );
		secondPoint.To = ApplyCurveScaling( secondPoint.To );

		firstPoint.To = AkMath::dBToLin( firstPoint.To );
		secondPoint.To = AkMath::dBToLin( secondPoint.To );

		out_fIntercept = static_cast<AkReal32>( AkMath::InterpolateNoCheck(
				firstPoint.From,
				static_cast<AkReal32>( firstPoint.To ),
				secondPoint.From,
				static_cast<AkReal32>( secondPoint.To ),
				0.0
				) );

		out_fSlope = static_cast<AkReal32>( AkMath::InterpolateNoCheck(
				firstPoint.From,
				static_cast<AkReal32>( firstPoint.To ),
				secondPoint.From,
				static_cast<AkReal32>( secondPoint.To ),
				1.0
				) ) - out_fIntercept;
	}

	AkForceInline AkRTPCGraphPoint & GetFirstPoint() const {
		return m_pArrayGraphPoints[1];
	}

	AkForceInline AkRTPCGraphPoint & GetLastPoint() const {
		return m_pArrayGraphPoints[m_ulArraySize - 2];
	}


	AkForceInline AkRTPCGraphPoint &Point(AkUInt32 i) const { return m_pArrayGraphPoints[i + 1]; }

	AkForceInline bool IsInitialized() const { return m_pArrayGraphPoints != NULL; }

	AkForceInline AkUInt32 Size() const { return m_ulArraySize != 0 ? m_ulArraySize - 2 : 0; }

	AkForceInline AkCurveScaling Scaling() const { return m_eScaling; }

private:		
	AkRTPCGraphPoint*			m_pArrayGraphPoints;
	AkUInt32					m_ulArraySize;		//The number of sets of points in the array
	AkCurveScaling				m_eScaling;
	struct Coefficients
	{
		AkReal32 fDivisor;
		AkReal32 fHeight;
	};
	volatile mutable AkUInt32		m_idx;
};

#endif //_CONVERSION_TABLE_H_
