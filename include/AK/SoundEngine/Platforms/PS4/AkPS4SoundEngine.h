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
/// Main Sound Engine interface, PS4 specific.

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

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

	// (SCE_AJM_JOB_INITIALIZE_SIZE*MAX_INIT_SOUND_PER_FRAME) + (SCE_AJM_JOB_RUN_SPLIT_SIZE(4)*MAX_BANK_SRC + (SCE_AJM_JOB_RUN_SPLIT_SIZE(5)*MAX_FILE_SRC
	AkUInt32            uLEngineAcpBatchBufferSize; ///< ACP batch buffer size: used for ATRAC9 decoding.
	bool				bHwCodecLowLatencyMode; ///< Use low latency mode for ATRAC9  (default is false).  If true, decoding jobs are submitted at the beginning of the Wwise update and it will be necessary to wait for the result.

	// Voices.
	AkUInt16            uNumRefillsInVoice;		///< Number of refill buffers in voice buffer. 2 == double-buffered, defaults to 4.	

	bool				bStrictAtrac9Aligment;	///< Forces checks for ATRAC9 alignment in banks.
};

namespace AK
{
	/// Returns the PS4 output port handle being used by the Wwise SoundEngine for the output matching
	/// the provided OutputDeviceId. This should be called only once the SoundEngine has been successfully
	/// initialized, otherwise the function will return an invalid value (-1). Invalid values will also
	/// be returned if the OutputDeviceId matches a device that is not an output for the Default, DVR,
	/// Communication, Personal, Pad_output, or Aux devices. 
	///
	/// Use <tt>AK::SoundEngine::RegisterAudioDeviceStatusCallback()</tt> to get notified when devices are created/destructed.
	///
	/// \return the current PS4 main output port handle or -1.

	extern int GetPS4OutputHandle(AkOutputDeviceID deviceId = 0);
};
