/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the 
"Apache License"); you may not use this file except in compliance with the 
Apache License. You may obtain a copy of the Apache License at 
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
*******************************************************************************/

/// \file 
/// Main Sound Engine interface, XboxOne-specific.

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

struct IXAudio2;
struct IMMDevice;

///< API used for audio output
///< Use with AkInitSettings to select the API used for audio output.  
///< Use AkAPI_Default, it will select the more appropriate API depending on the computer's capabilities.  Other values should be used for testing purposes.
///< \sa AK::SoundEngine::Init
typedef enum AkAudioAPIXboxOne
{
	AkAPI_Wasapi = 1 << 0,							///< Use WASAPI (this is the preferred API on XboxOne)
	AkAPI_Default = AkAPI_Wasapi,					///< Default value, will select the more appropriate API.	
	AkAPI_Dummy = 1 << 1,							///< Dummy output, simply eats the audio stream and outputs nothing.
} AkAudioAPI;

/// Platform specific initialization settings
/// \sa AK::SoundEngine::Init
/// \sa AK::SoundEngine::GetDefaultPlatformInitSettings
struct AkPlatformInitSettings
{
    // Threading model.
    AkThreadProperties  threadLEngine;			///< Lower engine threading properties
	AkThreadProperties  threadOutputMgr;		///< Ouput thread threading properties
	AkThreadProperties  threadBankManager;		///< Bank manager threading properties (its default priority is AK_THREAD_PRIORITY_NORMAL)
	AkThreadProperties  threadMonitor;			///< Monitor threading properties (its default priority is AK_THREAD_PRIORITY_ABOVENORMAL). This parameter is not used in Release build.

	// Voices.
	AkUInt16            uNumRefillsInVoice;		///< Number of refill buffers in voice buffer. 2 == double-buffered, defaults to 4.
	AkUInt16			uMaxXMAVoices;			///< Maximum number of hardware-accelerated XMA voices used at run-time. Default is 128 voices.
	bool				bHwCodecLowLatencyMode; ///< Use low latency mode for hardware XMA decoding (default is false).  If true, decoding jobs are submitted at the beginning of the Wwise update and it will be necessary to wait for the result.

	bool				bEnableSpatialAudio;	///< Enable to permit utilisation of Spatial Audio. Note that if audio devices were enumerated before SoundEngine::init, you will have to call the XB1 API "EnableSpatialAudio" before that enumeration.

	AkAudioAPI			eAudioAPI;				///< Main audio API to use. Leave to AkAPI_Default for the default sink (WASAPI).
												///< If a valid audioDeviceShareset plug-in is provided, the AkAudioAPI will be ignored.	
};

namespace AK
{
	/// Finds the device ID for particular Audio Endpoint.  
	/// \return The device ID.
	AK_EXTERNAPIFUNC( AkUInt32, GetDeviceID ) (IMMDevice* in_pDevice);

	/// Finds an audio endpoint that matches the token in the device name or device ID and returns an ID.  
	/// This is a helper function that searches in the device ID (as returned by IMMDevice->GetId) and IMMXboxDevice->GetPnpId()
	/// If you need to do matching on different conditions, use IMMXboxDeviceEnumerator directly.
	/// \return The device ID as returned by IMMDevice->GetId, hashed by AK::SoundEngine::GetIDFromName()
	AK_EXTERNAPIFUNC( AkUInt32, GetDeviceIDFromName )(wchar_t* in_szToken);
};
