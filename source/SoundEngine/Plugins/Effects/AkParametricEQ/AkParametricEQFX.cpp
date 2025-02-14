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

//////////////////////////////////////////////////////////////////////
//
// AkParametricEQFX.cpp
//
// ParametricEQ FX implementation.
// 
// Based on evaluation of bilinear transform equations for use with biquad
// y[n] = (b0/a0)*x[n] + (b1/a0)*x[n-1] + (b2/a0)*x[n-2]
//                     - (a1/a0)*y[n-1] - (a2/a0)*y[n-2]
//
//////////////////////////////////////////////////////////////////////

#include "AkParametricEQFX.h"
#include <AK/DSP/AkApplyGain.h>
#include <math.h>
#include <AK/Tools/Common/AkAssert.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/AkWwiseSDKVersion.h>

// Plugin mechanism. Dynamic create function whose address must be registered to the FX manager.
AK::IAkPlugin* CreateAkParametricEQFX( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AKASSERT( in_pAllocator != NULL );
	return AK_PLUGIN_NEW( in_pAllocator, CAkParametricEQFX( ) );
}


// Constructor.
CAkParametricEQFX::CAkParametricEQFX()
{
	m_pSharedParams = NULL;
	m_pfAllocatedMem = NULL;
}

// Destructor.
CAkParametricEQFX::~CAkParametricEQFX()
{

}

// Initializes and allocate memory for the effect
AKRESULT CAkParametricEQFX::Init(	AK::IAkPluginMemAlloc * in_pAllocator,		// Memory allocator interface.
									AK::IAkEffectPluginContext * in_pFXCtx,			// FX Context
									AK::IAkPluginParam * in_pParams,		// Effect parameters.
									AkAudioFormat &	in_rFormat				// Required audio input format.
								)
{
	// Save format internally
	m_uNumProcessedChannels = in_rFormat.GetNumChannels( );
	m_uSampleRate = in_rFormat.uSampleRate;

	// Set parameters.
	m_pSharedParams = static_cast<CAkParameterEQFXParams*>(in_pParams);

	// LFE passthrough behavior
	if( in_rFormat.HasLFE() && !m_pSharedParams->GetProcessLFE( ) )
	{
		--m_uNumProcessedChannels;
	}
		
	if ( m_uNumProcessedChannels )
	{
		for(AkUInt32 c = 0; c < NUMBER_FILTER_MODULES; c++)
		{
			if(m_Biquad[c].Init(in_pAllocator, m_uNumProcessedChannels) != AK_Success)
				return AK_InsufficientMemory;
		}
	}		

	// Set parameters as dirty to trigger coef compute on first Execute()
	m_pSharedParams->SetDirty( BAND1, true );
	m_pSharedParams->SetDirty( BAND2, true );
	m_pSharedParams->SetDirty( BAND3, true );

	// Gain ramp initialization for Output level
	m_fCurrentGain = m_pSharedParams->GetOutputLevel( );

	AK_PERF_RECORDING_RESET();

	return AK_Success;
}

// Terminates.
AKRESULT CAkParametricEQFX::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	// Free filter memory
	for(AkUInt32 c = 0; c < NUMBER_FILTER_MODULES; c++)
		m_Biquad[c].Term(in_pAllocator);	

	// Effect's deletion
	AK_PLUGIN_DELETE( in_pAllocator, this );
	return AK_Success;
}

// Reset or seek to start.
AKRESULT CAkParametricEQFX::Reset( )
{
	// Clear filter memory
	for(AkUInt32 c = 0; c < NUMBER_FILTER_MODULES; c++)
		m_Biquad[c].Reset();	
	
	return AK_Success;
}

// Effect info query.
AKRESULT CAkParametricEQFX::GetPluginInfo( AkPluginInfo & out_rPluginInfo )
{
	out_rPluginInfo.eType = AkPluginTypeEffect;
	out_rPluginInfo.bIsInPlace = true;
	out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
	return AK_Success;
}

//  Execute parametric EQ effect.
void CAkParametricEQFX::Execute( AkAudioBuffer* io_pBuffer	)
{
	if ( m_uNumProcessedChannels == 0 || io_pBuffer->uValidFrames == 0 )
		return;

	AK_PERF_RECORDING_START( "ParametricEQ", 25, 30 );

	// Compute filter coefficients (bypassed if no changes)
	bool bOnOff = true;

	for(AkUInt32 i = 0; i < NUMBER_FILTER_MODULES; i++)
	{
		EQModuleParams * pFilterParams;
		pFilterParams = m_pSharedParams->GetFilterModuleParams((AkBandNumber)i);
		bOnOff= pFilterParams->bOnOff;
		if(m_pSharedParams->GetDirty((AkBandNumber)i))
		{
			ComputeBiquadCoefs((AkBandNumber)i, pFilterParams);
			m_pSharedParams->SetDirty((AkBandNumber)i, false);
		}

		if(bOnOff)
			m_Biquad[i].ProcessBuffer(io_pBuffer->GetChannel(0), io_pBuffer->uValidFrames, io_pBuffer->MaxFrames());
	}

	
	AkReal32 fTargetGain = m_pSharedParams->GetOutputLevel( );	
	AK::DSP::ApplyGain(io_pBuffer, m_fCurrentGain, fTargetGain, m_pSharedParams->GetProcessLFE());
	m_fCurrentGain = fTargetGain;	

	AK_PERF_RECORDING_STOP( "ParametricEQ", 25, 30 );
}

void CAkParametricEQFX::ComputeBiquadCoefs(AkBandNumber in_eBandNum, EQModuleParams * in_sModuleParams)
{
	//Convert the filter type from the persisted (legacy) type to the biquad type.  Yep, we managed to have two different persisted orders.
	::DSP::FilterType eBiquadType;
	switch(in_sModuleParams->eFilterType)
	{
	case AKFILTERTYPE_LOWPASS: eBiquadType = ::DSP::FilterType_LowPass; break;
	case AKFILTERTYPE_HIPASS: eBiquadType = ::DSP::FilterType_HighPass; break;
	case AKFILTERTYPE_BANDPASS: eBiquadType = ::DSP::FilterType_BandPass; break;
	case AKFILTERTYPE_NOTCH: eBiquadType = ::DSP::FilterType_Notch; break;
	case AKFILTERTYPE_LOWSHELF: eBiquadType = ::DSP::FilterType_LowShelf; break;
	case AKFILTERTYPE_HISHELF: eBiquadType = ::DSP::FilterType_HighShelf; break;
	case AKFILTERTYPE_PEAKINGEQ: eBiquadType = ::DSP::FilterType_Peaking; break;
	default:
		AKASSERT(!"Unsupported filter type");
	}
	m_Biquad[in_eBandNum].ComputeCoefs(eBiquadType, (AkReal32)m_uSampleRate, in_sModuleParams->fFrequency, in_sModuleParams->fGain, in_sModuleParams->fQFactor);	
}
