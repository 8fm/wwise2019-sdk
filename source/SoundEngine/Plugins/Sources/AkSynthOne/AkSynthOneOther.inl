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
AkNoInline void GetPhaseFreq( const AkUInt32 in_uNumSample,
								const AkReal32 in_fBaseFreq,
								const AkReal32 in_fInvOverRate,
								AkSynthOneOscProc& io_oscProc,
								AkReal32* AK_RESTRICT out_pPhase,
								AkReal32* AK_RESTRICT out_pFreq )
{
	AkReal32 fFreq = in_fBaseFreq * io_oscProc.transRamp.GetCurrent();
	AkReal32 fFreqInc = in_fBaseFreq * io_oscProc.transRamp.GetIncrement();

	AkReal32 fPhase = io_oscProc.fPhase;
	AkReal32 fPhaseInc = (fFreq + fFreqInc) * in_fInvOverRate;

	AKSIMD_V4F32 vFreq;
	fFreq += fFreqInc; AKSIMD_SETELEMENT_V4F32( vFreq, 0, fFreq );
	fFreq += fFreqInc; AKSIMD_SETELEMENT_V4F32( vFreq, 1, fFreq );
	fFreq += fFreqInc; AKSIMD_SETELEMENT_V4F32( vFreq, 2, fFreq );
	fFreq += fFreqInc; AKSIMD_SETELEMENT_V4F32( vFreq, 3, fFreq );
	AKSIMD_V4F32 vFreqInc = AKSIMD_SET_V4F32( 4 * fFreqInc );

	AKSIMD_V4F32 vPhase;
	AKSIMD_SETELEMENT_V4F32( vPhase, 0, fPhase ); fPhase += fPhaseInc;
	fPhase = ( fPhase < 1.f ) ? fPhase : fPhase - 1.f;
	AKSIMD_SETELEMENT_V4F32( vPhase, 1, fPhase ); fPhase += fPhaseInc;
	fPhase = ( fPhase < 1.f ) ? fPhase : fPhase - 1.f;
	AKSIMD_SETELEMENT_V4F32( vPhase, 2, fPhase ); fPhase += fPhaseInc;
	fPhase = ( fPhase < 1.f ) ? fPhase : fPhase - 1.f;
	AKSIMD_SETELEMENT_V4F32( vPhase, 3, fPhase );
	AKSIMD_V4F32 vPhaseInc = AKSIMD_SET_V4F32( 4 * fPhaseInc );

	// Make sure calculated phase is within range ( 0 <= phase < 1 )
	AKSIMD_V4I32 iPhaseTrunc = AKSIMD_TRUNCATE_V4F32_TO_V4I32( vPhase );
	AKSIMD_V4F32 fPhaseTrunc = AKSIMD_CONVERT_V4I32_TO_V4F32( iPhaseTrunc );
	vPhase = AKSIMD_SUB_V4F32( vPhase, fPhaseTrunc );

	for( AkUInt32 i = 0; i < in_uNumSample; i += 4 )
	{
		AKSIMD_STORE_V4F32( out_pPhase + i, vPhase );
		vPhase = AKSIMD_ADD_V4F32( vPhase, vPhaseInc );

		AKSIMD_STORE_V4F32( out_pFreq + i, vFreq );
		vFreq = AKSIMD_ADD_V4F32( vFreq, vFreqInc );

		AKSIMD_V4I32 iTrunc = AKSIMD_TRUNCATE_V4F32_TO_V4I32( vPhase );
		AKSIMD_V4F32 fTrunc = AKSIMD_CONVERT_V4I32_TO_V4F32( iTrunc );
		vPhase = AKSIMD_SUB_V4F32( vPhase, fTrunc );
	}

	// Store phase for next frame
	io_oscProc.fPhase = AKSIMD_GETELEMENT_V4F32( vPhase, 0 );
}

//-----------------------------------------------------------------------------

static const AKSIMD_V4F32 cvZeroPtTwo = AKSIMD_SET_V4F32( 0.2f );
static const AKSIMD_V4F32 cvOnePtZero = AKSIMD_SET_V4F32( 1.f );
static const AKSIMD_V4F32 cvZero = AKSIMD_SET_V4F32( 0.f );

struct FmPolicyWith
{
	//static AkForceInline void PhaseAfterFm( const AkUInt32 in_uNumSample,
	//										const AkReal32 in_fInvOverRate,
	//										const AkSynthOneOscProc& io_oscProc,
	//										const ValueRamp& in_fmRamp,
	//										AkReal32* AK_RESTRICT io_pPhase,
	//										AkReal32* AK_RESTRICT in_pFreq,
	//										AkReal32* AK_RESTRICT in_pFm )
	//{
	//	AKSIMD_V4F32 vFm_0, vFm_1, vFmInc;

	//	{
	//		AkReal32 fFm = in_fmRamp.GetCurrent();
	//		AkReal32 fFmInc = in_fmRamp.GetIncrement();

	//		fFm += fFmInc; AKSIMD_SETELEMENT_V4F32( vFm_0, 0, fFm );
	//		fFm += fFmInc; AKSIMD_SETELEMENT_V4F32( vFm_0, 1, fFm );
	//		fFm += fFmInc; AKSIMD_SETELEMENT_V4F32( vFm_0, 2, fFm );
	//		fFm += fFmInc; AKSIMD_SETELEMENT_V4F32( vFm_0, 3, fFm );

	//		vFmInc = AKSIMD_SET_V4F32( 4 * fFmInc );
	//		vFm_1 = AKSIMD_ADD_V4F32( vFm_0, vFmInc );
	//		vFmInc = AKSIMD_ADD_V4F32( vFmInc, vFmInc );
	//	}

	//	const AKSIMD_V4F32 vInvOverRate = AKSIMD_SET_V4F32( in_fInvOverRate );
	//	const AKSIMD_V4F32 vConstant = AKSIMD_SET_V4F32( 20000.f * in_fInvOverRate );

	//	for( AkUInt32 i = 0, j = 4; i < in_uNumSample; i += 8, j += 8 )
	//	{
	//		AKSIMD_V4F32 vRegIncr_0 = AKSIMD_LOAD_V4F32( in_pFreq + i );
	//		AKSIMD_V4F32 vRegIncr_1 = AKSIMD_LOAD_V4F32( in_pFreq + j );
	//		vRegIncr_0 = AKSIMD_MUL_V4F32( vRegIncr_0, vInvOverRate );
	//		vRegIncr_1 = AKSIMD_MUL_V4F32( vRegIncr_1, vInvOverRate );

	//		AKSIMD_V4F32 vFmTemp_0 = AKSIMD_MUL_V4F32( vFm_0, cvZeroPtTwo );
	//		AKSIMD_V4F32 vFmTemp_1 = AKSIMD_MUL_V4F32( vFm_1, cvZeroPtTwo );
	//		vRegIncr_0 = AKSIMD_SUB_V4F32( vConstant, vRegIncr_0 );
	//		vRegIncr_1 = AKSIMD_SUB_V4F32( vConstant, vRegIncr_1 );

	//		vFm_0 = AKSIMD_ADD_V4F32( vFm_0, vFmInc );
	//		vFm_1 = AKSIMD_ADD_V4F32( vFm_1, vFmInc );

	//		AKSIMD_V4F32 vPhase_0 = AKSIMD_LOAD_V4F32( io_pPhase + i );
	//		AKSIMD_V4F32 vPhase_1 = AKSIMD_LOAD_V4F32( io_pPhase + j );

	//		AKSIMD_V4F32 vSample_0 = AKSIMD_LOAD_V4F32( in_pFm + i );
	//		AKSIMD_V4F32 vSample_1 = AKSIMD_LOAD_V4F32( in_pFm + j );

	//		vFmTemp_0 = AKSIMD_MUL_V4F32( vSample_0, vFmTemp_0 );
	//		vFmTemp_0 = AKSIMD_MUL_V4F32( vFmTemp_0, vRegIncr_0 );
	//		vPhase_0 = AKSIMD_ADD_V4F32( vPhase_0, vFmTemp_0 );

	//		vFmTemp_1 = AKSIMD_MUL_V4F32( vSample_1, vFmTemp_1 );
	//		vFmTemp_1 = AKSIMD_MUL_V4F32( vFmTemp_1, vRegIncr_1 );
	//		vPhase_1 = AKSIMD_ADD_V4F32( vPhase_1, vFmTemp_1 );

	//		// Make sure phase is in proper range ( 0 <= phase < 1 )
	//		AKSIMD_V4I32 iPhase_0 = AKSIMD_TRUNCATE_V4F32_TO_V4I32( vPhase_0 );
	//		AKSIMD_V4F32 fPhase_0 = AKSIMD_CONVERT_V4I32_TO_V4F32( iPhase_0 );
	//		vPhase_0 = AKSIMD_SUB_V4F32( vPhase_0, fPhase_0 );

	//		AKSIMD_V4I32 iPhase_1 = AKSIMD_TRUNCATE_V4F32_TO_V4I32( vPhase_1 );
	//		AKSIMD_V4F32 fPhase_1 = AKSIMD_CONVERT_V4I32_TO_V4F32( iPhase_1 );
	//		vPhase_1 = AKSIMD_SUB_V4F32( vPhase_1, fPhase_1 );

	//		// Handle negative phase
	//		AKSIMD_V4F32 vPlusOne_0 = AKSIMD_ADD_V4F32( vPhase_0, cvOnePtZero );
	//		AKSIMD_V4COND vNegPhase_0 = AKSIMD_GTEQ_V4F32( vPhase_0, cvZero );
	//		vPhase_0 = AKSIMD_VSEL_V4F32( vPlusOne_0, vPhase_0, vNegPhase_0 );

	//		AKSIMD_V4F32 vPlusOne_1 = AKSIMD_ADD_V4F32( vPhase_1, cvOnePtZero );
	//		AKSIMD_V4COND vNegPhase_1 = AKSIMD_GTEQ_V4F32( vPhase_1, cvZero );
	//		vPhase_1 = AKSIMD_VSEL_V4F32( vPlusOne_1, vPhase_1, vNegPhase_1 );

	//		AKSIMD_STORE_V4F32( io_pPhase + i, vPhase_0 );
	//		AKSIMD_STORE_V4F32( io_pPhase + j, vPhase_1 );
	//	}
	//}

	static AkNoInline void PhaseAfterFm( const AkUInt32 in_uNumSample,
											const AkReal32 in_fInvOverRate,
											const AkSynthOneOscProc& io_oscProc,
											const ValueRamp& in_fmRamp,
											AkReal32* AK_RESTRICT io_pPhase,
											AkReal32* AK_RESTRICT in_pFreq,
											AkReal32* AK_RESTRICT in_pFm )
	{
		AKSIMD_V4F32 vFmTemp;

		vFmTemp = AKSIMD_SET_V4F32( in_fmRamp.GetCurrent() );
		vFmTemp = AKSIMD_MUL_V4F32( vFmTemp, cvZeroPtTwo );

		const AKSIMD_V4F32 vInvOverRate = AKSIMD_SET_V4F32( in_fInvOverRate );
		const AKSIMD_V4F32 vConstant = AKSIMD_SET_V4F32( 20000.f * in_fInvOverRate );

		AKSIMD_V4F32 vRegIncr = AKSIMD_LOAD_V4F32( in_pFreq );
		vRegIncr = AKSIMD_MUL_V4F32( vRegIncr, vInvOverRate );
		vRegIncr = AKSIMD_SUB_V4F32( vConstant, vRegIncr );

		const AKSIMD_V4F32 vRegIncr_0 = vRegIncr;
		const AKSIMD_V4F32 vRegIncr_1 = vRegIncr;

		for( AkUInt32 i = 0, j = 4; i < in_uNumSample; i += 8, j += 8 )
		{
			//AKSIMD_V4F32 vRegIncr_0 = AKSIMD_LOAD_V4F32( in_pFreq + i );
			//vRegIncr_0 = AKSIMD_MUL_V4F32( vRegIncr_0, vInvOverRate );
			//vRegIncr_0 = AKSIMD_SUB_V4F32( vConstant, vRegIncr_0 );

			//AKSIMD_V4F32 vRegIncr_1 = AKSIMD_LOAD_V4F32( in_pFreq + j );
			//vRegIncr_1 = AKSIMD_MUL_V4F32( vRegIncr_1, vInvOverRate );
			//vRegIncr_1 = AKSIMD_SUB_V4F32( vConstant, vRegIncr_1 );

			AKSIMD_V4F32 vPhase_0 = AKSIMD_LOAD_V4F32( io_pPhase + i );
			AKSIMD_V4F32 vSample_0 = AKSIMD_LOAD_V4F32( in_pFm + i );

			AKSIMD_V4F32 vPhase_1 = AKSIMD_LOAD_V4F32( io_pPhase + j );
			AKSIMD_V4F32 vSample_1 = AKSIMD_LOAD_V4F32( in_pFm + j );

			AKSIMD_V4F32 vFmTemp_0 = AKSIMD_MUL_V4F32( vSample_0, vFmTemp );
			vFmTemp_0 = AKSIMD_MUL_V4F32( vFmTemp_0, vRegIncr_0 );
			vPhase_0 = AKSIMD_ADD_V4F32( vPhase_0, vFmTemp_0 );

			AKSIMD_V4F32 vFmTemp_1 = AKSIMD_MUL_V4F32( vSample_1, vFmTemp );
			vFmTemp_1 = AKSIMD_MUL_V4F32( vFmTemp_1, vRegIncr_1 );
			vPhase_1 = AKSIMD_ADD_V4F32( vPhase_1, vFmTemp_1 );

			// Make sure phase is in proper range ( 0 <= phase < 1 )
			AKSIMD_V4I32 iPhase_0 = AKSIMD_TRUNCATE_V4F32_TO_V4I32( vPhase_0 );
			AKSIMD_V4F32 fPhase_0 = AKSIMD_CONVERT_V4I32_TO_V4F32( iPhase_0 );
			vPhase_0 = AKSIMD_SUB_V4F32( vPhase_0, fPhase_0 );

			AKSIMD_V4I32 iPhase_1 = AKSIMD_TRUNCATE_V4F32_TO_V4I32( vPhase_1 );
			AKSIMD_V4F32 fPhase_1 = AKSIMD_CONVERT_V4I32_TO_V4F32( iPhase_1 );
			vPhase_1 = AKSIMD_SUB_V4F32( vPhase_1, fPhase_1 );

			// Handle negative phase
			AKSIMD_V4F32 vPlusOne_0 = AKSIMD_ADD_V4F32( vPhase_0, cvOnePtZero );
			AKSIMD_V4COND vNegPhase_0 = AKSIMD_GTEQ_V4F32( vPhase_0, cvZero );
			vPhase_0 = AKSIMD_VSEL_V4F32( vPlusOne_0, vPhase_0, vNegPhase_0 );

			AKSIMD_V4F32 vPlusOne_1 = AKSIMD_ADD_V4F32( vPhase_1, cvOnePtZero );
			AKSIMD_V4COND vNegPhase_1 = AKSIMD_GTEQ_V4F32( vPhase_1, cvZero );
			vPhase_1 = AKSIMD_VSEL_V4F32( vPlusOne_1, vPhase_1, vNegPhase_1 );

			AKSIMD_STORE_V4F32( io_pPhase + i, vPhase_0 );
			AKSIMD_STORE_V4F32( io_pPhase + j, vPhase_1 );
		}
	}
};

struct FmPolicyNone
{
	static AkForceInline void PhaseAfterFm( const AkUInt32 in_uNumSample,
											const AkReal32 in_fInvOverRate,
											const AkSynthOneOscProc& io_oscProc,
											const ValueRamp& in_fmRamp,
											AkReal32* AK_RESTRICT io_pPhase,
											AkReal32* AK_RESTRICT in_pFreq,
											AkReal32* AK_RESTRICT in_pFm )
	{
	}
};

//-----------------------------------------------------------------------------
static const AKSIMD_V4F32 vPwmZeroPtFive = AKSIMD_SET_V4F32( 0.5f );
static const AKSIMD_V4F32 vPwmOnePtZero = AKSIMD_SET_V4F32( 1.f );

struct PwmPolicyWith
{
	//static AkForceInline void PhaseAfterPwm( const AkUInt32 in_uNumSample,
	//										const AkSynthOneOscProc& io_oscProc,
	//										AkReal32* AK_RESTRICT io_pPhase )
	//{
	//	AKSIMD_V4F32 vPwm_0, vPwm_1, vPwmInc;

	//	{
	//		AkReal32 fPwm = io_oscProc.pwmRamp.GetCurrent() * 0.01f;
	//		AkReal32 fPwmInc = io_oscProc.pwmRamp.GetIncrement() * 0.01f;

	//		fPwm += fPwmInc; AKSIMD_SETELEMENT_V4F32( vPwm_0, 0, fPwm );
	//		fPwm += fPwmInc; AKSIMD_SETELEMENT_V4F32( vPwm_0, 1, fPwm );
	//		fPwm += fPwmInc; AKSIMD_SETELEMENT_V4F32( vPwm_0, 2, fPwm );
	//		fPwm += fPwmInc; AKSIMD_SETELEMENT_V4F32( vPwm_0, 3, fPwm );

	//		vPwmInc = AKSIMD_SET_V4F32( 4 * fPwmInc );
	//		vPwm_1 = AKSIMD_ADD_V4F32( vPwm_0, vPwmInc );
	//		vPwmInc = AKSIMD_ADD_V4F32( vPwmInc, vPwmInc );
	//	}

	//	// Code equivalent; will select numerator and denominator, then perform division
	//	//
	//	//if( in_fPhase < fPwmNow )
	//	//	fNewPhase = (in_fPhase / fPwmNow) * 0.5f;
	//	//else
	//	//	fNewPhase = ( ( (in_fPhase - fPwmNow) / (1 - fPwmNow) ) * 0.5f ) + 0.5f;

	//	for( AkUInt32 i = 0, j = 4; i < in_uNumSample; i += 8, j += 8 )
	//	{
	//		AKSIMD_V4F32 vPhase_0 = AKSIMD_LOAD_V4F32( io_pPhase + i );
	//		AKSIMD_V4F32 vPhase_1 = AKSIMD_LOAD_V4F32( io_pPhase + j );

	//		AKSIMD_V4COND vCond_0 = AKSIMD_GTEQ_V4F32( vPhase_0, vPwm_0 );
	//		AKSIMD_V4COND vCond_1 = AKSIMD_GTEQ_V4F32( vPhase_1, vPwm_1 );

	//		AKSIMD_V4F32 vNumer_0 = AKSIMD_SUB_V4F32( vPhase_0, vPwm_0 );
	//		AKSIMD_V4F32 vNumer_1 = AKSIMD_SUB_V4F32( vPhase_1, vPwm_1 );

	//		AKSIMD_V4F32 vDenom_0 = AKSIMD_SUB_V4F32( vPwmOnePtZero, vPwm_0 );
	//		AKSIMD_V4F32 vDenom_1 = AKSIMD_SUB_V4F32( vPwmOnePtZero, vPwm_1 );

	//		AKSIMD_V4F32 vAdd_0 = AKSIMD_SET_V4F32( 0.f );
	//		AKSIMD_V4F32 vAdd_1 = AKSIMD_SET_V4F32( 0.f );

	//		vNumer_0 = AKSIMD_VSEL_V4F32( vPhase_0, vNumer_0, vCond_0 );
	//		vDenom_0 = AKSIMD_VSEL_V4F32( vPwm_0, vDenom_0, vCond_0 );
	//		vAdd_0 = AKSIMD_VSEL_V4F32( vAdd_0, vPwmZeroPtFive, vCond_0 );

	//		vNumer_1 = AKSIMD_VSEL_V4F32( vPhase_1, vNumer_1, vCond_1 );
	//		vDenom_1 = AKSIMD_VSEL_V4F32( vPwm_1, vDenom_1, vCond_1 );
	//		vAdd_1 = AKSIMD_VSEL_V4F32( vAdd_1, vPwmZeroPtFive, vCond_1 );

	//		vPwm_0 = AKSIMD_ADD_V4F32( vPwm_0, vPwmInc );
	//		vPwm_1 = AKSIMD_ADD_V4F32( vPwm_1, vPwmInc );

	//		vNumer_0 = AKSIMD_MUL_V4F32( vNumer_0, vPwmZeroPtFive );
	//		AKSIMD_V4F32 vTemp_0 = AKSIMD_DIV_V4F32( vNumer_0, vDenom_0 );
	//		vTemp_0 = AKSIMD_ADD_V4F32( vTemp_0, vAdd_0 );

	//		vNumer_1 = AKSIMD_MUL_V4F32( vNumer_1, vPwmZeroPtFive );
	//		AKSIMD_V4F32 vTemp_1 = AKSIMD_DIV_V4F32( vNumer_1, vDenom_1 );
	//		vTemp_1 = AKSIMD_ADD_V4F32( vTemp_1, vAdd_1 );

	//		AKSIMD_V4COND vPhaseCond_0 = AKSIMD_GTEQ_V4F32( vTemp_0, cvOnePtZero );
	//		vPhase_0 = AKSIMD_VSEL_V4F32( vTemp_0, cvZero, vPhaseCond_0 );

	//		AKSIMD_V4COND vPhaseCond_1 = AKSIMD_GTEQ_V4F32( vTemp_1, cvOnePtZero );
	//		vPhase_1 = AKSIMD_VSEL_V4F32( vTemp_1, cvZero, vPhaseCond_1 );

	//		AKSIMD_STORE_V4F32( io_pPhase + i, vPhase_0 );
	//		AKSIMD_STORE_V4F32( io_pPhase + j, vPhase_1 );
	//	}
	//}

	static AkNoInline void PhaseAfterPwm( const AkUInt32 in_uNumSample,
											const AkSynthOneOscProc& io_oscProc,
											AkReal32* AK_RESTRICT io_pPhase )
	{
		AKSIMD_V4F32 vPwm;

		{
			AkReal32 fPwm = io_oscProc.pwmRamp.GetCurrent() * 0.01f;
			vPwm = AKSIMD_SET_V4F32( fPwm );
		}

		// Code equivalent; will select numerator and denominator, then perform division
		//
		//if( in_fPhase < fPwmNow )
		//	fNewPhase = (in_fPhase / fPwmNow) * 0.5f;
		//else
		//	fNewPhase = ( ( (in_fPhase - fPwmNow) / (1 - fPwmNow) ) * 0.5f ) + 0.5f;

		for( AkUInt32 i = 0, j = 4; i < in_uNumSample; i += 8, j += 8 )
		{
			AKSIMD_V4F32 vPhase_0 = AKSIMD_LOAD_V4F32( io_pPhase + i );
			AKSIMD_V4F32 vPhase_1 = AKSIMD_LOAD_V4F32( io_pPhase + j );

			AKSIMD_V4COND vCond_0 = AKSIMD_GTEQ_V4F32( vPhase_0, vPwm );
			AKSIMD_V4COND vCond_1 = AKSIMD_GTEQ_V4F32( vPhase_1, vPwm );

			AKSIMD_V4F32 vNumer_0 = AKSIMD_SUB_V4F32( vPhase_0, vPwm );
			AKSIMD_V4F32 vNumer_1 = AKSIMD_SUB_V4F32( vPhase_1, vPwm );

			AKSIMD_V4F32 vDenom_0 = AKSIMD_SUB_V4F32( vPwmOnePtZero, vPwm );
			AKSIMD_V4F32 vDenom_1 = AKSIMD_SUB_V4F32( vPwmOnePtZero, vPwm );

			AKSIMD_V4F32 vAdd_0 = AKSIMD_SET_V4F32( 0.f );
			AKSIMD_V4F32 vAdd_1 = AKSIMD_SET_V4F32( 0.f );

			vNumer_0 = AKSIMD_VSEL_V4F32( vPhase_0, vNumer_0, vCond_0 );
			vDenom_0 = AKSIMD_VSEL_V4F32( vPwm, vDenom_0, vCond_0 );
			vAdd_0 = AKSIMD_VSEL_V4F32( vAdd_0, vPwmZeroPtFive, vCond_0 );

			vNumer_1 = AKSIMD_VSEL_V4F32( vPhase_1, vNumer_1, vCond_1 );
			vDenom_1 = AKSIMD_VSEL_V4F32( vPwm, vDenom_1, vCond_1 );
			vAdd_1 = AKSIMD_VSEL_V4F32( vAdd_1, vPwmZeroPtFive, vCond_1 );

			vNumer_0 = AKSIMD_MUL_V4F32( vNumer_0, vPwmZeroPtFive );
			AKSIMD_V4F32 vTemp_0 = AKSIMD_DIV_V4F32( vNumer_0, vDenom_0 );
			vTemp_0 = AKSIMD_ADD_V4F32( vTemp_0, vAdd_0 );

			vNumer_1 = AKSIMD_MUL_V4F32( vNumer_1, vPwmZeroPtFive );
			AKSIMD_V4F32 vTemp_1 = AKSIMD_DIV_V4F32( vNumer_1, vDenom_1 );
			vTemp_1 = AKSIMD_ADD_V4F32( vTemp_1, vAdd_1 );

			AKSIMD_V4COND vPhaseCond_0 = AKSIMD_GTEQ_V4F32( vTemp_0, cvOnePtZero );
			vPhase_0 = AKSIMD_VSEL_V4F32( vTemp_0, cvZero, vPhaseCond_0 );

			AKSIMD_V4COND vPhaseCond_1 = AKSIMD_GTEQ_V4F32( vTemp_1, cvOnePtZero );
			vPhase_1 = AKSIMD_VSEL_V4F32( vTemp_1, cvZero, vPhaseCond_1 );

			AKSIMD_STORE_V4F32( io_pPhase + i, vPhase_0 );
			AKSIMD_STORE_V4F32( io_pPhase + j, vPhase_1 );
		}
	}
};

struct PwmPolicyNone
{
	static AkForceInline void PhaseAfterPwm( const AkUInt32 in_uNumSample,
											const AkSynthOneOscProc& io_oscProc,
											AkReal32* AK_RESTRICT io_pPhase )
	{
	}
};

//-----------------------------------------------------------------------------
AkForceInline void GetSampleIndex( const AkUInt32 in_uNumSample,
									AkReal32* AK_RESTRICT in_pPhase,
									AkInt32* AK_RESTRICT out_pLeft,
									AkInt32* AK_RESTRICT out_pRight,
									AkReal32* AK_RESTRICT out_pfFrac )
{
	static const AKSIMD_V4F32 vSize = AKSIMD_SET_V4F32( OSC_WAVETABLESIZE );
	static const AKSIMD_V4I32 vMask = AKSIMD_SET_V4I32( OSC_WAVETABLESIZEMASK );
	static const AKSIMD_V4I32 vOne = AKSIMD_SET_V4I32( 1 );

	AkReal32* AK_RESTRICT pPhase = in_pPhase;
	AkReal32* AK_RESTRICT pLeft = (AkReal32*)out_pLeft;
	AkReal32* AK_RESTRICT pRight = (AkReal32*)out_pRight;
	AkReal32* AK_RESTRICT pFrac = out_pfFrac;

	for( AkUInt32 i = 0, j = 4; i < in_uNumSample; i += 8, j += 8 )
	{
		AKSIMD_V4F32 vPhase_0 = AKSIMD_LOAD_V4F32( in_pPhase + i );
		AKSIMD_V4F32 vPhase_1 = AKSIMD_LOAD_V4F32( in_pPhase + j );

		vPhase_0 = AKSIMD_MUL_V4F32( vPhase_0, vSize );
		vPhase_1 = AKSIMD_MUL_V4F32( vPhase_1, vSize );

		AKSIMD_V4I32 vLeft_0 = AKSIMD_TRUNCATE_V4F32_TO_V4I32( vPhase_0 );
		AKSIMD_V4I32 vLeft_1 = AKSIMD_TRUNCATE_V4F32_TO_V4I32( vPhase_1 );
		AKSIMD_V4I32 vRight_0 = AKSIMD_ADD_V4I32( vLeft_0, vOne );
		AKSIMD_V4I32 vRight_1 = AKSIMD_ADD_V4I32( vLeft_1, vOne );
		vRight_0 = AKSIMD_AND_V4I32( vRight_0, vMask );
		vRight_1 = AKSIMD_AND_V4I32( vRight_1, vMask );

		SYNTHONE_ASSERT_IDX( vLeft_0 ); SYNTHONE_ASSERT_IDX( vRight_0 );
		SYNTHONE_ASSERT_IDX( vLeft_1 ); SYNTHONE_ASSERT_IDX( vRight_1 );

		AKSIMD_V4F32 vFrac_0 = AKSIMD_CONVERT_V4I32_TO_V4F32( vLeft_0 );
		AKSIMD_V4F32 vFrac_1 = AKSIMD_CONVERT_V4I32_TO_V4F32( vLeft_1 );

		AKSIMD_STORE_V4F32( (AkReal32*)(out_pLeft + i), *(AKSIMD_V4F32*)&vLeft_0 );
		AKSIMD_STORE_V4F32( (AkReal32*)(out_pRight + i), *(AKSIMD_V4F32*)&vRight_0 );
		AKSIMD_STORE_V4F32( (AkReal32*)(out_pLeft + j), *(AKSIMD_V4F32*)&vLeft_1 );
		AKSIMD_STORE_V4F32( (AkReal32*)(out_pRight + j), *(AKSIMD_V4F32*)&vRight_1 );

		vFrac_0 = AKSIMD_SUB_V4F32( vPhase_0, vFrac_0 );
		vFrac_1 = AKSIMD_SUB_V4F32( vPhase_1, vFrac_1 );
		SYNTHONE_ASSERT_FRAC( vFrac_0 );
		SYNTHONE_ASSERT_FRAC( vFrac_1 );

		AKSIMD_STORE_V4F32( out_pfFrac + i, vFrac_0 );
		AKSIMD_STORE_V4F32( out_pfFrac + j, vFrac_1 );
	}
}

//-----------------------------------------------------------------------------
AkForceInline void LookupSamples( const AkReal32* const in_pWav, const AkUInt32 in_uNumSample, AkInt32* AK_RESTRICT io_pLeft, AkInt32* AK_RESTRICT io_pRight )
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
AkForceInline void InterpolateSamples( const AkUInt32 in_uNumSample, const bool in_bInvert, AkReal32* AK_RESTRICT in_pLeft, AkReal32* AK_RESTRICT in_pRight, AkReal32* AK_RESTRICT in_pFrac, AkReal32* AK_RESTRICT out_pIntr )
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

		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc1, 0, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc1, 1, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc1, 2, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc1, 3, fLevel );
		vIncrOsc1 = AKSIMD_SET_V4F32( 4 * fIncr );
	}

	{
		AkReal32 fLevel = m_procOsc2.levelRamp.GetCurrent();
		AkReal32 fIncr = m_procOsc2.levelRamp.GetIncrement();

		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc2, 0, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc2, 1, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc2, 2, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vLevelOsc2, 3, fLevel );
		vIncrOsc2 = AKSIMD_SET_V4F32( 4 * fIncr );
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
AkNoInline void DecimateSamples( const AkUInt32 in_uNumSample, AkReal32* AK_RESTRICT in_pIn, AkReal32* AK_RESTRICT out_pOut )
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
AkNoInline void CAkSynthOneDsp::SimdProcOscX( const AkUInt32 in_uNumSample, const AkReal32 in_fBaseFreq, AkSynthOneOscProc& io_oscProc, AkReal32* AK_RESTRICT out_pOsc, AkReal32* AK_RESTRICT in_pFm, AkReal32* AK_RESTRICT in_pScratch )
{
	AkReal32* pPhase = in_pScratch;
	AkReal32* pFreq = pPhase + in_uNumSample;

	GetPhaseFreq( in_uNumSample, in_fBaseFreq, m_fInvOversampledRate, io_oscProc, pPhase,  pFreq );
	FmPolicy::PhaseAfterFm( in_uNumSample, m_fInvOversampledRate, io_oscProc, m_fmAmountRamp, pPhase, pFreq, in_pFm );
	PwmPolicy::PhaseAfterPwm( in_uNumSample, io_oscProc, pPhase );

	// Use scratch buffer to store sample indices
	AkInt32* piLeft = (AkInt32*)(in_pScratch);
	AkInt32* piRight = piLeft + in_uNumSample;
	AkReal32* pfFrac = (AkReal32*)(piRight + in_uNumSample);

	GetSampleIndex( in_uNumSample, pPhase, piLeft, piRight, pfFrac );
	LookupSamples( io_oscProc.pWavTable, in_uNumSample, piLeft, piRight );

	// Finally, interpolate samples to generate final oscillator buffer
	AkReal32* pfLeft = (AkReal32*)piLeft;
	AkReal32* pfRight = (AkReal32*)piRight;
	InterpolateSamples( in_uNumSample, io_oscProc.bInvert, pfLeft, pfRight, pfFrac, out_pOsc );
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
	AkReal32* pfOsc1 = in_pScratch;
	AkReal32* pfOsc2 = pfOsc1 + uNumSample;
	AkReal32* pScratch = pfOsc2 + uNumSample;

	// Get oscillator processing functions
	FnSimdOscProc pfnPscProc1 = GetSimdOscProcFn( m_procOsc1 );
	FnSimdOscProc pfnPscProc2 = GetSimdOscProcFn( m_procOsc2 );

	(this->*pfnPscProc2)( uNumSample, m_fBaseFreq, m_procOsc2, pfOsc2, NULL, pScratch );
	(this->*pfnPscProc1)( uNumSample, m_fBaseFreq, m_procOsc1, pfOsc1, pfOsc2, pScratch );

	// Determine where to write results
	AkReal32* pfResult = IsOverSampling() ? pfOsc1 : out_pBuf;

	// Combine oscillator samples
	if( IsMixingMode() )
		CombineSamples<SampleCbxPolicyMix>( uNumSample, pfOsc1, pfOsc2, pfResult );
	else
		CombineSamples<SampleCbxPolicyRing>( uNumSample, pfOsc1, pfOsc2, pfResult );

	if( IsOverSampling() )
	{
		// Filter results
		m_bqFilters[0].ProcessBufferMono( pfResult, uNumSample );
		m_bqFilters[1].ProcessBufferMono(pfResult, uNumSample);
		m_bqFilters[2].ProcessBufferMono(pfResult, uNumSample);

		// Reduce number of samples if oversampling was used
		DecimateSamples( uNumSample, pfResult, out_pBuf );
	}
}

//-----------------------------------------------------------------------------
// Rand seed generation; the seed values are split amongst two vectors to make
// multiplication easier.
// seed 0 -> r0[31:0]
// seed 1 -> r0[95:64]
// seed 2 -> r1[31:0]
// seed 3 -> r1[95:64]

#define RAND_SEED_MUL	196314165
#define RAND_SEED_ADD	907633515
/*
AkForceInline AKSIMD_V4I32 RandSeedSimd( AKSIMD_V4I32& io_r0, AKSIMD_V4I32& io_r1, AkUInt32 in_aRand[4] )
{
	static const AKSIMD_V4I32 cm = AKSIMD_SETV_V4I32( 0, RAND_SEED_MUL, 0, RAND_SEED_MUL );
	static const AKSIMD_V4I32 ca = AKSIMD_SETV_V4I32( 0, RAND_SEED_ADD, 0, RAND_SEED_ADD );

	io_r0 = AKSIMD_MUL_V4I32( io_r0, cm );
	io_r1 = AKSIMD_MUL_V4I32( io_r1, cm );

	io_r0 = AKSIMD_ADD_V4I32( io_r0, ca );
	io_r1 = AKSIMD_ADD_V4I32( io_r1, ca );

	AKSIMD_V4F32 mux = AKSIMD_MOVEEV_V4F32( *(AKSIMD_V4F32*)&io_r0, *(AKSIMD_V4F32*)&io_r1 );
	return *(AKSIMD_V4I32*)&mux;
}

AkForceInline void LoadRandSeed( AKSIMD_V4I32& out_r0, AKSIMD_V4I32& out_r1, AkUInt32 in_aRand[4] )
{
	AKSIMD_GETELEMENT_V4I32( out_r0, 0 ) = in_aRand[0];
	AKSIMD_GETELEMENT_V4I32( out_r0, 2 ) = in_aRand[1];

	AKSIMD_GETELEMENT_V4I32( out_r1, 0 ) = in_aRand[2];
	AKSIMD_GETELEMENT_V4I32( out_r1, 2 ) = in_aRand[3];
}

AkForceInline void StoreRandSeed( AKSIMD_V4I32& in_r0, AKSIMD_V4I32& in_r1, AkUInt32 out_aRand[4] )
{
	out_aRand[0] = AKSIMD_GETELEMENT_V4I32( in_r0, 0 );
	out_aRand[1] = AKSIMD_GETELEMENT_V4I32( in_r0, 2 );
	out_aRand[2] = AKSIMD_GETELEMENT_V4I32( in_r1, 0 );
	out_aRand[3] = AKSIMD_GETELEMENT_V4I32( in_r1, 2 );
}
*/
//-----------------------------------------------------------------------------
// Generate pseudo-random number buffer
AkNoInline void CAkSynthOneDsp::GenNoiseBuf( AkUInt32 in_uNumSample, AkUInt32* AK_RESTRICT out_pRand )
{
	//for( AkUInt32 i = 0; i < in_uNumSample; ++i )
	//{
	//	m_uSeedVal = (m_uSeedVal * RAND_SEED_MUL) + RAND_SEED_ADD;
	//	*out_pRand++ = m_uSeedVal;
	//}

	for( AkUInt32 i = 0; i < in_uNumSample; i += 4 )
	{
		m_aRandSeeds[0] = (m_aRandSeeds[0] * RAND_SEED_MUL) + RAND_SEED_ADD;
		m_aRandSeeds[1] = (m_aRandSeeds[1] * RAND_SEED_MUL) + RAND_SEED_ADD;
		m_aRandSeeds[2] = (m_aRandSeeds[2] * RAND_SEED_MUL) + RAND_SEED_ADD;
		m_aRandSeeds[3] = (m_aRandSeeds[3] * RAND_SEED_MUL) + RAND_SEED_ADD;

		out_pRand[0] = m_aRandSeeds[0];
		out_pRand[1] = m_aRandSeeds[1];
		out_pRand[2] = m_aRandSeeds[2];
		out_pRand[3] = m_aRandSeeds[3];

		out_pRand += 4;
	}
}

//-----------------------------------------------------------------------------

// Prep variables for pseudo-random generation
static const AKSIMD_V4I32 cNoiseMask = AKSIMD_SET_V4I32( 0x40000000 );
static const AKSIMD_V4I32 sNoiseMask = AKSIMD_SET_V4I32( 0x3fffffff );

#define ONEOVERMAXRAND ( 1.f / ((1 << 30) - 1) )

static const AKSIMD_V4I32 iNoiseZero = AKSIMD_SET_V4I32( 0 );
static const AKSIMD_V4F32 fNoiseOneOver = AKSIMD_SET_V4F32( ONEOVERMAXRAND );

// Generate noise and combine to signal to produce final output
AkNoInline void CAkSynthOneDsp::ProcessNoise( const AkUInt32 in_uNumSample, AkReal32* AK_RESTRICT io_pBuf, AkUInt32* AK_RESTRICT in_pRand )
{
	AKSIMD_V4F32 vOutLevel, vOutIncr;
	AKSIMD_V4F32 vNoiseLevel, vNoiseIncr;

	// Prep output level values
	{
		AkReal32 fLevel = m_levelRampOutput.GetCurrent();
		AkReal32 fIncr = m_levelRampOutput.GetIncrement();

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

		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vNoiseLevel, 0, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vNoiseLevel, 1, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vNoiseLevel, 2, fLevel );
		fLevel += fIncr; AKSIMD_SETELEMENT_V4F32( vNoiseLevel, 3, fLevel );
		vNoiseIncr = AKSIMD_SET_V4F32( 4 * fIncr );
	}

 	for( AkUInt32 i = 0; i < in_uNumSample; i += 4 )
	{
		AKSIMD_V4F32 vOut = AKSIMD_LOAD_V4F32( io_pBuf );

		//AKSIMD_V4I32 vRand = RandSeedSimd( r0, r1, m_uRandSeed );
		AKSIMD_V4I32 vRand = AKSIMD_LOAD_V4I32( (AKSIMD_V4I32*)in_pRand );
		in_pRand += 4;

		AKSIMD_V4I32 ibit = AKSIMD_AND_V4I32( vRand, cNoiseMask ); // bit 15 is used to determine sign of result
		AKSIMD_V4I32 iseed = AKSIMD_AND_V4I32( vRand, sNoiseMask ); // bits 14:0 are the absolute value of the result
		AKSIMD_V4ICOND cond = AKSIMD_EQ_V4I32( ibit, iNoiseZero );

		AKSIMD_V4F32 fseed = AKSIMD_CONVERT_V4I32_TO_V4F32( iseed );
		fseed = AKSIMD_MUL_V4F32( fseed, fNoiseOneOver );
		AKSIMD_V4F32 nseed = AKSIMD_NEG_V4F32( fseed );
		fseed = AKSIMD_VSEL_V4F32( fseed, nseed, *(AKSIMD_V4COND*)&cond );

		fseed = AKSIMD_MUL_V4F32( fseed, vNoiseLevel );
		vNoiseLevel = AKSIMD_ADD_V4F32( vNoiseLevel, vNoiseIncr );

		vOut = AKSIMD_ADD_V4F32( vOut, fseed );
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
		AkUInt32* pRand = (AkUInt32*)in_pScratch;
		GenNoiseBuf( in_uNumSample, pRand );
		ProcessNoise( in_uNumSample, io_pBuf, pRand );
	}
}

//-----------------------------------------------------------------------------
// Produce buffer samples.
void CAkSynthOneDsp::SimdProcDsp( const AkUInt32 in_uNumSamples, AkReal32* out_pBuf, AkReal32* in_pScratch, AkUInt32 in_uScratchSize )
{
	// Generate oscillator output
	SimdProcOsc( in_uNumSamples, out_pBuf, in_pScratch );

	// Create final output (mix in noise if required)
	FinalOutput( in_uNumSamples, out_pBuf, in_pScratch );
}
