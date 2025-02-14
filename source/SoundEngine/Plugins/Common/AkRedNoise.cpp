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

// Red (or brown) noise or 1/f^2 noise:
// Simply using white noise and filtering it with one pole filter that has -6 dB per octave slope -20 dB per decade)

#include "AkRedNoise.h"

namespace DSP
{
	CAkRedNoise::CAkRedNoise()
	{
		RedFilter.SetCoefs( AKREDNOISE_B0COEF, AKREDNOISE_A1COEF );
	}

	void CAkRedNoise::GenerateBuffer(	
		AkReal32 * out_pfBuffer, 
		AkUInt32 in_uNumFrames )
	{
		CAkWhiteNoise::GenerateBuffer( out_pfBuffer, in_uNumFrames );
		RedFilter.ProcessBuffer( out_pfBuffer, in_uNumFrames );
	}

} // namespace DSP

