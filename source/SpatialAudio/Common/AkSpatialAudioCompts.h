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


#ifndef _AK_EARLY_REFLECT_COMPT_H_
#define _AK_EARLY_REFLECT_COMPT_H_

#include <AK/SpatialAudio/Common/AkSpatialAudio.h>

#include "AkGameObject.h"
#include "AkCommon.h"

#include <AK/Tools/Common/AkSet.h>
#include <AK/Plugin/AkReflectGameData.h>

#include "AkImageSource.h"
#include "AkDiffractionPath.h"
#include "AkReflectInstance.h"
#include "AkStochasticPath.h"

#include "AkSpatialAudioComponent.h"
#include "AkRegisteredObj.h"

#define AK_REFLECT_PLUGIN_ID	171

class CAkSpatialAudioListener;
class AkAcousticRoom;

class AkGameObjectRefSet
{
public:
	typedef AkSet< CAkGameObject*, ArrayPoolSpatialAudio > GameObjPtrSet;

	AkGameObjectRefSet() {}
	~AkGameObjectRefSet() 
	{ 
		Clear(); 
		m_refdObjs.Term(); 
	}

	bool AddRef(CAkGameObject* pObj)
	{
		if (!m_refdObjs.Exists(pObj) &&
			m_refdObjs.Set(pObj) != NULL)
		{
			pObj->AddRef();
			return true;
		}
		return false;
	}

	bool Release(CAkGameObject* pObj)
	{
		if (m_refdObjs.Unset(pObj))
		{
			pObj->Release();
			return true;
		}
		return false;
	}

	void Clear()
	{
		for (GameObjPtrSet::Iterator it = m_refdObjs.Begin(); it != m_refdObjs.End(); ++it)
			(*it)->Release();

		m_refdObjs.RemoveAll();
	}

	bool IsEmpty() const { return m_refdObjs.IsEmpty(); }

private:

	GameObjPtrSet m_refdObjs;
};

template <AkUInt32 ComptIdx>
class GetSpatialAudioBase : public CAkTrackedGameObjComponent < ComptIdx >
{
public:
	GetSpatialAudioBase()
	{}

	virtual ~GetSpatialAudioBase() 
	{
		m_PathsToPortals.Term();
		m_VisibleEdges.Term();
	}

	void InvalidatePathsToPortals()
	{
		for (AkPathsToPortals::Iterator it = m_PathsToPortals.Begin(); it != m_PathsToPortals.End(); ++it)
		{
			(*it).Invalidate();
		}
	}

	void ClearInvalidPathsToPortals()
	{
		AkPathsToPortals::Iterator it = m_PathsToPortals.Begin();
		
		while(it != m_PathsToPortals.End())
		{
			if (!(*it).IsValid())
			{
				it = m_PathsToPortals.Erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	// Services from CAkSpatialAudioComponent
	CAkSpatialAudioComponent* GetSpatialAudioComponent() const	{ return CAkGameObjComponent::GetOwner()->template GetComponent<CAkSpatialAudioComponent>(); }
	AkRoomID GetActiveRoom() const			{ return GetSpatialAudioComponent()->GetActiveRoom(); }
	AkRoomID GetTransitionRoom() const		{ return GetSpatialAudioComponent()->GetTransitionRoom(); }
	AkUInt32 GetSync() const				{ return GetSpatialAudioComponent()->GetSync(); }


	AkEdgeArray m_VisibleEdges;

	AkPathsToPortals m_PathsToPortals;
};

class CAkSpatialAudioListener : public GetSpatialAudioBase < GameObjComponentIdx_SpatialAudioListener >
{
	typedef GetSpatialAudioBase < GameObjComponentIdx_SpatialAudioListener > tBase;

public:

	CAkSpatialAudioListener() : m_uEmitterCount(0), m_uSoundPropSync(0), m_fVisibilityRadius(0)
	{}

	virtual ~CAkSpatialAudioListener();

	const AkGameObjectRefSet& GetRoomGameObjs() const { return m_Rooms; }
	AkGameObjectRefSet& GetRoomGameObjs() { return m_Rooms; }

	AkForceInline void AddEmitter() { m_uEmitterCount++; }
	AkForceInline void RemoveEmitter() { m_uEmitterCount--; }

	AkUInt32 GetSoundPropSync() const { return m_uSoundPropSync; }

	void FinishSoundPropagation() { m_uSoundPropSync = GetSync(); }

	void UpdateVisibilityRadius(AkReal32 in_fMaxRadius) 
	{ 
		if (in_fMaxRadius > m_fVisibilityRadius)
		{
			m_fVisibilityRadius = in_fMaxRadius;
		}
	}

	void ResetVisibilityRadius()
	{
		m_fVisibilityRadius = 0.f;
	}

	AkReal32 GetVisibilityRadius() const { return m_fVisibilityRadius; }

	StochasticRayCollection& GetStochasticRays() { return m_stochasticRays; }
	const StochasticRayCollection& GetStochasticRays() const { return m_stochasticRays; }

	// Retrieve transform from sound engine's CAkListener component.
	const AkVector& GetPosition() const { return GetTransform().Position(); }
	const AkTransform& GetTransform() const { return CAkGameObjComponent::GetOwner()->GetComponent<CAkListener>()->GetData().position; }

protected:
	AkGameObjectRefSet m_Rooms;

	AkUInt32 m_uEmitterCount;

	AkUInt32 m_uSoundPropSync;

	AkReal32 m_fVisibilityRadius;

	StochasticRayCollection m_stochasticRays;
};


class CAkSpatialAudioEmitter : public GetSpatialAudioBase < GameObjComponentIdx_SpatialAudioEmitter >
{
	typedef GetSpatialAudioBase < GameObjComponentIdx_SpatialAudioEmitter > tBase;

public:
	CAkSpatialAudioEmitter() : m_pListener(NULL)
	{}

	virtual ~CAkSpatialAudioEmitter();

	AKRESULT AttachListener(AkGameObjectID in_listenerID);

	void BuildCustomRays(AkUInt32 in_diffractionFlags);

	void SetCustomImageSource(AkUniqueID in_auxBusID, AkUniqueID in_srcId, AkRoomID in_roomID, AkImageSourceSettings& in_iss);
	void RemoveCustomImageSource(AkUniqueID in_auxBusID, AkUniqueID in_srcId);
	void ClearCustomImageSources(AkUniqueID in_auxBusID);

	void PackageImageSources();

	CAkSpatialAudioListener* GetListener() const { return m_pListener; }

	void ClearListener() 
	{ 
		m_pListener = NULL; 
	}

	CAkReflectionPaths& GetReflectionPaths()
	{
		return m_ReflectionPaths;
	}

	CAkDiffractionPaths& GetDiffractionPaths()
	{
		return m_DiffractionPaths;
	}

	void ClearPathsToListener()
	{
		m_ReflectionPaths.RemoveAll();
		m_DiffractionPaths.Reset();
	}

	StochasticRayCollection& GetStochasticPaths()
	{
		return m_StochasticPaths;
	}

	bool HasReflections() const;
	bool HasDiffraction() const;
	bool HasReflectionsOrDiffraction() const { return HasReflections() || HasDiffraction(); }
	AkReal32 GetMaxDistance() const		{ return CAkGameObjComponent::GetOwner()->GetComponent<CAkSpatialAudioComponent>()->GetMaxDistance(); }

	// Retrieve transform from sound engine's CAkEmitter component.
	const AkVector& GetPosition() const { return GetTransform().Position(); }
	const AkTransform& GetTransform() const { return CAkGameObjComponent::GetOwner()->GetComponent<CAkEmitter>()->GetPosition().GetPositions()[0].position; }

protected:

	void BuildCustomRaysPerDiffractionPath(AkUInt32 in_diffractionFlags);
	void BuildCustomRaysPerSoundPosition(AkUInt32 in_diffractionFlags);
	void BuildTransmissionRays();

	CAkReflectionPaths m_ReflectionPaths;
	CAkDiffractionPaths m_DiffractionPaths;

	CAkSpatialAudioListener* m_pListener;

	typedef AkSortedKeyArray<AkUniqueID /*AuxBusID,*/, CAkReflectInstance, ArrayPoolSpatialAudio, CAkReflectInstance, AkGrowByPolicy_DEFAULT, CAkReflectInstance> ReflectDataPerAuxBus;
	
	CAkCustomReflectInstances m_customReflectInstances;

	StochasticRayCollection m_StochasticPaths;
	
	CAkGeometricReflectInstance m_gometricReflectInstance;
};

#endif