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
#pragma once

#ifndef AK_OPTIMIZED
#ifndef PROXYCENTRAL_CONNECTED

#include "ModulatorProxyLocal.h"

class ModulatorTimeProxyLocal : public ModulatorProxyLocal
{
public:
	ModulatorTimeProxyLocal( AkUniqueID in_id )
		: ModulatorProxyLocal( IModulatorProxy::ProxyType_Time, in_id )
	{}
	virtual ~ModulatorTimeProxyLocal() {}
};

#endif
#endif // #ifndef AK_OPTIMIZED
