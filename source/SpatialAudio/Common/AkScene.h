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
#include "AkSpatialAudioTasks.h"

typedef AkListBareLight<AkGeometrySet> AkGeometrySetList;

struct NoFilter;

template<typename f>
struct OcclusionChecker;

struct AkTaskSchedulerDesc;

class AkScene
{
public:
	AkScene() : m_uRefCount(0), m_bGeometryDirty(true) {}
	~AkScene() { Term(); }

	AKRESULT Init();
	void Term();

	// The dirty flag indicates that the geometry in the scene has changed (but not necessarily the portals).
	bool IsDirty() { return m_bGeometryDirty; }
	void SetDirty(bool in_bDirty) { m_bGeometryDirty = in_bDirty; }

	bool IsUnused() { return m_uRefCount == 0; }
	
	AkUInt32 AddRef() { return ++m_uRefCount; }
	AkUInt32 Release() { return --m_uRefCount; }

	AkTriangleIndex& GetTriangleIndex() { return m_TriangleIndex; }
	AkGeometrySetList& GetGeometrySetList() { return m_GeometrySetList; }

	void ClearVisibleEdges();
	
	void ConnectPortal(AkAcousticPortal* in_pPortal, bool in_bConnectToFront, bool in_bConnectToBack);
	void DisconnectPortal(AkAcousticPortal* in_pPortal, AkUInt32 in_FrontOrBack);

	AkScene* pNextLightItem;

private:
	void ConnectPortalToPlane(AkAcousticPortal* in_pPortal, const AkImageSourceTriangle* pTri, AkUInt32 in_FrontOrBack);

	AkTriangleIndex m_TriangleIndex;
	
	AkAcousticPortalArray m_NewPortals;

	AkGeometrySetList m_GeometrySetList;

	AkUInt32 m_uRefCount;

	bool m_bGeometryDirty;
};

typedef AkListBareLight<AkScene> AkSceneList;