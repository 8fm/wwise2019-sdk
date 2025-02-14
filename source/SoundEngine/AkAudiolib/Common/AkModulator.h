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
// AkModulator.h
//
// creator : nharris
//
//////////////////////////////////////////////////////////////////////

#ifndef _MODULATOR_H_
#define _MODULATOR_H_

#include "AkModifiers.h"
#include "AkModulatorProps.h"
#include "AkIndexable.h"
#include "AkAudioLibIndex.h"
#include "AkPropBundle.h"
#include "AkBitArray.h"
#include "AkRTPCMgr.h"

class CAkRegisteredObj;
class CAkModulatorCtx;
class CAkModulatorEngine;
class CAkModulatorData;
struct AkModulatorParams;
struct AkMidiNoteChannelPair;
struct AkModulatorTriggerParams;
struct AkModulatorSubscriberInfo;

enum AkModulatorType
{
	AkModulatorType_Unknown = -1,

	AkModulatorType_LFO,
	AkModulatorType_Envelope,
	AkModulatorType_Time
};

class CAkModulator : public CAkIndexable
{
public:
	static CAkModulator* Create(AkUniqueID in_ulID, AkModulatorType in_eType);
	virtual ~CAkModulator();

	AKRESULT Trigger(const AkModulatorSubscriberInfo& in_subscrInfo, const AkModulatorTriggerParams& in_params, CAkModulatorEngine& in_modEng, CAkModulatorData* io_pModPBIData);
	AKRESULT TriggerRelease(const AkModulatorTriggerParams& in_params);

	virtual AKRESULT SetInitialValues(AkUInt8* in_pData, AkUInt32 in_ulDataSize);

	// Set RTPC info
	AKRESULT SetRTPC(
		AkRtpcID				in_RTPC_ID,
		AkRtpcType				in_RTPCType,
		AkRtpcAccum				in_RTPCAccum,
		AkRTPC_ParameterID		in_ParamID,
		AkUniqueID				in_RTPCCurveID,
		AkCurveScaling			in_eScaling,
		AkRTPCGraphPoint*		in_pArrayConversion,		// NULL if none
		AkUInt32				in_ulConversionArraySize,	// 0 if none
		bool						in_bNotify = true
		);

	void UnsetRTPC(
		AkRTPC_ParameterID		in_ParamID,
		AkUniqueID				in_RTPCCurveID
		);

	void SetParamFromRTPCMgr(AkRTPC_ParameterID in_eParam, const AkReal32 in_fValue, const AkRTPCKey& in_rtpcKey );

	void SetAkProp( AkModulatorPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax );
	void SetAkProp( AkModulatorPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax );

	//Called before the first update, after the modulator is triggered. out_pParams can be cast to the appropriate type.
	virtual void GetInitialParams( AkModulatorParams* out_pParams, const CAkModulatorCtx* in_pModCtx ) = 0;

	AkModulatorType GetType(){ return m_eType; }

	virtual bool SupportsAutomatedParams() = 0;

	inline CAkModulatorCtx* FindCtx( AkRTPCKey in_rtpcValueKey )
	{
		AkModTreeValue* pVal = m_mapCtxs.FindExact(in_rtpcValueKey);
		return pVal != NULL ? pVal->pCtx : NULL;
	}

	AKRESULT AddSubscription( CAkRTPCMgr::AkRTPCSubscription * in_pSubscription )
	{
		AKRESULT res = AK_Success;
		if ( m_subscriptions.Exists(in_pSubscription) == NULL )
		{
			if ( m_subscriptions.Add(in_pSubscription) == NULL )
				res = AK_InsufficientMemory;
			else
				m_bAutomatedParamSubscribed = (m_bAutomatedParamSubscribed | (IsAutomatedParam(in_pSubscription->key.ParamID, in_pSubscription->eType) && SupportsAutomatedParams()) );
		}
		return res;
	}

	//Remove the reference to the subscription from this modulator's map.  
	//	Note the check to make sure that there are no additional curves that reference this modulator.  
	void RemoveSubscription( CAkRTPCMgr::AkRTPCSubscription * in_pSubscription )
	{
#ifndef AK_OPTIMIZED
		for( CAkRTPCMgr::RTPCCurveArray::Iterator iterCurves = in_pSubscription->Curves.Begin(); 
			iterCurves != in_pSubscription->Curves.End(); ++iterCurves )
		{
			AKASSERT( (*iterCurves).RTPC_ID != key );
		}
#endif
		m_subscriptions.Unset(in_pSubscription);
	}

	//Remove the reference to the subscription from this modulator's map, *only if* no additional curves 
	//	remain that reference this modulator. 
	bool RemoveSubscriptionIfNoCurvesRemain( CAkRTPCMgr::AkRTPCSubscription * in_pSubscription )
	{
		for( CAkRTPCMgr::RTPCCurveArray::Iterator iterCurves = in_pSubscription->Curves.Begin(); 
			iterCurves != in_pSubscription->Curves.End(); ++iterCurves )
		{
			//Bail out.. the subscription still has curves that reference this modulator
			if( (*iterCurves).RTPC_ID == key )
				return false;
		}

		m_subscriptions.Unset(in_pSubscription);
		return true;
	}

	//Remove all references to game obj, stop all game object scoped modulators.
	void UnregisterGameObject( CAkRegisteredObj * in_GameObj );

	bool HasSubscriber( void * in_pSubscription )
	{
		for ( CAkRTPCMgr::AkRTPCSubscriptions::Iterator it = m_subscriptions.Begin(), 
			itEnd = m_subscriptions.End(); 
			it != itEnd; ++it )
		{
			CAkRTPCMgr::AkRTPCSubscription& subscr = **it;
			if (subscr.key.pSubscriber == in_pSubscription)
				return true;
		}

		return false;
	}

	// Get modulator output value for a modulator context that corresponds to the scope defined by io_rtpcKey:
	bool GetCurrentValue(	AkRTPCKey& io_rtpcKey, AkReal32& out_value	);
	bool GetPreviousValue(	AkRTPCKey& io_rtpcKey, AkReal32& out_value	);

	void NotifySubscribers();

	AkUniqueID GetModulatorID() const { return (AkUniqueID) key;}

	bool StopWhenFinished() const;
	AkModulatorScope GetScope() const;

	//True if at least one subscriber has an audio-rate automated parameter.
	bool AutomatedParamSubscribed(){return m_bAutomatedParamSubscribed;}

	bool RemoveCtxsMatchingKey( const AkRTPCKey& in_key );

public:

	virtual AkUInt32 AddRef();
	virtual AkUInt32 Release();

protected:
	AkRTPCKey KeyForScope( AkModulatorScope in_eScope, const AkRTPCKey& in_key );

	virtual CAkModulatorCtx* _Trigger(CAkModulatorCtx* in_pExistingCtx, const AkModulatorTriggerParams& in_params, CAkParameterNodeBase* in_pTargetNode, AkModulatorScope in_eScope, bool& out_bAllocatedCtx);
	virtual bool ShouldRetrigger() const = 0;
	virtual bool IsTriggerOk(const AkModulatorTriggerParams& in_params) const = 0;

	AKRESULT Init();
	CAkModulator( AkUniqueID in_ulID );

	void AddToIndex();	// Adds the Attenuation in the General index
	void RemoveFromIndex();	// Removes the Attenuation from the General index

	AkForceInline void GetPropAndRTPC( AkReal32& out_fValue, AkModulatorPropID in_propId, AkRTPCKey& in_rtpcKey, AkReal32 in_fDefaultValue = 0.0f )
	{
		AkRTPCKey retrievedKey = in_rtpcKey;
		out_fValue = m_props.GetAkProp( in_propId, in_fDefaultValue ).fValue;
		AkRTPC_ModulatorParamID rtpcId = g_AkModulatorPropRTPCID[in_propId];
		if( m_RTPCBitArray.IsSet( rtpcId - RTPC_ModulatorRTPCIDStart ) )
		{
			out_fValue = g_pRTPCMgr->GetRTPCConvertedValue( (void*)this, rtpcId, retrievedKey);
		}
	}

	AkForceInline void GetPropAndRTPC( AkInt32& out_iValue, AkModulatorPropID in_propId, const AkRTPCKey& in_rtpcKey, AkInt32 in_iDefaultValue = 0 )
	{
		AkRTPCKey retrievedKey = in_rtpcKey;
		out_iValue = m_props.GetAkProp( in_propId, in_iDefaultValue ).iValue;
		AkRTPC_ModulatorParamID rtpcId = g_AkModulatorPropRTPCID[in_propId];
		if( m_RTPCBitArray.IsSet( rtpcId - RTPC_ModulatorRTPCIDStart ) )
		{
			out_iValue = (AkInt32) g_pRTPCMgr->GetRTPCConvertedValue( (void*)this, rtpcId, retrievedKey  );
		}
	}

	template<class T_VALUE>
	AkForceInline void ApplyRange( AkModulatorPropID in_ePropID, T_VALUE & io_value, T_VALUE in_clampMin, T_VALUE in_clampMax  )
	{
		RANGED_MODIFIERS<T_VALUE> * pRange = (RANGED_MODIFIERS<T_VALUE> *) m_ranges.FindProp( in_ePropID );
		if ( pRange )
		{
			io_value += RandomizerModifier::GetMod( *pRange );
			io_value = AkClamp(io_value, in_clampMin, in_clampMax);
		}
	}

	AkModulatorType m_eType;

	AkPropBundle< AkPropValue >	m_props;
	AkPropBundle< RANGED_MODIFIERS<AkPropValue> > m_ranges;

	CAkBitArray<AkUInt32>		m_RTPCBitArray;

	//Maps a AkRTPCKey to a CAkModulatorCtx
	AkModulatorCtxTree m_mapCtxs;

	// The modulator has a sorted list of the subscriptions.  Each subscription has a list of curves, each with
	//	an RTPC id that identifies a game parameter or modulator.  If the subscription is in this list then the assumption is
	//	that one of the curves references this modulator.
	CAkRTPCMgr::AkRTPCSubscriptions m_subscriptions; 

	bool m_bAutomatedParamSubscribed;

	static void ModCtx_SetParam( AkModTreeValue& in_val, const AkRTPCKey& in_rtpcKey, void* in_cookie );
	static bool ModCtx_MarkFinishedAndRelease( AkModTreeValue& in_val, const AkRTPCKey& in_rtpcKey, void* in_cookie );
	static bool ModCtx_NotifySubscribers( AkModTreeValue& in_val, const AkRTPCKey& in_rtpcKey, void* in_cookie );
};


class CAkLFOModulator: public CAkModulator
{
public:
	CAkLFOModulator(AkUniqueID in_ulID): CAkModulator(in_ulID) { m_eType = AkModulatorType_LFO; }
	virtual ~CAkLFOModulator(){};
	
	virtual void GetInitialParams( AkModulatorParams* out_pParams, const CAkModulatorCtx* in_pModCtx );

	static CAkLFOModulator* Create(AkUniqueID in_ulID) { return static_cast<CAkLFOModulator*>( CAkModulator::Create( in_ulID, AkModulatorType_LFO ) ); }
	static AkModulatorType GetType(){ return AkModulatorType_LFO; }
	bool SupportsAutomatedParams(){ return true; }
protected:
	virtual bool ShouldRetrigger() const {return false;}
	virtual bool IsTriggerOk(const AkModulatorTriggerParams& in_params) const;
};

enum EnvelopeTriggerMode
{
	EnvelopeTriggerMode_Play = 1,
	EnvelopeTriggerMode_NoteOff
};

class CAkEnvelopeModulator: public CAkModulator
{
public:
	CAkEnvelopeModulator(AkUniqueID in_ulID): CAkModulator(in_ulID) { m_eType = AkModulatorType_Envelope; }
	virtual ~CAkEnvelopeModulator(){};

	virtual void GetInitialParams( AkModulatorParams* out_pParams, const CAkModulatorCtx* in_pModCtx );

	static CAkEnvelopeModulator* Create(AkUniqueID in_ulID) { return static_cast<CAkEnvelopeModulator*>( CAkModulator::Create( in_ulID, AkModulatorType_Envelope ) ); }
	static AkModulatorType GetType(){ return AkModulatorType_Envelope; }
	bool SupportsAutomatedParams(){ return true; }
protected:
	virtual bool ShouldRetrigger() const { return true; }
	virtual bool IsTriggerOk(const AkModulatorTriggerParams& in_params) const;
};

class CAkTimeModulator : public CAkModulator
{
public:
	CAkTimeModulator(AkUniqueID in_ulID) : CAkModulator(in_ulID) { m_eType = AkModulatorType_Time; }
	virtual ~CAkTimeModulator(){};

	virtual void GetInitialParams( AkModulatorParams* out_pParams, const CAkModulatorCtx* in_pModCtx );

	static CAkTimeModulator* Create(AkUniqueID in_ulID) { return static_cast<CAkTimeModulator*>( CAkModulator::Create( in_ulID, AkModulatorType_Time ) ); }
	static AkModulatorType GetType(){ return AkModulatorType_Time; }
	bool SupportsAutomatedParams(){ return false; }
protected:
	virtual bool ShouldRetrigger() const { return false; }
	virtual bool IsTriggerOk(const AkModulatorTriggerParams& in_params) const;
};

#endif
