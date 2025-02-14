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
#include "AkDiffractionPath.h"
#include "AkDiffractionEdge.h"
#include "AkImageSource.h"
#include "AkSpatialAudioTasks.h"

class CAkSpatialAudioListener;
class AkScene;

struct AkPortalGeometry
{
	AkPortalGeometry() : m_Scene(NULL) 
	{
		for (AkUInt32 i = 0; i < 4; ++i)
			m_Edges[i].isPortalEdge = true;
	}

	// Two "negative space" triangle that fill the portal 
	AkImageSourceTriangle			m_Tris[2];

	// Four edges of the portal
	CAkDiffractionEdge				m_Edges[4];

	// Associated scene;
	AkScene*						m_Scene;
};

class AkAcousticPortal : public AkAcousticNode
{
public:

	enum
	{
		FrontRoom = 0,
		BackRoom,
		
		MaxLinkedRooms
	};

	AkAcousticPortal(AkPortalID in_PortalID) : key(in_PortalID)
		, pNextItem(NULL)
		, m_sync(-1)
		, m_fGain(1.f)
		, m_fWetDiffraction(0.f)
		, m_fObstruction(0.f)
		, m_fOcclusion(0.f)
		, m_bEnabled(false)
	{
		m_Links.Reserve(MaxLinkedRooms);
	}

	~AkAcousticPortal()
	{
		ClearGeometry();
		m_propagationPaths.Term();
		m_diffractionPaths.Term();
		m_stochasticRays[0].Term();
		m_stochasticRays[1].Term();
	}

	void SetParams(const AkPortalParams & in_Params);

	AkForceInline AKRESULT SetRoom(AkAcousticRoom* in_pRoom)
	{
		return AkAcousticNode::Link((AkAcousticNode*)in_pRoom);
	}

	AkForceInline AKRESULT RemoveRoom(AkAcousticRoom* in_pRoom)
	{
		return AkAcousticNode::Unlink((AkAcousticNode*)in_pRoom);
	}

	AkForceInline void ClearRooms()
	{
		return AkAcousticNode::ClearLinks();
	}

	const Ak3DVector& GetCenter() const { return m_Center; }
	const Ak3DVector& GetFront() const { return m_Front; }
	const Ak3DVector& GetUp() const { return m_Up; }
	const Ak3DVector& GetExtent() const { return m_Extent; }

	bool IsEnabled() const { return m_bEnabled; }

	bool IsDirty() const;

	AkReal32 GetGain() const { return m_fGain; }

	const AK::SpatialAudio::OsString& GetName() const { return m_strName; }

	AkPortalID GetID() const { return key; }

	AkPortalID			key;
	AkAcousticPortal* pNextItem;

	void Traverse(CAkSpatialAudioListener* in_pERL, AkUInt32 in_maxDepth, AkUInt32 in_currentDepth, AkReal32 in_pathLength, AkReal32 in_gain, AkReal32 in_diffraction, AkAcousticPortal** in_portals, AkAcousticRoom** io_rooms);

	void CalcIntersectPoint(const Ak3DVector& in_point0, const Ak3DVector& in_point1, Ak3DVector& out_portalPoint) const;

	void GetPortalPosition(const Ak3DVector& in_listenerPos, AkChannelEmitter& out_portalPosition) const;

	Ak3DVector GetClosestPointInOpening(const Ak3DVector& in_toPt) const;

	AkReal32 GetAparentSpreadAngle(const Ak3DVector& in_listenerPos, const Ak3DVector& in_listenerUp);
	AkReal32 GetAparentSpreadAngle_SIMD(const Ak3DVector& in_listenerPos, const Ak3DVector& in_listenerUp);

	// Front room is room in positive-Z direction of portal orientation.
	AkRoomID GetFrontRoom() const;
	
	AkAcousticRoom* GetRoomOnOtherSide(AkRoomID in_room) const;
	AkAcousticRoom* GetCommonRoom(AkAcousticPortal* in_otherPortal) const;
	void GetRooms(AkAcousticRoom*& out_pFrontRoom, AkAcousticRoom*& out_pBackRoom) const;
	bool IsConnectedTo(AkRoomID in_room) const;
	AkAcousticRoom* GetConnectedRoom(AkRoomID in_room) const;

	// in_dirToListener == true, we are positioning the room that the listener is in.
	// in_dirToListener == false, we are positioning the room behind the portal.
	Ak3DVector CalcPortalAparentPosition(AkRoomID in_Room, const Ak3DVector& in_listenerPos, AkRoomID in_listenerRoom) const;

	AkReal32 GetWetDiffraction() const { return m_fWetDiffraction; }
	void SetWetDiffraction(AkReal32 in_diffraction) { m_fWetDiffraction = in_diffraction; }

	bool GetTransitionRoom(const Ak3DVector& in_Point,
							AkAcousticRoom*& out_pRoomA, AkAcousticRoom*& out_pRoomB,
							AkReal32& out_fRatio
						) const;

	void SetObstructionAndOcclusion(AkReal32 in_fObstruction, AkReal32 in_fOcclusion) 
	{ 
		m_fObstruction = in_fObstruction; 
		m_fOcclusion = in_fOcclusion;
	}

	AkReal32 GetObstruction() const { return m_fObstruction; }
	AkReal32 GetOcclusion() const { return m_fOcclusion; }

	const AkPropagationPathArray& GetPropagationPaths(const CAkSpatialAudioListener* in_pERL) const { return m_propagationPaths; }
	
	CAkDiffractionPaths& GetDiffractionPaths() { return m_diffractionPaths; }

	void ClearPaths() 
	{ 
		m_propagationPaths.RemoveAll(); 
		m_diffractionPaths.Reset();
	}

	AkUInt32 GetSync() const { return m_sync; }

	// Portal geometry accessors.
	AkPortalGeometry& GetGeometry(AkUInt32 in_FrontOrBack)					{ return m_Geometry[in_FrontOrBack]; }
	CAkDiffractionEdge&	GetEdge(AkUInt32 in_FrontOrBack, AkUInt32 in_idx)	{ return m_Geometry[in_FrontOrBack].m_Edges[in_idx]; }
	AkImageSourceTriangle& GetTri(AkUInt32 in_FrontOrBack, AkUInt32 in_idx) { return m_Geometry[in_FrontOrBack].m_Tris[in_idx]; }

	Ak3DVector GetRayTracePosition(AkUInt32 FrontOrBack);

	Ak3DVector WorldToLocal(const Ak3DVector& in_World) const;
	Ak3DVector LocalToWorld(const Ak3DVector& in_Local) const;

	// Reset edges and triangles to default.
	void ClearGeometry();
	
	// Assign a set of portal geometry to a scene.
	void SetScene(AkScene* in_pScene, AkUInt32 in_FrontOrBack);

	StochasticRayCollection& GetStochasticRays(AkUInt32 in_room) { return m_stochasticRays[in_room]; }

	void BuildDiffractionPaths(
		AkRoomID in_emitterRoom,
		CAkSpatialAudioListener* in_pListener,
		const Ak3DVector& in_listenerPos);

	bool EnqueueTaskIfNeeded(const AkAcousticRoom* in_pRoom, AkSAPortalRayCastingTaskQueue& io_queue);

private:

	Ak3DVector						m_Center;

	Ak3DVector						m_Extent;
	Ak3DVector						m_Side;

	AkPropagationPathArray			m_propagationPaths;
	
	CAkDiffractionPaths				m_diffractionPaths;

	AkUInt32						m_sync;

	AkReal32						m_fGain;
	AkReal32						m_fWetDiffraction;
	AkReal32						m_fObstruction;
	AkReal32						m_fOcclusion;

	// Two possible sets, one for each room, if the rooms have two different scenes.
	AkPortalGeometry				m_Geometry[MaxLinkedRooms];

	/// Rays from the ray casting step (from the portal)
	///
	StochasticRayCollection m_stochasticRays[MaxLinkedRooms];

	bool							m_bEnabled;
};