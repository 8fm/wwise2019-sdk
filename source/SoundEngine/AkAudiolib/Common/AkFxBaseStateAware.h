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
// AkParameterNodeBase.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _FX_BASE_STATE_AWARE_H_
#define _FX_BASE_STATE_AWARE_H_

#include "AkStateAware.h"


struct FxBaseStateAwareData
{
	FxBaseStateAwareData()
	{}
	~FxBaseStateAwareData()
	{
		m_stateChunks.Term();
		m_stateProperties.Term();
	}

	StateGroupChunkList m_stateChunks;
	StatePropertyArray m_stateProperties;
};


class CAkFxBaseStateAware : public CAkStateAware
{
protected:
	CAkFxBaseStateAware();
	virtual ~CAkFxBaseStateAware();

public:

	virtual StatePropertyArray* GetStateProperties() { return m_pData ? &m_pData->m_stateProperties: NULL; }
	virtual void SetStateProperties( AkUInt32 in_uProperties, AkStatePropertyUpdate* in_pProperties, bool in_bNotify = true );

protected:
	virtual StateGroupChunkList* GetStateChunks() { return m_pData ? &m_pData->m_stateChunks : NULL; }

	virtual bool StateTransitionAdded() { return true; }
	virtual void StateTransitionRemoved() {}

private:
	virtual bool EnsureStateData();

private:
	FxBaseStateAwareData* m_pData;

public:
	virtual void PushStateParamUpdate( AkStatePropertyId in_propertyId, AkRtpcAccum in_rtpcAccum, AkStateGroupID in_stateGroup, AkReal32 in_oldValue, AkReal32 in_newValue, bool in_transitionDone );
	virtual void UpdateStateParamTargets();
	virtual void NotifyStateParamTargets();
};

#endif
