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
 
#ifndef _AKMISMATCHBUFFERING_H_
#define _AKMISMATCHBUFFERING_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>

// Mismatch buffer.  For dealing with effects that must process fixed-sized chunks of data at a time, that are 
//		not necessarily the same size as that of the audio pipeline buffer.  
//
// How to use:
/*
	class MyEffect
	{
		// Mismatch buffer declaration.
		AkMismatchBuffer mmb;
		... 
	};

	void MyEffect::Execute( AkAudioBuffer * io_pBuffer )
	{
		// Buffer pointer to data that needs to be processed in fixed-size chunks.
		AkAudioBuffer procBuffer;

		mmb.Prologue(*io_pBuffer);

		do
		{
			// Store the input ..
			mmb.BufferInput(*io_pBuffer);

			while (mmb.GetAudioToProcess(*io_pBuffer, procBuffer))
			{
				// Process audio which is in procBuffer with this imaginary function.  (don't touch io_pBuffer)
				ProcessAudio(procBuffer);

				// Indicate that we are done.
				mmb.ProcessingComplete(procBuffer);
			}

		}
		while ( mmb.GetOutputAudio(*io_pBuffer, procBuffer) ); //< - Get output, and keep going if necessary.
	}

*/

class AkMismatchBuffer
{
public:

	AkMismatchBuffer() : m_pBufferMem(NULL)
		, m_uProcessBlockFrames(0)
		, m_uChannels(0)
		, m_uWriteIdx(0)
		, m_uProcessIdx(0)
		, m_uReadIdx(0)
		, m_eState(AK_DataNeeded)
		, m_uPipelineBufferConsumed(0)
		, m_uPipelineBufferWritten(0)
		, m_uPrebufferingFrames(0)
		, m_uNominalBuffering(0)
	{}

	// Allocates the mismatch buffer, if the number of processing frames is not equal to the pipeline buffer size.
	//
	template< typename Alloc >
	AKRESULT Init(AkUInt32 in_uPipelineNumFrames, AkUInt32 in_uProcessingBufferFrames, AkUInt32 in_uChannels, Alloc& in_alloc)
	{
		AKRESULT res = AK_Success;

		if (in_uProcessingBufferFrames > in_uPipelineNumFrames)
		{
			m_uMmbSizeFrames = in_uProcessingBufferFrames;
			m_uNominalBuffering = (((in_uProcessingBufferFrames - 1) / in_uPipelineNumFrames) * in_uPipelineNumFrames);

			if (in_uProcessingBufferFrames % in_uPipelineNumFrames != 0)
			{
				// If there is an non-integer ratio of processing buffer frames to pipeline frames, then double buffer to handle overflow.
				m_uMmbSizeFrames *= 2;

				// The leftover dry frames after a process step are incurred as additional latency.
				m_uNominalBuffering += (in_uPipelineNumFrames - (in_uProcessingBufferFrames % in_uPipelineNumFrames));
			}
			
		}
		else if (	in_uPipelineNumFrames > in_uProcessingBufferFrames && 
					(in_uPipelineNumFrames % in_uProcessingBufferFrames) != 0 )
		{
			m_uMmbSizeFrames = 2 * in_uProcessingBufferFrames;
			m_uNominalBuffering = in_uProcessingBufferFrames;
		}
		else
		{
			// No mismatch between buffer size (or integer ratio with in_uPipelineNumFrames > in_uProcessingBufferFrames, 
			//	We leave it to the plugin (eg. convolution reverb) to handle this case, which it can do so slightly more efficiently (and with no latency).)
			m_uMmbSizeFrames = 0;
			m_uNominalBuffering = 0;
			m_pBufferMem = NULL;
		}

		if (m_uMmbSizeFrames > 0)
		{
			m_pBufferMem = (AkReal32 *)in_alloc.Malloc(m_uMmbSizeFrames*in_uChannels*sizeof(AkReal32));
			if (m_pBufferMem == NULL)
			{
				m_uMmbSizeFrames = 0;
				res = AK_Fail;
			}
		}

		m_uProcessBlockFrames = in_uProcessingBufferFrames;
		m_uChannels = in_uChannels;

		Reset();

		return res;
	}

	template< typename Alloc >
	void Term(Alloc& in_alloc)
	{
		if (m_pBufferMem != NULL)
		{
			in_alloc.Free(m_pBufferMem);
			m_pBufferMem = NULL;
		}
		m_uMmbSizeFrames = 0;
		m_uProcessBlockFrames = 0;
		m_uChannels = 0;
	}

	// Called upon entering Execute
	void Prologue(AkAudioBuffer& in_pipelineBuffer)
	{
		if (m_pBufferMem != NULL)
		{
			m_uPipelineBufferConsumed = 0;
			m_uPipelineBufferWritten = 0;

			// Adjust internal latency to match nominal buffering amount: happens on first buffer,
			// and when returning from Tail processing.
			if (in_pipelineBuffer.eState != AK_NoMoreData
				&& (m_uPrebufferingFrames + (m_uWriteIdx - m_uReadIdx) < m_uNominalBuffering))
			{
				m_uPrebufferingFrames = m_uNominalBuffering - (m_uWriteIdx - m_uReadIdx);
			}
		}
	}

	inline void Reset()
	{
		m_uWriteIdx = 0;
		m_uProcessIdx = 0;
		m_uReadIdx = 0;
		m_uPrebufferingFrames = 0;
	}

	// When mismatch buffering is enabled, this method buffers the input data from the pipeline buffer.  This function must be called again, as
	//	long as a susequent call to GetOutputAudio() returns true.
	// If buffering is not enabled, this function does nothing.
	void BufferInput( AkAudioBuffer& in_pipelineBuffer )
	{
		AKASSERT(in_pipelineBuffer.NumChannels() == m_uChannels);

		if (m_pBufferMem != NULL)
		{
			AKASSERT(m_uReadIdx <= m_uWriteIdx);
			AKASSERT(m_uMmbSizeFrames >= (m_uWriteIdx - m_uReadIdx));

			if (m_uPrebufferingFrames > 0)
			{
				AkUInt32 uFramesToZero = AkMin(m_uPrebufferingFrames, GetMmbWriteRoom());

				AkUInt32 uZeroesAddedToBuffer = 0;

				do
				{
					const AkUInt32 uWriteSize = GetMmbWriteSize(uZeroesAddedToBuffer, uFramesToZero);

					for (AkUInt32 uCh = 0; uCh < m_uChannels; ++uCh)
					{
						AKPLATFORM::AkMemSet(GetMmbWritePtr(uCh), 0, uWriteSize * sizeof(AkReal32));
					}
					uZeroesAddedToBuffer += uWriteSize;
					m_uWriteIdx += uWriteSize;
				} while (uZeroesAddedToBuffer < uFramesToZero);
				
				m_uPrebufferingFrames -= uFramesToZero;
			}

			//Store input audio in the mismatch buffer.
			if (in_pipelineBuffer.uValidFrames > m_uPipelineBufferConsumed)
			{
				AKASSERT(m_uPipelineBufferConsumed >= m_uPipelineBufferWritten);

				AkUInt32 uFramesAddedToBuffer = 0;
				AkUInt32 uMaxFramesToBuffer = AkMin(in_pipelineBuffer.uValidFrames - m_uPipelineBufferConsumed, GetMmbWriteRoom());

				do
				{
					const AkUInt32 uWriteSize = GetMmbWriteSize(uFramesAddedToBuffer, uMaxFramesToBuffer);

					for (AkUInt32 uCh = 0; uCh < m_uChannels; ++uCh)
					{
						AKPLATFORM::AkMemCpy(GetMmbWritePtr(uCh), in_pipelineBuffer.GetChannel(uCh) + m_uPipelineBufferConsumed + uFramesAddedToBuffer, uWriteSize*sizeof(AkReal32));
					}
					uFramesAddedToBuffer += uWriteSize;
					m_uWriteIdx += uWriteSize;
				} while (uFramesAddedToBuffer < uMaxFramesToBuffer);

				m_uPipelineBufferConsumed += uFramesAddedToBuffer;
			}
		}
	}

	// When mismatch buffering is enabled, this method will return a valid buffer in out_processBuffer,	if we have enough data to process. 
	//	If buffering is not enabled, than after this function is called out_processBuffer will point to the buffer pass in in_pipelineBuffer.
	//	Returns true if there is audio to process, and in which case out_processBuffer will point to valid data that needs to be processed.
	//	ProcessingComplete() must be called after the processing is finished.
	bool GetAudioToProcess(AkAudioBuffer& in_pipelineBuffer, AkAudioBuffer& out_processBuffer, bool in_bHasTail)
	{
		bool bNeedToProcess = true;

		if (m_pBufferMem != NULL)
		{
			AkUInt32 uFramesToProcess = m_uWriteIdx - m_uProcessIdx;// +m_uZeroFrames;

			if (GetMmbProcessRoom() >= m_uProcessBlockFrames
				&& (uFramesToProcess >= m_uProcessBlockFrames
				|| (in_pipelineBuffer.eState == AK_NoMoreData 
					&& m_uPipelineBufferConsumed >= in_pipelineBuffer.uValidFrames
					&& in_bHasTail)))
			{
				AkUInt32 uValidFrames = AkMin(m_uProcessBlockFrames, uFramesToProcess);

				out_processBuffer.AttachContiguousDeinterleavedData(
					GetMmbProcessPtr(),
					(AkUInt16)m_uProcessBlockFrames,
					(AkUInt16)uValidFrames,
					in_pipelineBuffer.GetChannelConfig());

				
				//Set the state of the convolution buffer to that of the last pipeline buffer.
				out_processBuffer.eState = in_pipelineBuffer.eState;
			}
			else
			{
				bNeedToProcess = false;
				out_processBuffer.eState = AK_NoDataReady;				
			}
		}
		else
		{
			if (m_eState == AK_DataNeeded)
			{
				//There is no mismatch between IR block size and pipeline buffer size, just pass the data through.
				out_processBuffer.AttachContiguousDeinterleavedData(
					in_pipelineBuffer.GetChannel(0),
					in_pipelineBuffer.MaxFrames(),
					in_pipelineBuffer.uValidFrames,
					in_pipelineBuffer.GetChannelConfig());

				out_processBuffer.eState = in_pipelineBuffer.eState;
			}
			else
			{
				bNeedToProcess = false;
			}
		}

		return bNeedToProcess;
	}

	// Called to indicate that a buffer returned from GetAudioToProcess() has been processed.  
	void ProcessingComplete(AkAudioBuffer& in_processBuffer)
	{
		if (m_pBufferMem != NULL )
		{
			AKASSERT(in_processBuffer.eState != AK_NoDataReady);

			//If we just processed a convolution buffer, then save the state and the number of valid frames
			m_uProcessIdx += in_processBuffer.uValidFrames;
			m_uWriteIdx = AkMax(m_uWriteIdx, m_uProcessIdx); // Bump the write pointer in case of tail handling
		}
		
		m_eState = in_processBuffer.eState;
	}

	//	When mismatch buffering is enabled, this method returns the next block of processed data from the mismatch buffer in the size appropriate for the pipeline.
	//		If mismatch buffering is not enabled, aftur returning, out_pipelineBuffer will point to the data in in_processBuffer (they are spapped back to normal).
	//		If this function returns true, there is more input that need buffering of more data to be output, and BufferInput() must be called again, followed by another call to GetOutputAudio()
	bool GetOutputAudio(	AkAudioBuffer& out_pipelineBuffer, 
							AkAudioBuffer& in_processBuffer
						)
	{
		AKASSERT(out_pipelineBuffer.MaxFrames() >= m_uPipelineBufferConsumed);
		AKASSERT(m_uPipelineBufferWritten < out_pipelineBuffer.MaxFrames());

		if (m_pBufferMem != NULL)
		{
			AkUInt32 uMaxFramesToCopy = (out_pipelineBuffer.eState == AK_NoMoreData && m_uPipelineBufferConsumed >= out_pipelineBuffer.uValidFrames)
				? out_pipelineBuffer.MaxFrames() 
				: m_uPipelineBufferConsumed;

			do
			{
				AkUInt32 uFramesToCopy = GetMmbReadSize(m_uPipelineBufferWritten, uMaxFramesToCopy);

				for (AkUInt32 uCh = 0; uCh < out_pipelineBuffer.NumChannels(); ++uCh)
				{
					AKPLATFORM::AkMemCpy(out_pipelineBuffer.GetChannel(uCh) + m_uPipelineBufferWritten,
											GetMmbReadPtr(uCh), 
											uFramesToCopy * sizeof(AkReal32)
											);
				}

				m_uPipelineBufferWritten += uFramesToCopy;

				m_uReadIdx += uFramesToCopy;
			} 
			while (m_uProcessIdx > m_uReadIdx && m_uPipelineBufferWritten < uMaxFramesToCopy);

			AkUInt32 uMaxFrames = out_pipelineBuffer.eState == AK_NoMoreData ? out_pipelineBuffer.MaxFrames() : out_pipelineBuffer.uValidFrames;
			AKASSERT(m_uPipelineBufferWritten <= uMaxFrames);

			if (m_uPipelineBufferWritten == uMaxFrames || m_eState == AK_NoMoreData )
			{
				out_pipelineBuffer.uValidFrames = (AkUInt16)m_uPipelineBufferWritten;

				if (m_uProcessIdx > m_uReadIdx || m_uWriteIdx > m_uReadIdx) //More data waiting next go round;
					out_pipelineBuffer.eState = AK_DataReady;
				else //Set the pipeline buffer state to that of the last processed convolution buffer.
					out_pipelineBuffer.eState = m_eState;

				return false; // Finished this frame
			}
			else
			{
				return true; // Keep going.
			}
		}
		else
		{
			//There is no mismatch between IR block size and pipeline buffer size, just pass the data through.
			out_pipelineBuffer.AttachContiguousDeinterleavedData(
				in_processBuffer.GetChannel(0),
				in_processBuffer.MaxFrames(),
				in_processBuffer.uValidFrames,
				in_processBuffer.GetChannelConfig());

			out_pipelineBuffer.eState = in_processBuffer.eState;
			m_eState = AK_DataNeeded;

			return false;
		}
	}

	bool IsMismatchBufferingEnabled()
	{
		return m_pBufferMem != NULL;
	}

	// AkMismatchBuffer Helper functions.
	static AkUInt32 RoundToNextPowerOfTwo(AkUInt32 in_Size)
	{
		if ((in_Size & (in_Size - 1)) == 0)// If the MSB gets flipped on -1, this was a power of two.
			return in_Size; // Already a power of two.

		while (in_Size & in_Size - 1)// while there is more than one bit
			in_Size = in_Size - 1;

		return in_Size << 1;// Power to next
	}

	AKRESULT GetBufferState()
	{
		return m_eState;
	}

private:

	AkForceInline AkReal32* GetMmbReadPtr(AkUInt32 uChan) const
	{
		const AkUInt32 uBufferBase = ((m_uReadIdx / m_uProcessBlockFrames) * m_uProcessBlockFrames) % m_uMmbSizeFrames;
		const AkUInt32 uReadFrameIdx = m_uReadIdx % m_uProcessBlockFrames;
		return m_pBufferMem + (uBufferBase*m_uChannels) + uReadFrameIdx + (uChan*m_uProcessBlockFrames);
	}

	AkForceInline AkUInt32 GetMmbReadSize(AkUInt32 in_uAlreadyRead, AkUInt32 in_uToBufferSize) const
	{
		const AkUInt32 uReadFrameIdx = m_uReadIdx % m_uProcessBlockFrames;
		return AkMin(m_uProcessIdx - m_uReadIdx, AkMin(m_uProcessBlockFrames - uReadFrameIdx, in_uToBufferSize - in_uAlreadyRead));
	}

	// Get the pointer to the start of the current processing buffer.  
	//	The current buffer is the one that we are not writing to, or have just finished writing to.
	AkForceInline AkReal32* GetMmbProcessPtr() const
	{
		AKASSERT(!(m_uProcessIdx % m_uProcessBlockFrames));
		const AkUInt32 uBufferBase = m_uProcessIdx % m_uMmbSizeFrames;
		return m_pBufferMem + uBufferBase*m_uChannels;
	}

	AkForceInline AkReal32* GetMmbWritePtr(AkUInt32 uChan) const
	{
		const AkUInt32 uBufferBase = ((m_uWriteIdx / m_uProcessBlockFrames) * m_uProcessBlockFrames) % m_uMmbSizeFrames;
		const AkUInt32 uWriteFrameIdx = m_uWriteIdx % m_uProcessBlockFrames;
		return m_pBufferMem + (uBufferBase*m_uChannels) + uWriteFrameIdx + (uChan*m_uProcessBlockFrames);
	}

	AkForceInline AkUInt32 GetMmbWriteSize(AkUInt32 in_uAlreadyWritten, AkUInt32 in_uFromBufferSize) const
	{
		const AkUInt32 uWriteFrameIdx = m_uWriteIdx % m_uProcessBlockFrames;
		return AkMin(m_uProcessBlockFrames - uWriteFrameIdx, in_uFromBufferSize - in_uAlreadyWritten);
	}

	AkForceInline AkUInt32 GetMmbWriteRoom() const
	{
		return m_uMmbSizeFrames - (m_uWriteIdx - m_uReadIdx);
	}

	AkForceInline AkUInt32 GetMmbProcessRoom() const
	{
		return m_uMmbSizeFrames - (m_uProcessIdx - m_uReadIdx);
	}

	AkReal32* m_pBufferMem;			// Buffer memory
	AkUInt32 m_uMmbSizeFrames;		// Size of the mismatch buffer, in frames.  Either 1X or 2X m_uProcessBlockFrames.

	AkUInt32 m_uProcessBlockFrames; // Size of the chunks of data to process. Invariant throughout lifetime of this object. 
	AkUInt32 m_uChannels;			// Number of channels. Invariant throughout lifetime of this object.

	AkUInt32 m_uWriteIdx;			// Sample index where we are writing, grows continuously.
	AkUInt32 m_uProcessIdx;			// Sample index where processing is at, grows continuously.
	AkUInt32 m_uReadIdx;			// Sample index where we are reading, grows continuously.
	AKRESULT m_eState;				// State of the last audio buffer added.

	AkUInt32 m_uPipelineBufferConsumed; 
	AkUInt32 m_uPipelineBufferWritten;
	AkUInt32 m_uPrebufferingFrames;
	AkUInt32 m_uNominalBuffering;	// Nominal distance between m_uWriteIdx and m_uReadIdx

} AK_ALIGN_DMA;
#endif // _AKMISMATCHBUFFERING_H_
