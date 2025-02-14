/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version: <VERSION>  Build: <BUILDNUMBER>
  Copyright (c) <COPYRIGHTYEAR> Audiokinetic Inc.
 ***********************************************************************/

#include "stdafx.h"
#include "AkOutputMgr.h"
#include "Ak3DListener.h"
#include "AkProfile.h"
#include "AkLEngine.h"
#include "AkSinkBase.h"
#include "AkRuntimeEnvironmentMgr.h"
#include "AkAudioMgr.h"
#include "AkCaptureMgr.h"
#include "AkVPL.h"
#include "AkEffectsMgr.h"
#include "AkFXMemAlloc.h"
#ifdef AK_IOS
#include "AkSink.h"
#endif

extern AkPlatformInitSettings g_PDSettings;
extern AkInitSettings g_settings;
extern CAkLock g_csMain;

AkRamp CAkOutputMgr::m_masterVolume;
AkOSChar *CAkOutputMgr::m_szCaptureNameForSecondary = NULL;
AkUInt32 CAkOutputMgr::m_uCaptureExtension = 0;
bool CAkOutputMgr::m_bResetMainDevice = false;
bool CAkOutputMgr::m_bAllSlaves = false;
AkThread CAkOutputMgr::m_hSuspendedThread;
bool CAkOutputMgr::m_bStopSuspendThread = false;
AkEvent CAkOutputMgr::m_hRunSuspendedThread;
bool CAkOutputMgr::m_bDeviceSuspended = false;
bool CAkOutputMgr::m_bRenderWhileSuspended = true;
AkUInt32 CAkOutputMgr::m_uStateDelay = 0;
AkDeviceList CAkOutputMgr::m_listDevices;
AkListenerSet CAkOutputMgr::m_setAssignedListeners;
AK::AkDeviceStatusCallbackFunc CAkOutputMgr::s_pSinkCallback = NULL;
bool CAkOutputMgr::m_bNeverInitialized = true;

#define SYSTEM_SHARESET_ID 3859886410UL

#define CAPTURE_ID_LEN 20

//
// IAkSinkPluginContext
//

AKRESULT AkDevice::GetOutputID(AkUInt32 & out_uOutputID, AkPluginID & out_uDevicePluginID) const
{
	out_uDevicePluginID = uSinkPluginID;
	out_uOutputID = AK_GET_DEVICE_ID_FROM_DEVICE_KEY(uDeviceID);
	return AK_Success;
}
void AkDevice::GetPluginMedia(AkUInt32 /*in_dataIndex*/, AkUInt8* &out_rpData, AkUInt32 &out_rDataSize)
{
	out_rpData = NULL;
	out_rDataSize = 0;
}
void AkDevice::GetPluginCustomGameData( void* &out_rpData, AkUInt32 &out_rDataSize)
{
	out_rpData = NULL;
	out_rDataSize = 0;
}

AKRESULT AkDevice::PostMonitorData(void * /*in_pData*/, AkUInt32 /*in_uDataSize*/)
{
	return AK_Fail;
}
bool AkDevice::CanPostMonitorData()
{
	return false;
}
AKRESULT AkDevice::PostMonitorMessage(const char* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel)
{
#ifndef AK_OPTIMIZED
	MONITOR_PLUGIN_MESSAGE(in_pszError, in_eErrorLevel, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, AK_INVALID_UNIQUE_ID, sink.GetPluginID(), AK_INVALID_UNIQUE_ID);
	return AK_Success;
#else
	return AK_Fail;
#endif
}
AkReal32 AkDevice::GetDownstreamGain()
{
	// No more gain stages downstream.
	return 1.f;
}
AKRESULT AkDevice::GetParentChannelConfig(AkChannelConfig& out_channelConfig) const
{
	out_channelConfig = speakerConfig;
	return AK_Success;
}
#if (defined AK_CPU_X86 || defined AK_CPU_X86_64) && !(defined AK_IOS)
AK::IAkProcessorFeatures * AkDevice::GetProcessorFeatures()
{
	return AkRuntimeEnvironmentMgr::Instance();
}
#endif

bool AkDevice::IsPrimary()
{
	return m_bPrimaryMaster;
}

AKRESULT AkDevice::SignalAudioThread()
{
	if ( m_bPrimaryMaster )
	{
		g_pAudioMgr->WakeupEventsConsumer();
		return AK_Success;
	}
	return AK_Fail;
}

AkUInt16 AkDevice::GetNumRefillsInVoice()
{
	return g_PDSettings.uNumRefillsInVoice;
}

void AkDevice::SetState(DeviceState in_eState)
{
	if (m_eState != in_eState)
	{
		m_eState = in_eState;
		if (AK::SoundEngine::IsInitialized())
			g_pAudioMgr->WakeupEventsConsumer(); // Make sure event thread wakes up (XAudio2)
	}
}

//
// AkDevice
//

AkDevice::AkDevice(AkOutputSettings &in_settings, AkOutputDeviceID in_uKey, bool in_bPrimary)
: uDeviceID(in_uKey)
, eListenerSetType(SetType_Exclusion)
, m_pfSpeakerAngles(NULL)
, uNumAngles( 0 )
, fOneOverMinAngleBetweenSpeakers( 0.f )
, fHeightAngle( 0.f )
, ePanningRule( AkPanningRule_Speakers )
, userSettings(in_settings)
, m_pCapture( NULL )
, m_pCaptureBuffer( NULL )
, m_pDeviceMix(NULL)
, m_eState(eToActivate)
, m_bMixConnected(false)
, m_bPrimaryMaster(in_bPrimary)
, m_bMultiInstance(false)
{
	SetPanningRule( in_settings.ePanningRule );		
}

AkDevice::~AkDevice()
{
	StopOutputCapture();

	ClearSink();

	if ( m_pfSpeakerAngles )
	{
		AkFree(AkMemID_Object, m_pfSpeakerAngles);
	}

	DestroyPanCaches();

	listenersSet.Term();
}

AkVPL* AkDevice::ConnectMix(bool& out_bNewVplCreated)
{
	out_bNewVplCreated = false;
	if (GetSpeakerConfig().IsValid())
	{
		if (m_bMixConnected && m_pDeviceMix == NULL)
		{
			//After the second mix node is connected to the device, create a new node to mix them together.

			CAkBusCtx busCtx(uDeviceID);
			m_pDeviceMix = CAkLEngine::GetExistingVPLMixBus(busCtx); // May actually exist already.
			if (!m_pDeviceMix)
			{
				out_bNewVplCreated = true;
				m_pDeviceMix = AkNew(AkMemID_Processing, AkVPL);
				if (m_pDeviceMix == NULL)
					return NULL;

				if (m_pDeviceMix->Init(busCtx, NULL) != AK_Success)
				{
					AkDelete(AkMemID_Processing, m_pDeviceMix);
					m_pDeviceMix = NULL;
				}
			}
		}

		m_bMixConnected = true;
	}
	return m_pDeviceMix;
}

//For clean up after a memory allocation failure.
void AkDevice::DeleteDeviceAndDisconnectMix()
{
	if (m_pDeviceMix)
		AkDelete(AkMemID_Processing, m_pDeviceMix);//calls TopNodeDeleted()
	
	//if a memory failure occurred, the is already one mix bus connected to this device.
	m_bMixConnected = true;
}

void AkDevice::TopNodeDeleted()
{
	m_bMixConnected = false;
	m_pDeviceMix = NULL;
}

void AkDevice::SetSink( AK::IAkSinkPlugin * in_pSink, AkChannelConfig in_channelConfig) 
{ 	
	sink.pSink = in_pSink;

#ifndef AK_OPTIMIZED
	// Store the "shareset"/"type" portion of the OutputDeviceId to the SinkPluginParams
	// (not the "outputId" part, though, because the pipelineID is intended for unique voices;
	// it doesn't make sense to set as outputId in this case)
	AkUInt32 deviceId, outputId;
	AK_DECOMPOSE_DEVICE_KEY(uDeviceID, deviceId, outputId);
	sink.UpdateContextID(deviceId, AK_INVALID_UNIQUE_ID);
#endif
	speakerConfig = in_channelConfig;	
}

void AkDevice::ClearSink()
{
	sink.Term();
	// Pass an invalid channel config: most efficient.
	SetSink(NULL, AkChannelConfig());
}

void AkDevice::StartOutputCapture( const AkOSChar* in_CaptureFileName )
{
	if (m_pCapture == NULL)
	{
		bool bCaptureBufferReady = false;
		AkChannelConfig speakerConfigCapture = speakerConfig;
		AkUInt16 uNumChannels = (AkInt16) speakerConfigCapture.uNumChannels;

		// Allocate capture buffer.
		if ( !m_pCaptureBuffer )
		{
			m_pCaptureBuffer = AkNew( AkMemID_Processing, AkAudioBuffer );
			if ( m_pCaptureBuffer )
			{
				// Capture buffer is meant to store _interleaved_ data in the _pipeline format_.
				AkUInt32 uDataSize = AK_NUM_VOICE_REFILL_FRAMES * uNumChannels * sizeof(AkCaptureType);
				void * pData = AkMalign( AkMemID_Processing, uDataSize, AK_SIMD_ALIGNMENT );
				if ( pData )
				{
					m_pCaptureBuffer->AttachContiguousDeinterleavedData( pData, AK_NUM_VOICE_REFILL_FRAMES, 0, speakerConfigCapture );
					bCaptureBufferReady = true;
				}
			}
		}

		if ( bCaptureBufferReady )
		{
			m_pCapture = AkCaptureMgr::Instance()->StartCapture(
				in_CaptureFileName,
				AK_CORE_SAMPLERATE,
#ifdef AK_CAPTURE_TYPE_FLOAT
				sizeof(AkReal32) * 8,
				AkFileParser::Float,
#else
				sizeof(AkUInt16) * 8,
				AkFileParser::Int16,
#endif
				speakerConfigCapture
				);
		}
	}
}

void AkDevice::StopOutputCapture()
{
	if (m_pCapture != NULL)
	{
		m_pCapture->StopCapture();
		m_pCapture = NULL;	//Instance is deleted by StopCapture
	}

	if ( m_pCaptureBuffer )
	{
		void * pBuffer = m_pCaptureBuffer->DetachContiguousDeinterleavedData();
		if ( pBuffer )
		{
			AkFalign( AkMemID_Processing, pBuffer );
		}
		AkDelete( AkMemID_Processing, m_pCaptureBuffer );
		m_pCaptureBuffer = NULL;
	}
}

void AkDevice::AddOutputCaptureMarker( const char* in_MarkerText )
{
	if(m_pCapture)
		m_pCapture->AddOutputCaptureMarker(in_MarkerText);
}

void AkDevice::PushData(AkAudioBuffer * in_pBuffer, AkRamp in_baseVolume)
{
	AKASSERT( in_pBuffer->uValidFrames > 0);

	// Add device-specifc volume.
	in_baseVolume.fPrev *= (m_fVolume.fPrev * CAkOutputMgr::GetMasterVolume().fPrev);
	in_baseVolume.fNext *= (m_fVolume.fNext * CAkOutputMgr::GetMasterVolume().fNext);

	if( m_pCapture != NULL )
	{
		AKASSERT( m_pCaptureBuffer );
		AkSinkServices::ConvertForCapture(in_pBuffer, m_pCaptureBuffer, in_baseVolume);
		m_pCaptureBuffer->uValidFrames = in_pBuffer->uValidFrames;
	}

	// Pass to sink.
	Sink()->Consume(in_pBuffer, in_baseVolume);
}

void AkDevice::FrameEnd()
{
	if ( m_pCapture != NULL )
	{
		AkUInt32 uDiskDataSize = m_pCaptureBuffer->MaxFrames() * m_pCaptureBuffer->NumChannels() * sizeof( AkCaptureType );
		if ( m_pCaptureBuffer->uValidFrames == 0 )
		{
			// Data was not pushed during this frame. Clear capture buffer.
			AkZeroMemLarge( m_pCaptureBuffer->GetInterleavedData(), uDiskDataSize );
		}
		m_pCapture->PassSampleData( m_pCaptureBuffer->GetInterleavedData(), uDiskDataSize );
		m_pCaptureBuffer->uValidFrames = 0;
	}

	// Notify sink.
	Sink()->OnFrameEnd();

	// Prepare for next frame.
	m_fVolume.fPrev = m_fVolume.fNext;
}

AKRESULT AkDevice::InitDefaultAngles()
{
	// Set device with default speaker angles.
	AkUInt32 uNumAngles = AK::GetNumberOfAnglesForConfig(AK_SPEAKER_SETUP_DEFAULT_PLANE);
	AkReal32 * pfSpeakerAngles = (AkReal32*)AkAlloca(uNumAngles * sizeof(AkReal32));
	AkReal32 fHeightAngleDeg;
	CAkSpeakerPan::GetDefaultSpeakerAngles(GetSpeakerConfig(), pfSpeakerAngles, fHeightAngleDeg);
	return SetSpeakerAngles(pfSpeakerAngles, uNumAngles, fHeightAngleDeg);	
}

AKRESULT AkDevice::GetSpeakerAngles(
	AkReal32 *			io_pfSpeakerAngles,	// Returned array of loudspeaker pair angles, in degrees relative to azimuth [0,180]. Pass NULL to get the required size of the array.
	AkUInt32 &			io_uNumAngles,		// Returned number of angles in io_pfSpeakerAngles, which is the minimum between the value that you pass in, and the number of angles corresponding to the output configuration, or just the latter if io_pfSpeakerAngles is NULL.
	AkReal32 &			out_fHeightAngle	// Elevation of the height layer, in degrees relative to the plane.
	)
{
	if (Sink())
	{
		AkUInt32 uDesiredNumberOfAngles = AK::GetNumberOfAnglesForConfig( AK_SPEAKER_SETUP_DEFAULT_PLANE );
		if ( !io_pfSpeakerAngles )
		{
			// Just querying the number of angles required.
			io_uNumAngles = uDesiredNumberOfAngles;
		}
		else
		{
			io_uNumAngles = AkMin( io_uNumAngles, uDesiredNumberOfAngles );
			CAkSpeakerPan::ConvertSpeakerAngles(m_pfSpeakerAngles, io_uNumAngles, io_pfSpeakerAngles);
		}

		// Convert height angle to degrees.
		out_fHeightAngle = 360.f * fHeightAngle / TWOPI;
		return AK_Success;
	}
	return AK_Fail;
}

AKRESULT AkDevice::SetSpeakerAngles(
	const AkReal32 *	in_pfSpeakerAngles,	// Array of loudspeaker pair angles, expressed in degrees relative to azimuth ([0,180]).
	AkUInt32			in_uNumAngles,		// Number of loudspeaker pair angles.
	AkReal32 			in_fHeightAngle		// Elevation of the height layer, in degrees relative to the plane.
	)
{
	AKASSERT(Sink() != NULL && in_uNumAngles > 0);

	// Check height.
	if (in_fHeightAngle < -90.f || in_fHeightAngle > 90.f)
	{
		AKASSERT(!"Invalid height layer angle");
		return AK_InvalidParameter;
	}

	// Allocate memory for angles.
	AkUInt32 uTotalAngles = AkMax(uNumAngles, in_uNumAngles);
	AkReal32 *pfNewSpeakerAngles = (AkReal32*)AkAlloc(AkMemID_Object, uTotalAngles * sizeof(AkReal32));
	if (!pfNewSpeakerAngles)
		return AK_Fail;

	// Create temporary array with new and previous angles (if applicable).
	AkReal32 * pfSpeakerAngles = (AkReal32 *)AkAlloca(uTotalAngles * sizeof(AkReal32));
	for (AkUInt32 uAngle = 0; uAngle < in_uNumAngles; uAngle++)
	{
		pfSpeakerAngles[uAngle] = in_pfSpeakerAngles[uAngle];
	}
	for (AkUInt32 uAngle = in_uNumAngles; uAngle < uNumAngles; uAngle++)
	{
		pfSpeakerAngles[uAngle] = 360.f * m_pfSpeakerAngles[uAngle] / TWOPI;
	}

	AkReal32 fNewMinAngleBetweenSpeakers;
	AKRESULT eResult = CAkSpeakerPan::SetSpeakerAngles(
		pfSpeakerAngles,
		in_uNumAngles,
		pfNewSpeakerAngles,
		fNewMinAngleBetweenSpeakers
		);
	if (eResult == AK_Success)
	{
		if (m_pfSpeakerAngles)
		{
			AkFree(AkMemID_Object, m_pfSpeakerAngles);
		}

		m_pfSpeakerAngles = pfNewSpeakerAngles;
		uNumAngles = uTotalAngles;
		fOneOverMinAngleBetweenSpeakers = 1.f / fNewMinAngleBetweenSpeakers;
		// Store height angle in radians.
		AkReal32 fHeightAngleRad = TWOPI * in_fHeightAngle / 360.f;
		fHeightAngle = fHeightAngleRad;
	}
	else
	{
		AkFree(AkMemID_Object, pfNewSpeakerAngles);
	}

	// Refresh pan cache.
	{
		CAkSpeakerPan::MapConfig2PanCache::Iterator it = m_mapConfig2PanCache.Begin();
		while (it != m_mapConfig2PanCache.End())
		{
			CAkSpeakerPan::CreatePanCache((*it).key, m_pfSpeakerAngles, fHeightAngle, (*it).item);
			++it;
		}
	}

	// Invalidate decoding matrices.
	{
		CAkSpeakerPan::MapConfig2DecodeMx::Iterator it = m_mapConfig2DecodeMx.Begin();
		while (it != m_mapConfig2DecodeMx.End())
		{
			AkFree(AkMemID_Object, (*it).item);
			++it;
		}
		m_mapConfig2DecodeMx.RemoveAll();
	}

	{
		CAkSpeakerPan::MapConfig2Spread::Iterator it = m_spreadCache.Begin();
		while (it != m_spreadCache.End())
		{
			(*it).item.Term();
			++it;
		}
		m_spreadCache.RemoveAll();
	}

	return eResult;
}

void * AkDevice::CreatePanCache(
	AkChannelConfig		in_outputConfig		// config of bus to which this signal is routed.
	)
{
	AKASSERT(!in_outputConfig.HasLFE());
	AKASSERT(in_outputConfig.eConfigType == AK_ChannelConfigType_Standard);	// only std configs atm
	void ** ppConfigPanCache = m_mapConfig2PanCache.Set(in_outputConfig);
	if (ppConfigPanCache)
	{
		AKASSERT(AK::GetNumberOfAnglesForConfig(in_outputConfig.uChannelMask & AK_SPEAKER_SETUP_DEFAULT_PLANE) <= uNumAngles);
		*ppConfigPanCache = NULL;
		CAkSpeakerPan::CreatePanCache(in_outputConfig, m_pfSpeakerAngles, fHeightAngle, *ppConfigPanCache);
	}
	if (!ppConfigPanCache || !*ppConfigPanCache)
	{
		m_mapConfig2PanCache.Unset(in_outputConfig);
		return NULL;
	}
	return *ppConfigPanCache;
}

const CAkSpeakerPan::ArraySimdVector * AkDevice::CreateSpreadCache(
	CAkSpeakerPan::ConfigIn2d3d key
	)
{
	AKASSERT(!m_spreadCache.Exists(key));
	CAkSpeakerPan::ArraySimdVector * pArray = m_spreadCache.Set(key);
	if (pArray)
	{
		if (CAkSpeakerPan::CreateSpreadCache(key, fOneOverMinAngleBetweenSpeakers, *pArray) == AK_Success)
			return pArray;
		m_spreadCache.Unset(key);
	}
	return NULL;
}

void AkDevice::DestroyPanCaches()
{
	{
		CAkSpeakerPan::MapConfig2PanCache::Iterator it = m_mapConfig2PanCache.Begin();
		while (it != m_mapConfig2PanCache.End())
		{
			CAkSpeakerPan::DestroyPanCache((*it).key, (*it).item);
			++it;
		}
		m_mapConfig2PanCache.Term();
	}

	{
		CAkSpeakerPan::MapConfig2DecodeMx::Iterator it = m_mapConfig2DecodeMx.Begin();
		while (it != m_mapConfig2DecodeMx.End())
		{
			AkFree(AkMemID_Object, (*it).item);
			++it;
		}
		m_mapConfig2DecodeMx.Term();
	}

	{
		CAkSpeakerPan::MapConfig2Spread::Iterator it = m_spreadCache.Begin();
		while (it != m_spreadCache.End())
		{
			(*it).item.Term();
			++it;
		}
		m_spreadCache.Term();
	}
}

AKRESULT AkDevice::CreateDummy(DeviceState in_eDummyState)
{
	if (in_eDummyState == eForcedDummy && m_eState == eForcedDummy)
	{ // We already are a Forced Dummy, let it be.
		return AK_Success;
	}

	CAkSinkDummy * pSinkDummy = AkNew(AkMemID_ProcessingPlugin, CAkSinkDummy());
	if (!pSinkDummy)
		return AK_InsufficientMemory;	// NOTE: this should NEVER happen

	AkAudioFormat config;
	config.channelConfig = userSettings.channelConfig;
	AKRESULT res = pSinkDummy->Init(NULL, NULL, NULL, config);
	if (res == AK_Success)
	{
		ClearSink();
		SetSink(pSinkDummy, config.channelConfig);
		m_eState = in_eDummyState;
		uSinkPluginID = AKPLUGINID_DUMMY_SINK;
	}
	return res;
}

AKRESULT AkDevice::ReInitSink()
{
	if(CreateSink() != AK_Success)
	{
		CreateDummy(eForcedDummy);
		return AK_Fail;
	}

	CAkOutputMgr::CallSinkCallback(userSettings, AK::AkAudioDeviceEvent_Initialization, AK_Success);

	//
	// We need to let the Sink the time to Init to prevent timeout.
	// Since the frame was "interrupted" with a possibly lengthy
	// init of the sink, we set the time last buffer to after
	// the sink init is complete to remove this delay from the timeout
	// This prevents infinite timeout loops.
	//
	// Don't reset if it's not successful or there will be no audio
	// frames processed when there are no sound cards
	//
	g_pAudioMgr->StampTimeLastBuffer();

	return AK_Success;
}

// return true if the sink was created.
AKRESULT AkDevice::CreateSink()
{
	AkChannelConfig sinkChannelconfig;
	AkSinkPluginParams OnStackSink;
	AkSinkPluginParams* pNewSink = &OnStackSink;	

	CAkSmartPtr<CAkAudioDevice> spAudioDevice;
	uSinkPluginID = AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_DEFAULT_SINK);
	if(userSettings.audioDeviceShareset != AK_INVALID_UNIQUE_ID)
	{
		spAudioDevice.Attach(g_pIndex->m_idxAudioDevices.GetPtrAndAddRef(userSettings.audioDeviceShareset));
		if(spAudioDevice)
		{
			uSinkPluginID = spAudioDevice->GetFXID(); // Retrieve the plugin ID from the shareset.
		}
		else
		{
			// Most likely there was a typo in the name or it was missing from the init bank.
			MONITOR_ERROREX(AK::Monitor::ErrorCode_AudioDeviceShareSetNotFound, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, userSettings.audioDeviceShareset, false);
			return AK_IDNotFound;
		}
	}
	
	CAkEffectsMgr::EffectTypeRecord* pPluginFactory = CAkEffectsMgr::GetEffectTypeRecord(uSinkPluginID);
	if(pPluginFactory == NULL)
	{
		MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_PluginNotRegistered, uSinkPluginID, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, AK_INVALID_UNIQUE_ID, false);
		return AK_PluginNotRegistered;
	}

	OnStackSink.pSink = (AK::IAkSinkPlugin*)(*pPluginFactory->pCreateFunc)(AkFXMemAlloc::GetLower());
	if (!OnStackSink.pSink)
		return AK_InsufficientMemory;
	
	AkAudioFormat format;
	format.SetAll(
		AK_CORE_SAMPLERATE,
		userSettings.channelConfig,
		AK_LE_NATIVE_BITSPERSAMPLE,
		AK_LE_NATIVE_BITSPERSAMPLE / 8 * userSettings.channelConfig.uNumChannels,
		AK_FLOAT,
		AK_NONINTERLEAVED
		);

	if(spAudioDevice)
	{
		OnStackSink.Clone(spAudioDevice, AkRTPCKey(), NULL, false);
	}

	AKRESULT eResult = OnStackSink.pSink->Init(AkFXMemAlloc::GetLower(), this, OnStackSink.pParam, format);
	if(eResult == AK_Success)
	{
		// Return correct speaker config.
		sinkChannelconfig = format.channelConfig;
		if(sinkChannelconfig.eConfigType == AK_ChannelConfigType_Standard)
			sinkChannelconfig.SetStandard(AK::BackToSideChannels(sinkChannelconfig.uChannelMask));

		if(userSettings.audioDeviceShareset == AK_INVALID_UNIQUE_ID)			
			userSettings.audioDeviceShareset = SYSTEM_SHARESET_ID;							
	}
	else
	{
		// Else all other errors: The engine will retry later.
		OnStackSink.Term();

		CAkOutputMgr::CallSinkCallback(userSettings, AK::AkAudioDeviceEvent_Initialization, eResult);		

		if (eResult != AK_Fail)	//AkFail will allow for periodic retries.
		{
			// Audio device Fallback
			// Sink plugin could not initialize due to a system issue (missing drivers or such)
			MONITOR_ERROREX(AK::Monitor::ErrorCode_AudioDeviceInitFailure, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, userSettings.audioDeviceShareset, false);		
		}
		return eResult;
	}

	pNewSink = &OnStackSink;	

	AKRESULT resReset = pNewSink->pSink->Reset();	
	if(resReset != AK_Success)
	{
		CAkOutputMgr::CallSinkCallback(userSettings, AK::AkAudioDeviceEvent_Initialization, resReset);
		pNewSink->Term();
		return resReset;
	}
	
	// Take all data from OnStackSink; OnStackSink will be Term'ed
	sink.TakeFrom( OnStackSink );

	//Only subscribe the real object, not the one on the stack...
	sink.SubscribeRTPC( AkRTPCKey(), NULL );

	SetSink(sink.pSink, sinkChannelconfig);
	m_eState = eActive;	
	
	return AK_Success;
}

AkDevice* CAkOutputMgr::FindDeviceOfType(AkUniqueID in_idShareset)
{	
	for (AkDeviceList::Iterator it = m_listDevices.Begin(); it != m_listDevices.End(); ++it)
	{
		if ((*it)->userSettings.audioDeviceShareset == SYSTEM_SHARESET_ID)
			return (*it);		
	}
	return NULL;
}

AKRESULT CAkOutputMgr::InitDevice(AkDevice*& out_pDevice)
{
	AK_IF_PERF_OFFLINE_RENDERING({ return out_pDevice->CreateDummy(AkDevice::eActive); }); //Only memory issues can occur with dummy.  So don't try to recover from that.
	
	AKRESULT res = AK_Fail;
	while (res != AK_Success)
	{
		res = out_pDevice->CreateSink();
		switch (res)
		{
		case AK_InsufficientMemory:
			return AK_InsufficientMemory;
		case AK_IDNotFound:
		case AK_PluginNotRegistered:
		case AK_NotCompatible:		
		{
			MONITOR_MSG(AKTEXT("Reverting to default Built-in Audio Device."));
			AkDevice* pDefault = FindDeviceOfType(SYSTEM_SHARESET_ID);
			if (pDefault)
			{				
				out_pDevice = pDefault;
				return AK_PartialSuccess;
			}
			else
			{
				//No system output created yet.
				//Override selection, output to System			
				CAkBus::ReplaceTopBusSharesets(out_pDevice->userSettings.audioDeviceShareset, SYSTEM_SHARESET_ID);
				out_pDevice->userSettings.audioDeviceShareset = SYSTEM_SHARESET_ID;
				out_pDevice->userSettings.idDevice = 0;
				out_pDevice->uDeviceID = AK_MAKE_DEVICE_KEY(out_pDevice->userSettings.audioDeviceShareset, out_pDevice->userSettings.idDevice);				
			}
			break;
		}
		case AK_DeviceNotCompatible:
		{
			// Sink does not allow default fallback. Use dummy sink instead.
			MONITOR_MSG(AKTEXT("Reverting to Dummy Audio Device (no output)."));
			return out_pDevice->CreateDummy(AkDevice::eActive);			
		}
		case AK_Fail:
		default:		
		{
			//Device is not ready, disconnected or any other temporary failure.
			//Keep the device and provide a dummy.  Keep it in eForcedDummy to trigger retries later.
			return out_pDevice->CreateDummy(AkDevice::eForcedDummy);			
		}		
		}		
	}

	return res;
}

AKRESULT CAkOutputMgr::Init()
{
	//Start the dummy wait thread that will simulate HW consumption when real HW devices fail.
	m_bStopSuspendThread = false;
	m_bDeviceSuspended = false;
	m_bRenderWhileSuspended = true;
	m_uStateDelay = 0;	
	AKRESULT res = AKPLATFORM::AkCreateEvent(m_hRunSuspendedThread);
	if(res != AK_Success)
		return res;

	AKPLATFORM::AkCreateThread(SuspendedThread, NULL, g_PDSettings.threadOutputMgr, &m_hSuspendedThread, "AK Suspended");
	if (!AkIsValidThread(&m_hSuspendedThread))
		return AK_Fail;

	StartSilentMode(true, false);

	//Init a default Dummy sink as the main output, while we wait for the Init bank to load
	AkOutputSettings dummySettings;
	AkDevice *pDevice = AkNew(AkMemID_Object, AkDevice(dummySettings, 0, true));
	if(!pDevice)
		return AK_InsufficientMemory;

	if(pDevice->CreateDummy(AkDevice::eActive) != AK_Success)
	{
		AkDelete(AkMemID_Object, pDevice);
		return AK_InsufficientMemory;
	}

	AKRESULT eResult = pDevice->InitDefaultAngles();
	if(eResult != AK_Success)
	{
		AkDelete(AkMemID_Object, pDevice);
		return eResult;
	}

	m_listDevices.AddLast(pDevice);

	return AK_Success;
}

void CAkOutputMgr::Term()
{	
	// We need to terminate the master LAST (and we can't iterate backwawrd in a ListBare)
	// The master is the First in the device list.
	AkDevice * pDeviceMaster = NULL;
	AkDeviceList::IteratorEx it = m_listDevices.BeginEx();
	while(it != m_listDevices.End())
	{
		AkDevice * pDevice = (*it);
		if (pDeviceMaster == NULL && pDevice->IsPrimary() )
		{
			pDeviceMaster = (*it);
			++it;
		}
		else
		{
			CAkOutputMgr::CallSinkCallback(pDevice->userSettings, AK::AkAudioDeviceEvent_Removal, AK_Success);
			++it;//must ++ before Deleting it
			AkDelete(AkMemID_Object, pDevice);
		}
	}
	if (pDeviceMaster)
	{
		CAkOutputMgr::CallSinkCallback(pDeviceMaster->userSettings, AK::AkAudioDeviceEvent_Removal, AK_Success);
		AkDelete(AkMemID_Object, pDeviceMaster);
	}
	m_listDevices.Term();
	
	m_masterVolume.fPrev = m_masterVolume.fNext = 1.f;
	if(m_szCaptureNameForSecondary)
	{
		AkFree(AkMemID_Object, m_szCaptureNameForSecondary);
		m_szCaptureNameForSecondary = NULL;
	}

	m_setAssignedListeners.Term();

	m_bAllSlaves = false;
	m_bNeverInitialized = true;
	if(AkIsValidThread(&m_hSuspendedThread))
	{
		m_bStopSuspendThread = true;
		AkSignalEvent(m_hRunSuspendedThread);
		AkWaitForSingleThread(&m_hSuspendedThread);
		AkCloseThread(&m_hSuspendedThread);
		AkClearThread(&m_hSuspendedThread);
	}

	AkDestroyEvent(m_hRunSuspendedThread);
	AkClearEvent(m_hRunSuspendedThread);

	//Reset global variables
	m_bResetMainDevice = false;	
}

AKRESULT CAkOutputMgr::_AddOutputDevice( 
	AkOutputDeviceID in_uKey, 
	AkOutputSettings & in_settings, 
	AkUInt32 in_uDeviceInstance, 
	AkListenerSet& io_listeners,
	AkSetType in_elistenerSetType,	
	bool in_bMainDevice,
	bool in_bDummyIfFail )
{
	AKASSERT( GetDevice(in_uKey) == NULL );

	AK_IF_PERF_OFFLINE_RENDERING({
		in_settings.channelConfig = AK_PERF_OFFLINE_SPEAKERCONFIG;
		AKASSERT(in_settings.channelConfig.IsValid());
		});

	AkDevice* pNewDevice;
	//Allocate a new device	
	pNewDevice = AkNew(AkMemID_Object, AkDevice(in_settings, in_uKey, in_bMainDevice));
	if (!pNewDevice)
		return AK_InsufficientMemory;

	AkDevice* pDevice = pNewDevice;
	AKRESULT eResult = InitDevice(pDevice);	
	if (eResult != AK_Success && eResult != AK_PartialSuccess)
	{
		AkDelete(AkMemID_Object, pNewDevice);
		return eResult;
	}

	if (eResult == AK_Success)
	{
		eResult = pDevice->InitDefaultAngles();
		if (eResult != AK_Success)
		{
			CAkOutputMgr::CallSinkCallback(pDevice->userSettings, AK::AkAudioDeviceEvent_Initialization, AK_InsufficientMemory);
			AkDelete(AkMemID_Object, pDevice);
			return eResult;
		}

		//Detect device types that have multiple instances.  The routing behavior is slightly different when
		//multiple instances are present, we need to rely on the listener-device association directly.
		for (AkDeviceList::IteratorEx it = m_listDevices.BeginEx(); it != m_listDevices.End(); ++it)
		{
			if ((*it)->userSettings.audioDeviceShareset == pDevice->userSettings.audioDeviceShareset)
			{
				//From this point on, we need to discriminate routing between similar devices with listeners.
				if (!(*it)->IsMultiInstance() && (*it)->listenersSet.IsEmpty())
				{				
					//This device did not have any listeners attached, so using default ones.
					//Transfer the default listeners to the device.
					AkListenerSet copy;
					copy.Copy(CAkConnectedListeners::GetDefault()->GetListeners());
					SetListenersOnDevice(copy, SetType_Inclusion, *(*it));
					copy.Term();
				}

				//Check that the new one has listeners distinct from the first one.
				//Otherwise, this device will never get found or used
				if (io_listeners.IsEmpty())
				{
					CAkOutputMgr::CallSinkCallback(pDevice->userSettings, AK::AkAudioDeviceEvent_Initialization, AK_NoDistinctListener);
					AkDelete(AkMemID_Object, pDevice);
					MONITOR_ERRORMSG("No distinct listener provided for AddOutput. Provide a listener specific to the new output.");	//TODO, change to Error_Code in master.
					return AK_NoDistinctListener;
				}

				AkListenerSet testIntersection;
				testIntersection.Copy((*it)->listenersSet);
				AkIntersection(testIntersection, io_listeners);
				bool bEmpty = testIntersection.IsEmpty();
				testIntersection.Term();
				if (!bEmpty)
				{
					CAkOutputMgr::CallSinkCallback(pDevice->userSettings, AK::AkAudioDeviceEvent_Initialization, AK_NoDistinctListener);
					AkDelete(AkMemID_Object, pDevice);
					MONITOR_ERRORMSG("No distinct listener provided for AddOutput. Provide a listener specific to the new output.");	//TODO, change to Error_Code in master.
					return AK_NoDistinctListener;
				}				

				(*it)->SetMultiInstance();
				pDevice->SetMultiInstance();
			}
		}

		// Successful: add to list.
		// Important: Do this before calling SetListenersOnDevice(), as it assumes a main device.
		// Important: Always add at the end.  The first device is assumed to be the main device.
		m_listDevices.AddLast(pDevice);

		if (m_szCaptureNameForSecondary != NULL)
		{
			size_t len = AKPLATFORM::OsStrLen(m_szCaptureNameForSecondary);
			m_szCaptureNameForSecondary[len - 5]++;
			pDevice->StartOutputCapture(m_szCaptureNameForSecondary);
		}
	}
	else if (eResult == AK_PartialSuccess)
	{
		AkDelete(AkMemID_Object, pNewDevice);
	}

	SetListenersOnDevice(io_listeners, in_elistenerSetType, *pDevice);	
	CAkLEngine::ReevaluateGraph();
	CAkOutputMgr::CallSinkCallback(pDevice->userSettings, AK::AkAudioDeviceEvent_Initialization, AK_Success);

	return AK_Success;
}

AkUInt32 CAkOutputMgr::IsDataNeeded()
{
	AK_IF_PERF_OFFLINE_RENDERING({
		return 1;
	})

	// Only main device's sink controls sync. Other sinks get data pushed onto them and we hope
	// that their consumption rate is similar.
	return ManageDevicesState(false);
}

AKRESULT CAkOutputMgr::InitMainDevice(const AkOutputSettings& in_MainSettings)
{
	AkListenerSet empty;
	AkOutputSettings settings = in_MainSettings;	

	AkOutputDeviceID idOutput = AK::SoundEngine::GetOutputID(in_MainSettings.audioDeviceShareset, in_MainSettings.idDevice);
	AkDevice *pDevice = GetPrimaryDevice();
	if(pDevice)
	{
		if(pDevice->uDeviceID == idOutput && !pDevice->IsSlave())
			return AK_Success;

		RemoveOutputDevice(pDevice->uDeviceID, !pDevice->IsSlave());
	}	
		
	AKRESULT res = _AddOutputDevice(idOutput, settings, in_MainSettings.idDevice, empty, SetType_Exclusion, true, true);	
	
	//m_bNeverInitialized is still true if any kind of error occured.
	m_bNeverInitialized = res != AK_Success || m_listDevices.First() == NULL || m_listDevices.First()->userSettings.audioDeviceShareset != in_MainSettings.audioDeviceShareset;
	return res;
}

AKRESULT CAkOutputMgr::AddOutputDevice(AkOutputSettings & in_outputSettings, AkListenerSet& io_listeners)
{
	//Check if device exists
	AkOutputDeviceID idDevice = AK::SoundEngine::GetOutputID(in_outputSettings.audioDeviceShareset, in_outputSettings.idDevice);

	if(GetDevice(idDevice) != NULL)
		return AK_Success;	//Already done.

	return _AddOutputDevice(idDevice, in_outputSettings, in_outputSettings.idDevice, io_listeners, SetType_Inclusion, false, false);	
}

AKRESULT CAkOutputMgr::RemoveOutputDevice( AkOutputDeviceID in_uDeviceID, bool in_bNotify )
{
	for(AkDeviceList::IteratorEx it = m_listDevices.BeginEx(); it != m_listDevices.End(); ++it)
	{
		if ((*it)->uDeviceID == in_uDeviceID)
		{
			AkDevice * pDevice = (*it);			
			
			UnsetListenersOnDevice(pDevice);

			if(in_bNotify && pDevice->uSinkPluginID != AKPLUGINID_DUMMY_SINK)
				CAkOutputMgr::CallSinkCallback((*it)->userSettings, AK::AkAudioDeviceEvent_Removal, AK_Success);
			
			m_listDevices.Erase(it);
			AkDelete(AkMemID_Object, pDevice );
			CAkLEngine::ReevaluateGraph();

			if(m_szCaptureNameForSecondary && m_listDevices.IsEmpty())
			{
				AkFree(AkMemID_Object, m_szCaptureNameForSecondary);
				m_szCaptureNameForSecondary = NULL;
			}
			
			return AK_Success;
		}
	}
	return AK_Fail;
}

AKRESULT CAkOutputMgr::ReplaceDevice(const AkOutputSettings &in_MainSettings, AkOutputDeviceID in_outputID)
{
	// Create a copy of the listeners already set on the output device we will remove, so we can re-add them to the new device
	AkUniqueID sharesetToReplace = AK_INVALID_UNIQUE_ID;
	AkListenerSet listenersToPersist;
	AkSetType listenerSetType = SetType_Inclusion;
	bool wasPrimary = false;

	AkDeviceList::Iterator it = m_listDevices.Begin();
	if (in_outputID == 0)
	{
		//Replace the main output
		for (; it != m_listDevices.End() && !(*it)->IsPrimary(); ++it)
			/*Searching*/;
	}
	else
	{
		for (; it != m_listDevices.End() && (*it)->uDeviceID != in_outputID; ++it)
			/*Searching*/;
	}

	if (it != m_listDevices.End())	//Found it
	{
		sharesetToReplace = (*it)->userSettings.audioDeviceShareset;
		AkUnion(listenersToPersist, (*it)->listenersSet);
		listenerSetType = (*it)->eListenerSetType;
		wasPrimary = (*it)->IsPrimary();		
		in_outputID = (*it)->uDeviceID;
	}

	// Update the Shareset used by any relevant top buses (e.g. Master Audio)
	CAkBus::ReplaceTopBusSharesets(sharesetToReplace, in_MainSettings.audioDeviceShareset);

	AKRESULT res = RemoveOutputDevice(in_outputID, true);
	if (res != AK_Success)
	{
		// If this error occurred, something suspicious happened - AkAudioLib's ReplaceOutput should have checked that the device ID
		// in question already existed. The device may have been removed by something else in-between ReplaceDevice being queued
		// for activity, and then being executed.
		MONITOR_ERROREX(AK::Monitor::ErrorCode_AudioDeviceRemoveFailure, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, (AkUniqueID)in_outputID, false);
	}

	AkOutputSettings settings = in_MainSettings;
	AkOutputDeviceID idOutput = AK::SoundEngine::GetOutputID(settings.audioDeviceShareset, settings.idDevice);
	res = _AddOutputDevice(idOutput, settings, settings.idDevice, listenersToPersist, listenerSetType, wasPrimary, true);
	return res;
}

AkDevice* CAkOutputMgr::FindDevice(const CAkBusCtx& in_ctxBus)
{
	if(in_ctxBus.HasBus())
	{				
		AkUniqueID idDeviceTypeFromBus = in_ctxBus.GetBus()->GetDeviceShareset();

		for(AkDeviceList::IteratorEx it = m_listDevices.BeginEx(); it != m_listDevices.End(); ++it)
		{
			AkDevice * pDevice = *it;			
 			if(pDevice->userSettings.audioDeviceShareset == idDeviceTypeFromBus)
 			{
 				//Check listener link now.
 				if(!pDevice->IsMultiInstance())
 				{
					if( pDevice->listenersSet.Contains(in_ctxBus.GameObjectID()) ||
						(pDevice->listenersSet.IsEmpty() && !m_setAssignedListeners.Contains(in_ctxBus.GameObjectID())))
 						return pDevice;
 				}
 				else
 				{
#if defined(WWISE_AUTHORING)
					//For Authoring, we will discriminate also per device id.  Currently, authoring doesn't support multiple game objects.
					if(in_ctxBus.GetBus()->GetTestDeviceID() == pDevice->userSettings.idDevice)
						return pDevice;
#endif
 					//If there is a listener for discrimination, then it must be in the set.
 					if(pDevice->listenersSet.Contains(in_ctxBus.GameObjectID()))
 						return pDevice;
 				}
 			}
		}
	}
	else
	{
		//Case of Merge-mixbus.
		for(AkDeviceList::IteratorEx it = m_listDevices.BeginEx(); it != m_listDevices.End(); ++it)
		{
			AkDevice * pDevice = *it;
			if(pDevice->uDeviceID == in_ctxBus.DeviceID())
			{
				return pDevice;
			}
		}

		return GetPrimaryDevice();
	}
	return NULL;
}

AkDevice* CAkOutputMgr::GetDevice(AkUniqueID in_shareset, AkUInt32 in_uID)
{
	if(in_shareset == AK_INVALID_UNIQUE_ID)
		return GetPrimaryDevice();
	
	return GetDevice(AK_MAKE_DEVICE_KEY(in_shareset, in_uID));

}

AkDevice* CAkOutputMgr::GetDevice(AkOutputDeviceID in_uID)
{
	if(in_uID == 0)
		return GetPrimaryDevice();

	AkDeviceList::Iterator it = m_listDevices.Begin();
	while(it != m_listDevices.End())
	{
		if((*it)->uDeviceID == in_uID)
			return (*it);
		++it;
	}
	return NULL;
}

AkDevice* CAkOutputMgr::GetPluginDevice(AkPluginID in_uPluginID)
{
	AkDeviceList::Iterator it = m_listDevices.Begin();
	while (it != m_listDevices.End())
	{
		if ((*it)->uSinkPluginID == in_uPluginID)
			return (*it);
		++it;
	}
	return NULL;
}

AkDevice* CAkOutputMgr::GetPrimaryDevice()
{
	for(AkDeviceList::IteratorEx it = m_listDevices.BeginEx(); it != m_listDevices.End(); ++it)
	{
		if((*it)->IsPrimary())
			return (*it);
	}
	return NULL;
}

void CAkOutputMgr::SetListenersOnDevice(AkListenerSet& io_Listeners, AkSetType in_setType, AkDevice& in_device)
{				
	in_device.listenersSet.Transfer(io_Listeners);
	in_device.eListenerSetType = in_setType;
	AkUnion(m_setAssignedListeners, in_device.listenersSet);
}

void CAkOutputMgr::UnsetListenersOnDevice(AkDevice * in_pDevice)
{
	//Check if this device used unique listeners that should now be unassigned.
	bool bReconstruct = !in_pDevice->listenersSet.IsEmpty();
	in_pDevice->listenersSet.RemoveAll();
	if(bReconstruct)
	{
		m_setAssignedListeners.RemoveAll();
		for(AkDeviceList::IteratorEx it = m_listDevices.BeginEx(); it != m_listDevices.End(); ++it)
		{
			if(in_pDevice == *it)
				continue;

			AkUnion(m_setAssignedListeners, (*it)->listenersSet);			
		}
	}	
} 

void CAkOutputMgr::StartOutputCapture( const AkOSChar* in_CaptureFileName )
{
	if(m_szCaptureNameForSecondary || m_bNeverInitialized)
		return;	//Already started.

	size_t len = AKPLATFORM::OsStrLen(in_CaptureFileName);
	if (len == 0)
		return;

	m_szCaptureNameForSecondary = (AkOSChar *)AkAlloc(AkMemID_Object, (len + 2) * sizeof(AkOSChar));
	if (!m_szCaptureNameForSecondary)
		return;

		memcpy(m_szCaptureNameForSecondary, in_CaptureFileName, len* sizeof(AkOSChar));

	m_uCaptureExtension = (AkUInt32)len - 1;
	for(; m_uCaptureExtension != 0 && (char)in_CaptureFileName[m_uCaptureExtension] != '.'; m_uCaptureExtension--)
		/*Empty*/;

	//Recopy the extension
	memcpy(m_szCaptureNameForSecondary + m_uCaptureExtension + 1, in_CaptureFileName + m_uCaptureExtension, (len - m_uCaptureExtension)*sizeof(AkOSChar));

	m_szCaptureNameForSecondary[len+1] = 0;
	m_szCaptureNameForSecondary[m_uCaptureExtension] = '0';
		
	AkDeviceList::Iterator it = m_listDevices.Begin();
	while ( it != m_listDevices.End() )
	{
		if ((*it)->uDeviceID == CAkOutputMgr::GetPrimaryDevice()->uDeviceID)
			(*it)->StartOutputCapture(in_CaptureFileName);
		else
		{							
			m_szCaptureNameForSecondary[m_uCaptureExtension]++;
			(*it)->StartOutputCapture(m_szCaptureNameForSecondary);
		}
		++it;
	}
}

void CAkOutputMgr::StopOutputCapture()
{
	AkDeviceList::Iterator it = m_listDevices.Begin();
	while ( it != m_listDevices.End() )
	{
		(*it)->StopOutputCapture();
		++it;
	}

	if (m_szCaptureNameForSecondary != NULL)
	{
		AkFree(AkMemID_Object, m_szCaptureNameForSecondary);
		m_szCaptureNameForSecondary = NULL;
	}
}

void CAkOutputMgr::AddOutputCaptureMarker(const char* in_MarkerText)
{
	// Copy the parameter content inside another buffer
	size_t textLength = strlen(in_MarkerText);
	char* szMarkerText = (char*) AkAlloca((textLength + 1) * sizeof(char));
	memcpy(szMarkerText, in_MarkerText, (textLength + 1) * sizeof(char));

	AkDeviceList::Iterator it = m_listDevices.Begin();
	while ( it != m_listDevices.End() )
	{
		(*it)->AddOutputCaptureMarker(szMarkerText);
		++it;
	}
}

//Entry point from the message queue.  Handles Suspend & Wakeup API.
AKRESULT CAkOutputMgr::SetDeviceSuspended(bool in_bSuspended, bool in_bRender, AkUInt32 in_uDelay)
{
	if(in_bSuspended == m_bDeviceSuspended && in_bRender == m_bRenderWhileSuspended)
		return AK_PartialSuccess;		//Nothing changed

	m_uStateDelay = AkMax(m_uStateDelay, in_uDelay);
	if(m_uStateDelay > (AkUInt32)AK_MS_PER_BUFFER_TICK)
	{
		m_uStateDelay -= AK_MS_PER_BUFFER_TICK;
		g_pAudioMgr->SuspendWakeup(in_bSuspended, in_bRender, m_uStateDelay);
		if(m_bDeviceSuspended && !m_bRenderWhileSuspended) //If the current state doesn't process messages regularly, wakeup the thread in silent mode.
		{
			in_bRender = true;
			in_bSuspended = true;
		}
		else
			return AK_Success;
	}
	else
		m_uStateDelay = 0;

	if(m_bDeviceSuspended || in_bSuspended)
	{
		//Either already suspended or going to suspend.
		m_bRenderWhileSuspended = in_bRender;
	}
	else
		m_bRenderWhileSuspended = true; //Not suspended?  Always rendering.

	AKRESULT res = AK_Success;
	// NOTE: Set suspended flag regardless whether silent mode op succeeds or not.
	// This gives AudioMgr a signal to repeatedly attempt wake up later.
	m_bDeviceSuspended = in_bSuspended;

	if(in_bSuspended)
		res = StartSilentMode(m_bRenderWhileSuspended);
	else
		ExitSilentMode();

	return res;
}


AKRESULT CAkOutputMgr::StartSilentMode(bool in_bRender, bool in_bNotify)
{
	//Release HW resources from all the real sinks
	//then replace with a dummy sink.
	AKRESULT resFinal = AK_Success;
	AkDeviceList::Iterator it = m_listDevices.Begin();
	for(; it != m_listDevices.End(); ++it)
	{
		AKRESULT res = (*it)->CreateDummy(AkDevice::eForcedDummy);//Handled like an error as it is a temporaryDummy, not a permanent one.
		if(resFinal == AK_Success && res != AK_Success)
			resFinal = res;
	}

	CAkLEngine::ReevaluateGraph();

	m_bAllSlaves = resFinal == AK_Success;
	m_bRenderWhileSuspended = in_bRender;
	if (in_bNotify)
	{
		MONITOR_MSG("Audio thread suspended.  Audio output will be silent.");
	}

	AKPLATFORM::AkSignalEvent(m_hRunSuspendedThread);

	return resFinal;
}

bool CAkOutputMgr::ExitSilentMode()
{
	AkDeviceList::Iterator it = m_listDevices.Begin();
	for (; it != m_listDevices.End(); ++it)
	{
		if ((*it)->GetState() != AkDevice::eActive)
			(*it)->SetState(AkDevice::eToActivate);
	}

	ManageDevicesState(true);	//Recreate sinks.

	return !m_bAllSlaves;
}

void CAkOutputMgr::ResetMainDevice()
{
	m_bResetMainDevice = true;
	if (g_pAudioMgr)
		g_pAudioMgr->WakeupEventsConsumer();
}

AkUInt32 CAkOutputMgr::ManageDevicesState(bool in_bDoReinitNow)
{
	if (AkAudioLibSettings::g_msPerBufferTick <= 0)
	{
		// WG-33816 Div-by-zero protection.
		AkAudioLibSettings::g_msPerBufferTick = (AkUInt32)(AK_DEFAULT_NUM_SAMPLES_PER_FRAME / (DEFAULT_NATIVE_FREQUENCY / 1000.0f));
	}
#if defined( AK_MIN_NUM_REFILLS_IN_VOICE_BUFFER )
	in_bDoReinitNow |= (g_pAudioMgr->GetBufferTick() % (1000 / AkAudioLibSettings::g_msPerBufferTick)) < g_PDSettings.uNumRefillsInVoice;
#else
	in_bDoReinitNow |= (g_pAudioMgr->GetBufferTick() % (1000 / AkAudioLibSettings::g_msPerBufferTick)) < 2;
#endif

	//This check is needed because the default endpoint change on some platform (Windows/XBox) is occurring in a kernel thread that can't block.
	//Getting the main device and setting it's state would require locks and possibly lead to deadlocks.
	if (m_bResetMainDevice && m_listDevices.Begin() != m_listDevices.End())
	{
		m_bResetMainDevice = false;
		CAkOutputMgr::CallSinkCallback(m_listDevices.First()->userSettings, AK::AkAudioDeviceEvent_SystemRemoval, AK_Success);

		m_listDevices.First()->CreateDummy(AkDevice::eForcedDummy);
		m_listDevices.First()->SetState(AkDevice::eToActivate);
		
#if defined(AK_WIN) || defined(AK_IOS)
		in_bDoReinitNow = false;	//Do not reinit now.  Leave enough time to completely release the drivers.
#endif
	}

	AkDevice ** pDevicesToReconnect = (AkDevice **)AkAlloca(m_listDevices.Length() * sizeof(AkDevice *));
	AkUInt32 cDevicesToReconnect = 0;

	bool bWasAllSlaves = m_bAllSlaves;
	bool bAllSlaves = true;

	AkUInt32 uNeeded = 0;
	AkUInt32 uActiveNeeded = (AkUInt32)-1;
	for (AkDeviceList::Iterator it = m_listDevices.Begin(); it != m_listDevices.End(); ++it)
	{
		AkDevice *pCurrent = (*it);
		AkDevice::DeviceState eState = pCurrent->GetState();
		if (pCurrent->Sink())
		{
			if (!m_bDeviceSuspended)
			{
				// Call IsDataNeeded().  The return code tells us the state of the sink.
				// In case of error, we'll try to reset the sink.
				if (eState != AkDevice::eActive)
				{
					// Retry those who aren't active or are in a failure situation.
					eState = AkDevice::eToActivate;
				}

				AKRESULT resDataNeeded = pCurrent->Sink()->IsDataNeeded(uNeeded);
				if(resDataNeeded == AK_Fail)
				{
					//Failure in IsDataNeeded means the device is gone.  Terminate it immediately.
					CAkOutputMgr::CallSinkCallback(pCurrent->userSettings, AK::AkAudioDeviceEvent_SystemRemoval, resDataNeeded);
					pCurrent->CreateDummy(AkDevice::eForcedDummy);
					eState = AkDevice::eToActivate;
				}
			}
		}
		else
			eState = AkDevice::eToActivate;

		bool bIsSlave = pCurrent->IsSlave();
		switch (eState)
		{
		case AkDevice::eToActivate:
			{
				if(!in_bDoReinitNow)
					break;
				
				AKRESULT eResult = pCurrent->ReInitSink();

#ifdef AK_IOS
				//Some platforms have things to do after becoming active (iOS, for example)
				if(bWasAllSlaves && eResult == AK_Success)
					CAkSinkiOS::PostWakeup();
#endif

				pDevicesToReconnect[cDevicesToReconnect++] = (*it);

				if (eResult != AK_Success)
					break;

				if (pCurrent->Sink()->IsDataNeeded(uNeeded) != AK_Success)
					break;

				if (bAllSlaves)
				{
					MONITOR_MSG("Audio thread resumed, audio restarts.");
				}
			}

			//Fallthrough the active case.
		case AkDevice::eActive:
			// While looping, get the actual frames to render, on the active devices.
			// This assumes that all devices are in sync.  TBD, handle out of sync devices.
			if (!bIsSlave)
				uActiveNeeded = AkMin(uActiveNeeded, uNeeded);
			bAllSlaves &= bIsSlave;
			break;

		default:
			break;
		}
	}
	
	m_bAllSlaves = bAllSlaves;

	if (m_listDevices.Length() > 0)
	{
		if(bAllSlaves)
		{
			AK::IAkSinkPlugin* pSink = m_listDevices.First()->Sink();
			if( !pSink || pSink->IsDataNeeded(uActiveNeeded) != AK_Success )
			{
				m_bResetMainDevice = true;	//Retry next frame.
				return 0;
			}
		}

		//If all devices are dummy, there's no HW driving the rendering.  Start the panic mode...
		if(bAllSlaves && !bWasAllSlaves)
		{
			StartSilentMode(true);
			uActiveNeeded = 0;
		}
		else if(!bAllSlaves && bWasAllSlaves && !m_bDeviceSuspended)
		{
			// We're in Silent mode, without being explicitly suspended.  This means there was a HW failure.
			// Now one of the sink is back.  Recompute the real required buffers to render.

			m_listDevices.First()->Sink()->IsDataNeeded(uActiveNeeded);
		}

		if (cDevicesToReconnect > 0)
		{
			CAkLEngine::ReevaluateGraph();
		}
	}
	else
		uActiveNeeded = 0;	//Engine not initialized yet.	

	return uActiveNeeded;
}

//This function will simulate the audio HW pulls while the engine is set into "active suspend"
//Implemented with a thread + sleep because not all platforms have proper timer API (to be revised later).
AK_DECLARE_THREAD_ROUTINE(CAkOutputMgr::SuspendedThread)
{
	AK_INSTRUMENT_THREAD_START("CAkOutputMgr::SuspendedThread");
	const AkUInt32 uTime = AK_MS_PER_BUFFER_TICK;

	while(!m_bStopSuspendThread)
	{
		AKPLATFORM::AkWaitForEvent(m_hRunSuspendedThread);

		//Re-check that the sinks are REAL.
		while(AllSlaves() && !m_bStopSuspendThread && g_pAudioMgr)
		{
			if (m_bRenderWhileSuspended)
			{
				g_pAudioMgr->WakeupEventsConsumer();
			}
			else
			{
#ifndef AK_OPTIMIZED
				g_pAudioMgr->ProcessCommunication();
#endif
			}

			AkSleep(uTime);
		}
	}

	AK::MemoryMgr::TermForThread();
	AkExitThread(AK_RETURN_THREAD_OK);
}
