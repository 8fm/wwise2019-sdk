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

//////////////////////////////////////////////////////////////////////
//
// AkModulator.cpp
//
// creator : nharris
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#define DEFINE_MODULATOR_PROP_DEFAULTS

#include "AkModulator.h"
#include "AkModulatorMgr.h"
#include "AkModulatorEngine.h"
#include "AkAudioLibIndex.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include "AkParameterNodeBase.h"
#include "AkModulatorCtx.h"
#include <float.h>

//#include "AkAudioLibTimer.h"

//AK_PROFILE_DECLARE_DOMAIN(D);

struct ModCtx_SetParamStruct
{
	ModCtx_SetParamStruct(AkRTPC_ModulatorParamID in_eParam, AkReal32 in_fValue) : eParam(in_eParam), fValue(in_fValue) {}
	AkRTPC_ModulatorParamID eParam;
	AkReal32 fValue;
};

void CAkModulator::ModCtx_SetParam( AkModTreeValue& in_val, const AkRTPCKey& in_rtpcKey, void* in_cookie )
{
	CAkModulatorCtx* pCtx = in_val.pCtx;
	AKASSERT(pCtx);
	ModCtx_SetParamStruct* sps = (ModCtx_SetParamStruct*)(in_cookie);
	AKASSERT(sps != NULL);

	pCtx->SetParam(sps->eParam, sps->fValue);
}

bool CAkModulator::ModCtx_MarkFinishedAndRelease( AkModTreeValue& in_val, const AkRTPCKey& in_rtpcKey, void* in_cookie )
{
	CAkModulatorCtx* pCtx = in_val.pCtx;
	AKASSERT(pCtx);
	pCtx->MarkFinished();
	pCtx->Release();
	return true; //Erase 
}

bool CAkModulator::ModCtx_NotifySubscribers( AkModTreeValue& in_val, const AkRTPCKey& in_rtpcKey, void* in_cookie )
{
	CAkModulatorCtx* pCtx = in_val.pCtx;
	AKASSERT(pCtx);
	CAkModulator* pMod = (CAkModulator*)(in_cookie);
	AKASSERT(pMod != NULL);

	AkRTPCExceptionChecker_<AkModulatorCtxTree> exChecker(in_rtpcKey, pMod->m_mapCtxs);

	AkDeltaMonitor::OpenMultiTargetBrace(in_rtpcKey.GameObj() != NULL ? AkDelta_Modulator : AkDelta_ModulatorGlobal, pMod->key, pCtx->GetCurrentValue());
	for ( CAkRTPCMgr::AkRTPCSubscriptions::Iterator it = pMod->m_subscriptions.Begin(), 
		itEnd = pMod->m_subscriptions.End(); 
		it != itEnd; ++it )
	{
		CAkRTPCMgr::AkRTPCSubscription& subscr = **it;
		if (!(IsAutomatedParam((AkRTPC_ParameterID)subscr.key.ParamID, subscr.eType) && pMod->SupportsAutomatedParams()) &&
			(subscr.eType != CAkRTPCMgr::SubscriberType_CAkParameterNodeBase || reinterpret_cast<CAkRTPCSubscriberNode*>(subscr.key.pSubscriber)->HasActiveTargets()))
		{			
			subscr.PushUpdate(pMod->key, pCtx->GetPreviousValue(), pCtx->GetCurrentValue(), pCtx->GetRTPCKey(), &exChecker, NULL );			
		}
	}
	AkDeltaMonitor::CloseMultiTargetBrace();

	if (pCtx->IsFinished())
	{
		pCtx->Release();
		return true;
	}
	else
	{
		return false;
	}
}

CAkModulator* CAkModulator::Create( AkUniqueID in_ulID, AkModulatorType in_eType )
{
	CAkModulator* pModulator = NULL;
	switch(in_eType)
	{
	case AkModulatorType_LFO:
		pModulator = AkNew(AkMemID_Structure, CAkLFOModulator( in_ulID ) );
		break;
	case AkModulatorType_Envelope:
		pModulator = AkNew(AkMemID_Structure, CAkEnvelopeModulator( in_ulID ) );
		break;
	case AkModulatorType_Time:
		pModulator = AkNew(AkMemID_Structure, CAkTimeModulator( in_ulID ) );
		break;
	default:
		AKASSERT(false && "Unknown modulator type.");
	}
	
	if( pModulator )
	{
		if( pModulator->Init() != AK_Success )
		{
			pModulator->Release();
			pModulator = NULL;
		}
	}

	return pModulator;
}

CAkModulator::CAkModulator( AkUniqueID in_ulID )
: CAkIndexable( in_ulID )
, m_eType( AkModulatorType_Unknown ), m_bAutomatedParamSubscribed(false)
{}

CAkModulator::~CAkModulator()
{
	SCOPE_CHECK_TREE( m_mapCtxs, AkModTreeValue );

	for ( AkUInt32 iBit = 0; !m_RTPCBitArray.IsEmpty(); ++iBit )
	{
		if ( m_RTPCBitArray.IsSet( iBit ) )
		{
			 
			g_pRTPCMgr->UnSubscribeRTPC( this, iBit + RTPC_ModulatorRTPCIDStart );
			m_RTPCBitArray.UnsetBit( iBit );
		}
	}

	m_mapCtxs.ForEachEx( &ModCtx_MarkFinishedAndRelease, NULL );
	g_pModulatorMgr->CleanUpFinishedCtxs(); //<- force clean up of the ctxs, that have a pointer to us
	m_mapCtxs.Term();
	m_subscriptions.Term();
}

AKRESULT CAkModulator::Init()
{ 
	AddToIndex(); 
	return AK_Success; 
}

void CAkModulator::AddToIndex()
{
	AKASSERT(g_pIndex);
	g_pIndex->m_idxModulators.SetIDToPtr( this );
}

void CAkModulator::RemoveFromIndex()
{
	AKASSERT(g_pIndex);
	g_pIndex->m_idxModulators.RemoveID( ID() );
}

AKRESULT CAkModulator::SetInitialValues(AkUInt8* in_pData, AkUInt32 in_ulDataSize)
{
	AKRESULT eResult = AK_Success;

	//Read ID
	// We don't care about the ID, just skip it
	SKIPBANKDATA( AkUInt32, in_pData, in_ulDataSize );

	eResult = m_props.SetInitialParams( in_pData, in_ulDataSize );
	if ( eResult != AK_Success )
		return AK_Fail;

	eResult = m_ranges.SetInitialParams( in_pData, in_ulDataSize );
	if ( eResult != AK_Success )
		return AK_Fail;

	eResult = SetInitialRTPC(in_pData, in_ulDataSize, this);

	CHECKBANKDATASIZE( in_ulDataSize, eResult );

	return eResult;
}

AKRESULT CAkModulator::SetRTPC(
					   AkRtpcID					in_RTPC_ID,
					   AkRtpcType				in_RTPCType,
					   AkRtpcAccum				in_RTPCAccum,
					   AkRTPC_ParameterID		in_ParamID,
					   AkUniqueID				in_RTPCCurveID,
					   AkCurveScaling			in_eScaling,
					   AkRTPCGraphPoint*		in_pArrayConversion,		// NULL if none
					   AkUInt32					in_ulConversionArraySize,	// 0 if none
					   bool						/*in_bNotify*/
					   )
{
	AKASSERT(g_pRTPCMgr);
	AKASSERT((unsigned int)in_ParamID >= RTPC_ModulatorRTPCIDStart);

	// Remember that there is RTPC on this param
	m_RTPCBitArray.SetBit( in_ParamID - RTPC_ModulatorRTPCIDStart );

	return g_pRTPCMgr->SubscribeRTPC( 
		this,
		in_RTPC_ID, 
		in_RTPCType,
		in_RTPCAccum,
		in_ParamID,
		in_RTPCCurveID,
		in_eScaling,
		in_pArrayConversion, 
		in_ulConversionArraySize,
		AkRTPCKey(),  // Get notified for all Game Objects
		CAkRTPCMgr::SubscriberType_Modulator
		);
}

void CAkModulator::UnsetRTPC(
						   AkRTPC_ParameterID		in_ParamID,
						   AkUniqueID				in_RTPCCurveID
						   )
{
	AKASSERT(g_pRTPCMgr);
	AKASSERT((unsigned int)in_ParamID >= RTPC_ModulatorRTPCIDStart);

	bool bMoreCurvesRemaining = false;

	if( g_pRTPCMgr )
	{
		g_pRTPCMgr->UnSubscribeRTPC(
			this,
			in_ParamID,
			in_RTPCCurveID,
			&bMoreCurvesRemaining
			);
	}

	if ( ! bMoreCurvesRemaining )
		m_RTPCBitArray.UnsetBit( in_ParamID - RTPC_ModulatorRTPCIDStart );
}

void CAkModulator::SetParamFromRTPCMgr(AkRTPC_ParameterID in_eParam, AkReal32 in_fValue, const AkRTPCKey& in_rtpcKey )
{
	ModCtx_SetParamStruct sps((AkRTPC_ModulatorParamID)in_eParam, in_fValue);
	m_mapCtxs.ForEachMatching( ModCtx_SetParam, KeyForScope(GetScope(),in_rtpcKey), (void*)&sps );
	SCOPE_CHECK_TREE( m_mapCtxs, AkModTreeValue );
}

void CAkModulator::SetAkProp( AkModulatorPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax )
{
	bool bChanged = false;
	AkPropValue fProp = m_props.GetAkProp( in_eProp, g_AkModulatorPropDefault[in_eProp] );
	if( fProp.fValue != in_fValue )
	{
		m_props.SetAkProp( in_eProp, in_fValue );
		bChanged = true;
	}

	if( in_fMin || in_fMax || m_ranges.FindProp( in_eProp ) )
	{
		RANGED_MODIFIERS<AkPropValue> range = m_ranges.GetAkProp(in_eProp, RANGED_MODIFIERS<AkPropValue>());
		bChanged |= range.m_min.fValue != in_fMin || range.m_max.fValue != in_fMax;

		range.m_min.fValue = in_fMin;
		range.m_max.fValue = in_fMax;

		m_ranges.SetAkProp( in_eProp, range );
	}

	if (bChanged)
	{
		ModCtx_SetParamStruct sps(g_AkModulatorPropRTPCID[in_eProp], in_fValue);
		m_mapCtxs.ForEach(&ModCtx_SetParam, (void*)&sps);
	}
}

void CAkModulator::SetAkProp( AkModulatorPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax )
{
	AkPropValue iProp = m_props.GetAkProp( in_eProp, g_AkModulatorPropDefault[in_eProp] );
	if( iProp.iValue != in_iValue )
	{
		m_props.SetAkProp( in_eProp, in_iValue );

		if (in_eProp == AkModulatorPropID_Scope)
		{
			//If the modulator scope changes, remove the old modulators.
			m_mapCtxs.ForEachEx( &ModCtx_MarkFinishedAndRelease, NULL );
		}
		else
		{
			ModCtx_SetParamStruct sps( g_AkModulatorPropRTPCID[ in_eProp ], (AkReal32)in_iValue);
			m_mapCtxs.ForEach( &ModCtx_SetParam, (void*)&sps );
		}
	}

	if (in_iMin || in_iMax || m_ranges.FindProp(in_eProp))
	{
		RANGED_MODIFIERS<AkPropValue> range;
		range.m_min.iValue = in_iMin;
		range.m_max.iValue = in_iMax;

		m_ranges.SetAkProp(in_eProp, range);
	}
}

AKRESULT CAkModulator::Trigger(const AkModulatorSubscriberInfo& in_subscrInfo, const AkModulatorTriggerParams& in_params, CAkModulatorEngine& in_modEng, CAkModulatorData* io_pModPBIData)
{
	AKRESULT res = AK_Fail;
	
	bool bAllocatedNewCtx = false;
	
	AkModulatorScope eScope = AkMax(in_subscrInfo.eNarrowestSupportedScope, GetScope()); //Take the wider scope of the two.
	AkRTPCKey key = KeyForScope( eScope, in_params.GetRTPCKey() );

	AkModTreeValue* pModVal = m_mapCtxs.Set(key);
	if (pModVal != NULL)
	{
		CAkModulatorCtx* pCtx = _Trigger(pModVal->pCtx, in_params, in_subscrInfo.pTargetNode, eScope, bAllocatedNewCtx);
		if (pCtx)
		{
			if (pModVal->pCtx != pCtx)
			{
				pCtx->AddRef();

				if (pModVal->pCtx != NULL)
					(pModVal->pCtx)->Release();

				pModVal->pCtx = pCtx;
			}
			
			if (io_pModPBIData)
			{
				res = io_pModPBIData->AddModulationSource(pCtx, in_params, in_subscrInfo);
				if (res == AK_Success)
					io_pModPBIData->ReleaseOnTrigger();
			}
			else
				res = AK_Success;
		}
		else if (pModVal->pCtx == NULL)
		{
			m_mapCtxs.Unset(key);
		}

		if (res == AK_Success && bAllocatedNewCtx)
		{
			AKASSERT(pCtx != NULL);
			
			if ( CAkScopedRtpcObj::AddedNewModulatorCtx(GetModulatorID(), key) == AK_Success )
				in_modEng.AddContext(pCtx);
			else
				RemoveCtxsMatchingKey(key);
		}

		
#if !defined(AK_OPTIMIZED)
		if (pCtx != NULL && pCtx->GetScope() == AkModulatorScope_Global)
		{
			if (GetScope() == AkModulatorScope_Note)
			{
				MONITOR_ERROREX( AK::Monitor::ErrorCode_ModulatorScopeError_Inst, in_params.playingId, ((in_params.pGameObj != NULL) ? in_params.pGameObj->ID() : AK_INVALID_GAME_OBJECT) , ID(), false );
			}
			else if (GetScope() == AkModulatorScope_GameObject)
			{
				MONITOR_ERROREX( AK::Monitor::ErrorCode_ModulatorScopeError_Obj, in_params.playingId, ((in_params.pGameObj != NULL) ? in_params.pGameObj->ID() : AK_INVALID_GAME_OBJECT) , ID(), false );
			}
		}
#endif

	}



	return res;
}

AKRESULT CAkModulator::TriggerRelease(const AkModulatorTriggerParams& in_params)
{
	AkRTPCKey key = KeyForScope( GetScope(), in_params.GetRTPCKey() );
	CAkModulatorCtx* pCtx = FindCtx( key );
	if (pCtx)
	{
		pCtx->TriggerRelease(in_params.uFrameOffset);
		return AK_Success;
	}
	return AK_Fail;
}


CAkModulatorCtx* CAkModulator::_Trigger(CAkModulatorCtx* in_pExistingCtx, const AkModulatorTriggerParams& in_params, CAkParameterNodeBase* in_pTargetNode, AkModulatorScope in_eScope, bool& out_bAllocatedCtx)
{
	out_bAllocatedCtx = false;
	CAkModulatorCtx* pNewCtx = NULL;

	//Determine if we can trigger on the MIDI event (if any)
	if (!IsTriggerOk( in_params ))
		return NULL;

	//Only allocate a new one if necessary
	if (in_pExistingCtx != NULL)
	{
		if ( ShouldRetrigger() )
		{
			if( in_params.eTriggerMode == AkModulatorTriggerParams::TriggerMode_FirstPlay ||
				in_params.eTriggerMode == AkModulatorTriggerParams::TriggerMode_EndOfNoteOn )
			{
				in_pExistingCtx->Trigger( this, in_params, in_pTargetNode, in_eScope );
			}
		}
		// Even if in_pExistingCtx is not-null, it may be terminated, so only set the newCtx if it is still alive
		if (!in_pExistingCtx->IsTerminated())
		{
			pNewCtx = in_pExistingCtx;
		}
	}
	
	if (pNewCtx == NULL &&
		(in_eScope == AkModulatorScope_Voice || //Voice-scoped modulators must be triggered on both first and subsequent plays of continuous containers.
		in_params.eTriggerMode != AkModulatorTriggerParams::TriggerMode_SubsequentPlay ))
	{
		switch (GetType())
		{
		case AkModulatorType_LFO:

			pNewCtx = AkNew(AkMemID_Object, CAkLFOCtx() );
			break;

		case AkModulatorType_Envelope:

			pNewCtx = AkNew(AkMemID_Object, CAkEnvelopeCtx() );
			break;

		case AkModulatorType_Time:

			pNewCtx = AkNew(AkMemID_Object, CAkTimeModCtx());
			break;

		default:
			AKASSERT(false);
		}
		
		if (pNewCtx)
		{
			pNewCtx->Trigger(this, in_params, in_pTargetNode, in_eScope);
			out_bAllocatedCtx = true;
		}
	}

	return pNewCtx;
}


bool CAkModulator::StopWhenFinished() const
{
	return m_eType == AkModulatorType_LFO ? false :
		m_props.GetAkProp(AkModulatorPropID_Envelope_StopPlayback, g_AkModulatorPropDefault[AkModulatorPropID_Envelope_StopPlayback]).iValue != 0;
}

AkModulatorScope CAkModulator::GetScope() const
{
	return (AkModulatorScope) m_props.GetAkProp(AkModulatorPropID_Scope, g_AkModulatorPropDefault[AkModulatorPropID_Scope]).iValue;
}

bool CAkModulator::RemoveCtxsMatchingKey( const AkRTPCKey& in_key )
{
	m_mapCtxs.ForEachMatchingEx( &ModCtx_MarkFinishedAndRelease, in_key, NULL );
	return true;
}


AkRTPCKey CAkModulator::KeyForScope( AkModulatorScope in_eScope, const AkRTPCKey& in_rtpcKey )
{
	AkRTPCKey out_Key(in_rtpcKey);
	
	if (in_eScope >= AkModulatorScope_Note)
	{
		// Note scope: set pbi to NULL to match all pbis
		//
		out_Key.PBI() = NULL;
		
		// For Note Scope: If there is a valid MIDI note number, ignore the playing id.
		if( in_rtpcKey.MidiNoteNo() != AK_INVALID_MIDI_NOTE )
			out_Key.PlayingID() = AK_INVALID_UNIQUE_ID;

		if (in_eScope >= AkModulatorScope_GameObject)
		{
			// Game object scope: set the midi note/channel and playing id to invalid
			//
			out_Key.MidiNoteNo() = AK_INVALID_MIDI_NOTE;
			out_Key.MidiChannelNo() = AK_INVALID_MIDI_CHANNEL;
			out_Key.MidiTargetID() = AK_INVALID_UNIQUE_ID;
			out_Key.PlayingID() = AK_INVALID_PLAYING_ID;

			if (in_eScope >= AkModulatorScope_Global)
			{
				// Global scope: All keys are set to invalid.
				//
				out_Key.GameObj() = NULL;
			}
		}
	}

	return out_Key;
}

bool CAkModulator::GetCurrentValue( AkRTPCKey& io_rtpcKey, AkReal32& out_value ) 
{
	AkRTPCKey key = KeyForScope( GetScope(), io_rtpcKey );
	CAkModulatorCtx * pCtx = FindCtx( key );
	if ( pCtx )
	{
		io_rtpcKey = key;
		out_value = pCtx->GetCurrentValue();
		return true;
	}
	return false;
}

bool CAkModulator::GetPreviousValue( AkRTPCKey& io_rtpcKey, AkReal32& out_value ) 
{
	AkRTPCKey key = KeyForScope( GetScope(), io_rtpcKey );
	CAkModulatorCtx * pCtx = FindCtx( key );
	if ( pCtx )
	{
		io_rtpcKey = key;
		out_value = pCtx->GetPreviousValue();
		return true;
	}
	return false;
}

void CAkModulator::NotifySubscribers()
{
//	AK_PROFILE_SCOPE( D, "CAkModulator::NotifySubscribers" );
	m_mapCtxs.ForEachEx( ModCtx_NotifySubscribers, (void*)this );
	SCOPE_CHECK_TREE( m_mapCtxs, AkModTreeValue );
}

void CAkLFOModulator::GetInitialParams( AkModulatorParams* out_pParams, const CAkModulatorCtx* in_pModCtx  )
{
	AkLFOParams* pParams = static_cast<AkLFOParams*>(out_pParams);

	AkRTPCKey rtpcKey = in_pModCtx->GetRTPCKey();

	GetPropAndRTPC( pParams->m_fDepth, AkModulatorPropID_Lfo_Depth, rtpcKey, g_AkModulatorPropDefault[AkModulatorPropID_Lfo_Depth].fValue );
	ApplyRange(AkModulatorPropID_Lfo_Depth, pParams->m_fDepth, 0.0f, 100.0f);
	pParams->m_fDepth /= 100.f; //Convert from percent.

	GetPropAndRTPC( (AkInt32&)(pParams->m_DspParams.eWaveform), AkModulatorPropID_Lfo_Waveform, rtpcKey, g_AkModulatorPropDefault[AkModulatorPropID_Lfo_Waveform].iValue );
	AKASSERT( pParams->m_DspParams.eWaveform >= DSP::LFO::WAVEFORM_FIRST && pParams->m_DspParams.eWaveform < DSP::LFO::WAVEFORM_NUM );
	
	GetPropAndRTPC( pParams->m_DspParams.fFrequency, AkModulatorPropID_Lfo_Frequency, rtpcKey, g_AkModulatorPropDefault[AkModulatorPropID_Lfo_Frequency].fValue );
	ApplyRange(AkModulatorPropID_Lfo_Frequency, pParams->m_DspParams.fFrequency, 0.f, 20000.f);

	GetPropAndRTPC( pParams->m_DspParams.fPWM, AkModulatorPropID_Lfo_PWM, rtpcKey, g_AkModulatorPropDefault[AkModulatorPropID_Lfo_PWM].fValue );
	ApplyRange(AkModulatorPropID_Lfo_PWM, pParams->m_DspParams.fPWM, 0.f, 100.f);
	pParams->m_DspParams.fPWM /= 100.f; //Convert from percent.

	GetPropAndRTPC( pParams->m_DspParams.fSmooth, AkModulatorPropID_Lfo_Smoothing, rtpcKey, g_AkModulatorPropDefault[AkModulatorPropID_Lfo_Smoothing].fValue );
	ApplyRange(AkModulatorPropID_Lfo_Smoothing, pParams->m_DspParams.fSmooth, 0.f, 100.f);
	pParams->m_DspParams.fSmooth /= 100.f; //Convert from percent.

	AkReal32 fAttack;
	GetPropAndRTPC(fAttack, AkModulatorPropID_Lfo_Attack, rtpcKey, g_AkModulatorPropDefault[AkModulatorPropID_Lfo_Attack].fValue );
	ApplyRange(AkModulatorPropID_Lfo_Attack, fAttack, 0.0f, FLT_MAX);
	pParams->m_uAttack = AkTimeConv::SecondsToSamples( fAttack );

	GetPropAndRTPC( pParams->m_fInitialPhase, AkModulatorPropID_Lfo_InitialPhase, rtpcKey, g_AkModulatorPropDefault[AkModulatorPropID_Lfo_InitialPhase].fValue );
	ApplyRange(AkModulatorPropID_Lfo_InitialPhase, pParams->m_fInitialPhase, -180.0f, 180.0f);

	pParams->m_lfo.Setup(DEFAULT_NATIVE_FREQUENCY, pParams->m_DspParams, pParams->m_fInitialPhase, AKRANDOM::AkRandom());
}

bool CAkLFOModulator::IsTriggerOk(const AkModulatorTriggerParams& in_params) const
{
	return !in_params.midiEvent.IsValid() || in_params.midiEvent.IsNoteOn();
}

void CAkEnvelopeModulator::GetInitialParams( AkModulatorParams* out_pParams, const CAkModulatorCtx* in_pModCtx  )
{
	AkEnvelopeParams* pParams = static_cast<AkEnvelopeParams*>(out_pParams);

	AkRTPCKey rtpcKey = in_pModCtx->GetRTPCKey();

	{
		AkReal32 fAttack;
		GetPropAndRTPC(fAttack, AkModulatorPropID_Envelope_AttackTime, rtpcKey, g_AkModulatorPropDefault[AkModulatorPropID_Envelope_AttackTime].fValue );
		ApplyRange(AkModulatorPropID_Envelope_AttackTime, fAttack, 0.0f, FLT_MAX);
		pParams->m_uAttack = AkTimeConv::SecondsToSamples( fAttack );
	}
	{
		AkReal32 fDecay;
		GetPropAndRTPC(fDecay, AkModulatorPropID_Envelope_DecayTime, rtpcKey, g_AkModulatorPropDefault[AkModulatorPropID_Envelope_DecayTime].fValue );
		ApplyRange(AkModulatorPropID_Envelope_DecayTime, fDecay, 0.0f, FLT_MAX);
		pParams->m_uDecay = AkTimeConv::SecondsToSamples( fDecay );
	}
	{
		AkReal32 fRelease;
		GetPropAndRTPC(fRelease, AkModulatorPropID_Envelope_ReleaseTime, rtpcKey, g_AkModulatorPropDefault[AkModulatorPropID_Envelope_ReleaseTime].fValue );
		ApplyRange(AkModulatorPropID_Envelope_ReleaseTime, fRelease, 0.0f, FLT_MAX);
		pParams->m_uRelease = AkTimeConv::SecondsToSamples( fRelease );
	}
	{
		AkReal32 fSustainTime;
		GetPropAndRTPC(fSustainTime, AkModulatorPropID_Envelope_SustainTime, rtpcKey, g_AkModulatorPropDefault[AkModulatorPropID_Envelope_SustainTime].fValue );
		if (fSustainTime >= 0.f)
		{
			ApplyRange(AkModulatorPropID_Envelope_SustainTime, fSustainTime, 0.0f, FLT_MAX);
			pParams->m_uReleaseFrame = pParams->m_uAttack + pParams->m_uDecay + AkTimeConv::SecondsToSamples( fSustainTime );
		}
		else
		{
			pParams->m_uReleaseFrame = INF_SUSTAIN;
		}
	}

	GetPropAndRTPC(pParams->m_fSustain, AkModulatorPropID_Envelope_SustainLevel, rtpcKey, g_AkModulatorPropDefault[AkModulatorPropID_Envelope_SustainLevel].fValue );
	ApplyRange(AkModulatorPropID_Envelope_SustainLevel, pParams->m_fSustain, 0.0f, 100.0f);
	pParams->m_fSustain /= 100.f; //Convert from percent.

	GetPropAndRTPC(pParams->m_fCurve, AkModulatorPropID_Envelope_AttackCurve, rtpcKey, g_AkModulatorPropDefault[AkModulatorPropID_Envelope_AttackCurve].fValue );
	ApplyRange(AkModulatorPropID_Envelope_AttackCurve, pParams->m_fCurve, 0.0f, 100.0f);
	pParams->m_fCurve /= 100.f; //Convert from percent.

	pParams->m_fStartValue = AkClamp( in_pModCtx->GetPreviousValue(), 0.f, 1.f );
}

bool CAkEnvelopeModulator::IsTriggerOk(const AkModulatorTriggerParams& in_params) const
{
	EnvelopeTriggerMode eTriggerOn =
		(EnvelopeTriggerMode)m_props.GetAkProp( AkModulatorPropID_Envelope_TriggerOn, g_AkModulatorPropDefault[ AkModulatorPropID_Envelope_TriggerOn ].iValue ).iValue;

	if ( in_params.midiEvent.IsValid() )
	{
		return ( eTriggerOn == EnvelopeTriggerMode_Play && in_params.eTriggerMode != AkModulatorTriggerParams::TriggerMode_EndOfNoteOn ) ||
				( eTriggerOn == EnvelopeTriggerMode_NoteOff && in_params.midiEvent.IsNoteOff() ) ;
	}
	else
	{
		return eTriggerOn == EnvelopeTriggerMode_Play;
	}
}

void CAkTimeModulator::GetInitialParams( AkModulatorParams* out_pParams, const CAkModulatorCtx* in_pModCtx )
{
	AkTimeModParams* pParams = static_cast<AkTimeModParams*>(out_pParams);

	AkRTPCKey rtpcKey = in_pModCtx->GetRTPCKey();

	AkReal32 fDuration = m_props.GetAkProp(AkModulatorPropID_Time_Duration, g_AkModulatorPropDefault[AkModulatorPropID_Time_Duration].fValue).fValue;
	pParams->m_uDuration = AkTimeConv::SecondsToSamples(fDuration);

	AkInt32 iStopPlay = m_props.GetAkProp(AkModulatorPropID_Envelope_StopPlayback, g_AkModulatorPropDefault[AkModulatorPropID_Envelope_StopPlayback].iValue).iValue;

	AkReal32 fPlaybackSpeed;
	GetPropAndRTPC(fPlaybackSpeed, AkModulatorPropID_Time_PlaybackRate, rtpcKey, g_AkModulatorPropDefault[AkModulatorPropID_Time_PlaybackRate].fValue);
	ApplyRange(AkModulatorPropID_Time_PlaybackRate, fPlaybackSpeed, 0.25f, 4.0f);
	pParams->m_fPlaybackSpeed = fPlaybackSpeed;

	AkReal32 fInitialDelay;
	GetPropAndRTPC(fInitialDelay, AkModulatorPropID_Time_InitialDelay, rtpcKey, g_AkModulatorPropDefault[AkModulatorPropID_Time_InitialDelay].fValue);
	ApplyRange(AkModulatorPropID_Time_InitialDelay, fInitialDelay, 0.0f, 4.0f);
	pParams->m_iInitialDelay = (AkInt32)AkTimeConv::SecondsToSamples(fInitialDelay);

	AkInt32 iLoops = m_props.GetAkProp(AkModulatorPropID_Time_Loops, g_AkModulatorPropDefault[AkModulatorPropID_Time_Loops].iValue).iValue;
	// If infinite -> stay infinite (don't apply randomizer)
	if (iLoops)
	{
		ApplyRange<AkInt32>(AkModulatorPropID_Time_Loops, iLoops, 1, 100);
		pParams->m_uLoopsDuration = iLoops * pParams->m_uDuration;
	}
	else
		pParams->m_uLoopsDuration = INF_SUSTAIN;

	if (iStopPlay)
		pParams->m_uReleaseFrame = pParams->m_uLoopsDuration;
	else
		pParams->m_uReleaseFrame = INF_SUSTAIN;
}

bool CAkTimeModulator::IsTriggerOk(const AkModulatorTriggerParams& in_params) const
{
	EnvelopeTriggerMode eTriggerOn =
		(EnvelopeTriggerMode)m_props.GetAkProp(AkModulatorPropID_Envelope_TriggerOn, g_AkModulatorPropDefault[AkModulatorPropID_Envelope_TriggerOn].iValue).iValue;

	if (in_params.midiEvent.IsValid())
	{
		return (eTriggerOn == EnvelopeTriggerMode_Play && in_params.eTriggerMode != AkModulatorTriggerParams::TriggerMode_EndOfNoteOn) ||
			(eTriggerOn == EnvelopeTriggerMode_NoteOff && in_params.midiEvent.IsNoteOff());
	}
	else
	{
		return eTriggerOn == EnvelopeTriggerMode_Play;
	}
}

AkUInt32 CAkModulator::AddRef()
{ 
	AkAutoLock<CAkLock> IndexLock( g_pIndex->m_idxModulators.GetLock() ); 
	return ++m_lRef; 
} 

AkUInt32 CAkModulator::Release() 
{ 
	AkAutoLock<CAkLock> IndexLock( g_pIndex->m_idxModulators.GetLock() ); 
	AkInt32 lRef = --m_lRef; 
	AKASSERT( lRef >= 0 ); 
	if ( !lRef ) 
	{
		RemoveFromIndex();
		AkDelete(AkMemID_Structure, this );
	}
	return lRef; 
}
