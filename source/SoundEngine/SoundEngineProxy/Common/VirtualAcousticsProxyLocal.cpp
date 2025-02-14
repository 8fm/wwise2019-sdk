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
#ifndef AK_OPTIMIZED
#ifndef PROXYCENTRAL_CONNECTED

#include "VirtualAcousticsProxyLocal.h"

#include "AkAudioLib.h"
#include "AkVirtualAcousticsManager.h"
#include "AkCritical.h"


VirtualAcousticsProxyLocal::VirtualAcousticsProxyLocal(IVirtualAcousticsProxy::ProxyType in_proxyType, AkUniqueID in_id)
{
	AKASSERT(in_proxyType == IVirtualAcousticsProxy::ProxyType_Texture);

	CAkIndexable* pIndexable = AK::SoundEngine::GetIndexable( in_id, AkIdxType_VirtualAcoustics );
	SetIndexable(pIndexable != NULL ? pIndexable : CAkVirtualAcoustics::Create(in_id, AkVirtualAcousticsType_Texture));
}

VirtualAcousticsProxyLocal::~VirtualAcousticsProxyLocal()
{
}

void VirtualAcousticsProxyLocal::SetAkProp(AkVirtualAcousticsPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax)
{
	CAkVirtualAcoustics* pIndexable = static_cast<CAkVirtualAcoustics*>(GetIndexable());
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetAkProp( in_eProp, in_fValue, in_fMin, in_fMax );
	}
}

void VirtualAcousticsProxyLocal::SetAkProp(AkVirtualAcousticsPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax)
{
	CAkVirtualAcoustics* pIndexable = static_cast<CAkVirtualAcoustics*>(GetIndexable());
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetAkProp( in_eProp, in_iValue, in_iMin, in_iMax );
	}
}

#endif
#endif // #ifndef AK_OPTIMIZED
