/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2019 Audiokinetic Inc.
 ***********************************************************************/

//////////////////////////////////////////////////////////////////////
//
// AkLEngine.cpp
//
// Implementation of the IAkLEngine interface. Win32 version.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkLEngine.h"

#include "Ak3DListener.h"
#include "AkAudioLibTimer.h"
#include "AkSink.h"
#include "AkProfile.h"
#include "AkSpeakerPan.h"
#include "AkEffectsMgr.h"
#include "xmmintrin.h"
#include "AkBus.h"
#include "AkAudioLib.h"
#include "AkURenderer.h"

#include "AkPositionRepository.h"

#include "AkSrcBankAt9.h"
#include "AkSrcFileAt9.h"
#include "AkOutputMgr.h"

#include <user_service/user_service_api.h>

extern CAkLock g_csMain;

extern AkInitSettings g_settings;
extern AkPlatformInitSettings g_PDSettings;

#define LENGINE_ACP_BATCH_BUFFER_SIZE	(90 * 1024)

namespace AK
{
	namespace SoundEngine 
	{
		AKRESULT WaitSoundEngineInit();
	}

	int GetPS4OutputHandle(AkOutputDeviceID deviceId)
	{		
		if (AK::SoundEngine::IsInitialized())
		{			
			if (AK::SoundEngine::WaitSoundEngineInit() != AK_Success)
			{
				AKASSERT(!"Init.bnk was not loaded prior to GetPS4OutputHandle call.");
				return -1;
			}

			AkAutoLock<CAkLock> gate(g_csMain);
			AkDevice *pDevice = CAkOutputMgr::GetDevice(deviceId);
			if (pDevice && pDevice->GetState() == AkDevice::eActive && pDevice->Sink() != NULL)
			{
				AkPluginID uSinkPluginID = pDevice->uSinkPluginID;
				if (uSinkPluginID == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_DEFAULT_SINK) ||
					uSinkPluginID == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_DVR_SINK) ||
					uSinkPluginID == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_COMMUNICATION_SINK) ||
					uSinkPluginID == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_PERSONAL_SINK) ||
					uSinkPluginID == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_PAD_SINK) ||
					uSinkPluginID == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_AUX_SINK))
				{
					CAkSinkPS4* pSink = (CAkSinkPS4*)pDevice->Sink();
					return pSink->GetPS4OutputHandle();
				}
			}
		}

		return -1;		
	}

	namespace SoundEngine
	{
		AKRESULT RegisterPluginDLL(const AkOSChar* in_DllName, const AkOSChar* in_DllPath /* = NULL */)
		{
			AkOSChar fullName[1024];
			fullName[0] = 0;
			CAkLEngine::GetPluginDLLFullPath(fullName, 1024, in_DllName, in_DllPath);

			SceKernelModule hLib = sceKernelLoadStartModule(fullName, 0, NULL, 0, NULL, NULL);
			if (hLib < 0)
				return AK_FileNotFound;

			AK::PluginRegistration** RegisterList = NULL;
			int err = sceKernelDlsym(hLib, "g_pAKPluginList", (void**)&RegisterList);
			if (err == 0 && RegisterList != NULL)
				return CAkEffectsMgr::RegisterPluginList(*RegisterList);

			return AK_InvalidFile;
		}
		
		// PS4 natively supports spatial audio
		AKRESULT GetDeviceSpatialAudioSupport(AkUInt32 in_idDevice)
		{
			return AK_Success;
		}
	}
};

//-----------------------------------------------------------------------------
//Static variables.
//-----------------------------------------------------------------------------
AkEvent						CAkLEngine::m_EventStop;
AK::IAkPlatformContext*	CAkLEngine::m_pPlatformContext = NULL;

class CAkPS4Context: public AK::IAkPS4Context
{
};

void CAkLEngine::GetDefaultPlatformInitSettings( 
	AkPlatformInitSettings &      out_pPlatformSettings      // Platform specific settings. Can be changed depending on hardware.
	)
{
	memset( &out_pPlatformSettings, 0, sizeof( AkPlatformInitSettings ) );

	GetDefaultPlatformThreadInitSettings(out_pPlatformSettings);

	out_pPlatformSettings.uLEngineAcpBatchBufferSize = LENGINE_ACP_BATCH_BUFFER_SIZE;
	out_pPlatformSettings.uNumRefillsInVoice = AK_DEFAULT_NUM_REFILLS_IN_VOICE_BUFFER;
	//out_pPlatformSettings.uSampleRate = 48000; //DEFAULT_NATIVE_FREQUENCY;

	out_pPlatformSettings.bHwCodecLowLatencyMode = false;
	out_pPlatformSettings.bStrictAtrac9Aligment = true;
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
	AK::SoundEngine::RegisterCodec(
		AKCOMPANYID_AUDIOKINETIC,
		AKCODECID_ATRAC9,
		CreateATRAC9FilePlugin,
		CreateATRAC9BankPlugin );

	AkAudioLibSettings::SetAudioBufferSettings(AK_CORE_SAMPLERATE, g_settings.uNumSamplesPerFrame);

	AKRESULT eResult = SoftwareInit();
	if( eResult != AK_Fail )
	{
		eResult = CAkACPManager::Get().Init();
	}

	if (CAkSinkPS4::IsBGMOverriden())
		CAkBus::MuteBackgroundMusic();
	return eResult;
} // Init

AKRESULT CAkLEngine::InitPlatformContext()
{
	m_pPlatformContext = AkNew(AkMemID_SoundEngine, CAkPS4Context());
	if (!m_pPlatformContext)
		return AK_InsufficientMemory;
	return AK_Success;
}

//-----------------------------------------------------------------------------
// Name: Term
// Desc: Terminate the object.
//
// Parameters:
//	None.
//
// Return:
//	Ak_Success: Object was terminated correctly.
//  AK_Fail:    Failed to terminate correctly.
//-----------------------------------------------------------------------------
void CAkLEngine::Term()
{
	CAkACPManager::Get().Term();
	SoftwareTerm();
} // Term

void CAkLEngine::TermPlatformContext()
{
	AkDelete(AkMemID_SoundEngine, m_pPlatformContext);
	m_pPlatformContext = NULL;
}

//-----------------------------------------------------------------------------
// Name: Perform
// Desc: Perform all VPLs.
//-----------------------------------------------------------------------------
void CAkLEngine::Perform()
{
	WWISE_SCOPED_PROFILE_MARKER( "CAkLEngine::Perform" );

	AkUInt32 uFlushZeroMode = _MM_GET_FLUSH_ZERO_MODE();
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);

	if (g_PDSettings.bHwCodecLowLatencyMode)
		CAkACPManager::Get().SubmitDecodeBatch();

    //In case we were not able to submit all the decodes in 1 batch, we need to finish of any extraneous batches here.
	while (CAkACPManager::Get().NeedAnotherPass())
		CAkACPManager::Get().SubmitDecodeBatch();
	
	HandleStarvation();

	//If the engine was suspended without rendering, avoid doing so
	bool bRender = CAkOutputMgr::RenderIsActive();

	g_pPositionRepository->UpdateTime();

	CAkLEngineCmds::ProcessDisconnectedSources(LE_MAX_FRAMES_PER_BUFFER);
	
	AnalyzeMixingGraph();
	CAkURenderer::ProcessLimiters();// Has to be executed right after the Mix graph was analyzed so we know who was possibly audible.
	
	if (g_settings.taskSchedulerDesc.fcnParallelFor)
	{
		// Newer execution model, compatible with parallel_for
		ProcessGraph(bRender);
		//Submit any outstanding ATRAC9 decode jobs.
		if (!g_PDSettings.bHwCodecLowLatencyMode)
			CAkACPManager::Get().SubmitDecodeBatch();
	}
	else
	{
		// Older execution model, optimized for single-thread execution
		ProcessSources(bRender);
		//Submit any outstanding ATRAC9 decode jobs.
		if (!g_PDSettings.bHwCodecLowLatencyMode)
			CAkACPManager::Get().SubmitDecodeBatch();
		PerformMixing(bRender);
	}

#ifndef AK_OPTIMIZED
	AkPerf::PostPipelineStats();
#endif

	// End of frame
	for (AkDeviceList::Iterator it = CAkOutputMgr::OutputBegin(); it != CAkOutputMgr::OutputEnd(); ++it)
		(*it)->FrameEnd();

	CAkOutputMgr::ResetMasterVolume(CAkOutputMgr::GetMasterVolume().fNext);	//Volume is now at target.

	RemoveMixBusses();

	_MM_SET_FLUSH_ZERO_MODE(uFlushZeroMode);
} // Perform

void CAkLEngine::GetDefaultOutputSettings( AkOutputSettings & out_settings )
{
	//Default to 5.1
	GetDefaultOutputSettingsCommon(out_settings);
}

bool CAkLEngine::PlatformSupportsHwVoices()
{
	return true;
}

void CAkLEngine::PlatformWaitForHwVoices()
{
}

AKRESULT CAkLEngine::GetPlatformDeviceList(
	AkPluginID in_pluginID,
	AkUInt32& io_maxNumDevices,					///< In: The maximum number of devices to read. Must match the memory allocated for AkDeviceDescription. Out: Returns the number of devices. Pass out_deviceDescriptions as NULL to have an idea of how many devices to expect.
	AkDeviceDescription* out_deviceDescriptions	///< The output array of device descriptions, one per device. Must be preallocated by the user with a size of at least io_maxNumDevices*sizeof(AkDeviceDescription).
)
{
	// Querying for the number of devices
	if (out_deviceDescriptions == NULL)
	{
		if ((in_pluginID == AKPLUGINID_DEFAULT_SINK) ||
			(in_pluginID == AKPLUGINID_DVR_SINK) ||
			(in_pluginID == AKPLUGINID_AUX_SINK))
		{
			io_maxNumDevices = 1;
			return AK_Success;
		}
		else
		{
			io_maxNumDevices = SCE_USER_SERVICE_MAX_LOGIN_USERS;
			return AK_Success;
		}
	}

	if (io_maxNumDevices == 0)
		return AK_Success;
	
	AkUInt32 deviceIndex = 0;

	if ((in_pluginID == AKPLUGINID_DEFAULT_SINK) ||
		(in_pluginID == AKPLUGINID_DVR_SINK) ||
		(in_pluginID == AKPLUGINID_AUX_SINK))
	{
		// System output
		out_deviceDescriptions[deviceIndex].idDevice = (AkUInt32)SCE_USER_SERVICE_USER_ID_SYSTEM;
		out_deviceDescriptions[deviceIndex].deviceStateMask = AkDeviceState_Active;
		AKPLATFORM::SafeStrCpy(out_deviceDescriptions[deviceIndex].deviceName, "System", AK_MAX_PATH);
		++deviceIndex;
	}
	else if (in_pluginID == AKPLUGINID_COMMUNICATION_SINK ||
			 in_pluginID == AKPLUGINID_PERSONAL_SINK ||
			 in_pluginID == AKPLUGINID_PAD_SINK)
	{
		// User specific outputs
		SceUserServiceLoginUserIdList userIdList;
		if (sceUserServiceGetLoginUserIdList(&userIdList) >= 0)
		{
			char username[SCE_USER_SERVICE_MAX_USER_NAME_LENGTH + 1];
			for (int i = 0; i < SCE_USER_SERVICE_MAX_LOGIN_USERS && deviceIndex < io_maxNumDevices; i++)
			{
				SceUserServiceUserId userId = userIdList.userId[i];
				if (userId == SCE_USER_SERVICE_USER_ID_INVALID)
					continue;

				int32_t ret = sceUserServiceGetUserName(userId, username, sizeof(username));
				if (ret != SCE_OK)
					continue;

				out_deviceDescriptions[deviceIndex].idDevice = (AkUInt32)userId;
				out_deviceDescriptions[deviceIndex].deviceStateMask = AkDeviceState_Active;
				AKPLATFORM::SafeStrCpy(out_deviceDescriptions[deviceIndex].deviceName, username, AK_MAX_PATH);
				++deviceIndex;
			}
		}
	}
	else
	{
		return AK_NotCompatible;
	}

	io_maxNumDevices = deviceIndex;
	return AK_Success;
}

