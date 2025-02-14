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
// AkBehavioralCtx.h
//
//////////////////////////////////////////////////////////////////////

#ifndef AK_BEHAVIORAL_CTX_H
#define AK_BEHAVIORAL_CTX_H

#include "AkRTPCSubscriber.h"
#include "AkGen3DParams.h"
#include "AkMonitor.h"
#include "AkSoundParams.h"

class CAkAttenuation;
class CAkBus;
struct AkInitialSoundParams;
struct NotifParams;
class CAkSpatialAudioVoice;

//	CAkBehavioralCtx
//
//		-This is a 'snapshot' of the behavioral parameters that apply to a specific instance of an audio signal.  
//		-These parameters may change dynamically and be updated by RTPCs, etc.
//		-Positioning, mixing, volume, filtering parameters and code to manipulate them belong here.
//		-However, parameters and code related to source, file, seeking, virtual voice, etc *do not* belong here, but instead in CAkPBI.
//
class CAkBehavioralCtx:  public CAkParameterTarget
{
public:
	CAkBehavioralCtx * pNextLightItem; // For sound's PBI List

	CAkBehavioralCtx(CAkRegisteredObj* in_pGameObj, CAkParameterNodeBase* pSoundNode, bool in_bPlayDirectly);
	~CAkBehavioralCtx();

	AKRESULT PreInit(AkReal32 in_MaxDistance, AkPathInfo* in_pPathInfo, bool in_bAllowedToPlayIfUnderThreshold, AkMonitorData::NotificationReason& io_eReason, AkInitialSoundParams* in_pInitialParams, bool & out_bIsInitiallyUnderThreshold);
	virtual AKRESULT Init();
	virtual void Term(bool in_bFailedToInit);

	void SetParam(
		AkPluginParamID in_paramID,         ///< Plug-in parameter ID
		const void *	in_pParam,          ///< Parameter value pointer
		AkUInt32		in_uParamSize		///< Parameter size
		);

	virtual void GetModulatorTriggerParams(AkModulatorTriggerParams& out_params, bool in_bFirstPlay=true);

	// Trigger the release portion of the envelope modulators.
	void ReleaseModulators();

	// Gives the information about the associated game object
	//
	// Return - CAkRegisteredObj * - The game object and Custom ID
	inline CAkRegisteredObj * GetGameObjectPtr() const { return m_rtpcKey.GameObj(); }
	inline CAkEmitter * GetEmitter() const { return m_rtpcKey.GameObj()->GetComponent<CAkEmitter>(); }
	inline AkUInt32 GetNumGameObjectPositions() const { return GetEmitter()->GetPosition().GetNumPosition(); }
	inline AK::SoundEngine::MultiPositionType GetGameObjectMultiPositionType() const { return GetEmitter()->GetPosition().GetMultiPositionType(); }
	inline AkReal32 GetGameObjectScaling() const { return GetEmitter()->GetScalingFactor(); }
	AKRESULT GetGameObjectPosition(
		AkUInt32 in_uIndex,
		AkSoundPosition & out_position
		) const;
	inline const AkListenerSet& GetListeners() const { return GetGameObjectPtr()->GetListeners(); }

	//version 1 for SDK - used by plugin API
	// Return, as an array in out_aListenerIDs, the set of listeners that are associated with the game object of the PBI.
	bool GetListeners(AkGameObjectID* out_aListenerIDs, AkUInt32& io_uSize) const
	{
		const AkListenerSet& listeners = GetListeners();
		if (out_aListenerIDs == NULL)
		{
			io_uSize = listeners.Length();
			return true;
		}
		else
		{
			io_uSize = AkMin(listeners.Length(), io_uSize);
			for (AkUInt32 i = 0; i < io_uSize; i++)
				out_aListenerIDs[i] = listeners[i];
			return io_uSize == listeners.Length();
		}
	}


	AKRESULT GetListenerData(
		AkGameObjectID in_uListener,
		AkListener & out_listener
		) const;

	// Notify the PBI that a 3d param changed
	void PositioningChangeNotification(
		AkReal32			in_RTPCValue,	// New muting level
		AkRTPC_ParameterID	in_ParameterID	// RTPC ParameterID, must be a Positioning ID.
		);

	// 3D sound parameters.
	CAk3DAutomationParams *	Get3DAutomation() const { return m_p3DAutomation; }
	AkReal32			EvaluateSpread(AkReal32 in_fDistance);
	AkReal32			EvaluateFocus(AkReal32 in_fDistance);

	const BaseGenParams& GetBasePosParams(){ return m_BasePosParams; }
	const BaseGenParams& GetPrevPosParams(){ return m_Prev2DParams; }
	inline void			UpdatePrevPosParams() { m_Prev2DParams = m_BasePosParams; }
	inline void			InvalidatePrevPosParams() { m_Prev2DParams.Invalidate(); }
	inline bool HasListenerRelativeRouting() const
	{
		return m_BasePosParams.bHasListenerRelativeRouting;
	}

	AKRESULT			SubscribeAttenuationRTPC(CAkAttenuation* in_pAttenuation);
	void				UnsubscribeAttenuationRTPC(CAkAttenuation* in_pAttenuation);

	bool IsAuxRoutable();
	bool HasUserDefineAux();
	
	AkForceInline bool IsGameDefinedAuxEnabled(){ return m_EffectiveParams.bGameDefinedAuxEnabled; }
	bool HasEarlyReflectionsAuxSend();

	AkUniqueID			GetSoundID() const;
	CAkBus*				GetOutputBusPtr();

	AkForceInline const AkSoundParams & GetEffectiveParams() { AKASSERT(m_bAreParametersValid); return m_EffectiveParams; }

	AkForceInline void CalcEffectiveParams(AkInitialSoundParams* in_pInitialSoundParams = NULL)
	{
		if (!m_bAreParametersValid)
			RefreshParameters(in_pInitialSoundParams);
		else if (m_bFadeRatioDirty)
			CalculateMutedEffectiveVolume();
	}

	// Compute collapsed volume for this node.
	AkForceInline AkReal32 ComputeBehavioralNodeVolume()
	{
		const AkSoundParams & params = GetEffectiveParams();
		return AkMath::dBToLin(params.Volume()) * params.fFadeRatio;	//TODO: Optimize this.  Add a dirty bit on volume and compute only when necessary.  Same for fFadeRatio.
	}

#ifdef AK_DELTAMONITOR_ENABLED
	AkForceInline const AkSoundParams & GetEffectiveParamsForMonitoring()
	{
		CalcEffectiveParams();
		return m_EffectiveParams;		
	}
	
	AkForceInline void OpenSoundBrace() const { AkDeltaMonitor::OpenSoundBrace(GetSoundID(), GetPipelineID()); }
	AkForceInline void LogPipelineID() const { AkDeltaMonitor::LogPipelineID(m_PipelineID); };
	AkForceInline void CloseSoundBrace() { AkDeltaMonitor::CloseSoundBrace(); }
#else
	AkForceInline void LogPipelineID() const {}
	AkForceInline void OpenSoundBrace() const {}
	AkForceInline void CloseSoundBrace() const {}
#endif

	// Compute emitter-listener pairs. 
	// Returns the number of rays.
	AkUInt32			ComputeEmitterListenerPairs();
	bool				IsMultiPositionTypeMultiSources();
	bool				UseSpatialAudioPanningMode();

	// For direct access for mixer plugins.
	AkForceInline AkSpeakerPanningType GetSpeakerPanningType() const { return (AkSpeakerPanningType)m_BasePosParams.ePannerType; }
	AkForceInline Ak3DPositionType	Get3DPositionType() const { return (Ak3DPositionType)m_posParams.settings.m_e3DPositionType; }
	AkForceInline Ak3DSpatializationMode GetSpatializationMode() const { return (Ak3DSpatializationMode)m_posParams.settings.m_eSpatializationMode; }
	// This is needed for 3d spatialization. REVIEW Tnis setting is in fact only really relevant with this, not with panning.
	AkForceInline AkReal32 GetDivergenceCenter() { return m_BasePosParams.m_fCenterPCT * 0.01f; }
	AkForceInline CAkAttenuation * GetActiveAttenuation() {
		return (m_posParams.settings.m_bEnableAttenuation) ? m_posParams.GetRawAttenuation() : NULL;
	}
	AkForceInline AkReal32 GetOutsideConeVolume() const {
		return m_posParams.m_fConeOutsideVolume;
	}
	AkForceInline AkReal32 GetConeLPF() const {
		return m_posParams.m_fConeLoPass;
	}
	AkForceInline AkReal32 GetConeHPF() const {
		return m_posParams.m_fConeHiPass;
	}
	// 0..1
	AkForceInline AkReal32 GetPositioningTypeBlend() const {
		return m_posParams.positioningTypeBlend * 0.01f;
	}

	AkForceInline bool EnableDiffration() const { return !m_posParams.settings.HasAutomation() && m_posParams.settings.m_bEnableDiffraction;	}

	// Emitter-listener pairs (game object position(s) vs listener in spherical coordinates).
	AkForceInline const AkVolumeDataArray & GetRays() const {
		return m_arVolumeData;
	}
	/// TODO Review relevance of functions below in the light of this one above.
	//
	// Get number of pairs of the associated game object for a subset listeners specified by given mask.
	// Note: Returns the real number of pairs, as if the sound was full-featured 3D.
	inline AkUInt32 GetNumEmitterListenerPairs() { return GetGameObjectPtr()->GetNumEmitterListenerPairs(); }

	// Get the ith emitter-listener pair. The returned pair is fully computed (distance and angles).
	AKRESULT GetEmitterListenerPair( 
		AkUInt32 in_uIndex,
		AkGameObjectID in_uForListener,
		AkEmitterListenerPair & out_emitterListenerPair
		) const;
	AKRESULT GetEmitterListenerPair(AkUInt32 in_uIndex, AkEmitterListenerPair & out_emitterListenerPair) const;

	// Access volume / emitter-listener rays.
	AkUInt32 GetNumRays(const AkListenerSet& in_uAllowedListeners, AkSetType in_setType);
	const AkRayVolumeData * GetRay(const AkListenerSet& in_uAllowedListeners, AkSetType in_setType, AkUInt32 in_uIndex);	// Returns NULL if no match.

	inline void FlushRays() { m_arVolumeData.RemoveAll(); }
	inline AKRESULT EnsureOneRayReserved() 
	{
		if (m_arVolumeData.Reserved() == 0)
		{
			if (AK_EXPECT_FALSE(m_arVolumeData.Reserve(1) != AK_Success))
				return AK_Fail;
		}
		return AK_Success;
	}
	inline void _ComputeVolumeRays()
	{
		// Get rays whenever context HasListenerRelativeRouting.
		// 3D: Compute 3D emitter-listener pairs and get volumes for each of them
		if (HasListenerRelativeRouting() && ComputeEmitterListenerPairs())
		{
			CAkListener::ComputeVolumeRays(this, m_arVolumeData);
		}
	}

	void GetAuxSendsValues(AkAuxSendArray & io_arAuxSend);
	AkForceInline AkReal32 GetDryLevelValue(AkGameObjectID in_listenerID){ return GetGameObjectPtr()->GetConnectedListeners().GetUserGain(in_listenerID); }
	AkForceInline AkReal32 GetOutputBusOutputLPF(){ return m_EffectiveParams.OutputBusLPF(); }
	AkForceInline AkReal32 GetOutputBusOutputHPF(){ return m_EffectiveParams.OutputBusHPF(); }
	AkForceInline AkReal32 GetOutputBusVolumeValue()
	{
		AkReal32 newVolume = m_EffectiveParams.OutputBusVolume();
		if (newVolume == m_fLastOutputBusVolume) {
			return m_fCachedOutputBusVolumeLin;
		}
		m_fLastOutputBusVolume = newVolume;
		m_fCachedOutputBusVolumeLin = AkMath::dBToLin(newVolume);
		return m_fCachedOutputBusVolumeLin;
	}
	AkForceInline AkPathInfo* GetPathInfo() { return &m_PathInfo; }
	CAkBus* GetControlBus();
	
	//CAkRTPCTarget inteface
	virtual void UpdateTargetParam(AkRTPC_ParameterID in_eParam, AkReal32 in_fValue, AkReal32 in_fDelta);
	virtual void NotifyParamChanged(bool in_bLiveEdit, AkRTPC_ParameterID in_rtpcID) { m_bAreParametersValid = false; }
	virtual void NotifyParamsChanged(bool in_bLiveEdit, const AkRTPCBitArray& in_bitArray) { m_bAreParametersValid = false; }
	virtual AkRTPCBitArray GetTargetedParamsSet()	{ return AkRTPCBitArray(RTPC_PBI_PARAMS_BITFIELD); }
	virtual bool RegisterToBusHierarchy()
	{
		return true;
	}

	// Notify the ctx that the PBI must recalculate the parameters
	void RecalcNotification(bool in_bLiveEdit, bool in_bLog = false);
	void ParamNotification(NotifParams& in_rParams);

	// Overridable methods.
	virtual void RefreshParameters(AkInitialSoundParams* in_pInitialSoundParams);
	virtual void CalculateMutedEffectiveVolume();

	inline CAkModulatorData& GetModulatorData() { return m_ModulatorData; }

	AKRESULT SetupPath(AkPathInfo* in_pPathInfo, bool in_bStart);
	AKRESULT Init3DPath(AkPathInfo* in_pPathInfo);
	AKRESULT Recalc3DPath();
	virtual void OnPathAdded(CAkPath* pPath);
	void	 StartPath();

#ifndef AK_OPTIMIZED

	void UpdateAttenuationInfo();
	void InvalidatePositioning();
	void InvalidatePaths();

#endif //AK_OPTIMIZED
	
	bool IsInitiallyUnderThreshold(AkInitialSoundParams* in_pInitialSoundParams);
	inline bool IsHDR() { return m_bIsHDR; }
	inline bool PlayDirectly() { return m_bPlayDirectly; }
	
	// Create attached props cloned parameter object if necessary and returns it.
	void GetAttachedPropsFxDesc(AkFXDesc& out_fxDesc);

	CAkParameterNodeBase * GetParameterNode() const { return m_pParamNode; }
	virtual void UpdatePriorityWithDistance(CAkAttenuation*, AkReal32) {}

#ifndef AK_OPTIMIZED
	void UpdateRTPC();
	void ResetRTPC();
#endif

	void Get3DParams();
	void Reset3DParams();

	// Voice-specific data that needs to be accessed quickly from the lower engine.
	inline bool IsForcedVirtualized() { return m_bIsForcedToVirtualizeForLimiting || m_bIsVirtualizedForInterruption; }
	
	inline void ForceParametersDirty() { m_bAreParametersValid = false; }

	AkForceInline AkPipelineID GetPipelineID() const { return m_PipelineID; }

	// Must be called if spatial audio params (other than send volumes) on the parameter node have changed.
	void SpatialAudioParamsUpdated();
protected:

	AKRESULT CacheEmitterPosition();

	AkReal32 Scale3DUserDefRTPCValue(AkReal32 in_fValue);
	
	void RefreshAutomationAndAttenation();

	void PausePath( bool in_bPause );

	// Helper function to reset 3DParams when changing Attenuation
	AKRESULT UpdateAttenuation(CAkAttenuation* in_pattenuation);

	void InitSpatialAudioVoice();
#ifndef AK_OPTIMIZED
	virtual void ReportSpatialAudioErrorCodes() = 0;
#endif

	CAkModulatorData	m_ModulatorData;

	AkSoundParams		m_EffectiveParams;			// Includes mutes and fades	

	AkPathInfo			m_PathInfo;			// our path if any

	BaseGenParams		m_BasePosParams;
	BaseGenParams		m_Prev2DParams;
	AkPositioningParams m_posParams;

	// Array of emitter-listener rays, containing listener-dependent linear volumes and optionally, angles.
	AkVolumeDataArray	m_arVolumeData;

	// Back up of the first position when the position of the sound is not dynamic
	AkArray<AkChannelEmitter, const AkChannelEmitter&> m_cachedGameObjectPosition;

	CAk3DAutomationParams *	m_p3DAutomation;					// 3D parameters.
	CAkParameterNodeBase *	m_pParamNode;

	CAkSpatialAudioVoice * m_pSpatialAudioVoice;

	AkReal32					m_fMaxDistance; // used for the query of the same name.
	AkPipelineID				m_PipelineID;	// ID to uniquely identify the instance for profiling.

	AkUInt8						m_bAreParametersValid : 1;
	AkUInt8						m_bGetAudioParamsCalled : 1;
	AkUInt8						m_bRefreshAllAfterRtpcChange : 1;	// refresh more parameters after RTPC (curve / data points or binding) change.
	AkUInt8						m_bGameObjPositionCached : 1;	// used when not dynamic.
	AkUInt8						m_bFadeRatioDirty : 1;
	AkUInt8						m_bIsAutomationOrAttenuationDirty : 1;
	AkUInt8						m_bPlayDirectly : 1;
	AkUInt8						m_bIsHDR : 1;

	// These pertain to voices only, but reading them is critical for performance of behavioral virtual voices.
	AkUInt8				m_bIsForcedToVirtualizeForLimiting : 1;
	AkUInt8				m_bIsVirtualizedForInterruption : 1;

private:
	AkReal32			m_fCachedOutputBusVolumeLin; // used to avoid recomputing dBToLin when not necessary
	AkReal32			m_fLastOutputBusVolume;    // used to avoid recomputing dBToLin when not necessary
};

#endif