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

#include "stdafx.h"
#include <AK/SpatialAudio/Common/AkSpatialAudio.h>
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/Tools//Common/AkLock.h>

#include <AK/Plugin/AkReflectGameData.h>

#include "Ak3DListener.h"
#include "AkRegistryMgr.h"
#include "AkLEngine.h"
#include "AkAudioLib.h"
#include "AkAudioMgr.h"
#include "AkQueuedMsg.h"
#include "AkCritical.h"
#include "AkPrivateTypes.h"

#include "AkSoundGeometry.h"
#include "AkSpatialAudioCompts.h"
#include "AkSpatialAudioMsg.h"
#include "AkDiffractionEdge.h"

#include "AkStochasticReflectionEngine.h"

#include "AkAcousticRoom.h"
#include "AkAcousticPortal.h"
#include "AkSpatialAudioTasks.h"

#include "AkSpatialAudioVoice.h"

extern CAkAudioMgr* g_pAudioMgr;
extern AkInitSettings g_settings; //Sound engine init settings

// Global initialized flag to know when spatial audio is in use.
extern bool g_bSpatialAudioInitialized;

AkSpatialAudioInitSettings g_SpatialAudioSettings;

#ifndef AK_OPTIMIZED
// Uncomment to enable all warnings
//#define AK_SPATIAL_AUDIO_VERBOSE
#endif

//#define REJECT_INVALID_FLOAT_INPUT // Enable this to reject AK::SpatialAudio API calls containing invalid float values.

#if defined(_DEBUG) || defined(REJECT_INVALID_FLOAT_INPUT)
#define CHECK_SOUND_ENGINE_INPUT_VALID
#endif

#ifdef REJECT_INVALID_FLOAT_INPUT
#define HANDLE_INVALID_FLOAT_INPUT( _msg ) { MONITOR_ERRORMSG( _msg ); return AK_InvalidParameter; }
#else
#define HANDLE_INVALID_FLOAT_INPUT( _msg ) MONITOR_ERRORMSG( _msg )
#endif

AkForceInline void AkMonitorError_WithID(const AkOSChar* msg, AkUInt64 id)
{
#ifndef AK_OPTIMIZED
	AkOSChar buf[256];
	AK_OSPRINTF(buf, (sizeof(buf) / sizeof(AkOSChar)), msg, id);
	AkMonitor::Monitor_PostString(buf, AK::Monitor::ErrorLevel_Error, AK_INVALID_PLAYING_ID, id);
#endif
}

AkForceInline void AkMonitorError_WithNameAndID(const AkOSChar* msg, const AK::SpatialAudio::OsString& name, AkUInt64 id)
{
#ifndef AK_OPTIMIZED
	AkOSChar buf[256];
	if (name.Get() != NULL)
	{
		AK_OSPRINTF(buf, (sizeof(buf) / sizeof(AkOSChar)), msg, name.Get(), id);
	}
	else
	{
		AK_OSPRINTF(buf, (sizeof(buf) / sizeof(AkOSChar)), msg, AKTEXT("<no name>"), id);
	}
	AkMonitor::Monitor_PostString(buf, AK::Monitor::ErrorLevel_Error, AK_INVALID_PLAYING_ID, id);
#endif
}

namespace AK { 
namespace SpatialAudio {

class CAkSpatialAudioPrivate;

struct AkSpatialAudioMsg_Reflector
{
	AkImageSourceID uSrcID;
	AkImageSourceSettings virtSrc;
	AkRoomID roomID;
	AkUniqueID uAuxBusID;
	AkGameObjectID gameObjectID;
	CAkSpatialAudioPrivate * pInstance;
};

void HandleRegisterListener(void* pData, AkUInt32 uSize);
void HandleUnregisterListener(void* pData, AkUInt32 uSize);
void HandleSetImageSource(void* pData, AkUInt32 uSize);
void SetImageSourceCleanup(void* pData, AkUInt32 uSize);
void HandleRemoveImageSource(void* pData, AkUInt32 uSize);
void HandleClearImageSources(void* pData, AkUInt32 uSize);
void HandleSetPortalObstructionAndOcclusion(void* pData, AkUInt32 uSize);
void HandleSetGeometry(void* pData, AkUInt32 uSize);
void SetGeometryCleanup(void* pData, AkUInt32 uSize);
void HandleRemoveGeometry(void* pData, AkUInt32 uSize);
void HandleSetGameObjectInRoom(void* pData, AkUInt32 uSize);
void HandleSetRoom(void* pData, AkUInt32 uSize);
void SetRoomCleanup(void* pData, AkUInt32 uSize);
void HandleRemoveRoom(void* pData, AkUInt32 uSize);
void HandleSetPortal(void* pData, AkUInt32 uSize);
void SetPortalCleanup(void* pData, AkUInt32 uSize);
void HandleRemovePortal(void* pData, AkUInt32 uSize);
void HandleSetEarlyReflectionsAuxSend(void* pData, AkUInt32 uSize);
void HandleSetEarlyReflectionsVolume(void* pData, AkUInt32 uSize);
void HandleSetReflectionsOrder(void* pData, AkUInt32 uSize);

AkSoundGeometry* LockGeometry();
void ReleaseGeometry();
const AkSoundGeometry* LockGeometryReadOnly();
void ReleaseGeometryReadOnly();

class CAkSpatialAudioPrivate
{
public:
	CAkSpatialAudioPrivate() 
		: m_lastExecTime(0)
		, m_bGeometryDirty(false)
#ifndef AK_OPTIMIZED
		, m_bSendGeometryUpdate(false)
#endif
		, m_ListenerID(AK_INVALID_GAME_OBJECT)
		, m_bUseDefaultListeners(true)
	{}

	void Execute();
	void GarbageCollectGameObjs();
	void UpdateEmitter(CAkSpatialAudioEmitter* pERE, CAkSpatialAudioListener* pERL);
	void UpdateListener(CAkSpatialAudioListener* pERL);
	void MonitorData();
	void MonitorRecap();

	bool GameObjectEmitterSerialize(CAkSpatialAudioEmitter* in_pEmitter, MonitorSerializer& serializer);
	bool RoomSerialize(AkGameObjectID in_uiID, MonitorSerializer& serializer, AkUInt32& in_uiNumChanged);


	AkGameObjectID GetListenerID() const;
	AkGameObjectID GetRegisteredListenerID() const;

	void SetListenerID(AkGameObjectID in_listenerID);

	AkAcousticRoom* UpdateComponentRoomsAndAuxs(CAkSpatialAudioComponent* in_pSpatialAudioCompnt, const Ak3DVector& in_position);

	AkForceInline bool GetDiffractionFlag(AkUInt32 in_uFlag) { return ((g_SpatialAudioSettings.uDiffractionFlags & in_uFlag) != 0); }
	
	AkSoundGeometry m_Geometry;
	
	AkSAObjectTaskQueue m_taskQueue;
	AkSAPortalRayCastingTaskQueue m_portalRaycastTaskQueue;
	AkSAPortalToPortalTaskQueue m_portalToPortalTaskQueue;

	AkInt64 m_lastExecTime;

	CAkLock m_GeometryLock;
	bool m_bGeometryDirty;

#ifndef AK_OPTIMIZED
	bool m_bSendGeometryUpdate;
#endif

	AkGameObjectID m_ListenerID;
	bool m_bUseDefaultListeners;
};
CAkSpatialAudioPrivate * g_pInstance = NULL;

#ifndef AK_OPTIMIZED
typedef AkHashList< AkGameObjectID, AK::Hash64::HashType > AkGODataHashMap;
static AkGODataHashMap g_GODataHashMap;

static void ClearHash()
{
	g_GODataHashMap.RemoveAll();
}
#endif

void SpatialAudioPostMessagesProcessed(
	AK::IAkGlobalPluginContext * ,	///< Engine context.
	AkGlobalCallbackLocation ,		///< Location where this callback is fired.
	void *							///< User cookie passed to AK::SoundEngine::RegisterGlobalCallback().
)
{
	AKASSERT(g_pInstance);
	g_pInstance->Execute();
}

void SpatialAudioBeginRender(
	AK::IAkGlobalPluginContext *,	///< Engine context.
	AkGlobalCallbackLocation,		///< Location where this callback is fired.
	void *							///< User cookie passed to AK::SoundEngine::RegisterGlobalCallback().
)
{
	AKASSERT(g_pInstance);	
}

#ifndef AK_OPTIMIZED
void MonitorData(
	AK::IAkGlobalPluginContext *,	///< Engine context.
	AkGlobalCallbackLocation,		///< Location where this callback is fired.
	void *							///< User cookie passed to AK::SoundEngine::RegisterGlobalCallback().
	)
{
	AKASSERT(g_pInstance);
	g_pInstance->MonitorData();
}

void MonitorRecap(
	AK::IAkGlobalPluginContext *,	///< Engine context.
	AkGlobalCallbackLocation,		///< Location where this callback is fired.
	void *							///< User cookie passed to AK::SoundEngine::RegisterGlobalCallback().
)
{
	AKASSERT(g_pInstance);
	g_pInstance->MonitorRecap();
	ClearHash();
}
#endif

void DestroySpatialAudio(
	AK::IAkGlobalPluginContext * ,	///< Engine context.
	AkGlobalCallbackLocation ,		///< Location where this callback is fired.
	void *							///< User cookie passed to AK::SoundEngine::RegisterGlobalCallback().
)
{
	if (g_pInstance)
	{
		for (AkUInt32 iMsgId = 0; iMsgId < AkSpatialAudioMsgID_Count; iMsgId++)
		{
			g_pAudioMgr->UnregisterMsgQueueHandler(iMsgId);
		}

		AK::SoundEngine::UnregisterGlobalCallback( SpatialAudioPostMessagesProcessed, AkGlobalCallbackLocation_PostMessagesProcessed );
		AK::SoundEngine::UnregisterGlobalCallback( SpatialAudioBeginRender, AkGlobalCallbackLocation_BeginRender );
#ifndef AK_OPTIMIZED
		AK::SoundEngine::UnregisterGlobalCallback( MonitorData, AkGlobalCallbackLocation_Monitor );
		AK::SoundEngine::UnregisterGlobalCallback( MonitorRecap, AkGlobalCallbackLocation_MonitorRecap );
#endif
		AK::SoundEngine::UnregisterGlobalCallback( DestroySpatialAudio, AkGlobalCallbackLocation_Term );

		while (!CAkSpatialAudioEmitter::List().IsEmpty())
		{
			CAkSpatialAudioEmitter::List().First()->GetOwner()->DeleteComponent<CAkSpatialAudioEmitter>();
		}

		while (!CAkSpatialAudioListener::List().IsEmpty())
		{
			CAkSpatialAudioListener::List().First()->GetOwner()->DeleteComponent<CAkSpatialAudioListener>();
		}

		AkDelete(AkMemID_SpatialAudio, g_pInstance);
		g_pInstance = NULL;

		AK::SpatialAudio::DbString::TermDB();

#ifndef AK_OPTIMIZED
		ClearHash();
		g_GODataHashMap.Term();
#endif

		g_bSpatialAudioInitialized = false;
	}
}

AKRESULT Init(const AkSpatialAudioInitSettings& in_settings)
{
	AKASSERT(g_pInstance == NULL);
	g_pInstance = AkNew(AkMemID_SpatialAudio, CAkSpatialAudioPrivate());
	if (g_pInstance == NULL)
		return AK_InsufficientMemory;
	
	g_SpatialAudioSettings = in_settings;

	if (
		(g_pInstance->m_Geometry.Init()																											!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleRegisterListener, AkSpatialAudioMsgID_RegisterListener)									!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleUnregisterListener, AkSpatialAudioMsgID_UnregisterListener)								!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleSetImageSource, SetImageSourceCleanup, AkSpatialAudioMsgID_SetImageSource)					!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleRemoveImageSource, AkSpatialAudioMsgID_RemoveImageSource)									!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleClearImageSources, AkSpatialAudioMsgID_ClearImageSources)									!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleSetPortalObstructionAndOcclusion, AkSpatialAudioMsgID_PortalObstructionAndOcclusion)		!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleSetGeometry, SetGeometryCleanup, AkSpatialAudioMsgID_SetGeometry)							!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleRemoveGeometry, AkSpatialAudioMsgID_RemoveGeometry)										!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleSetRoom, &SetRoomCleanup, AkSpatialAudioMsgID_SetRoom)										!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleRemoveRoom, AkSpatialAudioMsgID_RemoveRoom)												!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleSetGameObjectInRoom, AkSpatialAudioMsgID_SetGameObjectRoom)								!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleSetPortal, &SetPortalCleanup, AkSpatialAudioMsgID_SetPortal)								!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleRemovePortal, AkSpatialAudioMsgID_RemovePortal)											!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleSetEarlyReflectionsAuxSend, AkSpatialAudioMsgID_SetEarlyReflectionsAuxSend)				!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleSetEarlyReflectionsVolume, AkSpatialAudioMsgID_SetEarlyReflectionsVolume)					!= AK_Success) ||
		(g_pAudioMgr->RegisterMsgQueueHandler(&HandleSetReflectionsOrder, AkSpatialAudioMsgID_SetReflectionsOrder) != AK_Success) ||
		(AK::SpatialAudio::DbString::InitDB()																									!= AK_Success) ||
		(AK::SoundEngine::RegisterGlobalCallback( SpatialAudioPostMessagesProcessed, AkGlobalCallbackLocation_PostMessagesProcessed, NULL, AkPluginTypeGlobalExtension, AKCOMPANYID_AUDIOKINETIC, AKEXTENSIONID_SPATIALAUDIO )	!= AK_Success) ||
		(AK::SoundEngine::RegisterGlobalCallback( SpatialAudioBeginRender, AkGlobalCallbackLocation_BeginRender, NULL, AkPluginTypeGlobalExtension, AKCOMPANYID_AUDIOKINETIC, AKEXTENSIONID_SPATIALAUDIO) != AK_Success) ||
#ifndef AK_OPTIMIZED
		(AK::SoundEngine::RegisterGlobalCallback( MonitorData, AkGlobalCallbackLocation_Monitor)													!= AK_Success) ||
		(AK::SoundEngine::RegisterGlobalCallback( MonitorRecap, AkGlobalCallbackLocation_MonitorRecap) != AK_Success) ||
#endif
		(AK::SoundEngine::RegisterGlobalCallback( DestroySpatialAudio, AkGlobalCallbackLocation_Term)											!= AK_Success)
		)

	{
		DestroySpatialAudio(NULL, AkGlobalCallbackLocation_Term, NULL);
		return AK_Fail;
	}

	AkDiffractionPath::SetDiffractionSettings(	AkClamp( g_SpatialAudioSettings.fDiffractionShadowAttenFactor, 1.0f, 3.0f), 
												AkClamp( g_SpatialAudioSettings.fDiffractionShadowDegrees, 0.1f, 180.f));
	
	g_bSpatialAudioInitialized = true;

	return AK_Success;
}

void HandleRegisterListener(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_ListenerRegistration* pMsg = (AkSpatialAudioMsg_ListenerRegistration*)pData;

	CAkGameObject* pGameObj = g_pRegistryMgr->GetObjAndAddref(pMsg->gameObjectID);
	if (pGameObj != NULL)
	{
		CAkListener* pListenerCmpt = pGameObj->CreateComponent<CAkListener>();
		CAkSpatialAudioListener* pSAListenerCmpt = pGameObj->CreateComponent<CAkSpatialAudioListener>();
		CAkSpatialAudioComponent* pSACmptCmpt = pGameObj->CreateComponent<CAkSpatialAudioComponent>();
		
		if (pListenerCmpt == NULL || pSACmptCmpt == NULL || pSAListenerCmpt == NULL)
		{
			// All 3 components must exist for a spatial audio listener.
			// (deleting just the 2 spatial audio components. The sound engine listener component is not our business here.)
			pGameObj->DeleteComponent<CAkSpatialAudioListener>();
			pGameObj->DeleteComponent<CAkSpatialAudioComponent>();
		}

		pGameObj->Release();
	}

	g_pInstance->SetListenerID(pMsg->gameObjectID);
}

AKRESULT RegisterListener(AkGameObjectID in_gameObjectID)
{
	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkSpatialAudioMsg_ListenerRegistration));

	pMsg->apiExtension.uID = AkSpatialAudioMsgID_RegisterListener;

	AkSpatialAudioMsg_ListenerRegistration* pSAMsg = (AkSpatialAudioMsg_ListenerRegistration*)(&(pMsg->apiExtension) + 1);

	AkPlacementNew(pSAMsg) AkSpatialAudioMsg_ListenerRegistration;

	pSAMsg->gameObjectID = in_gameObjectID;

	g_pAudioMgr->FinishQueueWrite();

	return AK_Success;
}

void HandleUnregisterListener(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_ListenerRegistration* pMsg = (AkSpatialAudioMsg_ListenerRegistration*)pData;
	
	CAkGameObject* pGameObj = g_pRegistryMgr->GetObjAndAddref(pMsg->gameObjectID);
	if (pGameObj != NULL)
	{
		pGameObj->DeleteComponent<CAkSpatialAudioListener>();
		pGameObj->Release();
	}

	if (g_pInstance->m_ListenerID == pMsg->gameObjectID)
		g_pInstance->SetListenerID(AK_INVALID_GAME_OBJECT);

	
}

AKRESULT UnregisterListener(AkGameObjectID in_gameObjectID)
{
	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkSpatialAudioMsg_ListenerRegistration));

	pMsg->apiExtension.uID = AkSpatialAudioMsgID_UnregisterListener;

	AkSpatialAudioMsg_ListenerRegistration* pSAMsg = (AkSpatialAudioMsg_ListenerRegistration*)(&(pMsg->apiExtension) + 1);

	AkPlacementNew(pSAMsg) AkSpatialAudioMsg_ListenerRegistration;

	pSAMsg->gameObjectID = in_gameObjectID;

	g_pAudioMgr->FinishQueueWrite();

	return AK_Success;
}

void HandleSetImageSource(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_Reflector* pMsg = (AkSpatialAudioMsg_Reflector*)pData;

	CAkGameObject* pGameObj = g_pRegistryMgr->GetObjAndAddref(pMsg->gameObjectID);
	if (pGameObj != NULL)
	{
		CAkSpatialAudioComponent* pSACmpt = pGameObj->CreateComponent<CAkSpatialAudioComponent>();
		if (pSACmpt)
		{
			CAkSpatialAudioEmitter* pERE = pGameObj->CreateComponent<CAkSpatialAudioEmitter>();
			if (pERE)
			{
				pERE->SetCustomImageSource(pMsg->uAuxBusID, pMsg->uSrcID, pMsg->roomID, pMsg->virtSrc);
			}
		}
		pGameObj->Release();
	}
	else
	{
		AkMonitorError_WithID(AKTEXT("AK::SpatialAudio::SetImageSource: Game object (ID:%llu) is not registered."), pMsg->gameObjectID);
	}

	
	pMsg->virtSrc.name.Term();
}

void SetImageSourceCleanup(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_Reflector* pMsg = (AkSpatialAudioMsg_Reflector*)pData;
	pMsg->virtSrc.name.Term();
}

AKRESULT SetImageSource(AkImageSourceID in_srcID, const AkImageSourceSettings& in_info, AkUniqueID in_AuxBusID, AkRoomID in_roomID, AkGameObjectID in_gameObjectID/* = AK_INVALID_GAME_OBJECT*/)
{
	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkSpatialAudioMsg_Reflector));
	pMsg->apiExtension.uID = AkSpatialAudioMsgID_SetImageSource;
	AkSpatialAudioMsg_Reflector* pSAMsg = (AkSpatialAudioMsg_Reflector*)(&(pMsg->apiExtension) + 1);
	AkPlacementNew(pSAMsg) AkSpatialAudioMsg_Reflector();
	pSAMsg->uSrcID = in_srcID;
	pSAMsg->virtSrc = in_info;
	pSAMsg->virtSrc.name.AllocCopy();
	pSAMsg->roomID = in_roomID;
	pSAMsg->uAuxBusID = in_AuxBusID;
	pSAMsg->gameObjectID = in_gameObjectID;
	pSAMsg->pInstance = g_pInstance;
	g_pAudioMgr->FinishQueueWrite();

	return AK_Success;
}

void HandleRemoveImageSource(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_Reflector* pMsg = (AkSpatialAudioMsg_Reflector*)pData;

	CAkGameObject* pGameObj = g_pRegistryMgr->GetObjAndAddref(pMsg->gameObjectID);
	if (pGameObj != NULL)
	{
		CAkSpatialAudioEmitter* pERE = pGameObj->GetComponent<CAkSpatialAudioEmitter>();
		if (pERE)
		{
			pERE->RemoveCustomImageSource(pMsg->uAuxBusID, pMsg->uSrcID);
		}

		pGameObj->Release();
	}
	else
	{
		AkMonitorError_WithID(AKTEXT("AK::SpatialAudio::RemoveImageSource: Game object (ID:%llu) is not registered."), pMsg->gameObjectID);
	}
}

AKRESULT RemoveImageSource(AkUniqueID in_srcID, AkUniqueID in_AuxBusID, AkGameObjectID in_gameObjectID/* = AK_INVALID_GAME_OBJECT*/)
{
	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkSpatialAudioMsg_Reflector));
	pMsg->apiExtension.uID = AkSpatialAudioMsgID_RemoveImageSource;
	AkSpatialAudioMsg_Reflector* pSAMsg = (AkSpatialAudioMsg_Reflector*)(&(pMsg->apiExtension) + 1);
	pSAMsg->uSrcID = in_srcID;
	pSAMsg->uAuxBusID = in_AuxBusID;
	pSAMsg->gameObjectID = in_gameObjectID;
	pSAMsg->pInstance = g_pInstance;
	g_pAudioMgr->FinishQueueWrite();

	return AK_Success;
}

void HandleClearImageSources(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_Reflector* pMsg = (AkSpatialAudioMsg_Reflector*)pData;

	if (pMsg->gameObjectID == AK_INVALID_GAME_OBJECT)
	{
		// Clear all image sources
		for (CAkSpatialAudioEmitter::tList::Iterator it = CAkSpatialAudioEmitter::List().Begin(); it != CAkSpatialAudioEmitter::List().End(); ++it)
		{
			CAkSpatialAudioEmitter* pEmitter = static_cast<CAkSpatialAudioEmitter*>(*it);
			pEmitter->ClearCustomImageSources(pMsg->uAuxBusID);
		}
	}
	else
	{
		CAkGameObject* pGameObj = g_pRegistryMgr->GetObjAndAddref(pMsg->gameObjectID);
		if (pGameObj != NULL)
		{
			CAkSpatialAudioEmitter* pERE = pGameObj->GetComponent<CAkSpatialAudioEmitter>();
			if (pERE)
			{
				pERE->ClearCustomImageSources(pMsg->uAuxBusID);
			}

			pGameObj->Release();
		}
		else
		{
			AkMonitorError_WithID(AKTEXT("AK::SpatialAudio::ClearImageSources: Game object (ID:%llu) is not registered."), pMsg->gameObjectID);
		}
	}
}

AKRESULT ClearImageSources(AkUniqueID in_AuxBusID, AkGameObjectID in_gameObjectID/* = AK_INVALID_GAME_OBJECT*/)
{
	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkSpatialAudioMsg_Reflector));
	pMsg->apiExtension.uID = AkSpatialAudioMsgID_ClearImageSources;
	AkSpatialAudioMsg_Reflector* pSAMsg = (AkSpatialAudioMsg_Reflector*)(&(pMsg->apiExtension) + 1);
	pSAMsg->uSrcID = (AkImageSourceID)-1;
	pSAMsg->uAuxBusID = in_AuxBusID;
	pSAMsg->gameObjectID = in_gameObjectID;
	pSAMsg->pInstance = g_pInstance;
	g_pAudioMgr->FinishQueueWrite();

	return AK_Success;
}

void HandleSetPortalObstructionAndOcclusion(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_PortalObstructionAndOcclusion* pMsg = (AkSpatialAudioMsg_PortalObstructionAndOcclusion*)pData;

	const AkSoundGeometry* pGeom = LockGeometryReadOnly();
	const AkAcousticPortal* pPortal = pGeom->GetPortal(pMsg->portalID);
	if (pPortal)
	{
		// We are technically writing here, but we don't want to dirty the geometry, just to update two floats.
		const_cast<AkAcousticPortal*>(pPortal)->SetObstructionAndOcclusion(pMsg->obstruction, pMsg->occlusion);
	}
	else
	{
		AkMonitorError_WithID(AKTEXT("AK::SpatialAudio::SetPortalObstructionAndOcclusion: portal with ID: %llu not found."), pMsg->portalID);
	}
	ReleaseGeometryReadOnly();
}

AKRESULT SetPortalObstructionAndOcclusion(AkPortalID in_portalObjectID, AkReal32 in_fObstruction, AkReal32 in_fOcclusion)
{
	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkSpatialAudioMsg_PortalObstructionAndOcclusion));
	pMsg->apiExtension.uID = AkSpatialAudioMsgID_PortalObstructionAndOcclusion;
	AkSpatialAudioMsg_PortalObstructionAndOcclusion* pSAMsg = (AkSpatialAudioMsg_PortalObstructionAndOcclusion*)(&(pMsg->apiExtension) + 1);

	pSAMsg->portalID = in_portalObjectID;
	pSAMsg->obstruction = in_fObstruction;
	pSAMsg->occlusion = in_fOcclusion;

	g_pAudioMgr->FinishQueueWrite();

	return AK_Success;
}

AKRESULT QueryReflectionPaths(AkGameObjectID in_gameObjectID, AkVector& out_listenerPos, AkVector& out_emitterPos, AkReflectionPathInfo* out_Paths, AkUInt32& io_uArraySize)
{
	AKRESULT res = AK_Fail;
	
	CAkFunctionCritical globalLock;

	CAkSpatialAudioEmitter* pEmitter = g_pRegistryMgr->GetGameObjComponent<CAkSpatialAudioEmitter>(in_gameObjectID);
	if (pEmitter && pEmitter->GetListener())
	{
		const Ak3DVector& listenerPos = pEmitter->GetListener()->GetPosition();
		out_listenerPos.X = listenerPos.X;
		out_listenerPos.Y = listenerPos.Y;
		out_listenerPos.Z = listenerPos.Z;

		const Ak3DVector& emitterPos = pEmitter->GetPosition();
		out_emitterPos.X = emitterPos.X;
		out_emitterPos.Y = emitterPos.Y;
		out_emitterPos.Z = emitterPos.Z;

		CAkReflectionPaths& paths = pEmitter->GetReflectionPaths();
		io_uArraySize = AkMin(paths.Length(), io_uArraySize);
		for (AkUInt32 i = 0; i < io_uArraySize; i++)
		{
			AkReflectionPath& path = paths[i];

			for (AkUInt32 j = 0; j < path.numPathPoints; j++)
			{
				out_Paths[i].pathPoint[j] = Ak3DVector(path.pathPoint[j]);
				out_Paths[i].diffraction[j] = path.diffraction[j];
				
				if (path.surfaces[j] != NULL)
					out_Paths[i].surfaces[j] = *path.surfaces[j];
				else
					out_Paths[i].surfaces[j] = AkAcousticSurface();
			}

			out_Paths[i].imageSource = path.CalcImageSource(emitterPos, listenerPos);
			out_Paths[i].isOccluded = false; //deprecated
			out_Paths[i].numPathPoints = path.numPathPoints;
			out_Paths[i].numReflections = path.GetNumReflections();
			out_Paths[i].level = path.level;
		}
	
		

		res = AK_Success;
	}
	else
	{
		io_uArraySize = 0;
	}
	
	return res;
}

AKRESULT QueryDiffractionPaths(AkGameObjectID in_gameObjectID, AkVector& out_listenerPos, AkVector& out_emitterPos, AkDiffractionPathInfo* out_Paths, AkUInt32& io_uArraySize)
{
	AKRESULT res = AK_Fail;
	CAkFunctionCritical globalLock;

	CAkSpatialAudioEmitter* pEmitter = g_pRegistryMgr->GetGameObjComponent<CAkSpatialAudioEmitter>(in_gameObjectID);
	if (pEmitter && pEmitter->GetListener())
	{
		const Ak3DVector& listenerPos = pEmitter->GetListener()->GetPosition();
		out_listenerPos.X = listenerPos.X;
		out_listenerPos.Y = listenerPos.Y;
		out_listenerPos.Z = listenerPos.Z;

		const Ak3DVector& emitterPos = pEmitter->GetPosition();
		out_emitterPos.X = emitterPos.X;
		out_emitterPos.Y = emitterPos.Y;
		out_emitterPos.Z = emitterPos.Z;

		// diffraction paths
		const CAkDiffractionPaths& diffraction = pEmitter->GetDiffractionPaths();

		AkUInt32 pathIdx = 0;
		for (AkUInt32 i = 0; i < AkMin(io_uArraySize, diffraction.Length()); ++i)
		{
			AkDiffractionPath& dfrn = diffraction[i];

			AkUInt32 nodeIdx = 0;
			for (AkUInt32 edgeIdx = 0; edgeIdx < dfrn.nodeCount && nodeIdx < AkDiffractionPathInfo::kMaxNodes; edgeIdx++)
			{
				out_Paths[pathIdx].nodes[nodeIdx].X = dfrn.nodes[edgeIdx].X;
				out_Paths[pathIdx].nodes[nodeIdx].Y = dfrn.nodes[edgeIdx].Y;
				out_Paths[pathIdx].nodes[nodeIdx].Z = dfrn.nodes[edgeIdx].Z;

				out_Paths[pathIdx].angles[nodeIdx] = dfrn.angles[edgeIdx];

				out_Paths[pathIdx].portals[nodeIdx] = dfrn.portals[edgeIdx];
				out_Paths[pathIdx].rooms[nodeIdx] = dfrn.rooms[edgeIdx];

				++nodeIdx;
			}

			if (nodeIdx <= AkDiffractionPathInfo::kMaxNodes)
			{
				out_Paths[pathIdx].rooms[nodeIdx] = pEmitter->GetActiveRoom();
			}

			out_Paths[pathIdx].nodeCount = nodeIdx;
			out_Paths[pathIdx].virtualPos = dfrn.virtualPos;
			out_Paths[pathIdx].diffraction = dfrn.diffraction;
			out_Paths[pathIdx].totLength = dfrn.totLength;
			out_Paths[pathIdx].obstructionValue = dfrn.obstructionValue;

			++pathIdx;
		}

		io_uArraySize = pathIdx;

		res = AK_Success;
	}
	else
	{
		io_uArraySize = 0;
	}

	return res;
}

AKRESULT QueryWetDiffraction(AkPortalID in_portalID, AkReal32& out_fWetDiffraction)
{
	AKRESULT res = AK_Fail;
	CAkFunctionCritical globalLock;

	CAkSpatialAudioListener* pListener = g_pRegistryMgr->GetGameObjComponent<CAkSpatialAudioListener>(g_pInstance->GetListenerID());
	if (pListener)
	{
		const AkSoundGeometry* pGeometry = LockGeometryReadOnly();
		const AkAcousticPortal* pPortal = pGeometry->GetPortal(in_portalID);
		if (pPortal)
		{
			out_fWetDiffraction = pPortal->GetWetDiffraction();

			res = AK_Success;
		}

		ReleaseGeometryReadOnly();
	}

	return res;
}

static bool IsValidTriangle(const AkTriangle& in_tri, const AkVertex* in_verts)
{
	return	
#ifdef CHECK_SOUND_ENGINE_INPUT_VALID
		AkMath::IsValidFloatVector((const AkVector &)in_verts[in_tri.point0]) &&
		AkMath::IsValidFloatVector((const AkVector &)in_verts[in_tri.point1]) &&
		AkMath::IsValidFloatVector((const AkVector &)in_verts[in_tri.point2]) &&
#endif
		in_tri.point0 != in_tri.point1 &&
		in_tri.point1 != in_tri.point2 &&
		in_tri.point0 != in_tri.point2 && 
		*(Ak3DVector*)&(in_verts[in_tri.point0]) != *(Ak3DVector*)&(in_verts[in_tri.point1]) &&
		*(Ak3DVector*)&(in_verts[in_tri.point1]) != *(Ak3DVector*)&(in_verts[in_tri.point2]) &&
		*(Ak3DVector*)&(in_verts[in_tri.point0]) != *(Ak3DVector*)&(in_verts[in_tri.point2]);

}

AKRESULT SetGeometry(AkGeometrySetID in_GeomSetID, const AkGeometryParams& in_params)
{
	for (AkUInt32 i = 0; i < in_params.NumTriangles; ++i)
	{
		if (!IsValidTriangle(in_params.Triangles[i], in_params.Vertices))
		{
			MONITOR_ERRORMSG(AKTEXT("AK::SpatialAudio::SetGeometry - Geometry set contains 1 or more invalid triangles."));
			return AK_InvalidParameter;
		}
	}

	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkSpatialAudioMsg_SetGeometry));

	if (pMsg == NULL)
		return AK_Fail; // Command too large, ReserveQueue() will warn.

	pMsg->apiExtension.uID = AkSpatialAudioMsgID_SetGeometry;
	AkSpatialAudioMsg_SetGeometry* pSAMsg = (AkSpatialAudioMsg_SetGeometry*)(&(pMsg->apiExtension) + 1);
	AkPlacementNew(pSAMsg) AkSpatialAudioMsg_SetGeometry();

	pSAMsg->geometryID = in_GeomSetID;

	AKRESULT res = AK_Success;

	pSAMsg->params = in_params;
	pSAMsg->params.Triangles = NULL;
	pSAMsg->params.Surfaces = NULL;
	pSAMsg->params.Vertices = NULL;

	if (res == AK_Success && in_params.NumTriangles > 0)
	{
		pSAMsg->params.Triangles = (AkTriangle*)AkAlloc(AkMemID_SpatialAudioGeometry, sizeof(AkTriangle)*in_params.NumTriangles);
		if (pSAMsg->params.Triangles != NULL)
		{
			for (AkUInt32 i = 0; i < in_params.NumTriangles; ++i)
			{
				AkPlacementNew(&pSAMsg->params.Triangles[i]) AkTriangle();
				pSAMsg->params.Triangles[i] = in_params.Triangles[i];
			}
				 
			pSAMsg->params.NumTriangles = in_params.NumTriangles;
		}
		else
		{
			res = AK_InsufficientMemory;
		}
	}

	if (res == AK_Success && in_params.NumVertices > 0)
	{
		pSAMsg->params.Vertices = (AkVertex*)AkAlloc(AkMemID_SpatialAudioGeometry, sizeof(AkVertex)*in_params.NumVertices);
		if (pSAMsg->params.Vertices != NULL)
		{
			for (AkUInt32 i = 0; i < in_params.NumVertices; ++i)
			{
				AkPlacementNew(&pSAMsg->params.Vertices[i]) AkVertex();
				pSAMsg->params.Vertices[i] = in_params.Vertices[i];
			}

			pSAMsg->params.NumVertices = in_params.NumVertices;
		}
		else
		{
			res = AK_InsufficientMemory;
		}
	}

	if (res == AK_Success && in_params.NumSurfaces > 0)
	{
		pSAMsg->params.Surfaces = (AkAcousticSurface*)AkAlloc(AkMemID_SpatialAudioGeometry, sizeof(AkAcousticSurface)*in_params.NumSurfaces);
		if (pSAMsg->params.Surfaces != NULL)
		{
			for (AkUInt32 i = 0; i < in_params.NumSurfaces; ++i)
			{
				AkPlacementNew(&pSAMsg->params.Surfaces[i]) AkAcousticSurface();
				pSAMsg->params.Surfaces[i] = in_params.Surfaces[i];

				//Copy strings
				const char* pSrcStr = in_params.Surfaces[i].strName;
				const char*& pDstStr = pSAMsg->params.Surfaces[i].strName;
				if (pDstStr != NULL)
				{
					size_t uLen = strlen(pSrcStr);
					if (uLen > 0)
					{
						pDstStr = (char*)AkAlloc(AkMemID_SpatialAudioGeometry,(uLen + 1) * sizeof(char));
						if (pDstStr != NULL)
						{
							AKPLATFORM::AkMemCpy((void*)pDstStr, (void*)pSrcStr, (AkUInt32) ((uLen + 1L) * sizeof(char)));
						}
						else
						{
							res = AK_InsufficientMemory;
						}
					}
				}
			}
								 
			pSAMsg->params.NumSurfaces = in_params.NumSurfaces;
		}
		else
		{
			res = AK_InsufficientMemory;
		}
	}

	if (res != AK_Success)
	{
		TermAkGeometryParams(pSAMsg->params);
	}

	g_pAudioMgr->FinishQueueWrite();

	return res;
}
  
void HandleSetGeometry(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_SetGeometry* pMsg = (AkSpatialAudioMsg_SetGeometry*)pData;
	AkSoundGeometry* pGeom = (AkSoundGeometry*)LockGeometry();
	if (pGeom->SetGeometry(pMsg->geometryID, pMsg->params) != AK_Success)
	{
		MONITOR_ERRORMSG(AKTEXT("AK::SpatialAudio::SetGeometry: error adding geometry set."));
	}
	ReleaseGeometry();
}

void SetGeometryCleanup(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_SetGeometry* pSAMsg = (AkSpatialAudioMsg_SetGeometry*)pData;
	
	TermAkGeometryParams(pSAMsg->params);

	pSAMsg->~AkSpatialAudioMsg_SetGeometry();
}

AKRESULT RemoveGeometry(AkGeometrySetID in_SetID)
{
	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkGeometrySetID));
	pMsg->apiExtension.uID = AkSpatialAudioMsgID_RemoveGeometry;
	AkGeometrySetID* pID = (AkGeometrySetID*)(&(pMsg->apiExtension) + 1);
	*pID = in_SetID;
	g_pAudioMgr->FinishQueueWrite();
	return AK_Success;
}

void HandleRemoveGeometry(void* pData, AkUInt32 uSize)
{
	AkGeometrySetID geomSetID = *(AkGeometrySetID*)pData;
	AkSoundGeometry* pGeom = (AkSoundGeometry*) LockGeometry();
	if (pGeom->RemoveGeometry(geomSetID) != AK_Success)
	{
		MONITOR_ERRORMSG(AKTEXT("AK::SpatialAudio::RemoveGeometry: error removing geometry set."));
	}
	ReleaseGeometry();
}

AKRESULT SetGameObjectInRoom(AkGameObjectID in_gameObjectID, AkRoomID in_CurrentRoomID)
{
	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkSpatialAudioMsg_SetGameObjInRoom));
	pMsg->apiExtension.uID = AkSpatialAudioMsgID_SetGameObjectRoom;
	AkSpatialAudioMsg_SetGameObjInRoom* pSAMsg = (AkSpatialAudioMsg_SetGameObjInRoom*)(&(pMsg->apiExtension) + 1);

	pSAMsg->gameObjectID = in_gameObjectID;
	pSAMsg->roomID = in_CurrentRoomID;

	g_pAudioMgr->FinishQueueWrite();
	return AK_Success;
}

void HandleSetGameObjectInRoom(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_SetGameObjInRoom* pMsg = (AkSpatialAudioMsg_SetGameObjInRoom*)(pData);

	CAkGameObject* pGameObj = g_pRegistryMgr->GetObjAndAddref(pMsg->gameObjectID);
	if (pGameObj != NULL)
	{
#ifdef AK_SPATIAL_AUDIO_VERBOSE
		AkSoundGeometry* pGeom = (AkSoundGeometry*)LockGeometryReadOnly();
		if (pGeom)
		{
			if (pGeom->GetRoom(pMsg->roomID) == NULL)
			{
				AkMonitorError_WithID(AKTEXT("AK::SpatialAudio::SetGameObjectInRoom: Unknown Room ID (%llu). Make sure to call AK::SpatialAudio::SetRoom() first."), pMsg->roomID);
			}
		}
		ReleaseGeometryReadOnly();
#endif

		CAkSpatialAudioComponent* pSACmpt = pGameObj->CreateComponent<CAkSpatialAudioComponent>();
		if (pSACmpt)
			pSACmpt->SetCurrentRoom(pMsg->roomID);

		pGameObj->Release();
	}
	else
	{
		AkMonitorError_WithID(AKTEXT("AK::SpatialAudio::SetGameObjectInRoom: game object (ID:%llu) not registered."), pMsg->gameObjectID);
	}
}

AKRESULT SetReflectionsOrder(AkUInt32 in_uReflectionsOrder, bool in_bUpdatePaths)
{
	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkSpatialAudioMsg_SetReflectionsOrder));
	pMsg->apiExtension.uID = AkSpatialAudioMsgID_SetReflectionsOrder;
	AkSpatialAudioMsg_SetReflectionsOrder* pSAMsg = (AkSpatialAudioMsg_SetReflectionsOrder*)(&(pMsg->apiExtension) + 1);

	pSAMsg->reflectionsOrder = AkClamp( in_uReflectionsOrder, 0, 4 );
	pSAMsg->updatePaths = in_bUpdatePaths;

	g_pAudioMgr->FinishQueueWrite();
	return AK_Success;
}

void HandleSetReflectionsOrder(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_SetReflectionsOrder* pMsg = (AkSpatialAudioMsg_SetReflectionsOrder*)(pData);
	AKASSERT(g_pInstance != NULL);
	
	if (pMsg->updatePaths)
	{
		for (CAkSpatialAudioListener::tList::Iterator it = CAkSpatialAudioListener::List().Begin(); it != CAkSpatialAudioListener::List().End(); ++it)
		{
			CAkSpatialAudioListener* pListener = static_cast<CAkSpatialAudioListener*>(*it);
			if (pListener)
			{
				if (pMsg->reflectionsOrder < g_SpatialAudioSettings.uMaxReflectionOrder)
				{
					pListener->m_PathsToPortals.RemoveAll();
					pListener->GetStochasticRays().RemoveAll();
				}
				pListener->GetSpatialAudioComponent()->SetPositionDirty();
			}
		}

		for (CAkSpatialAudioEmitter::tList::Iterator it = CAkSpatialAudioEmitter::List().Begin(); it != CAkSpatialAudioEmitter::List().End(); ++it)
		{
			CAkSpatialAudioEmitter* pEmitter = static_cast<CAkSpatialAudioEmitter*>(*it);
			if (pEmitter)
			{
				if (pMsg->reflectionsOrder < g_SpatialAudioSettings.uMaxReflectionOrder)
				{
					pEmitter->m_PathsToPortals.RemoveAll();
					pEmitter->GetStochasticPaths().RemoveAll();
					pEmitter->GetReflectionPaths().RemoveAll();
				}
				pEmitter->GetSpatialAudioComponent()->SetPositionDirty();
			}
		}
	}

	g_SpatialAudioSettings.uMaxReflectionOrder = pMsg->reflectionsOrder;
}

AKRESULT SetNumberOfPrimaryRays(AkUInt32 in_uNbPrimaryRays)
{
	AKASSERT(g_pInstance != NULL);
	g_SpatialAudioSettings.uNumberOfPrimaryRays = in_uNbPrimaryRays;
	return AK_Success;
}

AKRESULT SetEarlyReflectionsAuxSend(AkGameObjectID in_gameObjectID, AkAuxBusID in_auxBusID)
{
	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkSpatialAudioMsg_SetEarlyReflectionsAuxSend));
	pMsg->apiExtension.uID = AkSpatialAudioMsgID_SetEarlyReflectionsAuxSend;
	AkSpatialAudioMsg_SetEarlyReflectionsAuxSend* pSAMsg = (AkSpatialAudioMsg_SetEarlyReflectionsAuxSend*)(&(pMsg->apiExtension) + 1);

	pSAMsg->gameObjectID = in_gameObjectID;
	pSAMsg->auxBus = in_auxBusID;

	g_pAudioMgr->FinishQueueWrite();
	return AK_Success;
}

void HandleSetEarlyReflectionsAuxSend(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_SetEarlyReflectionsAuxSend* pMsg = (AkSpatialAudioMsg_SetEarlyReflectionsAuxSend*)(pData);

	CAkGameObject* pGameObj = g_pRegistryMgr->GetObjAndAddref(pMsg->gameObjectID);
	if (pGameObj != NULL)
	{
		CAkSpatialAudioComponent* pSACmpt = pGameObj->CreateComponent<CAkSpatialAudioComponent>();
		if (pSACmpt)
		{
			pSACmpt->SetEarlyReflectionsAuxSend(pMsg->auxBus);
		}

		pGameObj->Release();
	}
	else
	{
		AkMonitorError_WithID(AKTEXT("AK::SpatialAudio::SetEarlyReflectionsAuxSend: game object (ID:%llu) not registered."), pMsg->gameObjectID);
	}
}

AKRESULT SetEarlyReflectionsVolume(AkGameObjectID in_gameObjectID, AkReal32 in_fControlValue)
{
	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkSpatialAudioMsg_SetEarlyReflectionsVolume));
	pMsg->apiExtension.uID = AkSpatialAudioMsgID_SetEarlyReflectionsVolume;
	AkSpatialAudioMsg_SetEarlyReflectionsVolume* pSAMsg = (AkSpatialAudioMsg_SetEarlyReflectionsVolume*)(&(pMsg->apiExtension) + 1);

	pSAMsg->gameObjectID = in_gameObjectID;
	pSAMsg->volume = in_fControlValue;

	g_pAudioMgr->FinishQueueWrite();
	return AK_Success;
}

void HandleSetEarlyReflectionsVolume(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_SetEarlyReflectionsVolume* pMsg = (AkSpatialAudioMsg_SetEarlyReflectionsVolume*)(pData);

	CAkGameObject* pGameObj = g_pRegistryMgr->GetObjAndAddref(pMsg->gameObjectID);
	if (pGameObj != NULL)
	{
		CAkSpatialAudioComponent* pSACmpt = pGameObj->CreateComponent<CAkSpatialAudioComponent>();
		if (pSACmpt)
		{
			pSACmpt->SetEarlyReflectionsVolume(pMsg->volume);
		}

		pGameObj->Release();
	}
	else
	{
		AkMonitorError_WithID(AKTEXT("AK::SpatialAudio::SetEarlyReflectionsVolume: game object (ID:%llu) not registered."), pMsg->gameObjectID);
	}
}

AKRESULT SetRoom( AkRoomID in_RoomID, const AkRoomParams& in_Params	)
{
	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkSpatialAudioMsg_SetRoom) );

	if (pMsg == NULL)
		return AK_Fail; // Command too large, ReserveQueue() will warn.

	pMsg->apiExtension.uID = AkSpatialAudioMsgID_SetRoom;
	AkSpatialAudioMsg_SetRoom* pSAMsg = (AkSpatialAudioMsg_SetRoom*)(&(pMsg->apiExtension) + 1);
	AkPlacementNew(pSAMsg) AkSpatialAudioMsg_SetRoom();

	pSAMsg->roomID = in_RoomID;
	pSAMsg->params = in_Params;
	pSAMsg->params.strName.AllocCopy();

	g_pAudioMgr->FinishQueueWrite();
	return AK_Success;
}

void HandleSetRoom(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_SetRoom* pSAMsg = (AkSpatialAudioMsg_SetRoom*)(pData);
	AkSoundGeometry* pGeom = (AkSoundGeometry*)LockGeometry();
	pGeom->SetRoom(pSAMsg->roomID, pSAMsg->params, g_pInstance->GetListenerID());
	ReleaseGeometry();
	pSAMsg->~AkSpatialAudioMsg_SetRoom();
}

void SetRoomCleanup(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_SetRoom* pSAMsg = (AkSpatialAudioMsg_SetRoom*)(pData);
	pSAMsg->~AkSpatialAudioMsg_SetRoom();
}


AKRESULT RemoveRoom(AkRoomID in_RoomID)
{
	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkRoomID));
	pMsg->apiExtension.uID = AkSpatialAudioMsgID_RemoveRoom;
	AkRoomID* pRoom = (AkRoomID*)(&(pMsg->apiExtension) + 1);
	*pRoom = in_RoomID;
	g_pAudioMgr->FinishQueueWrite();
	return AK_Success;
}

void HandleRemoveRoom(void* pData, AkUInt32 uSize)
{
	AkRoomID roomID = *(AkRoomID*)(pData);
	AkSoundGeometry* pGeom = (AkSoundGeometry*)LockGeometry();
	pGeom->DeleteRoom(roomID);
	ReleaseGeometry();
}

AKRESULT SetPortal(AkPortalID in_PortalID, const AkPortalParams& in_Params)
{
	if (in_Params.FrontRoom == in_Params.BackRoom)
	{
		AkMonitorError_WithNameAndID(AKTEXT("AK::SpatialAudio::SetPortal: Portal \"%s\" (ID:%llu) must have a front room which is distinct from its back room."), in_Params.strName, in_PortalID);
		return AK_InvalidParameter;
	}

	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkSpatialAudioMsg_SetPortal));
	pMsg->apiExtension.uID = AkSpatialAudioMsgID_SetPortal;

	AkSpatialAudioMsg_SetPortal* pSAMsg = (AkSpatialAudioMsg_SetPortal*)(&(pMsg->apiExtension) + 1);
	AkPlacementNew(pSAMsg) AkSpatialAudioMsg_SetPortal();

	pSAMsg->portalID = in_PortalID;
	pSAMsg->params = in_Params;
	pSAMsg->params.strName.AllocCopy();

	g_pAudioMgr->FinishQueueWrite();
	return AK_Success;
}

void HandleSetPortal(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_SetPortal* pSAMsg = (AkSpatialAudioMsg_SetPortal*)(pData);
	AkSoundGeometry* pGeom = (AkSoundGeometry*)LockGeometry();
	pGeom->SetPortal(pSAMsg->portalID, pSAMsg->params);
	ReleaseGeometry();
	pSAMsg->~AkSpatialAudioMsg_SetPortal();
}

void SetPortalCleanup(void* pData, AkUInt32 uSize)
{
	AkSpatialAudioMsg_SetPortal* pSAMsg = (AkSpatialAudioMsg_SetPortal*)(pData);
	pSAMsg->~AkSpatialAudioMsg_SetPortal();
}

AKRESULT RemovePortal(AkPortalID in_PortalID)
{
	AkQueuedMsg * pMsg = g_pAudioMgr->ReserveQueue(QueuedMsgType_ApiExtension, AkQueuedMsg::Sizeof_ApiExtension() + sizeof(AkPortalID));
	pMsg->apiExtension.uID = AkSpatialAudioMsgID_RemovePortal;
	AkPortalID* pPortal = (AkPortalID*)(&(pMsg->apiExtension) + 1);
	*pPortal = in_PortalID;
	g_pAudioMgr->FinishQueueWrite();
	return AK_Success;
}

void HandleRemovePortal(void* pData, AkUInt32 uSize)
{
	AkPortalID portalID = *(AkPortalID*)(pData);
	AkSoundGeometry* pGeom = (AkSoundGeometry*)LockGeometry();
	pGeom->DeletePortal(portalID);
	ReleaseGeometry();
}

AkSoundGeometry* LockGeometry()
{
	g_pInstance->m_GeometryLock.Lock();
	return &g_pInstance->m_Geometry;
}

void ReleaseGeometry()
{
	g_pInstance->m_bGeometryDirty = true;
	g_pInstance->m_GeometryLock.Unlock();
}

const AkSoundGeometry* LockGeometryReadOnly()
{
	g_pInstance->m_GeometryLock.Lock();
	return &g_pInstance->m_Geometry;
}

void ReleaseGeometryReadOnly()
{
	g_pInstance->m_GeometryLock.Unlock();
}

void CAkSpatialAudioPrivate::Execute()
{
	GarbageCollectGameObjs();

	LockGeometryReadOnly();
	
	AkGameObjectID spatialAudioListenerID = GetRegisteredListenerID();

	// Do a first pass on the emitters to update positions and attach listeners - this may create a listener component which needs to be updated in the listener loop below.
	for (CAkSpatialAudioComponent::tList::Iterator it = CAkSpatialAudioComponent::List().Begin();
		it != CAkSpatialAudioComponent::List().End(); ++it)
	{
		CAkSpatialAudioComponent* pObj = static_cast<CAkSpatialAudioComponent*>(*it);

		if (spatialAudioListenerID != AK_INVALID_GAME_OBJECT &&
			!pObj->GetOwner()->HasComponent(GameObjComponentIdx_SpatialAudioRoom) &&
			pObj->UpdatePosition(g_SpatialAudioSettings.fMovementThreshold))
		{
			CAkSpatialAudioEmitter* pEmitter = pObj->GetOwner()->CreateComponent<CAkSpatialAudioEmitter>();
			if (pEmitter)
				pEmitter->AttachListener(spatialAudioListenerID);
		}
		else
		{
			// 1) There should be no emitter if there is no listener.
			// 2) Rooms should not have a spatial audio emitter.
			// 3) Spatial audio emitters require >0 sound positions.
			pObj->GetOwner()->DeleteComponent<CAkSpatialAudioEmitter>();
		}
	}

	if (m_bGeometryDirty)
	{
		for (CAkSpatialAudioListener::tList::Iterator it = CAkSpatialAudioListener::List().Begin(); it != CAkSpatialAudioListener::List().End(); ++it)
		{
			CAkSpatialAudioListener* pListener = static_cast<CAkSpatialAudioListener*>(*it);
			if (pListener)
			{
				AkAcousticRoom* pRoom = m_Geometry.GetRoom(pListener->GetActiveRoom());
				if (pRoom == nullptr || pRoom->IsDirty())
				{
					pListener->m_PathsToPortals.RemoveAll();
					pListener->GetStochasticRays().RemoveAll();
				}
			}
		}

		for (CAkSpatialAudioEmitter::tList::Iterator it = CAkSpatialAudioEmitter::List().Begin(); it != CAkSpatialAudioEmitter::List().End(); ++it)
		{
			CAkSpatialAudioEmitter* pEmitter = static_cast<CAkSpatialAudioEmitter*>(*it);
			if (pEmitter)
			{
				AkAcousticRoom* pRoom = m_Geometry.GetRoom(pEmitter->GetActiveRoom());
				if (pRoom == nullptr || pRoom->IsDirty())
				{
					pEmitter->m_PathsToPortals.RemoveAll();
					pEmitter->GetStochasticPaths().RemoveAll();
					pEmitter->GetReflectionPaths().RemoveAll();
					pEmitter->GetDiffractionPaths().Reset();
				}
			}
		}

		m_Geometry.BuildVisibilityData(g_settings.taskSchedulerDesc, &g_SpatialAudioSettings);

		// Clear stale paths
		m_Geometry.ClearSoundPropagationPaths();
	}

	// Listener Updates
	for (CAkSpatialAudioListener::tList::Iterator it = CAkSpatialAudioListener::List().Begin(); it != CAkSpatialAudioListener::List().End(); ++it)
	{
		CAkSpatialAudioListener* pERLstnr = static_cast<CAkSpatialAudioListener*>(*it);
		if (pERLstnr->ID() == spatialAudioListenerID &&
			pERLstnr->GetOwner()->IsRegistered())
		{
			CAkSpatialAudioComponent* pSACmpt = pERLstnr->GetSpatialAudioComponent();
			CAkGameObject* pLstnrGameObj = (*it)->GetOwner();
			CAkListener* pListener = pLstnrGameObj->GetComponent<CAkListener>();
			if (pListener)
			{
				pSACmpt->UpdatePosition( g_SpatialAudioSettings.fMovementThreshold);
				
				UpdateListener(pERLstnr);

				// We need to remove the invalid ones instead of clearing the whole set
				if (pSACmpt->IsPositionDirty() || m_bGeometryDirty )
				{
					pERLstnr->InvalidatePathsToPortals();

					// Reset radius. It will be re-updated in the paths pre-pass below.
					pERLstnr->ResetVisibilityRadius();
				}
				
			}
		}
	}

	// Always run the listener independent task if the CPU limit is enabled.
	bool bRunListenerTask = false;

	for (CAkSpatialAudioEmitter::tList::Iterator it = CAkSpatialAudioEmitter::List().Begin(); it != CAkSpatialAudioEmitter::List().End(); ++it)
	{
		CAkSpatialAudioEmitter* pEmitter = static_cast<CAkSpatialAudioEmitter*>(*it);
		CAkSpatialAudioComponent* pEmitterComponent = pEmitter->GetSpatialAudioComponent();
		CAkSpatialAudioListener* pListener = pEmitter->GetListener();
		if (pListener != NULL)
		{
			CAkSpatialAudioComponent* pListenerComponent = pListener->GetSpatialAudioComponent();
			UpdateComponentRoomsAndAuxs(pEmitterComponent, pEmitter->GetPosition());

			if (pEmitterComponent->IsRoomDirty() || m_bGeometryDirty)
			{
				pEmitter->m_PathsToPortals.RemoveAll();
			}

			if (pEmitterComponent->HasBeenActivated())
			{
				bRunListenerTask = true;
			}

			if (pEmitterComponent->IsPositionDirty() || pListenerComponent->IsPositionDirty() || m_bGeometryDirty)
			{
				pEmitter->InvalidatePathsToPortals();

				if (pEmitter->GetOwner()->IsActive())
				{
					pListener->UpdateVisibilityRadius(pEmitter->GetMaxDistance());

					bool bAddedListenerPath = false;
					bool bAddedEmitterPath = false;

					const AkAcousticRoom* pEmitterRoom = m_Geometry.GetRoom(pEmitterComponent->GetActiveRoom());
					if (pEmitterRoom)
					{
						pEmitterRoom->InitContributingPortalPaths(pEmitter, pListener, pEmitter->GetMaxDistance(), m_portalRaycastTaskQueue, m_portalToPortalTaskQueue, bAddedEmitterPath, bAddedListenerPath);
					}
					
					if (pEmitterComponent->IsTransitioning())
					{
						// We need the transition paths to the portals in the transition room for reflection computation.
						pEmitterRoom = m_Geometry.GetRoom(pEmitterComponent->GetTransitionRoom());
						if (pEmitterRoom)
						{
							pEmitterRoom->InitContributingPortalPaths(pEmitter, pListener, pEmitter->GetMaxDistance(), m_portalRaycastTaskQueue, m_portalToPortalTaskQueue, bAddedEmitterPath, bAddedListenerPath);
						}
					}

					if (bAddedEmitterPath || 
						(pEmitterComponent->IsPositionDirty() && pEmitter->HasReflectionsOrDiffraction()) || 
						m_bGeometryDirty)
					{
						m_taskQueue.Enqueue(AkSAObjectTaskData::EmitterTask(pEmitter));
					}

					if (bAddedListenerPath)
					{
						// A new path to a portal was added and needs to be computed.
						bRunListenerTask = true;
					}
				}
				else
				{
					pEmitter->ClearPathsToListener();
				}

				pEmitter->ClearInvalidPathsToPortals();
			}
		}
	}

	// Take rooms into account for contributing portal paths. 
	for (CAkSpatialAudioRoomComponent::tList::Iterator itRoom = CAkSpatialAudioRoomComponent::List().Begin(); itRoom != CAkSpatialAudioRoomComponent::List().End(); ++itRoom)
	{
		CAkSpatialAudioComponent* pSAC = (*itRoom)->GetOwner()->GetComponent<CAkSpatialAudioComponent>();
		if (pSAC)
		{
			AkReal32 maxDistance = pSAC->GetMaxDistance();
			if (maxDistance > 0.f)
			{
				AkAcousticRoom* pRoom = m_Geometry.GetRoom(pSAC->ID());
				if (pRoom)
				{
					for (CAkSpatialAudioListener::tList::Iterator itListener = CAkSpatialAudioListener::List().Begin(); itListener != CAkSpatialAudioListener::List().End(); ++itListener)
					{
						CAkSpatialAudioListener* pSpatialAudioListener = static_cast<CAkSpatialAudioListener*>(*itListener);
						if (pSpatialAudioListener->ID() == spatialAudioListenerID &&
							pSpatialAudioListener->GetOwner()->IsRegistered())
						{
							bool bAddedListenerPath = false;
							pRoom->InitContributingPortalPaths_RoomTone(pSpatialAudioListener, maxDistance, m_portalRaycastTaskQueue, m_portalToPortalTaskQueue, bAddedListenerPath);

							pSpatialAudioListener->UpdateVisibilityRadius(maxDistance);

							if (bAddedListenerPath)
							{
								// A new path to a portal was added and needs to be computed.
								bRunListenerTask = true;
							}
						}
					}
				}
			}

		}
	}

	for (CAkSpatialAudioListener::tList::Iterator it = CAkSpatialAudioListener::List().Begin(); it != CAkSpatialAudioListener::List().End(); ++it)
	{
		CAkSpatialAudioListener* pListener = static_cast<CAkSpatialAudioListener*>(*it);
		if( bRunListenerTask || pListener->GetSpatialAudioComponent()->IsPositionDirty() || m_bGeometryDirty )
		{
			m_taskQueue.Enqueue(AkSAObjectTaskData::ListenerTask(pListener));
		}

		pListener->ClearInvalidPathsToPortals();
	}

	m_portalRaycastTaskQueue.Run(g_settings.taskSchedulerDesc, &m_Geometry, "AK::SpatialAudio::PortalRaycast");

	m_portalToPortalTaskQueue.Run(g_settings.taskSchedulerDesc, &m_Geometry, "AK::SpatialAudio::PortalPaths");

	m_taskQueue.Run(g_settings.taskSchedulerDesc, &m_Geometry, "AK::SpatialAudio::Independant");

	for (CAkSpatialAudioEmitter::tList::Iterator it = CAkSpatialAudioEmitter::List().Begin(); it != CAkSpatialAudioEmitter::List().End(); ++it)
	{
		CAkSpatialAudioEmitter* pEmitter = static_cast<CAkSpatialAudioEmitter*>(*it);
		CAkSpatialAudioComponent* pEmitterComponent = pEmitter->GetSpatialAudioComponent();
		CAkSpatialAudioListener* pListener = pEmitter->GetListener();

		if (pListener)
		{
			CAkSpatialAudioComponent* pListenerComponent = pListener->GetSpatialAudioComponent();
			
			if (pEmitter->GetOwner()->IsActive() &&
				(pEmitterComponent->IsPositionDirty() || pListenerComponent->IsPositionDirty() || m_bGeometryDirty))
			{
				pEmitter->ClearPathsToListener();

				if (pEmitter->HasReflections() || pEmitter->HasDiffraction())
				{
					m_taskQueue.Enqueue(AkSAObjectTaskData::EmitterListenerTask(pEmitter));
				}
			}
			else if (!pEmitter->GetOwner()->IsActive())
			{
				pEmitter->GetReflectionPaths().RemoveAll();
				pEmitter->GetDiffractionPaths().Reset();
			}
		}
		
	}

	m_taskQueue.Run(g_settings.taskSchedulerDesc, &m_Geometry, "AK::SpatialAudio::Dependant");


	// Update rooms
	for (CAkSpatialAudioListener::tList::Iterator it = CAkSpatialAudioListener::List().Begin(); it != CAkSpatialAudioListener::List().End(); ++it)
	{
		CAkSpatialAudioListener* pERLstnr = static_cast<CAkSpatialAudioListener*>(*it);
		if (pERLstnr->ID() == spatialAudioListenerID &&
			pERLstnr->GetOwner()->IsRegistered())
		{
			m_Geometry.UpdateRooms(pERLstnr, g_SpatialAudioSettings.uDiffractionFlags);
		}
	}
	

	// Emitter Updates 
	for (CAkSpatialAudioEmitter::tList::Iterator it = CAkSpatialAudioEmitter::List().Begin();
		it != CAkSpatialAudioEmitter::List().End(); ++it)
	{
		CAkSpatialAudioEmitter* pERE = static_cast<CAkSpatialAudioEmitter*>(*it);
		CAkSpatialAudioListener* pERL = pERE->GetListener();

		if (pERL != NULL)
		{
			pERE->PackageImageSources();

			UpdateEmitter(pERE, pERL);
		}
	}

	for (CAkSpatialAudioComponent::tList::Iterator it = CAkSpatialAudioComponent::List().Begin(); it != CAkSpatialAudioComponent::List().End(); ++it)
	{
		CAkSpatialAudioComponent* pSACmpt = static_cast<CAkSpatialAudioComponent*>(*it);
		pSACmpt->FinishUpdate();
	}

#ifndef AK_OPTIMIZED
	m_bSendGeometryUpdate = m_bSendGeometryUpdate || 
							m_bGeometryDirty;
#endif

	m_bGeometryDirty = false;
	ReleaseGeometryReadOnly();

	AKPLATFORM::PerformanceCounter(&m_lastExecTime);
}

static void SetRoomsAndAuxsHelper(CAkSpatialAudioComponent* in_pSpatialAudioCompnt, AkAcousticRoom* pRoomA, AkAcousticRoom* pRoomB, AkReal32 ratio, AkPortalID portal)
{
	AkRoomRvbSend roomA;
	pRoomA->GetRoomRvbSend(roomA);
	roomA.ctrlVal = (1.f - ratio);
	roomA.ctrlVal = 1.f - (roomA.ctrlVal*roomA.ctrlVal);
	roomA.ctrlVal *= pRoomA->GetReverbLevel();

	AkRoomRvbSend roomB;
	if (pRoomB != NULL)
	{
		pRoomB->GetRoomRvbSend(roomB);
		roomB.ctrlVal = 1.f - (ratio*ratio);
		roomB.ctrlVal *= pRoomB->GetReverbLevel();
	}

	in_pSpatialAudioCompnt->SetRoomsAndAuxs(roomA, roomB, ratio, portal);
}

AkAcousticRoom* CAkSpatialAudioPrivate::UpdateComponentRoomsAndAuxs(CAkSpatialAudioComponent* in_pSpatialAudioCompnt, const Ak3DVector& in_position)
{
	AkAcousticRoom* pRoomA = NULL;
	AkAcousticRoom* pRoomB = NULL;
	AkReal32 ratio = 1.0f;

	AkPortalID portal = in_pSpatialAudioCompnt->GetPortal();
	if (portal.IsValid())
	{
		// If the object was in a portal on the previous update, check to see if it is still inside that portal.

		AkAcousticPortal* pPortal = m_Geometry.GetPortal(portal);
		if (pPortal != NULL && pPortal->IsEnabled())
		{
			if (pPortal->GetTransitionRoom(in_position, pRoomA, pRoomB, ratio))
			{
				SetRoomsAndAuxsHelper(in_pSpatialAudioCompnt, pRoomA, pRoomB, ratio, portal);
				//
				return pRoomA;
			}
		}
	}


	// Get the room assigned by the api.
	pRoomA = m_Geometry.GetRoom(in_pSpatialAudioCompnt->GetAssignedRoom());
	if (pRoomA != NULL)
	{
		// Check if the object is inside a portal to determine the transition ratio (and actual room based on portal position).
		portal = pRoomA->GetTransitionRoom(in_position, pRoomA, pRoomB, ratio);

		SetRoomsAndAuxsHelper(in_pSpatialAudioCompnt, pRoomA, pRoomB, ratio, portal);
		//
		return pRoomA;
	}
	
	
	AkRoomRvbSend none;
	in_pSpatialAudioCompnt->SetRoomsAndAuxs(none, none, 1.f, AkPortalID());
	//
	return NULL;
}

void CAkSpatialAudioPrivate::UpdateListener(CAkSpatialAudioListener* pERLstnr)
{
	pERLstnr->GetRoomGameObjs().Clear();
	
	AkAcousticRoom* pRoom = UpdateComponentRoomsAndAuxs(pERLstnr->GetSpatialAudioComponent(), pERLstnr->GetPosition());
	
	if (pRoom != NULL)
	{
		if (pERLstnr->GetSpatialAudioComponent()->IsRoomDirty() || m_bGeometryDirty)
		{
			pERLstnr->m_PathsToPortals.RemoveAll();
			pRoom->PropagateSound(pERLstnr, AkMin(g_SpatialAudioSettings.uMaxSoundPropagationDepth, AK_MAX_SOUND_PROPAGATION_DEPTH));
		}
	}
}

void CAkSpatialAudioPrivate::UpdateEmitter(CAkSpatialAudioEmitter* pERE, CAkSpatialAudioListener* pERL)
{
	if (pERE->HasDiffraction())
	{
		if (pERL->GetActiveRoom() == pERE->GetActiveRoom())
		{
			pERE->GetDiffractionPaths().CalcDiffractionAndVirtualPos(pERL->GetPosition(), pERE->GetTransform());
		}
		else
		{
			const AkAcousticRoom* pEmitterRoom = m_Geometry.GetRoom(pERE->GetActiveRoom());
			if (pEmitterRoom != NULL)
			{
				AkPropagationPathArray paths;
				pEmitterRoom->GetPaths(pERE, pERL, paths);
				paths.BuildDiffractionPaths(pERL->GetPosition(), pERL->m_PathsToPortals, pERL->GetActiveRoom(), pERE->GetTransform(), pERE->m_PathsToPortals, pERE->GetActiveRoom(), pERE->GetMaxDistance(), pERE->GetDiffractionPaths());
			}
		}
	}
	
	pERE->BuildCustomRays(g_SpatialAudioSettings.uDiffractionFlags);
}

void CAkSpatialAudioPrivate::GarbageCollectGameObjs()
{
	for (CAkSpatialAudioRoomComponent::tList::Iterator it = CAkSpatialAudioRoomComponent::List().Begin(); it != CAkSpatialAudioRoomComponent::List().End();)
	{
		CAkSpatialAudioRoomComponent* pRoom = static_cast<CAkSpatialAudioRoomComponent*>(*it);
		++it;

		if (pRoom && !pRoom->GetOwner()->IsActive())
		{
			g_pRegistryMgr->UnregisterObject(pRoom->ID());
		}
	}
}

void CAkSpatialAudioPrivate::SetListenerID(AkGameObjectID in_ListenerID)
{
	m_bUseDefaultListeners = false;
	m_ListenerID = in_ListenerID;

	// Update the spatial audio rooms' listeners.
	AkAutoTermListenerSet roomListener;
	AkGameObjectID spatialAudioListener = GetListenerID();
	if (spatialAudioListener != AK_INVALID_GAME_OBJECT)
		roomListener.Set(GetListenerID());
	
	for (CAkSpatialAudioRoomComponent::tList::Iterator it = CAkSpatialAudioRoomComponent::List().Begin(); it != CAkSpatialAudioRoomComponent::List().End(); ++it)
		g_pRegistryMgr->UpdateListeners((*it)->ID(), roomListener, AkListenerOp_Set);
	

	g_pInstance->m_bGeometryDirty = true;
}

// Get the one and only spatial audio listener.
AkGameObjectID CAkSpatialAudioPrivate::GetListenerID() const
{
	if (m_bUseDefaultListeners)
	{
		CAkConnectedListeners* defaultListers = CAkConnectedListeners::GetDefault();
		if (defaultListers && defaultListers->GetListeners().Length() > 0)
			return defaultListers->GetListeners()[0];
	}

	return g_pInstance->m_ListenerID;
}

AkGameObjectID CAkSpatialAudioPrivate::GetRegisteredListenerID() const
{
	AkGameObjectID spatialAudioListener = GetListenerID();
	CAkRegisteredObj* pGameObj = g_pRegistryMgr->GetObjAndAddref(spatialAudioListener);
	if (pGameObj == NULL)
		spatialAudioListener = AK_INVALID_GAME_OBJECT;
	else
		pGameObj->Release();

	return spatialAudioListener;
}

#ifndef AK_OPTIMIZED
bool SerializeDiffractionPaths(MonitorSerializer& serializer, const CAkDiffractionPaths& diffraction)
{
	bool bRc = serializer.Put(diffraction.Length());
	
	AkUInt8 flags = 0;
	if (diffraction.HasDirectLineOfSight())
		flags |= 1U << 0;
	if (diffraction.IsOutOfRange())
		flags |= 1U << 1;

	bRc = bRc && serializer.Put(flags);

	for (AkUInt32 i = 0; i < diffraction.Length(); ++i)
	{
		AkDiffractionPath& dfrn = diffraction[i];

		bRc = bRc && serializer.Put(dfrn.nodeCount);
		bRc = bRc && serializer.Put(dfrn.totLength);
		bRc = bRc && serializer.Put(dfrn.diffraction); //dry diffraction
		bRc = bRc && serializer.Put(dfrn.obstructionValue);

		for (AkUInt32 idx = 0; idx < dfrn.nodeCount; idx++)
		{
			bRc = bRc && serializer.Put(dfrn.nodes[idx].X);
			bRc = bRc && serializer.Put(dfrn.nodes[idx].Y);
			bRc = bRc && serializer.Put(dfrn.nodes[idx].Z);

			bRc = bRc && serializer.Put(dfrn.angles[idx]);

			bRc = bRc && serializer.Put(dfrn.rooms[idx].id);
			bRc = bRc && serializer.Put(dfrn.portals[idx].id);
		}
	}

	return bRc;
}

bool SerializeReflectionsPaths(MonitorSerializer& serializer, CAkSpatialAudioEmitter* pEmitter)
{
	CAkReflectionPaths& paths = pEmitter->GetReflectionPaths();
	bool bRc = serializer.Put((AkUInt32)paths.Length());
	for (AkUInt32 i = 0; i < paths.Length(); i++)
	{
		AkReflectionPath& path = paths[i];

		Ak3DVector imageSource = path.CalcImageSource(pEmitter->GetPosition(), pEmitter->GetListener()->GetPosition());
		bRc = bRc && serializer.Put(imageSource.X);
		bRc = bRc && serializer.Put(imageSource.Y);
		bRc = bRc && serializer.Put(imageSource.Z);

		// number of reflections
		AkUInt32 numReflections = path.GetNumReflections();
		bRc = bRc && serializer.Put(numReflections);

		bRc = bRc && serializer.Put(path.numPathPoints);
		for (AkUInt32 j = 0; j < path.numPathPoints; j++)
		{
			Ak3DVector pt = Ak3DVector(path.pathPoint[j]);
			bRc = bRc && serializer.Put(pt.X);
			bRc = bRc && serializer.Put(pt.Y);
			bRc = bRc && serializer.Put(pt.Z);

			bRc = bRc && serializer.Put(path.diffraction[j]);
		}

		bRc = bRc && serializer.Put(path.level * paths.GetPortalFade(i, pEmitter));
	}

	return bRc;
}

bool ComputeHash(AkGameObjectID in_uiID, MonitorSerializer& serializer)
{
	bool bChanged = false;
	AK::FNVHash64 result;
	result.Compute(serializer.Bytes(), serializer.Count());
	AK::Hash64::HashType* pHash = g_GODataHashMap.Exists(in_uiID);
	if (pHash == NULL)
	{
		if ((pHash = g_GODataHashMap.Set(in_uiID)) != NULL)
		{
			*pHash = result.Get();
			bChanged = true;
		}
	}
	else
	{
		bChanged = (result.Get() != *pHash);
		if (bChanged)
			*pHash = result.Get();
	}
	return bChanged;
}
#endif

bool CAkSpatialAudioPrivate::GameObjectEmitterSerialize(CAkSpatialAudioEmitter* in_pEmitter, MonitorSerializer& serializer)
{
	bool bChanged = false;
#ifndef AK_OPTIMIZED
	bool bRc = serializer.Put(in_pEmitter->ID());

	AkUInt8 posIdx = 0; // unused for emitters
	bRc = bRc && serializer.Put(posIdx);

	const Ak3DVector& emitter = in_pEmitter->GetPosition();
	bRc = bRc && serializer.Put(emitter.X);
	bRc = bRc && serializer.Put(emitter.Y);
	bRc = bRc && serializer.Put(emitter.Z);

	const Ak3DVector& listener = in_pEmitter->GetListener()->GetPosition();
	bRc = bRc && serializer.Put(listener.X);
	bRc = bRc && serializer.Put(listener.Y);
	bRc = bRc && serializer.Put(listener.Z);

	// Serialize diffraction paths -----------------

	bRc = bRc && SerializeDiffractionPaths(serializer, in_pEmitter->GetDiffractionPaths());
	bRc = bRc && SerializeReflectionsPaths(serializer, in_pEmitter);

	bChanged = ComputeHash(in_pEmitter->ID(), serializer);
#endif
	return bChanged;
}

bool CAkSpatialAudioPrivate::RoomSerialize(AkGameObjectID in_uiID, MonitorSerializer& serializer, AkUInt32& in_uiNumChanged)
{
	bool bChanged = false;
#ifndef AK_OPTIMIZED
	bool bRc = true;
	AkAcousticRoom* pRoom = m_Geometry.GetRoom(in_uiID);
	if (pRoom != NULL)
	{
		const AkAcousticPortalArray& portals = pRoom->GetPortals();
		for (AkAcousticPortalArray::Iterator portalIt = portals.Begin(); portalIt != portals.End(); ++portalIt)
		{
			CAkDiffractionPaths& portalDiffractionPaths = (*portalIt)->GetDiffractionPaths();
			AkUInt32 numChanged = in_uiNumChanged;
			if (!portalDiffractionPaths.IsEmpty())
			{
				AkDiffractionPath& firstPath = portalDiffractionPaths[0];
				if (!(firstPath.nodeCount > 0 &&
					firstPath.rooms[firstPath.nodeCount - 1] == pRoom->GetID()))
				{
					++in_uiNumChanged;

					bRc = bRc && serializer.Put(in_uiID);

					AkUInt8 portalIdx = (AkUInt8)(portalIt.pItem - portals.Data());
					bRc = bRc && serializer.Put(portalIdx);

					Ak3DVector& emitter = firstPath.emitterPos;
					bRc = bRc && serializer.Put(emitter.X);
					bRc = bRc && serializer.Put(emitter.Y);
					bRc = bRc && serializer.Put(emitter.Z);

					const Ak3DVector& listener = firstPath.listenerPos;
					bRc = bRc && serializer.Put(listener.X);
					bRc = bRc && serializer.Put(listener.Y);
					bRc = bRc && serializer.Put(listener.Z);

					bRc = bRc && SerializeDiffractionPaths(serializer, portalDiffractionPaths);

					// no reflections
					bRc = bRc && serializer.Put((AkUInt32)0);
				}
				// else 
				// path is going the wrong way through the portal
				// This portal will be reported when the adjacent room comes around. Avoids reporting portals twice.
			}
			// We didn't write anything, send out data with no diffraction paths so authoring can catch a change in data when we transition from paths to no paths.
			// As long as something was written we'd have updated data for the room.
			if (in_uiNumChanged == numChanged)
			{
				++in_uiNumChanged;

				bRc = bRc && serializer.Put(in_uiID);

				AkUInt8 portalIdx = (AkUInt8)(portalIt.pItem - portals.Data());
				bRc = bRc && serializer.Put(portalIdx);

				bRc = bRc && serializer.Put((AkReal32)0.0);
				bRc = bRc && serializer.Put((AkReal32)0.0);
				bRc = bRc && serializer.Put((AkReal32)0.0);

				bRc = bRc && serializer.Put((AkReal32)0.0);
				bRc = bRc && serializer.Put((AkReal32)0.0);
				bRc = bRc && serializer.Put((AkReal32)0.0);

				// no diffractions, length == 0
				bRc = bRc && serializer.Put((AkUInt32)0);
				// flags
				bRc = bRc && serializer.Put((AkUInt8)0);

				// no reflections
				bRc = bRc && serializer.Put((AkUInt32)0);
			}
		}

		bChanged = ComputeHash(in_uiID, serializer);
	}
#endif
	return bChanged;
}

void CAkSpatialAudioPrivate::MonitorData()
{
#ifndef AK_OPTIMIZED

	if ((AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataSpatialAudioObjects)))
	{
		bool bRc = true;

		AkVarLenDataCreator creator(AkMonitorData::MonitorDataSpatialAudioObjects);
		MonitorSerializer& serializer = creator.GetSerializer();

		bRc = bRc && serializer.Put((AkUInt32)CAkSpatialAudioListener::List().Length());
		for (CAkSpatialAudioListener::tList::Iterator it = CAkSpatialAudioListener::List().Begin(); it != CAkSpatialAudioListener::List().End(); ++it)
		{
			CAkSpatialAudioListener* pListener = static_cast<CAkSpatialAudioListener*>(*it);
			bRc = bRc && serializer.Put(pListener->ID());
			bRc = bRc && serializer.Put(pListener->GetActiveRoom().id);
		}

		bRc = bRc && serializer.Put((AkUInt32)CAkSpatialAudioEmitter::List().Length());
		for (CAkSpatialAudioEmitter::tList::Iterator it = CAkSpatialAudioEmitter::List().Begin(); it != CAkSpatialAudioEmitter::List().End(); ++it)
		{
			CAkSpatialAudioEmitter* pEmitter = static_cast<CAkSpatialAudioEmitter*>(*it);
			bRc = bRc && serializer.Put(pEmitter->ID());
			bRc = bRc && serializer.Put(pEmitter->GetActiveRoom().id);
		}

		AkUInt32 numChangedObjs = 0;
		AkUInt32 numChangedObjsPtr = serializer.Count();
		bRc = bRc && serializer.Put(numChangedObjs);

		{
			MonitorSerializer tmpserializer(false);
			tmpserializer.SetMemPool(AkMemID_Profiler); // See AkMonitor::StartMonitoring()

			// Sufficient to go through this list and not all game objects
			for (CAkSpatialAudioEmitter::tList::Iterator it = CAkSpatialAudioEmitter::List().Begin(); it != CAkSpatialAudioEmitter::List().End(); ++it)
			{
				CAkSpatialAudioEmitter* pEmitter = static_cast<CAkSpatialAudioEmitter*>(*it);
				// We've always been sending only if both emitter and listener are defined - there's been no provision for sending only the emitter position
				if (pEmitter != NULL && pEmitter->GetListener() != NULL && GameObjectEmitterSerialize(pEmitter, tmpserializer))
				{
					AkInt32 lDummy = 0;
					serializer.WriteBytes(tmpserializer.Bytes(), tmpserializer.Count(), lDummy);
					++numChangedObjs;
				}
				tmpserializer.Clear();
			}

			for (CAkSpatialAudioRoomComponent::tList::Iterator it = CAkSpatialAudioRoomComponent::List().Begin(); it != CAkSpatialAudioRoomComponent::List().End(); ++it)
			{
				AkUInt32 numChanged = 0;
				if (RoomSerialize((*it)->GetOwner()->ID(), tmpserializer, numChanged))
				{
					AkInt32 lDummy = 0;
					serializer.WriteBytes(tmpserializer.Bytes(), tmpserializer.Count(), lDummy);
					numChangedObjs += numChanged;
				}
				tmpserializer.Clear();
			}
		}

		if (numChangedObjs)
		{
			AkUInt32 savePtr = serializer.Count();
			serializer.SetCount(numChangedObjsPtr);
			bRc = bRc && serializer.Put(numChangedObjs);
			serializer.SetCount(savePtr);
		}

	}

	if (m_bSendGeometryUpdate)
	{
		m_Geometry.MonitorData();
		m_bSendGeometryUpdate = false;
	}
	
#endif
}

void CAkSpatialAudioPrivate::MonitorRecap()
{
	m_Geometry.MonitorData();
#ifndef AK_OPTIMIZED
	m_bSendGeometryUpdate = false;
#endif
}

}
}