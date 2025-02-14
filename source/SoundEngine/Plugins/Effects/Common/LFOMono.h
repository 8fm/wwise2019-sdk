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

#ifndef _AKLFO_MONO_H_
#define _AKLFO_MONO_H_
#include <AK/SoundEngine/Common/AkTypes.h>
#include <math.h>
#include <AK/Tools/Common/AkAssert.h>

#include "AkMath.h"
#include "AkRandom.h"
#include "LFOPolicies.h"

// ---------------------------------------------------------------------
// MonoLFO.
// Single channel LFO.
// Hosts a polarity policy.
//
// Usage:
// - Instantiate a MonoLFO with desired polarity 
//		typedef MonoLFO<Unipolar>	UnipolarLFO;
//		or
//		typedef MonoLFO<Bipolar>	BipolarLFO;
// - Init(), then call SetParams() whenever parameters change.
// - Produce samples: Either call single-sample production methods 
//		explicitly inside a for() loop (see example below), or allocate 
//		a buffer and call ProduceBuffer().
//
/* Example of explicit single-sample production calls:
		LFO::State state;
		lfo.GetState( state );	// Get the LFO's state on the stack.
		while ( pBuf < pEnd )
		{
			*pBuf++ = lfo.ProduceNextSampleSine( state, fGain );
		}
		lfo.PutState( state, LFO::WAVEFORM_SINE );	// Push the resulting state into the LFO for next frame.
*/
// ---------------------------------------------------------------------


namespace DSP
{
	template <class PolarityPolicy, class OutputPolicy>
	class MonoLFO 
	{
	public:

		MonoLFO() {}

		// Initial setup.
		void Setup(
			AkUInt32		in_uSampleRate,		// Sample rate (Hz)
			const LFO::Params &	in_lfoParams,	// LFO parameters.
			AkReal32		in_fInitPhase,		// Initial phase (degrees [-360,360])
			AkUInt64		in_uSeed
			)
		{
			SetParams( in_uSampleRate, in_lfoParams );

			LFO::State state;
			GetState( state );

			AKASSERT( in_fInitPhase >= -360.f && in_fInitPhase <= 360.f );
			
			// Fix initial phase for some waveform types.
			const AkReal32 fMaxPhase = ( in_lfoParams.eWaveform == LFO::WAVEFORM_SINE ) ? LFO::TWO_PI : 1.f;
			state.fPhase = in_fInitPhase * fMaxPhase / 360.f;
			if ( state.eWaveform == LFO::WAVEFORM_TRIANGLE )
				state.fPhase += 0.25f;
			else if (state.eWaveform == LFO::WAVEFORM_SAW_UP)
				state.fPhase += 0.5f;
			else if (state.eWaveform == LFO::WAVEFORM_RND)
			{
				state.uSeed = in_uSeed;
				AKRANDOM::AkRandom(state.uSeed); // improve first sample distribution when initializing from various random sources.
			}

			// Handle wrapping.
			AK_FPSetValLT( state.fPhase, 0.f, state.fPhase, state.fPhase + fMaxPhase );
			AK_FPSetValGTE( state.fPhase, fMaxPhase, state.fPhase, state.fPhase - fMaxPhase );
			
			PutState( state );
		}

		// Call this whenever one of the LFO::Params has changed.
		void SetParams( 
			AkUInt32			in_uSampleRate,	// Sample rate (Hz)
			const LFO::Params &	in_lfoParams	// LFO parameters.
			)
		{
			AkReal32 fB0, fA1;
			ComputeFilterCoefsFromParams( in_uSampleRate, in_lfoParams, fB0, fA1 );
			SetParams( in_uSampleRate, in_lfoParams, fB0, fA1 );
		}

		// ...Or call this if you already know the filter coefficients.
		void SetParams( 
			AkUInt32			in_uSampleRate,	// Sample rate (Hz)
			const LFO::Params &	in_lfoParams,	// LFO parameters.
			AkReal32			in_fB0, 
			AkReal32			in_fA1 
			)
		{
			m_state.SetParams(in_uSampleRate, in_lfoParams, in_fB0, in_fA1);
		}

		// Public helper: Computes filter coefficients from Params. Can be passed to 2nd overload of SetParams().
		static void ComputeFilterCoefsFromParams( 
			AkUInt32			in_uSampleRate,	// Sample rate (Hz)
			const LFO::Params & in_lfoParams,	// LFO parameters.
			AkReal32 &			out_fB0, 
			AkReal32 &			out_fA1 
			)
		{
			if ( in_lfoParams.fSmooth == 0.f )
				OnePoleFilter::ComputeCoefs( DSP::OnePoleFilter::FILTERCURVETYPE_NONE, 0, 0, out_fB0, out_fA1 );
			else
			{
				AKASSERT( in_lfoParams.fSmooth > 0.f && in_lfoParams.fSmooth <= 1.f );

				// Map smoothing to Fc.
				const AkReal64 fMaxSr = in_uSampleRate / 2.0;
				AkReal64 fMinFc = in_lfoParams.fFrequency;
				if (fMinFc > in_uSampleRate)
					fMinFc = in_uSampleRate;

				// Mapping [0,1] follows a negative exponential from Nyquist to the LFO's fundamental frequency.			
				AkReal64 fFc = fMaxSr * exp( -( log( fMaxSr / fMinFc ) ) * in_lfoParams.fSmooth );

				OnePoleFilter::ComputeCoefs( DSP::OnePoleFilter::FILTERCURVETYPE_LOWPASS, fFc, in_uSampleRate, out_fB0, out_fA1 );
			}
		}

		// LFO State access. 
		AkForceInline void GetState( LFO::State & out_state ) const
		{
			out_state = m_state;
		}

		AkForceInline void PutState( const LFO::State & in_state )
		{
			m_state = in_state;

			// Fix phase. Range is [0,2*PI[ for sine, [0,1[ for other waveforms.
			if (in_state.eWaveform == LFO::WAVEFORM_SINE)
				m_state.fPhase = fmodf(in_state.fPhase, LFO::TWO_PI);
			else if (in_state.eWaveform != LFO::WAVEFORM_RND)
				m_state.fPhase = fmodf(in_state.fPhase, 1.f);
		}

		AkForceInline AkInt32 ComputeSteps(AkReal32 in_fRange, AkReal32 in_fDelta, AkUInt32 in_uSamples) 
		{
			AkInt32 iSteps = (AkInt32)ceilf(in_fRange/in_fDelta);
			if (iSteps < 0)
				iSteps = -1;
			else if (iSteps > (AkInt32)in_uSamples)
				iSteps = in_uSamples;
						
			return iSteps;
		}

		// Buffer processing.
		// Produce buffer with amplitude interpolation.
		void ProduceBuffer( AkReal32 * io_pfBuffer, AkUInt32 in_uNumFrames, AkReal32 in_fAmp, AkReal32 in_fPrevAmp, AkReal32 in_fPWM, OutputPolicy &in_rParams)
		{
			AkReal32 * AK_RESTRICT pfBuf = (AkReal32 * ) io_pfBuffer;
			AkReal32 * AK_RESTRICT pfEnd = (AkReal32 * ) io_pfBuffer + in_uNumFrames;

			// Help optimizer by keeping state on the stack.
			LFO::State state;
			GetState( state );

			// Amplitude ramp.
			const AkReal32 fAmpInc = (in_fAmp - in_fPrevAmp ) / in_uNumFrames;
			AkReal32 fCurrAmp = in_fPrevAmp ;
			AkReal32 *pSection = pfBuf;
			AkInt32 iBoundary = 0;

			bool bStatic = state.fPhaseDelta == 0;
			state.fPhaseDelta = AkMax(state.fPhaseDelta, 1.e-8f);	//Ensure that we don't divide by 0, and dont overflow the int when dividing

			switch ( state.eWaveform )
			{
			case LFO::WAVEFORM_SINE:
				//Quick sin is valid for 0 to PI/2.  Process the buffer up to the next quadrant boundary to save on useless checks.
				//Writing this as 4 separate loops is 50% faster than a single loop with wrap around checks.
				while ( pfBuf < pfEnd )
				{
					iBoundary = ComputeSteps(PIOVERTWO - state.fPhase, state.fPhaseDelta, in_uNumFrames);
					pSection = AkMin(pfBuf + iBoundary, pfEnd);
					while ( pfBuf < pSection )
					{
						fCurrAmp += fAmpInc;
						state.fPhase += state.fPhaseDelta;

						AkReal32 fLFO = fCurrAmp * PolarityPolicy::ScaleSin(QuickSin( state.fPhase ));
						in_rParams.OutputFromLFO(pfBuf, fLFO, fCurrAmp);
						pfBuf++;
					}

					iBoundary = ComputeSteps(PI - state.fPhase, state.fPhaseDelta, in_uNumFrames);
					pSection = AkMin(pfBuf + iBoundary, pfEnd);
					while ( pfBuf < pSection )
					{
						fCurrAmp += fAmpInc;
						state.fPhase += state.fPhaseDelta;

						AkReal32 fLFO = fCurrAmp * PolarityPolicy::ScaleSin(QuickSin( PI-state.fPhase ));
						in_rParams.OutputFromLFO(pfBuf, fLFO, fCurrAmp);
						pfBuf++;
					}

					iBoundary = ComputeSteps(PI+PIOVERTWO - state.fPhase, state.fPhaseDelta, in_uNumFrames);
					pSection = AkMin(pfBuf + iBoundary, pfEnd);
					while ( pfBuf < pSection )
					{
						fCurrAmp += fAmpInc;
						state.fPhase += state.fPhaseDelta;

						AkReal32 fLFO = fCurrAmp * PolarityPolicy::ScaleSin(-QuickSin( state.fPhase-PI ));
						in_rParams.OutputFromLFO(pfBuf, fLFO, fCurrAmp);
						pfBuf++;
					}

					iBoundary = ComputeSteps(PI*2 - state.fPhase, state.fPhaseDelta, in_uNumFrames);
					pSection = AkMin(pfBuf + iBoundary, pfEnd);
					while ( pfBuf < pSection )
					{
						fCurrAmp += fAmpInc;
						state.fPhase += state.fPhaseDelta;

						AkReal32 fLFO = fCurrAmp * PolarityPolicy::ScaleSin(-QuickSin( PI*2-state.fPhase ));
						in_rParams.OutputFromLFO(pfBuf, fLFO, fCurrAmp);
						pfBuf++;
					}				

					if (state.fPhase >= PI*2)
						state.fPhase -= PI*2;
				}
				break;
			case LFO::WAVEFORM_TRIANGLE:
				//Split the loop in two, the up part and the down part
				while(pfBuf < pfEnd)
				{
					iBoundary =  ComputeSteps(0.5f - state.fPhase, state.fPhaseDelta, in_uNumFrames);
					pSection = AkMin(pfBuf + iBoundary, pfEnd);
					while ( pfBuf < pSection )
					{
						fCurrAmp += fAmpInc;
						state.fPhase += state.fPhaseDelta;
						AkReal32 fLFO = state.filter.ProcessSample(PolarityPolicy::ScaleTriangle(state.fPhase)) * fCurrAmp;
						in_rParams.OutputFromLFO(pfBuf, fLFO, fCurrAmp);
						pfBuf++;
					}
					iBoundary =  ComputeSteps(1.f - state.fPhase, state.fPhaseDelta, in_uNumFrames);
					pSection = AkMin(pfBuf + iBoundary, pfEnd);
					while ( pfBuf < pSection  )
					{
						fCurrAmp += fAmpInc;
						state.fPhase += state.fPhaseDelta;
						AkReal32 fLFO = state.filter.ProcessSample(PolarityPolicy::ScaleTriangle(1.f - state.fPhase)) * fCurrAmp;
						in_rParams.OutputFromLFO(pfBuf, fLFO, fCurrAmp);
						pfBuf++;
					}
					AK_LFO_REM( state.fPhase );	//fPhase -= floor(fPhase);
				}

				break;
			case LFO::WAVEFORM_SQUARE:
				while(pfBuf < pfEnd)
				{
					iBoundary =  ComputeSteps(in_fPWM - state.fPhase, state.fPhaseDelta, in_uNumFrames);
					pSection = AkMin(pfBuf + iBoundary, pfEnd);
					while ( pfBuf < pSection )
					{
						fCurrAmp += fAmpInc;
						state.fPhase += state.fPhaseDelta;
						AkReal32 fLFO = state.filter.ProcessSample( fCurrAmp );
						in_rParams.OutputFromLFO(pfBuf, fLFO, fCurrAmp);
						pfBuf++;
						
					}

					iBoundary =  ComputeSteps(1.0f - state.fPhase, state.fPhaseDelta, in_uNumFrames);
					pSection = AkMin(pfBuf + iBoundary, pfEnd);
					while ( pfBuf < pSection )
					{
						fCurrAmp += fAmpInc;
						state.fPhase += state.fPhaseDelta;
						AkReal32 fLFO = state.filter.ProcessSample( PolarityPolicy::SquareAmp(fCurrAmp) );
						in_rParams.OutputFromLFO(pfBuf, fLFO, fCurrAmp);
						pfBuf++;
					}

					AK_LFO_REM( state.fPhase );	//fPhase -= floor(fPhase);
				}
				break;
			case LFO::WAVEFORM_SAW_UP:
				while ( pfBuf < pfEnd )
				{
					iBoundary =  ComputeSteps(1.0f - state.fPhase, state.fPhaseDelta, in_uNumFrames);
					pSection = AkMin(pfBuf + iBoundary, pfEnd);
					while ( pfBuf < pSection )
					{
						fCurrAmp += fAmpInc;
						state.fPhase += state.fPhaseDelta;

						AkReal32 fLFO = state.filter.ProcessSample( PolarityPolicy::ScaleSawUp(state.fPhase) * fCurrAmp );
						in_rParams.OutputFromLFO(pfBuf, fLFO, fCurrAmp);

						pfBuf++;
					}
					AK_LFO_REM( state.fPhase );	//fPhase -= floor(fPhase);
				}
				break;
			case LFO::WAVEFORM_SAW_DOWN:
				while ( pfBuf < pfEnd )
				{
					iBoundary =  ComputeSteps(1.0f - state.fPhase, state.fPhaseDelta, in_uNumFrames);
					pSection = AkMin(pfBuf + iBoundary, pfEnd);
					while ( pfBuf < pSection )
					{
						fCurrAmp += fAmpInc;
						state.fPhase += state.fPhaseDelta;

						AkReal32 fLFO = state.filter.ProcessSample( PolarityPolicy::ScaleSawDown(state.fPhase) * fCurrAmp ); 
						in_rParams.OutputFromLFO(pfBuf, fLFO, fCurrAmp);
						pfBuf++;
						
					}
					AK_LFO_REM( state.fPhase );	//fPhase -= floor(fPhase);
				}
				break;

			case LFO::WAVEFORM_RND:
				while (pfBuf < pfEnd)
				{
					iBoundary = ComputeSteps(1.0f - state.fPhase, state.fPhaseDelta, in_uNumFrames);
					pSection = AkMin(pfBuf + iBoundary, pfEnd);
					while (pfBuf < pSection)
					{
						fCurrAmp += fAmpInc;
						state.fPhase += state.fPhaseDelta;

						AkReal32 fLFO = state.filter.ProcessSample((AkReal32)AKRANDOM::SeedToOutputValue(state.uSeed) / AKRANDOM::AK_RANDOM_MAX * fCurrAmp);
						in_rParams.OutputFromLFO(pfBuf, fLFO, fCurrAmp);
						pfBuf++;
					}
					if (state.fPhase >= 1.f)
					{
						state.fPhase -= 1.f;
						AKRANDOM::AkRandom(state.uSeed);
					}
				}
				break;


			default:
				break;
			}

			if (!bStatic)
				PutState( state );
		}

		void SkipFrames( AkUInt32 in_uNumFrames )
		{
			LFO::State state;
			GetState( state );

			state.fPhase += state.fPhaseDelta * in_uNumFrames;

			PutState(state);
		}
		
		// Skip frames and wrap phase within bounds. Use this with "large" skips.
		void SkipFramesAndWrap(AkUInt32 in_uNumFrames)
		{
			LFO::State state;
			GetState(state);

			state.fPhase += state.fPhaseDelta * in_uNumFrames;

			// Wrap phase.
			// Deal with special sin phase interval (REVIEW!)
			if (state.eWaveform == DSP::LFO::WAVEFORM_SINE)
			{
				state.fPhase *= DSP::LFO::ONE_OVER_TWO_PI;
				state.fPhase -= floorf(state.fPhase);
				state.fPhase *= DSP::LFO::TWO_PI;
			}
			else if (state.eWaveform == DSP::LFO::WAVEFORM_RND)
			{
				if (state.fPhase >= 1.f)
				{
					state.fPhase -= floorf(state.fPhase);
					AKRANDOM::AkRandom(state.uSeed);
				}				
			}
			else
			{
				state.fPhase -= floorf(state.fPhase);
			}

			PutState(state);
		}

	protected:
		LFO::State m_state;		// LFO state.		
	};
}

#endif // _AKLFO_MONO_H_

