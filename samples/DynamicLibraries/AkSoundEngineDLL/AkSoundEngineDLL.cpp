/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

  Version: v2019.2.5  Build: 7349
  Copyright (c) 2006-2020 Audiokinetic Inc.
*******************************************************************************/

#include "stdafx.h"
#include "AkSoundEngineDLL.h"
#include <AK/Tools/Common/AkAssert.h>
#include <malloc.h>

#include <AK/SoundEngine/Common/AkMemoryMgr.h>
#include <AK/SoundEngine/Common/IAkStreamMgr.h>
#include "AkDefaultIOHookBlocking.h"
#ifndef AK_OPTIMIZED
#include <AK/Comm/AkCommunication.h>
#endif

#include <AK/Plugin/AkVorbisDecoderFactory.h>
#include <AK/Plugin/AkMeterFXFactory.h>

// Defines.

namespace AK
{
	namespace SOUNDENGINE_DLL
    {
        CAkDefaultIOHookBlocking m_lowLevelIO;

        //-----------------------------------------------------------------------------------------
        // Sound Engine initialization.
        //-----------------------------------------------------------------------------------------
        AKRESULT Init( 
            AkMemSettings *     in_pMemSettings,
            AkStreamMgrSettings * in_pStmSettings,
            AkDeviceSettings *  in_pDefaultDeviceSettings,
            AkInitSettings *    in_pSettings,
            AkPlatformInitSettings * in_pPlatformSettings,
			AkMusicSettings *	in_pMusicSettings
            )
        {
            // Check required arguments.
            if ( !in_pMemSettings ||
                 !in_pStmSettings ||
                 !in_pDefaultDeviceSettings )
            {
                AKASSERT( !"Invalid arguments" );
                return AK_InvalidParameter;
            }

            // Create and initialise an instance of our memory manager.
            if ( MemoryMgr::Init( in_pMemSettings ) != AK_Success )
            {
                AKASSERT( !"Could not create the memory manager." );
                return AK_Fail;
            }

            // Create and initialise an instance of the default stream manager.
			if ( !StreamMgr::Create( *in_pStmSettings ) )
            {
                AKASSERT( !"Could not create the Stream Manager" );
                return AK_Fail;
            }

            // Create an IO device.
			if ( m_lowLevelIO.Init( *in_pDefaultDeviceSettings ) != AK_Success )
            {
                AKASSERT( !"Cannot create streaming I/O device" );
                return AK_Fail;
            }
            
			// Initialize sound engine.
			if ( SoundEngine::Init( in_pSettings, in_pPlatformSettings ) != AK_Success )
            {
                AKASSERT( !"Cannot initialize sound engine" );
                return AK_Fail;
            }

			// Initialize music engine.
			if ( MusicEngine::Init( in_pMusicSettings ) != AK_Success )
            {
                AKASSERT( !"Cannot initialize music engine" );
                return AK_Fail;
            }

#ifndef AK_OPTIMIZED
			// Initialize communication.
			AkCommSettings settingsComm;
			AK::Comm::GetDefaultInitSettings( settingsComm );
			if ( AK::Comm::Init( settingsComm ) != AK_Success )
			{
				AKASSERT( !"Cannot initialize communication" );
				return AK_Fail;
			}
#endif // AK_OPTIMIZED

#ifdef AK_APPLE
			AK::SoundEngine::RegisterCodec(
				AKCOMPANYID_AUDIOKINETIC,
				AKCODECID_AAC,
				CreateAACFilePlugin,
				CreateAACBankPlugin);
#endif	
		
			return AK_Success;
        }

        //-----------------------------------------------------------------------------------------
        // Sound Engine termination.
        //-----------------------------------------------------------------------------------------
        void Term( )
        {
#ifndef AK_OPTIMIZED
			Comm::Term();
#endif // AK_OPTIMIZED

			MusicEngine::Term();

			SoundEngine::Term();

			m_lowLevelIO.Term();
            if ( IAkStreamMgr::Get() )
            	IAkStreamMgr::Get()->Destroy();
            	
	        MemoryMgr::Term();
        }
      
        //-----------------------------------------------------------------------------------------
        // Sound Engine periodic update.
        //-----------------------------------------------------------------------------------------
		void Tick( )
		{
			SoundEngine::RenderAudio( );
		}

        //-----------------------------------------------------------------------------------------
        // Access to LowLevelIO's file localization.
        //-----------------------------------------------------------------------------------------
        // File system interface.
		AKRESULT SetBasePath(
			const AkOSChar*   in_pszBasePath
			)
		{
			return m_lowLevelIO.SetBasePath( in_pszBasePath );
		}		
	}	
}

#if defined AK_XBOX
BOOL APIENTRY DllMain( HANDLE hModule, 
	DWORD  ul_reason_for_call, 
	LPVOID lpReserved )
{
	switch ( ul_reason_for_call )
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;

	}
	return TRUE;
}
#endif