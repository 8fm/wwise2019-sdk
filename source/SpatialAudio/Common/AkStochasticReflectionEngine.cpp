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
#include "AkSoundGeometry.h"
#include "AkImageSource.h"
#include "AkDiffractionPath.h"
#include "AkDiffractionEdge.h"
#include "AkStochasticReflectionEngine.h"
#include "AkReflectInstance.h"
#include "AkMath.h"
#include "AkOcclusionChecker.h"
#include "AkAcousticPortal.h"

//#include "AkRayDistribution.h"

#include "AkAudioLibTimer.h"

#define STOCHASTIC_MAX_ORDER 4

extern AkSpatialAudioInitSettings g_SpatialAudioSettings;

using namespace AkMath;

static const AKSIMD_V4F32 vNegOne = AKSIMD_SET_V4F32(-1.f);
static const AKSIMD_V4F32 vOne = AKSIMD_SET_V4F32(1.f);
static const AKSIMD_V4F32 vZero = AKSIMD_SETZERO_V4F32();

namespace
{
	const AkAcousticSurface* ValidateImageSource(const AKSIMD_V4F32& in_listenerPos, const AKSIMD_V4F32& in_imageSource, const AkImageSourcePlane& in_triPlane, AKSIMD_V4F32& out_hitPoint)
	{
		const AKSIMD_V4F32& ray_origin = in_listenerPos;
		const AKSIMD_V4F32 ray_direction = AKSIMD_SUB_V4F32(in_imageSource, in_listenerPos);

		AkUInt32 uLength = in_triPlane.m_Triangles.Length();

		const AkImageSourceTriangle* pHitTri = IntersectTriArray(in_triPlane.m_Triangles, ray_origin, ray_direction, out_hitPoint);
		if (pHitTri)
		{
			const AkImageSourceTriangle* pHitPortal = IntersectTriArray(in_triPlane.m_PortalTriangles, ray_origin, ray_direction, out_hitPoint);
			if (!pHitPortal)
				return &pHitTri->GetAcousticSurface();
		}

		return NULL;
	}

	const CAkDiffractionEdge* FindViewZoneDiffractionEdge(
		const AKSIMD_V4F32& in_listenerPos,
		const AKSIMD_V4F32& in_emitterPos,
		const AKSIMD_V4F32& in_imageSource,
		const AkImageSourcePlane& in_triPlane,
		const AkImageSourcePlane* in_pPrevPlane,
		AkReal32& io_level,
		AKSIMD_V4F32& out_DiffractionPoint,
		AkReal32& out_diffraction,
		const AkAcousticSurface*& out_pHitSurface		
	)
	{
		const CAkDiffractionEdge* pEdge = NULL;
		AkReal32 minDiffraction = 1.0f;
		AkReal32 maxLevel = 0.0f;

		Ak3DVector emitterPos = Ak3DVector(in_emitterPos);
		Ak3DVector listenerPos = Ak3DVector(in_listenerPos);
		Ak3DVector norm = Ak3DVector(in_triPlane.nx, in_triPlane.ny, in_triPlane.nz);

		AkUInt32 uLength = in_triPlane.m_Edges.Length();
		for (AkUInt32 edgeIdx = 0; edgeIdx < uLength; edgeIdx++)
		{
			CAkDiffractionEdge& edge = *in_triPlane.m_Edges[edgeIdx];

			if (in_pPrevPlane && in_pPrevPlane->HasEdge(&edge))
				continue;

			Ak3DVector edgeNorm, parallel;
			AkSurfIdx surface;

			Ak3DVector toEmitter = emitterPos - edge.start;
			Ak3DVector toListener = listenerPos - edge.start;

			AkReal32 e_dot_n0 = toEmitter.Dot(edge.n0);
			AkReal32 l_dot_n0 = toListener.Dot(edge.n0);

			if (e_dot_n0 > 0.f && l_dot_n0 > 0.f && fabsf(fabsf(edge.n0.Dot(norm)) - 1.0f) < AK_SA_EPSILON)
			{
				//n0 corresponds with surface in_triPlane
				edgeNorm = edge.n0;
				parallel = edge.direction.Cross(edge.n0);
				surface = edge.surf0->GetSurfIdx();
			}
			else
			{
				AkReal32 e_dot_n1 = toEmitter.Dot(edge.n1);
				AkReal32 l_dot_n1 = toListener.Dot(edge.n1);

				if (e_dot_n1 > 0.f && l_dot_n1 > 0.f && fabsf(fabsf(edge.n1.Dot(norm)) - 1.0f) < AK_SA_EPSILON)
				{
					//n1 corresponds with surface in_triPlane
					edgeNorm = edge.n1;
					parallel = edge.n1.Cross(edge.direction);
					surface = edge.surf1->GetSurfIdx();
				}
				else
				{
					continue;
				}
			}

			AkReal32 e_dot_pll = toEmitter.Dot(parallel);
			AkReal32 l_dot_pll = toListener.Dot(parallel);

			if (e_dot_pll < 0.f && l_dot_pll < 0.f)
				continue; // e and l on inside of edge normal. This is a specular case, or a miss.

			Ak3DVector edgePt;
			if (edge.FindDiffractionPt(listenerPos, Ak3DVector(in_imageSource) - listenerPos, edgePt))
			{
				Ak3DVector v0 = listenerPos - edgePt;
				AkReal32 v0Len = v0.Length();

				Ak3DVector v1 = Ak3DVector(in_emitterPos) - edgePt;
				AkReal32 v1Len = v1.Length();

				if (v0Len > AK_SA_EPSILON && v1Len > AK_SA_EPSILON)
				{
					AkReal32 v0_dot = edgeNorm.Dot(v0) / v0Len;
					AkReal32 v1_dot = edgeNorm.Dot(v1) / v1Len;

					AkReal32 a0 = AK::SpatialAudio::ACos(v0_dot);
					AkReal32 a1 = AK::SpatialAudio::ACos(v1_dot);
					
					AkReal32 maxAngle;
					AkReal32 diffractionAngle;
					if (a0 > a1)
					{
						if (e_dot_pll * l_dot_pll < 0.f)
						{
							maxAngle = (PI / 2.0f) - a1;
							diffractionAngle = a0 - a1;
						}
						else // corner reflection, both outside edge
						{
							maxAngle = (PI / 2.0f) + a1;
							diffractionAngle = a0 + a1;
						}
					}
					else
					{
						if (e_dot_pll * l_dot_pll < 0.f)
						{
							maxAngle = (PI / 2.0f) - a0;
							diffractionAngle = a1 - a0;
						}
						else // corner reflection, both outside edge
						{
							maxAngle = (PI / 2.0f) + a0;
							diffractionAngle = a1 + a0;
						}
					}

					AkReal32 diffraction = diffractionAngle / PI;
					AkReal32 r = diffractionAngle / maxAngle;
					AkReal32 level = 1.f - (r * r);
					if (level > maxLevel)
					{
						maxLevel = level;
						minDiffraction = diffraction;
						out_DiffractionPoint = edgePt.VectorV4F32();
						out_pHitSurface = &edge.pGeoSet->GetAcousticSurface(surface);
						pEdge = &edge;
					}
				}
			}
		}

		io_level *= maxLevel;
		out_diffraction = minDiffraction;
		return pEdge;
	}

	bool findDiffractioEdge(
		const AKSIMD_V4F32& in_origin,
		const AKSIMD_V4F32& in_receptorPoint,
		const AkImageSourcePlane& in_receptor,
		CAkDiffractionEdge*& out_diffractionEdge,
		AkInt32& out_edgeZone)
	{
		out_diffractionEdge = nullptr;
		AkReal32 minDistance = FLT_MAX;
		Ak3DVector point(in_receptorPoint);
		Ak3DVector origin(in_origin);

		AkUInt32 uLength = in_receptor.m_Edges.Length();
		for (AkUInt32 edgeIdx = 0; edgeIdx < uLength; edgeIdx++)
		{
			CAkDiffractionEdge& edge = *in_receptor.m_Edges[edgeIdx];
			if (!edge.isPortalEdge)
			{
				AkInt32 zone = edge.GetZone(origin);

				if (zone >= 0)
				{
					// Emitter is in shadow zone. Compute distance to the edge
					AkReal32 d = edge.SqDistPointSegment(point);
					if (d < minDistance)
					{
						out_diffractionEdge = &edge;
						out_edgeZone = zone;
						minDistance = d;
					}
				}
			}
		}

		return out_diffractionEdge != nullptr;
	}
	
	bool InShadowZone(const CAkDiffractionEdge* in_pEdge, const Ak3DVector& in_pt0, const Ak3DVector& in_pt1)
	{
		AkInt32 zone0 = in_pEdge->GetZone(in_pt0);
		AkInt32 zone1 = in_pEdge->GetZone(in_pt1);

		if (zone0 == 0 && zone1 == 1)
		{
			return in_pEdge->InShadowZoneQuick(in_pt0, in_pt1);
		}
		else if (zone0 == 1 && zone1 == 0)
		{
			return in_pEdge->InShadowZoneQuick(in_pt1, in_pt0);
		}

		return false;
	}

	bool CalcShadowZoneDiffraction(const CAkDiffractionEdge* in_pEdge, const AKSIMD_V4F32& in_pt0, const AKSIMD_V4F32& in_pt1, const AKSIMD_V4F32& in_diffractionPt, AkReal32& out_diffraction)
	{
		AKASSERT(in_pEdge != NULL);

		Ak3DVector from(in_pt0);
		Ak3DVector to(in_pt1);
		Ak3DVector dir = to - from;

		if (!InShadowZone(in_pEdge, from, to))
			return false;

		Ak3DVector pt(in_diffractionPt);
		
		Ak3DVector incident = from - pt;
		Ak3DVector diffracted = pt - to;
		AkReal32 lengthIncident = incident.Length();
		AkReal32 denom = (lengthIncident*diffracted.Length());
		out_diffraction = AK::SpatialAudio::ACos((incident.Dot(diffracted) / denom)) / (PI / 2.f);

		return true;
	}
}

AkUInt64 StochasticRayGenerator::m_PrimaryRayZSeed = 1;
AkUInt64 StochasticRayGenerator::m_PrimaryRayAngleSeed = 3;

AkUInt64 StochasticRayGenerator::m_diffractionRayZSeed = 5;
AkUInt64 StochasticRayGenerator::m_diffractionRayAngleSeed = 7;

CAkStochasticReflectionEngine::CAkStochasticReflectionEngine(
	AkUInt32 in_numberOfPrimaryRays, 
	AkUInt32 in_maxOrder, 
	AkReal32 in_maxDistance
)
	: m_Triangles(NULL)
	, m_lastValidatedPath(0)
	, m_reflectionOrder(AkMin(in_maxOrder, STOCHASTIC_MAX_ORDER))
	, m_diffractionOrder(AkMin(1, STOCHASTIC_MAX_ORDER - 1))
	, m_fMaxDist(in_maxDistance)
	, m_numberOfPrimaryRays(in_numberOfPrimaryRays)
	, m_diffractionOnReflectionsEnabled(false)
	, m_directPathDiffractionEnabled(false)
	, m_receptorSize(1000.0f)	
{
}

CAkStochasticReflectionEngine::~CAkStochasticReflectionEngine()
{
	// Do nothing
}

template<typename TArray>
struct ScopedDestructor
{
	ScopedDestructor(TArray& in_ref) : m_ref(in_ref) {}

	~ScopedDestructor()
	{
		m_ref.Term();
	}

	TArray& m_ref;
};

bool CAkStochasticReflectionEngine::ValidateExistingPath(
	EmitterInfo& inout_emitter,
	const AKSIMD_V4F32& in_listener,
	const AkStochasticRay& in_reflectorPath,
	AkReflectionPath& out_reflectionPath,
	AkStochasticRay& out_generatedPath
)
{
	Ak3DVector lastPathPoint;
	const AkImageSourcePlane* reflector;
	ValidationStatus res = ValidatePath(inout_emitter.position.PointV4F32(), in_listener, in_reflectorPath, out_reflectionPath, &lastPathPoint, &reflector);

	if (res == ValidationStatus::ListenerOutOfShadownZone)
	{
		out_generatedPath = in_reflectorPath;

		// Seems that there is a diffraction issue, we can try to skip the point
		if (out_generatedPath.m_sources.Length() < 2)
		{
			return false;
		}		
	
		out_generatedPath.m_sources.Erase(0);

		return ValidatePath(inout_emitter.position.PointV4F32(), in_listener, out_generatedPath, out_reflectionPath) == ValidationStatus::Valid;
	}
	else if (res == ValidationStatus::ListenerBehindObstacle)
	{
		// We are trying to find the nearest diffraction edge from the listener in order to extend the reflection with a diffraction path
		out_generatedPath = in_reflectorPath;
		Ak3DVector direction = lastPathPoint - Ak3DVector(in_listener);

		NearestOcclusionChecker<PlaneFilter<NoFilter> > nearestChecker(in_listener, direction.VectorV4F32(), PlaneFilter<NoFilter>(reflector));
		m_Triangles->RayNearestSearch(in_listener, direction.VectorV4F32(), nearestChecker, true);
					
		// Is the path occluded?
		if (nearestChecker.IsOccluded())
		{
			// We can find the nearest diffraction edge
			CAkDiffractionEdge *diffractionEdge;
			AkInt32 zone;
			if (findDiffractioEdge(in_listener, nearestChecker.getHitPoint(), *nearestChecker.getReflector(), diffractionEdge, zone)
				)
			{
				// Project hit point on the edge -> find the diffraction point
				Ak3DVector diffractionPt;
				if (diffractionEdge->FindDiffractionPt(Ak3DVector(in_listener), direction, diffractionPt))
				{
					// We add the diffraction edge as a new node in the path
					StochasticSource* node = out_generatedPath.m_sources.Insert(0);
					if (node)
					{
						node->setDiffractionEdge(diffractionEdge, zone);

						out_generatedPath.m_rayOrigin = in_listener;
						out_generatedPath.m_rayDirection = AKSIMD_SUB_V4F32(diffractionPt.PointV4F32(), in_listener);

						// We need to validate the path with the added node first
						bool valid = ValidatePath(inout_emitter.position.PointV4F32(), in_listener, out_generatedPath, out_reflectionPath) == ValidationStatus::Valid;

						// The direct path is not valid. Maybe there are several diffraction edges between the listener and the last reflection point
						if (!valid)
						{
							CAkEdgePathSearch directPathSearch;
							AkStochasticRay extendedPath;

							// Try to find a path from the listener to the last valid node in the last computed path
							AkInt32 maxNodes = out_generatedPath.m_sources.Length() < AK_MAX_REFLECTION_PATH_LENGTH ? AK_MAX_REFLECTION_PATH_LENGTH - out_generatedPath.m_sources.Length() : 0;
							bool found = directPathSearch.Search(Ak3DVector(in_listener), *diffractionEdge, lastPathPoint, *m_Triangles, extendedPath, maxNodes, inout_emitter.diffractionMaxPathLength);

							if (found)
							{
								// Insert the node in reverse
								for (AkInt32 i = (extendedPath.m_sources.Length() - 1); i >= 0; --i)
								{
									StochasticSource* node = out_generatedPath.m_sources.Insert(1);
									if (node)
									{
										*node = extendedPath.m_sources[i];
									}
									else
									{
										return false;
									}
								}

								// We need to validate the path with all the newly added diffraction edges
								return ValidatePath(inout_emitter.position.PointV4F32(), in_listener, out_generatedPath, out_reflectionPath) == ValidationStatus::Valid;
							}
						}

						return valid;
					}
				}
			}
		}

		return false;
	}
	else if (res == ValidationStatus::Valid)
	{
		out_generatedPath = in_reflectorPath;
		return true;
	}

	return false;
}

CAkStochasticReflectionEngine::ValidationStatus CAkStochasticReflectionEngine::ValidatePath(
	const AKSIMD_V4F32& in_emitter,
	const AKSIMD_V4F32& in_listener,
	const AkStochasticRay& in_reflectorPath,
	AkReflectionPath& out_reflectionPath,
	Ak3DVector* out_lastPathPoint,
	const AkImageSourcePlane** out_lastReflector
	)
{
	SourceCollection imageSources;
	ScopedDestructor<SourceCollection> arrayDestructor(imageSources);

	// Compute the image sources starting from listner
	// At the end, imageSources contains all the reflection images from the reflectors or the edges
	AKSIMD_V4F32 source = in_listener;
	for (AkUInt32 i = 0; i < in_reflectorPath.m_sources.Length(); ++i)
	{
		AKSIMD_V4F32 image;
		if (in_reflectorPath.m_sources[i].ComputeImage(in_listener, in_emitter, source, image))
		{
			imageSources.AddLast(image);
		}
		else
		{
			return ValidationStatus::NotValid;
		}

		source = image;
	}

	// Check that they is a correct number of image sources
	if (imageSources.IsEmpty() || imageSources.Length() >= AK_MAX_REFLECTION_PATH_LENGTH)
	{
		return ValidationStatus::NotValid;
	}

	// Reflection path will have the same length as the number of sources
	AkReflectionPath path(imageSources.Length());

	// Now going backward from emitter to listener, validate the path
	AKSIMD_V4F32 listener = in_emitter;
	AKSIMD_V4F32 previousListener = in_emitter;
	const AkImageSourcePlane* lastReflector = nullptr;
	const CAkDiffractionEdge* lastDiffractionEdge = nullptr;
	const CAkDiffractionEdge* lastReflectionEdge = nullptr;
	for (AkInt32 i = static_cast<AkInt32>(imageSources.Length() - 1); i >= 0; --i)
	{
		// i in ]n, 0]
		// pathIndex in [0, n[
		AkInt32 pathIndex = static_cast<AkInt32>(imageSources.Length()) - 1 - i;

		const AKSIMD_V4F32& is = imageSources[i];

		if (!in_reflectorPath.m_sources[i].isEdgeSource())
		{
			// This is a reflector
			const AkImageSourcePlane* reflector = in_reflectorPath.m_sources[i].getReflector();

			// Validate image source
			path.surfaces[pathIndex] = ValidateImageSource(listener, is, *reflector, path.pathPoint[pathIndex]);

			// No valid reflection. Try to find a diffraction edge visible from the listener
			const CAkDiffractionEdge* pEdge = nullptr;
			if (!path.surfaces[pathIndex])
			{
				AKSIMD_V4F32 previousImageSource = i > 0 ? imageSources[i - 1] : in_listener;
				
				pEdge = FindViewZoneDiffractionEdge(listener, previousImageSource, is, *reflector, lastReflector, path.level, path.pathPoint[pathIndex], path.diffraction[pathIndex], path.surfaces[pathIndex]);
				if (!pEdge ||
					!path.AccumulateDiffraction(path.diffraction[pathIndex]) ||
					path.level <= AK_SA_EPSILON)
				{
					return ValidationStatus::NotValid;
				}
			}

			// If we have a valid reflection or diffraction
			if (path.surfaces[pathIndex])
			{
				bool isOccluded = IsPathOccluded(path.pathPoint[pathIndex], listener, nullptr, reflector, pEdge, lastDiffractionEdge, lastReflector, lastReflectionEdge);

				// If the path is occluded, no need to go further
				if (isOccluded)
				{
					return ValidationStatus::NotValid;
				}
			}
			else
			{
				// No valid reflection, no need to go further
				return ValidationStatus::NotValid;
			}

			// Not in correct shadow zone, no need to go further
			// Note: pathIndex in [0, n[ -> lastDiffractionEdge != null -> pathIndex > 0 -> pathIndex - 1 >= 0
			if (lastDiffractionEdge)
			{
				// Compute the diffraction point and coefficient ??
				if (!CalcShadowZoneDiffraction(lastDiffractionEdge, previousListener, path.pathPoint[pathIndex], path.pathPoint[pathIndex - 1], path.diffraction[pathIndex - 1]) ||
					!path.AccumulateDiffraction(path.diffraction[pathIndex - 1]))
				{
					return ValidationStatus::NotValid;
				}
			}

			// The new source is the computed intersection point
			listener = path.pathPoint[pathIndex];
			lastReflector = reflector;
			lastReflectionEdge = pEdge;
			lastDiffractionEdge = nullptr;
		}
		else
		{
			// This is a diffraction edge
			if (!m_diffractionOnReflectionsEnabled)
			{
				return ValidationStatus::NotValid;
			}
			
			// Recompute the diffraction point in the plane of the listener/emitter
			Ak3DVector diffractionPt;
			if (!in_reflectorPath.m_sources[i].getDiffractionEdge()->FindDiffractionPt(Ak3DVector(in_listener), Ak3DVector(in_emitter) - Ak3DVector(in_listener), diffractionPt))
			{
				return ValidationStatus::NotValid;
			}
			
			path.pathPoint[pathIndex] = diffractionPt.PointV4F32();

			bool isOccluded = IsPathOccluded(path.pathPoint[pathIndex], listener, in_reflectorPath.m_sources[i].getDiffractionEdge(), nullptr, nullptr, lastDiffractionEdge, lastReflector, lastReflectionEdge);
			
			// If the path is occluded, no need to go further
			if (isOccluded)
			{
				return ValidationStatus::NotValid;
			}

			// Not in correct shadow zone, no need to go further			
			if (lastDiffractionEdge)
			{
				// Compute the diffraction point and coefficient ??
				// Note: pathIndex in [0, n[ -> lastDiffractionEdge != null -> pathIndex > 0 -> pathIndex - 1 >= 0
				if (!CalcShadowZoneDiffraction(lastDiffractionEdge, previousListener, path.pathPoint[pathIndex], path.pathPoint[pathIndex - 1], path.diffraction[pathIndex - 1]) ||
					!path.AccumulateDiffraction(path.diffraction[pathIndex - 1]))
				{
					return ValidationStatus::NotValid;
				}
			}

			// The new source is the computed intersection point
			previousListener = listener;
			listener = path.pathPoint[pathIndex];
			lastDiffractionEdge = in_reflectorPath.m_sources[i].getDiffractionEdge();
			lastReflector = nullptr;
			lastReflectionEdge = nullptr;
		}		
	}

	// At the end of the loop, the last path point set was at index 0
	// Test last point from the path to the final listener
	// imageSources.Length() > 0 -> imageSources.Length() - 1 >= 0
	bool isOccluded = IsPathOccluded(in_listener, path.pathPoint[imageSources.Length() - 1], nullptr, nullptr, nullptr, lastDiffractionEdge, lastReflector, lastReflectionEdge);
	
	// If path is occluded, path is not valid
	if (isOccluded)
	{
		if (out_lastPathPoint)
		{
			*out_lastPathPoint = Ak3DVector(path.pathPoint[imageSources.Length() - 1]);
			if (out_lastReflector)
			{
				*out_lastReflector = lastReflector;
			}			
		}

		return ValidationStatus::ListenerBehindObstacle;
	}

	// Check diffraction zones
	if (lastDiffractionEdge)
	{
		// Compute the diffraction point and coefficient ??
		if (!CalcShadowZoneDiffraction(lastDiffractionEdge, previousListener, in_listener, path.pathPoint[imageSources.Length() - 1], path.diffraction[imageSources.Length() - 1]) ||
			!path.AccumulateDiffraction(path.diffraction[imageSources.Length() - 1]))
		{
			return ValidationStatus::ListenerOutOfShadownZone;
		}
	}

	// Compute a unique path ID
	AK::FNVHash32 hash;
	AkInt32 depth = (AkInt32)in_reflectorPath.m_sources.Length() - 1;
	while (depth >= 0)
	{
		const AkImageSourcePlane* reflector = in_reflectorPath.m_sources[depth].getReflector();
		if (reflector)
		{
			hash.Compute(&reflector, sizeof(reflector));
		}
		else
		{
			const CAkDiffractionEdge* diffractionEdge = in_reflectorPath.m_sources[depth].getDiffractionEdge();
			hash.Compute(&diffractionEdge, sizeof(diffractionEdge));
		}

		--depth;
	}
	path.pathID = hash.Get();

	out_reflectionPath = path;

	return ValidationStatus::Valid;
}

bool CAkStochasticReflectionEngine::ValidateDirectDiffractionPath(
	const Ak3DVector& in_listener,
	EmitterInfo& inout_emitter,
	const AkStochasticRay& in_path,
	AkStochasticRay& out_generatedPath,
	AkDiffractionPathSegment& out_generatedDiffractionPath
)
{	
	AKASSERT(m_directPathDiffractionEnabled); // Should not be validating direct diffraction paths if not enabled.

	CAkEdgePathSearch directPathSearch;	
	if (in_path.IsValid())
	{
		// Try to validate from the begining edge
		{
			const StochasticSource& firstEdge = in_path.m_sources[0];

			Ak3DVector diffractionPt;
			bool pointFound = firstEdge.getDiffractionEdge()->FindDiffractionPt(in_listener, inout_emitter.position - in_listener, diffractionPt);
			EdgeFilter<NoFilter> edgeFilter(firstEdge.getDiffractionEdge());
			if (pointFound && !IsPathOccluded(in_listener.PointV4F32(), diffractionPt.PointV4F32(), edgeFilter))
			{
				if (directPathSearch.Search(in_listener, *firstEdge.getDiffractionEdge(), inout_emitter.position, *m_Triangles, out_generatedPath, out_generatedDiffractionPath, AK_MAX_REFLECTION_PATH_LENGTH, inout_emitter.diffractionMaxPathLength, false))
				{					
					return true;
				}
			}
		}

		// Try to validate from the end edge
		{
			const StochasticSource& lastEdge = in_path.m_sources[in_path.m_sources.Length() - 1];

			Ak3DVector diffractionPt;
			bool pointFound = lastEdge.getDiffractionEdge()->FindDiffractionPt(in_listener, inout_emitter.position - in_listener, diffractionPt);
			EdgeFilter<NoFilter> edgeFilter(lastEdge.getDiffractionEdge());
			if (pointFound && !IsPathOccluded(inout_emitter.position.PointV4F32(), diffractionPt.PointV4F32(), edgeFilter))
			{				
				if (directPathSearch.Search(inout_emitter.position, *lastEdge.getDiffractionEdge(), in_listener, *m_Triangles, out_generatedPath, out_generatedDiffractionPath, AK_MAX_REFLECTION_PATH_LENGTH, inout_emitter.diffractionMaxPathLength, true))
				{
					
					return true;
				}
			}
		}
	}

	return false;
}

void CAkStochasticReflectionEngine::ValidateExistingPaths(
	const Ak3DVector& in_listener,
	EmitterInfo& inout_emitter)
{
	// Parse all the existing paths and validate them
	StochasticRayCollection& stochasticPaths = inout_emitter.stochasticPaths;
	StochasticRayCollection::Iterator pathIt = stochasticPaths.Begin();

	// Newly generated path from existing ones
	// We use a temporary array to add the validated paths
	StochasticRayCollection generatedPaths;

	while (pathIt != stochasticPaths.End())
	{
		AkStochasticRay& path = *pathIt;

		if (path.IsValid())
		{
			// Validate the path
			if (IsDirectDiffractionPath(path))
			{
				// That's a diffraction path
				if (m_directPathDiffractionEnabled &&
					inout_emitter.enableDiffraction)
				{
					AkStochasticRay aPath;
					AkDiffractionPathSegment aDiffractionPath;

					if (ValidateDirectDiffractionPath(in_listener, inout_emitter, path, aPath, aDiffractionPath) && 
						!SearchStochasticPath(generatedPaths, aPath) )
					{
						AkDiffractionPathSegment* pAdded = inout_emitter.diffractionPaths.AddPath();
						if (pAdded)
							*pAdded = aDiffractionPath;

						generatedPaths.AddLast(aPath);
					}
				}
			}
			else
			{
				if (inout_emitter.enableReflections)
				{
					// That's a reflection path
					AkReflectionPath reflectionPath;
					AkStochasticRay aPath;
					if (ValidateExistingPath(inout_emitter, in_listener.PointV4F32(), path, reflectionPath, aPath))
					{
						if (!SearchStochasticPath(generatedPaths, aPath) && reflectionPath.ComputePathLength(in_listener, inout_emitter.position) < inout_emitter.diffractionMaxPathLength)
						{
							inout_emitter.reflectionPaths.AddLast(reflectionPath);
							generatedPaths.AddLast(aPath);
						}
					}
				}
			}
		}

		++pathIt;
	}

	stochasticPaths.Transfer(generatedPaths);
	generatedPaths.Term();
}

void CAkStochasticReflectionEngine::ComputeReflection(const Ak3DVector& in_vector, const AkImageSourcePlane& in_reflector, Ak3DVector& out_reflectedVector) const 
{
	// The path already exists so just go to next depth
	// Compute the reflected vector and trace
	// r = ray - 2 * (ray*n)n
	auto normal = in_reflector.Normal();
	out_reflectedVector = in_vector;
	out_reflectedVector = out_reflectedVector - normal * 2.0f * out_reflectedVector.Dot(normal);	
}

void CAkStochasticReflectionEngine::AdjustPlaneIntersectionPoint(const Ak3DVector& in_outVector, const AkImageSourcePlane& in_reflector, Ak3DVector& inout_point) const
{
	// Move hitpoint from the surface to prevent approximation errors
	// Plane are double-sided, meaning with have to check in which direction to move the point (positive or negative along the normal) based
	// on reflected ray direction		
	auto normal = in_reflector.Normal();
	if (in_outVector.Dot(normal) > 0.0f)
	{
		inout_point = inout_point + normal * AK_SA_EPSILON;
	}
	else
	{
		inout_point = inout_point - normal * AK_SA_EPSILON;
	}
}

void CAkStochasticReflectionEngine::Compute(
	CAkSpatialAudioListener* in_listener,
	const AkTriangleIndex& in_triangles,
	RayGenerator& in_rayGenerator)
{
	// Keep reference to geometry triangles
	m_Triangles = &in_triangles;

	// Compute the rays
	ComputeRays(in_listener->GetPosition(), in_listener->GetStochasticRays(), m_numberOfPrimaryRays, in_rayGenerator);	
}

void CAkStochasticReflectionEngine::Compute(
	AkAcousticPortal& in_acousticPortal,
	AkAcousticRoom& in_room,
	RayGenerator& in_rayGenerator)
{
	// Keep reference to geometry triangles
	m_Triangles = &in_room.GetScene()->GetTriangleIndex();

	// Compute the rays
	AkAcousticRoom* frontRoom = nullptr;
	AkAcousticRoom* backRoom = nullptr;

	in_acousticPortal.GetRooms(frontRoom, backRoom);

	if (&in_room == frontRoom)
	{
		ComputeRays(in_acousticPortal.GetRayTracePosition(AkAcousticPortal::FrontRoom), in_acousticPortal.GetStochasticRays(AkAcousticPortal::FrontRoom), m_numberOfPrimaryRays, in_rayGenerator);
	}
	else
	{
		ComputeRays(in_acousticPortal.GetRayTracePosition(AkAcousticPortal::BackRoom), in_acousticPortal.GetStochasticRays(AkAcousticPortal::BackRoom), m_numberOfPrimaryRays, in_rayGenerator);
	}
}

void CAkStochasticReflectionEngine::ComputeRays(
	const Ak3DVector& in_listener,
	StochasticRayCollection& inout_rays,
	AkUInt32 in_numberOfRays,
	RayGenerator& in_rayGenerator
)
{
	// Measure time from here
	AkInt64 start;
	AKPLATFORM::PerformanceCounter(&start);

	// Identify the first group
	AkUInt64 firstGroup;
	
	// Clear all the rays
	// In this case we get the last group number as all the paths are removed
	firstGroup = inout_rays.GetLastGroup();
	inout_rays.RemoveAll();
	
	// Reset the hash table if the listener has moved. This will allow re-collection of diffraction rays for the new position.
	inout_rays.ResetHashTable();
	
	// Initialize the ray generator for the next sequence
	in_rayGenerator.initialize();

	// Trace a bunch of rays from each listener	
	while (true)
	{
		// Compute a ray
		Ak3DVector ray = in_rayGenerator.NextPrimaryRay();

		// Ray length is at max path length
		ray = ray * m_fMaxDist;

		// Trace this ray until max depth has been hit or no hit exists		
		AkStochasticRay rayPath(inout_rays.CreateNewGroupNumber());
		TraceRay(in_listener.PointV4F32(), ray.VectorV4F32(), in_listener, inout_rays, 1, rayPath, in_rayGenerator);

		if ((inout_rays.GetCurrentGroupNumber() - firstGroup) > in_numberOfRays)
		{
			break;
		}
	}
}

void CAkStochasticReflectionEngine::ComputePaths(
	CAkSpatialAudioListener* in_listener,
	CAkSpatialAudioEmitter* in_emitter,
	const AkTriangleIndex& in_triangles,
	bool in_bClearPaths
)
{
	m_Triangles = &in_triangles;

		EmitterInfo emitter(
		in_emitter->GetPosition(),
		in_emitter->GetStochasticPaths(),
		in_emitter->GetReflectionPaths(),
		in_emitter->GetDiffractionPaths(),
		in_emitter->GetSpatialAudioComponent()->GetMaxDistance(),
		in_emitter->HasReflections(),
		in_emitter->HasDiffraction()
	);

	ValidatePaths(in_listener->GetPosition(), in_listener->GetStochasticRays(), emitter, in_bClearPaths, g_SpatialAudioSettings.bEnableTransmission);
}

void CAkStochasticReflectionEngine::ComputePaths(
	CAkSpatialAudioListener* in_listener,
	PortalPathsStruct& in_portal,
	AkAcousticPortal& in_acousticPortal,
	AkAcousticRoom& in_room
)
{
	m_Triangles = &in_room.GetScene()->GetTriangleIndex();

	AkAcousticRoom* frontRoom, *backRoom;
	in_acousticPortal.GetRooms(frontRoom, backRoom);
	AkUInt32 roomIndex = &in_room == frontRoom ? AkAcousticPortal::FrontRoom : AkAcousticPortal::BackRoom;

	EmitterInfo emitter(
		in_acousticPortal.GetRayTracePosition(roomIndex),
		in_portal.stochasticPaths,
		in_portal.reflections,
		in_portal.diffractions,
		in_listener->GetVisibilityRadius(),
		false,	// No reflections. We do not currently use the reflections on the listener-side of a portal. (Possible future enhancement?)
		true	// Yes diffraction.
	);

	ValidatePaths(in_listener->GetPosition(), in_listener->GetStochasticRays(), emitter, true, false);
}

void CAkStochasticReflectionEngine::ComputePaths(
	AkAcousticPortal& in_acousticPortal,
	PortalPathsStruct& inout_portal,
	AkAcousticRoom& in_room,
	CAkSpatialAudioEmitter * in_emitter)
{
	m_Triangles = &in_room.GetScene()->GetTriangleIndex();

	AkAcousticRoom* frontRoom, *backRoom;
	in_acousticPortal.GetRooms(frontRoom, backRoom);
	AkUInt32 roomIndex = &in_room == frontRoom ? AkAcousticPortal::FrontRoom : AkAcousticPortal::BackRoom;

	EmitterInfo emitter(
		in_emitter->GetPosition(),
		inout_portal.stochasticPaths,
		inout_portal.reflections,
		inout_portal.diffractions,
		in_emitter->GetMaxDistance(),
		in_emitter->HasReflections(),
		in_emitter->HasDiffraction()
	);

	ValidatePaths(in_acousticPortal.GetRayTracePosition(roomIndex), in_acousticPortal.GetStochasticRays(roomIndex), emitter, true, false);
}

void CAkStochasticReflectionEngine::ComputePaths(
	AkAcousticPortal& in_acousticPortal0, // acts as emitter
	AkAcousticPortal& in_acousticPortal1, // acts as listener
	AkAcousticRoom& in_room,
	CAkDiffractionPathSegments& out_diffractionPaths)
{
	m_Triangles = &in_room.GetScene()->GetTriangleIndex();

	AkAcousticRoom* frontRoom, *backRoom;
	in_acousticPortal0.GetRooms(frontRoom, backRoom);
	AkUInt32 roomIndex0 = &in_room == frontRoom ? AkAcousticPortal::FrontRoom : AkAcousticPortal::BackRoom;
	Ak3DVector p0 = in_acousticPortal0.GetRayTracePosition(roomIndex0);

	in_acousticPortal1.GetRooms(frontRoom, backRoom);
	AkUInt32 roomIndex1 = &in_room == frontRoom ? AkAcousticPortal::FrontRoom : AkAcousticPortal::BackRoom;
	Ak3DVector p1 = in_acousticPortal1.GetRayTracePosition(roomIndex1);

	AkReal32 euclidianDistance = (p0 - p1).Length();

	StochasticRayCollection& portal0RayCollection = in_acousticPortal0.GetStochasticRays(roomIndex0);
	StochasticRayCollection& portal1RayCollection = in_acousticPortal1.GetStochasticRays(roomIndex1);

	StochasticRayCollection unusedStochasticPaths; // will contain validated paths. Normally would be used to re-validate from frame to frame.
	CAkReflectionPaths unusedReflectionPaths;	// Not currently simulating reflections between two portals. Perhaps a future feature?

	if (portal1RayCollection.GetCurrentGroupNumber() >= portal0RayCollection.GetCurrentGroupNumber())
	{
		// portal0 acts as emitter, portal1 as the listener, since portal1 has more rays.

		EmitterInfo emitter(
			p0,
			unusedStochasticPaths,
			unusedReflectionPaths,
			out_diffractionPaths,
			euclidianDistance * 8.f,
			false,	//no reflections
			true	//yes diffraction
		);

		ValidatePaths(p1, in_acousticPortal1.GetStochasticRays(roomIndex1), emitter, true, false);
	}
	else
	{
		// portal1 acts as emitter, portal0 as the listener, since portal0 has more rays.

		EmitterInfo emitter(
			p1,
			unusedStochasticPaths,
			unusedReflectionPaths,
			out_diffractionPaths,
			euclidianDistance * 8.f,
			false,	//no reflections
			true	//yes diffraction
		);

		ValidatePaths(p0, in_acousticPortal0.GetStochasticRays(roomIndex0), emitter, true, false);

		// We have to reverse the paths since the paths must go from portal0 to portal1, where ID(portal0) < ID(portal1).
		out_diffractionPaths.ReversePaths();
	}

	unusedReflectionPaths.Term();
	unusedStochasticPaths.Term();

}

void CAkStochasticReflectionEngine::EnableDiffractionOnReflections(bool i_enable)
{
	m_diffractionOnReflectionsEnabled = i_enable;
}

void CAkStochasticReflectionEngine::EnableDirectPathDiffraction(bool i_enable)
{
	m_directPathDiffractionEnabled = i_enable;
}

void CAkStochasticReflectionEngine::SetOrderOfDiffraction(AkUInt32 i_order)
{
	m_diffractionOrder = AkMin(AkMin(i_order, m_reflectionOrder), STOCHASTIC_MAX_ORDER - 1);
}

void CAkStochasticReflectionEngine::TraceRay(
		const AKSIMD_V4F32& in_rayOrigin,
		const AKSIMD_V4F32& in_rayDirection,		
		const Ak3DVector& in_listener,
		StochasticRayCollection& inout_rays,
		AkUInt32 in_depth,
		const AkStochasticRay& in_stochasticRay,
		RayGenerator& in_rayGenerator
)
{	
	// Find a hit point
	NearestOcclusionChecker<NoFilter> nearestChecker(in_rayOrigin, in_rayDirection, NoFilter());
	m_Triangles->RayNearestSearch(in_rayOrigin, in_rayDirection, nearestChecker, false);

	// Is the path occluded? Yes => there is a hit point
	if (nearestChecker.IsOccluded())
	{
		// Get the point hit by the ray and the associated reflector
		Ak3DVector hitPoint(nearestChecker.getHitPoint());
		AkImageSourcePlane *reflector = nearestChecker.getReflector();
		
		// Compute reflected ray
		Ak3DVector reflectedRay;
		ComputeReflection(Ak3DVector(in_rayDirection), *reflector, reflectedRay);

		// Adjust the hit point accordingly
		AdjustPlaneIntersectionPoint(reflectedRay, *reflector, hitPoint);

		AkStochasticRay* pAdded = inout_rays.AddRay(in_stochasticRay, hitPoint.PointV4F32(), reflectedRay.VectorV4F32(), reflector);
		if (!pAdded)
			return;

		// Create a copy on the stack. Otherwise we get into trouble if we have to resize the array.
		AkStochasticRay reflectionRay = *pAdded;

		// If still some level of reflections
		if (in_depth < m_reflectionOrder)
		{
			// Trace the next ray
			TraceRay(hitPoint.PointV4F32(), reflectedRay.VectorV4F32(), in_listener, inout_rays, in_depth + 1, reflectionRay, in_rayGenerator);
		}
			
		// Diffract original ray 
		if (in_depth <= m_diffractionOrder)
		{
			AkUInt32 reflectorHash = reflectionRay.ComputeHash();
			if (!inout_rays.Exists(reflectorHash))
			{
				if (Diffract(in_rayOrigin, in_rayDirection, reflector, hitPoint.PointV4F32(), in_listener, inout_rays, in_depth, in_stochasticRay, in_rayGenerator))
				{
					// Only increment the hash table after we have added all the diffraction edges that are connected to the reflector.
					inout_rays.HashIncrement(reflectorHash);
				}
			}
		}

		// Also diffract the reflected ray
		if( in_depth <= m_diffractionOrder )
		{
			NearestOcclusionChecker<NoFilter> diffractedChecker(reflectionRay.m_rayOrigin, reflectionRay.m_rayDirection, NoFilter());
			m_Triangles->RayNearestSearch(reflectionRay.m_rayOrigin, reflectionRay.m_rayDirection, diffractedChecker, false);

			// Is the path occluded?
			if (diffractedChecker.IsOccluded())
			{
				Ak3DVector diffractedHitPoint(diffractedChecker.getHitPoint());
				AkImageSourcePlane *diffractedReflector = diffractedChecker.getReflector();
				
				Diffract(reflectionRay.m_rayOrigin, reflectionRay.m_rayDirection, diffractedReflector, diffractedHitPoint.PointV4F32(), in_listener, inout_rays, in_depth + 1, reflectionRay, in_rayGenerator);
			}
		}		
	}
}

bool CAkStochasticReflectionEngine::Diffract(
	const AKSIMD_V4F32& in_rayOrigin,
	const AKSIMD_V4F32& in_rayDirection,
	AkImageSourcePlane* in_reflector,
	AKSIMD_V4F32 in_hitPoint,
	const Ak3DVector& in_listener,
	StochasticRayCollection& inout_rays,
	AkUInt32 in_depth,
	const AkStochasticRay& in_stochasticRay,
	RayGenerator& in_rayGenerator)
{
	// Do we need to trace a diffraction ray?
	Ak3DVector point(in_hitPoint);
	Ak3DVector origin(in_rayOrigin);

	bool bAllEdgesAdded = true;

	for (AkUInt32 edgeIdx = 0; edgeIdx < in_reflector->m_Edges.Length(); edgeIdx++)
	{
		CAkDiffractionEdge& edge = *in_reflector->m_Edges[edgeIdx];
		if (!edge.isPortalEdge)
		{
			AkInt32 zone = edge.GetZone(origin);

			// Origin is in shadown zone of the edge -> it is valid
			if (zone >= 0)
			{
				if (!ProcessDiffractionEdge(&edge, zone, in_rayOrigin, in_rayDirection, in_listener, inout_rays, in_depth, in_stochasticRay, in_rayGenerator))
					bAllEdgesAdded = false;
			}
			else if (zone == -1) // In view zone
			{
				// Origin is not in the shadow zone of the edge, tries to extend to adjacent edges
				AkImageSourcePlane* otherPlane = edge.surf0->pPlane == in_reflector ? edge.surf1->pPlane : edge.surf0->pPlane;

				for (AkUInt32 connectedEdgeIdx = 0; connectedEdgeIdx < otherPlane->m_Edges.Length(); connectedEdgeIdx++)
				{
					CAkDiffractionEdge& connectedEdge = *otherPlane->m_Edges[connectedEdgeIdx];
					if (!connectedEdge.isPortalEdge && &connectedEdge != &edge)
					{
						AkInt32 zone = connectedEdge.GetZone(origin);

						if (zone >= 0)
						{
							if (!ProcessDiffractionEdge(&connectedEdge, zone, in_rayOrigin, in_rayDirection, in_listener, inout_rays, in_depth, in_stochasticRay, in_rayGenerator))
								bAllEdgesAdded = false;
						}
					}
				}
			}
		}
	}

	return bAllEdgesAdded;
}

bool CAkStochasticReflectionEngine::ProcessDiffractionEdge(
	CAkDiffractionEdge *diffractionEdge,
	AkUInt32 zone,
	const AKSIMD_V4F32& in_rayOrigin,
	const AKSIMD_V4F32& in_rayDirection,
	const Ak3DVector& in_listener,
	StochasticRayCollection& inout_rays,
	AkUInt32 in_depth,
	const AkStochasticRay& in_stochasticRay,
	RayGenerator& in_rayGenerator)
{
	if (!inout_rays.Exists(in_stochasticRay, diffractionEdge))
	{
		// Project hit point on the edge -> find the diffraction point
		Ak3DVector diffractionPt;
		if (diffractionEdge->FindDiffractionPt(in_listener, Ak3DVector(in_rayDirection), diffractionPt))
		{
			// Add diffraction point to the path
			AkStochasticRay* pAdded = inout_rays.AddRay( in_stochasticRay, in_rayOrigin, AKSIMD_SUB_V4F32(diffractionPt.PointV4F32(), in_rayOrigin), diffractionEdge, zone );
			if (!pAdded)
			{
				// Return if there is no memory
				return false;
			}

			// Follow reflection from diffraction
			if (m_diffractionOnReflectionsEnabled && in_depth < m_diffractionOrder)
			{
				// Create a copy on the stack. Otherwise we get into trouble if we have to resize the array.
				AkStochasticRay diffractionRayPath = *pAdded;

				for (AkUInt32 i = 0; i < m_numberOfPrimaryRays; ++i)
				{
					// Get a new ray
					Ak3DVector ray = in_rayGenerator.NextDiffractionRay();

					// Ray length is at max path length
					ray = ray * m_fMaxDist;

					// At this level only check that it is in the shadow zone
					if (InShadowZone(diffractionEdge, Ak3DVector(in_rayOrigin), diffractionPt + ray))
					{
						AkStochasticRay diffractionRelectionRayPath( diffractionRayPath );
						TraceRay(diffractionPt.PointV4F32(), ray.VectorV4F32(), in_listener, inout_rays, in_depth + 1, diffractionRelectionRayPath, in_rayGenerator);
					}
				}
			}

			return true;
		}
		else
		{
			// A path for this edge doesn't already exist in inout_rays, but this particular ray's diffraction point is off the end of the edge. 
			// Return false to signify that we still need to check the edges from other rays that hit the same reflector.
			return false;
		}
	}

	return true;
}

void CAkStochasticReflectionEngine::ValidatePaths(
	const Ak3DVector& in_listener,
	StochasticRayCollection& in_rays,
	EmitterInfo& inout_emitter,
	bool in_bClearPaths,
	bool in_bCalcTransmission)
{
	if (in_bClearPaths)
	{
		inout_emitter.reflectionPaths.RemoveAll();

		inout_emitter.diffractionPaths.ClearPaths();
		inout_emitter.diffractionPaths.SetOutOfRange(false);
		inout_emitter.diffractionPaths.SetHasDirectLineOfSight(false);
	}

	// Estimate receptor size
	if (inout_emitter.enableReflections)
	{
		EstimateReceptorSize(in_listener, in_rays);
	}

	// Validate previous paths
	ValidateExistingPaths(in_listener, inout_emitter);

	// Is the listener in the range of the emitter?
	bool inRange = (in_listener - inout_emitter.position).LengthSquared() < (inout_emitter.diffractionMaxPathLength * inout_emitter.diffractionMaxPathLength);

	// Add direct line of sight only if the distance is correct
	if ( inout_emitter.enableDiffraction && inRange)
	{
		AKSIMD_V4F32 pt0 = inout_emitter.position.PointV4F32();
		AKSIMD_V4F32 pt1 = in_listener.PointV4F32();
		AKSIMD_V4F32 dir = AKSIMD_SUB_V4F32(pt1, pt0);
		bool bOccluded;

		if (in_bCalcTransmission)
		{
			TransmissionOcclusionChecker checker(pt0, dir);
			m_Triangles->RaySearch(pt0, dir, checker);
			bOccluded = checker.IsOccluded();

			inout_emitter.diffractionPaths.SetOcclusion(checker.GetOcclusion());
		}
		else
		{
			OcclusionChecker<NoFilter> checker(pt0, dir, NoFilter());
			m_Triangles->RaySearch(pt0, dir, checker);
			bOccluded = checker.IsOccluded();
		}

		if (!m_directPathDiffractionEnabled || !bOccluded)
		{
			inout_emitter.diffractionPaths.AddDirectLineOfSightPath(inout_emitter.position, in_listener);
		}
	}

	// Emitter is too far from the listner, do not try to add any more paths
	if (!inRange)
	{
		inout_emitter.diffractionPaths.SetOutOfRange(true);
		
		return;
	}

	// Create the bounding box for validation of paths
	AkBoundingBox emitterReceptor;
	emitterReceptor.Update(inout_emitter.position);
	emitterReceptor.Update(inout_emitter.position + m_receptorSize);
	emitterReceptor.Update(inout_emitter.position - m_receptorSize);

	// Parse all ray path and add valid ones to the emitters
	const StochasticRayCollection& stochasticRays = in_rays;
	StochasticRayCollection::Iterator end = stochasticRays.End();
	
	for (StochasticRayCollection::Iterator it = stochasticRays.Begin(); it != end; ++it)
	{
		// Only validate paths not validated at previous step
		if ( (*it).IsValid() )
		{
			// Add valid reflection/diffraction paths
			AddValidPath(in_listener, in_rays, inout_emitter, (*it).m_rayOrigin, (*it).m_rayDirection, (*it), emitterReceptor);
			m_lastValidatedPath = (*it).GetGroupNumber();
		}	
	}

	if (inout_emitter.enableDiffraction && 
		inout_emitter.diffractionPaths.NumPaths() == 0)
	{
		inout_emitter.diffractionPaths.SetOutOfRange(true);
	}
}

void CAkStochasticReflectionEngine::AddValidPath(		
		const Ak3DVector& in_listener,
		StochasticRayCollection& in_rays,
		EmitterInfo& inout_emitter,
		const AKSIMD_V4F32& in_rayOrigin,
		const AKSIMD_V4F32& in_rayDirection,
		AkStochasticRay& in_reflectorPath,
		const AkBoundingBox& in_receptorBBox)
{
	StochasticRayCollection& stochasticPaths = inout_emitter.stochasticPaths;

	// The path is valid?
	bool valid = false;

	// Search the stochastic path inside the emitter stochastic paths? If already there, do nothing
	if (!SearchStochasticPath(stochasticPaths, in_reflectorPath))
	{
		bool isPureDiffraction = IsDirectDiffractionPath(in_reflectorPath);

		if (inout_emitter.enableReflections && !isPureDiffraction)
		{
			// Check if the ray intercept the receptor bounding box (the receptor is in between the ray origin and the hit point) of one of the emitters		
			if (RayIntersect(in_rayOrigin, in_rayDirection, in_receptorBBox))
			{
				// If path is valid for the emitter
				AkReflectionPath path;
				if (ValidatePath(inout_emitter.position.PointV4F32(), in_listener.PointV4F32(), in_reflectorPath, path) == ValidationStatus::Valid)
				{
					if (path.ComputePathLength(in_listener, inout_emitter.position) < inout_emitter.diffractionMaxPathLength)
					{
						valid = true;
					
						inout_emitter.reflectionPaths.AddLast(path);

						stochasticPaths.AddLast(in_reflectorPath);
					}
				}
			}
		}

		// The sotchastic ray path is not valid -> try to extend it with direct diffraction path and if it is not already in the emitter list, try to validate it
		if (!valid)
		{
			// Is it a pure direct diffraction path?
			if (isPureDiffraction)
			{
				if (m_directPathDiffractionEnabled &&
					inout_emitter.enableDiffraction)
				{
					AkStochasticRay aPath;
					AkDiffractionPathSegment aDiffractionPath;
					
					if (ValidateDirectDiffractionPath(in_listener, inout_emitter, in_reflectorPath, aPath, aDiffractionPath) &&
						!SearchStochasticPath(stochasticPaths, aPath))
					{
						AkDiffractionPathSegment* pAdded = inout_emitter.diffractionPaths.AddPath();
						if (pAdded)
							*pAdded = aDiffractionPath;

						stochasticPaths.AddLast(aPath);
					}
				}
			}
			else
			{
				if (inout_emitter.enableReflections && m_diffractionOnReflectionsEnabled)
				{
					// Is it a mixed path?
					const StochasticSource& source = in_reflectorPath.m_sources.Last();

					// The last point is an edge -> means diffraction
					if (source.isEdgeSource())
					{
						// Extend the path if possible
						AkStochasticRay directPathRay = in_reflectorPath;
						CAkEdgePathSearch directPathSearch;

						// Extend the path with a predefined maximum number of nodes
						AkInt32 maxNodes = directPathRay.m_sources.Length() < AK_MAX_REFLECTION_PATH_LENGTH ? AK_MAX_REFLECTION_PATH_LENGTH - directPathRay.m_sources.Length() : 0;
						bool found = directPathSearch.Search(Ak3DVector(directPathRay.m_rayOrigin), *source.getDiffractionEdge(), inout_emitter.position, *m_Triangles, directPathRay, maxNodes, inout_emitter.diffractionMaxPathLength);

						if (found)
						{
							// Does it already exist?				
							if (!SearchStochasticPath(stochasticPaths, directPathRay))
							{
								// No -> try to validate it							
								AkReflectionPath path;
								if (ValidatePath(inout_emitter.position.PointV4F32(), in_listener.PointV4F32(), directPathRay, path) == ValidationStatus::Valid)
								{
									if (path.ComputePathLength(in_listener, inout_emitter.position) < inout_emitter.diffractionMaxPathLength)
									{
										inout_emitter.reflectionPaths.AddLast(path);
										stochasticPaths.AddLast(directPathRay);
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

bool CAkStochasticReflectionEngine::RayIntersect(
	const AKSIMD_V4F32& in_rayOrigin,
	const AKSIMD_V4F32& in_rayDirection,
	const AkBoundingBox& in_bbox
)
{
	const AKSIMD_V4F32& p = in_rayOrigin;
	const AKSIMD_V4F32& d = in_rayDirection;

	const AKSIMD_V4F32& min = in_bbox.m_Min.PointV4F32();
	const AKSIMD_V4F32& max = in_bbox.m_Max.PointV4F32();
	
	static const AKSIMD_V4F32 one = AKSIMD_SET_V4F32(1.f);
	static const AKSIMD_V4F32 zero = AKSIMD_SET_V4F32(0.f);
		
	// Compute intersection t value of ray with near and far plane of slab 
	AKSIMD_V4F32 ood = AKSIMD_DIV_V4F32(one, d);
	AKSIMD_V4F32 t1 = AKSIMD_MUL_V4F32(AKSIMD_SUB_V4F32(min, p), ood);
	AKSIMD_V4F32 t2 = AKSIMD_MUL_V4F32(AKSIMD_SUB_V4F32(max, p), ood);

	AKSIMD_V4F32 tmin = AKSIMD_MAX_V4F32(AKSIMD_MIN_V4F32(t1, t2), zero);
	AKSIMD_V4F32 tmax = AKSIMD_MIN_V4F32(AKSIMD_MAX_V4F32(t2, t1), one);

	float maxComponent = AkMax(AKSIMD_GETELEMENT_V4F32(tmin, 0), AkMax(AKSIMD_GETELEMENT_V4F32(tmin, 1), AKSIMD_GETELEMENT_V4F32(tmin, 2)));
	float minComponent = AkMin(AKSIMD_GETELEMENT_V4F32(tmax, 0), AkMin(AKSIMD_GETELEMENT_V4F32(tmax, 1), AKSIMD_GETELEMENT_V4F32(tmax, 2)));

	return maxComponent <= minComponent;	
}

void CAkStochasticReflectionEngine::ComputeImageSource(
	const AKSIMD_V4F32& in_source,
	const AkImageSourcePlane* in_reflector,
	AKSIMD_V4F32& out_imageSource
)
{
	Ak3DVector source(in_source);

	Ak3DVector wallNorm = in_reflector->Normal();
	Ak3DVector wallPos = in_reflector->Position();
	Ak3DVector toWall = wallPos - source;
	AkReal32 proj = toWall.Dot(wallNorm);
	Ak3DVector imageSource = wallNorm * (2.f * proj) + source;

	out_imageSource = imageSource.PointV4F32();
}

bool CAkStochasticReflectionEngine::IsPathOccluded(const AKSIMD_V4F32& in_pt0, const AKSIMD_V4F32& in_pt1,
	const CAkDiffractionEdge* pE0, const AkImageSourcePlane* pPl0, const CAkDiffractionEdge* pRE0,
	const CAkDiffractionEdge* pE1, const  AkImageSourcePlane* pPl1, const CAkDiffractionEdge* pRE1
	) const
{
	if (pE0 != NULL && pE1 == NULL) // Diffraction edge to plane (or reflection edge)
	{
		if (pRE1 != NULL)
		{
			TwoEdgeFilter filter(pE0, pRE1);
			return IsPathOccluded(in_pt0, in_pt1, filter);
		}
		else
		{
			PlaneEdgeFilter filter(pPl1, pE0);
			return IsPathOccluded(in_pt0, in_pt1, filter);
		}
	}
	else if (pE0 == NULL && pE1 != NULL) // Plane (or reflection edge) to edge
	{
		if (pRE0 != NULL)
		{
			TwoEdgeFilter filter(pE1, pRE0);
			return IsPathOccluded(in_pt0, in_pt1, filter);
		}
		else
		{
			PlaneEdgeFilter filter(pPl0, pE1);
			return IsPathOccluded(in_pt0, in_pt1, filter);
		}
	}
	else if (pE0 != NULL && pE1 != NULL) // Diffraction edge to diffraction edge
	{
		TwoEdgeFilter filter(pE0, pE1);
		return IsPathOccluded(in_pt0, in_pt1, filter);
	}
	else // Plane (or reflection edge) to plane (or reflection edge)
	{
		if (pRE0 != NULL && pRE1 != NULL)
		{
			TwoEdgeFilter filter(pRE0, pRE1);
			return IsPathOccluded(in_pt0, in_pt1, filter);
		}
		else if (pRE0 != NULL)
		{
			PlaneEdgeFilter filter(pPl1, pRE0);
			return IsPathOccluded(in_pt0, in_pt1, filter);
		}
		else if (pRE1 != NULL)
		{
			PlaneEdgeFilter filter(pPl0, pRE1);
			return IsPathOccluded(in_pt0, in_pt1, filter);
		}
		else
		{
			TwoPlaneFilter filter(pPl0, pPl1);
			return IsPathOccluded(in_pt0, in_pt1, filter);
		}
	}
}

template<typename tFilter>
bool CAkStochasticReflectionEngine::IsPathOccluded(const AKSIMD_V4F32& in_pt0, const AKSIMD_V4F32& in_pt1, tFilter& in_filter) const
{
	WWISE_SCOPED_PROFILE_MARKER("CAkReflectionEngine::IsPathOccluded()");

	const AKSIMD_V4F32 dir = AKSIMD_SUB_V4F32(in_pt1, in_pt0);
	OcclusionChecker<tFilter> checker(in_pt0, dir, in_filter);

	m_Triangles->RaySearch(in_pt0, dir, checker);

	return checker.IsOccluded();
}

bool CAkStochasticReflectionEngine::IsDirectDiffractionPath(const AkStochasticRay& i_path) const
{
	for (AkUInt32 i = 0; i < i_path.m_sources.Length(); ++i)
	{
		if (!i_path.m_sources[i].isEdgeSource())
		{
			return false;
		}
	}

	return true;
}

void CAkStochasticReflectionEngine::EstimateReceptorSize(const Ak3DVector& in_origin, const StochasticRayCollection & in_rays)
{
	int nbPathSegments = 0;
	AkReal32 totalLength = 0.0f;

	// test around 20% of rays
	int numberOfRaysToTest = static_cast<int>(in_rays.Length() * 0.2f);

	// Randomly choose numberofRaysToTest
	for (int i = 0; i < numberOfRaysToTest; ++i)
	{
		AkInt32 index = static_cast<AkInt32>((in_rays.Length() - 1) * AKRANDOM::AkRandom() / (AkReal32)AK_INT_MAX);
		
		const AkStochasticRay& ray = in_rays[index];

		// Parse all segments
		Ak3DVector previousPoint = in_origin;
		
		for (AkUInt32 p = 0; p < ray.m_sources.Length(); ++p)
		{
			const StochasticSource& source = ray.m_sources[p];

			if( source.isEdgeSource() )
			{
				totalLength += (source.getDiffractionEdge()->start - previousPoint).Length();
				previousPoint = source.getDiffractionEdge()->start;
			}
			else
			{
				totalLength += (Ak3DVector(source.getHitPoint()) - previousPoint).Length();
				previousPoint = Ak3DVector(source.getHitPoint());
			}
			
			++nbPathSegments;
		}
	}

	if (nbPathSegments > 0)
	{
		m_receptorSize = totalLength / nbPathSegments;
	}
}
