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

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently

#pragma once

#include <AK/Tools/Common/AkAssert.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkSoundEngineExport.h>

#ifndef AK_APPLE
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#endif
