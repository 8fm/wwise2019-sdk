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

#include "CommandDataSerializer.h"
#include "AkAttenuationMgr.h"
#include "AkEndianByteSwap.h"
#include "AkParameterNodeBase.h"
#include "AkPath.h"
#include "AkStateAware.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>

extern AK::Comm::ICommunicationCentral * g_pCommCentral;

#ifdef PROXYCENTRAL_CONNECTED
CommandDataSerializer::CommandDataSerializer()
#else
CommandDataSerializer::CommandDataSerializer( bool in_bSwapEndian )
#endif
	: m_pReadBytes( NULL )
	, m_readPos( 0 )
	, m_readPosBeforePeeking( 0 )
#ifndef PROXYCENTRAL_CONNECTED
	, m_bSwapEndian( in_bSwapEndian )
#endif
{
}

CommandDataSerializer::~CommandDataSerializer()
{
	m_writer.Clear();
}

void CommandDataSerializer::Deserializing( const AkUInt8* in_pData )
{
	m_pReadBytes = in_pData;
	m_readPos = 0;
	m_readPosBeforePeeking = 0;
}

void CommandDataSerializer::Reset()
{
	m_writer.Clear();
	Deserializing( NULL );
}

void* CommandDataSerializer::AllocArrayMem( AkMemPoolId in_poolId, size_t in_uSize )
{
#ifdef AK_ANDROID
	return AkMalign( in_poolId, in_uSize, AK_64B_OS_STRUCT_ALIGN );
#else
	return AkAlloc( in_poolId, in_uSize );
#endif
}

void CommandDataSerializer::FreeArrayMem( AkMemPoolId in_poolId, void* in_pMemAddress )
{
#ifdef AK_ANDROID
	AkFalign( in_poolId, in_pMemAddress );
#else
	AkFree( in_poolId, in_pMemAddress );
#endif
}

bool CommandDataSerializer::Put( AkInt8 in_value )
{
	return m_writer.Write( in_value );
}

bool CommandDataSerializer::Get( AkInt8& out_rValue )
{
	out_rValue = *(AkUInt8*)(m_pReadBytes + m_readPos);
	m_readPos += 1;

	return true;
}

bool CommandDataSerializer::Put( AkUInt8 in_value )
{
	return m_writer.Write( in_value );
}

bool CommandDataSerializer::Get( AkUInt8& out_rValue )
{
	out_rValue = *(AkUInt8*)(m_pReadBytes + m_readPos);
	m_readPos += 1;

	return true;
}

bool CommandDataSerializer::Put( AkInt16 in_value )
{
	return m_writer.Write( Swap( in_value ) );
}

bool CommandDataSerializer::Get( AkInt16& out_rValue )
{
	out_rValue = Swap( *(AkInt16*)(m_pReadBytes + m_readPos) );
	m_readPos += 2;

	return true;
}

bool CommandDataSerializer::Put( AkInt32 in_value )
{
	return m_writer.Write( Swap( in_value ) );
}

bool CommandDataSerializer::Get( AkInt32& out_rValue )
{
	out_rValue = Swap(AK::ReadUnaligned<AkInt32>(m_pReadBytes + m_readPos));
	m_readPos += 4;

	return true;
}

bool CommandDataSerializer::Put( AkUInt16 in_value )
{
	return m_writer.Write( Swap( in_value ) );
}

bool CommandDataSerializer::Get( AkUInt16& out_rValue )
{
	out_rValue = Swap(AK::ReadUnaligned<AkUInt16>(m_pReadBytes + m_readPos));
	m_readPos += 2;

	return true;
}

bool CommandDataSerializer::Put( AkUInt32 in_value )
{
	return m_writer.Write( Swap( in_value ) );
}

bool CommandDataSerializer::Get( AkUInt32& out_rValue )
{
	out_rValue = Swap(AK::ReadUnaligned<AkUInt32>(m_pReadBytes + m_readPos));
	m_readPos += 4;

	return true;
}

bool CommandDataSerializer::Put( AkInt64 in_value )
{
	return m_writer.Write( Swap( in_value ) );
}

bool CommandDataSerializer::Get( AkInt64& out_rValue )
{
	out_rValue = Swap(AK::ReadUnaligned<AkInt64>(m_pReadBytes + m_readPos));
	m_readPos += 8;

	return true;
}

bool CommandDataSerializer::Put( AkUInt64 in_value )
{
    return m_writer.Write( Swap( in_value ) );
}

bool CommandDataSerializer::Get( AkUInt64& out_rValue )
{
	out_rValue = Swap(AK::ReadUnaligned<AkUInt64>(m_pReadBytes + m_readPos));
	m_readPos += 8;

	return true;
}

bool CommandDataSerializer::Put( AkReal32 in_value )
{
	AkInt32 lDummy = 0;

	AkReal32 valToWrite = Swap( in_value );
	
	return m_writer.WriteBytes( &valToWrite, 4, lDummy );
}

bool CommandDataSerializer::Get( AkReal32& out_rValue )
{
	out_rValue = Swap(AK::ReadUnaligned<AkReal32>(m_pReadBytes + m_readPos));
	m_readPos += 4;

	return true;
}

bool CommandDataSerializer::Put( AkReal64 in_value )
{
	AkInt32 lDummy = 0;

	AkReal64 valToWrite = Swap( in_value );
	
	return m_writer.WriteBytes( &valToWrite, 8, lDummy );
}

bool CommandDataSerializer::Get( AkReal64& out_rValue )
{
	return Get( *reinterpret_cast<AkInt64*>(&out_rValue) );
}

bool CommandDataSerializer::Put( bool in_value )
{
	return Put( (AkUInt8)in_value );
}

bool CommandDataSerializer::Get( bool& out_rValue )
{
	return Get( (AkUInt8&)out_rValue );
}

bool CommandDataSerializer::Put( const AKRESULT& in_rValue )
{
	return Put( (AkUInt32)in_rValue );
}

bool CommandDataSerializer::Get( AKRESULT& out_rValue )
{
	return Get( (AkUInt32&)out_rValue );
}

bool CommandDataSerializer::Put( const AkChannelConfig& in_rValue )
{
	return Put(in_rValue.Serialize());
}

bool CommandDataSerializer::Get( AkChannelConfig& out_rValue )
{
	AkUInt32 value;
	if ( Get(value) )
	{
		out_rValue.Deserialize(value);
		return true;
	}
	return false;
}

bool CommandDataSerializer::Put( const AkPanningRule& in_rValue )
{
	return Put( (AkUInt32)in_rValue );
}

bool CommandDataSerializer::Get( AkPanningRule& out_rValue )
{
	return Get( (AkUInt32&)out_rValue );
}

bool CommandDataSerializer::Put(const AkTransform& in_rValue)
{
	return Put( in_rValue.Position() )
		&& Put( in_rValue.OrientationFront() )
		&& Put( in_rValue.OrientationTop() );
}

bool CommandDataSerializer::Get(AkTransform& out_rValue)
{
	AkVector position, orientationFront, orientationTop;
	if (Get(position)
		&& Get(orientationFront)
		&& Get(orientationTop))
	{
		out_rValue.Set(position, orientationFront, orientationTop);
		return true;
	}
	return false;
}

bool CommandDataSerializer::Put(const AkChannelEmitter& in_rValue)
{
	return Put(in_rValue.position)
		&& Put(in_rValue.uInputChannels);
}

bool CommandDataSerializer::Get(AkChannelEmitter& out_rValue)
{
	return Get(out_rValue.position)
		&& Get(out_rValue.uInputChannels);
}

bool CommandDataSerializer::Put( const AkVector& in_rValue )
{
	return Put( in_rValue.X )
		&& Put( in_rValue.Y )
		&& Put( in_rValue.Z );
}

bool CommandDataSerializer::Get( AkVector& out_rValue )
{
	return Get( out_rValue.X )
		&& Get( out_rValue.Y )
		&& Get( out_rValue.Z );
}

bool CommandDataSerializer::Put( const AkMeterInfo& in_rValue )
{
	return Put( in_rValue.fGridOffset )
		&& Put( in_rValue.fGridPeriod )
		&& Put( in_rValue.fTempo )
		&& Put( in_rValue.uTimeSigBeatValue )
		&& Put( in_rValue.uTimeSigNumBeatsBar );
}
bool CommandDataSerializer::Get( AkMeterInfo& out_rValue )
{
	return Get( out_rValue.fGridOffset )
		&& Get( out_rValue.fGridPeriod )
		&& Get( out_rValue.fTempo )
		&& Get( out_rValue.uTimeSigBeatValue )
		&& Get( out_rValue.uTimeSigNumBeatsBar );
}

bool CommandDataSerializer::Put(const AkObstructionOcclusionValues& in_rValue)
{
	return Put(in_rValue.obstruction)
		&& Put(in_rValue.occlusion);
}

bool CommandDataSerializer::Get(AkObstructionOcclusionValues& out_rValue)
{
	return Get(out_rValue.obstruction)
		&& Get(out_rValue.occlusion);
}

bool CommandDataSerializer::Put( const AkPathVertex& in_rValue )
{
	return Put( in_rValue.Vertex )
		&& Put( (AkUInt32)in_rValue.Duration );
}

bool CommandDataSerializer::Get( AkPathVertex& out_rValue )
{
	return Get( out_rValue.Vertex )
		&& Get( (AkUInt32&)out_rValue.Duration );
}

bool CommandDataSerializer::Put( const AkPathListItemOffset& in_rValue )
{
	return Put( in_rValue.ulVerticesOffset )
		&& Put( in_rValue.iNumVertices );
}

bool CommandDataSerializer::Get( AkPathListItemOffset& out_rValue )
{
	return Get( (AkUInt32&)out_rValue.ulVerticesOffset )
		&& Get( (AkInt32&)out_rValue.iNumVertices );
}

bool CommandDataSerializer::Put( const AkMonitorData::AuxSendValue& in_rValue )
{
	return Put( in_rValue.listenerID )
		&& Put( in_rValue.auxBusID )
		&& Put( in_rValue.fControlValue)
		&& Put( in_rValue.associationType);
}

bool CommandDataSerializer::Get(AkMonitorData::AuxSendValue& out_rValue)
{
	return Get( out_rValue.listenerID )
		&& Get( out_rValue.auxBusID )
		&& Get( out_rValue.fControlValue)
		&& Get( out_rValue.associationType);
}

bool CommandDataSerializer::Put( const AkMusicMarkerWwise& in_rValue )
{
	return Put( in_rValue.id )
		&& Put( in_rValue.fPosition )
		&& Put( (const char*)in_rValue.pszName );
}

bool CommandDataSerializer::Get( AkMusicMarkerWwise& out_rValue )
{
	AkUInt32 uStringSize;	// not used
	return Get( out_rValue.id )
		&& Get( out_rValue.fPosition )
		&& Get( out_rValue.pszName, uStringSize );
}

bool CommandDataSerializer::Put( const AkTrackSrcInfo& in_rValue )
{
	return Put( in_rValue.fBeginTrimOffset )
		&& Put( in_rValue.fEndTrimOffset)
		&& Put( in_rValue.fPlayAt)
		&& Put( in_rValue.fSrcDuration)
		&& Put( in_rValue.eventID)
		&& Put( in_rValue.sourceID)
		&& Put( in_rValue.trackID);
}

bool CommandDataSerializer::Get( AkTrackSrcInfo& out_rValue )
{
	return Get( out_rValue.fBeginTrimOffset )
		&& Get( out_rValue.fEndTrimOffset)
		&& Get( out_rValue.fPlayAt)
		&& Get( out_rValue.fSrcDuration)
		&& Get( out_rValue.eventID)
		&& Get( out_rValue.sourceID)
		&& Get( out_rValue.trackID);
}

bool CommandDataSerializer::Put( const AkWwiseMusicTransitionRule& in_rValue )
{
	return SerializeArray( in_rValue.uNumSrc, in_rValue.srcIDs )
		&& SerializeArray( in_rValue.uNumDst, in_rValue.destIDs )
		&& Put( in_rValue.srcFade )
		&& Put( in_rValue.eSrcSyncType )
		&& Put( in_rValue.uSrcCueFilterHash )
		&& Put( in_rValue.bSrcPlayPostExit )
		&& Put( in_rValue.destFade )
		&& Put( in_rValue.uDestCueFilterHash )
		&& Put( in_rValue.uDestJumpToID )
		&& Put( in_rValue.eDestJumpToType )
		&& Put( in_rValue.eDestEntryType )
		&& Put( in_rValue.bDestPlayPreEntry )
		&& Put( in_rValue.bDestMatchSourceCueName )
		&& Put( in_rValue.bIsTransObjectEnabled )
		&& Put( in_rValue.segmentID )
		&& Put( in_rValue.transFadeIn )
		&& Put( in_rValue.transFadeOut )
		&& Put( in_rValue.bPlayPreEntry )
		&& Put( in_rValue.bPlayPostExit );
}

bool CommandDataSerializer::Get( AkWwiseMusicTransitionRule& out_rValue )
{
	return DeserializeArray( out_rValue.uNumSrc, out_rValue.srcIDs )
		&& DeserializeArray( out_rValue.uNumDst, out_rValue.destIDs )
		&& Get( out_rValue.srcFade )
		&& Get( out_rValue.eSrcSyncType )
		&& Get( out_rValue.uSrcCueFilterHash )
		&& Get( out_rValue.bSrcPlayPostExit )
		&& Get( out_rValue.destFade )
		&& Get( out_rValue.uDestCueFilterHash )
		&& Get( out_rValue.uDestJumpToID )
		&& Get( out_rValue.eDestJumpToType )
		&& Get( out_rValue.eDestEntryType )
		&& Get( out_rValue.bDestPlayPreEntry )
		&& Get( out_rValue.bDestMatchSourceCueName )
		&& Get( out_rValue.bIsTransObjectEnabled )
		&& Get( out_rValue.segmentID )
		&& Get( out_rValue.transFadeIn )
		&& Get( out_rValue.transFadeOut )
		&& Get( out_rValue.bPlayPreEntry )
		&& Get( out_rValue.bPlayPostExit );
}

bool CommandDataSerializer::Put( const AkWwiseMusicTrackTransRule& in_rValue )
{
	return Put( in_rValue.srcFade )
		&& Put( in_rValue.eSrcSyncType )
		&& Put( in_rValue.uSrcCueFilterHash )
		&& Put( in_rValue.destFade );
}

bool CommandDataSerializer::Get( AkWwiseMusicTrackTransRule& out_rValue )
{
	return Get( out_rValue.srcFade )
		&& Get( out_rValue.eSrcSyncType )
		&& Get( out_rValue.uSrcCueFilterHash )
		&& Get( out_rValue.destFade );
}

bool CommandDataSerializer::Put( const AkMusicFade& in_rValue )
{
	return Put( in_rValue.iFadeOffset )
		&& Put( in_rValue.transitionTime )
		&& PutEnum( in_rValue.eFadeCurve );
}

bool CommandDataSerializer::Get( AkMusicFade& out_rValue )
{
	return Get( out_rValue.iFadeOffset )
		&& Get( out_rValue.transitionTime )
		&& GetEnum( out_rValue.eFadeCurve );
}

bool CommandDataSerializer::Put( const AkMusicRanSeqPlaylistItem& in_rValue )
{
	return Put( in_rValue.m_SegmentID )
		&& Put( in_rValue.m_playlistItemID )
		&& Put( in_rValue.m_NumChildren )
		&& Put( in_rValue.m_wAvoidRepeatCount )
		&& Put( in_rValue.m_Weight )
		&& Put( in_rValue.m_bIsShuffle )
		&& Put( in_rValue.m_bIsUsingWeight )
		&& PutEnum( in_rValue.m_eRSType )
		&& Put( in_rValue.m_Loop )
		&& Put( in_rValue.m_LoopMin )
		&& Put( in_rValue.m_LoopMax );
}

bool CommandDataSerializer::Get( AkMusicRanSeqPlaylistItem& out_rValue )
{
	return Get( out_rValue.m_SegmentID )
		&& Get( out_rValue.m_playlistItemID )
		&& Get( out_rValue.m_NumChildren )
		&& Get( out_rValue.m_wAvoidRepeatCount )
		&& Get( out_rValue.m_Weight )
		&& Get( out_rValue.m_bIsShuffle )
		&& Get( out_rValue.m_bIsUsingWeight )
		&& GetEnum( out_rValue.m_eRSType )
		&& Get( out_rValue.m_Loop )
		&& Get( out_rValue.m_LoopMin )
		&& Get( out_rValue.m_LoopMax );
}

bool CommandDataSerializer::Put( const CAkStinger& in_rValue )
{
	return Put( in_rValue.m_TriggerID )
		&& Put( in_rValue.m_SegmentID )
		&& PutEnum( in_rValue.m_SyncPlayAt )
		&& Put( in_rValue.m_uCueFilterHash )
		&& Put( in_rValue.m_DontRepeatTime )
		&& Put( in_rValue.m_numSegmentLookAhead );
}

bool CommandDataSerializer::Get( CAkStinger& out_rValue )
{
	return Get( out_rValue.m_TriggerID )
		&& Get( out_rValue.m_SegmentID )
		&& GetEnum( out_rValue.m_SyncPlayAt )
		&& Get( out_rValue.m_uCueFilterHash )
		&& Get( out_rValue.m_DontRepeatTime )
		&& Get( out_rValue.m_numSegmentLookAhead );
}

bool CommandDataSerializer::Put( const AkMusicTrackType& in_rValue )
{
	return PutEnum( in_rValue );
}

bool CommandDataSerializer::Get( AkMusicTrackType& out_rValue )
{
	return GetEnum( out_rValue );
}

bool CommandDataSerializer::Put( const AkMusicSwitchAssoc& in_rValue )
{
	return Put( in_rValue.nodeID )
		&& Put( in_rValue.switchID );
}

bool CommandDataSerializer::Get( AkMusicSwitchAssoc& out_rValue )
{
	return Get( out_rValue.nodeID )
		&& Get( out_rValue.switchID );
}

bool CommandDataSerializer::Put( const AkMonitorData::MonitorDataItem& in_rValue )
{	
	bool bRet = Put( in_rValue.eDataType );
	if ( !bRet )
		return false;
	
	switch( in_rValue.eDataType )
	{
	case AkMonitorData::MonitorDataTimeStamp:
		bRet = Put( in_rValue.timeStampData );
		break;

	case AkMonitorData::MonitorDataObject:
		bRet = Put( in_rValue.objectData );
		break;

	case AkMonitorData::MonitorDataState:
		bRet = Put( in_rValue.stateData );
		break;

	case AkMonitorData::MonitorDataParamChanged:
		bRet = Put( in_rValue.paramChangedData );
		break;

	case AkMonitorData::MonitorDataBank:
		bRet =	Put( (AkInt32)in_rValue.bankData.wStringSize ) && // Variable array size
				Put( in_rValue.bankData );
		break;

	case AkMonitorData::MonitorDataPrepare:
		bRet =	Put( in_rValue.prepareData );
		break;

	case AkMonitorData::MonitorDataEventTriggered:
		bRet =	Put( in_rValue.eventTriggeredData );
		break;

	case AkMonitorData::MonitorDataActionDelayed:
		bRet =	Put( in_rValue.actionDelayedData );
		break;

	case AkMonitorData::MonitorDataActionTriggered:
		bRet =	Put( in_rValue.actionTriggeredData );
		break;

	case AkMonitorData::MonitorDataBusNotif:
		bRet =	Put( in_rValue.busNotifData );
		break;

	case AkMonitorData::MonitorDataSetParam:
		bRet =	Put( in_rValue.setParamData );
		break;

	case AkMonitorData::MonitorDataAudioPerf:
		bRet =	Put( in_rValue.audioPerfData );
		break;

	case AkMonitorData::MonitorDataGameObjPosition:
		bRet =	Put( (AkInt32)in_rValue.gameObjPositionData.ulNumGameObjPositions ) &&
				Put( in_rValue.gameObjPositionData );
		break;

	case AkMonitorData::MonitorDataObjRegistration:
		bRet =	Put( (AkInt32)in_rValue.objRegistrationData.wStringSize ) && // Variable array size
				Put( in_rValue.objRegistrationData );
		break;

	case AkMonitorData::MonitorDataPath:
		bRet =	Put( in_rValue.pathData );
		break;

	case AkMonitorData::MonitorDataErrorString:
	case AkMonitorData::MonitorDataMessageString:
		bRet =	Put( (AkInt32)in_rValue.debugData.wStringSize ) &&	// Variable array size
				Put( in_rValue.debugData );
		break;

	case AkMonitorData::MonitorDataErrorCode:
	case AkMonitorData::MonitorDataMessageCode:
		bRet =	Put( in_rValue.errorData1 );
		break;

	case AkMonitorData::MonitorDataSwitch:
		bRet =	Put( in_rValue.switchData );
		break;

	case AkMonitorData::MonitorDataCPUTimer:
		bRet =	Put( (AkInt32)in_rValue.cpuTimerData.uNumItems ) &&	// Variable array size
				Put( in_rValue.cpuTimerData );
		break;

	case AkMonitorData::MonitorDataMemoryPool:
		bRet =	Put( (AkInt32)in_rValue.memoryData.uNumPools ) &&	// Variable array size
				Put( in_rValue.memoryData );
		break;

	case AkMonitorData::MonitorDataMemoryPoolName:
		bRet =	Put( (AkInt32)in_rValue.memoryPoolNameData.wStringSize ) &&	// Variable array size
				Put( in_rValue.memoryPoolNameData );
		break;

	case AkMonitorData::MonitorDataEnvironment:
		bRet =	Put( (AkInt32)in_rValue.environmentData.ulNumEnvPacket ) &&	// Variable array size
				Put( (AkInt32)in_rValue.environmentData.ulTotalAuxSends ) &&	// Variable array size
				Put( in_rValue.environmentData );
		break;

	case AkMonitorData::MonitorDataSends:
		bRet =	Put( (AkInt32)in_rValue.sendsData.ulNumSends ) &&	// Variable array size
				Put( in_rValue.sendsData );
		break;

	case AkMonitorData::MonitorDataObsOcc:
		bRet =	Put( (AkInt32)in_rValue.obsOccData.ulNumPacket ) &&	// Variable array size
				Put( in_rValue.obsOccData );
		break;

	case AkMonitorData::MonitorDataListeners:
		bRet =	Put( in_rValue.listenerData );
		break;

    case AkMonitorData::MonitorDataDevicesRecord:
		bRet =	Put( in_rValue.deviceRecordData );
		break;

    case AkMonitorData::MonitorDataStreamsRecord:
		bRet =	Put( (AkInt32)in_rValue.streamRecordData.ulNumNewRecords ) && // Variable array size
				Put( in_rValue.streamRecordData );
		break;

    case AkMonitorData::MonitorDataStreaming:
		bRet =	Put( (AkInt32)in_rValue.streamingData.ulNumStreams ) &&	// Variable array size
				Put( in_rValue.streamingData );
		break;

	case AkMonitorData::MonitorDataMarkers:
		bRet =	Put( (AkInt32)in_rValue.markersData.wStringSize ) && // Variable array size
				Put( in_rValue.markersData );
		break;

	case AkMonitorData::MonitorDataSegmentPosition:
		bRet =	Put( (AkInt32)in_rValue.segmentPositionData.numPositions ) && // Variable array size
				Put( in_rValue.segmentPositionData );
		break;

	case AkMonitorData::MonitorDataSoundPosition:
		bRet = Put( (AkInt32)in_rValue.soundPositionData.numPositions ) && // Variable array size
			   Put( in_rValue.soundPositionData );
		break;

	case AkMonitorData::MonitorDataSoundBank:
		bRet =	Put( in_rValue.loadedSoundBankData );
		break;

	case AkMonitorData::MonitorDataMedia:
		bRet =	Put( (AkInt32)in_rValue.mediaPreparedData.uArraySize ) && // Variable array size
				Put( in_rValue.mediaPreparedData );
		break;

	case AkMonitorData::MonitorDataEvent:
		bRet =	Put( in_rValue.eventPreparedData );
		break;

	case AkMonitorData::MonitorDataGameSync:
		bRet =	Put( in_rValue.gameSyncData );
		break;

	case AkMonitorData::MonitorDataResolveDialogue:	
		bRet =	Put( (AkInt32)in_rValue.commonDialogueData.uPathSize ) &&	// Variable array size
				Put( in_rValue.commonDialogueData );
		break;

	case AkMonitorData::MonitorDataMusicTransition:
		bRet =	Put( in_rValue.musicTransData );
		break;

	case AkMonitorData::MonitorDataMidiTarget:
		bRet =	Put( in_rValue.midiTargetData );
		break;

	case AkMonitorData::MonitorDataMidiEvent:
		bRet =	Put( in_rValue.midiEventData );
		break;

	case AkMonitorData::MonitorDataPlugin:
		bRet =	Put( (AkInt32)in_rValue.pluginMonitorData.uDataSize ) && // Variable array size
				Put( in_rValue.pluginMonitorData );
		break;

	case AkMonitorData::MonitorDataPluginError:
	case AkMonitorData::MonitorDataPluginMessage:
		bRet =	Put( (AkInt32)in_rValue.pluginMonitorError.wStringSize ) && // Variable array size
				Put( in_rValue.pluginMonitorError );
		break;

	case AkMonitorData::MonitorDataExternalSource:
		bRet =	Put((AkInt32) in_rValue.templateSrcData.wStringSize) &&	// Variable array size
				Put(in_rValue.templateSrcData);
		break;

	case AkMonitorData::MonitorDataStreamingDevice:
		bRet =	Put( (AkInt32)in_rValue.streamingDeviceData.ulNumDevices ) &&	// Variable array size
				Put( in_rValue.streamingDeviceData );
		break;

	case AkMonitorData::MonitorDataPlatformSinkType:
		bRet =	Put((AkInt32)in_rValue.platformSinkTypeData.uBufSize) &&	// Variable array size
				Put(in_rValue.platformSinkTypeData);
		break;

	case AkMonitorData::MonitorDataSpatialAudioRoomsAndPortals:
	case AkMonitorData::MonitorDataSpatialAudioGeometry:
	case AkMonitorData::MonitorDataSpatialAudioObjects:
	case AkMonitorData::MonitorDataPipeline:
	case AkMonitorData::MonitorDataMeter:
	case AkMonitorData::MonitorDataApiFunctions:
	case AkMonitorData::MonitorDataParamDelta:
		bRet =	Put( in_rValue.varLenData.abyData, in_rValue.varLenData.dataSize ); // Variable size byte array
		break;

	case AkMonitorData::MonitorDataEndFrame:
		bRet = true;
		break;

	case AkMonitorData::MonitorDataInit:
		bRet = Put( in_rValue.initData );
		break;

	default:
		AKASSERT( !"Unknown MonitorDataItem to serialize." );
		bRet = false;
	}

	return bRet;
}

bool CommandDataSerializer::Get( AkMonitorData::MonitorDataItem*& out_rpValue )
{
	AkUInt8 eDataType;
	bool bRet = Get( eDataType );
	if ( !bRet )
		return false;

	AkUInt32 allocSize = MONITORDATAITEMBASESIZE;
	AkUInt32 arraySize = 0;

	/*
		Below, the allocSize grow and might shrink to adapt the variable array size.
		It might be weird but it is what we want. If the array size is zero, we won't allocate
		the array, even if the structure has a array of size 1. The array of size 1, is in 
		fact to mimic a pointer behavior.
		
		Ex:
		In the struct AkMonitorData::GameObjPositionMonitorData we have:
		Position positions[1];
	 
		allocSize += sizeof( AkMonitorData::GameObjPositionMonitorData );
		allocSize += sizeof( AkMonitorData::GameObjPositionMonitorData::Position ) * (arraySize - 1);
	 */
	
	switch( eDataType )
	{
	case AkMonitorData::MonitorDataTimeStamp:
		allocSize += sizeof( AkMonitorData::TimeStampData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );	
		bRet = Get( out_rpValue->timeStampData );
		break;

	case AkMonitorData::MonitorDataObject:
		allocSize += sizeof( AkMonitorData::ObjectMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );	
		bRet = Get( out_rpValue->objectData );
		break;

	case AkMonitorData::MonitorDataState:
		allocSize += sizeof( AkMonitorData::StateMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );	
		bRet = Get( out_rpValue->stateData );
		break;

	case AkMonitorData::MonitorDataParamChanged:
		allocSize += sizeof( AkMonitorData::ParamChangedMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->paramChangedData );
		break;

	case AkMonitorData::MonitorDataBank:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::BankMonitorData );
		allocSize += sizeof( char ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->bankData );
		break;

	case AkMonitorData::MonitorDataPrepare:
		allocSize += sizeof( AkMonitorData::PrepareMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->prepareData );
		break;

	case AkMonitorData::MonitorDataEventTriggered:
		allocSize += sizeof( AkMonitorData::EventTriggeredMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->eventTriggeredData );
		break;

	case AkMonitorData::MonitorDataActionDelayed:
		allocSize += sizeof( AkMonitorData::ActionDelayedMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->actionDelayedData );
		break;

	case AkMonitorData::MonitorDataActionTriggered:
		allocSize += sizeof( AkMonitorData::ActionTriggeredMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->actionTriggeredData );
		break;

	case AkMonitorData::MonitorDataBusNotif:
		allocSize += sizeof( AkMonitorData::BusNotifMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->busNotifData );
		break;

	case AkMonitorData::MonitorDataSetParam:
		allocSize += sizeof( AkMonitorData::SetParamMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->setParamData );
		break;

	case AkMonitorData::MonitorDataAudioPerf:
		allocSize += sizeof( AkMonitorData::AudioPerfMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->audioPerfData );
		break;

	case AkMonitorData::MonitorDataGameObjPosition:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::GameObjPositionMonitorData );
		allocSize += sizeof( AkMonitorData::GameObjPosition ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->gameObjPositionData );
		break;
		
	case AkMonitorData::MonitorDataObjRegistration:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::ObjRegistrationMonitorData );
		allocSize += sizeof( char ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->objRegistrationData );
		break;

	case AkMonitorData::MonitorDataPath:
		allocSize += sizeof( AkMonitorData::PathMonitorData);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->pathData );
		break;

	case AkMonitorData::MonitorDataErrorString:
	case AkMonitorData::MonitorDataMessageString:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::DebugMonitorData );
		allocSize += sizeof( AkUtf16 ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->debugData );
		break;

	case AkMonitorData::MonitorDataErrorCode:
	case AkMonitorData::MonitorDataMessageCode:
		allocSize += sizeof( AkMonitorData::ErrorMonitorData1 );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->errorData1 );
		break;

	case AkMonitorData::MonitorDataSwitch:
		allocSize += sizeof( AkMonitorData::SwitchMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->switchData );
		break;

	case AkMonitorData::MonitorDataCPUTimer:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::CPUTimerMonitorData );
		allocSize += sizeof( AkAudiolibTimer::Item ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->cpuTimerData );
		break;

	case AkMonitorData::MonitorDataMemoryPool:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::MemoryMonitorData );
		allocSize += sizeof( AkMonitorData::MemoryPoolData ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->memoryData );
		break;

	case AkMonitorData::MonitorDataMemoryPoolName:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::MemoryPoolNameMonitorData );
		allocSize += sizeof( AkUtf16 ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->memoryPoolNameData );
		break;

	case AkMonitorData::MonitorDataEnvironment:
	{
		AkUInt32 uTotalAuxSends = 0;
		bRet = Get(arraySize) && Get(uTotalAuxSends);
		allocSize += sizeof(AkMonitorData::EnvironmentMonitorData);
		allocSize += sizeof(AkMonitorData::EnvPacket) * (arraySize - 1);
		allocSize += sizeof(AkMonitorData::AuxSendValue) * uTotalAuxSends;
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc(allocSize);
		bRet = bRet && Get(out_rpValue->environmentData);
	}
		break;

	case AkMonitorData::MonitorDataSends:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::SendsMonitorData );
		allocSize += sizeof( AkMonitorData::SendsData ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->sendsData );
		break;

	case AkMonitorData::MonitorDataObsOcc:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::ObsOccMonitorData );
		allocSize += sizeof( AkMonitorData::ObsOccPacket ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->obsOccData );
		break;

	case AkMonitorData::MonitorDataListeners:
		{
			bRet = Get( arraySize );
			//uSizeTotal is read by the caller.

			AkUInt16 ulNumGameObjMask = 0;
			AkUInt16 ulNumListenerIDsForGameObjs = 0;
			AkUInt16 uVolumes = 0;
			AkUInt16 uNumListeners = 0;
			bRet = bRet & Get(ulNumGameObjMask);
			bRet = bRet & Get(ulNumListenerIDsForGameObjs);
			bRet = bRet & Get(uVolumes);
			bRet = bRet & Get(uNumListeners);

			allocSize += sizeof(AkMonitorData::ListenerMonitorData);
			allocSize += sizeof(AkMonitorData::ListenerPacket) * uNumListeners;
			allocSize += sizeof(AkMonitorData::GameObjectListenerMaskPacket) * ulNumGameObjMask;
			allocSize += sizeof(AkWwiseGameObjectID) * ulNumListenerIDsForGameObjs;
			allocSize += sizeof(AkReal32) * uVolumes;

			out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
			out_rpValue->listenerData.uSizeTotal = arraySize;
			out_rpValue->listenerData.ulNumGameObjMask = ulNumGameObjMask;
			out_rpValue->listenerData.ulNumListenerIDsForGameObjs = ulNumListenerIDsForGameObjs;
			out_rpValue->listenerData.uVolumes = uVolumes;
			out_rpValue->listenerData.uNumListeners = uNumListeners;

			bRet = bRet && Get( out_rpValue->listenerData );
		}
		break;

    case AkMonitorData::MonitorDataDevicesRecord:
		allocSize += sizeof( AkDeviceDesc );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->deviceRecordData );
		break;

     case AkMonitorData::MonitorDataStreamsRecord:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::StreamRecordMonitorData );
		allocSize += sizeof( AkMonitorData::StreamRecord ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->streamRecordData );
		break;

    case AkMonitorData::MonitorDataStreaming:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::StreamingMonitorData );
		allocSize += sizeof( AkMonitorData::StreamData ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->streamingData );
		break;

    case AkMonitorData::MonitorDataStreamingDevice:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::StreamDeviceMonitorData );
		allocSize += sizeof( AkMonitorData::DeviceData ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->streamingDeviceData );
		break;

	case AkMonitorData::MonitorDataMarkers:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::MarkersMonitorData );
		allocSize += sizeof( char ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->markersData );
		break;

	case AkMonitorData::MonitorDataSegmentPosition:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::SegmentPositionMonitorData );
		allocSize += sizeof( AkMonitorData::SegmentPositionData ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->segmentPositionData );
		break;

	case AkMonitorData::MonitorDataSoundPosition:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::SoundPositionMonitorData );
		// We permit 0 for this monitor data
		if (arraySize)
		{
			allocSize += sizeof(AkMonitorData::SoundPositionData) * (arraySize - 1);
		}
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->soundPositionData );
		break;

	case AkMonitorData::MonitorDataSoundBank:
		allocSize += sizeof( AkMonitorData::LoadedSoundBankMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->loadedSoundBankData );
		break;

	case AkMonitorData::MonitorDataMedia:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::MediaPreparedMonitorData );
		allocSize += sizeof( AkMonitorData::MediaPreparedMonitorData::BankMedia ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->mediaPreparedData );
		break;

	case AkMonitorData::MonitorDataEvent:
		allocSize += sizeof( AkMonitorData::EventPreparedMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->eventPreparedData );
		break;

	case AkMonitorData::MonitorDataGameSync:
		allocSize += sizeof( AkMonitorData::GameSyncMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->gameSyncData );
		break;

	case AkMonitorData::MonitorDataResolveDialogue:	
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::CommonDialogueMonitorData );
		allocSize += sizeof( AkArgumentValueID ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->commonDialogueData );
		break;

	case AkMonitorData::MonitorDataMusicTransition:
		allocSize += sizeof( AkMonitorData::MusicTransitionMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->musicTransData );
		break;

	case AkMonitorData::MonitorDataMidiTarget:
		allocSize += sizeof( AkMonitorData::MidiTargetMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->midiTargetData );
		break;

	case AkMonitorData::MonitorDataMidiEvent:
		allocSize += sizeof( AkMonitorData::MidiEventMonitorData );
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = Get( out_rpValue->midiEventData );
		break;

	case AkMonitorData::MonitorDataPlugin:
		bRet = Get( arraySize );
		allocSize += ( sizeof( AkMonitorData::PluginMonitorData ) );
		allocSize += (arraySize - 1);	// * sizeof(BYTE)
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->pluginMonitorData );
		break;

	case AkMonitorData::MonitorDataPluginError:
	case AkMonitorData::MonitorDataPluginMessage:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::PluginMonitorError );
		allocSize += sizeof( AkUtf16 ) * (arraySize - 1);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->pluginMonitorError );
		break;

	case AkMonitorData::MonitorDataExternalSource:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::ExternalSourceMonitorData );
		allocSize += (arraySize - 1) * sizeof(AkUtf16);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->templateSrcData, (AkUInt16)arraySize );			
		break;

	case AkMonitorData::MonitorDataPlatformSinkType:
		bRet = Get( arraySize );
		allocSize += sizeof( AkMonitorData::PlatformSinkTypeData );
		allocSize += sizeof( char ) * (arraySize);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );
		bRet = bRet && Get( out_rpValue->platformSinkTypeData );
		break;

	case AkMonitorData::MonitorDataSpatialAudioRoomsAndPortals:
	case AkMonitorData::MonitorDataSpatialAudioGeometry:
	case AkMonitorData::MonitorDataSpatialAudioObjects:
	case AkMonitorData::MonitorDataPipeline:
	case AkMonitorData::MonitorDataMeter:
	case AkMonitorData::MonitorDataApiFunctions:
	case AkMonitorData::MonitorDataParamDelta:
		{
			AkUInt32 dataSize = 0;

			bRet = Get( dataSize );

			allocSize += SIZEOF_MONITORDATA_TO( varLenData.abyData );
			allocSize += dataSize;
			out_rpValue = (AkMonitorData::MonitorDataItem*) malloc( allocSize );

			out_rpValue->varLenData.dataSize = dataSize;
			memcpy( out_rpValue->varLenData.abyData, GetReadBytes(), dataSize );
		}
		break;
	
	case AkMonitorData::MonitorDataEndFrame:
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc(allocSize + sizeof(AkMonitorData::EndOfFrameData));
		bRet = true;
		break;

	case AkMonitorData::MonitorDataInit:
		allocSize += sizeof(AkMonitorData::InitData);
		out_rpValue = (AkMonitorData::MonitorDataItem*) malloc(allocSize);
		bRet = Get( out_rpValue->initData );
		break;

	default:
		AKASSERT( !"Unknown MonitorDataItem to deserialize." );
		return false;
	}

	out_rpValue->eDataType = eDataType;
	
	return bRet;
}

bool CommandDataSerializer::Put( const	AkMonitorData::MeterWatch& in_rValue )
{
	return Put( in_rValue.busID ) &&
		Put( in_rValue.uBusMeterDataTypes );
}

bool CommandDataSerializer::Get( AkMonitorData::MeterWatch& out_rValue )
{
	bool bRet = Get( out_rValue.busID ) &&
		Get( out_rValue.uBusMeterDataTypes );

	return bRet;
}

bool CommandDataSerializer::Put( const AkMidiEventEx& in_rValue )
{
	return	Put(in_rValue.byType) && 
			Put(in_rValue.byChan) && 
			Put(in_rValue.Gen.byParam1) && 
			Put(in_rValue.Gen.byParam2);
}

bool CommandDataSerializer::Get( AkMidiEventEx& out_rValue )
{
	return	Get(out_rValue.byType) && 
			Get(out_rValue.byChan) && 
			Get(out_rValue.Gen.byParam1) && 
			Get(out_rValue.Gen.byParam2);
}

bool CommandDataSerializer::Put( const void* in_pvData, AkUInt32 in_size )
{
	AkInt32 lDummy = 0;
	
	return Put( in_size )
		&& m_writer.WriteBytes( in_pvData, in_size, lDummy );
}

bool CommandDataSerializer::Get( void*& out_rpData, AkUInt32& out_rSize )
{
	out_rpData = NULL;
	out_rSize = 0;
	
	if( Get( out_rSize ) && out_rSize != 0 )
	{
		out_rpData = (void*)(m_pReadBytes + m_readPos);

		m_readPos += out_rSize;
	}

	return true;
}

bool CommandDataSerializer::Put( const char* in_pszData )
{
	return Put( (void*)in_pszData, in_pszData != NULL ? (AkInt32)(::strlen( in_pszData ) + 1)  : 0 );
}

bool CommandDataSerializer::Put( const AkUtf16* in_pszData )
{
    if ( in_pszData )
	{
		AkInt32 lDummy = 0;

		size_t uNumChars = AKPLATFORM::AkUtf16StrLen( in_pszData ) + 1;
		size_t uStringSize = sizeof(AkUtf16) * uNumChars;

		if ( !GetSwapEndian() )
			return m_writer.WriteBytes( (void*)in_pszData, (AkInt32)(uStringSize), lDummy );
		else
		{
			AkUtf16 * pSwappedStr = (AkUtf16 *)AkAlloca( uStringSize );
			if ( !pSwappedStr ) 
				return false;
			
			for ( unsigned int i=0; i<uNumChars; i++ )
				pSwappedStr[i] = Swap( (AkUInt16&)in_pszData[i] );

			return m_writer.WriteBytes( (void*)pSwappedStr, (AkInt32)(uStringSize), lDummy );
		}
	}
	return Put( (AkUInt16)0 );
}

bool CommandDataSerializer::Get( char*& out_rpszData, AkUInt32& out_rSize )
{
	return Get( (void*&)out_rpszData, out_rSize );
}

bool CommandDataSerializer::Get( AkUtf16*& out_rpszData, AkUInt32& out_rSize )
{
	out_rpszData = (AkUtf16 *)(m_pReadBytes + m_readPos);
	if ( out_rpszData )
	{
		out_rSize = (AkInt32)AKPLATFORM::AkUtf16StrLen( out_rpszData ) + 1;
		
		m_readPos += ( out_rSize * sizeof(AkUtf16) );

		if ( GetSwapEndian() )
		{
			for ( AkUInt32 i=0; i<out_rSize; i++ )
				out_rpszData[i] = Swap( (AkUInt16&)out_rpszData[i] );
		}
	}
	else
		out_rSize = 0;

	return true;
}

bool CommandDataSerializer::Put( const AkMonitorData::TimeStampData& in_rValue )
{
	bool bRet = Put( in_rValue.timeStamp );

	return bRet;
}

bool CommandDataSerializer::Get( AkMonitorData::TimeStampData& out_rValue )
{
	bool bRet = Get( out_rValue.timeStamp );

	return bRet;
}

bool CommandDataSerializer::Put( const AkMonitorData::ObjectMonitorData& in_rValue )
{
	bool bRet = Put( in_rValue.playingID )
		&& Put( in_rValue.gameObjPtr )
		&& Put( (AkInt32)in_rValue.eNotificationReason );
	
	// Container history
	{
		bRet = bRet
			&& Put( in_rValue.cntrHistArray.uiArraySize );

		for( AkUInt32 i = 0; i < in_rValue.cntrHistArray.uiArraySize && bRet; ++i )
			Put( in_rValue.cntrHistArray.aCntrHist[i] );
	}

	bRet = bRet && Put( in_rValue.customParam );

	bRet = bRet
		&& Put( in_rValue.targetObjectID )
		&& Put( in_rValue.timeValue )
		&& Put( in_rValue.playlistItemID );

	return bRet;
}

bool CommandDataSerializer::Get( AkMonitorData::ObjectMonitorData& out_rValue )
{
	bool bRet = Get( out_rValue.playingID )
		&& Get( out_rValue.gameObjPtr )
		&& Get( (AkInt32&)out_rValue.eNotificationReason );
	
	// Container history
	{
		bRet = bRet
			&& Get( out_rValue.cntrHistArray.uiArraySize );

		for( AkUInt32 i = 0; i < out_rValue.cntrHistArray.uiArraySize && bRet; ++i )
			Get( out_rValue.cntrHistArray.aCntrHist[i] );
	}

	bRet = bRet && Get( out_rValue.customParam );

	bRet = bRet
		&& Get( out_rValue.targetObjectID )
		&& Get( out_rValue.timeValue )
		&& Get( out_rValue.playlistItemID );

	return bRet;
}

bool CommandDataSerializer::Put( const AkMonitorData::StateMonitorData& in_rValue )
{
	return Put( in_rValue.stateGroupID )
		&& Put( in_rValue.stateFrom )
		&& Put( in_rValue.stateTo );
}

bool CommandDataSerializer::Get( AkMonitorData::StateMonitorData& out_rValue )
{
	return Get( out_rValue.stateGroupID )
		&& Get( out_rValue.stateFrom )
		&& Get( out_rValue.stateTo );
}

bool CommandDataSerializer::Put( const AkMonitorData::ParamChangedMonitorData& in_rValue )
{
	return Put( (AkUInt32)in_rValue.eNotificationReason )
		&& Put( in_rValue.gameObjPtr )
		&& Put( in_rValue.elementID );
}

bool CommandDataSerializer::Get( AkMonitorData::ParamChangedMonitorData& out_rValue )
{
	return Get( (AkUInt32&)out_rValue.eNotificationReason )
		&& Get( out_rValue.gameObjPtr )
		&& Get( out_rValue.elementID );
}

bool CommandDataSerializer::Put( const AkMonitorData::SetParamMonitorData& in_rValue )
{
	bool bRet = Put( (AkUInt32)in_rValue.eNotificationReason )
		&& Put( in_rValue.gameObjPtr )
		&& Put( in_rValue.elementID );

	if( in_rValue.eNotificationReason == AkMonitorData::NotificationReason_VolumeChanged
		|| in_rValue.eNotificationReason == AkMonitorData::NotificationReason_PitchChanged )
		bRet = bRet && Put( in_rValue.fTarget );

	bRet = bRet && PutEnum( in_rValue.valueMeaning )
		&& Put( in_rValue.transitionTime );

	return bRet;
}

bool CommandDataSerializer::Get( AkMonitorData::SetParamMonitorData& out_rValue )
{
	bool bRet = Get( (AkUInt32&)out_rValue.eNotificationReason )
		&& Get( out_rValue.gameObjPtr )
		&& Get( out_rValue.elementID );

	if( out_rValue.eNotificationReason == AkMonitorData::NotificationReason_VolumeChanged 
		|| out_rValue.eNotificationReason == AkMonitorData::NotificationReason_PitchChanged )
		bRet = bRet && Get( out_rValue.fTarget );

	bRet = bRet 
		&& GetEnum( out_rValue.valueMeaning )
		&& Get( out_rValue.transitionTime );

	return bRet;
}

bool CommandDataSerializer::Put( const AkMonitorData::ActionTriggeredMonitorData& in_rValue )
{
	return Put( in_rValue.playingID )
		&& Put( in_rValue.actionID )
		&& Put( in_rValue.gameObjPtr )
		&& Put( in_rValue.customParam );
}

bool CommandDataSerializer::Get( AkMonitorData::ActionTriggeredMonitorData& out_rValue )
{
	return Get( out_rValue.playingID )
		&& Get( out_rValue.actionID )
		&& Get( out_rValue.gameObjPtr )
		&& Get( out_rValue.customParam );
}

bool CommandDataSerializer::Put( const AkMonitorData::ActionDelayedMonitorData& in_rValue )
{
	return Put( static_cast<AkMonitorData::ActionTriggeredMonitorData>( in_rValue ) )
		&& Put( in_rValue.delayTime );
}

bool CommandDataSerializer::Get( AkMonitorData::ActionDelayedMonitorData& out_rValue )
{
	return Get( static_cast<AkMonitorData::ActionTriggeredMonitorData&>( out_rValue ) )
		&& Get( out_rValue.delayTime );
}

bool CommandDataSerializer::Put( const AkMonitorData::EventTriggeredMonitorData& in_rValue )
{
	return Put( in_rValue.playingID )
		&& Put( in_rValue.eventID )
		&& Put( in_rValue.gameObjPtr )
		&& Put( in_rValue.customParam );
}

bool CommandDataSerializer::Get( AkMonitorData::EventTriggeredMonitorData& out_rValue )
{
	return Get( out_rValue.playingID )
		&& Get( out_rValue.eventID )
		&& Get( out_rValue.gameObjPtr )
		&& Get( out_rValue.customParam );
}

bool CommandDataSerializer::Put( const AkMonitorData::BankMonitorData& in_rValue )
{
	return Put( (AkUInt32)in_rValue.eNotificationReason )
		&& Put( in_rValue.bankID )
		&& Put( in_rValue.languageID )
		&& Put( in_rValue.uFlags )
		&& Put( in_rValue.wStringSize )
		&& Put( in_rValue.wStringSize ? in_rValue.szBankName : NULL );
}

bool CommandDataSerializer::Get( AkMonitorData::BankMonitorData& out_rValue )
{
	AkUInt32 nNameSize = 0;
	char *pszBankName = NULL;

	bool bResult = Get( (AkUInt32&)out_rValue.eNotificationReason )
		&& Get( out_rValue.bankID )
		&& Get( out_rValue.languageID )
		&& Get( out_rValue.uFlags )
		&& Get( out_rValue.wStringSize )
		&& Get( pszBankName, nNameSize );

	if ( bResult && out_rValue.wStringSize )
		memcpy( &out_rValue.szBankName, pszBankName, out_rValue.wStringSize );	

	return bResult;
}

bool CommandDataSerializer::Put( const AkMonitorData::PrepareMonitorData& in_rValue )
{
	return Put( (AkUInt32)in_rValue.eNotificationReason )
		&& Put( in_rValue.gamesyncIDorEventID )
		&& Put( in_rValue.groupID )
		&& PutEnum( in_rValue.groupType )
		&& Put( in_rValue.uNumEvents );
}

bool CommandDataSerializer::Get( AkMonitorData::PrepareMonitorData& out_rValue )
{
	return Get( (AkUInt32&)out_rValue.eNotificationReason )
		&& Get( out_rValue.gamesyncIDorEventID )
		&& Get( out_rValue.groupID )
		&& GetEnum( out_rValue.groupType )
		&& Get( out_rValue.uNumEvents );
}

bool CommandDataSerializer::Put( const AkMonitorData::BusNotifMonitorData& in_rValue )
{
	return Put( in_rValue.busID )
		&& Put( (AkUInt32)in_rValue.notifReason )
		&& Put( in_rValue.bitsFXBypass )
		&& Put( in_rValue.bitsMask );
}

bool CommandDataSerializer::Get( AkMonitorData::BusNotifMonitorData& out_rValue )
{
	return Get( out_rValue.busID )
		&& Get( (AkUInt32&)out_rValue.notifReason )
		&& Get( out_rValue.bitsFXBypass )
		&& Get( out_rValue.bitsMask );
}

bool CommandDataSerializer::Put( const AkMonitorData::AudioPerfMonitorData& in_rValue )
{
	bool bRet = Put( in_rValue.numFadeTransitionsUsed )
		&& Put( in_rValue.numStateTransitionsUsed )
		&& Put( in_rValue.numRegisteredObjects )
		&& Put( in_rValue.numPlayingIDs )
		&& Put( in_rValue.timers.fInterval )
		&& Put( in_rValue.uCommandQueueActualSize )
	    && Put( in_rValue.fCommandQueuePercentageUsed )
		&& Put( in_rValue.fDSPUsage )
		&& Put( in_rValue.uNumPreparedEvents )
		&& Put( in_rValue.uTotalMemBanks )
		&& Put( in_rValue.uTotalPreparedMemory );

	return bRet;
}

bool CommandDataSerializer::Get( AkMonitorData::AudioPerfMonitorData& out_rValue )
{
	bool bRet = Get( out_rValue.numFadeTransitionsUsed )
		&& Get( out_rValue.numStateTransitionsUsed )
		&& Get( out_rValue.numRegisteredObjects )
		&& Get( out_rValue.numPlayingIDs )
		&& Get( out_rValue.timers.fInterval )
		&& Get( out_rValue.uCommandQueueActualSize )
		&& Get( out_rValue.fCommandQueuePercentageUsed )
		&& Get( out_rValue.fDSPUsage )
		&& Get( out_rValue.uNumPreparedEvents )
		&& Get( out_rValue.uTotalMemBanks )
		&& Get( out_rValue.uTotalPreparedMemory );

	return bRet;
}

bool CommandDataSerializer::Put( const AkMonitorData::GameObjPositionMonitorData& in_rValue )
{
	if ( !Put( in_rValue.ulNumGameObjPositions ) )
		return false;

	for (AkUInt32 i = 0; i < in_rValue.ulNumGameObjPositions; ++i)
	{
		const AkMonitorData::GameObjPosition & data = in_rValue.positions[ i ];
		if ( 	!Put( data.gameObjID )
			||  !Put( data.posIndex )
			||	!Put( data.posType )
			|| 	!Put( data.emitter ) )
			return false;
	}

	return true;
}

bool CommandDataSerializer::Get( AkMonitorData::GameObjPositionMonitorData& out_rValue )
{
	if ( !Get( out_rValue.ulNumGameObjPositions ) )
		return false;

	for (AkUInt32 i = 0; i < out_rValue.ulNumGameObjPositions; ++i)
	{
		AkMonitorData::GameObjPosition & data = out_rValue.positions[ i ];
		if ( 	!Get( data.gameObjID ) 
			||	!Get( data.posIndex )
			||	!Get( data.posType )
			||	!Get( data.emitter) )
			return false;
	}

	return true;
}

bool CommandDataSerializer::Put( const AkMonitorData::ObjRegistrationMonitorData& in_rValue )
{
	return Put( in_rValue.uFlags )
		&& Put(in_rValue.gameObjPtr)
		&& Put(in_rValue.wStringSize)
		&& Put(in_rValue.wStringSize ? in_rValue.szName : NULL);
}

bool CommandDataSerializer::Get( AkMonitorData::ObjRegistrationMonitorData& out_rValue )
{
	AkUInt32 uDummy = 0;
	char* pszMessage = NULL;

	bool bResult = Get( out_rValue.uFlags )
				&& Get( out_rValue.gameObjPtr )
				&& Get( out_rValue.wStringSize )
				&& Get( pszMessage, uDummy );

	if ( bResult && out_rValue.wStringSize )
		memcpy( &out_rValue.szName, pszMessage, out_rValue.wStringSize );	

	return bResult;
}

bool CommandDataSerializer::Put( const AkMonitorData::ErrorMonitorData1& in_rValue )
{
	return Put( in_rValue.playingID )
		&& Put( in_rValue.gameObjID )
		&& PutEnum( in_rValue.eErrorCode )
		&& Put( in_rValue.uParam1 )
		&& Put( in_rValue.soundID );
}

bool CommandDataSerializer::Get( AkMonitorData::ErrorMonitorData1& out_rValue )
{
	return Get( out_rValue.playingID )
		&& Get( out_rValue.gameObjID )
		&& GetEnum( out_rValue.eErrorCode )
		&& Get( out_rValue.uParam1 )
		&& Get( out_rValue.soundID );
}

bool CommandDataSerializer::Put( const AkMonitorData::DebugMonitorData& in_rValue )
{
	return Put( in_rValue.playingID )
		&& Put(in_rValue.gameObjID)
		&& Put(in_rValue.soundID)
		&& Put(in_rValue.wStringSize)
		&& Put( in_rValue.wStringSize ? in_rValue.szMessage : NULL );
}

bool CommandDataSerializer::Get( AkMonitorData::DebugMonitorData& out_rValue )
{
	AkUInt32 uDummy = 0;
	AkUtf16* pszMessage = NULL;

	bool bResult = Get( out_rValue.playingID )
		&& Get(out_rValue.gameObjID)
		&& Get(out_rValue.soundID) 
		&& Get(out_rValue.wStringSize)
		&& Get( pszMessage, uDummy );

	if ( bResult && out_rValue.wStringSize )
		memcpy( &out_rValue.szMessage, pszMessage, out_rValue.wStringSize * sizeof(AkUtf16) );	

	return bResult;
}

bool CommandDataSerializer::Put( const AkMonitorData::PathMonitorData& in_rValue )
{
	return Put( in_rValue.playingID )
		&& Put( in_rValue.ulUniqueID )
		&& Put( (AkInt32)in_rValue.eEvent )
		&& Put( in_rValue.ulIndex );
}

bool CommandDataSerializer::Get( AkMonitorData::PathMonitorData& out_rValue )
{
	return Get( out_rValue.playingID )
		&& Get( out_rValue.ulUniqueID )
		&& Get( (AkInt32&)out_rValue.eEvent )
		&& Get( out_rValue.ulIndex );
}

bool CommandDataSerializer::Put( const AkCustomParamType& in_rValue )
{
	return Put( in_rValue.customParam )
		&& Put( in_rValue.ui32Reserved );
}

bool CommandDataSerializer::Get( AkCustomParamType& out_rValue )
{
	AkInt64 tmp;
	bool ret = Get(tmp) && Get( out_rValue.ui32Reserved );
	out_rValue.customParam = tmp;
	return ret;
}

bool CommandDataSerializer::Put( const AkMonitorData::SwitchMonitorData& in_rValue )
{
	return Put( in_rValue.switchGroupID )
		&& Put( in_rValue.switchState )
		&& Put( in_rValue.gameObj );
}

bool CommandDataSerializer::Get( AkMonitorData::SwitchMonitorData& out_rValue )
{
	return Get( out_rValue.switchGroupID )
		&& Get( out_rValue.switchState )
		&& Get( out_rValue.gameObj );
}

bool CommandDataSerializer::Put( const AkMonitorData::CPUTimerMonitorData& in_rValue )
{
	if (!Put(in_rValue.uNumItems))
		return false;

	for (AkUInt32 i = 0; i < in_rValue.uNumItems; ++i)
	{
		const AkAudiolibTimer::Item & data = in_rValue.item[i];
		if (!Put(data.iStartTick)
			|| !Put(data.iStopTick)
			|| !Put(data.uThreadIdx)
			|| !Put(data.uPluginID)
			|| !Put(data.uPipelineID)
			|| !Put(data.uExtraID))
			return false;
	}

	return true;
}

bool CommandDataSerializer::Get( AkMonitorData::CPUTimerMonitorData& out_rValue )
{
	if (!Get(out_rValue.uNumItems))
		return false;

	for (AkUInt32 i = 0; i < out_rValue.uNumItems; ++i)
	{
		AkAudiolibTimer::Item & data = out_rValue.item[i];
		if (!Get(data.iStartTick)
			|| !Get(data.iStopTick)
			|| !Get(data.uThreadIdx)
			|| !Get(data.uPluginID)
			|| !Get(data.uPipelineID)
			|| !Get(data.uExtraID))
			return false;
	}

	return true;
}

bool CommandDataSerializer::Put(const AkMonitorData::MemoryMonitorData& in_rValue)
{
	if (!Put(in_rValue.uNumPools)
		|| !Put(in_rValue.globalData.uUsed)
		|| !Put(in_rValue.globalData.uDeviceUsed)
		|| !Put(in_rValue.globalData.uReserved)
		|| !Put(in_rValue.globalData.uMax))
		return false;

	for (AkUInt32 i = 0; i < in_rValue.uNumPools; ++i)
	{
		const AkMonitorData::MemoryPoolData & data = in_rValue.poolData[i];
		if (!Put(data.uUsed)
			|| !Put(data.uAllocs)
			|| !Put(data.uFrees)
			|| !Put(data.uPeakUsed))
			return false;
	}

	return true;
}

bool CommandDataSerializer::Get(AkMonitorData::MemoryMonitorData& out_rValue)
{
	if (!Get(out_rValue.uNumPools)
		|| !Get(out_rValue.globalData.uUsed)
		|| !Get(out_rValue.globalData.uDeviceUsed)
		|| !Get(out_rValue.globalData.uReserved)
		|| !Get(out_rValue.globalData.uMax))
		return false;

	for (AkUInt32 i = 0; i < out_rValue.uNumPools; ++i)
	{
		AkMonitorData::MemoryPoolData & data = out_rValue.poolData[i];
		if (!Get(data.uUsed)
			|| !Get(data.uAllocs)
			|| !Get(data.uFrees)
			|| !Get(data.uPeakUsed))
			return false;
	}

	return true;
}

bool CommandDataSerializer::Put( const AkMonitorData::MemoryPoolNameMonitorData& in_rValue )
{
	return Put( in_rValue.ulPoolId )
		&& Put( in_rValue.wStringSize )
		&& Put( in_rValue.wStringSize ? in_rValue.szName : NULL );
}

bool CommandDataSerializer::Get( AkMonitorData::MemoryPoolNameMonitorData& out_rValue )
{
	AkUInt32 uDummy = 0;
	AkUtf16* pszName = NULL;

	bool bResult = 
		   Get( out_rValue.ulPoolId )
		&& Get( out_rValue.wStringSize )
		&& Get( pszName, uDummy );

	if ( bResult && out_rValue.wStringSize )
		memcpy( &out_rValue.szName, pszName, out_rValue.wStringSize * sizeof(AkUtf16) );

	return bResult;
}

bool CommandDataSerializer::Put( const AkMonitorData::EnvironmentMonitorData& in_rValue )
{
	if (!Put(in_rValue.ulNumEnvPacket) || !Put(in_rValue.ulTotalAuxSends))
		return false;

	const AkMonitorData::EnvPacket* pData = &in_rValue.envPacket[0];
	for ( AkUInt32 iPacket = 0; iPacket < in_rValue.ulNumEnvPacket; ++iPacket )
	{
		if (	!Put(pData->gameObjID)
			|| 	!Put(pData->uNumEnv) )
			return false;

		AkMonitorData::AuxSendValue* pAuxSend = (AkMonitorData::AuxSendValue*)(pData + 1);
		for (unsigned int i = 0; i < pData->uNumEnv; ++i)
		{
			if (!Put(*pAuxSend))
				return false;
			
			pAuxSend++;
		}

		pData = (AkMonitorData::EnvPacket*)(pAuxSend);
	}

	return true;
}

bool CommandDataSerializer::Get( AkMonitorData::EnvironmentMonitorData& out_rValue )
{
	if ( !Get( out_rValue.ulNumEnvPacket ) || ! Get(out_rValue.ulTotalAuxSends))
		return false;

	AkMonitorData::EnvPacket* pData = &out_rValue.envPacket[0];
	for ( AkUInt32 iPacket = 0; iPacket < out_rValue.ulNumEnvPacket; ++iPacket )
	{
		if (	!Get(pData->gameObjID)
			|| !Get(pData->uNumEnv))
			return false;

		AkMonitorData::AuxSendValue* pAuxSend = (AkMonitorData::AuxSendValue*)(pData + 1);
		for (unsigned int i = 0; i < pData->uNumEnv; ++i)
		{
			if (!Get(*pAuxSend))
				return false;

			pAuxSend++;
		}

		pData = (AkMonitorData::EnvPacket*)(pAuxSend);
	}

	return true;
}

bool CommandDataSerializer::Put( const AkMonitorData::SendsMonitorData& in_rValue )
{
	if ( !Put( in_rValue.ulNumSends ) )
		return false;

	for ( AkUInt32 iSend = 0; iSend < in_rValue.ulNumSends; ++iSend )
	{
		const AkMonitorData::SendsData & data = in_rValue.sends[ iSend ];
		if ( 	!Put( data.pipelineID ) 
			|| 	!Put( data.gameObjID )
			|| 	!Put( data.soundID )
			|| 	!Put( data.fControlValue )
			|| 	!Put( data.auxBusID )
			|| 	!Put( data.busObjID )
			|| 	!PutEnum( data.eSendType ) )
			return false;
	}

	return true;
}

bool CommandDataSerializer::Get( AkMonitorData::SendsMonitorData& out_rValue )
{
	if ( !Get( out_rValue.ulNumSends ) )
		return false;

	for ( AkUInt32 iSend = 0; iSend < out_rValue.ulNumSends; ++iSend )
	{
		AkMonitorData::SendsData & data = out_rValue.sends[ iSend ];
		if ( 	!Get( data.pipelineID ) 
			|| 	!Get( data.gameObjID )
			|| 	!Get( data.soundID )
			|| 	!Get( data.fControlValue )
			|| 	!Get( data.auxBusID )
			|| 	!Get( data.busObjID )
			|| 	!GetEnum( data.eSendType ) )
			return false;
	}

	return true;
}

bool CommandDataSerializer::Put( const AkMonitorData::ObsOccMonitorData& in_rValue )
{
	if ( !Put( in_rValue.ulNumPacket) )
		return false;

	for ( AkUInt32 iPacket = 0; iPacket < in_rValue.ulNumPacket; ++iPacket )
	{
		const AkMonitorData::ObsOccPacket & data = in_rValue.obsOccPacket[ iPacket ];
		if (	!Put(data.gameObjID) ||
				!Put(data.listenerID) ||
				!Put(data.fPositionX) ||
				!Put(data.fPositionY) ||
				!Put(data.fPositionZ) ||
				!Put(data.fObsValue) || 
				!Put(data.fOccValue) )
			return false;
	}

	return true;
}

bool CommandDataSerializer::Get( AkMonitorData::ObsOccMonitorData& out_rValue )
{
	if ( !Get( out_rValue.ulNumPacket ) )
		return false;

	for ( AkUInt32 iPacket = 0; iPacket < out_rValue.ulNumPacket; ++iPacket )
	{
		AkMonitorData::ObsOccPacket & data = out_rValue.obsOccPacket[ iPacket ];
		if ( !Get( data.gameObjID ) || 
			!Get(data.listenerID)	||
			!Get(data.fPositionX)   ||
			!Get(data.fPositionY)   ||
			!Get(data.fPositionZ)   ||
			!Get(data.fObsValue)	||
			!Get(data.fOccValue))
			return false;
	}

	return true;
}

bool CommandDataSerializer::Put( const AkMonitorData::ListenerMonitorData& in_rValue )
{
	AkUInt32 uNumListeners = in_rValue.uNumListeners;

	if (!Put(in_rValue.uSizeTotal) ||
		!Put(in_rValue.ulNumGameObjMask) ||
		!Put(in_rValue.ulNumListenerIDsForGameObjs) ||
		!Put(in_rValue.uVolumes) ||
		!Put(in_rValue.uNumListeners))
		return false;

	const AkMonitorData::ListenerPacket* pListenerPacket = (const AkMonitorData::ListenerPacket*)(&in_rValue + 1);
	for (AkUInt32 i = 0; i < uNumListeners; ++i)
	{
		if (	!Put(pListenerPacket[i].uListenerID)
			||	!Put(pListenerPacket[i].bSpatialized)
			||	!Put(pListenerPacket[i].uConfig))
			return false;
	}

	const AkMonitorData::GameObjectListenerMaskPacket* pGameObj = (const AkMonitorData::GameObjectListenerMaskPacket*)(pListenerPacket + uNumListeners);
	for ( AkUInt32 i = 0; i < in_rValue.ulNumGameObjMask; ++i )
	{
		if (	!Put(pGameObj->gameObject)
			||	!Put(pGameObj->uNumListeners))
			return false;

		const AkWwiseGameObjectID* pObjLstrns = (AkWwiseGameObjectID*)(pGameObj + 1);
		for (AkUInt32 j = 0; j < pGameObj->uNumListeners; ++j)
		{
			if (!Put(*(pObjLstrns+j)))
				return false;
		}

		pGameObj = (const AkMonitorData::GameObjectListenerMaskPacket*)(pObjLstrns + pGameObj->uNumListeners);
	}

	const AkReal32* pVolumes = (AkReal32*)(pGameObj);
	for ( AkUInt32 i = 0; i < in_rValue.uVolumes; ++i )
	{
		if (!Put(pVolumes[i]))
			return false;
	}

	return true;
}

bool CommandDataSerializer::Get( AkMonitorData::ListenerMonitorData& out_rValue )
{
	//uSizeTotal is read by the caller.
	//ulNumGameObjMask
	//uVolumes
	
	AkMonitorData::ListenerPacket* pListenerPacket = (AkMonitorData::ListenerPacket*)(&out_rValue + 1);
	for( AkUInt32 i = 0; i < out_rValue.uNumListeners; ++i )
	{
		
		if (	!Get(pListenerPacket[i].uListenerID)
			|| !Get(pListenerPacket[i].bSpatialized)
			|| !Get(pListenerPacket[i].uConfig))
			return false;
	}

	AkMonitorData::GameObjectListenerMaskPacket* pGameObj = (AkMonitorData::GameObjectListenerMaskPacket*)(pListenerPacket + out_rValue.uNumListeners);
	for ( AkUInt32 i = 0; i < out_rValue.ulNumGameObjMask; ++i )
	{
		if ( 	!Get( pGameObj->gameObject ) 
			|| 	!Get( pGameObj->uNumListeners ) )
			return false;

		AkWwiseGameObjectID* pObjLstrns = (AkWwiseGameObjectID*)(pGameObj + 1);
		for (AkUInt32 j = 0; j < pGameObj->uNumListeners; ++j)
		{
			if (!Get(*(pObjLstrns+j)))
				return false;
		}

		pGameObj = (AkMonitorData::GameObjectListenerMaskPacket*)(pObjLstrns + pGameObj->uNumListeners);
	}

	AkReal32* pVolumes = (AkReal32*)(pGameObj);
	for ( AkUInt32 i = 0; i < out_rValue.uVolumes; ++i )
	{
		if (!Get(pVolumes[i]))
			return false;
	}

	return true;
}

bool CommandDataSerializer::Put( const AkMonitorData::ControllerMonitorData& in_rValue )
{
	for( AkUInt32 i = 0; i < AK_MAX_NUM_CONTROLLER_MONITORING; ++i )
	{
		const AkMonitorData::ControllerPacket & data = in_rValue.controllers[ i ];
		if ( 	!Put( data.bIsActive ) 
			|| 	!Put( data.Volume ) )
			return false;
	}

	if ( !Put( in_rValue.ulNumGameObjMask ) )
		return false;

	for ( AkUInt32 i = 0; i < in_rValue.ulNumGameObjMask; ++i )
	{
		const AkMonitorData::GameObjectControllerMaskPacket & data = in_rValue.gameObjMask[ i ];
		if ( 	!Put( data.gameObject ) 
			|| 	!Put( data.uControllerMask ) )
			return false;
	}

	return true;
}

bool CommandDataSerializer::Get( AkMonitorData::ControllerMonitorData& out_rValue )
{
	for( AkUInt32 i = 0; i < AK_MAX_NUM_CONTROLLER_MONITORING; ++i )
	{
		AkMonitorData::ControllerPacket & data = out_rValue.controllers[ i ];
		if ( 	!Get( data.bIsActive ) 
			|| 	!Get( data.Volume ) )
			return false;
	}

	if ( !Get( out_rValue.ulNumGameObjMask ) )
		return false;

	for ( AkUInt32 i = 0; i < out_rValue.ulNumGameObjMask; ++i )
	{
		AkMonitorData::GameObjectControllerMaskPacket & data = out_rValue.gameObjMask[ i ];
		if ( 	!Get( data.gameObject ) 
			|| 	!Get( data.uControllerMask ) )
			return false;
	}

	return true;
}

bool CommandDataSerializer::Put( const AkMonitorData::DeviceRecordMonitorData& in_rValue )
{
	if ( !Put( in_rValue.deviceID )
		|| !Put( in_rValue.bCanWrite )
        || !Put( in_rValue.bCanRead )
		|| !Put( in_rValue.szDeviceName )
		|| !Put( in_rValue.uStringSize ) )
		return false;

    return true;
}

bool CommandDataSerializer::Get( AkMonitorData::DeviceRecordMonitorData& out_rValue )
{
    AkUInt32 uDummy = 0;
	AkUtf16* pszName = NULL;
    bool bResult = false;

	bResult = 
        ( Get( out_rValue.deviceID )
        && Get( out_rValue.bCanWrite )
		&& Get( out_rValue.bCanRead )
		&& Get( pszName, uDummy ) 
        && Get( out_rValue.uStringSize ) );

    if ( bResult )
    {
     	if ( out_rValue.uStringSize )
		{
			AKASSERT( uDummy == out_rValue.uStringSize );
    
			out_rValue.uStringSize = AkMin( out_rValue.uStringSize, AK_MONITOR_DEVICENAME_MAXLENGTH );
    
			memcpy( &out_rValue.szDeviceName, pszName, out_rValue.uStringSize*sizeof(AkUtf16) );	
    
			// In case the string was bigger than MONITOR_MSG_MAXLENGTH
			out_rValue.szDeviceName[ out_rValue.uStringSize - 1 ] = 0;
		}
		else
		{
			out_rValue.szDeviceName[0] = 0;
		}
    }
	
	return bResult;
}

bool CommandDataSerializer::Put( const AkMonitorData::StreamRecordMonitorData& in_rValue )
{
    if ( !Put( in_rValue.ulNumNewRecords ) )
		return false;

	for ( AkUInt32 i = 0; i < in_rValue.ulNumNewRecords; ++i )
	{
		const AkMonitorData::StreamRecord & data = in_rValue.streamRecords[ i ];
		if ( !Put( data.uStreamID )
		  || !Put( data.deviceID )
		  || !Put( data.szStreamName )
          || !Put( data.uStringSize )
		  || !Put( data.uFileSize )
		  || !Put( data.uCustomParamSize )
		  || !Put( data.uCustomParam )
		  || !Put( data.bIsAutoStream ) 
		  || !Put( data.bIsCachingStream ) )
			return false;
	}
    return true;
}

bool CommandDataSerializer::Get( AkMonitorData::StreamRecordMonitorData& out_rValue )
{
    AkUInt32 uDummy = 0;
	AkUtf16* pszName = NULL;
    bool bResult = false;

    if ( !Get( out_rValue.ulNumNewRecords ) )
		return false;

	for ( AkUInt32 i = 0; i < out_rValue.ulNumNewRecords; ++i )
	{
		AkMonitorData::StreamRecord & data = out_rValue.streamRecords[ i ];
		bResult = 
          ( Get( data.uStreamID )
		  && Get( data.deviceID )
		  && Get( pszName, uDummy )
          && Get( data.uStringSize )
		  && Get( data.uFileSize )
		  && Get( data.uCustomParamSize )
		  && Get( data.uCustomParam )
		  && Get( data.bIsAutoStream ) 
		  && Get( data.bIsCachingStream ) 
		  );

        if ( bResult )
        {
     	    if ( out_rValue.streamRecords[i].uStringSize )
		    {
			    AKASSERT( uDummy == out_rValue.streamRecords[i].uStringSize );
    	
				out_rValue.streamRecords[i].uStringSize = AkMin( out_rValue.streamRecords[i].uStringSize, AK_MONITOR_STREAMNAME_MAXLENGTH );
    	
			    memcpy( &out_rValue.streamRecords[i].szStreamName, pszName, out_rValue.streamRecords[i].uStringSize*sizeof(AkUtf16) );	
    	
			    // In case the string was bigger than MONITOR_MSG_MAXLENGTH
			    out_rValue.streamRecords[i].szStreamName[ out_rValue.streamRecords[i].uStringSize - 1 ] = 0;
		    }
		    else
		    {
			    out_rValue.streamRecords[i].szStreamName[0] = 0;
		    }
        }
	}

	return bResult;
}

bool CommandDataSerializer::Put( const AkMonitorData::StreamingMonitorData& in_rValue )
{
    if ( !Put( in_rValue.fInterval ) )
		return false;

    if ( !Put( in_rValue.ulNumStreams ) )
		return false;

	for ( AkUInt32 i = 0; i < in_rValue.ulNumStreams; ++i )
	{
		const AkMonitorData::StreamData & data = in_rValue.streamData[ i ];
		if ( !Put( data.uStreamID )
		  || !Put( data.uPriority )
		  || !Put( data.uFilePosition )
		  || !Put( data.uTargetBufferingSize )
		  || !Put( data.uVirtualBufferingSize )
		  || !Put( data.uBufferedSize )
		  || !Put( data.uNumBytesTransfered )
		  || !Put( data.uNumBytesTransferedLowLevel )
		  || !Put( data.uMemoryReferenced )
		  || !Put( data.fEstimatedThroughput )
		  || !Put( data.bActive ) )
			return false;
	}
    return true;
}

bool CommandDataSerializer::Get( AkMonitorData::StreamingMonitorData& out_rValue )
{
    if ( !Get( out_rValue.fInterval ) )
		return false;

    if ( !Get( out_rValue.ulNumStreams ) )
		return false;

	for ( AkUInt32 i = 0; i < out_rValue.ulNumStreams; ++i )
	{
		AkMonitorData::StreamData & data = out_rValue.streamData[ i ];
		if ( !Get( data.uStreamID )
		  || !Get( data.uPriority ) 
		  || !Get( data.uFilePosition )
		  || !Get( data.uTargetBufferingSize )
		  || !Get( data.uVirtualBufferingSize ) 
		  || !Get( data.uBufferedSize )
		  || !Get( data.uNumBytesTransfered )
		  || !Get( data.uNumBytesTransferedLowLevel )
		  || !Get( data.uMemoryReferenced )
		  || !Get( data.fEstimatedThroughput ) 
		  || !Get( data.bActive ) )
			return false;
	}

	return true;
}

bool CommandDataSerializer::Put( const	AkMonitorData::StreamDeviceMonitorData& in_rValue )
{
	if ( !Put( in_rValue.fInterval ) )
		return false;

    if ( !Put( in_rValue.ulNumDevices ) )
		return false;

	for ( AkUInt32 i = 0; i < in_rValue.ulNumDevices; ++i )
	{
		const AkMonitorData::DeviceData & data = in_rValue.deviceData[ i ];
		if ( !Put( data.deviceID )
		  || !Put( data.uMemSize )
		  || !Put( data.uMemUsed )
		  || !Put( data.uAllocs )
		  || !Put( data.uFrees )
		  || !Put( data.uPeakRefdMemUsed )
		  || !Put( data.uUnreferencedCachedBytes )
		  || !Put( data.uGranularity )
		  || !Put( data.uNumActiveStreams )
		  || !Put( data.uTotalBytesTransferred )
		  || !Put( data.uLowLevelBytesTransferred )
		  || !Put(data.fAvgCacheEfficiency)
		  || !Put( data.uNumLowLevelRequestsCompleted ) 
		  || !Put( data.uNumLowLevelRequestsCancelled ) 
		  || !Put( data.uNumLowLevelRequestsPending )
		  || !Put( data.uCustomParam )
		  || !Put( data.uCachePinnedBytes ) )
			return false;
	}
    return true;
}

bool CommandDataSerializer::Get( AkMonitorData::StreamDeviceMonitorData& out_rValue )
{
	if ( !Get( out_rValue.fInterval ) )
		return false;

    if ( !Get( out_rValue.ulNumDevices ) )
		return false;

	for ( AkUInt32 i = 0; i < out_rValue.ulNumDevices; ++i )
	{
		AkMonitorData::DeviceData & data = out_rValue.deviceData[ i ];
		if ( !Get( data.deviceID )
		  || !Get( data.uMemSize )
		  || !Get( data.uMemUsed )
		  || !Get( data.uAllocs )
		  || !Get( data.uFrees )
		  || !Get( data.uPeakRefdMemUsed )
		  || !Get( data.uUnreferencedCachedBytes )
		  || !Get( data.uGranularity )
		  || !Get( data.uNumActiveStreams )
		  || !Get( data.uTotalBytesTransferred )
		  || !Get( data.uLowLevelBytesTransferred )
		  || !Get( data.fAvgCacheEfficiency )
		  || !Get( data.uNumLowLevelRequestsCompleted )
		  || !Get( data.uNumLowLevelRequestsCancelled )
		  || !Get( data.uNumLowLevelRequestsPending )
		  || !Get( data.uCustomParam )
		  || !Get( data.uCachePinnedBytes ) )
			return false;
	}

	return true;
}

bool CommandDataSerializer::Put( const AkMonitorData::MarkersMonitorData& in_rValue )
{
	bool bRet = Put( in_rValue.playingID )
		&& Put( in_rValue.gameObjPtr )
		&& Put( (AkInt32)in_rValue.eNotificationReason );
	
	// Container history
	{
		bRet = bRet
			&& Put( in_rValue.cntrHistArray.uiArraySize );

		for( AkUInt32 i = 0; i < in_rValue.cntrHistArray.uiArraySize && bRet; ++i )
			Put( in_rValue.cntrHistArray.aCntrHist[i] );
	}

	bRet = bRet && Put( in_rValue.customParam )
		&& Put( in_rValue.targetObjectID )
		&& Put( in_rValue.wStringSize )
		&& Put( in_rValue.wStringSize ? in_rValue.szLabel : NULL );

	return bRet;
}

bool CommandDataSerializer::Get( AkMonitorData::MarkersMonitorData& out_rValue )
{
	bool bRet = Get( out_rValue.playingID )
		&& Get( out_rValue.gameObjPtr )
		&& Get( (AkInt32&)out_rValue.eNotificationReason );
	
	// Container history
	{
		bRet = bRet
			&& Get( out_rValue.cntrHistArray.uiArraySize );

		for( AkUInt32 i = 0; i < out_rValue.cntrHistArray.uiArraySize && bRet; ++i )
			Get( out_rValue.cntrHistArray.aCntrHist[i] );
	}

	AkUInt32 nLabelSize = 0;
	char *pszLabel = NULL;

	bRet = bRet && Get( out_rValue.customParam )
		&& Get( out_rValue.targetObjectID )
		&& Get( out_rValue.wStringSize )
		&& Get( pszLabel, nLabelSize );

	if ( bRet && out_rValue.wStringSize )
		memcpy( &out_rValue.szLabel, pszLabel, out_rValue.wStringSize );	

	return bRet;
}

bool CommandDataSerializer::Put( const AkMonitorData::SegmentPositionMonitorData& in_rValue )
{
    if ( !Put( in_rValue.numPositions )  )
		return false;

	for ( AkUInt16 i = 0; i < in_rValue.numPositions; ++i )
	{
		const AkMonitorData::SegmentPositionData & data = in_rValue.positions[ i ];

		if ( !Put( data.f64Position )
		  || !Put( data.playingID )
		  || !Put( data.segmentID )
		  || !Put( data.customParam ) )
			return false;
	}

	return true;
}

bool CommandDataSerializer::Get( AkMonitorData::SegmentPositionMonitorData& out_rValue )
{
    if ( !Get( out_rValue.numPositions ) )
		return false;

	for ( AkUInt16 i = 0; i < out_rValue.numPositions; ++i )
	{
		AkMonitorData::SegmentPositionData & data = out_rValue.positions[ i ];

		if ( !Get( data.f64Position )
		  || !Get( data.playingID )
		  || !Get( data.segmentID )
		  || !Get( data.customParam ) )
			return false;
	}

	return true;
}

bool CommandDataSerializer::Put(const AkMonitorData::SoundPositionMonitorData& in_rValue)
{
	if ( !Put( in_rValue.numPositions ) )
		return false;

	for ( AkUInt16 i = 0; i < in_rValue.numPositions; ++i )
	{
		const AkMonitorData::SoundPositionData & data = in_rValue.positions[i];

		if ( !Put( data.audioNodeID )
		  || !Put( data.mediaID )
		  || !Put( data.msTime ) )
			return false;
	}

	return true;
}

bool CommandDataSerializer::Get(AkMonitorData::SoundPositionMonitorData& out_rValue)
{
	if ( !Get( out_rValue.numPositions ) )
		return false;

	for ( AkUInt16 i = 0; i < out_rValue.numPositions; ++i )
	{
		AkMonitorData::SoundPositionData & data = out_rValue.positions[i];

		if ( !Get( data.audioNodeID )
		  || !Get( data.mediaID )
		  || !Get( data.msTime ) )
			return false;
	}

	return true;
}

bool CommandDataSerializer::Put( const	AkMonitorData::LoadedSoundBankMonitorData& in_rValue )
{
	return Put( in_rValue.bankID )
		&& Put( in_rValue.memPoolID )
		&& Put( in_rValue.uBankSize )
		&& Put( in_rValue.uMetaDataSize )
		&& Put( in_rValue.uNumIndexableItems )
		&& Put( in_rValue.uNumMediaItems )
		&& Put( in_rValue.bIsExplicitelyLoaded )
		&& Put( in_rValue.bIsDestroyed );
}

bool CommandDataSerializer::Get( AkMonitorData::LoadedSoundBankMonitorData& out_rValue )
{
	return Get( out_rValue.bankID )
		&& Get( out_rValue.memPoolID )
		&& Get( out_rValue.uBankSize )
		&& Get( out_rValue.uMetaDataSize )
		&& Get( out_rValue.uNumIndexableItems )
		&& Get( out_rValue.uNumMediaItems )
		&& Get( out_rValue.bIsExplicitelyLoaded )
		&& Get( out_rValue.bIsDestroyed );
}

bool CommandDataSerializer::Put( const	AkMonitorData::MediaPreparedMonitorData& in_rValue )
{
	if ( !Put( in_rValue.uMediaID )
	  || !Put( in_rValue.uArraySize ) )
		return false;

	for ( AkUInt16 i = 0; i < in_rValue.uArraySize; ++i )
	{
		if (!Put(in_rValue.bankMedia[i].bankID) || !Put(in_rValue.bankMedia[i].uMediaSize) || !Put(in_rValue.bankMedia[i].uFormat))
			return false;
	}
	return true;
}

bool CommandDataSerializer::Get( AkMonitorData::MediaPreparedMonitorData& out_rValue )
{
	if ( !Get( out_rValue.uMediaID )
	  || !Get( out_rValue.uArraySize ) )
		return false;

	for ( AkUInt16 i = 0; i < out_rValue.uArraySize; ++i )
	{
		if (!Get(out_rValue.bankMedia[i].bankID) || !Get(out_rValue.bankMedia[i].uMediaSize) || !Get(out_rValue.bankMedia[i].uFormat))
			return false;
	}
	return true;
}

bool CommandDataSerializer::Put( const	AkMonitorData::EventPreparedMonitorData& in_rValue )
{
	return Put( in_rValue.eventID )
		&& Put( in_rValue.uRefCount );
}

bool CommandDataSerializer::Get( AkMonitorData::EventPreparedMonitorData& out_rValue )
{
	return Get( out_rValue.eventID )
		&& Get( out_rValue.uRefCount );
}

bool CommandDataSerializer::Put( const WwiseObjectIDext& in_rValue )
{
	return Put( in_rValue.id )
		&& Put( in_rValue.bIsBus );
}
bool CommandDataSerializer::Get( WwiseObjectIDext& out_rValue )
{
	return Get( out_rValue.id )
		&& Get( out_rValue.bIsBus );
}

bool CommandDataSerializer::Put( const	AkMonitorData::GameSyncMonitorData& in_rValue )
{
	return Put( in_rValue.groupID )
		&& Put( in_rValue.syncID )
		&& PutEnum( in_rValue.eSyncType )
		&& Put( in_rValue.bIsEnabled );
}

bool CommandDataSerializer::Get( AkMonitorData::GameSyncMonitorData& out_rValue )
{
	return Get( out_rValue.groupID )
		&& Get( out_rValue.syncID )
		&& GetEnum( out_rValue.eSyncType )
		&& Get( out_rValue.bIsEnabled );
}

bool CommandDataSerializer::Put( const AkMonitorData::CommonDialogueMonitorData& in_rValue )
{
	if ( !Put( in_rValue.idDialogueEvent )
		|| !Put( in_rValue.idObject )
		|| !Put( in_rValue.uPathSize )
		|| !Put( in_rValue.idSequence )
		|| !Put( in_rValue.uRandomChoice )
		|| !Put( in_rValue.uTotalProbability )
		|| !Put( in_rValue.uWeightedDecisionType )
		|| !Put( in_rValue.uWeightedPossibleCount )
		|| !Put( in_rValue.uWeightedTotalCount ))
		return false;

	for ( AkUInt32 i = 0; i < in_rValue.uPathSize; ++i )
	{
		if ( !PutEnum( in_rValue.aPath[i] ) )
			return false;
	}

	return true;
}

bool CommandDataSerializer::Get( AkMonitorData::CommonDialogueMonitorData& out_rValue )
{
	if ( !Get( out_rValue.idDialogueEvent )
		|| !Get( out_rValue.idObject )
		|| !Get( out_rValue.uPathSize )
		|| !Get( out_rValue.idSequence ) 
		|| !Get( out_rValue.uRandomChoice )
		|| !Get( out_rValue.uTotalProbability )
		|| !Get( out_rValue.uWeightedDecisionType )
		|| !Get( out_rValue.uWeightedPossibleCount )
		|| !Get( out_rValue.uWeightedTotalCount ) )
		return false;

	for ( AkUInt32 i = 0; i < out_rValue.uPathSize; ++i )
	{
		if ( !GetEnum( out_rValue.aPath[i] ) )
			return false;
	}

	return true;
}

bool CommandDataSerializer::Put( const	AkMonitorData::MusicTransitionMonitorData& in_rValue )
{
	if (!Put((AkInt32)in_rValue.eNotificationReason) ||
		!Put(in_rValue.gameObj) ||
		!Put(in_rValue.ownerID) ||
		!Put(in_rValue.nodeSrcID) ||
		!Put(in_rValue.nodeDestID) ||
		!Put(in_rValue.playingID) ||
		!Put(in_rValue.segmentDest) ||
		!Put(in_rValue.segmentSrc) ||
		!Put(in_rValue.time) ||
		!Put(in_rValue.uTransitionRuleIndex) ||
		!Put(in_rValue.cueSrc) ||
		!Put(in_rValue.cueDest))
		return false;
	return true;
}

bool CommandDataSerializer::Get( AkMonitorData::MusicTransitionMonitorData& out_rValue )
{
	if (!Get((AkInt32&)out_rValue.eNotificationReason) ||
		!Get(out_rValue.gameObj) ||
		!Get(out_rValue.ownerID) ||
		!Get(out_rValue.nodeSrcID) ||
		!Get(out_rValue.nodeDestID) ||
		!Get(out_rValue.playingID) ||
		!Get(out_rValue.segmentDest) ||
		!Get(out_rValue.segmentSrc) ||
		!Get(out_rValue.time) ||
		!Get(out_rValue.uTransitionRuleIndex) ||
		!Get(out_rValue.cueSrc) ||
		!Get(out_rValue.cueDest))
		return false;
	return true;
}


bool CommandDataSerializer::Put( const	AkMonitorData::MidiTargetMonitorData& in_rValue )
{
	if (!Put((AkInt32)in_rValue.eNotificationReason) ||
		!Put(in_rValue.gameObj) ||
		!Put(in_rValue.playingID) ||
		!Put(in_rValue.musicTrackID) ||
		!Put(in_rValue.midiTargetID) )
		return false;
	return true;
}

bool CommandDataSerializer::Get( AkMonitorData::MidiTargetMonitorData& out_rValue )
{
	if (!Get((AkInt32&)out_rValue.eNotificationReason) ||
		!Get(out_rValue.gameObj) ||
		!Get(out_rValue.playingID) ||
		!Get(out_rValue.musicTrackID) ||
		!Get(out_rValue.midiTargetID) )
		return false;
	return true;
}


bool CommandDataSerializer::Put( const	AkMonitorData::MidiEventMonitorData& in_rValue )
{
	if (!Put( *(AkMidiEventEx*)in_rValue.midiEvent ) ||
		!Put(in_rValue.gameObj) ||
		!Put(in_rValue.musicTrackID) ||
		!Put(in_rValue.midiTargetID) )
		return false;
	return true;
}

bool CommandDataSerializer::Get( AkMonitorData::MidiEventMonitorData& out_rValue )
{
	if (!Get( *(AkMidiEventEx*)out_rValue.midiEvent ) ||
		!Get(out_rValue.gameObj) ||
		!Get(out_rValue.musicTrackID) ||
		!Get(out_rValue.midiTargetID) )
		return false;
	return true;
}

bool CommandDataSerializer::Put( const AkMonitorData::PluginMonitorData& in_rValue )
{
	// Write the byte stream represented by PluginCustomDataMonitorData directly.
	return Put( in_rValue.audioNodeID )
		&& Put(in_rValue.gameObj)
		&& Put(in_rValue.pluginID)
		&& Put( in_rValue.uFXIndex )
		&& Put( (void*)in_rValue.arBytes, in_rValue.uDataSize );
}
bool CommandDataSerializer::Get( AkMonitorData::PluginMonitorData& out_rValue )
{
	if ( !Get( out_rValue.audioNodeID )
		|| !Get(out_rValue.gameObj)
		|| !Get( out_rValue.pluginID ) 
		|| !Get( out_rValue.uFXIndex ) )
		return false;

	// Read the byte stream and COPY it onto the members of PluginCustomDataMonitorData.
	// out_rValue should obviously have been allocated with the appropriate size.
	void * pData;
	if ( !Get( pData, out_rValue.uDataSize ) )
		return false;

	if ( out_rValue.uDataSize > 0 )
		memcpy( out_rValue.arBytes, pData, out_rValue.uDataSize );
	else 
		out_rValue.arBytes[0] = 0;

	return true;
}

bool CommandDataSerializer::Put( const AkMonitorData::PluginMonitorError& in_rValue )
{
	// Write the byte stream represented by PluginCustomDataMonitorData directly.
	return Put( in_rValue.pluginTypeID )
		&& Put( in_rValue.pluginUniqueID )
		&& Put( in_rValue.playingID )
		&& Put( in_rValue.gameObjID )
		&& Put( in_rValue.soundID )
		&& Put( in_rValue.wStringSize )
		&& Put( in_rValue.wStringSize ? in_rValue.szMessage : NULL );
}

bool CommandDataSerializer::Get( AkMonitorData::PluginMonitorError& out_rValue )
{
	AkUInt32 uDummy = 0;
	AkUtf16* pszMessage = NULL;

	bool bResult = Get( out_rValue.pluginTypeID )
		&& Get( out_rValue.pluginUniqueID )
		&& Get( out_rValue.playingID )
		&& Get( out_rValue.gameObjID )
		&& Get( out_rValue.soundID )
		&& Get( out_rValue.wStringSize )
		&& Get( pszMessage, uDummy );

	if ( bResult && out_rValue.wStringSize )
		memcpy( &out_rValue.szMessage, pszMessage, out_rValue.wStringSize * sizeof(AkUtf16) );	

	return bResult;
}

bool CommandDataSerializer::Put( const AkMonitorData::ExternalSourceMonitorData &in_rValue )
{
	if ( !Put( in_rValue.idGameObj )
		|| !Put( in_rValue.idPlay ) 
		|| !Put( in_rValue.idSource )
		|| !Put( in_rValue.szUtf16File ))
		return false;
	return true;
}

bool CommandDataSerializer::Put( const AkWwiseGraphCurve& in_rValue )
{
	return Put((AkUInt16)in_rValue.m_eScaling) && SerializeArray(in_rValue.m_ulConversionArraySize, in_rValue.m_pArrayConversion);
}

bool CommandDataSerializer::Get( AkWwiseGraphCurve& out_rValue )
{
	AkUInt16 uScaling = 0;
	bool bRet = Get(uScaling) && DeserializeArray(out_rValue.m_ulConversionArraySize, out_rValue.m_pArrayConversion);
	out_rValue.m_eScaling = (AkCurveScaling)uScaling;
	return bRet;
}

bool CommandDataSerializer::Put( const AkWwiseRTPCreg& in_rValue )
{
	return Put( in_rValue.m_FXID )
		&& Put( in_rValue.m_RTPCID )
		&& PutEnum( in_rValue.m_paramID )
		&& Put( in_rValue.m_RTPCCurveID )
		&& Put((AkUInt16)in_rValue.m_eScaling) 
		&& SerializeArray(in_rValue.m_ulConversionArraySize, in_rValue.m_pArrayConversion);
}

bool CommandDataSerializer::Put( const AkStateUpdate& in_rValue )
{
	return Put(in_rValue.ulStateInstanceID) && Put(in_rValue.ulStateID);
}

bool CommandDataSerializer::Get( AkStateUpdate& out_rValue )
{
	return Get(out_rValue.ulStateInstanceID) && Get(out_rValue.ulStateID);
}

bool CommandDataSerializer::Put( const AkStateGroupUpdate& in_rValue )
{
	return Put(in_rValue.ulGroupID) && Put(in_rValue.ulStateCount) && PutEnum(in_rValue.eSyncType);
}

bool CommandDataSerializer::Get( AkStateGroupUpdate& out_rValue )
{
	return Get(out_rValue.ulGroupID) && Get(out_rValue.ulStateCount) && GetEnum(out_rValue.eSyncType);
}

bool CommandDataSerializer::Put( const AkStatePropertyUpdate& in_rValue )
{
	return Put(in_rValue.propertyId) && PutEnum(in_rValue.accumType) && Put(in_rValue.inDb);
}

bool CommandDataSerializer::Get( AkStatePropertyUpdate& out_rValue )
{
	return Get(out_rValue.propertyId) && GetEnum(out_rValue.accumType) && Get(out_rValue.inDb);
}


bool CommandDataSerializer::Put( const AkEffectUpdate& in_rValue )
{
	return Put(in_rValue.ulEffectID) && Put(in_rValue.uiIndex) && Put(in_rValue.bShared);
}

bool CommandDataSerializer::Get( AkEffectUpdate& out_rValue )
{
	return Get(out_rValue.ulEffectID) && Get(out_rValue.uiIndex) && Get(out_rValue.bShared);
}


bool CommandDataSerializer::Put( const AkMIDIEvent& in_rValue )
{
	return Put(in_rValue.byType) && Put(in_rValue.byChan) && Put(in_rValue.Gen.byParam1) && Put(in_rValue.Gen.byParam2);
}

bool CommandDataSerializer::Get( AkMIDIEvent& out_rValue )
{
	return Get(out_rValue.byType) && Get(out_rValue.byChan) && Get(out_rValue.Gen.byParam1) && Get(out_rValue.Gen.byParam2);
}


bool CommandDataSerializer::Get( AkWwiseRTPCreg& out_rValue )
{
	AkUInt16 uScaling = 0;
	bool bRet = Get( out_rValue.m_FXID )
		&& Get( out_rValue.m_RTPCID )
		&& GetEnum( out_rValue.m_paramID )
		&& Get( out_rValue.m_RTPCCurveID )
		&& Get(uScaling) 
		&& DeserializeArray(out_rValue.m_ulConversionArraySize, out_rValue.m_pArrayConversion);

	out_rValue.m_eScaling = (AkCurveScaling)uScaling;
	return bRet;
}

bool CommandDataSerializer::Get( AkMonitorData::ExternalSourceMonitorData &out_rValue, AkUInt16 in_uiStringSize)
{
	AkUtf16 *szUtf16File = NULL;
	AkUInt32 uDummy = 0;

	if ( !Get( out_rValue.idGameObj )
		|| !Get( out_rValue.idPlay ) 
		|| !Get( out_rValue.idSource )
		|| !Get( szUtf16File, uDummy ))
		return false;

	out_rValue.wStringSize = in_uiStringSize;
	memcpy( &out_rValue.szUtf16File, szUtf16File, in_uiStringSize * sizeof(AkUtf16) );
	return true;
}

bool CommandDataSerializer::Put( const AkMonitorData::PlatformSinkTypeData& in_rValue )
{
	return Put( in_rValue.uSinkType )
		&& Put( in_rValue.uBufSize )
		&& Put( in_rValue.uBufSize ? in_rValue.szNameBuf : NULL );
}

bool CommandDataSerializer::Get( AkMonitorData::PlatformSinkTypeData& out_rValue )
{
	AkUInt32 nNameSize = 0;
	char *pszName = NULL;

	bool bResult = Get( out_rValue.uSinkType )
		&& Get( out_rValue.uBufSize )
		&& Get( pszName, nNameSize );

	if ( bResult && out_rValue.uBufSize )
		memcpy( &out_rValue.szNameBuf, pszName, out_rValue.uBufSize );	

	return bResult;
}

bool CommandDataSerializer::Put(const AkMonitorData::InitData& in_rValue)
{
	bool bRet = Put(in_rValue.fFreqRatio)
		&& Put(in_rValue.uCoreSampleRate)
		&& Put(in_rValue.uFramesPerBuffer)
		&& Put(in_rValue.floorPlaneID);

	return bRet;
}

bool CommandDataSerializer::Get(AkMonitorData::InitData& out_rValue)
{
	bool bRet = Get(out_rValue.fFreqRatio)
		&& Get(out_rValue.uCoreSampleRate)
		&& Get(out_rValue.uFramesPerBuffer)
		&& Get(out_rValue.floorPlaneID);

	return bRet;
}

void CommandDataSerializer::SetDataPeeking( bool in_bPeekData )
{
	if( in_bPeekData )
		m_readPosBeforePeeking = m_readPos;
	else
		m_readPos = m_readPosBeforePeeking;
}

AkInt16 CommandDataSerializer::Swap( const AkInt16& in_rValue ) const
{
	return GetSwapEndian() ? AK::EndianByteSwap::WordSwap( in_rValue ) : in_rValue;
}

AkUInt16 CommandDataSerializer::Swap( const AkUInt16& in_rValue ) const
{
	return GetSwapEndian() ? AK::EndianByteSwap::WordSwap( in_rValue ) : in_rValue;
}

AkInt32 CommandDataSerializer::Swap( const AkInt32& in_rValue ) const
{
	return GetSwapEndian() ? AK::EndianByteSwap::DWordSwap( in_rValue ) : in_rValue;
}

AkUInt32 CommandDataSerializer::Swap( const AkUInt32& in_rValue ) const
{
	return GetSwapEndian() ? AK::EndianByteSwap::DWordSwap( in_rValue ) : in_rValue;
}

AkInt64 CommandDataSerializer::Swap( const AkInt64& in_rValue ) const
{
    AkInt64 liSwapped;
    liSwapped = GetSwapEndian() ? AK::EndianByteSwap::Int64Swap( in_rValue ) : in_rValue;
	return liSwapped;
}

AkUInt64 CommandDataSerializer::Swap( const AkUInt64& in_rValue ) const
{
    AkUInt64 uiSwapped;
    uiSwapped = GetSwapEndian() ? AK::EndianByteSwap::Int64Swap( in_rValue ) : in_rValue;
	return uiSwapped;
}
/*
AkUInt64 CommandDataSerializer::Swap( const AkUInt64& in_rValue ) const
{
	return GetSwapEndian() ? AK::EndianByteSwap::Int64Swap( in_rValue ) : in_rValue;
}
*/
AkReal32 CommandDataSerializer::Swap( const AkReal32& in_rValue ) const
{
	AkReal32 retVal = in_rValue;
	
	if( GetSwapEndian() )
		AK::EndianByteSwap::Swap( (AkUInt8*)&in_rValue, 4, (AkUInt8*)&retVal );
	
	return retVal;
}

AkReal64 CommandDataSerializer::Swap( const AkReal64& in_rValue ) const
{
	AkReal64 retVal = in_rValue;
	
	if( GetSwapEndian() )
		AK::EndianByteSwap::Swap( (AkUInt8*)&in_rValue, 8, (AkUInt8*)&retVal );
	
	return retVal;
}
