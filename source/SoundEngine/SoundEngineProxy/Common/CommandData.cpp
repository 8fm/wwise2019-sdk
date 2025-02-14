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
#ifndef AK_OPTIMIZED

#include "CommandData.h"
#include "IRendererProxy.h"
#include "IALMonitorSubProxy.h"
#include "IALMonitorProxy.h"
#include "IStateMgrProxy.h"
#include "IParameterableProxy.h"
#include "IParameterNodeProxy.h"
#include "ISoundProxy.h"
#include "IBusProxy.h"
#include "IEventProxy.h"
#include "IActionProxy.h"
#include "IStateProxy.h"
#include "IAttenuationProxy.h"
#include "IModulatorProxy.h"
#include "IMultiSwitchProxy.h"
#include "IMusicNodeProxy.h"
#include "IMusicTransAwareProxy.h"
#include "IMusicRanSeqProxy.h"
#include "IMusicSwitchProxy.h"
#include "ISegmentProxy.h"
#include "IRanSeqContainerProxy.h"
#include "ISwitchContainerProxy.h"
#include "ILayerContainerProxy.h"
#include "ILayerProxy.h"
#include "ITrackProxy.h"
#include "AkRanSeqBaseInfo.h"
#include "AkSound.h"
#include "IFxBaseProxy.h"
#include "IMidiDeviceMgrProxy.h"
#include "IVirtualAcousticsProxy.h"

#include "CommandDataSerializer.h"

extern AK::Comm::ICommunicationCentral * g_pCommCentral;

namespace ProxyCommandData
{
	bool CommandData::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return in_rSerializer.Put( m_commandType )
			&& in_rSerializer.Put( m_methodID );
	}

	bool CommandData::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		m_bWasDeserialized = true;
		return in_rSerializer.Get( m_commandType )
			&& in_rSerializer.Get( m_methodID );
	}
}

namespace RendererProxyCommandData
{
	CommandData::CommandData()
		: ProxyCommandData::CommandData()
	{}

	CommandData::CommandData( AkUInt16 in_methodID )
		: ProxyCommandData::CommandData( ProxyCommandData::TypeRendererProxy, in_methodID )
	{}

	bool CommandData::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer );
	}

	bool CommandData::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer );
	}


	ExecuteActionOnEvent::ExecuteActionOnEvent()
		: CommandData( AK::Comm::IRendererProxy::MethodExecuteActionOnEvent )
	{}

	bool ExecuteActionOnEvent::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_eventID )
			&& in_rSerializer.PutEnum( m_eActionType )
			&& in_rSerializer.Put( m_gameObjectID )
			&& in_rSerializer.Put( m_uTransitionDuration )
			&& in_rSerializer.PutEnum( m_eFadeCurve );
	}

	bool ExecuteActionOnEvent::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_eventID )
			&& in_rSerializer.GetEnum( m_eActionType )
			&& in_rSerializer.Get( m_gameObjectID )
			&& in_rSerializer.Get( m_uTransitionDuration )
			&& in_rSerializer.GetEnum( m_eFadeCurve );
	}	

	SetNetworkControllerName::SetNetworkControllerName()
		: CommandData(AK::Comm::IRendererProxy::MethodSetNetworkControllerName)
	{}

	bool SetNetworkControllerName::Serialize(CommandDataSerializer& in_rSerializer) const
	{
		return __base::Serialize(in_rSerializer)
			&& in_rSerializer.Put((const char*)m_pszNetworkAppName);
	}

	bool SetNetworkControllerName::Deserialize(CommandDataSerializer& in_rSerializer)
	{
		AkUInt32 uDummy = 0;

		return __base::Deserialize(in_rSerializer)
			&& in_rSerializer.Get((char*&)m_pszNetworkAppName, uDummy);
	}

	SeekOnEvent_MS::SeekOnEvent_MS()
		: CommandData(AK::Comm::IRendererProxy::MethodSeekOnEvent_MS)
	{}

	bool SeekOnEvent_MS::Serialize(CommandDataSerializer& in_rSerializer) const
	{
		return __base::Serialize(in_rSerializer)
			&& in_rSerializer.Put(m_eventID)
			&& in_rSerializer.PutEnum(m_gameObjectID)
			&& in_rSerializer.Put(m_iPosition)
			&& in_rSerializer.Put(m_bSeekToNearestMarker)
			&& in_rSerializer.PutEnum(m_PlayingID);
	}

	bool SeekOnEvent_MS::Deserialize(CommandDataSerializer& in_rSerializer)
	{
		return __base::Deserialize(in_rSerializer)
			&& in_rSerializer.Get(m_eventID)
			&& in_rSerializer.GetEnum(m_gameObjectID)
			&& in_rSerializer.Get(m_iPosition)
			&& in_rSerializer.Get(m_bSeekToNearestMarker)
			&& in_rSerializer.GetEnum(m_PlayingID);
	}

	SeekOnEvent_PCT::SeekOnEvent_PCT()
		: CommandData(AK::Comm::IRendererProxy::MethodSeekOnEvent_PCT)
	{}

	bool SeekOnEvent_PCT::Serialize(CommandDataSerializer& in_rSerializer) const
	{
		return __base::Serialize(in_rSerializer)
			&& in_rSerializer.Put(m_eventID)
			&& in_rSerializer.PutEnum(m_gameObjectID)
			&& in_rSerializer.Put(m_fPercent)
			&& in_rSerializer.Put(m_bSeekToNearestMarker)
			&& in_rSerializer.PutEnum(m_PlayingID);
	}

	bool SeekOnEvent_PCT::Deserialize(CommandDataSerializer& in_rSerializer)
	{
		return __base::Deserialize(in_rSerializer)
			&& in_rSerializer.Get(m_eventID)
			&& in_rSerializer.GetEnum(m_gameObjectID)
			&& in_rSerializer.Get(m_fPercent)
			&& in_rSerializer.Get(m_bSeekToNearestMarker)
			&& in_rSerializer.GetEnum(m_PlayingID);
	}

	PostEvent::PostEvent()
		: CommandData( AK::Comm::IRendererProxy::MethodPostEvent )
	{}

	bool PostEvent::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_eventID )
			&& in_rSerializer.Put( m_gameObjectPtr )
			&& in_rSerializer.Put( m_cookie )
			&& in_rSerializer.Put( m_flags )
			&& in_rSerializer.Put( m_seekOffset );
	}

	bool PostEvent::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_eventID )
			&& in_rSerializer.Get( m_gameObjectPtr )
			&& in_rSerializer.Get( m_cookie )
			&& in_rSerializer.Get( m_flags )
			&& in_rSerializer.Get( m_seekOffset );
	}

	RegisterGameObj::RegisterGameObj()
		:	CommandData( AK::Comm::IRendererProxy::MethodRegisterGameObject )
	{}

	bool RegisterGameObj::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_gameObjectPtr )
			&& in_rSerializer.Put( (const char*)m_pszObjectName );
	}
	
	bool RegisterGameObj::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		AkUInt32 uDummy = 0;

		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_gameObjectPtr )
			&& in_rSerializer.Get( (char*&)m_pszObjectName, uDummy );
	}

	UnregisterGameObj::UnregisterGameObj()
		:	CommandData( AK::Comm::IRendererProxy::MethodUnregisterGameObject )
	{}

	bool UnregisterGameObj::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_gameObjectPtr );
	}
	
	bool UnregisterGameObj::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_gameObjectPtr );
	}

	SetAttenuationScalingFactor::SetAttenuationScalingFactor()
		:	CommandData( AK::Comm::IRendererProxy::MethodSetAttenuationScalingFactor )
	{}

	bool SetAttenuationScalingFactor::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_gameObjectID )
			&& in_rSerializer.Put( m_fAttenuationScalingFactor );
	}
	bool SetAttenuationScalingFactor::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_gameObjectID )
			&& in_rSerializer.Get( m_fAttenuationScalingFactor );
	}

	SetPanningRule::SetPanningRule()
		:	CommandData( AK::Comm::IRendererProxy::MethodSetPanningRule )
	{}

	bool SetPanningRule::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_panningRule );
	}
	bool SetPanningRule::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_panningRule );
	}

	SetDefaultListeners::SetDefaultListeners()
		: CommandData(AK::Comm::IRendererProxy::MethodSetDefaultListeners)
		, m_pListeners(NULL)
	{}

	SetDefaultListeners::~SetDefaultListeners()
	{
		if (m_bWasDeserialized && m_pListeners)
		{
			CommandDataSerializer::FreeArrayMem(AkMemID_Profiler, m_pListeners);
		}
	}

	bool SetDefaultListeners::Serialize(CommandDataSerializer& in_rSerializer) const
	{
		return __base::Serialize(in_rSerializer)
			&& in_rSerializer.SerializeArray(m_uNumListeners, m_pListeners);
	}

	bool SetDefaultListeners::Deserialize(CommandDataSerializer& in_rSerializer)
	{
		bool bRet = __base::Deserialize(in_rSerializer)
			&& in_rSerializer.DeserializeArray(m_uNumListeners, m_pListeners);

		return bRet;
	}

	SetListeners::SetListeners()
		: CommandData(AK::Comm::IRendererProxy::MethodSetListeners)
		, m_pListeners(NULL)
	{}

	SetListeners::~SetListeners()
	{
		if (m_bWasDeserialized && m_pListeners)
		{
			CommandDataSerializer::FreeArrayMem(AkMemID_Profiler, m_pListeners);
		}
	}

	bool SetListeners::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_gameObjectID )
			&& in_rSerializer.SerializeArray(m_uNumListeners,m_pListeners);
	}
		
	bool SetListeners::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize(in_rSerializer)
			&& in_rSerializer.Get( m_gameObjectID )
			&& in_rSerializer.DeserializeArray(m_uNumListeners,m_pListeners);

		return bRet;
	}
	
	SetMultiplePositions::SetMultiplePositions()
		: CommandData( AK::Comm::IRendererProxy::MethodSetMultiplePositions )
		, m_pPositions(NULL)
	{}

	SetMultiplePositions::~SetMultiplePositions()
	{
		if( m_bWasDeserialized && m_pPositions )
		{
			CommandDataSerializer::FreeArrayMem( AkMemID_Profiler,  m_pPositions  );
		}
	}

	bool SetMultiplePositions::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_gameObjectPtr )
			&& in_rSerializer.PutEnum( m_eMultiPositionType )
			&& in_rSerializer.SerializeArray( m_numPositions, m_pPositions);
	
	}
	bool SetMultiplePositions::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_gameObjectPtr )
			&& in_rSerializer.GetEnum( m_eMultiPositionType )
			&& in_rSerializer.DeserializeArray( m_numPositions, m_pPositions);

		return bRet;
	}
	
	SetRelativePosition::SetRelativePosition()
		: CommandData( AK::Comm::IRendererProxy::MethodSetRelativePosition )
	{}

	bool SetRelativePosition::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_position );
	}

	bool SetRelativePosition::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_position );
	}

	SetPosition::SetPosition()
		: CommandData( AK::Comm::IRendererProxy::MethodSetPosition )
	{}

	bool SetPosition::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_gameObjectPtr )
			&& in_rSerializer.Put( m_position );
	}

	bool SetPosition::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_gameObjectPtr )
			&& in_rSerializer.Get( m_position );
	}

	SetListenerPosition::SetListenerPosition()
		: CommandData( AK::Comm::IRendererProxy::MethodSetListenerPosition )
	{}

	bool SetListenerPosition::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_position )
			&& in_rSerializer.Put( m_ulIndex );
	}

	bool SetListenerPosition::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_position )
			&& in_rSerializer.Get( m_ulIndex );
	}

	SetListenerScalingFactor::SetListenerScalingFactor()
		: CommandData( AK::Comm::IRendererProxy::MethodSetListenerScalingFactor )
	{}

	bool SetListenerScalingFactor::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_ulIndex )
			&& in_rSerializer.Put( m_fAttenuationScalingFactor );
	}

	bool SetListenerScalingFactor::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_ulIndex )
			&& in_rSerializer.Get( m_fAttenuationScalingFactor );
	}

	SetListenerSpatialization::SetListenerSpatialization()
		:	CommandData( AK::Comm::IRendererProxy::MethodSetListenerSpatialization )
	{}

	SetListenerSpatialization::~SetListenerSpatialization()
	{
		if ( m_bWasDeserialized && m_pVolumeOffsets )
		{
			CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pVolumeOffsets );
		}
	}

	bool SetListenerSpatialization::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		bool bRet = __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_uIndex )
			&& in_rSerializer.Put( m_bSpatialized )
			&& in_rSerializer.Put( m_bUseVolumeOffsets )
			&& in_rSerializer.Put( m_channelConfig );
		if ( bRet && m_bUseVolumeOffsets )
			bRet = in_rSerializer.SerializeArray( m_channelConfig.uNumChannels, m_pVolumeOffsets );
		return bRet;
	}
	
	bool SetListenerSpatialization::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_uIndex )
			&& in_rSerializer.Get( m_bSpatialized )
			&& in_rSerializer.Get( m_bUseVolumeOffsets )
			&& in_rSerializer.Get( m_channelConfig );
		if ( bRet && m_bUseVolumeOffsets )
		{
			AkUInt32 uNumChannels;
			bRet = in_rSerializer.DeserializeArray( uNumChannels, m_pVolumeOffsets );
			AKASSERT( uNumChannels == m_channelConfig.uNumChannels );
		}
		return bRet;
	}

	SetRTPC::SetRTPC()
		: CommandData( AK::Comm::IRendererProxy::MethodSetRTPC )
	{}

	bool SetRTPC::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_RTPCid )
			&& in_rSerializer.Put( m_value )
			&& in_rSerializer.Put( m_gameObj )
			&& in_rSerializer.Put( m_bBypassInternalValueInterpolation );
	}

	bool SetRTPC::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_RTPCid )
			&& in_rSerializer.Get( m_value )
			&& in_rSerializer.Get( m_gameObj )
			&& in_rSerializer.Get( m_bBypassInternalValueInterpolation );
	}

	SetDefaultRTPCValue::SetDefaultRTPCValue()
		: CommandData( AK::Comm::IRendererProxy::MethodSetDefaultRTPCValue )
	{}

	bool SetDefaultRTPCValue::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_RTPCid )
			&& in_rSerializer.Put( m_defaultValue );
	}

	bool SetDefaultRTPCValue::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_RTPCid )
			&& in_rSerializer.Get( m_defaultValue );
	}

	SetRTPCRamping::SetRTPCRamping()
		: CommandData( AK::Comm::IRendererProxy::MethodSetRTPCRamping )
	{}

	bool SetRTPCRamping::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_RTPCid )
			&& in_rSerializer.Put( m_rampingType )
			&& in_rSerializer.Put( m_fRampUp )
			&& in_rSerializer.Put( m_fRampDown );
	}

	bool SetRTPCRamping::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_RTPCid )
			&& in_rSerializer.Get( m_rampingType )
			&& in_rSerializer.Get( m_fRampUp )
			&& in_rSerializer.Get( m_fRampDown );
	}
	
	RTPCBindToBuiltInParam::RTPCBindToBuiltInParam()
		: CommandData( AK::Comm::IRendererProxy::MethodRTPCBindToBuiltInParam )
	{}

	bool RTPCBindToBuiltInParam::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_RTPCid )
			&& in_rSerializer.PutEnum( m_bindToParam );
	}

	bool RTPCBindToBuiltInParam::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool res = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_RTPCid )
			&& in_rSerializer.GetEnum( m_bindToParam );
		return res;
	}

	ResetRTPC::ResetRTPC()
		: CommandData( AK::Comm::IRendererProxy::MethodResetRTPC )
	{}

	bool ResetRTPC::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_gameObj );
	}

	bool ResetRTPC::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_gameObj );
	}

	ResetRTPCValue::ResetRTPCValue()
		: CommandData( AK::Comm::IRendererProxy::MethodResetRTPCValue )
	{}

	bool ResetRTPCValue::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_RTPCid )
			&& in_rSerializer.Put( m_gameObj )
			&& in_rSerializer.Put( m_bBypassInternalValueInterpolation );
	}

	bool ResetRTPCValue::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_RTPCid )
			&& in_rSerializer.Get( m_gameObj )
			&& in_rSerializer.Get( m_bBypassInternalValueInterpolation );
	}

	SetSwitch::SetSwitch()
		: CommandData( AK::Comm::IRendererProxy::MethodSetSwitch )
	{}

	bool SetSwitch::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_switchGroup )
			&& in_rSerializer.Put( m_switchState )
			&& in_rSerializer.Put( m_gameObj );
	}

	bool SetSwitch::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_switchGroup )
			&& in_rSerializer.Get( m_switchState )
			&& in_rSerializer.Get( m_gameObj );
	}

	PostTrigger::PostTrigger()
		: CommandData( AK::Comm::IRendererProxy::MethodPostTrigger )
	{}

	bool PostTrigger::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_trigger )
			&& in_rSerializer.Put( m_gameObj );
	}

	bool PostTrigger::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_trigger )
			&& in_rSerializer.Get( m_gameObj );
	}

	ResetSwitches::ResetSwitches()
		: CommandData( AK::Comm::IRendererProxy::MethodResetSwitches )
	{}

	bool ResetSwitches::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_gameObj );
	}

	bool ResetSwitches::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_gameObj );
	}

	ResetAllStates::ResetAllStates()
		: CommandData( AK::Comm::IRendererProxy::MethodResetAllStates )
	{}

	bool ResetAllStates::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer );
	}

	bool ResetAllStates::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer );
	}

	ResetRndSeqCntrPlaylists::ResetRndSeqCntrPlaylists()
		: CommandData( AK::Comm::IRendererProxy::MethodResetRndSeqCntrPlaylists )
	{}

	bool ResetRndSeqCntrPlaylists::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer );
	}

	bool ResetRndSeqCntrPlaylists::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer );
	}

	SetGameObjectAuxSendValues::SetGameObjectAuxSendValues()
		: CommandData( AK::Comm::IRendererProxy::MethodSetGameObjectAuxSendValues )
	{}

	SetGameObjectAuxSendValues::~SetGameObjectAuxSendValues()
	{
		if( m_bWasDeserialized && m_aEnvironmentValues )
		{
			CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_aEnvironmentValues );
		}
	}

	bool SetGameObjectAuxSendValues::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_gameObjectID )
			&& in_rSerializer.SerializeArray( m_uNumEnvValues, m_aEnvironmentValues);

	}

	bool SetGameObjectAuxSendValues::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_gameObjectID )
			&& in_rSerializer.DeserializeArray( m_uNumEnvValues, m_aEnvironmentValues);

		return bRet;
	}

	SetGameObjectOutputBusVolume::SetGameObjectOutputBusVolume()
		: CommandData( AK::Comm::IRendererProxy::MethodSetGameObjectOutputBusVolume )
	{}

	bool SetGameObjectOutputBusVolume::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
		&& in_rSerializer.Put( m_emitterID )
		&& in_rSerializer.Put( m_listenerID )
		&& in_rSerializer.Put( m_fControlValue );
	}
		
	bool SetGameObjectOutputBusVolume::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
		&& in_rSerializer.Get( m_emitterID )
		&& in_rSerializer.Get( m_listenerID )
		&& in_rSerializer.Get( m_fControlValue );
	}

	SetObjectObstructionAndOcclusion::SetObjectObstructionAndOcclusion()
		: CommandData( AK::Comm::IRendererProxy::MethodSetObjectObstructionAndOcclusion )	
	{}

	bool SetObjectObstructionAndOcclusion::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
		&& in_rSerializer.Put( m_EmitterID )
		&& in_rSerializer.Put( m_ListenerID )
		&& in_rSerializer.Put( m_fObstructionLevel )
		&& in_rSerializer.Put( m_fOcclusionLevel );
	}
		
	bool SetObjectObstructionAndOcclusion::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
		&& in_rSerializer.Get( m_EmitterID )
		&& in_rSerializer.Get( m_ListenerID )
		&& in_rSerializer.Get( m_fObstructionLevel )
		&& in_rSerializer.Get( m_fOcclusionLevel );
	}

	SetMultipleObstructionAndOcclusion::SetMultipleObstructionAndOcclusion()
		: CommandData(AK::Comm::IRendererProxy::MethodSetMultipleObstructionAndOcclusion)
	{}

	SetMultipleObstructionAndOcclusion::~SetMultipleObstructionAndOcclusion()
	{
		if (m_bWasDeserialized && m_fObstructionOcclusionValues)
		{
			CommandDataSerializer::FreeArrayMem(AkMemID_Profiler, m_fObstructionOcclusionValues);
		}
	}

	bool SetMultipleObstructionAndOcclusion::Serialize(CommandDataSerializer& in_rSerializer) const
	{
		return __base::Serialize(in_rSerializer)
			&& in_rSerializer.Put(m_EmitterID)
			&& in_rSerializer.Put(m_ListenerID)
			&& in_rSerializer.Put(m_uNumOcclusionObstruction)
			&& in_rSerializer.SerializeArray(m_uNumOcclusionObstruction, m_fObstructionOcclusionValues);
	}

	bool SetMultipleObstructionAndOcclusion::Deserialize(CommandDataSerializer& in_rSerializer)
	{
		return __base::Deserialize(in_rSerializer)
		&& in_rSerializer.Get(m_EmitterID)
		&& in_rSerializer.Get(m_ListenerID)
		&& in_rSerializer.Get(m_uNumOcclusionObstruction)
		&& in_rSerializer.DeserializeArray(m_uNumOcclusionObstruction, m_fObstructionOcclusionValues);
	}

	SetObsOccCurve::SetObsOccCurve()
		: CommandData( AK::Comm::IRendererProxy::MethodSetObsOccCurve )
	{}

	bool SetObsOccCurve::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return CommandData::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_curveXType )
			&& in_rSerializer.Put( m_curveYType )
			&& ProxyCommandData::CurveData<AkRTPCGraphPoint>::Serialize(in_rSerializer);
	}

	bool SetObsOccCurve::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return CommandData::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_curveXType )
			&& in_rSerializer.Get( m_curveYType )
			&& ProxyCommandData::CurveData<AkRTPCGraphPoint>::Deserialize(in_rSerializer);
	}


	SetObsOccCurveEnabled::SetObsOccCurveEnabled()
		: CommandData( AK::Comm::IRendererProxy::MethodSetObsOccCurveEnabled )
	{}

	SetObsOccCurveEnabled::~SetObsOccCurveEnabled()
	{
	}

	bool SetObsOccCurveEnabled::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		bool bRet = __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_curveXType )
			&& in_rSerializer.Put( m_curveYType )
			&& in_rSerializer.Put( m_bEnabled );

		return bRet;
	}

	bool SetObsOccCurveEnabled::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_curveXType )
			&& in_rSerializer.Get( m_curveYType )
			&& in_rSerializer.Get( m_bEnabled );

		return bRet;
	}

	AddSwitchRTPC::AddSwitchRTPC()
		: CommandData( AK::Comm::IRendererProxy::MethodAddSwitchRTPC )
	{}

	AddSwitchRTPC::~AddSwitchRTPC()
	{
		if( m_bWasDeserialized && m_pArrayConversion )
		{
			CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pArrayConversion );
		}
	}

	bool AddSwitchRTPC::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_uSwitchGroup )
			&& in_rSerializer.Put( m_RTPC_ID )
			&& in_rSerializer.PutEnum( m_RTPC_Type )
			&& in_rSerializer.SerializeArray( m_uArraySize, m_pArrayConversion);

	}

	bool AddSwitchRTPC::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_uSwitchGroup )
			&& in_rSerializer.Get( m_RTPC_ID )
			&& in_rSerializer.GetEnum( m_RTPC_Type )
			&& in_rSerializer.DeserializeArray( m_uArraySize, m_pArrayConversion);

		return bRet;
	}

	RemoveSwitchRTPC::RemoveSwitchRTPC()
		: CommandData( AK::Comm::IRendererProxy::MethodRemoveSwitchRTPC )
	{}

	bool RemoveSwitchRTPC::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_uSwitchGroup );
	}

	bool RemoveSwitchRTPC::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_uSwitchGroup );
	}

	SetVolumeThreshold::SetVolumeThreshold()
		: CommandData( AK::Comm::IRendererProxy::MethodSetVolumeThreshold )
	{}

	bool SetVolumeThreshold::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_fVolumeThreshold );
	}

	bool SetVolumeThreshold::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_fVolumeThreshold );
	}

	SetMaxNumVoicesLimit::SetMaxNumVoicesLimit()
		: CommandData( AK::Comm::IRendererProxy::MethodSetMaxNumVoicesLimit )
	{}

	bool SetMaxNumVoicesLimit::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_maxNumberVoices );
	}

	bool SetMaxNumVoicesLimit::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_maxNumberVoices );
	}

	SetMaxNumDangerousVirtVoicesLimit::SetMaxNumDangerousVirtVoicesLimit()
		: CommandData( AK::Comm::IRendererProxy::MethodSetMaxNumDangerousVirtVoicesLimit )
	{}

	bool SetMaxNumDangerousVirtVoicesLimit::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_maxNumberVoices );
	}

	bool SetMaxNumDangerousVirtVoicesLimit::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize(in_rSerializer)
			&& in_rSerializer.Get( m_maxNumberVoices );
	}

	PostMsgMonitor::PostMsgMonitor()
		: CommandData( AK::Comm::IRendererProxy::MethodPostMsgMonitor )
	{}

	bool PostMsgMonitor::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( (const AkUtf16*)m_pszMessage );
	}
	
	bool PostMsgMonitor::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		AkUInt32 uDummy = 0;

		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_pszMessage, uDummy );
	}

	StopAll::StopAll()
		: CommandData( AK::Comm::IRendererProxy::MethodStopAll )
	{}

	bool StopAll::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_GameObjPtr );
	}

	bool StopAll::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_GameObjPtr );
	}

	StopPlayingID::StopPlayingID()
		: CommandData( AK::Comm::IRendererProxy::MethodStopPlayingID )
	{}

	bool StopPlayingID::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_playingID )
			&& in_rSerializer.Put( m_uTransitionDuration )
			&& in_rSerializer.PutEnum( m_eFadeCurve );
	}

	bool StopPlayingID::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_playingID )
			&& in_rSerializer.Get( m_uTransitionDuration )
			&& in_rSerializer.GetEnum( m_eFadeCurve );
	}

	ExecuteActionOnPlayingID::ExecuteActionOnPlayingID()
		: CommandData(AK::Comm::IRendererProxy::MethodExecuteActionOnPlayingID)
	{}

	bool ExecuteActionOnPlayingID::Serialize(CommandDataSerializer& in_rSerializer) const
	{
		return __base::Serialize(in_rSerializer)
			&& in_rSerializer.PutEnum(m_eActionType)
			&& in_rSerializer.Put(m_playingID)
			&& in_rSerializer.Put(m_uTransitionDuration)
			&& in_rSerializer.PutEnum(m_eFadeCurve);
	}

	bool ExecuteActionOnPlayingID::Deserialize(CommandDataSerializer& in_rSerializer)
	{
		return __base::Deserialize(in_rSerializer)
			&& in_rSerializer.GetEnum(m_eActionType)
			&& in_rSerializer.Get(m_playingID)
			&& in_rSerializer.Get(m_uTransitionDuration)
			&& in_rSerializer.GetEnum(m_eFadeCurve);
	}


	RegisterPlugin::RegisterPlugin(): CommandData(AK::Comm::IRendererProxy::MethodRegisterPlugin)
	{}

	bool RegisterPlugin::Serialize(CommandDataSerializer& in_rSerializer) const
	{		
		return __base::Serialize(in_rSerializer)
			&& in_rSerializer.Put((const char*)m_szPlugin);
	}

	bool RegisterPlugin::Deserialize(CommandDataSerializer& in_rSerializer)
	{
		AkUInt32 uSize;
		return __base::Deserialize(in_rSerializer)
			&& in_rSerializer.Get(m_szPlugin, uSize);
	}

	MonitoringMuteGameObj::MonitoringMuteGameObj()
		: CommandData(AK::Comm::IRendererProxy::MethodMonitoringMuteGameObj)
	{}

	bool MonitoringMuteGameObj::Serialize(CommandDataSerializer& in_rSerializer) const
	{
		return __base::Serialize(in_rSerializer)
			&& in_rSerializer.Put(m_gameObjectPtr)
			&& in_rSerializer.Put(m_bMute);
	}

	bool MonitoringMuteGameObj::Deserialize(CommandDataSerializer& in_rSerializer)
	{
		return __base::Deserialize(in_rSerializer)
			&& in_rSerializer.Get(m_gameObjectPtr)
			&& in_rSerializer.Get(m_bMute);
	}

	MonitoringSoloGameObj::MonitoringSoloGameObj()
		: CommandData(AK::Comm::IRendererProxy::MethodMonitoringSoloGameObj)
	{}

	bool MonitoringSoloGameObj::Serialize(CommandDataSerializer& in_rSerializer) const
	{
		return __base::Serialize(in_rSerializer)
			&& in_rSerializer.Put(m_gameObjectPtr)
			&& in_rSerializer.Put(m_bSolo);
	}

	bool MonitoringSoloGameObj::Deserialize(CommandDataSerializer& in_rSerializer)
	{
		return __base::Deserialize(in_rSerializer)
			&& in_rSerializer.Get(m_gameObjectPtr)
			&& in_rSerializer.Get(m_bSolo);
	}
}

namespace ALMonitorProxyCommandData
{
	CommandData::CommandData()
		: ProxyCommandData::CommandData()
	{}

	CommandData::CommandData( AkUInt16 in_methodID )
		: ProxyCommandData::CommandData( ProxyCommandData::TypeALMonitorProxy, in_methodID )
	{}

	bool CommandData::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer );
	}

	bool CommandData::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer );
	}

	Monitor::Monitor()
		: CommandData( IALMonitorSubProxy::MethodMonitor )
	{}

	bool Monitor::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_uWhatToMonitor );
	}

	bool Monitor::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_uWhatToMonitor );
	}

	SetMeterWatches::SetMeterWatches()
		: CommandData( IALMonitorSubProxy::MethodSetMeterWatches )
		, m_uiWatchCount( 0 )
		, m_pWatches( NULL )
	{}

	SetMeterWatches::~SetMeterWatches()
	{
		if( m_bWasDeserialized && m_pWatches )
		{
			CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pWatches );
		}
	}

	bool SetMeterWatches::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )			
			&& in_rSerializer.SerializeArray( m_uiWatchCount, m_pWatches);

	}

	bool SetMeterWatches::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )			
			&& in_rSerializer.DeserializeArray( m_uiWatchCount, m_pWatches);

		return bRet;
	}
}

namespace StateMgrProxyCommandData
{
	CommandData::CommandData()
		: ProxyCommandData::CommandData()
	{}

	CommandData::CommandData( AkUInt16 in_methodID )
		: ProxyCommandData::CommandData( ProxyCommandData::TypeStateMgrProxy, in_methodID )
	{}

	bool CommandData::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer );
	}

	bool CommandData::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer );
	}

	AddStateGroup::AddStateGroup()
		: CommandData( IStateMgrProxy::MethodAddStateGroup )
	{}

	bool AddStateGroup::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_groupID );
	}

	bool AddStateGroup::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_groupID );
	}

	RemoveStateGroup::RemoveStateGroup()
		: CommandData( IStateMgrProxy::MethodRemoveStateGroup )
	{}

	bool RemoveStateGroup::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_groupID );
	}

	bool RemoveStateGroup::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_groupID );
	}

	AddStateTransition::AddStateTransition()
		: CommandData( IStateMgrProxy::MethodAddStateTransition )
	{}

	bool AddStateTransition::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_groupID )
			&& in_rSerializer.Put( m_stateID1 )
			&& in_rSerializer.Put( m_stateID2 )
			&& in_rSerializer.Put( m_transitionTime )
			&& in_rSerializer.Put( m_bIsShared );
	}

	bool AddStateTransition::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_groupID )
			&& in_rSerializer.Get( m_stateID1 )
			&& in_rSerializer.Get( m_stateID2 )
			&& in_rSerializer.Get( m_transitionTime )
			&& in_rSerializer.Get( m_bIsShared );
	}

	RemoveStateTransition::RemoveStateTransition()
		: CommandData( IStateMgrProxy::MethodRemoveStateTransition )
	{}

	bool RemoveStateTransition::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_groupID )
			&& in_rSerializer.Put( m_stateID1 )
			&& in_rSerializer.Put( m_stateID2 )
			&& in_rSerializer.Put( m_bIsShared );
	}

	bool RemoveStateTransition::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_groupID )
			&& in_rSerializer.Get( m_stateID1 )
			&& in_rSerializer.Get( m_stateID2 )
			&& in_rSerializer.Get( m_bIsShared );
	}

	ClearStateTransitions::ClearStateTransitions()
		: CommandData( IStateMgrProxy::MethodClearStateTransitions )
	{}

	bool ClearStateTransitions::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_groupID );
	}

	bool ClearStateTransitions::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_groupID );
	}

	SetDefaultTransitionTime::SetDefaultTransitionTime()
		: CommandData( IStateMgrProxy::MethodSetDefaultTransitionTime )
	{}

	bool SetDefaultTransitionTime::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_groupID )
			&& in_rSerializer.Put( m_transitionTime );
	}

	bool SetDefaultTransitionTime::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_groupID )
			&& in_rSerializer.Get( m_transitionTime );
	}

	SetState::SetState()
		: CommandData( IStateMgrProxy::MethodSetState )
	{}

	bool SetState::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_groupID )
			&& in_rSerializer.Put( m_stateID );
	}

	bool SetState::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_groupID )
			&& in_rSerializer.Get( m_stateID );
	}

	GetState::GetState()
		: CommandData( IStateMgrProxy::MethodGetState )
	{}

	bool GetState::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_groupID );
	}

	bool GetState::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_groupID );
	}
}

namespace ObjectProxyStoreCommandData
{
	CommandData::CommandData()
		: ProxyCommandData::CommandData()
	{}

	CommandData::CommandData( AkUInt16 in_methodID ) 
		: ProxyCommandData::CommandData( ProxyCommandData::TypeObjectProxyStore, in_methodID )
	{}

	bool CommandData::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_proxyInstancePtr )
			&& in_rSerializer.Put( m_objectID );
	}

	bool CommandData::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_proxyInstancePtr )
			&& in_rSerializer.Get( m_objectID );
	}

	Create::Create()
		: CommandData( MethodCreate )
	{}

	bool Create::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.PutEnum( m_eObjectType )
			&& in_rSerializer.PutEnum( m_actionType );
	}

	bool Create::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.GetEnum( m_eObjectType )
			&& in_rSerializer.GetEnum( m_actionType );
	}

	Release::Release()
		: CommandData( MethodRelease )
	{}

	bool Release::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer );
	}

	bool Release::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer );
	}
}

namespace ObjectProxyCommandData
{
	bool CommandData::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_proxyInstancePtr )
			&& in_rSerializer.Put( m_objectID )	;
	}

	bool CommandData::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_proxyInstancePtr )
			&& in_rSerializer.Get( m_objectID );
	}
}

namespace AudioNodeProxyCommandData
{
}

namespace ParameterableProxyCommandData
{
	SetRTPC::SetRTPC()
		: CommandData( IParameterNodeProxy::MethodSetRTPC )
	{}

	bool SetRTPC::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return CommandData::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_RTPCID )
			&& in_rSerializer.PutEnum( m_RTPCType )
			&& in_rSerializer.PutEnum( m_RTPCAccum )
			&& in_rSerializer.PutEnum( m_paramID )
			&& in_rSerializer.Put( m_RTPCCurveID )
			&& ProxyCommandData::CurveData<AkRTPCGraphPoint>::Serialize( in_rSerializer );
	}

	bool SetRTPC::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return CommandData::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_RTPCID )
			&& in_rSerializer.GetEnum( m_RTPCType )
			&& in_rSerializer.GetEnum( m_RTPCAccum )
			&& in_rSerializer.GetEnum( m_paramID )
			&& in_rSerializer.Get( m_RTPCCurveID )
			&& ProxyCommandData::CurveData<AkRTPCGraphPoint>::Deserialize( in_rSerializer );
	}


	SetStateProperties::SetStateProperties()
		: ObjectProxyCommandData::CommandData0<IParameterableProxy::MethodSetStateProperties>() 
	{
		m_uProperties = 0;
		m_pProperties = NULL;
	}

	SetStateProperties::SetStateProperties(IObjectProxy * in_pProxy)
		: ObjectProxyCommandData::CommandData0<IParameterableProxy::MethodSetStateProperties>(in_pProxy)
	{
		m_pProperties = NULL;
	}

	SetStateProperties::~SetStateProperties()
	{
		if (m_bWasDeserialized && m_pProperties)
		{
			CommandDataSerializer::FreeArrayMem(AkMemID_Profiler, m_pProperties);
		}
	}

	bool SetStateProperties::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_uProperties )
			&& in_rSerializer.SerializeArray(m_uProperties, m_pProperties);
	}

	bool SetStateProperties::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return  __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_uProperties )
			&& in_rSerializer.DeserializeArray(m_uProperties, m_pProperties);
	}


	UpdateStateGroups::UpdateStateGroups()
		: ObjectProxyCommandData::CommandData0<IParameterableProxy::MethodUpdateStateGroups>() 
	{
		m_pStates = NULL;
		m_pGroups = NULL;
	}

	UpdateStateGroups::UpdateStateGroups(IObjectProxy * in_pProxy)
		: ObjectProxyCommandData::CommandData0<IParameterableProxy::MethodUpdateStateGroups>(in_pProxy)
	{
		m_pStates = NULL;
		m_pGroups = NULL;
	}

	UpdateStateGroups::~UpdateStateGroups()
	{
		if (m_bWasDeserialized)
		{
			if (m_pStates)
				CommandDataSerializer::FreeArrayMem(AkMemID_Profiler, m_pStates);
			if (m_pGroups)
				CommandDataSerializer::FreeArrayMem(AkMemID_Profiler, m_pGroups);
		}
	}

	bool UpdateStateGroups::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		AkUInt32 count = 0;
		for (AkUInt32 i = 0; i < m_uGroups; i++)
			count += m_pGroups[i].ulStateCount;

		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_uGroups )
			&& in_rSerializer.SerializeArray(m_uGroups, m_pGroups)
			&& in_rSerializer.SerializeArray(count, m_pStates);
	}

	bool UpdateStateGroups::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		AkUInt32 uStateCount;
		return  __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_uGroups )
			&& in_rSerializer.DeserializeArray(m_uGroups, m_pGroups)
			&& in_rSerializer.DeserializeArray(uStateCount, m_pStates);
	}


	UpdateEffects::UpdateEffects() 
		: ObjectProxyCommandData::CommandData0<IParameterableProxy::MethodUpdateEffects>() 
	{
		m_pUpdates = NULL;
	}

	UpdateEffects::UpdateEffects(IObjectProxy * in_pProxy)
		: ObjectProxyCommandData::CommandData0<IParameterableProxy::MethodUpdateEffects>(in_pProxy)
	{
		m_pUpdates = NULL;
	}

	UpdateEffects::~UpdateEffects() 
	{
		if (m_pUpdates && m_bWasDeserialized)
			CommandDataSerializer::FreeArrayMem(AkMemID_Profiler, m_pUpdates);
	}

	bool UpdateEffects::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.SerializeArray(m_uCount, m_pUpdates);
	}

	bool UpdateEffects::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.DeserializeArray(m_uCount, m_pUpdates);
	}
}

namespace ParameterNodeProxyCommandData
{
	PosSetPath::PosSetPath()
		: CommandData( IParameterNodeProxy::MethodPosSetPath )
		, m_pArrayVertex( NULL )
		, m_pArrayPlaylist( NULL )
	{}

	PosSetPath::~PosSetPath()
	{
		if( m_bWasDeserialized )
		{
			if( m_pArrayVertex )
				CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pArrayVertex );
			if( m_pArrayPlaylist )
				CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pArrayPlaylist );
		}
	}

	bool PosSetPath::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.SerializeArray( m_ulNumVertices, m_pArrayVertex)
			&& in_rSerializer.SerializeArray( m_ulNumPlaylistItem, m_pArrayPlaylist);

	}

	bool PosSetPath::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.DeserializeArray( m_ulNumVertices, m_pArrayVertex)
			&& in_rSerializer.DeserializeArray( m_ulNumPlaylistItem, m_pArrayPlaylist);

		return bRet;
	}
}

namespace SoundProxyCommandData
{
	SetSource::SetSource()
		: CommandData( ISoundProxy::MethodSetSource )
	{}

	bool SetSource::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		AKASSERT( !"Serialization functions should not be called on remote setSource" );
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( (const char*)m_pszSourceName );
	}

	bool SetSource::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		AKASSERT( !"Serialization functions should not be called on remote setSource" );
		AKASSERT( sizeof( char ) == 1 );
		
		AkUInt32 uDummy = 0;

		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( (char*&)m_pszSourceName, uDummy );
	}
}

namespace TrackProxyCommandData
{
	SetPlayList::SetPlayList()
		: CommandData( ITrackProxy::MethodSetPlayList )
		, m_pArrayPlaylistItems( NULL )
	{}

	SetPlayList::~SetPlayList()
	{
		if( m_bWasDeserialized && m_pArrayPlaylistItems )
		{
			CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pArrayPlaylistItems );
		}
	}

	bool SetPlayList::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_uNumSubTrack )
			&& in_rSerializer.SerializeArray( m_uNumPlaylistItem, m_pArrayPlaylistItems);

	}

	bool SetPlayList::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_uNumSubTrack )
			&& in_rSerializer.DeserializeArray( m_uNumPlaylistItem, m_pArrayPlaylistItems);

		return bRet;
	}

	AddClipAutomation::AddClipAutomation()
		: CommandData( ITrackProxy::MethodAddClipAutomation )
		, m_pArrayPoints( NULL )
	{}

	AddClipAutomation::~AddClipAutomation()
	{
		if( m_bWasDeserialized && m_pArrayPoints )
		{
			CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pArrayPoints );
		}
	}

	bool AddClipAutomation::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_uClipIndex )
			&& in_rSerializer.PutEnum( m_eAutomationType )
			&& in_rSerializer.SerializeArray( m_uNumPoints, m_pArrayPoints );
	}

	bool AddClipAutomation::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_uClipIndex )
			&& in_rSerializer.GetEnum( m_eAutomationType )
			&& in_rSerializer.DeserializeArray( m_uNumPoints, m_pArrayPoints );

		return bRet;
	}


	SetSwitchGroup::SetSwitchGroup()
		: CommandData( ITrackProxy::MethodSetSwitchGroup )
		, m_ulGroup( 0 )
		, m_eGroupType( AkGroupType_Switch )
	{}

	SetSwitchGroup::~SetSwitchGroup()
	{
	}

	bool SetSwitchGroup::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_ulGroup )
			&& in_rSerializer.PutEnum( m_eGroupType );
	}

	bool SetSwitchGroup::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_ulGroup )
			&& in_rSerializer.GetEnum( m_eGroupType );

		return bRet;
	}


	SetDefaultSwitch::SetDefaultSwitch()
		: CommandData( ITrackProxy::MethodSetDefaultSwitch )
		, m_ulSwitch( 0 )
	{}

	SetDefaultSwitch::~SetDefaultSwitch()
	{
	}

	bool SetDefaultSwitch::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_ulSwitch );
	}

	bool SetDefaultSwitch::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_ulSwitch );

		return bRet;
	}


	SetSwitchAssoc::SetSwitchAssoc()
		: CommandData( ITrackProxy::MethodSetSwitchAssoc )
		, m_ulNumAssoc( 0 )
		, m_pulAssoc( NULL )
	{}

	SetSwitchAssoc::~SetSwitchAssoc()
	{
		if( m_bWasDeserialized && m_pulAssoc )
		{
			CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pulAssoc );
		}
	}

	bool SetSwitchAssoc::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.SerializeArray( m_ulNumAssoc, m_pulAssoc );
	}

	bool SetSwitchAssoc::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.DeserializeArray( m_ulNumAssoc, m_pulAssoc );

		return bRet;
	}


	SetTransRule::SetTransRule()
		: CommandData( ITrackProxy::MethodSetTransRule )
	{}

	SetTransRule::~SetTransRule()
	{
	}

	bool SetTransRule::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_transRule );
	}

	bool SetTransRule::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_transRule );

		return bRet;
	}
}

namespace AttenuationProxyCommandData
{
	SetAttenuationParams::SetAttenuationParams()
		: CommandData( IAttenuationProxy::MethodSetAttenuationParams )
	{
		m_Params.paCurves = NULL;
		m_Params.paRTPCReg = NULL;
	}

	SetAttenuationParams::~SetAttenuationParams()
	{
		if( m_bWasDeserialized )
		{
			if( m_Params.paCurves )
			{
				for( AkUInt32 i = 0; i < m_Params.uNumCurves; ++i )
				{
					if( m_Params.paCurves[i].m_pArrayConversion )
					{
						CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_Params.paCurves[i].m_pArrayConversion );
					}
				}
				CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_Params.paCurves );
			}

			if( m_Params.paRTPCReg )
			{
				for( AkUInt32 i = 0; i < m_Params.uNumRTPCReg; ++i )
				{
					if( m_Params.paRTPCReg[i].m_pArrayConversion )
					{
						CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_Params.paRTPCReg[i].m_pArrayConversion );
					}
				}
				CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_Params.paRTPCReg );
			}
		}
	}

	bool SetAttenuationParams::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		bool bRet = __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_Params.Cone.bIsConeEnabled )
			&& in_rSerializer.Put( m_Params.Cone.cone_fInsideAngle )
			&& in_rSerializer.Put( m_Params.Cone.cone_fOutsideAngle )
			&& in_rSerializer.Put( m_Params.Cone.cone_fOutsideVolume )
			&& in_rSerializer.Put( m_Params.Cone.cone_LoPass )
			&& in_rSerializer.Put( m_Params.Cone.cone_HiPass );

		for( AkUInt32 i = 0; i < AttenuationCurveID_MaxCount && bRet; ++i )
		{
			bRet = in_rSerializer.Put( m_Params.CurveIndexes[i] );
		}

		if (bRet)
			bRet = in_rSerializer.SerializeArray( m_Params.uNumCurves, m_Params.paCurves);
		if (bRet)
			bRet = in_rSerializer.SerializeArray( m_Params.uNumRTPCReg, m_Params.paRTPCReg);

		return bRet;
	}

	bool SetAttenuationParams::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_Params.Cone.bIsConeEnabled )
			&& in_rSerializer.Get( m_Params.Cone.cone_fInsideAngle )
			&& in_rSerializer.Get( m_Params.Cone.cone_fOutsideAngle )
			&& in_rSerializer.Get( m_Params.Cone.cone_fOutsideVolume )
			&& in_rSerializer.Get( m_Params.Cone.cone_LoPass )
			&& in_rSerializer.Get( m_Params.Cone.cone_HiPass );

		for( AkUInt32 i = 0; i < AttenuationCurveID_MaxCount && bRet; ++i )
		{
			bRet = in_rSerializer.Get( m_Params.CurveIndexes[i] );
		}

		if (bRet)
		{
			bRet = in_rSerializer.DeserializeArray( m_Params.uNumCurves, m_Params.paCurves);
			AKASSERT( !bRet || m_Params.uNumCurves );//cannot be 0 at this point, at least one is required.
		}
		if (bRet)
			bRet = in_rSerializer.DeserializeArray( m_Params.uNumRTPCReg, m_Params.paRTPCReg);

		return bRet;
	}
}

namespace ModulatorProxyCommandData
{
	SetRTPC::SetRTPC()
		: CommandData( IModulatorProxy::MethodSetRTPC )
	{}

	SetRTPC::~SetRTPC()
	{}

	bool SetRTPC::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		bool bRet = __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_rtpcID )
			&& in_rSerializer.PutEnum( m_rtpcType )
			&& in_rSerializer.PutEnum( m_rtpcAccum )
			&& in_rSerializer.PutEnum( m_paramID )
			&& in_rSerializer.Put( m_rtpcCurveID )
			&& ProxyCommandData::CurveData<AkRTPCGraphPoint>::Serialize( in_rSerializer );

		return bRet;
	}

	bool SetRTPC::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_rtpcID )
			&& in_rSerializer.GetEnum( m_rtpcType )
			&& in_rSerializer.GetEnum( m_rtpcAccum )
			&& in_rSerializer.GetEnum( m_paramID )
			&& in_rSerializer.Get( m_rtpcCurveID )
			&& ProxyCommandData::CurveData<AkRTPCGraphPoint>::Deserialize( in_rSerializer );

		return bRet;
	}


	UnsetRTPC::UnsetRTPC()
		: CommandData( IModulatorProxy::MethodSetRTPC )
	{}

	UnsetRTPC::~UnsetRTPC()
	{}

	bool UnsetRTPC::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		bool bRet = __base::Serialize( in_rSerializer )
			&& in_rSerializer.PutEnum( m_paramID )
			&& in_rSerializer.Put( m_rtpcCurveID );

		return bRet;
	}

	bool UnsetRTPC::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.GetEnum( m_paramID )
			&& in_rSerializer.Get( m_rtpcCurveID );

		return bRet;
	}
}

namespace RanSeqContainerProxyCommandData
{
	SetPlaylist::SetPlaylist()
		: CommandData( IRanSeqContainerProxy::MethodSetPlaylist )
	{}

	bool SetPlaylist::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_pvListBlock, m_ulParamBlockSize );
	}

	bool SetPlaylist::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_pvListBlock, m_ulParamBlockSize );
	}

	NextToPlay::NextToPlay()
		: CommandData( IRanSeqContainerProxy::MethodNextToPlay )
	{}

	bool NextToPlay::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_gameObjPtr );
	}

	bool NextToPlay::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_gameObjPtr );
	}
}

namespace SwitchContainerProxyCommandData
{
	SetSwitchGroup::SetSwitchGroup()
		: CommandData( ISwitchContainerProxy::MethodSetSwitchGroup )
	{}

	bool SetSwitchGroup::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_ulGroup )
			&& in_rSerializer.PutEnum( m_eGroupType );
	}

	bool SetSwitchGroup::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_ulGroup )
			&& in_rSerializer.GetEnum( m_eGroupType );
	}

	SetDefaultSwitch::SetDefaultSwitch()
		: CommandData( ISwitchContainerProxy::MethodSetDefaultSwitch )
	{}

	bool SetDefaultSwitch::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_switch );
	}

	bool SetDefaultSwitch::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_switch );
	}

	ClearSwitches::ClearSwitches()
		: CommandData( ISwitchContainerProxy::MethodClearSwitches )
	{}

	bool ClearSwitches::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer );
	}

	bool ClearSwitches::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer );
	}

	AddSwitch::AddSwitch()
		: CommandData( ISwitchContainerProxy::MethodAddSwitch )
	{}

	bool AddSwitch::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_switch );
	}

	bool AddSwitch::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_switch );
	}

	RemoveSwitch::RemoveSwitch()
		: CommandData( ISwitchContainerProxy::MethodRemoveSwitch )
	{}

	bool RemoveSwitch::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_switch );
	}

	bool RemoveSwitch::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_switch );
	}

	AddNodeInSwitch::AddNodeInSwitch()
		: CommandData( ISwitchContainerProxy::MethodAddNodeInSwitch )
	{}

	bool AddNodeInSwitch::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_switch )
			&& in_rSerializer.Put( m_nodeID );
	}

	bool AddNodeInSwitch::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_switch )
			&& in_rSerializer.Get( m_nodeID );
	}

	RemoveNodeFromSwitch::RemoveNodeFromSwitch()
		: CommandData( ISwitchContainerProxy::MethodRemoveNodeFromSwitch )
	{}

	bool RemoveNodeFromSwitch::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_switch )
			&& in_rSerializer.Put( m_nodeID );
	}

	bool RemoveNodeFromSwitch::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_switch )
			&& in_rSerializer.Get( m_nodeID );
	}

	SetContinuousValidation::SetContinuousValidation()
		: CommandData( ISwitchContainerProxy::MethodSetContinuousValidation )
	{}

	bool SetContinuousValidation::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_bIsContinuousCheck );
	}

	bool SetContinuousValidation::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_bIsContinuousCheck );
	}

	SetContinuePlayback::SetContinuePlayback()
		: CommandData( ISwitchContainerProxy::MethodSetContinuePlayback )
	{}

	bool SetContinuePlayback::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_nodeID )
			&& in_rSerializer.Put( m_bContinuePlayback );
	}

	bool SetContinuePlayback::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_nodeID )
			&& in_rSerializer.Get( m_bContinuePlayback );
	}

	SetFadeInTime::SetFadeInTime()
		: CommandData( ISwitchContainerProxy::MethodSetFadeInTime )
	{}

	bool SetFadeInTime::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_nodeID )
			&& in_rSerializer.Put( m_time );
	}

	bool SetFadeInTime::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_nodeID )
			&& in_rSerializer.Get( m_time );
	}

	SetFadeOutTime::SetFadeOutTime()
		: CommandData( ISwitchContainerProxy::MethodSetFadeOutTime )
	{}

	bool SetFadeOutTime::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_nodeID )
			&& in_rSerializer.Put( m_time );
	}

	bool SetFadeOutTime::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_nodeID )
			&& in_rSerializer.Get( m_time );
	}

	SetOnSwitchMode::SetOnSwitchMode()
		: CommandData( ISwitchContainerProxy::MethodSetOnSwitchMode )
	{}

	bool SetOnSwitchMode::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_nodeID )
			&& in_rSerializer.PutEnum( m_bSwitchMode );
	}

	bool SetOnSwitchMode::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_nodeID )
			&& in_rSerializer.GetEnum( m_bSwitchMode );
	}

	SetIsFirstOnly::SetIsFirstOnly()
		: CommandData( ISwitchContainerProxy::MethodSetIsFirstOnly )
	{}

	bool SetIsFirstOnly::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_nodeID )
			&& in_rSerializer.Put( m_bIsFirstOnly );
	}

	bool SetIsFirstOnly::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_nodeID )
			&& in_rSerializer.Get( m_bIsFirstOnly );
	}
}

namespace LayerContainerProxyCommandData
{
	//
	// AddLayer
	//

	AddLayer::AddLayer()
		: CommandData( ILayerContainerProxy::MethodAddLayer )
		, m_LayerID( 0 )
	{}

	bool AddLayer::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_LayerID );
	}

	bool AddLayer::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_LayerID );
	}

	//
	// RemoveLayer
	//

	RemoveLayer::RemoveLayer()
		: CommandData( ILayerContainerProxy::MethodRemoveLayer )
		, m_LayerID( 0 )
	{}

	bool RemoveLayer::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_LayerID );
	}

	bool RemoveLayer::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_LayerID );
	}
}

namespace LayerProxyCommandData
{
	//
	// SetRTPC
	//

	SetRTPC::SetRTPC()
		: CommandData( ILayerProxy::MethodSetRTPC )
		, m_RTPCID( 0 )
		, m_RTPCType( AkRtpcType_GameParameter )
		, m_RTPCAccum( AkRtpcAccum_Exclusive )
		, m_paramID( (AkRTPC_ParameterID)0 )
		, m_RTPCCurveID( 0 )
	{}

	bool SetRTPC::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return CommandData::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_RTPCID )
			&& in_rSerializer.PutEnum( m_RTPCType )
			&& in_rSerializer.PutEnum( m_RTPCAccum )
			&& in_rSerializer.PutEnum( m_paramID )
			&& in_rSerializer.Put( m_RTPCCurveID )
			&& ProxyCommandData::CurveData<AkRTPCGraphPoint>::Serialize( in_rSerializer );
	}

	bool SetRTPC::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return CommandData::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_RTPCID )
			&& in_rSerializer.GetEnum( m_RTPCType )
			&& in_rSerializer.GetEnum( m_RTPCAccum )
			&& in_rSerializer.GetEnum( m_paramID )
			&& in_rSerializer.Get( m_RTPCCurveID )
			&& ProxyCommandData::CurveData<AkRTPCGraphPoint>::Deserialize( in_rSerializer );
	}

	//
	// UnsetRTPC
	//

	UnsetRTPC::UnsetRTPC()
		: CommandData( ILayerProxy::MethodUnsetRTPC )
		, m_paramID( (AkRTPC_ParameterID)0 )
		, m_RTPCCurveID( 0 )
	{}

	bool UnsetRTPC::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.PutEnum( m_paramID )
			&& in_rSerializer.Put( m_RTPCCurveID );
	}

	bool UnsetRTPC::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.GetEnum( m_paramID )
			&& in_rSerializer.Get( m_RTPCCurveID );
	}

	//
	// SetChildAssoc
	//

	SetChildAssoc::SetChildAssoc()
		: CommandData( ILayerProxy::MethodSetChildAssoc )
		, m_ChildID( 0 )
		, m_pCrossfadingCurve( NULL )
		, m_ulCrossfadingCurveSize( 0 )
	{}

	SetChildAssoc::~SetChildAssoc()
	{
		if( m_bWasDeserialized && m_pCrossfadingCurve )
			CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pCrossfadingCurve );
	}

	bool SetChildAssoc::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_ChildID )
			&& in_rSerializer.SerializeArray( m_ulCrossfadingCurveSize, m_pCrossfadingCurve);

	}

	bool SetChildAssoc::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_ChildID )		
			&& in_rSerializer.DeserializeArray( m_ulCrossfadingCurveSize, m_pCrossfadingCurve);

		return bRet;
	}

	//
	// UnsetChildAssoc
	//

	UnsetChildAssoc::UnsetChildAssoc()
		: CommandData( ILayerProxy::MethodUnsetChildAssoc )
		, m_ChildID( 0 )
	{}

	bool UnsetChildAssoc::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_ChildID );
	}

	bool UnsetChildAssoc::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_ChildID );
	}

	//
	// SetCrossfadingRTPC
	//

	SetCrossfadingRTPC::SetCrossfadingRTPC()
		: CommandData( ILayerProxy::MethodSetCrossfadingRTPC )
		, m_rtpcID( 0 )
		, m_rtpcType( AkRtpcType_GameParameter )
	{}

	bool SetCrossfadingRTPC::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_rtpcID )
			&& in_rSerializer.PutEnum( m_rtpcType );
	}

	bool SetCrossfadingRTPC::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_rtpcID )
			&& in_rSerializer.GetEnum( m_rtpcType );
	}
}

namespace MusicNodeProxyCommandData
{
	MeterInfo::MeterInfo()
		: CommandData( IMusicNodeProxy::MethodMeterInfo )
	{}

	bool MeterInfo::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_bIsOverrideParent )
			&& in_rSerializer.Put( m_MeterInfo );
	}

	bool MeterInfo::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_bIsOverrideParent )
			&& in_rSerializer.Get( m_MeterInfo );
	}

	SetStingers::SetStingers()
		: CommandData( IMusicNodeProxy::MethodSetStingers )
		, m_pStingers( NULL )
	{}

	SetStingers::~SetStingers()
	{
		if( m_bWasDeserialized && m_pStingers )
		{
			CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pStingers );
		}
	}

	bool SetStingers::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.SerializeArray( m_NumStingers, m_pStingers);
	}

	bool SetStingers::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.DeserializeArray( m_NumStingers, m_pStingers);


		return bRet;
	}
}

namespace MusicTransAwareProxyCommandData
{
	SetRules::SetRules()
		: CommandData( IMusicTransAwareProxy::MethodSetRules )
	{}

	SetRules::~SetRules()
	{
		if( m_bWasDeserialized && m_pRules)
		{
			for (AkUInt32 i=0; i<m_NumRules; ++i)
			{
				if (m_pRules[i].srcIDs)
					CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pRules[i].srcIDs );
				if (m_pRules[i].destIDs)
					CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pRules[i].destIDs );
			}
			CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pRules );
		}
	}

	bool SetRules::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.SerializeArray( m_NumRules, m_pRules);
	}

	bool SetRules::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.DeserializeArray( m_NumRules, m_pRules);	

		return bRet;
	}
}

namespace MusicRanSeqProxyCommandData
{
	SetPlayList::SetPlayList()
		: CommandData( IMusicRanSeqProxy::MethodSetPlayList )
	{}

	SetPlayList::~SetPlayList()
	{
		if( m_bWasDeserialized && m_pArrayItems )
		{
			CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pArrayItems );
		}
	}

	bool SetPlayList::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.SerializeArray( m_NumItems, m_pArrayItems);
	}

	bool SetPlayList::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.DeserializeArray( m_NumItems, m_pArrayItems);	

		return bRet;
	}

}

namespace MusicSwitchProxyCommandData
{
	ContinuePlayback::ContinuePlayback()
		: CommandData( IMusicSwitchProxy::MethodContinuePlayback )
	{}

	bool ContinuePlayback::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_bContinuePlayback );
	}

	bool ContinuePlayback::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_bContinuePlayback );
	}

}

namespace MusicSegmentProxyCommandData
{
	Duration::Duration()
		: CommandData( ISegmentProxy::MethodDuration )
	{}

	bool Duration::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_fDuration );
	}

	bool Duration::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_fDuration );
	}

	StartPos::StartPos()
		: CommandData( ISegmentProxy::MethodStartPos )
	{}

	bool StartPos::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_fStartPos );
	}

	bool StartPos::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_fStartPos );
	}

	SetMarkers::SetMarkers()
		: CommandData( ISegmentProxy::MethodSetMarkers )
	{}

	SetMarkers::~SetMarkers()
	{
		if( m_bWasDeserialized && m_pArrayMarkers )
		{
			CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pArrayMarkers );
		}
	}

	bool SetMarkers::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.SerializeArray( m_ulNumMarkers, m_pArrayMarkers);	
	}

	bool SetMarkers::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		bool bRet = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.DeserializeArray( m_ulNumMarkers, m_pArrayMarkers);

		return bRet;
	}
}

namespace FxBaseProxyCommandData
{
	SetRTPC::SetRTPC()
		: CommandData( IFxBaseProxy::MethodSetRTPC )
	{}

	bool SetRTPC::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return CommandData::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_RTPCID )
			&& in_rSerializer.PutEnum( m_RTPCType )
			&& in_rSerializer.PutEnum( m_RTPCAccum )
			&& in_rSerializer.PutEnum( m_paramID )
			&& in_rSerializer.Put( m_RTPCCurveID )
			&& ProxyCommandData::CurveData<AkRTPCGraphPoint>::Serialize(in_rSerializer);
	}

	bool SetRTPC::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return CommandData::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_RTPCID )
			&& in_rSerializer.GetEnum( m_RTPCType )
			&& in_rSerializer.GetEnum( m_RTPCAccum )
			&& in_rSerializer.GetEnum( m_paramID )
			&& in_rSerializer.Get( m_RTPCCurveID )
			&& ProxyCommandData::CurveData<AkRTPCGraphPoint>::Deserialize(in_rSerializer);
	}

	SetStateProperties::SetStateProperties()
		: ObjectProxyCommandData::CommandData0<IFxBaseProxy::MethodSetStateProperties>() 
	{
		m_uProperties = 0;
		m_pProperties = NULL;
		m_bDeserialized = false;
	}

	SetStateProperties::SetStateProperties(IObjectProxy * in_pProxy)
		: ObjectProxyCommandData::CommandData0<IFxBaseProxy::MethodSetStateProperties>(in_pProxy)
	{
		m_pProperties = NULL;
		m_bDeserialized = false;
	}

	SetStateProperties::~SetStateProperties()
	{
		if (m_bDeserialized)
		{
			if (m_pProperties)
				AkFree(AkMemID_Profiler, m_pProperties);
		}
	}

	bool SetStateProperties::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_uProperties )
			&& in_rSerializer.SerializeArray(m_uProperties, m_pProperties);
	}

	bool SetStateProperties::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		m_bDeserialized = true;
		return  __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_uProperties )
			&& in_rSerializer.DeserializeArray(m_uProperties, m_pProperties);
	}

	UpdateStateGroups::UpdateStateGroups()
		: ObjectProxyCommandData::CommandData0<IFxBaseProxy::MethodUpdateStateGroups>() 
	{
		m_pStates = NULL;
		m_pGroups = NULL;
	}

	UpdateStateGroups::UpdateStateGroups(IObjectProxy * in_pProxy)
		: ObjectProxyCommandData::CommandData0<IFxBaseProxy::MethodUpdateStateGroups>(in_pProxy)
	{
		m_pStates = NULL;
		m_pGroups = NULL;
	}

	UpdateStateGroups::~UpdateStateGroups()
	{
		if (m_bWasDeserialized)
		{
			if (m_pStates)
				CommandDataSerializer::FreeArrayMem(AkMemID_Profiler, m_pStates);
			if (m_pGroups)
				CommandDataSerializer::FreeArrayMem(AkMemID_Profiler, m_pGroups);
		}
	}

	bool UpdateStateGroups::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		AkUInt32 count = 0;
		for (AkUInt32 i = 0; i < m_uGroups; i++)
			count += m_pGroups[i].ulStateCount;

		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_uGroups )
			&& in_rSerializer.SerializeArray(m_uGroups, m_pGroups)
			&& in_rSerializer.SerializeArray(count, m_pStates);
	}

	bool UpdateStateGroups::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		AkUInt32 uStateCount;
		return  __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_uGroups )
			&& in_rSerializer.DeserializeArray(m_uGroups, m_pGroups)
			&& in_rSerializer.DeserializeArray(uStateCount, m_pStates);
	}
}

namespace MultiSwitchProxyCommandData
{
	SetDecisionTree::SetDecisionTree()
		: CommandData( IMultiSwitchProxy::MethodSetDecisionTree )
		, m_pData( NULL )
		, m_uSize( 0 )
		, m_uDepth( 0 )
	{}

	SetDecisionTree::~SetDecisionTree()
	{
		if( m_bWasDeserialized && m_pData )
		{
			CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pData );
		}
	}

	bool SetDecisionTree::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		unsigned char* pData = (unsigned char*)m_pData;
		bool bRes = __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_uDepth )
			&& in_rSerializer.SerializeArray( m_uSize, pData );
		return bRes;
	}

	bool SetDecisionTree::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		unsigned char* pData = NULL;
		bool bRes = __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_uDepth )
			&& in_rSerializer.DeserializeArray( m_uSize, pData );
		m_pData = (void*)pData;
		return bRes;
	}


	SetArguments::SetArguments()
		: CommandData( IMultiSwitchProxy::MethodSetArguments )
		, m_pArgs( NULL )
		, m_pGroupTypes( NULL )
		, m_uNumArgs( 0 )
	{}

	SetArguments::~SetArguments()
	{
		if ( m_bWasDeserialized )
		{
			if ( m_pArgs )
				CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pArgs );
			if ( m_pGroupTypes )
				CommandDataSerializer::FreeArrayMem( AkMemID_Profiler, m_pGroupTypes );
		}
	}

	bool SetArguments::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.SerializeArray( m_uNumArgs, m_pArgs )
			&& in_rSerializer.SerializeArray( m_uNumArgs, m_pGroupTypes );
	}

	bool SetArguments::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.DeserializeArray( m_uNumArgs, m_pArgs )
			&& in_rSerializer.DeserializeArray( m_uNumArgs, m_pGroupTypes );
	}
}

namespace MidiDeviceMgrProxyCommandData
{
	CommandData::CommandData()
		: ProxyCommandData::CommandData()
	{}

	CommandData::CommandData( AkUInt16 in_methodID )
		: ProxyCommandData::CommandData( ProxyCommandData::TypeMidiDeviceMgrProxy, in_methodID )
	{}

	bool CommandData::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer );
	}

	bool CommandData::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer );
	}


	AddTarget::AddTarget()
		: CommandData( IMidiDeviceMgrProxy::MethodAddTarget )
	{}

	bool AddTarget::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_idTarget )
			&& in_rSerializer.Put( m_idGameObj );
	}

	bool AddTarget::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_idTarget )
			&& in_rSerializer.Get( m_idGameObj );
	}	


	RemoveTarget::RemoveTarget()
		: CommandData( IMidiDeviceMgrProxy::MethodRemoveTarget )
	{}

	bool RemoveTarget::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_idTarget )
			&& in_rSerializer.Put( m_idGameObj );
	}

	bool RemoveTarget::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_idTarget )
			&& in_rSerializer.Get( m_idGameObj );
	}	


	MidiEvent::MidiEvent()
		: CommandData( IMidiDeviceMgrProxy::MethodMidiEvent )
	{}

	bool MidiEvent::Serialize( CommandDataSerializer& in_rSerializer ) const
	{
		return __base::Serialize( in_rSerializer )
			&& in_rSerializer.Put( m_idTarget )
			&& in_rSerializer.Put( m_idGameObj )
			&& in_rSerializer.Put( m_uTimestampMs )
			&& in_rSerializer.Put( m_uMidiEvent );
	}

	bool MidiEvent::Deserialize( CommandDataSerializer& in_rSerializer )
	{
		return __base::Deserialize( in_rSerializer )
			&& in_rSerializer.Get( m_idTarget )
			&& in_rSerializer.Get( m_idGameObj )
			&& in_rSerializer.Get( m_uTimestampMs )
			&& in_rSerializer.Get( m_uMidiEvent );
	}	
}

#endif // #ifndef AK_OPTIMIZED
