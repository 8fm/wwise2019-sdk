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

#pragma once

#include <AK/Tools/Common/AkVectors.h>
#include <AK/Tools/Common/AkHeap.h>
#include <AK/Tools/Common/AkLock.h>
#include <AK/SpatialAudio/Common/AkSpatialAudio.h>

#include "AkSpatialAudioPrivateTypes.h"

class AkImageSourcePlane;
class AkImageSourceTriangle;
class IAkDiffractionPaths;
class AkGeometrySet;

struct AkStochasticRay;
struct AkDiffractionPathSegment;

typedef AkArray< AkEdgeIdx, AkEdgeIdx, ArrayPoolSpatialAudio > AkEdgeIdxArray;

void ComputeDiffraction(const Ak3DVector& in_emitter,
						const AkEdgeArray& in_emitterEdges, 
						const Ak3DVector& in_listener, 
						const AkEdgeArray& in_listenerEdges,
						const AkTriangleIndex& in_triangles,
						IAkDiffractionPaths& out_diffractionResults,
						AkUInt32 in_uMaxDegree, AkUInt32 in_uMaxPaths, AkReal32 in_fMaxPathLength);

class CAkDiffractionEdge;

/// Represent a link with the encapsulated edge
///
class AkEdgeLink
{
public:
	AkEdgeLink(CAkDiffractionEdge* i_edge)
		: m_edge(i_edge)
		, m_verified(false)
		, m_valid(false)
	{}

	AkEdgeLink()
		: m_edge(NULL)
		, m_verified(false)
		, m_valid(false)
	{}

	/// Set the link either valid of invalid.
	/// 
	///@post The link is marked as verified
	///
	void SetValid(bool i_valid) { m_verified = true; m_valid = i_valid; }

	/// Return true if the link is valid
	///
	bool IsValid() const { return m_valid; }

	/// Return true if the link is verified
	///
	bool IsVerified() const { return m_verified; }

	/// Return the diffraction edge
	///
	CAkDiffractionEdge* GetDiffractionEdge() { return m_edge;  }	

	/// Links are same if their edge is the same
	///
	bool operator==(const AkEdgeLink& i_link) const { return m_edge == i_link.m_edge;  }

private:
	/// The end edge the link represents
	///
	CAkDiffractionEdge* m_edge;

	/// If true the link has been verified
	///
	bool m_verified;

	/// If true the link is valid
	///
	bool m_valid;
};

/// Represent a map of visibility to edges.
///
/// When the map has been computed, user must call method MarkAsComputed.
///
class AkEdgeVisibility
	: public AkArray<AkEdgeLink, AkEdgeLink, ArrayPoolSpatialAudioGeometry, AkGrowByPolicy_DEFAULT>
{
public:
	/// Constructor
	///
	AkEdgeVisibility()
		: m_computed(0) {}

	/// Mark the map as fully computed
	///
	void MarkAsComputed() { m_computed = true;  }

	/// Mark the map as uncomputed
	///
	void Unvalidate() { m_computed = false; }

	/// Return true if the map has been fully computed
	///
	bool IsComputed() const { return m_computed; }

	/// Return the link associated with this edge. Or Null if not found
	///
	AkEdgeLink* GetLink(const CAkDiffractionEdge* in_edge) const
	{
		for (AkUInt32 i = 0; i < Length(); ++i)
		{
			if ((*this)[i].GetDiffractionEdge() == in_edge)
			{
				return &(*this)[i];
			}
		}

		return NULL;
	}

private:
	/// Indicate if the map has been fully computed
	///
	bool m_computed;
};

class CAkDiffractionEdge
{
public:
	CAkDiffractionEdge() : pGeoSet(NULL), length(0.f), surf0(nullptr), surf1(nullptr), isPortalEdge(false) {}
	CAkDiffractionEdge(AkGeometrySet* in_pGeoSet) : pGeoSet(in_pGeoSet), length(0.f), surf0(nullptr), surf1(nullptr), isPortalEdge(false) {}
	~CAkDiffractionEdge() {  ClearAllVisSets(); }

	// Init an edge that is connected to only one triangle
	//void Init(const Ak3DVector& in_start, const Ak3DVector& in_end, const Ak3DVector& in_pt, AkSurfIdx in_surf);
	void Init(const Ak3DVector& in_start, const Ak3DVector& in_end, const Ak3DVector& in_pt, AkImageSourceTriangle* in_surf);
	
	// Init an edge that is connected to two triangles
	//void Init(const Ak3DVector& in_start, const Ak3DVector& in_end, const Ak3DVector& in_p0, AkSurfIdx in_surf0, const Ak3DVector& in_p1, AkSurfIdx in_surf1);
	void Init(const Ak3DVector& in_start, const Ak3DVector& in_end, const Ak3DVector& in_p0, AkImageSourceTriangle* in_surf0, const Ak3DVector& in_p1, AkImageSourceTriangle* in_surf1);

	//Takes a point in world space, and returns the vector from 'start' to 'in_pt', 
	//projected onto the edge's rotation plane (about the edge's 'direction' vector)
	Ak3DVector PointToEdgeVec(const Ak3DVector& in_pt) const;

	AkForceInline Ak3DVector Center() const { return start + direction * (length / 2.f); }

	AkInt32 GetZone(const Ak3DVector& in_pt) const;

	bool InShadowZone(const Ak3DVector& in_pt0, const Ak3DVector& in_pt1) const;

	// Assumes that points in_pt0 and in_pt1 are in correct (and opposite) 'zones' as given by GetZone().
	bool InShadowZoneQuick(const Ak3DVector& in_pt0, const Ak3DVector& in_pt1) const;
	
	// same as InShadowZoneQuick, but takes 2 normalized vectors. in_incident points towards edge, and in_diffracted points away from it.
	bool InShadowZoneQuick_vec(const Ak3DVector& in_incident, const Ak3DVector& in_diffracted) const;

	bool FindDiffractionPt(const Ak3DVector& in_origin, const Ak3DVector& in_direction, Ak3DVector& out_diffractionPt) const;
	
	AkReal32 SqDistPointSegment(const Ak3DVector& pt) const;

	void InsertVisibleEdge(const CAkDiffractionEdge& in_Edge, AkInt32 startZone, AkInt32 endZone);

	// Clear the visible edges on this edge, going both ways.
	void ClearVisibleEdges();
	 
	static void ClearVisibleEdges(AkGeometrySet* in_pInSet);

	void EdgeEdgeShadowVis(
		CAkDiffractionEdge& edge,
		const AkTriangleIndex& in_tris,
		bool& out_bVisible,
		AkInt8& out_startZone0, AkInt8& out_endZone0,
		AkInt8& out_startZone1, AkInt8& out_endZone1);

	void ComputeVisibility();

	// Start point of the edge, in world space
	Ak3DVector start;

	// Normalized direction
	Ak3DVector direction;

	Ak3DVector n0;

	Ak3DVector n1;

	const AkGeometrySet* pGeoSet;

	AkEdgeVisibility edges0;
	AkEdgeVisibility edges1;

	// Length of the edge.
	AkReal32 length;

	// Associated triangle
	AkImageSourceTriangle* surf0;
	AkImageSourceTriangle* surf1;

	// Edge belongs to a portal.
	bool isPortalEdge;

	/// Lock is global to all edges
	///
	static CAkLock edgeVisibilityLock;
private:

	void InsertEdge(AkEdgeVisibility& array, const CAkDiffractionEdge& in_Edge);

	void ClearAllVisSets();

};

class CAkEdgePathSearch
{
	struct EdgeData
	{
		EdgeData(const CAkDiffractionEdge* in_pEdge): pEdge(in_pEdge)
			, pPrevEdgeInPath(NULL)
			, pNextItem(NULL)
			, distanceSeg(0.f)
			, distanceFromStart(FLT_MAX)
			, distanceToEnd(FLT_MAX)
			, diffractionPrev(0.f)
			, diffractionToEnd(0.f)
			, diffractionAccum(0.f)
			, fromZone(-1)
			, pathIdx(-1)
			, closedForPath(-1)
			, nodeCount(1)
			, admissable(true)
		{}

		Ak3DVector diffractedVec;	// Normalize direction vector between this node and the previous (or the listener).
		Ak3DVector edgePt;			// Node point on the edge

		const CAkDiffractionEdge* pEdge; // used as key in hash table

		EdgeData* pPrevEdgeInPath;
		EdgeData* pNextItem; // For AkHashListBare

		AkReal32 distanceSeg;		// length of the current segment.  Length of 'diffractedVec', the vector between this node and the previous (or the listener).
		AkReal32 distanceFromStart;		// Total accumulated distance for the path (not including distanceToEnd)
		AkReal32 distanceToEnd;		// distance from this node to the emitter.

		AkReal32 diffractionPrev;	// the diffraction angle of the preceding node.
		AkReal32 diffractionToEnd;  // the diffraction angle to the end node (assuming last in path).
		AkReal32 diffractionAccum;	// accumulation of all 'diffractionPrev' angles in the path, not including diffractionToEnd.

		AkInt8 fromZone;			// Edge-zone that the incident vector comes from (the diffracted vector must go to the opposite zone).
		AkInt8 pathIdx;				// Index of the path that this node has been evaluated for.			
		AkInt8 closedForPath;		// If this node has been evaluated for a path, it is the index of the path.
		AkInt8 nodeCount;			// Number of nodes in the path, including this node.

		bool admissable;	// False if this edge has already been used in a path.

		// Used as the heuristic value in the A* search.
		AkReal32 Cost(AkReal32 in_fMaxPathLength) const;

		static const CAkDiffractionEdge* Key(const EdgeData* in_pItem) { return in_pItem->pEdge; }
	};

	typedef AkDynaBlkPool<EdgeData, 8, ArrayPoolSpatialAudioPaths> EdgeDataPool;
	typedef AkHashListBare< const CAkDiffractionEdge*, EdgeData, ArrayPoolSpatialAudioPaths, EdgeData> EdgeDataMap;

public:
	CAkEdgePathSearch() : m_fMaxPathLength(0) {}
	~CAkEdgePathSearch() 
	{
		m_OpenSet.Term();
		m_EdgeData.Term(); 
	}

	void Search(
		const Ak3DVector& in_fromListener,
		const AkEdgeArray& in_listenerEdges,
		const Ak3DVector& in_toEmitter,
		const AkEdgeArray& in_emitterEdges,
		IAkDiffractionPaths& out_results,
		AkUInt32 in_uMaxDegree,
		AkUInt32 in_uMaxPaths, 
		AkReal32 in_fMaxPathLength);

	bool Search(
		const Ak3DVector& in_fromEdge,
		const CAkDiffractionEdge& in_edge,
		const Ak3DVector& in_toEmitter,
		const AkTriangleIndex& in_triangles,
		AkStochasticRay& inout_results,
		AkUInt32 in_uMaxDegree,
		AkReal32 in_fMaxPathLength);

	bool Search(
		const Ak3DVector& in_fromPoint,
		const CAkDiffractionEdge& in_edge,
		const Ak3DVector& in_toEmitter,
		const AkTriangleIndex& in_triangles,
		AkStochasticRay& out_stochasticPath,
		AkDiffractionPathSegment& out_diffractionPath,
		AkUInt32 in_uMaxDegree,
		AkReal32 in_fMaxPathLength,
		bool in_reverse);
		
private:
	void AddResult(EdgeData* in_pEndEdge, const Ak3DVector& in_fromListener, const Ak3DVector& in_toEmitter, IAkDiffractionPaths& out_results);
	void AddResult(EdgeData* in_pEndEdge, const Ak3DVector& in_listenerPos, const Ak3DVector& in_emitter, const AkStochasticRay& in_stochasticPath, AkDiffractionPathSegment& out_diffractionPath);
	void AddResult(EdgeData* in_pEndEdge, const Ak3DVector& in_fromListener, const Ak3DVector& in_toEmitter, bool in_reverse, AkStochasticRay& out_stochasticPath, AkDiffractionPathSegment& out_diffractionPath);
	void AddResult(EdgeData* in_pEndEdge, const Ak3DVector& in_fromListener, const Ak3DVector& in_toEmitter, AkStochasticRay& out_stochasticPath);
	bool EvalEdge(const CAkDiffractionEdge* in_pEdge, EdgeData* in_pPrev, const Ak3DVector& in_from, const Ak3DVector& in_to, AkUInt8 in_curPathIdx, AkReal32 in_fMaxPathLength);
	void InsertOpenEdge(EdgeData* in_pEdgeData);
	void ClearOpenList();
	EdgeData* Pop();

	bool ValidateEdgeData(EdgeData* in_edgeData, const AkTriangleIndex& in_triangles);

	EdgeDataPool m_EdgeDataPool;
	EdgeDataMap m_EdgeData;
	
	typedef  MapStruct<AkReal32, EdgeData*> HeapStruct;

	typedef CAkHeap< AkReal32, HeapStruct, ArrayPoolSpatialAudio, AkGetArrayKey< AkReal32, HeapStruct > > OpenSetHeap;
	OpenSetHeap m_OpenSet;

	AkReal32 m_fMaxPathLength;
};