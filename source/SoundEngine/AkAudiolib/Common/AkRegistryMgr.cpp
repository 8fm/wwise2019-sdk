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
// AkRegistryMgr.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkRegistryMgr.h"
#include "AkRegisteredObj.h"
#include "Ak3DListener.h"
#include "AkParameterNodeBase.h"
#include "AkMonitor.h"
#include "AkRTPCMgr.h"
#include "AkEnvironmentsMgr.h"
#include "AkLEngine.h"
#include "AkConnectedListeners.h"
#include "AkQueuedMsg.h"
#include "AkSpatialAudioComponent.h"

// -------------------------------------------------------------
// Globals
// -------------------------------------------------------------
#ifndef AK_OPTIMIZED
AkUInt32 CAkRegistryMgr::g_uSoloGameObjCount = 0;
AkUInt32 CAkRegistryMgr::g_uMuteGameObjCount = 0;
#endif

#define MIN_SIZE_REG_ENTRIES 8  //ListPolldID Size / 4bytes

CAkRegistryMgr::CAkRegistryMgr()
{
}

CAkRegistryMgr::~CAkRegistryMgr()
{
}

AKRESULT CAkRegistryMgr::Init()
{
	AKRESULT eResult = m_listModifiedNodes.Reserve(MIN_SIZE_REG_ENTRIES);
#ifndef AK_OPTIMIZED
	m_relativePositionTransport = AkPositionStore::GetDefaultPosition().position;
#endif
	return eResult;
}

void CAkRegistryMgr::Term()
{
	UnregisterAll(NULL);
	UnregisterObject(AkGameObjectID_Transport);
	m_mapRegisteredObj.Term();
	m_listModifiedNodes.Term();
}

CAkRegisteredObj * CAkRegistryMgr::GetObjAndAddref(AkGameObjectID in_GameObjectID)
{
	CAkRegisteredObj** l_ppObj = m_mapRegisteredObj.Exists( in_GameObjectID );
	if (!l_ppObj)
		return NULL;

	(*l_ppObj)->AddRef();

	return (*l_ppObj);
}

AKRESULT CAkRegistryMgr::RegisterObject(AkGameObjectID in_GameObjectID, void * in_pMonitorData)
{
	bool bSuccess = false;
	CAkRegisteredObj** l_ppObj = NULL;
	CAkRegisteredObj* l_pObj = NULL;
	
	l_ppObj = m_mapRegisteredObj.Exists(in_GameObjectID);
	if (l_ppObj)
	{
		l_pObj = *l_ppObj;
		MONITOR_FREESTRING(in_pMonitorData);
		in_pMonitorData = NULL;
	}
	else
	{
		l_pObj = AkNew(AkMemID_GameObject, CAkRegisteredObj(in_GameObjectID));
		if (l_pObj)
		{
			l_ppObj = m_mapRegisteredObj.Set(in_GameObjectID);
			if (l_ppObj)
			{
				*l_ppObj = l_pObj;
			}
		}
	}

	if (l_pObj)
	{
		if (l_pObj->GetComponent<CAkEmitter>() == NULL)
		{
			//Always ensure that the emitter component exists. 
			CAkEmitter* pEmitter = l_pObj->CreateComponent<CAkEmitter>();
			if (pEmitter)
			{
				// If we create a new emitter component, set initialize to the default position.
				pEmitter->SetPosition(
					&AkPositionStore::GetDefaultPosition(),
					1,
					AK::SoundEngine::MultiPositionType_SingleSource
				);

				bSuccess = true;
			}
		}
		else
		{
			bSuccess = true;
		}
		
		// Create the listener component if any associations to this game object as a listener already exist. 
		for (CAkConnectedListeners::tList::Iterator it = CAkConnectedListeners::List().Begin(); it != CAkConnectedListeners::List().End(); ++it)
		{
			CAkConnectedListeners& item = (CAkConnectedListeners&)(**it);
			if (item.GetListeners().Contains(in_GameObjectID))
			{
				CAkListener* pData = l_pObj->CreateComponent<CAkListener>();
				if (pData)
				{
					bSuccess = true;
				}
			}
		}
	}

	if (bSuccess)
	{
#ifndef AK_OPTIMIZED
		if (CAkRegistryMgr::IsMonitoringSoloActive())
			l_pObj->UpdateImplicitSolo(false);

		MONITOR_OBJREGISTRATION(AkMonitorData::MakeObjRegFlags(true, l_pObj->IsSoloExplicit(), l_pObj->IsSoloImplicit(), l_pObj->IsMute(), true), in_GameObjectID, in_pMonitorData);
#endif
		return AK_Success;
	}
	else
	{
		if (l_pObj)
		{
			AkDelete(AkMemID_GameObject, l_pObj);
			l_pObj = NULL;
		}

		if (l_ppObj)
		{
			m_mapRegisteredObj.Unset(in_GameObjectID);
			l_ppObj = NULL;
		}

		MONITOR_FREESTRING(in_pMonitorData);
		return AK_Fail;
	}
}

void CAkRegistryMgr::UnregisterObject(AkGameObjectID in_GameObjectID)
{
	AkUInt32 uFlags = 0;
	AkMapRegisteredObj::IteratorEx it = m_mapRegisteredObj.FindEx( in_GameObjectID );
	if( it != m_mapRegisteredObj.End() )
	{
#ifndef AK_OPTIMIZED
		ClearSoloMuteGameObj(in_GameObjectID);
#endif
		CAkRegisteredObj* pGameObj = (*it).item;

		uFlags = pGameObj->GetComponentFlags();

		m_mapRegisteredObj.Erase(it);

		pGameObj->SetRegistered(false);
		pGameObj->Release();

		// Unregistered
		MONITOR_OBJREGISTRATION(AkMonitorData::MakeObjRegFlags(false, false, false, false, true), in_GameObjectID, NULL);
	}

}
#ifndef AK_OPTIMIZED
AKRESULT CAkRegistryMgr::MonitoringMuteGameObj(AkGameObjectID in_GameObj, bool in_bMute, bool in_bNotify)
{
	CAkRegisteredObj** ppObj = m_mapRegisteredObj.Exists(in_GameObj);
	if (!ppObj)
		return AK_Fail;

	(*ppObj)->MuteGameObject(in_bMute, in_bNotify);

	return AK_Success;
}

AKRESULT CAkRegistryMgr::MonitoringSoloGameObj(AkGameObjectID in_GameObj, bool in_bSolo, bool in_bNotify)
{
	CAkRegisteredObj** ppObj = m_mapRegisteredObj.Exists(in_GameObj);
	if (!ppObj)
		return AK_Fail;

	(*ppObj)->SoloGameObject(in_bSolo, in_bNotify);

	return AK_Success;
}


void CAkRegistryMgr::ClearSoloMuteGameObj(AkGameObjectID in_GameObjectID)
{
	MonitoringSoloGameObj(in_GameObjectID, false);
	MonitoringMuteGameObj(in_GameObjectID, false);
}

void CAkRegistryMgr::ResetMonitoringMuteSolo()
{
	g_uSoloGameObjCount = 0;
	g_uMuteGameObjCount = 0;
	CAkParameterNodeBase::SetMonitoringMuteSoloDirty();
}

void CAkRegistryMgr::ClearMonitoringSoloMuteGameObj()
{
	for (AkMapRegisteredObj::Iterator iter = m_mapRegisteredObj.Begin(); iter != m_mapRegisteredObj.End(); ++iter)
	{
		MonitoringMuteGameObj((*iter).item->ID(), false, false);
		MonitoringSoloGameObj((*iter).item->ID(), false, false);
	}		
}

void CAkRegistryMgr::RefreshImplicitSolos(bool in_bNotify)
{
	if (g_uSoloGameObjCount > 0)
	{
		for (CAkRegistryMgr::AkMapRegisteredObj::Iterator it = m_mapRegisteredObj.Begin(); it != m_mapRegisteredObj.End(); ++it)
		{
			(*it).item->ClearImplicitSolo(in_bNotify);
		}

		for (CAkRegistryMgr::AkMapRegisteredObj::Iterator it = m_mapRegisteredObj.Begin(); it != m_mapRegisteredObj.End(); ++it)
		{
			CAkRegisteredObj* pObj = (*it).item;
			if (pObj->IsSoloExplicit())
				pObj->PropagateImplicitSolo(in_bNotify);
		}
	}

	CAkParameterNodeBase::SetMonitoringMuteSoloDirty();
}

void CAkRegistryMgr::SetRelativePosition( 
	const AkSoundPosition & in_rPosition
	)
{
	m_relativePositionTransport = in_rPosition;
	NotifyListenerPosChanged( AkListenerSet() );
}
#endif

AKRESULT CAkRegistryMgr::SetPosition( 
	AkGameObjectID in_GameObjectID, 
	const AkChannelEmitter* in_aPositions,
	AkUInt32 in_uNumPositions,
	AK::SoundEngine::MultiPositionType in_eMultiPositionType
	)
{
	AKRESULT res = AK_Fail;

	CAkRegisteredObj** ppObj = m_mapRegisteredObj.Exists(in_GameObjectID);
	if (ppObj)
	{
		CAkEmitter* pEmitter = (*ppObj)->GetComponent<CAkEmitter>();
		if (pEmitter)
		{
			pEmitter->SetPosition(in_aPositions, in_uNumPositions, in_eMultiPositionType);
			res = AK_Success;
		}

		if (in_uNumPositions > 0)
		{
			CAkListener* pListener = (*ppObj)->GetComponent<CAkListener>();
			if (pListener)
			{
				pListener->SetPosition(in_aPositions[0].position);
				res = AK_Success;
			}
		}
	}

	return res;
}

void CAkRegistryMgr::UpdateListeners( AkGameObjectID in_GameObjectID, const AkListenerSet& in_listeners, AkListenerOp in_operation )
{	
	CAkRegisteredObj** ppObj = m_mapRegisteredObj.Exists(in_GameObjectID);
	if (ppObj && *ppObj)
	{		
		CAkConnectedListeners* pCl = (*ppObj)->CreateComponent<CAkConnectedListeners>();
		if (pCl)
		{
			if (in_operation == AkListenerOp_Set)
			{
				EnsureListenersExist(in_listeners);
				pCl->SetListeners(in_listeners);
			}
			else if (in_operation == AkListenerOp_Add)
			{
				EnsureListenersExist(in_listeners);
				pCl->AddListeners(in_listeners);
			}
			else if (in_operation == AkListenerOp_Remove)
			{
				pCl->RemoveListeners(in_listeners);
			}

			CAkLEngine::ReevaluateGraph();
#ifndef AK_OPTIMIZED
			RefreshImplicitSolos(true);
#endif

			CAkEmitter* pEmitter = (*ppObj)->GetComponent<CAkEmitter>();
			if (pEmitter)
			{
				pEmitter->NotifyPositionDirty();
				pEmitter->GetPosition().ClearDataForListenersNotInSet(pCl->GetListeners());
			}
		}
	}
	else
	{
		MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_GameObjActiveListeners, AK_INVALID_PLAYING_ID, in_GameObjectID, AK_INVALID_UNIQUE_ID, false);
	}
}

void CAkRegistryMgr::UpdateDefaultListeners( const AkListenerSet& in_listeners, AkListenerOp in_operation )
{
	if (in_operation != AkListenerOp_Remove)
		EnsureListenersExist(in_listeners);

	CAkConnectedListeners* pCl = CAkConnectedListeners::GetDefault();
	if(pCl != NULL)
	{

		if (in_operation == AkListenerOp_Set)
			pCl->SetListeners(in_listeners);
		else if (in_operation == AkListenerOp_Add)
			pCl->AddListeners(in_listeners);
		else if (in_operation == AkListenerOp_Remove)
			pCl->RemoveListeners(in_listeners);

		CAkLEngine::ReevaluateGraph();
#ifndef AK_OPTIMIZED
		RefreshImplicitSolos(true);
#endif

		// Set the position dirty on all emitters that do not override the default set of listeners
		for (CAkEmitter::tList::Iterator it = CAkEmitter::List().Begin(); it != CAkEmitter::List().End(); ++it)
		{
			CAkEmitter& emitter = (CAkEmitter&)(**it);
			CAkConnectedListeners* pConnLstnrs = emitter.GetOwner()->GetComponent<CAkConnectedListeners>();
			if (pConnLstnrs == NULL || !pConnLstnrs->OverrideUserDefault())
			{
				emitter.NotifyPositionDirty();
				emitter.GetPosition().ClearDataForListenersNotInSet(pCl->GetListeners());
			}
		}
	}

}

void CAkRegistryMgr::ResetListenersToDefault( AkGameObjectID in_GameObjectID )
{
	CAkRegisteredObj** ppObj = m_mapRegisteredObj.Exists(in_GameObjectID);
	if(ppObj && *ppObj)
	{	
		CAkConnectedListeners* pCl = (*ppObj)->GetComponent<CAkConnectedListeners>();
		if (pCl)
		{
			pCl->ResetListenersToDefault();

			CAkLEngine::ReevaluateGraph();
#ifndef AK_OPTIMIZED
			RefreshImplicitSolos(true);
#endif

			CAkEmitter* pEmitter = (*ppObj)->CreateComponent<CAkEmitter>();
			if (pEmitter)
			{
				pEmitter->NotifyPositionDirty();
				pEmitter->GetPosition().ClearDataForListenersNotInSet(pCl->GetListeners());
			}
		}
	}
	else
	{
		MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_ResetListeners, AK_INVALID_PLAYING_ID, in_GameObjectID, AK_INVALID_UNIQUE_ID, false);
	}
}

AKRESULT CAkRegistryMgr::GetPosition( 
	AkGameObjectID in_GameObjectID, 
	const AkSoundPositionRef *& out_Position)
{
	out_Position = NULL;
	CAkEmitter* pEmitter = GetGameObjComponent<CAkEmitter>(in_GameObjectID);
	if (!pEmitter)
		return AK_Fail;

	out_Position = &pEmitter->GetPosition();

	return AK_Success;
}

void CAkRegistryMgr::SetGameObjectAuxSendValues(
	AkGameObjectID		in_emitterID,
	AkAuxSendValue*		in_aEnvironmentValues,
	AkUInt32			in_uNumEnvValues
	)
{
	CAkRegisteredObj** ppEmObj = m_mapRegisteredObj.Exists(in_emitterID);
	if (ppEmObj)
	{
		SetGameObjectAuxSendValues(*ppEmObj, in_aEnvironmentValues, in_uNumEnvValues);
	}
	else
	{
		MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_GameObjEnvValues, AK_INVALID_PLAYING_ID, in_emitterID, AK_INVALID_UNIQUE_ID, false);
	}
}

void CAkRegistryMgr::SetGameObjectAuxSendValues(
	CAkGameObject*		in_pEmitter,
	AkAuxSendValue*		in_aEnvironmentValues,
	AkUInt32			in_uNumEnvValues
	)
{
	CAkConnectedListeners* pConnLstnrs = in_pEmitter->CreateComponent<CAkConnectedListeners>();
	if (pConnLstnrs)
	{
		//Ensure that listener components exist for all the listener id's passed in.
		for (AkUInt32 i = 0; i < in_uNumEnvValues; ++i)
		{
			AkGameObjectID listenerID = in_aEnvironmentValues[i].listenerID;
			if (listenerID != AK_INVALID_GAME_OBJECT) //We allow the user to pass in AK_INVALID_GAME_OBJECT to mean "use the listeners from the SetListeners() API"
			{
				EnsureListenerExists(listenerID);
			}
		}

#ifndef AK_OPTIMIZED
		AkListenerSet prevAuxListeners;
		prevAuxListeners.Copy(pConnLstnrs->GetAuxAssocs().GetListeners());
#endif
		pConnLstnrs->SetAuxGains(in_aEnvironmentValues, in_uNumEnvValues);

		CAkEmitter* pEmitter = in_pEmitter->CreateComponent<CAkEmitter>();
		if (pEmitter)
		{
			pEmitter->NotifyPositionDirty();
			pEmitter->GetPosition().ClearDataForListenersNotInSet(pConnLstnrs->GetListeners());
		}

#ifndef AK_OPTIMIZED
		const AkListenerSet& curAuxListeners = pConnLstnrs->GetAuxAssocs().GetListeners();

		bool bRefresh = prevAuxListeners.Length() != curAuxListeners.Length();

		for (AkUInt32 i = 0; !bRefresh && i < prevAuxListeners.Length(); ++i)
			bRefresh = prevAuxListeners[i] != curAuxListeners[i];

		if (bRefresh)
			RefreshImplicitSolos(true);

		prevAuxListeners.Term();
#endif
	}
}

void CAkRegistryMgr::SetGameObjectOutputBusVolume( 
		AkGameObjectID		in_emitterID,
		AkGameObjectID		in_ListenerID,
		AkReal32			in_fControlValue
		)
{
	CAkRegisteredObj** ppEmObj = m_mapRegisteredObj.Exists(in_emitterID);
	if (ppEmObj)
	{
		CAkConnectedListeners* pConnLstnrs = (*ppEmObj)->CreateComponent<CAkConnectedListeners>();
		if (pConnLstnrs)
		{
			AkDeltaMonitor::OpenEmitterListenerBrace(in_emitterID, AkDelta_SetGameObjectOutputBusVolume, AkPropID_OutputBusVolume);

			if (in_ListenerID == AK_INVALID_GAME_OBJECT)
			{
				//Allow the user to pass in AK_INVALID_GAME_OBJECT as the listener ID to set the gains for all listeners.
				//	However, doing so will override the default.
				pConnLstnrs->SetAllUserGains(in_fControlValue);

#ifndef AK_OPTIMIZED
				const AkListenerSet& userListeners = pConnLstnrs->GetUserAssocs().GetListeners();
				for (AkListenerSet::Iterator listenerIter = userListeners.Begin(); listenerIter != userListeners.End(); ++listenerIter)
				{
					AkDeltaMonitor::LogListenerDelta(*listenerIter, in_fControlValue);
				}
#endif
			}
			else
			{
				EnsureListenerExists(in_ListenerID);
				pConnLstnrs->SetUserGain(in_ListenerID, in_fControlValue);
				AkDeltaMonitor::LogListenerDelta(in_ListenerID, in_fControlValue);
			}

			AkDeltaMonitor::CloseEmitterListenerBrace();

			CAkEmitter* pEmitter = (*ppEmObj)->GetComponent<CAkEmitter>();
			if (pEmitter)
			{
				pEmitter->NotifyPositionDirty();
				pEmitter->GetPosition().ClearDataForListenersNotInSet(pConnLstnrs->GetListeners());
			}
		}
	}
	else
	{
		MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_GameObjDryLevel, AK_INVALID_PLAYING_ID, in_emitterID, AK_INVALID_UNIQUE_ID, false);
	}
}

AKRESULT CAkRegistryMgr::SetGameObjectScalingFactor(
		AkGameObjectID		in_GameObjectID,
		AkReal32			in_fControlValue
		)
{
	//#TODO_3dBus - consolidate scaling factor
	AKRESULT res = AK_Fail;

	CAkRegisteredObj** ppObj = m_mapRegisteredObj.Exists(in_GameObjectID);
	if (ppObj)
	{
		CAkEmitter* pEmitter = (*ppObj)->GetComponent<CAkEmitter>();
		if (pEmitter)
		{
			pEmitter->SetScalingFactor(in_fControlValue);
			res = AK_Success;
		}

		CAkListener* pListener = (*ppObj)->GetComponent<CAkListener>();
		if (pListener)
		{
			pListener->SetScalingFactor(in_fControlValue);
			res = AK_Success;
		}
	}

	return AK_Success;
}

AKRESULT CAkRegistryMgr::SetObjectObstructionAndOcclusion(
	AkGameObjectID in_EmitterID,		///< Game object ID.
	AkGameObjectID in_ListenerID,		///< Listener ID.
	AkReal32 in_fObstructionLevel,		///< ObstructionLevel : [0.0f..1.0f]
	AkReal32 in_fOcclusionLevel			///< OcclusionLevel   : [0.0f..1.0f]
	)
{
	CAkRegisteredObj** ppEmitterObj = m_mapRegisteredObj.Exists(in_EmitterID);
	if (ppEmitterObj)
	{
		return SetObjectObstructionAndOcclusion(*ppEmitterObj, in_ListenerID, in_fObstructionLevel, in_fOcclusionLevel);
	}
	else
	{
		MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_GameObjObstruction, AK_INVALID_PLAYING_ID, in_EmitterID, AK_INVALID_UNIQUE_ID, false);
		return AK_Fail;
	}
}

AKRESULT CAkRegistryMgr::SetObjectObstructionAndOcclusion(  
	CAkGameObject* in_pEmitterObj,	///< Game object pointer.
	AkGameObjectID in_ListenerID,		///< Listener ID.
	AkReal32 in_fObstructionLevel,		///< ObstructionLevel : [0.0f..1.0f]
	AkReal32 in_fOcclusionLevel			///< OcclusionLevel   : [0.0f..1.0f]
	)
{
	EnsureListenerExists(in_ListenerID);

	CAkEmitter* pEmitter = in_pEmitterObj->CreateComponent<CAkEmitter>();
	if (pEmitter)
	{
		return pEmitter->SetObjectObstructionAndOcclusion(in_ListenerID, in_fObstructionLevel, in_fOcclusionLevel);
	}
	
	return AK_Fail;
}

AKRESULT CAkRegistryMgr::SetMultipleObstructionAndOcclusion(
	AkGameObjectID in_EmitterID,
	AkGameObjectID in_ListenerID,
	AkObstructionOcclusionValues* in_ObstructionOcclusionValue,
	AkUInt32 in_uNumObstructionOcclusion
)
{
	CAkRegisteredObj** ppEmitterObj = m_mapRegisteredObj.Exists(in_EmitterID);
	if (ppEmitterObj)
	{
		return SetMultipleObstructionAndOcclusion(*ppEmitterObj, in_ListenerID, in_ObstructionOcclusionValue, in_uNumObstructionOcclusion);
	}
	else
	{
		MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_UnknownGameObject, QueuedMsgType_GameObjMultipleObstruction, AK_INVALID_PLAYING_ID, in_EmitterID, AK_INVALID_UNIQUE_ID, false);
		return AK_Fail;
	}
}

AKRESULT CAkRegistryMgr::SetMultipleObstructionAndOcclusion(
	CAkGameObject* in_pEmitterObj,
	AkGameObjectID in_ListenerID,
	AkObstructionOcclusionValues* in_ObstructionOcclusionValue,
	AkUInt32 in_uNumObstructionOcclusion
	)
{
	EnsureListenerExists(in_ListenerID);

	CAkEmitter* pEmitter = in_pEmitterObj->CreateComponent<CAkEmitter>();
	if (pEmitter)
	{
		return pEmitter->SetMultipleObstructionAndOcclusion(in_ListenerID, in_ObstructionOcclusionValue, in_uNumObstructionOcclusion);
	}
	
	return AK_Fail;
}

AkSwitchHistItem CAkRegistryMgr::GetSwitchHistItem( 
	CAkRegisteredObj *		in_pGameObj,
	AkUniqueID          in_SwitchContID
	)
{
	AKASSERT( in_pGameObj );
	if ( in_pGameObj != NULL )
	{
		CAkSwitchHistory* pHistory = in_pGameObj->GetComponent<CAkSwitchHistory>();
		if (pHistory)
		{
			AkSwitchHistItem * pSwitchHistItem = pHistory->History.Exists(in_SwitchContID);
			if (pSwitchHistItem)
				return *pSwitchHistItem;
		}
	}

	AkSwitchHistItem item;
	item.LastSwitch = AK_INVALID_UNIQUE_ID;
	item.NumPlayBack = 0;
	return item;
}

AKRESULT CAkRegistryMgr::SetSwitchHistItem( 
	CAkRegisteredObj *	in_pGameObj,
	AkUniqueID          in_SwitchContID,
	const AkSwitchHistItem & in_SwitchHistItem
	)
{
	AKASSERT( in_pGameObj );
	if ( in_pGameObj != NULL )
	{
		CAkSwitchHistory* pHistory = in_pGameObj->CreateComponent<CAkSwitchHistory>();
		if (pHistory)
		{
			return pHistory->History.Set(in_SwitchContID, in_SwitchHistItem) ? AK_Success : AK_Fail;
		}
	}
	return AK_Fail;
}

AKRESULT CAkRegistryMgr::ClearSwitchHist(
	AkUniqueID          in_SwitchContID,
	CAkRegisteredObj *	in_pGameObj
	)
{
	if( in_pGameObj != NULL )
	{
		CAkSwitchHistory* pHistory = in_pGameObj->GetComponent<CAkSwitchHistory>();
		if (pHistory)
			pHistory->History.Unset(in_SwitchContID);
	}
	else
	{
		for (CAkSwitchHistory::tList::Iterator it = CAkSwitchHistory::List().Begin(); it != CAkSwitchHistory::List().End(); ++it)
			((CAkSwitchHistory*)(*it))->History.Unset(in_SwitchContID);
	}

	return AK_Success;
}

void CAkRegistryMgr::UnregisterAll(const AkListenerSet* in_pExcept)
{
	for (AkMapRegisteredObj::IteratorEx iter = m_mapRegisteredObj.BeginEx(); iter != m_mapRegisteredObj.End();)
	{
		CAkRegisteredObj* pObj = (*iter).item;

		if ((*iter).key != AkGameObjectID_Transport && 
			(in_pExcept == NULL || !in_pExcept->Contains((*iter).key)))
		{
#ifndef AK_OPTIMIZED
			MonitoringSoloGameObj(pObj->ID(), false);
			MonitoringMuteGameObj(pObj->ID(), false);
#endif
			pObj->SetRegistered(false);

			MONITOR_OBJREGISTRATION(AkMonitorData::MakeObjRegFlags(false, false, false, false, true), (*iter).key, NULL);

#ifndef AK_OPTIMIZED
			ClearSoloMuteGameObj(pObj->ID());
#endif
			pObj->Release();

			iter = m_mapRegisteredObj.Erase(iter);
		}
		else
			++iter;
	}
}

void CAkRegistryMgr::SetNodeIDAsModified(CAkParameterNodeBase* in_pNode)
{
	AKASSERT(in_pNode);
	WwiseObjectID wwiseId( in_pNode->ID(), in_pNode->IsBusCategory() );

	if( !m_listModifiedNodes.Exists( wwiseId ) )
	{
		// What if this AddLast fails... Should handle it correctly, allowing memory to be released when unregistering a game object.
		m_listModifiedNodes.AddLast( wwiseId );
	}
}

void CAkRegistryMgr::NotifyListenerPosChanged(
	const AkListenerSet& in_uListeners	// Set of listeners whose position changed.
	)
{
	for (CAkEmitter::tList::Iterator it = CAkEmitter::List().Begin(); it != CAkEmitter::List().End(); ++it)
	{
		CAkEmitter* pEmitter = ((CAkEmitter*)*it);
		AKASSERT(pEmitter->GetOwner() != NULL);

		const CAkConnectedListeners* pConnLstnrs = pEmitter->GetOwner()->GetComponentOrDefault<CAkConnectedListeners>();
		AKASSERT(pConnLstnrs != NULL);
		
		//The emitter position is dirty if one of the connected listeners has changed.
		if (!AkDisjoint(in_uListeners, pConnLstnrs->GetListeners()))
		{
			pEmitter->NotifyPositionDirty();
		}
	}
	
	// Update Wwise Transport game object position.
#ifndef AK_OPTIMIZED
	CAkRegisteredObj** ppObj = m_mapRegisteredObj.Exists( AkGameObjectID_Transport );
	if ( ppObj )
	{
		// m_relativePositionTransport vectors assume an unrotated listener. 
		// Construct real position for GO_Transport.
		const CAkListener* pListenerData = (*ppObj)->GetComponent<CAkListener>();
		CAkEmitter* pEmitter = (*ppObj)->GetComponent<CAkEmitter>();
		if (pListenerData != NULL && pEmitter != NULL)
		{
			const AkReal32 * pRotationMatrix = &pListenerData->GetMatrix()[0][0];

			// Position: rotate relative position with listener's orientation, then translate to its origin.
			AkVector rotatedPos;
			AkMath::RotateVector(m_relativePositionTransport.Position(), pRotationMatrix, rotatedPos);
			const AkVector & listenerPos = pListenerData->GetTransform().Position();
			AkVector absolutePos;
			absolutePos.X = listenerPos.X + rotatedPos.X;
			absolutePos.Y = listenerPos.Y + rotatedPos.Y;
			absolutePos.Z = listenerPos.Z + rotatedPos.Z;

			// Orientation: rotate relative orientation with listener's orientation.
			AkVector orientationFront;
			AkMath::RotateVector(m_relativePositionTransport.OrientationFront(), pRotationMatrix, orientationFront);

			AkChannelEmitter emitter;
			static const AkVector top = { AK_DEFAULT_TOP_X, AK_DEFAULT_TOP_Y, AK_DEFAULT_TOP_Z };
			emitter.position.Set(absolutePos, orientationFront, top);
			emitter.uInputChannels = AK_SPEAKER_SETUP_ALL_SPEAKERS;
			pEmitter->SetPosition(&emitter, 1, AK::SoundEngine::MultiPositionType_SingleSource);
			pEmitter->UpdateCachedPositions();
		}
	}
#endif
}

AKRESULT CAkRegistryMgr::GetActiveGameObjects( 
		AK::SoundEngine::Query::AkGameObjectsList& io_GameObjectList	///< returned list of active game objects.
		)
{
	for ( AkMapRegisteredObj::IteratorEx iter = m_mapRegisteredObj.BeginEx(); iter != m_mapRegisteredObj.End(); ++iter )
	{
		if( (*iter).item->IsActive() )
		{
			if( !io_GameObjectList.AddLast( (*iter).key ) )
				return AK_InsufficientMemory;
		}
	}

	return AK_Success;
}

bool CAkRegistryMgr::IsGameObjectActive( 
		AkGameObjectID in_GameObjectId
		)
{
	AkMapRegisteredObj::Iterator iter = m_mapRegisteredObj.FindEx( in_GameObjectId ) ;
	if( iter != m_mapRegisteredObj.End() )
		return (*iter).item->IsActive();

	return false;
}

bool CAkRegistryMgr::IsGameObjectRegistered(
	AkGameObjectID in_GameObjectId
)
{
	return m_mapRegisteredObj.FindEx(in_GameObjectId) != m_mapRegisteredObj.End();
}

void CAkRegistryMgr::UpdateGameObjectPositions()
{
	WWISE_SCOPED_PROFILE_MARKER("CAkRegistryMgr::UpdateGameObjectPositions");

	for (CAkEmitter::tList::Iterator it = CAkEmitter::List().Begin(); it != CAkEmitter::List().End(); ++it)
	{
		CAkEmitter* pEmitter = ((CAkEmitter*)*it);
		if (pEmitter->GetOwner()->IsActive() || pEmitter->GetOwner()->HasComponent<CAkListener>())
			pEmitter->UpdateCachedPositions();
	}
}

void CAkRegistryMgr::EnsureListenerExists(AkGameObjectID in_listenerID)
{
	CAkRegisteredObj** ppLstnrObj = m_mapRegisteredObj.Exists(in_listenerID);
	if (ppLstnrObj)
	{
		(*ppLstnrObj)->CreateComponent<CAkListener>();
	}
	else
	{
		MONITOR_ERROREX(AK::Monitor::ErrorCode_UnknownListener, AK_INVALID_PLAYING_ID, in_listenerID, AK_INVALID_UNIQUE_ID, false);
	}
}

void CAkRegistryMgr::EnsureListenersExist(const AkListenerSet& in_listenerSet)
{
	for (AkListenerSet::Iterator it = in_listenerSet.Begin(); it != in_listenerSet.End(); ++it)
	{
		EnsureListenerExists(*it);
	}
}

#ifndef AK_OPTIMIZED
void CAkRegistryMgr::PostEnvironmentStats()
{
	// Determine the length of monitoring data to allocate
	AkUInt32 uNumGameObj = 0;
	AkUInt32 uNumAssocs = 0;
	for (CAkConnectedListeners::tIter it = CAkConnectedListeners::Begin(); it != CAkConnectedListeners::End(); ++it)
	{
		CAkConnectedListeners* pConn = (CAkConnectedListeners*)(*it);
		if (pConn->GetOwner()
			&& !pConn->GetOwner()->IsRegistered())
			continue;

		if (pConn->OverrideUserDefault()) 
		{
			uNumAssocs += pConn->GetUserAssocs().Length();
		}

		if (pConn->OverrideAuxDefault())
		{
			uNumAssocs += pConn->GetAuxAssocs().Length();
			// make sure there is no invalid objects
			const CAkLstnrAssocs& assocs = pConn->GetAuxAssocs();
			for (CAkLstnrAssocs::Iter it = assocs.Begin(), itEnd = assocs.End(); it != itEnd; ++it)
			{
				for (CAkLstnrAssocs::Iter it = assocs.Begin(), itEnd = assocs.End(); it != itEnd; ++it)
				{
					if ((*it).key.listenerID == AK_INVALID_GAME_OBJECT) {
						uNumAssocs += pConn->GetListeners().Length();
					}
				}
			}

		}


		if (pConn->OverrideUserDefault() || pConn->OverrideAuxDefault())
			uNumGameObj++;
	}

    AkInt32 sizeofItem = SIZEOF_MONITORDATA_TO( environmentData.envPacket )
						+ uNumGameObj * sizeof( AkMonitorData::EnvPacket )
						+ uNumAssocs * sizeof( AkMonitorData::AuxSendValue );

    AkProfileDataCreator creator( sizeofItem );
	if ( !creator.m_pData )
		return;

    creator.m_pData->eDataType = AkMonitorData::MonitorDataEnvironment;

	creator.m_pData->environmentData.ulNumEnvPacket = uNumGameObj;
	creator.m_pData->environmentData.ulTotalAuxSends = uNumAssocs;

	AkMonitorData::EnvPacket* pEnvPacket = &creator.m_pData->environmentData.envPacket[0];

	int uNumEnvPacket = 0;
	for (CAkConnectedListeners::tIter it = CAkConnectedListeners::Begin(); it != CAkConnectedListeners::End(); ++it)
	{
		CAkConnectedListeners* pConn = (CAkConnectedListeners*)(*it);
		if (pConn->GetOwner()
			&& !pConn->GetOwner()->IsRegistered())
			continue;

		if (pConn->OverrideUserDefault() || pConn->OverrideAuxDefault())
		{
			pEnvPacket->gameObjID = (pConn->GetOwner() != NULL) ? pConn->GetOwner()->ID() : AK_INVALID_GAME_OBJECT;

			int uNumAuxSend = 0;
			AkMonitorData::AuxSendValue* pAuxSend = (AkMonitorData::AuxSendValue*)(pEnvPacket + 1);

			if (pConn->OverrideUserDefault())
			{
				const CAkLstnrAssocs& assocs = pConn->GetUserAssocs();
				for (CAkLstnrAssocs::Iter it = assocs.Begin(), itEnd = assocs.End(); it != itEnd; ++it)
				{
					pAuxSend[uNumAuxSend].listenerID = (*it).key.listenerID;
					AkGameObjectID listenerID = (*it).key.listenerID;
					if (listenerID != AK_INVALID_GAME_OBJECT)
					{
						pAuxSend[uNumAuxSend].listenerID = (*it).key.listenerID;
						pAuxSend[uNumAuxSend].auxBusID = (*it).key.busID;
						pAuxSend[uNumAuxSend].fControlValue = (*it).GetGain();
						pAuxSend[uNumAuxSend].associationType = AkMonitorData::AkAuxBusAssociationType_UserDefined;
						uNumAuxSend++;
					}
				}
			}

			if (pConn->OverrideAuxDefault())
			{

				const CAkLstnrAssocs& assocs = pConn->GetAuxAssocs();
				for (CAkLstnrAssocs::Iter it = assocs.Begin(), itEnd = assocs.End(); it != itEnd; ++it)
				{
					AkGameObjectID listenerID = (*it).key.listenerID;
					if (listenerID != AK_INVALID_GAME_OBJECT) 
					{
						pAuxSend[uNumAuxSend].listenerID = listenerID;
						pAuxSend[uNumAuxSend].auxBusID = (*it).key.busID;
						pAuxSend[uNumAuxSend].fControlValue = (*it).GetGain();
						pAuxSend[uNumAuxSend].associationType = AkMonitorData::AkAuxBusAssociationType_GameDefined;
						uNumAuxSend++;
					}
					else {
						const AkListenerSet& listenerSet = pConn->GetListeners();
						for (AkListenerSet::Iterator l = listenerSet.Begin(); l != listenerSet.End(); ++l) {
							AkGameObjectID listenerID = *l;
							if (listenerID != AK_INVALID_GAME_OBJECT)
							{
								pAuxSend[uNumAuxSend].listenerID = listenerID;
								pAuxSend[uNumAuxSend].auxBusID = (*it).key.busID;
								pAuxSend[uNumAuxSend].fControlValue = 1.0;
								pAuxSend[uNumAuxSend].associationType = AkMonitorData::AkAuxBusAssociationType_GameDefined;
								uNumAuxSend++;
							}
						}
					}
				}
			}


			pEnvPacket->uNumEnv = uNumAuxSend;
			pEnvPacket = (AkMonitorData::EnvPacket*)(&pAuxSend[uNumAuxSend]);

			uNumEnvPacket++;
		}
	}
}

void CAkRegistryMgr::PostObsOccStats()
{
	//Count num game objects that have an obs or occ and will require a packet
	AkUInt32 uNumPackets = 0;

	for ( AkMapRegisteredObj::Iterator iter = m_mapRegisteredObj.Begin(); iter != m_mapRegisteredObj.End(); ++iter )
	{
		CAkEmitter* pEmitter = (*iter).item->GetComponent<CAkEmitter>();
		if (pEmitter)
		{
			AkPerLstnrObjDataMap::Iterator dataIt = pEmitter->GetPosition().GetPerListenerData().Begin();
			for (; dataIt != pEmitter->GetPosition().GetPerListenerData().End(); ++dataIt)
			{
				uNumPackets += pEmitter->GetPosition().GetNumPosition();
			}
		}

		CAkSpatialAudioComponent* pSpatialAudioComponent = (*iter).item->GetComponent<CAkSpatialAudioComponent>();
		if (pSpatialAudioComponent)
		{
			AkVolumeDataArray& diffractionRays = pSpatialAudioComponent->GetDiffractionRays();
			uNumPackets += diffractionRays.Length();

			AkVolumeDataArray& roomSendRays = pSpatialAudioComponent->GetRoomSendRays();
			uNumPackets += roomSendRays.Length();
		}
	}

    AkInt32 sizeofItem = SIZEOF_MONITORDATA_TO( obsOccData.obsOccPacket )
						+ uNumPackets * sizeof( AkMonitorData::ObsOccPacket );

    AkProfileDataCreator creator( sizeofItem );
	if ( !creator.m_pData )
		return;

    creator.m_pData->eDataType = AkMonitorData::MonitorDataObsOcc;

	creator.m_pData->obsOccData.ulNumPacket = uNumPackets;

	int iCurrPacket = 0;
	for ( AkMapRegisteredObj::Iterator iter = m_mapRegisteredObj.Begin(); iter != m_mapRegisteredObj.End(); ++iter )
	{
		CAkEmitter* pEmitter = (*iter).item->GetComponent<CAkEmitter>();
		if (pEmitter)
		{
			AkPerLstnrObjDataMap::Iterator dataIt = pEmitter->GetPosition().GetPerListenerData().Begin();
			for (; dataIt != pEmitter->GetPosition().GetPerListenerData().End(); ++dataIt)
			{
				AkUInt32 nbPosition = pEmitter->GetPosition().GetNumPosition();
				AkGameRayParams* rayParams = (AkGameRayParams*)AkAlloca(nbPosition * sizeof(AkGameRayParams));

				pEmitter->GetPosition().GetGameRayParams((*dataIt).key, nbPosition, rayParams);

				for (AkUInt32 i = 0; i < nbPosition; i++)
				{
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].gameObjID = (*iter).key;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].listenerID = (*dataIt).key;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].fObsValue = rayParams[i].obstruction;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].fOccValue = rayParams[i].occlusion;
					const AkTransform & emitter = pEmitter->GetPosition().GetPositions()[i].position;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].fPositionX = emitter.Position().X;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].fPositionY = emitter.Position().Y;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].fPositionZ = emitter.Position().Z;

					++iCurrPacket;
				}
			}
			
			CAkSpatialAudioComponent* pSpatialAudioComponent = (*iter).item->GetComponent<CAkSpatialAudioComponent>();
			if (pSpatialAudioComponent)
			{
				AkVolumeDataArray& diffractionRays = pSpatialAudioComponent->GetDiffractionRays();
				for (auto it = diffractionRays.Begin(); it != diffractionRays.End(); ++it)
				{
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].gameObjID = (*iter).key;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].listenerID = (*it).ListenerID();
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].fObsValue = (*it).Obstruction() / 100.f;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].fOccValue = (*it).Occlusion() / 100.f;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].fPositionX = (*it).emitter.Position().X;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].fPositionY = (*it).emitter.Position().Y;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].fPositionZ = (*it).emitter.Position().Z;

					++iCurrPacket;
				}

				AkVolumeDataArray& roomSendRays = pSpatialAudioComponent->GetRoomSendRays();
				for (auto it = roomSendRays.Begin(); it != roomSendRays.End(); ++it)
				{
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].gameObjID = (*iter).key;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].listenerID = (*it).ListenerID();
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].fObsValue = (*it).Obstruction() / 100.f;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].fOccValue = (*it).Occlusion() / 100.f;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].fPositionX = (*it).emitter.Position().X;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].fPositionY = (*it).emitter.Position().Y;
					creator.m_pData->obsOccData.obsOccPacket[iCurrPacket].fPositionZ = (*it).emitter.Position().Z;

					++iCurrPacket;
				}
			}
		}
	}
}

void CAkRegistryMgr::PostListenerStats()
{
	AkUInt16 uNumListeners = 0;
	AkUInt32 uNumGameObj = 0;
	AkUInt32 uVolumes = 0;
	AkUInt32 uNumListenerFields = 0;

	for (AkMapRegisteredObj::Iterator iter = m_mapRegisteredObj.Begin(); iter != m_mapRegisteredObj.End(); ++iter)
	{
		CAkListener* pListener = (*iter).item->GetComponent<CAkListener>();
		if (pListener != NULL)
		{
			uNumListeners++;
			if (pListener->GetUserDefinedSpeakerGainsDB() != NULL)
				uVolumes += pListener->GetUserDefinedConfig().uNumChannels;
		}

		CAkEmitter* pEmitter = (*iter).item->GetComponent<CAkEmitter>();
		if (pEmitter != NULL)
		{
			uNumGameObj++;
			uNumListenerFields += (*iter).item->GetListeners().Length();
		}
	}
	
	AkInt32 sizeofItem = sizeof(AkMonitorData::ListenerMonitorData)
		+ uNumListeners * sizeof( AkMonitorData::ListenerPacket )
		+ uNumGameObj * sizeof( AkMonitorData::GameObjectListenerMaskPacket )
		+ uVolumes * sizeof(AkReal32)
		+ uNumListenerFields * sizeof(AkWwiseGameObjectID);
	

    AkProfileDataCreator creator( sizeofItem + SIZEOF_MONITORDATA_TO(listenerData) );
	if ( !creator.m_pData )
		return;

	creator.m_pData->eDataType = AkMonitorData::MonitorDataListeners;
	creator.m_pData->listenerData.uSizeTotal = sizeofItem;
	creator.m_pData->listenerData.ulNumGameObjMask = (AkUInt16)uNumGameObj;
	creator.m_pData->listenerData.ulNumListenerIDsForGameObjs = (AkUInt16)uNumListenerFields;
	creator.m_pData->listenerData.uVolumes = (AkUInt16)uVolumes;
	creator.m_pData->listenerData.uNumListeners = uNumListeners;

	AkMonitorData::ListenerPacket* pListenerPacketBase = (AkMonitorData::ListenerPacket*)(&creator.m_pData->listenerData + 1);
	AkReal32 *pVolumes = (AkReal32*)((unsigned char*)(pListenerPacketBase)
								+ (sizeof(AkMonitorData::ListenerPacket)*uNumListeners) 
								+ (sizeof(AkMonitorData::GameObjectListenerMaskPacket)*uNumGameObj) 
								+ uNumListenerFields * sizeof(AkWwiseGameObjectID));
	uVolumes = 0;
	AkUInt32 uListenerIdx = 0;
	for (AkMapRegisteredObj::Iterator iter = m_mapRegisteredObj.Begin(); iter != m_mapRegisteredObj.End(); ++iter)
	{
		CAkListener* pListener = (*iter).item->GetComponent<CAkListener>();
		if (!pListener)
			continue;

		AkGameObjectID listenerID = (*iter).key;
		const CAkListener & rListener = *(pListener);

		AkMonitorData::ListenerPacket &rPacket = pListenerPacketBase[uListenerIdx++];
		rPacket.bSpatialized = rListener.IsSpatialized();
		rPacket.uConfig = rListener.GetUserDefinedConfig().Serialize();
		rPacket.uListenerID = listenerID;

		if (rListener.GetUserDefinedSpeakerGainsDB() != NULL)
		{
			for(AkUInt16 c = 0; c < rListener.GetUserDefinedConfig().uNumChannels; c++)
			{
				pVolumes[uVolumes] = rListener.GetUserDefinedSpeakerGainsDB()[c];
				uVolumes++;
			}
		}
	}

	AkMonitorData::GameObjectListenerMaskPacket* pGameObjPacket = (AkMonitorData::GameObjectListenerMaskPacket*)(pListenerPacketBase + uNumListeners);

	for ( AkMapRegisteredObj::Iterator iter = m_mapRegisteredObj.Begin(); iter != m_mapRegisteredObj.End(); ++iter )
	{
		CAkEmitter* pEmitter = (*iter).item->GetComponent<CAkEmitter>();
		if (pEmitter != NULL)
		{
			pGameObjPacket->gameObject = (*iter).key;

			const AkListenerSet& objLstnrSet = (*iter).item->GetListeners();
			pGameObjPacket->uNumListeners = (AkUInt16)objLstnrSet.Length();

			AkWwiseGameObjectID* pListenerIDsForObj = (AkWwiseGameObjectID*)(pGameObjPacket + 1);
			for (AkListenerSet::Iterator lstnrIt = objLstnrSet.Begin(); lstnrIt != objLstnrSet.End(); ++lstnrIt)
			{
				*pListenerIDsForObj = *lstnrIt;
				pListenerIDsForObj++;
			}

			pGameObjPacket = (AkMonitorData::GameObjectListenerMaskPacket*)pListenerIDsForObj;
		}
	}

	AKASSERT((unsigned char*)pGameObjPacket == (unsigned char*)pVolumes);
}

void CAkRegistryMgr::RecapParamDelta()
{
	for (AkMapRegisteredObj::Iterator emitterIter = m_mapRegisteredObj.Begin(); emitterIter != m_mapRegisteredObj.End(); ++emitterIter)
	{
		CAkEmitter* pEmitter = (*emitterIter).item->GetComponent<CAkEmitter>();
		if (pEmitter == NULL)
			continue;

		CAkConnectedListeners* pConnLstnrs = (*emitterIter).item->GetComponent<CAkConnectedListeners>();
		if (pConnLstnrs == NULL)
			continue;

		AkDeltaMonitor::OpenEmitterListenerBrace(pEmitter->ID(), AkDelta_SetGameObjectOutputBusVolume, AkPropID_OutputBusVolume);
		{
			const AkListenerSet& userListeners = pConnLstnrs->GetUserAssocs().GetListeners();
			for (AkListenerSet::Iterator listenerIter = userListeners.Begin(); listenerIter != userListeners.End(); ++listenerIter)
			{
				AkGameObjectID listenerID = *listenerIter;

				// Ignore neutral values for recap since monitoring never knew about them
				AkDeltaMonitor::LogListenerDelta(listenerID, pConnLstnrs->GetUserGain(listenerID), 1.0f, false /* <-- don't log neutral */);
			}
		}
		AkDeltaMonitor::CloseEmitterListenerBrace();

		CAkSpatialAudioComponent* pSpatialAudioObj = (*emitterIter).item->GetComponent<CAkSpatialAudioComponent>();
		if (pSpatialAudioObj != NULL)
		{
			AkReal32 earlyReflectionsVolAPI = pSpatialAudioObj->GetEarlyReflectionsBusVolumeFromAPI();
			if (earlyReflectionsVolAPI != 1.0f)
			{
				AkDeltaMonitor::LogEarlyReflectionsVolume(pSpatialAudioObj->ID(), earlyReflectionsVolAPI);
			}
		}
	}
}

#endif // AK_OPTIMIZED
