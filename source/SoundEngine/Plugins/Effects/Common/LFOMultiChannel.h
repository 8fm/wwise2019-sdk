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

#ifndef _AKLFO_MULTICHANNEL_H_
#define _AKLFO_MULTICHANNEL_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/Tools/Common/AkObject.h>

#include "IAkGlobalPluginContextEx.h"
#include "LFOMono.h"

// ---------------------------------------------------------------------
// MultiChannelLFO.
// Holds N LFO channels. This class handles computation of individual
// initial phase offsets for each channel based on the phase parameters.
// Hosts a polarity policy.
//
// Usage:
// - Instantiate a MultiChannelLFO with desired polarity 
//		typedef MultiChannelLFO<Unipolar>	UnipolarMultiChannelLFO;
//		or
//		typedef MultiChannelLFO<Bipolar>	BipolarMultiChannelLFO;
// - Init(), then call SetParams() whenever parameters change.
// - Produce samples for each channel: 
//		Access each channel (GetChannel()), then 
//		either call single-sample production methods 
//		explicitly inside a for() loop (see example below), or allocate 
//		a buffer and call ProduceBuffer().
// ---------------------------------------------------------------------

namespace DSP
{
	template < class PolarityPolicy, class OutputPolicy >
	class MultiChannelLFO
	{
	public:

		MultiChannelLFO() 
			:
#ifndef AK_VOICE_MAX_NUM_CHANNELS
			  m_arLfo( NULL ), 
#endif
			m_uNumChannels( 0 )
			{}
		
		// Init multichannel LFO. Instantiates and initialize all internal single channel LFOs, with
		// proper individual phase offsets based on the phase parameters.
		AKRESULT Setup(
			AK::IAkPluginMemAlloc *	in_pAllocator,
			AK::IAkGlobalPluginContext * in_pGlobalCtx,
			AkChannelConfig in_channelConfig,// Channel configuration.
			AkUInt32 		in_uSampleRate,	// Output sample rate (Hz).
			const LFO::MultiChannel::AllParams & in_params		// LFO parameters.
			)
		{
			Release( in_pAllocator );

			if ( in_channelConfig.uNumChannels > 0 )
			{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
				m_arLfo = (MonoLFO<PolarityPolicy, OutputPolicy> *) AK_PLUGIN_ALLOC( in_pAllocator, in_channelConfig.uNumChannels * sizeof( MonoLFO<PolarityPolicy, OutputPolicy> ) );
				if ( !m_arLfo )
					return AK_InsufficientMemory;
#endif
				m_uNumChannels = in_channelConfig.uNumChannels;

				AkReal32 * arChannelOffsets = (AkReal32*)AkAlloca( m_uNumChannels * sizeof( AkReal32 ) );
				
				LFO::MultiChannel::ComputeInitialPhase(
					in_pGlobalCtx,
					in_channelConfig, 
					in_params.phaseParams,
					arChannelOffsets
					);

				for ( AkUInt32 iChan=0; iChan<m_uNumChannels; iChan++ )
				{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
					AkPlacementNew( m_arLfo + iChan ) DSP::MonoLFO<PolarityPolicy, OutputPolicy>;
#endif

					m_arLfo[iChan].Setup(
						in_uSampleRate, 
						in_params.lfoParams,
						arChannelOffsets[iChan],
						static_cast<AK::IAkGlobalPluginContextEx *>(in_pGlobalCtx)->Random());
				}
			}

			return AK_Success;
		}

		void Release( AK::IAkPluginMemAlloc * in_pAllocator )
		{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
			if ( m_arLfo )
			{
				AK_PLUGIN_FREE( in_pAllocator, m_arLfo );
				m_arLfo = NULL;
			}
#endif
			m_uNumChannels = 0;
		}

		// Call this whenever one of the LFO::Params has changed. Dispatches to all LFO channels.
		void SetParams(	
			AkUInt32			in_uSampleRate,	// Sample rate (Hz)
			const LFO::Params &	in_lfoParams	// Single-channel LFO parameters (applied on all channels).
			)
		{
			// Filter coefficients are the same for all LFO channels. Compute once.
			AkReal32 fB0, fA1;
			MonoLFO<PolarityPolicy, OutputPolicy>::ComputeFilterCoefsFromParams( in_uSampleRate, in_lfoParams, fB0, fA1 );
			
			for ( AkUInt32 iChan=0; iChan<m_uNumChannels; iChan++ )
			{
				m_arLfo[iChan].SetParams( in_uSampleRate, in_lfoParams, fB0, fA1 );
			}
		}

		// Access each LFO channel individually.
		AkForceInline AkUInt32 GetNumChannels() { return m_uNumChannels; }
		AkForceInline MonoLFO<PolarityPolicy, OutputPolicy> & GetChannel( AkUInt32 in_uChannel )
		{
			return m_arLfo[in_uChannel];
		}

	protected:

#ifdef AK_VOICE_MAX_NUM_CHANNELS
		MonoLFO<PolarityPolicy, OutputPolicy>	m_arLfo[AK_VOICE_MAX_NUM_CHANNELS];
#else
		MonoLFO<PolarityPolicy, OutputPolicy> * m_arLfo;
#endif
		AkUInt32				m_uNumChannels;

	} AK_ALIGN_DMA;

} // namespace DSP

#endif // _AKLFO_MULTICHANNEL_H_

