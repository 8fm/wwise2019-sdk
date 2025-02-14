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

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/AkWwiseSDKVersion.h>

// The Protocol subversion purpose is to enforce two communication systems from the same official MAJOR release version to be identified as incompatible.
// The goal is to avoid accidental connection from unmatching versions during development time and between Beta versions and official releases.
// These connection would possibly result into crashes.
// If you change something in the communication protocol that is not supported, bump up this version.
// Do not update (it unless really required in a patch version). Doing so will require a release note to warn user of the incompatibility caused by the upgrade.
#define AK_COMM_PROTOCOL_SUBVERSION 10

//This version number will appear readable in hexadecimal.
//For example, 2009.2 will be 0x20090200
#define AK_COMM_PROTOCOL_VERSION_MAJOFFSET1 28
#define AK_COMM_PROTOCOL_VERSION_MAJOFFSET2 24
#define AK_COMM_PROTOCOL_VERSION_MAJOFFSET3 20
#define AK_COMM_PROTOCOL_VERSION_MAJOFFSET4 16
#define AK_COMM_PROTOCOL_VERSION_MINOFFSET  8

#define AK_COMM_PROTOCOL_VERSION ((AK_WWISESDK_VERSION_MAJOR / 1000 << AK_COMM_PROTOCOL_VERSION_MAJOFFSET1) | \
	(AK_WWISESDK_VERSION_MAJOR % 1000 / 100 << AK_COMM_PROTOCOL_VERSION_MAJOFFSET2) | \
	(AK_WWISESDK_VERSION_MAJOR % 100 / 10 << AK_COMM_PROTOCOL_VERSION_MAJOFFSET3) | \
	(AK_WWISESDK_VERSION_MAJOR % 10 << AK_COMM_PROTOCOL_VERSION_MAJOFFSET4) | \
	(AK_WWISESDK_VERSION_MINOR << AK_COMM_PROTOCOL_VERSION_MINOFFSET) | \
	AK_COMM_PROTOCOL_SUBVERSION \
	)

namespace CommunicationDefines
{
	enum ConsoleType
	{
		// NEVER DELETE ITEMS IN THIS LIST.
		ConsoleUnknown,
		ConsoleWindows,
        ConsoleXBox360,		// XBox360 was deprecated, but keeping it to keep backward compatibility in the communication display with previous versions of Wwise.
		ConsolePS3,			// PS3 was deprecated, but keeping it to keep backward compatibility in the communication display with previous versions of Wwise.
		ConsoleWii,			// Wii was Deprecated, but keeping it to keep backward compatibility in the communication display with previous versions of Wwise.
		ConsoleMac,
		ConsoleVitaSW,		// VitaSW was deprecated, but keeping it to keep backward compatibility in the communication display with previous versions of Wwise.
		ConsoleVitaHW,		// VitaHW was deprecated, but keeping it to keep backward compatibility in the communication display with previous versions of Wwise.
		ConsolePS4,
		ConsoleiOS,
		Console3DS,			// 3DS was deprecated, but keeping it to keep backward compatibility in the communication display with previous versions of Wwise.
		ConsoleWiiUSW,		// WiiU was Deprecated, but keeping it to keep backward compatibility in the communication display with previous versions of Wwise.
		ConsoleWiiUHW,		// WiiU was Deprecated, but keeping it to keep backward compatibility in the communication display with previous versions of Wwise.
		ConsoleAndroid,
		ConsoleXboxOne,
		ConsoleLinux,
		ConsoleWindowsPhone, // WindowsPhone was Deprecated, but keeping it to keep backward compatibility in the communication display with previous versions of Wwise.
		ConsoleEmscripten,
		ConsoleNX,
		ConsoleLumin,
		ConsoleStadia,
		ConsolePellegrino,
		ConsoleChinook
	};

#if defined( AK_WIN )
	static ConsoleType g_eConsoleType = ConsoleWindows;
#elif defined( AK_MAC_OS_X )
	static ConsoleType g_eConsoleType __attribute__ ((__unused__)) = ConsoleMac;
#elif defined( AK_IOS )
	static ConsoleType g_eConsoleType __attribute__ ((__unused__)) = ConsoleiOS;
#elif defined( AK_PS4 )
	static ConsoleType g_eConsoleType = ConsolePS4;
#elif defined( AK_LUMIN )
	static ConsoleType g_eConsoleType = ConsoleLumin;
#elif defined( AK_ANDROID )
	static ConsoleType g_eConsoleType __attribute__ ((__unused__)) = ConsoleAndroid;
#elif defined( AK_XBOXONE )
	static ConsoleType g_eConsoleType = ConsoleXboxOne;
#elif defined( AK_CHINOOK )
	static ConsoleType g_eConsoleType = ConsoleChinook;
#elif defined( AK_LINUX )
	static ConsoleType g_eConsoleType __attribute__ ((__unused__)) = ConsoleLinux;
#elif defined( AK_QNX )
	static ConsoleType g_eConsoleType __attribute__ ((__unused__)) = ConsoleWindows;
#elif defined(AK_EMSCRIPTEN)
	static ConsoleType g_eConsoleType __attribute__ ((__unused__)) = ConsoleEmscripten;
#elif defined(AK_NX)
	static ConsoleType g_eConsoleType __attribute__((__unused__)) = ConsoleNX;
#elif defined(AK_GAMINGXBOX)
	static ConsoleType g_eConsoleType = ConsoleXboxOne;	//ConsoleGX;
#elif defined(AK_COMM_CONSOLE_TYPE)
	static ConsoleType g_eConsoleType = AK_COMM_CONSOLE_TYPE;
#else
	#error CommunicationDefines.h : Console is undefined
#endif

	inline bool NeedEndianSwap(ConsoleType in_eType)
	{
		return in_eType == ConsoleXBox360
			|| in_eType == ConsolePS3
			|| in_eType == ConsoleWiiUSW
			|| in_eType == ConsoleWiiUHW;
	}
	
	const unsigned int kProfilingSessionVersion = 2;	//First version

#define CUSTOM_CONSOLE_NAME_MAX_SIZE 128
#pragma pack(push, 1)
	struct ProfilingSessionHeader
	{
		AkUInt32 uVersion;
		AkUInt32 uProtocol;
		AkUInt32 uConsoleType;
		char CustomConsoleName[CUSTOM_CONSOLE_NAME_MAX_SIZE];
		AkUInt8 uBits;
	};
#pragma pack(pop)
}





