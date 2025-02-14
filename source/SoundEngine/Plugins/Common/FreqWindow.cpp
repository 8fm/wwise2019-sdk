/***********************************************************************
The content of this file includes source code for the sound engine
portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
Two Source Code" as defined in the Source Code Addendum attached
with this file.  Any use of the Level Two Source Code shall be
subject to the terms and conditions outlined in the Source Code
Addendum and the End User License Agreement for Wwise(R).

Version:  Build: 
Copyright (c) 2006-2020 Audiokinetic Inc.
***********************************************************************/

#include "FreqWindow.h"
#include <AkMath.h>
#include <AK/Tools/Common/AkAssert.h>
#include <AK/DSP/AkApplyGain.h>
#include <AK/SoundEngine/Common/AkSimd.h>
#include <AK/SoundEngine/Common/AkFPUtilities.h>
#include <math.h>

namespace DSP
{

namespace BUTTERFLYSET_NAMESPACE
{
	AKRESULT CAkFreqWindow::Alloc(	
		AK::IAkPluginMemAlloc *	in_pAllocator,
		AkUInt32 in_uFFTSize )
	{
		m_uFFTSize = in_uFFTSize;
		m_pfFreqData = (ak_fft_cpx *)AK_PLUGIN_ALLOC( in_pAllocator, AK_ALIGN_SIZE_FOR_DMA((m_uFFTSize/2+1)*sizeof(ak_fft_cpx)) );
		if ( m_pfFreqData == NULL )
			return AK_InsufficientMemory;

		return AK_Success;
	}

	void CAkFreqWindow::Free( AK::IAkPluginMemAlloc * in_pAllocator )
	{
		if ( m_pfFreqData )
		{	
			AK_PLUGIN_FREE( in_pAllocator, m_pfFreqData );
			m_pfFreqData = NULL;
		}
		m_bReady = false;
	}

	void CAkFreqWindow::CartToPol( )
	{
		CartToPol( m_pfFreqData);
	}

	void CAkFreqWindow::PolToCart( )
	{
		PolToCart( (AkPolar*)m_pfFreqData );
	}

	void CAkFreqWindow::Compute( AkReal32 * in_pfTimeDomainWindow, AkUInt32 in_uNumFrames, ak_fftr_state * in_pFFTState )
	{
		Compute( in_pfTimeDomainWindow, in_uNumFrames, in_pFFTState, m_pfFreqData );
	}

	void CAkFreqWindow::ConvertToTimeDomain( AkReal32 * out_pfTimeDomainBuffer, AkUInt32 in_uNumFrames, ak_fftr_state * in_pIFFTState  )
	{
		ConvertToTimeDomain( out_pfTimeDomainBuffer, in_uNumFrames, in_pIFFTState, m_pfFreqData );
	}

	void CAkFreqWindow::ComputeVocoderSpectrum( 
		AkPolar * in_pPreviousFrame, 
		AkPolar * in_pNextFrame,
		PhaseProcessingType * io_pPreviousSynthPhases,
		AkUInt32 in_uHopSize,
		AkReal32 in_fInterpLoc,
		bool in_bInitPhases )
	{
		ComputeVocoderSpectrum( 
			in_pPreviousFrame,
			in_pNextFrame,
			io_pPreviousSynthPhases,
			in_uHopSize,
			in_fInterpLoc,
			in_bInitPhases,
			(AkPolar *)m_pfFreqData ); 
	}


/////////////////////////////////////// LOW-LEVEL / PIC interface implementation //////////////////////////////////

// Cartesian to polar conversion
#define MAGNITUDE(Cmpx)	( AkSqrtEstimate( (Cmpx).r * (Cmpx).r + (Cmpx).i * (Cmpx).i ) )
#define PHASE(Cmpx)		( AkMath::FastAtan2f((Cmpx).i, (Cmpx).r) )
// Polar to cartesian conversion
#define	REAL(Pol)		( Pol.fMag * AkMath::FastCos( Pol.fPhase ) )
#define	IMAG(Pol)		( Pol.fMag * AkMath::FastSin( Pol.fPhase ) )

#ifdef AKSIMD_V4F32_SUPPORTED

void PolarToCart(AkReal32 * AK_RESTRICT in_pPol, AkUInt32 in_uPoints)
{
	//We can compute the sin and cos at the same time
	//Compute the offset needed for the cosinus.  If you compare with FastCos, the constants have been combined.
	const AKSIMD_DECLARE_V4F32( offsetNoWrap, PI/2, 0, PI/2, 0 );		// cos = sin(x+pi/2).   Offset for the sin part is 0
	const AKSIMD_DECLARE_V4F32( offsetWrap, PI/2-2*PI, 0, PI/2-2*PI, 0 );		// Wrap: cos(x) = cos(x - 2 pi).  Offset for the sin part is 0
	const AKSIMD_DECLARE_V4F32( vHalfPI, PI/2, 0, PI/2, 0 );
	const AKSIMD_V4F32 B = AKSIMD_SET_V4F32(4/PI);
	const AKSIMD_V4F32 C = AKSIMD_SET_V4F32(-4/(PI*PI));
	const AKSIMD_V4F32 P = AKSIMD_SET_V4F32(0.225f);
	
	const AkReal32* AK_RESTRICT pEnd = in_pPol + in_uPoints*2;
	while(in_pPol < pEnd)
	{
		AKSIMD_V4F32 pol = AKSIMD_LOAD_V4F32(in_pPol);
		AKSIMD_V4F32 angle = AKSIMD_DUP_ODD(pol);	 //Duplicate the phase.  One for cos, one for sin.  
		angle = AKSIMD_NEG_V4F32(angle);	//Change the sign of the phase too
		AKSIMD_V4F32 mag = AKSIMD_DUP_EVEN(pol);	 //Duplicate the magnitude, we'll multiply both cos and sin.

		// (cond1 >= cond2) ? b : a.  So: if (x > PI/2), offset by
		AKSIMD_V4F32 offset = AKSIMD_SEL_GTEZ_V4F32(AKSIMD_SUB_V4F32(angle, vHalfPI), offsetWrap, offsetNoWrap);
		angle = AKSIMD_ADD_V4F32(angle, offset);

		//Now the angle is offset for the cos, we can compute the sin for both
		//float y = B * x + C * x * fabs(x); //float y = X*(B+C*fabs(x));
		AKSIMD_V4F32 y = AKSIMD_ABS_V4F32(angle);
		y = AKSIMD_MADD_V4F32(y, C, B);
		y = AKSIMD_MUL_V4F32(y, angle);

		//	return P * (y * fabs(y) - y) + y; 
		AKSIMD_V4F32 sine = AKSIMD_ABS_V4F32(y);
		sine = AKSIMD_MSUB_V4F32(y, sine, y);
		sine = AKSIMD_MADD_V4F32(sine, P, y);

		//Finally, scale by the magnitude
		AKSIMD_STORE_V4F32(in_pPol, AKSIMD_MUL_V4F32(sine, mag));

		in_pPol += 4;
	}
}

#if defined AK_CPU_X86 || defined AK_CPU_X86_64
#define LOAD_X_Y \
	AKSIMD_V4F32 first = AKSIMD_LOAD_V4F32(in_pCart); \
	AKSIMD_V4F32 second = AKSIMD_LOAD_V4F32(in_pCart+4); \
	AKSIMD_V4F32 x = AKSIMD_SHUFFLE_V4F32(first, second, AKSIMD_SHUFFLE(2,0,2,0)); \
	AKSIMD_V4F32 y = AKSIMD_SHUFFLE_V4F32(first, second, AKSIMD_SHUFFLE(3,1,3,1));

#define STORE_MAG_PHASE \
	first = AKSIMD_SHUFFLE_V4F32(mag, phase, AKSIMD_SHUFFLE(1,0,1,0));	\
	first = AKSIMD_SHUFFLE_V4F32(first, first, AKSIMD_SHUFFLE(3,1,2,0)); \
	second = AKSIMD_SHUFFLE_V4F32(mag, phase, AKSIMD_SHUFFLE(3,2,3,2)); \
	second = AKSIMD_SHUFFLE_V4F32(second, second, AKSIMD_SHUFFLE(3,1,2,0)); \
	AKSIMD_STORE_V4F32(in_pCart, first);	\
	in_pCart += 4;	\
	AKSIMD_STORE_V4F32(in_pCart, second);	\
	in_pCart += 4;
	
#elif defined AK_CPU_ARM_NEON
#define x xy.val[0]
#define y xy.val[1]
#define LOAD_X_Y \
	AKSIMD_V4F32 first = AKSIMD_LOAD_V4F32(in_pCart); \
	AKSIMD_V4F32 second = AKSIMD_LOAD_V4F32(in_pCart+4); \
	float32x4x2_t xy = vuzpq_f32(first, second);
	
#define STORE_MAG_PHASE \
	xy = vzipq_f32(mag, phase); \
	AKSIMD_STORE_V4F32(in_pCart, x);	\
	in_pCart += 4;	\
	AKSIMD_STORE_V4F32(in_pCart, y);	\
	in_pCart += 4;
#endif

void PairedCartToPol(AkReal32 * AK_RESTRICT in_pCart, AkUInt32 in_uPoints)
{
	const AKSIMD_V4F32 vNeg = AKSIMD_SET_V4F32(-1.0f);
	const AKSIMD_V4F32 vOne = AKSIMD_SET_V4F32(1.0f);
	const AKSIMD_V4F32 vZero = AKSIMD_SET_V4F32(0.0f);
	const AKSIMD_V4F32 vK = AKSIMD_SET_V4F32(0.28f);
	const AKSIMD_V4F32 vKRepro = AKSIMD_SET_V4F32(1.f/0.28f);
	const AKSIMD_V4F32 vHalfPI = AKSIMD_SET_V4F32(PI/2);
	const AKSIMD_V4F32 vPI = AKSIMD_SET_V4F32(PI);
	const AKSIMD_V4F32 vEpsilon = AKSIMD_SET_V4F32(1e-18f);

	const AkReal32* AK_RESTRICT pEnd = in_pCart + in_uPoints*2;
	while(in_pCart < pEnd)
	{
		//Pick 2 complex and transform into 2 polar
		//Get the X and Y in separate variables
		LOAD_X_Y;

		//Compute the angle using atan2
		AKSIMD_V4F32 atan;
		{
			//Ensure x is not zero a == 0 ? b : c.			
			x = AKSIMD_VSEL_V4F32(x, vEpsilon, AKSIMD_GTEQ_V4F32(vEpsilon, AKSIMD_ABS_V4F32(x)));

			AKSIMD_V4F32 z = AKSIMD_DIV_V4F32(y, x);
			AKSIMD_V4F32 absz = AKSIMD_ABS_V4F32(z);
			AKSIMD_V4COND zcond = AKSIMD_GTEQ_V4F32(vOne, absz);

			//The approximation is done in 2 segments of the form: offset + z/a*(z*z + b);

			//if ( fabsf( z ) < 1.0f ) then use .28 for the a coef
			AKSIMD_V4F32 a = AKSIMD_VSEL_V4F32(vNeg, vK, zcond);

			//if ( fabsf( z ) < 1.0f ) then use 1 for the b factor, else use 0.28
			AKSIMD_V4F32 b = AKSIMD_VSEL_V4F32(vK, vKRepro, zcond);

			atan = AKSIMD_MADD_V4F32(z, z, b);
			atan = AKSIMD_MUL_V4F32(atan, a);
			atan = AKSIMD_VSEL_V4F32(atan, vEpsilon, AKSIMD_GTEQ_V4F32(vEpsilon, AKSIMD_ABS_V4F32(atan)));
			atan = AKSIMD_DIV_V4F32(z, atan);

			//Adjust for quadrant
			//	zcond	x<0		y<0	 offset
			//	1		0		0	 0			
			//	1		0		1	 0			
			//	1		1		0	 +PI		
			//	1		1		1	 -PI		
			//	0		0		0	 +PI/2		
			//	0		0		1	 -PI/2		
			//	0		1		0	 +PI/2		
			//	0		1		1	 -PI/2		

			AKSIMD_V4F32 offsetByX = AKSIMD_SEL_GTEZ_V4F32(x, vZero, vPI);
			AKSIMD_V4F32 offset = AKSIMD_VSEL_V4F32(vHalfPI, offsetByX, zcond);
			AKSIMD_V4F32 sign = AKSIMD_SEL_GTEZ_V4F32(y, vOne, vNeg);

			//Apply computed offset.  
			atan = AKSIMD_MADD_V4F32(offset, sign, atan);
		}

		AKSIMD_V4F32 phase = AKSIMD_NEG_V4F32(atan);
		AKSIMD_V4F32 mag = AKSIMD_SQRT_V4F32(AKSIMD_MADD_V4F32(y, y, AKSIMD_MUL_V4F32(x,x)));

		//Re-pack the phase and mag for each complex
		STORE_MAG_PHASE;				
	}
}

#elif defined AKSIMD_V2F32_SUPPORTED
void PolarToCart(AkReal32 * AK_RESTRICT in_pPol, AkUInt32 in_uPoints)
{
	//We can compute the sin and cos at the same time
	//This is the same code as AKSIMD_SIN_V2F32 above, with the offset added on the cos side alone.
	
	//Compute the offset needed for the cosinus.  If you compare with FastCos, the constants have been combined.
	const AKSIMD_V2F32 offsetNoWrap = {PI/2, 0};		// cos = sin(x+pi/2).   Offset for the sin part is 0
	const AKSIMD_V2F32 offsetWrap = {PI/2-2*PI,0};		// Wrap: cos(x) = cos(x - 2 pi).  Offset for the sin part is 0
	const AKSIMD_V2F32 vHalfPI = {PI/2, 0};
	const AKSIMD_V2F32 B = {4/PI, 4/PI};
	const AKSIMD_V2F32 C = {-4/(PI*PI), -4/(PI*PI)} ;
	const AKSIMD_V2F32 P = {0.225, 0.225};

	const AkReal32* AK_RESTRICT pEnd = in_pPol + in_uPoints*2;
	while(in_pPol < pEnd)
	{
		AKSIMD_V2F32 pol = AKSIMD_LOAD_V2F32_OFFSET(in_pPol, 0);
		AKSIMD_V2F32 angle = AKSIMD_NEG_V2F32(AKSIMD_UNPACKHI_V2F32(pol, pol));	//Duplicate the phase.  One for cos, one for sin.  Change the sign of the phase too
		AKSIMD_V2F32 mag = AKSIMD_UNPACKLO_V2F32(pol, pol);

		// (cond1 >= cond2) ? b : a.  So: if (x > PI/2), offset by
		AKSIMD_V2F32 offset = AKSIMD_SEL_V2F32(AKSIMD_SUB_V2F32(angle, vHalfPI), offsetWrap, offsetNoWrap);
		angle = AKSIMD_ADD_V2F32(angle, offset);

		//Now the angle is offset for the cos, we can compute the sin for both
		//float y = B * x + C * x * fabs(x); //float y = X*(B+C*fabs(x));
		AKSIMD_V2F32 y = AKSIMD_ABS_V2F32(angle);
		y = AKSIMD_MADD_V2F32(y, C, B);
		y = AKSIMD_MUL_V2F32(y, angle);

		//	return P * (y * fabs(y) - y) + y; 
		AKSIMD_V2F32 sine = AKSIMD_ABS_V2F32(y);
		sine = AKSIMD_MSUB_V2F32(y, sine, y);
		sine = AKSIMD_MADD_V2F32(sine, P, y);

		//Finally, scale by the magnitude
		AKSIMD_STORE_V2F32_OFFSET(in_pPol, 0, AKSIMD_MUL_V2F32(sine, mag));

		in_pPol += 2;
	}
}

	/*
	Based on the FastAtan2F found in AkMath
	if ( x != 0.0f )
	{
		float atan;
		float z = y/x;
		if ( fabsf( z ) < 1.0f )
		{
			//atan = z/(0.28f*z*z + 1.0f);
			atan = z/0.28(z*z + 1/0.28);
			if ( x < 0.0f )
			{
				if ( y < 0.0f ) 
					return atan - PI_FLOAT;
				else
					return atan + PI_FLOAT;
			}
		}
		else
		{
			atan = - z/(z*z + 0.28f);
			if ( y < 0.0f )
				return atan - PIBY2_FLOAT;
			else
				return atan + PIBY2_FLOAT;
		}
		return atan;
	}*/


void PairedCartToPol(AkReal32 * AK_RESTRICT in_pCart, AkUInt32 in_uPoints)
{
	const AKSIMD_V2F32 vNeg = AKSIMD_SET_V2F32(-1.0f);
	const AKSIMD_V2F32 vOne = AKSIMD_SET_V2F32(1.0f);
	const AKSIMD_V2F32 vZero = AKSIMD_SET_V2F32(0.0f);
	const AKSIMD_V2F32 vK = AKSIMD_SET_V2F32(0.28f);
	const AKSIMD_V2F32 vKRepro = AKSIMD_SET_V2F32(1.f/0.28f);
	const AKSIMD_V2F32 vHalfPI = AKSIMD_SET_V2F32(PI/2);
	const AKSIMD_V2F32 vPI = AKSIMD_SET_V2F32(PI);
	const AKSIMD_V2F32 vEpsilon = AKSIMD_SET_V2F32(1e-20f);

	const AkReal32* AK_RESTRICT pEnd = in_pCart + in_uPoints*2;
	while(in_pCart < pEnd)
	{
		//Pick 2 complex and transform into 2 polar
		AKSIMD_V2F32 first = AKSIMD_LOAD_V2F32_OFFSET(in_pCart, 0);
		AKSIMD_V2F32 second = AKSIMD_LOAD_V2F32_OFFSET(in_pCart, sizeof(AKSIMD_V2F32));
		
		//Get the X and Y in separate variables
		AKSIMD_V2F32 x = AKSIMD_UNPACKLO_V2F32(first, second);
		AKSIMD_V2F32 y = AKSIMD_UNPACKHI_V2F32(first, second);

		//Compute the angle using atan2
		AKSIMD_V2F32 atan;
		{
			//Ensure x is not zero a == 0 ? b : c.
			//(SEL means a >= 0 ? b : c.  Therefore, if the -abs(x) >= 0, it means it is actually = 0.  
			//This is for WiiU.  Other platforms, might have something more intelligent to do what we want
			x = AKSIMD_SEL_V2F32(AKSIMD_NEG_V2F32(AKSIMD_ABS_V2F32(x)), vEpsilon, x);

			AKSIMD_V2F32 z = AKSIMD_DIV_V2F32(y, x);
			AKSIMD_V2F32 absz = AKSIMD_ABS_V2F32(z);
			AKSIMD_V2F32 zcond = AKSIMD_SUB_V2F32(vOne, absz);

			//The approximation is done in 2 segments of the form: offset + z/a*(z*z + b);

			//if ( fabsf( z ) < 1.0f ) then use .28 for the a coef
			AKSIMD_V2F32 a = AKSIMD_SEL_V2F32(zcond, vK, vNeg);

			//if ( fabsf( z ) < 1.0f ) then use 1 for the b factor, else use 0.28
			AKSIMD_V2F32 b = AKSIMD_SEL_V2F32(zcond, vKRepro, vK);

			atan = AKSIMD_MADD_V2F32(z, z, b);
			atan = AKSIMD_MUL_V2F32(atan, a);
			atan = AKSIMD_DIV_V2F32(z, atan);

			//Adjust for quadrant
			//	zcond	x<0		y<0	 offset
			//	1		0		0	 0			
			//	1		0		1	 0			
			//	1		1		0	 +PI		
			//	1		1		1	 -PI		
			//	0		0		0	 +PI/2		
			//	0		0		1	 -PI/2		
			//	0		1		0	 +PI/2		
			//	0		1		1	 -PI/2		

			AKSIMD_V2F32 offsetByX = AKSIMD_SEL_V2F32(x, vZero, vPI);
			AKSIMD_V2F32 offset = AKSIMD_SEL_V2F32(zcond, offsetByX, vHalfPI);
			AKSIMD_V2F32 sign = AKSIMD_SEL_V2F32(y, vOne, vNeg);

			//Apply computed offset.  
			atan = AKSIMD_MADD_V2F32(offset, sign, atan);
		}
		
		AKSIMD_V2F32 phase = AKSIMD_NEG_V2F32(atan);
		AKSIMD_V2F32 mag = AKSIMD_SQRT_V2F32(AKSIMD_MADD_V2F32(y, y, AKSIMD_MUL_V2F32(x,x)));

		//Re-pack the phase and mag for each complex
		first = AKSIMD_UNPACKLO_V2F32(mag, phase);
		second = AKSIMD_UNPACKHI_V2F32(mag, phase);

		AKSIMD_STORE_V2F32_OFFSET(in_pCart, 0, first);
		AKSIMD_STORE_V2F32_OFFSET(in_pCart, sizeof(AKSIMD_V2F32), second);
		in_pCart += 4;
	}
}
#endif

	void CAkFreqWindow::CartToPol( ak_fft_cpx * io_pfFreqData )
	{
		const AkUInt32 uNumFreqPoints = m_uFFTSize/2;
		ak_fft_cpx * AK_RESTRICT pfFreqData = (ak_fft_cpx * ) io_pfFreqData;
		ak_fft_cpx firstCart = pfFreqData[0];
		ak_fft_cpx lastCart = pfFreqData[uNumFreqPoints];

#if defined AKSIMD_V2F32_SUPPORTED || defined AKSIMD_V4F32_SUPPORTED && !defined _MSC_VER //VC optimization flags completely destroy the maths in PairedCartToPol.  Don't know why.  So no SIMD for PC.
		PairedCartToPol((AkReal32*)io_pfFreqData, uNumFreqPoints);
#else
		for ( AkUInt32 i = 1; i < uNumFreqPoints; i++ )
		{
			ak_fft_cpx Cart = pfFreqData[i];
			((AkPolar*)pfFreqData)[i].fMag = MAGNITUDE( Cart );
			((AkPolar*)pfFreqData)[i].fPhase = -PHASE( Cart );
		}
#endif
		// Handle DC and Nyquist points separately
		((AkPolar*)pfFreqData)[0].fMag = MAGNITUDE( firstCart );
		((AkPolar*)pfFreqData)[0].fPhase = PHASE( firstCart );
		((AkPolar*)pfFreqData)[uNumFreqPoints].fMag = MAGNITUDE( lastCart );
		((AkPolar*)pfFreqData)[uNumFreqPoints].fPhase = PHASE( lastCart );
		m_bPolar = true;
	}
	
	void CAkFreqWindow::PolToCart( AkPolar * io_pfFreqData )
	{
		const AkUInt32 uNumFreqPoints = m_uFFTSize/2;
		AkPolar * AK_RESTRICT pfFreqData = (AkPolar * ) io_pfFreqData;
		AkPolar firstPol = pfFreqData[0];
		AkPolar lastPol = pfFreqData[uNumFreqPoints];

#if defined( AKSIMD_V2F32_SUPPORTED) || defined( AKSIMD_V4F32_SUPPORTED)
		//Do all of the points in SIMD, even the first and last.  Easier to overwrite them later.
		PolarToCart((AkReal32*)io_pfFreqData, uNumFreqPoints);
#else
		for ( AkUInt32 i = 0; i < uNumFreqPoints; i++ )
		{
			AkPolar Pol = pfFreqData[i];
			Pol.fPhase *= -1.f;
			((ak_fft_cpx*)pfFreqData)[i].r = REAL( Pol );
			((ak_fft_cpx*)pfFreqData)[i].i = IMAG( Pol );
		}	
#endif
		((ak_fft_cpx*)pfFreqData)[0].r = REAL( firstPol );
		((ak_fft_cpx*)pfFreqData)[0].i = IMAG( firstPol );
		((ak_fft_cpx*)pfFreqData)[uNumFreqPoints].r = REAL( lastPol );
		((ak_fft_cpx*)pfFreqData)[uNumFreqPoints].i = IMAG( lastPol );
		m_bPolar = false;
	}

	void CAkFreqWindow::Compute( 
		AkReal32 * in_pfTimeDomainWindow, 
		AkUInt32 in_uNumFrames, 
		ak_fftr_state * in_pFFTState, 
		ak_fft_cpx * io_pfFreqData )
	{
		AKASSERT( in_uNumFrames == m_uFFTSize ); // Not yet suporting zero-padding

		// Compute FFT of input buffer and store internally
		RESOLVEUSEALLBUTTERFLIES( ak_fftr(in_pFFTState,in_pfTimeDomainWindow,io_pfFreqData) );

		m_bReady = true;
		m_bPolar = false;
	}

	void CAkFreqWindow::ConvertToTimeDomain( 
		AkReal32 * out_pfTimeDomainBuffer, 
		AkUInt32 in_uNumFrames, 
		ak_fftr_state * in_pIFFTState,
		ak_fft_cpx * io_pfFreqData )
	{
		if ( m_bPolar )
			PolToCart( (AkPolar *)io_pfFreqData );
		AKASSERT( in_uNumFrames == m_uFFTSize ); 

		RESOLVEUSEALLBUTTERFLIES( ak_fftri(in_pIFFTState,io_pfFreqData,out_pfTimeDomainBuffer) ); 
		const AkReal32 fIFFTGain = 1.f/m_uFFTSize;
		AK::DSP::ApplyGain( out_pfTimeDomainBuffer, fIFFTGain, m_uFFTSize );
	}

	void CAkFreqWindow::ComputeVocoderSpectrum( 
		AkPolar * in_pPreviousFrame, 
		AkPolar * in_pNextFrame,
		PhaseProcessingType * io_pPreviousSynthPhases,
		AkUInt32 in_uHopSize,
		AkReal32 in_fInterpLoc,
		bool in_bInitPhases,
		AkPolar * io_pfFreqData )
	{
		const AkUInt32 uFFTSize = m_uFFTSize;
		const AkUInt32 uNumFreqPoints = uFFTSize/2+1;
		AkPolar * AK_RESTRICT pPrevFrame = (AkPolar * ) in_pPreviousFrame;
		AkPolar * AK_RESTRICT pNextFrame = (AkPolar * ) in_pNextFrame;
		AkPolar * AK_RESTRICT pfInternalPolarData = (AkPolar * ) io_pfFreqData;
		PhaseProcessingType * AK_RESTRICT pfPrevSynthPhases = (PhaseProcessingType * )io_pPreviousSynthPhases;

		// Linear interpolation of magnitudes
		for ( AkUInt32 i = 0; i < uNumFreqPoints; i++ )
		{
			pfInternalPolarData[i].fMag = (1.f-in_fInterpLoc)*pPrevFrame[i].fMag + in_fInterpLoc*pNextFrame[i].fMag;
		}

		// Phase vocoder style unwrapping + interpolation
		if ( in_bInitPhases )
		{
			// Initalizes phases to those of the first frame
			for ( AkUInt32 i = 0; i < uNumFreqPoints; i++ )
			{
				pfPrevSynthPhases[i] = (PhaseProcessingType)pPrevFrame[i].fPhase;
			}
		}

		for ( AkUInt32 i = 0; i < uNumFreqPoints; i++ )
		{
			// Compute phase advance deviation compared to phase obtain from bin frequency
			PhaseProcessingType fPhaseAdvance = (PhaseProcessingType)pNextFrame[i].fPhase - (PhaseProcessingType)pPrevFrame[i].fPhase;
			// Output phases with one frame delay
			pfInternalPolarData[i].fPhase = (AkReal32)pfPrevSynthPhases[i];
			// Cumulate phase for next time
			pfPrevSynthPhases[i] += fPhaseAdvance;
			// Wrap in (-pi,pi) range
			AK_FPSetValGTE( pfPrevSynthPhases[i], PI, pfPrevSynthPhases[i], pfPrevSynthPhases[i] - TWOPI );
			AK_FPSetValLT( pfPrevSynthPhases[i], -PI, pfPrevSynthPhases[i], pfPrevSynthPhases[i] + TWOPI );
		}

		m_bPolar = true;
		m_bReady = true;
	}
	
} // BUTTERFLYSET_NAMESPACE

} // namespace DSP
