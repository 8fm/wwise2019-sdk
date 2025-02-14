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
#ifndef _PARAM_NODE_STATE_AWARE_H_
#define _PARAM_NODE_STATE_AWARE_H_

#include "AkStateAware.h"
#include "AkRTPCSubscriber.h"


struct StateRegisteredTarget
{
	StateRegisteredTarget() : params(), pTgt(NULL) {}
	StateRegisteredTarget( AkRTPCBitArray in_params, CAkParameterTarget* in_pTgt ) : params( in_params ), pTgt( in_pTgt ) {}

	AkRTPCBitArray params;
	CAkParameterTarget* pTgt;
};


class AkStateParamTargetArray : public AkArray < StateRegisteredTarget, StateRegisteredTarget, ArrayPoolDefault >
{
public:
	bool AddTarget( StateRegisteredTarget& in_regTgt )
	{
		for ( AkUInt32 i = 0; i < Length(); ++i )
		{
			if ( m_pItems[i].pTgt == in_regTgt.pTgt )
			{
				m_pItems[i].params = in_regTgt.params;
				return false;
			}
		}
		AddLast( in_regTgt );
		return true;
	}

	bool RemoveTarget( CAkParameterTarget* in_pTgt )
	{
		for ( AkUInt32 i = 0; i < Length(); ++i )
		{
			if ( m_pItems[i].pTgt == in_pTgt )
			{
				Erase( i );
				return true;
			}
		}
		return false;
	}
};

class CAkParamNodeStateTargetData
{
public:
	CAkParamNodeStateTargetData()
	{}

	~CAkParamNodeStateTargetData()
	{
		m_targetsArray.Term();
	}

	//The set of parameters that this node states for.
	AkRTPCBitArray	m_stateBitArray;

	//Array of parameter targets
	AkStateParamTargetArray m_targetsArray;

	void ClearBits()
	{
		m_stateBitArray.ClearBits();
	}

	void RemoveTarget(CAkParameterTarget* in_pTgt)
	{
		m_targetsArray.RemoveTarget(in_pTgt);
	}

	void AddTarget(CAkParameterTarget* in_pTgt, AkRTPCBitArray in_params)
	{
		StateRegisteredTarget regTgt(in_params, in_pTgt);
		m_targetsArray.AddTarget(regTgt);
	}
};

struct ParamNodeStateAwareData
{
	ParamNodeStateAwareData()
	{}
	~ParamNodeStateAwareData()
	{
		m_stateChunks.Term();
		m_stateProperties.Term();
	}

	StateGroupChunkList m_stateChunks;
	CAkParamNodeStateTargetData m_stateTargets;
	StatePropertyArray m_stateProperties;
};


class CAkParamNodeStateAware : public CAkStateAware
{
protected:
	CAkParamNodeStateAware();
	virtual ~CAkParamNodeStateAware();

public:

	virtual StatePropertyArray* GetStateProperties() { return m_pData ? &m_pData->m_stateProperties: NULL; }
	virtual void SetStateProperties( AkUInt32 in_uProperties, AkStatePropertyUpdate* in_pProperties, bool in_bNotify = true );
	virtual bool IsStateProperty( AkRtpcID in_uProperty );

protected:
	virtual AKRESULT ReadStateChunk( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize );
	virtual StateGroupChunkList* GetStateChunks() { return m_pData ? &m_pData->m_stateChunks : NULL; }

	virtual bool StateTransitionAdded();
	virtual void StateTransitionRemoved();

private:
	virtual bool EnsureStateData();

private:
	ParamNodeStateAwareData* m_pData;

public:
	AkForceInline void RegisterParameterTarget( CAkParameterTarget* in_pTgt, const AkRTPCBitArray& in_params )
	{
		if ( m_pData != NULL )
		{
			AkRTPCBitArray params = m_pData->m_stateTargets.m_stateBitArray & in_params;

			if ( ! params.IsEmpty() )
			{
				m_pData->m_stateTargets.AddTarget( in_pTgt, params );
			}
			else
			{
				m_pData->m_stateTargets.RemoveTarget( in_pTgt );
			}
		}
	}

	AkForceInline void UnregisterParameterTarget( CAkParameterTarget* in_pTgt )
	{
		if ( m_pData != NULL )
		{
			m_pData->m_stateTargets.RemoveTarget( in_pTgt );
		}
	}

	virtual void PushStateParamUpdate( AkStatePropertyId in_propertyId, AkRtpcAccum in_rtpcAccum, AkStateGroupID in_stateGroup, AkReal32 in_oldValue, AkReal32 in_newValue, bool in_transitionDone );
	virtual void UpdateStateParamTargets();
	virtual void NotifyStateParamTargets();
};

#endif
