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
#include "IFxBaseProxy.h"

class FxBaseProxyConnected : public ObjectProxyConnected
{
public:
	FxBaseProxyConnected(AkUniqueID in_id, FxBaseProxyType in_eType);
	virtual ~FxBaseProxyConnected();

	virtual void HandleExecute( AkUInt16 in_uMethodID, CommandDataSerializer& in_rSerializer, CommandDataSerializer& out_rReturnSerializer );

private:
	void SetSource( char* in_pszSource );

	DECLARE_BASECLASS( ObjectProxyConnected );
};
#endif // #ifndef AK_OPTIMIZED
