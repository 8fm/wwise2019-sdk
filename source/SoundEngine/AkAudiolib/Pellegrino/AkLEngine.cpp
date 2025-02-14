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

#include "AkAjm.h"
#include "AkAjmScheduler.h"
#include "AkSrcMediaCodecAt9.h"
#include "AkOutputMgr.h"
#include "AkAudioMgr.h"

#include <libsysmodule.h>

extern CAkLock g_csMain;

extern AkInitSettings g_settings;
extern AkPlatformInitSettings g_PDSettings;

// Helper for posting errors from APIs
#if !defined(AK_OPTIMIZED)
#define AKLENGINE_POST_MONITOR_ERROR(...) {\
	AkOSChar msg[256]; \
	AK_OSPRINTF(msg, sizeof(msg)/sizeof(AkOSChar), __VA_ARGS__); \
	MONITOR_ERRORMSG(msg); \
}
#else
#define AKLENGINE_POST_MONITOR_ERROR(...)
#endif

//-----------------------------------------------------------------------------
// Implementation of Pellegrino platform context
//-----------------------------------------------------------------------------

namespace AK
{
	namespace SoundEngine 
	{
		AKRESULT WaitSoundEngineInit();
	}

	extern SceAudioOut2PortHandle GetAudioOut2PortHandle(AkOutputDeviceID deviceId)
	{		
		if (AK::SoundEngine::IsInitialized())
		{			
			if (AK::SoundEngine::WaitSoundEngineInit() != AK_Success)
			{
				AKASSERT(!"Init.bnk was not loaded prior to GetAudioOut2PortHandle call.");
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
					CAkSinkPellegrino* pSink = (CAkSinkPellegrino*)pDevice->Sink();
					return pSink->GetAudioOut2PortHandle();
				}
			}
		}
		return SCE_AUDIO_OUT2_PORT_HANDLE_INVALID;
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
		
		// Pellegrino natively supports spatial audio
		AKRESULT GetDeviceSpatialAudioSupport(AkUInt32 in_idDevice)
		{
			return AK_Success;
		}
	}
};

//-----------------------------------------------------------------------------
// Implementation of Pellegrino platform context
//-----------------------------------------------------------------------------
extern CAkAudioMgr*	g_pAudioMgr;

class CAkPellegrinoContext : public AK::IAkPellegrinoContext
{
public:
	CAkPellegrinoContext();
	virtual ~CAkPellegrinoContext();

	virtual SceAudioOut2ContextHandle GetAudioOutContextHandle();
	AKRESULT ExecuteAudioOutContextUpdate();

private:
	AKRESULT InitAudioOutContext();
	void TermAudioOutContext();

	SceAudioOut2ContextHandle m_hCtx;   // Handle to the active audioOut2 context
	void* m_pCtxBuffer;                 // Memory allocated for the context
};

CAkPellegrinoContext::CAkPellegrinoContext()
	: m_hCtx(SCE_AUDIO_OUT2_CONTEXT_HANDLE_INVALID)
	, m_pCtxBuffer(NULL)
{
}

CAkPellegrinoContext::~CAkPellegrinoContext()
{
	TermAudioOutContext();
}

// Returns the sceAudioOut2ContextHandle to be used when creating sinks that work with the sound engine
// To be used especially for the creation of ports
SceAudioOut2ContextHandle CAkPellegrinoContext::GetAudioOutContextHandle()
{
	if (m_hCtx != SCE_AUDIO_OUT2_CONTEXT_HANDLE_INVALID)
	{
		return m_hCtx;
	}

	// Initialize the shared context
	if (InitAudioOutContext() == AK_Success)
	{
		return m_hCtx;
	}

	// Init failed, so term it (i.e. clean up anything that may be init'd)
	TermAudioOutContext();
	return SCE_AUDIO_OUT2_CONTEXT_HANDLE_INVALID;
}

// Internal initialization for data related to the sceAudioOut2Context
AKRESULT CAkPellegrinoContext::InitAudioOutContext()
{
	SceAudioOut2ContextParam ctxParam;
	AkInt32 ret = sceAudioOut2ContextResetParam(&ctxParam);
	if (ret != SCE_OK)
	{
		AKLENGINE_POST_MONITOR_ERROR("%s: Failed sceAudioOut2ContextResetParam: %x", __func__, ret);
		return AK_Fail;
	}

	ctxParam.maxObjectPorts = g_PDSettings.uNumAudioOut2ObjectPorts;
	ctxParam.guaranteeObjectPorts = 0;
	ctxParam.maxPorts = g_PDSettings.uNumAudioOut2Ports;
	ctxParam.numGrains = g_settings.uNumSamplesPerFrame;
	ctxParam.queueDepth = g_PDSettings.uNumRefillsInVoice;
	ctxParam.flags = SCE_AUDIO_OUT2_CONTEXT_PARAM_FLAG_MAIN;
	memset(ctxParam.reserved, 0, sizeof(ctxParam.reserved));

	size_t ctxBufferSize = 0;
	ret = sceAudioOut2ContextQueryMemory(&ctxParam, &ctxBufferSize);
	if (ret != SCE_OK)
	{
		AKLENGINE_POST_MONITOR_ERROR("%s: Failed sceAudioOut2ContextQueryMemory with maxObjectPorts %d, maxPorts %d, numGrains %d, queueDepth %d: %x",
			__func__, ctxParam.maxObjectPorts, ctxParam.maxPorts, ctxParam.numGrains, ctxParam.queueDepth, ret);
		return AK_Fail;
	}

	m_pCtxBuffer = AkAlloc(AkMemID_SoundEngine, ctxBufferSize);
	if (!m_pCtxBuffer)
	{
		return AK_InsufficientMemory;
	}

	ret = sceAudioOut2ContextCreate(&ctxParam, m_pCtxBuffer, ctxBufferSize, &m_hCtx);
	if (ret != SCE_OK)
	{
		AKLENGINE_POST_MONITOR_ERROR("%s: Failed sceAudioOut2ContextCreate with maxObjectPorts %d, maxPorts %d, numGrains %d, queueDepth %d: %d",
			__func__, ctxParam.maxObjectPorts, ctxParam.maxPorts, ctxParam.numGrains, ctxParam.queueDepth, ret);
		return AK_Fail;
	}

	return AK_Success;
}

// Internal termination for data related to the sceAudioOut2Context
void CAkPellegrinoContext::TermAudioOutContext()
{
	// destroy the context
	if (m_hCtx != SCE_AUDIO_OUT2_CONTEXT_HANDLE_INVALID)
	{
		AkInt32 ret = sceAudioOut2ContextDestroy(m_hCtx);
		if (ret != SCE_OK)
		{
			AKLENGINE_POST_MONITOR_ERROR("%s: Failed sceAudioOut2ContextDestroy: %x", __func__, ret);
		}
	}
	if (m_pCtxBuffer)
	{
		AkFree(AkMemID_SoundEngine, m_pCtxBuffer);
	}
}

// Conditionally advances the sceAudioOut2Context,
// and notifies the ContextPushThreadFunc that it is ready to be pushed
AKRESULT CAkPellegrinoContext::ExecuteAudioOutContextUpdate()
{
	if (m_hCtx != SCE_AUDIO_OUT2_CONTEXT_HANDLE_INVALID)
	{
		AkInt32 ret = sceAudioOut2ContextAdvance(m_hCtx);
		if (ret != SCE_OK)
		{
			AKLENGINE_POST_MONITOR_ERROR("%s: Failed sceAudioOut2ContextAdvance: %x", __func__, ret);
			return AK_Fail;
		}

		ret = sceAudioOut2ContextPush(m_hCtx, SCE_AUDIO_OUT2_BLOCKING_ASYNC);
		if (ret != SCE_OK)
		{
			AKLENGINE_POST_MONITOR_ERROR("%s: Failed sceAudioOut2ContextAdvance: %x", __func__, ret);
			return AK_Fail;
		}
	}
	return AK_Success;
}

//-----------------------------------------------------------------------------
//Static variables.
//-----------------------------------------------------------------------------
AkEvent CAkLEngine::m_EventStop;
AK::IAkPlatformContext*	CAkLEngine::m_pPlatformContext = NULL;

void CAkLEngine::GetDefaultPlatformInitSettings( 
	AkPlatformInitSettings &      out_pPlatformSettings      // Platform specific settings. Can be changed depending on hardware.
	)
{
	memset( &out_pPlatformSettings, 0, sizeof( AkPlatformInitSettings ) );

	GetDefaultPlatformThreadInitSettings(out_pPlatformSettings);

	out_pPlatformSettings.uNumRefillsInVoice = AK_DEFAULT_NUM_REFILLS_IN_VOICE_BUFFER;

	out_pPlatformSettings.bStrictAtrac9Aligment = true;
	out_pPlatformSettings.bHwCodecLowLatencyMode = true;


	out_pPlatformSettings.uNumAudioOut2Ports = 16;
	out_pPlatformSettings.uNumAudioOut2ObjectPorts = 0;
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
		CreateATRAC9SrcMedia,
		CreateATRAC9SrcMedia );

	AkAudioLibSettings::SetAudioBufferSettings(AK_CORE_SAMPLERATE, g_settings.uNumSamplesPerFrame);

	AKRESULT eResult = AK_Success;
	
	// sceAudioOut2 Initialization
	AkInt32 ret = sceAudioOut2Initialize();
	if (ret != SCE_OK && ret != SCE_AUDIO_OUT2_ERROR_ALREADY_INITIALIZED)
	{
		AKLENGINE_POST_MONITOR_ERROR("%s: Failed sceAudioOut2Initialize: %x", __func__, ret);
		eResult = AK_Fail;
	}

	eResult = SoftwareInit();
	if( eResult != AK_Fail )
	{
		eResult = CAkGlobalAjmScheduler::Init();
	}

	if (CAkSinkPellegrino::IsBGMOverriden())
	{
		CAkBus::MuteBackgroundMusic();
	}

	return eResult;
} // Init

AKRESULT CAkLEngine::InitPlatformContext()
{
	// Initialize Acm
	AkInt32 ret = sceSysmoduleLoadModule(SCE_SYSMODULE_ACM);
	if (ret != SCE_OK)
	{
		AKLENGINE_POST_MONITOR_ERROR("%s: Failed sceSysmoduleLoadModule for SCE_SYSMODULE_ACM: %x", __func__, ret);
		return AK_Fail;
	}

	m_pPlatformContext = AkNew(AkMemID_SoundEngine, CAkPellegrinoContext());
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
	CAkGlobalAjmScheduler::Term();
	SoftwareTerm();
} // Term

void CAkLEngine::TermPlatformContext()
{
	// Initialize Acm
	AkInt32 ret = sceSysmoduleUnloadModule(SCE_SYSMODULE_ACM);
	if (ret != SCE_OK)
	{
		AKLENGINE_POST_MONITOR_ERROR("%s: Failed sceSysmoduleUnloadModule for SCE_SYSMODULE_ACM: %x", __func__, ret);
	}

	AkDelete(AkMemID_SoundEngine, m_pPlatformContext);
	m_pPlatformContext = NULL;
}

//-----------------------------------------------------------------------------
// Name: Perform
// Desc: Perform all VPLs.
//-----------------------------------------------------------------------------
void CAkLEngine::Perform()
{
	AkUInt32 uFlushZeroMode = _MM_GET_FLUSH_ZERO_MODE();
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);

	SoftwarePerform();

	if (CAkPellegrinoContext* pContext = static_cast<CAkPellegrinoContext*>(m_pPlatformContext))
	{
		pContext->ExecuteAudioOutContextUpdate();
	}

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

void CAkLEngine::DecodeHardwareSources()
{
	CAkGlobalAjmScheduler::Get().SubmitJobs();
}

void CAkLEngine::PlatformWaitForHwVoices()
{
	// In multicore mode, we want to make sure that the AJM batches are done before we process HW sources
	CAkGlobalAjmScheduler::Get().WaitForJobs();
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
