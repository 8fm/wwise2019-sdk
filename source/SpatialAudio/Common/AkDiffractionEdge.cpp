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
#include "AkDiffractionEdge.h"
#include "AkDiffractionPath.h"
#include "AkImageSource.h"
#include "AkOcclusionChecker.h"
#include "AkGeometrySet.h"
#include "AkImageSource.h"
#include "AkStochasticPath.h"
#include "AkScene.h"

#include <AK/Tools//Common/AkAutoLock.h>

enum Zone
{
	OcclusionZone = -2,
	ViewZone = -1,
	ShadowZone0 = 0,
	ShadowZone1 = 1,
};

AkForceInline AkHashType AkHash(const CAkDiffractionEdge* in_key) { return (AkHashType)(AkUIntPtr)in_key; }

CAkLock CAkDiffractionEdge::edgeVisibilityLock;

void CAkDiffractionEdge::Init(const Ak3DVector& in_start, const Ak3DVector& in_end, const Ak3DVector& in_pt, AkImageSourceTriangle* in_surf)
{
	AKASSERT(in_surf->pGeoSet == pGeoSet);

	start = in_start;
	direction = in_end - start;
	length = direction.Length();
	direction = direction / length;
	
	surf0 = in_surf;
	surf1 = in_surf;

	Ak3DVector v0 = PointToEdgeVec(in_pt);
	v0.Normalize();
	n0 = direction.Cross(v0);
	n1 = v0.Cross(direction);
}

// Init edge from a triangle and edge index.
void CAkDiffractionEdge::Init(const Ak3DVector& in_start, const Ak3DVector& in_end, const Ak3DVector& in_p0, AkImageSourceTriangle* in_surf0, const Ak3DVector& in_p1, AkImageSourceTriangle* in_surf1)
{
	AKASSERT(in_surf0->pGeoSet == pGeoSet);
	AKASSERT(in_surf1->pGeoSet == pGeoSet);

	start = in_start;
	direction = in_end - start;
	length = direction.Length();
	direction = direction / length;

	Ak3DVector v0 = PointToEdgeVec(in_p0);
	v0.Normalize();
		
	Ak3DVector v1 = PointToEdgeVec(in_p1);
	v1.Normalize();

	surf0 = in_surf0;
	surf1 = in_surf1;

	// Standardize direction vector wrt v0,v1 angles.
	if (v0 != v1)
	{
		Ak3DVector dir = v1.Cross(v0);
		if (dir.Dot(direction) < 0)
		{
			//swap v0,v1
			Ak3DVector temp = v0;
			v0 = v1;
			v1 = temp;

			surf0 = in_surf1;
			surf1 = in_surf0;
		}
	}
	
	n0 = direction.Cross(v0);
	n1 = v1.Cross(direction);
}

//Takes a point in world space, and returns the vector from 'start' to 'in_pt', 
//projected onto the edge's rotation plane (about the edge's 'direction' vector)
Ak3DVector CAkDiffractionEdge::PointToEdgeVec(const Ak3DVector& in_pt) const
{
	Ak3DVector toPt = in_pt - start;
	AkReal32 d = toPt.Dot(direction);
	return (toPt - direction * d);
}

AkInt32 CAkDiffractionEdge::GetZone(const Ak3DVector& in_pt) const
{
	static const float kZoneEpsilon = 0.0001f;

	Ak3DVector toPt = in_pt - start;

	AkReal32 ptdotn0 = toPt.Dot(n0);
	AkReal32 ptdotn1 = toPt.Dot(n1);

	if (ptdotn0 > -kZoneEpsilon && ptdotn1 < kZoneEpsilon)
		return ShadowZone0;

	if (ptdotn1 > -kZoneEpsilon && ptdotn0 < kZoneEpsilon)
		return ShadowZone1;

	if (ptdotn0 > kZoneEpsilon && ptdotn1 > kZoneEpsilon)
		return ViewZone; // view zone

	return OcclusionZone;
}

void CAkDiffractionEdge::EdgeEdgeShadowVis(
	CAkDiffractionEdge& edge,
	const AkTriangleIndex& in_tris,
	bool& out_bVisible,
	AkInt8& out_startZone0, AkInt8& out_endZone0,
	AkInt8& out_startZone1, AkInt8& out_endZone1)
{
	AKASSERT(&edge != this);

	// By bumping the test points by a little in the direction of each normal, we make the zone test a more robust.  
	Ak3DVector offset = n0 * AK_SA_EPSILON + n1 * AK_SA_EPSILON +
		edge.n0 * AK_SA_EPSILON + edge.n1 * AK_SA_EPSILON;

	Ak3DVector ptE1start = edge.start + offset;
	Ak3DVector ptE1end = edge.start + edge.direction * edge.length + offset;

	AkInt32 startZone0 = GetZone(ptE1start);
	AkInt32 endZone0 = GetZone(ptE1end);

	if (startZone0 >= 0 || endZone0 >= 0)
	{
		Ak3DVector ptE0start = start + offset;
		Ak3DVector ptE0end = start + direction * length + offset;

		AkInt32 startZone1 = edge.GetZone(ptE0start);
		AkInt32 endZone1 = edge.GetZone(ptE0end);

		if (startZone1 >= 0 || endZone1 >= 0)
		{
			out_bVisible = true;
			out_startZone0 = (AkInt8)startZone0;
			out_endZone0 = (AkInt8)endZone0;
			out_startZone1 = (AkInt8)startZone1;
			out_endZone1 = (AkInt8)endZone1;
			
			return;
		}
	}

	out_bVisible = false;
}

void CAkDiffractionEdge::ComputeVisibility()
{
	if (edges0.IsComputed() && edges1.IsComputed())
	{
		return;
	}

	// Retrieve all the geometry sets and parse them
	AkScene* scene = pGeoSet->pScene;
	AkGeometrySetList& geometrySet = scene->GetGeometrySetList();
	
	AkGeometrySetList::Iterator itSet;
	itSet = geometrySet.Begin();

	while (itSet != geometrySet.End())
	{
		AkGeometrySet* set = (*itSet);

		for (AkInt32 i = 0; i < (AkInt32)(set->numEdges); ++i)
		{
			CAkDiffractionEdge& edge = set->edges[i];
			if (&edge != this)
			{
				bool bVisible = false;
				AkInt8 startZone0;
				AkInt8 endZone0;
				AkInt8 startZone1;
				AkInt8 endZone1;

				// Compute the edge to edge visibility for shadow zone only
				EdgeEdgeShadowVis(edge, scene->GetTriangleIndex(), bVisible, startZone0, endZone0, startZone1, endZone1);
				if (bVisible)
				{
					InsertVisibleEdge(edge, startZone0, endZone0);
				}
			}
		}

		++itSet;
	}

	// We compute the visibility map for this edge
	edges0.MarkAsComputed();
	edges1.MarkAsComputed();
}

bool CAkDiffractionEdge::InShadowZone(const Ak3DVector& emitterPos, const Ak3DVector& listenerPos) const
{
	Ak3DVector to_emitter = emitterPos - start;
	Ak3DVector to_listener = listenerPos - start;

	AkReal32 e_dot_n0 = to_emitter.Dot(n0);
	AkReal32 e_dot_n1 = to_emitter.Dot(n1);
	AkReal32 l_dot_n0 = to_listener.Dot(n0);
	AkReal32 l_dot_n1 = to_listener.Dot(n1);

	bool emitter_front_of_plane0 = e_dot_n0 > AK_SA_EPSILON;
	bool emitter_front_of_plane1 = e_dot_n1 > AK_SA_EPSILON;
	bool listener_front_of_plane0 = l_dot_n0 > AK_SA_EPSILON;
	bool listener_front_of_plane1 = l_dot_n1 > AK_SA_EPSILON;

	// The listener and the emitter must be on opposite sides of each plane.
	if (listener_front_of_plane0 == emitter_front_of_plane0 ||
		listener_front_of_plane1 == emitter_front_of_plane1)
		return false;

	// The emitter and listener must each be in front of one plane, and behind the other. 
	if (emitter_front_of_plane0 == emitter_front_of_plane1 ||
		listener_front_of_plane0 == listener_front_of_plane1)
		return false;

	// Project to_emitter and to_listener onto the plane defined by edge.origin and edge.direction.    
	// This is the plane that is perpendicular to the edge direction.
	Ak3DVector to_emitter_proj =
		emitterPos - direction * to_emitter.Dot(direction) - start;
	to_emitter_proj.Normalize();

	Ak3DVector to_listener_proj =
		listenerPos - direction * to_listener.Dot(direction) - start;
	to_listener_proj.Normalize();

	// p is the vector that is parallel to the plane with normal n0, pointing away from the edge.
	Ak3DVector p = n0.Cross(direction);

	// Project to_emitter_proj, and to_listener_proj along p so that we may compare their angles.
	AkReal32 a0 = to_emitter_proj.Dot(p);
	AkReal32 a1 = to_listener_proj.Dot(p);

	if (-a0 < a1)// The listener is in the diffraction shadow zone
		return true;

	return false;
}

bool CAkDiffractionEdge::InShadowZoneQuick(const Ak3DVector& in_pt0, const Ak3DVector& in_pt1) const
{
	Ak3DVector cross = n0.Cross(direction);
	Ak3DVector toPt0 = PointToEdgeVec(in_pt0);
	Ak3DVector toPt1 = PointToEdgeVec(in_pt1);
	AkReal32 d0 = toPt0.Dot(cross) / toPt0.Length();
	AkReal32 d1 = toPt1.Dot(cross) / toPt1.Length();
	return -d0 < d1;
}

bool CAkDiffractionEdge::InShadowZoneQuick_vec(const Ak3DVector& in_incident, const Ak3DVector& in_diffracted) const
{
	Ak3DVector cross = n0.Cross(direction);

	Ak3DVector toPt0 = (in_incident - direction * in_incident.Dot(direction));
	Ak3DVector toPt1 = (direction * in_diffracted.Dot(direction) - in_diffracted);

	AkReal32 d0 = toPt0.Dot(cross) / toPt0.Length();
	AkReal32 d1 = toPt1.Dot(cross) / toPt1.Length();

	return -d0 < d1;
}

// Computes closest point on the edge to the line segment (ray) defined by in_origin and in_direction,
// if the closest point is clamped to the end of the edge, then return false.
// out_diffractionPt is only valid when function returns true.
bool CAkDiffractionEdge::FindDiffractionPt(	
	const Ak3DVector& in_origin, 
	const Ak3DVector& in_direction, 
	Ak3DVector& out_diffractionPt
) const
{
	
	Ak3DVector d = direction * length;
	Ak3DVector r = start - in_origin;

	AkReal32 a = length * length; // Squared length of edge, always nonnegative 
	if (a <= AK_SA_EPSILON)
	{
		// The edge is degenerate - a point.
		return false;
	}
	else 
	{
		AkReal32 s; //parameterizes the diffraction point on the edge

		AkReal32 e = in_direction.Dot(in_direction); // Squared length of emitter-listener ray.
		AkReal32 c = d.Dot(r);

		if (e <= AK_SA_EPSILON)
		{ 
			// Emitter and listener are coincident.
			// Diffraction point is the point on the edge closest to the emitter/listener point.
			s = -c / a;
		} 
		else 
		{ 
			// The general nondegenerate case starts here 

			AkReal32 f = in_direction.Dot(r);
			AkReal32 b = d.Dot(in_direction);
			AkReal32 denom = a*e-b*b; 
		
			// Always nonnegative
			if (denom != 0.0f) 
			{
				// If segments not parallel, compute closest point on L1 to L2 and 
				// clamp to segment S1. 
				s = (b*f - c*e) / denom;
			} 
			else
			{
				return false; // line segments are parallel
			}

			// t parameterizes the point on in_direction 
			AkReal32 t = (b*s + f) / e;
			if (t < 0.0f) 
			{ 
				s = -c / a;
			} 
			else if (t > 1.0f) 
			{ 
				s = (b - c) / a;
			}
		}

		if (s > 0.f && s < 1.f)
		{
			// return a valid diffraction point if the closest point on the edge is not at one of the ends.
			out_diffractionPt = start + d * s;
			return true;
		}
	}

	return false;
}


AkReal32 CAkDiffractionEdge::SqDistPointSegment(const Ak3DVector& c) const 
{
	Ak3DVector ab = direction * length;
	Ak3DVector ac = c - start;
	Ak3DVector bc = c - (start + direction * length);
	
	// Handle cases where c projects outside ab 
	AkReal32 e = ac.Dot(ab);
	if (e <= 0.0f) 
		return ac.Dot(ac);

	AkReal32 f = ab.Dot(ab);
	if (e >= f) 
		return bc.Dot(bc);
	
	// Handle cases where c projects onto ab
	return ac.Dot(ac) - e * e / f;
}

void CAkDiffractionEdge::ClearVisibleEdges()
{	
	edges0.RemoveAll();
	edges0.Unvalidate();
	edges1.RemoveAll();
	edges1.Unvalidate();
}

void CAkDiffractionEdge::ClearVisibleEdges(AkGeometrySet* in_pInSet)
{
	for (AkUInt32 i = 0; i < in_pInSet->numEdges; ++i)
	{
		CAkDiffractionEdge& edge = in_pInSet->edges[i];
		edge.ClearAllVisSets();
	}
}

void CAkDiffractionEdge::InsertVisibleEdge(const CAkDiffractionEdge& in_Edge, AkInt32 startZone, AkInt32 endZone)
{
	if (startZone == 0 || endZone == 0)
		InsertEdge(edges0, in_Edge);
		 	
	if (startZone == 1 || endZone == 1)
		InsertEdge(edges1, in_Edge);
}

void CAkDiffractionEdge::InsertEdge(AkEdgeVisibility& array, const CAkDiffractionEdge& in_Edge)
{
	// Insert edge such that they are sorted by decreasing distance.

	AkReal32 distSqr0 = (start - in_Edge.start).LengthSquared();
	AkUInt32 idx = 0;
	while (idx < array.Length())
	{
		CAkDiffractionEdge& edge1 = *(array[idx].GetDiffractionEdge());
		AkReal32 distSqr1 = (start - edge1.start).LengthSquared();
		if (distSqr0 < distSqr1)
			break;

		++idx;
	}

	AkEdgeVisibility::Iterator end = array.End();
	AkEdgeVisibility::Iterator it;
	for (it = array.Begin(); it != end; ++it)
	{
		if ((*it).GetDiffractionEdge() == &in_Edge)
		{
			return;
		}
	}

	AKASSERT(!in_Edge.isPortalEdge); // portal edges should not be included in the visibility graph.
	AKASSERT(in_Edge.pGeoSet->pScene == this->pGeoSet->pScene); // edges must be in the same scene.

	AkEdgeLink* link = array.Insert(idx);
	if (link != NULL)
	{
		*link = AkEdgeLink(const_cast<CAkDiffractionEdge*>(&in_Edge));
	}
}

void CAkDiffractionEdge::ClearAllVisSets()
{
	edges0.Term();
	edges0.Unvalidate();

	edges1.Term();
	edges1.Unvalidate();
}

void ComputeDiffraction(
	const Ak3DVector& in_emitter,
	const AkEdgeArray& in_emitterEdges,
	const Ak3DVector& in_listener,
	const AkEdgeArray& in_listenerEdges,
	const AkTriangleIndex& in_triangles,
	IAkDiffractionPaths& out_diffractionResults,
	AkUInt32 in_uMaxDegree, AkUInt32 in_uMaxPaths, AkReal32 in_fMaxPathLength)
{
	AKASSERT(in_uMaxDegree > 0 && in_uMaxPaths > 0);

	bool bIsOccluded = false;

	AkReal32 distSqr = (in_emitter - in_listener).LengthSquared();
	if (distSqr > 0.f)
	{
		if (distSqr < in_fMaxPathLength*in_fMaxPathLength)
		{
			AKSIMD_V4F32 emitter = in_emitter.PointV4F32();
			AKSIMD_V4F32 listener = in_listener.PointV4F32();

			const AKSIMD_V4F32 dir = AKSIMD_SUB_V4F32(listener, emitter);
			NoFilter nf;
			OcclusionChecker<NoFilter> checker(emitter, dir, nf);

			in_triangles.RaySearch(emitter, dir, checker);

			bIsOccluded = checker.IsOccluded();

			if (bIsOccluded)
			{
				CAkEdgePathSearch search;
				search.Search(in_listener, in_listenerEdges, in_emitter, in_emitterEdges, out_diffractionResults, in_uMaxDegree, in_uMaxPaths, in_fMaxPathLength);
			}
		}
	}
	else
	{
		bIsOccluded = false; // emitter and listener in same position
	}

	if (bIsOccluded && out_diffractionResults.NumPaths() == 0)
	{
		//Could not find a search path, or the sound is out of range. Add a default path.

		AkDiffractionPathSegment* path = out_diffractionResults.AddPath();
		if (path)
		{
			path->diffraction = kMaxDiffraction;
			path->nodeCount = 0;
			path->totLength = in_fMaxPathLength;
			path->emitterPos = in_emitter;
			path->listenerPos = in_listener;
		}
	}
}

AkReal32 CostFcn( AkReal32 in_pathLength, AkReal32 in_maxPathLength, AkReal32 in_diffraction )
{
	static const AkReal32 kWeightDiffraction = 2.f;
	static const AkReal32 kWeightDistance = 1.f;
	AkReal32 distanceFactor = (AkMin(in_pathLength, in_maxPathLength) / in_maxPathLength);
	AkReal32 diffractionFactor = (in_diffraction / kMaxDiffraction);
	return (distanceFactor * kWeightDistance + diffractionFactor * kWeightDiffraction) / (kWeightDistance + kWeightDiffraction);
}

void CAkEdgePathSearch::Search(
	const Ak3DVector& in_fromListener, 
	const AkEdgeArray& in_listenerEdges, 
	const Ak3DVector& in_toEmitter,
	const AkEdgeArray& in_emitterEdges, 
	IAkDiffractionPaths& out_results, 
	AkUInt32 in_uMaxDegree,
	AkUInt32 in_uMaxPaths,
	AkReal32 in_fMaxPathLength)
{
	AKASSERT(in_uMaxDegree > 0 && in_uMaxPaths > 0);

	m_fMaxPathLength = in_fMaxPathLength;

	AkUInt8 pathIdx = 0;
	bool bDone;

	do 
	{
		bDone = true;

		for (AkEdgeArray::Iterator it = in_listenerEdges.Begin(); it != in_listenerEdges.End(); ++it)
			EvalEdge(*it, NULL, in_fromListener, in_toEmitter, pathIdx, in_fMaxPathLength);

		while (!m_OpenSet.IsEmpty())
		{
			EdgeData* pCur = Pop();

			pCur->closedForPath = pathIdx;

			const CAkDiffractionEdge* pCurEdge = pCur->pEdge;

			AkInt32 to_zone = pCurEdge->GetZone(in_toEmitter);

			if (to_zone >= 0 && 
				to_zone != pCur->fromZone
				)
			{
				const CAkDiffractionEdge* pEdge = pCur->pEdge;
				
				//Check if the emitter is visible from this edge.
				if (in_emitterEdges.Exists(const_cast<CAkDiffractionEdge*>(pEdge)) != NULL)
				{
					if (pCurEdge->InShadowZoneQuick(pCur->pPrevEdgeInPath == NULL ? in_fromListener : pCur->pPrevEdgeInPath->edgePt, in_toEmitter))
					{
						// found path.  Add result.
						AddResult(pCur, in_fromListener, in_toEmitter, out_results);
						ClearOpenList();
						bDone = (out_results.NumPaths() == in_uMaxPaths);
						break;
					}
					else
					{
						// The emitter is visible from here, but we are not in the shadow zone.  Deeper paths won't be valid, so stop searching.
						continue;
					}
				}
			}
			
			if (pCur->nodeCount < (AkInt32)in_uMaxDegree)
			{
				const AkEdgeVisibility& edges = pCur->fromZone == 0 ? pCurEdge->edges1 : pCurEdge->edges0;
				for (AkEdgeVisibility::Iterator it = edges.Begin(); it != edges.End(); ++it)
				{
					EvalEdge((*it).GetDiffractionEdge(), pCur, in_fromListener, in_toEmitter, pathIdx, in_fMaxPathLength);
				}
			}
		}

		++pathIdx;

	} while (!bDone);

	//Clean up hash table
	for (EdgeDataMap::IteratorEx it = m_EdgeData.BeginEx(); it != m_EdgeData.End(); )
	{
		EdgeData* ed = *it;
		it = m_EdgeData.Erase(it);
		m_EdgeDataPool.Delete(ed);
	}
}

bool CAkEdgePathSearch::Search(
	const Ak3DVector& in_fromEdge,
	const CAkDiffractionEdge& in_edge,
	const Ak3DVector& in_toEmitter,
	const AkTriangleIndex& in_triangles,
	AkStochasticRay& inout_results,
	AkUInt32 in_uMaxDegree,	
	AkReal32 in_fMaxPathLength)
{
	AKASSERT(in_uMaxDegree > 0);

	m_fMaxPathLength = in_fMaxPathLength;

	AkUInt8 pathIdx = 0;
	bool bDone;
	bool found = false;

	do
	{
		bDone = true;

		EvalEdge(&in_edge, NULL, in_fromEdge, in_toEmitter, pathIdx, in_fMaxPathLength);

		while (!m_OpenSet.IsEmpty())
		{
			EdgeData* pCur = Pop();
			pCur->closedForPath = pathIdx;

			if (!ValidateEdgeData(pCur, in_triangles))
			{
				continue;
			}

			const CAkDiffractionEdge* pCurEdge = pCur->pEdge;
			AkInt32 to_zone = pCurEdge->GetZone(in_toEmitter);

			if (to_zone >= 0 &&
				to_zone != pCur->fromZone
				)
			{
				const CAkDiffractionEdge* pEdge = pCur->pEdge;

				//Check if the emitter is visible from this edge.
				OneEdgeFilter filter(pEdge);
				OcclusionChecker<OneEdgeFilter> checker(pCur->edgePt.PointV4F32(), (in_toEmitter - pCur->edgePt).VectorV4F32(), filter);
				in_triangles.RaySearch(pCur->edgePt.PointV4F32(), (in_toEmitter - pCur->edgePt).VectorV4F32(), checker, true);

				if( !checker.IsOccluded() )
				{
					if (pCurEdge->InShadowZoneQuick(pCur->pPrevEdgeInPath == NULL ? in_fromEdge : pCur->pPrevEdgeInPath->edgePt, in_toEmitter))
					{
						// found path.  Add result.
						AddResult(pCur, in_fromEdge, in_toEmitter, inout_results);
						ClearOpenList();
						bDone = true;
						found = true;
						break;
					}
					else
					{
						// The emitter is visible from here, but we are not in the shadow zone.  Deeper paths won't be valid, so stop searching.
						continue;
					}
				}
			}

			if (pCur->nodeCount < (AkInt32)in_uMaxDegree)
			{				
				const AkEdgeVisibility& edges = pCur->fromZone == 0 ? pCurEdge->edges1 : pCurEdge->edges0;
				if (!edges.IsComputed())
				{
					pCurEdge->edgeVisibilityLock.Lock();
					const_cast<CAkDiffractionEdge*>(pCurEdge)->ComputeVisibility();
					pCurEdge->edgeVisibilityLock.Unlock();
				}
				
				for (AkEdgeVisibility::Iterator it = edges.Begin(); it != edges.End(); ++it)
				{
					// 1. Edge has not been verified yet -> postpone the validation
					// 2. Edge is not valid -> do not add it to the evaluation
					if (!(*it).IsVerified() || (*it).IsValid())
					{
						EvalEdge((*it).GetDiffractionEdge(), pCur, in_fromEdge, in_toEmitter, pathIdx, in_fMaxPathLength);
					}
				}
			}
		}

		++pathIdx;

	} while (!bDone);

	//Clean up hash table
	for (EdgeDataMap::IteratorEx it = m_EdgeData.BeginEx(); it != m_EdgeData.End(); )
	{
		EdgeData* ed = *it;
		it = m_EdgeData.Erase(it);
		m_EdgeDataPool.Delete(ed);
	}

	return found;
}

bool CAkEdgePathSearch::Search(
	const Ak3DVector& in_fromPoint,
	const CAkDiffractionEdge& in_edge,
	const Ak3DVector& in_toEmitter,
	const AkTriangleIndex& in_triangles,
	AkStochasticRay& out_path,
	AkDiffractionPathSegment& out_diffractionPath,
	AkUInt32 in_uMaxDegree,
	AkReal32 in_fMaxPathLength,
	bool in_reverse)
{
	AKASSERT(in_uMaxDegree > 0);

	m_fMaxPathLength = in_fMaxPathLength;

	AkUInt8 pathIdx = 0;
	bool bDone;
	bool found = false;

	do
	{
		bDone = true;

		EvalEdge(&in_edge, NULL, in_fromPoint, in_toEmitter, pathIdx, in_fMaxPathLength);

		while (!m_OpenSet.IsEmpty())
		{
			// This is the next potential node in the A* search
			EdgeData* pCur = Pop();
			pCur->closedForPath = pathIdx;

			if (!ValidateEdgeData(pCur, in_triangles))
			{
				continue;
			}

			// Get the associated diffraction edge
			const CAkDiffractionEdge* pCurEdge = pCur->pEdge;				

			// Check if the emitter is visible from this edge
			AkInt32 to_zone = pCurEdge->GetZone(in_toEmitter);
			if (to_zone >= 0 &&
				to_zone != pCur->fromZone
				)
			{
				//Check if the emitter is visible from this edge.
				OneEdgeFilter filter(pCurEdge);
				OcclusionChecker<OneEdgeFilter> checker(pCur->edgePt.PointV4F32(), (in_toEmitter - pCur->edgePt).VectorV4F32(), filter);
				in_triangles.RaySearch(pCur->edgePt.PointV4F32(), (in_toEmitter - pCur->edgePt).VectorV4F32(), checker, true);

				if (!checker.IsOccluded())
				{
					if (pCurEdge->InShadowZoneQuick(pCur->pPrevEdgeInPath == NULL ? in_fromPoint : pCur->pPrevEdgeInPath->edgePt, in_toEmitter))
					{
						// found path.  Add result.
						AddResult(pCur, in_fromPoint, in_toEmitter, in_reverse, out_path, out_diffractionPath);
						ClearOpenList();
						bDone = true;
						found = true;
						break;
					}
					else
					{
						// The emitter is visible from here, but we are not in the shadow zone.  Deeper paths won't be valid, so stop searching.
						continue;
					}
				}
			}

			// Emitter not visible -> compute next edges
			if (pCur->nodeCount < (AkInt32)in_uMaxDegree)
			{
				
				const AkEdgeVisibility& edges = pCur->fromZone == 0 ? pCurEdge->edges1 : pCurEdge->edges0;
				if (!edges.IsComputed())
				{
					pCurEdge->edgeVisibilityLock.Lock();
					const_cast<CAkDiffractionEdge*>(pCurEdge)->ComputeVisibility();
					pCurEdge->edgeVisibilityLock.Unlock();
				}
				
				for (AkEdgeVisibility::Iterator it = edges.Begin(); it != edges.End(); ++it)
				{
					// 1. Edge has not been verified yet -> postpone the validation
					// 2. Edge is not valid -> do not add it to the evaluation
					if (!(*it).IsVerified() || (*it).IsValid())
					{
						EvalEdge((*it).GetDiffractionEdge(), pCur, in_fromPoint, in_toEmitter, pathIdx, in_fMaxPathLength);
					}
				}				
			}
		}

		++pathIdx;

	} while (!bDone);

	//Clean up hash table
	for (EdgeDataMap::IteratorEx it = m_EdgeData.BeginEx(); it != m_EdgeData.End(); )
	{
		EdgeData* ed = *it;
		it = m_EdgeData.Erase(it);
		m_EdgeDataPool.Delete(ed);
	}

	return found;
}

void CAkEdgePathSearch::AddResult(EdgeData* in_pEndEdge, const Ak3DVector& in_listenerPos, const Ak3DVector& in_emitter, IAkDiffractionPaths& out_results)
{
	AkDiffractionPathSegment* path = out_results.AddPath();
	if (path)
	{
		path->listenerPos = in_listenerPos;
		path->emitterPos = in_emitter;
		path->nodeCount = in_pEndEdge->nodeCount;

		AkUInt32 idx = 0;
		EdgeData* pEdge = in_pEndEdge;

		do
		{
			// remove from search, we want to find more paths that have different angles to the listener.
			pEdge->admissable = false;

			AkUInt32 arraySlot = path->nodeCount - idx - 1;
			if (arraySlot < AkDiffractionPath::kMaxLength)
			{
				path->nodes[arraySlot] = pEdge->edgePt;
				path->AugmentPathID(pEdge->pEdge);
			}

			++idx;
			pEdge = pEdge->pPrevEdgeInPath;
		}
		while (pEdge != NULL);

	}
}

void CAkEdgePathSearch::AddResult(
	EdgeData* in_pEndEdge,
	const Ak3DVector& in_listenerPos,
	const Ak3DVector& in_emitter, 
	const AkStochasticRay& in_stochasticPath,
	AkDiffractionPathSegment& out_diffractionPath)
{

	out_diffractionPath.listenerPos = in_listenerPos;
	out_diffractionPath.emitterPos = in_emitter;
	out_diffractionPath.nodeCount = in_pEndEdge->nodeCount + in_stochasticPath.m_sources.Length() - 1;

	// First add the edges from the stochastic path
	AkUInt32 idx = 0;
	for (AkUInt32 i = 0; i < in_stochasticPath.m_sources.Length() - 1; ++i)
	{
		const CAkDiffractionEdge* pDiffractionEdge = in_stochasticPath.m_sources[i].getDiffractionEdge();
		pDiffractionEdge->FindDiffractionPt(in_listenerPos, in_emitter - in_listenerPos, out_diffractionPath.nodes[i]);
		out_diffractionPath.AugmentPathID(pDiffractionEdge);

		++idx;
	}

	// Then add the edges from the A* search
	EdgeData* pEdge = in_pEndEdge;
	do
	{
		// remove from search, we want to find more paths that have different angles to the listener.
		pEdge->admissable = false;

		AkUInt32 arraySlot = out_diffractionPath.nodeCount - idx - 1;
		if (arraySlot < AkDiffractionPath::kMaxLength)
		{
			out_diffractionPath.nodes[arraySlot] = pEdge->edgePt;
			out_diffractionPath.AugmentPathID(pEdge->pEdge);
		}

		++idx;
		pEdge = pEdge->pPrevEdgeInPath;
	} while (pEdge != NULL);

}

void CAkEdgePathSearch::AddResult(
	EdgeData* in_pEndEdge,
	const Ak3DVector& in_listenerPos,
	const Ak3DVector& in_emitter,
	bool in_reverse,
	AkStochasticRay& out_stochasticPath,
	AkDiffractionPathSegment& out_diffractionPath)
{
	out_stochasticPath.m_sources.RemoveAll();

	EdgeData* pEdge = in_pEndEdge;

	out_diffractionPath.listenerPos = in_reverse ? in_emitter : in_listenerPos;
	out_diffractionPath.emitterPos = in_reverse ? in_listenerPos : in_emitter;
	out_diffractionPath.nodeCount = in_pEndEdge->nodeCount;

	AkUInt32 idx = 0;

	do
	{
		// remove from search, we want to find more paths that have different angles to the listener.
		pEdge->admissable = false;

		if (in_reverse)
		{
			StochasticSource* source = out_stochasticPath.m_sources.AddLast();
			if (source)
			{
				source->setDiffractionEdge(pEdge->pEdge, pEdge->fromZone);
			}
		}
		else
		{
			StochasticSource* source = out_stochasticPath.m_sources.Insert(0);
			if (source)
			{
				source->setDiffractionEdge(pEdge->pEdge, pEdge->fromZone);
			}
		}
			
		AkUInt32 arraySlot;			
		if (in_reverse)
		{
			arraySlot = idx;
		}
		else
		{
			arraySlot = out_diffractionPath.nodeCount - idx - 1;
		}

		if (arraySlot < AkDiffractionPath::kMaxLength)
		{
			out_diffractionPath.nodes[arraySlot] = pEdge->edgePt;
			out_diffractionPath.AugmentPathID(pEdge->pEdge);
		}

		++idx;
			
		pEdge = pEdge->pPrevEdgeInPath;

	} while (pEdge != NULL);
	
}

void CAkEdgePathSearch::AddResult(EdgeData* in_pEndEdge, const Ak3DVector& in_listenerPos, const Ak3DVector& in_emitter, AkStochasticRay& out_stochasticPath)
{
	EdgeData* pEdge = in_pEndEdge;	
	AkUInt32 initialLength = out_stochasticPath.m_sources.Length();

	do
	{
		// remove from search, we want to find more paths that have different angles to the listener.
		pEdge->admissable = false;

		if (pEdge->pPrevEdgeInPath)
		{
			StochasticSource* source = out_stochasticPath.m_sources.Insert(initialLength);
			if (source)
			{
				source->setDiffractionEdge(pEdge->pEdge, pEdge->fromZone);
			}
		}

		pEdge = pEdge->pPrevEdgeInPath;

	} while (pEdge != NULL);
}

bool CAkEdgePathSearch::EvalEdge(const CAkDiffractionEdge* in_pEdge, EdgeData* in_pPrev, const Ak3DVector& in_start, const Ak3DVector& in_end, AkUInt8 in_curPathIdx, AkReal32 in_fMaxPathLength)
{
	// Evaluate edge heuristics
	Ak3DVector prev = in_pPrev != NULL ? in_pPrev->edgePt : in_start;

	AkInt8 from_zone = (AkInt8)in_pEdge->GetZone(prev);
	if (from_zone < 0)
	{
		//Edge is not reachable from here.
		return false;
	}

	Ak3DVector edgePt;
	if (!in_pEdge->FindDiffractionPt(in_start, in_end - in_start, edgePt))
	{
		return false;
	}

	if (in_pPrev && in_pPrev->pEdge->GetZone(edgePt + in_pPrev->pEdge->n0 * AK_SA_EPSILON + in_pPrev->pEdge->n1 * AK_SA_EPSILON) < 0)
	{
		//Part of the edge is reachable, because it was in the prev edge's visible list, but the specific point (edgePt) on the path in not in the shadow zone.
		return false;
	}

	Ak3DVector diffracted = (prev - edgePt);
	AkReal32 lengthDiffracted = diffracted.Length();
	if (lengthDiffracted < AK_SA_EPSILON)
		return false;

	diffracted = diffracted / lengthDiffracted;
	AkReal32 distanceFromStart = lengthDiffracted;

	AkReal32 diffractionAngle = 0;
	AkReal32 diffractionAccum = 0;
	
	if (in_pPrev != NULL)
	{
		if (!in_pPrev->pEdge->InShadowZoneQuick_vec(in_pPrev->diffractedVec, diffracted))
			return false;

		// Calculate the diffraction angle of the previous node.
		AkReal32 dot = in_pPrev->diffractedVec.Dot(diffracted);
		diffractionAngle = AK::SpatialAudio::ACos(dot);
		
		diffractionAccum = in_pPrev->diffractionAccum;
		if (!AkDiffractionAccum(diffractionAccum, diffractionAngle / PI))
			return false;

		distanceFromStart = lengthDiffracted + in_pPrev->distanceFromStart;
	}

	// incident is the vector from the emitter to the edge.  It supposes that we have reached the end of the search, and is used for heuristics.
	Ak3DVector incident = (edgePt - in_end);
	AkReal32 distanceToEnd = incident.Length();

	if (distanceToEnd < AK_SA_EPSILON ||	// degenerate segment
		distanceToEnd + distanceFromStart > in_fMaxPathLength) // path exceeds max distance.
		return false;

	incident = incident / distanceToEnd;

	// Calculate potential diffraction angle to the emitter (in_end) to use as a heuristic.
	AkReal32 dot = diffracted.Dot(incident);
	AkReal32 diffractionToEnd = AK::SpatialAudio::ACos(dot);

	// Heuristic accumulated diffraction.
	AkReal32 heuristicDfrcnAccum = diffractionAccum;
	if (!AkDiffractionAccum(heuristicDfrcnAccum, diffractionToEnd / PI))
		return false;// path exceeds max angle.

	EdgeData* pData = m_EdgeData.Exists(in_pEdge);
	if (!pData)
	{
		pData = m_EdgeDataPool.New(in_pEdge);
		if (pData)
		{
			if (m_EdgeData.Set(pData))
			{
				pData->pEdge = in_pEdge;
			}
			else
			{
				m_EdgeDataPool.Delete(pData);
				pData = NULL;
			}
		}
	}

	if (pData == NULL || !pData->admissable || pData->closedForPath == in_curPathIdx)
		return false;

	// If a previous path to this node exists, check to see if this is a better path.
	if (pData->pathIdx == in_curPathIdx)
	{
		AkReal32 newCost = CostFcn(distanceToEnd + distanceFromStart, m_fMaxPathLength, heuristicDfrcnAccum);

		if (newCost > pData->Cost(m_fMaxPathLength)) 
			return false;//this is not a better path.
	}

	//
	//

	pData->diffractedVec = diffracted;
	pData->edgePt = edgePt;
	
	pData->pPrevEdgeInPath = in_pPrev;

	pData->distanceSeg = lengthDiffracted;
	pData->distanceFromStart = distanceFromStart;
	pData->distanceToEnd = distanceToEnd;

	pData->diffractionPrev = diffractionAngle;
	pData->diffractionAccum = diffractionAccum;
	pData->diffractionToEnd = diffractionToEnd;
	
	pData->fromZone = from_zone;
	pData->pathIdx = in_curPathIdx;
	pData->nodeCount = in_pPrev != NULL ? in_pPrev->nodeCount + 1 : 1;

	InsertOpenEdge(pData);

	return true;
}

void CAkEdgePathSearch::InsertOpenEdge(EdgeData* in_pEdgeData)
{
	HeapStruct* pData = m_OpenSet.Insert(in_pEdgeData->Cost(m_fMaxPathLength));
	if (pData)
	{
		pData->item = in_pEdgeData;
	}
}

void CAkEdgePathSearch::ClearOpenList()
{
	m_OpenSet.RemoveAll();
}

CAkEdgePathSearch::EdgeData* CAkEdgePathSearch::Pop()
{
	if (m_OpenSet.Length() > 0)
	{
		EdgeData* pFirst = NULL;
		AkReal32 cost;
		do
		{
			pFirst = m_OpenSet[0].item;
			cost = m_OpenSet[0].key;
			m_OpenSet.RemoveRoot();

			//If the key doesn't match the Cost(), it was bumped up in the heap, and therefor already processed.
		} 
		while (fabsf(cost - pFirst->Cost(m_fMaxPathLength)) > FLT_EPSILON
			&& m_OpenSet.Length() > 0);

		return pFirst;
	}

	return NULL;
}

bool CAkEdgePathSearch::ValidateEdgeData(EdgeData* in_edgeData, const AkTriangleIndex& in_triangles)
{
	EdgeData* pPrev = in_edgeData->pPrevEdgeInPath;
	if (pPrev)
	{
		const CAkDiffractionEdge* pCurEdge = in_edgeData->pEdge;
		
		const CAkDiffractionEdge* pPrevEdge = pPrev->pEdge;
		const AkEdgeVisibility& edges = pPrev->fromZone == 0 ? pPrevEdge->edges1 : pPrevEdge->edges0;

		AKASSERT(edges.IsComputed());

		AkEdgeLink* link = edges.GetLink(pCurEdge);
		AKASSERT(link != NULL);

		if (!link->IsVerified())
		{
			// Check visibility with ray casting
			Ak3DVector p0 = pPrevEdge->start + pPrevEdge->direction * pPrevEdge->length / 2.f;
			Ak3DVector p1 = pCurEdge->start + pCurEdge->direction * pCurEdge->length / 2.f;

			Ak3DVector dir = p1 - p0;

			AKSIMD_V4F32 o = p0.PointV4F32();
			AKSIMD_V4F32 d = dir.VectorV4F32();
	
			TwoEdgeFilter filter(pCurEdge,pPrevEdge);
			OcclusionChecker<TwoEdgeFilter> checker(o, d, filter);
			in_triangles.RaySearch(o, d, checker);

			link->SetValid(!checker.IsOccluded());
		}

		return link->IsValid();		
	}

	return true;
}

// Used as the heuristic value in the A* search.
AkReal32 CAkEdgePathSearch::EdgeData::Cost(AkReal32 in_fMaxPathLength) const
{
	AkReal32 accum = diffractionAccum;
	AkDiffractionAccum(accum, diffractionToEnd / PI);
	return CostFcn(distanceToEnd + distanceFromStart, in_fMaxPathLength, accum);
}
