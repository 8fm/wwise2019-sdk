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

#include "AkCompressor.h"
#include <AK/Tools/Common/AkObject.h> // Placement new
#include <AK/Tools/Common/AkAssert.h>
#include <stdlib.h>
#include "AkMath.h"
#include "AkDSPUtils.h"

namespace DSP
{

	const AkReal32 CAkCompressor::RMSWINDOWLENGTH = 0.02322f;	// ~ 1 audio buffer
	const AkReal32 CAkCompressor::SMALL_DC_OFFSET = 1.e-25f;	// DC offset to avoid log(0)
	// Note: Scaling factor is such that time specified corresponds to time to reach 90% of target value
	const AkReal32 CAkCompressor::SCALE_RAMP_TIME = 2.2f;		// Correction factor to make attack/release times accurate

	CAkCompressor::CAkCompressor()
		: m_pSideChain( NULL )
	{
	}

	AKRESULT CAkCompressor::Init( AK::IAkPluginMemAlloc * in_pAllocator,
								  bool in_bProcessLFE,
								  AkAudioFormat & in_rFormat,
								  AkReal32 in_fCachedAttack,
								  AkReal32 in_fCachedRelease,
								  bool in_bChannelLink )
	{
		// Should not be able to change those at run-time (Wwise only problem)
		m_bProcessLFE = in_bProcessLFE;

		// Audio settings
		m_uNumChannels = in_rFormat.GetNumChannels();
		m_fSampleRate = (AkReal32) in_rFormat.uSampleRate;

		// Initialized cached values
		m_fCachedAttack = in_fCachedAttack;
		m_fCachedAttackCoef = expf( -SCALE_RAMP_TIME / (m_fCachedAttack * m_fSampleRate) );
		m_fCachedRelease = in_fCachedRelease;
		m_fCachedReleaseCoef = expf( -SCALE_RAMP_TIME / (m_fCachedRelease * m_fSampleRate) );

		// Note: 1 channel case can be handled by both routines, faster with unlinked process
		m_uNumProcessedChannels = m_uNumChannels;
		if ( in_rFormat.HasLFE() && !m_bProcessLFE )
		{
			--m_uNumProcessedChannels;
		}
		if ( !in_bChannelLink || m_uNumChannels == 1 )
		{
			m_fpPerformDSP = &CAkCompressor::ProcessInternal;
		}
		else
		{
			m_fpPerformDSP = &CAkCompressor::ProcessInternalLinked;
		}
		m_uNumSideChain = in_bChannelLink ? 1 : m_uNumProcessedChannels;

		// Algorithm specific initializations
		m_pSideChain = (AkCompressorSideChain*)AK_PLUGIN_ALLOC( in_pAllocator, sizeof( AkCompressorSideChain )*m_uNumSideChain );
		if ( !m_pSideChain )
		{
			return AK_InsufficientMemory;
		}

		// Sidechain initialization
		m_fRMSFilterCoef = expf( -1.0f / (RMSWINDOWLENGTH * m_fSampleRate) );

		return AK_Success;
	}

	void CAkCompressor::Term( AK::IAkPluginMemAlloc * in_pAllocator )
	{
		if ( m_pSideChain )
		{
			AK_PLUGIN_FREE( in_pAllocator, m_pSideChain );
			m_pSideChain = NULL;
		}
	}

	void CAkCompressor::Reset()
	{
		// Sidechain initialization
		for ( AkUInt32 i = 0; i < m_uNumSideChain; ++i )
		{
			m_pSideChain[i].fGainDb = 0.0f;
			m_pSideChain[i].fMem = 0.0f;
		}
	}

	void CAkCompressor::ProcessBuffer( AkAudioBuffer * io_pBuffer,
									   AkReal32 in_fThreshold,
									   AkReal32 in_fRatio,
									   AkReal32 in_fAttack,
									   AkReal32 in_fRelease,
									   AkReal32 in_fRange )
	{
		AkReal32 fAttack = in_fAttack;
		if ( fAttack != m_fCachedAttack )
		{
			m_fCachedAttack = fAttack;
			m_fCachedAttackCoef = expf( -SCALE_RAMP_TIME / (fAttack * m_fSampleRate) );
		}
		AkReal32 fRelease = in_fRelease;
		if ( fRelease != m_fCachedRelease )
		{
			m_fCachedRelease = fRelease;
			m_fCachedReleaseCoef = expf( -SCALE_RAMP_TIME / (fRelease * m_fSampleRate) );
		}

		// Watchout: Params.bChannelLink and bProcessLFE may have been changed by Wwise
		// These parameters don't support run-time change and are ignored by DSP routine
		(this->*m_fpPerformDSP)(io_pBuffer, in_fThreshold, (1.f / in_fRatio) - 1.f, in_fRange, m_uNumProcessedChannels);
	}

	void CAkCompressor::ProcessBuffer(AkAudioBuffer * io_pBuffer,
		AkAudioBuffer * in_pSideChain,
		AkReal32 in_fThreshold,
		AkReal32 in_fRatio,
		AkReal32 in_fAttack,
		AkReal32 in_fRelease,
		AkReal32 in_fRange)
	{
		AkReal32 fAttack = in_fAttack;
		if (fAttack != m_fCachedAttack)
		{
			m_fCachedAttack = fAttack;
			m_fCachedAttackCoef = expf(-SCALE_RAMP_TIME / (fAttack * m_fSampleRate));
		}
		AkReal32 fRelease = in_fRelease;
		if (fRelease != m_fCachedRelease)
		{
			m_fCachedRelease = fRelease;
			m_fCachedReleaseCoef = expf(-SCALE_RAMP_TIME / (fRelease * m_fSampleRate));
		}

		// Watchout: Params.bChannelLink and bProcessLFE may have been changed by Wwise
		// These parameters don't support run-time change and are ignored by DSP routine
		ProcessInternalSideChain(io_pBuffer, in_pSideChain, in_fThreshold, (1.f / in_fRatio) - 1.f, in_fRange, m_uNumProcessedChannels);
	}

	void CAkCompressor::ProcessInternalSideChain(AkAudioBuffer * io_pBuffer,
		AkAudioBuffer * in_pSideChain,
		AkReal32 in_fThreshold,
		AkReal32 in_fRatio,
		AkReal32 in_fRange,
		AkUInt32 in_uNumProcessedChannels)
	{
		AKASSERT(in_uNumProcessedChannels <= m_uNumSideChain); // Unlinked processing
		AkReal32 fAttackCoef = m_fCachedAttackCoef;
		AkReal32 fReleaseCoef = m_fCachedReleaseCoef;
		AkReal32 fRMSFilterCoef = m_fRMSFilterCoef;

		// Skip LFE if necessary (LFE is always the last channel).
		for (AkUInt32 uChan = 0; uChan < in_uNumProcessedChannels; ++uChan)
		{
			AkReal32 * AK_RESTRICT pfBuf = io_pBuffer->GetChannel(uChan);
			AkReal32 * AK_RESTRICT pfBufSideChain = in_pSideChain->GetChannel(uChan);
			AkReal32 * AK_RESTRICT pfEnd = pfBuf + io_pBuffer->uValidFrames;

			// Optimization: Local variables
			AkReal32 fLocGainDb = m_pSideChain[uChan].fGainDb;
			AkReal32 fLocRMSFilterMem = m_pSideChain[uChan].fMem;

			do
			{
				// Sidechain computations
				AkReal32 fSqIn = *pfBufSideChain * *pfBufSideChain;					// x[n]^2 (rectification)
				fSqIn += SMALL_DC_OFFSET;							// Avoid log 0
				fLocRMSFilterMem = fSqIn + fRMSFilterCoef * (fLocRMSFilterMem - fSqIn);	// Run one-pole filter

				AkReal32 fPowerDb = 10.f*AK::FastLog10(fLocRMSFilterMem);	// Convert power estimation to dB
				AkReal32 fDbOverThresh = fPowerDb - in_fThreshold;		// Offset into non-linear range (over threshold)
				fDbOverThresh = AK_FPMax(fDbOverThresh, 0.f);

				// Attack and release smoothing
				AkReal32 fCoef = AK_FSEL(fDbOverThresh - fLocGainDb, fAttackCoef, fReleaseCoef);
				fLocGainDb = fDbOverThresh + fCoef * (fLocGainDb - fDbOverThresh);

				// Static transfer function evaluation
				AkReal32 fGainReductiondB = fLocGainDb * in_fRatio;				// Gain reduction (dB)

				// in_fRange clamps fGainReductiondB here
				fGainReductiondB = AK_FPMax(in_fRange, fGainReductiondB);

				AkReal32 fGainReduction = AK::dBToLin(fGainReductiondB);			// Convert to linear

				// Apply compression gain
				*pfBuf++ *= fGainReduction;
			} while (pfBuf < pfEnd);

			// Save local variables
			m_pSideChain[uChan].fGainDb = fLocGainDb;
			m_pSideChain[uChan].fMem = fLocRMSFilterMem;
		}
	}

	void CAkCompressor::ProcessInternal( AkAudioBuffer * io_pBuffer,
										 AkReal32 in_fThreshold,
										 AkReal32 in_fRatio,
										 AkReal32 in_fRange,
										 AkUInt32 in_uNumProcessedChannels )
	{
		AKASSERT( in_uNumProcessedChannels <= m_uNumSideChain ); // Unlinked processing
		AkReal32 fAttackCoef = m_fCachedAttackCoef;
		AkReal32 fReleaseCoef = m_fCachedReleaseCoef;		
		AkReal32 fRMSFilterCoef = m_fRMSFilterCoef;

		// Skip LFE if necessary (LFE is always the last channel).
		for ( AkUInt32 uChan = 0; uChan < in_uNumProcessedChannels; ++uChan )
		{
			AkReal32 * AK_RESTRICT pfBuf = io_pBuffer->GetChannel( uChan );
			AkReal32 * AK_RESTRICT pfEnd = pfBuf + io_pBuffer->uValidFrames;

			// Optimization: Local variables
			AkReal32 fLocGainDb = m_pSideChain[uChan].fGainDb;
			AkReal32 fLocRMSFilterMem = m_pSideChain[uChan].fMem;
			
			do
			{
				// Sidechain computations
				AkReal32 fSqIn = *pfBuf * *pfBuf;					// x[n]^2 (rectification)
				fSqIn += SMALL_DC_OFFSET;							// Avoid log 0
				fLocRMSFilterMem = fSqIn + fRMSFilterCoef * (fLocRMSFilterMem - fSqIn);	// Run one-pole filter

				AkReal32 fPowerDb = 10.f*AK::FastLog10( fLocRMSFilterMem );	// Convert power estimation to dB
				AkReal32 fDbOverThresh = fPowerDb - in_fThreshold;		// Offset into non-linear range (over threshold)
				fDbOverThresh = AK_FPMax( fDbOverThresh, 0.f );

				// Attack and release smoothing
				AkReal32 fCoef = AK_FSEL( fDbOverThresh - fLocGainDb, fAttackCoef, fReleaseCoef );
				fLocGainDb = fDbOverThresh + fCoef * (fLocGainDb - fDbOverThresh);

				// Static transfer function evaluation
				AkReal32 fGainReductiondB = fLocGainDb * in_fRatio;				// Gain reduction (dB)

				// in_fRange clamps fGainReductiondB here
				fGainReductiondB = AK_FPMax( in_fRange, fGainReductiondB );
				
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

	void CAkCompressor::ProcessInternalLinked( AkAudioBuffer * io_pBuffer,
											   AkReal32 in_fThreshold,
											   AkReal32 in_fRatio,
											   AkReal32 in_fRange,
											   AkUInt32 in_uNumProcessedChannels )
	{
		AKASSERT( m_uNumSideChain == 1 ); // Linked processing
		// Note: Using average power from all processed channels to determine whether to compress or not
		// Skip LFE if necessary (LFE is always the last channel).
		const AkReal32 fOneOverNumProcessedChannels = 1.f / in_uNumProcessedChannels;
		AkReal32 fAttackCoef = m_fCachedAttackCoef;
		AkReal32 fReleaseCoef = m_fCachedReleaseCoef;
		AkReal32 fRMSFilterCoef = m_fRMSFilterCoef;
		AkReal32 fLocGainDb = m_pSideChain[0].fGainDb;
		AkReal32 fLocRMSFilterMem = m_pSideChain[0].fMem;

		AkReal32 * pfBuf = io_pBuffer->GetChannel( 0 );
		AkReal32 * pfEnd = pfBuf + io_pBuffer->uValidFrames;

		AkUInt32 uChannelStride = io_pBuffer->MaxFrames();

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
			fLocRMSFilterMem = fAvgPower + fRMSFilterCoef * (fLocRMSFilterMem - fAvgPower);	// Run one-pole filter
			AkReal32 fPowerDb = 10.f*AK::FastLog10( fLocRMSFilterMem );	// Convert power estimation to dB (sqrt taken out)
			AkReal32 fDbOverThresh = fPowerDb - in_fThreshold;		// Offset into non-linear range (over threshold)
			fDbOverThresh = AK_FPMax( fDbOverThresh, 0.f );

			// Attack and release smoothing
			AkReal32 fCoef = AK_FSEL( fDbOverThresh - fLocGainDb, fAttackCoef, fReleaseCoef );
			fLocGainDb = fDbOverThresh + fCoef * (fLocGainDb - fDbOverThresh);

			// Static transfer function evaluation
			AkReal32 fGainReductiondB = fLocGainDb * in_fRatio;				// Gain reduction (dB)

			// in_fRange clamps fGainReductiondB here
			fGainReductiondB = AK_FPMax( in_fRange, fGainReductiondB );

			AkReal32 fGainReduction = AK::dBToLin( fGainReductiondB );			// Convert to linear

			// Apply compression gain to all channels
			AkReal32 * pfOutChan = pfBuf;
			for ( AkUInt32 uChan = 0; uChan < in_uNumProcessedChannels; ++uChan )
			{
				*pfOutChan *= fGainReduction;
				pfOutChan += uChannelStride;
			}
		}
		while ( ++pfBuf<pfEnd );

		// Save local variables
		m_pSideChain[0].fMem = fLocRMSFilterMem;
		m_pSideChain[0].fGainDb = fLocGainDb;
	}
}