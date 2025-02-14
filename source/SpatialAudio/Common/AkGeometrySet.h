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

#pragma once

#include <AK/SpatialAudio/Common/AkSpatialAudio.h>
#include "AkSpatialAudioPrivateTypes.h"

class AkScene;
class CAkDiffractionEdge;

class AkGeometrySet
{
public:
	AkGeometrySet(AkGeometrySetID in_GroupID) : groupID(in_GroupID), 
		tris(NULL), verts(NULL), surfs(NULL), collisionTris(NULL), edges(NULL), 
		numTris(0), numVerts(0), numSurfs(0), numEdges(0),
		pScene(NULL), pNextItem(NULL), bMemoryOwner(false), bEnableDiffraction(true), bEnableDiffractionOnBoundaryEdges(false) {}

	~AkGeometrySet() { Term(); }

	// AkGeometrySet will take ownership of memory arrays in AkGeometryParams.
	AKRESULT SetParams(AkGeometryParams& in_params);

	AkGeometrySetID		groupID;

	AkTriangle*				tris;
	AkVertex*				verts;
	AkAcousticSurface*			surfs;
	AkImageSourceTriangle*	collisionTris;
	CAkDiffractionEdge*		edges;

	AkTriIdx			numTris;
	AkVertIdx			numVerts;
	AkSurfIdx			numSurfs;
	AkEdgeIdx			numEdges;

	AkScene*	pScene;

	AkGeometrySet* pNextItem;
	AkGeometrySet* pNextLightItem;

	AkImageSourcePlaneArray m_PlanesIndex;

	bool bMemoryOwner;//applies to triangles, verts, and surfaces

	bool bEnableDiffraction;
	bool bEnableDiffractionOnBoundaryEdges;

	// Get key policy for AkHashListBare
	static AkForceInline AkGeometrySetID& Key(AkGeometrySet* in_pItem) { return in_pItem->groupID; }

	AkForceInline Ak3DVector& GetVert(AkVertIdx in_idx) const { return (Ak3DVector&)verts[in_idx]; }

	const AkAcousticSurface& GetAcousticSurface(const AkImageSourceTriangle* pTri) const;
	
	AkSurfIdx GetSurfIdx(const AkImageSourceTriangle* pTri) const;

	AkForceInline const AkAcousticSurface& GetAcousticSurface(AkSurfIdx in_idx) const
	{
		return in_idx < numSurfs ? surfs[in_idx] : sDefaultAcousticSurface;
	}

	AkForceInline AkTriIdx GetTriIdx(const AkTriangle* in_tri) const
	{ 
		AkTriIdx idx = (AkTriIdx)(AkUIntPtr)(in_tri - tris);
		AKASSERT(idx < numTris);
		return idx;
	}

	AkTriIdx GetTriIdx(const AkImageSourceTriangle* in_tri) const;

	AkEdgeIdx GetEdgeIdx(const CAkDiffractionEdge* in_edge) const;

	void GetTriangleVerts(const AkImageSourceTriangle* pTri, Ak3DVector& out_p0, Ak3DVector& out_p1, Ak3DVector& out_p2) const;

	void Term();

	AKRESULT Index(AkScene& in_scene, AkImageSourcePlanePool& in_planesPool);
	void Unindex(AkImageSourcePlanePool& in_planesPool);

	AkImageSourcePlaneArray& GetReflectors() { return m_PlanesIndex; }

	static AkAcousticSurface sDefaultAcousticSurface;
};
