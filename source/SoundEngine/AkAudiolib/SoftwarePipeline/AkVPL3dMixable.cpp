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

/////////////////////////////////////////////////////////////////////
//
// AkVPLSrcCbxNode.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include "AkVPL3dMixable.h"
#include "AkLEngine_SoftwarePipeline.h"
#include "Ak3DListener.h"
#include "AkEnvironmentsMgr.h"

CAkVPL3dMixable::~CAkVPL3dMixable()
{
	//Remove all outputs
	for (AkMixConnectionList::IteratorEx itCon = m_connections.BeginEx(); itCon != EndConnection(); )
	{
		itCon = RemoveOutputBus(itCon);
	}

	m_arSendValues.Term();
	m_mapHdrBusToPeak.Term();
	AKASSERT(m_inputs.IsEmpty());
	m_inputs.Term();
}

void CAkVPL3dMixable::DisconnectAllInputs()
{
	//Remove all inputs
	while (!m_inputs.IsEmpty())
	{
		bool bFound = false;
		AkMixConnection* pInput = m_inputs.First();
		for (AkMixConnectionList::IteratorEx itCon = pInput->Owner()->m_connections.BeginEx(); itCon != pInput->Owner()->EndConnection(); ++itCon)
		{
			if ((*itCon)->GetOutputBus() == this)
			{
				pInput->Owner()->RemoveOutputBus(itCon);
				bFound = true;
				break;
			}
		}

		AKASSERT(bFound);
		if (!bFound)
			m_inputs.RemoveFirst();
	}
}

// Returns first direct connection, NULL if none found.
const AkMixConnection * CAkVPL3dMixable::GetFirstDirectConnection()
{
	for (AkMixConnectionList::Iterator it = m_connections.Begin(); it != m_connections.End(); ++it)
	{
		if ((*it)->IsTypeDirect())
			return (*it);
	}
	return NULL;
}

// Compute max volume of all paths (auxiliary and dry), taking pre-computed
// bus gains into account. Obsolete connections are destroyed.
// Set in_bEvaluateAudibility to compute audibility based on cumulative downstream volume. Bus connections should not evaluate it since they are unaware of the 
// upstream gain stages. Voices can because there is nothing upstream.
void CAkVPL3dMixable::UpdateConnections(bool in_bEvaluateAudibility)
{
	// Evaluate source envelope volume for HDR (if applicable).
	CAkBehavioralCtx * AK_RESTRICT pContext = GetContext();
	AKASSERT(pContext != NULL);
	const AkSoundParams & params = pContext->GetEffectiveParams();
	AkReal32 fRelativeLoudness = (params.bEnableEnvelope) ? GetAnalyzedEnvelope() : 0.f;
#ifndef AK_OPTIMIZED
	m_fLastEnvelope = fRelativeLoudness;
#endif
	m_mapHdrBusToPeak.RemoveAll();
	
	m_fDownstreamGainDB = AK_SAFE_MINIMUM_VOLUME_LEVEL;

	pContext->OpenSoundBrace();
	
	AkReal32 fBehavioralVolume = m_fBehavioralVolume * pContext->GetModulatorData().GetPeakOutput(AkRTPCBitArray(1ULL << RTPC_Volume));
	const AkVolumeDataArray & rays = pContext->GetRays();
	AkDeltaMonitor::LogFilterRays((AkUInt8)rays.Length());
	
	AkMixConnectionList::IteratorEx itConnection = m_connections.BeginEx();
	while (itConnection != m_connections.End())
	{
		// Connection volumes: behavioral (common to all connections of this Mixable), connection-specific (dry or send values, HDR), and ray-specific (attenuation curves, obs/occ).
		// Furthermore, MaxVolume (used for logic - virtual voices, HDR) is the sum of all these gains, combined with the target bus' downstream gain.
		AkReal32 fConnectionVolume = 0.f;
		AkReal32 fRayVolume = 0.f;

		AkGameObjectID connectionListenerID = (*itConnection)->GetListenerID();

		// Check all rays that apply to this connection, and get proper gain according to connection type.
		AkConnectionType eType = (*itConnection)->GetType();
		AkVolumeDataArray::Iterator it = rays.Begin();
		if (eType == ConnectionType_Direct)
		{
			fConnectionVolume = pContext->GetOutputBusVolumeValue();	// "output bus volume" + collapsed control bus volume contributions.
			
			if (pContext->HasListenerRelativeRouting())
			{
				while (it != rays.End())
				{
					if (connectionListenerID == (*it).ListenerID())
					{
						if (fRayVolume < (*it).fDryMixGain)
							fRayVolume = (*it).fDryMixGain;
					}
					++it;
				}
				fConnectionVolume *= pContext->GetDryLevelValue(connectionListenerID); // SDK-controller direct volume per game object.
			}
			else
			{
				// This connection does not depend on an emitter-listener association.
				fRayVolume = 1;
			}

			// Set connection-specific filtering values.
			(*itConnection)->fLPF = params.OutputBusLPF();
			(*itConnection)->fHPF = params.OutputBusHPF();
		}
		else
		{
			// Aux send.

			// Find control value associated to this send and multiply it.
			AkReal32 fSendValue;
			AkReal32 fSendLPF;
			AkReal32 fSendHPF;
			if ( FindSendValue( (*itConnection)->GetOutputBusCtx(), fSendValue, fSendLPF, fSendHPF ) )
				fConnectionVolume = fSendValue;
			else
			{
				// Send has been disconnected. Wait until connection became silent before deleting it.
				/// IMPORTANT: Check "next silent", which was set in previous frame. "Silent" flags for this frame are updated only below.
				/// REVIEW: If we destroyed direct connections too, it would be cleaner to garbage collect connection after updating them, below. 
				fConnectionVolume = 0.f;
				if ((*itConnection)->IsNextSilent())
				{
					// Unused send connection: get rid of it.
					itConnection = RemoveOutputBus(itConnection);
					continue;
				}
			}

			// Set connection-specific filtering values.
			(*itConnection)->fLPF = fSendLPF;
			(*itConnection)->fHPF = fSendHPF;

			// Get attenuation volume for this send type (max of all rays)
			if (pContext->HasListenerRelativeRouting())
			{
				if (eType == ConnectionType_GameDefSend)
				{
					while (it != rays.End())
					{
						if (connectionListenerID == (*it).ListenerID())
						{
							if (fRayVolume < (*it).fGameDefAuxMixGain)
								fRayVolume = (*it).fGameDefAuxMixGain;
						}
						++it;
					}
				}
				else if (eType == ConnectionType_UserDefSend)
				{
					while (it != rays.End())
					{
						if (connectionListenerID == (*it).ListenerID())
						{
							if (fRayVolume < (*it).fUserDefAuxMixGain)
								fRayVolume = (*it).fUserDefAuxMixGain;
						}
						++it;
					}
				}
				else
				{
					// Reflections sends, etc.
					fRayVolume = 1.f;
				}
			}
			else
			{
				// This connection does not depend on an emitter-listener association.
				fRayVolume = 1;
			}
		}

		// Compute max cumulative downstream volume.		
		AkReal32 fNodeVolume = fBehavioralVolume * fConnectionVolume * fRayVolume;
		if (fNodeVolume > 0.f)
		{
			AkReal32 fCumulDownstreamVolumeDB = AkMath::FastLinTodB(fNodeVolume);

			// Add contribution of downstream bus gains.
			fCumulDownstreamVolumeDB += (*itConnection)->GetOutputVPL()->DownstreamGainDB();

			// Set volumes on connection.
			// Mix volumes and ray volumes are the best estimates we have at the moment, can be modified later (e.g. to include HDR, make-up gain, or to deal with multiple positions).
			// They are set here nonetheless in case we decide not to compute this node's spatialization.
			(*itConnection)->UpdateVolumes(fRayVolume, fConnectionVolume, fCumulDownstreamVolumeDB, in_bEvaluateAudibility);			

			// Collect max cumulative downstream gain of all connections and store it on this node;
			// this is the cumulative gain that will be considered by upstream nodes.
			if (fCumulDownstreamVolumeDB > m_fDownstreamGainDB)
				m_fDownstreamGainDB = fCumulDownstreamVolumeDB;

			// HDR
			if ((*itConnection)->IsInHdr() 
				&& pContext->IsHDR()) // WG-44440 Disable HDR on sends if dry signal is not routed to HDR
			{
				// Push volume to HDR bus only if within the region of interest.
				if (-fRelativeLoudness < params.HDRActiveRange())
					(*itConnection)->GetHdrBus()->PushEffectiveVoiceVolume(fCumulDownstreamVolumeDB + fRelativeLoudness);

				// Gather.
				AkHdrBus *pHdr = (*itConnection)->GetHdrBus();
				bool bExists;
				PeakPerHDRBus * pPeakForThisHdrBus = m_mapHdrBusToPeak.Set(pHdr, bExists);
				if (pPeakForThisHdrBus)
				{
					// done inside Set: pPair->key = pHdr;

					// Note: transform the voice peak through the HDR gain computer for a proper comparison with the global HDR window, later. 
					AkReal32 peak = pHdr->GetMaxVoiceWindowTop((*itConnection)->GetCumulativeDownstreamVolumeDB());
					if (!bExists || (peak > pPeakForThisHdrBus->peak))
						pPeakForThisHdrBus->peak = peak;
				}
				else
				{
					// Failed to register this connection to the set of connections which will require HDR attenuation. It could be very loud... Better off muting it.
					(*itConnection)->UpdateVolumes(fRayVolume, 0.f, AK_SAFE_MINIMUM_VOLUME_LEVEL, true);
				}
			}			
		}
		else
		{
			// IMPORTANT: the combined node-connection's volume is 0 (i.e. -infinity dB), so this connection is silent, regardless of the downstream
			// gain. No downstream gain value will ever compensate for this. This is why we ignore in_bEvaluateAudibility and pass "true" here.
			// This is not just an optimization; the combined node-connection's volume may be zero because there are no rays, in which case we 
			// absolutely need to avoid trying to compute spatialization or position-dependent filtering (see GetVolumes).
			(*itConnection)->UpdateVolumes(fRayVolume, fConnectionVolume, AK_SAFE_MINIMUM_VOLUME_LEVEL, true);
		}

		if (eType != ConnectionType_ReflectionsSend && // Reflections don't require filtering.
			(!(*itConnection)->IsPrevSilent() || !(*itConnection)->IsNextSilent()) && 
			rays.Length())
		{
			CAkListener::ComputeFiltering3D(pContext, rays, (*itConnection));
		}

		++itConnection;
	}

	pContext->CloseSoundBrace();
}

void CAkVPL3dMixable::UpdateHDR()
{
	for (SortedArrayHdrPeak::Iterator itHdr = m_mapHdrBusToPeak.Begin(); itHdr != m_mapHdrBusToPeak.End(); ++itHdr)
	{
		// Take the max between the bus window top and this voice's peak value.
		// Note: the voice peak had to be transformed through the HDR gain computer for
		// a proper comparison. 
		AkHdrBus * pHdrBus = (*itHdr).key;
		AkReal32 windowTop = pHdrBus->GetHdrWindowTop();
		if (windowTop < (*itHdr).peak)
			windowTop = (*itHdr).peak;

		// Compute HDR attenuation:
		// HDR gain (in dB) = Voice peak - Window top - (Max volume - HDR bus downstream gain).
		// The part in parentheses represents the difference between the actual voice volume and the volume that 
		// is seen by the HDR bus (in other words, all submixing stages between this voice and the HDR bus).
		// Since Voice Peak = Max Volume, we can reduce the formula to
		// HDR gain = Hdr downstream gain - Window top + Behavioral volume.
		AkReal32 hHdrGainDB = -windowTop + pHdrBus->DownstreamGainDB();
		AkReal32 fHdrGain = AkMath::dBToLin(hHdrGainDB);

		// Update volume on all connections under this HDR bus.
		for (AkMixConnectionList::Iterator it = m_connections.Begin(); it != m_connections.End(); ++it)
		{
			if ((*it)->GetHdrBus() == pHdrBus)
			{
				// Update volumes.
				(*it)->SetVolume((*it)->GetVolume() * fHdrGain);
				(*it)->SetCumulativeDownstreamVolumeDB((*it)->GetCumulativeDownstreamVolumeDB() + hHdrGainDB, true);

				// Save Window Top for profiling.
#ifndef AK_OPTIMIZED
				(*it)->SetWindowTopDB(-hHdrGainDB);
#endif
			}
		}
	}
}

bool CAkVPL3dMixable::ManageAuxSends()
{
	GetContext()->CalcEffectiveParams();

	m_arSendValues.RemoveAll();

	// Compute send values for this frame.
	bool bAuxRoutable = GetContext()->IsAuxRoutable();
	if (bAuxRoutable)
	{
		GetContext()->GetAuxSendsValues(m_arSendValues);
		
		// Current solution to the FBI (from-bus issue).  
		// If an aux send is set to use-game-defined sends, we do not allow the aux bus to send to other aux busses within the same
		// game object. This allows us to have multiple instances of a plugin (eg. AkReflect), that also send to another aux bus (eg late reverb).
		// on the same game object.
		bool bAllowLocalSends = GetContext()->GetParameterNode()->NodeCategory() != AkNodeCategory_AuxBus;
		for (AkAuxSendArray::Iterator it = m_arSendValues.Begin(); it != m_arSendValues.End(); ++it)
		{
			if (GetContext()->GetGameObjectPtr()->ID() != (*it).listenerID || // don't allow direct feedback into oneself (oneself === {busID,objectID} tuple), _or even if the bus is different_ (spot reflector case).
				(*it).eAuxType == ConnectionType_UserDefSend ||      // allow feedback for user-defined sends
				bAllowLocalSends)									  // allow local sends only if not an aux bus
			{
				CAkSmartPtr<CAkBus> spAuxBus;
				spAuxBus.Attach((CAkBus*)g_pIndex->GetNodePtrAndAddRef((*it).auxBusID, AkNodeType_Bus));
				if (spAuxBus != NULL && spAuxBus->IsMixingBus()) // should test for the bus being an aux, but it suffices for it to be mixing.
					CAkLEngine::GetAuxBus(spAuxBus, (*it), this);
			}
		}
	}

	return bAuxRoutable;
}

void CAkVPL3dMixable::GetVolumes(
	CAkBehavioralCtx* AK_RESTRICT in_pContext,
	AkChannelConfig in_inputConfig,
	AkReal32 in_fMakeUpGain,
	bool in_bStartWithFadeIn,
	bool & out_bNextSilent,		// True if sound is under threshold at the end of the frame
	bool & out_bAudible,		// False if sound is under threshold for whole frame: then it may be stopped or processsed as virtual.
	SpeakerVolumeMatrixCallback * pMatrixCallback //optional callback parameter
	)
{
	// Test for voice audibility: !audible if all connections are silent at both edges of the buffer.
	bool bAllNextSilent;
	out_bAudible = EvaluateAudibility(bAllNextSilent);

	// Bail out now if not audible.
	if (!out_bAudible)
	{
		out_bNextSilent = bAllNextSilent;
		return;
	}

	// REVIEW: Special fix up needed for sources (AkVPLSrcCbxNode) made virtual by behavioral engine.
	if (IsForcedVirtualized())
	{
		out_bNextSilent = true;
		
		if (m_bFirstBufferProcessed)
		{
			bool bAllPrevSilent = true;
			for (AkMixConnectionList::Iterator it = m_connections.Begin(); it != m_connections.End(); ++it)
			{
				bAllPrevSilent = bAllPrevSilent && (*it)->IsPrevSilent();
				(*it)->SetCumulativeDownstreamVolumeDB(AK_SAFE_MINIMUM_VOLUME_LEVEL, true);
			}
			// Not audible if all connections are silent on both edges.
			if (bAllPrevSilent)
			{
				out_bAudible = false;
				return;
			}
		}
		else
		{
			for (AkMixConnectionList::Iterator it = m_connections.Begin(); it != m_connections.End(); ++it)
			{
				(*it)->SetPrevSilent(true);
				(*it)->SetCumulativeDownstreamVolumeDB(AK_SAFE_MINIMUM_VOLUME_LEVEL, true);
			}
			out_bAudible = false;
			return;
		}
	}

	// m_fBehavioralVolume on stack: loudness normalization needs to remain separate for monitoring.
	AkReal32 fBehavioralVolume = m_fBehavioralVolume;
	// Add makeup gain and loudness normalization, after evaluating against threshold.
	// fBehavioralVolume is updated after having been stored in m_fBehavioralVolume in order to avoid
	// reporting normalization gain in voice volume.
	fBehavioralVolume *= in_fMakeUpGain;

	// Prepare speaker matrix for this frame. Move new "Next" to "Previous", 
	// and get the largest output channel config (required by CAkListener::ComputeSpeakerMatrix)
	if (in_inputConfig.uNumChannels > 0)	// Skip computing of spatialization if no channels.
	{
		for (AkMixConnectionList::Iterator it = m_connections.Begin(); it != m_connections.End(); ++it)
		{
			AkMixConnection * pConnection = (*it);
			
			if ((pConnection->IsPrevSilent() && pConnection->IsNextSilent())			// Don't compute panning if connection is not audible; it will not be mixed.
				|| pConnection->ResetMatrixAndFilters(in_inputConfig) != AK_Success)	// Prepare speaker matrix for new frame.
				continue;
			
			// Set behavioral volume: Collapsed volumes of actor and voice bus hierarchies, fades, and connection volume.
			/// REVIEW Consider moving this in UpdateConnections
			pConnection->mixVolume.fNext = (!pConnection->IsNextSilent()) ? (fBehavioralVolume * pConnection->GetVolume()) : 0.f;

			if (in_pContext->HasListenerRelativeRouting() && 
				pConnection->GetType() != ConnectionType_ReflectionsSend)	// Reflections sends are on the same object and don't require panning.
			{
				if (in_pContext->GetRays().Length() > 0)
				{
					// Compute spatialization
					// Spatialization.
					CAkListener::ComputeSpeakerMatrix3D(
						in_pContext,
						in_pContext->GetRays(),
						in_inputConfig,
						pConnection);
				}
			}
			else
			{
				// Speaker panning.
				CAkListener::ComputeSpeakerMatrix2D(
					in_pContext,
					in_inputConfig,
					pConnection
					);
			}


			// Send callback notification to allow user modifications to volume
			if (pMatrixCallback)
				(*pMatrixCallback)(*pConnection);
			
			if (pConnection->IsPrevSilent())
			{
				// Not audible during last frame: Clear previous volume values to avoid any glitch
				// (usually happens with streamed sounds with frame offset larger than audio frame).
				// Since spatialization was not computed during last frame, use it as a starting point.
				pConnection->CopyNextToPrevSpatialization();
				pConnection->mixVolume.fPrev = 0.f;
			}
			else if (!m_bFirstBufferProcessed)
			{
				// First run: set all previous volume values equal to next values that were just computed.
				pConnection->CopyNextToPrevSpatialization();
				pConnection->CopyNextToPrevMixVolumes();
				if (in_bStartWithFadeIn)
					pConnection->mixVolume.fPrev = 0.f;
			}

#ifndef AK_OPTIMIZED
			if (AK_EXPECT_FALSE(pConnection->IsMonitoringMute()))
			{
				pConnection->Mute();
			}
#endif
		}

		in_pContext->UpdatePrevPosParams();
	}

	out_bNextSilent = bAllNextSilent;

} //GetVolumes

void CAkVPL3dMixable::AddOutputBus(AkVPL *in_pVPL, AkConnectionType in_eConnectionType)
{
	if (m_pContext)
		m_pContext->InvalidatePrevPosParams();	// force recomputing panning

	AkHdrBus * pHdrBus = static_cast<AkHdrBus *>(in_pVPL->GetHdrBus());
	AkMixConnection * pConnection = m_connections.Create(this, in_pVPL, pHdrBus, in_eConnectionType, !m_bAudible || m_bFirstBufferProcessed);
	if (pConnection)
	{
		in_pVPL->m_MixBus.m_inputs.AddLast(pConnection);
		AkDevice * pDevice = CAkOutputMgr::FindDevice(pConnection->GetOutputBusCtx());
		if (!pDevice || pDevice->EnsurePanCacheExists(pConnection->GetOutputConfig()) != AK_Success)
		{
			// Failure: remove device, because pipeline processing takes for granted that pan caches exist.
			RemoveOutputBus(in_pVPL);
		}
		if (IsABus())
			CAkLEngine::SetVPLsDirty();
	}
}

void CAkVPL3dMixable::RemoveAllOutputBusses()
{
	AkMixConnectionList::IteratorEx it = m_connections.BeginEx();
	while ( it != m_connections.End() )
		it = RemoveOutputBus(it);
}

AkMixConnectionList::IteratorEx CAkVPL3dMixable::RemoveOutputBus(AkMixConnectionList::IteratorEx& io_rIter)
{
	if (m_pContext)
		m_pContext->InvalidatePrevPosParams();	// force recomputing panning
	if (IsABus())
		CAkLEngine::SetVPLsDirty();
	(*io_rIter)->GetOutputBus()->m_inputs.Remove(*io_rIter);
	return m_connections.Remove(io_rIter);
}

void CAkVPL3dMixable::RemoveOutputBus(AkVPL *in_pVPL)
{
	AkMixConnectionList::IteratorEx it = m_connections.FindEx(in_pVPL);
	if (it != m_connections.End())
		RemoveOutputBus(it);
}

void CAkVPL3dMixable::SetPreviousSilentOnAllConnections(bool in_bPrevSilent)
{
	for (AkMixConnectionList::Iterator it = m_connections.Begin(); it != m_connections.End(); ++it)
		(*it)->SetPrevSilent(in_bPrevSilent);
}

AkUInt32 CAkVPL3dMixable::GetNumForwardConnectionsToDepth(AkInt32 in_iDepth)
{
	AkUInt32 uCount = 0;
	for (AkMixConnectionList::Iterator it = m_connections.Begin(); it != m_connections.End(); ++it)
	{
		if (!(*it)->GetFeedback() && (*it)->GetOutputVPL()->m_iDepth == in_iDepth)
			uCount++;
	}
	return uCount;
}

#ifndef AK_OPTIMIZED

void CAkVPL3dMixable::SetMonitoringMuteSolo_Bus(bool bMuteAll, bool bSoloAll, bool bMuteDirectOnly, bool bSoloDirectOnly, bool bSoloImplicit)
{
	if (bSoloAll || bSoloDirectOnly)
		PropagateSoloUpstream();

	if (bMuteAll || bSoloAll || bMuteDirectOnly || bSoloDirectOnly || bSoloImplicit)
	{
		for (AkMixConnectionList::Iterator itCon = m_connections.Begin(); itCon != m_connections.End(); ++itCon)
		{
			AkMixConnection* pConn = *itCon;

			bool bMute = bMuteAll || (bMuteDirectOnly && pConn->IsTypeDirect());
			bool bSolo = bSoloAll || (bSoloDirectOnly && pConn->IsTypeDirect());

			if (bMute)
				pConn->SetMute();
			else if (bSolo)
				pConn->SetSolo();
			else if (bSoloImplicit)
				pConn->SetImplicitSolo();

			if (bSolo && !pConn->GetOutputBus()->m_bVisited)
			{
				pConn->GetOutputBus()->m_bVisited = true;
				pConn->GetOutputBus()->PropagateSoloDownstream();
				pConn->GetOutputBus()->m_bVisited = false;
			}
		}
	}
}

void CAkVPL3dMixable::SetMonitoringMuteSolo_Voice(bool bMute, bool bSolo)
{
	if (bMute || bSolo)
	{
		for (AkMixConnectionList::Iterator itCon = m_connections.Begin(); itCon != m_connections.End(); ++itCon)
		{
			AkMixConnection* pConn = *itCon;
			if (bMute)
				pConn->SetMute();
			else if (bSolo)
				pConn->SetSolo();

			if (bSolo && !pConn->GetOutputBus()->m_bVisited)
			{
				pConn->GetOutputBus()->m_bVisited = true;
				pConn->GetOutputBus()->PropagateSoloDownstream();
				pConn->GetOutputBus()->m_bVisited = false;
			}
		}
	}
}

void CAkVPL3dMixable::SetMonitoringMuteSolo_DirectOnly(bool bMute, bool bSolo)
{
	if (bMute || bSolo)
	{
		for (AkMixConnectionList::Iterator itCon = m_connections.Begin(); itCon != m_connections.End(); ++itCon)
		{
			AkMixConnection* pConn = *itCon;
			if (pConn->IsTypeDirect())
			{
				if (bMute)
				{
					pConn->SetMute();
				}
				else if (bSolo)
				{
					pConn->SetSolo();

					if (!pConn->GetOutputBus()->m_bVisited)
					{
						pConn->GetOutputBus()->m_bVisited = true;
						pConn->GetOutputBus()->PropagateSoloDownstream();
						pConn->GetOutputBus()->m_bVisited = false;
					}
				}
			}
		}
	}
}

void CAkVPL3dMixable::PropagateSoloUpstream()
{
	for (AkInputConnectionList::Iterator itIn = m_inputs.Begin(); itIn != m_inputs.End(); ++itIn)
	{
		CAkVPL3dMixable* pMixable = (*itIn)->Owner();
		for (AkMixConnectionList::Iterator itCon = pMixable->m_connections.Begin(); itCon != pMixable->m_connections.End(); ++itCon)
		{
			AkMixConnection* pConn = *itCon;
			if (!pConn->IsMuteSoloExplicit())
			{
				if (pConn->GetOutputBus() == this)
					pConn->SetImplicitSolo();

				else if (!pConn->IsMonitoringSolo())
					pConn->SetImplicitMute();
			}
		}

		if (!pMixable->m_bVisited)
		{
			pMixable->m_bVisited = true;
			pMixable->PropagateSoloUpstream();
			pMixable->m_bVisited = false;
		}
	}
}

void CAkVPL3dMixable::PropagateSoloDownstream()
{
	for (AkMixConnectionList::Iterator itCon = m_connections.Begin(); itCon != m_connections.End(); ++itCon)
	{
		AkMixConnection* pConn = *itCon;
		
		if (!pConn->IsMuteSoloExplicit())
		{
			pConn->SetImplicitSolo();
		}

		if (!pConn->GetOutputBus()->m_bVisited)
		{
			pConn->GetOutputBus()->m_bVisited = true;
			pConn->GetOutputBus()->PropagateSoloDownstream();
			pConn->GetOutputBus()->m_bVisited = false;
		}
	}
}

#endif