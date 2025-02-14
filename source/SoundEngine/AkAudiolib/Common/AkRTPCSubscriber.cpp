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

#include "stdafx.h"
#include "AkRTPCSubscriber.h"
#include <AK/Tools/Common/AkListBareLight.h>
#include "AkRTPCMgr.h"
#include "AkParameterNodeBase.h"


typedef  AkListBare<  CAkParameterTarget, AkListBareLightNextItem, AkCountPolicyNoCount, AkLastPolicyNoLast > AkTargetList;

AkForceInline AkHashType AkHash(AkTargetList& in_key) { return (AkHashType)(AkUIntPtr)in_key.First()->GetRootNode(); }

class AkParamTargetList
{
public:
	bool Add(CAkParameterTarget* in_pTarget)
	{
		AkHashType uKey = (AkHashType)(AkUIntPtr)in_pTarget->GetRootNode();		
		AkTargetList *pList = m_RootNodes.Exists(uKey);
		if (!pList)
		{
			pList = m_RootNodes.Set(uKey);
			if (!pList)
				return false;
		}		
		pList->AddFirst(in_pTarget);
		return true;
	}

	bool Remove(CAkParameterTarget* in_pTarget)
	{
		AKRESULT res = AK_Fail;
		AkHashType uKey = (AkHashType)(AkUIntPtr)in_pTarget->GetRootNode();
		AkRootNodeHash::IteratorEx itList = m_RootNodes.FindEx(uKey);
		if (itList != m_RootNodes.End())
		{
			res = itList.pItem->Assoc.item.Remove(in_pTarget);
			if (itList.pItem->Assoc.item.IsEmpty())
			{
				m_RootNodes.Erase(itList);
				if (m_RootNodes.Length() == 0)
					m_RootNodes.Term();
			}
		}	
		return res == AK_Success;
	}

	AkTargetList* Get(void* key)
	{
		return m_RootNodes.Exists((AkHashType)(AkUIntPtr)key);
	}

	typedef AkHashList<AkHashType, AkTargetList> AkRootNodeHash;

	AkRootNodeHash::Iterator Begin() { return m_RootNodes.Begin(); }
	AkRootNodeHash::Iterator End() { return m_RootNodes.End(); }

	void Term() { m_RootNodes.Term(); }

private:	
	AkRootNodeHash m_RootNodes;
};

static AkParamTargetList g_AllParamTargets;

//
// RegisteredTarget
//
			
RegisteredTarget::RegisteredTarget(AkRTPCBitArray in_params, CAkParameterTarget* in_pTgt) : params(in_params), pTgt(in_pTgt) {}
			
AkSortedParamTrtArray::Iterator AkSortedParamTrtArray::FindArrayPos(CAkParameterTarget* in_pTgt) const
	{
	return FindArrayPos(in_pTgt->GetRTPCKey());
	}

bool AkSortedParamTrtArray::RemoveTarget(CAkParameterTarget* in_pTgt)
{
	const AkRTPCKey& tgtKey = in_pTgt->GetRTPCKey();
	Iterator pFound = FindArrayPos(tgtKey);

	for (Iterator pItem = pFound, pEnd = End();
		pItem != pEnd &&
		(*pItem).pTgt->GetRTPCKey() == tgtKey;
		++pItem)
	{
		if ( (*pItem).pTgt == in_pTgt )
		{
			this->Erase( pItem );
			return true;
		}
	}

	return false;
}

////////////////////////
// CAkParameterTarget //
////////////////////////

void CAkParameterTarget::UpdateAllParameterTargets()
{
	for (AkParamTargetList::AkRootNodeHash::Iterator it = g_AllParamTargets.Begin(); it != g_AllParamTargets.End(); ++it)
	{
		AkTargetList &pList = (*it).item;
		for (AkTargetList::Iterator it = pList.Begin(); it != pList.End(); ++it)
		{
			CAkParameterTarget* tgt = (*it);
			tgt->UpdateRegistration();
		}
	}
}

void CAkParameterTarget::TermTargets()
{
	g_AllParamTargets.Term();
}

CAkParameterTarget::CAkParameterTarget(const AkRTPCKey& in_rtpcKey, bool in_bProagate): m_rtpcKey(in_rtpcKey), m_pRootNode(NULL)
#ifdef PARAM_TGT_CHECK
	, m_iRegCount(0)
#endif
{	
}

CAkParameterTarget::~CAkParameterTarget()
{
	UnregisterAllParamTarget();

	AKASSERT(m_pRootNode == NULL);
}

void CAkParameterTarget::SetRootNode(CAkParameterNodeBase* in_pNode)
{
	if (m_pRootNode == NULL)
	{
		AKASSERT(g_AllParamTargets.Get(this) == NULL);
		m_pRootNode = in_pNode;
		g_AllParamTargets.Add(this);	//No memory checking.  
										//Nothing of value can be done to recover and the only thing that 
										//will happen is RTPC notifications will miss this object.
	}
}

void CAkParameterTarget::RegisterParamTarget(CAkParameterNodeBase * in_pRootNode, const AkRTPCBitArray& in_requestedParams, bool in_bPropagateToBusHier)
{
	if (in_pRootNode != NULL)
	{
		AKASSERT(m_pRootNode == NULL || m_pRootNode == in_pRootNode );

		in_pRootNode->RegisterParameterTarget(this, in_requestedParams, in_bPropagateToBusHier);

		//Must set this member after registration, because IsRegistered() checks to see if it is not null.
		SetRootNode(in_pRootNode);
	}
}

void CAkParameterTarget::UpdateRegistration()
{
	RegisterParamTarget(m_pRootNode, GetTargetedParamsSet(), RegisterToBusHierarchy());
}

void CAkParameterTarget::UnregisterParamTarget(const AkRTPCBitArray& in_tgtParams, bool in_bPropagateToBusHier)
{	
	if (m_pRootNode != NULL)
	{
		g_AllParamTargets.Remove(this);
		m_pRootNode->UnregisterParameterTarget(this, in_tgtParams, in_bPropagateToBusHier);		
		m_pRootNode = NULL;
	}
#ifdef PARAM_TGT_CHECK
	AKASSERT( m_iRegCount == 0 );
#endif
}


///////////////////////////
// CAkRTPCSubscriberNode //
///////////////////////////

CAkRTPCSubscriberNode::CAkRTPCSubscriberNode(): m_Data(NULL)
{
}

CAkRTPCSubscriberNode::~CAkRTPCSubscriberNode()
{
#ifdef PARAM_TGT_CHECK
	//Ensure that all targets are unregistered before destructing.
	// Must call UnregisterAllParameterTargerts() or individually unregister.	
	AKASSERT(g_AllParamTargets.Get(this) == NULL);	
#endif

	if (m_Data != NULL)
	{
		for ( AkUInt32 iBit = 0; !m_Data->m_RTPCBitArray.IsEmpty(); ++iBit )
		{
			if ( m_Data->m_RTPCBitArray.IsSet( iBit ) )
			{
				g_pRTPCMgr->UnSubscribeRTPC( this, iBit );
				m_Data->m_RTPCBitArray.UnsetBit( iBit );
			}
		}

		DestroyData();
	}
}

void CAkRTPCSubscriberNode::UnregisterAllParameterTargets()
{
	AkTargetList *pList = g_AllParamTargets.Get(this);
	if (pList)
	{
		for (AkTargetList::Iterator it = pList->Begin(); it != pList->End(); ++it)
		{
			//We are the root node of a param target.  Need to unregister to maintain consistency.
			CAkParameterTarget* tgt = (*it);			
			tgt->UnregisterAllParamTarget();			
		}
	}
}

AKRESULT CAkRTPCSubscriberNode::SetRTPC( 
										AkRtpcID			in_RTPC_ID,
										AkRtpcType			in_RTPCType,
										AkRtpcAccum			in_RTPCAccum,
										AkRTPC_ParameterID	in_ParamID,
										AkUniqueID			in_RTPCCurveID,
										AkCurveScaling		in_eScaling,
										AkRTPCGraphPoint*	in_pArrayConversion,
										AkUInt32			in_ulConversionArraySize
										)
{
	AKRESULT res = AK_Fail;

	if ( CreateData() )
	{
		if (g_pRTPCMgr)
		{
			res = g_pRTPCMgr->SubscribeRTPC( 
				GetSubscriberPtr(),
				in_RTPC_ID,
				in_RTPCType,
				in_RTPCAccum,
				in_ParamID,
				in_RTPCCurveID,
				in_eScaling,
				in_pArrayConversion,
				in_ulConversionArraySize,
				AkRTPCKey(),
				CAkRTPCMgr::SubscriberType_CAkParameterNodeBase,
                HasActiveTargets()
				);
		}

		if (res == AK_Success )
		{
			res = EnableParam(in_ParamID);

			if (res == AK_Success &&
				in_RTPCType == AkRtpcType_Modulator)
			{
				m_Data->m_bCheckModulator = true;
			}
		}
	}

	return res;
}

AKRESULT CAkRTPCSubscriberNode::EnableParam( AkRTPC_ParameterID	in_ParamID )
{
	AKRESULT res = AK_Fail;

	if ( CreateData() )
	{
		if (!m_Data->m_RTPCBitArray.IsSet( in_ParamID ))
		{
			m_Data->m_RTPCBitArray.SetBit( in_ParamID );
			CAkParameterTarget::UpdateAllParameterTargets();
			m_Data->UpdateCommonParams();
		}
		
		res = AK_Success;
	}

	return res;
}

void CAkRTPCSubscriberNode::UnsetRTPC( AkRTPC_ParameterID in_ParamID, AkUniqueID in_RTPCCurveID  )
{
	if (m_Data != NULL)
	{
		bool bMoreCurvesRemaining = false;

		if (g_pRTPCMgr)
		{
			g_pRTPCMgr->UnSubscribeRTPC(
				this,
				in_ParamID,
				in_RTPCCurveID,
				&bMoreCurvesRemaining
				);
		}

		if ( ! bMoreCurvesRemaining )
		{
			DisableParam(in_ParamID);
		}
	}
}

void CAkRTPCSubscriberNode::DisableParam( AkRTPC_ParameterID in_ParamID )
{
	if (m_Data != NULL)
	{
		m_Data->m_RTPCBitArray.UnsetBit( in_ParamID );
		CAkParameterTarget::UpdateAllParameterTargets();

		if ( m_Data->m_RTPCBitArray == 0 && m_Data->m_targetsArray.IsEmpty() )
		{
			DestroyData();
		}
		else
		{
			m_Data->UpdateCommonParams();
		}
	}
}

void CAkRTPCSubscriberNode::PushParamUpdate( AkRTPC_ParameterID in_ParamID, const AkRTPCKey& in_rtpcKey, AkReal32 in_fValue, AkReal32 in_fDeltaValue, AkRTPCExceptionChecker* in_pExceptCheck )
{
    if ( in_rtpcKey.AnyFieldValid() )
    {
        if ( in_pExceptCheck == NULL )
            PushParamUpdate_Scoped( in_ParamID, in_rtpcKey, in_fValue, in_fDeltaValue  );
        else
            PushParamUpdate_ScopedExcept( in_ParamID, in_rtpcKey, *in_pExceptCheck, in_fValue, in_fDeltaValue  );
    }
    else
    {
        if ( in_pExceptCheck == NULL )
            PushParamUpdate_All( in_ParamID, in_fValue, in_fDeltaValue );
        else
            PushParamUpdate_Except( in_ParamID, *in_pExceptCheck, in_fValue, in_fDeltaValue );
    }
}

void CAkRTPCSubscriberNode::PushParamUpdate_Scoped( AkRTPC_ParameterID in_ParamID, const AkRTPCKey& in_rtpckKey, AkReal32 in_fValue, AkReal32 in_fDeltaValue )
{
	// NOTES:
	//	Currently this function only updates the correct targets is the in_rtpckKey is specified up to a certain key level (no 'wildcards'), and then the rest of the keys are unspecified.
	//	EG.  < GO:1, PID:5, CH:6, NOTE:*ANY*, Voice: *ANY* > will work but 
	//		 < GO:1, PID: *ANY*, CH:6, NOTE:*ANY*, Voice: *ANY* >  will not work, because it has to visit multiple sub-trees.

	// MORE NOTES:
	//	This also does not work if we are trying to match an item in the array that is less specified than the key passed into this function.  Eg. register a target with key
	//	< GO:1, PID:*ANY*, CH:*ANY*, NOTE:*ANY*, Voice:*ANY* >, and try to update all targets matching key < GO:1, PID:5, CH:*ANY*, NOTE:*ANY*, Voice:*ANY* >. 
	//	In other words, wildcards do not currently work for target keys.

	AKASSERT(m_Data);
	
	AkRTPCBitArray bfParam = (1ULL << in_ParamID);

	if ( (m_Data->m_CommonParams & bfParam) != 0 )
	{
		for (AkSortedParamTrtArray::Iterator pItem = m_Data->m_targetsArray.FindArrayPos(in_rtpckKey), pEnd = m_Data->m_targetsArray.End();
			pItem != pEnd && (*pItem).pTgt->GetRTPCKey().MatchValidFields( in_rtpckKey );
			++pItem )
		{
			(*pItem).pTgt->UpdateTargetParam( in_ParamID, in_fValue, in_fDeltaValue );
		}
	}
	else
	{
		for (AkSortedParamTrtArray::Iterator pItem = m_Data->m_targetsArray.FindArrayPos(in_rtpckKey), pEnd = m_Data->m_targetsArray.End();
			pItem != pEnd && (*pItem).pTgt->GetRTPCKey().MatchValidFields( in_rtpckKey );
			++pItem )
		{
			if (( bfParam & (*pItem).params) != 0)
				(*pItem).pTgt->UpdateTargetParam( in_ParamID, in_fValue, in_fDeltaValue );
		}
	}
}

void CAkRTPCSubscriberNode::PushParamUpdate_All( AkRTPC_ParameterID in_ParamID, AkReal32 in_fValue, AkReal32 in_fDeltaValue )
{
	AKASSERT(m_Data);

	AkRTPCBitArray bfParam = (1ULL << in_ParamID);
	AkSortedParamTrtArray::Iterator it = m_Data->m_targetsArray.Begin(), itEnd = m_Data->m_targetsArray.End();

	if ( (m_Data->m_CommonParams & bfParam) != 0 ) 
	{
		for ( ; it != itEnd; ++it )
		{
			(*it).pTgt->UpdateTargetParam( in_ParamID, in_fValue, in_fDeltaValue );
		}
	}
	else
	{
		for ( ; it != itEnd; ++it )
			if (( bfParam & (*it).params) != 0)
				(*it).pTgt->UpdateTargetParam( in_ParamID, in_fValue, in_fDeltaValue );
	}

}

void CAkRTPCSubscriberNode::PushParamUpdate_Except( AkRTPC_ParameterID in_ParamID, AkRTPCExceptionChecker& in_exCheck, AkReal32 in_fValue, AkReal32 in_fDeltaValue)
{	
	AKASSERT(m_Data);

	AkRTPCBitArray bfParam = (1ULL << in_ParamID);
	AkSortedParamTrtArray::Iterator it = m_Data->m_targetsArray.Begin(), itEnd = m_Data->m_targetsArray.End();

	if ( ( m_Data->m_CommonParams & bfParam ) != 0 ) 
	{
		for ( ; it != itEnd; ++it )
		{
			if (!in_exCheck.IsException( (*it).pTgt->GetRTPCKey() ))
			{
				(*it).pTgt->UpdateTargetParam( in_ParamID, in_fValue, in_fDeltaValue );
			}
		}
	}
	else
	{
		for ( ; it != itEnd; ++it )
			if (( bfParam & (*it).params) != 0 && !in_exCheck.IsException( (*it).pTgt->GetRTPCKey() ))
				(*it).pTgt->UpdateTargetParam( in_ParamID, in_fValue, in_fDeltaValue );
	}
}

void CAkRTPCSubscriberNode::PushParamUpdate_ScopedExcept( AkRTPC_ParameterID in_ParamID, const AkRTPCKey& in_rtpckKey, AkRTPCExceptionChecker& in_exCheck, AkReal32 in_fValue, AkReal32 in_fDeltaValue)
{	
	AKASSERT(m_Data);

	AkRTPCBitArray bfParam = (1ULL << in_ParamID);

	if ( (m_Data->m_CommonParams & bfParam) != 0 )
	{
		for (AkSortedParamTrtArray::Iterator pItem = m_Data->m_targetsArray.FindArrayPos(in_rtpckKey), pEnd = m_Data->m_targetsArray.End();
			pItem != pEnd && (*pItem).pTgt->GetRTPCKey().MatchValidFields( in_rtpckKey );
			++pItem )
		{
			if (!in_exCheck.IsException( (*pItem).pTgt->GetRTPCKey() ))
			{
				(*pItem).pTgt->UpdateTargetParam( in_ParamID, in_fValue, in_fDeltaValue );
			}
		}
	}
	else
	{
		for (AkSortedParamTrtArray::Iterator pItem = m_Data->m_targetsArray.FindArrayPos(in_rtpckKey), pEnd = m_Data->m_targetsArray.End();
			pItem != pEnd && (*pItem).pTgt->GetRTPCKey().MatchValidFields( in_rtpckKey );
			++pItem )
		{
			if (( bfParam & (*pItem).params) != 0 && !in_exCheck.IsException( (*pItem).pTgt->GetRTPCKey()) )
				(*pItem).pTgt->UpdateTargetParam( in_ParamID, in_fValue, in_fDeltaValue );
		}
	}
}

void CAkRTPCSubscriberNode::NotifyParamChanged( bool in_bLiveEdit, AkRTPC_ParameterID in_rtpcID )
{
	AKASSERT(m_Data);

	for ( AkSortedParamTrtArray::Iterator it = m_Data->m_targetsArray.Begin(); it != m_Data->m_targetsArray.End(); ++it )
	{
		CAkParameterTarget* tgt = (*it).pTgt;
		tgt->NotifyParamChanged(in_bLiveEdit, in_rtpcID);
	}
}

void CAkRTPCSubscriberNode::NotifyParamsChanged( bool in_bLiveEdit, const AkRTPCBitArray& in_bitArray )
{
	AKASSERT(m_Data);

	for ( AkSortedParamTrtArray::Iterator it = m_Data->m_targetsArray.Begin(); it != m_Data->m_targetsArray.End(); ++it )
	{
		CAkParameterTarget* tgt = (*it).pTgt;
		tgt->NotifyParamsChanged(in_bLiveEdit, in_bitArray);
	}
}


void CAkRTPCSubscriberNode::GetModulatorParamXfrm( AkModulatorParamXfrmArray& io_xforms, AkRTPC_ParameterID rtpcParamId, AkRtpcID in_modulatorID, const CAkRegisteredObj * in_GameObjPtr ) const
{
	AkModulatorParamXfrm thisXfrm;
	if( g_pModulatorMgr->GetParamXfrm( (void*)this, rtpcParamId, in_modulatorID, in_GameObjPtr, thisXfrm ) )
		io_xforms.AddLast( thisXfrm );
}

bool CAkRTPCSubscriberNode::CreateData()
{
	if (m_Data != NULL)
	{
		return true;
	}
	else
	{
		m_Data = AkNew(AkMemID_Structure, CAkRTPCSubscriberNodeData() );
		return (m_Data != NULL);
	}
}

void CAkRTPCSubscriberNode::DestroyData()
{
	AkDelete(AkMemID_Structure, m_Data );
	m_Data = NULL;
}
