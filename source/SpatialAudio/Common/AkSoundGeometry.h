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

#include "AkSpatialAudioPrivateTypes.h"
#include "AkImageSource.h"
#include "AkAcousticRoom.h"
#include "AkAcousticPortal.h"
#include "AkGeometrySet.h"
#include "AkScene.h"

AkForceInline AkHashType AkHash(AkRoomID in_key) { return (AkHashType)in_key.id; }
AkForceInline AkHashType AkHash(AkPortalID in_key) { return (AkHashType)in_key.id; }

typedef AkHashListBare<AkRoomID, AkAcousticRoom, ArrayPoolSpatialAudioGeometry> AkRoomMap;
typedef AkHashListBare<AkPortalID, AkAcousticPortal, ArrayPoolSpatialAudioGeometry> AkPortalMap;

typedef AkDynaBlkPool<AkAcousticRoom, 64, ArrayPoolSpatialAudioGeometry> AkAcousticRoomPool;
typedef AkDynaBlkPool<AkAcousticPortal, 64, ArrayPoolSpatialAudioGeometrySIMD> AkAcousticPortalPool;
typedef AkDynaBlkPool<AkImageSourceTriangle, 64, ArrayPoolSpatialAudioGeometrySIMD> AkTrianglePool;

typedef AkDynaBlkPool<AkGeometrySet, 64, ArrayPoolSpatialAudioGeometry> AkGeometrySetPool;
typedef AkHashListBare< AkGeometrySetID, AkGeometrySet, ArrayPoolSpatialAudioGeometry, AkGeometrySet> AkGeometrySetMap;

typedef AkListBareLight<AkGeometrySet> AkGeometrySetList;

class CAkSpatialAudioListener;
class CAkDiffractionEdge;
struct AkTaskSchedulerDesc;

namespace AK
{
	class WriteBytesMem;
	class ReadBytesMem;
}

class AkSoundGeometry
{
public:

	AkSoundGeometry();
	~AkSoundGeometry();

	AKRESULT SetRoom(
		AkRoomID in_RoomID,
		const AkRoomParams& in_RoomParams,
		AkGameObjectID in_SpatialAudioListenerID
	);

	AKRESULT SetPortal(
		AkPortalID in_PortalID,
		const AkPortalParams & in_Params
	);

	AKRESULT DeletePortal(AkPortalID in_PortalID);
	AKRESULT DeleteRoom(AkRoomID in_RoomID);

	void DeleteAllRoomsAndPortals();

	/// Add a geometry set.  In doing so, the triangles referenced by the set are indexed
	///	for fast look and some pre-computation is preformed for ray intersection.
	///	The array referenced by the set remains externally allocated and must exist for the 
	///	entire lifetime of the geometry set (untill calling RemoveGeometry())
	/// NOTE: Ownership of triangle array in in_geometrySet is transfered over to AkSoundGeometry.  Expect it to be NULL on return.
	AKRESULT SetGeometry(AkGeometrySetID in_id, AkGeometryParams& in_params);

	/// Remove a geometry set.  On successful removal, the set is copied into out_removedSet
	///	so that the triangle data may be freed by the called.
	AKRESULT RemoveGeometry(AkGeometrySetID in_GroupID);

	AKRESULT Init();
	void Term();

	AkAcousticRoom* GetRoom(AkRoomID in_SourceRoomID);

	AkAcousticPortal* GetPortal(AkPortalID in_PortalID);

	const AkAcousticRoom* GetRoom(AkRoomID in_SourceRoomID) const;
	const AkAcousticPortal* GetPortal(AkPortalID in_PortalID) const;

	bool UsingRooms() const { return m_RoomMap.Length() > 0; }
	bool UsingPortals() const { return m_PortalMap.Length() > 0; }

	void UpdateRooms(CAkSpatialAudioListener* in_pListener, AkUInt32 in_diffractionFlags);
	void ClearSoundPropagationPaths();

	AkScene* CreateScene();
	void DestroyScene(AkScene* in_pScene);

	void MonitorData();

	void BuildVisibilityData(AkTaskSchedulerDesc& taskScheduler, AkSpatialAudioInitSettings* in_settings)
	{
		if (m_bUpdateVis)
			_BuildVisibilityData(taskScheduler, in_settings);
	}

	AkScene* GetGlobalScene() { return m_pGlobalScene; }

protected:
	AkAcousticRoom* GetOrCreateRoom(AkRoomID in_SourceRoomID);
	void _BuildVisibilityData(AkTaskSchedulerDesc& taskScheduler, AkSpatialAudioInitSettings* in_settings);
	AKRESULT LinkPortalToRoom(AkAcousticPortal* pPortal, AkRoomID roomID);

	// Maps
	AkRoomMap m_RoomMap;
	AkPortalMap m_PortalMap;
	AkGeometrySetMap m_GeometrySetMap;

	// Pools
	AkAcousticRoomPool m_RoomPool;
	AkAcousticPortalPool m_PortalPool;
	AkGeometrySetPool m_GeometrySetPool;
	AkImageSourcePlanePool m_ReflectorPool;

	AkScene*	m_pGlobalScene;
	AkSceneList m_Scenes;

	AkUInt32 m_uMaxReflectionsOrder;
	
	bool m_bUpdateVis;
};

