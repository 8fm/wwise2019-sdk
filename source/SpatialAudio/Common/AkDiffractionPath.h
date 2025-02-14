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
#include <AK/Tools/Common/AkVectors.h>
#include "AkSpatialAudioPrivateTypes.h"
#include "AkImageSource.h"
#include "AkStochasticPath.h"

class CAkDiffractionEdge;
struct AkPropagationPath;
struct AkSAPortalRayCastingTaskQueue;
struct AkSAPortalToPortalTaskQueue;

struct AkDiffractionPathSegment
{
	static const AkUInt32 kMaxLength = AK_MAX_SOUND_PROPAGATION_DEPTH;

	AkDiffractionPathSegment() : nodeCount(0), diffraction(0.f), totLength(0.f), obstructionValue(0.f), pathID(AK::Hash32::s_offsetBasis)
	{}

	// Calculate diffraction of this path segment. Note: This version does not include the distance multiplier (sDfrnShadowAttenFactor).
	void CalcDiffraction();
	void Reverse();

	AkForceInline bool Accumulate(AkReal32 in_angle)
	{
		return AkDiffractionAccum(diffraction, in_angle / PI);
	}

	// Add a contributing path item (reflector, edge, portal, etc) to the pathID hash.
	template<typename T>
	void AugmentPathID(T in_pathContributor)
	{
		AK::FNVHash32 hash(pathID);
		hash.Compute(&in_pathContributor, sizeof(in_pathContributor));
		pathID = hash.Get();
	}

	Ak3DVector listenerPos;
	Ak3DVector nodes[kMaxLength]; //nodes[0] is closest to listenerPos, nodes[nodeCount-1] is closest to emitterPos.
	Ak3DVector emitterPos;

	AkUInt32 nodeCount;

	/// Diffraction amount, normalized to the range [0,1]
	AkReal32 diffraction;

	AkReal32 totLength;

	AkReal32 obstructionValue;

	// Path ID is a hash of the diffraction edges that contribute to the paths. 
	// It is used to differentiate image sources in the reflect plugin, and therefor only used in paths that are appended to reflection paths.
	AkUInt32 pathID;
};

struct AkDiffractionPath: public AkDiffractionPathSegment
{
	static AkReal32 sDfrnShadowRad;
	static AkReal32 sDfrnShadowAttenFactor;

	AkDiffractionPath() : AkDiffractionPathSegment()
	{
		for (AkUInt32 i = 0; i < kMaxLength; ++i)
			angles[i] = 0.0f;
	}

	AkReal32 angles[kMaxLength];

	AkRoomID rooms[kMaxLength];
	AkPortalID portals[kMaxLength];

	AkTransform virtualPos;

	static AkForceInline AkReal32 GetDistanceMultiplier(AkReal32 in_angle)
	{
		AkReal32 w = AkMin(in_angle, sDfrnShadowRad) / sDfrnShadowRad;
		AkReal32 multiplier = (1.f + ((sDfrnShadowAttenFactor - 1.f)*w));
		return multiplier;
	}
	
	void Init(Ak3DVector in_listenerPos);
	void Append(const AkDiffractionPathSegment& in_path, const Ak3DVector& in_emitterPos, AkRoomID in_roomID);
	void AppendBw(const AkDiffractionPathSegment& in_path, const Ak3DVector& in_emitterPos, AkRoomID in_roomID);
	void Append(const AkPropagationPath& in_path, AkUInt32 in_startAtNode, const Ak3DVector& in_emitterPos, bool bUseP2PPaths);
	void AppendP2PPath(const AkDiffractionPathSegment& in_path, const AkAcousticPortal* in_fromPortal, const AkAcousticPortal* in_toPortal, AkRoomID in_fromRoomID, AkRoomID in_toRoomID);

	void CalcDiffractionAndVirtualPos(const AkTransform& in_emitterTransform);

	static void SetDiffractionSettings(
			AkReal32 in_diffrShadowAttenFactor, 
			AkReal32 in_diffrShadowDegrees)
	{
		sDfrnShadowAttenFactor = in_diffrShadowAttenFactor;
		sDfrnShadowRad = in_diffrShadowDegrees * PI / 180.f;
	}

private:
	bool AppendNode(const Ak3DVector& in_position, AkRoomID in_room, AkPortalID in_portal);
};

struct AkPropagationPath
{
	AkPropagationPath(): nodeCount(0), length(FLT_MAX), gain(1.0), diffraction(0.f)
	{
		portals[0] = NULL; //Used as key
	}

	AkReal32 GetPortalObstructionValue() const;

	AkAcousticPortal* EmitterSidePortal() const { return portals[nodeCount - 1]; }
	AkAcousticPortal* ListenerSidePortal() const { return portals[0]; }

	AkAcousticPortal* portals[AkDiffractionPath::kMaxLength];	// portal[0] is the portal closest to the listener; portal[nodeCount-1] is the portal closest to the emitter.
	AkAcousticRoom* rooms[AkDiffractionPath::kMaxLength];		// room[0] is the listener's room; the emitters room is not represented here. room[i] is between portal[i-1] and portal[i]

	AkUInt32 nodeCount; // Number of room and portals in the above array.
	AkReal32 length;	// Path length of the segments between portals.  The length of the two end segments (ie. between listener -> portal[0] and portal[nodeCount-1] -> emitter ) are unaccounted.
	AkReal32 gain;		// Cumulative gain of all the portals.
	AkReal32 diffraction;//Pre-calculated diffraction between portals (due to room geometry). Does not include diffraction through portals themselves.

	// Get key fcn
	static AkForceInline AkAcousticPortal* & Get(AkPropagationPath & in_item)
	{
		return in_item.portals[0];
	}

	void EnqueuePortalTasks(
		const AkAcousticRoom* in_pEmitterRoom,
		AkSAPortalRayCastingTaskQueue& io_portalRaycastTasks,
		AkSAPortalToPortalTaskQueue& io_p2pTasks,
		bool& io_bAddedEmitterSidePortal
	) const;
};

class IAkDiffractionPaths
{
public:
	IAkDiffractionPaths() : m_hasDirectLineOfSight(true), m_outOfRange(false) {}

	void Reset() 
	{
		m_hasDirectLineOfSight = true;
		m_outOfRange = false;
		ClearPaths();
	}

	virtual void ClearPaths() = 0;
	virtual AkDiffractionPathSegment* AddPath() = 0;
	virtual	AkUInt32 NumPaths() const = 0;
	
	// Indicates that there is least one path, and that path has 0 nodes - a direct line of sight. There may or may not be additional obstructed paths.
	bool HasDirectLineOfSight() const { return m_hasDirectLineOfSight; }

	// Indicates that the sound is beyond its max attenuation distance. If the sound is closer than the max attenuation when measured by Cartesian distance but is out of range 
	// because there is no path found, then there will be a single 'out of range' path used to render the sound at max distance in the correct direction. See AddOutOfRangePath().
	bool IsOutOfRange() const { return m_outOfRange; }

	void AddDirectLineOfSightPath(const Ak3DVector& in_emitter, const Ak3DVector& in_listener);

	void SetHasDirectLineOfSight(bool in_hasDirectLineOfSight) { m_hasDirectLineOfSight = in_hasDirectLineOfSight; }
	void SetOutOfRange(bool in_outOfRange) { m_outOfRange = in_outOfRange; }

	AkReal32 GetOcclusion() const { return m_occlusion; }
	void SetOcclusion(AkReal32 in_transmissionIndex) { m_occlusion = in_transmissionIndex; }

protected:
	AkReal32 m_occlusion;

	bool m_hasDirectLineOfSight;
	bool m_outOfRange;
};

class CAkDiffractionPaths : public AkArray<AkDiffractionPath, const AkDiffractionPath&, ArrayPoolSpatialAudioSIMD>,
							public IAkDiffractionPaths
{
	typedef AkArray<AkDiffractionPath, const AkDiffractionPath&, ArrayPoolSpatialAudioPathsSIMD, AkGrowByPolicy_Legacy_SpatialAudio<4> > tBase;

public:
	virtual void ClearPaths() { return RemoveAll(); }
	virtual AkDiffractionPathSegment* AddPath() { return AddLast(); }
	virtual	AkUInt32 NumPaths() const { return Length(); }

	void CalcDiffractionAndVirtualPos(const Ak3DVector& in_listenerPos, const AkTransform& in_emitterTransform);

	// Get combined values for diffraction/obstruction - for cases where we are not rendering individual virtual positions.
	AkReal32 GetCombinedObstructionDiffraction() const;
	AkReal32 GetMinDiffraction() const;
	AkReal32 GetMinObstruction() const;
	AkDiffractionPath* ShortestPath() const;

	void TransferPaths(CAkDiffractionPaths& in_src)
	{
		m_hasDirectLineOfSight = in_src.m_hasDirectLineOfSight;
		m_outOfRange = in_src.m_outOfRange;
		this->Transfer(in_src);
	}
};

class CAkDiffractionPathSegments :	public AkArray<AkDiffractionPathSegment, const AkDiffractionPathSegment&, ArrayPoolSpatialAudioSIMD>,
									public IAkDiffractionPaths
{
	typedef AkArray<AkDiffractionPathSegment, const AkDiffractionPathSegment&, ArrayPoolSpatialAudioPathsSIMD, AkGrowByPolicy_Legacy_SpatialAudio<4> > tBase;
public: 
	virtual void ClearPaths() { return RemoveAll(); }
	virtual AkDiffractionPathSegment* AddPath() { return AddLast(); }
	virtual	AkUInt32 NumPaths() const { return Length(); }

	void CalcDiffraction();
	AkDiffractionPathSegment* ShortestPath() const;

	void ReversePaths();

	void TransferPaths(CAkDiffractionPathSegments& in_src)
	{
		m_hasDirectLineOfSight = in_src.m_hasDirectLineOfSight;
		m_outOfRange = in_src.m_outOfRange;
		this->Transfer(in_src);
	}
};

struct PortalPathsStruct
{
	enum
	{
		PathState_New,		//A new portal path was added and needs to be computed.
		PathState_Valid,	//Portal paths are valid and have existed for at least one previous update.
		PathState_Invalid	//Portal paths are no longer valid and can be cleaned up.
	};

	PortalPathsStruct()
		: pathState(PathState_New) 
	{}

	~PortalPathsStruct()
	{
		diffractions.Term();
		reflections.Term();		
		stochasticPaths.Term();
	}

	void Invalidate()
	{
		pathState = PathState_Invalid;
	}

	bool IsValid() const {	return pathState != PathState_Invalid;	}
	bool IsNew() const 	{	return pathState == PathState_New;	}

	void Validate() { pathState = PathState_Valid;	}

	void Transfer(PortalPathsStruct& in_src)
	{
		key = in_src.key;
		pathState = in_src.pathState;
		diffractions.TransferPaths(in_src.diffractions);
		reflections.Transfer(in_src.reflections);
		stochasticPaths.Transfer(in_src.stochasticPaths);
	}

	AkPortalID key;
	
	CAkDiffractionPathSegments diffractions;
	CAkReflectionPaths reflections;

	/// Stochastic paths represented as valid rays (to the portal)
	///
	StochasticRayCollection stochasticPaths;

private:
	AkUInt8 pathState;
};
typedef AkSortedKeyArray< AkPortalID, PortalPathsStruct, ArrayPoolSpatialAudio, AkGetArrayKey< AkPortalID, PortalPathsStruct >, AkGrowByPolicy_DEFAULT, AkTransferMovePolicy< PortalPathsStruct > > AkPathsToPortals;

class AkPropagationPathArray : public AkSortedKeyArray< AkAcousticPortal*, AkPropagationPath, ArrayPoolSpatialAudio, AkPropagationPath >
{
public:
	~AkPropagationPathArray() { Term(); }

	// Build diffraction paths by appending together:
	//	(1) geometric diffraction paths between emitter and portals,
	//	(2) sound propagation paths between portals, 
	//  (3) geometric diffraction paths between portals and listener.
	void BuildDiffractionPaths(
		const Ak3DVector& in_listenerPos,
		const AkPathsToPortals& in_toListener,
		AkRoomID in_listenerRoom,
		const AkTransform& in_emitterPos, 
		const AkPathsToPortals& in_fromEmitter,
		AkRoomID in_emitterRoom,
		AkReal32 in_maxDistance,
		CAkDiffractionPaths& out_finalPaths
	) const;

	// Simplified version for no geometric diffraction paths
	void BuildDiffractionPaths(
		const Ak3DVector& in_listenerPos,
		AkRoomID in_listenerRoom,
		const AkTransform& in_emitterPos,
		AkRoomID in_emitterRoom,
		CAkDiffractionPaths& out_finalPaths
	) const;

	void BuildPortalDiffractionPaths(
		const Ak3DVector& in_listenerPos,
		const AkPathsToPortals& in_toListener,
		AkRoomID in_listenerRoomID,
		const AkTransform& in_portalPos,
		AkPortalID in_portalID,
		AkRoomID in_emitterRoomID,
		AkReal32 in_maxDistance,
		CAkDiffractionPaths& out_finalPaths) const;

	void BuildReflectionPaths(
		const CAkSpatialAudioListener* in_pListener,
		const CAkSpatialAudioEmitter* in_pEmitter,
		CAkReflectionPaths& out_finalPaths
	) const;
};
