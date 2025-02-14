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

// Multi-tap delay
// Unit assumes that early reflection delay is handled outside and thus offset all tap times by
// minimum tap time so that the first reflection is output without delay
// For early reflections, the sum of all taps is used
// Limited to taps times smaller than 2^16-1 (and to the same number of taps) 
// For PIC usage, the whole delay line need to be present (assuming will need most of it by reading multiple taps)

#include "ERUnitDual.h"
#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/SoundEngine/Common/AkSimd.h>
#include <math.h>

#include <AK/Tools/Common/AkAssert.h>

namespace DSP
{
	AKRESULT ERUnitDual::Init(	AK::IAkPluginMemAlloc * in_pAllocator, 
		AkReal32 in_fRoomSizeScale,
		const AkRoomVerb::TapInfo * in_pTapInfoLeft, 
		const AkRoomVerb::TapInfo * in_pTapInfoRight, 
		AkUInt32 in_uNumTapsL,
		AkUInt32 in_uNumTapsR,
		AkUInt32 in_uSampleRate )
	{
		if ( (in_uNumTapsL == 0) || (in_uNumTapsL > NUMTAPS) ||
			(in_uNumTapsR == 0) || (in_uNumTapsR > NUMTAPS) )
		{
			return AK_Fail;
		}

		// Double or halve the length of the pattern bases on the size parameter (-100,100)
		AkReal32 fRoomSizeScaleFactor = powf( 2.f, in_fRoomSizeScale/100.f );
		AkReal32 fMinTime = AK_FPMin( in_pTapInfoLeft[0].fTapTime, in_pTapInfoRight[0].fTapTime ) * fRoomSizeScaleFactor;
		AkReal32 fMaxTime = AK_FPMax( in_pTapInfoLeft[in_uNumTapsL-1].fTapTime, in_pTapInfoRight[in_uNumTapsR-1].fTapTime ) * fRoomSizeScaleFactor;
		AkUInt32 u32DelayLineLength = (AkUInt32)((fMaxTime-fMinTime)/1000.f*in_uSampleRate);
		if ( u32DelayLineLength < 4 )
			u32DelayLineLength = 4;
		if ( u32DelayLineLength > 0xFFFF ) // Offsets stored on 16 bits
			return AK_Fail;
		uDelayLineLength = (AkUInt16)(u32DelayLineLength & ~0x03);

		pfDelay = (AkReal32*)AK_PLUGIN_ALLOC( in_pAllocator, AK_ALIGN_SIZE_FOR_DMA( sizeof(AkReal32) * uDelayLineLength ) );
		if ( pfDelay == NULL )
			return AK_InsufficientMemory;

		// Scale and convert tap times to tap lengths
		AkUInt16 uCurTap = 0;
		AkUInt16 uMaxOffset = 0;
		AkInt32 iPreviousDelaySize = -1; // Ensure no duplicate taps
		uIndexToNextWrappingTapL = 0;
		// Assumes tap times are sorted in ascending order
		// First effective tap delay will be 0
		for ( AkUInt16 i = 0; i < in_uNumTapsL; ++i )
		{

			AkUInt32 uDelaySize = (AkUInt32)((in_pTapInfoLeft[i].fTapTime*fRoomSizeScaleFactor-fMinTime)/1000.f*in_uSampleRate);
			uDelaySize &= ~0x3; // floor to 4-sample boundary
			if ( uDelaySize >= uDelayLineLength )
				uDelaySize = uDelayLineLength - 4;
			// Do not allow two ER taps at the same sample
			if ( uDelaySize != iPreviousDelaySize ) 
			{
				uTapOffsetsL[uCurTap] = (AkUInt16)(uDelayLineLength - uDelaySize);
				if ( uDelaySize == 0 )
					uTapOffsetsL[uCurTap] = 0; 

				fTapGainsL[uCurTap] = in_pTapInfoLeft[i].fTapGain;
				
				if ( uTapOffsetsL[uCurTap] > uMaxOffset )
				{
					uMaxOffset = uTapOffsetsL[uCurTap];
					uIndexToNextWrappingTapL = uCurTap;
				}
				uCurTap++;
			}
			iPreviousDelaySize = uDelaySize;
		}
		uNumTapsL = uCurTap;

		uCurTap = 0;
		uMaxOffset = 0;
		iPreviousDelaySize = -1; // Ensure no duplicate taps
		uIndexToNextWrappingTapR = 0;
		// Assumes tap times are sorted in ascending order
		// First effective tap delay will be 0
		for ( AkUInt16 i = 0; i < in_uNumTapsR; ++i )
		{
			AkUInt32 uDelaySize = (AkUInt32)((in_pTapInfoRight[i].fTapTime*fRoomSizeScaleFactor-fMinTime)/1000.f*in_uSampleRate);
			uDelaySize &= ~0x3; // floor to 4-sample boundary
			if ( uDelaySize >= uDelayLineLength )
				uDelaySize = uDelayLineLength - 4;
			// Do not allow two ER taps at the same sample
			if ( uDelaySize != iPreviousDelaySize ) 
			{
				uTapOffsetsR[uCurTap] = (AkUInt16)(uDelayLineLength - uDelaySize);
				if ( uDelaySize == 0 )
					uTapOffsetsR[uCurTap] = 0; 

				fTapGainsR[uCurTap] = in_pTapInfoRight[i].fTapGain;
				
				if ( uTapOffsetsR[uCurTap] > uMaxOffset )
				{
					uMaxOffset = uTapOffsetsR[uCurTap];
					uIndexToNextWrappingTapR = uCurTap;
				}
				uCurTap++;
			}
			iPreviousDelaySize = uDelaySize;
		}
		uNumTapsR = uCurTap;

		uWriteOffset = 0;

		return AK_Success;
	}

	void ERUnitDual::Term( AK::IAkPluginMemAlloc * in_pAllocator )
	{
		if ( pfDelay )
		{
			AK_PLUGIN_FREE( in_pAllocator, pfDelay );
			pfDelay = NULL;
		}
		uDelayLineLength = 0;
	}

	void ERUnitDual::Reset( )
	{
		if ( pfDelay )
		{
			memset( pfDelay, 0, sizeof(AkReal32) * uDelayLineLength );
		}
	}


#if (defined(AK_CPU_X86_64) || defined(AK_CPU_X86) || defined AK_CPU_ARM)
	void ERUnitDual::ProcessBuffer(	
		AkReal32 * in_pfInput,
		AkReal32 * out_pfEROutputL,
		AkReal32 * out_pfEROutputR,
		AkUInt32 in_uNumFrames
		)
	{
		AKASSERT( in_uNumFrames > 0 );

		AkReal32 * AK_RESTRICT pfDelayBuf = (AkReal32 * ) pfDelay;

		AkUInt32 uTapsUnrollBoth = AkMin( uNumTapsL, uNumTapsR ) / 4 * 4;

		// Minimum number of wraps
		AkUInt32 uFramesRemainingToProcess = in_uNumFrames;
		while ( uFramesRemainingToProcess )
		{
			AkUInt32 uERTapsFramesBeforeWrapL = uDelayLineLength - uTapOffsetsL[uIndexToNextWrappingTapL];
			AkUInt32 uERTapsFramesBeforeWrapR = uDelayLineLength - uTapOffsetsR[uIndexToNextWrappingTapR];
			AkUInt32 uWriteFramesBeforeWrap = uDelayLineLength - uWriteOffset;
			AkUInt32 uFramesBeforeWrap = AkMin( uERTapsFramesBeforeWrapL, uERTapsFramesBeforeWrapR );
			uFramesBeforeWrap = AkMin( uWriteFramesBeforeWrap, uFramesBeforeWrap );

			AkUInt32 uFramesToProcess = AkMin(uFramesRemainingToProcess,uFramesBeforeWrap);
			AkUInt32 i = uFramesToProcess;

			while (i>=4)
			{
				// Write input to delay
				AKSIMD_V4F32 vfIn = AKSIMD_LOAD_V4F32( in_pfInput );
				AKSIMD_STORE_V4F32( pfDelayBuf + uWriteOffset, vfIn );
				in_pfInput += 4;
				uWriteOffset += 4;

				AKSIMD_V4F32 vfSumL = AKSIMD_SETZERO_V4F32();
				AKSIMD_V4F32 vfSumR = AKSIMD_SETZERO_V4F32();

				AkUInt32 uBoth = 0;
				for ( ; uBoth < uTapsUnrollBoth; uBoth += 4 )
				{
					AkUInt32 uTapOffsetL0 = uTapOffsetsL[uBoth+0];
					AkUInt32 uTapOffsetL1 = uTapOffsetsL[uBoth+1];
					AkUInt32 uTapOffsetL2 = uTapOffsetsL[uBoth+2];
					AkUInt32 uTapOffsetL3 = uTapOffsetsL[uBoth+3];

					AkUInt32 uTapOffsetR0 = uTapOffsetsR[uBoth+0];
					AkUInt32 uTapOffsetR1 = uTapOffsetsR[uBoth+1];
					AkUInt32 uTapOffsetR2 = uTapOffsetsR[uBoth+2];
					AkUInt32 uTapOffsetR3 = uTapOffsetsR[uBoth+3];

					vfSumL = AKSIMD_MADD_V4F32( AKSIMD_LOAD_V4F32( pfDelayBuf + uTapOffsetL0 ), AKSIMD_LOAD1_V4F32( fTapGainsL[uBoth+0] ), vfSumL );
					vfSumL = AKSIMD_MADD_V4F32( AKSIMD_LOAD_V4F32( pfDelayBuf + uTapOffsetL1 ), AKSIMD_LOAD1_V4F32( fTapGainsL[uBoth+1] ), vfSumL );
					vfSumL = AKSIMD_MADD_V4F32( AKSIMD_LOAD_V4F32( pfDelayBuf + uTapOffsetL2 ), AKSIMD_LOAD1_V4F32( fTapGainsL[uBoth+2] ), vfSumL );
					vfSumL = AKSIMD_MADD_V4F32( AKSIMD_LOAD_V4F32( pfDelayBuf + uTapOffsetL3 ), AKSIMD_LOAD1_V4F32( fTapGainsL[uBoth+3] ), vfSumL );

					vfSumR = AKSIMD_MADD_V4F32( AKSIMD_LOAD_V4F32( pfDelayBuf + uTapOffsetR0 ), AKSIMD_LOAD1_V4F32( fTapGainsR[uBoth+0] ), vfSumR );
					vfSumR = AKSIMD_MADD_V4F32( AKSIMD_LOAD_V4F32( pfDelayBuf + uTapOffsetR1 ), AKSIMD_LOAD1_V4F32( fTapGainsR[uBoth+1] ), vfSumR );
					vfSumR = AKSIMD_MADD_V4F32( AKSIMD_LOAD_V4F32( pfDelayBuf + uTapOffsetR2 ), AKSIMD_LOAD1_V4F32( fTapGainsR[uBoth+2] ), vfSumR );
					vfSumR = AKSIMD_MADD_V4F32( AKSIMD_LOAD_V4F32( pfDelayBuf + uTapOffsetR3 ), AKSIMD_LOAD1_V4F32( fTapGainsR[uBoth+3] ), vfSumR );

					uTapOffsetsL[uBoth+0] = (AkUInt16) ( uTapOffsetL0 + 4 );
					uTapOffsetsL[uBoth+1] = (AkUInt16) ( uTapOffsetL1 + 4 );
					uTapOffsetsL[uBoth+2] = (AkUInt16) ( uTapOffsetL2 + 4 );
					uTapOffsetsL[uBoth+3] = (AkUInt16) ( uTapOffsetL3 + 4 );

					uTapOffsetsR[uBoth+0] = (AkUInt16) ( uTapOffsetR0 + 4 );
					uTapOffsetsR[uBoth+1] = (AkUInt16) ( uTapOffsetR1 + 4 );
					uTapOffsetsR[uBoth+2] = (AkUInt16) ( uTapOffsetR2 + 4 );
					uTapOffsetsR[uBoth+3] = (AkUInt16) ( uTapOffsetR3 + 4 );
				}
				// Read sum of all ER taps
				for ( AkUInt32 j = uBoth; j < uNumTapsL; ++j )
				{
					vfSumL = AKSIMD_MADD_V4F32( AKSIMD_LOAD_V4F32( pfDelayBuf + uTapOffsetsL[j] ), AKSIMD_LOAD1_V4F32( fTapGainsL[j] ), vfSumL );
					uTapOffsetsL[j] += 4;
				}

				for ( AkUInt32 j = uBoth; j < uNumTapsR; ++j )
				{
					vfSumR = AKSIMD_MADD_V4F32( AKSIMD_LOAD_V4F32( pfDelayBuf + uTapOffsetsR[j] ), AKSIMD_LOAD1_V4F32( fTapGainsR[j] ), vfSumR );
					uTapOffsetsR[j] += 4;
				}

				AKSIMD_STORE_V4F32( out_pfEROutputL, vfSumL );
				AKSIMD_STORE_V4F32( out_pfEROutputR, vfSumR );
				out_pfEROutputL += 4;
				out_pfEROutputR += 4;

				i-=4;
			}

			// Wrap taps if necessary
			if ( uTapOffsetsL[uIndexToNextWrappingTapL] == uDelayLineLength )
			{
				uTapOffsetsL[uIndexToNextWrappingTapL] = 0;
				uIndexToNextWrappingTapL = ++uIndexToNextWrappingTapL % uNumTapsL;
			}

			if ( uTapOffsetsR[uIndexToNextWrappingTapR] == uDelayLineLength )
			{
				uTapOffsetsR[uIndexToNextWrappingTapR] = 0;
				uIndexToNextWrappingTapR = ++uIndexToNextWrappingTapR % uNumTapsR;
			}

			if ( uWriteOffset == uDelayLineLength )
			{
				uWriteOffset = 0;
			}

			uFramesRemainingToProcess -= uFramesToProcess;
		}
	}
#elif defined(AKSIMD_V2F32_SUPPORTED)
void ERUnitDual::ProcessBuffer(	
		AkReal32 * in_pfInput,
		AkReal32 * out_pfEROutputL,
		AkReal32 * out_pfEROutputR,
		AkUInt32 in_uNumFrames
		)
	{
		AKASSERT( in_uNumFrames > 0 );

		AkReal32 * AK_RESTRICT pfDelayBuf = (AkReal32 * ) pfDelay;

		AkUInt32 uTapsUnrollBoth = AkMin( uNumTapsL, uNumTapsR ) / 4 * 4;

		AkPrefetchZero(out_pfEROutputL, in_uNumFrames * sizeof(AkReal32));
		AkPrefetchZero(out_pfEROutputR, in_uNumFrames * sizeof(AkReal32));

		// Minimum number of wraps
		AkUInt32 uFramesRemainingToProcess = in_uNumFrames;
		while ( uFramesRemainingToProcess )
		{
			AkUInt32 uERTapsFramesBeforeWrapL = uDelayLineLength - uTapOffsetsL[uIndexToNextWrappingTapL];
			AkUInt32 uERTapsFramesBeforeWrapR = uDelayLineLength - uTapOffsetsR[uIndexToNextWrappingTapR];
			AkUInt32 uWriteFramesBeforeWrap = uDelayLineLength - uWriteOffset;
			AkUInt32 uFramesBeforeWrap = AkMin( uERTapsFramesBeforeWrapL, uERTapsFramesBeforeWrapR );
			uFramesBeforeWrap = AkMin( uWriteFramesBeforeWrap, uFramesBeforeWrap );

			AkUInt32 uFramesToProcess = AkMin(uFramesRemainingToProcess,uFramesBeforeWrap);
			AkUInt32 i = uFramesToProcess;

			while (i>=4)
			{
				// Write input to delay
				AKSIMD_V2F32 vfIn = AKSIMD_LOAD_V2F32( in_pfInput );
				AKSIMD_STORE_V2F32( pfDelayBuf + uWriteOffset, vfIn );
				in_pfInput += 2;
				uWriteOffset += 2;
				vfIn = AKSIMD_LOAD_V2F32( in_pfInput );
				AKSIMD_STORE_V2F32( pfDelayBuf + uWriteOffset, vfIn );
				in_pfInput += 2;
				uWriteOffset += 2;

				AKSIMD_V2F32 vfSumL0 = AKSIMD_SETZERO_V2F32();
				AKSIMD_V2F32 vfSumR0 = AKSIMD_SETZERO_V2F32();
				AKSIMD_V2F32 vfSumL1 = AKSIMD_SETZERO_V2F32();
				AKSIMD_V2F32 vfSumR1 = AKSIMD_SETZERO_V2F32();

				AkUInt32 uBoth = 0;
				for ( ; uBoth < uTapsUnrollBoth; uBoth += 4 )
				{
					AkUInt32 uTapOffsetL0 = uTapOffsetsL[uBoth+0];
					AkUInt32 uTapOffsetL1 = uTapOffsetsL[uBoth+1];
					AkUInt32 uTapOffsetL2 = uTapOffsetsL[uBoth+2];
					AkUInt32 uTapOffsetL3 = uTapOffsetsL[uBoth+3];

					AkUInt32 uTapOffsetR0 = uTapOffsetsR[uBoth+0];
					AkUInt32 uTapOffsetR1 = uTapOffsetsR[uBoth+1];
					AkUInt32 uTapOffsetR2 = uTapOffsetsR[uBoth+2];
					AkUInt32 uTapOffsetR3 = uTapOffsetsR[uBoth+3];

					vfSumL0 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetL0 ), AKSIMD_SET_V2F32( fTapGainsL[uBoth+0] ), vfSumL0 );
					vfSumL0 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetL1 ), AKSIMD_SET_V2F32( fTapGainsL[uBoth+1] ), vfSumL0 );
					vfSumL0 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetL2 ), AKSIMD_SET_V2F32( fTapGainsL[uBoth+2] ), vfSumL0 );
					vfSumL0 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetL3 ), AKSIMD_SET_V2F32( fTapGainsL[uBoth+3] ), vfSumL0 );
					vfSumL1 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetL0+2 ), AKSIMD_SET_V2F32( fTapGainsL[uBoth+0] ), vfSumL1 );
					vfSumL1 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetL1+2 ), AKSIMD_SET_V2F32( fTapGainsL[uBoth+1] ), vfSumL1 );
					vfSumL1 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetL2+2 ), AKSIMD_SET_V2F32( fTapGainsL[uBoth+2] ), vfSumL1 );
					vfSumL1 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetL3+2 ), AKSIMD_SET_V2F32( fTapGainsL[uBoth+3] ), vfSumL1 );

					vfSumR0 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetR0 ), AKSIMD_SET_V2F32( fTapGainsR[uBoth+0] ), vfSumR0 );
					vfSumR0 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetR1 ), AKSIMD_SET_V2F32( fTapGainsR[uBoth+1] ), vfSumR0 );
					vfSumR0 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetR2 ), AKSIMD_SET_V2F32( fTapGainsR[uBoth+2] ), vfSumR0 );
					vfSumR0 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetR3 ), AKSIMD_SET_V2F32( fTapGainsR[uBoth+3] ), vfSumR0 );
					vfSumR1 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetR0+2 ), AKSIMD_SET_V2F32( fTapGainsR[uBoth+0] ), vfSumR1 );
					vfSumR1 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetR1+2 ), AKSIMD_SET_V2F32( fTapGainsR[uBoth+1] ), vfSumR1 );
					vfSumR1 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetR2+2 ), AKSIMD_SET_V2F32( fTapGainsR[uBoth+2] ), vfSumR1 );
					vfSumR1 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetR3+2 ), AKSIMD_SET_V2F32( fTapGainsR[uBoth+3] ), vfSumR1 );

					uTapOffsetsL[uBoth+0] = (AkUInt16) ( uTapOffsetL0 + 4 );
					uTapOffsetsL[uBoth+1] = (AkUInt16) ( uTapOffsetL1 + 4 );
					uTapOffsetsL[uBoth+2] = (AkUInt16) ( uTapOffsetL2 + 4 );
					uTapOffsetsL[uBoth+3] = (AkUInt16) ( uTapOffsetL3 + 4 );

					uTapOffsetsR[uBoth+0] = (AkUInt16) ( uTapOffsetR0 + 4 );
					uTapOffsetsR[uBoth+1] = (AkUInt16) ( uTapOffsetR1 + 4 );
					uTapOffsetsR[uBoth+2] = (AkUInt16) ( uTapOffsetR2 + 4 );
					uTapOffsetsR[uBoth+3] = (AkUInt16) ( uTapOffsetR3 + 4 );
				}

				// Read sum of all ER taps
				for ( AkUInt32 j = uBoth; j < uNumTapsL; ++j )
				{
					vfSumL0 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetsL[j] ), AKSIMD_SET_V2F32( fTapGainsL[j] ), vfSumL0 );
					uTapOffsetsL[j] += 2;
					vfSumL1 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetsL[j] ), AKSIMD_SET_V2F32( fTapGainsL[j] ), vfSumL1 );
					uTapOffsetsL[j] += 2;					
				}

				for ( AkUInt32 j = uBoth; j < uNumTapsR; ++j )
				{
					vfSumR0 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetsR[j] ), AKSIMD_SET_V2F32( fTapGainsR[j] ), vfSumR0 );
					uTapOffsetsR[j] += 2;
					vfSumR1 = AKSIMD_MADD_V2F32( AKSIMD_LOAD_V2F32( pfDelayBuf + uTapOffsetsR[j] ), AKSIMD_SET_V2F32( fTapGainsR[j] ), vfSumR1 );
					uTapOffsetsR[j] += 2;		
				}

				AKSIMD_STORE_V2F32( out_pfEROutputL, vfSumL0 );
				out_pfEROutputL += 2;
				AKSIMD_STORE_V2F32( out_pfEROutputL, vfSumL1 );
				out_pfEROutputL += 2;

				AKSIMD_STORE_V2F32( out_pfEROutputR, vfSumR0 );
				out_pfEROutputR += 2;
				AKSIMD_STORE_V2F32( out_pfEROutputR, vfSumR1 );
				out_pfEROutputR += 2;

				i-=4;
			}

			// Wrap taps if necessary
			if ( uTapOffsetsL[uIndexToNextWrappingTapL] == uDelayLineLength )
			{
				uTapOffsetsL[uIndexToNextWrappingTapL] = 0;
				uIndexToNextWrappingTapL = ++uIndexToNextWrappingTapL % uNumTapsL;
			}

			if ( uTapOffsetsR[uIndexToNextWrappingTapR] == uDelayLineLength )
			{
				uTapOffsetsR[uIndexToNextWrappingTapR] = 0;
				uIndexToNextWrappingTapR = ++uIndexToNextWrappingTapR % uNumTapsR;
			}

			if ( uWriteOffset == uDelayLineLength )
			{
				uWriteOffset = 0;
			}

			uFramesRemainingToProcess -= uFramesToProcess;
		}
	}
#else
	void ERUnitDual::ProcessBuffer(	
		AkReal32 * in_pfInput,
		AkReal32 * out_pfEROutputL,
		AkReal32 * out_pfEROutputR,
		AkUInt32 in_uNumFrames
		)
	{
		AKASSERT( in_uNumFrames > 0 );

		AkReal32 * AK_RESTRICT pfDelayBuf = (AkReal32 * ) pfDelay;
		AkReal32 * AK_RESTRICT pfBuf = (AkReal32 * ) in_pfInput;

		AkUInt32 uTapsUnrollBoth = AkMin( uNumTapsL, uNumTapsR ) / 4 * 4;

		// Minimum number of wraps
		AkUInt32 uFramesRemainingToProcess = in_uNumFrames;
		while ( uFramesRemainingToProcess )
		{
			AkUInt32 uERTapsFramesBeforeWrapL = uDelayLineLength - uTapOffsetsL[uIndexToNextWrappingTapL];
			AkUInt32 uERTapsFramesBeforeWrapR = uDelayLineLength - uTapOffsetsR[uIndexToNextWrappingTapR];
			AkUInt32 uWriteFramesBeforeWrap = uDelayLineLength - uWriteOffset;
			AkUInt32 uFramesBeforeWrap = AkMin( uERTapsFramesBeforeWrapL, uERTapsFramesBeforeWrapR );
			uFramesBeforeWrap = AkMin( uWriteFramesBeforeWrap, uFramesBeforeWrap );

			AkUInt32 uFramesToProcess = AkMin(uFramesRemainingToProcess,uFramesBeforeWrap);
			AkUInt32 i = uFramesToProcess;

			while( i-- )
			{
				// Write input to delay
				pfDelayBuf[uWriteOffset] = *pfBuf++;
				uWriteOffset++;

				AkReal32 fSumL = 0.f;
				AkReal32 fSumR = 0.f;

				AkUInt32 uBoth = 0;

				for ( ; uBoth < uTapsUnrollBoth; uBoth += 4 )
				{
					AkUInt32 uTapOffsetL0 = uTapOffsetsL[uBoth+0];
					AkUInt32 uTapOffsetL1 = uTapOffsetsL[uBoth+1];
					AkUInt32 uTapOffsetL2 = uTapOffsetsL[uBoth+2];
					AkUInt32 uTapOffsetL3 = uTapOffsetsL[uBoth+3];

					AkUInt32 uTapOffsetR0 = uTapOffsetsR[uBoth+0];
					AkUInt32 uTapOffsetR1 = uTapOffsetsR[uBoth+1];
					AkUInt32 uTapOffsetR2 = uTapOffsetsR[uBoth+2];
					AkUInt32 uTapOffsetR3 = uTapOffsetsR[uBoth+3];

					fSumL += pfDelayBuf[uTapOffsetL0] * fTapGainsL[uBoth+0];
					fSumL += pfDelayBuf[uTapOffsetL1] * fTapGainsL[uBoth+1];
					fSumL += pfDelayBuf[uTapOffsetL2] * fTapGainsL[uBoth+2];
					fSumL += pfDelayBuf[uTapOffsetL3] * fTapGainsL[uBoth+3];

					fSumR += pfDelayBuf[uTapOffsetR0] * fTapGainsR[uBoth+0];
					fSumR += pfDelayBuf[uTapOffsetR1] * fTapGainsR[uBoth+1];
					fSumR += pfDelayBuf[uTapOffsetR2] * fTapGainsR[uBoth+2];
					fSumR += pfDelayBuf[uTapOffsetR3] * fTapGainsR[uBoth+3];

					uTapOffsetsL[uBoth+0] = (AkUInt16) ( uTapOffsetL0 + 1 );
					uTapOffsetsL[uBoth+1] = (AkUInt16) ( uTapOffsetL1 + 1 );
					uTapOffsetsL[uBoth+2] = (AkUInt16) ( uTapOffsetL2 + 1 );
					uTapOffsetsL[uBoth+3] = (AkUInt16) ( uTapOffsetL3 + 1 );

					uTapOffsetsR[uBoth+0] = (AkUInt16) ( uTapOffsetR0 + 1 );
					uTapOffsetsR[uBoth+1] = (AkUInt16) ( uTapOffsetR1 + 1 );
					uTapOffsetsR[uBoth+2] = (AkUInt16) ( uTapOffsetR2 + 1 );
					uTapOffsetsR[uBoth+3] = (AkUInt16) ( uTapOffsetR3 + 1 );
				}

				// Read sum of all ER taps
				for ( AkUInt32 j = uBoth; j < uNumTapsL; ++j )
				{
					fSumL += pfDelayBuf[uTapOffsetsL[j]] * fTapGainsL[j];
					uTapOffsetsL[j]++;
				}

				for ( AkUInt32 j = uBoth; j < uNumTapsR; ++j )
				{
					fSumR += pfDelayBuf[uTapOffsetsR[j]] * fTapGainsR[j];
					uTapOffsetsR[j]++;
				}
				*out_pfEROutputL++ = fSumL;
				*out_pfEROutputR++ = fSumR;
			}

			// Wrap taps if necessary
			if ( uTapOffsetsL[uIndexToNextWrappingTapL] == uDelayLineLength )
			{
				uTapOffsetsL[uIndexToNextWrappingTapL] = 0;
				uIndexToNextWrappingTapL = ++uIndexToNextWrappingTapL % uNumTapsL;
			}

			if ( uTapOffsetsR[uIndexToNextWrappingTapR] == uDelayLineLength )
			{
				uTapOffsetsR[uIndexToNextWrappingTapR] = 0;
				uIndexToNextWrappingTapR = ++uIndexToNextWrappingTapR % uNumTapsR;
			}

			if ( uWriteOffset == uDelayLineLength )
			{
				uWriteOffset = 0;
			}

			uFramesRemainingToProcess -= uFramesToProcess;
		}
	}
#endif
}
