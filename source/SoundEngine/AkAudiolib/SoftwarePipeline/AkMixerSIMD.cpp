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
// AkMixerSIMD.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "AkMath.h"
#include "AkMixer.h"
#include "AkMixerCommon.h"
#include <AK/SoundEngine/Common/AkSimd.h>
#include "AudiolibDefs.h"

// Number of floats in vectors.
static const AkUInt32	ulVectorSize = 4;

// After running this macro, Vol will have the base, initial, volume for 4 samples of data, and volDelta will have the offset to apply each simd step
#define BUILD_VOLUME_DELTA_VECTOR( VOL, VOLDELTA, RAMP, NUMFRAMES ) \
	AKSIMD_V4F32 VOL = AKSIMD_SET_V4F32(RAMP.fPrev); \
	AKSIMD_V4F32 VOLDELTA = AKSIMD_SET_V4F32((RAMP.fNext - RAMP.fPrev) / (AkReal32)NUMFRAMES); \
	{ \
		AK_ALIGN_SIMD(const AkReal32 aVolumes[4]) = { 0.f, 1.f, 2.f, 3.f}; \
		VOL = AKSIMD_ADD_V4F32(VOL, AKSIMD_MUL_V4F32(VOLDELTA, AKSIMD_LOAD_V4F32(aVolumes))); \
		VOLDELTA = AKSIMD_MUL_V4F32(VOLDELTA, AKSIMD_SET_V4F32(AkReal32(ulVectorSize))); \
	} \

#define BUILD_VOLUME_VECTOR( VECT, VOLUME, VOLUME_DELTA ) \
	AK_ALIGN_SIMD(AkReal32 aVolumes[4]);\
	aVolumes[0] = VOLUME; \
	aVolumes[1] = VOLUME + VOLUME_DELTA; \
	aVolumes[2] = VOLUME + 2.f * VOLUME_DELTA; \
	aVolumes[3] = VOLUME + 3.f * VOLUME_DELTA; \
	AKSIMD_V4F32 VECT = AKSIMD_LOAD_V4F32( aVolumes );

// Assumes usMaxFrames % 8 == 0
void ApplyGainMono_V4F32(
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain)
{
	AkUInt16 usMaxFrames = in_pInputBuffer->MaxFrames();
	AkReal32* AK_RESTRICT pSourceData = in_pInputBuffer->GetChannel(0);
	AkReal32* AK_RESTRICT pDestData = in_pOutputBuffer->GetChannel(0);
	AkReal32* AK_RESTRICT pSourceEnd = pSourceData + usMaxFrames;

	BUILD_VOLUME_DELTA_VECTOR(vVolumes, vVolumesDelta, in_gain, usMaxFrames);
	
	if (in_gain.fNext == in_gain.fPrev)
	{
		AKASSERT(!(usMaxFrames % 8));
		do
		{
			// get eight samples								 
			AKSIMD_V4F32 vSrc1 = AKSIMD_LOAD_V4F32(pSourceData);
			AKSIMD_V4F32 vSrc2 = AKSIMD_LOAD_V4F32(pSourceData + ulVectorSize);
			pSourceData += ulVectorSize * 2;

			// apply volume
			AKSIMD_V4F32 vOut1 = AKSIMD_MUL_V4F32(vSrc1, vVolumes);
			AKSIMD_V4F32 vOut2 = AKSIMD_MUL_V4F32(vSrc2, vVolumes);

			// store the result								 
			AKSIMD_STORE_V4F32(pDestData, vOut1);
			AKSIMD_STORE_V4F32(pDestData + ulVectorSize, vOut2);
			pDestData += ulVectorSize * 2;
		} while (pSourceData < pSourceEnd);
	}
	else // has volume delta
	{
		AKASSERT(!(usMaxFrames % 8));

		AKSIMD_V4F32 vVolumes1 = vVolumes;
		AKSIMD_V4F32 vVolumes2 = AKSIMD_ADD_V4F32(vVolumes, vVolumesDelta);

		// multiply volumes delta by 2 because the loop is unrolled by 2.
		vVolumesDelta = AKSIMD_ADD_V4F32(vVolumesDelta, vVolumesDelta);

		do
		{
			// get eight samples								 
			AKSIMD_V4F32 vSrc1 = AKSIMD_LOAD_V4F32(pSourceData);
			AKSIMD_V4F32 vSrc2 = AKSIMD_LOAD_V4F32(pSourceData + ulVectorSize);
			pSourceData += ulVectorSize * 2;

			// apply volume
			AKSIMD_V4F32 vOutput1 = AKSIMD_MUL_V4F32(vSrc1, vVolumes1);
			AKSIMD_V4F32 vOutput2 = AKSIMD_MUL_V4F32(vSrc2, vVolumes2);

			// store the result								 
			AKSIMD_STORE_V4F32(pDestData, vOutput1);
			AKSIMD_STORE_V4F32(pDestData + ulVectorSize, vOutput2);
			pDestData += ulVectorSize * 2;

			// in_fVolume += in_fVolumeDelta;
			vVolumes1 = AKSIMD_ADD_V4F32(vVolumes1, vVolumesDelta);
			vVolumes2 = AKSIMD_ADD_V4F32(vVolumes2, vVolumesDelta);
		} while (pSourceData < pSourceEnd);
	}
}

void ApplyGainAndInterleaveStereoInt16_V4F32(
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain)
{
	AkUInt16 usMaxFrames = in_pInputBuffer->MaxFrames();
	AKSIMD_V4F32 mMul = AKSIMD_SET_V4F32(AUDIOSAMPLE_SHORT_MAX); // duplicate multiplier factor 4x
	BUILD_VOLUME_DELTA_VECTOR( vVolumes, vVolumesDelta, in_gain, usMaxFrames );

	AkReal32* AK_RESTRICT l_pSourceDataL = in_pInputBuffer->GetChannel(AK_IDX_SETUP_2_LEFT);
	AkReal32* AK_RESTRICT l_pSourceDataR = in_pInputBuffer->GetChannel( AK_IDX_SETUP_2_RIGHT );
	AkInt16* AK_RESTRICT pDestData = (AkInt16*)in_pOutputBuffer->GetInterleavedData();

	for( AkUInt32 ulFrames = usMaxFrames / ulVectorSize; ulFrames > 0; --ulFrames )
	{
		// *in_pDestData += in_fVolume * (*(in_pSourceData)++);
		// get four samples
		AKSIMD_V4F32 vVectL = AKSIMD_LOAD_V4F32( l_pSourceDataL );
		l_pSourceDataL += ulVectorSize;

		// apply volume
		vVectL = AKSIMD_MUL_V4F32( vVectL, vVolumes );

		AKSIMD_V4F32 vVectR = AKSIMD_LOAD_V4F32( l_pSourceDataR );
		l_pSourceDataR += ulVectorSize;

		// apply volume
		vVectR = AKSIMD_MUL_V4F32( vVectR, vVolumes );

		// increment the volumes
		vVolumes  = AKSIMD_ADD_V4F32( vVolumes, vVolumesDelta );

//////////////////////////////////////////////////////////////

		// vVectL = [ L3, L2, L1, L0] * volumes
		// vVectR = [ R3, R2, R1, R0] * volumes
		// we want vTmpFloat1 = [ R1, L1, R0, L0]
		AKSIMD_V4F32 vTmpFloat1 = AKSIMD_UNPACKLO_V4F32( vVectL, vVectR );

		// we want vTmpFloat2 = [ R3, L3, R2, L2]
		AKSIMD_V4F32 vTmpFloat2 = AKSIMD_UNPACKHI_V4F32( vVectL, vVectR );

		// Convert to samples to signed int
		AKSIMD_V4F32 mTmp1 = AKSIMD_MUL_V4F32( vTmpFloat1, mMul );
		AKSIMD_V4F32 mTmp2 = AKSIMD_MUL_V4F32( vTmpFloat2, mMul );

		// Convert from Float32 -> Int32 -> Int16
		AKSIMD_V4I32 mShorts = AKSIMD_PACKS_V4I32(AKSIMD_TRUNCATE_V4F32_TO_V4I32(mTmp1), AKSIMD_TRUNCATE_V4F32_TO_V4I32(mTmp2));
		// Store the data
		AKSIMD_STOREU_V4I32((AKSIMD_V4I32*)pDestData, mShorts);

		pDestData += ulVectorSize * 2;
	}
}

//====================================================================================================

void ApplyGainAndInterleaveStereo_V4F32( 
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain)
{
	AkUInt16 usMaxFrames = in_pInputBuffer->MaxFrames();
	BUILD_VOLUME_DELTA_VECTOR(vVolumes, vVolumesDelta, in_gain, usMaxFrames);

	AkReal32* AK_RESTRICT l_pSourceDataL = in_pInputBuffer->GetChannel( AK_IDX_SETUP_2_LEFT );
	AkReal32* AK_RESTRICT l_pSourceDataR = in_pInputBuffer->GetChannel( AK_IDX_SETUP_2_RIGHT );
	AkReal32* AK_RESTRICT pDestData = (AkReal32*)in_pOutputBuffer->GetInterleavedData();

	for( AkUInt32 ulFrames = usMaxFrames / ulVectorSize; ulFrames > 0; --ulFrames )
	{
		// *in_pDestData += in_fVolume * (*(in_pSourceData)++);
		// get four samples								 
		AKSIMD_V4F32 vVectL = AKSIMD_LOAD_V4F32( l_pSourceDataL );
		l_pSourceDataL += ulVectorSize;

		// apply volume									 
		vVectL = AKSIMD_MUL_V4F32( vVectL, vVolumes );
		
		AKSIMD_V4F32 vVectR = AKSIMD_LOAD_V4F32( l_pSourceDataR );
		l_pSourceDataR += ulVectorSize;

		// apply volume									 
		vVectR = AKSIMD_MUL_V4F32( vVectR, vVolumes );
		
		// increment the volumes
		vVolumes  = AKSIMD_ADD_V4F32( vVolumes, vVolumesDelta );
		
		// vVectL = [ L3, L2, L1, L0] * volumes
		// vVectR = [ R3, R2, R1, R0] * volumes
		// we want vDest1 = [ R1, L1, R0, L0]
		AKSIMD_V4F32 vDest1 = AKSIMD_UNPACKLO_V4F32( vVectL, vVectR );

		// we want vDest2 = [ R3, L3, R2, L2]
		AKSIMD_V4F32 vDest2 = AKSIMD_UNPACKHI_V4F32( vVectL, vVectR );

		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// store the vDest2 result
		AKSIMD_STORE_V4F32( pDestData, vDest2 );
		pDestData += ulVectorSize;
	}
}

//====================================================================================================
//====================================================================================================
void ApplyGainAndInterleave51_V4F32( 
	AkAudioBuffer* in_pInputBuffer, 
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain)
{
	AkUInt16 usMaxFrames = in_pInputBuffer->MaxFrames();
	BUILD_VOLUME_DELTA_VECTOR(vVolumes, vVolumesDelta, in_gain, usMaxFrames);

	AkReal32* AK_RESTRICT l_pSourceDataFL  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_5_FRONTLEFT );
	AkReal32* AK_RESTRICT l_pSourceDataFR  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_5_FRONTRIGHT );
	AkReal32* AK_RESTRICT l_pSourceDataC   = in_pInputBuffer->GetChannel( AK_IDX_SETUP_5_CENTER );
	AkReal32* AK_RESTRICT l_pSourceDataLFE = in_pInputBuffer->GetChannel( AK_IDX_SETUP_5_LFE );
	AkReal32* AK_RESTRICT l_pSourceDataRL  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_5_REARLEFT );
	AkReal32* AK_RESTRICT l_pSourceDataRR  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_5_REARRIGHT );
	AkReal32* AK_RESTRICT pDestData = (AkReal32*)in_pOutputBuffer->GetInterleavedData();

	for( AkUInt32 ulFrames = usMaxFrames / ulVectorSize; ulFrames > 0; --ulFrames )
	{
		/////////////////////////////////////////////////////////////////////
		// Apply the volumes to the source
		/////////////////////////////////////////////////////////////////////

		// get four samples								 
		AKSIMD_V4F32 vVectFL = AKSIMD_LOAD_V4F32( l_pSourceDataFL );
		l_pSourceDataFL += ulVectorSize;

		// apply volume									 
		vVectFL = AKSIMD_MUL_V4F32( vVectFL, vVolumes );
		
		AKSIMD_V4F32 vVectFR = AKSIMD_LOAD_V4F32( l_pSourceDataFR );
		l_pSourceDataFR += ulVectorSize;

		// apply volume									 
		vVectFR = AKSIMD_MUL_V4F32( vVectFR, vVolumes );

		AKSIMD_V4F32 vVectC = AKSIMD_LOAD_V4F32( l_pSourceDataC );
		l_pSourceDataC += ulVectorSize;

		// apply volume									 
		vVectC = AKSIMD_MUL_V4F32( vVectC, vVolumes );

		AKSIMD_V4F32 vVectLFE = AKSIMD_LOAD_V4F32( l_pSourceDataLFE );
		l_pSourceDataLFE += ulVectorSize;

		// apply volume									 
		vVectLFE = AKSIMD_MUL_V4F32( vVectLFE, vVolumes );

		AKSIMD_V4F32 vVectRL = AKSIMD_LOAD_V4F32( l_pSourceDataRL );
		l_pSourceDataRL += ulVectorSize;

		// apply volume									 
		vVectRL = AKSIMD_MUL_V4F32( vVectRL, vVolumes );

		AKSIMD_V4F32 vVectRR = AKSIMD_LOAD_V4F32( l_pSourceDataRR );
		l_pSourceDataRR += ulVectorSize;

		// apply volume									 
		vVectRR = AKSIMD_MUL_V4F32( vVectRR, vVolumes );
		
		// increment the volumes
		vVolumes  = AKSIMD_ADD_V4F32( vVolumes,  vVolumesDelta );
//----------------------------------------------------------------------------------------------------
// Interleave the data
//
//  12 AKSIMD_SHUFFLE_V4F32()
//   6 AKSIMD_STORE_V4F32()
//----------------------------------------------------------------------------------------------------
		AkRegister AKSIMD_V4F32 vFrontLeftRight = AKSIMD_SHUFFLE_V4F32( vVectFL, vVectFR, AKSIMD_SHUFFLE( 1, 0, 1, 0 ) );	// FR1, FR0, FL1, FL0
		AkRegister AKSIMD_V4F32 vCenterLfe = AKSIMD_SHUFFLE_V4F32( vVectC, vVectLFE, AKSIMD_SHUFFLE( 1, 0, 1, 0 ) );			// LFE1, LFE0, C1, C0
		AkRegister AKSIMD_V4F32 vRearLeftRight = AKSIMD_SHUFFLE_V4F32( vVectRL, vVectRR, AKSIMD_SHUFFLE( 1, 0, 1, 0 ) );		// RR1, RR0, RL1, RL0

		// we want vDest1 = [ LFE0, C0, FR0, FL0 ]
		AKSIMD_V4F32 vDest1 = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vCenterLfe, AKSIMD_SHUFFLE( 2, 0, 2, 0 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ FR1, FL1, RR0, RL0 ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vRearLeftRight, vFrontLeftRight, AKSIMD_SHUFFLE( 3, 1, 2, 0 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ RR1, RL1, LFE1, C1 ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vCenterLfe, vRearLeftRight, AKSIMD_SHUFFLE( 3, 1, 3, 1 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		vFrontLeftRight = AKSIMD_SHUFFLE_V4F32( vVectFL, vVectFR, AKSIMD_SHUFFLE( 3, 2, 3, 2 ) );	// FR3, FR2, FL3, FL2
		vCenterLfe = AKSIMD_SHUFFLE_V4F32( vVectC, vVectLFE, AKSIMD_SHUFFLE( 3, 2, 3, 2 ) );			// LFE3, LFE2, C3, C2
		vRearLeftRight = AKSIMD_SHUFFLE_V4F32( vVectRL, vVectRR, AKSIMD_SHUFFLE( 3, 2, 3, 2 ) );		// RR3, RR2, RL3, RL2

		// we want vDest1 = [ LFE2, C2, FR2, FL2 ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vCenterLfe, AKSIMD_SHUFFLE( 2, 0, 2, 0 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ FR3, FL3, RR2, RL2 ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vRearLeftRight, vFrontLeftRight, AKSIMD_SHUFFLE( 3, 1, 2, 0 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ RR3, RL3, LFE3, C3 ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vCenterLfe, vRearLeftRight, AKSIMD_SHUFFLE( 3, 1, 3, 1 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;
	}
}

void ApplyGainAndInterleave71FromStereo_V4F32(
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain)
{
	AkUInt16 usMaxFrames = in_pInputBuffer->MaxFrames();
	BUILD_VOLUME_DELTA_VECTOR(vVolumes, vVolumesDelta, in_gain, usMaxFrames);

	AkReal32* AK_RESTRICT l_pSourceDataFL  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_2_LEFT );
	AkReal32* AK_RESTRICT l_pSourceDataFR  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_2_RIGHT );
	AkReal32* AK_RESTRICT pDestData = (AkReal32*)in_pOutputBuffer->GetInterleavedData();

	for( AkUInt32 ulFrames = usMaxFrames / ulVectorSize; ulFrames > 0; --ulFrames )
	{
		/////////////////////////////////////////////////////////////////////
		// Apply the volumes to the source
		/////////////////////////////////////////////////////////////////////

		// *in_pDestData += in_fVolume * (*(in_pSourceData)++);
		// get four samples								 
		AKSIMD_V4F32 vVectFL = AKSIMD_LOAD_V4F32( l_pSourceDataFL );
		l_pSourceDataFL += ulVectorSize;
		// apply volume									 
		vVectFL = AKSIMD_MUL_V4F32( vVectFL, vVolumes );
		
		AKSIMD_V4F32 vVectFR = AKSIMD_LOAD_V4F32( l_pSourceDataFR );
		l_pSourceDataFR += ulVectorSize;
		// apply volume									 
		vVectFR = AKSIMD_MUL_V4F32( vVectFR, vVolumes );

		// increment the volumes
		vVolumes  = AKSIMD_ADD_V4F32( vVolumes,  vVolumesDelta );

//----------------------------------------------------------------------------------------------------
// Interleave the data
//----------------------------------------------------------------------------------------------------

		AkRegister AKSIMD_V4F32 vFrontLeftRight = AKSIMD_SHUFFLE_V4F32( vVectFL, vVectFR, AKSIMD_SHUFFLE( 1, 0, 1, 0 ) );	// FR1, FR0, FL1, FL0
		AkRegister AKSIMD_V4F32 vZeros = AKSIMD_SETZERO_V4F32();

		// we want [ LFE0, C0, FR0, FL0 ]
		AkRegister AKSIMD_V4F32 vDest = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vZeros, AKSIMD_SHUFFLE( 2, 0, 2, 0 ));
		AKSIMD_STORE_V4F32( pDestData, vDest );
		pDestData += ulVectorSize;

		// we want [ ER0, EL0, RR0, RL0  ]
		AKSIMD_STORE_V4F32( pDestData, vZeros );
		pDestData += ulVectorSize;

		// we want [ LFE1, C1, FR1, FL1 ]
		vDest = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vZeros, AKSIMD_SHUFFLE( 3, 1, 3, 1 ));
		AKSIMD_STORE_V4F32( pDestData, vDest );
		pDestData += ulVectorSize;

		// we want [ ER1, EL1, RR1, RL1 ]
		AKSIMD_STORE_V4F32( pDestData, vZeros );
		pDestData += ulVectorSize;

		vFrontLeftRight = AKSIMD_SHUFFLE_V4F32( vVectFL, vVectFR, AKSIMD_SHUFFLE( 3, 2, 3, 2 ) );					// FR3, FR2, FL3, FL2

		// we want [ LFE2, C2, FR2, FL2 ]
		vDest = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vZeros, AKSIMD_SHUFFLE( 2, 0, 2, 0 ));
		AKSIMD_STORE_V4F32( pDestData, vDest );
		pDestData += ulVectorSize;

		// we want [ ER2, EL2, RR2, RL2  ]
		AKSIMD_STORE_V4F32( pDestData, vZeros );
		pDestData += ulVectorSize;

		// we want [ LFE3, C3, FR3, FL3 ]
		vDest = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vZeros, AKSIMD_SHUFFLE( 3, 1, 3, 1 ) );
		AKSIMD_STORE_V4F32( pDestData, vDest );
		pDestData += ulVectorSize;

		// we want [ ER3, EL3, RR3, RL3 ]
		AKSIMD_STORE_V4F32( pDestData, vZeros );
		pDestData += ulVectorSize;
	}
}

void ApplyGainAndInterleave51FromStereo_V4F32(
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain)
{
	AkUInt16 usMaxFrames = in_pInputBuffer->MaxFrames();
	BUILD_VOLUME_DELTA_VECTOR(vVolumes, vVolumesDelta, in_gain, usMaxFrames);

	AkReal32* AK_RESTRICT l_pSourceDataFL  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_2_LEFT );
	AkReal32* AK_RESTRICT l_pSourceDataFR  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_2_RIGHT );
	AkReal32* AK_RESTRICT pDestData = (AkReal32*)in_pOutputBuffer->GetInterleavedData();

	for( AkUInt32 ulFrames = usMaxFrames / ulVectorSize; ulFrames > 0; --ulFrames )
	{
		/////////////////////////////////////////////////////////////////////
		// Apply the volumes to the source
		/////////////////////////////////////////////////////////////////////

		// *in_pDestData += in_fVolume * (*(in_pSourceData)++);
		// get four samples								 
		AKSIMD_V4F32 vVectFL = AKSIMD_LOAD_V4F32( l_pSourceDataFL );
		l_pSourceDataFL += ulVectorSize;
		// apply volume									 
		vVectFL = AKSIMD_MUL_V4F32( vVectFL, vVolumes );
		
		AKSIMD_V4F32 vVectFR = AKSIMD_LOAD_V4F32( l_pSourceDataFR );
		l_pSourceDataFR += ulVectorSize;
		// apply volume									 
		vVectFR = AKSIMD_MUL_V4F32( vVectFR, vVolumes );

		// increment the volumes
		vVolumes  = AKSIMD_ADD_V4F32( vVolumes,  vVolumesDelta );

//----------------------------------------------------------------------------------------------------
// Interleave the data
//----------------------------------------------------------------------------------------------------

		AkRegister AKSIMD_V4F32 vFrontLeftRight = AKSIMD_SHUFFLE_V4F32( vVectFL, vVectFR, AKSIMD_SHUFFLE( 1, 0, 1, 0 ) );	// FR1, FR0, FL1, FL0
		AkRegister AKSIMD_V4F32 vZeros = AKSIMD_SETZERO_V4F32();

		// we want [ LFE0, C0, FR0, FL0 ]
		AkRegister AKSIMD_V4F32 vDest = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vZeros, AKSIMD_SHUFFLE( 2, 0, 2, 0 ));
		AKSIMD_STORE_V4F32( pDestData, vDest );
		pDestData += ulVectorSize;

		// we want [ FR1, FL1, RR0, RL0 ]
		vDest = AKSIMD_SHUFFLE_V4F32( vZeros, vFrontLeftRight, AKSIMD_SHUFFLE( 3, 1, 3, 1 ));
		AKSIMD_STORE_V4F32( pDestData, vDest );
		pDestData += ulVectorSize;

		// we want [ RR1, RL1, LFE1, C1 ]
		AKSIMD_STORE_V4F32( pDestData, vZeros );
		pDestData += ulVectorSize;

		vFrontLeftRight = AKSIMD_SHUFFLE_V4F32( vVectFL, vVectFR, AKSIMD_SHUFFLE( 3, 2, 3, 2 ) );					// FR3, FR2, FL3, FL2

		// we want [ LFE2, C2, FR2, FL2 ]
		vDest = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vZeros, AKSIMD_SHUFFLE( 2, 0, 2, 0 ));
		AKSIMD_STORE_V4F32( pDestData, vDest );
		pDestData += ulVectorSize;

		// we want [ FR3, FL3, RR2, RL2 ]
		vDest = AKSIMD_SHUFFLE_V4F32( vZeros, vFrontLeftRight, AKSIMD_SHUFFLE( 3, 1, 3, 1 ));
		AKSIMD_STORE_V4F32( pDestData, vDest );
		pDestData += ulVectorSize;

		// we want [ RR3, RL3, LFE3, C3 ]
		AKSIMD_STORE_V4F32( pDestData, vZeros );
		pDestData += ulVectorSize;
	}
}

void ApplyGainAndInterleave71From51_V4F32(
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain)
{
	AkUInt16 usMaxFrames = in_pInputBuffer->MaxFrames();
	BUILD_VOLUME_DELTA_VECTOR(vVolumes, vVolumesDelta, in_gain, usMaxFrames);

	AkReal32* AK_RESTRICT l_pSourceDataFL  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_5_FRONTLEFT );
	AkReal32* AK_RESTRICT l_pSourceDataFR  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_5_FRONTRIGHT );
	AkReal32* AK_RESTRICT l_pSourceDataC   = in_pInputBuffer->GetChannel( AK_IDX_SETUP_5_CENTER );
	AkReal32* AK_RESTRICT l_pSourceDataLFE = in_pInputBuffer->GetChannel( AK_IDX_SETUP_5_LFE );
	AkReal32* AK_RESTRICT l_pSourceDataRL  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_5_REARLEFT );
	AkReal32* AK_RESTRICT l_pSourceDataRR  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_5_REARRIGHT );
	//AkReal32* AK_RESTRICT l_pSourceDataEL  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_7_SIDELEFT );
	//AkReal32* AK_RESTRICT l_pSourceDataER  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_7_SIDERIGHT );
	AkReal32* AK_RESTRICT pDestData = (AkReal32*)in_pOutputBuffer->GetInterleavedData();

	for( AkUInt32 ulFrames = usMaxFrames / ulVectorSize; ulFrames > 0; --ulFrames )
	{
		/////////////////////////////////////////////////////////////////////
		// Apply the volumes to the source
		/////////////////////////////////////////////////////////////////////

		// *in_pDestData += in_fVolume * (*(in_pSourceData)++);
		// get four samples								 
		AKSIMD_V4F32 vVectFL = AKSIMD_LOAD_V4F32( l_pSourceDataFL );
		l_pSourceDataFL += ulVectorSize;
		// apply volume									 
		vVectFL = AKSIMD_MUL_V4F32( vVectFL, vVolumes );
		
		AKSIMD_V4F32 vVectFR = AKSIMD_LOAD_V4F32( l_pSourceDataFR );
		l_pSourceDataFR += ulVectorSize;
		// apply volume									 
		vVectFR = AKSIMD_MUL_V4F32( vVectFR, vVolumes );

		AKSIMD_V4F32 vVectC = AKSIMD_LOAD_V4F32( l_pSourceDataC );
		l_pSourceDataC += ulVectorSize;
		// apply volume									 
		vVectC = AKSIMD_MUL_V4F32( vVectC, vVolumes );

		AKSIMD_V4F32 vVectLFE = AKSIMD_LOAD_V4F32( l_pSourceDataLFE );
		l_pSourceDataLFE += ulVectorSize;
		// apply volume									 
		vVectLFE = AKSIMD_MUL_V4F32( vVectLFE, vVolumes );

		AKSIMD_V4F32 vVectRL = AKSIMD_LOAD_V4F32( l_pSourceDataRL );
		l_pSourceDataRL += ulVectorSize;
		// apply volume									 
		vVectRL = AKSIMD_MUL_V4F32( vVectRL, vVolumes );

		AKSIMD_V4F32 vVectRR = AKSIMD_LOAD_V4F32( l_pSourceDataRR );
		l_pSourceDataRR += ulVectorSize;
		// apply volume									 
		vVectRR = AKSIMD_MUL_V4F32( vVectRR, vVolumes );

		/*AKSIMD_V4F32 vVectEL = AKSIMD_LOAD_V4F32( l_pSourceDataEL );
		l_pSourceDataEL += ulVectorSize;
		// apply volume									 
		vVectEL = AKSIMD_MUL_V4F32( vVectEL, vVolumes );

		AKSIMD_V4F32 vVectER = AKSIMD_LOAD_V4F32( l_pSourceDataER );
		l_pSourceDataER += ulVectorSize;
		// apply volume									 
		vVectER = AKSIMD_MUL_V4F32( vVectER, vVolumes );*/

		// increment the volumes
		vVolumes  = AKSIMD_ADD_V4F32( vVolumes,  vVolumesDelta );
//----------------------------------------------------------------------------------------------------
// Interleave the data
//
//  12 AKSIMD_SHUFFLE_V4F32()
//   6 AKSIMD_STORE_V4F32()
//----------------------------------------------------------------------------------------------------
		AkRegister AKSIMD_V4F32 vFrontLeftRight = AKSIMD_SHUFFLE_V4F32( vVectFL, vVectFR, AKSIMD_SHUFFLE( 1, 0, 1, 0 ) );	// FR1, FR0, FL1, FL0
		AkRegister AKSIMD_V4F32 vCenterLfe = AKSIMD_SHUFFLE_V4F32( vVectC, vVectLFE, AKSIMD_SHUFFLE( 1, 0, 1, 0 ) );			// LFE1, LFE0, C1, C0
		AkRegister AKSIMD_V4F32 vRearLeftRight = AKSIMD_SHUFFLE_V4F32( vVectRL, vVectRR, AKSIMD_SHUFFLE( 1, 0, 1, 0 ) );		// RR1, RR0, RL1, RL0
		AkRegister AKSIMD_V4F32 vExtraLeftRight = AKSIMD_SETZERO_V4F32(); //AKSIMD_SHUFFLE_V4F32( vVectEL, vVectER,  AKSIMD_SHUFFLE( 1, 0, 1, 0 ) );	// ER1, EL1, ER0, EL0

		// we want vDest1 = [ LFE0, C0, FR0, FL0 ]
		AkRegister AKSIMD_V4F32 vDest1 = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vCenterLfe, AKSIMD_SHUFFLE( 2, 0, 2, 0 ));
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ ER0, EL0, RR0, RL0  ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vRearLeftRight, vExtraLeftRight, AKSIMD_SHUFFLE( 2, 0, 2, 0 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ LFE1, C1, FR1, FL1 ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vCenterLfe, AKSIMD_SHUFFLE( 3, 1, 3, 1 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ ER1, EL1, RR1, RL1 ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vRearLeftRight, vExtraLeftRight, AKSIMD_SHUFFLE( 3, 1, 3, 1 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		vFrontLeftRight = AKSIMD_SHUFFLE_V4F32( vVectFL, vVectFR, AKSIMD_SHUFFLE( 3, 2, 3, 2 ) );					// FR3, FR2, FL3, FL2
		vCenterLfe = AKSIMD_SHUFFLE_V4F32( vVectC, vVectLFE, AKSIMD_SHUFFLE( 3, 2, 3, 2 ) );							// LFE3, LFE2, C3, C2
		vRearLeftRight = AKSIMD_SHUFFLE_V4F32( vVectRL, vVectRR, AKSIMD_SHUFFLE( 3, 2, 3, 2 ) );						// RR3, RR2, RL3, RL2
		vExtraLeftRight = AKSIMD_SETZERO_V4F32(); //AKSIMD_SHUFFLE_V4F32( vVectEL, vVectER,  AKSIMD_SHUFFLE( 3, 2, 3, 2 ) );					// ER3, EL2, ER3, EL2

		// we want vDest1 = [ LFE2, C2, FR2, FL2 ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vCenterLfe, AKSIMD_SHUFFLE( 2, 0, 2, 0 ));
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ ER2, EL2, RR2, RL2  ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vRearLeftRight, vExtraLeftRight, AKSIMD_SHUFFLE( 2, 0, 2, 0 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ LFE3, C3, FR3, FL3 ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vCenterLfe, AKSIMD_SHUFFLE( 3, 1, 3, 1 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ ER3, EL3, RR3, RL3 ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vRearLeftRight, vExtraLeftRight, AKSIMD_SHUFFLE( 3, 1, 3, 1 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;
	}
}

//====================================================================================================
//====================================================================================================

void ApplyGainAndInterleave71_V4F32(
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain)
{
	AkUInt16 usMaxFrames = in_pInputBuffer->MaxFrames();
	BUILD_VOLUME_DELTA_VECTOR(vVolumes, vVolumesDelta, in_gain, usMaxFrames);

	AkReal32* AK_RESTRICT l_pSourceDataFL  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_7_FRONTLEFT );
	AkReal32* AK_RESTRICT l_pSourceDataFR  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_7_FRONTRIGHT );
	AkReal32* AK_RESTRICT l_pSourceDataC   = in_pInputBuffer->GetChannel( AK_IDX_SETUP_7_CENTER );
	AkReal32* AK_RESTRICT l_pSourceDataLFE = in_pInputBuffer->GetChannel( AK_IDX_SETUP_7_LFE );
	AkReal32* AK_RESTRICT l_pSourceDataRL  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_7_REARLEFT );
	AkReal32* AK_RESTRICT l_pSourceDataRR  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_7_REARRIGHT );
	AkReal32* AK_RESTRICT l_pSourceDataEL  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_7_SIDELEFT );
	AkReal32* AK_RESTRICT l_pSourceDataER  = in_pInputBuffer->GetChannel( AK_IDX_SETUP_7_SIDERIGHT );
	AkReal32* AK_RESTRICT pDestData = (AkReal32*)in_pOutputBuffer->GetInterleavedData();

	for( AkUInt32 ulFrames = usMaxFrames / ulVectorSize; ulFrames > 0; --ulFrames )
	{
		/////////////////////////////////////////////////////////////////////
		// Apply the volumes to the source
		/////////////////////////////////////////////////////////////////////

		// *in_pDestData += in_fVolume * (*(in_pSourceData)++);
		// get four samples								 
		AKSIMD_V4F32 vVectFL = AKSIMD_LOAD_V4F32( l_pSourceDataFL );
		l_pSourceDataFL += ulVectorSize;
		// apply volume									 
		vVectFL = AKSIMD_MUL_V4F32( vVectFL, vVolumes );
		
		AKSIMD_V4F32 vVectFR = AKSIMD_LOAD_V4F32( l_pSourceDataFR );
		l_pSourceDataFR += ulVectorSize;
		// apply volume									 
		vVectFR = AKSIMD_MUL_V4F32( vVectFR, vVolumes );

		AKSIMD_V4F32 vVectC = AKSIMD_LOAD_V4F32( l_pSourceDataC );
		l_pSourceDataC += ulVectorSize;
		// apply volume									 
		vVectC = AKSIMD_MUL_V4F32( vVectC, vVolumes );

		AKSIMD_V4F32 vVectLFE = AKSIMD_LOAD_V4F32( l_pSourceDataLFE );
		l_pSourceDataLFE += ulVectorSize;
		// apply volume									 
		vVectLFE = AKSIMD_MUL_V4F32( vVectLFE, vVolumes );

		AKSIMD_V4F32 vVectRL = AKSIMD_LOAD_V4F32( l_pSourceDataRL );
		l_pSourceDataRL += ulVectorSize;
		// apply volume									 
		vVectRL = AKSIMD_MUL_V4F32( vVectRL, vVolumes );

		AKSIMD_V4F32 vVectRR = AKSIMD_LOAD_V4F32( l_pSourceDataRR );
		l_pSourceDataRR += ulVectorSize;
		// apply volume									 
		vVectRR = AKSIMD_MUL_V4F32( vVectRR, vVolumes );

		AKSIMD_V4F32 vVectEL = AKSIMD_LOAD_V4F32( l_pSourceDataEL );
		l_pSourceDataEL += ulVectorSize;
		// apply volume									 
		vVectEL = AKSIMD_MUL_V4F32( vVectEL, vVolumes );

		AKSIMD_V4F32 vVectER = AKSIMD_LOAD_V4F32( l_pSourceDataER );
		l_pSourceDataER += ulVectorSize;
		// apply volume									 
		vVectER = AKSIMD_MUL_V4F32( vVectER, vVolumes );

		// increment the volumes
		vVolumes  = AKSIMD_ADD_V4F32( vVolumes,  vVolumesDelta );
//----------------------------------------------------------------------------------------------------
// Interleave the data
//
//  12 AKSIMD_SHUFFLE_V4F32()
//   6 AKSIMD_STORE_V4F32()
//----------------------------------------------------------------------------------------------------
		AkRegister AKSIMD_V4F32 vFrontLeftRight = AKSIMD_SHUFFLE_V4F32( vVectFL, vVectFR, AKSIMD_SHUFFLE( 1, 0, 1, 0 ) );	// FR1, FR0, FL1, FL0
		AkRegister AKSIMD_V4F32 vCenterLfe = AKSIMD_SHUFFLE_V4F32( vVectC, vVectLFE, AKSIMD_SHUFFLE( 1, 0, 1, 0 ) );			// LFE1, LFE0, C1, C0
		AkRegister AKSIMD_V4F32 vRearLeftRight = AKSIMD_SHUFFLE_V4F32( vVectRL, vVectRR, AKSIMD_SHUFFLE( 1, 0, 1, 0 ) );		// RR1, RR0, RL1, RL0
		AkRegister AKSIMD_V4F32 vExtraLeftRight = AKSIMD_SHUFFLE_V4F32( vVectEL, vVectER,  AKSIMD_SHUFFLE( 1, 0, 1, 0 ) );	// ER1, EL1, ER0, EL0

		// we want vDest1 = [ LFE0, C0, FR0, FL0 ]
		AkRegister AKSIMD_V4F32 vDest1 = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vCenterLfe, AKSIMD_SHUFFLE( 2, 0, 2, 0 ));
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ ER0, EL0, RR0, RL0  ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vRearLeftRight, vExtraLeftRight, AKSIMD_SHUFFLE( 2, 0, 2, 0 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ LFE1, C1, FR1, FL1 ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vCenterLfe, AKSIMD_SHUFFLE( 3, 1, 3, 1 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ ER1, EL1, RR1, RL1 ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vRearLeftRight, vExtraLeftRight, AKSIMD_SHUFFLE( 3, 1, 3, 1 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		vFrontLeftRight = AKSIMD_SHUFFLE_V4F32( vVectFL, vVectFR, AKSIMD_SHUFFLE( 3, 2, 3, 2 ) );					// FR3, FR2, FL3, FL2
		vCenterLfe = AKSIMD_SHUFFLE_V4F32( vVectC, vVectLFE, AKSIMD_SHUFFLE( 3, 2, 3, 2 ) );							// LFE3, LFE2, C3, C2
		vRearLeftRight = AKSIMD_SHUFFLE_V4F32( vVectRL, vVectRR, AKSIMD_SHUFFLE( 3, 2, 3, 2 ) );						// RR3, RR2, RL3, RL2
		vExtraLeftRight = AKSIMD_SHUFFLE_V4F32( vVectEL, vVectER,  AKSIMD_SHUFFLE( 3, 2, 3, 2 ) );					// ER3, EL2, ER3, EL2

		// we want vDest1 = [ LFE2, C2, FR2, FL2 ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vCenterLfe, AKSIMD_SHUFFLE( 2, 0, 2, 0 ));
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ ER2, EL2, RR2, RL2  ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vRearLeftRight, vExtraLeftRight, AKSIMD_SHUFFLE( 2, 0, 2, 0 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ LFE3, C3, FR3, FL3 ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vFrontLeftRight, vCenterLfe, AKSIMD_SHUFFLE( 3, 1, 3, 1 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;

		// we want vDest1 = [ ER3, EL3, RR3, RL3 ]
		vDest1 = AKSIMD_SHUFFLE_V4F32( vRearLeftRight, vExtraLeftRight, AKSIMD_SHUFFLE( 3, 1, 3, 1 ) );
		// store the vDest1 result
		AKSIMD_STORE_V4F32( pDestData, vDest1 );
		pDestData += ulVectorSize;
	}
}

//////////////////////////////////////////////////////////////////////////


void ApplyGainNInt16_V4F32(
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain)
{
	const AkUInt32 VecWidth = 4;

	AkReal32* AK_RESTRICT pSrc = (AkReal32*)in_pInputBuffer->GetInterleavedData();
	AkInt16* AK_RESTRICT pDest = (AkInt16*)in_pOutputBuffer->GetInterleavedData();
	const AkUInt32 uChannelCount = in_pOutputBuffer->GetChannelConfig().uNumChannels;
	const AkUInt32 uFrames = in_pInputBuffer->MaxFrames();

	if (in_gain.fPrev == in_gain.fNext)
	{
		const AkUInt32 uSteps = (uFrames * uChannelCount) / VecWidth;
		AkUInt32 uCurrentStep = 0;

		// If there is no gain delta we have an easy codepath
		AKSIMD_V4F32 vfGain = AKSIMD_SET_V4F32(in_gain.fPrev * DENORMALIZEFACTORI16);
		while (uCurrentStep < uSteps)
		{
			AKSIMD_V4F32 vfSrc[2] = {
				AKSIMD_LOAD_V4F32(pSrc + 0),
				AKSIMD_LOAD_V4F32(pSrc + 4)
			};

			AKSIMD_V4F32 vfDest[2] = {
				AKSIMD_MUL_V4F32(vfSrc[0], vfGain),
				AKSIMD_MUL_V4F32(vfSrc[1], vfGain)
			};

			AKSIMD_V4I32 viShorts = AKSIMD_PACKS_V4I32(
				AKSIMD_TRUNCATE_V4F32_TO_V4I32(vfDest[0]),
				AKSIMD_TRUNCATE_V4F32_TO_V4I32(vfDest[1])
			);
			AKSIMD_STORE_V4I32((AKSIMD_V4I32*)pDest, viShorts);

			pSrc += VecWidth * 2;
			pDest += VecWidth * 2;
			uCurrentStep += 2;
		}
	}
	else
	{
		AkRamp denormalizeGain = in_gain;
		denormalizeGain.fPrev *= DENORMALIZEFACTORI16;
		denormalizeGain.fNext *= DENORMALIZEFACTORI16;
		BUILD_VOLUME_DELTA_VECTOR(vBaseVolumes, vBaseVolumesDelta, denormalizeGain, uFrames);

		// multiply volumes delta by 2 because the loop is unrolled by 2.
		AKSIMD_V4F32 vVolumesDelta = AKSIMD_ADD_V4F32(vBaseVolumesDelta, vBaseVolumesDelta);

		const AkUInt32 uSteps = uFrames / VecWidth;
		AkUInt32 uCurrentChannel = 0;
		while (uCurrentChannel < uChannelCount)
		{
			AkUInt32 uCurrentStep = 0;
			AKSIMD_V4F32 vfGain[2] = {
				vBaseVolumes,
				AKSIMD_ADD_V4F32(vBaseVolumes, vBaseVolumesDelta)
			};

			while (uCurrentStep < uSteps)
			{
				AKSIMD_V4F32 vfSrc[2] = {
					AKSIMD_LOAD_V4F32(pSrc + 0),
					AKSIMD_LOAD_V4F32(pSrc + 4)
				};

				AKSIMD_V4F32 vfDest[2] = {
					AKSIMD_MUL_V4F32(vfSrc[0], vfGain[0]),
					AKSIMD_MUL_V4F32(vfSrc[1], vfGain[1])
				};

				AKSIMD_V4I32 viShorts = AKSIMD_PACKS_V4I32(
					AKSIMD_TRUNCATE_V4F32_TO_V4I32(vfDest[0]),
					AKSIMD_TRUNCATE_V4F32_TO_V4I32(vfDest[1])
				);
				AKSIMD_STORE_V4I32((AKSIMD_V4I32*)pDest, viShorts);

				pSrc += VecWidth * 2;
				pDest += VecWidth * 2;
				uCurrentStep += 2;

				vfGain[0] = AKSIMD_ADD_V4F32(vfGain[0], vVolumesDelta);
				vfGain[1] = AKSIMD_ADD_V4F32(vfGain[1], vVolumesDelta);
			}
			uCurrentChannel++;
		}
	}
}

void ApplyGainN_V4F32(
	AkAudioBuffer* in_pInputBuffer,
	AkAudioBuffer* in_pOutputBuffer,
	AkRamp in_gain)
{
	const AkUInt32 VecWidth = 4;

	const AkUInt32 uChannelCount = in_pOutputBuffer->GetChannelConfig().uNumChannels;
	const AkUInt32 uFrames = in_pInputBuffer->MaxFrames();
	const AkUInt32 uSteps = uFrames / VecWidth;

	if (in_gain.fPrev == in_gain.fNext)
	{
		// If there is no gain delta we have an easy codepath
		AKSIMD_V4F32 vfGain = AKSIMD_SET_V4F32(in_gain.fPrev);
		AkUInt32 uCurrentChannel = 0;
		while (uCurrentChannel < uChannelCount)
		{
			AkUInt32 uCurrentStep = 0;
			AkReal32* AK_RESTRICT pSrc = (AkReal32*)in_pInputBuffer->GetChannel(uCurrentChannel);
			AkReal32* AK_RESTRICT pDest = (AkReal32*)in_pOutputBuffer->GetChannel(uCurrentChannel);

			while (uCurrentStep < uSteps)
			{
				AKSIMD_V4F32 vfSrc[2] = {
					AKSIMD_LOAD_V4F32(pSrc + 0),
					AKSIMD_LOAD_V4F32(pSrc + 4)
				};
				AKSIMD_V4F32 vfDest[2] = {
					AKSIMD_MUL_V4F32(vfSrc[0], vfGain),
					AKSIMD_MUL_V4F32(vfSrc[1], vfGain)
				};
				AKSIMD_STORE_V4F32(pDest + 0, vfDest[0]);
				AKSIMD_STORE_V4F32(pDest + 4, vfDest[1]);

				pSrc += VecWidth * 2;
				pDest += VecWidth * 2;
				uCurrentStep += 2;
			}
			uCurrentChannel++;
		}
	}
	else
	{
		BUILD_VOLUME_DELTA_VECTOR(vBaseVolumes, vBaseVolumesDelta, in_gain, uFrames);

		// multiply volumes delta by 2 because the loop is unrolled by 2.
		AKSIMD_V4F32 vVolumesDelta = AKSIMD_ADD_V4F32(vBaseVolumesDelta, vBaseVolumesDelta);

		AkUInt32 uCurrentChannel = 0;
		while (uCurrentChannel < uChannelCount)
		{
			AkUInt32 uCurrentStep = 0;
			AKSIMD_V4F32 vfGain[2] = {
				vBaseVolumes,
				AKSIMD_ADD_V4F32(vBaseVolumes, vBaseVolumesDelta)
			};

			AkReal32* AK_RESTRICT pSrc = (AkReal32*)in_pInputBuffer->GetChannel(uCurrentChannel);
			AkReal32* AK_RESTRICT pDest = (AkReal32*)in_pOutputBuffer->GetChannel(uCurrentChannel);

			while (uCurrentStep < uSteps)
			{
				AKSIMD_V4F32 vfSrc[2] = {
					AKSIMD_LOAD_V4F32(pSrc + 0),
					AKSIMD_LOAD_V4F32(pSrc + 4)
				};

				AKSIMD_V4F32 vfDest[2] = {
					AKSIMD_MUL_V4F32(vfSrc[0], vfGain[0]),
					AKSIMD_MUL_V4F32(vfSrc[1], vfGain[1])
				};

				AKSIMD_STORE_V4F32(pDest + 0, vfDest[0]);
				AKSIMD_STORE_V4F32(pDest + 4, vfDest[1]);

				pSrc += VecWidth * 2;
				pDest += VecWidth * 2;
				uCurrentStep += 2;

				vfGain[0] = AKSIMD_ADD_V4F32(vfGain[0], vVolumesDelta);
				vfGain[1] = AKSIMD_ADD_V4F32(vfGain[1], vVolumesDelta);
			}
			uCurrentChannel++;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

// Assumes in_uNumSamples % 8 == 0
void AkMixer::MixChannelSIMD(
	AkReal32*	in_pSourceData,
	AkReal32*	in_pDestData,
	AkReal32	in_fVolume,
	AkReal32	in_fVolumeDelta,
	AkUInt32	in_uNumSamples
)
{
	AkReal32* AK_RESTRICT pSourceData = in_pSourceData;
	AkReal32* AK_RESTRICT pDestData = in_pDestData;
	AkReal32* AK_RESTRICT pSourceEnd = pSourceData + in_uNumSamples;

	if (in_fVolumeDelta == 0.0f)
	{
		if (in_fVolume == 0.0f)
		{
			//everything is done over a previous cleared buffer, so if volume=0 we have nothing to do
			return;
		}

		AKASSERT(!(in_uNumSamples % 8));
		BUILD_VOLUME_VECTOR(vVolumes, in_fVolume, 0.0f);
		do
		{
			// get eight samples								 
			AKSIMD_V4F32 vSum1 = AKSIMD_LOAD_V4F32(pSourceData);
			AKSIMD_V4F32 vSum2 = AKSIMD_LOAD_V4F32(pSourceData + ulVectorSize);
			pSourceData += ulVectorSize * 2;

			// apply volume
			vSum1 = AKSIMD_MUL_V4F32(vSum1, vVolumes);
			vSum2 = AKSIMD_MUL_V4F32(vSum2, vVolumes);

			// get the previous ones						 
			AKSIMD_V4F32 vOutput1 = AKSIMD_LOAD_V4F32(pDestData);
			AKSIMD_V4F32 vOutput2 = AKSIMD_LOAD_V4F32(pDestData + ulVectorSize);

			// add to output sample							 
			vSum1 = AKSIMD_ADD_V4F32(vOutput1, vSum1);
			vSum2 = AKSIMD_ADD_V4F32(vOutput2, vSum2);

			// store the result								 
			AKSIMD_STORE_V4F32(pDestData, vSum1);
			AKSIMD_STORE_V4F32(pDestData + ulVectorSize, vSum2);
			pDestData += ulVectorSize * 2;
		} while (pSourceData < pSourceEnd);
	}
	else // has volume delta
	{
		AKASSERT(!(in_uNumSamples % 8));
		AkReal32 fVolumesDelta = in_fVolumeDelta * 4;
		AKSIMD_V4F32 vVolumesDelta = AKSIMD_LOAD1_V4F32(fVolumesDelta);

		BUILD_VOLUME_VECTOR(vVolumes1, in_fVolume, in_fVolumeDelta);
		AKSIMD_V4F32 vVolumes2 = AKSIMD_ADD_V4F32(vVolumes1, vVolumesDelta);

		// multiply volumes delta by 2 because the loop is unrolled by 2.
		vVolumesDelta = AKSIMD_ADD_V4F32(vVolumesDelta, vVolumesDelta);

		do
		{
			// get eight samples								 
			AKSIMD_V4F32 vSum1 = AKSIMD_LOAD_V4F32(pSourceData);
			AKSIMD_V4F32 vSum2 = AKSIMD_LOAD_V4F32(pSourceData + ulVectorSize);
			pSourceData += ulVectorSize * 2;

			// apply volume
			vSum1 = AKSIMD_MUL_V4F32(vSum1, vVolumes1);
			vSum2 = AKSIMD_MUL_V4F32(vSum2, vVolumes2);

			// get the previous ones						 
			AKSIMD_V4F32 vOutput1 = AKSIMD_LOAD_V4F32(pDestData);
			AKSIMD_V4F32 vOutput2 = AKSIMD_LOAD_V4F32(pDestData + ulVectorSize);

			// add to output sample							 
			vSum1 = AKSIMD_ADD_V4F32(vOutput1, vSum1);
			vSum2 = AKSIMD_ADD_V4F32(vOutput2, vSum2);

			// store the result								 
			AKSIMD_STORE_V4F32(pDestData, vSum1);
			AKSIMD_STORE_V4F32(pDestData + ulVectorSize, vSum2);
			pDestData += ulVectorSize * 2;

			// in_fVolume += in_fVolumeDelta;
			vVolumes1 = AKSIMD_ADD_V4F32(vVolumes1, vVolumesDelta);
			vVolumes2 = AKSIMD_ADD_V4F32(vVolumes2, vVolumesDelta);
		} while (pSourceData < pSourceEnd);
	}
}