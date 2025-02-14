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

#include "AkCompressorFXParams.h"
#include <math.h>
#include <AK/Tools/Common/AkBankReadHelpers.h>

// Creation function
AK::IAkPluginParam * CreateAkCompressorFXParams( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AKASSERT( in_pAllocator != NULL );
	return AK_PLUGIN_NEW( in_pAllocator, CAkCompressorFXParams( ) );
}

// Constructor/destructor.
CAkCompressorFXParams::CAkCompressorFXParams( )
{
}

CAkCompressorFXParams::~CAkCompressorFXParams( )
{
}

// Copy constructor.
CAkCompressorFXParams::CAkCompressorFXParams( const CAkCompressorFXParams & in_rCopy )
{
	m_Params = in_rCopy.m_Params;
}

// Create duplicate.
AK::IAkPluginParam * CAkCompressorFXParams::Clone( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AKASSERT( in_pAllocator != NULL );
	return AK_PLUGIN_NEW( in_pAllocator, CAkCompressorFXParams( *this ) );
}

// Init.
AKRESULT CAkCompressorFXParams::Init(	AK::IAkPluginMemAlloc *	in_pAllocator,									   
										const void *			in_pParamsBlock, 
										AkUInt32				in_ulBlockSize )
{
	if ( in_ulBlockSize == 0)
	{
		// Init default parameters.
		m_Params.fThreshold = AK_COMPRESSOR_THRESHOLD_DEF;
		m_Params.fRatio = AK_COMPRESSOR_RATIO_DEF;
		m_Params.fAttack = AK_COMPRESSOR_ATTACK_DEF;
		m_Params.fRelease = AK_COMPRESSOR_RELEASE_DEF;
		m_Params.fOutputLevel = AK_COMPRESSOR_GAIN_DEF;
		m_Params.bProcessLFE = AK_COMPRESSOR_PROCESSLFE_DEF;
		m_Params.bChannelLink = AK_COMPRESSOR_CHANNELLINK_DEF;

		return AK_Success;
	}
	return SetParamsBlock( in_pParamsBlock, in_ulBlockSize );
}

// Term.
AKRESULT CAkCompressorFXParams::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AKASSERT( in_pAllocator != NULL );
	AK_PLUGIN_DELETE( in_pAllocator, this );
	return AK_Success;
}

// Blob set.
AKRESULT CAkCompressorFXParams::SetParamsBlock(	const void * in_pParamsBlock, 
												AkUInt32 in_ulBlockSize
												)
{  
	AKRESULT eResult = AK_Success;
	AkUInt8 * pParamsBlock = (AkUInt8 *)in_pParamsBlock;
	m_Params.fThreshold = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );
	m_Params.fRatio = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );
	m_Params.fAttack = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );
	m_Params.fRelease = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );
	m_Params.fOutputLevel  = AK_DBTOLIN( READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize ) );
	m_Params.bProcessLFE = READBANKDATA( bool, pParamsBlock, in_ulBlockSize );
	m_Params.bChannelLink = READBANKDATA( bool, pParamsBlock, in_ulBlockSize );
	CHECKBANKDATASIZE( in_ulBlockSize, eResult );
    return eResult;
}

// Update one parameter.
AKRESULT CAkCompressorFXParams::SetParam(	AkPluginParamID in_ParamID,
											const void * in_pValue, 
											AkUInt32 in_ulParamSize )
{
	AKASSERT( in_pValue != NULL );
	if ( in_pValue == NULL )
	{
		return AK_InvalidParameter;
	}
	AKRESULT eResult = AK_Success;

	switch ( in_ParamID )
	{
	case AK_COMPRESSORFXPARAM_THRESHOLD_ID:
	{
		AkReal32 fValue = *reinterpret_cast<const AkReal32*>(in_pValue);
		m_Params.fThreshold = AkClamp( fValue, -96.3f, 0.f );
		break;
	}
	case AK_COMPRESSORFXPARAM_RATIO_ID:
		m_Params.fRatio = *reinterpret_cast<const AkReal32*>(in_pValue);
		break;
	case AK_COMPRESSORFXPARAM_ATTACK_ID:
	{
		AkReal32 fValue = *reinterpret_cast<const AkReal32*>(in_pValue);
		m_Params.fAttack = AkClamp( fValue, 0.f, 2.f );
		break;
	}
	case AK_COMPRESSORFXPARAM_RELEASE_ID:
	{
		AkReal32 fValue = *reinterpret_cast<const AkReal32*>(in_pValue);
		m_Params.fRelease = AkClamp( fValue, 0.f, 2.f );
		break;
	}
	case AK_COMPRESSORFXPARAM_GAIN_ID:
	{
		AkReal32 fValue = *reinterpret_cast<const AkReal32*>(in_pValue);
		fValue = AkClamp( fValue, -24.f, 24.f );
		m_Params.fOutputLevel  = powf( 10.f, fValue * 0.05f );
		break;
	}
	case AK_COMPRESSORFXPARAM_PROCESSLFE_ID:
		m_Params.bProcessLFE = *reinterpret_cast<const bool*>(in_pValue);
		break;
	case AK_COMPRESSORFXPARAM_CHANNELLINK_ID:
		m_Params.bChannelLink = *reinterpret_cast<const bool*>(in_pValue);
		break;
	default:
		AKASSERT(!"Invalid parameter.");
		eResult = AK_InvalidParameter;
	}

	return eResult;
}