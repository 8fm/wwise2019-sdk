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

#include "ObjectProxyConnected.h"
#include "AkVirtualAcousticsManager.h"

class VirtualAcousticsProxyConnected : public ObjectProxyConnected
{
public:
	VirtualAcousticsProxyConnected(AkUniqueID in_id, AkVirtualAcousticsType in_type);
	virtual ~VirtualAcousticsProxyConnected();

	virtual void HandleExecute( AkUInt16 in_uMethodID, CommandDataSerializer& in_rSerializer, CommandDataSerializer& out_rReturnSerializer );

private:
	DECLARE_BASECLASS( ObjectProxyConnected );
};

class AcousticTextureProxyConnected : public VirtualAcousticsProxyConnected
{
public:
	AcousticTextureProxyConnected(AkUniqueID in_id);
	virtual ~AcousticTextureProxyConnected();

private:
	DECLARE_BASECLASS(VirtualAcousticsProxyConnected);
};

#endif // #ifndef AK_OPTIMIZED
