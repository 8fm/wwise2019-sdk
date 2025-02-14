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

#include "AkSink.h"
#include "AkEffectsMgr.h"
#include "AkOutputMgr.h"
#include "AkRuntimeEnvironmentMgr.h"

#ifdef AK_USE_UWP_API
#include <combaseapi.h>
#include "AkGamepadRegistry.h"
#else
#include <AK/Tools/Win32/AkCOMScope.h>
#endif
#include <ks.h>
#include <ksmedia.h>

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
extern CAkLock			g_csMain;

#ifdef AK_USE_UWP_API
class CAkWindowsContext: public AK::IAkUWPContext
{
public:
	virtual Windows::Gaming::Input::Gamepad^ GetGamepadFromDeviceID(AkDeviceID deviceID)
	{
		return CAkGamepadRegistry::GetInstance().GetGamepad(deviceID);
	}
};
#else
class CAkWindowsContext: public AK::IAkWin32Context
{
};
#endif // AK_USE_UWP_API

#ifndef AK_USE_UWP_API

#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>

WNDPROC						CAkLEngine::m_WndProc = NULL;
HDEVNOTIFY					CAkLEngine::m_hDevNotify = NULL;
CAkMMNotificationClient *	CAkLEngine::m_pMMNotifClient = NULL;

class CAkMMNotificationClient
	: public IMMNotificationClient
{
public:
	CAkMMNotificationClient( IMMDeviceEnumerator * in_pEnumerator ) 
		: m_cRef( 1 )
		, m_pEnumerator( in_pEnumerator )
		, m_szDeviceID(NULL)
	{
		m_pEnumerator->AddRef();
		m_pEnumerator->RegisterEndpointNotificationCallback( this );

		IMMDevice *pDevice = NULL;
		HRESULT hr = m_pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
		if (pDevice)
		{
			pDevice->GetId(&m_szDeviceID);
			pDevice->Release();
		}
	}

	~CAkMMNotificationClient()
	{
		if (m_szDeviceID != NULL)
			CoTaskMemFree(m_szDeviceID);
		m_pEnumerator->UnregisterEndpointNotificationCallback( this );
		m_pEnumerator->Release();
	}

    virtual HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, void ** ppvObject )
	{
		const IID IID_IMMNotificationClient = __uuidof(IMMNotificationClient);

		if ( riid == IID_IUnknown )
		{
			*ppvObject = static_cast<IUnknown *>( this );
			m_cRef++;
			return S_OK;
		}
		else if ( riid == IID_IMMNotificationClient )
		{
			*ppvObject = static_cast<IMMNotificationClient *>( this );
			m_cRef++;
			return S_OK;
		}

		*ppvObject = NULL;
		return E_NOINTERFACE;
	}

    virtual ULONG STDMETHODCALLTYPE AddRef( void )
	{
		return ++m_cRef;
	}

    virtual ULONG STDMETHODCALLTYPE Release( void )
	{
		ULONG cRef = --m_cRef;
		AKASSERT( cRef >= 0 );
		if ( cRef == 0 )
		{
			AkDelete( AkMemID_Processing, this );
		}

		return cRef;
	}
	
	virtual HRESULT STDMETHODCALLTYPE OnDeviceStateChanged( LPCWSTR pwstrDeviceId, DWORD dwNewState ) 
	{ 			
		CAkOutputMgr::ResetMainDevice();
		return S_OK; 
	}

	virtual HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId)
	{
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnDeviceRemoved( LPCWSTR pwstrDeviceId ) 
	{ 
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged( EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId )
	{
		if ( flow == eRender && role == eMultimedia )
		{
			if (m_szDeviceID != NULL)
			{
				CoTaskMemFree(m_szDeviceID);
				m_szDeviceID = NULL;
			}

			if (pwstrDefaultDeviceId)
			{
				size_t len = wcslen(pwstrDefaultDeviceId);
				m_szDeviceID = (WCHAR*)CoTaskMemAlloc((len + 1) * sizeof(WCHAR));
				wcscpy(m_szDeviceID, pwstrDefaultDeviceId);
			}			

			//This notification always pertains to the main audio output.
			CAkOutputMgr::ResetMainDevice();			
		}
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnPropertyValueChanged( LPCWSTR pwstrDeviceId, const PROPERTYKEY key ) 
	{ 	
		//This notification always pertains to the main audio output.
		//Reset the device if the format changes.  This is particularly important with WASAPI when *other* software use ASIO.
		//What are those Prop Keys?  Funny you ask, I don't know either.  They are not documented.  Other apps use those.
		class __declspec(uuid("{3D6E1656-2E50-4C4C-8D85-D0ACAE3C6C68}")) A;
		class __declspec(uuid("{E4870E26-3CC5-4CD2-BA46-CA0A9A70ED04}")) B;
		class __declspec(uuid("{624F56DE-FD24-473E-814A-DE40AACAED16}")) C;

#define AK_DEFINE_PROPERTYKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) const PROPERTYKEY name = { { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }, pid }
		AK_DEFINE_PROPERTYKEY(AK_PKEY_AudioEngine_DeviceFormat, 0xf19f064d, 0x82c, 0x4e27, 0xbc, 0x73, 0x68, 0x82, 0xa1, 0xbb, 0x8e, 0x4c, 0);
		AK_DEFINE_PROPERTYKEY(AK_PKEY_AudioEngine_OEMFormat, 0xe4870e26, 0x3cc5, 0x4cd2, 0xba, 0x46, 0xca, 0xa, 0x9a, 0x70, 0xed, 0x4, 3);

		if(key == AK_PKEY_AudioEngine_DeviceFormat ||
			key == AK_PKEY_AudioEngine_OEMFormat ||
			key.fmtid == __uuidof(A) && (key.pid == 2 || key.pid == 3) ||
			key.fmtid == __uuidof(B) && key.pid == 0 ||
			key.fmtid == __uuidof(C) && key.pid == 3)			
		{
			CAkOutputMgr::ResetMainDevice();
		}	
				
		return S_OK; 
	}

private:		

	ULONG m_cRef;
	IMMDeviceEnumerator * m_pEnumerator;
	WCHAR *m_szDeviceID;
};

namespace
{
	IMMDeviceCollection* GetDeviceList(AkAudioDeviceState uDeviceStateMask)
	{
		const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
		const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

		IMMDeviceEnumerator* pEnumerator;

		HRESULT hr = CoCreateInstance(
			CLSID_MMDeviceEnumerator, NULL,
			CLSCTX_ALL, IID_IMMDeviceEnumerator,
			(void**)&pEnumerator);

		if (hr != S_OK)
			return NULL;

		IMMDeviceCollection* pReturn = NULL;
		pEnumerator->EnumAudioEndpoints(eRender, uDeviceStateMask, &pReturn);
		pEnumerator->Release();

		return pReturn;
	}

	AkUInt32 GetIDFromEndPoint(IMMDevice* in_pEndpoint)
	{
		LPWSTR pwszID = NULL;
		// Get the endpoint ID string.
		HRESULT hr = in_pEndpoint->GetId(&pwszID);
		if (hr != S_OK)
			return AK_INVALID_DEVICE_ID;

		AkUInt32 id = AK::SoundEngine::GetIDFromString(pwszID);
		CoTaskMemFree(pwszID);

		return id;
	}

	AkUInt32 CompareOneDevice(IMMDevice* in_pEndpoint, LPWSTR in_szToken)
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

		IPropertyStore* pProps = NULL;
		hr = in_pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
		if (hr != S_OK)
			return AK_INVALID_DEVICE_ID;

		// Initialize container for property value.
		PROPVARIANT varName;
		PropVariantInit(&varName);

		// Get the endpoint's friendly-name property.
		hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
		if (hr == S_OK)
		{
			pFound = wcsstr(varName.pwszVal, in_szToken);
			PropVariantClear(&varName);
		}

		pProps->Release();
		return pFound != NULL ? id : AK_INVALID_DEVICE_ID;
	}
}

#endif // AK_USE_UWP_API

bool CheckDeviceID(AkUInt32 in_idDevice)
{
	bool retVal = false;
	AkUInt32 id = AK_INVALID_DEVICE_ID;
	if (in_idDevice == AK_INVALID_DEVICE_ID)
		return true;
#ifndef AK_USE_UWP_API
	CAkCOMScope comScope;

	IMMDeviceCollection* pCol = ::GetDeviceList(AkDeviceState_All);
	if (pCol == NULL)
		return false;

	UINT uNum = 0;
	pCol->GetCount(&uNum);

	for (UINT i = 0; i < uNum && !retVal; i++)
	{
		// Get pointer to endpoint number i.
		IMMDevice* pEndpoint = NULL;
		HRESULT hr = pCol->Item(i, &pEndpoint);
		if (pEndpoint == NULL)
			continue;

		id = ::GetIDFromEndPoint(pEndpoint);

		DWORD state;
		pEndpoint->GetState(&state);
		if (state == DEVICE_STATE_ACTIVE || state == DEVICE_STATE_UNPLUGGED)
		{
			retVal = true;
		}

		pEndpoint->Release();
	}
	pCol->Release();
#endif
	return retVal;
}

namespace AK
{
	wchar_t * DeviceEnumeratorHelper(IMMDevice* in_pDevice, AkUInt32 &out_uDeviceID);
}

namespace AK
{	
	AkUInt32 GetDeviceID(IMMDevice* in_pDevice)
	{			
		AkUInt32 id = AK_INVALID_DEVICE_ID;
#ifndef AK_USE_UWP_API		
		if (in_pDevice == NULL)
			return AK_INVALID_DEVICE_ID;

		LPWSTR pwszID = NULL;
		// Get the endpoint ID string.
		HRESULT hr = in_pDevice->GetId(&pwszID);
		if(hr == S_OK)
		{
			id = AK::SoundEngine::GetIDFromString(pwszID);
			CoTaskMemFree(pwszID);
		}
#endif

		return id;
	}

	AkUInt32 GetDeviceIDFromName(wchar_t* in_szToken)
	{		
		AkUInt32 id = AK_INVALID_DEVICE_ID;	
		AkUInt32 idDisabled = AK_INVALID_DEVICE_ID;
#ifndef AK_USE_UWP_API
		if (in_szToken == NULL)
			return AK_INVALID_DEVICE_ID;

		CAkCOMScope comScope;

		IMMDeviceCollection * pCol = ::GetDeviceList(AkDeviceState_All);
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

			id = ::CompareOneDevice(pEndpoint, in_szToken);

			//Check if this device is enabled.  If not, we'll keep it, but only if we find none enabled.
			if(id != AK_INVALID_DEVICE_ID)
			{
				DWORD state;
				pEndpoint->GetState(&state);
				if(state != DEVICE_STATE_ACTIVE)
				{
					if (idDisabled == AK_INVALID_DEVICE_ID) //Keep the first!
						idDisabled = id;
					id = AK_INVALID_DEVICE_ID;
				}
			}
			pEndpoint->Release();
		}
		pCol->Release();
#endif

		return id != AK_INVALID_DEVICE_ID ? id : idDisabled;
	}

	// Include class declaration of ISpatialAudioClient because we are not yet able to target Windows 10 SDK
	struct __declspec(uuid("BBF8E066-AAAA-49BE-9A4D-FD2A858EA27F")) 
	__declspec(novtable) ISpatialAudioClient: public IUnknown
	{
	public:
		virtual HRESULT STDMETHODCALLTYPE GetStaticObjectPosition(
			int type, float *x, float *y, float *z) = 0;

		virtual HRESULT STDMETHODCALLTYPE GetNativeStaticObjectTypeMask(
			int *mask) = 0;

		virtual HRESULT STDMETHODCALLTYPE GetMaxDynamicObjectCount(
			UINT32 *value) = 0;

		virtual HRESULT STDMETHODCALLTYPE GetSupportedAudioObjectFormatEnumerator(
			void **enumerator) = 0;

		virtual HRESULT STDMETHODCALLTYPE GetMaxFrameCount(
			const void *objectFormat, UINT32 *frameCountPerBuffer) = 0;

		virtual HRESULT STDMETHODCALLTYPE IsAudioObjectFormatSupported(
			const void *objectFormat) = 0;

		virtual HRESULT STDMETHODCALLTYPE IsSpatialAudioStreamAvailable(
			REFIID streamUuid,
			const void *auxiliaryInfo) = 0;

		virtual HRESULT STDMETHODCALLTYPE ActivateSpatialAudioStream(
			const void *activationParams, REFIID riid, void **stream) = 0;
	};

	namespace SoundEngine
	{
		AKRESULT RegisterPluginDLL(const AkOSChar* in_DllName, const AkOSChar* in_DllPath /* = NULL */)
		{
			AkOSChar fullName[1024];	
			fullName[0] = 0;
			CAkLEngine::GetPluginDLLFullPath(fullName, 1024, in_DllName, in_DllPath);

#ifdef AK_WIN_UNIVERSAL_APP
			HMODULE hLib = ::LoadPackagedLibrary(fullName, 0);
#else
			HMODULE hLib = ::LoadLibrary(fullName);
#endif
			if (hLib == NULL)
				return AK_FileNotFound;

			AK::PluginRegistration** RegisterList = (AK::PluginRegistration**)::GetProcAddress(hLib, "g_pAKPluginList");
			if (RegisterList != NULL)
				return CAkEffectsMgr::RegisterPluginList(*RegisterList);

			return AK_InvalidFile;
		}

		AKRESULT GetDeviceSpatialAudioSupport(AkUInt32 in_idDevice)
		{
#ifndef AK_USE_UWP_API			
			IMMDeviceCollection* pCol = ::GetDeviceList(AkDeviceState_All);
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
				return AK_Fail;

			UINT32 maxObjects;
			hr = pSpatialAudioClient->GetMaxDynamicObjectCount(&maxObjects);
			pSpatialAudioClient->Release();
			if (hr != S_OK || maxObjects == 0)
				return AK_NotCompatible;

			return AK_Success;
#else
			return AK_NotCompatible;
#endif
		}
	}

#ifndef AK_USE_UWP_API
	wchar_t * DeviceEnumeratorHelper(IMMDevice* in_pDevice, AkUInt32 &out_uDeviceID)
	{
		static wchar_t s_DeviceName[AK_MAX_PATH];

		out_uDeviceID = AK::GetDeviceID(in_pDevice);

		IPropertyStore *pProps = NULL;
		HRESULT hr = in_pDevice->OpenPropertyStore(STGM_READ, &pProps);		
		if(hr != S_OK)
			return NULL;

		// Initialize container for property value.
		PROPVARIANT varName;
		PropVariantInit(&varName);

		// Get the endpoint's friendly-name property.
		hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
		if(hr == S_OK)
		{
			wcsncpy(s_DeviceName, varName.pwszVal, AK_MAX_PATH - 1);
			s_DeviceName[AK_MAX_PATH - 1] = 0;
			PropVariantClear(&varName);
			pProps->Release();
			return s_DeviceName;
		}
		return NULL;
	}
#endif

	const wchar_t * GetWindowsDeviceName(AkInt32 index, AkUInt32 &out_uDeviceID, AkAudioDeviceState uDeviceStateMask)
	{
		out_uDeviceID = NULL;
#ifndef AK_USE_UWP_API			
		const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
		const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);		
		
		HRESULT hr = S_OK;		

		CAkCOMScope comScope;

		IMMDeviceEnumerator * pEnumerator = NULL;
		hr = CoCreateInstance(
			CLSID_MMDeviceEnumerator, NULL,
			CLSCTX_ALL, IID_IMMDeviceEnumerator,
			(void**)&pEnumerator);

		if(hr != S_OK)
			return NULL;

		IMMDevice *pEndpoint = NULL;
		if(index == -1)
		{
			pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pEndpoint);
			pEnumerator->Release();		
		}
		else
		{
			IMMDeviceCollection *pCol = NULL;
			hr = pEnumerator->EnumAudioEndpoints(eRender, uDeviceStateMask, &pCol);
			pEnumerator->Release();

			if(pCol == NULL)
				return NULL;

			hr = pCol->Item(index, &pEndpoint);
			pCol->Release();
		}

		if(pEndpoint == NULL)
			return NULL;

		wchar_t* ret = DeviceEnumeratorHelper(pEndpoint, out_uDeviceID);
		pEndpoint->Release();
		return ret;
#else
		return NULL;
#endif
	}

	AkUInt32 GetWindowsDeviceCount(AkAudioDeviceState uDeviceStateMask)
	{
#ifndef AK_USE_UWP_API			
		IMMDeviceCollection *pCol = ::GetDeviceList(uDeviceStateMask);
		if (pCol == nullptr)
			return 0;

		HRESULT hr = S_OK;
		UINT deviceCount = 0;
		hr = pCol->GetCount(&deviceCount);
		pCol->Release();
		if (hr != S_OK)
			return 0;

		return (AkUInt32)deviceCount;
#else
		return 0;
#endif
	}

	bool GetWindowsDevice(AkInt32 index, AkUInt32 &out_uDeviceID, IMMDevice** out_ppDevice, AkAudioDeviceState uDeviceStateMask)
	{
#ifndef AK_USE_UWP_API			
		const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
		const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

		IMMDeviceEnumerator * pEnumerator = NULL;
		HRESULT hr = CoCreateInstance(
			CLSID_MMDeviceEnumerator, NULL,
			CLSCTX_ALL, IID_IMMDeviceEnumerator,
			(void**)&pEnumerator);

		if (hr != S_OK)
			return NULL;		

		IMMDeviceCollection * pCol = NULL;
		pEnumerator->EnumAudioEndpoints(eRender, uDeviceStateMask, &pCol);
		IMMDevice* pDevice;
		pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
		pEnumerator->Release();		

		if (pDevice)
		{
			out_uDeviceID = AK::GetDeviceID(pDevice);
			pDevice->Release();
			pDevice = NULL;
			if (index == -1)	//Return default device									
				return true;			
		}
		
		hr = pCol->Item(index, &pDevice);
		pCol->Release();

		if (hr != S_OK)
			return false;

		out_uDeviceID = AK::GetDeviceID(pDevice);
		if (out_ppDevice)
			(*out_ppDevice) = pDevice;

		return true;
#else
		return false;
#endif
	}

#ifdef AK_UWP_CPP_CX

	AkDeviceID GetDeviceIDFromGamepad(Windows::Gaming::Input::Gamepad^ rGamepad)
	{
		return CAkGamepadRegistry::GetInstance().GetDeviceID(rGamepad);
	}
	Windows::Gaming::Input::Gamepad^ GetGamepadFromDeviceID(AkDeviceID deviceId)
	{
		return CAkGamepadRegistry::GetInstance().GetGamepad(deviceId);
	}

#endif
};

void CAkLEngine::GetDefaultPlatformInitSettings( 
	AkPlatformInitSettings &      out_pPlatformSettings      // Platform specific settings. Can be changed depending on hardware.
	)
{
	memset( &out_pPlatformSettings, 0, sizeof( AkPlatformInitSettings ) );
#ifndef AK_USE_UWP_API
	out_pPlatformSettings.hWnd = ::GetForegroundWindow( );
#endif
	GetDefaultPlatformThreadInitSettings(out_pPlatformSettings);

	out_pPlatformSettings.threadBankManager.uStackSize = AK_DEFAULT_STACK_SIZE;
	out_pPlatformSettings.uNumRefillsInVoice = AK_DEFAULT_NUM_REFILLS_IN_VOICE_BUFFER;
#ifdef AK_WIN
	out_pPlatformSettings.bGlobalFocus = true;
#endif

	out_pPlatformSettings.uSampleRate = DEFAULT_NATIVE_FREQUENCY;

	out_pPlatformSettings.eAudioAPI = AkAPI_Default;
	out_pPlatformSettings.pXAudio2 = NULL;	

	out_pPlatformSettings.bEnableAvxSupport = true;
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
	CAkDefaultSink::s_CurrentAudioAPI = AkAPI_Default;	//Reset global variables.

	if (g_PDSettings.uSampleRate == 0)
	{
		// 0 is an invalid value for uSampleRate
		return AK_InvalidParameter;
	}
	
	if (g_settings.uNumSamplesPerFrame == 0)
	{
		// 0 is an invalid value for uNumSamplesPerFrame
		return AK_InvalidParameter;
	}
	
	// Re-set the runtimeEnvironmentManager in case we're re-initializing the sound engine
	AkRuntimeEnvironmentMgr::Instance()->Initialize();
	if (!g_PDSettings.bEnableAvxSupport)
	{
		AkRuntimeEnvironmentMgr::Instance()->DisableSIMDSupport(AK::AkSIMDProcessorSupport(AK_SIMD_AVX | AK_SIMD_AVX2));
	}

	AkAudioLibSettings::SetAudioBufferSettings(g_PDSettings.uSampleRate, g_settings.uNumSamplesPerFrame);

	AKRESULT eResult;

#ifdef AK_USE_UWP_API

	eResult = CAkGamepadRegistry::GetInstance().Init();
	if (eResult != AK_Success)
		return eResult;
#endif
	
	if ( AkCreateEvent( m_eventProcess ) != AK_Success )
		return AK_Fail;

	eResult = SoftwareInit();
	
	return eResult;
} // Init

AKRESULT CAkLEngine::InitPlatformContext()
{
	m_pPlatformContext = AkNew(AkMemID_SoundEngine, CAkWindowsContext());
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
	SoftwareTerm();

	AkDestroyEvent(m_eventProcess);
	m_eventProcess = NULL;

#ifdef AK_USE_UWP_API
	CAkGamepadRegistry::GetInstance().Term();
#endif
} // Term

void CAkLEngine::OnThreadStart()
{
	// Necessary for DirectSound / Vista audio
	// Here we try for the apartment-threaded model, but if it fails, we'll have to CoInitialize on the sound engine thread.
	if (!s_bCoInitializeSucceeded)
		s_bCoInitializeSucceeded = SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));

	RegisterDeviceChange();
}

void CAkLEngine::OnThreadEnd()
{
	UnregisterDeviceChange();
	CAkOutputMgr::Term();
	
	if (s_bCoInitializeSucceeded)
	{
		CoUninitialize(); // Necessary for DirectSound / Vista audio
		s_bCoInitializeSucceeded = false;
	}
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

	SoftwarePerform();

#if defined AK_CPU_X86 || defined AK_CPU_X86_64
	_MM_SET_FLUSH_ZERO_MODE(uFlushZeroMode);
#endif
} // Perform

#if !defined(AK_USE_UWP_API) 

LRESULT CALLBACK CAkLEngine::InternalWindowProc( HWND in_hWnd, UINT in_unMsg, WPARAM in_wParam, LPARAM in_lParam )
{
	//Handle the connection of headphones
	if(in_unMsg == WM_DEVICECHANGE && in_wParam == DBT_DEVICEARRIVAL && g_pAudioMgr /*Sound engine is not terminated*/)
	{
		DEV_BROADCAST_DEVICEINTERFACE *devHdr = (DEV_BROADCAST_DEVICEINTERFACE *)in_lParam;		
		if (devHdr->dbcc_classguid == KSCATEGORY_AUDIO || devHdr->dbcc_classguid == KSCATEGORY_RENDER)
			CAkOutputMgr::ResetMainDevice();
	}

	return CallWindowProc(m_WndProc, in_hWnd, in_unMsg, in_wParam, in_lParam);
}

#elif defined(AK_WINRT_DEVICENOTIFICATION)

Windows::Devices::Enumeration::DeviceWatcher^ CAkLEngine::m_rWatcher;
bool CAkLEngine::m_bWatcherEnumerationCompleted = false;

void CAkLEngine::OnDeviceAdded( Windows::Devices::Enumeration::DeviceWatcher ^, Windows::Devices::Enumeration::DeviceInformation ^ )
{
	if ( m_bWatcherEnumerationCompleted )
	{
		CAkOutputMgr::ResetMainDevice();			
	}
}

void CAkLEngine::OnDeviceUpdated( Windows::Devices::Enumeration::DeviceWatcher ^, Windows::Devices::Enumeration::DeviceInformationUpdate ^)
{
	if(m_bWatcherEnumerationCompleted)
	{
		CAkOutputMgr::ResetMainDevice();
	}
}

void CAkLEngine::OnDeviceRemoved( Windows::Devices::Enumeration::DeviceWatcher ^, Windows::Devices::Enumeration::DeviceInformationUpdate ^)
{
	if(m_bWatcherEnumerationCompleted)
	{
		CAkOutputMgr::ResetMainDevice();
	}
}

void CAkLEngine::OnDeviceEnumerationCompleted( Windows::Devices::Enumeration::DeviceWatcher ^, Platform::Object ^)
{
	m_bWatcherEnumerationCompleted = true;
}
#endif

void CAkLEngine::RegisterDeviceChange()
{
#if !defined(AK_USE_UWP_API) 
	IMMDeviceEnumerator * pEnumerator;

	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	HRESULT hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);

	// Windows Vista or higher: preferred notification system available
	if ( hr == S_OK && pEnumerator )
	{
		m_pMMNotifClient = AkNew( AkMemID_Processing, CAkMMNotificationClient( pEnumerator ) );
		pEnumerator->Release();
	}

	// Fallback for Windows XP: intermittently working device notification
	else
	{
		DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;

		ZeroMemory( &NotificationFilter, sizeof(NotificationFilter) );
		NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
		NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
		NotificationFilter.dbcc_classguid = GUID_NULL;

		m_hDevNotify = ::RegisterDeviceNotification(g_PDSettings.hWnd, &NotificationFilter, DEVICE_NOTIFY_ALL_INTERFACE_CLASSES | DEVICE_NOTIFY_WINDOW_HANDLE);
		m_WndProc = (WNDPROC)::SetWindowLongPtr(g_PDSettings.hWnd, GWLP_WNDPROC, (AkUIntPtr)InternalWindowProc);
	}
#elif defined(AK_WINRT_DEVICENOTIFICATION)
	m_rWatcher = Windows::Devices::Enumeration::DeviceInformation::CreateWatcher(Windows::Devices::Enumeration::DeviceClass::AudioRender);
	if ( m_rWatcher )
	{
		m_bWatcherEnumerationCompleted = false;
		m_rWatcher->Added += ref new Windows::Foundation::TypedEventHandler<Windows::Devices::Enumeration::DeviceWatcher ^, Windows::Devices::Enumeration::DeviceInformation ^>( &CAkLEngine::OnDeviceAdded);
		m_rWatcher->Updated += ref new Windows::Foundation::TypedEventHandler<Windows::Devices::Enumeration::DeviceWatcher ^, Windows::Devices::Enumeration::DeviceInformationUpdate ^>( &CAkLEngine::OnDeviceUpdated);
		m_rWatcher->Removed += ref new Windows::Foundation::TypedEventHandler<Windows::Devices::Enumeration::DeviceWatcher ^, Windows::Devices::Enumeration::DeviceInformationUpdate ^>( &CAkLEngine::OnDeviceRemoved);
		m_rWatcher->EnumerationCompleted += ref new Windows::Foundation::TypedEventHandler<Windows::Devices::Enumeration::DeviceWatcher ^, Platform::Object ^>( &CAkLEngine::OnDeviceEnumerationCompleted);
		m_rWatcher->Start();
	}

#endif
}

void CAkLEngine::UnregisterDeviceChange()
{
#if !defined(AK_USE_UWP_API) 
	if ( m_pMMNotifClient )
	{
		m_pMMNotifClient->Release();
		m_pMMNotifClient = NULL;
	}
	else
	{
		if ( m_hDevNotify != NULL )
			::UnregisterDeviceNotification( m_hDevNotify );

		//Reset the original WndProc
		if (::GetWindowLongPtr( g_PDSettings.hWnd, GWLP_WNDPROC) == (AkUIntPtr)InternalWindowProc )
			::SetWindowLongPtr( g_PDSettings.hWnd, GWLP_WNDPROC, (AkUIntPtr)m_WndProc );
	}
#elif defined(AK_WINRT_DEVICENOTIFICATION)
	if ( m_rWatcher )
	{
		m_rWatcher->Stop();
		m_rWatcher = nullptr;
	}
#endif
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

#ifndef AK_USE_UWP_API
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

		AkUInt32 deviceID = 0;
		const wchar_t* name = DeviceEnumeratorHelper(pDefaultDevice, deviceID);

		pDefaultDevice->Release();
	}

	pEnumerator->Release();
}
#endif


AKRESULT CAkLEngine::GetPlatformDeviceList(
	AkPluginID in_pluginID,
	AkUInt32& io_maxNumDevices,
	AkDeviceDescription* out_deviceDescriptions
)
{
#ifndef AK_USE_UWP_API
	if (in_pluginID != AKPLUGINID_DEFAULT_SINK && in_pluginID != AKPLUGINID_COMMUNICATION_SINK)
		return AK_NotCompatible;

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	if (SUCCEEDED(hr))
	{
		IMMDeviceCollection * pCol = ::GetDeviceList(AkDeviceState_All);
		if (pCol == NULL)
		{
			io_maxNumDevices = 0;
			CoUninitialize();
			return AK_Fail;
		}

		UINT uNum = 0;
		pCol->GetCount(&uNum);

		// Fetch the default device
		ERole eRole = in_pluginID == AKPLUGINID_COMMUNICATION_SINK ? eCommunications : eConsole;
		AkUInt32 defaultDeviceID = 0;
		GetDefaultDeviceID(eRole, defaultDeviceID);

		// Else fill as many devices as we can
		AkUInt32 deviceIndex = 0;
		for (UINT i = 0; i < uNum && (deviceIndex < io_maxNumDevices || out_deviceDescriptions == NULL); i++)
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
			wchar_t* name = DeviceEnumeratorHelper(pEndpoint, deviceID);
			if (name == NULL)
				continue; // skip devices with no name

			// write to output if device is valid
			if (out_deviceDescriptions != NULL)
			{
				out_deviceDescriptions[deviceIndex].idDevice = deviceID;
				SafeStrCpy(out_deviceDescriptions[deviceIndex].deviceName, name, AK_MAX_PATH);
				out_deviceDescriptions[deviceIndex].deviceStateMask = (AkAudioDeviceState)state;
				out_deviceDescriptions[deviceIndex].isDefaultDevice = (deviceID == defaultDeviceID);
			}
			++deviceIndex;
			pEndpoint->Release();
		}
		pCol->Release();
		CoUninitialize();
		io_maxNumDevices = deviceIndex;
	}

	return AK_Success;
#else
	return AK_NotImplemented;
#endif
}
