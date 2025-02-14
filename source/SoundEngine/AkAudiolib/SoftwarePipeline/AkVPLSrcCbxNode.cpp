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

/////////////////////////////////////////////////////////////////////
//
// AkVPLSrcCbxNode.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include "AkVPLSrcCbxNode.h"

#include "AkEffectsMgr.h"
#include "AkFXMemAlloc.h"
#include "AkSoundBase.h"
#include "AkVPLFilterNode.h"
#include "AkVPLFilterNodeOutOfPlace.h"
#include "Ak3DListener.h"
#include "AkPositionRepository.h"
#include "AkPlayingMgr.h"
#include "AkSrcPhysModel.h"

#define MAX_NODES		( 5 + AK_NUM_EFFECTS_PER_OBJ )	// Max nodes in the cbx node list. 4 is "LPF + HPF + Pitch + Source + VolumeAutomation"

extern AkInitSettings g_settings;

AkReal32 g_fVolumeThreshold = AK_OUTPUT_THRESHOLD;
AkReal32 g_fVolumeThresholdDB = AK_MINIMUM_VOLUME_DBFS;

AkVPLSrcCbxRec::AkVPLSrcCbxRec()
{
	for ( AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex )
		m_pFilter[ uFXIndex ] = NULL;
}

void AkVPLSrcCbxRec::ClearVPL()
{
	for ( AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex )
	{
		if( m_pFilter[ uFXIndex ] != NULL )
		{
			m_pFilter[ uFXIndex ]->Term();
			AkDelete( AkMemID_Processing, m_pFilter[ uFXIndex ] );
			m_pFilter[ uFXIndex ] = NULL;
		}
	}

	m_BQF.Term();
	m_VolAutm.Term();
}

CAkVPLSrcCbxNode::CAkVPLSrcCbxNode()
	: m_pPitch(NULL)
	, m_eState( NodeStateInit )
	, m_bHasStarved( false )
	, m_bPipelineAdded( false )
#ifndef AK_OPTIMIZED
	, m_iWasStarvationSignaled( 0 )
#endif
	, m_previousMakeUpGainDB( FLT_MAX )
	, m_previousMakeUpGainLinearNormalized( FLT_MAX )
{
	for ( AkUInt32 i = 0; i < MAX_NUM_SOURCES; ++i )
		m_pSources[i] = NULL;
}

CAkVPLSrcCbxNode::~CAkVPLSrcCbxNode()
{
	AKASSERT(m_inputs.IsEmpty());	// no inputs on voices.
	AKASSERT(m_pPitch == NULL);
}

void CAkVPLSrcCbxNode::Term()
{
	RemovePipeline(CtxDestroyReasonFinished);

	// Remove next sources.
	for (AkUInt32 i = 1; i < MAX_NUM_SOURCES; i++)
	{
		if (m_pSources[i])
		{
			m_pSources[i]->Term(CtxDestroyReasonFinished);
			AkDelete(AkMemID_Processing, m_pSources[i]);
			m_pSources[i] = NULL;
		}
	}
} // Term

//-----------------------------------------------------------------------------
// Name: Start
// Desc: Indication that processing will start.
//
// Parameters:
//	None.
//-----------------------------------------------------------------------------

void CAkVPLSrcCbxNode::Start()
{
	// Do not start node if not in init state.
	if( m_eState != NodeStateInit )
	{
		// Only report error if sound was not stopped... otherwise a false error.
		if( m_eState != NodeStateStop )
		{
			Stop();
			MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_CannotPlaySource_InconsistentState, m_pSources[0]->GetContext() );
		}
		return;
	}

	// Start the current node
	m_pSources[ 0 ]->Start();

#ifndef AK_OPTIMIZED
	RecapPluginParamDelta();
#endif

	m_eState = NodeStatePlay;
} // Start

//-----------------------------------------------------------------------------
// Name: FetchStreamedData
// Desc: Fetches streaming data of source record 0. 
//		 If the source is ready and the context is ready to be connected, 
//		 returns success.
//
// Return:
//	Ak_Success: Ready to create pipeline.
//	Ak_FormatNotReady: Not ready to create pipeline.
//-----------------------------------------------------------------------------
AKRESULT CAkVPLSrcCbxNode::FetchStreamedData(
	CAkPBI * in_pCtx 
	)
{
	AKASSERT( m_pSources[ 0 ] );

	AkUInt8 * pvBuffer;
    AkUInt32 ulBufferSize;
	m_pSources[ 0 ]->GetContext()->GetDataPtr( pvBuffer, ulBufferSize );

	AKRESULT l_eResult = m_pSources[ 0 ]->FetchStreamedData( pvBuffer, ulBufferSize );

	if ( l_eResult == AK_FormatNotReady )
	{
		// Not ready. Bail out. 
		// Notify source starvation if applicable.
		if ( in_pCtx->GetFrameOffset() < 0 )
		{
			// We're late, and the source is not prebuffering.
			HandleSourceStarvation();
		}
		return AK_FormatNotReady;
	}
	else if ( l_eResult == AK_Success )
	{
		// Optimization: keep sources disconnected until they are needed, that is, when their frame offset 
		// is smaller than one sink frame (will run within this frame) + the amount of look-ahead quanta.
		AkInt32 iMaxOffset = (1 + g_settings.uContinuousPlaybackLookAhead) * AK_NUM_VOICE_REFILL_FRAMES;

		if ( in_pCtx->GetFrameOffset() < in_pCtx->ScaleFrameSize( iMaxOffset ) )
		{
			// We're ready, and frame offset is smaller than an audio frame. Connect now 
			// to start pulling data in this audio frame.
			// Source is ready: Add pipeline.
			AKASSERT( m_pSources[ 0 ]->IsIOReady() );

			// Post Source Starvation notification if context requires sample accuracy.
			if ( in_pCtx->GetFrameOffset() < 0 )
			{
				// We're late, and the source is not prebuffering.
				HandleSourceStarvation();
			}
			return AK_Success;			
		}
		else
		{
			// Frame offset greater than an audio frame: keep it in list of sources not connected.
			return AK_FormatNotReady;
		}
	}
	return AK_Fail;
}

//-----------------------------------------------------------------------------
// Name: Pause
// Desc: Pause the source.
//
// Parameters:
//	None.
//-----------------------------------------------------------------------------
void  CAkVPLSrcCbxNode::Pause()
{
	switch( m_eState )
	{
	case NodeStatePlay:
		AKASSERT( m_pSources[ 0 ] );
		m_pSources[ 0 ]->Pause();
		m_eState = NodeStatePause;
		break;

	case NodeStatePause:
		// We were already paused, nothing to do.
		// Do Nothing : WG-33448
		break;

	default:
		Stop();
		MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_CannotPlaySource_InconsistentState, m_pSources[0]->GetContext());
		break;
	}
} // Pause

void CAkVPLSrcCbxNode::SetAudible( CAkPBI * in_pCtx, bool in_bAudible )
{
	AKASSERT( in_pCtx );

	if (m_eBelowThresholdBehavior != AkBelowThresholdBehavior_ContinueToPlay)
	{
		if (m_bAudible)
		{
			if (!in_bAudible)
			{
				in_pCtx->Virtualize();
			}
		}
		else if (in_bAudible)
		{
			in_pCtx->Devirtualize();
		}
	}
	
	m_bAudible = in_bAudible;
}

//-----------------------------------------------------------------------------
// Name: Stop
// Desc: Indication that processing will stop.
//
// Parameters:
//	None.
//-----------------------------------------------------------------------------
void CAkVPLSrcCbxNode::Stop()
{
	// Stop the current node

	if ( m_pSources[ 0 ] )
		m_pSources[ 0 ]->Stop();

	m_eState = NodeStateStop;
} // Stop



//-----------------------------------------------------------------------------
// Name: Resume
// Desc: Resume the source.
//
// Parameters:
//	None.
//-----------------------------------------------------------------------------
void CAkVPLSrcCbxNode::Resume()
{
	switch( m_eState )
	{
	case NodeStatePause:
		AKASSERT( m_pSources[ 0 ] );
		m_pSources[ 0 ]->Resume( GetLastRate() );
		m_eState = NodeStatePlay;
		break;

	case NodeStatePlay:
		// We were not paused, nothing to resume.
		// Do Nothing
		// This can happen and is not an error, see : WG-16984
		break;

	default:
		// Do not resume node if not in pause state
		Stop();
		MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_CannotPlaySource_InconsistentState, m_pSources[0]->GetContext());
		break;
	}
} // Resume

bool CAkVPLSrcCbxNode::StartRun( AkVPLState & io_state )
{
	// Do this as soon as possible to avoid a LHS on some platforms.
	const AkChannelConfig channelConfig = m_channelConfig;

	CAkVPLSrcNode * pSrcNode = m_pSources[0];
	CAkPBI * pSrcContext = pSrcNode->GetContext(); // remember the FIRST context to play in this frame
	AKASSERT( pSrcContext != NULL );

	AkUInt32 ulStopOffset = pSrcContext->GetStopOffset();
	if ( AK_EXPECT_FALSE( ulStopOffset != AK_NO_IN_BUFFER_STOP_REQUESTED ) )
	{
		m_eState = NodeStateStop;
		if ( AK_EXPECT_FALSE( ulStopOffset == 0 ) )
		{
			return false; // stop immediately: no need to run at all.
		}
	}

	io_state.ClearData();
	io_state.SetChannelConfig( channelConfig );
	io_state.SetRequestSize(AK_NUM_VOICE_REFILL_FRAMES);

	bool bNextSilent, bAudible;

	SpeakerVolumeMatrixCallback cb(pSrcContext->GetPlayingID(), channelConfig);

	UpdateMakeUpLinearNormalized( pSrcContext, pSrcNode );
	AkReal32 fMakeupGainLinearNormalized = m_previousMakeUpGainLinearNormalized;

#ifndef AK_OPTIMIZED
	AkReal32 fNormalization = pSrcNode->GetNormalization();
	pSrcContext->OpenSoundBrace();
	AkDeltaMonitor::LogDelta(AkDelta_Normalization, AkPropID_Volume, fNormalization, 1.0f);
	m_fNormalizationGain = fMakeupGainLinearNormalized * pSrcContext->GetModulatorData().GetPeakOutput(AkRTPCBitArray(1ULL << RTPC_MakeUpGain));	//GetPeakOutput logs the modulators data.
	pSrcContext->CloseSoundBrace();
#endif

	GetVolumes( pSrcContext, channelConfig, fMakeupGainLinearNormalized, pSrcNode->StartWithFadeIn(), bNextSilent, bAudible,
		((pSrcContext->GetRegisteredNotif() & AK_SpeakerVolumeMatrix) ? &cb : NULL));

	// Set voice LPF.
	const AkSoundParams & params = pSrcContext->GetEffectiveParams();
	m_cbxRec.m_BQF.SetLPF(params.LPF());
	m_cbxRec.m_BQF.SetHPF(params.HPF());

	bool bNeedToRun = true;
	AkInt32 iFrameSize = pSrcContext->ScaleFrameSize( io_state.MaxFrames() );

	if ( bAudible ) 
	{
		// Switching from non-audible to audible
		if ( !m_bAudible || !pSrcNode->IsIOReady() )
		{
			if ( m_eBelowThresholdBehavior == AkBelowThresholdBehavior_SetAsVirtualVoice ) 
			{
				if( !pSrcNode->IsIOReady() )
				{
					// Note: The pipeline is connected but streaming is not ready. This is the consequence
					// of the "voice initially virtual faking scheme" (see AddSrc). Let's repair it now.

					// IMPORTANT: Behavioral handling of virtual voices should be done here as well.
					// However this path is only used with FromBeginning voices Initially Under Threshold,
					// and currently no high-level engine handles this case differently.
					/// AkBehavioralVirtualHandlingResult eVirtualHandlingResult = pSrcContext->NotifyVirtualOff( m_eVirtualBehavior );
					AKASSERT( pSrcContext->NotifyVirtualOff( m_eVirtualBehavior ) == VirtualHandling_NotHandled );

					AkUInt8 * pvBuffer;
					AkUInt32 ulBufferSize;
					pSrcContext->GetDataPtr( pvBuffer, ulBufferSize );
					AKRESULT l_StreamResult = pSrcNode->FetchStreamedData( pvBuffer, ulBufferSize );
					if( l_StreamResult == AK_FormatNotReady )
					{
					    // not ready, let gain some time by telling we are not audible.
						bAudible = false;
						bNeedToRun = false;
					}
					else if( l_StreamResult != AK_Success )
					{
						Stop();
						// not ready, let's gain some time by telling we are not audible.
						bAudible = false;
						bNeedToRun = false;
					}
				}
				else // yes, must check it again, may have been set to false in previous call.
				{
					// Notify the upper engine now before processing VirtualOff on the pipeline.
					bool bVirtualOffUseSourceOffset = false;
					AkCtxVirtualHandlingResult eVirtualHandlingResult = pSrcContext->NotifyVirtualOff( m_eVirtualBehavior );
					switch ( eVirtualHandlingResult )
					{
					case VirtualHandling_ShouldStop:
						// Stop and don't monitor: the behavioral engine thinks that there is no point in restarting this voice.
						Stop();
						return false;
					case VirtualHandling_RequiresSeeking:
						bVirtualOffUseSourceOffset = true;
						break;
					default:
						break;
					}

					if ( m_cbxRec.Head()->VirtualOff( m_eVirtualBehavior, bVirtualOffUseSourceOffset ) != AK_Success )
					{
						Stop();
						MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_CannotPlaySource_VirtualOff, pSrcContext);
						return false;
					}
				}
			}
		}
	}
	else // !audible
	{
		if ( m_eBelowThresholdBehavior == AkBelowThresholdBehavior_SetAsVirtualVoice ) 
		{
			bNeedToRun = false;

			// Switching from audible to non-audible
			if ( m_bAudible ) 
			{
				m_cbxRec.Head()->VirtualOn( m_eVirtualBehavior );

				if ( m_eVirtualBehavior != AkVirtualQueueBehavior_Resume ) 
				{
					AKASSERT( m_eVirtualBehavior == AkVirtualQueueBehavior_FromBeginning 
						|| m_eVirtualBehavior == AkVirtualQueueBehavior_FromElapsedTime );

					// Release any incomplete output buffer.
					// Note. We could keep incomplete output buffers and consume them
					// before time skipping, but the pitch node does not support it.
					// Release (we dropped a few milliseconds)
					m_cbxRec.Head()->ReleaseBuffer();

					// Also post and release any pending markers
					AkMarkerNotificationRange range = m_PendingMarkers.Front();
					g_pPlayingMgr->NotifyMarkers(range.pMarkers, range.uLength);
					m_PendingMarkers.Dequeue(range.uLength);
				}
			}

			if ( m_eVirtualBehavior == AkVirtualQueueBehavior_FromElapsedTime )
			{
				// Time skip.
				// Note: Frame offset is consumed below.
				AkInt32 iFrameOffset = pSrcContext->GetFrameOffset();
				if( iFrameOffset < iFrameSize )
					io_state.result = SourceTimeSkip( iFrameSize );
			}
		}
		else if ( m_eBelowThresholdBehavior == AkBelowThresholdBehavior_KillVoice )
		{
			//Setting it to no more data will directly term it.
			Stop();
			bNeedToRun = false;

			pSrcContext->Monitor(AkMonitorData::NotificationReason_KilledVolumeUnderThreshold);
		}
	}

	// Handle source starvation.
	if ( AK_EXPECT_FALSE( m_bHasStarved ) )
	{
		// Seek source to correct position if pSrcContext supports sample-accurate VirtualOff through seeking.
		AkCtxVirtualHandlingResult eVirtualHandlingResult = pSrcContext->NotifyVirtualOff( m_eVirtualBehavior );
		if ( VirtualHandling_RequiresSeeking == eVirtualHandlingResult )
		{
			SeekSource();
			SetPreviousSilentOnAllConnections( true );	// To force a fade-in upon return.
		}
		else if ( VirtualHandling_ShouldStop == eVirtualHandlingResult )
		{
			Stop();
			bNeedToRun = false;
		}
			
		m_bHasStarved = false;
	}

	// upstream nodes are not yet required to output data if frame offset greater than one frame.
	AkInt32 iFrameOffset = pSrcContext->GetFrameOffset();
	bNeedToRun = bNeedToRun && ( iFrameOffset < iFrameSize );
	pSrcContext->ConsumeFrameOffset( iFrameSize );

	// Update state.
	SetAudible( pSrcContext, bAudible );

	if ( bNeedToRun )
	{
		if ( AK_EXPECT_FALSE( !PipelineAdded() ) )
		{
			AKRESULT eAddResult = AddPipeline();
			if ( eAddResult == AK_Success )
			{
				pSrcContext->CalcEffectiveParams(); // Motion parameters are invalidated by StartStream()
				// Now recalc volume using proper channel mask.
				pSrcContext->InvalidatePrevPosParams();
				io_state.SetChannelConfig( m_channelConfig );

				// For volume re-computation, act as if this was the first run.
				m_bFirstBufferProcessed = false;
				// AkReal32 fDummy;
				UpdateHDR();

				UpdateMakeUpLinearNormalized( pSrcContext, pSrcNode );
				fMakeupGainLinearNormalized = m_previousMakeUpGainLinearNormalized;

				SpeakerVolumeMatrixCallback cb(pSrcContext->GetPlayingID(), channelConfig);
				GetVolumes(pSrcContext, m_channelConfig, fMakeupGainLinearNormalized, pSrcNode->StartWithFadeIn(), bNextSilent, bAudible,
					((pSrcContext->GetRegisteredNotif() & AK_SpeakerVolumeMatrix) ? &cb : NULL));
			}
			else
			{
				Stop();
				bNeedToRun = false;
			}
		}
	}
	m_bFirstBufferProcessed = true;

	return bNeedToRun;
}

void CAkVPLSrcCbxNode::GetOneSourceBuffer(AkVPLState & io_state)
{
	io_state.SetRequestSize( LE_MAX_FRAMES_PER_BUFFER );

#ifndef AK_OPTIMIZED
	AkAudiolibTimer::Item * pTimerItem = NULL;
	{
		AkSrcTypeInfo * pSrcType = GetPBI()->GetSrcTypeInfo();
		AkPluginID dwID = pSrcType->mediaInfo.Type == SrcTypeModelled ? static_cast<CAkSrcPhysModel *>(m_pSources[0])->GetFxID() : pSrcType->dwID;
		pTimerItem = AK_START_TIMER(0, dwID, GetPBI()->GetPipelineID());
	}
#endif

	m_pSources[ 0 ]->GetBuffer( io_state );

#ifndef AK_OPTIMIZED
	AK_STOP_TIMER(pTimerItem);
	if ( io_state.result == AK_DataReady
		|| io_state.result == AK_NoMoreData )
	{
		if( m_iWasStarvationSignaled )
		{
			--m_iWasStarvationSignaled;
		}
	}
	else 
#endif
	{
		/// IMPORTANT: This needs to be done in Release also.
		if ( io_state.result == AK_NoDataReady )
			HandleSourceStarvation();
	}
	
#ifndef AK_OPTIMIZED
	if (g_settings.bDebugOutOfRangeCheckEnabled && GetPBI()->GetMediaFormat().GetTypeID() == AK_FLOAT && !io_state.CheckValidSamples())
	{
		AK::Monitor::PostString("Wwise audio out of range: After calling GetBuffer on a source", AK::Monitor::ErrorLevel_Error, GetPBI()->GetPlayingID(), GetPBI()->GetGameObjectPtr()->ID(), GetPBI()->GetSoundID());
	}
#endif
}

void CAkVPLSrcCbxNode::GetBuffer(AkVPLState & io_state)
{
	if (!m_pPitch)
	{
		// Trust source to handle resampling and pitch.
		GetOneSourceBuffer(io_state);
		if (io_state.result == AK_NoMoreData && m_pSources[1])
		{
			// Switch to next source.
			io_state.result = TrySwitchToNextSrc(io_state);
			if (io_state.result == AK_NoMoreData)
			{
				return; // Next source was rejected, transition will not be sample-accurate
			}
			if (io_state.result == AK_DataNeeded)
			{
				GetOneSourceBuffer(io_state);
			}
		}
		return;
	}

	// Pitch-FmtConv-Source loop
	m_pPitch->GetBuffer( io_state );
	if ( io_state.result != AK_DataNeeded )
	{
		return;
	}

	do
	{
		GetOneSourceBuffer(io_state);
		if ( io_state.result != AK_DataReady
			&& io_state.result != AK_NoMoreData )
		{
			return;
		}

		m_pPitch->ConsumeBuffer( io_state );
		if ( io_state.result != AK_DataNeeded )
		{
			return;
		}
	}
	while(1);
}

void CAkVPLSrcCbxNode::ConsumeBuffer( AkVPLState & io_state )
{
	CAkPBI * pCtx = m_pSources[ 0 ]->GetContext();
	if ( pCtx->GetRegisteredNotif() & AK_EnableGetSourcePlayPosition
		&& io_state.posInfo.IsValid() )
	{
		// The cookie is the source.
		g_pPositionRepository->UpdatePositionInfo( pCtx->GetPlayingID(), &io_state.posInfo, m_pSources[ 0 ] );
	}

	// Handle stop offsets.
	// IMPORTANT: Currently stop offsets and sample accurate containers using stitch buffers are 2 paths that are
	// mutually exclusive. If this changes, changing the number of valid frames on the io_state should be done 
	// differently with m_bufferMix.
	AkUInt32 uStopOffset = pCtx->GetAndClearStopOffset();
	if ( AK_EXPECT_FALSE( uStopOffset != AK_NO_IN_BUFFER_STOP_REQUESTED ) )
	{
		if ( uStopOffset < io_state.uValidFrames )
			io_state.uValidFrames = (AkUInt16)uStopOffset;
		m_eState = NodeStateStop;
	}

	/////////////////////////////////////////////////////////////////////
	// If a sample accurate source is pending, we must keep on fetching 
	// it since all PCMex files will require multiple fetch..
	// WG-14332
	// The problem could also occur if the file header is too big to make it possible to have the sound sample accurate.

	if ( m_pSources[ 1 ] )
	{
		AkUInt8 * pvBuffer;
		AkUInt32 ulBufferSize;
		m_pSources[ 1 ]->GetContext()->GetDataPtr( pvBuffer, ulBufferSize );
		AKRESULT eResult = m_pSources[ 1 ]->FetchStreamedData( pvBuffer, ulBufferSize );
		if ( eResult == AK_Fail )
		{
			io_state.result = AK_Fail;
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Name: ReleaseBuffer
// Desc: Releases a processed buffer.
//-----------------------------------------------------------------------------
void CAkVPLSrcCbxNode::ReleaseBuffer()
{
	// We need to release an upstream node's buffer.
	m_cbxRec.Head()->ReleaseBuffer();
	m_pMixableBuffer = NULL;
} // ReleaseBuffer

// Switch to next source on the combiner in the sample-accurate transition scenario
AKRESULT CAkVPLSrcCbxNode::TrySwitchToNextSrc(const AkPipelineBuffer &io_state)
{
	AKRESULT eResult;
	CAkPBI * pPBI = m_pSources[ 0 ]->GetContext();
	CAkPBI * pPBINew = m_pSources[ 1 ]->GetContext();

	// Check if there is a frame offset; we can honor it if it is larger than the space remaining in the buffer.
	// XXX TODO how to apply this to non-pitchnode code paths?
	AkInt32 iFrameOffset = pPBINew->GetFrameOffset();
	if( iFrameOffset > 0 )
	{
		AkInt32 iNextRequestFrames = pPBINew->ScaleFrameSize( io_state.MaxFrames() - io_state.uValidFrames);
		pPBINew->ConsumeFrameOffset( AkMin( iNextRequestFrames, iFrameOffset ) );

		return AK_NoMoreData; // Then catch nomoredata in runvpl and do an old-fashioned addpipeline with m_pSources[ 1 ]
	}

	AkUInt8 * pvBuffer;
    AkUInt32 ulBufferSize;
	pPBINew->GetDataPtr( pvBuffer, ulBufferSize );

	AKRESULT eResultFetch = m_pSources[ 1 ]->FetchStreamedData( pvBuffer, ulBufferSize );
	if ( eResultFetch == AK_FormatNotReady )
	{
		return AK_NoMoreData;
	}
	else if ( eResultFetch != AK_Success )
	{
		return AK_Fail;
	}

	// Only now that the next source has 'fetched' can we get the format
	AkAudioFormat fmtOld = pPBI->GetMediaFormat();
	AkAudioFormat fmtNew = pPBINew->GetMediaFormat();
	if ( fmtOld.channelConfig != fmtNew.channelConfig ) 
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_TransitionNotAccurateChannel, pPBI );
		return AK_NoMoreData;
	}

	if ((m_pPitch == NULL) != m_pSources[1]->SupportResampling())
	{
		// Different pitch node requirements; cannot proceed with sample-accurate transition.
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_TransitionNotAccuratePluginMismatch, pPBI );
		return AK_NoMoreData;
	}

	if (!m_pPitch)
	{
		// Since the source is going to handle the sample-accurate transition, the plugin IDs must match as well.
		AkSrcTypeInfo *pSrcTypeInfo = pPBI->GetSrcTypeInfo();

		if (pSrcTypeInfo->dwID != pSrcTypeInfo->dwID)
		{
			MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_TransitionNotAccuratePluginMismatch, pPBI );
			return AK_NoMoreData;
		}

		// Attempt the switch. Plugins themselves may reject it.
		eResult = m_pSources[0]->SwitchToNextSrc(m_pSources[1]);
		if (eResult != AK_Success)
		{
			MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_TransitionNotAccurateRejectedByPlugin, pPBI );
			return AK_NoMoreData;
		}
	}

	eResult = m_pSources[1]->Init();
	if (eResult != AK_Success)
	{
		return eResult;
	}

	pPBINew->CalcEffectiveParams();

	SwitchToNextSrc(); // This performs the change on all other nodes

	eResult = io_state.uValidFrames != io_state.MaxFrames() ? AK_DataNeeded : AK_DataReady;

	return eResult;
}

void CAkVPLSrcCbxNode::SwitchToNextSrc()
{
	if (m_pPitch)
	{
		m_pPitch->SwitchToNextSrc(m_pSources[1]);
	}
	else
	{
		// Find next link in the chain and connect it to the next source
		bool bFound = false;
		for (AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex)
		{
			if (m_cbxRec.m_pFilter[uFXIndex] != NULL)
			{
				m_cbxRec.m_pFilter[uFXIndex]->Connect(m_pSources[1]);
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			m_cbxRec.m_BQF.Connect(m_pSources[1]);
		}
	}

	m_pSources[ 0 ]->Term( CtxDestroyReasonFinished );
	AkDelete( AkMemID_Processing, m_pSources[ 0 ] );

	m_pSources[ 0 ] = m_pSources[ 1 ];
	m_pContext = m_pSources[0]->GetContext(); //<- update m_pContext to point to new source.
	m_pSources[ 1 ] = NULL;


	m_pSources[ 0 ]->Start();
	m_cbxRec.m_VolAutm.SetPBI(m_pSources[0]->GetContext());
	m_pSources[0]->GetContext()->WakeUpVoice();
}

void CAkVPLSrcCbxNode::UpdateFx( AkUInt32 in_uFXIndex )
{
	if ( in_uFXIndex == (AkUInt32) -1 ) 
	{
		AKASSERT( false && "UpdateFx not supported for source plugin!" );
		return;
	}
	// Find neighbors of the node (for re-connection)

	CAkVPLNode * pNext = NULL;
	CAkVPLNode * pPrev = NULL;

	bool bPrevIsFilter = false;
	bool bNextIsFilter = false;

	{	
		for ( int iNextFX = in_uFXIndex + 1; iNextFX < AK_NUM_EFFECTS_PER_OBJ; ++iNextFX )
		{
			if ( m_cbxRec.m_pFilter[ iNextFX ] )
			{
				pNext = m_cbxRec.m_pFilter[ iNextFX ];
				bNextIsFilter = true;
				break;
			}
		}

		if ( pNext == NULL )
			pNext = &m_cbxRec.m_BQF;

		for ( int iPrevFX = in_uFXIndex - 1; iPrevFX >= 0; --iPrevFX )
		{
			if ( m_cbxRec.m_pFilter[ iPrevFX ] )
			{
				pPrev = m_cbxRec.m_pFilter[ iPrevFX ];
				bPrevIsFilter = true;
				break;
			}
		}
		if (pPrev == NULL)
			pPrev = m_pPitch ? (CAkVPLNode*)m_pPitch : (CAkVPLNode*)m_pSources[0];
	}

	// Determine input format

	CAkPBI * pCtx = m_pSources[ 0 ]->GetContext();
	AkAudioFormat informat = pCtx->GetMediaFormat();
	if ( bPrevIsFilter )
		informat.channelConfig = static_cast<CAkVPLFilterNodeBase *>( pPrev )->GetOutputConfig();
	informat.uBitsPerSample = AK_LE_NATIVE_SAMPLETYPE == AK_FLOAT ? 32 : 16;
	informat.uBlockAlign = informat.GetNumChannels() * informat.uBitsPerSample / 8;
	informat.uInterleaveID = AK_LE_NATIVE_INTERLEAVE;
	informat.uSampleRate = AK_CORE_SAMPLERATE;
	informat.uTypeID = AK_LE_NATIVE_SAMPLETYPE;

	AkChannelConfig uOutConfig = informat.channelConfig;

	// Remove existing FX 

	CAkVPLFilterNodeBase * pFilter = m_cbxRec.m_pFilter[ in_uFXIndex ];
	if ( pFilter )
	{
		uOutConfig = pFilter->GetOutputConfig();

		// Out of place effect possibly consumed part of the buffer so release it, in place effect don<t need to
		// Need to release references to input buffer all the way to the next out of place effect
		for ( int iNextFX = in_uFXIndex; iNextFX < AK_NUM_EFFECTS_PER_OBJ; ++iNextFX )
		{
			if ( m_cbxRec.m_pFilter[ iNextFX ] )
			{	
				bool bBufferReleased = m_cbxRec.m_pFilter[ iNextFX ]->ReleaseInputBuffer();
				if ( bBufferReleased && iNextFX != in_uFXIndex )
					break;
			}
		}

		pNext->Disconnect( );

		pFilter->Term();
		AkDelete( AkMemID_Processing, pFilter );
		pFilter = NULL;

		m_cbxRec.m_pFilter[ in_uFXIndex ] = NULL;
	}
	
	// Insert new FX

	CAkSoundBase * pSound = pCtx->GetSound();		

	AkFXDesc fxDesc;
	pSound->GetFX( in_uFXIndex, fxDesc, pCtx->GetGameObjectPtr() );

	AkChannelConfig uNewOutConfig;

	if (fxDesc.pFx)
	{
		IAkPlugin * pPlugin = NULL;

		AkPluginInfo pluginInfo;
		AKRESULT l_eResult = CAkEffectsMgr::Alloc(fxDesc.pFx->GetFXID(), pPlugin, pluginInfo);
		if (l_eResult != AK_Success)
		{
			MONITOR_PLUGIN_ERROR(AK::Monitor::ErrorCode_PluginAllocationFailed, pCtx, fxDesc.pFx->GetFXID());
			pNext->Connect(pPrev);
			return; // Silently fail, add pipeline will continue with the pipeline creation without the effect
		}

		AK::Monitor::ErrorCode errCode = CAkEffectsMgr::ValidatePluginInfo(fxDesc.pFx->GetFXID(), AkPluginTypeEffect, pluginInfo);
		if (errCode != AK::Monitor::ErrorCode_NoError)
		{
			MONITOR_PLUGIN_ERROR(errCode, pCtx, fxDesc.pFx->GetFXID());
			pPlugin->Term(AkFXMemAlloc::GetLower());
			pNext->Connect(pPrev);
			return; // Silently fail, add pipeline will continue with the pipeline creation without the effect
		}

		if (pluginInfo.bIsInPlace && pluginInfo.bCanChangeRate)	// rate changing effects need to be out of place.
		{
			MONITOR_PLUGIN_ERROR(AK::Monitor::ErrorCode_PluginExecutionInvalid, pCtx, fxDesc.pFx->GetFXID());
			pPlugin->Term( AkFXMemAlloc::GetLower() );
			pNext->Connect( pPrev );
			return; // Silently fail, add pipeline will continue with the pipeline creation without the effect
		}
			
		if (pluginInfo.bIsInPlace)
			pFilter = AkNew(AkMemID_Processing, CAkVPLFilterNode());
		else
			pFilter = AkNew( AkMemID_Processing, CAkVPLFilterNodeOutOfPlace() );

		if( pFilter == NULL )
		{
			pNext->Connect( pPrev );
			return;
		}

		l_eResult = pFilter->Init( pPlugin, fxDesc, in_uFXIndex, this, informat );

		// Note: Effect can't be played for some reason but don't kill the sound itself yet...
		if ( l_eResult != AK_Success )
		{
			AKASSERT( pFilter );
			pFilter->Term();
			AkDelete( AkMemID_Processing, pFilter );
			pNext->Connect( pPrev );
			return;
		}

		m_cbxRec.m_pFilter[ in_uFXIndex ] = pFilter;

		pFilter->SetBypassed( fxDesc.iBypassed );
		pFilter->Connect( pPrev );

		uNewOutConfig = pFilter->GetOutputConfig();
	}
	else
	{
		uNewOutConfig = informat.channelConfig;
	}

	if( uNewOutConfig == uOutConfig )
	{
		pNext->Connect( pFilter ? pFilter : pPrev );
	}
	else if( bNextIsFilter )
	{
		// Need to reinitialize next effect if channel mask has changed
		UpdateFx( in_uFXIndex + 1 ); 
	}
	else
	{
		m_cbxRec.m_BQF.Term();
		m_cbxRec.m_BQF.Init( uNewOutConfig );
		// m_ObstructionBQF.Term();
		// m_ObstructionBQF.Init( uNewOutConfig );

		m_channelConfig = uNewOutConfig;

		pNext->Connect( pFilter ? pFilter : pPrev );
	}
}

void CAkVPLSrcCbxNode::UpdateSource(const AkUInt8* in_pOldDataPtr)
{
	bool bSourceRestarted = false;
	for ( AkUInt32 uSrc = 0; uSrc < MAX_NUM_SOURCES; ++uSrc )
	{
		if ( m_pSources[uSrc] && m_pSources[uSrc]->IsUsingThisSlot( in_pOldDataPtr ) )
		{
			m_pSources[uSrc]->RestartSourcePlugin();
			bSourceRestarted = true;
		}
	}

	if ( bSourceRestarted )
	{
		ClearVPL();
		m_pContext->InvalidatePrevPosParams();
		AddPipeline();
	}
}

void CAkVPLSrcCbxNode::SetFxBypass(
		AkUInt32 in_bitsFXBypass,
		AkUInt32 in_uTargetMask /* = 0xFFFFFFFF */ )
{
	for ( AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex )
	{
		AkUInt32 uFXMask = 1 << uFXIndex;
		CAkVPLFilterNodeBase * pFilter = m_cbxRec.m_pFilter[ uFXIndex ];
		if ( pFilter && ( in_uTargetMask & uFXMask ) )
		{
			AkInt16 iBypassed = ( in_bitsFXBypass & uFXMask) ? 1 : 0;
			pFilter->SetBypassed( iBypassed );
		}
	}
}

void CAkVPLSrcCbxNode::SetFxBypass( AkUInt32 in_fx, AkInt16 in_bypass )
{
	AKASSERT( in_fx < AK_NUM_EFFECTS_PER_OBJ );
	CAkVPLFilterNodeBase * pFilter = m_cbxRec.m_pFilter[ in_fx ];
	if(pFilter)
		pFilter->SetBypassed( in_bypass );
}

void CAkVPLSrcCbxNode::UpdateBypass(AkUInt32 in_fx, AkInt16 in_bypassOffset)
{
	AKASSERT( in_fx < AK_NUM_EFFECTS_PER_OBJ );
	CAkVPLFilterNodeBase * pFilter = m_cbxRec.m_pFilter[ in_fx ];
	if(pFilter)
	{
		pFilter->SetBypassed( pFilter->GetBypassed() + in_bypassOffset );
	}
}

void CAkVPLSrcCbxNode::RefreshBypassFx()
{	
	for ( AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex )
	{
		RefreshBypassFx( uFXIndex );
	}
}

void CAkVPLSrcCbxNode::RefreshBypassFx( AkUInt32 in_uFXIndex )
{
	if ( m_pSources[ 0 ] )
	{
		CAkPBI * pCtx = m_pSources[ 0 ]->GetContext();
		CAkSoundBase * pSound = pCtx->GetSound();		
		
		CAkVPLFilterNodeBase * pFilter = m_cbxRec.m_pFilter[ in_uFXIndex ];
		if ( pFilter )
		{
			AkFXDesc l_TempFxDesc;
			pSound->GetFX( in_uFXIndex, l_TempFxDesc, pCtx->GetGameObjectPtr() );
			pFilter->SetBypassed( l_TempFxDesc.iBypassed );
		}
	}
}


//-----------------------------------------------------------------------------
// Name: AddPipeline
// Desc: Create the pipeline for the source.
//-----------------------------------------------------------------------------
AKRESULT CAkVPLSrcCbxNode::AddPipeline()
{
	AKASSERT( m_pSources[ 0 ] != NULL );
	/*AKASSERT( in_pSrcRec->m_pSrc->IsIOReady() ); */ // could be "not IO ready" due to handling of voices initially virtual

	CAkVPLNode *	  l_pNode[MAX_NODES];
	AkUInt8			  l_cCnt	= 0;
	AKRESULT		  l_eResult = AK_Success;

	CAkPBI * l_pCtx = m_pSources[ 0 ]->GetContext();
	AKASSERT( l_pCtx != NULL );

	// Allocate space for pending markers
	m_PendingMarkers.Init();

	//---------------------------------------------------------------------
	// Create the source node.
	//---------------------------------------------------------------------	 
	l_pNode[l_cCnt++] = m_pSources[ 0 ];
	AkAudioFormat l_Format = l_pCtx->GetMediaFormat();
	l_eResult = m_pSources[0]->Init();
	if (l_eResult != AK_Success)
	{
		return l_eResult;
	}

	//---------------------------------------------------------------------
	// Create the resampler/pitch node (optional).
	//---------------------------------------------------------------------

	AkUInt32 uCoreSampleRate = AK_CORE_SAMPLERATE;
	if (!m_pSources[0]->SupportResampling())
	{
		m_pPitch = AkNew(AkMemID_Processing, CAkVPLPitchNode(this));
		if (!m_pPitch)
		{
			return AK_InsufficientMemory;
		}
		if (AK_EXPECT_FALSE(m_pPitch->Init(&l_Format, l_pCtx, uCoreSampleRate) != AK_Success))
			return AK_Fail;

		l_pNode[l_cCnt++] = m_pPitch;
	}

	m_bPipelineAdded = true;

	// now that the sample conversion stage is passed, the sample rate must be the native sample rate for others components.
	l_Format.uSampleRate = uCoreSampleRate;

	//---------------------------------------------------------------------
	// Create the insert effect.
	//---------------------------------------------------------------------

	for ( AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex )
	{
		AkFXDesc fxDesc;
		l_pCtx->GetFX( uFXIndex, fxDesc );

		if( fxDesc.pFx )
		{
			IAkPlugin * pPlugin = NULL;
			AkPluginInfo pluginInfo;
			l_eResult = CAkEffectsMgr::Alloc(fxDesc.pFx->GetFXID(), pPlugin, pluginInfo);
			if ( l_eResult != AK_Success ) // Note: Effect can't be created for some reason but don't kill the sound itself yet...
			{
				MONITOR_PLUGIN_ERROR( AK::Monitor::ErrorCode_PluginAllocationFailed, l_pCtx, fxDesc.pFx->GetFXID() );
				continue;
			}

			AK::Monitor::ErrorCode errCode = CAkEffectsMgr::ValidatePluginInfo(fxDesc.pFx->GetFXID(), AkPluginTypeEffect, pluginInfo);
			if (errCode != AK::Monitor::ErrorCode_NoError)
			{
				MONITOR_PLUGIN_ERROR(errCode, l_pCtx, fxDesc.pFx->GetFXID());
				pPlugin->Term(AkFXMemAlloc::GetLower());
				continue;
			}

			if (pluginInfo.bIsInPlace && pluginInfo.bCanChangeRate)	// rate changing effects need to be out of place.
			{
				MONITOR_PLUGIN_ERROR(AK::Monitor::ErrorCode_PluginExecutionInvalid, l_pCtx, fxDesc.pFx->GetFXID());
				pPlugin->Term( AkFXMemAlloc::GetLower() );
				continue;
			}
			
			CAkVPLFilterNodeBase * pFilter;
			if ( pluginInfo.bIsInPlace )
				pFilter = AkNew( AkMemID_Processing, CAkVPLFilterNode() );
			else
				pFilter = AkNew( AkMemID_Processing, CAkVPLFilterNodeOutOfPlace() );

			if( pFilter == NULL )
			{ 
				MONITOR_PLUGIN_ERROR(AK::Monitor::ErrorCode_PluginAllocationFailed, l_pCtx, fxDesc.pFx->GetFXID());
				l_eResult = AK_Fail; 
				pPlugin->Term( AkFXMemAlloc::GetLower() );
				goto AddPipelineError; 
			}

#ifdef AK_ENABLE_ASSERTS
			AkChannelConfig origConfig = l_Format.channelConfig;
#endif

			l_eResult = pFilter->Init( pPlugin, fxDesc, uFXIndex, this, l_Format );
			AKASSERT( !pluginInfo.bIsInPlace || origConfig == l_Format.channelConfig ); // Only out-of-place plug-ins can change channel mask.

			// Note: Effect can't be played for some reason but don't kill the sound itself yet...
			if ( l_eResult != AK_Success )
			{
				AKASSERT(pFilter);
				pFilter->Term();
				AkDelete( AkMemID_Processing, pFilter );
				continue;
			}

			m_cbxRec.m_pFilter[ uFXIndex ] = pFilter;
			l_pNode[l_cCnt++] = pFilter;
		}
	}

	m_channelConfig = l_Format.channelConfig; // remember this -- effects can change the channel mask.

	//---------------------------------------------------------------------
	// Create the bqf node.
	//---------------------------------------------------------------------
	l_eResult = m_cbxRec.m_BQF.Init( m_channelConfig );
	if( l_eResult != AK_Success ) 
		goto AddPipelineError;

	l_pNode[l_cCnt++] = &m_cbxRec.m_BQF;

	//---------------------------------------------------------------------
	// Create the volume automation node.
	//---------------------------------------------------------------------
	l_eResult = m_cbxRec.m_VolAutm.Init( l_pCtx );
	if( l_eResult != AK_Success ) 
		goto AddPipelineError;

	l_pNode[l_cCnt++] = &m_cbxRec.m_VolAutm;


	//---------------------------------------------------------------------
	// Connect the nodes.
	//---------------------------------------------------------------------
	while( --l_cCnt )
	{
		l_pNode[l_cCnt]->Connect( l_pNode[l_cCnt-1]) ;
	}

	if (m_pPitch) m_pPitch->PipelineAdded(&l_Format);
	// Make sure it is updated at least once before starting the pipeline.(WG-20417)
	RefreshBypassFx();

AddPipelineError:

	return l_eResult;
} // AddPipeline

// Compute volumes of all emitter-listener pairs for this sound. 
void CAkVPLSrcCbxNode::ComputeVolumeRays()
{
	CAkPBI * AK_RESTRICT pContext = m_pSources[ 0 ]->GetContext(); // remember the FIRST context to play in this frame
	AKASSERT( pContext != NULL );

	pContext->CalcEffectiveParams();

	if( !pContext->IsForcedVirtualized() )
	{
		CAkVPL3dMixable::_ComputeVolumeRays();
		CAkVPL3dMixable::ManageAuxSends();
	}
	else
	{
		// If virtualized for interruption, set virtual behaviour to Virtual Voice.
		if (pContext->ShouldChangeVirtualBehaviourForInterruption() || m_eBelowThresholdBehavior == AkBelowThresholdBehavior_ContinueToPlay)
		{
			m_eBelowThresholdBehavior = AkBelowThresholdBehavior_SetAsVirtualVoice;
			m_eVirtualBehavior = AkVirtualQueueBehavior_FromElapsedTime;
		}
		
		// Virtualized; no need to compute voice volume.
		// Important: m_fBehavioralVolume must be recomputed or reset each frame.
		m_fBehavioralVolume = 0.f;
		pContext->FlushRays();
		pContext->VirtualPositionUpdate();

		// Make sure send connections get garbage collected.
		m_arSendValues.RemoveAll();
	}
}

//-----------------------------------------------------------------------------
// Name: AddSrc
// Desc: Add a source.
//
// Parameters:
//	CAkPBI * in_pCtx    : Context to add.
//	bool			  in_bActive : true=source should be played. 
//
// Return:
//	Ak_Success :		    Source was added and pipeline created.
//  AK_InsufficientMemory : Error memory not available.
//  AK_NoDataReady :        IO not completed.
//-----------------------------------------------------------------------------
AKRESULT CAkVPLSrcCbxNode::AddSrc( CAkPBI * in_pCtx, bool in_bActive )
{
	AkSrcTypeInfo* pInfo = in_pCtx->GetSrcTypeInfo();
	
	AkUInt8 * pvBuffer;
	AkUInt32 ulBufferSize;
	in_pCtx->GetDataPtr(pvBuffer, ulBufferSize);
	if(pvBuffer && (ulBufferSize > 0))
	{
		AkFileParser::FormatInfo fmtInfo;
		AkUInt32 uLoopStart, uLoopEnd, uDataSize, uDataOffset;
		AKRESULT eResult = AkFileParser::Parse(pvBuffer, ulBufferSize, fmtInfo, NULL, &uLoopStart, &uLoopEnd, &uDataSize, &uDataOffset, NULL, NULL, true);
		if (AK_Success != eResult)
		{
			in_pCtx->Destroy(CtxDestroyReasonPlayFailed);
			return eResult;
		}

		if(fmtInfo.pFormat->wFormatTag == AK_WAVE_FORMAT_EXTENSIBLE) // Media is PCM, but bank may think it Vorbis if it was decoded on load: force correct media type.
		{
			pInfo->dwID = CAkEffectsMgr::GetMergedID(AkPluginTypeCodec, AKCOMPANYID_AUDIOKINETIC, AKCODECID_PCM);
		}

		if(uDataOffset + uDataSize <= ulBufferSize)
		{
			//The data is entirely in memory.
			pInfo->mediaInfo.Type = SrcTypeMemory;
		}
	}

	if (pInfo->mediaInfo.Type != SrcTypeMemory || in_pCtx->HasLoadedMedia())
	{
		CAkVPLSrcNode * pSrc = CAkVPLSrcNode::Create((AkSrcType)pInfo->mediaInfo.Type, pInfo->dwID, in_pCtx);
		if (pSrc)
			return AddSrc(pSrc, in_bActive, true);
	}
	else
	{
		MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_MediaNotLoaded, in_pCtx);
	}

	in_pCtx->Destroy( CtxDestroyReasonPlayFailed );
	return AK_Fail;
}

AKRESULT CAkVPLSrcCbxNode::AddSrc( CAkVPLSrcNode * in_pSrc, bool in_bActive, bool in_bFirstTime )
{
	AKRESULT l_eResult = AK_Success;

	CAkPBI * pCtx = in_pSrc->GetContext();
	pCtx->SetCbx( this );

	// Dont get this info if we are the sequel of a sample accurate container, they must all share the behavior of the first sound.
	if( in_bActive && in_bFirstTime )
	{
		m_eBelowThresholdBehavior = pCtx->GetVirtualBehavior( m_eVirtualBehavior );

		// Allocate a slot for one emitter-listener pair volume (common case). 
		// Multi-listener / position will require more.
		/// REVIEW Is this still beneficial?
		l_eResult = pCtx->EnsureOneRayReserved();
	}

	if ( l_eResult == AK_Success )
	{
		//Do check here to start stream only if required
		bool bIsUnderThreshold = false;
		if( m_eBelowThresholdBehavior != AkBelowThresholdBehavior_ContinueToPlay )
		{
			bIsUnderThreshold = pCtx->IsInitiallyUnderThreshold(NULL);
		}

		if( bIsUnderThreshold && m_eBelowThresholdBehavior == AkBelowThresholdBehavior_KillVoice )
		{
			pCtx->Monitor(AkMonitorData::NotificationReason_KilledVolumeUnderThreshold);
			l_eResult = AK_PartialSuccess;
		}
		// Must check in_bActive here, if not active, it is because we are in sample accurate mode, and in this mode, we have no choice but to keep streaming
		else if( bIsUnderThreshold && m_eVirtualBehavior == AkVirtualQueueBehavior_FromBeginning && in_bActive && in_bFirstTime )
		{
			l_eResult = AK_Success;//We fake success so the pipeline gets connected.

			// IMPORTANT. m_bAudible is used to know when we need to become virtual (call VirtualOn()). 
			// In this case - PlayFromBeginning mode, starting virtual - we skip the virtual handling mechanism:
			// do not start stream. 
			SetAudible( pCtx, false );
		}
		else
		{
			AkUInt8 * pvBuffer;
			AkUInt32 ulBufferSize;
			pCtx->GetDataPtr( pvBuffer, ulBufferSize );
			l_eResult = in_pSrc->FetchStreamedData( pvBuffer, ulBufferSize );
		}
	}

	// The source was created/initialized successfully, but I/O could still be pending.
	if( l_eResult == AK_Success || l_eResult == AK_FormatNotReady )
	{
		if ( in_bActive )
		{
			AKASSERT( !m_pSources[ 0 ] );
			m_pSources[ 0 ] = in_pSrc;
			m_pContext = m_pSources[0]->GetContext();
			pCtx->WakeUpVoice();
		}
		else
		{
			AKASSERT( !m_pSources[ 1 ] );
			m_pSources[ 1 ] = in_pSrc;
		}

		return l_eResult;
	}

	// Failure case: clear source.

	in_pSrc->Term( CtxDestroyReasonPlayFailed );
	AkDelete( AkMemID_Processing, in_pSrc );

	return l_eResult;
} // AddSrc

void CAkVPLSrcCbxNode::HandleSourceStarvation()
{
	if ( !m_pSources[ 0 ]->IsPreBuffering() )
	{
		m_bHasStarved = true;
		
		// Notify the playing manager.
		CAkPBI * pCtx = GetPBI();
		g_pPlayingMgr->NotifyStarvation( pCtx->GetPlayingID(), pCtx->GetSoundID() );

#ifndef AK_OPTIMIZED
		if ( !m_iWasStarvationSignaled ) 
		{
			m_pSources[0]->NotifySourceStarvation();
			m_iWasStarvationSignaled = MIMINUM_SOURCE_STARVATION_DELAY;
		}
#endif
	}
}

AkReal32 CAkVPLSrcCbxNode::BehavioralVolume() const 
{ 	
	AkReal32 fModulatorPeakVol = GetContext()->GetModulatorData().GetPeakOutput( AkRTPCBitArray( 1ULL << RTPC_Volume ) );	
	return m_fBehavioralVolume * fModulatorPeakVol; 
}

void CAkVPLSrcCbxNode::PrepareNextBuffer()
{
	if (m_pSources[0]->SrcProcessOrder() != SrcProcessOrder_SwVoice && m_eState == NodeStatePlay && PipelineAdded())
	{
		m_pSources[0]->PrepareNextBuffer();
		if (m_pSources[1] != nullptr && m_pSources[0]->CanPrepareNextSource() && m_pSources[1]->SrcProcessOrder() != SrcProcessOrder_SwVoice)
		{
			m_pSources[1]->PrepareNextBuffer();
		}
	}
}

void CAkVPLSrcCbxNode::NotifyMarkers(AkPipelineBuffer * in_pBuffer)
{
	AkUInt32 uNumMarkers = in_pBuffer->uPendingMarkerLength;
	if (uNumMarkers > 0)
	{
		AkMarkerNotificationRange range = m_PendingMarkers.Front();
		// We should always be flushing from the beginning of the queue, no skips
		AKASSERT(in_pBuffer->uPendingMarkerIndex == 0);
		AKASSERT(in_pBuffer->uPendingMarkerIndex + uNumMarkers <= range.uLength);
		g_pPlayingMgr->NotifyMarkers(range.pMarkers + in_pBuffer->uPendingMarkerIndex, uNumMarkers);
		m_cbxRec.Head()->PopMarkers(uNumMarkers);
		m_PendingMarkers.Dequeue(uNumMarkers);
		in_pBuffer->ResetMarkers();
	}
}

void CAkVPLSrcCbxNode::ClearVPL()
{
	if (m_pPitch)
	{
		m_pPitch->Term();
		AkDelete(AkMemID_Processing, m_pPitch);
		m_pPitch = NULL;
	}
	m_cbxRec.ClearVPL();
	m_bPipelineAdded = false;
}

void CAkVPLSrcCbxNode::RestorePreviousVolumes( AkPipelineBuffer* AK_RESTRICT io_pBuffer )
{
	GetContext()->InvalidatePrevPosParams();
}

void CAkVPLSrcCbxNode::StopLooping( CAkPBI * in_pCtx )
{
	if ( m_pSources[ 0 ] && in_pCtx == m_pSources[ 0 ]->GetContext() )
	{
		if ( !m_pSources[ 0 ]->IsIOReady() ||
			m_pSources[ 0 ]->StopLooping() != AK_Success )
		{
			// When StopLooping return an error, it means that the source must be stopped.
			// ( occurs with audio input plug-in only actually )
			Stop();
		}
	}
	else if ( m_pSources[ 1 ] && in_pCtx == m_pSources[ 1 ]->GetContext() ) 
	{
		m_pSources[ 1 ]->Term( CtxDestroyReasonFinished );
		AkDelete( AkMemID_Processing, m_pSources[ 1 ] );
		m_pSources[ 1 ] = NULL;
	}
}

void CAkVPLSrcCbxNode::SeekSource()
{
	// Bail out if FromBeginning: the behavior is undefined. Some sources seek or prepare their stream
	// when becoming virtual, others when becoming physical.
	// The source offset on the PBI needs to be cleared.
	if ( AkVirtualQueueBehavior_FromBeginning == m_eVirtualBehavior
		&& AkBelowThresholdBehavior_SetAsVirtualVoice == m_eBelowThresholdBehavior )
	{
		CAkPBI * pCtx = GetPBI();
		if ( pCtx )
			pCtx->SetSourceOffsetRemainder( 0 );
		return;
	}

	// If the source is not ready, seeking will be handled in StartStream().
	if ( m_pSources[ 0 ]
		&& m_pSources[ 0 ]->IsIOReady() )
	{
		ReleaseBuffer();

		if( m_cbxRec.Head()->Seek() != AK_Success )
		{
			// There was an error. Stop.
			Stop();
			return;
		}
	}
}

AKRESULT CAkVPLSrcCbxNode::SourceTimeSkip( AkUInt32 in_uMaxFrames )
{
	AKRESULT eReturn = m_cbxRec.Head()->TimeSkip( in_uMaxFrames ); // TimeLapse modified param according to the number of frames actually elapsed.

	if( eReturn == AK_Fail )
	{		
		if ( m_pSources[ 0 ] )
		{
			MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_CannotPlaySource_TimeSkip, m_pSources[ 0 ]->GetContext() );
		}
	}

	return eReturn;
}

bool CAkVPLSrcCbxNode::IsUsingThisSlot( const CAkUsageSlot* in_pUsageSlot )
{
	for ( AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex )
	{
		if( m_cbxRec.m_pFilter[ uFXIndex ] && m_cbxRec.m_pFilter[ uFXIndex ]->IsUsingThisSlot( in_pUsageSlot ) )
			return true;
	}

	for (AkUInt32 uSrc = 0; uSrc < MAX_NUM_SOURCES; ++uSrc)
	{
		if (m_pSources[uSrc] && m_pSources[uSrc]->IsUsingThisSlot(in_pUsageSlot))
			return true;
	}

	return false;
}

bool CAkVPLSrcCbxNode::IsUsingThisSlot( const AkUInt8* in_pData )
{
	for ( AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex )
	{
		if( m_cbxRec.m_pFilter[ uFXIndex ] && m_cbxRec.m_pFilter[ uFXIndex ]->IsUsingThisSlot( in_pData ) )
			return true;
	}

	for (AkUInt32 uSrc = 0; uSrc < MAX_NUM_SOURCES; ++uSrc)
	{
		if (m_pSources[uSrc] && m_pSources[uSrc]->IsUsingThisSlot(in_pData))
			return true;
	}

	return false;
}

void CAkVPLSrcCbxNode::RemovePipeline( AkCtxDestroyReason in_eReason )
{
	if ( m_pSources[ 0 ] )
	{
		m_pSources[ 0 ]->Term( in_eReason );
		AkDelete( AkMemID_Processing, m_pSources[ 0 ] );
		m_pSources[ 0 ] = NULL;
	}

	ClearVPL();
	m_PendingMarkers.Term();
	m_bAudible = true;
}

void CAkVPLSrcCbxNode::RelocateMedia( AkUInt8* in_pNewMedia,  AkUInt8* in_pOldMedia )
{
	if (m_pPitch)
	{
		m_pPitch->RelocateMedia(in_pNewMedia, in_pOldMedia);
	}
}

void CAkVPLSrcCbxNode::UpdateMakeUpLinearNormalized( CAkPBI* pSrcContext, CAkVPLSrcNode* pSrcNode ) {
	AkReal32 fMakeUpGaindB = pSrcContext->GetMakeupGaindB();
	if ( fMakeUpGaindB != m_previousMakeUpGainDB )
	{
		AkReal32 fNormalization = pSrcNode->GetNormalization();
		m_previousMakeUpGainLinearNormalized = AkMath::dBToLin( fMakeUpGaindB );
		m_previousMakeUpGainLinearNormalized *= fNormalization;
		m_previousMakeUpGainDB = fMakeUpGaindB;
	}
}

#ifndef AK_OPTIMIZED
void CAkVPLSrcCbxNode::RecapPluginParamDelta()
{
	AkDeltaMonitor::OpenSoundBrace(GetContext()->GetSoundID(), GetContext()->GetPipelineID());

	for (AkUInt32 i = 0; i < MAX_NUM_SOURCES; ++i)
	{
		if (m_pSources[i])
		{
			m_pSources[i]->RecapPluginParamDelta();
		}
	}

	for (AkUInt32 i = 0; i < AK_NUM_EFFECTS_PER_OBJ; ++i)
	{
		if (m_cbxRec.m_pFilter[i])
		{
			m_cbxRec.m_pFilter[i]->RecapParamDelta();
		}
	}

	AkDeltaMonitor::CloseSoundBrace();
}
#endif
