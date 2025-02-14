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


#include "stdafx.h"
#include "AkSink.h"
#ifdef AK_WASAPI

#include <mmdeviceapi.h>
#include <audioclient.h>
#include "AkLEngine.h"
#include "AkOutputMgr.h"
#include "AkSinkWasapi.h"

#ifdef AK_GAMINGXBOX
#include "Xaudio2XBox.h"
#endif


extern CAkAudioMgr *g_pAudioMgr;
extern AkPlatformInitSettings g_PDSettings;

CAkSinkWasapi::CAkSinkWasapi()
: CAkSink()
, m_pDeviceOut(NULL)
, m_pClientOut(NULL)
, m_pRenderClient(NULL)
, m_uBufferFrames(0)
, m_uNumChannels(0)
, m_hEvent(NULL)
, m_cShareCount(1)
, m_cSharedProcessingDone(1)
, m_cRef(1)
{
}

CAkSinkWasapi::~CAkSinkWasapi()
{
	if( m_MasterOut.HasData() )
	{
		AkFalign( AkMemID_Processing, m_MasterOut.GetContiguousDeinterleavedData() );
		m_MasterOut.ClearData();
	}

	AKASSERT(m_cRef == 1);
}

#if defined AK_XBOXONE && !defined AK_GAMINGXBOX
HRESULT CAkSinkWasapi::QueryInterface ( REFIID riid, void **ppvObject)
{
	const IID IID_IXboxVolumeNotificationClient = __uuidof(IXboxVolumeNotificationClient);
	const IID IID_IUnknown = __uuidof(IUnknown);
	if ( riid == IID_IUnknown )
	{
		*ppvObject = static_cast<IUnknown *>( this );
		m_cRef++;
		return S_OK;
	}
	else if ( riid == IID_IXboxVolumeNotificationClient )
	{
		*ppvObject = static_cast<IXboxVolumeNotificationClient *>( this );
		m_cRef++;
		return S_OK;
	}

	*ppvObject = NULL;
	return E_NOINTERFACE;
}


ULONG CAkSinkWasapi::AddRef()
{
	m_cRef++;
	return m_cRef;
}

ULONG CAkSinkWasapi::Release()
{
	m_cRef--;
	AKASSERT(m_cRef > 0);	// The "first" reference is never released.
	return m_cRef;
}

void CAkSinkWasapi::OnGameMediaMuted()
{
	CAkBus::MuteBackgroundMusic();	
}

void CAkSinkWasapi::OnGameMediaUnmuted()
{
	CAkBus::UnmuteBackgroundMusic();	
}
#endif

AkChannelConfig GetAkConfigFromChannelCount(AkUInt32 in_dwChannelCount)
{
	AkChannelConfig conf;
	if (in_dwChannelCount == 8)
		conf.SetStandard( AK_SPEAKER_SETUP_7POINT1);
	else if (in_dwChannelCount == 6)
		conf.SetStandard( AK_SPEAKER_SETUP_5POINT1 );
	else if (in_dwChannelCount == 4)
		conf.SetStandard( AK_SPEAKER_SETUP_4 );
	else if (in_dwChannelCount == 2)
		conf.SetStandard( AK_SPEAKER_SETUP_STEREO );
	else if (in_dwChannelCount == 1)
		conf.SetStandard( AK_SPEAKER_SETUP_MONO );
	else 	
		conf.SetAnonymous(in_dwChannelCount);

	return conf;
}

#if defined AK_XBOXONE && !defined AK_GAMINGXBOX
void CAkSinkWasapi::GetAudioSystemConfig(AkChannelConfig in_GameSpecifiedConfig, AkChannelConfig in_EndpointConfig, AkPluginID in_idPlugin, WAVEFORMATEXTENSIBLE& out_format)
{	
	bool bUseDefault = !in_GameSpecifiedConfig.IsValid();
	if(bUseDefault)
	{	
		// Always use 7.1 for XBoxOne main output, the output device mix only in that format.  The output stage conversion is done in HW, according to the output settings of the user.
		// Detect how many channels are supported by the user's system (not the console, the connected systems).
		if (in_idPlugin == AKPLUGINID_DEFAULT_SINK || in_idPlugin == AKPLUGINID_DVR_SINK)
		{
			// Regardless of what the game asked for, the endpoint must always be 7.1 (detected)	
			// But the outside world should run in whatever configuration is active in the system audio menu.

			UINT channelCountHDMI = 8;	//Default to 7.1
			UINT channelCountSPDIF = 8; //Default to 7.1
			IMMXboxDeviceEnumerator* pXboxDeviceEnumerator;

			IMMDeviceEnumerator * pEnumerator = NULL;
			CoCreateInstance(
				__uuidof(MMDeviceEnumerator), NULL,
				CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
				(void**)&pEnumerator);

			if (pEnumerator != NULL)
			{	
				if(pEnumerator->QueryInterface(__uuidof(IMMXboxDeviceEnumerator), reinterpret_cast<void**>(&pXboxDeviceEnumerator)) == S_OK)
				{
					HRESULT hr = pXboxDeviceEnumerator->GetHdAudioChannelCounts(&channelCountHDMI, &channelCountSPDIF);
					if(hr != S_OK)
					{
						channelCountHDMI = 8;	//Default to 7.1
						channelCountSPDIF = 8; //Default to 7.1
					}

					pXboxDeviceEnumerator->Release();
				}
				pEnumerator->Release();
			}

			UINT channelsToUse = 0;
			if(in_GameSpecifiedConfig.IsValid())
			{
				// Game has asked for a specific configuration.
				channelsToUse = in_GameSpecifiedConfig.uNumChannels;
			}
			else			{
				// Game has left it up to us to detect the best speaker configuration. Pick the largest number of speakers
				// supported by the connected system.
				channelsToUse = AkMax(channelCountHDMI, channelCountSPDIF);
			}

			if(channelsToUse == 0)
				channelsToUse = 8;

			m_SpeakersConfig = GetAkConfigFromChannelCount(channelsToUse);
		}		
		else
			m_SpeakersConfig = in_EndpointConfig;
	}
	else
	{
		m_SpeakersConfig = in_GameSpecifiedConfig;
	}
	
	out_format.dwChannelMask = in_EndpointConfig.uChannelMask;
	out_format.Format.nChannels = (WORD)in_EndpointConfig.uNumChannels;
	out_format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	out_format.Format.nSamplesPerSec = AK_CORE_SAMPLERATE;
	out_format.Format.nBlockAlign = out_format.Format.nChannels * sizeof(AkReal32);
	out_format.Format.nAvgBytesPerSec = out_format.Format.nSamplesPerSec * out_format.Format.nBlockAlign;
	out_format.Format.wBitsPerSample = sizeof(AkReal32)* 8;
	out_format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX);

	out_format.Samples.wValidBitsPerSample = 32;
	out_format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
}
#endif


AKRESULT CAkSinkWasapi::Init(interface IMMDevice * in_pDevice, const AkOutputSettings &in_settings, AkPluginID in_idPlugin, bool in_bRecordable)
{
	AKASSERT(in_pDevice);

	HRESULT hr;

	m_pDeviceOut = in_pDevice;
	m_pDeviceOut->AddRef();

	hr = m_pDeviceOut->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_pClientOut);
	if (hr != S_OK)
		return AK_Fail;

	REFERENCE_TIME timeDefaultPeriod, timeMinPeriod;
	hr = m_pClientOut->GetDevicePeriod(&timeDefaultPeriod, &timeMinPeriod);

#ifndef AK_XBOXONE
	AkAudioLibSettings::SetAudioBufferSettings(AK_CORE_SAMPLERATE, g_settings.uNumSamplesPerFrame);
#endif

	// now make sure we are going to do at least double-buffering of the system's buffer size
	// and use at least the min period provided by the hardware.

	WAVEFORMATEX * pWfSupported = NULL;
	AkChannelConfig endPointSpeakersConfig;

	hr = m_pClientOut->GetMixFormat(&pWfSupported);
	if (hr != S_OK || pWfSupported == NULL)
		return AK_Fail;

	WAVEFORMATEXTENSIBLE wfExt = *(WAVEFORMATEXTENSIBLE*)pWfSupported;

	endPointSpeakersConfig = GetAkConfigFromChannelCount(((WAVEFORMATEXTENSIBLE*)pWfSupported)->Format.nChannels);
	::CoTaskMemFree(pWfSupported);
	pWfSupported = NULL;

#if defined AK_XBOXONE && !defined AK_GAMINGXBOX
	GetAudioSystemConfig(in_settings.channelConfig, endPointSpeakersConfig, in_idPlugin, wfExt);
	SetWasapiThreadAffinityMask(g_PDSettings.threadLEngine.dwAffinityMask);
#else
	bool bUseDefault = !in_settings.channelConfig.IsValid();
	if (bUseDefault)
		m_SpeakersConfig = endPointSpeakersConfig;
	else
		m_SpeakersConfig = in_settings.channelConfig;
#endif // else

	// TO BE FIXED: synchronization between devices is not handled.
	// Currently the main device drives the show and other devices are slaved.  Clock drift might cause issues, especially with external HW devices (USB audio).
	AkUInt32 flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

	m_uNumChannels = endPointSpeakersConfig.uNumChannels;
	IAudioClient2 *pAudioClient2 = NULL;
	hr = m_pClientOut->QueryInterface(__uuidof(IAudioClient2), (void**)&pAudioClient2);
	if (hr == S_OK && pAudioClient2 != NULL) // IAudioClient2 not available in Wine
	{
		AudioClientProperties props;
		props.eCategory = AudioCategory_GameEffects;

		if (in_idPlugin == AKPLUGINID_COMMUNICATION_SINK || in_idPlugin == AKPLUGINID_PERSONAL_SINK)
			props.eCategory = AudioCategory_Communications;

		props.bIsOffload = false;
		pAudioClient2->IsOffloadCapable(props.eCategory, &props.bIsOffload);
		AkUInt32 minSamples = ((AkUInt32)timeMinPeriod * AK_CORE_SAMPLERATE) / (MFTIMES_PER_MILLISEC * 1000);
		AKASSERT(minSamples < (AkUInt32)AK_NUM_VOICE_REFILL_FRAMES * 2);
		props.cbSize = sizeof(AudioClientProperties);

#if defined AK_GAMINGXBOX
		props.Options = AUDCLNT_STREAMOPTIONS_NONE; //AUDCLNT_STREAMOPTIONS_RAW; ???
		if (!in_bRecordable)
			flags |= AUDCLNT_STREAMFLAGS_EXCLUDE_FROM_GAME_DVR_CAPTURE;
#elif defined AK_XBOXONE
		if (!in_bRecordable)
		{
			flags |= AUDCLNT_STREAMFLAGS_PREVENT_LOOPBACK_CAPTURE;
			hr = pAudioClient2->ExcludeFromGameDVRCapture();
			AKASSERT(hr == S_OK);	//Not critical
		}

		if (in_idPlugin == AKPLUGINID_DVR_SINK)
		{
			props.eCategory = AudioCategory_GameMedia;
			hr = pAudioClient2->RegisterXboxVolumeNotificationCallback(this);
			AKASSERT(hr == S_OK);	//Not critical
	}
#endif

		hr = pAudioClient2->SetClientProperties(&props);
		AKASSERT(hr == S_OK);

		pAudioClient2->Release();
	}


	// Correct for non 48kHz sample rates
	// first start with the time for a refill.
	REFERENCE_TIME oneBuffer = (REFERENCE_TIME)MFTIMES_PER_MILLISEC * 1000 * AK_NUM_VOICE_REFILL_FRAMES / AK_CORE_SAMPLERATE;
	REFERENCE_TIME timeBuffer = oneBuffer * g_PDSettings.uNumRefillsInVoice;
	while( timeBuffer < timeMinPeriod )
		timeBuffer += oneBuffer;	


#if !defined AK_XBOX && _MSC_VER >= 1600 
	if (wfExt.Format.nSamplesPerSec != AK_CORE_SAMPLERATE)
		flags |= AUDCLNT_STREAMFLAGS_RATEADJUST;
#endif	

 	hr = m_pClientOut->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, timeBuffer, 0, (WAVEFORMATEX *)&wfExt, NULL);
	if (hr != S_OK)
		return AK_Fail;

	hr = m_pClientOut->SetEventHandle(CAkLEngine::GetProcessEvent());
	if (hr != S_OK)
		return AK_Fail;

	hr = m_pClientOut->GetBufferSize( (UINT32*)&m_uBufferFrames );
	if ( hr != S_OK )
		return AK_Fail;	

	//AKASSERT( !( m_uBufferFrames % 1024 ) ); // result of our calculations should be a multiple of 1024 frames

	hr = m_pClientOut->GetService( __uuidof(IAudioRenderClient), (void **) &m_pRenderClient );
	if ( hr != S_OK )
		return AK_Fail;

	if (AllocBuffer(endPointSpeakersConfig) != AK_Success)
		return AK_Fail;
		
	AK_PC_WAIT_TIME = 1000 * AK_NUM_VOICE_REFILL_FRAMES / AK_CORE_SAMPLERATE * 2;	//Normally, we should be awoken by the hardware.  So if it takes twice the time, lets do something.
		
#if defined AK_XBOX
	return AK_Success;
#else
	return InitResampler(wfExt.Format.nSamplesPerSec, endPointSpeakersConfig);
#endif
}



AKRESULT CAkSinkWasapi::InitResampler(AkUInt32 in_uDeviceSampleRate, AkChannelConfig in_endpointSpeakerConfig)
{
#ifdef AK_WIN
	if (in_uDeviceSampleRate == AK_CORE_SAMPLERATE)
		return AK_Success;
	
#if _MSC_VER >= 1600
	//Try the WASAPI resampler, even if it is limited.
	IAudioClockAdjustment* pClockAdjust = NULL;
	HRESULT hr = m_pClientOut->QueryInterface(__uuidof(IAudioClockAdjustment), (LPVOID*)&pClockAdjust);
	if (pClockAdjust != NULL)
	{
		hr = pClockAdjust->SetSampleRate((float)AK_CORE_SAMPLERATE);
		pClockAdjust->Release();
	}

	if (hr == S_OK)
		return AK_Success;
#endif

	AkAudioFormat AudioFormat;
	AudioFormat.SetAll(
		AK_CORE_SAMPLERATE,
		in_endpointSpeakerConfig,
		AK_LE_NATIVE_BITSPERSAMPLE,
		AK_LE_NATIVE_BITSPERSAMPLE / 8 * in_endpointSpeakerConfig.uNumChannels,
		AK_FLOAT,
		AK_NONINTERLEAVED
		);

	if (m_Resampler.Init(&AudioFormat, in_uDeviceSampleRate) != AK_Success)
		return AK_Fail;

	m_Resampler.SetPitch(0.0f, false);

	//Allocate buffer for resampling, only do that if the sample rate is not 48000k.
	AkUInt32 m_ResampleBufferFrames = (m_uBufferFrames / 2);	//double buffer
	AkUInt32 uMasterOutSize = m_ResampleBufferFrames * in_endpointSpeakerConfig.uNumChannels * sizeof(AkReal32);
	void * pData = AkMalign(AkMemID_Processing, uMasterOutSize, AK_BUFFER_ALIGNMENT);
	if (!pData)
	{
		m_Resampler.Term();
		return AK_InsufficientMemory;
	}

	AkZeroMemAligned(pData, uMasterOutSize);
	m_ResampleBuffer.Clear();
	m_ResampleBuffer.AttachInterleavedData(
		pData,							// Buffer.
		(AkUInt16)m_ResampleBufferFrames,	// Buffer size (in sample frames).
		0,								// Valid frames.
		in_endpointSpeakerConfig);				// Chan config.

	//Set up the m_Resampler
	m_Resampler.SetRequestedFrames(m_ResampleBufferFrames);
#endif
	return AK_Success;
}

// CAkSink overrides

AKRESULT CAkSinkWasapi::Reset()
{
	AKASSERT( m_pClientOut );
	m_pClientOut->Start();
	return AK_Success;
}

AKRESULT CAkSinkWasapi::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	//Term only if this is the last reference.
	m_cShareCount--;
	if (m_cShareCount > 0)
		return AK_Success;

#ifdef AK_WIN
	if (m_ResampleBuffer.MaxFrames() > 0)
	{
		AkFalign(AkMemID_Processing, m_ResampleBuffer.GetContiguousDeinterleavedData());
		m_ResampleBuffer.Clear();
	}
#endif

	if ( m_pRenderClient )
	{
		m_pRenderClient->Release();
		m_pRenderClient = NULL;
	}

	if ( m_pClientOut )
	{
		HRESULT hr = m_pClientOut->Stop();

#if defined AK_XBOXONE && !defined AK_GAMINGXBOX
		IAudioClient2 *pAudioClient2 = NULL;
		m_pClientOut->QueryInterface(__uuidof(IAudioClient2), (void**)&pAudioClient2);
		if (pAudioClient2)
		{
			pAudioClient2->UnregisterXboxVolumeNotificationCallback();
			pAudioClient2->Release();
		}
#endif
		m_pClientOut->Release();
		m_pClientOut = NULL;
	}

	if ( m_pDeviceOut )
	{
		m_pDeviceOut->Release();
		m_pDeviceOut = NULL;
	}

	return CAkSink::Term( in_pAllocator );
}

bool CAkSinkWasapi::IsStarved()
{
	return false; // FIXME: no starvation detection
}

void CAkSinkWasapi::ResetStarved()
{
}

AKRESULT CAkSinkWasapi::IsDataNeeded( AkUInt32 & out_uBuffersNeeded )
{
	UINT32 uPaddingFrames;

	HRESULT hr = m_pClientOut->GetCurrentPadding( &uPaddingFrames );
	if ( hr != S_OK ) 
		return AK_Fail;

	AkUInt32 uTotalFrames = AK_NUM_VOICE_REFILL_FRAMES;
#if !defined AK_XBOX
	if (m_ResampleBuffer.MaxFrames() != 0)	//If not initialized, it means the output is 48k.
		uTotalFrames = m_ResampleBuffer.MaxFrames();
#endif

	out_uBuffersNeeded = (m_uBufferFrames - uPaddingFrames) / uTotalFrames;

	return AK_Success;
}

// Final processing.  We need to adjust the buffer to what the HW needs.
// Might need resampling and up/down mix.
// Always need gain and interleaving.
void CAkSinkWasapi::Consume(AkAudioBuffer* in_pInputBuffer, AkRamp in_gain)
{
	if (in_pInputBuffer->uValidFrames == 0 )
		return;
	
	// Downmix and volume.
	// Do final pass on data (interleave, format conversion, and the like) before presenting to sink.
	AkAudioMix mix;
	if (mix.Allocate(in_pInputBuffer->NumChannels(), m_MasterOut.NumChannels()) == AK_Success)
	{
		CAkSpeakerPan::GetSpeakerVolumesDirect(in_pInputBuffer->GetChannelConfig(), m_MasterOut.GetChannelConfig(), 0, mix.Prev());
		mix.CopyPrevToNext(in_pInputBuffer->NumChannels(), m_MasterOut.NumChannels());
		AkUInt16 maxFrames = m_MasterOut.MaxFrames();
		AkMixer::MixNinNChannels(in_pInputBuffer, &m_MasterOut, in_gain, mix.Prev(), mix.Next(), 1.f / (AkReal32)maxFrames, maxFrames);
		mix.Free();
	}

	m_MasterOut.uValidFrames = in_pInputBuffer->uValidFrames;

	m_bDataReady = true;
}

void CAkSinkWasapi::OnFrameEnd()
{
	m_cSharedProcessingDone--;
	if (m_cSharedProcessingDone == 0)
	{
		CAkSinkBase::OnFrameEnd();
		m_cSharedProcessingDone = m_cShareCount;
	}
}

void CAkSinkWasapi::SubmitBuffer(AkPipelineBufferBase &in_buffer)
{
	//Interleave the data
	CAkResampler::InterleaveAndSwapOutput(&in_buffer);
	BYTE * pData = NULL;

	HRESULT hr = m_pRenderClient->GetBuffer(in_buffer.uValidFrames, &pData);
	if (hr != S_OK || pData == NULL)
		return;

	void*	pvFrom = in_buffer.GetInterleavedData();

	AkUInt32 uNumSamplesRefill = in_buffer.uValidFrames * in_buffer.NumChannels();
	memcpy(pData, pvFrom, uNumSamplesRefill * sizeof(float));

	hr = m_pRenderClient->ReleaseBuffer(in_buffer.uValidFrames, 0);		
}

void CAkSinkWasapi::PassData()
{
#if !defined AK_XBOX
	// Resample if needed
	// If not initialized, it means the output is 48k.
	if (m_ResampleBuffer.MaxFrames() != 0)
	{
		AKRESULT result = m_ResampleBuffer.uValidFrames == m_ResampleBuffer.MaxFrames() ? AK_DataReady : AK_NoDataReady;
		do
		{
			if (result == AK_DataReady)
			{
				//submit resampled buffer
				SubmitBuffer(m_ResampleBuffer);
				m_ResampleBuffer.uValidFrames = 0;
				m_Resampler.SetOutputBufferOffset(0);
			}

			result = m_Resampler.Execute(&m_MasterOut, &m_ResampleBuffer);

		} while (result == AK_DataReady);

		m_Resampler.SetInputBufferOffset(0);
	}
	else
#endif
		SubmitBuffer(m_MasterOut);

	AkZeroMemAligned(m_MasterOut.GetInterleavedData(), m_MasterOut.MaxFrames() * sizeof(AkReal32)* m_MasterOut.NumChannels());
	m_MasterOut.uValidFrames = 0;
}

void CAkSinkWasapi::PassSilence()
{
	BYTE * pData = NULL;

#if !defined AK_XBOX
	//Last resampling buffer
	if(m_ResampleBuffer.uValidFrames != 0)
	{
		for(AkUInt32 i = 0; i < m_ResampleBuffer.NumChannels(); i++)
		{
			ZeroMemory((float*)m_ResampleBuffer.GetChannel(i) + m_ResampleBuffer.uValidFrames, (m_ResampleBuffer.MaxFrames() - m_ResampleBuffer.uValidFrames) *sizeof(float));
		}
		
		m_ResampleBuffer.uValidFrames = m_ResampleBuffer.MaxFrames();
		SubmitBuffer(m_ResampleBuffer);
		m_ResampleBuffer.uValidFrames = 0;
		m_Resampler.SetOutputBufferOffset(0);			
	}
	else
#endif
	{
		HRESULT hr = m_pRenderClient->GetBuffer(AK_NUM_VOICE_REFILL_FRAMES, &pData);
		if(hr != S_OK || pData == NULL)
			return;

		m_pRenderClient->ReleaseBuffer(AK_NUM_VOICE_REFILL_FRAMES, AUDCLNT_BUFFERFLAGS_SILENT);
	}
	
}
#endif // AK_WASAPI