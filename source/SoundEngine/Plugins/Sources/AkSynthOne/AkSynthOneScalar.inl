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


//-----------------------------------------------------------------------------
// Class is used simply to allow generation of AkForceInline code with easy access
// to CAkSynthOneDsp members.
class CAkSynthOneScalar
{
public:
	template< typename FmPolicy, typename PwmPolicy >
	static AkForceInline AkReal32 ScalarProcOsc( CAkSynthOneDsp& io_synth, AkReal32 in_fStartFreq, AkSynthOneOscProc& io_oscProc, AkReal32 in_fFmSample );
	static AkForceInline AkReal32 CombineWithFilter( CAkSynthOneDsp& io_synth, AkReal32 in_fOsc1Sample, AkReal32 in_fOsc2Sample );
	static AkForceInline AkReal32 CombineNoFilter( CAkSynthOneDsp& io_synth, AkReal32 in_fOsc1Sample, AkReal32 in_fOsc2Sample );

} AK_ALIGN_DMA;


//-----------------------------------------------------------------------------
// Helper functions
//-----------------------------------------------------------------------------

// Linear congruential method for generating pseudo-random number (float version)
AkForceInline AkReal32 PseudoRandomNumber( AkUInt32& io_uSeed )
{
	// Generate a (pseudo) random number (32 bits range)
	io_uSeed = (io_uSeed * 196314165) + 907633515;
	// Generate rectangular PDF
	AkInt32 iRandVal = (AkInt32)io_uSeed;
	// Scale to floating point range
	AkReal32 fRandVal = iRandVal * ONEOVERMAXRANDVAL;
	AKASSERT( fRandVal >= -1.f && fRandVal <= 1.f );
	// Output
	return fRandVal;
}

// Interpolates between |0.0f       and |1.0f       at position in_fX
//                      |in_fLowerY     |in_fUpperY
AkForceInline AkReal32 NormalizedInterpolate( AkReal32 in_fLowerY, AkReal32 in_fUpperY, AkReal32 in_fX )
{
	AkReal32 fInterpolated;
	fInterpolated = in_fLowerY + ( in_fX * (in_fUpperY - in_fLowerY) );
	return fInterpolated;
}


//-----------------------------------------------------------------------------
// Oscillator functions
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
struct FmPolicyWith
{
	static AkForceInline AkReal32 GetPhaseAfterFm( ValueRamp& io_fmRamp, AkReal32 in_fFreq, AkReal32 in_fPhase, AkReal32 in_fInvSampleRate, AkReal32 in_fSample )
	{
		AkReal32 fRegPhaseInc = in_fFreq * in_fInvSampleRate;
		AkReal32 fMaxPhaseInc = 20000.f * in_fInvSampleRate;
		AkReal32 fFmPercent = io_fmRamp.Step() * 0.2f;
		AkReal32 fPhaseInc = in_fSample * fFmPercent * (fMaxPhaseInc - fRegPhaseInc);

		in_fPhase += fPhaseInc;
		in_fPhase -= static_cast<AkInt32>( in_fPhase );

		in_fPhase += 1.f; // handle negative phase
		in_fPhase -= static_cast<AkInt32>( in_fPhase );

		AKASSERT( in_fPhase >= 0.f && in_fPhase < 1.f );

		return in_fPhase;
	}
};

struct FmPolicyNone
{
	static AkForceInline AkReal32 GetPhaseAfterFm( ValueRamp& io_fmRamp, AkReal32 in_fFreq, AkReal32 in_fPhase, AkReal32 in_fInvSampleRate, AkReal32 in_fSample )
	{
		return in_fPhase;
	}
};

//-----------------------------------------------------------------------------------------
struct PwmPolicyWith
{
	static AkForceInline AkReal32 GetPhaseAfterPwm( ValueRamp& io_pwmRamp, AkReal32 in_fPhase )
	{
		AKASSERT( in_fPhase >= 0.f && in_fPhase < 1.f );

		AkReal32 fPwmNow = io_pwmRamp.Step() * 0.01f;
		AKASSERT( fPwmNow >= 0.f && fPwmNow <= 1.f );

		AkReal32 fNewPhase;

		if( in_fPhase < fPwmNow )
			fNewPhase = (in_fPhase / fPwmNow) * 0.5f;
		else
			fNewPhase = ( ( (in_fPhase - fPwmNow) / (1 - fPwmNow) ) * 0.5f ) + 0.5f;

		fNewPhase -= static_cast<AkInt32>( fNewPhase );
		AKASSERT( fNewPhase >= 0.f && fNewPhase < 1.f );
		return fNewPhase;
	}
};

struct PwmPolicyNone
{
	static AkForceInline AkReal32 GetPhaseAfterPwm( ValueRamp& io_pwmRamp, AkReal32 in_fPhase )
	{
		return in_fPhase;
	}
};

//-----------------------------------------------------------------------------------------
template< typename FmPolicy, typename PwmPolicy >
AkForceInline AkReal32 CAkSynthOneScalar::ScalarProcOsc( CAkSynthOneDsp& io_synth, AkReal32 in_fStartFreq, AkSynthOneOscProc& io_oscProc, AkReal32 in_fFmSample )
{
	// Get instant frequency
	AkReal32 fInstantFreq = in_fStartFreq * io_oscProc.transRamp.Step();

	// Get current phase
	AkReal32 fProcPhase = io_oscProc.fPhase;

	// Modify phase according to FM
	fProcPhase = FmPolicy::GetPhaseAfterFm( io_synth.m_fmAmountRamp, fInstantFreq, fProcPhase, io_synth.m_fInvOversampledRate, in_fFmSample );

	// Get sampling phase after PWM
	fProcPhase = PwmPolicy::GetPhaseAfterPwm( io_oscProc.pwmRamp, fProcPhase );

	// Get table index, integer and fractional parts
	AKASSERT( fProcPhase >= 0.f && fProcPhase < 1.f );
	AkReal32 fIndex = fProcPhase * OSC_WAVETABLESIZE;
	AkInt32 uIndexInt = static_cast<AkInt32>( fIndex );
	AKASSERT( uIndexInt < OSC_WAVETABLESIZE );
	AkReal32 fIndexFrac = fIndex - uIndexInt;
	AKASSERT( fIndexFrac >= 0.f && fIndexFrac < 1.f );

	// Linear interpolation between 2 samples
	AkUInt32 uLeftSamp = uIndexInt & OSC_WAVETABLESIZEMASK;
	AkUInt32 uRightSamp = (uIndexInt + 1) & OSC_WAVETABLESIZEMASK;

	AkReal32 fLeftY = io_oscProc.pWavTable[uLeftSamp];
	AkReal32 fRightY = io_oscProc.pWavTable[uRightSamp];
	AkReal32 fOutY = NormalizedInterpolate( fLeftY, fRightY, fIndexFrac );

	// Sampling increment relation to oscillator frequency:
	// SI = N * f0
	//			--
	//			fs
	AkReal32 fNewPhase = io_oscProc.fPhase + (fInstantFreq * io_synth.m_fInvOversampledRate);

	// Ensure phase is in range [ 0 <= phase < 1 ]
	fNewPhase -= static_cast<AkInt32>( fNewPhase );
	AKASSERT( fNewPhase >= 0.f && fNewPhase < 1.f );
	io_oscProc.fPhase = fNewPhase;

	// Return sample
	return fOutY;
}

AkForceInline AkReal32 CAkSynthOneScalar::CombineWithFilter( CAkSynthOneDsp& io_synth, AkReal32 in_fOsc1Sample, AkReal32 in_fOsc2Sample )
{
	AkReal32 fOsc1Level = io_synth.m_procOsc1.levelRamp.Step();
	AkReal32 fOsc2Level = io_synth.m_procOsc2.levelRamp.Step();

	AkReal32 fOsc1Sample = in_fOsc1Sample * fOsc1Level;
	AkReal32 fOsc2Sample = in_fOsc2Sample * fOsc2Level;

	AkReal32 fMixSample = io_synth.IsMixingMode()
		? fOsc1Sample + fOsc2Sample
		: fOsc1Sample * fOsc2Sample;

	// Reset filter memory and set coefficients
	// 3 filter sections to avoid aliasing problems before decimation (loop unroll)
	DSP::Memories mem;
	io_synth.m_bqFilters[0].GetMemories(0, mem.fFFwd1, mem.fFFwd2, mem.fFFbk1, mem.fFFbk2);
	fMixSample = io_synth.m_bqFilters[0].ProcessSample(mem, fMixSample);
	io_synth.m_bqFilters[1].GetMemories(0, mem.fFFwd1, mem.fFFwd2, mem.fFFbk1, mem.fFFbk2);
	fMixSample = io_synth.m_bqFilters[1].ProcessSample( mem, fMixSample );
	io_synth.m_bqFilters[2].GetMemories(0, mem.fFFwd1, mem.fFFwd2, mem.fFFbk1, mem.fFFbk2);
	fMixSample = io_synth.m_bqFilters[2].ProcessSample( mem, fMixSample );


	return fMixSample;
}

AkForceInline AkReal32 CAkSynthOneScalar::CombineNoFilter( CAkSynthOneDsp& io_synth, AkReal32 in_fOsc1Sample, AkReal32 in_fOsc2Sample )
{
	AkReal32 fOsc1Level = io_synth.m_procOsc1.levelRamp.Step();
	AkReal32 fOsc2Level = io_synth.m_procOsc2.levelRamp.Step();

	AkReal32 fOsc1Sample = in_fOsc1Sample * fOsc1Level;
	AkReal32 fOsc2Sample = in_fOsc2Sample * fOsc2Level;

	AkReal32 fMixSample = io_synth.IsMixingMode()
		? fOsc1Sample + fOsc2Sample
		: fOsc1Sample * fOsc2Sample;

	return fMixSample;
}

#if defined(AUDIO_SAMPLE_TYPE_SHORT)
#define CONVERT_VALUE( _floatval_ ) ( (_floatval_) * 0x7FFF )
#else
#define CONVERT_VALUE( _floatval_ ) ( _floatval_ )
#endif

//-----------------------------------------------------------------------------
// Name: ProcessDsp
// Desc: Produces buffer samples.
//-----------------------------------------------------------------------------
void CAkSynthOneDsp::ScalarProcDsp( AkUInt32 in_uNumSample, AkSampleType* out_pBuf, AkReal32* in_pScratch )
{
	AkReal32* pScratch = in_pScratch;

	// First generate noise
	if( m_levelRampNoise.GetCurrent() != m_levelRampNoise.GetTarget()
		|| m_levelRampNoise.GetCurrent() > LEVELCUTOFF )
	{
		m_noiseGen.GenerateBuffer( pScratch, in_uNumSample );
	}
	else
	{
		AKPLATFORM::AkMemSet( pScratch, 0, in_uNumSample * sizeof(AkReal32) );
	}
	pScratch = in_pScratch;

	// Determine scaling factors (invert or not)
	AkReal32 fOsc1Scale = m_procOsc1.bInvert ? -1.f : 1.f;
	AkReal32 fOsc2Scale = m_procOsc2.bInvert ? -1.f : 1.f;

	// Main processing loop
	if( IsOverSampling() )
	{
		for( AkUInt32 uNumSample = in_uNumSample; uNumSample != 0; uNumSample-- )
		{
			// Retrieve noise sample
			AkReal32 fNoiseSample = *pScratch++ * m_levelRampNoise.Step();

			// Generate oscillator samples
			AkReal32 fOsc2Sample = CAkSynthOneScalar::ScalarProcOsc<FmPolicyNone,PwmPolicyWith>( *this, m_fBaseFreq, m_procOsc2, 0.f ) * fOsc2Scale;
			AkReal32 fOsc1Sample = CAkSynthOneScalar::ScalarProcOsc<FmPolicyWith,PwmPolicyWith>( *this, m_fBaseFreq, m_procOsc1, fOsc2Sample ) * fOsc1Scale;
			AkReal32 fMixSample = CAkSynthOneScalar::CombineWithFilter( *this, fOsc1Sample, fOsc2Sample );

			fOsc2Sample = CAkSynthOneScalar::ScalarProcOsc<FmPolicyNone,PwmPolicyWith>( *this, m_fBaseFreq, m_procOsc2, 0.f ) * fOsc2Scale;
			fOsc1Sample = CAkSynthOneScalar::ScalarProcOsc<FmPolicyWith,PwmPolicyWith>( *this, m_fBaseFreq, m_procOsc1, fOsc2Sample ) * fOsc1Scale;
			fMixSample = CAkSynthOneScalar::CombineWithFilter( *this, fOsc1Sample, fOsc2Sample );

			fOsc2Sample = CAkSynthOneScalar::ScalarProcOsc<FmPolicyNone,PwmPolicyWith>( *this, m_fBaseFreq, m_procOsc2, 0.f ) * fOsc2Scale;
			fOsc1Sample = CAkSynthOneScalar::ScalarProcOsc<FmPolicyWith,PwmPolicyWith>( *this, m_fBaseFreq, m_procOsc1, fOsc2Sample ) * fOsc1Scale;
			fMixSample = CAkSynthOneScalar::CombineWithFilter( *this, fOsc1Sample, fOsc2Sample );

			fOsc2Sample = CAkSynthOneScalar::ScalarProcOsc<FmPolicyNone,PwmPolicyWith>( *this, m_fBaseFreq, m_procOsc2, 0.f ) * fOsc2Scale;
			fOsc1Sample = CAkSynthOneScalar::ScalarProcOsc<FmPolicyWith,PwmPolicyWith>( *this, m_fBaseFreq, m_procOsc1, fOsc2Sample ) * fOsc1Scale;
			fMixSample = CAkSynthOneScalar::CombineWithFilter( *this, fOsc1Sample, fOsc2Sample );

			// Write to buffer
			fMixSample += fNoiseSample;
			fMixSample *= m_levelRampOutput.Step();
			*out_pBuf++ = CONVERT_VALUE( fMixSample );
		}
	}
	else
	{
		for( AkUInt32 uNumSample = in_uNumSample; uNumSample != 0; uNumSample-- )
		{
			// Retrieve noise sample
			AkReal32 fNoiseSample = *pScratch++ * m_levelRampNoise.Step();

			// Generate oscillator samples
			AkReal32 fOsc2Sample = CAkSynthOneScalar::ScalarProcOsc<FmPolicyNone,PwmPolicyWith>( *this, m_fBaseFreq, m_procOsc2, 0.f ) * fOsc2Scale;
			AkReal32 fOsc1Sample = CAkSynthOneScalar::ScalarProcOsc<FmPolicyWith,PwmPolicyWith>( *this, m_fBaseFreq, m_procOsc1, fOsc2Sample ) * fOsc1Scale;
			AkReal32 fMixSample = CAkSynthOneScalar::CombineNoFilter( *this, fOsc1Sample, fOsc2Sample );

			// Write to buffer
			fMixSample += fNoiseSample;
			fMixSample *= m_levelRampOutput.Step();
			*out_pBuf++ = CONVERT_VALUE( fMixSample );
		}
	}
}
