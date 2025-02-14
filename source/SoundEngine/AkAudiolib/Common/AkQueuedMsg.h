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

#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include "PrivateStructures.h"
#include <AK/Tools/Common/AkAssert.h>

class CAkDynamicSequence;
class CAkEvent;

enum AkQueuedMsgType
{
    QueuedMsgType_EndOfList,
	QueuedMsgType_Event,
	QueuedMsgType_RTPC,
	QueuedMsgType_RTPCWithTransition,
	QueuedMsgType_ResetRTPC,
	QueuedMsgType_ResetRTPCValue,
	QueuedMsgType_ResetRTPCValueWithTransition,
	QueuedMsgType_State,
	QueuedMsgType_Switch,
	QueuedMsgType_ResetSwitches,
	QueuedMsgType_Trigger,
	QueuedMsgType_RegisterGameObj,
	QueuedMsgType_UnregisterGameObj,
	QueuedMsgType_GameObjPosition,
	QueuedMsgType_GameObjActiveListeners,
	QueuedMsgType_GameObjActiveControllers,
	QueuedMsgType_DefaultActiveListeners,
	QueuedMsgType_ResetListeners,
	QueuedMsgType_ListenerSpatialization,
	QueuedMsgType_GameObjEnvValues,
	QueuedMsgType_GameObjDryLevel,
	QueuedMsgType_GameObjObstruction,
	QueuedMsgType_GameObjMultipleObstruction,

	QueuedMsgType_SetOutputVolume,

	QueuedMsgType_OpenDynamicSequence,
	QueuedMsgType_DynamicSequenceCmd,

	QueuedMsgType_KillBank,
	QueuedMsgType_StopAll,

	QueuedMsgType_AddOutput,
	QueuedMsgType_RemoveOutput,
	QueuedMsgType_ReplaceOutput,
	QueuedMsgType_SetBusDevice,

	QueuedMsgType_StopPlayingID,
	QueuedMsgType_EventAction,
	QueuedMsgType_EventPostMIDI,
	QueuedMsgType_EventStopMIDI,
	QueuedMsgType_LockUnlockStreamCache,
	QueuedMsgType_GameObjScalingFactor,
	QueuedMsgType_GameObjMultiPosition,
	QueuedMsgType_Seek,
    QueuedMsgType_SourcePluginAction,
	QueuedMsgType_StartStopOutputCapture,
	QueuedMsgType_AddOutputCaptureMarker,

	QueuedMsgType_SetEffect,
	QueuedMsgType_SetMixer,
	QueuedMsgType_SetBusConfig,
	QueuedMsgType_SetPanningRule,
	QueuedMsgType_SetSpeakerAngles,
	QueuedMsgType_SetProcessMode, 
	QueuedMsgType_SendMainOutputToDevice,

	QueuedMsgType_SetRandomSeed,
	QueuedMsgType_MuteBackgroundMusic,
	QueuedMsgType_PluginCustomGameData,
	
	QueuedMsgType_SuspendOrWakeup,	
	QueuedMsgType_InitSinkPlugin,

	QueuedMsgType_ApiExtension,

	QueuedMsgType_PlayingIDAction,

	QueuedMsgType_DynamicSequenceSeek,

	// WARNING : When adding a new message type, also add the matching string to s_cApiFunctionToText.

	QueuedMsgType_QueueWrapAround,		//Tag to let the reader thread (audio thread) that the rest of the message buffer is unused.
	QueuedMsgType_Invalid				//Tag to let the reader thread that the message was cancelled or incomplete after the reservation.
};

#pragma pack(push, 4)

struct AkQueuedMsg_GameObjectBase
	// This structure is not allowed to inherit any other class, GameObject is expected first.
{
	AkGameObjectID		gameObjID;		// Associated game object.  Always 64 bits because the API profiling needs structures of fixed size between 32 and 64 bits architectures.
};

struct AkQueuedMsg_EventBase
	:public AkQueuedMsg_GameObjectBase
{
	AkPlayingID			PlayingID;		// Playing ID
	AkPlayingID			TargetPlayingID;// Playing ID affected, not current!
	AkCustomParamType	CustomParam;
};

struct AkQueuedMsg_EventRef
{
	AkQueueMsgPtr<CAkEvent> pEvent;			// Pointer to the event
	AkUniqueID			eventID;		// ID of the event (knowingly used only for profiling at the moment.)
};

struct AkQueuedMsg_Event
	: public AkQueuedMsg_EventBase
	, public AkQueuedMsg_EventRef
{
};

enum AkSourcePluginActionType
{
    AkSourcePluginActionType_Play,
    AkSourcePluginActionType_Stop
};

struct AkQueuedMsg_SourcePluginAction
	: public AkQueuedMsg_EventBase
{
	AkUInt32			        PluginID;		// ID of the plug-in to instantiate.
    AkUInt32                    CompanyID;      // Company ID of the plug-i to instantiate.
    AkSourcePluginActionType    ActionType;
};

struct AkQueuedMsg_EventAction
	: public AkQueuedMsg_GameObjectBase
	, public AkQueuedMsg_EventRef
{
	AK::SoundEngine::AkActionOnEventType eActionToExecute;	// Action to be executed on the event
	AkTimeMs			uTransitionDuration;
	AkCurveInterpolation eFadeCurve;
	AkPlayingID			TargetPlayingID;
};

struct AkQueuedMsg_EventPostMIDI
	: public AkQueuedMsg_GameObjectBase
	, public AkQueuedMsg_EventRef
{
	AkUInt32			uNumPosts;
	AkMIDIPost			aPosts[1];
};

struct AkQueuedMsg_EventStopMIDI
	: public AkQueuedMsg_GameObjectBase
	, public AkQueuedMsg_EventRef
{
};

struct AkQueuedMsg_LockUnlockStreamCache
	: public AkQueuedMsg_GameObjectBase
	, public AkQueuedMsg_EventRef
{
	AkPriority			ActivePriority;
	AkPriority			InactivePriority;
	AkUInt16				Lock;				// To lock or to unlock
};

struct AkQueuedMsg_Rtpc
	:public AkQueuedMsg_GameObjectBase
{
	AkRtpcID		ID;
	AkRtpcValue		Value;
	AkPlayingID		PlayingID;
};

struct AkQueuedMsg_RtpcWithTransition : public AkQueuedMsg_Rtpc
{
	TransParamsBase		transParams;
};

struct AkQueuedMsg_State
{
	AkStateGroupID  StateGroupID;
	AkStateID		StateID;
	AkUInt16			bSkipTransition;
    AkUInt16			bSkipExtension;
};

struct AkQueuedMsg_Switch
	:public AkQueuedMsg_GameObjectBase
{
	AkSwitchGroupID SwitchGroupID;
	AkSwitchStateID SwitchStateID;
};

struct AkQueuedMsg_Trigger
	:public AkQueuedMsg_GameObjectBase
{
	AkTriggerID		TriggerID;	
};

// This is a variable sized array and must be the last member in the message.
struct AkQueuedMsg_ListenerIDs
{
	void SetListeners(const AkGameObjectID* in_pListenerObjs, AkUInt32 in_uNumListeners, AkListenerOp in_op)
	{
		uOp = (AkUInt32)in_op;
		uNumListeners = in_uNumListeners;
		for (AkUInt16 i = 0; i < in_uNumListeners; ++i)
			aListenerIDs[i].gameObjID = in_pListenerObjs[i];
	}

	AkUInt32 uOp;
	AkUInt32 uNumListeners;
	AkQueuedMsg_GameObjectBase aListenerIDs[1];
};

struct AkQueuedMsg_RegisterGameObj
	:public AkQueuedMsg_GameObjectBase
{

	AkQueueMsgPtr<void>	pMonitorData; // Monitor data, allocated in game thread, to be used in audio thread if registration successful
};

struct AkQueuedMsg_UnregisterGameObj
	:public AkQueuedMsg_GameObjectBase
{
};

struct AkQueuedMsg_GameObjPosition
	:public AkQueuedMsg_GameObjectBase
{
	AkSoundPosition Position;
};

struct AkQueuedMsg_GameObjMultiplePosition
	:public AkQueuedMsg_GameObjectBase
{
	AkUInt32							uNumPositions;
	AK::SoundEngine::MultiPositionType	eMultiPositionType;
	AkChannelEmitter					aMultiPosition[1];
};

struct AkQueuedMsg_GameObjActiveListeners
	:	public AkQueuedMsg_GameObjectBase, 
		public AkQueuedMsg_ListenerIDs
{
};

struct AkQueuedMsg_GameObjActiveControllers
	:public AkQueuedMsg_GameObjectBase
{
	AkUInt32		uActiveControllerMask;
};

struct AkQueuedMsg_DefaultActiveListeners: public AkQueuedMsg_ListenerIDs
{
};

struct AkQueuedMsg_ResetListeners : public AkQueuedMsg_GameObjectBase
{
};

struct AkQueuedMsg_ListenerSpatialization
{
	AkQueuedMsg_GameObjectBase	listener;
	AkQueueMsgPtr<AkReal32> pVolumes;
	AkUInt32		uChannelConfig;
	AkUInt16            bSpatialized;
	AkUInt16            bSetVolumes;
};

struct AkQueuedMsg_AddOutput : public AkQueuedMsg_ListenerIDs
{	
	AkUInt8				pSettings[sizeof(AkOutputSettings)];
};

struct AkQueuedMsg_RemoveOutput
{
	AkOutputDeviceID	idOutput;
};

struct AkQueuedMsg_ReplaceOutput
{
	AkUInt8				pSettings[sizeof(AkOutputSettings)];
	AkOutputDeviceID	idOutput;
};

struct AkQueuedMsg_SetBusDevice
{
	AkUniqueID idBus;
	AkUniqueID idDevice;
};

struct AkQueuedMsg_GameObjEnvValues
	:public AkQueuedMsg_GameObjectBase
{
	AkUInt32		uNumValues;
	AkAuxSendValue	pEnvValues[1];
};

struct AkQueuedMsg_GameObjDryLevel
{
	AkQueuedMsg_GameObjectBase	emitter;
	AkQueuedMsg_GameObjectBase	listener;
	AkReal32		fValue;
};

struct AkQueuedMsg_GameObjScalingFactor
	:public AkQueuedMsg_GameObjectBase
{
	AkReal32		fValue;
};

struct AkQueuedMsg_Seek
	: public AkQueuedMsg_GameObjectBase
	, public AkQueuedMsg_EventRef
{	
	union
	{
		AkTimeMs		iPosition;
		AkReal32		fPercent;
	};
	AkPlayingID			playingID;
	AkUInt16			bIsSeekRelativeToDuration;
	AkUInt16			bSnapToMarker;
};

struct AkQueuedMsg_GameObjObstruction
{
	AkQueuedMsg_GameObjectBase	emitter;
	AkQueuedMsg_GameObjectBase	listener;
	AkReal32		fObstructionLevel;
	AkReal32		fOcclusionLevel;
};

struct AkQueuedMsg_GameObjMultipleObstruction
{
	AkQueuedMsg_GameObjectBase emitter;
	AkQueuedMsg_GameObjectBase listener;
	AkUInt32 uNumObstructionOcclusionValues;
	AkObstructionOcclusionValues pObstructionOcclusionValues[1];
};

struct AkQueuedMsg_ResetSwitches
	:public AkQueuedMsg_GameObjectBase
{
};

struct AkQueuedMsg_ResetRTPC
	:public AkQueuedMsg_GameObjectBase
{
	AkPlayingID		PlayingID;
};

struct AkQueuedMsg_ResetRTPCValue
	:public AkQueuedMsg_GameObjectBase
{
	AkUInt32		ParamID;
	AkPlayingID		PlayingID;
};

struct AkQueuedMsg_ResetRtpcValueWithTransition : public AkQueuedMsg_ResetRTPCValue
{
	TransParamsBase		transParams;
};

struct AkQueuedMsg_SetOutputVolume
{
	AkOutputDeviceID idDevice;	
	AkReal32	fVolume;
};

struct AkQueuedMsg_OpenDynamicSequence
	: public AkQueuedMsg_EventBase
{
	AkQueueMsgPtr<CAkDynamicSequence>	pDynamicSequence;
};

class CAkUsageSlot;

struct AkQueuedMsg_KillBank
{
	AkQueueMsgPtr<CAkUsageSlot> pUsageSlot;
};

struct AkQueuedMsg_StopAll
	:public AkQueuedMsg_GameObjectBase
{
};

struct AkQueuedMsg_StopPlayingID
{
	AkPlayingID				playingID;
	AkTimeMs				uTransitionDuration;
	AkCurveInterpolation	eFadeCurve;
};

struct AkQueuedMsg_PlayingIDAction
{
	AK::SoundEngine::AkActionOnEventType eActionToExecute;	// Action to be executed on the playingID
	AkPlayingID				playingID;
	AkTimeMs				uTransitionDuration; // ??
	AkCurveInterpolation	eFadeCurve; // ??
};

struct AkQueuedMsg_DynamicSequenceCmd
{
	enum Command
	{
		Play,
		Pause,
		Resume,
		Close,
		Stop,
		Break,
		ResumeWaiting
	};

	AkQueueMsgPtr<CAkDynamicSequence> pDynamicSequence;
	Command						eCommand;
	AkTimeMs					uTransitionDuration;
	AkCurveInterpolation		eFadeCurve;
};

struct AkQueuedMsg_DynamicSequenceSeek
{
	AkQueueMsgPtr<CAkDynamicSequence> pDynamicSequence;
	union
	{
		AkTimeMs		iSeekTime;
		AkReal32		fSeekPercent;
	};
	AkUInt8			bIsSeekRelativeToDuration;
	AkUInt8			bSnapToNearestMarker;
};

struct AkQueuedMsg_StartStopCapture
{
	AkQueueMsgPtr<AkOSChar> szFileName;
};

struct AkQueuedMsg_AddOutputCaptureMarker
{
	AkQueueMsgPtr<char> szMarkerText;	
};

struct AkQueuedMsg_SetEffect
{
	AkUniqueID	audioNodeID;
	AkUInt32	uFXIndex;
	AkUniqueID	shareSetID;
	AkNodeType  eNodeType;
};

struct AkQueuedMsg_SetBusConfig
{
	AkUniqueID	audioNodeID;
	AkUInt32	uChannelConfig;
};

struct AkQueuedMsg_SetPanningRule
{
	AkOutputDeviceID idOutput;
	AkPanningRule	panRule;
};

struct AkQueuedMsg_SetSpeakerAngles
{
	AkQueueMsgPtr<AkReal32>	pfSpeakerAngles;
	AkUInt32		uNumAngles;
	AkReal32 		fHeightAngle;	
	AkOutputDeviceID idOutput;
};

struct AkQueuedMsg_SetProcessMode
{
	AkUInt32 eStatus;
	AkUInt32 msFade;
	AkCurveInterpolation eCurve;
	AkQueueMsgPtr<void> pUserData;
	void(*Callback)(void*);	
};

struct AkQueuedMsg_SetRandomSeed
{
	AkUInt32 uSeed;
};

struct AkQueuedMsg_MuteBackgroundMusic
{
	AkUInt16 bMute;
};

struct AkQueuedMsg_PluginCustomGameData
{
	// Routing
	AkUniqueID	busID;
	AkGameObjectID busObjectID;
	AkPluginType eType;
	AkUInt32 uCompanyID;
	AkUInt32 uPluginID;
	
	//Data
	AkQueueMsgPtr<void>	pData;
	AkUInt32	uSizeInBytes;
};

struct AkQueuedMsg_EndOfList
{
	AkUInt32 uFrameNumber;
};

struct AkQueuedMsg_Suspend
{	
	AkUInt32 uDelay;
	AkUInt16 bSuspend;
	AkUInt16 bRender;
};

struct AkQueuedMsg_ApiExtension
{
	AkUInt32 uID;
};

struct AkQueuedMsg_InitSinkPlugin
{
	//Dummies to take space
	//Should be the same size as AkOutputSettings.  Enforced at runtime.
	char buffer[sizeof(AkOutputSettings)];
};

#pragma pack(pop)

// ATTENTION!  Rules about Queued Messages structures:
// These structures are now shared between the sound engine and the authoring, for the API profiling.
// This means that you MUST reflect your changes in the MonitoringMgrItem.cpp file if:
// - You added/removed a new message type
// - You added/removed parameters in a message
//
// Also, they must be "sufficiently" packed i.e. the offset of the members doesn't change 
// between 32/64 versions and from compiler to compiler.  This means:
// - No raw pointers.  Use AkQueuedMsgPtr<> instead
// - No type smaller than 32 bits unless it is the last, or you hand-packed multiple small types so it rounds up to 4 bytes.
//
// Asserts have been put in place for this, don't ignore them :)
// Variable sized messages (with strings, for example) are exempt from safety checking, so beware!

struct AkQueuedMsg
{
	AkQueuedMsg( AkUInt16 in_type )
		:type( in_type )
	{}

	AkUInt16 size;
	AkUInt16 type;

	union
	{
        AkQueuedMsg_Event	event;
		AkQueuedMsg_Rtpc	rtpc;
		AkQueuedMsg_RtpcWithTransition	rtpcWithTransition;
		AkQueuedMsg_State   setstate;
		AkQueuedMsg_Switch	setswitch;
		AkQueuedMsg_Trigger trigger;
		AkQueuedMsg_RegisterGameObj reggameobj;
		AkQueuedMsg_UnregisterGameObj unreggameobj;
		AkQueuedMsg_GameObjPosition gameobjpos;
		AkQueuedMsg_GameObjMultiplePosition gameObjMultiPos;
		AkQueuedMsg_GameObjActiveListeners gameobjactlist;
		AkQueuedMsg_GameObjActiveControllers gameobjactcontroller;
		AkQueuedMsg_DefaultActiveListeners defaultactlist;
		AkQueuedMsg_ResetListeners resetListeners;
		AkQueuedMsg_ListenerSpatialization listspat;
		AkQueuedMsg_GameObjEnvValues gameobjenvvalues;
		AkQueuedMsg_GameObjDryLevel gameobjdrylevel;
		AkQueuedMsg_GameObjObstruction gameobjobstr;
		AkQueuedMsg_GameObjMultipleObstruction gameobjmultiobstr;
		AkQueuedMsg_ResetSwitches resetswitches;
		AkQueuedMsg_ResetRTPC resetrtpc;
		AkQueuedMsg_ResetRTPCValue resetrtpcvalue;
		AkQueuedMsg_ResetRtpcValueWithTransition resetrtpcvalueWithTransition;
		AkQueuedMsg_SetOutputVolume setoutputvolume;
		AkQueuedMsg_OpenDynamicSequence opendynamicsequence;
		AkQueuedMsg_DynamicSequenceCmd dynamicsequencecmd;
		AkQueuedMsg_DynamicSequenceSeek dynamicsequenceSeek;
		AkQueuedMsg_KillBank killbank;
		AkQueuedMsg_AddOutput addOutput;
		AkQueuedMsg_RemoveOutput removeOutput;
		AkQueuedMsg_ReplaceOutput replaceOutput;
		AkQueuedMsg_SetBusDevice setBusDevice;
		AkQueuedMsg_StopAll stopAll;
		AkQueuedMsg_StopPlayingID stopEvent;
		AkQueuedMsg_PlayingIDAction playingIDAction;
		AkQueuedMsg_EventAction eventAction;
		AkQueuedMsg_EventPostMIDI postMIDI;
		AkQueuedMsg_EventStopMIDI stopMIDI;
		AkQueuedMsg_LockUnlockStreamCache lockUnlockStreamCache;
		AkQueuedMsg_GameObjScalingFactor gameobjscalingfactor;
		AkQueuedMsg_Seek seek;
        AkQueuedMsg_SourcePluginAction sourcePluginAction;
		AkQueuedMsg_StartStopCapture outputCapture;
		AkQueuedMsg_AddOutputCaptureMarker captureMarker;
		AkQueuedMsg_SetEffect setEffect;
		AkQueuedMsg_SetBusConfig setBusConfig;
		AkQueuedMsg_SetPanningRule setPanningRule;
		AkQueuedMsg_SetSpeakerAngles setSpeakerAngles;
		AkQueuedMsg_SetProcessMode processMode;
		AkQueuedMsg_SetRandomSeed randomSeed;
		AkQueuedMsg_MuteBackgroundMusic muteBGM;
		AkQueuedMsg_PluginCustomGameData pluginCustomGameData;
		AkQueuedMsg_EndOfList endOfList;		
		AkQueuedMsg_Suspend suspend;
		AkQueuedMsg_ApiExtension apiExtension;	
		AkQueuedMsg_InitSinkPlugin initSink;		

		AkQueuedMsg_GameObjectBase _GameObjectHandle;// not a message in itself, but is an easy way to access GameObejct on all those who implement it.
	};

#define BASE_AKQUEUEMSGSIZE offsetof( AkQueuedMsg, event )
#define Sizeof_QueueWrapAround BASE_AKQUEUEMSGSIZE

#define DCL_MSG_OBJECT_SIZE( _NAME_, _OBJECT_ ) \
static AkUInt16 _NAME_()

	static AkUInt16 Sizeof_GameObjMultiPositionBase();
	static AkUInt16 Sizeof_GameObjMultiObstructionBase();
	static AkUInt16 Sizeof_EventPostMIDIBase();

	DCL_MSG_OBJECT_SIZE( Sizeof_Event,						AkQueuedMsg_Event );
	DCL_MSG_OBJECT_SIZE( Sizeof_Rtpc,						AkQueuedMsg_Rtpc );
	DCL_MSG_OBJECT_SIZE( Sizeof_RtpcWithTransition,			AkQueuedMsg_RtpcWithTransition );
	DCL_MSG_OBJECT_SIZE( Sizeof_State,						AkQueuedMsg_State );
	DCL_MSG_OBJECT_SIZE( Sizeof_Switch,						AkQueuedMsg_Switch );
	DCL_MSG_OBJECT_SIZE( Sizeof_Trigger,					AkQueuedMsg_Trigger );
	DCL_MSG_OBJECT_SIZE( Sizeof_RegisterGameObj,			AkQueuedMsg_RegisterGameObj );
	DCL_MSG_OBJECT_SIZE( Sizeof_UnregisterGameObj,			AkQueuedMsg_UnregisterGameObj );
	DCL_MSG_OBJECT_SIZE( Sizeof_GameObjPosition,			AkQueuedMsg_GameObjPosition );
	DCL_MSG_OBJECT_SIZE( Sizeof_GameObjActiveControllers,	AkQueuedMsg_GameObjActiveControllers );
	DCL_MSG_OBJECT_SIZE( Sizeof_DefaultActiveListeners, 	AkQueuedMsg_DefaultActiveListeners );
	DCL_MSG_OBJECT_SIZE( Sizeof_ResetListeners,				AkQueuedMsg_ResetListeners);
	DCL_MSG_OBJECT_SIZE( Sizeof_ListenerSpatialization, 	AkQueuedMsg_ListenerSpatialization );
	DCL_MSG_OBJECT_SIZE( Sizeof_GameObjEnvValues,			AkQueuedMsg_GameObjEnvValues );
	DCL_MSG_OBJECT_SIZE( Sizeof_GameObjDryLevel,			AkQueuedMsg_GameObjDryLevel );
	DCL_MSG_OBJECT_SIZE( Sizeof_GameObjObstruction, 		AkQueuedMsg_GameObjObstruction );
	DCL_MSG_OBJECT_SIZE( Sizeof_GameObjMultipleObstruction, AkQueuedMsg_GameObjMultipleObstruction );
	DCL_MSG_OBJECT_SIZE( Sizeof_ResetSwitches,				AkQueuedMsg_ResetSwitches );
	DCL_MSG_OBJECT_SIZE( Sizeof_ResetRTPC,					AkQueuedMsg_ResetRTPC );
	DCL_MSG_OBJECT_SIZE( Sizeof_ResetRTPCValue,				AkQueuedMsg_ResetRTPCValue );
	DCL_MSG_OBJECT_SIZE( Sizeof_ResetRTPCValueWithTransition, AkQueuedMsg_ResetRtpcValueWithTransition );
	DCL_MSG_OBJECT_SIZE( Sizeof_SetOutputVolume,	AkQueuedMsg_SetOutputVolume );
	DCL_MSG_OBJECT_SIZE( Sizeof_OpenDynamicSequence,		AkQueuedMsg_OpenDynamicSequence );
	DCL_MSG_OBJECT_SIZE( Sizeof_DynamicSequenceCmd, 		AkQueuedMsg_DynamicSequenceCmd );
	DCL_MSG_OBJECT_SIZE(Sizeof_DynamicSequenceSeek, AkQueuedMsg_DynamicSequenceSeek);
	DCL_MSG_OBJECT_SIZE( Sizeof_KillBank,					AkQueuedMsg_KillBank );
	DCL_MSG_OBJECT_SIZE( Sizeof_StopAll,					AkQueuedMsg_StopAll );
	DCL_MSG_OBJECT_SIZE( Sizeof_StopPlayingID,				AkQueuedMsg_StopPlayingID );
	DCL_MSG_OBJECT_SIZE( Sizeof_PlayingIDAction,			AkQueuedMsg_PlayingIDAction);
	DCL_MSG_OBJECT_SIZE( Sizeof_EventAction,				AkQueuedMsg_EventAction );
	DCL_MSG_OBJECT_SIZE( Sizeof_EventPostMIDI,				AkQueuedMsg_EventPostMIDI );
	DCL_MSG_OBJECT_SIZE( Sizeof_EventStopMIDI,				AkQueuedMsg_EventStopMIDI );
	DCL_MSG_OBJECT_SIZE( Sizeof_LockUnlockStreamCache,		AkQueuedMsg_LockUnlockStreamCache );
	DCL_MSG_OBJECT_SIZE( Sizeof_GameObjScalingFactor,		AkQueuedMsg_GameObjScalingFactor );
	DCL_MSG_OBJECT_SIZE( Sizeof_Seek,						AkQueuedMsg_Seek );
    DCL_MSG_OBJECT_SIZE( Sizeof_PlaySourcePlugin,		    AkQueuedMsg_SourcePluginAction );
	DCL_MSG_OBJECT_SIZE( Sizeof_StartStopCapture,			AkQueuedMsg_StartStopCapture );
	DCL_MSG_OBJECT_SIZE( Sizeof_AddOutputCaptureMarker,		AkQueuedMsg_AddOutputCaptureMarker );
	DCL_MSG_OBJECT_SIZE( Sizeof_SetEffect,					AkQueuedMsg_SetEffect );
	DCL_MSG_OBJECT_SIZE( Sizeof_SetBusConfig,				AkQueuedMsg_SetBusConfig);
	DCL_MSG_OBJECT_SIZE( Sizeof_SetPanningRule,				AkQueuedMsg_SetPanningRule );
	DCL_MSG_OBJECT_SIZE( Sizeof_SetSpeakerAngles,			AkQueuedMsg_SetSpeakerAngles );
	DCL_MSG_OBJECT_SIZE( Sizeof_SetProcessMode,				AkQueuedMsg_SetProcessMode );
	DCL_MSG_OBJECT_SIZE( Sizeof_SetRandomSeed,				AkQueuedMsg_SetRandomSeed );
	DCL_MSG_OBJECT_SIZE( Sizeof_MuteBackgroundMusic,		AkQueuedMsg_MuteBackgroundMusic);
	DCL_MSG_OBJECT_SIZE( Sizeof_PluginCustomGameData,		AkQueuedMsg_PluginCustomGameData );
	DCL_MSG_OBJECT_SIZE( Sizeof_EndOfList,					AkQueuedMsg_EndOfList );

	DCL_MSG_OBJECT_SIZE( Sizeof_Suspend,					AkQueuedMsg_FromSuspend );
	DCL_MSG_OBJECT_SIZE( Sizeof_InitSinkPlugin,				AkQueuedMsg_InitSinkPlugin );
	DCL_MSG_OBJECT_SIZE( Sizeof_ApiExtension,				AkQueuedMsg_ApiExtension );

	DCL_MSG_OBJECT_SIZE( Sizeof_AddRemoveBusToOutput,		AkQueuedMsg_AddRemoveBusToOutput );
	DCL_MSG_OBJECT_SIZE( Sizeof_SetBusDevice,				AkQueuedMsg_SetBusDevice );
	DCL_MSG_OBJECT_SIZE( Sizeof_ReplaceOutput,				AkQueuedMsg_ReplaceOutput);


	template <typename T> // T must inherit from AkQueuedMsg_ListenerIDs
	static AkUInt16 Sizeof_ListenerIDMsg(AkUInt32 in_uNumListeners)
	{
		return (AkUInt16)(BASE_AKQUEUEMSGSIZE + sizeof(T)
			+ (sizeof(AkQueuedMsg_GameObjectBase)* in_uNumListeners));
	}

};

static const AkOSChar* s_cApiFunctionToText[] =
{
	AKTEXT("RenderAudio"),
	AKTEXT("PostEvent"),
	AKTEXT("SetRTPCValue"), // ?
	AKTEXT("SetRTPCValueWithTransition"),
	AKTEXT("ResetRTPC"),
	AKTEXT("ResetRTPCValue"),
	AKTEXT("ResetRTPCValueWithTransition"), // ?
	AKTEXT("SetState"),
	AKTEXT("SetSwitch"),
	AKTEXT("ResetSwitches"),
	AKTEXT("PostTrigger"),
	AKTEXT("RegisterGameObj"),
	AKTEXT("UnregisterGameObj"), // Unregister All Game Objects ?
	AKTEXT("SetPosition"),
	AKTEXT(""),//Add/Remove/SetListeners, resolved in description details.
	AKTEXT("SetActiveControllers"),
	AKTEXT(""),//Add/Remove/SetDefaultListeners, resolved in description details.
	AKTEXT("ResetListenersToDefault"),
	AKTEXT("SetListenerSpatialization"),
	AKTEXT("SetGameObjectAuxSend"),
	AKTEXT("SetGameObjectOutputBusVolume"),
	AKTEXT("SetObjectObstructionAndOcclusion"),
	AKTEXT("SetMultipleObstructionAndOcclusion"),
	AKTEXT("SetOutputVolume"),
	AKTEXT("OpenDynamicSequence"),
	AKTEXT("DynamicSequenceCommand"),
	AKTEXT("FreeBank"),
	AKTEXT("StopAll"),
	AKTEXT("AddOutput"),
	AKTEXT("RemoveOutput"),
	AKTEXT("ReplaceOutput"),
	AKTEXT("SetBusDevice"),
	AKTEXT("StopPlayingID"),
	AKTEXT("ExecuteActionOnEvent"),
	AKTEXT("PostMIDIOnEvent"),
	AKTEXT("StopMIDIOnEvent"),
	AKTEXT("PinEventInStreamCache"),
	AKTEXT("SetAttenuationScalingFactor"),
	AKTEXT("SetMultiplePositions"),
	AKTEXT("SeekOnEvent"),
	AKTEXT("PlaySourcePlugin"),
	AKTEXT("OutputCapture"), // Start/Stop
	AKTEXT("AddOutputCaptureMarker"),
	AKTEXT("Set"), // for SetEffect. The effect type is determined in MonitoringMgrDataItem.cpp (Buss Effect or Actor-Mixer Effect) using the message parameters.
	AKTEXT("SetMixer"),
	AKTEXT("SetBusConfig"),
	AKTEXT("SetPanningRule"),
	AKTEXT("SetSpeakerAngles"),
	AKTEXT("SetProcessMode"),
	AKTEXT("SendMainOutputToDevice"),
	AKTEXT("SetRandomSeed"),
	AKTEXT("MuteBackgroundMusic"),
	AKTEXT("SetPluginCustomData"),
	AKTEXT("Suspend"),
	AKTEXT("InitSinkPlugin"),
	// AKTEXT("ApiExtension"), Here we should manage API extensions properly, there is only one extension for the moment, just assign it to SpatialAudio
	AKTEXT("SpatialAudio"),
	AKTEXT("ExecuteActionOnPlayingID"),
	AKTEXT("DynamicSequenceSeek")
	AKTEXT(""),	//WrapAround.  Not printed.
	AKTEXT("Invalid")

	// WARNING : When Adding a new message type, also create a new ApiSubFunctionDataItem in MonitoringMgrDataItem.cpp
};


// ATTENTION!  Rules about Queued Messages structures:
// These structures are now shared between the sound engine and the authoring, for the API profiling.
// This means that you MUST reflect your changes in the MonitoringMgrItem.cpp file if:
// - You added/removed a new message type
// - You added/removed parameters in a message
//
// Also, they must be "sufficiently" packed i.e. the offset of the members doesn't change 
// between 32/64 versions and from compiler to compiler.  This means:
// - No raw pointers.  Use AkQueuedMsgPtr<> instead
// - No type smaller than 32 bits unless it is the last, or you hand-packed multiple small types so it rounds up to 4 bytes.
//
// This checks only the static part of the structure. Variable sized messages (with strings, for example) are not checked, so beware!
inline void ValidateMessageQueueTypes()
{
	//These sizes must match on all platforms for API profiling to work properly
	AkStaticAssert<sizeof(AkQueuedMsg_EventAction) == 36>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_EventPostMIDI) == 32>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_EventStopMIDI) == 20>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_EventBase) == 36>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_Event) == 48>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_OpenDynamicSequence) == 44>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_GameObjectBase) == 8>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_EventBase) == 36>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_EventRef) == 12>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_Event) == 48>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_SourcePluginAction) == 48>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!	
	AkStaticAssert<sizeof(AkQueuedMsg_LockUnlockStreamCache) == 24>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_Rtpc) == 20>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_RtpcWithTransition) == 32>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_State) == 12>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_Switch) == 16>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_Trigger) == 12>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_RegisterGameObj) == 16>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_UnregisterGameObj) == 8>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_GameObjPosition) == 44>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_GameObjMultiplePosition) == 56>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_GameObjActiveListeners) == 24>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_GameObjActiveControllers) == 12>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_ListenerSpatialization) == 24>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_AddOutput) == 32>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_RemoveOutput) == 8>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_ReplaceOutput) == 24>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_SetBusDevice) == 8>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_GameObjEnvValues) == 28>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_GameObjDryLevel) == 20>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_GameObjScalingFactor) == 12>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_Seek) == 32>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_GameObjObstruction) == 24>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_GameObjMultipleObstruction) == 28>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_ResetSwitches) == 8>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_ResetRTPC) == 12>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_ResetRTPCValue) == 16>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_ResetRtpcValueWithTransition) == 28>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_SetOutputVolume) == 12>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_OpenDynamicSequence) == 44>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_KillBank) == 8>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_StopAll) == 8>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_StopPlayingID) == 12>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_PlayingIDAction) == 16>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_DynamicSequenceCmd) == 20>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_DynamicSequenceSeek) == 16>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!	
	AkStaticAssert<sizeof(AkQueuedMsg_StartStopCapture) == 8>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_AddOutputCaptureMarker) == 8>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_SetEffect) == 16>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_SetBusConfig) == 8>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_SetPanningRule) == 12>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_SetSpeakerAngles) == 24>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!	
	AkStaticAssert<sizeof(AkQueuedMsg_SetRandomSeed) == 4>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_MuteBackgroundMusic) == 2>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_PluginCustomGameData) == 36>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_EndOfList) == 4>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_Suspend) == 8>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_InitSinkPlugin) == 16>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!
	AkStaticAssert<sizeof(AkQueuedMsg_ApiExtension) == 4>::Assert(); //If you get an error here, it means you changed Queue Message.  Read the rules above!	

	AkStaticAssert<QueuedMsgType_Invalid == 59>::Assert();	//This is a reminder: Go in MonitoringMgrDataItem.cpp@CreateApiSubFunctionDataItem add your new message in the Monitoring.  Then change the number.
	AkStaticAssert<sizeof(s_cApiFunctionToText) / sizeof(AkOSChar*) == QueuedMsgType_Invalid>::Assert();	//You need to provide a proper string in the s_cApiFunctionToText array.
}