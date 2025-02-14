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

#ifndef _AKERUNITDUAL_H_
#define _AKERUNITDUAL_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>

#ifdef AK_APPLE
#include <string.h>
#endif

#include <AK/Tools/Common/AkPlatformFuncs.h>

namespace AkRoomVerb
{
	struct TapInfo
	{
		AkReal32 fTapTime; // ms
		AkReal32 fTapGain; // linear amplitude
	};
}

namespace DSP
{
	class ERUnitDual
	{
	public:

		static const AkUInt32 NUMTAPS = 64;

		ERUnitDual() : 
			uDelayLineLength( 0 ), 
			pfDelay( NULL ),
			uWriteOffset( 0 ),
			uIndexToNextWrappingTapL( 0 ), 
			uIndexToNextWrappingTapR( 0 ), 
			uNumTapsL( 0 ),
			uNumTapsR( 0 )
		{
			AKPLATFORM::AkMemSet( &fTapGainsL, 0, sizeof(AkReal32)*NUMTAPS );
			AKPLATFORM::AkMemSet(&fTapGainsR, 0, sizeof(AkReal32)*NUMTAPS);
			AKPLATFORM::AkMemSet(&uTapOffsetsL, 0, sizeof(AkUInt16)*NUMTAPS);
			AKPLATFORM::AkMemSet(&uTapOffsetsR, 0, sizeof(AkUInt16)*NUMTAPS);
		}
		// Note: Assumes tap times are sorted in increasing order
		AKRESULT Init(	
			AK::IAkPluginMemAlloc * in_pAllocator, 
			AkReal32 in_fRoomSizeScale,
			const AkRoomVerb::TapInfo * in_pTapInfoLeft, 
			const AkRoomVerb::TapInfo * in_pTapInfoRight, 
			AkUInt32 in_uNumTapsL,
			AkUInt32 in_uNumTapsR,
			AkUInt32 in_uSampleRate );
		void Term( AK::IAkPluginMemAlloc * in_pAllocator );
		void Reset( );

		void ProcessBuffer(	
			AkReal32 * in_pfInput,
			AkReal32 * out_pfEROutputL,
			AkReal32 * out_pfEROutputR,
			AkUInt32 in_uNumFrames
			);

		AkUInt32 GetDelayLength( )
		{
			return uDelayLineLength;
		}

	protected:

		AkUInt32 uDelayLineLength;		// DelayLine line length    
		AkReal32 * pfDelay;				// DelayLine line start position
		AkUInt32 uWriteOffset;			// DelayLine line write position
		AkReal32 fTapGainsL[NUMTAPS];	
		AkReal32 fTapGainsR[NUMTAPS];	
		AkUInt16 uTapOffsetsL[NUMTAPS];
		AkUInt16 uTapOffsetsR[NUMTAPS];
		AkUInt16 uIndexToNextWrappingTapL;
		AkUInt16 uIndexToNextWrappingTapR;
		AkUInt16 uNumTapsL;
		AkUInt16 uNumTapsR;
	} AK_ALIGN_DMA;

} // namespace DSP

#endif // _AKERUNITDUAL_H_
