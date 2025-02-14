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

class IStateProxy : virtual public IObjectProxy
{
	DECLARE_BASECLASS( IObjectProxy );
public:
    virtual void SetAkProp( AkPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fDefault ) = 0;

	enum MethodIDs
	{
		MethodSetAkProp = __base::LastMethodID,

		LastMethodID
	};
};

namespace StateProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData3
	< 
		IStateProxy::MethodSetAkProp,
		AkUInt32, // AkPropID
		AkReal32,
		AkReal32
	> SetAkProp;
}

#endif // #ifndef AK_OPTIMIZED
