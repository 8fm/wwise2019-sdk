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

#pragma once

#include "AkSrcMediaHelpers.h"
#include "AkSrcMedia.h"

class CAkSrcMediaCodecPCM : public IAkSrcMediaCodec
{
public:
	CAkSrcMediaCodecPCM();

	virtual Result Init(const AK::SrcMedia::Header &in_header, AK::SrcMedia::CodecInfo &out_codec, AkUInt16 uLoopCnt) override;
	virtual void Term() override;

	virtual Result Warmup(AK::SrcMedia::Stream::State* pStream) override { return AK_Success; } // no-op

	virtual Result PrepareNextBuffer(AK::SrcMedia::Stream::State* pStream, const AK::SrcMedia::Position::State &position, const PitchInfo &pitch) override { return AK_Success; } // no-op
	virtual Result GetBuffer(AK::SrcMedia::Stream::State* pStream, AkUInt16 uMaxFrames, BufferInfo &out_buffer) override;
	virtual void ReleaseBuffer(AK::SrcMedia::Stream::State* pStream) override;

	virtual void StopLooping(const AK::SrcMedia::Position::State &position) override {} // no-op
	virtual Result FindClosestFileOffset(AkUInt32 in_uDesiredSample, SeekInfo & out_SeekInfo) override;
	virtual Result Seek(AK::SrcMedia::Stream::State* pStream, const SeekInfo & seek, AkUInt32 uNumFrames, AkUInt16 uLoopCnt) override;

	virtual void VirtualOn() override;
	virtual Result VirtualOff() override { return AK_Success; } // no-op

private:

	AkUInt8	*        m_pStitchBuffer;
	AkUInt32         m_uNumBytesBuffered;	// Number of bytes temporarily stored in stitched sample frame buffer.
	
	AkUInt32         m_uSizeToRelease;		// Size of returned streamed buffer (in bytes).

	AkUInt16         m_uBlockAlign;         // Sample size in bytes
};
