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

//////////////////////////////////////////////////////////////////////
//
// AkSink.h
//
// Platform dependent part
//
//////////////////////////////////////////////////////////////////////
#pragma once

#include "../common/AkSinkBase.h" //RED
#include <AK/SoundEngine/Common/AkSoundEngine.h>

class AkAudioBuffer;

#ifdef AK_WIN
#define AK_XAUDIO2
#endif

#ifndef AK_USE_UWP_API	
	#if defined AK_WIN && defined WINAPI_FAMILY != WINAPI_FAMILY_GAMES
		#define AK_DIRECTSOUND
	#endif

	#define AK_WASAPI
#endif

class CAkSink : public CAkSinkBase
{
public:
	CAkSink();

	virtual ~CAkSink() {}

	static CAkSink * Create(AkOutputSettings & in_settings, AkUInt32 in_uInstance);

	/// REVIEW Move to base:
	inline AkChannelConfig GetSpeakerConfig() { return m_SpeakersConfig; }

	virtual AkAudioBuffer &NextWriteBuffer()
	{
		m_MasterOut.uValidFrames = 0;
		return m_MasterOut;
	}

	virtual AkAudioAPI GetType() = 0;

protected:

	AKRESULT AllocBuffer(AkChannelConfig in_channelConfig);

	/// REVIEW Move to base:
	AkChannelConfig m_SpeakersConfig;
	AkPipelineBufferBase m_MasterOut;
};

class CAkDefaultSink : public AK::IAkSinkPlugin	
{
public:
	CAkDefaultSink();
	~CAkDefaultSink();

	//
	// AK::IAkSinkPlugin implementation.
	//
	virtual AKRESULT Init(
		AK::IAkPluginMemAlloc *	in_pAllocator,				// Interface to memory allocator to be used by the effect.
		AK::IAkSinkPluginContext *	in_pSinkPluginContext,	// Interface to sink plug-in's context.
		AK::IAkPluginParam *		in_pParams,				// Interface to plug-in parameters.
		AkAudioFormat &			in_rFormat					// Audio data format of the input signal. 
		);
	virtual AKRESULT Term(
		AK::IAkPluginMemAlloc * in_pAllocator 	// UNUSED interface to memory allocator to be used by the plug-in
		);
	virtual AKRESULT Reset();
	virtual AKRESULT GetPluginInfo(
		AkPluginInfo & out_rPluginInfo	// Reference to the plug-in information structure to be retrieved
		);
	virtual void Consume(
		AkAudioBuffer *			in_pInputBuffer,		///< Input audio buffer data structure. Plugins should avoid processing data in-place.
		AkRamp					in_gain					///< Volume gain to apply to this input (prev corresponds to the beginning, next corresponds to the end of the buffer).
		);
	virtual void OnFrameEnd();
	virtual bool IsStarved();
	virtual void ResetStarved();
	virtual AKRESULT IsDataNeeded(AkUInt32 & out_uBuffersNeeded);
	
	//Keep the currently used API so all sinks are of the same type.
	static AkAudioAPI s_CurrentAudioAPI;

	CAkSink *RealSink() const { return m_pRealSink; };

private:
	static CAkSink * TryDirectSound(const AkOutputSettings &in_settings);
	static AKRESULT TryXAudio2(const AkOutputSettings &in_settings, AkPluginID in_uPlugin, AkUInt32 in_uInstance, bool in_bRecordable, CAkSink*& out_pSink);
	static CAkSink * TryWasapi(const AkOutputSettings &in_settings, AkPluginID in_uPlugin, AkUInt32 in_uInstance, bool in_bRecordable);

	CAkSink *m_pRealSink;	
};
