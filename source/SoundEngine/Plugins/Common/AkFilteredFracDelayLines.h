/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Version:  Build: 
Copyright (c) 2006-2019 Audiokinetic Inc.
*******************************************************************************/

#ifndef _AK_FILTEREDFRACDELYLINES_H_
#define _AK_FILTEREDFRACDELYLINES_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/Plugin/PluginServices/AkFXTailHandler.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/DSP/AkVariableDelayLineMemoryStream.h>

// log2(factor) * 1200
#define LOG_10_2 0.30102999566f
#define AK_FACTOR2PITCH( __Factor__ ) ((AkMath::Log10(__Factor__) / LOG_10_2) * 1200.f)

struct AkFilteredFracDelayLineParams
{
	AkReal32		fSpeedOfSound;
	AkReal32		fParamFilterCutoff;
	AkUInt32		uParamFilterIsFIR;
	AkReal32		fPitchThreshold;
	AkReal32		fDistanceThreshold;
	AkUInt32		uThresholdMode;
};

class CAkFilteredFracDelayLines {
public:
	CAkFilteredFracDelayLines() : m_pAllocator( NULL )
		, m_uMaxBufferLength( 0 )
		, m_fParamFilterb0(0.f)
		, m_resampledParamFIRCoeffs()
		, m_FIRparamFilterLen(0)
	{}

	~CAkFilteredFracDelayLines() {}

	void Init(
		AK::IAkPluginMemAlloc* in_pAllocator, 
		AkUInt16 in_uMaxBufferLength
		);

	void Term();

	void Reset();

	typedef AK::DSP::CAkVariableDelayLineMemory<AkReal32>::AkTap Tap;

	Tap * RegisterTap(
		AkReal32 delay
		);

	void UnregisterTap(
		CAkFilteredFracDelayLines::Tap & in_tap
		);

	AKRESULT Resize(
		AkReal32 in_fTargetDelayLen
		);

	void RefillDelayLine(
		AkSampleType * io_pfBuffer, 
		AkSampleType * in_pfBuffer, 
		AkUInt32 in_uNumFrames
		);
	
	void Mix(
		AkSampleType * io_pfBuffer, 
		AkSampleType * in_pfBuffer, 
		AkUInt32 in_uNumFrames
		);

	AKRESULT ProcessWrite(
		AkSampleType * in_pBuffer,
		AkUInt16 in_uNumFrames,
		bool bPurging = false
	);

	AKRESULT ProcessRead(
		AkAudioBuffer* out_pBuffer,
		AkUInt16 in_uNumFrames,
		AkUInt32 in_uSampleRate,
		AkReal32 in_fTargetDelayLen,
		bool bPurging,
		Tap & in_tap,
		AkFilteredFracDelayLineParams* in_FracDelayParams
	);

private:
	
	AkReal32 ProcessVariableDelayRead(
		AkSampleType* out_pBuffer,
		AkUInt16 in_uNumFrames,
		const AkReal32 in_fTargetDelayLen,
		AkReal32 currentDelay,
		AkReal32 fDelayGrowthSizeFrame
		);

	AkReal32 ProcessCrossfadeDelayRead(
		AkSampleType* out_pBuffer,
		AkUInt16 in_uNumFrames,
		const AkReal32 in_fTargetDelayLen,
		AkReal32 currentDelay
		);

	AkReal32 ProcessVolumeDuckingDelayRead(
		AkSampleType* out_pBuffer,
		AkUInt16 in_uNumFrames,
		const AkReal32 in_fTargetDelayLen,
		AkReal32 currentDelay,
		bool isMuting
		);
	
	AkReal32 ProcessCrossDuckingDelayRead(
		AkSampleType* out_pBuffer,
		AkUInt16 in_uNumFrames,
		const AkReal32 in_fTargetDelayLen,
		AkReal32 currentDelay
		);

	AkReal32 ProcessStationaryDelayRead(
		AkSampleType* out_pBuffer,
		AkUInt16 in_uNumFrames,
		const AkReal32 in_fTargetDelayLen
		);

	void EvaluateFIRParams( AkReal32& io_fParamFilterCutoff );
	void EvaluateIIRParams( AkReal32& io_fParamFilterCutoff, AkUInt16 in_uNumFrames, AkUInt32 in_uSampleRate );

	AkReal32 FilterParams(
		AkReal32 in_fParamFilterCutoff,
		const AkReal32 in_fTargetDelayLen,
		AkUInt32 in_uParamFilterIsFIR,
		Tap & in_tap);

	AkForceInline AkReal32 paramLowPassFilter( AkReal32 x, AkReal32& z )
	{
		// One pole low pass filter
		AkReal32 b0 = m_fParamFilterb0;
		AkReal32 a1 = 1.f - b0;
		return z = b0 * x + a1 * z;
	}

	AkForceInline AkReal32 paramLowPassFilter2( AkReal32 x, AkReal32* z )
	{
		const AkReal32* b = m_resampledParamFIRCoeffs;

		AkReal32 y = 0.f;
		for ( AkUInt32 i = m_FIRparamFilterLen - 2; i > 0; --i )
		{
			y += b[i + 1] * z[i];
			z[i] = z[i - 1];
		}
		y += b[0] * x;
		z[0] = x;

		return y;
	}

	AkForceInline void decimateFIRCoeffs( const AkReal32* in_firCoeffs, AkReal32* out_resampledCoeffs, AkUInt32 &io_uFIRlen, AkReal32 in_DecimateFactor )
	{
		AkReal32 fSum = 0.f;
		AkReal32 fIndex = 0.f;
		for ( AkUInt32 i = 0; i < io_uFIRlen-1; i++ )
		{
			AkUInt32 floorIndex = int( floor( fIndex ) );
			AkReal32 fraction = fIndex - (float)floorIndex;
			AkUInt32 ceilIndex = floorIndex + 1;
			out_resampledCoeffs[i] = in_DecimateFactor * ((1.f - fraction) * in_firCoeffs[floorIndex] + fraction * in_firCoeffs[ceilIndex]);
			fIndex += in_DecimateFactor;

			fSum += out_resampledCoeffs[i];
		}
		// Ensure the filter adds up to 1
		out_resampledCoeffs[io_uFIRlen-1] = 1.f - fSum;
	}

	AkForceInline AkReal32 linearInterp(AkReal32 *in_pfDelay, AkReal32 in_fIndex, AkUInt32 in_uMaxDelayLen)
	{
		AkReal32 fIndex = in_fIndex;

		// wrap around fIndex
		AK_FPSetValLT( fIndex, 0.f, fIndex, fIndex + (AkReal32)in_uMaxDelayLen );
		AK_FPSetValGTE( fIndex, (AkReal32)in_uMaxDelayLen, fIndex, fIndex - (AkReal32)in_uMaxDelayLen );

		AkSampleType values0 = 0.f;
		AkSampleType values1 = 0.f;
		AkSampleType weight0 = 0.f;
		AkSampleType weight1 = 0.f;

		// Linear interpolation between samples
		AKASSERT( fIndex >= 0.f && fIndex < in_uMaxDelayLen);

		AkUInt32 iIndex = (AkUInt32)floor( fIndex );

		weight0 = 1.0f - (AkSampleType)(fIndex - iIndex);
		weight1 = 1.0f - weight0;

		values0 = in_pfDelay[iIndex];

		iIndex++;
		if ( iIndex >= in_uMaxDelayLen )
			iIndex = 0;

		values1 = in_pfDelay[iIndex];

		return values0 * weight0 + values1 * weight1;
	}

	AK::IAkPluginMemAlloc* m_pAllocator;
	AK::DSP::CAkVariableDelayLineMemory<AkReal32> m_DelayMem;

	AkUInt16 m_uMaxBufferLength;
	AkReal32 m_fParamFilterb0;

	static const AkReal32 m_paramFIRCoeffs[AK_PARAM_FIR_ORDER];
	AkReal32 m_resampledParamFIRCoeffs[AK_PARAM_FIR_ORDER];
	AkUInt32 m_FIRparamFilterLen;
};

#endif // _AK_FILTEREDFRACDELYLINES_H_
