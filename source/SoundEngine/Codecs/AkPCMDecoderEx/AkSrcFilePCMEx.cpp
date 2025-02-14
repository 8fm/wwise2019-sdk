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
#include "AkSrcFilePCMEx.h"
#include "AkFileParserBase.h"
#include "AkMonitor.h"
#include "AkResamplerCommon.h"
#include "WavAnalysisEngine.h"
#include "WavLoudnessAnalysis.h"
#include <AKBase/AkWavStructs.h>

#include <AK/Plugin/AkPCMExFactory.h>

namespace AK
{
	namespace WWISESOUNDENGINE_DLL
	{
		extern GlobalAnalysisSet g_setAnalysis;
	}
}

// Plugin mechanism. Dynamic create function whose address must be registered to the FX manager.
AKPCMEXDECODER_API IAkSoftwareCodec* CreatePCMExFilePlugin(void* in_pCtx)
{
	return AkNew( AkMemID_Processing, CAkSrcFilePCMEx( (CAkPBI*)in_pCtx ) );
}

CAkSrcFilePCMEx::CAkSrcFilePCMEx( CAkPBI * in_pCtx )
	: CAkSrcFilePCM( in_pCtx )
	, m_pcmExState( State_Init )
	, m_pOutputBuffer( NULL )
	, m_uWAVFileSize( 0 )
	, m_uBlockAlign( 0 )
	, m_dataTypeID(AK_INT)
	, m_ordering(SourceChannelOrdering_Standard)
	, m_uEndFadeInSample(0)
	, m_uStartFadeOutSample( 0 )
	, m_eFadeInCurve( AkCurveInterpolation_Sine )
	, m_eFadeOutCurve( AkCurveInterpolation_Sine )
{	
	// For decoded sounds specifically. Fades and trims are already baked into the .decoded file. We need to avoid applying them at this point.
	const AkOSChar* pSrcFilename = m_pCtx->GetSrcTypeInfo()->GetFilename();
	size_t len = _tcslen(pSrcFilename);
	m_bProcessTrimAndFade = len < 8 ? true: _tcscmp(pSrcFilename + len - 8, _T(".decoded")) != 0;

	// Backup the source ID to get symmetric RegisterObserver/UnregisterObserver, since the Source ID can change
	m_uiSourceID = m_pCtx->GetSource()->GetSourceID();
}

CAkSrcFilePCMEx::~CAkSrcFilePCMEx()
{
	m_CrossfadeBuffer.Free();
	if (m_pOutputBuffer)
		free( m_pOutputBuffer );

	// Unregister from envelopes map if applicable.
	AK::WWISESOUNDENGINE_DLL::GlobalAnalysisSet::iterator it = AK::WWISESOUNDENGINE_DLL::g_setAnalysis.find(m_uiSourceID);
	if ( it != AK::WWISESOUNDENGINE_DLL::g_setAnalysis.end() )
	{
		(*it).second.UnregisterObserver( this );
	}
}

bool CAkSrcFilePCMEx::AllocCrossfadeBuffer()
{
	bool bSuccess = false;

	m_CrossfadeBuffer.Free();

	if (m_pCtx && DoLoop() && m_pCtx->GetLoopCrossfadeSeconds() > 0.0)
		bSuccess = m_CrossfadeBuffer.Alloc(m_pCtx, m_uPCMLoopStart, m_uPCMLoopEnd);

	return bSuccess;
}

void CAkSrcFilePCMEx::GetBuffer( AkVPLState & io_state )
{
	AkUInt32 bufferStartSample = m_uCurSample;
	CAkSrcFilePCM::GetBuffer( io_state );

	if ( io_state.result == AK_DataReady || io_state.result == AK_NoMoreData )
	{
		ConvertOnTheFlyWAV(m_pOutputBuffer, io_state.GetInterleavedData(), io_state.uValidFrames, io_state.MaxFrames(), m_uBlockAlign, io_state.GetChannelConfig(), m_ordering, m_dataTypeID == AK_FLOAT, false);
		
		// Swap output buffer.
		io_state.AttachInterleavedData(m_pOutputBuffer, io_state.MaxFrames(), io_state.uValidFrames, io_state.GetChannelConfig());

		if (m_bProcessTrimAndFade)
			ProcessFade(io_state, bufferStartSample);
		ProcessLoopEnd( io_state, bufferStartSample );
	}
}

void CAkSrcFilePCMEx::ProcessFade( AkVPLState & buffer, AkUInt32 in_uBufferStartSampleIdx )
{
	const AkAudioFormat& rFormatInfo = m_pCtx->GetMediaFormat();
	AKASSERT( rFormatInfo.uInterleaveID == AK_INTERLEAVED );

	AkInt32 uBufferStartSample = (AkInt32)in_uBufferStartSampleIdx;
	AkInt32 iBufferEndSampleIdx = uBufferStartSample + buffer.uValidFrames;

	if ( uBufferStartSample < (AkInt32)m_uEndFadeInSample )
	{
		DoFade( buffer.GetInterleavedData(), buffer.uValidFrames, 0, (AkUInt32) rFormatInfo.GetNumChannels(), 0.f, 1.f, (AkReal32) uBufferStartSample, (AkReal32) m_uEndFadeInSample, m_eFadeInCurve );
	}
	
	if ( iBufferEndSampleIdx >= (AkInt32)m_uStartFadeOutSample )
	{
		AkInt32 iOffset = AkMax( (int)m_uStartFadeOutSample - (int)uBufferStartSample, 0 );
		AkReal32 iFadeStartSample = (AkReal32) AkMax( ((int)uBufferStartSample - (int)m_uStartFadeOutSample), 0 );
		DoFade( buffer.GetInterleavedData(), buffer.uValidFrames, iOffset, rFormatInfo.GetNumChannels(), 1.f, 0.f, iFadeStartSample, (AkReal32) (m_uTotalSamples - m_uStartFadeOutSample), m_eFadeOutCurve );
	}
}


void CAkSrcFilePCMEx::ProcessLoopStart(AkUInt8 * in_pBuffer, AkUInt32 in_uBufferSize)
{
	//todo get the in_pBuffer position relative to the fil.
	AkUInt32 uBufferStartSampleIdx = (m_uPCMLoopStart - m_CrossfadeBuffer.iLoopCrossfadeDurSamples) + m_CrossfadeBuffer.uValidFrames;
	AkUInt32 uValidFrames = in_uBufferSize / m_uBlockAlign;

	AKASSERT(m_CrossfadeBuffer.GetInterleavedData() != NULL && m_CrossfadeBuffer.uValidFrames < m_CrossfadeBuffer.MaxFrames());

	AkInt32 iLoopSampleIdx = ((AkInt32)uBufferStartSampleIdx) - (AkInt32)m_uPCMLoopStart;

	AkUInt32 uNumChannels = m_pCtx->GetMediaFormat().GetNumChannels();

	AkUInt32 uOldValidFrames = m_CrossfadeBuffer.uValidFrames;

	{
		AkUInt32 uFramesToCopy = AkMin(uValidFrames, (AkUInt32)m_CrossfadeBuffer.MaxFrames() - (AkUInt32)m_CrossfadeBuffer.uValidFrames);
		AkInt16* pDst = (((AkInt16*)m_CrossfadeBuffer.GetInterleavedData()) + uNumChannels*m_CrossfadeBuffer.uValidFrames);
		ConvertOnTheFlyWAV(pDst, in_pBuffer, uFramesToCopy, uFramesToCopy, m_uBlockAlign, m_pCtx->GetMediaFormat().channelConfig, m_ordering, m_dataTypeID == AK_FLOAT, false);
		m_CrossfadeBuffer.uValidFrames += uFramesToCopy;
	}

	AkUInt32 uFadedFrames = 0;
	if (iLoopSampleIdx < 0)
	{
		AkInt32 iStartSample = uOldValidFrames;
		uFadedFrames += DoFade(m_CrossfadeBuffer.pData, m_CrossfadeBuffer.uValidFrames,
			uOldValidFrames, uNumChannels, 0.f, 1.f, (AkReal32)iStartSample,
			(AkReal32)m_CrossfadeBuffer.iLoopCrossfadeDurSamples, m_CrossfadeBuffer.eCrossfadeUpCurve);
		iLoopSampleIdx += uFadedFrames;
	}

	if (iLoopSampleIdx >= 0)
	{
		AkInt32 iStartSample = iLoopSampleIdx;
		uFadedFrames += DoFade(m_CrossfadeBuffer.pData, m_CrossfadeBuffer.uValidFrames,
			uOldValidFrames + uFadedFrames, uNumChannels, 1.f, 0.f,
			(AkReal32)iStartSample, (AkReal32)m_CrossfadeBuffer.iLoopCrossfadeDurSamples, m_CrossfadeBuffer.eCrossfadeDownCurve);
		iLoopSampleIdx += uFadedFrames;
	}
}

void CAkSrcFilePCMEx::ProcessLoopEnd( AkVPLState& io_rAudioBuffer, AkUInt32 in_uBufferStartSampleIdx  )
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
AkUInt16 CAkSrcFilePCMEx::GetBlockAlign() const
{
	return m_uBlockAlign;
}

AKRESULT CAkSrcFilePCMEx::ParseHeader_Init()
{
	// Read RIFF header. This should fit in the first buffer, otherwise granularity is set to a
	// ridiculous low size and we won't bother trying to support it.

	if ( ( sizeof( AkChunkHeader ) + sizeof( AkUInt32 ) ) > m_ulSizeLeft )
		return AK_InvalidFile; // Incomplete RIFF header

	AkChunkHeader * pChunkHeader = (AkChunkHeader *) m_pNextAddress;
	if ( pChunkHeader->ChunkId != RIFFChunkId )
		return AK_InvalidFile; // Unsupported format

	if ( *((AkUInt32 *) ( pChunkHeader + 1 ) ) != WAVEChunkId )
		return AK_InvalidFile; // Unsupported format

	AkStreamInfo info;
	m_pStream->GetInfo( info );

	m_uWAVFileSize = AkMin( (AkUInt32) info.uSize, pChunkHeader->dwChunkSize + sizeof( AkChunkHeader ) );

	ConsumeData( sizeof( AkChunkHeader ) + sizeof( AkUInt32 ) );

	m_pcmExState = State_ReadChunkHeader;
	m_uNumBytesBuffered = 0;

	return AK_Success;
}

AKRESULT CAkSrcFilePCMEx::ParseHeader_ReadChunkHeader( AkUInt8 * in_pBuffer )
{
	if ( m_uNumBytesBuffered < sizeof( AkChunkHeader ) )
	{
		AkUInt32 uCopySize = AkMin( m_ulSizeLeft, sizeof( AkChunkHeader ) - m_uNumBytesBuffered );
		if ( uCopySize )
		{
			AkMemCpy( (AkUInt8*) &m_chunkHeader + m_uNumBytesBuffered, m_pNextAddress, uCopySize ); 

			m_uNumBytesBuffered += uCopySize;
			ConsumeData( uCopySize );
		}
	}

	if ( m_uNumBytesBuffered == sizeof( AkChunkHeader ) )
	{

		// If the size of the chunk goes beyond the end of the file, the file is broken. Still,
		// we can salvage the playback if we've passed the format and data chunks.
		if ( ( (m_uWAVFileSize - ( m_ulFileOffset - m_ulSizeLeft )) < m_chunkHeader.dwChunkSize )
			&& ( m_chunkHeader.ChunkId != dataChunkId ) ) // WG-15898 Also support truncated data chunk
		{
			if ( m_uBlockAlign && m_uDataOffset ) // These variables are set when reading format and data chunk
				return ParseHeader_SetLoopPoints(in_pBuffer);
			else
				return AK_Fail;
		}

		if ( m_chunkHeader.ChunkId == fmtChunkId 
			|| m_chunkHeader.ChunkId == dataChunkId 
			|| m_chunkHeader.ChunkId == smplChunkId 
			|| m_chunkHeader.ChunkId == cueChunkId 
			|| m_chunkHeader.ChunkId == LISTChunkId
			|| m_chunkHeader.dwChunkSize <= m_ulSizeLeft )
		{
			m_pcmExState = State_ReadChunkData;
		}
		else
		{
			bool bEndOfStream = false;
			AkUInt64 uPosition = m_pStream->GetPosition( &bEndOfStream );
			uPosition += ( m_pNextAddress - in_pBuffer ); // adjust with intra-buffer position
			AkUInt32 uSeekTarget = (AkUInt32) uPosition + m_chunkHeader.dwChunkSize;

			if ( uSeekTarget >= ( m_uWAVFileSize - sizeof( AkChunkHeader ) ) )
				return ParseHeader_SetLoopPoints(in_pBuffer);

			// Set stream position now for next read.
			if ( SetStreamPosition( uSeekTarget ) != AK_Success )
				return AK_InvalidFile;

			if( m_chunkHeader.dwChunkSize & 1 )
				m_pcmExState = State_ReadChunkPadding;

			m_ulSizeLeft = 0;
			m_uNumBytesBuffered = 0;
			return AK_FormatNotReady;
		}
	}
	else 
	{
		AKASSERT( m_ulSizeLeft == 0 );
		return AK_FormatNotReady;
	}

	return AK_Success;
}

AKRESULT CAkSrcFilePCMEx::ParseHeader_ReadChunkData( AkUInt8 * in_pBuffer )
{
	if ( m_pStitchBuffer == NULL )
	{
		m_uNumBytesBuffered = 0;

		if ( m_chunkHeader.ChunkId == dataChunkId )
		{
			bool bEndOfStream = false;
			AkUInt64 uPosition = m_pStream->GetPosition( &bEndOfStream );

			m_uDataOffset = (AkUInt32) uPosition + (AkUInt32) ( m_pNextAddress - in_pBuffer );
			m_uDataSize = m_chunkHeader.dwChunkSize;

			// Clamp data size to file size in order to salvage a few more ill-formed files.
			AkStreamInfo info;
			m_pStream->GetInfo(info);
			if (info.uSize < ((AkUInt64)m_uDataOffset + m_uDataSize) && (info.uSize >= m_uDataOffset))
				m_uDataSize = (AkUInt32) info.uSize - m_uDataOffset;

			// Apply trim in, trim out
			if (m_bProcessTrimAndFade)
			{
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
			}
			
			if ( m_uDataSize == 0 )
				return AK_InvalidFile;

			//CAkSrcFileBase::ProcessFirstBuffer() will try and skip the header, and assert if it does not fit in the buffer.
			//	so we have to set the stream position and wait for the new data.
			AkUInt32 uSkipSize = m_uDataOffset - ((AkUInt32)uPosition + m_uiCorrection); //
			if (m_ulSizeLeft < uSkipSize || (m_uDataSize + m_uDataOffset) >= (m_uWAVFileSize - sizeof(AkChunkHeader)))
			{
				//Seek to get the pcm data
				return ParseHeader_SetLoopPoints(in_pBuffer);
			}
			else
			{
				//Chunks at the end of the file.

				AkUInt32 uSizeMove = m_uDataSize + m_uDataOffset;
				uSizeMove += m_uDataSize & 1; // WG-14791 Chunk data is 2-byte aligned

				// Set stream position now for next read.
				if ( SetStreamPosition( uSizeMove ) != AK_Success )
					return AK_InvalidFile;
				
				m_ulSizeLeft = 0;

				m_pcmExState = State_ReadChunkHeader;
				return AK_FormatNotReady;
			}
		}
	
		if ( m_chunkHeader.dwChunkSize )
		{
			m_pStitchBuffer = (AkUInt8*)AkAlloc( AkMemID_Processing, m_chunkHeader.dwChunkSize );
			if ( m_pStitchBuffer == NULL )
				return AK_InsufficientMemory;
		}
	}

	if ( m_uNumBytesBuffered < m_chunkHeader.dwChunkSize )
	{
		AkUInt32 uCopySize = AkMin( m_ulSizeLeft, m_chunkHeader.dwChunkSize - m_uNumBytesBuffered );
		if ( uCopySize )
		{
			AkMemCpy( m_pStitchBuffer + m_uNumBytesBuffered, m_pNextAddress, uCopySize ); 

			m_uNumBytesBuffered += uCopySize;
			ConsumeData( uCopySize );
		}
	}

	if ( m_uNumBytesBuffered == m_chunkHeader.dwChunkSize )
	{
		if ( m_pStitchBuffer )
		{
			switch ( m_chunkHeader.ChunkId ) 
			{
			case fmtChunkId:
				{
					WAVEFORMATEXTENSIBLE * pWFX = (WAVEFORMATEXTENSIBLE *) m_pStitchBuffer;

					AkUInt32 uSamplesPerSec = pWFX->Format.nSamplesPerSec;
					
					// Store the _original_ block align. If it is 24 bits, we set the PBI's format to 16 bits
					// and handle truncation in GetBuffer().
					// WG-38778: We encountered files with inconsistent headers: blockAlign < channels*bitsPerSample/8
					//           They will be corrected to the expected value. 
					//           It could be that block align is greater (eg. 24 bits being aligned to 32) so we'll leave those 
					//           alone, assuming the value is correct.
					m_uBlockAlign = AkMax(pWFX->Format.nBlockAlign, pWFX->Format.nChannels  * pWFX->Format.wBitsPerSample / 8);

					// Store the original data type
					if (pWFX->Format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT
						|| (pWFX->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE && pWFX->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
					{
						m_dataTypeID = AK_FLOAT;
					}
					else
					{
						m_dataTypeID = AK_INT;
					}
					
					AkChannelConfig channelConfig = AK::SetChannelConfigFromWAVEFORMATEXTENSIBLE( *pWFX );
					if ( !channelConfig.IsValid() )
						return AK_InvalidFile;

					AkInt64 iChannelConfigOverride = 0;
					AK::WWISESOUNDENGINE_DLL::GlobalAnalysisSet::iterator it = AK::WWISESOUNDENGINE_DLL::g_setAnalysis.find(m_uiSourceID);
					if (it != AK::WWISESOUNDENGINE_DLL::g_setAnalysis.end())
					{
						iChannelConfigOverride = it->second.GetChannelConfigOverride();
						if (iChannelConfigOverride)
						{
							AkChannelConfig channelOverride;
							channelOverride.Deserialize(AK_GET_CHANNELCONFIGOVERRIDE_CONFIG(iChannelConfigOverride));
							if (channelOverride.uNumChannels == channelConfig.uNumChannels)
							{
								channelConfig = channelOverride;
								m_ordering = AK_GET_CHANNELCONFIGOVERRIDE_ORDERING(iChannelConfigOverride);
							}
						}
					}

					if (iChannelConfigOverride == 0
						&& AkFileParser::IsFuMaCompatible(channelConfig))
					{
						m_ordering = SourceChannelOrdering_FuMa; // Detected Ambisonic files are always FuMa when 3rd order or less.
					}

					AkAudioFormat format = m_pCtx->GetMediaFormat();
					format.uSampleRate = uSamplesPerSec;
					format.channelConfig = channelConfig;
					format.uBitsPerSample = AkMin( 16, pWFX->Format.wBitsPerSample ); // If we're 24 bit, we will truncate to 16 bits in-place
					format.uBlockAlign = channelConfig.uNumChannels * format.uBitsPerSample / 8;
					format.uTypeID = AK_INT;		
					format.uInterleaveID = AK_INTERLEAVED;			
					m_pCtx->SetMediaFormat(format);
				}
				break;

			case analysisDataChunkId:
				{
					AkFileParser::AnalysisDataChunk analysisDataChunk;
					analysisDataChunk.uDataSize = m_chunkHeader.dwChunkSize;
					analysisDataChunk.pData = (AkFileParser::AnalysisData*)m_pStitchBuffer;
					CAkSrcFileBase::StoreAnalysisData( analysisDataChunk );
				}
				break;

			case smplChunkId:
				{
					AkUInt32 uNumLoops = reinterpret_cast<SampleChunk*>(m_pStitchBuffer)->dwSampleLoops;
					// Parse if there are loops in the chunk.
					if ( uNumLoops > 0 )
					{
						// Move parsing pointer past the SampleChunk structure.
						AkUInt8 * pThisParse = m_pStitchBuffer + sizeof(SampleChunk);

						// We should be at the first sample loop. Get relevant info.
						// Notes. 
						// - Only forward loops are supported. Taken for granted.
						// - Play count is ignored. Set within the Wwise.
						// - Fractions not supported. Ignored.
						// - In SoundForge, loop points are stored in sample frames. Always true?
						SampleLoop * pSampleLoop = reinterpret_cast<SampleLoop*>(pThisParse);
						m_uPCMLoopStart = pSampleLoop->dwStart;
						m_uPCMLoopEnd = pSampleLoop->dwEnd;
					}
				}
				break;

			case cueChunkId:
				{
					// Ignore second cue chunks
					if ( m_markers.m_pMarkers == NULL )
					{
						// Parse markers.
						// Note. Variable number of cue points. We need to access the memory manager
						// to dynamically allocate cue structures.
						// Prepare markers header.
						AkUInt32 uNumMarkers = *reinterpret_cast<AkUInt32*>(m_pStitchBuffer);

						if ( uNumMarkers > 0 )
						{
							// Allocate markers.
							AKRESULT eResult = m_markers.Allocate( uNumMarkers );
							if ( eResult != AK_Success )
								return eResult;

							AkUInt8 * pThisParse = m_pStitchBuffer + sizeof( AkUInt32 );

							// Get all the markers.
							AkUInt32 ulMarkerCount = 0;
							while ( ulMarkerCount < m_markers.Count() )
							{
								// Note. We do not support multiple data chunks (and never will).
								CuePoint * pCuePoint = reinterpret_cast<CuePoint*>(pThisParse);
								m_markers.m_pMarkers[ulMarkerCount].dwIdentifier = pCuePoint->dwIdentifier;
								m_markers.m_pMarkers[ulMarkerCount].dwPosition = pCuePoint->dwPosition;
								m_markers.m_pMarkers[ulMarkerCount].strLabel = NULL;
								pThisParse += sizeof(CuePoint);
								ulMarkerCount++;
							}
						}
					}
				}
				break;

			case LISTChunkId:
				{
					AkUInt8 * pChunkData = (AkUInt8 *) m_pStitchBuffer;
					AkUInt8 * pChunkEnd = pChunkData + m_chunkHeader.dwChunkSize;

					AkFourcc * pListFourcc = (AkFourcc *) pChunkData;
					pChunkData += sizeof( AkFourcc );

					if ( *pListFourcc != adtlChunkId )
						break;

					while ( pChunkData < pChunkEnd )
					{
						AkChunkHeader * pHdr = (AkChunkHeader *) pChunkData;
						pChunkData += sizeof( AkChunkHeader );

						if( pHdr->ChunkId == lablChunkId )
						{
							// Ignore label chunk if no cues were read, or if input arguments are NULL
							if ( m_markers.m_pMarkers != NULL )
							{
								LabelCuePoint * pCuePoint = (LabelCuePoint *) pChunkData;
								// Find corresponding cue point for this label
								AkUInt32 dwCuePointID = pCuePoint->dwCuePointID;
								AkUInt32 ulMarkerCount = 0;
								while ( ulMarkerCount < m_markers.Count() )
								{
									if( m_markers.m_pMarkers[ulMarkerCount].dwIdentifier == dwCuePointID )
									{
										// NOTE: We don't fail if we can't allocate marker. Just ignore.
										char* strFileLabel = pCuePoint->strLabel;
										AkUInt32 uStrSize = pHdr->dwChunkSize - sizeof( AkUInt32 );
										m_markers.SetLabel( ulMarkerCount, strFileLabel, uStrSize );
										break; //exit while loop
									}

									ulMarkerCount++;
								}
							}
						}

						pChunkData += pHdr->dwChunkSize;

						// WG-18021: consume padding if present
						if ( ( pHdr->dwChunkSize & 1 ) && ( *pChunkData == 0 ) )
							pChunkData += 1;
					}
				}
				break;
			}

			AkFree( AkMemID_Processing, m_pStitchBuffer );
			m_pStitchBuffer = NULL;
		}

		if( m_chunkHeader.dwChunkSize & 1 )
		{
			m_pcmExState = State_ReadChunkPadding;
			return AK_Success;
		}
		else
		{
			m_pcmExState = State_ReadChunkHeader;
			return ParseHeader_CheckForEOF( in_pBuffer );
		}
	}
	else 
	{
		AKASSERT( m_ulSizeLeft == 0 );
		return AK_FormatNotReady;
	}
}

AKRESULT CAkSrcFilePCMEx::ParseHeader_ReadChunkPadding( AkUInt8 * in_pBuffer )
{
	if ( m_ulSizeLeft == 0 )
	{
 	 	// WG-15950: handle the case where there _should_ be padding, but we reach the end of file
		// instead. reaching EOF at this time will trigger playback.

		ParseHeader_CheckForEOF( in_pBuffer );
		return AK_FormatNotReady;
	}

	//Fix alignment problems
	/*
	http://www.sonicspot.com/guide/wavefiles.html
	One tricky thing about RIFF file chunks is that they must be word aligned. This means that 
	their total size must be a multiple of 2 bytes (ie. 2, 4, 6, 8, and so on). If a chunk contains 
	an odd number of data bytes, causing it not to be word aligned, an extra padding byte with 
	a value of zero must follow the last data byte. This extra padding byte is not counted in 
	the chunk size, therefor a program must always word align a chunk headers size value in order 
	to calculate the offset of the following chunk.
	*/

	// WG-15292: only consume the padding if its value is zero. This takes care of the case of
	// wav files that do not follow the standard; for those, here is the first byte of the next
	// chunk header instead.
	if ( *m_pNextAddress == 0 )
		ConsumeData( 1 );

	m_pcmExState = State_ReadChunkHeader;
	return ParseHeader_CheckForEOF( in_pBuffer );
}

AKRESULT CAkSrcFilePCMEx::ParseHeader_CheckForEOF( AkUInt8 * in_pBuffer )
{
	m_uNumBytesBuffered = 0;

	// Check if we reached EOF

	bool bEndOfStream;
	AkUInt64 uPosition = m_pStream->GetPosition( &bEndOfStream ) + ( m_pNextAddress - in_pBuffer );
	if ( uPosition >= ( m_uWAVFileSize - sizeof( AkChunkHeader ) ) )
	{
		return ParseHeader_SetLoopPoints(in_pBuffer);
	}
	else
	{
		return AK_Success;
	}
}

AKRESULT CAkSrcFilePCMEx::ParseHeader_GotoDataChunkForPlayback()
{
	// Set stream position now for next read.
	if ( SetStreamPosition( m_uDataOffset ) != AK_Success )
		return AK_Fail;

	m_ulSizeLeft = 0;

	m_pcmExState = State_Play;
	return AK_FormatNotReady;
}


AKRESULT CAkSrcFilePCMEx::ParseHeader_SetLoopPoints(AkUInt8 * in_pBuffer)
{
	AkUInt32 uiBlockAlign = m_uBlockAlign;
	AKASSERT(uiBlockAlign > 0);

	m_uTotalSamples = m_uDataSize / uiBlockAlign;
	if (m_uTotalSamples == 0)
		return AK_InvalidFile;

	AkUInt32 ulEndOfData = m_uDataOffset + m_uDataSize;

	AkReal32 fSampleRate = (AkReal32)m_pCtx->GetMediaFormat().uSampleRate;

	//Use loop points from source properties. 
	m_uPCMLoopStart = AkMin(m_uTotalSamples - 1, (AkUInt32)m_pCtx->GetLoopStartOffsetFrames());
	m_uPCMLoopEnd = AkMax((AkInt32)m_uPCMLoopStart, (AkInt32)m_uTotalSamples - (AkInt32)m_pCtx->GetLoopEndOffsetFrames());

	m_ulLoopStart = m_uDataOffset + m_uPCMLoopStart*uiBlockAlign;
	m_ulLoopEnd = m_uDataOffset + m_uPCMLoopEnd*uiBlockAlign;

	m_uPCMLoopEnd -= 1; //must be inclusive

	//One more sanity check on the loop points
	if (m_uPCMLoopEnd <= m_uPCMLoopStart || m_uPCMLoopStart > m_uTotalSamples || m_uPCMLoopEnd >= m_uTotalSamples)
	{
		m_uPCMLoopStart = 0;
		m_uPCMLoopEnd = m_uTotalSamples-1;
		m_ulLoopStart = m_uDataOffset + m_uPCMLoopStart*uiBlockAlign;
		m_ulLoopEnd = m_uDataOffset + m_uTotalSamples*uiBlockAlign;
	}

	// Cache source fade settings in samples
	AkReal32 fBeginFadeOffsetSec = 0.f, fEndFadeOffsetSec = 0.f;
	m_pCtx->GetSourceFade(fBeginFadeOffsetSec, m_eFadeInCurve,
		fEndFadeOffsetSec, m_eFadeOutCurve);

	m_uEndFadeInSample = (AkUInt32)(fBeginFadeOffsetSec * fSampleRate);
	m_uStartFadeOutSample = m_uTotalSamples - (AkUInt32)(fEndFadeOffsetSec * fSampleRate);

	bool bEof;
	AkUInt32 uPosition = (AkUInt32) m_pStream->GetPosition(&bEof);

	if (AllocCrossfadeBuffer())
	{
		AkUInt32 uXFadeDataStart = m_ulLoopStart - (m_CrossfadeBuffer.iLoopCrossfadeDurSamples * uiBlockAlign);
		if (uPosition <= uXFadeDataStart && (uPosition + m_ulSizeLeft) > uXFadeDataStart)
		{
			AkUInt32 uOffset = (uXFadeDataStart - uPosition);
			AkUInt32 uBufferSize = (m_uCurFileOffset - uPosition) + m_ulSizeLeft;
			m_pcmExState = State_FillXFadeBuffer;
			return ParseHeader_FillXFadeBuffer(in_pBuffer + uOffset, uBufferSize - uOffset);
		}
		else
		{
			if (SetStreamPosition(uXFadeDataStart) != AK_Success)
				return AK_InvalidFile;

			m_ulSizeLeft = 0;
			m_uNumBytesBuffered = 0;
			m_pcmExState = State_FillXFadeBuffer;
			return AK_FormatNotReady;
		}
	}
	else
	{
		AkUInt32 uSkipSize = m_uDataOffset - ((AkUInt32)uPosition + m_uiCorrection); //
		if (m_ulSizeLeft < uSkipSize)
		{
			//Seek to get the pcm data
			return ParseHeader_GotoDataChunkForPlayback();
		}
		else //if ((m_uDataSize + m_uDataOffset) >= (m_uWAVFileSize - sizeof(AkChunkHeader)))
		{
			// Simulate a seek to here for the base class
			m_uiCorrection = (AkUInt32)( m_pNextAddress - in_pBuffer );
			m_ulSizeLeft += m_uiCorrection;
			m_ulFileOffset -= m_ulSizeLeft;
			m_pcmExState = State_Play;
			return AK_Success;
		}
	}
}

AKRESULT CAkSrcFilePCMEx::ParseHeader_FillXFadeBuffer(AkUInt8 * in_pBuffer, AkUInt32 in_uBufferSize)
{
	AkUInt8 * pBuffer = in_pBuffer;
	AkUInt32 uBufferRemaining = in_uBufferSize;

	if (m_pStitchBuffer != NULL && m_uNumBytesBuffered > 0)
	{
		AkUInt32 uToCopy = m_uBlockAlign - m_uNumBytesBuffered;
		memcpy(m_pStitchBuffer + m_uNumBytesBuffered, in_pBuffer, uToCopy);
		m_uNumBytesBuffered += uToCopy;

		pBuffer += uToCopy;
		uBufferRemaining -= uToCopy;

		ProcessLoopStart((AkUInt8 *)m_pStitchBuffer, m_uNumBytesBuffered);

		m_uNumBytesBuffered = 0;
	}

	ProcessLoopStart(pBuffer, uBufferRemaining);
	pBuffer += (uBufferRemaining / m_uBlockAlign) * m_uBlockAlign;
	uBufferRemaining = (uBufferRemaining % m_uBlockAlign);

	if (uBufferRemaining)
	{
		if (m_pStitchBuffer == NULL)
		{
			m_pStitchBuffer = (AkUInt8*)AkAlloc(AkMemID_Processing, m_uBlockAlign);
			if (m_pStitchBuffer == NULL)
				return AK_InsufficientMemory;
		}

		if (m_pStitchBuffer != NULL)
		{
			memcpy(m_pStitchBuffer, pBuffer, uBufferRemaining);
			m_uNumBytesBuffered = uBufferRemaining;
		}
	}

	if (m_CrossfadeBuffer.uValidFrames < m_CrossfadeBuffer.MaxFrames())
	{
		ConsumeData(m_ulSizeLeft);
		return AK_FormatNotReady;
	}
	else
	{
		if (m_pStitchBuffer != NULL)
		{
			AkFree(AkMemID_Processing, m_pStitchBuffer);
			m_pStitchBuffer = NULL;
		}

		return ParseHeader_GotoDataChunkForPlayback();
	}
}

AKRESULT CAkSrcFilePCMEx::ParseHeader(
	AkUInt8 * in_pBuffer	// Buffer to parse
	)
{
    AKASSERT( m_pStream );
	AKASSERT( in_pBuffer );

	if ( m_pcmExState != State_Play )
	{
		// Set next pointer.
		m_ulFileOffset += m_ulSizeLeft;
		m_pNextAddress = in_pBuffer + m_uiCorrection;

		// Update size left.
		m_ulSizeLeft -= m_uiCorrection;

		do
		{
			AKRESULT res;

			switch ( m_pcmExState )
			{
			case State_Init:
				res = ParseHeader_Init();
				break;

			case State_ReadChunkHeader:
				res = ParseHeader_ReadChunkHeader( in_pBuffer );
				break;

			case State_ReadChunkData:
				res = ParseHeader_ReadChunkData( in_pBuffer );
				break;

			case State_ReadChunkPadding:
				res = ParseHeader_ReadChunkPadding( in_pBuffer );
				break;
			
			case State_FillXFadeBuffer:
				res = ParseHeader_FillXFadeBuffer(in_pBuffer, m_ulSizeLeft);
				break;

			case State_Play:
				break;
			}

			if ( res == AK_InvalidFile )
				goto InvalidFile;
			else if ( res == AK_FormatNotReady )
				goto FormatNotReady;
			else if ( res != AK_Success )
				return res;
		}
		while ( m_pcmExState != State_Play );
	}

	AKASSERT( !m_pStitchBuffer );
	m_uNumBytesBuffered = 0;


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

	{
		const AkAudioFormat& rFormatInfo = m_pCtx->GetMediaFormat();
		m_pOutputBuffer = (AkInt16*)malloc(AK_NUM_VOICE_REFILL_FRAMES * rFormatInfo.GetNumChannels() * sizeof(AkInt16));
	}

	// Update stream buffering settings.
	// Cannot receive less than an whole sample frame.
	return m_pStream->SetMinimalBufferSize(m_uBlockAlign);

FormatNotReady:
	if ( m_ulSizeLeft == 0 )
	{
		m_pStream->ReleaseBuffer();
		m_pNextAddress = NULL;
	}
	return AK_FormatNotReady;

InvalidFile:
	MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_InvalidAudioFileHeader, m_pCtx );
	return AK_InvalidFile;
}

void CAkSrcFilePCMEx::OnAnalysisChanged( AK::WWISESOUNDENGINE_DLL::AnalysisInfo * in_pAnalysisInfo )
{
	RefreshAnalysisData(in_pAnalysisInfo, m_pCtx, m_pCtx->GetMediaFormat().channelConfig, m_ordering, m_pAnalysisData, m_uLastEnvelopePtIdx);

	// Original file; no downmix, unless ambisonics (FuMa/MaxN require scaling down).
	if (AkFileParser::IsFuMaCompatible(m_pCtx->GetMediaFormat().channelConfig)
		&& m_ordering == SourceChannelOrdering_FuMa)
	{
		m_pAnalysisData->fDownmixNormalizationGain = ROOTTWO;
	}
}
