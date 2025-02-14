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
// Implementation of the IAkLEngine interface. Contains code that is
// common to multiple platforms.
//
//   - Software pipeline-specific code can be found in
//     SoftwarePipeline/AkLEngine_SoftwarePipeline.cpp
//   - Platform-specific code can be found in <Platform>/AkLEngine.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "AkLEngine.h"
#include "Ak3DListener.h"
#include "AkFXMemAlloc.h"
#include "AkOutputMgr.h"
#include "AkPipelineID.h"
#include "AkEffectsMgr.h"

class CAkSink;

//-----------------------------------------------------------------------------
//Lower engine global singletons.
//-----------------------------------------------------------------------------
AkPlatformInitSettings  g_PDSettings;
extern AkInitSettings	g_settings; 
extern CAkLock g_csMain;

//-----------------------------------------------------------------------------
//Static variables.
//-----------------------------------------------------------------------------
AkUInt32					CAkLEngine::m_uLastStarvationTime = 0;
AkOSChar*					CAkLEngine::m_szPluginPath = NULL;

AkPipelineID AkPipelineIDGenerator::m_pipelineID = AK_INVALID_PIPELINE_ID + 1;

void CAkLEngine::GetDefaultPlatformThreadInitSettings(
	AkPlatformInitSettings & out_pPlatformSettings // Platform specific settings. Can be changed depending on hardware.
)
{
	AkGetDefaultThreadProperties(out_pPlatformSettings.threadBankManager);
	AkGetDefaultHighPriorityThreadProperties(out_pPlatformSettings.threadLEngine);
	AkGetDefaultHighPriorityThreadProperties(out_pPlatformSettings.threadOutputMgr);
	AkGetDefaultHighPriorityThreadProperties(out_pPlatformSettings.threadMonitor);
}

#ifndef AK_WIN
void CAkLEngine::OnThreadStart()
{
}

void CAkLEngine::OnThreadEnd()
{
	CAkOutputMgr::Term();
}
#endif

//-----------------------------------------------------------------------------
// Name: ApplyGlobalSettings
// Desc: Stores global settings in global variable g_PDSettings.
//
// Parameters: AkPlatformInitSettings * io_pPDSettings 
//-----------------------------------------------------------------------------
void CAkLEngine::ApplyGlobalSettings( AkPlatformInitSettings *   io_pPDSettings )
{
	// Settings.
	if ( io_pPDSettings == NULL )
	{
		GetDefaultPlatformInitSettings( g_PDSettings );
	}
	else
	{
		g_PDSettings = *io_pPDSettings;

#if defined( AK_MIN_NUM_REFILLS_IN_VOICE_BUFFER )
		if( g_PDSettings.uNumRefillsInVoice < AK_MIN_NUM_REFILLS_IN_VOICE_BUFFER )
			g_PDSettings.uNumRefillsInVoice = AK_DEFAULT_NUM_REFILLS_IN_VOICE_BUFFER;
#endif

		// Update client settings with actual values (might have changed due to hardware constraints).
		*io_pPDSettings = g_PDSettings;
	}
}

//-----------------------------------------------------------------------------
// Name: VPLDestroySource
// Desc: Destroy a specified source.
//
// Parameters:
//	CAkVPLSrcCbxNode * in_pCbx : Pointer to source to stop.
//-----------------------------------------------------------------------------
void CAkLEngine::VPLDestroySource(CAkVPLSrcCbxNode * in_pCbx, bool in_bNotify)
{
	if (in_bNotify && in_pCbx->GetContext())
	{
		MONITOR_SOURCE_ERROR(AK::Monitor::ErrorCode_CannotPlaySource_Create, in_pCbx->GetPBI());
	}

	in_pCbx->Term();

	// Free the context.
	AkDelete( AkMemID_Processing, in_pCbx );
} // VPLDestroySource

void CAkLEngine::SetPanningRule(
	AkOutputDeviceID	in_idOutput,
	AkPanningRule		in_panningRule 
	)
{
	// Set panning rule on default device.
	AkDevice * pDevice = CAkOutputMgr::GetDevice(in_idOutput);
	if ( pDevice )
	{
		pDevice->ePanningRule = in_panningRule;
		CAkListener::ResetListenerData();
	}
}

void CAkLEngine::GetDefaultOutputSettingsCommon( AkOutputSettings & out_settings )
{
	// For backward compatibility: by default, platforms that don't support rear speakers use 
	// "headphones" panning while others use "speaker" panning. 
#if defined AK_REARCHANNELS && !defined AK_ANDROID
	out_settings.ePanningRule = AkPanningRule_Speakers;
#else
	out_settings.ePanningRule = AkPanningRule_Headphones;
#endif

	out_settings.channelConfig.Clear();

	out_settings.audioDeviceShareset = AK_INVALID_UNIQUE_ID;
	out_settings.idDevice = AK_INVALID_UNIQUE_ID;
}

AkUInt32 CAkLEngine::GetNumBufferNeededAndSubmit()
{
	return CAkOutputMgr::IsDataNeeded();		
}


/// Get the DLLs full path, whether it is user-specified or using the one in InitSettings. 
/// If no path is set, the plug-in's full file name (including extension) is returned
void CAkLEngine::GetPluginDLLFullPath(AkOSChar* out_DllFullPath, AkUInt32 in_MaxPathLength, const AkOSChar* in_DllName, const AkOSChar* in_DllPath /* = NULL */)
{
	if (in_DllPath != NULL)
	{
		SafeStrCpy(out_DllFullPath, in_DllPath, in_MaxPathLength - 1);

		// Ensure given path had trailing separator
		size_t len = AKPLATFORM::OsStrLen(out_DllFullPath);
		if (out_DllFullPath[len - 1] != AK_PATH_SEPARATOR[0])
		{
			out_DllFullPath[len] = AK_PATH_SEPARATOR[0];
			out_DllFullPath[len + 1] = 0;
		}
	}
	else if (CAkEffectsMgr::GetPluginDLLPath() != NULL)
	{
		SafeStrCpy(out_DllFullPath, CAkEffectsMgr::GetPluginDLLPath(), in_MaxPathLength - 1);
	}

	AKPLATFORM::SafeStrCat(out_DllFullPath, AK_LIBRARY_PREFIX, in_MaxPathLength - 1);
	AKPLATFORM::SafeStrCat(out_DllFullPath, in_DllName, in_MaxPathLength - 1);
	AKPLATFORM::SafeStrCat(out_DllFullPath, AK_DYNAMIC_LIBRARY_EXTENSION, in_MaxPathLength - 1);
	out_DllFullPath[1023] = 0;
}