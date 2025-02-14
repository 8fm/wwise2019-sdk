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
// AkSettings.h
//
// Platform dependent part
// Allowing implementation tu run with different "native" platform settings
//
//////////////////////////////////////////////////////////////////////
#ifndef _AUDIOLIB_SETTINGS_H_
#define _AUDIOLIB_SETTINGS_H_

#include "PlatformAudiolibDefs.h"

#ifndef DEFAULT_NATIVE_FREQUENCY
#define DEFAULT_NATIVE_FREQUENCY 48000
#endif

// Slew time (time to reach target) ==  LPFPARAMUPDATEPERIOD * NUMBLOCKTOREACHTARGET sample frames
// We wish to minimize NUMBLOCKTOREACHTARGET to process bigger blocks and reduce the performance costs of parameter
// interpolation which requires a recomputation of the LPF coefficients.
// Number of samples between each LPF parameter interpolation ( triggers coef recalculate )
#define DEFAULT_LPF_UPDATE_PERIOD 128

#define DEFAULT_NUMBLOCKTOREACHTARGET 8

// Cross-platform settings.
namespace AkAudioLibSettings
{
	extern AkTimeMs g_msPerBufferTick;
}

#define AK_MS_PER_BUFFER_TICK			AkAudioLibSettings::g_msPerBufferTick

#if defined( AK_WIN ) || defined( AK_APPLE ) || defined( AK_ANDROID ) || defined( AK_LINUX ) || defined( AK_QNX ) || defined( AK_EMSCRIPTEN ) || defined( AK_NX )

namespace AkAudioLibSettings
{
	extern AkUInt32 g_pipelineCoreFrequency;
	extern AkUInt32 g_pcWaitTime;
	extern AkUInt32 g_uLpfUpdatePeriod;
	extern AkUInt32 g_uNumSamplesPerFrame;
	extern AkUInt16 g_uNumBlockToReachTarget;

	void SetAudioBufferSettings(AkUInt32 in_uSampleFrequency, AkUInt32 in_uNumSamplesPerFrame);
};

#ifndef AK_CORE_SAMPLERATE
	#define AK_CORE_SAMPLERATE				AkAudioLibSettings::g_pipelineCoreFrequency
#endif
#define AK_PC_WAIT_TIME					AkAudioLibSettings::g_pcWaitTime
#define LPFPARAMUPDATEPERIOD			AkAudioLibSettings::g_uLpfUpdatePeriod
#define NUMBLOCKTOREACHTARGET			AkAudioLibSettings::g_uNumBlockToReachTarget
#ifndef AK_NUM_VOICE_REFILL_FRAMES
	#define AK_NUM_VOICE_REFILL_FRAMES		(AkUInt16)AkAudioLibSettings::g_uNumSamplesPerFrame
#endif
#ifndef LE_MAX_FRAMES_PER_BUFFER
	#define LE_MAX_FRAMES_PER_BUFFER		(AkUInt16)AK_NUM_VOICE_REFILL_FRAMES
#endif

#elif defined( AK_SONY ) || defined( AK_XBOX )

namespace AkAudioLibSettings
{
	extern AkUInt32 g_pipelineCoreFrequency;
	extern AkUInt32 g_pcWaitTime;
	extern AkUInt32 g_uNumSamplesPerFrame;

	void SetAudioBufferSettings(AkUInt32 in_uSampleFrequency, AkUInt32 in_uNumSamplesPerFrame);
};

#define AK_CORE_SAMPLERATE				DEFAULT_NATIVE_FREQUENCY
#define AK_PC_WAIT_TIME					AkAudioLibSettings::g_pcWaitTime
#define LPFPARAMUPDATEPERIOD			DEFAULT_LPF_UPDATE_PERIOD
#define	NUMBLOCKTOREACHTARGET			DEFAULT_NUMBLOCKTOREACHTARGET
#define AK_NUM_VOICE_REFILL_FRAMES		(AkUInt16)AkAudioLibSettings::g_uNumSamplesPerFrame
#define LE_MAX_FRAMES_PER_BUFFER		(AkUInt16)AK_NUM_VOICE_REFILL_FRAMES

#else

#error "AkSettings not defined for platform!"

#endif

#endif //_AUDIOLIB_SETTINGS_H_
