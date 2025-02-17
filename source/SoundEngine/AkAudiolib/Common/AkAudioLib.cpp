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

// AkAudioLib.cpp : Defines the entry point for the DLL application.
//
#include "stdafx.h"

#include "AkAudioLib.h"

#include <time.h>
#include "AkAudioMgr.h"
#include "AkEffectsMgr.h"
#include "AkLEngine.h"
#include "AkSwitchMgr.h"
#include "AkRegistryMgr.h"
#include "AkEvent.h"
#include "AkDynamicSequence.h"
#include "AkDialogueEvent.h"
#include "AkModulator.h"
#include "AkTransitionManager.h"
#include "AkPlayingMgr.h"
#include "AkPathManager.h"
#include "AkMonitor.h"
#include "AkState.h"
#include "AkCaptureMgr.h"
#include "AkThreadedBankMgr.h"
#include "AkRanSeqCntr.h"
#include "AkVirtualAcousticsManager.h"

#include "AkURenderer.h"
#include "AkAudioLibTimer.h"
#include "AkProfile.h"
#include "AkEnvironmentsMgr.h"
#include "AkFXMemAlloc.h"
#include "AkPositionRepository.h"
#include "AkLayer.h"
#include "AkOutputMgr.h"
#include "AkQueuedMsg.h"
#include "AkStreamCacheMgmt.h"
#include "AkMidiDeviceMgr.h"
#include "Ak3DListener.h"

#include <AK/SoundEngine/Common/AkDynamicDialogue.h>	// Include here to ensure proper DllExport

#ifdef AK_IOS
#include "AkSink.h"
#endif

#include <AK/Tools/Common/AkFNVHash.h>

#include "AkRuntimeEnvironmentMgr.h"

#include "IAkGlobalPluginContextEx.h"
#include "ICommunicationCentral.h"

#if defined(AK_WIN) && !defined(AK_USE_UWP_API)
#include <process.h>
#endif
//-----------------------------------------------------------------------------
// Behavioral engine singletons.
//-----------------------------------------------------------------------------
CAkAudioLibIndex*		g_pIndex			 = NULL;
CAkAudioMgr*			g_pAudioMgr			 = NULL;
CAkSwitchMgr*			g_pSwitchMgr		 = NULL;
CAkStateMgr*			g_pStateMgr			 = NULL;
CAkRegistryMgr*			g_pRegistryMgr		 = NULL;
CAkBankMgr*				g_pBankManager		 = NULL;
CAkTransitionManager*	g_pTransitionManager = NULL;
CAkPathManager*			g_pPathManager		 = NULL;
AKSOUNDENGINE_API CAkRTPCMgr* g_pRTPCMgr	 = NULL;
CAkEnvironmentsMgr*		g_pEnvironmentMgr	 = NULL;
CAkPlayingMgr*			g_pPlayingMgr		 = NULL;
CAkPositionRepository*  g_pPositionRepository = NULL;
CAkModulatorMgr*		g_pModulatorMgr		 = NULL;
CAkVirtualAcousticsMgr*	g_pVirtualAcousticsMgr = NULL;
CAkBusCallbackMgr*		g_pBusCallbackMgr	 = NULL;

AkUInt64 AKRANDOM::g_uSeed = 0;

#ifndef AK_OPTIMIZED
AK::Comm::ICommunicationCentral * g_pCommCentral	= NULL;
#endif

// Behavioral settings.
AkInitSettings			g_settings;
extern AkPlatformInitSettings g_PDSettings;

#if defined( AK_ENABLE_ASSERTS )

	#if defined( AK_APPLE )
		static void _AkAssertHook(
			const char * in_pszExpression,
			const char * in_pszFileName,
			int in_lineNumber
			)
		{
			printf ("%s:%u failed assertion `%s'\n", in_pszFileName, in_lineNumber, in_pszExpression);
			pthread_kill (pthread_self(), SIGTRAP);
		}
		AKSOUNDENGINE_API AkAssertHook g_pAssertHook = _AkAssertHook;
	#elif defined( AK_ANDROID )
		#include <android/log.h>
		static void _AkAssertHook(
			const char * in_pszExpression,
			const char * in_pszFileName,
			int in_lineNumber
			)
		{
			__android_log_print(ANDROID_LOG_INFO, "AKASSERT","%s:%u failed assertion `%s'\n", in_pszFileName, in_lineNumber, in_pszExpression);
		}
		AKSOUNDENGINE_API AkAssertHook g_pAssertHook = _AkAssertHook;
	#elif defined(AK_WIN) && !defined(AK_USE_UWP_API)

		void AkAssertHookFunc(
			const char* in_pszExpression,
			const char* in_pszFileName,
			int in_lineNumber)
		{
			const int buffSize = 1024;
			char szBuff[buffSize];

			sprintf_s(szBuff, buffSize, "%s:%i Assertion Failed!\n Expression:%s\n(Press Retry to debug the application)", in_pszFileName, in_lineNumber, in_pszExpression);
			int result = MessageBoxA(NULL, szBuff, "Audiokinetic", MB_ABORTRETRYIGNORE | MB_ICONERROR);

			if (result == IDRETRY)
			{
				DebugBreak();
			}
			else if (result == IDABORT)
			{
				exit(0);
			}
		}

		AKSOUNDENGINE_API AkAssertHook g_pAssertHook = AkAssertHookFunc;
	#else
		void AkAssertHookFunc(
			const char* in_pszExpression,
			const char* in_pszFileName,
			int in_lineNumber)
		{
			printf("Assertion Failed! %s:%i: %s\n", in_pszFileName, in_lineNumber, in_pszExpression);
		}
		AKSOUNDENGINE_API AkAssertHook g_pAssertHook = AkAssertHookFunc;
	#endif

#else // !defined( AK_ENABLE_ASSERTS )

		void AkAssertHookFunc(
			const char* in_pszExpression,
			const char* in_pszFileName,
			int in_lineNumber)
		{
			//Do nothing. This is a dummy function, so that g_pAssertHook is never NULL.
		}
		AKSOUNDENGINE_API AkAssertHook g_pAssertHook = AkAssertHookFunc;
#endif

CAkLock g_csMain;

BehavioralExtensionArray g_aBehavioralExtensions[AkGlobalCallbackLocation_Num];
AkExternalStateHandlerCallback g_pExternalStateHandler = NULL;
AkExternalBankHandlerCallback g_pExternalBankHandlerCallback = NULL;
AkExternalUnloadBankHandlerCallback g_pExternalUnloadBankHandlerCallback = NULL;

AkExternalProfileHandlerCallback g_pExternalProfileHandlerCallback = NULL;

void _CallGlobalExtensions(AkGlobalCallbackLocation in_eLocation)
{
	// Match array index with desired location.
	AkUInt32 uIndex = 0;
	while ((1 << uIndex) < in_eLocation)
	{
		uIndex++;
	}
	for (int i = (int)g_aBehavioralExtensions[uIndex].Length() - 1; i >= 0; --i) // from end in case an extension unregisters itself in callback.
	{
		// This will return a start time of 0 for unregistered extensions
		AkAudiolibTimer::Item * pTimerItem = NULL;
		if (g_aBehavioralExtensions[uIndex][i].FXID != 0)
		{
			pTimerItem = AK_START_TIMER(0, g_aBehavioralExtensions[uIndex][i].FXID, 0);
		}

		g_aBehavioralExtensions[uIndex][i].pFunc(AK::SoundEngine::GetGlobalPluginContext(), in_eLocation, g_aBehavioralExtensions[uIndex][i].pCookie);

		AK_STOP_TIMER(pTimerItem);
	}
}

extern AkReal32 g_fVolumeThreshold;
extern AkReal32 g_fVolumeThresholdDB;
AK::SoundEngine::AkCommandPriority g_eVolumeThresholdPriority = AK::SoundEngine::AkCommandPriority_None;
AK::SoundEngine::AkCommandPriority g_eNumVoicesPriority = AK::SoundEngine::AkCommandPriority_None;
AK::SoundEngine::AkCommandPriority g_eNumDangerousVirtVoicesPriority = AK::SoundEngine::AkCommandPriority_None;

AkBankCallbackFunc g_pDefaultBankCallbackFunc = NULL;

char* g_pszCustomPlatformName = NULL;

void AKRANDOM::AkRandomInit(AkUInt64 in_uSeed)
{
	if (in_uSeed == 0)
	{
		in_uSeed = (AkUInt64)time(NULL);
	}

	g_uSeed = in_uSeed;
}


#ifdef AK_WIN
bool CheckDeviceID(AkUInt32 in_idDevice);
#endif

class AutoReserveMsg
{
public:
	inline AutoReserveMsg(AkQueuedMsgType in_eType, AkUInt32 in_uSize)
	{
		m_pItem = g_pAudioMgr->ReserveQueue(in_eType, in_uSize );
	}

	inline ~AutoReserveMsg()
	{
		g_pAudioMgr->FinishQueueWrite();
	}

	AkQueuedMsg * operator->()
	{
		return m_pItem;
	}

	AkQueuedMsg *m_pItem;
};

AkOutputSettings::AkOutputSettings(const char* in_szDeviceShareSet, AkUniqueID in_idDevice, AkChannelConfig in_channelConfig, AkPanningRule in_ePanning):
	audioDeviceShareset(AK::SoundEngine::GetIDFromString(in_szDeviceShareSet)),	
	idDevice(in_idDevice),	
	ePanningRule(in_ePanning),
	channelConfig(in_channelConfig){};

#ifdef AK_SUPPORT_WCHAR
AkOutputSettings::AkOutputSettings(const wchar_t* in_szDeviceShareSet, AkUniqueID in_idDevice, AkChannelConfig in_channelConfig, AkPanningRule in_ePanning):
	audioDeviceShareset(AK::SoundEngine::GetIDFromString(in_szDeviceShareSet)),
	idDevice(in_idDevice),
	ePanningRule(in_ePanning),
	channelConfig(in_channelConfig){};
#endif

namespace AK
{

namespace SoundEngine
{

// Privates

static bool s_bInitialized = false;
AKSOUNDENGINE_API AkAtomic32 g_PlayingID = 0;

#ifndef AK_OPTIMIZED
#define REJECT_INVALID_FLOAT_INPUT // Disable this if float checks are too costly.
#endif

#if defined(REJECT_INVALID_FLOAT_INPUT)
#define CHECK_SOUND_ENGINE_INPUT_VALID
#endif

#if defined(CHECK_SOUND_ENGINE_INPUT_VALID) || defined(REJECT_INVALID_FLOAT_INPUT)

static bool IsValidFloatVolumes( AK::SpeakerVolumes::ConstVectorPtr in_pVolumes, AkUInt32 in_uNumChannels )
{
	bool bValid = true;
	for ( AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++ )
		bValid = bValid && AkMath::IsValidFloatInput( in_pVolumes[uChan] );
	return bValid;
}

#ifdef REJECT_INVALID_FLOAT_INPUT
#define HANDLE_INVALID_FLOAT_INPUT( _msg ) { MONITOR_ERRORMSG( _msg ); return AK_InvalidFloatValue; }
#else
#define HANDLE_INVALID_FLOAT_INPUT( _msg ) MONITOR_ERRORMSG( _msg )
#endif

#endif // CHECK_SOUND_ENGINE_INPUT_VALID

// Forward declarations

static AKRESULT PreInit( AkInitSettings * io_pSettings );
static AKRESULT InitRenderer();

//////////////////////////////////////////////////////////////////////////////////
// MAIN PUBLIC INTERFACE IMPLEMENTATION
//////////////////////////////////////////////////////////////////////////////////



static void _MakeLower( char* in_pString, size_t in_strlen )
{
	for( size_t i = 0; i < in_strlen; ++i )
	{
		if( in_pString[i] >= 'A' && in_pString[i] <= 'Z' )
		{
			in_pString[i] += 0x20;
		}
	}
}

static inline AkUInt32 HashName( const char * in_pString, size_t in_strlen )
{
	AK::FNVHash32 MainHash;
	return MainHash.Compute( (const unsigned char *) in_pString, (unsigned int)in_strlen );
}

#ifdef AK_SUPPORT_WCHAR
AkUInt32 GetIDFromString( const wchar_t* in_pszString )
{
	if( !in_pszString )
		return AK_INVALID_UNIQUE_ID;

	// 1- Make char string.

	char szString[ AK_MAX_STRING_SIZE ];

	AkWideCharToChar( in_pszString, AK_MAX_STRING_SIZE-1, szString );
	szString[ AK_MAX_STRING_SIZE-1 ] = 0;

	size_t stringSize = strlen( szString );

	// 2- Make lower case.
	_MakeLower( szString, stringSize );

	// 3- Hash the resulting string.
	return HashName( szString, stringSize );
}
#endif //AK_SUPPORT_WCHAR

AkUInt32 GetIDFromString( const char* in_pszString )
{
	if( !in_pszString )
		return AK_INVALID_UNIQUE_ID;

	char szString[ AK_MAX_STRING_SIZE ];
	AKPLATFORM::SafeStrCpy( szString, in_pszString, AK_MAX_STRING_SIZE );

	size_t stringSize = strlen( in_pszString );

	// 1- Make lower case.
	_MakeLower( szString, stringSize );

	// 2- Hash the resulting string.
	return HashName( szString, stringSize );
}

bool IsInitialized()
{
	return s_bInitialized;
}

AKRESULT Init(
    AkInitSettings *			in_pSettings,   		///< Sound engine settings. Can be NULL (use default values).
    AkPlatformInitSettings *	in_pPlatformSettings  	///< Platform specific settings. Can be NULL (use default values).
	)
{
	AkPipelineBufferBase::InitFreeListBuckets();

	// g_eVolumeThresholdPriority must be reset at init, to pad the situation where the game would term and re-init the engine.
	g_eVolumeThresholdPriority = AK::SoundEngine::AkCommandPriority_None;
	g_eNumVoicesPriority = AK::SoundEngine::AkCommandPriority_None;
	g_eNumDangerousVirtVoicesPriority = AK::SoundEngine::AkCommandPriority_None;

	AKASSERT( g_pszCustomPlatformName == NULL );
	g_pszCustomPlatformName = NULL;

#if defined AK_WIN && defined AK_CPU_X86
	if ( !AkRuntimeEnvironmentMgr::Instance()->GetSIMDSupport(AK_SIMD_SSE) )
	{
		AKPLATFORM::OutputDebugMsg( "SSE instruction set not supported." );
        return AK_SSEInstructionsNotSupported;
	}
#endif

    // Check Memory manager.
	if ( !MemoryMgr::IsInitialized() )
    {
		AKPLATFORM::OutputDebugMsg("Memory manager is not initialized");
        return AK_MemManagerNotInitialized;
    }
    // Check Stream manager.
    if ( IAkStreamMgr::Get( ) == NULL )
    {
		AKPLATFORM::OutputDebugMsg("Stream manager does not exist" );
        return AK_StreamMgrNotInitialized;
    }

    // Store upper engine global settings.
    if ( in_pSettings == NULL )
    {
		GetDefaultInitSettings( g_settings );
    }
    else
    {
    	// TODO. Check values, clamp to min or max.
        g_settings = *in_pSettings;
    }

	if (g_settings.pfnAssertHook)
	g_pAssertHook = g_settings.pfnAssertHook;

	// Store lower engine global settings.
	CAkLEngine::ApplyGlobalSettings( in_pPlatformSettings );

    // Instantiate, initialize and assign global pointer.

	AKRESULT eResult = AK_Fail;

	AKASSERT( !s_bInitialized || !"SoundEngine::Init should be called only once" );
    if ( !s_bInitialized )
    {
#ifdef AK_IOS
            // Get RemoteIO audio unit in this thread
			CAkSinkiOS::InitRemoteAudioUnit();
#endif
			eResult = PreInit( in_pSettings );
			if( eResult == AK_Success )
				eResult = InitRenderer();

			if ( eResult == AK_Success )
			{
				if ( CAkMidiDeviceMgr::Create() == NULL )
					eResult = AK_Fail;
			}

			if ( eResult != AK_Success )
			{
                Term();
			}
			else
			{
				s_bInitialized = true;
			}
        }

	ValidateMessageQueueTypes();	//Compile time assert.  Will compile to nothing.

	_CallGlobalExtensions(AkGlobalCallbackLocation_Init);

#ifdef AK_DELTAMONITOR_RELEASE
	AkDeltaMonitorPrint::Init("VoiceInspectorDump.txt");
#endif

    return eResult;
}

void GetDefaultInitSettings(
    AkInitSettings & out_settings   		///< Default sound engine settings returned
	)
{
	out_settings.pfnAssertHook = NULL;
	out_settings.uMaxNumPaths = DEFAULT_MAX_NUM_PATHS;
	out_settings.uCommandQueueSize = COMMAND_QUEUE_SIZE;
	out_settings.bEnableGameSyncPreparation = false;
	out_settings.uContinuousPlaybackLookAhead = DEFAULT_CONTINUOUS_PLAYBACK_LOOK_AHEAD;

	out_settings.uNumSamplesPerFrame = AK_DEFAULT_NUM_SAMPLES_PER_FRAME;

    out_settings.uMonitorQueuePoolSize = MONITOR_QUEUE_DEFAULT_SIZE;

	out_settings.bUseSoundBankMgrThread = true;
	out_settings.bUseLEngineThread = true;

	out_settings.uMaxHardwareTimeoutMs = AK_MAX_HW_TIMEOUT_DEFAULT;

	out_settings.BGMCallback = NULL;
	out_settings.BGMCallbackCookie = NULL;

	out_settings.szPluginDLLPath = NULL;

	out_settings.eFloorPlane = AkFloorPlane_Default;
	out_settings.taskSchedulerDesc.fcnParallelFor = NULL;
	out_settings.taskSchedulerDesc.uNumSchedulerWorkerThreads = 1;
	out_settings.bDebugOutOfRangeCheckEnabled = false;
	out_settings.fDebugOutOfRangeLimit = 16.f;

	out_settings.uBankReadBufferSize = AK_DEFAULT_BANK_READ_BUFFER_SIZE;

	// Preparing global default settings; client does not care about the speaker angles at this point.
	CAkLEngine::GetDefaultOutputSettings(out_settings.settingsMainOutput);
}

void GetDefaultPlatformInitSettings(
    AkPlatformInitSettings &	out_platformSettings  	///< Default platform specific settings returned
	)
{
	CAkLEngine::GetDefaultPlatformInitSettings( out_platformSettings );
}

void Term()
{
	s_bInitialized = false;

#ifdef AK_DELTAMONITOR_RELEASE
	AkDeltaMonitorPrint::Term();
#endif

	//Stop the audio and bank threads before completely destroying the objects.
	if (g_pAudioMgr)
	{
		g_pAudioMgr->Stop();
	}

	if(g_pBankManager)
	{
		g_pBankManager->StopThread();
	}

	if (g_pModulatorMgr)
	{
		//Make sure all modulators are stopped
		g_pModulatorMgr->TermModulatorEngine();
	}

	CAkMidiDeviceMgr * pMidiMgr = CAkMidiDeviceMgr::Get();
	if ( pMidiMgr )
		pMidiMgr->Destroy();

	// Need to remove all playback objects before doing the last call on behavioral extensions
	CAkLEngine::Stop();

	// Callback global hooks registered to location 'Term' and term arrays.
	_CallGlobalExtensions(AkGlobalCallbackLocation_Term);
	for (int iLoc = 0; iLoc < AkGlobalCallbackLocation_Num; ++iLoc )
		g_aBehavioralExtensions[iLoc].Term();

	CAkURenderer::Term();

	if (g_pAudioMgr)
	{
		g_pAudioMgr->Term();
		AkDelete(AkMemID_SoundEngine,g_pAudioMgr);
		g_pAudioMgr = NULL;
	}

	if(g_pBankManager)
	{
		g_pBankManager->Term();
		AkDelete(AkMemID_SoundEngine,g_pBankManager);
		g_pBankManager = NULL;
	}

	if (g_pIndex)
	{
		// Must be done after unloading banks and killing the bank manager and before releasing others managers.
		g_pIndex->ReleaseTempObjects();
		g_pIndex->ReleaseDynamicSequences();
	}

	if( g_pszCustomPlatformName )
	{
		AkFree(AkMemID_SoundEngine,g_pszCustomPlatformName);
		g_pszCustomPlatformName = NULL;
	}

	if(g_pPathManager)
	{
		g_pPathManager->Term();
		AkDelete(AkMemID_SoundEngine,g_pPathManager);
		g_pPathManager = NULL;
	}

	if(g_pTransitionManager)
	{
		g_pTransitionManager->Term();
		AkDelete(AkMemID_SoundEngine,g_pTransitionManager);
		g_pTransitionManager = NULL;
	}

	if (g_pRegistryMgr)
	{
		g_pRegistryMgr->Term();
		AkDelete(AkMemID_SoundEngine,g_pRegistryMgr);
		g_pRegistryMgr = NULL;
	}

	if(g_pPlayingMgr)
	{
		g_pPlayingMgr->Term();
		AkDelete(AkMemID_SoundEngine,g_pPlayingMgr);
		g_pPlayingMgr = NULL;
	}

	if(g_pPositionRepository)
	{
		g_pPositionRepository->Term();
		AkDelete(AkMemID_SoundEngine,g_pPositionRepository);
		g_pPositionRepository = NULL;
	}

	if(g_pEnvironmentMgr)
	{
		g_pEnvironmentMgr->Term();
		AkDelete(AkMemID_SoundEngine,g_pEnvironmentMgr);
		g_pEnvironmentMgr = NULL;
	}

	if (g_pSwitchMgr)
	{
		g_pSwitchMgr->Term();
		AkDelete(AkMemID_SoundEngine,g_pSwitchMgr);
		g_pSwitchMgr = NULL;
	}

	if (g_pStateMgr)
	{
		g_pStateMgr->Term();
		AkDelete(AkMemID_SoundEngine,g_pStateMgr);
		g_pStateMgr = NULL;
	}

	if(g_pRTPCMgr)
	{
		AKVERIFY(g_pRTPCMgr->Term() == AK_Success);
		AkDelete(AkMemID_SoundEngine,g_pRTPCMgr);
		g_pRTPCMgr = NULL;
	}

	if(g_pModulatorMgr)
	{
		g_pModulatorMgr->Term();
		AkDelete(AkMemID_SoundEngine,g_pModulatorMgr);
		g_pModulatorMgr = NULL;
	}

	if (g_pIndex)//IMPORTANT : g_pIndex MUST STAY ANTE-PENULTIEME DELETION OF AKINIT()!!!!!!!!!!!!!!!!!!
	{
		g_pIndex->Term();
		AKASSERT(CAkEmitter::List().IsEmpty());
		AKASSERT(CAkListener::List().IsEmpty());
		AkDelete( AkMemID_SoundEngine, g_pIndex );
		g_pIndex = NULL;
	}

	CAkListener::Term();

	CAkParameterTarget::TermTargets();

#ifndef AK_OPTIMIZED
	AkMonitor * pMonitor = AkMonitor::Get();
	if ( pMonitor )
	{
		pMonitor->StopMonitoring();
		AkMonitor::Destroy();
	}

	AkResourceMonitor::Destroy();
#endif

	AkPipelineBufferBase::ClearFreeListBuckets();

	AK_PERF_TERM();
	AK_TERM_TIMERS();
}

void ResetGraph()
{
	g_csMain.Lock();
	CAkLEngine::ReevaluateGraph(true);
	g_csMain.Unlock();
}

AKRESULT GetAudioSettings(
	AkAudioSettings& out_audioSettings
	)
{
	if( ! IsInitialized() )
		return AK_Fail;

	out_audioSettings.uNumSamplesPerFrame = AK_NUM_VOICE_REFILL_FRAMES;
	out_audioSettings.uNumSamplesPerSecond = AK_CORE_SAMPLERATE;

	return AK_Success;
}

//////////////////////////////////////////////////////////////////////////////////
// Tell the Audiolib it may now process all the events in the queue
//////////////////////////////////////////////////////////////////////////////////
AKRESULT RenderAudio( bool in_bAllowSyncRender )
{
	AKASSERT(g_pAudioMgr);

	return g_pAudioMgr->RenderAudio( in_bAllowSyncRender );
}

// Component registration.
class CAkGlobalPluginContext : public AK::IAkGlobalPluginContextEx
{
public:
	CAkGlobalPluginContext() {}

	virtual IAkStreamMgr * GetStreamMgr() const
	{
		return AK::IAkStreamMgr::Get();
	}

	virtual const AkPlatformInitSettings* GetPlatformInitSettings() const
	{
		return &g_PDSettings;
	}

	virtual const AkInitSettings* GetInitSettings() const
	{
		return &g_settings;
	}

	virtual AkUInt16 GetMaxBufferLength() const
	{
		return AK_NUM_VOICE_REFILL_FRAMES;
	}

	virtual AkUInt32 GetSampleRate() const
	{
		return AK_CORE_SAMPLERATE;
	}

	virtual bool IsRenderingOffline() const
	{
		return AK_PERF_OFFLINE_RENDERING;
	}

	virtual AKRESULT PostMonitorMessage(
		const char* in_pszError,				///< Message or error string to be displayed
		AK::Monitor::ErrorLevel in_eErrorLevel	///< Specifies whether it should be displayed as a message or an error
		)
	{
		return AK::Monitor::PostString(in_pszError, in_eErrorLevel);
	}

	virtual AKRESULT RegisterPlugin(
		AkPluginType in_eType,						///< Plug-in type (for example, source or effect)
		AkUInt32 in_ulCompanyID,					///< Company identifier (as declared in the plug-in description XML file)
		AkUInt32 in_ulPluginID,						///< Plug-in identifier (as declared in the plug-in description XML file)
		AkCreatePluginCallback in_pCreateFunc,		///< Pointer to the plug-in's creation function
		AkCreateParamCallback in_pCreateParamFunc	///< Pointer to the plug-in's parameter node creation function
		)
	{
		return AK::SoundEngine::RegisterPlugin(in_eType, in_ulCompanyID, in_ulPluginID, in_pCreateFunc, in_pCreateParamFunc);
	}

	virtual AKRESULT RegisterCodec(
		AkUInt32 in_ulCompanyID,						// Company identifier (as declared in XML)
		AkUInt32 in_ulPluginID,							// Plugin identifier (as declared in XML)
		AkCreateFileSourceCallback in_pFileCreateFunc,  // File source.
		AkCreateBankSourceCallback in_pBankCreateFunc   // Bank source.
		)
	{
		return AK::SoundEngine::RegisterCodec(in_ulCompanyID, in_ulPluginID, in_pFileCreateFunc, in_pBankCreateFunc);
	}

	virtual AKRESULT RegisterGlobalCallback(
		AkPluginType in_eType,							// A valid Plug-in type (for example, source or effect). AkPluginTypeNone is not valid.
		AkUInt32 in_ulCompanyID,						// Company identifier (as declared in the plug-in description XML file).
		AkUInt32 in_ulPluginID,							// Plug-in identifier (as declared in the plug-in description XML file). Must be non-zero.
		AkGlobalCallbackFunc in_pCallback,				// Function to register as a global callback.
		AkUInt32 /*AkGlobalCallbackLocation*/ in_eLocation,	// Callback location. Bitwise OR multiple locations if needed.
		void * in_pCookie								// User cookie.
		)
	{
		if (in_eType == AkPluginTypeNone)
		{
			return AK_InvalidParameter;
		}
		else
		{
			return AK::SoundEngine::RegisterGlobalCallback( in_pCallback, in_eLocation, in_pCookie, in_eType, in_ulCompanyID, in_ulPluginID );
		}
	}

	virtual AKRESULT UnregisterGlobalCallback(
		AkGlobalCallbackFunc in_pCallback,				// Function to unregister as a global callback.
		AkUInt32 /*AkGlobalCallbackLocation*/ in_eLocation	// Must match in_eLocation as passed to RegisterGlobalCallback for this callback.
		)
	{
		return AK::SoundEngine::UnregisterGlobalCallback( in_pCallback, in_eLocation);
	}

	virtual AK::IAkPluginMemAlloc * GetAllocator()
	{
		return AkFXMemAlloc::GetUpper();
	}

	virtual AKRESULT SetRTPCValue(
		AkRtpcID in_rtpcID, 									///< ID of the game parameter
		AkRtpcValue in_value, 									///< Value to set
		AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT,///< Associated game object ID
		AkTimeMs in_uValueChangeDuration = 0,					///< Duration during which the game parameter is interpolated towards in_value
		AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,	///< Curve type to be used for the game parameter interpolation
		bool in_bBypassInternalValueInterpolation = false		///< True if you want to bypass the internal "slew rate" or "over time filtering" specified by the sound designer. This is meant to be used when for example loading a level and you dont want the values to interpolate.
		)
	{
		return AK::SoundEngine::SetRTPCValue(in_rtpcID, in_value, in_gameObjectID, in_uValueChangeDuration, in_eFadeCurve, in_bBypassInternalValueInterpolation);
	}

	virtual AKRESULT SendPluginCustomGameData(
		AkUniqueID in_busID,			///< Bus ID
		AkGameObjectID in_busObjectID,	///< Bus Object ID
		AkPluginType in_eType,			///< Plug-in type (for example, source or effect)
		AkUInt32 in_uCompanyID,		///< Company identifier (as declared in the plug-in description XML file)
		AkUInt32 in_uPluginID,			///< Plug-in identifier (as declared in the plug-in description XML file)
		const void* in_pData,			///< The data blob
		AkUInt32 in_uSizeInBytes		///< Size of data
		)
	{
		return AK::SoundEngine::SendPluginCustomGameData(
			in_busID,
			in_busObjectID,
			in_eType,
			in_uCompanyID,
			in_uPluginID,
			in_pData,
			in_uSizeInBytes
			);
	}

	virtual AKRESULT GetAudioSettings(
		AkAudioSettings &	out_audioSettings  	///< Returned audio settings
		) const
	{
		return AK::SoundEngine::GetAudioSettings(out_audioSettings);
	}

	virtual AkUInt32 GetIDFromString(const char* in_pszString) const
	{
		return AK::SoundEngine::GetIDFromString(in_pszString);
	}

	virtual AkPlayingID PostEventSync(
		AkUniqueID in_eventID,							///< Unique ID of the event
		AkGameObjectID in_gameObjectID,					///< Associated game object ID
		AkUInt32 in_uFlags = 0,							///< Bitmask: see \ref AkCallbackType
		AkCallbackFunc in_pfnCallback = NULL,			///< Callback function
		void * in_pCookie = NULL,						///< Callback cookie that will be sent to the callback function along with additional information
		AkUInt32 in_cExternals = 0,						///< Optional count of external source structures
		AkExternalSourceInfo *in_pExternalSources = NULL,///< Optional array of external source resolution information
		AkPlayingID	in_PlayingID = AK_INVALID_PLAYING_ID///< Optional (advanced users only) Specify the playing ID to target with the event. Will Cause active actions in this event to target an existing Playing ID. Let it be AK_INVALID_PLAYING_ID or do not specify any for normal playback.
		)
	{
		AKASSERT(g_pIndex);
		AKASSERT(g_pAudioMgr);
		AKASSERT(g_pPlayingMgr);

		// This function call does get the pointer and addref it in an atomic call.
		CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef(in_eventID);
		if (pEvent)
		{
			AkQueuedMsg_EventBase ev;
			if (AK_EXPECT_FALSE(in_cExternals != 0))
			{
				ev.CustomParam.customParam = 0;
				ev.CustomParam.ui32Reserved = 0;
				ev.CustomParam.pExternalSrcs = AkExternalSourceArray::Create(in_cExternals, in_pExternalSources);
				if (ev.CustomParam.pExternalSrcs == (AkUIntPtr)NULL)
					return AK_INVALID_PLAYING_ID;
			}
			else
			{
				ev.CustomParam.customParam = 0;
				ev.CustomParam.ui32Reserved = 0;
				ev.CustomParam.pExternalSrcs = NULL;
			}

			ev.PlayingID = (AkPlayingID)AkAtomicInc32(&g_PlayingID);
			ev.TargetPlayingID = in_PlayingID;
			ev.gameObjID = in_gameObjectID;

			AKRESULT eResult = g_pPlayingMgr->AddPlayingID(ev, in_pfnCallback, in_pCookie, in_uFlags, pEvent->ID());
			if (eResult != AK_Success)
			{
				pEvent->Release();
				return AK_INVALID_PLAYING_ID;
			}

			// Lock
			{
				CAkFunctionCritical GlobalLock;

				MONITOR_EVENTTRIGGERED(ev.PlayingID, pEvent->ID(), ev.gameObjID, ev.CustomParam);

				CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(ev.gameObjID);

				CAkAudioMgr::ExecuteEvent(pEvent, pGameObj, ev.gameObjID, ev.PlayingID, ev.TargetPlayingID, ev.CustomParam);

				if (pGameObj)
				{
					pGameObj->Release();
				}
				g_pPlayingMgr->RemoveItemActiveCount(ev.PlayingID);

				pEvent->Release();

				if (ev.CustomParam.pExternalSrcs)
					ev.CustomParam.pExternalSrcs->Release();

				return ev.PlayingID;
			}
		}

		MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_EventIDNotFound, in_eventID, AK_INVALID_PLAYING_ID, in_gameObjectID, in_eventID, false);
		return AK_INVALID_PLAYING_ID;
	}

	/// Executes a number of MIDI Events on all nodes that are referenced in the specified Event in an Action of type Play.
	/// Each MIDI event will be posted in AkMIDIPost::uOffset samples from the start of the current frame. The duration of
	/// a sample can be determined from the sound engine's audio settings, via a call to AK::SoundEngine::GetAudioSettings.
	/// \sa
	/// - <tt>AK::SoundEngine::GetAudioSettings</tt>
	/// - <tt>AK::SoundEngine::StopMIDIOnEvent</tt>
	virtual AKRESULT PostMIDIOnEventSync(
		AkUniqueID in_eventID,											///< Unique ID of the Event
		AkGameObjectID in_gameObjectID,									///< Associated game object ID
		AkMIDIPost* in_pPosts,											///< MIDI Events to post
		AkUInt16 in_uNumPosts											///< Number of MIDI Events to post
		)
	{
		if (in_uNumPosts == 0 || in_pPosts == NULL)
			return AK_InvalidParameter;

		AKASSERT(g_pAudioMgr);
		AKASSERT(g_pIndex);

		// This function call does get the pointer and addref it in an atomic call.
		CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef(in_eventID);
		if (pEvent == NULL)
		{
			MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_EventIDNotFound, in_eventID, AK_INVALID_PLAYING_ID, in_gameObjectID, in_eventID, false);
			return AK_Fail;
		}

		// Here, we need to create the complete variable size object on the stack.
		AkUInt32 uAllocSize = AkQueuedMsg::Sizeof_EventPostMIDIBase() + in_uNumPosts * sizeof(AkMIDIPost);
		if (uAllocSize > g_pAudioMgr->GetMaximumMsgSize())
		{
			MONITOR_ERRORMSG(AKTEXT("AK::SoundEngine::PostMIDIOnEvent: too many event posts."));
			pEvent->Release();
			return AK_InvalidParameter;
		}

		AkQueuedMsg_EventPostMIDI * midi = (AkQueuedMsg_EventPostMIDI *)AkAlloca(uAllocSize);
		midi->pEvent = pEvent;
		midi->eventID = in_eventID;
		midi->gameObjID = in_gameObjectID;
		midi->uNumPosts = in_uNumPosts;
		for (AkUInt16 i = 0; i < in_uNumPosts; ++i)
			midi->aPosts[i] = in_pPosts[i];

		// Lock
		{
			CAkFunctionCritical GlobalLock;

			CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(midi->gameObjID);
			if ((pGameObj && pGameObj->HasComponent<CAkEmitter>()) || midi->gameObjID == AK_INVALID_GAME_OBJECT)// Avoid performing if the GameObject was specified but does not exist.
			{
				g_pAudioMgr->ProcessAllActions(midi->pEvent, *midi, pGameObj);
			}
			else
			{
				if (!pGameObj)
				{
					MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_EventPostMIDI, AK_INVALID_PLAYING_ID, in_gameObjectID, pEvent->ID(), false);
				}
				else
				{
					MONITOR_ERROREX(AK::Monitor::ErrorCode_GameObjectIsNotEmitterEvent, AK_INVALID_PLAYING_ID, in_gameObjectID, pEvent->ID(), false);
				}
			}

			if (pGameObj)
			{
				pGameObj->Release();
			}

			midi->pEvent->Release();
		}

		return AK_Success;
	}

	/// Stops MIDI notes on all nodes that are referenced in the specified event in an action of type play,
	/// with the specified Game Object. Invalid parameters are interpreted as wildcards. For example, calling
	/// this function with in_eventID set to AK_INVALID_UNIQUE_ID will stop all MIDI notes for Game Object
	/// in_gameObjectID.
	/// \sa
	/// - <tt>AK::SoundEngine::PostMIDIOnEvent</tt>
	virtual AKRESULT StopMIDIOnEventSync(
		AkUniqueID in_eventID = AK_INVALID_UNIQUE_ID,					///< Unique ID of the Event
		AkGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT			///< Associated game object ID
		)
	{
		AKASSERT(g_pIndex);

		// This function call does get the pointer and addref it in an atomic call.
		CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef(in_eventID);
		if (pEvent == NULL && in_eventID != AK_INVALID_UNIQUE_ID)
		{
			MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_EventIDNotFound, in_eventID, AK_INVALID_PLAYING_ID, in_gameObjectID, in_eventID, false);
			return AK_Fail;
		}

		// Lock
		{
			CAkFunctionCritical GlobalLock;

			CAkRegisteredObj * pGameObj = g_pRegistryMgr->GetObjAndAddref(in_gameObjectID);
			if ((pGameObj && pGameObj->HasComponent<CAkEmitter>()) || in_gameObjectID == AK_INVALID_GAME_OBJECT)// Avoid performing if the GameObject was specified but does not exist.
			{
				AkQueuedMsg_EventStopMIDI stopmidi;
				stopmidi.pEvent = pEvent;
				stopmidi.eventID = in_eventID;
				stopmidi.gameObjID = in_gameObjectID;
				g_pAudioMgr->ProcessAllActions(stopmidi.pEvent, stopmidi, pGameObj);
			}
			else
			{
				if (!pGameObj)
				{
					MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_EventStopMIDI, AK_INVALID_PLAYING_ID, in_gameObjectID, pEvent ? pEvent->ID() : AK_INVALID_UNIQUE_ID, false);
				}
				else
				{
					MONITOR_ERROREX(AK::Monitor::ErrorCode_GameObjectIsNotEmitterEvent, AK_INVALID_PLAYING_ID, in_gameObjectID, pEvent ? pEvent->ID() : AK_INVALID_UNIQUE_ID, false);
				}

			}

			if (pGameObj)
				pGameObj->Release();

			if (pEvent)
				pEvent->Release();
		}

		return AK_Success;
	}

	virtual AkInt32 Random()
	{
		return AKRANDOM::AkRandom();
	}

	virtual AKRESULT PassSampleData(void* in_pData, AkUInt32 in_size, AkCaptureFile* in_pCaptureFile)
	{
		return in_pCaptureFile->PassSampleData(in_pData, in_size);
	}

	virtual AkCaptureFile* StartCapture(const AkOSChar* in_CaptureFileName,
		AkUInt32 in_uSampleRate,
		AkUInt32 in_uBitsPerSample,
		AkFileParser::FormatTag in_formatTag,
		AkChannelConfig in_channelConfig)
	{
		return AkCaptureMgr::Instance()->StartCapture(in_CaptureFileName, in_uSampleRate, in_uBitsPerSample, in_formatTag, in_channelConfig);
	}

	virtual void StopCapture(AkCaptureFile*& io_pCaptureFile)
	{
		io_pCaptureFile->StopCapture();
		io_pCaptureFile = NULL;
	}

	virtual bool IsOfflineRendering()
	{
		return AK_PERF_OFFLINE_RENDERING;
	}

	virtual AkTimeMs GetmsPerBufferTick()
	{
		return AkAudioLibSettings::g_msPerBufferTick;
	}

	virtual AkUInt32 GetPipelineCoreFrequency()
	{
		return AkAudioLibSettings::g_pipelineCoreFrequency;
	}

	virtual AkUInt32 GetNumOfSamplesPerFrame()
	{
		return AkAudioLibSettings::g_uNumSamplesPerFrame;
	}

	virtual void MixNinNChannels(
		AkAudioBuffer *	in_pInputBuffer,		///< Input multichannel buffer.
		AkAudioBuffer *	in_pMixBuffer,			///< Multichannel buffer with which the input buffer is mixed.
		AkReal32		in_fPrevGain,			///< Gain, corresponding to the beginning of the buffer, to apply uniformly to each mixed channel.
		AkReal32		in_fNextGain,			///< Gain, corresponding to the end of the buffer, to apply uniformly to each mixed channel.
		AK::SpeakerVolumes::ConstMatrixPtr in_mxPrevVolumes,	///< In/out channel volume distribution corresponding to the beginning of the buffer. (see AK::SpeakerVolumes::Matrix services).
		AK::SpeakerVolumes::ConstMatrixPtr in_mxNextVolumes	///< In/out channel volume distribution corresponding to the end of the buffer. (see AK::SpeakerVolumes::Matrix services).
		)
	{
		AkRamp gain(in_fPrevGain, in_fNextGain);
		AkUInt16 usMaxFrames = in_pMixBuffer->MaxFrames();
		AkMixer::MixNinNChannels(in_pInputBuffer, in_pMixBuffer, gain, in_mxPrevVolumes, in_mxNextVolumes, 1.f / (AkReal32)usMaxFrames, usMaxFrames);
	}

	virtual void Mix1inNChannels(
		AkReal32 * AK_RESTRICT in_pInChannel,	///< Input channel buffer.
		AkAudioBuffer *	in_pMixBuffer,			///< Multichannel buffer with which the input buffer is mixed.
		AkReal32		in_fPrevGain,			///< Gain, corresponding to the beginning of the input channel.
		AkReal32		in_fNextGain,			///< Gain, corresponding to the end of the input channel.
		AK::SpeakerVolumes::ConstVectorPtr in_vPrevVolumes,	///< Output channel volume distribution corresponding to the beginning of the buffer. (see AK::SpeakerVolumes::Vector services).
		AK::SpeakerVolumes::ConstVectorPtr in_vNextVolumes	///< Output channel volume distribution corresponding to the end of the buffer. (see AK::SpeakerVolumes::Vector services).
		)
	{
		AKASSERT(in_pMixBuffer->MaxFrames() > 0);
		AkRamp gain(in_fPrevGain, in_fNextGain);
		AkUInt16 usMaxFrames = in_pMixBuffer->MaxFrames();
		AkMixer::MixOneInNChannels(in_pInChannel, in_pMixBuffer, in_pMixBuffer->GetChannelConfig().RemoveLFE(), gain, in_vPrevVolumes, in_vNextVolumes, 1.f / (AkReal32)usMaxFrames, usMaxFrames);
	}

	virtual void MixChannel(
		AkReal32 * AK_RESTRICT in_pInBuffer,	///< Input channel buffer.
		AkReal32 * AK_RESTRICT in_pOutBuffer,	///< Output channel buffer.
		AkReal32		in_fPrevGain,			///< Gain, corresponding to the beginning of the input channel.
		AkReal32		in_fNextGain,			///< Gain, corresponding to the end of the input channel.
		AkUInt16		in_uNumFrames			///< Number of frames to mix.
		)
	{
		if (in_uNumFrames > 0)
		{
			AkMixer::MixChannelSIMD(in_pInBuffer, in_pOutBuffer, in_fPrevGain, (in_fNextGain - in_fPrevGain) / (AkReal32)in_uNumFrames, in_uNumFrames);
		}
	}

	/// Given non-interleaved audio in the provided in_pInputBuffer, will apply a ramping gain over the number
	/// of frames specified, and store the result in in_pOutputBuffer. Channel data from in_pInputBuffer will also be
	/// interleaved in in_pOutputBuffer's results, and optionally converted from 32-bit floats to 16-bit integers.
	virtual void ApplyGainAndInterleave(
		AkAudioBuffer* in_pInputBuffer,		///< Input audioBuffer data
		AkAudioBuffer* in_pOutputBuffer,	///< Output audioBuffer data
		AkRamp in_gain,						///< Ramping gain to apply over duration of buffer
		bool in_convertToInt16				///< Whether the input data should be converted to int16
	) const
	{
		AkMixer::ApplyGainAndInterleave(in_pInputBuffer, in_pOutputBuffer, in_gain, in_convertToInt16);
	}

	/// Given non-interleaved audio in the provided in_pInputBuffer, will apply a ramping gain over the number
	/// of frames specified, and store the result in in_pOutputBuffer. Audio data in in_pOutputBuffer will have
	/// the same layout as in_pInputBuffer, and optionally converted from 32-bit floats to 16-bit integers.
	virtual void ApplyGain(
		AkAudioBuffer* in_pInputBuffer,		///< Input audioBuffer data
		AkAudioBuffer* in_pOutputBuffer,	///< Output audioBuffer data
		AkRamp in_gain,						///< Ramping gain to apply over duration of buffer
		bool in_convertToInt16				///< Whether the input data should be converted to int16
	) const
	{
		AkMixer::ApplyGain(in_pInputBuffer, in_pOutputBuffer, in_gain, in_convertToInt16);
	}

	/// Computes gain vector for encoding a source with angles in_fAzimuth and in_fElevation to full-sphere ambisonics with order in_uOrder.
	/// Ambisonic channels are ordered by ACN and use the SN3D convention.
	virtual void ComputeAmbisonicsEncoding(
		AkReal32			in_fAzimuth,				///< Incident angle, in radians [-pi,pi], where 0 is the front (positive values are clockwise).
		AkReal32			in_fElevation,				///< Incident angle, in radians [-pi/2,pi/2], where 0 is the azimuthal plane.
		AkChannelConfig		in_cfgAmbisonics,			///< Ambisonic configuration. Supported configurations are 1-1, 2-2 and 3-3. Determines size of vector out_vVolumes.
		AK::SpeakerVolumes::VectorPtr out_vVolumes		///< Returned volumes (see AK::SpeakerVolumes::Vector services). Must be allocated prior to calling this function with the size returned by AK::SpeakerVolumes::Vector::GetRequiredSize() for the desired number of channels.
		)
	{
		AKASSERT(in_cfgAmbisonics.eConfigType == AK_ChannelConfigType_Ambisonic);
		CAkSpeakerPan::EncodeToAmbisonics(in_fAzimuth, in_fElevation, out_vVolumes, in_cfgAmbisonics.uNumChannels);
	}

	/// Computes gain matrix for decoding an SN3D-normalized ACN-ordered ambisonic signal of order sqrt(in_cfgAmbisonics.uNumChannels)-1, with max-RE weighting, on a (regularly) sampled sphere whose samples in_samples are
	/// expressed in left-handed cartesian coordinates, with unitary norm.
	/// This decoding technique is optimal for regular sampling.
	/// The returned matrix has in_cfgAmbisonics.uNumChannels inputs (rows) and in_uNumSamples outputs (columns), and is normalized by the number of samples.
	/// Supported ambisonic configurations are full sphere 1st to 5th order.
	/// \return AK_Fail when ambisonic configuration is not supported. AK_Success otherwise.
	virtual AKRESULT ComputeWeightedAmbisonicsDecodingFromSampledSphere(
		const AkVector		in_samples[],				///< Array of vector samples expressed in left-handed cartesian coordinates, where (1,0,0) points towards the front and (0,1,0) points towards the top. Vectors must be normalized.
		AkUInt32			in_uNumSamples,				///< Number of points in in_samples.
		AkChannelConfig		in_cfgAmbisonics,			///< Ambisonic configuration. Supported configurations are full sphere 1st to 5th order. Determines number of rows (input channels) in matrix out_mxVolume.
		AK::SpeakerVolumes::MatrixPtr out_mxVolume		///< Returned volume matrix of in_cfgAmbisonics.uNumChannels rows x in_uNumSamples colums. Must be allocated prior to calling this function with the size returned by AK::SpeakerVolumes::Matrix::GetRequiredSize() for the desired number of channels.
		)
	{
		// Check inputs.
		if (in_cfgAmbisonics.eConfigType != AK_ChannelConfigType_Ambisonic)
		{
			AKASSERT(!"Channel configuration not ambisonics");
			return AK_Fail;
		}
		bool bNumHarmonicsValid = false;
		int orderidx = 0; 
		while (!bNumHarmonicsValid && orderidx < 5)
		{
			if (in_cfgAmbisonics.uNumChannels == CAkSpeakerPan::OrderMinusOneToNumHarmonics(orderidx))
				bNumHarmonicsValid = true;
			else
				orderidx++;
		}
		if (!bNumHarmonicsValid)
		{
			AKASSERT(!"Invalid number of channels/harmonics");
			return AK_Fail;
		}

		// Convert and verify sample points. ComputeDecoderMatrix expects AkReal32[4] points.
		AkReal32 * points = (AkReal32*)AkAllocaSIMD(4 * sizeof(AkReal32) * in_uNumSamples);
		AkReal32 * pPoint = points;
		for (AkUInt32 uSample = 0; uSample < in_uNumSamples; uSample++)
		{
			if (!AkMath::IsVectorNormalized(in_samples[uSample]))
				return AK_Fail;
			*pPoint++ = in_samples[uSample].X;
			*pPoint++ = in_samples[uSample].Y;
			*pPoint++ = in_samples[uSample].Z;
			*pPoint++ = 0.f;
		}

		AkReal32 * decoder = (AkReal32 *)AkAllocaSIMD(in_cfgAmbisonics.uNumChannels * in_uNumSamples * sizeof(AkReal32));
		CAkSpeakerPan::ComputeDecoderMatrix(orderidx, points, in_uNumSamples, decoder);
		const AkReal32* weights = CAkSpeakerPan::GetReWeights(orderidx);

		// Copy (and scale) into volume matrix.
		const AkReal32 fFourPiOverL = 4.f * PI / (AkReal32)in_uNumSamples;
		for (AkUInt32 uAmbChan = 0; uAmbChan < in_cfgAmbisonics.uNumChannels; uAmbChan++)
		{
			AK::SpeakerVolumes::VectorPtr harmonic = AK::SpeakerVolumes::Matrix::GetChannel(out_mxVolume, uAmbChan, in_uNumSamples);
			AK::SpeakerVolumes::Vector::Copy(harmonic, decoder + uAmbChan * in_uNumSamples, in_uNumSamples, fFourPiOverL * weights[uAmbChan]);
		}

		return AK_Success;
	}

	virtual const AkAcousticTexture* GetAcousticTexture(
		AkAcousticTextureID in_AcousticTextureID		///< Acoustic texture's short it
		)
	{
		return g_pVirtualAcousticsMgr->GetAcousticTexture(in_AcousticTextureID);
	}

	virtual AKRESULT ComputeSphericalCoordinates(
		const AkEmitterListenerPair & in_pair,
		AkReal32 & out_fAzimuth,
		AkReal32 & out_fElevation
		) const
	{
		const CAkListener * pListener = CAkListener::GetListenerData(in_pair.ListenerID());
		if (pListener)
		{
			pListener->ComputeSphericalCoordinates(in_pair, out_fAzimuth, out_fElevation);
			return AK_Success;
		}
		return AK_Fail;
	}

	virtual void ComputeSpeakerVolumesDirect(
		AkChannelConfig		in_inputConfig,				///< Channel configuration of the input.
		AkChannelConfig		in_outputConfig,			///< Channel configuration of the mixer output.
		AkReal32			in_fCenterPerc,				///< Center percentage. Only applies to mono inputs with standard output configurations that have a center channel.
		AK::SpeakerVolumes::MatrixPtr out_mxVolumes		///< Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services).
		)
	{
		// For direct speaker assignment, it does not matter whether clients use side or back channels. However, our internal downmix algorithms works
		// with side channels. Fix it now.
		if ( in_inputConfig.eConfigType == AK_ChannelConfigType_Standard )
			in_inputConfig.uChannelMask = AK::BackToSideChannels( in_inputConfig.uChannelMask );
		if ( in_outputConfig.eConfigType == AK_ChannelConfigType_Standard )
			in_outputConfig.uChannelMask = AK::BackToSideChannels( in_outputConfig.uChannelMask );

		CAkSpeakerPan::GetSpeakerVolumesDirect( in_inputConfig, in_outputConfig, in_fCenterPerc, out_mxVolumes );
	}

	virtual void SetRTPCValueSync(
		AkRtpcID in_RTPCid,
		AkReal32 in_fValue,
		AkGameObjectID in_gameObjectID,
		AkTimeMs in_uValueChangeDuration = 0,
		AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,
		bool in_bBypassInternalValueInterpolation = false
		)
	{
		CAkRegisteredObj* pRegGameObj =
			( in_gameObjectID != AK_INVALID_GAME_OBJECT) ?
			g_pRegistryMgr->GetObjAndAddref( in_gameObjectID ) :
			NULL;

		TransParams transParams;
		transParams.TransitionTime = in_uValueChangeDuration;
		transParams.eFadeCurve = in_eFadeCurve;
		transParams.bBypassInternalValueInterpolation = in_bBypassInternalValueInterpolation;
		g_pRTPCMgr->SetRTPCInternal( in_RTPCid, in_fValue, pRegGameObj, transParams );
		if ( pRegGameObj )
			pRegGameObj->Release();
	}

	virtual AK::IAkPlatformContext * GetPlatformContext() const
	{
		return CAkLEngine::GetPlatformContext();
	}
};

AK::IAkGlobalPluginContext * GetGlobalPluginContext()
{
	// Pursue Wwise tradition of being static.
	static CAkGlobalPluginContext theGlobalPluginContext;
	return &theGlobalPluginContext;
}

AKRESULT RegisterPlugin(
	AkPluginType in_eType,
	AkUInt32 in_ulCompanyID,						// Company identifier (as declared in plugin description XML)
	AkUInt32 in_ulPluginID,							// Plugin identifier (as declared in plugin description XML)
    AkCreatePluginCallback	in_pCreateFunc,			// Pointer to the effect's Create() function.
    AkCreateParamCallback	in_pCreateParamFunc,	// Pointer to the effect param's Create() function.
	AkGetDeviceListCallback in_pGetDeviceListFunc   // Pointer to the effect GetDeviceList() function.
    )
{
	return CAkEffectsMgr::RegisterPlugin( in_eType, in_ulCompanyID, in_ulPluginID, in_pCreateFunc, in_pCreateParamFunc);
}

AKRESULT RegisterCodec(
	AkUInt32 in_ulCompanyID,						// Company identifier (as declared in plugin description XML)
	AkUInt32 in_ulPluginID,							// Plugin identifier (as declared in plugin description XML)
	AkCreateFileSourceCallback in_pFileCreateFunc,	// File source creation function
	AkCreateBankSourceCallback in_pBankCreateFunc	// Bank source creation function
    )
{
	return CAkEffectsMgr::RegisterCodec(in_ulCompanyID, in_ulPluginID, { in_pFileCreateFunc, in_pBankCreateFunc, nullptr, nullptr });
}

///////////////////////////////////////////////////////////////////////////
// RTPC
///////////////////////////////////////////////////////////////////////////
AKRESULT SetPosition(
	AkGameObjectID in_GameObj,
	const AkSoundPosition & in_Position
	)
{
	if (in_GameObj >= AkGameObjectID_ReservedStart) // omni
		return AK_Fail;

	return SetPositionInternal( in_GameObj, in_Position );
}

AKRESULT SetPositionInternal(
	AkGameObjectID in_GameObj,
	const AkSoundPosition & in_Position
	)
{
	AKASSERT(g_pAudioMgr);
	if (AkMath::IsTransformValid(in_Position))
	{
		AutoReserveMsg pItem(QueuedMsgType_GameObjPosition, AkQueuedMsg::Sizeof_GameObjPosition());

		pItem->gameobjpos.gameObjID = in_GameObj;
		pItem->gameobjpos.Position = in_Position;
		return AK_Success;
	}
	MONITOR_ERRORMSG(AKTEXT("AK::SoundEngine::SetPosition : Invalid transform"));
	return AK_InvalidParameter;
}

//Maximum position coordinate that can be passed.  This is simply sqrt(FLX_MAX/3) which will give FLT_MAX when used in distance calculation.
#define AK_MAX_DISTANCE 1.065023233e+19F

static AKRESULT _SetMultiplePositions(
	AkGameObjectID in_GameObjectID,
	const AkChannelEmitter* in_pPositions,
	AkUInt16 in_NumPositions,
	MultiPositionType in_eMultiPositionType /*= MultiPositionType_MultiDirection*/
    )
{
	AKASSERT(g_pAudioMgr);

	// Here, we need to create the complete variable size object on the stack.
	// And then use the placement new for the object.
	// We have to copy the array over since the enqueue function can only do one write and this write must be atomic.
	AkUInt32 uAllocSize = AkQueuedMsg::Sizeof_GameObjMultiPositionBase() + in_NumPositions * sizeof(AkChannelEmitter);
	if (uAllocSize > g_pAudioMgr->GetMaximumMsgSize())
	{
		MONITOR_ERRORMSG(AKTEXT("AK::SoundEngine::SetMultiplePositions: too many positions."));
		return AK_InvalidParameter;
	}

	AkQueuedMsg *pItem = g_pAudioMgr->ReserveQueue(QueuedMsgType_GameObjMultiPosition, uAllocSize);
	if (pItem == NULL)
		return AK_InvalidParameter;	//Too big, could not fit.	This is the only message where this could happen.

	AKRESULT eResult = AK_Success;
	pItem->gameObjMultiPos.eMultiPositionType = in_eMultiPositionType;
	pItem->gameObjMultiPos.gameObjID = in_GameObjectID;
	pItem->gameObjMultiPos.uNumPositions = in_NumPositions;
	for( AkUInt16 i = 0; i < in_NumPositions; ++i )
	{
		if (AkMath::IsTransformValid(in_pPositions[i].position))
		{
			pItem->gameObjMultiPos.aMultiPosition[i].position = in_pPositions[i].position;
			pItem->gameObjMultiPos.aMultiPosition[i].uInputChannels = in_pPositions[i].uInputChannels;
		}
		else
		{
			pItem->type = QueuedMsgType_Invalid;
			MONITOR_ERRORMSG(AKTEXT("AK::SoundEngine::SetMultiplePositions : Invalid transform"));
			eResult = AK_InvalidParameter;
			break;
		}
	}

	g_pAudioMgr->FinishQueueWrite();

	return eResult;
}

AKRESULT SetMultiplePositions(
	AkGameObjectID in_GameObjectID,
	const AkSoundPosition * in_pPositions,
	AkUInt16 in_NumPositions,
	MultiPositionType in_eMultiPositionType /*= MultiPositionType_MultiDirection*/
	)
{
	if (in_eMultiPositionType > MultiPositionType_MultiDirections ||
		(in_NumPositions > 0 && in_pPositions == NULL))
		return AK_InvalidParameter;

	AkChannelEmitter * pPositions = (AkChannelEmitter*)AkAlloca(in_NumPositions*sizeof(AkChannelEmitter));
	for (AkUInt16 i = 0; i < in_NumPositions; ++i)
	{
		pPositions[i].position = in_pPositions[i];
		pPositions[i].uInputChannels = AK_SPEAKER_SETUP_ALL_SPEAKERS;
	}
	return _SetMultiplePositions(in_GameObjectID, pPositions, in_NumPositions, in_eMultiPositionType);
}

AKRESULT SetMultiplePositions(
	AkGameObjectID in_GameObjectID,
	const AkChannelEmitter * in_pPositions,
	AkUInt16 in_NumPositions,
	MultiPositionType in_eMultiPositionType /*= MultiPositionType_MultiDirection*/
	)
{
	if (in_eMultiPositionType > MultiPositionType_MultiDirections ||
		(in_NumPositions > 0 && in_pPositions == NULL))
		return AK_InvalidParameter;

	return _SetMultiplePositions(in_GameObjectID, in_pPositions, in_NumPositions, in_eMultiPositionType);
}

AKRESULT SetScalingFactor(
	AkGameObjectID in_GameObj,
	AkReal32 in_fAttenuationScalingFactor
	)
{
	if( in_fAttenuationScalingFactor <= 0.0f )// negative and zero are not valid scaling factor.
	{
		return AK_InvalidParameter;
	}
	AKASSERT( g_pAudioMgr );
#ifdef CHECK_SOUND_ENGINE_INPUT_VALID
	if ( !(AkMath::IsValidFloatInput( in_fAttenuationScalingFactor ) ) )
		HANDLE_INVALID_FLOAT_INPUT( AKTEXT("AK::SoundEngine::SetAttenuationScalingFactor : Invalid Float in in_fAttenuationScalingFactor") );
#endif

	AutoReserveMsg pItem(QueuedMsgType_GameObjScalingFactor, AkQueuedMsg::Sizeof_GameObjScalingFactor() );
	pItem->gameobjscalingfactor.gameObjID = in_GameObj;
	pItem->gameobjscalingfactor.fValue = in_fAttenuationScalingFactor;


	return AK_Success;
}

AKRESULT AddRemoveOrSetListeners(
	AkGameObjectID in_EmitterID,
	const AkGameObjectID* in_pListenerObjs,
	AkUInt32 in_uNumListeners,
	AkListenerOp in_operation
)
{
	AKASSERT(g_pAudioMgr);

	AkUInt32 uAllocSize = AkQueuedMsg::Sizeof_ListenerIDMsg<AkQueuedMsg_GameObjActiveListeners>(in_uNumListeners);

	if (uAllocSize > g_pAudioMgr->GetMaximumMsgSize())
	{
		MONITOR_ERRORMSG(AKTEXT("AK::SoundEngined::SetListeners() - Too many game objects in array."));
		return AK_InvalidParameter;
	}

	AkQueuedMsg *pItem = g_pAudioMgr->ReserveQueue(QueuedMsgType_GameObjActiveListeners, uAllocSize);
	if (pItem == NULL)
		return AK_InvalidParameter;	//Too big, could not fit.	

	pItem->gameobjactlist.gameObjID = in_EmitterID;
	pItem->gameobjactlist.SetListeners(in_pListenerObjs, in_uNumListeners, in_operation);

	g_pAudioMgr->FinishQueueWrite();

	return AK_Success;
}

AKRESULT SetListeners(
	AkGameObjectID in_EmitterID,
	const AkGameObjectID* in_pListenerObjs,
	AkUInt32 in_uNumListeners
	)
{
	return AddRemoveOrSetListeners(in_EmitterID, in_pListenerObjs, in_uNumListeners, AkListenerOp_Set);
}

AKRESULT AddListener(
	AkGameObjectID in_EmitterID,
	AkGameObjectID in_ListenerID
)
{
	return AddRemoveOrSetListeners(in_EmitterID, &in_ListenerID, 1, AkListenerOp_Add);
}

AKRESULT RemoveListener(
	AkGameObjectID in_EmitterID,
	AkGameObjectID in_ListenerID
	)
{
	return AddRemoveOrSetListeners(in_EmitterID, &in_ListenerID, 1, AkListenerOp_Remove);
}

AKRESULT AddRemoveOrSetDefaultListeners(
	const AkGameObjectID* in_pListenerObjs,
	AkUInt32 in_uNumListeners,
	AkListenerOp in_operation
	)
{
	AKASSERT(g_pAudioMgr);

	AkUInt32 uAllocSize = AkQueuedMsg::Sizeof_ListenerIDMsg<AkQueuedMsg_DefaultActiveListeners>(in_uNumListeners);

	if (uAllocSize > g_pAudioMgr->GetMaximumMsgSize())
	{
		MONITOR_ERRORMSG(AKTEXT("AK::SoundEngined::SetDefaultListeners() - Too many game objects in array."));
		return AK_InvalidParameter;
	}

	AkQueuedMsg *pItem = g_pAudioMgr->ReserveQueue(QueuedMsgType_DefaultActiveListeners, uAllocSize);
	if (pItem == NULL)
		return AK_InvalidParameter;	//Too big, could not fit.	

	pItem->defaultactlist.SetListeners(in_pListenerObjs, in_uNumListeners, in_operation);

	g_pAudioMgr->FinishQueueWrite();

	return AK_Success;
}


AKRESULT SetDefaultListeners(
	const AkGameObjectID* in_pListenerObjs,
	AkUInt32 in_uNumListeners
)
{
	return AddRemoveOrSetDefaultListeners( in_pListenerObjs, in_uNumListeners, AkListenerOp_Set);
}

AKRESULT AddDefaultListener( AkGameObjectID in_ListenerID )
{
	return AddRemoveOrSetDefaultListeners( &in_ListenerID, 1, AkListenerOp_Add);
}

AKRESULT RemoveDefaultListener(	AkGameObjectID in_ListenerID )
{
	return AddRemoveOrSetDefaultListeners(&in_ListenerID, 1, AkListenerOp_Remove);
}

AKRESULT ResetListenersToDefault(
	AkGameObjectID in_EmitterID
	)
{
	AKASSERT(g_pAudioMgr);

	AutoReserveMsg pItem(QueuedMsgType_ResetListeners, AkQueuedMsg::Sizeof_ResetListeners());
	pItem->resetListeners.gameObjID = in_EmitterID;

	return AK_Success;
}

AKRESULT SetListenerSpatialization(
	AkGameObjectID in_ulIndex,						///< Listener index. 
	bool in_bSpatialized,
	AkChannelConfig in_channelConfig,
	AK::SpeakerVolumes::VectorPtr in_pVolumeOffsets
	)
{
	AKASSERT(g_pAudioMgr);

#ifdef CHECK_SOUND_ENGINE_INPUT_VALID
	if (in_pVolumeOffsets)
	{
		if (!(IsValidFloatVolumes(in_pVolumeOffsets, in_channelConfig.uNumChannels)))
			HANDLE_INVALID_FLOAT_INPUT(AKTEXT("AK::SoundEngine::SetListenerSpatialization : Invalid Float in in_pVolumeOffsets"));
	}
#endif

	AutoReserveMsg pItem( QueuedMsgType_ListenerSpatialization, AkQueuedMsg::Sizeof_ListenerSpatialization() );

	pItem->listspat.listener.gameObjID = in_ulIndex;
	pItem->listspat.bSpatialized = in_bSpatialized;
	pItem->listspat.uChannelConfig = in_channelConfig.Serialize();
	pItem->listspat.pVolumes = NULL;

	if ( in_pVolumeOffsets )
	{
		pItem->listspat.bSetVolumes = true;
		if ( in_channelConfig.uNumChannels )
		{
			pItem->listspat.pVolumes = AK::SpeakerVolumes::Vector::Allocate( in_channelConfig.uNumChannels, AkMemID_Object );
			if ( !pItem->listspat.pVolumes )
			{
				pItem->type = QueuedMsgType_Invalid;
				return AK_InsufficientMemory;
			}
			AK::SpeakerVolumes::Vector::Copy( pItem->listspat.pVolumes, in_pVolumeOffsets, in_channelConfig.uNumChannels );
		}
	}
	else
	{
		pItem->listspat.bSetVolumes = false;
	}


	return AK_Success;
}


AKRESULT _SetRTPCValue(
		    AkRtpcID in_rtpcID,
		    AkReal32 in_value,
		    AkGameObjectID in_gameObjectID /*= AK_INVALID_GAME_OBJECT*/,
			AkPlayingID in_playingID /*= AK_INVALID_PLAYING_ID*/,
			AkTimeMs in_uValueChangeDuration /*= 0*/,
			AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/,
			bool in_bBypassInternalValueInterpolation /*= false*/
		    )
{
	AKASSERT(g_pAudioMgr);
#ifdef CHECK_SOUND_ENGINE_INPUT_VALID
	if ( !(AkMath::IsValidFloatInput( in_value ) ) || in_value == FLT_MAX)	//WG-44348 FLT_MAX isn't supported, all other values, including -FLT_MAX are ok. 
		HANDLE_INVALID_FLOAT_INPUT( AKTEXT("AK::SoundEngine::SetRTPCValue : Invalid Float in in_Value") );
	if ( in_value == FLT_MAX )
		HANDLE_INVALID_FLOAT_INPUT(AKTEXT("AK::SoundEngine::SetRTPCValue : FLT_MAX not supported"));
#endif

	if ( in_uValueChangeDuration == 0 && in_bBypassInternalValueInterpolation == false)
	{
		AutoReserveMsg pItem( QueuedMsgType_RTPC, AkQueuedMsg::Sizeof_Rtpc() );

		pItem->rtpc.ID = in_rtpcID;
		pItem->rtpc.Value = in_value;
		pItem->rtpc.gameObjID = in_gameObjectID;
		pItem->rtpc.PlayingID = in_playingID;
	}
	else
	{
		AutoReserveMsg pItem( QueuedMsgType_RTPCWithTransition, AkQueuedMsg::Sizeof_RtpcWithTransition() );
		pItem->rtpcWithTransition.ID = in_rtpcID;
		pItem->rtpcWithTransition.Value = in_value;
		pItem->rtpcWithTransition.gameObjID = in_gameObjectID;
		pItem->rtpcWithTransition.PlayingID = in_playingID;
		pItem->rtpcWithTransition.transParams.bBypassInternalValueInterpolation = in_bBypassInternalValueInterpolation;
		pItem->rtpcWithTransition.transParams.TransitionTime = in_uValueChangeDuration;
		pItem->rtpcWithTransition.transParams.eFadeCurve = in_eFadeCurve;
	}


	return AK_Success;
}


AKRESULT SetRTPCValue(
					  AkRtpcID in_rtpcID,
					  AkReal32 in_value,
					  AkGameObjectID in_gameObjectID /*= AK_INVALID_GAME_OBJECT*/,
					  AkTimeMs in_uValueChangeDuration /*= 0*/,
					  AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/,
					  bool in_bBypassInternalValueInterpolation /*= false*/
					  )
{
	return _SetRTPCValue(
		in_rtpcID,
		in_value,
		in_gameObjectID /*= AK_INVALID_GAME_OBJECT*/,
		AK_INVALID_PLAYING_ID,
		in_uValueChangeDuration /*= 0*/,
		in_eFadeCurve /*= AkCurveInterpolation_Linear*/,
		in_bBypassInternalValueInterpolation /*= false*/
		);
}

AKRESULT SetRTPCValueByPlayingID(
								 AkRtpcID in_rtpcID,
								 AkReal32 in_value,
								 AkPlayingID in_playingID,
								 AkTimeMs in_uValueChangeDuration /*= 0*/,
								 AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/,
								 bool in_bBypassInternalValueInterpolation /*= false*/
								 )
{
	AkGameObjectID gameObjectID = g_pPlayingMgr->GetGameObjectFromPlayingID(in_playingID);
	if (gameObjectID != AK_INVALID_GAME_OBJECT)
	{
		return _SetRTPCValue( in_rtpcID, in_value, gameObjectID, in_playingID,	in_uValueChangeDuration, in_eFadeCurve, in_bBypassInternalValueInterpolation );
	}
	else
	{
		MONITOR_ERRORMSG( AKTEXT("AK::SoundEngine::SetRTPCValueByPlayingID : Playing ID not found. ") );
		return AK_PlayingIDNotFound;
	}
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT SetRTPCValueByPlayingID(
								 const wchar_t* in_pszRtpcName,
								 AkReal32 in_value,
								 AkPlayingID in_playingID,
								 AkTimeMs in_uValueChangeDuration /*= 0*/,
								 AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/,
								 bool in_bBypassInternalValueInterpolation /*= false*/
								 )
{
	AkRtpcID id = GetIDFromString( in_pszRtpcName );
	if ( id == AK_INVALID_RTPC_ID )
		return AK_IDNotFound;

	return SetRTPCValueByPlayingID(
		id,
		in_value,
		in_playingID,
		in_uValueChangeDuration,
		in_eFadeCurve,
		in_bBypassInternalValueInterpolation
	);
}
#endif

AKRESULT SetRTPCValueByPlayingID(
								 const char* in_pszRtpcName,
								 AkReal32 in_value,
								 AkPlayingID in_playingID,
								 AkTimeMs in_uValueChangeDuration /*= 0*/,
								 AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/,
								 bool in_bBypassInternalValueInterpolation /*= false*/
								 )
{
	AkRtpcID id = GetIDFromString( in_pszRtpcName );
	if ( id == AK_INVALID_RTPC_ID )
		return AK_IDNotFound;

	return SetRTPCValueByPlayingID(
		id,
		in_value,
		in_playingID,
		in_uValueChangeDuration,
		in_eFadeCurve,
		in_bBypassInternalValueInterpolation
	);
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT SetRTPCValue(
		        const wchar_t* in_pszRtpcName,
		        AkReal32 in_value,
		        AkGameObjectID in_gameObjectID /*= AK_INVALID_GAME_OBJECT */,
				AkTimeMs in_uValueChangeDuration /*= 0*/,
				AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/,
				bool in_bBypassInternalValueInterpolation /*= false*/
		        )
{
	AkRtpcID id = GetIDFromString( in_pszRtpcName );
	if ( id == AK_INVALID_RTPC_ID )
		return AK_IDNotFound;

	return _SetRTPCValue( id, in_value, in_gameObjectID, AK_INVALID_PLAYING_ID, in_uValueChangeDuration, in_eFadeCurve, in_bBypassInternalValueInterpolation );
}
#endif //AK_SUPPORT_WCHAR

AKRESULT SetRTPCValue(
		        const char* in_pszRtpcName,
		        AkReal32 in_value,
		        AkGameObjectID in_gameObjectID /*= AK_INVALID_GAME_OBJECT */,
				AkTimeMs in_uValueChangeDuration /*= 0*/,
				AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/,
				bool in_bBypassInternalValueInterpolation /*= false*/
		        )
{
	AkRtpcID id = GetIDFromString( in_pszRtpcName );
	if ( id == AK_INVALID_RTPC_ID )
		return AK_IDNotFound;

	return _SetRTPCValue( id, in_value, in_gameObjectID, AK_INVALID_PLAYING_ID, in_uValueChangeDuration, in_eFadeCurve, in_bBypassInternalValueInterpolation );
}

AKRESULT SetSwitch(
		    AkSwitchGroupID in_SwitchGroup,
		    AkSwitchStateID in_SwitchState,
		    AkGameObjectID in_GameObj /*= AK_INVALID_GAME_OBJECT*/
		    )
{
	AKASSERT(g_pAudioMgr);

	AutoReserveMsg pItem( QueuedMsgType_Switch, AkQueuedMsg::Sizeof_Switch() );

	pItem->setswitch.gameObjID = in_GameObj;
	pItem->setswitch.SwitchGroupID = in_SwitchGroup;
	pItem->setswitch.SwitchStateID = in_SwitchState;

	return AK_Success;
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT SetSwitch(
		                const wchar_t* in_pszSwitchGroup,
		                const wchar_t* in_pszSwitchState,
		                AkGameObjectID in_GameObj /*= AK_INVALID_GAME_OBJECT*/
		                )
{
	AkSwitchGroupID l_SwitchGroup = GetIDFromString( in_pszSwitchGroup );
	AkSwitchStateID l_SwitchState = GetIDFromString( in_pszSwitchState );

	if( l_SwitchGroup != AK_INVALID_RTPC_ID && l_SwitchState != AK_INVALID_RTPC_ID )
	{
		return SetSwitch( l_SwitchGroup, l_SwitchState, in_GameObj );
	}

	return AK_IDNotFound;
}
#endif //AK_SUPPORT_WCHAR

AKRESULT SetSwitch(
		                const char* in_pszSwitchGroup,
		                const char* in_pszSwitchState,
		                AkGameObjectID in_GameObj /*= AK_INVALID_GAME_OBJECT*/
		                )
{
	AkSwitchGroupID l_SwitchGroup = GetIDFromString( in_pszSwitchGroup );
	AkSwitchStateID l_SwitchState = GetIDFromString( in_pszSwitchState );

	if( l_SwitchGroup != AK_INVALID_RTPC_ID && l_SwitchState != AK_INVALID_RTPC_ID )
	{
		return SetSwitch( l_SwitchGroup, l_SwitchState, in_GameObj );
	}
	else
	{
		return AK_IDNotFound;
	}
}

AKRESULT PostTrigger(
			AkTriggerID in_Trigger,
		    AkGameObjectID in_GameObj /*= AK_INVALID_GAME_OBJECT*/
		    )
{
	AKASSERT(g_pAudioMgr);

	AutoReserveMsg pItem( QueuedMsgType_Trigger, AkQueuedMsg::Sizeof_Trigger() );
	pItem->trigger.gameObjID = in_GameObj;
	pItem->trigger.TriggerID = in_Trigger;


	return AK_Success;
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT PostTrigger(
			const wchar_t* in_pszTrigger,
			AkGameObjectID in_GameObj /*= AK_INVALID_GAME_OBJECT*/
		                )
{
	AkTriggerID l_TriggerID = GetIDFromString( in_pszTrigger );

	if( l_TriggerID != AK_INVALID_UNIQUE_ID  )
	{
		return PostTrigger( l_TriggerID, in_GameObj );
	}
	else
	{
		return AK_IDNotFound;
	}
}
#endif //AK_SUPPORT_WCHAR

AKRESULT PostTrigger(
			const char* in_pszTrigger,
			AkGameObjectID in_GameObj /*= AK_INVALID_GAME_OBJECT*/
		                )
{
	AkTriggerID l_TriggerID = GetIDFromString( in_pszTrigger );

	if( l_TriggerID != AK_INVALID_UNIQUE_ID  )
	{
		return PostTrigger( l_TriggerID, in_GameObj );
	}
	else
	{
		return AK_IDNotFound;
	}
}

AKRESULT SetState( AkStateGroupID in_StateGroup, AkStateID in_State )
{
	return SetState( in_StateGroup, in_State, false, false );
}

AKRESULT SetState( AkStateGroupID in_StateGroup, AkStateID in_State, bool in_bSkipTransitionTime, bool in_bSkipExtension )
{
	AKASSERT(g_pAudioMgr);

	AutoReserveMsg pItem (QueuedMsgType_State, AkQueuedMsg::Sizeof_State() );

	pItem->setstate.StateGroupID = in_StateGroup;
	pItem->setstate.StateID = in_State;
	pItem->setstate.bSkipTransition = in_bSkipTransitionTime;
    pItem->setstate.bSkipExtension = in_bSkipExtension;

	return AK_Success;
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT SetState( const wchar_t* in_pszStateGroup, const wchar_t* in_pszState )
{
	AkStateGroupID	l_StateGroup	= GetIDFromString( in_pszStateGroup );
	AkStateID		l_State			= GetIDFromString( in_pszState );

	if( l_StateGroup != AK_INVALID_UNIQUE_ID && l_State != AK_INVALID_UNIQUE_ID )
	{
		return SetState( l_StateGroup, l_State );
	}
	else
	{
		return AK_IDNotFound;
	}
}
#endif //AK_SUPPORT_WCHAR

AKRESULT SetState( const char* in_pszStateGroup, const char* in_pszState )
{
	AkStateGroupID	l_StateGroup	= GetIDFromString( in_pszStateGroup );
	AkStateID		l_State			= GetIDFromString( in_pszState );

	if( l_StateGroup != AK_INVALID_UNIQUE_ID && l_State != AK_INVALID_UNIQUE_ID )
	{
		return SetState( l_StateGroup, l_State );
	}
	else
	{
		return AK_IDNotFound;
	}
}

#ifndef AK_OPTIMIZED
AKRESULT ResetSwitches( AkGameObjectID in_GameObjID )
{
	AKASSERT( g_pAudioMgr );

	AutoReserveMsg pItem( QueuedMsgType_ResetSwitches, AkQueuedMsg::Sizeof_ResetSwitches() );
	pItem->resetswitches.gameObjID = in_GameObjID;


	return AK_Success;
}

AKRESULT ResetRTPC( AkGameObjectID in_GameObjID, AkPlayingID in_playingID )
{
	AKASSERT( g_pAudioMgr );

	AutoReserveMsg pItem( QueuedMsgType_ResetRTPC,  AkQueuedMsg::Sizeof_ResetRTPC() );
	pItem->resetrtpc.gameObjID = in_GameObjID;
	pItem->resetrtpc.PlayingID = in_playingID;


	return AK_Success;
}
#endif

AKRESULT ResetRTPCValue(AkUInt32 in_rtpcID,
				AkGameObjectID in_gameObjectID,
				AkTimeMs in_uValueChangeDuration /*= 0*/,
				AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/,
				bool in_bBypassInternalValueInterpolation /*= false*/
				)
{
	AKASSERT( g_pAudioMgr );

	if ( in_uValueChangeDuration == 0 && in_bBypassInternalValueInterpolation == false )
	{
		AutoReserveMsg pItem( QueuedMsgType_ResetRTPCValue, AkQueuedMsg::Sizeof_ResetRTPCValue() );
		pItem->resetrtpcvalue.gameObjID = in_gameObjectID;
		pItem->resetrtpcvalue.PlayingID = AK_INVALID_PLAYING_ID;
		pItem->resetrtpcvalue.ParamID = in_rtpcID;
	}
	else
	{
		AutoReserveMsg pItem( QueuedMsgType_ResetRTPCValueWithTransition, AkQueuedMsg::Sizeof_ResetRTPCValueWithTransition() );
		pItem->resetrtpcvalueWithTransition.gameObjID = in_gameObjectID;
		pItem->resetrtpcvalueWithTransition.PlayingID = AK_INVALID_PLAYING_ID;
		pItem->resetrtpcvalueWithTransition.ParamID = in_rtpcID;
		pItem->resetrtpcvalueWithTransition.transParams.TransitionTime = in_uValueChangeDuration;
		pItem->resetrtpcvalueWithTransition.transParams.eFadeCurve = in_eFadeCurve;
		pItem->resetrtpcvalueWithTransition.transParams.bBypassInternalValueInterpolation = in_bBypassInternalValueInterpolation;
	}


	return AK_Success;
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT ResetRTPCValue(const wchar_t* in_pszRtpcName,
				AkGameObjectID in_gameObjectID,
				AkTimeMs in_uValueChangeDuration /*= 0*/,
				AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/,
				bool in_bBypassInternalValueInterpolation /*= false*/
				)
{
	AkRtpcID id = GetIDFromString( in_pszRtpcName );
	if ( id == AK_INVALID_RTPC_ID )
		return AK_IDNotFound;

	return ResetRTPCValue(id, in_gameObjectID, in_uValueChangeDuration, in_eFadeCurve, in_bBypassInternalValueInterpolation);
}
#endif //AK_SUPPORT_WCHAR

AKRESULT ResetRTPCValue(const char* in_pszRtpcName,
				AkGameObjectID in_gameObjectID,
				AkTimeMs in_uValueChangeDuration /*= 0*/,
				AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/,
				bool in_bBypassInternalValueInterpolation /*= false*/
				)
{
	AkRtpcID id = GetIDFromString( in_pszRtpcName );
	if ( id == AK_INVALID_RTPC_ID )
		return AK_IDNotFound;

	return ResetRTPCValue( id, in_gameObjectID, in_uValueChangeDuration, in_eFadeCurve, in_bBypassInternalValueInterpolation );
}

AKRESULT SetGameObjectAuxSendValues(
		AkGameObjectID		in_GameObj,				///< the unique object ID
		AkAuxSendValue*	in_aEnvironmentValues,		///< variable-size array of AkAuxSendValue(s)
		AkUInt32			in_uNumEnvValues		///< number of elements in struct
		)
{
	AKASSERT(g_pAudioMgr);

	if( in_uNumEnvValues )
	{
#ifdef CHECK_SOUND_ENGINE_INPUT_VALID
		for ( AkUInt32 i = 0; i < in_uNumEnvValues; ++i )
		{
			if ( !(AkMath::IsValidFloatInput( in_aEnvironmentValues[i].fControlValue ) ) )
				HANDLE_INVALID_FLOAT_INPUT( AKTEXT("AK::SoundEngine::SetGameObjectAuxSendValues : Invalid Float in in_aEnvironmentValues") );
		}
#endif
	}

	AutoReserveMsg pItem(QueuedMsgType_GameObjEnvValues, AkQueuedMsg::Sizeof_GameObjEnvValues() + sizeof(AkAuxSendValue) * (in_uNumEnvValues-1));

	pItem->gameobjenvvalues.gameObjID = in_GameObj;
	pItem->gameobjenvvalues.uNumValues = in_uNumEnvValues;
	AKPLATFORM::AkMemCpy( pItem->gameobjenvvalues.pEnvValues, in_aEnvironmentValues, sizeof( AkAuxSendValue ) * in_uNumEnvValues );

	return AK_Success;
}

AKRESULT RegisterBusVolumeCallback(
			AkUniqueID in_busID,
			AkBusCallbackFunc in_pfnCallback
			)
{
	if( g_pBusCallbackMgr )
	{
		return g_pBusCallbackMgr->SetVolumeCallback( in_busID, in_pfnCallback );
	}
	return AK_Fail;
}

AKRESULT RegisterBusMeteringCallback(
			AkUniqueID in_busID,						///< Bus ID, as obtained by GetIDFromString( bus_name ).
			AkBusMeteringCallbackFunc in_pfnCallback,	///< Callback function.
			AkMeteringFlags in_eMeteringFlags			///< Metering flags.
			)
{
	if( g_pBusCallbackMgr )
	{
		return g_pBusCallbackMgr->SetMeteringCallback( in_busID, in_pfnCallback, in_eMeteringFlags );
	}
	return AK_Fail;
}

AKRESULT SetGameObjectOutputBusVolume(
		AkGameObjectID		in_emitterID,			///< the emitter unique object ID
		AkGameObjectID		in_listenerID,			///< the listener unique object ID
		AkReal32			in_fControlValue	///< control value for dry level
		)
{
	AKASSERT( g_pAudioMgr );
#ifdef CHECK_SOUND_ENGINE_INPUT_VALID
	if ( !(AkMath::IsValidFloatInput( in_fControlValue )) )
		HANDLE_INVALID_FLOAT_INPUT( AKTEXT("AK::SoundEngine::SetGameObjectOutputBusVolume : Invalid Float in in_fControlValue") );
#endif

	AutoReserveMsg pItem( QueuedMsgType_GameObjDryLevel, AkQueuedMsg::Sizeof_GameObjDryLevel() );
	pItem->gameobjdrylevel.emitter.gameObjID = in_emitterID;
	pItem->gameobjdrylevel.listener.gameObjID = in_listenerID;
	pItem->gameobjdrylevel.fValue = in_fControlValue;

	return AK_Success;
}

AKRESULT SetObjectObstructionAndOcclusion(
	AkGameObjectID in_EmitterID,           ///< Game object ID.
	AkGameObjectID in_uListenerID,         ///< Listener ID.
	AkReal32 in_fObstructionLevel,		///< ObstructionLevel : [0.0f..1.0f]
	AkReal32 in_fOcclusionLevel			///< OcclusionLevel   : [0.0f..1.0f]
	)
{
	AKASSERT( g_pAudioMgr );

	AutoReserveMsg pItem( QueuedMsgType_GameObjObstruction, AkQueuedMsg::Sizeof_GameObjObstruction() );

	pItem->gameobjobstr.emitter.gameObjID = in_EmitterID;
	pItem->gameobjobstr.listener.gameObjID = in_uListenerID;
	pItem->gameobjobstr.fObstructionLevel = in_fObstructionLevel;
	pItem->gameobjobstr.fOcclusionLevel = in_fOcclusionLevel;


	return AK_Success;
}

AKRESULT SetMultipleObstructionAndOcclusion(AkGameObjectID in_EmitterID, AkGameObjectID in_uListenerID, AkObstructionOcclusionValues* in_fObstructionOcclusionValues, AkUInt32 in_uNumOcclusionObstruction)
{
	AKASSERT( g_pAudioMgr );

	// Here, we need to create the complete variable size object on the stack.
	// And then use the placement new for the object.
	// We have to copy the array over since the enqueue function can only do one write and this write must be atomic.
	AkUInt32 uAllocSize = AkQueuedMsg::Sizeof_GameObjMultiObstructionBase() + in_uNumOcclusionObstruction * sizeof(AkObstructionOcclusionValues);
	if (uAllocSize > g_pAudioMgr->GetMaximumMsgSize())
	{
		MONITOR_ERRORMSG(AKTEXT("AK::SoundEngine::SetMultiplePositions: too many positions."));
		return AK_InvalidParameter;
	}

	AkQueuedMsg *pItem = g_pAudioMgr->ReserveQueue(QueuedMsgType_GameObjMultipleObstruction, uAllocSize);
	if (pItem == NULL)
		return AK_InvalidParameter;	//Too big, could not fit.	This is the only message where this could happen.

	pItem->gameobjmultiobstr.emitter.gameObjID = in_EmitterID;
	pItem->gameobjmultiobstr.listener.gameObjID = in_uListenerID;
	pItem->gameobjmultiobstr.uNumObstructionOcclusionValues = in_uNumOcclusionObstruction;
	for (AkUInt16 i = 0; i < in_uNumOcclusionObstruction; ++i)
	{
		pItem->gameobjmultiobstr.pObstructionOcclusionValues[i] = in_fObstructionOcclusionValues[i];
	}

	g_pAudioMgr->FinishQueueWrite();

	return AK_Success;
}


AKRESULT GetContainerHistory(AK::IWriteBytes * in_pBytes)
{
	CAkFunctionCritical GlobalLock;

	CAkIndexItem<CAkParameterNodeBase*>& idx = g_pIndex->GetNodeIndex(AkNodeType_Default);

	AkAutoLock<CAkLock> IndexLock(idx.GetLock());

	for (CAkIndexItem<CAkParameterNodeBase*>::AkMapIDToPtr::Iterator iter = idx.m_mapIDToPtr.Begin(); iter != idx.m_mapIDToPtr.End(); ++iter)
	{
		CAkParameterNodeBase* pNode = static_cast<CAkParameterNodeBase*>(*iter);
		if (pNode->NodeCategory() == AkNodeCategory_RanSeqCntr)
		{
			AK::WriteBytesMem bytesMem;
			bytesMem.SetMemPool(AkMemID_Object);

			if (static_cast<CAkRanSeqCntr*>(pNode)->SerializeHistory(&bytesMem) != AK_Success)
				return AK_Fail;

			AkInt32 iDummy = 0;
			if (!in_pBytes->Write<AkUniqueID>(pNode->ID())
				|| !in_pBytes->Write<AkInt32>(bytesMem.Count())
				|| !in_pBytes->WriteBytes(bytesMem.Bytes(), bytesMem.Count(), iDummy))
				return AK_Fail;
		}
	}

	return AK_Success;
}

AKRESULT SetContainerHistory(AK::IReadBytes * in_pBytes)
{
	CAkFunctionCritical GlobalLock;

	CAkIndexItem<CAkParameterNodeBase*>& idx = g_pIndex->GetNodeIndex(AkNodeType_Default);

	AkAutoLock<CAkLock> IndexLock(idx.GetLock());

	AkUniqueID idNode = AK_INVALID_UNIQUE_ID;
	while (in_pBytes->Read<AkUniqueID>(idNode))
	{
		AkInt32 iBytes = 0;
		if (!in_pBytes->Read<AkInt32>(iBytes))
			return AK_Fail;

		CAkParameterNodeBase * pNode = idx.GetPtrAndAddRef(idNode);
		if (pNode)
		{
			AKRESULT res = static_cast<CAkRanSeqCntr*>(pNode)->DeserializeHistory(in_pBytes);

			pNode->Release();

			if (res != AK_Success)
				return res;
		}
		else // Structure not found; read into dummy buffer
		{
			void * pDummy = AkAlloc(AkMemID_Object, iBytes);
			if (!pDummy)
				return AK_InsufficientMemory;

			AkInt32 iBytesRead;
			bool res = in_pBytes->ReadBytes(pDummy, iBytes, iBytesRead);

			AkFree(AkMemID_Object, pDummy);

			if (!res)
				return AK_Fail;
		}
	}

	return AK_Success;
}

AKRESULT SetVolumeThreshold( AkReal32 in_fVolumeThresholdDB )
{
#ifdef CHECK_SOUND_ENGINE_INPUT_VALID
	if ( !(AkMath::IsValidFloatInput( in_fVolumeThresholdDB ) ) )
		HANDLE_INVALID_FLOAT_INPUT( AKTEXT("AK::SoundEngine::SetVolumeThreshold : Invalid Float in in_fVolumeThresholdDB") );
#endif

	return SetVolumeThresholdInternal( in_fVolumeThresholdDB, AkCommandPriority_Game );
}

AKRESULT SetVolumeThresholdInternal( AkReal32 in_fVolumeThresholdDB, AkCommandPriority in_uReserved )
{
	if( in_fVolumeThresholdDB < AK_MINIMUM_VOLUME_DBFS
		|| in_fVolumeThresholdDB > AK_MAXIMUM_VOLUME_DBFS )
		return AK_InvalidParameter;

	// First, check if the provided value must be considered.
	// the rule is : If the games sets something, we accept it.
	// if Wwise sets something, we accept it, unless the game previously set one.
	// the init bank sets it only if neither the game of Wwise did set a different value.
	if( in_uReserved <= g_eVolumeThresholdPriority )
	{
		g_eVolumeThresholdPriority = in_uReserved;

		// To protect against numerical errors, use our three methods of doing the dB-to-Lin conversion
		// and pick the highest mark as our linear threshold.

		AkReal32 fThresholdLinA = powf( 10.0f, in_fVolumeThresholdDB * 0.05f );
		AkReal32 fThresholdLinB = AkMath::dBToLin( in_fVolumeThresholdDB );

		g_fVolumeThreshold = AkMath::Max( fThresholdLinA, fThresholdLinB );
		g_fVolumeThresholdDB = in_fVolumeThresholdDB;
		AKASSERT( g_fVolumeThreshold <= 1.0f && g_fVolumeThreshold >=0.0f );
	}
	return AK_Success;
}

AKRESULT SetMaxNumVoicesLimit( AkUInt16 in_maxNumberVoices )
{
	return SetMaxNumVoicesLimitInternal( in_maxNumberVoices, AkCommandPriority_Game );
}

AKRESULT WaitSoundEngineInit()
{
	if(g_pAudioMgr == NULL)
		return AK_Fail;

	while(CAkOutputMgr::NumDevices() == 0)
	{
		//Soundengine just started.  Wait until completed.
		g_pAudioMgr->WakeupEventsConsumer();
		AkSleep(5);
	}

	for (AkUInt32 i = 0; i < 200 && !CAkOutputMgr::MainDeviceInitialized(); i++)
	{
		AK::SoundEngine::RenderAudio();
		g_pAudioMgr->WakeupEventsConsumer();		
		AkSleep(5);
	}

	if (!CAkOutputMgr::MainDeviceInitialized())			
		return AK_InitBankNotLoaded;

	return AK_Success;
}

// Get the output speaker configuration.
AkChannelConfig GetSpeakerConfiguration(AkOutputDeviceID in_idOutput)
{
	AkChannelConfig channelConfig;
	if (WaitSoundEngineInit() != AK_Success)
	{
		AKASSERT(!"Init.bnk was not loaded prior to GetSpeakerConfiguration call.");
		return channelConfig;
	}

	g_csMain.Lock();
	AkDevice * pDevice = CAkOutputMgr::GetDevice(in_idOutput);
	if ( pDevice && pDevice->Sink() )
		channelConfig = pDevice->GetSpeakerConfig();
	g_csMain.Unlock();
	return channelConfig;
}

AKRESULT GetPanningRule(
	AkPanningRule &		out_ePanningRule,	///< Returned panning rule (AkPanningRule_Speakers or AkPanningRule_Headphone) for given output.
	AkOutputDeviceID in_idOutput
	)
{
	if (WaitSoundEngineInit() != AK_Success)
	{
		AKASSERT(!"Init.bnk was not loaded prior to GetPanningRule call.");
		return AK_Fail;
	}

	g_csMain.Lock();
	AKRESULT res = AK_Fail;
	AkDevice * pDevice = CAkOutputMgr::GetDevice(in_idOutput);
	if ( pDevice )
	{
		out_ePanningRule = pDevice->ePanningRule;
		res = AK_Success;
	}
	g_csMain.Unlock();
	return res;
}

AKRESULT SetPanningRule(
	AkPanningRule		in_ePanningRule,	///< Panning rule.
	AkOutputDeviceID in_idDevice
	)
{
	AutoReserveMsg pItem( QueuedMsgType_SetPanningRule, AkQueuedMsg::Sizeof_SetPanningRule() );
	pItem->setPanningRule.idOutput = in_idDevice;	
	pItem->setPanningRule.panRule = in_ePanningRule;


	return AK_Success;
}

AKRESULT GetSpeakerAngles(
	AkReal32 *			io_pfSpeakerAngles,			///< Returned array of loudspeaker pair angles, in degrees relative to azimuth [0,180]. Pass NULL to get the required size of the array.
	AkUInt32 &			io_uNumAngles,				///< Returned number of angles in io_pfSpeakerAngles, which is the minimum between the value that you pass in, and the number of angles corresponding to the output configuration, or just the latter if io_pfSpeakerAngles is NULL.
	AkReal32 &			out_fHeightAngle,			///< Elevation of the height layer, in degrees relative to the plane.
	AkOutputDeviceID	in_idOutput					
	)
{
	if (WaitSoundEngineInit() != AK_Success)
	{
		AKASSERT(!"Init.bnk was not loaded prior to GetSpeakerAngles call.");
		return AK_Fail;
	}

	AKRESULT res = AK_Fail;
	g_csMain.Lock();
	AkDevice * pDevice = CAkOutputMgr::GetDevice(in_idOutput);
	if ( pDevice )
		res = pDevice->GetSpeakerAngles( io_pfSpeakerAngles, io_uNumAngles, out_fHeightAngle );
	g_csMain.Unlock();
	return res;
}

AKRESULT SetSpeakerAngles(
	const AkReal32 *	in_pfSpeakerAngles,		///< Array of loudspeaker pair angles, in degrees relative to azimuth [0,180].
	AkUInt32			in_uNumAngles,			///< Number of elements in in_pfSpeakerAngles. It should correspond to the output configuration.
	AkReal32 			in_fHeightAngle,		///< Elevation of the height layer, in degrees relative to the plane.
	AkOutputDeviceID	in_idOutput
	)
{
	if ( !in_pfSpeakerAngles || in_uNumAngles < AK::GetNumberOfAnglesForConfig( AK_SPEAKER_SETUP_DEFAULT_PLANE ) )
	{
		AKASSERT( !"Invalid angles, or invalid number of angles" );
		return AK_InvalidParameter;
	}

#ifdef CHECK_SOUND_ENGINE_INPUT_VALID
	if ( !(IsValidFloatVolumes( in_pfSpeakerAngles, in_uNumAngles ) ) )
		HANDLE_INVALID_FLOAT_INPUT( AKTEXT("AK::SoundEngine::SetSpeakerAngles : Invalid Float in in_pfSpeakerAngles") );
#endif

	AutoReserveMsg pItem( QueuedMsgType_SetSpeakerAngles, AkQueuedMsg::Sizeof_SetSpeakerAngles() );

	size_t uSize = in_uNumAngles * sizeof( AkReal32 );
	pItem->setSpeakerAngles.pfSpeakerAngles = (AkReal32*)AkAlloc(AkMemID_Object, uSize );
	if ( !pItem->setSpeakerAngles.pfSpeakerAngles )
	{
		pItem->type = QueuedMsgType_Invalid;
		return AK_InsufficientMemory;
	}

	memcpy( pItem->setSpeakerAngles.pfSpeakerAngles, in_pfSpeakerAngles, uSize );
	pItem->setSpeakerAngles.uNumAngles = in_uNumAngles;
	pItem->setSpeakerAngles.fHeightAngle = in_fHeightAngle;	
	pItem->setSpeakerAngles.idOutput = in_idOutput;

	return AK_Success;
}

AKRESULT SetMaxNumVoicesLimitInternal( AkUInt16 in_maxNumberVoices, AkCommandPriority in_uReserved )
{
	if( in_maxNumberVoices == 0 )
		return AK_InvalidParameter;

	// First, check if the provided value must be considered.
	// the rule is : If the games sets something, we accept it.
	// if Wwise sets something, we accept it, unless the game previously set one.
	// the init bank sets it only if neither the game of Wwise did set a different value.
	if( in_uReserved <= g_eNumVoicesPriority )
	{
		g_eNumVoicesPriority = in_uReserved;

		CAkURenderer::GetGlobalLimiter().UpdateMax( in_maxNumberVoices );
	}
	return AK_Success;
}

AKRESULT SetMaxNumDangerousVirtVoicesLimitInternal( AkUInt16 in_maxNumberVoices, AkCommandPriority in_uReserved )
{
	// First, check if the provided value must be considered.
	// the rule is : If the games sets something, we accept it.
	// if Wwise sets something, we accept it, unless the game previously set one.
	// the init bank sets it only if neither the game of Wwise did set a different value.
	if ( in_uReserved <= g_eNumDangerousVirtVoicesPriority )
	{
		g_eNumDangerousVirtVoicesPriority = in_uReserved;

		CAkURenderer::SetMaxNumDangerousVirtVoices( in_maxNumberVoices );
	}
	return AK_Success;
}

#ifndef AK_OPTIMIZED
AkLoudnessFrequencyWeighting g_eLoudnessFrequencyWeighting;
AkLoudnessFrequencyWeighting GetLoudnessFrequencyWeighting()
{
	return g_eLoudnessFrequencyWeighting;
}
void SetLoudnessFrequencyWeighting( AkLoudnessFrequencyWeighting in_eLoudnessFrequencyWeighting )
{
	g_eLoudnessFrequencyWeighting = in_eLoudnessFrequencyWeighting;
}
#endif


//////////////////////////////////////////////////////////////////////////////////
//Monitoring
//////////////////////////////////////////////////////////////////////////////////
#ifndef AK_OPTIMIZED
IALMonitor* GetMonitor()
{
	return AkMonitor::Get();
}
#else
IALMonitor* GetMonitor()
{
	// No monitoring in optimized mode
	// Get Monitor should be removed from the SDK...
	return NULL;
}
#endif

//////////////////////////////////////////////////////////////////////////////////
// Event Management
//////////////////////////////////////////////////////////////////////////////////

AkPlayingID PostEvent(
	AkUniqueID in_ulEventID,
	AkGameObjectID in_GameObjID,
	AkUInt32 in_uiFlags,// = 0
	AkCallbackFunc in_pfnCallback, // = NULL
	void* in_pCookie, // = NULL
	AkUInt32 in_cExternals, // = 0
	AkExternalSourceInfo *in_pExternalSources, // = NULL
	AkPlayingID	in_PlayingID //= AK_INVALID_PLAYING_ID
	)
{
	AkCustomParamType temp;
	AkCustomParamType *pParam = NULL;
	if ( AK_EXPECT_FALSE( in_cExternals != 0 ) )
	{
		temp.customParam = 0;
		temp.ui32Reserved = 0;
		temp.pExternalSrcs = AkExternalSourceArray::Create(in_cExternals, in_pExternalSources);
		if (temp.pExternalSrcs == (AkUIntPtr)NULL)
			return AK_INVALID_PLAYING_ID ;

		pParam = &temp;
	}
	AkPlayingID playingID = PostEvent( in_ulEventID, in_GameObjID, in_uiFlags, in_pfnCallback, in_pCookie, pParam, in_PlayingID );

	if (playingID == AK_INVALID_PLAYING_ID && in_cExternals != 0)
		temp.pExternalSrcs->Release();

	return playingID;
}

AkPlayingID PostEvent(
	AkUniqueID in_ulEventID,
	AkGameObjectID in_GameObjID,
	AkUInt32 in_uiFlags,
	AkCallbackFunc in_pfnCallback,
	void* in_pCookie,
	AkCustomParamType * in_pCustomParam,
	AkPlayingID	in_PlayingID /*= AK_INVALID_PLAYING_ID*/
	)
{
	AKASSERT(g_pIndex);
	AKASSERT(g_pAudioMgr);
	AKASSERT( g_pPlayingMgr );

	// This function call does get the pointer and addref it in an atomic call.
	CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( in_ulEventID );
	if( pEvent )
	{
		AutoReserveMsg pItem(QueuedMsgType_Event, AkQueuedMsg::Sizeof_Event() );
		pItem->event.pEvent = pEvent;
		pItem->event.eventID = in_ulEventID;
		if ( in_pCustomParam != NULL )
		{
			pItem->event.CustomParam = *in_pCustomParam;
		}
		else
		{
			pItem->event.CustomParam.customParam = 0;
			pItem->event.CustomParam.ui32Reserved = 0;
			pItem->event.CustomParam.pExternalSrcs = NULL;
		}

		pItem->event.PlayingID = AkAtomicInc32(&g_PlayingID);
		pItem->event.TargetPlayingID = in_PlayingID;
		pItem->event.gameObjID = in_GameObjID;

		AKRESULT eResult = g_pPlayingMgr->AddPlayingID(pItem->event, in_pfnCallback, in_pCookie, in_uiFlags, pItem->event.pEvent->ID());
		if( eResult != AK_Success )
		{
			pEvent->Release();
			pItem->type = QueuedMsgType_Invalid;

			return AK_INVALID_PLAYING_ID;
		}


		return pItem->event.PlayingID;
	}

	MONITOR_ERROR_PARAM( AK::Monitor::ErrorCode_EventIDNotFound, in_ulEventID, AK_INVALID_PLAYING_ID, in_GameObjID, in_ulEventID, false );
	return AK_INVALID_PLAYING_ID;
}

#ifdef AK_SUPPORT_WCHAR
AkPlayingID PostEvent(
	const wchar_t* in_pszEventName,
	AkGameObjectID in_GameObjID,
	AkUInt32 in_uiFlags,
	AkCallbackFunc in_pfnCallback,
	void* in_pCookie,
	AkUInt32 in_cExternals, // = 0
	AkExternalSourceInfo *in_pExternalSources, // = NULL
	AkPlayingID	in_PlayingID //= AK_INVALID_PLAYING_ID
	)
{
	AkCustomParamType temp;
	AkCustomParamType *pParam = NULL;
	if ( AK_EXPECT_FALSE( in_cExternals != 0 ) )
	{
		temp.customParam = 0;
		temp.ui32Reserved = 0;
		temp.pExternalSrcs = AkExternalSourceArray::Create(in_cExternals, in_pExternalSources);
		if (temp.pExternalSrcs == (AkUIntPtr)NULL)
			return AK_INVALID_PLAYING_ID ;

		pParam = &temp;
	}

	AkUniqueID EventID = GetIDFromString( in_pszEventName );
    AkPlayingID returnedPlayingID = PostEvent( EventID, in_GameObjID, in_uiFlags, in_pfnCallback, in_pCookie, pParam, in_PlayingID );
	if( returnedPlayingID == AK_INVALID_PLAYING_ID )
	{
		MONITOR_ERRORMSG2( L"Failed posting event: ", in_pszEventName );

		if (in_cExternals != 0)
			temp.pExternalSrcs->Release();
	}
	return returnedPlayingID;
}
#endif //AK_SUPPORT_WCHAR

AkPlayingID PostEvent(
	const char* in_pszEventName,
	AkGameObjectID in_GameObjID,
	AkUInt32 in_uiFlags,
	AkCallbackFunc in_pfnCallback,
	void* in_pCookie,
	AkUInt32 in_cExternals, // = 0
	AkExternalSourceInfo *in_pExternalSources, // = NULL
	AkPlayingID	in_PlayingID //= AK_INVALID_PLAYING_ID
	)
{
	AkCustomParamType temp;
	AkCustomParamType *pParam = NULL;
	if ( AK_EXPECT_FALSE( in_cExternals != 0 ) )
	{
		temp.customParam = 0;
		temp.ui32Reserved = 0;
		temp.pExternalSrcs = AkExternalSourceArray::Create(in_cExternals, in_pExternalSources);
		if (temp.pExternalSrcs == (AkUIntPtr)NULL)
			return AK_INVALID_PLAYING_ID ;

		pParam = &temp;
	}

	AkUniqueID EventID = GetIDFromString( in_pszEventName );
    AkPlayingID returnedPlayingID = PostEvent( EventID, in_GameObjID, in_uiFlags, in_pfnCallback, in_pCookie, pParam, in_PlayingID );
	if( returnedPlayingID == AK_INVALID_PLAYING_ID )
	{
		MONITOR_ERRORMSG2( "Failed posting event: ", in_pszEventName );
		if (in_cExternals != 0)
			temp.pExternalSrcs->Release();
	}
	return returnedPlayingID;
}

// This function Was added to allow a very specific and special way to playback something.
// An instance of the plug-in will be created
AkPlayingID PlaySourcePlugin( AkUInt32 in_plugInID, AkUInt32 in_CompanyID, AkGameObjectID in_GameObjID )
{
	AKASSERT(g_pAudioMgr);

	AutoReserveMsg pItem(QueuedMsgType_SourcePluginAction, AkQueuedMsg::Sizeof_PlaySourcePlugin() );
    pItem->sourcePluginAction.CustomParam.customParam = 0;
	pItem->sourcePluginAction.CustomParam.ui32Reserved = 0;
	pItem->sourcePluginAction.CustomParam.pExternalSrcs = 0;

    pItem->sourcePluginAction.PluginID = in_plugInID;
    pItem->sourcePluginAction.CompanyID = in_CompanyID;

    pItem->sourcePluginAction.ActionType = AkSourcePluginActionType_Play;

	pItem->sourcePluginAction.PlayingID = AkAtomicInc32(&g_PlayingID);
	pItem->sourcePluginAction.TargetPlayingID = AK_INVALID_PLAYING_ID;
	pItem->sourcePluginAction.gameObjID = in_GameObjID;



	return pItem->sourcePluginAction.PlayingID;
}

AKRESULT StopSourcePlugin( AkUInt32 in_plugInID, AkUInt32 in_CompanyID, AkPlayingID in_playingID )
{
    AKASSERT(g_pAudioMgr);

	AutoReserveMsg pItem( QueuedMsgType_SourcePluginAction, AkQueuedMsg::Sizeof_PlaySourcePlugin() );

    pItem->sourcePluginAction.CustomParam.customParam = 0;
	pItem->sourcePluginAction.CustomParam.ui32Reserved = 0;
	pItem->sourcePluginAction.CustomParam.pExternalSrcs = 0;

    pItem->sourcePluginAction.PluginID = in_plugInID;
    pItem->sourcePluginAction.CompanyID = in_CompanyID;

    pItem->sourcePluginAction.ActionType = AkSourcePluginActionType_Stop;

	pItem->sourcePluginAction.PlayingID = in_playingID;
	pItem->sourcePluginAction.gameObjID = AK_INVALID_GAME_OBJECT;



	return AK_Success;
}

AKRESULT ExecuteActionOnEvent(
	AkUniqueID in_eventID,
	AkActionOnEventType in_ActionType,
    AkGameObjectID in_gameObjectID, /*= AK_INVALID_GAME_OBJECT*/
	AkTimeMs in_uTransitionDuration /*= 0*/,
	AkCurveInterpolation in_eFadeCurve, /*= AkCurveInterpolation_Linear*/
	AkPlayingID in_PlayingID /*= AK_INVALID_PLAYING_ID*/
	)
{
	AKASSERT(g_pIndex);

	// This function call does get the pointer and addref it in an atomic call.
	CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( in_eventID );
	if (pEvent != NULL)
	{
		AutoReserveMsg pItem( QueuedMsgType_EventAction, AkQueuedMsg::Sizeof_EventAction() );

		pItem->eventAction.pEvent = pEvent;
		pItem->eventAction.eventID = in_eventID;
		pItem->eventAction.gameObjID = in_gameObjectID;
		pItem->eventAction.eActionToExecute = in_ActionType;
		pItem->eventAction.uTransitionDuration = in_uTransitionDuration;
		pItem->eventAction.eFadeCurve = in_eFadeCurve;
		pItem->eventAction.TargetPlayingID = in_PlayingID;

		return AK_Success;
	}

	MONITOR_ERROR_PARAM( AK::Monitor::ErrorCode_EventIDNotFound, in_eventID, AK_INVALID_PLAYING_ID, in_gameObjectID, in_eventID, false );
	return AK_Fail;
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT ExecuteActionOnEvent(
	const wchar_t* in_pszEventName,
	AkActionOnEventType in_ActionType,
    AkGameObjectID in_gameObjectID, /*= AK_INVALID_GAME_OBJECT*/
	AkTimeMs in_uTransitionDuration /*= 0*/,
	AkCurveInterpolation in_eFadeCurve, /*= AkCurveInterpolation_Linear*/
	AkPlayingID in_PlayingID /*= AK_INVALID_PLAYING_ID*/
	)
{
	AkUniqueID EventID = GetIDFromString( in_pszEventName );
	return ExecuteActionOnEvent( EventID, in_ActionType, in_gameObjectID, in_uTransitionDuration, in_eFadeCurve, in_PlayingID );
}
#endif //AK_SUPPORT_WCHAR

AKRESULT ExecuteActionOnEvent(
	const char* in_pszEventName,
	AkActionOnEventType in_ActionType,
    AkGameObjectID in_gameObjectID, /*= AK_INVALID_GAME_OBJECT*/
	AkTimeMs in_uTransitionDuration /*= 0*/,
	AkCurveInterpolation in_eFadeCurve, /*= AkCurveInterpolation_Linear*/
	AkPlayingID in_PlayingID /*= AK_INVALID_PLAYING_ID*/
	)
{
	AkUniqueID EventID = GetIDFromString( in_pszEventName );
	return ExecuteActionOnEvent( EventID, in_ActionType, in_gameObjectID, in_uTransitionDuration, in_eFadeCurve, in_PlayingID );
}

AKRESULT PostMIDIOnEvent(
	AkUniqueID in_eventID,
    AkGameObjectID in_gameObjectID,
	AkMIDIPost* in_pPosts,
	AkUInt16 in_uNumPosts
	)
{
	if( in_uNumPosts == 0 || in_pPosts == NULL )
		return AK_InvalidParameter;

	AKASSERT(g_pAudioMgr);
	AKASSERT(g_pIndex);

	AKRESULT result = AK_Success;

	// This function call does get the pointer and addref it in an atomic call.
	CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( in_eventID );
	if ( pEvent == NULL )
	{
		MONITOR_ERROR_PARAM( AK::Monitor::ErrorCode_EventIDNotFound, in_eventID, AK_INVALID_PLAYING_ID, in_gameObjectID, in_eventID, false );
		result = AK_Fail;
	}

	AkUInt32 uAllocSize = 0;
	AkQueuedMsg *pItem = NULL;

	// Here, we need to create the complete variable size object on the stack.
	// And then use the placement new for the object.
	// We have to copy the array over since the enqueue function can only do one write and this write must be atomic.
	if ( result == AK_Success )
	{
		uAllocSize = AkQueuedMsg::Sizeof_EventPostMIDIBase() + in_uNumPosts * sizeof( AkMIDIPost );
		if (uAllocSize > g_pAudioMgr->GetMaximumMsgSize())
		{
			MONITOR_ERRORMSG( AKTEXT("AK::SoundEngine::PostMIDIOnEvent: too many event posts.") );
			result = AK_InvalidParameter;
		}
	}

	if ( result == AK_Success )
	{
		pItem = g_pAudioMgr->ReserveQueue( QueuedMsgType_EventPostMIDI, uAllocSize );
		if ( pItem == NULL )
			result = AK_InvalidParameter;	//Too big, could not fit.	This is the only message where this could happen.
	}

	if ( result != AK_Success )
	{
		if ( pEvent != NULL )
			pEvent->Release();

		return result;
	}

	pItem->postMIDI.pEvent = pEvent;
	pItem->postMIDI.eventID = in_eventID;
	pItem->postMIDI.gameObjID = in_gameObjectID;
	pItem->postMIDI.uNumPosts = in_uNumPosts;
	for ( AkUInt16 i = 0; i < in_uNumPosts; ++i )
		pItem->postMIDI.aPosts[i] = in_pPosts[i];

	g_pAudioMgr->FinishQueueWrite();

	return AK_Success;
}

AKRESULT StopMIDIOnEvent(
	AkUniqueID in_eventID /*AK_INVALID_UNIQUE_ID*/,
	AkGameObjectID in_gameObjectID /*AK_INVALID_GAME_OBJECT*/
	)
{
	AKASSERT(g_pIndex);

	// This function call does get the pointer and addref it in an atomic call.
	CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( in_eventID );
	if (pEvent == NULL && in_eventID != AK_INVALID_UNIQUE_ID)
	{
		MONITOR_ERROR_PARAM( AK::Monitor::ErrorCode_EventIDNotFound, in_eventID, AK_INVALID_PLAYING_ID, in_gameObjectID, in_eventID, false );
		return AK_Fail;
	}

	AutoReserveMsg pItem( QueuedMsgType_EventStopMIDI, AkQueuedMsg::Sizeof_EventStopMIDI() );

	pItem->stopMIDI.pEvent = pEvent;
	pItem->stopMIDI.eventID = in_eventID;
	pItem->stopMIDI.gameObjID = in_gameObjectID;

	return AK_Success;
}


AKRESULT PinEventInStreamCache( AkUniqueID in_eventID, AkPriority in_uActivePriority, AkPriority in_uInactivePriority )
{
	CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( in_eventID );
	if (pEvent != NULL)
	{
		AKASSERT(g_pIndex);

		AutoReserveMsg Item(QueuedMsgType_LockUnlockStreamCache, AkQueuedMsg::Sizeof_LockUnlockStreamCache());
		Item->lockUnlockStreamCache.pEvent = pEvent;
		Item->lockUnlockStreamCache.eventID = in_eventID;
		Item->lockUnlockStreamCache.gameObjID = AK_INVALID_GAME_OBJECT;
		Item->lockUnlockStreamCache.ActivePriority = in_uActivePriority;
		Item->lockUnlockStreamCache.InactivePriority = in_uInactivePriority;
		Item->lockUnlockStreamCache.Lock = true;

		return AK_Success;
	}
	else
	{
		MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_EventIDNotFound, in_eventID, AK_INVALID_PLAYING_ID, AK_INVALID_PLAYING_ID, in_eventID, false);
		return AK_Fail;
	}
}

AKRESULT PinEventInStreamCache( const char* in_pszEventName, AkPriority in_uActivePriority, AkPriority in_uInactivePriority )
{
	AkUniqueID EventID = GetIDFromString( in_pszEventName );
	return PinEventInStreamCache( EventID, in_uActivePriority, in_uInactivePriority);
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT PinEventInStreamCache( const wchar_t* in_pszEventName, AkPriority in_uActivePriority, AkPriority in_uInactivePriority )
{
	AkUniqueID EventID = GetIDFromString( in_pszEventName );
	return PinEventInStreamCache( EventID, in_uActivePriority, in_uInactivePriority );
}
#endif

AKRESULT UnpinEventInStreamCache( AkUniqueID in_eventID )
{
	CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( in_eventID );
	if (pEvent != NULL)
	{
		AKASSERT(g_pIndex);
		AutoReserveMsg pItem( QueuedMsgType_LockUnlockStreamCache, AkQueuedMsg::Sizeof_LockUnlockStreamCache() );
		pItem->lockUnlockStreamCache.pEvent = pEvent;
		pItem->lockUnlockStreamCache.eventID = in_eventID;
		pItem->lockUnlockStreamCache.gameObjID = AK_INVALID_GAME_OBJECT;
		pItem->lockUnlockStreamCache.Lock = false;


		return AK_Success;
	}

	MONITOR_ERROR_PARAM( AK::Monitor::ErrorCode_EventIDNotFound, in_eventID, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, in_eventID, false );
	return AK_Fail;
}

AKRESULT UnpinEventInStreamCache( const char* in_pszEventName )
{
	AkUniqueID EventID = GetIDFromString( in_pszEventName );
	return UnpinEventInStreamCache( EventID );
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT UnpinEventInStreamCache( const wchar_t* in_pszEventName )
{
	AkUniqueID EventID = GetIDFromString( in_pszEventName );
	return UnpinEventInStreamCache( EventID );
}
#endif

AKRESULT GetBufferStatusForPinnedEvent( AkUniqueID in_eventID, AkReal32& out_fPercentBuffered, bool& out_bCacheFull )
{
	CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( in_eventID );
	if (pEvent != NULL)
	{
		AKRESULT res = GetBufferStatusForPinnedFiles( pEvent, NULL, out_fPercentBuffered, out_bCacheFull );
		pEvent->Release();
		return res;
	}
	else
	{
		MONITOR_ERROR_PARAM( AK::Monitor::ErrorCode_EventIDNotFound, in_eventID, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, in_eventID, false );
		return AK_Fail;
	}
}

AKRESULT GetBufferStatusForPinnedEvent( const char* in_pszEventName, AkReal32& out_fPercentBuffered, bool& out_bCacheFull )
{
	AkUniqueID EventID = GetIDFromString( in_pszEventName );
	return GetBufferStatusForPinnedEvent( EventID, out_fPercentBuffered, out_bCacheFull );
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT GetBufferStatusForPinnedEvent( const wchar_t* in_pszEventName, AkReal32& out_fPercentBuffered, bool& out_bCacheFull )
{
	AkUniqueID EventID = GetIDFromString( in_pszEventName );
	return GetBufferStatusForPinnedEvent( EventID, out_fPercentBuffered, out_bCacheFull );
}
#endif

AKRESULT SeekOnEvent(
	AkUniqueID in_eventID,
	AkGameObjectID in_gameObjectID,
	AkTimeMs in_iPosition,
	bool in_bSeekToNearestMarker,
	AkPlayingID in_PlayingID
	)
{
	AKASSERT(g_pIndex);

	// This function call does get the pointer and addref it in an atomic call.
	CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( in_eventID );
	if (pEvent != NULL)
	{
		AutoReserveMsg pItem( QueuedMsgType_Seek, AkQueuedMsg::Sizeof_Seek() );
		pItem->seek.pEvent = pEvent;
		pItem->seek.eventID = in_eventID;
		pItem->seek.gameObjID = in_gameObjectID;
		pItem->seek.playingID = in_PlayingID;
		pItem->seek.bIsSeekRelativeToDuration = false;
		pItem->seek.iPosition = in_iPosition;
		pItem->seek.bSnapToMarker = in_bSeekToNearestMarker;
		return AK_Success;
	}

	MONITOR_ERROR_PARAM( AK::Monitor::ErrorCode_EventIDNotFound, in_eventID, AK_INVALID_PLAYING_ID, in_gameObjectID, in_eventID, false );
	return AK_Fail;
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT SeekOnEvent(
	const wchar_t* in_pszEventName,
	AkGameObjectID in_gameObjectID,
	AkTimeMs in_iPosition,
	bool in_bSeekToNearestMarker,
	AkPlayingID in_PlayingID
	)
{
	AkUniqueID EventID = GetIDFromString( in_pszEventName );
	return SeekOnEvent( EventID, in_gameObjectID, in_iPosition, in_bSeekToNearestMarker, in_PlayingID );
}
#endif //AK_SUPPORT_WCHAR

AKRESULT SeekOnEvent(
	const char* in_pszEventName,
	AkGameObjectID in_gameObjectID,
	AkTimeMs in_iPosition,
	bool in_bSeekToNearestMarker,
	AkPlayingID in_PlayingID
	)
{
	AkUniqueID EventID = GetIDFromString( in_pszEventName );
	return SeekOnEvent( EventID, in_gameObjectID, in_iPosition, in_bSeekToNearestMarker, in_PlayingID );
}

AKRESULT SeekOnEvent(
	AkUniqueID in_eventID,
	AkGameObjectID in_gameObjectID,
	AkReal32 in_fPercent,
	bool in_bSeekToNearestMarker,
	AkPlayingID in_PlayingID
	)
{
	AKASSERT(g_pIndex);

	// This function call does get the pointer and addref it in an atomic call.
	CAkEvent* pEvent = g_pIndex->m_idxEvents.GetPtrAndAddRef( in_eventID );
	if (pEvent != NULL)
	{
		AutoReserveMsg pItem( QueuedMsgType_Seek, AkQueuedMsg::Sizeof_Seek() );
		pItem->seek.pEvent = pEvent;
		pItem->seek.gameObjID = in_gameObjectID;
		pItem->seek.playingID = in_PlayingID;
		pItem->seek.bIsSeekRelativeToDuration	= true;
		pItem->seek.fPercent	= in_fPercent;
		pItem->seek.bSnapToMarker = in_bSeekToNearestMarker;
		return AK_Success;
	}
	MONITOR_ERROR_PARAM( AK::Monitor::ErrorCode_EventIDNotFound, in_eventID, AK_INVALID_PLAYING_ID, in_gameObjectID, in_eventID, false );
	return AK_Fail;
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT SeekOnEvent(
	const wchar_t* in_pszEventName,
	AkGameObjectID in_gameObjectID,
	AkReal32 in_fPercent,
	bool in_bSeekToNearestMarker,
	AkPlayingID in_PlayingID
	)
{
	AkUniqueID EventID = GetIDFromString( in_pszEventName );
	return SeekOnEvent( EventID, in_gameObjectID, in_fPercent, in_bSeekToNearestMarker, in_PlayingID );
}
#endif //AK_SUPPORT_WCHAR

AKRESULT SeekOnEvent(
	const char* in_pszEventName,
	AkGameObjectID in_gameObjectID,
	AkReal32 in_fPercent,
	bool in_bSeekToNearestMarker,
	AkPlayingID in_PlayingID
	)
{
	AkUniqueID EventID = GetIDFromString( in_pszEventName );
	return SeekOnEvent( EventID, in_gameObjectID, in_fPercent, in_bSeekToNearestMarker, in_PlayingID );
}

void CancelEventCallbackCookie( void* in_pCookie )
{
	if( g_pPlayingMgr )
		g_pPlayingMgr->CancelCallbackCookie( in_pCookie );
}


void CancelEventCallbackGameObject( AkGameObjectID in_gameObjectID )
{
	if (g_pPlayingMgr)
		g_pPlayingMgr->CancelCallbackGameObject( in_gameObjectID );
}

void CancelEventCallback( AkPlayingID in_playingID )
{
	if( g_pPlayingMgr )
		g_pPlayingMgr->CancelCallback( in_playingID );
}

AKRESULT GetSourcePlayPosition(
	AkPlayingID		in_PlayingID,				///< PlayingID returned by PostEvent
	AkTimeMs*		out_puPosition,				///< Position (in ms) of the source associated with that PlayingID
	bool			in_bExtrapolate /*= true*/	///< Real position is extrapolated based on the difference between when this function is called and when the real sound's position was updated by the sound engine.
	)
{
	if( out_puPosition == NULL )
		return AK_InvalidParameter;

	AkSourcePosition position = { 0 };
	AkUInt32 cPositions = 1;

	AKRESULT eResult = g_pPositionRepository->GetCurrPositions(in_PlayingID, &position, &cPositions, in_bExtrapolate);

	*out_puPosition = position.msTime;

	return eResult;
}

AKRESULT GetSourcePlayPositions(
	AkPlayingID		in_PlayingID,				///< PlayingID returned by PostEvent
	AkSourcePosition* out_puPositions,			///< Audio Node IDs and positions of sources associated with the specified playing ID
	AkUInt32 *		io_pcPositions,				///< Number of entries in out_puPositions. Needs to be set to the size of the array: it is adjusted to the actual number of returned entries
	bool			in_bExtrapolate /*= true*/	///< Real position is extrapolated based on the difference between when this function is called and when the real sound's position was updated by the sound engine.
	)
{
	if ( io_pcPositions == NULL || (out_puPositions == NULL && *io_pcPositions > 0) )
		return AK_InvalidParameter;

	return g_pPositionRepository->GetCurrPositions(in_PlayingID, out_puPositions, io_pcPositions, in_bExtrapolate);
}

AKRESULT GetSourceStreamBuffering(
	AkPlayingID		in_PlayingID,				///< Playing ID returned by AK::SoundEngine::PostEvent()
	AkTimeMs &		out_buffering,				///< Returned amount of buffering (in ms) of the source (or one of the sources) associated with that playing ID
	bool &			out_bIsBuffering			///< Returned buffering status of the source(s) associated with that playing ID
	)
{
	AKRESULT eStatus;
	AKRESULT eReturn = g_pPositionRepository->GetBuffering( in_PlayingID, out_buffering, eStatus );
	if ( eReturn == AK_Success )
		out_bIsBuffering = ( eStatus == AK_Success );
	else
	{
		out_buffering = 0;
		out_bIsBuffering = false;
	}
	return eReturn;
}

//////////////////////////////////////////////////////////////////////////////////
// Dynamic Dialog
//////////////////////////////////////////////////////////////////////////////////
namespace DynamicDialogue
{

AkUniqueID ResolveDialogueEvent(
		AkUniqueID			in_eventID,					///< Unique ID of the dialog event
		AkArgumentValueID*	in_aArgumentValues,			///< Array of argument value IDs
		AkUInt32			in_uNumArguments,			///< Number of argument value IDs in in_aArguments
		AkPlayingID			in_idSequence,				///< Optional sequence ID in which the token will be inserted (for profiling purposes)
		AkCandidateCallbackFunc in_candidateCallbackFunc,
		void* in_pCookie
	)
{
	CAkDialogueEvent * pDialogueEvent = g_pIndex->m_idxDialogueEvents.GetPtrAndAddRef( in_eventID );
	if ( !pDialogueEvent )
		return AK_INVALID_UNIQUE_ID;

	AkUniqueID audioNodeID = pDialogueEvent->ResolvePath( in_aArgumentValues, in_uNumArguments, in_idSequence, in_candidateCallbackFunc, in_pCookie );

	pDialogueEvent->Release();

	return audioNodeID;
}

#ifdef AK_SUPPORT_WCHAR
AkUniqueID ResolveDialogueEvent(
		const wchar_t*		in_pszEventName,			///< Name of the dialog event
		const wchar_t**		in_aArgumentValueNames,		///< Array of argument value names
		AkUInt32			in_uNumArguments,			///< Number of argument value names in in_aArguments
		AkPlayingID			in_idSequence,				///< Optional sequence ID in which the token will be inserted (for profiling purposes)
		AkCandidateCallbackFunc in_candidateCallbackFunc,
		void* in_pCookie
	)
{
	AkUniqueID eventID = GetIDFromString( in_pszEventName );

	CAkDialogueEvent * pDialogueEvent = g_pIndex->m_idxDialogueEvents.GetPtrAndAddRef( eventID );
	if ( !pDialogueEvent )
	{
		MONITOR_ERRORMSG2( L"Unknown Dialogue Event: ", in_pszEventName );
		return AK_INVALID_UNIQUE_ID;
	}

	AkArgumentValueID * pArgumentValues = (AkArgumentValueID *) AkAlloca( in_uNumArguments * sizeof( AkArgumentValueID ) );

	AkUniqueID audioNodeID = AK_INVALID_UNIQUE_ID;

	AKRESULT eResult = pDialogueEvent->ResolveArgumentValueNames( in_aArgumentValueNames, pArgumentValues, in_uNumArguments );
	if ( eResult == AK_Success )
	{
		audioNodeID = pDialogueEvent->ResolvePath( pArgumentValues, in_uNumArguments, in_idSequence, in_candidateCallbackFunc, in_pCookie );
	}

	pDialogueEvent->Release();

	return audioNodeID;
}
#endif //AK_SUPPORT_WCHAR

AkUniqueID ResolveDialogueEvent(
		const char*			in_pszEventName,			///< Name of the dialog event
		const char**		in_aArgumentValueNames,		///< Array of argument value names
		AkUInt32			in_uNumArguments,			///< Number of argument value names in in_aArguments
		AkPlayingID			in_idSequence,				///< Optional sequence ID in which the token will be inserted (for profiling purposes)
		AkCandidateCallbackFunc in_candidateCallbackFunc,
		void* in_pCookie
	)
{
	AkUniqueID eventID = GetIDFromString( in_pszEventName );

	CAkDialogueEvent * pDialogueEvent = g_pIndex->m_idxDialogueEvents.GetPtrAndAddRef( eventID );
	if ( !pDialogueEvent )
	{
		MONITOR_ERRORMSG2( "Unknown Dialogue Event: ", in_pszEventName );
		return AK_INVALID_UNIQUE_ID;
	}

	AkArgumentValueID * pArgumentValues = (AkArgumentValueID *) AkAlloca( in_uNumArguments * sizeof( AkArgumentValueID ) );

	AkUniqueID audioNodeID = AK_INVALID_UNIQUE_ID;

	AKRESULT eResult = pDialogueEvent->ResolveArgumentValueNames( in_aArgumentValueNames, pArgumentValues, in_uNumArguments );
	if ( eResult == AK_Success )
	{
		audioNodeID = pDialogueEvent->ResolvePath( pArgumentValues, in_uNumArguments, in_idSequence, in_candidateCallbackFunc, in_pCookie );
	}

	pDialogueEvent->Release();

	return audioNodeID;
}

AKRESULT GetDialogueEventCustomPropertyValue(
	AkUniqueID in_eventID,			///< Unique ID of dialogue event
	AkUInt32 in_uPropID,			///< Property ID
	AkInt32& out_iValue				///< Property Value
	)
{
	CAkDialogueEvent * pDialogueEvent = g_pIndex->m_idxDialogueEvents.GetPtrAndAddRef(in_eventID);
	if (!pDialogueEvent)
		return AK_IDNotFound;

	AkPropValue * pValue = pDialogueEvent->FindCustomProp(in_uPropID);
	if (pValue)
	{
		out_iValue = pValue->iValue;
		pDialogueEvent->Release();
		return AK_Success;
	}
	else
	{
		pDialogueEvent->Release();
		return AK_PartialSuccess;
	}
}

AKRESULT GetDialogueEventCustomPropertyValue(
	AkUniqueID in_eventID,			///< Unique ID of dialogue event
	AkUInt32 in_uPropID,			///< Property ID
	AkReal32& out_fValue			///< Property Value
	)
{
	CAkDialogueEvent * pDialogueEvent = g_pIndex->m_idxDialogueEvents.GetPtrAndAddRef(in_eventID);
	if (!pDialogueEvent)
		return AK_IDNotFound;

	AkPropValue * pValue = pDialogueEvent->FindCustomProp(in_uPropID);
	if (pValue)
	{
		out_fValue = pValue->fValue;
		pDialogueEvent->Release();
		return AK_Success;
	}
	else
	{
		pDialogueEvent->Release();
		return AK_PartialSuccess;
	}
}

} // namespace DynamicDialogue

//////////////////////////////////////////////////////////////////////////////////
// Dynamic Sequence
//////////////////////////////////////////////////////////////////////////////////
namespace DynamicSequence
{
AkPlayingID Open(
	AkGameObjectID	in_gameObjectID,
	AkUInt32		in_uiFlags /* = 0 */,
	AkCallbackFunc	in_pfnCallback /* = NULL*/,
	void* 			in_pCookie	   /* = NULL */,
	DynamicSequenceType in_eDynamicSequenceType /*= DynamicSequenceType_SampleAccurate*/
	)
{
	AKASSERT(g_pIndex);
	AKASSERT(g_pAudioMgr);
	AKASSERT( g_pPlayingMgr );

	AkPlayingID uPlayingID = AkAtomicInc32(&g_PlayingID);
	CAkDynamicSequence* pSequence = CAkDynamicSequence::Create( uPlayingID, in_eDynamicSequenceType );
	if( pSequence )
	{
		AutoReserveMsg pItem(QueuedMsgType_OpenDynamicSequence, AkQueuedMsg::Sizeof_OpenDynamicSequence() );
		pItem->opendynamicsequence.PlayingID = uPlayingID;
		pItem->opendynamicsequence.TargetPlayingID = AK_INVALID_PLAYING_ID;
		pItem->opendynamicsequence.pDynamicSequence = pSequence;

		pItem->opendynamicsequence.gameObjID = in_gameObjectID;
		pItem->opendynamicsequence.CustomParam.customParam = 0;
		pItem->opendynamicsequence.CustomParam.ui32Reserved = 0;
		pItem->opendynamicsequence.CustomParam.pExternalSrcs = 0;

		AKRESULT eResult = g_pPlayingMgr->AddPlayingID( pItem->opendynamicsequence, in_pfnCallback, in_pCookie, in_uiFlags, AK_INVALID_UNIQUE_ID );
		if( eResult != AK_Success )
		{
			pItem->opendynamicsequence.pDynamicSequence->Release();
			pItem->type = QueuedMsgType_Invalid;

			return AK_INVALID_PLAYING_ID;
		}


		return uPlayingID;
	}

	return AK_INVALID_PLAYING_ID;
}

static CAkDynamicSequence * GetSequence(AkPlayingID in_playingID)
{
	CAkDynamicSequence *pSequence = g_pIndex->m_idxDynamicSequences.GetPtrAndAddRef(in_playingID);
	if (pSequence)
	{
		if (!pSequence->WasClosed())
		{
			return pSequence;
		}
		else
		{
#ifndef AK_OPTIMIZED
			AkOSChar szMsg[64];
			AK_OSPRINTF(szMsg, 64, AKTEXT("Dynamic Sequence already closed: %u"), in_playingID);
			MONITOR_ERRORMSG_PLAYINGID(szMsg, in_playingID);
#endif
			pSequence->Release();
			return NULL;
		}
	}

#ifndef AK_OPTIMIZED
	AkOSChar szMsg[64];
	AK_OSPRINTF(szMsg, 64, AKTEXT("Dynamic Sequence ID not found: %u"), in_playingID);
	MONITOR_ERRORMSG(szMsg);
#endif
	return NULL;
}

static AKRESULT _DynamicSequenceCommand(
	AkPlayingID in_playingID,
	AkQueuedMsg_DynamicSequenceCmd::Command in_eCommand,
	AkTimeMs in_uTransitionDuration,
	AkCurveInterpolation in_eFadeCurve)
{
	AKASSERT(g_pIndex);
	AKASSERT(g_pAudioMgr);

	CAkDynamicSequence *pSequence = GetSequence( in_playingID );
	if( pSequence )
	{		
		AutoReserveMsg pItem( QueuedMsgType_DynamicSequenceCmd, AkQueuedMsg::Sizeof_DynamicSequenceCmd() );
		pItem->dynamicsequencecmd.pDynamicSequence = pSequence;

		if( in_eCommand == AkQueuedMsg_DynamicSequenceCmd::Close )
		{
			// Must do it here. to prevent another call to succeed.
			// this operation can be processed from here without locking
			pItem->dynamicsequencecmd.pDynamicSequence->Close();

			// Then we still have to push the close command so that the release is done to free the resources.
		}

		pItem->dynamicsequencecmd.eCommand = in_eCommand;
		pItem->dynamicsequencecmd.uTransitionDuration = in_uTransitionDuration;
		pItem->dynamicsequencecmd.eFadeCurve = in_eFadeCurve;
		return AK_Success;
	}
	
	return AK_Fail;
}

AKRESULT Play( AkPlayingID in_playingID, AkTimeMs in_uTransitionDuration, AkCurveInterpolation in_eFadeCurve )
{
	return _DynamicSequenceCommand( in_playingID, AkQueuedMsg_DynamicSequenceCmd::Play, in_uTransitionDuration, in_eFadeCurve );
}

AKRESULT Pause( AkPlayingID in_playingID, AkTimeMs in_uTransitionDuration, AkCurveInterpolation in_eFadeCurve )
{
	return _DynamicSequenceCommand( in_playingID, AkQueuedMsg_DynamicSequenceCmd::Pause, in_uTransitionDuration, in_eFadeCurve );
}

AKRESULT Resume( AkPlayingID in_playingID, AkTimeMs in_uTransitionDuration, AkCurveInterpolation in_eFadeCurve )
{
	return _DynamicSequenceCommand( in_playingID, AkQueuedMsg_DynamicSequenceCmd::Resume, in_uTransitionDuration, in_eFadeCurve );
}

AKRESULT Stop( AkPlayingID	in_playingID, AkTimeMs in_uTransitionDuration, AkCurveInterpolation in_eFadeCurve )
{
	return _DynamicSequenceCommand( in_playingID, AkQueuedMsg_DynamicSequenceCmd::Stop, in_uTransitionDuration, in_eFadeCurve );
}

AKRESULT Break( AkPlayingID	in_playingID )
{
	return _DynamicSequenceCommand( in_playingID, AkQueuedMsg_DynamicSequenceCmd::Break, 0, AkCurveInterpolation_Linear );
}

AKRESULT Seek( AkPlayingID in_playingID, AkTimeMs in_iPosition, bool in_bSeekToNearestMarker )
{
	CAkDynamicSequence *pSequence = GetSequence(in_playingID);
	if (pSequence)
	{
		AutoReserveMsg pItem(QueuedMsgType_DynamicSequenceSeek, AkQueuedMsg::Sizeof_DynamicSequenceSeek());
		pItem->dynamicsequenceSeek.pDynamicSequence = pSequence;
		pItem->dynamicsequenceSeek.iSeekTime = in_iPosition;
		pItem->dynamicsequenceSeek.bIsSeekRelativeToDuration = false;
		pItem->dynamicsequenceSeek.bSnapToNearestMarker = in_bSeekToNearestMarker;

		return AK_Success;
	}
	return AK_Fail;	
}

AKRESULT Seek(AkPlayingID in_playingID, AkReal32 in_fPercent, bool in_bSeekToNearestMarker)
{
	CAkDynamicSequence *pSequence = GetSequence(in_playingID);
	if (pSequence)
	{
		AutoReserveMsg pItem(QueuedMsgType_DynamicSequenceSeek, AkQueuedMsg::Sizeof_DynamicSequenceSeek());
		pItem->dynamicsequenceSeek.pDynamicSequence = pSequence;
		pItem->dynamicsequenceSeek.fSeekPercent = in_fPercent;
		pItem->dynamicsequenceSeek.bIsSeekRelativeToDuration = true;
		pItem->dynamicsequenceSeek.bSnapToNearestMarker = in_bSeekToNearestMarker;

		return AK_Success;
	}
	return AK_Fail;
}

AKRESULT GetPauseTimes(AkPlayingID in_playingID, AkUInt32 &out_uTime, AkUInt32 &out_uDuration)
{
	AKASSERT(g_pIndex);
	AKASSERT(g_pAudioMgr);

	CAkDynamicSequence *pSequence = GetSequence(in_playingID);
	if (pSequence)
	{	
		pSequence->GetPauseTimes(out_uTime, out_uDuration);
		pSequence->Release();
		return AK_Success;
	}

	return AK_Fail;
}

AKRESULT GetPlayingItem(AkPlayingID in_playingID, AkUniqueID & out_audioNodeID, void *& out_pCustomInfo)
{
	AKASSERT(g_pIndex);
	AKASSERT(g_pAudioMgr);

	CAkDynamicSequence *pSequence = GetSequence(in_playingID);
	if (pSequence)
	{
		pSequence->GetQueuedItem(out_audioNodeID, out_pCustomInfo);
		pSequence->Release();
		return AK_Success;
	}
	return AK_Fail;
}

AKRESULT Close(AkPlayingID in_playingID)
{
	return _DynamicSequenceCommand( in_playingID, AkQueuedMsg_DynamicSequenceCmd::Close, 0, AkCurveInterpolation_Linear );
}

Playlist * LockPlaylist(
	AkPlayingID in_playingID						///< AkPlayingID returned by DynamicSequence::Open
	)
{
	CAkDynamicSequence * pDynaSeq = g_pIndex->m_idxDynamicSequences.GetPtrAndAddRef( in_playingID );
	if ( !pDynaSeq )
	{
#ifndef AK_OPTIMIZED
		AkOSChar szMsg[ 64 ];
		AK_OSPRINTF( szMsg, 64, AKTEXT("Dynamic Sequence ID not found: %u"), in_playingID );
		MONITOR_ERRORMSG( szMsg );
#endif
		return NULL;
	}

	Playlist * pPlaylist = pDynaSeq->LockPlaylist();

	pDynaSeq->Release();

	return pPlaylist;
}

AKRESULT UnlockPlaylist(
	AkPlayingID in_playingID						///< AkPlayingID returned by DynamicSequence::Open
	)
{
	CAkDynamicSequence * pDynaSeq = g_pIndex->m_idxDynamicSequences.GetPtrAndAddRef( in_playingID );
	if ( !pDynaSeq )
	{
#ifndef AK_OPTIMIZED
		AkOSChar szMsg[ 64 ];
		AK_OSPRINTF( szMsg, 64, AKTEXT("Dynamic Sequence ID not found: %u"), in_playingID );
		MONITOR_ERRORMSG( szMsg );
#endif
		return AK_Fail;
	}

	pDynaSeq->UnlockPlaylist();

	pDynaSeq->Release();

	return AK_Success;
}

} // namespace DynamicSequence
//////////////////////////////////////////////////////////////////////////////////
// Game Objects
//////////////////////////////////////////////////////////////////////////////////
AKRESULT RegisterGameObj(AkGameObjectID in_GameObj)
{
	return RegisterGameObj(in_GameObj, NULL);
}

AKRESULT RegisterGameObj(AkGameObjectID in_GameObj, const char* in_pszObjName)
{
	if (in_GameObj >= AkGameObjectID_ReservedStart) // omni
		return AK_InvalidParameter;

	AutoReserveMsg pItem( QueuedMsgType_RegisterGameObj, AkQueuedMsg::Sizeof_RegisterGameObj() );
	pItem->reggameobj.gameObjID = in_GameObj;

#ifndef AK_OPTIMIZED
	if ( in_pszObjName )
		pItem->reggameobj.pMonitorData = AkMonitor::Monitor_AllocateGameObjNameString( in_GameObj, in_pszObjName );
	else
		pItem->reggameobj.pMonitorData = NULL;
#else
	pItem->reggameobj.pMonitorData = NULL;
#endif


	return AK_Success;
}

AKRESULT UnregisterGameObj( AkGameObjectID in_GameObj )
{
	if (in_GameObj >= AkGameObjectID_ReservedStart)
		return AK_Fail;

	AutoReserveMsg pItem( QueuedMsgType_UnregisterGameObj, AkQueuedMsg::Sizeof_UnregisterGameObj() );
	pItem->unreggameobj.gameObjID = in_GameObj;
	
	return AK_Success;
}

AKRESULT UnregisterAllGameObj()
{
	AutoReserveMsg pItem( QueuedMsgType_UnregisterGameObj, AkQueuedMsg::Sizeof_UnregisterGameObj() );
	pItem->unreggameobj.gameObjID = AK_INVALID_GAME_OBJECT;
	
	return AK_Success;
}

//////////////////////////////////////////////////////////////////////////////////
// Bank Management
//////////////////////////////////////////////////////////////////////////////////

void DefaultBankCallbackFunc(
                    AkBankID    /*in_bankID*/,
					const void*	/*in_pInMemoryBankPtr*/,
                    AKRESULT	in_eLoadResult,
					void *		in_pCookie )
{
		AKASSERT( in_pCookie );
		AkSyncCaller * pReturnInfo = (AkSyncCaller*)in_pCookie;

		// Fill status info.
		pReturnInfo->m_eResult	  = in_eLoadResult;

		pReturnInfo->Done();
}

AKRESULT ClearBanks()
{
	if( !g_pBankManager )
		return AK_Fail;

	// We must first clear prepared events prior clearing all banks.
	AKRESULT eResult = ClearPreparedEvents();

	if( eResult == AK_Success )
	{
		AkSyncCaller syncLoader;
		eResult = g_pBankManager->InitSyncOp(syncLoader);
		if( eResult != AK_Success )
			return eResult;

		CAkBankMgr::AkBankQueueItem item;
		item.eType						 = CAkBankMgr::QueueItemClearBanks;
		item.callbackInfo.pfnBankCallback = g_pDefaultBankCallbackFunc;
		item.callbackInfo.pCookie		 = &syncLoader;
		item.bankLoadFlag				= AkBankLoadFlag_None;

		eResult = g_pBankManager->QueueBankCommand( item );

		return g_pBankManager->WaitForSyncOp(syncLoader, eResult);
	}

	return eResult;
}

// Bank loading I/O settings.
AKRESULT SetBankLoadIOSettings(
    AkReal32            in_fThroughput,         // Average throughput of bank data streaming. Default is AK_DEFAULT_BANKLOAD_THROUGHPUT.
    AkPriority          in_priority             // Priority of bank streaming. Default is AK_DEFAULT_PRIORITY.
    )
{
    AKRESULT eResult = AK_Fail;

	AKASSERT( g_pBankManager );

	if( g_pBankManager )
	{
		eResult = g_pBankManager->SetBankLoadIOSettings(
            in_fThroughput,
            in_priority );
	}

	return eResult;
}

////////////////////////////////////////////////////////////////////////////
// Banks
////////////////////////////////////////////////////////////////////////////

// Load/Unload Bank Internal.
static AKRESULT LoadBankInternal(
	AkBankID            in_bankID,              // ID of the bank to load.
	AkFileNameString		in_pszBankFileName,
	AkBankLoadFlag		in_flag,
	CAkBankMgr::AkBankQueueItemType	in_eType,	// load or unload
	AkBankCallbackFunc  in_pfnBankCallback,	    // Callback function that will be called.
	void *              in_pCookie,				// Callback cookie.
	const void *		in_pInMemoryBank = NULL,	// Pointer to an in-memory bank
	AkUInt32			in_ui32InMemoryBankSize = 0 // Size of the specified in-memory bank
	)
{
    CAkBankMgr::AkBankQueueItem item;
	item.eType					= in_eType;
	item.bankID			= in_bankID;
    item.callbackInfo.pfnBankCallback	= in_pfnBankCallback;
    item.callbackInfo.pCookie			= in_pCookie;
	item.load.pInMemoryBank			= in_pInMemoryBank;
	item.load.ui32InMemoryBankSize	= in_ui32InMemoryBankSize;
	item.fileName		= in_pszBankFileName;
	item.bankLoadFlag				= in_flag;

	return g_pBankManager->QueueBankCommand(item);
}

static AKRESULT PrepareEventInternal(
	PreparationType		in_PreparationType,
    AkBankCallbackFunc  in_pfnBankCallback,	// Callback function that will be called.
	void *              in_pCookie,			// Callback cookie.
	AkUniqueID*			in_pEventID,
	AkUInt32			in_uNumEvents,
	bool				in_bDoAllocAndCopy = true // When set to false, the provided array can be used as is and must be freed.
	)
{
	if( in_uNumEvents == 0 )
		return AK_InvalidParameter;

    CAkBankMgr::AkBankQueueItem item;
	item.eType						= ( in_PreparationType == Preparation_Load ) ? CAkBankMgr::QueueItemPrepareEvent : CAkBankMgr::QueueItemUnprepareEvent;
	item.bankLoadFlag				= AkBankLoadFlag_None;
	item.callbackInfo.pCookie			= in_pCookie;
	item.callbackInfo.pfnBankCallback	= in_pfnBankCallback;

	item.prepare.numEvents = in_uNumEvents;

	if( in_uNumEvents == 1 )
	{
		item.prepare.eventID = *in_pEventID;
	}
	else if( in_bDoAllocAndCopy )
	{
		item.prepare.pEventID = (AkUniqueID*)AkAlloc( AkMemID_Object, in_uNumEvents * sizeof( AkUniqueID ) );
		if( !item.prepare.pEventID )
		{
			return AK_InsufficientMemory;
		}
		memcpy( item.prepare.pEventID, in_pEventID, in_uNumEvents * sizeof( AkUniqueID ) );
	}
	else
	{
		item.prepare.pEventID = in_pEventID;
	}

	AKRESULT eResult = g_pBankManager->QueueBankCommand( item );

	if( eResult != AK_Success && in_uNumEvents != 1 )
	{
		AKASSERT( item.prepare.pEventID );
		AkFree( AkMemID_Object, item.prepare.pEventID );
	}

	return eResult;
}

static AKRESULT PrepareGameSyncsInternal(
    AkBankCallbackFunc  in_pfnBankCallback,	    // Callback function that will be called.
	void *              in_pCookie,				// Callback cookie.
	bool				in_bSupported,
	AkGroupType			in_eGroupType,
	AkUInt32			in_GroupID,
	AkUInt32*			in_pGameSyncID,
	AkUInt32			in_uNumGameSync,
	bool				in_bDoAllocAndCopy = true // When set to false, the provided array can be used as is and must be freed.
	)
{
	if( in_uNumGameSync == 0 || in_pGameSyncID == NULL )
		return AK_InvalidParameter;

	CAkBankMgr::AkBankQueueItem item;
	item.eType						= CAkBankMgr::QueueItemSupportedGameSync;
	item.bankLoadFlag				= AkBankLoadFlag_None;
    item.callbackInfo.pfnBankCallback	= in_pfnBankCallback;
    item.callbackInfo.pCookie			= in_pCookie;
	item.gameSync.bSupported		= in_bSupported;
	item.gameSync.eGroupType		= in_eGroupType;
	item.gameSync.uGroupID			= in_GroupID;

	item.gameSync.uNumGameSync = in_uNumGameSync;

	if( in_uNumGameSync == 1 )
	{
		item.gameSync.uGameSyncID = *in_pGameSyncID;
	}
	else if( in_bDoAllocAndCopy )
	{
		item.gameSync.pGameSyncID = (AkUInt32*)AkAlloc( AkMemID_Object, in_uNumGameSync * sizeof( AkUInt32 ) );
		if( !item.gameSync.pGameSyncID )
		{
			return AK_InsufficientMemory;
		}
		memcpy( item.gameSync.pGameSyncID, in_pGameSyncID, in_uNumGameSync * sizeof( AkUInt32 ) );
	}
	else
	{
		item.gameSync.pGameSyncID = in_pGameSyncID;
	}

	AKRESULT eResult = g_pBankManager->QueueBankCommand( item );

	if( eResult != AK_Success && in_uNumGameSync != 1 )
	{
		AKASSERT( item.gameSync.pGameSyncID );
		AkFree( AkMemID_Object, item.gameSync.pGameSyncID );
	}

	return eResult;
}

#ifdef AK_SUPPORT_WCHAR
void RemoveFileExtension( wchar_t* in_pstring );// propotype to avoid no prototype warning with some compilers.
void RemoveFileExtension( wchar_t* in_pstring )
{
	int i = (int)wcslen(in_pstring) - 1;

	while (i >= 0)
	{
		if (in_pstring[i] == L'.')
		{
			in_pstring[i] = L'\0';
			return;
		}

		i--;
	}
}
#endif //AK_SUPPORT_WCHAR

static void RemoveFileExtension( char* in_pstring )
{
	int i = (int)strlen(in_pstring) - 1;

	while (i >= 0)
	{
		if (in_pstring[i] == '.')
		{
			in_pstring[i] = '\0';
			return;
		}

		i--;
	}
}

static AkBankID GetBankIDFromString( const char* in_pszString )
{
	// Remove the file extension of it was provided.

	char szStringWithoutExtension[ AK_MAX_STRING_SIZE ];
	AKPLATFORM::SafeStrCpy( szStringWithoutExtension, in_pszString, AK_MAX_STRING_SIZE );

	RemoveFileExtension( szStringWithoutExtension );

	return GetIDFromString(szStringWithoutExtension);
}

#ifdef AK_SUPPORT_WCHAR
static AkBankID GetBankIDFromString( const wchar_t* in_pszString )
{
	// Convert to char* so it is conform with UpdateBankName(...)
	char szString[ AK_MAX_STRING_SIZE ];
	AkWideCharToChar( in_pszString, AK_MAX_STRING_SIZE-1, szString );
	szString[ AK_MAX_STRING_SIZE-1 ] = 0;

	return GetBankIDFromString( szString );
}
#endif //AK_SUPPORT_WCHAR

#ifdef AK_SUPPORT_WCHAR
// Synchronous bank load (by string).
AKRESULT LoadBank(
	    const wchar_t*      in_pszString,		    // Name/path of the bank to load
        AkBankID &          out_bankID				// Returned bank ID.
	    )
{
	AkFileNameString bankNameStr;
	if (bankNameStr.Copy(in_pszString) != AK_Success)
		return AK_InsufficientMemory;

	out_bankID = GetBankIDFromString(bankNameStr.Get());

	AkSyncCaller syncLoader;
	AKRESULT eResult = g_pBankManager->InitSyncOp(syncLoader);
	if( eResult != AK_Success )
		return eResult;

    eResult = LoadBankInternal(
	                        out_bankID,             // ID of the bank to load.
							bankNameStr,
							AkBankLoadFlag_None,
                            CAkBankMgr::QueueItemLoad,
                            g_pDefaultBankCallbackFunc,// Callback function that will be called.
							&syncLoader			// Callback cookie.
	                        );

	return g_pBankManager->WaitForSyncOp(syncLoader, eResult);
}
#endif //AK_SUPPORT_WCHAR

AKRESULT LoadBank(
	    const char*         in_pszString,		    // Name/path of the bank to load
        AkBankID &          out_bankID				// Returned bank ID.
	    )
{
	AkFileNameString bankNameStr;
	if (bankNameStr.Set(in_pszString) != AK_Success)
		return AK_InsufficientMemory;

	out_bankID = GetBankIDFromString( in_pszString );

	AkSyncCaller syncLoader;
	AKRESULT eResult = g_pBankManager->InitSyncOp(syncLoader);
	if( eResult != AK_Success )
		return eResult;

    eResult = LoadBankInternal(
	                        out_bankID,             // ID of the bank to load.
							bankNameStr,
							AkBankLoadFlag_None,
                            CAkBankMgr::QueueItemLoad,
                            g_pDefaultBankCallbackFunc,// Callback function that will be called.
							&syncLoader			// Callback cookie.
	                        );

	return g_pBankManager->WaitForSyncOp(syncLoader, eResult);
}

// Synchronous bank load (by id).
AKRESULT LoadBank(
	    AkBankID			in_bankID              // Bank ID of the bank to load
	    )
{
    AkSyncCaller syncLoader;
	AKRESULT eResult = g_pBankManager->InitSyncOp(syncLoader);
	if( eResult != AK_Success )
		return eResult;

    eResult = LoadBankInternal(
	                        in_bankID,             // ID of the bank to load.
							AkFileNameString(),
							AkBankLoadFlag_UsingFileID,
                            CAkBankMgr::QueueItemLoad,
                            g_pDefaultBankCallbackFunc,// Callback function that will be called.
							&syncLoader,		    // Callback cookie.
							NULL,
							0 );

	return g_pBankManager->WaitForSyncOp(syncLoader, eResult);
}

// Synchronous bank load (from in-memory Bank, in-place).
AKRESULT LoadBankMemoryView(
	    const void *		in_pInMemoryBankPtr,	// Pointer to the in-memory bank to load
		AkUInt32			in_ui32InMemoryBankSize,// Size of the in-memory bank to load
        AkBankID &          out_bankID				// Returned bank ID.
	    )
{
	AKRESULT eResult = CAkBankMgr::GetBankInfoFromPtr( in_pInMemoryBankPtr, in_ui32InMemoryBankSize, true, out_bankID);
	if (eResult != AK_Success)
		return eResult;

    AkSyncCaller syncLoader;
	eResult = g_pBankManager->InitSyncOp(syncLoader);
	if( eResult != AK_Success )
		return eResult;

    eResult = LoadBankInternal(
	                        out_bankID,					 // ID of the bank to load.
							AkFileNameString(),
							AkBankLoadFlag_FromMemoryInPlace,
                            CAkBankMgr::QueueItemLoad,
                            g_pDefaultBankCallbackFunc,	// Callback function that will be called.
							&syncLoader,				// Callback cookie.
							in_pInMemoryBankPtr,
							in_ui32InMemoryBankSize
	                        );

	return g_pBankManager->WaitForSyncOp(syncLoader, eResult);
}

// Synchronous bank load (from in-memory Bank, out-of-place).
AKRESULT LoadBankMemoryCopy(
				  const void *		in_pInMemoryBankPtr,	// Pointer to the in-memory bank to load
				  AkUInt32			in_ui32InMemoryBankSize,// Size of the in-memory bank to load
				  AkBankID &          out_bankID				// Returned bank ID.
				  )
{
	AKRESULT eResult = CAkBankMgr::GetBankInfoFromPtr(in_pInMemoryBankPtr, in_ui32InMemoryBankSize, false, out_bankID);
	if (eResult != AK_Success)
		return eResult;

	AkSyncCaller syncLoader;
	eResult = g_pBankManager->InitSyncOp(syncLoader);
	if( eResult != AK_Success )
		return eResult;

	eResult = LoadBankInternal(
		out_bankID,					 // ID of the bank to load.
		AkFileNameString(),
		AkBankLoadFlag_FromMemoryOutOfPlace,
		CAkBankMgr::QueueItemLoad,
		g_pDefaultBankCallbackFunc,	// Callback function that will be called.
		&syncLoader,				// Callback cookie.
		in_pInMemoryBankPtr,
		in_ui32InMemoryBankSize
		);

	return g_pBankManager->WaitForSyncOp(syncLoader, eResult);
}

AK_EXTERNAPIFUNC( AKRESULT, DecodeBank )(
	const void *		in_pInMemoryBankPtr,
	AkUInt32			in_uInMemoryBankSize,
	AkMemPoolId			in_uPoolForDecodedBank,
	void * &			out_pDecodedBankPtr,
	AkUInt32 &			out_uDecodedBankSize
	)
{
 	AkUInt8 * pDataPtr = (AkUInt8 *) in_pInMemoryBankPtr;
	AkUInt8 * pDataEnd = pDataPtr + in_uInMemoryBankSize;

	AkSubchunkHeader * pDataSectionHdr = NULL;
	AkUInt8 * pDataSection = NULL;

	AkSubchunkHeader * pDataIndexHdr = NULL;
	AkBank::MediaHeader * pDataIndex = NULL;

	AkUInt32 uOutSize = 0;

	do
	{
		AkSubchunkHeader * pChunkHdr = (AkSubchunkHeader *) pDataPtr;
		pDataPtr += sizeof( AkSubchunkHeader );

		switch ( pChunkHdr->dwTag )
		{
		case BankDataChunkID:
			pDataSectionHdr = pChunkHdr;
			pDataSection = pDataPtr;
			uOutSize += sizeof( AkSubchunkHeader );
			break;

		case BankDataIndexChunkID:
			pDataIndexHdr = pChunkHdr;
			pDataIndex = (AkBank::MediaHeader *) pDataPtr;
			// no break

		default:
			// will be copied
			uOutSize += sizeof( AkSubchunkHeader ) + pChunkHdr->dwChunkSize;
			break;
		}

		pDataPtr += pChunkHdr->dwChunkSize;
	}
	while(pDataPtr < pDataEnd);

	AkUInt32 uDecodedDataSectionSize = 0;
	AkUInt32 uNumMedias = 0;
	if ( pDataIndexHdr && pDataSectionHdr )
	{
		uNumMedias = pDataIndexHdr->dwChunkSize / sizeof( AkBank::MediaHeader );
		for ( AkUInt32 iMedia = 0; iMedia < uNumMedias; ++iMedia )
		{
			AkFileParser::FormatInfo fmtInfo;
			AkUInt32 uLoopStart, uLoopEnd, uDataSize, uDataOffset;
			AKRESULT eResult = AkFileParser::Parse( pDataSection + pDataIndex [ iMedia ].uOffset, pDataIndex [ iMedia ].uSize, fmtInfo, NULL, &uLoopStart, &uLoopEnd, &uDataSize, &uDataOffset, NULL, NULL, true  );
			
			// Accept invalid files, they can be midi files, or plug-in media and do not have a proper wwise parseable header
			// In case of invalid files, decode should simply plain copy it as undecodable data.
			if (eResult != AK_Success && eResult != AK_InvalidFile)
				return eResult;

			if ( eResult == AK_InvalidFile
				|| uDataSize + uDataOffset > pDataIndex[iMedia].uSize
				|| (fmtInfo.pFormat->wFormatTag != AK_WAVE_FORMAT_VORBIS && fmtInfo.pFormat->wFormatTag != AK_WAVE_FORMAT_OPUS) )
			{
				AkUInt32 uPaddedSize = ( pDataIndex[ iMedia ].uSize + 0xf ) & ~0xf;
				uDecodedDataSectionSize += uPaddedSize;
			}
			else
			{
				AkUInt32 uMediaSize = CAkBankMgr::GetBufferSizeForDecodedMedia(fmtInfo, uDataSize, uDataOffset);
				AkUInt32 uPaddedSize = (uMediaSize + 0xf) & ~0xf;
				uDecodedDataSectionSize += uPaddedSize;
			}
		}
	}

	uOutSize += uDecodedDataSectionSize;

	out_uDecodedBankSize = uOutSize;
	if ( in_uPoolForDecodedBank == AK_INVALID_POOL_ID )
	{
		if ( out_pDecodedBankPtr == NULL )
			return AK_Success;
		// else use user-specified pointer.
	}
	else
	{
		//We have a pool id.  The pointer will be ignored so if it is not null, then the user may have made an error.
		AKASSERT(out_pDecodedBankPtr == NULL);
		out_pDecodedBankPtr = AkAlloc( in_uPoolForDecodedBank, uOutSize );

		if ( out_pDecodedBankPtr == NULL )
			return AK_InsufficientMemory;
	}

	// Now go back and create decoded bank.

	AKRESULT eResult = AK_Success;

	pDataPtr = (AkUInt8 *) in_pInMemoryBankPtr;
	AkUInt8 * pDecodePtr = (AkUInt8 *) out_pDecodedBankPtr;

	AkSubchunkHeader * pDecodedIndexHdr = NULL;
	AkBank::MediaHeader * pDecodedIndex = NULL;

	do
	{
		AkSubchunkHeader * pChunkHdr = (AkSubchunkHeader *) pDataPtr;
		AkSubchunkHeader * pDecodedChunkHdr = (AkSubchunkHeader *) pDecodePtr;
		memcpy( pDecodePtr, pDataPtr, sizeof( AkSubchunkHeader ) );
		pDataPtr += sizeof( AkSubchunkHeader );
		pDecodePtr += sizeof( AkSubchunkHeader );
		uOutSize -= sizeof(AkSubchunkHeader);

		switch ( pChunkHdr->dwTag )
		{
		case BankDataChunkID:
			AKASSERT( pDecodedIndexHdr );
			{
				AkUInt8 * pDecodePtrStart = pDecodePtr;

				for ( AkUInt32 iMedia = 0; iMedia < uNumMedias; ++iMedia )
				{
					AkFileParser::FormatInfo fmtInfo;
					AkUInt32 uLoopStart, uLoopEnd, uDataSize, uDataOffset;
					AkUInt8 * pMediaItem = pDataSection + pDataIndex [ iMedia ].uOffset;
					eResult = AkFileParser::Parse( pMediaItem, pDataIndex [ iMedia ].uSize, fmtInfo, NULL, &uLoopStart, &uLoopEnd, &uDataSize, &uDataOffset, NULL, NULL, true );
					
					// Accept invalid files, they can be midi files, or plug-in media and do not have a proper wwise parseable header
					// In case of invalid files, decode should simply plain copy it as undecodable data.
					if (eResult != AK_Success && eResult != AK_InvalidFile)
						break;

					if ( eResult == AK_InvalidFile
						|| uDataSize + uDataOffset > pDataIndex[iMedia].uSize
						|| ((fmtInfo.pFormat->wFormatTag != AK_WAVE_FORMAT_VORBIS ) && (fmtInfo.pFormat->wFormatTag != AK_WAVE_FORMAT_OPUS) ))
					{
						eResult = AK_Success;// if the Error was AK_InvalidFile, consider it a success.
						// copy
						memcpy( pDecodePtr, pMediaItem, pDataIndex [ iMedia ].uSize );

						pDecodedIndex[ iMedia ].id = pDataIndex [ iMedia ].id;
						pDecodedIndex[ iMedia ].uOffset = (AkUInt32) ( pDecodePtr - pDecodePtrStart );
						pDecodedIndex[ iMedia ].uSize = pDataIndex [ iMedia ].uSize;

						AkUInt32 uPaddedSize = ( pDataIndex[ iMedia ].uSize + 0xf ) & ~0xf;
						pDecodePtr += uPaddedSize;
					}
					else
					{
						eResult = CAkBankMgr::DecodeMedia(pDecodePtr, uOutSize, pMediaItem, pDataIndex[iMedia].uSize, (AkUInt32)((AkUInt8 *)fmtInfo.pFormat - pMediaItem), uDataOffset, sizeof(AkInt16) * fmtInfo.pFormat->nChannels, fmtInfo );
						if ( eResult != AK_Success )
							break;

						pDecodedIndex[ iMedia ].id = pDataIndex [ iMedia ].id;
						pDecodedIndex[ iMedia ].uOffset = (AkUInt32) ( pDecodePtr - pDecodePtrStart );
						pDecodedIndex[iMedia].uSize = CAkBankMgr::GetBufferSizeForDecodedMedia(fmtInfo, uDataSize, uDataOffset);

						AkUInt32 uPaddedSize = (pDecodedIndex[iMedia].uSize + 0xf) & ~0xf;
						pDecodePtr += uPaddedSize;
				}
				}
				pDecodedChunkHdr->dwChunkSize = (AkUInt32) ( pDecodePtr - pDecodePtrStart );
			}
			break;

		case BankDataIndexChunkID:
			pDecodedIndexHdr = pDecodedChunkHdr;
			pDecodedIndex = (AkBank::MediaHeader *) pDecodePtr;
			// will be rebuilt while processing data chunk.
			pDecodePtr += pChunkHdr->dwChunkSize;
			break;

		default:
			memcpy( pDecodePtr, pDataPtr, pChunkHdr->dwChunkSize );
			pDecodePtr += pChunkHdr->dwChunkSize;
			break;
		}

		pDataPtr += pChunkHdr->dwChunkSize;
	}
	while(pDataPtr < pDataEnd && eResult == AK_Success);

	if (eResult != AK_Success && out_pDecodedBankPtr != NULL && in_uPoolForDecodedBank != AK_INVALID_POOL_ID)
	{
		AkFree(in_uPoolForDecodedBank, out_pDecodedBankPtr);
		out_pDecodedBankPtr = NULL;
	}


	return eResult;
}

#ifdef AK_SUPPORT_WCHAR
// Asynchronous bank load (by string).
AKRESULT LoadBank(
	    const wchar_t*      in_pszString,           // Name/path of the bank to load.
		AkBankCallbackFunc  in_pfnBankCallback,	    // Callback function.
		void *              in_pCookie,				// Callback cookie.
		AkBankID &          out_bankID				// Returned bank ID.
	    )
{
	AkFileNameString bankString;
	if (bankString.Copy(in_pszString) != AK_Success)
		return AK_InsufficientMemory;

	out_bankID = GetBankIDFromString(bankString.Get());

    return LoadBankInternal(		out_bankID,                 // ID of the bank to load.
									bankString,
									AkBankLoadFlag_None,
                                    CAkBankMgr::QueueItemLoad,	// true = load, false = unload
                                    in_pfnBankCallback,			// Callback function that will be called.
	                                in_pCookie);					// Callback cookie.
                                    
}
#endif //AK_SUPPORT_WCHAR

AKRESULT LoadBank(
	    const char*         in_pszString,           // Name/path of the bank to load.
		AkBankCallbackFunc  in_pfnBankCallback,	    // Callback function.
		void *              in_pCookie,				// Callback cookie.
		AkBankID &          out_bankID				// Returned bank ID.
	    )
{
	AkFileNameString bankString;
	if (bankString.Copy(in_pszString) != AK_Success)
		return AK_InsufficientMemory;

	out_bankID = GetBankIDFromString(bankString.Get());

    return LoadBankInternal(		out_bankID,                 // ID of the bank to load.
									bankString,
									AkBankLoadFlag_None,
                                    CAkBankMgr::QueueItemLoad,	// true = load, false = unload
                                    in_pfnBankCallback,			// Callback function that will be called.
	                                in_pCookie);					// Callback cookie.
}

// Asynchronous bank load (by id).
AKRESULT LoadBank(
	    AkBankID			in_bankID,              // Bank ID of the bank to load.
		AkBankCallbackFunc  in_pfnBankCallback,	    // Callback function.
		void *              in_pCookie				// Callback cookie.
	    )
{
    return LoadBankInternal( in_bankID,             // ID of the bank to load.
						     AkFileNameString(),
							 AkBankLoadFlag_UsingFileID,
                             CAkBankMgr::QueueItemLoad,
                             in_pfnBankCallback,	 // Callback function that will be called.
	                         in_pCookie,             // Callback cookie.
							 NULL,
							 0
							 );
}

// Asynchronous bank load (from in-memory bank, in-place).
AKRESULT LoadBankMemoryView(
	    const void *		in_pInMemoryBankPtr,	// Pointer to the in-memory bank to load
		AkUInt32			in_ui32InMemoryBankSize,// Size of the in-memory bank to load
		AkBankCallbackFunc  in_pfnBankCallback,	    // Callback function.
		void *              in_pCookie,				// Callback cookie.
		AkBankID &          out_bankID				// Returned bank ID.
	    )
{
	AKRESULT eResult = CAkBankMgr::GetBankInfoFromPtr(in_pInMemoryBankPtr, in_ui32InMemoryBankSize, true, out_bankID);
	if (eResult != AK_Success)
		return eResult;

    return LoadBankInternal( out_bankID,			// ID of the bank to load.
							 AkFileNameString(),
							 AkBankLoadFlag_FromMemoryInPlace,
                             CAkBankMgr::QueueItemLoad,
                             in_pfnBankCallback,	// Callback function that will be called.
	                         in_pCookie,			// Callback cookie.
							 in_pInMemoryBankPtr,
							 in_ui32InMemoryBankSize
							 );
}

// Asynchronous bank load (from in-memory bank, out-of-place).
AKRESULT LoadBankMemoryCopy(
				  const void *		in_pInMemoryBankPtr,		// Pointer to the in-memory bank to load
				  AkUInt32			in_ui32InMemoryBankSize,	// Size of the in-memory bank to load
				  AkBankCallbackFunc  in_pfnBankCallback,	    // Callback function.
				  void *              in_pCookie,				// Callback cookie.
				  AkBankID &          out_bankID				// Returned bank ID.
				  )
{
	AKRESULT eResult = CAkBankMgr::GetBankInfoFromPtr(in_pInMemoryBankPtr, in_ui32InMemoryBankSize, false, out_bankID);
	if (eResult != AK_Success)
		return eResult;

	return LoadBankInternal( out_bankID,			// ID of the bank to load.
		AkFileNameString(),
		AkBankLoadFlag_FromMemoryOutOfPlace,
		CAkBankMgr::QueueItemLoad,
		in_pfnBankCallback,	// Callback function that will be called.
		in_pCookie,			// Callback cookie.
		in_pInMemoryBankPtr,
		in_ui32InMemoryBankSize
		);
}

#ifdef AK_SUPPORT_WCHAR
// Synchronous bank unload (by string).
AKRESULT UnloadBank(
	    const wchar_t*      in_pszString,				// Name/path of the bank to unload.
		const void *		in_pInMemoryBankPtr
	    )
{
    AkBankID bankID = GetBankIDFromString( in_pszString );
    return UnloadBank( bankID, in_pInMemoryBankPtr );
}
#endif //AK_SUPPORT_WCHAR

AKRESULT UnloadBank(
	    const char*         in_pszString,				// Name/path of the bank to unload.
		const void *		in_pInMemoryBankPtr
	    )
{
    AkBankID bankID = GetBankIDFromString( in_pszString );
    return UnloadBank( bankID, in_pInMemoryBankPtr );
}

// Synchronous bank unload (by id).
AKRESULT UnloadBank(
	    AkBankID            in_bankID,              // ID of the bank to unload.
		const void *		in_pInMemoryBankPtr
	    )
{
	AkSyncCaller syncLoader;
	AKRESULT eResult = g_pBankManager->InitSyncOp(syncLoader);
	if( eResult != AK_Success )
		return eResult;

    eResult = LoadBankInternal(
	                        in_bankID,              // ID of the bank to load.
							AkFileNameString(),
							AkBankLoadFlag_FromMemoryInPlace,
                            CAkBankMgr::QueueItemUnload,
                            g_pDefaultBankCallbackFunc,// Callback function that will be called.
							&syncLoader,		    // Callback cookie.
							in_pInMemoryBankPtr
	                        );

	eResult = g_pBankManager->WaitForSyncOp(syncLoader, eResult);

	return eResult;
}

#ifdef AK_SUPPORT_WCHAR
// Asynchronous bank unload (by string).
AKRESULT UnloadBank(
	    const wchar_t*      in_pszString,           // Name/path of the bank to load.
		const void *		in_pInMemoryBankPtr,
		AkBankCallbackFunc  in_pfnBankCallback,	    // Callback function.
		void *              in_pCookie 				// Callback cookie.
	    )
{
	AkBankID bankID = GetBankIDFromString(in_pszString);

    return LoadBankInternal(bankID,                 // ID of the bank to load.
							AkFileNameString(),			// no need to pass in the bank string when unloading.
							(in_pInMemoryBankPtr) ? AkBankLoadFlag_FromMemoryInPlace : AkBankLoadFlag_None,
                            CAkBankMgr::QueueItemUnload,
                            in_pfnBankCallback,	    // Callback function that will be called.
                            in_pCookie,             // Callback cookie.
							in_pInMemoryBankPtr
                            );
}
#endif //AK_SUPPORT_WCHAR

AKRESULT UnloadBank(
	    const char*         in_pszString,           // Name/path of the bank to load.
		const void *		in_pInMemoryBankPtr,
		AkBankCallbackFunc  in_pfnBankCallback,	    // Callback function.
		void *              in_pCookie 				// Callback cookie.
	    )
{
	AkBankID bankID = GetBankIDFromString( in_pszString );

    return LoadBankInternal(bankID,                 // ID of the bank to load.
							AkFileNameString(),			// no need to pass in the bank string when unloading.
							(in_pInMemoryBankPtr) ? AkBankLoadFlag_FromMemoryInPlace : AkBankLoadFlag_None,
                            CAkBankMgr::QueueItemUnload,
                            in_pfnBankCallback,	    // Callback function that will be called.
                            in_pCookie,             // Callback cookie.
							in_pInMemoryBankPtr
                            );
}

// Asynchronous bank unload (by id and Memory Ptr).
AKRESULT UnloadBank(
	    AkBankID            in_bankID,              // ID of the bank to unload.
		const void *		in_pInMemoryBankPtr,
		AkBankCallbackFunc  in_pfnBankCallback,	    // Callback function.
		void *              in_pCookie 				// Callback cookie.
	    )
{
    return LoadBankInternal( in_bankID,              // ID of the bank to load.
						     AkFileNameString(),
							 (in_pInMemoryBankPtr) ? AkBankLoadFlag_FromMemoryInPlace : AkBankLoadFlag_None,
							 CAkBankMgr::QueueItemUnload,
                             in_pfnBankCallback,	 // Callback function that will be called.
	                         in_pCookie,             // Callback cookie.
							 in_pInMemoryBankPtr
                             );
}

void CancelBankCallbackCookie(
		void * in_pCookie
		)
{
	if( g_pBankManager )
	{
		g_pBankManager->CancelCookie( in_pCookie );
	}
}

static AKRESULT PrepareBankInternal(
	PreparationType		in_PreparationType,					///< Preparation type ( Preparation_Load or Preparation_Unload )
	AkBankID            in_bankID,							///< ID of the bank to Prepare/Unprepare.
	AkFileNameString	in_strBankName,
	AkBankCallbackFunc	in_pfnBankCallback,					///< Callback function
	void *              in_pCookie,							///< Callback cookie (reserved to user, passed to the callback function)
	AkBankContent		in_uFlags /*= AkBankContent_All*/	///< Media, structure or both (Events are included in structural)
	)
{
	CAkBankMgr::AkBankQueueItem item;
	item.eType = (in_PreparationType == Preparation_Load || in_PreparationType == Preparation_LoadAndDecode )? CAkBankMgr::QueueItemPrepareBank : CAkBankMgr::QueueItemUnprepareBank;
	item.bankLoadFlag = AkBankLoadFlag_None;
	item.callbackInfo.pfnBankCallback = in_pfnBankCallback;
	item.callbackInfo.pCookie = in_pCookie;
	item.bankID = in_bankID;
	item.fileName = in_strBankName;
	item.bankPreparation.uFlags = in_uFlags;
	item.bankPreparation.bDecode = (in_PreparationType == Preparation_LoadAndDecode);

	return g_pBankManager->QueueBankCommand(item);
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT PrepareBank(
			PreparationType		in_PreparationType,					///< Preparation type ( Preparation_Load or Preparation_Unload )
			const wchar_t*      in_pszString,						///< Name of the bank to Prepare/Unprepare.
			AkBankContent		in_uFlags /*= AkBankContent_All*/	///< Media, structure or both (Events are included in structural)
			)
{
	AkFileNameString bankNameStr;
	if (bankNameStr.Copy(in_pszString) != AK_Success)
		return AK_InsufficientMemory;

	AkSyncCaller syncLoader;
	AKRESULT eResult = g_pBankManager->InitSyncOp(syncLoader);
	if (eResult != AK_Success)
		return eResult;

	AkBankID bankID = GetBankIDFromString(bankNameStr.Get());
	eResult = PrepareBankInternal(in_PreparationType, bankID, bankNameStr, g_pDefaultBankCallbackFunc, &syncLoader, in_uFlags);

	return g_pBankManager->WaitForSyncOp(syncLoader, eResult);
}
#endif //AK_SUPPORT_WCHAR

AKRESULT PrepareBank(
			PreparationType		in_PreparationType,					///< Preparation type ( Preparation_Load or Preparation_Unload )
			const char*         in_pszString,						///< Name of the bank to Prepare/Unprepare.
			AkBankContent		in_uFlags /*= AkBankContent_All*/	///< Media, structure or both (Events are included in structural)
			)
{
	AkFileNameString bankNameStr;
	if (bankNameStr.Copy(in_pszString) != AK_Success)
		return AK_InsufficientMemory;

	AkSyncCaller syncLoader;
	AKRESULT eResult = g_pBankManager->InitSyncOp(syncLoader);
	if (eResult != AK_Success)
		return eResult;

	AkBankID bankID = GetBankIDFromString( bankNameStr.Get() );

	eResult = PrepareBankInternal(in_PreparationType, bankID, bankNameStr, g_pDefaultBankCallbackFunc, &syncLoader, in_uFlags);

	return g_pBankManager->WaitForSyncOp(syncLoader, eResult);
}

AKRESULT PrepareBank(
			PreparationType		in_PreparationType,					///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkBankID            in_bankID,							///< ID of the bank to Prepare/Unprepare.
			AkBankContent		in_uFlags /*= AkBankContent_All*/	///< Media, structure or both (Events are included in structural)
			)
{
	AkSyncCaller syncLoader;
	AKRESULT eResult = g_pBankManager->InitSyncOp(syncLoader);
	if( eResult != AK_Success )
		return eResult;

	eResult = PrepareBankInternal(in_PreparationType, in_bankID, AkFileNameString(), g_pDefaultBankCallbackFunc, &syncLoader, in_uFlags);

	return g_pBankManager->WaitForSyncOp(syncLoader, eResult);
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT PrepareBank(
			PreparationType		in_PreparationType,					///< Preparation type ( Preparation_Load or Preparation_Unload )
			const wchar_t*      in_pszString,						///< Name of the bank to Prepare/Unprepare.
			AkBankCallbackFunc	in_pfnBankCallback,					///< Callback function
			void *              in_pCookie,							///< Callback cookie (reserved to user, passed to the callback function)
			AkBankContent		in_uFlags /*= AkBankContent_All*/	///< Media, structure or both (Events are included in structural)
			)
{
	AkFileNameString bankNameStr;
	if (bankNameStr.Copy(in_pszString) != AK_Success)
		return AK_InsufficientMemory;

	AkBankID bankID = GetBankIDFromString(bankNameStr.Get());
	return PrepareBankInternal(in_PreparationType, bankID, bankNameStr, in_pfnBankCallback, in_pCookie, in_uFlags);
}
#endif //AK_SUPPORT_WCHAR

AKRESULT PrepareBank(
			PreparationType		in_PreparationType,					///< Preparation type ( Preparation_Load or Preparation_Unload )
			const char*         in_pszString,						///< Name of the bank to Prepare/Unprepare.
			AkBankCallbackFunc	in_pfnBankCallback,					///< Callback function
			void *              in_pCookie,							///< Callback cookie (reserved to user, passed to the callback function)
			AkBankContent		in_uFlags /*= AkBankContent_All*/	///< Media, structure or both (Events are included in structural)
			)
{
	AkFileNameString bankNameStr;
	if (bankNameStr.Copy(in_pszString) != AK_Success)
		return AK_InsufficientMemory;

	AkBankID bankID = GetBankIDFromString(bankNameStr.Get());
	return PrepareBankInternal(in_PreparationType, bankID, bankNameStr, in_pfnBankCallback, in_pCookie, in_uFlags);
}

AKRESULT PrepareBank(
			PreparationType		in_PreparationType,					///< Preparation type ( Preparation_Load or Preparation_Unload )
			AkBankID            in_bankID,							///< ID of the bank to Prepare/Unprepare.
			AkBankCallbackFunc	in_pfnBankCallback,					///< Callback function
			void *              in_pCookie,							///< Callback cookie (reserved to user, passed to the callback function)
			AkBankContent		in_uFlags /*= AkBankContent_All*/	///< Media, structure or both (Events are included in structural)
			)
{
	return PrepareBankInternal(in_PreparationType, in_bankID, AkFileNameString(), in_pfnBankCallback, in_pCookie, in_uFlags);
}

// Synchronous PrepareEvent (by id).
AKRESULT PrepareEvent(
	PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
	AkUniqueID*			in_pEventID,			///< Array of Event ID
	AkUInt32			in_uNumEvent			///< number of Event ID in the array
	)
{
	AkSyncCaller syncLoader;
	AKRESULT eResult = g_pBankManager->InitSyncOp(syncLoader);
	if( eResult != AK_Success )
		return eResult;

	eResult = PrepareEventInternal( in_PreparationType, g_pDefaultBankCallbackFunc, &syncLoader, in_pEventID, in_uNumEvent );

	return g_pBankManager->WaitForSyncOp(syncLoader, eResult);
}

#ifdef AK_SUPPORT_WCHAR
// Synchronous PrepareEvent (by string).
AKRESULT PrepareEvent(
	PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
	const wchar_t**		in_ppszString,			///< Array of EventName
	AkUInt32			in_uNumEvent			///< Number of Events in the array
	)
{
	AKRESULT eResult;
	switch ( in_uNumEvent )
	{
	case 0:
		eResult = AK_InvalidParameter;
		break;

	case 1:
		{
			AkUniqueID eventID = GetIDFromString( in_ppszString[0] );
			eResult = PrepareEvent( in_PreparationType, &eventID, 1 );
		}
		break;

	default:
		{
			AkUInt32* pEventIDArray = (AkUInt32*)AkAlloc( AkMemID_Object, in_uNumEvent * sizeof( AkUInt32 ) );
			if( pEventIDArray )
			{
				for( AkUInt32 i = 0; i < in_uNumEvent; ++i )
				{
					pEventIDArray[i] = 	GetIDFromString( in_ppszString[i] );
				}
				AkSyncCaller syncLoader;
				eResult = g_pBankManager->InitSyncOp(syncLoader);
				if( eResult == AK_Success )
				{
					eResult = PrepareEventInternal( in_PreparationType, g_pDefaultBankCallbackFunc, &syncLoader, pEventIDArray, in_uNumEvent, false );
					eResult = g_pBankManager->WaitForSyncOp(syncLoader, eResult);
				}
			}
			else
			{
				eResult = AK_InsufficientMemory;
			}
		}
		break;
	}

	return eResult;
}
#endif //AK_SUPPORT_WCHAR

AKRESULT PrepareEvent(
	PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
	const char**		in_ppszString,			///< Array of EventName
	AkUInt32			in_uNumEvent			///< Number of Events in the array
	)
{
	AKRESULT eResult;
	switch ( in_uNumEvent )
	{
	case 0:
		eResult = AK_InvalidParameter;
		break;

	case 1:
		{
			AkUniqueID eventID = GetIDFromString( in_ppszString[0] );
			eResult = PrepareEvent( in_PreparationType, &eventID, 1 );
		}
		break;

	default:
		{
			AkUInt32* pEventIDArray = (AkUInt32*)AkAlloc( AkMemID_Object, in_uNumEvent * sizeof( AkUInt32 ) );
			if( pEventIDArray )
			{
				for( AkUInt32 i = 0; i < in_uNumEvent; ++i )
				{
					pEventIDArray[i] = 	GetIDFromString( in_ppszString[i] );
				}
				AkSyncCaller syncLoader;
				eResult = g_pBankManager->InitSyncOp(syncLoader);
				if( eResult == AK_Success )
				{
					eResult = PrepareEventInternal( in_PreparationType, g_pDefaultBankCallbackFunc, &syncLoader, pEventIDArray, in_uNumEvent, false );
					eResult = g_pBankManager->WaitForSyncOp(syncLoader, eResult);
				}
			}
			else
			{
				eResult = AK_InsufficientMemory;
			}
		}
		break;
	}

	return eResult;
}

// Asynchronous PrepareEvent (by id).
AKRESULT PrepareEvent(
	PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
	AkUniqueID*			in_pEventID,			///< Array of Event ID
	AkUInt32			in_uNumEvent,			///< number of Event ID in the array
	AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
	void *              in_pCookie				///< Callback cookie
	)
{
	return PrepareEventInternal( in_PreparationType, in_pfnBankCallback, in_pCookie, in_pEventID, in_uNumEvent );
}

#ifdef AK_SUPPORT_WCHAR
// Asynchronous PrepareEvent (by string).
AKRESULT PrepareEvent(
	PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
	const wchar_t**		in_ppszString,			///< Array of EventName
	AkUInt32			in_uNumEvent,			///< Number of Events in the array
	AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
	void *              in_pCookie				///< Callback cookie
	)
{
	AKRESULT eResult;
	switch ( in_uNumEvent )
	{
	case 0:
		eResult = AK_InvalidParameter;
		break;

	case 1:
		{
			AkUniqueID eventID = GetIDFromString( in_ppszString[0] );
			eResult = PrepareEventInternal( in_PreparationType, in_pfnBankCallback, in_pCookie, &eventID, 1 );
		}
		break;

	default:
		{
			AkUInt32* pEventIDArray = (AkUInt32*)AkAlloc( AkMemID_Object, in_uNumEvent * sizeof( AkUInt32 ) );
			if( pEventIDArray )
			{
				for( AkUInt32 i = 0; i < in_uNumEvent; ++i )
				{
					pEventIDArray[i] = 	GetIDFromString( in_ppszString[i] );
				}
				eResult = PrepareEventInternal( in_PreparationType, in_pfnBankCallback, in_pCookie, pEventIDArray, in_uNumEvent, false);
			}
			else
			{
				eResult = AK_InsufficientMemory;
			}
		}
		break;
	}

	return eResult;
}
#endif //AK_SUPPORT_WCHAR

AKRESULT PrepareEvent(
	PreparationType		in_PreparationType,		///< Preparation type ( Preparation_Load or Preparation_Unload )
	const char**		in_ppszString,			///< Array of EventName
	AkUInt32			in_uNumEvent,			///< Number of Events in the array
	AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
	void *              in_pCookie				///< Callback cookie
	)
{
	AKRESULT eResult;
	switch ( in_uNumEvent )
	{
	case 0:
		eResult = AK_InvalidParameter;
		break;

	case 1:
		{
			AkUniqueID eventID = GetIDFromString( in_ppszString[0] );
			eResult = PrepareEventInternal( in_PreparationType, in_pfnBankCallback, in_pCookie, &eventID, 1 );
		}
		break;

	default:
		{
			AkUInt32* pEventIDArray = (AkUInt32*)AkAlloc( AkMemID_Object, in_uNumEvent * sizeof( AkUInt32 ) );
			if( pEventIDArray )
			{
				for( AkUInt32 i = 0; i < in_uNumEvent; ++i )
				{
					pEventIDArray[i] = 	GetIDFromString( in_ppszString[i] );
				}
				eResult = PrepareEventInternal( in_PreparationType, in_pfnBankCallback, in_pCookie, pEventIDArray, in_uNumEvent, false);
			}
			else
			{
				eResult = AK_InsufficientMemory;
			}
		}
		break;
	}

	return eResult;
}

AKRESULT ClearPreparedEvents()
{
	AkSyncCaller syncLoader;
	AKRESULT eResult = g_pBankManager->InitSyncOp(syncLoader);
	if( eResult != AK_Success )
		return eResult;

	CAkBankMgr::AkBankQueueItem item;
	item.eType						 = CAkBankMgr::QueueItemUnprepareAllEvents;
	item.bankLoadFlag				= AkBankLoadFlag_None;
    item.callbackInfo.pfnBankCallback = g_pDefaultBankCallbackFunc;
    item.callbackInfo.pCookie		 = &syncLoader;

	eResult = g_pBankManager->QueueBankCommand( item );

	return g_pBankManager->WaitForSyncOp(syncLoader, eResult);
}

AKRESULT SetMedia(
	AkSourceSettings *	in_pSourceSettings,
	AkUInt32			in_uNumSourceSettings
	)
{
	if( in_pSourceSettings == NULL )
	{
		return AK_InvalidParameter;
	}

	if( in_uNumSourceSettings == 0 )
	{
		return AK_Success;
	}

	return g_pBankManager->AddMediaToTable(in_pSourceSettings, in_uNumSourceSettings);
}

AKRESULT UnsetMedia(
	AkSourceSettings *	in_pSourceSettings,
	AkUInt32			in_uNumSourceSettings
	)
{
	if( in_pSourceSettings == NULL )
	{
		return AK_InvalidParameter;
	}

	if( in_uNumSourceSettings == 0 )
	{
		return AK_Success;
	}

	return g_pBankManager->RemoveMediaFromTable(in_pSourceSettings, in_uNumSourceSettings);
}

/// Async IDs
AKRESULT PrepareGameSyncs(
	PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
	AkGroupType			in_eGameSyncType,		///< The type of game sync.
	AkUInt32			in_GroupID,				///< The state group ID or the Switch Group ID.
	AkUInt32*			in_paGameSyncID,		///< Array of ID of the gamesyncs to either support or not support.
	AkUInt32			in_uNumGameSyncs,		///< The number of game sync ID in the array.
	AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
	void *				in_pCookie				///< Callback cookie
	)
{
	return PrepareGameSyncsInternal( in_pfnBankCallback, in_pCookie, (in_PreparationType == Preparation_Load), in_eGameSyncType, in_GroupID, in_paGameSyncID, in_uNumGameSyncs );
}

#ifdef AK_SUPPORT_WCHAR
/// Async strings
AKRESULT PrepareGameSyncs(
	PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
	AkGroupType			in_eGameSyncType,		///< The type of game sync.
	const wchar_t*		in_pszGroupName,		///< The state group Name or the Switch Group Name.
	const wchar_t**		in_ppszGameSyncName,	///< The specific ID of the state to either support or not support.
	AkUInt32			in_uNumGameSyncNames,	///< The number of game sync in the string array.
	AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
	void *				in_pCookie				///< Callback cookie
	)
{
	AKRESULT eResult;

	if( !in_ppszGameSyncName || !in_uNumGameSyncNames )
		return AK_InvalidParameter;

	AkUInt32 groupID = GetIDFromString( in_pszGroupName );

	if ( in_uNumGameSyncNames == 1 )
	{
		AkUInt32 gameSyncID = GetIDFromString( in_ppszGameSyncName[0] );

		eResult = PrepareGameSyncsInternal( in_pfnBankCallback, in_pCookie, (in_PreparationType == Preparation_Load), in_eGameSyncType, groupID, &gameSyncID, 1 );
	}
	else
	{
		AkUInt32* pGameSyncIDArray = (AkUInt32*)AkAlloc( AkMemID_Object, in_uNumGameSyncNames * sizeof( AkUInt32 ) );
		if( pGameSyncIDArray )
		{
			for( AkUInt32 i = 0; i < in_uNumGameSyncNames; ++i )
			{
				pGameSyncIDArray[i] = 	GetIDFromString( in_ppszGameSyncName[i] );
			}
			eResult = PrepareGameSyncsInternal( in_pfnBankCallback, in_pCookie, (in_PreparationType == Preparation_Load), in_eGameSyncType, groupID, pGameSyncIDArray, in_uNumGameSyncNames, false );
		}
		else
		{
			eResult = AK_InsufficientMemory;
		}
	}

	return eResult;
}
#endif //AK_SUPPORT_WCHAR

AKRESULT PrepareGameSyncs(
	PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
	AkGroupType			in_eGameSyncType,		///< The type of game sync.
	const char*			in_pszGroupName,		///< The state group Name or the Switch Group Name.
	const char**		in_ppszGameSyncName,	///< The specific ID of the state to either support or not support.
	AkUInt32			in_uNumGameSyncNames,	///< The number of game sync in the string array.
	AkBankCallbackFunc	in_pfnBankCallback,		///< Callback function
	void *				in_pCookie				///< Callback cookie
	)
{
	AKRESULT eResult;

	if( !in_ppszGameSyncName || !in_uNumGameSyncNames )
		return AK_InvalidParameter;

	AkUInt32 groupID = GetIDFromString( in_pszGroupName );

	if ( in_uNumGameSyncNames == 1 )
	{
		AkUInt32 gameSyncID = GetIDFromString( in_ppszGameSyncName[0] );

		eResult = PrepareGameSyncsInternal( in_pfnBankCallback, in_pCookie, (in_PreparationType == Preparation_Load), in_eGameSyncType, groupID, &gameSyncID, 1 );
	}
	else
	{
		AkUInt32* pGameSyncIDArray = (AkUInt32*)AkAlloc( AkMemID_Object, in_uNumGameSyncNames * sizeof( AkUInt32 ) );
		if( pGameSyncIDArray )
		{
			for( AkUInt32 i = 0; i < in_uNumGameSyncNames; ++i )
			{
				pGameSyncIDArray[i] = 	GetIDFromString( in_ppszGameSyncName[i] );
			}
			eResult = PrepareGameSyncsInternal( in_pfnBankCallback, in_pCookie, (in_PreparationType == Preparation_Load), in_eGameSyncType, groupID, pGameSyncIDArray, in_uNumGameSyncNames, false );
		}
		else
		{
			eResult = AK_InsufficientMemory;
		}
	}

	return eResult;
}

/// Sync IDs
AKRESULT PrepareGameSyncs(
	PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
	AkGroupType		in_eGameSyncType,			///< The type of game sync.
	AkUInt32		in_GroupID,					///< The state group ID or the Switch Group ID.
	AkUInt32*		in_paGameSyncID,			///< Array of ID of the gamesyncs to either support or not support.
	AkUInt32		in_uNumGameSyncs			///< The number of game sync ID in the array.
	)
{
	AkSyncCaller syncLoader;
	AKRESULT eResult = g_pBankManager->InitSyncOp(syncLoader);
	if( eResult != AK_Success )
		return eResult;

	eResult = PrepareGameSyncsInternal( g_pDefaultBankCallbackFunc, &syncLoader, (in_PreparationType == Preparation_Load), in_eGameSyncType, in_GroupID, in_paGameSyncID, in_uNumGameSyncs );

	return g_pBankManager->WaitForSyncOp(syncLoader, eResult);
}

#ifdef AK_SUPPORT_WCHAR
/// Sync strings
AKRESULT PrepareGameSyncs(
	PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
	AkGroupType		in_eGameSyncType,			///< The type of game sync.
	const wchar_t*	in_pszGroupName,			///< The state group Name or the Switch Group Name.
	const wchar_t**	in_ppszGameSyncName,		///< The specific ID of the state to either support or not support.
	AkUInt32		in_uNumGameSyncNames		///< The number of game sync in the string array.
	)
{
	AKRESULT eResult;

	if( !in_ppszGameSyncName || !in_uNumGameSyncNames )
		return AK_InvalidParameter;

	AkUInt32 groupID = GetIDFromString( in_pszGroupName );

	if ( in_uNumGameSyncNames == 1 )
	{
		AkUInt32 gameSyncID = GetIDFromString( in_ppszGameSyncName[0] );

		eResult = PrepareGameSyncs( in_PreparationType, in_eGameSyncType, groupID, &gameSyncID, 1 );
	}
	else
	{
		AkUInt32* pGameSyncIDArray = (AkUInt32*)AkAlloc( AkMemID_Object, in_uNumGameSyncNames * sizeof( AkUInt32 ) );
		if( pGameSyncIDArray )
		{
			for( AkUInt32 i = 0; i < in_uNumGameSyncNames; ++i )
			{
				pGameSyncIDArray[i] = 	GetIDFromString( in_ppszGameSyncName[i] );
			}

			AkSyncCaller syncLoader;
			eResult = g_pBankManager->InitSyncOp(syncLoader);
			if( eResult == AK_Success )
			{
				eResult = PrepareGameSyncsInternal( g_pDefaultBankCallbackFunc, &syncLoader, (in_PreparationType == Preparation_Load), in_eGameSyncType, groupID, pGameSyncIDArray, in_uNumGameSyncNames, false );
				eResult = g_pBankManager->WaitForSyncOp(syncLoader, eResult);
			}
		}
		else
		{
			eResult = AK_InsufficientMemory;
		}
	}

	return eResult;
}
#endif //AK_SUPPORT_WCHAR

AKRESULT PrepareGameSyncs(
	PreparationType	in_PreparationType,			///< Preparation type ( Preparation_Load or Preparation_Unload )
	AkGroupType		in_eGameSyncType,			///< The type of game sync.
	const char*		in_pszGroupName,			///< The state group Name or the Switch Group Name.
	const char**	in_ppszGameSyncName,		///< The specific ID of the state to either support or not support.
	AkUInt32		in_uNumGameSyncNames		///< The number of game sync in the string array.
	)
{
	AKRESULT eResult;

	if( !in_ppszGameSyncName || !in_uNumGameSyncNames )
		return AK_InvalidParameter;

	AkUInt32 groupID = GetIDFromString( in_pszGroupName );

	if ( in_uNumGameSyncNames == 1 )
	{
		AkUInt32 gameSyncID = GetIDFromString( in_ppszGameSyncName[0] );

		eResult = PrepareGameSyncs( in_PreparationType, in_eGameSyncType, groupID, &gameSyncID, 1 );
	}
	else
	{
		AkUInt32* pGameSyncIDArray = (AkUInt32*)AkAlloc( AkMemID_Object, in_uNumGameSyncNames * sizeof( AkUInt32 ) );
		if( pGameSyncIDArray )
		{
			for( AkUInt32 i = 0; i < in_uNumGameSyncNames; ++i )
			{
				pGameSyncIDArray[i] = 	GetIDFromString( in_ppszGameSyncName[i] );
			}

			AkSyncCaller syncLoader;
			eResult = g_pBankManager->InitSyncOp(syncLoader);
			if( eResult == AK_Success )
			{
				eResult = PrepareGameSyncsInternal( g_pDefaultBankCallbackFunc, &syncLoader, (in_PreparationType == Preparation_Load), in_eGameSyncType, groupID, pGameSyncIDArray, in_uNumGameSyncNames, false );
				eResult = g_pBankManager->WaitForSyncOp(syncLoader, eResult);
			}
		}
		else
		{
			eResult = AK_InsufficientMemory;
		}
	}

	return eResult;
}

////////////////////////////////////////////////////////////////////////////
// Banks new API END.
////////////////////////////////////////////////////////////////////////////

#ifndef PROXYCENTRAL_CONNECTED
#ifdef AK_SUPPORT_WCHAR
AKRESULT LoadMediaFileSync( AkUniqueID in_MediaID, const wchar_t* in_szFileName, LoadMediaFile_ActionType in_eActionType )
{
	AkSyncCaller syncLoader;
	AKRESULT eResult = g_pBankManager->InitSyncOp(syncLoader);
	if( eResult != AK_Success )
		return eResult;

    CAkBankMgr::AkBankQueueItem item;
	switch( in_eActionType )
	{
	case LoadMediaFile_Load:
		item.eType = CAkBankMgr::QueueItemLoadMediaFile;
		break;
	case LoadMediaFile_Unload:
		item.eType = CAkBankMgr::QueueItemUnloadMediaFile;
		break;
	case LoadMediaFile_Swap:
		item.eType = CAkBankMgr::QueueItemLoadMediaFileSwap;
		break;
	}

	item.bankLoadFlag						= AkBankLoadFlag_None;
	item.callbackInfo.pCookie				= &syncLoader;
	item.callbackInfo.pfnBankCallback		= g_pDefaultBankCallbackFunc;
	item.loadMediaFile.MediaID				= in_MediaID;

	if( in_eActionType == LoadMediaFile_Unload )
	{
		// No string attached, simplier.
		eResult = g_pBankManager->QueueBankCommand( item );
	}
	else
	{
		if( item.fileName.Copy(in_szFileName,NULL/*<-do not force a particular file extension*/) == AK_Success )
		{
			eResult = g_pBankManager->QueueBankCommand( item );
		}
		else
		{
			eResult = AK_InsufficientMemory;
		}
	}

	return g_pBankManager->WaitForSyncOp(syncLoader, eResult);
}
#endif //AK_SUPPORT_WCHAR

AKRESULT LoadMediaFileSync( AkUniqueID in_MediaID, const char* in_szFileName, LoadMediaFile_ActionType in_eActionType )
{
	AkSyncCaller syncLoader;
	AKRESULT eResult = g_pBankManager->InitSyncOp(syncLoader);
	if( eResult != AK_Success )
		return eResult;

    CAkBankMgr::AkBankQueueItem item;
	switch( in_eActionType )
	{
	case LoadMediaFile_Load:
		item.eType = CAkBankMgr::QueueItemLoadMediaFile;
		break;
	case LoadMediaFile_Unload:
		item.eType = CAkBankMgr::QueueItemUnloadMediaFile;
		break;
	case LoadMediaFile_Swap:
		item.eType = CAkBankMgr::QueueItemLoadMediaFileSwap;
		break;
	}

	item.bankLoadFlag						= AkBankLoadFlag_None;
	item.callbackInfo.pCookie				= &syncLoader;
	item.callbackInfo.pfnBankCallback		= g_pDefaultBankCallbackFunc;
	item.loadMediaFile.MediaID				= in_MediaID;

	if( in_eActionType == LoadMediaFile_Unload )
	{
		// No string attached, simplier.
		eResult = g_pBankManager->QueueBankCommand( item );
	}
	else
	{
		if( item.fileName.Copy(in_szFileName,NULL/*<-do not force a particular file extension*/) == AK_Success )
		{
			eResult = g_pBankManager->QueueBankCommand( item );
		}
		else
		{
			eResult = AK_InsufficientMemory;
		}
	}

	return g_pBankManager->WaitForSyncOp(syncLoader, eResult);
}
#endif //PROXYCENTRAL_CONNECTED

#ifdef AK_SUPPORT_WCHAR
AKRESULT SetBusEffect(
			const wchar_t* in_pszBusName,
			AkUInt32 in_uFXIndex,
			AkUniqueID in_shareSetID
		                )
{
	AkTriggerID l_busID = GetIDFromString( in_pszBusName );

	if( l_busID != AK_INVALID_UNIQUE_ID  )
	{
		return SetBusEffect( l_busID, in_uFXIndex, in_shareSetID );
	}
	else
	{
		return AK_IDNotFound;
	}
}
#endif //AK_SUPPORT_WCHAR

AKRESULT SetBusEffect(
			const char* in_pszBusName,
			AkUInt32 in_uFXIndex,
			AkUniqueID in_shareSetID
		                )
{
	AkTriggerID l_busID = GetIDFromString( in_pszBusName );

	if( l_busID != AK_INVALID_UNIQUE_ID  )
	{
		return SetBusEffect( l_busID, in_uFXIndex, in_shareSetID );
	}
	else
	{
		return AK_IDNotFound;
	}
}

AKRESULT SetBusEffect(
	AkUniqueID in_audioNodeID,
	AkUInt32 in_uFXIndex,
	AkUniqueID in_shareSetID )
{
	AKASSERT( in_audioNodeID != AK_INVALID_UNIQUE_ID );
	AKASSERT( in_uFXIndex < AK_NUM_EFFECTS_PER_OBJ );

	AutoReserveMsg pItem( QueuedMsgType_SetEffect, AkQueuedMsg::Sizeof_SetEffect() );
	pItem->setEffect.audioNodeID = in_audioNodeID;
	pItem->setEffect.uFXIndex = in_uFXIndex;
	pItem->setEffect.shareSetID = in_shareSetID;
	pItem->setEffect.eNodeType = AkNodeType_Bus;

	return AK_Success;
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT SetMixer(
			const wchar_t* in_pszBusName,
			AkUniqueID in_shareSetID
		                )
{
	AkTriggerID l_busID = GetIDFromString( in_pszBusName );

	if( l_busID != AK_INVALID_UNIQUE_ID  )
	{
		return SetMixer( l_busID, in_shareSetID );
	}
	else
	{
		return AK_IDNotFound;
	}
}
#endif //AK_SUPPORT_WCHAR

AKRESULT SetMixer(
			const char* in_pszBusName,
			AkUniqueID in_shareSetID
		                )
{
	AkTriggerID l_busID = GetIDFromString( in_pszBusName );

	if( l_busID != AK_INVALID_UNIQUE_ID  )
	{
		return SetMixer( l_busID, in_shareSetID );
	}
	else
	{
		return AK_IDNotFound;
	}
}

AKRESULT SetMixer(
	AkUniqueID in_audioNodeID,
	AkUniqueID in_shareSetID )
{
	AKASSERT( in_audioNodeID != AK_INVALID_UNIQUE_ID );

	AutoReserveMsg pItem( QueuedMsgType_SetMixer, AkQueuedMsg::Sizeof_SetEffect() );
	pItem->setEffect.audioNodeID = in_audioNodeID;
	pItem->setEffect.shareSetID = in_shareSetID;
	pItem->setEffect.eNodeType = AkNodeType_Bus;

	return AK_Success;
}

AKRESULT SetBusConfig(
	AkUniqueID in_audioNodeID,					///< Bus Short ID.
	AkChannelConfig in_channelConfig			///< Desired channel configuration. An invalid configuration (from default constructor) means "as parent".
	)
{
	AKASSERT(in_audioNodeID != AK_INVALID_UNIQUE_ID);

	AutoReserveMsg pItem(QueuedMsgType_SetBusConfig, AkQueuedMsg::Sizeof_SetBusConfig());
	pItem->setBusConfig.audioNodeID = in_audioNodeID;
	pItem->setBusConfig.uChannelConfig = in_channelConfig.Serialize();

	return AK_Success;
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT SetBusConfig(
	const wchar_t* in_pszBusName,
	AkChannelConfig in_channelConfig			///< Desired channel configuration. An invalid configuration (from default constructor) means "as parent".
	)
{
	AkUniqueID l_busID = GetIDFromString(in_pszBusName);

	if (l_busID != AK_INVALID_UNIQUE_ID)
	{
		return SetBusConfig(l_busID, in_channelConfig);
	}
	else
	{
		return AK_IDNotFound;
	}
}
#endif //AK_SUPPORT_WCHAR

AKRESULT SetBusConfig(
	const char* in_pszBusName,
	AkChannelConfig in_channelConfig			///< Desired channel configuration. An invalid configuration (from default constructor) means "as parent".
	)
{
	AkUniqueID l_busID = GetIDFromString(in_pszBusName);

	if (l_busID != AK_INVALID_UNIQUE_ID)
	{
		return SetBusConfig(l_busID, in_channelConfig);
	}
	else
	{
		return AK_IDNotFound;
	}
}


AKRESULT SetActorMixerEffect(
	AkUniqueID in_audioNodeID,
	AkUInt32 in_uFXIndex,
	AkUniqueID in_shareSetID )
{
	AKASSERT( in_audioNodeID != AK_INVALID_UNIQUE_ID );
	AKASSERT( in_uFXIndex < AK_NUM_EFFECTS_PER_OBJ );

	AutoReserveMsg pItem( QueuedMsgType_SetEffect, AkQueuedMsg::Sizeof_SetEffect() );
	pItem->setEffect.audioNodeID = in_audioNodeID;
	pItem->setEffect.uFXIndex = in_uFXIndex;
	pItem->setEffect.shareSetID = in_shareSetID;
	pItem->setEffect.eNodeType = AkNodeType_Default;

	return AK_Success;
}

////////////////////////////////////////////////////////////////////////////
// Behavioral extensions registration
////////////////////////////////////////////////////////////////////////////

AKRESULT RegisterGlobalCallback(
	AkGlobalCallbackFunc in_pCallback,				// Function to register as a global callback.
	AkUInt32 /*AkGlobalCallbackLocation*/ in_eLocation,	// Callback location. Bitwise OR multiple locations if needed.
	void * in_pCookie,								// User cookie.
	AkPluginType in_eType,							// Plug-in type (for example, source or effect). AkPluginTypeNone for no timing.
	AkUInt32 in_ulCompanyID,						// Company identifier (as declared in the plug-in description XML file). 0 for no timing.
	AkUInt32 in_ulPluginID							// Plug-in identifier (as declared in the plug-in description XML file). 0 for no timing.
	)
{
	CAkFunctionCritical GlobalLock;

	if (in_eLocation >= (1 << AkGlobalCallbackLocation_Num)
		|| !in_pCallback)
		return AK_InvalidParameter;

	// Register callback in each location.
	AKRESULT eResult = AK_Success;
	AkUInt32 uLocation = 0;

	AkPluginID FXID = AKMAKECLASSID( in_eType, in_ulCompanyID, in_ulPluginID );
	while (((AkUInt32)(1 << uLocation) <= in_eLocation)
		&& eResult == AK_Success)
	{
		AkUInt32 uTempLocation = ( 1 << uLocation ) & in_eLocation;
		if (uTempLocation)
		{
			// Don't register timer for Reg and Term locations
			AkPluginID tempFXID = ( uTempLocation == AkGlobalCallbackLocation_Register || uTempLocation == AkGlobalCallbackLocation_Term ) ? 0 : FXID;

			AkGlobalCallbackRecord callback(in_pCallback, in_pCookie, tempFXID );
			eResult = g_aBehavioralExtensions[uLocation].AddLast(callback) ? AK_Success : AK_InsufficientMemory;
		}
		++uLocation;
	}

	if (eResult == AK_Success
		&& (in_eLocation & AkGlobalCallbackLocation_Register)
		&& in_pCallback != NULL)
	{
		in_pCallback(AK::SoundEngine::GetGlobalPluginContext(), AkGlobalCallbackLocation_Register, in_pCookie);
	}

	return eResult;
}

AKRESULT UnregisterGlobalCallback(
	AkGlobalCallbackFunc in_pCallback,				// Function to unregister as a global callback.
	AkUInt32 /*AkGlobalCallbackLocation*/ in_eLocation // Must match in_eLocation as passed to RegisterGlobalCallback for this callback.
	)
{
	CAkFunctionCritical GlobalLock;

	if (in_eLocation >= (1 << AkGlobalCallbackLocation_Num))
		return AK_InvalidParameter;

	AKRESULT eResult = AK_Success;
	AkUInt32 uLocation = 0;
	while ((AkUInt32)(1 << uLocation) <= in_eLocation)
	{
		if ((1 << uLocation) & in_eLocation)
		{
			AkGlobalCallbackRecord callback(in_pCallback);	// Ignore cookie

			// Do the .Remove() search here so the timer assocaiated with that location can be unregistered as well
			BehavioralExtensionArray::Iterator it = g_aBehavioralExtensions[uLocation].FindEx( callback );
			if (it != g_aBehavioralExtensions[uLocation].End())
			{
				g_aBehavioralExtensions[uLocation].Erase( it );
			}
			else
			{
				eResult = AK_InvalidParameter;
			}

		}
		++uLocation;
	}

	return eResult;
}

AKRESULT RegisterResourceMonitorCallback(
	AkResourceMonitorCallbackFunc in_pCallback				// Function to register as a resource monitor callback.
)
{
#ifndef AK_OPTIMIZED
	return AkResourceMonitor::Register(in_pCallback);
#endif
	return AK_Fail;
}

AKRESULT UnregisterResourceMonitorCallback(
	AkResourceMonitorCallbackFunc in_pCallback				// Function to unregister as a resource monitor callback.
)
{
#ifndef AK_OPTIMIZED
	return AkResourceMonitor::Unregister(in_pCallback);
#endif
	return AK_Fail;
}

void AddExternalStateHandler(
    AkExternalStateHandlerCallback in_pCallback
    )
{
    g_pExternalStateHandler = in_pCallback;
}
void AddExternalBankHandler(
	AkExternalBankHandlerCallback in_pCallback
	)
{
	g_pExternalBankHandlerCallback = in_pCallback;
}
void AddExternalProfileHandler(
	AkExternalProfileHandlerCallback in_pCallback
	)
{
	g_pExternalProfileHandlerCallback = in_pCallback;
}

void AddExternalUnloadBankHandler(AkExternalUnloadBankHandlerCallback in_pCallback)
{
	g_pExternalUnloadBankHandlerCallback = in_pCallback;
}

///////////////////////////////////////////////////////////////////////////
// Output Capture
///////////////////////////////////////////////////////////////////////////

AKRESULT StartOutputCapture( const AkOSChar* in_CaptureFileName )
{
	if (in_CaptureFileName == NULL)
		return AK_InvalidParameter;

	size_t uStrLen = AKPLATFORM::OsStrLen(in_CaptureFileName) + 1;
	AkOSChar *szFileName = (AkOSChar*)AkAlloc( AkMemID_Object, uStrLen * sizeof(AkOSChar) );
	if (szFileName == NULL)
		return AK_InsufficientMemory;

	AutoReserveMsg pItem(QueuedMsgType_StartStopOutputCapture, AkQueuedMsg::Sizeof_StartStopCapture() );

	pItem->outputCapture.szFileName = szFileName;
	memcpy( (void*)pItem->outputCapture.szFileName, (void*)in_CaptureFileName, uStrLen * sizeof(AkOSChar));


	return AK_Success;
}

AKRESULT StopOutputCapture()
{
	AutoReserveMsg pItem(QueuedMsgType_StartStopOutputCapture, AkQueuedMsg::Sizeof_StartStopCapture() );
	pItem->outputCapture.szFileName = NULL;	//Setting no file name signals the end.

	return  AK_Success;
}

AKRESULT AddOutputCaptureMarker(
	const char* in_MarkerText					///< Text of the marker
	)
{
	if (g_pAudioMgr == NULL)
		return AK_Fail;

	// We don't insert empty markers
	if(*in_MarkerText == 0)
		return AK_InvalidParameter;

	AkQueuedMsg Item( QueuedMsgType_AddOutputCaptureMarker );

	size_t uStrLen = strlen(in_MarkerText) + 1; // Getting the length including the null-terminating character
	char *szMarkerText = (char*)AkAlloc( AkMemID_Object, uStrLen * sizeof(char) );
	if(szMarkerText == NULL)
		return AK_InsufficientMemory;

	// Copying marker text to our buffer
	memcpy( szMarkerText, in_MarkerText, uStrLen * sizeof(char));

	// Send task to the message queue
	AutoReserveMsg pItem(QueuedMsgType_AddOutputCaptureMarker, AkQueuedMsg::Sizeof_AddOutputCaptureMarker() );
	pItem->captureMarker.szMarkerText = szMarkerText;

	return AK_Success;
}

AKRESULT StartProfilerCapture(
	const AkOSChar* in_CaptureFileName				///< Name of the output profiler file
	)
{
#ifdef AK_OPTIMIZED
	return AK_NotCompatible;
#else
	if (in_CaptureFileName == NULL)
		return AK_InvalidParameter;
	
	if(AkMonitor::Get())
		return AkMonitor::Get()->StartProfilerCapture(in_CaptureFileName);
	
	return AK_Success;
#endif
}

AKRESULT StopProfilerCapture()
{
#ifdef AK_OPTIMIZED
	return AK_NotCompatible;
#else
	if (AkMonitor::Get())
		return AkMonitor::Get()->StopProfilerCapture();

	return AK_Success;
#endif
}

//====================================================================================================
// Internal INITS
//====================================================================================================
AKRESULT PreInit(AkInitSettings * io_pSettings)
{
	// make sure you get rid of things in the reverse order
	// you created them

	AKRESULT eResult = AK_Success;

#define CHECK_PREINIT_FAILURE {if( eResult != AK_Success ) goto preinit_failure;}

#define CHECK_ALLOCATION_PREINIT_FAILURE( _IN_PTR_ ) {if( _IN_PTR_ == NULL ) {eResult = AK_InsufficientMemory; goto preinit_failure;}}

#ifndef AK_OPTIMIZED
	CAkParameterNodeBase::ResetMonitoringMuteSolo();
	CAkRegistryMgr::ResetMonitoringMuteSolo();
	AkMonitor * pMonitor = AkMonitor::Instance();
	CHECK_ALLOCATION_PREINIT_FAILURE(pMonitor);

	eResult = pMonitor->StartMonitoring();
	CHECK_PREINIT_FAILURE;
#endif

	//IMPORTANT : g_pIndex MUST STAY SECOND CREATION OF AKINIT()!!!!!!!!!!!!!!!!!!
	if (!g_pIndex)
	{
		g_pIndex = AkNew(AkMemID_SoundEngine, CAkAudioLibIndex());
		CHECK_ALLOCATION_PREINIT_FAILURE(g_pIndex);

		if(!g_pIndex->Init())
			return AK_InsufficientMemory;
	}

	CHECK_PREINIT_FAILURE;

	if (!g_pRTPCMgr)
	{
		g_pRTPCMgr = AkNew(AkMemID_SoundEngine, CAkRTPCMgr());
		CHECK_ALLOCATION_PREINIT_FAILURE(g_pRTPCMgr);

		eResult = g_pRTPCMgr->Init();
		CHECK_PREINIT_FAILURE;
	}

	if (!g_pEnvironmentMgr)
	{
		g_pEnvironmentMgr = AkNew(AkMemID_SoundEngine, CAkEnvironmentsMgr());
		CHECK_ALLOCATION_PREINIT_FAILURE(g_pEnvironmentMgr);

		eResult = g_pEnvironmentMgr->Init();
		CHECK_PREINIT_FAILURE;
	}

	if (!g_pBankManager)
	{
		if (g_settings.bUseSoundBankMgrThread)
		{
			g_pDefaultBankCallbackFunc = DefaultBankCallbackFunc;
			g_pBankManager = AkNew(AkMemID_SoundEngine, CAkThreadedBankMgr());
		}
		else
		{
			g_pDefaultBankCallbackFunc = NULL;
			g_pBankManager = AkNew(AkMemID_SoundEngine, CAkBankMgr());
		}

		CHECK_ALLOCATION_PREINIT_FAILURE(g_pBankManager);

		eResult = g_pBankManager->Init();
		CHECK_PREINIT_FAILURE;
	}

	if (!g_pPlayingMgr)
	{
		g_pPlayingMgr = AkNew(AkMemID_SoundEngine, CAkPlayingMgr());
		CHECK_ALLOCATION_PREINIT_FAILURE(g_pPlayingMgr);

		eResult = g_pPlayingMgr->Init();
		CHECK_PREINIT_FAILURE;
	}

	if (!g_pPositionRepository)
	{
		g_pPositionRepository = AkNew(AkMemID_SoundEngine, CAkPositionRepository());
		CHECK_ALLOCATION_PREINIT_FAILURE(g_pPositionRepository);

		eResult = g_pPositionRepository->Init();
		CHECK_PREINIT_FAILURE;
	}

	if (!g_pRegistryMgr)
	{
		g_pRegistryMgr = AkNew(AkMemID_SoundEngine, CAkRegistryMgr());
		CHECK_ALLOCATION_PREINIT_FAILURE(g_pRegistryMgr);

		eResult = g_pRegistryMgr->Init();
		CHECK_PREINIT_FAILURE;
	}

	if (!g_pModulatorMgr)
	{
		g_pModulatorMgr = AkNew(AkMemID_SoundEngine, CAkModulatorMgr());
		CHECK_ALLOCATION_PREINIT_FAILURE(g_pModulatorMgr);

		eResult = g_pModulatorMgr->Init();
		CHECK_PREINIT_FAILURE;
	}

	// this one has to be ready before StateMgr is
	if (!g_pTransitionManager)
	{
		g_pTransitionManager = AkNew(AkMemID_SoundEngine, CAkTransitionManager());
		CHECK_ALLOCATION_PREINIT_FAILURE(g_pTransitionManager);
	}

	// this one needs math and transitions to be ready
	if (!g_pPathManager)
	{
		g_pPathManager = AkNew(AkMemID_SoundEngine, CAkPathManager());
		CHECK_ALLOCATION_PREINIT_FAILURE(g_pPathManager);

		eResult = g_pPathManager->Init(g_settings.uMaxNumPaths);
		CHECK_PREINIT_FAILURE;
	}

	if (!g_pSwitchMgr)
	{
		g_pSwitchMgr = AkNew(AkMemID_SoundEngine, CAkSwitchMgr());
		CHECK_ALLOCATION_PREINIT_FAILURE(g_pSwitchMgr);

		eResult = g_pSwitchMgr->Init();
		CHECK_PREINIT_FAILURE;
	}

	if (!g_pStateMgr)
	{
		g_pStateMgr = AkNew(AkMemID_SoundEngine, CAkStateMgr());
		CHECK_ALLOCATION_PREINIT_FAILURE(g_pStateMgr);

		eResult = g_pStateMgr->Init();
		CHECK_PREINIT_FAILURE;
	}

preinit_failure:

    // Update client settings with (possibly) modified values.
    if ( io_pSettings )
        *io_pSettings = g_settings;

    return eResult;
}

AKRESULT InitRenderer()
{
	AKRESULT eResult = CAkURenderer::Init();

	if (!g_pAudioMgr && eResult == AK_Success )
	{
		g_pAudioMgr = AkNew(AkMemID_SoundEngine,CAkAudioMgr());
		if ( g_pAudioMgr )
		{
			eResult = g_pAudioMgr->Init();
			
			//Initialise the timers for performance measurement.
			AK_INIT_TIMERS();
			AK_PERF_INIT();
			
			if( eResult == AK_Success )
				eResult = g_pAudioMgr->Start();
		}
		else
		{
			eResult = AK_InsufficientMemory;
		}
	}
	return eResult;
}

void StopAll( AkGameObjectID in_gameObjectID /*= AK_INVALID_GAME_OBJECT*/ )
{
	AutoReserveMsg pItem(QueuedMsgType_StopAll, AkQueuedMsg::Sizeof_StopAll() );
	pItem->stopAll.gameObjID = in_gameObjectID;

}

void StopPlayingID(
				   AkPlayingID in_playingID,
				   AkTimeMs in_uTransitionDuration /*= 0*/,
				   AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/
				   )
{
	// WG-20735, must handle invalid ID.
	if( in_playingID == AK_INVALID_PLAYING_ID )
		return;

	AKASSERT(g_pIndex);
	AKASSERT(g_pAudioMgr);

	AutoReserveMsg pItem(QueuedMsgType_StopPlayingID, AkQueuedMsg::Sizeof_StopPlayingID() );

	pItem->stopEvent.playingID = in_playingID;
	pItem->stopEvent.uTransitionDuration = in_uTransitionDuration;
	pItem->stopEvent.eFadeCurve = in_eFadeCurve;

}

void ExecuteActionOnPlayingID(
	AkActionOnEventType in_ActionType,
	AkPlayingID in_playingID,
	AkTimeMs in_uTransitionDuration /*= 0*/,
	AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/
	)
{
	// WG-20735, must handle invalid ID.
	if (in_playingID == AK_INVALID_PLAYING_ID)
		return;

	AKASSERT(g_pIndex);
	AKASSERT(g_pAudioMgr);

	AutoReserveMsg pItem(QueuedMsgType_PlayingIDAction, AkQueuedMsg::Sizeof_PlayingIDAction());

	pItem->playingIDAction.eActionToExecute = in_ActionType;
	pItem->playingIDAction.playingID = in_playingID;
	pItem->playingIDAction.uTransitionDuration = in_uTransitionDuration;
	pItem->playingIDAction.eFadeCurve = in_eFadeCurve;
}

#ifndef AK_OPTIMIZED

CAkIndexable* GetIndexable( AkUniqueID in_IndexableID, AkIndexType in_eIndexType )
{
	CAkIndexable* pReturnedIndexable = NULL;

	//Ensure the Index Was initialized
	AKASSERT(g_pIndex);

	switch(in_eIndexType)
	{
	case AkIdxType_AudioNode:
		pReturnedIndexable = g_pIndex->GetNodePtrAndAddRef(in_IndexableID, AkNodeType_Default);
		break;
	case AkIdxType_BusNode:
		pReturnedIndexable = g_pIndex->GetNodePtrAndAddRef(in_IndexableID, AkNodeType_Bus);
		break;
	case AkIdxType_Action:
		pReturnedIndexable = g_pIndex->m_idxActions.GetPtrAndAddRef(in_IndexableID);
		break;
	case AkIdxType_Event:
		pReturnedIndexable = g_pIndex->m_idxEvents.GetPtrAndAddRef(in_IndexableID);
		break;
	case AkIdxType_DialogueEvent:
		pReturnedIndexable = g_pIndex->m_idxDialogueEvents.GetPtrAndAddRef(in_IndexableID);
		break;
	case AkIdxType_Layer:
		pReturnedIndexable = g_pIndex->m_idxLayers.GetPtrAndAddRef(in_IndexableID);
		break;
	case AkIdxType_Attenuation:
		pReturnedIndexable = g_pIndex->m_idxAttenuations.GetPtrAndAddRef(in_IndexableID);
		break;
	case AkIdxType_DynamicSequence:
		pReturnedIndexable = g_pIndex->m_idxDynamicSequences.GetPtrAndAddRef(in_IndexableID);
		break;
	case AkIdxType_FxShareSet:
		pReturnedIndexable = g_pIndex->m_idxFxShareSets.GetPtrAndAddRef(in_IndexableID);
		break;
	case AkIdxType_FxCustom:
		pReturnedIndexable = g_pIndex->m_idxFxCustom.GetPtrAndAddRef(in_IndexableID);
		break;
	case AkIdxType_AudioDevice:
		pReturnedIndexable = g_pIndex->m_idxAudioDevices.GetPtrAndAddRef(in_IndexableID);
		break;
	case AkIdxType_CustomState:
		pReturnedIndexable = g_pIndex->m_idxCustomStates.GetPtrAndAddRef( in_IndexableID );
		break;
	case AkIdxType_Modulator:
		pReturnedIndexable = g_pIndex->m_idxModulators.GetPtrAndAddRef( in_IndexableID );
		break;
	case AkIdxType_VirtualAcoustics:
		pReturnedIndexable = g_pIndex->m_idxVirtualAcoustics.GetPtrAndAddRef(in_IndexableID);
		break;
	default:
		AKASSERT(!"Invalid Index Type");
		break;
	}

	return pReturnedIndexable;
}

#endif // AK_OPTIMIZED

AkOutputDeviceID GetOutputID(AkUniqueID in_idShareset, AkUInt32 in_uDevice)
{	
	AkOutputDeviceID ret = AK_MAKE_DEVICE_KEY(in_idShareset, in_uDevice);
	if (in_idShareset == 0 && in_uDevice == 0 && IsInitialized())
	{
		//Shortcut to get the Main output.  This is the only output we don't give the ID when adding during init.
		g_csMain.Lock();
		AkDevice* pDevice = CAkOutputMgr::GetDevice(0);				
		if (pDevice)
			ret = pDevice->uDeviceID;
		g_csMain.Unlock();
	}
	return ret;
}


AkOutputDeviceID GetOutputID(const char* in_idShareset, AkUInt32 in_uDevice)
{
	return GetOutputID(AK::SoundEngine::GetIDFromString(in_idShareset), in_uDevice);
}

#ifdef AK_SUPPORT_WCHAR
AkOutputDeviceID GetOutputID(const wchar_t*  in_idShareset, AkUInt32 in_uDevice)
{
	return GetOutputID(AK::SoundEngine::GetIDFromString(in_idShareset), in_uDevice);
}
#endif

AKRESULT AddOutput(const AkOutputSettings & in_settings, AkOutputDeviceID * out_pOutputID, const AkGameObjectID* in_pListenerIDs, AkUInt32 in_uNumListeners)
{
	// Error management.  Get the plugin.  If it is one of the built ins, make sure the user provided a proper id.
	AkPluginID idPlugin = AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_DEFAULT_SINK);
	if(in_settings.audioDeviceShareset != AK_INVALID_UNIQUE_ID)
	{
		CAkAudioDevice* pDevice = g_pIndex->m_idxAudioDevices.GetPtrAndAddRef(in_settings.audioDeviceShareset);
		if(!pDevice)
		{
			MONITOR_ERROREX(AK::Monitor::ErrorCode_AudioDeviceShareSetNotFound, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, in_settings.audioDeviceShareset, false);
			return AK_IDNotFound;
		}

		idPlugin = pDevice->GetFXID();
		pDevice->Release();
	}	
	
	if(out_pOutputID)
		*out_pOutputID = AK_MAKE_DEVICE_KEY(in_settings.audioDeviceShareset, in_settings.idDevice);
#ifdef AK_WIN	
	if(idPlugin == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_DEFAULT_SINK))
	{		
		//Validate there is an output with this id
		if(!CheckDeviceID(in_settings.idDevice))
		{
			MONITOR_ERROREX(AK::Monitor::ErrorCode_AudioDeviceNotValid, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, in_settings.idDevice, false);
			return AK_DeviceNotReady;
		}		
	}
#endif

#ifdef AK_XBOX
	if(idPlugin == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_PERSONAL_SINK) && 
		(in_settings.idDevice == 0 || in_settings.idDevice == 0xFFFFFFFF))
	{
		MONITOR_ERROREX(AK::Monitor::ErrorCode_AudioDeviceNotValid, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, in_settings.idDevice, false);
		return AK_InvalidParameter;
	}
#elif defined(AK_SONY)
	if((idPlugin == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_PAD_SINK) ||
		idPlugin == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_PERSONAL_SINK) ||
		idPlugin == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_VOICE_SINK)) &&
		 in_settings.idDevice == 0)
	{
		MONITOR_ERROREX(AK::Monitor::ErrorCode_AudioDeviceNotValid, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, in_settings.idDevice, false);
		return AK_InvalidParameter;
	}
#endif
	//REVIEW SWITCH???	

	AkUInt32 uAllocSize = AkQueuedMsg::Sizeof_ListenerIDMsg<AkQueuedMsg_AddOutput>(in_uNumListeners);
	if (uAllocSize > g_pAudioMgr->GetMaximumMsgSize())
	{
		MONITOR_ERRORMSG(AKTEXT("AK::SoundEngine::AddOutput() - Too many game objects in array."));
		return AK_InvalidParameter;
	}

	AkQueuedMsg* pItem = g_pAudioMgr->ReserveQueue(QueuedMsgType_AddOutput, uAllocSize);
	if (pItem == NULL)
		return AK_InvalidParameter;	
	
	pItem->addOutput.SetListeners(in_pListenerIDs, in_uNumListeners, AkListenerOp_Set);
	memcpy(pItem->addOutput.pSettings, &in_settings, sizeof(AkOutputSettings));	

	g_pAudioMgr->FinishQueueWrite();

	return AK_Success;
}

AKRESULT RemoveOutput(AkOutputDeviceID in_idOutput)
{
	AutoReserveMsg pItem(QueuedMsgType_RemoveOutput, AkQueuedMsg::Sizeof_ListenerIDMsg<AkQueuedMsg_RemoveOutput>(0));
	pItem->removeOutput.idOutput = in_idOutput;	

	return AK_Success;
}

AKRESULT SetOutputVolume(AkOutputDeviceID in_idOutput, AkReal32 in_fVolume)
{
#ifdef CHECK_SOUND_ENGINE_INPUT_VALID
	if ( !(AkMath::IsValidFloatInput( in_fVolume ) ) )
		HANDLE_INVALID_FLOAT_INPUT(AKTEXT("AK::SoundEngine::SetOutputVolume : Invalid Float in in_fVolume"));
#endif

	AutoReserveMsg pItem( QueuedMsgType_SetOutputVolume, AkQueuedMsg::Sizeof_SetOutputVolume() );	
	pItem->setoutputvolume.idDevice = in_idOutput;
	pItem->setoutputvolume.fVolume = in_fVolume;

	return AK_Success;
}

AKRESULT SetBusDevice(AkUniqueID in_idBus, AkUniqueID in_idNewDevice)
{
	CAkParameterNodeBase * pBus = g_pIndex->GetNodeIndex(AkNodeType_Bus).GetPtrAndAddRef(in_idBus);
	if(!pBus)
	{
		char msg[100];
		sprintf(msg, "Bus ID %i not found in Init bank.", in_idBus);
		MONITOR_ERRORMSG(msg);
		return AK_IDNotFound;
	}
	bool bTop = pBus->Parent() == NULL;	
	pBus->Release();

	if(!bTop)
	{
		MONITOR_ERRORMSG("Can't call AK::SoundEngine::SetBusDevice on child bus.  Use on Master busses only.");
		return AK_InvalidParameter;
	}	

	CAkAudioDevice* pDevice = g_pIndex->m_idxAudioDevices.GetPtrAndAddRef(in_idNewDevice);
	if(!pDevice)
	{
		char msg[100];
		sprintf(msg, "Audio ShareSet ID %i not found in Init bank.", in_idBus);
		MONITOR_ERRORMSG(msg);
		return AK_IDNotFound;
	}
	pDevice->Release();

	AutoReserveMsg pItem(QueuedMsgType_SetBusDevice, AkQueuedMsg::Sizeof_SetBusDevice());
	pItem->setBusDevice.idBus = in_idBus;
	pItem->setBusDevice.idDevice = in_idNewDevice;
	return AK_Success;
}

AKRESULT SetBusDevice(const char* in_szBus, const char* in_szNewDevice)
{
	AkUniqueID idBus = GetIDFromString(in_szBus);
	AkUniqueID idDevice = GetIDFromString(in_szNewDevice);
	CAkParameterNodeBase * pBus = g_pIndex->GetNodeIndex(AkNodeType_Bus).GetPtrAndAddRef(idBus);
	if(!pBus)
	{
		char msg[100];
		sprintf(msg, "Bus %s not found in Init bank.", in_szBus);
		MONITOR_ERRORMSG(msg);
		return AK_IDNotFound;
	}
	bool bTop = pBus->Parent() == NULL;
	pBus->Release();

	if(!bTop)
	{
		MONITOR_ERRORMSG("Can't call AK::SoundEngine::SetBusDevice on child bus.  Use on Master busses only.");
		return AK_InvalidParameter;
	}

	CAkAudioDevice* pDevice = g_pIndex->m_idxAudioDevices.GetPtrAndAddRef(idDevice);
	if(!pDevice)
	{
		char msg[100];
		sprintf(msg, "Audio ShareSet ID %s not found in Init bank.", in_szNewDevice);
		MONITOR_ERRORMSG(msg);
		return AK_IDNotFound;
	}
	pDevice->Release();

	AutoReserveMsg pItem(QueuedMsgType_SetBusDevice, AkQueuedMsg::Sizeof_SetBusDevice());
	pItem->setBusDevice.idBus = idBus;
	pItem->setBusDevice.idDevice = idDevice;
	return AK_Success;	
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT SetBusDevice(const wchar_t* in_szBus, const wchar_t* in_szNewDevice)
{
	AkUniqueID idBus = GetIDFromString(in_szBus);
	AkUniqueID idDevice = GetIDFromString(in_szNewDevice);
	CAkParameterNodeBase * pBus = g_pIndex->GetNodeIndex(AkNodeType_Bus).GetPtrAndAddRef(idBus);
	if(!pBus)
	{
		AkUInt32 len = 1 + (AkUInt32)wcslen(in_szBus);
		char *busName = (char*)AkAlloca(len*4);
		AKPLATFORM::AkWideCharToChar(in_szBus, len, busName);
		char msg[100];
		sprintf(msg, "Bus %s not found in Init bank.", busName);
		MONITOR_ERRORMSG(msg);
		return AK_IDNotFound;
	}
	bool bTop = pBus->Parent() == NULL;
	pBus->Release();

	if(!bTop)
	{
		MONITOR_ERRORMSG("Can't call AK::SoundEngine::SetBusDevice on child bus.  Use on Master busses only.");
		return AK_InvalidParameter;
	}

	CAkAudioDevice* pDevice = g_pIndex->m_idxAudioDevices.GetPtrAndAddRef(idDevice);
	if(!pDevice)
	{
		AkUInt32 len = 1 + (AkUInt32)wcslen(in_szBus);
		char *deviceName = (char*)AkAlloca(len*4);
		AKPLATFORM::AkWideCharToChar(in_szNewDevice, len, deviceName);
		char msg[100];
		sprintf(msg, "Audio Device ID %s not found in Init bank.", deviceName);
		MONITOR_ERRORMSG(msg);
		return AK_IDNotFound;
	}
	pDevice->Release();

	AutoReserveMsg pItem(QueuedMsgType_SetBusDevice, AkQueuedMsg::Sizeof_SetBusDevice());
	pItem->setBusDevice.idBus = idBus;
	pItem->setBusDevice.idDevice = idDevice;
	return AK_Success;
}
#endif

AKRESULT GetSinkPluginDeviceList(
	AkUInt32 ulPluginID,
	AkUInt32& io_maxNumDevices,
	AkDeviceDescription* out_deviceDescriptions)
{
	CAkEffectsMgr::EffectTypeRecord* pPluginFactory = CAkEffectsMgr::GetEffectTypeRecord(ulPluginID);
	if (pPluginFactory == NULL)
	{
		return AK_PluginNotRegistered;
	}

	if (pPluginFactory->pGetDeviceListFunc == NULL)
	{
		return AK_NotImplemented;
	}

	return (AKRESULT)(*pPluginFactory->pGetDeviceListFunc)(io_maxNumDevices, out_deviceDescriptions);
}

AKRESULT GetDeviceList(
	AkUInt32 in_ulCompanyID,
	AkUInt32 in_ulPluginID,
	AkUInt32& io_maxNumDevices,
	AkDeviceDescription* out_deviceDescriptions)
{
	AKRESULT res = AK_Fail;

	AkUInt32 ulPluginID = CAkEffectsMgr::GetMergedID(AkPluginTypeSink, in_ulCompanyID, in_ulPluginID);

	if ((in_ulCompanyID == AKCOMPANYID_AUDIOKINETIC) &&
		(AKPLUGINID_DEFAULT_SINK <= in_ulPluginID && in_ulPluginID < AKPLUGINID_DUMMY_SINK))
	{
		// System sinks
		res = CAkLEngine::GetPlatformDeviceList(in_ulPluginID, io_maxNumDevices, out_deviceDescriptions);
	}
	else if ((in_ulCompanyID == AKCOMPANYID_AUDIOKINETIC) && (in_ulPluginID == AKPLUGINID_DUMMY_SINK))
	{
		// Dummy sink always only supports default device ID
		res = AK_NotImplemented;
	}
	else if (CAkEffectsMgr::IsPluginRegistered(ulPluginID))
	{
		// Custom sink plug-in (registered)
		res = GetSinkPluginDeviceList(ulPluginID, io_maxNumDevices, out_deviceDescriptions);
	}
	else
	{
		// Custom sink plug-in (not registered) or not found
		res = AK_PluginNotRegistered;
	}

	if (res == AK_PluginNotRegistered)
	{
		MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_PluginNotRegistered, ulPluginID, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, AK_INVALID_UNIQUE_ID, false);
	}

	if (res != AK_Success)
	{
		io_maxNumDevices = 0;
	}

	return res;
}

AKRESULT GetDeviceList(
	AkUInt32 in_audioDeviceShareSetID,
	AkUInt32& io_maxNumDevices,
	AkDeviceDescription* out_deviceDescriptions)
{
	// Find plug-in ID
	if (in_audioDeviceShareSetID == AK_INVALID_UNIQUE_ID)
	{
		MONITOR_ERROREX(AK::Monitor::ErrorCode_AudioDeviceShareSetNotFound, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, in_audioDeviceShareSetID, false);
		return AK_InvalidID;
	}

	// Check to see if the shareset provided exists
	AkPluginID idPlugin = AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_DEFAULT_SINK);
	{
		CAkAudioDevice* pAudioDevice = g_pIndex->m_idxAudioDevices.GetPtrAndAddRef(in_audioDeviceShareSetID);
		if (!pAudioDevice)
		{
			MONITOR_ERROREX(AK::Monitor::ErrorCode_AudioDeviceShareSetNotFound, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, in_audioDeviceShareSetID, false);
			return AK_IDNotFound;
		}
		idPlugin = pAudioDevice->GetFXID();
		pAudioDevice->Release();
	}
	
	return GetDeviceList(AKGETCOMPANYIDFROMCLASSID(idPlugin), AKGETPLUGINIDFROMCLASSID(idPlugin), io_maxNumDevices, out_deviceDescriptions);
}

AKRESULT ReplaceOutput(const AkOutputSettings & in_settings, AkOutputDeviceID in_outputDeviceId, AkOutputDeviceID * out_pOutputDeviceId)
{
	if (in_settings.audioDeviceShareset == AK_INVALID_UNIQUE_ID)
	{
		MONITOR_ERROREX(AK::Monitor::ErrorCode_AudioDeviceShareSetNotFound, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, in_settings.audioDeviceShareset, false);
		return AK_InvalidID;
	}

	// check to see if the shareset provided exists
	AkPluginID idPlugin = AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_DEFAULT_SINK);
	{
		CAkAudioDevice* pAudioDevice = g_pIndex->m_idxAudioDevices.GetPtrAndAddRef(in_settings.audioDeviceShareset);
		if (!pAudioDevice)
		{
			MONITOR_ERROREX(AK::Monitor::ErrorCode_AudioDeviceShareSetNotFound, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, in_settings.audioDeviceShareset, false);
			return AK_IDNotFound;
		}
		idPlugin = pAudioDevice->GetFXID();
		pAudioDevice->Release();
	}

	// check to see if in_outputID maps to an active device
	{
		g_csMain.Lock();
		AkDevice* pDevice = CAkOutputMgr::GetDevice(in_outputDeviceId);
		g_csMain.Unlock();
		if (pDevice == NULL)
		{
			MONITOR_ERROREX(AK::Monitor::ErrorCode_AudioDeviceNotFound, AK_INVALID_PLAYING_ID, in_outputDeviceId, AK_INVALID_UNIQUE_ID, false);
			return AK_DeviceNotFound;
		}
	}

#if defined AK_WIN	
	if (idPlugin == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_DEFAULT_SINK))
	{
		//Validate there is an output with this id
		if (!CheckDeviceID(in_settings.idDevice))
		{
			MONITOR_ERROREX(AK::Monitor::ErrorCode_AudioDeviceNotValid, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, in_settings.idDevice, false);
			return AK_DeviceNotReady;
		}
	}
#elif defined AK_XBOX
	if (idPlugin == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_PERSONAL_SINK) &&
		(in_settings.idDevice == 0 || in_settings.idDevice == 0xFFFFFFFF))
	{
		MONITOR_ERROREX(AK::Monitor::ErrorCode_AudioDeviceNotValid, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, in_settings.idDevice, false);
		return AK_InvalidParameter;
	}
#elif defined AK_SONY
	if ((idPlugin == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_PAD_SINK) ||
		idPlugin == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_PERSONAL_SINK) ||
		idPlugin == AKMAKECLASSID(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_VOICE_SINK)) &&
		in_settings.idDevice == 0)
	{
		MONITOR_ERROREX(AK::Monitor::ErrorCode_AudioDeviceNotValid, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, in_settings.idDevice, false);
		return AK_InvalidParameter;
	}
#endif

	if (out_pOutputDeviceId)
	{
		*out_pOutputDeviceId = AK_MAKE_DEVICE_KEY(in_settings.audioDeviceShareset, in_settings.idDevice);
	}

	AutoReserveMsg pItem(QueuedMsgType_ReplaceOutput, AkQueuedMsg::Sizeof_ReplaceOutput());
	memcpy(pItem->replaceOutput.pSettings, &in_settings, sizeof(AkOutputSettings));
	pItem->replaceOutput.idOutput = in_outputDeviceId;

	return AK_Success;
}

void SetRandomSeed( AkUInt32 in_uSeed )
{
	AutoReserveMsg pItem( QueuedMsgType_SetRandomSeed, AkQueuedMsg::Sizeof_SetRandomSeed() );
	pItem->randomSeed.uSeed = in_uSeed;
}

void MuteBackgroundMusic (bool in_bMute)
{
	AutoReserveMsg pItem( QueuedMsgType_MuteBackgroundMusic, AkQueuedMsg::Sizeof_MuteBackgroundMusic() );
	pItem->muteBGM.bMute = in_bMute;
}

bool GetBackgroundMusicMute()
{
	return CAkBus::IsBackgroundMusicMuted();
}

AKRESULT SendPluginCustomGameData(
	AkUniqueID in_busID, 
	AkGameObjectID in_busObjectID, 
	AkPluginType in_eType,
	AkUInt32 in_uCompanyID,
	AkUInt32 in_uPluginID,
	const void* in_pData, 
	AkUInt32 in_uSizeInBytes)
{
	AKRESULT res = AK_Success;
	if (in_uSizeInBytes == 0)
	{
		// Data will be cleaned up
		AutoReserveMsg pItem(QueuedMsgType_PluginCustomGameData, AkQueuedMsg::Sizeof_PluginCustomGameData());
		pItem->pluginCustomGameData.busID = in_busID;
		pItem->pluginCustomGameData.busObjectID = in_busObjectID;
		pItem->pluginCustomGameData.eType = in_eType;
		pItem->pluginCustomGameData.uCompanyID = in_uCompanyID;
		pItem->pluginCustomGameData.uPluginID = in_uPluginID;
		pItem->pluginCustomGameData.pData = NULL;
		pItem->pluginCustomGameData.uSizeInBytes = in_uSizeInBytes;
		return res;
	}

	void* pDataInternalCopy = AkAlloc( AkMemID_Processing, in_uSizeInBytes );

	if (pDataInternalCopy != NULL)
	{
		AutoReserveMsg pItem(QueuedMsgType_PluginCustomGameData, AkQueuedMsg::Sizeof_PluginCustomGameData());

		AKPLATFORM::AkMemCpy(pDataInternalCopy, in_pData, in_uSizeInBytes);
		pItem->pluginCustomGameData.busID = in_busID;
		pItem->pluginCustomGameData.busObjectID = in_busObjectID;
		pItem->pluginCustomGameData.eType = in_eType;
		pItem->pluginCustomGameData.uCompanyID = in_uCompanyID;
		pItem->pluginCustomGameData.uPluginID = in_uPluginID;
		pItem->pluginCustomGameData.pData = pDataInternalCopy;
		pItem->pluginCustomGameData.uSizeInBytes = in_uSizeInBytes;
	}
	else
	{
		res = AK_InsufficientMemory;
	}

	return res;
}

AKRESULT Suspend(bool in_bRender)
{
    return g_pAudioMgr->SuspendWakeup(true, in_bRender, 0);
}

AKRESULT WakeupFromSuspend()
{
	return g_pAudioMgr->SuspendWakeup(false, true, 0);
}

AkUInt32 GetBufferTick()
{
	return g_pAudioMgr->GetBufferTick();
}

AKRESULT SetCustomPlatformName(char* in_pCustomPlatformName)
{
	if (g_pszCustomPlatformName != NULL)
	{
		// Was already set, simply validate it is the same.
		if (strcmp(in_pCustomPlatformName, g_pszCustomPlatformName))
		{
			// Two subsystem tried to set different custom platform name, this is not allowed.
			// Can be caused by imcompatible banks or connecting with the wrong platform version.
			return AK_InvalidCustomPlatformName;
		}
		else
		{
			// same and already set, is fine.
			return AK_Success;
		}
	}

	g_pszCustomPlatformName = (char*)AkAlloc(AkMemID_SoundEngine, strlen(in_pCustomPlatformName)+1);
	if (g_pszCustomPlatformName == NULL)
		return AK_InsufficientMemory;

	strcpy(g_pszCustomPlatformName, in_pCustomPlatformName);

	return AK_Success;
}

AKRESULT RegisterAudioDeviceStatusCallback(AK::AkDeviceStatusCallbackFunc in_pCallback)
{
	CAkOutputMgr::SetSinkCallback(in_pCallback);
	return AK_Success;
}

AKRESULT UnregisterAudioDeviceStatusCallback()
{	
	CAkOutputMgr::SetSinkCallback(NULL);
	return AK_Success;
}

} // namespace SoundEngine


namespace Monitor
{

AKRESULT PostCode(
	ErrorCode in_eErrorCode,
	ErrorLevel in_eErrorLevel,
	AkPlayingID in_playingID,
	AkGameObjectID in_gameObjID,
	AkUniqueID in_audioNodeID,
	bool in_bIsBus
	)
{
#ifndef AK_OPTIMIZED
	if ( AkMonitor::Get() )
		AkMonitor::Monitor_PostCode(in_eErrorCode, in_eErrorLevel, in_playingID, in_gameObjID, in_audioNodeID, in_bIsBus);

	return AK_Success;
#else
	return AK_NotCompatible;
#endif
}

#ifdef AK_SUPPORT_WCHAR
AKRESULT PostString(
	const wchar_t* in_pszError,
	ErrorLevel in_eErrorLevel,
	AkPlayingID in_playingID,
	AkGameObjectID in_gameObjID,
	AkUniqueID in_audioNodeID,
	bool in_bIsBus
	)
{
#ifndef AK_OPTIMIZED
	if ( AkMonitor::Get() )
		AkMonitor::Monitor_PostString(in_pszError, in_eErrorLevel, in_playingID, in_gameObjID, in_audioNodeID, in_bIsBus);

	return AK_Success;
#else
	return AK_NotCompatible;
#endif
}
#endif //AK_SUPPORT_WCHAR

AKRESULT PostString(
	const char* in_pszError,
	ErrorLevel in_eErrorLevel,
	AkPlayingID in_playingID,
	AkGameObjectID in_gameObjID,
	AkUniqueID in_audioNodeID,
	bool in_bIsBus
	)
{
#ifndef AK_OPTIMIZED
	if ( AkMonitor::Get() )
		AkMonitor::Monitor_PostString(in_pszError, in_eErrorLevel, in_playingID, in_gameObjID, in_audioNodeID, in_bIsBus);

	return AK_Success;
#else
	return AK_NotCompatible;
#endif
}

AKRESULT SetLocalOutput(
	AkUInt32 in_uErrorLevel,
	LocalOutputFunc in_pMonitorFunc
	)
{
#ifndef AK_OPTIMIZED
	AkMonitor::SetLocalOutput( in_uErrorLevel, in_pMonitorFunc );
	return AK_Success;
#else
	return AK_NotCompatible;
#endif
}

AkTimeMs GetTimeStamp()
{
#ifndef AK_OPTIMIZED
	return AkMonitor::GetThreadTime();
#else
	return 0;
#endif
}

} // namespace Monitor

} // namespace AK


AkExternalSourceArray* AkExternalSourceArray::Create(AkUInt32 in_nCount, AkExternalSourceInfo* in_pSrcs)
{
	AkUInt32 iSize = (in_nCount - 1/*One is already in the struct*/) * sizeof(AkExternalSourceInfo) + sizeof(AkExternalSourceArray);
	AkExternalSourceArray* pArray = (AkExternalSourceArray *)AkAlloc(AkMemID_Object, iSize);
	if (AK_EXPECT_FALSE(pArray == NULL))
		return NULL;

	pArray->m_cRefCount = 1;
	pArray->m_nCount = in_nCount;

	for(AkUInt32 i = 0; i < in_nCount; ++i)
	{
		AkExternalSourceInfo& rInfo = in_pSrcs[i];
		AkExternalSourceInfo& rCopy = pArray->m_pSrcs[i];

		rCopy = rInfo;

		if (rInfo.szFile != NULL)
		{
			//Copy the paths.
			AkUInt32 len = (AkUInt32)(AKPLATFORM::OsStrLen(rInfo.szFile) + 1) * sizeof(AkOSChar);
			rCopy.szFile = (AkOSChar*)AkAlloc(AkMemID_Object, len);
			if (rCopy.szFile != NULL)
			{
				memcpy((void*)rCopy.szFile, rInfo.szFile, len);
			}
			else
			{
				//Free everything allocated up to now
				pArray->m_nCount = i;
				pArray->Release();
				return NULL;
			}
		}
	}

	return pArray;
}

void AkExternalSourceArray::Release()
{
	m_cRefCount--;
	if (m_cRefCount == 0)
	{
		for(AkUInt32 i = 0; i < m_nCount; i++)
		{
			if (m_pSrcs[i].szFile != NULL)
				AkFree(AkMemID_Object, (void*)m_pSrcs[i].szFile);
		}

		AkFree(AkMemID_Object, (void*)this);
	}
}
