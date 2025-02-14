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
// AkModulatorPBIData.h
//
//////////////////////////////////////////////////////////////////////

#ifndef __AK_MODULATOR_PBIDATA__
#define __AK_MODULATOR_PBIDATA__

#include "AkCommon.h"
#include "AkList2.h"
#include "AkModulatorProps.h"

class CAkModulatorCtx;
struct AkModulatorTriggerParams;
struct AkModulatorSubscriberInfo;

typedef AkArray< CAkModulatorCtx*, CAkModulatorCtx* > ModulatorCtxArray;
class CAkModCtxRefContainer
{
public:
	CAkModCtxRefContainer(){}
	CAkModCtxRefContainer(const CAkModCtxRefContainer& in_from);
	~CAkModCtxRefContainer();
	CAkModCtxRefContainer& operator = (const CAkModCtxRefContainer& in_from);
	void AddModulatorCtx(CAkModulatorCtx* in_pCtx);
	bool HasModulatorCtx(CAkModulatorCtx* in_pCtx);
	void Release();
	bool IsEmpty(){ return m_Ctx.IsEmpty(); }

private:
	AKRESULT DoCopy(const CAkModCtxRefContainer& in_from);

	ModulatorCtxArray m_Ctx;
};

////
// CAkModulatorPBIData
//
// Contains modulator-related data that needs to be accessed through the PBI, by the lower engine.
////
class CAkModulatorPBIData
{
protected:
	struct AkModCtxParams
	{
		CAkModulatorCtx* pCtx;

		//Maps a AkRTPC_ParameterID to a linear transform (scale and offset)
		//	These are the parameters that need to be updated at audio sample rate.
		AkModulatorParamXfrmArray paramXfrms;
	};

public:

	CAkModulatorPBIData();
	~CAkModulatorPBIData();

	AKRESULT Init();

	void Term();

	// Add a modulator context to the associated PBI.  This function will query the hierarchy for the appropriate transforms to apply to the modulator.
	AKRESULT AddModulationSource(CAkModulatorCtx* in_pCtx, const AkModulatorTriggerParams& in_triggerParams, const AkModulatorSubscriberInfo& in_subscrInfo );
	
	// Is in_pCtx associated with the PBI that owns this CAkModulatorPBIData?
	bool HasModulationSource(CAkModulatorCtx* in_pCtx) const;

	// Trigger the release phase for all the modulator ctx's associates with the PBI.
	void TriggerRelease( AkUInt32 in_uFrameOffset );

	// Pause resume controls - for all modulators referenced.
	void Pause();
	void Resume();

	//Get the number of automated modulators effecting the associated PBI.  The modulators are split into two counts:
	//	(1) modulators which require a buffer (high frequency)
	//	(2) modulators which only apply an interpolated gain (low frequency)
	void GetNumAutomatedModulators( const AkRTPCBitArray& in_paramIDs, AkUInt32& out_numBufs, AkUInt32& out_numGains ) const;

	// Clear all param xfrms; this is called prior to an update of the list due to live editing
	void ClearModulationSources();

	//Get all the audio-rate automation buffers and their transforms for all modulators effecting the associated PBI.
	void GetBufferList(
				const AkRTPCBitArray& in_paramIDs, 
				AkModulatorXfrm out_pXfrmArray[], 
				AkModulatorGain out_pGainArray[],
				AkReal32* out_pAutmBufArray[], 
				AkUInt32& io_uXfrmArrSize,
				AkUInt32& io_uGainArrSize ) const;

	//Get the accumulated, transformed control-rate output value for all the modulators effecting the associated PBI.
	AkReal32 GetCtrlRateOutput( const AkRTPCBitArray& in_paramIDs );

	//Get the accumulated, transformed peak value for the previous frame, for all the modulators effecting the associated PBI.
	AkReal32 GetPeakOutput( const AkRTPCBitArray& in_paramIDs );

	void RemovePbiRefs();
	void GetModulatorCtxs(AkModulatorScope in_eDesiredScope, CAkModCtxRefContainer& out_Modulators);

	AkUInt32 AddRef();
	AkUInt32 Release();

#ifndef AK_OPTIMIZED
	// Keep the list of nodes that triggered this modulator
	// so we can associate them to this modulator when profiling
	typedef	AkArray< AkUniqueID, AkUniqueID, ArrayPoolDefault > SubscribersIDList;
	SubscribersIDList m_arSubscribersID;
#endif

protected:
	typedef CAkList2<AkModCtxParams, const AkModCtxParams&, AkAllocAndKeep > AkModCtxParamsList;
	AkModCtxParamsList m_ctxParamsList;
private:
	AkUInt32 m_uRef;
};


//
//	CAkModulatorData
//		A handle/container class for CAkModulatorPBIData that implements wrapper functions.
//
class CAkModulatorData
{
public:
	CAkModulatorData();

	~CAkModulatorData();

	void ReleaseData();

	AKRESULT AddModulationSource(CAkModulatorCtx* in_pCtx, const AkModulatorTriggerParams& in_triggerParams, const AkModulatorSubscriberInfo& in_subscrInfo)
	{
		AKRESULT res = AK_Fail;

		if (m_pData == NULL) 
			AllocData();

		if (m_pData != NULL)
			res = m_pData->AddModulationSource(in_pCtx, in_triggerParams, in_subscrInfo);

		return res;
	}

	bool HasModulationSource(CAkModulatorCtx* in_pCtx) const
	{
		return (m_pData != NULL) ? m_pData->HasModulationSource(in_pCtx) : false;
	}

	void TriggerRelease( AkUInt32 in_uFrameOffset )
	{
		if (m_pData != NULL)
			m_pData->TriggerRelease(in_uFrameOffset);
		else
			m_uReleaseOffset = in_uFrameOffset;
	}

	void Pause()
	{
		if (m_pData != NULL)
			m_pData->Pause();
	}

	void Resume()
	{
		if (m_pData != NULL)
			m_pData->Resume();
	}

	void GetNumAutomatedModulators( const AkRTPCBitArray& in_paramIDs, AkUInt32& out_numBufs, AkUInt32& out_numGains ) const
	{
		out_numBufs = out_numGains = 0;
		if (m_pData != NULL)
			m_pData->GetNumAutomatedModulators(in_paramIDs, out_numBufs, out_numGains);
	}

	void ClearModulationSources()	
	{
		if (m_pData != NULL)
			m_pData->ClearModulationSources();
	}

	void GetBufferList(
		const AkRTPCBitArray& in_paramIDs, 
		AkModulatorXfrm out_pXfrmArray[], 
		AkModulatorGain out_pGainArray[],
		AkReal32* out_pAutmBufArray[], 
		AkUInt32& io_uXfrmArrSize,
		AkUInt32& io_uGainArrSize ) const
	{
		if (m_pData != NULL)
		{
			m_pData->GetBufferList(	in_paramIDs, 
										out_pXfrmArray, 
										out_pGainArray,
										out_pAutmBufArray, 
										io_uXfrmArrSize,
										io_uGainArrSize );
		}
	}

	AkReal32 GetCtrlRateOutput( const AkRTPCBitArray& in_paramIDs )
	{
		return (m_pData != NULL) ? m_pData->GetCtrlRateOutput(in_paramIDs) : 1.0f;
	}

	AkReal32 GetPeakOutput( const AkRTPCBitArray& in_paramIDs )
	{
		return (m_pData != NULL) ? m_pData->GetPeakOutput(in_paramIDs) : 1.0f;
	}
	
	bool IsEmpty() { return m_pData == NULL; }

	#define INVALID_RELEASE_OFFSET (~0)

	void ReleaseOnTrigger()
	{
		if (m_uReleaseOffset != INVALID_RELEASE_OFFSET)
			TriggerRelease( m_uReleaseOffset );
	}

	void InheritModulatorData(const CAkModulatorData& in_from )
	{
		AKASSERT(m_pData == NULL);
		if (in_from.m_pData != NULL)
		{
			m_pData = in_from.m_pData;
			m_pData->AddRef();
		}
	}

	void GetModulatorCtxs(AkModulatorScope in_eDesiredScope, CAkModCtxRefContainer& out_ModulatorCtxs)
	{
		if (m_pData != NULL)
			m_pData->GetModulatorCtxs(in_eDesiredScope, out_ModulatorCtxs);
	}
private:
	AKRESULT AllocData();

	CAkModulatorPBIData*	m_pData;

	// Frame offset used when when releasing on trigger (same frame) with modulator on an effect (e.g. SynthOne).
	// DO NOT USE in any other case!!!  This is to be used only in function ReleaseOnTrigger() !!!
	AkUInt32				m_uReleaseOffset;
};

#endif
