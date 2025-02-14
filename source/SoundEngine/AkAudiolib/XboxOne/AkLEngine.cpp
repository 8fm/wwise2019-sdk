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

#include <AK/Tools/Win32/AkCOMScope.h>

#include "Ak3DListener.h"
#include "AkAudioLibTimer.h"
#include "AkAudioMgr.h"
#include "AkSink.h"
#include "AkProfile.h"
#include "AkSpeakerPan.h"
#include "AkEffectsMgr.h"
#include "AkOutputMgr.h"
#include "AkAudioLib.h"
#include "AkACPMgr.h"
#include "AkURenderer.h"
#include "AkPositionRepository.h"
#ifdef AK_USE_UWP_API
#include <combaseapi.h>
#endif

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <ks.h>
#include <ksmedia.h>
#include <../um/SpatialAudioClient.h>

//-----------------------------------------------------------------------------
//Static variables.
//-----------------------------------------------------------------------------
extern AkInitSettings		g_settings;
extern AkPlatformInitSettings g_PDSettings;
AkEvent						CAkLEngine::m_EventStop;
static bool					s_bCoInitializeSucceeded = false;
HANDLE						CAkLEngine::m_eventProcess = NULL;
AK::IAkPlatformContext*	CAkLEngine::m_pPlatformContext = NULL;

extern CAkAudioMgr*		g_pAudioMgr;

IAkSoftwareCodec* CreateXMAFilePlugin(void* in_pCtx);
IAkSoftwareCodec* CreateXMABankPlugin(void* in_pCtx);

class CAkXboxOneContext: public AK::IAkXboxOneContext
{
};

IMMDeviceCollection *GetDeviceList();

namespace AK
{	
	namespace SoundEngine
	{
		AKRESULT RegisterPluginDLL(const AkOSChar* in_DllName, const AkOSChar* in_DllPath /* = NULL */)
		{
			AkOSChar fullName[1024];
			fullName[0] = 0;
			CAkLEngine::GetPluginDLLFullPath(fullName, 1024, in_DllName, in_DllPath);

			HMODULE hLib = ::LoadLibrary(fullName);
			if (hLib == NULL)
				return AK_FileNotFound;

			AK::PluginRegistration** RegisterList = (AK::PluginRegistration**)::GetProcAddress(hLib, "g_pAKPluginList");
			if (RegisterList != NULL)
				return CAkEffectsMgr::RegisterPluginList(*RegisterList);

			return AK_InvalidFile;
		}
		
		AKRESULT GetDeviceSpatialAudioSupport(AkUInt32 in_idDevice)
		{
			IMMDeviceCollection* pCol = ::GetDeviceList();
			if (pCol == nullptr)
				return AK_Fail;

			HRESULT hr = S_OK;

			UINT deviceCount = 0;
			hr = pCol->GetCount(&deviceCount);
			if (hr != S_OK)
			{
				pCol->Release();
				return AK_Fail;
			}

			IMMDevice* pDevice = nullptr;
			for (UINT i = 0; i < deviceCount; ++i)
			{
				hr = pCol->Item(i, &pDevice);
				if (hr != S_OK || pDevice == nullptr)
				{
					pCol->Release();
					return AK_Fail;
				}

				if (AK::GetDeviceID(pDevice) == in_idDevice)
				{
					break;
				}
				else
				{
					pDevice->Release();
					pDevice = nullptr;
				}
			}
			pCol->Release();
			if (pDevice == nullptr)
			{
				return AK_Fail;
			}

			ISpatialAudioClient* pSpatialAudioClient;
			hr = pDevice->Activate(__uuidof(ISpatialAudioClient),
				CLSCTX_INPROC_SERVER, nullptr, reinterpret_cast<void**>(&pSpatialAudioClient));
			pDevice->Release();
			if (hr != S_OK || pSpatialAudioClient == nullptr)
				return AK_NotCompatible;

			UINT32 maxObjects;
			hr = pSpatialAudioClient->GetMaxDynamicObjectCount(&maxObjects);
			pSpatialAudioClient->Release();
			if (hr != S_OK || maxObjects == 0)
				return AK_NotCompatible;

			return AK_Success;
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
	out_pPlatformSettings.uMaxXMAVoices = 128;

	out_pPlatformSettings.eAudioAPI = AkAPI_Default;	
	out_pPlatformSettings.bHwCodecLowLatencyMode = false;

	out_pPlatformSettings.bEnableSpatialAudio = false; // Recommendation from Microsoft to not enable spatial audio unless application wants it
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
	// Necessary for DirectSound / Vista audio
	s_bCoInitializeSucceeded = SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED));

	if ( AkCreateEvent( m_eventProcess ) != AK_Success )
		return AK_Fail;

	AK::SoundEngine::RegisterCodec(
		AKCOMPANYID_AUDIOKINETIC,
		AKCODECID_XMA,
		CreateXMAFilePlugin,
		CreateXMABankPlugin);

	if (g_PDSettings.bEnableSpatialAudio)
	{
		HRESULT hr = EnableSpatialAudio();
		if (FAILED(hr))
		{
			char szMsg[250];
			snprintf(szMsg, sizeof(szMsg) / sizeof(char), "EnableSpatialAudio failed during engine init: %08x\n", hr);
			AKPLATFORM::OutputDebugMsg(szMsg);
		}
	}

	if(g_PDSettings.threadLEngine.dwAffinityMask == 0)
		g_PDSettings.threadLEngine.dwAffinityMask = 0x3F;	// Default affinity for audio thread is logical cores 0-5.

	g_PDSettings.threadLEngine.dwAffinityMask &= 0x3F;		// Acceptable cores for audio thread are limited to logical cores 0-5.
	if(g_PDSettings.threadLEngine.dwAffinityMask == 0)
	{
		AKPLATFORM::OutputDebugMsg("Invalid affinity mask for Wwise audio thread; please use a core within [0,5]");
		AKASSERT(!"Invalid affinity mask for Wwise audio thread");
		return AK_Fail;
	}

	AkAudioLibSettings::SetAudioBufferSettings(AK_CORE_SAMPLERATE, g_settings.uNumSamplesPerFrame);
	
	AKRESULT eResult = SoftwareInit();
	if (eResult == AK_Success)
	{
		if ( g_PDSettings.uMaxXMAVoices > 0 )
			eResult = AkACPMgr::Get().Init();
	}

	return eResult;
} // Init

AKRESULT CAkLEngine::InitPlatformContext()
{
	m_pPlatformContext = AkNew(AkMemID_SoundEngine, CAkXboxOneContext());
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
	AkACPMgr::Get().Term();

	SoftwareTerm();

	if (s_bCoInitializeSucceeded)
	{
		CoUninitialize(); // Necessary for DirectSound / Vista audio
		s_bCoInitializeSucceeded = false;
	}

	AkDestroyEvent(m_eventProcess);
	m_eventProcess = NULL;
}

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

	// Avoid denormal problems in audio processing
#if defined AK_CPU_X86 || defined AK_CPU_X86_64
#if defined (_MSC_VER) && (_MSC_VER < 1700)
	AkUInt32 uFlushZeroMode = _MM_GET_FLUSH_ZERO_MODE(dummy);
#else
	AkUInt32 uFlushZeroMode = _MM_GET_FLUSH_ZERO_MODE();
#endif
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif

	//Calling AkACPMgr::Get().Update() before SoftwarePerform() will result in lower latency for HW voices, however the audio thread will have to block at some pont 
	//	to wait for the data to be ready.
	if (g_PDSettings.bHwCodecLowLatencyMode)
		AkACPMgr::Get().Update();

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
		//Calling AkACPMgr::Get().Update() after SoftwarePerform() will result in the highest possible throughput for HW voices because HW and sowftware processing is staggared, 
		//	however an extra audio frame of latency will be incurred.
		if (!g_PDSettings.bHwCodecLowLatencyMode)
			AkACPMgr::Get().Update();
	}
	else
	{
		ProcessSources(bRender);

		//Calling AkACPMgr::Get().Update() after SoftwarePerform() will result in the highest possible throughput for HW voices because HW and sowftware processing is staggared, 
		//	however an extra audio frame of latency will be incurred.
		if (!g_PDSettings.bHwCodecLowLatencyMode)
			AkACPMgr::Get().Update();
		
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

#if defined AK_CPU_X86 || defined AK_CPU_X86_64
	_MM_SET_FLUSH_ZERO_MODE(uFlushZeroMode);
#endif
} // Perform

void CAkLEngine::GetDefaultOutputSettings( AkOutputSettings & out_settings )
{
	GetDefaultOutputSettingsCommon(out_settings);
}

bool CAkLEngine::PlatformSupportsHwVoices()
{
	return true;
}

void CAkLEngine::PlatformWaitForHwVoices()
{
	if (!g_PDSettings.bHwCodecLowLatencyMode)
		AkACPMgr::Get().Update();
}

// Returns the device name as described in GetPnpId documentation.
// Device name matches this form: 
//   DeviceType[#AdditionInfo1][#AdditionalInfo2][#VID_*][#PID_*][#{Device specific GUID}\global]  
bool GetDeviceName(IMMDevice* in_pDevice, wchar_t* io_name, AkUInt32 in_maxNameSize, AkUInt32 &out_uDeviceID)
{
	bool res = false;

	if (in_pDevice == NULL || io_name == NULL)
		return res;

	// Get the endpoint plug and play ID string.
	const IID IID_IMMXboxDevice = __uuidof(IMMXboxDevice);

	IMMXboxDevice *pEndPoint = NULL;
	in_pDevice->QueryInterface(IID_IMMXboxDevice, (void**)&pEndPoint);
	if (pEndPoint != NULL)
	{
		LPWSTR pwszID = NULL;
		if (pEndPoint->GetPnpId(&pwszID) == S_OK)
		{
			// plug-and-play ID is more a more user-friendly name
			AKPLATFORM::SafeStrCpy(io_name, pwszID, in_maxNameSize);
			CoTaskMemFree(pwszID);

			// but we still use the ID hash for the device ID in the sink
			if (pEndPoint->GetId(&pwszID) == S_OK)
			{
				out_uDeviceID = AK::SoundEngine::GetIDFromString(pwszID);
				CoTaskMemFree(pwszID);
				res = true;
			}
		}

		pEndPoint->Release();
	}

	return res;
}

void GetDefaultDeviceID(ERole in_deviceRole, AkUInt32& out_defaultDeviceID)
{
	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

	IMMDeviceEnumerator * pEnumerator;

	HRESULT hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);

	IMMDevice * pDefaultDevice = NULL;
	if (pEnumerator->GetDefaultAudioEndpoint(eRender, in_deviceRole, &pDefaultDevice) == S_OK)
	{
		out_defaultDeviceID = AK::GetDeviceID(pDefaultDevice);
		pDefaultDevice->Release();
	}

	pEnumerator->Release();
}

AKRESULT CAkLEngine::GetPlatformDeviceList(
	AkPluginID in_pluginID,
	AkUInt32& io_maxNumDevices,					///< In: The maximum number of devices to read. Must match the memory allocated for AkDeviceDescription. Out: Returns the number of devices. Pass out_deviceDescriptions as NULL to have an idea of how many devices to expect.
	AkDeviceDescription* out_deviceDescriptions	///< The output array of device descriptions, one per device. Must be preallocated by the user with a size of at least io_maxNumDevices*sizeof(AkDeviceDescription).
)
{
	if (in_pluginID < AKPLUGINID_DEFAULT_SINK ||
		in_pluginID > AKPLUGINID_PERSONAL_SINK)
	{
		return AK_NotCompatible;
	}

	// Valid for the remaining plug-ins
	AKASSERT(
		in_pluginID == AKPLUGINID_DEFAULT_SINK ||
		in_pluginID == AKPLUGINID_DVR_SINK ||
		in_pluginID == AKPLUGINID_COMMUNICATION_SINK ||
		in_pluginID == AKPLUGINID_PERSONAL_SINK
	);

	IMMDeviceCollection * pCol = GetDeviceList();
	if (pCol == NULL)
	{
		io_maxNumDevices = 0;
		return AK_Fail;
	}

	UINT uNum = 0;
	pCol->GetCount(&uNum);

	// if user passed NULL, return the number of devices
	if (out_deviceDescriptions == NULL)
	{
		pCol->Release();
		io_maxNumDevices = uNum;
		return AK_Success;
	}

	// Fetch the default device
	ERole eRole = in_pluginID == AKPLUGINID_COMMUNICATION_SINK ? eCommunications : eConsole;
	AkUInt32 defaultDeviceID = 0;
	GetDefaultDeviceID(eRole, defaultDeviceID);

	// and fill as many devices as we can
	AkUInt32 deviceIndex = 0;
	for (UINT i = 0; i < uNum && deviceIndex < io_maxNumDevices; i++)
	{
		// Get pointer to endpoint number i.
		IMMDevice *pEndpoint = NULL;
		HRESULT hr = pCol->Item(i, &pEndpoint);
		if (pEndpoint == NULL)
			continue;

		// device state
		DWORD state;
		HRESULT res = pEndpoint->GetState(&state);
		if (res != S_OK)
			continue;

		// device ID and device name
		AkUInt32 deviceID;
		if (!GetDeviceName(pEndpoint, out_deviceDescriptions[deviceIndex].deviceName, AK_MAX_PATH, deviceID))
			continue; // skip devices with no name

		// write to output if device is valid
		out_deviceDescriptions[deviceIndex].idDevice = deviceID;
		out_deviceDescriptions[deviceIndex].deviceStateMask = (AkAudioDeviceState)state;
		out_deviceDescriptions[deviceIndex].isDefaultDevice = (deviceID == defaultDeviceID);

		++deviceIndex;
		pEndpoint->Release();
	}
	pCol->Release();
	io_maxNumDevices = deviceIndex;

	return AK_Success;
}

IMMDeviceCollection *GetDeviceList()
{
	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	const IID IID_IMMXboxDeviceEnumerator = __uuidof(IMMXboxDeviceEnumerator);

	IMMDeviceEnumerator * pEnumerator;
	IMMXboxDeviceEnumerator* pXboxDeviceEnumerator;

	HRESULT hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);

	if (hr != S_OK)
		return NULL;

	IMMDeviceCollection * pReturn = NULL;
	if ( pEnumerator->QueryInterface( __uuidof( IMMXboxDeviceEnumerator ), reinterpret_cast< void** >( &pXboxDeviceEnumerator ) ) == S_OK )
	{			
		pXboxDeviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATEMASK_ALL, &pReturn);
		pXboxDeviceEnumerator->Release();
	}
	pEnumerator->Release();

	return pReturn;
}

AkUInt32 CompareOneDevice(IMMDevice *in_pEndpoint, LPWSTR in_szToken)
{
	LPWSTR pwszID = NULL;
	// Get the endpoint ID string.
	HRESULT hr = in_pEndpoint->GetId(&pwszID);
	if (hr != S_OK)
		return AK_INVALID_DEVICE_ID;

	AkUInt32 id = AK::SoundEngine::GetIDFromString(pwszID);
	wchar_t* pFound = wcsstr(pwszID, in_szToken);
	CoTaskMemFree(pwszID);

	if (pFound != NULL)
		return id;

	const IID IID_IMMXboxDevice = __uuidof(IMMXboxDevice);

	IMMXboxDevice *pEndPoint = NULL;
	in_pEndpoint->QueryInterface(IID_IMMXboxDevice, (void**)&pEndPoint);
	if (pEndPoint != NULL)
	{
		pEndPoint->GetPnpId(&pwszID);
		pFound = wcsstr(pwszID, in_szToken);
		CoTaskMemFree(pwszID);
		pEndPoint->Release();
	}
	
	return pFound != NULL ? id : AK_INVALID_DEVICE_ID;
}

namespace AK
{
	AkUInt32 GetDeviceID(IMMDevice* in_pDevice)
	{
		AkUInt32 id = AK_INVALID_DEVICE_ID;
		if (in_pDevice == NULL)
			return AK_INVALID_DEVICE_ID;

		LPWSTR pwszID = NULL;
		// Get the endpoint ID string.
		HRESULT hr = in_pDevice->GetId(&pwszID);
		if (hr != S_OK)
			return AK_INVALID_DEVICE_ID;

		id = AK::SoundEngine::GetIDFromString(pwszID);
		CoTaskMemFree(pwszID);
		return id;
	}

	AkUInt32 GetDeviceIDFromName(wchar_t* in_szToken)
	{
		AkUInt32 id = AK_INVALID_DEVICE_ID;	
		if (in_szToken == NULL)
			return AK_INVALID_DEVICE_ID;

		CAkCOMScope comScope;

		IMMDeviceCollection * pCol = GetDeviceList();
		if (pCol == NULL)
			return AK_INVALID_DEVICE_ID;

		UINT uNum = 0;
		pCol->GetCount(&uNum);

		for (UINT i = 0; i < uNum && id == AK_INVALID_DEVICE_ID; i++)
		{
			// Get pointer to endpoint number i.
			IMMDevice *pEndpoint = NULL;
			HRESULT hr = pCol->Item(i, &pEndpoint);
			if (pEndpoint == NULL)
				continue;

			id = CompareOneDevice(pEndpoint, in_szToken);
			pEndpoint->Release();
		}
		pCol->Release();
		return id;
	}
}
