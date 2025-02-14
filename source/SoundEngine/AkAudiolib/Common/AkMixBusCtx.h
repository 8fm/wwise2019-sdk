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
// AkMixBusCtx.h
//
//////////////////////////////////////////////////////////////////////

#ifndef AK_MIX_BUS_CTX
#define AK_MIX_BUS_CTX

#include "AkBehavioralCtx.h"

class CAkVPLMixBusNode;

class CAkMixBusCtx : public CAkBehavioralCtx
{
public:
	typedef AkListBareLight<CAkBehavioralCtx> AkListLightCtxs;

	CAkMixBusCtx(CAkRegisteredObj* in_pGameObj, CAkBus* in_pBus);
	~CAkMixBusCtx();

	void SetMixBusNode(CAkVPLMixBusNode* in_pMixMus){ m_pMixBus = in_pMixMus; }

	virtual AKRESULT Init();
	virtual void Term(bool in_bFailedToInit);

	AkForceInline CAkBus* GetBus(){ return (CAkBus*)m_pParamNode; }

	virtual void UpdateTargetParam(AkRTPC_ParameterID in_eParam, AkReal32 in_fValue, AkReal32 in_fDelta);
	virtual AkRTPCBitArray GetTargetedParamsSet();

	virtual void RefreshParameters(AkInitialSoundParams* in_pInitialSoundParams);

	AkForceInline AkUniqueID ID() const { return GetSoundID(); }

	static void ManageAuxRoutableBusses();

#ifndef AK_OPTIMIZED
	void ClearMonitoringMuteSolo(bool in_bIsSoloActive)
	{
		m_pMixBus->ClearMonitoringMuteSolo(in_bIsSoloActive);	
		m_bMonitoringMute = false;
	}
	void RefreshMonitoringMuteSolo();
	bool IsMonitoringMute() const { return m_bMonitoringMute; }		
#endif
protected:
	virtual void NotifyParamChanged(bool in_bLiveEdit, AkRTPC_ParameterID in_rtpcID);
	virtual void NotifyParamsChanged(bool in_bLiveEdit, const AkRTPCBitArray& in_bitArray);

#ifndef AK_OPTIMIZED
	virtual void ReportSpatialAudioErrorCodes();
#endif

	CAkVPLMixBusNode* m_pMixBus;

	static AkListLightCtxs s_AuxRoutableBusses;
	CAkBehavioralCtx* pPrevAuxRoutable;

	void RemoveFromAuxList();
	void AddToAuxList();

#ifndef AK_OPTIMIZED
	// This monitoring mute flag is used for the master bus (which has no connections) and the meters.
	AkUInt8 m_bMonitoringMute : 1;
#endif

private:
	void RefreshBypassFx();
};

#endif