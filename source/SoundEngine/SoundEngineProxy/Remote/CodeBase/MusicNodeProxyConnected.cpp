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

#include "MusicNodeProxyConnected.h"
#include "AkMusicNode.h"
#include "CommandDataSerializer.h"
#include "IMusicNodeProxy.h"

MusicNodeProxyConnected::MusicNodeProxyConnected()
{
}

MusicNodeProxyConnected::~MusicNodeProxyConnected()
{
}

void MusicNodeProxyConnected::HandleExecute( AkUInt16 in_uMethodID, CommandDataSerializer& in_rSerializer, CommandDataSerializer& out_rReturnSerializer )
{
	CAkMusicNode* pMusicNode = static_cast<CAkMusicNode*>( GetIndexable() );

	switch( in_uMethodID )
	{
	case IMusicNodeProxy::MethodMeterInfo:
		{
			MusicNodeProxyCommandData::MeterInfo meterInfo;
			if (in_rSerializer.Get( meterInfo ))
				pMusicNode->MeterInfo( meterInfo.m_bIsOverrideParent ? &meterInfo.m_MeterInfo : NULL );
			break;
		}

	case IMusicNodeProxy::MethodSetStingers:
		{
			MusicNodeProxyCommandData::SetStingers setStingers;
			if(in_rSerializer.Get( setStingers ))
				pMusicNode->SetStingers( setStingers.m_pStingers, setStingers.m_NumStingers );
			break;
		}

	case IMusicNodeProxy::MethodSetOverride:
		{
			MusicNodeProxyCommandData::SetOverride setOverride;
			if (in_rSerializer.Get( setOverride ))
				pMusicNode->SetOverride( (AkMusicOverride)setOverride.m_param1, setOverride.m_param2 != 0 );
			break;
		}

	case IMusicNodeProxy::MethodSetFlag:
		{
			MusicNodeProxyCommandData::SetFlag setFlag;
			if (in_rSerializer.Get( setFlag ))
				pMusicNode->SetFlag( (AkMusicFlag)setFlag.m_param1, setFlag.m_param2 != 0 );
			break;
		}

	default:
		__base::HandleExecute( in_uMethodID, in_rSerializer, out_rReturnSerializer );
	}
}
#endif // #ifndef AK_OPTIMIZED
