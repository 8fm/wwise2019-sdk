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
// AkGainFX.cpp
//
// Gain FX implementation.
//
//////////////////////////////////////////////////////////////////////

#include "AkGainFX.h"
#include <AK/DSP/AkApplyGain.h>
#include <math.h>
#include <AK/Tools/Common/AkAssert.h>
#include <AK/AkWwiseSDKVersion.h>

// Plugin mechanism. Dynamic create function whose address must be registered to the FX manager.
AK::IAkPlugin* CreateAkGainFX( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AKASSERT( in_pAllocator != NULL );
	return AK_PLUGIN_NEW( in_pAllocator, CAkGainFX( ) );
}


// Constructor.
CAkGainFX::CAkGainFX()
{
}

// Destructor.
CAkGainFX::~CAkGainFX()
{

}

// Initializes and allocate memory for the effect
AKRESULT CAkGainFX::Init(	AK::IAkPluginMemAlloc * in_pAllocator,		// Memory allocator interface.
									AK::IAkEffectPluginContext * in_pFXCtx,	// FX Context
									AK::IAkPluginParam * in_pParams,		// Effect parameters.
									AkAudioFormat &	in_rFormat				// Required audio input format.
								)
{
	// Save format internally
	m_uNumProcessedChannels = in_rFormat.GetNumChannels( );
	m_uSampleRate = in_rFormat.uSampleRate;

	// Set parameters.
	m_pSharedParams = static_cast<CAkGainFXParams*>(in_pParams);

	// Gain ramp initialization Gains level
	m_fCurrentFullBandGain = m_pSharedParams->GetFullbandGain();
	m_fCurrentLFEGain = m_pSharedParams->GetLFEGain();

	AK_PERF_RECORDING_RESET();

	return AK_Success;
}

// Terminates.
AKRESULT CAkGainFX::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	// Effect's deletion
	AK_PLUGIN_DELETE( in_pAllocator, this );
	return AK_Success;
}

// Reset or seek to start.
AKRESULT CAkGainFX::Reset( )
{
	return AK_Success;
}

// Effect info query.
AKRESULT CAkGainFX::GetPluginInfo( AkPluginInfo & out_rPluginInfo )
{
	out_rPluginInfo.eType = AkPluginTypeEffect;
	out_rPluginInfo.bIsInPlace = true;
	out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
	return AK_Success;
}

//  Execute Gain effect.
void CAkGainFX::Execute( AkAudioBuffer* io_pBuffer	)
{
	if ( m_uNumProcessedChannels == 0 || io_pBuffer->uValidFrames == 0 )
		return;

	AK_PERF_RECORDING_START( "Gain", 25, 30 );
	
	AkReal32 fTargetFullBandGain = m_pSharedParams->GetFullbandGain();
	AkReal32 fTargetLFEGain = m_pSharedParams->GetLFEGain();

	AK::DSP::ApplyGain( io_pBuffer, m_fCurrentFullBandGain, fTargetFullBandGain, false/*Don't process LFE*/ );
	AK::DSP::ApplyGainLFE( io_pBuffer, m_fCurrentLFEGain, fTargetLFEGain );
	m_fCurrentFullBandGain = fTargetFullBandGain;
	m_fCurrentLFEGain = fTargetLFEGain;

	AK_PERF_RECORDING_STOP( "Gain", 25, 30 );
}
