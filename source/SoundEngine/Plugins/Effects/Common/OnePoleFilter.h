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

// Generic one pole filter. Coefficients are set explicitely by client.
// y[n] = fB0 * x[n] - fA1 * y[n - 1]
// To be used on mono signals, create as many instances as there are channels if need be
// Also provides processing per sample

#ifndef _AKONEPOLEFILTER_H_
#define _AKONEPOLEFILTER_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include "AkDSPUtils.h"
#include <math.h>

namespace DSP
{

	class OnePoleFilter
	{
	public:
		enum FilterType
		{
			FILTERCURVETYPE_NONE			=  0,	
			FILTERCURVETYPE_LOWPASS			=  1,	
			FILTERCURVETYPE_HIGHPASS		=  2
		};

		OnePoleFilter() 
			:	fFFbk1( 0.f ),
			fB0( 0.f ),
			fA1( 0.f ){}

		void Reset( )
		{
			fFFbk1 = 0.f;
		}

		// Note: To avoid repeat costly filter coefficient computation when multi-channel have the same coefs,
		// ComputeCoefs() may be used followed by an explicit SetCoefs per channel
		// Calculations done in 64-bit to properly handle low fc values (such as in the LFO).
		static void ComputeCoefs( FilterType eType, AkReal64 in_fFc, AkReal64 in_fSr, AkReal32 & out_fB0, AkReal32 & out_fA1 )
		{
			switch ( eType )
			{
				case FILTERCURVETYPE_NONE:
					out_fA1 = 0.f;
					out_fB0 = 1.f;
					break;
				case FILTERCURVETYPE_LOWPASS:
					{
						AkReal64 fNormalizedFc = in_fFc / in_fSr;
						AkReal64 fTemp = 2.0 - cos(2.0*3.14159265358979323846*fNormalizedFc);
						out_fA1 = (AkReal32) ( sqrt(fTemp * fTemp - 1.0) - fTemp );
						out_fB0 = 1.f + out_fA1;
						break;
					}
				case FILTERCURVETYPE_HIGHPASS:
					{
						AkReal64 fNormalizedFc = in_fFc / in_fSr;
						AkReal64 fTemp = 2.0 + cos(2.0*3.14159265358979323846*fNormalizedFc);
						out_fA1 = (AkReal32) ( fTemp - sqrt(fTemp * fTemp - 1.0 ) );
						out_fB0 = 1.f - out_fA1;
					}
			}
		}
		
		void SetCoefs( AkReal32 in_fB0, AkReal32 in_fA1 )
		{
			fB0 = in_fB0;
			fA1 = in_fA1;
		}

		void SetCoefs( FilterType eType, AkReal64 in_fFc, AkReal64 in_fSr )
		{
			ComputeCoefs( eType, in_fFc, in_fSr, fB0, fA1 );
		}
		
		void ProcessBuffer( AkReal32 * io_pfBuffer, AkUInt32 in_uNumFrames )
		{
			AkReal32 * AK_RESTRICT pfBuf = (AkReal32 *) io_pfBuffer;
			const AkReal32 * const fEnd = pfBuf + in_uNumFrames;
			AkReal32 l_fFFbk1 = fFFbk1;
			while ( pfBuf < fEnd )
			{
				AkReal32 fOut = *pfBuf * fB0 - fA1 * l_fFFbk1;
				l_fFFbk1 = fOut;
				*pfBuf++ = fOut;		
			}

			RemoveDenormal( l_fFFbk1 );
			fFFbk1 = l_fFFbk1;
		}

		AkForceInline AkReal32 ProcessSample( AkReal32 in_fIn )
		{
			AkReal32 fOut = in_fIn * fB0 - fA1 * fFFbk1;
			fFFbk1 = fOut;
			return fOut;
		}

		AkForceInline AkReal32 ProcessSample( AkReal32 in_fIn, AkReal32 & io_fFFbk1 )
		{
			AkReal32 fOut = in_fIn * fB0 - fA1 * io_fFFbk1;
			io_fFFbk1 = fOut;
			return fOut;
		}

		//	protected:
		AkReal32 fFFbk1;	// first order feedback memory
		AkReal32 fB0;
		AkReal32 fA1;
	};

} // namespace DSP

#endif // _AKONEPOLEFILTER_H_
