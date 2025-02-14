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
// AkLEngine.cpp
//
// Implementation of the IAkLEngine interface. Android version.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <AK/SoundEngine/Common/AkSimd.h>
#include "AkLEngine.h"

#include <AK/SoundEngine/Platforms/GGP/AkPlatformContext.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

#include "Ak3DListener.h"
#include "AkAudioLibTimer.h"
#include "AkSettings.h"
#include "../Linux/AkSink.h"
#include "AkProfile.h"
#include "AkMonitor.h"
#include "Ak3DParams.h"
#include "AkDefault3DParams.h"			// g_DefaultListenerPosition.
#include "AkRegistryMgr.h"
#include "AkResampler.h"
#include "AkRTPCMgr.h"
#include "AkFXMemAlloc.h"
#include "AkSpeakerPan.h"
#include "AkEnvironmentsMgr.h"
#include "AkAudioMgr.h"
#include "AkEffectsMgr.h"
#include "AkPlayingMgr.h"
#include "AkPositionRepository.h"
#include "AkSound.h"
#include "AkVPLFilterNodeOutOfPlace.h"
#include "AkFxBase.h"
#include "AkOutputMgr.h"
#include <dlfcn.h>

#include <AK/Tools/Common/AkFNVHash.h>

#include <ggp/ggp.h>
#include <ggp/audio_playback.h>

namespace
{
	struct EndpointInfo
	{
		char streamName[AK_MAX_PATH] = "";
		AkUInt32 nbChannels = 0;
		bool isEnabled = false; // states if audio should be output into this endpoint
		bool isInitialized = false; // set to true once initialization is complete
		bool hasChanged = false; // signal that endpoint should be reinitialized

		bool NeedsReset() { return isInitialized && hasChanged; }
	};
}

//-----------------------------------------------------------------------------
//Static variables.
//-----------------------------------------------------------------------------
extern AkInitSettings		g_settings;
extern AkPlatformInitSettings g_PDSettings;

AkEvent	CAkLEngine::m_EventStop;
AK::IAkPlatformContext* CAkLEngine::m_pPlatformContext = NULL;

class CAkGGPContext : public AK::IAkGGPContext
{
public:
	// --- IAkLinuxContext --- ///

	bool UsePulseAudioServerInfo() override
	{
		return false;
	}

	bool IsPluginSupported(AkPluginID pluginID) override
	{
		return pluginID == AKPLUGINID_DEFAULT_SINK || pluginID == AKPLUGINID_COMMUNICATION_SINK;
	}

	bool IsStreamReady(AkPluginID pluginID) override
	{
		return GetEndpoint(pluginID).isEnabled;
	}

	const char* GetStreamName(AkPluginID pluginID, AkDeviceID /* deviceID */) override
	{
		return GetEndpoint(pluginID).streamName;
	}

	const char* GetStreamName(AkDeviceID /* deviceID */) override
	{
		return m_audioEndpoint.streamName; // AKPLUGINID_DEFAULT_SINK
	}

	AkUInt32 GetChannelCount(AkPluginID pluginID, AkDeviceID /* deviceID */) override
	{
		return GetEndpoint(pluginID).nbChannels;
	}

	AkUInt32 GetChannelCount(AkDeviceID /* deviceID */) override
	{
		return m_audioEndpoint.nbChannels; // AKPLUGINID_DEFAULT_SINK
	}

	void SetSinkInitialized(AkPluginID pluginID, bool isInitialized) override
	{
		::EndpointInfo& endpoint = GetEndpoint(pluginID);

		endpoint.isInitialized = isInitialized;
		if (isInitialized)
		{
			endpoint.hasChanged = false;
		}
	}

	void SetSinkInitialized(bool isInitialized) override
	{
		SetSinkInitialized(AKPLUGINID_DEFAULT_SINK, isInitialized);
	}

	// ---

	::EndpointInfo& GetEndpoint(AkPluginID pluginID)
	{
		if (pluginID == AKPLUGINID_COMMUNICATION_SINK)
			return m_voiceChatEndpoint;

		AKASSERT(pluginID == AKPLUGINID_DEFAULT_SINK && "Caller should check IsPluginSupported()");

		return m_audioEndpoint; // AKPLUGINID_DEFAULT_SINK
	}

	::EndpointInfo& GetEndpoint(ggp::AudioEndpoint::Type type)
	{
		AKASSERT(type != ggp::AudioEndpoint::Type::kInvalid);

		if (type == ggp::AudioEndpoint::Type::kVoiceChat)
			return m_voiceChatEndpoint;

		return m_audioEndpoint; // ggp::AudioEndpoint::kGameAudio
	}

	::EndpointInfo m_audioEndpoint = {};
	::EndpointInfo m_voiceChatEndpoint = {};

	// -- GGP Event Handling -- //

	void RegisterAudioEndpointCallback();

	void UnregisterAudioEndpointCallback()
	{
		ggp::RemoveAudioEndpointChangedHandler(m_audioEndpointChangedHandler);
	}
	
	void ProcessEvents()
	{
		while (m_eventQueue.Process());
	}

	ggp::EventQueue m_eventQueue;
	ggp::EventHandle m_audioEndpointChangedHandler = 0;
};

void CAkGGPContext::RegisterAudioEndpointCallback()
{
	m_audioEndpointChangedHandler = ggp::AddAudioEndpointChangedHandler(&m_eventQueue,
		[this](const ggp::AudioEndpointChangedEvent& event)
		{
			if (event.endpoint_state == ggp::AudioEndpoint::State::kInvalid)
				return;

			// Choose the right endpoint to modify
			::EndpointInfo& endpoint = GetEndpoint(event.endpoint_type);

			// Either enable or disable the endpoint
			if (event.endpoint_state == ggp::AudioEndpoint::State::kDisabled)
			{
				endpoint.isEnabled = false;
				endpoint.hasChanged = true;
			}
			else
			{
				ggp::Status status;
				ggp::String name = event.audio_endpoint.GetPulseAudioDeviceName(&status);
				if (!status.Ok())
					return;

				// Update stream parameters
				AKPLATFORM::SafeStrCpy(endpoint.streamName, name.c_str(), AK_MAX_PATH);
				endpoint.nbChannels = event.audio_endpoint.GetChannelCount(&status);

				// Signal audio endpoint change
				endpoint.isEnabled = true;
				endpoint.hasChanged = true;
			}
		}
	);
}

namespace AK
{
	namespace SoundEngine
	{
		AKRESULT RegisterPluginDLL(const AkOSChar* in_DllName, const AkOSChar* in_DllPath /* = NULL */)
		{
			//Find the DLL path from Android			
			AkOSChar fullName[1024];
			fullName[0] = 0;
			CAkLEngine::GetPluginDLLFullPath(fullName, 1024, in_DllName, in_DllPath);

			void *hLib = dlopen(fullName, RTLD_NOW | RTLD_DEEPBIND); //RTLD_DEEPBIND force local symbol to be check first then global
			if (hLib == NULL)
				return AK_FileNotFound;

			AK::PluginRegistration** RegisterList = (AK::PluginRegistration**)dlsym(hLib, "g_pAKPluginList");
			if (RegisterList == NULL)
				return AK_InvalidFile;

			return CAkEffectsMgr::RegisterPluginList(*RegisterList);
		}

		AKRESULT GetDeviceSpatialAudioSupport(AkUInt32 in_idDevice)
		{
			return AK_NotCompatible;
		}
	}
};


void CAkLEngine::GetDefaultPlatformInitSettings( 
	AkPlatformInitSettings &      out_pPlatformSettings      // Platform specific settings. Can be changed depending on hardware.
)
{
	memset( &out_pPlatformSettings, 0, sizeof( AkPlatformInitSettings ) );

	GetDefaultPlatformThreadInitSettings(out_pPlatformSettings);

	out_pPlatformSettings.uNumRefillsInVoice = AK_DEFAULT_NUM_REFILLS_IN_VOICE_BUFFER;
	out_pPlatformSettings.uSampleRate = DEFAULT_NATIVE_FREQUENCY;
	out_pPlatformSettings.sampleType = AK_DEFAULT_SAMPLE_TYPE;
}

void CAkLEngine::GetDefaultOutputSettings( AkOutputSettings & out_settings )
{
	GetDefaultOutputSettingsCommon(out_settings);
}

bool CAkLEngine::PlatformSupportsHwVoices()
{
	return false;
}

void CAkLEngine::PlatformWaitForHwVoices()
{
}

//-----------------------------------------------------------------------------
// Name: Init
// Desc: Initialise the object.
//
// Parameters:
//
// Return: 
//	Ak_Success:          Object was initialised correctly.
//  AK_InvalidParameter: Invalid parameters.
//  AK_Fail:             Failed to initialise the object correctly.
//-----------------------------------------------------------------------------
AKRESULT CAkLEngine::Init()
{
	if (g_PDSettings.sampleType != AK_INT && g_PDSettings.sampleType != AK_FLOAT)
	{
		AkOSChar msg[256];
		AK_OSPRINTF(msg, 256,
			"Invalid AkPlatformInitSettings::sampleType: '%d'. "
			"Supported values are AK_INT (0) and AK_FLOAT (1). "
			"Reverting to default (%d).\n",
			g_PDSettings.sampleType, AK_DEFAULT_SAMPLE_TYPE
		);
		AKPLATFORM::OutputDebugMsg(msg);
		g_PDSettings.sampleType = AK_DEFAULT_SAMPLE_TYPE;
	}

	AKRESULT res = SoftwareInit();

	AkAudioLibSettings::SetAudioBufferSettings(g_PDSettings.uSampleRate, g_settings.uNumSamplesPerFrame);

	auto* platformContext = static_cast<CAkGGPContext*>(m_pPlatformContext);
	platformContext->RegisterAudioEndpointCallback();
	platformContext->ProcessEvents();

	return res;
} // Init

AKRESULT CAkLEngine::InitPlatformContext()
{
	m_pPlatformContext = AkNew(AkMemID_SoundEngine, CAkGGPContext());
	if (!m_pPlatformContext)
		return AK_InsufficientMemory;
	return AK_Success;
}

void CAkLEngine::Term()
{
	SoftwareTerm();
} // Term

void CAkLEngine::TermPlatformContext()
{
	auto* context = static_cast<CAkGGPContext*>(m_pPlatformContext);
	context->UnregisterAudioEndpointCallback();
	AkDelete(AkMemID_SoundEngine, m_pPlatformContext);
	m_pPlatformContext = NULL;
}

//-----------------------------------------------------------------------------
// Name: Perform
// Desc: Perform all VPLs.
//-----------------------------------------------------------------------------
void CAkLEngine::Perform()
{
#if defined(AK_CPU_X86) || defined(AK_CPU_X86_64)
	// Avoid denormal problems in audio processing
	AkUInt32 uFlushZeroMode = _MM_GET_FLUSH_ZERO_MODE();
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif

	CAkGGPContext* context = static_cast<CAkGGPContext*>(m_pPlatformContext);

	context->ProcessEvents();

	// Game Audio Endpoint
	if (context->m_audioEndpoint.NeedsReset())
	{
		CAkOutputMgr::ResetMainDevice();
		context->m_audioEndpoint.hasChanged = false;
	}

	// Voice Chat Endpoint
	if (context->m_voiceChatEndpoint.NeedsReset())
	{
		auto* device = CAkOutputMgr::GetPluginDevice(AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_COMMUNICATION_SINK));
		if (device != nullptr)
		{
			device->SetState(AkDevice::eToActivate);
		}
		context->m_voiceChatEndpoint.hasChanged = false; // should this be done inside the if (device)?
	}

	SoftwarePerform();

#if defined(AK_CPU_X86) || defined(AK_CPU_X86_64)
	_MM_SET_FLUSH_ZERO_MODE(uFlushZeroMode);
#endif
} // Perform

AKRESULT CAkLEngine::GetPlatformDeviceList(
	AkPluginID in_pluginID,
	AkUInt32& io_maxNumDevices,					///< In: The maximum number of devices to read. Must match the memory allocated for AkDeviceDescription. Out: Returns the number of devices. Pass out_deviceDescriptions as NULL to have an idea of how many devices to expect.
	AkDeviceDescription* out_deviceDescriptions	///< The output array of device descriptions, one per device. Must be preallocated by the user with a size of at least io_maxNumDevices*sizeof(AkDeviceDescription).
)
{
	return AK_NotImplemented;
}

namespace AK
{
	namespace SoundEngine
	{
		AkUInt32 GetDeviceID(const ggp::Keyboard& in_keyboard)
		{
			AK::FNVHash32 hash;
			hash.Compute(in_keyboard); // hash handle, keyboard_ (the only data member)
			return hash.Get();
		}

		AkUInt32 GetDeviceID(const ggp::Gamepad& in_gamepad)
		{
			AK::FNVHash32 hash;
			hash.Compute(in_gamepad); // hash handle, gamepad_ (the only data member)
			return hash.Get();
		}
	}
}
