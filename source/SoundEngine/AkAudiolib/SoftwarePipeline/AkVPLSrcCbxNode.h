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
// AkVPLSrcCbxNode.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_VPL_SRC_CBX_NODE_H_
#define _AK_VPL_SRC_CBX_NODE_H_

#include "AkPBI.h"
#include "AkVPLLPFNode.h"
#include "AkVPLPitchNode.h"
#include "AkVPLVolAutmNode.h"
#include "AkVPLSrcNode.h"
#include "AkMixConnection.h"
#include "AkVPL3dMixable.h"
#include "AkMarkersQueue.h"

#define MAX_NUM_SOURCES			2			// Max no. of sources in sample accurate container. 

#define MIMINUM_SOURCE_STARVATION_DELAY		(20)	//~400ms hysteresis on starvation logs.

class CAkVPLFilterNodeBase;
class CAkVPLSrcCbxNode;

struct AkVPLSrcCbxRec
{
	AkVPLSrcCbxRec();
	void ClearVPL();

	CAkVPLBQFNode 				m_BQF;
	CAkVPLFilterNodeBase *		m_pFilter[AK_NUM_EFFECTS_PER_OBJ];
	CAkVPLVolAutmNode			m_VolAutm;

	AkForceInline CAkVPLBQFNode * Head() { return &m_BQF; }
};


// CBX node: a Voice object which may combine two CAkVPLSrcNodes (in the case of sample-accurate transitions).
class CAkVPLSrcCbxNode :	public CAkVPL3dMixable
{
	friend class AkVPL;
	friend class CAkLEngine;
	friend class CAkVPLPitchNode;

public:
	CAkVPLSrcCbxNode * pNextItem; // for AkListVPLSrcs

	CAkVPLSrcCbxNode();
	virtual ~CAkVPLSrcCbxNode();

	void Term();
	void Start();
	void Stop();
	void Pause();
	void Resume();
	void StopLooping(CAkPBI * in_pCtx);
	void SeekSource();
	AKRESULT TrySwitchToNextSrc(const AkPipelineBuffer &io_state);
	void SwitchToNextSrc();

	bool StartRun( AkVPLState & io_state );
	void GetOneSourceBuffer( AkVPLState & io_state );
	void GetBuffer( AkVPLState & io_state );
	void ConsumeBuffer( AkVPLState & io_state );
	void ReleaseBuffer();
	AKRESULT AddPipeline();

	bool IsUsingThisSlot(const CAkUsageSlot* in_pUsageSlot);
	bool IsUsingThisSlot(const AkUInt8* in_pData);
	AKRESULT FetchStreamedData(CAkPBI * in_pCtx);

	void UpdateFx(AkUInt32 in_uFXIndex);
	void UpdateSource(const AkUInt8* in_pOldDataPtr);

	void SetFxBypass(
		AkUInt32 in_bitsFXBypass,
		AkUInt32 in_uTargetMask /* = 0xFFFFFFFF */
		);
	void SetFxBypass( AkUInt32 in_fx, AkInt16 in_bypass );
	void UpdateBypass( AkUInt32 in_fx, AkInt16 in_bypassOffset );

	void RefreshBypassFx();
	void RefreshBypassFx(AkUInt32 in_uFXIndex);

	AkForceInline const AkVPLSrcCbxRec& GetVPLSrcCbx() const { return m_cbxRec; }
	inline VPLNodeState GetState() { return m_eState; }
	inline AkReal32 GetLastRate() { return m_pPitch ? m_pPitch->GetLastRate() : 1.0f; }

	void				HandleSourceStarvation();

	// For monitoring.
	inline bool			IsVirtualVoice() const { return !m_bAudible; }
	AkReal32 BehavioralVolume() const; //linear

#ifndef AK_OPTIMIZED	
	inline AkReal32		GetEnvelope() { return m_fLastEnvelope; }
	inline AkReal32		GetNormalizationMonitoring() { return m_fNormalizationGain; }
	
	void RecapPluginParamDelta();
#endif

	AkForceInline bool	PipelineAdded() { return m_bPipelineAdded; }
	void				RemovePipeline( AkCtxDestroyReason in_eReason );

	virtual void RelocateMedia(AkUInt8* in_pNewMedia, AkUInt8* in_pOldMedia);

	CAkVPLSrcNode**		GetSources(){ return m_pSources; }

	AkForceInline AkReal32 LastLPF() const { return m_cbxRec.m_BQF.GetLPF(); }
	AkForceInline AkReal32 LastHPF() const { return m_cbxRec.m_BQF.GetHPF(); }

	AKRESULT			AddSrc(CAkPBI * in_pCtx, bool in_bActive);
	AKRESULT			AddSrc(CAkVPLSrcNode * in_pSrc, bool in_bActive, bool in_bFirstTime);

	// Compute volumes of all emitter-listener pairs for this sound. 
	void ComputeVolumeRays();

	// AK::IAkVoicePluginInfo interface
	virtual AkPlayingID GetPlayingID() const { return GetPBI()->GetPlayingID(); }
	virtual AkPriority GetPriority() const { return GetPBI()->GetPriority(); }
	virtual AkPriority ComputePriorityWithDistance(AkReal32 in_fDistance) const { return GetPBI()->ComputePriorityWithDistance(in_fDistance); }
	
	// AK::IAkGameObjectPluginInfo interface
	inline AkInt32 SrcProcessOrder() const
	{				
		AkInt32 uProcessOrder = 0;
		for( AkInt32 i=0; i < MAX_NUM_SOURCES; ++i )
		{
			if( m_pSources[ i ] != NULL)
				uProcessOrder = AkMax(uProcessOrder, m_pSources[ i ]->SrcProcessOrder());
		}
		return uProcessOrder;
	}

	inline CAkPBI* GetPBI() const { return static_cast<CAkPBI*>(m_pContext); }

	AkForceInline void SetMixableBuffer() { m_pMixableBuffer = &m_vplState; }

	// Prepare buffer for next frame (if the VPL is using an asynchronous decoder)
	void PrepareNextBuffer();

	void NotifyMarkers(AkPipelineBuffer * in_pBuffer);

	AkForceInline CAkMarkersQueue * GetPendingMarkersQueue() { return &m_PendingMarkers; }
	AkForceInline AkMarkerNotificationRange GetPendingMarkers() const { return m_PendingMarkers.Front(); }

protected:
	void ClearVPL();

	virtual AkReal32 GetAnalyzedEnvelope()
	{
		return m_pSources[0]->GetAnalyzedEnvelope(m_pPitch ? m_pPitch->GetNumBufferedInputSamples() : 0);
	}	

	void RestorePreviousVolumes(AkPipelineBuffer* AK_RESTRICT io_pBuffer);
	AKRESULT SourceTimeSkip(AkUInt32 in_uFrames);

	// Helpers.
	// 
	void SetAudible( CAkPBI * in_pCtx, bool in_bAudible );

	AkVPLSrcCbxRec		m_cbxRec;
	CAkVPLSrcNode *		m_pSources[MAX_NUM_SOURCES];	// [0] == Current, [1] == Next
	CAkVPLPitchNode * 	m_pPitch;

	VPLNodeState m_eState;

	CAkMarkersQueue m_PendingMarkers;

	//Not uselessly initialized.
	AkVirtualQueueBehavior m_eVirtualBehavior;
	AkBelowThresholdBehavior m_eBelowThresholdBehavior;

	AkUInt8				m_bHasStarved:1;
	AkUInt8				m_bPipelineAdded:1;

#ifndef AK_OPTIMIZED
	AkUInt8				m_iWasStarvationSignaled;// must signal when == 0, reset to MIMINUM_SOURCE_STARVATION_DELAY.
#endif

	AkVPLState m_vplState;

	AkChannelConfig		m_channelConfig;

	void				UpdateMakeUpLinearNormalized( CAkPBI* pSrcContext, CAkVPLSrcNode* pSrcNode );
	AkReal32			m_previousMakeUpGainDB;
	AkReal32			m_previousMakeUpGainLinearNormalized;
};

#endif //_AK_VPL_SRC_CBX_NODE_H_
