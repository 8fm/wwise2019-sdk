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

#pragma	once

#ifndef AK_SERIALIZER_WRITEBYTESMEM
	#ifdef PROXYCENTRAL_CONNECTED
		#include "ALBytesMem.h"
		#define AK_SERIALIZER_WRITEBYTESMEM AK::ALWriteBytesMem
	#else
		#include <AKBase/BytesMem.h>
		#define AK_SERIALIZER_WRITEBYTESMEM ::WriteBytesMem
	#endif
#endif

#include "../../Communication/Remote/CodeBase/ICommunicationCentral.h"
#include "AkMidiStructs.h"
#include "AkMonitorData.h"
#include "AkMusicStructs.h"
extern AK::Comm::ICommunicationCentral * g_pCommCentral;

struct AkPathVertex;
struct AkPathListItemOffset;
struct AkStateUpdate;
struct AkStateGroupUpdate;
struct AkStatePropertyUpdate;
struct AkEffectUpdate;
struct AkWwiseGraphCurve;
struct AkWwiseRTPCreg;

class CommandDataSerializer
{
public:
#ifdef PROXYCENTRAL_CONNECTED
	CommandDataSerializer();
#else
	CommandDataSerializer( bool	in_bSwapEndian = false );
#endif
	~CommandDataSerializer();

	inline AkUInt8* GetWrittenBytes() const { return m_writer.Bytes(); }
	inline int GetWrittenSize() const { return m_writer.Count(); }

#ifdef PROXYCENTRAL_CONNECTED
	inline bool GetSwapEndian() const { return false; }
#else
	inline bool GetSwapEndian() const { return m_bSwapEndian; }
#endif

	void Deserializing(	const AkUInt8*	in_pData );
	inline const AkUInt8 * GetReadBytes() const { return m_pReadBytes + m_readPos; }

	void Reset();

	class AutoSetDataPeeking
	{
	public:
		inline AutoSetDataPeeking( CommandDataSerializer& in_rSerializer )
			: m_rSerializer( in_rSerializer	)
		{
			m_rSerializer.SetDataPeeking( true );
		}

		inline ~AutoSetDataPeeking()
		{
			m_rSerializer.SetDataPeeking( false	);
		}

	private:
		CommandDataSerializer& m_rSerializer;
	};

	// External, catch all
	template <class	T>
	inline bool Put( const T& in_rValue );

	template <class	T>
	inline bool Get( T& out_rValue );

	// Basic, known	size types.
	bool Put( AkInt8 in_value );
	bool Get( AkInt8& out_rValue );
	bool Put( AkUInt8 in_value );
	bool Get( AkUInt8& out_rValue );
	bool Put( AkInt16 in_value );
	bool Get( AkInt16& out_rValue );
	bool Put( AkInt32 in_value );
	bool Get( AkInt32& out_rValue );
	bool Put( AkUInt16 in_value	);
	bool Get( AkUInt16&	out_rValue );
	bool Put( AkUInt32 in_value	);
	bool Get( AkUInt32&	out_rValue );
	bool Put( AkInt64 in_value );
	bool Get( AkInt64& out_rValue );

	bool Put( AkUInt64 in_value	);
	bool Get( AkUInt64&	out_rValue );

//	bool Put( AkUInt64	in_value );
//	bool Get( AkUInt64& out_rValue	);
	bool Put( AkReal32 in_value );
	bool Get( AkReal32& out_rValue );
	bool Put( AkReal64 in_value );
	bool Get( AkReal64& out_rValue );

	inline bool PutFloat24(const AkReal32 val)
	{
#if defined(AK_ENDIANNESS_LITTLE)
		AkInt32 lDummy = 0;
		AkUInt8 *p = ((AkUInt8*)&val);
		m_writer.WriteBytes(p+1, 3, lDummy);	
#else
#error Big Endian not supported yet		
#endif
		return true;
	}

	bool GetFloat24(AkReal32 &out_value)
	{
#if defined(AK_ENDIANNESS_LITTLE)
		AkUInt8* p = (AkUInt8*)(&out_value);
		p[0] = 0;	//The low byte was dropped. Just assume 0.
		p[1] = m_pReadBytes[m_readPos];
		p[2] = m_pReadBytes[m_readPos + 1];
		p[3] = m_pReadBytes[m_readPos + 2];
		m_readPos += 3;
#else
#error Big Endian not supported yet
#endif
		return true;
	}

	// Basic, possibly unknown size	types.
	bool Put( bool in_value	);
	bool Get( bool&	out_rValue );

	bool Put( const WwiseObjectIDext& in_rValue );
	bool Get( WwiseObjectIDext& out_rValue );
	
	// Typedefs, enums,	etc. types

	template <class	T>
	bool PutEnum( const	T& in_rValue );

	template <class	T>
	bool GetEnum( T& out_rValue	);

	bool Put( const	AKRESULT& in_rValue	);
	bool Get( AKRESULT&	out_rValue );

	bool Put( const	AkChannelConfig& in_rValue	);
	bool Get( AkChannelConfig&	out_rValue );

	// Composite types
	bool Put( const	AkChannelEmitter& in_rValue );
	bool Get( AkChannelEmitter& out_rValue );
	bool Put( const	AkTransform & in_rValue );
	bool Get( AkTransform& out_rValue );
	bool Put( const	AkPanningRule& in_rValue );
	bool Get( AkPanningRule& out_rValue );
	bool Put( const AkVector& in_rValue );
	bool Get( AkVector& out_rValue );
	bool Put( const AkMeterInfo& in_rValue );
	bool Get( AkMeterInfo& out_rValue );
	bool Put(const AkObstructionOcclusionValues& in_rValue);
	bool Get(AkObstructionOcclusionValues& out_rValue);

	template< class VALUE_TYPE >
	bool Put( const AkGraphPointBase<VALUE_TYPE>& in_rValue );
	template< class VALUE_TYPE >
	bool Get( AkGraphPointBase<VALUE_TYPE>& out_rValue );

	bool Put( const AkPathVertex& in_rValue );
	bool Get( AkPathVertex& out_rValue );
	bool Put( const AkPathListItemOffset& in_rValue );
	bool Get( AkPathListItemOffset& out_rValue );
	bool Put(const AkMonitorData::AuxSendValue& in_rValue);
	bool Get( AkMonitorData::AuxSendValue& out_rValue );

	bool Put( const AkMusicMarkerWwise& in_rValue );
	bool Get( AkMusicMarkerWwise& out_rValue );
	bool Put( const AkTrackSrcInfo& in_rValue );
	bool Get( AkTrackSrcInfo& out_rValue );
	bool Put( const AkWwiseMusicTransitionRule& in_rValue );
	bool Get( AkWwiseMusicTransitionRule& out_rValue );
	bool Put( const AkWwiseMusicTrackTransRule& in_rValue );
	bool Get( AkWwiseMusicTrackTransRule& out_rValue );
	bool Put( const AkMusicFade& in_rValue );
	bool Get( AkMusicFade& out_rValue );
	bool Put( const AkMusicRanSeqPlaylistItem& in_rValue );
	bool Get( AkMusicRanSeqPlaylistItem& out_rValue );
	bool Put( const CAkStinger& in_rValue );
	bool Get( CAkStinger& out_rValue );
	bool Put( const AkMusicTrackType& in_rValue );
	bool Get( AkMusicTrackType& out_rValue );

	bool Put( const AkMusicSwitchAssoc& in_rValue );
	bool Get( AkMusicSwitchAssoc& out_rValue );

	bool Put( const	AkStateUpdate& in_rValue );
	bool Get( AkStateUpdate& out_rValue );

	bool Put( const	AkStateGroupUpdate& in_rValue );
	bool Get( AkStateGroupUpdate& out_rValue );

	bool Put( const AkStatePropertyUpdate& in_rValue );
	bool Get( AkStatePropertyUpdate& out_rValue );

	bool Put( const	AkEffectUpdate& in_rValue );
	bool Get( AkEffectUpdate& out_rValue );

	bool Put( const AkMIDIEvent& in_rValue );
	bool Get( AkMIDIEvent& out_rValue );

	bool Put( const	AkMonitorData::MonitorDataItem&	in_rValue );
	bool Get( AkMonitorData::MonitorDataItem*& out_rpValue ); // client	of method must deallocate returned pointer with	free()

	bool Put( const	AkMonitorData::MeterWatch& in_rValue );
	bool Get( AkMonitorData::MeterWatch& out_rValue );

	bool Put( const	AkMidiEventEx& in_rValue );
	bool Get( AkMidiEventEx& out_rValue );

	// Variable	length types
	bool Put( const	void* in_pvData, AkUInt32 in_size );
	bool Get( void*& out_rpData, AkUInt32& out_rSize );
	bool Put( const	char* in_pszData );
	bool Get( char*& out_rpszData, AkUInt32&	out_rSize );
	bool Put( const	AkUtf16* in_pszData	);
	bool Get( AkUtf16*&	out_rpszData, AkUInt32& out_rSize );

    AK_SERIALIZER_WRITEBYTESMEM* GetWriter(){return &m_writer;}

	template<class ARRAYITEM>
	bool SerializeArray(AkUInt32 in_rNum, const ARRAYITEM* in_pItems);

	template<class ARRAYITEM>
	bool DeserializeArray(AkUInt32 &out_rNum, ARRAYITEM* &in_pItems);

	static void* AllocArrayMem(AkMemPoolId in_poolId, size_t in_uSize);
	static void  FreeArrayMem(AkMemPoolId in_poolId, void* in_pMemAddress);

private:
	// Composite types not serializable	from outside this class.
	bool Put( const	AkMonitorData::TimeStampData& in_rValue	);
	bool Get( AkMonitorData::TimeStampData&	out_rValue );
	bool Put( const	AkMonitorData::ObjectMonitorData& in_rValue	);
	bool Get( AkMonitorData::ObjectMonitorData&	out_rValue );
	bool Put( const	AkMonitorData::StateMonitorData& in_rValue );
	bool Get( AkMonitorData::StateMonitorData& out_rValue );
	bool Put( const	AkMonitorData::ParamChangedMonitorData&	in_rValue );
	bool Get( AkMonitorData::ParamChangedMonitorData& out_rValue );
	bool Put( const	AkMonitorData::SetParamMonitorData&	in_rValue );
	bool Get( AkMonitorData::SetParamMonitorData& out_rValue );
	bool Put( const	AkMonitorData::ActionTriggeredMonitorData& in_rValue );
	bool Get( AkMonitorData::ActionTriggeredMonitorData& out_rValue	);
	bool Put( const	AkMonitorData::ActionDelayedMonitorData& in_rValue );
	bool Get( AkMonitorData::ActionDelayedMonitorData& out_rValue );
	bool Put( const	AkMonitorData::EventTriggeredMonitorData& in_rValue	);
	bool Get( AkMonitorData::EventTriggeredMonitorData&	out_rValue );
	bool Put( const	AkMonitorData::BankMonitorData&	in_rValue );
	bool Get( AkMonitorData::BankMonitorData& out_rValue );
	bool Put( const	AkMonitorData::PrepareMonitorData&	in_rValue );
	bool Get( AkMonitorData::PrepareMonitorData& out_rValue );
	bool Put( const	AkMonitorData::BusNotifMonitorData&	in_rValue );
	bool Get( AkMonitorData::BusNotifMonitorData& out_rValue );
	bool Put( const	AkMonitorData::AudioPerfMonitorData& in_rValue );
	bool Get( AkMonitorData::AudioPerfMonitorData& out_rValue );
	bool Put( const	AkMonitorData::GameObjPositionMonitorData& in_rValue );
	bool Get( AkMonitorData::GameObjPositionMonitorData& out_rValue );
	bool Put( const	AkMonitorData::ObjRegistrationMonitorData& in_rValue );
	bool Get( AkMonitorData::ObjRegistrationMonitorData& out_rValue	);
	bool Put( const	AkMonitorData::ErrorMonitorData1& in_rValue );
	bool Get( AkMonitorData::ErrorMonitorData1& out_rValue );
	bool Put( const	AkMonitorData::DebugMonitorData& in_rValue );
	bool Get( AkMonitorData::DebugMonitorData& out_rValue );
	bool Put( const	AkMonitorData::PathMonitorData&	in_rValue );
	bool Get( AkMonitorData::PathMonitorData& out_rValue );
	bool Put( const	AkCustomParamType& in_rValue );
	bool Get( AkCustomParamType& out_rValue	);
	bool Put( const	AkMonitorData::SwitchMonitorData& in_rValue	);
	bool Get( AkMonitorData::SwitchMonitorData&	out_rValue );
	bool Put( const	AkMonitorData::CPUTimerMonitorData& in_rValue );
	bool Get( AkMonitorData::CPUTimerMonitorData& out_rValue	);
	bool Put( const	AkMonitorData::MemoryMonitorData& in_rValue	);
	bool Get( AkMonitorData::MemoryMonitorData&	out_rValue );
	bool Put( const	AkMonitorData::MemoryPoolNameMonitorData& in_rValue	);
	bool Get( AkMonitorData::MemoryPoolNameMonitorData&	out_rValue );
	bool Put( const	AkMonitorData::EnvironmentMonitorData& in_rValue );
	bool Get( AkMonitorData::EnvironmentMonitorData& out_rValue	);
	bool Put( const	AkMonitorData::SendsMonitorData& in_rValue );
	bool Get( AkMonitorData::SendsMonitorData& out_rValue	);
	bool Put( const AkMonitorData::ObsOccMonitorData& in_rValue );
	bool Get( AkMonitorData::ObsOccMonitorData& out_rValue );
	bool Put( const	AkMonitorData::ListenerMonitorData&	in_rValue );
	bool Get( AkMonitorData::ListenerMonitorData& out_rValue );
	bool Put( const	AkMonitorData::ControllerMonitorData&	in_rValue );
	bool Get( AkMonitorData::ControllerMonitorData& out_rValue );
	bool Put( const	AkMonitorData::DeviceRecordMonitorData&	in_rValue );
	bool Get( AkMonitorData::DeviceRecordMonitorData& out_rValue );
	bool Put( const	AkMonitorData::StreamRecordMonitorData&	in_rValue );
	bool Get( AkMonitorData::StreamRecordMonitorData& out_rValue );
	bool Put( const	AkMonitorData::StreamingMonitorData& in_rValue );
	bool Get( AkMonitorData::StreamingMonitorData& out_rValue );
	bool Put( const	AkMonitorData::StreamDeviceMonitorData& in_rValue );
	bool Get( AkMonitorData::StreamDeviceMonitorData& out_rValue );
	bool Put( const	AkMonitorData::MarkersMonitorData& in_rValue );
	bool Get( AkMonitorData::MarkersMonitorData& out_rValue );	
	bool Put( const	AkMonitorData::SegmentPositionMonitorData& in_rValue );
	bool Get( AkMonitorData::SegmentPositionMonitorData& out_rValue );
	bool Put( const	AkMonitorData::SoundPositionMonitorData& in_rValue );
	bool Get( AkMonitorData::SoundPositionMonitorData& out_rValue );

	bool Put( const	AkMonitorData::LoadedSoundBankMonitorData& in_rValue );
	bool Get( AkMonitorData::LoadedSoundBankMonitorData& out_rValue );
	bool Put( const	AkMonitorData::MediaPreparedMonitorData& in_rValue );
	bool Get( AkMonitorData::MediaPreparedMonitorData& out_rValue );
	bool Put( const	AkMonitorData::EventPreparedMonitorData& in_rValue );
	bool Get( AkMonitorData::EventPreparedMonitorData& out_rValue );
	bool Put( const	AkMonitorData::GameSyncMonitorData& in_rValue );
	bool Get( AkMonitorData::GameSyncMonitorData& out_rValue );
	bool Put( const	AkMonitorData::CommonDialogueMonitorData& in_rValue );
	bool Get( AkMonitorData::CommonDialogueMonitorData& out_rValue );

	bool Put( const	AkMonitorData::MusicTransitionMonitorData& in_rValue );
	bool Get( AkMonitorData::MusicTransitionMonitorData& out_rValue );
	bool Put( const	AkMonitorData::MidiTargetMonitorData& in_rValue );
	bool Get( AkMonitorData::MidiTargetMonitorData& out_rValue );
	bool Put( const	AkMonitorData::MidiEventMonitorData& in_rValue );
	bool Get( AkMonitorData::MidiEventMonitorData& out_rValue );
	bool Put( const	AkMonitorData::PluginMonitorData& in_rValue );
	bool Get( AkMonitorData::PluginMonitorData& out_rValue );
	bool Put( const	AkMonitorData::PluginMonitorError& in_rValue );
	bool Get( AkMonitorData::PluginMonitorError& out_rValue );
	bool Put( const AkMonitorData::ExternalSourceMonitorData &in_rValue );
	bool Get( AkMonitorData::ExternalSourceMonitorData &out_rValue, AkUInt16 in_uiStringSize);

	bool Put( const	AkMonitorData::PlatformSinkTypeData& in_rValue );
	bool Get( AkMonitorData::PlatformSinkTypeData& out_rValue );

	bool Put( const	AkMonitorData::InitData& in_rValue );
	bool Get( AkMonitorData::InitData& out_rValue );

	bool Put( const AkWwiseGraphCurve& in_rValue );
	bool Get( AkWwiseGraphCurve& out_rValue );
	bool Put( const AkWwiseRTPCreg& in_rValue );
	bool Get( AkWwiseRTPCreg& out_rValue );

	friend class AutoSetDataPeeking;
	void SetDataPeeking( bool in_bPeekData );

	AkInt16	Swap( const	AkInt16& in_rValue ) const;
	AkUInt16 Swap( const AkUInt16& in_rValue ) const;
	AkInt32	Swap( const	AkInt32& in_rValue ) const;
	AkUInt32 Swap( const AkUInt32& in_rValue ) const;
	AkInt64	Swap( const	AkInt64& in_rValue ) const;
	AkUInt64 Swap( const AkUInt64& in_rValue ) const;
	AkReal32 Swap( const AkReal32& in_rValue ) const;
	AkReal64 Swap( const AkReal64& in_rValue ) const;

	AK_SERIALIZER_WRITEBYTESMEM m_writer;

	const AkUInt8*	m_pReadBytes;
	AkUInt32 m_readPos;
	AkUInt32 m_readPosBeforePeeking;

#ifndef PROXYCENTRAL_CONNECTED
	const bool m_bSwapEndian;
#endif
};

template <class	T>
bool CommandDataSerializer::Put( const T& in_rValue	)
{
	return in_rValue.Serialize(	*this );
}

template <class	T>
bool CommandDataSerializer::Get( T&	out_rValue )
{
	return out_rValue.Deserialize( *this );
}

template <class	T>
bool CommandDataSerializer::PutEnum( const T& in_rValue	)
{
	return Put(	(AkUInt32) in_rValue );
}

template <class	T>
bool CommandDataSerializer::GetEnum( T&	out_rValue )
{
	AkUInt32 uValue; // for platforms where sizeof( enum ) < 32 bits
	bool bResult = Get(	uValue );
	out_rValue = (T) uValue; 
	return bResult;
}

template< class VALUE_TYPE >
bool CommandDataSerializer::Put( const AkGraphPointBase<VALUE_TYPE>& in_rValue )
{
	return Put( in_rValue.From )
		&& Put( in_rValue.To )
		&& PutEnum( in_rValue.Interp );
}

template< class VALUE_TYPE >
bool CommandDataSerializer::Get( AkGraphPointBase<VALUE_TYPE>& out_rValue )
{
	return Get( out_rValue.From )
		&& Get( out_rValue.To )
		&& GetEnum( out_rValue.Interp );
}

template<class ARRAYITEM>
bool CommandDataSerializer::SerializeArray(AkUInt32 in_rNum, const ARRAYITEM* in_pItems)
{
	bool bRet = Put(in_rNum);
	for(AkUInt32 i = 0; i < in_rNum && bRet; ++i)
		bRet = Put(in_pItems[i]);

	return bRet;
}

template<class ARRAYITEM>
bool CommandDataSerializer::DeserializeArray(AkUInt32 &out_rNum, ARRAYITEM* &in_pItems)
{
	out_rNum = 0;
	in_pItems = NULL;

	bool bRet = Get(out_rNum);
	if (!bRet || out_rNum == 0)
	{
		out_rNum = 0;
		return bRet;
	}

	in_pItems = (ARRAYITEM*)AllocArrayMem(AkMemID_Profiler, out_rNum * sizeof(ARRAYITEM));
	if (in_pItems == NULL)
	{
		out_rNum = 0;
		return false;
	}

	AkUInt32 i = 0;
	for(i = 0; i < out_rNum && bRet; ++i)
		bRet = Get(in_pItems[i]);

	//Not all items were deserialized, so not all items are valid.
	if (!bRet)
		out_rNum = i-1;

	return bRet;
}
