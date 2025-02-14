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
#include "AkSpatialAudioTasks.h"
#include "AkOcclusionChecker.h"
#include "AkSoundGeometry.h"
#include "AkStochasticReflectionEngine.h"

extern AkSpatialAudioInitSettings g_SpatialAudioSettings;

struct VisibleEdgeFinder
{
	VisibleEdgeFinder(AkEdgeArray& in_visibleEdges, const Ak3DVector& in_pt, const AkTriangleIndex& in_triangles) : visibleEdges(in_visibleEdges), pt(in_pt), triangles(in_triangles) {}

	AkForceInline bool Add(CAkDiffractionEdge* in_pEdge)
	{
		if (in_pEdge->GetZone(pt) != -2)
		{
			Ak3DVector offset = in_pEdge->n0 * AK_SA_EPSILON + in_pEdge->n1 * AK_SA_EPSILON;
			Ak3DVector ptEdge = in_pEdge->start + (in_pEdge->direction * in_pEdge->length / 2.f) + offset;
			Ak3DVector dir = pt - ptEdge;

			AKSIMD_V4F32 o = ptEdge.PointV4F32();
			AKSIMD_V4F32 d = dir.VectorV4F32();

			OneEdgeFilter filter(in_pEdge);
			OcclusionChecker<OneEdgeFilter> checker(o, d, filter);
			triangles.RaySearch(o, d, checker);

			if (!checker.IsOccluded())
				visibleEdges.AddLast(in_pEdge);
		}

		return true;
	}

	AkEdgeArray& visibleEdges;
	const Ak3DVector& pt;
	const AkTriangleIndex& triangles;
};

void UpdateVisibleEdges(AkEdgeArray& out_visibleEdges, const Ak3DVector& in_position, const AkBoundingBox& in_searchSpace, const AkTriangleIndex& in_triangles, const AkEdgeIndex& in_edges)
{
	VisibleEdgeFinder finder(out_visibleEdges, in_position, in_triangles);
	in_edges.Search(in_searchSpace.m_Min.PointV4F32(), in_searchSpace.m_Max.PointV4F32(), finder);
}

void GetSceneHelper(CAkSpatialAudioComponent* in_pCompt, AkSoundGeometry* in_pGeometry, AkAcousticRoom** out_pRooms, AkScene** out_pScenes, AkUInt32& io_uNumScenes )
{
	AkUInt32 uNumScenes = 1;
	out_pScenes[0] = in_pGeometry->GetGlobalScene();
	out_pRooms[0] = NULL;

	AkRoomID roomA = in_pCompt->GetActiveRoom();
	out_pRooms[0] = in_pGeometry->GetRoom(roomA);
	if (out_pRooms[0] != NULL)
	{
		out_pScenes[0] = out_pRooms[0]->GetScene();
	}

	if (io_uNumScenes > 1 && in_pCompt->IsTransitioning())
	{
		AkRoomID roomB = in_pCompt->GetTransitionRoom();
		out_pRooms[1] = in_pGeometry->GetRoom(roomB);
		if (out_pRooms[1] != NULL && out_pRooms[1]->GetScene() != out_pScenes[0])
		{
			out_pScenes[1] = out_pRooms[1]->GetScene();
			uNumScenes++;
		}
	}

	io_uNumScenes = uNumScenes;
}

// Update listener, independent of emitters.  This task may only modify/write to data on a single listener.
// Only called if the listener has moved (or geometry has changed), but not (necessarily) if any emitter has moved.
void UpdateListenerIndependent(CAkSpatialAudioListener* pListener, AkSoundGeometry* pGeometry)
{
	AkAcousticRoom* pListenerRoom = pGeometry->GetRoom(pListener->GetActiveRoom());
	AkScene* pScene = pListenerRoom != NULL ? pListenerRoom->GetScene() : pGeometry->GetGlobalScene();
	
	CAkStochasticReflectionEngine stochasticEngine(g_SpatialAudioSettings.uNumberOfPrimaryRays, g_SpatialAudioSettings.uMaxReflectionOrder, g_SpatialAudioSettings.fMaxPathLength);
	stochasticEngine.EnableDiffractionOnReflections(g_SpatialAudioSettings.bEnableDiffractionOnReflection);
	
	if (pListener->GetSpatialAudioComponent()->IsRoomDirty())
	{
		pListener->GetStochasticRays().RemoveAll();
	}
	
	StochasticRayGeneratorWithDirectRay rayGenerator(pListener);
	stochasticEngine.Compute(pListener, pScene->GetTriangleIndex(), rayGenerator);

	// Listener-side diffraction
	if (pListenerRoom != NULL)
	{
		bool bListenerMoved = pListener->GetSpatialAudioComponent()->IsPositionDirty();

		stochasticEngine.EnableDirectPathDiffraction(g_SpatialAudioSettings.bEnableDirectPathDiffraction);
		for (AkPathsToPortals::Iterator it = pListener->m_PathsToPortals.Begin(); it != pListener->m_PathsToPortals.End(); ++it)
		{
			if (bListenerMoved || (*it).IsNew())
			{
				AkPortalID portalID = (*it).key;
				AkAcousticPortal* pPortal = pGeometry->GetPortal(portalID);

				stochasticEngine.ComputePaths(pListener, *it, *pPortal, *pListenerRoom);

				// We need the path length for use in determining the shortest path to a portal in AkAcousticRoom::GetShortestPath().
				(*it).diffractions.CalcDiffraction();
				(*it).Validate();
			}
		}
	}
}

// Update emitter, independent of listener. This task may only modify/write to data on a single emitter.
// Only called if the emitter has moved (or geometry has changed), but not (necessarily) if the listener has moved.
void UpdateEmitterIndependent(CAkSpatialAudioEmitter* pEmitter, AkSoundGeometry* pGeometry)
{
	AkScene* pScene[2];
	AkAcousticRoom* pEmitterRoom[2];
	AkUInt32 numScene = 2;
	GetSceneHelper(pEmitter->GetSpatialAudioComponent(), pGeometry, pEmitterRoom, pScene, numScene);
	
	// Compute Reflections/Diffractions from emitter to portal and store the info into to the portal
	for (AkUInt32 i = 0; i < numScene; ++i)
	{
		for (AkPathsToPortals::Iterator it = pEmitter->m_PathsToPortals.Begin(); it != pEmitter->m_PathsToPortals.End(); ++it)
		{
			AkPortalID portalID = (*it).key;
			AkAcousticPortal* pPortal = pGeometry->GetPortal(portalID);

			CAkStochasticReflectionEngine stochasticEngine(g_SpatialAudioSettings.uNumberOfPrimaryRays, g_SpatialAudioSettings.uMaxReflectionOrder, g_SpatialAudioSettings.fMaxPathLength);
			stochasticEngine.EnableDiffractionOnReflections(g_SpatialAudioSettings.bEnableDiffractionOnReflection);
			stochasticEngine.EnableDirectPathDiffraction(g_SpatialAudioSettings.bEnableDirectPathDiffraction);

			stochasticEngine.ComputePaths(*pPortal, *it, *pEmitterRoom[i], pEmitter);

			(*it).diffractions.CalcDiffraction();
		}
	}
}

// Update emitter dependent on previous independent tasks above.
// Called when either the emitter or the listener has moved.
void UpdateEmitterListener(CAkSpatialAudioEmitter* pEmitter, AkSoundGeometry* pGeometry)
{
	CAkSpatialAudioListener* pListener = pEmitter->GetListener();

	AkScene* pScene[2];
	AkAcousticRoom* pEmitterRoom[2];
	AkUInt32 numScene = 2;
	GetSceneHelper(pEmitter->GetSpatialAudioComponent(), pGeometry, pEmitterRoom, pScene, numScene);

	pEmitter->ClearPathsToListener();

	if (pEmitter->HasReflections() || pEmitter->HasDiffraction())
	{
		for (AkUInt32 i = 0; i < numScene; ++i)
		{
			if (pListener)
			{
				if ( (i == 0 && pEmitter->GetActiveRoom() == pListener->GetActiveRoom())	||
					 (i == 1 && pEmitter->GetTransitionRoom() == pListener->GetActiveRoom()) )
				{
					// In the same room, we should use the triangles from the room				
					CAkStochasticReflectionEngine stochasticEngine(g_SpatialAudioSettings.uNumberOfPrimaryRays, g_SpatialAudioSettings.uMaxReflectionOrder, g_SpatialAudioSettings.fMaxPathLength);
					stochasticEngine.EnableDiffractionOnReflections(g_SpatialAudioSettings.bEnableDiffractionOnReflection);
					stochasticEngine.EnableDirectPathDiffraction(g_SpatialAudioSettings.bEnableDirectPathDiffraction);

					stochasticEngine.ComputePaths(pListener, pEmitter, pScene[i]->GetTriangleIndex(), i == 0);
				}
				else if (pEmitterRoom[i] != nullptr)
				{
					AkPropagationPathArray paths;
					pEmitterRoom[i]->GetPaths(pEmitter, pListener, paths);
					paths.BuildReflectionPaths(pListener, pEmitter, pEmitter->GetReflectionPaths());
				}
			}

			if (i == 0)
			{
				// Mark which paths come from the active room, and which from the transition room, so that we can apply a fade later on.
				pEmitter->GetReflectionPaths().MarkTransitionIdx();
			}
		}
	}

	if (g_SpatialAudioSettings.bEnableTransmission)
	{
		CAkSpatialAudioComponent* pListenerComponent = pListener->GetSpatialAudioComponent();
		CAkSpatialAudioComponent* pEmitterComponent = pEmitter->GetSpatialAudioComponent();

		if (pEmitterComponent->HasDiffraction() &&
			pListenerComponent->GetActiveRoom() != pEmitterComponent->GetActiveRoom())
		{
			AkAcousticRoom* pListenerRoom = pGeometry->GetRoom(pListenerComponent->GetActiveRoom());

			// Compute transmission path between listener and emitters not in the same room.
			AkReal32 occlusionValue = 0.f;
			
			if (pEmitterRoom[0] != nullptr)
				occlusionValue = AkMax( pEmitterRoom[0]->GetWallOcclusion(), occlusionValue) ;

			if (pListenerRoom != nullptr)
				occlusionValue = AkMax( pListenerRoom->GetWallOcclusion(), occlusionValue );

			if (occlusionValue < 1.0f)
			{
				Ak3DVector p0 = Ak3DVector(pEmitter->GetPosition());
				Ak3DVector p1 = Ak3DVector(pListener->GetPosition());
				Ak3DVector d = p1 - p0;

				AkReal32 emitterMaxDist = pEmitterComponent->GetMaxDistance();

				if (d.LengthSquared() < emitterMaxDist*emitterMaxDist)
				{
					AKSIMD_V4F32 pt0 = p0.PointV4F32();
					AKSIMD_V4F32 dir = d.VectorV4F32();

					TransmissionOcclusionChecker checker(pt0, dir);
					pScene[0]->GetTriangleIndex().RaySearch(pt0, dir, checker);
					occlusionValue = AkMax(occlusionValue, checker.GetOcclusion());

					AkScene* pListenerScene = pListenerRoom ? pListenerRoom->GetScene() : pGeometry->GetGlobalScene();
					if (pListenerScene && pListenerScene != pScene[0])
					{
						TransmissionOcclusionChecker checker(pt0, dir);
						pListenerScene->GetTriangleIndex().RaySearch(pt0, dir, checker);
						occlusionValue = AkMax(occlusionValue, checker.GetOcclusion());
					}
				}
			}

			pEmitter->GetDiffractionPaths().SetOcclusion(occlusionValue);
		}
	}
}

void AkSAObjectTaskFcn(void* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, AkTaskContext in_ctx, void* in_pCommonData)
{
	AkSoundGeometry* pGeometry = (AkSoundGeometry*)in_pCommonData;
	
	for(AkUInt32 idx = in_uIdxBegin; idx != in_uIdxEnd; ++idx)
	{
		AkSAObjectTaskData* pTask = (AkSAObjectTaskData*)in_pData + idx;
	
		switch (pTask->eTaskID)
		{
		case TaskID_Listener:
		{
			UpdateListenerIndependent(pTask->pListener, pGeometry);
		}
		break;

		case TaskID_Emitter:
		{
			UpdateEmitterIndependent(pTask->pEmitter, pGeometry);
		}
		break;

		case TaskID_EmitterListener:
		{
			UpdateEmitterListener(pTask->pEmitter, pGeometry);
		}
		break;

		default:
			AKASSERT(false);
		}
	}
}

bool AkSAPortalTaskData::ResolvePointers(AkSoundGeometry* pGeometry, AkAcousticPortal*& out_pPortal0, AkAcousticPortal*& out_pPortal1, AkAcousticRoom*& out_pRoom)
{
	out_pPortal0 = pGeometry->GetPortal(pair.p0);
	out_pPortal1 = pGeometry->GetPortal(pair.p1);

	if (out_pPortal0 == nullptr || out_pPortal1 == nullptr)
		return false;

	out_pRoom = out_pPortal0->GetCommonRoom(out_pPortal1);

	if (out_pRoom == nullptr)
		return false;

	return true;
}

void AkSAPortalTaskFcn(void* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, AkTaskContext in_ctx, void* in_pCommonData)
{
	AkSoundGeometry* pGeometry = (AkSoundGeometry*)in_pCommonData;

	for (AkUInt32 idx = in_uIdxBegin; idx != in_uIdxEnd; ++idx)
	{
		AkSAPortalTaskData* pTask = (AkSAPortalTaskData*)in_pData + idx;
		
		AkAcousticPortal* pPortal0, *pPortal1;
		AkAcousticRoom* pRoom;

		AKVERIFY(pTask->ResolvePointers(pGeometry, pPortal0, pPortal1, pRoom));

		CAkStochasticReflectionEngine stochasticEngine(g_SpatialAudioSettings.uNumberOfPrimaryRays, g_SpatialAudioSettings.uMaxReflectionOrder, g_SpatialAudioSettings.fMaxPathLength);
		stochasticEngine.EnableDiffractionOnReflections(true);
		stochasticEngine.EnableDirectPathDiffraction(g_SpatialAudioSettings.bEnableDirectPathDiffraction);

		stochasticEngine.ComputePaths( *pPortal0, *pPortal1, *pRoom, pTask->paths );

		pTask->paths.CalcDiffraction();
	}
}

void AkSAPortalTaskData::ProcessResults(AkSAPortalTaskData* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, void* in_pUserData)
{
	AkSoundGeometry* pGeometry = (AkSoundGeometry*)in_pUserData;

	for (AkUInt32 idx = in_uIdxBegin; idx != in_uIdxEnd; ++idx)
	{
		AkSAPortalTaskData* pTask = in_pData + idx;

		AkAcousticPortal* pPortal0, *pPortal1;
		AkAcousticRoom* pRoom;

		AKVERIFY(pTask->ResolvePointers(pGeometry, pPortal0, pPortal1, pRoom));

		pRoom->SetP2PPath(pPortal0->GetID(), pPortal1->GetID(), pTask->paths);

		pTask->paths.Term();
	}
}

void AkSAPortalRaycCastingTaskFnc(void* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, AkTaskContext in_ctx, void* in_pUserData)
{
	for (AkUInt32 idx = in_uIdxBegin; idx != in_uIdxEnd; ++idx)
	{
		AkSAPortalRayCastingTaskData* pTask = (AkSAPortalRayCastingTaskData*)in_pData + idx;

		AkAcousticPortal* pPortal = pTask->pPortal;
		AkAcousticRoom* pRoomFront, *pRoomBack;
		pPortal->GetRooms(pRoomFront, pRoomBack);
		
		if ((pTask->roomFlags & AkSAPortalRayCastingTaskData::Front) != 0)
		{
			CAkStochasticReflectionEngine stochasticEngine(g_SpatialAudioSettings.uNumberOfPrimaryRays, g_SpatialAudioSettings.uMaxReflectionOrder, g_SpatialAudioSettings.fMaxPathLength);			
			stochasticEngine.EnableDiffractionOnReflections(g_SpatialAudioSettings.bEnableDiffractionOnReflection);

			StochasticRayGenerator rayGenerator;
			stochasticEngine.Compute(*pPortal, *pRoomFront, rayGenerator);
		}

		if ((pTask->roomFlags & AkSAPortalRayCastingTaskData::Back) != 0)
		{
			CAkStochasticReflectionEngine stochasticEngine(g_SpatialAudioSettings.uNumberOfPrimaryRays, g_SpatialAudioSettings.uMaxReflectionOrder, g_SpatialAudioSettings.fMaxPathLength);			
			stochasticEngine.EnableDiffractionOnReflections(g_SpatialAudioSettings.bEnableDiffractionOnReflection);

			StochasticRayGenerator rayGenerator;
			stochasticEngine.Compute(*pPortal, *pRoomBack, rayGenerator);
		}
	}
}

void AkSAPortalRayCastingTaskData::ProcessResults(AkSAPortalRayCastingTaskData* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, void* in_pUserData)
{
	// Do nothing
}

