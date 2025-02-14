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

#include "ObjectProxyLocal.h"
#include "IEventProxy.h"

class EventProxyLocal : public ObjectProxyLocal
						, virtual public IEventProxy
{
public:
	EventProxyLocal( AkUniqueID in_id );
	virtual ~EventProxyLocal();

	// IEventProxy members
	virtual void Add( AkUniqueID in_actionID );
	virtual void Remove( AkUniqueID in_actionID );
	virtual void Clear();
	virtual void SetPlayDirectly( bool in_bPlayDirectly );
};

#endif // #ifndef PROXYCENTRAL_CONNECTED
#endif // #ifndef AK_OPTIMIZED
