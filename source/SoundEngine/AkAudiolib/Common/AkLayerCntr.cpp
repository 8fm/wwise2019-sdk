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
// AkLayerCntr.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include "AkLayerCntr.h"
#include "AkLayer.h"
#include "AkPBI.h"
#include "AkPlayingMgr.h"
#include "AkURenderer.h"
#include "AkBankMgr.h"

#include <AK/Tools/Common/AkDynaBlkPool.h>
static AkDynaBlkPool<CAkLayerCntr::ContPlaybackItem, 128, ArrayPoolDefault> g_LayerCntrChunkPool;

CAkLayerCntr::CAkLayerCntr(AkUniqueID in_ulID)
	: CAkMultiPlayNode( in_ulID )
	, m_bIsContinuousValidation(false)
{
}

CAkLayerCntr::~CAkLayerCntr()
{
	// Cleanup: Release our Layers
	for ( LayerList::Iterator itLayer = m_layers.Begin(), itLayerEnd = m_layers.End();
		  itLayer != itLayerEnd;
		  ++itLayer )
	{
		(*itLayer)->SetOwner( NULL );
		(*itLayer)->Release();
	}

	m_layers.Term();

	m_listContPlayback.Term();

	__base::Term();
}

void CAkLayerCntr::OnPreRelease()
{
	// Ensure the clean-up of the switch container histories is done before we disconnect from
	// our parent and busses, so the activity count is propagated to them
	// See WG-19700
	CleanContPlayback(); // required to free the play histories

	__base::OnPreRelease();
}

void CAkLayerCntr::CleanContPlayback()
{
	while(!m_listContPlayback.IsEmpty())
	{
		ContPlaybackItem * pItem = m_listContPlayback.First();
		m_listContPlayback.RemoveFirst();

		NotifyEndContinuous(*pItem);
		g_LayerCntrChunkPool.Delete(pItem);
	}
}

void CAkLayerCntr::NotifyEndContinuous(ContPlaybackItem& in_rContPlayback)
{
	g_pPlayingMgr->RemoveItemActiveCount(in_rContPlayback.UserParameters.PlayingID());
	DecrementActivityCount();
	RemoveFromURendererActiveNodes();
	in_rContPlayback.rtpcKey.GameObj()->Release();
	in_rContPlayback.childMuteCount.Term();
	in_rContPlayback.modulatorData.ReleaseData();
}

AKRESULT CAkLayerCntr::SetInitialValues(AkUInt8* in_pData, AkUInt32 in_ulDataSize)
{
	AKOBJECT_TYPECHECK(AkNodeCategory_LayerCntr);

	AKRESULT eResult = AK_Success;

	// Read ID
	{
		// We don't care about the ID, just skip it
		const AkUInt32 ulID = READBANKDATA(AkUInt32, in_pData, in_ulDataSize);
		AKASSERT( ID() == ulID );
	}

	// Read basic parameters
	eResult = SetNodeBaseParams(in_pData, in_ulDataSize, false);

	if( eResult == AK_Success )
	{
		eResult = SetChildren( in_pData, in_ulDataSize );
	}

	if(eResult == AK_Success)
	{
		// Process Layers list
		const AkUInt32 ulNumLayers = READBANKDATA( AkUInt32, in_pData, in_ulDataSize );

		for( AkUInt32 i = 0; i < ulNumLayers; ++i )
		{
			// Peek the Layer ID
			AkUniqueID ulLayerID = AK::ReadUnaligned<AkUInt32>((AkUInt8*)in_pData);

			// Create the Layer object
			CAkLayer* pLayer = CAkLayer::Create( ulLayerID );

			if( ! pLayer )
			{
				eResult = AK_Fail;
			}
			else
			{
				// Read Layer's stuff from the bank
				pLayer->SetOwner( this );
				eResult = pLayer->SetInitialValues( in_pData, in_ulDataSize );

				if( eResult != AK_Success )
				{
					pLayer->Release();
				}
				else
				{
					// Add the layer to our list
					if ( ! m_layers.AddLast( pLayer ) )
					{
						eResult = AK_Fail;
						pLayer->Release();
					}
				}
			}

			if( eResult != AK_Success )
			{
				break;
			}
		}

		const AkUInt8 uContinuousValidation = READBANKDATA(AkUInt8, in_pData, in_ulDataSize);
		SetContinuousValidation(uContinuousValidation != 0);
	}

	CHECKBANKDATASIZE( in_ulDataSize, eResult );

	return eResult;
}

AKRESULT CAkLayerCntr::AddChildInternal( CAkParameterNodeBase* in_pChild )
{
	AKRESULT eResult = CAkParentNode::AddChildInternal( in_pChild );

	for ( LayerList::Iterator itLayer = m_layers.Begin(), itLayerEnd = m_layers.End();
		itLayer != itLayerEnd;
		++itLayer )
	{
		// If there is a key with a value that has no m_pChild, we must try to re-add that m_pChild.
		const CAkLayer::AssociatedChildMap::Iterator itChild = (*itLayer)->m_assocs.FindEx(in_pChild->ID());

		if (itChild != (*itLayer)->m_assocs.End() && itChild.pItem->item.m_pChild == NULL )
		{
			(*itLayer)->UpdateChildPtr( itChild.pItem->key );
		}
	}

	return eResult;
}

AKRESULT CAkLayerCntr::AddChild(WwiseObjectIDext in_ulID)
{
	AKRESULT eResult = __base::AddChild( in_ulID );

	if ( eResult == AK_Success )
	{
		// Maybe some layers referred to that child before it existed. Let's
		// tell them about it so they can connect to the child.
		for ( LayerList::Iterator itLayer = m_layers.Begin(), itLayerEnd = m_layers.End();
			  itLayer != itLayerEnd;
			  ++itLayer )
		{
			(*itLayer)->UpdateChildPtr( in_ulID.id );
		}
	}

	return eResult;
}

void CAkLayerCntr::RemoveChild( CAkParameterNodeBase* in_pChild )
{
	// Maybe some layers referred to that child. Let's tell it's about to go
	for ( LayerList::Iterator itLayer = m_layers.Begin(), itLayerEnd = m_layers.End();
			itLayer != itLayerEnd;
			++itLayer )
	{
		(*itLayer)->UnsetChildAssoc( in_pChild->ID() );
	}

	return __base::RemoveChild( in_pChild );
}

CAkLayerCntr* CAkLayerCntr::Create( AkUniqueID in_ulID )
{
	CAkLayerCntr* pAkLayerCntr = AkNew(AkMemID_Structure, CAkLayerCntr( in_ulID ) );
	if( pAkLayerCntr )
	{
		if( pAkLayerCntr->Init() != AK_Success )
		{
			pAkLayerCntr->Release();
			pAkLayerCntr = NULL;
		}
	}
	return pAkLayerCntr;
}

AkNodeCategory CAkLayerCntr::NodeCategory()
{
	return AkNodeCategory_LayerCntr;
}


void CAkLayerCntr::ExecuteAction(ActionParams& in_rAction)
{
	ExecuteActionInternal(in_rAction);
	__base::ExecuteAction(in_rAction);
}

void CAkLayerCntr::ExecuteActionNoPropagate(ActionParams& in_rAction)
{
	ExecuteActionInternal(in_rAction);
	__base::ExecuteActionNoPropagate(in_rAction);
}

void CAkLayerCntr::ExecuteActionInternal(ActionParams& in_rAction)
{
	switch (in_rAction.eType)
	{
	case ActionParamType_Stop:
	case ActionParamType_Break:
		StopContInst(in_rAction.pGameObj, in_rAction.playingID);
		break;
	case ActionParamType_Pause:
		SetPlaybackStateContInst(in_rAction.pGameObj, in_rAction.playingID, PB_Paused);
		break;
	case ActionParamType_Resume:
		SetPlaybackStateContInst(in_rAction.pGameObj, in_rAction.playingID, PB_Playing);
		break;
	default:
		break;
	}
}

void CAkLayerCntr::ExecuteActionExcept(ActionParamsExcept& in_rAction)
{
	switch (in_rAction.eType)
	{
	case ActionParamType_Stop:
		StopContInst(in_rAction.pGameObj, in_rAction.playingID);
		break;
	case ActionParamType_Pause:
		SetPlaybackStateContInst(in_rAction.pGameObj, in_rAction.playingID, PB_Paused);
		break;
	case ActionParamType_Resume:
		SetPlaybackStateContInst(in_rAction.pGameObj, in_rAction.playingID, PB_Playing);
		break;
	default:
		break;
	}
	__base::ExecuteActionExcept(in_rAction);
}

void CAkLayerCntr::SetPlaybackStateContInst(CAkRegisteredObj * in_pGameObj,
	AkPlayingID in_PlayingID,
	AkPlaybackState in_PBState
	)
{
	for (AkListContPlayback::Iterator iter = m_listContPlayback.Begin(); iter != m_listContPlayback.End(); ++iter)
	{
		ContPlaybackItem* pItem = *iter;
		if ((in_pGameObj == NULL || in_pGameObj == pItem->rtpcKey.GameObj()) &&
			(in_PlayingID == AK_INVALID_PLAYING_ID || pItem->UserParameters.PlayingID() == in_PlayingID))
		{
			pItem->ePlaybackState = in_PBState;
		}
	}
}

void CAkLayerCntr::StopContInst(CAkRegisteredObj * in_pGameObj,
	AkPlayingID in_PlayingID
	)
{
	AkListContPlayback::IteratorEx iter = m_listContPlayback.BeginEx();
	while (iter != m_listContPlayback.End())
	{
		ContPlaybackItem* pItem = *iter;
		if ((in_pGameObj == NULL || in_pGameObj == pItem->rtpcKey.GameObj()) &&
			(in_PlayingID == AK_INVALID_PLAYING_ID || pItem->UserParameters.PlayingID() == in_PlayingID))
		{
			// TBD: we will need to determine if this stop event was filtered out by the MIDI filters.
			iter = m_listContPlayback.Erase(iter);
			NotifyEndContinuous(*pItem);
			g_LayerCntrChunkPool.Delete(pItem);
		}
		else
		{
			++iter;
		}
	}
}

AKRESULT CAkLayerCntr::PlayInternal( AkPBIParams& in_rPBIParams )
{
	AKRESULT eResult = AK_Success;

#ifndef AK_OPTIMIZED
	// Set tracing data for Wwise profiler
	in_rPBIParams.SetPlayTargetID( ID() );
#endif

	// Populate the initial sound params so that they can be shared across sound instances.
	in_rPBIParams.PopulateInitialSoundParams(this);

    bool l_bIsContinuousPlay = in_rPBIParams.eType != AkPBIParams::PBI;
	if (m_bIsContinuousValidation)
	{
		if (!IncrementActivityCount())
		{
			DecrementActivityCount();
			return AK_Fail;
		}

		AddToURendererActiveNodes();

		ContPlaybackItem * pContItem = g_LayerCntrChunkPool.New();
		if (!pContItem)
		{
			DecrementActivityCount();
			RemoveFromURendererActiveNodes();
			return AK_Fail;
			//These were the ONLY return statement allowed in this function, the NotificationReason_IncrementCount must absolutely send NotificationReason_DecrementCount
		}

		m_listContPlayback.AddFirst(pContItem);

		pContItem->rtpcKey.PlayingID() = in_rPBIParams.userParams.PlayingID();
		pContItem->rtpcKey.GameObj() = in_rPBIParams.pGameObj;
		AkMidiNoteChannelPair noteAndChannel = in_rPBIParams.GetMidiEvent().GetNoteAndChannel();
		pContItem->rtpcKey.MidiChannelNo() = noteAndChannel.channel;
		pContItem->rtpcKey.MidiNoteNo() = noteAndChannel.note;
		if (in_rPBIParams.GetMidiEvent().IsValid() && in_rPBIParams.pInstigator)
			pContItem->rtpcKey.MidiTargetID() = in_rPBIParams.pInstigator->ID();

		pContItem->rtpcKey.GameObj()->AddRef();
		pContItem->UserParameters = in_rPBIParams.userParams;
		pContItem->ePlaybackState = in_rPBIParams.ePlaybackState;

#ifndef AK_OPTIMIZED
		pContItem->playTargetID = in_rPBIParams.playTargetID;
		pContItem->bPlayDirectly = in_rPBIParams.bPlayDirectly;
#endif

		g_pPlayingMgr->AddItemActiveCount(in_rPBIParams.userParams.PlayingID());

		for (LayerList::Iterator itLayer = m_layers.Begin(), itLayerEnd = m_layers.End(); itLayer != itLayerEnd; ++itLayer)
		{
			// Trigger modulators
			{
				AkModulatorSubscriberInfo subscrInfo;
				subscrInfo.pSubscriber = (*itLayer);
				subscrInfo.eSubscriberType = CAkRTPCMgr::SubscriberType_CAkLayer;
				subscrInfo.pTargetNode = this;

				AkModulatorTriggerParams params;
				params.midiEvent = in_rPBIParams.midiEvent;
				params.pMidiNoteState = in_rPBIParams.pMidiNoteState;
				params.midiTargetId = in_rPBIParams.GetMidiTargetID();
				params.pGameObj = in_rPBIParams.pGameObj;
				params.uFrameOffset = in_rPBIParams.uFrameOffset;
				params.playingId = in_rPBIParams.userParams.PlayingID();
				params.eTriggerMode = in_rPBIParams.bIsFirst ? AkModulatorTriggerParams::TriggerMode_FirstPlay : AkModulatorTriggerParams::TriggerMode_SubsequentPlay;
				params.pPbi = NULL;

				g_pModulatorMgr->Trigger(subscrInfo, params, &pContItem->modulatorData);
			}
		}

		// Simply play each child
		for (AkMapChildID::Iterator iter = this->m_mapChildId.Begin(); iter != this->m_mapChildId.End(); ++iter)
		{
			CAkParameterNodeBase* pNode = (*iter);

			AkUInt32 uMuted = 0;

			for (LayerList::Iterator itLayer = m_layers.Begin(), itLayerEnd = m_layers.End(); itLayer != itLayerEnd; ++itLayer)
			{
				AkUniqueID idRTPC = (*itLayer)->m_crossfadingRTPCID;
				if (idRTPC)
				{
					const CAkLayer::AssociatedChildMap::Iterator itChild = (*itLayer)->m_assocs.FindEx(pNode->ID());
					if (itChild != (*itLayer)->m_assocs.End())
					{
						AkReal32 rtpcValue;
						AkRTPCKey rtpcKey = pContItem->rtpcKey;
						bool bIsAutomatedParam = false;
						if (!g_pRTPCMgr->GetRTPCValue<CurrentValue>(idRTPC, RTPC_MaxNumRTPC, CAkRTPCMgr::SubscriberType_CAkCrossfadingLayer, rtpcKey, rtpcValue, bIsAutomatedParam))
							rtpcValue = g_pRTPCMgr->GetDefaultValue(idRTPC);

						if (itChild.pItem->item.IsOutsideOfActiveRange(rtpcValue))
							++uMuted;
					}
				}
			}

			ContPlaybackChildMuteCount * pMuteCount = pContItem->childMuteCount.Set(pNode->ID());
			if (pMuteCount)
				pMuteCount->uCount = uMuted;
			else
				uMuted = 0; // if we can't allocate for mute count, we won't be able to unmute... so we disable muting for this voice.

			if (uMuted == 0)
			{
				AkPBIParams params = in_rPBIParams; // ensure that each play is truly independent
				params.pInstigator = this;
				params.eType = AkPBIParams::PBI;
				params.pContinuousParams = NULL;
				params.sequenceID = AK_INVALID_SEQUENCE_ID;

				eResult = static_cast<CAkParameterNode*>(pNode)->Play(params);
			}
		}
	}
	else
	{
		// We must use this backup since there may be multiple playback, and this field will be overriden if multiple children are played.
		SafeContinuationList safeContList(in_rPBIParams, this);
		AkUInt32 l_ulNumSoundsStarted = 0;

		// Simply play each child
		AkUInt32 numPlayable = this->m_mapChildId.Length();
		for (AkMapChildID::Iterator iter = this->m_mapChildId.Begin(); iter != this->m_mapChildId.End(); ++iter)
		{
			CAkParameterNodeBase* pNode = (*iter);
			AkPBIParams params = in_rPBIParams; // ensure that each play is truly independent

			if (!l_bIsContinuousPlay)
			{
				eResult = static_cast<CAkParameterNode*>(pNode)->Play(params);
			}
			else
			{
				AkContParamsAndPath continuousParams(params.pContinuousParams);

				// Cross fade on blend containers should be possible only when there is only one sound in the actual object, 
				// otherwise, we do the normal behavior
				if (numPlayable == 1)
				{
					ContGetList(params.pContinuousParams->spContList, continuousParams.Get().spContList);
					eResult = AK_Success;
				}
				else
				{
					params.sequenceID = AK_INVALID_SEQUENCE_ID;//WG-16875 - Must prevent multi playback sounds from being sample accurate, they will default as using the normal transition system.

					continuousParams.Get().spContList.Attach(CAkContinuationList::Create());

					if (continuousParams.Get().spContList)
						eResult = AddMultiplayItem(continuousParams, params, safeContList);
					else
						eResult = AK_InsufficientMemory;
				}

				if (eResult == AK_Success)
				{
					params.pContinuousParams = &continuousParams.Get();
					eResult = static_cast<CAkParameterNode*>(pNode)->Play(params);
				}
			}

			if (eResult == AK_Success)
			{
				++l_ulNumSoundsStarted;
			}
		}
		if (l_ulNumSoundsStarted >= 1)
		{
			// Fixing WG-15390
			// we ignore errors on purpose here, if at least one children did play something, we call it a success.
			eResult = AK_Success;
		}

		if (l_bIsContinuousPlay)
		{
			if (l_ulNumSoundsStarted == 0)
			{
				eResult = PlayAndContinueAlternateMultiPlay(in_rPBIParams);
			}
			else
			{
				if (safeContList.Get())
				{
					eResult = ContUnrefList(safeContList.Get());
				}
			}
		}
	}

	return eResult;
}

AKRESULT CAkLayerCntr::AddLayer(
	AkUniqueID in_LayerID
)
{
	// Get the Layer object
	CAkLayer* pLayer = g_pIndex->m_idxLayers.GetPtrAndAddRef( in_LayerID );
	if ( !pLayer )
		return AK_IDNotFound;

	// When the object has been loaded from a bank, and is then
	// packaged by the WAL, it's possible that AddLayer is called
	// twice for the same layer, so let's make sure it's a new one
	if ( m_layers.FindEx( pLayer ) != m_layers.End() )
	{
		pLayer->Release();
		return AK_Success;
	}

	// Add the layer to our list
	if ( ! m_layers.AddLast( pLayer ) )
	{
		pLayer->Release();
		return AK_Fail;
	}

	pLayer->SetOwner( this );
	// keeping addref'd pLayer

	return AK_Success;
}

AKRESULT CAkLayerCntr::RemoveLayer(
	AkUniqueID in_LayerID 
)
{
	// Get the Layer object
	CAkLayer* pLayer = g_pIndex->m_idxLayers.GetPtrAndAddRef( in_LayerID );
	if ( !pLayer )
		return AK_IDNotFound;

	// Remove it from the list
	AKRESULT eResult = m_layers.Remove( pLayer );

	if ( eResult == AK_Success )
	{
		pLayer->SetOwner( NULL );
		pLayer->Release();
	}

	pLayer->Release();

	return eResult;
}

void CAkLayerCntr::SetContinuousValidation(
	bool in_bIsContinuousCheck
	)
{
	m_bIsContinuousValidation = in_bIsContinuousCheck;
}
