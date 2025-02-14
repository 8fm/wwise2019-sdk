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

#include "AkGuitarDistortionFX.h"
#include <math.h>
#include <AK/Tools/Common/AkObject.h> // Placement new
#include "Mix2Interp.h"
#include <AK/Tools/Common/AkAssert.h>
#include <AK/AkWwiseSDKVersion.h>

// Plugin mechanism. Dynamic create function whose address must be registered to the FX manager.
AK::IAkPlugin* CreateAkGuitarDistortionFX( AK::IAkPluginMemAlloc * in_pAllocator )
{
	return AK_PLUGIN_NEW( in_pAllocator, CAkGuitarDistortionFX( ) );
}


// Constructor.
CAkGuitarDistortionFX::CAkGuitarDistortionFX() 
	: m_pSharedParams(NULL)
	, m_pAllocator(NULL)
	, m_pDCFilters(NULL)
{

}

// Destructor.
CAkGuitarDistortionFX::~CAkGuitarDistortionFX()
{

}

// Initializes and allocate memory for the effect
AKRESULT CAkGuitarDistortionFX::Init(	AK::IAkPluginMemAlloc * in_pAllocator,	// Memory allocator interface.
										AK::IAkEffectPluginContext * in_pFXCtx,	// FX Context
										AK::IAkPluginParam * in_pParams,		// Effect parameters.
										AkAudioFormat &	in_rFormat				// Required audio input format.
									)
{
	m_pSharedParams = static_cast<CAkGuitarDistortionFXParams*>(in_pParams);
	m_pAllocator = in_pAllocator;

#define RETURNIFNOTSUCCESS( __FONCTIONEVAL__ )	\
{												\
	AKRESULT __result__ = (__FONCTIONEVAL__);	\
	if ( __result__ != AK_Success )				\
		return __result__;						\
}												\

	// Initialize components
	m_FXInfo.uNumChannels = in_rFormat.GetNumChannels();
	m_FXInfo.uSampleRate = in_rFormat.uSampleRate;

	RETURNIFNOTSUCCESS( m_FXInfo.PreEQ.Init( in_pAllocator, (AkUInt16)m_FXInfo.uNumChannels, NUMPREDISTORTIONEQBANDS ) );
	RETURNIFNOTSUCCESS(  m_FXInfo.PostEQ.Init( in_pAllocator, (AkUInt16)m_FXInfo.uNumChannels, NUMPOSTDISTORTIONEQBANDS ) );
	RETURNIFNOTSUCCESS( InitDCFilters( in_pAllocator ) );

	// Initial computations
	m_pSharedParams->GetParams( &m_FXInfo.Params );

	// Optional, they would be done at Execute anyways
	SetupEQs( m_FXInfo.Params ); 

	m_FXInfo.Distortion.SetParameters(	(DSP::CAkDistortion::DistortionType)m_FXInfo.Params.Distortion.eDistortionType,
										m_FXInfo.Params.Distortion.fDrive,
										m_FXInfo.Params.Distortion.fTone,
										true);
	m_FXInfo.Rectifier.SetRectification( m_FXInfo.Params.Distortion.fRectification, true );
	m_FXInfo.fCurrentGain = m_FXInfo.Params.fOutputLevel;
	m_FXInfo.fCurrentWetDryMix = m_FXInfo.Params.fWetDryMix;

	AK_PERF_RECORDING_RESET();

	return AK_Success;
}

AKRESULT CAkGuitarDistortionFX::InitDCFilters( AK::IAkPluginMemAlloc * in_pAllocator )
{
	m_pDCFilters = (DSP::OnePoleZeroHPFilter*) AK_PLUGIN_ALLOC( in_pAllocator, AK_ALIGN_SIZE_FOR_DMA( m_FXInfo.uNumChannels*sizeof(DSP::OnePoleZeroHPFilter) ) );
	if ( !m_pDCFilters )
		return AK_InsufficientMemory;
	for ( AkUInt32 i = 0; i < m_FXInfo.uNumChannels; ++i )
	{
		AkPlacementNew( &m_pDCFilters[i] ) DSP::OnePoleZeroHPFilter();
		m_pDCFilters[i].ComputeCoefs( 40.f, m_FXInfo.uSampleRate );
	}		
	return AK_Success;
}

// Compute EQ coefficients only when parameters have changed
void CAkGuitarDistortionFX::SetupEQs( AkGuitarDistortionFXParams & in_FXParams )
{
	for ( AkUInt32 i = 0; i < NUMPREDISTORTIONEQBANDS; ++i )
	{
		if ( in_FXParams.PreEQ[i].bHasChanged )
		{
			m_FXInfo.PreEQ.SetCoefficients(	i, 
										m_FXInfo.uSampleRate, 
										(DSP::FilterType) in_FXParams.PreEQ[i].eFilterType, 
										in_FXParams.PreEQ[i].fFrequency, 
										in_FXParams.PreEQ[i].fGain, 
										in_FXParams.PreEQ[i].fQFactor );
			m_FXInfo.PreEQ.SetBandActive( i, in_FXParams.PreEQ[i].bOnOff );
		}
	}
	for ( AkUInt32 i = 0; i < NUMPOSTDISTORTIONEQBANDS; ++i )
	{
		if ( in_FXParams.PostEQ[i].bHasChanged )
		{
			m_FXInfo.PostEQ.SetCoefficients(	i, 
										m_FXInfo.uSampleRate, 
										(DSP::FilterType) in_FXParams.PostEQ[i].eFilterType, 
										in_FXParams.PostEQ[i].fFrequency, 
										in_FXParams.PostEQ[i].fGain, 
										in_FXParams.PostEQ[i].fQFactor );
			m_FXInfo.PostEQ.SetBandActive( i, in_FXParams.PostEQ[i].bOnOff );
		}
	}
}

// Terminates.
AKRESULT CAkGuitarDistortionFX::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	// Terminate components
	m_FXInfo.PreEQ.Term( in_pAllocator );
	m_FXInfo.PostEQ.Term( in_pAllocator );
	if ( m_pDCFilters )
	{
		AK_PLUGIN_FREE( in_pAllocator, m_pDCFilters );
		m_pDCFilters = NULL;
	}
	AK_PLUGIN_DELETE( in_pAllocator, this );
	return AK_Success;
}

// Reset or seek to start.
AKRESULT CAkGuitarDistortionFX::Reset( )
{
	// Reset components
	m_FXInfo.PreEQ.Reset( );
	m_FXInfo.PostEQ.Reset( );
	for ( AkUInt32 i = 0 ; i < m_FXInfo.uNumChannels; i++ )
	{
		m_pDCFilters[i].Reset();
	}
	return AK_Success;
}


// Effect info query.
AKRESULT CAkGuitarDistortionFX::GetPluginInfo( AkPluginInfo & out_rPluginInfo )
{
	out_rPluginInfo.eType = AkPluginTypeEffect;
	out_rPluginInfo.bIsInPlace = true;
	out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
	return AK_Success;
}

//  Execute parametric EQ effect.
void CAkGuitarDistortionFX::Execute( AkAudioBuffer * io_pBuffer )
{
	// Compute changes if parameters have changed
	m_pSharedParams->GetParams( &m_FXInfo.Params );
	SetupEQs( m_FXInfo.Params );
	// Changes parameters for distortion and rectifier
	if ( m_FXInfo.Params.Distortion.bHasChanged )
	{
		m_FXInfo.Distortion.SetParameters(	(DSP::CAkDistortion::DistortionType)m_FXInfo.Params.Distortion.eDistortionType,
											m_FXInfo.Params.Distortion.fDrive,
											m_FXInfo.Params.Distortion.fTone );
		m_FXInfo.Rectifier.SetRectification( m_FXInfo.Params.Distortion.fRectification );
	}
	
	if ( AK_EXPECT_FALSE(io_pBuffer->uValidFrames < 32) )	//Internal processes don't support less than 32.
		return;

	AK_PERF_RECORDING_START( "GuitarDistortion", 25, 30 );

	// Save the dry signal for later
	// Note: Could reduce the size to just one channel if the helper classes processed channel by channel instead
	// Always generate full buffers for SIMD code
	io_pBuffer->ZeroPadToMaxFrames();
	const AkUInt32 uNumFrames = io_pBuffer->uValidFrames;
	const AkUInt32 uMaxFrames = io_pBuffer->MaxFrames();
	AKASSERT( m_FXInfo.uNumChannels == io_pBuffer->NumChannels() );
	AkReal32 * pfDryBufferStorage  = (AkReal32*) AK_PLUGIN_ALLOC( m_pAllocator, uMaxFrames * m_FXInfo.uNumChannels * sizeof(AkReal32) );
	if ( !pfDryBufferStorage )
		return; // Pipeline cannot handle errors here, just play dry signal when out of memory
	for ( AkUInt32 i = 0; i < m_FXInfo.uNumChannels; ++i )
	{
		AKPLATFORM::AkMemCpy(&pfDryBufferStorage[uMaxFrames*i], io_pBuffer->GetChannel(i), uNumFrames*sizeof(AkReal32) ); 
	}
	
	// DSP
	m_FXInfo.PreEQ.ProcessBuffer( io_pBuffer );
	m_FXInfo.Distortion.ProcessBuffer( io_pBuffer );
	m_FXInfo.Rectifier.ProcessBuffer( io_pBuffer );
	m_FXInfo.PostEQ.ProcessBuffer( io_pBuffer );
	
	const AkReal32 fCurrentDryGain = (100.f-m_FXInfo.fCurrentWetDryMix)*0.01f;
	const AkReal32 fTargetDryGain = (100.f-m_FXInfo.Params.fWetDryMix)*0.01f;
	const AkReal32 fCurrentWetGain = 1.f-fCurrentDryGain;
	const AkReal32 fTargetWetGain = 1.f-fTargetDryGain;
	for ( AkUInt32 i = 0 ; i < m_FXInfo.uNumChannels; i++ )
	{
		AkReal32 * pfChannel = io_pBuffer->GetChannel( i );
		DSP::Mix2Interp(	pfChannel, &pfDryBufferStorage[uMaxFrames*i], 
							fCurrentWetGain*m_FXInfo.fCurrentGain, fTargetWetGain*m_FXInfo.Params.fOutputLevel, 
							fCurrentDryGain*m_FXInfo.fCurrentGain, fTargetDryGain*m_FXInfo.Params.fOutputLevel, 
							uNumFrames );
		m_pDCFilters[i].ProcessBuffer( pfChannel, uNumFrames );
	}

	if ( pfDryBufferStorage )
	{
		AK_PLUGIN_FREE( m_pAllocator, pfDryBufferStorage );
		pfDryBufferStorage = NULL;
	}

	// Gain ramps have been reached
	m_FXInfo.fCurrentGain = m_FXInfo.Params.fOutputLevel;
	m_FXInfo.fCurrentWetDryMix = m_FXInfo.Params.fWetDryMix;

	AK_PERF_RECORDING_STOP( "GuitarDistortion", 25, 30 );
}
