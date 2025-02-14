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

#ifndef _AKLFO_H_
#define _AKLFO_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>
#include "OnePoleFilter.h"

// ---------------------------------------------------------------------
// LFO Definitions.
// Defines parameters, common structures and helper functions needed 
// by LFO and multichannel LFO modules.
// ---------------------------------------------------------------------

namespace DSP
{
	namespace LFO
	{
		static const AkReal32 TWO_PI = 2.f * 3.14159265358979323846f;
		static const AkReal32 ONE_OVER_TWO_PI = 0.5f * 0.318309886183790671538f;

		// Waveform type.
		enum Waveform
		{
			WAVEFORM_FIRST = 0,

			WAVEFORM_SINE = WAVEFORM_FIRST,
			WAVEFORM_TRIANGLE,
			WAVEFORM_SQUARE,
			WAVEFORM_SAW_UP,
			WAVEFORM_SAW_DOWN,
			WAVEFORM_RND,

			WAVEFORM_NUM
		};

		// (Single-channel) LFO parameters. 
		// They should be kept by the owner of the LFO module.
		// Note: Gain/amplitude is NOT a parameter of an LFO.
		struct Params
		{
			Waveform 	eWaveform;		// Waveform type.
			AkReal32 	fFrequency;		// LFO frequency (Hz).
			AkReal32	fSmooth;		// Waveform smoothing [0,1].
			AkReal32	fPWM;			// Pulse width modulation (applies to square wave only).
		};


		// ---------------------------------------------------------------------
		// LFO State.
		// Defines the state of a single channel oscillator.
		// ---------------------------------------------------------------------
		class State
		{
		public:
			State()
				: fPhase( 0.f )
				, fPhaseDelta( 0.f )
				, eWaveform( WAVEFORM_SINE )
				, uSeed(0)
			{}

			void SetParams(
				AkUInt32			in_uSampleRate,	// Sample rate (Hz)
				const LFO::Params &	in_lfoParams,	// LFO parameters.
				AkReal32			in_fB0,
				AkReal32			in_fA1
				)
			{
				if (in_lfoParams.fFrequency < in_uSampleRate)
				{
					// Compute new phase delta and filter.
					fPhaseDelta = in_lfoParams.fFrequency / in_uSampleRate;
				}
				else
				{
					// We do not support LFO with frequency higher than the sample rate.
					// Since the run-time sample rate is unknown by the authoring, it might not have been limited properly.
					// This is not expected to happen, but if it does, we simply make it a maximum frequency.
					fPhaseDelta = 1.0f;
				}

				// Fix phase delta for some waveform types.
				if (in_lfoParams.eWaveform == LFO::WAVEFORM_SINE)
					fPhaseDelta *= LFO::TWO_PI;

				// Update filter.
				filter.SetCoefs(in_fB0, in_fA1);

				// Update phase if waveform type has changed.
				// Range is [0,2*PI[ for sine, [0,1[ for other waveforms.
				if (eWaveform != in_lfoParams.eWaveform)
				{
					if (eWaveform == LFO::WAVEFORM_SINE)
						fPhase *= LFO::ONE_OVER_TWO_PI;	// From sine. Convert to [0,1[.
					else if (in_lfoParams.eWaveform == LFO::WAVEFORM_SINE)
						fPhase *= LFO::TWO_PI;			// To sine. Convert to [0,2*PI[.
				}
				eWaveform = in_lfoParams.eWaveform;
			}

			DSP::OnePoleFilter	filter;		// Filter for smoothing.
			AkReal32			fPhase;		// Current phase.
			AkReal32			fPhaseDelta;// Phase increment. Depends on LFO frequency (and also wave type).
			Waveform 			eWaveform;	// Waveform type.
			AkUInt64			uSeed;		// Random seed (applies to random wave only).
		};

		// ---------------------------------------------------------------------
		// Multichannel specific definitions.
		// ---------------------------------------------------------------------
		namespace MultiChannel
		{
			// Phase spread mode.
			enum PhaseMode
			{
				PHASE_MODE_LEFT_RIGHT = 0,
				PHASE_MODE_FRONT_REAR,
				PHASE_MODE_CIRCULAR,
				PHASE_MODE_RANDOM
			};

			// Multichannel LFO parameters. 
			// These are only definitions. They should be kept by the owner of the LFO module.
			struct PhaseParams
			{
				AkReal32		fPhaseOffset;	// Phase offset (all channels) (degrees [0,360]).
				AkReal32		fPhaseSpread;	// Phase spread (degrees [0,360]).
				PhaseMode		ePhaseMode;		// Phase spread mode.
			};

			// This structure holds all multichannel LFO parameters.
			// Typically, an effect that has a multichannel LFO section should hold this.
			struct AllParams
			{
				Params			lfoParams;		// LFO parameters (common to all LFO channels).
				PhaseParams		phaseParams;	// Phase parameters.
			};

			// Helper: Compute initial phase for each channel based on the channel mask and phase parameters.
			void ComputeInitialPhase(
				AK::IAkGlobalPluginContext * in_pGlobalCtx,
				AkChannelConfig		in_uChannelConfig, 
				const PhaseParams & in_phaseParams,
				AkReal32			out_uChannelPhase[]	// Returned array of channel phase offsets. Caller needs to allocate in_uNumChannels*sizeof(AkReal32)
				);
		}
	}

} // namespace DSP

#endif // _AKLFO_H_

