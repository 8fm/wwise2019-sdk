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

#ifndef _INTERNALPITCHSTATE_H_
#define _INTERNALPITCHSTATE_H_

#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include "AkSettings.h"

// Slew time for pitch parameter to reach its target (in samples)
static const AkUInt32 PITCHRAMPLENGTH = 1024;
#define PITCHRAMPBASERATE DEFAULT_NATIVE_FREQUENCY

// Fixed point interpolation index
#define FPBITS 16
#define FPMUL (1<<FPBITS)
#define FPMASK (FPMUL-1)
#define SINGLEFRAMEDISTANCE (FPMUL)

// Class members used at runtime required by all platforms grouped together
struct AkInternalPitchState
{
	// Note: Keep this first to guarantee alignement on 16 bytes
	union
	{
		AkReal32	fLastValueStatic[AK_STANDARD_MAX_NUM_CHANNELS];
		AkInt16		iLastValueStatic[AK_STANDARD_MAX_NUM_CHANNELS];
	};

	union									// (In/Out) Last buffer values depending on format 
	{
		AkReal32 *	fLastValue;
		AkInt16	 *	iLastValue;
	};

	// Pitch internal state
	AkUInt32 uInFrameOffset;				// (In/Out) Offset in buffer currently being read
	AkUInt32 uOutFrameOffset;				// (In/Out) Offset in buffer currently being filled

	AkUInt32 uFloatIndex;					// (In/Out) Fixed point index value
	AkUInt32 uCurrentFrameSkip;				// (In/Out) Current sample frame skip
	AkUInt32 uTargetFrameSkip;				// (In) Target frame skip
	AkUInt32 uInterpolationRampCount;		// (In/Out) Sample count for pitch interpolation (interpolation finished when == PITCHRAMPLENGTH)
	AkUInt32 uInterpolationRampInc;			// (In) increment to ramp count during interpolate pitch transition
	AkUInt32 uRequestedFrames;				// (In) Desired output frames (max frames)

	AkUInt8  uChannelMappingStatic[AK_STANDARD_MAX_NUM_CHANNELS];  // (In) Channel remapping to apply when resampling. Format: Input Channel Index -> Output Channel Index
	AkUInt8* uChannelMapping;
	// Platform does not define a strict number of channels.
	bool bLastValuesAllocated;
	
} AK_ALIGN_DMA;

#endif // _INTERNALPITCHSTATE_H_
