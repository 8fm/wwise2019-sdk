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

#include "AkPhaseVocoder.h"
#include <AK/Tools/Common/AkAssert.h>
#include <AK/Tools/Common/AkObject.h>

namespace DSP
{

namespace BUTTERFLYSET_NAMESPACE
{

	CAkPhaseVocoder::CAkPhaseVocoder() 
		: 
#ifndef AK_VOICE_MAX_NUM_CHANNELS
	m_pChannels(NULL),
#endif
		  m_pFFTState(NULL)
		, m_pIFFTState(NULL)
		, m_uNumChannels( 0 )
	{
	}

	CAkPhaseVocoder::~CAkPhaseVocoder()
	{

	}

	AKRESULT CAkPhaseVocoder::Init(	
		AK::IAkPluginMemAlloc *	in_pAllocator,	
		AkUInt32				in_uNumChannels,
		AkUInt32				in_uSampleRate,
		AkUInt32				in_uFFTSize,
		bool					in_bUseInputBuffer /* = true */
		)
	{	
		AKASSERT( in_uFFTSize % PV_OVERLAP_FACTOR == 0 );

		m_uFFTSize = in_uFFTSize;
		m_uNumChannels = in_uNumChannels;
		m_uSampleRate = in_uSampleRate;

		// FFT configuration are the same for all channels and thus can be shared
		// Allocate FFT state
		RESOLVEUSEALLBUTTERFLIES( ak_fftr_alloc(m_uFFTSize, 0, NULL, &m_uFFTSpaceRequirements) );
		m_uFFTSpaceRequirements = AK_ALIGN_SIZE_FOR_DMA(m_uFFTSpaceRequirements);
		m_pFFTState = (ak_fftr_state *)AK_PLUGIN_ALLOC( in_pAllocator, m_uFFTSpaceRequirements );
		if ( m_pFFTState == NULL )
			return AK_InsufficientMemory;
		// Allocate iFFT state
		RESOLVEUSEALLBUTTERFLIES( ak_fftr_alloc(m_uFFTSize, 1, NULL, &m_uIFFTSpaceRequirements) );
		m_uIFFTSpaceRequirements = AK_ALIGN_SIZE_FOR_DMA(m_uIFFTSpaceRequirements);
		m_pIFFTState = (ak_fftr_state *)AK_PLUGIN_ALLOC( in_pAllocator, m_uIFFTSpaceRequirements );
		if ( m_pIFFTState == NULL )
			return AK_InsufficientMemory;

		// Configure  FFT and IFFT transforms
		// Note: These routines don't really allocate memory, they use the one allocated above
		RESOLVEUSEALLBUTTERFLIES( ak_fftr_alloc(m_uFFTSize,0,(void*)m_pFFTState,&m_uFFTSpaceRequirements) );
		RESOLVEUSEALLBUTTERFLIES( ak_fftr_alloc(m_uFFTSize,1,(void*)m_pIFFTState,&m_uIFFTSpaceRequirements) );
		AKASSERT( (m_pFFTState != NULL) && (m_pIFFTState != NULL) );

		// Setup windowing function
		AKRESULT eResult = m_TimeWindow.Init( in_pAllocator, m_uFFTSize, DSP::CAkTimeWindow::WINDOWTYPE_HANN, true );
		if ( eResult != AK_Success )
			return eResult;

		// Minimum supported buffering is m_uFFTSize for input and output OLA buffers
		// Additional buffering will give more flexibility to the algorithm to reduce the algorithm logic overhead 
		// On PS3 this may be significant as it will control the number of PPU/SPU roundtrips
		// Setup output circular buffer for overlapped time domain data
		m_bUseInputBuffer = in_bUseInputBuffer;

		// Setup past and current spectral window storage
		if ( m_uNumChannels )
		{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
			m_pChannels = (Channel *) AK_PLUGIN_ALLOC( in_pAllocator, m_uNumChannels * sizeof( Channel ) );
			if ( m_pChannels == NULL )
				return AK_InsufficientMemory;
			
			for ( AkUInt32 i = 0; i < m_uNumChannels; i++ )
			{
				AkPlacementNew( m_pChannels + i ) Channel();
			}
#endif
			
			for ( AkUInt32 i = 0; i < m_uNumChannels; i++ )
			{
				Channel & channel = m_pChannels[ i ];
				channel.m_pfPrevSynthesisPhase = NULL;
				channel.m_uFreqWindowIndex = 0;
				
				eResult = channel.m_FreqWindow[0].Alloc( in_pAllocator, m_uFFTSize );
				if ( eResult != AK_Success )
					return eResult;
				eResult = channel.m_FreqWindow[1].Alloc( in_pAllocator, m_uFFTSize );
				if ( eResult != AK_Success )
					return eResult;
				eResult = channel.m_VocoderWindow.Alloc( in_pAllocator, m_uFFTSize );
				if ( eResult != AK_Success )
					return eResult;

				channel.m_pfPrevSynthesisPhase = (PhaseProcessingType *)AK_PLUGIN_ALLOC( in_pAllocator, AK_ALIGN_SIZE_FOR_DMA( ((m_uFFTSize/2)+1)*sizeof(PhaseProcessingType) ) );
				if ( channel.m_pfPrevSynthesisPhase == NULL )
					return AK_InsufficientMemory;
			
				if ( in_bUseInputBuffer )
				{
					eResult = channel.m_InputAccumBuf.Init( in_pAllocator, m_uFFTSize+m_uFFTSize/4 );
					if ( eResult != AK_Success )
						return eResult;
				}
				eResult = channel.m_OLAOutCircBuf.Init( in_pAllocator, m_uFFTSize, m_uFFTSize );
				if ( eResult != AK_Success )
					return eResult;
			}
		}
		return AK_Success;
	}

	void CAkPhaseVocoder::Term( AK::IAkPluginMemAlloc * in_pAllocator )
	{
		if ( m_pFFTState )
		{	
			AK_PLUGIN_FREE( in_pAllocator, m_pFFTState );
			m_pFFTState = NULL;
		}
		if ( m_pIFFTState )
		{	
			AK_PLUGIN_FREE( in_pAllocator, m_pIFFTState );
			m_pIFFTState = NULL;
		}
		m_TimeWindow.Term( in_pAllocator );

		if ( m_pChannels )
		{
			for ( AkUInt32 i = 0; i < m_uNumChannels; i++ )
			{
				Channel & channel = m_pChannels[ i ];
				channel.m_FreqWindow[0].Free( in_pAllocator );
				channel.m_FreqWindow[1].Free( in_pAllocator );
				channel.m_VocoderWindow.Free( in_pAllocator );
				if ( m_bUseInputBuffer )
					channel.m_InputAccumBuf.Term( in_pAllocator );
				channel.m_OLAOutCircBuf.Term( in_pAllocator );
				if ( channel.m_pfPrevSynthesisPhase )
				{
					AK_PLUGIN_FREE( in_pAllocator, channel.m_pfPrevSynthesisPhase );
				}
			}

#ifndef AK_VOICE_MAX_NUM_CHANNELS
			AK_PLUGIN_FREE( in_pAllocator, m_pChannels );
			m_pChannels = NULL;
#endif
		}
	}

	void CAkPhaseVocoder::Reset( )
	{
		if ( m_pChannels )
		{
			for ( AkUInt32 i = 0; i < m_uNumChannels; i++ )
			{
				Channel & channel = m_pChannels[ i ];
				channel.m_FreqWindow[0].SetReady( false );
				channel.m_FreqWindow[1].SetReady( false );
				channel.m_VocoderWindow.SetReady( false );
				if ( m_bUseInputBuffer )
					channel.m_InputAccumBuf.Reset();
				channel.m_OLAOutCircBuf.Reset();
				if ( channel.m_pfPrevSynthesisPhase )
				{
					AkZeroMemLarge( channel.m_pfPrevSynthesisPhase, AK_ALIGN_SIZE_FOR_DMA( ((m_uFFTSize/2)+1)*sizeof(PhaseProcessingType) ) );
				}
			}
		}
		m_fInterpPos = 0.f;
		m_uInputFramesToDiscard = 0;
		m_bInitPhases = true;
		m_bInputStartFill = true;
	}

	void CAkPhaseVocoder::Execute( 
		AkAudioBuffer * io_pInBuffer, 
		AkUInt32		in_uInOffset,
		AkAudioBuffer * io_pOutBuffer,
		AkReal32		in_fTSFactor,
		bool			in_bEnterNoTSMode,
		AkReal32 *		in_pfTempStorage	
		)
	{
		AKASSERT( m_bUseInputBuffer );

		AkReal32 * pfTDWindowStorage = in_pfTempStorage;

		if ( in_bEnterNoTSMode )
		{
			m_fInterpPos = 0.f;
			m_bInitPhases = true;
		}

		AKASSERT( m_uFFTSize % PV_OVERLAP_FACTOR == 0 );
		const AkUInt32 uHopSize = m_uFFTSize / PV_OVERLAP_FACTOR;
		const AkReal32 fTimeScale = 100.f/in_fTSFactor;

		// Compute overall system gain and compensate for windowing induced gain
		// Window factor = WindowSize / WindowCummulativeSum
		// Analysis overlap gain = overlap factor / Window factor
		// Compensation gain = 1.f / Analysis overlap gain 
		const AkReal32 fAnalysisGain = (m_TimeWindow.GetCummulativeSum() * PV_OVERLAP_FACTOR) / (AkReal32)m_uFFTSize;
		const AkReal32 fCompGain = 1.f/fAnalysisGain;

		const AkUInt32 uFFTSize = m_uFFTSize;
		// Modify those locally for each channel and set them just before returning at the end
		AkUInt32 uInputValidFrames;
		AkUInt32 uOutputValidFrames;
		AkReal32 fInterpPos;
		AkUInt32 uInputFramesToDiscard;
		AkUInt32 uInOffset;
		bool bInitPhases;
		bool bInputStartFill;

		AKASSERT( m_uNumChannels > 0 );

		AkUInt32 i = 0;
		do
		{
			Channel & channel = m_pChannels[ i ];

			// Modify those locally for each channel and set them just before returning at the end
			uInputValidFrames = io_pInBuffer->uValidFrames;
			uOutputValidFrames = io_pOutBuffer->uValidFrames;
			fInterpPos = m_fInterpPos;
			uInputFramesToDiscard = m_uInputFramesToDiscard;
			bInitPhases = m_bInitPhases; 
			uInOffset = in_uInOffset;
			bInputStartFill = m_bInputStartFill;

			AkReal32 * AK_RESTRICT pInBuf = (AkReal32 * ) io_pInBuffer->GetChannel( i ); 
			AkReal32 * AK_RESTRICT pfOutBuf = (AkReal32 * ) io_pOutBuffer->GetChannel( i );

			while ( true )
			{
				// If there are frames remaining to discard, the input buffer should be empty
				AKASSERT( uInputFramesToDiscard == 0 || channel.m_InputAccumBuf.FramesReady() == 0 );
				AkUInt32 uFramesDiscarded = AkMin( uInputValidFrames, uInputFramesToDiscard );
				uInputFramesToDiscard -= uFramesDiscarded;
				uInputValidFrames -= uFramesDiscarded;
				uInOffset += uFramesDiscarded;
				if ( bInputStartFill && uInputValidFrames == 0 && io_pInBuffer->eState != AK_NoMoreData )
				{
					io_pOutBuffer->eState = AK_DataNeeded;
					break;
				}

				// Once we reached the position, we can start filling the accumulation buffer with enough data for FFTs
				AkUInt32 uFramesConsumed = channel.m_InputAccumBuf.PushFrames( pInBuf+uInOffset, uInputValidFrames );
				if ( channel.m_InputAccumBuf.FramesEmpty() == 0 )
					bInputStartFill = false;

				uInputValidFrames -= (AkUInt16)uFramesConsumed;	
				uInOffset += uFramesConsumed;
				if ( bInputStartFill && channel.m_InputAccumBuf.FramesEmpty() != 0 && uInputValidFrames == 0 && io_pInBuffer->eState != AK_NoMoreData )
				{
					io_pOutBuffer->eState = AK_DataNeeded;
					break;
				}	

				bool bNoMoreInputData = io_pInBuffer->eState == AK_NoMoreData && uInputValidFrames == 0; 
				
				// Compute spectrum of previous FFT frame
				if ( !channel.m_FreqWindow[FREQWIN_PREV].IsReady() )
				{
					bool bSuccess = channel.m_InputAccumBuf.ReadFrameBlock( pfTDWindowStorage, uFFTSize, bNoMoreInputData );
					if ( bSuccess )
					{
						AkUInt32 uFrameAdvance = channel.m_InputAccumBuf.AdvanceFrames( uHopSize );
						AKASSERT( bNoMoreInputData || uFrameAdvance == uHopSize );
						m_TimeWindow.Apply( pfTDWindowStorage, uFFTSize );
						channel.m_FreqWindow[FREQWIN_PREV].Compute( pfTDWindowStorage, uFFTSize, m_pFFTState );
						channel.m_FreqWindow[FREQWIN_PREV].CartToPol();
					}
				}

				// Compute spectrum of next FFT frame
				if ( !channel.m_FreqWindow[FREQWIN_CUR].IsReady() )
				{
					bool bSuccess = channel.m_InputAccumBuf.ReadFrameBlock( pfTDWindowStorage, uFFTSize, bNoMoreInputData);
					if ( bSuccess )
					{
						AkUInt32 uFrameAdvance = channel.m_InputAccumBuf.AdvanceFrames( uHopSize );
						AKASSERT( bNoMoreInputData || uFrameAdvance == uHopSize );
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

					fInterpPos += fTimeScale;
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
							AkUInt32 uFramesSkipped = channel.m_InputAccumBuf.AdvanceFrames( uFramesToFlushFromInput );
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
				// Note: Do we need to consider spectral (prev and next) windows as well?
				bool bFlushingOLABuffer =	(bNoMoreInputData &&
											channel.m_InputAccumBuf.FramesReady() == 0 &&
											!channel.m_VocoderWindow.IsReady() &&
											channel.m_OLAOutCircBuf.FramesReady() == 0 );

				AkUInt32 uFramesProduced = channel.m_OLAOutCircBuf.PopFrames( 
					pfOutBuf+uOutputValidFrames, 
					io_pOutBuffer->MaxFrames() - uOutputValidFrames,
					bFlushingOLABuffer
					);
				uOutputValidFrames += (AkUInt16)uFramesProduced;

				if (	bFlushingOLABuffer &&
						channel.m_OLAOutCircBuf.IsDoneTail() )
				{
					io_pOutBuffer->eState = AK_NoMoreData;
					break;
				}
				else if ( uOutputValidFrames == io_pOutBuffer->MaxFrames() ) 
				{
					io_pOutBuffer->eState = AK_DataReady;
					break;
				}
				else if ( !bNoMoreInputData && uInputValidFrames == 0 )
				{
					io_pOutBuffer->eState = AK_DataNeeded;
					break;
				}

			} // While
		} // Channel loop
		while ( ++i < m_uNumChannels );
	
		io_pInBuffer->uValidFrames = (AkUInt16)uInputValidFrames;
		io_pOutBuffer->uValidFrames = (AkUInt16)uOutputValidFrames;
		m_fInterpPos = fInterpPos;
		m_uInputFramesToDiscard = uInputFramesToDiscard;
		m_bInitPhases = bInitPhases;
		m_bInputStartFill = bInputStartFill;
	}

} // namespace BUTTERFLYSET_NAMESPACE

} // namespace DSP
