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

#include "AkSpatialAudioComponent.h"
#include "AkSpatialAudioVoice.h"
#include "AkBehavioralCtx.h"
#include "Ak3DListener.h"
#include "AkRegistryMgr.h"

#include "AkLEngine.h"
#define AK_REFLECT_PLUGIN_ID	171

// CAkSpatialAudioComponent

CAkSpatialAudioComponent::CAkSpatialAudioComponent() :	
	m_EarlyReflectionsBus(AK_INVALID_AUX_ID),
	m_EarlyReflectionsVolume(1.f),
	m_RatioAtoB(0.f),
	m_ReflectionsVoiceCount(0),
	m_DiffractionVoiceCount(0),
	m_MaxDistance(0.f),
	m_uSync(0),
	m_bPositionDirty(true),
	m_bRoomDirty(true)
{
	m_DiffractionRays.Reserve(8);
	m_RoomSendRays.Reserve(2);
	m_TransmissionRays.Reserve(1);
}

CAkSpatialAudioComponent::~CAkSpatialAudioComponent()
{
	AKASSERT(m_List.IsEmpty());
	AKASSERT(m_ReflectionsVoiceCount == 0);
	AKASSERT(m_DiffractionVoiceCount == 0);

	SetEarlyReflectionsAuxSend(AK_INVALID_AUX_ID);
	SetEarlyReflectionsVolume(m_EarlyReflectionsVolume);

	AKASSERT(m_ReflectionsAuxBusses.IsEmpty());

	m_DiffractionRays.Term();
	m_RoomSendRays.Term();
	m_UserRays.Term();
	m_TransmissionRays.Term();

	m_ReflectionsAuxBusses.Term();
	m_ImageSourceAuxBus.Term();
}

void CAkSpatialAudioComponent::TrackVoice(CAkSpatialAudioVoice* in_pVoice)
{
	bool bHadReflectionsOrDiffraction = HasReflections() || HasDiffraction();

	AKASSERT(in_pVoice->pNextItem == NULL);

	bool bReflections = in_pVoice->HasReflections();
	if (bReflections)
		m_ReflectionsVoiceCount++;

	in_pVoice->m_bTrackingReflections = bReflections;

	bool bDiffraction = in_pVoice->HasDiffraction();
	if (bDiffraction)
		m_DiffractionVoiceCount++;

	in_pVoice->m_bTrackingDiffraction = bDiffraction;

	TrackReflectionsAuxBus(in_pVoice);

	AkReal32 fVoiceMaxDistance = in_pVoice->GetMaxDistance();
	if (fVoiceMaxDistance > m_MaxDistance)
	{
		m_MaxDistance = fVoiceMaxDistance;
	}

	m_List.AddFirst(in_pVoice);

	// Dirty position to force recompute of reflection/diffraction paths, since it may now be required.
	m_bPositionDirty = true;
	m_bActivated = true;
}

void CAkSpatialAudioComponent::TrackReflectionsAuxBus(CAkSpatialAudioVoice* in_pVoice)
{
	AkUniqueID reflectionsAuxBus = in_pVoice->GetReflectionsAuxBus();
	if (reflectionsAuxBus != AK_INVALID_AUX_ID)
	{
		AkAuxBusRef* pRef = m_ReflectionsAuxBusses.Set(reflectionsAuxBus);
		if (pRef)
		{
			AKASSERT(pRef->refCount >= 0);
			pRef->refCount++;
		}

		in_pVoice->m_trackingReflectionsAuxBus = reflectionsAuxBus;
	}
}

void CAkSpatialAudioComponent::UntrackVoice(CAkSpatialAudioVoice* in_pVoice)
{
	if (in_pVoice->m_bTrackingReflections)
		m_ReflectionsVoiceCount--;
	in_pVoice->m_bTrackingReflections = false;

	if (in_pVoice->m_bTrackingDiffraction)
		m_DiffractionVoiceCount--;
	in_pVoice->m_bTrackingDiffraction = false;

	UntrackReflectionsAuxBus(in_pVoice);

	AKASSERT(m_ReflectionsVoiceCount >= 0);
	AKASSERT(m_DiffractionVoiceCount >= 0);

	AKVERIFY(m_List.Remove(in_pVoice));

	if (m_List.IsEmpty())
	{
		m_MaxDistance = 0.f;
	}

	//Note: We don't dirty the position here, because even if the reflections turn off, 
	//	we want to make sure the delay lines play out. Dirtying the position would clear the reflections paths and 
	//	remove the image sources from the reflect plugin.
}

void CAkSpatialAudioComponent::UntrackReflectionsAuxBus(CAkSpatialAudioVoice* in_pVoice)
{
	AkUniqueID reflectionsAuxBus = in_pVoice->m_trackingReflectionsAuxBus;
	if (reflectionsAuxBus != AK_INVALID_AUX_ID)
	{
		AkAuxBusRef* pRef = m_ReflectionsAuxBusses.Exists(reflectionsAuxBus);
		if (pRef)
		{
			pRef->refCount--;
			AKASSERT(pRef->refCount >= 0);
			if (pRef->refCount == 0)
			{
				m_ReflectionsAuxBusses.Unset(reflectionsAuxBus);
			}
		}

		in_pVoice->m_trackingReflectionsAuxBus = AK_INVALID_AUX_ID;
	}
}

void CAkSpatialAudioComponent::UpdateTracking(CAkSpatialAudioVoice* in_pVoice)
{
	AKASSERT(m_List.FindEx(in_pVoice) != m_List.End());

	bool bReflections = in_pVoice->HasReflections();
	if (bReflections && !in_pVoice->m_bTrackingReflections)
		m_ReflectionsVoiceCount++;
	else if (!bReflections && in_pVoice->m_bTrackingReflections)
		m_ReflectionsVoiceCount--;

	in_pVoice->m_bTrackingReflections = bReflections;

	bool bDiffraction = in_pVoice->HasDiffraction();
	if (bDiffraction && !in_pVoice->m_bTrackingDiffraction)
		m_DiffractionVoiceCount++;
	else if (!bDiffraction && in_pVoice->m_bTrackingDiffraction)
		m_DiffractionVoiceCount--;

	in_pVoice->m_bTrackingDiffraction = bDiffraction;

	if (in_pVoice->GetReflectionsAuxBus() != in_pVoice->m_trackingReflectionsAuxBus)
	{
		UntrackReflectionsAuxBus(in_pVoice);
		TrackReflectionsAuxBus(in_pVoice);
	}

	m_MaxDistance = AkMax(m_MaxDistance, in_pVoice->GetMaxDistance());

	AKASSERT(m_ReflectionsVoiceCount >= 0);
	AKASSERT(m_DiffractionVoiceCount >= 0);

	//for update of reflections/diffraction calculations.
	m_bPositionDirty = true;
}

bool CAkSpatialAudioComponent::HasReflections() const
{
	return	!m_List.IsEmpty() &&
			m_EarlyReflectionsVolume > 0.f &&
			(m_ReflectionsVoiceCount > 0 || m_EarlyReflectionsBus != AK_INVALID_AUX_ID);
}

bool CAkSpatialAudioComponent::HasDiffraction() const
{
	return m_DiffractionVoiceCount > 0;
}

AkReal32 CAkSpatialAudioComponent::GetMaxDistance() const 
{
	return m_MaxDistance * GetOwner()->GetComponent<CAkEmitter>()->GetScalingFactor(); 
}

void CAkSpatialAudioComponent::GetRoomReverbAuxSends(AkAuxSendArray & io_arAuxSends, AkReal32 in_fVol, AkReal32 in_fLPF, AkReal32 in_fHPF) const
{
	AkDeltaMonitor::StartRoomSendVolumes();

	AkGameObjectID gameObjA = m_RoomA.room.AsGameObjectID();
	if (m_RoomA.IsActive())
	{
		AkAuxSendValueEx* pSend = io_arAuxSends.AddLast();
		if (pSend)
		{
			pSend->auxBusID = m_RoomA.auxBus;
			pSend->fControlValue = m_RoomA.ctrlVal * in_fVol;
			pSend->listenerID = gameObjA;
			pSend->fLPFValue = in_fLPF;
			pSend->fHPFValue = in_fHPF;
			pSend->eAuxType = ConnectionType_GameDefSend; // using game-defined curve for room aux sends
			AkDeltaMonitor::LogRoomSendVolume(pSend->auxBusID, pSend->listenerID, m_RoomA.ctrlVal);
		}
	}

	AkGameObjectID gameObjB = m_RoomB.room.AsGameObjectID();
	if (m_RoomB.IsActive())
	{
		AkAuxSendValueEx* pSend = io_arAuxSends.AddLast();
		if (pSend)
		{
			pSend->auxBusID = m_RoomB.auxBus;
			pSend->fControlValue = m_RoomB.ctrlVal * in_fVol;
			pSend->listenerID = gameObjB;
			pSend->fLPFValue = 0.f;
			pSend->fHPFValue = 0.f;
			pSend->eAuxType = ConnectionType_GameDefSend;// using game-defined curve for room aux sends
			AkDeltaMonitor::LogRoomSendVolume(pSend->auxBusID, pSend->listenerID, m_RoomB.ctrlVal);
		}
	}
}

void CAkSpatialAudioComponent::GetSendsForTransmission(AkRoomRvbSend& in_ListenerRoomSend, AkAuxSendArray & io_arAuxSends, AkReal32 in_fVol, AkReal32 in_fLPF, AkReal32 in_fHPF) const
{
	AkDeltaMonitor::StartRoomSendVolumes();

	if (in_ListenerRoomSend.IsActive())
	{
		AkAuxSendValueEx* pSend = io_arAuxSends.AddLast();
		if (pSend != NULL)
		{
			pSend->listenerID = in_ListenerRoomSend.room.AsGameObjectID();
			pSend->auxBusID = in_ListenerRoomSend.auxBus;
			pSend->fControlValue = in_ListenerRoomSend.ctrlVal * in_fVol;
			pSend->fLPFValue = 0.f;
			pSend->fHPFValue = 0.f;
			pSend->eAuxType = ConnectionType_GameDefSend;// using game-defined curve for room aux sends
			AkDeltaMonitor::LogRoomSendVolume(pSend->auxBusID, pSend->listenerID, m_RoomB.ctrlVal);
		}
	}
}

void CAkSpatialAudioComponent::SetRoomsAndAuxs(const AkRoomRvbSend& in_roomA, const AkRoomRvbSend& in_roomB, AkReal32 in_fRatioAtoB, AkPortalID in_currentPortal)
{
	AkRoomID oldActiveRoom = GetActiveRoom();
	AkPortalID oldPortal = GetPortal();

	m_Portal = in_currentPortal;
	m_RoomA = in_roomA;
	m_RoomB = in_roomB;
	m_RatioAtoB = in_fRatioAtoB;
	
	if (oldActiveRoom != GetActiveRoom())
	{
		m_bPositionDirty = true;
		m_bRoomDirty = true;
	}

	if (oldPortal != GetPortal())
	{
		m_bPositionDirty = true;
	}
	
}

bool CAkSpatialAudioComponent::UpdatePosition(AkReal32 in_fThresh)
{
	AkPositionStore & ps = GetOwner()->GetComponent<CAkEmitter>()->GetPosition();
	if (ps.GetNumPosition() > 0)
	{
		const AkTransform& in_pos = ps.GetPositions()[0].position;

		if ((((Ak3DVector&)in_pos.Position()) -
			(Ak3DVector&)(m_lastUpdatePos)).LengthSquared()
			> in_fThresh*in_fThresh)
		{
			m_lastUpdatePos = in_pos.Position();
			m_bPositionDirty = true;
		}

		return true;
	}

	// No positions - don't try to calculate spatial audio stuff.
	m_bPositionDirty = false;
	return false;
}

void CAkSpatialAudioComponent::SetEarlyReflectionsAuxSend(AkAuxBusID in_aux)
{
	if (m_EarlyReflectionsBus != in_aux)
	{
		// Un-track old bus if valid.
		if (m_EarlyReflectionsBus != AK_INVALID_AUX_ID)
		{
			AkAuxBusRef* pRef = m_ReflectionsAuxBusses.Exists(m_EarlyReflectionsBus);
			if (pRef)
			{
				pRef->refCount--;
				AKASSERT(pRef->refCount >= 0);
				if (pRef->refCount == 0)
				{
					m_ReflectionsAuxBusses.Unset(m_EarlyReflectionsBus);
				}
			}
		}

		// Track new bus.
		if (in_aux != AK_INVALID_AUX_ID)
		{
			AkAuxBusRef* pRef = m_ReflectionsAuxBusses.Set(in_aux);
			if (pRef)
			{
				AKASSERT(pRef->refCount >= 0);
				pRef->refCount++;
			}
		}

		m_EarlyReflectionsBus = in_aux;

		m_bPositionDirty = true;
	}
}

void CAkSpatialAudioComponent::SetEarlyReflectionsVolume(AkReal32 in_vol)
{	
	if (m_EarlyReflectionsVolume != in_vol)
	{
		m_EarlyReflectionsVolume = in_vol;

		m_bPositionDirty = true;

		AkDeltaMonitor::LogEarlyReflectionsVolume(ID(), m_EarlyReflectionsVolume);
	}
}

void CAkSpatialAudioComponent::GetEarlyReflectionsAuxSends(AkAuxSendArray & io_arAuxSends, AkAuxBusID in_reflectionsAux, AkReal32 in_fVol)
{
	AkGameObjectID myGameObjID = ID();

	// Add sends from the SetImageSource API
	for (AkUniqueIDSet::Iterator it = m_ImageSourceAuxBus.Begin(); it != m_ImageSourceAuxBus.End(); ++it)
	{
		AkAuxSendValueEx* pSend = io_arAuxSends.AddLast();
		if (pSend != NULL)
		{
			pSend->listenerID = myGameObjID;
			pSend->auxBusID = (*it);
			pSend->fControlValue = in_fVol;
			pSend->fLPFValue = 0.f;
			pSend->fHPFValue = 0.f;
			pSend->eAuxType = ConnectionType_ReflectionsSend;
		}
	}

	if (m_bEnableReflectionsAuxSend)
	{
		// If the bus specified on the sound is valid, use it, else try and use the bus specified by the API
		AkAuxBusID auxBus = in_reflectionsAux != AK_INVALID_AUX_ID ? in_reflectionsAux : m_EarlyReflectionsBus;
		if (auxBus != AK_INVALID_AUX_ID)
		{
			AkAuxSendValueEx* pSend = io_arAuxSends.AddLast();
			if (pSend)
			{
				pSend->auxBusID = auxBus;
				pSend->fControlValue = m_EarlyReflectionsVolume * in_fVol;
				pSend->listenerID = myGameObjID;
				pSend->fLPFValue = 0.f;
				pSend->fHPFValue = 0.f;
				pSend->eAuxType = ConnectionType_ReflectionsSend;
			}
		}
	}
}

void CAkSpatialAudioComponent::SetImageSourceAuxBus(AkAuxBusID in_auxID)
{
	m_ImageSourceAuxBus.Set(in_auxID);
}

void CAkSpatialAudioComponent::UnsetImageSourceAuxBus(AkAuxBusID in_auxID)
{
	m_ImageSourceAuxBus.Unset(in_auxID);
}

void CAkSpatialAudioComponent::UpdateUserRays(AkGameObjectID in_spatialAudioListener)
{
	m_UserRays.RemoveAll();

	// We don't want any rays from the emitter component that pertain to spatial audio listeners.
	//	These rays are customized by spatial audio (m_DiffractionRays, m_RoomSendRays) and we don't want conflicts.
	AkAutoTermListenerSet ignoredListeners;

	ignoredListeners.Set(in_spatialAudioListener);

	if (m_RoomA.IsActive())
		ignoredListeners.Set(m_RoomA.room.AsGameObjectID());

	if (m_RoomB.IsActive())
		ignoredListeners.Set(m_RoomB.room.AsGameObjectID());

	CAkEmitter* pEmitter = GetOwner()->GetComponent<CAkEmitter>();
	AKASSERT(pEmitter);

	pEmitter->BuildVolumeRays(m_UserRays, ignoredListeners);
}

AkReal32 CAkSpatialAudioComponent::GetRayVolumeData(AkVolumeDataArray& out_arVolumeData, bool in_bEnableDiffraction, bool in_bEnableRooms)
{
	AkReal32 fMinDistance = AK_UPPER_MAX_DISTANCE;

	out_arVolumeData.RemoveAll();
	
	AkUInt32 uNumRays = 0;
	if (in_bEnableDiffraction)
	{
		uNumRays += m_DiffractionRays.Length();
		uNumRays += m_UserRays.Length();
	}

	if (in_bEnableRooms)
	{
		uNumRays += m_RoomSendRays.Length();
	}

	if (in_bEnableDiffraction) // transmission is coupled to the diffraction check-box, until we add another in the authoring tool.
	{
		uNumRays += m_TransmissionRays.Length();
	}

	if (!out_arVolumeData.Resize(uNumRays))
		return fMinDistance;
	
	{
		AkRayVolumeData* pRay = out_arVolumeData.Data();

		if (in_bEnableDiffraction)
		{
			for (AkVolumeDataArray::Iterator it = m_DiffractionRays.Begin(); it != m_DiffractionRays.End(); ++it)
			{
				// Get min distance for priority computation.
				AkReal32 fDistance = (*it).Distance();
				if (fDistance < fMinDistance)
					fMinDistance = fDistance;

				pRay->CopyEmitterListenerData(*it); //Volumes not copied.

				++pRay;
			}

			for (AkVolumeDataArray::Iterator it = m_UserRays.Begin(); it != m_UserRays.End(); ++it)
			{
				// Get min distance for priority computation.
				AkReal32 fDistance = (*it).Distance();
				if (fDistance < fMinDistance)
					fMinDistance = fDistance;

				pRay->CopyEmitterListenerData(*it); //Volumes not copied.

				++pRay;
			}
		}

		if (in_bEnableRooms)
		{
			for (AkVolumeDataArray::Iterator it = m_RoomSendRays.Begin(); it != m_RoomSendRays.End(); ++it)
			{
				// Get min distance for priority computation.
				AkReal32 fDistance = (*it).Distance();
				if (fDistance < fMinDistance)
					fMinDistance = fDistance;

				pRay->CopyEmitterListenerData(*it); //Volumes not copied.

				pRay++;
			}
		}

		if (in_bEnableDiffraction) // transmission is coupled to the diffraction check-box, until we add another in the authoring tool.
		{
			for (AkVolumeDataArray::Iterator it = m_TransmissionRays.Begin(); it != m_TransmissionRays.End(); ++it)
			{
				// Get min distance for priority computation.
	// 			AkReal32 fDistance = (*it).Distance();
	// 			if (fDistance < fMinDistance)
	// 				fMinDistance = fDistance;

				pRay->CopyEmitterListenerData(*it); //Volumes not copied.

				pRay++;
			}
		}

		AKASSERT(out_arVolumeData.Data() + uNumRays == pRay);
	}

	if (!in_bEnableDiffraction)
	{
		AkAutoTermListenerSet emptySet;
		GetOwner()->GetComponent<CAkEmitter>()->BuildVolumeRays(out_arVolumeData, emptySet);
	}

	return fMinDistance;
}

void CAkSpatialAudioComponent::UpdateBuiltInParams(AkGameObjectID in_listenerID)
{
	const CAkListener* pListener = g_pRegistryMgr->CreateGameObjComponent<CAkListener>(in_listenerID);
	if (pListener != nullptr)
	{
		float minDistance = m_MaxDistance;
		AkRayVolumeData* pBuiltInParamRay = nullptr;

		for (AkVolumeDataArray::Iterator it = m_TransmissionRays.Begin(); it != m_TransmissionRays.End(); ++it)
		{
			if ((*it).ListenerID() == in_listenerID)
			{
				float rayDistance = (*it).Distance();
				if (rayDistance < minDistance)
				{
					pBuiltInParamRay = &(*it);
					minDistance = rayDistance;
				}
			}
		}

		if (pBuiltInParamRay == nullptr) // diffraction rays will always be >= transmission rays.
		{
			for (AkVolumeDataArray::Iterator it = m_DiffractionRays.Begin(); it != m_DiffractionRays.End(); ++it)
			{
				if ((*it).ListenerID() == in_listenerID)
				{
					float rayDistance = (*it).Distance();
					if (rayDistance < minDistance)
					{
						pBuiltInParamRay = &(*it);
						minDistance = rayDistance;
					}
				}
			}
		}

		if (pBuiltInParamRay != nullptr)
		{
			GetOwner()->GetComponent<CAkEmitter>()->UpdateBuiltInParamValues(*pBuiltInParamRay, *pListener);
		}
	}
	
}

bool CAkSpatialAudioComponent::IsReflectionsAuxBus(AkUniqueID in_auxId)
{
	// Check it in_auxId is a reflections bus set via the SetImageSource() API.
	for (AkUniqueIDSet::Iterator it = m_ImageSourceAuxBus.Begin(); it != m_ImageSourceAuxBus.End(); ++it)
	{
		if (in_auxId == *it)
			return true;
	}

	// Check it in_auxId is a reflections bus assigned to this object through a sound the authoring tool.
	for (auto it = m_ReflectionsAuxBusses.Begin(); it != m_ReflectionsAuxBusses.End(); ++it)
	{
		if (in_auxId == (*it).key)
			return true;
	}

	return false;
}

void CAkSpatialAudioComponent::ClearVolumeRays()
{
	m_DiffractionRays.RemoveAll();
	m_RoomSendRays.RemoveAll();
	m_UserRays.RemoveAll();
}