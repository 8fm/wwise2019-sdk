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
// AkGameObject.h
//
//////////////////////////////////////////////////////////////////////

#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkListBare.h>
#include "AkScopedRtpcObj.h"

#ifndef AK_GAME_OBJECT
#define AK_GAME_OBJECT

enum AkGameObjComponentIdx
{
	GameObjComponentIdx_Emitter,
	GameObjComponentIdx_ConnectedListeners,	
	GameObjComponentIdx_Listener,
	GameObjComponentIdx_ModifiedNodes,
	GameObjComponentIdx_SwitchHistory,
	GameObjComponentIdx_SpatialAudioObj,
	GameObjComponentIdx_SpatialAudioEmitter,
	GameObjComponentIdx_SpatialAudioListener,
	GameObjComponentIdx_SpatialAudioRoom,
	//
	GameObjComponent_NumIdxs
};

class CAkGameObject;

class CAkGameObjComponent
{
public:
	CAkGameObjComponent() : m_pOwner(NULL) {}
	virtual ~CAkGameObjComponent(){}

	virtual AKRESULT Init(AkGameObjectID in_uGameObjectID) { return AK_Success; }

	AkGameObjectID ID() const;

	void SetOwner(CAkGameObject * in_pOwner){ m_pOwner = in_pOwner; }
	CAkGameObject * GetOwner() const { return m_pOwner; }

	static CAkGameObjComponent* GetDefault() { return NULL; }
protected:
	CAkGameObject * m_pOwner;
};

struct AkGameObjComponentPtr
{
	AkGameObjComponentPtr() : ptr(NULL) {}

	void Set(CAkGameObjComponent* in_ptr) { ptr = in_ptr; }

	template <typename T>
	T* Get() { return (T*)ptr; }

	template <typename T>
	const T* Get() const { return (const T*)ptr; }

	bool IsNull() const { return ptr == NULL; }

	void Delete();

	CAkGameObjComponent* ptr;
};

typedef AkArray<AkGameObjComponentPtr, AkGameObjComponentPtr, AkHybridAllocator<sizeof(AkGameObjComponentPtr) * 2, AK_64B_OS_STRUCT_ALIGN, AkMemID_GameObject> > AkGameObjComponentArray;

template <AkUInt32 _kTypeIdx>
class CAkIndexedGameObjComponent: public CAkGameObjComponent
{
public:
	static const AkUInt32 kTypeIdx = _kTypeIdx;

	virtual ~CAkIndexedGameObjComponent(){}
};

template <AkUInt32 kTypeIdx>
class CAkTrackedGameObjComponent : public CAkIndexedGameObjComponent<kTypeIdx>
{
public:
	typedef	AkListBare< CAkTrackedGameObjComponent<kTypeIdx>, AkListBareNextItem, AkCountPolicyWithCount, AkLastPolicyWithLast > tList;
	typedef	typename tList::Iterator tIter;

	CAkTrackedGameObjComponent() : pPrevItem(sList.Last())
	{
		sList.AddLast(this);
	}

	virtual ~CAkTrackedGameObjComponent()
	{
		if (pNextItem != NULL)
			pNextItem->pPrevItem = pPrevItem;
		
		sList.RemoveItem(this, pPrevItem);
	}

	CAkTrackedGameObjComponent<kTypeIdx>* pNextItem;
	CAkTrackedGameObjComponent<kTypeIdx>* pPrevItem;

	static tList& List(){ return sList; }
	static tIter Begin(){ return sList.Begin(); }
	static tIter End(){ return sList.End(); }

private:
	
	static tList sList;
};

template <AkUInt32 kTypeIdx>
typename CAkTrackedGameObjComponent<kTypeIdx>::tList CAkTrackedGameObjComponent<kTypeIdx>::sList;

class CAkGameObject : public CAkScopedRtpcObj
{
public:
	CAkGameObject(AkGameObjectID in_id) : CAkScopedRtpcObj(), m_GameObjID(in_id), m_refCount(1), m_bRegistered(true)
	{}

	virtual AKSOUNDENGINE_API ~CAkGameObject();

	AkGameObjectID ID() const { return m_GameObjID; }

	// Get a (const) component if it exists, if not return the default if one is defined in the component class.
	template<typename tComponent>
	const tComponent* GetComponentOrDefault() const
	{
		tComponent* ptr = NULL;
		if (m_components.Length() > tComponent::kTypeIdx)
			ptr = m_components[tComponent::kTypeIdx].template Get<tComponent>();
		
		if (ptr == NULL)
			ptr = static_cast<tComponent*>(tComponent::GetDefault());

		return ptr;
	}

	// Get a (non-const) component if it exists.  ***Else return NULL.
	template<typename tComponent>
	tComponent* GetComponent() const
	{
		tComponent* ptr = NULL;
		if (m_components.Length() > tComponent::kTypeIdx)
			ptr = m_components[tComponent::kTypeIdx].template Get<tComponent>();

		return ptr;
	}

	// Create a component and return it, or get a component if it already exists.
	template<typename tComponent>
	tComponent* CreateComponent()
	{
		tComponent* ptr = NULL;
		if (m_components.Length() > tComponent::kTypeIdx || m_components.Resize(tComponent::kTypeIdx + 1))
		{
			ptr = m_components[tComponent::kTypeIdx].template Get<tComponent>();
			if (ptr == NULL)
			{
				ptr = AkNew(AkMemID_GameObject, tComponent());
				if (ptr)
				{
					m_components[tComponent::kTypeIdx].Set(ptr);
					ptr->SetOwner(this);
					if (ptr->Init(ID()) != AK_Success)
					{
						AkDelete(AkMemID_GameObject, ptr);
						ptr = NULL;
						m_components[tComponent::kTypeIdx].Set(NULL);
					}
				}
			}
		}

		return ptr;
	}

	// Delete a component and return true if it exists, return false otherwise.
	template<typename tComponent>
	bool DeleteComponent()
	{
		if (m_components.Length() > tComponent::kTypeIdx && !m_components[tComponent::kTypeIdx].IsNull())
		{
			m_components[tComponent::kTypeIdx].Delete();
			return true;
		}

		return false;
	}

	// Return true if a component exists.
	template<typename tComponent>
	bool HasComponent() const
	{
		return (m_components.Length() > tComponent::kTypeIdx) && !m_components[tComponent::kTypeIdx].IsNull();
	}

	bool HasComponent(AkGameObjComponentIdx in_idx) const
	{
		return (m_components.Length() > (AkUInt32)in_idx) && !m_components[(AkUInt32)in_idx].IsNull();
	}

	AkForceInline void AddRef()
	{
		++m_refCount;
	}

	AkForceInline void Release()
	{
		AKASSERT(m_refCount > 0);
		if (--m_refCount <= 0)
			AkDelete(AkMemID_GameObject, this);
	}

	AkForceInline bool IsActive()
	{
		// If the object is referenced (from outside the registry mgr), it means that it is active .
		// When m_refCount == 1, the object is valid, but not active ( sleeping )
		return m_refCount > 1 || !m_bRegistered;
	}

	bool IsRegistered() const { return m_bRegistered; }
	void SetRegistered(bool in_bIsRegistered){ m_bRegistered = in_bIsRegistered; }
	
	AkUInt32 GetComponentFlags() const
	{
		AkUInt32 uFlags = 0;
		for (AkUInt32 idx = 0; idx < m_components.Length(); ++idx)
		{
			if (!m_components[idx].IsNull())
				uFlags |= (1U << idx);
		}
		return uFlags;
	}

protected:
	AkGameObjComponentArray m_components;
	AkGameObjectID			m_GameObjID;
	AkUInt32				m_refCount;
	bool					m_bRegistered;
};

AkForceInline AkGameObjectID CAkGameObjComponent::ID() const { return m_pOwner->ID(); }

#endif

