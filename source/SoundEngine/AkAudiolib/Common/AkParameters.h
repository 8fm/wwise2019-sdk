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

#pragma once

#include "AkPrivateTypes.h"
#include "AkRTPC.h"

enum ParamType
{
	PT_All				= 0xffffffff,
    PT_Volume			= 0x0001,
    PT_Pitch			= 0x0002,
    PT_LPF				= 0x0004,
	PT_HPF				= 0x0008,
	PT_BusVolume		= 0x0010,
	PT_HDR				= 0x0020,
	PT_BypassAllFX		= 0x0040
};

enum AkLoopValue
{
	AkLoopVal_Infinite		= 0,
	AkLoopVal_NotLooping	= 1
};

// Continous range for possible properties on ParameterNodeBase and descendants.
enum AkPropID
{
	// These four values must stay together in this order at offset 0 and equivalent to RTPC_Volume et AL.
    AkPropID_Volume					= 0,// Group 1 do not separate
    AkPropID_LFE					= 1,// Group 1 do not separate
    AkPropID_Pitch					= 2,// Group 1 do not separate
	AkPropID_LPF					= 3,// Group 1 do not separate
	AkPropID_HPF					= 4,// Group 1 do not separate
	AkPropID_BusVolume				= 5,// Group 1 do not separate
	AkPropID_MakeUpGain				= 6,// Group 1 do not separate

	/*Not a prop*/AkPropID_StatePropNum			= 7,// <-- Indicate the state property count

	AkPropID_Priority				= 7,
	AkPropID_PriorityDistanceOffset	= 8,

	//AkPropID_FeedbackVolume	= 9,  // Group 2 do not separate. Deprecated in Motion refactor.
	//AkPropID_FeedbackLPF		= 10, // Group 2 do not separate Deprecated in Motion refactor.

	AkPropID_MuteRatio				= 11,

	AkPropID_PAN_LR					= 12,
	AkPropID_PAN_FR					= 13,
	AkPropID_CenterPCT				= 14,

	AkPropID_DelayTime				= 15,
	AkPropID_TransitionTime			= 16,
	AkPropID_Probability			= 17,

	AkPropID_DialogueMode			= 18,

    AkPropID_UserAuxSendVolume0		= 19,// Group 3 do not separate
    AkPropID_UserAuxSendVolume1		= 20,// Group 3 do not separate
    AkPropID_UserAuxSendVolume2		= 21,// Group 3 do not separate
    AkPropID_UserAuxSendVolume3		= 22,// Group 3 do not separate

    AkPropID_GameAuxSendVolume		= 23,// Group 3 do not separate

    AkPropID_OutputBusVolume		= 24,// Group 3 do not separate
    AkPropID_OutputBusHPF			= 25,// Group 3 do not separate
    AkPropID_OutputBusLPF			= 26,// Group 3 do not separate

	AkPropID_HDRBusThreshold		= 27,
	AkPropID_HDRBusRatio			= 28,
	AkPropID_HDRBusReleaseTime		= 29,
	AkPropID_HDRBusGameParam		= 30,
	AkPropID_HDRBusGameParamMin		= 31,
	AkPropID_HDRBusGameParamMax		= 32,
	AkPropID_HDRActiveRange			= 33,

	AkPropID_LoopStart				= 34, // 
	AkPropID_LoopEnd				= 35, //
	AkPropID_TrimInTime				= 36, //
	AkPropID_TrimOutTime			= 37, //
	AkPropID_FadeInTime				= 38, //
	AkPropID_FadeOutTime			= 39, //
	AkPropID_FadeInCurve			= 40, //
	AkPropID_FadeOutCurve			= 41, //
	AkPropID_LoopCrossfadeDuration	= 42, //
	AkPropID_CrossfadeUpCurve		= 43, //
	AkPropID_CrossfadeDownCurve		= 44, //

	AkPropID_MidiTrackingRootNote	= 45,
	AkPropID_MidiPlayOnNoteType		= 46,
	AkPropID_MidiTransposition		= 47,
	AkPropID_MidiVelocityOffset		= 48,
	AkPropID_MidiKeyRangeMin		= 49,
	AkPropID_MidiKeyRangeMax		= 50,
	AkPropID_MidiVelocityRangeMin	= 51,
	AkPropID_MidiVelocityRangeMax	= 52,
	AkPropID_MidiChannelMask		= 53,

	AkPropID_PlaybackSpeed			= 54,

	AkPropID_MidiTempoSource		= 55,
	AkPropID_MidiTargetNode			= 56,

	AkPropID_AttachedPluginFXID		= 57,

	AkPropID_Loop					= 58,
	AkPropID_InitialDelay			= 59,

	AkPropID_UserAuxSendLPF0		= 60,
	AkPropID_UserAuxSendLPF1		= 61,
	AkPropID_UserAuxSendLPF2		= 62,
	AkPropID_UserAuxSendLPF3		= 63,

	AkPropID_UserAuxSendHPF0		= 64,
	AkPropID_UserAuxSendHPF1		= 65,
	AkPropID_UserAuxSendHPF2		= 66,
	AkPropID_UserAuxSendHPF3		= 67,

	AkPropID_GameAuxSendLPF			= 68,
	AkPropID_GameAuxSendHPF			= 69,

	AkPropID_AttenuationID			= 70,
	AkPropID_PositioningTypeBlend	= 71,

	AkPropID_ReflectionBusVolume	= 72,

	AkPropID_NUM // Number of properties defined above
};

#if defined(AK_WIN) && defined(_DEBUG) && defined(_HAS_EXCEPTIONS) && _HAS_EXCEPTIONS
	#define AKPROP_TYPECHECK
#endif

#ifdef AK_DEFINE_AKPROPDEFAULT

extern const AkPropValue g_AkPropDefault[AkPropID_NUM] =
{
	0.0f, // AkPropID_Volume
	0.0f, // AkPropID_LFE
	0.0f, // AkPropID_Pitch
	0.0f, // AkPropID_LPF
	0.0f, // AkPropID_HPF
	0.0f, // AkPropID_BusVolume
	0.0f, // AkPropID_MakeUpGain
	(AkReal32)AK_DEFAULT_PRIORITY, // AkPropID_Priority
	-10.0f, // AkPropID_PriorityDistanceOffset
	0.0f, // AkPropID_FeedbackVolume
	0.0f, // AkPropID_FeedbackLPF
	AK_UNMUTED_RATIO, // AkPropID_MuteRatio
	(AkReal32)AK_DEFAULT_PAN_RL_VALUE, // AkPropID_PAN_LR
	(AkReal32)AK_DEFAULT_PAN_FR_VALUE, // AkPropID_PAN_FR
	0.0f, // AkPropID_CenterPCT
	(AkInt32)0, // AkPropID_DelayTime
	(AkInt32)0, // AkPropID_TransitionTime
	(AkReal32)DEFAULT_PROBABILITY, // AkPropID_Probability
	(AkInt32)0, // AkPropID_DialogueMode
	0.0f, // AkPropID_UserAuxSendVolume0
	0.0f, // AkPropID_UserAuxSendVolume1
	0.0f, // AkPropID_UserAuxSendVolume2
	0.0f, // AkPropID_UserAuxSendVolume3
	0.0f, // AkPropID_GameAuxSendVolume
	0.0f, // AkPropID_OutputBusVolume
	0.0f, // AkPropID_OutputBusHPF
	0.0f, // AkPropID_OutputBusLPF
	AK_DEFAULT_HDR_BUS_THRESHOLD, // AkPropID_HDRBusThreshold
	AK_DEFAULT_HDR_BUS_RATIO, // AkPropID_HDRBusRatio
	AK_DEFAULT_HDR_BUS_RELEASE_TIME, // AkPropID_HDRBusReleaseTime
	(AkInt32)AK_INVALID_UNIQUE_ID, // AkPropID_HDRBusGameParam
	AK_DEFAULT_HDR_BUS_GAME_PARAM_MIN, // AkPropID_HDRBusGameParamMin
	AK_DEFAULT_HDR_BUS_GAME_PARAM_MAX, // AkPropID_HDRBusGameParamMax
	AK_DEFAULT_HDR_ACTIVE_RANGE, // AkPropID_HDRActiveRange

	0.0f,  // AkPropID_LoopStart
	0.0f,  // AkPropID_LoopEnd
	0.0f,  // AkPropID_TrimInTime
	0.0f,  // AkPropID_TrimOutTime
	0.0f,  // AkPropID_FadeInTime	
	0.0f,  // AkPropID_FadeOutTime
	(AkInt32)AkCurveInterpolation_Sine,  // AkPropID_FadeInCurve
	(AkInt32)AkCurveInterpolation_Sine,  // AkPropID_FadeOutCurve
	0.0f,  // AkPropID_LoopCrossfadeDuration
	(AkInt32)AkCurveInterpolation_Sine,		//AkPropID_CrossfadeUpCurve
	(AkInt32)AkCurveInterpolation_SineRecip, //AkPropID_CrossfadeDownCurve

	(AkInt32)60,								//AkPropID_MidiTrackingRootNote,
	(AkInt32)1,								//AkPropID_MidiPlayOnNoteType, == AkPropID_MidiPlayOnNoteType_On
	(AkInt32)0,								//AkPropID_MidiTransposition,
	(AkInt32)0,								//AkPropID_MidiVelocityOffset,
	(AkInt32)0,								//AkPropID_MidiKeyRangeMin,
	(AkInt32)127,							//AkPropID_MidiKeyRangeMax,
	(AkInt32)0,								//AkPropID_MidiVelocityRangeMin,
	(AkInt32)127,							//AkPropID_MidiVelocityRangeMax,
	(AkInt32)0xFFFF,						//AkPropID_MidiChannelMask,
	AK_DEFAULT_PLAYBACK_SPEED,				//AkPropID_PlaybackSpeed

	(AkInt32)0,								//AkPropID_MidiTempoSource
	(AkInt32)0,								//AkPropID_MidiTargetNode

	(AkInt32)AK_INVALID_PLUGINID,			//AkPropID_AttachedPluginFXID
	(AkInt32)AkLoopVal_NotLooping, // AkPropID_Loop
	0.0f, // AkPropID_InitialDelay

	0.0f, // AkPropID_UserAuxSendLPF0
	0.0f, // AkPropID_UserAuxSendLPF1
	0.0f, // AkPropID_UserAuxSendLPF2
	0.0f, // AkPropID_UserAuxSendLPF3

	0.0f, // AkPropID_UserAuxSendHPF0
	0.0f, // AkPropID_UserAuxSendHPF1
	0.0f, // AkPropID_UserAuxSendHPF2
	0.0f, // AkPropID_UserAuxSendHPF3

	0.0f, // AkPropID_GameAuxSendLPF
	0.0f, // AkPropID_GameAuxSendHPF

	(AkInt32)AK_INVALID_UNIQUE_ID,	// AkPropID_AttenuationID
	100.f,	// AkPropID_PositioningTypeBlend

	0.f,//AkPropID_ReflectionBusVolume
};



extern const AkRTPC_ParameterID g_AkPropRTPCID[ AkPropID_NUM ] =
{
	RTPC_Volume, // AkPropID_Volume
	RTPC_LFE, // AkPropID_LFE
	RTPC_Pitch, // AkPropID_Pitch
	RTPC_LPF, // AkPropID_LPF
	RTPC_HPF, // AkPropID_HPF
	RTPC_BusVolume, // AkPropID_BusVolume
	RTPC_MakeUpGain, // AkPropID_MakeUpGain
	RTPC_Priority, // AkPropID_Priority
	RTPC_MaxNumRTPC, // AkPropID_PriorityDistanceOffset
	RTPC_MaxNumRTPC, // AkPropID_FeedbackVolume
	RTPC_MaxNumRTPC, // AkPropID_FeedbackLPF
	RTPC_MuteRatio,			// AkPropID_MuteRatio
	RTPC_Position_PAN_X_2D, // AkPropID_PAN_LR
	RTPC_Position_PAN_Y_2D, // AkPropID_PAN_FR
	RTPC_Positioning_Divergence_Center_PCT, // AkPropID_CenterPCT
	RTPC_MaxNumRTPC, // AkPropID_DelayTime
	RTPC_MaxNumRTPC, // AkPropID_TransitionTime
	RTPC_MaxNumRTPC, // AkPropID_Probability
	RTPC_MaxNumRTPC, // AkPropID_DialogueMode
    RTPC_UserAuxSendVolume0,	// AkPropID_UserAuxSendVolume0
    RTPC_UserAuxSendVolume1,	// AkPropID_UserAuxSendVolume1
    RTPC_UserAuxSendVolume2,	// AkPropID_UserAuxSendVolume2
    RTPC_UserAuxSendVolume3,	// AkPropID_UserAuxSendVolume3
    RTPC_GameAuxSendVolume,		// AkPropID_GameAuxSendVolume
    RTPC_OutputBusVolume,		// AkPropID_OutputBusVolume
    RTPC_OutputBusHPF,			// AkPropID_OutputBusHPF
	RTPC_OutputBusLPF,			// AkPropID_OutputBusLPF
	RTPC_HDRBusThreshold,		// AkPropID_HDRBusThreshold
	RTPC_HDRBusRatio,			// AkPropID_HDRBusRatio
	RTPC_HDRBusReleaseTime,		// AkPropID_HDRBusReleaseTime
	RTPC_MaxNumRTPC,			// AkPropID_HDRBusGameParam
	RTPC_MaxNumRTPC,			// AkPropID_HDRBusGameParamMin
	RTPC_MaxNumRTPC,			// AkPropID_HDRBusGameParamMax
	RTPC_HDRActiveRange,		// AkPropID_HDRActiveRange
	RTPC_MaxNumRTPC, // AkPropID_LoopStart
	RTPC_MaxNumRTPC, // AkPropID_LoopEnd
	RTPC_MaxNumRTPC,  // AkPropID_TrimInTime
	RTPC_MaxNumRTPC,  // AkPropID_TrimOutTime
	RTPC_MaxNumRTPC,  // AkPropID_FadeInTime	
	RTPC_MaxNumRTPC,  // AkPropID_FadeOutTime
	RTPC_MaxNumRTPC,  // AkPropID_FadeInCurve
	RTPC_MaxNumRTPC,  // AkPropID_FadeOutCurve
	RTPC_MaxNumRTPC,  // AkPropID_LoopCrossfadeDuration
	RTPC_MaxNumRTPC,  // AkPropID_CrossfadeUpCurve
	RTPC_MaxNumRTPC,  // AkPropID_CrossfadeDownCurve

	RTPC_MaxNumRTPC,			//AkPropID_MidiTrackingRootNote,
	RTPC_MaxNumRTPC,			//AkPropID_MidiPlayOnNoteType,
	RTPC_MidiTransposition,		//AkPropID_MidiTransposition,
	RTPC_MidiVelocityOffset,	//AkPropID_MidiVelocityOffset,
	RTPC_MaxNumRTPC,			//AkPropID_MidiKeyRangeMin,
	RTPC_MaxNumRTPC,			//AkPropID_MidiKeyRangeMax,
	RTPC_MaxNumRTPC,			//AkPropID_MidiVelocityRangeMin,
	RTPC_MaxNumRTPC,			//AkPropID_MidiVelocityRangeMax,
	RTPC_MaxNumRTPC,			//AkPropID_MidiChannelMask,
	RTPC_PlaybackSpeed,			//AkPropID_PlaybackSpeed

	RTPC_MaxNumRTPC,	//AkPropID_MidiTempoSource
	RTPC_MaxNumRTPC,		//AkPropID_MidiTargetNode

	RTPC_MaxNumRTPC,	// AkPropID_AttachedPluginFXID
	RTPC_MaxNumRTPC,	// AkPropID_Loop
	RTPC_InitialDelay,	// AkPropID_InitialDelay

	RTPC_UserAuxSendLPF0, // AkPropID_UserAuxSendLPF0
	RTPC_UserAuxSendLPF1, // AkPropID_UserAuxSendLPF1
	RTPC_UserAuxSendLPF2, // AkPropID_UserAuxSendLPF2
	RTPC_UserAuxSendLPF3, // AkPropID_UserAuxSendLPF3

	RTPC_UserAuxSendHPF0, // AkPropID_UserAuxSendHPF0
	RTPC_UserAuxSendHPF1, // AkPropID_UserAuxSendHPF1
	RTPC_UserAuxSendHPF2, // AkPropID_UserAuxSendHPF2
	RTPC_UserAuxSendHPF3, // AkPropID_UserAuxSendHPF3

	RTPC_GameAuxSendLPF, // AkPropID_GameAuxSendLPF
	RTPC_GameAuxSendHPF, // AkPropID_GameAuxSendHPF

	RTPC_MaxNumRTPC, // AkPropID_AttenuationID
	RTPC_PositioningTypeBlend,	// AkPropID_PositioningTypeBlend

	RTPC_ReflectionsVolume, // AkPropID_ReflectionBusVolume
};

extern const AkPropID g_AkRTPCToPropID[RTPC_MaxNumRTPC] = 
{
	AkPropID_Volume, // RTPC__Volume
	AkPropID_LFE,
	AkPropID_Pitch,

	// Main LPF
	AkPropID_LPF,
	AkPropID_HPF,

	AkPropID_BusVolume,

	AkPropID_InitialDelay,

	AkPropID_MakeUpGain,

	AkPropID_NUM,
	AkPropID_NUM,
	AkPropID_NUM,

	AkPropID_MidiTransposition,
	AkPropID_MidiVelocityOffset,
	AkPropID_PlaybackSpeed,

	AkPropID_MuteRatio,

	// Random/Sequence Container
	AkPropID_NUM,

	// Advanced Settings
	AkPropID_NUM,

	// Advanced Settings
	AkPropID_Priority,

	// Positioning - Position specific
	AkPropID_PAN_LR,
	AkPropID_PAN_FR,
	// Positioning - 3D position
	AkPropID_NUM,
	AkPropID_NUM,
	AkPropID_NUM,
	// Panning vs 3D Spatialization blend
	AkPropID_PositioningTypeBlend,
	// Positioning - Parameters common to Radius and Position
	AkPropID_CenterPCT,
	AkPropID_NUM,
	AkPropID_NUM,
	AkPropID_NUM,
	AkPropID_NUM,

	// The 4 effect slots
	AkPropID_NUM,
	AkPropID_NUM,
	AkPropID_NUM,
	AkPropID_NUM,
	AkPropID_NUM, 
	// HDR
	AkPropID_HDRBusThreshold,
	AkPropID_HDRBusReleaseTime,
	AkPropID_HDRBusRatio,
	AkPropID_HDRActiveRange,

	// Game defined send
	AkPropID_GameAuxSendVolume,

	// User defined sends
	AkPropID_UserAuxSendVolume0,
	AkPropID_UserAuxSendVolume1,
	AkPropID_UserAuxSendVolume2,
	AkPropID_UserAuxSendVolume3,

	// Bus output params - are only valid if there is a bus output node.
	AkPropID_OutputBusVolume,
	AkPropID_OutputBusHPF,
	AkPropID_OutputBusLPF,

	AkPropID_NUM,

	AkPropID_ReflectionBusVolume,

	AkPropID_UserAuxSendLPF0,
	AkPropID_UserAuxSendLPF1,
	AkPropID_UserAuxSendLPF2,
	AkPropID_UserAuxSendLPF3,

	AkPropID_UserAuxSendHPF0,
	AkPropID_UserAuxSendHPF1,
	AkPropID_UserAuxSendHPF2,
	AkPropID_UserAuxSendHPF3,

	AkPropID_GameAuxSendLPF,
	AkPropID_GameAuxSendHPF
};


#ifdef AKPROP_TYPECHECK
	#include <typeinfo>
	extern const std::type_info * g_AkPropTypeInfo[ AkPropID_NUM ] = 
	{	// The lifetime of the object returned by typeid extends to the end of the program.
		&typeid(AkReal32), // AkPropID_Volume
		&typeid(AkReal32), // AkPropID_LFE
		&typeid(AkReal32), // AkPropID_Pitch
		&typeid(AkReal32), // AkPropID_LPF
		&typeid(AkReal32), // AkPropID_HPF
		&typeid(AkReal32), // AkPropID_BusVolume
		&typeid(AkReal32), // AkPropID_MakeUpGain
		&typeid(AkReal32), // AkPropID_Priority
		&typeid(AkReal32), // AkPropID_PriorityDistanceOffset
		&typeid(AkReal32), // AkPropID_FeedbackVolume
		&typeid(AkReal32), // AkPropID_FeedbackLPF
		&typeid(AkReal32), // AkPropID_MuteRatio
		&typeid(AkReal32), // AkPropID_PAN_LR
		&typeid(AkReal32), // AkPropID_PAN_FR
		&typeid(AkReal32), // AkPropID_CenterPCT
		&typeid(AkInt32),  // AkPropID_DelayTime
		&typeid(AkInt32),  // AkPropID_TransitionTime
		&typeid(AkReal32), // AkPropID_Probability
		&typeid(AkInt32),  // AkPropID_DialogueMode
		&typeid(AkReal32), // AkPropID_UserAuxSendVolume0
		&typeid(AkReal32), // AkPropID_UserAuxSendVolume1
		&typeid(AkReal32), // AkPropID_UserAuxSendVolume2
		&typeid(AkReal32), // AkPropID_UserAuxSendVolume3
		&typeid(AkReal32), // AkPropID_GameAuxSendVolume
		&typeid(AkReal32), // AkPropID_OutputBusVolume
		&typeid(AkReal32), // AkPropID_OutputBusHPF
		&typeid(AkReal32), // AkPropID_OutputBusLPF
		&typeid(AkReal32), // AkPropID_HDRBusThreshold
		&typeid(AkReal32), // AkPropID_HDRBusRatio
		&typeid(AkReal32), // AkPropID_HDRBusReleaseTime	
		&typeid(AkInt32), // AkPropID_HDRBusGameParam
		&typeid(AkReal32), // AkPropID_HDRBusGameParamMin
		&typeid(AkReal32), // AkPropID_HDRBusGameParamMax
		&typeid(AkReal32), // AkPropID_HDRActiveRange
		
		&typeid(AkReal32),  // AkPropID_LoopStart
		&typeid(AkReal32),  // AkPropID_LoopEnd
		&typeid(AkReal32),  // AkPropID_TrimInTime
		&typeid(AkReal32),  // AkPropID_TrimOutTime
		&typeid(AkReal32),  // AkPropID_FadeInTime	
		&typeid(AkReal32),  // AkPropID_FadeOutTime
		&typeid(AkInt32),  // AkPropID_FadeInCurve
		&typeid(AkInt32),  // AkPropID_FadeOutCurve
		&typeid(AkReal32),  // AkPropID_LoopCrossfadeDuration
		&typeid(AkInt32),  // AkPropID_CrossfadeUpCurve
		&typeid(AkInt32),  // AkPropID_CrossfadeDownCurve

		&typeid(AkInt32),	//AkPropID_MidiTrackingRootNote,
		&typeid(AkInt32),	//AkPropID_MidiPlayOn,
		&typeid(AkInt32),	//AkPropID_MidiTransposition,
		&typeid(AkInt32),	//AkPropID_MidiVelocityOffset,
		&typeid(AkInt32),	//AkPropID_MidiKeyRangeMin,
		&typeid(AkInt32),	//AkPropID_MidiKeyRangeMax,
		&typeid(AkInt32),	//AkPropID_MidiVelocityRangeMin,
		&typeid(AkInt32),	//AkPropID_MidiVelocityRangeMax,
		&typeid(AkInt32),	//AkPropID_MidiChannelMask,
		&typeid(AkReal32),	//AkPropID_PlaybackSpeed,

		&typeid(AkInt32),	//AkPropID_MidiTempoSource
		&typeid(AkInt32),	//AkPropID_MidiTargetNode

		&typeid(AkInt32),	//AkPropID_AttachedPluginFXID
		&typeid(AkInt32),	// AkPropID_Loop
		&typeid(AkReal32),	// AkPropID_InitialDelay

		&typeid(AkReal32), // AkPropID_UserAuxSendLPF0
		&typeid(AkReal32), // AkPropID_UserAuxSendLPF1
		&typeid(AkReal32), // AkPropID_UserAuxSendLPF2
		&typeid(AkReal32), // AkPropID_UserAuxSendLPF3

		&typeid(AkReal32), // AkPropID_UserAuxSendHPF0
		&typeid(AkReal32), // AkPropID_UserAuxSendHPF1
		&typeid(AkReal32), // AkPropID_UserAuxSendHPF2
		&typeid(AkReal32), // AkPropID_UserAuxSendHPF3

		&typeid(AkReal32), // AkPropID_GameAuxSendLPF
		&typeid(AkReal32), // AkPropID_GameAuxSendHPF

		&typeid(AkInt32),  // AkPropID_AttenuationID
		&typeid(AkReal32), // AkPropID_PositioningTypeBlend

		&typeid(AkReal32), // AkPropID_ReflectionBusVolume
	};
#endif

#else

extern const AkPropValue g_AkPropDefault[AkPropID_NUM];
extern const AkRTPC_ParameterID g_AkPropRTPCID[ AkPropID_NUM ];
extern const AkPropID g_AkRTPCToPropID[RTPC_MaxNumRTPC];


#ifdef AKPROP_TYPECHECK
	#include <typeinfo>
	extern const std::type_info * g_AkPropTypeInfo[ AkPropID_NUM ];
#endif

#endif
