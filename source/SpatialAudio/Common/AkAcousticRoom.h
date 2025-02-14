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

#include "AkAcousticNode.h"
#include "AkAcousticPortal.h"
#include "AkSpatialAudioCompts.h"
#include <AK/Tools/Common/AkSet.h>

class AkScene;

struct AkPortalToPortalInfo
{
	AkPortalToPortalInfo() : pathDiffraction(0), pathLength(0) {}
	~AkPortalToPortalInfo() { paths.Term(); }

	AkPortalPair key;
	AkReal32 pathDiffraction;
	AkReal32 pathLength;
	CAkDiffractionPathSegments paths;

	bool IsOutOfRange() { return pathLength == FLT_MAX;	}

	void Transfer(AkPortalToPortalInfo& in_src)
	{
		key = in_src.key;
		pathDiffraction = in_src.pathDiffraction;
		pathLength = in_src.pathLength;
		paths.Transfer(in_src.paths);
	}
};
typedef AkSortedKeyArray< AkPortalPair, AkPortalToPortalInfo, ArrayPoolSpatialAudio, AkGetArrayKey<AkPortalPair, AkPortalToPortalInfo>, AkGrowByPolicy_DEFAULT, AkTransferMovePolicy<AkPortalToPortalInfo> > AkPortalToPortalMap;

class AkAcousticRoom : public AkAcousticNode
{
public:
	static const AkUInt32 kMaxPathTraversal = 8;

	typedef AkSet<CAkSpatialAudioEmitter*, ArrayPoolSpatialAudio> EmmitersInRoom;
	typedef EmmitersInRoom::Iterator EmitterIter;

public:

	AkAcousticRoom(AkRoomID in_RoomID) : AkAcousticNode(), key(in_RoomID)
		, pNextItem(NULL)
		, m_ReverbAuxBus(AK_INVALID_AUX_ID)
		, m_ReverbLevel(1.f)
		, m_ERLevel(1.f)
		, m_WallOcclusion(1.f)
		, m_sync(-1)
		, m_SelfSendLvl(0.0f)
		, m_Scene(NULL)
		, m_AddRefdObj(false)
	{}

	~AkAcousticRoom();

	void SetParams(const AkRoomParams& in_Params, AkGameObjectID in_SpatialAudioListenerID);

	bool IsDirty();

	AkReal32 GetERLevel() const;

	AkForceInline const AkAcousticPortalArray& GetPortals() const { return (const AkAcousticPortalArray&)this->AkAcousticNode::m_Links; }

	AkUInt32 GetEnabledPortalCount() const;

	AkForceInline AKRESULT SetPortal(AkAcousticPortal* in_pPortal)
	{
		return AkAcousticNode::Link((AkAcousticNode*)in_pPortal);
	}

	AkForceInline AKRESULT RemovePortal(AkAcousticPortal* in_pPortal)
	{
		return AkAcousticNode::Unlink((AkAcousticNode*)in_pPortal);
	}

	AkScene* GetScene() const { return m_Scene; }

	void SetScene(AkScene* in_scene);

	AkForceInline void ClearPortals()
	{
		return AkAcousticNode::ClearLinks();
	}

	AkForceInline AkRoomID GetID() const { return key; }

	AkForceInline void GetRoomRvbSend(AkRoomRvbSend& out_rvbSend) const
	{
		out_rvbSend.room = GetID();
		out_rvbSend.auxBus = GetReverbAuxBus();
		out_rvbSend.ctrlVal = GetReverbLevel();
		out_rvbSend.wallOcclusion = GetWallOcclusion();
	}

	// Returns the ID of the portal that the point is in, if it is in a portal.
	AkPortalID GetTransitionRoom(const Ak3DVector& in_Point,
							AkAcousticRoom*& out_pRoomA, AkAcousticRoom*& out_pRoomB,
							AkReal32& out_fRatio
							);
	
	void PropagateSound(CAkSpatialAudioListener* in_pERL, AkUInt32 in_maxDepth);

	void Traverse(CAkSpatialAudioListener* in_pERL, AkUInt32 in_maxDepth, AkUInt32 in_currentDepth, AkReal32 in_pathLength, AkReal32 in_gain, AkReal32 in_diffraction, AkAcousticPortal** in_portals, AkAcousticRoom** io_rooms);

	void InitContributingPortalPaths(	CAkSpatialAudioEmitter* in_pEmitter, 
										CAkSpatialAudioListener* in_pListener, 
										AkReal32 in_maxLength,
										AkSAPortalRayCastingTaskQueue& in_portalRaycastTasks,
										AkSAPortalToPortalTaskQueue& in_p2pTasks,
										bool& io_bAddedEmitterPath, 
										bool& io_bAddedListenerPath ) const;

	void InitContributingPortalPaths_RoomTone(	
										CAkSpatialAudioListener* in_pListener, 
										AkReal32 in_maxLength,
										AkSAPortalRayCastingTaskQueue& in_portalRaycastTasks,
										AkSAPortalToPortalTaskQueue& in_p2pTasks,
										bool& io_bAddedListenerPath 
											) const;

	void GetPaths(const CAkSpatialAudioEmitter* in_pEmitter, const CAkSpatialAudioListener* in_pListener, AkPropagationPathArray& out_paths) const;

	AkAcousticPortal* GetConnectedPortal(AkPortalID in_id) const;

	AkAcousticPortal* GetClosestPortal(const Ak3DVector& in_pt) const;

	AkPropagationPath* GetShortestPath(const CAkSpatialAudioListener* in_pListener) const;

	void Update(CAkSpatialAudioListener* in_pListener, AkUInt32 uDiffractionFlags);
	
	AkAuxBusID	GetReverbAuxBus() const { return m_ReverbAuxBus; }
	AkReal32	GetReverbLevel() const { return m_ReverbLevel; }
	AkReal32	GetWallOcclusion() const { return m_WallOcclusion; }

	AkRoomID		key;
	AkAcousticRoom* pNextItem;

	// Info about paths from one portal to another.
	void SetP2PPath(AkPortalID in_portal0, AkPortalID in_portal1, CAkDiffractionPathSegments& paths);
	void ClearP2PPaths();

	const CAkDiffractionPathSegments* GetP2PPaths(const AkAcousticPortal* in_portal0, const AkAcousticPortal* in_portal1) const;
	const AkDiffractionPathSegment* GetShortestP2PPath(const AkAcousticPortal* in_portal0, const AkAcousticPortal* in_portal1) const;

private:

	bool GetP2PPathLength(const AkAcousticPortal* in_portal0, const AkAcousticPortal* in_portal1, AkReal32& out_fPathLength, AkReal32& out_fDiffraction);

	void UpdateListenerRoomGameObj(CAkGameObject* pRoomGameObj, CAkSpatialAudioComponent* in_pListener, AkUInt32 uDiffractionFlags) const;
	void UpdateAdjacentRoomGameObj(CAkGameObject* pRoomGameObj, const AkPropagationPath& in_path, CAkSpatialAudioComponent* in_pListener, AkUInt32 uDiffractionFlags) const;
	void UpdateTransitionRoomGameObj(CAkGameObject* pRoomGameObj, CAkSpatialAudioComponent* in_pListener, AkUInt32 uDiffractionFlags) const;

	void GetPortalPositionData(
		AkAcousticPortal* pPortal,
		AkRoomID in_ListenerRoom,
		const Ak3DVector& in_listenerPos,
		const Ak3DVector& in_listenerUp,
		AkChannelEmitter& out_soundPos,
		AkReal32& out_spread,
		AkReal32& out_focus,
		AkObstructionOcclusionValues& out_obsOcc) const;

	void GetPortalPositionDataEx(
		AkAcousticPortal* pPortal,
		const Ak3DVector& in_listenerPos,
		const Ak3DVector& in_listenerUp,
		AkUInt32 uDiffractionFlags,
		AkChannelEmitter* out_soundPos,
		AkReal32* out_spread,
		AkReal32* out_focus,
		AkObstructionOcclusionValues* out_obsOcc,
		AkReal32& io_wetDiffractionCoef,
		AkUInt32& io_posIdx) const;

	void UpdateRoomGameObjHelper(
		CAkGameObject* pRoomGameObj,
		AkGameObjectID in_listenerGameObjID,
		const Ak3DVector& in_roomPickupPos,
		bool bSpatialized,
		AkObstructionOcclusionValues* obsOcc,
		AkChannelEmitter* soundPos,
		AkReal32* spread,
		AkReal32* focus,
		AkUInt32 numPos,
		AkReal32 minDiffraction,
		AkUInt32 uDiffractionFlags) const;

	void EnsureGameObjIsRegistered(CAkSpatialAudioListener* in_pListener);

	void KeepRoomGameObjRegistered(bool in_bPersistant, AkGameObjectID in_SpatialAudioListenerID);

	AkUInt32 GetSendToSelf(AkAuxSendValue* out_pValues) const;

	void SetOutOfRange(CAkSpatialAudioComponent* in_pListener, AkUInt32 uDiffractionFlags) const;

	AkPortalToPortalMap				m_P2PPaths;

	AkAuxBusID						m_ReverbAuxBus;
	AkReal32						m_ReverbLevel;

	AkReal32						m_ERLevel;

	AkReal32						m_WallOcclusion;

	AkUInt32						m_sync;

	AkReal32						m_SelfSendLvl;

	AkScene*						m_Scene;

	bool							m_AddRefdObj;
};