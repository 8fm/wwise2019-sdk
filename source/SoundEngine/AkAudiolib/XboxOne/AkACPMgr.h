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

/// \file 
/// ACP HAL manager.

#pragma once

//#define XMA_DEBUG_OUTPUT
#if defined (_DEBUG) && defined(AK_ENABLE_ASSERTS)
#define XMA_CHECK_VOICE_SYNC
#endif

#ifndef _DEBUG
#define NO_SHAPE_CONTEXT_VALIDATION
#endif

#include <acphal.h>
#include <AK/Tools/Common/AkListBareLight.h>
#include "AkLEngineCmds.h"
#include "AkStaticArray.h"
// Defines

#define AK_NUM_DMA_FRAMES_PER_BUFFER	(unsigned int)(AK_NUM_VOICE_REFILL_FRAMES / SHAPE_FRAME_SIZE)

// Need an extra SHAPE frame of buffering to hold incomplete buffers exposed to pitch node across Wwise audio frames.
// The DMA read and write circle around this buffer. But from the ACP Mgr point of view, we are full as soon as we 
// have AK_NUM_DMA_FRAMES_PER_BUFFER DMA'ed frames .
#define AK_NUM_DMA_FRAMES_IN_OUTPUT_BUFFER		(AK_NUM_DMA_FRAMES_PER_BUFFER+1)

#define AK_DMA_BUFFER_SIZE_PER_CHANNEL	(SHAPE_FRAME_SIZE * sizeof(AkReal32 ) * AK_NUM_DMA_FRAMES_IN_OUTPUT_BUFFER)

#define AK_SHAPE_SUB_SAMPLE_MASK 0x3ffff

 // Ramp over AK_NUM_DMA_FRAMES_PER_BUFFER SHAPE frames (1) or ramp over 1 frame (0)
#define AK_SHAPE_SRC_PITCH_RAMP  0

class AkACPVoice;

class IAkSHAPEAware
{
public:
	virtual void UpdateInput(AkUInt32 in_uStreamIdx) = 0;
	virtual bool OnFlowgraphPrepare( AkUInt32 in_uPass, AkUInt32 in_uStreamIndex ) = 0;
	virtual bool NeedsUpdate() = 0;
	virtual void HwVoiceFinished() = 0;
	virtual void SetFatalError() = 0;
	virtual AkACPVoice* GetAcpVoice(AkUInt32 in_uIdx) = 0;
	virtual AkUInt32 GetNumStreams() const = 0;
};

class AkACPVoice
{
public:

	inline void UpdateInput(){ 	pOwner->UpdateInput(uStreamIndex); }
	inline bool OnFlowgraphPrepare( AkUInt32 in_uPass ) { return pOwner->OnFlowgraphPrepare( in_uPass, uStreamIndex ); }
	inline bool NeedsUpdate() { return pOwner->NeedsUpdate(); }

	AkACPVoice( SHAPE_XMA_CONTEXT * in_pXmaContext, SHAPE_SRC_CONTEXT * in_pSrcContext, SHAPE_DMA_CONTEXT * in_pDmaContext0, SHAPE_DMA_CONTEXT * in_pDmaContext1, void * in_pOutputBuffer, void * in_pOverlapBuffer ) 
		: pXmaContext( in_pXmaContext )
		, pSrcContext( in_pSrcContext )
		, pOwner( NULL )
		, m_pOutputBuffer( in_pOutputBuffer )
		, m_pOverlapBuffer( in_pOverlapBuffer )
		, m_bXmaCtxEnable( true ) // assuming enabled by default, need to force disable
	{
		pDmaContext[0] = in_pDmaContext0;
		pDmaContext[1] = in_pDmaContext1;		
	}

	inline AkUInt32 GetSamplesProducedSinceLastUpdate()
	{
		AKASSERT( pSrcContext->sampleCount >= uTotalDecodedSamples );
		AkUInt32 uSamplesProducedSinceLastUpdate = pSrcContext->sampleCount - uTotalDecodedSamples;
		AKASSERT( uSrcSubSamplesConsumed == uSamplesProducedSinceLastUpdate*SHAPE_Q18 + (pSrcContext->samplePointer & AK_SHAPE_SUB_SAMPLE_MASK) || 
					( pSrcContext->samplePointer==0 )	// uSrcSubSamplesConsumed will overshoot on the last frame of a given source, 
														//	samplePointer will be reset to zero, so dont assert.
					);
		return uSamplesProducedSinceLastUpdate;
	}

	inline void ResetSamplesProduced()
	{
		uSrcSubSamplesConsumed = (pSrcContext->samplePointer & AK_SHAPE_SUB_SAMPLE_MASK);
		uTotalDecodedSamples = pSrcContext->sampleCount;
	}

	inline bool IsInStopEndMode()
	{
		return GetShapeSrcCommandType(pSrcContext) == SHAPE_SRC_COMMAND_TYPE_STOP_END;
	}

	inline void SwitchToStopEndMode()
	{
		DWORD hr = SetShapeSrcCommand( pSrcContext, SHAPE_SRC_COMMAND_TYPE_STOP_END );
		AKASSERT( SUCCEEDED( hr ) );
	}

	AKRESULT Reset( IAkSHAPEAware * in_pOwner, AkUInt32 in_uNumChannels, AkUInt8 in_uStreamIndex, AkReal32 in_fPitch );

	AkACPVoice * pNextLightItem; // for AkACPMgr::m_voices

	SHAPE_DMA_CONTEXT * pDmaContext[2];
	SHAPE_SRC_CONTEXT * pSrcContext;
	SHAPE_XMA_CONTEXT * pXmaContext;

    bool IsXmaCtxEnable() { return m_bXmaCtxEnable; }

	void *	m_pOutputBuffer;
	void *	m_pOverlapBuffer;

	IAkSHAPEAware * pOwner;

	AkUInt32 uTotalDecodedSamples;
	AkUInt32 uSrcSubSamplesConsumed; // number of fractional samples that has been consumed by the SRC since last update.

	AkUInt8 uMaxFlowgraphsToRun; // number of SHAPE frames available on the input side.
	AkUInt8 uDmaReadPointer; // dma read pointer, sync updated from audio thread
	AkUInt8 uDmaFramesWritten;	// number of dma frames produced (1 frame == SHAPE_FRAME_SIZE samples), ready to be consumed by the audio thread
	AkUInt8 uStreamIndex;

	AkUInt8 uNumChannels	:2; // 1 or 2
	AkUInt8	m_bXmaCtxEnable	:1;
	AkUInt8	m_bStopXmaCtxForUpdate	:1;
};

struct AkShapeFrame
{
	AkShapeFrame():	m_uSubmittedVoiceCount(0)
			,m_uFlowgraphCommandsCount(0)
			,m_uMixBufferCount(0)
			,m_uSrcUpdateCommandsCount(0)
			,m_pFlowgraphCommands(nullptr)
			,m_pSrcUpdateCommands(nullptr)
	{ 
		Reset();
	}

	AKRESULT Alloc( AkMemPoolId in_poolIdShape );
	void Free( AkMemPoolId in_poolIdShape );
	void Reset();

	void SubmitVoiceToProcess( UINT32 uNumChannels, UINT32 uXmaContextId, UINT32 uSrcContextId, UINT32 uDmaContextId0, UINT32 uDmaContextId1);
	bool LaunchFlowgraph(IAcpHal* in_pAchHal);

	AkUInt32 m_uSubmittedVoiceCount;
	AkUInt32 m_uFlowgraphCommandsCount;
	AkUInt32 m_uMixBufferCount;
	AkUInt32 m_uSrcUpdateCommandsCount;

	SHAPE_FLOWGRAPH_COMMAND* m_pFlowgraphCommands;
	ACP_COMMAND_UPDATE_SRC_CONTEXT_ENTRY* m_pSrcUpdateCommands;
	
#ifdef XMA_CHECK_VOICE_SYNC
	UINT32 m_bfVoicesInFlowgraph[SHAPE_NUM_XMA_CONTEXT_BITFIELDS];
	bool VoiceIsInFlowgraph(UINT32 uXmaContextId)	{	return (m_bfVoicesInFlowgraph[uXmaContextId >> SHAPE_XMA_CONTEXT_INDEX_SHIFT] & (1 << (uXmaContextId & SHAPE_XMA_CONTEXT_BIT_MASK))) != 0;	}
#endif
	bool m_bLaunched;
};


class AkACPMgr
{
public:
	AkACPMgr();

	AKRESULT Init();
	void Term();

	void Update();

	inline bool IsInitialized() { return m_bIsInitialized; }
	
	AKRESULT RegisterVoice( IAkSHAPEAware * in_pOwner, AkACPVoice ** out_ppVoice, AkUInt32 in_uNumChannels, AkUInt8 in_uStreamIndex, AkReal32 in_fPitch );
	void UnregisterVoices( AkACPVoice ** in_pVoiceArray, AkUInt32 in_uNumVoices );
	
	// CleanUpVoices() is to be used only when allocation of a voice failes. It assumes a vaoice was never actually used, and does not recycle voices or call DecodingFinished().
	void CleanUpVoices(AkACPVoice ** in_pVoiceArray, AkUInt32 in_uNumVoices);

	void IncrementDmaPointer( SHAPE_DMA_CONTEXT * in_pDmaContext, AkUInt32 in_uNumFramesIncrement );
	void UpdatePitchTarget(AkACPVoice* in_pVoice, AkUInt32 in_uShapeFrameIdx, AkUInt32 in_uTargetSamplingIncr);
	void UpdateXmaLoopCount(AkACPVoice* in_pVoice, AkUInt32 in_uLoopCount);

	void WaitForFlowgraphs();

	// Wait for flowgraphs to finish, recycle all voices and stop XMA contexts of voices no longer in use.
	void WaitForHwVoices();

	void * GetApuBuffer(AkUInt32 in_uSize, AkUInt32 in_uAlign);
	void ReleaseApuBuffer(void * in_pvBuffer);

	static inline AkACPMgr& Get() { return s_AcpMgr; }

private:
	void WaitForFlowgraphsNoLock();

	void SubmitVoiceToProcess(AkACPVoice * in_pVoice, AkUInt32 in_uShapeFrameIdx);
	void WaitForCommandCompletion();

	void StopXmaContext_Sync(AkACPVoice* in_pVoice);
	void StartXmaContext_Async(AkACPVoice* in_pVoice);

	void LaunchFlowgraph(AkUInt32 in_uFgIdx);
	void Clear();
	void RegisterForFlowgraphMessages();

	bool EnableDisableXmaContextsForInputUpdate(bool in_bEnable);
	void ProcessMessageQueue();

	void RecycleVoices();
	bool StopRecycledXmaContexts();
	void FinishRecycleAndNotify();

    SHAPE_CONTEXTS m_contexts;

    IAcpHal * m_pAcpHal;

	AkACPVoice * m_pVoices; // base address of voices memory allocation
	
	AkListBareLight<AkACPVoice> m_freeVoices;
	AkListBareLight<AkACPVoice> m_usedVoices;
	AkListBare<AkACPVoice,AkListBareLightNextItem,AkCountPolicyWithCount,AkLastPolicyNoLast> m_recycledVoices;
	
	void * m_pXmaMem;
	AkUInt32 m_uNumPendingDMAReadIncrement;
	AkUInt32 m_uNumCtxEnableDisable;
	volatile AkUInt32 m_uNumFlowgraphsRunning;
	AkUInt32 m_uFgIdxNextFrame;
	AkUInt32 m_uFgIdxRunning;
	bool m_bIsInitialized;

	CAkLock m_lockWait;

	AkShapeFrame* m_frames;

#ifdef AK_ENABLE_ASSERTS
	AkUInt32 m_bSpinCount;
#endif
#ifdef XMA_CHECK_VOICE_SYNC
	void CheckAcpVoiceSync(AkACPVoice* pVoice);
public:
	bool VoiceIsInFlowgraph(AkACPVoice* pVoice);
#endif

	static AkACPMgr	s_AcpMgr;
};


namespace AK
{
	namespace SHAPEHELPERS
	{

	inline AkUInt32 GetNumSamplesConsumedBySRC( AkUInt32 in_uSamplingIncr, AkUInt32 in_uTargetSamplingIncr, AkUInt32& io_uSampleFraction )
	{
		AkUInt32 uNewSample = io_uSampleFraction + (( 127 * in_uTargetSamplingIncr + 129 * in_uSamplingIncr ) / 2);
		io_uSampleFraction = uNewSample % SHAPE_Q18;
		return uNewSample/SHAPE_Q18;
	}

	inline AkUInt32 IncrementSubSamplesConsumedBySRC( AkUInt32 in_uCurSubSample, AkUInt32 in_uSamplingIncr, AkUInt32 in_uTargetSamplingIncr )
	{
		return in_uCurSubSample + (( 127 * in_uTargetSamplingIncr + 129 * in_uSamplingIncr ) / 2);
	}

	//Given a number of input frames, how many flowgraphs can we run without starving?
	// in_uInputFramesAvailableToSRC - number of decoded SHAPE in XMA output buffer plus the number of encoded (128-sample) SHAPE frames available to XMA decoded.
	// in_fPitchRatio - Pitch conversion factor.

	inline AkUInt32 GetNumFlowgraphsThatCanRun( AkUInt32 in_uInputFramesAvailableToSRC, AkUInt32 in_uCurSamplingIncr, AkUInt32 in_uTgtSamplingIncr, AkUInt32 in_uSrcSamplePointer)
	{
		AkUInt32 uSubSampleFraction = in_uSrcSamplePointer & AK_SHAPE_SUB_SAMPLE_MASK;
		AkUInt32 uSamplingIncr = in_uCurSamplingIncr;

		AkInt32 iInputSamplesAvailable = (in_uInputFramesAvailableToSRC * SHAPE_FRAME_SIZE - 8);
		AkUInt32 uOutputFramesGenerated = 0;
		for (unsigned int i =0; i < AK_NUM_DMA_FRAMES_PER_BUFFER; ++i)
		{
#if AK_SHAPE_SRC_PITCH_RAMP
			AkUInt32 uTargetSamplingIncr = (AkUInt32)((AkInt32)in_uCurSamplingIncr + (((AkInt32)i+1)*((AkInt32)in_uCurSamplingIncr-(AkInt32)in_uTgtSamplingIncr))/(AkInt32)AK_NUM_DMA_FRAMES_PER_BUFFER);
#else
			AkUInt32 uTargetSamplingIncr = in_uTgtSamplingIncr;
#endif
			AkUInt32 uConsumedSamples = GetNumSamplesConsumedBySRC(uSamplingIncr,uTargetSamplingIncr,uSubSampleFraction);
			if ( iInputSamplesAvailable < ((AkInt32)uConsumedSamples ))
			{
				break;
			}
			else
			{
				iInputSamplesAvailable -= uConsumedSamples;
				uOutputFramesGenerated++;
				uSamplingIncr = uTargetSamplingIncr;
			}
		}

		return uOutputFramesGenerated;
	}

	inline AkUInt32 GetNumShapeFramesNeeded( AkUInt32 in_uToProduceThisManyShapeFrames, AkUInt32 in_uCurSamplingIncr, AkUInt32 in_uTgtSamplingIncr, AkUInt32 in_uSrcSamplePointer )
	{
		if (in_uToProduceThisManyShapeFrames > 0)
		{
			AkUInt32 uSubSampleFraction = in_uSrcSamplePointer & AK_SHAPE_SUB_SAMPLE_MASK;
			AkInt32 iInputSamplesConsumed = 8;
			AkUInt32 uSamplingIncr = in_uCurSamplingIncr;
			for (unsigned int i=0; i < in_uToProduceThisManyShapeFrames; ++i)
			{
#if AK_SHAPE_SRC_PITCH_RAMP
				AkUInt32 uTargetSamplingIncr = (AkUInt32)((AkInt32)in_uCurSamplingIncr + (((AkInt32)i+1)*((AkInt32)in_uCurSamplingIncr-(AkInt32)in_uTgtSamplingIncr))/(AkInt32)AK_NUM_DMA_FRAMES_PER_BUFFER);
#else
				AkUInt32 uTargetSamplingIncr = in_uTgtSamplingIncr;
#endif
				iInputSamplesConsumed += GetNumSamplesConsumedBySRC(uSamplingIncr,uTargetSamplingIncr,uSubSampleFraction);
				uSamplingIncr = uTargetSamplingIncr;
			}

			return (iInputSamplesConsumed + SHAPE_FRAME_SIZE-1) / SHAPE_FRAME_SIZE;
		}
		return 0;
	}

	inline AkUInt32 GetXmaOutputBufferNumDecodedSamples(SHAPE_XMA_CONTEXT * pXmaCtx, SHAPE_SRC_CONTEXT* pSrcCtx)
	{
		// GetShapeXmaOutputBufferNumFramesAvailable() returns the number of 256 byte blocks. Assuming 16-bits per channel, this is 128 mono samples, or 64 stereo samples
		AkUInt32 uXmaOutputBufferNumSamplesAvailable = (GetShapeXmaOutputBufferNumFramesAvailable(pXmaCtx) * SHAPE_FRAME_SIZE) / (pXmaCtx->numChannels+1); // 

		if (uXmaOutputBufferNumSamplesAvailable > 0)
		{
			//Subtract off the samples that have already been consumed by the SRC.
			AkUInt32 uOffset = ((pSrcCtx->samplePointer & ~AK_SHAPE_SUB_SAMPLE_MASK) >> 18);
			AKASSERT(uOffset <= uXmaOutputBufferNumSamplesAvailable);
			AKASSERT(uOffset <= SHAPE_FRAME_SIZE / (pXmaCtx->numChannels+1));
			uXmaOutputBufferNumSamplesAvailable -= uOffset;
		}
		
		return uXmaOutputBufferNumSamplesAvailable;
	}
}
}