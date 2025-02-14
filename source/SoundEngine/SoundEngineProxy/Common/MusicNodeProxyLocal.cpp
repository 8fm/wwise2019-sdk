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

#include "MusicNodeProxyLocal.h"
#include "AkMusicNode.h"
#include "AkCritical.h"

void MusicNodeProxyLocal::MeterInfo(
		bool in_bIsOverrideParent,
        const AkMeterInfo& in_MeterInfo
        )
{
	CAkMusicNode* pMusicNode = static_cast<CAkMusicNode*>( GetIndexable() );
	if( pMusicNode )
	{
		pMusicNode->MeterInfo( in_bIsOverrideParent ? &in_MeterInfo : NULL );
	}
}

void MusicNodeProxyLocal::SetStingers( 
		CAkStinger* in_pStingers, 
		AkUInt32 in_NumStingers
		)
{
	CAkMusicNode* pMusicNode = static_cast<CAkMusicNode*>( GetIndexable() );
	if( pMusicNode )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pMusicNode->SetStingers( in_pStingers, in_NumStingers );
	}
}

void MusicNodeProxyLocal::SetOverride( 
	AkUInt8 in_eOverride, 
	AkUInt8 in_uValue
	)
{
	CAkMusicNode* pMusicNode = static_cast<CAkMusicNode*>( GetIndexable() );
	if( pMusicNode )
	{
		pMusicNode->SetOverride( (AkMusicOverride)in_eOverride, in_uValue != 0 );
	}
}

void MusicNodeProxyLocal::SetFlag( 
	AkUInt8 in_eFlag, 
	AkUInt8 in_uValue
	)
{
	CAkMusicNode* pMusicNode = static_cast<CAkMusicNode*>( GetIndexable() );
	if( pMusicNode )
	{
		pMusicNode->SetFlag( (AkMusicFlag)in_eFlag, in_uValue != 0 );
	}
}

#endif
#endif // #ifndef AK_OPTIMIZED
