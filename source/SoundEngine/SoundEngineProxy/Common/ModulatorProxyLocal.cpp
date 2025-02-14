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

#include "ModulatorProxyLocal.h"

#include "AkAudioLib.h"
#include "AkModulatorMgr.h"
#include "AkCritical.h"


ModulatorProxyLocal::ModulatorProxyLocal( IModulatorProxy::ProxyType in_proxyType, AkUniqueID in_id )
{
	AkModulatorType modType = (in_proxyType == IModulatorProxy::ProxyType_Lfo) ? AkModulatorType_LFO : ( ( in_proxyType == IModulatorProxy::ProxyType_Envelope) ? AkModulatorType_Envelope : AkModulatorType_Time );

	CAkIndexable* pIndexable = AK::SoundEngine::GetIndexable( in_id, AkIdxType_Modulator );
	SetIndexable( pIndexable != NULL ? pIndexable : CAkModulator::Create( in_id, modType ) );
}

ModulatorProxyLocal::~ModulatorProxyLocal()
{
}

void ModulatorProxyLocal::SetAkProp( AkModulatorPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax )
{
	CAkModulator* pIndexable = static_cast<CAkModulator*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetAkProp( in_eProp, in_fValue, in_fMin, in_fMax );
	}
}

void ModulatorProxyLocal::SetAkProp( AkModulatorPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax )
{
	CAkModulator* pIndexable = static_cast<CAkModulator*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetAkProp( in_eProp, in_iValue, in_iMin, in_iMax );
	}
}

void ModulatorProxyLocal::SetRTPC( AkRtpcID in_RtpcID, AkRtpcType in_RtpcType, AkRtpcAccum in_RtpcAccum, AkRTPC_ParameterID in_ParamID, AkUniqueID in_RtpcCurveID, AkCurveScaling in_eScaling, AkRTPCGraphPoint* in_pArrayConversion, AkUInt32 in_ulConversionArraySize )
{
	CAkModulator* pIndexable = static_cast<CAkModulator*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->SetRTPC( in_RtpcID, in_RtpcType, in_RtpcAccum, in_ParamID, in_RtpcCurveID, in_eScaling, in_pArrayConversion, in_ulConversionArraySize );
	}
}

void ModulatorProxyLocal::UnsetRTPC( AkRTPC_ParameterID in_ParamID, AkUniqueID in_RtpcCurveID )
{
	CAkModulator* pIndexable = static_cast<CAkModulator*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;
		pIndexable->UnsetRTPC( in_ParamID, in_RtpcCurveID );
	}
}

#endif
#endif // #ifndef AK_OPTIMIZED
