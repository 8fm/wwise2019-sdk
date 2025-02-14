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

#ifndef _AKSRCFILEXMA_H
#define _AKSRCFILEXMA_H

#include "AkSrcFileBase.h"
#include "AkSrcXMABase.h"


// Use at most 4 input buffers, submit at most 2 at the time.
#define AK_XMA_NUMSTREAMBUFFERS 4

struct AkXmaInputBuffer
{
	AkXmaInputBuffer()
		: m_pBuffer( NULL )
		, m_uDataSize( 0 )
		, m_uExpectedSize( 0 )
		, m_uXmaBuffer( 0 )
		, m_bOwnsMemory( false )
	{}

	// Clear/Init

	// State
	inline bool HasData() const { return (m_pBuffer != nullptr); }
	inline bool IsSubmitted() const { return (m_pBuffer != nullptr && m_uXmaBuffer != 0); }
	inline bool IsReady() const { AKASSERT((m_uExpectedSize == 0) || (m_uDataSize < m_uExpectedSize) || (m_uDataSize % m_uExpectedSize == 0)); return (m_uExpectedSize && m_uDataSize >= m_uExpectedSize); }
	inline bool ReferencesStmBuffer() { return HasData() && !m_bOwnsMemory; }

	inline AkUInt8 *	Data() const { return m_pBuffer; }
	inline AkUInt32		Size() const { return m_uDataSize; }

	inline AkUInt8 XmaBufferMask() const { return m_uXmaBuffer; }
	inline void SetXmaBuffer0() { AKASSERT(m_uXmaBuffer == 0); m_uXmaBuffer = 0x1; }
	inline void SetXmaBuffer1() { AKASSERT(m_uXmaBuffer == 0); m_uXmaBuffer = 0x2; }

	// Free (if out-of-place) and clear this input buffer.
	inline void Release()
	{
		AKASSERT(HasData());
		if ( m_bOwnsMemory )
		{
			AkACPMgr::Get().ReleaseApuBuffer(m_pBuffer);
		}
		m_pBuffer = nullptr;
		m_uDataSize = 0;
		m_uExpectedSize = 0;
		m_uXmaBuffer = 0;
		m_bOwnsMemory = false;
	}

	// Assign data to this input buffer. It uses the data in-place.
	// The size used is computed internally and returned. Since data is used in-place, the data size needs to be greater or equal to the required size. 
	inline AkUInt32 Assign(AkUInt8* in_pData, AkUInt32 in_uDataSize, AkUInt32 in_uRequiredSize)
	{
		// We have enough data to submit.
		AkUInt32 uSizeAssigned = in_uDataSize - ( in_uDataSize % in_uRequiredSize );

		AKASSERT( in_pData != NULL 
			&& ( (AkUIntPtr)in_pData & SHAPE_XMA_INPUT_BUFFER_MASK ) == 0 
			&& uSizeAssigned > 0 
			&& ( uSizeAssigned & SHAPE_XMA_INPUT_BUFFER_SIZE_MASK ) == 0 );

		AKASSERT( !HasData() && XmaBufferMask() == 0 );

		m_pBuffer = in_pData;
		m_uDataSize = uSizeAssigned;
		m_uExpectedSize = in_uRequiredSize;
		m_bOwnsMemory = false;

		return uSizeAssigned;
	}

	// Copy data over to this input buffer. Memory is allocated if needed, and data is copied at the current location.
	// Stitch buffers are optimally small, thus memory is allocated with size in_uRequiredSize, in_uRequiredSize must be consistent across calls to this function, or be 0 if
	// don't care (memory must already have been allocated), and the total data size that will be stitched will match the required size. 
	// The function returns the number of bytes that were copied over during this stitching operation. 0 is a fatal error (means allocation failed).
	inline AkUInt32 Stitch(AkUInt8* in_pData, AkUInt32 in_uDataSize, AkUInt32 in_uRequiredSize = 0)
	{
		AKASSERT( in_pData != NULL 
			&& ( (AkUIntPtr)in_pData & SHAPE_XMA_INPUT_BUFFER_MASK ) == 0 
			&& in_uDataSize > 0 
			&& ( in_uDataSize & SHAPE_XMA_INPUT_BUFFER_SIZE_MASK ) == 0 );

		if ( !m_pBuffer )
		{
			AKASSERT( in_uRequiredSize > 0 && in_uDataSize <= in_uRequiredSize  );			
			void *pBuffer = AkACPMgr::Get().GetApuBuffer(in_uRequiredSize, SHAPE_XMA_INPUT_BUFFER_ALIGNMENT);
			if ( !pBuffer )
				return 0;
			m_pBuffer = (AkUInt8*)pBuffer;
			m_uExpectedSize = in_uRequiredSize;
			m_bOwnsMemory = true;
		}
		else
		{
			AKASSERT( m_pBuffer && m_bOwnsMemory );
			AKASSERT( in_uRequiredSize == 0 
					|| ( m_uExpectedSize == in_uRequiredSize 
						&& m_uDataSize + in_uDataSize <= in_uRequiredSize ) );
		}

		AkUInt32 uSizeToCopy = m_uExpectedSize - m_uDataSize;
		if ( in_uDataSize < uSizeToCopy )
			uSizeToCopy = in_uDataSize;
		memcpy( m_pBuffer + m_uDataSize, in_pData, uSizeToCopy );
		m_uDataSize += uSizeToCopy;
		return uSizeToCopy;
	}
	
private:

	AkUInt8*	m_pBuffer;
	AkUInt32	m_uDataSize;
	AkUInt32	m_uExpectedSize :28;	// Minimum size to have in order to be ready. Size must be an integer multiple of this value. Typically, 2KB for mono/stereo, BlockSize for multichannel, anything for last block.
	AkUInt32	m_uXmaBuffer	:2;		// XMA buffer this is submitted to. Matches SHAPE_XMA_CONTEXT::validBuffer (but cannot be 0x3).
	AkUInt32	m_bOwnsMemory	:1;
};

class CAkSrcFileXMA 
	: public CAkSrcXMABase<CAkSrcFileBase>
	, public IAkSHAPEAware
{
public:

	//Constructor and destructor
	CAkSrcFileXMA( CAkPBI * in_pCtx );
	virtual ~CAkSrcFileXMA();
	
	// Data access.
	virtual AKRESULT StartStream( AkUInt8 * pvBuffer, AkUInt32 ulBufferSize );
	virtual void	 StopStream();
	virtual void	 GetBuffer( AkVPLState & io_state );
    virtual void     VirtualOn( AkVirtualQueueBehavior eBehavior );
    virtual AKRESULT VirtualOff( AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset );
	virtual AKRESULT ChangeSourcePosition();

	// IO/buffering status notifications. 
	// In-memory XMA may starve because of the XMA decoder. 
	virtual void	 NotifySourceStarvation();

	virtual AKRESULT ParseHeader( AkUInt8 * in_pBuffer );

protected:
	// IAkSHAPEAware interface.
	virtual void UpdateInput(AkUInt32 in_uStreamIdx);
	virtual bool OnFlowgraphPrepare( AkUInt32 in_uShapeFrameIdx, AkUInt32 in_uStreamIndex ) { return CAkSrcXMABase<CAkSrcFileBase>::OnFlowgraphPrepare(in_uShapeFrameIdx,in_uStreamIndex); }
	virtual bool NeedsUpdate() { return true; }
	virtual void HwVoiceFinished();
	virtual AkUInt32 GetNumStreams() const { return CAkSrcXMABase<CAkSrcFileBase>::GetNumStreams(); }
	virtual AkACPVoice* GetAcpVoice(AkUInt32 in_uIdx) { return CAkSrcXMABase<CAkSrcFileBase>::GetAcpVoice(in_uIdx); }
	virtual void SetFatalError() 
	{ 
		m_bReadyForDecoding = false;
		CAkSrcXMABase<CAkSrcFileBase>::SetFatalError();
	};

	// Helpers.
	bool IsDataNeeded();
	AKRESULT AddBuffer();
	void ReleaseStreamBuffer(unsigned int index);
	
	// Overriden from CAkSrcFileBase.
	// -------------------------------------------

	// Finds the closest offset in file that corresponds to the desired sample position.
	// Returns the file offset (in bytes, relative to the beginning of the file) and its associated sample position.
	// Returns AK_Fail if the codec is unable to seek.
	virtual AKRESULT FindClosestFileOffset( 
							AkUInt32 in_uDesiredSample,		// Desired sample position in file.
							AkUInt32 & out_uSeekedSample,	// Returned sample where file position was set.
							AkUInt32 & out_m_uFileOffset		// Returned file offset where file position was set.
							);
	
private:

	AKRESULT		CreateVoices();
	// Initialize XMA context if it does not exist, and places the hardware position to the desired 
	// source offset (PBI) if specified, otherwise it places it to the current PCM offset.
	AKRESULT        StartXMA();
	
	AKRESULT		SetXMAOffsets( AkUInt32 in_uSampleOffset, AkUInt32 in_uSkipSamples, 
									AkUInt32 in_uBufferSize, DWORD * in_pSeekTable, 
									AkUInt8* pData, AkUInt32 in_uNbStreams, 
									AkUInt32 * out_dwBitOffset, AkUInt32 & out_dwSubframesSkip);
	
	DWORD 			RoundToBlock( AkUInt32 in_uSampleOffset, DWORD * in_pSeekTable, AkUInt32& out_blockIndex );
	void			StopDecoding();
	AKRESULT 		StartDecoding();

	void			SubmitXMABuffer();

	inline AkUInt32	GetNextXmaSlotAndUpdate()
	{
		AKASSERT( GetXmaInputMask() != 0x3 );
		AkUInt8 uNextXmaSlot = m_uNextXmaSlot;
		m_uNextXmaSlot = m_uNextXmaSlot ^ 1;
		return uNextXmaSlot;
	}
	inline AkUInt32	GetXmaInputMask()
	{
		AkUInt32 uXmaBufferMask = 0;
		for (unsigned int i = 0; i < AK_XMA_NUMSTREAMBUFFERS; i++)
		{
			uXmaBufferMask = uXmaBufferMask | m_arXmaInputBuffers[i].XmaBufferMask();
		}
		return uXmaBufferMask;
	}

	inline void GetXmaInputBuffers(AkUInt32 in_uCurrentIndex, AkUInt32 in_uValidMask, AkXmaInputBuffer*& out_pCurrentBuffer, AkXmaInputBuffer*& out_pNextBuffer )
	{
		AkUInt32 uMaskCurrent = 0x1 << in_uCurrentIndex;
		AkUInt32 uMaskNext = 0x1 << (in_uCurrentIndex^1);
		for (unsigned int i = 0; i < AK_XMA_NUMSTREAMBUFFERS; i++)
		{
			if ( m_arXmaInputBuffers[i].XmaBufferMask() & uMaskCurrent & in_uValidMask)
				out_pCurrentBuffer = &m_arXmaInputBuffers[i];
			else if ( m_arXmaInputBuffers[i].XmaBufferMask() & uMaskNext & in_uValidMask)
				out_pNextBuffer = &m_arXmaInputBuffers[i];
		}
	}

	inline bool _IsLastBlockInFile() 
	{ 
		AkUInt32 uLastBlockPosition = m_uDataOffset + (m_XMA2WaveFormat.BlockCount-1) * m_XMA2WaveFormat.BytesPerBlock;
		return ( m_uCurFileOffset >= uLastBlockPosition );
	}
	inline AkUInt32 _LastBlockSize() 
	{ 
		return m_uDataSize - (m_XMA2WaveFormat.BlockCount-1) * m_XMA2WaveFormat.BytesPerBlock;
	}
	inline AkUInt32 ComputeRequiredSize()
	{
		if ( m_uNumXmaStreams > 1 )
		{
			if ( _IsLastBlockInFile() )
				return _LastBlockSize() ;
			else
				return m_XMA2WaveFormat.BytesPerBlock;
		}
		return XMA_BYTES_PER_PACKET;
	}

	// Stream-specific members.
	AkXmaInputBuffer m_arXmaInputBuffers[AK_XMA_NUMSTREAMBUFFERS];
	AkXMA2WaveFormat m_XMA2WaveFormat;
	AkUInt32 *		m_arSeekTable;
	
	AkUInt32		m_uRemainingSamplesAfterSeek;
	AkUInt8			m_uNextIdxToSubmit;		// 0,num_input-1
	AkUInt8			m_uStmBuffersRefCnt			:2;
	AkUInt8			m_uNextXmaSlot				:1;
	AkUInt8			m_bReadyForDecoding			:1;
	AkUInt8			m_bIsVirtual				:1;
};

#endif
