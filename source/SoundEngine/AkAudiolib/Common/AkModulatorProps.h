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


#ifndef _AK_MODULATOR_PROPS_
#define _AK_MODULATOR_PROPS_

#include "AkPrivateTypes.h"

enum AkModulatorPropID
{
	AkModulatorPropID_Scope,
	AkModulatorPropID_Envelope_StopPlayback,

	AkModulatorPropID_Lfo_Depth,
	AkModulatorPropID_Lfo_Attack,
	AkModulatorPropID_Lfo_Frequency,
	AkModulatorPropID_Lfo_Waveform,
	AkModulatorPropID_Lfo_Smoothing,
	AkModulatorPropID_Lfo_PWM,
	AkModulatorPropID_Lfo_InitialPhase,

	AkModulatorPropID_Envelope_AttackTime,
	AkModulatorPropID_Envelope_AttackCurve,
	AkModulatorPropID_Envelope_DecayTime,
	AkModulatorPropID_Envelope_SustainLevel,
	AkModulatorPropID_Envelope_SustainTime,
	AkModulatorPropID_Envelope_ReleaseTime,
	AkModulatorPropID_Envelope_TriggerOn,

	AkModulatorPropID_Time_Duration,
	AkModulatorPropID_Time_Loops,
	AkModulatorPropID_Time_PlaybackRate,
	AkModulatorPropID_Time_InitialDelay,

	AkModulatorPropID_NUM
};

enum AkModulatorScope
{
	AkModulatorScope_Voice,
	AkModulatorScope_Note,
	AkModulatorScope_GameObject,
	AkModulatorScope_Global
};


enum AkRTPC_ModulatorParamID
{
	//Modulator RTPC's, for LFO
	RTPC_ModulatorRTPCIDStart = 0,

	RTPC_ModulatorLfoDepth = RTPC_ModulatorRTPCIDStart,
	RTPC_ModulatorLfoAttack,
	RTPC_ModulatorLfoFrequency,
	RTPC_ModulatorLfoWaveform,
	RTPC_ModulatorLfoSmoothing,
	RTPC_ModulatorLfoPWM,
	RTPC_ModulatorLfoInitialPhase,
	RTPC_ModulatorLfoRetrigger,

	//Modulator RTPC's, for Envelope
	RTPC_ModulatorEnvelopeAttackTime,
	RTPC_ModulatorEnvelopeAttackCurve,
	RTPC_ModulatorEnvelopeDecayTime,
	RTPC_ModulatorEnvelopeSustainLevel,
	RTPC_ModulatorEnvelopeSustainTime,
	RTPC_ModulatorEnvelopeReleaseTime,

	RTPC_ModulatorTimePlaybackSpeed,
	RTPC_ModulatorTimeInitialDelay,

	RTPC_MaxNumModulatorRTPC
};

const AkUInt32 AkAudioRateModulatorProps_NUM = 2;

#ifdef DEFINE_MODULATOR_PROP_DEFAULTS

extern const AkPropValue g_AkModulatorPropDefault[ AkModulatorPropID_NUM ] =
{
	(AkInt32)0,		// 	AkModulatorPropID_Lfo_Scope
	(AkInt32)1,		// 	AkModulatorPropID_Envelope_StopPlayback

	1.f,			// 	AkModulatorPropID_Lfo_Depth
	1.f,			// 	AkModulatorPropID_Lfo_Attack
	1.0f,			// 	AkModulatorPropID_Lfo_Frequency
	(AkInt32)0,		// 	AkModulatorPropID_Lfo_Waveform
	0.f,			// 	AkModulatorPropID_Lfo_Smoothing
	0.f,			// 	AkModulatorPropID_Lfo_PWM
	0.f,			// 	AkModulatorPropID_Lfo_InitialPhase

	100.f,			// 	AkModulatorPropID_Envelope_AttackTime
	0.5f,			// 	AkModulatorPropID_Envelope_AttackCurve
	200.f,			// 	AkModulatorPropID_Envelope_DecayTime
	0.3f,			// 	AkModulatorPropID_Envelope_SustainLevel
	-1.f,			//	AkModulatorPropID_Envelope_SustainTime	
	1000.f,			// 	AkModulatorPropID_Envelope_ReleaseTime
	(AkInt32)1,		// 	AkModulatorPropID_Envelope_TriggerOn

	10.0f,			// AkModulatorPropID_Time_Duration
	(AkInt32)1,		// AkModulatorPropID_Time_Loops
	1.f,			// AkModulatorPropID_Time_PlaybackRate
	0.f				// AkModulatorPropID_Time_InitialDelay
};

extern const AkRTPC_ModulatorParamID g_AkModulatorPropRTPCID[ AkModulatorPropID_NUM ] =
{
	RTPC_MaxNumModulatorRTPC, //AkModulatorPropID_Scope
	RTPC_MaxNumModulatorRTPC, //AkModulatorPropID_StopPlayback

	RTPC_ModulatorLfoDepth,
	RTPC_ModulatorLfoAttack,
	RTPC_ModulatorLfoFrequency,
	RTPC_ModulatorLfoWaveform,
	RTPC_ModulatorLfoSmoothing,
	RTPC_ModulatorLfoPWM,
	RTPC_ModulatorLfoInitialPhase,

	RTPC_ModulatorEnvelopeAttackTime,
	RTPC_ModulatorEnvelopeAttackCurve,
	RTPC_ModulatorEnvelopeDecayTime,
	RTPC_ModulatorEnvelopeSustainLevel,
	RTPC_ModulatorEnvelopeSustainTime,
	RTPC_ModulatorEnvelopeReleaseTime,
	RTPC_MaxNumModulatorRTPC,		// 	AkModulatorPropID_Envelope_TriggerOn

	RTPC_MaxNumModulatorRTPC,   // AkModulatorPropID_Time_Duration
	RTPC_MaxNumModulatorRTPC,   // AkModulatorPropID_Time_Loops
	RTPC_ModulatorTimePlaybackSpeed,
	RTPC_ModulatorTimeInitialDelay
};

#ifdef AKPROP_TYPECHECK
#include <typeinfo>
extern const std::type_info * g_AkModulatorPropTypeInfo[ AkModulatorPropID_NUM ] = 
{	// The lifetime of the object returned by typeid extends to the end of the program.

	&typeid(AkInt32), // 	AkModulatorPropID_Lfo_Scope
	&typeid(AkInt32), // 	AkModulatorPropID_Envelope_StopPlayback

	&typeid(AkReal32), // 	AkModulatorPropID_Lfo_Depth
	&typeid(AkReal32), // 	AkModulatorPropID_Lfo_Attack
	&typeid(AkReal32), // 	AkModulatorPropID_Lfo_Frequency
	&typeid(AkInt32), // 	AkModulatorPropID_Lfo_Waveform
	&typeid(AkReal32), // 	AkModulatorPropID_Lfo_Smoothing
	&typeid(AkReal32), // 	AkModulatorPropID_Lfo_PWM
	&typeid(AkReal32), // 	AkModulatorPropID_Lfo_InitialPhase

	&typeid(AkReal32), // 	AkModulatorPropID_Envelope_AttackTime
	&typeid(AkReal32), // 	AkModulatorPropID_Envelope_AttackCurve
	&typeid(AkReal32), // 	AkModulatorPropID_Envelope_DecayTime
	&typeid(AkReal32), // 	AkModulatorPropID_Envelope_SustainLevel
	&typeid(AkReal32), // 	AkModulatorPropID_Envelope_SustainTime
	&typeid(AkReal32), //	AkModulatorPropID_Envelope_ReleaseTime
	&typeid(AkInt32), //	AkModulatorPropID_Envelope_TriggerOn

	&typeid(AkReal32), //	AkModulatorPropID_Time_Duration
	&typeid(AkInt32),  //	AkModulatorPropID_Time_Loops
	&typeid(AkReal32), //	AkModulatorPropID_Time_PlaybackRate
	&typeid(AkReal32)  //	AkModulatorPropID_Time_InitialDelay
};
#endif

#else

extern const AkPropValue g_AkModulatorPropDefault[ AkModulatorPropID_NUM ];
extern const AkRTPC_ModulatorParamID g_AkModulatorPropRTPCID[ AkModulatorPropID_NUM ];

#ifdef AKPROP_TYPECHECK
#include <typeinfo>
extern const std::type_info * g_AkModulatorPropTypeInfo[ AkModulatorPropID_NUM ];
#endif

#endif


#endif
