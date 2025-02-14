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

#include "stdafx.h"
#include "AkSettings.h"

#include "PlatformAudiolibDefs.h"

#if defined( AK_WIN ) || defined( AK_APPLE ) || defined( AK_ANDROID ) || defined( AK_LINUX ) || defined( AK_QNX ) || defined( AK_EMSCRIPTEN ) || defined( AK_NX )

//Minimal block size to update the LPF.  MUST be a power of 2.
#define LPF_PERIOD_MIN_UPDATE 32

namespace AkAudioLibSettings
{
	AkUInt32 g_pipelineCoreFrequency = DEFAULT_NATIVE_FREQUENCY ;
	AkTimeMs g_msPerBufferTick = (AkUInt32)( AK_DEFAULT_NUM_SAMPLES_PER_FRAME / ( DEFAULT_NATIVE_FREQUENCY / 1000.0f ) );
	AkUInt32 g_pcWaitTime = ((AkUInt32)( 1000.0f * AK_DEFAULT_NUM_SAMPLES_PER_FRAME / DEFAULT_NATIVE_FREQUENCY / 4.0  ));
	AkUInt32 g_uLpfUpdatePeriod = DEFAULT_LPF_UPDATE_PERIOD;
	AkUInt32 g_uNumSamplesPerFrame = AK_DEFAULT_NUM_SAMPLES_PER_FRAME;
	AkUInt16 g_uNumBlockToReachTarget = DEFAULT_NUMBLOCKTOREACHTARGET;

	void SetAudioBufferSettings( AkUInt32 in_uSampleFrequency, AkUInt32 in_uNumSamplesPerFrame )
	{
		g_pipelineCoreFrequency = in_uSampleFrequency;
		g_uNumSamplesPerFrame = in_uNumSamplesPerFrame;

		g_msPerBufferTick = (AkUInt32)(g_uNumSamplesPerFrame / (g_pipelineCoreFrequency / 1000.0f));
		g_pcWaitTime = ((AkUInt32)(1000.0f * g_uNumSamplesPerFrame / g_pipelineCoreFrequency / 4.0));				
		g_uLpfUpdatePeriod = (g_pipelineCoreFrequency * DEFAULT_LPF_UPDATE_PERIOD) / DEFAULT_NATIVE_FREQUENCY;
		g_uLpfUpdatePeriod = ((g_uLpfUpdatePeriod + 3) / 4) * 4;	//Round-up to next multiple of 4 (for SIMD processing)
		g_uNumBlockToReachTarget = (AkUInt16)((g_uNumSamplesPerFrame + (g_uLpfUpdatePeriod/2 )) / g_uLpfUpdatePeriod);			//Round to closest integer
	}
}

#else

namespace AkAudioLibSettings
{
	AkTimeMs g_msPerBufferTick = (AkUInt32)( AK_DEFAULT_NUM_SAMPLES_PER_FRAME / ( DEFAULT_NATIVE_FREQUENCY / 1000.0f ) );
	AkUInt32 g_pcWaitTime = ((AkUInt32)( 1000.0f * AK_DEFAULT_NUM_SAMPLES_PER_FRAME / DEFAULT_NATIVE_FREQUENCY / 4.0f ));
	AkUInt32 g_uNumSamplesPerFrame = AK_DEFAULT_NUM_SAMPLES_PER_FRAME;
	AkUInt32 g_pipelineCoreFrequency = DEFAULT_NATIVE_FREQUENCY;
	AkUInt16 g_uNumBlockToReachTarget = DEFAULT_NUMBLOCKTOREACHTARGET;

	void SetAudioBufferSettings(AkUInt32 in_uSampleFrequency, AkUInt32 in_uNumSamplesPerFrame)
	{
		g_uNumSamplesPerFrame = in_uNumSamplesPerFrame;

		g_msPerBufferTick = (AkUInt32)( g_uNumSamplesPerFrame / ( DEFAULT_NATIVE_FREQUENCY / 1000.0f ) );
		g_pcWaitTime = ((AkUInt32)( 1000.0f * g_uNumSamplesPerFrame / DEFAULT_NATIVE_FREQUENCY / 4.0f ));
		g_uNumBlockToReachTarget = (AkUInt16) (g_uNumSamplesPerFrame / DEFAULT_LPF_UPDATE_PERIOD);
	}
}

#endif
