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

#ifndef _AKIMAGESOURCE_H_
#define _AKIMAGESOURCE_H_

#include <AK/Tools/Common/AkVectors.h>
#include <AK/SoundEngine/Common/AkSimd.h>
#include <AK/SpatialAudio/Common/AkSpatialAudio.h>
#include "AkSpatialAudioPrivateTypes.h"

class CAkReflectInstance;
class AkImageSourcePlane;
class CAkDiffractionEdge;
class AkGeometrySet;
class CAkSpatialAudioListener;
class CAkSpatialAudioEmitter;
class AkSoundGeometry;
struct AkDiffractionPathSegment;

class AkImageSourceTriangle
{
public:
	AkImageSourceTriangle() : pPlane(NULL) {}

	/// Construct an AkImageSourceTriangle from three points and a material ID.
	AkImageSourceTriangle(const AkGeometrySet* in_geoSet):
		pGeoSet(in_geoSet),
		pPlane(NULL)
	{
	}

	AKRESULT Init(const Ak3DVector& point0, const Ak3DVector& point1, const Ak3DVector& point2)
	{
		Ak3DVector AB = point1 - point0;
		Ak3DVector AC = point2 - point0;

		Ak3DVector N = AB.Cross(AC);; // AB X AC

		nx = N.X;
		ny = N.Y;
		nz = N.Z;
		nd = N.Dot(point0);

		AkReal32 fSqrNorm = N.Dot(N);

		if (!AkMath::IsValidFloatInput(fSqrNorm))
			return AK_Fail; // May be infinite if points are too far apart.

		Ak3DVector N1 = AC.Cross(N) / fSqrNorm;
		ux = N1.X;
		uy = N1.Y;
		uz = N1.Z;
		ud = -1.f * N1.Dot(point0);

		AKASSERT( fabs( (N1.Dot(point1) + ud) - 1.f ) < AK_SA_EPSILON);

		Ak3DVector N2 = N.Cross(AB) / fSqrNorm;
		vx = N2.X;
		vy = N2.Y;
		vz = N2.Z;
		vd = -1.f * N2.Dot(point0);

		AKASSERT( fabs( (N2.Dot(point2) + vd) - 1.f ) < AK_SA_EPSILON);

		/*The barycentric coordinates are expressed as scaled distance
		from their respective planes. For the u coordinate, the side
		AC lies on such a plane and the plane’s normal vector N1 is
		scaled so that N1 ·B +d1 = 1. For the v coordinate, the plane
		is constructed similarly (side AB and vertex C). */

		return AK_Success;
	}

	Ak3DVector Project(const Ak3DVector& in_pos) const
	{
		Ak3DVector n(nx,ny,nz);
		AkReal32 len = n.Length();
		n = n / len;
		Ak3DVector p = n * (nd / len);
		return (in_pos + (n*(p - in_pos).Dot(n)));
	}

	// Precomputed triangle planes for Havel - Herout ray - triangle intersection algorithm.
	// Plane representing the face of the triangle
	AK_ALIGN_SIMD(
	AkReal32 nx);
	AkReal32 ny;
	AkReal32 nz;
	AkReal32 nd;

	// Plane representing the edges of the triangle, with the normals facing inward 
	//	and each scaled by the other edge.
	AK_ALIGN_SIMD(
	AkReal32 ux);
	AkReal32 uy;
	AkReal32 uz;
	AkReal32 ud;

	AK_ALIGN_SIMD(
	AkReal32 vx);
	AkReal32 vy;
	AkReal32 vz;
	AkReal32 vd;

	const AkGeometrySet* pGeoSet;
	AkImageSourcePlane* pPlane;

	const AkAcousticSurface& GetAcousticSurface() const;
	AkSurfIdx GetSurfIdx() const;
};

class AkImageSourcePlane
{
public:
	AkImageSourcePlane(const Ak3DVector& in_p0, const Ak3DVector& in_p1, const Ak3DVector& in_p2)
	{
		Ak3DVector N = (in_p1 - in_p0).Cross(in_p2 - in_p0);
		N.Normalize();
		nx = N.X;
		ny = N.Y;
		nz = N.Z;
		nd = N.Dot(in_p0);
	}

	~AkImageSourcePlane()
	{
		TermTriangles(m_Triangles);
		TermTriangles(m_PortalTriangles);
		m_Edges.Term();
	}

	bool IsCoplanar(const Ak3DVector& in_p0, const Ak3DVector& in_p1, const Ak3DVector& in_p2)
	{
		Ak3DVector e0 = in_p1 - in_p0;
		Ak3DVector e1 = in_p2 - in_p1;
		Ak3DVector e2 = in_p2 - in_p0;

		AkReal32 l0 = e0.LengthSquared();
		AkReal32 l1 = e1.LengthSquared();
		AkReal32 l2 = e2.LengthSquared();

		AkReal32 l = sqrtf( AkMax(l0, AkMax(l1, l2)) );

		AkReal32 minThickness = l * AK_SA_PLANE_THICKNESS_RATIO;

		Ak3DVector N = Normal();
		return	fabsf(N.Dot(in_p0) - nd) < minThickness &&
				fabsf(N.Dot(in_p1) - nd) < minThickness &&
				fabsf(N.Dot(in_p2) - nd) < minThickness;
	}

	bool AddTriangleIfCoplanar(AkImageSourceTriangle* in_pTriangle, const Ak3DVector& in_p0, const Ak3DVector& in_p1, const Ak3DVector& in_p2)
	{
		if (IsCoplanar(in_p0, in_p1, in_p2))
		{
			if (m_Triangles.AddLast(in_pTriangle) != NULL)
			{
				in_pTriangle->pPlane = this;
				return true;
			}
		}
		return false;
	}

	AkUInt32 RemoveTriangle(AkImageSourceTriangle* in_pTriangle)
	{
		for (AkTriangleArray::Iterator it = m_Triangles.Begin(); it != m_Triangles.End(); ++it)
		{
			if (*it == in_pTriangle)
			{
				m_Triangles.Erase(it);
				return m_Triangles.Length();
			}
		}
		AKASSERT(false);
		return m_Triangles.Length();
	}

	bool HasEdge(const CAkDiffractionEdge* in_pEdge) const 
	{
		for (AkEdgeArray::Iterator it = m_Edges.Begin(); it != m_Edges.End(); ++it)
		{
			if (*it == in_pEdge)
				return true;
		}
		return false;
	}

	void Transfer(AkImageSourcePlane& src)
	{
		nx = src.nx;
		ny = src.ny;
		ny = src.ny;
		nd = src.nd;

		m_Triangles.Transfer(src.m_Triangles);
		m_Edges.Transfer(src.m_Edges);
	}

	Ak3DVector Position() const
	{
		return Normal() * nd;
	}

	Ak3DVector Normal() const
	{
		return Ak3DVector(nx, ny, nz);
	}

	Ak3DVector Project(const Ak3DVector& in_pos) const
	{
		Ak3DVector n = Normal();
		return (in_pos + (n*(Position() - in_pos).Dot(n)));
	}

	// NormalV4F32() - returns <nx,ny,nz,nd> as a V4F32.
	const AKSIMD_V4F32& PlaneV4F32() const
	{
		return (AKSIMD_V4F32&)nx;
	}

	// Unit-length normal vector x,y,z
	AK_ALIGN_SIMD(
	AkReal32 nx);
	AkReal32 ny; 
	AkReal32 nz; 
	
	// D represents the signed distance, relative to the normal, from 
	//	the origin to the closest point on the plane
	AkReal32 nd;

	// Set of coplanar triangles.
	AkTriangleArray m_Triangles;
	
	AkTriangleArray m_PortalTriangles;

	// Set of connected edges.
	AkEdgeArray m_Edges;

private:
	void TermTriangles(AkTriangleArray& in_array)
	{
		for (AkTriangleArray::Iterator it = in_array.Begin(); it != in_array.End(); ++it)
		{
			AkImageSourceTriangle* pTri = *it;
			AKASSERT(pTri->pPlane == this);
			pTri->pPlane = NULL; //nullify plane reference.
		}

		in_array.Term();
	}
};

struct AkReflectionPath
{
	AkReflectionPath() { Init(0); }
	AkReflectionPath(AkUInt32 in_numPathPoints) { Init(in_numPathPoints); }

	bool AccumulateDiffraction(AkReal32 in_val)
	{
		return AkDiffractionAccum(totalDiffraction, in_val);
	}

	void Init(AkUInt32 in_numPathPoints)
	{
		numPathPoints = in_numPathPoints;
		totalDiffraction = 0.f;
		level = 1.0f;

		for (int i = 0; i < AK_MAX_REFLECTION_PATH_LENGTH; ++i)
		{
			pathPoint[i] = AKSIMD_SETZERO_V4F32();
			surfaces[i] = NULL;
			diffraction[i] = 0.f;
		}
	}

	// Recalculate the image source, which may have moved due to diffraction or listener/emitter movement between path updates.
	Ak3DVector CalcImageSource(const Ak3DVector& in_emitter, const Ak3DVector& in_listener);

	// Add an additional diffraction point (in_diffractionPt) at the listener's side (end) of the path, using in_listener as a reference for calculating the diffraction value.	
	bool AddPoint(Ak3DVector in_diffractionPt, const Ak3DVector& in_listener);

	bool AddPointFromEmitter(Ak3DVector in_diffractionPt, const Ak3DVector& in_emitter);

	bool AppendPathSegment(AkAcousticPortal* pPortal, const AkDiffractionPathSegment& diffractionPath, const Ak3DVector& listenerPos, bool bReversePath);

	Ak3DVector PointClosestToListener() { return Ak3DVector(pathPoint[numPathPoints - 1]); }

	/// Compute the total path length
	AkReal32 ComputePathLength(const Ak3DVector& i_listener, const Ak3DVector& i_emitter) const;

	// Returns the the number of path points, not including shadow-zone diffraction points. 
	AkUInt32 GetNumReflections() const;

	// Add a contributing path item (reflector, edge, portal, etc) to the pathID hash.
	template<typename T>
	void AugmentPathID(T in_pathContributor)
	{
		AK::FNVHash32 hash(pathID);
		hash.Compute(&in_pathContributor, sizeof(in_pathContributor));
		pathID = hash.Get();
	}

	// Point on the reflector wall where the sound bounces off of.
	/// pathPoint[0] is closest to the emitter, pathPoint[numPathPoints-1] is closest to the listener.
	AK_ALIGN_SIMD(AKSIMD_V4F32 pathPoint[AK_MAX_REFLECTION_PATH_LENGTH]);

	// Triangle that that sound bounces off of.
	const AkAcousticSurface * surfaces[AK_MAX_REFLECTION_PATH_LENGTH];

	AkReal32 diffraction[AK_MAX_REFLECTION_PATH_LENGTH];

	AkReal32 totalDiffraction;

	// Gain factor used to taper specular reflections due to limits on view zone diffraction range.
	AkReal32 level;

	AkUInt32 numPathPoints;

	// Path ID is a hash of the reflectors and diffraction edges that contribute to the paths. 
	// It is used to differentiate image sources in the reflect plugin.
	AkUInt32 pathID;
};

class CAkReflectionPaths : public AkArray<AkReflectionPath, const AkReflectionPath&, ArrayPoolSpatialAudioSIMD>
{
public:

	void RemoveAll()
	{
		AkArray<AkReflectionPath, const AkReflectionPath&, ArrayPoolSpatialAudioSIMD>::RemoveAll();
		transitionIdx = (AkUInt32)-1;
	}

	void Transfer(CAkReflectionPaths& in_rSource)
	{
		AkArray<AkReflectionPath, const AkReflectionPath&, ArrayPoolSpatialAudioSIMD>::Transfer(in_rSource);
		transitionIdx = in_rSource.transitionIdx;
	}

	void MarkTransitionIdx()
	{
		transitionIdx = Length();
	}

	// Return the fade [0,1] for reflection index 'in_reflectionIdx' based on emitters current portal transition state.
	AkReal32 GetPortalFade(AkUInt32 in_reflectionIdx, const CAkSpatialAudioEmitter* in_pEmitter) const;

	// Get virtual sources to set to Reflect.
	void GetVirtualSources(CAkReflectInstance& out_reflect, const CAkSpatialAudioListener* in_pListener, const CAkSpatialAudioEmitter* in_pEmitter) const;

	// Marks the index of the transition point. Entries < transitionIdx are from the emitter's active room, and entries >= transitionIdx are from the emitter's transition room.
	AkUInt32 transitionIdx;
};


#endif // _AKIMAGESOURCE_H_