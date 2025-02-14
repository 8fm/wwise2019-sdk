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

#ifndef AK_OPTIMIZED

#include "AkRTPC.h"
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include "CommandData.h"

class IConsoleConnector;
struct AkInitSettings;

namespace AK
{
	namespace Comm
	{
		class IRendererProxy
		{
		public:
			virtual void SetNetworkControllerName(const char* in_pszNetworkAppName = "") const = 0; // First notification, optional, only purpose is to communicate our network name to the remote host.
			virtual AkPlayingID PostEvent(AkUniqueID in_eventID, AkWwiseGameObjectID in_gameObjectPtr = 0, unsigned int in_cookie = 0, AkUInt32 in_uFlags = 0, AkTimeMs in_iSeekOffset = 0 ) const = 0;
			virtual AKRESULT ExecuteActionOnEvent( AkUniqueID in_eventID, AK::SoundEngine::AkActionOnEventType in_ActionType, AkWwiseGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT, AkTimeMs in_uTransitionDuration = 0, AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear ) = 0;

			virtual bool SeekOnEvent(AkUniqueID in_eventID, AkWwiseGameObjectID in_gameObjectID, AkTimeMs in_iPosition, bool in_bSeekToNearestMarker = false, AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID) const = 0;
			virtual bool SeekOnEvent(AkUniqueID in_eventID, AkWwiseGameObjectID in_gameObjectID, AkReal32 in_fPercent, bool in_bSeekToNearestMarker = false, AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID) const = 0;

			//About RTPC Manager
			virtual AKRESULT RegisterGameObj( AkWwiseGameObjectID in_GameObj, const char* in_pszObjectName = "" ) = 0;
			virtual AKRESULT UnregisterGameObj( AkWwiseGameObjectID in_GameObj ) = 0;
			
			virtual AKRESULT MonitoringMuteGameObj( AkWwiseGameObjectID in_GameObj, bool in_bMute, bool in_bNotify = true ) = 0;
			virtual AKRESULT MonitoringSoloGameObj(AkWwiseGameObjectID in_GameObj, bool in_bSolo, bool in_bNotify = true ) = 0;

			virtual AKRESULT SetDefaultListeners(AkWwiseGameObjectID* in_pListenerIDs, AkUInt32 in_uNumListeners) = 0;
			virtual AKRESULT SetListeners(AkWwiseGameObjectID in_GameObjectID, AkWwiseGameObjectID* in_pListenerIDs, AkUInt32 in_uNumListeners) = 0;
			virtual AKRESULT SetAttenuationScalingFactor( AkWwiseGameObjectID in_GameObjectID, AkReal32 in_fAttenuationScalingFactor ) = 0;
			virtual void SetRelativePosition( const AkSoundPosition & in_rPosition ) = 0;
			virtual AKRESULT SetPosition( AkWwiseGameObjectID in_GameObj, const AkSoundPosition& in_rPosition ) = 0;
			virtual AKRESULT SetMultiplePositions( AkWwiseGameObjectID in_GameObjectID, const AkSoundPosition * in_pPositions, AkUInt16 in_NumPositions, AK::SoundEngine::MultiPositionType in_eMultiPositionType = AK::SoundEngine::MultiPositionType_MultiDirections ) = 0;
			virtual AKRESULT SetListenerPosition(const AkListenerPosition& in_rPosition, AkWwiseGameObjectID in_ulIndex = 0) = 0;
			virtual AKRESULT SetListenerScalingFactor( AkWwiseGameObjectID in_ulIndex, AkReal32 in_fAttenuationScalingFactor ) = 0;
			virtual AKRESULT SetListenerSpatialization( AkWwiseGameObjectID in_uIndex, bool in_bSpatialized, AkChannelConfig in_channelConfig, AK::SpeakerVolumes::VectorPtr in_pVolumeOffsets = NULL ) = 0;
			virtual void SetRTPC( AkRtpcID in_RTPCid, AkReal32 in_Value, AkWwiseGameObjectID in_GameObj = AK_INVALID_GAME_OBJECT, bool in_bBypassInternalValueInterpolation = false ) = 0;
			virtual void SetDefaultRTPCValue( AkRtpcID in_RTPCid, AkReal32 in_defaultValue ) = 0;
			virtual void SetRTPCRamping(  AkRtpcID in_RTPCid, int in_rampingType, AkReal32 in_fRampUp, AkReal32 in_fRampDown ) = 0;
			virtual void BindRTPCToBuiltInParam( AkRtpcID in_RTPCid, AkBuiltInParam in_eBindToParam ) = 0;
			virtual void SetPanningRule( AkPanningRule in_panningRule ) = 0;
			virtual AKRESULT ResetRTPC( AkWwiseGameObjectID in_GameObjectPtr = AK_INVALID_GAME_OBJECT ) = 0;
			virtual void ResetRTPCValue( AkRtpcID in_RTPCid, AkWwiseGameObjectID in_GameObjectPtr = AK_INVALID_GAME_OBJECT, bool in_bBypassInternalValueInterpolation = false ) = 0;
			virtual AKRESULT SetSwitch( AkSwitchGroupID in_SwitchGroup, AkSwitchStateID in_SwitchState, AkWwiseGameObjectID in_GameObj = AK_INVALID_GAME_OBJECT ) = 0;
			virtual AKRESULT ResetSwitches( AkWwiseGameObjectID in_GameObjectPtr = AK_INVALID_GAME_OBJECT ) = 0;
			virtual AKRESULT PostTrigger( AkTriggerID in_Trigger, AkWwiseGameObjectID in_GameObjPtr = AK_INVALID_GAME_OBJECT ) = 0;

			virtual AKRESULT ResetAllStates() = 0;
			virtual AKRESULT ResetRndSeqCntrPlaylists() = 0;

			virtual AKRESULT SetGameObjectAuxSendValues(AkWwiseGameObjectID in_gameObjectID, AkMonitorData::AuxSendValue* in_aEnvironmentValues, AkUInt32 in_uNumEnvValues) = 0;
			virtual AKRESULT SetGameObjectOutputBusVolume(AkWwiseGameObjectID in_emitterID, AkWwiseGameObjectID in_listenerID, AkReal32 in_fControlValue) = 0;
			virtual AKRESULT SetObjectObstructionAndOcclusion(AkWwiseGameObjectID in_EmitterID, AkWwiseGameObjectID in_ListenerID, AkReal32 in_fObstructionLevel, AkReal32 in_fOcclusionLevel) = 0;
			virtual AKRESULT SetMultipleObstructionAndOcclusion(AkWwiseGameObjectID in_EmitterID, AkWwiseGameObjectID in_listenerID, AkObstructionOcclusionValues* in_fObstructionOcclusionValues, AkUInt32 in_uNumOcclusionObstruction) = 0;

			virtual AKRESULT SetObsOccCurve( int in_curveXType, int in_curveYType, AkUInt32 in_uNumPoints, AkRTPCGraphPoint* in_apPoints, AkCurveScaling in_eScaling ) = 0;
			virtual AKRESULT SetObsOccCurveEnabled( int in_curveXType, int in_curveYType, bool in_bEnabled ) = 0;

			virtual AKRESULT AddSwitchRTPC( AkSwitchGroupID in_switchGroup, AkRtpcID in_RTPC_ID, AkRtpcType in_RTPC_Type, AkSwitchGraphPoint* in_pArrayConversion, AkUInt32 in_ulConversionArraySize ) = 0;
			virtual void	 RemoveSwitchRTPC( AkSwitchGroupID in_switchGroup ) = 0;

			virtual void	 SetVolumeThreshold( AkReal32 in_VolumeThreshold ) = 0;
			virtual void	 SetMaxNumVoicesLimit( AkUInt16 in_maxNumberVoices ) = 0;
			virtual void	 SetMaxNumDangerousVirtVoicesLimit( AkUInt16 in_maxNumberVoices ) = 0;

			virtual AKRESULT PostMsgMonitor( const AkUtf16* in_pszMessage ) = 0;

			virtual AKRESULT PostMsgMonitor( const char* in_pszMessage ) = 0;

			virtual void StopAll( AkWwiseGameObjectID in_GameObjPtr = AK_INVALID_GAME_OBJECT ) = 0;
			virtual void StopPlayingID( AkPlayingID in_playingID, AkTimeMs in_uTransitionDuration = 0, AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear ) = 0;
			virtual void ExecuteActionOnPlayingID(AK::SoundEngine::AkActionOnEventType in_ActionType, AkPlayingID in_playingID, AkTimeMs in_uTransitionDuration = 0, AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear) = 0;

			virtual void SetLoudnessFrequencyWeighting( AkLoudnessFrequencyWeighting in_eLoudnessFrequencyWeighting ) = 0;

			virtual void AddOutput(AkOutputSettings &in_rSetting, const AkGameObjectID* in_pListenerIDs, AkUInt32 in_uNumListeners) = 0;
			virtual void RegisterPlugin(const char *in_szPlugin) = 0;			
						
			enum MethodIDs
			{
				MethodSetNetworkControllerName = 1,
				MethodPostEvent,
				MethodExecuteActionOnEvent,
				MethodSeekOnEvent_MS,
				MethodSeekOnEvent_PCT,
				MethodRegisterGameObject,
				MethodUnregisterGameObject,
				MethodSetListeners,
				MethodSetAttenuationScalingFactor,
				MethodSetRelativePosition,
				MethodSetPosition,
				MethodSetMultiplePositions,
				MethodSetListenerPosition,
				MethodSetListenerScalingFactor,
				MethodSetListenerSpatialization,
				MethodSetPanningRule,
				MethodSetRTPC,
				MethodSetDefaultRTPCValue,
				MethodSetRTPCRamping,
				MethodRTPCBindToBuiltInParam,
				MethodResetRTPC,
				MethodResetRTPCValue,
				MethodSetSwitch,
				MethodResetSwitches,
				MethodPostTrigger,

				MethodResetAllStates,
				MethodResetRndSeqCntrPlaylists,

				MethodSetGameObjectAuxSendValues,
				MethodSetGameObjectOutputBusVolume,
				MethodSetObjectObstructionAndOcclusion,
				MethodSetMultipleObstructionAndOcclusion,

				MethodSetObsOccCurve,
				MethodSetObsOccCurveEnabled,

				MethodAddSwitchRTPC,
				MethodRemoveSwitchRTPC,

				MethodSetVolumeThreshold,
				MethodSetMaxNumVoicesLimit,
				MethodSetMaxNumDangerousVirtVoicesLimit,

				MethodPostMsgMonitor,

				MethodStopAll,
				MethodStopPlayingID,
				MethodExecuteActionOnPlayingID,

				MethodSetLoudnessFrequencyWeighting,

				MethodEnableSecondaryOutput,

				MethodRegisterPlugin,
				MethodMonitoringMuteGameObj,
				MethodMonitoringSoloGameObj,

				MethodSetDefaultListeners,

				LastMethodID
			};
		};
	}
}
namespace RendererProxyCommandData
{
	struct CommandData : public ProxyCommandData::CommandData
	{
		CommandData();
		CommandData( AkUInt16 in_methodID );

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		DECLARE_BASECLASS( ProxyCommandData::CommandData );
	};


	struct SetNetworkControllerName : public CommandData
	{
		SetNetworkControllerName();

		bool Serialize(CommandDataSerializer& in_rSerializer) const;
		bool Deserialize(CommandDataSerializer& in_rSerializer);

		char* m_pszNetworkAppName;

		DECLARE_BASECLASS(CommandData);
	};


	struct ExecuteActionOnEvent : public CommandData
	{
		ExecuteActionOnEvent();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUniqueID m_eventID;
		AK::SoundEngine::AkActionOnEventType m_eActionType;
        AkWwiseGameObjectID m_gameObjectID;
		AkTimeMs m_uTransitionDuration;
		AkCurveInterpolation m_eFadeCurve;

		DECLARE_BASECLASS( CommandData );
	};

	struct SeekOnEvent_MS : public CommandData
	{
		SeekOnEvent_MS();

		bool Serialize(CommandDataSerializer& in_rSerializer) const;
		bool Deserialize(CommandDataSerializer& in_rSerializer);

		AkUniqueID m_eventID;
		AkWwiseGameObjectID m_gameObjectID;
		AkTimeMs m_iPosition;
		bool m_bSeekToNearestMarker;
		AkPlayingID m_PlayingID;

		DECLARE_BASECLASS(CommandData);
	};

	struct SeekOnEvent_PCT : public CommandData
	{
		SeekOnEvent_PCT();

		bool Serialize(CommandDataSerializer& in_rSerializer) const;
		bool Deserialize(CommandDataSerializer& in_rSerializer);

		AkUniqueID m_eventID;
		AkWwiseGameObjectID m_gameObjectID;
		AkReal32 m_fPercent;
		bool m_bSeekToNearestMarker;
		AkPlayingID m_PlayingID;

		DECLARE_BASECLASS(CommandData);
	};

	struct PostEvent : public CommandData
	{
		PostEvent();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUniqueID m_eventID;
		AkWwiseGameObjectID m_gameObjectPtr;
		AkUInt32 m_cookie;
		AkUInt32 m_flags;
		AkTimeMs m_seekOffset;

		DECLARE_BASECLASS( CommandData );
	};

	struct RegisterGameObj : public CommandData
	{
		RegisterGameObj();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_gameObjectPtr;
		char* m_pszObjectName;

		DECLARE_BASECLASS( CommandData );
	};

	struct UnregisterGameObj : public CommandData
	{
		UnregisterGameObj();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_gameObjectPtr;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetDefaultListeners : public CommandData
	{
		SetDefaultListeners();
		~SetDefaultListeners();

		bool Serialize(CommandDataSerializer& in_rSerializer) const;
		bool Deserialize(CommandDataSerializer& in_rSerializer);

		AkWwiseGameObjectID* m_pListeners;
		AkUInt32 m_uNumListeners;

		DECLARE_BASECLASS(CommandData);
	};

	struct SetListeners : public CommandData
	{
		SetListeners();
		~SetListeners();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_gameObjectID;
		AkWwiseGameObjectID* m_pListeners;
		AkUInt32 m_uNumListeners;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetAttenuationScalingFactor : public CommandData
	{
		SetAttenuationScalingFactor();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_gameObjectID;
		AkReal32 m_fAttenuationScalingFactor;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetPanningRule : public CommandData
	{
		SetPanningRule();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkPanningRule m_panningRule;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetMultiplePositions : public CommandData
	{
		SetMultiplePositions();
		~SetMultiplePositions();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_gameObjectPtr;
		AkSoundPosition * m_pPositions;
		AkUInt32 m_numPositions;
		AK::SoundEngine::MultiPositionType m_eMultiPositionType;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetRelativePosition : public CommandData
	{
		SetRelativePosition();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkSoundPosition m_position;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetPosition : public CommandData
	{
		SetPosition();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_gameObjectPtr;
		AkSoundPosition m_position;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetListenerPosition : public CommandData
	{
		SetListenerPosition();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkListenerPosition m_position;
		AkWwiseGameObjectID m_ulIndex;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetListenerScalingFactor : public CommandData
	{
		SetListenerScalingFactor();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_ulIndex;
		AkReal32 m_fAttenuationScalingFactor;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetListenerSpatialization : public CommandData
	{
		SetListenerSpatialization();
		~SetListenerSpatialization();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_uIndex;
		bool m_bSpatialized;
		bool m_bUseVolumeOffsets;		// Use to know if the volumes be used (and transferred).
		AkChannelConfig m_channelConfig;
		AK::SpeakerVolumes::VectorPtr m_pVolumeOffsets;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetRTPC : public CommandData
	{
		SetRTPC();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkRtpcID m_RTPCid;
		AkReal32 m_value;
		AkWwiseGameObjectID m_gameObj;
		bool m_bBypassInternalValueInterpolation;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetDefaultRTPCValue : public CommandData
	{
		SetDefaultRTPCValue();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkRtpcID m_RTPCid;
		AkReal32 m_defaultValue;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetRTPCRamping : public CommandData
	{
		SetRTPCRamping();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkRtpcID m_RTPCid;
		AkUInt32 m_rampingType;
		AkReal32 m_fRampUp;
		AkReal32 m_fRampDown;

		DECLARE_BASECLASS( CommandData );
	};

	struct RTPCBindToBuiltInParam : public CommandData
	{
		RTPCBindToBuiltInParam();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkRtpcID m_RTPCid;
		AkBuiltInParam m_bindToParam;

		DECLARE_BASECLASS( CommandData );
	};

	struct ResetRTPC : public CommandData
	{
		ResetRTPC();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_gameObj;

		DECLARE_BASECLASS( CommandData );
	};

	struct ResetRTPCValue : public CommandData
	{
		ResetRTPCValue();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkRtpcID m_RTPCid;
		AkWwiseGameObjectID m_gameObj;
		bool m_bBypassInternalValueInterpolation;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetSwitch : public CommandData
	{
		SetSwitch();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkSwitchGroupID m_switchGroup;
		AkSwitchStateID m_switchState;
		AkWwiseGameObjectID m_gameObj;

		DECLARE_BASECLASS( CommandData );
	};

	struct PostTrigger : public CommandData
	{
		PostTrigger();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkTriggerID m_trigger;
		AkWwiseGameObjectID m_gameObj;

		DECLARE_BASECLASS( CommandData );
	};

	struct ResetSwitches : public CommandData
	{
		ResetSwitches();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_gameObj;

		DECLARE_BASECLASS( CommandData );
	};

	struct ResetAllStates : public CommandData
	{
		ResetAllStates();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		DECLARE_BASECLASS( CommandData );
	};

	struct ResetRndSeqCntrPlaylists : public CommandData
	{
		ResetRndSeqCntrPlaylists();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		DECLARE_BASECLASS( CommandData );
	};

	struct SetGameObjectAuxSendValues : public CommandData
	{
		SetGameObjectAuxSendValues();
		~SetGameObjectAuxSendValues();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_gameObjectID; 
		AkMonitorData::AuxSendValue* m_aEnvironmentValues;
		AkUInt32 m_uNumEnvValues;

	private:
		DECLARE_BASECLASS( CommandData );
	};

	struct SetGameObjectOutputBusVolume : public CommandData
	{
		SetGameObjectOutputBusVolume();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_emitterID;
		AkWwiseGameObjectID m_listenerID;
		AkReal32 m_fControlValue;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetObjectObstructionAndOcclusion : public CommandData
	{
		SetObjectObstructionAndOcclusion();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_EmitterID;
		AkWwiseGameObjectID m_ListenerID;
		AkReal32 m_fObstructionLevel;
		AkReal32 m_fOcclusionLevel;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetMultipleObstructionAndOcclusion : public CommandData
	{
		SetMultipleObstructionAndOcclusion();
		~SetMultipleObstructionAndOcclusion();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_EmitterID;
		AkWwiseGameObjectID m_ListenerID;
		AkObstructionOcclusionValues* m_fObstructionOcclusionValues;
		AkUInt32 m_uNumOcclusionObstruction;

		DECLARE_BASECLASS( CommandData );
	};
	
	struct SetObsOccCurve :  public CommandData, public ProxyCommandData::CurveData<AkRTPCGraphPoint>
	{
		SetObsOccCurve();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkInt32 m_curveXType;
		AkInt32 m_curveYType;

	private:
	
		DECLARE_BASECLASS( CommandData );
	};

	struct SetObsOccCurveEnabled :  public CommandData
	{
		SetObsOccCurveEnabled();
		~SetObsOccCurveEnabled();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkInt32 m_curveXType;
		AkInt32 m_curveYType;
		bool m_bEnabled;
	
		DECLARE_BASECLASS( CommandData );
	};

	struct AddSwitchRTPC : public CommandData
	{
		AddSwitchRTPC();
		~AddSwitchRTPC();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_uSwitchGroup;
		AkRtpcID m_RTPC_ID;
		AkRtpcType m_RTPC_Type;
		AkUInt32 m_uArraySize;
		AkSwitchGraphPoint* m_pArrayConversion;

	private:
		DECLARE_BASECLASS( CommandData );
	};

	struct RemoveSwitchRTPC :  public CommandData
	{
		RemoveSwitchRTPC();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt32 m_uSwitchGroup;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetVolumeThreshold :  public CommandData
	{
		SetVolumeThreshold();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkReal32 m_fVolumeThreshold;

		DECLARE_BASECLASS( CommandData );
	};
	
	struct SetMaxNumVoicesLimit :  public CommandData
	{
		SetMaxNumVoicesLimit();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt16 m_maxNumberVoices;

		DECLARE_BASECLASS( CommandData );
	};

	struct SetMaxNumDangerousVirtVoicesLimit : public CommandData
	{
		SetMaxNumDangerousVirtVoicesLimit();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt16 m_maxNumberVoices;

		DECLARE_BASECLASS( CommandData );
	};

	struct PostMsgMonitor : public CommandData
	{
		PostMsgMonitor();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUtf16* m_pszMessage;

		DECLARE_BASECLASS( CommandData );
	};

	struct StopAll :  public CommandData
	{
		StopAll();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkWwiseGameObjectID m_GameObjPtr;

		DECLARE_BASECLASS( CommandData );
	};

	struct StopPlayingID :  public CommandData
	{
		StopPlayingID();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkPlayingID m_playingID;
		AkTimeMs m_uTransitionDuration;
		AkCurveInterpolation m_eFadeCurve;

		DECLARE_BASECLASS( CommandData );
	};

	struct ExecuteActionOnPlayingID : public CommandData
	{
		ExecuteActionOnPlayingID();

		bool Serialize(CommandDataSerializer& in_rSerializer) const;
		bool Deserialize(CommandDataSerializer& in_rSerializer);

		AkPlayingID m_playingID;
		AK::SoundEngine::AkActionOnEventType m_eActionType;
		AkTimeMs m_uTransitionDuration;
		AkCurveInterpolation m_eFadeCurve;

		DECLARE_BASECLASS(CommandData);
	};

	struct RegisterPlugin : public CommandData
	{
		RegisterPlugin();
		
		bool Serialize(CommandDataSerializer& in_rSerializer) const;
		bool Deserialize(CommandDataSerializer& in_rSerializer);

		char *m_szPlugin;		

		DECLARE_BASECLASS(CommandData);
	};

	struct MonitoringMuteGameObj : public CommandData
	{
		MonitoringMuteGameObj();

		bool Serialize(CommandDataSerializer& in_rSerializer) const;
		bool Deserialize(CommandDataSerializer& in_rSerializer);

		AkWwiseGameObjectID m_gameObjectPtr;
		bool m_bMute;

		DECLARE_BASECLASS(CommandData);
	};

	struct MonitoringSoloGameObj : public CommandData
	{
		MonitoringSoloGameObj();

		bool Serialize(CommandDataSerializer& in_rSerializer) const;
		bool Deserialize(CommandDataSerializer& in_rSerializer);

		AkWwiseGameObjectID m_gameObjectPtr;
		bool m_bSolo;

		DECLARE_BASECLASS(CommandData);
	};
}

#endif // #ifndef AK_OPTIMIZED
