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

// Simple delay y[n] = x[n - D] 
// Works for delays smaller than number of frames to process (wraps multiple times) if used as below:
// Non PIC usage 
// -> ProcessBuffer( pInOutData, uFramesToProcess )
// PIC usage for delay line > uFramesToProcess 
// -> DMA delay line from current position single DMA sufficient for less than 4096 frames
// -> CurrentPos is the start of first DMA + Offset because DMA was 16 bytes aligned
// -> Wrap pos is the second DMA buffer received (address aligned so no offset to handle)
// PIC usage for delay line <= uFramesToProcess 
// -> DMA delay line from its start (address aligned so no offset to handle)
// -> CurrentPos is the start of first DMA + uCurOffset (no DMA offset necessary)
// -> Wrap pos is same DMA buffer (address aligned so no offset to handle)

// Also supports single sample process

#include "DelayLine.h"
#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <string.h>
#include <AK/Tools/Common/AkAssert.h>

namespace DSP
{
	AKRESULT DelayLine::Init( AK::IAkPluginMemAlloc * in_pAllocator, AkUInt32 in_uDelayLineLength )
	{
		//at least four sample delay
		if ( in_uDelayLineLength < 4 )
			in_uDelayLineLength = 4;
		uDelayLineLength = in_uDelayLineLength; 
		AkUInt32 uSize = sizeof(AkReal32) * ((uDelayLineLength + 3) & ~3); // Round size to make sure unaligned simd accesses don't corrupt memory
		pfDelay = (AkReal32*)AK_PLUGIN_ALLOC( in_pAllocator, AK_ALIGN_SIZE_FOR_DMA(uSize));
		if ( pfDelay == NULL )
			return AK_InsufficientMemory;

		uCurOffset = 0;
		return AK_Success;
	}

	void DelayLine::Term( AK::IAkPluginMemAlloc * in_pAllocator )
	{
		if ( pfDelay )
		{
			AK_PLUGIN_FREE( in_pAllocator, pfDelay );
			pfDelay = NULL;
		}
		uDelayLineLength = 0;
	}

	void DelayLine::Reset( )
	{
		if ( pfDelay )
		{
			AkZeroMemLarge( pfDelay, AK_ALIGN_SIZE_FOR_DMA( sizeof(AkReal32) * uDelayLineLength ) );
		}
	}

#if defined(AKSIMD_V4F32_SUPPORTED)

	// Process delay buffer in place when access to direct memory is ok (not PIC)
	void DelayLine::ProcessBuffer( AkReal32 * io_pfBuffer, AkUInt32 in_uNumFrames )
	{
		AkReal32 * AK_RESTRICT pfDelayPtr = (AkReal32 * ) &pfDelay[uCurOffset];
		AkReal32 * AK_RESTRICT pfBuf = (AkReal32 * ) io_pfBuffer;

		while ( in_uNumFrames )
		{
			AkUInt32 uFramesBeforeWrap = uDelayLineLength - uCurOffset;
			AkUInt32 uFramesToProcess = AkMin( in_uNumFrames, uFramesBeforeWrap );
			AkUInt32 uNumSimdIter = uFramesToProcess >> 2; // divide by 4
			AkUInt32 uNumScalarIter = uFramesToProcess & 3; // modulo 4

			for ( AkUInt32 i = 0; i < uNumSimdIter; ++i )
			{
				AKSIMD_V4F32 vfDelay = AKSIMD_LOADU_V4F32( pfDelayPtr );
				AKSIMD_V4F32 vfBuf = AKSIMD_LOADU_V4F32( pfBuf );
				AKSIMD_STOREU_V4F32( pfDelayPtr, vfBuf );
				AKSIMD_STOREU_V4F32( pfBuf, vfDelay );
				pfDelayPtr += 4;
				pfBuf += 4;
			}

			for ( AkUInt32 i = 0; i < uNumScalarIter; ++i )
			{
				AkReal32 fDelay = *pfDelayPtr;
				*pfDelayPtr++ = *pfBuf;
				*pfBuf++ = fDelay;
			}

			uCurOffset += uFramesToProcess;
			AKASSERT( uCurOffset <= uDelayLineLength );

			if ( uCurOffset == uDelayLineLength )
			{
				pfDelayPtr = (AkReal32 * ) pfDelay;
				uCurOffset = 0;
			}

			in_uNumFrames -= uFramesToProcess;
		}
	}

	// Process delay buffer out-of-place when access to direct memory is ok (not PIC)
	void DelayLine::ProcessBuffer(	
		AkReal32 * in_pfInBuffer, 
		AkReal32 * out_pfOutBuffer, 
		AkUInt32 in_uNumFrames )
	{
		AkReal32 * AK_RESTRICT pfDelayPtr = (AkReal32 * ) &pfDelay[uCurOffset];
		AkReal32 * AK_RESTRICT pfInBuf = (AkReal32 * ) in_pfInBuffer;
		AkReal32 * AK_RESTRICT pfOutBuf = (AkReal32 * ) out_pfOutBuffer;

		while ( in_uNumFrames )
		{
			AkUInt32 uFramesBeforeWrap = uDelayLineLength - uCurOffset;
			AkUInt32 uFramesToProcess = AkMin( in_uNumFrames, uFramesBeforeWrap );
			AkUInt32 uNumSimdIter = uFramesToProcess >> 2;
			AkUInt32 uNumScalarIter = uFramesToProcess & 3;

			for ( AkUInt32 i = 0; i < uNumSimdIter; ++i )
			{
				AKSIMD_V4F32 vfDelay = AKSIMD_LOADU_V4F32( pfDelayPtr );
				AKSIMD_V4F32 vfBuf = AKSIMD_LOADU_V4F32( pfInBuf );
				AKSIMD_STOREU_V4F32( pfDelayPtr, vfBuf );
				AKSIMD_STOREU_V4F32( pfOutBuf, vfDelay );
				pfDelayPtr += 4;
				pfInBuf += 4;
				pfOutBuf += 4;
			}

			for ( AkUInt32 i = 0; i < uNumScalarIter; ++i )
			{
				AkReal32 fDelay = *pfDelayPtr;
				*pfDelayPtr++ = *pfInBuf++;
				*pfOutBuf++ = fDelay;
			}

			uCurOffset += uFramesToProcess;
			AKASSERT( uCurOffset <= uDelayLineLength );

			if ( uCurOffset == uDelayLineLength )
			{
				pfDelayPtr = (AkReal32 * ) pfDelay;
				uCurOffset = 0;
			}

			in_uNumFrames -= uFramesToProcess;
		}
	}

#else // defined(AKSIMD_V4F32_SUPPORTED)

	// Process delay buffer in place when access to direct memory is ok (not PIC)
	void DelayLine::ProcessBuffer( AkReal32 * io_pfBuffer, AkUInt32 in_uNumFrames )
	{
		AkReal32 * AK_RESTRICT pfDelayPtr = (AkReal32 * ) &pfDelay[uCurOffset];
		AkReal32 * AK_RESTRICT pfBuf = (AkReal32 * ) io_pfBuffer;
		AkUInt32 uFramesBeforeWrap = uDelayLineLength - uCurOffset;
		if ( uFramesBeforeWrap > in_uNumFrames )
		{
			// Not wrapping this time
			AkUInt32 i = in_uNumFrames;
			while ( i-- )
			{
				AkReal32 fDelay = *pfDelayPtr;		
				*pfDelayPtr++ = *pfBuf;			
				*pfBuf++ = fDelay;
			}
			uCurOffset += in_uNumFrames;
			AKASSERT( uCurOffset <= uDelayLineLength );
		}
		else
		{
			// Minimum number of wraps
			AkUInt32 uFramesRemainingToProcess = in_uNumFrames;
			while ( uFramesRemainingToProcess )
			{
				AkUInt32 uFramesToProcess = AkMin(uFramesRemainingToProcess,uFramesBeforeWrap);
				for ( AkUInt32 i = 0; i < uFramesToProcess; ++i )
				{
					AkReal32 fDelay = *pfDelayPtr;
					*pfDelayPtr++ = *pfBuf;
					*pfBuf++ = fDelay;
				}

				uCurOffset += uFramesToProcess;
				AKASSERT( uCurOffset <= uDelayLineLength );

				if ( uCurOffset == uDelayLineLength )
				{
					pfDelayPtr = (AkReal32 * ) pfDelay;
					uCurOffset = 0;
				}

				uFramesRemainingToProcess -= uFramesToProcess;
				uFramesBeforeWrap = uDelayLineLength - uCurOffset;
			}
		}
	}

	// Process delay buffer out-of-place when access to direct memory is ok (not PIC)
	void DelayLine::ProcessBuffer(	
		AkReal32 * in_pfInBuffer, 
		AkReal32 * out_pfOutBuffer, 
		AkUInt32 in_uNumFrames )
	{

		AkReal32 * AK_RESTRICT pfDelayPtr = (AkReal32 * ) &pfDelay[uCurOffset];
		AkReal32 * AK_RESTRICT pfBufIn = (AkReal32 * ) in_pfInBuffer;
		AkReal32 * AK_RESTRICT pfBufOut = (AkReal32 * ) out_pfOutBuffer;
		AkUInt32 uFramesBeforeWrap = uDelayLineLength - uCurOffset;
		if ( uFramesBeforeWrap > in_uNumFrames )
		{
			// Not wrapping this time
			AkUInt32 i = in_uNumFrames;
			while ( i-- )
			{
				AkReal32 fDelay = *pfDelayPtr;
				*pfDelayPtr++ = *pfBufIn++;	
				*pfBufOut++ = fDelay;
			}
			uCurOffset += in_uNumFrames;
			AKASSERT( uCurOffset <= uDelayLineLength );
		}
		else
		{
			// Minimum number of wraps
			AkUInt32 uFramesRemainingToProcess = in_uNumFrames;
			while ( uFramesRemainingToProcess )
			{
				AkUInt32 uFramesToProcess = AkMin(uFramesRemainingToProcess,uFramesBeforeWrap);
				for ( AkUInt32 i = 0; i < uFramesToProcess; ++i )
				{
					AkReal32 fDelay = *pfDelayPtr;
					*pfDelayPtr++ = *pfBufIn++;
					*pfBufOut++ = fDelay;
				}
				uCurOffset += uFramesToProcess;
				AKASSERT( uCurOffset <= uDelayLineLength );

				if ( uCurOffset == uDelayLineLength )
				{
					pfDelayPtr = (AkReal32 * ) pfDelay;
					uCurOffset = 0;
				}

				uFramesRemainingToProcess -= uFramesToProcess;
				uFramesBeforeWrap = uDelayLineLength - uCurOffset;
			}
		}
	}

#endif // defined(AKSIMD_V4F32_SUPPORTED)
}