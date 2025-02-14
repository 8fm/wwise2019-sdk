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

#include "AkSrcFileBase.h"
#include "opusfile.h"

class HWWorkContext;

class CAkSrcFileOpus : public CAkSrcFileBase
{
public:

	//Constructor and destructor
	CAkSrcFileOpus(CAkPBI * in_pCtx);
	virtual ~CAkSrcFileOpus();

	// Data access.
	virtual void		GetBuffer(AkVPLState & io_state);
	virtual void		ReleaseBuffer();
	virtual AKRESULT	StartStream(AkUInt8 * in_pBuffer, AkUInt32 ulBufferSize);
	virtual void		StopStream(); 

	// Virtual voices.
	virtual void     VirtualOn(AkVirtualQueueBehavior eBehavior);
	virtual AKRESULT VirtualOff(AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset);

	virtual AKRESULT	ChangeSourcePosition();

protected:

	// Update the source after fetching a streaming buffer.
	virtual AKRESULT ProcessStreamBuffer(
		AkUInt8 *	in_pBuffer,					// Stream buffer to interpret
		bool		in_bIsReadingPrefecth = false
		);


	virtual AKRESULT ParseHeader(				// Parse header information
		AkUInt8 * in_pBuffer	// Buffer to parse
		);	

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

	virtual AKRESULT SeekStream(
		AkUInt32 in_uDesiredSample,		// Desired sample position in file.
		AkUInt32 & out_uSeekedSample	// Returned sample where file position was set.
		);
	
	AKRESULT _ProcessFirstBuffer();
	void FreeOutputBuffer();

	int Read(unsigned char *_ptr, int _nbytes);
	int SeekOpus(opus_int64 _offset, int _whence);
	AKRESULT SeekHelper();
	
	//File access overrides for Opus File lib.
	static int read_opus (void *_stream, unsigned char *_ptr, int _nbytes);
	static int seek_opus(void *_stream, opus_int64 _offset, int _whence);
	static opus_int64 tell_opus(void *_stream);
	static int close_opus(void *_stream);

	static const OpusFileCallbacks c_OpusFileCB;

private:

	OggOpusFile *	m_pOpusFile;
	AkUInt32 m_uSeekedSample; 
	AkUInt32 m_uOpusFileStart;
	bool m_bVirtual;

	AkReal32*		m_pOutputBuffer;
	AkUInt32		m_uOutputBufferSize;

#ifdef XAPU_WORKS
	HWWorkContext *m_pHWContext;
#endif
};

class CAkOpusFileCodec : public IAkFileCodec
{
	virtual AKRESULT DecodeFile(AkUInt8 * pDst, AkUInt32 uDstSize, AkUInt8 * pSrc, AkUInt32 uSrcSize, AkUInt32 &out_uSizeWritten) override;
};
