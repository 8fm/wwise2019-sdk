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

#pragma once

#include <ajm/at9_decoder.h>
#include <ajm.h>
#include "AkSrcBase.h"

#include "AkSrcACPHelper.h"

#include "AkVPLSrcCbxNode.h"

//#define AT9_STREAM_DEBUG 1
//#define AT9_STREAM_DEBUG_OUTPUT 1

#define AK_AT9_FRAME_SIZE 256

template <class T_BASE>
class CAkSrcACPBase : public T_BASE
{
public:
	// -------------------------------------------------------------------------------------
	//Constructor / destructor

	CAkSrcACPBase( CAkPBI * in_pCtx );
	virtual ~CAkSrcACPBase();

	// -------------------------------------------------------------------------------------
	// From CAkSrcBaseEx

	virtual void		StopStream();
	virtual void		ReleaseBuffer();
	virtual AKRESULT 	SeekToSourceOffset();
	virtual AKRESULT 	ChangeSourcePosition();
	virtual bool		SupportMediaRelocation() const;
	// Sets current sample and updates loop count.
	virtual AKRESULT TimeSkip( AkUInt32 & io_uFrames );

	// -------------------------------------------------------------------------------------
	// From CAkSrcBaseEx
	
	AkForceInline bool DecodingStarted() const
	{ 
		return m_DecodingStarted;
	}

	virtual void		GetBuffer( AkVPLState & io_state );
	
	virtual AkInt32 SrcProcessOrder() { return SrcProcessOrder_HwVoice; }

	//Pitch that is passed to audio pipeline
	virtual AkReal32 GetPitch() const 
	{
		//pitch is stored to be in sync with decoded data, and may be delayed a frame.
		return m_fCurPitchForDecodedData; 
	}

	// -------------------------------------------------------------------------------------
	// Hardware decoder functions
	
	virtual bool 		Register();
	virtual void		Unregister();
	AKRESULT 			CreateHardwareInstance( const SceAjmContextId in_AjmContextId );
	void				InitIfNeeded(SceAjmBatchInfo* in_Batch);
	AKRESULT 			CreateDecodingJob(SceAjmBatchInfo* in_Batch);
	AKRESULT 			InitialiseDecoderJob( SceAjmBatchInfo* in_Batch );
	bool				IsInitialized() const { return m_Initialized; }

	// -------------------------------------------------------------------------------------
	// Decoding job helper functions

	AkForceInline AkUInt32 FloorSampleToBlockSize( AkUInt32 in_startSample ) const
	{
		return (in_startSample / m_nSamplesPerBlock) * m_nSamplesPerBlock;  
	}

	AkForceInline AkUInt32 CeilSampleToBlockSize( AkUInt32 in_startSample ) const
	{
		return ((in_startSample + m_nSamplesPerBlock-1) / m_nSamplesPerBlock) * m_nSamplesPerBlock;  
	}

	AkForceInline AkUInt32 FixDecodingLength( AkUInt32 in_numSample ) const
	{
		if ( in_numSample % m_nSamplesPerBlock != 0 )
		{
			// Round to the next block
			return ((in_numSample / m_nSamplesPerBlock) + 1) * m_nSamplesPerBlock;
		}

		return in_numSample;
	}

	AkForceInline AkUInt32 GetNumSamplesToDecode(AkUInt32& io_extraRequiredFrames)
	{
		// If we still have some samples to decode for this frame, use those
		AkUInt32 samplesToDecode = (io_extraRequiredFrames > 0) ? io_extraRequiredFrames : GetNumSamplesToDecodeHelper();		

		// More space needed in decoding buffer
		if (m_Ringbuffer.CalcFreeSpace() < samplesToDecode)
		{
			// Might as well grow an extra block than needed
			AkUInt32 newTotalSample = CeilSampleToBlockSize( m_Ringbuffer.CalcUsedSpace() + samplesToDecode + m_nSamplesPerBlock );
			if (m_Ringbuffer.Grow( newTotalSample, T_BASE::m_pCtx->GetMediaFormat().channelConfig.uNumChannels ) != AK_Success)
			{
				DecodingFailed();
				return 0;
			}
		}

		AkUInt32 maxSamplesAvailable = FloorSampleToBlockSize(GetMaxSamplesToDecodeHelper());
		// Not all sample can be decoded, will require anoter pass
		if (maxSamplesAvailable < samplesToDecode)
		{
			io_extraRequiredFrames = CeilSampleToBlockSize(samplesToDecode - maxSamplesAvailable);
			samplesToDecode = maxSamplesAvailable;
		}
		else
		{
			io_extraRequiredFrames = 0;
		}

		AKASSERT( m_Ringbuffer.m_RingbufferUsedSample + samplesToDecode >= 0 && m_Ringbuffer.m_RingbufferUsedSample + samplesToDecode <= m_Ringbuffer.m_TotalSamples );
		AKASSERT( samplesToDecode % m_nSamplesPerBlock == 0);

		return samplesToDecode;
	}

	AkForceInline AkUInt32 GetCodecLoopEnd() const
	{
		// On SuperFrames, m_uSkipSampleOnFileStart changes where frames lands.
		// ex: skipOnFileStart = 256, samplePerBlocks = 1024 will yields reads in our decoderSample tracking of [-256, 768], [768, 1792], ...
		// Therefore, clamping the loopEnd needs to take into account m_uSkipSamplesOnFileStart, to end at a boundary % m_nSamplesPerBlock - m_uSkipSamplesOnFileStart
		return CeilSampleToBlockSize( (T_BASE::m_uPCMLoopEnd + 1) + m_uSkipSamplesOnFileStart) - ( m_uSkipSamplesOnFileStart );  
	}

	AkForceInline AkUInt32 GetCodecLoopStart() const
	{
		return DecodeStartPosition( T_BASE::m_uPCMLoopStart );  
	}
	
	virtual AKRESULT StopLooping()
	{
		AKRESULT res = T_BASE::StopLooping();
		T_BASE::SetLoopCnt( T_BASE::GetLoopCnt() + m_uDecoderLoopCntAhead );
		return res;
	}

	AkForceInline bool Decoder_Doloop() const
	{ 
		if ( T_BASE::GetLoopCnt() == LOOPING_INFINITE )
			return true;
		// Otherwise, deduce streaming loop count from source loop count and streaming loop ahead.
		AKASSERT( T_BASE::GetLoopCnt() >= m_uDecoderLoopCntAhead && ( T_BASE::GetLoopCnt() - m_uDecoderLoopCntAhead ) >= 1 );
		return ( ( T_BASE::GetLoopCnt() - m_uDecoderLoopCntAhead ) != LOOPING_ONE_SHOT );
	}

	bool AkForceInline Decoder_IsLoopPending() const 
	{
		return ( m_uDecoderLoopCntAhead > 0 );
	}

	AkUInt32 AkForceInline NumSamplesToSkipOnLoop() const 
	{
		AkUInt32 uLoopEnd = T_BASE::m_uPCMLoopEnd+1;
		AkInt32 fixedEndOfLoop = GetCodecLoopEnd();
		AkInt32 fixedStartOfLoop = GetCodecLoopStart();

		AkUInt32 skipEndSamples = (fixedEndOfLoop - uLoopEnd);
		AkUInt32 skipStartSamples = (T_BASE::m_uPCMLoopStart - fixedStartOfLoop);

		return skipEndSamples + skipStartSamples;
	}

	//	return (m_DecoderSamplePosition + in_numSampleToRead > this->m_uPCMLoopEnd && Decoder_IsLoopPending());
	virtual AkForceInline bool	IsLoopOnThisFrame( AkUInt32 in_numSampleToRead ) const
	{		
		// Use actual loopEnd and not the gentle file loop point for stream, discarted samples should be included to get all data needed and finish on boundaries
		AkInt32 fixedEndOfLoop = GetCodecLoopEnd();
		AkInt32 lastSample = (AkInt32)(m_DecoderSamplePosition + in_numSampleToRead);
		return (( Decoder_Doloop() > 0 ) && (lastSample >= fixedEndOfLoop));
	}
		
	virtual AkForceInline bool DataNeeded()
	{
		if(m_bStartOfFrame)
		{
			//The pitch may change between now and when the data is ready.
			m_fCurPitchForDecodedData = GetCurrentEffectivePitch();

			AkUInt32 extraRequiredFrames = m_uRemainingSamples; // discarted
			return !m_IsVirtual && 
				(AkInt32)GetNumSamplesToDecode(extraRequiredFrames) > 0 && 
				(m_DecoderSamplePosition < (AkInt32)T_BASE::m_uTotalSamples || Decoder_Doloop()) &&
				(FloorSampleToBlockSize( m_Ringbuffer.CalcFreeSpace() ) > 0);
		}
		else
		{	
			return !m_IsVirtual && m_uRemainingSamples > 0;
		}
		
	}

	AkForceInline bool	FileEndOnThisFrame( AkUInt32 in_numSampleToRead ) const
	{
		return (m_DecoderSamplePosition + in_numSampleToRead >= T_BASE::m_uTotalSamples && !Decoder_Doloop());
	}

	virtual int			SetDecodingInputBuffers(AkInt32 in_uSampleStart, AkInt32 in_uSampleEnd, AkUInt16 in_uBufferIndex, AkUInt32& out_remainingSize){return 0;};
	AkUInt32			SetDecodingOutputBuffers(size_t in_uSampleLength);
	void				SetSkipSamplesForLooping();

	
	virtual void		PrepareRingbuffer(){};
#ifdef _DEBUG
	AkInt32 			GetCurrentDecodedReadSample() const;
#endif	

	AkInt32				DecodeStartPosition( AkUInt32 in_uTargetPosition ) const;

	// -------------------------------------------------------------------------------------
	// Compressed / uncompressed size conversion helper functions
	
	AkForceInline AkUInt32 	ByteToSample( AkUInt32 in_byte ) const
	{
		AKASSERT( in_byte <= T_BASE::m_uDataSize );
		AKASSERT( (in_byte % m_nBlockSize) == 0 );
		AKASSERT( ((AkInt64)in_byte * m_nSamplesPerBlock / m_nBlockSize) * m_nBlockSize / m_nSamplesPerBlock == in_byte );
		
		// values should be set in children classes, default to 1 (PCM)
		return (in_byte / m_nBlockSize * m_nSamplesPerBlock);
	}

	AkForceInline AkUInt32 	ByteToSampleFloor( AkUInt32 in_byte ) const
	{		
		// values should be set in children classes, default to 1 (PCM)
		return ((in_byte / m_nBlockSize) * m_nSamplesPerBlock);
	}

	AkForceInline AkUInt32 	ByteToSampleCeil( AkUInt32 in_byte ) const
	{		
		// values should be set in children classes, default to 1 (PCM)
		return (((in_byte+m_nBlockSize-1) / m_nBlockSize) * m_nSamplesPerBlock);
	}

	AkForceInline AkUInt32 	SampleToByte( AkUInt32 in_sample ) const
	{
		AKASSERT( ((AkInt64)in_sample * m_nBlockSize / m_nSamplesPerBlock) <= ByteToSampleCeil(T_BASE::m_uDataSize) );
		AKASSERT( ((AkInt64)in_sample * m_nBlockSize / m_nSamplesPerBlock) % m_nBlockSize == 0 );

		// values should be set in children classes, default to 1 (PCM)
		return (in_sample / m_nSamplesPerBlock * m_nBlockSize) ;
	}

	// AT9 has a block to discart at the begining of the file. Hence, byte:0 -> sample:-m_nSamplesPerBlock
	AkForceInline AkInt32 	ByteToSampleRelative( AkUInt32 in_byte ) const
	{
		AKASSERT( in_byte <= T_BASE::m_uDataSize );
		AKASSERT( (in_byte % m_nBlockSize) == 0 );
		AKASSERT( ((AkInt64)in_byte * m_nSamplesPerBlock / m_nBlockSize) * m_nBlockSize / m_nSamplesPerBlock == in_byte );
		
		// values should be set in children classes, default to 1 (PCM)
		return ((in_byte) / m_nBlockSize * m_nSamplesPerBlock ) - m_uSkipSamplesOnFileStart;
	}

	// AT9 has a block to discart at the begining of the file. Hence, sample:0 -> byte:+m_nBlockSize
	AkForceInline AkUInt32 	SampleToByteRelative( AkInt32 in_sample ) const
	{
		in_sample += m_uSkipSamplesOnFileStart; //
		AKASSERT( ((AkInt64)in_sample * m_nBlockSize / m_nSamplesPerBlock) <= T_BASE::m_uDataSize );
		AKASSERT( ((AkInt64)in_sample * m_nBlockSize / m_nSamplesPerBlock) % m_nBlockSize == 0 );

		// values should be set in children classes, default to 1 (PCM)
		return in_sample / m_nSamplesPerBlock * m_nBlockSize ;
	}

	virtual AKRESULT OnLoopComplete(
		bool in_bEndOfFile		// True if this was the end of file, false otherwise.
		) 
	{
		if ( !in_bEndOfFile )
		{
			AKASSERT( Decoder_IsLoopPending() ); 
			--m_uDecoderLoopCntAhead;
		}

		return T_BASE::OnLoopComplete( in_bEndOfFile );
	}

	AkReal32 GetCurrentEffectivePitch() const
	{ 
		T_BASE::m_pCtx->CalcEffectiveParams();
		return AkMin(T_BASE::m_pCtx->GetEffectiveParams().Pitch(), 2400.0f);
	}

	AkForceInline AkReal32 GetLastRate() const
	{ 		
		return ((CAkVPLSrcCbxNode*)T_BASE::m_pCtx->GetCbx())->GetLastRate();
	}
	
	void DecodingFailed()
	{
		MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_ATRAC9DecodeFailed, T_BASE::m_pCtx);
		m_DecodingFailed = true;
		m_DecodingStarted = false;
	}

protected:

	AkForceInline AkReal32 GetFrameThroughput() const
	{
		AkAudioFormat format = T_BASE::m_pCtx->GetMediaFormat();
		return AK_NUM_VOICE_REFILL_FRAMES * ((1.f * format.uSampleRate) / AK_CORE_SAMPLERATE); 
	}

	AkForceInline AkUInt32 GetNumSamplesToDecodeHelper() const
	{
		AkReal32 fPitch = AkMath::Max( powf(2.0f, m_fCurPitchForDecodedData / 1200.0f), GetLastRate());

		AkUInt32 samplesNeeded = CeilSampleToBlockSize( (AkUInt32)( (fPitch * GetFrameThroughput()) + 2 /*for resampling interpolation*/ ) );

		if ( IsLoopOnThisFrame( samplesNeeded ) )
		{
			// About to loop, will try to decode more data to account for skipped samples
			samplesNeeded += NumSamplesToSkipOnLoop();
		}

		if ((AkInt32)samplesNeeded <= m_Ringbuffer.CalcAvailableFrames())
		{
			return 0;
		}

		AKASSERT((AkInt32)samplesNeeded > m_Ringbuffer.CalcAvailableFrames());
		samplesNeeded = CeilSampleToBlockSize(samplesNeeded - m_Ringbuffer.CalcAvailableFrames());

		return samplesNeeded;
	}

	AkForceInline AkUInt32 GetMaxSamplesToDecodeHelper() const
	{
		AkUInt32 uMaxExistingSamples = AkMin(FloorSampleToBlockSize(m_Ringbuffer.CalcFreeSpace()), ByteToSampleCeil(T_BASE::m_uDataSize));
		return AkMin(m_MaxOutputFramesPerJob, uMaxExistingSamples);
	}
	
	AkInt32				m_DecoderSamplePosition;
	AkUInt32			m_MaxOutputFramesPerJob;
	AkUInt32			m_uDecoderLoopCntAhead;
	AkUInt32			m_uNextSkipSamples;
	AkUInt32			m_uSkipSamplesOnFileStart;
	AkUInt32			m_uRemainingSamples;

	AkReal32			m_fCurPitchForDecodedData;

	AkUInt16			m_nSamplesPerBlock;
	AkUInt16			m_nBlockSize;

	AkUInt8				m_DecodingStarted			:1;
	AkUInt8				m_Initialized				:1;
	AkUInt8				m_DecodingFailed			:1;
	AkUInt8				m_IsVirtual					:1;
	AkUInt8				m_bFirstDecodingFrameCompleted		:1;
	AkUInt8				m_bStartOfFrame				:1;	

	SceAjmBuffer		m_InputBufferDesc[AKAT9_MAX_DECODER_INPUT_BUFFERS];
	SceAjmBuffer		m_OutputBufferDesc[2];

	sAkDecodeResult		m_DecodeResult;
	SceAjmInitializeResult	m_InitResult;
	SceAjmInstanceId	m_AjmInstanceId;
	SceAjmContextId		m_AjmContextId;

	CAkDecodingRingbuffer	m_Ringbuffer;
	SceAjmDecAt9InitializeParameters 	m_InitParam;
};

#include "AkSrcACPBase.inl"