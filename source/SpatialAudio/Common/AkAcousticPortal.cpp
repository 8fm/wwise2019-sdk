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
#include <AK/SoundEngine/Common/AkSimdMath.h>
#include "AkAcousticPortal.h"
#include "AkAcousticRoom.h"
#include "AkScene.h"

extern AkSpatialAudioInitSettings g_SpatialAudioSettings;

void AkAcousticPortal::SetParams(const AkPortalParams & in_Params)
{
	m_Center = in_Params.Transform.Position();
	
	m_Front = in_Params.Transform.OrientationFront();
	m_Front.Normalize();
	
	m_Up = in_Params.Transform.OrientationTop(); 
	m_Up.Normalize();
	
	m_Side = m_Up.Cross(m_Front); // Assumes a left-handed coordinate system.

	m_Extent.X = AkMath::Abs( in_Params.Extent.X );
	m_Extent.Y = AkMath::Abs( in_Params.Extent.Y );
	m_Extent.Z = AkMath::Abs( in_Params.Extent.Z );

	m_strName = in_Params.strName;
	m_strName.AllocCopy();

	m_bEnabled = in_Params.bEnabled;
	
	m_bDirty = true;
}

bool AkAcousticPortal::IsDirty() const 
{ 
	if (m_bDirty)
		return true;

	for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
	{
		AkAcousticRoom* pRoom = (AkAcousticRoom*)*it;
		if (pRoom->IsDirty())
			return true;
	}

	return false;
}

void AkAcousticPortal::Traverse(CAkSpatialAudioListener* in_pListener, AkUInt32 in_maxDepth, AkUInt32 in_currentDepth, AkReal32 in_pathLength, AkReal32 in_gain, AkReal32 in_diffraction, AkAcousticPortal** io_portals, AkAcousticRoom** io_rooms)
{
	io_portals[in_currentDepth] = this;

	if (m_sync != in_pListener->GetSync())
	{
		ClearPaths();
		m_sync = in_pListener->GetSync();
	}

	/// Add a path token to the room object
	AkPropagationPath* pPath = m_propagationPaths.Set(io_portals[0]);
	if (pPath && pPath->length > in_pathLength) // keeping the shortest path to each portal (near listener).
	{
		pPath->length = in_pathLength;
		pPath->nodeCount = in_currentDepth + 1;
		pPath->gain = in_gain;
		pPath->diffraction = AkMin(1.0f, in_diffraction);
		AkUInt32 max = AkMin(in_currentDepth + 1, AkDiffractionPath::kMaxLength);
		for (AkUInt32 i = 0; i < max; ++i)
		{
			pPath->portals[i] = io_portals[i];
			pPath->rooms[i] = io_rooms[i];
		}
	}

	for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
	{
		AkAcousticRoom* pRoom = (AkAcousticRoom*)*it;
		if (!AkInArray(pRoom, io_rooms, in_currentDepth + 1))
		{
			pRoom->Traverse(in_pListener, in_maxDepth, in_currentDepth + 1, in_pathLength, GetGain()*in_gain, in_diffraction, io_portals, io_rooms);
		}
	}
}

void AkAcousticPortal::CalcIntersectPoint(const Ak3DVector& in_point0, const Ak3DVector& in_point1, Ak3DVector& out_portalPoint) const
{
	Ak3DVector point0 = in_point0;
	Ak3DVector toP1 = in_point1 - m_Center;
	Ak3DVector toP0 = point0 - m_Center;

	AkReal32 frDotP0 = m_Front.Dot(toP0);
	AkReal32 frDotP1 = m_Front.Dot(toP1);
	bool bSameSide = frDotP0*frDotP1 > 0.f;
	if (bSameSide)
	{
		// points are on the same side of the portal plane. Mirror the point across it first.
		point0 = m_Front * -2.f * frDotP0 + point0;
		toP0 = point0 - m_Center;
	}

	Ak3DVector ray = in_point1 - point0;
	AkReal32 frDotRay = AkMath::Abs(m_Front.Dot(ray));
	AkReal32 si = AkMath::Abs(frDotP0 / frDotRay);

	// the vector from the center of the portal to the point where the ray intersects the portal plane.
	Ak3DVector intersect = toP0 + ray * si;

	Ak3DVector v;
	v.X = AkClamp(intersect.Dot(m_Side), -m_Extent.X, m_Extent.X);
	v.Y = AkClamp(intersect.Dot(m_Up), -m_Extent.Y, m_Extent.Y);
	v.Z = AkClamp(intersect.Dot(m_Front), -m_Extent.Z, m_Extent.Z);
	if (bSameSide)
	{
#if defined(_MSC_VER) && (_MSC_VER >= 1800)
#define COPYSIGN(x,y) \
		copysignf(x,y)
#else
#define COPYSIGN(x,y) \
		x * ((y >= 0) ? 1.f : -1.f)
#endif
		//'extend' the portal point to the nearest portal boarder
		Ak3DVector absDiff = (m_Extent - v.Abs());
		if (absDiff.X < absDiff.Y)
			v.X = COPYSIGN(m_Extent.X, v.X);
		else if (absDiff.Y < absDiff.X)
			v.Y = COPYSIGN(m_Extent.Y, v.Y);
	}

	out_portalPoint = m_Center;
	out_portalPoint = out_portalPoint + m_Side * v.X;
	out_portalPoint = out_portalPoint + m_Up * v.Y;
	out_portalPoint = out_portalPoint + m_Front * v.Z;
}

void AkAcousticPortal::GetPortalPosition(const Ak3DVector& in_listener, AkChannelEmitter& out_chanEm) const
{
 	out_chanEm.position.SetPosition((const AkVector&)m_Center);
 	out_chanEm.position.SetOrientation((const AkVector &)GetFront(), (const AkVector &)GetUp());
 	out_chanEm.uInputChannels = (AkChannelMask)-1;
}

Ak3DVector AkAcousticPortal::GetClosestPointInOpening(const Ak3DVector& in_toPt) const
{
	Ak3DVector d = in_toPt - m_Center;
	Ak3DVector closestPoint = m_Center;
	closestPoint = closestPoint + m_Side * (AkClamp(d.Dot(m_Side), -m_Extent.X, m_Extent.X));
	closestPoint = closestPoint + m_Up * (AkClamp(d.Dot(m_Up), -m_Extent.Y, m_Extent.Y));
	//closestPoint = closestPoint + m_Front * (AkClamp(d.Dot(m_Front), -m_Extent.Z, m_Extent.Z));
	return closestPoint;
}

AkReal32 AkAcousticPortal::GetAparentSpreadAngle(const Ak3DVector& in_listenerPos, const Ak3DVector& in_listenerUp)
{
	// The spread is computed by taking the 4 corners of the portal, projecting them on to the listener plane, 
	//	and then taking the max of the angles between the two projected diagonals.

	// Bottom left corner of portal.
	Ak3DVector p0 = m_Center - m_Side * m_Extent.X - m_Up * m_Extent.Y;
	Ak3DVector toP0 = p0 - in_listenerPos;
	Ak3DVector p0proj = p0 - in_listenerUp * in_listenerUp.Dot(toP0); //poject onto listener plane
	Ak3DVector toP0proj = p0proj - in_listenerPos;
	toP0proj.Normalize();

	// Top right corner
	Ak3DVector p1 = m_Center + m_Side * m_Extent.X + m_Up * m_Extent.Y;
	Ak3DVector toP1 = p1 - in_listenerPos;
	Ak3DVector p1proj = p1 - in_listenerUp * in_listenerUp.Dot(toP1); //poject onto listener plane
	Ak3DVector toP1proj = p1proj - in_listenerPos;
	toP1proj.Normalize();

	// angle between diagonal 0 after being projected onto the listener plane
	AkReal32 angle0 = AK::SpatialAudio::ACos( toP0proj.Dot(toP1proj) );

	// Bottom right
	Ak3DVector p2 = m_Center + m_Side * m_Extent.X - m_Up * m_Extent.Y;
	Ak3DVector toP2 = p2 - in_listenerPos;
	Ak3DVector p2proj = p2 - in_listenerUp * in_listenerUp.Dot(toP2); //poject onto listener plane
	Ak3DVector toP2proj = p2proj - in_listenerPos;
	toP2proj.Normalize();

	// Top left
	Ak3DVector p3 = m_Center - m_Side * m_Extent.X + m_Up * m_Extent.Y;
	Ak3DVector toP3 = p3 - in_listenerPos;
	Ak3DVector p3proj = p3 - in_listenerUp * in_listenerUp.Dot(toP3);//poject onto listener plane
	Ak3DVector toP3proj = p3proj - in_listenerPos;
	toP3proj.Normalize();

	// angle between diagonal 1 after being projected onto the listener plane
	AkReal32 angle1 = AK::SpatialAudio::ACos( toP2proj.Dot(toP3proj) );

	return AkMax(angle0, angle1);
}

AkReal32 AkAcousticPortal::GetAparentSpreadAngle_SIMD(const Ak3DVector& in_listenerPos, const Ak3DVector& in_listenerUp)
{
	AKSIMD_V4F32 center = m_Center.PointV4F32();
	AKSIMD_V4F32 side = m_Side.VectorV4F32();
	AKSIMD_V4F32 up = m_Up.VectorV4F32();

	AKSIMD_V4F32 lpos = in_listenerPos.PointV4F32();
	AKSIMD_V4F32 lup = in_listenerUp.VectorV4F32();

	AKSIMD_V4F32 e_x = AKSIMD_MUL_V4F32(AKSIMD_LOAD1_V4F32(m_Extent.X), side);
	AKSIMD_V4F32 e_y = AKSIMD_MUL_V4F32(AKSIMD_LOAD1_V4F32(m_Extent.Y), up);
	
	AKSIMD_V4F32 p0 = AKSIMD_SUB_V4F32(AKSIMD_SUB_V4F32(center, e_x), e_y);
	AKSIMD_V4F32 toP0 = AKSIMD_SUB_V4F32(p0, lpos);

	AKSIMD_V4F32 p1 = AKSIMD_ADD_V4F32(AKSIMD_ADD_V4F32(center, e_x), e_y);
	AKSIMD_V4F32 toP1 = AKSIMD_SUB_V4F32(p1, lpos);

	AKSIMD_V4F32 p2 = AKSIMD_SUB_V4F32(AKSIMD_ADD_V4F32(center, e_x), e_y);
	AKSIMD_V4F32 toP2 = AKSIMD_SUB_V4F32(p2, lpos);

	AKSIMD_V4F32 p3 = AKSIMD_ADD_V4F32(AKSIMD_SUB_V4F32(center, e_x), e_y);
	AKSIMD_V4F32 toP3 = AKSIMD_SUB_V4F32(p3, lpos);

	AKSIMD_V4F32 to_x, to_y, to_z;
	AkMath::PermuteVectors3(toP0, toP1, toP2, toP3, to_x, to_y, to_z);

	AKSIMD_V4F32 d = AkMath::DotPoduct3_1x4(lup, to_x, to_y, to_z);

	AKSIMD_V4F32 d0 = AKSIMD_SHUFFLE_V4F32(d, d, AKSIMD_SHUFFLE(0, 0, 0, 0));
	AKSIMD_V4F32 toP0Proj = AKSIMD_SUB_V4F32( AKSIMD_SUB_V4F32(p0, AKSIMD_MUL_V4F32(lup, d0)), lpos);
	AKSIMD_V4F32 toP0ProjSqr = AKSIMD_MUL_V4F32(toP0Proj, toP0Proj);

	AKSIMD_V4F32 d1 = AKSIMD_SHUFFLE_V4F32(d, d, AKSIMD_SHUFFLE(1, 1, 1, 1));
	AKSIMD_V4F32 toP1Proj = AKSIMD_SUB_V4F32(AKSIMD_SUB_V4F32(p1, AKSIMD_MUL_V4F32(lup, d1)), lpos);
	AKSIMD_V4F32 toP1ProjSqr = AKSIMD_MUL_V4F32(toP1Proj, toP1Proj);

	AKSIMD_V4F32 d2 = AKSIMD_SHUFFLE_V4F32(d, d, AKSIMD_SHUFFLE(2, 2, 2, 2));
	AKSIMD_V4F32 toP2Proj = AKSIMD_SUB_V4F32(AKSIMD_SUB_V4F32(p2, AKSIMD_MUL_V4F32(lup, d2)), lpos);
	AKSIMD_V4F32 toP2ProjSqr = AKSIMD_MUL_V4F32(toP2Proj, toP2Proj);

	AKSIMD_V4F32 d3 = AKSIMD_SHUFFLE_V4F32(d, d, AKSIMD_SHUFFLE(3, 3, 3, 3));
	AKSIMD_V4F32 toP3Proj = AKSIMD_SUB_V4F32(AKSIMD_SUB_V4F32(p3, AKSIMD_MUL_V4F32(lup, d3)), lpos);
	AKSIMD_V4F32 toP3ProjSqr = AKSIMD_MUL_V4F32(toP3Proj, toP3Proj);

	AKSIMD_V4F32 proj_x, proj_y, proj_z;
	AkMath::PermuteVectors3(toP0ProjSqr, toP1ProjSqr, toP2ProjSqr, toP3ProjSqr, proj_x, proj_y, proj_z);

	AKSIMD_V4F32 len = AKSIMD_SQRT_V4F32(AKSIMD_ADD_V4F32(proj_x, AKSIMD_ADD_V4F32(proj_y, proj_z)));
	toP0Proj = AKSIMD_DIV_V4F32(toP0Proj, AKSIMD_SHUFFLE_V4F32(len, len, AKSIMD_SHUFFLE(0, 0, 0, 0)));
	toP1Proj = AKSIMD_DIV_V4F32(toP1Proj, AKSIMD_SHUFFLE_V4F32(len, len, AKSIMD_SHUFFLE(1, 1, 1, 1)));
	toP2Proj = AKSIMD_DIV_V4F32(toP2Proj, AKSIMD_SHUFFLE_V4F32(len, len, AKSIMD_SHUFFLE(2, 2, 2, 2)));
	toP3Proj = AKSIMD_DIV_V4F32(toP3Proj, AKSIMD_SHUFFLE_V4F32(len, len, AKSIMD_SHUFFLE(3, 3, 3, 3)));

	AKSIMD_V4F32 xx0xx2 = AKSIMD_SHUFFLE_V4F32(toP0Proj, toP2Proj, AKSIMD_SHUFFLE(0, 0, 0, 0));
	AKSIMD_V4F32 yy0yy2 = AKSIMD_SHUFFLE_V4F32(toP0Proj, toP2Proj, AKSIMD_SHUFFLE(1, 1, 1, 1));
	AKSIMD_V4F32 zz0zz2 = AKSIMD_SHUFFLE_V4F32(toP0Proj, toP2Proj, AKSIMD_SHUFFLE(2, 2, 2, 2));

	AKSIMD_V4F32 xx1xx3 = AKSIMD_SHUFFLE_V4F32(toP1Proj, toP3Proj, AKSIMD_SHUFFLE(0, 0, 0, 0));
	AKSIMD_V4F32 yy1yy3 = AKSIMD_SHUFFLE_V4F32(toP1Proj, toP3Proj, AKSIMD_SHUFFLE(1, 1, 1, 1));
	AKSIMD_V4F32 zz1zz3 = AKSIMD_SHUFFLE_V4F32(toP1Proj, toP3Proj, AKSIMD_SHUFFLE(2, 2, 2, 2));

	AKSIMD_V4F32 dot = AkMath::DotPoduct3_4x4(xx0xx2, yy0yy2, zz0zz2, xx1xx3, yy1yy3, zz1zz3);

	AkReal32* fDot = (AkReal32*)&dot;
	AkReal32 res = AkMin(fDot[0], fDot[2]);
	return AK::SpatialAudio::ACos(res);
}

AkRoomID AkAcousticPortal::GetFrontRoom() const
{
	if (m_Links.Length() > FrontRoom)
	{
		AkAcousticRoom* pRoom = (AkAcousticRoom*)m_Links[FrontRoom];
		return pRoom->GetID();
	}

	return AkRoomID();
}

AkAcousticRoom* AkAcousticPortal::GetRoomOnOtherSide(AkRoomID in_room) const
{
	for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
	{
		AkAcousticRoom* pRoom = (AkAcousticRoom*)*it;
		if (pRoom->GetID() != in_room)
			return pRoom;
	}
	return NULL;
}

AkAcousticRoom* AkAcousticPortal::GetCommonRoom(AkAcousticPortal* in_otherPortal) const
{
	for (tLinkIter it0 = m_Links.Begin(); it0 != m_Links.End(); ++it0)
	{
		AkAcousticRoom* pRoom0 = (AkAcousticRoom*)*it0;
		for (tLinkIter it1 = in_otherPortal->m_Links.Begin(); it1 != in_otherPortal->m_Links.End(); ++it1)
		{
			AkAcousticRoom* pRoom1 = (AkAcousticRoom*)*it1;
			if (pRoom0 == pRoom1)
				return pRoom0;
		}
	}
	return NULL;
}

void AkAcousticPortal::GetRooms(AkAcousticRoom*& out_pFrontRoom, AkAcousticRoom*& out_pBackRoom) const
{
	out_pFrontRoom = m_Links.Length() > FrontRoom ? (AkAcousticRoom*)m_Links[FrontRoom] : NULL;
	out_pBackRoom = m_Links.Length() > BackRoom ? (AkAcousticRoom*)m_Links[BackRoom] : NULL;
}

bool AkAcousticPortal::IsConnectedTo(AkRoomID in_room) const
{
	return GetConnectedRoom(in_room) != NULL;
}

AkAcousticRoom* AkAcousticPortal::GetConnectedRoom(AkRoomID in_room) const
{
	for (tLinkIter it = m_Links.Begin(); it != m_Links.End(); ++it)
	{
		AkAcousticRoom* pRoom = (AkAcousticRoom*)*it;
		if (pRoom->GetID() == in_room)
			return pRoom;
	}
	return NULL;
}

Ak3DVector AkAcousticPortal::LocalToWorld(const Ak3DVector& v) const
{
	Ak3DVector res;
	res.X = v.X * m_Side.X + v.Y * m_Up.X + v.Z * m_Front.X;
	res.Y = v.X * m_Side.Y + v.Y * m_Up.Y + v.Z * m_Front.Y;
	res.Z = v.X * m_Side.Z + v.Y * m_Up.Z + v.Z * m_Front.Z;
	res = m_Center + res;
	return res;
}

Ak3DVector AkAcousticPortal::WorldToLocal(const Ak3DVector& in_v) const
{
	Ak3DVector v = in_v - m_Center;
	Ak3DVector res;
	res.X = v.X * m_Side.X		+ v.Y * m_Side.Y	+ v.Z * m_Side.Z;
	res.Y = v.X * m_Up.X		+ v.Y * m_Up.Y		+ v.Z * m_Up.Z;
	res.Z = v.X * m_Front.X		+ v.Y * m_Front.Y	+ v.Z * m_Front.Z;
	AKASSERT( (LocalToWorld(res) - in_v).Abs() < Ak3DVector(0.001f, 0.001f, 0.001f) );
	return res;
}

Ak3DVector AkAcousticPortal::CalcPortalAparentPosition(AkRoomID in_Room, const Ak3DVector& in_listenerPos, AkRoomID in_listenerRoom) const
{
	const AkReal32 kOffset = 0.01f;

	AkReal32 dirToRoom = 1.f;
	AkRoomID frontRoom = GetFrontRoom();
	if (frontRoom != in_Room)
		dirToRoom *= -1.f;

	Ak3DVector toListener = in_listenerPos - GetCenter();
	Ak3DVector offset = toListener;
	offset.Normalize();

	Ak3DVector posLstnrRoom = (in_listenerPos + offset*kOffset);

	Ak3DVector position;

 	AkReal32 depth = m_Extent.Z;
	if (depth > 0.f)
	{
 		Ak3DVector l = WorldToLocal(in_listenerPos); // Listener position, in portal's local coords.
		
		Ak3DVector ptOnPlane; // Point of the listener, projected on to the portal plane and clamped to the portal 'window'
		ptOnPlane.X = AkClamp(l.X, -m_Extent.X, m_Extent.X);
		ptOnPlane.Y = AkClamp(l.Y, -m_Extent.Y, m_Extent.Y);
		ptOnPlane.Z = 0.f;

		Ak3DVector vLen = l - ptOnPlane;

		AkReal32 val = AkMin(vLen.Length() / depth, 1.f);

		// Flip the value to represent the perspective of the listener, if necessary.
		val = (in_Room == in_listenerRoom) ? val : -1 * val;
		AkReal32 penetration = (val + 1.f) / 2.f;

		// The adjacent room position is placed 'behind' the portal, for a smoother transition.
		ptOnPlane.Z = dirToRoom * depth;
		Ak3DVector posAdjRoom = LocalToWorld(ptOnPlane);
			
		// Interpolate.
		position = posAdjRoom * (1.f - penetration) + posLstnrRoom *  penetration;
	}
	else
	{
		position = (in_Room == in_listenerRoom) ? posLstnrRoom : GetCenter();
	}

	return position;
}

bool AkAcousticPortal::GetTransitionRoom(const Ak3DVector& in_Point, 
											AkAcousticRoom*& out_pRoomA, AkAcousticRoom*& out_pRoomB,
											AkReal32& out_fRatio
										) const
{
	AkUInt32 len = m_Links.Length();
	if (len >= 2)
	{
		Ak3DVector pt = in_Point - m_Center;

		if (fabsf(pt.Dot(m_Side)) <= m_Extent.X)
		{
			if (fabsf(pt.Dot(m_Up)) <= m_Extent.Y)
			{
				AkReal32 ptDotZ = pt.Dot(m_Front);
				if (fabsf(ptDotZ) <= m_Extent.Z)
				{
					out_fRatio = (m_Extent.Z + fabsf(ptDotZ)) / (2.f*m_Extent.Z);

					if (ptDotZ > 0.f)
					{
						out_pRoomA = (AkAcousticRoom*)m_Links[FrontRoom];
						out_pRoomB = (AkAcousticRoom*)m_Links[BackRoom];
					}
					else
					{
						out_pRoomA = (AkAcousticRoom*)m_Links[BackRoom];
						out_pRoomB = (AkAcousticRoom*)m_Links[FrontRoom];
					}
					return true;
				}
			}
		}
	}
	return false;
}

Ak3DVector AkAcousticPortal::GetRayTracePosition(AkUInt32 FrontOrBack)
{
	AkPortalGeometry& geom = GetGeometry(FrontOrBack);
	if (geom.m_Scene != NULL)
	{
		// Take the average of the center of all the edges for the portal position. Notably, this position is different than the portal Center(), 
		// because it is derived from the plane that the portal intersects.

		Ak3DVector position = (geom.m_Edges[0].Center() +
			geom.m_Edges[1].Center() +
			geom.m_Edges[2].Center() +
			geom.m_Edges[3].Center())
			/ 4.f;

		// And as is customary, we add a small offset to the position so that it does not intersect with the plane.
		position = position + GetFront() * 0.1f * (FrontOrBack == AkAcousticPortal::FrontRoom ? 1.0f : -1.0f);

		return position;
	}
	else
	{
		return GetCenter();
	}
}

void AkAcousticPortal::ClearGeometry()
{
	if (m_Geometry[0].m_Scene != NULL)
	{
		m_Geometry[0].m_Scene->DisconnectPortal(this,0);
		m_Geometry[0].~AkPortalGeometry();
		AkPlacementNew(&m_Geometry[0]) AkPortalGeometry();
	}

	if (m_Geometry[1].m_Scene != NULL)
	{
		m_Geometry[1].m_Scene->DisconnectPortal(this,1);
		m_Geometry[1].~AkPortalGeometry();
		AkPlacementNew(&m_Geometry[1]) AkPortalGeometry();
	}

	m_stochasticRays[0].ClearRays();
	m_stochasticRays[1].ClearRays();
}

void AkAcousticPortal::SetScene(AkScene* in_pScene, AkUInt32 in_FrontOrBack)
{
	if (m_Geometry[in_FrontOrBack].m_Scene != NULL)
	{
		m_Geometry[in_FrontOrBack].m_Scene->DisconnectPortal(this, in_FrontOrBack);
		m_Geometry[in_FrontOrBack].~AkPortalGeometry();
		AkPlacementNew(&m_Geometry[in_FrontOrBack]) AkPortalGeometry();
	}

	m_Geometry[in_FrontOrBack].m_Scene = in_pScene;
}

void AkAcousticPortal::BuildDiffractionPaths(
	AkRoomID in_emitterRoom,
	CAkSpatialAudioListener* in_pListener,
	const Ak3DVector& in_listenerPos)
{
	AkAcousticRoom *pEmitterRoom, *pAdjacentRoom;
	GetRooms(pEmitterRoom, pAdjacentRoom);
	if (pEmitterRoom->GetID() != in_emitterRoom)
	{
		AkAcousticRoom *pTemp = pEmitterRoom;
		pEmitterRoom = pAdjacentRoom;
		pAdjacentRoom = pTemp;
	}
	AKASSERT(pEmitterRoom->GetID() == in_emitterRoom);

	Ak3DVector portalAparentListenerPosition = in_listenerPos;
	Ak3DVector portalAparentEmitterPosition = CalcPortalAparentPosition(pEmitterRoom->GetID(), in_listenerPos, pAdjacentRoom->GetID());

	AkTransform portalTransform;
	portalTransform.Set((const AkVector&)portalAparentEmitterPosition, (const AkVector&)pEmitterRoom->GetFront(), (const AkVector&)pEmitterRoom->GetUp());

	GetPropagationPaths(in_pListener).BuildPortalDiffractionPaths(
		in_pListener->GetPosition(),
		in_pListener->m_PathsToPortals,
		in_pListener->GetActiveRoom(),
		portalTransform,
		GetID(),
		pEmitterRoom->GetID(),
		in_pListener->GetVisibilityRadius(),
		m_diffractionPaths);
}

bool AkAcousticPortal::EnqueueTaskIfNeeded(const AkAcousticRoom* in_pRoom, AkSAPortalRayCastingTaskQueue& io_queue)
{
	if (in_pRoom->GetScene() != NULL &&
		!in_pRoom->GetScene()->GetGeometrySetList().IsEmpty())
	{
		AkUInt32 frontOrBack = (GetFrontRoom() == in_pRoom->GetID() ? FrontRoom : BackRoom);

		if (m_stochasticRays[frontOrBack].GetCurrentGroupNumber() < g_SpatialAudioSettings.uNumberOfPrimaryRays)
		{
			AkSAPortalRayCastingTaskData* pTask = io_queue.Set(this);
			if (pTask != nullptr)
			{
				pTask->pPortal = this;

				if (frontOrBack == FrontRoom)
					pTask->roomFlags |= AkSAPortalRayCastingTaskData::Front;
				else
					pTask->roomFlags |= AkSAPortalRayCastingTaskData::Back;
			}

			return true;
		}
	}

	return false;
}