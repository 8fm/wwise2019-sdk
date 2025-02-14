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

#include "AkRecorderFXParams.h"
#include <math.h>
#include <AK/Tools/Common/AkBankReadHelpers.h>

// Creation function
AK::IAkPluginParam * CreateAkRecorderFXParams( AK::IAkPluginMemAlloc * in_pAllocator )
{
	return AK_PLUGIN_NEW( in_pAllocator, CAkRecorderFXParams( ) );
}

// Constructor/destructor.
CAkRecorderFXParams::CAkRecorderFXParams( )
{
}

CAkRecorderFXParams::~CAkRecorderFXParams( )
{
}

// Copy constructor.
CAkRecorderFXParams::CAkRecorderFXParams( const CAkRecorderFXParams & in_rCopy )
{
	RTPC = in_rCopy.RTPC;
	NonRTPC = in_rCopy.NonRTPC;
}

// Create duplicate.
AK::IAkPluginParam * CAkRecorderFXParams::Clone( AK::IAkPluginMemAlloc * in_pAllocator )
{
	return AK_PLUGIN_NEW( in_pAllocator, CAkRecorderFXParams( *this ) );
}

// Init.
AKRESULT CAkRecorderFXParams::Init(
	AK::IAkPluginMemAlloc *	in_pAllocator,									   
	const void *			in_pParamsBlock, 
	AkUInt32				in_ulBlockSize )
{
	if ( in_ulBlockSize == 0)
	{
		// Init default parameters.
		RTPC.fCenter = AK_RECORDER_CENTER_DEF;
		RTPC.fFront = AK_RECORDER_FRONT_DEF;
		RTPC.fSurround = AK_RECORDER_SURROUND_DEF;
		RTPC.fRear = AK_RECORDER_REAR_DEF;
		RTPC.fLFE = AK_RECORDER_LFE_DEF;

		NonRTPC.iFormat = AK_RECORDER_FORMAT_DEF;
		NonRTPC.szFilename[0] = 0;
		NonRTPC.bDownmixToStereo = AK_RECORDER_DOWNMIXTOSTEREO_DEF;
		NonRTPC.bApplyDownstreamVolume = AK_RECORDER_APPLYDOWNSTREAMVOLUME_DEF;
		NonRTPC.iAmbisonicChannelOrdering = AK_RECORDER_AMBCHANNELSORDERING_DEF;

		return AK_Success;
	}
	return SetParamsBlock( in_pParamsBlock, in_ulBlockSize );
}

// Term.
AKRESULT CAkRecorderFXParams::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AK_PLUGIN_DELETE( in_pAllocator, this );
	return AK_Success;
}

// Blob set.
AKRESULT CAkRecorderFXParams::SetParamsBlock(	const void * in_pParamsBlock, 
												AkUInt32 in_ulBlockSize
												)
{  
	AKRESULT eResult = AK_Success;
	AkUInt8 * pParamsBlock = (AkUInt8 *)in_pParamsBlock;

	RTPC.fCenter = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );	
	RTPC.fFront = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );
	RTPC.fSurround = READBANKDATA(AkReal32, pParamsBlock, in_ulBlockSize);
	RTPC.fRear = READBANKDATA(AkReal32, pParamsBlock, in_ulBlockSize);
	RTPC.fLFE = READBANKDATA(AkReal32, pParamsBlock, in_ulBlockSize);

	NonRTPC.iFormat = READBANKDATA(AkInt16, pParamsBlock, in_ulBlockSize);

	AK_UTF16_TO_OSCHAR( NonRTPC.szFilename, (const AkUtf16 *) pParamsBlock, sizeof( NonRTPC.szFilename ) / sizeof( AkOSChar ) );
	SKIPBANKBYTES( (AkUInt32)( AKPLATFORM::OsStrLen( NonRTPC.szFilename ) + 1 ) * sizeof( AkUtf16 ), pParamsBlock, in_ulBlockSize );

	NonRTPC.bDownmixToStereo = READBANKDATA( bool, pParamsBlock, in_ulBlockSize );	
	NonRTPC.bApplyDownstreamVolume = READBANKDATA( bool, pParamsBlock, in_ulBlockSize );	

	NonRTPC.iAmbisonicChannelOrdering = READBANKDATA(AkInt16, pParamsBlock, in_ulBlockSize);

	CHECKBANKDATASIZE( in_ulBlockSize, eResult );
	return eResult;
}

// Update one parameter.
AKRESULT CAkRecorderFXParams::SetParam(	
	AkPluginParamID in_ParamID,
	const void * in_pValue, 
	AkUInt32 in_ulParamSize )
{
	AKRESULT eResult = AK_Success;

	switch ( in_ParamID )
	{
	case AK_RECORDERFXPARAM_CENTER_ID:
		RTPC.fCenter = *reinterpret_cast<const AkReal32*>(in_pValue);
		break;
	case AK_RECORDERFXPARAM_FRONT_ID:
		RTPC.fFront = *reinterpret_cast<const AkReal32*>(in_pValue);
		break;
	case AK_RECORDERFXPARAM_SURROUND_ID:
		RTPC.fSurround = *reinterpret_cast<const AkReal32*>(in_pValue);
		break;
	case AK_RECORDERFXPARAM_REAR_ID:
		RTPC.fRear = *reinterpret_cast<const AkReal32*>(in_pValue);
		break;
	case AK_RECORDERFXPARAM_LFE_ID:
		RTPC.fLFE = *reinterpret_cast<const AkReal32*>(in_pValue);
		break;
	case AK_RECORDERFXPARAM_FORMAT_ID:
		NonRTPC.iFormat = *reinterpret_cast<const AkInt16*>(in_pValue);
		break;
#ifdef WWISE_AUTHORING
	case AK_RECORDERFXPARAM_AUTHORINGFILENAME_ID:
		AK_UTF16_TO_OSCHAR( NonRTPC.szFilename, (const AkUtf16 *) in_pValue, sizeof( NonRTPC.szFilename ) / sizeof( AkOSChar ) );
		break;
	case AK_RECORDERFXPARAM_GAMEFILENAME_ID:
		break;
#else
	case AK_RECORDERFXPARAM_AUTHORINGFILENAME_ID:
		break;
	case AK_RECORDERFXPARAM_GAMEFILENAME_ID:
		AK_UTF16_TO_OSCHAR( NonRTPC.szFilename, (const AkUtf16 *) in_pValue, sizeof( NonRTPC.szFilename ) / sizeof( AkOSChar ) );
		break;
#endif
	case AK_RECORDERFXPARAM_DOWNMIXTOSTEREO_ID:
		NonRTPC.bDownmixToStereo = *reinterpret_cast<const bool*>(in_pValue);
		break;
	case AK_RECORDERFXPARAM_APPLYDOWNSTREAMVOLUME_ID:
		NonRTPC.bApplyDownstreamVolume = *reinterpret_cast<const bool*>(in_pValue);
		break;
	case AK_RECORDERFXPARAM_AMBCHANNELORDERING:
		NonRTPC.iAmbisonicChannelOrdering = *reinterpret_cast<const AkInt16*>(in_pValue);
		break;
	default:
		AKASSERT(!"Invalid parameter.");
		eResult = AK_InvalidParameter;
	}

	return eResult;
}