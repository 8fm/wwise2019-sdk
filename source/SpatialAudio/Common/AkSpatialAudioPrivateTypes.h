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
#include <AK/Tools/Common/AkLock.h>
#include <AK/Tools/Common/AkString.h>
#include <AK/Tools/Common/AkSet.h>

#include "AkMath.h"
#include "AkRTree.h"

// Geometry Bank version
#define GEO_BANK_VERSION 0

// AkRTree typedefs
#define AK_RTREE_MIN 4
#define AK_RTREE_MAX 16

// Uncomment to use quaternions for virtual emitter rotation. Otherwise use rotation matrices.
//#define AK_SA_USE_QUATERNIONS

class AkImageSourceTriangle;
class CAkDiffractionEdge;

typedef AkRTree<AkImageSourceTriangle*, AkReal32, 3, AkReal32, AK_RTREE_MAX, AK_RTREE_MIN, ArrayPoolSpatialAudioGeometrySIMD> AkTriangleIndex;
typedef AkRTree<CAkDiffractionEdge*, AkReal32, 3, AkReal32, AK_RTREE_MAX, AK_RTREE_MIN, ArrayPoolSpatialAudioGeometrySIMD> AkEdgeIndex;

// AkArray typedefs
typedef AkArray<AkImageSourceTriangle*, AkImageSourceTriangle*, ArrayPoolSpatialAudio> AkTriangleArray;
typedef AkArray<CAkDiffractionEdge*, CAkDiffractionEdge*, ArrayPoolSpatialAudio> AkEdgeArray;
typedef AkArray<AkAcousticRoom*, AkAcousticRoom*, ArrayPoolSpatialAudio> AkImageSourceBoxArray;
typedef AkArray<AkAcousticPortal*, AkAcousticPortal*, ArrayPoolSpatialAudio> AkAcousticPortalArray;

typedef AkArray<AkImageSourcePlane*, AkImageSourcePlane*, ArrayPoolSpatialAudio> AkImageSourcePlaneArray;
typedef AkDynaBlkPool<AkImageSourcePlane, 128, ArrayPoolSpatialAudioSIMD> AkImageSourcePlanePool;

class Ak3DVector;

template<typename T>
static bool AkInArray(T in_element, T* in_array, AkUInt32 in_arraySize)
{
	for (AkUInt32 i = 0; i < in_arraySize; ++i)
	{
		if (in_array[i] == in_element)
			return true;
	}
	return false;
}

// Diffraction accumulation function. 
// NOTE: io_accumulated is the normalized diffraction value [0,1] (not an angle)
AkForceInline bool AkDiffractionAccum(AkReal32& io_accumulated, AkReal32 in_next)
{	io_accumulated += in_next;	if (io_accumulated > kMaxDiffraction)	{		io_accumulated = kMaxDiffraction;		return false;	}	return true;};

// Helper function to clean up geometry params data.
AkForceInline void TermAkGeometryParams(AkGeometryParams& in_params)
{
	if (in_params.Triangles)
	{
		AkFree(AkMemID_SpatialAudioGeometry, in_params.Triangles);
	}
	
	if (in_params.Vertices)
	{
		AkFree(AkMemID_SpatialAudioGeometry, in_params.Vertices);
	}

	if (in_params.Surfaces)
	{
		for (AkUInt32 i = 0; i < in_params.NumSurfaces; ++i)
		{
			if (in_params.Surfaces[i].strName != NULL)
				AkFree(AkMemID_SpatialAudioGeometry, (void*)in_params.Surfaces[i].strName);
		}

		AkFree(AkMemID_SpatialAudioGeometry, in_params.Surfaces);
	}

	in_params.NumTriangles = 0;
	in_params.Triangles = NULL;
	in_params.NumVertices = 0;
	in_params.Vertices = NULL;
	in_params.NumSurfaces = 0;
	in_params.Surfaces = NULL;
}

struct AkPortalPair
{
	AkPortalPair() {}
	AkPortalPair(AkPortalID in_a, AkPortalID in_b)
	{
		if (in_a < in_b)
		{
			p0 = in_a;
			p1 = in_b;
		}
		else
		{
			p0 = in_b;
			p1 = in_a;
		}
	}

	bool operator<(const AkPortalPair& in_other) const { return p0 == in_other.p0 ? p1 < in_other.p1 : p0 < in_other.p0; }
	bool operator>(const AkPortalPair& in_other) const { return p0 == in_other.p0 ? p1 > in_other.p1 : p0 > in_other.p0; }
	bool operator==(const AkPortalPair& in_other) const { return p0 == in_other.p0 && p1 == in_other.p1; }

	AkPortalID p0, p1; // such that p0 < p1
};

namespace AK
{
	namespace SpatialAudio
	{
		AkForceInline AkReal32 ACos(AkReal32 in_fAngle)
		{
			// Use exact trig function, but slower.
			return AkMath::ACos(AkClamp(in_fAngle, -1.f, 1.f));
		
			// Use fast trig approximation 
			//return AkMath::FastACos(AkClamp(in_fAngle, -1.f, 1.f));
		}
	}
}
