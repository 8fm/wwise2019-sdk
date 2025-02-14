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

#ifndef __AK_FLOAT16__
#define __AK_FLOAT16__

#include <AK/SoundEngine/Common/AkTypes.h>

//#define AKFLOAT16_UNITTEST

typedef AkUInt16 AkReal16;

namespace AkFloat16
{
	void AkReal32ToReal16( AkReal32 *in_pf32Src, AkReal16 *out_pf16Dst, AkUInt32 in_uNumFrames );
	void AkReal16ToReal32( AkReal16 *in_pf16Src, AkReal32 * out_pf32Dst, AkUInt32 in_uNumFrames );
#ifdef AKFLOAT16_UNITTEST
	void UnitTestConversion( );
#endif
}

#endif // __AK_FLOAT16__
