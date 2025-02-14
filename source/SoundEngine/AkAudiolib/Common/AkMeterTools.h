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
// AkMeterTools.h
//
//////////////////////////////////////////////////////////////////////

#ifndef _METERTOOLS_H_
#define _METERTOOLS_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include "AkMath.h"
#include "AkSettings.h"
#include <AK/SoundEngine/Common/AkFPUtilities.h>
#include <AK/SoundEngine/Common/AkSimd.h>
#include "BiquadFilter.h"
#include "AkDSPUtils.h"
#include "AkSpeakerVolumesEx.h"
#include "AkSpeakerPan.h"
#include "AkMixer.h"
#include "AkOutputMgr.h"
#include <AK/Tools/Common/AkObject.h>

#define AK_TRUE_PEAK_FIR_ORDER			(12)
#define AK_TRUE_PEAK_FIR_ORDER_DIV4		(AK_TRUE_PEAK_FIR_ORDER / 4)	// for SIMD
#define AK_BACK_CHANNELS_LOUDNESS_GAIN	(1.4125375f)	// 1.5 dB (power)
#define AK_BUS_VOLUME_CORRECTION_FACTOR	(1.0009619f)	// 0 dB to lin approx yields ~0.999 instead 1. Fix in order to get compliant values in test conditions.
#ifndef AKSIMD_V2F32_SUPPORTED

inline AkReal32 ComputePeak( void* in_pData, const AkUInt32 in_uMaxFrames )
{
	AKASSERT( in_uMaxFrames % 4 == 0 ); // Just making sure as it is assumed in this function.

	// Init min at max and max at min.
	AKSIMD_F32  fMin = (AkReal32) AK_INT_MAX;
	AKSIMD_F32  fMax = (AkReal32) AK_INT_MIN;
	AKSIMD_V4F32 vMin = AKSIMD_LOAD1_V4F32( fMin ); 
	AKSIMD_V4F32 vMax = AKSIMD_LOAD1_V4F32( fMax );

	// Set the buffer boundaries
	AKSIMD_V4F32 * AK_RESTRICT pfBuf = (AKSIMD_V4F32* ) in_pData;
	AKSIMD_V4F32 * AK_RESTRICT pfEnd = pfBuf + (in_uMaxFrames / 4);

	do
	{
		vMin = AKSIMD_MIN_V4F32( vMin, *pfBuf );
		vMax = AKSIMD_MAX_V4F32( vMax, *pfBuf );

		++pfBuf;
	}while ( pfBuf < pfEnd );

	// Compile results into one value for this channel.
	fMin = AkMath::Min( ((float*)&vMin)[0],	((float*)&vMin)[1] );
	fMax = AkMath::Max( ((float*)&vMax)[0],	((float*)&vMax)[1] );
	fMin = AkMath::Min( fMin,				((float*)&vMin)[2] );
	fMax = AkMath::Max( fMax,				((float*)&vMax)[2] );
	fMin = AkMath::Min( fMin,				((float*)&vMin)[3] );
	fMax = AkMath::Max( fMax,				((float*)&vMax)[3] );

	return AK_FPMax( fabsf( fMin ),  fMax );
}

inline AkReal32 ComputePeakIndex(void* in_pData, const AkUInt32 in_uMaxFrames, AkUInt32& out_uiPeakIndex)
{
	AkReal32 fMax = (AkReal32)AK_INT_MIN;
	AkReal32 fPeakVal = fMax;
	out_uiPeakIndex = 0;
	for (AkUInt32 i = 0; i < in_uMaxFrames; ++i)
	{
		AkReal32 fV = fabsf(((float*)in_pData)[i]);
		if (fV > fMax)
		{
			fMax = fV;
			fPeakVal = ((float*)in_pData)[i]; // Can be negative
			out_uiPeakIndex = i;
		}
	}
	return fPeakVal;
}


inline AkReal32 ComputeRMS( void* in_pData, const AkUInt32 in_uMaxFrames )
{
	AKASSERT( in_uMaxFrames % 4 == 0 ); // Just making sure as it is assumed in this function.

	// Set the buffer boundaries
	AKSIMD_V4F32 * AK_RESTRICT pfBuf = (AKSIMD_V4F32* ) in_pData;
	AKSIMD_V4F32 * AK_RESTRICT pfEnd = pfBuf + (in_uMaxFrames / 4);

	AKSIMD_V4F32 vRMS = AKSIMD_SET_V4F32( 0.f );

	do
	{
		AKSIMD_V4F32 vSquare = AKSIMD_MUL_V4F32( *pfBuf, *pfBuf );
		vRMS = AKSIMD_ADD_V4F32( vRMS, vSquare );
		++pfBuf;
	}
	while ( pfBuf < pfEnd );

	vRMS = AKSIMD_HORIZONTALADD_V4F32( vRMS );
	AkReal32 fRMS = AKSIMD_GETELEMENT_V4F32( vRMS, 0 );

	return sqrtf( fRMS / in_uMaxFrames );
}

static const AKSIMD_DECLARE_V4F32_TYPE s_vCoeffPhase[AK_TRUE_PEAK_FIR_ORDER] = 
{ 
	{ 0.0017089843750f, -0.0291748046875f, -0.0189208984375f, -0.0083007812500f },
	{ 0.0109863281250f, 0.0292968750000f, 0.0330810546875f, 0.0148925781250f },
	{ -0.0196533203125f, -0.0517578125000f, -0.0582275390625f, -0.0266113281250f },
	{ 0.0332031250000f, 0.0891113281250f, 0.1015625000000f, 0.0476074218750f }, 
	{ -0.0594482421875f, -0.1665039062500f, -0.2003173828125f, -0.1022949218750f }, 
	{ 0.1373291015625f, 0.4650878906250f, 0.7797851562500f, 0.9721679687500f },
	{ 0.9721679687500f, 0.7797851562500f, 0.4650878906250f, 0.1373291015625f },
	{ -0.1022949218750f, -0.2003173828125f, -0.1665039062500f, -0.0594482421875f },
	{ 0.0476074218750f, 0.1015625000000f, 0.0891113281250f, 0.0332031250000f },
	{ -0.0266113281250f, -0.0582275390625f, -0.0517578125000f, -0.0196533203125f },
	{ 0.0148925781250f, 0.0330810546875f, 0.0292968750000f, 0.0109863281250f },
	{ -0.0083007812500f, -0.0189208984375f, -0.0291748046875f, 0.0017089843750f }
};

// Performs convolution of pvIn with all four kernels at the same time, and returns the min and max values.
// Convolution is causal, as values before pIn are not accessed, but FIR_ORDER-1 samples are read _after_ pEnd.
inline void ConvolvePolyPhase(
	AKSIMD_V4F32 * pvIn,
	AKSIMD_V4F32 * pvEnd,
	AKSIMD_V4F32 & io_vMin,
	AKSIMD_V4F32 & io_vMax
	)
{
	while ( pvIn < pvEnd )
	{
		AKSIMD_V4F32 vOut0 = AKSIMD_SET_V4F32( 0.f );
		AKSIMD_V4F32 vOut1 = AKSIMD_SET_V4F32( 0.f );
		AKSIMD_V4F32 vOut2 = AKSIMD_SET_V4F32( 0.f );
		AKSIMD_V4F32 vOut3 = AKSIMD_SET_V4F32( 0.f );

		AKSIMD_V4F32 * pvInConv = pvIn;

		AKSIMD_V4F32 vNextFourInputs = *pvInConv++;
		AKSIMD_V4F32 vIn = AKSIMD_SPLAT_V4F32( vNextFourInputs, 0 );
		vOut0 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 0 ], vOut0 );
		
		vIn = AKSIMD_SPLAT_V4F32( vNextFourInputs, 1 );
		vOut1 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 0 ], vOut1 );
		vOut0 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 1 ], vOut0 );			
		
		vIn = AKSIMD_SPLAT_V4F32( vNextFourInputs, 2 );
		vOut2 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 0 ], vOut2 );
		vOut1 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 1 ], vOut1 );
		vOut0 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 2 ], vOut0 );
		
		vIn = AKSIMD_SPLAT_V4F32( vNextFourInputs, 3 );
		vOut3 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 0 ], vOut3 );
		vOut2 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 1 ], vOut2 );
		vOut1 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 2 ], vOut1 );
		vOut0 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 3 ], vOut0 );

		vNextFourInputs = *pvInConv++;

		vIn = AKSIMD_SPLAT_V4F32( vNextFourInputs, 0 );
		vOut3 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 1 ], vOut3 );
		vOut2 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 2 ], vOut2 );
		vOut1 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 3 ], vOut1 );
		vOut0 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 4 ], vOut0 );

		vIn = AKSIMD_SPLAT_V4F32( vNextFourInputs, 1 );
		vOut3 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 2 ], vOut3 );
		vOut2 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 3 ], vOut2 );
		vOut1 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 4 ], vOut1 );
		vOut0 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 5 ], vOut0 );

		vIn = AKSIMD_SPLAT_V4F32( vNextFourInputs, 2 );
		vOut3 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 3 ], vOut3 );
		vOut2 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 4 ], vOut2 );
		vOut1 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 5 ], vOut1 );
		vOut0 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 6 ], vOut0 );

		vIn = AKSIMD_SPLAT_V4F32( vNextFourInputs, 3 );
		vOut3 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 4 ], vOut3 );
		vOut2 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 5 ], vOut2 );
		vOut1 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 6 ], vOut1 );
		vOut0 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 7 ], vOut0 );

		vNextFourInputs = *pvInConv++;

		vIn = AKSIMD_SPLAT_V4F32( vNextFourInputs, 0 );
		vOut3 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 5 ], vOut3 );
		vOut2 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 6 ], vOut2 );
		vOut1 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 7 ], vOut1 );
		vOut0 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 8 ], vOut0 );

		vIn = AKSIMD_SPLAT_V4F32( vNextFourInputs, 1 );
		vOut3 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 6 ], vOut3 );
		vOut2 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 7 ], vOut2 );
		vOut1 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 8 ], vOut1 );
		vOut0 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 9 ], vOut0 );

		vIn = AKSIMD_SPLAT_V4F32( vNextFourInputs, 2 );
		vOut3 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 7 ], vOut3 );
		vOut2 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 8 ], vOut2 );
		vOut1 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 9 ], vOut1 );
		vOut0 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 10 ], vOut0 );

		vIn = AKSIMD_SPLAT_V4F32( vNextFourInputs, 3 );
		vOut3 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 8 ], vOut3 );
		vOut2 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 9 ], vOut2 );
		vOut1 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 10 ], vOut1 );
		vOut0 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 11 ], vOut0 );

		vNextFourInputs = *pvInConv++;

		vIn = AKSIMD_SPLAT_V4F32( vNextFourInputs, 0 );
		vOut3 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 9 ], vOut3 );
		vOut2 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 10 ], vOut2 );
		vOut1 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 11 ], vOut1 );
		
		vIn = AKSIMD_SPLAT_V4F32( vNextFourInputs, 1 );
		vOut3 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 10 ], vOut3 );
		vOut2 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 11 ], vOut2 );
		
		vIn = AKSIMD_SPLAT_V4F32( vNextFourInputs, 2 );
		vOut3 = AKSIMD_MADD_V4F32( vIn, s_vCoeffPhase[ 11 ], vOut3 );


		io_vMin = AKSIMD_MIN_V4F32( io_vMin, vOut0 );
		io_vMin = AKSIMD_MIN_V4F32( io_vMin, vOut1 );
		io_vMin = AKSIMD_MIN_V4F32( io_vMin, vOut2 );
		io_vMin = AKSIMD_MIN_V4F32( io_vMin, vOut3 );
		io_vMax = AKSIMD_MAX_V4F32( io_vMax, vOut0 );
		io_vMax = AKSIMD_MAX_V4F32( io_vMax, vOut1 );
		io_vMax = AKSIMD_MAX_V4F32( io_vMax, vOut2 );
		io_vMax = AKSIMD_MAX_V4F32( io_vMax, vOut3 );

		++pvIn;
	}
}

inline AkReal32 ComputeTruePeak( void* in_pData, AkReal32 * in_pPrevFrame, const AkUInt32 in_uMaxFrames )
{
	AKASSERT( in_uMaxFrames % 4 == 0 ); // Just making sure as it is assumed in this function.
	AKASSERT( AK_TRUE_PEAK_FIR_ORDER % 4 == 0 ); // Just making sure as it is assumed in this function.

	// True peak measurement: oversample by 4 and filter at pi/4. We use a 4-phase 12-order FIR filter
	// in order to perform this in one step. Each phase filter is computed as a direct, causal convolution.
	
	// Init min at max and max at min.
	AKSIMD_F32 fMin = (AKSIMD_F32) AK_INT_MAX;
	AKSIMD_F32 fMax = (AKSIMD_F32) AK_INT_MIN;
	AKSIMD_V4F32 vMin = AKSIMD_LOAD1_V4F32( fMin ); 
	AKSIMD_V4F32 vMax = AKSIMD_LOAD1_V4F32( fMax );
	
	// Perform convolution over end of previous frame and beginning of current.
	AK_ALIGN_SIMD( AKSIMD_V4F32 pTemp[ 2 * AK_TRUE_PEAK_FIR_ORDER_DIV4 ] );
	memcpy( pTemp, in_pPrevFrame, sizeof(AKSIMD_V4F32) * AK_TRUE_PEAK_FIR_ORDER_DIV4 );
	// Note: copying one more sample than we need.
	memcpy( pTemp + AK_TRUE_PEAK_FIR_ORDER_DIV4, in_pData, sizeof(AKSIMD_V4F32) * ( AK_TRUE_PEAK_FIR_ORDER_DIV4 ) );

	AKSIMD_V4F32 * pvIn = (AKSIMD_V4F32 * ) pTemp;
	AKSIMD_V4F32 * pvEnd = pvIn + AK_TRUE_PEAK_FIR_ORDER_DIV4;

	ConvolvePolyPhase( pvIn, pvEnd, vMin, vMax );
	
	// Perform convolution on new buffer.
	pvIn = (AKSIMD_V4F32 *) in_pData;
	pvEnd = pvIn + ( in_uMaxFrames / 4 ) - AK_TRUE_PEAK_FIR_ORDER_DIV4;
	
	ConvolvePolyPhase( pvIn, pvEnd, vMin, vMax );

	// Store end of frame for next pass.
	memcpy( in_pPrevFrame, pvEnd, sizeof(AKSIMD_V4F32) * AK_TRUE_PEAK_FIR_ORDER_DIV4 );

	// Collapse min and max of each phase.
	fMin = AkMath::Min( ((float*)&vMin)[0],	((float*)&vMin)[1] );
	fMax = AkMath::Max( ((float*)&vMax)[0],	((float*)&vMax)[1] );
	fMin = AkMath::Min( fMin,				((float*)&vMin)[2] );
	fMax = AkMath::Max( fMax,				((float*)&vMax)[2] );
	fMin = AkMath::Min( fMin,				((float*)&vMin)[3] );
	fMax = AkMath::Max( fMax,				((float*)&vMax)[3] );

	return AK_FPMax( fabsf( fMin ), fMax );
}

#else 

inline AkReal32 ComputePeak( void* in_pData, const AkUInt32 in_uMaxFrames )
{
	AKASSERT( in_uMaxFrames % 2 == 0 ); // Just making sure as it is assumed in this function.

	AKSIMD_F32  fMax = (AkReal32) AK_INT_MIN;
	AKSIMD_V2F32 vMax = AKSIMD_SET_V2F32( fMax );

	// Set the buffer boundaries
	AKSIMD_V2F32 * AK_RESTRICT pfBuf = (AKSIMD_V2F32* ) in_pData;
	AKSIMD_V2F32 * AK_RESTRICT pfEnd = pfBuf + (in_uMaxFrames / 2);

	do
	{
		vMax = AKSIMD_MAX_V2F32( vMax, AKSIMD_ABS_V2F32(*pfBuf) );
		++pfBuf;
	}while ( pfBuf < pfEnd );

	// Compile results into one value for this channel.
	return AkMath::Max( ((float*)&vMax)[0],	((float*)&vMax)[1] );
}

inline AkReal32 ComputeRMS( void* in_pData, const AkUInt32 in_uMaxFrames )
{
	AKASSERT( in_uMaxFrames % 2 == 0 ); // Just making sure as it is assumed in this function.

	// Set the buffer boundaries
	AKSIMD_V2F32 * AK_RESTRICT pfBuf = (AKSIMD_V2F32* ) in_pData;
	AKSIMD_V2F32 * AK_RESTRICT pfEnd = pfBuf + (in_uMaxFrames / 2);

	AKSIMD_V2F32 vRMS = AKSIMD_SET_V2F32( 0.f );

	do
	{
		AKSIMD_V2F32 vIn = *pfBuf;
		AKSIMD_V2F32 vSquare = AKSIMD_MUL_V2F32( vIn, vIn );
		vRMS = AKSIMD_ADD_V2F32( vRMS, vSquare );
		++pfBuf;
	}
	while ( pfBuf < pfEnd );

	// Horizontal add.
	AKSIMD_V2F32 vRMS10 = AKSIMD_SWAP_V2F32( vRMS );
	vRMS = AKSIMD_ADD_V2F32( vRMS, vRMS10 );

	//sqrtf( fRMS / in_uMaxFrames );
	const AKSIMD_V2F32 vOneOverNumFrames = { ( 1.f / (AkReal32)in_uMaxFrames ), ( 1.f / (AkReal32)in_uMaxFrames ) };
	vRMS = AKSIMD_MUL_V2F32( vRMS, vOneOverNumFrames );
	vRMS = AKSIMD_SQRT_V2F32( vRMS );
	return vRMS[0];
}

static AkReal32 s_fCoeffPhase[4][AK_TRUE_PEAK_FIR_ORDER] = 
{ 
	{ 0.0017089843750f, 0.0109863281250f, -0.0196533203125f, 0.0332031250000f, -0.0594482421875f, 0.1373291015625f, 0.9721679687500f, -0.1022949218750f, 0.0476074218750f, -0.0266113281250f, 0.0148925781250f, -0.0083007812500f },
	{ -0.0291748046875f, 0.0292968750000f, -0.0517578125000f, 0.0891113281250f, -0.1665039062500f, 0.4650878906250f, 0.7797851562500f, -0.2003173828125f, 0.1015625000000f, -0.0582275390625f, 0.0330810546875f, -0.0189208984375f },
	{ -0.0189208984375f, 0.0330810546875f, -0.0582275390625f, 0.1015625000000f, -0.2003173828125f, 0.7797851562500f, 0.4650878906250f, -0.1665039062500f, 0.0891113281250f, -0.0517578125000f, 0.0292968750000f, -0.0291748046875f },
	{ -0.0083007812500f, 0.0148925781250f, -0.0266113281250f, 0.0476074218750f, -0.1022949218750f, 0.9721679687500f, 0.1373291015625f, -0.0594482421875f, 0.0332031250000f, -0.0196533203125f, 0.0109863281250f, 0.0017089843750f }
};

inline void ConvolvePolyPhase(
	AkReal32 * pfIn,
	AkReal32 * pfEnd,
	AkReal32 & io_fMin,
	AkReal32 & io_fMax
	)
{
	// Scalar implementation.
	/// TODO SIMD_V2F32
	AkReal32 fMin[4];
	fMin[0] = io_fMin;
	fMin[1] = io_fMin;
	fMin[2] = io_fMin;
	fMin[3] = io_fMin;
	AkReal32 fMax[4];
	fMax[0] = io_fMax;
	fMax[1] = io_fMax;
	fMax[2] = io_fMax;
	fMax[3] = io_fMax;

	while ( pfIn < pfEnd )
	{		
		AkReal32 fOut0 = 0.f;
		AkReal32 fOut1 = 0.f;
		AkReal32 fOut2 = 0.f;
		AkReal32 fOut3 = 0.f;
		
		AkUInt32 uTap = 0;
		do
		{
			fOut0 += pfIn[uTap] * s_fCoeffPhase[0][uTap];
			fOut1 += pfIn[uTap] * s_fCoeffPhase[1][uTap];
			fOut2 += pfIn[uTap] * s_fCoeffPhase[2][uTap];
			fOut3 += pfIn[uTap] * s_fCoeffPhase[3][uTap];
			
			++uTap;
		}
		while ( uTap < AK_TRUE_PEAK_FIR_ORDER );

		fMin[0] = AkMath::Min( fMin[0], fOut0 );
		fMin[1] = AkMath::Min( fMin[1], fOut1 );
		fMin[2] = AkMath::Min( fMin[2], fOut2 );
		fMin[3] = AkMath::Min( fMin[3], fOut3 );
		fMax[0] = AkMath::Max( fMax[0], fOut0 );
		fMax[1] = AkMath::Max( fMax[1], fOut1 );
		fMax[2] = AkMath::Max( fMax[2], fOut2 );
		fMax[3] = AkMath::Max( fMax[3], fOut3 );

		++pfIn;
	}

	// Collapse results
	io_fMin = AkMath::Min( fMin[0], fMin[1] );
	io_fMin = AkMath::Min( io_fMin, fMin[2] );
	io_fMin = AkMath::Min( io_fMin, fMin[3] );
	io_fMax = AkMath::Max( fMax[0], fMax[1] );
	io_fMax = AkMath::Max( io_fMax, fMax[2] );
	io_fMax = AkMath::Max( io_fMax, fMax[3] );
}

// Scalar implementation.
/// TODO SIMD_V2F32
inline AkReal32 ComputeTruePeak( void* in_pData, AkReal32 * in_pPrevFrame, const AkUInt32 in_uMaxFrames )
{
	AKASSERT( AK_TRUE_PEAK_FIR_ORDER % 4 == 0 ); // Just making sure as it is assumed in this function.

	// True peak measurement: oversample by 4 and filter at pi/4. We use a 4-phase 12-order FIR filter
	// in order to perform this in one step. Each phase filter is computed as a direct, causal convolution.
	
	// Init min at max and max at min.
	AkReal32 fMin = (AKSIMD_F32) AK_INT_MAX;
	AkReal32 fMax = (AKSIMD_F32) AK_INT_MIN;
	
	// Perform convolution over end of previous frame and beginning of current.
	AkReal32 pTemp[ 2 * AK_TRUE_PEAK_FIR_ORDER - 1 ];
	memcpy( pTemp, in_pPrevFrame, sizeof(AkReal32) * AK_TRUE_PEAK_FIR_ORDER );
	memcpy( pTemp + AK_TRUE_PEAK_FIR_ORDER, in_pData, sizeof(AkReal32) * ( AK_TRUE_PEAK_FIR_ORDER - 1 ) );

	AkReal32 * pfIn = (AkReal32 *) pTemp;
	AkReal32 * pfEnd = pfIn + AK_TRUE_PEAK_FIR_ORDER;

	ConvolvePolyPhase( pfIn, pfEnd, fMin, fMax );
	
	pfIn = (AkReal32 *) in_pData + AK_TRUE_PEAK_FIR_ORDER;
	pfEnd = pfIn + in_uMaxFrames - 2 * AK_TRUE_PEAK_FIR_ORDER;

	ConvolvePolyPhase( pfIn, pfEnd, fMin, fMax );

	// Store end of frame for next pass.
	memcpy( in_pPrevFrame, pfIn, sizeof(AkReal32) * AK_TRUE_PEAK_FIR_ORDER );

	return AK_FPMax( fabs( fMin ), fMax );
}

#endif

// Loudness metering context. Contains filters and power computed in last frame.
class AkMeterCtx : public AK::IAkMetering
{
public:
	
	/// IAkMetering interface.
	virtual AK::SpeakerVolumes::ConstVectorPtr GetPeak() { return GetMeterPeak(); }
	virtual AK::SpeakerVolumes::ConstVectorPtr GetTruePeak() { return GetMeterTruePeak(); }
	virtual AK::SpeakerVolumes::ConstVectorPtr GetRMS() { return GetMeterRMS(); }
	virtual AkReal32 GetKWeightedPower() { return GetMeanPowerLoudness(); }

	inline AkChannelConfig GetChannelConfig() const { return m_channelConfig; }
	inline AkReal32 GetMeanPowerLoudness() const { return m_fMeanPowerK; }
	inline AK::SpeakerVolumes::ConstVectorPtr GetMeterPeak() const { return m_peak; }
	inline AK::SpeakerVolumes::ConstVectorPtr GetMeterRMS() const { return m_rms; }
    inline AK::SpeakerVolumes::ConstVectorPtr GetMeter3DPeaks() const { return m_3DPeaks; }
	inline AK::SpeakerVolumes::ConstVectorPtr GetMeterTruePeak() const { return m_truePeak; }

	inline AkMeteringFlags MeterTypes() const { return m_eMeterTypes; }

protected:

	AkChannelConfig m_channelConfig;

#ifndef AK_VOICE_MAX_NUM_CHANNELS

	// Peak.
	AK::SpeakerVolumes::VectorPtr m_peak;			// Peak of each channel

	// RMS.
	AK::SpeakerVolumes::VectorPtr m_rms;			// Mean power of each channel

	// True peak.
	AK::SpeakerVolumes::VectorPtr m_truePeak;		// True peak of each channel
	AkReal32 *				m_fPrevOverlapSave;		// AK_TRUE_PEAK_FIR_ORDER samples in previous buffer.

    AK::SpeakerVolumes::VectorPtr m_3DPeaks;     // Peak at each point in a sampled sphere.

	// Loudness.
	// K filters.
	DSP::BiquadFilterMonoPerSample *	m_arFilterKPre;		
	DSP::BiquadFilterMonoPerSample *	m_arFilterKRLB;

#else

	// Peak.
	AkReal32 m_peak[AK_VOICE_MAX_NUM_CHANNELS];			// Peak of each channel

	// RMS.
	AkReal32 m_rms[AK_VOICE_MAX_NUM_CHANNELS];			// Mean power of each channel

	// True peak.
	AkReal32 m_truePeak[AK_VOICE_MAX_NUM_CHANNELS];		// True peak of each channel
	AkReal32 m_fPrevOverlapSave[AK_TRUE_PEAK_FIR_ORDER*AK_VOICE_MAX_NUM_CHANNELS];		// AK_TRUE_PEAK_FIR_ORDER samples in previous buffer.

	// Loudness.
	// K filters.
	DSP::BiquadFilterMonoPerSample	m_arFilterKPre[AK_VOICE_MAX_NUM_CHANNELS];		
	DSP::BiquadFilterMonoPerSample	m_arFilterKRLB[AK_VOICE_MAX_NUM_CHANNELS];
	
#endif	// AK_VOICE_MAX_NUM_CHANNELS

	AkReal32				m_fMeanPowerK;		// Mean power of K-filtered signal

	// Meter Type.
	AkMeteringFlags			m_eMeterTypes;

public:
	AkMeterCtx(AkChannelConfig in_channelConfig)
		: m_channelConfig(in_channelConfig)

#ifndef AK_VOICE_MAX_NUM_CHANNELS
		, m_peak( NULL )
		, m_rms( NULL )
		, m_truePeak( NULL )
		, m_fPrevOverlapSave( NULL )
        , m_3DPeaks( NULL )
		, m_arFilterKPre( NULL )
		, m_arFilterKRLB( NULL )
#endif
		, m_fMeanPowerK( 0 )
		, m_eMeterTypes( AK_NoMetering )
	{}

	~AkMeterCtx()
	{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
		AK::SpeakerVolumes::Vector::Free( m_peak );
		AK::SpeakerVolumes::Vector::Free( m_rms );
		AK::SpeakerVolumes::Vector::Free( m_truePeak );
        AK::SpeakerVolumes::Vector::Free( m_3DPeaks );
		if ( m_fPrevOverlapSave )
		{
			AkFalign( AkMemID_Processing, m_fPrevOverlapSave );
		}
		if ( m_arFilterKPre )
		{
			AkFree( AkMemID_Processing, m_arFilterKPre );
		}
		if ( m_arFilterKRLB )
		{
			AkFree( AkMemID_Processing, m_arFilterKRLB );
		}
#endif
	}

	AKRESULT Init( AkUInt32 in_uSampleRate, AkMeteringFlags in_eMeterTypes )
	{
		if ( in_eMeterTypes & AkMonitorData::BusMeterDataType_Peak )
		{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
			m_peak = AK::SpeakerVolumes::Vector::Allocate(m_channelConfig.uNumChannels);
			if ( !m_peak )
				return AK_Fail;
#endif
			AK::SpeakerVolumes::Vector::Zero(m_peak, m_channelConfig.uNumChannels);
		}

		if ( in_eMeterTypes & AkMonitorData::BusMeterDataType_RMS )
		{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
			m_rms = AK::SpeakerVolumes::Vector::Allocate(m_channelConfig.uNumChannels);
			if ( !m_rms )
				return AK_Fail;
#endif
			AK::SpeakerVolumes::Vector::Zero(m_rms, m_channelConfig.uNumChannels);
		}

		if ( in_eMeterTypes & AkMonitorData::BusMeterDataType_KPower )
		{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
			// Special case for ambisonics: just one channel (W) is processed.
			AkUInt32 numFilters = (m_channelConfig.eConfigType == AK_ChannelConfigType_Ambisonic) ? 1 : m_channelConfig.uNumChannels;
			m_arFilterKPre = (DSP::BiquadFilterMonoPerSample*)AkAlloc( AkMemID_Processing, numFilters * sizeof( DSP::BiquadFilterMonoPerSample ) );
			m_arFilterKRLB = (DSP::BiquadFilterMonoPerSample*)AkAlloc( AkMemID_Processing, numFilters * sizeof( DSP::BiquadFilterMonoPerSample ) );
			if ( !m_arFilterKPre || !m_arFilterKRLB )
				return AK_Fail;
#endif

			// Init K filters.
			{
				const AkReal32 fc = 1.5029e3f;
				const AkReal32 Q = 0.71f;
				const AkReal32 V = powf( 10.0f, (4.f/40.f) );
				const AkReal32 w0 = 2 * PI * fc / (AkReal32)in_uSampleRate;
				const AkReal32 alpha = sinf(w0)/(2*Q);
				const AkReal32 fTwoSqrtValpha = 2 * sqrtf(V) * alpha;
				const AkReal32 cosw0 = cosf(w0);

				AkReal32 a0 = (V+1) - (V-1)*cosw0 + fTwoSqrtValpha;
				AkReal32 a1 = (2*((V-1) - (V+1)*cosw0) ) / a0;
				AkReal32 a2 = ( (V+1) - (V-1)*cosw0 - fTwoSqrtValpha ) / a0;
				AkReal32 b0 = ( V*( (V+1) + (V-1)*cosw0 + fTwoSqrtValpha ) ) / a0;
				AkReal32 b1 = ( -2*V*( (V-1) + (V+1)*cosw0 ) ) / a0;
				AkReal32 b2 = ( V*( (V+1) + (V-1)*cosw0 - fTwoSqrtValpha ) ) / a0;

				for ( unsigned int iChan = 0; iChan < numFilters; iChan++ )
				{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
					AkPlacementNew( &m_arFilterKRLB[iChan] ) DSP::BiquadFilterMonoPerSample();
#endif
					m_arFilterKRLB[iChan].SetCoefs( b0, b1, b2, a1, a2 );
				}
			}

			{
				AkReal32 w0 = 2 * PI * 38 / (AkReal32)in_uSampleRate;
				AkReal32 alpha = sinf(w0);
				AkReal32 cosw0 = cosf(w0);
				AkReal32 a0 = 1 + alpha;
				AkReal32 a1 = -(2 * cosw0) / a0; 
				AkReal32 a2 = (1 - alpha) / a0;
				AkReal32 b0 = ((1 + cosw0)/2) / a0;
				AkReal32 b1 = -(1 + cosw0) / a0;
				AkReal32 b2 = ((1 + cosw0)/2) / a0;

				for ( unsigned int iChan = 0; iChan < numFilters; iChan++ )
				{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
					AkPlacementNew( &m_arFilterKPre[iChan] ) DSP::BiquadFilterMonoPerSample();
#endif
					m_arFilterKPre[iChan].SetCoefs( b0, b1, b2, a1, a2 );
				}
			}
		}

		if ( in_eMeterTypes & AkMonitorData::BusMeterDataType_TruePeak )
		{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
			m_truePeak = AK::SpeakerVolumes::Vector::Allocate(m_channelConfig.uNumChannels);
			if ( !m_truePeak )
				return AK_Fail;
#endif
			AK::SpeakerVolumes::Vector::Zero(m_truePeak, m_channelConfig.uNumChannels);
			size_t uOverlapBufferSize = m_channelConfig.uNumChannels * AK_TRUE_PEAK_FIR_ORDER * sizeof(AkReal32);
#ifndef AK_VOICE_MAX_NUM_CHANNELS
			m_fPrevOverlapSave = (AkReal32*)AkMalign( AkMemID_Processing, uOverlapBufferSize, AK_SIMD_ALIGNMENT );
			if ( !m_fPrevOverlapSave )
				return AK_Fail;
#endif
			memset( m_fPrevOverlapSave, 0, uOverlapBufferSize );
		}

#ifndef AK_VOICE_MAX_NUM_CHANNELS
        if (in_eMeterTypes & AkMonitorData::BusMeterDataType_3DMeter 
			&& m_channelConfig.eConfigType == AK_ChannelConfigType_Ambisonic)
        {
            m_3DPeaks = AK::SpeakerVolumes::Vector::Allocate(m_channelConfig.uNumChannels);
            if (!m_3DPeaks)
                return AK_Fail;

            AK::SpeakerVolumes::Vector::Zero(m_3DPeaks, m_channelConfig.uNumChannels);
        }
#endif
		m_eMeterTypes = in_eMeterTypes;
		return AK_Success;		
	}

	// Multichannel meter methods.
	
	inline void MeterBufferPeak( 
		AkReal32 in_fBusGain, 
		AkAudioBuffer * in_pBuffer
		)
	{
		AkUInt32 uNumChannels = in_pBuffer->NumChannels();
		if (in_fBusGain > 0.f)
		{
			AkReal32 fBaseGain = in_fBusGain * AK_BUS_VOLUME_CORRECTION_FACTOR;
			AkUInt32 i = 0;
			do
			{
				m_peak[i] = fBaseGain * ComputePeak( in_pBuffer->GetChannel( i ), in_pBuffer->MaxFrames() );
				++i;
			}
			while( i < uNumChannels );
		}
		else
			AK::SpeakerVolumes::Vector::Zero( m_peak, uNumChannels );
	}

	inline void MeterBufferRMS( 
		AkReal32 in_fBusGain, 
		AkAudioBuffer * in_pBuffer
		)
	{
		AkUInt32 uNumChannels = in_pBuffer->NumChannels();
		if (in_fBusGain > 0.f)
		{
			AkReal32 fBaseGain = in_fBusGain * AK_BUS_VOLUME_CORRECTION_FACTOR;
			AkUInt32 i = 0;
			do
			{
				m_rms[i] = fBaseGain * ComputeRMS( in_pBuffer->GetChannel( i ), in_pBuffer->MaxFrames() );
				++i;
			}while( i < uNumChannels );
		}
		else
			AK::SpeakerVolumes::Vector::Zero( m_rms, uNumChannels );
	}

	inline void MeterBufferTruePeak( 
		AkReal32 in_fBusGain, 
		AkAudioBuffer * in_pBuffer
		)
	{
		AkUInt32 uNumChannels = in_pBuffer->NumChannels();
		if (in_fBusGain > 0.f)
		{
			AkReal32 fBaseGain = in_fBusGain * AK_BUS_VOLUME_CORRECTION_FACTOR;
			AkUInt32 i = 0;
			do
			{
				m_truePeak[i] = fBaseGain * ComputeTruePeak( in_pBuffer->GetChannel( i ), m_fPrevOverlapSave + (i*AK_TRUE_PEAK_FIR_ORDER), in_pBuffer->MaxFrames() );
				++i;
			}while( i < uNumChannels );
		}
		else
			AK::SpeakerVolumes::Vector::Zero( m_truePeak, uNumChannels );
	}

	inline void MeterBufferPeakIndex(
		AkReal32 in_fBusGain,
		AkAudioBuffer* in_pBuffer
	)
	{
		if (m_3DPeaks)
		{
			AkUInt32 uNumChannels = in_pBuffer->NumChannels();
			if (in_fBusGain > 0.f)
			{
				AkReal32 fBaseGain = in_fBusGain * AK_BUS_VOLUME_CORRECTION_FACTOR;
				AkUInt32 uiPeakIndex = 0;
				m_3DPeaks[0] = fBaseGain * ComputePeakIndex(in_pBuffer->GetChannel(0), in_pBuffer->MaxFrames(), uiPeakIndex);
				AkUInt32 i = 1;
				while (i < uNumChannels)
				{
					m_3DPeaks[i] = fBaseGain * in_pBuffer->GetChannel(i)[uiPeakIndex];
					++i;
				}
			}
			else
				AK::SpeakerVolumes::Vector::Zero(m_3DPeaks, uNumChannels);
		}
	}

	// Compute K power for all channels not available on SPU
	inline void ComputeKPower( 
		AkReal32 in_fBusGain, 
		AkAudioBuffer * in_pBuffer
		)
	{
		AkChannelConfig channels;
		AkChannelConfig backChannels;	// Special case for back channels: compensation boost (standard configs only).
		AkUInt32 uTotalChannels;
		if (in_pBuffer->GetChannelConfig().eConfigType == AK_ChannelConfigType_Standard && in_pBuffer->GetChannelConfig().uChannelMask != AK_SPEAKER_LOW_FREQUENCY)
		{
			/// TEMP: No standard for height channels yet. For now, they are just ignored.
			AkChannelMask maskStdConfig = in_pBuffer->GetChannelConfig().uChannelMask & (AK_SPEAKER_SETUP_DEFAULT_PLANE & ~AK_SPEAKER_LOW_FREQUENCY);
			AkChannelConfig channelCfgNoLfe;
			channelCfgNoLfe.SetStandard(maskStdConfig);
			channels.SetStandard(channelCfgNoLfe.uChannelMask & AK_SPEAKER_SETUP_3STEREO);
			uTotalChannels = channelCfgNoLfe.uNumChannels;
			backChannels.SetStandard(channelCfgNoLfe.uChannelMask & ~(channels.uChannelMask));

		}
		else 
		{
			if (in_pBuffer->GetChannelConfig().eConfigType == AK_ChannelConfigType_Ambisonic)
			{
				// Ambisonics: Compute loudness from W. Standard loudness metering depends on reproduction setup. This is the best and simplest approximation we can do.
				channels.SetAmbisonic(1);
			}
			else
			{
				// Other, anonymous and 0.1 configs: Sum power of all channels without compensation.
				channels = in_pBuffer->GetChannelConfig();
			}
			uTotalChannels = channels.uNumChannels;
		}

		AkReal32 fTotalPower = 0;
		AkReal32* in_pDataStart = in_pBuffer->GetChannel(0);
		
		// Setup: get filter memories on the stack.
		AkReal32 * AK_RESTRICT pIn = in_pDataStart;
		DSP::BiquadFilterMonoPerSample * pFilterKPre = m_arFilterKPre;
		DSP::BiquadFilterMonoPerSample * pFilterKRLB = m_arFilterKRLB;
		DSP::Memories * memKPre = (DSP::Memories*)AkAlloca( uTotalChannels * sizeof( DSP::Memories ) );
		DSP::Memories * memKRLB = (DSP::Memories*)AkAlloca( uTotalChannels * sizeof( DSP::Memories ) );
		
		for ( unsigned int uChan = 0; uChan < uTotalChannels; uChan++ )
		{
			pFilterKPre[uChan].GetMemories(uChan, memKPre[uChan].fFFwd1, memKPre[uChan].fFFwd2, memKPre[uChan].fFFbk1, memKPre[uChan].fFFbk2);
			pFilterKRLB[uChan].GetMemories(uChan, memKRLB[uChan].fFFwd1, memKRLB[uChan].fFFwd2, memKRLB[uChan].fFFbk1, memKRLB[uChan].fFFbk2);		
		}

		// Channels are contiguous, and front channels always come up before back channels, 
		// which always come up before LFE (ignored in loudness metering).
		AkUInt32 uChannel = 0;
		const AkUInt32 uMaxFrames = in_pBuffer->MaxFrames();

		// Start with front / unaltered channels.
		
		{	
			while( uChannel < channels.uNumChannels )
			{
				const AkReal32 * pEnd = pIn + uMaxFrames;
				while( pIn < pEnd )
				{
					/// TODO Use SIMD version
					
					AkReal32 fK = pFilterKPre[uChannel].ProcessSample( memKPre[uChannel], *pIn++ );
					fK = pFilterKRLB[uChannel].ProcessSample( memKRLB[uChannel], fK );

					fTotalPower += ( fK * fK );
				}
				++uChannel;
			}
		}

		// Then back channels. Their contribution needs to be boosted by 1.5 dB.
		{
			AkReal32 fTotalPowerBackChannels = 0;
			AkUInt32 uNumChannels = backChannels.uNumChannels + channels.uNumChannels;
			while( uChannel < uNumChannels )
			{
				const AkReal32 * pEnd = pIn + uMaxFrames;
				while( pIn < pEnd )
				{
					/// TODO Use SIMD version

					AkReal32 fK = pFilterKPre[uChannel].ProcessSample( memKPre[uChannel], *pIn++ );
					fK = pFilterKRLB[uChannel].ProcessSample( memKRLB[uChannel], fK );

					fTotalPowerBackChannels += ( fK * fK );
				}
				++uChannel;
			}

			// Add 1.5 dB (power): 10^(1.5/10)
			fTotalPower += ( fTotalPowerBackChannels * AK_BACK_CHANNELS_LOUDNESS_GAIN );
		}

		// Returned power must correspond to output bus. Apply gain here (in terms of power).
		in_fBusGain *= AK_BUS_VOLUME_CORRECTION_FACTOR;
		m_fMeanPowerK = in_fBusGain * in_fBusGain * fTotalPower / ( (AkReal32)uMaxFrames );

		// Teardown.
		for ( unsigned int uChan = 0; uChan < uTotalChannels; uChan++ )
		{
			RemoveDenormal( memKPre[uChan].fFFbk1 );
			RemoveDenormal( memKPre[uChan].fFFbk2 );
			pFilterKPre[uChan].SetMemories(uChan, memKPre[uChan].fFFwd1, memKPre[uChan].fFFwd2, memKPre[uChan].fFFbk1, memKPre[uChan].fFFbk2);

			RemoveDenormal( memKRLB[uChan].fFFbk1 );
			RemoveDenormal( memKRLB[uChan].fFFbk2 );
			pFilterKRLB[uChan].SetMemories(uChan, memKRLB[uChan].fFFwd1, memKRLB[uChan].fFFwd2, memKRLB[uChan].fFFbk1, memKRLB[uChan].fFFbk2);
		}
	}
} AK_ALIGN_DMA;

// Entry point.
// -----------------------------
inline void MeterBuffer( AkReal32 in_fNextVolume, AkAudioBuffer* in_pAudioBuffer, AkMeterCtx * in_pMeterCtx )
{
	AkReal32 fNextVol = in_pAudioBuffer->uValidFrames > 0 ? in_fNextVolume : 0.f;
	AkUInt8 types = in_pMeterCtx->MeterTypes();
	if ( types & AkMonitorData::BusMeterDataType_Peak )
		in_pMeterCtx->MeterBufferPeak( fNextVol, in_pAudioBuffer );
	if ( types & AkMonitorData::BusMeterDataType_RMS )
		in_pMeterCtx->MeterBufferRMS( fNextVol, in_pAudioBuffer );
	if ( types & AkMonitorData::BusMeterDataType_TruePeak )
		in_pMeterCtx->MeterBufferTruePeak( fNextVol, in_pAudioBuffer );
	if( types & AkMonitorData::BusMeterDataType_KPower )
		in_pMeterCtx->ComputeKPower( fNextVol, in_pAudioBuffer );
    if (types & AkMonitorData::BusMeterDataType_3DMeter)
        in_pMeterCtx->MeterBufferPeakIndex(fNextVol, in_pAudioBuffer);
}

// Factory object that will return the same instance as long as parameters do not change
class AkMeterCtxFactory
{
public:
	AkMeterCtxFactory()
		: m_flags(AK_NoMetering)
		, m_meter(nullptr)
	{
	}

	~AkMeterCtxFactory()
	{
		if (m_meter)
		{
			AkDelete(AkMemID_Processing, m_meter);
			m_meter = nullptr;
		}
	}

	AkMeterCtx * CreateMeterContext()
	{
		if (m_meter && (m_flags != m_meter->MeterTypes() || m_channelConfig != m_meter->GetChannelConfig()))
		{
			AkDelete(AkMemID_Processing, m_meter);
			m_meter = nullptr;
		}

		if (!m_meter && m_flags != AK_NoMetering)
		{
			m_meter = (AkMeterCtx *)AkNew(AkMemID_Processing, AkMeterCtx(m_channelConfig));
			if (m_meter)
			{
				AKRESULT eResult = m_meter->Init(AK_CORE_SAMPLERATE, m_flags);
				if (eResult != AK_Success)
				{
					AkDelete(AkMemID_Processing, m_meter);
					m_meter = nullptr;
				}
			}
		}
		return m_meter;
	}

	void SetParameters(const AkChannelConfig &channelConfig, AkMeteringFlags flags)
	{
		m_channelConfig = channelConfig;
		m_flags = flags;
	}

private:
	AkMeteringFlags m_flags;
	AkChannelConfig m_channelConfig;
	AkMeterCtx * m_meter;
};

#endif //_METERTOOLS_H_
