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

#include "AkSoundGeometry.h"
#include "AkDiffractionEdge.h"
#include "AkSpatialAudioTasks.h"
#include "AkOcclusionChecker.h"

AKRESULT AkScene::Init()
{
	AKRESULT res = m_TriangleIndex.Init();
	if (res != AK_Success)
		Term();

	return res;
}

void AkScene::Term()
{
	m_TriangleIndex.Term();
	m_NewPortals.Term();
}

void AkScene::ClearVisibleEdges()
{
	for (AkGeometrySetList::Iterator it = m_GeometrySetList.Begin(); it != m_GeometrySetList.End(); ++it)
	{
		CAkDiffractionEdge::ClearVisibleEdges(*it);
	}
}

void AkScene::ConnectPortalToPlane(AkAcousticPortal* in_pPortal, const AkImageSourceTriangle* pTri, AkUInt32 in_FrontOrBack)
{
	in_pPortal->SetScene(this, in_FrontOrBack);

	Ak3DVector extent = in_pPortal->GetExtent();
	
	// Project the 4 corners of the portal on to the triangle.
	Ak3DVector p0 = pTri->Project(in_pPortal->LocalToWorld(extent*Ak3DVector(-1.f, -1.f, -1.f)));//bottom left
	Ak3DVector p1 = pTri->Project(in_pPortal->LocalToWorld(extent*Ak3DVector(-1.f, 1.f, -1.f)));//top left
	Ak3DVector p2 = pTri->Project(in_pPortal->LocalToWorld(extent*Ak3DVector(1.f, 1.f, -1.f)));//top right
	Ak3DVector p3 = pTri->Project(in_pPortal->LocalToWorld(extent*Ak3DVector(1.f, -1.f, -1.f)));//bottom right

	AkImageSourcePlane* pPlane = pTri->pPlane;
	AkSurfIdx surf = pTri->GetSurfIdx();
	const AkGeometrySet* pGeoSet = pTri->pGeoSet;

	AkImageSourceTriangle* pTri0 = &in_pPortal->GetTri(in_FrontOrBack, 0);
	pTri0->Init(p0, p1, p2);
	pTri0->pPlane = pPlane;
	pTri0->pGeoSet = NULL;
	pPlane->m_PortalTriangles.AddLast(pTri0);

	AkImageSourceTriangle* pTri1 = &in_pPortal->GetTri(in_FrontOrBack, 1);
	pTri1->Init(p2, p3, p0);
	pTri1->pPlane = pPlane;
	pTri1->pGeoSet = NULL;
	pPlane->m_PortalTriangles.AddLast(pTri1);

	{// Left edge
		CAkDiffractionEdge* pEdge0 = &in_pPortal->GetEdge(in_FrontOrBack, 0);
		Ak3DVector wallPt0 = p0 + (p0 - p3);
		pEdge0->pGeoSet = pGeoSet;
		pEdge0->Init(p0, p1, wallPt0, const_cast<AkImageSourceTriangle*>(pTri));
		if (!pPlane->HasEdge(pEdge0))
			pPlane->m_Edges.AddLast(pEdge0);
	}
	{// Right edge
		CAkDiffractionEdge* pEdge1 = &in_pPortal->GetEdge(in_FrontOrBack, 1);
		Ak3DVector wallPt1 = p2 + (p2 - p1);
		pEdge1->pGeoSet = pGeoSet;
		pEdge1->Init(p3, p2, wallPt1, const_cast<AkImageSourceTriangle*>(pTri));
		if (!pPlane->HasEdge(pEdge1))
			pPlane->m_Edges.AddLast(pEdge1);
	}
	{// Top edge
		CAkDiffractionEdge* pEdge2 = &in_pPortal->GetEdge(in_FrontOrBack, 2);
		Ak3DVector wallPt2 = p1 + (p1 - p0);
		pEdge2->pGeoSet = pGeoSet;
		pEdge2->Init(p1, p2, wallPt2, const_cast<AkImageSourceTriangle*>(pTri));
		if (!pPlane->HasEdge(pEdge2))
			pPlane->m_Edges.AddLast(pEdge2);
	}
	{// Bottom edge
		CAkDiffractionEdge* pEdge3 = &in_pPortal->GetEdge(in_FrontOrBack, 3);
		Ak3DVector wallPt3 = p0 + (p0 - p1);
		pEdge3->pGeoSet = pGeoSet;
		pEdge3->Init(p0, p3, wallPt3, const_cast<AkImageSourceTriangle*>(pTri));
		if (!pPlane->HasEdge(pEdge3))
			pPlane->m_Edges.AddLast(pEdge3);
	}
}

void AkScene::DisconnectPortal(AkAcousticPortal* in_pPortal, AkUInt32 in_FrontOrBack)
{
	AkPortalGeometry* pGeom = &in_pPortal->GetGeometry(in_FrontOrBack);
	AkImageSourcePlane* pPlane = pGeom->m_Tris[0].pPlane;
	AKASSERT(pPlane == pGeom->m_Tris[1].pPlane);
	if (pPlane != NULL)
	{
		for (AkTriangleArray::Iterator it = pPlane->m_PortalTriangles.Begin(); it != pPlane->m_PortalTriangles.End(); )
		{
			AkImageSourceTriangle* pTri = *it;
			if (pTri == &pGeom->m_Tris[0] ||
				pTri == &pGeom->m_Tris[1] )
			{
				it = pPlane->m_PortalTriangles.Erase(it);
			}
			else
			{
				++it;
			}
		}

		for (AkEdgeArray::Iterator it = pPlane->m_Edges.Begin(); it != pPlane->m_Edges.End(); )
		{
			CAkDiffractionEdge* pEdge = *it;
			if (pEdge == &pGeom->m_Edges[0] ||
				pEdge == &pGeom->m_Edges[1] ||
				pEdge == &pGeom->m_Edges[2] ||
				pEdge == &pGeom->m_Edges[3])
			{
				it = pPlane->m_Edges.Erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	for (AkUInt32 i = 0; i < 4; ++i)
	{
		pGeom->m_Edges[i].ClearVisibleEdges();
		pGeom->m_Edges[i].pGeoSet = NULL;
	}
}


struct PortalConnector
{
	PortalConnector(AkAcousticPortal* in_pPortal) :
		n_x(AKSIMD_SETZERO_V4F32()), n_y(AKSIMD_SETZERO_V4F32()), n_z(AKSIMD_SETZERO_V4F32()), n_w(AKSIMD_SETZERO_V4F32()),
		u_x(AKSIMD_SETZERO_V4F32()), u_y(AKSIMD_SETZERO_V4F32()), u_z(AKSIMD_SETZERO_V4F32()), u_d(AKSIMD_SETZERO_V4F32()),
		v_x(AKSIMD_SETZERO_V4F32()), v_y(AKSIMD_SETZERO_V4F32()), v_z(AKSIMD_SETZERO_V4F32()), v_d(AKSIMD_SETZERO_V4F32()),
		pPortal(in_pPortal), 
		pFront(NULL), 
		pBack(NULL), 
		uNumTri(0)
	{
		Ak3DVector extent = in_pPortal->GetExtent();
		ray_direction = (in_pPortal->GetFront()*extent.Z*2.f).VectorV4F32();
		ray_origin = in_pPortal->LocalToWorld(extent*Ak3DVector(0.f, 0.f, -1.f)).PointV4F32();

		z_Front = -1 * in_pPortal->GetExtent().Z;
		z_Back = in_pPortal->GetExtent().Z;
	}

	//Retrieve the results of the search. 
	void GetResults(const AkImageSourceTriangle*& out_pFront, const AkImageSourceTriangle*& out_pBack)
	{
		if (uNumTri != 0)
		{
			//We may have to call IntersectTriangles() one more time to process the remainder.
			IntersectTriangles();
			uNumTri = 0;
		}

		out_pFront = pFront;
		out_pBack = pBack;
	}

	// To pass into the RTree search.
	AkForceInline AKSIMD_V4F32 Origin() { return ray_origin; }
	AkForceInline AKSIMD_V4F32 Direction() { return ray_direction; }

	// Callback from RTree search.
	AkForceInline bool Add(AkImageSourceTriangle* in_triangle)
	{
		AKASSERT(in_triangle != NULL);

		AKSIMD_GETELEMENT_V4F32(n_x, uNumTri) = in_triangle->nx;
		AKSIMD_GETELEMENT_V4F32(n_y, uNumTri) = in_triangle->ny;
		AKSIMD_GETELEMENT_V4F32(n_z, uNumTri) = in_triangle->nz;
		AKSIMD_GETELEMENT_V4F32(n_w, uNumTri) = in_triangle->nd;

		AKSIMD_GETELEMENT_V4F32(u_x, uNumTri) = in_triangle->ux;
		AKSIMD_GETELEMENT_V4F32(u_y, uNumTri) = in_triangle->uy;
		AKSIMD_GETELEMENT_V4F32(u_z, uNumTri) = in_triangle->uz;
		AKSIMD_GETELEMENT_V4F32(u_d, uNumTri) = in_triangle->ud;

		AKSIMD_GETELEMENT_V4F32(v_x, uNumTri) = in_triangle->vx;
		AKSIMD_GETELEMENT_V4F32(v_y, uNumTri) = in_triangle->vy;
		AKSIMD_GETELEMENT_V4F32(v_z, uNumTri) = in_triangle->vz;
		AKSIMD_GETELEMENT_V4F32(v_d, uNumTri) = in_triangle->vd;

		pTris[uNumTri] = in_triangle;

		++uNumTri;

		if (uNumTri == 4)
		{
			IntersectTriangles();
			uNumTri = 0;
		}

		return true; // keep traversing tree to check all triangles
	}

protected:

	AKSIMD_V4F32 n_x, n_y, n_z, n_w;
	AKSIMD_V4F32 u_x, u_y, u_z, u_d;
	AKSIMD_V4F32 v_x, v_y, v_z, v_d;

	AKSIMD_V4F32 ray_origin;
	AKSIMD_V4F32 ray_direction;

	const AkImageSourceTriangle* pTris[4];

	const AkAcousticPortal* pPortal;
	const AkImageSourceTriangle* pFront;
	const AkImageSourceTriangle* pBack;
	AkReal32 z_Front;
	AkReal32 z_Back;

	AkUInt32 uNumTri;

	AkForceInline void HitTriangle(const AkImageSourceTriangle* in_pHitTri, AKSIMD_V4F32 in_hitPoint)
	{
		Ak3DVector hitPosLocal = pPortal->WorldToLocal(Ak3DVector(in_hitPoint));
		AkReal32 z_val = hitPosLocal.Z;

		if (z_val > z_Front)
		{
			z_Front = z_val;
			pFront = in_pHitTri;
		}

		if (z_val < z_Back)
		{
			z_Back = z_val;
			pBack = in_pHitTri;
		}
	}

	bool IntersectTriangles()
	{
		const AKSIMD_V4F32 o_x = AKSIMD_SHUFFLE_V4F32(ray_origin, ray_origin, AKSIMD_SHUFFLE(0, 0, 0, 0));
		const AKSIMD_V4F32 o_y = AKSIMD_SHUFFLE_V4F32(ray_origin, ray_origin, AKSIMD_SHUFFLE(1, 1, 1, 1));
		const AKSIMD_V4F32 o_z = AKSIMD_SHUFFLE_V4F32(ray_origin, ray_origin, AKSIMD_SHUFFLE(2, 2, 2, 2));

		AKSIMD_V4F32 det = AkMath::DotPoduct3_1x4(ray_direction, n_x, n_y, n_z);

		static const AKSIMD_V4F32 vNegOne = AKSIMD_SET_V4F32(-1.f);
		static const AKSIMD_V4F32 vOne = AKSIMD_SET_V4F32(1.f);
		const AKSIMD_V4F32 dett = AkMath::DotPoduct4_4x4(AKSIMD_MUL_V4F32(vNegOne, n_x), AKSIMD_MUL_V4F32(vNegOne, n_y), AKSIMD_MUL_V4F32(vNegOne, n_z), n_w,
			o_x, o_y, o_z, vOne);

		AKASSERT(uNumTri >= 1 && uNumTri <= 4);
		AkUInt32 validMask = 0xf >> (4 - uNumTri);

		AkUInt32 missMask = (validMask & AKSIMD_MASK_V4F32(AKSIMD_XOR_V4F32(dett, AKSIMD_SUB_V4F32(AKSIMD_MUL_V4F32(vOne, det), dett))));
		if (missMask != validMask)
		{
			const AKSIMD_V4F32 d_x = AKSIMD_SHUFFLE_V4F32(ray_direction, ray_direction, AKSIMD_SHUFFLE(0, 0, 0, 0));
			const AKSIMD_V4F32 d_y = AKSIMD_SHUFFLE_V4F32(ray_direction, ray_direction, AKSIMD_SHUFFLE(1, 1, 1, 1));
			const AKSIMD_V4F32 d_z = AKSIMD_SHUFFLE_V4F32(ray_direction, ray_direction, AKSIMD_SHUFFLE(2, 2, 2, 2));

			AKSIMD_V4F32 detp_x = AKSIMD_ADD_V4F32(AKSIMD_MUL_V4F32(o_x, det), AKSIMD_MUL_V4F32(dett, d_x));
			AKSIMD_V4F32 detp_y = AKSIMD_ADD_V4F32(AKSIMD_MUL_V4F32(o_y, det), AKSIMD_MUL_V4F32(dett, d_y));
			AKSIMD_V4F32 detp_z = AKSIMD_ADD_V4F32(AKSIMD_MUL_V4F32(o_z, det), AKSIMD_MUL_V4F32(dett, d_z));

			const AKSIMD_V4F32 detu = AkMath::DotPoduct4_4x4(u_x, u_y, u_z, u_d, detp_x, detp_y, detp_z, det);

			// Review: what to do when detu == 0?
			AkUInt32 neq0_mask = ~AKSIMD_MASK_V4F32(AKSIMD_EQ_V4F32(detu, AKSIMD_SETZERO_V4F32()));

			missMask |= (validMask & neq0_mask & AKSIMD_MASK_V4F32(AKSIMD_XOR_V4F32(detu, AKSIMD_SUB_V4F32(det, detu))));
			if (missMask != validMask)
			{
				const AKSIMD_V4F32 detv = AkMath::DotPoduct4_4x4(v_x, v_y, v_z, v_d, detp_x, detp_y, detp_z, det);

				missMask |= (validMask & AKSIMD_MASK_V4F32(AKSIMD_XOR_V4F32(detv, AKSIMD_SUB_V4F32(det, AKSIMD_ADD_V4F32(detu, detv)))));
				if (missMask != validMask)
				{
					const AKSIMD_V4F32 inv_det = AKSIMD_DIV_V4F32(vOne, det);
					detp_x = AKSIMD_MUL_V4F32(detp_x, inv_det);
					detp_y = AKSIMD_MUL_V4F32(detp_y, inv_det);
					detp_z = AKSIMD_MUL_V4F32(detp_z, inv_det);

					AkUInt32 hitMask = (~missMask & validMask);

					AkUInt32 idx = 0;
					while(true)
					{
						if ((hitMask & 1U) != 0)
						{
							AKSIMD_V4F32 xxyy = AKSIMD_SHUFFLE_V4F32(detp_x, detp_y, AKSIMD_SHUFFLE(0, 0, 0, 0));
							AKSIMD_V4F32 zzww = AKSIMD_SHUFFLE_V4F32(detp_z, vOne, AKSIMD_SHUFFLE(0, 0, 0, 0));
							AKSIMD_V4F32 hitPoint = AKSIMD_SHUFFLE_V4F32(xxyy, zzww, AKSIMD_SHUFFLE(2, 0, 2, 0));

							HitTriangle(pTris[idx], hitPoint);
						}

						hitMask = hitMask >> 1;
						if (hitMask == 0)
							break;

						detp_x = AKSIMD_SHUFFLE_BCDA(detp_x);
						detp_y = AKSIMD_SHUFFLE_BCDA(detp_y);
						detp_z = AKSIMD_SHUFFLE_BCDA(detp_z);

						++idx;
					}

					return false;
				}
			}
		}

		return false;
	}
};

void AkScene::ConnectPortal(AkAcousticPortal* in_pPortal, bool in_bConnectToFront, bool in_bConnectToBack)
{
	PortalConnector pc(in_pPortal);
	m_TriangleIndex.RaySearch(pc.Origin(), pc.Direction(), pc, true);

	const AkImageSourceTriangle *pFront, *pBack;
	pc.GetResults(pFront, pBack);

	if (pFront != NULL && in_bConnectToFront)
	{
		ConnectPortalToPlane(in_pPortal, pFront, AkAcousticPortal::FrontRoom);
	}

	if (pBack != NULL && in_bConnectToBack)
	{
		ConnectPortalToPlane(in_pPortal, pBack, AkAcousticPortal::BackRoom);
	}

	m_NewPortals.AddLast(in_pPortal);
}