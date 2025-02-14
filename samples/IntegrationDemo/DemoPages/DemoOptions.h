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

#pragma once

#include "Page.h"
#include "Platform.h"
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCallback.h>
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <string>

/// Options page to configure settings on the sound engine
class DemoOptions : public Page
{
public:
	struct DeviceId {
		DeviceId()
			: sharesetId(0)
			, deviceId(0)
		{}

		DeviceId(AkUInt32 in_sharesetId, AkUInt32 in_deviceId)
			: sharesetId(in_sharesetId)
			, deviceId(in_deviceId)
		{}

		AkUInt32 sharesetId;
		AkUInt32 deviceId;
	};

	/// DemoOptions class constructor
	DemoOptions(Menu& in_ParentMenu);

	/// Initializes the page.
	/// \return True if successful and False otherwise.
	virtual bool Init();

	/// Releases resources used by the page.
	virtual void Release();

	/// Override of the Page::Draw() method.
	virtual void Draw();
	void InitControls();

private:

	void UpdateSpeakerConfigForShareset();

	void outputDeviceOption_Changed(void* in_pSender, ControlEvent* in_pControlEvent);
	void spatialAudioOption_Changed(void* in_pSender, ControlEvent* in_pControlEvent);
	void speakerPosOption_Changed(void* in_pSender, ControlEvent* in_pControlEvent);
	void speakerConfigOption_Changed(void* in_pSender, ControlEvent* in_pControlEvent);

	void updateOutputDevice();

	AkUInt32 m_activeDeviceIdx;
	std::vector<DeviceId> m_deviceIds;

	AkPanningRule m_activePanningRule;
	AkUInt32 m_activeChannelConfig;
	bool m_spatialAudioRequested;
	bool m_spatialAudioDeviceAvailable;

	ToggleControl* m_speakerConfigToggle;
};

