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
#include "IVirtualAcousticsProxy.h"

class VirtualAcousticsProxyLocal : public ObjectProxyLocal
	, virtual public IVirtualAcousticsProxy
{
public:
	VirtualAcousticsProxyLocal(IVirtualAcousticsProxy::ProxyType in_proxyType, AkUniqueID in_id);
	virtual ~VirtualAcousticsProxyLocal();

	virtual void SetAkProp(AkVirtualAcousticsPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax);
	virtual void SetAkProp(AkVirtualAcousticsPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax);
};

class AcousticTextureProxyLocal : public VirtualAcousticsProxyLocal
{
public:
	AcousticTextureProxyLocal(AkUniqueID in_id)
		: VirtualAcousticsProxyLocal(IVirtualAcousticsProxy::ProxyType_Texture, in_id)
	{}
	virtual ~AcousticTextureProxyLocal() {}
};

#endif
#endif // #ifndef AK_OPTIMIZED
