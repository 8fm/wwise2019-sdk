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

#include <math.h>
#include <AK/Tools/Common/AkAssert.h>

static const AkReal32 FLOATMIN = -1.f;
static const AkReal32 FLOATMAX = 1.f;

#ifdef SYNTHONE_SIMD
#include "AkSynthOneSimd.inl"
#else
#include "AkSynthOneScalar.inl"
#endif
