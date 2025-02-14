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

#ifndef _AK_SRC_FILE_VORBIS_H_
#define _AK_SRC_FILE_VORBIS_H_

#include "AkSrcFileBase.h"
#include "AkVorbisCodec.h"

class CAkSrcFileVorbis : public CAkSrcFileBase
{
public:

	//Constructor and destructor
    CAkSrcFileVorbis( CAkPBI * in_pCtx );
	virtual ~CAkSrcFileVorbis();

	// Data access.
	virtual void		GetBuffer( AkVPLState & io_state );
	virtual void		ReleaseBuffer();
	virtual AKRESULT	StartStream( AkUInt8 * in_pBuffer, AkUInt32 ulBufferSize );// Overide default implementation
	virtual void		StopStream( ); // Overide default implementation

	// Use inherited default implementation
	virtual AKRESULT	StopLooping();
	virtual void		VirtualOn( AkVirtualQueueBehavior eBehavior );
	virtual AKRESULT	VirtualOff( AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset );

	virtual AKRESULT	ChangeSourcePosition();
	
protected:

	virtual AKRESULT ParseHeader(				// Parse header information
		AkUInt8 * in_pBuffer	// Buffer to parse
		);

	// Finds the closest offset in file that corresponds to the desired sample position.
	// Returns the file offset (in bytes, relative to the beginning of the file) and its associated sample position.
	// Returns AK_Fail if the codec is unable to seek.
	virtual AKRESULT FindClosestFileOffset( 
		AkUInt32 in_uDesiredSample,		// Desired sample position in file.
		AkUInt32 & out_uSeekedSample,	// Returned sample where file position was set.
		AkUInt32 & out_uFileOffset		// Returned file offset where file position was set.
		);

	// Override OnLoopComplete() handler: "restart DSP" (set primimg frames) and fix decoder status 
	// if it's a loop end.
	virtual AKRESULT OnLoopComplete(
		bool in_bEndOfFile		// True if this was the end of file, false otherwise.
		);

private:

	AKRESULT GetNextPacket();

	AKRESULT ProcessFirstBuffer();		// Use this implementation instead of default.
	AKRESULT DecodeVorbisHeader();		// Decode headers and seek table
	AKRESULT InitVorbisInfo();
	void InitVorbisState();
	void TermVorbisState();
	void LoopInit();
	
	void FreeStitchBuffer();
	void FreeOutputBuffer();

private:

	// Override SubmitBufferAndUpdate(): Check decoder status, post error message if applicable
	// and restart DSP if loop is resolved.
	void SubmitBufferAndUpdateVorbis( AkVPLState & io_state );
	inline bool	HasSeekTable(){ return m_VorbisState.pSeekTable != NULL; }
	inline void VorbisDSPRestart( AkUInt32 in_uSrcOffsetRemainder )
	{
		// Re-Initialize global decoder state
		vorbis_dsp_restart( 
			&m_VorbisState.TremorInfo.VorbisDSPState, 
			in_uSrcOffsetRemainder, 
			DoLoop() ? m_VorbisState.VorbisInfo.LoopInfo.uLoopEndExtra : m_VorbisState.VorbisInfo.uLastGranuleExtra );

		m_VorbisState.TremorInfo.ReturnInfo.eDecoderState = PACKET_STREAM;
	}

	inline AkUInt16 CurrentPacketSize()
	{
		return ((OggPacketHeader*)m_pOggPacketData)->uPacketSize;
	}

	// Shared between file and bank sources
	AkVorbisSourceState	m_VorbisState;			// Shared source information with vorbis file

	// mechanism for accumulating complete packet accross stream buffer for vorbis header setup
	AkUInt8 * 		m_pOggPacketData;			// Current Ogg Packet data under construction	
	AkUInt32 		m_uPacketDataGathered;		// Amount of Packet data gathered
	AkUInt32 		m_uPacketHeaderGathered;	// Amount of Packet header data gathered
	bool			m_bStitching;

	AkReal32*		m_pOutputBuffer;
	AkUInt32		m_uOutputBufferSize;
};

class CAkVorbisFileCodec : public IAkFileCodec
{
	virtual AKRESULT DecodeFile(AkUInt8 * pDst, AkUInt32 uDstSize, AkUInt8 * pSrc, AkUInt32 uSrcSize, AkUInt32 &out_uSizeWritten) override;
};

#endif // _AK_SRC_FILE_VORBIS_H_
