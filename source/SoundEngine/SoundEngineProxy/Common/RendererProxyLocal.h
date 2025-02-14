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

#include "IRendererProxy.h"

class RendererProxyLocal : public AK::Comm::IRendererProxy
{
public:
	RendererProxyLocal();
	virtual ~RendererProxyLocal();

	virtual void SetNetworkControllerName(const char* in_pszNetworkAppName = "") const;

	// IRendererProxy members
	virtual AkPlayingID PostEvent( AkUniqueID in_eventID, AkWwiseGameObjectID in_gameObjectPtr, unsigned int in_cookie, AkUInt32 in_uFlags = 0, AkTimeMs in_iSeekOffset = 0 ) const;
	virtual AKRESULT ExecuteActionOnEvent( AkUniqueID in_eventID, AK::SoundEngine::AkActionOnEventType in_ActionType, AkWwiseGameObjectID in_gameObjectID = AK_INVALID_GAME_OBJECT, AkTimeMs in_uTransitionDuration = 0, AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear );

	virtual bool SeekOnEvent(AkUniqueID in_eventID, AkWwiseGameObjectID in_gameObjectID, AkTimeMs in_iPosition, bool in_bSeekToNearestMarker = false, AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID) const;
	virtual bool SeekOnEvent(AkUniqueID in_eventID, AkWwiseGameObjectID in_gameObjectID, AkReal32 in_fPercent, bool in_bSeekToNearestMarker = false, AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID) const;

	//About RTPC Manager
	virtual AKRESULT RegisterGameObj( AkWwiseGameObjectID in_GameObj, const char* in_pszObjectName = "" );
	virtual AKRESULT UnregisterGameObj( AkWwiseGameObjectID in_GameObj );

	virtual AKRESULT MonitoringMuteGameObj(AkWwiseGameObjectID in_gameObjID, bool in_bState, bool in_bNotify = true );
	virtual AKRESULT MonitoringSoloGameObj(AkWwiseGameObjectID in_gameObjID, bool in_bState, bool in_bNotify = true );

	virtual AKRESULT SetDefaultListeners(AkWwiseGameObjectID* in_pListenerIDs, AkUInt32 in_uNumListeners);
	virtual AKRESULT SetListeners(AkWwiseGameObjectID in_GameObjectID, AkWwiseGameObjectID* in_pListenerIDs, AkUInt32 in_uNumListeners);
	virtual AKRESULT SetAttenuationScalingFactor( AkWwiseGameObjectID in_GameObjectID, AkReal32 in_fAttenuationScalingFactor );
	virtual void SetRelativePosition( const AkSoundPosition & in_rPosition );
	virtual AKRESULT SetPosition( AkWwiseGameObjectID in_GameObj, const AkSoundPosition& in_rPosition );
	virtual AKRESULT SetMultiplePositions( AkWwiseGameObjectID in_GameObjectID, const AkSoundPosition * in_pPositions, AkUInt16 in_NumPositions, AK::SoundEngine::MultiPositionType in_eMultiPositionType = AK::SoundEngine::MultiPositionType_MultiDirections );
	virtual AKRESULT SetListenerPosition(const AkListenerPosition& in_rPosition, AkWwiseGameObjectID in_ulIndex = 0);
	virtual AKRESULT SetListenerScalingFactor( AkWwiseGameObjectID in_ulIndex, AkReal32 in_fAttenuationScalingFactor );
	virtual AKRESULT SetListenerSpatialization( AkWwiseGameObjectID in_uIndex, bool in_bSpatialized, AkChannelConfig in_channelConfig, AK::SpeakerVolumes::VectorPtr in_pVolumeOffsets = NULL );
	virtual void SetRTPC( AkRtpcID in_RTPCid, AkReal32 in_Value, AkWwiseGameObjectID in_GameObj = AK_INVALID_GAME_OBJECT , bool in_bBypassInternalValueInterpolation = false );
	virtual void SetDefaultRTPCValue( AkRtpcID in_RTPCid, AkReal32 in_defaultValue );
	virtual void SetRTPCRamping(  AkRtpcID in_RTPCid, int in_rampingType, AkReal32 in_fRampUp, AkReal32 in_fRampDown );
	virtual void BindRTPCToBuiltInParam( AkRtpcID in_RTPCid, AkBuiltInParam in_eBindToParam );
	virtual void SetPanningRule( AkPanningRule in_panningRule );
	virtual AKRESULT ResetRTPC( AkWwiseGameObjectID in_GameObjectPtr = AK_INVALID_GAME_OBJECT );
	virtual void ResetRTPCValue( AkRtpcID in_RTPCid, AkWwiseGameObjectID in_GameObjectPtr = AK_INVALID_GAME_OBJECT, bool in_bBypassInternalValueInterpolation = false );
	virtual AKRESULT SetSwitch( AkSwitchGroupID in_SwitchGroup, AkSwitchStateID in_SwitchState, AkWwiseGameObjectID in_GameObj = AK_INVALID_GAME_OBJECT );
	virtual AKRESULT ResetSwitches( AkWwiseGameObjectID in_GameObjectPtr = AK_INVALID_GAME_OBJECT );
	virtual AKRESULT PostTrigger( AkTriggerID in_Trigger, AkWwiseGameObjectID in_GameObjectPtr = AK_INVALID_GAME_OBJECT );

	virtual AKRESULT ResetAllStates();
	virtual AKRESULT ResetRndSeqCntrPlaylists();

	virtual AKRESULT SetGameObjectAuxSendValues(AkWwiseGameObjectID in_gameObjectID, AkMonitorData::AuxSendValue* in_aEnvironmentValues, AkUInt32 in_uNumEnvValues);
	virtual AKRESULT SetGameObjectOutputBusVolume(AkWwiseGameObjectID in_emitterID, AkWwiseGameObjectID in_listenerID, AkReal32 in_fControlValue);
	virtual AKRESULT SetObjectObstructionAndOcclusion(AkWwiseGameObjectID in_EmitterID, AkWwiseGameObjectID in_ListenerID, AkReal32 in_fObstructionLevel, AkReal32 in_fOcclusionLevel);
	virtual AKRESULT SetMultipleObstructionAndOcclusion(AkWwiseGameObjectID in_EmitterID, AkWwiseGameObjectID in_ListenerID, AkObstructionOcclusionValues* in_ObstructionOcclusionValue, AkUInt32 in_uNumObstructionOcclusion);

	virtual AKRESULT SetObsOccCurve( int in_curveXType, int in_curveYType, AkUInt32 in_uNumPoints, AkRTPCGraphPoint * in_apPoints, AkCurveScaling in_eScaling );
	virtual AKRESULT SetObsOccCurveEnabled( int in_curveXType, int in_curveYType, bool in_bEnabled );

	virtual AKRESULT AddSwitchRTPC( AkSwitchGroupID in_switchGroup, AkRtpcID in_RTPC_ID, AkRtpcType in_RTPC_Type, AkSwitchGraphPoint* in_pArrayConversion, AkUInt32 in_ulConversionArraySize );
	virtual void	 RemoveSwitchRTPC( AkSwitchGroupID in_switchGroup );

	virtual void	 SetVolumeThreshold( AkReal32 in_VolumeThreshold );
	virtual void	 SetMaxNumVoicesLimit( AkUInt16 in_maxNumberVoices );
	virtual void	 SetMaxNumDangerousVirtVoicesLimit( AkUInt16 in_maxNumberVoices );

	virtual AKRESULT PostMsgMonitor( const AkUtf16* in_pszMessage );

	virtual AKRESULT PostMsgMonitor( const char* in_pszMessage );

	virtual void StopAll( AkWwiseGameObjectID in_GameObjPtr = AK_INVALID_GAME_OBJECT );
	virtual void StopPlayingID( AkPlayingID in_playingID, AkTimeMs in_uTransitionDuration = 0, AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear );
	virtual void ExecuteActionOnPlayingID(AK::SoundEngine::AkActionOnEventType in_ActionType, AkPlayingID in_playingID, AkTimeMs in_uTransitionDuration = 0, AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear);

	virtual void SetLoudnessFrequencyWeighting( AkLoudnessFrequencyWeighting in_eLoudnessFrequencyWeighting );

	virtual void AddOutput(AkOutputSettings &in_rSetting, const AkGameObjectID* in_pListenerIDs, AkUInt32 in_uNumListeners);
	virtual void RegisterPlugin(const char *in_szPlugin);
};

#endif // #ifndef AK_OPTIMIZED
