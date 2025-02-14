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

#ifndef AK_DEVICE_MGR_H
#define AK_DEVICE_MGR_H

#include "AkSpeakerPan.h"
#include <AK/Tools/Common/AkAutoLock.h>
#include <AK/Tools/Common/AkListBare.h>
#include <AK/Tools/Common/AkLock.h>
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include "AkPrivateTypes.h"
#include "AkConnectedListeners.h"
#include "IAkRTPCSubscriberPlugin.h"
#include "AkFxBase.h"
#include "AkBusCtx.h"

class AkCaptureFile;

class AkDevice : public AK::IAkSinkPluginContext
{
public:
	AkDevice(AkOutputSettings &in_settings, AkOutputDeviceID in_uKey, bool in_bPrimary);
	virtual ~AkDevice();

	void PushData(class AkAudioBuffer * in_pBuffer, AkRamp in_baseVolume);
	void FrameEnd();

	inline AkRamp GetVolume() { return m_fVolume; }
	inline void SetNextVolume( AkReal32 in_fVolume ) { m_fVolume.fNext = in_fVolume; }	// Volume offset for this device instance. In dB.
	inline void ResetVolume( AkReal32 in_fVolume ) { m_fVolume.fPrev = m_fVolume.fNext = in_fVolume; }

	AKRESULT CreateSink();
	inline AK::IAkSinkPlugin * Sink() { return sink.pSink; }
	void SetSink( AK::IAkSinkPlugin * in_pSink, AkChannelConfig in_channelConfig);

	void StartOutputCapture( const AkOSChar * in_CaptureFileName );
	void StopOutputCapture();

	void AddOutputCaptureMarker( const char* in_MarkerText );
	

	//
	// Positioning.
	//
	inline AkChannelConfig GetSpeakerConfig() { return speakerConfig; }
	AKRESULT GetSpeakerAngles(
		AkReal32 *			io_pfSpeakerAngles,	// Returned array of loudspeaker pair angles, in degrees relative to azimuth [0,180]. Pass NULL to get the required size of the array.
		AkUInt32 &			io_uNumAngles,		// Returned number of angles in io_pfSpeakerAngles, which is the minimum between the value that you pass in, and the number of angles corresponding to the output configuration, or just the latter if io_pfSpeakerAngles is NULL.
		AkReal32 &			out_fHeightAngle	// Elevation of the height layer, in degrees relative to the plane.
		);

	AKRESULT SetSpeakerAngles(
		const AkReal32 *	in_pfSpeakerAngles,	// Array of loudspeaker pair angles, expressed in degrees relative to azimuth ([0,180]).
		AkUInt32			in_uNumAngles,		// Number of loudspeaker pair angles.
		AkReal32 			in_fHeightAngle		// Elevation of the height layer, in degrees relative to the plane.
		);

	inline void SetPanningRule( AkPanningRule in_ePanningRule ) { ePanningRule = in_ePanningRule; }

	AKRESULT InitDefaultAngles();

	// Creates it if it doesn't exist.
	inline void * GetPanCache(
		AkChannelConfig		in_outputConfig		// config of bus to which this signal is routed.
		)
	{
		AkAutoLock<CAkLock> lock(m_lockPanCache);

		AKASSERT(!in_outputConfig.HasLFE());

		CAkSpeakerPan::MapConfig2PanCache::Iterator it = m_mapConfig2PanCache.FindEx(in_outputConfig);
		if (it != m_mapConfig2PanCache.End())
			return (*it).item;
		else
		{
			return CreatePanCache(in_outputConfig);
		}
	}

	void * CreatePanCache(
		AkChannelConfig		in_outputConfig		// config of bus to which this signal is routed.
		);

	inline const CAkSpeakerPan::ArraySimdVector * GetSpreadCache(
		AkUInt32 in_uNumChansIn,
		bool b3D
		)
	{
		AkAutoLock<CAkLock> lock(m_lockSpreadCache);
		
		CAkSpeakerPan::ConfigIn2d3d key;
		key.uNumChansIn = in_uNumChansIn;
		key.b3D = b3D;
		CAkSpeakerPan::MapConfig2Spread::Iterator it = m_spreadCache.FindEx(key);
		if (it != m_spreadCache.End())
			return &(*it).item;
		else
		{
			return CreateSpreadCache(key);
		}
	}

	const CAkSpeakerPan::ArraySimdVector * CreateSpreadCache(
		CAkSpeakerPan::ConfigIn2d3d key
		);

	inline AKRESULT EnsurePanCacheExists(
		AkChannelConfig	in_outputConfig	// config of bus to which this signal is routed.
		)
	{
		// Pan cache only required for standard configs.
		if (in_outputConfig.eConfigType != AK_ChannelConfigType_Standard)
			return AK_Success;

		// Ignore LFE.
		in_outputConfig = in_outputConfig.RemoveLFE();
		if (in_outputConfig.uNumChannels <= 1)	// No pan cache needed for 1.x and 0.1
			return AK_Success;	// 0.1

		AKRESULT eResult = AK_Success;
		if (!m_mapConfig2PanCache.Exists(in_outputConfig))
			eResult = CreatePanCache(in_outputConfig) != NULL ? AK_Success : AK_Fail;	// Does not exist. Create cache now.

#ifdef AK_LFECENTER
		// If the non-mono config has a center channel, we need to allocate a non-center config too.
		if (in_outputConfig.HasCenter()
			&& eResult == AK_Success )
		{
			in_outputConfig = in_outputConfig.RemoveCenter();
			if (!m_mapConfig2PanCache.Exists(in_outputConfig))
				eResult = CreatePanCache(in_outputConfig) != NULL ? AK_Success : AK_Fail;	// Does not exist. Create cache now.
		}
#endif
		return eResult;
	}

	//
	// AK::IAkPluginContextBase
	//
	virtual AK::IAkGlobalPluginContext * GlobalContext(void) const{ return AK::SoundEngine::GetGlobalPluginContext(); }
	virtual AK::IAkGameObjectPluginInfo * GetGameObjectInfo() { return NULL; }
	virtual AKRESULT GetOutputID(AkUInt32 & out_uOutputID, AkPluginID & out_uDeviceType) const;
	virtual void GetPluginMedia(AkUInt32 in_dataIndex, AkUInt8* &out_rpData, AkUInt32 &out_rDataSize);
	virtual void GetPluginCustomGameData( void* &out_rpData, AkUInt32 &out_rDataSize );
	
	virtual AKRESULT PostMonitorData(void * in_pData, AkUInt32	in_uDataSize);
	virtual bool	 CanPostMonitorData();
	virtual AKRESULT PostMonitorMessage(const char* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel);
	virtual AkReal32 GetDownstreamGain();
	virtual AKRESULT GetParentChannelConfig(AkChannelConfig& out_channelConfig) const;
#if (defined AK_CPU_X86 || defined AK_CPU_X86_64) && !(defined AK_IOS)
	virtual AK::IAkProcessorFeatures * GetProcessorFeatures();
#endif

	//
	// AK::IAkSinkPluginContext.
	//
	virtual bool IsPrimary();
	virtual AKRESULT SignalAudioThread();
	virtual AkUInt16 GetNumRefillsInVoice();

	//

	enum DeviceState { eForcedDummy, eActive, eToActivate };

	AKRESULT CreateDummy( DeviceState in_eDummyState );
	AKRESULT ReInitSink();

	inline bool IsSlave()
	{
		return !m_bPrimaryMaster || m_eState == eForcedDummy ||  uSinkPluginID == AKPLUGINID_DUMMY_SINK;
	}

	// Internal, non-virtual version.
	inline bool _IsPrimary()
	{
		return m_bPrimaryMaster;
	}	

	inline bool IsMultiInstance() { return m_bMultiInstance; }
	inline void SetMultiInstance() { m_bMultiInstance = true; }
	
	DeviceState GetState() const { return m_eState; }
	void SetState(DeviceState in_eState);

	class AkVPL* ConnectMix(bool& out_bMixBusCreated);
	void DeleteDeviceAndDisconnectMix();
	void TopNodeDeleted();
	const AkVPL *GetExtraMixNode() const { return m_pDeviceMix; }


private:
	void ClearSink();
	void DestroyPanCaches();
	
public:	/// TODO Make this protected.

	AkDevice *			pNextItem;			// AkListBare sibling.

	AkOutputDeviceID	uDeviceID;			//Device ID
	AkPluginID			uSinkPluginID;
	AkListenerSet		listenersSet;		//Listener mask.  All these listeners output on this device.

	AkSetType eListenerSetType;

	// 3D positioning.
	AkChannelConfig		speakerConfig;
protected:
	AkReal32 *			m_pfSpeakerAngles;	// Angles in uint, [0,PI] -> [0,PAN_CIRCLE/2]
public:	/// TODO Make this protected.
	AkUInt32			uNumAngles;
	AkReal32			fOneOverMinAngleBetweenSpeakers;	// 1 / (uMinAngleBetweenSpeakers)
	AkReal32 			fHeightAngle;		// Elevation of the height layer, in RADIANS relative to the plane.
	AkPanningRule		ePanningRule;
	AkOutputSettings	userSettings;

	IAkRTPCSubscriberPlugin * GetRTPCSubscriber(){ return &sink; }

	/// REVIEW Consider merging with pan map.
	CAkSpeakerPan::MapConfig2DecodeMx m_mapConfig2DecodeMx;
protected:
	CAkSpeakerPan::MapConfig2PanCache m_mapConfig2PanCache;
	CAkLock m_lockPanCache;
	CAkSpeakerPan::MapConfig2Spread   m_spreadCache;
	CAkLock m_lockSpreadCache;

	AkSinkPluginParams	sink;
	AkRamp				m_fVolume;		// Volume at device-scope (currently only set via SDK). In dB.
	
	AkCaptureFile*		m_pCapture;
	AkAudioBuffer *		m_pCaptureBuffer;			
	AkVPL*				m_pDeviceMix;	
	DeviceState			m_eState;
	bool				m_bMixConnected;//Goes up as top-level vpl-nodes are created and then is reset after they are all deleted.
	bool				m_bPrimaryMaster;	
	bool				m_bMultiInstance;
};

typedef AkListBare<AkDevice, AkListBareNextItem, AkCountPolicyWithCount, AkLastPolicyWithLast > AkDeviceList;

class CAkOutputMgr
{
public:
	static AKRESULT Init();
	static void Term();
	static AKRESULT InitMainDevice(const AkOutputSettings &in_MainSettings);
	static AKRESULT AddOutputDevice(AkOutputSettings & in_outputSettings, AkListenerSet& io_listeners);
	static AKRESULT RemoveOutputDevice( AkOutputDeviceID in_uDeviceID, bool in_bNotify = true );
	static AKRESULT ReplaceDevice(const AkOutputSettings &in_MainSettings, AkOutputDeviceID in_outputID);
	static void SetListenersOnDevice(AkListenerSet& io_Listeners, AkSetType in_setType, AkDevice& in_device);
	
	static AkUInt32 IsDataNeeded();

	static AkForceInline AkUInt32 NumDevices() {return m_listDevices.Length();}
	static AkForceInline AkDeviceList::Iterator OutputBegin() {return m_listDevices.Begin();}
	static AkForceInline AkDeviceList::Iterator OutputEnd() {return m_listDevices.End();}

	static AkDevice* GetDevice(AkUniqueID in_shareset, AkUInt32 in_uID);
	static AkDevice* GetDevice(AkOutputDeviceID in_uID);
	static AkDevice* GetPluginDevice(AkPluginID in_uPluginID);

	static AkDevice* FindDevice(const AK::CAkBusCtx& in_ctxBus);
	static AkDevice* FindDevice(AkGameObjectID in_listenerID);
	static AkDevice* GetPrimaryDevice();	

	// The bus context passed in parameter may be connected to a device if the device that owns this listener/gameobject ID is found, is in the proper hierarchy (master, secondary), and
	// has a valid channel config (i.e. one that can carry data). Otherwise this bus cannot be connected and should therefore not exist.
	static AkForceInline AkDevice* CanConnectToDevice(const AK::CAkBusCtx & in_ctxBus)
	{
		AkDevice* pDevice = FindDevice(in_ctxBus);
		if (!pDevice																	// Device exists for this listener.
			|| !pDevice->GetSpeakerConfig().IsValid()									// Speaker config is not trivial / carries data
			)
		{
			return NULL;
		}
		return pDevice;
	}

	static AkForceInline bool GetDeviceListeners(AkOutputDeviceID in_uID, const AkListenerSet*& out_pListeners, AkSetType& out_setType)
	{
		//Note: the audio-to-motion device will return all listeners (empty exclusion set)
		AkDeviceList::Iterator it = m_listDevices.Begin();
		while (it != m_listDevices.End())
		{
			if ((*it)->uDeviceID == in_uID)
			{
				out_pListeners = &(*it)->listenersSet;
				out_setType = (*it)->eListenerSetType;
				return true;
			}
			++it;
		}
		return false;
	}

	static void UnsetListenersOnDevice(AkDevice * in_pDevice);

	static void StartOutputCapture(const AkOSChar* in_CaptureFileName);
	static void StopOutputCapture();
	static void AddOutputCaptureMarker(const char* in_MarkerText);
	static void GetCaptureName(AkOSChar*& out_szName, AkUInt32& out_iExt) {out_szName = m_szCaptureNameForSecondary; out_iExt = m_uCaptureExtension;}

	// Master volume (all devices) control:
	static inline AkRamp GetMasterVolume() { return m_masterVolume; }
	// SetMasterVolume sets master volume target (at the end of next frame) on all devices.
	// ResetMasterVolume also forces current volume to same value.
	static inline void SetMasterVolume( AkReal32 in_fVolume ) { m_masterVolume.fNext = in_fVolume; }
	static inline void ResetMasterVolume( AkReal32 in_fVolume ) { m_masterVolume.fPrev = m_masterVolume.fNext = in_fVolume; }

	static AKRESULT StartSilentMode(bool in_bRender, bool in_bNotify = true);
	static bool ExitSilentMode();

	static void ResetMainDevice();
	static bool IsResetMainDeviceSet() { return m_bResetMainDevice; }
	
	static bool AllSlaves() {return m_bAllSlaves;}	
	static bool RenderIsActive(){ return !m_bDeviceSuspended || m_bRenderWhileSuspended; }
	static bool IsSuspended(){ return m_bDeviceSuspended; }
	static AKRESULT SetDeviceSuspended(bool in_bSuspended, bool in_bRender, AkUInt32 in_uDelay);

	static void SetSinkCallback(AK::AkDeviceStatusCallbackFunc in_pSinkCallback) { s_pSinkCallback = in_pSinkCallback; }
	static void CallSinkCallback(const AkOutputSettings& in_settings, AK::AkAudioDeviceEvent in_eEvent, AKRESULT in_eRes){ if(s_pSinkCallback) s_pSinkCallback(AK::SoundEngine::GetGlobalPluginContext(), in_settings.audioDeviceShareset, in_settings.idDevice, in_eEvent, in_eRes); }
	
	static bool MainDeviceInitialized() { return !m_bNeverInitialized; }
private:
	static AK_DECLARE_THREAD_ROUTINE(SuspendedThread);
	static AkUInt32 ManageDevicesState(bool in_bDoReinitNow);
	static AKRESULT _AddOutputDevice(AkOutputDeviceID in_uKey, AkOutputSettings & in_outputSettings, AkUInt32 in_uDevice, AkListenerSet& io_listeners, AkSetType in_elistenerSetType, bool in_bMainDevice, bool in_bDummyIfFail);
	static AkDevice* FindDeviceOfType(AkUniqueID in_idShareset);
	static AKRESULT InitDevice(AkDevice*& in_pDevice);
	static AkDeviceList m_listDevices;
	static AkRamp m_masterVolume;

	static AkOSChar *m_szCaptureNameForSecondary;
	static AkUInt32 m_uCaptureExtension;	
	static bool m_bResetMainDevice;	//This variable is needed because the default endpoint change on some platform (Windows/XBox) is occurring in a kernel thread that can't block.	
	static bool m_bAllSlaves;
	
	static bool m_bDeviceSuspended;			// Was the sound engine suspended explicitly?
	static bool	m_bRenderWhileSuspended;	// When suspended, do we still render in background?
	static AkThread	m_hSuspendedThread;
	static AkEvent m_hRunSuspendedThread;
	static bool m_bStopSuspendThread;
	static AkUInt32	m_uStateDelay;	
	static AkListenerSet m_setAssignedListeners;
	static AK::AkDeviceStatusCallbackFunc s_pSinkCallback;
	static bool m_bNeverInitialized;

};
#endif

