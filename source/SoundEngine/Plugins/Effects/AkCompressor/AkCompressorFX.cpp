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

//////////////////////////////////////////////////////////////////////
//
// AkCompressorFX.cpp
//
// Compressor processing FX implementation.
// 
//
// Copyright 2020 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#include "AkCompressorFX.h"
#include <AK/DSP/AkApplyGain.h>
#include "AkMath.h"
#include "AkDSPUtils.h"
#include <AK/AkWwiseSDKVersion.h>

static const AkReal32 RMSWINDOWLENGTH = 0.02322f;	// ~ 1 audio buffer
static const AkReal32 SMALL_DC_OFFSET = 1.e-25f;	// DC offset to avoid log(0)
// Note: Scaling factor is such that time specified corresponds to time to reach 90% of target value
static const AkReal32 SCALE_RAMP_TIME = 2.2f;		// Correction factor to make attack/release times accurate

// Plugin mechanism. Dynamic create function whose address must be registered to the FX manager.
AK::IAkPlugin* CreateAkCompressorFX( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AKASSERT( in_pAllocator != NULL );
	return AK_PLUGIN_NEW( in_pAllocator, CAkCompressorFX( ) );
}

// Constructor.
CAkCompressorFX::CAkCompressorFX()
{
	m_pSharedParams = NULL;
	m_fpPerformDSP = NULL;
	m_pSideChain = NULL;
#ifndef AK_OPTIMIZED
	m_pCtx = NULL;
#endif
}

// Destructor.
CAkCompressorFX::~CAkCompressorFX()
{

}

inline static AkReal32 GetCachedCoeffecient( AkReal32 in_fCachedTime, AkReal32 in_fSampleRate )
{
	AkReal32 fAttackRelease = AkMax( in_fCachedTime, 0.001f );
	return expf( -SCALE_RAMP_TIME / (fAttackRelease * in_fSampleRate) );
}

// Initializes and allocate memory for the effect
AKRESULT CAkCompressorFX::Init(	AK::IAkPluginMemAlloc * in_pAllocator,		// Memory allocator interface.
								AK::IAkEffectPluginContext * in_pFXCtx,		// FX Context
								AK::IAkPluginParam * in_pParams,			// Effect parameters.
								AkAudioFormat &	in_rFormat					// Required audio input format.
								)
{
	// Set parameters interface and retrieve init params.
	m_pSharedParams = static_cast<CAkCompressorFXParams*>(in_pParams);
	const AkCompressorFXParams & params = m_pSharedParams->GetParams();

#ifndef AK_OPTIMIZED
	m_pCtx = in_pFXCtx;
#endif

	// Should not be able to change those at run-time (Wwise only problem)
	m_bProcessLFE = params.bProcessLFE;

	// Allocate sidechain resources
	m_uNumChannels = in_rFormat.GetNumChannels( );
	m_fSampleRate = (AkReal32) in_rFormat.uSampleRate;

	// Initialized cached values
	m_fCachedAttack = params.fAttack;
	m_fCachedAttackCoef = GetCachedCoeffecient( m_fCachedAttack, m_fSampleRate );
	m_fCachedRelease = params.fRelease;
	m_fCachedReleaseCoef = GetCachedCoeffecient( m_fCachedRelease, m_fSampleRate );

	// Note: 1 channel case can be handled by both routines, faster with unlinked process
	m_uNumProcessedChannels = m_uNumChannels;
	if ( in_rFormat.HasLFE() && !params.bProcessLFE )
	{
		--m_uNumProcessedChannels;
	}
	if ( params.bChannelLink == false || m_uNumChannels == 1 )
	{
		m_fpPerformDSP = &CAkCompressorFX::Process;
	}
	else
	{
		m_fpPerformDSP = &CAkCompressorFX::ProcessLinked;
	}
	m_uNumSideChain = params.bChannelLink ? 1 : m_uNumProcessedChannels;

	// Algorithm specific initializations
	m_pSideChain = (AkCompressorSideChain*)AK_PLUGIN_ALLOC( in_pAllocator, sizeof(AkCompressorSideChain)*m_uNumSideChain );	
	if ( m_pSideChain == NULL )
		return AK_InsufficientMemory;
	
	// Sidechain initialization
	m_fRMSFilterCoef = expf( -1.f / ( RMSWINDOWLENGTH * m_fSampleRate ) );

	// Gain ramp initialization for Output level
	m_fCurrentGain = params.fOutputLevel;

	AK_PERF_RECORDING_RESET();

	return AK_Success;
}

// Terminates.
AKRESULT CAkCompressorFX::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	if ( m_pSideChain )
		AK_PLUGIN_FREE( in_pAllocator, m_pSideChain );
	AK_PLUGIN_DELETE( in_pAllocator, this );
	return AK_Success;
}

// Reset
AKRESULT CAkCompressorFX::Reset( )
{
	// Sidechain initialization
	for (AkUInt32 i = 0; i < m_uNumSideChain; ++i )
	{
		m_pSideChain[i].fGainDb = 0.f;
		m_pSideChain[i].fMem = 0.f;
	}
	return AK_Success;
}

// Effect info query.
AKRESULT CAkCompressorFX::GetPluginInfo( AkPluginInfo & out_rPluginInfo )
{
	out_rPluginInfo.eType = AkPluginTypeEffect;
	out_rPluginInfo.bIsInPlace = true;
	out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
	return AK_Success;
}

// Helper function for monitoring
#ifndef AK_OPTIMIZED
static void _FindPeaks( AkAudioBuffer* io_pBuffer, AkReal32 * out_pPeaks, AkUInt32 in_cChannels )
{
	for ( AkUInt32 uChan = 0; uChan < in_cChannels; ++uChan )
	{
		AkReal32 * AK_RESTRICT pfBuf = io_pBuffer->GetChannel( uChan );
		AkReal32 * AK_RESTRICT pfEnd = pfBuf + io_pBuffer->uValidFrames;

		AkReal32 fPeak = 0;

		while ( pfBuf < pfEnd )
		{
			AkReal32 fIn = fabsf( *pfBuf++ );
			fPeak = AK_FPMax( fIn, fPeak );
		}

		out_pPeaks[uChan] = fPeak;
	}
}
#endif

//-----------------------------------------------------------------------------
// Name: Execute
// Desc: Execute dynamics processing effect.
//-----------------------------------------------------------------------------
void CAkCompressorFX::Execute(	AkAudioBuffer * io_pBuffer ) 
{
	if ( io_pBuffer->uValidFrames == 0 )
		return;

	const AkCompressorFXParams & params = m_pSharedParams->GetParams();

#ifndef AK_OPTIMIZED
	char * pMonitorData = NULL;
	int sizeofData = 0;
	if ( m_pCtx->CanPostMonitorData() )
	{
		sizeofData = sizeof( AkUInt32 ) * 2
			+ sizeof( AkReal32 ) * m_uNumProcessedChannels * 2
			+ sizeof( AkReal32 ) * m_uNumSideChain;
		pMonitorData = (char *)AkAlloca( sizeofData );

		AkChannelConfig channelConfig = m_bProcessLFE ? io_pBuffer->GetChannelConfig() : io_pBuffer->GetChannelConfig().RemoveLFE();
		AKASSERT( channelConfig.uNumChannels == m_uNumProcessedChannels );

		*((AkUInt32 *)pMonitorData) = channelConfig.Serialize();
		*((AkUInt32 *)(pMonitorData + 4)) = (AkUInt32)m_uNumSideChain;

		_FindPeaks( io_pBuffer, (AkReal32 *)pMonitorData + 2, m_uNumProcessedChannels );
	}
#endif


	AK_PERF_RECORDING_START( "Compressor", 25, 30 );

	AkReal32 fAttack = params.fAttack;
	if ( fAttack != m_fCachedAttack )
	{
		m_fCachedAttack = fAttack;
		m_fCachedAttackCoef = GetCachedCoeffecient( m_fCachedAttack, m_fSampleRate );
	}

	AkReal32 fRelease = params.fRelease;
	if ( fRelease != m_fCachedRelease )
	{
		m_fCachedRelease = fRelease;
		m_fCachedReleaseCoef = GetCachedCoeffecient( m_fCachedRelease, m_fSampleRate );
	}


	// Watchout: Params.bChannelLink and bProcessLFE may have been changed by Wwise
	// These parameters don't support run-time change and are ignored by DSP routine
	(this->*m_fpPerformDSP)(io_pBuffer, params.fThreshold, (1.f / params.fRatio) - 1.f, m_uNumProcessedChannels);

	// Apply output gain
	AK::DSP::ApplyGain( io_pBuffer, m_fCurrentGain, params.fOutputLevel, m_bProcessLFE );
	m_fCurrentGain = params.fOutputLevel;

	AK_PERF_RECORDING_STOP( "Compressor", 25, 30 );

#ifndef AK_OPTIMIZED
	if ( pMonitorData )
	{
		_FindPeaks( io_pBuffer, (AkReal32 *)pMonitorData + m_uNumProcessedChannels + 2, m_uNumProcessedChannels );

		AkReal32 fRatioFactor = (1.f / params.fRatio) - 1.f;
		for ( AkUInt32 uChan = 0; uChan < m_uNumSideChain; ++uChan )
			((AkReal32 *)pMonitorData)[m_uNumProcessedChannels * 2 + uChan + 2] = m_pSideChain[uChan].fGainDb * fRatioFactor;

		m_pCtx->PostMonitorData( pMonitorData, sizeofData );
	}
#endif
}

void CAkCompressorFX::Process( AkAudioBuffer * io_pBufferIn, AkReal32 in_fThresh, AkReal32 in_fRatioFactor, AkUInt32 in_uNumProcessedChannels)
{
	AKASSERT( in_uNumProcessedChannels <= m_uNumSideChain ); // Unlinked processing
	AkReal32 fAttackCoef = m_fCachedAttackCoef;
	AkReal32 fReleaseCoef = m_fCachedReleaseCoef;
	AkReal32 fRMSFilterCoef = m_fRMSFilterCoef;

	// Skip LFE if necessary (LFE is always the last channel).
	for ( AkUInt32 uChan = 0; uChan < in_uNumProcessedChannels; ++uChan )
	{
		AkReal32 * AK_RESTRICT pfBuf = io_pBufferIn->GetChannel(uChan);	
		AkReal32 * AK_RESTRICT pfEnd = pfBuf + io_pBufferIn->uValidFrames;

		// Optimization: Local variables
		AkReal32 fLocGainDb = m_pSideChain[uChan].fGainDb;
		AkReal32 fLocRMSFilterMem = m_pSideChain[uChan].fMem;

		do
		{
			// Sidechain computations
			AkReal32 fSqIn = *pfBuf * *pfBuf;					// x[n]^2 (rectification)
			fSqIn += SMALL_DC_OFFSET;							// Avoid log 0
			fLocRMSFilterMem = fSqIn + fRMSFilterCoef * ( fLocRMSFilterMem - fSqIn );	// Run one-pole filter
			
			AkReal32 fPowerDb = 10.f*AK::FastLog10( fLocRMSFilterMem );	// Convert power estimation to dB
			AkReal32 fDbOverThresh = fPowerDb - in_fThresh;		// Offset into non-linear range (over threshold)
			fDbOverThresh = AK_FPMax( fDbOverThresh, 0.f );

			// Attack and release smoothing
			AkReal32 fCoef = AK_FSEL(fDbOverThresh - fLocGainDb, fAttackCoef, fReleaseCoef );
			fLocGainDb = fDbOverThresh + fCoef * ( fLocGainDb - fDbOverThresh );
	
			// Static transfer function evaluation
			AkReal32 fGainReductiondB = fLocGainDb * in_fRatioFactor;				// Gain reduction (dB)
			AkReal32 fGainReduction = AK::dBToLin( fGainReductiondB );			// Convert to linear

			// Apply compression gain
			*pfBuf++ *= fGainReduction;
		}
		while ( pfBuf < pfEnd );

		// Save local variables
		m_pSideChain[uChan].fGainDb = fLocGainDb;
		m_pSideChain[uChan].fMem = fLocRMSFilterMem;
	}
}

void CAkCompressorFX::ProcessLinked( AkAudioBuffer * io_pBufferIn, AkReal32 in_fThresh, AkReal32 in_fRatioFactor, AkUInt32 in_uNumProcessedChannels )
{
	AKASSERT( m_uNumSideChain == 1); // Linked processing
	// Note: Using average power from all processed channels to determine whether to compress or not
	// Skip LFE if necessary (LFE is always the last channel).
	const AkReal32 fOneOverNumProcessedChannels = 1.f/in_uNumProcessedChannels;
	AkReal32 fAttackCoef = m_fCachedAttackCoef;
	AkReal32 fReleaseCoef = m_fCachedReleaseCoef;
	AkReal32 fRMSFilterCoef = m_fRMSFilterCoef;
	AkReal32 fLocGainDb = m_pSideChain[0].fGainDb;
	AkReal32 fLocRMSFilterMem = m_pSideChain[0].fMem;

	AkReal32 * pfBuf = io_pBufferIn->GetChannel(0);	
	AkReal32 * pfEnd = pfBuf + io_pBufferIn->uValidFrames;

	AkUInt32 uChannelStride = io_pBufferIn->MaxFrames();

	do
	{
		AkReal32 fAvgPower = 0.f;

		AkReal32 * pfInChan = pfBuf;
		for ( AkUInt32 uChan = 0; uChan < in_uNumProcessedChannels; ++uChan )
		{
			AkReal32 fIn = *pfInChan;
			pfInChan += uChannelStride;
			fAvgPower += fIn * fIn;			// x[n]^2 (rectification)		
		}

		fAvgPower *= fOneOverNumProcessedChannels;
		fAvgPower += SMALL_DC_OFFSET;							// Avoid log 0 problems
		fLocRMSFilterMem = fAvgPower + fRMSFilterCoef * ( fLocRMSFilterMem - fAvgPower );	// Run one-pole filter
		AkReal32 fPowerDb = 10.f*AK::FastLog10( fLocRMSFilterMem );	// Convert power estimation to dB (sqrt taken out)
		AkReal32 fDbOverThresh = fPowerDb - in_fThresh;		// Offset into non-linear range (over threshold)
		fDbOverThresh = AK_FPMax( fDbOverThresh, 0.f );

		// Attack and release smoothing
		AkReal32 fCoef = AK_FSEL(fDbOverThresh - fLocGainDb, fAttackCoef, fReleaseCoef );
		fLocGainDb = fDbOverThresh + fCoef * ( fLocGainDb - fDbOverThresh );

		// Static transfer function evaluation
		AkReal32 fGainReductiondB = fLocGainDb * in_fRatioFactor;				// Gain reduction (dB)
		AkReal32 fGainReduction = AK::dBToLin( fGainReductiondB );			// Convert to linear

		// Apply compression gain to all channels
		AkReal32 * pfOutChan = pfBuf;
		for ( AkUInt32 uChan = 0; uChan < in_uNumProcessedChannels; ++uChan )
		{
			*pfOutChan *= fGainReduction;
			pfOutChan += uChannelStride;
		}
	}
	while (++pfBuf<pfEnd);

	// Save local variables
	m_pSideChain[0].fGainDb = fLocGainDb;
	m_pSideChain[0].fMem = fLocRMSFilterMem;
}
