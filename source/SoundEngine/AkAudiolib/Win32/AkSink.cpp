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
// AkSink.cpp
//
// Platform dependent part
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "AkSink.h"
#include "AkSinkDirectSound.h"
#include "AkSinkXAudio2.h"
#include "AkOutputMgr.h"
#include "BGMSinkParams.h"
#include "AkFXMemAlloc.h"

#ifdef AK_WASAPI
#include "AkSinkWasapi.h"
#endif

extern AkPlatformInitSettings g_PDSettings;

AkAudioAPI CAkDefaultSink::s_CurrentAudioAPI = AkAPI_Default;

AK::IAkPlugin* AkCreateDefaultSink(AK::IAkPluginMemAlloc * in_pAllocator)
{
	return AK_PLUGIN_NEW(in_pAllocator, CAkDefaultSink());
}

CAkDefaultSink::CAkDefaultSink()
{
	m_pRealSink = NULL;
}

CAkDefaultSink::~CAkDefaultSink()
{

}

AKRESULT CAkDefaultSink::Init(
	AK::IAkPluginMemAlloc *	in_pAllocator,				// Interface to memory allocator to be used by the effect.
	AK::IAkSinkPluginContext *	in_pSinkPluginContext,	// Interface to sink plug-in's context.
	AK::IAkPluginParam *		in_pParams,				// Interface to plug-in parameters.
	AkAudioFormat &			in_rFormat)					// Audio data format of the input signal. 
{
	AkUInt32 idDevice;
	AkPluginID idPlugin;
	in_pSinkPluginContext->GetOutputID(idDevice, idPlugin);

	AkAudioAPI eAPIType = (AkAudioAPI)(g_PDSettings.eAudioAPI & AkAPI_Default);
	if(eAPIType == 0 || eAPIType == AkAPI_Default)
		eAPIType = s_CurrentAudioAPI; // Restore previously successfully used API, could be AkAPI_Default if still undetermined.

	CAkSink* pSink = NULL;

	AkOutputSettings settings;
	memset(&settings, 0, sizeof(AkOutputSettings));
	settings.channelConfig = in_rFormat.channelConfig;
	
	m_pRealSink = NULL;

	idPlugin = AKGETPLUGINIDFROMCLASSID(idPlugin);

	bool bRecordable = true;
#ifdef AK_XBOXONE	
	if (in_pParams)
	{
		BGMSinkParams* pParams = (BGMSinkParams*)in_pParams;
		bRecordable = pParams->m_bDVRRecordable;
	}
#endif
	
#ifdef AK_XAUDIO2
	if(m_pRealSink == NULL && (eAPIType & AkAPI_XAudio2) != 0)
	{
		AKRESULT res = TryXAudio2(settings, idPlugin, idDevice, bRecordable, m_pRealSink);
		if(res == AK_Fail)
			return AK_Fail;	//Generic failure, we'll retry later with the same API and settings.

		//Other failures: AK_DLLNotLoaded can be returned.  In this case, switch to WASAPI as a fallback.
	}
#endif

	if(m_pRealSink == NULL && (eAPIType & AkAPI_Wasapi) != 0)
	{
		m_pRealSink = TryWasapi(settings, idPlugin, idDevice, bRecordable);
	}

#ifdef AK_DIRECTSOUND
	if(m_pRealSink == NULL && (eAPIType & AkAPI_DirectSound) != 0)
		m_pRealSink = TryDirectSound(settings);
#endif

	if(m_pRealSink)
	{
		in_rFormat.channelConfig = m_pRealSink->GetSpeakerConfig();
		return AK_Success;
	}

	return AK_Fail;
}
	
AKRESULT CAkDefaultSink::Term(AK::IAkPluginMemAlloc * in_pAllocator)
{
	if(m_pRealSink)
	{
		m_pRealSink->Term(in_pAllocator);
		m_pRealSink = NULL;
	}
	AK_PLUGIN_DELETE(in_pAllocator, this);
	return AK_Success;
}

AKRESULT CAkDefaultSink::Reset()
{
	return m_pRealSink->Reset();
}

AKRESULT CAkDefaultSink::GetPluginInfo(AkPluginInfo & out_rPluginInfo)
{
	out_rPluginInfo.bCanChangeRate = false;
	out_rPluginInfo.bIsInPlace = true;
	out_rPluginInfo.bReserved = true;
	out_rPluginInfo.eType = AkPluginTypeSink;
	out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
	return AK_Success;
}

void CAkDefaultSink::Consume(AkAudioBuffer * in_pInputBuffer, AkRamp in_gain)
{
	return m_pRealSink->Consume(in_pInputBuffer, in_gain);
}

void CAkDefaultSink::OnFrameEnd()
{
	return m_pRealSink->OnFrameEnd();
}

bool CAkDefaultSink::IsStarved()
{
	return m_pRealSink->IsStarved();
}

void CAkDefaultSink::ResetStarved()
{
	return m_pRealSink->ResetStarved();
}

AKRESULT CAkDefaultSink::IsDataNeeded(AkUInt32 & out_uBuffersNeeded)
{
	return m_pRealSink->IsDataNeeded(out_uBuffersNeeded);
}

IMMDevice * FindDevice(AkPluginID in_idIntentedPlugin, AkUInt32 in_uInstance, AkUInt32 &out_idDefault)
{
	out_idDefault = AK_INVALID_DEVICE_ID; 
	IMMDevice * pDevice = NULL;
#if !defined AK_USE_UWP_API || defined AK_XBOX
	IMMDeviceEnumerator * pEnumerator = NULL;
	CoCreateInstance(
		__uuidof(MMDeviceEnumerator), NULL,
		CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
		(void**)&pEnumerator);

	if(!pEnumerator)
		return NULL;

	CAkSinkWasapi * pSinkWasapi = NULL;

	ERole deviceRole = eConsole;
	if(in_idIntentedPlugin == AKPLUGINID_COMMUNICATION_SINK)
		deviceRole = eCommunications;
	
	pEnumerator->GetDefaultAudioEndpoint(eRender, deviceRole, &pDevice);
	if(pDevice)
	{
		out_idDefault = AK::GetDeviceID(pDevice);
		pDevice->Release();
		pDevice = NULL;
	}

#ifdef AK_XBOXONE
	if((in_idIntentedPlugin == AKPLUGINID_DEFAULT_SINK || in_idIntentedPlugin == AKPLUGINID_DVR_SINK) && in_uInstance == 0)
		in_uInstance = out_idDefault;
#else
	if(in_uInstance == 0)
		in_uInstance = out_idDefault;
#endif

	IMMDeviceCollection * pDevices = NULL;
	pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATEMASK_ALL, &pDevices);
	if(pDevices)
	{
		UINT dwCount = 0;
		pDevices->GetCount(&dwCount);
		for(AkUInt32 i = 0; i < dwCount; i++)
		{
			pDevices->Item(i, &pDevice);
			if(pDevice)
			{
				if(AK::GetDeviceID(pDevice) == in_uInstance)
					break;
				pDevice->Release();
				pDevice = NULL;
			}
		}
		pDevices->Release();
	}

	pEnumerator->Release();
#endif
	return pDevice;
}

AKRESULT CAkDefaultSink::TryXAudio2(const AkOutputSettings &in_settings, AkPluginID in_uPlugin, AkUInt32 in_uInstance, bool in_bRecordable, CAkSink*& out_pSink)
{
	AKRESULT res = AK_Fail;
#ifdef AK_XAUDIO2
	IMMDevice *pDevice = NULL;
	if(in_uInstance || in_uPlugin == AKPLUGINID_COMMUNICATION_SINK)
	{
		AkUInt32 idDefault = 0;
		pDevice = FindDevice(in_uPlugin, in_uInstance, idDefault);
		if(pDevice == NULL)
			return AK_Fail;
	}
	
	CAkSinkXAudio2 * pSinkXAudio2 = AkNew(AkMemID_ProcessingPlugin, CAkSinkXAudio2());
	if (!pSinkXAudio2)
		return AK_Fail;
	
	res = pSinkXAudio2->Init(pDevice, in_uPlugin, g_PDSettings, in_settings, in_bRecordable);
#if !defined AK_USE_UWP_API || defined AK_XBOX
	if (pDevice)
		pDevice->Release();
#else
	AKASSERT(pDevice == nullptr);
#endif

	if(res == AK_Success)
	{
		s_CurrentAudioAPI = AkAPI_XAudio2;
		out_pSink = pSinkXAudio2;
		return AK_Success;
	}

	pSinkXAudio2->Term(AkFXMemAlloc::GetLower());
#endif
	return res;
}

#if defined AK_WASAPI 

static inline bool IsBuiltInWasapiDevice(AkDevice* in_pDevice)
{
	return (in_pDevice->uSinkPluginID == AKPLUGINID_DEFAULT_SINK ||
		in_pDevice->uSinkPluginID == AKPLUGINID_COMMUNICATION_SINK ||
		in_pDevice->uSinkPluginID == AKPLUGINID_DVR_SINK ||
		in_pDevice->uSinkPluginID == AKPLUGINID_PERSONAL_SINK);
}

CAkSinkWasapi* CheckSharedDevice(IMMDevice *in_pDevice, AkUInt32 in_idDefault)
{
	CAkSinkWasapi* pSinkWasapi = NULL;
	AkUInt32 id = AK::GetDeviceID(in_pDevice);
	for (AkDeviceList::Iterator it = CAkOutputMgr::OutputBegin(); it != CAkOutputMgr::OutputEnd(); ++it)
	{	
		if(IsBuiltInWasapiDevice(*it) && AK_GET_DEVICE_ID_FROM_DEVICE_KEY((*it)->uDeviceID) == id && (*it)->GetState() == AkDevice::eActive)
		{
			pSinkWasapi = (CAkSinkWasapi*)((CAkDefaultSink*)((*it)->Sink()))->RealSink();
			pSinkWasapi->Share();
			return pSinkWasapi;
		}
	}

	//Also check the default device id.  When the Default Endpoint changes, we can end up with multiple devices on the same AKOutput, therefore on the same Wasapi sink.
	if (in_idDefault == id)
	{		
		AkDevice * pAkDevice = CAkOutputMgr::GetPrimaryDevice();
		if(pAkDevice && IsBuiltInWasapiDevice(pAkDevice) && pAkDevice->GetState() == AkDevice::eActive)
		{
			pSinkWasapi = (CAkSinkWasapi*)((CAkDefaultSink*)(pAkDevice->Sink()))->RealSink();
			pSinkWasapi->Share();	//Now shared.
			return pSinkWasapi;
		}
	}

	return NULL;
}
#endif

CAkSink * CAkDefaultSink::TryWasapi(const AkOutputSettings &in_settings, AkPluginID in_idPlugin, AkUInt32 in_uInstance, bool in_bRecordable)
{		
#if defined AK_WASAPI 	
	CAkSinkWasapi * pSinkWasapi = NULL;
	AkUInt32 idDefault = 0;
	IMMDevice *pDevice = FindDevice(in_idPlugin, in_uInstance, idDefault);
	if (pDevice == NULL)
		return NULL;	

#ifndef AK_XBOXONE	//No device sharing on XB1.
	pSinkWasapi = CheckSharedDevice(pDevice, idDefault);
#endif

	//If no shared sink found, init a new one.
	if (!pSinkWasapi)
	{
		pSinkWasapi = AkNew(AkMemID_ProcessingPlugin, CAkSinkWasapi());
		if(pSinkWasapi && pSinkWasapi->Init(pDevice, in_settings, in_idPlugin, in_bRecordable) != AK_Success)
		{
			pSinkWasapi->Term(AkFXMemAlloc::GetLower());
			pSinkWasapi = NULL;
		}		
	}
	pDevice->Release();

	if (pSinkWasapi)
		s_CurrentAudioAPI = AkAPI_Wasapi;

	return pSinkWasapi;
#else // AK_WASAPI	
	return NULL;
#endif

}

CAkSink * CAkDefaultSink::TryDirectSound(const AkOutputSettings &in_settings)
{
#ifdef AK_DIRECTSOUND
	CAkSinkDirectSound * pSinkDS = AkNew(AkMemID_ProcessingPlugin, CAkSinkDirectSound());
	if(!pSinkDS)
		return NULL;

	if(pSinkDS->Init(g_PDSettings, in_settings.channelConfig.uChannelMask) == AK_Success)
	{
		s_CurrentAudioAPI = AkAPI_DirectSound;
		return pSinkDS;
	}

	pSinkDS->Term(AkFXMemAlloc::GetLower());
#endif
	return NULL;
}

CAkSink::CAkSink()
{
	m_MasterOut.Clear();
}

AKRESULT CAkSink::AllocBuffer(AkChannelConfig in_channelConfig)
{
	AkUInt32 uMasterOutSize = LE_MAX_FRAMES_PER_BUFFER * in_channelConfig.uNumChannels * sizeof(AkReal32);
	void * pData = AkMalign(AkMemID_Processing, uMasterOutSize, AK_BUFFER_ALIGNMENT);
	if(!pData)
		return AK_InsufficientMemory;

	AkZeroMemAligned(pData, uMasterOutSize);

	m_MasterOut.Clear();
	m_MasterOut.AttachInterleavedData(
		pData,						// Buffer.
		LE_MAX_FRAMES_PER_BUFFER,	// Buffer size (in sample frames).
		0,							// Valid frames.
		in_channelConfig);		// Chan config.

	return AK_Success;
}