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

#include "AkResamplingPhaseVocoder.h"
#include <AK/Tools/Common/AkAssert.h>
#include <AK/Tools/Common/AkObject.h>

namespace DSP
{

namespace BUTTERFLYSET_NAMESPACE
{
	CAkResamplingPhaseVocoder::CAkResamplingPhaseVocoder()
		: m_ResamplingInputAccumBuf( NULL )
	{
	}

	void CAkResamplingPhaseVocoder::ResetInputFill( )
	{
		if ( m_ResamplingInputAccumBuf )
		{
			for ( AkUInt32 i = 0; i < m_uNumChannels; i++ )
			{
				m_ResamplingInputAccumBuf[i].ForceFullBuffer( );
			}
		}
	}

	AKRESULT CAkResamplingPhaseVocoder::Init(	
		AK::IAkPluginMemAlloc *	in_pAllocator,	
		AkUInt32				in_uNumChannels,
		AkUInt32				in_uSampleRate,
		AkUInt32				in_uFFTSize,
		bool					in_bUseInputBuffer /* = false */
		)
	{
		AKRESULT res = CAkPhaseVocoder::Init( in_pAllocator, in_uNumChannels, in_uSampleRate, in_uFFTSize, in_bUseInputBuffer );
		if (res != AK_Success)
			return res;

		if ( in_uNumChannels )
		{
			m_ResamplingInputAccumBuf = (DSP::CAkResamplingCircularBuffer *) AK_PLUGIN_ALLOC( in_pAllocator, in_uNumChannels * sizeof( DSP::CAkResamplingCircularBuffer ) );
			if ( m_ResamplingInputAccumBuf == NULL )
				return AK_InsufficientMemory;
		}

		for ( AkUInt32 i = 0; i < m_uNumChannels; i++ )
		{	
			AkPlacementNew( m_ResamplingInputAccumBuf + i ) CAkResamplingCircularBuffer();
		}

		for ( AkUInt32 i = 0; i < m_uNumChannels; i++ )
		{	
			AKRESULT eResult = m_ResamplingInputAccumBuf[i].Init( in_pAllocator, m_uFFTSize+m_uFFTSize/4 );
			if ( eResult != AK_Success )
					return eResult;
		}
		return AK_Success;
	}

	void CAkResamplingPhaseVocoder::Term( AK::IAkPluginMemAlloc * in_pAllocator )
	{
		if ( m_ResamplingInputAccumBuf ) 
		{
			for ( AkUInt32 i = 0; i < m_uNumChannels; i++ )
			{
				m_ResamplingInputAccumBuf[i].Term( in_pAllocator );
				m_ResamplingInputAccumBuf[i].~CAkResamplingCircularBuffer();
			}

			AK_PLUGIN_FREE( in_pAllocator, m_ResamplingInputAccumBuf );
			m_ResamplingInputAccumBuf = NULL;
		}
		CAkPhaseVocoder::Term( in_pAllocator );
	}

	void CAkResamplingPhaseVocoder::Reset( )
	{
		CAkPhaseVocoder::Reset( );
		for ( AkUInt32 i = 0; i < m_uNumChannels; i++ )
			m_ResamplingInputAccumBuf[i].Reset();
	}

	// Phase vocoder which resamples the content of the input by a given factor, a time stretch of the inverse factor is applied to get pitch shift
	// Channels are processed sequentially by caller
	AKRESULT CAkResamplingPhaseVocoder::ProcessPitchChannel( 
		AkReal32 *		in_pfInBuf, 
		AkUInt32		in_uNumFrames,
		bool			in_bNoMoreData,
		AkUInt32		i, // Channel index
		AkReal32 *		io_pfOutBuf,
		AkReal32		in_fResamplingFactor,
		AkReal32 *		in_pfTempStorage	
		)
	{
		Channel & channel = m_pChannels[ i ];

		AkReal32 * pfTDWindowStorage = in_pfTempStorage;

		AKRESULT eState = AK_DataReady;
		const AkUInt32 uFFTSize = m_uFFTSize;
		AKASSERT( uFFTSize % PV_OVERLAP_FACTOR == 0 );
		const AkUInt32 uHopSize = uFFTSize / PV_OVERLAP_FACTOR;

		// Compute overall system gain and compensate for windowing induced gain
		// Window factor = WindowSize / WindowCummulativeSum
		// Analysis overlap gain = overlap factor / Window factor
		// Compensation gain = 1.f / Analysis overlap gain 
		const AkReal32 fAnalysisGain = (m_TimeWindow.GetCummulativeSum() * PV_OVERLAP_FACTOR) / (AkReal32)m_uFFTSize;
		const AkReal32 fCompGain = 1.f/fAnalysisGain;
		const AkReal32 fTSFactor = 1.f/in_fResamplingFactor;

			AkUInt32 uInputValidFrames = in_uNumFrames;
			AkUInt32 uOutputValidFrames = 0;
			AkReal32 fInterpPos = m_fInterpPos;
			AkUInt32 uInputFramesToDiscard = m_uInputFramesToDiscard;
			AkUInt32 uInOffset = 0;
			bool bInitPhases = m_bInitPhases; 

			AkReal32 * AK_RESTRICT pfInBuf = (AkReal32 * ) in_pfInBuf; 
			AkReal32 * AK_RESTRICT pfOutBuf = (AkReal32 * ) io_pfOutBuf;

			AkUInt32 uMasterBreaker = 0;
			
			while ( (uInputValidFrames > 0) || (uOutputValidFrames < in_uNumFrames) )
			{
				// Safety mechanism to avoid infinite loops in case things go horribly wrong. Should never happen.
				if ( uMasterBreaker >= 100 )
				{
					AKASSERT( false && "AkHarmonizer: Infinite loop condition detected." );
					uMasterBreaker = 0xFFFFFFFF;
					break;
				}
				uMasterBreaker++;

				// If there are frames remaining to discard, the input buffer should be empty
				AKASSERT( uInputFramesToDiscard == 0 || m_ResamplingInputAccumBuf[i].FramesReady() == 0 );
				AkUInt32 uFramesDiscarded = AkMin( uInputValidFrames, uInputFramesToDiscard );
				uInputFramesToDiscard -= uFramesDiscarded;
				uInputValidFrames -= uFramesDiscarded;
				uInOffset += uFramesDiscarded;
				AKASSERT( uInputFramesToDiscard == 0 );

				// Once we reached the position, we can start filling the accumulation buffer with enough data for FFTs
				AkUInt32 uFramesConsumed = 0;

				if ( pfInBuf != NULL && uInputValidFrames > 0 )
					uFramesConsumed = m_ResamplingInputAccumBuf[i].PushFrames( pfInBuf+uInOffset, uInputValidFrames, in_fResamplingFactor );

				uInputValidFrames -= (AkUInt16)uFramesConsumed;	
				uInOffset += uFramesConsumed;

				// Compute spectrum of previous FFT frame
				if ( !channel.m_FreqWindow[FREQWIN_PREV].IsReady() )
				{
					bool bSuccess = m_ResamplingInputAccumBuf[i].ReadFrameBlock( pfTDWindowStorage, uFFTSize, in_bNoMoreData );
					if ( bSuccess )
					{
						AkUInt32 uFrameAdvance = m_ResamplingInputAccumBuf[i].AdvanceFrames( uHopSize );
						AKASSERT( in_bNoMoreData || uFrameAdvance == uHopSize );
						m_TimeWindow.Apply( pfTDWindowStorage, uFFTSize );
						channel.m_FreqWindow[FREQWIN_PREV].Compute( pfTDWindowStorage, uFFTSize, m_pFFTState );
						channel.m_FreqWindow[FREQWIN_PREV].CartToPol();
					}
				}

				// Compute spectrum of next FFT frame
				if ( !channel.m_FreqWindow[FREQWIN_CUR].IsReady() )
				{
					bool bSuccess = m_ResamplingInputAccumBuf[i].ReadFrameBlock( pfTDWindowStorage, uFFTSize, in_bNoMoreData);
					if ( bSuccess )
					{
						AkUInt32 uFrameAdvance = m_ResamplingInputAccumBuf[i].AdvanceFrames( uHopSize );
						AKASSERT( in_bNoMoreData || uFrameAdvance == uHopSize );
						m_TimeWindow.Apply( pfTDWindowStorage, uFFTSize );
						channel.m_FreqWindow[FREQWIN_CUR].Compute( pfTDWindowStorage, uFFTSize, m_pFFTState );
						channel.m_FreqWindow[FREQWIN_CUR].CartToPol();				
					}
				}

				// Compute interpolated spectrum window for this synthesis frame
				if ( channel.m_FreqWindow[FREQWIN_CUR].IsReady() && 
					 channel.m_FreqWindow[FREQWIN_PREV].IsReady() && 
					 !channel.m_VocoderWindow.IsReady() )
				{
					channel.m_VocoderWindow.ComputeVocoderSpectrum( 
						(AkPolar *)channel.m_FreqWindow[FREQWIN_PREV].Get(), 
						(AkPolar *)channel.m_FreqWindow[FREQWIN_CUR].Get(),
						channel.m_pfPrevSynthesisPhase,
						uHopSize,
						fInterpPos,
						bInitPhases );
					bInitPhases = false;
				}

				// Output to OLA buffer when space is available and advance time
				if ( channel.m_VocoderWindow.IsReady() &&
					channel.m_OLAOutCircBuf.FramesEmpty() >= uFFTSize )
				{
					// IFFT
					channel.m_VocoderWindow.ConvertToTimeDomain( pfTDWindowStorage, uFFTSize, m_pIFFTState );
					// Apply synthesis window
					m_TimeWindow.Apply( pfTDWindowStorage, uFFTSize, fCompGain );
					// OLA
					channel.m_OLAOutCircBuf.PushOverlappedWindow( pfTDWindowStorage, uHopSize );	
									
					channel.m_VocoderWindow.SetReady( false ); // Even with same 2 spectral window, a new phase vocoded window is necessary

					fInterpPos += fTSFactor;
					// Advance spectral windows if necessary
					if ( fInterpPos >= 1.f )
					{
						channel.m_FreqWindow[FREQWIN_PREV].SetReady( false ); 
						AkReal32 fInterpPosIntegerPart = floorf(fInterpPos);
						AkUInt32 uNumHopSkip = (AkUInt32) fInterpPosIntegerPart;
						if ( uNumHopSkip >= 2 )
						{
							channel.m_FreqWindow[FREQWIN_CUR].SetReady( false ); // Time compression can skip such that both windows are invalidated
							// Time compression may require skipping over some data: 
							// 1) If already in input buffer, simply flush these frames
							// 2) Set the remainder of what needs to be skipped (but not yet cached in input buffer)
							// so that it can be discarded when new input arrives
							AkUInt32 uFramesToFlushFromInput = (uNumHopSkip-2)*uHopSize;
							AkUInt32 uFramesSkipped = m_ResamplingInputAccumBuf[i].AdvanceFrames( uFramesToFlushFromInput );
							AKASSERT( uInputFramesToDiscard == 0 );
							uInputFramesToDiscard = uFramesToFlushFromInput - uFramesSkipped; 
						}
						else
							channel.m_uFreqWindowIndex++;	// Past == Current, switch the 2 windows
						
						fInterpPos -= fInterpPosIntegerPart;
					}	
					AKASSERT( fInterpPos >= 0.f && fInterpPos < 1.f );
				}

				// Consume what is available from the OLA buffer to put in pipeline's output buffer
				bool bFlushingOLABuffer =	(in_bNoMoreData &&
											m_ResamplingInputAccumBuf[i].FramesReady() == 0 &&
											!channel.m_VocoderWindow.IsReady() &&
											channel.m_OLAOutCircBuf.FramesReady() == 0 );

				AkUInt32 uFramesProduced = channel.m_OLAOutCircBuf.PopFrames( 
					pfOutBuf+uOutputValidFrames, 
					in_uNumFrames - uOutputValidFrames,
					bFlushingOLABuffer 
					);
				uOutputValidFrames += (AkUInt16)uFramesProduced;

				if ( bFlushingOLABuffer && channel.m_OLAOutCircBuf.IsDoneTail() )
					eState = AK_NoMoreData;

			} // While

			// Post processing
			if ( i == (m_uNumChannels-1) )
			{
				m_fInterpPos = fInterpPos;
				m_uInputFramesToDiscard = uInputFramesToDiscard;
				m_bInitPhases = bInitPhases;
			}

		return eState;
	}

} // namespace BUTTERFLYSET_NAMESPACE

} // namespace DSP
