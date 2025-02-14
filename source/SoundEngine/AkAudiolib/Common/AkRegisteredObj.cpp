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
// AkRegisteredObj.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkRegisteredObj.h"
#include "AkParameterNodeBase.h"
#include "AkSwitchMgr.h"
#include "Ak3DListener.h"
#include "AkRegistryMgr.h"
#include "AkAudioLib.h"
#include "AkMonitor.h"

//
//	CAkRegisteredObj
//

CAkRegisteredObj::~CAkRegisteredObj()
{
	g_pSwitchMgr->UnregisterGameObject(this);
}

const CAkConnectedListeners& CAkRegisteredObj::GetConnectedListeners() const
{
	return *GetComponentOrDefault<CAkConnectedListeners>();
}

const AkListenerSet& CAkRegisteredObj::GetListeners() const
{
	return GetConnectedListeners().GetListeners();
}

AkUInt32 CAkRegisteredObj::GetNumEmitterListenerPairs() const
{
	CAkEmitter* pEmitter = GetComponent<CAkEmitter>();
	if (!pEmitter)
		return 0;
	
	return GetListeners().Length() * pEmitter->GetPosition().GetNumPosition();
}

#ifndef AK_OPTIMIZED


struct AkHelper_SetImplicitSolo
{
	AkHelper_SetImplicitSolo(bool in_bImplicitSolo, bool in_bNotify) : bImplicitSolo(in_bImplicitSolo), bNotify(in_bNotify) {}

	bool bImplicitSolo; 
	bool bNotify;

	void operator()(CAkGameObject* in_pObj)
	{
		CAkRegisteredObj* pObj = (CAkRegisteredObj*) in_pObj;

		if (bImplicitSolo)
		{
			++(pObj->m_uSoloImplicit);
		}
		else
		{
			AKASSERT(pObj->m_uSoloImplicit != 0);
			--(pObj->m_uSoloImplicit);
		}

		if (bNotify)
		{
			MONITOR_OBJREGISTRATION_2(pObj);
		}
	}
};

struct AkHelper_CountExplicitSolo
{
	AkHelper_CountExplicitSolo() : uNumExplicitSolo(0) {}

	AkUInt32 uNumExplicitSolo;

	void operator()(CAkGameObject* in_pObj)
	{
		if (((CAkRegisteredObj*)in_pObj)->m_bSoloExplicit)
			uNumExplicitSolo++;
	}
};


void CAkRegisteredObj::MuteGameObject(bool in_bMute, bool in_bNotify)
{
	if (m_bMute != in_bMute)
	{
		m_bMute = in_bMute;

		if (m_bMute)
			++CAkRegistryMgr::g_uMuteGameObjCount;
		else
			--CAkRegistryMgr::g_uMuteGameObjCount;

		if (in_bNotify)
		{
			MONITOR_OBJREGISTRATION_2(this);
		}

		CAkParameterNodeBase::SetMonitoringMuteSoloDirty();
	}
};

void CAkRegisteredObj::SoloGameObject(bool in_bSolo, bool in_bNotify)
{
	if (m_bSoloExplicit != in_bSolo)
	{
		m_bSoloExplicit = in_bSolo;

		if (m_bSoloExplicit)
			++CAkRegistryMgr::g_uSoloGameObjCount;
		else
			--CAkRegistryMgr::g_uSoloGameObjCount;
	
		PropagateImplicitSolo(in_bNotify);

		CAkParameterNodeBase::SetMonitoringMuteSoloDirty();
	}
};

void CAkRegisteredObj::PropagateImplicitSolo(bool in_bNotify)
{
	AkEmitterListenerPathTraversal traversal;
	traversal.Forwards(this);
	traversal.Backwards(this);

	AkHelper_SetImplicitSolo set(IsSoloExplicit(), in_bNotify);
	traversal.ApplyOp(set);
}

void CAkRegisteredObj::UpdateImplicitSolo(bool in_bNotify)
{
	AkEmitterListenerPathTraversal traversal;
	traversal.Forwards(this);
	traversal.Backwards(this);

	AkHelper_CountExplicitSolo check;
	traversal.ApplyOp(check);

	m_uSoloImplicit = check.uNumExplicitSolo;

	if (in_bNotify)
	{
		MONITOR_OBJREGISTRATION_2(this);
	}
}

void CAkRegisteredObj::ClearImplicitSolo(bool in_bNotify)
{ 
	if (m_uSoloImplicit > 0)
	{
		m_uSoloImplicit = 0;
		if (in_bNotify)
		{
			MONITOR_OBJREGISTRATION_2(this);
		}
	}
}

#endif

//
//	CAkModifiedNodes
//

CAkModifiedNodes::CAkModifiedNodes() : CAkTrackedGameObjComponent < GameObjComponentIdx_ModifiedNodes >()
{
}

CAkModifiedNodes::~CAkModifiedNodes()
{
	for (AkListNode::Iterator iter = m_ListModifiedNodes.Begin(); iter != m_ListModifiedNodes.End(); ++iter)
	{
		CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef((*iter));
		if (pNode)
		{
			pNode->Unregister((CAkRegisteredObj*)GetOwner());
			pNode->Release();
		}
	}
	m_ListModifiedNodes.Term();
}

AKRESULT CAkModifiedNodes::SetNodeAsModified(CAkParameterNodeBase* in_pNode)
{
	AKRESULT eResult = AK_Success;
	AKASSERT(in_pNode);

	WwiseObjectID wwiseId(in_pNode->ID(), in_pNode->IsBusCategory());

	if (m_ListModifiedNodes.Exists(wwiseId))
	{
		eResult = AK_Success;// technically useless since it was already initialized to AK_Success, but I let it there to keep the code clean.
	}
	else if (m_ListModifiedNodes.AddLast(wwiseId) == NULL)
	{
		eResult = AK_Fail;
	}

	return eResult;
}


CAkEmitter::CAkEmitter() : CAkTrackedGameObjComponent<GameObjComponentIdx_Emitter>()
	, m_fScalingFactor( 1.0f )
	, m_bPositionDirty( true )
{
}

CAkEmitter::~CAkEmitter()
{
	m_arCachedEmitListPairs.Term();
}

void CAkEmitter::SetPosition(
	const AkChannelEmitter* in_aPositions,
	AkUInt32 in_uNumPositions,
	AK::SoundEngine::MultiPositionType in_eMultiPositionType
	)
{
#ifndef AK_OPTIMIZED
	bool bChanged = false;
	if (AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataGameObjPosition))
	{
		bChanged = (in_eMultiPositionType != m_PositionRegistry.GetMultiPositionType()) || !m_PositionRegistry.PositionsEqual(in_aPositions, in_uNumPositions);
	}
#endif
	m_PositionRegistry.SetPosition( in_aPositions, in_uNumPositions );
	m_PositionRegistry.SetMultiPositionType( in_eMultiPositionType );
	m_bPositionDirty = true;
#ifndef AK_OPTIMIZED
	if (bChanged && GetOwner())
		AkMonitor::AddChangedGameObject(GetOwner()->ID());
#endif
}

void CAkEmitter::BuildVolumeRaysHelper(AkVolumeDataArray& out_arVolumeData, const AkListenerSet& in_listeners, const CAkConnectedListeners& connectedListeners, const AkRayVolumeData*& io_pShortestRay, const CAkListener*& io_pClosestListener)
{
	AkReal32 fMinDist = AK_UPPER_MAX_DISTANCE;

	AkUInt32 uNumPosition = GetPosition().GetNumPosition();
	AkUInt32 uNumListeners = in_listeners.Length();
	AkUInt32 uNumRays = uNumPosition * uNumListeners;

	AkUInt32 origLen = out_arVolumeData.Length();
	if (!out_arVolumeData.Resize(origLen + uNumRays))
		return;// BAIL!

	const AkChannelEmitter * posEmitter = GetPosition().GetPositions();
	AkGameRayParams * arRayParams = (AkGameRayParams *)AkAlloca(uNumPosition * sizeof(AkGameRayParams));

	AkRayVolumeData* pRay = out_arVolumeData.Data() + origLen;

	for (AkListenerSet::Iterator it = in_listeners.Begin(); it != in_listeners.End(); ++it)
	{
		const CAkListener * pListener = CAkListener::GetListenerData(*it);
		if (pListener != NULL)
		{
			GetPosition().GetGameRayParams(pListener->ID(), uNumPosition, arRayParams);

			// isDirectOutputListener, as opposed to an aux-send listener.
			bool isDirectOutputListener = connectedListeners.GetUserAssocs().GetListeners().Contains(*it);

			for (AkUInt32 posIdx = 0; posIdx < uNumPosition; posIdx++, ++pRay)
			{
				pRay->UpdateRay(posEmitter[posIdx].position, arRayParams[posIdx], pListener->ID(), posEmitter[posIdx].uInputChannels);
				AkReal32 fScaledDistance = pListener->ComputeRayDistanceAndAngles(GetScalingFactor(), *pRay);

				// For priority handling.
				if (isDirectOutputListener && fScaledDistance < fMinDist)
				{
					fMinDist = fScaledDistance;
					io_pShortestRay = pRay;
					io_pClosestListener = pListener;
				}
			}
		}
	}
}

void CAkEmitter::BuildCachedVolumeRays()
{
	const CAkListener * pClosestListener = NULL;
	const AkRayVolumeData * pShortestRay = NULL;

	const CAkConnectedListeners& connectedListeners = ((CAkRegisteredObj*)GetOwner())->GetConnectedListeners();
	const AkListenerSet & listeners = connectedListeners.GetListeners();

	m_arCachedEmitListPairs.RemoveAll();
	
	BuildVolumeRaysHelper(m_arCachedEmitListPairs, listeners, connectedListeners, pShortestRay, pClosestListener);

	if (pClosestListener && pShortestRay)
	{
		UpdateBuiltInParamValues(*pShortestRay, *pClosestListener);
	}
}

void CAkEmitter::BuildVolumeRays(AkVolumeDataArray& out_arVolumeData, AkListenerSet& in_excludedListeners)
{
	const CAkListener * pClosestListener = NULL;
	const AkRayVolumeData * pShortestRay = NULL;

	const CAkConnectedListeners& connectedListeners = ((CAkRegisteredObj*)GetOwner())->GetConnectedListeners();
	AkAutoTermListenerSet listeners;

	listeners.Copy(connectedListeners.GetListeners());

	AkSubtraction(listeners, in_excludedListeners);

	BuildVolumeRaysHelper(out_arVolumeData, listeners, connectedListeners, pShortestRay, pClosestListener);

	// Note that the build in game parameters are not updated by this function.
}

void CAkEmitter::UpdateCachedPositions()
{
	if (m_bPositionDirty)
 	{
		BuildCachedVolumeRays();

		m_bPositionDirty = false;
	}
}

AKRESULT CAkEmitter::SetObjectObstructionAndOcclusion(
		AkGameObjectID		in_uListenerID,
		AkReal32			in_fObstructionValue,
		AkReal32			in_fOcclusionValue
		)
{
	AKRESULT res = AK_Fail;

	AkObstructionOcclusionValues values;
	values.obstruction = in_fObstructionValue;
	values.occlusion = in_fOcclusionValue;

	res = m_PositionRegistry.SetObstructionOcclusion(in_uListenerID, &values, 1);

	m_bPositionDirty = true;

	return res;
}

AKRESULT CAkEmitter::SetMultipleObstructionAndOcclusion(
		AkGameObjectID in_uListenerID,
		AkObstructionOcclusionValues* in_ObstructionOcclusionValue,
		AkUInt32 in_uNumObstructionOcclusion
		)
{
	AKRESULT res = AK_Fail;

	m_bPositionDirty = true;

	res = m_PositionRegistry.SetObstructionOcclusion(in_uListenerID, in_ObstructionOcclusionValue, in_uNumObstructionOcclusion);

	return res;
}

void CAkEmitter::SetSpreadAndFocusValues(AkGameObjectID in_uListenerID, AkReal32* in_spread, AkReal32* in_focus, AkUInt32 in_uNumValues)
{
	m_bPositionDirty = true;
	m_PositionRegistry.SetSpreadAndFocusValues(in_uListenerID, in_spread, in_focus, in_uNumValues);
}

void CAkEmitter::UpdateBuiltInParamValues(const AkRayVolumeData& in_volumeRay, const CAkListener& in_listener)
{
	AkRTPCKey key((CAkRegisteredObj*)GetOwner());

	AkReal32 az, el;
	in_listener.ComputeSphericalCoordinates(in_volumeRay, az, el);
	g_pRTPCMgr->SetBuiltInParamValue(BuiltInParam_Distance, key, in_volumeRay.Distance());
	g_pRTPCMgr->SetBuiltInParamValue(BuiltInParam_Azimuth, key, AkMath::ToDegrees(az));
	g_pRTPCMgr->SetBuiltInParamValue(BuiltInParam_Elevation, key, AkMath::ToDegrees(el));
	//g_pRTPCMgr->SetBuiltInParamValue(BuiltInParam_EmitterCone, key, AkMath::ToDegrees(in_volumeRay.EmitterAngle()));

	//g_pRTPCMgr->SetBuiltInParamValue(BuiltInParam_Obsruction, key, in_volumeRay.fObstruction);
	//g_pRTPCMgr->SetBuiltInParamValue(BuiltInParam_Occlusion, key, in_volumeRay.fOcclusion);
	//g_pRTPCMgr->SetBuiltInParamValue(BuiltInParam_ListenerCone, key, AkMath::ToDegrees(in_volumeRay.ListenerAngle()));
}



