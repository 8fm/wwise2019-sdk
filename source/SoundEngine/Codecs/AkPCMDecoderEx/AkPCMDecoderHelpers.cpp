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

#include "stdafx.h"
#include "AkPCMDecoderHelpers.h"

AkUInt32 AK::DoFade(void* pBuf, AkUInt32 uValidFrames,
	AkUInt32 uBufStartOffset,
	AkUInt32 uNumChannels,
	AkReal32 fInitial,
	AkReal32 fTarget,
	AkReal32 fFadeStartSample,
	AkReal32 fFadeDuration,
	AkCurveInterpolation eFadeCurve)
{
	AkInt16* pSample = (AkInt16*)pBuf + uBufStartOffset*uNumChannels;
	AkUInt32 nNumOfFrame = AkClamp(uValidFrames - uBufStartOffset, 0, (AkUInt32)(fFadeDuration - fFadeStartSample));

	AkUInt32 nCount = nNumOfFrame;
	while (nCount--)
	{
		AkReal32 fRatio = fFadeStartSample / fFadeDuration;
		AkReal32 fGain = AkInterpolation::InterpolateNoCheck(fRatio, fInitial, fTarget, eFadeCurve);
		fGain = AkClamp(fGain, 0.f, 1.f);

		for (int ch = 0; ch < (int)uNumChannels; ++ch)
		{
			*pSample = (AkInt16)(*pSample * fGain);
			pSample++;
		}

		fFadeStartSample++;
	}

	return nNumOfFrame;
}

void AK::ConvertOnTheFly(AkInt16 * pDstBuffer, void * pSrcBuffer, AkUInt32 uValidFrames, AkUInt32 uMaxFrames, AkUInt32 uBlockAlign, AkUInt32 uNumChannels, bool in_bFloat, bool in_bSwap)
{
	// Cannot modify stream buffer because of caching.
	AkUInt32 uBytesPerSample = uBlockAlign / uNumChannels;
	if (uBytesPerSample == 4)
	{
		AkUInt32 uNumSamples = uValidFrames * uNumChannels;

		if (in_bFloat)
		{
			AkReal32 * pBuffer = (AkReal32 *)pSrcBuffer;

			if (in_bSwap)
			{
				for (AkUInt32 i = 0; i < uNumSamples; ++i)
				{
					AkUInt32 uSwapped = AK::EndianByteSwap::DWordSwap(*(AkUInt32*)(&(pBuffer[i])));
					pDstBuffer[i] = FLOAT_TO_INT16(*((float*)&uSwapped));
				}
			}
			else
			{
				for (AkUInt32 i = 0; i < uNumSamples; ++i)
				{
					pDstBuffer[i] = FLOAT_TO_INT16(pBuffer[i]);
				}
			}
		}
		else
		{
			AkInt32 * pBuffer = (AkInt32 *)pSrcBuffer;

			if (in_bSwap)
			{
				for (AkUInt32 i = 0; i < uNumSamples; ++i)
				{
					AkInt32 iSwapped = (AkInt32)AK::EndianByteSwap::DWordSwap((AkUInt32)pBuffer[i]);
					pDstBuffer[i] = (AkInt16)(iSwapped >> 16);
				}
			}
			else
			{
				for (AkUInt32 i = 0; i < uNumSamples; ++i)
				{
					pDstBuffer[i] = (AkInt16)(pBuffer[i] >> 16);
				}
			}
		}
	}
	else if (uBytesPerSample == 3) // 24 bit? truncate to 16 bit
	{
		AkUInt8 * pBuffer = (AkUInt8 *)pSrcBuffer;
		AkUInt32 uNumSamples = uValidFrames * uNumChannels;

		for (AkUInt32 i = 0; i < uNumSamples; ++i)
		{
			pDstBuffer[i] = *(AkInt16 *)(pBuffer + i * 3 + 1);
		}
	}
	else
	{
		AKASSERT(uBytesPerSample == 2);

		if (in_bSwap)
		{
			AkInt16 * pIn = (AkInt16 *)pSrcBuffer;
			for (unsigned int i = 0; i < uMaxFrames * uNumChannels; ++i)
			{
				pDstBuffer[i] = AK::EndianByteSwap::WordSwap(pIn[i]);
			}
		}
		else
		{
			memcpy(pDstBuffer, pSrcBuffer, uBlockAlign * uMaxFrames);
		}
	}
}

void AK::RefreshAnalysisData(AK::WWISESOUNDENGINE_DLL::AnalysisInfo * in_pAnalysisInfo, CAkPBI * in_pCtx, AkChannelConfig in_config, AkSourceChannelOrdering in_ordering, AkFileParser::AnalysisData *& io_pAnalysisData, AkUInt32 & io_uLastEnvelopePtIdx)
{
	// Destroy former analysis data chunk.
	if (io_pAnalysisData)
	{
		AkFree(AkMemID_Processing, io_pAnalysisData);
		io_pAnalysisData = NULL;
	}

	AkReal32 fLoudnessNormalizationGain = 1.f;
	AkUInt32 uNumEnvelopePoints = 0;

	// Loudness
	{
		// Get loudness directly from analysis engine.
		WavAnalysisEngine analysisEngine(WavAnalysisEngine::DataTypeMask_Loudness);
		AkSrcTypeInfo * pSrcType = in_pCtx->GetSrcTypeInfo();
		AKASSERT(pSrcType->GetFilename());
		analysisEngine.SetSource(pSrcType->GetFilename());
		analysisEngine.Load();

		IWavDataAnalysis * pAnalysis = analysisEngine.GetDataAnalysis(WavAnalysisEngine::DataType_Loudness);
		if (pAnalysis)
		{
			WavLoudnessAnalysis * pLoudnessAnalysis = ((WavLoudnessAnalysis*)pAnalysis);
			fLoudnessNormalizationGain = pLoudnessAnalysis->GetNormalizationGain(
				in_config,
				in_ordering,
				AK::SoundEngine::GetLoudnessFrequencyWeighting());
		}
	}

	// HDR
	if (in_pAnalysisInfo)
		uNumEnvelopePoints = in_pAnalysisInfo->GetNumPoints();

	// Allocate and write analysis chunk.

	size_t uAnalysisChunkSize =
		sizeof(AkReal32)		// normalization gain.
		+ sizeof(AkReal32)	// downmix normalization gain.
		+ sizeof(AkUInt32)	// num envelope points.
		+ sizeof(AkReal32)	// envelope peak.
		+ (uNumEnvelopePoints * sizeof(AkFileParser::EnvelopePoint));	// envelope.

	io_pAnalysisData = (AkFileParser::AnalysisData*)AkAlloc(AkMemID_Processing, uAnalysisChunkSize);
	if (io_pAnalysisData)
	{
		io_pAnalysisData->fLoudnessNormalizationGain = fLoudnessNormalizationGain;
		// Original file; no downmix, unless Ambisonics FuMa (require scaling down).
		io_pAnalysisData->fDownmixNormalizationGain = (in_ordering == SourceChannelOrdering_FuMa && AkFileParser::IsFuMaCompatible(in_config)) ? ROOTTWO : 1.f;
		io_pAnalysisData->uNumEnvelopePoints = uNumEnvelopePoints;
		io_pAnalysisData->fEnvelopePeak = 0;
		if (uNumEnvelopePoints > 0)
		{
			AKASSERT(in_pAnalysisInfo->GetEnvelope());
			memcpy((&io_pAnalysisData->arEnvelope), in_pAnalysisInfo->GetEnvelope(), sizeof(AkFileParser::EnvelopePoint) * uNumEnvelopePoints);
			io_pAnalysisData->fEnvelopePeak = in_pAnalysisInfo->GetMaxEnvValue();
		}
	}

	io_uLastEnvelopePtIdx = 0;
}
