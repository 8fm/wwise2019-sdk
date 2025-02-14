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
// AkSoundGeometry.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <AK/SoundEngine/Common/AkBytesMem.h>

#include "AkSoundGeometry.h"
#include "AkDiffractionEdge.h"
#include "AkMonitor.h"
#include "AkSpatialAudioTasks.h"

AkForceInline AkHashType AkHash(Ak3DVector& in_key) 
{ 
	AK::FNVHash32 hash;
	hash.Compute(in_key.X);
	hash.Compute(in_key.Y);
	hash.Compute(in_key.Z);
	return hash.Get();
}

AKRESULT AkSoundGeometry::SetGeometry(AkGeometrySetID in_id, AkGeometryParams& in_params)
{
	AKRESULT res = AK_Success;

	AkGeometrySet* pGeomSet = m_GeometrySetMap.Exists(in_id);

	if (pGeomSet != NULL)
	{
		AkScene* pScene = pGeomSet->pScene;

		pGeomSet->Unindex(m_ReflectorPool);
		pGeomSet->Term();

		if (pScene->IsUnused())
			DestroyScene(pScene);

	}

	if (pGeomSet == NULL)
	{
		pGeomSet = m_GeometrySetPool.New(in_id);

		if (pGeomSet)
		{
			if (!m_GeometrySetMap.Set(pGeomSet))
			{
				m_GeometrySetPool.Delete(pGeomSet);
				pGeomSet = NULL;
			}
		}
	}

	if (pGeomSet != NULL)
	{
		res = pGeomSet->SetParams(in_params);

		if (res == AK_Success)
		{
			AkScene* pScene = NULL;
			if (in_params.RoomID.IsValid())
			{
				AkAcousticRoom* pRoom = GetOrCreateRoom(in_params.RoomID);
				if (pRoom)
				{
					if (pRoom->GetScene() == NULL || pRoom->GetScene() == m_pGlobalScene)
						pRoom->SetScene(CreateScene());

					pScene = pRoom->GetScene();
				}
			}
			else 
			{
				pScene = m_pGlobalScene;
			}

			if (pScene)
				res = pGeomSet->Index(*pScene, m_ReflectorPool);
			else
				res = AK_InsufficientMemory;
		}

		if (res != AK_Success)
		{
			pGeomSet->Unindex(m_ReflectorPool);
			m_GeometrySetMap.Unset(pGeomSet->groupID);
			m_GeometrySetPool.Delete(pGeomSet);
		}
	}
	else
	{
		res = AK_Fail;
	}

	m_bUpdateVis = true;

	TermAkGeometryParams(in_params);

	return res;
}

AKRESULT AkSoundGeometry::RemoveGeometry(AkGeometrySetID in_GroupID)
{
	AKRESULT res = AK_IDNotFound;

	AkGeometrySetMap::IteratorEx it = m_GeometrySetMap.FindEx(in_GroupID);
	if (it != m_GeometrySetMap.End())
	{
		AkGeometrySet* pToDelete = *it;
		AkScene* pScene = pToDelete->pScene;

		pToDelete->Unindex(m_ReflectorPool);
		m_GeometrySetMap.Erase(it);

		m_GeometrySetPool.Delete(pToDelete);

		if (pScene->IsUnused())
			DestroyScene(pScene);

		res = AK_Success;
	}

	m_bUpdateVis = true;
	return res;
}

AKRESULT AkSoundGeometry::SetRoom(AkRoomID in_roomID, const AkRoomParams& in_Params, AkGameObjectID in_listenerID )
{
	AKRESULT res = AK_Success;

	AkAcousticRoom* pBox = m_RoomMap.Exists(in_roomID);

	if (pBox == NULL)
	{
		pBox = m_RoomPool.New(in_roomID);
		if (pBox)
		{
			if (!m_RoomMap.Set(pBox))
			{
				m_RoomPool.Delete(pBox);
				pBox = NULL;
			}
		}
	}

	if (pBox != NULL)
	{
		pBox->SetParams(in_Params, in_listenerID);
		if (pBox->GetScene() == NULL)
			pBox->SetScene(m_pGlobalScene);
	}
	else
	{
		res = AK_InsufficientMemory;
	}

	return res;
}

AKRESULT AkSoundGeometry::LinkPortalToRoom(AkAcousticPortal* pPortal, AkRoomID roomID)
{
	AkAcousticRoom* pRoom = GetOrCreateRoom(roomID);
	if (pRoom)
	{
		pPortal->Link(pRoom);

		return AK_Success;
	}
	else
	{
		return AK_InsufficientMemory;
	}
}

AKRESULT AkSoundGeometry::SetPortal(AkPortalID in_portalID, const AkPortalParams& in_Params)
{
	AKRESULT res = AK_Success;

	AkAcousticPortal* pPortal = m_PortalMap.Exists(in_portalID);
	if (pPortal == NULL)
	{
		pPortal = m_PortalPool.New(in_portalID);
		if (pPortal)
		{
			if (!m_PortalMap.Set(pPortal))
			{
				m_PortalPool.Delete(pPortal);
				pPortal = NULL;
			}
		}
	}

	if (pPortal != NULL)
	{
		pPortal->SetParams(in_Params);

		pPortal->ClearLinks();

		if (LinkPortalToRoom(pPortal, in_Params.FrontRoom) != AK_Success || 
			LinkPortalToRoom(pPortal, in_Params.BackRoom) != AK_Success)
		{
			res = AK_InsufficientMemory;
		}

	}
	else
	{
		res = AK_InsufficientMemory;
	}

	if (res != AK_Success && pPortal != NULL)
	{
		m_PortalMap.Unset(in_portalID);
		m_PortalPool.Delete(pPortal);
		pPortal = NULL;
	}

	m_bUpdateVis = true;

	return res;
}

AkSoundGeometry::AkSoundGeometry(): m_pGlobalScene(NULL), m_uMaxReflectionsOrder(0), m_bUpdateVis(false)
{}

AkSoundGeometry::~AkSoundGeometry()
{
	Term();
}

AKRESULT AkSoundGeometry::Init()
{
	AKASSERT(m_pGlobalScene == NULL);
	m_pGlobalScene = CreateScene();
	m_pGlobalScene->AddRef();
	return m_pGlobalScene != NULL ? AK_Success : AK_InsufficientMemory;
}

void AkSoundGeometry::Term()
{
	DeleteAllRoomsAndPortals();

	m_RoomMap.Term();
	m_PortalMap.Term();

	for (AkGeometrySetMap::IteratorEx it = m_GeometrySetMap.BeginEx(); it != m_GeometrySetMap.End(); )
	{
		AkGeometrySet* pGeomSet = *it;
		pGeomSet->Unindex(m_ReflectorPool);
		it = m_GeometrySetMap.Erase(it);
		m_GeometrySetPool.Delete(pGeomSet);
	}

	m_pGlobalScene->Release();
	m_pGlobalScene = NULL;

	while (!m_Scenes.IsEmpty())
	{
		AKASSERT(m_Scenes.First()->IsUnused());
		DestroyScene(m_Scenes.First());
	}

	m_GeometrySetMap.Term();

}

AkAcousticRoom* AkSoundGeometry::GetOrCreateRoom(AkRoomID in_RoomID)
{
	AkAcousticRoom* pRoom = m_RoomMap.Exists(in_RoomID);
	if (pRoom == NULL)
	{
		//Room does not (yet) exist. Create an 'empty' room assuming that it will be filled out later.
		pRoom = m_RoomPool.New(in_RoomID);
		if (pRoom != NULL)
		{
			if (!m_RoomMap.Set(pRoom))
			{
				m_RoomPool.Delete(pRoom);
				pRoom = NULL;
			}
		}

		if (pRoom != NULL)
		{

			AkRoomParams roomParams;
			roomParams.Up = Ak3DVector(0.f, 1.f, 0.f);
			roomParams.Front = Ak3DVector(0.f, 0.f, 1.f);
			if (!in_RoomID.IsValid())
			{
				roomParams.WallOcclusion = 0.f;
				roomParams.strName = "Outdoors";
			}
			pRoom->SetParams(roomParams, AK_INVALID_GAME_OBJECT/*unused*/);
			if (pRoom->GetScene() == NULL)
				pRoom->SetScene(m_pGlobalScene);
		}
	}

	return pRoom;
}

AkAcousticRoom* AkSoundGeometry::GetRoom(AkRoomID in_SourceRoomID)
{
	return m_RoomMap.Exists(in_SourceRoomID);
}

AkAcousticPortal* AkSoundGeometry::GetPortal(AkPortalID in_PortalID)
{
	return m_PortalMap.Exists(in_PortalID);
}

const AkAcousticRoom* AkSoundGeometry::GetRoom(AkRoomID in_SourceRoomID) const
{
	return m_RoomMap.Exists(in_SourceRoomID);
}

const AkAcousticPortal* AkSoundGeometry::GetPortal(AkPortalID in_PortalID) const
{
	return m_PortalMap.Exists(in_PortalID);
}

void AkSoundGeometry::DeleteAllRoomsAndPortals()
{
	for (AkRoomMap::IteratorEx it = m_RoomMap.BeginEx(); it != m_RoomMap.End(); )
	{
		AkAcousticRoom* pRoom = *it;
		AkScene* pScene = pRoom->GetScene();

		it = m_RoomMap.Erase(it);
		m_RoomPool.Delete(pRoom);

		if (pScene != NULL && pScene->IsUnused())
			DestroyScene(pScene);
	}

	for (AkPortalMap::IteratorEx it = m_PortalMap.BeginEx(); it != m_PortalMap.End(); )
	{
		AkAcousticPortal* pPortal = *it;
		it = m_PortalMap.Erase(it);
		m_PortalPool.Delete(pPortal);
	}

	m_bUpdateVis = true;
}

AKRESULT AkSoundGeometry::DeletePortal(AkPortalID in_PortalID)
{
	AkPortalMap::IteratorEx it = m_PortalMap.FindEx(in_PortalID);
	if (it != m_PortalMap.End())
	{
		AkAcousticPortal* pPortal = *it;
		m_PortalMap.Erase(it);
		m_PortalPool.Delete(pPortal);
		m_bUpdateVis = true;
		return AK_Success;
	}
	else
	{
		return AK_IDNotFound;
	}
}

AKRESULT AkSoundGeometry::DeleteRoom(AkRoomID in_RoomID)
{
	AkRoomMap::IteratorEx it = m_RoomMap.FindEx(in_RoomID);
	if (it != m_RoomMap.End())
	{
		AkAcousticRoom* pRoom = *it;
		AkScene* pScene = pRoom->GetScene();
		
		m_RoomMap.Erase(it);
		m_RoomPool.Delete(pRoom);
		
		if (pScene != NULL && pScene->IsUnused())
			DestroyScene(pScene);

		return AK_Success;
	}
	else
	{
		return AK_IDNotFound;
	}
}

void AkSoundGeometry::UpdateRooms(CAkSpatialAudioListener* in_pListener, AkUInt32 in_diffractionFlags)
{
	for (AkRoomMap::IteratorEx it = m_RoomMap.BeginEx(); it != m_RoomMap.End(); ++it)
	{
		AkAcousticRoom* pRoom = *it;
		pRoom->Update(in_pListener, in_diffractionFlags);
	}
}

void AkSoundGeometry::ClearSoundPropagationPaths()
{
	for (AkPortalMap::IteratorEx it = m_PortalMap.BeginEx(); it != m_PortalMap.End(); ++it)
	{
		AkAcousticPortal* pPortal = *it;
		pPortal->ClearPaths();
	}
}


AkScene* AkSoundGeometry::CreateScene()
{
	AkScene* pScene = AkNew(AkMemID_SpatialAudio, AkScene());
	if (pScene != NULL)
	{
		if (pScene->Init() == AK_Success)
		{
			m_Scenes.AddFirst(pScene);
		}
		else
		{
			AkDelete(AkMemID_SpatialAudio, pScene);
			pScene = NULL;
		}
	}
	return pScene;
}

void AkSoundGeometry::DestroyScene(AkScene* in_pScene)
{
	AKASSERT(in_pScene->IsUnused());

	in_pScene->Term();
	m_Scenes.Remove(in_pScene);
	AkDelete(AkMemID_SpatialAudio, in_pScene);
}

void AkSoundGeometry::_BuildVisibilityData(AkTaskSchedulerDesc& taskScheduler, AkSpatialAudioInitSettings* in_settings)
{
	for (AkSceneList::Iterator it = m_Scenes.Begin(); it != m_Scenes.End(); ++it)
	{
		if ((*it)->IsDirty())
		{
			(*it)->ClearVisibleEdges();
		}
	}

	for (AkPortalMap::Iterator it = m_PortalMap.BeginEx(); it != m_PortalMap.End(); ++it)
	{
		AkAcousticPortal* pPortal = *it;
		if (pPortal->IsDirty())
		{
			pPortal->ClearGeometry();

			if (pPortal->IsEnabled())
			{
				AkAcousticRoom *pRoomFr, *pRoomBk;
				pPortal->GetRooms(pRoomFr, pRoomBk);

				AkScene* pSceneFr = pRoomFr != NULL ? pRoomFr->GetScene() : NULL;
				AkScene* pSceneBk = pRoomBk != NULL ? pRoomBk->GetScene() : NULL;

				if (pSceneFr != NULL)
				{
					pSceneFr->ConnectPortal(pPortal, true, pSceneBk == pSceneFr);
				}

				// Connect the back room separately if it is connected to a unique scene.
				if (pSceneBk != NULL && pSceneBk != pSceneFr)
				{
					pSceneBk->ConnectPortal(pPortal, false, true);
				}
			}
		}
	}

	// Clear dirty flags
	for (AkRoomMap::Iterator it = m_RoomMap.BeginEx(); it != m_RoomMap.End(); ++it)
		(*it)->SetDirty(false);

	for (AkPortalMap::Iterator it = m_PortalMap.BeginEx(); it != m_PortalMap.End(); ++it)
		(*it)->SetDirty(false);

	for (AkSceneList::Iterator it = m_Scenes.Begin(); it != m_Scenes.End(); ++it)
		(*it)->SetDirty(false);

	m_bUpdateVis = false;
}


void AkSoundGeometry::MonitorData()
{
#ifndef AK_OPTIMIZED
	if ((AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataSpatialAudioRoomsAndPortals)))
	{
		AkVarLenDataCreator creator(AkMonitorData::MonitorDataSpatialAudioRoomsAndPortals);
		AK::MonitorSerializer& serializer = creator.GetSerializer();
		
		bool bRc = serializer.Put((AkUInt32)m_RoomMap.Length());
		for (AkRoomMap::IteratorEx it = m_RoomMap.BeginEx(); it != m_RoomMap.End(); ++it)
		{
			AkAcousticRoom* pRoom = *it;

			bRc = bRc && serializer.Put(pRoom->GetID().id);
			bRc = bRc && serializer.Put(pRoom->GetReverbAuxBus());
			bRc = bRc && serializer.Put(pRoom->GetReverbLevel());
			bRc = bRc && serializer.Put(pRoom->GetWallOcclusion());
			bRc = bRc && serializer.Put(pRoom->GetFront().X);
			bRc = bRc && serializer.Put(pRoom->GetFront().Y);
			bRc = bRc && serializer.Put(pRoom->GetFront().Z);
			bRc = bRc && serializer.Put(pRoom->GetUp().X);
			bRc = bRc && serializer.Put(pRoom->GetUp().Y);
			bRc = bRc && serializer.Put(pRoom->GetUp().Z);

			AK::SpatialAudio::String nameStr = pRoom->GetName();
			bRc = bRc && serializer.Put(nameStr.Get());
		}

		bRc = bRc && serializer.Put(m_PortalMap.Length());
		for (AkPortalMap::IteratorEx it = m_PortalMap.BeginEx(); it != m_PortalMap.End(); ++it)
		{
			AkAcousticPortal* pPortal = *it;

			bRc = bRc && serializer.Put(pPortal->GetID().id);
				
			bRc = bRc && serializer.Put(pPortal->GetCenter().X);
			bRc = bRc && serializer.Put(pPortal->GetCenter().Y);
			bRc = bRc && serializer.Put(pPortal->GetCenter().Z);

			bRc = bRc && serializer.Put(pPortal->GetFront().X);
			bRc = bRc && serializer.Put(pPortal->GetFront().Y);
			bRc = bRc && serializer.Put(pPortal->GetFront().Z);

			bRc = bRc && serializer.Put(pPortal->GetUp().X);
			bRc = bRc && serializer.Put(pPortal->GetUp().Y);
			bRc = bRc && serializer.Put(pPortal->GetUp().Z);

			bRc = bRc && serializer.Put(pPortal->GetExtent().X);
			bRc = bRc && serializer.Put(pPortal->GetExtent().Y);
			bRc = bRc && serializer.Put(pPortal->GetExtent().Z);

			bRc = bRc && serializer.Put(pPortal->GetGain());

			bRc = bRc && serializer.Put(pPortal->GetObstruction());

			bRc = bRc && serializer.Put((AkUInt8)pPortal->IsEnabled());

			bRc = bRc && serializer.Put(pPortal->GetWetDiffraction());

			AK::SpatialAudio::String nameStr = pPortal->GetName();
			bRc = bRc && serializer.Put(nameStr.Get());

			for (AkUInt32 frontOrBack = 0; frontOrBack < AkAcousticPortal::MaxLinkedRooms; ++frontOrBack)
			{
				AkPortalGeometry& geom = pPortal->GetGeometry(frontOrBack);

				if (geom.m_Scene != NULL)
				{
					bRc = bRc && serializer.Put(true);

					for (AkUInt32 e = 0; e < 4; ++e)
					{
						CAkDiffractionEdge& edge = geom.m_Edges[e];
						
						bRc = bRc && serializer.Put(edge.start.X);
						bRc = bRc && serializer.Put(edge.start.Y);
						bRc = bRc && serializer.Put(edge.start.Z);

						bRc = bRc && serializer.Put(edge.direction.X);
						bRc = bRc && serializer.Put(edge.direction.Y);
						bRc = bRc && serializer.Put(edge.direction.Z);

						bRc = bRc && serializer.Put(edge.length);
					}
				}
				else
				{
					bRc = bRc && serializer.Put(false);
				}
			}
		}

		if (!bRc)
		{
			serializer.Clear();
		}
	}

	if ((AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataSpatialAudioGeometry)) || 
		(AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataSpatialAudioGeometryDebug)))
	{
		AkVarLenDataCreator creator(AkMonitorData::MonitorDataSpatialAudioGeometry);
		AK::MonitorSerializer& serializer = creator.GetSerializer();

		bool bRc = serializer.Put(m_GeometrySetMap.Length());
		for (AkGeometrySetMap::IteratorEx it = m_GeometrySetMap.BeginEx(); it != m_GeometrySetMap.End(); ++it)
		{
			AkGeometrySet* pGeomSet = *it;

			bRc = bRc && serializer.Put(pGeomSet->groupID.id);

			bRc = bRc && serializer.Put(pGeomSet->numVerts);
			for (AkUInt32 i = 0; i < pGeomSet->numVerts; ++i)
			{
				AkVertex& vert = pGeomSet->verts[i];

				bRc = bRc && serializer.Put(vert.X);
				bRc = bRc && serializer.Put(vert.Y);
				bRc = bRc && serializer.Put(vert.Z);
			}

			bRc = bRc && serializer.Put(pGeomSet->numTris);
			for (AkUInt32 i = 0; i < pGeomSet->numTris; ++i)
			{
				AkTriangle& tri = pGeomSet->tris[i];

				bRc = bRc && serializer.Put(tri.point0);
				bRc = bRc && serializer.Put(tri.point1);
				bRc = bRc && serializer.Put(tri.point2);
				bRc = bRc && serializer.Put(tri.surface);
			}

			bRc = bRc && serializer.Put(pGeomSet->numSurfs);
			for (AkUInt32 i = 0; i < pGeomSet->numSurfs; ++i)
			{
				AkAcousticSurface& surf = pGeomSet->surfs[i];

				bRc = bRc && serializer.Put(surf.textureID);
				AkUInt32 placeholder = 0; // Was reflectorChannelMask. Saving this 32 bits as a placeholder for surface occlusion, after dev branch for WG-43736 is merged.
				bRc = bRc && serializer.Put(placeholder);
			}

			bRc = bRc && serializer.Put(pGeomSet->numEdges);
			for (AkUInt32 i = 0; i < pGeomSet->numEdges; ++i)
			{
				CAkDiffractionEdge& edge = pGeomSet->edges[i];

				bRc = bRc && serializer.Put(edge.start.X);
				bRc = bRc && serializer.Put(edge.start.Y);
				bRc = bRc && serializer.Put(edge.start.Z);

				bRc = bRc && serializer.Put(edge.direction.X);
				bRc = bRc && serializer.Put(edge.direction.Y);
				bRc = bRc && serializer.Put(edge.direction.Z);

				bRc = bRc && serializer.Put(edge.length);

				bRc = bRc && serializer.Put(edge.n0.X);
				bRc = bRc && serializer.Put(edge.n0.Y);
				bRc = bRc && serializer.Put(edge.n0.Z);

				bRc = bRc && serializer.Put(edge.n1.X);
				bRc = bRc && serializer.Put(edge.n1.Y);
				bRc = bRc && serializer.Put(edge.n1.Z);

 				if (AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataSpatialAudioGeometryDebug))
 				{
					// TODO: Serialization of edge visibility network.
					bRc = bRc && serializer.Put((AkUInt16)0);
					bRc = bRc && serializer.Put((AkUInt16)0);
					bRc = bRc && serializer.Put((AkUInt16)0);
 				}
 				else
				{
					bRc = bRc && serializer.Put((AkUInt16)0);
					bRc = bRc && serializer.Put((AkUInt16)0);
					bRc = bRc && serializer.Put((AkUInt16)0);
				}
			}
		}

		if (AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataSpatialAudioGeometryDebug))
		{
			// TODO: Serialization of edge visibility network.
			bRc = bRc && serializer.Put((AkUInt16)0);
		}
		else
		{
			bRc = bRc && serializer.Put((AkUInt16)0);
		}

		if (!bRc)
		{
			serializer.Clear();
		}
	}
#endif
}
