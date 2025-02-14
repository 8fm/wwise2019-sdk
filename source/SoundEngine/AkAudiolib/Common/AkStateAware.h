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
// AkParameterNodeBase.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _STATE_AWARE_H_
#define _STATE_AWARE_H_

#include "AkActionParams.h"
#include "AkStateMgr.h"
#include "AkRTPCMgr.h"
#include <AK/Tools/Common/AkListBareLight.h>
#include "AkDeltaMonitor.h"

struct AkStateGroupUpdate
{
	AkUniqueID ulGroupID;
	AkUInt32 ulStateCount;
	AkSyncType eSyncType;
};

struct AkStateUpdate
{
	AkUniqueID ulStateInstanceID;//StateInstanceID to be added
	AkStateID ulStateID;//StateID
};

struct AkStatePropertyUpdate
{
	AkUInt32 propertyId;
	AkRtpcAccum accumType;
	bool inDb;
};


typedef AkListBareLight< AkStateGroupChunk, AkListBareLightNextInNode > StateGroupChunkList;

struct AkStatePropertyInfo
{
	AkStatePropertyId propertyId;
	AkUInt8 accumType;
	AkUInt8 inDb:1;

	AkStatePropertyInfo( AkStatePropertyId in_propertyId = (AkStatePropertyId)~0, AkRtpcAccum in_rtpcAccum = AkRtpcAccum_Additive, bool in_inDb = false )
		: propertyId( in_propertyId )
		, accumType( in_rtpcAccum )
		, inDb( in_inDb ? 1 : 0 )
	{}

	bool operator==(const AkStatePropertyInfo& other) const { return propertyId == other.propertyId; }

	AkUInt32 ToUInt() {
		AkUInt32 value = ( (AkUInt32)propertyId << 0 )
			| ( (AkUInt32)accumType << 16 )
			| ( (AkUInt32)inDb << 24 );
		return value;
	}

	void FromUInt( AkUInt32 in_value ) {
		propertyId = (AkStatePropertyId)( (in_value >> 0) & 0xffff );
		accumType = (AkUInt8)( (in_value >> 16) & 0xff );
		inDb = (AkUInt8)( (in_value >> 24) & 0x1 );
	}
};

typedef AkArray< AkStatePropertyInfo, AkStatePropertyInfo& > StatePropertyArray;


class CAkStateAware
{
protected:
	CAkStateAware();
	virtual ~CAkStateAware();

	void FlushStateTransitions();

public:

	// Functions required by class derived from this
	virtual bool StateTransitionAdded() = 0;
	virtual void StateTransitionRemoved() = 0;

	virtual bool HasState( AkRtpcID in_uProperty );
	virtual bool IsStateProperty( AkRtpcID in_uProperty );
	virtual void SetStateProperties( AkUInt32 in_uProperties, AkStatePropertyUpdate* in_pProperties, bool in_bNotify );
	virtual StatePropertyArray* GetStateProperties() = 0;

	virtual void UpdateStateParamTargets() {}
	virtual void PushStateParamUpdate( AkStatePropertyId in_propertyId, AkRtpcAccum in_rtpcAccum, AkStateGroupID in_stateGroup, AkReal32 in_oldValue, AkReal32 in_newValue, bool in_transitionDone );
	virtual void NotifyStateParamTargets() {}

	AkStateGroupChunk* GetStateGroupChunk( AkStateGroupID in_ulStateGroupID );

	//Removes the associated channel of this node
	//After thet the channel Do not respond to any state channel untill a new channel is set
	virtual void RemoveStateGroup(AkStateGroupID in_ulStateGroupID, bool in_bNotify = true);
	virtual AkStateGroupChunk* AddStateGroup(AkStateGroupID in_ulStateGroupID, bool in_bNotify = true);
	virtual AKRESULT UpdateStateGroups(AkUInt32 in_uGroups, AkStateGroupUpdate* in_pGroups, AkStateUpdate* in_pUpdates);

	void RemoveStateGroups(bool in_bNotify = true);

	// Add/remove a state to the sound object
	AKRESULT AddState( AkStateGroupID in_ulStateGroupID, AkUniqueID in_ulStateInstanceID, AkStateID in_ulStateID );
	void RemoveState( AkStateGroupID in_ulStateGroupID, AkStateID in_ulStateID );

	typedef AkArray<AkSyncType, const AkSyncType, ArrayPoolDefault> StateSyncArray;

	class CAkStateSyncArray
	{
	public:
		StateSyncArray&		GetStateSyncArray() { return m_StateSyncArray; }
		void				Term() { m_StateSyncArray.Term(); }
		void				RemoveAllSync() { m_StateSyncArray.RemoveAll(); }
	private:
		StateSyncArray		m_StateSyncArray;
	};

	void SetStateSyncType( AkStateGroupID in_stateGroupID, AkUInt32/*AkSyncType*/ in_eSyncType );
	// return true means we are done checking
	bool CheckSyncTypes( AkStateGroupID in_stateGroupID, CAkStateSyncArray* io_pSyncTypes );

	// Functions to block state usage
	bool UseState() const {	return m_bUseState != 0; }
	void UseState( bool in_bUseState );

	// This function is called on the ParameterNode from the state it is using to signify that the
	// currently in use state settings were modified and that existing PBIs have to be informed
	void NotifyStateParametersModified();

	inline void CheckPauseTransition( ActionParams& in_rAction)
	{
		if( in_rAction.bApplyToStateTransitions && in_rAction.bIsMasterCall )
		{
			switch( in_rAction.eType )
			{
			case ActionParamType_Pause:
				PauseTransitions( true );
				break;
			case ActionParamType_Resume:
			case ActionParamType_Stop: // Here, I believe Stop should npt be there, but keeping it to maintain backward behavior safety.
				PauseTransitions( false );
				break;
			default:
				break;
			}
		}
	}

private:
	void PauseTransitions( bool in_bPause );

protected:
	virtual AKRESULT ReadStateChunk( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize );

public:

	template < typename Accumulator, typename Monitor >
	AkReal32 GetStateParamValue( AkRTPC_ParameterID in_rtpcId )
	{
		AkReal32 fValue = Accumulator::BaseValue();

		if ( UseState() && IsStateProperty( in_rtpcId ) )
		{
			StateGroupChunkList* pStateChunks = GetStateChunks();
			if (pStateChunks)
			{
				for (StateGroupChunkList::Iterator iter = pStateChunks->Begin(); iter != pStateChunks->End(); ++iter)
				{
					AkStateGroupChunk* pStateGroupChunk = *iter;

					AkStateValue* pValue = pStateGroupChunk->m_values.FindProp(in_rtpcId);
					if (pValue)
					{
						Monitor::LogState(pStateGroupChunk->m_ulStateGroup, in_rtpcId, pStateGroupChunk->m_ulActualState, pValue->fValue);	//Should we log RTPC id and value?
						Accumulator::Accumulate(fValue, pValue->fValue);
					}
				}
			}
		}

		return fValue;
	}

	template < typename Accumulator >
	AkReal32 GetStateParamValue(AkRTPC_ParameterID in_rtpcId)
	{
		return GetStateParamValue<Accumulator, AkDeltaMonitorDisabled>(in_rtpcId);
	}

	AkReal32 GetStateParamValue(AkRtpcAccum in_accumType, AkRTPC_ParameterID in_propertyId)
	{
		return (in_accumType == AkRtpcAccum_Multiply)
			? GetStateParamValue<AccumulateMult>(in_propertyId)
			: GetStateParamValue<AccumulateAdd>(in_propertyId);
	}

	template < typename T, typename Accumulator >
	bool GetStateParamValues( T& in_caller, AkStatePropertyId in_rtpcId, AkReal32& io_value )
	{
		StateGroupChunkList* pStateChunks = GetStateChunks();
		if (pStateChunks)
		{
			bool bHasStateGroups = false;
			for (StateGroupChunkList::Iterator iter = pStateChunks->Begin(); iter != pStateChunks->End(); ++iter)
			{
				AkStateGroupChunk* pStateGroupChunk = *iter;
				AkStateValue* pValue = pStateGroupChunk->m_values.FindProp(in_rtpcId);

				AkReal32 fValue = pValue && UseState() ? pValue->fValue : Accumulator::BaseValue();

				in_caller.StateAwareParamValue( in_rtpcId, pStateGroupChunk->m_ulStateGroup, fValue );

				Accumulator::Accumulate( io_value, fValue );

				bHasStateGroups = true;
			}
			return bHasStateGroups;
		}
		return false;
	}

protected:
	virtual bool EnsureStateData() = 0;
	virtual StateGroupChunkList* GetStateChunks() = 0;

private:
	AkUInt8 m_bUseState;	// Enable and disable the use of the state
};

#endif // _STATE_AWARE_H_
