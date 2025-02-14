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

#include <AK/SpatialAudio/Common/AkSpatialAudioTypes.h>
#include <AK/SpatialAudio/Common/AkSpatialAudio.h>

enum AkSpatialAudioMsgIDs
{
	AkSpatialAudioMsgID_RegisterListener,
	AkSpatialAudioMsgID_UnregisterListener,
	AkSpatialAudioMsgID_SetImageSource,
	AkSpatialAudioMsgID_RemoveImageSource,
	AkSpatialAudioMsgID_ClearImageSources,
	AkSpatialAudioMsgID_PortalObstructionAndOcclusion,
	AkSpatialAudioMsgID_SetGeometry,
	AkSpatialAudioMsgID_RemoveGeometry,
	AkSpatialAudioMsgID_SetRoom,
	AkSpatialAudioMsgID_RemoveRoom,
	AkSpatialAudioMsgID_SetGameObjectRoom,
	AkSpatialAudioMsgID_SetPortal,
	AkSpatialAudioMsgID_RemovePortal,
	AkSpatialAudioMsgID_SetEarlyReflectionsAuxSend,
	AkSpatialAudioMsgID_SetEarlyReflectionsVolume,
	AkSpatialAudioMsgID_SetReflectionsOrder,
	//
	AkSpatialAudioMsgID_Count
};

struct AkSpatialAudioMsg_ListenerRegistration
{
	AkGameObjectID gameObjectID;
};

struct AkSpatialAudioMsg_Position
{
	AkGameObjectID gameObjectID;
	AkTransform sourcePos;
};

struct AkSpatialAudioMsg_PortalObstructionAndOcclusion
{
	AkPortalID portalID;
	AkReal32 obstruction;
	AkReal32 occlusion;
};

struct AkSpatialAudioMsg_SetGameObjInRoom
{
	AkGameObjectID gameObjectID;
	AkRoomID roomID;
};

struct AkSpatialAudioMsg_SetRoom
{
	AkRoomID roomID;
	AkRoomParams params;
};

struct AkSpatialAudioMsg_SetPortal
{
	AkPortalID portalID;
	AkPortalParams params;
};

struct AkSpatialAudioMsg_SetGeometry
{
	AkGeometrySetID geometryID;
	AkGeometryParams params;
};

struct AkSpatialAudioMsg_SetEarlyReflectionsAuxSend
{
	AkGameObjectID gameObjectID;
	AkAuxBusID auxBus;
};

struct AkSpatialAudioMsg_SetEarlyReflectionsVolume
{
	AkGameObjectID gameObjectID;
	AkReal32 volume;
};

struct AkSpatialAudioMsg_SetReflectionsOrder
{
	AkUInt32 reflectionsOrder;
	bool updatePaths;
};