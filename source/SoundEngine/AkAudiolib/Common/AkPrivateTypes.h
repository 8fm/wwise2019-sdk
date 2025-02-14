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
// AkPrivateTypes.h
//
// Audiokinetic Data Type Definition (internal)
//
//////////////////////////////////////////////////////////////////////
#ifndef _AKPRIVATETYPES_H
#define _AKPRIVATETYPES_H

// Moved to SDK.
#include <AK/SoundEngine/Common/AkTypes.h>

// Below: Internal only definitions.
//----------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------
#include "AudiolibLimitations.h"

//----------------------------------------------------------------------------------------------------
// Structs.
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
// Macros.
//----------------------------------------------------------------------------------------------------

#define DECLARE_BASECLASS( baseClass )	\
	private:							\
		typedef baseClass __base		\

//----------------------------------------------------------------------------------------------------
// Enums
//----------------------------------------------------------------------------------------------------

enum AkVirtualQueueBehavior
{
//DONT ADD ANYTHING HERE, STORED ON 3 BITS!
	// See documentation for the definition of the behaviors.
	AkVirtualQueueBehavior_FromBeginning 	= 0,
	AkVirtualQueueBehavior_FromElapsedTime  = 1,
	AkVirtualQueueBehavior_Resume 			= 2
//DONT ADD ANYTHING HERE, STORED ON 3 BITS!
#define VIRTUAL_QUEUE_BEHAVIOR_NUM_STORAGE_BIT 3
};

enum AkBelowThresholdBehavior
{
//DONT ADD ANYTHING HERE, STORED ON 3 BITS!
	// See documentation for the definition of the behaviors.
	AkBelowThresholdBehavior_ContinueToPlay 	= 0,
	AkBelowThresholdBehavior_KillVoice			= 1,
	AkBelowThresholdBehavior_SetAsVirtualVoice 	= 2,
	AkBelowThresholdBehavior_KillIfOneShotElseVirtual = 3
//DONT ADD ANYTHING HERE, STORED ON 3 BITS!
#define BELOW_THRESHOLD_BEHAVIOR_NUM_STORAGE_BIT 4
};

///Transition mode selection
enum AkTransitionMode
{
//DONT ADD ANYTHING HERE, STORED ON 4 BITS!
	Transition_Disabled				= 0,	// Sounds are followed without any delay
	Transition_CrossFadeAmp			= 1,	// Sound 2 starts before sound 1 finished (constant amplitude)
	Transition_CrossFadePower		= 2,	// Sound 2 starts before sound 1 finished (constant power)
	Transition_Delay				= 3,	// There is a delay before starting sound 2 once sound 1 terminated
	Transition_SampleAccurate		= 4,	// Next sound is prepared in advance and uses the same pipeline than the previous one
	Transition_TriggerRate			= 5		// Sound 2 starts after a fixed delay
//DONT ADD ANYTHING HERE, STORED ON 4 BITS!
#define TRANSITION_MODE_NUM_STORAGE_BIT 4
};

//----------------------------------------------------------------------------------------------------
// Private types, common to Wwise and the sound engine
//----------------------------------------------------------------------------------------------------

// Type to uniquely identify something in the pipeline (sound, bus, cbxNode, ...)
typedef AkUInt32 AkPipelineID;

static const AkPipelineID AK_INVALID_PIPELINE_ID = AK_INVALID_UNIQUE_ID; // Invalid pipeline ID

// Type for game objects used in the authoring tool, and in communication.
// IMPORTANT: Never use AkGameObjectID in communication and in the tool, otherwise there would
// be incompatibilities between 32 and 64-bit versions of the sound engine and authoring tool.
typedef AkGameObjectID	AkWwiseGameObjectID;
static const AkWwiseGameObjectID WWISE_INVALID_GAME_OBJECT = AK_INVALID_GAME_OBJECT;	// Invalid game object (may also mean all game objects)

static const AkGameObjectID AkGameObjectID_ReservedStart = (AkGameObjectID)(-32);

static const AkGameObjectID AkGameObjectID_ReservedEnd = AK_INVALID_GAME_OBJECT;

// Reserved Game Object IDs
static const AkGameObjectID AkGameObjectID_Transport = (AkGameObjectID)(-2);

/// Internal structure.
class AkExternalSourceArray;
#pragma pack(push, 4)

// Pointer wrapper for the Message Queue.  This template will always take 64 bit, regardless of architecture.
// Never use a raw pointer in the Message Queue.
template<class CL>
struct AkQueueMsgPtr
{
	AkForceInline CL* operator->() const { return m_pPtr; } 
	AkForceInline operator CL*() const { return m_pPtr; }
	AkForceInline operator const CL*() const { return m_pPtr; }
	AkForceInline operator bool() const { return m_pPtr != NULL; }
	AkForceInline CL* operator=(CL* in_ptr) { m_pPtr = in_ptr; return in_ptr; }
	template<class COMP>
	AkForceInline bool operator==(const COMP c) { return reinterpret_cast<const COMP>(m_pPtr) == c; }
	template<class COMP>
	AkForceInline bool operator!=(const COMP c) { return reinterpret_cast<const COMP>(m_pPtr) != c; }
	CL* m_pPtr;
#ifndef AK_POINTER_64
	AkUInt32 dummy;
#endif
};

/// Optional parameter.
struct AkCustomParamType
{
	AkQueueMsgPtr<AkExternalSourceArray>  pExternalSrcs;	///< Reserved
	AkInt64					customParam;	///< Reserved, must be 0	
	AkUInt32				ui32Reserved;	///< Reserved, must be 0	
};

#pragma pack(pop)

// Loudness frequency weighting types.
enum AkLoudnessFrequencyWeighting
{
	FrequencyWeighting_None	= 0,
	FrequencyWeighting_K,
	FrequencyWeighting_A,
	AK_NUM_FREQUENCY_WEIGHTINGS
};



//----------------------------------------------------------------------------------------------------
// Common defines
//----------------------------------------------------------------------------------------------------

/// Oldest version of Wwise SDK supported by the Wwise Authoring compiled with this SDK.
#define AK_OLDEST_SUPPORTED_WWISESDK_VERSION ((2018<<8) | 1)

#define AK_UNMUTED_RATIO	(1.f)
#define AK_MUTED_RATIO	  	(0.f)

#define AK_DEFAULT_LEVEL_DB		(0.0f)
#define AK_MAXIMUM_VOLUME_DBFS	(0.0f)
#define AK_MINIMUM_VOLUME_DBFS	(-96.3f)

#define AK_DEFAULT_HDR_BUS_THRESHOLD			(96.f)
#define AK_DEFAULT_HDR_BUS_RATIO				(16.f)	// 16:1 = infinity
#define AK_DEFAULT_HDR_BUS_RELEASE_TIME			(1.f)	// seconds
#define AK_DEFAULT_HDR_ACTIVE_RANGE				(12.f)	// dB
#define AK_DEFAULT_HDR_BUS_GAME_PARAM_MIN		(0.f)
#define AK_DEFAULT_HDR_BUS_GAME_PARAM_MAX		(200.f)

#define AK_LOUDNESS_BIAS				(14.125375f)	// 23 dB

#define AK_EVENTWITHCOOKIE_RESERVED_BIT 0x00000001
#define AK_EVENTFROMWWISE_RESERVED_BIT	0x40000000

//ID Bit that can be used only by audionodes created by the sound engine.
#define AK_SOUNDENGINE_RESERVED_BIT (0x80000000)

#define DOUBLE_WEIGHT_TO_INT_CONVERSION_FACTOR	1000
#define DEFAULT_RANDOM_WEIGHT					(50*DOUBLE_WEIGHT_TO_INT_CONVERSION_FACTOR)
#define DOUBLE_WEIGHT_TO_INT( _in_double_val_ ) ( (AkUInt32)(_in_double_val_*DOUBLE_WEIGHT_TO_INT_CONVERSION_FACTOR) )

#define DEFAULT_PROBABILITY		100

#define NO_PLAYING_ID			0

#define AK_DEFAULT_PITCH						(0)
#define AK_DEFAULT_PLAYBACK_SPEED				(1.f)
#define AK_MIN_PLAYBACK_SPEED					(0.25f)
#define AK_MAX_PLAYBACK_SPEED					(4.f)
#define AK_LOWER_MIN_DISTANCE					(0.001f)
#define AK_UPPER_MAX_DISTANCE					(10000000000.0f)

#define AK_DEFAULT_DISTANCE_FACTOR				(1.0f)

// cone

#define AK_OMNI_INSIDE_CONE_ANGLE				(TWOPI)
#define AK_OMNI_OUTSIDE_CONE_ANGLE				(TWOPI)
#define AK_DEFAULT_OUTSIDE_VOLUME				(-10.0f)

//(<255) Stored on AkUInt8 
#define	AK_MIN_LOPASS_VALUE						(0)
#define	AK_MAX_LOPASS_VALUE						(100) 
#define	AK_DEFAULT_LOPASS_VALUE					(0) 

#define AK_MIN_PAN_RL_VALUE						(-100)
#define AK_MAX_PAN_RL_VALUE						(100)
#define AK_DEFAULT_PAN_RL_VALUE					(0)

#define AK_MIN_PAN_FR_VALUE						(-100)
#define AK_MAX_PAN_FR_VALUE						(100)
#define AK_DEFAULT_PAN_FR_VALUE					(0)

// Sample accurate stopping.
#define AK_NO_IN_BUFFER_STOP_REQUESTED			(AK_UINT_MAX)

// Bus ducking.
#define AK_DEFAULT_MAX_BUS_DUCKING				(-96.3f)	// Arbitrary. Should be -INF.


// AK::SoundEngine::GetIDFromString("No_Output")
#define AK_NO_OUTPUT_SHARESET_ID				2317455096

// To combine audio device shareset id and device id.
#define AK_MAKE_DEVICE_KEY(_type, _id) (((AkUInt64)_id << 32) | _type)

#define AK_DECOMPOSE_DEVICE_KEY(_deviceKey, _type, _id) \
	{						\
		_type = (AkUInt32)(_deviceKey); \
		_id = (AkUInt32)(_deviceKey >> 32); \
	}

// Built-in plugin ID.  These are persisted, do not change the order or the numbers!
enum AkBuiltInSinks
{
	AKPLUGINID_DEFAULT_SINK = 174,
	AKPLUGINID_DVR_SINK,			//DVR output on XB1, PS4
	AKPLUGINID_COMMUNICATION_SINK,	//Chat on XB1, PS4, Windows, Stadia
	AKPLUGINID_PERSONAL_SINK,		//Controller Headset on XB1 or PS4
	AKPLUGINID_VOICE_SINK,			//OBSOLETE
	AKPLUGINID_PAD_SINK,			//Controller Speaker on PS4.
	AKPLUGINID_AUX_SINK,
	AKPLUGINID_DUMMY_SINK
};

struct AkPropValue
{
	union
	{
		AkReal32 fValue;
		AkInt32  iValue;
	};
	AkPropValue() : fValue(0.0f) {}
	AkPropValue(AkReal32 in_fValue) : fValue(in_fValue) {}
	AkPropValue(AkInt32 in_iValue) : iValue(in_iValue) {}
	void GetValue(AkReal32 &o_fValue) { o_fValue = fValue; }
	void GetValue(AkInt32& o_iValue) { o_iValue = iValue; }
};


enum AkValueMeaning
{
	AkValueMeaning_Default = 0,		//Use default parameter instead
	AkValueMeaning_Independent = 1,		//Use this parameter directly (also means override when on an object)
	AkValueMeaning_Offset = 2			//Use this parameter as an offset from the default value
#define VALUE_MEANING_NUM_STORAGE_BIT 4
};


enum AkDeltaType
{
	AkDelta_None,

	// High levels packets
	AkDelta_Pipeline, // Associated with a pipeline ID
	AkDelta_EmitterListener, // Associated with an emitter, then listener(s)
	AkDelta_MultiUpdate, // A parameter that affects multiple targets
	AkDelta_MultiUnset, // A parameter that needs to be removed on possibly multiple targets
	AkDelta_CachePrecomputedParams, // Cached parameters to reuse in the children

	// Pipeline packets
	AkDelta_BaseValue,
	AkDelta_RTPC,
	AkDelta_Switch,
	AkDelta_State,
	AkDelta_Event,
	AkDelta_Mute,
	AkDelta_RangeMod,
	AkDelta_Ducking,
	AkDelta_AuxUserDef,
	AkDelta_Distance,
	AkDelta_Cone,
	AkDelta_Occlusion,
	AkDelta_Obstruction,
	AkDelta_HDRGain,
	AkDelta_MusicSegmentEnvelope,
	AkDelta_Modulator,
	AkDelta_BGMMute,
	AkDelta_Fade,
	AkDelta_Pause,
	AkDelta_Rays,
	AkDelta_RaysFilter,
	AkDelta_LiveEdit,
	AkDelta_UsePrecomputedParams, // Send when we should use the cached precomputed params
	AkDelta_SetGameObjectAuxSendValues,
	AkDelta_PortalCrossfade,
	AkDelta_Normalization,

	// Emitter-listener packets
	AkDelta_SetGameObjectOutputBusVolume,
	AkDelta_SetEarlyReflectionsVolume,
	AkDelta_GameObjectDestroyed, // this is also a high-level packet

	AkDelta_RTPCGlobal,		// Temporary the last one to help with backward compatibilty
	AkDelta_ModulatorGlobal,// Temporary the last one to help with backward compatibilty

	AkDelta_Count
};

#define AKDELTA_RTPC_PACKET_SIZE (sizeof(AkUniqueID) + sizeof(AkUInt32) + sizeof(AkUniqueID) + sizeof(AkUInt8) + sizeof(AkReal32))

// NOT thread safe. This is used with the define MSTC_SYSTEMATIC_MEMORY_STRESS to systematic test memory allocation failure.
#ifdef MSTC_SYSTEMATIC_MEMORY_STRESS

class AkAutoDenyAllocFailure {
public:

	AkAutoDenyAllocFailure();
	~AkAutoDenyAllocFailure();
};
#endif

#endif // _AKPRIVATETYPES_H
