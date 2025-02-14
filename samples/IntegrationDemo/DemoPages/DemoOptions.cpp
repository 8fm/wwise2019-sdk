/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

  Copyright (c) 2020 Audiokinetic Inc.
*******************************************************************************/

#include "stdafx.h"
#include "Platform.h"
#include "DemoOptions.h"
#include "DemoOptionsUtil.h"
#include <AK/SoundEngine/Common/AkSoundEngine.h>    // Sound engine

#include "IntegrationDemo.h"

// SpatialAudio configuration is disabled in IntegrationDemo because the related plug-ins are optional,
// and may not be present. If you do have the plug-in contents and want to try it, include the relevant
// plug-in factory, and link the relevant lib.
#define SPATIAL_AUDIO_PLUGIN_AVAILABLE 0

// To persist the active device when exiting and reentering the demo page
static AkUInt32 g_activeDeviceIdx = 0;
static bool g_spatialAudioEnabled = false;

/// DemoOptions class constructor
DemoOptions::DemoOptions( Menu& in_ParentMenu ) : Page( in_ParentMenu, "Options Menu" )
	, m_activeDeviceIdx(g_activeDeviceIdx)
	, m_activePanningRule(AkPanningRule_Speakers)
	, m_activeChannelConfig(0)
	, m_spatialAudioRequested(g_spatialAudioEnabled)
	, m_spatialAudioDeviceAvailable(g_spatialAudioEnabled)
	, m_speakerConfigToggle(NULL)
{
	m_szHelp = "This page provides options to configure the sound engine, the changes of which are reflected in the other demos.";
}

/// Initializes the demo.
/// \return True if successful and False otherwise.
bool DemoOptions::Init()
{
	// Initialize the page
	if (Page::Init())
	{
		updateOutputDevice();
		return true;
	}
	return false;
}

/// Releases resources used by the demo.
void DemoOptions::Release()
{
	g_activeDeviceIdx = m_activeDeviceIdx;
	g_spatialAudioEnabled = m_spatialAudioRequested && m_spatialAudioDeviceAvailable;
	Page::Release();
}

void PopulateOutputDeviceOptions(std::vector<DemoOptions::DeviceId>& out_deviceIds, AkUInt32& out_activeDeviceIdx, ToggleControl& out_toggle)
{
	const std::vector<const char*>& supportedSharesets = IntegrationDemo::GetSupportedAudioDeviceSharesets();

	// Set active device if we're querying the System sink default device (with id = 0)
	bool isDefaultDeviceRequested =
		IntegrationDemo::Instance().GetOutputDeviceId() ==
		AK::SoundEngine::GetOutputID(IntegrationDemo::GetDefaultAudioDeviceSharesetId(), IntegrationDemo::GetDefaultAudioDeviceId());

	for (const auto& sharesetName : supportedSharesets)
	{
		AkUInt32 sharesetId = AK::SoundEngine::GetIDFromString(sharesetName);

		AkUInt32 deviceCount = 0;
		AKRESULT res = AK::SoundEngine::GetDeviceList(sharesetId, deviceCount, nullptr);
		if (deviceCount == 0 || res != AK_Success)
		{
			char name[AK_MAX_PATH] = { 0 };
			snprintf(name, AK_MAX_PATH, "%s - Primary Output", sharesetName);
			out_toggle.AddOption(name);
			out_deviceIds.push_back(DemoOptions::DeviceId(sharesetId, 0));
			continue;
		}

		std::vector<AkDeviceDescription> devices(deviceCount);
		AK::SoundEngine::GetDeviceList(sharesetId, deviceCount, devices.data());

		for (AkUInt32 i = 0; i < deviceCount; ++i)
		{
			if (devices[i].deviceStateMask != AkDeviceState_Active)
				continue;

			char* deviceName = NULL;
			CONVERT_OSCHAR_TO_CHAR(devices[i].deviceName, deviceName);

			char name[AK_MAX_PATH] = { 0 };
			snprintf(name, AK_MAX_PATH, "%s - %s", sharesetName, deviceName);
	
			out_toggle.AddOption(name);
			out_deviceIds.push_back(DemoOptions::DeviceId(sharesetId, devices[i].idDevice));

			if (isDefaultDeviceRequested && devices[i].isDefaultDevice)
			{
				out_activeDeviceIdx = i;
			}
		}
	}
}

void DemoOptions::InitControls()
{
	ToggleControl* newToggle = NULL;

	newToggle = new ToggleControl(*this);
	newToggle->SetLabel("Device:");
	PopulateOutputDeviceOptions(m_deviceIds, m_activeDeviceIdx, *newToggle);
	if (m_activeDeviceIdx < m_deviceIds.size())
		newToggle->SetSelectedIndex(m_activeDeviceIdx);
	else
		m_activeDeviceIdx = 0;

	newToggle->SetDelegate((PageMFP)&DemoOptions::outputDeviceOption_Changed);
	m_Controls.push_back(newToggle);

	// Note that despite providing these for all platforms, not all platforms support
	// all of these options equally. For example, Spatial Audio will be requested,
	// but unless the platform/device supports it (see: AK::SoundEngine::GetDeviceSpatialAudioSupport)
	// it will be ignored. Also, Speaker Configuration is handled automatically on most
	// platforms, and will be ignored.

#if SPATIAL_AUDIO_PLUGIN_AVAILABLE
	newToggle = new ToggleControl(*this);
	newToggle->SetLabel("Spatial Audio:");
	newToggle->AddOption("Disabled", (void*)false);
	newToggle->AddOption("Enabled", (void*)true);
	newToggle->SetSelectedIndex(g_spatialAudioEnabled ? 1 : 0);
	newToggle->SetDelegate((PageMFP)&DemoOptions::spatialAudioOption_Changed);
	m_Controls.push_back(newToggle);
#endif

	newToggle = new ToggleControl(*this);
	newToggle->SetLabel("Speaker Positioning:");
	newToggle->AddOption("Speakers", (void*)AkPanningRule_Speakers);
	newToggle->AddOption("Headphones", (void*)AkPanningRule_Headphones);
	newToggle->SetDelegate((PageMFP)&DemoOptions::speakerPosOption_Changed);
	m_Controls.push_back(newToggle);

	m_speakerConfigToggle = new ToggleControl(*this);
	m_speakerConfigToggle->SetLabel("Speaker Configuration:");
	demoOptionsUtil::PopulateOutputSpeakerOptions(*m_speakerConfigToggle);
	m_speakerConfigToggle->SetDelegate((PageMFP)&DemoOptions::speakerConfigOption_Changed);
	m_Controls.push_back(m_speakerConfigToggle);
}

void DemoOptions::UpdateSpeakerConfigForShareset()
{
	if (m_speakerConfigToggle == nullptr)
		return;

	// Custom speaker configurations are only valid for the default sink
	if (m_deviceIds[m_activeDeviceIdx].sharesetId == IntegrationDemo::GetDefaultAudioDeviceSharesetId())
	{
		m_activeChannelConfig = (AkUInt32)((uintptr_t)m_speakerConfigToggle->SelectedValue());
	}
	else
	{
		m_activeChannelConfig = 0;
		m_speakerConfigToggle->SetSelectedIndex(0);
	}
}

void DemoOptions::outputDeviceOption_Changed(void* in_pSender, ControlEvent* in_pControlEvent)
{
	ToggleControl& toggleControl = *((ToggleControl*)in_pSender);
	m_activeDeviceIdx = toggleControl.SelectedIndex();
	UpdateSpeakerConfigForShareset();
	updateOutputDevice();
}

void DemoOptions::spatialAudioOption_Changed(void* in_pSender, ControlEvent* in_pControlEvent)
{
	ToggleControl& toggleControl = *((ToggleControl*)in_pSender);
	m_spatialAudioRequested = ((uintptr_t)toggleControl.SelectedValue()) != 0;
	updateOutputDevice();
}

void DemoOptions::speakerPosOption_Changed(void* in_pSender, ControlEvent* in_pControlEvent)
{
	ToggleControl& toggleControl = *((ToggleControl*)in_pSender);
	m_activePanningRule = (AkPanningRule)((uintptr_t)toggleControl.SelectedValue());
	updateOutputDevice();
}

void DemoOptions::speakerConfigOption_Changed(void* in_pSender, ControlEvent* in_pControlEvent)
{
	UpdateSpeakerConfigForShareset();
	updateOutputDevice();
}

void DemoOptions::updateOutputDevice()
{
	DeviceId newDeviceId = m_activeDeviceIdx < m_deviceIds.size() ?
		m_deviceIds[m_activeDeviceIdx] : DeviceId(IntegrationDemo::GetDefaultAudioDeviceSharesetId(), 0);

	AkUInt32 newChannelConfig = m_activeChannelConfig;

	m_spatialAudioDeviceAvailable = (AK::SoundEngine::GetDeviceSpatialAudioSupport(newDeviceId.deviceId) == AK_Success);

	AkOutputSettings newSettings;
	newSettings.audioDeviceShareset = (m_spatialAudioRequested && m_spatialAudioDeviceAvailable) ?
		IntegrationDemo::GetSpatialAudioSharesetId() : newDeviceId.sharesetId;

	newSettings.idDevice = newDeviceId.deviceId;
	newSettings.ePanningRule = m_activePanningRule;
	if (newChannelConfig)
	{
		newSettings.channelConfig.SetStandard(newChannelConfig);
	}
	else
	{
		newSettings.channelConfig.Clear();
	}

	AK::SoundEngine::ReplaceOutput(newSettings,
		IntegrationDemo::Instance().GetOutputDeviceId(),
		&IntegrationDemo::Instance().GetOutputDeviceIdRef());
}

void DemoOptions::Draw()
{
	Page::Draw();
	DrawTextOnScreen(m_szHelp.c_str(), 70, 300, DrawStyle_Text);
#if SPATIAL_AUDIO_PLUGIN_AVAILABLE
	if (m_spatialAudioDeviceAvailable)
	{
		DrawTextOnScreen("Spatial Audio is available for the selected device", 70, 400, DrawStyle_Text);
	}
	else
	{
		DrawTextOnScreen("Spatial Audio is not available for the selected device", 70, 400, DrawStyle_Text);
	}
#endif
}



