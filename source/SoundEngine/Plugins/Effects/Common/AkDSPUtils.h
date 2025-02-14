/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

//////////////////////////////////////////////////////////////////////
//
// AkDSPUtils.h
//
// DSP Utils header file.
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_DSPUTILS_H_
#define _AK_DSPUTILS_H_

#include <AK/SoundEngine/Common/AkTypes.h>

void AkForceInline RemoveDenormal( AkReal32 & fVal )
{
	// if the whole calculation is done in the FPU registers, 
	// a 80-bit arithmetic may be used, with 64-bit mantissas. The
	// anti_denormal value should therefore be 2^64 times higher than 
	// FLT_MIN. On the other hand, if everything is done with a 32-bit accuracy, 
	// one may reduce the anti_denormal value to get a better
	// accuracy for the small values. (FLT_MIN == 1.175494351e-38F) 
}

#endif //_AK_DSPUTILS_H_
