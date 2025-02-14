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
#include "stdafx.h"

#include "AkGeometrySet.h"
#include "AkSoundGeometry.h"
#include "AkImageSource.h"
#include "AkDiffractionEdge.h"
#include "AkMonitor.h"

namespace {

	typedef ArrayPoolDefault GeomLoadingScratchPool; // Using default pool so as to not fragment spatial audio pool, but could be a different temp pool.

	struct Edge
	{
		Edge() :v0(AK_INVALID_VERTEX), v1(AK_INVALID_VERTEX) {}
		Edge(AkVertIdx in_a, AkVertIdx in_b)
		{
			if (in_a < in_b)
			{
				v0 = in_a;
				v1 = in_b;
			}
			else
			{
				v0 = in_b;
				v1 = in_a;
			}
		}
		
		bool operator<(const Edge& in_other) const
		{
			return v0 == in_other.v0 ? v1 < in_other.v1 : v0 < in_other.v0;
		}
		bool operator>(const Edge& in_other) const
		{
			return v0 == in_other.v0 ? v1 > in_other.v1 : v0 > in_other.v0;
		}

		bool operator==(const Edge& in_other) const
		{
			return v0 == in_other.v0 && v1 == in_other.v1;
		}

		AkVertIdx v0, v1; // such that v0 < v1
	};
	struct AdjacencyData
	{
		AdjacencyData() : tri0(AK_INVALID_TRIANGLE), tri1(AK_INVALID_TRIANGLE), plane0(NULL), plane1(NULL) {}
		Edge key;
		AkTriIdx tri0;
		AkTriIdx tri1;
		AkImageSourcePlane* plane0;
		AkImageSourcePlane* plane1;
	};

	typedef AkSortedKeyArray<Edge, AdjacencyData, GeomLoadingScratchPool > AdjacentTriangleArray;

}
AkAcousticSurface AkGeometrySet::sDefaultAcousticSurface;

AKRESULT AkGeometrySet::SetParams(AkGeometryParams& in_params)
{
	AKASSERT(tris == NULL);
	AKASSERT(verts == NULL);
	AKASSERT(surfs == NULL);

	// **** AkGeometrySet will take ownership of memory arrays in AkGeometryParams.

	tris = in_params.Triangles;
	numTris = in_params.NumTriangles;
	
	verts = in_params.Vertices;
	numVerts = in_params.NumVertices;

	surfs = in_params.Surfaces;
	numSurfs = in_params.NumSurfaces;

	bMemoryOwner = true;

	bEnableDiffraction = in_params.EnableDiffraction;
	bEnableDiffractionOnBoundaryEdges = in_params.EnableDiffractionOnBoundaryEdges;

	// Nullify the params, so that we don't try to delete the data later.
	in_params.Triangles = NULL;
	in_params.NumTriangles = 0;
	in_params.Vertices = NULL;
	in_params.NumVertices = 0;
	in_params.Surfaces = NULL;
	in_params.NumSurfaces = 0;

	return AK_Success;
}

const AkAcousticSurface& AkGeometrySet::GetAcousticSurface(const AkImageSourceTriangle* pTri) const
{
	AkTriIdx idx = (AkTriIdx)(AkUIntPtr)(pTri - collisionTris);
	AKASSERT(idx < numTris);
	AkTriangle& tri = tris[idx];
	return GetAcousticSurface(tri.surface);
}

AkSurfIdx AkGeometrySet::GetSurfIdx(const AkImageSourceTriangle* pTri) const
{
	AkTriIdx idx = (AkTriIdx)(AkUIntPtr)(pTri - collisionTris);
	AKASSERT(idx < numTris);
	AkTriangle& tri = tris[idx];
	return tri.surface;
}

AkTriIdx AkGeometrySet::GetTriIdx(const AkImageSourceTriangle* in_tri) const
{
	AkTriIdx idx = (AkTriIdx)(AkUIntPtr)(in_tri - collisionTris);
	AKASSERT(idx < numTris);
	return idx;
}

AkEdgeIdx AkGeometrySet::GetEdgeIdx(const CAkDiffractionEdge* in_edge) const
{
	AkEdgeIdx idx = (AkEdgeIdx)(AkUIntPtr)(in_edge - edges);
	AKASSERT(idx < numEdges);
	return idx;
}

void AkGeometrySet::GetTriangleVerts(const AkImageSourceTriangle* pTri, Ak3DVector& out_p0, Ak3DVector& out_p1, Ak3DVector& out_p2) const
{
	AkTriIdx idx = (AkTriIdx)(AkUIntPtr)(pTri - collisionTris);
	AKASSERT(idx < numTris);
	AkTriangle& tri = tris[idx];
	out_p0 = GetVert(tri.point0);
	out_p1 = GetVert(tri.point1);
	out_p2 = GetVert(tri.point2);
}

void AkGeometrySet::Term()
{
	if (bMemoryOwner)
	{
		if (tris != NULL)
		{
			for (AkUInt32 i = 0; i < numTris; ++i)
				tris[i].~AkTriangle();
			
			AkFree(AkMemID_SpatialAudioGeometry, tris);
		}

		if (verts != NULL)
		{
			for (AkUInt32 i = 0; i < numVerts; ++i)
				verts[i].~AkVertex();

			AkFree(AkMemID_SpatialAudioGeometry, verts);
		}

		if (surfs != NULL)
		{
			for (AkUInt32 i = 0; i < numSurfs; ++i)
			{
				if (surfs[i].strName != NULL)
					AkFree(AkMemID_SpatialAudioGeometry, (void*) surfs[i].strName);

				surfs[i].~AkAcousticSurface();
			}

			AkFree(AkMemID_SpatialAudioGeometry, surfs);
		}

		bMemoryOwner = false;
	}

	AKASSERT(collisionTris == NULL);	// <- must call Unindex()!!
	AKASSERT(edges == NULL);			// <- must call Unindex()!!
	AKASSERT(pScene == NULL);			// <- must call Unindex()!!
	AKASSERT(m_PlanesIndex.IsEmpty());	// <- must call Unindex()!!

	m_PlanesIndex.Term();

	tris = NULL;
	verts = NULL;
	surfs = NULL;
	collisionTris = NULL;
	edges = NULL;

	numTris = 0;
	numVerts = 0;
	numSurfs = 0;
	numEdges = 0;
}

void ConnectEdge(AkVertIdx in_v0, AkVertIdx in_v1, AkTriIdx in_tri, AkImageSourcePlane* in_pPlane, AdjacentTriangleArray& io_adjacentTris)
{
	Edge e( in_v0, in_v1 );
	AdjacencyData* pItem = io_adjacentTris.Set(e);
	AKASSERT(pItem != NULL); //Reserved memory ahead of time.
	
#ifndef AK_OPTIMIZED
	if (pItem->tri0 != AK_INVALID_TRIANGLE && pItem->tri1 != AK_INVALID_TRIANGLE)
	{
		AkOSChar buf[256];
		AK_OSPRINTF(buf, (sizeof(buf) / sizeof(AkOSChar)), AKTEXT("AK::SpatialAudio::SetGeometry - More than two triangles (%i, %i, %i) are connected to the same edge [%i, %i].\n"),
			pItem->tri0, pItem->tri1, in_tri, in_v0, in_v1);
		MONITOR_ERRORMSG(buf);
	}
#endif

	if (pItem->tri0 == AK_INVALID_TRIANGLE)
	{
		AKASSERT(pItem->tri1 == AK_INVALID_TRIANGLE);
		pItem->tri0 = in_tri;
		pItem->plane0 = in_pPlane;
	}
	else if (pItem->plane0 != in_pPlane)
	{
		AKASSERT(pItem->tri1 == AK_INVALID_TRIANGLE);
		pItem->tri1 = in_tri;
		pItem->plane1 = in_pPlane;
	}
	else
	{
		//Triangles are co-planar, so remove data entry
		AdjacentTriangleArray::Iterator it;
		it.pItem = pItem;
		io_adjacentTris.Erase(it);
	}
}

AKRESULT IndexTriangle(AkGeometrySet* gs, AkTriangle& in_tri, AkImageSourceTriangle& in_ColTri, const Ak3DVector& in_p0, const Ak3DVector& in_p1, const Ak3DVector& in_p2,
	AkTriangleIndex& in_triIdx, AkImageSourcePlanePool& in_planesPool, AkImageSourcePlaneArray& in_planesIdx, AdjacentTriangleArray& io_adjacentTris, bool in_enableDiffraction)
{
	AKRESULT res = AK_Success;

	bool bFoundPlane = false;
	AkImageSourcePlaneArray::Iterator it = in_planesIdx.Begin();
	const AkImageSourcePlaneArray::Iterator itEnd = in_planesIdx.End();
	while (it != itEnd && !bFoundPlane)
	{
		bFoundPlane = (*it)->AddTriangleIfCoplanar(&in_ColTri, in_p0, in_p1, in_p2);
		++it;
	}

	if (!bFoundPlane)
	{
		AkImageSourcePlane* pPlane = in_planesPool.New(in_p0, in_p1, in_p2);
		AKASSERT((AkUIntPtr)pPlane % 16 == 0);
		if (pPlane != NULL)
		{
			AKVERIFY(pPlane->AddTriangleIfCoplanar(&in_ColTri, in_p0, in_p1, in_p2));

			if (in_ColTri.pPlane)
				in_planesIdx.AddLast(pPlane);
			else
				in_planesPool.Delete(pPlane);
		}
	}

	if (in_ColTri.pPlane != NULL)
	{
		AkBoundingBox bb;
		bb.Update(gs->GetVert(in_tri.point0));
		bb.Update(gs->GetVert(in_tri.point1));
		bb.Update(gs->GetVert(in_tri.point2));
		
		in_triIdx.Insert(bb.m_Min.PointV4F32(), bb.m_Max.PointV4F32(), &in_ColTri);

		if (in_enableDiffraction)
		{
			AkTriIdx triIdx = gs->GetTriIdx(&in_tri);
			ConnectEdge(in_tri.point0, in_tri.point1, triIdx, in_ColTri.pPlane, io_adjacentTris);
			ConnectEdge(in_tri.point1, in_tri.point2, triIdx, in_ColTri.pPlane, io_adjacentTris);
			ConnectEdge(in_tri.point2, in_tri.point0, triIdx, in_ColTri.pPlane, io_adjacentTris);
		}
	}
	else
	{
		res = AK_Fail;
	}

	return res;
}

AKRESULT AkGeometrySet::Index(AkScene& in_scene, AkImageSourcePlanePool& in_planesPool)
{
	AKASSERT(pScene == NULL);
	pScene = &in_scene;
	pScene->AddRef();

	AkTriangleIndex& in_triIdx = in_scene.GetTriangleIndex();

	AKRESULT res = AK_InsufficientMemory;

	collisionTris = (AkImageSourceTriangle*)AkMalign(AkMemID_SpatialAudio, numTris * sizeof(AkImageSourceTriangle), 16);
	if (collisionTris != NULL)
	{
		res = AK_Success;

		AdjacentTriangleArray adjacentTriArray;
		if (bEnableDiffraction)
			res = adjacentTriArray.Reserve(numTris * 3);

		if (res != AK_Success)
		{
			AkFalign(AkMemID_SpatialAudio, collisionTris);
			collisionTris = NULL;
		}
		else
		{
			res = AK_Success;

			for (AkTriIdx i = 0; i < numTris; ++i)
			{
				AkTriangle& tri = tris[i];

				Ak3DVector p0 = GetVert(tri.point0);
				Ak3DVector p1 = GetVert(tri.point1);
				Ak3DVector p2 = GetVert(tri.point2);

				AkPlacementNew(&collisionTris[i]) AkImageSourceTriangle(this);

				res = collisionTris[i].Init(p0, p1, p2);
				if (res != AK_Success)
				{
					AkOSChar buf[256];
					AK_OSPRINTF(buf, (sizeof(buf) / sizeof(AkOSChar)),
						AKTEXT("AK::SpatialAudio::SetGeometry - Triangle %i formed by vertices [%i, %i, %i] is too large.\n"),
						i, tri.point0, tri.point1, tri.point2);
					MONITOR_ERRORMSG(buf);
				}

				if (res == AK_Success)
					res = IndexTriangle(this, tris[i], collisionTris[i], p0, p1, p2, in_triIdx, in_planesPool, m_PlanesIndex, adjacentTriArray, bEnableDiffraction);
			}

			if (res != AK_Success)
			{
				Unindex(in_planesPool);
			}
			else if (bEnableDiffraction)
			{
				AKASSERT(edges == NULL);
				
				if (!bEnableDiffractionOnBoundaryEdges)
				{ 
					//The actual number of edges does not include the boundary edges, so filter them out.
					AdjacentTriangleArray::Iterator it = adjacentTriArray.Begin(); 
					while (it != adjacentTriArray.End())
					{
						if ((*it).tri1 == AK_INVALID_TRIANGLE)
							adjacentTriArray.EraseSwap(it);
						else
							++it;
					}
				}

				//Allocate Edge buffer
				numEdges = (AkEdgeIdx)adjacentTriArray.Length();
				if (numEdges > 0)
				{
					edges = (CAkDiffractionEdge*)AkMalign(AkMemID_SpatialAudio, numEdges * sizeof(CAkDiffractionEdge), 16);

					if (edges != NULL)
					{
						for (AkUInt32 i = 0; i < numEdges; ++i)
						{
							AdjacencyData data = adjacentTriArray[i];

							AkPlacementNew(&edges[i]) CAkDiffractionEdge(this);

							Ak3DVector start = GetVert(data.key.v0);
							Ak3DVector end = GetVert(data.key.v1);

							AKASSERT(data.tri0 != AK_INVALID_TRIANGLE);

							AkTriangle tri0 = tris[data.tri0];
							AkVertIdx* vpt = &tri0.point0;
							while (*vpt == data.key.v0 || *vpt == data.key.v1)
								vpt++;

							Ak3DVector pt0 = GetVert(*vpt);

							if (data.tri1 != AK_INVALID_TRIANGLE)
							{
								AkTriangle tri1 = tris[data.tri1];
								AkVertIdx* vpt = &tri1.point0;
								while (*vpt == data.key.v0 || *vpt == data.key.v1)
									vpt++;

								Ak3DVector pt1 = GetVert(*vpt);
								
								AkImageSourceTriangle* sourceTri0 = &collisionTris[data.tri0];
								AkImageSourceTriangle* sourceTri1 = &collisionTris[data.tri1];
								edges[i].Init(start, end, pt0, sourceTri0, pt1, sourceTri1);

								AKASSERT(data.plane0 != data.plane1);
								data.plane0->m_Edges.AddLast(&edges[i]);
								data.plane1->m_Edges.AddLast(&edges[i]);
							}
							else 
							{
								AKASSERT(bEnableDiffractionOnBoundaryEdges); //Should have been filtered out otherwise.

								AkImageSourceTriangle* sourceTri0 = &collisionTris[data.tri0];
								edges[i].Init(start, end, pt0, sourceTri0);

								data.plane0->m_Edges.AddLast(&edges[i]);
							}

						}
					}
					else
					{
						res = AK_InsufficientMemory;
						numEdges = 0;
					}
				}
			}
			else
			{
				numEdges = 0;
			}
		}

		adjacentTriArray.Term();
	}

	if (res == AK_Success)
	{
		in_scene.GetGeometrySetList().AddFirst(this);
		in_scene.SetDirty(true);
	}

	return res;
}


void UnindexTriangle(AkGeometrySet* gs, AkTriangle& in_tri, AkImageSourceTriangle& in_ColTri, AkTriangleIndex& in_triIdx, AkImageSourcePlanePool& in_planesPool, AkImageSourcePlaneArray& in_planesIdx)
{
	AkTriangleArray results;

	AkBoundingBox bb;
	bb.Update(gs->GetVert(in_tri.point0));
	bb.Update(gs->GetVert(in_tri.point1));
	bb.Update(gs->GetVert(in_tri.point2));

	in_triIdx.Remove(bb.m_Min.PointV4F32(), bb.m_Max.PointV4F32(), &in_ColTri);

	AkImageSourcePlane* pPlane = in_ColTri.pPlane;
	if (pPlane != NULL)
	{
		// Erase edges that belong to this geo set.
		for (AkEdgeArray::Iterator it = pPlane->m_Edges.Begin(); it != pPlane->m_Edges.End(); )
		{
			CAkDiffractionEdge* pEdge = *it;
			if(pEdge >= gs->edges && pEdge < gs->edges + gs->numEdges)
			{
				it = pPlane->m_Edges.EraseSwap(it);
			}
			else
			{
				++it;
			}
		}

		if (pPlane->RemoveTriangle(&in_ColTri) == 0)
		{
			for (AkImageSourcePlaneArray::Iterator it = in_planesIdx.Begin(), itEnd = in_planesIdx.End();
				it != itEnd; ++it)
			{
				if (*it == pPlane)
				{
					in_planesIdx.EraseSwap(it);
					break;
				}
			}

			in_planesPool.Delete(pPlane);
		}
	}
}

void AkGeometrySet::Unindex(AkImageSourcePlanePool& in_planesPool )
{
	if (pScene == NULL)
		return;

	AkTriangleIndex& in_triIdx = pScene->GetTriangleIndex();

	if (collisionTris != NULL)
	{
		for (AkTriIdx i = 0; i < numTris; ++i)
		{
			UnindexTriangle(this, tris[i], collisionTris[i], in_triIdx, in_planesPool, m_PlanesIndex);
		}

		for (AkUInt32 i = 0; i < numTris; ++i)
			collisionTris[i].~AkImageSourceTriangle();

		AkFalign(AkMemID_SpatialAudio, collisionTris);

		collisionTris = NULL;
	}

	if (edges != NULL)
	{
		for (AkUInt32 i = 0; i < numEdges; ++i)
		{
			AkBoundingBox bb;
			bb.Update(edges[i].start);
			bb.Update(edges[i].start + edges[i].direction*edges[i].length);
			
			edges[i].ClearVisibleEdges();
			edges[i].~CAkDiffractionEdge();
		}

		AkFalign(AkMemID_SpatialAudio, edges);

		edges = NULL;
	}

	pScene->GetGeometrySetList().Remove(this);
	pScene->SetDirty(true);
	pScene->Release();

	pScene = NULL;
}
