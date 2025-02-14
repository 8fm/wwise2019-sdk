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
#include "AkAcousticRoom.h"
#include "AkGameObject.h"
#include "AkConnectedListeners.h"
#include "AkRegisteredObj.h"
#include "Ak3DListener.h"
#include "AkRTPCMgr.h"
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include "AkRegistryMgr.h"
#include "AkMonitor.h"
#include "AkScene.h"

// Can be used for debugging spread angle, since it is not available to the profiler.
//#define AK_DEBUG_PORTAL_SPREAD

#ifndef AK_OPTIMIZED
#define _GetRoomGameObj(in_GameObjID, in_auxBusID, in_ListenerGameObjID, in_strName) GetRoomGameObj(in_GameObjID, in_auxBusID, in_ListenerGameObjID, in_strName)
#else
#define _GetRoomGameObj(in_GameObjID, in_auxBusID, in_ListenerGameObjID, in_strName) GetRoomGameObj(in_GameObjID, in_auxBusID, in_ListenerGameObjID)
#endif

//Returns an AddRef'd GameObject, Registering it if it doesn't already exist.
CAkGameObject* GetRoomGameObj(AkGameObjectID in_GameObjID, AkAuxBusID in_auxBusID, AkGameObjectID in_ListenerGameObjID
#ifndef AK_OPTIMIZED
	, const AK::SpatialAudio::OsString& in_strName
#endif
	)
{
	CAkGameObject* pGameObj = g_pRegistryMgr->GetObjAndAddref(in_GameObjID);
	if (pGameObj == NULL)
	{
		void* str = NULL;
#ifndef AK_OPTIMIZED
		if (in_strName.Get() != NULL)
		{
			AK::SpatialAudio::String strConverter(in_strName.Get());
			if (strConverter.Get() != NULL)
				str = AkMonitor::Monitor_AllocateGameObjNameString(in_GameObjID, strConverter.Get());
		}
#endif
		g_pRegistryMgr->RegisterObject(in_GameObjID, str);
		pGameObj = g_pRegistryMgr->GetObjAndAddref(in_GameObjID);
	}

	if (pGameObj != NULL)
	{
		// Dry path listener - always the player listener.
		if (g_pRegistryMgr->IsGameObjectRegistered(in_ListenerGameObjID))
		{
			AkAutoTermListenerSet listener;
			listener.Set(in_ListenerGameObjID);
			g_pRegistryMgr->UpdateListeners(pGameObj->ID(), listener, AkListenerOp_Set);
		}

		// Add CAkSpatialAudioRoomComponent spatial audio room tracking component
		CAkSpatialAudioRoomComponent* pRoomComponent = pGameObj->CreateComponent<CAkSpatialAudioRoomComponent>();
			
		// Add CAkListener component so sounds can send to the room.
		CAkListener* pListenerComponent = pGameObj->CreateComponent<CAkListener>();

		if (pRoomComponent != NULL && pListenerComponent != NULL)
		{
			// delete the emitter component (if exists)
			pGameObj->DeleteComponent<CAkSpatialAudioEmitter>();
			pRoomComponent->SetAuxID(in_auxBusID);
		}
		else
		{
			//Fail, backtrack.
			g_pRegistryMgr->UnregisterObject(in_GameObjID);
			pGameObj->Release();
			pGameObj = NULL;
		}
	}

	return pGameObj;
}

AkAcousticRoom::~AkAcousticRoom()
{
	KeepRoomGameObjRegistered(false, AK_INVALID_GAME_OBJECT);
	SetScene(NULL);
	m_P2PPaths.Term();
}

void AkAcousticRoom::SetParams(const AkRoomParams& in_Params, AkGameObjectID in_SpatialAudioListenerID)
{
	m_strName = in_Params.strName;
	m_strName.AllocCopy();

	m_ReverbAuxBus = in_Params.ReverbAuxBus;
	m_ReverbLevel = in_Params.ReverbLevel;
	m_WallOcclusion = in_Params.WallOcclusion;
	
	m_Front = in_Params.Front;
	m_Up = in_Params.Up;

	m_SelfSendLvl = in_Params.RoomGameObj_AuxSendLevelToSelf;

	KeepRoomGameObjRegistered(in_Params.RoomGameObj_KeepRegistered, in_SpatialAudioListenerID);
}

bool AkAcousticRoom::IsDirty()
{ 
	return m_bDirty || (m_Scene != NULL && m_Scene->IsDirty()); 
}

void AkAcousticRoom::KeepRoomGameObjRegistered(bool in_bKeepRegistered, AkGameObjectID in_SpatialAudioListenerID)
{
	if (in_bKeepRegistered && !m_AddRefdObj)
	{
		CAkGameObject* pRoomGameObj = _GetRoomGameObj(GetID().AsGameObjectID(), m_ReverbAuxBus, in_SpatialAudioListenerID, GetName());
		if (pRoomGameObj)
		{
			// We hold a reference (AddRefd in _GetRoomGameObj) to the game object so that it persists throughout the lifetime of the room. 
			m_AddRefdObj = true;

			g_pRegistryMgr->SetPosition(GetID().AsGameObjectID(), NULL, 0, AK::SoundEngine::MultiPositionType_MultiDirections);

			AkAuxSendValue sends;
			AkUInt32 uNumSends = GetSendToSelf(&sends);
			g_pRegistryMgr->SetGameObjectAuxSendValues(pRoomGameObj, &sends, uNumSends);
		}
	}
	else if (!in_bKeepRegistered && m_AddRefdObj)
	{
		CAkGameObject* pRoomGameObj = g_pRegistryMgr->GetObjAndAddref(GetID().AsGameObjectID());
		if (pRoomGameObj)
		{
			// Yes, Release 2x
			pRoomGameObj->Release(); // from GetObjAndAddref
			pRoomGameObj->Release(); // from m_AddRefdObj
			m_AddRefdObj = false;
		}
	}
}

AkReal32 AkAcousticRoom::GetERLevel() const
{
	return m_ERLevel;
}

AkUInt32 AkAcousticRoom::GetEnabledPortalCount() const
{
	AkUInt32 count = 0;
	for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
	{
		AkAcousticPortal* pPortal = (AkAcousticPortal*)*it;
		if (pPortal->IsEnabled())
			++count;
	}
	return count;
}

void AkAcousticRoom::SetScene(AkScene* in_scene)
{
	if (in_scene != m_Scene)
	{
		if (m_Scene != NULL)
			m_Scene->Release();

		m_Scene = in_scene;

		if (m_Scene != NULL)
			m_Scene->AddRef();

		for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
		{
			((AkAcousticPortal*)*it)->SetDirty(true);
		}
	}
}


AkPortalID AkAcousticRoom::GetTransitionRoom(	const Ak3DVector& in_Point,
											AkAcousticRoom*& out_pRoomA, AkAcousticRoom*& out_pRoomB,
											AkReal32& out_fRatio
											)
{
	out_pRoomA = this;
	out_pRoomB = NULL;
	out_fRatio = 1.f;

	for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
	{
		AkAcousticPortal* pPortal = (AkAcousticPortal*)*it;
		if (pPortal->IsEnabled() && pPortal->GetTransitionRoom(in_Point, out_pRoomA, out_pRoomB, out_fRatio))
			return pPortal->GetID();
	}

	return AkPortalID();
}

void AkAcousticRoom::InitContributingPortalPaths(	CAkSpatialAudioEmitter* in_pEmitter, 
													CAkSpatialAudioListener* in_pListener, 
													AkReal32 in_maxLength,
													AkSAPortalRayCastingTaskQueue& in_portalRaycastTasks,
													AkSAPortalToPortalTaskQueue& in_p2pTasks,
													bool& io_bAddedEmitterPath, 
													bool& io_bAddedListenerPath ) const
{

	if (in_pListener->GetSoundPropSync() != m_sync) // if the sync does not match, we are out of range.
		return;

	if (in_pListener->GetActiveRoom() == GetID()) // This room is assumed to be either the emitter's active or transition room.
		return;

	const Ak3DVector& emitterPos = in_pEmitter->GetPosition();
	const Ak3DVector& listenerPos = in_pListener->GetPosition();

	for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
	{
		AkAcousticPortal* pPortal = (AkAcousticPortal*)*it;
		if (pPortal->IsEnabled() && in_pListener->GetSoundPropSync() == pPortal->GetSync())
		{
			const AkPropagationPathArray& pathsPortalToListener = pPortal->GetPropagationPaths(in_pListener);
			for (AkPropagationPathArray::Iterator pathIt = pathsPortalToListener.Begin(); pathIt != pathsPortalToListener.End(); ++pathIt)
			{
				AkPropagationPath& portalPath = *pathIt;
				if (portalPath.rooms[portalPath.nodeCount - 1] != this) // skip paths that pass through this room.
				{
					Ak3DVector portalToEmitter = pPortal->GetClosestPointInOpening(emitterPos) - emitterPos;

					AkAcousticPortal* pListenerSidePortal = portalPath.ListenerSidePortal();
					Ak3DVector portalToListener = pListenerSidePortal->GetClosestPointInOpening(listenerPos) - listenerPos;

					AkReal32 totalPathLength = portalToEmitter.Length() + portalToListener.Length() + portalPath.length;
					if (totalPathLength < in_maxLength)
					{
						portalPath.EnqueuePortalTasks(this, in_portalRaycastTasks, in_p2pTasks, io_bAddedEmitterPath);

						if (in_pEmitter->HasReflectionsOrDiffraction())
						{
							PortalPathsStruct *portalPathStructEmitter = in_pEmitter->m_PathsToPortals.Exists(pPortal->GetID());
							if (!portalPathStructEmitter)
							{
								in_pEmitter->m_PathsToPortals.Set(pPortal->GetID());
								io_bAddedEmitterPath = true; // If at least one path from to a portal was added, we will need to perform a path update for the emitter.
							}
							else if (!portalPathStructEmitter->IsValid())
							{
								portalPathStructEmitter->Validate();
							}
						}

						// The listener-side paths are needed for wet diffraction, regardless of whether or not the emitter has diffraction/reflections.
						{
							AkPortalID listenerSidePortalID = pListenerSidePortal->GetID();
							PortalPathsStruct *portalPathStructListener = in_pListener->m_PathsToPortals.Exists(listenerSidePortalID);
							if (!portalPathStructListener)
							{
								in_pListener->m_PathsToPortals.Set(listenerSidePortalID);
								io_bAddedListenerPath = true; // If at least one path from to a portal was added, we will need to perform a path update for the listener.
							}
							else if (!portalPathStructListener->IsValid())
							{
								portalPathStructListener->Validate();
							}
						}
					}
				}
			}
		}
	}
}

void AkAcousticRoom::InitContributingPortalPaths_RoomTone(	
													CAkSpatialAudioListener* in_pListener, 
													AkReal32 in_maxLength,
													AkSAPortalRayCastingTaskQueue& in_portalRaycastTasks,
													AkSAPortalToPortalTaskQueue& in_p2pTasks,
													bool& io_bAddedListenerPath ) const
{

	if (in_pListener->GetSoundPropSync() != m_sync) // if the sync does not match, we are out of range.
		return;

	if (in_pListener->GetActiveRoom() == GetID()) // Listener is in this room, so the room tone will not go through a portal
		return;

	const Ak3DVector& listenerPos = in_pListener->GetPosition();

	for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
	{
		AkAcousticPortal* pPortal = (AkAcousticPortal*)*it;
		if (pPortal->IsEnabled() && in_pListener->GetSoundPropSync() == pPortal->GetSync())
		{
			const AkPropagationPathArray& pathsPortalToListener = pPortal->GetPropagationPaths(in_pListener);
			for (AkPropagationPathArray::Iterator pathIt = pathsPortalToListener.Begin(); pathIt != pathsPortalToListener.End(); ++pathIt)
			{
				AkPropagationPath& portalPath = *pathIt;
				if (portalPath.rooms[portalPath.nodeCount - 1] != this) // skip paths that pass through this room.
				{
					AkAcousticPortal* pListenerSidePortal = portalPath.ListenerSidePortal();
					Ak3DVector portalToListener = pListenerSidePortal->GetClosestPointInOpening(listenerPos) - listenerPos;

					AkReal32 totalPathLength = portalToListener.Length() + portalPath.length;
					if (totalPathLength < in_maxLength)
					{
						bool unused;
						portalPath.EnqueuePortalTasks(this, in_portalRaycastTasks, in_p2pTasks, unused);

						AkPortalID listenerSidePortalID = pListenerSidePortal->GetID();
						PortalPathsStruct *portalPathStructListener = in_pListener->m_PathsToPortals.Exists(listenerSidePortalID);
						if (!portalPathStructListener)
						{
							in_pListener->m_PathsToPortals.Set(listenerSidePortalID);
							io_bAddedListenerPath = true; // If at least one path from to a portal was added, we will need to perform a path update for the listener.
						}
						else if (!portalPathStructListener->IsValid())
						{
							portalPathStructListener->Validate();
						}
					}
				}
			}
		}
	}
}

void AkAcousticRoom::GetPaths(const CAkSpatialAudioEmitter* in_pEmitter, const CAkSpatialAudioListener* in_pListener, AkPropagationPathArray& out_paths) const
{ 
	if (in_pListener->GetSoundPropSync() != m_sync) // if the sync does not match, we are out of range.
		return;

 	if (in_pListener->GetActiveRoom() == GetID()) // This room is assumed to be either the emitter's active or transition room.
 		return;

	const Ak3DVector& emitterPos = in_pEmitter->GetPosition();
	for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
	{
		AkAcousticPortal* pPortal = (AkAcousticPortal*)*it;
		if (pPortal->IsEnabled() && in_pListener->GetSoundPropSync() == pPortal->GetSync())
		{
			const AkPropagationPathArray& pathsFromPortal = pPortal->GetPropagationPaths(in_pListener);
			if (!pathsFromPortal.IsEmpty())
			{
				Ak3DVector v = pPortal->GetClosestPointInOpening(emitterPos) - emitterPos;
				AkReal32 distPortalToEmitter = v.Length();

				for (AkPropagationPathArray::Iterator pathIt = pathsFromPortal.Begin(); pathIt != pathsFromPortal.End(); ++pathIt)
				{
					AkPropagationPath& portalPath = *pathIt;
					if ( portalPath.rooms[portalPath.nodeCount-1] != this ) // skip paths that pass through this room.
					{
						AkPropagationPath* pPath = out_paths.Set(AkPropagationPath::Get(portalPath));
						if (pPath)
						{
							if (portalPath.length + distPortalToEmitter < pPath->length)
							{
								*pPath = portalPath;

								//update length to properly include segment from portal to emitter.
								pPath->length = (portalPath.length + distPortalToEmitter);
							}
						}
					}
				}
			}
		}
	}
	
}

void AkAcousticRoom::PropagateSound(CAkSpatialAudioListener* in_pERL, AkUInt32 in_maxDepth)
{
	AkAcousticPortal** portals = (AkAcousticPortal**)alloca(in_maxDepth * sizeof(AkAcousticPortal*));
	AkAcousticRoom** rooms = (AkAcousticRoom**)alloca(in_maxDepth * sizeof(AkAcousticPortal*) + 1);

	Traverse(in_pERL, in_maxDepth, 0, 0.f, 1.f, 0.f, portals, rooms);

	in_pERL->FinishSoundPropagation();
}

void AkAcousticRoom::Traverse(CAkSpatialAudioListener* in_pERL, AkUInt32 in_maxDepth, AkUInt32 in_currentDepth, AkReal32 in_pathLength, AkReal32 in_gain, AkReal32 in_diffraction, AkAcousticPortal** io_portals, AkAcousticRoom** io_rooms)
{
	EnsureGameObjIsRegistered(in_pERL);

	io_rooms[in_currentDepth] = this;
	
	m_sync = in_pERL->GetSync();

	if (in_currentDepth < in_maxDepth)
	{
		for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
		{
			AkAcousticPortal* pPortal = (AkAcousticPortal*)*it;
			if (pPortal->IsEnabled() && !AkInArray(pPortal, io_portals, in_currentDepth))
			{
				bool bReachable = true;
				AkReal32 distance = 0, diffraction = 0;
				if (in_currentDepth > 0)
				{
					bReachable = GetP2PPathLength(pPortal, io_portals[in_currentDepth - 1], distance, diffraction);
				}

				if (bReachable)
					pPortal->Traverse(in_pERL, in_maxDepth, in_currentDepth, in_pathLength + distance, in_gain, in_diffraction + diffraction, io_portals, io_rooms);
			}
		}
	}
}

AkAcousticPortal* AkAcousticRoom::GetConnectedPortal(AkPortalID in_id) const
{
	for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
	{
		AkAcousticPortal* pPortal = (AkAcousticPortal*)*it;
		if (pPortal->GetID() == in_id)
			return pPortal;
	}

	return NULL;
}

AkAcousticPortal* AkAcousticRoom::GetClosestPortal(const Ak3DVector& in_pt) const
{
	if (m_Links.Length() > 1)
	{
		AkReal32 dSqrMin = FLT_MAX;
		AkAcousticPortal* pClosestPortal = NULL;

		for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
		{
			AkAcousticPortal* pPortal = (AkAcousticPortal*)*it;
			if (pPortal->IsEnabled())
			{
				Ak3DVector portalVec = pPortal->GetClosestPointInOpening(in_pt) - in_pt;
				AkReal32 dSqr = portalVec.LengthSquared();
				if (dSqr < dSqrMin)
				{
					pClosestPortal = pPortal;
					dSqrMin = dSqr;
				}
			}
		}

		return pClosestPortal;
	}
	else if (m_Links.Length() > 0)
	{
		AkAcousticPortal* pPortal = (AkAcousticPortal*)(*m_Links.Begin());
		return pPortal->IsEnabled() ? pPortal : NULL;
	}
	else
	{
		return NULL;
	}
}

AkPropagationPath* AkAcousticRoom::GetShortestPath(const CAkSpatialAudioListener* in_pListener) const
{
	AkReal32 fShortestPathLen = FLT_MAX;
	AkPropagationPath* pPath = NULL;
	for (tLinkIter itPortal = m_Links.Begin(); itPortal != m_Links.End(); ++itPortal)
	{
		AkAcousticPortal* pPortal = ((AkAcousticPortal*)*itPortal);
		if (pPortal->IsEnabled())
		{
			const AkPropagationPathArray& paths = pPortal->GetPropagationPaths(in_pListener);
			for (AkPropagationPathArray::Iterator it = paths.Begin(); it != paths.End(); ++it)
			{
				AkPropagationPath& path = *it;
				if (path.rooms[path.nodeCount - 1] != this) // skip paths that pass through this room.
				{
					AkReal32 listenerToPortal;

					AkAcousticPortal* pListenerSidePortal = path.ListenerSidePortal();
					PortalPathsStruct* pps = in_pListener->m_PathsToPortals.Exists(pListenerSidePortal->GetID());
					if (pps)
					{
						if (pps->diffractions.HasDirectLineOfSight())
						{
							Ak3DVector portalPos = pListenerSidePortal->GetClosestPointInOpening(in_pListener->GetPosition());
							listenerToPortal = (portalPos - in_pListener->GetPosition()).Length();
						}
						else
						{
							AkDiffractionPathSegment* pathSegmentListenerToPortal = pps->diffractions.ShortestPath();
							if (pathSegmentListenerToPortal != NULL)
								listenerToPortal = pathSegmentListenerToPortal->totLength;
							else
								listenerToPortal = in_pListener->GetVisibilityRadius(); // No paths in range, but preferred to return an out-of-range path over no path.
						}
					}
					else
					{
						listenerToPortal = in_pListener->GetVisibilityRadius(); // No paths in range, but preferred to return an out-of-range path over no path.
					}

					AkReal32 totalDistance = path.length + listenerToPortal;
					if (totalDistance < fShortestPathLen)
					{
						fShortestPathLen = totalDistance;
						pPath = &path;
					}
				}
			}
		}
	}
	return pPath;
}

void AkAcousticRoom::Update(CAkSpatialAudioListener* in_pListener, AkUInt32 uDiffractionFlags)
{
	CAkSpatialAudioComponent* pListenerComponent = in_pListener->GetSpatialAudioComponent();

	if (m_sync != in_pListener->GetSoundPropSync())
	{
		// Room is 'out of range'
		SetOutOfRange(pListenerComponent, uDiffractionFlags);
	}
	else
	{
		if (pListenerComponent->GetActiveRoom() != GetID())
		{
			/// Add a path token to the room object
			AkPropagationPath* pPath = GetShortestPath(in_pListener);

			// -- Room Game object update -- for depth > 0 (not listeners room)
			if (pPath != NULL)
			{
				CAkGameObject* pRoomGameObj = _GetRoomGameObj(GetID().AsGameObjectID(), m_ReverbAuxBus, in_pListener->ID(), GetName());
				if (pRoomGameObj)
				{
					in_pListener->GetRoomGameObjs().AddRef(pRoomGameObj);

					// depth > 1 - chaining rooms
					const AkUInt32 kMaxSends = 2;
					AkAuxSendValue sends[kMaxSends];

					//Get the sends from the path.
					AkRoomRvbSend prevRoom;
					AkAcousticRoom* pPrevRoom = pPath->rooms[pPath->nodeCount - 1];
					pPrevRoom->EnsureGameObjIsRegistered(in_pListener);
					pPrevRoom->GetRoomRvbSend(prevRoom);
					sends[0].auxBusID = prevRoom.auxBus;
					sends[0].fControlValue = prevRoom.ctrlVal;
					sends[0].listenerID = prevRoom.room.AsGameObjectID();
					AkUInt32 uNumSends = 1;

					if (pListenerComponent->IsTransitioning() && pListenerComponent->GetTransitionRoom() == GetID())
					{
						//The listener is transitioning into this room.  Fade out the send according to the ratio.
						AkReal32 fade = 2.f * (pListenerComponent->GetTransitionRatio() - 0.5f);
						sends[0].fControlValue *= fade;
					}
					
					uNumSends += GetSendToSelf(sends + uNumSends);

					AKASSERT(uNumSends <= kMaxSends);
					g_pRegistryMgr->SetGameObjectAuxSendValues(pRoomGameObj, sends, uNumSends);

					if (pListenerComponent->IsTransitioning() && pListenerComponent->GetTransitionRoom() == GetID())
						UpdateTransitionRoomGameObj(pRoomGameObj, pListenerComponent, uDiffractionFlags);
					else
						UpdateAdjacentRoomGameObj(pRoomGameObj, *pPath, pListenerComponent, uDiffractionFlags);

					pRoomGameObj->Release();
				}
			}

		}
		else
		{
			// -- Room Game object update -- for the player listener's room,
			CAkGameObject* pRoomGameObj = _GetRoomGameObj(GetID().AsGameObjectID(), m_ReverbAuxBus, in_pListener->ID(), GetName());
			if (pRoomGameObj != NULL)
			{
				in_pListener->GetRoomGameObjs().AddRef(pRoomGameObj);
				
				if (pListenerComponent->IsTransitioning())
					UpdateTransitionRoomGameObj(pRoomGameObj, pListenerComponent, uDiffractionFlags);
				else
					UpdateListenerRoomGameObj(pRoomGameObj, pListenerComponent, uDiffractionFlags);

				AkAuxSendValue sends;
				AkUInt32 uNumSends = GetSendToSelf(&sends);

				// Clear the aux sends on the listener's room game object.
				g_pRegistryMgr->SetGameObjectAuxSendValues(pRoomGameObj, &sends, uNumSends);

				pRoomGameObj->Release();
			}

		}
	}
}

void AkAcousticRoom::UpdateListenerRoomGameObj(CAkGameObject* pRoomGameObj, CAkSpatialAudioComponent* in_pListener, AkUInt32 uDiffractionFlags) const
{
	CAkListener* pListenerListenerCmpt = in_pListener->GetOwner()->GetComponent<CAkListener>();
	const AkTransform& xfrm = pListenerListenerCmpt->GetTransform();
	Ak3DVector in_listenerPos = xfrm.Position();
	Ak3DVector in_listenerUp = xfrm.OrientationTop();

	AkChannelEmitter chanEm;
	chanEm.uInputChannels = (AkChannelMask)-1;
	chanEm.position.SetPosition((const AkVector&)in_listenerPos);
	chanEm.position.SetOrientation((const AkVector&)GetFront(), (const AkVector&)GetUp());

	AkReal32 spread_pct = 100.f;
	AkReal32 focus_pct = 0.f;

	AkAcousticPortal* pClosestPortal = GetClosestPortal(in_listenerPos);
	if (pClosestPortal)
	{
		Ak3DVector portalPosition = pClosestPortal->CalcPortalAparentPosition(GetID(), in_listenerPos, in_pListener->GetActiveRoom());

		chanEm.position.SetPosition((AkVector)portalPosition);

		AkReal32 spread_rad = pClosestPortal->GetAparentSpreadAngle_SIMD(in_listenerPos, in_listenerUp);
		
		if (in_pListener->GetActiveRoom() == GetID())
			spread_pct = 100.f - (spread_rad * 50.f / PI);
		else
			spread_pct = (spread_rad * 50.f / PI); // <- Transitioning into active room.
	}

	AkObstructionOcclusionValues obsOcc;
	obsOcc.obstruction = 0.f;
	obsOcc.occlusion = 0.f;

	UpdateRoomGameObjHelper(
		pRoomGameObj,
		in_pListener->ID(),
		in_listenerPos,
		false,
		&obsOcc,
		&chanEm,
		&spread_pct,
		&focus_pct,
		1,
		0.f,
		uDiffractionFlags
		);

}

void AkAcousticRoom::UpdateTransitionRoomGameObj(CAkGameObject* pRoomGameObj, CAkSpatialAudioComponent* in_pListener, AkUInt32 uDiffractionFlags) const
{
	AKASSERT(in_pListener->IsTransitioning());
	AKASSERT(in_pListener->GetActiveRoom() == GetID() || in_pListener->GetTransitionRoom() == GetID());

	CAkSpatialAudioListener* pSpatialAudioListenerComponent = in_pListener->GetOwner()->GetComponent<CAkSpatialAudioListener>();
	CAkListener* pListenerListenerCmpt = in_pListener->GetOwner()->GetComponent<CAkListener>();
	const AkTransform& xfrm = pListenerListenerCmpt->GetTransform();
	Ak3DVector in_listenerPos = xfrm.Position();
	Ak3DVector in_listenerUp = xfrm.OrientationTop();
	
	AkUInt32 uNumSoundPositions = 1; // start with one for the transition portal. 

	AkAcousticPortal* pTransitionPortal = GetConnectedPortal(in_pListener->GetPortal());
	AkAcousticRoom* pAdjacentRoom = NULL;
	if (pTransitionPortal != NULL)
	{
		pAdjacentRoom = pTransitionPortal->GetRoomOnOtherSide(GetID());
		if (pAdjacentRoom != NULL)
		{
			for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
			{
				AkAcousticPortal* pPortal = (AkAcousticPortal*)*it;

				if (pPortal != pTransitionPortal &&
					pPortal->IsEnabled() &&
					pPortal->IsConnectedTo(pAdjacentRoom->GetID())
					)
				{
					pPortal->BuildDiffractionPaths(GetID(), pSpatialAudioListenerComponent, in_listenerPos);

					uNumSoundPositions += pPortal->GetDiffractionPaths().Length();
				}
			}
		}
	}

	AkChannelEmitter* soundPos = (AkChannelEmitter*)alloca(uNumSoundPositions * sizeof(AkChannelEmitter));
	AkReal32* spread = (AkReal32*)alloca(uNumSoundPositions * sizeof(AkReal32));
	AkReal32* focus = (AkReal32*)alloca(uNumSoundPositions * sizeof(AkReal32));
	AkObstructionOcclusionValues* obsOcc = (AkObstructionOcclusionValues*)alloca(uNumSoundPositions * sizeof(AkObstructionOcclusionValues));

	soundPos[0].uInputChannels = (AkChannelMask)-1;
	soundPos[0].position.SetPosition((const AkVector&)in_listenerPos);
	soundPos[0].position.SetOrientation((const AkVector&)GetFront(), (const AkVector&)GetUp());

	spread[0] = 100.f;
	focus[0] = 0.f;

	obsOcc[0].obstruction = 0.f;
	obsOcc[0].occlusion = 0.f;

	AkReal32 ratio = in_pListener->GetTransitionRatio();

	AkUInt32 numPos = 1;
	
	if (pTransitionPortal)
	{
		Ak3DVector portalPosition = pTransitionPortal->CalcPortalAparentPosition(GetID(), in_listenerPos, in_pListener->GetActiveRoom());

		soundPos[0].position.SetPosition((AkVector)portalPosition);

		AkReal32 spread_rad = pTransitionPortal->GetAparentSpreadAngle_SIMD(in_listenerPos, in_listenerUp);

		if (in_pListener->GetActiveRoom() == GetID())
		{
			spread[0] = 100.f - (spread_rad * 50.f / PI);
		}
		else
		{
			// This room is the transition room.

			spread[0] = (spread_rad * 50.f / PI);

			obsOcc[0].obstruction = pTransitionPortal->GetObstruction();
			obsOcc[0].occlusion = pTransitionPortal->GetOcclusion();

			// flip the ratio to make it relative to this room.
			ratio = 1.f - ratio;
		}

		// Add sound positions for the other portals that are also connected to this room and the adjacent room.
		
		if (pAdjacentRoom != NULL)
		{
			for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
			{
				AkAcousticPortal* pPortal = (AkAcousticPortal*)*it;

				if (pPortal != pTransitionPortal &&
					pPortal->IsEnabled() &&
					pPortal->IsConnectedTo(pAdjacentRoom->GetID())
					)
				{
					AkReal32 unused;

					// We get the positions of the portals from the perspective of when the listener is in the adjacent room.
					GetPortalPositionDataEx(
						pPortal, 
						in_listenerPos, 
						in_listenerUp,
						uDiffractionFlags,
						soundPos, 
						spread, 
						focus, 
						obsOcc,
						unused,
						numPos
						);
				}
				else
				{
					pPortal->GetDiffractionPaths().Term();
				}
			}
		}
	}

	if (numPos > 1)
	{
		// Contract the positions of the other portals (ie not the one the listener is transitioning through) 
		Ak3DVector pos0 = soundPos[0].position.Position();

		AkReal32 fadeRatio = 1.f - ratio;
		fadeRatio = fadeRatio * fadeRatio * fadeRatio; //cubic fade
		for (AkUInt32 i = 1; i < numPos; ++i)
		{
			Ak3DVector pos_i = soundPos[i].position.Position();
			Ak3DVector pos_interp = pos0 * (1.f - fadeRatio) + pos_i * fadeRatio;
			
			soundPos[i].position.SetPosition((AkVector)pos_interp);
		}
	}

	UpdateRoomGameObjHelper(
		pRoomGameObj,
		in_pListener->ID(),
		in_listenerPos,
		false,
		obsOcc,
		soundPos,
		spread,
		focus,
		numPos,
		0.f,
		uDiffractionFlags
	);

	if (numPos > 0 &&
		pAdjacentRoom != NULL &&
		in_pListener->GetActiveRoom() != GetID())
	{
		CAkEmitter* pRoomEmitter = pRoomGameObj->CreateComponent<CAkEmitter>();
		if (pRoomEmitter)
		{
			//Set occlusion for the wet send to the adjacent room (obstruction is not used here, because there is an aux connection only).
			pRoomEmitter->SetMultipleObstructionAndOcclusion(pAdjacentRoom->GetID().AsGameObjectID(), obsOcc, numPos);
		}
	}
}

void AkAcousticRoom::UpdateAdjacentRoomGameObj(CAkGameObject* pRoomGameObj, const AkPropagationPath& in_path, CAkSpatialAudioComponent* in_pListener, AkUInt32 uDiffractionFlags) const
{
	CAkSpatialAudioListener* pSpatialAudioListenerComponent = in_pListener->GetOwner()->GetComponent<CAkSpatialAudioListener>();
	CAkListener* pListenerListenerCmpt = in_pListener->GetOwner()->GetComponent<CAkListener>();
	const AkTransform& xfrm = pListenerListenerCmpt->GetTransform();
	Ak3DVector in_listenerPos = xfrm.Position();
	Ak3DVector in_listenerUp = xfrm.OrientationTop();

	AkRoomID nextRoom = in_path.rooms[in_path.nodeCount - 1]->GetID();

	AkUInt32 uNumSoundPositions = 0;
	for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
	{
		AkAcousticPortal* pPortal = (AkAcousticPortal*)*it;

		if (pPortal->IsEnabled() && pPortal->IsConnectedTo(nextRoom))
		{
			pPortal->BuildDiffractionPaths(GetID(), pSpatialAudioListenerComponent, in_listenerPos);

			uNumSoundPositions += pPortal->GetDiffractionPaths().Length();
		}
	}

	AkReal32* spread = (AkReal32*)alloca(uNumSoundPositions *sizeof(AkReal32));
	AkReal32* focus = (AkReal32*)alloca(uNumSoundPositions * sizeof(AkReal32));
	AkObstructionOcclusionValues* obsOcc = (AkObstructionOcclusionValues*)alloca(uNumSoundPositions * sizeof(AkObstructionOcclusionValues));
	AkChannelEmitter* soundPos = (AkChannelEmitter*)alloca(uNumSoundPositions *sizeof(AkChannelEmitter));
	AkUInt32 positionIdx = 0;
	AkReal32 minWetDiffractionCoef = 1.0;
	bool bHasEnabledLink = false;
	
	for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
	{
		AkAcousticPortal* pPortal = (AkAcousticPortal*)*it;

		if (pPortal->IsEnabled() && pPortal->IsConnectedTo(nextRoom))
		{
			bHasEnabledLink = true;

			AkReal32 wetDiffractionCoef = 0.f;
			GetPortalPositionDataEx(
				pPortal,
				in_listenerPos, 
				in_listenerUp, 
				uDiffractionFlags,
				soundPos, 
				spread, 
				focus, 
				obsOcc, 
				wetDiffractionCoef,
				positionIdx);
			
			// The min is sent to the RTPC value, because we do not have per-ray RTPC.
			minWetDiffractionCoef = AkMin(minWetDiffractionCoef, wetDiffractionCoef);
		}
		else
		{
			pPortal->GetDiffractionPaths().Term();
		}
	}

	AKASSERT(positionIdx == uNumSoundPositions);

	UpdateRoomGameObjHelper(
		pRoomGameObj,
		in_pListener->ID(),
		in_listenerPos,
		false,
		obsOcc,
		soundPos,
		spread,
		focus,
		positionIdx,
		(bHasEnabledLink ? minWetDiffractionCoef : 0.f),
		uDiffractionFlags
		);


	if (positionIdx > 0)
	{
		CAkEmitter* pRoomEmitter = pRoomGameObj->CreateComponent<CAkEmitter>();
		if (pRoomEmitter)
		{
			//Set occlusion for the wet send to the adjacent room (obstruction is not used here, because there is an aux connection only).
			pRoomEmitter->SetMultipleObstructionAndOcclusion(nextRoom.AsGameObjectID(), obsOcc, positionIdx);
		}
	}
}

void AkAcousticRoom::GetPortalPositionData(
	AkAcousticPortal* pPortal,
	AkRoomID in_ListenerRoom,
	const Ak3DVector& in_listenerPos,
	const Ak3DVector& in_listenerUp,
	AkChannelEmitter& out_soundPos,
	AkReal32& out_spread,
	AkReal32& out_focus,
	AkObstructionOcclusionValues& out_obsOcc) const
{
	Ak3DVector portalPosition = pPortal->CalcPortalAparentPosition(GetID(), in_listenerPos, in_ListenerRoom);

	out_soundPos.position.SetPosition(portalPosition);
	out_soundPos.position.SetOrientation((const AkVector&)GetFront(), (const AkVector&)GetUp());
	out_soundPos.uInputChannels = (AkChannelMask)-1;

	AkReal32 spread_rad = pPortal->GetAparentSpreadAngle_SIMD(in_listenerPos, in_listenerUp);
	out_spread = spread_rad * 50.f / PI;
	out_focus = 0.f;

	out_obsOcc.obstruction = pPortal->GetObstruction();
	out_obsOcc.occlusion = pPortal->GetOcclusion();

	pPortal->SetWetDiffraction(0.f);
}

void AkAcousticRoom::GetPortalPositionDataEx(
	AkAcousticPortal* pPortal,
	const Ak3DVector& in_listenerPos,
	const Ak3DVector& in_listenerUp,
	AkUInt32 uDiffractionFlags,
	AkChannelEmitter* out_soundPos,
	AkReal32* out_spread,
	AkReal32* out_focus,
	AkObstructionOcclusionValues* out_obsOcc,
	AkReal32& out_wetDiffractionCoef,
	AkUInt32& io_posIdx) const
{
	CAkDiffractionPaths& portalDiffractionPaths = pPortal->GetDiffractionPaths();

	AkReal32 portalMinDiffraction = portalDiffractionPaths.IsEmpty() ? 0.f : FLT_MAX;
	
	for (CAkDiffractionPaths::Iterator it = portalDiffractionPaths.Begin(); it != portalDiffractionPaths.End(); ++it)
	{
		AkDiffractionPath& path = *it;

		if ((uDiffractionFlags & DiffractionFlags_CalcEmitterVirtualPosition) != 0)
		{
			out_soundPos[io_posIdx].position = path.virtualPos;
		}
		else
		{
			out_soundPos[io_posIdx].position.Set((AkVector&)path.emitterPos, (AkVector&)GetFront(), (AkVector&)GetUp());
		}

		out_soundPos[io_posIdx].uInputChannels = (AkChannelMask)-1;

		Ak3DVector portalAparentListenerPosition = in_listenerPos;
		if (path.nodeCount > 1)
		{
			// take the first node after pPortal (which should be node path->nodes[path->nodeCount - 1])
			portalAparentListenerPosition = path.nodes[path.nodeCount - 2];
		}

		AkReal32 spread_rad = pPortal->GetAparentSpreadAngle_SIMD(portalAparentListenerPosition, in_listenerUp);
		out_spread[io_posIdx] = spread_rad * 50.f / PI;
		out_focus[io_posIdx] = 0.f;

		out_obsOcc[io_posIdx].occlusion = pPortal->GetOcclusion();

		if ((uDiffractionFlags & DiffractionFlags_UseObstruction) != 0)
		{
			out_obsOcc[io_posIdx].obstruction = AkMax(pPortal->GetObstruction(), path.diffraction);
		}
		else
		{
			out_obsOcc[io_posIdx].obstruction = pPortal->GetObstruction();
		}

		if (path.diffraction < portalMinDiffraction)
			portalMinDiffraction = path.diffraction;

		++io_posIdx;
	}

	pPortal->SetWetDiffraction(portalMinDiffraction);

	out_wetDiffractionCoef = portalMinDiffraction;
}

void AkAcousticRoom::UpdateRoomGameObjHelper(	
			CAkGameObject* pRoomGameObj, 
			AkGameObjectID in_listenerGameObjID,
			const Ak3DVector& in_roomPickupPos,
			bool bSpatialized,
			AkObstructionOcclusionValues* obsOcc, 
			AkChannelEmitter* soundPos, 
			AkReal32* spread, 
			AkReal32* focus, 
			AkUInt32 numPos, 
			AkReal32 minDiffractionCoef,
			AkUInt32 uDiffractionFlags
			) const
{
	CAkEmitter* pRoomEmitter = pRoomGameObj->CreateComponent<CAkEmitter>();
	if (pRoomEmitter)
	{
		pRoomEmitter->SetPosition(soundPos, numPos, AK::SoundEngine::MultiPositionType_MultiDirections);
		pRoomEmitter->SetSpreadAndFocusValues(in_listenerGameObjID, spread, focus, numPos);
		pRoomEmitter->SetMultipleObstructionAndOcclusion(in_listenerGameObjID, obsOcc, numPos);

		// Need to also set the occlusion on the room game object, if sending to self.
		if (m_SelfSendLvl > 0.f)
			pRoomEmitter->SetMultipleObstructionAndOcclusion(GetID().AsGameObjectID(), obsOcc, numPos);

#ifdef AK_DEBUG_PORTAL_SPREAD
		AK::SoundEngine::SetRTPCValue("Spread", spread[0], pRoomGameObj->ID());
#endif
	}

	//Also update the listener component's position, if it exists.
	CAkListener* pListenerCmpt = pRoomGameObj->GetComponent<CAkListener>();
	if (pListenerCmpt)
	{
		//portalListenerPos = portalListenerPos / (AkReal32)numPos;
		AkListenerPosition lstnrPos;
		lstnrPos.SetPosition((const AkVector&)in_roomPickupPos);
		lstnrPos.SetOrientation((const AkVector&)GetFront(), (const AkVector&)GetUp());
		pListenerCmpt->SetSpatialized(bSpatialized);
		pListenerCmpt->SetPosition(lstnrPos);
	}


	if ((uDiffractionFlags & DiffractionFlags_UseBuiltInParam) != 0)
	{
		g_pRTPCMgr->SetBuiltInParamValue(BuiltInParam_Diffraction, AkRTPCKey((CAkRegisteredObj*)pRoomGameObj), minDiffractionCoef * 100.f);
	}
}

void AkAcousticRoom::EnsureGameObjIsRegistered(CAkSpatialAudioListener* in_pListener)
{
	//Register the game object during the traversal.
	CAkGameObject* pObj = _GetRoomGameObj(GetID().AsGameObjectID(), m_ReverbAuxBus, in_pListener->ID(), GetName());
	if (pObj)
		pObj->Release();
}

AkUInt32 AkAcousticRoom::GetSendToSelf(AkAuxSendValue* out_pValues) const
{
	AkUInt32 numAdded = 0;

	if (m_SelfSendLvl > 0.f)
	{
		out_pValues[numAdded].listenerID = GetID().AsGameObjectID();
		out_pValues[numAdded].auxBusID = m_ReverbAuxBus;
		out_pValues[numAdded].fControlValue = m_ReverbLevel * m_SelfSendLvl;
		++numAdded;
	}

	return numAdded;
}

void AkAcousticRoom::SetOutOfRange(CAkSpatialAudioComponent* in_pListener, AkUInt32 uDiffractionFlags) const
{
	// If the object is not already registered, do nothing here.
	CAkGameObject* pRoomGameObj = g_pRegistryMgr->GetObjAndAddref(GetID().AsGameObjectID());
	if (pRoomGameObj)
	{
		// Clear sends, except the "send to self"
		AkAuxSendValue sends;
		AkUInt32 uNumSends = GetSendToSelf(&sends);
		g_pRegistryMgr->SetGameObjectAuxSendValues(pRoomGameObj, &sends, uNumSends);

		// Set the transmission path occlusion - according to the WallOcclusion value - taken as the max value between the this room and the listener's room
		AkReal32 fWallOcclusion = AkMax(m_WallOcclusion, in_pListener->GetWallOcclusion());

		CAkSpatialAudioListener* pSpatialAudioListenerComponent = in_pListener->GetOwner()->GetComponent<CAkSpatialAudioListener>();
		CAkListener* pListenerListenerCmpt = in_pListener->GetOwner()->GetComponent<CAkListener>();
		const AkTransform& xfrm = pListenerListenerCmpt->GetTransform();
		Ak3DVector in_listenerPos = xfrm.Position();
		Ak3DVector in_listenerUp = xfrm.OrientationTop();

		AkUInt32 uNumSoundPositions = m_Links.Length();

		AkReal32* spread = (AkReal32*)alloca(uNumSoundPositions * sizeof(AkReal32));
		AkReal32* focus = (AkReal32*)alloca(uNumSoundPositions * sizeof(AkReal32));
		AkObstructionOcclusionValues* obsOcc = (AkObstructionOcclusionValues*)alloca(uNumSoundPositions * sizeof(AkObstructionOcclusionValues));
		AkChannelEmitter* soundPos = (AkChannelEmitter*)alloca(uNumSoundPositions * sizeof(AkChannelEmitter));

		for (AkUInt32 idx = 0; idx < m_Links.Length(); ++idx)
		{
			AkAcousticPortal* pPortal = (AkAcousticPortal*)m_Links[idx];

			pPortal->GetDiffractionPaths().Term();

			GetPortalPositionData(
				pPortal,
				pSpatialAudioListenerComponent->GetActiveRoom(),
				in_listenerPos,
				in_listenerUp,
				soundPos[idx],
				spread[idx],
				focus[idx],
				obsOcc[idx]
				);

			obsOcc[idx].occlusion = AkMax(obsOcc[idx].occlusion, fWallOcclusion);
		}

		UpdateRoomGameObjHelper(
			pRoomGameObj,
			in_pListener->ID(),
			in_listenerPos,
			false,
			obsOcc,
			soundPos,
			spread,
			focus,
			uNumSoundPositions,
			0.f,
			uDiffractionFlags
		);


		pRoomGameObj->Release();
	}
}

void AkAcousticRoom::SetP2PPath(AkPortalID in_portal0, AkPortalID in_portal1, CAkDiffractionPathSegments& paths)
{
	AkPortalToPortalInfo* vis = m_P2PPaths.Set(AkPortalPair(in_portal0, in_portal1));
	if (vis)
	{
		vis->paths.TransferPaths(paths); // take ownership of paths.

		if (vis->paths.HasDirectLineOfSight())
		{
			vis->pathDiffraction = 0.f;
			
			// Resolve IDs to pointers. 
			AkAcousticPortal *pPortal0 = NULL, *pPortal1 = NULL;
			for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
			{
				AkAcousticPortal* pPortal = (AkAcousticPortal*)*it;
				if (pPortal->GetID() == in_portal0)
					pPortal0 = pPortal;
				else if (pPortal->GetID() == in_portal1)
					pPortal1 = pPortal;
			}

			AKASSERT(pPortal0 != NULL && pPortal1 != NULL);
			vis->pathLength = (pPortal0->GetCenter() - pPortal1->GetCenter()).Length(); 
		}
		else if (vis->paths.IsOutOfRange())
		{
			vis->pathDiffraction = 1.f;
			vis->pathLength = FLT_MAX;
		}
		else
		{
			AkDiffractionPathSegment* pPath = vis->paths.ShortestPath();
			if (pPath != NULL)
			{
				vis->pathDiffraction = pPath->diffraction;
				vis->pathLength = pPath->totLength;
			}
			else
			{
				// No paths - assume not reachable.
				vis->pathDiffraction = 1.f;
				vis->pathLength = FLT_MAX;
			}
		}
	}
}

void AkAcousticRoom::ClearP2PPaths()
{
	m_P2PPaths.RemoveAll();
}

const CAkDiffractionPathSegments* AkAcousticRoom::GetP2PPaths(const AkAcousticPortal* in_portal0, const AkAcousticPortal* in_portal1) const
{
	AkPortalToPortalInfo* vis = m_P2PPaths.Exists(AkPortalPair(in_portal0->GetID(), in_portal1->GetID()));
	if (vis)
	{
		if (!vis->IsOutOfRange())
			return &vis->paths;
	}
	
	return NULL;
}

const AkDiffractionPathSegment* AkAcousticRoom::GetShortestP2PPath(const AkAcousticPortal* in_portal0, const AkAcousticPortal* in_portal1) const
{
	AkPortalToPortalInfo* vis = m_P2PPaths.Exists(AkPortalPair(in_portal0->GetID(), in_portal1->GetID()));
	if (vis)
	{
		if (!vis->IsOutOfRange())
			return vis->paths.ShortestPath();
	}

	return NULL;
}

bool AkAcousticRoom::GetP2PPathLength(const AkAcousticPortal* in_portal0, const AkAcousticPortal* in_portal1, AkReal32& out_fPathLength, AkReal32& out_fDiffraction)
{
	AkPortalToPortalInfo* vis = m_P2PPaths.Exists(AkPortalPair(in_portal0->GetID(),in_portal1->GetID()));
	if (vis)
	{
		out_fPathLength = vis->pathLength;
		out_fDiffraction = vis->pathDiffraction;
		return !vis->IsOutOfRange();
	}
	else
	{
		out_fPathLength = (in_portal0->GetCenter() - in_portal1->GetCenter()).Length(); // return euclidean distance if portals not found in map
		out_fDiffraction = 0.f;
		return true;
	}

}