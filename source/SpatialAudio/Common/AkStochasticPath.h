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

#ifndef _AK_STOCH_PATH_H_
#define _AK_STOCH_PATH_H_

#include <AK/SoundEngine/Common/AkSimd.h>
#include <AK/Tools/Common/AkVectors.h>

#include "AkSpatialAudioPrivateTypes.h"

class AkImageSourcePlane;
class CAkDiffractionEdge;
class CAkSpatialAudioEmitter;

/// Represent a stochastic source
/// A stochastic source could be either a reflector or an edge
///
class StochasticSource    
{
    public:
		/// Default constructor
		///
		StochasticSource()
			: m_reflector(nullptr)
			, m_diffractionEdge(nullptr)
		{}

		/// Construct a stochastic source as a reflector
		///
        StochasticSource(const AkImageSourcePlane* in_reflector, const AKSIMD_V4F32& in_hitPoint)
			: m_hitPoint(in_hitPoint)
			, m_reflector(in_reflector)
			, m_diffractionEdge(nullptr)
		{}

		/// Construct a stochastic source as a diffraction edge
		///
        StochasticSource(const CAkDiffractionEdge* i_diffractionEdge, AkInt32 i_zone)
			: m_zone(i_zone)
			, m_reflector(nullptr)
			, m_diffractionEdge(i_diffractionEdge)
		{}

		/// True if the stochasitc source is a diffraction edge.
		///
        bool isEdgeSource() const { return m_diffractionEdge != nullptr; }

		/// Return the diffraction edge. Can be null
		///
        const CAkDiffractionEdge* getDiffractionEdge() const { return m_diffractionEdge; }

		/// Return the diffraction zone. Valid if isEdgeSource returns true.
		///
		AkInt32 getDiffractionZone() const { AKASSERT(isEdgeSource()); return m_zone; }

		/// Return the reflector. Can be null
		///
        const AkImageSourcePlane* getReflector() const { return m_reflector; }

		/// Return the hit point. Valid if isEdgeSource return false.
		///
		const AKSIMD_V4F32& getHitPoint() const { AKASSERT(!isEdgeSource()); return m_hitPoint; }

		/// Compute the image of the source
		///
		bool ComputeImage(	const AKSIMD_V4F32& in_emitter,
							const AKSIMD_V4F32& in_listener,
							const AKSIMD_V4F32& in_source,
							AKSIMD_V4F32& out_imageSource) const;

		/// Set the source as a diffraction edge
		/// @post: reflector is null
		///
		void setDiffractionEdge(const CAkDiffractionEdge* in_edge, AkInt32 in_zone) { 
			m_reflector = nullptr; 
			m_diffractionEdge = in_edge;
			m_zone = in_zone;
		}

		/// Set the source as a reflector
		/// @post: diffraction edge is null
		///
		void setReflector(const AkImageSourcePlane* in_reflector, const AKSIMD_V4F32& in_hitPoint)
		{
			m_reflector = in_reflector;
			m_hitPoint = in_hitPoint;
			m_diffractionEdge = nullptr;
		}

    private:

		union 
		{
			/// Hit point. Relevant in case of a reflector
			///
			AKSIMD_V4F32 m_hitPoint;


			/// The diffraction zone
			///
			AkInt32 m_zone;
		};

		/// The reflector
		///
		const AkImageSourcePlane* m_reflector;

		/// The diffraction edge
		///
        const CAkDiffractionEdge* m_diffractionEdge;


		/// Comparison operator: two paths are different is they have different reflectors or edges
		///
        friend bool operator!=(const StochasticSource& in_sl, const StochasticSource& in_s2);
};

/// A stochastic path is composed of a list of stochastic sources
///
struct AkStochasticRay
{
	/// Collection of stochastic sources
	///
 	typedef AkArray<StochasticSource, const StochasticSource&, ArrayPoolSpatialAudioPathsSIMD, AkGrowByPolicy_Legacy_SpatialAudio<1>> SourceCollection;

	/// Default constructor
	///
	AkStochasticRay()
		: m_groupNumber(0)
	{}

	AkStochasticRay(AkUInt64 in_group)
		: m_groupNumber(in_group)
	{}

	/// Copy constructor
	///
	AkStochasticRay(const AkStochasticRay& in_src) 
	{
		Copy(in_src);
	}
	
	/// Virtual destructor
	///
	virtual ~AkStochasticRay() { m_sources.Term(); }

	/// Assignment operator
	///
	AkStochasticRay& operator=(const AkStochasticRay& in_src)
	{	
		m_groupNumber = in_src.m_groupNumber;
		m_sources.Copy(in_src.m_sources);
		m_rayOrigin = in_src.m_rayOrigin;
		m_rayDirection = in_src.m_rayDirection;
		
		return *this;
	}

	/// Transfer function defines for the AkArray memory management
	///
	void Transfer(AkStochasticRay& in_src)
	{			
		m_sources.Transfer(in_src.m_sources);
		m_groupNumber = in_src.m_groupNumber;
		m_rayOrigin = in_src.m_rayOrigin;
		m_rayDirection = in_src.m_rayDirection;
	}

	void Copy(const AkStochasticRay& in_src)
	{
		m_groupNumber = in_src.m_groupNumber;
		m_rayOrigin = in_src.m_rayOrigin;
		m_rayDirection = in_src.m_rayDirection;

		// Allocate just enough for one extra source, which will be added shortly.
		AkUInt32 numSrc = in_src.m_sources.Length();
		if (m_sources.Reserve(numSrc + 1) == AK_Success)
		{
			m_sources.Resize(numSrc);
			memcpy(m_sources.Data(), in_src.m_sources.Data(), numSrc * sizeof(StochasticSource));
		}
	}

	/// Return the group number.
	/// Rays originated from the same primary ray have identical group number
	///
	AkUInt64 GetGroupNumber() const
	{
		return m_groupNumber;
	}

	/// A path is valid if it has at least one source
	///
	bool IsValid() const
	{
		return !m_sources.IsEmpty();
	}

	AkUInt32 ComputeHash() const
	{
		AK::FNVHash32 hash;
		for (SourceCollection::Iterator it = m_sources.Begin(); it != m_sources.End(); ++it)
		{
			if ((*it).isEdgeSource())
			{
				const CAkDiffractionEdge* pEdge = (*it).getDiffractionEdge();
				hash.Compute(&pEdge, sizeof(pEdge));
			}
			else
			{
				const AkImageSourcePlane* pPlane = (*it).getReflector();
				hash.Compute(&pPlane, sizeof(pPlane));
			}
		}
		return hash.Get();
	}

	AkUInt32 ComputeHash(const CAkDiffractionEdge* in_nextEdge) const
	{
		AK::FNVHash32 hash(ComputeHash());
		hash.Compute(&in_nextEdge, sizeof(in_nextEdge));
		return hash.Get();
	}

	/// This is the last ray origin in the path
	///
	AKSIMD_V4F32 m_rayOrigin;

	/// This is the last ray direction in the path
	///
	AKSIMD_V4F32 m_rayDirection;

	/// Ray group number
	///
	AkUInt64 m_groupNumber;

	/// Sources that compose the path
	///
	SourceCollection m_sources;

};

inline bool operator!=(const StochasticSource& in_s1, const StochasticSource& in_s2)
{
    return !((in_s1.isEdgeSource() && in_s2.isEdgeSource() && in_s1.m_diffractionEdge == in_s2.m_diffractionEdge) ||
             (!in_s1.isEdgeSource() && !in_s2.isEdgeSource() && in_s1.m_reflector == in_s2.m_reflector));
}

/// A collection of stochastic rays (paths)
/// Rays should always be partially ordered on their group number
/// Note: the class itself does not inforce the ordering. Users should maintain it.
///
class StochasticRayCollection
	: public AkArray<AkStochasticRay, const AkStochasticRay&, ArrayPoolSpatialAudioPathsSIMD, AkGrowByPolicy_Legacy_SpatialAudio<8>, AkTransferMovePolicy<AkStochasticRay> >
{	
public:
	/// Constructor
	///
	StochasticRayCollection()
		: m_groupNumber(0)
	{}

	~StochasticRayCollection()
	{
		m_hashTable.Term();
	}
	
	/// Create a new group number (auto-incremented value)
	///
	AkUInt64 CreateNewGroupNumber()
	{
		return m_groupNumber++;
	}

	/// Return the current group number
	///
	AkUInt64 GetCurrentGroupNumber() const
	{
		return m_groupNumber;
	}

	/// Return the first group in the arrray
	///
	AkUInt64 GetFirstGroup() const
	{
		if (!IsEmpty())
		{
			return (*this)[0].GetGroupNumber();
		}
		else
		{
			return GetCurrentGroupNumber();
		}
	}
	
	/// Return the last group in the array
	///
	AkUInt64 GetLastGroup() const
	{
		if (!IsEmpty())
		{
			return (*this)[m_uLength-1].GetGroupNumber();
		}
		else
		{
			return GetCurrentGroupNumber();
		}
	}

	void ClearRays()
	{
		RemoveAll();
		m_groupNumber = 0;
	}

	AkStochasticRay* AddRay(
		const AkStochasticRay& in_ray,
		AKSIMD_V4F32 in_rayOrigin,
		AKSIMD_V4F32 in_rayDirection,
		AkImageSourcePlane *in_nextReflector
	)
	{
		AkStochasticRay* pAdded = AddLast();
		if (!pAdded)
			return NULL;

		pAdded->Copy(in_ray);

		pAdded->m_rayOrigin = in_rayOrigin;
		pAdded->m_rayDirection = in_rayDirection;

		StochasticSource *stochSource = pAdded->m_sources.AddLast();
		if (stochSource)
		{
			stochSource->setReflector(in_nextReflector, in_rayOrigin);
		}

		//Note: Not incrementing the hash table entry here. We need to make sure that all the diffraction paths are considered before doing so.

		return pAdded;
	}

	AkStochasticRay* AddRay(
		const AkStochasticRay& in_ray,
		AKSIMD_V4F32 in_rayOrigin,
		AKSIMD_V4F32 in_rayDirection,
		CAkDiffractionEdge *in_nextEdge,
		AkInt32 zone)
	{
		AkStochasticRay* pAdded = AddLast();
		if (!pAdded)
			return NULL;

		pAdded->Copy(in_ray);

		pAdded->m_rayOrigin = in_rayOrigin;
		pAdded->m_rayDirection = in_rayDirection;

		StochasticSource *stochSource = pAdded->m_sources.AddLast();
		if (stochSource)
		{
			stochSource->setDiffractionEdge(in_nextEdge, zone);
		}

		HashIncrement(pAdded->ComputeHash());

		return pAdded;
	}

	bool Exists(const AkStochasticRay & in_ray)
	{
		AkUInt32 hash = in_ray.ComputeHash();

		return m_hashTable.Exists(hash) != NULL;
	}

	bool Exists(const AkStochasticRay & in_ray, CAkDiffractionEdge *in_nextEdge)
	{
		AkUInt32 hash = in_ray.ComputeHash(in_nextEdge);

		return m_hashTable.Exists(hash) != NULL;
	}

	void ResetHashTable()
	{
		m_hashTable.RemoveAll();
	}

	void HashIncrement(AkUInt32 hash)
	{
		AkUInt32* count = m_hashTable.Exists(hash);
		if (count == NULL)
		{
			count = m_hashTable.Set(hash);
			if (count != NULL)
				*count = 1;
		}
		else
		{
			++(*count);
		}
	}

private:

	AkHashList< AkUInt32, AkUInt32, ArrayPoolSpatialAudioPaths> m_hashTable;

	/// Current group number
	///
	AkUInt64 m_groupNumber;
};


#endif // _AK_STOCH_PATH_H_
