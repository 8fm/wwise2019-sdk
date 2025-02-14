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

#include "IObjectProxy.h"
#include "AkPrivateTypes.h"

class IEventProxy : virtual public IObjectProxy
{
	DECLARE_BASECLASS( IObjectProxy );
public:
	virtual void Add( AkUniqueID in_actionID ) = 0;
	virtual void Remove( AkUniqueID in_actionID ) = 0;
	virtual void Clear() = 0;
	virtual void SetPlayDirectly( bool in_bPlayDirectly ) = 0;

	enum MethodIDs
	{
		MethodAdd = __base::LastMethodID,
		MethodRemove,
		MethodClear,
		MethodSetPlayDirectly,
		LastMethodID
	};
};

namespace EventProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1
	< 
		IEventProxy::MethodAdd,
		AkUniqueID
	> Add;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IEventProxy::MethodRemove,
		AkUniqueID
	> Remove;

	typedef ObjectProxyCommandData::CommandData0
	< 
		IEventProxy::MethodClear
	> Clear;
	
	typedef ObjectProxyCommandData::CommandData1
	< 
		IEventProxy::MethodSetPlayDirectly,
		bool
	> SetPlayDirectly;
}

#endif // #ifndef AK_OPTIMIZED
