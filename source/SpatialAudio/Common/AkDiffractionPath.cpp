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
#include "AkDiffractionPath.h"
#include "AkAcousticPortal.h"
#include "AkAcousticRoom.h"
#include "AkSpatialAudioCompts.h"
#include "AkSpatialAudioTasks.h"

AkReal32 AkDiffractionPath::sDfrnShadowRad = AK_DEFAULT_DIFFR_SHADOW_DEGREES * PI / 180.f;
AkReal32 AkDiffractionPath::sDfrnShadowAttenFactor = AK_DEFAULT_DIFFR_SHADOW_ATTEN;

void AkDiffractionPath::Init(Ak3DVector in_listenerPos)
{
	listenerPos = in_listenerPos;
}

void AkDiffractionPath::Append(const AkDiffractionPathSegment& in_path, const Ak3DVector& in_emitterPos, AkRoomID in_roomID)
{
	for (AkUInt32 i = 0; i < in_path.nodeCount && nodeCount + i < kMaxLength; ++i)
	{
		nodes[nodeCount + i] = in_path.nodes[i];
		rooms[nodeCount + i] = in_roomID;
	}

	emitterPos = in_emitterPos;
	nodeCount = AkMin(kMaxLength, nodeCount + in_path.nodeCount);
	obstructionValue = AkMax(in_path.obstructionValue, obstructionValue);
}

void AkDiffractionPath::AppendBw(const AkDiffractionPathSegment& in_path, const Ak3DVector& in_emitterPos, AkRoomID in_roomID)
{
	for (AkUInt32 i = 0; i < in_path.nodeCount && nodeCount + i < kMaxLength; ++i)
	{
		nodes[nodeCount + i] = in_path.nodes[in_path.nodeCount-i-1];
		rooms[nodeCount + i] = in_roomID;
	}

	emitterPos = in_emitterPos;
	nodeCount = AkMin(kMaxLength, nodeCount + in_path.nodeCount);
	obstructionValue = AkMax(in_path.obstructionValue, obstructionValue);
}

bool AkDiffractionPath::AppendNode(const Ak3DVector& in_position, AkRoomID in_room, AkPortalID in_portal)
{
	if (nodeCount < kMaxLength)
	{
		nodes[nodeCount] = in_position;
		rooms[nodeCount] = in_room;
		portals[nodeCount] = in_portal;
		
		++nodeCount;

		return true;
	}

	return false;
}

void AkDiffractionPath::AppendP2PPath(const AkDiffractionPathSegment& in_path, const AkAcousticPortal* in_fromPortal, const AkAcousticPortal* in_toPortal, AkRoomID in_fromRoomID, AkRoomID in_toRoomID)
{
	bool bReversePath = in_fromPortal->GetID() < in_toPortal->GetID();

	Ak3DVector pt0 = nodeCount > 0 ? nodes[nodeCount - 1] : listenerPos;
	
	Ak3DVector pt1 = in_path.nodeCount > 0 ? (bReversePath ? in_path.nodes[in_path.nodeCount - 1] : in_path.nodes[0]) : in_toPortal->GetCenter();

	Ak3DVector portalPos;
	in_fromPortal->CalcIntersectPoint(pt0, pt1, portalPos);

	AppendNode(portalPos, in_fromRoomID, in_fromPortal->GetID());

	if (bReversePath)
	{
		AppendBw(in_path, portalPos, in_toRoomID);
	}
	else
	{
		Append(in_path, portalPos, in_toRoomID);
	}
}

void AkDiffractionPath::Append(const AkPropagationPath& in_path, AkUInt32 in_startAtNode, const Ak3DVector& in_emitterPos, bool in_bUseP2PPaths)
{

	Ak3DVector ptPrev = nodeCount > 0 ? nodes[nodeCount-1] : listenerPos;
	
	AkUInt32 i = in_startAtNode;
 	for (; i < in_path.nodeCount-1; ++i)
	{
		const AkDiffractionPathSegment* shortest = in_bUseP2PPaths ? in_path.rooms[i + 1]->GetShortestP2PPath(in_path.portals[i], in_path.portals[i + 1]) : NULL;
		if (shortest != NULL)
		{
			AppendP2PPath(*shortest, in_path.portals[i], in_path.portals[i + 1], in_path.rooms[i]->GetID(), in_path.rooms[i + 1]->GetID());

		}
		else
		{
			Ak3DVector portalPos;
			in_path.portals[i]->CalcIntersectPoint(ptPrev, in_path.portals[i + 1]->GetCenter(), portalPos);
			
			AppendNode(portalPos, in_path.rooms[i]->GetID(), in_path.portals[i]->GetID());

			ptPrev = portalPos;
		}
	}

	Ak3DVector portalPos;
	in_path.portals[i]->CalcIntersectPoint(ptPrev, in_emitterPos, portalPos);

	AppendNode(portalPos, in_path.rooms[i]->GetID(), in_path.portals[i]->GetID());

	obstructionValue = AkMax(obstructionValue, in_path.GetPortalObstructionValue());

	emitterPos = in_emitterPos;

}

void IAkDiffractionPaths::AddDirectLineOfSightPath(const Ak3DVector& in_emitter, const Ak3DVector& in_listener)
{
	AkDiffractionPathSegment* path = AddPath();
	if (path)
	{
		path->nodeCount = 0;
		path->totLength = (in_listener - in_emitter).LengthSquared();
		path->emitterPos = in_emitter;
		path->listenerPos = in_listener;
	}

	m_hasDirectLineOfSight = true;
	m_outOfRange = false;
}

// Calculate diffraction of this path segment. Note: This version does not include the distance multiplier (sDfrnShadowAttenFactor).
void AkDiffractionPathSegment::CalcDiffraction()
{
	diffraction = 0.f;
	totLength = 0.f;

	Ak3DVector p0 = emitterPos;

	for (AkInt32 i = nodeCount - 1; i >= 0; --i)
	{
		Ak3DVector nodePos = nodes[i];
		Ak3DVector p1 = nodes[i - 1];

		Ak3DVector d0 = nodePos - p0;
		AkReal32 d0Length = d0.Length();

		Ak3DVector d1 = p1 - nodePos;
		AkReal32 d1Length = d1.Length();

		AkReal32 d = d0Length * d1Length;
		if (d > AK_SA_EPSILON)
		{
			totLength += d0Length;

			AkReal32 dot = d0.Dot(d1) / d;
			if (dot < 1.0f - AK_SA_EPSILON)
			{
				AkReal32 angle = AK::SpatialAudio::ACos(dot);
				Accumulate(angle);
			}

			p0 = nodePos;
		}
	}

	Ak3DVector d = p0 - listenerPos;
	AkReal32 seg_length = d.Length();
	if (seg_length > 0.f)
	{
		totLength += seg_length;
	}
}

void AkDiffractionPathSegment::Reverse()
{
	for (AkUInt32 i = 0; i < nodeCount / 2; ++i)
	{
		Ak3DVector& left = nodes[i];
		Ak3DVector& right = nodes[nodeCount - i - 1];

		Ak3DVector temp = left;
		left = right;
		right = temp;
	}

	Ak3DVector temp = listenerPos;
	listenerPos = emitterPos;
	emitterPos = temp;
	
}

void AkDiffractionPath::CalcDiffractionAndVirtualPos(const AkTransform& in_emitterTransform)
{
	Ak3DVector emitterFront = in_emitterTransform.OrientationFront();
	Ak3DVector emitterUp = in_emitterTransform.OrientationTop();

	totLength = 0.f;
	diffraction = 0.f;

	Ak3DVector p0 = emitterPos;

	for (AkInt32 i = nodeCount - 1; i >= 0; --i)
	{
		Ak3DVector nodePos = nodes[i];
		Ak3DVector p1 = nodes[i-1];

		Ak3DVector d0 = nodePos - p0;
		AkReal32 d0Length = d0.Length();

		Ak3DVector d1 = p1 - nodePos;
		AkReal32 d1Length = d1.Length();

		AkReal32 d = d0Length*d1Length;
		if (d > AK_SA_EPSILON)
		{
			totLength += d0Length * AkDiffractionPath::GetDistanceMultiplier(diffraction * PI);

			AkReal32 dot = d0.Dot(d1) / d;
			if (dot < 1.0f - AK_SA_EPSILON && 
				dot > -1.0f + AK_SA_EPSILON)
			{
				AkReal32 angle = AK::SpatialAudio::ACos(dot);
				angles[i] = angle;
				Accumulate(angle);
#ifdef AK_SA_USE_QUATERNIONS
				AkQuaternion rotation(d0 / d0Length, d1 / d1Length);
#else
				Ak3DVector cross = d0.Cross(d1) / d;
				AkReal32 sin = cross.Length();

				AkMatrix3x3 rotation;
				AkMatrix3x3::Rotation(rotation, sin, dot, sin > 0 ? cross / sin : Ak3DVector(0.f, 1.f, 0.f));
#endif
				Ak3DVector rd0 = rotation * d0;
				emitterFront = rotation * (d0 + emitterFront) - rd0;
				emitterUp = rotation * (d0 + emitterUp) - rd0;
			}

			p0 = nodePos;
		}
	}

	Ak3DVector d = p0 - listenerPos;
	AkReal32 seg_length = d.Length();
	if (seg_length > 0.f)
	{
		totLength += seg_length * AkDiffractionPath::GetDistanceMultiplier(diffraction * PI);

		Ak3DVector emitterPos = listenerPos + (d / seg_length) * totLength;
		virtualPos.Set(emitterPos, emitterFront, emitterUp);
	}
	else
	{
		// The emitter and listener are in exactly the same position.
		virtualPos = in_emitterTransform;
	}
}

void CAkDiffractionPaths::CalcDiffractionAndVirtualPos(const Ak3DVector& in_listenerPos, const AkTransform& in_emitterTransform)
{
	if (IsOutOfRange())
	{
		if (!IsEmpty())
		{
			AkDiffractionPath& path = *Begin();
			Ak3DVector d = path.emitterPos - path.listenerPos;
			AkReal32 seg_length = d.Length();
			if (seg_length > 0.f)
			{
				Ak3DVector vp = path.listenerPos + (d / seg_length) * path.totLength;
				path.virtualPos.Set(vp, in_emitterTransform.OrientationFront(), in_emitterTransform.OrientationTop());
			}
			else
			{
				// The emitter and listener are in exactly the same position.
				path.virtualPos = in_emitterTransform;
			}
		}
	}
	else
	{
		for (CAkDiffractionPaths::Iterator it = Begin(); it != End(); ++it)
		{
			(*it).listenerPos = in_listenerPos;
			(*it).CalcDiffractionAndVirtualPos(in_emitterTransform);
		}
	}
}

AkReal32 CAkDiffractionPaths::GetCombinedObstructionDiffraction() const
{
	if (!IsEmpty())
	{
		AkReal32 minObs = FLT_MAX;
		for (CAkDiffractionPaths::Iterator it = Begin(); it != End(); ++it)
		{
			AkReal32 obs = AkMax((*it).diffraction, (*it).obstructionValue);
			if (obs < minObs)
				minObs = obs;
		}
		return minObs;
	}
	else
	{
		return 0.f;
	}
}

AkReal32 CAkDiffractionPaths::GetMinDiffraction() const
{
	if (!IsEmpty())
	{
		AkReal32 minDiffraction = FLT_MAX;
		for (CAkDiffractionPaths::Iterator it = Begin(); it != End(); ++it)
		{
			if ((*it).diffraction < minDiffraction)
				minDiffraction = (*it).diffraction;
		}
		return minDiffraction;
	}
	else
	{
		return 0.f;
	}
}

AkReal32 CAkDiffractionPaths::GetMinObstruction() const
{
	if (!IsEmpty())
	{
		AkReal32 minObstruction = FLT_MAX;
		for (CAkDiffractionPaths::Iterator it = Begin(); it != End(); ++it)
		{
			if ((*it).obstructionValue < minObstruction)
				minObstruction = (*it).obstructionValue;
		}
		return minObstruction;
	}
	else
	{
		return 0.f;
	}
}

AkDiffractionPath* CAkDiffractionPaths::ShortestPath() const
{
	AkReal32 minLength = FLT_MAX;
	AkDiffractionPath* pShortest = NULL;
	for (CAkDiffractionPaths::Iterator it = Begin(); it != End(); ++it)
	{
		AKASSERT((*it).totLength > 0.f);
		if ((*it).totLength < minLength)
		{
			minLength = (*it).totLength;
			pShortest = &(*it);
		}
	}

	return pShortest;
}

void CAkDiffractionPathSegments::CalcDiffraction()
{
	for (CAkDiffractionPathSegments::Iterator it = Begin(); it != End(); ++it)
	{
		(*it).CalcDiffraction();
	}
	//Review: Sort by distance here?
}

AkDiffractionPathSegment* CAkDiffractionPathSegments::ShortestPath() const
{
	AkReal32 minLength = FLT_MAX;
	AkDiffractionPathSegment* pShortest = NULL;
	for (CAkDiffractionPathSegments::Iterator it = Begin(); it != End(); ++it)
	{
		AKASSERT((*it).totLength > 0.f);
		if ((*it).totLength < minLength)
		{
			minLength = (*it).totLength;
			pShortest = &(*it);
		}
	}

	return pShortest;
}

void CAkDiffractionPathSegments::ReversePaths()
{
	for (CAkDiffractionPathSegments::Iterator it = Begin(); it != End(); ++it)
	{
		(*it).Reverse();
	}
}

AkReal32 AkPropagationPath::GetPortalObstructionValue() const
{
	AkReal32 obstruction = 0.f;
	for (AkUInt32 i = 0; i < nodeCount; ++i)
	{
		obstruction = AkMax(obstruction, portals[i]->GetObstruction());
	}
	return obstruction;
}

void AkPropagationPath::EnqueuePortalTasks(
	const AkAcousticRoom* in_pEmitterRoom,
	AkSAPortalRayCastingTaskQueue& in_portalRaycastTasks,
	AkSAPortalToPortalTaskQueue& in_p2pTasks,
	bool& io_bAddedEmitterSidePortal
	) const
{
	AkAcousticPortal* pPortalPrev = NULL;
	bool bAddedPrev = false;
	
	for (AkUInt32 i = 0; i < nodeCount; ++i)
	{
		AkAcousticPortal* pPortal = portals[i];

		// pRoom it the room emitter-side of pPortal.
		const AkAcousticRoom* pRoom = (i + 1) < nodeCount ? rooms[i + 1] : in_pEmitterRoom;

		bool bAdded = pPortal->EnqueueTaskIfNeeded(pRoom, in_portalRaycastTasks);

		if (pPortalPrev != nullptr && (bAdded || bAddedPrev) )
		{
			AkSAPortalTaskData task = AkSAPortalTaskData::PortalTask(pPortal->GetID(), pPortalPrev->GetID());
			in_p2pTasks.Set(task);
		}

		bAddedPrev = bAdded;
		pPortalPrev = pPortal;
	}

	// Triggering new rays on the portal closest to the emitter will potentially add new paths to the emitter.
	if (bAddedPrev)
	{
		io_bAddedEmitterSidePortal = true;
	}
}

void AkPropagationPathArray::BuildDiffractionPaths(	const Ak3DVector& in_listenerPos, AkRoomID in_listenerRoomID,
													const AkTransform& in_emitterPos, AkRoomID in_emitterRoomID, 
													CAkDiffractionPaths& out_finalPaths) const
{
	out_finalPaths.RemoveAll();

	for (AkUInt32 i = 0; i < Length(); ++i)
	{
		AkPropagationPath& propPath = (*this)[i];
		AkDiffractionPath* pNewPath = out_finalPaths.AddLast();
		if (pNewPath != NULL)
		{
			pNewPath->Init(in_listenerPos);
			pNewPath->Append(propPath, 0, in_emitterPos.Position(), false);
			pNewPath->CalcDiffractionAndVirtualPos(in_emitterPos);
		}
	}
}

void AkPropagationPathArray::BuildDiffractionPaths(	const Ak3DVector& in_listenerPos, const AkPathsToPortals& in_toListener, AkRoomID in_listenerRoomID,
													const AkTransform& in_emitterPos, const AkPathsToPortals& in_fromEmitter, AkRoomID in_emitterRoomID,
													AkReal32 in_maxDistance,
													CAkDiffractionPaths& out_finalPaths) const
{
	out_finalPaths.RemoveAll();
	out_finalPaths.SetHasDirectLineOfSight(false);
	out_finalPaths.SetOutOfRange(false);

	for (AkUInt32 i = 0; i < Length(); ++i)
	{
		AkPropagationPath& propPath = (*this)[i];
		
		PortalPathsStruct* ppsEmtr = in_fromEmitter.Exists(propPath.EmitterSidePortal()->GetID());
		if (ppsEmtr == NULL)
			continue; // if the portal is not in the map, the sound is unreachable (see AkAcousticRoom::InitContributingPortalPaths())

		CAkDiffractionPathSegments* pEmitterSidePaths = &ppsEmtr->diffractions;
		if (pEmitterSidePaths->IsEmpty() || pEmitterSidePaths->IsOutOfRange())
			continue;

		PortalPathsStruct* ppsLstnr = in_toListener.Exists(propPath.ListenerSidePortal()->GetID());
		if (ppsLstnr == NULL)
			continue;

		CAkDiffractionPathSegments* pListenerSidePaths = &ppsLstnr->diffractions;
		if (pListenerSidePaths->IsEmpty() || pListenerSidePaths->IsOutOfRange())
			continue;
		
		if (propPath.nodeCount > 1 ) // use multiple p2p paths, but just the shortest emitter-side path.
		{
			for (CAkDiffractionPathSegments::Iterator itLsp = pListenerSidePaths->Begin(); itLsp != pListenerSidePaths->End(); ++itLsp)
			{
				AkDiffractionPathSegment* pEsp = pEmitterSidePaths->ShortestPath();

				const AkAcousticRoom* pRoom = propPath.rooms[1]; // room behind first portal.
				const CAkDiffractionPathSegments* pP2PPaths = pRoom->GetP2PPaths(propPath.portals[0], propPath.portals[1]);
				if (pP2PPaths != NULL)
				{
					for (CAkDiffractionPathSegments::Iterator p2pIt = pP2PPaths->Begin(); p2pIt != pP2PPaths->End(); ++p2pIt)
					{
						AkDiffractionPath* pNewPath = out_finalPaths.AddLast();
						if (pNewPath != NULL)
						{
							pNewPath->Init(in_listenerPos);
							pNewPath->Append(*itLsp, (*itLsp).emitterPos, in_listenerRoomID);
							pNewPath->AppendP2PPath(*p2pIt, propPath.portals[0], propPath.portals[1], propPath.rooms[0]->GetID(), propPath.rooms[1]->GetID());
							pNewPath->Append(propPath, 1, in_emitterPos.Position(), true);
							pNewPath->Append(*pEsp, (*pEsp).emitterPos, in_emitterRoomID);
							pNewPath->CalcDiffractionAndVirtualPos(in_emitterPos);
							if (pNewPath->diffraction < AK_SA_EPSILON)
								out_finalPaths.SetHasDirectLineOfSight(true);
						}
					}
				}
				else
				{
					AkDiffractionPath* pNewPath = out_finalPaths.AddLast();
					if (pNewPath != NULL)
					{
						pNewPath->Init(in_listenerPos);
						pNewPath->Append(*itLsp, (*itLsp).emitterPos, in_listenerRoomID);
						pNewPath->Append(propPath, 0, in_emitterPos.Position(), false);
						pNewPath->Append(*pEsp, (*pEsp).emitterPos, in_emitterRoomID);
						pNewPath->CalcDiffractionAndVirtualPos(in_emitterPos);
						if (pNewPath->diffraction < AK_SA_EPSILON)
							out_finalPaths.SetHasDirectLineOfSight(true);
					}
				}
			}
		}
		else // use multiple emitter-side paths
		{
			for (CAkDiffractionPathSegments::Iterator itLsp = pListenerSidePaths->Begin(); itLsp != pListenerSidePaths->End(); ++itLsp)
			{
				for (CAkDiffractionPathSegments::Iterator itEsp = pEmitterSidePaths->Begin(); itEsp != pEmitterSidePaths->End(); ++itEsp)
				{
					AkDiffractionPath* pNewPath = out_finalPaths.AddLast();
					if (pNewPath != NULL)
					{
						pNewPath->Init(in_listenerPos);
						pNewPath->Append(*itLsp, (*itLsp).emitterPos, in_listenerRoomID);
						pNewPath->Append(propPath, 0, (*itEsp).nodeCount > 0 ? (*itEsp).nodes[0] : (*itEsp).emitterPos, true);
						pNewPath->Append(*itEsp, (*itEsp).emitterPos, in_emitterRoomID);
						pNewPath->CalcDiffractionAndVirtualPos(in_emitterPos);
						if (pNewPath->diffraction < AK_SA_EPSILON)
							out_finalPaths.SetHasDirectLineOfSight(true);
					}
				}
			}
		}
	}

	if (!IsEmpty() && out_finalPaths.IsEmpty())
	{
		out_finalPaths.SetOutOfRange(true);
		out_finalPaths.CalcDiffractionAndVirtualPos(in_listenerPos, in_emitterPos);
	}
}

void AkPropagationPathArray::BuildPortalDiffractionPaths(
	const Ak3DVector& in_listenerPos, 
	const AkPathsToPortals& in_toListener, 
	AkRoomID in_listenerRoomID,
	const AkTransform& in_emitterPos, 
	AkPortalID in_portalID, 
	AkRoomID in_emitterRoomID,
	AkReal32 in_maxDistance,
	CAkDiffractionPaths& out_finalPaths) const
{
	out_finalPaths.RemoveAll();
	out_finalPaths.SetHasDirectLineOfSight(false);
	out_finalPaths.SetOutOfRange(false);

	for (AkUInt32 i = 0; i < Length(); ++i)
	{
		AkPropagationPath& propPath = (*this)[i];

		if (propPath.EmitterSidePortal()->GetID() != in_portalID ||	// path doesn't start with correct portal
			propPath.rooms[propPath.nodeCount - 1]->GetID() == in_emitterRoomID)  // path goes wrong way through portal
		{
			continue;
		}

		PortalPathsStruct* ppsLstnr = in_toListener.Exists(propPath.ListenerSidePortal()->GetID());
		if (ppsLstnr == NULL)
		{
			continue; // listener can not reach this path
		}

		CAkDiffractionPathSegments* pListenerSidePaths = &ppsLstnr->diffractions;
		if (pListenerSidePaths->IsEmpty() || pListenerSidePaths->IsOutOfRange())
		{
			continue;
		}

		if (propPath.nodeCount > 1) // use multiple p2p paths
		{
			for (CAkDiffractionPathSegments::Iterator itLsp = pListenerSidePaths->Begin(); itLsp != pListenerSidePaths->End(); ++itLsp)
			{
				const AkAcousticRoom* pRoom = propPath.rooms[1]; // room behind first portal.
				const CAkDiffractionPathSegments* pP2PPaths = pRoom->GetP2PPaths(propPath.portals[0], propPath.portals[1]);
				if (pP2PPaths != NULL)
				{
					for (CAkDiffractionPathSegments::Iterator p2pIt = pP2PPaths->Begin(); p2pIt != pP2PPaths->End(); ++p2pIt)
					{
						AkDiffractionPath* pNewPath = out_finalPaths.AddLast();
						if (pNewPath != NULL)
						{
							pNewPath->Init(in_listenerPos);
							pNewPath->Append(*itLsp, (*itLsp).emitterPos, in_listenerRoomID);
							pNewPath->AppendP2PPath(*p2pIt, propPath.portals[0], propPath.portals[1], propPath.rooms[0]->GetID(), propPath.rooms[1]->GetID());
							pNewPath->Append(propPath, 1, in_emitterPos.Position(), true);
							pNewPath->CalcDiffractionAndVirtualPos(in_emitterPos);
							if (pNewPath->diffraction < AK_SA_EPSILON)
								out_finalPaths.SetHasDirectLineOfSight(true);
						}
					}
				}
				else
				{
					AkDiffractionPath* pNewPath = out_finalPaths.AddLast();
					if (pNewPath != NULL)
					{
						pNewPath->Init(in_listenerPos);
						pNewPath->Append(*itLsp, (*itLsp).emitterPos, in_listenerRoomID);
						pNewPath->Append(propPath, 0, in_emitterPos.Position(), false);
						pNewPath->CalcDiffractionAndVirtualPos(in_emitterPos);
						if (pNewPath->diffraction < AK_SA_EPSILON)
							out_finalPaths.SetHasDirectLineOfSight(true);
					}
				}
			}
		}
		else 
		{
			for (CAkDiffractionPathSegments::Iterator itLsp = pListenerSidePaths->Begin(); itLsp != pListenerSidePaths->End(); ++itLsp)
			{
				AkDiffractionPath* pNewPath = out_finalPaths.AddLast();
				if (pNewPath != NULL)
				{
					pNewPath->Init(in_listenerPos);
					pNewPath->Append(*itLsp, (*itLsp).emitterPos, in_listenerRoomID);
					pNewPath->Append(propPath, 0, in_emitterPos.Position(), true);
					pNewPath->CalcDiffractionAndVirtualPos(in_emitterPos);
					if (pNewPath->diffraction < AK_SA_EPSILON)
						out_finalPaths.SetHasDirectLineOfSight(true);
				}
			}
		}
	}

	if (!IsEmpty() && out_finalPaths.IsEmpty())
	{
		out_finalPaths.SetOutOfRange(true);
		out_finalPaths.CalcDiffractionAndVirtualPos(in_listenerPos, in_emitterPos);
	}
}

void AkPropagationPathArray::BuildReflectionPaths(
	const CAkSpatialAudioListener* in_pListener,
	const CAkSpatialAudioEmitter* in_pEmitter,
	CAkReflectionPaths& out_finalPaths
) const
{
	for (AkPropagationPathArray::Iterator it = Begin(); it != End(); ++it)
	{
		AkPropagationPath& propagationPath = *it;
		if (propagationPath.length >= in_pEmitter->GetMaxDistance() )
		{
			continue;
		}

		AkAcousticPortal* pPortal = propagationPath.portals[0];

		PortalPathsStruct* pps = in_pListener->m_PathsToPortals.Exists(propagationPath.ListenerSidePortal()->GetID());
		if (pps == NULL || pps->diffractions.IsOutOfRange() || pps->diffractions.IsEmpty())
		{
			continue;
		}

		PortalPathsStruct* emitter_pps = in_pEmitter->m_PathsToPortals.Exists(propagationPath.EmitterSidePortal()->GetID());
		if (emitter_pps == NULL)
		{
			continue;
		}

		CAkReflectionPaths& reflectionPaths = emitter_pps->reflections;

		for (AkUInt32 idxReflectionPath = 0; idxReflectionPath < reflectionPaths.Length(); ++idxReflectionPath)
		{
			bool bPathValid = true;
			AkReflectionPath reflectionPath = reflectionPaths[idxReflectionPath]; // making a copy on the stack.

			// Add diffraction from the sound propagation path (not yet including the portal closest to the listener)
			for (AkUInt32 propPathIdx = propagationPath.nodeCount - 1; bPathValid && propPathIdx > 0; --propPathIdx)
			{
				const AkDiffractionPathSegment* shortest = propagationPath.rooms[propPathIdx]->GetShortestP2PPath(propagationPath.portals[propPathIdx - 1], propagationPath.portals[propPathIdx]);
				if (shortest != NULL && shortest->nodeCount != 0)
				{
					bool bReverse = propagationPath.portals[propPathIdx - 1]->GetID() < propagationPath.portals[propPathIdx]->GetID();
					Ak3DVector ptListenerSide = propagationPath.portals[propPathIdx - 1]->GetCenter();
					bPathValid = reflectionPath.AppendPathSegment(propagationPath.portals[propPathIdx], *shortest, ptListenerSide, bReverse);
				}
				else
				{
					Ak3DVector portalPos;
					Ak3DVector ptListenerSide = propagationPath.portals[propPathIdx - 1]->GetCenter();
					propagationPath.portals[propPathIdx]->CalcIntersectPoint(reflectionPath.PointClosestToListener(), ptListenerSide, portalPos);

					bPathValid = reflectionPath.AddPoint(portalPos, ptListenerSide);
				}
			}

			if (bPathValid)
			{
				// Add diffraction from the diffraction path segments, also including the portal closest to the listener. 

				const CAkDiffractionPathSegments& diffractionPaths = pps->diffractions;
				for (CAkDiffractionPathSegments::Iterator it = diffractionPaths.Begin(); it != diffractionPaths.End(); ++it)
				{
					AkDiffractionPathSegment& diffractionPath = *it;

					AkReflectionPath reflectionPathWithDiffraction = reflectionPath; // make a copy on the stack
					if (reflectionPathWithDiffraction.AppendPathSegment(pPortal, diffractionPath, in_pListener->GetPosition(), false))
					{
						out_finalPaths.AddLast(reflectionPathWithDiffraction);
					}
				}
				
			}
		}
	}
}
