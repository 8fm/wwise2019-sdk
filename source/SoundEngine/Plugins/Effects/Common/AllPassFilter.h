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

#ifndef _AKALLPASSFILTER_H_
#define _AKALLPASSFILTER_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>

namespace DSP
{
	class AllpassFilter
	{
	public:

		AllpassFilter() : 
			uDelayLineLength( 0 ), 
			pfDelay( NULL ), 
			uCurOffset( 0 ),
			fG( 0.f ) {}
		  AKRESULT Init( AK::IAkPluginMemAlloc * in_pAllocator, AkUInt32 in_uDelayLineLength, AkReal32 in_fG );
		  void Term( AK::IAkPluginMemAlloc * in_pAllocator );
		  void Reset( );
		  void ProcessBuffer( AkReal32 * io_pfBuffer, AkUInt32 in_uNumFrames ); // In-place
		  void ProcessBuffer(	
			  AkReal32 * in_pfInBuffer, 
			  AkReal32 * out_pfOutBuffer, 
			  AkUInt32 in_uNumFrames ); // Out-of-place
		  void SetGain( AkReal32 in_fG )
		  {
			  fG = in_fG;
		  }

		AkUInt32 GetDelayLength( )
		{
			return uDelayLineLength;
		}

	protected:

		AkUInt32 uDelayLineLength;		// DelayLine line length
		AkReal32 * pfDelay;				// DelayLine line start position
		AkUInt32 uCurOffset;			// DelayLine line read/write position	
		AkReal32 fG;
	};

} // namespace DSP

#endif // _AKALLPASSFILTER_H_
