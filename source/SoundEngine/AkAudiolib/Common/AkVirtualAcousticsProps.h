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


#ifndef _AK_VIRTUALACOUSTICS_PROPS_
#define _AK_VIRTUALACOUSTICS_PROPS_

#include "AkPrivateTypes.h"

enum AkVirtualAcousticsPropID
{
	AkAcousticTexturePropID_AbsorptionOffset,
	AkAcousticTexturePropID_AbsorptionLow,
	AkAcousticTexturePropID_AbsorptionMidLow,
	AkAcousticTexturePropID_AbsorptionMidHigh,
	AkAcousticTexturePropID_AbsorptionHigh,
	AkAcousticTexturePropID_Scattering,

	AkVirtualAcousticsPropID_NUM
};

#ifdef DEFINE_VIRTUAL_ACOUSTICS_PROP_DEFAULTS

extern const AkPropValue g_AkVirtualAcousticsPropDefault[AkVirtualAcousticsPropID_NUM] =
{
	0.f, //	AkAcousticTexturePropID_AbsorptionOffset,
	0.f, //	AkAcousticTexturePropID_AbsorptionLow,
	0.f, //	AkAcousticTexturePropID_AbsorptionMidLow,
	0.f, //	AkAcousticTexturePropID_AbsorptionMidHigh,
	0.f, //	AkAcousticTexturePropID_AbsorptionHigh,
	0.f //	AkAcousticTexturePropID_Scattering
};

#ifdef AKPROP_TYPECHECK
#include <typeinfo>
extern const std::type_info * g_AkVirtualAcousticsPropTypeInfo[AkVirtualAcousticsPropID_NUM] =
{	// The lifetime of the object returned by typeid extends to the end of the program.

	&typeid(AkReal32), //AkAcousticTexturePropID_AbsorptionOffset,
	&typeid(AkReal32), //AkAcousticTexturePropID_AbsorptionLow,
	&typeid(AkReal32), //AkAcousticTexturePropID_AbsorptionMidLow,
	&typeid(AkReal32), //AkAcousticTexturePropID_AbsorptionMidHigh,
	&typeid(AkReal32), //AkAcousticTexturePropID_AbsorptionHigh,
	&typeid(AkReal32), //AkAcousticTexturePropID_Scattering,
};

#endif
#else

extern const AkPropValue g_AkVirtualAcousticsPropDefault[AkVirtualAcousticsPropID_NUM];

#ifdef AKPROP_TYPECHECK
#include <typeinfo>
extern const std::type_info * g_AkVirtualAcousticsPropTypeInfo[AkVirtualAcousticsPropID_NUM];
#endif

#endif


#endif
