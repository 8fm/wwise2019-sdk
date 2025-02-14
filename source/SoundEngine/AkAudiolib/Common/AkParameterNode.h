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
// AkParameterNode.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _PARAMETER_NODE_H_
#define _PARAMETER_NODE_H_

#include "AkParameterNodeBase.h"

// class corresponding to node having parameters and states
//
// Author:  alessard
class CAkParameterNode : public CAkParameterNodeBase
{
	friend class CAkPBI;
	friend class CAkBehavioralCtx;
	friend class CAkSIS;

public:
	// Constructors
    CAkParameterNode(AkUniqueID in_ulID);

	//Destructor
    virtual ~CAkParameterNode();

	AKRESULT Init(){ return CAkParameterNodeBase::Init(); }

	virtual AKRESULT PlayInternal( AkPBIParams& in_rPBIParams ) = 0;
	
	AKRESULT Play( AkPBIParams& in_rPBIParams )
	{
		if (in_rPBIParams.midiEvent.IsValid() && in_rPBIParams.midiEvent.IsNoteOn())
		{
			bool bMidiCheckParent = in_rPBIParams.bMidiCheckParent;
			AKRESULT eFilterResult = FilterAndTransformMidiEvent( in_rPBIParams.midiEvent, in_rPBIParams.GetMidiTargetID(), bMidiCheckParent, in_rPBIParams.pGameObj, in_rPBIParams.userParams.PlayingID() );
			if (eFilterResult != AK_Success)
				return eFilterResult;
			in_rPBIParams.bMidiCheckParent = bMidiCheckParent;
		}

		AKRESULT eResult = HandleInitialDelay( in_rPBIParams );
		if( eResult == AK_PartialSuccess )
			return AK_Success;
		else if( eResult == AK_Success )
			return PlayInternal( in_rPBIParams );
	
		return eResult;
	}

	// Set the value of a property at the node level (float)
	virtual void SetAkProp( 
		AkPropID in_eProp, 
		AkReal32 in_fValue, 
		AkReal32 in_fMin, 
		AkReal32 in_fMax 
		);
	
	// Set the value of a property at the node level (int)
	virtual void SetAkProp( 
		AkPropID in_eProp, 
		AkInt32 in_iValue, 
		AkInt32 in_iMin, 
		AkInt32 in_iMax 
		);

	// Set a runtime property value (SIS)
	virtual void SetAkProp(
		AkPropID in_eProp, 
		CAkRegisteredObj * in_pGameObj,		// Game object associated to the action
		AkValueMeaning in_eValueMeaning,	// Target value meaning
		AkReal32 in_TargetValue = 0,		// Pitch target value
		AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,
		AkTimeMs in_lTransitionTime = 0
		);

	// Mute the element
	virtual void Mute(
		CAkRegisteredObj *	in_pGameObj,
		AkCurveInterpolation		in_eFadeCurve = AkCurveInterpolation_Linear,
		AkTimeMs		in_lTransitionTime = 0
		);

	// Un-Mute the element(per object and main)
	virtual void UnmuteAll(
		AkCurveInterpolation		in_eFadeCurve = AkCurveInterpolation_Linear,
		AkTimeMs		in_lTransitionTime = 0
		);

	//Unmute all per object elements
	virtual void UnmuteAllObj(
		AkCurveInterpolation		in_eFadeCurve = AkCurveInterpolation_Linear,
		AkTimeMs		in_lTransitionTime = 0
		);

	virtual bool ParamOverriden( AkRTPC_ParameterID in_ParamID )
	{
		bool  bOverriden;
		switch (in_ParamID)
		{
		case RTPC_UserAuxSendVolume0:
		case RTPC_UserAuxSendVolume1:
		case RTPC_UserAuxSendVolume2:
		case RTPC_UserAuxSendVolume3:
		case RTPC_UserAuxSendLPF0:
		case RTPC_UserAuxSendLPF1:
		case RTPC_UserAuxSendLPF2:
		case RTPC_UserAuxSendLPF3:
		case RTPC_UserAuxSendHPF0:
		case RTPC_UserAuxSendHPF1:
		case RTPC_UserAuxSendHPF2:
		case RTPC_UserAuxSendHPF3:
			bOverriden = GetOverrideUserAuxSends() || !m_pParentNode;
			break;

		case RTPC_GameAuxSendVolume:
		case RTPC_GameAuxSendLPF:
		case RTPC_GameAuxSendHPF:
			bOverriden = GetOverrideGameAuxSends() || !m_pParentNode;
			break;

		case RTPC_OutputBusVolume:
		case RTPC_OutputBusLPF:
		case RTPC_OutputBusHPF:
			bOverriden = m_pBusOutputNode != NULL;
			break;
		
		case RTPC_ReflectionsVolume:
			bOverriden = GetOverrideReflectionsAuxBus() || !m_pParentNode;
			break;

		default:
			bOverriden = false;
		}
		return bOverriden;
	}

	virtual bool ParamMustNotify( AkRTPC_ParameterID in_ParamID )
	{
		bool  bMustNotify;
		switch (in_ParamID)
		{
		case RTPC_UserAuxSendVolume0:
		case RTPC_UserAuxSendVolume1:
		case RTPC_UserAuxSendVolume2:
		case RTPC_UserAuxSendVolume3:
		case RTPC_UserAuxSendLPF0:
		case RTPC_UserAuxSendLPF1:
		case RTPC_UserAuxSendLPF2:
		case RTPC_UserAuxSendLPF3:
		case RTPC_UserAuxSendHPF0:
		case RTPC_UserAuxSendHPF1:
		case RTPC_UserAuxSendHPF2:
		case RTPC_UserAuxSendHPF3:
			bMustNotify = GetOverrideUserAuxSends() || !m_pParentNode;
			break;

		case RTPC_GameAuxSendVolume:
		case RTPC_GameAuxSendLPF:
		case RTPC_GameAuxSendHPF:
			bMustNotify = GetOverrideGameAuxSends() || !m_pParentNode;
			break;

		case RTPC_OutputBusVolume:
		case RTPC_OutputBusLPF:
		case RTPC_OutputBusHPF:
			bMustNotify = m_pBusOutputNode != NULL;
			break;

		default:
			bMustNotify = true;
		}
		return bMustNotify;
	}

	// Fill the parameters structures with the new parameters
	//
	// Return - AKRESULT - AK_Success if succeeded
	virtual AKRESULT GetAudioParameters(
		AkSoundParams				&out_Parameters,	// Set of parameter to be filled		
		AkMutedMap&					io_rMutedMap,			// Map of muted elements
		const AkRTPCKey&			in_rtpcKey,				// Key to retrieve rtpc parameters
		AkPBIModValues*				io_pRanges,				// Range structure to be filled (or NULL if not required)
		AkModulatorsToTrigger*		out_pModulators,		// Modulators to trigger (or NULL if not required)
		bool						in_bDoBusCheck = true,
		CAkParameterNodeBase*		in_pStopAtNode = NULL
		);

	AKRESULT PlayAndContinueAlternate( AkPBIParams& in_rPBIParams );

	virtual void ExecuteActionExceptParentCheck( ActionParamsExcept& in_rAction );

	virtual AKRESULT SetInitialFxParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize, bool in_bPartialLoadOnly );
	void OverrideFXParent( bool in_bIsFXOverrideParent );

	virtual void GetFX(
		AkUInt32	in_uFXIndex,
		AkFXDesc&	out_rFXInfo,
		CAkRegisteredObj * in_GameObj = NULL
		);

	virtual void GetFXDataID(
		AkUInt32	in_uFXIndex,
		AkUInt32	in_uDataIndex,
		AkUInt32&	out_rDataID
		);

	inline virtual void SetBelowThresholdBehavior( AkBelowThresholdBehavior in_eBelowThresholdBehavior )
	{
		m_eBelowThresholdBehavior = in_eBelowThresholdBehavior;
		
		if ( m_eBelowThresholdBehavior == AkBelowThresholdBehavior_KillIfOneShotElseVirtual )
			m_eVirtualQueueBehavior = AkVirtualQueueBehavior_FromElapsedTime;
		else
			m_eVirtualQueueBehavior = m_eCurVirtualQueueBehavior;
	}

	inline void SetVirtualQueueBehavior( AkVirtualQueueBehavior in_eBehavior )
	{
		m_eCurVirtualQueueBehavior = m_eVirtualQueueBehavior = in_eBehavior;
	}

	// Used to increment/decrement the playcount used for notifications and ducking
	virtual AKRESULT IncrementPlayCount( CounterParameters& io_params, bool in_bNewInstance, bool in_bSwitchingOutputBus = false );

	virtual void DecrementPlayCount(
		CounterParameters& io_params
		);

	virtual void ApplyMaxNumInstances( AkUInt16 in_u16MaxNumInstance );

	bool IsOrIsChildOf( CAkParameterNodeBase * in_pNodeToTest, bool in_bIsBusChecked = false );

	// Returns true if the Context may jump to virtual, false otherwise.
	AkBelowThresholdBehavior GetVirtualBehavior( AkVirtualQueueBehavior& out_Behavior ) const;

	AKRESULT AssociateLayer( CAkLayer* in_pLayer );
	AKRESULT DissociateLayer( CAkLayer* in_pLayer );

	bool GetFXOverrideParent() const { return (m_overriddenParams & (RTPC_FX_PARAMS_BITFIELD)) != 0; }

	AKRESULT HandleInitialDelay( AkPBIParams& in_rPBIParams );
	AKRESULT DelayPlayback( AkReal32 in_fDelay, AkPBIParams& in_rPBIParams );

	// HDR
	void SetOverrideHdrEnvelope( bool in_bOverrideParent );
	void SetOverrideAnalysis( bool in_bOverrideParent );
	void SetNormalizeLoudness( bool in_bNormalizeLoudness );
	void SetEnableEnvelope( bool in_bEnableEnvelope );

	bool GetOverrideHdrEnvelope() const
	{
		return m_overriddenParams.IsSet(RTPC_HDRActiveRange);
	}

	MidiPlayOnNoteType GetMidiPlayOnNoteType() const;
	MidiEventActionType GetMidiEventAction( const AkMidiEventEx& in_midiEvent ) const;
	MidiEventActionType GetMidiNoteOffAction() const;
	MidiEventActionType GetMidiNoteOnAction() const;
	AKRESULT FilterAndTransformMidiEvent( AkMidiEventEx& io_midiEvent, AkUniqueID in_midiTarget, bool& io_bMidiCheckParent, CAkRegisteredObj* in_pGameObj, AkPlayingID in_playingID );
	
	virtual void TriggerModulators( const AkModulatorTriggerParams& in_params, CAkModulatorData* out_pModPBIData, bool in_bDoBusCheck = true ) const;

	static void IncrementLESyncCount();

protected:
	// Returns true if out_iBypassAllFx was set (either to true or false) because this was a node with FX (either a top-level or overriden node)
	bool GetBypassAllFX( CAkRegisteredObj * in_pGameObj, AkInt16& out_iBypassAllFx );

	virtual AKRESULT SetInitialParams( AkUInt8*& pData, AkUInt32& ulDataSize );
	
	virtual AKRESULT SetAdvSettingsParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize );

	template<class T_VALUE, typename Monitor>
	AkForceInline void ApplyRange(AkPropID in_ePropID, T_VALUE & io_value)
	{
#ifdef AKPROP_TYPECHECK
		AKASSERT( typeid( T_VALUE ) == *g_AkPropTypeInfo[ in_ePropID ] );
#endif
		RANGED_MODIFIERS<T_VALUE> * pRange = (RANGED_MODIFIERS<T_VALUE> *) m_ranges.FindProp( in_ePropID );
		if (pRange)
		{
			T_VALUE fVal = RandomizerModifier::GetMod(*pRange);
			io_value += fVal;
			Monitor::LogDelta(AkDelta_RangeMod, in_ePropID, (AkReal32)fVal);
		}
	}

	template<class T_VALUE>
	AkForceInline void ApplyRange(AkPropID in_ePropID, T_VALUE & io_value)
	{
		return ApplyRange<T_VALUE, AkDeltaMonitorDisabled>(in_ePropID, io_value);
	}

// members
private:

	AkPropBundle< RANGED_MODIFIERS<AkPropValue>, AkUInt8, AkMemID_Structure > m_ranges;

	typedef AkArrayAllocatorNoAlign<AkMemID_Object> ArrayPoolStructure;
	typedef AkArray<CAkLayer*, CAkLayer*, ArrayPoolStructure> LayerList;
	LayerList* m_pAssociatedLayers;

public:
	AkUInt8 m_eCurVirtualQueueBehavior : VIRTUAL_QUEUE_BEHAVIOR_NUM_STORAGE_BIT;
	AkUInt8	m_eVirtualQueueBehavior :VIRTUAL_QUEUE_BEHAVIOR_NUM_STORAGE_BIT;
	AkUInt8	m_eBelowThresholdBehavior:BELOW_THRESHOLD_BEHAVIOR_NUM_STORAGE_BIT;
	AkUInt8	m_bOverrideAnalysis		:1;	// Analysis used for auto-normalization
	AkUInt8	m_bNormalizeLoudness	:1;
	AkUInt8	m_bEnableEnvelope		:1;
};

#endif
