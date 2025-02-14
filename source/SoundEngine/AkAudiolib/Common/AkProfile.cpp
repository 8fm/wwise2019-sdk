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

/***************************************************************************************************
**
** AkProfile.cpp
**
***************************************************************************************************/
#include "stdafx.h"

#ifndef AK_OPTIMIZED

#include "AkProfile.h"

#include "AkLEngine.h"
#include "AkMath.h"
#include "AkMonitor.h"
#include "AkAudioMgr.h"         // For upper engine access.
#include "AkAudioLibTimer.h"
#include "AkTransitionManager.h"
#include "AkRegistryMgr.h"
#include "AkAudioLib.h"
#include "AkCritical.h"
#include "AkPlayingMgr.h"
#include "AkOutputMgr.h"

#ifdef AK_PS4
	#include "AkACPManager.h"
#endif

#if !defined(AK_OPTIMIZED)
bool g_bOfflineRendering = false;
AkChannelConfig g_offlineSpeakerConfig( 2, AK_SPEAKER_SETUP_STEREO );
#endif

extern void _CallGlobalExtensions(AkGlobalCallbackLocation in_eLocation);

/***************************************************************************************************
**
** AkPerf
**
***************************************************************************************************/

AkInt64 AkPerf::m_iLastUpdateAudio = {0};

AkInt64 AkPerf::m_iLastUpdateMemory = {0};
AkInt64 AkPerf::m_iLastUpdateStreaming = {0};

AkUInt32 AkPerf::m_ulPreparedEvents = 0;
AkUInt64 AkPerf::m_ulBankMemory = 0;
AkUInt64 AkPerf::m_ulPreparedEventMemory = 0;

AkInt32 AkPerf::m_iTicksPerPerfBlock = 0;
AkInt32 AkPerf::m_iNextPerf = 0;
AkInt32 AkPerf::m_iTicksPerCursor = 0;
AkInt32 AkPerf::m_iNextCursor = 0;

void AkPerf::Init()
{
	AkUInt64 iNow;
	AKPLATFORM::PerformanceCounter( (AkInt64*)&iNow );
	m_iLastUpdateAudio = iNow;

	m_iLastUpdateMemory = iNow;
	m_iLastUpdateStreaming = iNow;

	m_iTicksPerPerfBlock = AkMax(AK_PROFILE_PERF_INTERVALS / AK_MS_PER_BUFFER_TICK, 1);	//Approximate one perf packet per 200 ms.
	m_iNextPerf = 0;

	m_iTicksPerCursor = AkMax(m_iTicksPerPerfBlock / 4, 1);	//Have 4 times more cursor updates.
	m_iNextCursor = 0;
}

void AkPerf::Term()
{

}

void AkPerf::TickAudio()
{
	// HasSubscribers() could be true even if GetNotifFilter() is not. We have to check before returning
	if (!AkMonitor::GetNotifFilter())
	{
		// Check before getting the GlobalLock
		if (AkResourceMonitor::HasSubscribers())
		{
			CAkFunctionCritical GlobalLock;
			AkResourceMonitor::SetNbActiveEvents(g_pPlayingMgr->NumPlayingIDs());
		}
		return;
	}

	// Protect access to Upper Engine data -- Might not be necessary, but better be safe.
	CAkFunctionCritical GlobalLock;
	AkResourceMonitor::SetNbActiveEvents(g_pPlayingMgr->NumPlayingIDs());

	////////////////////////////////////////////////////////////////////////
	// Section 1
	// Perform at every iterations:
	////////////////////////////////////////////////////////////////////////

	if( (AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataMeter ) ) )
	{	
		// Send meter watches
		CAkLEngine::PostMeterWatches();
	}

	////////////////////////////////////////////////////////////////////////
	// Section 2
	// Don't do anything if it's not time yet.
	////////////////////////////////////////////////////////////////////////
	AkUInt64 iNow;
	AKPLATFORM::PerformanceCounter( (AkInt64*)&iNow );

	m_iNextPerf--;
	m_iNextCursor--;

	bool bDoAudioPerf = ( (AkMonitor::GetNotifFilter() & 
			( AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataAudioPerf ) 
			| AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataMemoryPool )) 
		) && m_iNextPerf <= 0 ) || g_bOfflineRendering;

	if ( m_iNextCursor > 0 && !bDoAudioPerf && !g_bOfflineRendering)
		return;

	m_iNextCursor = m_iTicksPerCursor;

	// Don't do anything if nobody's listening to perf.
	if ( bDoAudioPerf )
	{
		AkMonitor::ResetOutOfMemoryMsgGate();

		m_iNextPerf = m_iTicksPerPerfBlock;

		AkReal32 fInterval = (AkReal32) ( ( iNow - m_iLastUpdateAudio ) / AK::g_fFreqRatio );
		if (g_bOfflineRendering)
			fInterval = (AkReal32) AK_MS_PER_BUFFER_TICK;

		// Post primary lower thread info.
		{
			AkUInt32 sizeofData = SIZEOF_MONITORDATA( audioPerfData );
			AkProfileDataCreator creator( sizeofData );
			if ( !creator.m_pData )
				return;

			creator.m_pData->eDataType = AkMonitorData::MonitorDataAudioPerf;

			// voice info
			g_pTransitionManager->GetTransitionsUsage( 
				creator.m_pData->audioPerfData.numFadeTransitionsUsed, 
				creator.m_pData->audioPerfData.numStateTransitionsUsed
				);

			// global performance timers
			creator.m_pData->audioPerfData.numRegisteredObjects = (AkUInt16)( g_pRegistryMgr->NumRegisteredObject() );
			creator.m_pData->audioPerfData.numPlayingIDs = (AkUInt16)( g_pPlayingMgr->NumPlayingIDs() );

			creator.m_pData->audioPerfData.timers.fInterval = fInterval;
		
			// Message queue stats
			AkUInt32 maxQueueSizeUsed = g_pAudioMgr->GetMaxSizeQueueFilled();
			creator.m_pData->audioPerfData.uCommandQueueActualSize = maxQueueSizeUsed;
			creator.m_pData->audioPerfData.fCommandQueuePercentageUsed = (maxQueueSizeUsed*100.0f / g_pAudioMgr->GetMaxQueueSize());

			// Update DSP usage
#if defined(AK_PS4)
			creator.m_pData->audioPerfData.fDSPUsage = CAkACPManager::Get().GetDSPUsage();
#else
			creator.m_pData->audioPerfData.fDSPUsage = 0;
#endif

			creator.m_pData->audioPerfData.uNumPreparedEvents	= m_ulPreparedEvents;
			creator.m_pData->audioPerfData.uTotalMemBanks		= m_ulBankMemory;
			creator.m_pData->audioPerfData.uTotalPreparedMemory	= m_ulPreparedEventMemory;
		}

		// Auxiliaries, sorted in order of importance. last ones are more likely to be skipped.

		PostMemoryStats( iNow );
		PostStreamingStats( iNow );
		PostEnvironmentStats();
		PostSendsStats();
		PostObsOccStats();
		PostListenerStats();		
		PostGameObjPositions();

		_CallGlobalExtensions(AkGlobalCallbackLocation_Monitor);
		
		m_iLastUpdateAudio = iNow;
	}

	// This is not perf information, we have to send it to Wwise, even if it is not listening specifically to perfs.
	// It is used to post position of playing segments at regular time intervals.
	PostInteractiveMusicInfo();

	PostSoundInfo();
}

void AkPerf::PostTimers()
{
	// Resource monitor CPU calculation
	if ( AkResourceMonitor::HasSubscribers() )
	{
		AkInt64 iThreadTotal = 0;
		AkInt64 iPluginTotal = 0;

		AkUInt32 uNumItems = AkMin(AkAudiolibTimer::items.Length(), (AkUInt32)AkAudiolibTimer::iItemIdx);
		for (unsigned int i = 0; i < uNumItems; ++i)
		{
			const AkAudiolibTimer::Item & item = AkAudiolibTimer::items[i];
			AkInt64 iInterval = item.iStopTick - item.iStartTick;

			if (item.uPluginID == AkAudiolibTimer::uTimerEventMgrThread)
			{
				iThreadTotal += iInterval;
			}
			else
			{
				iPluginTotal += iInterval;
			}
		}

		float fMsPerBuffer = (float)AK_NUM_VOICE_REFILL_FRAMES / AK_CORE_SAMPLERATE * 1000.f;
		AkResourceMonitor::SetTotalCPU(iThreadTotal / AK::g_fFreqRatio / fMsPerBuffer * 100.f);
		AkResourceMonitor::SetPluginCPU(iPluginTotal / AK::g_fFreqRatio / fMsPerBuffer * 100.f);
	}

	if (AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataCPUTimer))
	{
		AkUInt32 uNumItems = AkMin(AkAudiolibTimer::items.Length(), (AkUInt32)AkAudiolibTimer::iItemIdx);
		AkUInt32 sizeofItem = SIZEOF_MONITORDATA_TO(cpuTimerData.item)
			+ uNumItems * sizeof(AkAudiolibTimer::Item);

		AkProfileDataCreator creator(sizeofItem);
		if (!creator.m_pData)
			return;

		creator.m_pData->eDataType = AkMonitorData::MonitorDataCPUTimer;
		creator.m_pData->cpuTimerData.uNumItems = uNumItems;
		memcpy(creator.m_pData->cpuTimerData.item, AkAudiolibTimer::items.Data(), uNumItems * sizeof(AkAudiolibTimer::Item));
	}

	AkAudiolibTimer::ResetTimers();
}

void AkPerf::PostMemoryStats( AkInt64 in_iNow )
{
	if ( !( AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataMemoryPool ) ) )
		return;

	AkUInt32 sizeofItem = SIZEOF_MONITORDATA_TO(memoryData.poolData) + (AkMemID_NUM * 2) * sizeof(AkMonitorData::MemoryPoolData);

	AkProfileDataCreator creator( sizeofItem );
	if ( !creator.m_pData )
		return;

	creator.m_pData->eDataType = AkMonitorData::MonitorDataMemoryPool;
	creator.m_pData->memoryData.uNumPools = AkMemID_NUM * 2;

	AK::MemoryMgr::GetGlobalStats(creator.m_pData->memoryData.globalData);

	for (AkUInt32 uPool = 0; uPool < AkMemID_NUM * 2; ++uPool)
	{
		AK::MemoryMgr::GetCategoryStats((AkMemPoolId)uPool, creator.m_pData->memoryData.poolData[uPool]);
	}
	m_iLastUpdateMemory = in_iNow;
}

void AkPerf::PostStreamingStats( AkInt64 in_iNow )
{
	if ( !( AkMonitor::GetNotifFilter() & 
			( AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataStreaming )
			| AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataStreamsRecord )
			| AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataDevicesRecord ) ) ) )
		return;

    // ALGORITHM:
    // Try to reserve room needed to send new device records.
    // If succeeded, get them and send IF ANY.
    // Try to reserve room needed to send new streams records.
    // If succeeded, get them and send IF ANY.
    // Try yo reserve room to send stream data.
    // If succeeded, get them and send ALWAYS.

    AKASSERT( AK::IAkStreamMgr::Get( ) );
    AK::IAkStreamMgrProfile * pStmMgrProfile = AK::IAkStreamMgr::Get( )->GetStreamMgrProfile( );
    if ( pStmMgrProfile == NULL )
        return; // Profiling interface not implemented in that stream manager.

    unsigned int uiNumDevices = pStmMgrProfile->GetNumDevices( );
	uiNumDevices = AkMin( AK_MAX_DEVICE_NUM, uiNumDevices );

	class AkDeviceProfileData
	{
	public:
		AkDeviceProfileData() 
			:pDevice( NULL )
		{}

		~AkDeviceProfileData()
		{
			if ( pDevice )
				pDevice->OnProfileEnd();
		}

		inline AK::IAkDeviceProfile * GetDevice() { return pDevice; }

		inline void SetDevice( AK::IAkDeviceProfile * in_pDevice )
		{
			AKASSERT( in_pDevice && !pDevice );
			in_pDevice->OnProfileStart();
			pDevice = in_pDevice;
		}

		unsigned int        uiNumStreams;

	protected:
		AK::IAkDeviceProfile *  pDevice;
	};
    AkDeviceProfileData pDevices[ AK_MAX_DEVICE_NUM ];
    unsigned int uiNumStreams = 0;

    for ( unsigned int uiDevice=0; uiDevice<uiNumDevices; uiDevice++ )
    {
        AK::IAkDeviceProfile * pDevice = pStmMgrProfile->GetDeviceProfile( uiDevice );
        AKASSERT( pDevice != NULL );
		pDevices[uiDevice].SetDevice( pDevice );

        // Get record if new.
        if ( pDevice->IsNew( ) )
        {
            // Send every new device.
            AkInt32 sizeofItem = SIZEOF_MONITORDATA(deviceRecordData);

            AkProfileDataCreator creator( sizeofItem );
	        if ( !creator.m_pData )
		        return;

            // Can send. Get actual data.
            creator.m_pData->eDataType = AkMonitorData::MonitorDataDevicesRecord;

            pDevice->GetDesc( creator.m_pData->deviceRecordData );
            pDevice->ClearNew( );

            // Send now.
        }

        unsigned int uiNumStmsDevice = pDevice->GetNumStreams( );

        // Store.
        uiNumStreams += pDevices[uiDevice].uiNumStreams = uiNumStmsDevice;

        // Get number of new streams.
        unsigned int uiNumNewStreams = 0;
        IAkStreamProfile * pStmProfile;
        for ( unsigned int uiStm = 0; uiStm < uiNumStmsDevice; ++uiStm )
	    {
            pStmProfile = pDevice->GetStreamProfile( uiStm );
            AKASSERT( pStmProfile != NULL );

            // Get record if new.
            if ( pStmProfile->IsNew( ) )
            {
                ++uiNumNewStreams;
            }
	    }

        // Send new stream records if any.
        if ( uiNumNewStreams > 0 )
        {
            // Compute size needed.
            AkInt32 sizeofItem = SIZEOF_MONITORDATA_TO( streamRecordData.streamRecords)
                + uiNumNewStreams * sizeof( AkMonitorData::StreamRecord );

            AkProfileDataCreator creator( sizeofItem );
	        if ( !creator.m_pData )
                return;

            // Can send. Get actual data.
            creator.m_pData->eDataType = AkMonitorData::MonitorDataStreamsRecord;

	        creator.m_pData->streamRecordData.ulNumNewRecords = uiNumNewStreams;

            AkUInt32 ulNewStream = 0;

	        for ( unsigned int uiStm = 0; uiStm < uiNumStmsDevice; ++uiStm )
	        {
                pStmProfile = pDevice->GetStreamProfile( uiStm );
                AKASSERT( pStmProfile != NULL );

                // Get record if new.
                if ( pStmProfile->IsNew( ) )
                {
                    pStmProfile->GetStreamRecord( creator.m_pData->streamRecordData.streamRecords[ ulNewStream ] );
                    pStmProfile->ClearNew( );
                    ulNewStream++;
                }
	        }

            // Send new streams now.
        }
    }

	// Send all devices data.
	{
		AkInt32 sizeofItem = SIZEOF_MONITORDATA_TO( streamingDeviceData.deviceData )
							+ uiNumDevices * sizeof( AkMonitorData::DeviceData );

        AkProfileDataCreator creator( sizeofItem );
	    if ( !creator.m_pData )
		    return;

        creator.m_pData->eDataType = AkMonitorData::MonitorDataStreamingDevice;

	    creator.m_pData->streamingDeviceData.ulNumDevices = uiNumDevices;
        creator.m_pData->streamingDeviceData.fInterval = (AkReal32) ( ( in_iNow - m_iLastUpdateStreaming ) / AK::g_fFreqRatio );

        for ( unsigned int uiDevice=0; uiDevice<uiNumDevices; uiDevice++ )
        {
			pDevices[uiDevice].GetDevice()->GetData( creator.m_pData->streamingDeviceData.deviceData[ uiDevice ] );
        }
    }

    // Send all streams' data, even if none.

    {
        AkInt32 sizeofItem = SIZEOF_MONITORDATA_TO( streamingData.streamData )
                        + uiNumStreams * sizeof( AkMonitorData::StreamData );

        AkProfileDataCreator creator( sizeofItem );
	    if ( !creator.m_pData )
		    return;

        creator.m_pData->eDataType = AkMonitorData::MonitorDataStreaming;
	    creator.m_pData->streamingData.ulNumStreams = uiNumStreams;
        creator.m_pData->streamingData.fInterval = (AkReal32) ( ( in_iNow - m_iLastUpdateStreaming ) / AK::g_fFreqRatio );

        IAkStreamProfile * pStmProfile;
		AkUInt32 uNumStreamsAllDevices = 0;
        for ( unsigned int uiDevice=0; uiDevice<uiNumDevices; uiDevice++ )
        {
	        for ( unsigned int ulStm = 0; ulStm < pDevices[uiDevice].uiNumStreams; ++ulStm )
	        {
                pStmProfile = pDevices[uiDevice].GetDevice()->GetStreamProfile( ulStm );
                AKASSERT( pStmProfile != NULL );
                pStmProfile->GetStreamData( creator.m_pData->streamingData.streamData[ uNumStreamsAllDevices ] );
				++uNumStreamsAllDevices;
	        }
        }
    }

    m_iLastUpdateStreaming = in_iNow;
}

void AkPerf::PostPipelineStats()
{
	if (AkResourceMonitor::HasSubscribers())
		CAkLEngine::UpdateResourceMonitorPipelineData();

	if ( !( AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataPipeline ) ) )
		return;

	AkVarLenDataCreator creator( AkMonitorData::MonitorDataPipeline );

	CAkLEngine::GetPipelineData( creator );
}

void AkPerf::PostEnvironmentStats()
{
	if ( !( AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataEnvironment ) ) )
		return;

	g_pRegistryMgr->PostEnvironmentStats();
}

void AkPerf::PostSendsStats()
{
	if ( !( AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataSends ) ) )
		return;

	CAkLEngine::PostSendsData();
}

void AkPerf::PostObsOccStats()
{
	if ( !( AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataObsOcc ) ) )
		return;

	g_pRegistryMgr->PostObsOccStats();
}

void AkPerf::PostListenerStats()
{
	if ( !( AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataListeners ) ) )
		return;

	g_pRegistryMgr->PostListenerStats();
}

void AkPerf::PostGameObjPositions()
{
	if ( !( AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK( AkMonitorData::MonitorDataGameObjPosition ) ) )
		return;

	AkMonitor::PostChangedGameObjPositions();
}

void AkPerf::PostInteractiveMusicInfo()
{
	extern AkExternalProfileHandlerCallback g_pExternalProfileHandlerCallback;
	if( g_pExternalProfileHandlerCallback )
		g_pExternalProfileHandlerCallback();
}

void AkPerf::PostSoundInfo()
{
	// Call CAkPlayingMgr::GetPlayingIDsFromGameObject() to get playing ids for the transport game object, then loop through the g_pPositionRepository 
	// items and send out notifs for each one having one of the transport game object's playing ids
	static bool bPlaying = false;
	AkUInt32 iNumIds = 0;
	AkUInt32 iNumPositions = 0;
	AkPlayingID* pPlayingIds = NULL;
	AkUInt32* pNumIds = NULL;
	// First get the number to allocate (pass 0 in iNumIds)
	g_pPlayingMgr->GetPlayingIDsFromGameObject(AkGameObjectID_Transport, iNumIds, NULL);
	if (iNumIds == 0)
	{
		if (!bPlaying)
			return;
		bPlaying = false;
	}
	else
	{
		bPlaying = true;
		iNumIds = AkMin(iNumIds, 100);
		// Now allocate pPlayingIds and get the info
		pPlayingIds = static_cast<AkPlayingID*>(AkAlloca(iNumIds * sizeof(AkPlayingID)));
		g_pPlayingMgr->GetPlayingIDsFromGameObject(AkGameObjectID_Transport, iNumIds, pPlayingIds);

		pNumIds = static_cast<AkUInt32*>(AkAlloca(iNumIds * sizeof(AkUInt32)));
		for (AkUInt32 i = 0; i < iNumIds; ++i)
		{
			AkUInt32 i_num = 0;
			AK::SoundEngine::GetSourcePlayPositions(pPlayingIds[i], NULL, &i_num);
			pNumIds[i] = i_num;
			iNumPositions += i_num;
		}
	}
	AkInt32 sizeofItem = SIZEOF_MONITORDATA_TO(soundPositionData.positions)
		+ iNumPositions * sizeof(AkMonitorData::SoundPositionData);
	AkProfileDataCreator creator(sizeofItem);
	if (!creator.m_pData)
		return;

	creator.m_pData->eDataType = AkMonitorData::MonitorDataSoundPosition;

	creator.m_pData->soundPositionData.numPositions = iNumPositions;
	AkUInt32 i_currentPos = 0;
	for (AkUInt32 i = 0; i < iNumIds; ++i)
	{
		AkUInt32 i_num = pNumIds[i];
		if (i_num)
		{
			AK::SoundEngine::GetSourcePlayPositions(pPlayingIds[i], &creator.m_pData->soundPositionData.positions[i_currentPos], &i_num);
			i_currentPos += i_num;
		}
	}
}

#endif
