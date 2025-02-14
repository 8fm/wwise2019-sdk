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
// AkParameterNode.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkParameterNode.h"

#include "AkBus.h"
#include "AkRanSeqCntr.h"
#include "AkAudioMgr.h"
#include "AkActionPlayAndContinue.h"
#include "AkLayer.h"
#include "AkMultiPlayNode.h"
#include "AkLEngineCmds.h"
#include "AkBanks.h"

CAkParameterNode::CAkParameterNode(AkUniqueID in_ulID)
:CAkParameterNodeBase(in_ulID)
,m_pAssociatedLayers(NULL)
,m_eVirtualQueueBehavior( AkVirtualQueueBehavior_FromBeginning )
,m_eBelowThresholdBehavior( AkBelowThresholdBehavior_ContinueToPlay )
,m_bOverrideAnalysis(false)
,m_bNormalizeLoudness(false)
,m_bEnableEnvelope(false)
{
}

CAkParameterNode::~CAkParameterNode()
{
	// get rid of the path played flags if any
	FreePathInfo();

	if( m_pAssociatedLayers )
	{
		m_pAssociatedLayers->Term();
		AkDelete(AkMemID_Structure, m_pAssociatedLayers);
		// m_pAssociatedLayers = NULL; // not required since in destructor and not using it afterward.
	}
}

void CAkParameterNode::SetAkProp( AkPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax )
{
	AkDeltaMonitorObjBrace braceObj(ID());
#ifdef AKPROP_TYPECHECK
	AKASSERT( typeid(AkReal32) == *g_AkPropTypeInfo[ in_eProp] );
#endif

	CAkParameterNodeBase::SetAkProp( in_eProp, in_fValue, in_fMin, in_fMax );

	if( in_fMin || in_fMax || m_ranges.FindProp( in_eProp ) )
	{
		RANGED_MODIFIERS<AkPropValue> range;
		range.m_min.fValue = in_fMin;
		range.m_max.fValue = in_fMax;

		m_ranges.SetAkProp( in_eProp, range );
	}
}

void CAkParameterNode::SetAkProp( 
		AkPropID in_eProp, 
		AkInt32 in_iValue, 
		AkInt32 in_iMin, 
		AkInt32 in_iMax 
		)
{
#ifdef AKPROP_TYPECHECK
	AKASSERT( typeid(AkInt32) == *g_AkPropTypeInfo[ in_eProp ] );
#endif

	CAkParameterNodeBase::SetAkProp( in_eProp, in_iValue, in_iMin, in_iMax );

	if( in_iMin || in_iMax || m_ranges.FindProp( in_eProp ) )
	{
		RANGED_MODIFIERS<AkPropValue> range;
		range.m_min.iValue = in_iMin;
		range.m_max.iValue = in_iMax;

		m_ranges.SetAkProp( in_eProp, range );
	}
}

AKRESULT CAkParameterNode::GetAudioParameters(AkSoundParams &io_Parameters, AkMutedMap& io_rMutedMap, const AkRTPCKey& in_rtpcKey, AkPBIModValues* io_pRanges, AkModulatorsToTrigger* in_pTriggerModulators, bool in_bDoBusCheck /*= true*/, CAkParameterNodeBase* in_pStopAtNode)
{
	AKRESULT eResult = AK_Success;	
	AkDeltaMonitorObjBrace braceDelta(key);

	if (!io_Parameters.bBypassServiced)
	{
		io_Parameters.bBypassServiced = GetBypassAllFX(in_rtpcKey.GameObj(), io_Parameters.iBypassAllFX);					
	}

	if (!io_Parameters.bNormalizationServiced && (m_bOverrideAnalysis || !m_pParentNode))
	{
		io_Parameters.bNormalizeLoudness = m_bNormalizeLoudness;
		io_Parameters.bNormalizationServiced = true;
	}

	if (io_Parameters.Request.IsSet(AkPropID_HDRActiveRange) && !io_Parameters.bHdrServiced && (GetOverrideHdrEnvelope() || !m_pParentNode))
	{		
		io_Parameters.Request.UnsetBit(AkPropID_HDRActiveRange);
		if (HasRTPC(RTPC_HDRActiveRange))		
			io_Parameters.SetNoLogHDRActiveRange(GetRTPCConvertedValue<AkDeltaMonitor>(RTPC_HDRActiveRange, in_rtpcKey));		
		else		
			io_Parameters.SetNoLogHDRActiveRange(m_props.GetAkProp(AkPropID_HDRActiveRange, AK_DEFAULT_HDR_ACTIVE_RANGE).fValue);
		
		io_Parameters.bEnableEnvelope = m_bEnableEnvelope;
		io_Parameters.bHdrServiced = true;
	}

	if (!io_Parameters.bUserDefinedServiced && (GetOverrideUserAuxSends() || !Parent()))
	{
		io_Parameters.ResetUserAuxValues();
		if (m_pAuxChunk)
		{			
			io_Parameters.Request.AddUserAuxProps();			
			for (int i = 0; i < AK_NUM_USER_AUX_SEND_PER_OBJ; ++i)
			{
				io_Parameters.aAuxSend[i] = m_pAuxChunk->aAux[i];
			}
		}
		else
		{			
			if (GetOverrideUserAuxSends())
			{				
				for (int i = 0; i < AK_NUM_USER_AUX_SEND_PER_OBJ; ++i)								
					io_Parameters.aAuxSend[i] = AK_INVALID_UNIQUE_ID;				
			}
		}		

		io_Parameters.bUserDefinedServiced = true;
	}

	// Game Defined
	if (!io_Parameters.bGameDefinedServiced && (GetOverrideGameAuxSends() || !Parent()))
	{		
		io_Parameters.SetNoLogGameAuxSendVolume(0.f);
		io_Parameters.bGameDefinedServiced = true;
		io_Parameters.bGameDefinedAuxEnabled = m_bUseGameAuxSends;
		io_Parameters.Request.AddGameDefProps();
	}

	if (!io_Parameters.bOutputBusParamServiced && m_pBusOutputNode)
	{
		// WG-33393 Reset bus values to make sure we only get the current node bus params		
		if (!in_bDoBusCheck)
		{
			io_Parameters.SetNoLogOutputBusVolume(0.0f);
			io_Parameters.SetNoLogOutputBusLPF(0.0f);
			io_Parameters.SetNoLogOutputBusHPF(0.0f);
		}
		io_Parameters.Request.AddOutputBusProps();
		
		io_Parameters.bOutputBusParamServiced = true;		
	}

	// Spatial Audio
	if (!io_Parameters.bSpatialAudioServiced && (GetOverrideReflectionsAuxBus() || !Parent()))
	{
		io_Parameters.SetNoLogReflectionBusVolume(0.f); //ReflectionBusVolume is override, not additive.
		io_Parameters.bSpatialAudioServiced = true;
		io_Parameters.reflectionsAuxBus = m_reflectionsAuxBus;
		io_Parameters.Request.AddSpatialAudioProps();
	}

	//Get ALL selected RTPCs and states.
	GetPropAndRTPCAndState(io_Parameters, in_rtpcKey);	
	
	ApplyTransitionsOnProperties(io_Parameters, io_rMutedMap, in_rtpcKey);	//Transitions

	//In all cases, we must not recheck those	
	io_Parameters.Request.RemoveServicedProps();	//Not sure i need this anymore.

	if (io_pRanges != NULL && !m_ranges.IsEmpty())
	{	
		ApplyRange<AkReal32, AkDeltaMonitor>(AkPropID_Volume, io_pRanges->VolumeOffset);
		ApplyRange<AkReal32>(AkPropID_Pitch, io_pRanges->PitchOffset);
		ApplyRange<AkReal32, AkDeltaMonitor>(AkPropID_LPF, io_pRanges->LPFOffset);
		ApplyRange<AkReal32, AkDeltaMonitor>(AkPropID_HPF, io_pRanges->HPFOffset);
		ApplyRange<AkReal32, AkDeltaMonitor>(AkPropID_MakeUpGain, io_pRanges->MakeUpGainOffset);
	}	

	//Trigger Modulators if params are passed in
	if (in_pTriggerModulators && HasModulator() )
	{
		AkModulatorSubscriberInfo subcrbrInfo;
		subcrbrInfo.pSubscriber = GetSubscriberPtr();
		subcrbrInfo.eSubscriberType = CAkRTPCMgr::SubscriberType_CAkParameterNodeBase;
		subcrbrInfo.pTargetNode = this;
		g_pModulatorMgr->GetModulators(subcrbrInfo, *in_pTriggerModulators);
	}

	if(in_bDoBusCheck && m_pBusOutputNode)
	{
		if(m_pParentNode != NULL && in_pStopAtNode != m_pParentNode)
		{
			m_pParentNode->GetAudioParameters(io_Parameters, io_rMutedMap, in_rtpcKey, io_pRanges, in_pTriggerModulators, false, in_pStopAtNode);
		}

		m_pBusOutputNode->GetAudioParameters(io_Parameters, io_rMutedMap, in_rtpcKey, io_pRanges, in_pTriggerModulators, false, in_pStopAtNode);
	}
	else
	{
		if(m_pParentNode != NULL && in_pStopAtNode != m_pParentNode)
		{
			m_pParentNode->GetAudioParameters(io_Parameters, io_rMutedMap, in_rtpcKey, io_pRanges, in_pTriggerModulators, in_bDoBusCheck, in_pStopAtNode);
		}
	}

	if( m_pAssociatedLayers )
	{
		for ( LayerList::Iterator it = m_pAssociatedLayers->Begin(), itEnd = m_pAssociatedLayers->End(); it != itEnd; ++it )
		{
			(*it)->GetAudioParameters(this, io_Parameters, io_rMutedMap, in_rtpcKey, in_pTriggerModulators);
		}
	}

	return eResult;
}

void CAkParameterNode::SetAkProp(
	AkPropID in_eProp,
	CAkRegisteredObj * in_GameObjPtr,
	AkValueMeaning in_eValueMeaning,
	AkReal32 in_fTargetValue,
	AkCurveInterpolation in_eFadeCurve,
	AkTimeMs in_lTransitionTime
	)
{
#ifndef AK_OPTIMIZED
	switch ( in_eProp )
	{
	case AkPropID_Pitch:
		{
			MONITOR_SETPARAMNOTIF_FLOAT( AkMonitorData::NotificationReason_PitchChanged, ID(), false, in_GameObjPtr?in_GameObjPtr->ID():AK_INVALID_GAME_OBJECT, in_fTargetValue, in_eValueMeaning, in_lTransitionTime );

			if(    ( in_eValueMeaning == AkValueMeaning_Offset && in_fTargetValue != 0 )
				|| ( in_eValueMeaning == AkValueMeaning_Independent && ( in_fTargetValue != m_props.GetAkProp( AkPropID_Pitch, 0.0f ).fValue ) ) ) 
			{
				MONITOR_PARAMCHANGED( AkMonitorData::NotificationReason_PitchChanged, ID(), false, in_GameObjPtr?in_GameObjPtr->ID():AK_INVALID_GAME_OBJECT );
			}
		}
		break;
	case AkPropID_Volume:
		{
			MONITOR_SETPARAMNOTIF_FLOAT( AkMonitorData::NotificationReason_VolumeChanged, ID(), false, in_GameObjPtr?in_GameObjPtr->ID():AK_INVALID_GAME_OBJECT, in_fTargetValue, in_eValueMeaning, in_lTransitionTime );

			if(    ( in_eValueMeaning == AkValueMeaning_Offset && in_fTargetValue != 0 )
				|| ( in_eValueMeaning == AkValueMeaning_Independent && in_fTargetValue != m_props.GetAkProp( AkPropID_Volume, 0.0f).fValue ) )
			{
				MONITOR_PARAMCHANGED(AkMonitorData::NotificationReason_VolumeChanged, ID(), false, in_GameObjPtr?in_GameObjPtr->ID():AK_INVALID_GAME_OBJECT );
			}
		}
		break;
	case AkPropID_LPF:
		{
			MONITOR_SETPARAMNOTIF_FLOAT( AkMonitorData::NotificationReason_LPFChanged, ID(), false, in_GameObjPtr?in_GameObjPtr->ID():AK_INVALID_GAME_OBJECT, in_fTargetValue, in_eValueMeaning, in_lTransitionTime );

			if(    ( in_eValueMeaning == AkValueMeaning_Offset && in_fTargetValue != 0 )
				|| ( in_eValueMeaning == AkValueMeaning_Independent && ( in_fTargetValue != m_props.GetAkProp( AkPropID_LPF, 0.0f).fValue ) ) )
			{
				MONITOR_PARAMCHANGED(AkMonitorData::NotificationReason_LPFChanged, ID(), false, in_GameObjPtr?in_GameObjPtr->ID():AK_INVALID_GAME_OBJECT );
			}
		}
		break;
	case AkPropID_HPF:
		{
			MONITOR_SETPARAMNOTIF_FLOAT( AkMonitorData::NotificationReason_HPFChanged, ID(), false, in_GameObjPtr?in_GameObjPtr->ID():AK_INVALID_GAME_OBJECT, in_fTargetValue, in_eValueMeaning, in_lTransitionTime );

			if(    ( in_eValueMeaning == AkValueMeaning_Offset && in_fTargetValue != 0 )
				|| ( in_eValueMeaning == AkValueMeaning_Independent && ( in_fTargetValue != m_props.GetAkProp( AkPropID_HPF, 0.0f).fValue ) ) )
			{
				MONITOR_PARAMCHANGED(AkMonitorData::NotificationReason_HPFChanged, ID(), false, in_GameObjPtr?in_GameObjPtr->ID():AK_INVALID_GAME_OBJECT );
			}
		}
		break;
	default:
		AKASSERT( false );
	}
#endif

	CAkSIS* pSIS = GetSIS( in_GameObjPtr );
	if ( pSIS )
		StartSISTransition( pSIS, in_eProp, in_fTargetValue, in_eValueMeaning, in_eFadeCurve, in_lTransitionTime );
}

void CAkParameterNode::Mute(
		CAkRegisteredObj *	in_GameObjPtr,
		AkCurveInterpolation		in_eFadeCurve /*= AkCurveInterpolation_Linear*/,
		AkTimeMs		in_lTransitionTime /*= 0*/
		)
{
	MONITOR_SETPARAMNOTIF_FLOAT( AkMonitorData::NotificationReason_Muted, ID(), false, in_GameObjPtr?in_GameObjPtr->ID():AK_INVALID_GAME_OBJECT, 0, AkValueMeaning_Default, in_lTransitionTime );
	
	MONITOR_PARAMCHANGED( AkMonitorData::NotificationReason_Muted, ID(), false, in_GameObjPtr?in_GameObjPtr->ID():AK_INVALID_GAME_OBJECT );

	CAkSIS* pSIS = GetSIS( in_GameObjPtr );
	if ( pSIS )
		StartSisMuteTransitions(pSIS,AK_MUTED_RATIO,in_eFadeCurve,in_lTransitionTime);
}

void CAkParameterNode::UnmuteAllObj(AkCurveInterpolation in_eFadeCurve,AkTimeMs in_lTransitionTime)
{
	if( m_pMapSIS )
	{
		for( AkMapSIS::Iterator iter = m_pMapSIS->Begin(); iter != m_pMapSIS->End(); ++iter )
		{
			AkSISValue * pValue = (*iter).item->m_values.FindProp( AkPropID_MuteRatio );
			if ( pValue && pValue->fValue != AK_UNMUTED_RATIO )
			{
				Unmute( (*iter).item->m_pGameObj, in_eFadeCurve,in_lTransitionTime );
			}
		}
	}
}

void CAkParameterNode::UnmuteAll(AkCurveInterpolation in_eFadeCurve,AkTimeMs in_lTransitionTime)
{
	Unmute(NULL,in_eFadeCurve,in_lTransitionTime);
	UnmuteAllObj(in_eFadeCurve,in_lTransitionTime);
}

static CAkActionPlayAndContinue* CreateDelayedAction( ContParams* in_pContinuousParams, AkPBIParams& in_rPBIParams, AkUniqueID in_uTargetElementID )
{
	CAkActionPlayAndContinue* pAction = CAkActionPlayAndContinue::Create( AkActionType_PlayAndContinue, 0, in_pContinuousParams->spContList );
	if(pAction)
	{
		pAction->SetPauseCount( in_pContinuousParams->ulPauseCount );
		pAction->SetHistory( in_rPBIParams.playHistory );
		WwiseObjectID wwiseId( in_uTargetElementID );
		pAction->SetElementID( wwiseId );
		pAction->SetInstigator( in_rPBIParams.pInstigator );
#ifndef AK_OPTIMIZED
		pAction->SetPlayTargetID( in_rPBIParams.playTargetID );
#endif
		pAction->SetPlayDirectly( in_rPBIParams.bPlayDirectly );
		pAction->SetSAInfo( in_rPBIParams.sequenceID );
		pAction->SetIsFirstPlay( in_rPBIParams.bIsFirst );
		pAction->SetInitialPlaybackState( in_rPBIParams.ePlaybackState );
		pAction->SetTransParams(in_rPBIParams.pTransitionParameters);
	}
	return pAction;
}

static AKRESULT CreateDelayedPendingAction( ContParams* in_pContinuousParams, AkPBIParams& in_rPBIParams, AkInt32 iDelaySamples, CAkActionPlayAndContinue* in_pAction )
{
	AKRESULT eResult = AK_InsufficientMemory;

	AkPendingAction* pPendingAction = AkNew(AkMemID_Object, AkPendingAction(in_rPBIParams.pGameObj));
	if( pPendingAction )
	{
		// copy the transitions we have
		if (
			in_pAction->SetPlayStopTransition( in_pContinuousParams->pPlayStopTransition, pPendingAction ) == AK_Success
			&&
			in_pAction->SetPauseResumeTransition( in_pContinuousParams->pPauseResumeTransition, pPendingAction ) == AK_Success
			)
		{
			in_pAction->SetPathInfo( in_pContinuousParams->pPathInfo );

			eResult = in_pAction->SetAkProp( AkPropID_DelayTime, iDelaySamples + (AkInt32) in_rPBIParams.uFrameOffset, 0, 0 );
			if ( eResult == AK_Success )
			{
				pPendingAction->pAction = in_pAction;
				pPendingAction->UserParam = in_rPBIParams.userParams;

				g_pAudioMgr->EnqueueOrExecuteAction( pPendingAction );
			}
			else
			{
				AkDelete(AkMemID_Object, pPendingAction );
				pPendingAction = NULL;
			}
		}
		else
		{
			AkDelete(AkMemID_Object, pPendingAction );
			pPendingAction = NULL;
		}
	}
	return eResult;
}

AKRESULT CAkParameterNode::DelayPlayback( AkReal32 in_fDelay, AkPBIParams& in_rPBIParams )
{
	AKRESULT eResult = AK_InsufficientMemory;

	ContParams* pContinuousParams = NULL;

	ContParams continuousParams;

	if( in_rPBIParams.pContinuousParams )
	{
		pContinuousParams = in_rPBIParams.pContinuousParams;
	}
	else
	{
		// Create a new ContinuousParameterSet.
		continuousParams.spContList.Attach( CAkContinuationList::Create() );
		if ( !continuousParams.spContList )
			return AK_Fail;
		pContinuousParams = &continuousParams;
	}

	CAkActionPlayAndContinue* pAction = CreateDelayedAction( pContinuousParams, in_rPBIParams, ID() );
	if(pAction)
	{
		pAction->m_delayParams = in_rPBIParams.delayParams;
		pAction->m_delayParams.bSkipDelay = true;
		pAction->m_ePBIType = in_rPBIParams.eType;

#ifndef AK_OPTIMIZED
		pAction->SetPlayTargetID( in_rPBIParams.playTargetID );
#endif
		MidiEventActionType eAction = GetMidiNoteOffAction();
		pAction->AssignMidi( eAction, in_rPBIParams.pMidiNoteState, in_rPBIParams.midiEvent );
		
		if (in_rPBIParams.pContinuousParams != NULL)
			pAction->AssignModulator(in_rPBIParams.pContinuousParams->triggeredModulators);

		eResult = CreateDelayedPendingAction( pContinuousParams, in_rPBIParams, AkTimeConv::SecondsToSamples( in_fDelay ), pAction );

		// we are done with these
		pAction->Release();
	}

	return eResult;
}

MidiPlayOnNoteType CAkParameterNode::GetMidiPlayOnNoteType() const
{
	CAkParameterNodeBase* pParent = Parent();
	if ( ! pParent || m_bOverrideMidiEventsBehavior )
	{
		AkPropID id = AkPropID_MidiPlayOnNoteType;
		AkInt32 iPlayOnNoteType = m_props.GetAkProp( id, g_AkPropDefault[id] ).iValue;
		return (MidiPlayOnNoteType)iPlayOnNoteType;
	}
	else 
	{
		return static_cast<CAkParameterNode*>(Parent())->GetMidiPlayOnNoteType();
	}
}

MidiEventActionType CAkParameterNode::GetMidiNoteOffAction() const
{
	MidiPlayOnNoteType playOnNoteType = GetMidiPlayOnNoteType();
	MidiEventActionType actionToExecute = MidiEventActionType_NoAction;

	if( playOnNoteType == MidiPlayOnNoteType_NoteOff )
		actionToExecute = MidiEventActionType_Play;
	else if( IsMidiBreakLoopOnNoteOff() )
		actionToExecute = MidiEventActionType_Break;

	return actionToExecute;
}

MidiEventActionType CAkParameterNode::GetMidiNoteOnAction() const
{
	MidiPlayOnNoteType playOnNoteType = GetMidiPlayOnNoteType();
	MidiEventActionType actionToExecute = MidiEventActionType_NoAction;

	if( playOnNoteType == MidiPlayOnNoteType_NoteOn )
		actionToExecute = MidiEventActionType_Play;

	return actionToExecute;
}

MidiEventActionType CAkParameterNode::GetMidiEventAction( const AkMidiEventEx& in_midiEvent ) const
{
	if ( in_midiEvent.IsNoteOff() )
		return GetMidiNoteOffAction();
	else
		return GetMidiNoteOnAction();
}

AKRESULT CAkParameterNode::FilterAndTransformMidiEvent( AkMidiEventEx& io_midiEvent, AkUniqueID in_midiTarget, bool& io_bMidiCheckParent, CAkRegisteredObj* in_GameObjPtr, AkPlayingID in_playingID )
{
	AKASSERT( io_midiEvent.IsValid() );
	AKASSERT( io_midiEvent.IsNoteOn() );

	AkPropID id;

	//	Check channel range
	//
	id = AkPropID_MidiChannelMask;
	AkInt32 iChannelMask = m_props.GetAkProp( id, g_AkPropDefault[id] ).iValue;

	if ( ( iChannelMask & ( 0x1 << io_midiEvent.byChan ) ) == 0 )
		 return AK_RejectedByFilter;

	// Check if parent must be consulted for transposing
	if ( io_bMidiCheckParent )
	{
		CAkParameterNode* pParent = (CAkParameterNode*)( this->Parent() );
		if ( pParent )
		{
			AKRESULT res = pParent->FilterAndTransformMidiEvent( io_midiEvent, in_midiTarget, io_bMidiCheckParent, in_GameObjPtr, in_playingID );
			if ( res != AK_Success )
				return res;
		}
		
		// Make sure no one else checks their parent for this note
		io_bMidiCheckParent = false;
	}

	// Create RTPC key
	AkRTPCKey rtpcKey( in_GameObjPtr, in_playingID, io_midiEvent.GetNoteAndChannel(), in_midiTarget );

	//	Check Note range
	//

	id = AkPropID_MidiTransposition;
	AkInt32 iTranspose = 0;
	GetPropAndRTPCAndState( iTranspose, id, rtpcKey );
	AkInt32 iTransformedNote = ((AkInt32)io_midiEvent.NoteOnOff.byNote) + iTranspose;
	iTransformedNote = AkClamp( iTransformedNote, 0, (AkInt32)AK_MIDI_MAX_NUM_NOTES-1 );

	id = AkPropID_MidiKeyRangeMin;
	AkInt32 iKeyRangeMin = m_props.GetAkProp( id, g_AkPropDefault[id] ).iValue;

	id = AkPropID_MidiKeyRangeMax;
	AkInt32 iKeyRangeMax = m_props.GetAkProp( id, g_AkPropDefault[id] ).iValue;

	if ( iTransformedNote > iKeyRangeMax || 
		 iTransformedNote < iKeyRangeMin )
		 return AK_RejectedByFilter;


	//	Check velocity range
	//

	AkInt32 iVelocityOffset = 0;
	id = AkPropID_MidiVelocityOffset;
	GetPropAndRTPCAndState( iVelocityOffset, id, rtpcKey );
	AkInt32 iTransformedVelocity = (AkInt32)io_midiEvent.NoteOnOff.byVelocity + iVelocityOffset;
	iTransformedVelocity = AkClamp( iTransformedVelocity, 1, (AkInt32)AK_MIDI_VELOCITY_MAX_VAL ); //Have to clamp to 1, because 0 -> note-off

	id = AkPropID_MidiVelocityRangeMin;
	AkInt32 iVelocityRangeMin = m_props.GetAkProp( id, g_AkPropDefault[id] ).iValue;

	id = AkPropID_MidiVelocityRangeMax;
	AkInt32 iVelocityRangeMax = m_props.GetAkProp( id, g_AkPropDefault[id] ).iValue;

	if ( iTransformedVelocity > iVelocityRangeMax || 
		 iTransformedVelocity < iVelocityRangeMin )
		 return AK_RejectedByFilter;

	// Apply MIDI transformations
	io_midiEvent.NoteOnOff.byNote = (AkUInt8)iTransformedNote;
	io_midiEvent.NoteOnOff.byVelocity = (AkUInt8)iTransformedVelocity;

	return AK_Success;
}

void CAkParameterNode::TriggerModulators( const AkModulatorTriggerParams& in_params, CAkModulatorData* out_pModPBIData, bool in_bDoBusCheck ) const
{
	CAkParameterNodeBase::TriggerModulators(in_params, out_pModPBIData, in_bDoBusCheck);

	if( m_pAssociatedLayers )
	{
		for ( LayerList::Iterator it = m_pAssociatedLayers->Begin(), itEnd = m_pAssociatedLayers->End();
			it != itEnd;
			++it )
		{
			(*it)->TriggerModulators(in_params,out_pModPBIData);
		}
	}

}

AKRESULT CAkParameterNode::PlayAndContinueAlternate( AkPBIParams& in_rPBIParams )
{
	AKRESULT eResult = AK_Fail;

    if( in_rPBIParams.pContinuousParams && in_rPBIParams.pContinuousParams->spContList )
	{
		// Set history ready for next
		AkUInt32& ulrCount = in_rPBIParams.playHistory.HistArray.uiArraySize;

		while( ulrCount )
		{
			if( in_rPBIParams.playHistory.IsContinuous( ulrCount -1 ) )
			{
				break;
			}
			else
			{
				--ulrCount;
			}
		}

		//Determine next
		AkUniqueID			NextElementToPlayID	= AK_INVALID_UNIQUE_ID;
		AkTransitionMode	l_eTransitionMode	= Transition_Disabled;
		AkReal32			l_fTransitionTime	= 0;
		AkUInt16			wPositionSelected	= 0;

		while( !in_rPBIParams.pContinuousParams->spContList->m_listItems.IsEmpty() )
		{
			CAkContinueListItem & item = in_rPBIParams.pContinuousParams->spContList->m_listItems.Last();
			if( !( item.m_pMultiPlayNode ) )
			{
				AkUniqueID uSelectedNodeID_UNUSED;
				CAkParameterNodeBase* pNode = item.m_pContainer->GetNextToPlayContinuous( in_rPBIParams.pGameObj, wPositionSelected, uSelectedNodeID_UNUSED, item.m_pContainerInfo, item.m_LoopingInfo );
				if(pNode)
				{
					in_rPBIParams.playHistory.HistArray.aCntrHist[ in_rPBIParams.playHistory.HistArray.uiArraySize - 1 ] = wPositionSelected;
					NextElementToPlayID = pNode->ID();
					pNode->Release();
					l_eTransitionMode = item.m_pContainer->TransitionMode();
					l_fTransitionTime = item.m_pContainer->TransitionTime( AkRTPCKey( in_rPBIParams.pGameObj, in_rPBIParams.userParams.PlayingID() ) );
					break;
				}
				else
				{
					in_rPBIParams.playHistory.RemoveLast();
					while( in_rPBIParams.playHistory.HistArray.uiArraySize
						&& !in_rPBIParams.playHistory.IsContinuous( in_rPBIParams.playHistory.HistArray.uiArraySize - 1 ) )
					{
						in_rPBIParams.playHistory.RemoveLast();
					}
					in_rPBIParams.pContinuousParams->spContList->m_listItems.RemoveLast();
				}
			}
			else // Encountered a switch block
			{
				item.m_pMultiPlayNode->ContGetList( item.m_pAlternateContList, in_rPBIParams.pContinuousParams->spContList );
				in_rPBIParams.pContinuousParams->spContList->m_listItems.RemoveLast();
				
				if( !in_rPBIParams.pContinuousParams->spContList )
				{
					eResult = AK_PartialSuccess;
					break;
				}
			}
		}

		//Then launch next if there is a next
		if( NextElementToPlayID != AK_INVALID_UNIQUE_ID )
		{
			// create the action we need
			CAkActionPlayAndContinue* pAction = CreateDelayedAction( in_rPBIParams.pContinuousParams, in_rPBIParams, NextElementToPlayID );
			if(pAction)
			{
				TransParams transParams;
				MidiEventActionType eAction = GetMidiNoteOffAction();
				pAction->AssignMidi( eAction, in_rPBIParams.pMidiNoteState, in_rPBIParams.midiEvent );
				pAction->AssignModulator(in_rPBIParams.pContinuousParams->triggeredModulators);

				AkInt32 iDelay;
				AkInt32 iMinimalDelay = AK_NUM_VOICE_REFILL_FRAMES * AK_WAIT_BUFFERS_AFTER_PLAY_FAILED;

				if ( l_eTransitionMode == Transition_Delay || l_eTransitionMode == Transition_TriggerRate )
				{
					iDelay = AkTimeConv::MillisecondsToSamples( l_fTransitionTime );
					if( iDelay < iMinimalDelay )
					{
						// WG-24660 - Pad the situation the user specified a Delay of 0 ms (or any transition under one frame)

						iDelay = iMinimalDelay;
					}
				}
				else
				{
					// WG-2352: avoid freeze on loop
					// WG-4724: Delay must be exactly the size of a
					//         buffer to avoid sample accurate glitches
					//         and Buffer inconsistencies
					iDelay = iMinimalDelay;
				}

				eResult = CreateDelayedPendingAction( in_rPBIParams.pContinuousParams, in_rPBIParams, iDelay, pAction );

				// we are done with these
				pAction->Release();
			}
		}
		if ( in_rPBIParams.pContinuousParams->spContList && eResult != AK_Success && eResult != AK_PartialSuccess )
		{
			in_rPBIParams.pContinuousParams->spContList = NULL;
		}

		if( eResult != AK_Success && eResult != AK_PartialSuccess )
		{
			MONITOR_OBJECTNOTIF( in_rPBIParams.userParams.PlayingID(), in_rPBIParams.pGameObj->ID(), in_rPBIParams.userParams.CustomParam(), AkMonitorData::NotificationReason_ContinueAborted, in_rPBIParams.playHistory.HistArray, ID(), false, 0 );
		}
	}

	if (eResult == AK_PartialSuccess)
	{
		eResult = AK_Success;
	}

	return eResult;
}

void CAkParameterNode::ExecuteActionExceptParentCheck( ActionParamsExcept& in_rAction )
{
	//This function is to be called when the message passes from a bus to a non-bus children.
	//The first pointed children must them run up the actor mixer hierarchy to find if it should be part of the exception.

	CAkParameterNode* pNode = (CAkParameterNode*)( this->Parent() );
	while( pNode )
	{
		if( IsException( pNode, *(in_rAction.pExeceptionList) ) )
		{
			return;// We are part of the exception.
		}
		pNode = (CAkParameterNode*)( pNode->Parent() );
	}

	// Not an exception, continue.
	ExecuteActionExcept( in_rAction );
}

AKRESULT CAkParameterNode::SetInitialParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{	
	AKRESULT eResult = m_props.SetInitialParams( io_rpData, io_rulDataSize );
	if ( eResult != AK_Success )
		return AK_Fail;

	eResult = m_ranges.SetInitialParams( io_rpData, io_rulDataSize );
	if ( eResult != AK_Success )
		return AK_Fail;

	return AK_Success;
}

AKRESULT CAkParameterNode::SetInitialFxParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize, bool in_bPartialLoadOnly)
{
	AKRESULT eResult = AK_Success;

	// Read Num Fx
	AkUInt8 bIsOverrideParentFX = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
	if(!in_bPartialLoadOnly)
		m_overriddenParams.Mask(RTPC_FX_PARAMS_BITFIELD, bIsOverrideParentFX !=0 );

	AkUInt32 uNumFx = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
	AKASSERT( uNumFx <= AK_NUM_EFFECTS_PER_OBJ );
	if ( uNumFx )
	{
		AkUInt32 bitsFXBypass = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );

		for ( AkUInt32 uFX = 0; uFX < uNumFx; ++uFX )
		{
			AkUInt32 uFXIndex = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
			AkUniqueID fxID = READBANKDATA( AkUniqueID, io_rpData, io_rulDataSize);
			AkUInt8 bIsShareSet = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
			AkUInt8 bIsRendered = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
			RenderedFX( uFXIndex, ( bIsRendered != 0 ) );

			if ( !bIsRendered )
			{
				// Read Size
				if(fxID != AK_INVALID_UNIQUE_ID && 
				   !in_bPartialLoadOnly)
				{
					eResult = SetFX( uFXIndex, fxID, bIsShareSet != 0, SharedSetOverride_Bank );
				}
			}

			if( eResult != AK_Success )
				break;
		}

		if(!in_bPartialLoadOnly)
			MainBypassFX( bitsFXBypass );
	}

	return eResult;
}

void CAkParameterNode::OverrideFXParent( bool in_bIsFXOverrideParent )
{
#ifndef AK_OPTIMIZED
	if ( m_pFXChunk )
	{
		for( int i = 0; i < AK_NUM_EFFECTS_PER_OBJ; ++i )
		{
			if( m_pFXChunk->aFX[ i ].bRendered )
				return;
		}
	}
#endif
	m_overriddenParams.Mask(RTPC_FX_PARAMS_BITFIELD, in_bIsFXOverrideParent);
}

// Returns true if the Context may jump to virtual voices, false otherwise.
AkBelowThresholdBehavior CAkParameterNode::GetVirtualBehavior( AkVirtualQueueBehavior& out_Behavior ) const
{
	if ( m_bIsVVoicesOptOverrideParent || Parent() == NULL )
	{
		out_Behavior = (AkVirtualQueueBehavior)m_eVirtualQueueBehavior;
		return (AkBelowThresholdBehavior)m_eBelowThresholdBehavior;
	}
	else
	{
		return static_cast<CAkParameterNode*>(Parent())->GetVirtualBehavior( out_Behavior );
	}
}

AKRESULT CAkParameterNode::SetAdvSettingsParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	AkUInt8 byBitVector = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
	bool bKillNewest =					GETBANKDATABIT( byBitVector, BANK_BITPOS_ADVSETTINGS_KILL_NEWEST );
	bool bUseVirtualBehavior =			GETBANKDATABIT( byBitVector, BANK_BITPOS_ADVSETTINGS_USE_VIRTUAL );
	bool bMaxNumInstIgnoreParent =	GETBANKDATABIT( byBitVector, BANK_BITPOS_ADVSETTINGS_MAXNUMINST_IGNORE );
	bool bVVoicesOptOverrideParent =	GETBANKDATABIT( byBitVector, BANK_BITPOS_ADVSETTINGS_VIRTVOICEOPT_OVERRIDE );

	AkVirtualQueueBehavior eVirtualQueueBehavior = (AkVirtualQueueBehavior) READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
	// Do not use SetMaxNumInstances() here to avoid useless processing on load bank.
	m_bIsGlobalLimit =					GETBANKDATABIT( byBitVector, BANK_BITPOS_ADVSETTINGS_GLOBAL_LIMIT );
	m_u16MaxNumInstance	=				READBANKDATA( AkUInt16, io_rpData, io_rulDataSize );
	AkBelowThresholdBehavior eBelowThresholdBehavior = (AkBelowThresholdBehavior) READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );

	// HDR. Here in advanced settings. Must be here and not in base (see AKBKParameterNode), and 
	// adding a virtual call for these 4 bits is inefficient and would not be clearer/cleaner.
	byBitVector = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
	bool bOverrideHdrEnvelope =			GETBANKDATABIT( byBitVector, BANK_BITPOS_HDR_ENVELOPE_OVERRIDE );
	m_bOverrideAnalysis =				GETBANKDATABIT( byBitVector, BANK_BITPOS_HDR_ANALYSIS_OVERRIDE );
	m_bNormalizeLoudness =				GETBANKDATABIT( byBitVector, BANK_BITPOS_HDR_NORMAL_LOUDNESS );
	m_bEnableEnvelope =					GETBANKDATABIT( byBitVector, BANK_BITPOS_HDR_ENVELOPE_ENABLE );

	SetVirtualQueueBehavior( eVirtualQueueBehavior );
	SetMaxReachedBehavior( bKillNewest );
	SetOverLimitBehavior( bUseVirtualBehavior );
	SetBelowThresholdBehavior( eBelowThresholdBehavior );
	SetMaxNumInstIgnoreParent( bMaxNumInstIgnoreParent );
	SetVVoicesOptOverrideParent( bVVoicesOptOverrideParent );
	SetOverrideHdrEnvelope( bOverrideHdrEnvelope );

	return AK_Success;
}

void CAkParameterNode::GetFX(
		AkUInt32	in_uFXIndex,
		AkFXDesc&	out_rFXInfo,
		CAkRegisteredObj *	in_GameObj /*= NULL*/
		)
{
	if( GetFXOverrideParent() || !Parent() )
	{
		AKASSERT( in_uFXIndex < AK_NUM_EFFECTS_PER_OBJ );

		if ( m_pFXChunk )
		{
			FXStruct & fx = m_pFXChunk->aFX[ in_uFXIndex ];
			if ( fx.id != AK_INVALID_UNIQUE_ID )
			{
				if ( fx.bShareSet )
					out_rFXInfo.pFx.Attach( g_pIndex->m_idxFxShareSets.GetPtrAndAddRef( fx.id ) );
				else
					out_rFXInfo.pFx.Attach( g_pIndex->m_idxFxCustom.GetPtrAndAddRef( fx.id ) );
			}
			else
			{
				out_rFXInfo.pFx = NULL;
			}

			out_rFXInfo.iBypassed = GetBypassFX( in_uFXIndex, in_GameObj );
		}
		else
		{
			out_rFXInfo.pFx = NULL;
			out_rFXInfo.iBypassed = 0;
		}
	}
	else
	{
		Parent()->GetFX( in_uFXIndex, out_rFXInfo, in_GameObj );
	}
}

void CAkParameterNode::GetFXDataID(
		AkUInt32	in_uFXIndex,
		AkUInt32	in_uDataIndex,
		AkUInt32&	out_rDataID
		)
{
	if( GetFXOverrideParent() || !Parent() )
	{
		AKASSERT( in_uFXIndex < AK_NUM_EFFECTS_PER_OBJ );

		out_rDataID = AK_INVALID_SOURCE_ID;

		if ( m_pFXChunk )
		{
			FXStruct & fx = m_pFXChunk->aFX[ in_uFXIndex ];

			CAkFxBase * pFx;
			if ( fx.bShareSet )
				pFx = g_pIndex->m_idxFxShareSets.GetPtrAndAddRef( fx.id );
			else
				pFx = g_pIndex->m_idxFxCustom.GetPtrAndAddRef( fx.id );

			if ( pFx )
			{
				out_rDataID = pFx->GetMediaID( in_uDataIndex );
				pFx->Release();
			}
		}
	}
	else
	{
		Parent()->GetFXDataID( in_uFXIndex, in_uDataIndex, out_rDataID );
	}
}

bool CAkParameterNode::GetBypassAllFX(
		CAkRegisteredObj * in_GameObjPtr, AkInt16& out_iBypassAllFx )
{
	bool bOverrideOrTopNode = ( GetFXOverrideParent() || !Parent() );

	if (bOverrideOrTopNode)
	{
		out_iBypassAllFx = _GetBypassAllFX(in_GameObjPtr);
	}

	return bOverrideOrTopNode;
}

AKRESULT CAkParameterNode::AssociateLayer( CAkLayer* in_pLayer )
{
	if( !m_pAssociatedLayers )
	{
		m_pAssociatedLayers = AkNew(AkMemID_Structure, LayerList());
	}

	if ( !m_pAssociatedLayers || !m_pAssociatedLayers->AddLast( in_pLayer ) )
	{
		if( m_pAssociatedLayers && m_pAssociatedLayers->IsEmpty() )
		{
			AkDelete(AkMemID_Structure, m_pAssociatedLayers);
			m_pAssociatedLayers = NULL;
		}
		return AK_InsufficientMemory;
	}

	RecalcNotification( false );

	return AK_Success;
}

AKRESULT CAkParameterNode::DissociateLayer( CAkLayer* in_pLayer )
{
	AKRESULT eResult = AK_Fail;

	if( m_pAssociatedLayers )
	{
		eResult = m_pAssociatedLayers->Remove( in_pLayer );

		if ( eResult == AK_Success )
		{
			RecalcNotification( false );
		}
	}

	return eResult;
}

AKRESULT CAkParameterNode::IncrementPlayCount(CounterParameters& io_params, bool in_bNewInstance, bool in_bSwitchingOutputBus)
{
	AKRESULT eResult = AK_Success;
	
	if ( in_bNewInstance )
		eResult = IncrementPlayCountValue( io_params.uiFlagForwardToBus & AK_ForwardToBusType_Normal );
	// We must continue and go up to the top even if IncrementPlayCountValue failed.
	// Decrement will soon be called and will go to the top too, so it must be done all the way up even in case of failure.

	if ( eResult == AK_Success )
	{
		if ( !io_params.bMaxConsidered )
		{
			if( IsGlobalLimit() )
			{
				if ( !in_bSwitchingOutputBus )
					eResult = ProcessGlobalLimiter( io_params, in_bNewInstance );
			}
			else
				eResult = ProcessGameObjectLimiter( io_params, in_bNewInstance );

			io_params.bMaxConsidered = MaxNumInstIgnoreParent();
		}
	}

	// bMaxConsidered must be set to false before passing it to Busses
	// We will reset it later before passing to parents.
	bool bMaxConsideredBackup = io_params.bMaxConsidered;

	if( io_params.uiFlagForwardToBus & AK_ForwardToBusType_Normal )
	{
		if(m_pBusOutputNode)
		{
			io_params.uiFlagForwardToBus &= ~AK_ForwardToBusType_Normal;
			io_params.bMaxConsidered = false;

			AKRESULT newResult = m_pBusOutputNode->IncrementPlayCount( io_params, in_bNewInstance , in_bSwitchingOutputBus );
			eResult = GetNewResultCodeForVirtualStatus( eResult, newResult );
		}
	}

	if(m_pParentNode)
	{
		//Restore bMaxConsideredBackup;
		io_params.bMaxConsidered = bMaxConsideredBackup;
		AKRESULT newResult = m_pParentNode->IncrementPlayCount( io_params, in_bNewInstance, in_bSwitchingOutputBus );
		eResult = GetNewResultCodeForVirtualStatus( eResult, newResult );
	}

	return eResult;
}

void CAkParameterNode::DecrementPlayCount( 
	CounterParameters& io_params
	)
{
	DecrementPlayCountValue( io_params.uiFlagForwardToBus & AK_ForwardToBusType_Normal );

	if( IsActivityChunkEnabled() )
	{
		if( IsGlobalLimit() )
		{
			CheckAndDeleteActivityChunk();
		}
		else
		{
			CheckAndDeleteGameObjLimiter(io_params.pGameObj );
		}
	}

	// bMaxConsidered must be set to false before passing it to Busses
	// We will reset it later before passing to parents.
	bool bMaxConsideredBackup = io_params.bMaxConsidered;

	if( io_params.uiFlagForwardToBus & AK_ForwardToBusType_Normal )
	{
		if(m_pBusOutputNode)
		{
			io_params.uiFlagForwardToBus &= ~AK_ForwardToBusType_Normal;
			io_params.bMaxConsidered = false;
			m_pBusOutputNode->DecrementPlayCount( io_params );
		}
	}

	if(m_pParentNode)
	{
		io_params.bMaxConsidered = bMaxConsideredBackup;
		m_pParentNode->DecrementPlayCount( io_params );
	}
}

void CAkParameterNode::ApplyMaxNumInstances( AkUInt16 in_u16MaxNumInstance )
{
	/// Wwise is calling to update the value, and we are not driven by RTPC, so we update every game object.
	if( IsActivityChunkEnabled() )
	{
		if( IsGlobalLimit() )
		{
			UpdateMaxNumInstanceGlobal( in_u16MaxNumInstance );
		}
		else
		{
			for( AkPerObjPlayCount::Iterator iterMax = GetActivityChunk()->m_ListPlayCountPerObj.Begin(); 
				iterMax != GetActivityChunk()->m_ListPlayCountPerObj.End(); 
				++iterMax )
			{
				iterMax.pItem->item.SetMax( in_u16MaxNumInstance );
			}
		}
	}
	m_u16MaxNumInstance = in_u16MaxNumInstance; // Only set it when not from RTPC.
}

bool CAkParameterNode::IsOrIsChildOf( CAkParameterNodeBase * in_pNodeToTest, bool )
{
	bool bRet = false;
	bool l_bIsBusChecked = false;
	CAkParameterNode* pNode = this;
	
	while( !bRet && pNode )
	{
		bRet = in_pNodeToTest == pNode;
		if( !bRet && !l_bIsBusChecked && pNode->ParentBus() != NULL )
		{
			bRet = static_cast<CAkBus*>( pNode->ParentBus() )->IsOrIsChildOf( in_pNodeToTest );
			l_bIsBusChecked = true;
		}
		pNode = static_cast<CAkParameterNode*>( pNode->Parent() );
	}
	return bRet;
}

void CAkParameterNode::SetOverrideHdrEnvelope( bool in_bOverride )
{
	m_overriddenParams.Set(RTPC_HDRActiveRange, in_bOverride);
	RecalcNotification( false );
}

void CAkParameterNode::SetOverrideAnalysis( bool in_bOverride )
{
	m_bOverrideAnalysis = in_bOverride;
	RecalcNotification( false );
}

void CAkParameterNode::SetNormalizeLoudness( bool in_bNormalizeLoudness )
{
	m_bNormalizeLoudness = in_bNormalizeLoudness;
	RecalcNotification( false );
}

void CAkParameterNode::SetEnableEnvelope( bool in_bEnableEnvelope )
{
	m_bEnableEnvelope = in_bEnableEnvelope;
	RecalcNotification( false );
}


AKRESULT CAkParameterNode::HandleInitialDelay( AkPBIParams& in_rPBIParams )
{
	if ( in_rPBIParams.bPlayDirectly )
		return AK_Success;

	AKRESULT eResult = AK_Success;
	if( in_rPBIParams.delayParams.bSkipDelay )
	{
		in_rPBIParams.delayParams.bSkipDelay = false;
	}
	else//if( DelayedSafetyBit ) // TODO OptimizationUsefulHere.....
	{
		AkReal32 fDelay = 0.f;
		GetPropAndRTPCAndState( fDelay, AkPropID_InitialDelay, AkRTPCKey(in_rPBIParams.pGameObj,in_rPBIParams.userParams.PlayingID()) );
		ApplyRange<AkReal32>( AkPropID_InitialDelay, fDelay );
		if( fDelay > 0.f )
		{
			if( in_rPBIParams.sequenceID != AK_INVALID_SEQUENCE_ID )
			{
				// Use the frame Offset system when workign with sample accurate sounds.
				AkUInt32 uDelayInSamples = AkTimeConv::SecondsToSamples( fDelay );
				in_rPBIParams.uFrameOffset += uDelayInSamples;
			}
			else
			{
				// Use the existing PlayAndContinue Action system for all others situations.
				eResult = DelayPlayback( fDelay, in_rPBIParams );
				if( eResult == AK_Success )
					return AK_PartialSuccess;// Here, partial success means the playback was successfully delayed. Consider as success but dont play it!
			}
		}
	}
	return eResult;
}


void CAkParameterNode::IncrementLESyncCount()
{
	CAkLEngineCmds::IncrementSyncCount();
}
