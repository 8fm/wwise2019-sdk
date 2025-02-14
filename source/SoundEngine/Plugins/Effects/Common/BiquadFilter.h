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

// Direct form biquad filter y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
// To be used on mono signals, create as many instances as there are channels if need be

#ifndef _AKBIQUADFILTER_H_
#define _AKBIQUADFILTER_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkSimd.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>
#include "AkDSPUtils.h"
#include <AK/Tools/Common/AkAssert.h>
#include <AK/Tools/Common/AkObject.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include "AkMath.h"

namespace DSP
{
	// Filter memories
	struct Memories
	{		
		AkReal32 fFFwd1;
		AkReal32 fFFwd2;
		AkReal32 fFFbk1;
		AkReal32 fFFbk2;

		Memories()
		{
			fFFwd1 = 0.f;
			fFFwd2 = 0.f;
			fFFbk1 = 0.f;
			fFFbk2 = 0.f;
		}
	};

	struct BiquadCoefficients
	{
		AkReal32 fB0;
		AkReal32 fB1;
		AkReal32 fB2;
		AkReal32 fA1;
		AkReal32 fA2;
	};

	// Must align with AkFilterType (offset by 1)
	enum FilterType
	{
		FilterType_LowShelf = 0,
		FilterType_Peaking,
		FilterType_HighShelf,
		FilterType_LowPass,
		FilterType_HighPass,
		FilterType_BandPass,
		FilterType_Notch,
		FilterType_LowShelfQ,
		FilterType_HighShelfQ
	};

	static inline void ComputeBiquadCoefs(		
		AkReal32 &out_fb0,
		AkReal32 &out_fb1,
		AkReal32 &out_fb2,		
		AkReal32 &out_fa1,
		AkReal32 &out_fa2,
		FilterType in_eFilterType,
		const AkReal32 in_fSampleRate,
		const AkReal32 in_fFreq,
		const AkReal32 in_fGain = 0.f,
		const AkReal32 in_fQ = 1.f)
	{
		// Maximum shelf slope
#define SHELFSLOPE 1.f 
#define SQRT2 1.41421356237309504880f

		AkReal32 fb0 = 0.f;
		AkReal32 fb1 = 0.f;
		AkReal32 fb2 = 0.f;	// Feed forward coefficients
		AkReal32 fa0 = 0.f;
		AkReal32 fa1 = 0.f;
		AkReal32 fa2 = 0.f;	// Feed back coefficients

		// Note: Q and "bandwidth" linked by:
		// 1/Q = 2*sinh(ln(2)/2*BandWidth*fOmega/sin(fOmega))

		AkReal32 fFrequency = in_fFreq;

		// Frequency must be less or equal to the half of the sample rate
		// 0.9 is there because of bilinear transform behavior at frequencies close to Nyquist
		const AkReal32 fMaxFrequency = in_fSampleRate * 0.5f * 0.9f;
		fFrequency = AkMin(fMaxFrequency, fFrequency);

		if(in_eFilterType == FilterType_LowPass)
		{
			// Butterworth low pass for flat pass band
			AkReal32 PiFcOSr = PI * fFrequency / in_fSampleRate;
			AkReal32 fTanPiFcSr = tanf(PiFcOSr);
			AkReal32 fIntVal = 1.0f / fTanPiFcSr;
			AkReal32 fRootTwoxIntVal = SQRT2 * fIntVal;
			AkReal32 fSqIntVal = fIntVal	* fIntVal;
			AkReal32 fOnePlusSqIntVal = 1.f + fSqIntVal;
			// Coefficient formulas
			fb0 = (1.0f / (fRootTwoxIntVal + fOnePlusSqIntVal));
			fb1 = 2.f * fb0;
			fb2 = fb0;
			fa0 = 1.f;
			fa1 = fb1 * (1.0f - fSqIntVal);
			fa2 = (fOnePlusSqIntVal - fRootTwoxIntVal) * fb0;
		}
		else if(in_eFilterType == FilterType_HighPass)
		{
			// Butterworth high pass for flat pass band
			AkReal32 PiFcOSr = PI * fFrequency / in_fSampleRate;
			AkReal32 fTanPiFcSr = tanf(PiFcOSr);
			AkReal32 fRootTwoxIntVal = SQRT2 * fTanPiFcSr;
			AkReal32 fSqIntVal = fTanPiFcSr	* fTanPiFcSr;
			AkReal32 fOnePlusSqIntVal = 1.f + fSqIntVal;
			// Coefficient formulas
			fb0 = (1.0f / (fRootTwoxIntVal + fOnePlusSqIntVal));
			fb1 = -2.f * fb0;
			fb2 = fb0;
			fa0 = 1.f;
			fa1 = -fb1 * (fSqIntVal - 1.0f);
			fa2 = (fOnePlusSqIntVal - fRootTwoxIntVal) * fb0;
		}
		else if(in_eFilterType == FilterType_BandPass)
		{
			AkReal32 fOmega = 2.f * PI * fFrequency / in_fSampleRate;	// Normalized angular frequency
			AkReal32 fCosOmega = cosf(fOmega);											// Cos omega
			AkReal32 fSinOmega = sinf(fOmega);											// Sin omega
			// 0 dB peak (normalized passband gain)
			// alpha = sin(w0)/(2*Q)
			AkReal32 fAlpha = fSinOmega / (2.f*in_fQ);
			// Coefficient formulas
			fb0 = fAlpha;
			fb1 = 0.f;
			fb2 = -fAlpha;
			fa0 = 1.f + fAlpha;
			fa1 = -2.f*fCosOmega;
			fa2 = 1.f - fAlpha;
		}
		else if(in_eFilterType == FilterType_Notch)
		{
			AkReal32 fOmega = 2.f * PI * fFrequency / in_fSampleRate;	// Normalized angular frequency
			AkReal32 fCosOmega = cosf(fOmega);											// Cos omega
			AkReal32 fSinOmega = sinf(fOmega);											// Sin omega
			// Normalized passband gain
			AkReal32 fAlpha = fSinOmega / (2.f*in_fQ);
			// Coefficient formulas
			fb0 = 1.f;
			fb1 = -2.f*fCosOmega;
			fb2 = 1.f;
			fa0 = 1.f + fAlpha;
			fa1 = fb1;
			fa2 = 1.f - fAlpha;
		}
		else if(in_eFilterType == FilterType_LowShelf)
		{
#ifdef AKSIMD_FAST_EXP_IMPLEMENTED
			AkReal32 fLinAmp = AKSIMD_POW10_SCALAR(in_fGain*0.025f);
#else
			AkReal32 fLinAmp = AkMath::FastPow10(in_fGain*0.025f);
#endif
			AkReal32 fOmega = 2.f * PI * fFrequency / in_fSampleRate;	// Normalized angular frequency
			AkReal32 fAlpha = sinf(fOmega) / 2.f * sqrtf((fLinAmp + 1.f / fLinAmp)*(1.f / SHELFSLOPE - 1.f) + 2.f);
			AkReal32 fCosOmega = cosf(fOmega);
			AkReal32 fLinAmpPlusOne = fLinAmp + 1.f;
			AkReal32 fLinAmpMinusOne = fLinAmp - 1.f;
			AkReal32 fTwoSqrtATimesAlpha = 2.f*sqrtf(fLinAmp)*fAlpha;
			AkReal32 fLinAmpMinusOneTimesCosOmega = fLinAmpMinusOne*fCosOmega;
			AkReal32 fLinAmpPlusOneTimesCosOmega = fLinAmpPlusOne*fCosOmega;

			fb0 = fLinAmp*(fLinAmpPlusOne - fLinAmpMinusOneTimesCosOmega + fTwoSqrtATimesAlpha);
			fb1 = 2.f*fLinAmp*(fLinAmpMinusOne - fLinAmpPlusOneTimesCosOmega);
			fb2 = fLinAmp*(fLinAmpPlusOne - fLinAmpMinusOneTimesCosOmega - fTwoSqrtATimesAlpha);
			fa0 = fLinAmpPlusOne + fLinAmpMinusOneTimesCosOmega + fTwoSqrtATimesAlpha;
			fa1 = -2.f*(fLinAmpMinusOne + fLinAmpPlusOneTimesCosOmega);
			fa2 = fLinAmpPlusOne + fLinAmpMinusOneTimesCosOmega - fTwoSqrtATimesAlpha;
		}
		else if (in_eFilterType == FilterType_LowShelfQ)
		{
#ifdef AKSIMD_FAST_EXP_IMPLEMENTED
			AkReal32 fLinAmp = AKSIMD_POW10_SCALAR(in_fGain*0.025f);
#else
			AkReal32 fLinAmp = AkMath::FastPow10(in_fGain*0.025f);
#endif
			AkReal32 fOmega = 2.f * PI * fFrequency / in_fSampleRate;	// Normalized angular frequency
			AkReal32 fAlpha = sinf(fOmega) / (2.f*in_fQ);
			AkReal32 fCosOmega = cosf(fOmega);
			AkReal32 fLinAmpPlusOne = fLinAmp + 1.f;
			AkReal32 fLinAmpMinusOne = fLinAmp - 1.f;
			AkReal32 fTwoSqrtATimesAlpha = 2.f*sqrtf(fLinAmp)*fAlpha;
			AkReal32 fLinAmpMinusOneTimesCosOmega = fLinAmpMinusOne*fCosOmega;
			AkReal32 fLinAmpPlusOneTimesCosOmega = fLinAmpPlusOne*fCosOmega;

			fb0 = fLinAmp*(fLinAmpPlusOne - fLinAmpMinusOneTimesCosOmega + fTwoSqrtATimesAlpha);
			fb1 = 2.f*fLinAmp*(fLinAmpMinusOne - fLinAmpPlusOneTimesCosOmega);
			fb2 = fLinAmp*(fLinAmpPlusOne - fLinAmpMinusOneTimesCosOmega - fTwoSqrtATimesAlpha);
			fa0 = fLinAmpPlusOne + fLinAmpMinusOneTimesCosOmega + fTwoSqrtATimesAlpha;
			fa1 = -2.f*(fLinAmpMinusOne + fLinAmpPlusOneTimesCosOmega);
			fa2 = fLinAmpPlusOne + fLinAmpMinusOneTimesCosOmega - fTwoSqrtATimesAlpha;
		}
		else if(in_eFilterType == FilterType_HighShelf)
		{
#ifdef AKSIMD_FAST_EXP_IMPLEMENTED
			AkReal32 fLinAmp = AKSIMD_POW10_SCALAR(in_fGain*0.025f);
#else
			AkReal32 fLinAmp = AkMath::FastPow10(in_fGain*0.025f);
#endif
			AkReal32 fOmega = 2.f * PI * fFrequency / in_fSampleRate;	// Normalized angular frequency
			AkReal32 fAlpha = sinf(fOmega) / 2.f * sqrtf((fLinAmp + 1.f / fLinAmp)*(1.f / SHELFSLOPE - 1.f) + 2.f);
			AkReal32 fCosOmega = cosf(fOmega);
			AkReal32 fLinAmpPlusOne = fLinAmp + 1.f;
			AkReal32 fLinAmpMinusOne = fLinAmp - 1.f;
			AkReal32 fTwoSqrtATimesAlpha = 2.f*sqrtf(fLinAmp)*fAlpha;
			AkReal32 fLinAmpMinusOneTimesCosOmega = fLinAmpMinusOne*fCosOmega;
			AkReal32 fLinAmpPlusOneTimesCosOmega = fLinAmpPlusOne*fCosOmega;

			fb0 = fLinAmp*(fLinAmpPlusOne + fLinAmpMinusOneTimesCosOmega + fTwoSqrtATimesAlpha);
			fb1 = -2.f*fLinAmp*(fLinAmpMinusOne + fLinAmpPlusOneTimesCosOmega);
			fb2 = fLinAmp*(fLinAmpPlusOne + fLinAmpMinusOneTimesCosOmega - fTwoSqrtATimesAlpha);
			fa0 = fLinAmpPlusOne - fLinAmpMinusOneTimesCosOmega + fTwoSqrtATimesAlpha;
			fa1 = 2.f*(fLinAmpMinusOne - fLinAmpPlusOneTimesCosOmega);
			fa2 = fLinAmpPlusOne - fLinAmpMinusOneTimesCosOmega - fTwoSqrtATimesAlpha;
		}
		else if (in_eFilterType == FilterType_HighShelfQ)
		{
#ifdef AKSIMD_FAST_EXP_IMPLEMENTED
			AkReal32 fLinAmp = AKSIMD_POW10_SCALAR(in_fGain*0.025f);
#else
			AkReal32 fLinAmp = AkMath::FastPow10(in_fGain*0.025f);
#endif
			AkReal32 fOmega = 2.f * PI * fFrequency / in_fSampleRate;	// Normalized angular frequency
			AkReal32 fAlpha = sinf(fOmega) / (2.f*in_fQ);
			AkReal32 fCosOmega = cosf(fOmega);
			AkReal32 fLinAmpPlusOne = fLinAmp+1.f;
			AkReal32 fLinAmpMinusOne = fLinAmp-1.f;
			AkReal32 fTwoSqrtATimesAlpha = 2.f*sqrtf(fLinAmp)*fAlpha;
			AkReal32 fLinAmpMinusOneTimesCosOmega = fLinAmpMinusOne*fCosOmega;
			AkReal32 fLinAmpPlusOneTimesCosOmega = fLinAmpPlusOne*fCosOmega;

			fb0 = fLinAmp*( fLinAmpPlusOne + fLinAmpMinusOneTimesCosOmega + fTwoSqrtATimesAlpha );
			fb1 = -2.f*fLinAmp*( fLinAmpMinusOne + fLinAmpPlusOneTimesCosOmega );
			fb2 = fLinAmp*( fLinAmpPlusOne + fLinAmpMinusOneTimesCosOmega - fTwoSqrtATimesAlpha );
			fa0 = fLinAmpPlusOne - fLinAmpMinusOneTimesCosOmega + fTwoSqrtATimesAlpha;
			fa1 = 2.f*( fLinAmpMinusOne - fLinAmpPlusOneTimesCosOmega );
			fa2 = fLinAmpPlusOne - fLinAmpMinusOneTimesCosOmega - fTwoSqrtATimesAlpha;
		}
		else if(in_eFilterType == FilterType_Peaking)
		{
			AkReal32 fOmega = 2.f * PI * fFrequency / in_fSampleRate;	// Normalized angular frequency
			AkReal32 fCosOmega = cosf(fOmega);											// Cos omega
#ifdef AKSIMD_FAST_EXP_IMPLEMENTED
			AkReal32 fLinAmp = AKSIMD_POW10_SCALAR(in_fGain*0.025f);
#else
			AkReal32 fLinAmp = AkMath::FastPow10(in_fGain*0.025f);
#endif
			// alpha = sin(w0)/(2*Q)
			AkReal32 fAlpha = sinf(fOmega) / (2.f*in_fQ);
			// Coefficient formulas
			fb0 = 1.f + fAlpha*fLinAmp;
			fb1 = -2.f*fCosOmega;
			fb2 = 1.f - fAlpha*fLinAmp;
			fa0 = 1.f + fAlpha / fLinAmp;
			fa1 = -2.f*fCosOmega;
			fa2 = 1.f - fAlpha / fLinAmp;
		}

		// Normalize all 6 coefficients into 5 and flip sign of recursive coefficients 
		// to add in difference equation instead
		// Note keep gain separate from fb0 coefficients to avoid 
		// recomputing coefficient on output volume changes
		out_fb0 = fb0 / fa0;
		out_fb1 = fb1 / fa0;
		out_fb2 = fb2 / fa0;
		out_fa1 = fa1 / fa0;
		out_fa2 = fa2 / fa0;		
	}


	struct SingleChannelPolicyScalar
	{
		AkForceInline bool Allocate(AK::IAkPluginMemAlloc * in_pAllocator, AkUInt32 in_uChannels) { return true; }
		AkForceInline void Free(AK::IAkPluginMemAlloc * in_pAllocator){};
		AkForceInline AkReal32* GetRawMemories() { return (AkReal32*)&m_Memories; }
		AkForceInline void GetMemories(AkUInt32 /*in_uChannel*/, AkReal32& fFFwd1, AkReal32& fFFwd2, AkReal32& fFFbk1, AkReal32& fFFbk2)
		{
			fFFwd1 = m_Memories.fFFwd1;
			fFFwd2 = m_Memories.fFFwd2;
			fFFbk1 = m_Memories.fFFbk1;
			fFFbk2 = m_Memories.fFFbk2;
		}

		AkForceInline void SetMemories(AkUInt32 /*in_uChannel*/, AkReal32& fFFwd1, AkReal32& fFFwd2, AkReal32& fFFbk1, AkReal32& fFFbk2)
		{
			m_Memories.fFFwd1 = fFFwd1;
			m_Memories.fFFwd2 = fFFwd2;
			m_Memories.fFFbk1 = fFFbk1;
			m_Memories.fFFbk2 = fFFbk2;
		}

		AkForceInline void Reset() { AkZeroMemSmall(&m_Memories, sizeof(Memories)); }
		AkForceInline bool IsInitialized(){ return true; }
		AkForceInline AkUInt32 NumChannels() { return 1; }
		
		Memories m_Memories;
	};

	struct MultiChannelPolicyScalar
	{
		AkForceInline MultiChannelPolicyScalar()
			: m_pMemories(NULL)
			, m_uChannels(0)
		{
		}
		AkForceInline ~MultiChannelPolicyScalar()
		{
			AKASSERT(m_pMemories == NULL);
		}

		AkForceInline bool Allocate(AK::IAkPluginMemAlloc * in_pAllocator, AkUInt32 in_uChannels)
		{
			m_uChannels = in_uChannels;

			//Compute the filter memories size.
			AkUInt32 uMemSize = in_uChannels * 4 * sizeof(AkReal32);			
			m_pMemories = (AkReal32*)AK_PLUGIN_ALLOC(in_pAllocator, uMemSize);
			if(m_pMemories)
			{
				AkZeroMemSmall(m_pMemories, uMemSize);
				return true;
			}
	
			return false;
		};

		AkForceInline void Free(AK::IAkPluginMemAlloc * in_pAllocator)
		{
			if(m_pMemories)
			{
				AK_PLUGIN_FREE(in_pAllocator, m_pMemories);
				m_pMemories = NULL;
			}
		};

		AkForceInline AkReal32* GetRawMemories() { return m_pMemories; }

		AkForceInline void GetMemories(AkUInt32 in_uChannel, AkReal32& fFFwd1, AkReal32& fFFwd2, AkReal32& fFFbk1, AkReal32& fFFbk2)
		{
			fFFwd1 = *(m_pMemories + in_uChannel * 4 + 0);
			fFFwd2 = *(m_pMemories + in_uChannel * 4 + 1);
			fFFbk1 = *(m_pMemories + in_uChannel * 4 + 2);
			fFFbk2 = *(m_pMemories + in_uChannel * 4 + 3);
		}

		AkForceInline void SetMemories(AkUInt32 in_uChannel, AkReal32& fFFwd1, AkReal32& fFFwd2, AkReal32& fFFbk1, AkReal32& fFFbk2)
		{
			*(m_pMemories + in_uChannel * 4 + 0) = fFFwd1;
			*(m_pMemories + in_uChannel * 4 + 1) = fFFwd2;
			*(m_pMemories + in_uChannel * 4 + 2) = fFFbk1;
			*(m_pMemories + in_uChannel * 4 + 3) = fFFbk2;
		}
		
		AkForceInline void Reset()
		{
			if(m_pMemories)
				AkZeroMemSmall(m_pMemories, m_uChannels*4*sizeof(AkReal32));
		}

		AkForceInline bool IsInitialized(){ return m_pMemories != NULL; }
		AkForceInline AkUInt32 NumChannels() { return m_uChannels; }

		AkReal32* m_pMemories;		
		AkUInt32 m_uChannels;
		BiquadCoefficients m_Coefs;
	};	

	struct InPlaceScalarPolicy
	{
		AkForceInline void WriteScalar(AkReal32* AK_RESTRICT in_pIn, AkReal32 in_val) { *in_pIn = in_val; }
		AkForceInline void AdvanceScalar(AkReal32* AK_RESTRICT & in_pIn) { in_pIn++; }
	};

	struct OutPlaceScalarPolicy
	{		
		AkForceInline void WriteScalar(AkReal32* AK_RESTRICT in_pIn, AkReal32 in_val) { *pOut = in_val; }
		AkForceInline void AdvanceScalar(AkReal32* AK_RESTRICT & in_pIn) { in_pIn++;  pOut++; }
		AkReal32* pOut;
	};

	template<class CHANNEL_POLICY>
	class BiquadFilterScalar
	{
	public:				
		AKRESULT Init(AK::IAkPluginMemAlloc * in_pAllocator, AkUInt32 in_iChannels, bool in_bUnused = true)
		{
			if(m_Memories.Allocate(in_pAllocator, in_iChannels))
				return AK_Success;

			return AK_InsufficientMemory;
		}

		void Term(AK::IAkPluginMemAlloc * in_pAllocator)
		{
			m_Memories.Free(in_pAllocator);
		}

		AkForceInline bool IsInitialized()
		{
			return m_Memories.IsInitialized();
		}

		AkForceInline void Reset()
		{
			m_Memories.Reset();
		}

		AkForceInline void ComputeCoefs(FilterType in_eFilterType,
			const AkReal32 in_fSampleRate,
			const AkReal32 in_fFreq,
			const AkReal32 in_fGain = 0.f,
			const AkReal32 in_fQ = 1.f)
		{
			AkReal32 fB0, fB1, fB2, fA1, fA2;			
			ComputeBiquadCoefs(fB0, fB1, fB2, fA1, fA2, in_eFilterType, in_fSampleRate, in_fFreq, in_fGain, in_fQ);
			SetCoefs(fB0, fB1, fB2, fA1, fA2);
		}

		AkForceInline void SetCoefs(
			AkReal32 in_fB0,
			AkReal32 in_fB1,
			AkReal32 in_fB2,
			AkReal32 in_fA1,
			AkReal32 in_fA2)
		{
			in_fA1 = -in_fA1;
			in_fA2 = -in_fA2;

			m_Coefs.fB0 = in_fB0;
			m_Coefs.fB1 = in_fB1;
			m_Coefs.fB2 = in_fB2;
			m_Coefs.fA1 = in_fA1;
			m_Coefs.fA2 = in_fA2;			
		}

		AkForceInline void GetMemories(AkUInt32 in_uChannel, AkReal32& fFFwd1, AkReal32& fFFwd2, AkReal32& fFFbk1, AkReal32& fFFbk2)
		{
			return m_Memories.GetMemories(in_uChannel, fFFwd1, fFFwd2, fFFbk1, fFFbk2);
		}

		AkForceInline AkReal32 ProcessSample(Memories & io_rMemories, AkReal32 in_fIn)
		{
			// Feedforward part 			
			AkReal32 fOut = in_fIn * m_Coefs.fB0;
			io_rMemories.fFFwd2 = io_rMemories.fFFwd2 * m_Coefs.fB2;
			fOut = fOut + io_rMemories.fFFwd2;
			io_rMemories.fFFwd2 = io_rMemories.fFFwd1;
			io_rMemories.fFFwd1 = io_rMemories.fFFwd1 * m_Coefs.fB1;
			fOut = fOut + io_rMemories.fFFwd1;
			io_rMemories.fFFwd1 = in_fIn;

			// Feedback part
			io_rMemories.fFFbk2 = io_rMemories.fFFbk2 * m_Coefs.fA2;
			fOut = fOut + io_rMemories.fFFbk2;
			io_rMemories.fFFbk2 = io_rMemories.fFFbk1;
			io_rMemories.fFFbk1 = io_rMemories.fFFbk1 * m_Coefs.fA1;
			fOut = fOut + io_rMemories.fFFbk1;
			io_rMemories.fFFbk1 = fOut;
			return fOut;
		}

		AkForceInline void ProcessBuffer(AkReal32 * io_pBuffer, const AkUInt32 in_uNumframes, const AkUInt32 in_uChannelStride)
		{
			InPlaceScalarPolicy irrelevent;
			for(AkUInt32 c = 0; c < m_Memories.NumChannels(); c++)
				ProcessBufferMono<InPlaceScalarPolicy>(io_pBuffer + c*in_uChannelStride, in_uNumframes, c, irrelevent);
		}

		AkForceInline void ProcessBuffer(AkReal32 * io_pBuffer, const AkUInt32 in_uNumframes, const AkUInt32 in_uChannelStride, AkReal32 *in_pOutput)
		{
			OutPlaceScalarPolicy output = { in_pOutput };
			for(AkUInt32 c = 0; c < m_Memories.NumChannels(); c++)
				ProcessBufferMono<OutPlaceScalarPolicy>(io_pBuffer + c*in_uChannelStride, in_uNumframes, c, output);
		}

		AkForceInline void ProcessBufferMono(AkReal32 * io_pBuffer, const AkUInt32 in_uNumframes, const AkUInt32 in_iChannel = 0)
		{	
			InPlaceScalarPolicy irrelevent;
			ProcessBufferMono<InPlaceScalarPolicy>(io_pBuffer, in_uNumframes, in_iChannel, irrelevent);
		}

		AkForceInline void ProcessBufferMono(AkReal32 * io_pBuffer, const AkUInt32 in_uNumframes, const AkUInt32 in_iChannel, AkReal32 *in_pOutput)
		{
			OutPlaceScalarPolicy output = { in_pOutput };
			ProcessBufferMono<OutPlaceScalarPolicy>(io_pBuffer, in_uNumframes, in_iChannel, output);
		}
		
		template<class OUTPUT_POLICY>
		AkForceInline void ProcessBufferMono(AkReal32 * io_pBuffer, AkUInt32 in_uNumframes, AkUInt32 in_iChannel, OUTPUT_POLICY &in_Output)
		{
			AkReal32 * AK_RESTRICT pfBuf = (AkReal32 *)io_pBuffer;
			const AkReal32 * pfEnd = io_pBuffer + in_uNumframes;

			AkReal32 *pMems = m_Memories.GetRawMemories();

			AkReal32 fFFwd1 = *(pMems + in_iChannel * 4 + 0);
			AkReal32 fFFwd2 = *(pMems + in_iChannel * 4 + 1);
			AkReal32 fFFbk1 = *(pMems + in_iChannel * 4 + 2);
			AkReal32 fFFbk2 = *(pMems + in_iChannel * 4 + 3);
		
			while(pfBuf < pfEnd)
			{
				// Feedforward part
				AkReal32 fOut = *pfBuf * m_Coefs.fB0;
				fFFwd2 = fFFwd2 * m_Coefs.fB2;
				fOut = fOut + fFFwd2;
				fFFwd2 = fFFwd1;
				fFFwd1 = fFFwd1 * m_Coefs.fB1;
				fOut = fOut + fFFwd1;
				fFFwd1 = *pfBuf;

				// Feedback part
				fFFbk2 = fFFbk2 * m_Coefs.fA2;
				fOut = fOut + fFFbk2;
				fFFbk2 = fFFbk1;
				fFFbk1 = fFFbk1 * m_Coefs.fA1;
				fOut = fOut + fFFbk1;
				fFFbk1 = fOut;

				in_Output.WriteScalar(pfBuf, fOut);
				in_Output.AdvanceScalar(pfBuf);				
			}

			RemoveDenormal(fFFbk1);
			RemoveDenormal(fFFbk2);

			*(pMems + in_iChannel * 4 + 0) = fFFwd1;
			*(pMems + in_iChannel * 4 + 1) = fFFwd2;
			*(pMems + in_iChannel * 4 + 2) = fFFbk1;
			*(pMems + in_iChannel * 4 + 3) = fFFbk2;
		}

		AkForceInline void SetMemories(AkUInt32 in_uChannel, AkReal32& fFFwd1, AkReal32& fFFwd2, AkReal32& fFFbk1, AkReal32& fFFbk2)
		{
			m_Memories.SetMemories(in_uChannel, fFFwd1, fFFwd2, fFFbk1, fFFbk2);
		}

	protected:
		CHANNEL_POLICY	m_Memories;
		BiquadCoefficients m_Coefs;
	};


#ifdef AKSIMD_V4F32_SUPPORTED
	struct SingleChannelPolicySIMD
	{
		AkForceInline AKRESULT Allocate(AK::IAkPluginMemAlloc * in_pAllocator, AkUInt32 in_uChannels, bool in_bMono) {return AK_Success;}
		AkForceInline void Free( AK::IAkPluginMemAlloc * in_pAllocator ){};
		AkForceInline AkReal32* GetRawMemories() {return (AkReal32*)&m_Memories;}
		AkForceInline void GetMemories(AkUInt32 /*in_uChannel*/, AkReal32& fFFwd1, AkReal32& fFFwd2, AkReal32& fFFbk1, AkReal32& fFFbk2) 
		{ 
			fFFwd1 = m_Memories.fFFwd1;
			fFFwd2 = m_Memories.fFFwd2;
			fFFbk1 = m_Memories.fFFbk1;
			fFFbk2 = m_Memories.fFFbk2;
		}

		AkForceInline void SetMemories(AkUInt32 /*in_uChannel*/, AkReal32& fFFwd1, AkReal32& fFFwd2, AkReal32& fFFbk1, AkReal32& fFFbk2)
		{
			m_Memories.fFFwd1 = fFFwd1;
			m_Memories.fFFwd2 = fFFwd2;
			m_Memories.fFFbk1 = fFFbk1;
			m_Memories.fFFbk2 = fFFbk2;
		}

		AkForceInline void Reset() {AkZeroMemSmall(&m_Memories, sizeof(Memories));}
		AkForceInline bool IsInitialized() const{ return true; }
		AkForceInline AkUInt32 NumChannels() const{ return 1; }
		AkForceInline bool MonoProcess() const { return true; }

		AkForceInline AkReal32* GetMonoCoefPtr()
		{
			return (AkReal32*)m_pCoefs;
		}
	
		AKSIMD_V4F32 m_pCoefs[8];
		Memories m_Memories;
	};	

	struct MultiChannelPolicySIMD
	{
		AkForceInline MultiChannelPolicySIMD()
			: m_pMemories(NULL)
			, m_pUnaligned(NULL)
			, m_pCoefs(NULL)
			, m_uSize(0)
			, m_uChannels(0)
			, m_bMonoProcess(false)
		{
		}
		AkForceInline ~MultiChannelPolicySIMD()
		{
			AKASSERT( m_pMemories == NULL );
		}
		
		AkForceInline AKRESULT Allocate(AK::IAkPluginMemAlloc * in_pAllocator, AkUInt32 in_uChannels, bool in_bMonoProcess) 
		{
			m_bMonoProcess = in_bMonoProcess;
			m_uChannels = in_uChannels;

			//Compute the filter memories size.
			//We can compute 4 channels at a time
			//Then compute 2 channels
			//Then 1.
			//The memories for the 4-channel need to be contiguous and aligned
			AkUInt32 uNumSIMDPasses = in_uChannels / 4;
			uNumSIMDPasses += (in_uChannels & 2) >> 1; //Remaining channels.
			AkUInt32 uSoloChannel = in_uChannels & 1;
			
			AkUInt32 uMemSize = (uNumSIMDPasses * 4) * sizeof(AKSIMD_V4F32) + uSoloChannel * 4 * sizeof(AkReal32);

			AkUInt32 uCoefSize;
			if(m_bMonoProcess)
				uCoefSize = 8 * sizeof(AKSIMD_V4F32);
			else
				uCoefSize = ((uSoloChannel ? 8 : 0) + ((in_uChannels & 2) ? 6 : 0)) * sizeof(AKSIMD_V4F32) + (in_uChannels >= 4 ? 5 : 0) * sizeof(AkReal32);

			m_uSize = uMemSize + uCoefSize + AK_SIMD_ALIGNMENT - 1;	//Need larger in order to align.

			//The layout of memory is then, for 8 channels, for example
			//[fFFwd1-0 fFFwd1-1 fFFwd1-2 fFFwd1-3] [fFFwd2-0 fFFwd2-1 fFFwd2-2 fFFwd2-3] [fFFbk1-0 fFFbk1-1 fFFbk1-2 fFFbk1-3] [fFFbk2-0 fFFbk2-1 fFFbk2-2 fFFbk2-3]
			//[fFFwd1-4 fFFwd1-5 fFFwd1-6 fFFwd1-7] [fFFwd2-4 fFFwd2-5 fFFwd2-6 fFFwd2-7] [fFFbk1-4 fFFbk1-5 fFFbk1-6 fFFbk1-7] [fFFbk2-4 fFFbk2-5 fFFbk2-6 fFFbk2-7]

			//For 6 channels
			//[fFFwd1-0 fFFwd1-1 fFFwd1-2 fFFwd1-3] [fFFwd2-0 fFFwd2-1 fFFwd2-2 fFFwd2-3] [fFFbk1-0 fFFbk1-1 fFFbk1-2 fFFbk1-3] [fFFbk2-0 fFFbk2-1 fFFbk2-2 fFFbk2-3]
			//[fFFwd1-4 fFFwd1-5 padding padding] [fFFwd2-4 fFFwd2-5 padding padding] [fFFbk1-4 fFFbk1-5 padding padding] [fFFbk2-4 fFFbk2-5 padding padding]

			//Coefficients are after.
									
			m_pUnaligned = (AkReal32*)AK_PLUGIN_ALLOC(in_pAllocator, m_uSize);
			if(m_pUnaligned)
			{
				AkZeroMemSmall(m_pUnaligned, m_uSize);
				m_pMemories = (AkReal32*)((((AkUIntPtr)m_pUnaligned) + (AK_SIMD_ALIGNMENT - 1)) & ~(AK_SIMD_ALIGNMENT - 1));
				m_uSize = uMemSize;
				m_pCoefs = m_pMemories + uMemSize / sizeof(AkReal32);
				return AK_Success;
			}

			return AK_InsufficientMemory;
		};

		AkForceInline void Free( AK::IAkPluginMemAlloc * in_pAllocator )
		{
			if(m_pUnaligned)
			{
				AK_PLUGIN_FREE(in_pAllocator, m_pUnaligned);
				m_pMemories = NULL;
				m_pUnaligned = NULL;
			}
		};

		AkForceInline AkReal32* GetRawMemories() { return m_pMemories; }

		AkForceInline void GetMemories(AkUInt32 in_uChannel, AkReal32& fFFwd1, AkReal32& fFFwd2, AkReal32& fFFbk1, AkReal32& fFFbk2)
		{
			AkUInt32 uV4Block = in_uChannel / 4;
			AkUInt32 uV4Mod = in_uChannel & 3;
			if(((m_uChannels - 1) == in_uChannel && (m_uChannels & 1)))
			{
				if(m_uChannels != 1 && uV4Mod > 1)
					uV4Block++;	//The memories of the last odd-channel are out of a v4 block.

				//Last channel, and odd.
				fFFwd1 = *(m_pMemories + uV4Block * 16 + 0);
				fFFwd2 = *(m_pMemories + uV4Block * 16 + 1);
				fFFbk1 = *(m_pMemories + uV4Block * 16 + 2);
				fFFbk2 = *(m_pMemories + uV4Block * 16 + 3);
			}
			else
			{
				fFFwd1 = *(m_pMemories + uV4Block * 16 + uV4Mod + 0);
				fFFwd2 = *(m_pMemories + uV4Block * 16 + uV4Mod + 4);
				fFFbk1 = *(m_pMemories + uV4Block * 16 + uV4Mod + 8);
				fFFbk2 = *(m_pMemories + uV4Block * 16 + uV4Mod + 12);
			}
		}

		AkForceInline void SetMemories(AkUInt32 in_uChannel, AkReal32& fFFwd1, AkReal32& fFFwd2, AkReal32& fFFbk1, AkReal32& fFFbk2)
		{
			AkUInt32 uV4Block = in_uChannel / 4;
			AkUInt32 uV4Mod = in_uChannel & 3;
			if(((m_uChannels - 1) == in_uChannel && (m_uChannels & 1)))
			{
				if(m_uChannels != 1 && uV4Mod > 1)
					uV4Block++;	//The memories of the last odd-channel are out of a v4 block.

				//Last channel, and odd.
				*(m_pMemories + uV4Block * 16 + 0) = fFFwd1;
				*(m_pMemories + uV4Block * 16 + 1) = fFFwd2;
				*(m_pMemories + uV4Block * 16 + 2) = fFFbk1;
				*(m_pMemories + uV4Block * 16 + 3) = fFFbk2;
			}
			else
			{
				*(m_pMemories + uV4Block * 16 + uV4Mod + 0) = fFFwd1;
				*(m_pMemories + uV4Block * 16 + uV4Mod + 4) = fFFwd2;
				*(m_pMemories + uV4Block * 16 + uV4Mod + 8) = fFFbk1;
				*(m_pMemories + uV4Block * 16 + uV4Mod + 12) = fFFbk2;
			}
		}

		AkForceInline AkReal32* GetMonoCoefPtr()
		{
			return m_pCoefs;
		}


		AkForceInline AkReal32* GetStereoCoefPtr()
		{
			return m_pCoefs + (m_uChannels & 1) * 8 * 4;	//Stereo coefficients are after the mono coefficients.  8 coeffs, in SIMD (4 floats)
		}

		AkForceInline AkReal32* GetQuadCoefPtr()
		{
			return GetStereoCoefPtr() + ((m_uChannels & 2) >> 1) * 6 * 4;  //Quad coefficients are after the Stereo coefficients.  6 coeffs, in SIMD (4 floats)
		}

		AkForceInline void Reset() 
		{
			if(m_pMemories)
				AkZeroMemSmall(m_pMemories, m_uSize);
		}

		AkForceInline bool IsInitialized() const {return m_pMemories != NULL;}

		AkForceInline AkUInt32  NumChannels() const { return m_uChannels; }
		AkForceInline bool MonoProcess() const { return m_bMonoProcess; }

		AkReal32* m_pMemories;
		AkReal32 *m_pUnaligned;
		AkReal32 *m_pCoefs;
		AkUInt32 m_uSize;
		AkUInt32 m_uChannels;		
		bool	 m_bMonoProcess;
	};

	template<class CHANNEL_POLICY>
	class BiquadFilter
	{
	public:			
		BiquadFilter()
		{		
		}

		AKRESULT Init( AK::IAkPluginMemAlloc * in_pAllocator, AkUInt32 in_iChannels, bool in_bMonoProcess = false)
		{		
			return m_Memories.Allocate(in_pAllocator, in_iChannels, in_bMonoProcess);
		}

		void Term( AK::IAkPluginMemAlloc * in_pAllocator )
		{
			m_Memories.Free( in_pAllocator );
		}

		AkForceInline bool IsInitialized()
		{
			return m_Memories.IsInitialized();
		}

		AkForceInline void Reset()
		{
			m_Memories.Reset();
		}		

		AkForceInline void ComputeCoefs(FilterType in_eFilterType,
			const AkReal32 in_fSampleRate,
			const AkReal32 in_fFreq,
			const AkReal32 in_fGain = 0.f,
			const AkReal32 in_fQ = 1.f)
		{
			AkReal32 fB0, fB1, fB2, fA1, fA2;
			ComputeBiquadCoefs(fB0, fB1, fB2, fA1, fA2, in_eFilterType, in_fSampleRate, in_fFreq, in_fGain, in_fQ);
			SetCoefs(fB0, fB1, fB2, fA1, fA2);
		}

		AkForceInline void SetCoefs(
			AkReal32 in_fB0,
			AkReal32 in_fB1,
			AkReal32 in_fB2,
			AkReal32 in_fA1,
			AkReal32 in_fA2)
		{
			in_fA1 = -in_fA1;
			in_fA2 = -in_fA2;

			AkReal32 *pCoefs = (AkReal32*)m_Memories.m_pCoefs;
			if(m_Memories.NumChannels() & 1 || m_Memories.MonoProcess())
			{
				//Mono SIMD process coefficients (8 terms)

				//First term
				pCoefs[0] = in_fB0;
				pCoefs[1] = in_fB0;
				pCoefs[2] = in_fB0;
				pCoefs[3] = in_fB0;
				pCoefs += 4;

				//Second term
				pCoefs[0] = 0.f;
				pCoefs[1] = 0.f;
				pCoefs[2] = 0.f;
				pCoefs[3] = in_fB1 + in_fA1*in_fB0;
				pCoefs += 4;

				//Third Term
				pCoefs[0] = 0.f;
				pCoefs[1] = 0.f;
				pCoefs[2] = in_fB1 + in_fA1*in_fB0;
				pCoefs[3] = in_fB2 + pCoefs[2] * in_fA1 + in_fB0*in_fA2;
				pCoefs += 4;

				//Fourth term
				pCoefs[0] = 0.f;
				pCoefs[1] = in_fB1 + in_fA1*in_fB0;
				pCoefs[2] = in_fB2 + pCoefs[1] * in_fA1 + in_fB0*in_fA2;
				pCoefs[3] = pCoefs[2] * in_fA1 + in_fA1*in_fA2*in_fB0 + in_fA2*in_fB1;
				pCoefs += 4;

				//vXPrev1 term
				pCoefs[0] = in_fB1;
				pCoefs[1] = in_fB2 + in_fB1*in_fA1;
				pCoefs[2] = in_fA1*(in_fB2 + in_fB1*in_fA1) + in_fB1*in_fA2;
				pCoefs[3] = in_fA1*pCoefs[2] + in_fA2*in_fB2 + in_fB1*in_fA2*in_fA1;
				pCoefs += 4;

				//vXPrev2
				pCoefs[0] = in_fB2;
				pCoefs[1] = in_fB2*in_fA1;
				pCoefs[2] = in_fB2*in_fA1*in_fA1 + in_fB2*in_fA2;
				pCoefs[3] = in_fB2*in_fA1*in_fA1*in_fA1 + 2 * in_fB2*in_fA2*in_fA1;
				pCoefs += 4;

				//vYPrev1 term
				pCoefs[0] = in_fA1;
				pCoefs[1] = in_fA1*in_fA1 + in_fA2;
				pCoefs[2] = in_fA1*in_fA1*in_fA1 + 2 * in_fA2*in_fA1;
				pCoefs[3] = in_fA1*in_fA1*in_fA1*in_fA1 + 3 * in_fA2*in_fA1*in_fA1 + in_fA2*in_fA2;
				pCoefs += 4;

				//vYPrev2 term
				pCoefs[0] = in_fA2;
				pCoefs[1] = in_fA2*in_fA1;
				pCoefs[2] = in_fA2*in_fA1*in_fA1 + in_fA2*in_fA2;
				pCoefs[3] = in_fA2*in_fA1*in_fA1*in_fA1 + 2 * in_fA2*in_fA2*in_fA1;
				pCoefs += 4;

				if(m_Memories.MonoProcess())
					return;
			}

			////// Stereo process coefficients ////
			if(m_Memories.NumChannels() & 2)
			{
				// y0a =			x0*B0 +				xm1*B1 +			xm2*B2 +		ym1*A1 +			ym2*A2
				// y1a = xp1*B0 +	x0*(B1 + B0*A1) +	xm1*(B2 + B1*A1) +	xm2*B2*A1 +		ym1*(A2 + A1*A1) +	ym2*A2*A1
				//First term
				pCoefs[0] = 0;
				pCoefs[1] = in_fB0;
				pCoefs[2] = 0;
				pCoefs[3] = in_fB0;
				pCoefs += 4;

				//Second term
				pCoefs[0] = in_fB0;
				pCoefs[1] = in_fB1 + in_fB0*in_fA1;
				pCoefs[2] = in_fB0;
				pCoefs[3] = in_fB1 + in_fB0*in_fA1;
				pCoefs += 4;

				//Third term
				pCoefs[0] = in_fB1;
				pCoefs[1] = in_fB2 + in_fB1*in_fA1;
				pCoefs[2] = in_fB1;
				pCoefs[3] = in_fB2 + in_fB1*in_fA1;
				pCoefs += 4;

				//Fourth term
				pCoefs[0] = in_fB2;
				pCoefs[1] = in_fB2*in_fA1;
				pCoefs[2] = in_fB2;
				pCoefs[3] = in_fB2*in_fA1;
				pCoefs += 4;

				//Fifth term
				pCoefs[0] = in_fA1;
				pCoefs[1] = in_fA2 + in_fA1*in_fA1;
				pCoefs[2] = in_fA1;
				pCoefs[3] = in_fA2 + in_fA1*in_fA1;
				pCoefs += 4;

				//Sixth term
				pCoefs[0] = in_fA2;
				pCoefs[1] = in_fA2*in_fA1;
				pCoefs[2] = in_fA2;
				pCoefs[3] = in_fA2*in_fA1;
				pCoefs += 4;
			}

			if(m_Memories.NumChannels() >= 4)
			{
				pCoefs[0] = in_fB0;
				pCoefs[1] = in_fB1;
				pCoefs[2] = in_fB2;
				pCoefs[3] = in_fA1;
				pCoefs[4] = in_fA2;
			}
		}
	
		AkForceInline void GetMemories(AkUInt32 in_uChannel, AkReal32& fFFwd1, AkReal32& fFFwd2, AkReal32& fFFbk1, AkReal32& fFFbk2) 
		{ 
			return m_Memories.GetMemories(in_uChannel, fFFwd1, fFFwd2, fFFbk1, fFFbk2);
		}

		AkForceInline void SetMemories(AkUInt32 in_uChannel, AkReal32& fFFwd1, AkReal32& fFFwd2, AkReal32& fFFbk1, AkReal32& fFFbk2)
		{
			m_Memories.SetMemories(in_uChannel, fFFwd1, fFFwd2, fFFbk1, fFFbk2);
		}

	protected:		
		CHANNEL_POLICY	m_Memories;

#include "./BiquadFilter.cpp"	//Inlined code.
	} AK_ALIGN_DMA;

#endif

#ifdef AKSIMD_V4F32_SUPPORTED
	typedef BiquadFilter<SingleChannelPolicySIMD> BiquadFilterMonoSIMD;
	typedef BiquadFilter<MultiChannelPolicySIMD> BiquadFilterMultiSIMD;
	typedef BiquadFilterScalar<SingleChannelPolicyScalar> BiquadFilterMonoPerSample;
	typedef BiquadFilterScalar<SingleChannelPolicyScalar> BiquadFilterMultiPerSample;
#else
	typedef BiquadFilterScalar<SingleChannelPolicyScalar> BiquadFilterMonoSIMD;
	typedef BiquadFilterScalar<MultiChannelPolicyScalar> BiquadFilterMultiSIMD;
	typedef BiquadFilterScalar<SingleChannelPolicyScalar> BiquadFilterMonoPerSample;
	typedef BiquadFilterScalar<MultiChannelPolicyScalar> BiquadFilterMultiPerSample;
#endif
} // namespace DSP

#endif // _AKBIQUADFILTER_H_
