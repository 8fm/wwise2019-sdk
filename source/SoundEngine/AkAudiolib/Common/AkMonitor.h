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
// AkMonitor.h
//
// alessard
//
//////////////////////////////////////////////////////////////////////

#ifndef _AKMONITOR_H_
#define _AKMONITOR_H_

#include "IALMonitor.h"
#include <AK/Tools/Common/AkLock.h>

#include "AkMonitorData.h"
#include "../../Communication/Common/MonitorSerializer.h"

#include "AkChunkRing.h"
#include "AkKeyList.h"
#include "AkStateMgr.h"

#include <AK/Tools/Common/AkHashList.h>

#include <AK/SoundEngine/Common/AkCallback.h>

using namespace AKPLATFORM;

class CAkUsageSlot;
class AkMediaEntry;
class AkLockLessMsgQueue;
struct AkQueuedMsg;

#define MONITOR_QUEUE_DEFAULT_SIZE ( 256 * 1024 )

class AkMonitor 
	: public AK::IALMonitor
{
public:

	typedef AkArrayAllocatorNoAlign<AkMemID_Profiler> MonitorPoolDefault;

	typedef AkHashList<AkGameObjectID, AkUInt32, MonitorPoolDefault> AkMapGameObjectProfile;

    virtual ~AkMonitor();

	//Singleton
	static AkMonitor* Instance();
	static AkMonitor* Get() { return m_pInstance; }
	static void Destroy();

	// IALMonitor members
    virtual void Register( AK::IALMonitorSink* in_pMonitorSink, AkMonitorData::MaskType in_whatToMonitor );
    virtual void Unregister( AK::IALMonitorSink* in_pMonitorSink );
	virtual void SetMeterWatches( AkMonitorData::MeterWatch* in_pWatches, AkUInt32 in_uiWatchCount );

	static void RecapRegisteredObjects();
	static void RecapRegisteredObjectPositions();
	static void RecapSwitches();
    static void RecapDevices();
    static void RecapStreamRecords();

	static void RecapMemoryPools();
	static void RecapDataSoundBank();
	static void RecapMedia();
	static void RecapEvents();
	static void RecapGameSync();
	static void RecapParamDelta();
	
	// AkMonitor members
	AKRESULT StartMonitoring();
	void StopMonitoring();

	static bool IsMonitoring() { return !Get()->m_sink2Filter.IsEmpty(); }
	static AkMonitorData::MaskType GetNotifFilter() { return Get()->m_uiNotifFilter; } // what is being monitored
	static bool GetAndClearMeterWatchesDirty() { bool bDirty = Get()->m_bMeterWatchesDirty; Get()->m_bMeterWatchesDirty = false; return bDirty; }

	static void PostChangedGameObjPositions();

	static void AddChangedGameObject(AkGameObjectID in_GameObject);

	static AkTimeMs GetThreadTime(){return m_ThreadTime;}
	static void SetThreadTime(AkTimeMs in_ThreadTime){m_ThreadTime = in_ThreadTime;}

	static void Monitor_PostCode( AK::Monitor::ErrorCode in_eErrorCode, AK::Monitor::ErrorLevel in_eErrorLevel, AkPlayingID in_playingID = AK_INVALID_PLAYING_ID, AkGameObjectID in_gameObjID = AK_INVALID_GAME_OBJECT, AkUniqueID in_soundID = AK_INVALID_UNIQUE_ID, bool in_bIsBus = false );
	static void Monitor_PostCodeWithParam( AK::Monitor::ErrorCode in_eErrorCode, AK::Monitor::ErrorLevel in_eErrorLevel, AkUInt32 in_param1, AkPlayingID in_playingID = AK_INVALID_PLAYING_ID, AkGameObjectID in_gameObjID = AK_INVALID_GAME_OBJECT, AkUniqueID in_soundID = AK_INVALID_UNIQUE_ID, bool in_bIsBus = false );
	static void Monitor_PostPluginError( const char* in_szMsg, AK::Monitor::ErrorLevel in_eErrorLevel, AkPluginID in_pluginTypeID, AkUniqueID in_pluginUniqueID, AkPlayingID in_playingID = AK_INVALID_PLAYING_ID, AkGameObjectID in_gameObjID = AK_INVALID_GAME_OBJECT, AkUniqueID in_soundID = AK_INVALID_UNIQUE_ID, bool in_bIsBus = false );
#ifdef AK_SUPPORT_WCHAR	
	static void Monitor_PostString(const wchar_t* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel, AkPlayingID in_playingID, AkGameObjectID in_gameObjID, AkUniqueID in_soundID = AK_INVALID_UNIQUE_ID, bool in_bIsBus = false);
	static void Monitor_AuthoringLog(const wchar_t* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel, AkPlayingID in_playingID = AK_INVALID_PLAYING_ID, AkGameObjectID in_gameObjID = AK_INVALID_GAME_OBJECT, AkUniqueID in_soundID = AK_INVALID_UNIQUE_ID, bool in_bIsBus = false, bool in_bForceLog = false);
#endif //AK_SUPPORT_WCHAR	
	static void Monitor_PostString(const char* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel, AkPlayingID in_playingID, AkGameObjectID in_gameObjID, AkUniqueID in_soundID = AK_INVALID_UNIQUE_ID, bool in_bIsBus = false);
	static void Monitor_AuthoringLog(const char* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel, AkPlayingID in_playingID = AK_INVALID_PLAYING_ID, AkGameObjectID in_gameObjID = AK_INVALID_GAME_OBJECT, AkUniqueID in_soundID = AK_INVALID_UNIQUE_ID, bool in_bIsBus = false, bool in_bForceLog = false);
	static void Monitor_ObjectNotif( AkPlayingID in_PlayingID, AkGameObjectID in_GameObject, const AkCustomParamType & in_CustomParam, AkMonitorData::NotificationReason in_eNotifReason, AkCntrHistArray in_cntrHistArray, AkUniqueID in_targetObjectID, bool in_bTargetIsBus, AkTimeMs in_timeValue, AkUniqueID in_playlistItemID = AK_INVALID_UNIQUE_ID );
	static void Monitor_ObjectBusNotif(AkGameObjectID in_GameObject, AkMonitorData::NotificationReason in_eNotifReason, AkUniqueID in_targetObjectID);
	static void Monitor_MarkersNotif( AkPlayingID in_PlayingID, AkGameObjectID in_GameObject, const AkCustomParamType & in_CustomParam, AkMonitorData::NotificationReason in_eNotifReason, AkCntrHistArray in_cntrHistArray, const char* in_strLabel );
	static void Monitor_BankNotif(AkUniqueID in_BankID, AkUniqueID in_LanguageID, AkMonitorData::NotificationReason in_eNotifReason, AkUInt32 in_uPrepareFlags, const char* pszBankFileName=NULL);
	static void Monitor_PrepareNotif( AkMonitorData::NotificationReason in_eNotifReason, AkUniqueID in_GameSyncorEventID, AkUInt32 in_groupID, AkGroupType in_GroupType, AkUInt32 in_NumEvents );
	static void Monitor_StateChanged( AkStateGroupID in_StateGroup, AkStateID in_PreviousState, AkStateID in_NewState );
	static void Monitor_SwitchChanged( AkSwitchGroupID in_SwitchGroup, AkSwitchStateID in_Switch, AkGameObjectID in_GameObject );
	static void Monitor_ObjectRegistration(AkUInt8 in_uRegistrationFlags, AkGameObjectID in_GameObject, void * in_pMonitorData, bool in_bRecap = false);
	static void Monitor_FreeString( void * in_pMonitorData );
	static void Monitor_ParamChanged( AkMonitorData::NotificationReason in_eNotifReason, AkUniqueID in_ElementID, bool in_bIsBusElement, AkGameObjectID in_GameObject );
	static void Monitor_EventTriggered( AkPlayingID in_PlayingID, AkUniqueID in_EventID, AkGameObjectID in_GameObject, const AkCustomParamType & in_CustomParam);
	static void Monitor_ActionDelayed( AkPlayingID in_PlayingID, AkUniqueID in_ActionID, AkGameObjectID in_GameObject, AkTimeMs in_DelayTime, const AkCustomParamType & in_CustomParam );
	static void Monitor_ActionTriggered( AkPlayingID in_PlayingID, AkUniqueID in_ActionID, AkGameObjectID in_GameObject, const AkCustomParamType & in_CustomParam );
	static void Monitor_BusNotification( AkUniqueID in_BusID, AkMonitorData::BusNotification in_NotifReason, AkUInt32 in_bitsFXBypass, AkUInt32 in_bitsMask );
	static void Monitor_PathEvent( AkPlayingID _playingI_D, AkUniqueID _who_, AkMonitorData::AkPathEvent _event_, AkUInt32 _index_);
	static void Monitor_MusicSwitchTransNotif( AkPlayingID in_PlayingID, AkGameObjectID in_GameObject, AkMonitorData::NotificationReason in_eNotifReason, AkUInt32 in_transRuleIndex, AkUniqueID in_ownerID, AkUniqueID in_nodeSrcID, AkUniqueID in_nodeDestID, AkUniqueID in_segmentSrcID, AkUniqueID in_segmentDestID, AkUniqueID in_cueSrc, AkUniqueID in_cueDest, AkTimeMs in_time );
	static void Monitor_MusicPlaylistTransNotif( AkPlayingID in_PlayingID, AkGameObjectID in_GameObject, AkMonitorData::NotificationReason in_eNotifReason, AkUInt32 in_transRuleIndex, AkUniqueID in_ownerID,AkUniqueID in_segmentSrcID, AkUniqueID in_segmentDestID, AkTimeMs in_time );
	static void Monitor_MusicTrackTransNotif( AkPlayingID in_PlayingID, AkGameObjectID in_GameObject, AkMonitorData::NotificationReason in_eNotifReason, AkUniqueID in_musicTrackID, AkUniqueID in_switchGroupID, AkUniqueID in_prevSwitchID, AkUniqueID in_nextSwitchID, AkTimeMs in_time );
	static void Monitor_Api_Functions(AkLockLessMsgQueue* in_MsgQueue, const int& in_iEndOfListCount);
	static void Monitor_Api_Function(AkQueuedMsg* in_message);
	static void Monitor_TimeStamp(bool in_bForceLog = false);
	static void Monitor_EndOfFrame();
	static void Monitor_Init();
	
#ifdef AK_SUPPORT_WCHAR
	static void Monitor_errorMsg2( const wchar_t* in_psz1, const wchar_t* in_psz2 );
#endif //AK_SUPPORT_WCHAR	
	static void Monitor_errorMsg2( const char* in_psz1, const char* in_psz2 );

	static void Monitor_LoadedBank( CAkUsageSlot* in_pUsageSlot, bool in_bIsDestroyed );
	static void Monitor_MediaPrepared( AkMediaEntry& in_rMediaEntry );
	static void Monitor_EventPrepared( AkUniqueID in_EventID, AkUInt32 in_RefCount );
	static void Monitor_GameSync(  AkUniqueID in_GroupID, AkUniqueID in_GameSyncID, bool in_bIsEnabled, AkGroupType in_GroupType  );

#ifdef AK_SUPPORT_WCHAR
	static void Monitor_SetPoolName( AkMemPoolId in_PoolId, const wchar_t * in_tcsPoolName );
#endif //AK_SUPPORT_WCHAR	
	static void Monitor_SetPoolName( AkMemPoolId in_PoolId, const char * in_tcsPoolName );

	static void Monitor_SetParamNotif_Float( AkMonitorData::NotificationReason in_eNotifReason, AkUniqueID in_ElementID, bool in_bIsBusElement, AkGameObjectID in_GameObject, AkReal32 in_TargetValue, AkValueMeaning in_ValueMeaning, AkTimeMs in_TransitionTime );
	static void Monitor_Dialogue( AkMonitorData::MonitorDataType in_type, AkUniqueID in_idDialogueEvent, AkUniqueID in_idObject, AkUInt32 in_cPath, AkArgumentValueID * in_pPath, AkPlayingID in_idSequence, AkUInt16 in_uRandomChoice, AkUInt16 in_uTotalProbability, AkUInt8 in_uWeightedDecisionType, AkUInt32 in_uWeightedPossibleCount, AkUInt32 in_uWeightedTotalCount );
	static void Monitor_PluginSendData(void * in_pData, AkUInt32 in_uDataSize, AkUniqueID in_audioNodeID, AkGameObjectID in_gameObjectID, AkPluginID in_pluginID, AkUInt32 in_uFXIndex);

	static void Monitor_MidiTargetNotif( AkPlayingID in_PlayingID, AkGameObjectID in_GameObject, AkMonitorData::NotificationReason in_eNotifReason, AkUniqueID in_musicTrackID, AkUniqueID in_midiTargetID, AkTimeMs in_time );

	static void Monitor_MidiEvent( AkMidiEventEx in_midiEv, AkGameObjectID in_GameObject, AkUniqueID in_musicTrackID, AkUniqueID in_midiTargetID );

	static void Monitor_ExternalSourceData(AkPlayingID in_idPlay, AkGameObjectID in_idGameObj, AkUniqueID in_idSrc, const AkOSChar* in_pszFile);

	static void * Monitor_AllocateGameObjNameString( AkGameObjectID in_GameObject, const char* in_GameObjString );

	static inline void SetLocalOutput( AkUInt32 in_uErrorLevel, AK::Monitor::LocalOutputFunc in_pMonitorFunc ) { m_uLocalOutputErrorLevel = in_uErrorLevel; m_funcLocalOutput = in_pMonitorFunc; }

	static AkUInt8 GetMeterWatchDataTypes( AkUniqueID in_busID );

	void Recap( AkMonitorData::MaskType newMonitorTypes );

	AKRESULT StartProfilerCapture( const AkOSChar* in_szFilename );
	AKRESULT StopProfilerCapture();

	static void ResetOutOfMemoryMsgGate();
	static void MonitorAllocFailure(const char * in_szPoolName, size_t in_uFailedAllocSize, AkUInt64 in_uTotalMappedMemory, AkUInt64 in_uMemoryLimit);
	bool DispatchNotification();

	static void MonitorQueueFull(size_t in_uFailedAllocSize);
	inline void * BeginWrite(AkInt32 in_lSize) { return m_ringItems.BeginWrite(in_lSize); }
	inline void EndWrite(void * in_pWritePtr, AkInt32 in_lSize) { return m_ringItems.EndWrite(in_pWritePtr, in_lSize, m_hMonitorEvent); }
	inline void SignalNotifyEvent() { AkSignalEvent(m_hMonitorEvent); };
	
	inline static void SetDeltaRecap() { m_bRecapDeltaMonitor = true; }
	inline static bool NeedsDeltaRecap() { return m_bRecapDeltaMonitor && IsMonitoring() && (GetNotifFilter() & AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataParamDelta)); }
protected:
	//Singleton
	AkMonitor();

	// Helpers.
	static void Monitor_SendErrorData1( AK::Monitor::ErrorCode in_eErrorCode, AK::Monitor::ErrorLevel in_eErrorLevel, AkUInt32 in_param1, AkPlayingID in_playingID, AkGameObjectID in_gameObjID, AkUniqueID in_soundID, bool in_bIsBus );
	static void Monitor_PostToLocalOutput( AK::Monitor::ErrorCode in_eErrorCode, AK::Monitor::ErrorLevel in_eErrorLevel, const AkOSChar * in_pszError, AkPlayingID in_playingID, AkGameObjectID in_gameObjID );

private:

	AkEvent m_hMonitorEvent;
	AkEvent m_hMonitorDoneEvent;
	bool	m_bStopThread;
	bool	m_bMayPostOutOfMemoryMsg;
	static bool	m_bRecapDeltaMonitor;

	friend struct AkMonitorDataCreator;
	friend struct AkProfileDataCreator;
	friend struct AkVarLenDataCreator;
	friend struct AkMeterDataCreator;

    static void StartStreamProfiler( );
    static void StopStreamProfiler( );

	static void RecapGroupHelper(CAkStateMgr::PreparationGroups& in_Groups, AkGroupType in_Type );

	static AkTimeMs m_ThreadTime;

    static AK_DECLARE_THREAD_ROUTINE( MonitorThreadFunc );

	static AkMonitor* m_pInstance; // Pointer on the unique Monitor instance

	static AkThread		m_hThread;	

    typedef CAkKeyList<AK::IALMonitorSink*, AkMonitorData::MaskType, AkFixedSize, AkMemID_Profiler> MonitorSink2Filter;
	MonitorSink2Filter m_sink2Filter;

	CAkLock m_registrationLock;
	
	AkChunkRing m_ringItems;

	AK::MonitorSerializer m_serializer;

	AkMonitorData::MaskType m_uiNotifFilter; // Global filter 

	bool m_bMeterWatchesDirty;

	class AkLocalProfilerCaptureSink * m_pLocalProfilerCaptureSink;

	static AkMapGameObjectProfile m_mapGameObjectChanged;

	typedef AkHashList<AkUniqueID/*bus id*/, AkUInt8 /*bitmask of BusMeterDataType*/, MonitorPoolDefault> AkMeterWatchMap;
	static AkMeterWatchMap m_meterWatchMap;

	static AkUInt32 m_uLocalOutputErrorLevel; // Bitfield of AK::Monitor::ErrorLevel
	static AK::Monitor::LocalOutputFunc m_funcLocalOutput;
};

// Use this to send a monitor data item: space is initialized in constructor, 
// item is 'queued' in destructor.
struct AkMonitorDataCreator
{
	AkMonitorDataCreator(AkMonitorData::MonitorDataType in_MonitorDataType, AkInt32 in_lSize, bool in_bIsTraceableError = false);
	~AkMonitorDataCreator();

	AkMonitorData::MonitorDataItem * m_pData;

private:
	AkInt32 m_lSize;
	bool m_bOfflineTracking;
	static bool m_bIsLogging;
};

// Use this to send a profiling data item: space is initialized in constructor, 
// item is 'queued' in destructor. The difference between this and AkMonitorDataCreator
// is that this one doesn't block on a full queue, it just skips the item (as profiling info
// is not 'critical')
struct AkProfileDataCreator
{
	AkProfileDataCreator( AkInt32 in_lSize );
	~AkProfileDataCreator();

	AkMonitorData::MonitorDataItem * m_pData;

private:
	AkInt32 m_lSize;
};

struct AkVarLenDataCreator
{
	AkVarLenDataCreator( AkMonitorData::MonitorDataType in_type );
	~AkVarLenDataCreator();

	AK::MonitorSerializer& GetSerializer() { return m_serializer; }

	void OutOfMemory( int in_iFailedAllocSize );
	void Post();

private:
	AK::MonitorSerializer& m_serializer;
	AkMonitorData::MonitorDataType m_type;
};

typedef AkArray<AkResourceMonitorCallbackFunc, AkResourceMonitorCallbackFunc&> ResourceMonitorFuncArray;

class AkResourceMonitor
{
public:
	static const AkResourceMonitorDataSummary* GetDataSummary() { return &m_DataSummary; }
	static void Destroy();
	static bool HasSubscribers();
	static AKRESULT Register(AkResourceMonitorCallbackFunc in_pCallback);
	static AKRESULT Unregister(AkResourceMonitorCallbackFunc in_pCallback);
	static void Callback();

	static void SetTotalCPU(AkReal32 in_uTotalCPU) { m_DataSummary.totalCPU = in_uTotalCPU; }
	static void SetPluginCPU(AkReal32 in_uPluginCPU) { m_DataSummary.pluginCPU = in_uPluginCPU; }
	static void SetPhysicalVoices(AkUInt32 in_uPhysicalVoices) { m_DataSummary.physicalVoices = in_uPhysicalVoices; }
	static void SetVirtualVoices(AkUInt32 in_uVirtualVoices) { m_DataSummary.virtualVoices = in_uVirtualVoices; }
	static void SetTotalVoices(AkUInt32 in_uTotalVoices) { m_DataSummary.totalVoices = in_uTotalVoices; }
	static void SetNbActiveEvents(AkUInt32 in_uNbActiveEvents) { m_DataSummary.nbActiveEvents = in_uNbActiveEvents; }

private:
	static AkResourceMonitorDataSummary m_DataSummary;
	static ResourceMonitorFuncArray aFuncs;
};

//Please use macros

//The monitor is started and Stopped no matter the build type

#ifndef AK_OPTIMIZED

#define MONITOR_BANKNOTIF( in_BankID, in_LanguageID, in_eNotifReason )\
		AkMonitor::Monitor_BankNotif( in_BankID, in_LanguageID, in_eNotifReason, 0, NULL )

#define MONITOR_BANKNOTIF_NAME( in_BankID, in_LanguageID, in_eNotifReason, in_pBanksNameStr )\
		AkMonitor::Monitor_BankNotif( in_BankID, in_LanguageID, in_eNotifReason, 0, in_pBanksNameStr )

#define MONITOR_PREPAREEVENTNOTIF( in_eNotifReason, in_EventID )\
		AkMonitor::Monitor_PrepareNotif( in_eNotifReason, in_EventID, 0, AkGroupType_Switch, 0 )

#define MONITOR_PREPARENOTIFREQUESTED( in_eNotifReason, in_NumNotifs )\
		AkMonitor::Monitor_PrepareNotif( in_eNotifReason, 0, 0, AkGroupType_Switch, in_NumNotifs )

#define MONITOR_PREPAREGAMESYNCNOTIF( in_eNotifReason, in_GameSyncID, in_GroupID, in_GroupType )\
		AkMonitor::Monitor_PrepareNotif( in_eNotifReason, in_GameSyncID, in_GroupID, in_GroupType, 0 )

#define MONITOR_PREPAREBANKREQUESTED( in_eNotifReason, in_BankID, in_uPrepareFlags )\
		AkMonitor::Monitor_BankNotif( in_BankID, AK_INVALID_UNIQUE_ID, in_eNotifReason, in_uPrepareFlags, NULL )

#define MONITOR_STATECHANGED(in_StateGroup, in_PreviousState, in_NewState)\
		AkMonitor::Monitor_StateChanged(in_StateGroup, in_PreviousState, in_NewState)

#define MONITOR_SWITCHCHANGED( in_SwitchGroup, in_Switch, in_GameObject )\
		AkMonitor::Monitor_SwitchChanged( in_SwitchGroup, in_Switch, in_GameObject )

#define MONITOR_OBJREGISTRATION( in_uRegistrationFlags, in_GameObject, in_GameObjString )\
		AkMonitor::Monitor_ObjectRegistration( in_uRegistrationFlags, in_GameObject, in_GameObjString )

#define MONITOR_OBJREGISTRATION_2( in_pGO )\
		AkMonitor::Monitor_ObjectRegistration( AkMonitorData::MakeObjRegFlags(in_pGO->IsRegistered(), in_pGO->IsSoloExplicit(), in_pGO->IsSoloImplicit(), in_pGO->IsMute(), false), in_pGO->ID(), NULL, false );

#define MONITOR_FREESTRING( in_GameObjString )\
		AkMonitor::Monitor_FreeString( in_GameObjString )

#define MONITOR_OBJECTNOTIF( in_PlayingID, in_GameObject, in_CustomParam, in_eNotifReason, in_cntrHistArray, in_targetObjectID, in_bTargetIsBus, in_timeValue )\
		AkMonitor::Monitor_ObjectNotif( in_PlayingID, in_GameObject, in_CustomParam, in_eNotifReason, in_cntrHistArray, in_targetObjectID, in_bTargetIsBus, in_timeValue )

#define MONITOR_OBJECTBUSNOTIF( in_GameObject, in_eNotifReason, in_targetObjectID )\
		AkMonitor::Monitor_ObjectBusNotif( in_GameObject, in_eNotifReason, in_targetObjectID )

#define MONITOR_MUSICOBJECTNOTIF( in_PlayingID, in_GameObject, in_CustomParam, in_eNotifReason, in_targetObjectID, in_playlistItemID )\
		AkMonitor::Monitor_ObjectNotif( in_PlayingID, in_GameObject, in_CustomParam, in_eNotifReason, CAkCntrHist(), in_targetObjectID, false, 0, in_playlistItemID );

#define MONITOR_EVENTENDREACHEDNOTIF( in_PlayingID, in_GameObject, in_CustomParam, in_EventID_ )\
		AkMonitor::Monitor_ObjectNotif( in_PlayingID, in_GameObject, in_CustomParam, AkMonitorData::NotificationReason_EventEndReached, CAkCntrHist(), false, 0, 0 );

#define MONITOR_EVENTMARKERNOTIF( in_PlayingID, in_GameObject, in_CustomParam, in_EventID_, in_strLabel )\
		AkMonitor::Monitor_MarkersNotif( in_PlayingID, in_GameObject, in_CustomParam, AkMonitorData::NotificationReason_EventMarker, CAkCntrHist(), in_strLabel );

#define MONITOR_PARAMCHANGED( in_eNotifReason, in_ElementID, in_bIsBusElement, in_GameObject )\
		AkMonitor::Monitor_ParamChanged( in_eNotifReason, in_ElementID, in_bIsBusElement, in_GameObject )

#define MONITOR_SETPARAMNOTIF_FLOAT( in_eNotifReason, in_ElementID, in_bIsBusElement, in_GameObject, in_TargetValue, in_ValueMeaning, in_TransitionTime ) \
		AkMonitor::Monitor_SetParamNotif_Float( in_eNotifReason, in_ElementID, in_bIsBusElement, in_GameObject, in_TargetValue, in_ValueMeaning, in_TransitionTime )

#define MONITOR_ERROR( in_ErrorCode )\
		AkMonitor::Monitor_PostCode( in_ErrorCode, AK::Monitor::ErrorLevel_Error )

#define MONITOR_ERRORMESSAGE( in_ErrorCode )\
		AkMonitor::Monitor_PostCode( in_ErrorCode, AK::Monitor::ErrorLevel_Message )

#define MONITOR_MSGEX( in_ErrorCode, in_PlayingID, in_GameObject, in_WwiseObjectID, in_WwiseObjectIsBus )\
		AkMonitor::Monitor_PostCode( in_ErrorCode, AK::Monitor::ErrorLevel_Message, in_PlayingID, in_GameObject, in_WwiseObjectID )

#define MONITOR_ERROREX( in_ErrorCode, in_PlayingID, in_GameObject, in_WwiseObjectID, in_WwiseObjectIsBus )\
		AkMonitor::Monitor_PostCode( in_ErrorCode, AK::Monitor::ErrorLevel_Error, in_PlayingID, in_GameObject, in_WwiseObjectID )

#define MONITOR_ERROR_PARAM( in_ErrorCode, in_param1, in_PlayingID, in_GameObject, in_WwiseObjectID, in_WwiseObjectIsBus )\
		AkMonitor::Monitor_PostCodeWithParam( in_ErrorCode, AK::Monitor::ErrorLevel_Error, in_param1, in_PlayingID, in_GameObject, in_WwiseObjectID, in_WwiseObjectIsBus )

#define MONITOR_SOURCE_ERROR( in_ErrorCode, in_pContext )\
		if ( in_pContext ) {	\
			AkSrcTypeInfo * _pSrcType = in_pContext->GetSrcTypeInfo();	\
			AkMonitor::Monitor_PostCodeWithParam( in_ErrorCode, AK::Monitor::ErrorLevel_Error, _pSrcType->mediaInfo.sourceID, in_pContext->GetPlayingID(), in_pContext->GetGameObjectPtr()->ID(), in_pContext->GetSoundID(), false ); \
		}

#define MONITOR_PLUGIN_ERROR( in_ErrorCode, in_pContext, in_pluginID )\
		AkMonitor::Monitor_PostCodeWithParam( in_ErrorCode, AK::Monitor::ErrorLevel_Error, in_pluginID, in_pContext->GetPlayingID(), in_pContext->GetGameObjectPtr()->ID(), in_pContext->GetSoundID(), false );

#define MONITOR_BUS_PLUGIN_ERROR( in_ErrorCode, in_pBusContext, in_pluginID )\
		AkMonitor::Monitor_PostCodeWithParam( in_ErrorCode, AK::Monitor::ErrorLevel_Error, in_pluginID, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, in_pBusContext->ID(), true );

#define MONITOR_PLUGIN_MESSAGE( in_szMsg, in_ErrorLevel, in_playingID, in_gameObjID, in_soundID, in_pluginTypeID, in_pluginUniqueID ) \
		AkMonitor::Monitor_PostPluginError( in_szMsg, in_ErrorLevel, in_pluginTypeID, in_pluginUniqueID, in_playingID, in_gameObjID, in_soundID, false );

#define MONITOR_BUS_PLUGIN_MESSAGE( in_szMsg, in_ErrorLevel, in_busID, in_pluginTypeID, in_pluginUniqueID ) \
		AkMonitor::Monitor_PostPluginError( in_szMsg, in_ErrorLevel, in_pluginTypeID, in_pluginUniqueID, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, in_busID, true ); \

#define MONITOR_EVENTTRIGGERED( in_PlayingID, in_EventID, in_GameObject, in_CustomParam )\
		AkMonitor::Monitor_EventTriggered( in_PlayingID, in_EventID, in_GameObject, in_CustomParam )

#define MONITOR_ACTIONDELAYED( in_PlayingID, in_ActionID, in_GameObject, in_DelayTime, in_CustomParam )	\
		AkMonitor::Monitor_ActionDelayed( in_PlayingID, in_ActionID, in_GameObject, in_DelayTime, in_CustomParam )

#define MONITOR_ACTIONTRIGGERED( in_PlayingID, in_ActionID, in_GameObject, in_CustomParam )	\
		AkMonitor::Monitor_ActionTriggered( in_PlayingID, in_ActionID, in_GameObject, in_CustomParam )

#define MONITOR_BUSNOTIFICATION( in_BusID, in_NotifReason, in_bitsFXBypass, in_bitsMask )\
		AkMonitor::Monitor_BusNotification( in_BusID, in_NotifReason, in_bitsFXBypass, in_bitsMask )	

#define MONITOR_PATH_EVENT( _playingID_, _who_, _event_, _index_ )\
		AkMonitor::Monitor_PathEvent( _playingID_, _who_, _event_, _index_ )

#define MONITOR_ERRORMSG2( _MSG1_, _MSG2_ )\
		AkMonitor::Monitor_errorMsg2( _MSG1_, _MSG2_ )

#define MONITOR_ERRORMSG_PLAYINGID( _MSG1_, _PLAYINGID_ )\
		AkMonitor::Monitor_PostString( _MSG1_, AK::Monitor::ErrorLevel_Error, _PLAYINGID_, AK_INVALID_GAME_OBJECT )

#define MONITOR_ERRORMSG( _MSG1_ )\
		AkMonitor::Monitor_PostString( _MSG1_, AK::Monitor::ErrorLevel_Error, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT )

#define MONITOR_MSG( _MSG_ )\
		AkMonitor::Monitor_PostString( _MSG_, AK::Monitor::ErrorLevel_Message, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT )

#define MONITOR_LOADEDBANK( _BankSlot_, _IsDestroyed_ )\
		AkMonitor::Monitor_LoadedBank( _BankSlot_, _IsDestroyed_ )

#define MONITOR_MEDIAPREPARED( _MediaItem_ )\
		AkMonitor::Monitor_MediaPrepared( _MediaItem_ )

#define MONITOR_EVENTPREPARED( _EventID_, _RefCount_ )\
		AkMonitor::Monitor_EventPrepared( _EventID_, _RefCount_ )

#define MONITOR_GAMESYNC( _GroupID_, _SyncID_, _IsEnabled_, _SyncType_ )\
		AkMonitor::Monitor_GameSync( _GroupID_, _SyncID_, _IsEnabled_, _SyncType_ )

#define MONITOR_MUSICSWITCHTRANS( in_PlayingID, in_GameObject, in_eNotifReason, in_transRuleIndex, in_switchCntrID, in_nodeSrcID, in_nodeDestID, in_segmentSrcID, in_segmentDestID, in_cueSrc, in_cueDest, in_time )\
		AkMonitor::Monitor_MusicSwitchTransNotif( in_PlayingID, in_GameObject, in_eNotifReason, in_transRuleIndex, in_switchCntrID, in_nodeSrcID, in_nodeDestID, in_segmentSrcID, in_segmentDestID, in_cueSrc, in_cueDest, in_time )

#define MONITOR_MUSICTRACKTRANS( in_PlayingID, in_GameObject, in_eNotifReason, in_musicTrackID, in_switchGroupID, in_prevSwitchID, in_nextSwitchID, in_time )\
	AkMonitor::Monitor_MusicTrackTransNotif( in_PlayingID, in_GameObject, in_eNotifReason, in_musicTrackID, in_switchGroupID, in_prevSwitchID, in_nextSwitchID, in_time )

#define MONITOR_MUSICPLAYLISTTRANS( in_PlayingID, in_GameObject, in_eNotifReason, in_transRuleIndex, in_playlistCntrID, in_segmentSrcID, in_segmentDestID, in_time )\
	AkMonitor::Monitor_MusicPlaylistTransNotif( in_PlayingID, in_GameObject, in_eNotifReason, in_transRuleIndex, in_playlistCntrID, in_segmentSrcID, in_segmentDestID, in_time )

#define MONITOR_PLUGINSENDDATA( in_pData, in_uDataSize, in_audioNodeID, in_gameObjectID, in_pluginID, in_uFXIndex )\
		AkMonitor::Monitor_PluginSendData( in_pData, in_uDataSize, in_audioNodeID, in_gameObjectID, in_pluginID, in_uFXIndex );

#define MONITOR_RESOLVEDIALOGUE( in_idDialogueEvent, in_idResolved, in_cPath, in_pPath, in_idSequence, in_uRandomChoice, in_uTotalProbability, in_uWeightedDecisionType, in_uWeightedPossibleCount, in_uWeightedTotalCount )\
	AkMonitor::Monitor_Dialogue( AkMonitorData::MonitorDataResolveDialogue, in_idDialogueEvent, in_idResolved, in_cPath, in_pPath, in_idSequence, in_uRandomChoice, in_uTotalProbability, in_uWeightedDecisionType, in_uWeightedPossibleCount, in_uWeightedTotalCount )

#define MONITOR_MIDITARGET( in_PlayingID, in_GameObject, in_eNotifReason, in_musicTrackID, in_midiTargetID, in_time )\
	AkMonitor::Monitor_MidiTargetNotif( in_PlayingID, in_GameObject, in_eNotifReason, in_musicTrackID, in_midiTargetID, in_time )

#define MONITOR_MIDIEVENT( in_midiEvent, in_GameObject, in_musicTrackID, in_midiTargetID)\
	AkMonitor::Monitor_MidiEvent( in_midiEvent, in_GameObject, in_musicTrackID, in_midiTargetID)

#define MONITOR_TEMPLATESOURCE( _in_idPlay, _in_idGameObj, _in_idSrc, _in_szFile ) \
	AkMonitor::Monitor_ExternalSourceData( _in_idPlay, _in_idGameObj, _in_idSrc, _in_szFile )

#define MONITOR_API_FUNCTION_CALL(_in_message)\
	AkMonitor::Monitor_Api_Function(_in_message)


#else

#define MONITOR_BANKNOTIF( in_BankID, in_LanguageID, in_eNotifReason )

#define  MONITOR_BANKNOTIF_NAME( in_BankID, in_LanguageID, in_eNotifReason, in_name )

#define MONITOR_PREPAREEVENTNOTIF( in_eNotifReason, in_EventID )

#define MONITOR_PREPARENOTIFREQUESTED( in_eNotifReason, in_NumNotifs )

#define MONITOR_PREPAREGAMESYNCNOTIF( in_eNotifReason, in_GameSyncID, in_GroupID, in_GroupType )

#define MONITOR_PREPAREBANKREQUESTED( in_eNotifReason, in_BankID, in_uPrepareFlags )

#define MONITOR_STATECHANGED(in_StateGroup, in_PreviousState, in_NewState)

#define MONITOR_SWITCHCHANGED( in_SwitchGroup, in_Switch, in_GameObject )

#define MONITOR_OBJREGISTRATION( in_uRegistrationFlags, in_GameObject, in_GameObjString )

#define MONITOR_OBJREGISTRATION_2( in_pGO )

#define MONITOR_FREESTRING( in_GameObjString )

#define MONITOR_OBJECTNOTIF( in_PlayingID, in_GameObject, in_CustomParam, in_eNotifReason, in_cntrHistArray, in_targetObjectID, in_bTargetIsBus, in_timeValue )

#define MONITOR_OBJECTBUSNOTIF( in_GameObject, in_eNotifReason, in_targetObjectID)

#define MONITOR_MUSICOBJECTNOTIF( in_PlayingID, in_GameObject, in_CustomParam, in_eNotifReason, in_targetObjectID, in_playlistItemID )

#define MONITOR_EVENTENDREACHEDNOTIF( in_PlayingID, in_GameObject, in_CustomParam, in_EventID_  )

#define MONITOR_EVENTMARKERNOTIF( in_PlayingID, in_GameObject, in_CustomParam, in_EventID_, in_strLabel  )

#define MONITOR_PARAMCHANGED( in_eNotifReason, in_ElementID, in_bIsBusElement, in_GameObject )

#define MONITOR_SETPARAMNOTIF_FLOAT( in_eNotifReason, in_ElementID, in_bIsBusElement, in_GameObject, in_TargetValue, in_ValueMeaning, in_TransitionTime ) 

#define MONITOR_ERROR( in_ErrorCode )

#define MONITOR_ERRORMESSAGE( in_ErrorCode )

#define MONITOR_MSGEX( in_ErrorCode, in_PlayingID, in_GameObject, in_WwiseObjectID, in_WwiseObjectIsBus )

#define MONITOR_ERROREX( in_ErrorCode, in_PlayingID, in_GameObject, in_WwiseObjectID, in_WwiseObjectIsBus )

#define MONITOR_ERROR_PARAM( in_ErrorCode, in_param1, in_PlayingID, in_GameObject, in_WwiseObjectID, in_WwiseObjectIsBus )

#define MONITOR_SOURCE_ERROR( in_ErrorCode, in_pContext )

#define MONITOR_PLUGIN_ERROR( in_ErrorCode, in_pContext, in_pluginID )

#define MONITOR_BUS_PLUGIN_ERROR( in_ErrorCode, in_pBusContext, in_pluginID )

#define MONITOR_PLUGIN_MESSAGE( in_szMsg, in_ErrorLevel, in_playingID, in_gameObjID, in_soundID, in_pluginTypeID, in_pluginUniqueID )

#define MONITOR_BUS_PLUGIN_MESSAGE( in_szMsg, in_ErrorLevel, in_busID, in_pluginTypeID, in_pluginUniqueID )

#define MONITOR_EVENTTRIGGERED( in_PlayingID, in_EventID, in_GameObject, in_CustomParam )

#define MONITOR_ACTIONDELAYED( in_PlayingID, in_ActionID, in_GameObject, in_DelayTime, in_CustomParam )

#define MONITOR_ACTIONTRIGGERED( in_PlayingID, in_ActionID, in_GameObject, in_CustomParam )

#define MONITOR_BUSNOTIFICATION( in_BusID, in_NotifReason, in_bitsFXBypass, in_bitsMask )

#define MONITOR_PATH_EVENT( _playingID_, _who_, _event_, _index_ )

#define MONITOR_ERRORMSG2( _MSG1_, _MSG2_ )

#define MONITOR_ERRORMSG_PLAYINGID( _MSG1_, _PLAYINGID_ )

#define MONITOR_ERRORMSG( _MSG1_ )

#define MONITOR_MSG( _MSG_ )

#define MONITOR_LOADEDBANK( _BankSlot_, _IsDestroyed_ )

#define MONITOR_MEDIAPREPARED( _MediaItem_ )

#define MONITOR_EVENTPREPARED( _EventID_, _RefCount_ )

#define MONITOR_GAMESYNC( _GroupID_, _SyncID_, _IsEnabled_, _SyncType_ )

#define MONITOR_RESOLVEDIALOGUE( in_idDialogueEvent, in_idResolved, in_cPath, in_pPath, in_idSequence, in_uRandomChoice, in_uTotalProbability, in_uWeightedDecisionType, in_uWeightedPossibleCount, in_uWeightedTotalCount )

#define MONITOR_MUSICSWITCHTRANS( in_PlayingID, in_GameObject, in_eNotifReason, in_transRuleIndex, in_switchCntrID, in_nodeSrcID, in_nodeDestID, in_segmentSrcID, in_segmentDestID, in_cueSrc, in_cueDest, in_time )

#define MONITOR_MUSICTRACKTRANS( in_PlayingID, in_GameObject, in_eNotifReason, in_musicTrackID, in_switchGroupID, in_prevSwitchID, in_nextSwitchID, in_time )

#define MONITOR_MUSICPLAYLISTTRANS( in_PlayingID, in_GameObject, in_eNotifReason, in_transRuleIndex, in_playlistCntrID, in_segmentSrcID, in_segmentDestID, in_time )

#define MONITOR_PLUGINSENDDATA( in_pData, in_uDataSize, in_audioNodeID, in_gameObjectID, in_pluginID, in_uFXIndex )

#define MONITOR_MIDITARGET( in_PlayingID, in_GameObject, in_eNotifReason, in_musicTrackID, in_midiTargetID, in_time )

#define MONITOR_MIDIEVENT( in_midiEvent, in_GameObject, in_musicTrackID, in_midiTargetID)

#define MONITOR_TEMPLATESOURCE( _in_idPlay, _in_idGameObj, _in_idSrc, _in_szFile )

#define MONITOR_API_FUNCTION_CALL(_in_message)

#endif

#endif	// _AKMONITOR_H_
