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

#include "VirtualAcousticsProxyConnected.h"
#include "AkAudioLib.h"
#include "CommandDataSerializer.h"
#include "AkAttenuationMgr.h"
#include "IAttenuationProxy.h"
#include "IVirtualAcousticsProxy.h"
#include "AkVirtualAcousticsManager.h"

VirtualAcousticsProxyConnected::VirtualAcousticsProxyConnected(AkUniqueID in_id, AkVirtualAcousticsType in_type)
{
	CAkIndexable* pIndexable = AK::SoundEngine::GetIndexable( in_id, AkIdxType_VirtualAcoustics );
	if ( !pIndexable )
		pIndexable = CAkVirtualAcoustics::Create(in_id, in_type);

	SetIndexable( pIndexable );
}

VirtualAcousticsProxyConnected::~VirtualAcousticsProxyConnected()
{
}

void VirtualAcousticsProxyConnected::HandleExecute( AkUInt16 in_uMethodID, CommandDataSerializer& in_rSerializer, CommandDataSerializer& /*out_rReturnSerializer*/ )
{
	CAkVirtualAcoustics * pVirtualAcoustics = static_cast<CAkVirtualAcoustics *>(GetIndexable());

	switch( in_uMethodID )
	{
	case IVirtualAcousticsProxy::MethodSetAkPropF:
		{
			VirtualAcousticsProxyCommandData::SetAkPropF setAkProp;
			if( in_rSerializer.Get( setAkProp ) )
				pVirtualAcoustics->SetAkProp((AkVirtualAcousticsPropID)setAkProp.m_param1, setAkProp.m_param2, setAkProp.m_param3, setAkProp.m_param4);
			break;
		}

	case IVirtualAcousticsProxy::MethodSetAkPropI:
		{
			VirtualAcousticsProxyCommandData::SetAkPropI setAkProp;
			if( in_rSerializer.Get( setAkProp ) )
				pVirtualAcoustics->SetAkProp((AkVirtualAcousticsPropID)setAkProp.m_param1, setAkProp.m_param2, setAkProp.m_param3, setAkProp.m_param4);
			break;
		}

	default:
		AKASSERT( false );
	}
}



AcousticTextureProxyConnected::AcousticTextureProxyConnected(AkUniqueID in_id)
	: VirtualAcousticsProxyConnected(in_id, AkVirtualAcousticsType_Texture)
{}

AcousticTextureProxyConnected::~AcousticTextureProxyConnected()
{}


#endif // #ifndef AK_OPTIMIZED
