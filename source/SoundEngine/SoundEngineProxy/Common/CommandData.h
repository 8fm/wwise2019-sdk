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

#include "CommandDataSerializer.h"
#include "AkPrivateTypes.h"
#include "AkAction.h"

namespace AkMonitorData
{
	struct Watch;
}

namespace ProxyCommandData
{
	enum CommandType
	{
		TypeRendererProxy = 1,
		TypeALMonitorProxy,
		TypeStateMgrProxy,
		TypeObjectProxyStore,
		TypeObjectProxy,
		TypeMidiDeviceMgrProxy
	};

	struct CommandData
	{
		inline CommandData() {}
		inline CommandData( CommandType in_eCommandType, AkUInt16 in_methodID )
			: m_commandType( (AkUInt16)in_eCommandType )
			, m_methodID( in_methodID )
			, m_bWasDeserialized( false )
		{
		}
		
		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt16 m_commandType;
		AkUInt16 m_methodID;
		bool m_bWasDeserialized;
	};

	template <class POINT>
	struct CurveData
	{
		CurveData()
			: m_bWasCurveDeserialized(false)
			, m_eScaling( AkCurveScaling_None )
			, m_pArrayConversion( NULL )
			, m_ulConversionArraySize( 0 )
		{};

		~CurveData()
		{
			if (m_bWasCurveDeserialized && m_pArrayConversion)
				CommandDataSerializer::FreeArrayMem(AkMemID_Profiler, m_pArrayConversion);
		}

		bool Deserialize( CommandDataSerializer &in_rSerializer )
		{
			m_bWasCurveDeserialized = true;
			return in_rSerializer.GetEnum( m_eScaling ) &&
				in_rSerializer.DeserializeArray( m_ulConversionArraySize, m_pArrayConversion);		
		}

		bool Serialize( CommandDataSerializer &in_rSerializer ) const
		{
			return in_rSerializer.PutEnum( m_eScaling ) && 
				in_rSerializer.SerializeArray( m_ulConversionArraySize, m_pArrayConversion);
		}

		bool m_bWasCurveDeserialized;
		AkCurveScaling m_eScaling;
		POINT* m_pArrayConversion;
		AkUInt32 m_ulConversionArraySize;
	};

	typedef AkArrayAllocatorNoAlign<AkMemID_Profiler> ArrayPool;
}

namespace ObjectProxyStoreCommandData
{
	enum MethodIDs
	{
		MethodCreate = 1,
		MethodRelease
	};

	enum ObjectType
	{
		TypeSound = 1,
		TypeEvent,
		TypeDialogueEvent,
		TypeAction,
		TypeCustomState,
		TypeRanSeqContainer,
		TypeSwitchContainer,
		TypeActorMixer,
		TypeBus,
		TypeAuxBus,
		TypeLayerContainer,
		TypeLayer,
		TypeMusicTrack,
		TypeMusicSegment,
		TypeMusicRanSeq,
		TypeMusicSwitch,
		TypeAttenuation,
		TypeFxShareSet,
		TypeFxCustom,
		TypeModulatorLfo,
		TypeModulatorEnvelope,
		TypeModulatorTime,
		TypeAudioDevice,
		TypeAcousticTexture
	};

	struct CommandData : public ProxyCommandData::CommandData
	{
		CommandData();
		CommandData( AkUInt16 in_methodID );

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUInt64 m_proxyInstancePtr;
		AkUniqueID m_objectID;

		DECLARE_BASECLASS( ProxyCommandData::CommandData );
	};

	struct Create : public CommandData
	{
		Create();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		ObjectType m_eObjectType;
		AkActionType m_actionType;	// Optional

		DECLARE_BASECLASS( CommandData );
	};

	struct Release : public CommandData
	{
		Release();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		DECLARE_BASECLASS( CommandData );
	};
}

#endif // #ifndef AK_OPTIMIZED
