/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

#include "stdafx.h"
#ifndef AK_OPTIMIZED
#ifndef PROXYCENTRAL_CONNECTED

//////////////////////////////////////////////////////////////////////
//
// LayerProxyLocal.cpp
//
//////////////////////////////////////////////////////////////////////


#include "LayerProxyLocal.h"

#include "AkLayer.h"
#include "AkAudioLib.h"
#include "AkRegistryMgr.h"
#include "AkCritical.h"


LayerProxyLocal::LayerProxyLocal( AkUniqueID in_id )
{
	CAkIndexable* pIndexable = AK::SoundEngine::GetIndexable( in_id, AkIdxType_Layer );

	SetIndexable( pIndexable != NULL ? pIndexable : CAkLayer::Create( in_id ) );
}

void LayerProxyLocal::SetRTPC(
	AkRtpcID in_RTPC_ID,
	AkRtpcType in_RTPCType,
	AkRtpcAccum in_RTPCAccum,
	AkRTPC_ParameterID in_ParamID,
	AkUniqueID in_RTPCCurveID,
	AkCurveScaling in_eScaling,
	AkRTPCGraphPoint* in_pArrayConversion,
	AkUInt32 in_ulConversionArraySize
)
{
	CAkLayer* pLayer = static_cast<CAkLayer*>( GetIndexable() );
	if( pLayer )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pLayer->SetRTPC( in_RTPC_ID, in_RTPCType, in_RTPCAccum, in_ParamID, in_RTPCCurveID, in_eScaling, in_pArrayConversion, in_ulConversionArraySize );
	}
}

void LayerProxyLocal::UnsetRTPC(
	AkRTPC_ParameterID in_ParamID,
	AkUniqueID in_RTPCCurveID
)
{
	CAkLayer* pLayer = static_cast<CAkLayer*>( GetIndexable() );
	if( pLayer )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pLayer->UnsetRTPC( in_ParamID, in_RTPCCurveID );
	}
}

void LayerProxyLocal::SetChildAssoc(
	AkUniqueID in_ChildID,
	AkRTPCGraphPoint* in_pCrossfadingCurve,	// NULL if none
	AkUInt32 in_ulCrossfadingCurveSize		// 0 if none
)
{
	CAkLayer* pLayer = static_cast<CAkLayer*>( GetIndexable() );
	if ( pLayer )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pLayer->SetChildAssoc( in_ChildID, in_pCrossfadingCurve, in_ulCrossfadingCurveSize );
	}
}

void LayerProxyLocal::UnsetChildAssoc(
	AkUniqueID in_ChildID 
)
{
	CAkLayer* pLayer = static_cast<CAkLayer*>( GetIndexable() );
	if ( pLayer )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pLayer->UnsetChildAssoc( in_ChildID );
	}
}

void LayerProxyLocal::SetCrossfadingRTPC(
	AkRtpcID in_rtpcID,
	AkRtpcType in_rtpcType
)
{
	CAkLayer* pLayer = static_cast<CAkLayer*>( GetIndexable() );
	if ( pLayer )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pLayer->SetCrossfadingRTPC( in_rtpcID, in_rtpcType );
	}
}

#endif
#endif // #ifndef AK_OPTIMIZED
