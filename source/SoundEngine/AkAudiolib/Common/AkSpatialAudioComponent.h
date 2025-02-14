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

#pragma once

#include <AK/SpatialAudio/Common/AkSpatialAudioTypes.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkVectors.h>
#include <AK/Tools/Common/AkListBare.h>
#include "AkCommon.h"
#include "AkGameObject.h"
#include "Ak3DParams.h"

class CAkBehavioralCtx;
class CAkSpatialAudioVoice;

struct AkRoomRvbSend
{
	AkRoomRvbSend() : auxBus(AK_INVALID_AUX_ID), ctrlVal(0.f), wallOcclusion(1.f), priority(0) {}

	AkForceInline bool IsActive() const { return auxBus != AK_INVALID_AUX_ID && ctrlVal > 0.f; }

	AkRoomID room;
	AkAuxBusID auxBus;
	AkReal32 ctrlVal;
	AkReal32 wallOcclusion;
	AkInt32	priority;
};

typedef AkListBare<CAkSpatialAudioVoice, AkListBareNextItem, AkCountPolicyNoCount, AkLastPolicyNoLast> AkSpatialAudioVoiceList;

struct AkAuxBusRef
{
	AkAuxBusRef() : key(AK_INVALID_AUX_ID), refCount(0) {}
	AkUniqueID key;
	AkInt32 refCount;
};

typedef AkSortedKeyArray< AkUniqueID, AkAuxBusRef, ArrayPoolDefault> AkAuxBusRefs;

class CAkSpatialAudioComponent: public CAkTrackedGameObjComponent<GameObjComponentIdx_SpatialAudioObj>
{
public:
	CAkSpatialAudioComponent();
	~CAkSpatialAudioComponent();

	void TrackVoice(CAkSpatialAudioVoice* in_pVoice);
	void UntrackVoice(CAkSpatialAudioVoice* in_pVoice);
	void UpdateTracking(CAkSpatialAudioVoice* in_pVoice);

	bool HasReflections() const;
	bool HasDiffraction() const;
	AkReal32 GetMaxDistance() const;

	AkForceInline AkRoomID GetAssignedRoom() const { return m_uRoomID; }
	AkForceInline AkRoomID GetActiveRoom() const { return m_RoomA.room; }

	AkForceInline const AkRoomRvbSend& GetActiveRoomRvbSend() const { return m_RoomA; }
	AkForceInline AkRoomID GetTransitionRoom() const { return m_RoomB.room; }
	AkForceInline const AkRoomRvbSend& GetTransitionRoomRvbSend() const { return m_RoomB; }
	
	AkReal32 GetWallOcclusion() const
	{
		return m_RoomA.wallOcclusion * m_RatioAtoB + m_RoomB.wallOcclusion * (1.f - m_RatioAtoB);
	}

	AkReal32 GetTransitionRatio() const { return m_RatioAtoB; }
	bool IsTransitioning() const { return m_Portal.IsValid(); }

	AkPortalID GetPortal() const { return m_Portal; }

	AkForceInline bool IsDirty() const { return m_bRoomDirty || m_bPositionDirty; }
	AkForceInline bool IsRoomDirty() const { return m_bRoomDirty; }
	AkForceInline bool IsPositionDirty() const { return m_bPositionDirty; }
	AkForceInline void SetPositionDirty() { m_bPositionDirty = true; }
	AkForceInline bool HasBeenActivated() const { return m_bActivated; }

	AkForceInline void FinishUpdate()
	{
		m_bPositionDirty = false;
		m_bRoomDirty = false;
		m_bActivated = false;
		m_uSync++;
	}

	AkForceInline AkUInt32 GetSync() const { return m_uSync; }

	AkForceInline void SetCurrentRoom(AkRoomID in_uRoomID)
	{
		AkRoomID oldRoom = GetActiveRoom();
		m_uRoomID = in_uRoomID;
		m_bRoomDirty = m_bRoomDirty || (oldRoom != GetActiveRoom());
	}

	bool UpdatePosition(AkReal32 in_fThresh);

	void GetRoomReverbAuxSends(AkAuxSendArray & io_arAuxSends, AkReal32 in_fVol, AkReal32 in_fLPF, AkReal32 in_fHPF) const;
	void GetSendsForTransmission(AkRoomRvbSend& in_ListenerRoomSend, AkAuxSendArray & io_arAuxSends, AkReal32 in_fVol, AkReal32 in_fLPF, AkReal32 in_fHPF) const;
	
	void SetRoomsAndAuxs(const AkRoomRvbSend& in_roomA, const AkRoomRvbSend& in_roomB, AkReal32 in_fRatioAtoB, AkPortalID in_currentPortal);

	// Set ER bus from the API via AK::SpatialAudio::SetEarlyReflectionsAuxSend
	void SetEarlyReflectionsAuxSend(AkAuxBusID in_aux);

	// Set ER vol from the API via AK::SpatialAudio::SetEarlyReflectionsVolume
	void SetEarlyReflectionsVolume(AkReal32 in_vol);

	// Called by pipeline to gather ER aux sends for a particular voice.
	void GetEarlyReflectionsAuxSends(AkAuxSendArray & io_arAuxSends, AkAuxBusID in_reflectionsAux, AkReal32 in_fVol);

	// Set/Unset sends for image source specified by the custom image source api (AK::SpatialAudio::SetImageSource())
	void SetImageSourceAuxBus(AkAuxBusID in_auxID);
	void UnsetImageSourceAuxBus(AkAuxBusID in_auxID);

	// Get all possible aux bus sends that are specified by active voices in the wwise authoring tool.
	const AkAuxBusRefs& GetReflectionsAuxBusses() { return m_ReflectionsAuxBusses; }

	// Switch to enable/disable aux sends based on whether or not there are active reflections to render. Applies only to geometric reflections.
	// Aux sends to reflect must be disabled so that voices can go virtual.
	void EnableReflectionsAuxSend(bool in_bEnable) { m_bEnableReflectionsAuxSend = in_bEnable; }

	// Called by pipeline to enable/disable aux sends. 
	// Returns true when there are active geometric reflections. 
	bool HasReflectionsAuxSends() {	return m_bEnableReflectionsAuxSend; }

	// Called by pipeline to enable/disable aux sends. 
	// Returns true when there are active custom image source sends.
	bool HasCustomImageSourceAuxSends() { return !m_ImageSourceAuxBus.IsEmpty(); }

	// Called by pipeline to enable/disable aux sends. 
	// Returns true when a geometric reflections aux bus has been specified through the API for all voices on this game object.
	bool HasEarlyReflectionsBusFromAPI() { return m_EarlyReflectionsVolume > 0.f && m_EarlyReflectionsBus != AK_INVALID_AUX_ID; }
	AkReal32 GetEarlyReflectionsBusVolumeFromAPI() { return m_EarlyReflectionsVolume; }

	AkVolumeDataArray&	GetDiffractionRays() { return m_DiffractionRays; }
	AkVolumeDataArray&	GetRoomSendRays() { return m_RoomSendRays; }
	AkVolumeDataArray&	GetTransmissionRays() { return m_TransmissionRays; }
	void UpdateUserRays(AkGameObjectID in_spatialAudioListener);
	void ClearVolumeRays();

	// Called by the behavioral context
	AkReal32 GetRayVolumeData(AkVolumeDataArray& out_arVolumeData, bool in_bEnableDiffraction, bool in_bEnableRooms);

	// Return true if in_auxId is an early reflections aux bus assigned to this object.
	bool IsReflectionsAuxBus(AkUniqueID in_auxId);

	void UpdateBuiltInParams(AkGameObjectID in_listenerID);

private:
	void TrackReflectionsAuxBus(CAkSpatialAudioVoice* in_pVoice);
	void UntrackReflectionsAuxBus(CAkSpatialAudioVoice* in_pVoice);

	// Rays pertaining to the spatial audio listener, including virtual positions due to diffraction.
	AkVolumeDataArray	m_DiffractionRays;
	
	// Rays pertaining to the room game objects.
	AkVolumeDataArray	m_RoomSendRays;
	
	// Rays pertaining to sends to listeners other than the spatial audio listener and room objects. Set by the user via the API. Often empty.
	AkVolumeDataArray	m_UserRays;		

	AkVolumeDataArray	m_TransmissionRays;

	// Aux bus sends for reflections. Ref counted by voices that use spatial audio.
	AkAuxBusRefs m_ReflectionsAuxBusses;

	// Aux bus sends for custom image sources, for the AK::SpatialAudio::SetImageSource() API.
	AkUniqueIDSet m_ImageSourceAuxBus;

	Ak3DVector m_lastUpdatePos;

	// This is the room that is set via the API.
	AkRoomID m_uRoomID;

	// This is the portal that the object is in; invalid if not in a portal.
	AkPortalID m_Portal;

	// ER bus and volume that have been set by the AK::SpatialAudio::SetEarlyReflectionsAuxSend() API.
	AkAuxBusID m_EarlyReflectionsBus;
	AkReal32 m_EarlyReflectionsVolume;

	// m_uRoomA and m_uRoomB are rooms that have been determined via a portal containment/transition test.
	// m_uRoomA is always the room that the game object is in,  m_uRoomB is a room that the game object is not (quite) in, but very close to (transitioning in to/out of).  

	AkRoomRvbSend m_RoomA;
	AkRoomRvbSend m_RoomB;
	AkReal32 m_RatioAtoB;

	AkSpatialAudioVoiceList m_List;

	AkInt32 m_ReflectionsVoiceCount;
	AkInt32 m_DiffractionVoiceCount;

	AkReal32 m_MaxDistance;

	AkUInt32 m_uSync;

	bool m_bPositionDirty;
	bool m_bRoomDirty;
	bool m_bEnableReflectionsAuxSend;
	bool m_bActivated;
};

// This component is attached to a game object that represents a spatial audio room.
//
class CAkSpatialAudioRoomComponent : public CAkTrackedGameObjComponent < GameObjComponentIdx_SpatialAudioRoom >
{
public:
	CAkSpatialAudioRoomComponent() : m_auxID(AK_INVALID_AUX_ID) {}

	AkRoomID GetRoomID() const { return (AkRoomID)ID(); }
	
	void SetAuxID(AkAuxBusID in_auxID) { m_auxID = in_auxID; }
	AkAuxBusID GetAuxID() const { return m_auxID; }

private:
	AkAuxBusID m_auxID;
};