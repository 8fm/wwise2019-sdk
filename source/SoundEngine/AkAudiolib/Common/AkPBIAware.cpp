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

//////////////////////////////////////////////////////////////////////
//
// AkPBIAware.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include "AkPBIAware.h"
#include "AudiolibDefs.h"
#include "AkContinuousPBI.h"

CAkPBIAware::~CAkPBIAware()
{
}

CAkPBI* CAkPBIAware::CreatePBI(	CAkSoundBase*		in_pSound,
							    CAkSource*			in_pSource,
                                AkPBIParams&		in_rPBIParams,
                                const PriorityInfoCurrent& in_rPriority
								) const
{
    AKASSERT( in_rPBIParams.eType == AkPBIParams::PBI );
	return AkNew( AkMemID_Object, CAkPBI( 
		in_rPBIParams,
		in_pSound,
		in_pSource,
		in_rPriority,
		0
		)
		);
}
