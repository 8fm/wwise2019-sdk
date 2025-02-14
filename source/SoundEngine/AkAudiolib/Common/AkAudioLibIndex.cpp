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
// AkAudioLibIndex.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkAudioLibIndex.h"
#include "AkCritical.h"
#include "AkRanSeqCntr.h"
#include "AkPrivateTypes.h"
#include "AkFxBase.h"
#include "AkAttenuationMgr.h"
#include "AkDynamicSequence.h"
#include "AkPlayingMgr.h"
#include "AkRegistryMgr.h"
#include "AkState.h"

extern CAkPlayingMgr* g_pPlayingMgr;

bool CAkAudioLibIndex::Init()
{
	return m_idxAudioNode.Init() &&
		m_idxBusses.Init() &&
		m_idxCustomStates.Init() &&
		m_idxEvents.Init() &&
		m_idxActions.Init() &&
		m_idxLayers.Init() &&
		m_idxAttenuations.Init() &&
		m_idxModulators.Init() &&
		m_idxDynamicSequences.Init() &&
		m_idxDialogueEvents.Init() &&
		m_idxFxShareSets.Init() &&
		m_idxFxCustom.Init() &&
		m_idxAudioDevices.Init() &&
		m_idxVirtualAcoustics.Init();
}

void CAkAudioLibIndex::Term()
{
	m_idxAudioNode.Term();
	m_idxBusses.Term();
	m_idxCustomStates.Term();
	m_idxEvents.Term();
	m_idxActions.Term();
	m_idxLayers.Term();
	m_idxAttenuations.Term();
	m_idxModulators.Term();
	m_idxDynamicSequences.Term();
	m_idxDialogueEvents.Term();
	m_idxFxShareSets.Term();
	m_idxFxCustom.Term();
	m_idxAudioDevices.Term();

	// FIXME Attach these properly to usage slot
	while (m_idxVirtualAcoustics.m_mapIDToPtr.Length())
	{
		(*m_idxVirtualAcoustics.m_mapIDToPtr.Begin())->Release();
	}
	m_idxVirtualAcoustics.Term();
}

void CAkAudioLibIndex::ReleaseTempObjects()
{
	{
		CAkIndexItem<CAkParameterNodeBase*>& l_rIdx = m_idxAudioNode;

		AkAutoLock<CAkLock> IndexLock( l_rIdx.m_IndexLock );

		CAkIndexItem<CAkParameterNodeBase*>::AkMapIDToPtr::Iterator iter = l_rIdx.m_mapIDToPtr.Begin();
		while( iter != l_rIdx.m_mapIDToPtr.End() )
		{
			CAkParameterNodeBase* pNode = static_cast<CAkParameterNodeBase*>( *iter );
			++iter;
			if( pNode->ID() & AK_SOUNDENGINE_RESERVED_BIT )
			{
				pNode->Release();
			}
		}
	}

	{
		CAkIndexItem<CAkFxCustom*>& l_rIdx = m_idxFxCustom;

		CAkIndexItem<CAkFxCustom*>::AkMapIDToPtr::Iterator iter = l_rIdx.m_mapIDToPtr.Begin();
		while( iter != l_rIdx.m_mapIDToPtr.End() )
		{
			CAkFxCustom* pCustom = static_cast<CAkFxCustom*>( *iter );
			++iter;
			if( pCustom->ID() & AK_SOUNDENGINE_RESERVED_BIT )
			{
				pCustom->Release();
			}
		}
	}
}
void CAkAudioLibIndex::ReleaseDynamicSequences()
{
	CAkIndexItem<CAkDynamicSequence*>& l_rIdx = m_idxDynamicSequences;

	AkAutoLock<CAkLock> IndexLock( l_rIdx.m_IndexLock );

	CAkIndexItem<CAkDynamicSequence*>::AkMapIDToPtr::Iterator iter = l_rIdx.m_mapIDToPtr.Begin();
	while( iter != l_rIdx.m_mapIDToPtr.End() )
	{
		CAkDynamicSequence* pNode = static_cast<CAkDynamicSequence*>( *iter );
		++iter;
		g_pPlayingMgr->RemoveItemActiveCount( pNode->GetPlayingID() );
		pNode->Release();	
	}
}

CAkParameterNodeBase* CAkAudioLibIndex::GetNodePtrAndAddRef( AkUniqueID in_ID, AkNodeType in_NodeType )
{
	if( in_NodeType == AkNodeType_Default )
	{
		return m_idxAudioNode.GetPtrAndAddRef( in_ID );
	}
	else
	{
		AKASSERT( in_NodeType == AkNodeType_Bus );
		return m_idxBusses.GetPtrAndAddRef( in_ID );
	}
}

CAkParameterNodeBase* CAkAudioLibIndex::GetNodePtrAndAddRef( WwiseObjectIDext& in_rIDext )
{
	return GetNodePtrAndAddRef( in_rIDext.id, in_rIDext.GetType() );
}

CAkLock& CAkAudioLibIndex::GetNodeLock( AkNodeType in_NodeType )
{
	if( in_NodeType == AkNodeType_Default )
	{
		return m_idxAudioNode.GetLock();
	}
	else
	{
		AKASSERT( in_NodeType == AkNodeType_Bus );
		return m_idxBusses.GetLock();
	}
}

CAkIndexItem<CAkParameterNodeBase*>& CAkAudioLibIndex::GetNodeIndex( AkNodeType in_NodeType )
{
	if( in_NodeType == AkNodeType_Default )
	{
		return m_idxAudioNode;
	}
	else
	{
		AKASSERT( in_NodeType == AkNodeType_Bus );
		return m_idxBusses;
	}
}


#ifndef AK_OPTIMIZED
AKRESULT CAkAudioLibIndex::ResetRndSeqCntrPlaylists()
{
	for( CAkIndexItem<CAkParameterNodeBase*>::AkMapIDToPtr::Iterator iter = m_idxAudioNode.m_mapIDToPtr.Begin(); iter != m_idxAudioNode.m_mapIDToPtr.End(); ++iter )
	{
		CAkParameterNodeBase* pNode = static_cast<CAkParameterNodeBase*>( *iter );
		if( pNode->NodeCategory() == AkNodeCategory_RanSeqCntr )
		{
			static_cast<CAkRanSeqCntr*>( pNode )->ResetSpecificInfo();
		}
	}
	return AK_Success;
}
void CAkAudioLibIndex::ClearMonitoringSoloMute()
{
	if (CAkParameterNodeBase::IsVoiceMonitoringMuteSoloActive())
	{
		for (CAkIndexItem<CAkParameterNodeBase*>::AkMapIDToPtr::Iterator iter = m_idxAudioNode.m_mapIDToPtr.Begin(); iter != m_idxAudioNode.m_mapIDToPtr.End(); ++iter)
		{
			CAkParameterNodeBase* pNode = static_cast<CAkParameterNodeBase*>(*iter);
			pNode->MonitoringSolo(false);
			pNode->MonitoringMute(false);
		}
		CAkParameterNodeBase::SetMonitoringMuteSoloDirty();
	}
	if (CAkParameterNodeBase::IsBusMonitoringMuteSoloActive())
	{
		for (CAkIndexItem<CAkParameterNodeBase*>::AkMapIDToPtr::Iterator iter = m_idxBusses.m_mapIDToPtr.Begin(); iter != m_idxBusses.m_mapIDToPtr.End(); ++iter)
		{
			CAkParameterNodeBase* pNode = static_cast<CAkParameterNodeBase*>(*iter);
			pNode->MonitoringSolo(false);
			pNode->MonitoringMute(false);
		}
		CAkParameterNodeBase::SetMonitoringMuteSoloDirty();
	}
	if (CAkRegistryMgr::IsMonitoringMuteSoloActive())
	{
		// This will call CAkParameterNodeBase::SetMonitoringMuteSoloDirty()
		g_pRegistryMgr->ClearMonitoringSoloMuteGameObj();
	}
}
#endif
