/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

//////////////////////////////////////////////////////////////////////
//
// AkLEngine_common.h
//
// Implementation of the Lower Audio Engine.
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_LENGINE_COMMON_H_
#define _AK_LENGINE_COMMON_H_

#include "AkKeyList.h"
#include "AkLEngineStructs.h"
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/SoundEngine/Common/IAkPlatformContext.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkListBare.h>
#include <AK/Tools/Common/AkLock.h>
#include "AkStaticArray.h"
#include "AkVPLSrcCbxNode.h"
#include "AkVPLMixBusNode.h"
#include "AkLEngineCmds.h"


#if (defined AK_ANDROID && !defined AK_LUMIN)
class CAkAndroidSystem;
#endif

#ifdef AK_WIN
	#ifndef AK_USE_UWP_API
		#include <dbt.h>
		class CAkMMNotificationClient;
	#else
		#define AK_WINRT_DEVICENOTIFICATION
	#endif //AK_USE_UWP_API
#endif //AK_WIN

//-----------------------------------------------------------------------------
// CAkLEngine class.
//-----------------------------------------------------------------------------
class CAkLEngine
{
public:

	// Initialize/terminate.
	static void ApplyGlobalSettings( AkPlatformInitSettings *   io_pPDSettings );
	static AKRESULT Init();
	static void	Stop();
	static void Term();

	static void OnThreadStart();
	static void OnThreadEnd();

	static void GetDefaultPlatformThreadInitSettings(
		AkPlatformInitSettings & out_pPlatformSettings // Platform specific settings. Can be changed depending on hardware.
	);

	static void GetDefaultPlatformInitSettings(
		AkPlatformInitSettings & out_pPlatformSettings // Platform specific settings. Can be changed depending on hardware.
		);

	static void GetDefaultOutputSettings(AkOutputSettings & out_settings);	//To be redefined per platform
	static void GetDefaultOutputSettingsCommon(AkOutputSettings & out_settings);

	// Platform functions for hw voice processing
	static bool PlatformSupportsHwVoices();
	static void PlatformWaitForHwVoices();

	// Returns all audio output devices for a platform built-in sinks
	// Should handle all plug-ins in AkBuiltInSinks, except AKPLUGINID_DUMMY_SINK.
	// Should return AK_NotCompatible if the platform does not support the sink plug-in.
	// Should return AK_NotImplemented if only the default id is supported for now.
	static AKRESULT GetPlatformDeviceList(
		AkPluginID in_pluginID,
		AkUInt32& io_maxNumDevices,					///< In: The maximum number of devices to read. Must match the memory allocated for AkDeviceDescription. Out: Returns the number of devices. Pass out_deviceDescriptions as NULL to have an idea of how many devices to expect.
		AkDeviceDescription* out_deviceDescriptions	///< The output array of device descriptions, one per device. Must be preallocated by the user with a size of at least io_maxNumDevices*sizeof(AkDeviceDescription).
	);

	// Perform processing.
	static AkUInt32 GetNumBufferNeededAndSubmit();
	static void Perform();

	// Cross-platform interface for command queue.
	static AKRESULT	AddSound(AkLECmd & io_cmd);
	static CAkVPLSrcCbxNode * ResolveCommandVPL(AkLECmd & io_cmd);
	static AKRESULT VPLTryConnectSource(CAkPBI * in_pContext, CAkVPLSrcCbxNode * in_pCbx);
	static void	VPLDestroySource(CAkVPLSrcCbxNode * in_pCbx, bool in_bNotify = false);
	static void ReevaluateGraph(bool in_bDestroyAll = false);
	static void ConnectSourceToGraph(CAkVPLSrcCbxNode* pCbx);
	static void DisconnectMixable(CAkVPL3dMixable* pCbx);

	static void SetPanningRule(AkOutputDeviceID in_idOutput, AkPanningRule in_panningRule);

	// Bus management.
	static void MixBusParamNotification( AkUniqueID in_MixBusID, NotifParams& in_rParams );
	static void ResetBusVolume(AkUniqueID in_MixBusID);	
	static void EnsureAuxBusExist(CAkVPL3dMixable * in_pCbx, const AkAuxSendValueEx & in_auxSend, const AkListenerSet& in_listeners);
	static void EnableVolumeCallback(AkUniqueID in_MixBusID, bool in_bEnable);
	static void EnableMeteringCallback(AkUniqueID in_MixBusID, AkMeteringFlags in_eMeteringFlags);
	static void BypassBusFx(AkUniqueID in_MixBusID, AkUInt32 in_bitsFXBypass, AkUInt32 in_uTargetMask, CAkRegisteredObj * in_GameObj);
	static void PositioningChangeNotification(AkUniqueID in_MixBusID, AkReal32	in_RTPCValue, AkRTPC_ParameterID in_ParameterID);
	static void UpdateMixBusFX(AkUniqueID in_MixBusID, AkUInt32 in_uFXIndex);
	static void UpdateChannelConfig(AkUniqueID in_MixBusID);
	static void UpdateResourceMonitorPipelineData();

#ifndef AK_OPTIMIZED
	// Lower engine profiling.
	static void GetNumPipelines(AkUInt16 &out_rAudio, AkUInt16& out_rFeedback, AkUInt16& out_rDevMap);
	static void GetPipelineData(AkVarLenDataCreator& in_rCreator);
	static bool SerializeConnections(MonitorSerializer& in_rSerializer, CAkVPL3dMixable* pCbx);
	static void PostSendsData();
	static void RefreshMonitoringMuteSolo();

	static void PostMeterWatches();

	// Live edit
	static void InvalidatePositioning(AkUniqueID in_MixBusID);

	static void RecapParamDelta();

	typedef void(*AkForAllMixCtxFunc)(CAkBehavioralCtx * in_pCtx, void * in_pCookie);

	static void ForAllMixContext(AkForAllMixCtxFunc in_funcForAll, void * in_pCookie);

#endif //AK_OPTIMIZED

	static AKRESULT SetPluginDLLPath(AkOSChar in_szPath);	
	static void GetPluginDLLFullPath(AkOSChar* out_DllFullPath, AkUInt32 in_MaxPathLength, const AkOSChar* in_DllName, const AkOSChar* in_DllPath = NULL);
	
private:
	static AkEvent				m_EventStop;				// Event to stop the thread.
	static AkUInt32				m_uLastStarvationTime;		// Last time we sent a starvation notification		

	static AkOSChar*			m_szPluginPath;

	static AK::IAkPlatformContext* m_pPlatformContext;

	static AKRESULT InitPlatformContext();
	static void TermPlatformContext();

public:
	static AKRESULT				SoftwareInit();
	static void					SoftwareTerm();
	static void					SoftwarePerform();

	static AK::IAkPlatformContext* GetPlatformContext() { return m_pPlatformContext; }

	// Bus management.
	static AkVPL *				CreateVPLMixBus(CAkBusCtx in_BusCtx, AkVPL* in_pParentBus);

	static AkVPL *				GetExistingVPLMixBus(const CAkBusCtx & in_ctxBus);
	static AkVPL *				GetVPLMixBusInternal(CAkBusCtx & in_ctxBus);

	static void					DestroyAllVPLMixBusses();
	static AKRESULT				EnsureVPLExists(CAkVPLSrcCbxNode * in_pCbx, CAkPBI *in_pCtx);
	static AkVPL *				GetVPLMixBus(CAkBusCtx & in_ctxBus);
	static void					GetAuxBus(CAkBus* in_pAuxBus, AkAuxSendValueEx& in_auxSend, CAkVPL3dMixable * in_pCbx);

	typedef AkArray<AkVPL*, AkVPL*, ArrayPoolLEngineDefault> AkArrayVPL;
	typedef AkArray<AkInt32, AkInt32, ArrayPoolLEngineDefault> AkArrayInt32;
	typedef AkArray<bool, bool, ArrayPoolLEngineDefault> AkArrayBool;
	typedef AkArray<CAkVPLSrcCbxNode*, CAkVPLSrcCbxNode*, ArrayPoolLEngineDefault> AkArrayVPLSrcs;

	static bool					SortSiblingVPLs(AkArrayVPL & in_pVPLs, AkInt32 in_iDepth, AkInt32 in_iRangeStart, AkInt32 in_iRangeLen);

	// Execution.
	static void					RunVPL(CAkVPLSrcCbxNode * in_pCbx, AkVPLState & io_state);
	static void					AnalyzeMixingGraph();
	static void					FinishRun(CAkVPLSrcCbxNode * in_pCbx, AkVPLState & io_state);
	static void					PreprocessSources(bool in_bRender, AkUInt32 &out_idxFirstHwVoice, AkUInt32 &out_idxFirstLLVoice);
	static void					ProcessGraph(bool in_bRender);
	static void					ProcessSources(bool in_bRender);
	static void					PerformMixing(bool in_bRender);
	static AkAudioBuffer * TransferBuffer(AkVPL* in_pVPL);	// Returns buffer if it needs propagating to mix connections
	static void					ProcessVPLList(AkArrayVPL& in_VPLList);
	static void					HandleStarvation();
	static void					ResetMixingVoiceCount();

#if defined(AK_HARDWARE_DECODING_SUPPORTED)
	// Hardware decoding.
	static void					DecodeHardwareSources();
#endif

	// VPL management.
	static void					RemoveMixBusses();
	static CAkVPLSrcCbxNode *	FindExistingVPLSrc(CAkPBI * in_pCtx);

	static void StopMixBussesUsingThisSlot(const CAkUsageSlot* in_pSlot);
	static void ResetAllEffectsUsingThisMedia(const AkUInt8* in_pData);
	static inline void SetVPLsDirty() { m_bVPLsDirty = true; }

private:
    static void VoiceRangeTask(void* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, AkTaskContext in_ctx, void* in_pUserData);
	static void BusRangeTask(void* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, AkTaskContext in_ctx, void* in_pUserData);

	static void BusTask(AkVPL * in_pVPL);
	static void FeedbackTask(AkVPL * in_pVPL);

	static void VPLRefreshDepth(AkVPL * in_pVPL, AkInt32 in_iDepth, AkInt32 & io_iMaxDepth, bool & io_bHasCycles);
	static AKRESULT SortVPLs();
	
	static AkArrayVPLSrcs		m_Sources;					// All sources.

	static AkArrayVPL			m_arrayVPLs;                  // VPLs (busses)
	static AkArrayInt32			m_arrayVPLDepthCnt;           // Number of VPLs in each depth bin.
	static AkArrayBool			m_arrayVPLDepthInterconnects; // Same-depth connections in each depth bin.
	static bool					m_bVPLsHaveCycles;            // True if there are cycles (feedback loops) in the VPL graph
	static bool					m_bVPLsDirty;                 // True if m_arrayVPLs need re-sorting (after insertion/deletion/reconnection)
	static bool					m_bFullReevaluation;


#if defined(AK_NX)
public:
	static AkEvent & GetProcessEvent() { return m_eventProcess; }
private:
	static AkEvent m_eventProcess;
#if defined(AK_USE_SINK_EVENT)
public:
	static nn::os::SystemEvent & GetSinkEvent() { return m_eventSink; }
private:
	static nn::os::SystemEvent m_eventSink;
#endif // defined(AK_USE_SINK_EVENT)
#endif // defined(AK_NX) 

#ifdef AK_WIN
public:
	#if defined(AK_WINRT_DEVICENOTIFICATION)
		static void OnDeviceAdded(Windows::Devices::Enumeration::DeviceWatcher ^, Windows::Devices::Enumeration::DeviceInformation ^);
		static void OnDeviceUpdated(Windows::Devices::Enumeration::DeviceWatcher ^, Windows::Devices::Enumeration::DeviceInformationUpdate ^);
		static void OnDeviceRemoved(Windows::Devices::Enumeration::DeviceWatcher ^, Windows::Devices::Enumeration::DeviceInformationUpdate ^);
		static void OnDeviceEnumerationCompleted(Windows::Devices::Enumeration::DeviceWatcher ^, Platform::Object ^);
	#endif

private:
	static void RegisterDeviceChange();
	static void UnregisterDeviceChange();
	static LRESULT CALLBACK InternalWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	#if !defined(AK_USE_UWP_API) 
		static WNDPROC				m_WndProc;					// The WndProc of the game.
		static HDEVNOTIFY			m_hDevNotify;
		static CAkMMNotificationClient * m_pMMNotifClient;
	#elif defined(AK_WINRT_DEVICENOTIFICATION)
		static Windows::Devices::Enumeration::DeviceWatcher^ m_rWatcher;
		static bool											 m_bWatcherEnumerationCompleted;
	#endif

#endif //AK_WIN

#if defined( AK_XBOX ) || defined( AK_WIN )
public:
	static HANDLE GetProcessEvent() { return m_eventProcess; }
private:
	static HANDLE m_eventProcess;
#endif
    
#ifdef AK_IOS
public:
    static void RegisterNotifications();
#endif
#if (defined AK_ANDROID && !defined AK_LUMIN)
public:
	static CAkAndroidSystem * m_androidSystem;
#endif
};

#endif//_AK_LENGINE_COMMON_H_
