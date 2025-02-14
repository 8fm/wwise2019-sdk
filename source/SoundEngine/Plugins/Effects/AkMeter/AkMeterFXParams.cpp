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

#include "AkMeterFXParams.h"
#include <math.h>
#include <AK/Tools/Common/AkBankReadHelpers.h>

// Creation function
AK::IAkPluginParam * CreateAkMeterFXParams( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AKASSERT( in_pAllocator != NULL );
	return AK_PLUGIN_NEW( in_pAllocator, CAkMeterFXParams( ) );
}

// Constructor/destructor.
CAkMeterFXParams::CAkMeterFXParams( )
{
}

CAkMeterFXParams::~CAkMeterFXParams( )
{
}

// Copy constructor.
CAkMeterFXParams::CAkMeterFXParams( const CAkMeterFXParams & in_rCopy )
{
	RTPC = in_rCopy.RTPC;
	NonRTPC = in_rCopy.NonRTPC;
}

// Create duplicate.
AK::IAkPluginParam * CAkMeterFXParams::Clone( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AKASSERT( in_pAllocator != NULL );
	return AK_PLUGIN_NEW( in_pAllocator, CAkMeterFXParams( *this ) );
}

// Init.
AKRESULT CAkMeterFXParams::Init(
	AK::IAkPluginMemAlloc *	in_pAllocator,									   
	const void *			in_pParamsBlock, 
	AkUInt32				in_ulBlockSize )
{
	if ( in_ulBlockSize == 0)
	{
		// Init default parameters.
		RTPC.fAttack = AK_METER_ATTACK_DEF;
		RTPC.fRelease = AK_METER_RELEASE_DEF;
		RTPC.fMin = AK_METER_MIN_DEF;
		RTPC.fMax = AK_METER_MAX_DEF;
		RTPC.fHold = AK_METER_HOLD_DEF;

		NonRTPC.eMode = AK_METER_MODE_DEF;
		NonRTPC.eScope = AK_METER_SCOPE_DEF;
		NonRTPC.bApplyDownstreamVolume = AK_METER_APPLYDOWNSTREAMVOLUME_DEF;
		NonRTPC.uGameParamID = AK_METER_GAMEPARAM_DEF;

		return AK_Success;
	}
	return SetParamsBlock( in_pParamsBlock, in_ulBlockSize );
}

// Term.
AKRESULT CAkMeterFXParams::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AKASSERT( in_pAllocator != NULL );
	AK_PLUGIN_DELETE( in_pAllocator, this );
	return AK_Success;
}

// Blob set.
AKRESULT CAkMeterFXParams::SetParamsBlock(	const void * in_pParamsBlock, 
												AkUInt32 in_ulBlockSize
												)
{  
	AKRESULT eResult = AK_Success;
	AkUInt8 * pParamsBlock = (AkUInt8 *)in_pParamsBlock;

	RTPC.fAttack = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );
	RTPC.fRelease = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );	
	RTPC.fMin = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );	
	RTPC.fMax = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );	
	RTPC.fHold = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );	

	NonRTPC.eMode = (AkMeterMode) *pParamsBlock;
	SKIPBANKDATA( AkUInt8, pParamsBlock, in_ulBlockSize );
	NonRTPC.eScope = (AkMeterScope)*pParamsBlock;
	SKIPBANKDATA(AkUInt8, pParamsBlock, in_ulBlockSize);
	NonRTPC.bApplyDownstreamVolume = READBANKDATA( bool, pParamsBlock, in_ulBlockSize );	
	NonRTPC.uGameParamID = READBANKDATA( AkUniqueID, pParamsBlock, in_ulBlockSize );	

	CHECKBANKDATASIZE( in_ulBlockSize, eResult );
	return eResult;
}

// Update one parameter.
AKRESULT CAkMeterFXParams::SetParam(	
	AkPluginParamID in_ParamID,
	const void * in_pValue, 
	AkUInt32 in_ulParamSize )
{
	AKRESULT eResult = AK_Success;

	switch ( in_ParamID )
	{
	case AK_METERFXPARAM_ATTACK_ID:
	{
		AkReal32 fValue = *reinterpret_cast<const AkReal32*>(in_pValue);
		RTPC.fAttack = AkClamp( fValue, 0.f, 10.f );
		break;
	}
	case AK_METERFXPARAM_RELEASE_ID:
	{
		AkReal32 fValue = *reinterpret_cast<const AkReal32*>(in_pValue);
		RTPC.fRelease = AkClamp( fValue, 0.f, 10.f );
		break;
	}
	case AK_METERFXPARAM_MODE_ID:
		NonRTPC.eMode = (AkMeterMode) *reinterpret_cast<const AkUInt32*>(in_pValue);
		break;
	case AK_METERFXPARAM_SCOPE_ID:
		NonRTPC.eScope = (AkMeterScope)*reinterpret_cast<const AkUInt32*>(in_pValue);
		break;
	case AK_METERFXPARAM_MIN_ID:
	{
		AkReal32 fValue = *reinterpret_cast<const AkReal32*>(in_pValue);
		RTPC.fMin = AkClamp( fValue, -93.3f, 0.f );
		break;
	}
	case AK_METERFXPARAM_MAX_ID:
	{
		AkReal32 fValue = *reinterpret_cast<const AkReal32*>(in_pValue);
		RTPC.fMax = AkClamp( fValue, -93.3f, 12.f );
		break;
	}
	case AK_METERFXPARAM_HOLD_ID:
	{
		AkReal32 fValue = *reinterpret_cast<const AkReal32*>(in_pValue);
		RTPC.fHold = AkClamp( fValue, 0.f, 10.f );
		break;
	}
	case AK_METERFXPARAM_APPLYDOWNSTREAMVOLUME_ID:
		NonRTPC.bApplyDownstreamVolume = *reinterpret_cast<const bool*>(in_pValue);
		break;
	case AK::IAkPluginParam::ALL_PLUGIN_DATA_ID:
		NonRTPC.uGameParamID = *reinterpret_cast<const AkUniqueID*>(in_pValue);
		break;
	default:
		AKASSERT(!"Invalid parameter.");
		eResult = AK_InvalidParameter;
	}

	return eResult;
}
