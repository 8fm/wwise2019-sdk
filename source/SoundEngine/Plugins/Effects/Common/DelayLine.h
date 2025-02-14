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

#ifndef _AKDELAYLINE_H_
#define _AKDELAYLINE_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>
#include <AK/SoundEngine/Common/AkSimd.h>

namespace DSP
{

	class DelayLine
	{
	public:

		DelayLine() : 
			uDelayLineLength( 0 ), 
			pfDelay( NULL ), 
			uCurOffset( 0 )
			{}

		AKRESULT Init( AK::IAkPluginMemAlloc * in_pAllocator, AkUInt32 in_uDelayLineLength );
		void Term( AK::IAkPluginMemAlloc * in_pAllocator );
		void Reset( );
		void ProcessBuffer( AkReal32 * io_pfBuffer, AkUInt32 in_uNumFrames ); // In-place
		void ProcessBuffer(	AkReal32 * in_pfInBuffer, 
			AkReal32 * out_pfOutBuffer, 
			AkUInt32 in_uNumFrames ); // Out-of-place

		AkReal32 ProcessSample( AkReal32 in_fIn )
		{
			AkReal32 fDelay = pfDelay[uCurOffset];		
			pfDelay[uCurOffset] = in_fIn;
			++uCurOffset;
			if ( uCurOffset == uDelayLineLength )
				uCurOffset = 0;
			return fDelay;
		}

		AkReal32 ReadSample( )
		{
			return pfDelay[uCurOffset];
		}

		void WriteSample( AkReal32 in_fIn )
		{
			pfDelay[uCurOffset] = in_fIn;
			++uCurOffset;
			if ( uCurOffset == uDelayLineLength )
				uCurOffset = 0;
		}

		void WriteSampleNoWrapCheck( AkReal32 in_fIn )
		{
			pfDelay[uCurOffset] = in_fIn;
			++uCurOffset;
		}

		AkReal32 ReadSample( AkUInt32 in_curOffset )
		{
			return pfDelay[in_curOffset];
		}

		void WriteSample( AkReal32 in_fIn, AkUInt32 & in_curOffset )
		{
			pfDelay[in_curOffset] = in_fIn;
			++in_curOffset;
			if ( in_curOffset == uDelayLineLength )
				in_curOffset = 0;
		}

		void WriteSampleNoWrapCheck( AkReal32 in_fIn, AkUInt32 & in_curOffset )
		{
			pfDelay[in_curOffset] = in_fIn;
			++in_curOffset;
		}

		void WrapDelayLine()
		{
			if ( uCurOffset == uDelayLineLength )
			{
				uCurOffset = 0;
			}
		}

#if defined(AKSIMD_V2F32_SUPPORTED)
		void ReadSamples( AKSIMD_V2F32 &out_Low, AKSIMD_V2F32 &out_High )
		{
			out_Low = AKSIMD_LOAD_V2F32( pfDelay + uCurOffset );
			out_High = AKSIMD_LOAD_V2F32( pfDelay + uCurOffset + 2 );
		}
#elif defined(AKSIMD_V4F32_SUPPORTED)
		void ReadSamples( AKSIMD_V4F32 &out )
		{
			out = AKSIMD_LOADU_V4F32( pfDelay + uCurOffset );
		}
#endif

#if defined(AKSIMD_V2F32_SUPPORTED)
		void WriteSamplesNoWrapCheck(AKSIMD_V2F32 in_vfIn)
		{
			AKSIMD_STORE_V2F32( pfDelay + uCurOffset, in_vfIn );
			uCurOffset += 2;
		}
#elif defined(AKSIMD_V4F32_SUPPORTED)
		void WriteSamplesNoWrapCheck(AKSIMD_V4F32 in_vfIn)
		{
			AKSIMD_STOREU_V4F32( pfDelay + uCurOffset, in_vfIn );
			uCurOffset += 4;
		}
#endif

		AkUInt32 GetDelayLength( )
		{
			return uDelayLineLength;
		}

//	protected:

		AkUInt32 uDelayLineLength;		// DelayLine line length
		AkReal32 * pfDelay;				// DelayLine line start position
		AkUInt32 uCurOffset;			// DelayLine line read/write position	
	};

} // namespace DSP

#endif // _AKDELAYLINE_H_
