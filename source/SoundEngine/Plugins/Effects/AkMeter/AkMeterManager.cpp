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

#include "AkMeterManager.h"
#include <AK/Tools/Common/AkAutoLock.h>
#include "AkAudioLib.h"
#include "AkRegistryMgr.h"
#include "AkRTPCMgr.h"
#include "IAkGlobalPluginContextEx.h"
#include <float.h>

struct MeterKey
{
	MeterKey() : uGameParamID(AK_INVALID_UNIQUE_ID), uGameObjID(AK_INVALID_GAME_OBJECT), uNodeID(AK_INVALID_UNIQUE_ID) {}
	MeterKey(CAkMeterFX * in_pMeter) : 
		  uGameParamID(in_pMeter->m_uGameParamID)
		, uGameObjID(in_pMeter->m_uGameObjectID)
		, uNodeID(in_pMeter->m_uNodeID) {}

	AkUniqueID uGameParamID;
	AkGameObjectID uGameObjID;
	AkUniqueID uNodeID;

	bool operator<(const MeterKey& in_rhs)
	{
		return  (uGameParamID < in_rhs.uGameParamID) 
				|| 
				((uGameParamID == in_rhs.uGameParamID) && (uGameObjID < in_rhs.uGameObjID)) 
				||
				((uGameObjID == in_rhs.uGameObjID) && (uNodeID < in_rhs.uNodeID));
	}
};

CAkMeterManager * CAkMeterManager::pInstance = NULL;

CAkMeterManager::CAkMeterManager(AK::IAkPluginMemAlloc * in_pAllocator, AK::IAkGlobalPluginContext * in_pCtx)
	: m_pAllocator( in_pAllocator )
	, m_pGlobalContext(in_pCtx)
{
	AKASSERT(pInstance == NULL);
	pInstance = this;

	// Register the callbacks seperately to only time the rendering
	in_pCtx->RegisterGlobalCallback( AkPluginTypeEffect, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_METER, CAkMeterManager::BehavioralExtension, AkGlobalCallbackLocation_BeginRender | AkGlobalCallbackLocation_Term );
}

CAkMeterManager::~CAkMeterManager() 
{
	m_pGlobalContext->UnregisterGlobalCallback( CAkMeterManager::BehavioralExtension, AkGlobalCallbackLocation_BeginRender | AkGlobalCallbackLocation_Term );
	m_meters.Term();
	pInstance = NULL;
}

void CAkMeterManager::Register(CAkMeterFX * in_pFX)
{ 
	AkAutoLock<CAkLock> IndexLock(m_RegistrationLock);

	MeterKey newMeterKey(in_pFX);

	Meters::IteratorEx it = m_meters.BeginEx();
	while (it != m_meters.End() && newMeterKey < MeterKey(*it))
		++it;
	
	m_meters.Insert(it, in_pFX);
}

void CAkMeterManager::Unregister(CAkMeterFX * in_pFX)
{
	AkAutoLock<CAkLock> IndexLock(m_RegistrationLock);

	Meters::IteratorEx it = m_meters.BeginEx();
	while (it != m_meters.End())
	{
		if (*it == in_pFX)
		{
			m_meters.Erase(it);
			break;
		}

		++it;
	}
}

void CAkMeterManager::BehavioralExtension(AK::IAkGlobalPluginContext * in_pContext, AkGlobalCallbackLocation in_eLocation, void * in_pCookie)
{ 
	AKASSERT(pInstance);
	pInstance->Execute();
}

void CAkMeterManager::Execute()
{
	struct ValueEntry
	{
		ValueEntry() { Init(AK_INVALID_UNIQUE_ID, AK_INVALID_GAME_OBJECT); }

		AkUniqueID uGameParamID;
		AkGameObjectID uGameObjectID;
		
		AkMeterMode eMode;
		AkReal32 fValueRMS;
		AkUInt32 uNumRMSVals;
		AkReal32 fValueMax;
		
		void Init(AkUniqueID  in_uGameParamID, AkGameObjectID in_uGameObjectID)
		{
			//Initial values
			uGameParamID = in_uGameParamID;
			uGameObjectID = in_uGameObjectID;
			fValueRMS = 0.f;
			uNumRMSVals = 0;
			fValueMax = -FLT_MAX;
		}

		AkReal32 GetValue()
		{
			AkReal32 fValue = -FLT_MAX;
			if (uNumRMSVals > 0)
			{
				fValue = AkMath::FastLinTodB( fValueRMS / ((AkReal32)uNumRMSVals)) / 2;
			}
			
			fValue = AK_FPMax(fValueMax, fValue);

			return fValue;
		}

		void PushToRTPC( AK::IAkGlobalPluginContext* in_pGlobalContext )
		{
			if ( uGameParamID == AK_INVALID_UNIQUE_ID )
				return;

			AK::IAkGlobalPluginContextEx* pGlobalContextEx = ( AK::IAkGlobalPluginContextEx* )in_pGlobalContext;
			pGlobalContextEx->SetRTPCValueSync( uGameParamID, GetValue(), uGameObjectID );
#ifdef WWISE_AUTHORING
			pGlobalContextEx->SetRTPCValueSync( uGameParamID, GetValue(), AkGameObjectID_Transport );
#endif
		}

		void Accumulate(AkReal32 fValue, AkMeterMode eMode)
		{

			if (eMode == AkMeterMode_RMS)
			{
				AkReal32 fLin = AkMath::dBToLin(fValue);
				fValueRMS += (fLin*fLin);
				uNumRMSVals++;
			}
			else
			{
				fValueMax = AK_FPMax(fValueMax, fValue);
			}
		}
	};

	ValueEntry value;

	for (Meters::IteratorEx it = m_meters.BeginEx(); it != m_meters.End();)
	{
		CAkMeterFX* pMeter = *it;

		AkUniqueID uGameParamID = pMeter->m_uGameParamID;
		if (uGameParamID != AK_INVALID_UNIQUE_ID)
		{
			AkGameObjectID uGameObjectID = pMeter->m_uGameObjectID;

			if (value.uGameParamID != uGameParamID || value.uGameObjectID != uGameObjectID)
			{
				value.PushToRTPC( m_pGlobalContext );
				value.Init(uGameParamID, uGameObjectID);
			}

			AkReal32 fValue = pMeter->m_state.fLastValue;
			if (pMeter->m_bTerminated)
				fValue = pMeter->m_fMin;// WG-18165: force send out min value when terminated

			value.Accumulate(fValue, pMeter->m_eMode);
		}

		if (pMeter->m_bTerminated)
		{
			it = m_meters.Erase(it);
			AK_PLUGIN_DELETE(m_pAllocator, pMeter);
		}
		else
		{
			++it;
		}
	}

	value.PushToRTPC( m_pGlobalContext );

	if ( m_meters.IsEmpty() )
	{
		this->~CAkMeterManager();
		m_pAllocator->Free(this);
	}
}

CAkMeterManager * CAkMeterManager::Instance(AK::IAkPluginMemAlloc * in_pAllocator, AK::IAkGlobalPluginContext * in_pCtx)
{
	if ( !pInstance )
	{
		void * pSpace = in_pAllocator->Malloc(sizeof(CAkMeterManager));
		if (!pSpace)
			return NULL;
		AkPlacementNew(pSpace) CAkMeterManager(in_pAllocator, in_pCtx);
	}

	return pInstance;
}
