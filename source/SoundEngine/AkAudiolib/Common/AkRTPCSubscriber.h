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

#ifndef AK_RTPC_SUBSCRIBER
#define AK_RTPC_SUBSCRIBER

#include "AkCommon.h"
#include "AkRTPC.h"
#include "AkRTPCKey.h"
#include "AkRTPCMgr.h"

class CAkRegisteredObj;
class CAkParameterNodeBase;

#if defined(AK_ENABLE_ASSERTS)
#define PARAM_TGT_CHECK
#endif

//A target that needs to receive parameter updates from the rtpc system inherits from this class
class CAkParameterTarget
{
public:
	CAkParameterTarget(const AkRTPCKey& in_rtpcKey, bool in_bProagate = true);
	virtual ~CAkParameterTarget();

	// The param setter, to be implemented by inheriting class.  Called to notify of a parameter change.
	//	NOTE: The implementation of this function should be as lightweight as possible. Preferably just checking the parameter
	//		incrementing a float. It will likely be called multiple times in an update if the target subscribes at multiple places
	//		in the hierarchy.
	virtual void UpdateTargetParam(AkRTPC_ParameterID in_eParam, AkReal32 in_fValue, AkReal32 in_fDelta) = 0;

	// An unspecific notification that a parameter has changed.  The target will need to pull updates for relevant parameters.
	virtual void NotifyParamChanged(bool in_bLiveEdit, AkRTPC_ParameterID in_rtpcID){};

	// An unspecific notification that one or more parameters have changed.  The target will need to pull updates for relevant parameters.
	virtual void NotifyParamsChanged(bool in_bLiveEdit, const AkRTPCBitArray& in_bitArray){};

	// Retrieve the set of params that this target is interested in receiving updates for.
	virtual AkRTPCBitArray GetTargetedParamsSet() = 0;
	virtual bool RegisterToBusHierarchy(){ return true; }

	AkForceInline const AkRTPCKey& GetRTPCKey() const						{ return m_rtpcKey; }
	AkForceInline void SetRTPCKey(const AkRTPCKey& in_rtpcKey)				{ m_rtpcKey = in_rtpcKey; }


	void SetRootNode(CAkParameterNodeBase* in_pNode);
	
	AkForceInline CAkParameterNodeBase * GetRootNode() { return m_pRootNode; }

	AkForceInline void RegisterParamTarget( CAkParameterNodeBase * in_pRootNode )
	{ RegisterParamTarget( in_pRootNode, GetTargetedParamsSet(), RegisterToBusHierarchy() ); }

	void RegisterParamTarget(CAkParameterNodeBase * in_pRootNode, const AkRTPCBitArray& in_requestedParams, bool in_bPropagateToBusHier);

	AkForceInline void UnregisterAllParamTarget() { UnregisterParamTarget( ~(AkUInt64)0, true); }
	AkForceInline void UnregisterParamTarget() { UnregisterParamTarget( GetTargetedParamsSet(), RegisterToBusHierarchy() ); }

	void UnregisterParamTarget(const AkRTPCBitArray& in_tgtParams, bool in_bPropagateToBusHier);

	static void UpdateAllParameterTargets();

	static void TermTargets();
	
	void UpdateRegistration();
	
	CAkParameterTarget* pNextLightItem;

protected:

	AkRTPCKey m_rtpcKey;
	CAkParameterNodeBase * m_pRootNode;

#ifdef PARAM_TGT_CHECK
public:
	AkInt32 m_iRegCount;
#endif
};

struct RegisteredTarget
{
	RegisteredTarget() : pTgt(NULL) {}
	RegisteredTarget(AkRTPCBitArray in_params, CAkParameterTarget* in_pTgt);

	AkRTPCBitArray params;
	CAkParameterTarget* pTgt;
};


class AkSortedParamTrtArray : public AkArray < RegisteredTarget, RegisteredTarget >
{
public:
	inline bool InsertTarget(RegisteredTarget& in_pTgtToInsert, Iterator in_pInsertBefore)
	{
		bool bRes = false;
		if (in_pInsertBefore != End())
		{
			unsigned int uIdx = (unsigned int)(in_pInsertBefore.pItem - this->m_pItems);

			AKASSERT(uIdx < Length());
			AKASSERT(uIdx == 0 || this->m_pItems[uIdx - 1].pTgt->GetRTPCKey() < this->m_pItems[uIdx].pTgt->GetRTPCKey());
			AKASSERT(uIdx == (this->Length() - 1) || this->m_pItems[uIdx + 1].pTgt->GetRTPCKey() >= this->m_pItems[uIdx].pTgt->GetRTPCKey());
			AKASSERT(uIdx == 0 || this->m_pItems[uIdx - 1].pTgt->GetRTPCKey() < in_pTgtToInsert.pTgt->GetRTPCKey());
			AKASSERT(this->m_pItems[uIdx].pTgt->GetRTPCKey() >= in_pTgtToInsert.pTgt->GetRTPCKey());
			AKASSERT(uIdx == (this->Length() - 1) || this->m_pItems[uIdx + 1].pTgt->GetRTPCKey() >= in_pTgtToInsert.pTgt->GetRTPCKey());

			RegisteredTarget* pRegTgt = this->Insert(uIdx);
			if (pRegTgt)
			{
				*pRegTgt = in_pTgtToInsert;
				bRes = true;
			}
		}
		else
		{
			if (AddLast(in_pTgtToInsert) != NULL)
				bRes = true;
		}

		return bRes;
	}

	Iterator FindArrayPos(const AkRTPCKey& keyToFind) const
	{
		if (m_pItems != NULL)
		{
			AkInt32 uLeft = 0, uRight = this->Length();
			while (uLeft < uRight)
			{
				AkInt32 uMiddle = (uLeft + uRight) / 2;
				if (m_pItems[uMiddle].pTgt->GetRTPCKey() < keyToFind)
					uLeft = uMiddle + 1;
				else
					uRight = uMiddle;
			}

			AKASSERT(uRight <= (AkInt32)this->Length());
			AKASSERT(uRight == 0 || this->m_pItems[uRight - 1].pTgt->GetRTPCKey() < keyToFind);
			AKASSERT(uRight == (AkInt32) this->Length() || this->m_pItems[uRight].pTgt->GetRTPCKey() >= keyToFind);
			AKASSERT(uRight >= (AkInt32)(this->Length() - 1) || this->m_pItems[uRight + 1].pTgt->GetRTPCKey() >= keyToFind);

			Iterator retVal;
			retVal.pItem = &(this->m_pItems[uRight]);
			return retVal;
		}
		return Begin();
	}

	Iterator FindArrayPos(CAkParameterTarget* in_pTgt) const;

	bool AddTarget(RegisteredTarget& in_regTgt)
	{
		bool bRes = false;

		Iterator pFound = FindArrayPos(in_regTgt.pTgt);

		for (Iterator pItem = pFound, pEnd = End();
			pItem != pEnd &&
			(*pItem).pTgt->GetRTPCKey() == in_regTgt.pTgt->GetRTPCKey();
			++pItem)
		{
			if ((*pItem).pTgt == in_regTgt.pTgt)
			{
				(*pItem).params = in_regTgt.params; //update params
				return false; // already exists.
			}
		}

		bRes = InsertTarget(in_regTgt, pFound);

		return bRes;
	}

	bool RemoveTarget(CAkParameterTarget* in_pTgt);
};

class CAkRTPCSubscriberNodeData
{
public:
	CAkRTPCSubscriberNodeData() : m_bCheckModulator(false)
	{
		UpdateCommonParams();
	}

	~CAkRTPCSubscriberNodeData()
	{
		m_targetsArray.Term();
	}

	//The set of parameters that this node subscribes to in the RTPC mgr.
	AkRTPCBitArray	m_RTPCBitArray;

	//The set of parameters that is common to all registered targets.
	AkRTPCBitArray	m_CommonParams;

	AkSortedParamTrtArray m_targetsArray;

	bool m_bCheckModulator;

	void RemoveTarget(CAkParameterTarget* in_pTgt)
	{
		if (m_targetsArray.RemoveTarget(in_pTgt))
		{
#ifdef PARAM_TGT_CHECK
			AKASSERT(in_pTgt->m_iRegCount >= 0);
			in_pTgt->m_iRegCount--;
#endif
			//It may be beneficial to update this more frequently..
			if (m_targetsArray.IsEmpty())
				UpdateCommonParams();
		}
	}

	bool AddTarget(CAkParameterTarget* in_pTgt, AkRTPCBitArray in_params)
	{
		RegisteredTarget regTgt(in_params, in_pTgt);
		if (m_targetsArray.AddTarget(regTgt))
		{
			m_CommonParams &= in_params;
#ifdef PARAM_TGT_CHECK
			AKASSERT(in_pTgt->m_iRegCount >= 0);
			in_pTgt->m_iRegCount++;
#endif
			return true;
		}
		return false;
	}

	void UpdateCommonParams()
	{
		AkRTPCBitArray all;
		m_CommonParams = ~(all);
		for (AkSortedParamTrtArray::Iterator it = m_targetsArray.Begin(), itEnd = m_targetsArray.End(); it != itEnd; ++it)
			m_CommonParams &= (*it).params;
	}
};

//  A node in the hierarchy that maintains subscriptions to the RTPC mgr inherits from this class.
class CAkRTPCSubscriberNode
{
public:
	CAkRTPCSubscriberNode();
	~CAkRTPCSubscriberNode();

	AKRESULT SetRTPC( 
		AkRtpcID			in_RTPC_ID,
		AkRtpcType			in_RTPCType,
		AkRtpcAccum			in_RTPCAccum,
		AkRTPC_ParameterID	in_ParamID,
		AkUniqueID			in_RTPCCurveID,
		AkCurveScaling		in_eScaling,
		AkRTPCGraphPoint*	in_pArrayConversion,
		AkUInt32			in_ulConversionArraySize
		);

	void UnsetRTPC( 
		AkRTPC_ParameterID in_ParamID,
		AkUniqueID in_RTPCCurveID
		);

	// Enables the parameter to propagate updates, however does not subscribe to the RTPC manager.  The assumption being that
	//	somebody else will call PushParamUpdate.  Used for SIS and Ducking system.
	AKRESULT EnableParam( AkRTPC_ParameterID	in_ParamID );
	void DisableParam( AkRTPC_ParameterID	in_ParamID );

	AkForceInline void RegisterParameterTarget( CAkParameterTarget* in_pTgt, const AkRTPCBitArray& in_params)
	{
		if (m_Data != NULL)
		{
			AkRTPCBitArray params = m_Data->m_RTPCBitArray & in_params;

			if (!params.IsEmpty())
			{
				bool bActivate = !HasActiveTargets();				
				if (m_Data->AddTarget(in_pTgt, params) & bActivate)
					g_pRTPCMgr->ActivateSubscription(GetSubscriberPtr(), m_Data->m_RTPCBitArray);
			}
			else
			{
				m_Data->RemoveTarget(in_pTgt);
			}
		}
	}

	AkForceInline void UnregisterParameterTarget( CAkParameterTarget* in_pTgt )
	{
		if (m_Data)
		{
			m_Data->RemoveTarget(in_pTgt);
			
			if (!HasActiveTargets())
				g_pRTPCMgr->DeactivateSubscription(GetSubscriberPtr(), m_Data->m_RTPCBitArray);
		}
	}

	void UnregisterAllParameterTargets();

	// Called by RTPC mgr to notify target of a parameter change.  
	virtual void PushParamUpdate( AkRTPC_ParameterID in_ParamID, const AkRTPCKey& in_rtpckKey, AkReal32 in_fValue, AkReal32 in_fDeltaValue, AkRTPCExceptionChecker* in_pExceptCheck );
	virtual void NotifyParamChanged( bool in_bLiveEdit, AkRTPC_ParameterID in_rtpcID );
	virtual void NotifyParamsChanged( bool in_bLiveEdit, const AkRTPCBitArray& in_bitArray );

	template <typename Monitor>
	inline AkReal32 GetRTPCConvertedValue(AkUInt32 in_ParamID, const AkRTPCKey& in_rtpcKey)
	{
		return g_pRTPCMgr->GetRTPCConvertedValue<Monitor>((void*)this, in_ParamID, in_rtpcKey);
	}

	inline AkReal32 GetRTPCConvertedValue(AkUInt32 in_ParamID, const AkRTPCKey& in_rtpcKey)
	{
		return GetRTPCConvertedValue<AkDeltaMonitorDisabled>(in_ParamID, in_rtpcKey);
	}

	void GetModulatorParamXfrm( AkModulatorParamXfrmArray& io_xforms, AkRTPC_ParameterID rtpcParamId, AkRtpcID in_modulatorID, const CAkRegisteredObj * in_GameObjPtr ) const;


	inline bool HasActiveTargets() const
	{
		return m_Data != NULL && !m_Data->m_targetsArray.IsEmpty();
	}

	inline bool HasRTPC(AkRTPC_ParameterID in_ePropID) const
	{
		return m_Data != NULL && m_Data->m_RTPCBitArray.IsSet(in_ePropID);
	}

	//Indicates that this node at one point had a modulator.  (But may have been removed in a live edit)
	inline bool HasModulator() const
	{
		return m_Data != NULL && m_Data->m_bCheckModulator;
	}

	// Parameter passed to modulator params and rtpc subscriptions.
	void* GetSubscriberPtr() const { return (void*)this; }

	// PushParamUpdate_* - Three versions of this function, which are dispatched by the virtual function PushParamUpdate().  
	//
	// Scoped - Update all parameter targets that match the key passed in "in_rtpckKey".
	void PushParamUpdate_Scoped( AkRTPC_ParameterID in_ParamID, const AkRTPCKey& in_rtpckKey, AkReal32 in_fValue, AkReal32 in_fDeltaValue );
	//
	//	All - Update the param on all param targets
	void PushParamUpdate_All( AkRTPC_ParameterID in_ParamID, AkReal32 in_fValue, AkReal32 in_fDeltaValue );
	//
	//	Except - Update the param on all param targets, while checking for exceptions with "in_exceptCheck". 
	void PushParamUpdate_Except( AkRTPC_ParameterID in_ParamID, AkRTPCExceptionChecker& in_exceptCheck, AkReal32 in_fValue, AkReal32 in_fDeltaValue);
	//
	// Scoped, Except - Update all parameter targets that match the key passed in "in_rtpckKey", while checking for exceptions with "in_exceptCheck". 
	void PushParamUpdate_ScopedExcept( AkRTPC_ParameterID in_ParamID, const AkRTPCKey& in_rtpckKey, AkRTPCExceptionChecker& in_exceptCheck, AkReal32 in_fValue, AkReal32 in_fDeltaValue );

	AkUInt64 GetAllRtpcBits() {
		return m_Data != NULL ? m_Data->m_RTPCBitArray.m_iBitArray : 0;
	}

private:
	//All data for the node is accessed through a pointer, and is only allocated when needed to reduce memory of node structures.
	CAkRTPCSubscriberNodeData* m_Data;

	bool CreateData();
	void DestroyData();
};


void UpdateAllParameterTargets();


#endif
