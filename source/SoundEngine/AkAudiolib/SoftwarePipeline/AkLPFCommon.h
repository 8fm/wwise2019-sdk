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

#ifndef _AK_LPF_COMMON_H_
#define _AK_LPF_COMMON_H_

#include "AkInternalLPFState.h"
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include "AkSettings.h"

struct AkLpfParamEval
{
	static AkReal32 EvalCutoff( AkReal32 in_fCurrentLPFPar, const AkUInt32 in_uMaxFrames );

	static AkForceInline bool EvalBypass( AkReal32 in_fCurrentLPFPar )
	{
		return in_fCurrentLPFPar <= BYPASSMAXVAL;
	}

	static AkForceInline void ComputeCoefs(DSP::BiquadFilterMultiSIMD & in_rFilter, AkReal32 fCutFreq)
	{
		AKASSERT(fCutFreq <= AK_CORE_SAMPLERATE/2);
		AkReal32 PiFcOSr			= PI * ( fCutFreq / AK_CORE_SAMPLERATE );
		AkReal32 fIntVal			= 1.f/tanf(PiFcOSr);
		AkReal32 fRootTwoxIntVal	= ROOTTWO * fIntVal;
		AkReal32 fSqIntVal			= fIntVal * fIntVal;

		AkReal32 fB0 = 1.0f / ( 1.0f + fRootTwoxIntVal + fSqIntVal);
		in_rFilter.SetCoefs(fB0, fB0+fB0, fB0, 2.0f * ( 1.0f - fSqIntVal) *fB0, ( 1.0f - fRootTwoxIntVal + fSqIntVal) * fB0);
	}
};

struct AkHpfParamEval
{
	static AkReal32 EvalCutoff( AkReal32 in_fCurrentLPFPar, const AkUInt32 in_uMaxFrames );

	static AkForceInline bool EvalBypass( AkReal32 in_fCurrentLPFPar )
	{
		return in_fCurrentLPFPar <= BYPASSMAXVAL;
	}

	static AkForceInline void ComputeCoefs(DSP::BiquadFilterMultiSIMD & in_rFilter, AkReal32 fCutFreq)
	{
		// Butterworth high pass for flat pass band
		AKASSERT(fCutFreq <= AK_CORE_SAMPLERATE/2);
		AkReal32 PiFcOSr	= PI * ( fCutFreq / AK_CORE_SAMPLERATE );
		AkReal32 fTanPiFcSr = tanf(PiFcOSr);
		AkReal32 fRootTwoxIntVal	= ROOTTWO * fTanPiFcSr;
		AkReal32 fSqIntVal =	fTanPiFcSr	* fTanPiFcSr;
		AkReal32 fOnePlusSqIntVal = 1.f + fSqIntVal;

		// Coefficient formulas
		AkReal32 fb0 = (1.0f / ( fRootTwoxIntVal + fOnePlusSqIntVal));
		AkReal32 fb1 = -2.f * fb0;
		AkReal32 fb2 = fb0;
		AkReal32 fa0 = 1.f;
		AkReal32 fa1 = -fb1 * ( fSqIntVal - 1.0f );
		AkReal32 fa2 = ( fOnePlusSqIntVal - fRootTwoxIntVal ) * fb0;

		in_rFilter.SetCoefs(fb0/fa0, fb1/fa0, fb2/fa0, fa1/fa0, fa2/fa0);
	}
};
#endif // _AK_LPF_COMMON_H_
