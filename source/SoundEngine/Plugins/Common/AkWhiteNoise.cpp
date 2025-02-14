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

// Generates uniform probability white noise using linear congruential generator algorithm

#include "AkWhiteNoise.h"
#include <AK/Tools/Common/AkAssert.h>

namespace DSP
{
	void CAkWhiteNoise::GenerateBuffer(	
		AkReal32 * AK_RESTRICT out_pfBuffer, 
		AkUInt32 in_uNumFrames )
	{
		// PC vectorized version of this was slower
		// http://software.intel.com/en-us/articles/fast-random-number-generator-on-the-intel-pentiumr-4-processor/

		// Use scalar LCG algorithm (vectorized version was slower)
		{
			// Straight-up C++ version
			AkReal32 * AK_RESTRICT pfBuf = (AkReal32 * AK_RESTRICT) out_pfBuffer;
			const AkReal32 * pfEnd = (AkReal32 *) ( pfBuf + in_uNumFrames );
			while ( pfBuf < pfEnd )
			{
				*pfBuf++ = GenerateAudioSample();
			}
		}
	}
} // namespace DSP

