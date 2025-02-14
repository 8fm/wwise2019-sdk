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

#include "AkStereoDelayFXParams.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include <AK/Tools/Common/AkAssert.h>
#include <math.h>


// Creation function
AK::IAkPluginParam * CreateAkStereoDelayFXParams( AK::IAkPluginMemAlloc * in_pAllocator )
{
	return AK_PLUGIN_NEW( in_pAllocator, CAkStereoDelayFXParams( ) );
}

// Constructor/destructor.
CAkStereoDelayFXParams::CAkStereoDelayFXParams( )
{

}

CAkStereoDelayFXParams::~CAkStereoDelayFXParams( )
{
}

// Copy constructor.
CAkStereoDelayFXParams::CAkStereoDelayFXParams( const CAkStereoDelayFXParams & in_rCopy )
{
	m_Params = in_rCopy.m_Params;
	m_ParamChangeHandler.SetAllParamChanges();
}

// Create duplicate.
AK::IAkPluginParam * CAkStereoDelayFXParams::Clone( AK::IAkPluginMemAlloc * in_pAllocator )
{
	return AK_PLUGIN_NEW( in_pAllocator, CAkStereoDelayFXParams( *this ) );
}

// Init/Term.
AKRESULT CAkStereoDelayFXParams::Init(	AK::IAkPluginMemAlloc *	in_pAllocator,									   
											const void *			in_pParamsBlock, 
											AkUInt32				in_ulBlockSize )
{
	if ( in_ulBlockSize != 0)
	{
		return SetParamsBlock( in_pParamsBlock, in_ulBlockSize );
	}
	// Wwise does serialized SetParam()
	return AK_Success;
}

AKRESULT CAkStereoDelayFXParams::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AK_PLUGIN_DELETE( in_pAllocator, this );
	return AK_Success;
}

// Blob set.
AKRESULT CAkStereoDelayFXParams::SetParamsBlock(	const void * in_pParamsBlock, 
														AkUInt32 in_ulBlockSize
												)
{  
	AKRESULT eResult = AK_Success;
	AkUInt8 * pParamsBlock = (AkUInt8 *)in_pParamsBlock;

	// Left channel
	m_Params.eInputType[0] = READBANKDATA( AkInputChannelType, pParamsBlock, in_ulBlockSize );
	m_Params.StereoDelayParams[0].fDelayTime = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );
	m_Params.StereoDelayParams[0].fFeedback = AK_DBTOLIN( READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize ) );
	m_Params.StereoDelayParams[0].fCrossFeed = AK_DBTOLIN( READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize ) );
	// Right channel
	m_Params.eInputType[1] = READBANKDATA( AkInputChannelType, pParamsBlock, in_ulBlockSize );
	m_Params.StereoDelayParams[1].fDelayTime = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );
	m_Params.StereoDelayParams[1].fFeedback = AK_DBTOLIN( READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize ) );
	m_Params.StereoDelayParams[1].fCrossFeed = AK_DBTOLIN( READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize ) );
	// Global parameters
	m_Params.FilterParams.eFilterType = READBANKDATA( AkFilterType, pParamsBlock, in_ulBlockSize );
	m_Params.FilterParams.fFilterGain = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );
	m_Params.FilterParams.fFilterFrequency = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );	
	m_Params.FilterParams.fFilterQFactor = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );
	m_Params.fDryLevel = AK_DBTOLIN( READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize ) );
	m_Params.fWetLevel = AK_DBTOLIN( READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize ) );
	m_Params.fFrontRearBalance = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );
	m_Params.bEnableFeedback = READBANKDATA( bool, pParamsBlock, in_ulBlockSize );
	m_Params.bEnableCrossFeed = READBANKDATA( bool, pParamsBlock, in_ulBlockSize );

	CHECKBANKDATASIZE( in_ulBlockSize, eResult );
 
	m_ParamChangeHandler.SetAllParamChanges();

	return eResult;
}

// Update one parameter.
AKRESULT CAkStereoDelayFXParams::SetParam(	AkPluginParamID in_ParamID,
											const void * in_pValue, 
											AkUInt32 in_ulParamSize )
{
    // Set parameter value.
    switch ( in_ParamID )
    {
	case AKSTEREODELAYPARAMID_ENABLEFEEDBACK:
		// Note RTPC parameters are always of type float regardless of property type in XML plugin description
		m_Params.bEnableFeedback = (*(AkReal32*)(in_pValue)) != 0;
		break;
	case AKSTEREODELAYPARAMID_ENABLECROSSFEED:
		// Note RTPC parameters are always of type float regardless of property type in XML plugin description
		m_Params.bEnableCrossFeed = (*(AkReal32*)(in_pValue)) != 0;
		break;
	case AKSTEREODELAYPARAMID_DRYLEVEL:
	{
		AkReal32 fValue = *(AkReal32*)in_pValue;
		fValue = AkClamp( fValue, -96, 24.f );
		m_Params.fDryLevel = AK_DBTOLIN( fValue );
		break;
	}
	case AKSTEREODELAYPARAMID_WETLEVEL:
	{
		AkReal32 fValue = *(AkReal32*)in_pValue;
		fValue = AkClamp( fValue, -96, 24.f );
		m_Params.fWetLevel = AK_DBTOLIN( fValue );
		break;
	}
	case AKSTEREODELAYPARAMID_FRONTREARBALANCE:
	{
		AkReal32 fValue = *(AkReal32*)in_pValue;
		m_Params.fFrontRearBalance = AkClamp( fValue, -100.f, 100.f );
		break;
	}
	case AKSTEREODELAYPARAMID_LEFTINPUTTYPE:
		m_Params.eInputType[0] = (AkInputChannelType)(*(AkUInt32*)in_pValue);
		break;
	case AKSTEREODELAYPARAMID_LEFTDELAYTIME:
		m_Params.StereoDelayParams[0].fDelayTime = *(AkReal32*)in_pValue;
		break;
	case AKSTEREODELAYPARAMID_LEFTFEEDBACK:
	{
		AkReal32 fValue = *(AkReal32*)in_pValue;
		fValue = AkClamp( fValue, -48.f, 0.f );
		m_Params.StereoDelayParams[0].fFeedback = AK_DBTOLIN(fValue);
		break;
	}
	case AKSTEREODELAYPARAMID_LEFTCROSSFEED:
	{
		AkReal32 fValue = *(AkReal32*)in_pValue;
		fValue = AkClamp( fValue, -48.f, 0.f );
		m_Params.StereoDelayParams[0].fCrossFeed = AK_DBTOLIN(fValue);
		break;
	}
	case AKSTEREODELAYPARAMID_RIGHTINPUTTYPE:
		m_Params.eInputType[1] = (AkInputChannelType)(*(AkUInt32*)in_pValue);
		break;
	case AKSTEREODELAYPARAMID_RIGHTDELAYTIME:
		m_Params.StereoDelayParams[1].fDelayTime = *(AkReal32*)in_pValue;
		break;
	case AKSTEREODELAYPARAMID_RIGHTFEEDBACK:
	{
		AkReal32 fValue = *(AkReal32*)in_pValue;
		fValue = AkClamp( fValue, -48.f, 0.f );
		m_Params.StereoDelayParams[1].fFeedback = AK_DBTOLIN(fValue);
		break;
	}
	case AKSTEREODELAYPARAMID_RIGHTCROSSFEED:
	{
		AkReal32 fValue = *(AkReal32*)in_pValue;
		fValue = AkClamp( fValue, -48.f, 0.f );
		m_Params.StereoDelayParams[1].fCrossFeed = AK_DBTOLIN(fValue);
		break;
	}
	case AKSTEREODELAYPARAMID_FILTERTYPE:
		m_Params.FilterParams.eFilterType = (AkFilterType)( (AkUInt32)*((AkReal32*)in_pValue) );
		break;
	case AKSTEREODELAYPARAMID_FILTERGAIN:
	{
		AkReal32 fValue = *(AkReal32*)in_pValue;
		m_Params.FilterParams.fFilterGain = AkClamp( fValue, -24.f, 24.f );
		break;
	}
	case AKSTEREODELAYPARAMID_FILTERFREQUENCY:
		m_Params.FilterParams.fFilterFrequency = *(AkReal32*)in_pValue;
		break;
	case AKSTEREODELAYPARAMID_FILTERQFACTOR:
		m_Params.FilterParams.fFilterQFactor = *(AkReal32*)in_pValue;
		break;
	default:
		return AK_InvalidParameter;
	}
	m_ParamChangeHandler.SetParamChange( in_ParamID );

	return AK_Success;
}

// Retrieve all parameters at once
void CAkStereoDelayFXParams::GetParams( AkStereoDelayFXParams* out_pParams )
{
	*out_pParams = m_Params;
}
