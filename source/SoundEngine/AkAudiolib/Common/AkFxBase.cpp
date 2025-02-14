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

#include "stdafx.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include "AkEffectsMgr.h"
#include "AkFxBase.h"
#include "AkFXMemAlloc.h"
#include "AkLEngine.h"
#include "AkRTPCMgr.h"
#include "AkSoundBase.h"

namespace
{
	AkForceInline AkReal32 _BaseValue( AkRtpcAccum in_rtpcAccum )
	{
		if ( in_rtpcAccum == AkRtpcAccum_Multiply )
			return AccumulateMult::BaseValue();
		else
			return AccumulateAdd::BaseValue();
	}

	AkForceInline void _Accumulate( AkRtpcAccum in_rtpcAccum, AkReal32& io_result, AkReal32 in_value )
	{
		if ( in_rtpcAccum == AkRtpcAccum_Multiply )
			AccumulateMult::Accumulate( io_result, in_value );
		else
			AccumulateAdd::Accumulate( io_result, in_value );
	}
};

CAkFxBase::CAkFxBase( AkUniqueID in_ulID )
	: CAkIndexable( in_ulID )
	, m_FXID( AK_INVALID_PLUGINID )
	, m_pParam( NULL )
{
}

CAkFxBase::~CAkFxBase()
{
	AKASSERT( m_instances.IsEmpty() );

	for ( PluginRtpcSubscriptions::Iterator iter = m_rtpcSubscriptions.Begin(); iter != m_rtpcSubscriptions.End(); ++iter )
	{
		(*iter).conversionTable.Unset();
	}

	m_rtpcSubscriptions.Term();
	m_propertyValues.Term();

	m_media.Term();

	// Delete the existing parameter object.
	if( m_pParam )
	{
		m_pParam->Term( AkFXMemAlloc::GetUpper( ) );
	}
}

AKRESULT CAkFxBase::SetInitialValues( AkUInt8* in_pData, AkUInt32 in_uDataSize )
{
	// We don't care about the shareset ID, just skip it
	SKIPBANKDATA( AkUInt32, in_pData, in_uDataSize );

	AkPluginID fxID = READBANKDATA( AkPluginID, in_pData, in_uDataSize );
	AkUInt32 uSize = READBANKDATA( AkUInt32, in_pData, in_uDataSize );

	AKRESULT eResult = AK_Success;

	if( fxID != AK_INVALID_PLUGINID )
		eResult = SetFX( fxID, in_pData, uSize );

	// Skip the plugin data blob

	in_pData += uSize;
	in_uDataSize -= uSize;

	// Read media

	if( eResult == AK_Success )
	{
		AkUInt32 uNumBankData = READBANKDATA( AkUInt8, in_pData, in_uDataSize );
		if ( uNumBankData != 0 )
			eResult = m_media.Reserve( uNumBankData );

		if( eResult == AK_Success )
		{
			for( AkUInt32 i = 0; i < uNumBankData; ++i )
			{
				AkUInt32 index = READBANKDATA( AkUInt8, in_pData, in_uDataSize );
				AkUInt32 sourceId = READBANKDATA( AkUInt32, in_pData, in_uDataSize );
				m_media.Set( index, sourceId );
			}

			// Read RTPC
			AkUInt32 numRtpc = GetNumberOfRTPC( in_pData );
			if ( numRtpc != 0 )
				eResult = m_rtpcSubscriptions.Reserve( numRtpc );

			if( eResult == AK_Success )
			{
				eResult = SetInitialRTPC(in_pData, in_uDataSize, this, false);
				if (eResult == AK_Success)
				{
					// Read state info (ignore two bytes for now)
					eResult = ReadStateChunk( in_pData, in_uDataSize );

					if (eResult == AK_Success)
					{
						// Read initial values for controllable properties
						AkUInt32 numValues = READBANKDATA(AkUInt16, in_pData, in_uDataSize);

						if ( numValues != 0 )
							eResult = m_propertyValues.Reserve( numValues );

						if ( eResult == AK_Success )
						{
							for (AkUInt32 i = 0; i < numValues; ++i)
							{
								const AkPluginPropertyId propertyId = (AkPluginPropertyId)READVARIABLESIZEBANKDATA(AkUInt32, in_pData, in_uDataSize);
								const AkRtpcAccum rtpcAccum = (AkRtpcAccum)READBANKDATA(AkUInt8, in_pData, in_uDataSize);
								const AkReal32 fValue = (AkReal32)READBANKDATA(AkReal32, in_pData, in_uDataSize);
								m_propertyValues.AddLast( PluginPropertyValue( propertyId, rtpcAccum, fValue ) );
							}
						}
					}
				}
			}
		}
	}

	return eResult;
}

void CAkFxBase::SetFX( 
	AkPluginID in_FXID,
	AK::IAkPluginParam * in_pParam
	)
{
	// Delete the existing parameter object.
	if( m_pParam )
	{
		m_pParam->Term( AkFXMemAlloc::GetUpper( ) );
		m_pParam = NULL;
	}

	m_FXID = in_FXID;
	m_pParam = in_pParam;
}

AKRESULT CAkFxBase::SetFX( 
	AkPluginID in_FXID,
	void * in_pParamBlock,
	AkUInt32 in_uParamBlockSize
	)
{
	// Create and init the parameter object for the specified effect.
	AK::IAkPluginParam * pParam = NULL;
	AKRESULT eResult = CAkEffectsMgr::AllocParams( AkFXMemAlloc::GetUpper(), in_FXID, pParam );

	if( eResult != AK_Success || pParam == NULL )
	{
		// let's just keep the fx ID, most likely it was not registered, this will help the error in the log so user will see what was missing.
		m_FXID = in_FXID;

		// Yes success. We don't want a bank to fail loading because an FX is not registered or not working.
		// So we don't set the FX and return, as if there was never any FX.
		// The sound will play without FX.
		return AK_Success;
	}

	eResult = pParam->Init( AkFXMemAlloc::GetUpper(), in_pParamBlock, in_uParamBlockSize );
	if( eResult != AK_Success )
	{
		pParam->Term( AkFXMemAlloc::GetUpper( ) );
		return eResult;
	}

	SetFX( in_FXID, pParam );

	return AK_Success;
}

void CAkFxBase::SetFXParam(
	AkPluginParamID in_uParamID,	// ID of the param to modify, will be done by the plug-in itself
	void * in_pParamBlock,			// Pointer to the Param block
	AkUInt32 in_uParamBlockSize		// BLOB size
	)
{
	AKASSERT( in_pParamBlock );
	if ( m_pParam && in_pParamBlock )
	{
		// Set parameter in master copy of FX params
		m_pParam->SetParam( 
			in_uParamID,
			in_pParamBlock,
			in_uParamBlockSize );

		// Check master FX param init values (in case the parameter has an RTPC and/or State)
		if( in_uParamBlockSize == sizeof(AkReal32) )
		{
			// Parameter may have RTPC and/or State; keep track of value for all instances
			SetPropertyValue( (AkPluginPropertyId)in_uParamID, *(AkReal32*)in_pParamBlock );

			// Notify all running instances of FX param change
			RecalculatePropertyValue( (AkPluginPropertyId)in_uParamID );
		}
		else
		{
			// Push new parameter value to all instances WITHOUT checking RTPCs nor States
			for ( Instances::Iterator instance = m_instances.Begin(); instance != m_instances.End(); ++instance )
				(*instance)->SetPluginParam( (AkPluginPropertyId)in_uParamID, in_pParamBlock, in_uParamBlockSize );
		}
	}
}

AKRESULT CAkFxBase::SetRTPC(
	AkRtpcID			in_rtpcId,
	AkRtpcType			in_rtpcType,
	AkRtpcAccum			in_rtpcAccum,
	AkRTPC_ParameterID	in_paramId,
	AkUniqueID			in_curveId,
	AkCurveScaling		in_scaling,
	AkRTPCGraphPoint*	in_pArrayConversion, // = NULL
	AkUInt32			in_ulConversionArraySize, // = 0
	bool				in_bNotify // = true
	)
{
	AKRESULT eResult = AK_Success;
	UnsetRTPC( in_paramId, in_curveId, false );

	PluginRtpcSubscription * pSubscription = m_rtpcSubscriptions.AddLast();
	if ( pSubscription )
	{
		pSubscription->curveId = in_curveId;
		pSubscription->rtpcId = in_rtpcId;
		pSubscription->propertyId = (AkPluginPropertyId)in_paramId;
		pSubscription->rtpcType = (AkUInt8)in_rtpcType;
		pSubscription->rtpcAccum = (AkUInt8)in_rtpcAccum;

		if( in_pArrayConversion && in_ulConversionArraySize )
		{
			eResult = pSubscription->conversionTable.Set(in_pArrayConversion, in_ulConversionArraySize, in_scaling);
		}

		// Update RTPC accumulation for property
		// We need to know which properties support controllers so NOT to update properties that shouldn't be
		if ( eResult == AK_Success )
		{
			PluginPropertyValues::Iterator iv = m_propertyValues.Begin();
			for ( ; iv != m_propertyValues.End(); ++iv )
			{
				if ( (*iv).propertyId == in_paramId )
				{
					(*iv).rtpcAccum = in_rtpcAccum;
					break;
				}
			}
		}

		// Subscribe each instance to RTPC 
		if ( eResult == AK_Success && in_bNotify )
		{
			for ( Instances::Iterator it = m_instances.Begin(); it != m_instances.End(); ++it )
			{
				PluginRTPCSub* pInstance = *it;

				g_pRTPCMgr->SubscribeRTPC(
					pInstance,
					pSubscription->rtpcId,
					in_rtpcType,
					in_rtpcAccum,
					in_paramId,
					in_curveId,
					in_scaling,
					in_pArrayConversion,
					in_ulConversionArraySize,
					pInstance->rtpcKey,
					CAkRTPCMgr::SubscriberType_Plugin );
			}

			RecalculatePropertyValue( (AkPluginPropertyId)in_paramId );
		}
	}
	else
	{
		eResult = AK_InsufficientMemory;
	}
	return eResult;
}

void CAkFxBase::UnsetRTPC( 
	AkRTPC_ParameterID in_paramId,
	AkUniqueID in_curveId,
	bool in_bNotify
	)
{
	bool bRemoved = false;

	PluginRtpcSubscriptions::Iterator iter = m_rtpcSubscriptions.Begin();
	while( iter != m_rtpcSubscriptions.End() )
	{
		if( (*iter).propertyId == (AkPluginPropertyId)in_paramId && (*iter).curveId == in_curveId )
		{
			(*iter).conversionTable.Unset();
			iter = m_rtpcSubscriptions.Erase( iter );
			bRemoved = true;
		}
		else
		{
			++iter;
		}
	}

	if ( bRemoved && in_bNotify )
	{
		for ( Instances::Iterator it = m_instances.Begin(); it != m_instances.End(); ++it )
		{
			PluginRTPCSub* pInstance = *it;
			g_pRTPCMgr->UnSubscribeRTPC( pInstance, in_paramId, in_curveId );
		}

		RecalculatePropertyValue( (AkPluginPropertyId)in_paramId );
	}
}

AKRESULT CAkFxBase::SetPropertyValue( AkPluginPropertyId in_propertyId, AkReal32 in_fValue )
{
	PluginPropertyValues::Iterator it = m_propertyValues.Begin();
	for( ; it != m_propertyValues.End(); ++it )
	{
		if( (*it).propertyId == in_propertyId )
		{
			(*it).fValue = in_fValue;
			return AK_Success;
		}
	}

	PluginPropertyValue* propertyValue = m_propertyValues.AddLast();
	if ( propertyValue )
	{
		propertyValue->propertyId = in_propertyId;
		propertyValue->rtpcAccum = AkRtpcAccum_None;
		propertyValue->fValue = in_fValue;
	}
	else
	{
		return AK_InsufficientMemory;
	}

	return AK_Success;
}

void CAkFxBase::RecalculatePropertyValues()
{
	// Loop through all instances
	for ( Instances::Iterator instance = m_instances.Begin(); instance != m_instances.End(); ++instance )
	{
		(*instance)->RecalculatePropertyUpdates();
	}
}

void CAkFxBase::RecalculatePropertyValue( AkPluginPropertyId in_propertyId )
{
	// Loop through all instances
	for ( Instances::Iterator instance = m_instances.Begin(); instance != m_instances.End(); ++instance )
	{
		(*instance)->RecalculatePropertyUpdate( in_propertyId );
	}
}

void CAkFxBase::SetMediaID( AkUInt32 in_uIdx, AkUniqueID in_mediaID )
{
	m_media.Set( in_uIdx, in_mediaID );
}

void CAkFxBase::SubscribeRTPC( IAkRTPCSubscriberPlugin* in_pSubscriber, const AkRTPCKey& in_rtpcKey )
{
	for( PluginRtpcSubscriptions::Iterator iter = m_rtpcSubscriptions.Begin(); iter != m_rtpcSubscriptions.End(); ++iter )
	{
		PluginRtpcSubscription& subscription = *iter;

		g_pRTPCMgr->SubscribeRTPC(
			in_pSubscriber,
			subscription.rtpcId,
			(AkRtpcType)subscription.rtpcType,
			(AkRtpcAccum)subscription.rtpcAccum,
			(AkRTPC_ParameterID)subscription.propertyId,
			subscription.curveId,
			subscription.conversionTable.Scaling(),
			&subscription.conversionTable.GetFirstPoint(),
			subscription.conversionTable.Size(),
			in_rtpcKey,
			CAkRTPCMgr::SubscriberType_Plugin
			);
	}
}

void CAkFxBase::UnsubscribeRTPC( IAkRTPCSubscriberPlugin* in_pSubscriber )
{
	for( PluginRtpcSubscriptions::Iterator iter = m_rtpcSubscriptions.Begin(); iter != m_rtpcSubscriptions.End(); ++iter )
	{
		PluginRtpcSubscription& subscription = *iter;

		g_pRTPCMgr->UnSubscribeRTPC( 
			in_pSubscriber,
			(AkRTPC_ParameterID)subscription.propertyId
			);
	}
}

void CAkFxBase::TriggerModulators( const AkModulatorSubscriberInfo& in_subscrInfo, const AkModulatorTriggerParams& in_params, CAkModulatorData* io_pModPBIData )
{
	for( PluginRtpcSubscriptions::Iterator iter = m_rtpcSubscriptions.Begin(); iter != m_rtpcSubscriptions.End(); ++iter )
	{
		PluginRtpcSubscription& subscription = *iter;

		if (subscription.rtpcType == AkRtpcType_Modulator)
		{
			g_pModulatorMgr->Trigger(in_subscrInfo, in_params, io_pModPBIData);
		}
	}
}

void CAkFxBase::AddInstance( PluginRTPCSub* in_instance )
{
	if ( in_instance )
	{
		m_instances.AddFirst( in_instance );
		AddRef();
	}
}

void CAkFxBase::RemoveInstance( PluginRTPCSub* in_instance )
{
	if ( in_instance )
	{
		if ( m_instances.Remove( in_instance ) == AK_Success )
			Release();
	}
}


CAkFxShareSet* CAkFxShareSet::Create( AkUniqueID in_ulID )
{
	CAkFxShareSet* pFx = AkNew( AkMemID_Structure, CAkFxShareSet( in_ulID ) );
	if( pFx )
		g_pIndex->m_idxFxShareSets.SetIDToPtr( pFx );	
	return pFx;
}

AkUInt32 CAkFxShareSet::AddRef() 
{ 
	AkAutoLock<CAkLock> IndexLock( g_pIndex->m_idxFxShareSets.GetLock() ); 
    return ++m_lRef; 
} 

AkUInt32 CAkFxShareSet::Release() 
{ 
	AkAutoLock<CAkLock> IndexLock( g_pIndex->m_idxFxShareSets.GetLock() ); 
    AkInt32 lRef = --m_lRef; 
    AKASSERT( lRef >= 0 ); 
    if ( !lRef ) 
    { 
		g_pIndex->m_idxFxShareSets.RemoveID( ID() );
        AkDelete(AkMemID_Structure, this );
    } 
    return lRef; 
}

CAkAudioDevice::CAkAudioDevice(AkUniqueID in_ulID)
	: CAkFxBase( in_ulID )
{
}

CAkAudioDevice* CAkAudioDevice::Create(AkUniqueID in_ulID)
{
	CAkAudioDevice* pDevice = AkNew(AkMemID_Structure, CAkAudioDevice(in_ulID));
	if (pDevice)
		g_pIndex->m_idxAudioDevices.SetIDToPtr(pDevice);
	return pDevice;
}

AkUInt32 CAkAudioDevice::AddRef()
{
	AkAutoLock<CAkLock> IndexLock(g_pIndex->m_idxAudioDevices.GetLock());
	return ++m_lRef;
}

AkUInt32 CAkAudioDevice::Release()
{
	AkAutoLock<CAkLock> IndexLock(g_pIndex->m_idxAudioDevices.GetLock());
	AkInt32 lRef = --m_lRef;
	AKASSERT(lRef >= 0);
	if (!lRef)
	{
		g_pIndex->m_idxAudioDevices.RemoveID(ID());
		AkDelete(AkMemID_Structure, this);
	}
	return lRef;
}

CAkFxShareSet::CAkFxShareSet(AkUniqueID in_ulID)
: CAkFxBase(in_ulID)
{
}

CAkFxCustom* CAkFxCustom::Create( AkUniqueID in_ulID )
{
	CAkFxCustom* pFx = AkNew(AkMemID_Structure, CAkFxCustom( in_ulID ) );
	if( pFx )
		g_pIndex->m_idxFxCustom.SetIDToPtr( pFx );	
	return pFx;
}

AkUInt32 CAkFxCustom::AddRef() 
{ 
	AkAutoLock<CAkLock> IndexLock( g_pIndex->m_idxFxCustom.GetLock() ); 
    return ++m_lRef; 
} 

AkUInt32 CAkFxCustom::Release() 
{ 
	AkAutoLock<CAkLock> IndexLock( g_pIndex->m_idxFxCustom.GetLock() ); 
    AkInt32 lRef = --m_lRef; 
    AKASSERT( lRef >= 0 ); 
    if ( !lRef ) 
    { 
		g_pIndex->m_idxFxCustom.RemoveID( ID() );
        AkDelete(AkMemID_Structure, this );
    } 
    return lRef; 
}

CAkFxCustom::CAkFxCustom( AkUniqueID in_ulID )
	: CAkFxBase( in_ulID )
{
}



//---------------------------------------------------------------------------------
// PluginRTPCSub


AK::IAkPluginParam* PluginRTPCSub::Clone(CAkFxBase* in_pCloneFrom, CAkBehavioralCtx* in_pCtx)
{
	AKASSERT(in_pCtx != NULL);

	return _Clone( in_pCloneFrom, in_pCtx, in_pCtx->GetRTPCKey(), NULL, true );
}

AK::IAkPluginParam* PluginRTPCSub::Clone(CAkFxBase* in_pCloneFrom, const AkRTPCKey& in_rtpcKey, CAkModulatorData* in_pModulatorData, bool in_bSubscribeRTPC)
{
	// in_bSubscribeRTPC: Some system like the AudioDevices need to delay RTPC registration because the PluginRTPCSub may be on the stack temporarily. 
	return _Clone( in_pCloneFrom, NULL, in_rtpcKey, in_pModulatorData, in_bSubscribeRTPC );
}

AK::IAkPluginParam* PluginRTPCSub::_Clone(CAkFxBase* in_pCloneFrom, CAkBehavioralCtx* in_pCtx, const AkRTPCKey& in_rtpcKey, CAkModulatorData* in_pModulatorData, bool in_bSubscribeRTPC)
{
	AKASSERT( pParam == NULL && pFx == NULL );

	AK::IAkPluginParam* pParamsFrom = in_pCloneFrom->GetFXParam();
	if (pParamsFrom)
	{
		bool bResult = true;

		pParam = pParamsFrom->Clone(AkFXMemAlloc::GetLower());
		pFx = in_pCloneFrom;
		pFx->AddInstance(this);

		// Resize update array to expected size
		propertyUpdates.RemoveAll();
		AkUInt32 expectedSize = GetPropertyUpdateArraySize();
		if ( expectedSize != 0 )
			bResult = bResult && propertyUpdates.Resize( expectedSize );

		if ( bResult && pParam )
		{
			// Copy property values (those that have either RTPC and/or States)
			for ( AkUInt32 i = 0; i < pFx->m_propertyValues.Length(); ++i )
			{
				propertyUpdates[i] = pFx->m_propertyValues[i];
			}

			// Subscribe to the RTPCs; the RTPCMgr will update propertyValues
			// Very important: leave as two seperate calls: this leads to different TriggerModulators
			if (in_pCtx)
				SubscribeRTPC(in_pCtx);
			else if (in_bSubscribeRTPC)
				SubscribeRTPC(in_rtpcKey, in_pModulatorData);

			// Update propertyValues with states
			CalculatePropertyUpdateStates();
		}
		else
		{
			Term();
		}
	}
	return pParam;
}

AkUInt32 PluginRTPCSub::GetPropertyUpdateArraySize() const
{
	// Start with FxBase property (initial) values
	AkUInt32 expectedSize = pFx->m_propertyValues.Length();

	// Add number of non-exclusive RTPCs
	PluginRtpcSubscriptions::Iterator itRtpc = pFx->m_rtpcSubscriptions.Begin();
	for ( ; itRtpc != pFx->m_rtpcSubscriptions.End(); ++itRtpc )
	{
		if ( (*itRtpc).rtpcAccum != AkRtpcAccum_Exclusive )
			++expectedSize;
	}

	// Add state group entries
	StatePropertyArray* stateProperties = pFx->GetStateProperties();
	StateGroupChunkList* stateChunks = pFx->GetStateChunks();
	if ( stateProperties && stateChunks )
	{
		AkUInt32 numStateGroups = 0;

		StateGroupChunkList::Iterator itGroup = stateChunks->Begin();
		for ( ; itGroup != stateChunks->End(); ++itGroup )
			++numStateGroups;

		expectedSize += stateProperties->Length() * numStateGroups;
	}

	return expectedSize;
}

void PluginRTPCSub::SubscribeRTPC(const AkRTPCKey& in_rtpcKey, CAkModulatorData* in_pModulatorData)
{
	if (pFx)
	{
		pFx->SubscribeRTPC(this, in_rtpcKey);
		rtpcKey = in_rtpcKey;
	}
	
	TriggerModulators(in_rtpcKey.GameObj(), in_pModulatorData);
}

void PluginRTPCSub::SubscribeRTPC(CAkBehavioralCtx* in_pCtx)
{
	AKASSERT(in_pCtx != NULL);
	if (pFx)
	{
		pFx->SubscribeRTPC(this, in_pCtx->GetRTPCKey());
		rtpcKey = in_pCtx->GetRTPCKey();
	}
	
	TriggerModulators(in_pCtx);
}

void PluginRTPCSub::UnsubscribeRTPC()
{
	if (pFx)
		pFx->UnsubscribeRTPC(this);
	rtpcKey = AkRTPCKey();
}

void PluginRTPCSub::TriggerModulators(CAkBehavioralCtx* in_pCtx)
{
	if (pFx)
	{
		AkModulatorSubscriberInfo subscrInfo;
		subscrInfo.pSubscriber = this;
		subscrInfo.eSubscriberType = CAkRTPCMgr::SubscriberType_Plugin;

		AkModulatorTriggerParams modParams;
		if( in_pCtx )
		{
			//Set the Voice specific subscrInfo and modulator params
			subscrInfo.pTargetNode = in_pCtx->GetParameterNode();
			in_pCtx->GetModulatorTriggerParams(modParams);
			modParams.uFrameOffset = 0; //For source plug-ins, the offset gets applied after the modulator at the pitch node.
		}
		else
		{
			// Not Voice related (Global)
			subscrInfo.eNarrowestSupportedScope = AkModulatorScope_Global;
		}
		pFx->TriggerModulators(subscrInfo, modParams, in_pCtx ? &(in_pCtx->GetModulatorData()) : NULL);
	}
}

void PluginRTPCSub::TriggerModulators(CAkRegisteredObj* in_pGameObj, CAkModulatorData* in_pModulatorData)
{
	if (pFx)
	{
		AkModulatorSubscriberInfo subscrInfo;
		subscrInfo.pSubscriber = this;
		subscrInfo.eSubscriberType = CAkRTPCMgr::SubscriberType_Plugin;
		subscrInfo.eNarrowestSupportedScope = AkModulatorScope_GameObject;

		AkModulatorTriggerParams modParams;
		modParams.pGameObj = in_pGameObj;

		pFx->TriggerModulators(subscrInfo, modParams, in_pModulatorData);
	}
}

void PluginRTPCSub::UpdateTargetParam( PluginPropertyUpdateType in_updateType, AkUniqueID in_updateId, AkRTPC_ParameterID in_paramId, AkRtpcAccum in_rtpcAccum, AkReal32 in_value )
{
	if ( ! pParam )
		return;

	// Log dummy RTPC value for plug-in properties to send a valid message.
	// Since plug-in RTPCs are not real properties, log AkPropID_NUM instead.
	// Log 0.0f as the delta value since we're only interested in the plug-in RTPC value.
	// The RTPC id and value should have already been logged above with AkDeltaMonitor::LogDriver.
#ifndef AK_OPTIMIZED
	AkDeltaMonitor::LogUpdate(AkPropID_NUM, 0.0f, GetContextID(), GetContextPipelineID());
#endif

	if ( in_rtpcAccum == AkRtpcAccum_Exclusive )
	{
		// Simply set parameter in plugin instance
		pParam->SetParam( static_cast<AkPluginPropertyId>(in_paramId), &in_value, sizeof(in_value) );
	}
	else
	{
		AkReal32 newValue = _BaseValue( in_rtpcAccum );

		// Assume we won't find an entry for this update
		PluginPropertyUpdates::Iterator itValue = propertyUpdates.End();

		// Look for all update values
		PluginPropertyUpdates::Iterator itSearch = propertyUpdates.Begin();
		for ( ; itSearch != propertyUpdates.End(); ++itSearch )
		{
			PluginPropertyUpdate& update = *itSearch;
			if ( update.propertyId == in_paramId )
			{
				// Do we need to update value
				if ( update.updateType == in_updateType && update.updateId == in_updateId )
				{
					update.fValue = in_value;
					itValue = itSearch;
				}

				// Need to accumulate value
				if ( in_rtpcAccum != AkRtpcAccum_Boolean || (*itSearch).updateType != PluginPropertyUpdateType_FxBase )
					_Accumulate( in_rtpcAccum, newValue, update.fValue );
			}
		}

		// If we didn't find an entry then add it now
		if ( itValue == propertyUpdates.End() )
		{
			PluginPropertyUpdate* newUpdate = propertyUpdates.AddLast(
				PluginPropertyUpdate( in_updateType, in_updateId, in_paramId, in_value )
				);
			if ( newUpdate )
				_Accumulate( in_rtpcAccum, newValue, in_value );
		}

		pParam->SetParam( static_cast<AkPluginParamID>(in_paramId), &newValue, sizeof(newValue) );
	}
}

void PluginRTPCSub::RecalculatePropertyUpdates()
{
	// Remove all update entries
	propertyUpdates.RemoveAll();

	// Call recalc for each property
	if ( pFx )
	{
		PluginPropertyValues::Iterator iv = pFx->m_propertyValues.Begin();
		for ( ; iv != pFx->m_propertyValues.End(); ++iv )
		{
			// Only update properties that support controllers
			if ( (AkRtpcAccum)(*iv).rtpcAccum != AkRtpcAccum_None )
			{
				_RecalculatePropertyUpdate( (*iv).propertyId, (*iv).fValue );
			}
		}
	}
}

void PluginRTPCSub::RecalculatePropertyUpdate( AkPluginPropertyId in_propertyId )
{
	// Erase all entries propertyUpdates
	PluginPropertyUpdates::Iterator itUpdate = propertyUpdates.Begin();
	for ( ; itUpdate != propertyUpdates.End(); )
	{
		if ( (*itUpdate).propertyId == in_propertyId )
			itUpdate = propertyUpdates.Erase( itUpdate );
		else
			++itUpdate;
	}

	const PluginPropertyValue search( in_propertyId );
	PluginPropertyValues::Iterator itFx = pFx->m_propertyValues.FindEx( PluginPropertyValue( in_propertyId ) );
	if ( itFx != pFx->m_propertyValues.End() )
		_RecalculatePropertyUpdate( in_propertyId, (*itFx).fValue );
}

void PluginRTPCSub::_RecalculatePropertyUpdate( AkPluginPropertyId in_propertyId, AkReal32 in_fxValue )
{
	// All propertyUpdate entries have been erased by calling function

	// Starting with value set for parameter
	AkReal32 newValue = in_fxValue;

	// Set to invalid type; we'll know if we have an Rtpc and/or State
	AkRtpcAccum rtpcAccum = AkRtpcAccum_None;

	// Update state properties
	RecalculatePropertyUpdateStates( (AkPluginPropertyId)in_propertyId, newValue, rtpcAccum );

	// Update with rtpc values
	RecalculatePropertyUpdateRtpc( (AkPluginPropertyId)in_propertyId, newValue, rtpcAccum );

	// Boolean properties do not use property's initial value
	if ( rtpcAccum == AkRtpcAccum_Boolean )
		newValue -= in_fxValue;
	else
		AKVERIFY( propertyUpdates.AddLast( PluginPropertyUpdate( PluginPropertyUpdateType_FxBase, AK_INVALID_UNIQUE_ID, in_propertyId, in_fxValue ) ) );

	pParam->SetParam( (AkPluginParamID)in_propertyId, &newValue, sizeof(newValue) );
}

void PluginRTPCSub::CalculatePropertyUpdateStates()
{
	StatePropertyArray* stateProperties = pFx->GetStateProperties();
	if ( stateProperties )
	{
		// Remember the size of propertyUpdates, because we will only be adding to the end
		AkUInt32 length = propertyUpdates.Length();

		for ( StatePropertyArray::Iterator state = stateProperties->Begin(); state != stateProperties->End(); ++state )
		{
			AkStatePropertyId propertyId = (*state).propertyId;
			AkRtpcAccum rtpcAccum = (AkRtpcAccum)(*state).accumType;
			bool hasController = false;

			AkReal32 newValue = _BaseValue( rtpcAccum );
			AkReal32 fxBaseValue = _BaseValue( rtpcAccum );

			// Get value of property before we update with states
			for ( AkUInt32 i = 0; i < length; ++i )
			{
				const PluginPropertyUpdate& update = propertyUpdates[i];
				if ( update.propertyId == propertyId )
				{
					_Accumulate( rtpcAccum, newValue, update.fValue );

					if ( update.updateType != PluginPropertyUpdateType_FxBase )
						hasController = true;
					else
						fxBaseValue = update.fValue;
				}
			}

			// Add to propertyUpdates for each state group and update value
			{
				bool stateController = GetPropertyStateValues( propertyId, rtpcAccum, newValue );
				hasController = hasController || stateController;
			}

			// A boolean property does not use its value when there are controllers
			if ( rtpcAccum == AkRtpcAccum_Boolean && hasController )
				newValue -= fxBaseValue;

			// Update the plugin parameter
			pParam->SetParam( (AkPluginParamID)propertyId, &newValue, sizeof(newValue) );
		}
	}
}

void PluginRTPCSub::RecalculatePropertyUpdateStates( AkPluginPropertyId in_propertyId, AkReal32& io_value, AkRtpcAccum& out_rtpcAccum )
{
	StatePropertyArray* stateProperties = pFx->GetStateProperties();
	if ( stateProperties )
	{
		for ( StatePropertyArray::Iterator state = stateProperties->Begin(); state != stateProperties->End(); ++state )
		{
			if ( (*state).propertyId == in_propertyId )
			{
				AkRtpcAccum rtpcAccum = (AkRtpcAccum)(*state).accumType;
				if ( GetPropertyStateValues( in_propertyId, rtpcAccum, io_value ) )
					out_rtpcAccum = rtpcAccum;
				break;
			}
		}
	}
}

void PluginRTPCSub::StateAwareParamValue( AkStatePropertyId in_propertyId, AkStateGroupID in_stateGroup, AkReal32 in_value )
{
	PluginPropertyUpdate* propertyUpdate = propertyUpdates.AddLast(
		PluginPropertyUpdate( PluginPropertyUpdateType_State, (AkUniqueID)in_stateGroup, in_propertyId, in_value )
		);
	AKASSERT( propertyUpdate );
}

bool PluginRTPCSub::GetPropertyStateValues( AkStatePropertyId in_propertyId, AkRtpcAccum in_rtpcAccum, AkReal32& io_value )
{
	if ( in_rtpcAccum == AkRtpcAccum_Multiply )
		return pFx->GetStateParamValues<PluginRTPCSub,AccumulateMult>( *this, in_propertyId, io_value );
	else
		return pFx->GetStateParamValues<PluginRTPCSub,AccumulateAdd>( *this, in_propertyId, io_value );
}

void PluginRTPCSub::RecalculatePropertyUpdateRtpc( AkPluginPropertyId in_propertyId, AkReal32& io_value, AkRtpcAccum& out_rtpcAccum )
{
	if ( pFx )
	{
		PluginRtpcSubscriptions::Iterator subscription = pFx->m_rtpcSubscriptions.Begin();
		for ( ; subscription != pFx->m_rtpcSubscriptions.End(); ++subscription )
		{
			if ( (*subscription).propertyId == in_propertyId )
			{
				out_rtpcAccum = (AkRtpcAccum)(*subscription).rtpcAccum;

				AkReal32 fValue = g_pRTPCMgr->GetRTPCConvertedValue( this, in_propertyId, rtpcKey );

				PluginPropertyUpdate* propertyUpdate = propertyUpdates.AddLast(
					PluginPropertyUpdate( PluginPropertyUpdateType_Rtpc, (AkUniqueID)(*subscription).rtpcId, in_propertyId, fValue )
					);
				AKASSERT( propertyUpdate );
				if ( propertyUpdate )
					_Accumulate( out_rtpcAccum, io_value, fValue );
			}
		}
	}
}

#ifndef AK_OPTIMIZED
void PluginRTPCSub::RecapRTPCParams()
{
	if (pFx)
	{
		PluginRtpcSubscriptions::Iterator subscription = pFx->m_rtpcSubscriptions.Begin();
		for (; subscription != pFx->m_rtpcSubscriptions.End(); ++subscription)
		{
			PluginRtpcSubscription* sub = subscription.pItem;

			AkRTPCKey key = rtpcKey;
			AkReal32 value = 0.0f;
			bool isAutomatedParam = false;
			g_pRTPCMgr->GetRTPCValue<CurrentValue>(sub->rtpcId, (AkRTPC_ParameterID)sub->propertyId, CAkRTPCMgr::SubscriberType_Plugin, key, value, isAutomatedParam);

			// todo: can we know if this RTPC is global or on a game object?
			AkDeltaMonitor::LogPluginRTPC(sub->rtpcId, value, rtpcKey.GameObj() != NULL ? AkDelta_RTPC : AkDelta_RTPCGlobal);
		}
	}
}

void PluginRTPCSub::UpdateContextID(AkUniqueID in_contextId, AkUniqueID in_pipelineId)
{
	m_contextId = in_contextId;
	m_contextPipelineId = in_pipelineId;
}

AkUniqueID PluginRTPCSub::GetContextID()
{
	return m_contextId;
}

AkUniqueID PluginRTPCSub::GetContextPipelineID()
{
	return m_contextPipelineId;
}
#endif

void PluginRTPCSub::SetPluginParam( AkPluginPropertyId in_propertyId, void * in_pParamBlock, AkUInt32 in_uParamBlockSize ) const
{
	if( pParam )
	{
		pParam->SetParam( static_cast<AkPluginParamID>(in_propertyId), in_pParamBlock, in_uParamBlockSize );
	}
}

AkPluginID PluginRTPCSub::GetPluginID()
{
	return pFx ? pFx->GetFXID() : AK_INVALID_UNIQUE_ID;
}

void PluginRTPCSub::Term()
{
	UnsubscribeRTPC();

	if (pParam)
	{
		pParam->Term(AkFXMemAlloc::GetLower());
		pParam = NULL;
	}

	if (pFx)
	{
		pFx->RemoveInstance( this );
		pFx = NULL;
	}

	propertyUpdates.Term();
}

PluginRTPCSub& PluginRTPCSub::operator=( const PluginRTPCSub& )
{
	AKASSERT( false && "PluginRTPCSub cannot use assignment operator!" );
	return *this;
}

void PluginRTPCSub::TakeFrom( PluginRTPCSub& in_from )
{
	// Term in case we had something of value
	Term();

	// Copy over pointers
	pParam = in_from.pParam;
	pFx = in_from.pFx;

#ifndef AK_OPTIMIZED
	m_contextId = in_from.m_contextId;
	m_contextPipelineId = in_from.m_contextPipelineId;
#endif

	// Register this instance
	if ( pFx )
		pFx->AddInstance( this );

	// Clear Param so it's not destroyed by in_from
	in_from.pParam = NULL;

	// Term the other instance
	in_from.Term();
}

void AkSinkPluginParams::Term()
{
	if (pSink)
	{
		pSink->Term(AkFXMemAlloc::GetLower());
		pSink = NULL;
	}

	PluginRTPCSub::Term();
}

AkSinkPluginParams& AkSinkPluginParams::operator=( const AkSinkPluginParams& )
{
	AKASSERT( false && "AkSinkPluginParams cannot use assignment operator!" );
	return *this;
}

void AkSinkPluginParams::TakeFrom( AkSinkPluginParams& in_from )
{
	// Copy the sink first; the Takefrom will term in_from
	AK::IAkSinkPlugin* copy = in_from.pSink;

	in_from.pSink = NULL;

	// Base class will copy what it needs then Term in_from
	PluginRTPCSub::TakeFrom( in_from );

	// Keep other's sink
	pSink = copy;
}
