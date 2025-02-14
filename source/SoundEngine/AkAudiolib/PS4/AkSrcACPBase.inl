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

#include "stdafx.h"
#include "AkSrcBase.h"
#include "AkCommon.h"

#include "AkACPManager.h"
#include "AkLEngine.h"

#include <ajm/at9_decoder.h>

#include "AkSrcACPHelper.h"

template <class T_BASE>
CAkSrcACPBase<T_BASE>::CAkSrcACPBase( CAkPBI * in_pCtx )
: T_BASE( in_pCtx )
, m_DecoderSamplePosition(0)
, m_MaxOutputFramesPerJob(0)
, m_uDecoderLoopCntAhead(0)
, m_uNextSkipSamples(0)
, m_uSkipSamplesOnFileStart(0)
, m_uRemainingSamples(0)
, m_fCurPitchForDecodedData(0.f)
, m_nSamplesPerBlock(0)
, m_nBlockSize(0)
, m_DecodingStarted(false)
, m_Initialized(false)
, m_DecodingFailed(false)
, m_IsVirtual(false)
, m_bFirstDecodingFrameCompleted(false)
, m_bStartOfFrame(true)
, m_AjmInstanceId(0)
, m_AjmContextId(0)
{
	in_pCtx->CalcEffectiveParams();
	m_fCurPitchForDecodedData = GetCurrentEffectivePitch();
}

template <class T_BASE>
CAkSrcACPBase<T_BASE>::~CAkSrcACPBase()
{
}

template <class T_BASE>
AKRESULT CAkSrcACPBase<T_BASE>::CreateHardwareInstance( const SceAjmContextId in_AjmContextId )
{
	AKASSERT(m_AjmInstanceId == 0);
	AKASSERT(m_AjmContextId == 0);
	AKASSERT(m_Initialized == false);

	CAkACPManager::Get().Wait();
	m_AjmContextId = in_AjmContextId;

	uint64_t uiFlags = SCE_AJM_INSTANCE_FLAG_MAX_CHANNEL( T_BASE::m_pCtx->GetMediaFormat().channelConfig.uNumChannels ) | SCE_AJM_INSTANCE_FLAG_FORMAT(SCE_AJM_FORMAT_ENCODING_S16);

	int sceResult = sceAjmInstanceCreate(in_AjmContextId, SCE_AJM_CODEC_AT9_DEC, uiFlags, &m_AjmInstanceId);
	if (sceResult != SCE_OK)
	{
		m_AjmInstanceId = 0;
		return AK_Fail;
	}
	return AK_Success;
}

template <class T_BASE>
AKRESULT CAkSrcACPBase<T_BASE>::InitialiseDecoderJob( SceAjmBatchInfo* in_Batch )
{
	AKASSERT(!CAkACPManager::Get().IsBusy());

	// Set up the At9 config data
	int sceResult = sceAjmBatchJobInitialize(in_Batch, m_AjmInstanceId, &m_InitParam, sizeof(m_InitParam), &m_InitResult);

	m_Initialized = true;

	AKASSERT( m_AjmContextId > 0 );
	AKASSERT( m_AjmInstanceId > 0 );
	
	// Keep the handle for destruction
	return (sceResult == SCE_OK) ? AK_Success : AK_Fail;
}

template <class T_BASE>
void CAkSrcACPBase<T_BASE>::StopStream()
{
	Unregister();
	AKASSERT(!m_DecodingStarted);
	T_BASE::StopStream();
}

template <class T_BASE>
void CAkSrcACPBase<T_BASE>::ReleaseBuffer()
{
	// If data was being held by the pipeline when we needed to grow the decoding buffer, now is the right time to release it
	m_Ringbuffer.ReleasePreviousDataIfNeeded();

	m_Ringbuffer.m_ReadSampleIndex = ((m_Ringbuffer.m_ReadSampleIndex + m_Ringbuffer.m_SamplesBeingConsumed) % m_Ringbuffer.m_TotalSamples);
	AKASSERT( m_Ringbuffer.m_RingbufferUsedSample >= m_Ringbuffer.m_SamplesBeingConsumed );
	m_Ringbuffer.m_RingbufferUsedSample -= m_Ringbuffer.m_SamplesBeingConsumed;
	m_Ringbuffer.m_SamplesBeingConsumed = 0;
	
#ifdef AT9_STREAM_DEBUG_OUTPUT
	/*char msg[256];
	sprintf( msg, "-- -- ReleaseBuffer %i \n", m_Ringbuffer.m_ReadSampleIndex);
	AKPLATFORM::OutputDebugMsg( msg );*/
#endif
}

template <class T_BASE>
AKRESULT CAkSrcACPBase<T_BASE>::SeekToSourceOffset()
{
	AKRESULT result = AK_Success;
	T_BASE::m_uCurSample = /*139264;*/ T_BASE::GetSourceOffset();	
	T_BASE::m_pCtx->SetSourceOffsetRemainder( 0 );

	if ( T_BASE::m_uCurSample >= T_BASE::m_uTotalSamples )
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_SeekAfterEof, T_BASE::m_pCtx );
		result = AK_Fail;
	}

	PrepareRingbuffer();
	return result;
}

template <class T_BASE>
AKRESULT CAkSrcACPBase<T_BASE>::ChangeSourcePosition()
{
	AKASSERT( T_BASE::m_pCtx->RequiresSourceSeek() );
	CAkACPManager::Get().Wait();
	return SeekToSourceOffset();
}

template <class T_BASE>
bool CAkSrcACPBase<T_BASE>::SupportMediaRelocation() const
{
	return true;
}

template <class T_BASE>
AKRESULT CAkSrcACPBase<T_BASE>::TimeSkip( AkUInt32 & io_uFrames )
{
	m_fCurPitchForDecodedData = GetCurrentEffectivePitch();

	return CAkSrcBaseEx::TimeSkip( io_uFrames );
}

template <class T_BASE>
void CAkSrcACPBase<T_BASE>::GetBuffer( AkVPLState & io_state )
{
	CAkACPManager::Get().Wait();
	m_bStartOfFrame = true;

	if (m_DecodingFailed)
	{
		io_state.result = AK_Fail;
		return;
	}

	if(m_Ringbuffer.m_RingbufferUsedSample <= 0)
	{
		io_state.result = AK_NoDataReady;
		return;
	}

#ifdef _DEBUG
	AKASSERT( T_BASE::m_uCurSample == GetCurrentDecodedReadSample() );
#endif
	AkUInt32 uNumChannels = T_BASE::m_pCtx->GetMediaFormat().channelConfig.uNumChannels;
	AKASSERT(m_Ringbuffer.m_SamplesBeingConsumed == 0);

	AkInt32 uMaxFrames = (AkUInt16)AkMin( io_state.MaxFrames(), m_Ringbuffer.CalcUsedSpace() );

	// going around the ring buffer
	if ( m_Ringbuffer.m_ReadSampleIndex + uMaxFrames > m_Ringbuffer.m_TotalSamples)
	{
		uMaxFrames = (m_Ringbuffer.m_TotalSamples - m_Ringbuffer.m_ReadSampleIndex);
	}

	// Trucating at loop end
	AkUInt32 loopEnd = (T_BASE::DoLoop()) ? ( T_BASE::m_uPCMLoopEnd + 1 ) : T_BASE::m_uTotalSamples;
	if ( T_BASE::m_uCurSample + uMaxFrames > loopEnd )
	{
		uMaxFrames = loopEnd - T_BASE::m_uCurSample;
	}

	uMaxFrames = m_Ringbuffer.SkipFramesIfNeeded(uMaxFrames);
	if (uMaxFrames <= 0) // Just skipped some data, but now there's not enough valid samples left, starve.
	{
		io_state.result = AK_NoDataReady;
		return;
	}

	// going around the ring buffer, again
	if ( m_Ringbuffer.m_ReadSampleIndex + uMaxFrames > m_Ringbuffer.m_TotalSamples)
	{
		uMaxFrames = (m_Ringbuffer.m_TotalSamples - m_Ringbuffer.m_ReadSampleIndex);
	}

	m_Ringbuffer.m_SamplesBeingConsumed += uMaxFrames;

	AKASSERT( m_Ringbuffer.m_SamplesBeingConsumed > 0 );

#ifdef AT9_STREAM_DEBUG_OUTPUT
	char msg[256];
	sprintf( msg, "-- -- GetBuffer buffindex[%i, %i] sample[%i, %i]#%i - decoder %i\n", 
		m_Ringbuffer.m_ReadSampleIndex, m_Ringbuffer.m_ReadSampleIndex + uMaxFrames, 
		T_BASE::m_uCurSample, T_BASE::m_uCurSample + uMaxFrames, 
		uMaxFrames, m_DecoderSamplePosition);
	AKPLATFORM::OutputDebugMsg( msg );
#endif
	AKASSERT( m_Ringbuffer.m_RingbufferUsedSample >= m_Ringbuffer.m_SamplesBeingConsumed );

	AKASSERT( m_Ringbuffer.m_ReadSampleIndex+uMaxFrames <= m_Ringbuffer.m_TotalSamples );
	AKASSERT( T_BASE::m_uCurSample + uMaxFrames <= T_BASE::m_uTotalSamples );
	AKASSERT(io_state.result != AK_NoDataReady);

	if (m_Ringbuffer.m_ReadSampleIndex < m_Ringbuffer.m_WriteSampleIndex)
	{
		AKASSERT( m_Ringbuffer.m_ReadSampleIndex + uMaxFrames <= m_Ringbuffer.m_WriteSampleIndex );
	}
	else
	{
		AKASSERT( m_Ringbuffer.m_ReadSampleIndex + uMaxFrames <= m_Ringbuffer.m_TotalSamples );
	}

	T_BASE::SubmitBufferAndUpdate(
			m_Ringbuffer.GetReadBuffer( SampleToBufferIndex(m_Ringbuffer.m_ReadSampleIndex) ),
			uMaxFrames,
			T_BASE::m_pCtx->GetMediaFormat().uSampleRate,
			T_BASE::m_pCtx->GetMediaFormat().channelConfig,
			io_state );
	AKASSERT(io_state.result != AK_NoDataReady);
	T_BASE::LeavePreBufferingState();

	AKASSERT(io_state.result != AK_NoDataReady);
}

// -------------------------------------------------------------------------------------
// Hardware decoder functions
template <class T_BASE>
bool CAkSrcACPBase<T_BASE>::Register()
{
	AkUInt32 uNumChannels = T_BASE::m_pCtx->GetMediaFormat().channelConfig.uNumChannels;
	AkUInt32 numSampleInBuffer = CeilSampleToBlockSize((AkUInt32)GetFrameThroughput() + m_uSkipSamplesOnFileStart);
	
	if (!m_Ringbuffer.Create(numSampleInBuffer, uNumChannels) )
	{
		return false;
	}

	AkUInt32 maxOutputFramesPerJobFromApiMaxInput = (((SCE_AJM_DEC_AT9_MAX_INPUT_BUFFER_SIZE / sizeof( AkUInt16 )) * m_nSamplesPerBlock / m_nBlockSize) / m_nSamplesPerBlock) * m_nSamplesPerBlock ;
	AkUInt32 maxOutputFramesPerJobFromApiMaxOutput = (((SCE_AJM_DEC_AT9_MAX_OUTPUT_BUFFER_SIZE / sizeof( AkUInt16 )) / uNumChannels ) / m_nSamplesPerBlock) * m_nSamplesPerBlock;
	AkUInt32 maxOutputFramesPerJobFromApi = AkMin(maxOutputFramesPerJobFromApiMaxInput, maxOutputFramesPerJobFromApiMaxOutput);

	m_MaxOutputFramesPerJob = maxOutputFramesPerJobFromApi ;
	AKASSERT(m_MaxOutputFramesPerJob > 0);

	return true;
}

template <class T_BASE>
void CAkSrcACPBase<T_BASE>::Unregister()
{
	if (m_AjmInstanceId > 0)
	{
		CAkACPManager::Get().Wait();

		AKASSERT(m_AjmInstanceId > 0);
		AKVERIFY(sceAjmInstanceDestroy(m_AjmContextId, m_AjmInstanceId) == SCE_OK);
	}
	AKASSERT(!m_DecodingStarted);

	m_AjmInstanceId = 0;
	m_AjmContextId = 0;
	m_Initialized = false;

	m_Ringbuffer.Destroy();
}

template <class T_BASE>
void CAkSrcACPBase<T_BASE>::InitIfNeeded(SceAjmBatchInfo* in_Batch)
{
	// ----------------------------------------------------------------------------
	// On first decoding task, add an extra initialized job

	if( !m_Initialized )
	{
		AKRESULT res = InitialiseDecoderJob(in_Batch);
		if (res != AK_Success)
		{
			MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_ATRAC9DecodeFailed, T_BASE::m_pCtx);
			m_DecodingFailed = true;
		}
	}
}


template <class T_BASE>
AKRESULT CAkSrcACPBase<T_BASE>::CreateDecodingJob(SceAjmBatchInfo* in_Batch)
{	
	AKASSERT(!CAkACPManager::Get().IsBusy());
	AKASSERT( m_AjmContextId > 0 );
	AKASSERT( m_AjmInstanceId > 0 );

	// An error may have occur following the last decoding job
	if (m_DecodingFailed)
	{
		return AK_Fail;
	}

	AkUInt32 uNumChannels = T_BASE::m_pCtx->GetMediaFormat().channelConfig.uNumChannels;
	int numBuffersIn = 0;
	int numBuffersOut = 0;

	AKASSERT(m_bStartOfFrame || m_uRemainingSamples > 0);
	AkUInt32 numSamplesToDecode = GetNumSamplesToDecode(m_uRemainingSamples);

	// A grow buffer allocation can fail in GetNumSamplesToDecode, double check
	if (m_DecodingFailed)
	{
		return AK_Fail;
	}
	
	AKASSERT(m_Ringbuffer.m_RingbufferUsedSample+ numSamplesToDecode > 0 && m_Ringbuffer.m_RingbufferUsedSample + numSamplesToDecode <= m_Ringbuffer.m_TotalSamples);
	m_bStartOfFrame = false; //set to true in GetBuffer
	AkUInt32 originalSamplesToDecode = numSamplesToDecode + m_uRemainingSamples;
	
#ifdef AT9_STREAM_DEBUG_OUTPUT
	char msg[256];
	sprintf( msg, "-- -- CreateDecodingJob buffer is at : %i, we need %i \n", m_Ringbuffer.m_RingbufferUsedSample, numSamplesToDecode);
	AKPLATFORM::OutputDebugMsg( msg );
#endif

	AKASSERT(numSamplesToDecode > 0);
	AKASSERT(numSamplesToDecode%m_nSamplesPerBlock == 0);
	
	AkInt32 nextSamplePosition = 0;

	m_InputBufferDesc[0].szSize = 0;
	m_InputBufferDesc[1].szSize = 0;
	m_OutputBufferDesc[0].szSize = 0;
	m_OutputBufferDesc[1].szSize = 0;

	AkUInt32 remainingSizeToDecode = 0;
	nextSamplePosition = m_DecoderSamplePosition + numSamplesToDecode; // default case
	
	AKASSERT ( m_uNextSkipSamples == 0 );
	AKASSERT ( (m_DecoderSamplePosition + m_uSkipSamplesOnFileStart) % m_nSamplesPerBlock == 0  );
	
	// ------------------------------------------------------------------------------------------------------------------
	// If file is ending on this frame, 
	// remove extra tail required by at9 to be blockSize align
	

	if ( FileEndOnThisFrame( numSamplesToDecode ) )
	{

		AKASSERT((AkInt32)T_BASE::m_uTotalSamples > m_DecoderSamplePosition);
#ifdef AT9_STREAM_DEBUG_OUTPUT
		char msg[256];
		sprintf( msg, "FileEndOnThisFrame %p\n", this );
		AKPLATFORM::OutputDebugMsg( msg );
#endif

		AKASSERT( !IsLoopOnThisFrame( numSamplesToDecode ) );

		numSamplesToDecode = T_BASE::m_uTotalSamples - m_DecoderSamplePosition;
		size_t newNumSamplesToDecode = FixDecodingLength(numSamplesToDecode);

		// discard added samples
		AkUInt32 uSkipEndSamplesLength = newNumSamplesToDecode - numSamplesToDecode;
		AkUInt32 skipStartIndex = m_Ringbuffer.m_WriteSampleIndex + numSamplesToDecode;
		bool bReadBeforeSkip = m_Ringbuffer.ReadBeforeSkipRequired( skipStartIndex );
		m_Ringbuffer.m_SkipFrames.SetSkipFrames( skipStartIndex % m_Ringbuffer.m_TotalSamples, uSkipEndSamplesLength, bReadBeforeSkip );
		numSamplesToDecode = newNumSamplesToDecode;
		originalSamplesToDecode = newNumSamplesToDecode;

		nextSamplePosition = T_BASE::m_uTotalSamples;
	}

	// ------------------------------------------------------------------------------------------------------------------
	// If looping, split into two segments
	
	int inputBufferIndex = 0;
	if ( IsLoopOnThisFrame( numSamplesToDecode ) )
	{
		AkUInt32 uLoopEnd = T_BASE::m_uPCMLoopEnd+1;
		AkInt32 fixedEndOfLoop = GetCodecLoopEnd();
		AkInt32 fixedStartOfLoop = GetCodecLoopStart();

		AkUInt32 skipEndSamples = (fixedEndOfLoop - uLoopEnd);
		AkUInt32 skipStartSamples = (T_BASE::m_uPCMLoopStart - fixedStartOfLoop);

		if (skipEndSamples > 0 || skipStartSamples > 0)
		{
			AKASSERT( m_DecoderSamplePosition < (AkInt32)GetCodecLoopEnd()  );
			AkUInt32 skipStartIndex = m_Ringbuffer.m_WriteSampleIndex + (uLoopEnd - m_DecoderSamplePosition);

			bool bReadBeforeSkip = m_Ringbuffer.ReadBeforeSkipRequired( skipStartIndex );
			m_Ringbuffer.m_SkipFrames.SetSkipFrames( skipStartIndex % m_Ringbuffer.m_TotalSamples, skipEndSamples + skipStartSamples, bReadBeforeSkip );
			AKASSERT((fixedEndOfLoop + m_uSkipSamplesOnFileStart) >= T_BASE::m_uPCMLoopEnd);
		}

		AKASSERT(numSamplesToDecode >= (fixedEndOfLoop - m_DecoderSamplePosition));
		inputBufferIndex = SetDecodingInputBuffers(m_DecoderSamplePosition, fixedEndOfLoop, 0, remainingSizeToDecode);

		AKASSERT(numSamplesToDecode >= (fixedEndOfLoop - m_DecoderSamplePosition));
		remainingSizeToDecode += numSamplesToDecode - (fixedEndOfLoop - m_DecoderSamplePosition);
		if (inputBufferIndex >= AKAT9_MAX_DECODER_INPUT_BUFFERS) // will not reach end of loop on this frame after all
		{
			if (numSamplesToDecode - (fixedEndOfLoop - m_DecoderSamplePosition) == remainingSizeToDecode ) // reached end of loop
			{
				numSamplesToDecode -= remainingSizeToDecode;
				nextSamplePosition = fixedStartOfLoop;
			}
			else
			{
				numSamplesToDecode -= remainingSizeToDecode;
				nextSamplePosition = m_DecoderSamplePosition + numSamplesToDecode;
				m_Ringbuffer.m_SkipFrames.Reset();
				m_uDecoderLoopCntAhead--;
			}
		}
		else if (remainingSizeToDecode >= m_nSamplesPerBlock) // go to loop start
		{
			AkUInt32 decodeLength = remainingSizeToDecode;
			inputBufferIndex = SetDecodingInputBuffers( fixedStartOfLoop, fixedStartOfLoop+decodeLength, inputBufferIndex, remainingSizeToDecode );
			nextSamplePosition = fixedStartOfLoop + decodeLength - remainingSizeToDecode;
			numSamplesToDecode -= remainingSizeToDecode;
			AKASSERT(nextSamplePosition <= (int)(T_BASE::m_uTotalSamples + m_nSamplesPerBlock) / m_nSamplesPerBlock * m_nSamplesPerBlock);
		}
		else
		{
			numSamplesToDecode -= remainingSizeToDecode;
			nextSamplePosition = fixedStartOfLoop;
		}
		
		m_uDecoderLoopCntAhead++;
	}
	else
	{
		inputBufferIndex = SetDecodingInputBuffers(m_DecoderSamplePosition, m_DecoderSamplePosition+numSamplesToDecode, 0, remainingSizeToDecode);
		numSamplesToDecode -= remainingSizeToDecode;
		nextSamplePosition = m_DecoderSamplePosition + numSamplesToDecode;
		AKASSERT(nextSamplePosition <= (int)(T_BASE::m_uTotalSamples + m_uSkipSamplesOnFileStart + m_nSamplesPerBlock - 1) / m_nSamplesPerBlock * m_nSamplesPerBlock);
	}
	
	numBuffersIn = inputBufferIndex; //+1;
	numBuffersOut = SetDecodingOutputBuffers(numSamplesToDecode);

	// ------------------------------------------------------------------------------------------------------------------
	// Set job

	const uint64_t DECODE_FLAGS = SCE_AJM_FLAG_RUN_MULTIPLE_FRAMES | SCE_AJM_FLAG_SIDEBAND_STREAM | SCE_AJM_FLAG_SIDEBAND_FORMAT;
	AKASSERT(numBuffersIn <= AKAT9_MAX_DECODER_INPUT_BUFFERS);
	AKASSERT(numBuffersOut <= 2);
	int res = sceAjmBatchJobRunSplit(in_Batch, m_AjmInstanceId, DECODE_FLAGS, m_InputBufferDesc, numBuffersIn,
						   m_OutputBufferDesc, numBuffersOut, &m_DecodeResult, sizeof (m_DecodeResult));

	//AKVERIFY( res == SCE_OK );
	if (res != SCE_OK)
	{ 
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_ATRAC9DecodeFailed, T_BASE::m_pCtx);
		m_DecodingFailed = true;
		return AK_Fail;
	}
	AKASSERT( !(Decoder_Doloop() > 0) || (nextSamplePosition < (AkInt32)GetCodecLoopEnd()) );

	// ------------------------------------------------------------------------------------------------------------------
	// Increment pointers for next job
	AKASSERT(nextSamplePosition <= (int)(T_BASE::m_uTotalSamples + m_nSamplesPerBlock + m_nSamplesPerBlock) / m_nSamplesPerBlock * m_nSamplesPerBlock);
	m_DecoderSamplePosition = nextSamplePosition;

	m_Ringbuffer.m_RingbufferUsedSample += numSamplesToDecode;
	AKASSERT(originalSamplesToDecode >= numSamplesToDecode);
	m_uRemainingSamples = originalSamplesToDecode - numSamplesToDecode;
	m_bFirstDecodingFrameCompleted = true;
	AKASSERT(m_Ringbuffer.m_RingbufferUsedSample > 0 && m_Ringbuffer.m_RingbufferUsedSample <= m_Ringbuffer.m_TotalSamples);
	m_Ringbuffer.m_WriteSampleIndex = (m_Ringbuffer.m_WriteSampleIndex + numSamplesToDecode) % m_Ringbuffer.m_TotalSamples;
	
	if (m_InputBufferDesc[0].szSize + m_InputBufferDesc[1].szSize == 0)
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_StreamingSourceStarving, T_BASE::m_pCtx );
		return AK_NoDataReady;
	}

#ifdef AT9_STREAM_DEBUG_OUTPUT
	if(m_uRemainingSamples != 0)
	{
		sprintf( msg, "-- -- CreateDecodingJob remaining : %i \n", m_uRemainingSamples);
		AKPLATFORM::OutputDebugMsg( msg );
	}
#endif

	AKASSERT(m_Ringbuffer.m_RingbufferUsedSample <= m_Ringbuffer.m_TotalSamples);

	m_DecodingStarted = true;
	return AK_Success;
}

// -------------------------------------------------------------------------------------
// Decoding job helper functions

template <class T_BASE>
AkUInt32 CAkSrcACPBase<T_BASE>::SetDecodingOutputBuffers(size_t in_uSampleLength)
{
	size_t numBuffersOut;
	AkUInt32 uNumChannels = this->m_pCtx->GetMediaFormat().channelConfig.uNumChannels;

	if ((m_Ringbuffer.m_WriteSampleIndex + in_uSampleLength) > m_Ringbuffer.m_TotalSamples)
	{		
		// Output
		m_OutputBufferDesc[0].pAddress = m_Ringbuffer.GetWriteBuffer( SampleToBufferIndex(m_Ringbuffer.m_WriteSampleIndex) );

		size_t numSamplesA = (m_Ringbuffer.m_TotalSamples - m_Ringbuffer.m_WriteSampleIndex);
		size_t numSamplesB = (in_uSampleLength - (m_Ringbuffer.m_TotalSamples - m_Ringbuffer.m_WriteSampleIndex));

		m_OutputBufferDesc[0].szSize = (size_t)SamplesToOutputBytes( numSamplesA );
		m_OutputBufferDesc[1].pAddress = m_Ringbuffer.GetWriteBuffer(0);
		m_OutputBufferDesc[1].szSize = (size_t)SamplesToOutputBytes( numSamplesB );

		AKASSERT( m_OutputBufferDesc[0].szSize % m_nSamplesPerBlock == 0);
		AKASSERT( m_OutputBufferDesc[1].szSize % m_nSamplesPerBlock == 0);
		AKASSERT( (m_OutputBufferDesc[0].szSize + m_OutputBufferDesc[1].szSize) <= SCE_AJM_DEC_AT9_MAX_OUTPUT_BUFFER_SIZE );
		numBuffersOut = 2;
	}
	else
	{
		m_OutputBufferDesc[0].pAddress = m_Ringbuffer.GetWriteBuffer( SampleToBufferIndex(m_Ringbuffer.m_WriteSampleIndex) );
		m_OutputBufferDesc[0].szSize = (size_t)SamplesToOutputBytes(in_uSampleLength);
		m_OutputBufferDesc[1].szSize = 0;
			
		AKASSERT( m_OutputBufferDesc[0].szSize % m_nSamplesPerBlock == 0);
		AKASSERT(m_OutputBufferDesc[0].szSize <= SCE_AJM_DEC_AT9_MAX_OUTPUT_BUFFER_SIZE );
		numBuffersOut = 1;
	}

#ifdef AT9_STREAM_DEBUG_OUTPUT
		char msg[256];
		sprintf( msg, "-- -- -- -- SetDecodingOutputBuffers target len:%i, nb:%i [%p,%i], [%p,%i]\n", in_uSampleLength, numBuffersOut,
			m_Ringbuffer.m_WriteSampleIndex/*m_OutputBufferDesc[0].pAddress*/, m_OutputBufferDesc[0].szSize,
			0/*m_OutputBufferDesc[1].pAddress*/, m_OutputBufferDesc[1].szSize);
		AKPLATFORM::OutputDebugMsg( msg );
#endif

	return numBuffersOut;
}

#ifdef _DEBUG
// Ensures the decoding write index is following the current read sample on every fame, used only for debugging purpose.
template <class T_BASE>
AkInt32 CAkSrcACPBase<T_BASE>::GetCurrentDecodedReadSample() const
{
	//AKASSERT( m_DecoderSamplePosition <= this->m_uPCMLoopEnd );
	AkInt32 currentReadSample;
	if ( m_Ringbuffer.m_SkipFrames.uSkipSamplesLength == 0 && m_Ringbuffer.m_SkipFramesStart.uSkipSamplesLength == 0 )
	{
		currentReadSample = m_DecoderSamplePosition - m_Ringbuffer.m_RingbufferUsedSample;
	}
	else
	{
		currentReadSample = this->m_uCurSample;
	}

	/*if (this->m_uCurSample < m_DecoderSamplePosition)
	{
		AKASSERT((m_Ringbuffer.m_RingbufferUsedSample - m_Ringbuffer.m_SkipFramesStart.uSkipSamplesLength) <= m_DecoderSamplePosition);
	}*/

	// for debugging
	if( (AkInt32)this->m_uCurSample != currentReadSample )
	{
		return currentReadSample;
	}
	return currentReadSample;
}
#endif

template <class T_BASE>
AkInt32 CAkSrcACPBase<T_BASE>::DecodeStartPosition( AkUInt32 in_uTargetPosition ) const
{
	bool isSuperframesOn = m_uSkipSamplesOnFileStart != m_nSamplesPerBlock;
	AkInt32 newPosition = 0;
	
	AKASSERT( !isSuperframesOn || (m_uSkipSamplesOnFileStart * 4) == m_nSamplesPerBlock );

	if (in_uTargetPosition < m_nSamplesPerBlock)
	{
		newPosition = 0;
	}
	else if ( in_uTargetPosition % m_nSamplesPerBlock != 0 )
	{
		if (isSuperframesOn && ( in_uTargetPosition % m_nSamplesPerBlock < AK_AT9_FRAME_SIZE ) )
		{
			newPosition = FloorSampleToBlockSize( in_uTargetPosition - m_nSamplesPerBlock );
		}
		else
		{
			newPosition = FloorSampleToBlockSize( in_uTargetPosition );
		}
		
	}
	else // ( in_uTargetPosition % m_nSamplesPerBlock == 0 )
	{
		newPosition = in_uTargetPosition;
	}

	newPosition -= m_uSkipSamplesOnFileStart;

	AKASSERT(newPosition >= -AK_AT9_FRAME_SIZE);
	return newPosition;
}