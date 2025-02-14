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

// Some math approximation that may be useful to speed things up (particularly on PowerPC based architectures)
// WARNING: You should always ensure that the error that these routines yield for the input range you are using is acceptable

#include "math.h"
#include "float.h"
static const float shift23 = (1<<23);
static const float OOshift23 = 1.f / (1<<23);


AkForceInline AkReal32 Log2Approx(AkReal32 fVal)
{
	static const AkReal32 fOneOverLog2 = 1.442695040888963f;
	return logf(fVal) * fOneOverLog2; 
}

AkForceInline AkReal32 Pow2Approx(AkReal32 fVal)
{
	return powf(2.f,fVal);
}

AkForceInline AkReal32 PowApprox(AkReal32 fBase, AkReal32 fExp)
{
	return powf(fBase,fExp);
}

AkForceInline AkReal32 ExpApprox(AkReal32 fVal)
{
	return expf(fVal);
}

AkForceInline AkReal32 SqrtApprox(AkReal32 fVal)
{
	return sqrtf(fVal);
}
