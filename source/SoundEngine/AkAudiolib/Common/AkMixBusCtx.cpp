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
// AkMixBusCtx.cpp
//
//////////////////////////////////////////////////////////////////////

#include <stdafx.h>
#include "AkVPLMixBusNode.h"
#include "AkMixBusCtx.h"
#include "AkBus.h"
#include "AkRegistryMgr.h"
#include "AkSpatialAudioVoice.h"

// Global initialized flag to know when spatial audio is in use.
extern bool g_bSpatialAudioInitialized;

CAkMixBusCtx::AkListLightCtxs CAkMixBusCtx::s_AuxRoutableBusses;

CAkMixBusCtx::CAkMixBusCtx(CAkRegisteredObj* in_pGameObj, CAkBus* in_pBus) : CAkBehavioralCtx(in_pGameObj, in_pBus, false)
	, m_pMixBus(NULL)
	, pPrevAuxRoutable(NULL)
#ifndef AK_OPTIMIZED
	, m_bMonitoringMute(false)
#endif
{	
	pNextLightItem = NULL;
}

CAkMixBusCtx::~CAkMixBusCtx()
{
}

AKRESULT CAkMixBusCtx::Init()
{
	// For now do not call up to CAkBehavioralCtx, but override Init() behavior.
	//CAkBehavioralCtx::Init();

	bool bIsIncrementSuccessful = GetBus()->IncrementActivityCount();
	if (!bIsIncrementSuccessful)
		return AK_Fail;

	RegisterParamTarget( m_pParamNode );

	InitSpatialAudioVoice();

	return AK_Success;
}

#ifndef AK_OPTIMIZED
void CAkMixBusCtx::ReportSpatialAudioErrorCodes()
{
	if (g_bSpatialAudioInitialized &&
		m_BasePosParams.bHasListenerRelativeRouting &&
		!GetGameObjectPtr()->HasComponent(GameObjComponentIdx_SpatialAudioRoom) &&
		m_posParams.settings.m_e3DPositionType == AK_3DPositionType_ListenerWithAutomation)
	{
		const AkSoundParams& params = GetEffectiveParams();
		if (params.reflectionsAuxBus != AK_INVALID_AUX_ID)
		{
			MONITOR_ERROREX(AK::Monitor::ErrorCode_SpatialAudio_ListenerAutomationNotSupported, AK_INVALID_PLAYING_ID, GetGameObjectPtr()->ID(), GetSoundID(), false);
		}
	}
}
#endif

void CAkMixBusCtx::Term(bool in_bFailedToInit)
{
	UnregisterParamTarget();
	GetBus()->DecrementActivityCount();

	RemoveFromAuxList();

	CAkBehavioralCtx::Term(in_bFailedToInit);
}

AkRTPCBitArray CAkMixBusCtx::GetTargetedParamsSet()
{
	return AkRTPCBitArray(RTPC_MIXBUS_PARAMS_BITFIELD);
}

void CAkMixBusCtx::NotifyParamChanged(bool in_bLiveEdit, AkRTPC_ParameterID in_rtpcID)
{
	m_bAreParametersValid = false;
	GetBus()->SetHdrCompressorDirty();
}

void CAkMixBusCtx::NotifyParamsChanged( bool in_bLiveEdit, const AkRTPCBitArray& in_bitArray )
{
	m_bAreParametersValid = false;
	GetBus()->SetHdrCompressorDirty();
}

void CAkMixBusCtx::RefreshParameters(AkInitialSoundParams* in_pInitialSoundParams)
{
	OpenSoundBrace();

	m_EffectiveParams.ClearEx();

	m_pParamNode->UpdateBaseParams(GetRTPCKey(), &m_BasePosParams, m_p3DAutomation);

	m_Prev2DParams.Invalidate();

	GetBus()->GetMixingBusParameters(m_EffectiveParams, GetRTPCKey());

	RefreshBypassFx();

	if (m_bRefreshAllAfterRtpcChange)
	{
		AkModulatorTriggerParams modTriggerParams;
		GetModulatorTriggerParams(modTriggerParams);
		m_pParamNode->TriggerModulators(modTriggerParams, &m_ModulatorData, true);
		m_pParamNode->GetPositioningParams(GetRTPCKey(), m_BasePosParams, m_posParams);
		m_bRefreshAllAfterRtpcChange = false;
	}

	m_bGetAudioParamsCalled = true;
	m_bAreParametersValid = true;

	if (IsAuxRoutable())
		AddToAuxList();

	CloseSoundBrace();

	SpatialAudioParamsUpdated();
}

void CAkMixBusCtx::UpdateTargetParam(AkRTPC_ParameterID in_eParam, AkReal32 in_fValue, AkReal32 in_fDelta)
{
	AkDeltaMonitor::OpenRTPCTargetBrace(m_pParamNode->ID(), m_PipelineID);
	switch(in_eParam)
	{
		// Bus volume changes always apply to "this" bus, because RTPCs make them mixing.
		case RTPC_BusVolume:
			AkDeltaMonitor::LogUpdate(AkPropID_BusVolume, in_fValue);
			m_EffectiveParams.Accumulate(AkPropID_Volume, in_fDelta, AkDelta_None);
			break;

		case RTPC_BypassFX0:
		case RTPC_BypassFX1:
		case RTPC_BypassFX2:
		case RTPC_BypassFX3:
		{
			AkUInt32 uFXIdx = in_eParam - RTPC_BypassFX0;
			m_pMixBus->UpdateBypass(uFXIdx, (AkInt16)in_fDelta);
			break;
		}
		case RTPC_BypassAllFX:
		{
			AkInt16 iBypass = m_pMixBus->GetFxBypassAll() + (AkInt16)in_fDelta;
			m_pMixBus->SetFxBypassAll(iBypass);
			break;
		}
		default:
			CAkBehavioralCtx::UpdateTargetParam( in_eParam, in_fValue, in_fDelta );
			break;
	}
	AkDeltaMonitor::CloseRTPCTargetBrace();
}

void CAkMixBusCtx::RefreshBypassFx()
{
	CAkBus* pBus = GetBus();

	if ( m_pMixBus && pBus )
	{
		for ( AkUInt32 i = 0; i < AK_NUM_EFFECTS_PER_OBJ; ++i )
		{
			AkInt16 bypass = pBus->GetBypassFX( i, GetRTPCKey().GameObj() );
			m_pMixBus->SetFxBypass( i, bypass );
		}

		{
			AkInt16 bypass = pBus->GetBypassAllFX( GetRTPCKey().GameObj() );
			m_pMixBus->SetFxBypassAll( bypass );
		}
	}
}

void CAkMixBusCtx::ManageAuxRoutableBusses()
{
	CAkBehavioralCtx* pFirstCtx;
	do
	{
		pFirstCtx = s_AuxRoutableBusses.First();

		for (AkListLightCtxs::IteratorEx it = s_AuxRoutableBusses.BeginEx(); it != s_AuxRoutableBusses.End(); )
		{
			CAkMixBusCtx* pMixBusCtx = (CAkMixBusCtx*)(*it);
			CAkVPLMixBusNode* pMixBus = pMixBusCtx->m_pMixBus;

			if (!pMixBus->ManageAuxSends())
			{
				if (pMixBusCtx->pNextLightItem != NULL)
					((CAkMixBusCtx*)pMixBusCtx->pNextLightItem)->pPrevAuxRoutable = pMixBusCtx->pPrevAuxRoutable;

				it = s_AuxRoutableBusses.Erase(it);
				pMixBusCtx->pNextLightItem = pMixBusCtx->pPrevAuxRoutable = NULL;
			}
			else
				++it;
		}
	} 
	while (pFirstCtx != s_AuxRoutableBusses.First()); // If Aux Busses were created, we need another pass to handle _their_ sends
}

#ifndef AK_OPTIMIZED
void CAkMixBusCtx::RefreshMonitoringMuteSolo()
{
	// Retrieve mute/solo state from this node in the bus hierarchy.
	bool bNodeSolo = false, bNodeMute = false;
	m_pParamNode->GetMonitoringMuteSoloState(bNodeSolo, bNodeMute);

	// Retrieve control bus mute/solo state.  This will only be applied to the direct output (not aux sends).
	bool bCtrlBusSolo = false, bCtrlBusMute = false;
	m_pParamNode->GetCtrlBusMonitoringMuteSoloState(bCtrlBusSolo, bCtrlBusMute);

	//Retrieve mute/solo state from game object.
	bool bGameObjSoloExplicit = GetGameObjectPtr()->IsSoloExplicit();
	bool bGameObjSoloImplicit = GetGameObjectPtr()->IsSoloImplicit();
	bool bGameObjMute = GetGameObjectPtr()->IsMute();

	m_bMonitoringMute = bNodeMute || bGameObjMute;

	bool bOverallSolo = (bGameObjSoloExplicit && (!CAkParameterNodeBase::IsBusMonitoringSoloActive() || bNodeSolo)) ||
		 (bNodeSolo && (!g_pRegistryMgr->IsMonitoringSoloActive() || bGameObjSoloExplicit || bGameObjSoloImplicit));
	
	m_pMixBus->SetMonitoringMuteSolo_Bus(
		m_bMonitoringMute, 
		bOverallSolo, // -- will be propagated through graph.
		bCtrlBusMute,
		bCtrlBusSolo,
		bGameObjSoloImplicit && (!CAkParameterNodeBase::IsBusMonitoringSoloActive() || bNodeSolo) // Implicit game object solo, only affects local connections (not propagated)
		);
}
#endif

void CAkMixBusCtx::RemoveFromAuxList()
{
	if (pPrevAuxRoutable != NULL || pNextLightItem != NULL || s_AuxRoutableBusses.First() == this)
	{
		if (pNextLightItem != NULL)
			((CAkMixBusCtx*)pNextLightItem)->pPrevAuxRoutable = pPrevAuxRoutable;

		s_AuxRoutableBusses.RemoveItem(this, pPrevAuxRoutable);
		pNextLightItem = pPrevAuxRoutable = NULL;
	}
}

void CAkMixBusCtx::AddToAuxList()
{
	if (pPrevAuxRoutable == NULL && pNextLightItem == NULL && s_AuxRoutableBusses.First() != this)
	{
		if (s_AuxRoutableBusses.First() != NULL)
			((CAkMixBusCtx*)s_AuxRoutableBusses.First())->pPrevAuxRoutable = this;

		s_AuxRoutableBusses.AddFirst(this);
	}
}
