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
// AkSpatialAudioComponent.h
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkSpatialAudioCompts.h"
#include "AkDiffractionPath.h"
#include "AkRegistryMgr.h"
#include "AkLEngine.h"
#include "AkRTPCMgr.h"
#include "AkSpatialAudioComponent.h"
#include "AkMonitor.h"

extern AkSpatialAudioInitSettings g_SpatialAudioSettings;

CAkSpatialAudioEmitter::~CAkSpatialAudioEmitter()
{
	if (m_pListener != NULL)
	{
		m_pListener->RemoveEmitter();
		m_pListener = NULL;
	}

	m_customReflectInstances.Term(this);
	m_gometricReflectInstance.Term(this);

	m_ReflectionPaths.Term();
	m_DiffractionPaths.Term();	
	m_StochasticPaths.Term();

	CAkSpatialAudioComponent* pSACmpt = GetOwner()->GetComponent<CAkSpatialAudioComponent>();
	if (pSACmpt != NULL)
		pSACmpt->ClearVolumeRays();
}

AKRESULT CAkSpatialAudioEmitter::AttachListener(AkGameObjectID in_listenerID)
{
	AKRESULT res = AK_Success;

	if ( m_pListener != NULL )
	{
		if (m_pListener->ID() != in_listenerID || !m_pListener->GetOwner()->IsRegistered() )
		{
			m_pListener->RemoveEmitter();
			m_pListener = NULL;
		}
	}

	if (m_pListener == NULL && in_listenerID != AK_INVALID_GAME_OBJECT)
	{
		res = AK_Fail;
		
		CAkGameObject* pMyLstner = g_pRegistryMgr->GetObjAndAddref(in_listenerID);
		if (pMyLstner)
		{
			CAkListener* pListenerCmpt = pMyLstner->CreateComponent<CAkListener>();
			CAkSpatialAudioListener* pSAListenerCmpt = pMyLstner->CreateComponent<CAkSpatialAudioListener>();
			CAkSpatialAudioComponent* pSACmptCmpt = pMyLstner->CreateComponent<CAkSpatialAudioComponent>();

			if (	pListenerCmpt != NULL &&	// All 3 components must be successfully created.
					pSAListenerCmpt != NULL && 
					pSACmptCmpt != NULL)
			{
				m_pListener = pSAListenerCmpt;
				m_pListener->AddEmitter();
				res = AK_Success;
			}
			else
			{
				// Else, delete just the 2 spatial audio components. The sound engine listener component is not our business here.
				pMyLstner->DeleteComponent<CAkSpatialAudioListener>();
				pMyLstner->DeleteComponent<CAkSpatialAudioComponent>();
			}

			pMyLstner->Release();
		}
	}

	return res;
}

void CAkSpatialAudioEmitter::BuildCustomRaysPerSoundPosition(AkUInt32 in_diffractionFlags)
{
	AkGameObjectID listenerID = GetListener()->ID();

	CAkEmitter* pEmitter = GetOwner()->GetComponent<CAkEmitter>();
	CAkSpatialAudioComponent* pSACmpt = GetOwner()->GetComponent<CAkSpatialAudioComponent>();
	const CAkListener* pListener = g_pRegistryMgr->CreateGameObjComponent<CAkListener>(listenerID);
	if (!pListener || !pSACmpt || !pEmitter)
		return; //!! BAIL

	AkPositionStore& positionStore = pEmitter->GetPosition();

	AkUInt32 uNumSoundPos = positionStore.GetNumPosition();

	AkVolumeDataArray& diffractionRays = pSACmpt->GetDiffractionRays();
	diffractionRays.RemoveAll();

	AkVolumeDataArray& roomSendRays = pSACmpt->GetRoomSendRays();
	roomSendRays.RemoveAll();
	AkUInt32 uNumRoomSendRays = 0;

	// If there are no paths, then this is a transmission-only scenario. BuildTransmissionRays() will add rays.
	if (pSACmpt->HasDiffraction() && m_DiffractionPaths.IsEmpty())
		return;

	AkRoomRvbSend roomA = pSACmpt->GetActiveRoomRvbSend();
	AkRoomRvbSend roomB = pSACmpt->GetTransitionRoomRvbSend();
	if (roomA.IsActive())
		uNumRoomSendRays += uNumSoundPos;
	if (roomB.IsActive())
		uNumRoomSendRays += uNumSoundPos;

	// Reserve space. Bailing out here if allocation fails.
	if (!diffractionRays.Resize(uNumSoundPos) ||
		!roomSendRays.Resize(uNumRoomSendRays))
		return;

	AkReal32 fObstruction = ((in_diffractionFlags & DiffractionFlags_UseObstruction) != 0) ? 
							m_DiffractionPaths.GetCombinedObstructionDiffraction() :
							m_DiffractionPaths.GetMinObstruction();

	AkReal32 fOcclusion = 0.f;

	AkRayVolumeData* pDiffractionRay = diffractionRays.Data();
	AkRayVolumeData* pRoomSendRay = roomSendRays.Data();

	for (AkUInt32 posIdx = 0; posIdx < uNumSoundPos; ++posIdx)
	{
		AkGameRayParams perPositionRayParams;
		positionStore.GetGameRayParams(listenerID, posIdx, perPositionRayParams);

		AkGameRayParams rayParams(perPositionRayParams);

		rayParams.occlusion = AkMax(perPositionRayParams.occlusion, fOcclusion);
		rayParams.obstruction = AkMax(perPositionRayParams.obstruction, fObstruction);

		const AkChannelEmitter& position = positionStore.GetPositions()[posIdx];

		pDiffractionRay->UpdateRay(position.position, rayParams, listenerID, position.uInputChannels);

		pListener->ComputeRayDistanceAndAngles(pEmitter->GetScalingFactor(), *pDiffractionRay);

		if (roomA.IsActive())
		{
			pRoomSendRay->CopyEmitterListenerData(*pDiffractionRay);
			pRoomSendRay->SetListenerID(roomA.room.AsGameObjectID());

			++pRoomSendRay;
		}

		if (roomB.IsActive())
		{
			pRoomSendRay->CopyEmitterListenerData(*pDiffractionRay);
			pRoomSendRay->SetListenerID(roomB.room.AsGameObjectID());

			++pRoomSendRay;
		}

		++pDiffractionRay;
	}

	if ((in_diffractionFlags & DiffractionFlags_UseBuiltInParam) != 0)
	{
		AkReal32 diffractionBuiltInParamVal = m_DiffractionPaths.GetMinDiffraction();
		g_pRTPCMgr->SetBuiltInParamValue(BuiltInParam_Diffraction, AkRTPCKey((CAkRegisteredObj*)GetOwner()), diffractionBuiltInParamVal * 100.f);
	}
}

void CAkSpatialAudioEmitter::BuildCustomRaysPerDiffractionPath(AkUInt32 in_diffractionFlags)
{
	AkGameObjectID listenerID = GetListener()->ID();

	CAkEmitter* pEmitter = GetOwner()->GetComponent<CAkEmitter>();
	CAkSpatialAudioComponent* pSACmpt = GetOwner()->GetComponent<CAkSpatialAudioComponent>();
	const CAkListener* pListener = g_pRegistryMgr->CreateGameObjComponent<CAkListener>(listenerID);
	if (!pListener || !pSACmpt || !pEmitter)
		return; //!! BAIL

	//source position, set through api
	AKASSERT(pEmitter->GetPosition().GetNumPosition() > 0);
	AkChannelEmitter sourcePos = pEmitter->GetPosition().GetPositions()[0];
	AKASSERT(AkMath::IsTransformValid(sourcePos.position));

	AkGameRayParams sourceRayParams;
	pEmitter->GetPosition().GetGameRayParams(listenerID, 0, sourceRayParams);

	AkVolumeDataArray& diffractionRays = pSACmpt->GetDiffractionRays();
	diffractionRays.RemoveAll();

	AkVolumeDataArray& roomSendRays = pSACmpt->GetRoomSendRays();
	roomSendRays.RemoveAll();
	
	AkUInt32 uNumRoomSendRays = 0;
	AkRoomRvbSend roomA = pSACmpt->GetActiveRoomRvbSend();
	AkRoomRvbSend roomB = pSACmpt->GetTransitionRoomRvbSend();
	if (roomA.IsActive())
		uNumRoomSendRays += m_DiffractionPaths.Length();
	if (roomB.IsActive())
		uNumRoomSendRays += m_DiffractionPaths.Length();

	// Reserve room in the arrays. Bail out if allocation fails.
	if (!diffractionRays.Resize(m_DiffractionPaths.Length()) ||
		!roomSendRays.Resize(uNumRoomSendRays))
		return;

	AkReal32 diffractionBuiltInParamVal = 1.0f;

	AkRayVolumeData* pDiffractionRay = diffractionRays.Data();
	AkRayVolumeData* pRoomSendRay = roomSendRays.Data();

	for (CAkDiffractionPaths::Iterator it = m_DiffractionPaths.Begin(); it != m_DiffractionPaths.End(); ++it)
	{
		AkDiffractionPath& path = *it;
		if (path.diffraction < diffractionBuiltInParamVal)
			diffractionBuiltInParamVal = path.diffraction;

		AkGameRayParams rayParams = sourceRayParams; // init params from position index zero, set by client through API.

		AKASSERT(AkMath::IsTransformValid(path.virtualPos));

		rayParams.obstruction = AkMax(path.obstructionValue, sourceRayParams.obstruction);

		if ((in_diffractionFlags & DiffractionFlags_UseObstruction) != 0)
			rayParams.obstruction = AkMax(rayParams.obstruction, path.diffraction);

		pDiffractionRay->UpdateRay(path.virtualPos, rayParams, listenerID, sourcePos.uInputChannels);
		
		pListener->ComputeRayDistanceAndAngles(pEmitter->GetScalingFactor(), *pDiffractionRay);
		
		if (roomA.IsActive())
		{
			pRoomSendRay->CopyEmitterListenerData(*pDiffractionRay);
			pRoomSendRay->SetListenerID(roomA.room.AsGameObjectID());

			++pRoomSendRay;
		}

		if (roomB.IsActive())
		{
			pRoomSendRay->CopyEmitterListenerData(*pDiffractionRay);
			pRoomSendRay->SetListenerID(roomB.room.AsGameObjectID());

			++pRoomSendRay;
		}

		++pDiffractionRay;
	}

	if ((in_diffractionFlags & DiffractionFlags_UseBuiltInParam) != 0)
	{
		g_pRTPCMgr->SetBuiltInParamValue(BuiltInParam_Diffraction, AkRTPCKey((CAkRegisteredObj*)GetOwner()), diffractionBuiltInParamVal * 100.f);
	}
}

void CAkSpatialAudioEmitter::BuildTransmissionRays()
{
	AkGameObjectID listenerID = GetListener()->ID();

	CAkEmitter* pEmitter = GetOwner()->GetComponent<CAkEmitter>();
	CAkSpatialAudioComponent* pSACmpt = GetOwner()->GetComponent<CAkSpatialAudioComponent>();
	const CAkListener* pListener = g_pRegistryMgr->CreateGameObjComponent<CAkListener>(listenerID);
	if (!pListener || !pSACmpt || !pEmitter)
		return; //!! BAIL

	//source position, set through api
	AKASSERT(pEmitter->GetPosition().GetNumPosition() > 0);
	AkChannelEmitter sourcePos = pEmitter->GetPosition().GetPositions()[0];
	AKASSERT(AkMath::IsTransformValid(sourcePos.position));

	AkGameRayParams sourceRayParams;
	pEmitter->GetPosition().GetGameRayParams(listenerID, 0, sourceRayParams);

	AkReal32 distanceScaling = pEmitter->GetScalingFactor();
	AkReal32 minDistance = FLT_MAX;
	const AkRayVolumeData * pShortestRay = NULL;

	AkVolumeDataArray& transmissionRays = pSACmpt->GetTransmissionRays();
	transmissionRays.RemoveAll();

	if (GetDiffractionPaths().HasDirectLineOfSight())
 		return; // Bail out. No transmission path.

	AkVolumeDataArray& roomSendRays = pSACmpt->GetRoomSendRays();

	AkUInt32 uNumTransmissionRoomSendRays = 0;

	AkRoomRvbSend roomA = pSACmpt->GetActiveRoomRvbSend();
	AkRoomRvbSend roomB = pSACmpt->GetTransitionRoomRvbSend();
	if (roomA.IsActive())
		uNumTransmissionRoomSendRays++;
	if (roomB.IsActive())
		uNumTransmissionRoomSendRays++;

	AkUInt32 uNumExistingRoomSendRays = roomSendRays.Length();

	// Reserve room in the arrays. Bail out if allocation fails.
	if (!transmissionRays.Resize(1) ||
		!roomSendRays.Resize(uNumExistingRoomSendRays + uNumTransmissionRoomSendRays))
		return;

	AkRayVolumeData* pTransmissionRay = transmissionRays.Data();
	AkRayVolumeData* pRoomSendRay = roomSendRays.Data() + uNumExistingRoomSendRays;

	AkGameRayParams rayParams(sourceRayParams);

	// Max geometric transmission-occlusion with API-occlusion.
	rayParams.occlusion = AkMax( rayParams.occlusion, GetDiffractionPaths().GetOcclusion() );

	if (pSACmpt->GetActiveRoom() != GetListener()->GetActiveRoom())
	{
		// If emitter and listener are not in the same room, check the wall occlusion values.
		AkReal32 wallOcclusionValue = AkMax(pSACmpt->GetWallOcclusion(), GetListener()->GetSpatialAudioComponent()->GetWallOcclusion());
		rayParams.occlusion = AkMax(rayParams.occlusion, wallOcclusionValue);
	}

	pTransmissionRay->UpdateRay(sourcePos.position, rayParams, listenerID, sourcePos.uInputChannels);

	pListener->ComputeRayDistanceAndAngles(distanceScaling, *pTransmissionRay);

	if (roomA.IsActive())
	{
		pRoomSendRay->CopyEmitterListenerData(*pTransmissionRay);
		pRoomSendRay->SetListenerID(roomA.room.AsGameObjectID());

		++pRoomSendRay;
	}

	if (roomB.IsActive())
	{
		pRoomSendRay->CopyEmitterListenerData(*pTransmissionRay);
		pRoomSendRay->SetListenerID(roomB.room.AsGameObjectID());

		++pRoomSendRay;
	}
}

void CAkSpatialAudioEmitter::BuildCustomRays(AkUInt32 in_diffractionFlags)
{
	CAkEmitter* pEmitter = GetOwner()->GetComponent<CAkEmitter>();
	CAkSpatialAudioComponent* pSpatialAudioComponent = GetSpatialAudioComponent();

	if (pSpatialAudioComponent->HasDiffraction() &&
		(in_diffractionFlags & DiffractionFlags_CalcEmitterVirtualPosition) != 0 &&
		pEmitter->GetPosition().GetNumPosition() == 1
		)
	{
		BuildCustomRaysPerDiffractionPath(in_diffractionFlags);
	}
	else
	{
		BuildCustomRaysPerSoundPosition(in_diffractionFlags);
	}

	if (g_SpatialAudioSettings.bEnableTransmission || 
		m_DiffractionPaths.IsEmpty()) // <- Even with transmission turned off, add transmission rays if no paths available to support legacy transmission mode. 
	{
		BuildTransmissionRays();
	}

	pSpatialAudioComponent->UpdateUserRays(GetListener()->ID());

	pSpatialAudioComponent->UpdateBuiltInParams(GetListener()->ID());

	// Clear the dirty flag so that the sound engine does not uselessly update the rays cached 
	// inside of the emitter component and push a new value to the built in params (overwriting the one pushed above).
	pEmitter->NotifyPositionUpdated();

#ifndef AK_OPTIMIZED
	if (GetOwner())
		AkMonitor::Get()->AddChangedGameObject(GetOwner()->ID());
#endif
}

void CAkSpatialAudioEmitter::SetCustomImageSource(AkUniqueID in_auxBusID, AkUniqueID in_srcId, AkRoomID in_roomID, AkImageSourceSettings& in_iss)
{
	m_customReflectInstances.SetCustomImageSource(in_auxBusID, in_srcId, in_roomID, in_iss);
}

void CAkSpatialAudioEmitter::RemoveCustomImageSource(AkUniqueID in_auxBusID, AkUniqueID in_srcId)
{
	m_customReflectInstances.RemoveCustomImageSource(in_auxBusID, in_srcId);
}

void CAkSpatialAudioEmitter::ClearCustomImageSources(AkUniqueID in_auxBusID)
{
	m_customReflectInstances.ClearCustomImageSources(in_auxBusID);
}

void CAkSpatialAudioEmitter::PackageImageSources()
{
	m_customReflectInstances.PackageReflectors(this);
	m_gometricReflectInstance.PackageReflectors(this);
}

bool CAkSpatialAudioEmitter::HasReflections() const 
{ 
	return g_SpatialAudioSettings.uMaxReflectionOrder > 0 &&
		GetOwner()->GetComponent<CAkSpatialAudioComponent>()->HasReflections(); 
}

bool CAkSpatialAudioEmitter::HasDiffraction() const 
{ 
	return GetOwner()->GetComponent<CAkSpatialAudioComponent>()->HasDiffraction(); 
}

// CAkSpatialAudioListener

CAkSpatialAudioListener::~CAkSpatialAudioListener()
{
	if (m_uEmitterCount > 0)
	{
		for (CAkSpatialAudioEmitter::tList::Iterator it = CAkSpatialAudioEmitter::List().Begin();
			it != CAkSpatialAudioEmitter::List().End(); ++it)
		{
			CAkSpatialAudioEmitter* pEmitter = static_cast<CAkSpatialAudioEmitter*>(*it);
			if (pEmitter->GetListener() == this)
			{
				pEmitter->ClearListener();
				--m_uEmitterCount;
			}
		}
	}

	AKASSERT(m_uEmitterCount == 0);

	m_Rooms.Clear();

	m_stochasticRays.Term();
}
