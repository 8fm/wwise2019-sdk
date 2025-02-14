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
// AkRTPC.h
//
// Data structures used for RTPC.
//
//////////////////////////////////////////////////////////////////////
#ifndef _RTPC_H_
#define _RTPC_H_

#include "AkPrivateTypes.h"
#include "AkBitArray.h"
#include <AK/Tools/Common/AkAssert.h>

// Every point of a conversion graph
template< class VALUE_TYPE >
struct AkGraphPointBase
{
	AkReal32				From;		// Representing X Axis
	VALUE_TYPE				To;	    	// Representing Y Axis
	AkCurveInterpolation	Interp;		// How to interpolate between this point and the next
};

// Every point of a conversion graph
typedef AkGraphPointBase<AkReal32> AkRTPCGraphPoint;

// Points in switch conversion curves
typedef AkGraphPointBase<AkUInt32> AkSwitchGraphPoint;

enum AkCurveScaling // Written as AkUInt8 in bank
{
	AkCurveScaling_None							= 0, // No special scaling
	AkCurveScaling_dB							= 2, // dB scaling (mirrored around 0)
	AkCurveScaling_Log							= 3, // log scaling (typically used to map frequencies)
	AkCurveScaling_dBToLin						= 4, // curve is dB values, result wanted in linear gain

	AkCurveScaling_MaxNum						= 8
};

enum AkRtpcType
{
	AkRtpcType_GameParameter					= 0,
	AkRtpcType_MIDIParameter					= 1,
	AkRtpcType_Modulator						= 2,

	AkRtpcType_MaxNum							= 8
};

enum AkRtpcAccum
{
	AkRtpcAccum_None							= 0,
	AkRtpcAccum_Exclusive						= 1,
	AkRtpcAccum_Additive						= 2,
	AkRtpcAccum_Multiply						= 3,
	AkRtpcAccum_Boolean							= 4,

	AkRtpcAccum_MaxNum							= 8
};

// ** If you change this enum ** 
//		-Run the python script 'Scripts\UpdateAudioEnginePropertyIds.py' to update WOBjects.xml
//
// IDs of the AudioLib known RTPC capable parameters
enum AkRTPC_ParameterID
{
	RTPC_ADDITIVE_PARAMS_START = 0,

	// Main Audionode
	RTPC_Volume										= RTPC_ADDITIVE_PARAMS_START,
	RTPC_LFE,
	RTPC_Pitch,

	// Main LPF
	RTPC_LPF,
	RTPC_HPF,

	RTPC_BusVolume,

	RTPC_InitialDelay,

	RTPC_MakeUpGain,

	Deprecated_RTPC_FeedbackVolume,
	Deprecated_RTPC_FeedbackLowpass,
	Deprecated_RTPC_FeedbackPitch, // not actually a rtpcable parameter

	RTPC_MidiTransposition,
	RTPC_MidiVelocityOffset,
	RTPC_PlaybackSpeed,

	RTPC_MuteRatio, // #Nate can we remove this??

	// Random/Sequence Container
	RTPC_PlayMechanismSpecialTransitionsValue,

	// Advanced Settings
	RTPC_MaxNumInstances,

	//
	RTPC_OVERRIDABLE_PARAMS_START, //----------------------------------------

	// Advanced Settings
	RTPC_Priority						= RTPC_OVERRIDABLE_PARAMS_START,
	
	// Positioning - Position specific
	RTPC_Position_PAN_X_2D,
	RTPC_Position_PAN_Y_2D,
	// Positioning - 3D position
	RTPC_Position_PAN_X_3D,
	RTPC_Position_PAN_Y_3D,
	RTPC_Position_PAN_Z_3D,
	// Panning vs 3D Spatialization blend
	RTPC_PositioningTypeBlend,
	// Positioning - Parameters common to Radius and Position
	RTPC_Positioning_Divergence_Center_PCT,
	RTPC_Positioning_Cone_Attenuation_ON_OFF,
	RTPC_Positioning_Cone_Attenuation,
	RTPC_Positioning_Cone_LPF,
	RTPC_Positioning_Cone_HPF,

	// The 4 effect slots
	RTPC_BypassFX0,
	RTPC_BypassFX1,
	RTPC_BypassFX2,
	RTPC_BypassFX3,
	RTPC_BypassAllFX, // placed here to follow bit order in bitfields

	// HDR
	RTPC_HDRBusThreshold,
	RTPC_HDRBusReleaseTime,
	RTPC_HDRBusRatio,
	RTPC_HDRActiveRange,

	// Game defined send
	RTPC_GameAuxSendVolume,

	// User defined sends
    RTPC_UserAuxSendVolume0,
    RTPC_UserAuxSendVolume1,
    RTPC_UserAuxSendVolume2,
    RTPC_UserAuxSendVolume3,

	// Bus output params - are only valid if there is a bus output node.
	RTPC_OutputBusVolume,
	RTPC_OutputBusHPF,
    RTPC_OutputBusLPF,

	RTPC_Positioning_EnableAttenuation,

	RTPC_ReflectionsVolume,

	RTPC_UserAuxSendLPF0,
	RTPC_UserAuxSendLPF1,
	RTPC_UserAuxSendLPF2,
	RTPC_UserAuxSendLPF3,

	RTPC_UserAuxSendHPF0,
	RTPC_UserAuxSendHPF1,
	RTPC_UserAuxSendHPF2,
	RTPC_UserAuxSendHPF3,

	RTPC_GameAuxSendLPF,
	RTPC_GameAuxSendHPF,

	RTPC_MaxNumRTPC
};

enum AkNonRtpcOverrides
{
	m_bOverrideMidiEventsBehavior = RTPC_MaxNumRTPC,
	m_bOverrideMidiNoteTracking,
	m_bOverrideAttachmentParams,
	m_bOverrideAnalysis,

	RTPC_BITFIELD_WIDTH
};

//AKSTATICASSERT((RTPC_MaxNumRTPC <= 64), "AkRTPC_ParameterID's must fit into a AkUInt64");
//AKSTATICASSERT((RTPC_MaxNumRTPC - RTPC_OVERRIDABLE_PARAMS_START) <= 32, "Overridable params must fit into a AkUInt32");

typedef CAkBitArray<AkUInt64, 0> AkRTPCBitArray;
typedef CAkBitArray<AkUInt32, RTPC_OVERRIDABLE_PARAMS_START> AkOverridableParamsBitArray;

enum AkAssignableMidiControl
{
	AssignableMidiControl_Invalid = 0,
	AssignableMidiControl_Start = 1,

	//
	// MIDI values controlled by CC messages.
	// NOTE: There is a +1 offset between the MIDI CC values from the midi events and this enum.
	//		This is so that AssignableMidiControl_Invalid == AK_INVALID_RTPC_ID
	AssignableMidiControl_CcStart = AssignableMidiControl_Start,
	AssignableMidiControl_CcEnd = AssignableMidiControl_CcStart+128,
	//
	// MIDI values controlled by special messages
	AssignableMidiControl_Velocity = AssignableMidiControl_CcEnd,
	AssignableMidiControl_Aftertouch,
	AssignableMidiControl_PitchBend,
	AssignableMidiControl_Frequency,
	AssignableMidiControl_Note,
	//
	AssignableMidiControl_Max
};

enum AkBuiltInParam
{
	BuiltInParam_None = 0,
	BuiltInParam_Start = 1,
	//
	BuiltInParam_Distance = BuiltInParam_Start,
	BuiltInParam_Azimuth,
	BuiltInParam_Elevation,
	BuiltInParam_EmitterCone,
	BuiltInParam_Obsruction,
	BuiltInParam_Occlusion,
	BuiltInParam_ListenerCone,
	BuiltInParam_Diffraction,
	//
	BuiltInParam_Max
};

inline bool IsMidiControl( AkRtpcID in_RtpcID )
{
	return (in_RtpcID >= AssignableMidiControl_Start && in_RtpcID < AssignableMidiControl_Max);
}

// *** These midi controls are disabled on busses -- or any globalaly scoped nodes ***
//	Busses do not have a MIDI channel.  To support MIDI cc's on busses, we
//	also set the global value each time we set a channel value.
//	If another sound has a mapping to a channel or note scoped value that has not 
//	been set, it will fall back to the global value.  This will most likely result in a
//	unintended value.
inline bool IsMidiCtrlSupportedInGlobalScope( AkAssignableMidiControl in_eMidiCtrl )
{
	return (in_eMidiCtrl >= AssignableMidiControl_Start && in_eMidiCtrl < AssignableMidiControl_CcEnd);
}

// Set of parameters related to positioning.
#define RTPC_POSITIONING_BITFIELD (\
				1ULL << RTPC_PositioningTypeBlend						|\
				1ULL << RTPC_Positioning_Divergence_Center_PCT			|\
				1ULL << RTPC_Positioning_Cone_Attenuation_ON_OFF		|\
				1ULL << RTPC_Positioning_Cone_Attenuation				|\
				1ULL << RTPC_Positioning_Cone_LPF						|\
				1ULL << RTPC_Positioning_Cone_HPF						|\
				1ULL << RTPC_Position_PAN_X_2D							|\
				1ULL << RTPC_Position_PAN_Y_2D							|\
				1ULL << RTPC_Position_PAN_X_3D							|\
				1ULL << RTPC_Position_PAN_Y_3D							|\
				1ULL << RTPC_Position_PAN_Z_3D							|\
				1ULL << RTPC_Positioning_EnableAttenuation				)

#define RTPC_USER_AUX_SEND_PARAM_BITFIELD (				\
					1ULL << RTPC_UserAuxSendVolume0 |\
					1ULL << RTPC_UserAuxSendVolume1 |\
					1ULL << RTPC_UserAuxSendVolume2 |\
					1ULL << RTPC_UserAuxSendVolume3 |\
					1ULL << RTPC_UserAuxSendLPF0 |\
					1ULL << RTPC_UserAuxSendLPF1 |\
					1ULL << RTPC_UserAuxSendLPF2 |\
					1ULL << RTPC_UserAuxSendLPF3 |\
					1ULL << RTPC_UserAuxSendHPF0 |\
					1ULL << RTPC_UserAuxSendHPF1 |\
					1ULL << RTPC_UserAuxSendHPF2 |\
					1ULL << RTPC_UserAuxSendHPF3 )

#define RTPC_GAME_AUX_SEND_PARAPM_BITFIELD ( \
					1ULL << RTPC_GameAuxSendVolume |\
					1ULL << RTPC_GameAuxSendLPF |\
					1ULL << RTPC_GameAuxSendHPF )

// Set of params that become available when a bus is in HDR mode.
#define RTPC_HDR_BUS_PARAMS_BITFIELD (\
					1ULL << RTPC_HDRBusThreshold							|\
					1ULL << RTPC_HDRBusReleaseTime							|\
					1ULL << RTPC_HDRBusRatio								)

// Set of params related to effects
#define RTPC_FX_PARAMS_BITFIELD (\
					1ULL << RTPC_BypassFX0							|\
					1ULL << RTPC_BypassFX1							|\
					1ULL << RTPC_BypassFX2							|\
					1ULL << RTPC_BypassFX3							|\
					1ULL << RTPC_BypassAllFX )

// Set of parameter related to spatial audio
#define RTPC_SPATIAL_AUDIO_BITFIELD (\
					1ULL << RTPC_ReflectionsVolume						)

// Actor-Mixer Node -only overrides.  
//	Scope: These overrides only exist in the AMH.  
//  Logic: The RTPCable parameter is active, if the override is enabled, OR if there is NO parent AM node.
#define RTPC_AM_OVERRIDE_WITH_NO_PARENT_AM_NODE_BF (						\
				1ULL << RTPC_Priority										| \
				RTPC_GAME_AUX_SEND_PARAPM_BITFIELD							| \
				1ULL << RTPC_HDRActiveRange									| \
				RTPC_FX_PARAMS_BITFIELD										| \
				RTPC_POSITIONING_BITFIELD									| \
				RTPC_USER_AUX_SEND_PARAM_BITFIELD							| \
				RTPC_SPATIAL_AUDIO_BITFIELD									)

// Common overridable (one-place-in-hierarchy) parameters.
//  Scope: These parameters exist in both the AM and the BUS hierarchy.
//	Logic: If the RTPCable parameter is active if it is explicitly overridden.  Otherwise, the parameter is active in the bus hierarchy if there is no valid output bus node, 
//		and the parameter is active in the AM hierarchy if there is no valid parent node.
#define RTPC_COMMON_OVERRIDABLE_PARAMS_BF (							\
				0ULL						)

// "Chained" parameters in bus hierarchy
//	Scope: These parameters exist in the Bus hierarchy.  They may also exist in the AM hierarchy, but with different logic.
//	Logic: These parameters can be active in multiple places in the bus hierarchy because they are applied to independent (chained) bus instances. 
#define RTPC_BUS_CHAINED_PARAMS_BF	(			\
				RTPC_FX_PARAMS_BITFIELD			| \
				RTPC_HDR_BUS_PARAMS_BITFIELD	| \
				RTPC_POSITIONING_BITFIELD		| \
				RTPC_USER_AUX_SEND_PARAM_BITFIELD	| \
				RTPC_SPATIAL_AUDIO_BITFIELD			)

// The set of parameters that a CAkPBI will register to receive notifications for.
#define RTPC_PBI_PARAMS_BITFIELD (											\
				1ULL << RTPC_Volume											| \
				1ULL << RTPC_BusVolume										| \
				1ULL << RTPC_Pitch											| \
				1ULL << RTPC_LPF											| \
				1ULL << RTPC_HPF											| \
				1ULL << RTPC_HDRActiveRange									| \
				1ULL << RTPC_MakeUpGain										| \
				1ULL << RTPC_Priority										| \
				RTPC_USER_AUX_SEND_PARAM_BITFIELD							| \
				RTPC_GAME_AUX_SEND_PARAPM_BITFIELD							| \
				1ULL << RTPC_OutputBusVolume								| \
				1ULL << RTPC_OutputBusLPF									| \
				1ULL << RTPC_OutputBusHPF									| \
				RTPC_FX_PARAMS_BITFIELD										| \
				1ULL << RTPC_MuteRatio										| \
				1ULL << RTPC_PlaybackSpeed									| \
				RTPC_POSITIONING_BITFIELD									| \
				RTPC_SPATIAL_AUDIO_BITFIELD									)

#define RTPC_MUSIC_PBI_PARAMS_BITFIELD (									\
				RTPC_PBI_PARAMS_BITFIELD & ~(1ULL << RTPC_Pitch)			)

// The set of parameters that a CAkBusVolumes (AkVPLMixBusNode.h) will register to receive notifications for.
#define RTPC_MIXBUS_PARAMS_BITFIELD (										\
				1ULL << RTPC_BusVolume										| \
				1ULL << RTPC_OutputBusVolume								| \
				1ULL << RTPC_OutputBusLPF									| \
				1ULL << RTPC_OutputBusHPF									| \
				1ULL << RTPC_HDRBusReleaseTime								| \
				1ULL << RTPC_HDRBusThreshold								| \
				RTPC_FX_PARAMS_BITFIELD										| \
				1ULL << POSID_Position_PAN_X_2D								| \
				1ULL << POSID_Position_PAN_Y_2D								| \
				1ULL << RTPC_MuteRatio										| \
				1ULL << POSID_Positioning_Divergence_Center_PCT				| \
				RTPC_USER_AUX_SEND_PARAM_BITFIELD							| \
				RTPC_POSITIONING_BITFIELD									| \
				RTPC_SPATIAL_AUDIO_BITFIELD									)

// Params that are additive and must be registered on each bus node
#define RTPC_BUS_NODE_ADDITIVE_PARAMS_BITFIELD	(							\
				1ULL << RTPC_LFE											|\
				1ULL << RTPC_Pitch											|\
				1ULL << RTPC_LPF											|\
				1ULL << RTPC_HPF											|\
				1ULL << RTPC_BusVolume										|\
				1ULL << RTPC_OutputBusVolume								| \
				1ULL << RTPC_OutputBusLPF									| \
				1ULL << RTPC_OutputBusHPF									| \
				1ULL << RTPC_Volume											|\
				1ULL << RTPC_MakeUpGain 									|\
				1ULL << RTPC_MaxNumInstances								)

// Bus params that can be collapsed to the first mixing bus upstream
#define RTPC_BUS_NODE_COLLAPSABLE_PARAMS_BITFIELD (							\
				1ULL << RTPC_BusVolume										|\
				1ULL << RTPC_OutputBusVolume								|\
				1ULL << RTPC_OutputBusLPF									|\
				1ULL << RTPC_OutputBusHPF									)

// Params that are additive and must be registered on each AM node
#define RTPC_AM_NODE_ADDITIVE_PARAMS_BITFIELD (								\
				1ULL << RTPC_LFE											| \
				1ULL << RTPC_Volume											| \
				1ULL << RTPC_Pitch											| \
				1ULL << RTPC_LPF											| \
				1ULL << RTPC_HPF											| \
				1ULL << RTPC_MakeUpGain										| \
				1ULL << RTPC_InitialDelay									| \
				1ULL << RTPC_PlaybackSpeed									| \
				1ULL << RTPC_MaxNumInstances								)



#endif //_RTPC_H_
