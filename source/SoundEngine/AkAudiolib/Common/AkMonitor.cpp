/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

//////////////////////////////////////////////////////////////////////
//
// AkMonitor.cpp
//
// alessard
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include "AkDeltaMonitor.h"
#ifndef AK_OPTIMIZED

#define AK_MONITOR_IMPLEMENT_ERRORCODES // so that AkMonitorError.h defines the table

#include "AkLEngine.h"
#include "AkMonitor.h"
#include "IALMonitorSink.h"
#include "AkEndianByteSwap.h"
#include "AkRegisteredObj.h"
#include "AkSwitchMgr.h"
#include "AkRTPCMgr.h"
#include "AkRegistryMgr.h"
#include "Ak3DListener.h"
#include "AkBankMgr.h"
#include "AkEvent.h"
#include "CommandDataSerializer.h"
#include "AkProfile.h"
#include "AkURenderer.h"
#include "AkSpatialAudioComponent.h"

#include "../../Communication/Common/MonitorSerializer.h"
#include "../../Communication/Common/CommunicationDefines.h"

#include "AkIDStringMap.h"
#include "AkQueuedMsg.h"

extern AK::Comm::ICommunicationCentral * g_pCommCentral;
extern void _CallGlobalExtensions(AkGlobalCallbackLocation in_eLocation);

// resource monitor
AkResourceMonitorDataSummary AkResourceMonitor::m_DataSummary;
ResourceMonitorFuncArray AkResourceMonitor::aFuncs;

#include <ctype.h>

extern AkPlatformInitSettings g_PDSettings;

#define AK_MAX_MONITORING_SINKS				4
#define MAX_NOTIFICATIONS_PER_CALL			256

// Matches 2 strings exactly, or if either string has a '*' for the last character then match substrings.
bool CompareNoCaseWithWildcard(const char* in_s1, const char* in_s2)
{
	size_t n1 = strlen(in_s1);
	size_t n2 = strlen(in_s2);

	size_t c = 0;
	size_t n = AkMin(n1, n2);
	while( c < n )
	{
		if ((in_s1[c] == '*' && c == (n1 - 1)) || 
			(in_s2[c] == '*' && c == (n2 - 1)))
			return true;

		if( tolower( in_s1[c] ) != tolower( in_s2[c] ) )
			return false;

		++c;
	}
	
	return n1 == n2 || 
		(n1 < n2 && in_s2[c] == '*') ||
		(n1 > n2 && in_s1[c] == '*');
}

extern AkInitSettings g_settings;

AkMonitor*	AkMonitor::m_pInstance 	= NULL;
AkIDStringHash g_idxGameObjectString;

AkMonitor::AkMapGameObjectProfile AkMonitor::m_mapGameObjectChanged;
AkMonitor::AkMeterWatchMap AkMonitor::m_meterWatchMap;
bool AkMonitor::m_bRecapDeltaMonitor = true;

AkUInt32 AkMonitor::m_uLocalOutputErrorLevel = AK::Monitor::ErrorLevel_Error; 
AK::Monitor::LocalOutputFunc AkMonitor::m_funcLocalOutput = NULL;

AkThread AkMonitor::m_hThread;

bool AkMonitorDataCreator::m_bIsLogging = false;

AkMonitorDataCreator::AkMonitorDataCreator(AkMonitorData::MonitorDataType in_MonitorDataType, AkInt32 in_lSize, bool in_bIsTraceableError)
	: m_pData( NULL )
	, m_lSize( in_lSize )
	, m_bOfflineTracking( in_bIsTraceableError )
{
	AkMonitor * pMonitor = AkMonitor::Get();
	
	// Note: Insufficient memory errors can land here after the monitor manager was term()ed.
	if (!pMonitor)
		return;

	if ( pMonitor->m_sink2Filter.IsEmpty() )
	{
		if (!m_bOfflineTracking)
			return; // Not monitoring, no offline tracking, nothing to do.

		pMonitor->m_ringItems.LockWrite();
		if (pMonitor->m_ringItems.IsEmpty() && !m_bIsLogging)
		{
			m_bIsLogging = true; // Prevent the head message to write a head message.

			AkMonitor::Monitor_TimeStamp(true); // Stub placeholder, time will be re-written at recapitulation.
			AkMonitor::Monitor_AuthoringLog("Some errors occured pre-connection", AK::Monitor::ErrorLevel_Message, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, AK_INVALID_UNIQUE_ID, false, true);

			m_bIsLogging = false;
		}
		pMonitor->m_ringItems.UnlockWrite();

		m_pData = (AkMonitorData::MonitorDataItem *) pMonitor->BeginWrite( m_lSize );
		if (!m_pData)
			return;
	}
	else
	{
		m_bOfflineTracking = false; // We are online, no offline tracking required.

		// Filter out the data types that are not requested.  It is useless to take memory in the queue for something that won't be sent.
		if ((pMonitor->m_uiNotifFilter & AKMONITORDATATYPE_TOMASK(in_MonitorDataType)) == 0)
			return;

		// Retry queuing data until there is enough space. Another thread is emptying the queue
		// and signals MonitorDone when going back to sleep.
		while (!(m_pData = (AkMonitorData::MonitorDataItem *) pMonitor->BeginWrite(m_lSize)))
		{
			AK_IF_PERF_OFFLINE_RENDERING({
				pMonitor->DispatchNotification();
			})
			AK_ELSE_PERF_OFFLINE_RENDERING({
				pMonitor->SignalNotifyEvent();
				AkWaitForEvent(pMonitor->m_hMonitorDoneEvent);
			})
		}
	}

	m_pData->eDataType = in_MonitorDataType;
}

AkMonitorDataCreator::~AkMonitorDataCreator()
{
	if ( !m_pData )
		return;

	AKASSERT( m_pData->eDataType < AkMonitorData::MonitorDataEndOfItems );
	AKASSERT( m_lSize == AkMonitorData::RealSizeof( *m_pData ) );

	AkMonitor * pMonitor = AkMonitor::Get();

	pMonitor->EndWrite(m_pData, m_lSize);	
}
void AkMonitor::ResetOutOfMemoryMsgGate()
{
	AkMonitor * pMonitor = AkMonitor::Get();
	if ( pMonitor )
		pMonitor->m_bMayPostOutOfMemoryMsg = true;
}

void AkMonitor::MonitorAllocFailure(const char * in_szPoolName, size_t in_uFailedAllocSize, AkUInt64 in_uTotalMappedMemory, AkUInt64 in_uMemoryLimit)
{
	AkMonitor * pMonitor = AkMonitor::Get();
	if (pMonitor && pMonitor->m_bMayPostOutOfMemoryMsg)
	{
		pMonitor->m_bMayPostOutOfMemoryMsg = false;

		char szBuffer[AK_MAX_STRING_SIZE];
		static const char* format = "Memory allocation failed: %zu bytes in pool '%s' - currently allocated %zu / %zu bytes";
		snprintf(szBuffer, AK_MAX_STRING_SIZE, format, in_uFailedAllocSize, in_szPoolName, in_uTotalMappedMemory, in_uMemoryLimit);

		MONITOR_ERRORMSG(szBuffer);
	}
}

void AkMonitor::MonitorQueueFull(size_t in_uFailedAllocSize)
{
	AkMonitor * pMonitor = AkMonitor::Get();
	if (pMonitor && pMonitor->m_bMayPostOutOfMemoryMsg)
	{
		pMonitor->m_bMayPostOutOfMemoryMsg = false;

		AkOSChar wszBuffer[AK_MAX_STRING_SIZE];
		static const AkOSChar* format = AKTEXT("Monitor queue full: message size %zu bytes");
		AK_OSPRINTF(wszBuffer, AK_MAX_STRING_SIZE, format, in_uFailedAllocSize);

		MONITOR_ERRORMSG(wszBuffer);
	}
}

AkProfileDataCreator::AkProfileDataCreator( AkInt32 in_lSize )
	: m_pData( NULL )
	, m_lSize( in_lSize )
{
	AkMonitor * pMonitor = AkMonitor::Get();

	if ( pMonitor->m_sink2Filter.IsEmpty() )
		return;

	// Try once to write in the queue.
	m_pData = (AkMonitorData::MonitorDataItem *) pMonitor->BeginWrite( m_lSize );
	if (!m_pData && AK_PERF_OFFLINE_RENDERING)
	{
		pMonitor->DispatchNotification();
		m_pData = (AkMonitorData::MonitorDataItem *) pMonitor->BeginWrite(m_lSize);
	}
}

AkProfileDataCreator::~AkProfileDataCreator()
{
	if ( !m_pData )
	{
		if (m_lSize != 0)
			AkMonitor::MonitorQueueFull(m_lSize);
		return;
	}

	AKASSERT( m_pData->eDataType < AkMonitorData::MonitorDataEndOfItems );
	AKASSERT( m_lSize == AkMonitorData::RealSizeof( *m_pData ) );

	AkMonitor * pMonitor = AkMonitor::Get();

	pMonitor->EndWrite( m_pData, m_lSize );		
}

AkVarLenDataCreator::AkVarLenDataCreator( AkMonitorData::MonitorDataType in_type )
	: m_serializer( AkMonitor::Get()->m_serializer )
	, m_type( in_type )
{
	AkMonitor * pMonitor = AkMonitor::Get();

	if ( pMonitor->m_sink2Filter.IsEmpty() )
		return;

	// Clear out serializer writer
	m_serializer.Clear();

	AKASSERT( m_type < AkMonitorData::MonitorDataEndOfItems );
}

AkVarLenDataCreator::~AkVarLenDataCreator()
{
	Post();
}

void AkVarLenDataCreator::Post()
{
	AkInt32 dataSize = m_serializer.Count();
	if ( dataSize == 0 )
		return;

	AkInt32 itemSize = SIZEOF_MONITORDATA_TO( varLenData.abyData ) + dataSize;

	AkMonitor * pMonitor = AkMonitor::Get();
	AkMonitorData::MonitorDataItem* pItem = (AkMonitorData::MonitorDataItem*)pMonitor->BeginWrite( itemSize );
	AK_IF_PERF_OFFLINE_RENDERING({
		// In offline mode, we empty the buffer and make room for the new item
		while (!pItem)
		{
			pMonitor->DispatchNotification();
			pItem = (AkMonitorData::MonitorDataItem*)pMonitor->BeginWrite(itemSize);
		}
	})

	if ( pItem )
	{
		// write monitor data type (blob)
		pItem->eDataType = m_type;

		// write data size
		pItem->varLenData.dataSize = (AkUInt32)dataSize;

		// write data
		AKPLATFORM::AkMemCpy( (AkUInt8*)pItem->varLenData.abyData, m_serializer.Bytes(), dataSize );

		pMonitor->EndWrite( pItem, itemSize );			
	}
	else
	{
		AkMonitor::MonitorQueueFull(itemSize);
	}
	// Clear out serializer writer
	m_serializer.Clear();
}

void AkVarLenDataCreator::OutOfMemory( int in_iFailedAllocSize )
{
	AkMonitor::MonitorQueueFull(in_iFailedAllocSize);
}

extern char* g_pszCustomPlatformName;
class AkLocalProfilerCaptureSink AK_FINAL
	: public AK::IALMonitorSink
{
public:
	AkLocalProfilerCaptureSink( AK::IAkStdStream * in_pStream )
		: m_pStream( in_pStream )
	{
		CommunicationDefines::ProfilingSessionHeader hdr;

		if ( CommunicationDefines::NeedEndianSwap( CommunicationDefines::g_eConsoleType ) )
		{
			hdr.uVersion = AK::EndianByteSwap::DWordSwap( CommunicationDefines::kProfilingSessionVersion );
			hdr.uProtocol = AK::EndianByteSwap::DWordSwap( AK_COMM_PROTOCOL_VERSION );
			hdr.uConsoleType = AK::EndianByteSwap::DWordSwap( CommunicationDefines::g_eConsoleType );
		}
		else
		{
			hdr.uVersion = CommunicationDefines::kProfilingSessionVersion;
			hdr.uProtocol = AK_COMM_PROTOCOL_VERSION;
			hdr.uConsoleType = CommunicationDefines::g_eConsoleType;
		}

		if (g_pszCustomPlatformName == NULL)
			hdr.CustomConsoleName[0] = 0;
		else 
			strncpy(hdr.CustomConsoleName, g_pszCustomPlatformName, sizeof(hdr.CustomConsoleName));
		
		hdr.uBits = sizeof(AkUIntPtr) * 8;

		AkInt32 cWritten = 0;
		m_serializerOuter.GetWriter()->WriteBytes( (void *) &hdr, (AkUInt32) sizeof( hdr ), cWritten ); // bypass serializer to avoid size chunk being placed

		AkMonitor::Get()->Register( this, AkMonitorData::AllMonitorData );
	}

	~AkLocalProfilerCaptureSink()
	{
		AkMonitor::Get()->Unregister( this );

		if ( m_serializerOuter.GetWrittenSize() )
		{
			AkUInt32 uWritten = 0;
			m_pStream->Write( m_serializerOuter.GetWrittenBytes(), m_serializerOuter.GetWrittenSize(), true, AK_DEFAULT_PRIORITY, 0.0f, uWritten );
		}

		m_pStream->Destroy();
	}

    virtual void MonitorNotification( const AkMonitorData::MonitorDataItem& in_rMonitorItem, bool )
	{
		m_serializerInner.Put( in_rMonitorItem );
		m_serializerOuter.Put( (void *) m_serializerInner.GetWrittenBytes(), (AkUInt32) m_serializerInner.GetWrittenSize() );
	    m_serializerInner.GetWriter()->Clear();
		if ( m_serializerOuter.GetWrittenSize() >= 16384 )
		{
			AkUInt32 uWritten = 0;
			m_pStream->Write( m_serializerOuter.GetWrittenBytes(), (AkUInt32) m_serializerOuter.GetWrittenSize(), true, AK_DEFAULT_PRIORITY, 0.0f, uWritten );
		    m_serializerOuter.GetWriter()->Clear();
		}
	}

	virtual void FlushAccumulated()
	{
	}

private:
	AK::IAkStdStream * m_pStream;				// Profiling session written to this stream
    CommandDataSerializer m_serializerInner;
    CommandDataSerializer m_serializerOuter;
};

AkMonitor::AkMonitor()
	: m_bMayPostOutOfMemoryMsg(true)	
	, m_serializer(false)
	, m_uiNotifFilter(0)
	, m_bMeterWatchesDirty(false)
	, m_pLocalProfilerCaptureSink(NULL)
	
{
	AKASSERT( AkMonitorData::MonitorDataEndOfItems <= 64 ); // If this asserts, MaskType needs to be bigger than 64 bits.
	AkClearThread(&m_hThread);
	AkClearEvent(m_hMonitorEvent);
	AkClearEvent(m_hMonitorDoneEvent);
	m_bStopThread = false;
	m_ringItems.SignalOnWrite(false);
}

AkMonitor::~AkMonitor()
{
	StopMonitoring();
}

AkMonitor* AkMonitor::Instance()
{
    if (!m_pInstance)
    {
        m_pInstance = AkNew(AkMemID_Profiler, AkMonitor());
    }
    return m_pInstance;
}

void AkMonitor::Destroy()
{
	if (m_pInstance)
	{
		AkDelete(AkMemID_Profiler,m_pInstance);
		m_pInstance = NULL;
	}
}

void AkMonitor::Register( AK::IALMonitorSink* in_pMonitorSink, AkMonitorData::MaskType in_whatToMonitor )
{
	CAkFunctionCritical SpaceSetAsCritical;

	// Halt all other writers to the monitoring ring buffer, then flush it.
	// This is necessary so that the registering client does not receive notifications
	// prior to their respective recaps.

	m_ringItems.SignalOnWrite(true);
	m_ringItems.LockWrite();
	while (!m_ringItems.IsEmpty() && m_sink2Filter.Length() != 0)
	{
		AK_IF_PERF_OFFLINE_RENDERING({
			DispatchNotification();
		})
		AK_ELSE_PERF_OFFLINE_RENDERING({
			SignalNotifyEvent();
			AkWaitForEvent(m_hMonitorDoneEvent);
		})
	}

	// At this point, the monitoring thread is starved for notifications and goes back to sleep.

	m_registrationLock.Lock(); // Do all sink2filter operations inside the registration lock.
	
	// Preserve existing sinks, update global notify mask
	AkMonitorData::MaskType eTypesCurrent = 0;
	AkMonitorData::MaskType eTypesGlobal = in_whatToMonitor;
	int cSinks = 0;
	MapStruct<AK::IALMonitorSink*, AkMonitorData::MaskType> aSinksCopy[ AK_MAX_MONITORING_SINKS ];
	{
		for (MonitorSink2Filter::Iterator it = m_sink2Filter.Begin(); it != m_sink2Filter.End(); ++it)
		{
			if ((*it).key == in_pMonitorSink)
			{
				eTypesCurrent = (*it).item;
				(*it).item = in_whatToMonitor;
				if (in_whatToMonitor)
					aSinksCopy[cSinks++] = (*it);
			}
			else
			{
				eTypesGlobal |= (*it).item;
				aSinksCopy[cSinks++] = (*it);
			}
		}
	}

	m_sink2Filter.RemoveAll();

	AkMonitorData::MaskType eTypesRecap = in_whatToMonitor & (~eTypesCurrent);
	if ( eTypesRecap )
	{
		// Register only recapping sink (other sinks must NOT receive recap)
		AkMonitorData::MaskType * pType = m_sink2Filter.Set( in_pMonitorSink );
		*pType = eTypesRecap;
		m_uiNotifFilter = eTypesRecap;

		m_registrationLock.Unlock();

		Recap( eTypesRecap );
		
		// Make sure recap is done before proceeding.
		if (!AK_PERF_OFFLINE_RENDERING)
		{
			while (!m_ringItems.IsEmpty())
			{
				SignalNotifyEvent();
				AkWaitForEvent(m_hMonitorDoneEvent);
			}
		}

		m_registrationLock.Lock();

		*pType = in_whatToMonitor;
	}

	// Reinstate all other sinks
	for (int i = 0; i < cSinks; ++i)
		m_sink2Filter.Set(aSinksCopy[i].key, aSinksCopy[i].item);

	AkMonitorData::MaskType eTypesRemoved = m_uiNotifFilter & (~eTypesGlobal);
	if (eTypesRemoved & (AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataDevicesRecord) | AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataStreamsRecord)))
	{
		// Stop stream profiling if no one listens.
		// (See note near StopStreamProfiler( ) implementation).
		StopStreamProfiler();
	}

	if (eTypesRemoved & AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataParamDelta))
	{
		AkDeltaMonitor::Disable();
	}

	m_uiNotifFilter = eTypesGlobal;

	m_registrationLock.Unlock();

	m_ringItems.UnlockWrite();
}

void AkMonitor::Recap( AkMonitorData::MaskType newMonitorTypes )
{
	if( newMonitorTypes & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataTimeStamp ) )
	{
		// This also means this is a first connection.

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Important notice about :
		//        logging messages accumulated in the message queue while not connected.
		//
		// The message queue is expected to start with a time stamp.
		// If there is none - > add one.
		// If there is one - > Fix the time to "now"
		//   Explanations : 
		//         The "fix to now" is to preserve an existing behavior.
		//         Adding a time that is before the connection time is causing issues in Wwise.
		//         (Wwise computes the time it was connected between the oldest and the newest message 
		//         it received, and if the game has been running for a long time and contained an old error message
		//         it leads to drawing/performance/UX issues.)
		if (!AkMonitor::Get()->m_ringItems.IsEmpty())
		{
			// Peek the first one, if it is a time stamp, fix it to "now".
			AkMonitorData::MonitorDataItem * pItem = (AkMonitorData::MonitorDataItem *) m_ringItems.BeginRead();
			if (pItem && pItem->eDataType == AkMonitorData::MonitorDataTimeStamp)
			{
				pItem->timeStampData.timeStamp = GetThreadTime();
				SignalNotifyEvent();// SignalNotifyEvent only after timeStamp update
			}
			else
		{
				// If this asserts, it means there was content in the message queue but no time stamp preceeding it... You will get these message with no associated time. (Capture log and possibly timeline/graph drawing issues...)
				// Not Fatal, but clearly not ideal.
				AKASSERT(pItem && pItem->eDataType == AkMonitorData::MonitorDataTimeStamp);
				SignalNotifyEvent(); // Before Monitor_TimeStamp, as Monitor_TimeStamp would block if the ring buffer is full and signal was not sent.
				Monitor_TimeStamp();
			}

			Monitor_AuthoringLog("End of pre-connection errors.", AK::Monitor::ErrorLevel_Message);
		}
		else
		{
			Monitor_TimeStamp();
		}
	}

	if (newMonitorTypes & AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataInit))
	{
		Monitor_Init();
	}

	if( newMonitorTypes & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataObjRegistration ) )
	{
		RecapRegisteredObjects();
	}

	if (newMonitorTypes & AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataGameObjPosition))
	{
		RecapRegisteredObjectPositions();
	}

	if( newMonitorTypes & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataSwitch ) )
	{
		RecapSwitches();
	}

	if ( newMonitorTypes & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataMemoryPoolName ) )
	{
		RecapMemoryPools();
	}

	if ( newMonitorTypes & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataSoundBank ) )
	{
		RecapDataSoundBank();
	}

	if ( newMonitorTypes & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataMedia ) )
	{
		RecapMedia();
	}

	if ( newMonitorTypes & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataEvent ) )
	{
		RecapEvents();
	}

	if ( newMonitorTypes & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataGameSync ) )
	{
		RecapGameSync();
	}

    if ( newMonitorTypes & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataDevicesRecord ) )
	{
		// Note: On the stream mgr API, StartMonitoring() means "start listening",
		// which is true only when someone listens. 
        StartStreamProfiler( );

		RecapDevices();
	}

    if ( newMonitorTypes & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataStreamsRecord ) )
	{
        // Note: On the stream mgr API, StartMonitoring() means "start listening",
		// which is true only when someone listens. 
        StartStreamProfiler( );
		RecapStreamRecords();
	}

	if (newMonitorTypes & AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataParamDelta))
	{
		AkDeltaMonitor::Enable();
		RecapParamDelta();	
	}

	_CallGlobalExtensions(AkGlobalCallbackLocation_MonitorRecap);
}

void AkMonitor::Unregister( AK::IALMonitorSink* in_pMonitorSink )
{
	CAkFunctionCritical SpaceSetAsCritical;

	AkAutoLock<CAkLock> gate( m_registrationLock );

	m_sink2Filter.Unset( in_pMonitorSink );

	m_uiNotifFilter = 0;
	for( MonitorSink2Filter::Iterator it = m_sink2Filter.Begin(); it != m_sink2Filter.End(); ++it )
		m_uiNotifFilter |= (*it).item;

	if ( !( m_uiNotifFilter & ( AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataDevicesRecord) | AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataStreamsRecord) ) ) )
	{
		// Stop stream profiling if no one listens.
		// (See note near StopStreamProfiler( ) implementation).
		StopStreamProfiler();
	}

	if (m_sink2Filter.IsEmpty())
		m_ringItems.SignalOnWrite(false);
}

AkUInt8 AkMonitor::GetMeterWatchDataTypes( AkUniqueID in_busID )
{
	AkUInt8* pMask = m_meterWatchMap.Exists( in_busID );

	if( pMask )
		return *pMask;

	return 0;
}

void AkMonitor::SetMeterWatches( AkMonitorData::MeterWatch* in_pWatches, AkUInt32 in_uiWatchCount )
{
	m_meterWatchMap.RemoveAll();

	for( AkUInt32 i = 0; i < in_uiWatchCount; ++i )
	{
		AkUniqueID busID = in_pWatches[i].busID;
		
		AkUInt8 * pWatch = m_meterWatchMap.Set(busID);
		if ( pWatch ) // properly handle out-of-memory
			*pWatch = in_pWatches[i].uBusMeterDataTypes;
	}

	m_bMeterWatchesDirty = true;
}

void AkMonitor::RecapRegisteredObjects()
{
	CAkRegistryMgr::AkMapRegisteredObj& regObjects = g_pRegistryMgr->GetRegisteredObjectList();
	for (CAkRegistryMgr::AkMapRegisteredObj::Iterator it = regObjects.Begin(); it != regObjects.End(); ++it)
	{
		AkGameObjectID gameObjID = (*it).key;
		AkIDStringHash::AkStringHash::IteratorEx Iterator = g_idxGameObjectString.m_list.FindEx(gameObjID);
		CAkRegisteredObj* pObj = (*it).item;
		Monitor_ObjectRegistration(AkMonitorData::MakeObjRegFlags(true, pObj->IsSoloExplicit(), pObj->IsSoloImplicit(), pObj->IsMute(), true), gameObjID, Iterator.pItem, true);
	}
}

void RecapSwitches_ForEach( AkRTPCValue& in_val, const AkRTPCKey& in_rtpcKey, void* in_cookie )
{
	AkMonitor::Monitor_SwitchChanged(
			*reinterpret_cast<AkSwitchGroupID*>( in_cookie ),
			*reinterpret_cast<AkSwitchStateID*>( &in_val.uValue ),
			in_rtpcKey.GameObj() ? in_rtpcKey.GameObj()->ID() : AK_INVALID_GAME_OBJECT );
}

void AkMonitor::RecapSwitches()
{
	AKASSERT( g_pSwitchMgr );
	for( CAkSwitchMgr::AkMapSwitchEntries::Iterator iter = g_pSwitchMgr->m_mapEntries.Begin(); iter != g_pSwitchMgr->m_mapEntries.End(); ++iter )
	{
		CAkSwitchMgr::AkSwitchEntry* pEntry = *iter;
		if( pEntry )
			pEntry->values.ForEach( RecapSwitches_ForEach, reinterpret_cast<void*>( &pEntry->key ) );
	}
}

namespace AK
{
	namespace MemoryMgr
	{
		extern char const* g_aPoolNames[AkMemID_NUM];
	}
}

void AkMonitor::RecapMemoryPools()
{
	for (AkUInt32 ulPool = 0; ulPool < AkMemID_NUM; ++ulPool)
	{
		Monitor_SetPoolName(ulPool, AK::MemoryMgr::g_aPoolNames[ulPool]);
	}

	for (AkUInt32 ulPool = AkMemID_NUM; ulPool < AkMemID_NUM * 2; ++ulPool)
	{
		char szName[256];
		strcpy(szName, AK::MemoryMgr::g_aPoolNames[ulPool - AkMemID_NUM]);
		strcat(szName, " (device)");

		Monitor_SetPoolName(ulPool, szName);
	}
}

void AkMonitor::RecapDataSoundBank()
{
	CAkBankList& rMainBankList = g_pBankManager->GetBankListRef();
	AkAutoLock<CAkBankList> BankListGate( rMainBankList );
	CAkBankList::AkListLoadedBanks& rBankList = rMainBankList.GetUNSAFEBankListRef();
	for( CAkBankList::AkListLoadedBanks::Iterator iter = rBankList.Begin(); iter != rBankList.End(); ++iter )
		Monitor_LoadedBank(iter.pItem, false);
}

void AkMonitor::RecapMedia()
{
	AkAutoLock<CAkLock> gate( g_pBankManager->m_MediaLock );

	CAkBankMgr::AkMediaHashTable::Iterator iter = g_pBankManager->m_MediaHashTable.Begin();
	while( iter != g_pBankManager->m_MediaHashTable.End() )
	{
		Monitor_MediaPrepared((*iter).item);
		++iter;
	}
}

void AkMonitor::RecapEvents()
{
	AKASSERT( g_pIndex );
	CAkIndexItem<CAkEvent*>& l_rIdx = g_pIndex->m_idxEvents;
	AkAutoLock<CAkLock> IndexLock( g_pIndex->m_idxEvents.GetLock() );

	CAkIndexItem<CAkEvent*>::AkMapIDToPtr::Iterator iter = l_rIdx.m_mapIDToPtr.Begin();
	while( iter != l_rIdx.m_mapIDToPtr.End() )
	{
		Monitor_EventPrepared((*iter)->ID(), static_cast<CAkEvent*>(*iter)->GetPreparationCount());
		++iter;
	}
}

void AkMonitor::RecapGroupHelper(CAkStateMgr::PreparationGroups& in_Groups, AkGroupType in_Type )
{
	CAkStateMgr::PreparationGroups::Iterator iter = in_Groups.Begin();
	while( iter != in_Groups.End() )
	{
		CAkStateMgr::PreparationStateItem* pItem = iter.pItem;

		// We have a group, we will then have to iterate trough each game sync.
		CAkPreparedContent::ContentList& rContentList = pItem->GetPreparedcontent()->m_PreparableContentList;
		CAkPreparedContent::ContentList::Iterator subIter = rContentList.Begin();
		while( subIter != rContentList.End() )
		{
			Monitor_GameSync(pItem->GroupID(), *(subIter.pItem), true, in_Type);
			++subIter;
		}
		++iter;
	}
}

void AkMonitor::RecapGameSync()
{
	if( g_pStateMgr )
	{
		AkAutoLock<CAkLock> gate( g_pStateMgr->m_PrepareGameSyncLock );// to pad monitoring recaps.
		RecapGroupHelper( g_pStateMgr->m_PreparationGroupsStates, AkGroupType_State );
		RecapGroupHelper( g_pStateMgr->m_PreparationGroupsSwitches, AkGroupType_Switch );
	}
}

void AkMonitor::RecapDevices()
{
    AKASSERT( AK::IAkStreamMgr::Get( ) );
    AK::IAkStreamMgrProfile * pStmMgrProfile = AK::IAkStreamMgr::Get( )->GetStreamMgrProfile( );
    if ( !pStmMgrProfile )
        return; // Profiling interface not implemented in that stream manager.

    // Get all devices.
    AkUInt32 ulNumDevices = pStmMgrProfile->GetNumDevices( );
	for ( AkUInt32 ulDevice = 0; ulDevice < ulNumDevices; ++ulDevice )
	{
        AK::IAkDeviceProfile * pDevice = pStmMgrProfile->GetDeviceProfile( ulDevice );
        AKASSERT( pDevice );

		AkMonitorDataCreator creator( AkMonitorData::MonitorDataDevicesRecord, SIZEOF_MONITORDATA( deviceRecordData ) );
		if ( !creator.m_pData )
			return;

		pDevice->GetDesc( creator.m_pData->deviceRecordData );
        pDevice->ClearNew( );   // Whether it was new or not, it is not anymore.
	}
}

void AkMonitor::RecapStreamRecords()
{
    AKASSERT( AK::IAkStreamMgr::Get( ) );
    AK::IAkStreamMgrProfile * pStmMgrProfile = AK::IAkStreamMgr::Get( )->GetStreamMgrProfile( );
    if ( !pStmMgrProfile )
        return; // Profiling interface not implemented in that stream manager.

    // Get all stream records, for all devices.
    AkUInt32 ulNumDevices = pStmMgrProfile->GetNumDevices( );
    IAkDeviceProfile * pDevice;
    for ( AkUInt32 ulDevice=0; ulDevice<ulNumDevices; ulDevice++ )
    {
        pDevice = pStmMgrProfile->GetDeviceProfile( ulDevice );
        AKASSERT( pDevice != NULL );

		pDevice->OnProfileStart();

        AkUInt32 ulNumStreams = pDevice->GetNumStreams( );
        AK::IAkStreamProfile * pStream;
	    for ( AkUInt32 ulStream = 0; ulStream < ulNumStreams; ++ulStream )
	    {
			AkMonitorDataCreator creator( AkMonitorData::MonitorDataStreamsRecord, SIZEOF_MONITORDATA( streamRecordData ) );
			if ( !creator.m_pData )
			{
				pDevice->OnProfileEnd();
				return;
			}

			creator.m_pData->streamRecordData.ulNumNewRecords = 1;
            pStream = pDevice->GetStreamProfile( ulStream );
            AKASSERT( pStream );

            pStream->GetStreamRecord( *creator.m_pData->streamRecordData.streamRecords );
            pStream->ClearNew( );   // Whether it was new or not, it is not anymore.		
	    }

		pDevice->OnProfileEnd();
    }
}

// Note: On the stream mgr API, StartMonitoring() means "start listening",
// which is false unless someone listens. It could be NOT the case even if 
// AkMonitor::StartMonitoring() had been called (see AkMonitor::IsMonitoring()
// implementation).
void AkMonitor::StartStreamProfiler( )
{
    AKASSERT( AK::IAkStreamMgr::Get( ) );
    if ( AK::IAkStreamMgr::Get( )->GetStreamMgrProfile( ) )
        AK::IAkStreamMgr::Get( )->GetStreamMgrProfile( )->StartMonitoring( );
}

void AkMonitor::StopStreamProfiler( )
{
    AKASSERT( AK::IAkStreamMgr::Get( ) );
    if ( AK::IAkStreamMgr::Get( )->GetStreamMgrProfile( ) )
    	AK::IAkStreamMgr::Get( )->GetStreamMgrProfile( )->StopMonitoring( );
}

AKRESULT AkMonitor::StartProfilerCapture( const AkOSChar* in_szFilename )
{
	if ( m_pLocalProfilerCaptureSink )
		return AK_AlreadyConnected;

	AK::IAkStdStream * pStream = NULL;
	
	AkFileSystemFlags fsFlags;
	fsFlags.uCompanyID = AKCOMPANYID_AUDIOKINETIC;
	fsFlags.uCodecID = AKCODECID_PROFILERCAPTURE;
	fsFlags.bIsLanguageSpecific = false;
	fsFlags.pCustomParam = NULL;
	fsFlags.uCustomParamSize = 0;

	AKRESULT eResult = AK::IAkStreamMgr::Get()->CreateStd( in_szFilename, &fsFlags, AK_OpenModeWriteOvrwr, pStream, true );
	if( eResult != AK_Success )
		return eResult;

	pStream->SetStreamName( in_szFilename );

	m_pLocalProfilerCaptureSink = AkNew( AkMemID_Profiler, AkLocalProfilerCaptureSink( pStream ) );
	if ( !m_pLocalProfilerCaptureSink )
	{
		pStream->Destroy();
		return AK_InsufficientMemory;
	}

	return AK_Success;
}

AKRESULT AkMonitor::StopProfilerCapture()
{
	if ( !m_pLocalProfilerCaptureSink )
		return AK_Fail;

	AkDelete( AkMemID_Profiler, m_pLocalProfilerCaptureSink );
	m_pLocalProfilerCaptureSink = NULL;

	return AK_Success;
}

AKRESULT AkMonitor::StartMonitoring()
{
	if (AkIsValidThread(&m_hThread))
		return AK_Success;

	AkUInt32 uQueuePoolSize = g_settings.uMonitorQueuePoolSize ? g_settings.uMonitorQueuePoolSize : MONITOR_QUEUE_DEFAULT_SIZE;
	AKRESULT res = m_ringItems.Init( AkMemID_MonitorQueue, uQueuePoolSize );
	if ( res != AK_Success )
		return AK_Fail;

	if (m_sink2Filter.Reserve(AK_MAX_MONITORING_SINKS) != AK_Success)
		return AK_Fail;

	m_serializer.SetMemPool(AkMemID_Profiler);
	res = AkDeltaMonitor::Init();
	if (res != AK_Success)
		return AK_InsufficientMemory;

	m_bStopThread = false;
	if (AkCreateEvent(m_hMonitorEvent) != AK_Success
		|| AkCreateEvent(m_hMonitorDoneEvent) != AK_Success)
		return AK_Fail;

	// In offline mode we process de monitor data in the sound engine thread thus we start the m_hThread only if we are not in offline mode.
	if (!AK_PERF_OFFLINE_RENDERING)
	{
		AkCreateThread(MonitorThreadFunc,    // Start address
			this,                 // Parameter
			g_PDSettings.threadMonitor,
			&m_hThread,
			"AK::Monitor");	  // Handle
		if (!AkIsValidThread(&m_hThread))
		{
			AKASSERT(!"Could not create monitor thread");
			return AK_Fail;
		}
	}
	
	return AK_Success;
}

void AkMonitor::StopMonitoring()
{
	if (AkIsValidThread(&m_hThread))
	{
		m_bStopThread = true;
		AkSignalEvent(m_hMonitorEvent);		
		AkWaitForSingleThread(&m_hThread);

		AkCloseThread(&m_hThread);	
	}
	
	AkDestroyEvent(m_hMonitorEvent);
	AkDestroyEvent(m_hMonitorDoneEvent);
	
	StopProfilerCapture();
    StopStreamProfiler( );

	AkDeltaMonitor::Term();

	m_sink2Filter.Term();
	g_idxGameObjectString.Term();
	m_mapGameObjectChanged.Term();
	m_meterWatchMap.Term();
	m_serializer.SetMemPool(AK_INVALID_POOL_ID);
	m_ringItems.Term(AkMemID_MonitorQueue);
}

namespace {

	typedef AkArray<AkMonitorData::GameObjPosition, const AkMonitorData::GameObjPosition&, AkMonitor::MonitorPoolDefault> GameObjPositionArray;

	AkMonitorData::GameObjPosition* FindPos(const AkTransform& in_xfrm, GameObjPositionArray& in_array, AkUInt32 in_startAt)
	{
		if (in_startAt < in_array.Length())
		{
			AkMonitorData::GameObjPosition* pPos = &in_array[in_startAt];
			AkMonitorData::GameObjPosition* pEnd = in_array.End().pItem;
			while (pPos < pEnd)
			{
				if (AkMath::CompareAkTransform(in_xfrm, pPos->emitter.position))
					return pPos;

				++pPos;
			}
		}
		return NULL;
	}
}

void AkMonitor::AddChangedGameObject(AkGameObjectID in_gameObject)
{
	m_mapGameObjectChanged.Set(in_gameObject);
}

void AkMonitor::RecapRegisteredObjectPositions()
{
	CAkRegistryMgr::AkMapRegisteredObj& regObjects = g_pRegistryMgr->GetRegisteredObjectList();
	for (CAkRegistryMgr::AkMapRegisteredObj::Iterator it = regObjects.Begin(); it != regObjects.End(); ++it)
	{
		AkMonitor::AddChangedGameObject((*it).key);
	}
	AkMonitor::PostChangedGameObjPositions();
}

void AkMonitor::PostChangedGameObjPositions()
{
	GameObjPositionArray tempPositionArray;
	AkListenerSet associatedListeners;

	tempPositionArray.Reserve(m_mapGameObjectChanged.Length() * 4); // Just a guess...

	for (AkMapGameObjectProfile::Iterator iter = m_mapGameObjectChanged.Begin(); iter != m_mapGameObjectChanged.End(); ++iter)
	{
		AkGameObjectID gameObjectId = (*iter).key;

		// Global watches don't have positions. don't track them.
		if (gameObjectId == AK_INVALID_GAME_OBJECT)
			continue;

		CAkRegisteredObj* pObj = g_pRegistryMgr->GetObjAndAddref(gameObjectId);
		if (pObj != NULL)
		{
			AkUInt32 firstPosIdx = tempPositionArray.Length();

			CAkEmitter* pEmitter = pObj->GetComponent<CAkEmitter>();
			if (pEmitter != NULL)
			{
				AkUnion(associatedListeners, pObj->GetListeners());

				AkPositionStore& ps = pEmitter->GetPosition();
				AkVolumeDataArray* pDiffractionRays = NULL;
				CAkSpatialAudioComponent* pSACmpt = pObj->GetComponent<CAkSpatialAudioComponent>();
				if (pSACmpt != NULL && !pSACmpt->GetDiffractionRays().IsEmpty())
					pDiffractionRays = &(pSACmpt->GetDiffractionRays());

				const AkChannelEmitter * positions = ps.GetPositions();

				for (AkUInt32 iPos = 0; iPos < ps.GetNumPosition(); ++iPos)
				{
					AkMonitorData::GameObjPosition* pGameObjPos = tempPositionArray.AddLast();
					if (pGameObjPos)
					{
						pGameObjPos->gameObjID = gameObjectId;
						pGameObjPos->emitter = positions[iPos];
						pGameObjPos->posIndex = (AkInt16)(ps.GetNumPosition() <= 1 ? -1 : iPos);
						pGameObjPos->posType = (AkInt16)AkMonitorData::GameObjPosition::Type_EmitterPos;
					}
				}

				CAkListener* pListener = pObj->GetComponent<CAkListener>();
				if (pListener)
				{
					AkMonitorData::GameObjPosition* pGameObjPos = FindPos(pListener->GetTransform(), tempPositionArray, firstPosIdx);
					if (pGameObjPos == NULL)
					{
						pGameObjPos = tempPositionArray.AddLast();
						if (pGameObjPos != NULL)
						{
							pGameObjPos->gameObjID = gameObjectId;
							pGameObjPos->emitter.position = pListener->GetTransform();
							pGameObjPos->emitter.uInputChannels = (AkUInt32)-1;
							pGameObjPos->posIndex = -1;
							pGameObjPos->posType = AkMonitorData::GameObjPosition::Type_ListenerPos;
						}
					}
					else
					{
						pGameObjPos->posType |= AkMonitorData::GameObjPosition::Type_ListenerPos;
					}
				}

				AkMonitorData::GameObjPosition* pFirstCustomRayGameObjPos = NULL;
				AkInt16 customRayPosIdx = 0;
				const CAkConnectedListeners& connectedListener = pObj->GetConnectedListeners();

				if (pDiffractionRays != NULL)
				{
					for (AkUInt32 i = 0; i < pDiffractionRays->Length(); ++i)
					{
						const AkRayVolumeData& dray = (*pDiffractionRays)[i];

						if (connectedListener.GetUserAssocs().GetListeners().Contains(dray.ListenerID()))
						{
							// Only send custom rays positions that reference listeners that are user-assigned, direct (as opposed to aux-send) listeners.

							AkMonitorData::GameObjPosition* pGameObjPos = FindPos(dray.emitter, tempPositionArray, firstPosIdx);
							if (pGameObjPos == NULL)
							{
								pGameObjPos = tempPositionArray.AddLast();
								if (pGameObjPos != NULL)
								{
									pGameObjPos->gameObjID = gameObjectId;
									pGameObjPos->emitter.position = dray.emitter;
									pGameObjPos->emitter.uInputChannels = dray.uEmitterChannelMask;
									pGameObjPos->posIndex = customRayPosIdx++;
									pGameObjPos->posType = (AkInt16)AkMonitorData::GameObjPosition::Type_VirtualPos;
								}
							}
							else
							{
								if (pGameObjPos->posIndex == -1) // an index was not assigned
									pGameObjPos->posIndex = customRayPosIdx++;
								pGameObjPos->posType |= AkMonitorData::GameObjPosition::Type_VirtualPos;
							}

							// Keep track of unique listeners that are referenced by the watched objects, we want to send their positions too.
							associatedListeners.Set(dray.ListenerID());

							if (pFirstCustomRayGameObjPos == NULL)
								pFirstCustomRayGameObjPos = pGameObjPos;
						}
					}
				}

				if (customRayPosIdx == 1 && pFirstCustomRayGameObjPos != NULL)
				{
					pFirstCustomRayGameObjPos->posIndex = -1; //don't show index if there is only one.
				}

			}

			pObj->Release();
		}
	}

	for (AkListenerSet::Iterator it = associatedListeners.Begin(); it != associatedListeners.End(); ++it)
	{
		AkGameObjectID gameObjID = *it;
		if (!m_mapGameObjectChanged.Exists(gameObjID)) // if it is watched, then the position data was inserted above.
		{
			const CAkListener* pListener = CAkListener::GetListenerData(gameObjID);
			if (pListener != NULL)
			{
				AkMonitorData::GameObjPosition* pGameObjPos = tempPositionArray.AddLast();
				if (pGameObjPos)
				{
					pGameObjPos->gameObjID = gameObjID;
					pGameObjPos->emitter.position = pListener->GetTransform();
					pGameObjPos->emitter.uInputChannels = 0; //not applicable for listeners.
					pGameObjPos->posIndex = -1;
					pGameObjPos->posType = (AkInt16)AkMonitorData::GameObjPosition::Type_ListenerPos;
				}
			}
		}
	}

	AkUInt32 uNumObjs = tempPositionArray.Length();

	if (uNumObjs)
	{
		AkInt32 sizeofItem = SIZEOF_MONITORDATA_TO(gameObjPositionData.positions)
			+ uNumObjs * sizeof(AkMonitorData::GameObjPosition);

		AkProfileDataCreator creator(sizeofItem);
		if (creator.m_pData)
		{
			creator.m_pData->eDataType = AkMonitorData::MonitorDataGameObjPosition;
			creator.m_pData->gameObjPositionData.ulNumGameObjPositions = uNumObjs;
			AkMemCpy(creator.m_pData->gameObjPositionData.positions, tempPositionArray.Data(), uNumObjs * sizeof(AkMonitorData::GameObjPosition));
		}
	}

	associatedListeners.Term();
	tempPositionArray.Term();
	m_mapGameObjectChanged.RemoveAll();
}


bool AkMonitor::DispatchNotification()
{
	WWISE_SCOPED_PROFILE_MARKER( "AkMonitor::DispatchNotification" );

	bool bReturnVal = false;

	// First keep a local copy of the sinks -- this is to prevent problems with sinks registering from other threads
	// while we notify.

	m_registrationLock.Lock();

	// Allocate copy on the stack ( CAkKeyList copy was identified as a bottleneck here )
	int cSinks = m_sink2Filter.Length();
	MapStruct<AK::IALMonitorSink*, AkMonitorData::MaskType> aSinksCopy[ AK_MAX_MONITORING_SINKS ];
	{
		int i = 0;
		for( MonitorSink2Filter::Iterator it = m_sink2Filter.Begin(); it != m_sink2Filter.End(); ++it )
			aSinksCopy[ i++ ] = (*it);
	}

	// Next, send a maximum of MAX_NOTIFICATIONS_PER_CALL notifications in the queue
	int iNotifCount = MAX_NOTIFICATIONS_PER_CALL;
	while ( !m_ringItems.IsEmpty() )
	{
		if (!AK_PERF_OFFLINE_RENDERING)
		{
			if (--iNotifCount == 0)
			{
				bReturnVal = true; //remaining items to notify
				break; //exit while loop
			}
		}
		AkMonitorData::MonitorDataItem * pItem = (AkMonitorData::MonitorDataItem * ) m_ringItems.BeginRead();
		AKASSERT( pItem->eDataType < AkMonitorData::MonitorDataEndOfItems );

		AkMonitorData::MaskType uMask = AKMONITORDATATYPE_TOMASK( pItem->eDataType ) ;
		for ( int i = 0; i < cSinks; ++i )
		{
			if ( aSinksCopy[ i ].item & uMask ) //Apply filter for this sink
				aSinksCopy[ i ].key->MonitorNotification( *pItem, true/*Accumulate mode ON*/ );
		}

		m_ringItems.EndRead( pItem, AkMonitorData::RealSizeof( *pItem ) );
	}
	AK_IF_PERF_OFFLINE_RENDERING({
		m_ringItems.Reset();
	})

	AkSignalEvent( m_hMonitorDoneEvent );

	for ( int i = 0; i < cSinks; ++i )
	{
		aSinksCopy[ i ].key->FlushAccumulated();
	}

	m_registrationLock.Unlock();

	return bReturnVal;
}

AK_DECLARE_THREAD_ROUTINE( AkMonitor::MonitorThreadFunc )
{	
	AK_THREAD_INIT_CODE(g_PDSettings.threadMonitor);
	AkMonitor& rThis = *AK_GET_THREAD_ROUTINE_PARAMETER_PTR(AkMonitor);	

	AK_INSTRUMENT_THREAD_START( "AkMonitor::MonitorThreadFunc" );

	while( true )
	{
		if (rThis.m_ringItems.IsEmpty())
			AkWaitForEvent( rThis.m_hMonitorEvent );

		if ( rThis.m_bStopThread )
		{
			// Stop event.  Bail out.
			break;
		}

		// Something in the queue!
		while( rThis.DispatchNotification() ) {} //loop until there are no more notifications
	}

	AK::MemoryMgr::TermForThread();
	AkExitThread(AK_RETURN_THREAD_OK);
}

void AkMonitor::Monitor_PostCode( 
	AK::Monitor::ErrorCode in_eErrorCode, 
	AK::Monitor::ErrorLevel in_eErrorLevel, 
	AkPlayingID in_playingID,
	AkGameObjectID in_gameObjID,
	AkUniqueID in_soundID,
	bool in_bIsBus)
{
	if ( in_eErrorCode >= 0 && in_eErrorCode < AK::Monitor::Num_ErrorCodes && ( m_uLocalOutputErrorLevel & in_eErrorLevel ) )
	{
		Monitor_PostToLocalOutput( in_eErrorCode, in_eErrorLevel, AK::Monitor::s_aszErrorCodes[ in_eErrorCode ], in_playingID, in_gameObjID );
	}

	Monitor_SendErrorData1( in_eErrorCode, in_eErrorLevel, 0, in_playingID, in_gameObjID, in_soundID, in_bIsBus );
}

void AkMonitor::Monitor_PostCodeWithParam( 
	AK::Monitor::ErrorCode in_eErrorCode, 
	AK::Monitor::ErrorLevel in_eErrorLevel, 
	AkUInt32 in_param1,
	AkPlayingID in_playingID,
	AkGameObjectID in_gameObjID,
	AkUniqueID in_soundID,
	bool in_bIsBus)
{
	if ( in_eErrorCode >= 0 && in_eErrorCode < AK::Monitor::Num_ErrorCodes && ( m_uLocalOutputErrorLevel & in_eErrorLevel ) )
	{
		const AkOSChar* pszError = AK::Monitor::s_aszErrorCodes[ in_eErrorCode ];

		const size_t k_uMaxStrLen = 128;
		AkOSChar szMsg[ k_uMaxStrLen ];
		AKPLATFORM::SafeStrCpy( szMsg, pszError, k_uMaxStrLen );
		if ( in_param1 != AK_INVALID_UNIQUE_ID )
		{
			const size_t k_uMaxIDStrLen = 16;
			AkOSChar szID[ k_uMaxIDStrLen ];
			AK_OSPRINTF( szID, k_uMaxIDStrLen, AKTEXT(": %u"), in_param1 );
			AKPLATFORM::SafeStrCat( szMsg, szID, k_uMaxStrLen );
		}

		Monitor_PostToLocalOutput( in_eErrorCode, in_eErrorLevel, szMsg, in_playingID, in_gameObjID );
	}

	Monitor_SendErrorData1( in_eErrorCode, in_eErrorLevel, in_param1, in_playingID, in_gameObjID, in_soundID, in_bIsBus );
}

void AkMonitor::Monitor_PostPluginError( 
	const char* in_szMsg, 
	AK::Monitor::ErrorLevel in_eErrorLevel, 
	AkPluginID in_pluginTypeID,
	AkUniqueID in_pluginUniqueID,
	AkPlayingID in_playingID,
	AkGameObjectID in_gameObjID,
	AkUniqueID in_soundID,
	bool in_bIsBus)
{
	if( ! in_szMsg || in_pluginTypeID == AK_INVALID_PLUGINID )
		return;

	if ( m_uLocalOutputErrorLevel & in_eErrorLevel )
	{
		const size_t k_uMaxStrLen = 128;
		char szMsg[ k_uMaxStrLen ];
		AKPLATFORM::SafeStrCpy( szMsg, in_szMsg, k_uMaxStrLen );

		const size_t k_uMaxIDStrLen = 16;
		char szID[ k_uMaxIDStrLen ];
		sprintf( szID, ": %u", in_pluginTypeID );
		AKPLATFORM::SafeStrCat( szMsg, szID, k_uMaxStrLen );

		if ( m_funcLocalOutput )
		{
#ifdef AK_OS_WCHAR
			wchar_t wszMsg[ AK_MAX_STRING_SIZE ]; 
			AkUtf8ToWideChar( szMsg, AK_MAX_STRING_SIZE, wszMsg );
			wszMsg[ AK_MAX_STRING_SIZE - 1 ] = 0;
			m_funcLocalOutput( Monitor::ErrorCode_NoError, wszMsg, in_eErrorLevel, in_playingID, in_gameObjID );
#else
			m_funcLocalOutput( Monitor::ErrorCode_NoError, szMsg, in_eErrorLevel, in_playingID, in_gameObjID );
#endif
		}
		else
		{
			AKPLATFORM::OutputDebugMsg( in_eErrorLevel == AK::Monitor::ErrorLevel_Message ? AKTEXT("AK Message: ") : AKTEXT("AK Error: ") );
			AKPLATFORM::OutputDebugMsg( szMsg );
			AKPLATFORM::OutputDebugMsg( AKTEXT("\n") );
		}
	}

	AkUInt16 wStringSize = (AkUInt16) strlen(in_szMsg) + 1;
	AkMonitorDataCreator creator( 
		in_eErrorLevel == AK::Monitor::ErrorLevel_Message ? AkMonitorData::MonitorDataPluginMessage : AkMonitorData::MonitorDataPluginError, 
		SIZEOF_MONITORDATA_TO( pluginMonitorError.szMessage ) + wStringSize * sizeof( AkUtf16 ), 
		true);
	if ( !creator.m_pData )
		return;

	creator.m_pData->pluginMonitorError.pluginTypeID = in_pluginTypeID;
	creator.m_pData->pluginMonitorError.pluginUniqueID = in_pluginUniqueID;
	creator.m_pData->pluginMonitorError.playingID = in_playingID;
	creator.m_pData->pluginMonitorError.gameObjID = in_gameObjID;
	creator.m_pData->pluginMonitorError.soundID.id = in_soundID;
	creator.m_pData->pluginMonitorError.soundID.bIsBus = in_bIsBus;

	creator.m_pData->pluginMonitorError.wStringSize = wStringSize;
	AK_CHAR_TO_UTF16( creator.m_pData->pluginMonitorError.szMessage, in_szMsg, wStringSize );
}

inline bool IsErrorTraceable( AK::Monitor::ErrorCode in_eErrorCode )
{
	// List error codes we do not want to track while Wwise Authoring is not connected.

	// For Usability reason, we do not track Voice starvations errors.
	return in_eErrorCode != AK::Monitor::ErrorCode_VoiceStarving; 
}

inline bool IsMessageTraceable( AK::Monitor::ErrorLevel in_eErrorLevel )
{
	// For Usability reason, we do not track Messages type that were not flagged as errors.
	return in_eErrorLevel != AK::Monitor::ErrorLevel_Message;
}

void AkMonitor::Monitor_SendErrorData1( 
	AK::Monitor::ErrorCode in_eErrorCode, 
	AK::Monitor::ErrorLevel in_eErrorLevel, 
	AkUInt32 in_param1,
	AkPlayingID in_playingID,
	AkGameObjectID in_gameObjID,
	AkUniqueID in_soundID,
	bool in_bIsBus )
{
	AkMonitorDataCreator creator( 
		in_eErrorLevel == AK::Monitor::ErrorLevel_Message ? AkMonitorData::MonitorDataMessageCode : AkMonitorData::MonitorDataErrorCode, 
		SIZEOF_MONITORDATA( errorData1 ),
		IsErrorTraceable( in_eErrorCode )
		); 

	if ( !creator.m_pData )
		return;

	creator.m_pData->errorData1.playingID = in_playingID;
	creator.m_pData->errorData1.gameObjID = in_gameObjID;
	creator.m_pData->errorData1.eErrorCode = in_eErrorCode;
	creator.m_pData->errorData1.uParam1 = in_param1;
	creator.m_pData->errorData1.soundID.id = in_soundID;
	creator.m_pData->errorData1.soundID.bIsBus = in_bIsBus;
}

void AkMonitor::Monitor_PostToLocalOutput( 
	AK::Monitor::ErrorCode in_eErrorCode, 
	AK::Monitor::ErrorLevel in_eErrorLevel, 
	const AkOSChar * in_pszError,
	AkPlayingID in_playingID,
	AkGameObjectID in_gameObjID )
{
	if ( m_funcLocalOutput )
	{
		m_funcLocalOutput( in_eErrorCode, in_pszError, in_eErrorLevel, in_playingID, in_gameObjID );
	}
	else
	{
		AKPLATFORM::OutputDebugMsg( in_eErrorLevel == AK::Monitor::ErrorLevel_Message ? AKTEXT("AK Message: ") : AKTEXT("AK Error: ") );
		AKPLATFORM::OutputDebugMsg( in_pszError );
		AKPLATFORM::OutputDebugMsg( AKTEXT("\n") );
	}
}

#ifdef AK_SUPPORT_WCHAR
void AkMonitor::Monitor_PostString(const wchar_t* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel, AkPlayingID in_playingID, AkGameObjectID in_gameObjID, AkUniqueID in_soundID, bool in_bIsBus)
{
	if ( in_pszError )
	{
		if ( m_uLocalOutputErrorLevel & in_eErrorLevel )
		{
			if ( m_funcLocalOutput )
			{
#ifdef AK_OS_WCHAR
				m_funcLocalOutput(Monitor::ErrorCode_NoError, in_pszError, in_eErrorLevel, in_playingID, in_gameObjID);
#else
				char szError[ AK_MAX_STRING_SIZE ]; 
				AkWideCharToChar( in_pszError, AK_MAX_STRING_SIZE, szError );
				m_funcLocalOutput( Monitor::ErrorCode_NoError, szError, in_eErrorLevel, in_playingID, in_gameObjID );
#endif
			}
			else
			{
				AKPLATFORM::OutputDebugMsg( in_eErrorLevel == AK::Monitor::ErrorLevel_Message ? L"AK Message: " : L"AK Error: " );
				AKPLATFORM::OutputDebugMsg( in_pszError );
				AKPLATFORM::OutputDebugMsg( L"\n" );
			}
		}

		Monitor_AuthoringLog(in_pszError, in_eErrorLevel, in_playingID, in_gameObjID, in_soundID, in_bIsBus);
	}
}

void AkMonitor::Monitor_AuthoringLog(const wchar_t* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel, AkPlayingID in_playingID, AkGameObjectID in_gameObjID, AkUniqueID in_soundID, bool in_bIsBus, bool in_bForceLog)
{
	AkUInt16 wStringSize = (AkUInt16)wcslen(in_pszError) + 1;
	AkMonitorDataCreator creator(
		in_eErrorLevel == AK::Monitor::ErrorLevel_Message ? AkMonitorData::MonitorDataMessageString : AkMonitorData::MonitorDataErrorString,
		offsetof(AkMonitorData::MonitorDataItem, debugData.szMessage) + wStringSize * sizeof(AkUtf16),
		in_bForceLog || IsMessageTraceable(in_eErrorLevel));
	if (!creator.m_pData)
		return;

	creator.m_pData->debugData.playingID = in_playingID;
	creator.m_pData->debugData.gameObjID = in_gameObjID;
	creator.m_pData->debugData.soundID.id = in_soundID;
	creator.m_pData->debugData.soundID.bIsBus = in_bIsBus;
	creator.m_pData->debugData.wStringSize = wStringSize;

	AK_WCHAR_TO_UTF16(creator.m_pData->debugData.szMessage, in_pszError, wStringSize);
}
#endif //AK_SUPPORT_WCHAR


#ifdef AK_SUPPORT_WCHAR
void AkMonitor::Monitor_PostString(const char* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel, AkPlayingID in_playingID, AkGameObjectID in_gameObjID, AkUniqueID in_soundID, bool in_bIsBus)
{
	if ( in_pszError )
	{
		wchar_t wszError[ AK_MAX_STRING_SIZE ]; 
		AkUtf8ToWideChar( in_pszError, AK_MAX_STRING_SIZE, wszError );
		wszError[ AK_MAX_STRING_SIZE - 1 ] = 0;
		Monitor_PostString(wszError, in_eErrorLevel, in_playingID, in_gameObjID, in_soundID, in_bIsBus);
	}
}

void AkMonitor::Monitor_AuthoringLog(const char* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel, AkPlayingID in_playingID, AkGameObjectID in_gameObjID, AkUniqueID in_soundID, bool in_bIsBus, bool in_bForceLog)
{
	if (in_pszError)
	{
		wchar_t wszError[AK_MAX_STRING_SIZE];
		AkUtf8ToWideChar(in_pszError, AK_MAX_STRING_SIZE, wszError);
		wszError[AK_MAX_STRING_SIZE - 1] = 0;
		Monitor_AuthoringLog(wszError, in_eErrorLevel, in_playingID, in_gameObjID, in_soundID, in_bIsBus, in_bForceLog);
	}
}
#else
void AkMonitor::Monitor_PostString( const char* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel, AkPlayingID in_playingID, AkGameObjectID in_gameObjID, AkUniqueID in_soundID, bool in_bIsBus )
{
	if ( in_pszError )
	{
		if ( m_uLocalOutputErrorLevel & in_eErrorLevel )
		{
			if ( m_funcLocalOutput )
			{
				m_funcLocalOutput( Monitor::ErrorCode_NoError, in_pszError, in_eErrorLevel, in_playingID, in_gameObjID );
			}
			else
			{
				AKPLATFORM::OutputDebugMsg( in_eErrorLevel == AK::Monitor::ErrorLevel_Message ? AKTEXT("AK Message: ") : AKTEXT("AK Error: ") );
				AKPLATFORM::OutputDebugMsg( in_pszError );
				AKPLATFORM::OutputDebugMsg( AKTEXT("\n") );
			}
		}
		Monitor_AuthoringLog(in_pszError, in_eErrorLevel, in_playingID, in_gameObjID, in_soundID, in_bIsBus);
	}
}

void AkMonitor::Monitor_AuthoringLog(const char* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel, AkPlayingID in_playingID, AkGameObjectID in_gameObjID, AkUniqueID in_soundID, bool in_bIsBus, bool in_bForceLog)
{
	AkUInt16 wStringSize = (AkUInt16)strlen(in_pszError) + 1;
	AkMonitorDataCreator creator(
		in_eErrorLevel == AK::Monitor::ErrorLevel_Message ? AkMonitorData::MonitorDataMessageString : AkMonitorData::MonitorDataErrorString,
		SIZEOF_MONITORDATA_TO(debugData.szMessage) + wStringSize * sizeof(AkUtf16),
		in_bForceLog || IsMessageTraceable(in_eErrorLevel));
	if (!creator.m_pData)
		return;

	creator.m_pData->debugData.playingID = in_playingID;
	creator.m_pData->debugData.gameObjID = in_gameObjID;
	creator.m_pData->debugData.soundID.id = in_soundID;
	creator.m_pData->debugData.soundID.bIsBus = in_bIsBus;
	creator.m_pData->debugData.wStringSize = wStringSize;

	AK_CHAR_TO_UTF16(creator.m_pData->debugData.szMessage, in_pszError, wStringSize);
}
#endif //AK_SUPPORT_WCHAR

void AkMonitor::Monitor_ObjectNotif( 
									AkPlayingID in_PlayingID, 
									AkGameObjectID in_GameObject, 
									const AkCustomParamType & in_CustomParam, 
									AkMonitorData::NotificationReason in_eNotifReason, 
									AkCntrHistArray in_cntrHistArray, 
									AkUniqueID in_targetObjectID,
									bool in_bTargetIsBus,
									AkTimeMs in_timeValue, 
									AkUniqueID in_playlistItemID )
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataObject, SIZEOF_MONITORDATA( objectData ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->objectData.eNotificationReason = in_eNotifReason ;
	creator.m_pData->objectData.gameObjPtr = in_GameObject;
	creator.m_pData->objectData.customParam = in_CustomParam;
	creator.m_pData->objectData.playingID = in_PlayingID;
	creator.m_pData->objectData.cntrHistArray = in_cntrHistArray;
	if( creator.m_pData->objectData.cntrHistArray.uiArraySize > AK_CONT_HISTORY_SIZE )
		creator.m_pData->objectData.cntrHistArray.uiArraySize = AK_CONT_HISTORY_SIZE;
	creator.m_pData->objectData.targetObjectID.id = in_targetObjectID;
	creator.m_pData->objectData.targetObjectID.bIsBus = in_bTargetIsBus;
	creator.m_pData->objectData.timeValue = in_timeValue;
	creator.m_pData->objectData.playlistItemID = in_playlistItemID;
}

void AkMonitor::Monitor_ObjectBusNotif(
	AkGameObjectID in_GameObject,
	AkMonitorData::NotificationReason in_eNotifReason,
	AkUniqueID in_targetObjectID)
{
	AkMonitorDataCreator creator(AkMonitorData::MonitorDataObject, SIZEOF_MONITORDATA(objectData));
	if (!creator.m_pData)
		return;

	memset(&creator.m_pData->objectData, 0, sizeof(creator.m_pData->objectData));
	creator.m_pData->objectData.eNotificationReason = in_eNotifReason;
	creator.m_pData->objectData.gameObjPtr = in_GameObject;
	creator.m_pData->objectData.targetObjectID.id = in_targetObjectID;
	creator.m_pData->objectData.targetObjectID.bIsBus = true;
}

void AkMonitor::Monitor_BankNotif(AkUniqueID in_BankID, AkUniqueID in_LanguageID, AkMonitorData::NotificationReason in_eNotifReason, AkUInt32 in_uPrepareFlags, const char* pszBankFileName )
{
	AkUInt16 wStringSize = 0;
	if (pszBankFileName)
		wStringSize = (AkUInt16)(strlen(pszBankFileName) + 1);

	AkMonitorDataCreator creator( AkMonitorData::MonitorDataBank, SIZEOF_MONITORDATA_TO( bankData.szBankName ) + wStringSize );
	if ( !creator.m_pData )
		return;

	creator.m_pData->bankData.bankID = in_BankID;
	creator.m_pData->bankData.languageID = in_LanguageID;
	creator.m_pData->bankData.uFlags = in_uPrepareFlags;
	creator.m_pData->bankData.eNotificationReason = in_eNotifReason;
	creator.m_pData->bankData.wStringSize = wStringSize;

	if( wStringSize )
	{
		AkMemCpy(creator.m_pData->bankData.szBankName, pszBankFileName, wStringSize);
	}
}

void AkMonitor::Monitor_PrepareNotif( AkMonitorData::NotificationReason in_eNotifReason, AkUniqueID in_GameSyncorEventID, AkUInt32 in_groupID, AkGroupType in_GroupType, AkUInt32 in_NumEvents )
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataPrepare, SIZEOF_MONITORDATA( prepareData ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->prepareData.eNotificationReason = in_eNotifReason;
	creator.m_pData->prepareData.gamesyncIDorEventID = in_GameSyncorEventID;
	creator.m_pData->prepareData.groupID			 = in_groupID;
	creator.m_pData->prepareData.groupType			 = in_GroupType;
	creator.m_pData->prepareData.uNumEvents			 = in_NumEvents;
}

void AkMonitor::Monitor_StateChanged( AkStateGroupID in_StateGroup, AkStateID in_PreviousState, AkStateID in_NewState )
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataState, SIZEOF_MONITORDATA( stateData ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->stateData.stateGroupID = in_StateGroup;
	creator.m_pData->stateData.stateFrom = in_PreviousState;
	creator.m_pData->stateData.stateTo = in_NewState;
}

void AkMonitor::Monitor_SwitchChanged( AkSwitchGroupID in_SwitchGroup, AkSwitchStateID in_Switch, AkGameObjectID in_GameObject )
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataSwitch, SIZEOF_MONITORDATA( switchData ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->switchData.switchGroupID = in_SwitchGroup;
	creator.m_pData->switchData.switchState = in_Switch;
	creator.m_pData->switchData.gameObj = in_GameObject;
}

void AkMonitor::Monitor_ObjectRegistration(AkUInt8 in_uFlags, AkGameObjectID in_GameObject, void * in_pMonitorData, bool in_bRecap)
{
	AkIDStringHash::AkStringHash::Item * pItem = (AkIDStringHash::AkStringHash::Item *) in_pMonitorData;

	// Send notification
	{
		AkUInt16 wStringSize = 0;
		if ( pItem )
			wStringSize = (AkUInt16) strlen( &( pItem->Assoc.item ) ) + 1;

		AkMonitorDataCreator creator( AkMonitorData::MonitorDataObjRegistration, SIZEOF_MONITORDATA_TO( objRegistrationData.szName ) + wStringSize );
		if ( creator.m_pData )
		{
			creator.m_pData->objRegistrationData.uFlags = in_uFlags;
			creator.m_pData->objRegistrationData.gameObjPtr = in_GameObject ;
			creator.m_pData->objRegistrationData.uFlags = (AkUInt16) in_uFlags;
			creator.m_pData->objRegistrationData.wStringSize = wStringSize;

			if (pItem && ((in_uFlags & AkMonitorData::ObjRegistrationFlags_UpdateName) != 0))
				AkMemCpy( creator.m_pData->objRegistrationData.szName, (void *) &( pItem->Assoc.item ), wStringSize );
		}
	}
	
	if(in_bRecap)
		return;

	// Remember name in our own map
	if (((in_uFlags & AkMonitorData::ObjRegistrationFlags_IsRegistered) != 0))
	{
		if( pItem )
		{
			if (g_idxGameObjectString.Set(pItem) != AK_Success)
			{
				MONITOR_FREESTRING( pItem );
				pItem = NULL;
			}
			// g_idxGameObjectString. Set fails, there is nothing to do, this string will stay unknown.
		}
	}
	else
	{
		g_idxGameObjectString.Unset(in_GameObject);
	}
}

void AkMonitor::Monitor_FreeString( void * in_pMonitorData )
{
	if (in_pMonitorData)
	{
		AkIDStringHash::AkStringHash::Item * pItem = (AkIDStringHash::AkStringHash::Item *) in_pMonitorData;
		g_idxGameObjectString.FreePreallocatedString( pItem );
	}
}

void AkMonitor::Monitor_ParamChanged( AkMonitorData::NotificationReason in_eNotifReason, AkUniqueID in_ElementID, bool in_bIsBusElement, AkGameObjectID in_GameObject )
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataParamChanged, SIZEOF_MONITORDATA( paramChangedData ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->paramChangedData.eNotificationReason = in_eNotifReason;
	creator.m_pData->paramChangedData.elementID.id = in_ElementID;
	creator.m_pData->paramChangedData.elementID.bIsBus = in_bIsBusElement;
	creator.m_pData->paramChangedData.gameObjPtr = in_GameObject ;
}

void AkMonitor::Monitor_EventTriggered( AkPlayingID in_PlayingID, AkUniqueID in_EventID, AkGameObjectID in_GameObject, const AkCustomParamType & in_CustomParam)
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataEventTriggered, SIZEOF_MONITORDATA( eventTriggeredData ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->eventTriggeredData.playingID = in_PlayingID;
	creator.m_pData->eventTriggeredData.eventID = in_EventID;
	creator.m_pData->eventTriggeredData.gameObjPtr = in_GameObject ;
	creator.m_pData->eventTriggeredData.customParam = in_CustomParam;
}

void AkMonitor::Monitor_ActionDelayed( AkPlayingID in_PlayingID, AkUniqueID in_ActionID, AkGameObjectID in_GameObject, AkTimeMs in_DelayTime, const AkCustomParamType & in_CustomParam )
{
	if( in_ActionID != AK_INVALID_UNIQUE_ID )
	{
		AkMonitorDataCreator creator( AkMonitorData::MonitorDataActionDelayed, SIZEOF_MONITORDATA( actionDelayedData ) );
		if ( !creator.m_pData )
			return;

		creator.m_pData->actionDelayedData.playingID = in_PlayingID;
		creator.m_pData->actionDelayedData.actionID = in_ActionID;
		creator.m_pData->actionDelayedData.gameObjPtr = in_GameObject;
		creator.m_pData->actionDelayedData.delayTime = in_DelayTime;
		creator.m_pData->actionDelayedData.customParam = in_CustomParam;
	}
}

void AkMonitor::Monitor_ActionTriggered( AkPlayingID in_PlayingID, AkUniqueID in_ActionID, AkGameObjectID in_GameObject, const AkCustomParamType & in_CustomParam )
{
	if( in_ActionID != AK_INVALID_UNIQUE_ID )
	{
		AkMonitorDataCreator creator( AkMonitorData::MonitorDataActionTriggered, SIZEOF_MONITORDATA( actionTriggeredData ) );
		if ( !creator.m_pData )
			return;

		creator.m_pData->actionTriggeredData.playingID = in_PlayingID;
		creator.m_pData->actionTriggeredData.actionID = in_ActionID;
		creator.m_pData->actionTriggeredData.gameObjPtr = in_GameObject;
		creator.m_pData->actionTriggeredData.customParam = in_CustomParam;
	}
}

void AkMonitor::Monitor_BusNotification( AkUniqueID in_BusID, AkMonitorData::BusNotification in_NotifReason, AkUInt32 in_bitsFXBypass, AkUInt32 in_bitsMask )
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataBusNotif, SIZEOF_MONITORDATA( busNotifData ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->busNotifData.busID = in_BusID;
	creator.m_pData->busNotifData.notifReason = in_NotifReason;
	creator.m_pData->busNotifData.bitsFXBypass = (AkUInt8) in_bitsFXBypass;
	creator.m_pData->busNotifData.bitsMask = (AkUInt8) in_bitsMask;
}

void AkMonitor::Monitor_PathEvent( AkPlayingID in_playingID, AkUniqueID in_who, AkMonitorData::AkPathEvent in_eEvent, AkUInt32 in_index)
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataPath, SIZEOF_MONITORDATA( pathData ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->pathData.playingID = (in_playingID);
	creator.m_pData->pathData.ulUniqueID = (in_who);
	creator.m_pData->pathData.eEvent =(in_eEvent);
	creator.m_pData->pathData.ulIndex = (in_index);
}

#ifdef AK_SUPPORT_WCHAR
void AkMonitor::Monitor_errorMsg2( const wchar_t* in_psz1, const wchar_t* in_psz2 )
{
	if( in_psz1 && in_psz2 )
	{
		wchar_t wszBuffer[ AK_MAX_STRING_SIZE ];
		AKPLATFORM::SafeStrCpy( wszBuffer, in_psz1, AK_MAX_STRING_SIZE );
		AKPLATFORM::SafeStrCat( wszBuffer, in_psz2, AK_MAX_STRING_SIZE );

		Monitor_PostString( wszBuffer, AK::Monitor::ErrorLevel_Error, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT);
	}
}
#endif //AK_SUPPORT_WCHAR

void AkMonitor::Monitor_errorMsg2( const char* in_psz1, const char* in_psz2 )
{
	if( in_psz1 && in_psz2 )
	{
		char szBuffer[ AK_MAX_STRING_SIZE ];
		AKPLATFORM::SafeStrCpy( szBuffer, in_psz1, AK_MAX_STRING_SIZE );
		AKPLATFORM::SafeStrCat( szBuffer, in_psz2, AK_MAX_STRING_SIZE );

		Monitor_PostString(szBuffer, AK::Monitor::ErrorLevel_Error, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT);
	}
}

void AkMonitor::Monitor_LoadedBank( CAkUsageSlot* in_pUsageSlot, bool in_bIsDestroyed )
{
	if( in_pUsageSlot )// In some situations, NULL will be received, and it means that nothing must be notified.( keeping the if inside the MACRO )
	{
		AkMonitorDataCreator creator( AkMonitorData::MonitorDataSoundBank, SIZEOF_MONITORDATA( loadedSoundBankData ) );
		if ( !creator.m_pData )
			return;

		creator.m_pData->loadedSoundBankData.bankID = in_pUsageSlot->key.bankID;
		creator.m_pData->loadedSoundBankData.memPoolID = AkMemID_Media;
		creator.m_pData->loadedSoundBankData.uBankSize = in_pUsageSlot->m_uLoadedDataSize;
		creator.m_pData->loadedSoundBankData.uMetaDataSize = in_pUsageSlot->m_uLoadedMetaDataSize;
		creator.m_pData->loadedSoundBankData.uNumIndexableItems = in_pUsageSlot->m_listLoadedItem.Length();
		creator.m_pData->loadedSoundBankData.uNumMediaItems = in_pUsageSlot->m_uNumLoadedItems;
		creator.m_pData->loadedSoundBankData.bIsExplicitelyLoaded = in_pUsageSlot->WasLoadedAsABank();

		creator.m_pData->loadedSoundBankData.bIsDestroyed = in_bIsDestroyed;
	}
}

void AkMonitor::Monitor_MediaPrepared(AkMediaEntry& in_rMediaEntry)
{
	// One entry per bank + one if the media was explicitely prepared too.
	AkUInt32 numBankOptions = in_rMediaEntry.GetNumBankOptions();
	AkUInt32 arraysize = numBankOptions;

	if (in_rMediaEntry.IsDataPrepared())
		++arraysize;

	AkMonitorDataCreator creator(AkMonitorData::MonitorDataMedia, SIZEOF_MONITORDATA_TO(mediaPreparedData.bankMedia) + (arraysize * sizeof(AkMonitorData::MediaPreparedMonitorData::BankMedia)));
	if (!creator.m_pData)
		return;

	creator.m_pData->mediaPreparedData.uMediaID = in_rMediaEntry.GetSourceID();
	creator.m_pData->mediaPreparedData.uArraySize = arraysize;

	AkUInt32 i = 0;
	for (; i < numBankOptions; ++i)
	{
		CAkUsageSlot * pSlot = in_rMediaEntry.m_BankSlots[i].slot;
		if (pSlot)
		{
			creator.m_pData->mediaPreparedData.bankMedia[i].bankID = pSlot->key.bankID;
		}
		else
		{
			creator.m_pData->mediaPreparedData.bankMedia[i].bankID = AK_INVALID_BANK_ID;
		}
		creator.m_pData->mediaPreparedData.bankMedia[i].uMediaSize = in_rMediaEntry.m_BankSlots[i].info.uInMemoryDataSize;
		creator.m_pData->mediaPreparedData.bankMedia[i].uFormat = in_rMediaEntry.m_BankSlots[i].info.GetFormatTag();
	}
	if (in_rMediaEntry.IsDataPrepared())
	{
		creator.m_pData->mediaPreparedData.bankMedia[i].bankID = AK_INVALID_UNIQUE_ID;
		creator.m_pData->mediaPreparedData.bankMedia[i].uMediaSize = in_rMediaEntry.m_preparedMediaInfo.uInMemoryDataSize;
		creator.m_pData->mediaPreparedData.bankMedia[i].uFormat = in_rMediaEntry.m_preparedMediaInfo.GetFormatTag();
	}
}

void AkMonitor::Monitor_EventPrepared( AkUniqueID in_EventID, AkUInt32 in_RefCount )
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataEvent, SIZEOF_MONITORDATA( eventPreparedData ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->eventPreparedData.eventID = in_EventID;
	creator.m_pData->eventPreparedData.uRefCount = in_RefCount;
}

void AkMonitor::Monitor_GameSync( AkUniqueID in_GroupID, AkUniqueID in_GameSyncID, bool in_bIsEnabled, AkGroupType in_GroupType )
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataGameSync, SIZEOF_MONITORDATA( gameSyncData ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->gameSyncData.groupID = in_GroupID;
	creator.m_pData->gameSyncData.syncID = in_GameSyncID;
	creator.m_pData->gameSyncData.bIsEnabled = in_bIsEnabled;
	creator.m_pData->gameSyncData.eSyncType = in_GroupType;
}

void AkMonitor::Monitor_SetParamNotif_Float( AkMonitorData::NotificationReason in_eNotifReason, AkUniqueID in_ElementID, bool in_bIsBusElement, AkGameObjectID in_GameObject, AkReal32 in_TargetValue, AkValueMeaning in_ValueMeaning, AkTimeMs in_TransitionTime )
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataSetParam, SIZEOF_MONITORDATA( setParamData ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->setParamData.eNotificationReason = in_eNotifReason;
	creator.m_pData->setParamData.elementID.id = in_ElementID;
	creator.m_pData->setParamData.elementID.bIsBus = in_bIsBusElement;
	creator.m_pData->setParamData.gameObjPtr = in_GameObject;
	creator.m_pData->setParamData.valueMeaning = in_ValueMeaning;
	creator.m_pData->setParamData.fTarget = in_TargetValue;
	creator.m_pData->setParamData.transitionTime = in_TransitionTime;
}

void AkMonitor::Monitor_Dialogue( 
	AkMonitorData::MonitorDataType in_type, 
	AkUniqueID in_idDialogueEvent, 
	AkUniqueID in_idObject, 
	AkUInt32 in_cPath, 
	AkArgumentValueID * in_pPath, 
	AkPlayingID in_idSequence, 
	AkUInt16 in_uRandomChoice, 
	AkUInt16 in_uTotalProbability,
	AkUInt8 in_uWeightedDecisionType, 
	AkUInt32 in_uWeightedPossibleCount, 
	AkUInt32 in_uWeightedTotalCount )
{
	if ( in_idDialogueEvent == 0 )
		return;

	AkMonitorDataCreator creator( in_type, SIZEOF_MONITORDATA_TO( commonDialogueData.aPath ) + ( in_cPath * sizeof( AkArgumentValueID ) ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->commonDialogueData.idDialogueEvent = in_idDialogueEvent;
	creator.m_pData->commonDialogueData.idObject = in_idObject;
	creator.m_pData->commonDialogueData.uPathSize = in_cPath;
	creator.m_pData->commonDialogueData.idSequence = in_idSequence;
	creator.m_pData->commonDialogueData.uRandomChoice = in_uRandomChoice;
	creator.m_pData->commonDialogueData.uTotalProbability = in_uTotalProbability;
	creator.m_pData->commonDialogueData.uWeightedDecisionType = in_uWeightedDecisionType;
	creator.m_pData->commonDialogueData.uWeightedPossibleCount = in_uWeightedPossibleCount;
	creator.m_pData->commonDialogueData.uWeightedTotalCount = in_uWeightedTotalCount;

	for ( AkUInt32 i = 0; i < in_cPath; ++i )
		creator.m_pData->commonDialogueData.aPath[ i ] = in_pPath[ i ];
}

//***********************************

void * AkMonitor::Monitor_AllocateGameObjNameString( AkGameObjectID in_GameObject, const char* in_GameObjString )
{
	return g_idxGameObjectString.Preallocate(in_GameObject, in_GameObjString);
}

#ifdef AK_SUPPORT_WCHAR
void AkMonitor::Monitor_SetPoolName( AkMemPoolId in_PoolId, const wchar_t * in_tcsPoolName )
{
	// Monitor is not yet instantiated when some basic pools get created -- just skip the
	// notification, no one is possibly connected anyway.
	if ( !AkMonitor::Get() || !in_tcsPoolName )
		return;

	AkUInt16 wStringSize = (AkUInt16) wcslen( in_tcsPoolName ) + 1;

	AkMonitorDataCreator creator( AkMonitorData::MonitorDataMemoryPoolName, offsetof(AkMonitorData::MonitorDataItem, memoryPoolNameData.szName) + wStringSize * sizeof( AkUtf16 ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->memoryPoolNameData.ulPoolId = in_PoolId;
	creator.m_pData->memoryPoolNameData.wStringSize = wStringSize;
	
	AK_WCHAR_TO_UTF16( creator.m_pData->memoryPoolNameData.szName, in_tcsPoolName, wStringSize);
}

void AkMonitor::Monitor_SetPoolName( AkMemPoolId in_PoolId, const char * in_tcsPoolName )
{
	wchar_t wideString[ AK_MAX_PATH ];	
	if(AkCharToWideChar( in_tcsPoolName, AK_MAX_PATH, wideString ) > 0)
		Monitor_SetPoolName( in_PoolId, wideString );
}

#else
void AkMonitor::Monitor_SetPoolName( AkMemPoolId in_PoolId, const char * in_tcsPoolName )
{
	// Monitor is not yet instantiated when some basic pools get created -- just skip the
	// notification, no one is possibly connected anyway.
	if ( !AkMonitor::Get() || !in_tcsPoolName )
		return;

	AkUInt16 wStringSize = (AkUInt16) OsStrLen( in_tcsPoolName ) + 1;

	AkMonitorDataCreator creator( AkMonitorData::MonitorDataMemoryPoolName, SIZEOF_MONITORDATA_TO( memoryPoolNameData.szName ) + wStringSize * sizeof( AkUtf16 ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->memoryPoolNameData.ulPoolId = in_PoolId;
	creator.m_pData->memoryPoolNameData.wStringSize = wStringSize;
	
	AK_CHAR_TO_UTF16( creator.m_pData->memoryPoolNameData.szName, in_tcsPoolName, wStringSize);
}

#endif //AK_SUPPORT_WCHAR

void AkMonitor::Monitor_MarkersNotif( AkPlayingID in_PlayingID, AkGameObjectID in_GameObject, const AkCustomParamType & in_CustomParam, AkMonitorData::NotificationReason in_eNotifReason, AkCntrHistArray in_cntrHistArray, const char* in_strLabel )
{
	AkUInt16 wStringSize = 0;
	if( in_strLabel )
		wStringSize = (AkUInt16)strlen( in_strLabel )+1;

	AkUInt32 sizeofData = SIZEOF_MONITORDATA_TO( markersData.szLabel ) + wStringSize;
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataMarkers, sizeofData );
	if ( !creator.m_pData )
		return;

	creator.m_pData->markersData.eNotificationReason = in_eNotifReason ;
	creator.m_pData->markersData.gameObjPtr = in_GameObject;
	creator.m_pData->markersData.customParam = in_CustomParam;
	creator.m_pData->markersData.playingID = in_PlayingID;
	creator.m_pData->markersData.cntrHistArray = in_cntrHistArray;
	creator.m_pData->markersData.targetObjectID = 0;
	creator.m_pData->markersData.wStringSize = wStringSize;
	if( wStringSize )
		AkMemCpy(creator.m_pData->markersData.szLabel, in_strLabel, wStringSize);
}

void AkMonitor::Monitor_MusicSwitchTransNotif( 
	AkPlayingID in_PlayingID, 
	AkGameObjectID in_GameObject, 
	AkMonitorData::NotificationReason in_eNotifReason, 
	AkUInt32 in_transRuleIndex, 
	AkUniqueID in_ownerID,
	AkUniqueID in_nodeSrcID,
	AkUniqueID in_nodeDestID, 
	AkUniqueID in_segmentSrcID, 
	AkUniqueID in_segmentDestID, 
	AkUniqueID in_cueSrc,	// Pass cue name hash if transition was scheduled on cue, AK_INVALID_UNIQUE_ID otherwise
	AkUniqueID in_cueDest,	// Pass cue name hash if transition was scheduled on cue, AK_INVALID_UNIQUE_ID otherwise
	AkTimeMs in_time )
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataMusicTransition, SIZEOF_MONITORDATA( musicTransData ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->musicTransData.playingID				= in_PlayingID;
	creator.m_pData->musicTransData.gameObj					= in_GameObject;
	creator.m_pData->musicTransData.eNotificationReason		= in_eNotifReason;
	creator.m_pData->musicTransData.uTransitionRuleIndex	= in_transRuleIndex;
	creator.m_pData->musicTransData.ownerID					= in_ownerID;
	creator.m_pData->musicTransData.nodeSrcID				= in_nodeSrcID;
	creator.m_pData->musicTransData.nodeDestID				= in_nodeDestID;
	creator.m_pData->musicTransData.segmentSrc				= in_segmentSrcID;
	creator.m_pData->musicTransData.segmentDest				= in_segmentDestID;
	creator.m_pData->musicTransData.time					= in_time;
	creator.m_pData->musicTransData.cueSrc					= in_cueSrc;
	creator.m_pData->musicTransData.cueDest					= in_cueDest;
}

void AkMonitor::Monitor_MusicTrackTransNotif( 
	AkPlayingID in_PlayingID, 
	AkGameObjectID in_GameObject, 
	AkMonitorData::NotificationReason in_eNotifReason, 
	AkUniqueID in_musicTrackID,
	AkUniqueID in_switchGroupID,
	AkUniqueID in_prevSwitchID,
	AkUniqueID in_nextSwitchID, 
	AkTimeMs in_time )
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataMusicTransition, SIZEOF_MONITORDATA( musicTransData ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->musicTransData.playingID				= in_PlayingID;
	creator.m_pData->musicTransData.gameObj					= in_GameObject;
	creator.m_pData->musicTransData.eNotificationReason		= in_eNotifReason;
	creator.m_pData->musicTransData.uTransitionRuleIndex	= 0;
	creator.m_pData->musicTransData.ownerID					= in_musicTrackID;
	creator.m_pData->musicTransData.nodeSrcID				= in_prevSwitchID;
	creator.m_pData->musicTransData.nodeDestID				= in_nextSwitchID;
	creator.m_pData->musicTransData.segmentSrc				= in_switchGroupID;
	creator.m_pData->musicTransData.segmentDest				= AK_INVALID_UNIQUE_ID;
	creator.m_pData->musicTransData.time					= in_time;
	creator.m_pData->musicTransData.cueSrc					= AK_INVALID_UNIQUE_ID;
	creator.m_pData->musicTransData.cueDest					= AK_INVALID_UNIQUE_ID;
}

bool GetHiddenData(AkQueuedMsg* in_message, char*& io_pHiddenData, AkUInt32& io_usHiddenDataSize)
{
	bool bHasHiddenData = false;

	io_pHiddenData = NULL;
	io_usHiddenDataSize = 0;

	if (in_message->type == QueuedMsgType_RegisterGameObj)
	{
		bHasHiddenData = true;
		AkIDStringHash::AkStringHash::Item * pItem = (AkIDStringHash::AkStringHash::Item *) (void*)in_message->reggameobj.pMonitorData;
		if (pItem)
		{
			io_pHiddenData = (char *)&(pItem->Assoc.item);
			if (io_pHiddenData)
			{
				io_usHiddenDataSize = (AkUInt32)(sizeof(char)*(strlen(io_pHiddenData) + 1));
			}
		}
	}

	return bHasHiddenData;
}

void AkMonitor::Monitor_Api_Function(AkQueuedMsg* in_message)
{
	if ((AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataApiFunctions)))
	{
		AkUInt32 iExtraHiddenSize = 0;
		char* pHiddenString = NULL;
		AkUInt32 uTotalHiddenSize = 0;

		bool bHasHiddenData = GetHiddenData(in_message, pHiddenString, iExtraHiddenSize);
		
		int itemSize = SIZEOF_MONITORDATA_TO(varLenData.abyData) + in_message->size;
		if (bHasHiddenData)
		{
			uTotalHiddenSize = iExtraHiddenSize + sizeof(AkUInt32);
		}
		AkMonitorDataCreator creator(AkMonitorData::MonitorDataApiFunctions, itemSize + uTotalHiddenSize);
		creator.m_pData->varLenData.dataSize = in_message->size;

		AkUInt8* pWritePtr = &(creator.m_pData->varLenData.abyData[0]);
		AKPLATFORM::AkMemCpy(pWritePtr, in_message, in_message->size);
		pWritePtr += in_message->size;
		if ( bHasHiddenData )
		{
			// Writing Hidden Data size (can be 0)
			*((AkUInt32*)pWritePtr) = iExtraHiddenSize;
			pWritePtr += sizeof(AkUInt32);

			// Writing Data if any
			if (iExtraHiddenSize)
			{
				memcpy(pWritePtr, pHiddenString, iExtraHiddenSize);
			}
			creator.m_pData->varLenData.dataSize += uTotalHiddenSize;
		}
	}
}

void AkMonitor::Monitor_MusicPlaylistTransNotif( 
	AkPlayingID in_PlayingID, 
	AkGameObjectID in_GameObject, 
	AkMonitorData::NotificationReason in_eNotifReason, 
	AkUInt32 in_transRuleIndex, 
	AkUniqueID in_ownerID,
	AkUniqueID in_segmentSrcID, 
	AkUniqueID in_segmentDestID,
	AkTimeMs in_time )
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataMusicTransition, SIZEOF_MONITORDATA( musicTransData ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->musicTransData.playingID				= in_PlayingID;
	creator.m_pData->musicTransData.gameObj					= in_GameObject;
	creator.m_pData->musicTransData.eNotificationReason		= in_eNotifReason;
	creator.m_pData->musicTransData.uTransitionRuleIndex	= in_transRuleIndex;
	creator.m_pData->musicTransData.ownerID					= in_ownerID;
	creator.m_pData->musicTransData.nodeSrcID				= AK_INVALID_UNIQUE_ID;
	creator.m_pData->musicTransData.nodeDestID				= AK_INVALID_UNIQUE_ID;
	creator.m_pData->musicTransData.segmentSrc				= in_segmentSrcID;
	creator.m_pData->musicTransData.segmentDest				= in_segmentDestID;
	creator.m_pData->musicTransData.time					= in_time;
	creator.m_pData->musicTransData.cueSrc					= 0;
	creator.m_pData->musicTransData.cueDest					= 0;
}

void AkMonitor::Monitor_MidiTargetNotif( 
	AkPlayingID in_PlayingID, 
	AkGameObjectID in_GameObject, 
	AkMonitorData::NotificationReason in_eNotifReason, 
	AkUniqueID in_musicTrackID, 
	AkUniqueID in_midiTargetID,
	AkTimeMs in_time )
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataMidiTarget, SIZEOF_MONITORDATA( midiTargetData ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->midiTargetData.playingID				= in_PlayingID;
	creator.m_pData->midiTargetData.gameObj					= in_GameObject;
	creator.m_pData->midiTargetData.eNotificationReason		= in_eNotifReason;
	creator.m_pData->midiTargetData.musicTrackID			= in_musicTrackID;
	creator.m_pData->midiTargetData.midiTargetID			= in_midiTargetID;
	creator.m_pData->musicTransData.time					= in_time;
}

void AkMonitor::Monitor_MidiEvent( AkMidiEventEx in_midiEv, AkGameObjectID in_GameObject, AkUniqueID in_musicTrackID, AkUniqueID in_midiTargetID )
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataMidiEvent, SIZEOF_MONITORDATA( midiEventData ) );
	if ( !creator.m_pData )
		return;

	*((AkMidiEventEx*)creator.m_pData->midiEventData.midiEvent) = in_midiEv;
	creator.m_pData->midiEventData.gameObj				= in_GameObject;
	creator.m_pData->midiEventData.musicTrackID			= in_musicTrackID;
	creator.m_pData->midiEventData.midiTargetID			= in_midiTargetID;
}

void AkMonitor::Monitor_TimeStamp( bool in_bForceLog )
{
	AkMonitorDataCreator creator( AkMonitorData::MonitorDataTimeStamp, SIZEOF_MONITORDATA( timeStampData ), in_bForceLog);
	if ( !creator.m_pData )
		return;

	creator.m_pData->timeStampData.timeStamp = GetThreadTime();
}

void AkMonitor::Monitor_EndOfFrame()
{
	AkMonitorDataCreator creator(AkMonitorData::MonitorDataEndFrame, SIZEOF_MONITORDATA(endOfFrameData));
	if (!creator.m_pData)
		return;
}

void AkMonitor::Monitor_Init()
{
	AkMonitorDataCreator creator(AkMonitorData::MonitorDataInit, SIZEOF_MONITORDATA(initData));
	if (!creator.m_pData)
		return;

	creator.m_pData->initData.fFreqRatio = AK::g_fFreqRatio;
	creator.m_pData->initData.uCoreSampleRate = AK_CORE_SAMPLERATE;
	creator.m_pData->initData.uFramesPerBuffer = AK_NUM_VOICE_REFILL_FRAMES;
	creator.m_pData->initData.floorPlaneID = (AkUInt8)g_settings.eFloorPlane;
}

void AkMonitor::Monitor_PluginSendData(void * in_pData, AkUInt32 in_uDataSize, AkUniqueID in_audioNodeID, AkGameObjectID in_gameObjectID, AkPluginID in_pluginID, AkUInt32 in_uFXIndex)
{
	if ( in_uDataSize > 0 )
	{
		AkProfileDataCreator creator( SIZEOF_MONITORDATA_TO( pluginMonitorData.arBytes ) + in_uDataSize );
		if ( !creator.m_pData )
			return;

		creator.m_pData->eDataType = AkMonitorData::MonitorDataPlugin;
		creator.m_pData->pluginMonitorData.audioNodeID	= in_audioNodeID;
		creator.m_pData->pluginMonitorData.gameObj = in_gameObjectID;
		creator.m_pData->pluginMonitorData.pluginID		= in_pluginID;
		creator.m_pData->pluginMonitorData.uFXIndex		= in_uFXIndex;
		creator.m_pData->pluginMonitorData.uDataSize		= in_uDataSize;
		memcpy( creator.m_pData->pluginMonitorData.arBytes, in_pData, in_uDataSize );
	}
}

void AkMonitor::Monitor_ExternalSourceData( AkPlayingID in_idPlay, AkGameObjectID in_idGameObj, AkUniqueID in_idSrc, const AkOSChar* in_pszFile )
{
	AkUInt16 wStringSize = (AkUInt16) AKPLATFORM::OsStrLen( in_pszFile ) + 1;

	AkMonitorDataCreator creator( AkMonitorData::MonitorDataExternalSource, SIZEOF_MONITORDATA_TO( templateSrcData.szUtf16File ) + wStringSize * sizeof( AkUtf16 ) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->templateSrcData.idGameObj = in_idGameObj;
	creator.m_pData->templateSrcData.idPlay = in_idPlay;
	creator.m_pData->templateSrcData.idSource = in_idSrc;
	creator.m_pData->templateSrcData.wStringSize = wStringSize;

	AK_OSCHAR_TO_UTF16( creator.m_pData->templateSrcData.szUtf16File, in_pszFile, wStringSize);
}

// This function was previously inlined, but the 360 compiler had problems inlining it.
AkUInt32 AkMonitorData::RealSizeof( const MonitorDataItem & in_rItem )
{	
	switch ( in_rItem.eDataType )
	{
	case MonitorDataTimeStamp:
		return SIZEOF_MONITORDATA( timeStampData );

	case MonitorDataCPUTimer:
		return SIZEOF_MONITORDATA_TO(cpuTimerData.item)
			+ in_rItem.cpuTimerData.uNumItems * sizeof(AkAudiolibTimer::Item);

	case MonitorDataMemoryPool:
		return SIZEOF_MONITORDATA_TO( memoryData.poolData )
			+ in_rItem.memoryData.uNumPools * sizeof( MemoryPoolData );

	case MonitorDataEnvironment:
		return SIZEOF_MONITORDATA_TO( environmentData.envPacket )
			+ in_rItem.environmentData.ulNumEnvPacket * sizeof( EnvPacket )
			+ in_rItem.environmentData.ulTotalAuxSends * sizeof( AuxSendValue );

	case MonitorDataObsOcc:
		return SIZEOF_MONITORDATA_TO( obsOccData.obsOccPacket )
			+ in_rItem.obsOccData.ulNumPacket * sizeof( ObsOccPacket );

	case MonitorDataListeners:
		return SIZEOF_MONITORDATA_TO( listenerData ) + in_rItem.listenerData.uSizeTotal;

    case MonitorDataStreaming:
		return SIZEOF_MONITORDATA_TO( streamingData.streamData )
			+ in_rItem.streamingData.ulNumStreams * sizeof( StreamData );

    case MonitorDataStreamingDevice:
		return SIZEOF_MONITORDATA_TO( streamingDeviceData.deviceData )
			+ in_rItem.streamingDeviceData.ulNumDevices * sizeof( DeviceData );

    case MonitorDataStreamsRecord:
		return SIZEOF_MONITORDATA_TO( streamRecordData.streamRecords )
			+ in_rItem.streamRecordData.ulNumNewRecords * sizeof( StreamRecord );

    case MonitorDataDevicesRecord:
        return SIZEOF_MONITORDATA( deviceRecordData );

	case MonitorDataObject:
		return SIZEOF_MONITORDATA( objectData );

	case MonitorDataAudioPerf:
		return SIZEOF_MONITORDATA( audioPerfData );

	case MonitorDataGameObjPosition:
		return SIZEOF_MONITORDATA_TO( gameObjPositionData.positions )
			+ in_rItem.gameObjPositionData.ulNumGameObjPositions * sizeof( GameObjPosition );

	case MonitorDataBank:
		return SIZEOF_MONITORDATA_TO( bankData.szBankName )
			+ in_rItem.bankData.wStringSize;

	case MonitorDataPrepare:
		return SIZEOF_MONITORDATA( prepareData );

	case MonitorDataState:
		return SIZEOF_MONITORDATA( stateData );

	case MonitorDataSwitch:
		return SIZEOF_MONITORDATA( switchData );

	case MonitorDataParamChanged:
		return SIZEOF_MONITORDATA( paramChangedData );

	case MonitorDataEventTriggered:
		return SIZEOF_MONITORDATA( eventTriggeredData );

	case MonitorDataActionDelayed:
		return SIZEOF_MONITORDATA( actionDelayedData );

	case MonitorDataActionTriggered:
		return SIZEOF_MONITORDATA( actionTriggeredData );

	case MonitorDataBusNotif:
		return SIZEOF_MONITORDATA( busNotifData );

	case MonitorDataPath:
		return SIZEOF_MONITORDATA( pathData );

	case MonitorDataSoundBank:
		return SIZEOF_MONITORDATA( loadedSoundBankData );

	case MonitorDataEvent:
		return SIZEOF_MONITORDATA( eventPreparedData );

	case MonitorDataGameSync:
		return SIZEOF_MONITORDATA( gameSyncData );

	case MonitorDataSetParam:
		return SIZEOF_MONITORDATA( setParamData );

	case MonitorDataObjRegistration:
		return SIZEOF_MONITORDATA_TO( objRegistrationData.szName )
			+ in_rItem.objRegistrationData.wStringSize;

	case MonitorDataErrorCode:
	case MonitorDataMessageCode:
		return SIZEOF_MONITORDATA( errorData1 );

	case MonitorDataErrorString:
	case MonitorDataMessageString:
		return SIZEOF_MONITORDATA_TO( debugData.szMessage )
			+ in_rItem.debugData.wStringSize * sizeof( AkUtf16 );

	case MonitorDataMemoryPoolName:
		return SIZEOF_MONITORDATA_TO( memoryPoolNameData.szName )
			+ in_rItem.memoryPoolNameData.wStringSize * sizeof( AkUtf16 );

	case MonitorDataMarkers:
		return SIZEOF_MONITORDATA_TO( markersData.szLabel )
			+ in_rItem.markersData.wStringSize;

	case MonitorDataSegmentPosition:
		return SIZEOF_MONITORDATA_TO( segmentPositionData.positions )
			+ in_rItem.segmentPositionData.numPositions * sizeof( SegmentPositionData );
			
	case MonitorDataMedia:
		return SIZEOF_MONITORDATA_TO( mediaPreparedData.bankMedia )
			+ in_rItem.mediaPreparedData.uArraySize * sizeof( AkMonitorData::MediaPreparedMonitorData::BankMedia );

	case MonitorDataResolveDialogue:	
		return SIZEOF_MONITORDATA_TO( commonDialogueData.aPath )
			+ in_rItem.commonDialogueData.uPathSize * sizeof( AkArgumentValueID );

	case MonitorDataMusicTransition:
		return SIZEOF_MONITORDATA( musicTransData );

	case MonitorDataMidiTarget:
		return SIZEOF_MONITORDATA( midiTargetData );

	case MonitorDataMidiEvent:
		return SIZEOF_MONITORDATA( midiEventData );

	case MonitorDataPlugin:
		return SIZEOF_MONITORDATA_TO( pluginMonitorData.arBytes )
			+ in_rItem.pluginMonitorData.uDataSize;

	case MonitorDataPluginError:
	case MonitorDataPluginMessage:
		return SIZEOF_MONITORDATA_TO( pluginMonitorError.szMessage )
			+ in_rItem.pluginMonitorError.wStringSize * sizeof( AkUtf16 );

	case MonitorDataExternalSource:
		return SIZEOF_MONITORDATA_TO( templateSrcData.szUtf16File )
			+ in_rItem.templateSrcData.wStringSize * sizeof( AkUtf16 );

	case MonitorDataSends:
		return SIZEOF_MONITORDATA( sendsData )
			+ (in_rItem.sendsData.ulNumSends - 1) * sizeof(AkMonitorData::SendsData);

	case MonitorDataPlatformSinkType:
		return SIZEOF_MONITORDATA( platformSinkTypeData )
			+ (in_rItem.platformSinkTypeData.uBufSize) * sizeof(char);

	case MonitorDataSpatialAudioObjects:
	case MonitorDataSpatialAudioRoomsAndPortals:
	case MonitorDataSpatialAudioGeometry:
	case MonitorDataMeter:
	case MonitorDataPipeline:
	case MonitorDataApiFunctions:
	case MonitorDataParamDelta:
		return SIZEOF_MONITORDATA_TO( varLenData.abyData )
			+ in_rItem.varLenData.dataSize;
	
	
	case MonitorDataSoundPosition:
		return SIZEOF_MONITORDATA_TO(soundPositionData.positions)
			+ in_rItem.soundPositionData.numPositions * sizeof(AkMonitorData::SoundPositionData);	
		
		
	case MonitorDataEndFrame:
		return SIZEOF_MONITORDATA(endOfFrameData);


	case MonitorDataInit:
		return SIZEOF_MONITORDATA(initData);


	default:
		AKASSERT( false && "Need accurate size for MonitorDataItem member" );
		return sizeof( MonitorDataItem );
	}
}

void AkMonitor::RecapParamDelta()
{		
	if (AkDeltaMonitor::Enabled())
	{
		CAkURenderer::RecapParamDelta();	// Sounds.
		CAkLEngine::RecapParamDelta();		// Busses.
		g_pRegistryMgr->RecapParamDelta();  // Emitter-listener
		m_bRecapDeltaMonitor = false;
	}
}

AkUInt8 * AkDeltaMonitorEnabled::m_pData = NULL;
AkUInt8 * AkDeltaMonitorEnabled::m_pWritePtr = NULL;
AkUInt32 AkDeltaMonitorEnabled::m_cReserved = 0;

AkUniqueID AkDeltaMonitorEnabled::m_idSound(AK_INVALID_UNIQUE_ID);
AkUniqueID AkDeltaMonitorEnabled::m_idSource(AK_INVALID_UNIQUE_ID);
AkPipelineID AkDeltaMonitorEnabled::m_pipelineID(AK_INVALID_PIPELINE_ID);
AkUniqueID AkDeltaMonitorEnabled::m_idRTPC = AK_INVALID_UNIQUE_ID;
AkArray<AkUniqueID, AkUniqueID> AkDeltaMonitorEnabled::m_Stack;

bool AkDeltaMonitorEnabled::m_bActive(false);
bool AkDeltaMonitorEnabled::m_bSerializeSuccess(false);
bool AkDeltaMonitorEnabled::m_bRealDelta = false;

AKRESULT AkDeltaMonitorEnabled::Init()
{
	m_bActive = false;
	m_bSerializeSuccess = false;
	m_bRealDelta = false;
	m_idSound = AK_INVALID_UNIQUE_ID;
	m_pipelineID = AK_INVALID_PIPELINE_ID;	
	m_idRTPC = AK_INVALID_UNIQUE_ID;
	m_cReserved = AKDELTAMONITOR_MIN_INCREMENT;
	m_pData = (AkUInt8*)AkAlloc(AkMemID_Profiler, m_cReserved);
	if (m_pData == NULL)
		return AK_InsufficientMemory;

	m_pWritePtr = m_pData;
	return AK_Success;
}

void AkDeltaMonitorEnabled::Term()
{
	if (m_pData)
	{
		AkFree(AkMemID_Profiler, m_pData);
		m_pData = NULL;
		m_pWritePtr = NULL;
		m_cReserved = 0;
	}

	m_Stack.Term();
}
 
bool AkDeltaMonitorEnabled::Post()
{
	AkInt32 dataSize = (AkInt32)(m_pWritePtr - m_pData);
	if (dataSize == 0)
		return true;

	AkInt32 itemSize = SIZEOF_MONITORDATA_TO(varLenData.abyData) + dataSize;

	AkMonitor * pMonitor = AkMonitor::Get();
	AkMonitorData::MonitorDataItem* pItem = (AkMonitorData::MonitorDataItem*)pMonitor->BeginWrite(itemSize);
	AK_IF_PERF_OFFLINE_RENDERING({
		// In offline mode, we empty the buffer and make room for the new item
		while (!pItem)
		{
			pMonitor->DispatchNotification();
			pItem = (AkMonitorData::MonitorDataItem*)pMonitor->BeginWrite(itemSize);
		}
	})

	if (pItem)
	{
		// write monitor data type (blob)
		pItem->eDataType = AkMonitorData::MonitorDataParamDelta;

		// write data size
		pItem->varLenData.dataSize = (AkUInt32)dataSize;

		// write data
		AKPLATFORM::AkMemCpy((AkUInt8*)pItem->varLenData.abyData, m_pData, dataSize);

		pMonitor->EndWrite(pItem, itemSize);
	}
	else
	{
		AkMonitor::SetDeltaRecap();
		return false;
	}	

	return true;
}

bool AkDeltaMonitorEnabled::ReallocBuffer(AkUInt32 in_uSize)
{
	AkUInt8* pNewPtr = (AkUInt8*)AK::MemoryMgr::Realloc(AkMemID_Profiler, m_pData, m_cReserved + AkMax(in_uSize, AKDELTAMONITOR_MIN_INCREMENT));
	if (pNewPtr != m_pData)
	{
		if (pNewPtr)
		{
			m_pWritePtr = pNewPtr + (m_pWritePtr - m_pData);
			m_pData = pNewPtr;
		}
		else
		{
			m_bSerializeSuccess = false;
			return false;
		}
	}
	m_cReserved += AkMax(in_uSize, AKDELTAMONITOR_MIN_INCREMENT);
	return true;
}

static void StateGroupUnsetLogging(CAkPBI * in_pPBI, const AkRTPCKey &, void * in_pCookie)
{	
	AkDeltaMonitor::LogPipelineID(in_pPBI->GetPipelineID());
}

static void StateGroupUnsetLoggingBus(CAkBehavioralCtx * in_pCtx, void * in_pCookie)
{
	in_pCtx->LogPipelineID();	
}

void AkDeltaMonitor::LogStateUnset(CAkParameterNodeBase* in_pNode, AkUniqueID in_uStateGroup)
{
	if (in_pNode->IsActive() || in_pNode->IsPlaying())
	{
		OpenUpdateBrace(AkDelta_State, in_pNode->key, AkDelta_MultiUnset);

		// Since we're at the start of the buffer, 
		// assume there's enough space and skip CheckSize().
		Put(in_pNode->key);
		Put(in_uStateGroup);
		in_pNode->ForAllPBI(StateGroupUnsetLogging, AkRTPCKey(), NULL);
		CAkLEngine::ForAllMixContext(StateGroupUnsetLoggingBus, NULL);
		
		CloseUpdateBrace(in_pNode->key);
	}
}

void AkResourceMonitor::Callback()
{
	for (int i = (int)AkResourceMonitor::aFuncs.Length() - 1; i >= 0; --i) // from end in case an extension unregisters itself in callback.
	{
		AkResourceMonitor::aFuncs[i](AkResourceMonitor::GetDataSummary());
	}
}

void AkResourceMonitor::Destroy() 
{
	AkResourceMonitor::aFuncs.Term();
}

AKRESULT AkResourceMonitor::Register(AkResourceMonitorCallbackFunc in_pCallback)
{
	CAkFunctionCritical GlobalLock;

	if (AK::SoundEngine::IsInitialized() == false)
		return AK_Fail;

	return AkResourceMonitor::aFuncs.AddLast(in_pCallback) ? AK_Success : AK_InsufficientMemory;
}

AKRESULT AkResourceMonitor::Unregister(AkResourceMonitorCallbackFunc in_pCallback)
{
	CAkFunctionCritical GlobalLock;
	
	if (AK::SoundEngine::IsInitialized() == false)
		return AK_Fail;
	
	AKRESULT eResult = AK_Success;

	ResourceMonitorFuncArray::Iterator it = AkResourceMonitor::aFuncs.FindEx(in_pCallback);
	if (it != AkResourceMonitor::aFuncs.End())
	{
		AkResourceMonitor::aFuncs.Erase(it);
	}
	else
	{
		eResult = AK_InvalidParameter;
	}
	return eResult;
}

bool AkResourceMonitor::HasSubscribers()
{
	return AkResourceMonitor::aFuncs.Length() ? true : false;
}

#endif
#ifdef AK_DELTAMONITOR_RELEASE
FILE *AkDeltaMonitorPrint::m_pFile = NULL;
char AkDeltaMonitorPrint::m_TmpBuffer[AK_DELTAMONITOR_BUFFER];
AkUInt32 AkDeltaMonitorPrint::m_uUsed = 0;
bool AkDeltaMonitorPrint::m_bRealData = false;
char *AkDeltaMonitorPrint::AkDeltaReasons[] = {
	"AkDelta_None",
	"AkDelta_Pipeline", // Associated with a pipeline ID
	"AkDelta_EmitterListener", // Associated with an emitter", then listener(s)
	"AkDelta_MultiUpdate", // A parameter that affects multiple targets
	"AkDelta_MultiUnset", // A parameter that needs to be removed on possibly multiple targets
	"AkDelta_CachePrecomputedParams", // Cached parameters to reuse in the children
	"AkDelta_BaseValue",
	"AkDelta_RTPC",
	"AkDelta_Switch",
	"AkDelta_State",
	"AkDelta_Event",
	"AkDelta_Mute",
	"AkDelta_RangeMod",
	"AkDelta_Ducking",
	"AkDelta_AuxUserDef",
	"AkDelta_Distance",
	"AkDelta_Cone",
	"AkDelta_Occlusion",
	"AkDelta_Obstruction",
	"AkDelta_HDRGain",
	"AkDelta_MusicSegmentEnvelope",
	"AkDelta_Modulator",
	"AkDelta_BGMMute",
	"AkDelta_Fade",
	"AkDelta_Pause",
	"AkDelta_Rays",
	"AkDelta_RaysFilter",
	"AkDelta_LiveEdit",
	"AkDelta_UsePrecomputedParams", // Send when we should use the cached precomputed params
	"AkDelta_SetGameObjectAuxSendValues",
	"AkDelta_Normalization",
	"AkDelta_SetGameObjectOutputBusVolume",
	"AkDelta_GameObjectDestroyed", // this is also a high-level packet
	"AkDelta_RTPCGlobal",
	"AkDelta_ModulatorGlobal",
};
#endif