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
 
#ifndef _AK_HARMONIZERFXINFO_H_
#define _AK_HARMONIZERFXINFO_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include "AkHarmonizerFXParams.h"
#include "AkResamplingPhaseVocoder.h"
#include "BiquadFilter.h"
#include "DelayLineLight.h"
#include <AK/Plugin/PluginServices/AkFXTailHandler.h>

typedef ::DSP::BiquadFilterMultiSIMD HarmonizerFilter;

struct AkHarmonizerFXInfo
{  	
	AkHarmonizerFXInfo() : uTotalNumChannels( 0 ) {}

	DSP::BUTTERFLYSET_NAMESPACE::CAkResamplingPhaseVocoder			PhaseVocoder[AKHARMONIZER_NUMVOICES];
	HarmonizerFilter											Filter[AKHARMONIZER_NUMVOICES];
#ifdef AK_VOICE_MAX_NUM_CHANNELS
	::DSP::CDelayLight												DryDelay[AK_VOICE_MAX_NUM_CHANNELS];
#else
	::DSP::CDelayLight *											DryDelay;
#endif
	AkHarmonizerFXParams											Params;
	AkHarmonizerFXParams											PrevParams;
	AkFXTailHandler													FXTailHandler;	
	AkChannelConfig													configProcessed;
	AkUInt32														uTotalNumChannels;
	AkUInt32														uSampleRate;
	bool															bWetPathEnabled;
} AK_ALIGN_DMA;

#endif // _AK_HARMONIZERFXINFO_H_


