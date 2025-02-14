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

/////////////////////////////////////////////////////////////////////
//
// AkVPLPitchNode.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkLEngine.h"
#include "AkVPLPitchNode.h"
#include "AkVPLSrcCbxNode.h"
#include "AudiolibDefs.h"     // Pool IDs definition.
#include "AkMonitor.h"
#include "AkMarkers.h"
#include "math.h"

static inline void ZeroPrePadBuffer(AkAudioBuffer * io_pAudioBuffer, AkUInt32 in_ulNumFramestoZero)
{
	if (in_ulNumFramestoZero)
	{
		// Do all channels
		AkUInt32 uNumChannels = io_pAudioBuffer->NumChannels();
		for (unsigned int uChanIter = 0; uChanIter < uNumChannels; ++uChanIter)
		{
			memset(io_pAudioBuffer->GetChannel(uChanIter), 0, in_ulNumFramestoZero * sizeof(AkSampleType));
		}
	}
}

void CAkVPLPitchNode::GetBuffer( AkVPLState & io_state )
{
	AKASSERT( m_pInput != NULL );
	AKASSERT( m_pPBI != NULL );

	AKASSERT( io_state.MaxFrames() == AK_NUM_VOICE_REFILL_FRAMES );

	m_Pitch.SetRequestedFrames( io_state.MaxFrames() );
	m_bStartPosInfoUpdated = false;

	// Apply new pitch from context
	m_Pitch.SetPitch(m_pInput->GetPitch(), m_pPBI->GetPitchShiftType() == PitchShiftType_LinearInterpolate );

	// Consume what we have
	if( m_BufferIn.uValidFrames != 0 )
	{
		ConsumeBuffer( io_state );
		return;
	}

	if ( m_bLast )
	{
		// Upstream node (source) had already finished and the pitch node has no 
		// data in its input buffer. This can occur when the voice becomes physical
		// FromElapsedTime just after the source finished, but the pitch node still 
		// has "virtual buffered samples". Simply continue processing downstream
		// (with 0 valid input samples). WG-9004.
		io_state.result = AK_NoMoreData;
	}
}

void CAkVPLPitchNode::ConsumeBuffer( AkVPLState & io_state )
{
	if ( io_state.result == AK_NoMoreData )
		m_bLast = true;

	if ( m_BufferIn.uValidFrames == 0 )
	{
		//AKASSERT( io_state.uValidFrames != 0 );
		// WG-9121 Source plug-ins with RTPCable duration may end up (legitimally) in this condition
		if (m_BufferIn.HasData())
			ReleaseInputBuffer(io_state);

		if ( AK_EXPECT_FALSE( io_state.uValidFrames == 0 && io_state.result == AK_DataReady ) )
		{
			// Handle the case where sources return AK_DataReady with 0 frames.
			// This is legal: need to ask it data again.
			io_state.result = AK_DataNeeded;
			return;
		}

		m_BufferIn = io_state;
		io_state.ResetMarkers(); // Markers are moved, not copied.
	}

	if ( !m_BufferOut.HasData() )
	{
		// output same channel config as input
		m_BufferOut.SetChannelConfig( m_BufferIn.GetChannelConfig() );
		m_BufferOut.SetRequestSize( (AkUInt16)AkAudioLibSettings::g_uNumSamplesPerFrame );
		if ( m_BufferOut.GetCachedBuffer( ) == AK_Success )
		{
			AkPrefetchZero(m_BufferOut.GetInterleavedData(), m_BufferOut.NumChannels()*m_Pitch.GetRequestedFrames()*sizeof(AkReal32));

			// IM
			// Force to start the sound in a specific sample in the buffer.
			// Allows to start sound on non-buffer boundaries.
			if ( m_bPadFrameOffset )
			{
				AKASSERT( !m_Pitch.HasOffsets() );

				AkInt32 l_iFrameOffset = m_pPBI->GetFrameOffsetBeforeFrame();
				if( l_iFrameOffset > 0 )
				{
					AKASSERT( l_iFrameOffset < m_BufferOut.MaxFrames() );

					ZeroPrePadBuffer( &m_BufferOut, l_iFrameOffset );// Set the skipped samples to zero

					m_Pitch.SetOutputBufferOffset( l_iFrameOffset );// set the starting offset of the output buffer
				}
				m_bPadFrameOffset = false;
			}
		}
		else
		{
			io_state.result = AK_Fail;
			return;
		}
	}

	// If the source was not starting at the beginning of the file and the format was not decoded sample accurately, there
	// may be a remaining number of samples to skip so that the source starts accurately to the right sample.
	// Note: The pitch node does not care about the source "really" requires a source offset (seek). When this 
	// code executes, the PBI's source offset only represents the remainder of the original source seeking value.
	// The task of the pitch node is to correct the error if the source doesn't have sample-accurate seeking ability.
	AkInt32 l_iSourceFrameOffset = m_pPBI->GetSourceOffsetRemainder();
	if( l_iSourceFrameOffset )
	{
		if( m_BufferIn.uValidFrames > l_iSourceFrameOffset)
		{
			m_Pitch.SetInputBufferOffset( l_iSourceFrameOffset );
			m_BufferIn.uValidFrames -= (AkUInt16)l_iSourceFrameOffset;
			m_pPBI->SetSourceOffsetRemainder( 0 );
		}
		else
		{
			m_pPBI->SetSourceOffsetRemainder( l_iSourceFrameOffset - m_BufferIn.uValidFrames );
			m_BufferIn.uValidFrames = 0;
			io_state.uValidFrames = 0;
			ReleaseInputBuffer( io_state );
			io_state.result = m_bLast ? AK_NoMoreData : AK_DataNeeded;
			return;
		}
	}

	AKASSERT( m_BufferIn.HasData() || m_BufferIn.uValidFrames == 0 );

	AkUInt32 l_ulInputFrameOffset = m_Pitch.GetInputFrameOffset();
	AkUInt16 l_usConsumedInputFrames = m_BufferIn.uValidFrames;

	// Note. The number of frames already present in the output buffer must NEVER exceed the 
	// number of requested frames. This situation should have been caught in VPLPitchNode::GetBuffer().
	AKRESULT eResult = m_Pitch.Execute( &m_BufferIn, &m_BufferOut );

	l_usConsumedInputFrames -= m_BufferIn.uValidFrames;

	CAkMarkers::TransferRelevantMarkers( m_pCbx->GetPendingMarkers(), &m_BufferIn, &m_BufferOut, l_ulInputFrameOffset, l_usConsumedInputFrames );
	
	// Adjust position information
	if( ( m_BufferIn.posInfo.uStartPos != (AkUInt32) -1 ) && !m_bStartPosInfoUpdated )
	{
		m_BufferOut.posInfo = m_BufferIn.posInfo;
		m_BufferOut.posInfo.uStartPos = m_BufferIn.posInfo.uStartPos + l_ulInputFrameOffset;
		m_bStartPosInfoUpdated = true;
	}
	m_BufferOut.posInfo.fLastRate = m_Pitch.GetLastRate();

	// Input entirely consumed release it.
	if( m_BufferIn.uValidFrames == 0 )
	{
		ReleaseInputBuffer( io_state );
		
		if( m_bLast == true )
		{
			if ( m_pCbx->m_pSources[ 1 ] )
				eResult = m_pCbx->TrySwitchToNextSrc(m_BufferOut);
			else
				eResult = AK_NoMoreData;
		}
	}

	AKASSERT( m_BufferOut.MaxFrames() == m_Pitch.GetRequestedFrames() );

	if ( eResult == AK_DataReady || eResult == AK_NoMoreData )
	{
		*((AkPipelineBuffer *) &io_state) = m_BufferOut;
		m_BufferOut.ResetMarkers(); // Markers are moved, not copied.
	}

	io_state.result = eResult;
}

//-----------------------------------------------------------------------------
// Name: ReleaseBuffer
// Desc: Releases a processed buffer.
//-----------------------------------------------------------------------------
void CAkVPLPitchNode::ReleaseBuffer()
{
	AKASSERT( m_pInput != NULL );

	m_BufferOut.ResetMarkers();

	// Assume output buffer was entirely consumed by client.
	if( m_BufferOut.HasData() )
	{
		m_BufferOut.ReleaseCachedBuffer();
		
		m_BufferOut.Clear();

		m_Pitch.SetOutputBufferOffset( 0 );
	}
} // ReleaseBuffer

// io_buffer is the VPL state's buffer.
void CAkVPLPitchNode::ReleaseInputBuffer( AkPipelineBuffer & io_buffer )
{
	m_pInput->ReleaseBuffer();
	m_BufferIn.Clear();

	// Note. Technically we should reassign our cleared input buffer to the
	// pipeline buffer (in this case the pipeline should only hold a reference 
	// of our input buffer), but just clearing the data does the trick: the
	// request is reset in RunVPL().
	//io_buffer = m_BufferIn;
	io_buffer.ClearData();
	// WG-21537 Don't forget to clear references to markers
	io_buffer.ResetMarkers();
}

AKRESULT CAkVPLPitchNode::TimeSkip(AkUInt32 & io_uFrames)
{
	// Apply new pitch from context
	m_Pitch.SetPitchForTimeSkip(m_pInput->GetPitch());

	AkUInt32 uFramesToProduce = io_uFrames;
	m_Pitch.TimeOutputToInput(uFramesToProduce);

	AkUInt32 uProducedFrames = 0;
	AKRESULT eResult = AK_DataReady;

	while (uFramesToProduce)
	{
		// Need to get 'more data' from source ?

		if (!m_BufferIn.uValidFrames &&
			!m_bLast)
		{
			AkUInt32 uSrcRequest = io_uFrames;

			AKRESULT eThisResult = m_pInput->TimeSkip(uSrcRequest);

			if (eThisResult != AK_DataReady && eThisResult != AK_NoMoreData)
				return eThisResult;
			else if (eThisResult == AK_NoMoreData)
				m_bLast = true;

			// Consume source offset remainder stored in the PBI if any.
			AkUInt32 uSrcOffsetToConsume = m_pPBI->GetSourceOffsetRemainder();
			AkUInt32 uRemainingSrcOffsetToConsume = 0;
			if (uSrcOffsetToConsume > uSrcRequest)
			{
				// Cannot consume source offset completely from input's 'data'. Clamp and push the rest to PBI.
				uRemainingSrcOffsetToConsume = uSrcOffsetToConsume - uSrcRequest;
				uSrcOffsetToConsume = uSrcRequest;
			}
			uSrcRequest -= uSrcOffsetToConsume;
			m_pPBI->SetSourceOffsetRemainder(uRemainingSrcOffsetToConsume);

			m_BufferIn.uValidFrames = (AkUInt16)uSrcRequest;
		}

		AkUInt32 uFrames = AkMin(uFramesToProduce, m_BufferIn.uValidFrames);

		uProducedFrames += uFrames;
		uFramesToProduce -= uFrames;

		m_BufferIn.uValidFrames -= (AkUInt16)uFrames;

		if (!m_BufferIn.uValidFrames)
		{
			if (m_bLast)
			{
				eResult = AK_NoMoreData;
				break;
			}
		}
	}

	m_Pitch.TimeInputToOutput(uProducedFrames);

	io_uFrames = uProducedFrames;

	return eResult;
}

void CAkVPLPitchNode::VirtualOn( AkVirtualQueueBehavior eBehavior )
{
	if ( eBehavior != AkVirtualQueueBehavior_Resume )
	{
		// we do not support skipping some data in the input buffer and then coming back:
		// flush what's left.

		if ( m_BufferIn.HasData() )
			m_pInput->ReleaseBuffer();

		m_BufferIn.Clear();
		m_Pitch.ResetOffsets();
	}

	if( eBehavior == AkVirtualQueueBehavior_FromBeginning )
	{
		// WG-5935 
		// If the last buffer was drained and we restart from beginning, the flag must be resetted.
		m_bLast = false;
	}

    if ( !m_bLast )
	{
		m_pInput->VirtualOn( eBehavior );
	}
}

AKRESULT CAkVPLPitchNode::VirtualOff( AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset )
{
	if ( eBehavior == AkVirtualQueueBehavior_FromElapsedTime )
	{
		// We used this as a marker for elapsed frames, flush it now.
		m_BufferIn.uValidFrames = 0;
	}

    if ( !m_bLast )
		return m_pInput->VirtualOff( eBehavior, in_bUseSourceOffset );

	return AK_Success;
}

AKRESULT CAkVPLPitchNode::Seek()
{
	// Clear the input buffer first, then propagate to the input if not finished.
	m_pInput->ReleaseBuffer();
	m_BufferIn.Clear();

	// Reset resampler.
	m_Pitch.ResetOffsets();

	if (!m_bLast)
		return m_pInput->Seek();
	return AK_Success;
}

void CAkVPLPitchNode::PopMarkers(AkUInt32 uNumMarkers)
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


//-----------------------------------------------------------------------------
// Name: Init
// Desc: Initializes the node.
//
// Parameters:
//	AkSoundParams* in_pSoundParams		   : Pointer to the sound parameters.
//	AkAudioFormat* io_pFormat			   : Pointer to the format of the sound.
//-----------------------------------------------------------------------------
AKRESULT CAkVPLPitchNode::Init( AkAudioFormat * io_pFormat,			// Format.
								CAkPBI* in_pPBI, // PBI, to access the initial pbi that created the pipeline.
								AkUInt32 in_usSampleRate
								)
{
	m_pPBI = in_pPBI;
	m_bLast						= false;
	m_bPadFrameOffset			= true;

	return m_Pitch.Init( io_pFormat, in_usSampleRate );
} // Init

//-----------------------------------------------------------------------------
// Name: Term
// Desc: Term.
//
// Parameters:
//
// Return:
//	AK_Success : Terminated correctly.
//	AK_Fail    : Failed to terminate correctly.
//-----------------------------------------------------------------------------
void CAkVPLPitchNode::Term()
{
	if( m_BufferOut.HasData() )
	{
		// WG-14924: Markers may be left here when stopping a just-starved voice.
		m_BufferOut.ResetMarkers();
	
		m_BufferOut.ReleaseCachedBuffer();
	}

	// Throw away remaining frames in  bufferIn so channel config is updated.
	m_BufferIn.uValidFrames = 0;

	// Release any input markers that could have been left there.
	m_BufferIn.ResetMarkers();

	m_pPBI = NULL;

	m_Pitch.Term();
} // Term

void CAkVPLPitchNode::RelocateMedia( AkUInt8* in_pNewMedia,  AkUInt8* in_pOldMedia )
{
	if( m_BufferIn.HasData() )
	{
		m_BufferIn.RelocateMedia( in_pNewMedia, in_pOldMedia );
	}
}

void CAkVPLPitchNode::SwitchToNextSrc(CAkVPLSrcNode *pNext)
{
	m_pInput = pNext;
	m_pPBI = pNext->GetContext();

	m_Pitch.SwitchTo(m_pPBI->GetMediaFormat(), m_pInput->GetPitch(), &m_BufferOut, AK_CORE_SAMPLERATE);

	m_bLast = false;
}

void CAkVPLPitchNode::PipelineAdded(AkAudioFormat * in_pFormat)
{
	// Allow our input to optionally override the channel mapping
	AkChannelMappingFunc pfMap = m_pInput->GetChannelMappingFunc();
	if (pfMap)
	{
		m_Pitch.SetChannelMapping(in_pFormat->channelConfig, pfMap);
	}
}
