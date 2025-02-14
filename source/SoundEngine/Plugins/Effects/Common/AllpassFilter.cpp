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

// Allpass filter with feedforward and feedback memories interleaved
// y[n] = g * x[n] + x[n - D] - g * y[n - D]
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


#include "AllPassFilter.h"
#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <string.h>
#include <AK/Tools/Common/AkAssert.h>

namespace DSP
{

	AKRESULT AllpassFilter::Init( AK::IAkPluginMemAlloc * in_pAllocator, AkUInt32 in_uDelayLineLength, AkReal32 in_fG )
	{
		//at least one sample delay
		if ( in_uDelayLineLength < 1 )
			in_uDelayLineLength = 1;
		uDelayLineLength = in_uDelayLineLength; 
		pfDelay = (AkReal32*)AK_PLUGIN_ALLOC( in_pAllocator, AK_ALIGN_SIZE_FOR_DMA( 2 * sizeof(AkReal32) * uDelayLineLength ) );
		if ( pfDelay == NULL )
			return AK_InsufficientMemory;

		uCurOffset = 0;
		SetGain( in_fG );
		return AK_Success;
	}

	void AllpassFilter::Term( AK::IAkPluginMemAlloc * in_pAllocator )
	{
		if ( pfDelay )
		{
			AK_PLUGIN_FREE( in_pAllocator, pfDelay );
			pfDelay = NULL;
		}
		uDelayLineLength = 0;
	}

	void AllpassFilter::Reset( )
	{
		if ( pfDelay )
		{
			memset( pfDelay, 0, 2 * sizeof(AkReal32) * uDelayLineLength );
		}
	}

	// Process delay buffer in place when access to direct memory is ok (not PIC)
	void AllpassFilter::ProcessBuffer( AkReal32 * io_pfBuffer, AkUInt32 in_uNumFrames )
	{
		AkReal32 * AK_RESTRICT pfDelayPtr = (AkReal32 * ) &pfDelay[uCurOffset*2];
		AkReal32 * AK_RESTRICT pfBuf = (AkReal32 * ) io_pfBuffer;
		AkUInt32 uFramesBeforeWrap = uDelayLineLength - uCurOffset;
		if ( uFramesBeforeWrap > in_uNumFrames )
		{
			// Not wrapping this time
			AkUInt32 i = in_uNumFrames;

			while ( i-- )
			{
				AkReal32 fXn = *pfBuf;
				AkReal32 fFwdMem = *pfDelayPtr;		
				*pfDelayPtr++ = fXn;
				AkReal32 fFbkMem = *pfDelayPtr;
				AkReal32 fYn = fG * ( fXn - fFbkMem) + fFwdMem;
				*pfDelayPtr++ = fYn;
				*pfBuf++ = fYn;
			} 

			uCurOffset += in_uNumFrames;
			AKASSERT( uCurOffset <= uDelayLineLength );
		}
		else
		{
			// Minimum number of wraps
			AkUInt32 uFramesRemainingToProcess = in_uNumFrames;
			do
			{
				AkUInt32 uFramesToProcess = AkMin(uFramesRemainingToProcess, uFramesBeforeWrap);

				for ( AkUInt32 i = 0; i < uFramesToProcess; ++i )
				{
					AkReal32 fXn = *pfBuf;
					AkReal32 fFwdMem = *pfDelayPtr;		
					*pfDelayPtr++ = fXn;
					AkReal32 fFbkMem = *pfDelayPtr;
					AkReal32 fYn = fG * ( fXn - fFbkMem) + fFwdMem;
					*pfDelayPtr++ = fYn;
					*pfBuf++ = fYn;
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
			while ( uFramesRemainingToProcess );
		}
	}

	// Process delay buffer out-of-place when access to direct memory is ok (not PIC)
	void AllpassFilter::ProcessBuffer(	AkReal32 * in_pfInBuffer, 
		AkReal32 * out_pfOutBuffer, 
		AkUInt32 in_uNumFrames )
	{
		AkReal32 * AK_RESTRICT pfDelayPtr = (AkReal32 * ) &pfDelay[uCurOffset*2];
		AkReal32 * AK_RESTRICT pfBufIn = (AkReal32 * ) in_pfInBuffer;
		AkReal32 * AK_RESTRICT pfBufOut = (AkReal32 * ) out_pfOutBuffer;
		AkUInt32 uFramesBeforeWrap = uDelayLineLength - uCurOffset;
		if ( uFramesBeforeWrap > in_uNumFrames )
		{
			// Not wrapping this time
			AkUInt32 i = in_uNumFrames;
			while ( i-- )
			{
				AkReal32 fXn = *pfBufIn++;
				AkReal32 fFwdMem = *pfDelayPtr;		
				*pfDelayPtr++ = fXn;
				AkReal32 fFbkMem = *pfDelayPtr;
				AkReal32 fYn = fG * ( fXn - fFbkMem) + fFwdMem;
				*pfDelayPtr++ = fYn;
				*pfBufOut++ = fYn;
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
					AkReal32 fXn = *pfBufIn++;
					AkReal32 fFwdMem = *pfDelayPtr;		
					*pfDelayPtr++ = fXn;
					AkReal32 fFbkMem = *pfDelayPtr;
					AkReal32 fYn = fG * ( fXn - fFbkMem) + fFwdMem;
					*pfDelayPtr++ = fYn;
					*pfBufOut++ = fYn;
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
}
