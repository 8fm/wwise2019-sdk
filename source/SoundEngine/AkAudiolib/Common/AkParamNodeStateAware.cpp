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
// AkParameterNodeBase.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "AkParameterNodeBase.h"
#include "AkParamNodeStateAware.h"
#include "AkURenderer.h"

CAkParamNodeStateAware::CAkParamNodeStateAware()
	: m_pData( NULL )
{}

CAkParamNodeStateAware::~CAkParamNodeStateAware()
{
	if ( m_pData )
	{
		if( !m_pData->m_stateChunks.IsEmpty() )
		{
			FlushStateTransitions();
			RemoveStateGroups( false );
		}

		AkDelete(AkMemID_Structure, m_pData );
		m_pData = NULL;
	}
}

bool CAkParamNodeStateAware::EnsureStateData()
{
	if ( ! m_pData )
		m_pData = AkNew(AkMemID_Structure, ParamNodeStateAwareData() );

	return m_pData != NULL;
}

void CAkParamNodeStateAware::SetStateProperties( AkUInt32 in_uProperties, AkStatePropertyUpdate* in_pProperties, bool in_bNotify )
{
	CAkStateAware::SetStateProperties( in_uProperties, in_pProperties, false );
	AKASSERT( m_pData );
	if ( ! m_pData )
		return;

	CAkParamNodeStateTargetData& targetData = m_pData->m_stateTargets;

	targetData.ClearBits();

	for ( AkUInt32 i = 0; i < in_uProperties; ++i )
	{
		targetData.m_stateBitArray.SetBit( in_pProperties[i].propertyId );
	}

	if (in_bNotify)
		NotifyStateParametersModified();
}

bool CAkParamNodeStateAware::IsStateProperty( AkRtpcID in_uProperty )
{
	return m_pData && m_pData->m_stateTargets.m_stateBitArray.IsSet( in_uProperty );
}

AKRESULT CAkParamNodeStateAware::ReadStateChunk( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	AKRESULT result = CAkStateAware::ReadStateChunk( io_rpData, io_rulDataSize );
	if ( result == AK_Success && m_pData )
	{
		CAkParamNodeStateTargetData& targetData = m_pData->m_stateTargets;
		targetData.ClearBits();

		StatePropertyArray& stateProps = m_pData->m_stateProperties;

		for ( AkUInt32 propIdx = 0; propIdx < stateProps.Length(); ++propIdx )
		{
			targetData.m_stateBitArray.SetBit( stateProps[propIdx].propertyId );
		}
	}
	return result;
}

bool CAkParamNodeStateAware::StateTransitionAdded()
{
	bool res = static_cast<CAkParameterNodeBase*>(this)->IncrementActivityCount();
	static_cast<CAkParameterNodeBase*>(this)->AddToURendererActiveNodes();
	return res;
}

void CAkParamNodeStateAware::StateTransitionRemoved()
{
	static_cast<CAkParameterNodeBase*>(this)->DecrementActivityCount();
	static_cast<CAkParameterNodeBase*>(this)->RemoveFromURendererActiveNodes();
}

void CAkParamNodeStateAware::PushStateParamUpdate( AkStatePropertyId in_propertyId, AkRtpcAccum in_rtpcAccum, AkStateGroupID in_stateGroup, AkReal32 in_oldValue, AkReal32 in_newValue, bool in_transitionDone )
{
	AKASSERT( m_pData );

	CAkParamNodeStateTargetData& targetData = m_pData->m_stateTargets;	
	for ( AkStateParamTargetArray::Iterator it = targetData.m_targetsArray.Begin();
		it != targetData.m_targetsArray.End(); ++it )
	{
		StateRegisteredTarget& regTgt = *it;
		if (regTgt.params.IsSet(in_propertyId))
		{
			AkDeltaMonitorObjBrace deltaBrace(static_cast<CAkParameterNodeBase*>(this)->ID());
			regTgt.pTgt->UpdateTargetParam((AkRTPC_ParameterID)in_propertyId, in_newValue, in_newValue - in_oldValue);			
		}
	}

	if ( in_transitionDone )
		NotifyStateParamTargets();
}

void CAkParamNodeStateAware::UpdateStateParamTargets()
{
	CAkParameterTarget::UpdateAllParameterTargets();

	NotifyStateParamTargets();
}

void CAkParamNodeStateAware::NotifyStateParamTargets()
{
	if ( m_pData )
	{
		CAkParamNodeStateTargetData& targetData = m_pData->m_stateTargets;

		for ( AkStateParamTargetArray::Iterator it = targetData.m_targetsArray.Begin();
			it != targetData.m_targetsArray.End(); ++it )
		{
			StateRegisteredTarget& regTgt = *it;
			regTgt.pTgt->NotifyParamsChanged( false, targetData.m_stateBitArray );
		}
	}
}
