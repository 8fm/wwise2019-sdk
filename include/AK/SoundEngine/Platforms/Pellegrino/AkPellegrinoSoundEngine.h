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
  Copyright (c) 2006-2019 Audiokinetic Inc.
*******************************************************************************/

/// \file 
/// Main Sound Engine interface, Pellegrino specific.

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <audio_out2.h>

/// Platform specific initialization settings
/// \sa AK::SoundEngine::Init
/// \sa AK::SoundEngine::GetDefaultPlatformInitSettings
/// - \ref soundengine_initialization_advanced_soundengine_using_memory_threshold
struct AkPlatformInitSettings
{
    // Threading model.
    AkThreadProperties  threadLEngine;			///< Lower engine threading properties
	AkThreadProperties  threadOutputMgr;		///< Ouput thread threading properties
	AkThreadProperties  threadBankManager;		///< Bank manager threading properties (its default priority is AK_THREAD_PRIORITY_NORMAL)
	AkThreadProperties  threadMonitor;			///< Monitor threading properties (its default priority is AK_THREAD_PRIORITY_ABOVENORMAL). This parameter is not used in Release build.

    // Memory.
    AkUInt32            uLEngineDefaultPoolSize;///< Lower Engine default memory pool size

	// Voices.
	AkUInt16            uNumRefillsInVoice;		///< The queueDepth of the audioOut2Context. May need to be increased if audio starvation occurs, at the cost of greater audio latency. 2 == double-buffered, defaults to 4.	

	bool                bStrictAtrac9Aligment;	///< Forces checks for ATRAC9 alignment in banks.
	bool                bHwCodecLowLatencyMode;  ///< Use low latency mode for hardware codecs such as ATRAC9.  If true, decoding jobs are submitted at the beginning of the Wwise update and it will be necessary to wait for the result. Defaults to true.

	// AudioOut2 setup
	AkUInt32            uNumAudioOut2Ports;		///< The number of ports to initialize the audioOut2Context with. May need to be increased if using many sinks
	AkUInt32            uNumAudioOut2ObjectPorts;	///< The number of object ports to initialize the audioOut2Context with. Will need to be increased depending on sceAudio3D configuration
};

namespace AK
{
	/// Returns the current SceAudioOut2PortHandle being used by the Wwise SoundEngine for main output.
	/// This should be called only once the SoundEngine has been successfully initialized, otherwise
	/// the function will return an invalid value (SCE_AUDIO_OUT2_PORT_HANDLE_INVALID).
	///
	/// Use \ref RegisterAudioDeviceStatusCallback to get notified when devices are created/destructed.
	///
	/// \return the current sceAudioOut2 main output port handle or SCE_AUDIO_OUT2_PORT_HANDLE_INVALID.
	extern SceAudioOut2PortHandle GetAudioOut2PortHandle(AkOutputDeviceID deviceId = 0);
};
