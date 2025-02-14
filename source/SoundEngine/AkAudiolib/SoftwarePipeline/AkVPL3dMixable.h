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
// AkVPLSrcCbxNode.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_VPL_3d_MIXABLE_H_
#define _AK_VPL_3d_MIXABLE_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include "AkMixConnection.h"
#include "AkBusCtx.h"
#include "AkBehavioralCtx.h"

struct SpeakerVolumeMatrixCallback;


class CAkVPL3dMixable : public AK::IAkVoicePluginInfo
{
	friend class AkVPL;
	friend class CAkLEngine;

public:
	CAkVPL3dMixable() : m_pContext(NULL)
					  , m_fBehavioralVolume(0)
					  , m_fDownstreamGainDB(AK_SAFE_MINIMUM_VOLUME_LEVEL)
#ifndef AK_OPTIMIZED
					  , m_fWindowTop(0)
					  , m_fLastEnvelope(0)
					  , m_fNormalizationGain(1.f)
#endif
					  , m_bAudible(true)
					  , m_bFirstBufferProcessed(false)
					  , m_bIsABus(0)
#ifndef AK_OPTIMIZED
					  , m_bVisited(false)
#endif
					  , m_pMixableBuffer(NULL)
	{}

	virtual ~CAkVPL3dMixable();

	inline bool IsABus() const { return m_bIsABus != 0; }

	inline AkUInt32 NumDirectConnections()
	{
		AkUInt32 uCount = 0;
		for (AkMixConnectionList::Iterator it = m_connections.Begin(); it != m_connections.End(); ++it)
		{
			if ((*it)->IsTypeDirect())
				++uCount;
		}
		return uCount;
	}

	// Returns first direct connection, NULL if none found.
	const AkMixConnection * GetFirstDirectConnection();

	// Get max downstream gain of all direct connections.
	inline AkReal32 GetDownstreamGainDB() const { return m_fDownstreamGainDB; }

	// Compute volumes of all emitter-listener pairs for this sound. 
	// NOTE: non-virtual and defined in the subclasses for performance reasons.
	//void ComputeVolumeRays();
	bool ManageAuxSends();
protected:
	
	inline void _ComputeVolumeRays()
	{
		// Get volumes on this node coming from hierarchy.
		m_fBehavioralVolume = GetContext()->ComputeBehavioralNodeVolume();
		GetContext()->_ComputeVolumeRays();
	}

public:

	// Update connections: Compute logical volumes of all connections/paths (auxiliary and dry).
	// Set in_bEvaluateAudibility to compute audibility based on cumulative downstream volume. Bus connections should not evaluate it since they are unaware of the 
	// upstream gain stages. Voices can because there is nothing upstream.
	void UpdateConnections(bool in_bEvaluateAudibility);

	// Get sum of send values to aux bus specified by aux bus context (ID & game object). 
	// Sends are in parallel, so io_sendValue takes the contribution of all identical sends (hence the addition of linear values).
	// Returns true if the aux bus was found, false otherwise.
	inline bool FindSendValue(const AK::CAkBusCtx & in_busCtx, AkReal32 & out_fsendValue, AkReal32 & out_fsendLPF, AkReal32 & out_fsendHPF)
	{
		out_fsendValue = 0.f;
		out_fsendLPF = 0.f;
		out_fsendHPF = 0.f;
		bool bFound = false;
		for (AkAuxSendArray::Iterator it = m_arSendValues.Begin(); it != m_arSendValues.End(); ++it)
		{
			if ((*it).auxBusID == in_busCtx.ID() && (*it).listenerID == in_busCtx.GameObjectID())
			{
				out_fsendValue += (*it).fControlValue;
				out_fsendLPF += (*it).fLPFValue;
				out_fsendHPF += (*it).fHPFValue;
				bFound = true;
			}
		}
		return bFound;
	}
	inline AkUInt32 GetNumSends() const { return m_arSendValues.Length(); }
	inline const AkAuxSendArray & GetSendsArray() const { return m_arSendValues; }

	AkUInt32 GetNumForwardConnectionsToDepth(AkInt32 in_iDepth);

	// Find connection pointing to given bus context.
	inline AkMixConnection * FindConnection(AK::CAkBusCtx & in_output) { return m_connections.Find(in_output); }

	// Add output bus: if it fails the voice needs to remain in a consistent state. It will just not output sound in this graph.
	void AddOutputBus(AkVPL *in_pVPL, AkConnectionType in_eConnectionType);

	AkMixConnectionList::IteratorEx RemoveOutputBus(AkMixConnectionList::IteratorEx& io_rIter);

	void RemoveOutputBus(AkVPL *in_pVPL);
	void RemoveAllOutputBusses();

	// Destroys all connections *not to* graphs specified by in_currentListeners.
	inline void DestroyObsoleteConnections(const AkListenerSet& in_currentListeners)
	{
		AkMixConnectionList::IteratorEx it = m_connections.BeginEx();
		while (it != m_connections.End())
		{
			if ((*it)->IsTypeDirect() && !in_currentListeners.Contains((*it)->GetListenerID()))
				it = RemoveOutputBus(it);
			else
				++it;
		}
	}

	// Destroys all connections *not to* graphs to in_notToListener.
	inline void DestroyObsoleteConnections(AkGameObjectID in_notToListener)
	{
		AkMixConnectionList::IteratorEx it = m_connections.BeginEx();
		while (it != m_connections.End())
		{
			if ((*it)->IsTypeDirect() && (*it)->GetListenerID() != in_notToListener)
				it = RemoveOutputBus(it);
			else
				++it;
		}
	}

	CAkBehavioralCtx* GetContext() const { return m_pContext; }

	inline AkMixConnectionList::Iterator BeginConnection(){return m_connections.Begin();}
	inline AkMixConnectionList::Iterator EndConnection(){return m_connections.End();}
	inline bool HasConnections() const { return !m_connections.IsEmpty(); }

	virtual AkUniqueID GetAudioNodeID() { return GetContext()->GetSoundID(); }

	// AK::IAkGameObjectPluginInfo interface
	virtual AkGameObjectID GetGameObjectID() const { return GetContext()->GetGameObjectPtr()->ID(); }
	virtual AkUInt32 GetNumEmitterListenerPairs() const { return GetContext()->GetGameObjectPtr()->GetNumEmitterListenerPairs(); }
	virtual AKRESULT GetEmitterListenerPair(
		AkUInt32 in_uIndex,
		AkEmitterListenerPair & out_emitterListenerPair
		) const { return GetContext()->GetEmitterListenerPair( in_uIndex, out_emitterListenerPair ); }
	virtual AkUInt32 GetNumGameObjectPositions() const { return GetContext()->GetNumGameObjectPositions(); }
	virtual AK::SoundEngine::MultiPositionType GetGameObjectMultiPositionType() const { return GetContext()->GetGameObjectMultiPositionType(); }
	virtual AkReal32 GetGameObjectScaling() const { return GetContext()->GetGameObjectScaling(); }
	virtual AKRESULT GetGameObjectPosition(
		AkUInt32 in_uIndex,
		AkSoundPosition & out_position
		) const { return GetContext()->GetGameObjectPosition( in_uIndex, out_position ); }
	virtual bool GetListeners(AkGameObjectID* out_aListenerIDs, AkUInt32& io_uSize) const
	{
		return GetContext()->GetListeners(out_aListenerIDs, io_uSize);
	}
	virtual AKRESULT GetListenerData(
		AkGameObjectID in_uListenerID,
		AkListener & out_listener
		) const { return GetContext()->GetListenerData( in_uListenerID, out_listener ); }

	inline bool IsAudible()
	{
		bool bAllNextSilent = true;//unused
		return EvaluateAudibility(bAllNextSilent);
	}

	const AkInputConnectionList & Inputs() { return m_inputs; }
	
#ifndef AK_OPTIMIZED
	void ClearMonitoringMuteSolo(bool bIsSoloActive)
	{
		for (CAkMixConnections::Iterator it = BeginConnection(); it != EndConnection(); ++it)
			(*it)->ClearMuteSolo(bIsSoloActive);
	}

	void SetMonitoringMuteSolo_Bus(bool bMuteAll, bool bSoloAll, bool bMuteDirectOnly, bool bSoloDirectOnly, bool bSoloImplicit);
	void SetMonitoringMuteSolo_Voice(bool bMute, bool bSolo);
	void SetMonitoringMuteSolo_DirectOnly(bool bMute, bool bSolo);

private:
	void PropagateSoloUpstream();
	void PropagateSoloDownstream();
public:
#endif

protected:

	void DisconnectAllInputs();

	virtual AkReal32 GetAnalyzedEnvelope(){	return 0.f;	}

	// Helpers.
	// 
	void SetPreviousSilentOnAllConnections( bool in_bPrevSilent );

	void UpdateHDR();

	// Evaluate max volume against threshold and compute complete set of 
	// speaker volumes if needed.
	void GetVolumes( 
		CAkBehavioralCtx* AK_RESTRICT in_pContext, 
		AkChannelConfig in_uInputConfig,
		AkReal32 in_fMakeUpGain,
		bool in_bStartWithFadeIn,
		bool & out_bNextSilent,		// True if sound is under threshold at the end of the frame
		bool & out_bAudible,		// False if sound is under threshold for whole frame: then it may be stopped or processsed as virtual.
		SpeakerVolumeMatrixCallback* pMatrixCallback //
		);

	AkForceInline bool EvaluateAudibility(bool& out_bAllNextSilent)
	{
		out_bAllNextSilent = true;
		bool bAllPrevSilent = true;
		for (AkMixConnectionList::Iterator it = m_connections.Begin(); it != m_connections.End(); ++it)
		{
			bAllPrevSilent = bAllPrevSilent && (*it)->IsPrevSilent();
			out_bAllNextSilent = out_bAllNextSilent && (*it)->IsNextSilent();
		}

		if (m_bFirstBufferProcessed)
		{
			// No. Not audible if all connections are silent on both edges.
			return !out_bAllNextSilent || !bAllPrevSilent;
		}
		else
		{
			// Yes. Audibility depends on next edge only, and previous edge inherits from NeedsFadeIn flag in PBI.
			return !out_bAllNextSilent;
		}
	}

	// Voice-specific; put in base behavioral context for performance.
	inline bool IsForcedVirtualized() const { return m_pContext->IsForcedVirtualized(); }
	

	CAkBehavioralCtx* m_pContext;

	AkReal32			m_fBehavioralVolume;		// listener-independent volume (common to all rays), linear.
	AkReal32			m_fDownstreamGainDB;		// Downstream gain in dB (max of all connections).
	
#ifndef AK_OPTIMIZED
	AkReal32			m_fWindowTop;				// Window top for this voice in dB (used for monitoring).
	AkReal32			m_fLastEnvelope;			// Last envelope value, in dB, for monitoring.
	AkReal32			m_fNormalizationGain;		// Last normalization gain, linear, for monitoring.
#endif

	AkInputConnectionList m_inputs;
	CAkMixConnections	m_connections;					// Voice connections to mixing graphs: All voice data that need to exist independently for each graph.
	
	AkAuxSendArray		m_arSendValues;

	struct PeakPerHDRBus
	{
		AkHdrBus * key;
		AkReal32 peak;
	};

	typedef AkSortedKeyArray<AkHdrBus *, PeakPerHDRBus, ArrayPoolLEngineDefault> SortedArrayHdrPeak;
	SortedArrayHdrPeak m_mapHdrBusToPeak;

	// Volume status. 
	// m_bAudible is false when the sound was completely silent for the entire previous audio frame;
	// The voice's state (stopping, pausing, virtual) depends on it.
	AkUInt8				m_bAudible : 1;					// For virtual voices

	// Volume status. 
	AkUInt8				m_bFirstBufferProcessed : 1;
	AkUInt8				m_bIsABus : 1;
#ifndef AK_OPTIMIZED
	AkUInt8				m_bVisited : 1;
#endif
public:
	AkAudioBuffer * m_pMixableBuffer;
};


#endif //_AK_VPL_SRC_CBX_NODE_H_
