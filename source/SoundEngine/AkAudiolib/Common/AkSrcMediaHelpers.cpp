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

#include "stdafx.h"
#include "AkSrcMediaHelpers.h"
#include "AkSettings.h"

void AK::SrcMedia::Position::Init(State * pState, const Header &header, const CodecInfo &codec, AkUInt16 uLoopCnt)
{
	pState->uCurSample = 0;
	pState->uTotalSamples = codec.uTotalSamples;
	pState->uPCMLoopStart = header.uPCMLoopStart;
	pState->uPCMLoopEnd = header.uPCMLoopEnd;
	pState->uLoopCnt = uLoopCnt;
}

AKRESULT AK::SrcMedia::Position::Forward(State * pState, AkUInt32 in_uFrames, bool & out_bLoop)
{
	AKASSERT(pState->uCurSample + in_uFrames <= pState->uTotalSamples);
	out_bLoop = false;

	pState->uCurSample += in_uFrames;

	// Resolve looping or end of file.
	AKRESULT eResult = in_uFrames > 0 ? AK_DataReady : AK_NoDataReady;
	if (WillLoop(pState))
	{
		// Loop pending. 
		// Number of frames produced should have been truncated up to the loop end.
		AKASSERT(pState->uCurSample <= (pState->uPCMLoopEnd + 1));

		if (pState->uCurSample > pState->uPCMLoopEnd)
		{
			// Reached end of loop.
			pState->uCurSample = pState->uPCMLoopStart;
			if (pState->uLoopCnt > 1) --pState->uLoopCnt;
			out_bLoop = true;
		}
	}
	else
	{
		if (pState->uCurSample >= pState->uTotalSamples)
		{
			eResult = AK_NoMoreData;
		}
	}
	return eResult;
}

AkReal32 AK::SrcMedia::Duration(AkUInt32 uTotalSamples, AkUInt32 uSampleRate, AkUInt32 uPCMLoopStart, AkUInt32 uPCMLoopEnd, AkUInt16 uNumLoops)
{
	if (uNumLoops == 0)
		return 0;

	AkReal32 fTotalNumSamples = (AkReal32)uTotalSamples + (AkReal32)(uNumLoops - 1)*(uPCMLoopEnd + 1 - uPCMLoopStart);
	return (fTotalNumSamples * 1000.f) / (AkReal32)uSampleRate;		// mSec.
}

void AK::SrcMedia::AbsoluteToRelativeSourceOffset(
	AkUInt32 in_uAbsoluteSourcePosition,
	AkUInt32 in_uLoopStart,
	AkUInt32 in_uLoopEnd,
	AkUInt16 in_usLoopCount,
	AkUInt32 & out_uRelativeSourceOffset,
	AkUInt16 & out_uRemainingLoops)
{
	out_uRemainingLoops = in_usLoopCount;

	if (IS_LOOPING(in_usLoopCount)
		&& in_uAbsoluteSourcePosition > in_uLoopEnd // Strictly greater than, because loop end is _always inclusive_
		&& in_uLoopEnd > in_uLoopStart)
	{
		AkUInt32 uTotalMinusStart = in_uAbsoluteSourcePosition - in_uLoopStart;
		AkUInt32 uLoopRegionLength = (in_uLoopEnd - in_uLoopStart + 1);
		AkUInt32 uLoopCount = uTotalMinusStart / uLoopRegionLength;

		if (uLoopCount < out_uRemainingLoops || out_uRemainingLoops == 0)
		{
			out_uRemainingLoops = (out_uRemainingLoops == 0) ? 0 : out_uRemainingLoops - (AkUInt16)uLoopCount;
			out_uRelativeSourceOffset = (uTotalMinusStart % uLoopRegionLength) + in_uLoopStart;
		}
		else
		{
			// Passed loop region: clamp to total loop count and re-express postion according to it.
			AKASSERT(in_uAbsoluteSourcePosition >= (out_uRemainingLoops - 1) * uLoopRegionLength);
			out_uRelativeSourceOffset = in_uAbsoluteSourcePosition - (out_uRemainingLoops - 1)* uLoopRegionLength;
			out_uRemainingLoops = 1;
		}
	}
	else
	{
		out_uRelativeSourceOffset = in_uAbsoluteSourcePosition;
	}
}

AkReal32 AK::SrcMedia::EnvelopeAnalyzer::GetEnvelope(State* pState, AkFileParser::AnalysisData * in_pAnalysisData, AkUInt32 in_uCurSample)
{
	// Linear search, starting with last point visited.
	if (in_pAnalysisData && in_pAnalysisData->uNumEnvelopePoints > 0)
	{
		AKASSERT(pState->uLastEnvelopePtIdx < in_pAnalysisData->uNumEnvelopePoints);

		const AkUInt32 uNumEntries = in_pAnalysisData->uNumEnvelopePoints;
		AkUInt32 uCurEntry = pState->uLastEnvelopePtIdx;
		AkFileParser::EnvelopePoint * pPoint = in_pAnalysisData->arEnvelope + uCurEntry;
		AkUInt32 uPrevPosition = pPoint->uPosition;
		AkUInt16 uPrevAttenuation = pPoint->uAttenuation;
		AkUInt32 uNextEntry = uCurEntry + 1;

		do
		{
			while (uNextEntry < uNumEntries)
			{
				pPoint = in_pAnalysisData->arEnvelope + uNextEntry;
				AkUInt32 uNextPosition = pPoint->uPosition;
				if (in_uCurSample >= uPrevPosition
					&& in_uCurSample < uNextPosition)
				{
					// CurSample is in range [curEntry.position,nextEntry.position[
					pState->uLastEnvelopePtIdx = uCurEntry;

					// Linear interpolation.
					AkReal32 fPrevAttenuation = (AkReal32)uPrevAttenuation;
					AkReal32 fAttenuation = fPrevAttenuation + ((in_uCurSample - uPrevPosition) * ((AkReal32)pPoint->uAttenuation - fPrevAttenuation)) / (uNextPosition - uPrevPosition);

					// Normalize.
					AKASSERT(-fAttenuation <= in_pAnalysisData->fEnvelopePeak);
					return -fAttenuation - in_pAnalysisData->fEnvelopePeak;
				}
				uPrevPosition = uNextPosition;
				uPrevAttenuation = pPoint->uAttenuation;
				++uCurEntry;
				++uNextEntry;
			}

			// Last point?
			if (in_uCurSample >= pPoint->uPosition)
			{
				pState->uLastEnvelopePtIdx = uCurEntry;

				// Normalize.
				AKASSERT(-(AkReal32)pPoint->uAttenuation <= in_pAnalysisData->fEnvelopePeak);
				return -(AkReal32)pPoint->uAttenuation - in_pAnalysisData->fEnvelopePeak;
			}

			// Did not find point (must have looped). Restart from beginning.
			// Note: loop interpolation is not supported.
			uCurEntry = 0;
			pPoint = in_pAnalysisData->arEnvelope;
			uPrevPosition = pPoint->uPosition;
			uPrevAttenuation = pPoint->uAttenuation;
			uNextEntry = 1;
		} while (true);
	}
	return 0;
}

void AK::SrcMedia::ResamplingRamp::Init(State * pState, AkUInt32 uSrcSampleRate)
{
	pState->fSampleRateConvertRatio = (AkReal32)uSrcSampleRate / AK_CORE_SAMPLERATE;
	pState->fLastRatio = pState->fSampleRateConvertRatio; // Assume pitch 0 to start
	pState->fTargetRatio = pState->fLastRatio;
}


AKRESULT AK::SrcMedia::ConstantFrameSeekTable::Init(AkUInt32 in_uEntries, void *in_pSeekData, AkUInt16 in_uSamplesPerPacket)
{
	m_pSeekTable = (AkUInt16*)AkAlloc(AkMemID_Processing, in_uEntries * sizeof(AkUInt16));
	if (m_pSeekTable == nullptr)
		return AK_InsufficientMemory;

	m_uSeekTableLength = in_uEntries;
	m_uSamplesPerPacket = in_uSamplesPerPacket;

	memcpy(m_pSeekTable, in_pSeekData, in_uEntries * sizeof(AkUInt16));
	return AK_Success;
}

void AK::SrcMedia::ConstantFrameSeekTable::Term()
{
	if (m_pSeekTable)
		AkFree(AkMemID_Processing, m_pSeekTable);
	m_pSeekTable = nullptr;
	m_uSeekTableLength = 0;
}

AkUInt16 AK::SrcMedia::ConstantFrameSeekTable::GetPacketSize(AkUInt32 uPacketIndex)
{
	return m_pSeekTable[uPacketIndex];
}

AkUInt32 AK::SrcMedia::ConstantFrameSeekTable::GetPacketDataOffset(AkUInt32 uDesiredSample)
{
	// This is a verification, using the seek table, that the correct packet was passed to decode the current sample.
	AkUInt32 uPacketIndex = uDesiredSample / m_uSamplesPerPacket;
	AKASSERT(uPacketIndex < m_uSeekTableLength);
	AkUInt32 uOffset = 0;
	for (AkUInt32 i = 0; i < uPacketIndex; i++)
		uOffset += m_pSeekTable[i];
	return uOffset;
}

AkUInt32 AK::SrcMedia::ConstantFrameSeekTable::GetPacketDataOffsetFromIndex(AkUInt32 uPacketIndex)
{
	AKASSERT(uPacketIndex <= m_uSeekTableLength);
	AkUInt32 uOffset = 0;
	for (AkUInt32 i = 0; i < uPacketIndex; i++)
		uOffset += m_pSeekTable[i];
	return uOffset;
}
