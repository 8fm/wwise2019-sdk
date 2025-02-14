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

#pragma once

#include "AkEndianByteSwap.h"
#include "AkFileParserBase.h"
#include "WavAnalysisEngine.h"
#include "WavLoudnessAnalysis.h"
#include "AkPBI.h"
#include "AkAudioLib.h"

class AkPCMCrossfadeBuffer
{
public:
	AkPCMCrossfadeBuffer()
		: pData(NULL)
		, uMaxFrames(0)
		, uValidFrames(0)
		, eCrossfadeUpCurve(AkCurveInterpolation_Sine)
		, eCrossfadeDownCurve(AkCurveInterpolation_SineRecip)
		, iLoopCrossfadeDurSamples(0)
	{
	}

	void* GetInterleavedData() { return pData; }
	AkUInt32 MaxFrames() { return uMaxFrames; }

	bool Alloc(CAkPBI * in_pCtx, AkUInt32 in_uPCMLoopStart, AkUInt32 in_uPCMLoopEnd)
	{
		bool bSuccess = false;

		AKASSERT(pData == NULL);

		const AkAudioFormat& rFormatInfo = in_pCtx->GetMediaFormat();
		AkUInt32 uNumChannels = rFormatInfo.GetNumChannels();
		AkReal32 fSampleRate = (AkReal32)rFormatInfo.uSampleRate;

		AKASSERT(fSampleRate > 0.0);

		iLoopCrossfadeDurSamples = (AkUInt32)(in_pCtx->GetLoopCrossfadeSeconds()*fSampleRate);
		iLoopCrossfadeDurSamples = AkMin((AkUInt32)iLoopCrossfadeDurSamples, ((in_uPCMLoopEnd + 1) - in_uPCMLoopStart) / 2);

		if (iLoopCrossfadeDurSamples > 0)
		{
			AkUInt32 uFramesToAlloc = 2 * iLoopCrossfadeDurSamples;
			pData = (AkUInt8*)malloc(uFramesToAlloc*sizeof(AkInt16)*uNumChannels);
			if (pData)
			{
				uMaxFrames = uFramesToAlloc;
				uValidFrames = 0;

				bSuccess = true;
			}
		}

		in_pCtx->LoopCrossfadeCurveShape(eCrossfadeUpCurve, eCrossfadeDownCurve);

		return bSuccess;
	}

	void Free()
	{
		if (pData != NULL)
		{
			free(pData);
			pData = NULL;
			uMaxFrames = 0;
			uValidFrames = 0;
			iLoopCrossfadeDurSamples = 0;
		}
	}

	void *			pData;
	AkUInt32		uMaxFrames;
	AkUInt32		uValidFrames;

	AkCurveInterpolation eCrossfadeUpCurve;
	AkCurveInterpolation eCrossfadeDownCurve;
	AkInt32				 iLoopCrossfadeDurSamples;
};

namespace AK
{
	AkUInt32 DoFade(void* pBuf, AkUInt32 uValidFrames,
		AkUInt32 uBufStartOffset,
		AkUInt32 uNumChannels,
		AkReal32 fInitial,
		AkReal32 fTarget,
		AkReal32 fFadeStartSample,
		AkReal32 fFadeDuration,
		AkCurveInterpolation eFadeCurve);
	void ConvertOnTheFly(AkInt16 * pDstBuffer, void * pSrcBuffer, AkUInt32 uValidFrames, AkUInt32 uMaxFrames, AkUInt32 uBlockAlign, AkUInt32 uNumChannels, bool in_bFloat, bool in_bSwap);
	void RefreshAnalysisData(AK::WWISESOUNDENGINE_DLL::AnalysisInfo * in_pAnalysisInfo, CAkPBI * in_pCtx, AkChannelConfig in_config, AkSourceChannelOrdering in_ordering, AkFileParser::AnalysisData *& io_pAnalysisData, AkUInt32 & io_uLastEnvelopePtIdx);
}
