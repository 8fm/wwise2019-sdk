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
#define SYNTHONE_ASSERT_FRAC( __vFrac__ ) \
{ \
	AKASSERT( AKSIMD_GETELEMENT_V4F32(__vFrac__, 0) >= 0.f && AKSIMD_GETELEMENT_V4F32(__vFrac__, 0) < 1.f ); \
	AKASSERT( AKSIMD_GETELEMENT_V4F32(__vFrac__, 1) >= 0.f && AKSIMD_GETELEMENT_V4F32(__vFrac__, 1) < 1.f ); \
	AKASSERT( AKSIMD_GETELEMENT_V4F32(__vFrac__, 2) >= 0.f && AKSIMD_GETELEMENT_V4F32(__vFrac__, 2) < 1.f ); \
	AKASSERT( AKSIMD_GETELEMENT_V4F32(__vFrac__, 3) >= 0.f && AKSIMD_GETELEMENT_V4F32(__vFrac__, 3) < 1.f ); \
}

#define SYNTHONE_ASSERT_PHASE( __vPhase__ ) SYNTHONE_ASSERT_FRAC( __vPhase__ )

#define SYNTHONE_ASSERT_IDX( __vIdx__ ) \
{ \
	AKASSERT( AKSIMD_GETELEMENT_V4I32(__vIdx__, 0) >= 0 && AKSIMD_GETELEMENT_V4I32(__vIdx__, 0) < OSC_WAVETABLESIZE ); \
	AKASSERT( AKSIMD_GETELEMENT_V4I32(__vIdx__, 1) >= 0 && AKSIMD_GETELEMENT_V4I32(__vIdx__, 1) < OSC_WAVETABLESIZE ); \
	AKASSERT( AKSIMD_GETELEMENT_V4I32(__vIdx__, 2) >= 0 && AKSIMD_GETELEMENT_V4I32(__vIdx__, 2) < OSC_WAVETABLESIZE ); \
	AKASSERT( AKSIMD_GETELEMENT_V4I32(__vIdx__, 3) >= 0 && AKSIMD_GETELEMENT_V4I32(__vIdx__, 3) < OSC_WAVETABLESIZE ); \
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
AkForceInline void PrepFreqPhaseVars( const AkSynthOneOscProc& io_oscProc,
									 const AkReal32 in_fFreq,
									 const AkReal32 in_fInvOversampledRate,
									AKSIMD_V4F32& io_vFreq, AKSIMD_V4F32& io_vFreqInc,
									AKSIMD_V4F32& io_vPhase, AKSIMD_V4F32& io_vPhaseUpM, AKSIMD_V4F32& io_vPhaseUpB )
{
	AkReal32 fFreq = in_fFreq * io_oscProc.transRamp.GetCurrent();
	AkReal32 fFreqInc = in_fFreq * io_oscProc.transRamp.GetIncrement();

	fFreq += fFreqInc; AKSIMD_SETELEMENT_V4F32( io_vFreq, 0, fFreq );
	fFreq += fFreqInc; AKSIMD_SETELEMENT_V4F32( io_vFreq, 1, fFreq );
	fFreq += fFreqInc; AKSIMD_SETELEMENT_V4F32( io_vFreq, 2, fFreq );
	fFreq += fFreqInc; AKSIMD_SETELEMENT_V4F32( io_vFreq, 3, fFreq );

	io_vFreqInc = AKSIMD_SET_V4F32( 4 * fFreqInc );

	AkReal32 fPhase = io_oscProc.fPhase;
	AKSIMD_SETELEMENT_V4F32( io_vPhase, 0, fPhase );
	fFreq = AKSIMD_GETELEMENT_V4F32( io_vFreq, 0 );

	fPhase += fFreq * in_fInvOversampledRate;
	AKSIMD_SETELEMENT_V4F32( io_vPhase, 1, fPhase );
	fFreq = AKSIMD_GETELEMENT_V4F32( io_vFreq, 1 );

	fPhase += fFreq * in_fInvOversampledRate;
	AKSIMD_SETELEMENT_V4F32( io_vPhase, 2, fPhase );
	fFreq = AKSIMD_GETELEMENT_V4F32( io_vFreq, 2 );

	fPhase += fFreq * in_fInvOversampledRate;
	AKSIMD_SETELEMENT_V4F32( io_vPhase, 3, fPhase );

	// Make sure initial phase is within range ( 0 <= phase < 1 )
	AKSIMD_V4I32 iPhaseTrunc = AKSIMD_TRUNCATE_V4F32_TO_V4I32( io_vPhase );
	AKSIMD_V4F32 fPhaseTrunc = AKSIMD_CONVERT_V4I32_TO_V4F32( iPhaseTrunc );
	io_vPhase = AKSIMD_SUB_V4F32( io_vPhase, fPhaseTrunc );

	// Setup constants used for phase calculations (y = m*x + b)
	io_vPhaseUpM = AKSIMD_SET_V4F32( 4.f * in_fInvOversampledRate );
	io_vPhaseUpB = AKSIMD_SET_V4F32( 6.f * in_fInvOversampledRate * fFreqInc );
}

static const AKSIMD_DECLARE_V4F32( cvOne, 1.f, 1.f, 1.f, 1.f );

AkForceInline void NextFrameFreqPhase( const AKSIMD_V4F32& in_vInvOverRate,
									  AKSIMD_V4F32& io_vPhase, const AKSIMD_V4F32& in_vPhaseUpM, const AKSIMD_V4F32& in_vPhaseUpB,
									  AKSIMD_V4F32& io_vFreq, const AKSIMD_V4F32& in_vFreqInc )
{
	AKSIMD_V4F32 vPhaseMul = AKSIMD_MUL_V4F32( io_vFreq, in_vPhaseUpM );
	io_vFreq = AKSIMD_ADD_V4F32( io_vFreq, in_vFreqInc );
	io_vPhase = AKSIMD_ADD_V4F32( io_vPhase, in_vPhaseUpB );
	io_vPhase = AKSIMD_ADD_V4F32( io_vPhase, vPhaseMul );

	// Make sure calculated phase is within range ( 0 <= phase < 1 )
	AKSIMD_V4I32 iPhaseTrunc = AKSIMD_TRUNCATE_V4F32_TO_V4I32( io_vPhase );
	AKSIMD_V4F32 fPhaseTrunc = AKSIMD_CONVERT_V4I32_TO_V4F32( iPhaseTrunc );
	io_vPhase = AKSIMD_SUB_V4F32( io_vPhase, fPhaseTrunc );
}

//-----------------------------------------------------------------------------
AkForceInline void PrepFmVars( const ValueRamp& in_fmRamp, AKSIMD_V4F32& io_vFm, AKSIMD_V4F32& io_vFmInc )
{
	AkReal32 fFm = in_fmRamp.GetCurrent();
	const AkReal32 fFmInc = in_fmRamp.GetIncrement();

	fFm += fFmInc; AKSIMD_SETELEMENT_V4F32( io_vFm, 0, fFm );
	fFm += fFmInc; AKSIMD_SETELEMENT_V4F32( io_vFm, 1, fFm );
	fFm += fFmInc; AKSIMD_SETELEMENT_V4F32( io_vFm, 2, fFm );
	fFm += fFmInc; AKSIMD_SETELEMENT_V4F32( io_vFm, 3, fFm );

	io_vFmInc = AKSIMD_SET_V4F32( 4 * fFmInc );
}

//-----------------------------------------------------------------------------

static const AKSIMD_DECLARE_V4F32( cvPtTwo, 0.2f, 0.2f, 0.2f, 0.2f );
static const AKSIMD_DECLARE_V4F32( cvZero, 0.f, 0.f, 0.f, 0.f );

struct FmPolicyWith
{
	static AkForceInline void GetFmPhase( const AKSIMD_V4F32& in_vInvOverRate, const AKSIMD_V4F32& in_FmConst, const AKSIMD_V4F32& in_vFreq, AKSIMD_V4F32& io_vPhase, AKSIMD_V4F32& io_vFmMul, AKSIMD_V4F32& in_vFmInc, const AkReal32* in_pFm, const AkUInt32 in_idxFm )
	{
		AKSIMD_V4F32 vRegIncr = AKSIMD_MUL_V4F32( in_vFreq, in_vInvOverRate );

		AKSIMD_V4F32 vFmTemp = AKSIMD_MUL_V4F32( io_vFmMul, cvPtTwo );
		vRegIncr = AKSIMD_SUB_V4F32( in_FmConst, vRegIncr );

		io_vFmMul = AKSIMD_ADD_V4F32( io_vFmMul, in_vFmInc );

		AKSIMD_V4F32 vSample = AKSIMD_LOAD_V4F32( in_pFm + in_idxFm );

		vFmTemp = AKSIMD_MUL_V4F32( vSample, vFmTemp );
		vFmTemp = AKSIMD_MUL_V4F32( vFmTemp, vRegIncr );

		io_vPhase = AKSIMD_ADD_V4F32( io_vPhase, vFmTemp );

		// Make sure phase is in proper range ( 0 <= phase < 1 )
		AKSIMD_V4I32 iPhase = AKSIMD_TRUNCATE_V4F32_TO_V4I32( io_vPhase );
		AKSIMD_V4F32 fPhase = AKSIMD_CONVERT_V4I32_TO_V4F32( iPhase );
		io_vPhase = AKSIMD_SUB_V4F32( io_vPhase, fPhase );

		// Handle negative phase
		AKSIMD_V4F32 vPlusOne = AKSIMD_ADD_V4F32( io_vPhase, cvOne );
		AKSIMD_V4COND vPlusCond = AKSIMD_GTEQ_V4F32( io_vPhase, cvZero );
		io_vPhase = AKSIMD_VSEL_V4F32( vPlusOne, io_vPhase, vPlusCond );
	
		AKSIMD_V4F32 vMinusOne = AKSIMD_SUB_V4F32( io_vPhase, cvOne );
		AKSIMD_V4COND vMinusCond = AKSIMD_GTEQ_V4F32( vMinusOne, cvZero );
		io_vPhase = AKSIMD_VSEL_V4F32( io_vPhase, vMinusOne, vMinusCond );

		SYNTHONE_ASSERT_PHASE( io_vPhase );
	}
};

struct FmPolicyNone
{
	static AkForceInline void GetFmPhase( const AKSIMD_V4F32& in_vInvOverRate, const AKSIMD_V4F32& in_FmConst, const AKSIMD_V4F32& in_vFreq, AKSIMD_V4F32& io_vPhase, AKSIMD_V4F32& io_vFmMul, AKSIMD_V4F32& in_vFmInc, const AkReal32* in_pFm, const AkUInt32 in_idxFm )
	{
	}
};

//-----------------------------------------------------------------------------
AkForceInline void PrepPwmVars( const AkSynthOneOscProc& io_oscProc, AKSIMD_V4F32& io_vPwm, AKSIMD_V4F32& io_vPwmInc )
{
	AkReal32 fPwm = io_oscProc.pwmRamp.GetCurrent() * 0.01f;
	AkReal32 fPwmInc = io_oscProc.pwmRamp.GetIncrement() * 0.01f;

	fPwm += fPwmInc; AKSIMD_SETELEMENT_V4F32( io_vPwm, 0, fPwm );
	fPwm += fPwmInc; AKSIMD_SETELEMENT_V4F32( io_vPwm, 1, fPwm );
	fPwm += fPwmInc; AKSIMD_SETELEMENT_V4F32( io_vPwm, 2, fPwm );
	fPwm += fPwmInc; AKSIMD_SETELEMENT_V4F32( io_vPwm, 3, fPwm );

	io_vPwmInc = AKSIMD_SET_V4F32( 4 * fPwmInc );
}

//-----------------------------------------------------------------------------
static const AKSIMD_V4F32 cvPtFive = AKSIMD_SET_V4F32( 0.5f );

struct PwmPolicyWith
{
	static AkForceInline void GetPwmPhase( AKSIMD_V4F32& io_vPhase, AKSIMD_V4F32& io_vPwm, AKSIMD_V4F32& in_vPwmInc )
	{
		// Code equivalent; will select numerator and denominator, then perform division
		//
		//if( in_fPhase < fPwmNow )
		//	fNewPhase = (in_fPhase / fPwmNow) * 0.5f;
		//else
		//	fNewPhase = ( ( (in_fPhase - fPwmNow) / (1 - fPwmNow) ) * 0.5f ) + 0.5f;

		AKSIMD_V4COND vCond = AKSIMD_GTEQ_V4F32( io_vPhase, io_vPwm );

		AKSIMD_V4F32 vNumer = AKSIMD_SUB_V4F32( io_vPhase, io_vPwm );
		AKSIMD_V4F32 vDenom = AKSIMD_SUB_V4F32( cvOne, io_vPwm );

		AKSIMD_V4F32 vAdd = AKSIMD_SET_V4F32( 0.f );

		vNumer = AKSIMD_VSEL_V4F32( io_vPhase, vNumer, vCond );
		vDenom = AKSIMD_VSEL_V4F32( io_vPwm, vDenom, vCond );
		vAdd = AKSIMD_VSEL_V4F32( vAdd, cvPtFive, vCond );

		io_vPwm = AKSIMD_ADD_V4F32( io_vPwm, in_vPwmInc );

		vNumer = AKSIMD_MUL_V4F32( vNumer, cvPtFive );
		AKSIMD_V4F32 vTemp = AKSIMD_DIV_V4F32( vNumer, vDenom );
		vTemp = AKSIMD_ADD_V4F32( vTemp, vAdd );

		AKSIMD_V4COND vPhaseCond = AKSIMD_GTEQ_V4F32( vTemp, cvOne );
		io_vPhase = AKSIMD_VSEL_V4F32( vTemp, cvZero, vPhaseCond );
	}
};

struct PwmPolicyNone
{
	static AkForceInline void GetPwmPhase( AKSIMD_V4F32& io_vPhase, AKSIMD_V4F32& io_vPwm, AKSIMD_V4F32& in_vPwmInc )
	{
	}
};

static const AKSIMD_V4F32 vSize = AKSIMD_SET_V4F32( (AkReal32)OSC_WAVETABLESIZE );
static const AKSIMD_V4I32 ivMask = AKSIMD_SET_V4I32( OSC_WAVETABLESIZEMASK );
static const AKSIMD_V4I32 ivOne = AKSIMD_SET_V4I32( 1 );

//-----------------------------------------------------------------------------
AkForceInline void GetSampleIndex( AKSIMD_V4F32& in_vPhase, AKSIMD_V4I32& out_vLeft, AKSIMD_V4I32& out_vRight, AKSIMD_V4F32& out_vFrac )
{
	AKSIMD_V4F32 vPhase = AKSIMD_MUL_V4F32( in_vPhase, vSize );

	out_vLeft = AKSIMD_TRUNCATE_V4F32_TO_V4I32( vPhase );
	out_vRight = AKSIMD_ADD_V4I32( out_vLeft, ivOne );
	out_vRight = AKSIMD_AND_V4I32( out_vRight, ivMask );

	SYNTHONE_ASSERT_IDX( out_vLeft );
	SYNTHONE_ASSERT_IDX( out_vRight );

	out_vFrac = AKSIMD_CONVERT_V4I32_TO_V4F32( out_vLeft );
	out_vFrac = AKSIMD_SUB_V4F32( vPhase, out_vFrac );

	SYNTHONE_ASSERT_FRAC( out_vFrac );
}

//-----------------------------------------------------------------------------
AkNoInline void LookupSamples( const AkReal32* const in_pWav, AkUInt32 in_uNumSample, AkInt32* AK_RESTRICT io_pLeft, AkInt32* AK_RESTRICT io_pRight )
{
	AkInt32* piLeft = io_pLeft;
	AkInt32* piRight = io_pRight;

	for( AkUInt32 i = 0; i < in_uNumSample; i += 8 )
	{
		AkReal32* pfLeft = (AkReal32*)piLeft;
		AkReal32* pfRight = (AkReal32*)piRight;

		// Lookup samples
		pfLeft[0] = in_pWav[ piLeft[0] ];
		pfLeft[1] = in_pWav[ piLeft[1] ];
		pfLeft[2] = in_pWav[ piLeft[2] ];
		pfLeft[3] = in_pWav[ piLeft[3] ];
		pfLeft[4] = in_pWav[ piLeft[4] ];
		pfLeft[5] = in_pWav[ piLeft[5] ];
		pfLeft[6] = in_pWav[ piLeft[6] ];
		pfLeft[7] = in_pWav[ piLeft[7] ];

		pfRight[0] = in_pWav[ piRight[0] ];
		pfRight[1] = in_pWav[ piRight[1] ];
		pfRight[2] = in_pWav[ piRight[2] ];
		pfRight[3] = in_pWav[ piRight[3] ];
		pfRight[4] = in_pWav[ piRight[4] ];
		pfRight[5] = in_pWav[ piRight[5] ];
		pfRight[6] = in_pWav[ piRight[6] ];
		pfRight[7] = in_pWav[ piRight[7] ];

		// Adjust pointers
		piLeft += 8;
		piRight += 8;
	}
}

//-----------------------------------------------------------------------------
AkNoInline void InterpolateSamples( const AkUInt32 in_uNumSample, const bool in_bInvert, AkReal32* AK_RESTRICT in_pLeft, AkReal32* AK_RESTRICT in_pRight, AkReal32* AK_RESTRICT in_pFrac, AkReal32* AK_RESTRICT out_pIntr )
{
	AKSIMD_V4F32 vInvMul = AKSIMD_SET_V4F32( 1.f );
	if( in_bInvert )
		vInvMul = AKSIMD_SET_V4F32( -1.f );

	for( AkUInt32 i = 0; i < in_uNumSample; i += 8 )
	{
		AKSIMD_V4F32 vLeft_0 = AKSIMD_LOAD_V4F32( in_pLeft + i );
		AKSIMD_V4F32 vRight_0 = AKSIMD_LOAD_V4F32( in_pRight + i );
		AKSIMD_V4F32 vFrac_0 = AKSIMD_LOAD_V4F32( in_pFrac + i );

		AkUInt32 j = i + 4;
		AKSIMD_V4F32 vLeft_1 = AKSIMD_LOAD_V4F32( in_pLeft + j );
		AKSIMD_V4F32 vRight_1 = AKSIMD_LOAD_V4F32( in_pRight + j );
		AKSIMD_V4F32 vFrac_1 = AKSIMD_LOAD_V4F32( in_pFrac + j );

		// intr = left + ( frac * (right - left) );

		AKSIMD_V4F32 vIntr_0, vIntr_1;

		vIntr_0 = AKSIMD_SUB_V4F32( vRight_0, vLeft_0 );
		vIntr_1 = AKSIMD_SUB_V4F32( vRight_1, vLeft_1 );

		vIntr_0 = AKSIMD_MUL_V4F32( vIntr_0, vFrac_0 );
		vIntr_1 = AKSIMD_MUL_V4F32( vIntr_1, vFrac_1 );

		vIntr_0 = AKSIMD_ADD_V4F32( vIntr_0, vLeft_0 );
		vIntr_1 = AKSIMD_ADD_V4F32( vIntr_1, vLeft_1 );

		vIntr_0 = AKSIMD_MUL_V4F32( vIntr_0, vInvMul );
		vIntr_1 = AKSIMD_MUL_V4F32( vIntr_1, vInvMul );

		AKSIMD_STORE_V4F32( out_pIntr + i, vIntr_0 );
		AKSIMD_STORE_V4F32( out_pIntr + j, vIntr_1 );
	}
}

//-----------------------------------------------------------------------------
// Policy for combining samples
struct SampleCbxPolicyMix
{
	static inline AKSIMD_V4F32 Combine( const AKSIMD_V4F32& a, const AKSIMD_V4F32& b )
	{
		return AKSIMD_ADD_V4F32( a, b );
	}
};

struct SampleCbxPolicyRing
{
	static inline AKSIMD_V4F32 Combine( const AKSIMD_V4F32& a, const AKSIMD_V4F32& b )
	{
		return AKSIMD_MUL_V4F32( a, b );
	}
};

//-----------------------------------------------------------------------------
// Combine samples from both oscillators; put results in osc1 buffer.
template < typename SampleCbxPolicy >
void CAkSynthOneDsp::CombineSamples( const AkUInt32 in_uNumSample, AkReal32* AK_RESTRICT in_pOsc1, AkReal32* AK_RESTRICT in_pOsc2, AkReal32* AK_RESTRICT out_pBuf )
{
	AKSIMD_V4F32 vLevelOsc1, vIncrOsc1;
	AKSIMD_V4F32 vLevelOsc2, vIncrOsc2;

	// Prep output level values
	{
		AkReal32 fLevel = m_procOsc1.levelRamp.GetCurrent();
		AkReal32 fIncr = m_procOsc1.levelRamp.GetIncrement();

		vLevelOsc1 = AKSIMD_SET_V4F32( 0.f );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc1, 0, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc1, 1, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc1, 2, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc1, 3, fLevel );
		vIncrOsc1 = AKSIMD_SET_V4F32( 4.0f * fIncr );
	}

	{
		AkReal32 fLevel = m_procOsc2.levelRamp.GetCurrent();
		AkReal32 fIncr = m_procOsc2.levelRamp.GetIncrement();

		vLevelOsc2 = AKSIMD_SET_V4F32( 0.f );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc2, 0, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc2, 1, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc2, 2, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc2, 3, fLevel );
		vIncrOsc2 = AKSIMD_SET_V4F32( 4.0f * fIncr );
	}

	// Apply output levels to each oscillator, then combine samples
	for( AkUInt32 i = 0; i < in_uNumSample; i += 8 )
	{
		AKSIMD_V4F32 vOsc1_0 = AKSIMD_LOAD_V4F32( in_pOsc1 + i );
		AKSIMD_V4F32 vOsc2_0 = AKSIMD_LOAD_V4F32( in_pOsc2 + i );

		AkUInt32 j = i + 4;

		AKSIMD_V4F32 vOsc1_1 = AKSIMD_LOAD_V4F32( in_pOsc1 + j );
		AKSIMD_V4F32 vOsc2_1 = AKSIMD_LOAD_V4F32( in_pOsc2 + j );

		vOsc1_0 = AKSIMD_MUL_V4F32( vOsc1_0, vLevelOsc1 );
		vOsc2_0 = AKSIMD_MUL_V4F32( vOsc2_0, vLevelOsc2 );

		vLevelOsc1 = AKSIMD_ADD_V4F32( vLevelOsc1, vIncrOsc1 );
		vLevelOsc2 = AKSIMD_ADD_V4F32( vLevelOsc2, vIncrOsc2 );

		vOsc1_1 = AKSIMD_MUL_V4F32( vOsc1_1, vLevelOsc1 );
		vOsc2_1 = AKSIMD_MUL_V4F32( vOsc2_1, vLevelOsc2 );

		vLevelOsc1 = AKSIMD_ADD_V4F32( vLevelOsc1, vIncrOsc1 );
		vLevelOsc2 = AKSIMD_ADD_V4F32( vLevelOsc2, vIncrOsc2 );

		AKSIMD_V4F32 vMix_0 = SampleCbxPolicy::Combine( vOsc1_0, vOsc2_0 );
		AKSIMD_V4F32 vMix_1 = SampleCbxPolicy::Combine( vOsc1_1, vOsc2_1 );

		AKSIMD_STORE_V4F32( out_pBuf + i, vMix_0 );
		AKSIMD_STORE_V4F32( out_pBuf + j, vMix_1 );
	}
}

//-----------------------------------------------------------------------------
void DecimateSamples( const AkUInt32 in_uNumSample, AkReal32* AK_RESTRICT in_pIn, AkReal32* AK_RESTRICT out_pOut )
{
	AKASSERT( OSCOVERSAMPLING == 4 );

	AkReal32* pIn = in_pIn;
	AkReal32* pOut = out_pOut;
	
	for( AkUInt32 i = 0; i < in_uNumSample; i += 16 )
	{
		pOut[0] = pIn[3];
		pOut[1] = pIn[7];
		pOut[2] = pIn[11];
		pOut[3] = pIn[15];

		pOut += 4;
		pIn += 16;
	}
}

//-----------------------------------------------------------------------------
// Process oscillator frame
template < typename FmPolicy, typename PwmPolicy >
void GetSampleIndices( const AkUInt32 in_uNumSample, const AkReal32 in_fBaseFreq,
						const AkReal32 in_fInvOverRate, AkSynthOneOscProc& io_oscProc,
						ValueRamp& in_fmRamp, const AkReal32* in_pFm,
						const AkInt32* out_pLeft, const AkInt32* out_pRight, const AkReal32* out_pFrac )
{
	AKSIMD_V4F32 vFreq; AKSIMD_V4F32 vFreqInc;
	AKSIMD_V4F32 vPhase; AKSIMD_V4F32 vPhaseUpM; AKSIMD_V4F32 vPhaseUpB;
	PrepFreqPhaseVars( io_oscProc, in_fBaseFreq, in_fInvOverRate, vFreq, vFreqInc, vPhase, vPhaseUpM, vPhaseUpB );

	AKSIMD_V4F32 vFm; AKSIMD_V4F32 vFmInc;
	PrepFmVars( in_fmRamp, vFm, vFmInc );

	AKSIMD_V4F32 vPwm; AKSIMD_V4F32 vPwmInc;
	PrepPwmVars( io_oscProc, vPwm, vPwmInc );

	// Constant vectors for FM
	const AKSIMD_V4F32 vInvOverRate = AKSIMD_SET_V4F32( in_fInvOverRate );
	const AKSIMD_V4F32 vFmConstant = AKSIMD_SET_V4F32( 20000.f * in_fInvOverRate );

	for( AkUInt32 i = 0; i < in_uNumSample; i += 4 )
	{
		// Copy phase for current frame processing
		AKSIMD_V4F32 vOscPhase = vPhase;

		// Adjust phase according to FM
		FmPolicy::GetFmPhase( vInvOverRate, vFmConstant, vFreq, vOscPhase, vFm, vFmInc, in_pFm, i );

		// Adjust phase according to PWM
		PwmPolicy::GetPwmPhase( vOscPhase, vPwm, vPwmInc );

		// Increment phase and frequency for next frame
		NextFrameFreqPhase( vInvOverRate, vPhase, vPhaseUpM, vPhaseUpB, vFreq, vFreqInc );

		// Get samples indices
		AKSIMD_V4I32 vLeft, vRight;
		AKSIMD_V4F32 vFrac;
		GetSampleIndex( vOscPhase, vLeft, vRight, vFrac );

		// Store the indices in scratch buffer
		AKSIMD_STORE_V4I32( (AKSIMD_V4I32*)(out_pLeft + i), vLeft );
		AKSIMD_STORE_V4I32( (AKSIMD_V4I32*)(out_pRight + i), vRight );
		AKSIMD_STORE_V4F32( (AKSIMD_V4F32*)(out_pFrac + i), vFrac );
	}

	// Store phase for next frame
	io_oscProc.fPhase = AKSIMD_GETELEMENT_V4F32( vPhase, 0 );
}

//-----------------------------------------------------------------------------
// Process oscillator frame
template < typename FmPolicy, typename PwmPolicy >
void CAkSynthOneDsp::SimdProcOscX( const AkUInt32 in_uNumSample, const AkReal32 in_fBaseFreq, AkSynthOneOscProc& io_oscProc, AkReal32* AK_RESTRICT out_pOsc, AkReal32* AK_RESTRICT in_pFm, AkReal32* AK_RESTRICT in_pScratch )
{
	// Use scratch buffer to store sample indices
	AkReal32* pFrac = (AkReal32*)(out_pOsc);
	AkInt32* pLeft = (AkInt32*)(in_pScratch);
	AkInt32* pRight = (AkInt32*)(pLeft + in_uNumSample);

	// Get sample indices
	GetSampleIndices<FmPolicy,PwmPolicy>( in_uNumSample, in_fBaseFreq, m_fInvOversampledRate, io_oscProc, m_fmAmountRamp, in_pFm, pLeft, pRight, pFrac );

	// Perform sample lookup for entire buffer, and store back in scratch buffer
	LookupSamples( io_oscProc.pWavTable, in_uNumSample, pLeft, pRight );

	// Finally, interpolate samples to generate final oscillator buffer
	InterpolateSamples( in_uNumSample, io_oscProc.bInvert, (AkReal32*)pLeft, (AkReal32*)pRight, pFrac, out_pOsc );
}

//-----------------------------------------------------------------------------
void CAkSynthOneDsp::SimdProcOsc( const AkUInt32 in_uNumSample, const AkReal32 in_fBaseFreq, AkSynthOneOscProc& io_oscProc, AkReal32* out_pOsc, AkReal32* in_pFm, AkReal32* in_pScratch )
{
	SimdProcOscX<FmPolicyNone,PwmPolicyNone>( in_uNumSample, in_fBaseFreq, io_oscProc, out_pOsc, in_pFm, in_pScratch );
}

void CAkSynthOneDsp::SimdProcOscFm( const AkUInt32 in_uNumSample, const AkReal32 in_fBaseFreq, AkSynthOneOscProc& io_oscProc, AkReal32* out_pOsc, AkReal32* in_pFm, AkReal32* in_pScratch )
{
	SimdProcOscX<FmPolicyWith,PwmPolicyNone>( in_uNumSample, in_fBaseFreq, io_oscProc, out_pOsc, in_pFm, in_pScratch );
}

void CAkSynthOneDsp::SimdProcOscPwm( const AkUInt32 in_uNumSample, const AkReal32 in_fBaseFreq, AkSynthOneOscProc& io_oscProc, AkReal32* out_pOsc, AkReal32* in_pFm, AkReal32* in_pScratch )
{
	SimdProcOscX<FmPolicyNone,PwmPolicyWith>( in_uNumSample, in_fBaseFreq, io_oscProc, out_pOsc, in_pFm, in_pScratch );
}

void CAkSynthOneDsp::SimdProcOscFmPwm( const AkUInt32 in_uNumSample, const AkReal32 in_fBaseFreq, AkSynthOneOscProc& io_oscProc, AkReal32* out_pOsc, AkReal32* in_pFm, AkReal32* in_pScratch )
{
	SimdProcOscX<FmPolicyWith,PwmPolicyWith>( in_uNumSample, in_fBaseFreq, io_oscProc, out_pOsc, in_pFm, in_pScratch );
}

//-----------------------------------------------------------------------------
CAkSynthOneDsp::FnSimdOscProc CAkSynthOneDsp::GetSimdOscProcFn( const AkSynthOneOscProc& in_oscProc )
{
	bool bProcessFm = in_oscProc.eOsc == AkSynthOneOsc1 &&
						(m_fmAmountRamp.GetCurrent() != 0.f || m_fmAmountRamp.GetTarget() != 0.f);

	bool bProcessPwm = in_oscProc.pwmRamp.GetCurrent() != in_oscProc.pwmRamp.GetTarget() ||
						in_oscProc.pwmRamp.GetTarget() != 50.f;

	if( ! bProcessFm )
	{
		if( ! bProcessPwm )
			return &CAkSynthOneDsp::SimdProcOsc;
		else
			return &CAkSynthOneDsp::SimdProcOscPwm;
	}
	else
	{
		if( ! bProcessPwm )
			return &CAkSynthOneDsp::SimdProcOscFm;
		else
			return &CAkSynthOneDsp::SimdProcOscFmPwm;
	}
}

//-----------------------------------------------------------------------------
// Process oscillator frame
void CAkSynthOneDsp::SimdProcOsc( const AkUInt32 in_uNumSample, AkReal32* AK_RESTRICT out_pBuf, AkReal32* AK_RESTRICT in_pScratch )
{
	// Get local copy of number of samples
	AkUInt32 uNumSample = IsOverSampling() ?
		in_uNumSample * OSCOVERSAMPLING : in_uNumSample;

	// Generate samples for each oscillator
	AkReal32* pfOsc2 = in_pScratch;
	AkReal32* pfOsc1 = pfOsc2 + uNumSample;
	AkReal32* pScratch = pfOsc1 + uNumSample;

	// Get oscillator processing functions
	FnSimdOscProc pfnOscProc1 = GetSimdOscProcFn( m_procOsc1 );
	FnSimdOscProc pfnOscProc2 = GetSimdOscProcFn( m_procOsc2 );

	(this->*pfnOscProc2)( uNumSample, m_fBaseFreq, m_procOsc2, pfOsc2, NULL, pScratch );
	(this->*pfnOscProc1)( uNumSample, m_fBaseFreq, m_procOsc1, pfOsc1, pfOsc2, pScratch );

	// Determine where to write results
	AkReal32* pfResult = IsOverSampling() ? pScratch : out_pBuf;

	// Combine oscillator samples
	if( IsMixingMode() )
		CombineSamples<SampleCbxPolicyMix>( uNumSample, pfOsc1, pfOsc2, pfResult );
	else
		CombineSamples<SampleCbxPolicyRing>( uNumSample, pfOsc1, pfOsc2, pfResult );

	if( IsOverSampling() )
	{
		// Filter results
		m_bqFilters[0].ProcessBufferMono(pfResult, uNumSample, uNumSample);
		m_bqFilters[1].ProcessBufferMono(pfResult, uNumSample, uNumSample);
		m_bqFilters[2].ProcessBufferMono(pfResult, uNumSample, uNumSample);

		// Reduce number of samples if oversampling was used
		DecimateSamples( uNumSample, pfResult, out_pBuf );
	}
}

//-----------------------------------------------------------------------------
// Generate pseudo-random number buffer
AkNoInline void CAkSynthOneDsp::GenNoiseBuf( AkUInt32 in_uNumSample, AkReal32* AK_RESTRICT out_pRand )
{
	m_noiseGen.GenerateBuffer( out_pRand, in_uNumSample );
}

//-----------------------------------------------------------------------------

// Generate noise and combine to signal to produce final output
AkNoInline void CAkSynthOneDsp::ProcessNoise( const AkUInt32 in_uNumSample, AkReal32* AK_RESTRICT io_pBuf, AkReal32* AK_RESTRICT in_pRand )
{
	AKSIMD_V4F32 vOutLevel, vOutIncr;
	AKSIMD_V4F32 vNoiseLevel, vNoiseIncr;

	// Prep output level values
	{
		AkReal32 fLevel = m_levelRampOutput.GetCurrent();
		AkReal32 fIncr = m_levelRampOutput.GetIncrement();

		vOutLevel = AKSIMD_SET_V4F32( 0.f );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vOutLevel, 0, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vOutLevel, 1, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vOutLevel, 2, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vOutLevel, 3, fLevel );

		vOutIncr = AKSIMD_SET_V4F32( 4 * fIncr );
	}

	// Prep noise level values
	{
		AkReal32 fLevel = m_levelRampNoise.GetCurrent();
		AkReal32 fIncr = m_levelRampNoise.GetIncrement();

		vNoiseLevel = AKSIMD_SET_V4F32( 0.f );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vNoiseLevel, 0, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vNoiseLevel, 1, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vNoiseLevel, 2, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vNoiseLevel, 3, fLevel );
		vNoiseIncr = AKSIMD_SET_V4F32( 4 * fIncr );
	}

 	for( AkUInt32 i = 0; i < in_uNumSample; i += 4 )
	{
		AKSIMD_V4F32 vOut = AKSIMD_LOAD_V4F32( io_pBuf );
		AKSIMD_V4F32 vRand = AKSIMD_LOAD_V4F32( in_pRand );
		in_pRand += 4;

		AKSIMD_V4F32 vNoise = AKSIMD_MUL_V4F32( vRand, vNoiseLevel );
		vNoiseLevel = AKSIMD_ADD_V4F32( vNoiseLevel, vNoiseIncr );

		vOut = AKSIMD_ADD_V4F32( vOut, vNoise );
		vOut = AKSIMD_MUL_V4F32( vOut, vOutLevel );
		vOutLevel = AKSIMD_ADD_V4F32( vOutLevel, vOutIncr );

		AKSIMD_STORE_V4F32( io_pBuf, vOut );

		io_pBuf += 4;
	}
}

//-----------------------------------------------------------------------------
// Apply output level to final signal
void CAkSynthOneDsp::BypassNoise( const AkUInt32 in_uNumSample, AkReal32* AK_RESTRICT io_pBuf )
{
	AKSIMD_V4F32 vLevel_0, vLevel_1, vLevel_2, vLevel_3, vIncr;

	AkReal32 fLevel = m_levelRampOutput.GetCurrent();
	AkReal32 fIncr = m_levelRampOutput.GetIncrement();

	vLevel_0 = AKSIMD_SET_V4F32( 0.f );
	fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevel_0, 0, fLevel );
	fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevel_0, 1, fLevel );
	fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevel_0, 2, fLevel );
	fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevel_0, 3, fLevel );

	vIncr = AKSIMD_SET_V4F32( 4 * fIncr );

	vLevel_1 = AKSIMD_ADD_V4F32( vLevel_0, vIncr );
	vIncr = AKSIMD_ADD_V4F32( vIncr, vIncr );

	vLevel_2 = AKSIMD_ADD_V4F32( vLevel_0, vIncr );
	vLevel_3 = AKSIMD_ADD_V4F32( vLevel_1, vIncr );

	vIncr = AKSIMD_ADD_V4F32( vIncr, vIncr );

	for( AkUInt32 i = 0; i < in_uNumSample; i += 16 )
	{
		AkUInt32 j = i + 4;
		AkUInt32 m = i + 8;
		AkUInt32 n = i + 12;

		AKSIMD_V4F32 v_0 = AKSIMD_LOAD_V4F32( io_pBuf + i );
		AKSIMD_V4F32 v_1 = AKSIMD_LOAD_V4F32( io_pBuf + j );
		AKSIMD_V4F32 v_2 = AKSIMD_LOAD_V4F32( io_pBuf + m );
		AKSIMD_V4F32 v_3 = AKSIMD_LOAD_V4F32( io_pBuf + n );

		v_0 = AKSIMD_MUL_V4F32( v_0, vLevel_0 );
		v_1 = AKSIMD_MUL_V4F32( v_1, vLevel_1 );
		v_2 = AKSIMD_MUL_V4F32( v_2, vLevel_2 );
		v_3 = AKSIMD_MUL_V4F32( v_3, vLevel_3 );

		vLevel_0 = AKSIMD_ADD_V4F32( vLevel_0, vIncr );
		vLevel_1 = AKSIMD_ADD_V4F32( vLevel_1, vIncr );
		vLevel_2 = AKSIMD_ADD_V4F32( vLevel_2, vIncr );
		vLevel_3 = AKSIMD_ADD_V4F32( vLevel_3, vIncr );

		AKSIMD_STORE_V4F32( io_pBuf + i, v_0 );
		AKSIMD_STORE_V4F32( io_pBuf + j, v_1 );
		AKSIMD_STORE_V4F32( io_pBuf + m, v_2 );
		AKSIMD_STORE_V4F32( io_pBuf + n, v_3 );
	}
}

//-----------------------------------------------------------------------------
// Apply output level to final signal
AkNoInline void CAkSynthOneDsp::FinalOutput( const AkUInt32 in_uNumSample, AkReal32* AK_RESTRICT io_pBuf, AkReal32* AK_RESTRICT in_pScratch )
{
	// Determine if noise must be generated
	if( m_levelRampNoise.GetCurrent() <= LEVELCUTOFF &&
		m_levelRampNoise.GetIncrement() <= LEVELCUTOFF )
	{
		BypassNoise( in_uNumSample, io_pBuf );
	}
	else
	{
		AkReal32* pRand = in_pScratch;
		GenNoiseBuf( in_uNumSample, pRand );
		ProcessNoise( in_uNumSample, io_pBuf, pRand );
	}
}

//-----------------------------------------------------------------------------
// Produce buffer samples.
void CAkSynthOneDsp::SimdProcDsp( const AkUInt32 in_uNumSamples, AkReal32* out_pBuf, AkReal32* in_pScratch )
{
	// Generate oscillator output
	SimdProcOsc( in_uNumSamples, out_pBuf, in_pScratch );

	// Create final output (mix in noise if required)
	FinalOutput( in_uNumSamples, out_pBuf, in_pScratch );
}
