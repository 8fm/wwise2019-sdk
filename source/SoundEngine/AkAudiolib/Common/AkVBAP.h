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

#ifndef _AkVBAP_H_
#define _AkVBAP_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkSpeakerVolumes.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>
#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkVectors.h>

//#define AKVBAP_DEBUG 1
#define AKVBAP_FILTEROPTIM 1
#define AKVBAP_CIRCUMCIRCLE 1
#define AKVBAP_OPTIMBELOW 1
struct AkCircumcircle
{
	//-----------------------------------------------------------
	// Basic operators
	bool PointInCircle(const Ak2DVector& p) const
	{
		// http://stackoverflow.com/questions/481144/equation-for-testing-if-a-point-is-inside-a-circle
		const AkReal32 POSITIVE_TEST_EPSILON = 0.05f;
		const AkReal32 dx = (AkReal32)fabs(center.X - p.X);
		const AkReal32 dy = (AkReal32)fabs(center.Y - p.Y);

		if (dx + dy <= radius)
			return true;
		if (dx > radius)
			return false;
		if (dy > radius)
			return false;
		if (dx *dx + dy * dy <= (radius + POSITIVE_TEST_EPSILON)*(radius + POSITIVE_TEST_EPSILON))
			return true;
		else
			return false;
		
		//return (center.X - p.X)*(center.X - p.X) + (center.Y - p.Y)*(center.Y - p.Y) < (radius*radius + POSITIVE_TEST_EPSILON);
	}
	Ak2DVector center;
	AkReal32 radius;
};

// Contains the indexes for three points. Every slice forms a triangle of surrounding outputs for the VBAP algorithm
struct AkVBAPTriplet
{
	//-----------------------------------------------------------
	// Constructor/Destructor functions
	/*bool operator==(const AkVBAPTriplet& b) const
	{
		bool equal = true;

		equal = equal && (b.A == A || b.A == B || b.A == C);
		equal = equal && (b.B == A || b.B == B || b.B == C);
		equal = equal && (b.C == A || b.C == B || b.C == C);

		return equal;
	}

	bool operator!=(const AkVBAPTriplet& b) const
	{
		bool equal = true;

		equal = equal && (b.A == A || b.A == B || b.A == C);
		equal = equal && (b.B == A || b.B == B || b.B == C);
		equal = equal && (b.C == A || b.C == B || b.C == C);

		return !equal;
	}*/

	AkForceInline void Set( 
		AkUInt32				in_A, 
		AkUInt32				in_B, 
		AkUInt32				in_C )
	{
		A = in_A;
		B = in_B;
		C = in_C;

		// Initialise bounding box to large values.
		// We're using spherical coordinates so values are garantied to be [-PI, PI]
#ifndef AKVBAP_CIRCUMCIRCLE
		m_BoundingBoxMin.X = TWOPI;
		m_BoundingBoxMin.Y = TWOPI;
		m_BoundingBoxMax.X = -TWOPI;
		m_BoundingBoxMax.Y = -TWOPI;
#endif
	}
	
	// The bounding box optimise finding is a point lies in the triangle. If it does, furthur calculations are worth it.
	AkForceInline void UpdateBoundingBox( const Ak2DVector& v )
	{
#ifndef AKVBAP_CIRCUMCIRCLE
		const AkReal32 BOX_EPS = 0.1;
		if (v.X < m_BoundingBoxMin.X)
		{
			m_BoundingBoxMin.X = v.X - BOX_EPS;
		}

		if (v.Y < m_BoundingBoxMin.Y)
		{
			m_BoundingBoxMin.Y = v.Y - BOX_EPS;
		}

		if (v.X > m_BoundingBoxMax.X)
		{
			m_BoundingBoxMax.X = v.X + BOX_EPS;
		}

		if (v.Y > m_BoundingBoxMax.Y)
		{
			m_BoundingBoxMax.Y = v.Y + BOX_EPS;
		}
#endif
	}

	AkForceInline bool IsPointInsideBoundingBox(const Ak2DVector& in_Source) const
	{
#ifdef AKVBAP_CIRCUMCIRCLE
		return m_Circumcircle.PointInCircle(in_Source);
#else
		return (in_Source.X >= m_BoundingBoxMin.X &&
			in_Source.Y >= m_BoundingBoxMin.Y &&
			in_Source.X <= m_BoundingBoxMax.X &&
			in_Source.Y <= m_BoundingBoxMax.Y );
#endif
	}
	
#ifdef AKVBAP_CIRCUMCIRCLE
	void SetCircumcircle(
		const Ak2DVector&				A,
		const Ak2DVector&				B,
		const Ak2DVector&				C)
	{
		// http://mathworld.wolfram.com/Circumcircle.html
		AkReal32 a = Ak3DVector::Determinant(
			Ak3DVector(A.X, A.Y, 1.f),
			Ak3DVector(B.X, B.Y, 1.f),
			Ak3DVector(C.X, C.Y, 1.f)
			);

		AkReal32 c = -1.f * Ak3DVector::Determinant(
			Ak3DVector(A.X*A.X + A.Y*A.Y, A.X, A.Y),
			Ak3DVector(B.X*B.X + B.Y*B.Y, B.X, B.Y),
			Ak3DVector(C.X*C.X + C.Y*C.Y, C.X, C.Y)
			);

		AkReal32 bx = -1.f * Ak3DVector::Determinant(
			Ak3DVector(A.X*A.X + A.Y*A.Y, A.Y, 1.f),
			Ak3DVector(B.X*B.X + B.Y*B.Y, B.Y, 1.f),
			Ak3DVector(C.X*C.X + C.Y*C.Y, C.Y, 1.f)
			);

		AkReal32 by = Ak3DVector::Determinant(
			Ak3DVector(A.X*A.X + A.Y*A.Y, A.X, 1.f),
			Ak3DVector(B.X*B.X + B.Y*B.Y, B.X, 1.f),
			Ak3DVector(C.X*C.X + C.Y*C.Y, C.X, 1.f)
			);

		m_Circumcircle.center.X = (-1.f * bx) / (2.f * a);
		m_Circumcircle.center.Y = (-1.f * by) / (2.f * a);

		m_Circumcircle.radius = sqrtf(bx*bx + by*by - (4 * a * c)) / (2.f*(AkReal32)fabs(a));

	}
#endif

	// index of vertices 
	AkUInt32						A;
	AkUInt32						B;
	AkUInt32						C;

#ifdef AKVBAP_CIRCUMCIRCLE
	AkCircumcircle					m_Circumcircle;
#else
	Ak2DVector						m_BoundingBoxMin;
	Ak2DVector						m_BoundingBoxMax;
#endif
};


class AkVBAPTripletStackArray
{
public:
	AkVBAPTripletStackArray():
		m_Array(NULL),
		m_uSize(0),
		m_uCount(0)
	{}
	~AkVBAPTripletStackArray(){};

	void Set(AkVBAPTriplet* in_Array, AkUInt32 in_uSize)
	{
		m_Array = in_Array;
		m_uSize = in_uSize;
	}

	void Add(AkVBAPTriplet in_VBAPTriplet)
	{
		AKASSERT(m_uCount < m_uSize);
		m_Array[m_uCount++] = in_VBAPTriplet;
	}

	AkVBAPTriplet* AddLast()
	{
		AKASSERT(m_uCount < m_uSize);
		return &m_Array[m_uCount++];
	}

	bool IsEmpty() const
	{
		return (m_uSize == 0);
	}

	AkUInt32 GetCount() const
	{
		return m_uCount;
	}

	AkUInt32 GetSize() const
	{
		return m_uSize;
	}

	AkVBAPTriplet* GetAt(AkUInt32 in_index)
	{
		AKASSERT(in_index < m_uSize);
		return &m_Array[in_index];
	}

private:
	AkVBAPTriplet* m_Array;
	AkUInt32 m_uSize;
	AkUInt32 m_uCount;
};

typedef AkArray<AkVBAPTriplet, const AkVBAPTriplet&, ArrayPoolLEngineDefault> AkVBAPTripletArray;

class AkVBAPMap
{
public:
	//-----------------------------------------------------------
	// Constructor/Destructor functions
	AkVBAPMap();
    ~AkVBAPMap();

	//-----------------------------------------------------------
	// Allocate necessary ressources
	AKRESULT Allocate( 
		AK::IAkPluginMemAlloc*		in_pAllocator, 
		const AkUInt32				in_NbPoints );

	void Term( 
		AK::IAkPluginMemAlloc*		in_pAllocator );
	
	//-----------------------------------------------------------
	// Converts a set of points into a set of VBAP triangles and store it for future use with virtual sources points
	// Points are in spherical coordinates
	void PointsToVBAPMap( 
		const AkSphericalCoord*		in_SphericalPositions, 
		const AkUInt32				in_NbPoints );

	void ComputeVBAP(	
		AkReal32					in_fAngleAzimuthal,			///< Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are clockwise)
		AkReal32					in_fAngleElevation,			///< Incident angle, in radians [-pi/2,pi/2], where 0 is the elevation (positive values are above plane)
		AkUInt32					in_uNumChannels,
		AK::SpeakerVolumes::VectorPtr out_vVolumes );

	void ComputeVBAPSquared(
		AkReal32					in_fAngleAzimuthal,			///< Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are clockwise)
		AkReal32					in_fAngleElevation,			///< Incident angle, in radians [-pi/2,pi/2], where 0 is the elevation (positive values are above plane)
		AkUInt32					in_uNumChannels,
		AK::SpeakerVolumes::VectorPtr out_vVolumes);

	// Cartesian override: here the system is right handed with X pointing forward, Y to the left and Z to the top.
	void ComputeVBAPSquared(
		AkReal32						x,
		AkReal32						y,
		AkReal32						z,
		AkUInt32						in_uNumChannels,
		AK::SpeakerVolumes::VectorPtr	out_vVolumes);

	void NormalizePreSquaredGain(
		AkUInt32					in_uNumChannels,
		AK::SpeakerVolumes::VectorPtr out_vVolumes );

private:
	//-----------------------------------------------------------
	// Triangulation functions
	void RunTriangulation(
		const Ak2DVector*			in_Vertices,
		AkVBAPTripletArray*			in_PreviousVBAPSlices, 
#ifdef AKVBAP_OPTIMBELOW
		AkVBAPTripletArray*			in_PreviousVBAPSlicesBelow,
#endif
		AkVBAPTripletArray*			out_VBAPSlices
#ifdef AKVBAP_OPTIMBELOW
		, AkVBAPTripletArray*		out_VBAPSlicesBelow
#endif
#ifdef AKVBAP_DEBUG
		, int							in_debugId
#endif
		);

	bool TriangleExistsInList(
		int							a, 
		int							b, 
		int							c, 
		AkVBAPTripletArray*			in_VBAPSlices) const;

	bool FindPointInTriangle(
		const Ak2DVector*			in_Vertices, 
		int							a, 
		int							b, 
		int							c) const;

	AkReal32 ArePointsCCW(
		const Ak2DVector&			A, 
		const Ak2DVector&			B, 
		const Ak2DVector&			C) const;

	bool IsPointInCircle(
		const Ak2DVector&			P, 
		const Ak2DVector&			A, 
		const Ak2DVector&			B, 
		const Ak2DVector&			C) const;

	bool IsPointInTriangle(
		const Ak2DVector&			P,
		const Ak2DVector&			A,
		const Ak2DVector&			B, 
		const Ak2DVector&			C) const;

	bool IsPointInsideBoundingBox(const Ak2DVector& in_Source) const;

	void AddPointsInArea(
		const Ak2DVector&			in_Source, 
		const AkVBAPTripletArray&	in_VBAPSlices, 
		const Ak2DVector*			in_m_Vertice, 
		AkVBAPTripletStackArray*	out_resultList) const;

	void FindTriangle(
		const Ak3DVector&			in_Source, 
		AkReal32					in_fAngleAzimuthal,	
		AkReal32					in_fAngleElevation,
		AkVBAPTripletStackArray*	out_resultList);

	bool ComputeVBAP2D(
		AkVBAPTriplet*				in_SpeakerTriangle,
		Ak3DVector					in_cartSource,
		AK::SpeakerVolumes::VectorPtr out_vVolumes);

	bool ComputeVBAP3D(
		AkVBAPTriplet*				in_SpeakerTriangle,
		Ak3DVector					in_cartSource,
		AK::SpeakerVolumes::VectorPtr out_vVolumes);

	AkForceInline bool IsTriplet2D(
		AkVBAPTriplet*				in_SpeakerTriangle) const
	{
		return (in_SpeakerTriangle->A == m_VerticesCount - 1 || in_SpeakerTriangle->B == m_VerticesCount - 1 || in_SpeakerTriangle->C == m_VerticesCount - 1);
	}

	AkForceInline AkReal32 sign(
		Ak2DVector					p1,
		Ak2DVector					p2, 
		Ak2DVector					p3) const
	{
		return (p1.X - p3.X) * (p2.Y - p3.Y) - (p2.X - p3.X) * (p1.Y - p3.Y);
	}

	AkForceInline AkReal32 DotProduct(
		const Ak3DVector&			a, 
		const Ak3DVector&			b) const
	{
		return a.X*b.X + a.Y*b.Y + a.Z*b.Z;
	}

	AkForceInline AkReal32 DotProduct(
		const Ak2DVector&			a, 
		const Ak2DVector&			b) const
	{
		return a.X*b.X + a.Y*b.Y;
	}
	
	AkUInt32						m_VerticesCount; // m_VBAPSlicesCount = m_VerticesCount - 2

	AkVBAPTripletArray				m_VBAPSlicesHullA;  // triangle list, East
	AkVBAPTripletArray				m_VBAPSlicesHullB; // West

#ifdef AKVBAP_OPTIMBELOW
	AkVBAPTripletArray				m_VBAPSlicesHullABelow; // triangle list, East
	AkVBAPTripletArray				m_VBAPSlicesHullBBelow; // West
#endif

	Ak3DVector*						m_Vertice3D;
	// Spherical coordinate (radius, theta, phi) with a fixed radius. Therefore, (x, y) -> (theta, phi)
	Ak2DVector*						m_Vertice2DHullA; // East 2D spherical coordinate (on a sphere, radius removed)
	Ak2DVector*						m_Vertice2DHullB; // " west "
};

//-----------------------------------------------------------------------------
// Name: class CAkVBAP
//-----------------------------------------------------------------------------
class AkVBAP
{
public:
	// Constructor/destructor
    AkVBAP();
    ~AkVBAP();

	static AkVBAPMap* AllocateVBAPMap( 
		AK::IAkPluginMemAlloc*		in_pAllocator, 
		const AkUInt32				in_NbPoints )
	{
		AkVBAPMap* vBAPMap;
		vBAPMap = (AkVBAPMap*)AK_PLUGIN_ALLOC( 
			in_pAllocator, 
			sizeof(AkVBAPMap) ); 

		if (vBAPMap)
		{
			memset( vBAPMap, 0, sizeof(AkVBAPMap) );
			AKRESULT res = vBAPMap->Allocate(
				in_pAllocator, 
				in_NbPoints );

			if (res != AK_Success)
			{
				AK_PLUGIN_FREE(in_pAllocator, vBAPMap);
				vBAPMap = NULL;
			}
		}

		return vBAPMap;
	}
	
	static void Term(  
		AK::IAkPluginMemAlloc*		in_pAllocator, 
		void*						in_pPannerData )
	{	
		AKASSERT(in_pPannerData);
		AkVBAPMap* pVBAPMap = static_cast<AkVBAPMap*>(in_pPannerData);
		pVBAPMap->Term(in_pAllocator);
		AK_PLUGIN_FREE( in_pAllocator, pVBAPMap );
		pVBAPMap = NULL;
	}

	static void PushVertices( 
		AK::IAkPluginMemAlloc*		in_pAllocator, 
		const AkSphericalCoord*		in_SphericalPositions, 
		const AkUInt32				in_NbPoints, 
		void *&						out_pPannerData )
	{
		AkVBAPMap* pVBAPMap = AllocateVBAPMap(
			in_pAllocator, 
			in_NbPoints);

		if (pVBAPMap)
		{
			pVBAPMap->PointsToVBAPMap(
			in_SphericalPositions,
			in_NbPoints);
		}

		out_pPannerData = static_cast<void*>(pVBAPMap);
	}

	static void ComputeVBAP(	
		void*						in_pPannerData,
		AkReal32					in_fAngleAzimuthal,			///< Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are clockwise)
		AkReal32					in_fAngleElevation,			///< Incident angle, in radians [-pi/2,pi/2], where 0 is the elevation (positive values are above plane)
		AkUInt32					in_uNumChannels,
		AK::SpeakerVolumes::VectorPtr out_vVolumes )
	{
		AKASSERT(in_pPannerData);

		AkVBAPMap* pVBAPMap = static_cast<AkVBAPMap*>(in_pPannerData);
		pVBAPMap->ComputeVBAP(
			in_fAngleAzimuthal, 
			in_fAngleElevation,
			in_uNumChannels,
			out_vVolumes);
	}

	static void ComputeVBAPSquared(
		void*						in_pPannerData,
		AkReal32					in_fAngleAzimuthal,			///< Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are clockwise)
		AkReal32					in_fAngleElevation,			///< Incident angle, in radians [-pi/2,pi/2], where 0 is the elevation (positive values are above plane)
		AkUInt32					in_uNumChannels,
		AK::SpeakerVolumes::VectorPtr out_vVolumes)
	{
		AKASSERT(in_pPannerData);

		AkVBAPMap* pVBAPMap = static_cast<AkVBAPMap*>(in_pPannerData);
		pVBAPMap->ComputeVBAPSquared(
			in_fAngleAzimuthal,
			in_fAngleElevation,
			in_uNumChannels,
			out_vVolumes);
	}

	// Cartesian override: here the system is right handed with X pointing forward, Y to the left and Z to the top.
	static void ComputeVBAPSquared(
		void*						in_pPannerData,
		AkReal32						x,
		AkReal32						y,
		AkReal32						z,
		AkUInt32					in_uNumChannels,
		AK::SpeakerVolumes::VectorPtr out_vVolumes)
	{
		AKASSERT(in_pPannerData);

		AkVBAPMap* pVBAPMap = static_cast<AkVBAPMap*>(in_pPannerData);
		pVBAPMap->ComputeVBAPSquared(
			x,
			y,
			z,
			in_uNumChannels,
			out_vVolumes);
	}

	static void NormalizePreSquaredGain(
		void*						in_pPannerData,
		AkUInt32					in_uNumChannels,
		AK::SpeakerVolumes::VectorPtr out_vVolumes )
	{
		AKASSERT(in_pPannerData);

		AkVBAPMap* pVBAPMap = static_cast<AkVBAPMap*>(in_pPannerData);
		pVBAPMap->NormalizePreSquaredGain(
			in_uNumChannels,
			out_vVolumes);
	}
	
private:

};

#endif // _AkVBAP_H_
