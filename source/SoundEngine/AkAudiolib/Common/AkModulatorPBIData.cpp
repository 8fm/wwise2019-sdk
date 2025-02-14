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
// AkModulatorPBIData.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkModulatorPBIData.h"
#include "AkModulatorCtx.h"
#include "AkLayer.h"
#include "AkParameterNodeBase.h"

CAkModCtxRefContainer::~CAkModCtxRefContainer()
{
	Release();
	m_Ctx.Term();
}

CAkModCtxRefContainer& CAkModCtxRefContainer::operator=(const CAkModCtxRefContainer& in_from)
{
	Release();
	DoCopy(in_from);
	return *this;
}

void CAkModCtxRefContainer::AddModulatorCtx(CAkModulatorCtx* in_pCtx)
{
	CAkModulatorCtx** ppCtx = m_Ctx.AddLast(in_pCtx);
	if (ppCtx != NULL)
	{
		(*ppCtx)->AddRef();
		(*ppCtx)->IncrVoiceCount();
	}
}

bool CAkModCtxRefContainer::HasModulatorCtx(CAkModulatorCtx* in_pCtx)
{
	for (ModulatorCtxArray::Iterator it = m_Ctx.Begin(); it != m_Ctx.End(); ++it)
	{
		if ((*it) == in_pCtx)
			return true;
	}
	return false;
}

void CAkModCtxRefContainer::Release()
{
	for (ModulatorCtxArray::Iterator it = m_Ctx.Begin(); it != m_Ctx.End(); ++it)
	{
		(*it)->DecrVoiceCount();
		(*it)->Release();
	}
	m_Ctx.RemoveAll();
}

AKRESULT CAkModCtxRefContainer::DoCopy(const CAkModCtxRefContainer& in_from)
{
	if (m_Ctx.Reserve(in_from.m_Ctx.Length()) == AK_Success)
	{
		for (ModulatorCtxArray::Iterator it = in_from.m_Ctx.Begin(); it != in_from.m_Ctx.End(); ++it)
			AddModulatorCtx(*it);
		return AK_Success;
	}
	else
		return AK_InsufficientMemory;
}


CAkModulatorPBIData::CAkModulatorPBIData(): m_uRef(1) {}
CAkModulatorPBIData::~CAkModulatorPBIData(){Term();}

AKRESULT CAkModulatorPBIData::Init()
{
	return m_ctxParamsList.Reserve(4);
}

void CAkModulatorPBIData::Term()
{
#ifndef AK_OPTIMIZED
	m_arSubscribersID.Term();
#endif

	for( AkModCtxParamsList::Iterator it = m_ctxParamsList.Begin(); it != m_ctxParamsList.End(); ++it )
	{
		(*it).pCtx->DecrVoiceCount();
		(*it).pCtx->Release();
		(*it).paramXfrms.Term();
	}

	m_ctxParamsList.Term();
}

AKRESULT CAkModulatorPBIData::AddModulationSource(CAkModulatorCtx* in_pModCtx, const AkModulatorTriggerParams& in_triggerParams, const AkModulatorSubscriberInfo& in_subscrInfo )
{
	AKASSERT(in_pModCtx != NULL );

	AkModCtxParams * ctxParamXfrms = NULL;

	// We do not want to have the same modulator context in the list more than once.  This would result in the effect being applied multiple times.
	for( AkModCtxParamsList::Iterator it = m_ctxParamsList.Begin(); it != m_ctxParamsList.End(); ++it )
	{
		if (in_pModCtx == (*it).pCtx)
		{
			ctxParamXfrms = &*it;
		}
	}

	if (!ctxParamXfrms)
	{
		if ( NULL != (ctxParamXfrms = m_ctxParamsList.AddLast()) )
		{
			ctxParamXfrms->pCtx = in_pModCtx;
			ctxParamXfrms->pCtx->AddRef();
			ctxParamXfrms->pCtx->IncrVoiceCount();
		}
	}

	if (ctxParamXfrms)
	{
		if (in_subscrInfo.pSubscriber)
		{
			// add the scale and offset for each audio-rate-modulated parameter to the xform map.
			if (in_subscrInfo.eSubscriberType == CAkRTPCMgr::SubscriberType_CAkParameterNodeBase)
			{
				const CAkParameterNodeBase* pNode = static_cast<const CAkParameterNodeBase* >(reinterpret_cast<const CAkRTPCSubscriberNode*>(in_subscrInfo.pSubscriber));
				pNode->GetModulatorParamXfrms(ctxParamXfrms->paramXfrms, in_pModCtx->GetModulatorID(), in_triggerParams.pGameObj );

#ifndef AK_OPTIMIZED
				// Add node ID to the list of modulation sources for profiling
				m_arSubscribersID.AddLast(pNode->ID());
#endif
			}
			else if (in_subscrInfo.eSubscriberType == CAkRTPCMgr::SubscriberType_CAkCrossfadingLayer || in_subscrInfo.eSubscriberType == CAkRTPCMgr::SubscriberType_CAkLayer)
			{
				const CAkLayer* pLayer = reinterpret_cast<const CAkLayer*>(in_subscrInfo.pSubscriber);
				pLayer->GetModulatorParamXfrms(ctxParamXfrms->paramXfrms, in_pModCtx->GetModulatorID(), in_triggerParams.pGameObj );
			}

			// If we add support for audio-rate modulated parameters to any other subscriber type, then we will have to get
			//	the param transforms here.
		}

		return AK_Success;
	}

	return AK_Fail;
}

bool CAkModulatorPBIData::HasModulationSource(CAkModulatorCtx* in_pModCtx) const
{
	for (AkModCtxParamsList::Iterator it = m_ctxParamsList.Begin(); it != m_ctxParamsList.End(); ++it)
	{
		if (in_pModCtx == (*it).pCtx)
			return true;
	}
	return false;
}

void CAkModulatorPBIData::TriggerRelease( AkUInt32 in_uFrameOffset )
{
	for( AkModCtxParamsList::Iterator it = m_ctxParamsList.Begin(); it != m_ctxParamsList.End(); ++it )
	{
		(*it).pCtx->TriggerRelease( in_uFrameOffset );
	}
}

void CAkModulatorPBIData::Pause()
{
	for( AkModCtxParamsList::Iterator it = m_ctxParamsList.Begin(); it != m_ctxParamsList.End(); ++it )
		(*it).pCtx->Pause();
}

void CAkModulatorPBIData::Resume()
{
	for( AkModCtxParamsList::Iterator it = m_ctxParamsList.Begin(); it != m_ctxParamsList.End(); ++it )
		(*it).pCtx->Resume();
}

void CAkModulatorPBIData::GetNumAutomatedModulators( const AkRTPCBitArray& in_paramIDs, AkUInt32& out_numBufs, AkUInt32& out_numGains ) const
{
	out_numBufs = out_numGains = 0;
	for( AkModCtxParamsList::Iterator it = m_ctxParamsList.Begin(); it != m_ctxParamsList.End(); ++it )
	{
		AkModCtxParams& params = *it;
		if( params.pCtx != NULL )
		{
			AkModulatorParamXfrmArray::Iterator ip = params.paramXfrms.Begin();
			for( ; ip != params.paramXfrms.End(); ++ip )
			{
				if ( in_paramIDs.IsSet( (*ip).m_rtpcParamID ) )
				{
					if( params.pCtx->GetOutputBuffer() )
						out_numBufs++;
					else
						out_numGains++;
				}
			}
		}
	}
}

void CAkModulatorPBIData::ClearModulationSources()
{
	for( AkModCtxParamsList::Iterator it = m_ctxParamsList.Begin(); it != m_ctxParamsList.End(); ++it )
		(*it).paramXfrms.RemoveAll();
}

void CAkModulatorPBIData::GetBufferList( const AkRTPCBitArray& in_paramIDs, 
											AkModulatorXfrm out_pXfrmArray[], 
											AkModulatorGain out_pGainArray[],
											AkReal32* out_pAutmBufArray[], 
											AkUInt32& io_uXfrmArrSize,
											AkUInt32& io_uGainArrSize ) const
{
	AkUInt32 xfrmCount = 0;
	AkUInt32 gainCount = 0;

	for( AkModCtxParamsList::Iterator it = m_ctxParamsList.Begin(); it != m_ctxParamsList.End(); ++it )
	{
		AkModCtxParams& params = *it;
		if( params.pCtx != NULL )
		{
			AkModulatorParamXfrmArray::Iterator ip = params.paramXfrms.Begin();
			for( ; ip != params.paramXfrms.End(); ++ip )
			{
				if( in_paramIDs.IsSet( (*ip).m_rtpcParamID ) )
				{
					if( params.pCtx->GetOutputBuffer() != NULL && xfrmCount < io_uXfrmArrSize )
					{
						out_pAutmBufArray[xfrmCount] = params.pCtx->GetOutputBuffer();
						out_pXfrmArray[xfrmCount] = *ip;
						xfrmCount++;
					}
					else if( params.pCtx->GetOutputBuffer() == NULL && gainCount < io_uGainArrSize )
					{
						out_pGainArray[gainCount].m_fStart = params.pCtx->GetPreviousValue() * (*ip).m_fScale + (*ip).m_fOffset;
						out_pGainArray[gainCount].m_fEnd = params.pCtx->GetCurrentValue() * (*ip).m_fScale + (*ip).m_fOffset;
						gainCount++;
					}
				}
			}
		}
	}

	io_uXfrmArrSize = xfrmCount;
	io_uGainArrSize = gainCount;
}

AkReal32 CAkModulatorPBIData::GetCtrlRateOutput( const AkRTPCBitArray& in_paramIDs )
{
	AkReal32 fVal = 1.0f;
	for( AkModCtxParamsList::Iterator it = m_ctxParamsList.Begin(); it != m_ctxParamsList.End(); ++it )
	{
		AkModCtxParams& params = *it;
		AkModulatorParamXfrmArray::Iterator xfrm = params.paramXfrms.Begin();
		for( ; xfrm != params.paramXfrms.End(); ++xfrm )
		{
			if( in_paramIDs.IsSet( (*xfrm).m_rtpcParamID ) )
				fVal *= params.pCtx->GetPreviousValue() * (*xfrm).m_fScale + (*xfrm).m_fOffset;
		}
	}

	return fVal;
}

AkReal32 CAkModulatorPBIData::GetPeakOutput( const AkRTPCBitArray& in_paramIDs )
{
	AkReal32 fVal = 1.0f;
	for( AkModCtxParamsList::Iterator it = m_ctxParamsList.Begin(); it != m_ctxParamsList.End(); ++it )
	{
		AkModCtxParams& params = *it;
		AkModulatorParamXfrmArray::Iterator xfrm = params.paramXfrms.Begin();
		for( ; xfrm != params.paramXfrms.End(); ++xfrm )
		{
			if (in_paramIDs.IsSet((*xfrm).m_rtpcParamID))
			{				
				AkReal32 fModPeak = params.pCtx->GetPeakValue();
				AkReal32 fThis = fModPeak * (*xfrm).m_fScale + (*xfrm).m_fOffset;
				fVal *= fThis;

#ifndef AK_OPTIMIZED
				// Null modulator happens on last frame. Don't log. If GetPeakOutput is used outside of a brace, it is ok (probably profiling)
				if (params.pCtx->Modulator() && AkDeltaMonitor::IsSoundBraceOpen())
				{
					// The same modulator can be triggered multiple times by different nodes, log all contributions with their object
					for (SubscribersIDList::Iterator itID = m_arSubscribersID.Begin(); itID != m_arSubscribersID.End(); ++itID)
					{
						AkDeltaMonitor::LogModulator(params.pCtx->GetModulatorID(), (*xfrm).m_rtpcParamID, fThis, fModPeak, (*itID), params.pCtx->GetGameObj() != NULL ? AkDelta_Modulator : AkDelta_ModulatorGlobal);
					}
				}
#endif
			}
		}
	}

	return fVal;
}

void CAkModulatorPBIData::RemovePbiRefs()
{
	for( AkModCtxParamsList::Iterator it = m_ctxParamsList.Begin(); it != m_ctxParamsList.End(); ++it )
		(*it).pCtx->RemovePbiRef();
}

void CAkModulatorPBIData::GetModulatorCtxs(AkModulatorScope in_eDesiredScope, CAkModCtxRefContainer& out_Modulators)
{
	for (AkModCtxParamsList::Iterator it = m_ctxParamsList.Begin(); it != m_ctxParamsList.End(); ++it)
	{
		if ((*it).pCtx->GetScope() == in_eDesiredScope)
			out_Modulators.AddModulatorCtx((*it).pCtx);
	}
}

AkUInt32 CAkModulatorPBIData::AddRef()
{
	return ++m_uRef;
}

AkUInt32 CAkModulatorPBIData::Release()
{
	AKASSERT( m_uRef > 0 ); 
	AkInt32 lRef = --m_uRef;
	
	if (!lRef)
		AkDelete(AkMemID_Object, this);
	
	return lRef; 
}

//
//////////////////////////////////////////////////////////////////////////


CAkModulatorData::CAkModulatorData(): m_pData(NULL) , m_uReleaseOffset(INVALID_RELEASE_OFFSET)
{}

CAkModulatorData::~CAkModulatorData()
{
	ReleaseData();
}

void CAkModulatorData::ReleaseData()
{	
	if (m_pData)
	{
		m_pData->RemovePbiRefs();
		m_pData->Release();
		m_pData = NULL;
	}
}

AKRESULT CAkModulatorData::AllocData()
{
	AKRESULT res = AK_Fail;

	AKASSERT(m_pData == NULL);
	m_pData = AkNew(AkMemID_Object, CAkModulatorPBIData() );
	if (m_pData)
	{
		m_pData->Init();
		res = AK_Success;
	}

	return res;
}

