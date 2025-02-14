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

#include "AkSink.h"

#ifdef AK_WASAPI

#if defined AK_XBOX
#include <audioclient.h>
#else
#include "AkResampler.h"
#endif
#include <mmdeviceapi.h>



// REFERENCE_TIME time units per millisecond
#define MFTIMES_PER_MILLISEC  10000

class CAkSinkWasapi 
	: public CAkSink
#if defined AK_XBOX && !defined AK_GAMINGXBOX
	, IXboxVolumeNotificationClient
#endif	//AK_XBOXONE
{
public:
	CAkSinkWasapi();
	~CAkSinkWasapi();

	AKRESULT Init(interface IMMDevice * in_pEnumerator, const AkOutputSettings &in_settings, AkPluginID in_idPlugin, bool in_bRecordable);

	// CAkSink overrides
	virtual AKRESULT Reset();
	virtual AKRESULT Term( AK::IAkPluginMemAlloc * in_pAllocator );

	virtual bool IsStarved();
	virtual void ResetStarved();
	virtual AKRESULT IsDataNeeded( AkUInt32 & out_uBuffersNeeded );
	virtual void PassData();
	virtual void PassSilence();
	virtual void Consume(AkAudioBuffer * in_pInputBuffer, AkRamp in_gain);		
	virtual void OnFrameEnd();
	
	virtual AkAudioAPI GetType(){ return AkAPI_Wasapi; }
	
#if defined AK_XBOX && !defined AK_GAMINGXBOX
	virtual HRESULT QueryInterface (REFIID riid, void **ppvObject);
	virtual ULONG AddRef();
	virtual ULONG Release();
	virtual void OnGameMediaMuted();
	virtual void OnGameMediaUnmuted();
#endif	//AK_XBOXONE

	void Share() { m_cShareCount++; }

private:
	AKRESULT InitResampler(AkUInt32	in_uDeviceSampleRate, AkChannelConfig in_DeviceConfig);
	void SubmitBuffer(AkPipelineBufferBase &in_buffer);
	void GetAudioSystemConfig(AkChannelConfig in_GameSpecifiedConfig, AkChannelConfig in_EndpointConfig, AkPluginID in_eType, WAVEFORMATEXTENSIBLE& out_format);

	interface IMMDevice * m_pDeviceOut;
	interface IAudioClient * m_pClientOut;
	interface IAudioRenderClient * m_pRenderClient;
	HANDLE m_hEvent;

	AkUInt32 m_uBufferFrames;	// Number of audio frames in output buffer.
	AkUInt32 m_uNumChannels;
	AkUInt32 m_cShareCount;
	AkUInt32 m_cSharedProcessingDone;
	AkUInt32 m_cRef;

#if !defined AK_XBOX
	CAkResampler	m_Resampler;
	AkPipelineBufferBase	m_ResampleBuffer;
#endif	//!AK_XBOXONE
};

#endif	// defined AK_WASAPI
