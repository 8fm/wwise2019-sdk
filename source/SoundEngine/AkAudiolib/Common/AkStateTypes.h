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
// AkState.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _STATE_TYPES_H_
#define _STATE_TYPES_H_

#include "AkPropBundle.h"

typedef AkUInt16 AkStatePropertyId; // property's Audio Engine ID (i.e. AkRtpcID)
									// must be 16-bits to support plugins

typedef AkPropBundle<AkReal32,AkStatePropertyId> AkStatePropBundle;

#endif
