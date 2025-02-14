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
#include "AkRTPC.h"
#include "AkVirtualAcousticsProps.h"

class IVirtualAcousticsProxy : virtual public IObjectProxy
{
	DECLARE_BASECLASS( IObjectProxy );
public:
	virtual void SetAkProp(AkVirtualAcousticsPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax) = 0;
	virtual void SetAkProp(AkVirtualAcousticsPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax) = 0;

	enum ProxyType
	{
		ProxyType_Texture = 0
	};

	enum MethodIDs
	{
		MethodSetAkPropF = __base::LastMethodID,
		MethodSetAkPropI,

		LastMethodID
	};
};

namespace VirtualAcousticsProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData4
		< 
		IVirtualAcousticsProxy::MethodSetAkPropF,
		AkUInt32, // AkPropID
		AkReal32,
		AkReal32,
		AkReal32
		> SetAkPropF;

	typedef ObjectProxyCommandData::CommandData4
		< 
		IVirtualAcousticsProxy::MethodSetAkPropI,
		AkUInt32, // AkPropID
		AkInt32,
		AkInt32,
		AkInt32
		> SetAkPropI;
}

#endif // #ifndef AK_OPTIMIZED
