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
#include "AkACPMgr.h"
#include "AkMonitor.h"
#include "PlatformAudiolibDefs.h"
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/Tools/Common/AkObject.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include "AkCommon.h"
#include "AkLEngine.h"

extern AkPlatformInitSettings g_PDSettings;

#define AK_NUM_COMMANDS_PER_VOICE	(5)

#define AK_COOKIE_COMMANDID_DMA_INCREMENT	(33)
#define AK_COOKIE_COMMANDID_CTX_STOP	(50)
#define AK_COOKIE_COMMANDID_CTX_START	(51)
#define AK_COOKIE_COMMANDID_LOAD_FLOWGRAPH	(52)
#define AK_COOKIE_COMMANDID_UPDATE_SRC_CONTEXTS (53)
#define AK_COOKIE_COMMANDID_UPDATE_XMA_CONTEXTS (54)
#define AK_COOKIE_COMMANDID_SEND_STOP_COMMAND (55)

AkACPMgr AkACPMgr::s_AcpMgr;

AKRESULT AkACPVoice::Reset( IAkSHAPEAware * in_pOwner, AkUInt32 in_uNumChannels, AkUInt8 in_uStreamIndex, AkReal32 in_fPitch )
{
    AKASSERT( !IsXmaCtxEnable() );

	::ZeroMemory( pSrcContext, sizeof(SHAPE_SRC_CONTEXT) );
    ::ZeroMemory( pDmaContext[0], sizeof(SHAPE_DMA_CONTEXT) );
    ::ZeroMemory( pDmaContext[1], sizeof(SHAPE_DMA_CONTEXT) );
	::ZeroMemory( pXmaContext, sizeof(SHAPE_XMA_CONTEXT) );
	
	pOwner = in_pOwner;
	uStreamIndex = in_uStreamIndex;

	// Clear output buffer for XMA decoder
	ZeroMemory( m_pOutputBuffer, SHAPE_XMA_MAX_OUTPUT_BUFFER_SIZE );

	// Clear overlap buffer for XMA decoder
	ZeroMemory( m_pOverlapBuffer, SHAPE_XMA_OVERLAP_ADD_BUFFER_SIZE_MONO * in_uNumChannels );
	
	uMaxFlowgraphsToRun = 0;
	uDmaFramesWritten = 0;
	uSrcSubSamplesConsumed = 0;
	uDmaReadPointer = 0;
	uTotalDecodedSamples = 0;
	uNumChannels = in_uNumChannels;

	HRESULT hr = SetShapeXmaOutputBuffer(pXmaContext, m_pOutputBuffer);
	AKVERIFY( SUCCEEDED( hr ) );

	hr = SetShapeXmaOutputBufferSize(pXmaContext, SHAPE_XMA_MAX_OUTPUT_BUFFER_SIZE);
	AKVERIFY( SUCCEEDED( hr ) );

	hr = SetShapeXmaOverlapAddBuffer(pXmaContext, m_pOverlapBuffer);
	AKVERIFY( SUCCEEDED( hr ) );

	hr = SetShapeSrcPitch( pSrcContext, in_fPitch );
	AKVERIFY( SUCCEEDED( hr ) );

	hr = SetShapeSrcPitchTarget( pSrcContext, in_fPitch );
	AKVERIFY( SUCCEEDED( hr ) );

	return AK_Success;
}

static void DebugPrintAcpMessage( const ACP_MESSAGE& message )
{
    char string[1024];

    switch( message.type)
    {
    case ACP_MESSAGE_TYPE_AUDIO_FRAME_START:
        sprintf_s(string,"ACP_MESSAGE: AUDIO_FRAME_START: audio frame = %u\n", message.audioFrameStart.audioFrame);
        break;
    case ACP_MESSAGE_TYPE_FLOWGRAPH_COMPLETED:
        sprintf_s(string,"ACP_MESSAGE: FLOWGRAPH_COMPLETED: flowgraph = 0x%x\n", message.flowgraphCompleted.flowgraph);
        break;
    case ACP_MESSAGE_TYPE_SRC_BLOCKED:
        sprintf_s(string,"ACP_MESSAGE: SRC_BLOCKED: context = %u - \n", message.shapeCommandBlocked.contextIndex);
        break;
    case ACP_MESSAGE_TYPE_DMA_BLOCKED:
        sprintf_s(string,"ACP_MESSAGE: DMA_BLOCKED: context = %u - \n", message.shapeCommandBlocked.contextIndex);
        break;
    case ACP_MESSAGE_TYPE_COMMAND_COMPLETED:
        sprintf_s(string,"ACP_MESSAGE: COMMAND_COMPLETED: audioframe = %u, command = %llu\n", message.commandCompleted.audioFrame, message.commandCompleted.commandId);
        break;
    case ACP_MESSAGE_TYPE_FLOWGRAPH_TERMINATED:
        sprintf_s(string,"ACP_MESSAGE: FLOWGRAPH_TERMINATED: Completed = %u, reason = %s\n", message.flowgraphTerminated.numCommandsCompleted, 
                        ( message.flowgraphTerminated.reason == ACP_FLOWGRAPH_TERMINATED_REASON_INVALID_GRAPH) ? "ACP_FLOWGRAPH_TERMINATED_REASON_INVALID_GRAPH" : 
                        ( message.flowgraphTerminated.reason == ACP_FLOWGRAPH_TERMINATED_TIME_EXCEEDED) ? "ACP_FLOWGRAPH_TERMINATED_TIME_EXCEEDED" :
                        ( message.flowgraphTerminated.reason == ACP_FLOWGRAPH_TERMINATED_DISCONNECT) ? "ACP_FLOWGRAPH_TERMINATED_DISCONNECT" : "UNKNOWN");
/*        if( message.flowgraphTerminated.reason == ACP_FLOWGRAPH_TERMINATED_REASON_INVALID_GRAPH)
        {
            for(UINT32 c = 0; c < m_NumFlowgraphCommands; ++c)
            {
                if( m_FlowgraphCmds[c].header.error != 0)
                {
                    sprintf_s(string,"         %lu ", c);
                    DebugPrintShapeCommand( m_FlowgraphCmds[c]);
                }
            }
        }
        else if( message.flowgraphTerminated.reason == ACP_FLOWGRAPH_TERMINATED_TIME_EXCEEDED)
        {
            for(UINT32 c = 0; c < m_NumFlowgraphCommands; ++c)
            {
                if( m_FlowgraphCmds[c].header.queued == 0)
                {
                    sprintf_s(string,"         %lu ", c);
                    DebugPrintShapeCommand( m_FlowgraphCmds[c]);
                }
            }
        }*/
        break;
    case ACP_MESSAGE_TYPE_ERROR:
        sprintf_s(string,"ACP_MESSAGE: ERROR: errorCode = 0x%08X, additionalData = 0x%08X\n", message.error.errorCode, message.error.additionalData);
        break;
    case ACP_MESSAGE_TYPE_DISCONNECTED:
        sprintf_s(string,"ACP_MESSAGE: DISCONNECTED\n");
        break;
    default:
        sprintf_s(string,"ACP_MESSAGE: *** ERROR: UNKNWON MESSAGE ***\n");
        break;
    }

	MONITOR_ERRORMSG(string);
}

AKRESULT AkShapeFrame::Alloc( AkMemPoolId in_poolIdShape )
{
	AkUInt32 uFgCmdSz = ( sizeof(SHAPE_FLOWGRAPH_COMMAND) * g_PDSettings.uMaxXMAVoices * AK_NUM_COMMANDS_PER_VOICE );
	m_pFlowgraphCommands = (SHAPE_FLOWGRAPH_COMMAND*)AkMalign( in_poolIdShape | AkMemType_Device, (uFgCmdSz + 0xff) & ~0xff, 256 );//round size up to multiple of 256, align to 256
	if (m_pFlowgraphCommands == NULL)
	{
		Free(in_poolIdShape);
		return AK_InsufficientMemory;
	}
#if AK_SHAPE_SRC_PITCH_RAMP
	AkUInt32 uSrcCmdSz = sizeof(ACP_COMMAND_UPDATE_SRC_CONTEXT_ENTRY) *  g_PDSettings.uMaxXMAVoices;
	m_pSrcUpdateCommands = (ACP_COMMAND_UPDATE_SRC_CONTEXT_ENTRY*)AkMalign( in_poolIdShape | AkMemType_Device, (uSrcCmdSz + 0xff) & ~0xff, 256);
	if (m_pSrcUpdateCommands == NULL)
	{
		Free(in_poolIdShape);
		return AK_InsufficientMemory;
	}
#endif

	return AK_Success;
}

void AkShapeFrame::Free( AkMemPoolId in_poolIdShape )
{
	if ( m_pFlowgraphCommands )
		AkFalign( in_poolIdShape | AkMemType_Device, m_pFlowgraphCommands );

	if(m_pSrcUpdateCommands)
		AkFalign(in_poolIdShape | AkMemType_Device, m_pSrcUpdateCommands);
}

void AkShapeFrame::Reset()
{
	m_uFlowgraphCommandsCount = 0;
	m_uMixBufferCount = 0;
	m_uSubmittedVoiceCount = 0;
	m_uSrcUpdateCommandsCount = 0;
	m_bLaunched = false;

#ifdef _DEBUG
	if (m_pFlowgraphCommands)
		AkMemSet( m_pFlowgraphCommands, 0xAA, sizeof(SHAPE_FLOWGRAPH_COMMAND) * g_PDSettings.uMaxXMAVoices * AK_NUM_COMMANDS_PER_VOICE );
	if (m_pSrcUpdateCommands)
		AkMemSet( m_pSrcUpdateCommands, 0xBB, sizeof(ACP_COMMAND_UPDATE_SRC_CONTEXT_ENTRY) *  g_PDSettings.uMaxXMAVoices );
#endif
#ifdef XMA_CHECK_VOICE_SYNC
	::ZeroMemory(m_bfVoicesInFlowgraph, sizeof(m_bfVoicesInFlowgraph));
#endif
}

void AkShapeFrame::SubmitVoiceToProcess( UINT32 uNumChannels, UINT32 uXmaContextId, UINT32 uSrcContextId, UINT32 uDmaContextId0, UINT32 uDmaContextId1 )
{
	AKASSERT(!m_bLaunched);

	HRESULT hr;
	if ( uNumChannels == 1 )
	{
		hr = SetShapeAllocMixBufferCommand( m_pFlowgraphCommands + m_uFlowgraphCommandsCount++, m_uMixBufferCount, 1, 1, SHAPE_MIXBUFFER_ATTENUATION_0_DB );
		AKASSERT( SUCCEEDED( hr ) );

		hr = SetShapeSrcCommand( m_pFlowgraphCommands + m_uFlowgraphCommandsCount++, uSrcContextId, m_uMixBufferCount, m_uMixBufferCount );
		AKASSERT( SUCCEEDED( hr ) );

		hr = SetShapeDmaCommand( m_pFlowgraphCommands + m_uFlowgraphCommandsCount++, uDmaContextId0, m_uMixBufferCount, false );
		AKASSERT( SUCCEEDED( hr ) );

		m_uMixBufferCount += 1;
	}
	else
	{
		hr = SetShapeAllocMixBufferCommand( m_pFlowgraphCommands + m_uFlowgraphCommandsCount++, m_uMixBufferCount + 0, 1, 1, SHAPE_MIXBUFFER_ATTENUATION_0_DB );
		AKASSERT( SUCCEEDED( hr ) );

		hr = SetShapeAllocMixBufferCommand( m_pFlowgraphCommands + m_uFlowgraphCommandsCount++, m_uMixBufferCount + 1, 1, 1, SHAPE_MIXBUFFER_ATTENUATION_0_DB );
		AKASSERT( SUCCEEDED( hr ) );

		hr = SetShapeSrcCommand( m_pFlowgraphCommands + m_uFlowgraphCommandsCount++, uSrcContextId, m_uMixBufferCount + 0, m_uMixBufferCount + 1 );
		AKASSERT( SUCCEEDED( hr ) );

		hr = SetShapeDmaCommand( m_pFlowgraphCommands + m_uFlowgraphCommandsCount++, uDmaContextId0, m_uMixBufferCount + 0, false );
		AKASSERT( SUCCEEDED( hr ) );

		hr = SetShapeDmaCommand( m_pFlowgraphCommands + m_uFlowgraphCommandsCount++, uDmaContextId1, m_uMixBufferCount + 1, false );
		AKASSERT( SUCCEEDED( hr ) );

		m_uMixBufferCount += 2;
	}

	++m_uSubmittedVoiceCount;

#ifdef AK_ENABLE_INSTRUMENT
	AK_PIX_SET_MARKER_HOOK(AK_PIX_COLOR(242,24,113), "Submit Voice Idx %i - %i submitted.", uXmaContextId, m_uSubmittedVoiceCount);
#endif

#ifdef XMA_CHECK_VOICE_SYNC
	// This voice must not have already been submitted for processing in this flowgraph!
	AKASSERT((m_bfVoicesInFlowgraph[uXmaContextId >> SHAPE_XMA_CONTEXT_INDEX_SHIFT] & (1 << (uXmaContextId & SHAPE_XMA_CONTEXT_BIT_MASK))) == 0);
	m_bfVoicesInFlowgraph[uXmaContextId >> SHAPE_XMA_CONTEXT_INDEX_SHIFT] |= 1 << (uXmaContextId & SHAPE_XMA_CONTEXT_BIT_MASK);
#endif
}

bool AkShapeFrame::LaunchFlowgraph(IAcpHal* in_pAcpHal)
{
	AKASSERT(!m_bLaunched);

	if (m_uFlowgraphCommandsCount > 0)
	{
		HRESULT hr;

#if AK_SHAPE_SRC_PITCH_RAMP
		if (m_uSrcUpdateCommandsCount > 0)
		{
			ACP_COMMAND_UPDATE_CONTEXTS cmdUpdateCtxs = { m_uSrcUpdateCommandsCount, ApuMapVirtualAddress(m_pSrcUpdateCommands), 0, 0};
			HRESULT hr = in_pAcpHal->SubmitCommand( ACP_COMMAND_TYPE_UPDATE_SRC_CONTEXTS, AK_COOKIE_COMMANDID_UPDATE_SRC_CONTEXTS, ACP_SUBMIT_PROCESS_COMMAND_ASAP, &cmdUpdateCtxs );
			AKASSERT( SUCCEEDED( hr ) );
		}
#endif
		ACP_COMMAND_LOAD_SHAPE_FLOWGRAPH cmdLoadFlowgraph = { m_uFlowgraphCommandsCount, ApuMapVirtualAddress(m_pFlowgraphCommands), 0, 0 };
		hr = in_pAcpHal->SubmitCommand( ACP_COMMAND_TYPE_LOAD_SHAPE_FLOWGRAPH, AK_COOKIE_COMMANDID_LOAD_FLOWGRAPH, ACP_SUBMIT_PROCESS_COMMAND_ASAP, &cmdLoadFlowgraph );
		AKASSERT( SUCCEEDED( hr ) );

		m_bLaunched = true;

		return true;
	}
	else
	{
#ifdef AK_ENABLE_ASSERTS
		AKASSERT(m_uSrcUpdateCommandsCount == 0);
		AKASSERT(m_uMixBufferCount == 0);
#ifdef XMA_CHECK_VOICE_SYNC
		for (UINT32 i = 0; i < SHAPE_NUM_XMA_CONTEXT_BITFIELDS; ++i)
			AKASSERT(m_bfVoicesInFlowgraph[i] == 0);
#endif
#endif
		//Every other field should be 0, but m_uSubmittedVoiceCount includes skipped voices.  Reset it now.
		m_uSubmittedVoiceCount = 0;
	}

	return false;
}

AkACPMgr::AkACPMgr()
	: m_pAcpHal( nullptr )
	, m_pXmaMem( nullptr )
	, m_uNumPendingDMAReadIncrement( 0 )
	, m_uNumCtxEnableDisable( 0 )
	, m_bIsInitialized( false )
	, m_uNumFlowgraphsRunning( 0 )
	, m_uFgIdxNextFrame( 0 )
	, m_uFgIdxRunning(0)
	, m_frames(nullptr)
{
	Clear();
}

#ifdef XMA_CHECK_VOICE_SYNC
bool AkACPMgr::VoiceIsInFlowgraph(AkACPVoice* pThisVoice)
{
	AkAutoLock<CAkLock> Lock(m_lockWait);

	if (m_uNumFlowgraphsRunning > 0 && pThisVoice != nullptr)
	{
		AkUInt32 uNumStreams = pThisVoice->pOwner->GetNumStreams();
		for (AkUInt32 i = 0; i < uNumStreams; ++i)
		{
			AkACPVoice* pVoice = pThisVoice->pOwner->GetAcpVoice(i);
			if (pVoice != nullptr)
			{
				for (AkUInt32 f = 0; f < AK_NUM_DMA_FRAMES_PER_BUFFER; ++f)
				{
					AkShapeFrame& frame = m_frames[f];
					if (frame.VoiceIsInFlowgraph((AkUInt32)(pVoice - m_pVoices)))
						return true;
				}
			}
		}
	}
	return false;
}

void AkACPMgr::CheckAcpVoiceSync(AkACPVoice* pThisVoice)
{
	AKASSERT(m_frames != NULL);
	AkUInt32 uNumStreams = pThisVoice->pOwner->GetNumStreams();
	if (uNumStreams > 1)
	{
		for (AkUInt32 i = 0; i < uNumStreams; ++i)
		{
			bool bInAnyFlowgraph = false;
			AkACPVoice* pVoice = pThisVoice->pOwner->GetAcpVoice(i);
			for (AkUInt32 f = 0; f < AK_NUM_DMA_FRAMES_PER_BUFFER; ++f)
			{
				AkShapeFrame& frame = m_frames[f];
				bool bInFlowgraph = frame.VoiceIsInFlowgraph((AkUInt32)(pVoice - m_pVoices));
				AKASSERT(bInFlowgraph == frame.VoiceIsInFlowgraph((AkUInt32)(pThisVoice - m_pVoices)));
				bInAnyFlowgraph = bInAnyFlowgraph || bInFlowgraph;
			}
			if (bInAnyFlowgraph)  
			{
				//Ok, so its not good if sampleCount/samplePointer is not in sync, but as long as it is not going to be submitted to a flowgraph we can handle it gracefully.
				AKASSERT(pVoice->pSrcContext->sampleCount == pThisVoice->pSrcContext->sampleCount);
				AKASSERT((pVoice->pSrcContext->samplePointer & AK_SHAPE_SUB_SAMPLE_MASK) == (pThisVoice->pSrcContext->samplePointer & AK_SHAPE_SUB_SAMPLE_MASK));
			}
			AKASSERT(pVoice->uSrcSubSamplesConsumed == pThisVoice->uSrcSubSamplesConsumed);
			AKASSERT(pVoice->uDmaFramesWritten == pThisVoice->uDmaFramesWritten);
			AKASSERT(pVoice->uTotalDecodedSamples == pThisVoice->uTotalDecodedSamples);
		}
	}

	
}
#endif

void AkACPMgr::Clear()
{
	if (m_frames)
	{
		for(AkUInt32 i=0; i<AK_NUM_DMA_FRAMES_PER_BUFFER; ++i)
		{
			m_frames[i].Reset();
		}
	}

	if (m_pAcpHal)
	{
		m_pAcpHal->Release();
		m_pAcpHal = nullptr;
	}
	
	m_pXmaMem = nullptr;
	m_uNumPendingDMAReadIncrement = 0;
	m_uNumFlowgraphsRunning = 0;
	m_uFgIdxNextFrame = 0;
	m_uFgIdxRunning = 0;

	AKASSERT( ( AK_NUM_VOICE_REFILL_FRAMES % SHAPE_FRAME_SIZE ) == 0 );
	::ZeroMemory( &m_contexts, sizeof( m_contexts ) );

#ifdef AK_ENABLE_ASSERTS
	m_bSpinCount = 0;
#endif
}

AKRESULT AkACPMgr::Init()
{
	// Important: ACP manager is static so we need to clear its members here also.
	
	Clear();
	m_bIsInitialized = true;

	m_contexts.numSrcContexts = g_PDSettings.uMaxXMAVoices; // Last context is used for sending updates updating.
    m_contexts.numDmaContexts = 2 * g_PDSettings.uMaxXMAVoices;
	m_contexts.numXmaContexts = g_PDSettings.uMaxXMAVoices;
    
	HRESULT hr = AcpHalAllocateShapeContexts(&m_contexts);
	if ( FAILED( hr ) )
	{
		AKPLATFORM::OutputDebugMsg( "Wwise: AcpHalAllocateShapeContexts failed" );
		Term();
		return AK_Fail;
	}

    ::ZeroMemory(m_contexts.srcContextArray, m_contexts.numSrcContexts * sizeof(SHAPE_SRC_CONTEXT));
    ::ZeroMemory(m_contexts.dmaContextArray, m_contexts.numDmaContexts * sizeof(SHAPE_DMA_CONTEXT));
    ::ZeroMemory(m_contexts.xmaContextArray, m_contexts.numXmaContexts * sizeof(SHAPE_XMA_CONTEXT));
    ::ZeroMemory(m_contexts.pcmContextArray, m_contexts.numPcmContexts * sizeof(SHAPE_PCM_CONTEXT));

    // Create the interface to the HAL
    hr = AcpHalCreate(&m_pAcpHal);
	if ( FAILED( hr ) )
	{
		AKPLATFORM::OutputDebugMsg( "Wwise: AcpHalCreate failed" );
		Term();
		return AK_Fail;
	}

    static const UINT32 COMMAND_QUEUE_LENGTH = 16384;
    static const UINT32 MESSAGE_QUEUE_LENGTH = 16384;
	hr = m_pAcpHal->Connect( COMMAND_QUEUE_LENGTH, MESSAGE_QUEUE_LENGTH );
	if ( FAILED( hr ) )
	{
		AKPLATFORM::OutputDebugMsg( "Wwise: IAcpHal::Connect failed" );
		Term();
		return AK_Fail;
	}

	m_pVoices = (AkACPVoice *) AkAlloc( AkMemID_Processing, sizeof( AkACPVoice ) * g_PDSettings.uMaxXMAVoices );
	if ( !m_pVoices )
	{	
		Term();
		return AK_InsufficientMemory;
	}

	m_frames = (AkShapeFrame*)AkAlloc(AkMemID_Processing, sizeof(AkShapeFrame)*AK_NUM_DMA_FRAMES_PER_BUFFER );
	if ( !m_frames )
	{	
		Term();
		return AK_InsufficientMemory;
	}

	for(AkUInt32 i=0; i<AK_NUM_DMA_FRAMES_PER_BUFFER; ++i)
	{
		AkPlacementNew( &m_frames[i] ) AkShapeFrame();
		if ( m_frames[i].Alloc(AkMemID_Processing) != AK_Success)
		{
			Term();
			return AK_InsufficientMemory;
		}
	}

	// XMA Output pool size = g_PDSettings.uMaxXMAVoices * (output buffer + stereo overlap)
	AkUInt32 uXMAOutPoolSize = g_PDSettings.uMaxXMAVoices * ( SHAPE_XMA_MAX_OUTPUT_BUFFER_SIZE + SHAPE_XMA_OVERLAP_ADD_BUFFER_SIZE_STEREO );
	
	// Get the one and only block.
	m_pXmaMem = AkMalign(AkMemID_Processing | AkMemType_Device, uXMAOutPoolSize, SHAPE_XMA_OUTPUT_BUFFER_SIZE_ALIGNMENT);
	if (m_pXmaMem == NULL)
		return AK_InsufficientMemory;

	AkUInt8 * pXMAOutputStart = (AkUInt8*)m_pXmaMem;
	AKASSERT( pXMAOutputStart );
	AkUInt8 * pXMAOutput = pXMAOutputStart;
	AkUInt8 * pXMAOverlapStart = pXMAOutputStart + g_PDSettings.uMaxXMAVoices * SHAPE_XMA_MAX_OUTPUT_BUFFER_SIZE;
	AkUInt8 * pXMAOverlap = pXMAOverlapStart;
	AkUInt8 * pXMAOverlapEnd = pXMAOverlapStart + g_PDSettings.uMaxXMAVoices * SHAPE_XMA_OVERLAP_ADD_BUFFER_SIZE_STEREO;

	for ( int i = g_PDSettings.uMaxXMAVoices - 1; i >= 0; --i ) // iterate backwards so that first free voice is idx 0
	{
		AkPlacementNew( m_pVoices + i ) AkACPVoice( 
			&m_contexts.xmaContextArray[ i ], 
			&m_contexts.srcContextArray[ i ], 
			&m_contexts.dmaContextArray[ i*2 ], 
			&m_contexts.dmaContextArray[ i*2 + 1 ],
			pXMAOutput,
			pXMAOverlap );
		
		m_freeVoices.AddFirst( m_pVoices + i );

		pXMAOutput += SHAPE_XMA_MAX_OUTPUT_BUFFER_SIZE;
		AKASSERT( pXMAOutput <= pXMAOverlapStart );
		AKASSERT( ( (AkUIntPtr)pXMAOutput % SHAPE_XMA_OUTPUT_BUFFER_SIZE_ALIGNMENT ) == 0 );
		pXMAOverlap += SHAPE_XMA_OVERLAP_ADD_BUFFER_SIZE_STEREO;
		AKASSERT( pXMAOverlap <= pXMAOverlapEnd );
		AKASSERT( ( (AkUIntPtr)pXMAOverlap % SHAPE_XMA_OVERLAP_ADD_BUFFER_ALIGNMENT ) == 0 );
	}

	RegisterForFlowgraphMessages();

	{
		ACP_COMMAND_ENABLE_OR_DISABLE_XMA_CONTEXTS  commandXma;
		::ZeroMemory( &commandXma, sizeof( commandXma ) );

		for ( auto it = m_freeVoices.Begin(); it != m_freeVoices.End(); ++it )
		{
			AkACPVoice * pVoice = (*it);

			UINT32 uXmaContextId = (UINT32) ( pVoice->pXmaContext - m_contexts.xmaContextArray );
			commandXma.context[uXmaContextId >> SHAPE_XMA_CONTEXT_INDEX_SHIFT] |= ( 1 << (uXmaContextId & SHAPE_XMA_CONTEXT_BIT_MASK) );
			AKASSERT( pVoice->IsXmaCtxEnable() );
			pVoice->m_bXmaCtxEnable = false;
		}

		HRESULT hr;
		hr = m_pAcpHal->SubmitCommand(ACP_COMMAND_TYPE_DISABLE_XMA_CONTEXTS, AK_COOKIE_COMMANDID_CTX_STOP, ACP_SUBMIT_PROCESS_COMMAND_ASAP, &commandXma);
		AKASSERT( SUCCEEDED( hr ) );
     
		m_uNumCtxEnableDisable++;

		do 
			ProcessMessageQueue();
		while (m_uNumCtxEnableDisable > 0);
		
	}

	return AK_Success;
}

void AkACPMgr::Term()
{
	if ( !m_bIsInitialized || m_pAcpHal == NULL )
		return;
	 
	ProcessMessageQueue();
	
	if (!m_recycledVoices.IsEmpty())
		RecycleVoices();

	HRESULT hr;

	if ( m_pAcpHal )
	{
		hr = m_pAcpHal->Disconnect();
		// Can fail if Connect() failed. AKASSERT( SUCCEEDED( hr ) );
	}

	hr = AcpHalReleaseShapeContexts();
	// Can fail if Connect() failed. AKASSERT( SUCCEEDED( hr ) );

	m_freeVoices.Term();
	AKASSERT(m_usedVoices.First() == NULL);
	AKASSERT(m_recycledVoices.First() == NULL);

	if (m_pVoices)
	{
		for (int i = 0; i < g_PDSettings.uMaxXMAVoices; i++)
		{
			m_pVoices[i].~AkACPVoice();
		}

		AkFree(AkMemID_Processing, m_pVoices);
		m_pVoices = NULL;
	}

	if (m_frames != NULL)
	{
		for(AkUInt32 i=0; i<AK_NUM_DMA_FRAMES_PER_BUFFER; ++i)
			m_frames[i].Free(AkMemID_Processing | AkMemType_Device);

		AkFree(AkMemID_Processing, m_frames);
		m_frames = NULL;
	}

	AkFalign(AkMemID_Processing | AkMemType_Device, m_pXmaMem);

	// Important: ACP manager is static so we need to clear its members here also, because we can double-term.
	/// TODO Make as singleton.
	Clear();
}


void AkACPMgr::Update()
{
	if (m_bIsInitialized)
	{
		AkAutoLock<CAkLock> Lock(m_lockWait);

		AK_INSTRUMENT_BEGIN_C( AK_PIX_COLOR(242,220,229), "AkACPMgr::Update()" );

		WaitForFlowgraphsNoLock();

		//Disable XMA contexts that need input update, and XMA contexts that have been recycled.
		bool bWait = EnableDisableXmaContextsForInputUpdate(false);
		bWait = StopRecycledXmaContexts() || bWait;

		//Wait for commands submitted above.
		if (bWait)
			WaitForCommandCompletion();

		FinishRecycleAndNotify();

		for ( auto it = m_usedVoices.Begin(); it != m_usedVoices.End(); ++it )
		{
			AkACPVoice * pVoice = (*it);
			pVoice->UpdateInput();
		}

		EnableDisableXmaContextsForInputUpdate(true);
		//No need to wait for completion.

		for ( auto it = m_usedVoices.Begin(); it != m_usedVoices.End(); ++it )
		{
			AkACPVoice * pVoice = (*it);
			for (AkUInt32 uFgIdx = 0; uFgIdx < AK_NUM_DMA_FRAMES_PER_BUFFER; ++uFgIdx )
			{
				if ( pVoice->OnFlowgraphPrepare(uFgIdx) )
					SubmitVoiceToProcess(pVoice, uFgIdx);
				else
					break;
			}
		}

		if (m_uNumFlowgraphsRunning < AK_NUM_DMA_FRAMES_PER_BUFFER)
		{
#ifdef XMA_CHECK_VOICE_SYNC
			for (auto it = m_usedVoices.Begin(); it != m_usedVoices.End(); ++it)
				CheckAcpVoiceSync(*it);
#endif // AK_ENABLE_ASSERTS

			// Launch the remaining flowgraphs that have been queued up.
			if (m_frames[m_uFgIdxNextFrame].m_uSubmittedVoiceCount > 0)
			{
				for(AkUInt32 i=0; i < AK_NUM_DMA_FRAMES_PER_BUFFER; ++i )
				{
					AkUInt32 uIdxFg = m_uFgIdxNextFrame + i;
					AKASSERT(uIdxFg < AK_NUM_DMA_FRAMES_PER_BUFFER);
					LaunchFlowgraph(uIdxFg);
				}
			}
		}

		AK_INSTRUMENT_END("");
	}
}

void AkACPMgr::SubmitVoiceToProcess( AkACPVoice * in_pVoice, AkUInt32 in_uShapeFrameIdx )
{
	AKASSERT(m_bIsInitialized);

	AKASSERT( in_pVoice->uDmaFramesWritten < AK_NUM_DMA_FRAMES_PER_BUFFER );
	AkUInt32 uFgIndexForVoice = m_uFgIdxNextFrame + in_uShapeFrameIdx;
	AKASSERT(uFgIndexForVoice < AK_NUM_DMA_FRAMES_PER_BUFFER);

	UINT32 uXmaContextId = (UINT32) ( in_pVoice->pXmaContext - m_contexts.xmaContextArray );
	UINT32 uSrcContextId = (UINT32) ( in_pVoice->pSrcContext - m_contexts.srcContextArray );
	UINT32 uDmaContextId0 = (UINT32) ( in_pVoice->pDmaContext[0] - m_contexts.dmaContextArray );
	UINT32 uDmaContextId1 = (UINT32) ( in_pVoice->pDmaContext[1] - m_contexts.dmaContextArray );
	
	m_frames[uFgIndexForVoice].SubmitVoiceToProcess(in_pVoice->uNumChannels, uXmaContextId, uSrcContextId, uDmaContextId0, uDmaContextId1 );

	AKASSERT( m_uNumFlowgraphsRunning < AK_NUM_DMA_FRAMES_PER_BUFFER );
	AKASSERT( in_pVoice->uDmaFramesWritten < AK_NUM_DMA_FRAMES_PER_BUFFER );

	in_pVoice->uDmaFramesWritten++;
}

void AkACPMgr::LaunchFlowgraph(AkUInt32 in_uFgIdx)
{
	AkShapeFrame& frame = m_frames[in_uFgIdx];

	if (frame.LaunchFlowgraph(m_pAcpHal) )
	{
		AKASSERT(m_uNumFlowgraphsRunning < AK_NUM_DMA_FRAMES_PER_BUFFER);
		
		if (m_uNumFlowgraphsRunning == 0)
			m_uFgIdxRunning = in_uFgIdx;
		
		m_uNumFlowgraphsRunning++;

#ifdef AK_ENABLE_INSTRUMENT
		AK_PIX_SET_MARKER_HOOK( AK_PIX_COLOR(44,24,242), "LaunchFlowgraph %i-voices, %i-running", frame.m_uSubmittedVoiceCount, m_uNumFlowgraphsRunning );
#endif
	}

	if (((in_uFgIdx+1) % AK_NUM_DMA_FRAMES_PER_BUFFER) == 0) //last fg for this buffer
		m_uFgIdxNextFrame = (m_uFgIdxNextFrame+AK_NUM_DMA_FRAMES_PER_BUFFER) % AK_NUM_DMA_FRAMES_PER_BUFFER;
}

void AkACPMgr::StopXmaContext_Sync(AkACPVoice* in_pVoice)
{
	if (in_pVoice->IsXmaCtxEnable())
	{
		UINT32 uXmaContextId = (UINT32) ( in_pVoice->pXmaContext - m_contexts.xmaContextArray );
		ACP_COMMAND_ENABLE_OR_DISABLE_XMA_CONTEXT commandXma = { uXmaContextId };
		HRESULT hr = m_pAcpHal->SubmitCommand(ACP_COMMAND_TYPE_DISABLE_XMA_CONTEXT, AK_COOKIE_COMMANDID_CTX_STOP, ACP_SUBMIT_PROCESS_COMMAND_ASAP, &commandXma);
		AKASSERT( SUCCEEDED( hr ) );

		m_uNumCtxEnableDisable++;
    	in_pVoice->m_bXmaCtxEnable = false;
		
		WaitForCommandCompletion();
	}
}

void AkACPMgr::StartXmaContext_Async(AkACPVoice* in_pVoice)
{
	AKASSERT(!in_pVoice->IsXmaCtxEnable());

	// No need to start voice if there is no input.
	if (in_pVoice->pXmaContext->validBuffer)
	{
		UINT32 uXmaContextId = (UINT32) ( in_pVoice->pXmaContext - m_contexts.xmaContextArray );
		ACP_COMMAND_ENABLE_OR_DISABLE_XMA_CONTEXT commandXma = { uXmaContextId };
		HRESULT hr = m_pAcpHal->SubmitCommand(ACP_COMMAND_TYPE_ENABLE_XMA_CONTEXT, AK_COOKIE_COMMANDID_CTX_START, ACP_SUBMIT_PROCESS_COMMAND_ASAP, &commandXma);
		AKASSERT( SUCCEEDED( hr ) );

		m_uNumCtxEnableDisable++;
   		in_pVoice->m_bXmaCtxEnable = true;
	}
}

bool AkACPMgr::EnableDisableXmaContextsForInputUpdate(bool in_bEnable)
{
	AK_INSTRUMENT_BEGIN_C(AK_PIX_COLOR(222,220,242), "AkACPMgr::EnableDisableXmaContextsForInputUpdate()");

	bool bCommandsSubmitted = false;

	ACP_COMMAND_ENABLE_OR_DISABLE_XMA_CONTEXTS  commandXma;
	::ZeroMemory( &commandXma, sizeof( commandXma ) );

	AkUInt32 uNumCmdSubmitted = 0;

	for ( auto it = m_usedVoices.Begin(); it != m_usedVoices.End(); ++it )
	{
		AkACPVoice * pVoice = (*it);

		if (	( !in_bEnable && pVoice->NeedsUpdate() && pVoice->IsXmaCtxEnable() ) || // Disable voices only if NeedsUpdate() is true, or
				( in_bEnable && !pVoice->IsXmaCtxEnable() ) // Enable all disabled voices 
			)
		{
			UINT32 uXmaContextId = (UINT32) ( pVoice->pXmaContext - m_contexts.xmaContextArray );
			commandXma.context[uXmaContextId >> SHAPE_XMA_CONTEXT_INDEX_SHIFT] |= ( 1 << (uXmaContextId & SHAPE_XMA_CONTEXT_BIT_MASK) );
			pVoice->m_bXmaCtxEnable = in_bEnable;
			uNumCmdSubmitted++;
		}
	}

	if (uNumCmdSubmitted > 0)
	{
		HRESULT hr = m_pAcpHal->SubmitCommand(	in_bEnable ? ACP_COMMAND_TYPE_ENABLE_XMA_CONTEXTS : ACP_COMMAND_TYPE_DISABLE_XMA_CONTEXTS, 
												in_bEnable ? AK_COOKIE_COMMANDID_CTX_START : AK_COOKIE_COMMANDID_CTX_STOP, 
												ACP_SUBMIT_PROCESS_COMMAND_ASAP, 
												&commandXma );

		AKASSERT( SUCCEEDED( hr ) );

		m_uNumCtxEnableDisable++;

		bCommandsSubmitted = true;
	}

	AK_INSTRUMENT_END("");

	return bCommandsSubmitted;
}


AKRESULT AkACPMgr::RegisterVoice( IAkSHAPEAware * in_pOwner, AkACPVoice ** out_ppVoice, AkUInt32 in_uNumChannels, AkUInt8 in_uStreamIndex, AkReal32 in_fPitch )
{
	AkAutoLock<CAkLock> Lock(m_lockWait);

	AkACPVoice * pVoice = m_freeVoices.First();
	*out_ppVoice = pVoice;

	if ( !pVoice )
		return AK_Fail;

	m_freeVoices.RemoveFirst();

	AKASSERT(!pVoice->IsXmaCtxEnable());

	AKRESULT eResult = pVoice->Reset( in_pOwner, in_uNumChannels, in_uStreamIndex, in_fPitch );
	if ( eResult == AK_Success )
	m_usedVoices.AddFirst( pVoice );

	return eResult;
}

void AkACPMgr::UnregisterVoices( AkACPVoice ** in_pVoiceArray, AkUInt32 in_uNumVoices )
{
	AkAutoLock<CAkLock> Lock(m_lockWait);

	for (AkUInt32 i = 0; i < in_uNumVoices; ++ i)
	{
		if (in_pVoiceArray[i] != NULL)
		{
			m_usedVoices.Remove(in_pVoiceArray[i]);
			m_recycledVoices.AddFirst(in_pVoiceArray[i]);
		}
	}
}

// CleanUpVoices() is to be used only when allocation of a voice failes. It assumes a vaoice was never actually used, and does not recycle voices or call DecodingFinished().
void AkACPMgr::CleanUpVoices(AkACPVoice ** in_pVoiceArray, AkUInt32 in_uNumVoices)
{
	AkAutoLock<CAkLock> Lock(m_lockWait);

	for (AkUInt32 i = 0; i < in_uNumVoices; ++i)
	{
		if (in_pVoiceArray[i] != NULL)
		{
			AkACPVoice * pVoice = in_pVoiceArray[i];
			
			//The voice must not have been (ever) added to a flowgraph, started, used in any way.
#ifdef XMA_CHECK_VOICE_SYNC
			AKASSERT(!VoiceIsInFlowgraph(pVoice));
#endif
			AKASSERT(!pVoice->IsXmaCtxEnable());
			
			m_usedVoices.Remove(pVoice);
			m_freeVoices.AddFirst(pVoice); // <-Skipping the m_recycledVoices step
		}
	}
}

void AkACPMgr::ProcessMessageQueue()
{
	while (true)
	{
		ACP_MESSAGE message;
		if ( !m_pAcpHal->PopMessage( &message ) )
		{
			AKASSERT(m_bSpinCount++ < 50000000);
			break;
		}

		AKASSERT( !(m_bSpinCount = 0)/*<-reset spin count*/ );
		AKASSERT( message.droppedMessageCount == 0 );

		switch( message.type )
		{

		case ACP_MESSAGE_TYPE_ERROR:
		{
			if (message.error.errorCode == ACP_E_XMA_PARSER_ERROR)
			{
				SHAPE_XMA_CONTEXT * pXmaContext = &m_contexts.xmaContextArray[message.error.additionalData];
				for (AkUInt32 uHwVoiceIdx = 0; uHwVoiceIdx < g_PDSettings.uMaxXMAVoices; ++uHwVoiceIdx)
				{
					AkACPVoice& voice = m_pVoices[uHwVoiceIdx];
					if (voice.pXmaContext == pXmaContext)
					{
						voice.pOwner->SetFatalError();
						break;
					}
				}
			}
			else
			{
				DebugPrintAcpMessage(message);
			}
			break;
		}
		

		case ACP_MESSAGE_TYPE_FLOWGRAPH_TERMINATED:

			DebugPrintAcpMessage(message);
			AKASSERT(false);
			// no break --- 

		case ACP_MESSAGE_TYPE_FLOWGRAPH_COMPLETED:
			{
				AKASSERT( m_frames[m_uFgIdxRunning].m_bLaunched );
				
#ifdef AK_ENABLE_INSTRUMENT
				AK_PIX_SET_MARKER_HOOK( AK_PIX_COLOR(222,242,24), "Flowgraph Completed. Idx-%i, %i-running", m_uFgIdxRunning, m_uNumFlowgraphsRunning );
#endif
				m_frames[m_uFgIdxRunning].Reset();

				m_uFgIdxRunning = (m_uFgIdxRunning+1) % AK_NUM_DMA_FRAMES_PER_BUFFER;
				m_uNumFlowgraphsRunning--;
			}
			break;

		case ACP_MESSAGE_TYPE_COMMAND_COMPLETED:
			switch ( message.commandCompleted.commandId )
			{
			case AK_COOKIE_COMMANDID_DMA_INCREMENT:
				m_uNumPendingDMAReadIncrement--;
				break;
			case AK_COOKIE_COMMANDID_CTX_STOP:
			case AK_COOKIE_COMMANDID_CTX_START:
				m_uNumCtxEnableDisable--;
				break;
			case  AK_COOKIE_COMMANDID_LOAD_FLOWGRAPH:

				break;

			case  AK_COOKIE_COMMANDID_UPDATE_SRC_CONTEXTS:
			case  AK_COOKIE_COMMANDID_UPDATE_XMA_CONTEXTS:
			case  AK_COOKIE_COMMANDID_SEND_STOP_COMMAND:
				break;
			default:
				DebugPrintAcpMessage( message );
				AKASSERT( !"Unexpected command ID" );
				break;
			}
			///DebugPrintAcpMessage( message );
			break;

		default:
			DebugPrintAcpMessage( message );
			AKASSERT(false);
			break;
		}
	}
}

void AkACPMgr::WaitForFlowgraphs()
{
	if ( m_uNumFlowgraphsRunning > 0 ) // Perform initial test outside of critical section, to optimize common case.
	{
		AkAutoLock<CAkLock> lock(m_lockWait);
		WaitForFlowgraphsNoLock();
	}
}

void AkACPMgr::WaitForFlowgraphsNoLock()
{
	if (m_uNumFlowgraphsRunning == 0) // Necessary atomic version of the above test
	{
		return;
	}

	AK_INSTRUMENT_BEGIN_C(AK_PIX_COLOR(158,247,166), "AkACPMgr::WaitForFlowgraphs()");
		
	do
	{
		ProcessMessageQueue();
	}
	while ( m_uNumFlowgraphsRunning > 0 );

	AK_INSTRUMENT_END("");

#ifdef XMA_CHECK_VOICE_SYNC
	// Sometimes streams go out of sync after waiting for the flowgraphs... 
	//for (auto it = m_usedVoices.Begin(); it != m_usedVoices.End(); ++it)
	//	CheckAcpVoiceSync(*it);

	for (AkUInt32 i = 0; i < AK_NUM_DMA_FRAMES_PER_BUFFER; ++i)
		::ZeroMemory(&(m_frames[i].m_bfVoicesInFlowgraph), sizeof(UINT32)*SHAPE_NUM_XMA_CONTEXT_BITFIELDS);
#endif
}

// Wait for flowgraphs to finish, recycle all voices and stop XMA contexts of voices no longer in use.
void AkACPMgr::WaitForHwVoices()
{
	AkAutoLock<CAkLock> Lock(m_lockWait);

	WaitForFlowgraphsNoLock();

	// It is now safe to free buffers, re-use voices, and such.
	if (!m_recycledVoices.IsEmpty())
		RecycleVoices();

}

void AkACPMgr::WaitForCommandCompletion()
{
	if ( m_uNumPendingDMAReadIncrement > 0 ||
		m_uNumCtxEnableDisable > 0 	)
	{
		AK_INSTRUMENT_BEGIN_C(AK_PIX_COLOR(200,0,0), "AkACPMgr::WaitForCommandCompletion()");

		do
		{
			ProcessMessageQueue();
		} 
		while ( m_uNumPendingDMAReadIncrement > 0 ||
			m_uNumCtxEnableDisable > 0 	);

		AK_INSTRUMENT_END("");
	}
}

void AkACPMgr::IncrementDmaPointer( SHAPE_DMA_CONTEXT * in_pDmaContext, AkUInt32 in_uNumFramesIncrement )
{
	AkAutoLock<CAkLock> Lock(m_lockWait);

	UINT32 uDmaContextId = (UINT32) ( in_pDmaContext - m_contexts.dmaContextArray );

	ACP_COMMAND_INCREMENT_DMA_POINTER command = { uDmaContextId, in_uNumFramesIncrement };
	HRESULT hr = m_pAcpHal->SubmitCommand(ACP_COMMAND_TYPE_INCREMENT_DMA_READ_POINTER, AK_COOKIE_COMMANDID_DMA_INCREMENT, ACP_SUBMIT_PROCESS_COMMAND_ASAP, &command);
	AKASSERT( SUCCEEDED( hr ) );
	++m_uNumPendingDMAReadIncrement;
}

void AkACPMgr::RegisterForFlowgraphMessages() 
{
    ACP_COMMAND_MESSAGE command = { 0 };

    command.message = ACP_MESSAGE_TYPE_FLOWGRAPH_COMPLETED;
    command.message |= ACP_MESSAGE_TYPE_COMMAND_COMPLETED;
    command.message |= ACP_MESSAGE_TYPE_FLOWGRAPH_TERMINATED;
    //command.message |= ACP_MESSAGE_TYPE_SRC_BLOCKED;
    //command.message |= ACP_MESSAGE_TYPE_DMA_BLOCKED;
    command.message |= ACP_MESSAGE_TYPE_ERROR;
    //command.message |= ACP_MESSAGE_TYPE_DISCONNECTED;
    HRESULT hr = m_pAcpHal->SubmitCommand(ACP_COMMAND_TYPE_REGISTER_MESSAGE, 0, ACP_SUBMIT_PROCESS_COMMAND_ASAP, &command);
    AKASSERT( SUCCEEDED( hr ) );
}

void AkACPMgr::UpdatePitchTarget(AkACPVoice* in_pVoice, AkUInt32 in_uShapeFrameIdx, AkUInt32 in_uTargetSamplingIncr)
{
	AkAutoLock<CAkLock> Lock(m_lockWait);

#if AK_SHAPE_SRC_PITCH_RAMP
	if ( in_uTargetSamplingIncr != in_pVoice->pSrcContext->samplingIncrementTarget )
	{
		AkUInt32 uFgIdx = m_uFgIdxNextFrame + in_uShapeFrameIdx;
		AKASSERT(uFgIdx < AK_NUM_DMA_FRAMES_PER_BUFFER);

		AkShapeFrame& frame = m_frames[uFgIdx];

		AKASSERT(frame.m_uSrcUpdateCommandsCount < g_PDSettings.uMaxXMAVoices);

		UINT32 uSrcContextIdx = (UINT32) ( in_pVoice->pSrcContext - m_contexts.srcContextArray );

		ACP_COMMAND_UPDATE_SRC_CONTEXT_ENTRY& entry = frame.m_pSrcUpdateCommands[frame.m_uSrcUpdateCommandsCount];

		entry.contextIndex = uSrcContextIdx;
		entry.samplingIncrementTarget = in_uTargetSamplingIncr;

		frame.m_uSrcUpdateCommandsCount++;
	}
#else
	AKASSERT(m_uNumFlowgraphsRunning == 0);
	if (in_uShapeFrameIdx == 0)
		in_pVoice->pSrcContext->samplingIncrementTarget = in_uTargetSamplingIncr;
#endif
}


void AkACPMgr::UpdateXmaLoopCount(AkACPVoice* in_pVoice, AkUInt32 in_uLoopCount)
{
	AkAutoLock<CAkLock> Lock(m_lockWait);

	//TODO: make this asynchronous

	AKASSERT( in_pVoice );
	UINT32 numLoops = ( in_uLoopCount == LOOPING_INFINITE ) ? SHAPE_XMA_INFINITE_LOOP_COUNT : AkMin( SHAPE_XMA_MAX_LOOP_COUNT, in_uLoopCount-1 );
	
	bool bEnabledVoice = in_pVoice->IsXmaCtxEnable();
	if( bEnabledVoice )
		StopXmaContext_Sync(in_pVoice);

	HRESULT hr = SetShapeXmaNumLoops( in_pVoice->pXmaContext, numLoops );
	AKVERIFY( SUCCEEDED( hr ) );

	if( bEnabledVoice )
		StartXmaContext_Async(in_pVoice);
	
}

void * AkACPMgr::GetApuBuffer(AkUInt32 in_uSize, AkUInt32 in_uAlign)
{
	AKASSERT(in_uAlign % 256 == 0);
	return AkMalign(AkMemID_Processing | AkMemType_Device, in_uSize, in_uAlign);
}

void AkACPMgr::ReleaseApuBuffer(void * in_pvBuffer)
{
	AkFalign(AkMemID_Processing | AkMemType_Device, in_pvBuffer);
}

void AkACPMgr::RecycleVoices()
{
	StopRecycledXmaContexts();
	WaitForCommandCompletion();
	FinishRecycleAndNotify();
}

bool AkACPMgr::StopRecycledXmaContexts()
{
	AK_INSTRUMENT_BEGIN_C(AK_PIX_COLOR(158, 247, 243), "AkACPMgr::StopRecycledXmaContexts()");

	AKASSERT(m_uNumFlowgraphsRunning == 0);

	ACP_COMMAND_ENABLE_OR_DISABLE_XMA_CONTEXTS  commandXma;
	::ZeroMemory(&commandXma, sizeof(commandXma));
	AkUInt32 uNumCmdSubmitted = 0;

	for (auto it = m_recycledVoices.BeginEx(); it != m_recycledVoices.End(); ++it)
	{
		AkACPVoice * pVoice = (*it);
		if (pVoice->IsXmaCtxEnable())
		{
			UINT32 uXmaContextId = (UINT32)(pVoice->pXmaContext - m_contexts.xmaContextArray);
			commandXma.context[uXmaContextId >> SHAPE_XMA_CONTEXT_INDEX_SHIFT] |= (1 << (uXmaContextId & SHAPE_XMA_CONTEXT_BIT_MASK));
			pVoice->m_bXmaCtxEnable = false;
			uNumCmdSubmitted++;
		}
	}

	if (uNumCmdSubmitted > 0)
	{
		HRESULT hr = m_pAcpHal->SubmitCommand(ACP_COMMAND_TYPE_DISABLE_XMA_CONTEXTS, AK_COOKIE_COMMANDID_CTX_STOP, ACP_SUBMIT_PROCESS_COMMAND_ASAP, &commandXma);
		AKASSERT(SUCCEEDED(hr));
		m_uNumCtxEnableDisable++;
	}

	AK_INSTRUMENT_END("");

	return uNumCmdSubmitted > 0;
}

void AkACPMgr::FinishRecycleAndNotify()
{
	AK_INSTRUMENT_BEGIN_C(AK_PIX_COLOR(158, 247, 243), "AkACPMgr::FinishRecycleAndNotify()");

	AKASSERT(m_uNumCtxEnableDisable == 0);

	while (!m_recycledVoices.IsEmpty())
	{
		auto it = m_recycledVoices.BeginEx();
		AkACPVoice * pVoice = (*it);

		AKASSERT(!pVoice->IsXmaCtxEnable());

		if (pVoice->pOwner != NULL)
			pVoice->pOwner->HwVoiceFinished();
		
		// Move all recycled voices to m_freeVoices.
		m_recycledVoices.RemoveFirst();
		m_freeVoices.AddFirst(pVoice);
	}

	AK_INSTRUMENT_END("");
}
