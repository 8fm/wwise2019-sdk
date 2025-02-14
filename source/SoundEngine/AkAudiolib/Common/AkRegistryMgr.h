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
// AkRegistryMgr.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _REGISTRY_MGR_H_
#define _REGISTRY_MGR_H_

#include <AK/Tools/Common/AkHashList.h>
#include <AK/Tools/Common/AkLock.h>
#include "AkRegisteredObj.h"
#include <AK/SoundEngine/Common/AkQueryParameters.h>
#include "PrivateStructures.h"

class CAkParameterNodeBase;
class CAkPBI;
class AkSoundPositionRef;

inline bool CheckObjAndPlayingID(
								 const CAkRegisteredObj* in_pObjSearchedFor, 
								 const CAkRegisteredObj* in_pObjActual,
								 AkPlayingID in_PlayingIDSearched, 
								 AkPlayingID in_PlayingIDActual)
{
	if( in_pObjSearchedFor == NULL || in_pObjSearchedFor == in_pObjActual )
	{
		if( in_PlayingIDSearched == AK_INVALID_PLAYING_ID || in_PlayingIDSearched == in_PlayingIDActual )
		{
			return true;
		}
	}
	return false;
}

//CAkRegistryMgr Class
//Unique container containing the registered objects info
class CAkRegistryMgr
{
public:
	CAkRegistryMgr();
	~CAkRegistryMgr();

	AKRESULT	Init();
	void		Term();

	// Register a game object
	AKRESULT RegisterObject(
		AkGameObjectID in_GameObjectID,	//GameObject (Ptr) to register
		void * in_pMonitorData = NULL
		);

	// Unregister the specified game object
	void UnregisterObject(
		AkGameObjectID in_GameObjectID	//Game Object to unregister
		);

#ifndef AK_OPTIMIZED
	// Mute a specified game object.
	AKRESULT MonitoringMuteGameObj(
		AkGameObjectID in_GameObjectID,
		bool in_bState, bool in_bNotify = true);

	// Solo a specified game object.
	AKRESULT MonitoringSoloGameObj(
		AkGameObjectID in_GameObjectID,
		bool in_bState, bool in_bNotify = true);

	inline static bool IsMonitoringSoloActive() { return (g_uSoloGameObjCount > 0); }
	inline static bool IsMonitoringMuteSoloActive() { return (g_uSoloGameObjCount > 0 || g_uMuteGameObjCount > 0); }

	static void ResetMonitoringMuteSolo();

	void ClearMonitoringSoloMuteGameObj();

	// Refresh the implicit solo state for all game objects - eg. when a connection changes.
	void RefreshImplicitSolos(bool in_bNotify);

	// Set position relative to a listener. 
	// Currently, position of game object 0 (Transport) relative to listener 0 (default) is assumed,
	// hence the absence of corresponding input arguments. 
	void SetRelativePosition(
		const AkSoundPosition & in_rPosition
		);
#endif

	// Set the current position of a game object
	AKRESULT SetPosition(
		AkGameObjectID in_GameObjectID,
		const AkChannelEmitter * in_aPositions,
		AkUInt32 in_uNumPositions,
		AK::SoundEngine::MultiPositionType in_eMultiPositionType
		);

	void UpdateListeners(
		AkGameObjectID in_GameObjectID,
		const AkListenerSet& in_uListeners,
		AkListenerOp in_operation
		);

	void UpdateDefaultListeners(
		const AkListenerSet& in_uListeners,
		AkListenerOp in_operation
	);

	void ResetListenersToDefault(
		AkGameObjectID in_GameObjectID	///< Game object.
		);

	AKRESULT GetPosition(
		AkGameObjectID in_GameObjectID,
		const AkSoundPositionRef*& out_Position
		);

	void SetGameObjectAuxSendValues(
		AkGameObjectID		in_GameObjectID,
		AkAuxSendValue*	in_aEnvironmentValues,
		AkUInt32			in_uNumEnvValues
		);

	void SetGameObjectAuxSendValues(
		CAkGameObject*		in_pEmitter,
		AkAuxSendValue*		in_aEnvironmentValues,
		AkUInt32			in_uNumEnvValues
	);

	void SetGameObjectOutputBusVolume(
		AkGameObjectID		in_emitterID,
		AkGameObjectID		in_listenerID,
		AkReal32			in_fControlValue
		);

	AKRESULT SetGameObjectScalingFactor(
		AkGameObjectID		in_GameObj,
		AkReal32			in_fControlValue
		);


	AKRESULT SetObjectObstructionAndOcclusion(
		AkGameObjectID in_EmitterID,		///< Game object ID.
		AkGameObjectID in_ListenerID,       ///< Listener ID.
		AkReal32 in_fObstructionLevel,		///< ObstructionLevel : [0.0f..1.0f]
		AkReal32 in_fOcclusionLevel			///< OcclusionLevel   : [0.0f..1.0f]
		);
	
	AKRESULT SetObjectObstructionAndOcclusion(
		CAkGameObject* in_pEmitterObj,	///< Game object pointer.
		AkGameObjectID in_ListenerID,		///< Listener ID.
		AkReal32 in_fObstructionLevel,		///< ObstructionLevel : [0.0f..1.0f]
		AkReal32 in_fOcclusionLevel			///< OcclusionLevel   : [0.0f..1.0f]
	);

	AKRESULT SetMultipleObstructionAndOcclusion(
		AkGameObjectID in_EmitterID,
		AkGameObjectID in_ListenerID,
		AkObstructionOcclusionValues* in_ObstructionOcclusionValue,
		AkUInt32 in_uNumObstructionOcclusion
		);

	AKRESULT SetMultipleObstructionAndOcclusion(
		CAkGameObject* in_pEmitterObj,
		AkGameObjectID in_ListenerID,
		AkObstructionOcclusionValues* in_ObstructionOcclusionValue,
		AkUInt32 in_uNumObstructionOcclusion
	);

	AkSwitchHistItem GetSwitchHistItem( 
		CAkRegisteredObj *		in_pGameObj,
		AkUniqueID          in_SwitchContID
		);

	AKRESULT SetSwitchHistItem( 
		CAkRegisteredObj *		in_pGameObj,
		AkUniqueID          in_SwitchContID,
		const AkSwitchHistItem &  in_SwitchHistItem
		);

	AKRESULT ClearSwitchHist( 
		AkUniqueID          in_SwitchContID,
		CAkRegisteredObj *		in_pGameObj = NULL
		);

	AKSOUNDENGINE_API CAkRegisteredObj * GetObjAndAddref(AkGameObjectID in_GameObjectID);

	CAkEmitter* ActivatePBI(
		CAkPBI * in_pPBI,			// PBI to set as active
		AkGameObjectID in_GameObjectID	//Game object associated to the PBI
		);

	// Signify to the registry that the specified AudioNode is containing specific information
	void SetNodeIDAsModified(
		CAkParameterNodeBase* in_pNode		//Audionode Modified
		);

	void NotifyListenerPosChanged(
			const AkListenerSet& in_uListeners	// Bitmask of listeners whose position changed.
		);

	// Unregister all the objects registered in the registry.
	// It also removes all the specific memory allocated for specific parameters.
	void UnregisterAll(const AkListenerSet* in_pExcept = NULL);

	void PostEnvironmentStats();
	void PostObsOccStats();
	void PostListenerStats();

	//inline, for profiling purpose only, no lock required since this information is not time critical
	AkUInt32 NumRegisteredObject(){ return m_mapRegisteredObj.Length(); }

	AKRESULT GetActiveGameObjects( 
		AK::SoundEngine::Query::AkGameObjectsList& io_GameObjectList	// returned list of active game objects.
		);

	bool IsGameObjectActive( 
		AkGameObjectID in_GameObjectId
		);

	bool IsGameObjectRegistered(
		AkGameObjectID in_GameObjectId
	);

	typedef AkHashList< AkGameObjectID, CAkRegisteredObj*> AkMapRegisteredObj;
	const AkListNode*		GetModifiedElementList(){ return &m_listModifiedNodes; }
	AkMapRegisteredObj&		GetRegisteredObjectList(){ return m_mapRegisteredObj; }
	
	void UpdateGameObjectPositions();

	template <typename T>
	T* GetGameObjComponent(AkGameObjectID in_GameObjectID)
	{
		CAkRegisteredObj** ppObj = m_mapRegisteredObj.Exists(in_GameObjectID);
		if (ppObj)
			return (*ppObj)->GetComponent<T>();
		else
			return NULL;
	}


	template <typename T>
	T* CreateGameObjComponent(AkGameObjectID in_GameObjectID)
	{
		CAkRegisteredObj** ppObj = m_mapRegisteredObj.Exists(in_GameObjectID);
		if (ppObj)
			return (*ppObj)->CreateComponent<T>();
		else
			return NULL;
	}

private:
	// Check that listener(s) exists and create the listener component if it does.
	void EnsureListenerExists(AkGameObjectID in_listenerID);
	void EnsureListenersExist(const AkListenerSet& in_listenerSet);

	AkListNode m_listModifiedNodes;
	
	AkMapRegisteredObj m_mapRegisteredObj; //map of all actually registered objects

#ifndef AK_OPTIMIZED
	void ClearSoloMuteGameObj(AkGameObjectID in_GameObjectID);
public:
	void RecapParamDelta();

	AkSoundPosition m_relativePositionTransport;
	// Monitoring mute/solo gameObj
	static AkUInt32	g_uSoloGameObjCount;		// Total number of nodes set to SOLO.
	static AkUInt32	g_uMuteGameObjCount;		// Total number of nodes set to MUTE.
#endif
};

extern AKSOUNDENGINE_API CAkRegistryMgr* g_pRegistryMgr;

#endif
