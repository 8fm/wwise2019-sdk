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

#include <AK/SoundEngine/Common/AkSoundEngineExport.h>
#include "AkPluginCodec.h"
#include "AkSrcMediaStream.h"
#include "AkMarkers.h"
#include "AkSrcMediaHelpers.h"

/// Decodes a media stream into discrete PCM buffers
class IAkSrcMediaCodec
{
public:
	struct Result
	{
		Result() : eResult(), eMonitorError(AK::Monitor::ErrorCode_NoError) {}
		Result(AKRESULT r) : eResult(r), eMonitorError(AK::Monitor::ErrorCode_NoError) {}
		Result(AKRESULT r, AK::Monitor::ErrorCode m) : eResult(r), eMonitorError(m) {}

		inline operator AKRESULT() { return eResult; }

		AKRESULT eResult;
		AK::Monitor::ErrorCode eMonitorError;
	};
	struct PitchInfo
	{
		AkReal32 fPitch;
		AkPitchShiftType eType;
	};
	struct SeekInfo
	{
		AkUInt32 uPCMOffset;      // PCM offset of the next buffer returned by the decoder after seek
		AkUInt32 uSkipLength;     // How many frames codec must skip to accomplish seek (codec is responsible for the skip)

		AkUInt32 uFileOffset;     // Where to position the stream for seeking (from data offset; header size is not taken into account)
	};
	struct BufferInfo
	{
		AkAudioBuffer Buffer;
		AkUInt16 uSrcFrames;      // Source frames (prior to any resampling)
	};

	virtual ~IAkSrcMediaCodec() {}

	/// Initialize a new codec based on pre-parsed media header. Codec must fill out the CodecInfo struct entirely.
	virtual Result Init(const AK::SrcMedia::Header &in_header, AK::SrcMedia::CodecInfo &out_codec, AkUInt16 uLoopCnt) = 0;

	/// Allow the codec to pre-fetch some more data from the stream before decoding starts. Must return one of: AK_Success, AK_Fail, or AK_FormatNotReady.
	virtual Result Warmup(AK::SrcMedia::Stream::State* pStream) = 0;

	/// Release all resources acquired during Init()
	virtual void Term() = 0;

	/// For asynchronous codecs, decoding of the next buffer starts here.
	virtual Result PrepareNextBuffer(AK::SrcMedia::Stream::State* pStream, const AK::SrcMedia::Position::State &position, const PitchInfo &pitch) = 0;

	/// Produce a decoded buffer. Must return one of: AK_DataReady, AK_NoMoreData, AK_NoDataReady, or AK_Fail
	virtual Result GetBuffer(AK::SrcMedia::Stream::State* pStream, AkUInt16 uMaxFrames, BufferInfo &out_buffer) = 0;

	/// Release a buffer previously returned by GetBuffer
	virtual void ReleaseBuffer(AK::SrcMedia::Stream::State* pStream) = 0;

	/// Voice is going virtual; codec should release as many resources as possible
	virtual void VirtualOn() = 0;

	/// Voice is coming back from virtual; codec should re-acquire necessary resources. Expect a Seek() call to follow
	virtual Result VirtualOff() = 0;

	/// Resolve the file location corresponding to the closest seekable sample to uDesiredSample.
	virtual Result FindClosestFileOffset(AkUInt32 in_uDesiredSample, SeekInfo &out_SeekInfo) = 0;

	/// Seek to a seekable point in the file. When this is called, the media stream has already been seeked.
	virtual Result Seek(AK::SrcMedia::Stream::State* pStream, const SeekInfo &seek, AkUInt32 uNumFramesInRange, AkUInt16 uLoopCnt) = 0;

	/// Voice has stopped looping; codec must update state with latest loop count/decode range info from voice
	virtual void StopLooping(const AK::SrcMedia::Position::State &position) = 0;
};

AK_CALLBACK( IAkSrcMediaCodec*, AkCreateSrcMediaCodecCallback )( const AK::SrcMedia::Header * in_pHeader );

/// A source node that reads from a media file produced by Wwise
class CAkSrcMedia final : public IAkSoftwareCodec
{
public:
	CAkSrcMedia(CAkPBI * pCtx, AkCreateSrcMediaCodecCallback pCodecFactory);

	virtual AKRESULT StartStream(AkUInt8 * in_pBuffer, AkUInt32 ulBufferSize) override;
	virtual void StopStream() override;

	virtual void GetBuffer(AkVPLState & io_state) override;
	virtual void ReleaseBuffer() override;
	virtual void PrepareNextBuffer() override;

	virtual AKRESULT TimeSkip( AkUInt32 & io_uFrames ) override;

	virtual void VirtualOn(AkVirtualQueueBehavior eBehavior) override;
	virtual AKRESULT VirtualOff(AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset) override;

	virtual AkReal32 GetDuration() const override;
	virtual AKRESULT StopLooping() override;

	virtual AKRESULT Seek() override;
	virtual AKRESULT ChangeSourcePosition() override;

	virtual AkReal32 GetPitch() const override;

	virtual AkReal32 GetAnalyzedEnvelope(AkUInt32 in_uBufferedFrames) override;

#ifndef AK_OPTIMIZED
	virtual void RecapPluginParamDelta() override {} // no-op (only useful for other implementations of CAkVPLSrcNode)
#endif

	virtual AkInt32 SrcProcessOrder() override
	{
		switch (m_uCodecAttributes & (AK_CODEC_ATTR_HARDWARE | AK_CODEC_ATTR_HARDWARE_LL))
		{
			case AK_CODEC_ATTR_HARDWARE | AK_CODEC_ATTR_HARDWARE_LL:
				return SrcProcessOrder_HwVoiceLowLatency;
			case AK_CODEC_ATTR_HARDWARE:
				return SrcProcessOrder_HwVoice;
			default:
				return SrcProcessOrder_SwVoice;
		}
	}
	virtual bool SupportResampling() const override
	{
		// To bypass the pitch node, the codec must be able to do these two things
		return (m_uCodecAttributes & (AK_CODEC_ATTR_PITCH | AK_CODEC_ATTR_SA_TRANS)) == (AK_CODEC_ATTR_PITCH | AK_CODEC_ATTR_SA_TRANS);
	}
	virtual bool SupportMediaRelocation() const override
	{
		return true;
	}
	virtual bool MustRelocatePitchInputBufferOnMediaRelocation() override
	{
		return (m_uCodecAttributes & AK_CODEC_ATTR_RELOCATE_PITCH) != 0;
	}
	virtual bool MustRelocateAnalysisDataOnMediaRelocation() override
	{
		return m_pAnalysisData != nullptr && m_bHeaderFromBank;
	}

	virtual AKRESULT RelocateMedia(AkUInt8* in_pNewMedia, AkUInt8* in_pOldMedia) override;

	virtual AkChannelMappingFunc GetChannelMappingFunc() override;

private:
	AKRESULT ParseHeader(AkUInt8 * in_pBuffer, AkUInt32 in_uBufferSize, AK::SrcMedia::Header &out_header);
	AKRESULT InitCodec(AkUInt8 * in_pBuffer, AkUInt32 in_uBufferSize, AK::SrcMedia::Header &io_header);

	void SaveMarkersForBuffer(AkPipelineBuffer & io_buffer, AkUInt32 in_ulBufferStartPos, AkUInt32 in_ulNumFrames);
	void UpdatePositionInfo(AkUInt32 uCurrSample, AkReal32 fLastRate);

	void GetSourceOffset(AkUInt32 &out_uPosition, AkUInt16 &out_uLoopCnt) const;
	AKRESULT SeekTo(AkUInt32 uSeekOffset, AkUInt16 uLoopCnt);

	AKRESULT PrepareCodecNextBuffer();

	void MonitorCodecError(AKRESULT eResult) = delete; // This is to catch mistakenly passing a pure AKRESULT (which would get coerced to Result) at compile-time
	inline void MonitorCodecError(const IAkSrcMediaCodec::Result &result)
	{
		if (result.eMonitorError != AK::Monitor::ErrorCode_NoError)
		{
			MONITOR_SOURCE_ERROR(result.eMonitorError, m_pCtx);
		}
	}

	AK::SrcMedia::Position::State         m_Position;
	AK::SrcMedia::Stream::State           m_StreamState;
	AK::SrcMedia::EnvelopeAnalyzer::State m_Envelope;
	CAkMarkers                            m_Markers;

	AkCreateSrcMediaCodecCallback m_pCodecFactory;
	IAkSrcMediaCodec * m_pCodec;

	AkUInt32 m_uDataOffset;
	AkUInt32 m_uSourceSampleRate;

	AkUInt16 m_uCodecAttributes;

	AK::SrcMedia::ChannelOrdering m_eOrdering :AK_SRCMEDIA_CHANNEL_ORDERING_NUM_STORAGE_BIT;
	bool m_bHeaderFromBank :1;
	bool m_bCodecVirtual   :1;
	bool m_bVoiceVirtual   :1;
};
