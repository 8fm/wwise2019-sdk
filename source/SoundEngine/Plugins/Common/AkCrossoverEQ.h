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

#ifndef _AK_CROSSOVEREQ_H
#define _AK_CROSSOVEREQ_H

#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include "AkMultiBandEQ.h"
#include "BiquadFilter.h"
#include <AK/Tools/Common/AkAssert.h>

#define MAX_CROSSOVER_BANDS 5

namespace DSP
{
	/*
	// Topology for LR4 filter indexing
	// Size of tree + double for for top 2 diagonal stages
	// # of filters for N bands = (N*(N+1))/2 - 1 + 2*(N-1)
												  +-> [HP20/21] -->
									+-> [HP16/17] +-> [LP18/19] -->
					  +-> [HP11/12] +-> [LP13/14] --> [AP15]	-->
		  +-> [HP5/6] +-> [LP7/8]	--> [AP9]	  --> [AP10]	-->
	x(n) -+-> [LP0/1] --> [AP2]		--> [AP3]	  --> [AP4]		-->
	edge freq  e0		   e1		      e2		   e4

	// LR2
	// # of filters for N bands = (N*(N+1))/2 - 1
	// TODO: Since AP are 1st order, look into an optimized first order filter implementation
										 +-> [HP13] -->
							  +-> [HP11] +-> [LP12] -->
					+-> [HP8] +-> [LP9]  --> [AP10] -->
		  +-> [HP4] +-> [LP5] --> [AP6]  --> [AP7]  -->
	x(n) -+-> [LP0] --> [AP1] --> [AP2]  --> [AP3]  -->
	edge freq  e0		 e1		   e2		  e4
	When changing band number, select tap points from lower stage
	*/
	class CAkCrossoverEQ : public CAkMultiBandEQ
	{
	public:

		// Override the init to allocate all filters for the tree, as opposed to number of bands
		AKRESULT Init( AK::IAkPluginMemAlloc * in_pAllocator,
					   AkUInt16 in_uNumChannels,
					   AkUInt16 in_uMaxBands,
					   bool in_LR4);

		// Set the frequency for a specific band edge
		void SetCoefficients( AkUInt32 in_uEdge, AkUInt32 in_uSampleRate, AkReal32 in_fFreq );

		// Override to return only one subband, calling function is responsible for assembling subbands
		// Gain is set outside of the function
		void ProcessBuffer( AkAudioBuffer * io_pBuffers, AkUInt32 in_uNumBands );

	private:
		// Override to return subbands
		// Gain is set outside of the function
		void ProcessBufferInternalLR4( DSP::BiquadFilterMultiSIMD * pFilter, AkAudioBuffer * io_pBuffers, AkUInt32 in_uNumBands );
		void ProcessBufferInternalLR2( DSP::BiquadFilterMultiSIMD * pFilter, AkAudioBuffer * io_pBuffers, AkUInt32 in_uNumBands );

		// Computer LR2 Biquad Sections
		void SetLR2Coefs( AkUInt32 in_uEdge, AkReal32 in_fSampleRate, DSP::FilterType in_eCurve, AkReal32 in_fFreq );

		void SetAllPassCoefs( AkUInt32 in_uEdge, AkReal32 in_fSampleRate, AkReal32 in_fFreq, bool in_LR4 );

	private:
		void (CAkCrossoverEQ::*m_fpPerformDSP) ( DSP::BiquadFilterMultiSIMD * pFilter, AkAudioBuffer * io_pBuffers, AkUInt32 in_uNumBands );
		bool m_LR4; // Boolean to determine LR4/LR2 design - True for LR4 design
	};
}

#endif // _AK_CROSSOVEREQ_H