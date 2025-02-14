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

#ifndef _AK_SRC_BANK_VORBIS_H_
#define _AK_SRC_BANK_VORBIS_H_

#include "AkSrcBase.h"
#include "AkSrcVorbis.h"
#include "AkVorbisCodec.h"
#include "AkPluginCodec.h"

class CAkSrcBankVorbis : public CAkSrcBaseEx
{
public:

	//Constructor and destructor
    CAkSrcBankVorbis( CAkPBI * in_pCtx );
	virtual ~CAkSrcBankVorbis();

	// Data access.
	virtual void		GetBuffer( AkVPLState & io_state );
	virtual void		ReleaseBuffer();
	virtual AKRESULT	StartStream( AkUInt8 * in_pBuffer, AkUInt32 in_pBufferSize );// Overide default implementation
	virtual void		StopStream( ); // Overide default implementation

	virtual AKRESULT	StopLooping();
	virtual void		VirtualOn( AkVirtualQueueBehavior eBehavior );
	virtual AKRESULT	VirtualOff( AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset );

	virtual AKRESULT	ChangeSourcePosition();

	virtual bool SupportMediaRelocation() const;
	virtual AKRESULT RelocateMedia( AkUInt8* in_pNewMedia,  AkUInt8* in_pOldMedia );

	virtual AKRESULT Seek( AkUInt32 in_SourceOffset );

protected:

	// Override OnLoopComplete() handler: "restart DSP" (set primimg frames) and fix decoder status
	// if it's a loop end, and place input pointer.
	virtual AKRESULT OnLoopComplete(
		bool in_bEndOfFile		// True if this was the end of file, false otherwise.
		);

	AKRESULT DecodeVorbisHeader();							// Decode headers and seek table
	AKRESULT VirtualSeek( AkUInt32 & io_uSeekPosition );	// Seek to new position in virtual voice
	AKRESULT InitVorbisInfo();
	void InitVorbisState();
	void TermVorbisState();
	void LoopInit();
	AKRESULT SeekToNativeOffset();				// Seek to source offset obtained from PBI (number of samples at the pipeline's sample rate).
	AKRESULT SeekToNativeOffset( AkUInt32 in_SourceOffset, AkUInt32& out_SourceOffsetRemainder );
	AkUInt32 GetMaxInputDataSize();
	void FreeOutputBuffer();

private:
	inline bool	HasSeekTable(){ return m_VorbisState.pSeekTable != NULL; }
	inline void VorbisDSPRestart( AkUInt16 in_uSrcOffsetRemainder )
	{
		// Re-Initialize global decoder state
		vorbis_dsp_restart(
			&m_VorbisState.TremorInfo.VorbisDSPState,
			in_uSrcOffsetRemainder,
			DoLoop() ? m_VorbisState.VorbisInfo.LoopInfo.uLoopEndExtra : m_VorbisState.VorbisInfo.uLastGranuleExtra );

		m_VorbisState.TremorInfo.ReturnInfo.eDecoderState = PACKET_STREAM;
	}

	// Shared between file and bank sources
	AkVorbisSourceState	m_VorbisState;			// Shared source information with vorbis file

	// Bank specific
	AkUInt8*			m_pucData;				// current data pointer
	AkUInt8*			m_pucDataStart;			// start of audio data	

	AkReal32*			m_pOutputBuffer;
	AkUInt32			m_uOutputBufferSize;
};

class CAkVorbisGrainCodec : public IAkGrainCodec
{
public:
	CAkVorbisGrainCodec();

	virtual AKRESULT Init(AkUInt8 * in_pBuffer, AkUInt32 in_pBufferSize) override;

	virtual void GetBuffer(AkAudioBuffer & io_state) override;
	virtual void ReleaseBuffer() override;

	virtual AKRESULT SeekTo(AkUInt32 in_SourceOffset) override;

	virtual void Term() override;

private:
	CAkSrcBankVorbis m_Node;
};

#endif // _AK_SRC_BANK_VORBIS_H_
