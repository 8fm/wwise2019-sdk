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

#include "AkSynthOneParams.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>

// Plugin mechanism. Parameters node creation function to be registered to the FX manager.
AK::IAkPluginParam * CreateAkSynthOneParams(AK::IAkPluginMemAlloc * in_pAllocator)
{
	return AK_PLUGIN_NEW( in_pAllocator, CAkSynthOneParams() );
}

// Constructor.
CAkSynthOneParams::CAkSynthOneParams()
{

}

// Copy constructor.
CAkSynthOneParams::CAkSynthOneParams( const CAkSynthOneParams & in_rCopy )
{
	m_Params = in_rCopy.m_Params;	 
}

// Destructor.
CAkSynthOneParams::~CAkSynthOneParams()
{

}

// Create parameter node duplicate.
AK::IAkPluginParam * CAkSynthOneParams::Clone( AK::IAkPluginMemAlloc * in_pAllocator )
{
	return AK_PLUGIN_NEW( in_pAllocator, CAkSynthOneParams(*this) );	 
}

// Parameters node initialization.
AKRESULT CAkSynthOneParams::Init( AK::IAkPluginMemAlloc *	/*in_pAllocator*/,								 
								 const void *				in_pParamsBlock, 
								 AkUInt32					in_uBlockSize 
								)
{
	if ( in_uBlockSize == 0)
	{
		// Init with default values if we got invalid parameter block.
		// RTPCed
		m_Params.eOpMode = AkSynthOneOperationMode_Mix;
		m_Params.fBaseFreq = 0.f;
		m_Params.fOutputLevel = 0.f;
		m_Params.eNoiseType = AkSynthOneNoiseType_White;
		m_Params.fNoiseLevel = 0.f;
		m_Params.fFmAmount = 0.f;
		m_Params.bOverSampling = true;
		m_Params.eOsc1Waveform = AkSynthOneWaveType_Sine;
		m_Params.bOsc1Invert = false;
		m_Params.iOsc1Transpose = 0;
		m_Params.fOsc1Level = 0.f;
		m_Params.fOsc1Pwm = 50.f;
		m_Params.eOsc2Waveform = AkSynthOneWaveType_Sine;
		m_Params.bOsc2Invert = false;
		m_Params.iOsc2Transpose = 0;
		m_Params.fOsc2Level = 0.f;
		m_Params.fOsc2Pwm = 50.f;
		// Static
		m_Params.eFreqMode = AkSynthOneFrequencyMode_Specify;
		
		return AK_Success;
	}

	return SetParamsBlock( in_pParamsBlock, in_uBlockSize );
}

// Parameters node termination.
AKRESULT CAkSynthOneParams::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AK_PLUGIN_DELETE( in_pAllocator, this );
	return AK_Success;
}

// Set all shared parameters at once.
AKRESULT CAkSynthOneParams::SetParamsBlock( const void * in_pParamsBlock, 
										  AkUInt32 in_ulBlockSize
										  )
{
	AKRESULT eResult = AK_Success;
	AkUInt8 * pParamsBlock = (AkUInt8 *)in_pParamsBlock;

	m_Params.eFreqMode = (AkSynthOneFrequencyMode)READBANKDATA( AkInt8, pParamsBlock, in_ulBlockSize );
	m_Params.fBaseFreq = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );

	m_Params.eOpMode = (AkSynthOneOperationMode)READBANKDATA( AkInt8, pParamsBlock, in_ulBlockSize );
	m_Params.fOutputLevel = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );

	m_Params.eNoiseType = (AkSynthOneNoiseType)READBANKDATA( AkInt8, pParamsBlock, in_ulBlockSize );
	m_Params.fNoiseLevel = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );

	m_Params.fFmAmount = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );

	m_Params.bOverSampling = READBANKDATA( AkInt8, pParamsBlock, in_ulBlockSize ) != 0;

	m_Params.eOsc1Waveform = (AkSynthOneWaveType)READBANKDATA( AkInt8, pParamsBlock, in_ulBlockSize );
	m_Params.bOsc1Invert = READBANKDATA( AkInt8, pParamsBlock, in_ulBlockSize ) != 0;
	m_Params.iOsc1Transpose = READBANKDATA( AkInt32, pParamsBlock, in_ulBlockSize );
	m_Params.fOsc1Level = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );
	m_Params.fOsc1Pwm = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );

	m_Params.eOsc2Waveform = (AkSynthOneWaveType)READBANKDATA( AkInt8, pParamsBlock, in_ulBlockSize );
	m_Params.bOsc2Invert = READBANKDATA( AkInt8, pParamsBlock, in_ulBlockSize ) != 0;
	m_Params.iOsc2Transpose = READBANKDATA( AkInt32, pParamsBlock, in_ulBlockSize );
	m_Params.fOsc2Level = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );
	m_Params.fOsc2Pwm = READBANKDATA( AkReal32, pParamsBlock, in_ulBlockSize );

	CHECKBANKDATASIZE( in_ulBlockSize, eResult );

	return eResult;
}

// Update single parameter.
AKRESULT CAkSynthOneParams::SetParam(	AkPluginParamID in_ParamID,
										const void *	in_pValue, 
										AkUInt32		in_uParamSize
									)
{
	// Consistency check.
	if ( in_pValue == NULL )
	{
		return AK_InvalidParameter;
	}
	
	AkUInt8* dataPtr = const_cast<AkUInt8*>((AkUInt8*)in_pValue);

	// Set parameter value.
	switch ( in_ParamID )
	{
	case AK_SYNTHONE_FXPARAM_FREQUENCYMODE_ID:
		m_Params.eFreqMode = (AkSynthOneFrequencyMode)READBANKDATA(AkInt32, dataPtr, in_uParamSize);
		break;
	// This parameter is RTPCed
	case AK_SYNTHONE_FXPARAM_BASEFREQUENCY_ID:
		m_Params.fBaseFreq = READBANKDATA(AkReal32, dataPtr, in_uParamSize);
		break;
	case AK_SYNTHONE_FXPARAM_OPERATIONMODE_ID:
		m_Params.eOpMode = (AkSynthOneOperationMode)(AkUInt32)READBANKDATA(AkReal32, dataPtr, in_uParamSize);
		break;
	// This parameter is RTPCed
	case AK_SYNTHONE_FXPARAM_OUTPUTLEVEL_ID:
		m_Params.fOutputLevel = READBANKDATA(AkReal32, dataPtr, in_uParamSize);
		break;
		// This parameter is RTPCed
	case AK_SYNTHONE_FXPARAM_NOISESHAPE_ID:
		m_Params.eNoiseType = (AkSynthOneNoiseType)(AkUInt32)READBANKDATA(AkReal32, dataPtr, in_uParamSize);
		break;
	// This parameter is RTPCed
	case AK_SYNTHONE_FXPARAM_NOISELEVEL_ID:
		m_Params.fNoiseLevel = READBANKDATA(AkReal32, dataPtr, in_uParamSize);
		break;
	// This parameter is RTPCed
	case AK_SYNTHONE_FXPARAM_FMAMOUNT_ID:
		m_Params.fFmAmount = READBANKDATA(AkReal32, dataPtr, in_uParamSize);
		m_Params.fFmAmount = AkClamp( m_Params.fFmAmount, SYNTHONE_FMAMOUNT_MIN, SYNTHONE_FMAMOUNT_MAX );
		break;
	case AK_SYNTHONE_FXPARAM_OVERSAMPLING_ID:
		m_Params.bOverSampling = READBANKDATA(AkUInt16, dataPtr, in_uParamSize) != 0;
		break;
	case AK_SYNTHONE_FXPARAM_OSC1_WAVEFORM_ID:
		m_Params.eOsc1Waveform = (AkSynthOneWaveType)(AkUInt32)READBANKDATA(AkReal32, dataPtr, in_uParamSize);
		break;
	case AK_SYNTHONE_FXPARAM_OSC1_INVERT_ID:
		m_Params.bOsc1Invert = (AkUInt32)READBANKDATA(AkReal32, dataPtr, in_uParamSize) != 0;
		break;
	// This parameter is RTPCed
	case AK_SYNTHONE_FXPARAM_OSC1_TRANSPOSE_ID:	
		m_Params.iOsc1Transpose = (AkInt32)READBANKDATA(AkReal32, dataPtr, in_uParamSize);
		break;
	// This parameter is RTPCed
	case AK_SYNTHONE_FXPARAM_OSC1_LEVEL_ID:	
		m_Params.fOsc1Level = READBANKDATA(AkReal32, dataPtr, in_uParamSize);
		break;
	// This parameter is RTPCed
	case AK_SYNTHONE_FXPARAM_OSC1_PWM_ID:	
		m_Params.fOsc1Pwm = READBANKDATA(AkReal32, dataPtr, in_uParamSize);
		m_Params.fOsc1Pwm = AkClamp( m_Params.fOsc1Pwm, SYNTHONE_PWM_MIN, SYNTHONE_PWM_MAX );
		break;
	case AK_SYNTHONE_FXPARAM_OSC2_WAVEFORM_ID:
		m_Params.eOsc2Waveform = (AkSynthOneWaveType)(AkUInt32)READBANKDATA(AkReal32, dataPtr, in_uParamSize);
		break;
	case AK_SYNTHONE_FXPARAM_OSC2_INVERT_ID:
		m_Params.bOsc2Invert = (AkUInt32)READBANKDATA(AkReal32, dataPtr, in_uParamSize) != 0;
		break;
	// This parameter is RTPCed
	case AK_SYNTHONE_FXPARAM_OSC2_TRANSPOSE_ID:	
		m_Params.iOsc2Transpose = (AkInt32)READBANKDATA(AkReal32, dataPtr, in_uParamSize);
		break;
	// This parameter is RTPCed
	case AK_SYNTHONE_FXPARAM_OSC2_LEVEL_ID:	
		m_Params.fOsc2Level = READBANKDATA(AkReal32, dataPtr, in_uParamSize);
		break;
	// This parameter is RTPCed
	case AK_SYNTHONE_FXPARAM_OSC2_PWM_ID:	
		m_Params.fOsc2Pwm = READBANKDATA(AkReal32, dataPtr, in_uParamSize);
		m_Params.fOsc2Pwm = AkClamp( m_Params.fOsc2Pwm, SYNTHONE_PWM_MIN, SYNTHONE_PWM_MAX );
		break;
	default:
		AKASSERT( !"Unknown parameter" );
		break;
	}

	return AK_Success;
}
