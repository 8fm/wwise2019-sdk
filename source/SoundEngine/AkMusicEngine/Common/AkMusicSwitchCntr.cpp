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
// AkMusicRanSeqCntr.cpp
//
// Music Random/Sequence container definition.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkMusicSwitchCntr.h"
#include "AkMusicSwitchCtx.h"
#include "AkCritical.h"
#include "AkSwitchMgr.h"
#include "AkMusicRenderer.h"
#include "AkBankMgr.h"

CAkMusicSwitchCntr::CAkMusicSwitchCntr( AkUniqueID in_ulID )
:CAkMusicTransAware( in_ulID )
,m_bIsContinuePlayback( true )
,m_pArguments(NULL)
,m_pGroupTypes(NULL)
{
}

CAkMusicSwitchCntr::~CAkMusicSwitchCntr()
{
    Term();
}

// Thread safe version of the constructor.
CAkMusicSwitchCntr * CAkMusicSwitchCntr::Create(
    AkUniqueID in_ulID
    )
{
    CAkFunctionCritical SpaceSetAsCritical;
	CAkMusicSwitchCntr * pSwitchCntr = AkNew(AkMemID_Structure, CAkMusicSwitchCntr( in_ulID ) );
    if( pSwitchCntr )
	{
		if( pSwitchCntr->Init() != AK_Success )
		{
			pSwitchCntr->Release();
			pSwitchCntr = NULL;
		}
	}
    return pSwitchCntr;
}

AKRESULT CAkMusicSwitchCntr::SetInitialValues( AkUInt8* in_pData, AkUInt32 in_ulDataSize )
{	
	AKOBJECT_TYPECHECK(AkNodeCategory_MusicSwitchCntr);

	AKRESULT eResult = SetMusicTransNodeParams( in_pData, in_ulDataSize, false );
	if ( eResult != AK_Success )
		return eResult;

	m_bIsContinuePlayback = (READBANKDATA( AkUInt8, in_pData, in_ulDataSize) != 0);

	AkUInt32 uTreeDepth = READBANKDATA(AkUInt32, in_pData, in_ulDataSize);

	AkUInt32 uArgumentsSize = uTreeDepth * ( sizeof( AkUniqueID ) );
	AkUInt32 uArgumentsGroupTypesSize = uTreeDepth * ( sizeof( AkUInt8 ) );

	eResult = SetArguments((AkUniqueID*)in_pData, (AkUInt8*)in_pData+uArgumentsSize, uTreeDepth);
	if ( eResult != AK_Success )
		return eResult;

	SKIPBANKBYTES( uArgumentsSize+uArgumentsGroupTypesSize, in_pData, in_ulDataSize );

	// Read decision tree

	AkUInt32 uTreeDataSize = READBANKDATA(AkUInt32, in_pData, in_ulDataSize);

	m_decisionTree.SetMode( READBANKDATA(AkUInt8, in_pData, in_ulDataSize) );

	eResult = m_decisionTree.SetTree( in_pData, uTreeDataSize, uTreeDepth );
	if ( eResult != AK_Success )
		return eResult;

	SKIPBANKBYTES( uTreeDataSize, in_pData, in_ulDataSize );

	CHECKBANKDATASIZE( in_ulDataSize, eResult );

	return eResult;
}

void CAkMusicSwitchCntr::SetAkProp( AkPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax )
{
	CAkMusicNode::SetAkProp(in_eProp, in_fValue, in_fMin, in_fMax);
}

void CAkMusicSwitchCntr::SetAkProp( AkPropID in_eProp, AkInt32 in_iValue, AkInt32 in_iMin, AkInt32 in_iMax )
{
	if ( in_eProp == AkPropID_DialogueMode )
		m_decisionTree.SetMode( (AkUInt8) in_iValue );
	else
		CAkMusicNode::SetAkProp(in_eProp, in_iValue, in_iMin, in_iMax);
}

AKRESULT CAkMusicSwitchCntr::SetDecisionTree( void* in_pData, AkUInt32 in_uSize, AkUInt32 in_uDepth )
{
	return m_decisionTree.SetTree( in_pData, in_uSize, in_uDepth );
}

AKRESULT CAkMusicSwitchCntr::SetArguments( AkUniqueID* in_pArgs, AkUInt8* in_pGroupTypes, AkUInt32 in_uNumArgs )
{
	AKRESULT res = AK_Success;

	ReleaseArgments();

	AKASSERT(m_pArguments==NULL);
	AkUInt32 argsSize = in_uNumArgs*sizeof(AkUniqueID);
	m_pArguments = (AkUniqueID *) AkAlloc(AkMemID_Structure, argsSize );
	if ( m_pArguments )
	{
		memcpy( m_pArguments, in_pArgs, argsSize );

		AKASSERT(m_pGroupTypes==NULL);
		AkUInt32 grouspTypesSize = in_uNumArgs*sizeof(AkUInt8);
		m_pGroupTypes = (AkUInt8*) AkAlloc(AkMemID_Structure, grouspTypesSize );
		if ( m_pGroupTypes )
		{
			memcpy( m_pGroupTypes, in_pGroupTypes, grouspTypesSize );
		}
		else
		{
			res = AK_InsufficientMemory;
		}
	}
	else
	{
		if ( in_uNumArgs > 0 )
			res = AK_InsufficientMemory;
	}

	return res;
}

void CAkMusicSwitchCntr::ReleaseArgments()
{
	if (m_pArguments)
	{
		AkFree(AkMemID_Structure, m_pArguments );
		m_pArguments = NULL;
	}
	if (m_pGroupTypes)
	{
		AkFree(AkMemID_Structure, m_pGroupTypes );
		m_pGroupTypes = NULL;
	}
}

void CAkMusicSwitchCntr::Term()
{
	ReleaseArgments();
}

// Return the node category.
AkNodeCategory CAkMusicSwitchCntr::NodeCategory()
{
    return AkNodeCategory_MusicSwitchCntr;
}
	
// Override MusicObject::ExecuteAction() to catch Seek actions.
void CAkMusicSwitchCntr::ExecuteAction( ActionParams& in_rAction )
{
    if ( ActionParamType_Seek == in_rAction.eType )
	{
		// No need to propagate this action on children; only segments can handle this.
		SeekActionParams & rSeekActionParams = (SeekActionParams&)in_rAction;
		if ( rSeekActionParams.bIsSeekRelativeToDuration )
		{
			CAkMusicRenderer::SeekPercent( this, rSeekActionParams.pGameObj, rSeekActionParams.playingID, rSeekActionParams.fSeekPercent, rSeekActionParams.bSnapToNearestMarker );
		}
		else
		{
			CAkMusicRenderer::SeekTimeAbsolute( this, rSeekActionParams.pGameObj, rSeekActionParams.playingID, rSeekActionParams.iSeekTime, rSeekActionParams.bSnapToNearestMarker );
		}
	}
	else
	{
		CAkMusicNode::ExecuteAction( in_rAction );
	}
}

// Hierarchy enforcement: Music RanSeq Cntr can only have Segments as parents.
AKRESULT CAkMusicSwitchCntr::CanAddChild(
    CAkParameterNodeBase * in_pAudioNode 
    )
{
    AKASSERT( in_pAudioNode );

	AkNodeCategory eCategory = in_pAudioNode->NodeCategory();

	AKRESULT eResult = AK_Success;	
	if(Children() >= AK_MAX_NUM_CHILD)
	{
		eResult = AK_MaxReached;
	}
	else if(eCategory != AkNodeCategory_MusicSegment &&
            eCategory != AkNodeCategory_MusicRanSeqCntr &&
            eCategory != AkNodeCategory_MusicSwitchCntr )
	{
		eResult = AK_NotCompatible;
	}
	else if(in_pAudioNode->Parent() != NULL)
	{
		eResult = AK_ChildAlreadyHasAParent;
	}
	else if(m_mapChildId.Exists(in_pAudioNode->ID()))
	{
		eResult = AK_AlreadyConnected;
	}
	else if(ID() == in_pAudioNode->ID())
	{
		eResult = AK_CannotAddItseflAsAChild;
	}
	return eResult;	
}

CAkMatrixAwareCtx * CAkMusicSwitchCntr::CreateContext( 
    CAkMatrixAwareCtx * in_pParentCtx,
    CAkRegisteredObj * in_GameObject,
    UserParams &  in_rUserparams,
	bool in_bPlayDirectly
    )
{
    CAkMusicSwitchCtx * pSwitchCntrCtx = AkNew(AkMemID_Object, CAkMusicSwitchCtx(
        this,
        in_pParentCtx ) );
    if ( pSwitchCntrCtx )
    {
		pSwitchCntrCtx->AddRef();
        if ( ( pSwitchCntrCtx->Init( in_GameObject, 
                                   in_rUserparams,
								   in_bPlayDirectly
                                    ) == AK_Success ) && ( pSwitchCntrCtx->RefCount() > 1 ) )
		{
			pSwitchCntrCtx->Release();
		}
		else
        {
            pSwitchCntrCtx->_Cancel();
			pSwitchCntrCtx->Release();
            pSwitchCntrCtx = NULL;
        }
    }
    return pSwitchCntrCtx;

}

AKRESULT CAkMusicSwitchCntr::PlayInternal( AkPBIParams& in_rPBIParams )
{
    // Create a top-level switch container context (that is, attached to the Music Renderer).
    // OPTIM. Could avoid virtual call.
    CAkMatrixAwareCtx * pCtx = CreateContext( NULL, in_rPBIParams.pGameObj, in_rPBIParams.userParams, in_rPBIParams.bPlayDirectly );
    if ( pCtx )
    {
		// Complete initialization of the context.
		pCtx->EndInit();

        // Do not set source offset: let it start playback at the position specified by the descendant's 
        // transition rules.
        
        AkMusicFade fadeParams;
        fadeParams.transitionTime   = in_rPBIParams.pTransitionParameters->TransitionTime;
		fadeParams.eFadeCurve       = in_rPBIParams.pTransitionParameters->eFadeCurve;
		// Set fade offset to context's silent duration.
        fadeParams.iFadeOffset		= pCtx->GetSilentDuration();
        pCtx->_Play( fadeParams );
		return AK_Success;
    }
    return AK_Fail;
}

//NOTE: Game sync preparation is only supported for single-argument switch containers.
AKRESULT CAkMusicSwitchCntr::ModifyActiveState( AkUInt32 in_stateID, bool in_bSupported )
{
	AKRESULT eResult = AK_Success;

	if( m_uPreparationCount != 0 && GetTreeDepth() == 1)
	{
		AkUniqueID audioNode = m_decisionTree.GetAudioNodeForState(in_stateID);
		if( audioNode != AK_INVALID_UNIQUE_ID )
		{
			if( in_bSupported )
			{
				eResult = PrepareNodeData( audioNode );
			}
			else
			{
				UnPrepareNodeData( audioNode );
			}
		}
	}

	return eResult;
}

//NOTE: Game sync preparation is only supported for single-argument switch containers.
AKRESULT CAkMusicSwitchCntr::PrepareData()
{
 	extern AkInitSettings g_settings;
	if( !g_settings.bEnableGameSyncPreparation || GetTreeDepth() != 1)
	{
		return CAkMusicNode::PrepareData();
	}

 	AKRESULT eResult = AK_Success;

	if( m_uPreparationCount == 0 )
	{
		eResult = PrepareMusicalDependencies();
		if( eResult == AK_Success )
		{
			AkUInt32 uGroupID = GetSwitchGroup(0);
			AkGroupType eGroupType = GetSwitchGroupType(0);

			CAkPreparedContent* pPreparedContent = GetPreparedContent( uGroupID, eGroupType );
			if( pPreparedContent )
			{
				AkDecisionTree::SwitchNodeAssoc switchNodeAssoc;
				m_decisionTree.GetSwitchNodeAssoc(switchNodeAssoc);

				for( AkDecisionTree::SwitchNodeAssoc::Iterator iter = switchNodeAssoc.Begin(); iter != switchNodeAssoc.End(); ++iter )
				{
					//Always include the fallback argument path.
					if( iter.pItem->key == 0 || pPreparedContent->IsIncluded( iter.pItem->key ) )
					{
						eResult = PrepareNodeData( iter.pItem->item );
					}
					if( eResult != AK_Success )
					{
						// Do not let this half-prepared, unprepare what has been prepared up to now.
						for( AkDecisionTree::SwitchNodeAssoc::Iterator iterFlush = switchNodeAssoc.Begin(); iterFlush != iter; ++iterFlush )
						{
							if( pPreparedContent->IsIncluded( iterFlush.pItem->key ) )
							{
								UnPrepareNodeData( iter.pItem->item );
							}
						}
					}
				}

				switchNodeAssoc.Term();

				if( eResult == AK_Success )
				{
					++m_uPreparationCount;
					eResult = SubscribePrepare( uGroupID, eGroupType );
					if( eResult != AK_Success )
						UnPrepareData();
				}
			}
			else
			{
				eResult = AK_InsufficientMemory;
			}
			if( eResult != AK_Success )
			{
				UnPrepareMusicalDependencies();
			}
		}
	}
	else
	{
		++m_uPreparationCount;
	}
	return eResult;
}

//NOTE: Game sync preparation is only supported for single-argument switch containers.
void CAkMusicSwitchCntr::UnPrepareData()
{
	extern AkInitSettings g_settings;
	if( !g_settings.bEnableGameSyncPreparation || GetTreeDepth() != 1)
	{
		return CAkMusicNode::UnPrepareData();
	}

	if( m_uPreparationCount != 0 ) // must check in case there were more unprepare than prepare called that succeeded.
	{
		if( --m_uPreparationCount == 0 )
		{
			AkUInt32 uGroupID = GetSwitchGroup(0);
			AkGroupType eGroupType = GetSwitchGroupType(0);

			CAkPreparedContent* pPreparedContent = GetPreparedContent( uGroupID, eGroupType );
			if( pPreparedContent )
			{
				AkDecisionTree::SwitchNodeAssoc switchNodeAssoc;
				m_decisionTree.GetSwitchNodeAssoc(switchNodeAssoc);

				for( AkDecisionTree::SwitchNodeAssoc::Iterator iter = switchNodeAssoc.Begin(); iter != switchNodeAssoc.End(); ++iter )
				{
					if( iter.pItem->key == 0 || pPreparedContent->IsIncluded( iter.pItem->key ) )
					{
						UnPrepareNodeData( iter.pItem->item );
					}
				}
				switchNodeAssoc.Term();
			}
			CAkPreparationAware::UnsubscribePrepare( uGroupID, eGroupType );
			UnPrepareMusicalDependencies();
		}
	}
}

void CAkMusicSwitchCntr::GatherSounds( AkSoundArray& io_aActiveSounds, AkSoundArray& io_aInactiveSounds, AkGameSyncArray& io_aGameSyncs, bool in_bIsActive, CAkRegisteredObj* in_pGameObj, AkUInt32 in_uUpdateGameSync, AkUInt32 in_uNewGameSyncValue )
{
	AkUniqueID audioNode = AK_INVALID_UNIQUE_ID;

	if (in_bIsActive)
	{
		AkUInt32 uNumGroups = m_decisionTree.Depth();
		AkUniqueID* pSwitchIDs = (AkUniqueID*)AkAlloca(sizeof(AkUniqueID)*uNumGroups);

		for (AkUInt32 i = 0; i < uNumGroups; ++i)
		{
			//Add the game sync to the list
			AkGameSync gs;
			gs.m_eGroupType = (AkGroupType)m_pGroupTypes[i];
			gs.m_ulGroup = m_pArguments[i];
			io_aGameSyncs.AddLast(gs);

			//Figure out the active node.  If the group is 'in_uUpdateGameSync' the use the passed in value (in_uNewGameSyncValue),
			//	because that means that this is being called on a notification, and the new value had not yet been updated in the rtpc mgr.
			// Get the switch to use
			AkSwitchStateID ulSwitchState = in_uNewGameSyncValue;
			if (gs.m_ulGroup != in_uUpdateGameSync)
			{
				if (gs.m_eGroupType == AkGroupType_State)
				{
					ulSwitchState = g_pStateMgr->GetState(gs.m_ulGroup);
				}
				else
				{
					AKASSERT(gs.m_eGroupType == AkGroupType_Switch);
					ulSwitchState = g_pSwitchMgr->GetSwitch(gs.m_ulGroup, AkRTPCKey(in_pGameObj));
				}
			}

			pSwitchIDs[i] = ulSwitchState;
		}


		audioNode = ResolvePath(pSwitchIDs, uNumGroups, 0);

		if (audioNode != AK_INVALID_UNIQUE_ID)
		{
			CAkMusicNode * pNode = static_cast<CAkMusicNode*>(g_pIndex->GetNodePtrAndAddRef(audioNode, AkNodeType_Default));
			if (pNode)
			{
				pNode->GatherSounds(io_aActiveSounds, io_aInactiveSounds, io_aGameSyncs, in_bIsActive, in_pGameObj, in_uUpdateGameSync, in_uNewGameSyncValue);
				pNode->Release();
			}
		}
	}

	AkMapChildID::Iterator iter = m_mapChildId.Begin();
	for (; iter != m_mapChildId.End(); ++iter)
	{
		if ( (*iter)->ID() != audioNode )
			(*iter)->GatherSounds(io_aActiveSounds, io_aInactiveSounds, io_aGameSyncs, false, in_pGameObj, in_uUpdateGameSync, in_uNewGameSyncValue);
	}
}
