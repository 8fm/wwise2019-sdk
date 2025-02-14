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
// CAkStochasticReflectionEngine.h
//
//////////////////////////////////////////////////////////////////////


#ifndef _AK_STOCHASTIC_REFLECTION_ENG_H_
#define _AK_STOCHASTIC_REFLECTION_ENG_H_

#include <AK/SoundEngine/Common/AkSimd.h>
#include <AK/SpatialAudio/Common/AkSpatialAudioTypes.h>
#include <AK/Tools/Common/AkVectors.h>
#include <AkSettings.h>

#include "AkStochasticPath.h"
#include "AkDiffractionPath.h"

#include "AkRandom.h"

class CAkImageSourceStack;
class CAkReflectionPaths;
class Ak3DVector;
class AkImageSourcePlane;
class AkRayBundleInterface;
struct AkBoundingBox;

/// A RayGenerator abstracts the generation of primary and diffraction rays
///
class RayGenerator
{
public:
	virtual ~RayGenerator() {}

	/// Initialize the ray generator
	/// Should be called for each ray triggering sequence
	///
	virtual void initialize() = 0;

	/// Generate the next ray
	///
	virtual Ak3DVector NextPrimaryRay() = 0;

	/// Generate the next diffraction ray
	///
	virtual Ak3DVector NextDiffractionRay() = 0;
};

/// Model: RayGenerator
///
/// A RayGenerator abstracts the generation of primary rays.
///
/// class RayGenerator
///  + NextRay: Ak3DVector -> generates a ray as a normalized 3D vector
///
///

/// An implementation of RayGenerator model
///
/// Implement a uniform distribution of rays on a sphere surface.
///
class StochasticRayGenerator
	: public RayGenerator
{
public:
	StochasticRayGenerator()
		: m_zDistributionRange(2.0f)
		, m_angleDistributionRange(2.0f * PI)
	{
		// Do nothing
	}

	inline virtual void initialize() override 
	{
		// Do nothing
	}

	/// Return a ray as a normalized vector
	///
	inline virtual Ak3DVector NextPrimaryRay() override
	{
		Ak3DVector ray;

		ray.Z = -1.0f + AKRANDOM::AkRandom(m_PrimaryRayZSeed) * m_zDistributionRange / (AkReal32)AK_INT_MAX;
		AkReal32 angle = AKRANDOM::AkRandom(m_PrimaryRayAngleSeed) * m_angleDistributionRange / (AkReal32)AK_INT_MAX;

		ray.X = sqrtf(1 - ray.Z * ray.Z) * cosf(angle);
		ray.Y = sqrtf(1 - ray.Z * ray.Z) * sinf(angle);

		return ray;
	}

	inline virtual Ak3DVector NextDiffractionRay() override
	{
		Ak3DVector ray;

		ray.Z = -1.0f + AKRANDOM::AkRandom(m_diffractionRayZSeed) * m_zDistributionRange / (AkReal32)AK_INT_MAX;
		AkReal32 angle = AKRANDOM::AkRandom(m_diffractionRayAngleSeed) * m_angleDistributionRange / (AkReal32)AK_INT_MAX;

		ray.X = sqrtf(1 - ray.Z * ray.Z) * cosf(angle);
		ray.Y = sqrtf(1 - ray.Z * ray.Z) * sinf(angle);

		return ray;
	}

private:
	AkReal32 m_zDistributionRange;
	AkReal32 m_angleDistributionRange;
	
	static AkUInt64 m_PrimaryRayZSeed;
	static AkUInt64 m_PrimaryRayAngleSeed;

	static AkUInt64 m_diffractionRayZSeed;
	static AkUInt64 m_diffractionRayAngleSeed;
};

class StochasticRayGeneratorWithDirectRay
	: public StochasticRayGenerator
{
public:
	StochasticRayGeneratorWithDirectRay(CAkSpatialAudioListener* in_listener)
		: m_listener(in_listener)
	{}

	inline virtual void initialize() override
	{
		m_emitterIterator = CAkSpatialAudioEmitter::List().Begin();
	}

	/// Return a ray as a normalized vector
	///
	inline virtual Ak3DVector NextPrimaryRay() override
	{
		// Break conditions: either we are at the end of the list or an emitter has been found and the function returns
		while(m_emitterIterator != CAkSpatialAudioEmitter::List().End())
		{
			CAkSpatialAudioEmitter* pEmitter = static_cast<CAkSpatialAudioEmitter*>(*m_emitterIterator);

			if (pEmitter->GetListener() == m_listener &&
				pEmitter->GetOwner()->IsActive() &&
				pEmitter->HasReflectionsOrDiffraction() &&
				m_listener->GetActiveRoom() == pEmitter->GetActiveRoom()
				)
			{
				// Compute direct ray
				Ak3DVector ray = Ak3DVector(pEmitter->GetPosition()) - Ak3DVector(m_listener->GetPosition());

				++m_emitterIterator;

				return ray;
			}
			
			++m_emitterIterator;		
		}

		return StochasticRayGenerator::NextPrimaryRay();
	}

private:
	/// Listener
	///
	CAkSpatialAudioListener* m_listener;

	/// Emitter positions
	///
	CAkSpatialAudioEmitter::tList::Iterator m_emitterIterator;	
};

/// This reflection engine uses a stochastic ray casting to find reflections and (diffractions due to reflections)
///
class CAkStochasticReflectionEngine
{
public:
	///	Constructor
	///
	CAkStochasticReflectionEngine(
		AkUInt32 in_numberOfPrimaryRays = 100, 
		AkUInt32 in_maxOrder = 1, 
		AkReal32 in_maxDistance = 100.0
		);

	/// Destructor
	///
	~CAkStochasticReflectionEngine();

	/// Cast rays for the given listener
	/// At the end, the listener contains all the possible reflection/diffraction paths
	/// TRayGenerator is a model of ray generator that provides the following method:
	/// Ak3DVector NextRay(): returns the next ray to trace.
	///
	/// By default TRayGenerator uses a uniform distribution on a sphere.
	///
	void Compute(
		CAkSpatialAudioListener* in_listener,
		const AkTriangleIndex& in_triangles,
		RayGenerator& in_rayGenerator);
	
	/// Cast rays from the given acoustic portal into the given room
	///
	/// TRayGenerator is a model of ray generator that provides the following method:
	/// Ak3DVector NextRay(): returns the next ray to trace.
	///
	/// By default TRayGenerator uses a uniform distribution on a sphere.
	///
	void Compute(
		AkAcousticPortal& in_acousticPortal,		
		AkAcousticRoom& in_room,
		RayGenerator& in_rayGenerator);

	/// Compute valid reflection/diffraction paths using the ray paths (computed by Compute or ComputeBundle) from the given listener to the given emitter
	///
	void ComputePaths(
		CAkSpatialAudioListener* in_listener,
		CAkSpatialAudioEmitter* in_emitter,
		const AkTriangleIndex& in_triangles,
		bool in_bClearPaths = true);

	/// Compute valid reflection/diffraction paths using the ray paths (computed by Compute or ComputeBundle) from the given listener to the given portal
	///
	void ComputePaths(
		CAkSpatialAudioListener* in_listener,
		PortalPathsStruct& in_portal,
		AkAcousticPortal& in_acousticPortal,
		AkAcousticRoom& in_room
	);

	/// Compute valid reflection/diffraction paths using the ray paths (computed by Compute or ComputeBundle) from the given portal to the given emitter
	///
	void ComputePaths(		
		AkAcousticPortal& in_acousticPortal,
		PortalPathsStruct& inout_portal,
		AkAcousticRoom& in_room,
		CAkSpatialAudioEmitter* in_emitter
	);

	/// Compute valid diffraction paths using the ray paths (computed by Compute or ComputeBundle) from the one portal to another. 
	///
	void ComputePaths(
		AkAcousticPortal& in_acousticPortal0,
		AkAcousticPortal& in_acousticPortal1,
		AkAcousticRoom& in_room,
		CAkDiffractionPathSegments& out_diffractionPaths
	);

	/// Enable/Disable diffraction on reflections
	///
	void EnableDiffractionOnReflections(bool i_enable = true);

	/// Enable/Disable direct path diffraction. Diffraction must be enabled on each source individually.
	///
	void EnableDirectPathDiffraction(bool i_enable = true);

	/// Set the order of diffraction.
	/// This will be bound by the order of reflection
	///
	void SetOrderOfDiffraction(AkUInt32 i_order);

private:
	/// Used to pack emitter during path validation
	///
	struct EmitterInfo
	{

		EmitterInfo(
			const Ak3DVector& in_position,
			StochasticRayCollection& inout_stochasticPaths,
			CAkReflectionPaths& inout_reflectionPaths,
			IAkDiffractionPaths& inout_diffractionPaths,			
			AkReal32 in_diffractionMaxPathLength,
			bool in_enableReflections,
			bool in_enableDiffraction)
			: position(in_position)
			, stochasticPaths(inout_stochasticPaths)
			, reflectionPaths(inout_reflectionPaths)
			, diffractionPaths(inout_diffractionPaths)			
			, diffractionMaxPathLength(in_diffractionMaxPathLength)
			, enableReflections(in_enableReflections)
			, enableDiffraction(in_enableDiffraction)
		{}

		Ak3DVector position;
		StochasticRayCollection& stochasticPaths;
		CAkReflectionPaths& reflectionPaths;
		IAkDiffractionPaths& diffractionPaths;
		AkReal32 diffractionMaxPathLength;
		bool enableReflections;
		bool enableDiffraction;
	};

	typedef AkArray<AKSIMD_V4F32, const AKSIMD_V4F32&, ArrayPoolSpatialAudioPathsSIMD, AkGrowByPolicy_Legacy_SpatialAudio<8>> SourceCollection;

	enum class ValidationStatus
	{
		Valid = 0,
		NotValid,
		ListenerOutOfShadownZone,
		ListenerBehindObstacle
	};

private:
	/// Cast rays for computing reflection and diffration paths
	///
	void ComputeRays(
		const Ak3DVector& in_listener,
		StochasticRayCollection& inout_rays,
		AkUInt32 in_numberOfRays,
		RayGenerator& in_rayGenerator
	);

	/// Add the path to the list of valid path after validation
	/// in_emitter: an emitter
	/// in_listener: a listener
	/// in_rayOrigin: ray origin
	/// in_hitPoint: hit point from the last ray
	/// in_reflectorPath: the constructed stochasctic path to add if valid
	///	
	void AddValidPath(
		const Ak3DVector& in_listener,
		StochasticRayCollection& in_rays,
		EmitterInfo& inout_emitter,
		const AKSIMD_V4F32& in_rayOrigin,
		const AKSIMD_V4F32& in_rayDirection,
		AkStochasticRay& in_reflectorPath,
		const AkBoundingBox& in_receptorBBox);

	/// Trace a single ray:
	///
	/// in_rayOrigin: ray origin
	/// in_rayDirection: ray direction
	/// in_listener: a listener
	/// in_depth: current depth of reflection/diffraction
	/// inout_reflectorPath: the constructed stochasctic path
	///
	void TraceRay(
		const AKSIMD_V4F32& in_rayOrigin,
		const AKSIMD_V4F32& in_rayDirection,
		const Ak3DVector& in_listener,
		StochasticRayCollection& inout_rays,
		AkUInt32 in_depth,
		const AkStochasticRay& in_stochasticRay,
		RayGenerator& in_rayGenerator
	);

	/// Compute diffraction from the ray origin and hitpoint on a specific reflector
	/// in_rayOrigin: ray origin
	/// in_rayDirection: ray direction
	/// in_reflector: reflector the last ray intersects
	/// in_hitPoint: point of intersection between the reflector and the ray
	/// in_listener: listener position
	/// inout_rays: collection of rays
	/// in_depth: current depth
	/// inout_stochasticRay: the current stochastic ray path
	///
	bool Diffract(
		const AKSIMD_V4F32& in_rayOrigin,
		const AKSIMD_V4F32& in_rayDirection,
		AkImageSourcePlane* in_reflector,
		AKSIMD_V4F32 in_hitPoint,
		const Ak3DVector& in_listener,
		StochasticRayCollection& inout_rays,
		AkUInt32 in_depth,
		const AkStochasticRay& in_stochasticRay,
		RayGenerator& in_rayGenerator);

	bool ProcessDiffractionEdge(
		CAkDiffractionEdge *diffractionEdge,
		AkUInt32 zone,
		const AKSIMD_V4F32& in_rayOrigin,
		const AKSIMD_V4F32& in_rayDirection,
		const Ak3DVector& in_listener,
		StochasticRayCollection& inout_rays,
		AkUInt32 in_depth,
		const AkStochasticRay& in_stochasticRay,
		RayGenerator& in_rayGenerator);

	/// Validate the current stochastic paths from the listener to the emitter
	///
	void ValidatePaths(
		const Ak3DVector& in_listener,
		StochasticRayCollection& in_rays,
		EmitterInfo& inout_emitter,
		bool in_bClearPaths = true,
		bool in_bCalcTransmission = false);

	/// Return true if the ray intersects the bounding box
	///
	bool RayIntersect(
		const AKSIMD_V4F32& in_rayOrigin,
		const AKSIMD_V4F32& in_rayDirection,
		const AkBoundingBox& in_bbox
		);

	/// Compute image of the source through reflector in_reflector
	///
	void ComputeImageSource(
		const AKSIMD_V4F32& in_source,
		const AkImageSourcePlane* in_reflector,
		AKSIMD_V4F32& out_imageSource
		);

	/// Search if the stochastic path already exists
	///
	template<typename TCollection>
	bool SearchStochasticPath(
		const TCollection& in_paths,
		const AkStochasticRay& in_path
		);

	/// Validate the givent path from the emitter to the listener. Return true if it is validated
	/// in_emitter: an emitter
	/// in_listener: a listener
	/// in_reflectorPath: stochastic path to validate
	/// out_reflectionPath: created reflection path\
	/// in_validateSubPaths: if true try to validate the paths and the subpaths removing one node at the time
	///
	bool ValidateExistingPath(
		EmitterInfo& inout_emitter,
		const AKSIMD_V4F32& in_listener,		
		const AkStochasticRay& in_reflectorPath,
		AkReflectionPath& out_reflectionPath,
		AkStochasticRay& out_generatedPath
	);

	/// Validate the givent path from the emitter to the listener. Return true if it is validated
	/// in_emitter: an emitter
	/// in_listener: a listener
	/// in_reflectorPath: stochastic path to validate
	/// out_reflectionPath: created reflection path
	/// out_lastPathPoint: last point in the path
	/// out_lastReflector: last reflector (if any) computed for the path
	///
	ValidationStatus ValidatePath(
		const AKSIMD_V4F32& in_emitter,
		const AKSIMD_V4F32& in_listener,
		const AkStochasticRay& in_reflectorPath,	
		AkReflectionPath& out_reflectionPath,		
		Ak3DVector* out_lastPathPoint = nullptr,
		const AkImageSourcePlane** out_lastReflector = nullptr
	);
	
	/// Construct and validate a diffraction path starting from the stochastic path in_path.
	/// At the end, the emitter contains zero or one more diffractio path.
	/// Return true if a diffraction path has been added to the emitter diffraction paths.
	/// in_listener: a listener
	/// inout_emitter: an emitter
	/// in_path: the base stochastic path
	/// in_validateSubPaths: if true, we will try to remove one node at the time (starting from the one nearest from the listener) to find a valid direct diffraction path
	///
	bool ValidateDirectDiffractionPath(
		const Ak3DVector& in_listener,
		EmitterInfo& inout_emitter,
		const AkStochasticRay& in_path,
		AkStochasticRay& out_generatedPath,
		AkDiffractionPathSegment& out_generatedDiffractionPath
	);

	/// Validate the current stochastic paths from the listener to the emitter
	///
	void ValidateExistingPaths(
		const Ak3DVector& in_listener,		
		EmitterInfo& inout_emitter);
	
	/// Compute the reflection of the vector on plane in_reflector
	/// in_vector: a vector
	/// in_reflector: the plane of reflection
	/// out_reflectedVector: the reflected vector
	///
	void ComputeReflection(const Ak3DVector& in_vector, const AkImageSourcePlane& in_reflector, Ak3DVector& out_reflectedVector) const;

	/// Ajust the position of the intersection point by moving it out of the plane by some epsilon value using info from a vector going "out" of the plane
	/// in_outVector: vector pointing out of the plane (like a reflected ray)
	/// inout_point: a point on the plane
	/// in_reflector: the plane of reflection
	///
	void AdjustPlaneIntersectionPoint(const Ak3DVector& in_outVector, const AkImageSourcePlane& in_reflector, Ak3DVector& inout_point) const;

	/// Return true if the path segment (from in_pt0 to in_pt1) is occluded
	///
	bool IsPathOccluded(const AKSIMD_V4F32& in_pt0, const AKSIMD_V4F32& in_pt1,
		const CAkDiffractionEdge* pE0, const AkImageSourcePlane* pPl0, const CAkDiffractionEdge* pRE0,
		const CAkDiffractionEdge* pE1, const AkImageSourcePlane* pPl1, const CAkDiffractionEdge* pRE1
		) const;

	/// Return true if the path segment(from in_pt0 to in_pt1) is occluded
	///
	template<typename tFilter>
	bool IsPathOccluded(const AKSIMD_V4F32& in_pt0, const AKSIMD_V4F32& in_pt1, tFilter& in_filter) const;
	
	/// Return true if the path contains no reflection
	///
	bool IsDirectDiffractionPath(const AkStochasticRay& i_path) const;


	/// Compute the receptor size based on an estimate of the average path segment length
	///
	void EstimateReceptorSize(const Ak3DVector& in_origin, const StochasticRayCollection& in_rays);

	/// Scene as triangles (in an optimisation structure)
	///
	const AkTriangleIndex* m_Triangles;
	
	/// The range of the casted rays is between ]m_firstGroup, ...]
	///
	AkUInt64 m_lastValidatedPath;

	/// Max order of reflection/diffraction
	///
	AkUInt32 m_reflectionOrder;

	/// Max order of diffraction.
	/// <= m_reflectionOrder
	///
	AkUInt32 m_diffractionOrder;

	/// Max path distance
	///
	AkReal32 m_fMaxDist;

	/// The number of primary rays
	///
	AkUInt32 m_numberOfPrimaryRays;

	/// True if diffraction on reflections is enabled
	///
	bool m_diffractionOnReflectionsEnabled;

	/// True if direct path diffraction is enabled
	///
	bool m_directPathDiffractionEnabled;

	/// The receptor size for validating paths.
	/// Default 1000
	///
	AkReal32 m_receptorSize;
};

template<typename TCollection>
bool CAkStochasticReflectionEngine::SearchStochasticPath(
	const TCollection& in_paths,
	const AkStochasticRay& in_path
)
{
	for (AkUInt32 i = 0; i < in_paths.Length(); ++i)
	{
		if (in_paths[i].m_sources.Length() == in_path.m_sources.Length())
		{
			// Same size. Check if they have the same exact nodes in the same order.
			bool same = true;
			for (AkUInt32 p = 0; p < in_path.m_sources.Length(); ++p)
			{
				if (in_paths[i].m_sources[p] != in_path.m_sources[p])
				{
					// This one differs => go to next path
					same = false;
					break;
				}
			}

			// We did not find any difference so we found the path
			if (same)
			{
				return true;
			}
		}	
	}

	return false;
}


#endif // _AK_STOCHASTIC_REFLECTION_ENG_H_
