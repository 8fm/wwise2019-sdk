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

// LFO: low frequency sine and sawtooth waveform generator.

#include "LFO.h"
#include <AK/Tools/Common/AkAssert.h>
#include "AkRandom.h"
#include "IAkGlobalPluginContextEx.h"

#include <AK/SoundEngine/Common/IAkPlugin.h>


namespace DSP
{
namespace LFO
{
namespace MultiChannel
{
	// Helper: Compute initial phase for each channel based on the channel mask and phase parameters.
	void ComputeInitialPhase( 
		AK::IAkGlobalPluginContext * in_pGlobalCtx,
		AkChannelConfig in_channelConfig, 
		const PhaseParams & in_phaseParams,
		AkReal32			out_uChannelPhase[]	// Returned array of channel phase offsets. Caller needs to allocate in_uNumChannels*sizeof(AkReal32)
		)
	{
		memset( out_uChannelPhase, 0, in_channelConfig.uNumChannels * sizeof(AkReal32) );

		switch ( in_phaseParams.ePhaseMode )
		{
		case PHASE_MODE_LEFT_RIGHT:
			{
				AkUInt32 uChannelIndexOffset = 0;
				if ( in_channelConfig.uChannelMask & AK_SPEAKER_FRONT_RIGHT )
				{
					// FR channel is always second.
					out_uChannelPhase[1] = in_phaseParams.fPhaseSpread;
					uChannelIndexOffset = 2;

					if ( in_channelConfig.uChannelMask & AK_SPEAKER_FRONT_CENTER )
					{
						// Center channel is always third if there are a left and a right.
						out_uChannelPhase[uChannelIndexOffset] = in_phaseParams.fPhaseSpread / 2.f;
						++uChannelIndexOffset;
					}
				}
				if ( AK::HasSurroundChannels( in_channelConfig.uChannelMask ) )
				{
					out_uChannelPhase[uChannelIndexOffset] = 0;					// Rear left.
					out_uChannelPhase[uChannelIndexOffset+1] = in_phaseParams.fPhaseSpread;	// Rear right.
#ifdef AK_71AUDIO
					if ( AK::HasSideAndRearChannels(in_channelConfig.uChannelMask) )
					{
						out_uChannelPhase[uChannelIndexOffset+2] = 0;					// Side left.
						out_uChannelPhase[uChannelIndexOffset+3] = in_phaseParams.fPhaseSpread;	// Side right.
					}
#endif
				}

			}
			break;

		case PHASE_MODE_FRONT_REAR:
			if ( AK::HasSurroundChannels( in_channelConfig.uChannelMask ) )
			{
				AkUInt32 uRearChannelsOffset = ( in_channelConfig.uChannelMask & AK_SPEAKER_FRONT_CENTER ) ? 3 : 2;
				out_uChannelPhase[uRearChannelsOffset] = in_phaseParams.fPhaseSpread;	// Rear left.
				out_uChannelPhase[uRearChannelsOffset+1] = in_phaseParams.fPhaseSpread;	// Rear right.
#ifdef AK_71AUDIO
				if ( AK::HasSideAndRearChannels(in_channelConfig.uChannelMask) )
				{
					out_uChannelPhase[uRearChannelsOffset+2] = in_phaseParams.fPhaseSpread / 2.f;	// Side left.
					out_uChannelPhase[uRearChannelsOffset+3] = in_phaseParams.fPhaseSpread / 2.f;	// Side right.
				}
#endif
			}
			break;

		case PHASE_MODE_CIRCULAR:
			{
				if ( AK::HasSurroundChannels( in_channelConfig.uChannelMask ) )
				{
					// Front and rear channels.
					// 0		1/4		1/2
					//
					// 1/2				1
					AkUInt32 uChan = 1;

					out_uChannelPhase[uChan] = in_phaseParams.fPhaseSpread / 2.f;	// FR.
					++uChan;										

					if ( in_channelConfig.uChannelMask & AK_SPEAKER_FRONT_CENTER )
					{
						out_uChannelPhase[uChan] = in_phaseParams.fPhaseSpread / 4.f;	// C.
						++uChan;
					}

					out_uChannelPhase[uChan] = in_phaseParams.fPhaseSpread / 2.f;	// RL.
					++uChan;
					out_uChannelPhase[uChan] = in_phaseParams.fPhaseSpread;	// RR.
#ifdef AK_71AUDIO
					if ( AK::HasSideAndRearChannels(in_channelConfig.uChannelMask) )
					{
						// 6 and 7 fullband channels. This is an extension of 4/5 channels because
						// the other ones keep the same value, like so:
						// 0		1/4		1/2
						// 1/4				3/4
						// 1/2				1
						++uChan;
						out_uChannelPhase[uChan] = in_phaseParams.fPhaseSpread / 4.f;	// SL.
						++uChan;
						out_uChannelPhase[uChan] = 3.f * in_phaseParams.fPhaseSpread / 4.f;	// SR.
					}
#endif
				}
				else if ( in_channelConfig.uChannelMask & AK_SPEAKER_SETUP_2_0 )
				{
					// Front channels only
					out_uChannelPhase[1] = in_phaseParams.fPhaseSpread;	// R.

					if ( in_channelConfig.uChannelMask & AK_SPEAKER_FRONT_CENTER )
					{
						out_uChannelPhase[2] = in_phaseParams.fPhaseSpread / 2.f;	// C.
					}
				}
			}
			break;

		case PHASE_MODE_RANDOM:
			{
				AkChannelConfig configNoLFE = in_channelConfig.RemoveLFE();
				for ( AkUInt32 uChan=1; uChan<configNoLFE.uNumChannels; uChan++ )
				{
					AkReal32 fRandom = static_cast<AK::IAkGlobalPluginContextEx *>(in_pGlobalCtx)->Random() / static_cast<AkReal32>(AKRANDOM::AK_RANDOM_MAX);
					out_uChannelPhase[uChan] = fRandom * in_phaseParams.fPhaseSpread;
				}
			}
			break;

		default:
			AKASSERT( !"Invalid phase mode" );
		}

		// Increment all values with global phase offset.
		for ( AkUInt32 uChan=0; uChan<in_channelConfig.uNumChannels; uChan++ )
		{
			out_uChannelPhase[uChan] += in_phaseParams.fPhaseOffset;
		}
	}

}
}
}
