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
// AkPositionedCtx.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include "AkBehavioralCtx.h"
#include "AkRegisteredObj.h"
#include "AkSpatialAudioComponent.h"
#include "AkParameterNodeBase.h"
#include "AkPathManager.h"
#include "Ak3DListener.h"
#include "AkSpeakerPan.h"
#include "AkSpatialAudioVoice.h"
#include "AkPipelineID.h"

CAkBehavioralCtx::CAkBehavioralCtx(CAkRegisteredObj* in_pGameObj, CAkParameterNodeBase* pSoundNode, bool in_bPlayDirectly)
	: CAkParameterTarget(in_pGameObj)	
	, m_p3DAutomation(NULL)
	, m_pParamNode(pSoundNode)
	, m_pSpatialAudioVoice(NULL)
	, m_fMaxDistance(0.0f)
	, m_PipelineID(AkPipelineIDGenerator::GetNext())
	, m_bAreParametersValid(false)
	, m_bGetAudioParamsCalled(false)
	, m_bRefreshAllAfterRtpcChange(false)
	, m_bGameObjPositionCached(false)
	, m_bFadeRatioDirty(true)
	, m_bIsAutomationOrAttenuationDirty(false)
	, m_bPlayDirectly(in_bPlayDirectly)
	, m_bIsHDR(false)
	, m_bIsForcedToVirtualizeForLimiting(false)
	, m_bIsVirtualizedForInterruption(false)
	, m_fCachedOutputBusVolumeLin(DBTOLIN_ZERO)
	, m_fLastOutputBusVolume(0.0f)

{
	m_PathInfo.pPBPath = NULL;
	m_PathInfo.PathOwnerID = AK_INVALID_UNIQUE_ID;

	m_bIsHDR = pSoundNode->IsInHdrHierarchy();

	GetGameObjectPtr()->AddRef();

	pSoundNode->AddRef();

	m_Prev2DParams.Invalidate();
}


CAkBehavioralCtx::~CAkBehavioralCtx()
{
	AKASSERT(m_pSpatialAudioVoice == NULL);

	m_arVolumeData.Term();
	m_cachedGameObjectPosition.Term();
}


void CAkBehavioralCtx::Term(bool in_bFailedToInit)
{
	if (m_pSpatialAudioVoice != NULL)
	{
		AkDelete(AkMemID_Object, m_pSpatialAudioVoice);
		m_pSpatialAudioVoice = NULL;
	}

	if (m_PathInfo.pPBPath != NULL)
	{
		// if continous then the path got rid of the played flags
		if (m_PathInfo.pPBPath->IsContinuous())
		{
			AkPathState* pPathState = m_pParamNode->GetPathState();
			pPathState->pbPlayed = NULL;
			pPathState->ulCurrentListIndex = 0;
		}
		g_pPathManager->RemovePathUser(m_PathInfo.pPBPath, this);
		m_PathInfo.pPBPath = NULL;
		m_PathInfo.PathOwnerID = AK_INVALID_UNIQUE_ID;
	}

	UnregisterParamTarget();

	GetGameObjectPtr()->Release();	

	Reset3DParams();

	m_pParamNode->Release();
}

// Question Why is there a Pre-Init and an Init?
// Answer:
// Preinit must make volume calculation to call IsInitiallyUnderThreshold possible before Real Init is called.
// The check must be performed BEFORE the whole IncrementPlaycount mechanism be done before then calling the real init function.

AKRESULT CAkBehavioralCtx::PreInit(AkReal32 in_MaxDistance, AkPathInfo* in_pPathInfo, bool in_bAllowedToPlayIfUnderThreshold, AkMonitorData::NotificationReason& io_eReason, AkInitialSoundParams* in_pInitialParams, bool& out_bIsInitiallyUnderThreshold)
{
	m_fMaxDistance = in_MaxDistance;

	AKRESULT eResult = AK_Success;

	// Setup 3D sound.
	Get3DParams();

	CAkAttenuation* pAttenuation = m_posParams.GetRawAttenuation();
	if (pAttenuation)
	{
		eResult = UpdateAttenuation(pAttenuation);
		if (eResult != AK_Success)
			return eResult;
	}
		
	if (m_p3DAutomation)
		eResult = SetupPath(in_pPathInfo, false);
	
	out_bIsInitiallyUnderThreshold = IsInitiallyUnderThreshold(in_pInitialParams);
	if (eResult == AK_Success && !in_bAllowedToPlayIfUnderThreshold && out_bIsInitiallyUnderThreshold && !m_bPlayDirectly)
	{
		io_eReason = AkMonitorData::NotificationReason_KilledVolumeUnderThreshold;
		eResult = AK_PartialSuccess;
	}

	return eResult;
}

AKRESULT CAkBehavioralCtx::Init()
{
	m_pParamNode->AddPBI(this);

	RegisterParamTarget( m_pParamNode );

	return AK_Success;
}

void CAkBehavioralCtx::InitSpatialAudioVoice()
{
	m_pSpatialAudioVoice = CAkSpatialAudioVoice::Create(this);

#ifndef AK_OPTIMIZED
	ReportSpatialAudioErrorCodes();
#endif
}

bool CAkBehavioralCtx::IsInitiallyUnderThreshold(AkInitialSoundParams* in_pInitialSoundParams)
{
	//must gather params since that have never been queried yet

	CalcEffectiveParams(in_pInitialSoundParams);

	return (ComputeBehavioralNodeVolume() <= g_fVolumeThreshold);
}

AKRESULT CAkBehavioralCtx::SetupPath(AkPathInfo* in_pPathInfo, bool in_bStart)
{
	if (m_PathInfo.pPBPath)
		return AK_Success;//Already Setup.

	AKRESULT eResult = Init3DPath(in_pPathInfo);

	if (eResult == AK_Success && m_PathInfo.pPBPath)
	{
		eResult = Recalc3DPath();

		if (eResult == AK_Success && in_bStart)
			StartPath();
	}
	return eResult;
}


AKRESULT CAkBehavioralCtx::Init3DPath(AkPathInfo* in_pPathInfo)
{
	if (!m_posParams.settings.HasAutomation() || !m_p3DAutomation)
		return AK_Success;

	Ak3DAutomationParams & r3DParams = m_p3DAutomation->GetParams();

	AKRESULT eResult = AK_Success;

	// get ID
	AkUniqueID PathOwnerID = m_p3DAutomation->GetPathOwner();

	// got one ?
	if (in_pPathInfo && in_pPathInfo->pPBPath)
	{
		// same owner ?
		if (in_pPathInfo->PathOwnerID == PathOwnerID)
		{
			// use this path
			m_PathInfo.pPBPath = in_pPathInfo->pPBPath;
			// keep the id
			m_PathInfo.PathOwnerID = in_pPathInfo->PathOwnerID;
		}
	}

	// already got one ?				
	//If we are in continuous mode, the StepNewSound option must not be there.
	AKASSERT((r3DParams.m_ePathMode & (AkPathContinuous | AkPathStepNewSound)) != (AkPathContinuous | AkPathStepNewSound));

	if (m_p3DAutomation->HasPathPlayList() &&
		(m_PathInfo.pPBPath == NULL || r3DParams.m_ePathMode & AkPathStepNewSound))
	{
		// no, get a path from the manager
		m_PathInfo.pPBPath = g_pPathManager->AddPathToList();

		// if we've got one then proceed
		if (m_PathInfo.pPBPath != NULL)
		{
			// set m_pPath according to what's in the sound
			eResult = m_p3DAutomation->SetPathPlayList(m_PathInfo.pPBPath, m_pParamNode->GetPathState());

			if (eResult != AK_Success)
			{
				g_pPathManager->RemovePathFromList(m_PathInfo.pPBPath);

				m_PathInfo.pPBPath = NULL;
				PathOwnerID = AK_INVALID_UNIQUE_ID;
			}
			// keep the id
			m_PathInfo.PathOwnerID = PathOwnerID;
		}
	}

	if (m_posParams.settings.m_bHoldListenerOrient && m_PathInfo.pPBPath != NULL)
		m_PathInfo.pPBPath->InitRotationMatricesForNoFollowMode(GetGameObjectPtr()->GetListeners());

	return eResult;
}

AKRESULT CAkBehavioralCtx::Recalc3DPath()
{
	if (m_PathInfo.pPBPath)
	{
		AKRESULT tempResult = g_pPathManager->AddPathUser(m_PathInfo.pPBPath, this);
		if (tempResult == AK_Fail)
		{
			m_PathInfo.pPBPath = NULL;
		}
		else
		{
			OnPathAdded(m_PathInfo.pPBPath);
		}
		return tempResult;
	}
	return AK_Success;
}

void CAkBehavioralCtx::OnPathAdded(CAkPath* pPBPath)
{
	//Set parameters of the path that are used to identify it to the profiler.
	pPBPath->SetSoundUniqueID(m_pParamNode->ID());
}


void CAkBehavioralCtx::StartPath()
{
	if (GetPathInfo()->pPBPath != NULL)
		g_pPathManager->Start(m_PathInfo.pPBPath, m_pParamNode->GetPathState());
}

void CAkBehavioralCtx::GetModulatorTriggerParams(AkModulatorTriggerParams& out_params, bool in_bFirstPlay)
{
	out_params.pGameObj = GetGameObjectPtr();
	out_params.eTriggerMode =
		(in_bFirstPlay ?
		AkModulatorTriggerParams::TriggerMode_FirstPlay :
		AkModulatorTriggerParams::TriggerMode_ParameterUpdated
		);
}

// Trigger the release portion of the envelope modulators.
void CAkBehavioralCtx::ReleaseModulators()
{
	m_ModulatorData.TriggerRelease(0);
}

void CAkBehavioralCtx::ParamNotification(NotifParams& in_rParams)
{
	if (m_bPlayDirectly)
		return;

	if (GetRTPCKey().MatchValidFields(in_rParams.rtpcKey)) // TODO_NATE : Check that this is necessary.
	{
		UpdateTargetParam(in_rParams.eType, in_rParams.fValue, in_rParams.fDelta);
	}
}

void CAkBehavioralCtx::UpdateTargetParam(AkRTPC_ParameterID in_eParam, AkReal32 in_fValue, AkReal32 in_fDelta)
{
	if (PlayDirectly())
		return;
	AKASSERT(RTPC_BusVolume != in_eParam);	//Supposed to be handled in derived classes.	
	switch (in_eParam)
	{		

	case RTPC_Position_PAN_X_2D:
	case RTPC_Position_PAN_Y_2D:
	case RTPC_Position_PAN_X_3D:
	case RTPC_Position_PAN_Y_3D:
	case RTPC_Position_PAN_Z_3D:
		PositioningChangeNotification(in_fDelta, in_eParam);	//Additive properties
		break;

	case RTPC_Positioning_EnableAttenuation:
		FlushRays();		// Force reinitialization of ray data.
		// no break intentional
	case RTPC_Positioning_Divergence_Center_PCT:
	case RTPC_Positioning_Cone_Attenuation_ON_OFF:
	case RTPC_Positioning_Cone_Attenuation:
	case RTPC_Positioning_Cone_LPF:
	case RTPC_Positioning_Cone_HPF:
	case RTPC_PositioningTypeBlend:	
		PositioningChangeNotification(in_fValue, in_eParam);	//Exclusive
		break;
	default:
	{
		AkPropID pid = g_AkRTPCToPropID[in_eParam];
		if (pid != AkPropID_NUM)
		{
			AkDeltaMonitor::LogUpdate(pid, in_fValue);
			m_EffectiveParams.Accumulate(pid, in_fDelta, AkDelta_None);
		}
		break;
	}
	}		

	if (m_bAreParametersValid && in_eParam == RTPC_ReflectionsVolume)
		SpatialAudioParamsUpdated();
}

void CAkBehavioralCtx::RecalcNotification(bool in_bLiveEdit, bool in_bLog)
{
	m_bAreParametersValid = false;
	if (in_bLiveEdit)
		m_bRefreshAllAfterRtpcChange = true;

#ifdef AK_DELTAMONITOR_ENABLED
	if (in_bLog)
		LogPipelineID();
#endif
}

void CAkBehavioralCtx::RefreshParameters(AkInitialSoundParams* in_pInitialSoundParams)
{
	// Default implementation does nothing.
}

void CAkBehavioralCtx::CalculateMutedEffectiveVolume()
{ 
	m_bFadeRatioDirty = false;
}

AkReal32 CAkBehavioralCtx::Scale3DUserDefRTPCValue(AkReal32 in_fValue)
{
	// IMPORTANT Here, we scale the automation radius based on the attenuation's max distance, regardless of whether attenuation is enabled or not. A sign that "max distance" is misplaced...
	CAkAttenuation* pAttenuation = m_posParams.GetRawAttenuation();
	if (pAttenuation)
	{
		CAkAttenuation::AkAttenuationCurve* pVolumeDryCurve = pAttenuation->GetCurve(AttenuationCurveID_VolumeDry);
		if (pVolumeDryCurve)
			return in_fValue * pVolumeDryCurve->GetLastPoint().From / 100.0f;
	}

	return in_fValue;
}

void CAkBehavioralCtx::PositioningChangeNotification(
	AkReal32			in_RTPCValue,
	AkRTPC_ParameterID	in_ParameterID	// RTPC ParameterID, must be a Positioning ID.
	)
{
	if (m_bPlayDirectly)
		return;

	switch ((AkPositioning_ParameterID)in_ParameterID)
	{
	case POSID_Position_PAN_X_2D:
		m_BasePosParams.m_fPAN_X_2D += in_RTPCValue;
		return;
	case POSID_Position_PAN_Y_2D:
		m_BasePosParams.m_fPAN_Y_2D += in_RTPCValue;
		return;
	case POSID_Position_PAN_X_3D:
		if (m_p3DAutomation)
			m_p3DAutomation->GetParams().m_Position.X += Scale3DUserDefRTPCValue(in_RTPCValue);
		return;
	case POSID_Position_PAN_Y_3D:
		if (m_p3DAutomation)
			m_p3DAutomation->GetParams().m_Position.Z += Scale3DUserDefRTPCValue(in_RTPCValue);
		return;
	case POSID_Position_PAN_Z_3D:
		if (m_p3DAutomation)
			m_p3DAutomation->GetParams().m_Position.Y += Scale3DUserDefRTPCValue(in_RTPCValue);
		return;
	case POSID_Positioning_Divergence_Center_PCT:
		m_BasePosParams.m_fCenterPCT = in_RTPCValue;
		return;
	case POSID_PanningType:
		m_BasePosParams.ePannerType = (AkUInt8)(in_RTPCValue);
		return;
	case POSID_ListenerRelativeRouting:
		m_BasePosParams.bHasListenerRelativeRouting = (in_RTPCValue > 0);
		return;
	case POSID_PositioningTypeBlend:
		m_posParams.positioningTypeBlend = in_RTPCValue;
		return;
	case POSID_IsPositionDynamic:
		m_posParams.settings.m_bHoldEmitterPosAndOrient = (in_RTPCValue ? true : false);
		break;
	case POSID_PositioningEnableAttenuation:
		m_posParams.settings.m_bEnableAttenuation = (in_RTPCValue ? true : false);
		break;
	case POSID_Attenuation:
#ifndef AK_OPTIMIZED
		InvalidatePaths();
#endif
		Reset3DParams();
		break;
	default:
		break;
	}

	if (m_p3DAutomation)
	{
		switch ((AkPositioning_ParameterID)in_ParameterID)
		{
		case POSID_IsLooping:
			m_p3DAutomation->SetIsLooping(in_RTPCValue ? true : false);
			break;
		case POSID_Transition:
			m_p3DAutomation->SetTransition((AkTimeMs)in_RTPCValue);
			break;
		case POSID_PathMode:
			m_p3DAutomation->SetPathMode((AkPathMode)(int)in_RTPCValue);
			break;
		default:
			break;
		}
	}
}


#ifndef AK_OPTIMIZED
void CAkBehavioralCtx::InvalidatePositioning()
{
	InvalidatePaths();
	Reset3DParams();
	RecalcNotification(true);
}

void CAkBehavioralCtx::InvalidatePaths()
{
	if (m_PathInfo.pPBPath != NULL)
	{
		// if continuous then the path got rid of the played flags
		if (m_PathInfo.pPBPath->IsContinuous())
		{
			AkPathState* pPathState = m_pParamNode->GetPathState();
			pPathState->pbPlayed = NULL;
			pPathState->ulCurrentListIndex = 0;
		}
		g_pPathManager->RemovePathUser(m_PathInfo.pPBPath, this);
		m_PathInfo.pPBPath = NULL;
		m_PathInfo.PathOwnerID = AK_INVALID_UNIQUE_ID;

		m_bIsAutomationOrAttenuationDirty = true;
	}
	if (m_p3DAutomation)
	{
		m_p3DAutomation->InvalidatePaths();
	}
}
#endif

void CAkBehavioralCtx::SetParam(
	AkPluginParamID in_paramID,
	const void *	in_pParam,
	AkUInt32		in_uParamSize
	)
{
	AKASSERT(in_uParamSize == sizeof(AkReal32));
	AkReal32 l_newVal = *((AkReal32*)in_pParam);

	switch (in_paramID)
	{
	case POSID_Positioning_Cone_Attenuation:
		m_posParams.m_fConeOutsideVolume = (l_newVal);
		break;
	case POSID_Positioning_Cone_LPF:
		m_posParams.m_fConeLoPass = (l_newVal);
		break;
	case POSID_Positioning_Cone_HPF:
		m_posParams.m_fConeHiPass = (l_newVal);
		break;

	default:
		AKASSERT(!"Invalid or unknown Positioning parameter passed to the PBI");
		break;
	}
}

AkUniqueID CAkBehavioralCtx::GetSoundID() const
{
	return m_pParamNode->ID();
}

CAkBus* CAkBehavioralCtx::GetOutputBusPtr()
{
	if (m_bPlayDirectly)
		return NULL;

	CAkBus* pBusOutput = m_pParamNode->GetMixingBus();
	return pBusOutput;
}

bool CAkBehavioralCtx::IsMultiPositionTypeMultiSources()
{
	return !UseSpatialAudioPanningMode() &&
		(GetEmitter()->GetPosition().GetMultiPositionType() == AK::SoundEngine::MultiPositionType_MultiSources);
}

bool CAkBehavioralCtx::UseSpatialAudioPanningMode()
{
	return (m_pSpatialAudioVoice != NULL &&
			EnableDiffration() &&
			GetEmitter()->GetPosition().GetNumPosition() == 1);
}

AKRESULT CAkBehavioralCtx::GetGameObjectPosition(
	AkUInt32 in_uIndex,
	AkSoundPosition & out_position
	) const
{
	if (in_uIndex < GetNumGameObjectPositions())
	{
		out_position = GetEmitter()->GetPosition().GetPositions()[in_uIndex].position;
		return AK_Success;
	}
	return AK_Fail;
}

AKRESULT CAkBehavioralCtx::GetListenerData(
	AkGameObjectID in_uListener,
	AkListener & out_listener
	) const
{
	if (GetGameObjectPtr()->GetListeners().Exists(in_uListener) != NULL)
	{
		const CAkListener* pData = CAkListener::GetListenerData(in_uListener);
		if (pData)
		{
			out_listener = pData->GetData();
		return AK_Success;
	}
	}
	return AK_Fail;
}

// Get the ith emitter-listener pair. 
AKRESULT CAkBehavioralCtx::GetEmitterListenerPair(
	AkUInt32 in_uIndex,
	AkGameObjectID in_uForListener,
	AkEmitterListenerPair & out_emitterListenerPair
	) const
{
	// Look in game object cache. 
	AKASSERT(GetEmitter()->IsPositionCached());

	// Find desired pair.
	AkUInt32 uPair = 0;
	const AkVolumeDataArray & arEmitListenPairs = GetEmitter()->GetCachedEmitListenPairs();
	AkVolumeDataArray::Iterator it = arEmitListenPairs.Begin();
	while (it != arEmitListenPairs.End())
	{
		if ((*it).ListenerID() == in_uForListener)
		{
			if (uPair == in_uIndex)
				break;

			++uPair;	// Count pairs associated to the given listener
		}
		++it;
	}

	if (it != arEmitListenPairs.End())
	{
		// Found it. Copy.
		out_emitterListenerPair = *it;
		return AK_Success;
	}

	return AK_Fail;
}


// Get the ith emitter-listener pair. 
AKRESULT CAkBehavioralCtx::GetEmitterListenerPair(AkUInt32 in_uIndex, AkEmitterListenerPair & out_emitterListenerPair) const
{
	// Look in game object cache. 
	AKASSERT(GetEmitter()->IsPositionCached());
	const AkVolumeDataArray & arEmitListenPairs = GetEmitter()->GetCachedEmitListenPairs();
	if (in_uIndex < arEmitListenPairs.Length())
	{
		out_emitterListenerPair = arEmitListenPairs[in_uIndex];
		return AK_Success;
	}
	return AK_Fail;
}


AkReal32 CAkBehavioralCtx::EvaluateSpread(AkReal32 in_fDistance)
{
	AkReal32 fSpread = 0.f;

	CAkAttenuation * pAttenuation = GetActiveAttenuation();
	if (pAttenuation)
	{
		CAkAttenuation::AkAttenuationCurve * pSpreadCurve = pAttenuation->GetCurve(AttenuationCurveID_Spread);
		if (pSpreadCurve)
			fSpread = pSpreadCurve->Convert(in_fDistance);
	}
	return fSpread;
}

AkReal32 CAkBehavioralCtx::EvaluateFocus(AkReal32 in_fDistance)
{
	AkReal32 fFocus = 0.f;

	CAkAttenuation * pAttenuation = GetActiveAttenuation();
	if (pAttenuation)
	{
		CAkAttenuation::AkAttenuationCurve * pFocusCurve = pAttenuation->GetCurve(AttenuationCurveID_Focus);
		if (pFocusCurve)
			fFocus = pFocusCurve->Convert(in_fDistance);
	}
	return fFocus;
}

AKRESULT CAkBehavioralCtx::CacheEmitterPosition()
{
	AKASSERT(!m_bGameObjPositionCached);
	AkUInt32 numPos = GetEmitter()->GetPosition().GetNumPosition();
	AKRESULT eRes = m_cachedGameObjectPosition.Reserve(numPos);
	if (eRes == AK_Success)
	{
		const AkChannelEmitter * pos = GetEmitter()->GetPosition().GetPositions();
		for (AkUInt32 i = 0; i < numPos; i++)
		{
			AkChannelEmitter * newpos = m_cachedGameObjectPosition.AddLast();
			AKASSERT(newpos);	// Should have been reserved.
			*newpos = pos[i];
		}
		m_bGameObjPositionCached = true;
	}
	return eRes;
}

// Compute emitter-listener pairs.
// Returns the number of rays.
AkUInt32 CAkBehavioralCtx::ComputeEmitterListenerPairs()
{
	RefreshAutomationAndAttenation();	

	AkReal32 fMinDistance = AK_UPPER_MAX_DISTANCE;

	// Gather rays, with emitter position set depending on m_e3DPositionType and HoldEmitterPosAndOrient.

	if (m_posParams.settings.m_e3DPositionType != AK_3DPositionType_ListenerWithAutomation)
	{
		// Get position from game object or from copied version stored in context (if HoldEmitterPosAndOrient ticked).
		if (!m_posParams.settings.m_bHoldEmitterPosAndOrient)
		{
			bool bUseSpatialAudioRays = GetGameObjectPtr()->HasComponent(GameObjComponentIdx_SpatialAudioObj) &&
										GetGameObjectPtr()->HasComponent(GameObjComponentIdx_SpatialAudioEmitter);

			if (bUseSpatialAudioRays)
			{
				CAkSpatialAudioComponent* pSACmpt = GetGameObjectPtr()->GetComponent<CAkSpatialAudioComponent>();
				bool bHasDiffraction = m_pSpatialAudioVoice != nullptr ? m_pSpatialAudioVoice->HasDiffraction() : false;
				fMinDistance = pSACmpt->GetRayVolumeData(m_arVolumeData, bHasDiffraction, IsGameDefinedAuxEnabled());
			}
			else
			{
				// Try to use cached rays.				
				const AkVolumeDataArray & arEmitListenPairs = GetEmitter()->GetCachedEmitListenPairs();
				if (!m_arVolumeData.Resize(arEmitListenPairs.Length()))
					return 0;			

				AkRayVolumeData* AK_RESTRICT pPairs = arEmitListenPairs.Data();
				AkRayVolumeData* AK_RESTRICT pRays = m_arVolumeData.Data();
				for (AkUInt32 i = 0; i < arEmitListenPairs.Length(); i++)
				{						
					// Get min distance for priority computation.
					AkReal32 fDistance = pPairs[i].Distance();
					if (fDistance < fMinDistance)
						fMinDistance = fDistance;

					pRays[i].CopyEmitterListenerData(pPairs[i]); //Volumes not copied.
				}
			}
		}
		else
		{
			// Cache game object position the first time.
			if (AK_EXPECT_FALSE(!m_bGameObjPositionCached))
			{
				if (CacheEmitterPosition() != AK_Success)
					return 0;
			}

			AkUInt32 uNumPosition = m_cachedGameObjectPosition.Length();
			const AkListenerSet & listeners = GetGameObjectPtr()->GetListeners();
			AkUInt32 uNumListeners = listeners.Length();
			AkUInt32 uNumRays = uNumPosition * uNumListeners;

			if(!m_arVolumeData.Resize(uNumRays))
				return 0;

			AkRayVolumeData* pRays = m_arVolumeData.Data();

			AkGameRayParams * arRayParams = (AkGameRayParams*)AkAlloca(uNumPosition*sizeof(AkGameRayParams));
			for (AkListenerSet::Iterator it = listeners.Begin(); it != listeners.End(); ++it)
			{
				const CAkListener * pListener = CAkListener::GetListenerData(*it);
				AkUInt32 iRay = 0;
				if (pListener != NULL)
				{
					GetEmitter()->GetPosition().GetGameRayParams(pListener->ID(), uNumPosition, arRayParams);

					AkChannelEmitter* AK_RESTRICT pEmitter = m_cachedGameObjectPosition.Data();
					AkChannelEmitter* AK_RESTRICT pEnd = pEmitter + uNumPosition;
					for (; pEmitter != pEnd; pRays++, iRay++, pEmitter++)
					{																								
						pRays->UpdateRay(pEmitter->position, arRayParams[iRay], pListener->ID(), pEmitter->uInputChannels);
						// Note: ComputeRayDistanceAndAngles() is rerun below if there is automation.
						AkReal32 fScaledDistance = pListener->ComputeRayDistanceAndAngles(GetEmitter()->GetScalingFactor(), *pRays);
						if (fScaledDistance < fMinDistance)
							fMinDistance = fScaledDistance;
					}
				}
			}
		}
	}
	else
	{
		// Create rays for listener-centric automation.
		// Note: At the moment listeners only have one position.
		const AkListenerSet& listeners = GetGameObjectPtr()->GetListeners();
		AkUInt32 numRays = listeners.Length();
		if (!m_arVolumeData.Resize(numRays))
			// BAIL!
			return 0;

		AkRayVolumeData* pRays = m_arVolumeData.Data();
		
		for (AkListenerSet::Iterator it = listeners.Begin(); it != listeners.End(); ++it,++pRays)
		{
			AkGameObjectID uListener = *it;
			const CAkListener* pListener = CAkListener::GetListenerData(uListener);
			if (pListener != NULL)
			{					
				// Set per-listener ray data
				// Note: we get listener-specific data (such as obs/occ, emitter scaling, ...) from the Emitter object, because although positioning is automated around
				// the listener, the sound is still played over an emitter that is distinct from the listener game object. Doing so is the most flexible.
				AkGameRayParams rayParams;
				GetEmitter()->GetPosition().GetGameRayParams(uListener, 1, &rayParams);				
				pRays->UpdateRay(pListener->GetTransform(), rayParams, uListener, 0xFFFFFFFF);
			}
		}
	}

	if (m_posParams.settings.HasAutomation() && m_p3DAutomation)
	{
		// Automation
		/// LX REVIEW What if we had run out of memory?
		Ak3DAutomationParams & r3DParams = m_p3DAutomation->GetParams();

		if (m_PathInfo.pPBPath)
		{
			// this one might have been changed so pass it along
			bool bIsLooping = r3DParams.m_bIsLooping;
			m_PathInfo.pPBPath->SetIsLooping(bIsLooping);

			if (bIsLooping
				&& m_PathInfo.pPBPath->IsContinuous()
				&& m_PathInfo.pPBPath->IsIdle())
			{
				StartPath();
			}
		}

		// For each ray, take the automation position r3DParams.m_Position (absolute against AK_DEFAULT_SOUND_ORIENTATION), rotate it in the
		// desired orientation frame (emitter, listener, or "Held" listener orientation), and then offset the ray's emitter.

		// Review opti: We could avoid computing a few things (namely, emitter orientation) if !bSpatialized && !bRequiresCone, but this case is uncommon.
		//bool bSpatialized = m_posParams.settings.m_eSpatializationMode != AK_SpatializationMode_None && pListener->IsSpatialized();

		// Since distances will likely change, reset fMinDistance for priority handling.
		fMinDistance = AK_UPPER_MAX_DISTANCE;

		AkVolumeDataArray::Iterator it = m_arVolumeData.Begin();
		for (; it != m_arVolumeData.End(); ++it)
		{
			AkRayVolumeData & ray = (*it);

			// Review: Could cache the listenerDatas on the stack while building the ray array. BUT it is unneeded for emitter w/o automation, which is the most common case...
			const CAkListener* pListener = CAkListener::GetListenerData(ray.ListenerID());
			if (!pListener)
				continue;	// Listener was unregistered.

			AkVector vRotatedAutomatedPosition = r3DParams.m_Position;
			if (m_posParams.settings.m_e3DPositionType == AK_3DPositionType_ListenerWithAutomation)
			{
				if (m_posParams.settings.m_bHoldListenerOrient
					&& m_PathInfo.pPBPath)
				{
					AkReal32* pNoFollowMatrix = m_PathInfo.pPBPath->GetNoFollowRotationMatrix(ray.ListenerID());
					if (pNoFollowMatrix != NULL)	//If not enough memory, don't care, just don't rotate.
					{
						// The matrix computed above represents the relative rotation of the listener since 
						// the "path" started. In order to simulate a non following spot effect, rotate the position 
						// vector of the source in the opposite direction by multiplying it by the inverted rotation matrix that was 
						// stored when the path was created
						AkMath::RotateVector(r3DParams.m_Position, pNoFollowMatrix, vRotatedAutomatedPosition);

						// Do the same with the default front and top vectors, or equivalently, take them directly from the "no follow" matrix (i.e. the snapshot of the listener orientation when the sound was triggered).
						ray.emitter.SetOrientation(pNoFollowMatrix[6], pNoFollowMatrix[7], pNoFollowMatrix[8], pNoFollowMatrix[3], pNoFollowMatrix[4], pNoFollowMatrix[5]);
					}
				}
				else
				{
					// Set the orientation to that of the listener.
					const AkTransform & rXform = pListener->GetTransform();
					ray.emitter.SetOrientation(rXform.OrientationFront(), rXform.OrientationTop());

					// Position must be made relative to listener's point of view.
					AkMath::RotateVector(r3DParams.m_Position, (const AkReal32*)pListener->GetMatrix(), vRotatedAutomatedPosition);
				}

				// Offset ray's emitter by vRotatedAutomatedPosition
				ray.emitter.SetPosition(ray.emitter.Position() + vRotatedAutomatedPosition);

				// Update rays: distance and cones, and compute fMinDistance for priority handling.
				AkReal32 fScaledDistance = pListener->ComputeRayDistanceAndAngles(GetEmitter()->GetScalingFactor(), ray);
				if (fScaledDistance < fMinDistance)
					fMinDistance = fScaledDistance;

				// In listener-centric mode, cone used is that of the listener.
				ray.SetEmitterAngle(ray.ListenerAngle());
			}
			else
			{
				// Position must be made relative to emitter's point of view.
				// Build our old-school rotation matrix from emitter orientation vectors.
				AkVector OrientationSide = AkMath::CrossProduct(ray.emitter.OrientationTop(), ray.emitter.OrientationFront());
				AkReal32 mxEmitRot[3][3];
				{
					AkReal32* pFloat = &(mxEmitRot[0][0]);

					// Build up rotation matrix out of our 3 basic row vectors, stored in row major order.
					*pFloat++ = OrientationSide.X;
					*pFloat++ = OrientationSide.Y;
					*pFloat++ = OrientationSide.Z;

					*pFloat++ = ray.emitter.OrientationTop().X;
					*pFloat++ = ray.emitter.OrientationTop().Y;
					*pFloat++ = ray.emitter.OrientationTop().Z;

					*pFloat++ = ray.emitter.OrientationFront().X;
					*pFloat++ = ray.emitter.OrientationFront().Y;
					*pFloat++ = ray.emitter.OrientationFront().Z;
				}
				AkMath::RotateVector(r3DParams.m_Position, &(mxEmitRot[0][0]), vRotatedAutomatedPosition);
				
				// Offset ray's emitter by vRotatedAutomatedPosition
				ray.emitter.SetPosition(ray.emitter.Position() + vRotatedAutomatedPosition);

				// Update rays: distance and cones, and compute fMinDistance for priority handling.
				AkReal32 fScaledDistance = pListener->ComputeRayDistanceAndAngles(GetEmitter()->GetScalingFactor(), ray);
				if (fScaledDistance < fMinDistance)
					fMinDistance = fScaledDistance;
			}
		}
	}

	//Get curve max radius
	// IMPORTANT Here, we update priority based on the attenuation's max distance, regardless of whether attenuation is enabled or not. A sign that "max distance" is misplaced...
	CAkAttenuation* pAttenuation = m_posParams.GetRawAttenuation();
	if (pAttenuation)
		UpdatePriorityWithDistance(pAttenuation, fMinDistance);

	return m_arVolumeData.Length();
}

AkUInt32 CAkBehavioralCtx::GetNumRays(const AkListenerSet& in_uAllowedListeners, AkSetType in_setType)
{
	AkUInt32 uNumRays = 0;
	AkVolumeDataArray::Iterator it = m_arVolumeData.Begin();
	while (it != m_arVolumeData.End())
	{
		if (AkContains(in_uAllowedListeners, in_setType, (*it).ListenerID()))
			++uNumRays;
		++it;
	}
	return uNumRays;
}

const AkRayVolumeData * CAkBehavioralCtx::GetRay(
	const AkListenerSet& in_uAllowedListeners,
	AkSetType in_setType,
	AkUInt32 in_uIndex
)
{
	AkVolumeDataArray::Iterator it = m_arVolumeData.Begin();
	while (it != m_arVolumeData.End())
	{
		if (AkContains(in_uAllowedListeners, in_setType, (*it).ListenerID()))
		{
			if (in_uIndex == 0)
			{
				// This is the one.
				return &(*it);
			}
			--in_uIndex;
		}
		++it;
	}
	return NULL;
}

void CAkBehavioralCtx::GetAuxSendsValues(AkAuxSendArray & io_arAuxSends)
{
//	AKASSERT(!IsForcedVirtualized());	// do not call this function if ForcedVirtualized.

	const CAkConnectedListeners& connectedListeners = GetGameObjectPtr()->GetConnectedListeners();

	AkReal32 fLinGameAuxSendVolume = 0.f;
	AkReal32 fGameAuxLPF = 0.f;
	AkReal32 fGameAuxHPF = 0.f;

	// Get game-defined sends
	if (IsGameDefinedAuxEnabled())
	{
		fLinGameAuxSendVolume = AkMath::dBToLin(m_EffectiveParams.GameAuxSendVolume());
		fGameAuxLPF = m_EffectiveParams.GameAuxSendLPF();
		fGameAuxHPF = m_EffectiveParams.GameAuxSendHPF();

		OpenSoundBrace(); // Log the game defined sends set from SetGameObjectAuxSendValues()
		connectedListeners.GetAuxGains(io_arAuxSends, fLinGameAuxSendVolume, fGameAuxLPF, fGameAuxHPF);
		CloseSoundBrace();
	}

	// Get user-defined sends
	// 1 set per listener
	AkGameObjectID myObjId = GetGameObjectPtr()->ID();
	const AkGameObjectID* pGameObjects = &myObjId;
	AkUInt32 uNumGameObj = 1;
	if (HasListenerRelativeRouting())
	{
		const AkListenerSet& listeners = connectedListeners.GetUserAssocs().GetListeners();
		pGameObjects = listeners.Begin().pItem;
		uNumGameObj = listeners.Length();
	}

	for (AkUInt32 i = 0; i < AK_NUM_USER_AUX_SEND_PER_OBJ; ++i)
	{
		AkAuxBusID auxID = m_EffectiveParams.aAuxSend[i];
		if (auxID == AK_INVALID_AUX_ID)
			continue;

		for (AkUInt32 goIdx = 0; goIdx < uNumGameObj; ++goIdx)
		{
			if (m_EffectiveParams.GetUserAuxSendVolume(i) > g_fVolumeThresholdDB)
			{
				AkAuxSendValueEx * pNewSend = io_arAuxSends.AddLast();
				if (pNewSend)
				{
					pNewSend->listenerID = pGameObjects[goIdx];
					pNewSend->auxBusID = m_EffectiveParams.aAuxSend[i];
					pNewSend->fControlValue = AkMath::dBToLin(m_EffectiveParams.GetUserAuxSendVolume(i));
					pNewSend->fLPFValue = m_EffectiveParams.GetUserAuxSendLPF(i);
					pNewSend->fHPFValue = m_EffectiveParams.GetUserAuxSendHPF(i);
					pNewSend->eAuxType = ConnectionType_UserDefSend;
				}
			}
		}
	}

	if (m_pSpatialAudioVoice != NULL)
	{
		OpenSoundBrace();
		m_pSpatialAudioVoice->GetAuxSendsValues(io_arAuxSends, fLinGameAuxSendVolume, fGameAuxLPF, fGameAuxHPF);
		CloseSoundBrace();
	}
}

AKRESULT CAkBehavioralCtx::SubscribeAttenuationRTPC(CAkAttenuation* in_pAttenuation)
{
	if (m_bPlayDirectly)
		return AK_Success;
	AKRESULT  eResult = AK_Success;

	const CAkAttenuation::RTPCSubsArray & list = in_pAttenuation->GetRTPCSubscriptionList();
	for (CAkAttenuation::RTPCSubsArray::Iterator iter = list.Begin(); iter != list.End(); ++iter)
	{
		CAkAttenuation::RTPCSubs& l_rRTPC = *iter;

		eResult = g_pRTPCMgr->SubscribeRTPC(static_cast<CAkBehavioralCtx*>(this),
			l_rRTPC.RTPCID,
			l_rRTPC.RTPCType,
			l_rRTPC.RTPCAccum,
			l_rRTPC.ParamID,
			l_rRTPC.RTPCCurveID,
			l_rRTPC.ConversionTable.Scaling(),
			&l_rRTPC.ConversionTable.GetFirstPoint(),
			l_rRTPC.ConversionTable.Size(),
			GetRTPCKey(),
			CAkRTPCMgr::SubscriberType_PBI
			);
		if (eResult != AK_Success)
			break;
	}
	return eResult;
}

void CAkBehavioralCtx::UnsubscribeAttenuationRTPC(CAkAttenuation* in_pAttenuation)
{
	const CAkAttenuation::RTPCSubsArray & list = in_pAttenuation->GetRTPCSubscriptionList();
	for (CAkAttenuation::RTPCSubsArray::Iterator iter = list.Begin(); iter != list.End(); ++iter)
	{
		CAkAttenuation::RTPCSubs& l_rRTPC = *iter;

		g_pRTPCMgr->UnSubscribeRTPC(this, l_rRTPC.ParamID);
	}
}

void CAkBehavioralCtx::PausePath(bool in_bPause)
{
	if (m_PathInfo.pPBPath != NULL)
	{
		if (in_bPause)
		{
			g_pPathManager->Pause(m_PathInfo.pPBPath);
		}
		else
		{
			g_pPathManager->Resume(m_PathInfo.pPBPath);
		}
	}
}

#ifndef AK_OPTIMIZED

void CAkBehavioralCtx::UpdateAttenuationInfo()
{
	AKASSERT(!m_bPlayDirectly);

	Get3DParams();

	CAkAttenuation* pAttenuation = m_posParams.GetRawAttenuation();

	if (m_bIsAutomationOrAttenuationDirty)
	{
		m_bIsAutomationOrAttenuationDirty = false;

		// Init Path, if any.
		if (!m_PathInfo.pPBPath)
		{
			if (SetupPath(NULL, true) != AK_Success)
				return;
		}

		if (UpdateAttenuation(pAttenuation) != AK_Success)
			return;
	}
	
	if (pAttenuation)
	{
		// Taking the value from the attenuation, if they are RTPC based, they will be overriden upon RTPC subscription, coming right after this.
		m_posParams.m_fConeOutsideVolume = (pAttenuation->m_ConeParams.fOutsideVolume);
		m_posParams.m_fConeLoPass = (pAttenuation->m_ConeParams.LoPass);
		m_posParams.m_fConeHiPass = (pAttenuation->m_ConeParams.HiPass);

		g_pRTPCMgr->UnSubscribeRTPC(this); // Do not call UnsubscribeAttenuationRTPC( pAttenuation ); here: list may have changed since last subscription
		SubscribeAttenuationRTPC(pAttenuation);
	}
}

#endif //AK_OPTIMIZED

CAkBus* CAkBehavioralCtx::GetControlBus()
{
	AKASSERT(m_pParamNode != NULL);
	if (m_bPlayDirectly)
		return NULL;
	return m_pParamNode->GetControlBus();
}

bool CAkBehavioralCtx::IsAuxRoutable()
{
	AKASSERT(m_bAreParametersValid);

	//////////////////////////////////////////////////////////////////////////
	// This function should return true if there is an Aux bus set in Wwise 
	// or if it is possible to use Game Defined Aux Sends.
	//////////////////////////////////////////////////////////////////////////
	return IsGameDefinedAuxEnabled() || HasUserDefineAux() || HasEarlyReflectionsAuxSend();
}

bool CAkBehavioralCtx::HasUserDefineAux()
{
	for (int i = 0; i < AK_NUM_USER_AUX_SEND_PER_OBJ; ++i)
	{
		if (m_EffectiveParams.aAuxSend[i] != AK_INVALID_UNIQUE_ID)
			return true;
	}
	return false;
}

bool CAkBehavioralCtx::HasEarlyReflectionsAuxSend()
{ 
	if (m_pSpatialAudioVoice != NULL)
	{
		CAkSpatialAudioComponent* pSACmpt = GetGameObjectPtr()->GetComponent<CAkSpatialAudioComponent>();
		if (pSACmpt)
		{
			if (pSACmpt->HasReflectionsAuxSends() &&			// There are currently active reflections.
				(m_pSpatialAudioVoice->HasReflections() ||	// ER bus enabled through sound
				pSACmpt->HasEarlyReflectionsBusFromAPI()))	// ER bus enabled through API (per-game obj)
			{
				return true;
			}

			if (pSACmpt->HasCustomImageSourceAuxSends())	
			{
				return true;
			}
		}
	}

	return false;
}

void CAkBehavioralCtx::GetAttachedPropsFxDesc(AkFXDesc& out_fxDesc)
{
	m_pParamNode->GetAttachedPropFX(out_fxDesc);
}

void CAkBehavioralCtx::RefreshAutomationAndAttenation()
{
	if (m_bIsAutomationOrAttenuationDirty)
	{
		Reset3DParams();// Reset must be called again, When live editing parameter may have been updated by the engine in between. (WG-30722, not the original bug but the last re-open of if)
		m_bIsAutomationOrAttenuationDirty = false;

		Get3DParams();
		
		if (m_p3DAutomation)
		{
			// Init Path, if any.
			if(!m_PathInfo.pPBPath)
				SetupPath(NULL, true);
		}

		CAkAttenuation* pAttenuation = m_posParams.GetRawAttenuation();
		if (pAttenuation)
			UpdateAttenuation(pAttenuation);
	}
}

void CAkBehavioralCtx::Reset3DParams()
{
	CAkAttenuation* pAttenuation = m_posParams.GetRawAttenuation();
	if (pAttenuation)
	{
		UnsubscribeAttenuationRTPC(pAttenuation);
#ifndef AK_OPTIMIZED
		pAttenuation->RemovePBI(this);
#endif
		m_posParams.ReleaseAttenuation();
	}
	if (m_p3DAutomation)
	{
		AkDelete(AkMemID_Processing, m_p3DAutomation);
		m_p3DAutomation = NULL;
	}

	m_bIsAutomationOrAttenuationDirty = true;
}

void CAkBehavioralCtx::Get3DParams()
{
	if (!m_bPlayDirectly)
	{
		m_pParamNode->GetPositioningParams(m_rtpcKey, m_BasePosParams, m_posParams);
		// Fetch a 3D automation object if it's been cleared and we want automation.
		if (m_BasePosParams.bHasListenerRelativeRouting && m_posParams.settings.HasAutomation() && !m_p3DAutomation)
		    m_pParamNode->Get3DAutomationParams(m_p3DAutomation);
	}
	else
	{
		m_BasePosParams.Init();
	}
}

void CAkBehavioralCtx::SpatialAudioParamsUpdated()
{
	if (m_pSpatialAudioVoice != NULL)
	{
		m_pSpatialAudioVoice->ParamsUpdated();
	}
}

AKRESULT CAkBehavioralCtx::UpdateAttenuation(CAkAttenuation* in_pattenuation)
{
	AKRESULT eResult = AK_Success;
	if (in_pattenuation)
	{
		// Taking the value from the attenuation, if they are RTPC based, they will be overriden upon RTPC subscription, coming right after this.
		m_posParams.m_fConeOutsideVolume = (in_pattenuation->m_ConeParams.fOutsideVolume);
		m_posParams.m_fConeLoPass = (in_pattenuation->m_ConeParams.LoPass);
		m_posParams.m_fConeHiPass = (in_pattenuation->m_ConeParams.HiPass);

		eResult = SubscribeAttenuationRTPC(in_pattenuation);
		if (eResult != AK_Success)
			return eResult;

#ifndef AK_OPTIMIZED
		in_pattenuation->AddPBI(this);
#endif
	}
	else
	{
		//we were expecting an attenuation but we couldn't get one
		if (m_posParams.m_uAttenuationID != AK_INVALID_UNIQUE_ID)
			return AK_Fail; //return an error (see WG-6760)
	}
	return eResult;
}

#ifndef AK_OPTIMIZED
void CAkBehavioralCtx::UpdateRTPC()
{
	RegisterParamTarget( m_pParamNode );
}

void CAkBehavioralCtx::ResetRTPC()
{
	UnregisterAllParamTarget();
}
#endif
