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

#include "stdafx.h"
#include "AkSinkXAudio2.h"

#ifdef AK_XAUDIO2
#include "AkAudioMgr.h"
#include "AkOutputMgr.h"

#include "xaudio2.h"

#if !defined(AK_USE_UWP_API)
#if defined AK_WIN 
#include <setupapi.h>
#pragma comment(lib, "setupapi.lib")

#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <initguid.h>
#include <devpkey.h>
#else
#include <mmdeviceapi.h>
#endif //AK_WIN
#endif //!AK_USE_UWP_API

extern AkPlatformInitSettings g_PDSettings;
#define AK_NUM_VOICE_BUFFERS (g_PDSettings.uNumRefillsInVoice)

#define	AK_PRIMARY_BUFFER_SAMPLE_TYPE			AkReal32

XAudio2Sinks CAkSinkXAudio2::s_XAudio2List;

static bool s_bXAudio27Pinned = false;
CXAudio2Base::XAudio2Version CXAudio2Base::m_XAudioVersion = XAudio2NotFound;
HMODULE CXAudio2Base::m_hlibXAudio2 = NULL;

typedef HRESULT(WINAPI *XAudio2CreateFn)(IXAudio2** ppXAudio2, UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor);

//IXAudio2 interface from Xaudio2.7.  The interface is not compatible with Xaudio2.8 or 2.9.  The includes above target the 2.8/2.9 version (named IXAudio2).
class IXAudio27
{
public:
	STDMETHOD(QueryInterface) (THIS_ REFIID riid, void** ppvInterface) = NULL;
	STDMETHOD_(ULONG, AddRef) (THIS)= NULL;	
	STDMETHOD_(ULONG, Release) (THIS)= NULL;
	STDMETHOD(GetDeviceCount) (THIS_  UINT32* pCount) = NULL;
	STDMETHOD(GetDeviceDetails) (THIS_ UINT32 Index,  XAUDIO2_DEVICE_DETAILS* pDeviceDetails) = NULL;
	STDMETHOD(Initialize) (THIS_ UINT32 Flags X2DEFAULT(0), XAUDIO2_PROCESSOR XAudio2Processor X2DEFAULT(XAUDIO2_DEFAULT_PROCESSOR)) = NULL;
	STDMETHOD(RegisterForCallbacks) (__in IXAudio2EngineCallback* pCallback) = NULL;
	STDMETHOD_(void, UnregisterForCallbacks) (__in IXAudio2EngineCallback* pCallback) = NULL;
	STDMETHOD(CreateSourceVoice) (THIS_ __deref_out IXAudio2SourceVoice** ppSourceVoice,
		__in const WAVEFORMATEX* pSourceFormat,
		UINT32 Flags X2DEFAULT(0),
		float MaxFrequencyRatio X2DEFAULT(XAUDIO2_DEFAULT_FREQ_RATIO),
		 IXAudio2VoiceCallback* pCallback X2DEFAULT(NULL),
		 const XAUDIO2_VOICE_SENDS* pSendList X2DEFAULT(NULL),
		 const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) = NULL;

	STDMETHOD(CreateSubmixVoice) (THIS_ __deref_out IXAudio2SubmixVoice** ppSubmixVoice,
		UINT32 InputChannels, UINT32 InputSampleRate,
		UINT32 Flags X2DEFAULT(0), UINT32 ProcessingStage X2DEFAULT(0),
		 const XAUDIO2_VOICE_SENDS* pSendList X2DEFAULT(NULL),
		 const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) = NULL;

	STDMETHOD(CreateMasteringVoice) (THIS_ __deref_out IXAudio2MasteringVoice** ppMasteringVoice,
		UINT32 InputChannels X2DEFAULT(XAUDIO2_DEFAULT_CHANNELS),
		UINT32 InputSampleRate X2DEFAULT(XAUDIO2_DEFAULT_SAMPLERATE),
		UINT32 Flags X2DEFAULT(0), UINT32 DeviceIndex X2DEFAULT(0),
		 const XAUDIO2_EFFECT_CHAIN* pEffectChain X2DEFAULT(NULL)) = NULL;

	STDMETHOD(StartEngine) (THIS)= NULL;
	STDMETHOD_(void, StopEngine) (THIS)= NULL;
	STDMETHOD(CommitChanges) (THIS_ UINT32 OperationSet) = NULL;
	STDMETHOD_(void, GetPerformanceData) (THIS_  XAUDIO2_PERFORMANCE_DATA* pPerfData) = NULL;
	STDMETHOD_(void, SetDebugConfiguration) (THIS_  const XAUDIO2_DEBUG_CONFIGURATION* pDebugConfiguration,
		 __reserved void* pReserved X2DEFAULT(NULL)) = NULL;
};

class __declspec(uuid("{8bcf1f58-9fe7-4583-8ac6-e2adc465c8bb}")) IID_IXAudio27;
class __declspec(uuid("{5a508685-a254-4fba-9b82-9a24b00306af}")) CLSID_XAudio27;

// Copy of Windows Version check function in order to be independent of Windows SDK version.
#define AK_WIN7 0x0601	//Windows 7.  Defined here because it is not available in previous Win SDK and we still want to build with it.
#define AK_WIN8 0x0602 // Windows 8.  Defined here because it is not available in previous Win SDK and we still want to build with it.
#define AK_WIN10 0x0A00 // Windows 10.  Defined here because it is not available in previous Win SDK and we still want to build with it.

#if !defined(AK_USE_UWP_API)
static inline bool IsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
	OSVERSIONINFOEXW osvi = { sizeof(osvi), 0, 0, 0, 0, { 0 }, 0, 0 };
	DWORDLONG        const dwlConditionMask = VerSetConditionMask(
		VerSetConditionMask(
		VerSetConditionMask(
		0, VER_MAJORVERSION, VER_GREATER_EQUAL),
		VER_MINORVERSION, VER_GREATER_EQUAL),
		VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

	osvi.dwMajorVersion = wMajorVersion;
	osvi.dwMinorVersion = wMinorVersion;
	osvi.wServicePackMajor = wServicePackMajor;

	return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}
static inline bool IsWindows10OrLater()
{
	return IsWindowsVersionOrGreater(HIBYTE(AK_WIN10), LOBYTE(AK_WIN10), 0);
}

static inline bool IsWindows8OrLater()
{
	return IsWindowsVersionOrGreater(HIBYTE(AK_WIN8), LOBYTE(AK_WIN8), 0);
}

static inline bool IsWindows7OrLater()
{
	return IsWindowsVersionOrGreater(HIBYTE(AK_WIN7), LOBYTE(AK_WIN7), 0);
}

bool CXAudio2Base::FindDevicePathForIMMDevice(IMMDevice* in_pDevice, LPWSTR out_szID, AkUInt32 in_uReserved)
{
	LPWSTR szID = NULL;
	in_pDevice->GetId(&szID);

	_wcsupr(szID);

	::HDEVINFO devInfoSet;
	::DWORD devCount = 0;
	::SP_DEVINFO_DATA devInfo;
	::SP_DEVICE_INTERFACE_DATA devInterface;
#define AK_DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
	AK_DEFINE_GUID(AK_DEVINTERFACE_AUDIO_RENDER, 0xe6327cad, 0xdcec, 0x4949, 0xae, 0x8a, 0x99, 0x1e, 0x97, 0x6a, 0x79, 0xd2);

	DWORD size = 0;
	devInfoSet = SetupDiGetClassDevs(&AK_DEVINTERFACE_AUDIO_RENDER, 0, 0, DIGCF_INTERFACEDEVICE | DIGCF_PRESENT); // DIGCF_PRESENT 
	if(devInfoSet == INVALID_HANDLE_VALUE)
		return false;

	devInfo.cbSize = sizeof(SP_DEVINFO_DATA);
	devInterface.cbSize = sizeof(::SP_DEVICE_INTERFACE_DATA);
	DWORD deviceIndex = 0;

	while(true) {
		if(!SetupDiEnumDeviceInterfaces(devInfoSet, 0, &AK_DEVINTERFACE_AUDIO_RENDER, deviceIndex, &devInterface)) {
			DWORD error = GetLastError();
			if(error == ERROR_NO_MORE_ITEMS)
				break;
		}
		deviceIndex++;

		// See how large a buffer we require for the device interface details (ignore error, it should be returning ERROR_INSUFFICIENT_BUFFER
		SetupDiGetDeviceInterfaceDetail(devInfoSet, &devInterface, 0, 0, &size, 0);

		SP_DEVICE_INTERFACE_DETAIL_DATA* interfaceDetail = (SP_DEVICE_INTERFACE_DETAIL_DATA*)alloca(size);
		interfaceDetail->cbSize = sizeof(::SP_DEVICE_INTERFACE_DETAIL_DATA);
		devInfo.cbSize = sizeof(::SP_DEVINFO_DATA);
		if(!::SetupDiGetDeviceInterfaceDetail(devInfoSet, &devInterface, interfaceDetail, size, 0, &devInfo))
			continue;

		wchar_t deviceId[2048];
		size = sizeof(deviceId);
		deviceId[0] = 0;
		DWORD requiredSize;
		if(!SetupDiGetDeviceInstanceId(devInfoSet, &devInfo, deviceId, size, &requiredSize))
			continue;

		if(wcsstr(deviceId, szID) != NULL)
		{
			//FOUND!
			wcsncpy(out_szID, interfaceDetail->DevicePath, in_uReserved - 1);
			out_szID[in_uReserved - 1] = 0;
			break;
		}
	}

	if(devInfoSet) {
		SetupDiDestroyDeviceInfoList(devInfoSet);
	}

	CoTaskMemFree(szID);

	return out_szID[0] != 0;
}

HMODULE  CXAudio2Base::FindLoadedXAudioDLL()
{		
	m_XAudioVersion = XAudio2NotFound;
	HMODULE hmodXAudioDLL = NULL;	
	if(IsWindows10OrLater() && GetModuleHandleExA(0, "XAudio2_9.dll", &hmodXAudioDLL))
	{
		m_XAudioVersion = XAudio29;
		return hmodXAudioDLL;
	}

	if(IsWindows8OrLater() && GetModuleHandleExA(0, "XAudio2_8.dll", &hmodXAudioDLL))
	{
		m_XAudioVersion = XAudio28;
		return hmodXAudioDLL;
	}

	// Work around XAudio2 DLL unload issue on Windows.
	DWORD flags = 0;
	if(!s_bXAudio27Pinned)
		flags = GET_MODULE_HANDLE_EX_FLAG_PIN;

	if(IsWindows7OrLater() && GetModuleHandleExA(flags, "XAudio2_7.dll", &hmodXAudioDLL))
	{
		m_XAudioVersion = XAudio27;
		s_bXAudio27Pinned = true;
		return hmodXAudioDLL;
	}

	return NULL;
}

HMODULE  CXAudio2Base::LoadXAudioDLL()
{
	HMODULE hmodXAudioDLL = NULL;
	m_XAudioVersion = XAudio2NotFound;
	if(IsWindows10OrLater())
	{
		hmodXAudioDLL = LoadLibraryA("XAudio2_9.dll");
		m_XAudioVersion = XAudio29;
	}

	if(hmodXAudioDLL == NULL && IsWindows8OrLater())
	{
		hmodXAudioDLL = LoadLibraryA("XAudio2_8.dll");
		m_XAudioVersion = XAudio28;
	}

	if(hmodXAudioDLL == NULL && IsWindows7OrLater())
	{
		hmodXAudioDLL = LoadLibraryA("XAudio2_7.dll");
		m_XAudioVersion = XAudio27;

		// Work around XAudio2 DLL unload issue on Windows.
		DWORD flags = 0;
		if(!s_bXAudio27Pinned)
		{
			s_bXAudio27Pinned = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN, "XAudio2_7.dll", &hmodXAudioDLL) == TRUE;			
		}
	}

	return hmodXAudioDLL;
}

HRESULT CXAudio2Base::Init(IXAudio2* in_pXAudio2)
{
	HRESULT hr = 0;	
	if(in_pXAudio2)
	{
		m_hlibXAudio2 = FindLoadedXAudioDLL();
		m_pXAudio2 = in_pXAudio2;
		m_pXAudio2->AddRef();
		return S_OK;
	}

	if(m_hlibXAudio2 == NULL)
		m_hlibXAudio2 = LoadXAudioDLL();

	if(m_hlibXAudio2 == NULL)
		return E_NOINTERFACE;

	if(m_XAudioVersion == XAudio27)
	{
		IXAudio27* pXAudio27 = NULL;
		hr = CoCreateInstance(__uuidof(CLSID_XAudio27), NULL, CLSCTX_INPROC_SERVER, __uuidof(IID_IXAudio27), (void**)&m_pXAudio27);
		if(SUCCEEDED(hr))
		{
			hr = m_pXAudio27->Initialize(AK_XAUDIO2_FLAGS, XAUDIO2_DEFAULT_PROCESSOR);
			if(FAILED(hr))
			{
				m_pXAudio27->Release();
				m_pXAudio27 = NULL;
			}
		}
	}
	else
	{
		XAudio2CreateFn CreateProc = (XAudio2CreateFn)GetProcAddress(m_hlibXAudio2, "XAudio2Create");
		if (CreateProc != NULL)
		{
			hr = CreateProc((IXAudio2**)&m_pXAudio2, AK_XAUDIO2_FLAGS, XAUDIO2_DEFAULT_PROCESSOR);
		}
		else
		{
			hr = E_FAIL;// must return en error.
		}
	}
	return hr;
}
#else	
HRESULT CXAudio2Base::Init(IXAudio2* in_pXAudio2)
{
	return XAudio2Create(&m_pXAudio2, AK_XAUDIO2_FLAGS, XAUDIO2_DEFAULT_PROCESSOR);	
}
#endif

void CXAudio2Base::Term()
{
	if(m_pXAudio2)
	{		
		m_pXAudio2->Release();
		m_pXAudio2 = NULL;
	}
	if(m_pXAudio27)
	{
		m_pXAudio27->Release();
		m_pXAudio27 = NULL;
	}
}

AKRESULT CXAudio2Base::CreateMasteringVoice(IXAudio2MasteringVoice** ppMasteringVoice,
	UINT32 InputChannels,
	UINT32 InputSampleRate,
	UINT32 Flags, IMMDevice * Device)
{
	if (m_pXAudio2)
	{
		LPWSTR pSelectedDevice = NULL;
#ifndef AK_USE_UWP_API
		WCHAR szID[1024] = { 0 };
		if (Device && FindDevicePathForIMMDevice(Device, szID, 1024))
			pSelectedDevice = szID;
#endif

		HRESULT hr = m_pXAudio2->CreateMasteringVoice(ppMasteringVoice, InputChannels, InputSampleRate, Flags, pSelectedDevice);
		return FAILED(hr) ? AK_Fail : AK_Success;
	}

	UINT32 deviceIndex;
	AKRESULT res = FindXAudio27DeviceIndexForIMMDevice(Device, deviceIndex);
	if (res != AK_Success)
		return res;
	
	HRESULT hr = m_pXAudio27->CreateMasteringVoice(ppMasteringVoice, InputChannels, InputSampleRate, Flags, deviceIndex);
	return FAILED(hr) ? AK_Fail : AK_Success;
}

AKRESULT CXAudio2Base::FindXAudio27DeviceIndexForIMMDevice(IMMDevice * in_pDevice, UINT32 &out_deviceIndex)
{
#ifndef AK_USE_UWP_API
	if (in_pDevice != NULL)
	{
		UINT32 deviceCount;
		HRESULT hr = m_pXAudio27->GetDeviceCount(&deviceCount);
		if (FAILED(hr))
			return AK_Fail;
		LPTSTR deviceId;
		in_pDevice->GetId(&deviceId);
		UINT32 deviceIndex;
		for (deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++)
		{
			XAUDIO2_DEVICE_DETAILS details;
			hr = m_pXAudio27->GetDeviceDetails(deviceIndex, &details);
			if (FAILED(hr))
				continue;
			if (wcsstr(deviceId, details.DeviceID) != NULL)
			{
				//FOUND!
				out_deviceIndex = deviceIndex;
				break;
			}
		}
		CoTaskMemFree(deviceId);
		if (deviceIndex >= deviceCount)
			out_deviceIndex = 0;
	}
	else
	{
		out_deviceIndex = 0; // Default device
	}
#else
	out_deviceIndex = 0;
#endif
	return AK_Success;
}

HRESULT CXAudio2Base::RegisterForCallbacks(IXAudio2EngineCallback* pCallback)
{
	if(m_pXAudio2)
		return m_pXAudio2->RegisterForCallbacks(pCallback);

	return m_pXAudio27->RegisterForCallbacks(pCallback);
}

void CXAudio2Base::UnregisterForCallbacks(IXAudio2EngineCallback* pCallback)
{
	if(m_pXAudio2)
		m_pXAudio2->UnregisterForCallbacks(pCallback);
	if(m_pXAudio27)
		m_pXAudio27->UnregisterForCallbacks(pCallback);
}

HRESULT CXAudio2Base::CreateSourceVoice(IXAudio2SourceVoice** ppSourceVoice,
	const WAVEFORMATEX* pSourceFormat,
	UINT32 Flags,
	float MaxFrequencyRatio,
	IXAudio2VoiceCallback* pCallback)
{
	if(m_pXAudio2)
		return m_pXAudio2->CreateSourceVoice(ppSourceVoice, pSourceFormat, Flags, MaxFrequencyRatio, pCallback, NULL, NULL);

	return m_pXAudio27->CreateSourceVoice(ppSourceVoice, pSourceFormat, Flags, MaxFrequencyRatio, pCallback, NULL, NULL);
}

HRESULT CXAudio2Base::GetDeviceDetails(UINT32 Index, XAUDIO2_DEVICE_DETAILS* pDeviceDetails)
{
	if(m_pXAudio2)
		return E_FAIL;

	return m_pXAudio27->GetDeviceDetails(Index, pDeviceDetails);
}

IUnknown* CXAudio2Base::GetIUnknown()
{
	if(m_pXAudio2)
		return (IUnknown*)m_pXAudio2;
	return (IUnknown*)m_pXAudio27;
}

CAkSinkXAudio2::CAkSinkXAudio2()
	: CAkSink()
    , m_ulNumChannels( 0 )	
	, m_pMasteringVoice( NULL )
	, m_pSourceVoice( NULL )
	, m_pvAudioBuffer( NULL )
	, m_uWriteBufferIndex( 0 )
	, m_uReadBufferIndex( 0 )
	, m_nbBuffersRB( 0 )
	, m_usBlockAlign( 0 )
	, m_bStarved( false )
	, m_bCriticalError( false )
	, pNextLightItem( NULL )
{
	for ( AkUInt32 i = 0; i < AK_MAX_NUM_VOICE_BUFFERS; ++i )
		m_ppvRingBuffer[i] = NULL;
}

CAkSinkXAudio2::~CAkSinkXAudio2()
{
}

AKRESULT CAkSinkXAudio2::Init(IMMDevice* in_pDevice, AkPluginID in_uPlugin, AkPlatformInitSettings & in_settings, const AkOutputSettings &in_OutputSettings, bool in_bRecordable)
{	
	// Create XAudio2 engine with default options
	UINT32 Flags = AK_XAUDIO2_FLAGS;	
	HRESULT hr = m_XAudio2.Init(in_settings.pXAudio2);	

	if(FAILED(hr))
		return AK_DLLCannotLoad;
	
	UINT flags = 0;
#ifdef AK_XBOXONE
	// Create a 6-channel 48 kHz mastering voice feeding the default device
	AUDIO_STREAM_CATEGORY eCategory = AudioCategory_GameEffects;
	switch( in_uPlugin )
	{
		case AKPLUGINID_COMMUNICATION_SINK: 
		case AKPLUGINID_PERSONAL_SINK:
			eCategory = AudioCategory_Communications;
			flags = XAUDIO2_EXCLUDE_FROM_GAME_DVR_CAPTURE ;
			break;
		case AKPLUGINID_DVR_SINK: 
			eCategory = AudioCategory_GameMedia; 
			flags = in_bRecordable ? 0 : XAUDIO2_EXCLUDE_FROM_GAME_DVR_CAPTURE;
			break;
	}
	
	hr = m_pXAudio2->CreateMasteringVoice( &m_pMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, AK_CORE_SAMPLERATE, flags, NULL, NULL, eCategory) ;
	if (!SUCCEEDED(hr))
		return AK_Fail;
#else
	AKRESULT res = m_XAudio2.CreateMasteringVoice(&m_pMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, AK_CORE_SAMPLERATE, flags, in_pDevice);
	if (res != AK_Success)
		return res;
#endif

	// Get current channel mask from xaudio2 device
	if (in_OutputSettings.channelConfig.uChannelMask == 0)
	{
		AkUInt32 uChannelCount = GetChannelCount();		
#ifdef AK_71AUDIO
		if ( uChannelCount >= 7 )
			m_SpeakersConfig.SetStandard( AK_SPEAKER_SETUP_7POINT1 );
		else
#endif
		if ( uChannelCount == 4 )
			m_SpeakersConfig.SetStandard( AK_SPEAKER_SETUP_4_0 );
		else if (uChannelCount > 4)
			m_SpeakersConfig.SetStandard( AK_SPEAKER_SETUP_5POINT1 );
		else
			m_SpeakersConfig.SetStandard( AK_SPEAKER_SETUP_STEREO );
	}
	else
	{
		m_SpeakersConfig.SetStandard(in_OutputSettings.channelConfig.uChannelMask);
	}
	
	m_ulNumChannels = m_SpeakersConfig.uNumChannels;

	if (AllocBuffer(m_SpeakersConfig) != AK_Success)
		return AK_Fail;

	m_usBlockAlign = (AkUInt16) ( m_ulNumChannels * sizeof(AkReal32) );
	AkUInt32 uBufferBytes = AK_NUM_VOICE_REFILL_FRAMES * m_usBlockAlign;

	// Allocate ring buffer 
	m_pvAudioBuffer = AkAlloc( AkMemID_Processing, AK_MAX_NUM_VOICE_BUFFERS*uBufferBytes );
	if ( m_pvAudioBuffer == NULL )
	{
		return AK_Fail;
	}

	::ZeroMemory( m_pvAudioBuffer, AK_MAX_NUM_VOICE_BUFFERS*uBufferBytes );

	// Initialize ring buffer ptrs
	for ( AkUInt32 i = 0; i < AK_MAX_NUM_VOICE_BUFFERS; ++i )
		m_ppvRingBuffer[i] = (AkUInt8*)m_pvAudioBuffer + i*uBufferBytes;

	// try to create the source voice
	WAVEFORMATEXTENSIBLE formatX;
	formatX.dwChannelMask = m_SpeakersConfig.uChannelMask; 
	formatX.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
	formatX.Samples.wValidBitsPerSample = 32;
	formatX.Format.nChannels = (WORD)m_ulNumChannels;
	formatX.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	formatX.Format.nSamplesPerSec = AK_CORE_SAMPLERATE;
	formatX.Format.nBlockAlign = formatX.Format.nChannels * sizeof( AkReal32 );
	formatX.Format.nAvgBytesPerSec = formatX.Format.nSamplesPerSec * formatX.Format.nBlockAlign;
	formatX.Format.wBitsPerSample = sizeof( AkReal32 ) * 8;
	formatX.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

	if( FAILED( m_XAudio2.CreateSourceVoice( &m_pSourceVoice, (WAVEFORMATEX*)&(formatX), 0, 1.0, this ) ) )
		return AK_Fail;

	m_pSourceVoice->SetVolume(1.0f);

	//Synchronize this sink with the main sink too.  This is necessary to avoid starvation of the slave sink in a multi-sink environment
	//So start the ring buffer *exactly* at the same place as the main sink is.	
	if (s_XAudio2List.First() != NULL)
	{		
		CAkSinkXAudio2* pMainSink = s_XAudio2List.First();
		m_uWriteBufferIndex = (pMainSink->m_nbBuffersRB + 1) % AK_MAX_NUM_VOICE_BUFFERS;
		m_nbBuffersRB = pMainSink->m_nbBuffersRB;
		m_uReadBufferIndex = 0;
		
		for(AkInt32 i = 0; i < m_nbBuffersRB; i++)
			SubmitPacketRB();
	}

	AK_PC_WAIT_TIME = 1000 * AK_NUM_VOICE_REFILL_FRAMES / AK_CORE_SAMPLERATE * 2;	//Normally, we should be awoken by the hardware.  So if it takes twice the time, lets do something.
	s_XAudio2List.AddFirst(this);
	m_XAudio2.RegisterForCallbacks(this);
	
	return AK_Success;
}

AKRESULT CAkSinkXAudio2::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{	
	if(m_pSourceVoice)//must check since Stop may be called on Term after the Init Failed
	{
		DWORD buffers = m_nbBuffersRB;
		HRESULT hr = m_pSourceVoice->Stop(0, XAUDIO2_COMMIT_NOW);
		if(hr == S_OK)
		{
			//Wait for Stop confirmation
			hr = m_pSourceVoice->FlushSourceBuffers();
			if(hr == S_OK)
			{
				DWORD waitReturn = 0;
				while(m_nbBuffersRB != 0 && m_nbBuffersRB == buffers && waitReturn != WAIT_TIMEOUT)
				{
					waitReturn = ::WaitForSingleObjectEx(g_pAudioMgr->GetAudioThread().GetEvent(), AkAudioLibSettings::g_msPerBufferTick + 1, FALSE);
				}
			}
		}
		// Unregister callbacks here (cannot be done in a callback according to the doc)
		m_XAudio2.UnregisterForCallbacks(this);
		
		// Call this before clearing buffer to avoid race with voice callback.
		// This call will wait until all callbacks are issued for this voice
		// so we need to handle possible errors in these callbacks.
		m_pSourceVoice->DestroyVoice();
		m_pSourceVoice = NULL;		
	}

	if( m_MasterOut.HasData() )
	{
		AkFalign( AkMemID_Processing, m_MasterOut.GetContiguousDeinterleavedData() );
		m_MasterOut.ClearData();
	}

	if( m_pMasteringVoice )
	{		
		m_pMasteringVoice->DestroyVoice();
		m_pMasteringVoice = NULL;
	}
		
	m_XAudio2.Term();	


	// Clear our ring buffer
	m_uWriteBufferIndex = 0;
	m_uReadBufferIndex = 0;
	m_nbBuffersRB = 0;
	for ( AkUInt32 i = 0; i < AK_MAX_NUM_VOICE_BUFFERS; ++i )
		m_ppvRingBuffer[i] = NULL;

	if ( m_pvAudioBuffer )
	{
		AkFree( AkMemID_Processing, m_pvAudioBuffer );
		m_pvAudioBuffer = NULL;
	}

	s_XAudio2List.Remove(this);	

	return CAkSink::Term( in_pAllocator );
}

AKRESULT CAkSinkXAudio2::Reset()
{
	HRESULT hr = m_pSourceVoice->Start(0);
	return SUCCEEDED( hr ) ? AK_Success : AK_Fail;
}

bool CAkSinkXAudio2::IsStarved()
{
	return m_bStarved;
}

void CAkSinkXAudio2::ResetStarved()
{
	m_bStarved = false;
}

AkUInt16 CAkSinkXAudio2::IsDataNeededRB( )
{
	if (m_nbBuffersRB < AK_NUM_VOICE_BUFFERS)
		return (AkUInt16)(AK_NUM_VOICE_BUFFERS - m_nbBuffersRB);
	else
		return 0;
}

bool CAkSinkXAudio2::SubmitPacketRB( )
{
	// Build and submit the packet
	XAUDIO2_BUFFER XAudio2Buffer = { 0 };
	XAudio2Buffer.AudioBytes = AK_NUM_VOICE_REFILL_FRAMES * m_usBlockAlign;
	XAudio2Buffer.pAudioData = (BYTE*)( m_ppvRingBuffer[m_uReadBufferIndex] );
	m_uReadBufferIndex = m_uReadBufferIndex < (AK_MAX_NUM_VOICE_BUFFERS - 1) ? m_uReadBufferIndex + 1 : 0;

	HRESULT hResult = m_pSourceVoice->SubmitSourceBuffer(&XAudio2Buffer);
	
	// Do not release the buffer in ring buffer here, as SubmitSourceBuffer holds the pointer
	// until OnBufferEnd.

	return SUCCEEDED(hResult);
}

//====================================================================================================
// this is the entry point for passing buffers
//====================================================================================================
void CAkSinkXAudio2::PassData()
{
	if (m_bCriticalError)
		return;

	// Check first to preserve the valid range [0, AK_NUM_VOICE_BUFFERS]
	if (m_nbBuffersRB == AK_NUM_VOICE_BUFFERS)
	{
		AKPLATFORM::OutputDebugMsg(L"Audio Buffer Dropped!\n");
		return;
	}

	AKASSERT( m_pSourceVoice );

	AkUInt32 uRefilledFrames = m_MasterOut.uValidFrames;
	AKASSERT( uRefilledFrames != 0 );

	void* pvBuffer = m_ppvRingBuffer[m_uWriteBufferIndex];

	AkUInt8* pbSourceData = (AkUInt8*)m_MasterOut.GetInterleavedData();

	AkInt32 lNumSamples = uRefilledFrames * m_ulNumChannels;
	AKASSERT( !( lNumSamples % 4 ) ); // we operate 4 samples at a time.
	memcpy(pvBuffer, pbSourceData, lNumSamples*sizeof(AkReal32));

	m_uWriteBufferIndex = m_uWriteBufferIndex < (AK_MAX_NUM_VOICE_BUFFERS - 1) ? m_uWriteBufferIndex + 1 : 0;

	// This is the only thread incrementing this value
	// so we won't exceed the upper range since we checked first
	::InterlockedIncrement(&m_nbBuffersRB);
}

void CAkSinkXAudio2::PassSilence()
{
	if (m_bCriticalError)
		return;

	// Check first to preserve the valid range [0, AK_NUM_VOICE_BUFFERS]
	if (m_nbBuffersRB == AK_NUM_VOICE_BUFFERS)
	{
		AKPLATFORM::OutputDebugMsg(L"Audio Buffer Dropped!\n");
		return;
	}

	AKASSERT( m_pSourceVoice );

	void* pvBuffer = m_ppvRingBuffer[m_uWriteBufferIndex];

	AkInt32 lNumSamples = AK_NUM_VOICE_REFILL_FRAMES * m_ulNumChannels;

	AkZeroMemAligned(pvBuffer, lNumSamples*sizeof(AkReal32));

	m_uWriteBufferIndex = m_uWriteBufferIndex < (AK_MAX_NUM_VOICE_BUFFERS - 1) ? m_uWriteBufferIndex + 1 : 0;

	// This is the only thread incrementing this value
	// so we won't exceed the upper range since we checked first
	::InterlockedIncrement(&m_nbBuffersRB);
}

AKRESULT CAkSinkXAudio2::IsDataNeeded( AkUInt32 & out_uBuffersNeeded )
{
	// All the XAudio sinks need to be synchronized with the rendering thread.  Due to threading timings, we need to query all the sinks at the same time.
	// Normally, IsDataNeededRB should return the same thing, except when the threads are switching at the wrong time...  
	// Therefore, only return the number of buffers we are sure we have space for in all sinks.
	out_uBuffersNeeded = 1000;
	for(XAudio2Sinks::Iterator it = s_XAudio2List.Begin(); it != s_XAudio2List.End(); ++it)
		out_uBuffersNeeded = min(out_uBuffersNeeded, (*it)->IsDataNeededRB());

	// WG-19498: do not submit data from this thread, as this can cause a deadlock.
	return m_bCriticalError ? AK_Fail : AK_Success;
}

AkUInt32 CAkSinkXAudio2::GetChannelCount()
{		
	AkUInt32 uChannelCount = 0;
	if(m_XAudio2.m_XAudioVersion == CXAudio2Base::XAudio27)
	{
		XAUDIO2_DEVICE_DETAILS deviceDetails;		
		if(m_XAudio2.GetDeviceDetails(0, &deviceDetails))
		{
			AKASSERT(!"Unable to query XAudio2 mastering voice");
			return AK_Fail;
		}
		uChannelCount = deviceDetails.OutputFormat.Format.nChannels;
	}
	else
	{
		DWORD dwChannelMask;
		if(!SUCCEEDED(m_pMasteringVoice->GetChannelMask(&dwChannelMask)))
		{
			AKASSERT(!"Unable to get mastering voice channel mask");
			return AK_Fail;
		}
		uChannelCount = AK::GetNumNonZeroBits(dwChannelMask);
	}

	return uChannelCount;
}

//=================================================
//	IXAudio2EngineCallback interface implementation
//=================================================	 

void CAkSinkXAudio2::OnProcessingPassStart()
{
}

void CAkSinkXAudio2::OnProcessingPassEnd()
{
}

void CAkSinkXAudio2::OnCriticalError( HRESULT Error )
{
	m_bCriticalError = true;
	g_pAudioMgr->WakeupEventsConsumer();
}

//=================================================
//	IXAudio2VoiceCallback interface implementation
//=================================================

// Called just before this voice's processing pass begins.
void CAkSinkXAudio2::OnVoiceProcessingPassStart( UINT32 BytesRequired )
{
	XAUDIO2_VOICE_STATE X2VoiceState;
	m_pSourceVoice->GetState( &X2VoiceState );

	UINT SamplesRequired = BytesRequired / m_usBlockAlign;
	UINT BuffersSubmitted = 0;

	// Submit all buffers that have not been submitted yet.
	while ((LONG)X2VoiceState.BuffersQueued < m_nbBuffersRB)
	{
		SubmitPacketRB();
		X2VoiceState.BuffersQueued++;
		BuffersSubmitted++;
	}

	UINT SamplesReady = BuffersSubmitted * AK_NUM_VOICE_REFILL_FRAMES;

	// set only, don't reset; do not report initial starvation (right after voice start)
	if(X2VoiceState.SamplesPlayed && SamplesReady < SamplesRequired)
		m_bStarved = true;

	if(X2VoiceState.BuffersQueued < AK_NUM_VOICE_BUFFERS)
	{
		g_pAudioMgr->WakeupEventsConsumer(); // Voice still not 'full': trigger buffer generation now.		
	}	
}

// Called just after this voice's processing pass ends.
void CAkSinkXAudio2::OnVoiceProcessingPassEnd()
{
	//Not used
}

// Called when this voice has just finished playing a buffer stream
// (as marked with the XAUDIO2_END_OF_STREAM flag on the last buffer).
void CAkSinkXAudio2::OnStreamEnd()
{
	//Not used
}

// Called when this voice is about to start processing a new buffer.
void CAkSinkXAudio2::OnBufferStart( void* pBufferContext)
{
	//Not used
}

// Called when this voice has just finished processing a buffer.
// The buffer can now be reused or destroyed.
void CAkSinkXAudio2::OnBufferEnd( void* pBufferContext)
{
	// This is the only thread decrementing this value
	// so we won't subceed the lower range if we check first.
	if (m_nbBuffersRB > 0)
	{
		::InterlockedDecrement(&m_nbBuffersRB);
	}
	else
	{
		// This means we read too far. This should not happen but if it does,
		// recover by going back one buffer so the read pointer never exceeds the write pointer
		m_uReadBufferIndex = m_uReadBufferIndex == 0 ? AK_MAX_NUM_VOICE_BUFFERS - 1 : m_uReadBufferIndex - 1;
		AKASSERT(false && "Submitted too many buffers, possible race condition with m_nbBuffersRB");
	}

	g_pAudioMgr->WakeupEventsConsumer();

	//SwitchToThread();	//Switch right now.
}

// Called when this voice has just reached the end position of a loop.
void CAkSinkXAudio2::OnLoopEnd( void* pBufferContext)
{
	//Not used
}

// Called in the event of a critical error during voice processing,
// such as a failing XAPO or an error from the hardware XMA decoder.
// The voice may have to be destroyed and re-created to recover fromF
// the error.  The callback arguments report which buffer was being
// processed when the error occurred, and its HRESULT code.
void CAkSinkXAudio2::OnVoiceError( void* pBufferContext, HRESULT Error )
{
	//Not used
}

namespace AK
{
	IUnknown * GetWwiseXAudio2Interface()
	{
		return CAkSinkXAudio2::GetWwiseXAudio2Interface();
	}
};

IUnknown* CAkSinkXAudio2::GetWwiseXAudio2Interface()
{
	//Return any of the sink's interfaces.  It is all the same instance internally.
	if (s_XAudio2List.First())
		return s_XAudio2List.First()->m_XAudio2.GetIUnknown();	
	return NULL;
}

#endif
