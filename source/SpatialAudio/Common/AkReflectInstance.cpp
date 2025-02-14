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
// AkSpatialAudioComponent.h
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkReflectInstance.h"
#include "AkSpatialAudioCompts.h"
#include "AkCustomPluginDataStore.h"

void CAkCustomReflectInstance::SetCustomImageSource(AkImageSourceID in_srcId, AkRoomID in_roomID, AkImageSourceSettings& in_iss)
{
	AkImageSource* pIS = m_customImageSources.Set(in_srcId);
	if (pIS)
	{
		pIS->virtSrc.params = in_iss.params;
		pIS->virtSrc.texture = in_iss.texture;
		pIS->virtSrc.name.Transfer(in_iss.name);
		pIS->uRoomID = in_roomID;
	}
}

void CAkCustomReflectInstance::RemoveCustomImageSource(AkImageSourceID in_srcId)
{
	m_customImageSources.Unset(in_srcId);
}

void CAkCustomReflectInstance::ClearCustomImageSources()
{
	m_customImageSources.Term();
}

void CAkCustomReflectInstance::PackageReflectors(CAkSpatialAudioEmitter* in_pEmitter)
{
	CAkSpatialAudioComponent* pEmitterComponent = in_pEmitter->GetSpatialAudioComponent();

	AkRoomID emitterRoom = in_pEmitter->GetActiveRoom();
	AkRoomID listenerRoom = in_pEmitter->GetListener()->GetActiveRoom();

	// Un-comment to enable culling based on maximum attenuation distance.
	/*
	AkReal32 maxDistSqr = pEmitterComponent->GetMaxDistance();
	maxDistSqr *= maxDistSqr;

	AkReal32 distanceSqr = ((Ak3DVector&)(in_pEmitter->GetPosition()) - (Ak3DVector&)(in_pEmitter->GetListener()->GetPosition())).LengthSquared();
	*/
	AkUInt32 numReflectors = 0;
	if (/*distanceSqr < maxDistSqr && */
		emitterRoom == listenerRoom)
	{
		for (ImageSourcePerID::Iterator it = m_customImageSources.Begin(); it != m_customImageSources.End(); ++it)
		{
			if (it.pItem->uRoomID == emitterRoom && it.pItem->uRoomID == listenerRoom)
				numReflectors++;
		}
	}

	if (ResizeVirtualSources(numReflectors) && numReflectors > 0)
	{
		AkUInt32 idxImageSrc = 0;
		for (ImageSourcePerID::Iterator it = m_customImageSources.Begin(); it != m_customImageSources.End(); ++it)
		{
			if (it.pItem->uRoomID == emitterRoom && it.pItem->uRoomID == listenerRoom)
			{
				UpdateImageSource(idxImageSrc, (*it).uSrcID, (*it).virtSrc);
				idxImageSrc++;
			}
		}
	}

	SendPluginData(in_pEmitter->GetListener()->ID(), in_pEmitter->ID());

	if (IsActive())
	{
		pEmitterComponent->SetImageSourceAuxBus(m_AuxBusID);
	}
	else
	{
		pEmitterComponent->UnsetImageSourceAuxBus(m_AuxBusID);
	}
}

void CAkCustomReflectInstance::Term(CAkSpatialAudioEmitter* in_pEmitter)
{
	AkCustomPluginDataStore::SetPluginCustomGameData(m_AuxBusID, in_pEmitter->ID(), AkPluginTypeEffect, AKCOMPANYID_AUDIOKINETIC, AK_REFLECT_PLUGIN_ID, NULL, 0, false);
	CAkReflectInstance::Term();

	CAkSpatialAudioComponent* pSACmpt = in_pEmitter->GetSpatialAudioComponent();
	// Normally we can assume that pSACmpt is valid, except in this case where Term() may be called from the destructor of CAkSpatialAudioEmitter.
	if (pSACmpt != NULL) 
		pSACmpt->UnsetImageSourceAuxBus(m_AuxBusID);

	m_customImageSources.Term();
}

void CAkGeometricReflectInstance::PackageReflectors(CAkSpatialAudioEmitter* in_pEmitter)
{
	CAkSpatialAudioComponent* pEmitterComponent = in_pEmitter->GetSpatialAudioComponent();

	bool bEnableAuxSend = false;

	ResizeVirtualSources(0);

	CAkReflectionPaths& reflectionPaths = in_pEmitter->GetReflectionPaths();
	reflectionPaths.GetVirtualSources(*this, in_pEmitter->GetListener(), in_pEmitter);

	if (IsActive())
	{
		bEnableAuxSend = true;

		// Collect the aux busses that we need to send data to.
		const AkAuxBusRefs& reflectionAuxs = pEmitterComponent->GetReflectionsAuxBusses();
		for (AkAuxBusRefs::Iterator itAux = reflectionAuxs.Begin(); itAux != reflectionAuxs.End(); ++itAux)
		{
			m_activeAuxs.Set((*itAux).key);
		}

		// Send data to the new buses added to the set, and also to any old buses that we previously send to. 
		// The old buses may be useless now, but since the data is shared, we need to update it (it may have been reallocated).
		SendPluginData(in_pEmitter->GetListener()->ID(), in_pEmitter->ID());			
	}
	else
	{
		Term(in_pEmitter);
	}
	

	pEmitterComponent->EnableReflectionsAuxSend(bEnableAuxSend);
}

void CAkGeometricReflectInstance::Term(CAkSpatialAudioEmitter* in_pEmitter)
{
	// Remove the data from all the aux buses at once here in Term().
	// We can not know when reflect is done playing out the remaining delay lines,
	// and we don't want to cut off the reflections by prematurely yanking the data out.

	AkGameObjectID id = in_pEmitter->ID();

	for (AkUniqueIDSet::Iterator it = m_activeAuxs.Begin(); it != m_activeAuxs.End(); ++it)
	{
		AkCustomPluginDataStore::SetPluginCustomGameData(*it, id, AkPluginTypeEffect, AKCOMPANYID_AUDIOKINETIC, AK_REFLECT_PLUGIN_ID, NULL, 0, false);
	}

	m_activeAuxs.RemoveAll();

	CAkReflectInstance::Term();
}

CAkGeometricReflectInstance::~CAkGeometricReflectInstance()
{
	m_activeAuxs.Term();
}

CAkReflectInstance::~CAkReflectInstance()
{
	Term();
}

AKRESULT CAkReflectInstance::AllocVirtualSources(AkUInt32 in_uMaxSrc)
{
	if (in_uMaxSrc > m_uMaxVirtSrc)
	{
		void * pData = AkAlloc(AkMemID_SpatialAudio, AkReflectGameData::GetSize(in_uMaxSrc));
		if (!pData)
			return AK_InsufficientMemory;
		
		if (m_pData && m_pData->uNumImageSources > 0)
		{
			memcpy(pData, m_pData, AkReflectGameData::GetSize(m_pData->uNumImageSources));
		}
		else
		{
			((AkReflectGameData*)pData)->listenerID = AK_INVALID_GAME_OBJECT;
			((AkReflectGameData*)pData)->uNumImageSources = 0;
		}

		if (m_pData != NULL)
			AkFree(AkMemID_SpatialAudio, m_pData);

		m_pData = (AkReflectGameData*)pData;
		m_uMaxVirtSrc = in_uMaxSrc;
	}

	return AK_Success;
}

void CAkReflectInstance::Term()
{
	if (m_pData)
	{
		AkFree(AkMemID_SpatialAudio, m_pData);
	}

	m_pData = NULL;
	m_uMaxVirtSrc = 0;
}

bool CAkReflectInstance::Reserve(AkUInt32 in_uNumSrcs)
{
	if (m_pData == NULL || in_uNumSrcs > m_uMaxVirtSrc)
	{
		//Grow
		AKRESULT res = AllocVirtualSources(in_uNumSrcs);
		if (res != AK_Success)
			return false;
	}

	return true;
}

bool CAkReflectInstance::ResizeVirtualSources(AkUInt32 in_uNumSrcs)
{
	if (m_pData == NULL || in_uNumSrcs > m_uMaxVirtSrc)
	{
		//Grow
		AKRESULT res = AllocVirtualSources(in_uNumSrcs);
		if (res != AK_Success)
			return false;
	}

	if (m_pData != NULL)
	{
		//Init new elements of the array.
		for (AkUInt32 i = m_pData->uNumImageSources; i < in_uNumSrcs; ++i)
		{
			AkReflectImageSource* pSrc = &(m_pData->arSources[i]);

			*pSrc = AkReflectImageSource();
		}

		m_pData->uNumImageSources = in_uNumSrcs;
	}

	return true;
}

void CAkReflectInstance::PreparePluginData(const AkGameObjectID & in_listener, void*& out_pData, AkUInt32& out_uSize)
{
	AkReflectGameData* pData = m_pData;
	AkUInt32 uSize = 0;
	if (pData != NULL)
	{
		uSize = AkReflectGameData::GetSize(pData->uNumImageSources);
		pData->listenerID = in_listener;
	}

	out_pData = (void*)pData;
	out_uSize = uSize;
}

void CAkCustomReflectInstance::SendPluginData(const AkGameObjectID & in_listener, const AkGameObjectID & in_emitter)
{
	AKASSERT(m_AuxBusID != AK_INVALID_AUX_ID);

	void* pData;
	AkUInt32 uSize;
	PreparePluginData(in_listener, pData, uSize);

	AkCustomPluginDataStore::SetPluginCustomGameData(m_AuxBusID, in_emitter, AkPluginTypeEffect, AKCOMPANYID_AUDIOKINETIC, AK_REFLECT_PLUGIN_ID, pData, uSize, false);
}

void CAkGeometricReflectInstance::SendPluginData(const AkGameObjectID & in_listener, const AkGameObjectID & in_emitter)
{
	void* pData;
	AkUInt32 uSize;
	PreparePluginData(in_listener, pData, uSize);

	for (AkUniqueIDSet::Iterator it = m_activeAuxs.Begin(); it != m_activeAuxs.End(); ++it)
	{
		AkCustomPluginDataStore::SetPluginCustomGameData(*it, in_emitter, AkPluginTypeEffect, AKCOMPANYID_AUDIOKINETIC, AK_REFLECT_PLUGIN_ID, pData, uSize, false);
	}
}

void CAkCustomReflectInstances::Term(CAkSpatialAudioEmitter* in_pEmitter)
{
	for (Iterator it = Begin(); it != End(); ++it)
		(*it).Term(in_pEmitter);

	AkSortedKeyArray<AkUniqueID, CAkCustomReflectInstance, ArrayPoolSpatialAudio, CAkCustomReflectInstance, AkGrowByPolicy_DEFAULT, AkTransferMovePolicy<CAkCustomReflectInstance> >::Term();
}

void CAkCustomReflectInstances::SetCustomImageSource(AkUniqueID in_auxBusID, AkUniqueID in_srcId, AkRoomID in_roomID, AkImageSourceSettings& in_iss)
{
	CAkCustomReflectInstance* pRI = Set(in_auxBusID);
	if (pRI)
	{
		pRI->SetCustomImageSource(in_srcId, in_roomID, in_iss);
	}
}

void CAkCustomReflectInstances::RemoveCustomImageSource(AkUniqueID in_auxBusID, AkUniqueID in_srcId)
{
	CAkCustomReflectInstance* pRI = Exists(in_auxBusID);
	if (pRI)
	{
		pRI->RemoveCustomImageSource(in_srcId);
	}
}

void CAkCustomReflectInstances::ClearCustomImageSources(AkUniqueID in_auxBusID)
{
	if (in_auxBusID == AK_INVALID_AUX_ID)
	{
		for (CAkCustomReflectInstances::Iterator it = Begin(); it != End(); ++it)
		{
			(*it).ClearCustomImageSources();
		}
	}
	else
	{
		CAkCustomReflectInstance* pRI = Exists(in_auxBusID);
		if (pRI)
		{
			pRI->ClearCustomImageSources();
		}
	}
}

void CAkCustomReflectInstances::PackageReflectors(CAkSpatialAudioEmitter* in_pEmitter)
{
	m_bIsActive = false;
	
	for (CAkCustomReflectInstances::Iterator it = Begin(); it != End(); )
	{
		(*it).PackageReflectors(in_pEmitter);

		if (!(*it).HasCustomImageSources())
		{
			(*it).Term(in_pEmitter);
			it = Erase(it);
		}
		else
		{
			m_bIsActive = m_bIsActive || (*it).IsActive();
			++it;
		}
	}
}