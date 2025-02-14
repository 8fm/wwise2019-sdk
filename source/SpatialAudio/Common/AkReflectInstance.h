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

//////////////////////////////////////////////////////////////////////
//
// AkSpatialAudioComponent.h
//
//////////////////////////////////////////////////////////////////////


#ifndef _AK_REFLECT_INSTANCE_H_
#define _AK_REFLECT_INSTANCE_H_

#include <AK/SpatialAudio/Common/AkSpatialAudio.h>
#include <AK/Tools/Common/AkSet.h>

#include "AkGameObject.h"

class CAkSpatialAudioEmitter;

struct AkImageSource
{
	AkImageSource() {}
	~AkImageSource() {}

	static AkForceInline AkImageSourceID & Get(AkImageSource & in_item)
	{
		return in_item.uSrcID;
	}
	static AkForceInline void Move(AkImageSource& in_Dest, AkImageSource& in_Src)
	{
		in_Dest.uRoomID = in_Src.uRoomID;
		in_Dest.uSrcID = in_Src.uSrcID;
		in_Dest.virtSrc.params = in_Src.virtSrc.params;
		in_Dest.virtSrc.texture = in_Src.virtSrc.texture;
		in_Dest.virtSrc.name.Transfer(in_Src.virtSrc.name);
	}

	static AkForceInline bool IsTrivial() { return false; }

	AkImageSourceID uSrcID;
	AkRoomID uRoomID;
	AkImageSourceSettings virtSrc;
};
typedef AkSortedKeyArray<AkImageSourceID, AkImageSource, ArrayPoolSpatialAudio, AkImageSource, AkGrowByPolicy_DEFAULT, AkImageSource> ImageSourcePerID;

class CAkReflectInstance
{
public:
	CAkReflectInstance()
		: m_uMaxVirtSrc(0)
		, m_pData(NULL)
	{}

	~CAkReflectInstance();

	void Term();

	AkUInt32 NumActiveImageSources() const { return m_pData ? m_pData->uNumImageSources : 0; }

	bool IsActive() { return NumActiveImageSources() > 0; }

	// Ensure that there is room for at least in_uNumSrcs virtual sources in the buffer.
	bool Reserve(AkUInt32 in_uNumSrcs);

	bool ResizeVirtualSources(AkUInt32 in_uNumSrcs);

	//Get a virtual source, return a reference to an existing source
	AkReflectImageSource& GetVirtualSource(AkUInt32 in_uIdx) const
	{
		AKASSERT(m_pData != NULL && in_uIdx < m_pData->uNumImageSources);
		return m_pData->arSources[in_uIdx];
	}

	//Add a new virtual source, increment the count, return a pointer to the newly added source
	AkReflectImageSource* AddVirtualSource() const
	{
		if (m_pData != NULL && m_pData->uNumImageSources < m_uMaxVirtSrc)
		{
			AkReflectImageSource* pSource = &m_pData->arSources[m_pData->uNumImageSources];
			m_pData->uNumImageSources++;
			return pSource;
		}
		return NULL;
	}

	void UpdateImageSource(AkUInt32 in_uIdx, AkImageSourceID in_srcID, AkImageSourceSettings& in_imgSrcSettings)
	{
		AKASSERT(m_pData != NULL && in_uIdx < m_pData->uNumImageSources);
		m_pData->arSources[in_uIdx].uID = in_srcID;
		m_pData->arSources[in_uIdx].params = in_imgSrcSettings.params;
		m_pData->arSources[in_uIdx].texture = in_imgSrcSettings.texture;
		m_pData->arSources[in_uIdx].SetName(in_imgSrcSettings.name.Get());
	}
	
protected:
	void PreparePluginData(const AkGameObjectID & in_listener, void*& out_pData, AkUInt32& out_uSize);

	AKRESULT AllocVirtualSources(AkUInt32 in_uMaxSrc);

	AkUInt32 m_uMaxVirtSrc;
	AkReflectGameData* m_pData;
};

// Class for controlling Reflect data pertaining to geometric reflections generated via spatial audio.
class CAkGeometricReflectInstance : public CAkReflectInstance
{
public:
	CAkGeometricReflectInstance() : CAkReflectInstance() {}

	~CAkGeometricReflectInstance();

	// Must be called before deleting object. in_pEmitter must be invariant throughout the lifetime of this object.
	void Term(CAkSpatialAudioEmitter* in_pEmitter);

	void PackageReflectors(CAkSpatialAudioEmitter* in_pERE);

	void SendPluginData(const AkGameObjectID & in_listener, const AkGameObjectID & in_emitter);

protected:

	// Set of aux buses that we have send data to at some point. 
	// We add auxs as necessary, according to the AkSpatialAudioComponent, but only remove them all in one go, since we don't know how long
	// tail processing for reflect will take. 
	// It is also important to know what data to clean up in Term().
	AkUniqueIDSet m_activeAuxs;
};

// Class for controlling Reflect data pertaining to custom image sources set via the SetImageSource API.
class CAkCustomReflectInstance: public CAkReflectInstance
{
public:
	CAkCustomReflectInstance() 
		: CAkReflectInstance()
		, m_AuxBusID(AK_INVALID_UNIQUE_ID) 
	{}

	// Must be called before deleting object. in_pEmitter must be invariant throughout the lifetime of this object.
	void Term(CAkSpatialAudioEmitter* in_pEmitter);

	// Has at least one custom image source. Note that they may not be active due to room restraints, not to be confused with IsActive() or NumActiveImageSources().
	bool HasCustomImageSources() { return !m_customImageSources.IsEmpty(); }

	void SetCustomImageSource(AkImageSourceID in_srcId, AkRoomID in_roomID, AkImageSourceSettings& in_iss);
	void RemoveCustomImageSource(AkImageSourceID in_srcId);
	void ClearCustomImageSources();

	void PackageReflectors(CAkSpatialAudioEmitter* in_pERE);

	//Get key policy for AkSortedKeyArray.
	static AkForceInline AkUniqueID & Get(CAkCustomReflectInstance & in_item)
	{
		return in_item.m_AuxBusID;
	}

	void Transfer(CAkCustomReflectInstance& in_Src)
	{
		m_AuxBusID = in_Src.m_AuxBusID;
		m_pData = in_Src.m_pData;
		m_uMaxVirtSrc = in_Src.m_uMaxVirtSrc;

		in_Src.m_AuxBusID = AK_INVALID_AUX_ID;
		in_Src.m_pData = NULL;
		in_Src.m_uMaxVirtSrc = 0;

		m_customImageSources.Transfer(in_Src.m_customImageSources);
	}

	// Note: in_emitter must be invariant throughout the lifetime of this object.
	void SendPluginData(const AkGameObjectID & in_listener, const AkGameObjectID & in_emitter);

protected:
	AkUniqueID m_AuxBusID;

	ImageSourcePerID m_customImageSources;
};


// Collection of CAkCustomReflectInstance
class CAkCustomReflectInstances : private AkSortedKeyArray<AkUniqueID, CAkCustomReflectInstance, ArrayPoolSpatialAudio, CAkCustomReflectInstance, AkGrowByPolicy_DEFAULT, AkTransferMovePolicy<CAkCustomReflectInstance> >
{
public:
	CAkCustomReflectInstances() 
		: AkSortedKeyArray<AkUniqueID, CAkCustomReflectInstance, ArrayPoolSpatialAudio, CAkCustomReflectInstance, AkGrowByPolicy_DEFAULT, AkTransferMovePolicy<CAkCustomReflectInstance> >()
		, m_bIsActive(false) 
	{}
	
	void Term(CAkSpatialAudioEmitter* in_pEmitter);

	void SetCustomImageSource(AkUniqueID in_auxBusID, AkUniqueID in_srcId, AkRoomID in_roomID, AkImageSourceSettings& in_iss);
	void RemoveCustomImageSource(AkUniqueID in_auxBusID, AkUniqueID in_srcId);
	void ClearCustomImageSources(AkUniqueID in_auxBusID);

	void PackageReflectors(CAkSpatialAudioEmitter* in_pEmitter);

	// True if at least one reflect instance has active image sources. 
	bool IsActive() const { return m_bIsActive; }

private:
	bool m_bIsActive;
};

#endif