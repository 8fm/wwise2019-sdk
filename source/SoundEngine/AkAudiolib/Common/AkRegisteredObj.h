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
// AkRegisteredObj.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _REGISTERED_OBJ_H_
#define _REGISTERED_OBJ_H_

#include <AK/Tools/Common/AkKeyArray.h>
#include "AkList2.h"
#include "AkPositionKeeper.h"
#include "AkConnectedListeners.h"
#include "AkGameObject.h"
#include "Ak3DListener.h"

class CAkParameterNodeBase;

class CAkRegisteredObj : public CAkGameObject
{
public:
	CAkRegisteredObj(AkGameObjectID in_id) : CAkGameObject(in_id)
#ifndef AK_OPTIMIZED
		, m_uSoloImplicit(0)
		, m_bSoloExplicit(false)
		, m_bMute(false)
#endif
	{}
	virtual AKSOUNDENGINE_API ~CAkRegisteredObj();

	const CAkConnectedListeners& GetConnectedListeners() const;

	const AkListenerSet& GetListeners() const;

	// Get number of emitter-listener pairs for this game object, for listeners that are specified by the given set.
	AkUInt32 GetNumEmitterListenerPairs() const;

#ifndef AK_OPTIMIZED

	inline bool  IsSolo() { return IsSoloExplicit() || IsSoloImplicit(); }
	inline bool  IsMute() { return m_bMute; }

	inline bool  IsSoloExplicit() { return m_bSoloExplicit; }

	// IsSoloImplicit() - At least one connected game object has is soloed.
	inline bool  IsSoloImplicit() { return m_uSoloImplicit > 0; }

	void MuteGameObject(bool in_bMute, bool in_bNotify);
	void SoloGameObject(bool in_bSolo, bool in_bNotify);

	// Clear state for a global refresh
	void ClearImplicitSolo(bool in_bNotify);

	// Push this object's (recently changed) solo state onto the implicit solo of game objects in the chain.
	void PropagateImplicitSolo(bool in_bNotify);

	// Update this object's implicit solo state based on explicit solos in the chain.
	void UpdateImplicitSolo(bool in_bNotify);

private:
	AkUInt32			m_uSoloImplicit;
	AkUInt8				m_bSoloExplicit : 1;
	AkUInt8				m_bMute : 1;

	friend struct AkHelper_CountExplicitSolo;
	friend struct AkHelper_SetImplicitSolo;

#endif
};

struct AkSwitchHistItem
{
	AkForceInline void IncrementPlayback( AkUInt32 in_Switch )
	{
		if( LastSwitch == in_Switch )
		{
			// If the switch is the same than last time, simply increment it
			++NumPlayBack;
		}
		else
		{
			LastSwitch = in_Switch;
			NumPlayBack = 1;
		}
	}

	AkUInt32 LastSwitch;
	AkUInt32 NumPlayBack;
};

typedef CAkKeyArray<AkUniqueID, AkSwitchHistItem> AkSwitchHist; // Switch Container to SwitchHist
class CAkSwitchHistory : public CAkTrackedGameObjComponent < GameObjComponentIdx_SwitchHistory >
{
public:
	CAkSwitchHistory() : CAkTrackedGameObjComponent < GameObjComponentIdx_SwitchHistory >() {}
	virtual ~CAkSwitchHistory(){ History.Term(); }

	AkSwitchHist History;
};

typedef CAkList2<WwiseObjectIDext, const WwiseObjectIDext&, AkAllocAndFree> AkListNode;
class CAkModifiedNodes : public CAkTrackedGameObjComponent < GameObjComponentIdx_ModifiedNodes >
{
public:
	CAkModifiedNodes();
	virtual ~CAkModifiedNodes();

	// Set the specified Node as modified (this node has an SIS that will require to be deleted)
	AKRESULT SetNodeAsModified( CAkParameterNodeBase* in_pNode	);
	const AkListNode* GetModifiedElementList(){ return &m_ListModifiedNodes; }

private:
	AkListNode m_ListModifiedNodes;
};


//Class representing a registered Object
class CAkEmitter: public CAkTrackedGameObjComponent<GameObjComponentIdx_Emitter>
{
public:
	CAkEmitter();
	virtual AKSOUNDENGINE_API ~CAkEmitter();
	
	void SetPosition( 
		const AkChannelEmitter* in_aPositions,
		AkUInt32 in_uNumPositions,
		AK::SoundEngine::MultiPositionType in_eMultiPositionType
		);

	AkPositionStore & GetPosition()
	{
		return m_PositionRegistry;
	}

	void SetScalingFactor( AkReal32 in_fScalingFactor )
	{
		m_fScalingFactor = in_fScalingFactor;
		m_bPositionDirty = true;
	}

	inline AkReal32 GetScalingFactor()
	{
		return m_fScalingFactor;
	}

	void SetSpreadAndFocusValues(AkGameObjectID in_uListenerID, AkReal32* in_spread, AkReal32* in_focus, AkUInt32 in_uNumValues);

	// Get cached emitter listener pairs. Use them only if
	// this game object's position is not dirty
	inline const AkVolumeDataArray & GetCachedEmitListenPairs() const { return m_arCachedEmitListPairs; }
	
	void UpdateCachedPositions();

	AKRESULT SetObjectObstructionAndOcclusion( 
		AkGameObjectID		in_uListenerID,
		AkReal32			in_fObstructionValue,
		AkReal32			in_fOcclusionValue
		);

	AKRESULT SetMultipleObstructionAndOcclusion(
		AkGameObjectID in_uListenerID,
		AkObstructionOcclusionValues* in_ObstructionOcclusionValue,
		AkUInt32 in_uNumObstructionOcclusion
		);

	AkForceInline bool IsPositionCached() { return !m_bPositionDirty; }
	AkForceInline void NotifyPositionDirty(){ m_bPositionDirty = true; }
	AkForceInline void NotifyPositionUpdated() { m_bPositionDirty = false; }

	// Build volume rays for external use - ie. Spatial Audio. 
	// The rays returned in out_arVolumeData will have all combinations of each sound position and each listeners (excluding listener in in_excludedListeners)
	// Note that the build in game parameters are not updated by this function.
	void BuildVolumeRays(AkVolumeDataArray& out_arVolumeData, AkListenerSet& in_excludedListeners);

	void UpdateBuiltInParamValues(const AkRayVolumeData& in_volumeRay, const CAkListener& in_listener);

private:
	void BuildCachedVolumeRays();
	void BuildVolumeRaysHelper(	
		AkVolumeDataArray& out_arVolumeData, 
		const AkListenerSet& in_listeners, 
		const CAkConnectedListeners& connectedListeners, 
		const AkRayVolumeData*& io_pShortestRay, 
		const CAkListener*& io_pClosestListener
	);
	
	AkPositionStore		m_PositionRegistry;

	AkVolumeDataArray	m_arCachedEmitListPairs;

	AkReal32				m_fScalingFactor;

	AkUInt32				m_bPositionDirty	:1;
};
#endif
