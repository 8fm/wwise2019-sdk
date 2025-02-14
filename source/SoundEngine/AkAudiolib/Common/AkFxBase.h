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

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>
#include "AkMidiStructs.h"
#include "AkConversionTable.h"
#include "AkIndexable.h"
#include "AkFxBaseStateAware.h"
#include "IAkRTPCSubscriberPlugin.h"
#include <AK/Tools/Common/AkKeyArray.h>
#include "AkRTPC.h"
#include "AkRTPCKey.h"
#include "AkRTPCMgr.h"

class CAkBehavioralCtx;
class CAkRegisteredObj;
class CAkModulatorData;
struct AkModulatorTriggerParams;
struct AkModulatorSubscriberInfo;

typedef CAkKeyArray<AkUInt32/*index*/, AkUniqueID/*media id*/> AkMediaMap;

typedef AkUInt16 AkPluginPropertyId;
typedef AkPropBundle<AkReal32,AkPluginPropertyId> AkPluginPropBundle;

struct PluginRtpcSubscription
{
	AkRtpcID			rtpcId;
	AkUInt8				rtpcType;
	AkUInt8				rtpcAccum;
	AkPluginPropertyId	propertyId;		//# of the param that must be notified on change
	AkUniqueID			curveId;
	CAkConversionTable	conversionTable;
};
typedef AkArray<PluginRtpcSubscription, const PluginRtpcSubscription&> PluginRtpcSubscriptions;

struct PluginPropertyValue
{
	PluginPropertyValue(
		AkPluginPropertyId in_propertyId = (AkPluginPropertyId)0,
		AkRtpcAccum in_rtpcAccum = AkRtpcAccum_None,
		AkReal32 in_fValue = 0.f
		)
		: propertyId( in_propertyId )
		, rtpcAccum( (AkUInt8)in_rtpcAccum )
		, fValue( in_fValue )
	{}

	bool operator == ( const PluginPropertyValue& in_other ) { return propertyId == in_other.propertyId; }

	AkPluginPropertyId	propertyId;		//# of the param that must be notified on change
	AkUInt8				rtpcAccum;
	AkReal32			fValue;
};
typedef AkArray<PluginPropertyValue, const PluginPropertyValue&> PluginPropertyValues;


enum PluginPropertyUpdateType { PluginPropertyUpdateType_FxBase, PluginPropertyUpdateType_Rtpc, PluginPropertyUpdateType_State };

struct PluginPropertyUpdate
{
	PluginPropertyUpdate(
		PluginPropertyUpdateType in_updateType = PluginPropertyUpdateType_FxBase,
		AkUniqueID in_updateId = AK_INVALID_UNIQUE_ID,
		AkPluginPropertyId in_propertyId = (AkPluginPropertyId)0,
		AkReal32 in_fValue = 0.f
		)
		: updateType( in_updateType )
		, updateId( in_updateId )
		, propertyId( in_propertyId )
		, fValue( in_fValue )
	{}

	bool operator == ( const PluginPropertyValue& in_other ) { return propertyId == in_other.propertyId; }

	void operator = ( const PluginPropertyValue& in_value )
	{
		propertyId = in_value.propertyId;
		fValue = in_value.fValue;
	}

	PluginPropertyUpdateType	updateType;
	AkUniqueID					updateId;
	AkPluginPropertyId			propertyId;
	AkReal32					fValue;
};

typedef AkArray<PluginPropertyUpdate, const PluginPropertyUpdate&> PluginPropertyUpdates;



struct PluginRTPCSub;


class CAkFxBase
	: public CAkIndexable
	, public CAkFxBaseStateAware
{
public:
	// CAkFxBase
	virtual bool IsShareSet() = 0;

	AKRESULT SetInitialValues( AkUInt8* in_pData, AkUInt32 in_uDataSize );

	void SetFX( 
		AkPluginID in_FXID,
		AK::IAkPluginParam * in_pParam
		);

	AKRESULT SetFX( 
		AkPluginID in_FXID,
		void * in_pParamBlock,
		AkUInt32 in_uParamBlockSize
		);

	AK::IAkPluginParam * GetFXParam() { return m_pParam; }

	void SetFXParam( 
		AkPluginParamID in_uParamID,	// ID of the param to modify, will be done by the plug-in itself
		void * in_pParamBlock,			// Pointer to the Param block
		AkUInt32 in_uParamBlockSize		// BLOB size
		);

	AKRESULT SetRTPC(
		AkRtpcID			in_RTPC_ID,
		AkRtpcType			in_RTPCType,
		AkRtpcAccum			in_RTPCAccum,
		AkRTPC_ParameterID	in_ParamID,
		AkUniqueID			in_RTPCCurveID,
		AkCurveScaling		in_eScaling,
		AkRTPCGraphPoint*	in_pArrayConversion = NULL,
		AkUInt32			in_ulConversionArraySize = 0,
		bool				in_bNotify = true
		);

	void UnsetRTPC( 
		AkRTPC_ParameterID in_ParamID,
		AkUniqueID in_RTPCCurveID,
		bool in_bNotify = true
		);

	AKRESULT SetPropertyValue( 
		AkPluginPropertyId	in_propertyId,
		AkReal32			in_fValue
		);

	AkPluginID GetFXID() { return m_FXID; }

	AkUniqueID GetMediaID( AkUInt32 in_uIdx ) { AkUniqueID * pID = m_media.Exists( in_uIdx ); return pID ? *pID : AK_INVALID_UNIQUE_ID; }
	void SetMediaID( AkUInt32 in_uIdx, AkUniqueID in_mediaID );

	void SubscribeRTPC( IAkRTPCSubscriberPlugin* in_pSubscriber, const AkRTPCKey& in_rtpcKey = AkRTPCKey() );
	void UnsubscribeRTPC( IAkRTPCSubscriberPlugin* in_pSubscriber );

protected:

	AkPluginID					m_FXID;
	AK::IAkPluginParam *		m_pParam;
	AkMediaMap					m_media;
	PluginRtpcSubscriptions		m_rtpcSubscriptions;
	PluginPropertyValues		m_propertyValues;

protected:
	CAkFxBase( AkUniqueID in_ulID );
	virtual ~CAkFxBase();

private:
	friend struct PluginRTPCSub;
	friend class CAkFxBaseStateAware;

	typedef AkListBareLight<PluginRTPCSub> Instances;
	Instances m_instances;

	void AddInstance( PluginRTPCSub* in_instance );
	void RemoveInstance( PluginRTPCSub* in_instance );

	void RecalculatePropertyValue( AkPluginPropertyId in_propertyId );
	void RecalculatePropertyValues();

	void TriggerModulators(const AkModulatorSubscriberInfo& in_subscrInfo, const AkModulatorTriggerParams& in_params, CAkModulatorData* io_pModPBIData);
};

class CAkFxShareSet
	: public CAkFxBase
{
public:
	static CAkFxShareSet* Create( AkUniqueID in_ulID = 0 );

	// CAkCommonBase
	virtual AkUInt32 AddRef();
	virtual AkUInt32 Release();

	// CAkFxBase
	virtual bool IsShareSet() { return true; }

protected:
	CAkFxShareSet( AkUniqueID in_ulID );
};

class CAkFxCustom
	: public CAkFxBase
{
public:
	static CAkFxCustom* Create( AkUniqueID in_ulID = 0 );

	// CAkCommonBase
	virtual AkUInt32 AddRef();
	virtual AkUInt32 Release();

	// CAkFxBase
	virtual bool IsShareSet() { return false; }

protected:
	CAkFxCustom( AkUniqueID in_ulID );
};

class CAkAudioDevice
	: public CAkFxBase
{
public:
	static CAkAudioDevice* Create(AkUniqueID in_ulID = 0);

	// CAkCommonBase
	virtual AkUInt32 AddRef();
	virtual AkUInt32 Release();

	// CAkFxBase
	virtual bool IsShareSet() { return true; }// Doesn't matter in fact...

protected:
	CAkAudioDevice(AkUniqueID in_ulID);
};

struct PluginRTPCSub : public IAkRTPCSubscriberPlugin
{
	AK::IAkPluginParam*		pParam;				// Parameters.
	CAkFxBase*				pFx;				// Pointer to FX node.
	AkRTPCKey				rtpcKey;			// RTPC key used to subscribe
	PluginPropertyUpdates	propertyUpdates;	// Values kept for properties that may have RTPCs / States
	PluginRTPCSub*			pNextLightItem;		// Light list maintained by CAkFxBase

#ifndef AK_OPTIMIZED
private:
	AkUniqueID				m_contextId;		// The ID of a plugin context (e.g. from a CAkPBI or CAkMixBusCtx), used for profiling
	AkUniqueID				m_contextPipelineId;
public:
#endif

	PluginRTPCSub()
		: pParam( NULL )
		, pFx( NULL )
		, pNextLightItem( NULL )
#ifndef AK_OPTIMIZED
		, m_contextId(AK_INVALID_UNIQUE_ID)
		, m_contextPipelineId(AK_INVALID_UNIQUE_ID)
#endif
	{}

	virtual ~PluginRTPCSub() { Term(); }

	virtual void Term();

	PluginRTPCSub& operator=(const PluginRTPCSub&);

	AK::IAkPluginParam* Clone(CAkFxBase* in_pCloneFrom, CAkBehavioralCtx* in_pCtx);
	AK::IAkPluginParam* Clone(CAkFxBase* in_pCloneFrom, const AkRTPCKey& in_rtpcKey, CAkModulatorData* in_pModulatorData, bool in_bSubscribeRTPC = true);
	void SubscribeRTPC(CAkBehavioralCtx* in_pCtx);
	void SubscribeRTPC(const AkRTPCKey& in_rtpcKey, CAkModulatorData* in_pModulatorData);
	void UnsubscribeRTPC();

	void TakeFrom( PluginRTPCSub& in_from );

#ifndef AK_OPTIMIZED
	void RecapRTPCParams();
	void UpdateContextID(AkUniqueID in_contextId, AkUniqueID in_pipelineId);
	AkUniqueID GetContextID();
	AkUniqueID GetContextPipelineID();
#endif

	virtual AkPluginID GetPluginID();

	// Functions for RTPC manager
	virtual void UpdateTargetRtpcParam( AkRTPC_ParameterID in_paramId, AkUniqueID in_rtpcId, AkRtpcAccum in_rtpcAccum, AkReal32 in_value )
	{ UpdateTargetParam( PluginPropertyUpdateType_Rtpc, in_rtpcId, in_paramId, in_rtpcAccum, in_value ); }

	virtual void NotifyRtpcParamChanged( AkRTPC_ParameterID in_paramId, AkUniqueID in_rtpcId, AkRtpcAccum in_rtpcAccum, AkReal32 in_value, bool in_bRecalculate )
	{ UpdateTargetParam( PluginPropertyUpdateType_Rtpc, in_rtpcId, in_paramId, in_rtpcAccum, in_value ); }

protected:
	void UpdateTargetParam( PluginPropertyUpdateType in_updateType, AkUniqueID in_updateId, AkRTPC_ParameterID in_paramId, AkRtpcAccum in_rtpcAccum, AkReal32 in_value );

private:
	friend class CAkFxBase;
	friend class CAkStateAware;
	friend class CAkFxBaseStateAware;

	AK::IAkPluginParam* _Clone(CAkFxBase* in_pCloneFrom, CAkBehavioralCtx* in_pCtx, const AkRTPCKey& in_rtpcKey, CAkModulatorData* in_pModulatorData, bool in_bSubscribeRTPC);
	AkUInt32 GetPropertyUpdateArraySize() const;

	void TriggerModulators(CAkBehavioralCtx* in_pCtx);
	void TriggerModulators(CAkRegisteredObj* in_pGameObj, CAkModulatorData* in_pModulatorData);

	AkReal32 AccumulatePropertyUpdates( AkPluginPropertyId in_propertyId, AkRtpcAccum in_rtpcAccum, AkUInt32 in_length );

	void RecalculatePropertyUpdates();
	void RecalculatePropertyUpdate( AkPluginPropertyId in_propertyId );
	void _RecalculatePropertyUpdate( AkPluginPropertyId in_propertyId, AkReal32 in_fxValue );
	void RecalculatePropertyUpdateStates( AkPluginPropertyId in_propertyId, AkReal32& io_value, AkRtpcAccum& out_rtpcAccum );
	void RecalculatePropertyUpdateRtpc( AkPluginPropertyId in_propertyId, AkReal32& io_value, AkRtpcAccum& out_rtpcAccum );

	void CalculatePropertyUpdateStates();
	bool GetPropertyStateValues( AkStatePropertyId in_propertyId, AkRtpcAccum in_rtpcAccum, AkReal32& io_value );
	void StateAwareParamValue( AkStatePropertyId in_propertyId, AkStateGroupID in_stateGroup, AkReal32 in_value );

	void SetPluginParam( AkPluginPropertyId in_propertyId, void * in_pParamBlock, AkUInt32 in_uParamBlockSize ) const;
};

struct AkSinkPluginParams : public PluginRTPCSub
{
	AK::IAkSinkPlugin* 		pSink;

	AkSinkPluginParams()
		: pSink(NULL)
	{}

	virtual ~AkSinkPluginParams() { Term(); }

	virtual void Term();

	AkSinkPluginParams& operator=(const AkSinkPluginParams&);
	void TakeFrom( AkSinkPluginParams& in_from );
};
