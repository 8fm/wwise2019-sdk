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
// AkConnectedListeners.h
//
//////////////////////////////////////////////////////////////////////


#ifndef _AK_CONNECTED_LISTENERS_H_
#define _AK_CONNECTED_LISTENERS_H_

#include "AkCommon.h"
#include "AkGameObject.h"
#include <AK/Tools/Common/AkSet.h>
#include <AK/Tools/Common/AkKeyArray.h>

struct AkAuxSendValueEx;

typedef AkSet<AkGameObjectID, AkHybridAllocator<sizeof(AkGameObjectID), AK_64B_OS_STRUCT_ALIGN, AkMemID_GameObject> > AkListenerSet;

class AkAutoTermListenerSet : public AkListenerSet
{
public:
	AkAutoTermListenerSet() {}
	~AkAutoTermListenerSet(){Term();}
};


//CAkLstnrAssoc
// Represents an association between an emitter (implicit) and a <listener,busId> tuple. 
// 
class CAkLstnrAssoc
{
public:
	struct Key
	{
		// Note: in this case, AK_INVALID_UNIQUE_ID is used to indicate all user-defined routing, including the direct (dry) output, and the user-defined sends.

		Key() : listenerID(AK_INVALID_GAME_OBJECT), busID(AK_INVALID_UNIQUE_ID) {}
		Key(AkGameObjectID in_gameObj) : listenerID(in_gameObj), busID(AK_INVALID_UNIQUE_ID) {}
		Key(AkGameObjectID in_gameObj, AkUniqueID in_busID) : listenerID(in_gameObj), busID(in_busID) {}

		AkGameObjectID	listenerID;
		AkUniqueID		busID;

		bool operator<(const Key& in_rhs) const
		{
			return this->listenerID < in_rhs.listenerID || (this->listenerID == in_rhs.listenerID && this->busID < in_rhs.busID);
		}
		bool operator>(const Key& in_rhs) const
		{
			return this->listenerID > in_rhs.listenerID || (this->listenerID == in_rhs.listenerID && this->busID > in_rhs.busID);
		}

		bool operator==(const Key& in_rhs) const
		{
			return this->listenerID == in_rhs.listenerID && this->busID == in_rhs.busID;
		}
	};

	CAkLstnrAssoc() : m_fGain(1.f) {}
	
	void SetGain(AkReal32 in_fGain){ m_fGain = in_fGain; }
	AkReal32 GetGain() const { return m_fGain; }

	// Key policy for AkSortedKeyArray.
	static AkForceInline Key & Get(CAkLstnrAssoc& in_item)	{ return in_item.key; }

	Key				key;
protected:
	
	AkReal32		m_fGain;
};

// A sorted array of unique CAkLstnrAssoc's.  
class CAkLstnrAssocs
{
public:
	typedef AkSortedKeyArray<CAkLstnrAssoc::Key, CAkLstnrAssoc, AkHybridAllocator<sizeof(CAkLstnrAssoc), AK_64B_OS_STRUCT_ALIGN, AkMemID_GameObject>, CAkLstnrAssoc > AssocArray;
	typedef AssocArray::Iterator Iter;

	CAkLstnrAssocs() {}

	AKRESULT CopyFrom(const CAkLstnrAssocs& in_other);

	~CAkLstnrAssocs() { Term(); }

	void Term();

	const CAkLstnrAssoc* Get(CAkLstnrAssoc::Key in_key) const { return m_assoc.Exists(in_key); };
	CAkLstnrAssoc* Get(CAkLstnrAssoc::Key in_key)				{ return m_assoc.Exists(in_key); };
	CAkLstnrAssoc* Add(CAkLstnrAssoc::Key in_key);
	void Remove(CAkLstnrAssoc::Key in_key);

	//Set the gain, adding the association if necessary.
	bool SetGain(CAkLstnrAssoc::Key in_key, AkReal32 in_fGain);
	void SetAllGains(AkReal32 in_fGain);

	bool SetGains(const AkAuxSendValue* in_aEnvironmentValues, AkUInt32 in_uNumEnvValues);

	void GetGains(AkAuxSendArray & io_arAuxSends, AkReal32 in_fScaleGains, AkReal32 in_fScaleLPF, AkReal32 in_fScaleHPF, const AkListenerSet& in_fallbackListeners) const;

	AkReal32 GetGain(CAkLstnrAssoc::Key in_key) const;

	void Clear();

	//Iterate through CAkListenerAssoc's.
	const Iter Begin() const { return m_assoc.Begin(); };
	const Iter End() const { return m_assoc.End(); }
	AkUInt32 Length() const { return m_assoc.Length(); }

	const AkListenerSet& GetListeners() const { return m_listenerSet; }

	// For backwards compatability with AK::SoundEngine::SetActiveListener() API
	bool SetListeners(const AkListenerSet& in_newListenerSet);
	bool AddListeners(const AkListenerSet& in_ListenersToAdd);
	bool RemoveListeners(const AkListenerSet& in_ListenersToRemove);
	
protected:
	void PreAssocRemoved(CAkLstnrAssoc* pAssoc);

	AssocArray m_assoc;

	AkListenerSet m_listenerSet;

#ifndef AK_OPTIMIZED
public:
	void CheckSet(const AkListenerSet& in_set);
	bool CheckId(AkGameObjectID in_id) const;
#endif
};

//
// CAkConnectedListeners - a game object component.
//	Maintains two sets of emitter-listener associations.  One for user-defined bus outputs (the main bus output and user-defined aux) and 
//	a second for game-defined aux sends.  For the user-defined associations, the bus id in the CAkLstnrAssoc is always AK_INVALID_UNIQUE_ID, because 
//	it applies to any and all busses defined by the user.
//
class CAkConnectedListeners : public CAkTrackedGameObjComponent < GameObjComponentIdx_ConnectedListeners >
{
public:
	CAkConnectedListeners() : m_bOverrideUserDefault(0), m_bOverrideAuxDefault(0) {}
	virtual ~CAkConnectedListeners() { Term(); }

	void Term();

	void SetUserGain(AkGameObjectID in_uListenerID, AkReal32 in_fGain);
	void SetAllUserGains(AkReal32 in_fGain);
	void SetAuxGains(const AkAuxSendValue* in_aEnvironmentValues, AkUInt32& out_uNumEnvValues);
	void SetAuxGain(AkGameObjectID in_uListenerID, AkUniqueID in_AuxBusID, AkReal32 in_fGain);
	void ClearAuxGains();
	void SetListeners(const AkListenerSet& in_newListenerSet);
	void AddListeners(const AkListenerSet& in_ListenersToAdd);
	void RemoveListeners(const AkListenerSet& in_ListenersToRemove);
	void ResetListenersToDefault();
	
	AkReal32 GetUserGain(AkGameObjectID in_uListenerID) const
	{
		return GetUserAssocs().GetGain(in_uListenerID);
	}

	void GetAuxGains(AkAuxSendArray & io_arAuxSends, AkReal32 in_fScaleGains, AkReal32 in_fScaleLPF, AkReal32 in_fScaleHPF) const
	{
		GetAuxAssocs().GetGains(io_arAuxSends, in_fScaleGains, in_fScaleLPF, in_fScaleHPF, GetUserAssocs().GetListeners());
	}

	const AkListenerSet& GetListeners() const { return m_listenerSet; }

	//Get the user-defined outputs' associations, falling back to default if appropriate.
	const CAkLstnrAssocs& GetUserAssocs() const 
	{
		return m_bOverrideUserDefault ? m_user : GetDefault()->GetUserAssocs();
	}

	//Get the game-defined aux sends' associations, falling back to default if appropriate.
	const CAkLstnrAssocs& GetAuxAssocs() const
	{
		return m_bOverrideAuxDefault ? m_aux : GetDefault()->GetAuxAssocs();
	}

	bool OverrideAuxDefault() const { return m_bOverrideAuxDefault; }
	bool OverrideUserDefault() const { return m_bOverrideUserDefault; }

	bool IAmTheDefault() const { return s_pDefaultConnectedListeners == this; }

	// This object is returned when calling GetComponent() on a game object that does not have its
	//	CAkConnectedListeners component created.  It is an override of GetDefault() in CAkGameObjComponent. 
	static CAkConnectedListeners* GetDefault()
	{
		if ( AK_EXPECT_FALSE(s_pDefaultConnectedListeners == NULL) )
			InitDefault();
		return s_pDefaultConnectedListeners;
	}

	static AKRESULT InitDefault()
	{
		AKASSERT(s_pDefaultConnectedListeners == NULL);
		s_pDefaultConnectedListeners = AkNew(AkMemID_GameObject, CAkConnectedListeners());
		if (s_pDefaultConnectedListeners)
		{
			s_pDefaultConnectedListeners->m_bOverrideUserDefault = true;
			s_pDefaultConnectedListeners->m_bOverrideAuxDefault = true;
			return AK_Success;
		}
		return AK_InsufficientMemory;
	}

	static void TermDefault()
	{
		if (s_pDefaultConnectedListeners != NULL)
		{
			AkDelete(AkMemID_GameObject, s_pDefaultConnectedListeners);
			s_pDefaultConnectedListeners = NULL;
		}
	}


protected:
	bool CloneAndOverrideUserDefault();
	bool CloneAndOverrideAuxDefault();

	// Build the union of the two sets of listeners.
	// Must be called after modifying any associations to keep the set up to date.
	void BuildListenerSet();

	//the user-defined outputs' emitter-listener associations (if m_bOverrideUserDefault).
	CAkLstnrAssocs m_user;

	//the game-defined aux sends' emitter-listener associations (if m_bOverrideAuxDefault).
	CAkLstnrAssocs m_aux;

	// The union of the listeners defined in both above sets of associations.
	AkListenerSet m_listenerSet;	
	
	bool m_bOverrideUserDefault;
	bool m_bOverrideAuxDefault;

#ifndef AK_OPTIMIZED
	void Check();
#endif

	static CAkConnectedListeners* s_pDefaultConnectedListeners;
};

class AkEmitterListenerPathTraversal
{
public:
	~AkEmitterListenerPathTraversal()
	{ 
		m_visited.Term();
		m_result.Term();
	}
	
	void Backwards(CAkGameObject* in_pObj)
	{
		m_visited.RemoveAll();
		m_visited.Set(in_pObj);
		_Backwards(in_pObj);
		AkUnion(m_result, m_visited);
	}

	void Forwards(CAkGameObject* in_pObj)
	{
		m_visited.RemoveAll();
		m_visited.Set(in_pObj);
		_Forwards(in_pObj);
		AkUnion(m_result, m_visited);
	}

	template<typename Op>
	void ApplyOp(Op& op)
	{
		for (GameObjectSet::Iterator it = m_result.Begin(); it != m_result.End(); ++it)
			op(*it);
	}

	template<typename Op>
	void ApplyOp2(Op& op)
	{
		bool bKeepGoing = true;
		for (GameObjectSet::Iterator it = m_result.Begin(); it != m_result.End() && bKeepGoing; ++it)
			bKeepGoing = op(*it);
	}


private:

	void _Backwards(CAkGameObject* in_pObj);
	void _Forwards(CAkGameObject* in_pObj);

	typedef AkSet<CAkGameObject*, ArrayPoolDefault> GameObjectSet;
	GameObjectSet m_visited;
	GameObjectSet m_result;
};

#endif