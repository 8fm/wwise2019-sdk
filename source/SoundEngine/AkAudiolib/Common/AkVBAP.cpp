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
#include "AkVBAP.h"
#include "AkMath.h"                                   // for AK_EPSILON, PIOVERTWO

// Constructor.
AkVBAPMap::AkVBAPMap():
	m_VerticesCount(0)
	, m_Vertice3D(NULL)
	, m_Vertice2DHullA(NULL)
	, m_Vertice2DHullB(NULL)
{
}

// Destructor.
AkVBAPMap::~AkVBAPMap()
{
}

void AkVBAPMap::RunTriangulation(
	const Ak2DVector*				in_V, 
	AkVBAPTripletArray*				in_PreviousVBAPSlices,
#ifdef AKVBAP_OPTIMBELOW
	AkVBAPTripletArray*				in_PreviousVBAPSlicesBelow,
#endif
	AkVBAPTripletArray*				out_VBAPSlices
#ifdef AKVBAP_OPTIMBELOW
	, AkVBAPTripletArray*			out_VBAPSlicesBelow
#endif
#ifdef AKVBAP_DEBUG
	, int							in_debugId
#endif
	)
{	
	// http://en.wikipedia.org/wiki/Delaunay_triangulation
	// A triangle is added only if no points can be found inside the outercircle formed by its three points.

#ifdef AKVBAP_DEBUG
	char msg[256];
	sprintf(msg, "cart%i = [\n", in_debugId);
	AKPLATFORM::OutputDebugMsg( msg );
	for (AkUInt32 i = 0; i < m_VerticesCount; i++)
	{
		//sprintf( msg , "%0.2f\t %0.2f\n", in_V[i].X, in_V[i].Y );
		sprintf(msg, "%0.2f, %0.2f, %0.2f;\n", m_Vertice3D[i].X, m_Vertice3D[i].Y, m_Vertice3D[i].Z);
		
		AKPLATFORM::OutputDebugMsg( msg );
	}
	sprintf(msg, "]; \n");
	AKPLATFORM::OutputDebugMsg( msg );

//	char msg[256];
	sprintf(msg, "twod%i = [\n", in_debugId);
	AKPLATFORM::OutputDebugMsg( msg );
	for (AkUInt32 i = 0; i < m_VerticesCount; i++)
	{
		//sprintf( msg , "%0.2f\t %0.2f\n", in_V[i].X, in_V[i].Y );
		sprintf( msg , "%0.2f, %0.2f;\n", in_V[i].X, in_V[i].Y );
		AKPLATFORM::OutputDebugMsg( msg );
	}
	sprintf(msg, "]; \n");
	AKPLATFORM::OutputDebugMsg( msg );

	sprintf(msg, "triangles%i = [\n", in_debugId);
	AKPLATFORM::OutputDebugMsg( msg );
#endif
	// test every 3 points combinations
	for (AkUInt32 i = 0; i < m_VerticesCount-2; i++)
	{
		for (AkUInt32 j = i+1; j < m_VerticesCount-1; j++)
		{
			for (AkUInt32 k = j+1; k < m_VerticesCount; k++)
			{
				AkVBAPTripletArray* previousList = in_PreviousVBAPSlices;
				AkVBAPTripletArray* currentList = out_VBAPSlices;
#ifdef AKVBAP_OPTIMBELOW
				if (i == m_VerticesCount - 1 || j == m_VerticesCount - 1 || k == m_VerticesCount - 1)
				{
					previousList = in_PreviousVBAPSlicesBelow;
					currentList = out_VBAPSlicesBelow;
				}
#endif
				if (!FindPointInTriangle(in_V, i, j, k) && !TriangleExistsInList(i, j, k, previousList))
				{
					AkVBAPTriplet* triangle = NULL;
					triangle = currentList->AddLast();
					triangle->Set(i, j, k);
					triangle->UpdateBoundingBox(in_V[i]);
					triangle->UpdateBoundingBox(in_V[j]);
					triangle->UpdateBoundingBox(in_V[k]);

#ifdef AKVBAP_CIRCUMCIRCLE
					triangle->SetCircumcircle(in_V[i], in_V[j], in_V[k]);
#endif
#ifdef AKVBAP_DEBUG
					sprintf( msg , "%i, %i, %i; \n", i, j, k );
					AKPLATFORM::OutputDebugMsg( msg );
#endif
				}
			}
		}
	}

#ifdef AKVBAP_DEBUG
	sprintf(msg, "]; \n");
	AKPLATFORM::OutputDebugMsg( msg );
#endif
}

bool AkVBAPMap::TriangleExistsInList(
	int								a, 
	int								b, 
	int								c, 
	AkVBAPTripletArray*				in_VBAPSlices) const
{
	if (!in_VBAPSlices)
		return false;

	bool found = false;

	// Does not work for bounding box
#ifdef AKVBAP_CIRCUMCIRCLE
	// 
	for (AkVBAPTripletArray::Iterator it = in_VBAPSlices->Begin(); it != in_VBAPSlices->End() && !found; ++it)
	{
		if (a == (*it).A && b == (*it).B && c == (*it).C)
		{
			found = true;
		}
	}
#endif
	return found;
}

bool AkVBAPMap::FindPointInTriangle(
	const Ak2DVector*				in_Vertices, 
	int								a, 
	int								b, 
	int								c) const
{
	// Points need to be in conter-clockwize order, swap two points if needed
	AkReal32 ccwDeterminant = ArePointsCCW(
		in_Vertices[a], 
		in_Vertices[b], 
		in_Vertices[c]);

	if (ccwDeterminant == 0.0f)
	{
		// Points are colinear, reject triangle
		return true;
	}
	else if (ccwDeterminant < 0.0f)
	{
		int itemp = b;
		b = c;
		c = itemp;
	}

	bool in = false;
	// Return as soon as a point inside the circumscribed is found
	for (AkUInt32 i = 0; i < m_VerticesCount && !in; i++)
	{
		if(i != a && i != b && i != c)
		{
			in = IsPointInCircle(
				in_Vertices[i], 
				in_Vertices[a], 
				in_Vertices[b], 
				in_Vertices[c]);
		}
	}

	return in;
}

AkReal32 AkVBAPMap::ArePointsCCW(
	const Ak2DVector&				A,
	const Ak2DVector&				B, 
	const Ak2DVector&				C) const
{	
	// http://en.wikipedia.org/wiki/Graham_scan#Pseudocode
	// Three points are a counter-clockwise turn if ccw > 0, clockwise if
	// ccw < 0, and collinear if ccw = 0 because ccw is a determinant that
	// gives the signed area of the triangle formed by p1, p2 and p3.

	return (B.X - A.X) * (C.Y - A.Y)  -
					(B.Y - A.Y) * (C.X - A.X);
}

const AkReal32 AK_VBAP_EPSILON = 0.01;
bool AkVBAPMap::IsPointInCircle(
	const Ak2DVector&				P, 
	const Ak2DVector&				A, 
	const Ak2DVector&				B, 
	const Ak2DVector&				C) const
{
	// http://en.wikipedia.org/wiki/Delaunay_triangulation#Algorithms

	// When A, B and C are sorted in a counterclockwise order, this determinant is positive if and only if D lies inside the circumcircle.
	//				|	Ax - Dx,			Ay - Dy,		(Ax^2 - Dx^2) + (Ay^2 - Dy^2)	|
	//				|	Bx - Dx,			By - Dy,		(Bx^2 - Bx^2) + (By^2 - Dy^2)	|
	//				|	Cx - Dx,			Cy - Dy,		(Cx^2 - Cx^2) + (Cy^2 - Dy^2)	|
	

	Ak3DVector a( A.X - P.X,	A.Y - P.Y,		A.X*A.X - P.X*P.X +	A.Y*A.Y - P.Y*P.Y );
	Ak3DVector b( B.X - P.X,	B.Y - P.Y,		B.X*B.X - P.X*P.X + B.Y*B.Y - P.Y*P.Y );
	Ak3DVector c( C.X - P.X,	C.Y - P.Y,		C.X*C.X - P.X*P.X +	C.Y*C.Y - P.Y*P.Y );
	
	AkReal32 det = Ak3DVector::Determinant(a, b, c);
	/*if (det <= EPSILON && det >= -EPSILON) //[-EPSILON, EPSILON]
	{
		// In this case reject the triangle of point D would be a better candidate. ie: 
		// the point is on circumcircle
		return A.X > D.X; // resolve by favoring the 
	}*/

	return det > AK_VBAP_EPSILON;
}

bool AkVBAPMap::IsPointInTriangle(
	const Ak2DVector&				P, 
	const Ak2DVector&				A, 
	const Ak2DVector&				B, 
	const Ak2DVector&				C) const
{
	// http://www.blackpawn.com/texts/pointinpoly/default.html
	// 

	// Compute vectors        
	Ak2DVector v0 = C - A;
	Ak2DVector v1 = B - A;
	Ak2DVector v2 = P - A;

	// Compute dot products
	float dot00 = DotProduct(v0, v0);
	float dot01 = DotProduct(v0, v1);
	float dot02 = DotProduct(v0, v2); // ** Only these need to be done, other values could be stored
	float dot11 = DotProduct(v1, v1);
	float dot12 = DotProduct(v1, v2); // ** Only these need to be done, other values could be stored

	// Compute barycentric coordinates
	float denom = (dot00 * dot11 - dot01 * dot01); // could be stored

	if (denom == 0.0f)
		return true;

	float u = (dot11 * dot02 - dot01 * dot12) / denom;
	float v = (dot00 * dot12 - dot01 * dot02) / denom;

	// Check if point is in triangle
	return (u >= 0.0f) && (v >= 0.0f) && ((u + v) <= 1.0f);
}

void AkVBAPMap::AddPointsInArea(
	const Ak2DVector&				in_Source, 
	const AkVBAPTripletArray&		in_VBAPSlices, 
	const Ak2DVector*				in_m_Vertice, 
	AkVBAPTripletStackArray*		out_resultList) const
{
	AKASSERT((out_resultList->GetSize() - out_resultList->GetCount()) >= in_VBAPSlices.Length());

	// Add all the relevant triangles to the list, VPAB algorithm will decide on the best fit.
	for (AkVBAPTripletArray::Iterator it = in_VBAPSlices.Begin(); it != in_VBAPSlices.End(); ++it)
	{
		if ((*it).IsPointInsideBoundingBox(in_Source) )
		{
			AkVBAPTriplet* triangle = out_resultList->AddLast();
			triangle->Set((*it).A, (*it).B, (*it).C);
		}
	}
}

void AkVBAPMap::FindTriangle(
	const Ak3DVector&				in_Source, 
	AkReal32						in_fAngleAzimuthal,	
	AkReal32						in_fAngleElevation,
	AkVBAPTripletStackArray*		out_resultList)
{
	Ak2DVector sourceSpherical;

#if AKVBAP_FILTEROPTIM
	// HullA
	sourceSpherical.X = in_fAngleAzimuthal;
	sourceSpherical.Y = in_fAngleElevation;

#ifdef AKVBAP_OPTIMBELOW
	if (in_fAngleElevation > 0.f)
#endif
	{
		AddPointsInArea(
			sourceSpherical,
			m_VBAPSlicesHullA,
			m_Vertice2DHullA,
			out_resultList);

		// HullB
		// todo: improve visibility test and search in second convex hull only if necessary
		sourceSpherical.CartesianToSpherical(in_Source.Rotate180X_90Y());
		AddPointsInArea(
			sourceSpherical,
			m_VBAPSlicesHullB,
			m_Vertice2DHullB,
			out_resultList);
	}
#ifdef AKVBAP_OPTIMBELOW
	else
	{
		AddPointsInArea(
			sourceSpherical,
			m_VBAPSlicesHullABelow,
			m_Vertice2DHullA,
			out_resultList);

		// HullB
		// todo: improve visibility test and search in second convex hull only if necessary
		sourceSpherical.CartesianToSpherical(in_Source.Rotate180X_90Y());
		AddPointsInArea(
			sourceSpherical,
			m_VBAPSlicesHullBBelow,
			m_Vertice2DHullB,
			out_resultList);
	}
#endif //AKVBAP_OPTIMBELOW
#else //AKVBAP_FILTEROPTIM
	for ( AkVBAPTripletArray::Iterator it = m_VBAPSlicesHullA.Begin(); it != m_VBAPSlicesHullA.End(); ++it )
	{
		AkVBAPTriplet* triangle = out_resultList.AddLast();
		triangle->Set((*it).A, (*it).B, (*it).C);
	}

	for ( AkVBAPTripletArray::Iterator it = m_VBAPSlicesHullB.Begin(); it != m_VBAPSlicesHullB.End(); ++it )
	{
		AkVBAPTriplet* triangle = out_resultList.AddLast();
		triangle->Set((*it).A, (*it).B, (*it).C);
	}
#endif //AKVBAP_FILTEROPTIM

	//AKASSERT(!out_resultList->IsEmpty());	Note: this can happen in extreme cases at close distances from -zenith in semi-hemispheric configurations.
}

AKRESULT AkVBAPMap::Allocate( 
	AK::IAkPluginMemAlloc*			in_pAllocator, 
	const AkUInt32					in_NbPoints )
{	
	// a virtual point is added at the bottom of the virtual circle. 2D VBAP will be apply when used.
	m_VerticesCount = in_NbPoints + 1;

	AKASSERT( !m_VBAPSlicesHullA.Reserved() && !m_VBAPSlicesHullB.Reserved());
#ifdef AKVBAP_OPTIMBELOW
	AKASSERT(!m_VBAPSlicesHullABelow.Reserved() && !m_VBAPSlicesHullBBelow.Reserved());
#endif
	
	m_VBAPSlicesHullA.Reserve(2*m_VerticesCount);
	m_VBAPSlicesHullB.Reserve(2*m_VerticesCount);

#ifdef AKVBAP_OPTIMBELOW
	m_VBAPSlicesHullABelow.Reserve(2*m_VerticesCount);
	m_VBAPSlicesHullBBelow.Reserve(2*m_VerticesCount);
#endif

	m_Vertice3D				=	(Ak3DVector*)AK_PLUGIN_ALLOC( in_pAllocator, m_VerticesCount*sizeof(Ak3DVector) ); 
	m_Vertice2DHullA		=	(Ak2DVector*)AK_PLUGIN_ALLOC( in_pAllocator, m_VerticesCount*sizeof(Ak2DVector) ); 
	m_Vertice2DHullB		=	(Ak2DVector*)AK_PLUGIN_ALLOC( in_pAllocator, m_VerticesCount*sizeof(Ak2DVector) ); 

	if (m_Vertice3D == NULL || 
		m_Vertice2DHullA == NULL ||
		m_Vertice2DHullB == NULL)
	{
		Term (in_pAllocator);
		return AK_InsufficientMemory;
	}

	return AK_Success;
}

void AkVBAPMap::Term(
	AK::IAkPluginMemAlloc*			in_pAllocator )
{
	m_VBAPSlicesHullA.Term();
	m_VBAPSlicesHullB.Term();

#ifdef AKVBAP_OPTIMBELOW
	m_VBAPSlicesHullABelow.Term();
	m_VBAPSlicesHullBBelow.Term();
#endif

	if (m_Vertice3D)
		AK_PLUGIN_FREE( in_pAllocator, m_Vertice3D );

	if (m_Vertice2DHullA)
		AK_PLUGIN_FREE( in_pAllocator, m_Vertice2DHullA );

	if (m_Vertice2DHullB)
		AK_PLUGIN_FREE( in_pAllocator, m_Vertice2DHullB );

	m_Vertice3D = NULL;
	m_Vertice2DHullA = NULL;
	m_Vertice2DHullB = NULL;
}


void AkVBAPMap::PointsToVBAPMap( 
	const AkSphericalCoord*			in_SphericalPositions, 
	const AkUInt32					in_NbPoints )
{
	Ak2DVector point;
	AKASSERT( in_NbPoints == m_VerticesCount-1 );

	for ( AkUInt32 i = 0; i < in_NbPoints; i++ )
	{
		point = in_SphericalPositions[i];
		point.NormalizeSpherical();
		Ak2DVector sphericalPositions = point;
		// Need to go back to cartesian for the final VBAP step and also to rotate easilly for the second side.
		m_Vertice3D[i].SphericalToCartesian(sphericalPositions.X, sphericalPositions.Y);

		m_Vertice2DHullA[i] = sphericalPositions;
		m_Vertice2DHullB[i].CartesianToSpherical( m_Vertice3D[i].Rotate180X_90Y() );
	}

	Ak2DVector centerbottom =  Ak2DVector(0.0, -PIOVERTWO);
	m_Vertice3D[m_VerticesCount-1].SphericalToCartesian( centerbottom.X, centerbottom.Y );
	m_Vertice2DHullA[m_VerticesCount-1] = centerbottom;
	m_Vertice2DHullB[m_VerticesCount-1].CartesianToSpherical( m_Vertice3D[m_VerticesCount-1].Rotate180X_90Y() );

#ifdef AKVBAP_DEBUG
	char msg[256];
	sprintf( msg , "spe = [\n" );
	AKPLATFORM::OutputDebugMsg( msg );
	for (AkUInt32 i = 0; i < m_VerticesCount; i++)
	{
		//sprintf( msg , "%0.2f\t %0.2f\n", in_V[i].X, in_V[i].Y );
		sprintf( msg , "%0.2f, %0.2f, %0.2f;\n", in_SphericalPositions[i].theta, in_SphericalPositions[i].phi, in_SphericalPositions[i].r );
		
		AKPLATFORM::OutputDebugMsg( msg );
	}
	sprintf(msg, "];\n");
	AKPLATFORM::OutputDebugMsg( msg );
#endif

#ifdef AKVBAP_OPTIMBELOW
	RunTriangulation(
		m_Vertice2DHullA, 
		NULL, 
		NULL,
		&m_VBAPSlicesHullA,
		&m_VBAPSlicesHullABelow
#ifdef AKVBAP_DEBUG
		, 1
#endif
		); // HullA

	RunTriangulation(
		m_Vertice2DHullB, 
		&m_VBAPSlicesHullA, 
		&m_VBAPSlicesHullABelow,
		&m_VBAPSlicesHullB,
		&m_VBAPSlicesHullBBelow 
#ifdef AKVBAP_DEBUG
		, 2
#endif
		); // HullB
#else
	RunTriangulation(
		m_Vertice2DHullA, 
		NULL, 
		&m_VBAPSlicesHullA/*, 1*/); // HullA

	RunTriangulation(
		m_Vertice2DHullB, 
		&m_VBAPSlicesHullA, 
		&m_VBAPSlicesHullB /*, 2*/); // HullB
#endif
}

void AkVBAPMap::ComputeVBAP(
	AkReal32						in_fAngleAzimuthal,			///< Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are counterclockwise)
	AkReal32						in_fAngleElevation,			///< Incident angle, in radians [-pi/2,pi/2], where 0 is the elevation (positive values are above plane)
	AkUInt32						in_uNumChannels,
	AK::SpeakerVolumes::VectorPtr	out_vVolumes)
{
	ComputeVBAPSquared(
		in_fAngleAzimuthal,
		in_fAngleElevation,
		in_uNumChannels,
		out_vVolumes);

	NormalizePreSquaredGain(
		in_uNumChannels,
		out_vVolumes);
}

void AkVBAPMap::ComputeVBAPSquared(
	AkReal32						in_fAngleAzimuthal,			///< Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are counterclockwise)
	AkReal32						in_fAngleElevation,			///< Incident angle, in radians [-pi/2,pi/2], where 0 is the elevation (positive values are above plane)
	AkUInt32						in_uNumChannels,
	AK::SpeakerVolumes::VectorPtr	out_vVolumes )
{
	Ak3DVector cartSource;

	cartSource.SphericalToCartesian(
		in_fAngleAzimuthal, 
		in_fAngleElevation);

	AkVBAPTripletStackArray resultList;
	// Allocating for worst case: twice the total number of vertices, as triangles are searched twice.
	AkVBAPTriplet* resultArray = (AkVBAPTriplet *)AkAlloca(2 * in_uNumChannels * sizeof(AkVBAPTriplet));
	resultList.Set(resultArray, 2 * in_uNumChannels);


	FindTriangle(
		cartSource, 
		in_fAngleAzimuthal,
		in_fAngleElevation,
		&resultList);

	bool bFound = false;

	AKASSERT(in_uNumChannels == m_VerticesCount-1);
	AK::SpeakerVolumes::Vector::Zero( out_vVolumes, in_uNumChannels );
	
	for (AkUInt16 i = 0; i < resultList.GetCount(); i++)
	{
		AkVBAPTriplet* vbapTriplet = resultList.GetAt(i);
		if (IsTriplet2D(vbapTriplet))
		{
			bFound |= ComputeVBAP2D(vbapTriplet, cartSource, out_vVolumes);
		}
		else
		{
			bFound |= ComputeVBAP3D(vbapTriplet, cartSource, out_vVolumes);
		}
	}
	
	//AKASSERT(bFound); Note: this can happen in extreme cases at close distances from -zenith in semi-hemispheric configurations.

}

// Cartesian override: here the system is right handed with X pointing forward, Y to the left and Z to the top.
void AkVBAPMap::ComputeVBAPSquared(
	AkReal32						x,
	AkReal32						y,
	AkReal32						z,
	AkUInt32						in_uNumChannels,
	AK::SpeakerVolumes::VectorPtr	out_vVolumes)
{
	Ak3DVector cartSource;

	cartSource.X = x;
	cartSource.Y = y; 
	cartSource.Z = z;

	AkVBAPTripletStackArray resultList;
	// Allocating for worst case: twice the total number of vertices, as triangles are searched twice.
	AkVBAPTriplet* resultArray = (AkVBAPTriplet *)AkAlloca(2 * in_uNumChannels * sizeof(AkVBAPTriplet));
	resultList.Set(resultArray, 2 * in_uNumChannels);


	Ak2DVector tuple;
	tuple.CartesianToSpherical(cartSource);

	FindTriangle(
		cartSource,
		tuple.X,
		tuple.Y,
		&resultList);

	bool bFound = false;

	AKASSERT(in_uNumChannels == m_VerticesCount - 1);
	AK::SpeakerVolumes::Vector::Zero(out_vVolumes, in_uNumChannels);

	for (AkUInt16 i = 0; i < resultList.GetCount(); i++)
	{
		AkVBAPTriplet* vbapTriplet = resultList.GetAt(i);
		if (IsTriplet2D(vbapTriplet))
		{
			bFound |= ComputeVBAP2D(vbapTriplet, cartSource, out_vVolumes);
		}
		else
		{
			bFound |= ComputeVBAP3D(vbapTriplet, cartSource, out_vVolumes);
		}
	}

	//AKASSERT(bFound); Note: this can happen in extreme cases at close distances from -zenith in semi-hemispheric configurations.

}

bool AkVBAPMap::ComputeVBAP2D(
	AkVBAPTriplet*					in_SpeakerTriangle,	
	Ak3DVector						in_cartSource,
	AK::SpeakerVolumes::VectorPtr	out_vVolumes)
{
	bool ret = false;
	// maping below speaker hemisphere, compute 2D VBAP
	// VBAP 2D
	AkUInt32 vbap2D_A = (in_SpeakerTriangle->A != (m_VerticesCount - 1) ? in_SpeakerTriangle->A : in_SpeakerTriangle->B);
	AkUInt32 vbap2D_B = (in_SpeakerTriangle->C != (m_VerticesCount - 1) ? in_SpeakerTriangle->C : in_SpeakerTriangle->B);

	Ak2DVector sourcePoint = Ak2DVector(in_cartSource.X, in_cartSource.Y);
	Ak2DVector pointA = Ak2DVector(m_Vertice3D[vbap2D_A].X, m_Vertice3D[vbap2D_A].Y);
	Ak2DVector pointB = Ak2DVector(m_Vertice3D[vbap2D_B].X, m_Vertice3D[vbap2D_B].Y);
	Ak2DVector g = sourcePoint.LinearCombination(
		pointA, 
		pointB);

	if (g.IsAllPositive())
	{
		g.X *= g.X;
		g.Y *= g.Y;

		AkReal32 lengthSquare = g.X + g.Y;
		if (lengthSquare <= AK_FLT_EPSILON && lengthSquare >= -AK_FLT_EPSILON)
			return false;

		g /= lengthSquare;

		out_vVolumes[vbap2D_A] += g.X;
		out_vVolumes[vbap2D_B] += g.Y;
		ret = true;
	}

	return ret;
}

bool AkVBAPMap::ComputeVBAP3D(
	AkVBAPTriplet*					in_SpeakerTriangle,
	Ak3DVector						in_cartSource,
	AK::SpeakerVolumes::VectorPtr	out_vVolumes)
{
	bool ret = false;
	Ak3DVector g = in_cartSource.LinearCombination(
		m_Vertice3D[in_SpeakerTriangle->A], 
		m_Vertice3D[in_SpeakerTriangle->B], 
		m_Vertice3D[in_SpeakerTriangle->C]);

	if (g.IsAllPositive())
	{
		g.X *= g.X;
		g.Y *= g.Y;
		g.Z *= g.Z;

		AkReal32 lengthSquare = g.X + g.Y + g.Z;
		if (lengthSquare < AK_EPSILON && lengthSquare > -AK_EPSILON)
			return false;

		g /= lengthSquare;

		out_vVolumes[in_SpeakerTriangle->A] += g.X;
		out_vVolumes[in_SpeakerTriangle->B] += g.Y;
		out_vVolumes[in_SpeakerTriangle->C] += g.Z;
		ret = true;
	}

	return ret;
}

void AkVBAPMap::NormalizePreSquaredGain(
	AkUInt32						in_uNumChannels,
	AK::SpeakerVolumes::VectorPtr	out_vVolumes )
{
	AkReal32 vectLen = 0;
	for (AkUInt32 i = 0; i < in_uNumChannels; i++)
	{
		vectLen += out_vVolumes[i];
	}

	if (vectLen == 0.0f)
	{
		vectLen += 1.f;
	}

	AkReal32 oneover_vectLen = 1 / vectLen;
	for (AkUInt32 i = 0; i < in_uNumChannels; i++)
	{
		out_vVolumes[i] *= oneover_vectLen;
		out_vVolumes[i] = sqrtf(out_vVolumes[i]);
		AKASSERT(out_vVolumes[i] <= 1.f);
	}
}

// Constructor.
AkVBAP::AkVBAP()
{
}

// Destructor.
AkVBAP::~AkVBAP()
{
}
