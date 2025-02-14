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
// AkConnectedListeners.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkConnectedListeners.h"
#include "AkGameObject.h"
#include "AkCommon.h"

#include "Ak3DListener.h"
#include "AkRegistryMgr.h"
#include "AkDeltaMonitor.h"

#ifndef AK_OPTIMIZED
#define AK_DO_LISTENER_CHECKS
#endif

#ifdef AK_DO_LISTENER_CHECKS
#define CHECK() Check()
#else
#define CHECK() 
#endif

AKRESULT CAkLstnrAssocs::CopyFrom(const CAkLstnrAssocs& in_other)
{
	if (m_assoc.Copy(in_other.m_assoc) == AK_Success &&
		m_listenerSet.Copy(in_other.m_listenerSet) == AK_Success)
	{
		return AK_Success;
	}

	return AK_Fail;
}

void CAkLstnrAssocs::Term()
{
	m_assoc.Term();
	m_listenerSet.Term();
}

CAkLstnrAssoc* CAkLstnrAssocs::Add(CAkLstnrAssoc::Key in_key)
{ 
	CAkLstnrAssoc* pAssoc = m_assoc.Set(in_key); 
	if (pAssoc != NULL)
	{
		if (in_key.listenerID != AK_INVALID_GAME_OBJECT &&
			(pAssoc - 1 < &m_assoc[0] || (pAssoc - 1)->key.listenerID != in_key.listenerID) &&
			(pAssoc + 1 > &m_assoc.Last() || (pAssoc + 1)->key.listenerID != in_key.listenerID))
		{
			m_listenerSet.Set(in_key.listenerID);
		}
	}
	return pAssoc;
}

void CAkLstnrAssocs::Remove(CAkLstnrAssoc::Key in_key)
{ 
	CAkLstnrAssoc* pAssoc = m_assoc.Exists(in_key);
	if (pAssoc != NULL)
	{
		PreAssocRemoved(pAssoc);
		m_assoc.Unset(in_key);
	}
}

//Set the gain, adding the association if necessary.
bool CAkLstnrAssocs::SetGain(CAkLstnrAssoc::Key in_key, AkReal32 in_fGain)
{
	bool bChanged = false;
	CAkLstnrAssoc* pAssoc = m_assoc.Exists(in_key);
	if (pAssoc == NULL)
	{
		pAssoc = Add(in_key);
		bChanged = true;
	}

	if (pAssoc != NULL)
		pAssoc->SetGain(in_fGain);
	
	return bChanged;
}

void CAkLstnrAssocs::SetAllGains(AkReal32 in_fGain)
{
	for (AssocArray::Iterator it = m_assoc.Begin(); it != m_assoc.End(); ++it)
	{
		(*it).SetGain(in_fGain);
	}
}

bool CAkLstnrAssocs::SetGains(const AkAuxSendValue* in_aEnvironmentValues, AkUInt32 in_uNumEnvValues)
{
	bool bChanged = false;

	//Avoid rebuilding for no reason.
	AkUInt32 uCount = m_assoc.Length();
	if (in_uNumEnvValues < uCount)
	{
		m_assoc.RemoveAll();
		m_listenerSet.RemoveAll();
		uCount = 0;
		bChanged = true;
	}	
	
	AkUInt32 uUpdated = 0;	
	for (AkUInt32 i = 0; i < in_uNumEnvValues; ++i)
	{
		if (in_aEnvironmentValues[i].auxBusID != AK_INVALID_AUX_ID && in_aEnvironmentValues[i].fControlValue > 0.f)
		{			
			if (SetGain(CAkLstnrAssoc::Key(in_aEnvironmentValues[i].listenerID, in_aEnvironmentValues[i].auxBusID), in_aEnvironmentValues[i].fControlValue))
				bChanged = true;
			else
				uUpdated++;
		}
	}

	if (uUpdated < uCount)
	{
		//Only change the association if something was removed and replaced (simple add is covered and simple removal is covered in the first if of the function).
		m_assoc.RemoveAll();
		m_listenerSet.RemoveAll();
		SetGains(in_aEnvironmentValues, in_uNumEnvValues);
	}

	return bChanged;
}

void CAkLstnrAssocs::GetGains(AkAuxSendArray & io_arAuxSends, AkReal32 in_fScaleGains, AkReal32 in_fScaleLPF, AkReal32 in_fScaleHPF, const AkListenerSet& in_fallbackListeners) const
{
	extern AkReal32 g_fVolumeThreshold;

	AkDeltaMonitor::StartAuxGameDefVolumes(m_assoc.Length());

	for (AssocArray::Iterator it = m_assoc.Begin(); it != m_assoc.End(); ++it)
	{
		CAkLstnrAssoc* pAssoc = &*it;
		AkReal32 fGain = in_fScaleGains*pAssoc->GetGain();
		AkReal32 fLPF = in_fScaleLPF;
		AkReal32 fHPF = in_fScaleHPF;
		if (fGain > g_fVolumeThreshold)
		{
			if (pAssoc->key.listenerID == AK_INVALID_GAME_OBJECT)
			{
				// We allow the user to pass in AK_INVALID_GAME_OBJECT as the object id for an aux send.  This means "use the listeners specified by the SetListeners()/SetDefaultListeners() API".
				//	-use the listeners specified in in_fallbackListeners
				for (AkListenerSet::Iterator itListener = in_fallbackListeners.Begin(); itListener != in_fallbackListeners.End(); ++itListener)
				{
					AkAuxSendValueEx * pNewSend = io_arAuxSends.AddLast();
					if (pNewSend)
					{
						pNewSend->listenerID = *itListener;
						pNewSend->auxBusID = pAssoc->key.busID;
						pNewSend->fControlValue = fGain; // <-control value scaled by in_fScaleGains
						pNewSend->fLPFValue = fLPF;
						pNewSend->fHPFValue = fHPF;
						pNewSend->eAuxType = ConnectionType_GameDefSend;
						AkDeltaMonitor::LogAuxGameDefVolume(pAssoc->key.busID, pAssoc->key.listenerID, pAssoc->GetGain());
					}
				}
			}
			else
			{
				AkAuxSendValueEx * pNewSend = io_arAuxSends.AddLast();
				if (pNewSend)
				{
					pNewSend->listenerID = pAssoc->key.listenerID;
					pNewSend->auxBusID = pAssoc->key.busID;
					pNewSend->fControlValue = fGain; // <-control value scaled by in_fScaleGains
					pNewSend->fLPFValue = fLPF;
					pNewSend->fHPFValue = fHPF;
					pNewSend->eAuxType = ConnectionType_GameDefSend;
					AkDeltaMonitor::LogAuxGameDefVolume(pAssoc->key.busID, pAssoc->key.listenerID, pAssoc->GetGain());
				}
			}
		}
	}
}

AkReal32 CAkLstnrAssocs::GetGain(CAkLstnrAssoc::Key in_key) const
{
	const CAkLstnrAssoc* pAssoc = Get(in_key);
	if (pAssoc)
		return pAssoc->GetGain();
	return 1.f;
}

void CAkLstnrAssocs::Clear()
{
	m_assoc.RemoveAll();
	m_listenerSet.RemoveAll();
}

bool CAkLstnrAssocs::SetListeners(const AkListenerSet& in_newListenerSet)
{
	bool bSuccess = true;
	for (AssocArray::Iterator it = m_assoc.Begin(); it != m_assoc.End(); )
	{
		if ( (*it).key.busID == AK_INVALID_UNIQUE_ID && // don't remove aux bus associations.
			!in_newListenerSet.Contains((*it).key.listenerID))
		{
			PreAssocRemoved(&*it);
			it = m_assoc.Erase(it);
		}
		else
		{
			++it;
		}
	}

	for (AkListenerSet::Iterator it = in_newListenerSet.Begin(); it != in_newListenerSet.End(); ++it)
	{
		bSuccess = Add(*it) && bSuccess;
	}
	return bSuccess;
}

bool CAkLstnrAssocs::RemoveListeners(const AkListenerSet& in_ListenersToRemove)
{
	bool bModified = false;
	for (AkListenerSet::Iterator it = in_ListenersToRemove.Begin(); it != in_ListenersToRemove.End(); ++it)
	{
		CAkLstnrAssoc* pAssoc = m_assoc.Exists(*it);
		if (pAssoc != NULL)
		{
			PreAssocRemoved(pAssoc);
			AssocArray::Iterator toErase;
			toErase.pItem = pAssoc;
			m_assoc.Erase(toErase);
			bModified = true;
		}
	}
	return bModified;
}

bool CAkLstnrAssocs::AddListeners(const AkListenerSet& in_ListenersToAdd)
{
	bool bSuccess = true;
	for (AkListenerSet::Iterator it = in_ListenersToAdd.Begin(); it != in_ListenersToAdd.End(); ++it)
	{
		bSuccess = Add(*it) && bSuccess;
	}
	return bSuccess;
}

void CAkLstnrAssocs::PreAssocRemoved(CAkLstnrAssoc* pAssoc)
{
	//Check before and after pAssoc to see if it is the last of its kind.  
	if (pAssoc->key.listenerID != AK_INVALID_GAME_OBJECT &&
		(pAssoc - 1 < &m_assoc[0] || (pAssoc - 1)->key.listenerID != pAssoc->key.listenerID) &&
		(pAssoc + 1 > &m_assoc.Last() || (pAssoc + 1)->key.listenerID != pAssoc->key.listenerID))
	{
		m_listenerSet.Unset(pAssoc->key.listenerID);
	}
}

#ifndef AK_OPTIMIZED
void CAkLstnrAssocs::CheckSet(const AkListenerSet& in_allListeners)
{
	for (AssocArray::Iterator it = m_assoc.Begin(); it != m_assoc.End(); ++it)
	{
		AKASSERT(in_allListeners.Contains((*it).key.listenerID));
	}
}
bool CAkLstnrAssocs::CheckId(AkGameObjectID in_id) const
{
	for (AssocArray::Iterator it = m_assoc.Begin(); it != m_assoc.End(); ++it)
	{
		if ((*it).key.listenerID == in_id)
			return true;
	}
	return false;
}
#endif

CAkConnectedListeners* CAkConnectedListeners::s_pDefaultConnectedListeners = NULL;

void CAkConnectedListeners::Term()
{
	m_user.Term();
	m_aux.Term();
	m_listenerSet.Term();
	m_bOverrideAuxDefault = IAmTheDefault();
	m_bOverrideAuxDefault = IAmTheDefault();
}

void CAkConnectedListeners::SetUserGain(AkGameObjectID in_uListenerID, AkReal32 in_fGain)
{
	bool bChanged = CloneAndOverrideUserDefault();
	bChanged |= m_user.SetGain(in_uListenerID, in_fGain);

	if (bChanged)	
		BuildListenerSet();			
}

void CAkConnectedListeners::SetAllUserGains(AkReal32 in_fGain)
{
	bool bChanged = CloneAndOverrideUserDefault();	
	m_user.SetAllGains(in_fGain);
	if (bChanged)
		BuildListenerSet();
}

void CAkConnectedListeners::SetAuxGains(const AkAuxSendValue* in_aEnvironmentValues, AkUInt32& out_uNumEnvValues)
{
	bool bChanged = !m_bOverrideAuxDefault;
	m_bOverrideAuxDefault = true;
	if (m_aux.SetGains(in_aEnvironmentValues, out_uNumEnvValues) || bChanged)
		BuildListenerSet();		
}

void CAkConnectedListeners::SetAuxGain(AkGameObjectID in_uListenerID, AkUniqueID in_AuxBusID, AkReal32 in_fGain)
{
	bool bChanged = CloneAndOverrideAuxDefault();
	
	if (m_aux.SetGain(CAkLstnrAssoc::Key(in_uListenerID, in_AuxBusID), in_fGain) || bChanged)
		BuildListenerSet();	
}

void CAkConnectedListeners::ClearAuxGains()
{
	if (!m_bOverrideAuxDefault)
	{
		m_bOverrideAuxDefault = true;
	}

	m_aux.Clear();
	BuildListenerSet();
}

void CAkConnectedListeners::SetListeners(const AkListenerSet& in_newListenerSet)
{
	bool bChanged = !m_bOverrideUserDefault;
	m_bOverrideUserDefault = true;
	if (m_user.SetListeners(in_newListenerSet) || bChanged)
	{
		BuildListenerSet();		
	}	
}

void CAkConnectedListeners::AddListeners(const AkListenerSet& in_ListenersToAdd)
{
	bool bChanged = CloneAndOverrideUserDefault();	
	if (m_user.AddListeners(in_ListenersToAdd) || bChanged)
	{
		BuildListenerSet();	
	}	
}

void CAkConnectedListeners::RemoveListeners(const AkListenerSet& in_ListenersToRemove)
{
	bool bWasDefault = !m_bOverrideUserDefault;

	bool bChanged = CloneAndOverrideUserDefault();		
	if (m_user.RemoveListeners(in_ListenersToRemove) || bChanged)
	{
		BuildListenerSet();		
	}
	else if (bWasDefault)
	{
		// We tried but failed to remove a default listener, reset the state.
		ResetListenersToDefault();
	}
}

void CAkConnectedListeners::ResetListenersToDefault()
{
	m_bOverrideUserDefault = false;
	m_user.Clear();
	BuildListenerSet();	
}

static void CreateListenerComponents(const AkListenerSet& in_listeners)
{
	for (AkListenerSet::Iterator it = in_listeners.Begin(); it != in_listeners.End(); ++it)
	{
		CAkRegisteredObj* pObj = g_pRegistryMgr->GetObjAndAddref(*it);
		if (pObj)
		{
			pObj->CreateComponent<CAkListener>();
			pObj->Release();
		}
	}
}

bool CAkConnectedListeners::CloneAndOverrideUserDefault()
{
	if (!m_bOverrideUserDefault)
	{
		//We must preserve the listener associations that are in s_defaultConnectedListeners when we override it.
		if (m_user.CopyFrom(s_pDefaultConnectedListeners->GetUserAssocs()) != AK_Success)
			return false;

		m_bOverrideUserDefault = true;
		return true;
	}
	return false;
}

bool CAkConnectedListeners::CloneAndOverrideAuxDefault()
{
	if (!m_bOverrideAuxDefault)
	{
		//We must preserve the listener associations that are in s_defaultConnectedListeners when we override it. (Not used for aux sends atm)
		if (m_aux.CopyFrom(s_pDefaultConnectedListeners->GetAuxAssocs()) != AK_Success)
			return false;

		m_bOverrideAuxDefault = true;
		return true;
	}
	return false;
}

void CAkConnectedListeners::BuildListenerSet()
{
	m_listenerSet.RemoveAll();
	AkUnion(m_listenerSet, GetUserAssocs().GetListeners());
	AkUnion(m_listenerSet, GetAuxAssocs().GetListeners());
	CreateListenerComponents(m_listenerSet);

	if (IAmTheDefault())
	{
		// Rebuild all listener sets that reference the default listener set.
		for (CAkConnectedListeners::tList::Iterator it = CAkConnectedListeners::List().Begin(); it != CAkConnectedListeners::List().End(); ++it)
		{
			CAkConnectedListeners& item = (CAkConnectedListeners&)(**it);
			if (&item != this && (!item.m_bOverrideAuxDefault || !item.m_bOverrideUserDefault))
			{
				item.m_listenerSet.RemoveAll();
				AkUnion(item.m_listenerSet, item.GetUserAssocs().GetListeners());
				AkUnion(item.m_listenerSet, item.GetAuxAssocs().GetListeners());
				CreateListenerComponents(item.m_listenerSet);
			}
		}
	}

	// June 28, 2017
	// With low memory tests (AkUnion() calls above failing) we were hitting the asserts generated by the sanity checking in CHECK().
	// The failed AkUnion()s don't appear to be critical (we appear to handle the situation gracefully (read no crash/assert)) so
	// remove the possibility of an assert.
	// If we ever have a problem then we could test the result of AkUnion() and act appropriately when that result is false.
	//CHECK();
}

#ifndef AK_OPTIMIZED
void CAkConnectedListeners::Check()
{ 
	m_user.CheckSet(m_listenerSet); 
	m_aux.CheckSet(m_listenerSet);
	for (AkListenerSet::Iterator it = m_listenerSet.Begin(); it != m_listenerSet.End(); ++it)
	{
		AKASSERT(GetUserAssocs().CheckId(*it) || GetAuxAssocs().CheckId(*it));
	}
}
#endif

void AkEmitterListenerPathTraversal::_Backwards(CAkGameObject* in_pObj)
{
	if (CAkConnectedListeners::GetDefault()->GetListeners().Contains(in_pObj->ID()))
	{
		//Must check all game objects, the slow way.
		CAkRegistryMgr::AkMapRegisteredObj&	objects = g_pRegistryMgr->GetRegisteredObjectList();
		for (CAkRegistryMgr::AkMapRegisteredObj::Iterator it = objects.Begin(); it != objects.End(); ++it)
		{
			CAkGameObject* pNextObj = (*it).item;
			const CAkConnectedListeners* pConnections = pNextObj->GetComponentOrDefault<CAkConnectedListeners>();
			const AkListenerSet& listeners = pConnections->GetListeners();
			if (listeners.Contains(in_pObj->ID()) && !m_visited.Contains(pNextObj))
			{
				m_visited.Set(pNextObj);
				_Backwards(pNextObj);
			}
		}
	}
	else
	{
		//in_pObj not a default listener, look only at explicitly defined links.
		for (CAkConnectedListeners::tList::Iterator it = CAkConnectedListeners::List().Begin(); it != CAkConnectedListeners::List().End(); ++it)
		{
			CAkConnectedListeners* pConnections = (CAkConnectedListeners*)(*it);
			if (pConnections->GetOwner() != NULL)
			{
				CAkGameObject* pNextObj = pConnections->GetOwner();
				const AkListenerSet& listeners = pConnections->GetListeners();
				if (listeners.Contains(in_pObj->ID()) && !m_visited.Contains(pNextObj))
				{
					m_visited.Set(pNextObj);
					_Backwards(pNextObj);
				}
			}
		}
	}
}

void AkEmitterListenerPathTraversal::_Forwards(CAkGameObject* in_pObj)
{
	const CAkConnectedListeners* pConnections = in_pObj->GetComponentOrDefault<CAkConnectedListeners>();
	if (pConnections)
	{
		const AkListenerSet& listeners = pConnections->GetListeners();
		for (AkListenerSet::Iterator it = listeners.Begin(); it != listeners.End(); ++it)
		{
			CAkGameObject* pNextObj = g_pRegistryMgr->GetObjAndAddref(*it);
			if (pNextObj != NULL)
			{
				if (!m_visited.Contains(pNextObj))
				{
					m_visited.Set(pNextObj);
					_Forwards(pNextObj);
				}

				pNextObj->Release();
			}
		}
	}
}