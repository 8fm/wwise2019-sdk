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

#ifndef _AK_COMPRESSOR_H
#define _AK_COMPRESSOR_H

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/Tools/Common/AkAssert.h>

namespace DSP
{
	struct AkCompressorSideChain
	{
		AkReal32 fGainDb;
		AkReal32 fMem;
	};
	class CAkCompressor
	{
	public:
		CAkCompressor();
		AKRESULT Init( AK::IAkPluginMemAlloc * in_pAllocator,
					   bool in_bProcessLFE,
					   AkAudioFormat &	in_rFormat,
					   AkReal32 in_fCachedAttack,
					   AkReal32 in_fCachedRelease,
					   bool in_bChannelLink);

		void Term( AK::IAkPluginMemAlloc * in_pAllocator );
		void Reset();

		void ProcessBuffer( AkAudioBuffer * io_pBuffer,
							AkReal32 in_fThreshold,
							AkReal32 in_fRatio,
							AkReal32 in_fAttack,
							AkReal32 in_fRelease,
							AkReal32 in_fRange = -96.0f );

		void ProcessBuffer(AkAudioBuffer * io_pBuffer,
			AkAudioBuffer * in_pSideChain,
			AkReal32 in_fThreshold,
			AkReal32 in_fRatio,
			AkReal32 in_fAttack,
			AkReal32 in_fRelease,
			AkReal32 in_fRange = -96.0f);
		
		AkUInt32 GetNumSideChain(){ return m_uNumSideChain; };

		AkReal32 GetGainDb( AkUInt32 in_uChannel ) { return m_pSideChain[in_uChannel].fGainDb; };

		AkUInt32 GetNumChannels(){ return m_uNumChannels; };

		AkUInt32 GetNumProcessedChannels() { return m_uNumProcessedChannels; };

		bool GetProcessLFE() { return m_bProcessLFE; };

	private:
		void ProcessInternalSideChain(AkAudioBuffer * io_pBuffer,
			AkAudioBuffer * in_pSideChain,
			AkReal32 in_fThreshold,
			AkReal32 in_fRatio,
			AkReal32 in_fRange,
			AkUInt32 in_uNumProcessedChannels);
			
		void ProcessInternal(AkAudioBuffer * io_pBuffer,
							  AkReal32 in_fThreshold,
							  AkReal32 in_fRatio,
							  AkReal32 in_fRange,
							  AkUInt32 in_uNumProcessedChannels );

		void ProcessInternalLinked( AkAudioBuffer * io_pBuffer,
									AkReal32 in_fThreshold,
									AkReal32 in_fRatio,
									AkReal32 in_fRange,
									AkUInt32 in_uNumProcessedChannels );

	private:

		// Function ptr to the appropriate DSP execution routine
		void (CAkCompressor::*m_fpPerformDSP)  ( AkAudioBuffer * io_pBuffer,
												 AkReal32 in_fThreshold,
												 AkReal32 in_fRatio,
												 AkReal32 in_fRange,
												 AkUInt32 in_uNumProcessedChannels);
	private:

		// Compressor settings
		static const AkReal32 RMSWINDOWLENGTH;	// ~ 1 audio buffer
		static const AkReal32 SMALL_DC_OFFSET;	// DC offset to avoid log(0)
		// Note: Scaling factor is such that time specified corresponds to time to reach 90% of target value
		static const AkReal32 SCALE_RAMP_TIME;		// Correction factor to make attack/release times accurate

		// Audio format information
		AkUInt32	m_uNumChannels;
		AkReal32	m_fSampleRate;

		// Processed channels
		AkUInt32	m_uNumProcessedChannels;

		// Side chain
		AkUInt32	m_uNumSideChain;
		AkReal32	m_fRMSFilterCoef;			// RMS filter coefficient (same for all sidechains)
		AkCompressorSideChain *	m_pSideChain;	// RMS evaluation sidechain

		// Cached values for optimization
		AkReal32 m_fCachedAttack;
		AkReal32 m_fCachedAttackCoef;
		AkReal32 m_fCachedRelease;
		AkReal32 m_fCachedReleaseCoef;

		bool 	m_bProcessLFE;				// LFE behavior
	};
}

#endif // _AK_COMPRESSOR_H