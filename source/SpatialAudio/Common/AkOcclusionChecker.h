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

#include "AkImageSource.h"
#include "AkDiffractionEdge.h"
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkSimdMath.h>
#include "AkMath.h"
 
struct NoFilter
{
	bool Filter(AkImageSourceTriangle* in_triangle) { return false; }
};

AkForceInline const AkImageSourceTriangle* IntersectTriArray(const AkTriangleArray& in_array, const AKSIMD_V4F32& ray_origin, const AKSIMD_V4F32 ray_direction, AKSIMD_V4F32& out_hitPoint)
{
	AkUInt32 uLength = in_array.Length();
	for (AkUInt32 triIdx = 0; triIdx < uLength; triIdx += 4)
	{
		AkImageSourceTriangle& tri0 = *in_array[triIdx];
		AkImageSourceTriangle& tri1 = triIdx + 1 < uLength ? *in_array[triIdx + 1] : tri0;
		AkImageSourceTriangle& tri2 = triIdx + 2 < uLength ? *in_array[triIdx + 2] : tri1;
		AkImageSourceTriangle& tri3 = triIdx + 3 < uLength ? *in_array[triIdx + 3] : tri2;

		AkUInt32 mask = 0;		
		const AKSIMD_V4F32 n0 = AKSIMD_LOAD_V4F32(&tri0.nx);
		const AKSIMD_V4F32 n1 = AKSIMD_LOAD_V4F32(&tri1.nx);
		const AKSIMD_V4F32 n2 = AKSIMD_LOAD_V4F32(&tri2.nx);
		const AKSIMD_V4F32 n3 = AKSIMD_LOAD_V4F32(&tri3.nx);

		AKSIMD_V4F32 n_x, n_y, n_z, n_w;
		AkMath::PermuteVectors4(n0, n1, n2, n3, n_x, n_y, n_z, n_w);

		const AKSIMD_V4F32 det = AkMath::DotPoduct3_1x4(ray_direction, n_x, n_y, n_z);

		const AKSIMD_V4F32 o_x = AKSIMD_SHUFFLE_V4F32(ray_origin, ray_origin, AKSIMD_SHUFFLE(0, 0, 0, 0));
		const AKSIMD_V4F32 o_y = AKSIMD_SHUFFLE_V4F32(ray_origin, ray_origin, AKSIMD_SHUFFLE(1, 1, 1, 1));
		const AKSIMD_V4F32 o_z = AKSIMD_SHUFFLE_V4F32(ray_origin, ray_origin, AKSIMD_SHUFFLE(2, 2, 2, 2));

		static const AKSIMD_V4F32 vNegOne = AKSIMD_SET_V4F32(-1.f);
		static const AKSIMD_V4F32 vOne = AKSIMD_SET_V4F32(1.f);
		const AKSIMD_V4F32 dett = AkMath::DotPoduct4_4x4(AKSIMD_MUL_V4F32(vNegOne, n_x), AKSIMD_MUL_V4F32(vNegOne, n_y), AKSIMD_MUL_V4F32(vNegOne, n_z), n_w,
			o_x, o_y, o_z, vOne);

		mask |= AKSIMD_MASK_V4F32(AKSIMD_XOR_V4F32(dett, AKSIMD_SUB_V4F32(AKSIMD_MUL_V4F32(vOne, det), dett)));
		if (mask != 0xf)
		{
			const AKSIMD_V4F32 d_x = AKSIMD_SHUFFLE_V4F32(ray_direction, ray_direction, AKSIMD_SHUFFLE(0, 0, 0, 0));
			const AKSIMD_V4F32 d_y = AKSIMD_SHUFFLE_V4F32(ray_direction, ray_direction, AKSIMD_SHUFFLE(1, 1, 1, 1));
			const AKSIMD_V4F32 d_z = AKSIMD_SHUFFLE_V4F32(ray_direction, ray_direction, AKSIMD_SHUFFLE(2, 2, 2, 2));

			const AKSIMD_V4F32 detp_x = AKSIMD_ADD_V4F32(AKSIMD_MUL_V4F32(o_x, det), AKSIMD_MUL_V4F32(dett, d_x));
			const AKSIMD_V4F32 detp_y = AKSIMD_ADD_V4F32(AKSIMD_MUL_V4F32(o_y, det), AKSIMD_MUL_V4F32(dett, d_y));
			const AKSIMD_V4F32 detp_z = AKSIMD_ADD_V4F32(AKSIMD_MUL_V4F32(o_z, det), AKSIMD_MUL_V4F32(dett, d_z));

			const AKSIMD_V4F32 u0 = AKSIMD_LOAD_V4F32(&tri0.ux);
			const AKSIMD_V4F32 u1 = AKSIMD_LOAD_V4F32(&tri1.ux);
			const AKSIMD_V4F32 u2 = AKSIMD_LOAD_V4F32(&tri2.ux);
			const AKSIMD_V4F32 u3 = AKSIMD_LOAD_V4F32(&tri3.ux);

			AKSIMD_V4F32 u_x, u_y, u_z, u_d;
			AkMath::PermuteVectors4(u0, u1, u2, u3, u_x, u_y, u_z, u_d);

			const AKSIMD_V4F32 detu = AkMath::DotPoduct4_4x4(u_x, u_y, u_z, u_d, detp_x, detp_y, detp_z, det);

			// Review: what to do when detu == 0?
			AkUInt32 neq0_mask = 0xF & ~AKSIMD_MASK_V4F32(AKSIMD_EQ_V4F32(detu, AKSIMD_SETZERO_V4F32()));

			mask |= neq0_mask & AKSIMD_MASK_V4F32(AKSIMD_XOR_V4F32(detu, AKSIMD_SUB_V4F32(det, detu)));
			if (mask != 0xf)
			{
				const AKSIMD_V4F32 v0 = AKSIMD_LOAD_V4F32(&tri0.vx);
				const AKSIMD_V4F32 v1 = AKSIMD_LOAD_V4F32(&tri1.vx);
				const AKSIMD_V4F32 v2 = AKSIMD_LOAD_V4F32(&tri2.vx);
				const AKSIMD_V4F32 v3 = AKSIMD_LOAD_V4F32(&tri3.vx);

				AKSIMD_V4F32 v_x, v_y, v_z, v_d;
				AkMath::PermuteVectors4(v0, v1, v2, v3, v_x, v_y, v_z, v_d);

				const AKSIMD_V4F32 detv = AkMath::DotPoduct4_4x4(v_x, v_y, v_z, v_d, detp_x, detp_y, detp_z, det);

				mask |= AKSIMD_MASK_V4F32(AKSIMD_XOR_V4F32(detv, AKSIMD_SUB_V4F32(det, AKSIMD_ADD_V4F32(detu, detv))));
				if (mask != 0xf)
				{
					// The hit point is the same for all triangles, because we know they are co-planar
					const AKSIMD_V4F32 inv_det = AKSIMD_DIV_V4F32(vOne, det);
					const AKSIMD_V4F32 xxyy = AKSIMD_SHUFFLE_V4F32(AKSIMD_MUL_V4F32(detp_x, inv_det), AKSIMD_MUL_V4F32(detp_y, inv_det), AKSIMD_SHUFFLE(0, 0, 0, 0));
					const AKSIMD_V4F32 zzww = AKSIMD_SHUFFLE_V4F32(AKSIMD_MUL_V4F32(detp_z, inv_det), vOne, AKSIMD_SHUFFLE(0, 0, 0, 0));
					AKSIMD_STORE_V4F32(&out_hitPoint, AKSIMD_SHUFFLE_V4F32(xxyy, zzww, AKSIMD_SHUFFLE(2, 0, 2, 0)));

					if (mask != 0xf)
					{
						// The index of the hit triangle is given by the first zero in the mask.
						int idx = 0;
						while ((mask & 0x1) == 1)
						{
							mask = mask >> 1;
							++idx;
						}
						AKASSERT(idx < 4);

						return in_array[triIdx + idx];
					}
				}
			}
		}
	}

	return NULL;
}

template<typename tFilter = NoFilter>
struct OcclusionChecker
{
	OcclusionChecker(const AKSIMD_V4F32& in_origin, const AKSIMD_V4F32& in_dir, const tFilter& in_filter) :
		n_x(AKSIMD_SETZERO_V4F32()), n_y(AKSIMD_SETZERO_V4F32()), n_z(AKSIMD_SETZERO_V4F32()), n_w(AKSIMD_SETZERO_V4F32()),
		u_x(AKSIMD_SETZERO_V4F32()), u_y(AKSIMD_SETZERO_V4F32()), u_z(AKSIMD_SETZERO_V4F32()), u_d(AKSIMD_SETZERO_V4F32()),
		v_x(AKSIMD_SETZERO_V4F32()), v_y(AKSIMD_SETZERO_V4F32()), v_z(AKSIMD_SETZERO_V4F32()), v_d(AKSIMD_SETZERO_V4F32()),
		ray_origin(in_origin),
		ray_direction(in_dir),
		m_filter(in_filter),		
		uNumTri(0),
		bPathIsOccluded(false)
	{
	}

	bool IsOccluded()
	{
		if (uNumTri != 0)
		{
			bPathIsOccluded = IntersectTriangles() || bPathIsOccluded;
			uNumTri = 0;
		}

		return bPathIsOccluded;
	}

	AkForceInline bool Add(AkImageSourceTriangle* in_triangle)
	{
		AKASSERT(in_triangle != NULL);

		if (in_triangle && !m_filter.Filter(in_triangle))
		{
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

			pDarkTris[uNumTri] = &in_triangle->pPlane->m_PortalTriangles;

			++uNumTri;

			if (uNumTri == 4)
			{
				bPathIsOccluded = IntersectTriangles() || bPathIsOccluded;
				uNumTri = 0;
			}
		}

		return !bPathIsOccluded;
	}

protected:

	AKSIMD_V4F32 n_x, n_y, n_z, n_w;
	AKSIMD_V4F32 u_x, u_y, u_z, u_d;
	AKSIMD_V4F32 v_x, v_y, v_z, v_d;

	AKSIMD_V4F32 ray_origin;
	AKSIMD_V4F32 ray_direction;

	tFilter m_filter;
	
	AkUInt32 uNumTri;

	AkTriangleArray* pDarkTris[4];

	bool bPathIsOccluded;

	bool IntersectTriangles()
	{
		static const AKSIMD_V4F32 vEpsilon = AKSIMD_SET_V4F32(AK_EPSILON);

		const AKSIMD_V4F32 o_x = AKSIMD_SHUFFLE_V4F32(ray_origin, ray_origin, AKSIMD_SHUFFLE(0, 0, 0, 0));
		const AKSIMD_V4F32 o_y = AKSIMD_SHUFFLE_V4F32(ray_origin, ray_origin, AKSIMD_SHUFFLE(1, 1, 1, 1));
		const AKSIMD_V4F32 o_z = AKSIMD_SHUFFLE_V4F32(ray_origin, ray_origin, AKSIMD_SHUFFLE(2, 2, 2, 2));

		AKSIMD_V4F32 det = AkMath::DotPoduct3_1x4(ray_direction, n_x, n_y, n_z);

		// Contains 32 zeros for a false lane, 32 one for a true lane (4x lane)
		// 0 Means the det is correct
		// 1 Means the det is 0 so not correct => direction align with triangle plane
		AkUInt32 detMask = AKSIMD_MASK_V4F32(AKSIMD_LT_V4F32(AKSIMD_ABS_V4F32(det), vEpsilon));
		
		static const AKSIMD_V4F32 vNegOne = AKSIMD_SET_V4F32(-1.f);
		static const AKSIMD_V4F32 vOne = AKSIMD_SET_V4F32(1.f);
		const AKSIMD_V4F32 dett = AkMath::DotPoduct4_4x4(AKSIMD_MUL_V4F32(vNegOne, n_x), AKSIMD_MUL_V4F32(vNegOne, n_y), AKSIMD_MUL_V4F32(vNegOne, n_z), n_w,
			o_x, o_y, o_z, vOne);

		AKASSERT(uNumTri >= 1 && uNumTri <= 4);
		AkUInt32 validMask = 0xf >> (4 - uNumTri);

		AkUInt32 missMask = (validMask & AKSIMD_MASK_V4F32(AKSIMD_XOR_V4F32(dett, AKSIMD_SUB_V4F32(AKSIMD_MUL_V4F32(vOne, det), dett))));
		
		// We either miss if we miss the triangle or the determinant is not correct
		missMask = detMask | missMask;
	
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
					AkUInt32 hitMask = (~missMask & validMask);

					AkUInt32 idx = 0;
					do
					{
						if (((hitMask >> idx) & 1U) != 0)
						{
							AKSIMD_V4F32 unused;
							if (pDarkTris[idx] == NULL || !IntersectTriArray(*pDarkTris[idx], ray_origin, ray_direction, unused))
								return true;
						}
					} while (++idx < 4);

					return false;
				}
			}
		}

		return false;
	}
};

template<typename tFilter = NoFilter>
struct NearestOcclusionChecker
{
	NearestOcclusionChecker(const AKSIMD_V4F32& in_origin, const AKSIMD_V4F32& in_dir, const tFilter& in_filter) :
		n_x(AKSIMD_SETZERO_V4F32()), n_y(AKSIMD_SETZERO_V4F32()), n_z(AKSIMD_SETZERO_V4F32()), n_w(AKSIMD_SETZERO_V4F32()),
		u_x(AKSIMD_SETZERO_V4F32()), u_y(AKSIMD_SETZERO_V4F32()), u_z(AKSIMD_SETZERO_V4F32()), u_d(AKSIMD_SETZERO_V4F32()),
		v_x(AKSIMD_SETZERO_V4F32()), v_y(AKSIMD_SETZERO_V4F32()), v_z(AKSIMD_SETZERO_V4F32()), v_d(AKSIMD_SETZERO_V4F32()),
		ray_origin(in_origin),
		ray_direction(in_dir),
		m_filter(in_filter),		
		uNumTri(0),
		bPathIsOccluded(false),
		minDistance(FLT_MAX),
		nearestTriangle(nullptr)
	{
	}

	bool IsOccluded()
	{
		if (uNumTri != 0)
		{
			bPathIsOccluded = bPathIsOccluded || IntersectTriangles();
			uNumTri = 0;
		}

		return bPathIsOccluded;
	}

	AkForceInline bool Add(AkImageSourceTriangle* in_triangle)
	{
		AKASSERT(in_triangle != NULL);

		if (!m_filter.Filter(in_triangle))
		{
			triangleToProcess[uNumTri] = in_triangle;

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

			++uNumTri;

			if (uNumTri == 4)
			{
				bPathIsOccluded = IntersectTriangles() || bPathIsOccluded;
				uNumTri = 0;
			}
		}

		return !bPathIsOccluded;
	}

	bool FinalCheck()
	{
		if (uNumTri != 0)
		{
			bPathIsOccluded = IntersectTriangles() || bPathIsOccluded;
			uNumTri = 0;
		}

		return !bPathIsOccluded;
	}

	AkReal32 GetMinDistance() const
	{
		return minDistance;
	}

	AKSIMD_V4F32 getHitPoint() const
	{
		return hitPoint;
	}

	AkImageSourcePlane* getReflector() const
	{
		if (nearestTriangle)
		{
			return nearestTriangle->pPlane;
		}

		return nullptr;
	}

protected:

	AKSIMD_V4F32 n_x, n_y, n_z, n_w;
	AKSIMD_V4F32 u_x, u_y, u_z, u_d;
	AKSIMD_V4F32 v_x, v_y, v_z, v_d;

	AKSIMD_V4F32 ray_origin;
	AKSIMD_V4F32 ray_direction;

	tFilter m_filter;

	AkUInt32 uNumTri;

	bool bPathIsOccluded;

	/// Distance of the nearest triangle intersected
	///
	AkReal32 minDistance;

	/// Nearest triangle intersected
	///
	AkImageSourceTriangle* nearestTriangle;

	/// Triangles to process
	///
	AkImageSourceTriangle* triangleToProcess[4];

	/// Ray hit point if found (bPathIsOccluded must be true)
	///
	AKSIMD_V4F32 hitPoint;
	
	bool IntersectTriangles()
	{
		static const AKSIMD_V4F32 vEpsilon = AKSIMD_SET_V4F32(AK_EPSILON);

		const AKSIMD_V4F32 o_x = AKSIMD_SHUFFLE_V4F32(ray_origin, ray_origin, AKSIMD_SHUFFLE(0, 0, 0, 0));
		const AKSIMD_V4F32 o_y = AKSIMD_SHUFFLE_V4F32(ray_origin, ray_origin, AKSIMD_SHUFFLE(1, 1, 1, 1));
		const AKSIMD_V4F32 o_z = AKSIMD_SHUFFLE_V4F32(ray_origin, ray_origin, AKSIMD_SHUFFLE(2, 2, 2, 2));

		AKSIMD_V4F32 det = AkMath::DotPoduct3_1x4(ray_direction, n_x, n_y, n_z);

		// Contains 32 zeros for a false lane, 32 one for a true lane (4x lane)
		// 0 Means the det is correct
		// 1 Means the det is 0 so not correct => direction align with triangle plane
		AkUInt32 detMask = AKSIMD_MASK_V4F32(AKSIMD_LT_V4F32(AKSIMD_ABS_V4F32(det), vEpsilon));

		static const AKSIMD_V4F32 vNegOne = AKSIMD_SET_V4F32(-1.f);
		static const AKSIMD_V4F32 vOne = AKSIMD_SET_V4F32(1.f);
		const AKSIMD_V4F32 dett = AkMath::DotPoduct4_4x4(AKSIMD_MUL_V4F32(vNegOne, n_x), AKSIMD_MUL_V4F32(vNegOne, n_y), AKSIMD_MUL_V4F32(vNegOne, n_z), n_w,
			o_x, o_y, o_z, vOne);

		AKASSERT(uNumTri >= 1 && uNumTri <= 4);
		AkUInt32 validMask = 0xf >> (4 - uNumTri);

		AkUInt32 missMask = (validMask & AKSIMD_MASK_V4F32(AKSIMD_XOR_V4F32(dett, AKSIMD_SUB_V4F32(AKSIMD_MUL_V4F32(vOne, det), dett))));
		
		// We either miss if we miss the triangle or the determinant is not correct
		missMask = detMask | missMask;
		
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
					AkUInt32 hitMask = (~missMask & validMask);

					AKSIMD_V4F32 inv_det = AKSIMD_DIV_V4F32(AKSIMD_SET_V4F32(1.0), det);
					AKSIMD_V4F32 t = AKSIMD_MUL_V4F32(dett, inv_det);
				
					AkUInt32 idx = 0;
					do
					{
						if (((hitMask >> idx) & 1U) != 0)
						{
							AKSIMD_V4F32 unused;
							if (triangleToProcess[idx] == NULL || !IntersectTriArray(triangleToProcess[idx]->pPlane->m_PortalTriangles, ray_origin, ray_direction, unused))
							{
								AkReal32 distance = AkMin(AKSIMD_GETELEMENT_V4F32(t, idx), FLT_MAX);
								if (distance < minDistance)
								{
									minDistance = distance;
									nearestTriangle = triangleToProcess[idx];
									hitPoint = AKSIMD_ADD_V4F32(ray_origin, AKSIMD_MUL_V4F32(ray_direction, AKSIMD_SET_V4F32(minDistance)));								
								}							
							}
						}
					} while (++idx < 4);
		
					return nearestTriangle != nullptr;
				}
			}
		}

		return false;
	}
};

struct TransmissionOcclusionChecker
{
	TransmissionOcclusionChecker(const AKSIMD_V4F32& in_origin, const AKSIMD_V4F32& in_dir) :
		n_x(AKSIMD_SETZERO_V4F32()), n_y(AKSIMD_SETZERO_V4F32()), n_z(AKSIMD_SETZERO_V4F32()), n_w(AKSIMD_SETZERO_V4F32()),
		u_x(AKSIMD_SETZERO_V4F32()), u_y(AKSIMD_SETZERO_V4F32()), u_z(AKSIMD_SETZERO_V4F32()), u_d(AKSIMD_SETZERO_V4F32()),
		v_x(AKSIMD_SETZERO_V4F32()), v_y(AKSIMD_SETZERO_V4F32()), v_z(AKSIMD_SETZERO_V4F32()), v_d(AKSIMD_SETZERO_V4F32()),
		ray_origin(in_origin),
		ray_direction(in_dir),
		uNumTri(0),
		maxOcclusion(0.f),
		bPathIsOccluded(false)
	{
	}

	bool IsOccluded()
	{
		if (uNumTri != 0)
		{
			bPathIsOccluded = IntersectTriangles() || bPathIsOccluded;
			uNumTri = 0;
		}

		return bPathIsOccluded;
	}

	AkReal32 GetOcclusion()
	{
		if (uNumTri != 0)
		{
			bPathIsOccluded = IntersectTriangles() || bPathIsOccluded;
			uNumTri = 0;
		}

		return maxOcclusion;
	}

	AkForceInline bool Add(AkImageSourceTriangle* in_triangle)
	{
		AKASSERT(in_triangle != NULL);

		if (in_triangle)
		{
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

			pDarkTris[uNumTri] = &in_triangle->pPlane->m_PortalTriangles;
			occlusion[uNumTri] = in_triangle->GetAcousticSurface().occlusion;

			++uNumTri;

			if (uNumTri == 4)
			{
				bPathIsOccluded = IntersectTriangles() || bPathIsOccluded;
				uNumTri = 0;
			}
		}

		return true; // keep going to get max transmission value
	}

protected:

	AKSIMD_V4F32 n_x, n_y, n_z, n_w;
	AKSIMD_V4F32 u_x, u_y, u_z, u_d;
	AKSIMD_V4F32 v_x, v_y, v_z, v_d;

	AKSIMD_V4F32 ray_origin;
	AKSIMD_V4F32 ray_direction;

	AkUInt32 uNumTri;

	AkTriangleArray* pDarkTris[4];
	AkReal32 occlusion[4];

	AkReal32 maxOcclusion;
	bool bPathIsOccluded;

	bool IntersectTriangles()
	{
		static const AKSIMD_V4F32 vEpsilon = AKSIMD_SET_V4F32(AK_EPSILON);

		const AKSIMD_V4F32 o_x = AKSIMD_SHUFFLE_V4F32(ray_origin, ray_origin, AKSIMD_SHUFFLE(0, 0, 0, 0));
		const AKSIMD_V4F32 o_y = AKSIMD_SHUFFLE_V4F32(ray_origin, ray_origin, AKSIMD_SHUFFLE(1, 1, 1, 1));
		const AKSIMD_V4F32 o_z = AKSIMD_SHUFFLE_V4F32(ray_origin, ray_origin, AKSIMD_SHUFFLE(2, 2, 2, 2));

		AKSIMD_V4F32 det = AkMath::DotPoduct3_1x4(ray_direction, n_x, n_y, n_z);

		// Contains 32 zeros for a false lane, 32 one for a true lane (4x lane)
		// 0 Means the det is correct
		// 1 Means the det is 0 so not correct => direction align with triangle plane
		AkUInt32 detMask = AKSIMD_MASK_V4F32(AKSIMD_LT_V4F32(AKSIMD_ABS_V4F32(det), vEpsilon));

		static const AKSIMD_V4F32 vNegOne = AKSIMD_SET_V4F32(-1.f);
		static const AKSIMD_V4F32 vOne = AKSIMD_SET_V4F32(1.f);
		const AKSIMD_V4F32 dett = AkMath::DotPoduct4_4x4(AKSIMD_MUL_V4F32(vNegOne, n_x), AKSIMD_MUL_V4F32(vNegOne, n_y), AKSIMD_MUL_V4F32(vNegOne, n_z), n_w,
			o_x, o_y, o_z, vOne);

		AKASSERT(uNumTri >= 1 && uNumTri <= 4);
		AkUInt32 validMask = 0xf >> (4 - uNumTri);

		AkUInt32 missMask = (validMask & AKSIMD_MASK_V4F32(AKSIMD_XOR_V4F32(dett, AKSIMD_SUB_V4F32(AKSIMD_MUL_V4F32(vOne, det), dett))));

		// We either miss if we miss the triangle or the determinant is not correct
		missMask = detMask | missMask;

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
					bool bHit = false;

					AkUInt32 hitMask = (~missMask & validMask);

					AkUInt32 idx = 0;
					do
					{
						if (((hitMask >> idx) & 1U) != 0)
						{
							AKSIMD_V4F32 unused;
							if (pDarkTris[idx] == NULL || !IntersectTriArray(*pDarkTris[idx], ray_origin, ray_direction, unused))
							{
								bHit = true;
								maxOcclusion = AkMax(maxOcclusion, occlusion[idx]);
							}
						}
					} while (++idx < 4);

					return bHit;
				}
			}
		}

		return false;
	}
};

template<typename tNextFilter = NoFilter>
struct PlaneFilter
{
	PlaneFilter(const AkImageSourcePlane* in_pPlane) : pPlane(in_pPlane), filter(tNextFilter()) {}
	PlaneFilter(const AkImageSourcePlane* in_pPlane, const tNextFilter& in_filter) : pPlane(in_pPlane), filter(in_filter) {}

	bool Filter(AkImageSourceTriangle* in_triangle)
	{
		return in_triangle->pPlane == pPlane || filter.Filter(in_triangle);
	}

	const AkImageSourcePlane* pPlane;
	tNextFilter filter;
};

template<typename tNextFilter = NoFilter>
struct EdgeFilter
{
	EdgeFilter(const CAkDiffractionEdge* in_pEdge) : pEdge(in_pEdge), filter(tNextFilter()) {}
	EdgeFilter(const CAkDiffractionEdge* in_pEdge, const tNextFilter& in_filter) : pEdge(in_pEdge), filter(in_filter) {}

	bool Filter(AkImageSourceTriangle* in_triangle)
	{
		// Maybe add a point from edge to plane?
		for (AkEdgeArray::Iterator it = in_triangle->pPlane->m_Edges.Begin(); it != in_triangle->pPlane->m_Edges.End(); ++it)
		{
			if (*it == pEdge)
				return true;
		}

		return filter.Filter(in_triangle);
	}

	const CAkDiffractionEdge* pEdge;
	tNextFilter filter;
};

struct OnePlaneFilter : PlaneFilter<NoFilter>
{
	OnePlaneFilter(const AkImageSourcePlane* in_pPlane) : PlaneFilter<NoFilter>(in_pPlane) {}
};

struct TwoPlaneFilter : PlaneFilter< PlaneFilter<NoFilter> >
{
	TwoPlaneFilter(const AkImageSourcePlane* in_pPlane0, const AkImageSourcePlane* in_pPlane1) : PlaneFilter< PlaneFilter<NoFilter> >(in_pPlane0, PlaneFilter<NoFilter>(in_pPlane1) ) {}
};

struct PlaneEdgeFilter : PlaneFilter< EdgeFilter<NoFilter> >
{
	PlaneEdgeFilter(const AkImageSourcePlane* in_pPlane,
		const CAkDiffractionEdge* in_pEdge)
		:
		PlaneFilter< EdgeFilter<NoFilter> >(in_pPlane, EdgeFilter<NoFilter>(in_pEdge, NoFilter())) {}
};

struct OneEdgeFilter : EdgeFilter<NoFilter>
{
	OneEdgeFilter(const CAkDiffractionEdge* in_pEdge) : EdgeFilter<NoFilter>(in_pEdge) {}
};

struct TwoEdgeFilter : EdgeFilter< EdgeFilter<NoFilter> >
{
	TwoEdgeFilter(const CAkDiffractionEdge* in_pEdge0, const CAkDiffractionEdge* in_pEdge1) : EdgeFilter< EdgeFilter<NoFilter> >(in_pEdge0, EdgeFilter<NoFilter>(in_pEdge1)) {}
};
