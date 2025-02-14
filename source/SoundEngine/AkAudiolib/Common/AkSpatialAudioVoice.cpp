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

#include "stdafx.h"

#include "AkSpatialAudioVoice.h"
#include "AkSpatialAudioComponent.h"
#include "AkBehavioralCtx.h"

extern AkReal32 g_fVolumeThresholdDB;
extern AkReal32 g_fVolumeThreshold;

// Global initialized flag to know when spatial audio is in use.
bool g_bSpatialAudioInitialized = false;

// CAkSpatialAudioVoice
CAkSpatialAudioVoice::CAkSpatialAudioVoice(): pNextItem(NULL), m_pOwner(NULL), m_trackingReflectionsAuxBus(AK_INVALID_AUX_ID), m_bTrackingReflections(false), m_bTrackingDiffraction(false)
{
}

CAkSpatialAudioVoice* CAkSpatialAudioVoice::Create(CAkBehavioralCtx* in_pOwner)
{
	CAkSpatialAudioVoice* pSpatialAudioVoice = NULL;

	// Here are the setting that are used to determine if spatial audio is enabled for a given voice. 
	//	If these settings change, makes sure to update the error message conditions in the CAkBPI::ReportSpatialAudioErrorCodes() and CAkMixBusCtx::ReportSpatialAudioErrorCodes().

	bool bSpatialAudioEnabledForVoice = g_bSpatialAudioInitialized &&
		in_pOwner->HasListenerRelativeRouting() &&
		in_pOwner->Get3DPositionType() != AK_3DPositionType_ListenerWithAutomation;

	if (bSpatialAudioEnabledForVoice)
	{
		CAkSpatialAudioRoomComponent* pRoomComponent = in_pOwner->GetGameObjectPtr()->GetComponent<CAkSpatialAudioRoomComponent>();
		if (pRoomComponent != NULL)
		{
			// This voice is playing on a spatial audio room.
			if (in_pOwner->GetSoundID() == pRoomComponent->GetAuxID())
			{
				// If this voice is the room aux bus, then we don't want a spatial audio voice. The diffraction and reflection flags are ignored.
				bSpatialAudioEnabledForVoice = false;
			}
		}
	}

	if (bSpatialAudioEnabledForVoice)
	{
		CAkSpatialAudioComponent* pSpatialAudioComponent = in_pOwner->GetGameObjectPtr()->GetComponent<CAkSpatialAudioComponent>();
		if (pSpatialAudioComponent)
		{
			if (pSpatialAudioComponent->IsReflectionsAuxBus(in_pOwner->GetSoundID()))
			{
				// If this voice is an early reflections aux bus assigned to this object, we don't want a spatial audio voice. The diffraction and reflection flags are ignored.
				bSpatialAudioEnabledForVoice = false;
			}
		}
	}

	if (bSpatialAudioEnabledForVoice)
	{
		pSpatialAudioVoice = AkNew(AkMemID_Object, CAkSpatialAudioVoice());
		if (pSpatialAudioVoice != NULL)
		{
			if (pSpatialAudioVoice->Init(in_pOwner) != AK_Success)
			{
				AkDelete(AkMemID_Object, pSpatialAudioVoice);
				pSpatialAudioVoice = NULL;
			}
		}
	}

	return pSpatialAudioVoice;
}

AKRESULT CAkSpatialAudioVoice::Init(CAkBehavioralCtx* in_pOwner)
{
	CAkGameObject* pGameObj = in_pOwner->GetGameObjectPtr();
	AKASSERT(pGameObj != NULL);

	CAkSpatialAudioComponent* pSACmpt = pGameObj->CreateComponent<CAkSpatialAudioComponent>();
	if (pSACmpt != NULL)
	{
		m_pOwner = in_pOwner;

		pSACmpt->TrackVoice(this);

		return AK_Success;
	}

	return AK_Fail;
}

void CAkSpatialAudioVoice::Term()
{
	if (m_pOwner != NULL)
	{
		CAkGameObject* pGameObj = m_pOwner->GetGameObjectPtr();
		AKASSERT(pGameObj != NULL);

		CAkSpatialAudioComponent* pSACmpt = pGameObj->GetComponent<CAkSpatialAudioComponent>();
		AKASSERT(pSACmpt != NULL);

		pSACmpt->UntrackVoice(this);

		m_pOwner = NULL;
	}
}

void CAkSpatialAudioVoice::ParamsUpdated()
{
	if (m_pOwner != NULL)
	{
		CAkGameObject* pGameObj = m_pOwner->GetGameObjectPtr();
		AKASSERT(pGameObj != NULL);

		CAkSpatialAudioComponent* pSACmpt = pGameObj->GetComponent<CAkSpatialAudioComponent>();
		if (pSACmpt != NULL)
		{
			pSACmpt->UpdateTracking(this);
		}
	}
}

void CAkSpatialAudioVoice::GetAuxSendsValues(AkAuxSendArray & io_arAuxSends, AkReal32 in_fVol, AkReal32 in_fLPF, AkReal32 in_fHPF) const
{
	AKASSERT(m_pOwner != NULL);
	
	const AkSoundParams& params = m_pOwner->GetEffectiveParams();

	CAkGameObject* pGameObj = m_pOwner->GetGameObjectPtr();
	AKASSERT(pGameObj != NULL);

	CAkSpatialAudioComponent* pSACmpt = pGameObj->GetComponent<CAkSpatialAudioComponent>();
	AKASSERT(pSACmpt != NULL);

	if (m_pOwner->IsGameDefinedAuxEnabled())
	{
		pSACmpt->GetRoomReverbAuxSends(io_arAuxSends, in_fVol, in_fLPF, in_fHPF);
	}

	pSACmpt->GetEarlyReflectionsAuxSends(io_arAuxSends, params.reflectionsAuxBus, AkMath::dBToLin(params.ReflectionBusVolume()));
}

bool CAkSpatialAudioVoice::HasReflections() const
{
	if (m_pOwner != NULL)
	{
		const AkSoundParams& params = m_pOwner->GetEffectiveParams();
		return	params.reflectionsAuxBus != AK_INVALID_AUX_ID && 
				params.ReflectionBusVolume() > g_fVolumeThresholdDB;
	}
	return false;
}

bool CAkSpatialAudioVoice::HasDiffraction() const
{
	if (m_pOwner != NULL)
	{
		return m_pOwner->EnableDiffration();
	}
	return false;
}

AkReal32 CAkSpatialAudioVoice::GetMaxDistance() const
{
	AkReal32 maxDist = AK_UPPER_MAX_DISTANCE;
	if (m_pOwner != NULL)
	{
		CAkAttenuation * pAttenuation = m_pOwner->GetActiveAttenuation();
		if (pAttenuation)
		{
			CAkAttenuation::AkAttenuationCurve* pVolumeDryCurve = pAttenuation->GetCurve(AttenuationCurveID_VolumeDry);
			if (pVolumeDryCurve)
			{
				AkRTPCGraphPoint& lastPtDry = pVolumeDryCurve->GetLastPoint();
				if (lastPtDry.To > g_fVolumeThreshold)
				{
					maxDist = AK_UPPER_MAX_DISTANCE;
				}
				else
				{
					maxDist = lastPtDry.From;

					CAkAttenuation::AkAttenuationCurve* pVolumeDryWet = pAttenuation->GetCurve(AttenuationCurveID_VolumeAuxGameDef);
					if (pVolumeDryWet)
					{
						if (pVolumeDryWet->GetLastPoint().To > g_fVolumeThreshold)
						{
							maxDist = AK_UPPER_MAX_DISTANCE;
						}
					}
				}
			}
		}
	}
	return maxDist;
}

AkUniqueID CAkSpatialAudioVoice::GetReflectionsAuxBus() const
{
	if (m_pOwner != NULL)
	{
		const AkSoundParams& params = m_pOwner->GetEffectiveParams();
		return params.reflectionsAuxBus;
	}
	return AK_INVALID_AUX_ID;
}