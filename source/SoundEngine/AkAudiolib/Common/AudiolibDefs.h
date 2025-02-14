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
// AudiolibDefs.h
//
// AkAudioLib Internal defines
//
//////////////////////////////////////////////////////////////////////
#ifndef _AUDIOLIB_DEFS_H_
#define _AUDIOLIB_DEFS_H_

//----------------------------------------------------------------------------------------------------
// Behavioral engine.
//----------------------------------------------------------------------------------------------------
#define DEFAULT_CONTINUOUS_PLAYBACK_LOOK_AHEAD	(1)	// 1 audio frame sync refill

//----------------------------------------------------------------------------------------------------
// Sources.
//----------------------------------------------------------------------------------------------------
// Invalid ID (AkPluginID for plugins).
#define AK_INVALID_SOURCE_ID                    (AK_UINT_MAX)

//----------------------------------------------------------------------------------------------------
// SrcFile.
//----------------------------------------------------------------------------------------------------

#define AK_MARKERS_POOL_ID	                    (AkMemID_Processing)

//----------------------------------------------------------------------------------------------------
// Paths
//----------------------------------------------------------------------------------------------------

#define DEFAULT_MAX_NUM_PATHS                   (255)

//----------------------------------------------------------------------------------------------------
// Pipeline sample types
//----------------------------------------------------------------------------------------------------
#define AUDIOSAMPLE_FLOAT_MIN -1.f
#define AUDIOSAMPLE_FLOAT_MAX 1.f
#define AUDIOSAMPLE_SHORT_MIN -32768.f
#define AUDIOSAMPLE_SHORT_MAX 32767.f
#define AUDIOSAMPLE_UCHAR_MAX 255.f
#define AUDIOSAMPLE_UCHAR_MIN 0.f

// Below this volume, a node won't bother sending out data to the mix bus
#define AK_OUTPUT_THRESHOLD						(0.5f/32768.0f)

#define AK_MAX_HW_TIMEOUT_DEFAULT 1000

#define AK_DEFAULT_BANK_READ_BUFFER_SIZE 32 * 1024

#endif //_AUDIOLIB_DEFS_H_

