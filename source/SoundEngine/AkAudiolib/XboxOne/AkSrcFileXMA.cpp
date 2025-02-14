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

#include "AkSrcFileXMA.h"
#include "AkFileParserBase.h"
#include "AudiolibDefs.h"
#include "AkMonitor.h"
#include <stdio.h>
#include "AkPlayingMgr.h"
#include "AkLEngine.h"
#include "AkProfile.h"

IAkSoftwareCodec* CreateXMAFilePlugin(void* in_pCtx)
{
	return AkNew(AkMemID_Processing, CAkSrcFileXMA((CAkPBI*)in_pCtx));
}

CAkSrcFileXMA::CAkSrcFileXMA( CAkPBI * in_pCtx )
	: CAkSrcXMABase<CAkSrcFileBase>( in_pCtx )
	, m_arSeekTable( NULL )
	, m_uRemainingSamplesAfterSeek( 0 )
	, m_uNextIdxToSubmit( 0 )
	, m_uStmBuffersRefCnt( 0 ) 
	, m_uNextXmaSlot( 0 )
	, m_bReadyForDecoding( false )
	, m_bIsVirtual( false )
{
}

CAkSrcFileXMA::~CAkSrcFileXMA()
{
	if ( m_arSeekTable )
	{
		AkFree( AkMemID_Processing, m_arSeekTable );
	}
}

void CAkSrcFileXMA::StopStream()
{
	StopDecoding();
	
	// Have to wait for any jobs to complete, then DecodingFinished() will be called by the AcpMgr to release the stream buffers.
	WaitForHardwareVoices();
	
	CAkSrcFileBase::StopStream();
}

void CAkSrcFileXMA::StopDecoding()
{
	if (m_ulSizeLeft > 0)
	{
		// A stream buffer has been acquired but not attached to an input buffer.
		CAkSrcFileBase::ReleaseStreamBuffer();
		ConsumeData(m_ulSizeLeft);
	}

	m_bReadyForDecoding = false;
	FreeHardwareVoices();
}

void CAkSrcFileXMA::HwVoiceFinished()
{
	for (AkUInt32 i = 0; i < AK_XMA_NUMSTREAMBUFFERS; i++)
	{
		if (m_arXmaInputBuffers[i].HasData())
			ReleaseStreamBuffer(i);
	}

	CAkSrcXMABase<CAkSrcFileBase>::HwVoiceFinished();
}

bool CAkSrcFileXMA::IsDataNeeded()
{	
	AKASSERT(HardwareVoicesRegistered());

	if (m_bIsVirtual||m_bFatalError)
	{
		return false;
	}

	AkUInt32 uXMAInputMask = 0;
	bool bMoreDataAvailable = !HasNoMoreStreamData();
	if (m_bReadyForDecoding)
    {
        for (unsigned int i = 0; i < AK_XMA_NUMSTREAMBUFFERS; i++)
        {
			// Release all submitted buffers that are no longer valid in all XMA contexts.
			if (m_arXmaInputBuffers[i].IsSubmitted())
			{
				bool bAllExhausted = true;

				for (unsigned int xmaStream = 0; xmaStream < m_uNumXmaStreams && bAllExhausted; xmaStream++)
				{
					bAllExhausted = bAllExhausted && ( (m_pHwVoice[xmaStream]->pXmaContext->validBuffer & (m_arXmaInputBuffers[i].XmaBufferMask())) == 0 );
				}

				if ( bAllExhausted )
					ReleaseStreamBuffer(i);
				
				AKASSERT( ( m_arXmaInputBuffers[i].XmaBufferMask() & uXMAInputMask ) == 0 || !"Two buffers in same XMA input" );
				uXMAInputMask = uXMAInputMask | m_arXmaInputBuffers[i].XmaBufferMask();
			}
			else if (m_arXmaInputBuffers[i].HasData())
			{
				bMoreDataAvailable = true;	// This input buffer has data that could be attached.
			}
        }
    }

	// Return true if the XMA contexts have at least one free slot, and we can provide data (either already cached or by fetching new stream buffer).
	return ( uXMAInputMask != 0x3 && bMoreDataAvailable );
}

void CAkSrcFileXMA::SubmitXMABuffer()
{
	AKASSERT(HardwareVoicesRegistered());

	AkXmaInputBuffer & input = m_arXmaInputBuffers[m_uNextIdxToSubmit];

	AKASSERT( input.Data() 
		&& (AkUIntPtr)input.Data() % SHAPE_XMA_INPUT_BUFFER_ALIGNMENT == 0 );
	AKASSERT( input.IsReady() );
	
	// Since we're locked, take the opportunity to check for parser errors.
	bool bParserError = false;
	AkUInt32 uXmaSlot = GetNextXmaSlotAndUpdate();
	

	//#MONITOR_MSG
#ifdef AK_MONITOR_XMA_STREAM_BUFFERES
	{
		char chbuf[256];
		int n = sprintf(chbuf,"-> Submit slot %i, (%u)\n", uXmaSlot, ApuMapVirtualAddress(input.Data()) );
		for (AkUInt8 i = 0; i < m_uNumXmaStreams; i++)
			n += sprintf(chbuf+n, ", [%i: cur=%u, valid=%u]\n", i, m_pHwVoice[i]->pXmaContext->currentBuffer, m_pHwVoice[i]->pXmaContext->validBuffer );
		MONITOR_MSG(chbuf);
	}
#endif

	if ( uXmaSlot == 0 )
	{
		AKASSERT( ( GetXmaInputMask() & 0x1 ) == 0 || !"Slot is not free" );

		for (AkUInt8 i = 0; i < m_uNumXmaStreams; i++)
		{
			AKASSERT( !m_pHwVoice[i]->IsXmaCtxEnable() 
					&& !GetShapeXmaInputBuffer0Valid(m_pHwVoice[i]->pXmaContext) );
			HRESULT hr = SetShapeXmaInputBuffer0(m_pHwVoice[i]->pXmaContext, input.Data());
			AKVERIFY( SUCCEEDED( hr ) );
			hr = SetShapeXmaInputBuffer0Size(m_pHwVoice[i]->pXmaContext, input.Size());
			AKVERIFY( SUCCEEDED( hr ) );
			hr = SetShapeXmaInputBuffer0Valid(m_pHwVoice[i]->pXmaContext, true);
			AKVERIFY( SUCCEEDED( hr ) );

			bParserError = bParserError || GetShapeXmaParserErrorStatus( m_pHwVoice[i]->pXmaContext );
			AKASSERT( !bParserError );
		}
		input.SetXmaBuffer0();
	}
	else
	{
		AKASSERT( uXmaSlot == 1 );
		AKASSERT( ( GetXmaInputMask() & 0x2 ) == 0 || !"Slot is not free" );
				
		for (AkUInt8 i = 0; i < m_uNumXmaStreams; i++)
		{
			AKASSERT( !m_pHwVoice[i]->IsXmaCtxEnable() 
					&& !GetShapeXmaInputBuffer1Valid(m_pHwVoice[i]->pXmaContext) );
			HRESULT hr = SetShapeXmaInputBuffer1(m_pHwVoice[i]->pXmaContext, input.Data());
			AKVERIFY( SUCCEEDED( hr ) );
			hr = SetShapeXmaInputBuffer1Size(m_pHwVoice[i]->pXmaContext, input.Size());
			AKVERIFY( SUCCEEDED( hr ) );
			hr = SetShapeXmaInputBuffer1Valid(m_pHwVoice[i]->pXmaContext, true);
			AKVERIFY( SUCCEEDED( hr ) );

			bParserError = bParserError || GetShapeXmaParserErrorStatus( m_pHwVoice[i]->pXmaContext );
			AKASSERT( !bParserError );
		}
		input.SetXmaBuffer1();
	}

	// Tie loose ends.
	for (AkUInt8 i = 0; i < m_uNumXmaStreams; i++)
	{
		if ( (uXmaSlot ^ 1) == 0 )
		{
			if ( !GetShapeXmaInputBuffer0Valid(m_pHwVoice[i]->pXmaContext) )
			{
				//#MONITOR_MSG
				//char chbuf[256]; sprintf(chbuf,"** %i buf-0 invalid! currentBuffer:%u, offsetRead:%u \n",i, ((m_pHwVoice[i])->pXmaContext)->currentBuffer, ((m_pHwVoice[i])->pXmaContext)->offsetRead );	MONITOR_MSG(chbuf);

				HRESULT hr = SetShapeXmaInputBuffer0( m_pHwVoice[i]->pXmaContext, input.Data() );
				AKVERIFY( SUCCEEDED( hr ) );
			}
		}
		else
		{
			AKASSERT( (uXmaSlot ^ 1) == 1 );
			if ( !GetShapeXmaInputBuffer1Valid(m_pHwVoice[i]->pXmaContext) )
			{
				//#MONITOR_MSG
				//char chbuf[256]; sprintf(chbuf,"** %i buf-1 invalid! currentBuffer:%u, offsetRead:%u\n",i, ((m_pHwVoice[i])->pXmaContext)->currentBuffer, ((m_pHwVoice[i])->pXmaContext)->offsetRead );	MONITOR_MSG(chbuf);

				HRESULT hr = SetShapeXmaInputBuffer1( m_pHwVoice[i]->pXmaContext, input.Data() );
				AKVERIFY( SUCCEEDED( hr ) );
			}
		}
	}

	// Increment m_uNextIdxToSubmit.
	m_uNextIdxToSubmit++;
	if ( m_uNextIdxToSubmit == AK_XMA_NUMSTREAMBUFFERS )
		m_uNextIdxToSubmit = 0;

	if (bParserError)
	{
		SetFatalError();
	}
}

// Returns AK_Fail if a fatal error occurred, AK_Success otherwise.
AKRESULT CAkSrcFileXMA::AddBuffer() 
{
	AKASSERT(HardwareVoicesRegistered());
	AKASSERT( m_pNextAddress != NULL 
		&& ( (AkUIntPtr)m_pNextAddress & SHAPE_XMA_INPUT_BUFFER_MASK ) == 0 
		&& m_ulSizeLeft > 0 
		&& ( m_ulSizeLeft & SHAPE_XMA_INPUT_BUFFER_SIZE_MASK ) == 0 );

	AKASSERT( m_pHwVoice[0] != NULL );
	
	AKRESULT eResult = AK_Fail;
	bool bStreamBufferReferenced = false;
	bool bBufferSubmitted = false;	// Submit one and only one buffer per call to Add().

	if (!m_bReadyForDecoding)
	{
		if ( StartXMA() != AK_Success )
			goto ConsumeStreamBuffer;
	}

	AKASSERT( m_uStmBuffersRefCnt < 2 );

	if ( m_arXmaInputBuffers[m_uNextIdxToSubmit].HasData() )
	{
		// Next buffer to submit already has some data. It is an incomplete stitch buffer with allocated size equal to one XMA block.
		AKASSERT( m_arXmaInputBuffers[m_uNextIdxToSubmit].Size() > 0 
			&& !m_arXmaInputBuffers[m_uNextIdxToSubmit].IsReady() );
		
		// Append
		AkUInt32 uSizeStitched = m_arXmaInputBuffers[m_uNextIdxToSubmit].Stitch(m_pNextAddress, m_ulSizeLeft);
		AKASSERT( uSizeStitched > 0 || !"We know it was already allocated" );

		ConsumeData(uSizeStitched);

		if ( m_arXmaInputBuffers[m_uNextIdxToSubmit].IsReady() )
		{
			// Submit stream buffer.
			SubmitXMABuffer(); 
			bBufferSubmitted = true;
		}
	}
		
	// Assign stream buffer to next input buffer if it is large enough, or move remaining data to another stitch buffer if it isn't.
	{
		AkUInt32 uRequiredSize = ComputeRequiredSize();

		if ( m_ulSizeLeft >= uRequiredSize )
		{
			AkUInt32 uSizeAssigned = m_arXmaInputBuffers[m_uNextIdxToSubmit].Assign(m_pNextAddress, m_ulSizeLeft, uRequiredSize);
			AKASSERT( !bStreamBufferReferenced || !"Cannot be addref'd twice" );
			bStreamBufferReferenced = true;

			// Next stream buffer has now enough data to be submitted.
			AKASSERT( m_arXmaInputBuffers[m_uNextIdxToSubmit].IsReady() );

			// Consume data but do not release buffer. It was assigned to m_arXmaInputBuffers[m_uNextIdxToSubmit].
			ConsumeData(uSizeAssigned);

			if ( !bBufferSubmitted )
			{
				// Submit stream buffer.
				SubmitXMABuffer(); 
			}
		}
	}

	// Deal with remaining data in this stream buffer, if any,
	if ( m_ulSizeLeft > 0 )
	{
		// Some of the stream data has not been consumed. Find next available input buffer.
		AkUInt32 uNextAvailableBuffer = m_uNextIdxToSubmit;
		while ( m_arXmaInputBuffers[uNextAvailableBuffer].IsReady() )
		{
			uNextAvailableBuffer++;
			AKASSERT( uNextAvailableBuffer != m_uNextIdxToSubmit || !"No buffer available for caching" );
			if ( uNextAvailableBuffer == AK_XMA_NUMSTREAMBUFFERS )
				uNextAvailableBuffer = 0;
		}

		// Allocate stitch buffer and copy data in it.
		AkUInt32 uSizeStitched = m_arXmaInputBuffers[uNextAvailableBuffer].Stitch(m_pNextAddress, m_ulSizeLeft, ComputeRequiredSize());
		if ( uSizeStitched == 0 )
			goto ConsumeStreamBuffer;
			
		AKASSERT(uSizeStitched == m_ulSizeLeft);
		ConsumeData(uSizeStitched);
	}		
	
	eResult = AK_Success;

ConsumeStreamBuffer:

	AKASSERT( m_ulSizeLeft == 0 || eResult != AK_Success );
	if ( eResult != AK_Success )
		ConsumeData(m_ulSizeLeft);	// Make sure the stream buffer is marked as consumed in case buffer adding/stitching failed.

	// Release stream buffer now if we can. Otherwise wait for the XMA context to release it.
	if ( bStreamBufferReferenced )
		m_uStmBuffersRefCnt++;
	else
	{
		if ( m_uStmBuffersRefCnt == 0 )
			CAkSrcFileBase::ReleaseStreamBuffer();
		else
		{
			// If we cannot release this new stream buffer because an older buffer is already being referenced, but yet have copied all its content in stitch buffers,
			// then we need to increment the reference count. Stream buffers will be released accordingly next time inputs are recycled.
			m_uStmBuffersRefCnt++;
		}
	}

	return eResult;
}

void CAkSrcFileXMA::ReleaseStreamBuffer(unsigned int index) 
{
#ifdef XMA_CHECK_VOICE_SYNC
	AKASSERT(!AkACPMgr::Get().VoiceIsInFlowgraph(m_pHwVoice[0]));
#endif
	AKASSERT( m_arXmaInputBuffers[index].HasData() );
 	
	if ( m_arXmaInputBuffers[index].XmaBufferMask() & 0x1 )
 	{
		for (AkUInt8 i = 0; i < m_uNumXmaStreams; i++)
		{
			AKASSERT( !m_pHwVoice[i]->IsXmaCtxEnable() );
			HRESULT hr = SetShapeXmaInputBuffer0Valid(m_pHwVoice[i]->pXmaContext, false);
			AKVERIFY( SUCCEEDED( hr ) );
			// Both buffer pointers must point to valid memory locations (to avoid a potential HW hang) even if you only use one of the two buffers.
			hr = SetShapeXmaInputBuffer0(m_pHwVoice[i]->pXmaContext, GetShapeXmaInputBuffer1(m_pHwVoice[i]->pXmaContext));	// Set to same as buffer1 
			AKVERIFY( SUCCEEDED( hr ) );
			// NOTE -> DON’T change the size of the input buffers (to zero) when you release a stream buffer.
		}
	}
	else if ( m_arXmaInputBuffers[index].XmaBufferMask() & 0x2 )
	{
		for (AkUInt8 i = 0; i < m_uNumXmaStreams; i++)
		{
			AKASSERT( !m_pHwVoice[i]->IsXmaCtxEnable() );
			HRESULT hr = SetShapeXmaInputBuffer1Valid(m_pHwVoice[i]->pXmaContext, false);
			AKVERIFY( SUCCEEDED( hr ) );
			// Both buffer pointers must point to valid memory locations (to avoid a potential HW hang) even if you only use one of the two buffers.
			hr = SetShapeXmaInputBuffer1(m_pHwVoice[i]->pXmaContext, GetShapeXmaInputBuffer0(m_pHwVoice[i]->pXmaContext));	// Set to same as buffer0 
			AKVERIFY( SUCCEEDED( hr ) );
			// NOTE -> DON’T change the size of the input buffers (to zero) when you release a stream buffer.
		}
 	}

	//#MONITOR_MSG
#ifdef AK_MONITOR_XMA_STREAM_BUFFERES
	{
		AkUInt32 uSlot = ( m_arXmaInputBuffers[index].XmaBufferMask() & 0x2 ) 1 : 0;
		char chbuf[256];
		int n = sprintf(chbuf,"-> Release slot %i, (%u)\n", uSlot, ApuMapVirtualAddress(m_arXmaInputBuffers[index].Data()) );
		for (AkUInt8 i = 0; i < m_uNumXmaStreams; i++)
			n += sprintf(chbuf+n, ", [%i: cur=%u, valid=%u]\n", i, m_pHwVoice[i]->pXmaContext->currentBuffer, m_pHwVoice[i]->pXmaContext->validBuffer );
		MONITOR_MSG(chbuf);
	}
#endif

	m_arXmaInputBuffers[index].Release();

	// Release stream buffers.
	// We may have zero, one or two stream buffers to release. Two stream buffers will be released if the content of a stream buffer was entirely copied to stitch buffers,
	// but we could not release it back then because an older stream buffer was being referenced.
	// Scan the input buffers and count the number of streams buffers that are still referenced.
	AKASSERT( m_uStmBuffersRefCnt <= 2 );
	AkUInt32 uStmBufferReferenced = 0;
	for (AkUInt32 i = 0; i < AK_XMA_NUMSTREAMBUFFERS; i++)
	{
		if (m_arXmaInputBuffers[i].ReferencesStmBuffer())
			uStmBufferReferenced++;
	}
	AKASSERT( uStmBufferReferenced <= m_uStmBuffersRefCnt );
	while ( m_uStmBuffersRefCnt > uStmBufferReferenced )
	{
		CAkSrcFileBase::ReleaseStreamBuffer();
		m_uStmBuffersRefCnt--;
	}
}

void CAkSrcFileXMA::UpdateInput(AkUInt32 in_uStreamIdx)
{
#ifdef AK_ENABLE_ASSERTS
	AKASSERT(HardwareVoicesRegistered());
	AkUInt32 curBuf = m_pHwVoice[0]->pXmaContext->currentBuffer;
	AkUInt32 valBuf = m_pHwVoice[0]->pXmaContext->validBuffer;
	AkUInt32 sampIncr = m_pHwVoice[0]->pSrcContext->samplingIncrement;
	AkUInt32 sampIncrTgt = m_pHwVoice[0]->pSrcContext->samplingIncrementTarget;
	for (AkUInt8 i = 0; i < m_uNumXmaStreams; i++)
	{
		AKASSERT( curBuf == m_pHwVoice[i]->pXmaContext->currentBuffer || valBuf != m_pHwVoice[i]->pXmaContext->validBuffer );
		AKASSERT( sampIncrTgt ==  m_pHwVoice[i]->pSrcContext->samplingIncrementTarget );
		AKASSERT( sampIncr ==  m_pHwVoice[i]->pSrcContext->samplingIncrement );
	}
#endif

	while ( IsDataNeeded() )
	{
		// Use buffer we already have waiting to be submitted, if any.
		if ( m_arXmaInputBuffers[m_uNextIdxToSubmit].IsReady() )
		{
			// There must be a free slot available. 
			AKASSERT( m_bReadyForDecoding );
			AKASSERT( GetXmaInputMask() != 0x3 );
			SubmitXMABuffer(); 
		}
		else
		{
			// Need new stream buffer.
			AKRESULT res = FetchStreamBuffer();
			if (res == AK_DataReady)
				res = AddBuffer();
			
			// Fatal errors will be handled at next GetBuffer()
			if (res == AK_Fail)
				SetFatalError();

			if (res != AK_Success ) // can be AK_NoDataReady
				break;
		}
	}

	AkACPVoice* pVoice = m_pHwVoice[in_uStreamIdx];
	AKASSERT(pVoice);

	SHAPE_XMA_CONTEXT* pXmaCtx = pVoice->pXmaContext;

	//See how much unconsumed decoded data we have.
	pVoice->uMaxFlowgraphsToRun = 0;

	if ( m_bReadyForDecoding && !m_bIsVirtual)
	{
		SHAPE_SRC_CONTEXT* pSrcCtx = pVoice->pSrcContext;
		AkUInt32 uXmaOutputBufferNumDecodedSamples = SHAPEHELPERS::GetXmaOutputBufferNumDecodedSamples(pXmaCtx, pSrcCtx);
		AKASSERT( DoLoop() || m_uTotalSamples - m_uCurSample >= uXmaOutputBufferNumDecodedSamples);
		if ( !DoLoop() && m_uTotalSamples - m_uCurSample == uXmaOutputBufferNumDecodedSamples )
		{
			// All samples from the source have been decoded.  
			pVoice->SwitchToStopEndMode();
			pVoice->uMaxFlowgraphsToRun = AK_NUM_DMA_FRAMES_PER_BUFFER;
		}
		else
		{
			AkUInt32 uDecodedShapeFramesAvailable = uXmaOutputBufferNumDecodedSamples / SHAPE_FRAME_SIZE;
			AkUInt32 uEncodedShapeFramesAvailable = 0;

			AkUInt32 uShapeFramesNeeded = SHAPEHELPERS::GetNumShapeFramesNeeded( AK_NUM_DMA_FRAMES_PER_BUFFER, m_iCurSamplingIncr, m_iTgtSamplingIncr, pSrcCtx->samplePointer);

			if (uDecodedShapeFramesAvailable < uShapeFramesNeeded)
			{
				// The minimum number of complete XMA frames needed to make the required number of SHAPE frames 
				AkUInt32 uXmaFramesNeededInBuffer = ( ((uShapeFramesNeeded - uDecodedShapeFramesAvailable + pXmaCtx->numSubframesToSkip) * SHAPE_FRAME_SIZE) + (XMA_SAMPLES_PER_FRAME-1) ) / XMA_SAMPLES_PER_FRAME;
				AKASSERT(uXmaFramesNeededInBuffer > 0);
				
				DWORD uXmaFramesAvailable = 0;

				AkXmaInputBuffer* pCurBuffer = NULL;
				AkXmaInputBuffer* pNextBuffer = NULL;
				GetXmaInputBuffers(pXmaCtx->currentBuffer, pXmaCtx->validBuffer, pCurBuffer, pNextBuffer );

				{
					//Determine how many XMA frames are available, the complicated way...
					DWORD uXmaOffset = pXmaCtx->offsetRead;
					DWORD uFramesNeeded = uXmaFramesNeededInBuffer;

					if ( pCurBuffer != NULL )
						uXmaFramesAvailable += XMAHELPERS::GetNumXmaFramesAvailable(uXmaFramesNeededInBuffer, pCurBuffer->Data(), pCurBuffer->Size(), uXmaOffset);

					if (uXmaFramesAvailable < uXmaFramesNeededInBuffer && pNextBuffer != NULL)
						uXmaFramesAvailable += XMAHELPERS::GetNumXmaFramesAvailable(uXmaFramesNeededInBuffer, pNextBuffer->Data(), pNextBuffer->Size(), uXmaOffset );
				}

				// Convert the available encoded XMA frames back to SHAPE frames, subract the number of frames that will be skipped by the decoder.
				uEncodedShapeFramesAvailable = (AkUInt8) AkMax( (AkInt32)(((uXmaFramesAvailable * XMA_SAMPLES_PER_FRAME) / SHAPE_FRAME_SIZE)) - (AkInt32)pXmaCtx->numSubframesToSkip, 0 );

#ifdef AK_MONITOR_XMA_STREAM
				//#MONITOR_MSG
				{
					char chbuf_pitch[64],chbuf[256];
					if ( m_iTgtSamplingIncr == m_iCurSamplingIncr ) sprintf(chbuf_pitch,"%.3f", GetCurrentPitchRatio());
					else sprintf(chbuf_pitch,"%.3f -> %.3f", GetCurrentPitchRatio(), GetNextPitchRatio());
					sprintf(chbuf,"XMA(%u)__ FGs:%u, need: %u, avail: (dec:%u, enc:%u) pitch: %s\n",
						in_uStreamIdx,
						SHAPEHELPERS::GetNumFlowgraphsThatCanRun(uEncodedShapeFramesAvailable+uDecodedShapeFramesAvailable, m_iCurSamplingIncr, m_iTgtSamplingIncr, pSrcCtx->samplePointer ),
						uShapeFramesNeeded,
						uDecodedShapeFramesAvailable, 
						uEncodedShapeFramesAvailable,
						chbuf_pitch
						);
					MONITOR_MSG(chbuf);
				}
#endif
			}

			pVoice->uMaxFlowgraphsToRun = (AkUInt8)SHAPEHELPERS::GetNumFlowgraphsThatCanRun(uEncodedShapeFramesAvailable+uDecodedShapeFramesAvailable, m_iCurSamplingIncr, m_iTgtSamplingIncr, pSrcCtx->samplePointer);
			AKASSERT(SHAPEHELPERS::GetNumShapeFramesNeeded(pVoice->uMaxFlowgraphsToRun, m_iCurSamplingIncr, m_iTgtSamplingIncr, pSrcCtx->samplePointer) <= uEncodedShapeFramesAvailable+uDecodedShapeFramesAvailable );
		}
	}
}

void CAkSrcFileXMA::GetBuffer( AkVPLState & io_state )
{
	AkACPMgr::Get().WaitForFlowgraphs();

	// XMA cannot handle prebuffering correctly. Just push buffering data if applicable.
	UpdateBufferingInfo();

	if ( m_bFatalError )
	{
		io_state.result = AK_Fail;
		return;
	}
	else if ( !m_bReadyForDecoding )
	{
		io_state.result = AK_NoDataReady;
		return;
	}

	AKASSERT(m_pHwVoice[0] != NULL);
	
	// Update source if required (once per Wwise frame).
	if ( UpdateSource( io_state ) == AK_Fail )
	{
		io_state.result = AK_Fail;
		return;
	}

	// Handle starvation.
	if ( m_uNumShapeFramesAvailable == 0 ) 
	{
		// The pipeline is trying to pull too much data at a time.
		io_state.result = AK_NoDataReady;
		return;
	}

	const AkAudioFormat &rFormat = m_pCtx->GetMediaFormat();

	// Number of samples produced for the pipeline corresponds to 1 SHAPE_FRAME.
	AKASSERT(m_uNumShapeFramesAvailable >= 1);
	AKASSERT(m_uNumShapeFramesGranted == 0);
	m_uNumShapeFramesGranted = 1;
	AkUInt16 uNumSamplesRefill = (AkUInt16)( 1 * SHAPE_FRAME_SIZE );
	AkReal32* pCurrentDmaBuffer = m_pDmaBuffer + m_pHwVoice[0]->uDmaReadPointer * SHAPE_FRAME_SIZE * rFormat.GetNumChannels();

	if (rFormat.HasLFE())
	{
		// LFE should follow left, right, center
		AkUInt32 uMask = (rFormat.channelConfig.uChannelMask & AK_SPEAKER_FRONT_LEFT) |
			(rFormat.channelConfig.uChannelMask & AK_SPEAKER_FRONT_RIGHT) |
			(rFormat.channelConfig.uChannelMask & AK_SPEAKER_FRONT_CENTER);

		// move LFE last
		XMAHELPERS::MoveLFEBuffer(pCurrentDmaBuffer, uNumSamplesRefill, (AkUInt16)AK::GetNumNonZeroBits( uMask ), (AkUInt16)rFormat.GetNumChannels()-1);
	}

	io_state.AttachInterleavedData( 
		pCurrentDmaBuffer,	// use our own dma read pointer because the other is updated asynchronously
		uNumSamplesRefill, 
		uNumSamplesRefill,
		rFormat.channelConfig );

	io_state.posInfo.uSampleRate = m_uSourceSampleRate;
	io_state.posInfo.uStartPos = m_uCurSample;
	io_state.posInfo.uFileEnd = m_uTotalSamples;

	ConsumeDmaFrames();

	// End of source?
	// Wait until all SHAPE frames previously produced are passed to the pipeline (including this one) before returning NoMoreData.
	io_state.result = ( !m_bEndOfSource || ValidDmaFrames() > 0 ) ? AK_DataReady : AK_NoMoreData;
}

void CAkSrcFileXMA::VirtualOn( AkVirtualQueueBehavior eBehavior )
{
    // "FromBeginning" & "FromElapsedTime": flush everything. "Resume": stand still.
	if ( eBehavior == AkVirtualQueueBehavior_FromBeginning 
        || eBehavior == AkVirtualQueueBehavior_FromElapsedTime )
	{
		StopDecoding();
    }
	m_bIsVirtual = true;
	CAkSrcFileBase::VirtualOn(eBehavior);
}

AKRESULT CAkSrcFileXMA::VirtualOff( AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset )
{
	// "FromBeginning" & "FromElapsedTime": restart stream at proper position.
	// "Resume": just re-enter prebuffering state.
	m_bIsVirtual = false;
	if ( eBehavior == AkVirtualQueueBehavior_Resume )
	{
		return CAkSrcFileBase::VirtualOff(eBehavior, in_bUseSourceOffset);
	}
	else if ( eBehavior == AkVirtualQueueBehavior_FromBeginning )
	{
		m_uCurSample = 0;
		ResetLoopCnt();
	}

	AKASSERT(m_eHwVoiceState != CAkSrcXMABase::HwVoiceState_Allocated);

	WaitForHardwareVoices();
	if (StartDecoding() != AK_Success)
		return AK_Fail;

	return CAkSrcFileBase::VirtualOff(eBehavior, in_bUseSourceOffset);
}

AKRESULT CAkSrcFileXMA::ParseHeader( AkUInt8 * in_pBuffer )
{
	AkFileParser::FormatInfo fmtInfo;
	AkFileParser::SeekInfo seekInfo;
	AkFileParser::AnalysisDataChunk analysisDataChunk;
	AKRESULT eResult = AkFileParser::Parse( in_pBuffer,			// Data buffer
											m_ulSizeLeft,		// Buffer size
											fmtInfo,			// Returned audio format info.
											&m_markers,			// Markers.
											&m_uPCMLoopStart,   // Beginning of loop offset.
											&m_uPCMLoopEnd,		// End of loop offset.
											&m_uDataSize,		// Data size.
											&m_uDataOffset,		// Offset to data.
											&analysisDataChunk,	// Analysis info.
											&seekInfo, 			// Format specific seek table.
											true);

    if ( eResult != AK_Success )
    {
        MONITOR_SOURCE_ERROR( AkFileParser::ParseResultToMonitorMessage( eResult ), m_pCtx );
		return AK_InvalidFile;
    }
	if( m_uDataOffset % 2048 != 0 || 
		( m_uDataSize & 2047 ) )// Data should be a multiple of 2K
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_InvalidAudioFileHeader, m_pCtx );
		return AK_InvalidFile;
	}

	// XMA expects an AkXMA2WaveFormat
	AKASSERT( fmtInfo.uFormatSize == sizeof( AkXMA2WaveFormat ) );
	AkXMA2WaveFormat * pXMAfmt = (AkXMA2WaveFormat*)fmtInfo.pFormat;

	m_XMA2WaveFormat = (AkXMA2WaveFormat&)*fmtInfo.pFormat;

	//Setup format on the PBI

	AkUInt32 dwChannelMask = pXMAfmt->ChannelMask;	
	const WORD uChannels = pXMAfmt->wfx.nChannels;

	// SRC is performed within this source; store sample rate of source, and show 48KHz to the pipeline.
	m_uSourceSampleRate = pXMAfmt->wfx.nSamplesPerSec;

	AkChannelConfig cfg;
	cfg.SetStandard( dwChannelMask ); // supports standard configs only
	AkAudioFormat format;
	format.SetAll(
		AK_CORE_SAMPLERATE, 
		cfg,
		32, 
		sizeof(AkReal32) * uChannels, 
		AK_FLOAT,								
		AK_NONINTERLEAVED);					
	m_pCtx->SetMediaFormat(format);

	// Store analysis data if it was present. 
	// Note: StoreAnalysisData may fail, but it is not critical.
	if ( analysisDataChunk.uDataSize > 0 )
		StoreAnalysisData( analysisDataChunk );


	m_uTotalSamples = pXMAfmt->SamplesEncoded;

	// ignore loop points on xma streams
	m_uPCMLoopStart = 0; //pXMAfmt->LoopBegin;
    m_uPCMLoopEnd   = pXMAfmt->SamplesEncoded - 1; 
	m_ulLoopStart = m_uDataOffset; 
	m_ulLoopEnd = m_uDataOffset + m_uDataSize;

	// Store seek table. XMA 1 is no longer supported
    AKASSERT( m_arSeekTable == NULL );

	if ( seekInfo.uSeekChunkSize == 0
		|| seekInfo.uSeekChunkSize != pXMAfmt->BlockCount * sizeof(AkUInt32) )
    {
        AKASSERT( !"Invalid seek table" );
        return AK_InvalidFile;
    }
    
    m_arSeekTable = (AkUInt32*)AkAlloc( AkMemID_Processing, seekInfo.uSeekChunkSize );
	if( !m_arSeekTable )
		return AK_InsufficientMemory;

	AKPLATFORM::AkMemCpy( m_arSeekTable, seekInfo.pSeekTable, seekInfo.uSeekChunkSize );

    // Update stream heuristics.
	AkAutoStmHeuristics heuristics;
    m_pStream->GetHeuristics( heuristics );
    // Throughput.
	/// IMPORTANT: Must use the header's sample rate because banks can store invalid values (see WG-9838).
	heuristics.fThroughput = ((AkReal32) m_uDataSize * pXMAfmt->wfx.nSamplesPerSec) / ( (AkReal32) m_uTotalSamples * 1000.f );
    // Priority.
	heuristics.priority = m_pCtx->GetPriority();
    // Looping.
    GetStreamLoopHeuristic( DoLoop(), heuristics ); // Sets loop values of AkAutoStmHeuristics according to in_sNumLoop. Does not change the other fields.
	// Buffers.
	heuristics.uMinNumBuffers = 2;
    m_pStream->SetHeuristics( heuristics );

	// Seek to appropriate place.
	AKASSERT( m_XMA2WaveFormat.NumStreams == (uChannels + 1) / 2 
			&& ((m_XMA2WaveFormat.NumStreams & 0xFF) == m_XMA2WaveFormat.NumStreams ) );
	m_uNumXmaStreams = (AkUInt8)m_XMA2WaveFormat.NumStreams;

	m_uCurSample = 0;
	
	// We just set up the decoder (because of playback start or seeking), so we need to reset the buffering status.
	EnterPreBufferingState();

    return m_pStream->SetMinimalBufferSize( m_XMA2WaveFormat.BytesPerBlock );
}

AKRESULT CAkSrcFileXMA::CreateVoices()
{
	m_bReadyForDecoding = false;
	return RegisterHardwareVoices(this);
}

AKRESULT CAkSrcFileXMA::StartXMA()
{	
	AKASSERT(HardwareVoicesRegistered());

	AkUInt32 dwBitOffset[AK_MAX_XMASTREAMS];
	AkUInt32 dwSubframesSkip;	
	AKRESULT eResult = SetXMAOffsets(m_uCurSample, m_uRemainingSamplesAfterSeek, m_ulSizeLeft, m_arSeekTable, m_pNextAddress, m_uNumXmaStreams, dwBitOffset, dwSubframesSkip);
	if (eResult != AK_Success)
	{
		return eResult;
	}

	// Compute pitch.
	// REVIEW Ensure that lower engine parameters are ready at this point. It is not always the case.
	m_pCtx->CalcEffectiveParams();
	AkReal32 fSrcPitch = ComputeSrcPitch();
	AKASSERT( fSrcPitch <= SHAPE_SRC_MAX_PITCH );
	m_iTgtSamplingIncr = m_iPrevSamplingIncr = m_iCurSamplingIncr = PitchToShapeSrcSamplingIncrement(fSrcPitch);

	// XMA
	SHAPE_XMA_SAMPLE_RATE eSampleRate;
	switch ( m_XMA2WaveFormat.wfx.nSamplesPerSec )
	{
	case 24000:
		eSampleRate = SHAPE_XMA_SAMPLE_RATE_24K;
		break;
	case 32000:
		eSampleRate = SHAPE_XMA_SAMPLE_RATE_32K;
		break;
	case 44100:
		eSampleRate = SHAPE_XMA_SAMPLE_RATE_44_1K;
		break;
	case 48000:
	default:
		eSampleRate = SHAPE_XMA_SAMPLE_RATE_48K;
		break;
	}

	const AkAudioFormat &rFormat = m_pCtx->GetMediaFormat();
	
	HRESULT hr;

	// Init XMA decoder / voice.
	for (AkUInt8 i = 0; i < m_uNumXmaStreams; i++)
	{
		AkUInt32 uStreamChannels = 2;
		if ( (rFormat.GetNumChannels() % 2 == 1) && i == (m_uNumXmaStreams-1) )
		{
			uStreamChannels = 1;
		}
		
		// SRC
		SetVoicePitch( fSrcPitch, i );
        
		SetDecoderLooping( 0, i );

		hr = SetShapeXmaNumSubframesToDecode(m_pHwVoice[i]->pXmaContext, 1);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaNumChannels(m_pHwVoice[i]->pXmaContext, uStreamChannels);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaSampleRate(m_pHwVoice[i]->pXmaContext, eSampleRate);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaOutputBufferReadOffset(m_pHwVoice[i]->pXmaContext, 0);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaPacketMetadata(m_pHwVoice[i]->pXmaContext, 0);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaCurrentInputBuffer(m_pHwVoice[i]->pXmaContext, 0);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaOutputBufferValid(m_pHwVoice[i]->pXmaContext, true );
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaNumSubframesToSkip(m_pHwVoice[i]->pXmaContext, dwSubframesSkip);
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaInputBufferReadOffset(m_pHwVoice[i]->pXmaContext, dwBitOffset[i] );
		AKVERIFY( SUCCEEDED( hr ) );

		// Even though the input buffers are invalid, it is necessary to initialize them to 8k to prevent some streams from going out of sync (for some unknown reason).
		hr = SetShapeXmaInputBuffer0Size(m_pHwVoice[i]->pXmaContext, 8*1024 );
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeXmaInputBuffer1Size(m_pHwVoice[i]->pXmaContext, 8*1024 );
		AKVERIFY( SUCCEEDED( hr ) );

		AKASSERT( !GetShapeXmaParserErrorSet(m_pHwVoice[i]->pXmaContext) );

		hr = SetShapeSrcCommand( m_pHwVoice[i]->pSrcContext, SHAPE_SRC_COMMAND_TYPE_START );
		AKVERIFY( SUCCEEDED( hr ) );

		// DMA
		for ( AkUInt32 j = 0; j < uStreamChannels; ++j )
		{
			hr = SetShapeDmaReadPointer( m_pHwVoice[i]->pDmaContext[j], 0 );
			AKVERIFY( SUCCEEDED( hr ) );

			hr = SetShapeDmaWritePointer( m_pHwVoice[i]->pDmaContext[j], 0 );
			AKVERIFY( SUCCEEDED( hr ) );
			
			hr = SetShapeDmaBufferFull( m_pHwVoice[i]->pDmaContext[j], false );
			AKVERIFY( SUCCEEDED( hr ) );

			AKASSERT(m_pDmaBuffer != NULL);
			hr = SetShapeDmaAudioBuffer( m_pHwVoice[i]->pDmaContext[j], m_pDmaBuffer );
			AKVERIFY( SUCCEEDED( hr ) );

			hr = SetShapeDmaNumFrames( m_pHwVoice[i]->pDmaContext[j], AK_NUM_DMA_FRAMES_IN_OUTPUT_BUFFER );
			AKVERIFY( SUCCEEDED( hr ) );

			hr = SetShapeDmaFloatSamples( m_pHwVoice[i]->pDmaContext[j], true );
			AKVERIFY( SUCCEEDED( hr ) );
			
			hr = SetShapeDmaNumChannels( m_pHwVoice[i]->pDmaContext[j], rFormat.GetNumChannels() );
			AKVERIFY( SUCCEEDED( hr ) );

			hr = SetShapeDmaChannel( m_pHwVoice[i]->pDmaContext[j], (i*2)+j ); // 2 channel per stream, expect last one which can be either 1 or 2
			AKVERIFY( SUCCEEDED( hr ) );
		}
	}

	m_uNextXmaSlot = 0;
	m_bReadyForDecoding = true;
	m_bEndOfSource = false;
	
    return eResult;
}

AKRESULT CAkSrcFileXMA::StartStream( AkUInt8 * pvBuffer, AkUInt32 ulBufferSize )
{
	if (!AkACPMgr::Get().IsInitialized())
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_XMACreateDecoderLimitReached, m_pCtx );
		return AK_Fail;
	}

	// Set buffer constraints for XMA.
	AkAutoStmBufSettings bufSettings;
	bufSettings.uBufferSize		= 0;		// No constraint on size.
	bufSettings.uMinBufferSize	= SHAPE_XMA_INPUT_BUFFER_SIZE_ALIGNMENT;
	bufSettings.uBlockSize		= SHAPE_XMA_INPUT_BUFFER_ALIGNMENT;
	AKRESULT res = CAkSrcFileBase::_StartStream(bufSettings);

	if ( res == AK_Success )
		res = StartDecoding();

	return res;
}

AKRESULT CAkSrcFileXMA::StartDecoding()
{
	// Create voices before starting decoding, but do so only once CAkSrcFileBase::StartStream() returns AK_Success.
	// We don't want to acquire voices without being able to consume the data we got with the file header.
	AKRESULT res = CreateVoices();
	if ( res == AK_Success )
	{
		AKASSERT( !m_bReadyForDecoding );
		if ( m_ulSizeLeft != 0 )
		{
			res = AddBuffer();
			UpdateBufferingInfo();	// Update buffering status if applicable.
		}
	}

	return res;
}

AKRESULT CAkSrcFileXMA::SetXMAOffsets( AkUInt32 in_uSampleOffset, AkUInt32 in_uSkipSamples, AkUInt32 in_uBufferSize, DWORD * in_pSeekTable, AkUInt8* pData, AkUInt32 in_uNbStreams, AkUInt32 * out_dwBitOffset, AkUInt32 & out_dwSubframesSkip )
{
	AKASSERT(pData != NULL);

	bool bDataOutOfBounds = false;

	AKRESULT res = AK::XMAHELPERS::FindDecodePosition( pData, in_uBufferSize, in_uSkipSamples, in_uNbStreams, out_dwBitOffset, out_dwSubframesSkip, bDataOutOfBounds );
    if ( res == AK_Success )
    {
		/// m_pCtx->SetSourceOffsetRemainder( uSourceOffset - uRealOffset );
		/// NOTE: SHAPE XMA does not support sample accurate looping from pitch node. This would require decoding one more SHAPE
		/// buffer in order to be ready for these extra consumed samples, and would be inefficient. 
		m_pCtx->SetSourceOffsetRemainder( 0 );
		AKASSERT( !bDataOutOfBounds );

		// Adjust current sample with stored remainder number of samples left over from the find decode position
		m_uCurSample += m_uRemainingSamplesAfterSeek - in_uSkipSamples;
		m_uRemainingSamplesAfterSeek = 0;

		if (bDataOutOfBounds)
		{
			// Trying to seek in a multichannel file and some channel's data is beyond the current stream buffer.
			MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_XMAStreamBufferTooSmall, m_pCtx );	
			res = AK_Fail;
		}

    }
	else
	{
		// Trying to seek in a multichannel file and some channel's data is beyond the current stream buffer.
        MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_XMAStreamBufferTooSmall, m_pCtx );	
	}

	return res;
}

AKRESULT CAkSrcFileXMA::ChangeSourcePosition()
{
	//To seek into an active voice, destroy and re-init the HW voice.	
	//Virtual voices will start in the correct position when they become real.
	
	bool bWasDecoding = m_eHwVoiceState == HwVoiceState_Allocated;
	
	//Stop the decoder if necessary.
	StopDecoding();
	WaitForHardwareVoices();

	AKRESULT eResult = CAkSrcFileBase::ChangeSourcePosition();
	if (bWasDecoding && eResult == AK_Success)
		return StartDecoding();
	
	return eResult;
}

// IO/buffering status notifications. 
void CAkSrcFileXMA::NotifySourceStarvation()
{
	//MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_XMADecoderSourceStarving, m_pCtx );
	MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_StreamingSourceStarving, m_pCtx );
}

// Finds the closest offset in file that corresponds to the desired sample position.
// Returns the file offset (in bytes, relative to the beginning of the file) and its associated sample position.
// Returns AK_Fail if the codec is unable to seek.
AKRESULT CAkSrcFileXMA::FindClosestFileOffset( //todo: rename since it's not const anymore
	AkUInt32 in_uDesiredSample,		// Desired sample position in file.
	AkUInt32 & out_uSeekedSample,	// Returned sample where file position was set.
	AkUInt32 & out_uFileOffset		// Returned file offset where file position was set.
	)
{
	AkUInt32 blockIndex;
	DWORD * pSeekTable = m_arSeekTable;

	out_uFileOffset = m_uDataOffset + RoundToBlock( in_uDesiredSample, pSeekTable, blockIndex );

	if (blockIndex == 0)
		out_uSeekedSample = 0;
	else
		out_uSeekedSample = pSeekTable[blockIndex-1];

	m_uRemainingSamplesAfterSeek = in_uDesiredSample - out_uSeekedSample;

	return AK_Success;
}

DWORD CAkSrcFileXMA::RoundToBlock( AkUInt32 in_uSampleOffset, DWORD * in_pSeekTable, AkUInt32& out_blockIndex )
{
	// Find block in seek table.
	int i = 0;

    while ( in_uSampleOffset > in_pSeekTable[i] ) // test seeking on last block boundary.
        ++i;

	out_blockIndex = i;
	return m_XMA2WaveFormat.BytesPerBlock * i; 
}