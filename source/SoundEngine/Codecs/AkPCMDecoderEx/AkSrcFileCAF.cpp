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
#include "AkSrcFileCAF.h"
#include "AkFileParserBase.h"
#include "AkMonitor.h"
#include "AkResamplerCommon.h"
#include "WavAnalysisEngine.h"
#include "WavLoudnessAnalysis.h"
#include <AKBase/AkWavStructs.h>
#include "AkFXMemAlloc.h"

//#include "../../../../../Authoring/source/AKBase/AkCAFReader.cpp"

namespace AK
{
	namespace WWISESOUNDENGINE_DLL
	{
		extern GlobalAnalysisSet g_setAnalysis;
	}
}

// Plugin mechanism. Dynamic create function whose address must be registered to the FX manager.
IAkSoftwareCodec* CreateCAFFilePlugin( void* in_pCtx )
{
	return AkNew( AkMemID_Processing, CAkSrcFileCAF( (CAkPBI*)in_pCtx ) );
}

CAkSrcFileCAF::CAkSrcFileCAF( CAkPBI * in_pCtx )
	: CAkSrcFilePCM( in_pCtx )
	, m_reader(AkFXMemAlloc::GetLower())
	, m_pOutputBuffer(NULL)
	, m_pcmExState(State_Init)
	, m_uBlockAlign( 0 )
	, m_ordering(SourceChannelOrdering_Standard)
	, m_uEndFadeInSample( 0 )
	, m_uStartFadeOutSample( 0 )
	, m_eFadeInCurve( AkCurveInterpolation_Sine )
	, m_eFadeOutCurve( AkCurveInterpolation_Sine )
{
	// Backup the source ID to get symmetric RegisterObserver/UnregisterObserver, since the Source ID can change
	m_uiSourceID = m_pCtx->GetSource()->GetSourceID();
}

CAkSrcFileCAF::~CAkSrcFileCAF()
{
	m_CrossfadeBuffer.Free();
	if ( m_pOutputBuffer )
		free( m_pOutputBuffer );

	// Unregister from envelopes map if applicable.
	AK::WWISESOUNDENGINE_DLL::GlobalAnalysisSet::iterator it = AK::WWISESOUNDENGINE_DLL::g_setAnalysis.find(m_uiSourceID);
	if ( it != AK::WWISESOUNDENGINE_DLL::g_setAnalysis.end() )
	{
		(*it).second.UnregisterObserver( this );
	}
}

bool CAkSrcFileCAF::AllocCrossfadeBuffer()
{
	bool bSuccess = false;

	m_CrossfadeBuffer.Free();

	if ( m_pCtx && DoLoop() && m_pCtx->GetLoopCrossfadeSeconds() > 0.0 )
		bSuccess = m_CrossfadeBuffer.Alloc(m_pCtx, m_uPCMLoopStart, m_uPCMLoopEnd);

	return bSuccess;
}

void CAkSrcFileCAF::GetBuffer( AkVPLState & io_state )
{
	AkUInt32 bufferStartSample = m_uCurSample;
	CAkSrcFilePCM::GetBuffer( io_state );

	if ( io_state.result == AK_DataReady || io_state.result == AK_NoMoreData )
	{
		ConvertOnTheFlyCAF(m_pOutputBuffer, io_state.GetInterleavedData(), io_state.uValidFrames, io_state.MaxFrames(), m_uBlockAlign, io_state.GetChannelConfig(), m_reader);
		
		// Swap output buffer.
		io_state.AttachInterleavedData(m_pOutputBuffer, io_state.MaxFrames(), io_state.uValidFrames, io_state.GetChannelConfig());

		ProcessFade( io_state, bufferStartSample );
		ProcessLoopEnd( io_state, bufferStartSample );
	}
}

void CAkSrcFileCAF::ProcessFade( AkVPLState & buffer, AkUInt32 in_uBufferStartSampleIdx )
{
	const AkAudioFormat& rFormatInfo = m_pCtx->GetMediaFormat();
	AKASSERT( rFormatInfo.uInterleaveID == AK_INTERLEAVED );

	AkInt32 uBufferStartSample = (AkInt32)in_uBufferStartSampleIdx;
	AkInt32 iBufferEndSampleIdx = uBufferStartSample + buffer.uValidFrames;

	if ( uBufferStartSample < (AkInt32)m_uEndFadeInSample )
	{
		DoFade( buffer.GetInterleavedData(), buffer.uValidFrames, 0, rFormatInfo.GetNumChannels(), 0.f, 1.f, (AkReal32) uBufferStartSample, (AkReal32) m_uEndFadeInSample, m_eFadeInCurve );
	}
	
	if ( iBufferEndSampleIdx >= (AkInt32)m_uStartFadeOutSample )
	{
		AkInt32 iOffset = AkMax( (int)m_uStartFadeOutSample - (int)uBufferStartSample, 0 );
		AkReal32 iFadeStartSample = (AkReal32) AkMax( ((int)uBufferStartSample - (int)m_uStartFadeOutSample), 0 );
		DoFade( buffer.GetInterleavedData(), buffer.uValidFrames, iOffset, rFormatInfo.GetNumChannels(), 1.f, 0.f, iFadeStartSample, (AkReal32)(m_uTotalSamples - m_uStartFadeOutSample), m_eFadeOutCurve );
	}
}

void CAkSrcFileCAF::ProcessLoopEnd( AkVPLState& io_rAudioBuffer, AkUInt32 in_uBufferStartSampleIdx  )
{
	if (m_CrossfadeBuffer.GetInterleavedData() != NULL)
	{
		AkInt32 iLoopSampleIdx = ((AkInt32)in_uBufferStartSampleIdx) - (AkInt32)(m_uPCMLoopEnd+1);

		if ( (iLoopSampleIdx+(AkInt32)io_rAudioBuffer.uValidFrames) >= (AkInt32)(-m_CrossfadeBuffer.iLoopCrossfadeDurSamples) && iLoopSampleIdx < (AkInt32)(m_CrossfadeBuffer.iLoopCrossfadeDurSamples) )
		{
			const AkAudioFormat& rFormatInfo = m_pCtx->GetMediaFormat();
			AkUInt32 uNumChannels = rFormatInfo.GetNumChannels();

			AkUInt32 uAudioBufStart = AkMax( -m_CrossfadeBuffer.iLoopCrossfadeDurSamples - iLoopSampleIdx, 0 );
			AkInt32 iFadeStartIdx = iLoopSampleIdx + uAudioBufStart;
			
			AkUInt32 uFadedFrames = 0;
			iLoopSampleIdx += uAudioBufStart;
			if (iLoopSampleIdx < 0)
			{
				AkInt32 iStartSample = iLoopSampleIdx + m_CrossfadeBuffer.iLoopCrossfadeDurSamples;
				uFadedFrames += DoFade( io_rAudioBuffer.GetInterleavedData(), io_rAudioBuffer.uValidFrames, 
										uAudioBufStart, uNumChannels, 1.f, 0.f, (AkReal32)iStartSample, 
										(AkReal32)m_CrossfadeBuffer.iLoopCrossfadeDurSamples, m_CrossfadeBuffer.eCrossfadeDownCurve );
				iLoopSampleIdx += uFadedFrames;
			}
			
			if (iLoopSampleIdx >= 0)
			{
				AkInt32 iStartSample = iLoopSampleIdx;
				uFadedFrames += DoFade( io_rAudioBuffer.GetInterleavedData(), io_rAudioBuffer.uValidFrames, 
										uAudioBufStart+uFadedFrames, uNumChannels, 0.f, 1.f, (AkReal32)iStartSample, 
										(AkReal32)m_CrossfadeBuffer.iLoopCrossfadeDurSamples, m_CrossfadeBuffer.eCrossfadeUpCurve );
				iLoopSampleIdx += uFadedFrames;
			}

			AkInt32 iFadeBufferConsumed = (iFadeStartIdx + m_CrossfadeBuffer.iLoopCrossfadeDurSamples);
			AKASSERT( iFadeBufferConsumed <= (int) m_CrossfadeBuffer.MaxFrames() );
			
			{	
				AkInt16* pSrc = ((AkInt16*)m_CrossfadeBuffer.GetInterleavedData()) + uNumChannels*iFadeBufferConsumed;
				AkInt16* pDst = ((AkInt16*)io_rAudioBuffer.GetInterleavedData()) + uNumChannels*uAudioBufStart;
				AkUInt32 uFrames = AkMin( (AkUInt32)io_rAudioBuffer.uValidFrames - uAudioBufStart, (AkUInt32)m_CrossfadeBuffer.MaxFrames() - iFadeBufferConsumed );
				
				while( uFrames-- )
				{	
					for ( int ch=0; ch < (int)uNumChannels; ++ch )
					{
						*pDst += *pSrc;

						pDst++;
						pSrc++;
					}
				}
			}

		}

	}
}


//////////////

// Returns block align. This function is virtual, in order for derived sources to manage their own 
// block align independently of that of the pipeline/PBI (read 24 bits).
AkUInt16 CAkSrcFileCAF::GetBlockAlign() const
{
	return m_uBlockAlign;
}

AKRESULT CAkSrcFileCAF::StartStream(AkUInt8 * in_pBuffer, AkUInt32 ulBufferSize)
{
	if (!m_reader.IsInitialized())
	{
		m_stream.Init(this);
		m_reader.Init(&m_stream);
	}

	return CAkSrcFilePCM::StartStream(in_pBuffer, ulBufferSize);
}

AKRESULT CAkSrcFileCAF::ParseHeader(
	AkUInt8 * in_pBuffer	// Buffer to parse
	)
{
	m_stream.SetBuffer(in_pBuffer);
	
	while (m_pcmExState != State_Play)
	{
		AKRESULT res;

		switch (m_pcmExState)
		{
		case State_Init:
			res = m_reader.ReadChunks();
			if (res == AK_Success)
			{
				AkUInt64 uDataPosition;
				AkUInt64 uDataSize;
				m_reader.GetDataLocation(uDataPosition, uDataSize);

				m_uDataOffset = (AkUInt32)uDataPosition;
				m_uDataSize = (AkUInt32)uDataSize;

				AkAudioFormat fmt = m_reader.GetFormat();

				// Store the _original_ block align. If it is 24 bits, we set the PBI's format to 16 bits
				// and handle truncation in GetBuffer().
				m_uBlockAlign = (AkUInt16)fmt.GetBlockAlign();

				fmt.uBitsPerSample = AkMin(16, fmt.uBitsPerSample); // If we're 24 bit, we will truncate to 16 bits in-place
				fmt.uBlockAlign = fmt.GetNumChannels() * fmt.uBitsPerSample / 8;

				AK::WWISESOUNDENGINE_DLL::GlobalAnalysisSet::iterator it = AK::WWISESOUNDENGINE_DLL::g_setAnalysis.find(m_uiSourceID);
				if (it != AK::WWISESOUNDENGINE_DLL::g_setAnalysis.end())
				{
					AkInt64 iChannelConfigOverride = it->second.GetChannelConfigOverride();
					if (iChannelConfigOverride)
					{
						AkChannelConfig channelOverride;
						channelOverride.Deserialize(AK_GET_CHANNELCONFIGOVERRIDE_CONFIG(iChannelConfigOverride));
						if (channelOverride.uNumChannels == fmt.GetNumChannels())
						{
							fmt.channelConfig = channelOverride;
							m_ordering = AK_GET_CHANNELCONFIGOVERRIDE_ORDERING(iChannelConfigOverride);
						}
					}
				}

				m_pCtx->SetMediaFormat(fmt);

				// Apply trim in, trim out
				AkReal32 fTrimStart = 0.0, fTrimEnd = 0.0;
				m_pCtx->GetTrimSeconds(fTrimStart, fTrimEnd);
				const AkAudioFormat& rFormat = m_pCtx->GetMediaFormat();

				//Notice we are not using the block align from the PBI, because if the file is 24 bit, it will incorrect.
				AKASSERT(m_uBlockAlign != 0);
				AkUInt32 uTrimOutDataOffset = ((AkUInt32)(fTrimEnd*(AkReal32)rFormat.uSampleRate - 0.5f)) * m_uBlockAlign;
				AkUInt32 uTrimInDataOffset = ((AkUInt32)(fTrimStart*(AkReal32)rFormat.uSampleRate + 0.5f)) * m_uBlockAlign;

				m_uDataOffset += uTrimInDataOffset;
				m_uDataSize -= uTrimInDataOffset;
				m_uDataSize -= uTrimOutDataOffset;

				if (m_uDataSize == 0)
					return AK_InvalidFile;

				m_uTotalSamples = (AkUInt32)(m_uDataSize / m_uBlockAlign);
				if (m_uTotalSamples == 0)
					return AK_InvalidFile;

				//Use loop points from source properties. 
				m_uPCMLoopStart = AkMin(m_uTotalSamples - 1, (AkUInt32)m_pCtx->GetLoopStartOffsetFrames());
				m_uPCMLoopEnd = AkMax((AkInt32)m_uPCMLoopStart, (AkInt32)m_uTotalSamples - (AkInt32)m_pCtx->GetLoopEndOffsetFrames());

				m_ulLoopStart = m_uDataOffset + m_uPCMLoopStart*m_uBlockAlign;
				m_ulLoopEnd = m_uDataOffset + m_uPCMLoopEnd*m_uBlockAlign;

				m_uPCMLoopEnd -= 1; //must be inclusive

				//One more sanity check on the loop points
				if (m_uPCMLoopEnd <= m_uPCMLoopStart || m_uPCMLoopStart > m_uTotalSamples || m_uPCMLoopEnd >= m_uTotalSamples)
					return AK_Fail;


				// Cache source fade settings in samples
				AkReal32 fBeginFadeOffsetSec = 0.f, fEndFadeOffsetSec = 0.f;
				m_pCtx->GetSourceFade(fBeginFadeOffsetSec, m_eFadeInCurve,
					fEndFadeOffsetSec, m_eFadeOutCurve);

				AkReal32 fSampleRate = (AkReal32)m_pCtx->GetMediaFormat().uSampleRate;
				m_uEndFadeInSample = (AkUInt32)(fBeginFadeOffsetSec * fSampleRate);
				m_uStartFadeOutSample = m_uTotalSamples - (AkUInt32)(fEndFadeOffsetSec * fSampleRate);

				if (AllocCrossfadeBuffer())
				{
					AkUInt32 uXFadeDataStart = m_ulLoopStart - (m_CrossfadeBuffer.iLoopCrossfadeDurSamples * m_uBlockAlign);
					if (m_stream.SetPosition(uXFadeDataStart) != AK_Success)
						return AK_Fail;

					if (!IsDataLeftInBuffer())
						res = AK_NoDataReady;

					m_pcmExState = State_FillXFadeBuffer;
				}
				else
				{
					if (m_stream.SetPosition(m_uDataOffset) != AK_Success)
						return AK_Fail;

					m_pcmExState = State_Play;

					if (!IsDataLeftInBuffer())
						res = AK_NoDataReady;
				}
			}
			break;

		case State_FillXFadeBuffer:
		{
			void * pBuffer = NULL;
			AkUInt32 uSizeRead = 0;
			AkUInt32 uFramesToCopy = 2 * m_CrossfadeBuffer.iLoopCrossfadeDurSamples;
			res = m_stream.Read(uFramesToCopy * m_uBlockAlign, pBuffer, uSizeRead);
			if (res == AK_DataReady)
			{
				{
					AKASSERT(m_CrossfadeBuffer.GetInterleavedData() != NULL && m_CrossfadeBuffer.uValidFrames < m_CrossfadeBuffer.MaxFrames());

					AkUInt32 uNumChannels = m_pCtx->GetMediaFormat().GetNumChannels();

					ConvertOnTheFlyCAF((AkInt16*)m_CrossfadeBuffer.GetInterleavedData(), pBuffer, uFramesToCopy, uFramesToCopy, m_uBlockAlign, m_pCtx->GetMediaFormat().channelConfig, m_reader);
					m_CrossfadeBuffer.uValidFrames = uFramesToCopy;

					DoFade(m_CrossfadeBuffer.pData, m_CrossfadeBuffer.uValidFrames,
						0, uNumChannels, 0.f, 1.f, 0.0f,
						(AkReal32)m_CrossfadeBuffer.iLoopCrossfadeDurSamples, m_CrossfadeBuffer.eCrossfadeUpCurve);

					DoFade(m_CrossfadeBuffer.pData, m_CrossfadeBuffer.uValidFrames,
						m_CrossfadeBuffer.iLoopCrossfadeDurSamples, uNumChannels, 1.f, 0.f, 0.0f,
						(AkReal32)m_CrossfadeBuffer.iLoopCrossfadeDurSamples, m_CrossfadeBuffer.eCrossfadeDownCurve);
				}

				if (m_stream.SetPosition(m_uDataOffset) != AK_Success)
					return AK_Fail;

				m_pcmExState = State_Play;

				if (!IsDataLeftInBuffer())
					res = AK_NoDataReady;
			}

			break;
		}

		case State_Play:
		default:
			res = AK_Success;
			break;
		}

		if (res == AK_NoDataReady)
		{
			if (m_ulSizeLeft == 0)
			{
				ReleaseStreamBuffer();
				m_pNextAddress = NULL;
			}
			return AK_FormatNotReady;
		}
		else if (res != AK_Success)
		{
			return res;
		}
	}

	// Clean up any left over in stitch buffer.
	if (m_pStitchBuffer)
	{
		AkFree(AkMemID_Processing, m_pStitchBuffer);
		m_pStitchBuffer = NULL;
		m_uNumBytesBuffered = 0;
	}

	// Simulate a seek to here for the base class
	m_uiCorrection = (AkUInt32)(m_pNextAddress - in_pBuffer);
	m_ulSizeLeft += m_uiCorrection;
	m_ulFileOffset -= m_ulSizeLeft;

	{
		// Find corresponding analysis info from global map and register to it.
		AK::WWISESOUNDENGINE_DLL::AnalysisInfo * pAnalysisInfo = NULL;
		AK::WWISESOUNDENGINE_DLL::GlobalAnalysisSet::iterator it = AK::WWISESOUNDENGINE_DLL::g_setAnalysis.find(m_uiSourceID);
		if ( it != AK::WWISESOUNDENGINE_DLL::g_setAnalysis.end() )
		{
			pAnalysisInfo = &((*it).second);
			pAnalysisInfo->RegisterObserver( this );
		}

		OnAnalysisChanged(pAnalysisInfo);
	}

	// Update stream heuristics.
	AkAutoStmHeuristics heuristics;
	m_pStream->GetHeuristics(heuristics);

	// Throughput.
	heuristics.fThroughput = (AkReal32)(m_uBlockAlign * m_pCtx->GetMediaFormat().uSampleRate) / 1000.f;

	// Looping.
	GetStreamLoopHeuristic(DoLoop(), heuristics);

	// Priority.
	heuristics.priority = m_pCtx->GetPriority();

	m_pStream->SetHeuristics(heuristics);

	const AkAudioFormat& rFormatInfo = m_pCtx->GetMediaFormat();
	m_pOutputBuffer = (AkInt16*)malloc(AK_NUM_VOICE_REFILL_FRAMES * rFormatInfo.GetNumChannels() * sizeof(AkInt16));

	// Update stream buffering settings.
	// Cannot receive less than an whole sample frame.
	return m_pStream->SetMinimalBufferSize(m_uBlockAlign);
}

void CAkSrcFileCAF::OnAnalysisChanged( AK::WWISESOUNDENGINE_DLL::AnalysisInfo * in_pAnalysisInfo )
{
	RefreshAnalysisData(in_pAnalysisInfo, m_pCtx, m_pCtx->GetMediaFormat().channelConfig, m_ordering, m_pAnalysisData, m_uLastEnvelopePtIdx);
}
