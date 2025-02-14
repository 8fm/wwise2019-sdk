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

#ifndef _AK_PLUGIN_CODEC_H_
#define _AK_PLUGIN_CODEC_H_

#include "AkVPLSrcNode.h"

/// Decoder interface for converting an entire file to PCM data
class IAkFileCodec
{
public:
	virtual ~IAkFileCodec() {}

	/// Decode an entire encoded file (including headers) into interleaved 16-bit PCM raw data.
	virtual AKRESULT DecodeFile(AkUInt8 * pDst, AkUInt32 uDstSize, AkUInt8 * pSrc, AkUInt32 uSrcSize, AkUInt32 &out_uSizeWritten) = 0;
};

/// Codec interface used by SoundSeed Grain plugin
class IAkGrainCodec
{
public:
	virtual ~IAkGrainCodec() {};

	virtual AKRESULT	Init( AkUInt8 * in_pBuffer, AkUInt32 in_pBufferSize ) = 0;

	virtual void		GetBuffer( AkAudioBuffer & io_state ) = 0;
	virtual void		ReleaseBuffer() = 0;

	virtual AKRESULT	SeekTo( AkUInt32 in_SourceOffset ) = 0;

	virtual void		Term() = 0;
};

// IAkSoftwareCodec needs to be kept because of forward defines in the SDK for codec registration and factories. 
class IAkSoftwareCodec : public CAkVPLSrcNode
{
public:
	IAkSoftwareCodec( CAkPBI * in_pCtx ) : CAkVPLSrcNode( in_pCtx ) {}
};

#endif //_AK_PLUGIN_CODEC_H_