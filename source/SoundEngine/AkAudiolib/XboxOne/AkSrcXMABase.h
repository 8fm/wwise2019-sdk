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

#ifndef _AKSRCXMABASE_H
#define _AKSRCXMABASE_H

#include "AkXMAHelpers.h"
#include "AkACPMgr.h"
#include "AkLEngine.h"

// Need to switch SRC mode to STOP_END before last frame. Experience has shown that SHAPE_FRAME_SIZE * src_ratio was 
// too short in rare cases. So we switch 4 frames before. It doesn't seem to have negative effects.
#define AK_NUM_SHAPE_FRAMES_STOP_BEFORE_END		(4 * SHAPE_FRAME_SIZE + 8)

//#define AK_MONITOR_XMA_STREAM
//#define AK_MONITOR_XMA_STREAM_BUFFERES

template <class T_BASE>
class CAkSrcXMABase : public T_BASE
{
public:

	CAkSrcXMABase( CAkPBI * in_pCtx )
		: T_BASE( in_pCtx )
		, m_pDmaBuffer( NULL )
		, m_iCurSamplingIncr( PitchToShapeSrcSamplingIncrement(1.f) )
		, m_iPrevSamplingIncr( PitchToShapeSrcSamplingIncrement(1.f) )
		, m_iTgtSamplingIncr( PitchToShapeSrcSamplingIncrement(1.f) )
		, m_uNumXmaStreams( 0 )
		, m_uNumShapeFramesAvailable( 0 )
		, m_uNumShapeFramesGranted( 0 )
		, m_bEndOfSource( false )
		, m_bFatalError( false )
		, m_eHwVoiceState(HwVoiceState_Free)
	{
		for (AkUInt32 i = 0; i < AK_MAX_XMASTREAMS; i++)
			m_pHwVoice[i] = NULL;
	}

	virtual ~CAkSrcXMABase()
	{
		AKASSERT(m_eHwVoiceState == HwVoiceState_Free);
		AKASSERT( !m_pHwVoice[0] && !m_pDmaBuffer );	// Should have all been freed already in StopStream.
	}

	// Process XMA voices last to give a change to other codecs to run while the hardware does its thing.
	virtual AkInt32 SrcProcessOrder() { return SrcProcessOrder_HwVoice; }

	// Return false if flowgraph execution is not needed. 
	bool OnFlowgraphPrepare( 
		AkUInt32 in_uShapeFrameIdx, AkUInt32 in_uStreamIndex
		)
	{
		AKASSERT(m_eHwVoiceState == HwVoiceState_Allocated);

		AkACPVoice* pVoice = m_pHwVoice[in_uStreamIndex];

		// Reset m_uNumShapeFramesAvailable to force updating the source at next GetBuffer().
		m_uNumShapeFramesAvailable = m_uNumShapeFramesGranted;

		if ( pVoice->uDmaFramesWritten == (AkUInt32)(AK_NUM_DMA_FRAMES_PER_BUFFER) || 
			 in_uShapeFrameIdx == GetNumFlowgraphsToSubmit() )
		{
#ifdef AK_MONITOR_XMA_STREAM
			//#MONITOR_MSG
			char chbuf[256]; sprintf(chbuf,">> stream %i FGs <<%u>> (uDmaFramesWritten: %u)  \n", in_uStreamIndex, in_uShapeFrameIdx, pVoice->uDmaFramesWritten );	MONITOR_MSG(chbuf);
#endif
			return false;	// We're full or ran out of data
		}

		// Compute pitch target for next SHAPE frame as linear interpolation between prev and next Wwise frame.
		AKASSERT( pVoice->uDmaFramesWritten < (AkUInt32)(AK_NUM_DMA_FRAMES_PER_BUFFER) );

#if AK_SHAPE_SRC_PITCH_RAMP
		//Interpolate over AK_NUM_DMA_FRAMES_PER_BUFFER flowgraph.  This seem to not work, SHAPE just process all the SRC update commands at once.
		AkInt32 iIncrementPerShapeFrame = (m_iTgtSamplingIncr - m_iPrevSamplingIncr)) / (AkInt32)AK_NUM_DMA_FRAMES_PER_BUFFER;
		AkInt32 iTargetSamplingIncrement = m_iPrevSamplingIncr + (((AkInt32)in_uShapeFrameIdx+1) * iIncrementPerShapeFrame;
		AkInt32 iCurrentSamplingIncrement = m_iPrevSamplingIncr + (((AkInt32)in_uShapeFrameIdx) * iIncrementPerShapeFrame;
#else
		//Perform pitch interpolation all at once in first flowgraph.
		AkInt32 iTargetSamplingIncrement = m_iTgtSamplingIncr;
		AkInt32 iCurrentSamplingIncrement = m_iPrevSamplingIncr;
		if ( in_uShapeFrameIdx > 0 )
			iCurrentSamplingIncrement = m_iTgtSamplingIncr;
#endif
		AkUInt32 uSrcSubSamplesConsumedThisPass = SHAPEHELPERS::IncrementSubSamplesConsumedBySRC( pVoice->uSrcSubSamplesConsumed, iCurrentSamplingIncrement, iTargetSamplingIncrement );
		
		// Handle SRC mode change.
		if ( !DoLoop() )
		{
			AkUInt32 uSamplesConsumedThisPass = uSrcSubSamplesConsumedThisPass/SHAPE_Q18;
			if ( m_uCurSample + uSamplesConsumedThisPass + AK_NUM_SHAPE_FRAMES_STOP_BEFORE_END >= m_uTotalSamples )
			{	
				pVoice->SwitchToStopEndMode(); //Tell the SRC to not hang if there is no more data, as we will soon arrive at the end of the file.
				
				AkUInt32 uSamplesConsumedLastPass = (pVoice->uSrcSubSamplesConsumed / SHAPE_Q18);
				if ( m_uCurSample + uSamplesConsumedLastPass >= m_uTotalSamples )
					return false;	// Return false so as to not submit any more flowgraphs.
			}
		}

		//Enqueue the pitch update command.
		AkACPMgr::Get().UpdatePitchTarget(pVoice, in_uShapeFrameIdx, iTargetSamplingIncrement);

		//Update voice src samples consumed.
		pVoice->uSrcSubSamplesConsumed = uSrcSubSamplesConsumedThisPass;

		//Update m_uCurSamplingIncr with the last value we sent to SHAPE, so we know what is going on in SHAPE-land.
		m_iCurSamplingIncr = iTargetSamplingIncrement;

		return true;
	}

	AkUInt32 GetNumFlowgraphsToSubmit()
	{
		AKASSERT(m_eHwVoiceState == HwVoiceState_Allocated);
		AkUInt8 uNumFlographs = AK_NUM_DMA_FRAMES_PER_BUFFER;
		for (AkUInt8 i = 0; i < m_uNumXmaStreams; i++)
			uNumFlographs = AkMin(uNumFlographs, m_pHwVoice[i]->uMaxFlowgraphsToRun );
		return uNumFlographs;
	}

	// Data access.
	virtual AKRESULT StopLooping()
	{

		for (AkUInt32 i = 0; i < m_uNumXmaStreams; i++)
		{
			if ( m_pHwVoice[i] )
			{ 
				AkACPMgr::Get().UpdateXmaLoopCount(m_pHwVoice[i], 1);
			}
		}
		return T_BASE::StopLooping();
	} 

	virtual void	 ReleaseBuffer()
	{
		if (m_uNumShapeFramesGranted > 0)
		{
			for (AkUInt32 i = 0; i < m_uNumXmaStreams; i++)
			{
				if ( m_pHwVoice[i] )
				{
					// Update user-side DMA read pointer.
					m_pHwVoice[i]->uDmaReadPointer += m_uNumShapeFramesGranted;
					AkUInt8 uNumShapeFrames = m_pHwVoice[i]->pDmaContext[0]->numFrames + 1;
					if ( m_pHwVoice[i]->uDmaReadPointer >= uNumShapeFrames )
						m_pHwVoice[i]->uDmaReadPointer -= uNumShapeFrames;

					AKASSERT( m_pHwVoice[i]->uDmaReadPointer < uNumShapeFrames );

					// Submit commands to advance DMA read pointer on SHAPE side.
					AkACPMgr::Get().IncrementDmaPointer(m_pHwVoice[i]->pDmaContext[0], m_uNumShapeFramesGranted);
					if ( m_pHwVoice[i]->uNumChannels > 1 )
						AkACPMgr::Get().IncrementDmaPointer(m_pHwVoice[i]->pDmaContext[1], m_uNumShapeFramesGranted);
				}
			}
			AKASSERT( m_uNumShapeFramesAvailable >= m_uNumShapeFramesGranted );
			m_uNumShapeFramesAvailable -= m_uNumShapeFramesGranted;
			m_uNumShapeFramesGranted = 0;
		}
	}

	virtual AKRESULT TimeSkip( AkUInt32 & io_uFrames )	// Override TimeSkip(): need to implement pitch scaling (pitch node won't do it for us).
	{
		// Compute number of samples to advance on the source side based on pitch.
		// Return io_uFrames untouched: from the pipeline/pitch node point of view we produced io_uFrames just as we were asked.
		AkUInt32 uNumSamplesSrc = (AkUInt32)( io_uFrames * ComputeSrcPitch() );
		return CAkSrcBaseEx::TimeSkip( uNumSamplesSrc );
	}
	virtual AkReal32 GetPitch() const { return 0.f; }			// Override GetPitch: SRC is executed here.

	// Override GetSourceSampleRate(); default implementation returns core sample rate (unwanted), because XMA sources handle SRC.
	virtual AkUInt32 GetSourceSampleRate() const { return m_uSourceSampleRate; }

	inline AkACPVoice* GetAcpVoice(AkUInt32 in_uIdx)
	{
		return m_pHwVoice[in_uIdx];
	}

	inline AkUInt32 GetNumStreams() const
	{
		return (AkUInt32)m_uNumXmaStreams;
	}

protected:

	// Services

	AKRESULT RegisterHardwareVoices(IAkSHAPEAware* pThis)
	{
		AKASSERT(m_eHwVoiceState == HwVoiceState_Free);

		m_bFatalError = false;

		const AkUInt32 uTotalNumChannels = m_pCtx->GetMediaFormat().GetNumChannels();

		if (!m_pDmaBuffer)
		{
			m_pDmaBuffer = (AkReal32 *)AkACPMgr::Get().GetApuBuffer(AK_DMA_BUFFER_SIZE_PER_CHANNEL * uTotalNumChannels, SHAPE_DMA_ADDRESS_ALIGNMENT);
			if (!m_pDmaBuffer)
				return AK_InsufficientMemory;
		}

		AKRESULT eResult = AK_Success;

		// Init pitch
		m_pCtx->CalcEffectiveParams();
		AkReal32 fSrcPitch = ComputeSrcPitch();
		AKASSERT(fSrcPitch >= SHAPE_SRC_MIN_PITCH);
		AKASSERT(fSrcPitch <= SHAPE_SRC_MAX_PITCH);
		m_iTgtSamplingIncr = m_iPrevSamplingIncr = m_iCurSamplingIncr = PitchToShapeSrcSamplingIncrement(fSrcPitch);
		
		for (AkUInt8 i = 0; i < m_uNumXmaStreams; i++)
		{
			AKASSERT(m_pHwVoice[i] == NULL);

			AkUInt32 uStreamChannels = 2;
			if ((uTotalNumChannels % 2) == 1 && i == (m_uNumXmaStreams - 1))
				uStreamChannels = 1;

			eResult = AkACPMgr::Get().RegisterVoice(pThis, &m_pHwVoice[i], uStreamChannels, i, fSrcPitch);
			if(eResult != AK_Success )
			{
				break;
			}
		}

		if (eResult != AK_Success)
		{
			MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_XMACreateDecoderLimitReached, m_pCtx);
			
			AkACPMgr::Get().CleanUpVoices(m_pHwVoice, m_uNumXmaStreams);
			
			//Make sure to free any resources
			m_eHwVoiceState = HwVoiceState_Releasing;
			pThis->HwVoiceFinished();
			AKASSERT(m_eHwVoiceState == HwVoiceState_Free);

			return AK_Fail;
		}

		m_eHwVoiceState = HwVoiceState_Allocated;
		return AK_Success;
	}

	void FreeHardwareVoices()
	{
		if (m_eHwVoiceState == HwVoiceState_Allocated)
		{
			m_eHwVoiceState = HwVoiceState_Releasing;
			AkACPMgr::Get().UnregisterVoices(m_pHwVoice, m_uNumXmaStreams);
		}
	}

	void WaitForHardwareVoices()
	{
		if (m_eHwVoiceState == HwVoiceState_Releasing)
		{
			AkACPMgr::Get().WaitForHwVoices();
		}
	}

	bool HardwareVoicesRegistered() const 
	{
		return (m_eHwVoiceState == HwVoiceState_Allocated);
	}

	// Called by AcpMgr when it is safe to free resources needed by a HW voice.
	void HwVoiceFinished()
	{
		AKASSERT(m_eHwVoiceState != HwVoiceState_Allocated);
#ifdef XMA_CHECK_VOICE_SYNC
		AKASSERT(!AkACPMgr::Get().VoiceIsInFlowgraph(m_pHwVoice[0]));
#endif
		
		//It is expected to get multiple calls to DecodingFinished(), one per hw stream.
		if (m_eHwVoiceState == HwVoiceState_Releasing)
		{
			if (m_pDmaBuffer)
			{
				AkACPMgr::Get().ReleaseApuBuffer(m_pDmaBuffer);
				m_pDmaBuffer = NULL;
			}

			for (AkUInt32 i = 0; i < m_uNumXmaStreams; i++)
				m_pHwVoice[i] = NULL;

			m_eHwVoiceState = HwVoiceState_Free;
		}
	}
	
	void SetFatalError()
	{
		MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_XMADecodingError, GetContext());
		m_bFatalError = true;
	}

	// Perform source update, once per Wwise audio frame. Returns AK_Fail if there was a problem, AK_Success otherwise.
	AKRESULT UpdateSource( AkPipelineBuffer & io_buffer )
	{
		if (m_uNumShapeFramesAvailable)
			return AK_Success;
		else if (m_bFatalError)
			return AK_Fail;

		// Update pitch.
		UpdatePitch( ComputeSrcPitch() );

		// Refresh number of valid frames in DMA buffer.
		m_uNumShapeFramesAvailable = ValidDmaFrames();
		if ( m_uNumShapeFramesAvailable == 0)
		{
			// No frames are ready at the DMA output. This can happen when becoming physical, seeking, and so on.
			return AK_Success;
		}

		// Compute number of _source_ samples that has been decoded since last GetBuffer() and update uTotalDecodedSamples.
		// NOTE: Not strictly greater; case with 0 sample advance by the XMA decoder needs to be supported.
		AkUInt32 uProducedSamples = m_pHwVoice[0]->GetSamplesProducedSinceLastUpdate();
		m_pHwVoice[0]->ResetSamplesProduced();

		for (unsigned int i = 1; i < m_uNumXmaStreams; i++)
		{
			m_pHwVoice[i]->ResetSamplesProduced();

			if (m_pHwVoice[i]->uTotalDecodedSamples != m_pHwVoice[0]->uTotalDecodedSamples ||
				m_pHwVoice[i]->uSrcSubSamplesConsumed != m_pHwVoice[0]->uSrcSubSamplesConsumed)
			{
				//The streams have gone out of sync. Bail out
				return AK_Fail;
			}
		}

		// Got data; steady-state mode.
		LeavePreBufferingState();
		AKRESULT eState = AK_DataReady;

		AkUInt32 uStartPos = m_uCurSample;

		while( uProducedSamples > 0 && eState == AK_DataReady)
		{
			AkUInt16 uSamplesProducedBeforeLoop = (AkUInt16)uProducedSamples;
			ClampRequestedFrames( uSamplesProducedBeforeLoop );
			
			AKASSERT( m_uCurSample + uSamplesProducedBeforeLoop <= m_uTotalSamples );

			AppendMarkers(uSamplesProducedBeforeLoop, io_buffer);

			// Update.
			m_uCurSample += uSamplesProducedBeforeLoop;
			eState = HandleLoopingOrEndOfFile();
			
			uProducedSamples -= uSamplesProducedBeforeLoop;
		}

		AKASSERT( eState != AK_Fail );
		if( eState == AK_NoMoreData )
			m_bEndOfSource = true;

		return AK_Success;
	}

	/** TODO Factorize InitXMA and CreateVoices.
	AKRESULT		CreateVoices();
	**/

	inline AkReal32	ComputeSrcPitch()
	{
		AkReal32 fSampleRateConvertRatio = (AkReal32) m_uSourceSampleRate / (AkReal32) AK_CORE_SAMPLERATE;
		AkReal32 fSrcPitch = ( ( fSampleRateConvertRatio * powf( 2.f, m_pCtx->GetEffectiveParams().Pitch() / 1200.f ) ) );
		fSrcPitch = AkClamp( fSrcPitch, 1.f/4.f, SHAPE_SRC_MAX_PITCH ); // SHAPE max pitch is slightly smaller than Wwise's.
		return fSrcPitch;
	}

	inline void	SetVoicePitch( AkReal32 in_fPitch, AkUInt32 in_uStreamIndex )
	{
		AkACPVoice* pVoice = m_pHwVoice[in_uStreamIndex];
		AKASSERT( pVoice );

		HRESULT hr = SetShapeSrcPitch( pVoice->pSrcContext, in_fPitch );
		AKVERIFY( SUCCEEDED( hr ) );

		hr = SetShapeSrcPitchTarget( pVoice->pSrcContext, in_fPitch );
		AKVERIFY( SUCCEEDED( hr ) );
	}

	// Note: HW context must exist.
    inline void	SetDecoderLooping( AkUInt16 in_uLoopCount, AkUInt32 in_uStreamIndex )
	{
		AKASSERT( m_pHwVoice[in_uStreamIndex] );
		UINT32 numLoops = ( in_uLoopCount == LOOPING_INFINITE ) ? SHAPE_XMA_INFINITE_LOOP_COUNT : AkMin( SHAPE_XMA_MAX_LOOP_COUNT, in_uLoopCount-1 );
		AKASSERT( !m_pHwVoice[in_uStreamIndex]->IsXmaCtxEnable() );
		HRESULT hr = SetShapeXmaNumLoops( m_pHwVoice[in_uStreamIndex]->pXmaContext, numLoops );
		AKVERIFY( SUCCEEDED( hr ) );
	}

	inline void AppendMarkers(AkUInt16 in_uNumSamples, AkPipelineBuffer & out_buffer)
	{
		// Copy markers to the output buffer if applicable.
		// Note: here we will copy all markers corresponding to the source's sample increment during this frame
		AkPipelineBuffer bufferForMarkers;
		bufferForMarkers.ResetMarkers();
		SaveMarkersForBuffer(bufferForMarkers, m_uCurSample, in_uNumSamples);
		CAkMarkers::TransferRelevantMarkers(T_BASE::GetContext()->GetCbx()->GetPendingMarkersQueue()->Front(), &bufferForMarkers, &out_buffer, 0, in_uNumSamples);
	}

	// Valid DMA frames consumable by pipeline downstream is the minimum value of interleaved multichannel XMA streams.
	inline AkUInt8 ValidDmaFrames()
	{
		AkUInt32 validframes = m_pHwVoice[0]->uDmaFramesWritten;
		for (int i = 1; i < m_uNumXmaStreams; i++)
		{
			if (validframes > m_pHwVoice[i]->uDmaFramesWritten) 
				validframes = m_pHwVoice[i]->uDmaFramesWritten;
		}

		return (AkUInt8) validframes;	
	}

	inline void	ConsumeDmaFrames()
	{
		AkUInt32 validframes = 0;
		for (int i = 0; i < m_uNumXmaStreams; i++)
		{
			// Update user-side DMA write pointer now so that SHAPE can fill up the extra buffer in-between Wwise frames.
			// This is important if the pitch node still holds a buffer (hence the extra SHAPE frame bytes in our output buffer).
			AKASSERT(m_pHwVoice[i]->uDmaFramesWritten >= m_uNumShapeFramesGranted);
			m_pHwVoice[i]->uDmaFramesWritten -= m_uNumShapeFramesGranted;
		}
	}

	inline void UpdatePitch(AkReal32 in_fNewPitchRatio)
	{
		AKASSERT( in_fNewPitchRatio <= SHAPE_SRC_MAX_PITCH );
		m_iTgtSamplingIncr = PitchToShapeSrcSamplingIncrement(in_fNewPitchRatio);
		m_iPrevSamplingIncr = m_iCurSamplingIncr;
	}

	AkReal32 GetCurrentPitchRatio() const
	{
		return ShapeSrcSamplingIncrementToPitch(m_iCurSamplingIncr);
	}

	AkReal32 GetNextPitchRatio(AkUInt32 in_uNumShapeFrames = AK_NUM_DMA_FRAMES_PER_BUFFER) const
	{
		return ShapeSrcSamplingIncrementToPitch(m_iTgtSamplingIncr);
	}

	// XMA
	AkACPVoice *	m_pHwVoice[AK_MAX_XMASTREAMS];
	AkReal32 *		m_pDmaBuffer;

	AkUInt32		m_uSourceSampleRate;
	AkInt32			m_iPrevSamplingIncr;
	AkInt32			m_iCurSamplingIncr;				// The last sampling increment that we sent to the SRC context.
	AkInt32			m_iTgtSamplingIncr;
	AkUInt8			m_uNumXmaStreams;
	AkUInt8			m_uNumShapeFramesAvailable;		// Number of shape frames available in DMA buffer.
	AkUInt8			m_uNumShapeFramesGranted;		// Number of shape frames fed to pipeline. Set at GetBuffer() reset at ReleaseBuffer().
	AkUInt8			m_bEndOfSource				:1;	// Set once we called UpdateSource() and source reached its end and should stop. Will return AK_NoMoreData.
	AkUInt8			m_bFatalError				:1;

	enum HwVoiceState
	{
		HwVoiceState_Free,
		HwVoiceState_Allocated,
		HwVoiceState_Releasing
	};
	HwVoiceState	m_eHwVoiceState;
};

#endif // _AKSRCXMABASE_H
