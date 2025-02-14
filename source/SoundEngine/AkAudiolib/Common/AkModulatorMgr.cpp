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
// AkModulatorMgr.cpp
//
// creator : nharris
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkModulatorMgr.h"
#include "AkModulatorEngine.h"

AkRTPCKey AkModulatorTriggerParams::GetRTPCKey() const
{
	AkRTPCKey key;
	key.GameObj() = pGameObj;
	key.PlayingID() = playingId;
	key.MidiChannelNo() = midiEvent.GetNoteAndChannel().channel;
	key.MidiNoteNo() = midiEvent.GetNoteAndChannel().note;
	key.MidiTargetID() = midiTargetId;
	key.PBI() = pPbi;
	return key;
}



CAkModulatorMgr::CAkModulatorMgr(): m_pEngine(NULL)
{
}

CAkModulatorMgr::~CAkModulatorMgr()
{
}

AKRESULT CAkModulatorMgr::Init()
{
	if (!m_ModulatorSubscribers.Init(0))
		return AK_InsufficientMemory;

	m_pEngine = AkNew(AkMemID_Object, CAkModulatorEngine());
	if (m_pEngine)
		return AK_Success;
	 
	return AK_InsufficientMemory;
}

void CAkModulatorMgr::Term()
{
	AKASSERT( g_pIndex->m_idxModulators.m_mapIDToPtr.Length() == 0 );
	m_ModulatorSubscribers.Term();

	TermModulatorEngine();
}

void CAkModulatorMgr::TermModulatorEngine()
{
	if (m_pEngine)
	{
		AkDelete(AkMemID_Object, m_pEngine);
		m_pEngine = NULL;
	}
}

void CAkModulatorMgr::GetModulators(const AkModulatorSubscriberInfo& in_info, AkModulatorsToTrigger& out_modulators)
{
	AKASSERT(in_info.pSubscriber != NULL);
	AkModulatorArray* pModulators = m_ModulatorSubscribers.Exists((AkUIntPtr)in_info.pSubscriber);
	if (pModulators)
		out_modulators.AddLast(AkModulatorToTrigger(in_info, pModulators));
}

AKRESULT CAkModulatorMgr::Trigger(const AkModulatorsToTrigger& in_modulators, const AkModulatorTriggerParams& in_params, CAkModulatorData* io_pModPBIData)
{
	AKRESULT overallResult = AK_Success;
	
	for (AkModulatorsToTrigger::Iterator itModToTrig = in_modulators.Begin(),
		itModToTrigEnd = in_modulators.End();
		itModToTrig != itModToTrigEnd;
		++itModToTrig)
	{
		AkModulatorArray* pModulators = (*itModToTrig).pModulators;
		AkModulatorSubscriberInfo&	subscriberInfo = (*itModToTrig).subscriberInfo;

		for (AkModulatorArray::Iterator it = pModulators->Begin(),
			itEnd = pModulators->End();
			it != itEnd; ++it)
		{
			CAkModulator* pMod = *it;
			AKRESULT res = pMod->Trigger(subscriberInfo, in_params, *m_pEngine, io_pModPBIData);
			overallResult = (overallResult == AK_Success && res == AK_Success) ? AK_Success : AK_Fail;

			{
				//For modulators that are modulated by other modulators.
				AkModulatorSubscriberInfo subscriber;
				subscriber.pSubscriber = pMod;
				subscriber.pTargetNode = subscriberInfo.pTargetNode; //inherit target node.
				subscriber.eSubscriberType = CAkRTPCMgr::SubscriberType_Modulator;
				subscriber.eNarrowestSupportedScope = subscriberInfo.eNarrowestSupportedScope;  //inherit scope restriction.
				Trigger(subscriber, in_params, io_pModPBIData);
			}
		}
	}
	

	return overallResult;
}

AKRESULT CAkModulatorMgr::Trigger(const AkModulatorSubscriberInfo& in_subscriberInfo, const AkModulatorTriggerParams& in_params, CAkModulatorData* io_pModPBIData)
{
	AKRESULT overallResult = AK_Success;
	AKASSERT(in_subscriberInfo.pSubscriber != NULL);
	AkModulatorArray* pModulators = m_ModulatorSubscribers.Exists((AkUIntPtr)in_subscriberInfo.pSubscriber);
	if (pModulators)
	{
		for (AkModulatorArray::Iterator it = pModulators->Begin(),
			itEnd = pModulators->End();
			it != itEnd; ++it)
		{
			CAkModulator* pMod = *it;
			AKRESULT res = pMod->Trigger(in_subscriberInfo, in_params, *m_pEngine, io_pModPBIData);
			overallResult = (overallResult == AK_Success && res == AK_Success) ? AK_Success : AK_Fail;

			{
				//For modulators that are modulated by other modulators.
				AkModulatorSubscriberInfo subscriberInfo;
				subscriberInfo.pSubscriber = pMod;
				subscriberInfo.eSubscriberType = CAkRTPCMgr::SubscriberType_Modulator;
				subscriberInfo.eNarrowestSupportedScope = in_subscriberInfo.eNarrowestSupportedScope;  //inherit scope restriction.
				Trigger(subscriberInfo, in_params, io_pModPBIData);
			}
		}
	}

	return overallResult;
}

AKRESULT CAkModulatorMgr::AddSubscription( AkRtpcID in_rtpcID, CAkRTPCMgr::AkRTPCSubscription * in_pSubscription )
{
	AKRESULT res = AK_Fail;

	CAkModulator* pMod = g_pIndex->m_idxModulators.GetPtrAndAddRef( in_rtpcID );
	if( pMod )
	{
		AkModulatorArray* pModArray = m_ModulatorSubscribers.Exists((AkUIntPtr)in_pSubscription->key.pSubscriber);
		if (!pModArray)
		{
			//TODO: The number of items in this array is constant throughout the lifetime of a game that is not
			//	live-edited, so it would be nice to pre-allocated this array space here.
			pModArray = AkNew(AkMemID_Object, AkModulatorArray());
			if (pModArray)
			{
				pModArray->key = (AkUIntPtr)in_pSubscription->key.pSubscriber;
				if(!m_ModulatorSubscribers.Set(pModArray))
				{
					AkDelete(AkMemID_Object, pModArray);
					pModArray = NULL;
				}
					
			}
		}
		
		if (pModArray)
		{
			bool exists = false;
			for (AkModulatorArray::Iterator it = pModArray->Begin(),
				itEnd = pModArray->End(); 
				it != itEnd; ++it)
			{
				if (*it == pMod)
				{
					exists = true;
					res = AK_Success;
					break;
				}
			}

			if ( !exists && pModArray->AddLast(pMod) )
			{
				pMod->AddRef();
				res = AK_Success;
			}

			if (res == AK_Success)
			{
				res = pMod->AddSubscription(in_pSubscription);
			}
		}
	
		pMod->Release();
	}
	
	return res;
}

void CAkModulatorMgr::RemoveSubscription( CAkRTPCMgr::AkRTPCSubscription * in_pSubscription, AkRtpcID in_RTPCid )
{
	AkModulatorArray* pModulatorsArr = m_ModulatorSubscribers.Exists((AkUIntPtr)in_pSubscription->key.pSubscriber);
	if (pModulatorsArr)
	{
		for (AkModulatorArray::Iterator it = pModulatorsArr->Begin();
			it != pModulatorsArr->End(); )
		{
			CAkModulator* pMod = *it;
			if (in_RTPCid == AK_INVALID_UNIQUE_ID || pMod->key == in_RTPCid)
			{
				if ( pMod->RemoveSubscriptionIfNoCurvesRemain(in_pSubscription) )
				{
					//There may be other subscriptions (modulating a different parameter) but with the same subscriber.
					//	If not, remove the reference to the modulator.
					if ( !pMod->HasSubscriber(in_pSubscription->key.pSubscriber) )
					{
						it = pModulatorsArr->Erase(it);
						pMod->Release();
						continue;
					}
				}
			}


			++it;
		}

		if (pModulatorsArr->IsEmpty())
		{
			pModulatorsArr->Term();
			m_ModulatorSubscribers.Unset((AkUIntPtr)in_pSubscription->key.pSubscriber);
			AkDelete(AkMemID_Object, pModulatorsArr);
		}
	}
}

bool CAkModulatorMgr::GetCurrentModulatorValue(
							  AkRtpcID in_RTPC_ID,
							  AkRTPCKey& io_rtpcKey,
							  AkReal32& out_value
							  )
{
	bool ret = false;
	if( CAkModulator* pMod = g_pIndex->m_idxModulators.GetPtrAndAddRef( in_RTPC_ID ) )
	{
		ret = pMod->GetCurrentValue(io_rtpcKey, out_value);
		pMod->Release();
	}
	return ret;
}

bool CAkModulatorMgr::SupportsAutomatedParams(
							  AkRtpcID in_RTPC_ID
							  )
{
	bool ret = false;
	if( CAkModulator* pMod = g_pIndex->m_idxModulators.GetPtrAndAddRef( in_RTPC_ID ) )
	{
		ret = pMod->SupportsAutomatedParams();
		pMod->Release();
	}
	return ret;
}

bool CAkModulatorMgr::GetParamXfrm( const void* in_pSubscriber, AkRTPC_ParameterID in_ParamID, AkRtpcID in_modulatorID, const CAkRegisteredObj* in_GameObj, AkModulatorParamXfrm& out_paramXfrm )
{
	bool res = false;

	CAkRTPCMgr::AkRTPCSubscription* pSubscr = g_pRTPCMgr->GetSubscription(in_pSubscriber, in_ParamID);
	if ( pSubscr && IsAutomatedParam(in_ParamID, pSubscr->eType) && SupportsAutomatedParams(in_modulatorID) )
	{
		// Only effects subscribe to a specific game object, and effects can not have automated parameters, therefor this should always be true.
		AKASSERT(( (in_GameObj == NULL && pSubscr->TargetKey.GameObj() == NULL) || in_GameObj == pSubscr->TargetKey.GameObj())
			|| (in_GameObj == NULL) 
			|| (pSubscr->TargetKey.GameObj() == NULL)
			);
		
		for (CAkRTPCMgr::RTPCCurveArray::Iterator curveIt = pSubscr->Curves.Begin(); curveIt != pSubscr->Curves.End(); ++curveIt )
		{
			CAkRTPCMgr::RTPCCurve& curve = *curveIt;

			if (curve.RTPC_ID == in_modulatorID)
			{
				out_paramXfrm.m_rtpcParamID = in_ParamID;
				curve.ConversionTable.GetLine(out_paramXfrm.m_fScale, out_paramXfrm.m_fOffset);
				res = true;
			}
		}
		
	}

	return res;
}

void CAkModulatorMgr::ProcessModulators()
{
	AKASSERT(m_pEngine);
	m_pEngine->ProcessModulators(AK_NUM_VOICE_REFILL_FRAMES);

	{
		AkAutoLock<CAkLock> IndexLock(g_pIndex->m_idxModulators.GetLock());

		CAkIndexItem< CAkModulator* >::AkMapIDToPtr::Iterator it = g_pIndex->m_idxModulators.m_mapIDToPtr.Begin();
		for (; it != g_pIndex->m_idxModulators.m_mapIDToPtr.End(); ++it)
		{
			CAkModulator* pModulator = static_cast<CAkModulator*>(*it);
			pModulator->NotifySubscribers();
		}
	}

	m_pEngine->CleanUpFinishedCtxs();
}


void CAkModulatorMgr::RemovedScopedRtpcObj( AkRtpcID in_RTPCid, const AkRTPCKey& in_rtpcKey )
{
	CAkModulator* pMod = g_pIndex->m_idxModulators.GetPtrAndAddRef(in_RTPCid);
	if (pMod)
	{
		pMod->RemoveCtxsMatchingKey( in_rtpcKey  );
		pMod->Release();
	}
}

void CAkModulatorMgr::CleanUpFinishedCtxs()
{
	if (m_pEngine)
		m_pEngine->CleanUpFinishedCtxs();
}
