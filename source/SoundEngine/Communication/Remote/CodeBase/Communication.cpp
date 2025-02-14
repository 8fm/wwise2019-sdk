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


#include "stdafx.h"

#include "ALBytesMem.h"

#include <AK/Comm/AkCommunication.h>
#include <AK/SoundEngine/Common/AkSoundEngine.h>

#include "CommunicationCentral.h"

#include "IProxyFrameworkConnected.h"

#include "AkCritical.h"

#ifndef AK_OPTIMIZED
	extern AK::Comm::ICommunicationCentral * g_pCommCentral;
#endif

namespace AK
{
	namespace Comm
	{
#ifndef AK_OPTIMIZED
		static AK::Comm::IProxyFrameworkConnected *	s_pProxyFramework = NULL;
		AkCommSettings s_settings;
#endif
		// Communication module public interface.

		AKRESULT Init( const AkCommSettings	& in_settings )
		{
#ifndef AK_OPTIMIZED
			if ( 0 == in_settings.ports.uDiscoveryBroadcast )
			{
				// uDiscoveryBroadcast cannot be dynamic
				AKASSERT( !"in_settings.ports.uDiscoveryBroadcast cannot be 0 (cannot be dynamic/ephemeral)!" );
				return AK_InvalidParameter;
			}

			if ( 0 != in_settings.ports.uCommand
				&& ( in_settings.ports.uCommand == in_settings.ports.uDiscoveryBroadcast
					|| in_settings.ports.uCommand == in_settings.ports.uNotification ) )
			{
				AKASSERT( !"in_settings.ports.uCommand must either be 0 (dynamic/ephemeral) or be different from all other ports in in_settings.ports!" );
				return AK_InvalidParameter;
			}
			
			if ( 0 != in_settings.ports.uNotification
				&& ( in_settings.ports.uNotification == in_settings.ports.uDiscoveryBroadcast
					|| in_settings.ports.uNotification == in_settings.ports.uCommand ) )
			{
				AKASSERT( !"in_settings.ports.uNotification must either be 0 (dynamic/ephemeral) or be different from all other ports in in_settings.ports!" );
				return AK_InvalidParameter;
			}

			// We must lock for two reasons:
			// - The exposed API AK::Comm::Reset() will term and reinit the comm, and this operation is unsafe at the moment if the lock is not held while the sound engine thread was already started.
			// - There is a race condition where g_pCommCentral = AkNew( AkMemID_Profiler, CommunicationCentral( AkMemID_Profiler ) ); is called, then initialized, making g_pCommCentral to be incomplete until g_pCommCentral->Init() returned with success.
			//   (g_pCommCentral->Init() is apparently calling things that need g_pCommCentral from internal calls)
			CAkFunctionCritical SpaceSetAsCritical;

			if (s_pProxyFramework)
			{
				// User Called Init() Twice without calling Term. He should have called Term.
				Term();
			}

			// keep copy of the comm settings.
			s_settings = in_settings;

			// Create and initialize the Proxy Framework and Communication Central
			s_pProxyFramework = IProxyFrameworkConnected::Create();
			if (!s_pProxyFramework)
				return AK_InsufficientMemory;

			g_pCommCentral = AkNew(AkMemID_Profiler, CommunicationCentral());
			if ( !g_pCommCentral )
				return AK_InsufficientMemory;

			if ( !g_pCommCentral->Init(s_pProxyFramework, s_pProxyFramework, in_settings.bInitSystemLib) )
			{
				Term();
				return AK_Fail;
			}

			s_pProxyFramework->Init();
			s_pProxyFramework->SetNotificationChannel( g_pCommCentral->GetNotificationChannel()	);
			
			return AK_Success;
#else
			return AK_Fail;
#endif
		}

		void GetDefaultInitSettings(
            AkCommSettings &	out_settings	///< Returned default platform-independent comm settings
		    )
		{
			// The AkCommSettings::Ports structure has a constructor that
			// initializes it to the default values
			out_settings.ports = AkCommSettings::Ports();

			out_settings.bInitSystemLib = true;
			AKPLATFORM::SafeStrCpy(out_settings.szAppNetworkName, "Unspecified", AK_COMM_SETTINGS_MAX_STRING_SIZE);
		}

		void Term()
		{
#ifndef AK_OPTIMIZED
			AK::SoundEngine::StopProfilerCapture();
			
			g_csMain.Lock();
			if ( g_pCommCentral )
			{
				//This will terminate the thread that is using s_pProxyFramework first.
				g_pCommCentral->PreTerm();
			}
			// Destroy the Proxy Framework
			if ( s_pProxyFramework )
			{
				s_pProxyFramework->Term();
				s_pProxyFramework->Destroy();
				s_pProxyFramework = NULL;
			}

			// Destroy the Communication Central
			if ( g_pCommCentral )
			{
				g_pCommCentral->Term();
				g_pCommCentral->Destroy();
				g_pCommCentral = NULL;
			}

			g_csMain.Unlock();
#endif
		}
		
		AKRESULT Reset()
		{
#ifndef AK_OPTIMIZED
			Term();
			return Init(s_settings);
#else
			return AK_Fail;
#endif // #ifndef AK_OPTIMIZED
		}

		const AkCommSettings& GetCurrentSettings()
		{
#ifndef AK_OPTIMIZED
			return s_settings;
#else
			static AkCommSettings Dummy;
			return Dummy;
#endif
		}
	}
}
