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

#include "AkVPLFilterNodeOutOfPlace.h"
#include "AkVPLSrcCbxNode.h"
#include "AkFXMemAlloc.h"
#include "AkAudioLibTimer.h"
#include "AkSpeakerPan.h"
#include "AkMixer.h"
#include "AkMarkers.h"

extern AkInitSettings		g_settings;
//Number of frames averaged for rate estimation
#define FRAMES_AVERAGE 8	

void CAkVPLFilterNodeOutOfPlace::GetBuffer( AkVPLState & io_state )
{
	m_usRequestedFrames = io_state.MaxFrames();

	// Upstream node has reported end of data, output effect tail
	if( m_bLast )
	{
		io_state.result = AK_NoMoreData;
		ConsumeBuffer( io_state );
		return;
	}

	// Start going downstream if we still have input remaining
	if( m_BufferIn.uValidFrames != 0 )
	{
		ConsumeBuffer( io_state );
	}
}

void CAkVPLFilterNodeOutOfPlace::ConsumeBuffer( AkVPLState & io_state )
{
	if ( io_state.result == AK_NoMoreData )
		m_bLast = true;
        
	if ( m_BufferIn.uValidFrames == 0 )
	{
		m_uInOffset = 0;
		InitInputBuffer(io_state);
	}
	//m_BufferIn.eState = !m_bLast ? io_state.result : AK_NoMoreData;
	m_BufferIn.eState = io_state.result;
	
	if ( !m_BufferOut.HasData() )
	{
		AkUInt8* pData = (AkUInt8*)AkMalign(AkMemID_Processing, m_usRequestedFrames * sizeof(AkReal32)*m_BufferOut.NumChannels(), AK_BUFFER_ALIGNMENT);
		if ( pData )
		{
			((AkPipelineBufferBase*)&m_BufferOut)->AttachInterleavedData( pData, m_usRequestedFrames, 0, m_BufferOut.GetChannelConfig() );
		}
		else
		{
			io_state.result = AK_Fail;
			return;
		}
	}

	m_InputFramesBeforeExec = m_BufferIn.uValidFrames;

	// Bypass FX if necessary
	if( m_iBypassed || m_pCbx->GetPBI()->GetBypassAllFX() )
	{
		if ( !m_bLastBypassed )	// Reset FX if not bypassed last buffer
			m_pEffect->Reset( );

		m_bLastBypassed = true;

		AkUInt32 uNumFrames = AkMin( m_BufferIn.uValidFrames, m_BufferOut.MaxFrames() );

		if ( uNumFrames > 0 )
		{
			// Mixing services require AK_SIMD_ALIGNMENT alignment. m_BufferIn.MaxFrames() complies to it.
			AkUInt32 uNumFramesAligned = ( uNumFrames + AK_SIMD_ALIGNMENT - 1 ) / AK_SIMD_ALIGNMENT * AK_SIMD_ALIGNMENT;
			AKASSERT( uNumFramesAligned <= m_BufferIn.MaxFrames() );
			AkUInt32 uNumOutputBytesClear = uNumFramesAligned  * sizeof(AkReal32);
			for ( AkUInt32 uChan = 0; uChan < m_BufferOut.GetChannelConfig().uNumChannels; uChan++ )
			{
				AkReal32 * pfOut = m_BufferOut.GetChannel( uChan );
				memset( pfOut, 0, uNumOutputBytesClear );
			}
			
			if ( uNumFramesAligned != uNumFrames )
			{
				AKASSERT( uNumFramesAligned > uNumFrames );
				AkUInt32 uNumBytesPadding = ( uNumFramesAligned - uNumFrames ) * sizeof(AkReal32);
				// Unaligned number of frames. Pad the input too.
				for ( AkUInt32 uChan = 0; uChan < m_BufferIn.GetChannelConfig().uNumChannels; uChan++ )
				{
					AkReal32 * pfInPad = m_BufferIn.GetChannel( uChan ) + uNumFrames;
					memset( pfInPad, 0, uNumBytesPadding );
				}
			}
			
			AkUInt32 uAllocSize = AK::SpeakerVolumes::Matrix::GetRequiredSize( m_BufferIn.GetChannelConfig().uNumChannels, m_BufferOut.GetChannelConfig().uNumChannels );
			AkReal32 * volumes = (AkReal32 *)AkAlloca( uAllocSize );
			CAkSpeakerPan::GetSpeakerVolumes2DPan( 0, 0, 1.f, AK_DirectSpeakerAssignment, m_BufferIn.GetChannelConfig(), m_BufferOut.GetChannelConfig(), volumes, NULL );

			AkRamp attn = AkRamp( 1.f, 1.f );
			AkMixer::MixNinNChannels( &m_BufferIn, &m_BufferOut, attn, volumes, volumes, 1.f / (AkReal32)uNumFrames, (AkUInt16)uNumFramesAligned );
		}
		
		m_BufferIn.uValidFrames = 0;
		if ( m_bLast )
			m_BufferOut.eState = AK_NoMoreData;
		else if ( m_BufferOut.uValidFrames == m_BufferOut.MaxFrames() )
			m_BufferOut.eState = AK_DataReady;
		else
			m_BufferOut.eState = AK_DataNeeded;
	}
	else
	{
		m_bLastBypassed = false;

		AkAudiolibTimer::Item * pTimerItem = AK_START_TIMER(0, m_pluginParams.fxID, m_pCbx->GetContext()->GetPipelineID());
 		m_pEffect->Execute( &m_BufferIn, m_uInOffset, &m_BufferOut );
		AK_STOP_TIMER(pTimerItem);
	}

	ProcessDone(io_state);
#ifndef AK_OPTIMIZED
	if (g_settings.bDebugOutOfRangeCheckEnabled && !io_state.CheckValidSamples())
	{
		AK::Monitor::PostString("Wwise audio out of range: ConsumeFilter inside filter node out of place", AK::Monitor::ErrorLevel_Error, m_pCbx->GetPBI()->GetPlayingID(), m_pCbx->GetPBI()->GetGameObjectPtr()->ID(), m_pCbx->GetPBI()->GetSoundID());
	}
#endif
	
	AKSIMD_ASSERTFLUSHZEROMODE;
}

void CAkVPLFilterNodeOutOfPlace::ProcessDone( AkVPLState & io_state )
{
	AkUInt32 uConsumedInputFrames = m_InputFramesBeforeExec - m_BufferIn.uValidFrames;
	CAkMarkers::TransferRelevantMarkers(m_pCbx->GetPendingMarkers(), &m_BufferIn, &m_BufferOut, m_uInOffset, uConsumedInputFrames);

	if ((m_pCbx->GetPBI()->GetRegisteredNotif() & AK_EnableGetSourcePlayPosition))
	{	
		m_uConsumedSinceLastOutput += uConsumedInputFrames;

		if (m_BufferOut.eState == AK_DataReady || m_BufferOut.eState == AK_NoMoreData)
		{
			m_fAveragedInput = (m_fAveragedInput * (FRAMES_AVERAGE - 1) + m_uConsumedSinceLastOutput) / FRAMES_AVERAGE;
			m_fAveragedOutput = (m_fAveragedOutput * (FRAMES_AVERAGE - 1) + m_BufferOut.uValidFrames) / FRAMES_AVERAGE;

			m_uConsumedSinceLastOutput = 0;

			if ( m_BufferIn.posInfo.IsValid() )
			{
				io_state.posInfo.uSampleRate = m_BufferIn.posInfo.uSampleRate;
				io_state.posInfo.uStartPos = m_BufferIn.posInfo.uStartPos + m_uInOffset;
				io_state.posInfo.uFileEnd = m_BufferIn.posInfo.uFileEnd;
				AKASSERT(m_fAveragedOutput != 0.f);
				io_state.posInfo.fLastRate = m_fAveragedInput/m_fAveragedOutput;			
			}
		}
	}

	m_uInOffset += uConsumedInputFrames;
	m_uRequestedInputFrames += uConsumedInputFrames;
	m_uConsumedInputFrames = m_uRequestedInputFrames;
	
	// Input entirely consumed release it.
	if( m_BufferIn.uValidFrames == 0 )
	{
		m_pInput->ReleaseBuffer();

		m_BufferIn.Clear();
		
		// Note. Technically we should reassign our cleared input buffer to the
		// pipeline buffer (in this case the pipeline should only hold a reference 
		// of our input buffer), but just clearing the data does the trick: the
		// request is reset in RunVPL().
		//io_state = m_BufferIn;
		io_state.ClearData();
		io_state.uValidFrames = 0;
	}

	if ( m_BufferOut.eState == AK_DataReady || m_BufferOut.eState == AK_NoMoreData )
	{
		/////////////////////////
		// Patch for WG-17598 
		m_BufferOut.posInfo = io_state.posInfo; // Quick fix for propagating position information when processing out of place effects.
		/////////////////////////
		
		*((AkPipelineBuffer *) &io_state) = m_BufferOut;
		m_BufferOut.ResetMarkers(); // Markers are moved, not copied.
	}

	io_state.result = m_BufferOut.eState;
	AKASSERT( m_BufferOut.uValidFrames <= io_state.MaxFrames() );	// Produce <= than requested
}

//-----------------------------------------------------------------------------
// Name: ReleaseBuffer
// Desc: Releases a processed buffer.
//-----------------------------------------------------------------------------
void CAkVPLFilterNodeOutOfPlace::ReleaseBuffer()
{
	m_BufferOut.ResetMarkers();

	// Assume output buffer was entirely consumed by client.
	if( m_BufferOut.HasData() )
	{
		AkFalign(AkMemID_Processing, m_BufferOut.GetInterleavedData());
		((AkPipelineBufferBase*)&m_BufferOut)->DetachData();

		m_BufferOut.Clear();
	}
} // ReleaseBuffer

AKRESULT CAkVPLFilterNodeOutOfPlace::Seek()
{
	m_pEffect->Reset();

	// Clear the input buffer first, then propagate to the input.
	m_pInput->ReleaseBuffer();

	m_BufferIn.Clear();

	// Seek anywhere: reset m_bLast!
	m_bLast = false;

	return m_pInput->Seek();
}

void CAkVPLFilterNodeOutOfPlace::PopMarkers(AkUInt32 uNumMarkers)
{
	if (m_BufferIn.uPendingMarkerLength > 0)
	{
		AKASSERT(m_BufferIn.uPendingMarkerIndex >= uNumMarkers);
		m_BufferIn.uPendingMarkerIndex -= uNumMarkers;
	}
	if (m_BufferOut.uPendingMarkerLength > 0)
	{
		AKASSERT(m_BufferOut.uPendingMarkerIndex >= uNumMarkers);
		m_BufferOut.uPendingMarkerIndex -= uNumMarkers;
	}
	CAkVPLNode::PopMarkers(uNumMarkers);
}

void CAkVPLFilterNodeOutOfPlace::VirtualOn( AkVirtualQueueBehavior eBehavior )
{
	if ( eBehavior != AkVirtualQueueBehavior_Resume )
	{
		m_pEffect->Reset();
		ReleaseInputBuffer();
	}

	CAkVPLFilterNodeBase::VirtualOn( eBehavior );
}

AKRESULT CAkVPLFilterNodeOutOfPlace::Init( 
		IAkPlugin * in_pPlugin,
		const AkFXDesc & in_fxDesc,
		AkUInt32 in_uFXIndex,
		CAkVPLSrcCbxNode * in_pCbx,
		AkAudioFormat &	io_format )
{
	m_pEffect				= (IAkOutOfPlaceEffectPlugin *) in_pPlugin;
	m_BufferIn.Clear();
	m_BufferOut.Clear();

	m_uRequestedInputFrames = 0;
	m_uConsumedInputFrames = 0;
	m_fAveragedInput = 1.f;
	m_fAveragedOutput = 1.f;
	m_uConsumedSinceLastOutput = 0;

	AKRESULT eResult = CAkVPLFilterNodeBase::Init( 
		in_pPlugin,
		in_fxDesc,
		in_uFXIndex,
		in_pCbx,
		io_format );
	if ( eResult == AK_Success )
	{
		eResult = m_pEffect->Init(
			AkFXMemAlloc::GetLower(),
			m_pInsertFXContext,	
			m_pluginParams.pParam,
			io_format
			);
	
		if ( eResult == AK_Success )
		{
			AKASSERT( io_format.channelConfig.eConfigType != AK_ChannelConfigType_Standard 
					|| io_format.channelConfig.uNumChannels == AK::GetNumNonZeroBits( io_format.channelConfig.uChannelMask ) );
			m_BufferOut.SetChannelConfig( io_format.channelConfig ); // preserve output format selection for buffer allocation

			eResult = m_pEffect->Reset( );
		}

		if (eResult != AK_Success)
		{
			CAkPBI * pCtx = in_pCbx->GetPBI();

			switch ( eResult )
			{ 
			case AK_UnsupportedChannelConfig:
				MONITOR_PLUGIN_ERROR( AK::Monitor::ErrorCode_PluginUnsupportedChannelConfiguration, pCtx, m_pluginParams.fxID );	
				break;
			case AK_PluginMediaNotAvailable:
				MONITOR_PLUGIN_ERROR( AK::Monitor::ErrorCode_PluginMediaUnavailable, pCtx, m_pluginParams.fxID );	
				break;
			default:
				MONITOR_PLUGIN_ERROR( AK::Monitor::ErrorCode_PluginInitialisationFailed, pCtx, m_pluginParams.fxID );	
				break;
			}
		}
	}
	return eResult;

} // Init

void CAkVPLFilterNodeOutOfPlace::Term()
{
	ReleaseMemory();
	CAkVPLFilterNodeBase::Term();

	if( m_BufferOut.HasData() )
	{
		m_BufferOut.ResetMarkers();
	
		AkFalign(AkMemID_Processing, m_BufferOut.GetInterleavedData());
		((AkPipelineBufferBase*)&m_BufferOut)->DetachData();
	}

	// Reset any input markers that could have been left there.
	m_BufferIn.ResetMarkers();
} // Term

void CAkVPLFilterNodeOutOfPlace::ReleaseMemory()
{
	if( m_pEffect != NULL )
	{
		m_pEffect->Term( AkFXMemAlloc::GetLower() );
		m_pEffect = NULL;
	}
}

AKRESULT CAkVPLFilterNodeOutOfPlace::TimeSkip( AkUInt32 & io_uFrames )
{
	if ( m_bLast )
		return AK_NoMoreData;

	AkUInt32 uSrcRequest = io_uFrames;
	
	AKRESULT eResult = m_pEffect->TimeSkip( uSrcRequest );
	m_uRequestedInputFrames += uSrcRequest;
	uSrcRequest = m_uRequestedInputFrames - m_uConsumedInputFrames;

	//Request frames from the input node.  The request must be AK_NUM_VOICE_REFILL_FRAMES so we accumulate enough requests to make that.
	//This means that upon returning from virtual mode, the synchronization will may be lost by a few buffers if time was slowed down.
	while ( uSrcRequest >= AK_NUM_VOICE_REFILL_FRAMES && eResult == AK_DataReady )
	{
		AkUInt32 uFramesToProduce = AK_NUM_VOICE_REFILL_FRAMES;
		eResult = m_pInput->TimeSkip(uFramesToProduce);
		uSrcRequest -= uFramesToProduce;
		m_uConsumedInputFrames += uFramesToProduce;

		//Out of place effects can not produce data if the input doesn't have data.
		//Effects with tails effectively can't compute the tail if it hasn't normally computed previous audio samples.
		//So when virtual, if the input is exhausted, there can't be any output.
		m_bLast = (eResult == AK_NoMoreData);
	}
	
	return eResult;
}

void CAkVPLFilterNodeOutOfPlace::InitInputBuffer(AkVPLState &in_buffer)
{
	m_BufferIn = in_buffer;
	in_buffer.ResetMarkers(); // Markers are moved, not copied.
};

bool CAkVPLFilterNodeOutOfPlace::ReleaseInputBuffer()
{
	if ( m_pInput )
		m_pInput->ReleaseBuffer();

	m_BufferIn.Clear();

	return true;
}

AkChannelConfig CAkVPLFilterNodeOutOfPlace::GetOutputConfig()
{
	return m_BufferOut.GetChannelConfig();
}
