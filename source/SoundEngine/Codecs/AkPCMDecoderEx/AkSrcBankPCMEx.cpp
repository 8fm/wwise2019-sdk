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

#include "stdafx.h"
#include "AkSrcBankPCMEx.h"

#include <AK/Plugin/AkPCMExFactory.h>

// Plugin mechanism. Dynamic create function whose address must be registered to the FX manager.
AKPCMEXDECODER_API IAkSoftwareCodec* CreatePCMExBankPlugin(void* in_pCtx)
{
	AKASSERT( false && "Not Implemented" );
	return NULL; // Not implemented
}
