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
// AkRanSeqCntr.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include "AkRanSeqCntr.h"
#include "AkAudioLibIndex.h"
#include "AkRandom.h"
#include "AkContinuationList.h"
#include "PrivateStructures.h"
#include "AkPlayingMgr.h"
#include "AkMonitor.h"
#include "AkAudioMgr.h"
#include "AkCntrHistory.h"
#include "AkModifiers.h"
#include "AkPlaylist.h"
#include "AkContinuousPBI.h"
#include "AkBanks.h"
#include "AkRegistryMgr.h"
#include "AkBankMgr.h"

#define AK_MIN_NUM_PER_OBJ_CNTR_INFO 8

inline AkHashType AkHash(CAkRegisteredObj *  in_obj) { return (AkHashType)(AkUIntPtr)in_obj; }

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WG-33084
// Warning: Making this number this low can create easy to repro performance issues in Wwise, the sound engine and the communication.
// This number cannot be zero as it would cause a request to play an infinity of sounds, causing stack overflow or shortage of memory.
// Problems grows exponentially when nearing 0.
AkReal32 g_kMinimumTriggerRateDelay = 0.001f; // Seconds.
////////////////////////////////////////////////////////////////////////////////////////////////////////////

CAkRanSeqCntr::CAkRanSeqCntr(AkUniqueID in_ulID, AkContainerMode in_ContainerMode /*= ContainerMode_Sequence*/)
:CAkContainerBase( in_ulID )
,m_pPlayList( NULL )
,m_pGlobalContainerInfo( NULL )
,m_bContainerBeenPlayed( false )
{
	m_LoopRanged.m_base = AkLoopVal_NotLooping;
	m_bIsContinuous = false;
	m_bIsGlobal = true;
	m_bIsRestartBackward = false;
	m_bIsUsingWeight = false;
	m_bResetPlayListAtEachPlay = true;
	m_eMode = in_ContainerMode;
	m_eRandomMode = RandomMode_Normal;
	m_eTransitionMode = Transition_Disabled;
	m_wAvoidRepeatCount = 0;
}

bool CAkRanSeqCntr::IsPlaylistDifferent(AkUInt8* in_pData, AkUInt32 in_ulDataSize)
{
    AkUInt32 ulPlayListItem = READBANKDATA( AkUInt16, in_pData, in_ulDataSize );
    if( ulPlayListItem != m_pPlayList->Length() )
        return true;

	for(AkUInt16 i = 0; i < ulPlayListItem; ++i)
	{
		AkUInt32 ulPlayID = READBANKDATA( AkUInt32, in_pData, in_ulDataSize );
		AkUInt32 weight = READBANKDATA( AkUInt32, in_pData, in_ulDataSize );
        if( m_pPlayList->ID( i ) != ulPlayID
            || m_pPlayList->GetWeight( i ) != weight )
            return true;
	}

    return false;
}

AKRESULT CAkRanSeqCntr::SetPlaylistWithoutCheck(AkUInt8*& io_pData, AkUInt32& io_ulDataSize)
{
    AKRESULT eResult = AK_Success;
    //Process Play list
	AkUInt32 ulPlayListItem = READBANKDATA( AkUInt16, io_pData, io_ulDataSize );
	for(AkUInt32 i = 0; i < ulPlayListItem; ++i)
	{
		AkUInt32 ulPlayID = READBANKDATA( AkUInt32, io_pData, io_ulDataSize );
		AkUInt32 weight = READBANKDATA( AkUInt32, io_pData, io_ulDataSize );
		eResult = AddPlaylistItem(ulPlayID, weight);
		if( eResult == AK_IDNotFound )
		{
			// It is not an error if something is in a playlist but not in a childlist.
			// It may occurs for example if a Parent container is packaged only because there is a play directly on one of its children
			eResult = AK_Success;
		}
		else if(eResult != AK_Success)
		{
			break;
		}
	}
    return eResult;
}

AKRESULT CAkRanSeqCntr::SetInitialValues(AkUInt8* in_pData, AkUInt32 in_ulDataSize)
{
	AKOBJECT_TYPECHECK(AkNodeCategory_RanSeqCntr);

	AKRESULT eResult = AK_Success;

	//Read ID
	// We don't care about the ID, just skip it
	SKIPBANKDATA(AkUInt32, in_pData, in_ulDataSize);

	//ReadParameterNode
	eResult = SetNodeBaseParams(in_pData, in_ulDataSize, false);

	//Read Looping info
	if(eResult == AK_Success)
	{
		AkUInt16 us1 = READBANKDATA(AkUInt16, in_pData, in_ulDataSize);
		AkUInt16 usMin = READBANKDATA(AkUInt16, in_pData, in_ulDataSize);
		AkUInt16 usMax = READBANKDATA(AkUInt16, in_pData, in_ulDataSize);

		Loop( true, us1 == 0, us1, usMin, usMax );

		AkReal32	fTransitionTime			= READBANKDATA( AkReal32, in_pData, in_ulDataSize );
		AkReal32	fTransitionTimeModMin	= READBANKDATA( AkReal32, in_pData, in_ulDataSize );
		AkReal32	fTransitionTimeModMax	= READBANKDATA( AkReal32, in_pData, in_ulDataSize );

		AkUInt16	wAvoidRepeatCount		= READBANKDATA( AkUInt16, in_pData, in_ulDataSize );

		AkUInt8	eTransitionMode				= READBANKDATA( AkUInt8, in_pData, in_ulDataSize );
		AkUInt8	eRandomMode					= READBANKDATA( AkUInt8, in_pData, in_ulDataSize );
		AkUInt8	eMode						= READBANKDATA( AkUInt8, in_pData, in_ulDataSize );
		
		AkUInt8 byBitVector					= READBANKDATA( AkUInt8, in_pData, in_ulDataSize );
		//AkUInt8 bIsUsingWeight			= GETBANKDATABIT( byBitVector, BANK_BITPOS_RNDSEQCNTR_USING_WEIGHT );
		AkUInt8 bResetPlayListAtEachPlay	= GETBANKDATABIT( byBitVector, BANK_BITPOS_RNDSEQCNTR_RESET_AT_EACH_PLAY );
		AkUInt8 bIsRestartBackward			= GETBANKDATABIT( byBitVector, BANK_BITPOS_RNDSEQCNTR_RESTART_BACKWARD );
		AkUInt8 bIsContinuous				= GETBANKDATABIT( byBitVector, BANK_BITPOS_RNDSEQCNTR_CONTINUOUS );
		AkUInt8 bIsGlobal					= GETBANKDATABIT( byBitVector, BANK_BITPOS_RNDSEQCNTR_GLOBAL );

		eResult = Mode( (AkContainerMode)eMode ); //Mode must be set first
		if(eResult == AK_Success)
		{
			TransitionTime( fTransitionTime, fTransitionTimeModMin, fTransitionTimeModMax );

			AvoidRepeatingCount	( wAvoidRepeatCount );
			TransitionMode		( (AkTransitionMode)eTransitionMode );
			RandomMode			( (AkRandomMode)eRandomMode );
			//bIsUsingWeight is useless for now... will be recalculated by the playlist
			ResetPlayListAtEachPlay( bResetPlayListAtEachPlay != 0 );
			RestartBackward		( bIsRestartBackward != 0 );
			Continuous			( bIsContinuous != 0 );
			IsGlobal			( bIsGlobal != 0 );
		}
	}

	if(eResult == AK_Success)
	{
		eResult = SetChildren( in_pData, in_ulDataSize );
	}

	if(eResult == AK_Success)
	{
		eResult = SetPlaylistWithoutCheck( in_pData, in_ulDataSize );
	}

	CHECKBANKDATASIZE( in_ulDataSize, eResult );

	return eResult;
}

CAkRanSeqCntr::~CAkRanSeqCntr()
{
	Term();
}

//====================================================================================================
//====================================================================================================
AKRESULT CAkRanSeqCntr::Init()
{
	AKRESULT eResult = CAkContainerBase::Init();

	if( eResult == AK_Success )
	{
		if(m_eMode == ContainerMode_Sequence)
		{
			m_pPlayList = AkNew(AkMemID_Structure,CAkPlayListSequence());
		}
		else
		{
			m_pPlayList = AkNew(AkMemID_Structure,CAkPlayListRandom());
		}
		if( m_pPlayList != NULL )
		{
			eResult = m_pPlayList->Init();
		}
		else
		{
			eResult = AK_Fail;
		}
	}
    
	return eResult;
}
//====================================================================================================
//====================================================================================================
void CAkRanSeqCntr::Term()
{
	if( m_pPlayList )
	{
		if(m_pPlayList->Length())
		{
			m_pPlayList->RemoveAll();
			m_bIsUsingWeight = false;

			DestroySpecificInfo();
		}
		m_pPlayList->Destroy();
	}
	m_mapObjectCntrInfo.Term();
}
//====================================================================================================
//====================================================================================================

CAkRanSeqCntr* CAkRanSeqCntr::Create(AkUniqueID in_ulID, AkContainerMode in_ContainerMode /*= ContainerMode_Sequence*/)
{
	CAkRanSeqCntr* pAkRanSeqCntr = AkNew(AkMemID_Structure, CAkRanSeqCntr( in_ulID, in_ContainerMode ) );
	if( pAkRanSeqCntr )
	{
		if( pAkRanSeqCntr->Init() != AK_Success )
		{
			pAkRanSeqCntr->Release();
			pAkRanSeqCntr = NULL;
		}
	}
	return pAkRanSeqCntr;
}

AkNodeCategory CAkRanSeqCntr::NodeCategory()
{
	return AkNodeCategory_RanSeqCntr;
}

// Notify the children that the associated object was unregistered
void CAkRanSeqCntr::Unregister(
	CAkRegisteredObj * in_pGameObj //Game object associated to the action
	)
{
	CAkContainerBase::Unregister( in_pGameObj );

	CntrInfoEntry * pEntry = m_mapObjectCntrInfo.Exists( in_pGameObj );
	if  ( pEntry )
	{
		pEntry->pInfo->Destroy();
		m_mapObjectCntrInfo.Unset( in_pGameObj );
	}
}

CAkSequenceInfo* CAkRanSeqCntr::CreateSequenceInfo()
{
	return AkNew(AkMemID_Object, CAkSequenceInfo() );
}

CAkRandomInfo* CAkRanSeqCntr::CreateRandomInfo( AkUInt16 in_uPlaylistSize )
{
	CAkRandomInfo* pRandomInfo = AkNew(AkMemID_Object, CAkRandomInfo( in_uPlaylistSize ) );
	if( !pRandomInfo )
		return NULL;

	if( pRandomInfo->Init(m_wAvoidRepeatCount) != AK_Success )
	{
		pRandomInfo->Destroy();
		return NULL;
	}

	if(m_bIsUsingWeight)
	{
		pRandomInfo->m_ulTotalWeight = pRandomInfo->m_ulRemainingWeight = CalculateTotalWeight();
	}
	return pRandomInfo;
}

CAkSequenceInfo* CAkRanSeqCntr::GetExistingSequenceInfo( CAkRegisteredObj * in_pGameObj )
{
	CAkSequenceInfo* pSeqInfo = NULL;
	if( IsGlobal() )
	{
		if(!m_pGlobalContainerInfo)
		{
			m_pGlobalContainerInfo = CreateSequenceInfo();
		}
		if( m_pGlobalContainerInfo )
		{
			pSeqInfo = static_cast<CAkSequenceInfo*>(m_pGlobalContainerInfo);
		}
	}
	else//PerObject
	{
		CntrInfoEntry * pEntry = m_mapObjectCntrInfo.Exists( in_pGameObj );
		if( !pEntry )
		{
			pSeqInfo = CreateSequenceInfo();
			if( !pSeqInfo)
				return NULL;

			CAkModifiedNodes* pModNodes = in_pGameObj->CreateComponent<CAkModifiedNodes>();
			if (!pModNodes || pModNodes->SetNodeAsModified(this) != AK_Success)
			{
				pSeqInfo->Destroy();
				return NULL;
			}

			pEntry = m_mapObjectCntrInfo.Set( in_pGameObj );
			if ( !pEntry )
			{
				pSeqInfo->Destroy();
				return NULL;
			}

			pEntry->pInfo = pSeqInfo;
		}
		else
		{
			pSeqInfo = static_cast<CAkSequenceInfo*>( pEntry->pInfo );
		}
	}
	return pSeqInfo;
}

CAkRandomInfo* CAkRanSeqCntr::GetExistingRandomInfo( AkUInt16 in_uPlaylistSize, CAkRegisteredObj * in_pGameObj )
{
	CAkRandomInfo* pRandomInfo = NULL;
	if( IsGlobal() )
	{
		if(!m_pGlobalContainerInfo)
		{
			m_pGlobalContainerInfo = CreateRandomInfo( in_uPlaylistSize );
		}
		if( m_pGlobalContainerInfo )
		{
			pRandomInfo = static_cast<CAkRandomInfo*>(m_pGlobalContainerInfo);
		}
	}
	else//PerObject
	{
		CntrInfoEntry * pEntry = m_mapObjectCntrInfo.Exists( in_pGameObj );
		if( !pEntry )
		{
			pRandomInfo = CreateRandomInfo( in_uPlaylistSize );
			if( !pRandomInfo )
				return NULL;

			CAkModifiedNodes* pModNodes = in_pGameObj->CreateComponent<CAkModifiedNodes>();
			if (!pModNodes || pModNodes->SetNodeAsModified(this) != AK_Success)
			{
				pRandomInfo->Destroy();
				return NULL;
			}
			pEntry = m_mapObjectCntrInfo.Set( in_pGameObj );
			if ( !pEntry )
			{
				pRandomInfo->Destroy();
				return NULL;
			}

			pEntry->pInfo = pRandomInfo;
		}
		else
		{
			pRandomInfo = static_cast<CAkRandomInfo*>( pEntry->pInfo );
		}
	}
	return pRandomInfo;
}

CAkParameterNodeBase* CAkRanSeqCntr::GetNextToPlay(CAkRegisteredObj * in_pGameObj, AkUInt16& out_rwPositionSelected, AkUniqueID& out_uSelectedNodeID)
{
	CAkParameterNodeBase* pNode = NULL; 
	out_rwPositionSelected = 0;
	out_uSelectedNodeID = AK_INVALID_UNIQUE_ID;

	AkUInt32 uPlaylistLength = m_pPlayList->Length();
	if( uPlaylistLength != 0 )
	{
		if( uPlaylistLength != 1 )
		{
			AkUInt32 numIterations = 0;
			bool bDeterministicMode = false;
			AkUInt16 wPos = 0;

			CAkSequenceInfo* pSeqInfo = NULL;
			CAkRandomInfo* pRandomInfo = NULL;
			if(m_eMode == ContainerMode_Sequence)
			{
				pSeqInfo = GetExistingSequenceInfo( in_pGameObj );
				if( !pSeqInfo )
					return NULL;
			}
			else
			{
				pRandomInfo = GetExistingRandomInfo( (AkUInt16)uPlaylistLength, in_pGameObj );
				if( !pRandomInfo )
					return NULL;
			}

			while( true )
			{
				bool bIsValid = true;
				if( bDeterministicMode )
				{
					++wPos;
					if( wPos >= uPlaylistLength )
						wPos = 0;//wrap around.

					bIsValid = CanPlayPosition( pRandomInfo, wPos );// So we at least respect Avoid Repeat count.
					if( bIsValid )
					{
						//here, update the pRandomInfo
						UpdateNormalAvoidRepeat( pRandomInfo, wPos );
					}
				}
				else if(m_eMode == ContainerMode_Sequence)
				{
					wPos = SelectSequentially( pSeqInfo, bIsValid );
				}
				else //Else is random
				{
					wPos = SelectRandomly( pRandomInfo, bIsValid );
				}
				if(bIsValid)
				{
					out_uSelectedNodeID = m_pPlayList->ID(wPos);
					pNode = g_pIndex->GetNodePtrAndAddRef( out_uSelectedNodeID, AkNodeType_Default );
					if( pNode )
					{
						if( pNode->IsPlayable() )
						{
							out_rwPositionSelected = wPos;
							break;//Found one, use it.
						}
						else
						{
							pNode->Release();
							pNode = NULL;
						}

					}
				}
				else
				{
					if( !bDeterministicMode )
					{
						//The selected node is not valid since we reached the end of the sequence, bail out with no selection.
						break;
					}
				}

				if( numIterations == 0 )
				{
					if( m_eMode == ContainerMode_Random && RandomMode() != RandomMode_Shuffle )
					{
						// We are random, and not shuffle, meaning that the worst case to find it is infinite...
						// We are going to force luck helping it being deterministic.
						bDeterministicMode = true;
					}
				}

				// Now looking for an alternative.
				++numIterations;

				if( numIterations >= uPlaylistLength )
				{
					break;// None was available, no alternative. Bail out.
				}
			}
		}
		else
		{
			out_uSelectedNodeID = m_pPlayList->ID(0);
			pNode = g_pIndex->GetNodePtrAndAddRef( out_uSelectedNodeID, AkNodeType_Default );
		}
	}
	return pNode;
}

CAkParameterNodeBase* CAkRanSeqCntr::GetNextToPlayContinuous(CAkRegisteredObj * in_pGameObj, AkUInt16& out_rwPositionSelected, AkUniqueID& out_uSelectedNodeID, CAkContainerBaseInfoPtr& io_pContainerInfo, AkLoop& io_rLoopInfo)
{
	AKASSERT(g_pIndex);
	CAkParameterNodeBase* pNode = NULL;
	out_uSelectedNodeID = AK_INVALID_UNIQUE_ID;
	out_rwPositionSelected = 0;

	AkUInt32 uPlaylistLength = m_pPlayList->Length();
	if(uPlaylistLength != 0)
	{
		if(uPlaylistLength != 1)
		{
			bool bIsValid = true;
			AkUInt16 wPos = 0;
			if(!m_bResetPlayListAtEachPlay && !io_pContainerInfo && m_eMode == ContainerMode_Sequence)
			{
				CAkSequenceInfo* pSeqInfo = NULL;
				if(!io_pContainerInfo)
				{
					CAkSequenceInfo* pSeqInfoReference = GetExistingSequenceInfo( in_pGameObj );
					if( ! pSeqInfoReference )
						return NULL;

					pSeqInfo = CreateSequenceInfo();
					if( !pSeqInfo )
						return NULL;

					*pSeqInfo = *pSeqInfoReference;
				}
				else
				{
					pSeqInfo = static_cast<CAkSequenceInfo*>( io_pContainerInfo );
				}	

				wPos = SelectSequentially(pSeqInfo,bIsValid,&io_rLoopInfo);
				io_pContainerInfo = pSeqInfo;
			}
			else //m_bResetPlayListAtEachPlay == TRUE (normal behavior)
			{
				if( m_eMode == ContainerMode_Sequence )
				{
					CAkSequenceInfo* pSeqInfo = NULL;
					if(!io_pContainerInfo)
					{
						io_pContainerInfo = CreateSequenceInfo();
						if( !io_pContainerInfo)
							return NULL;

						if(m_pGlobalContainerInfo)
						{
							CAkSequenceInfo* pSeqInfoGlobalLocal = static_cast<CAkSequenceInfo*>(m_pGlobalContainerInfo);
							//Support the ForceNextToPlay behavior on non-playing Sequence sound
							static_cast<CAkSequenceInfo*>(io_pContainerInfo)->m_i16LastPositionChosen = pSeqInfoGlobalLocal->m_i16LastPositionChosen;
							pSeqInfoGlobalLocal->m_i16LastPositionChosen = -1;
						}
					}
					pSeqInfo = static_cast<CAkSequenceInfo*>(io_pContainerInfo);
					AKASSERT(pSeqInfo);

					wPos = SelectSequentially(pSeqInfo,bIsValid,&io_rLoopInfo);
					if(!m_bResetPlayListAtEachPlay)
					{
						UpdateResetPlayListSetup(pSeqInfo, in_pGameObj);
					}
				}
				else //Else is random
				{
					CAkRandomInfo* pRandomInfo = NULL;
					if(!io_pContainerInfo)
					{
						pRandomInfo = CreateRandomInfo( (AkUInt16)uPlaylistLength );
						if( !pRandomInfo )
							return NULL;

						io_pContainerInfo = pRandomInfo;
					}
					else
					{
						pRandomInfo = static_cast<CAkRandomInfo*>(io_pContainerInfo);
					}	
					AKASSERT(pRandomInfo);

					wPos = SelectRandomly(pRandomInfo,bIsValid,&io_rLoopInfo);
				}
			}
			if(bIsValid)
			{
				out_rwPositionSelected = wPos;
				out_uSelectedNodeID = m_pPlayList->ID(wPos);
				pNode = g_pIndex->GetNodePtrAndAddRef( out_uSelectedNodeID, AkNodeType_Default );
			}
		}
		else
		{
			if(io_rLoopInfo.lLoopCount > 0)
			{
				if(!io_rLoopInfo.bIsInfinite)
				{
					--(io_rLoopInfo.lLoopCount);
				}
				out_uSelectedNodeID = m_pPlayList->ID(0);
				pNode = g_pIndex->GetNodePtrAndAddRef( out_uSelectedNodeID, AkNodeType_Default );
			}
		}
	}
	return pNode;
}

AKRESULT CAkRanSeqCntr::AddPlaylistItem(AkUniqueID in_ElementID, AkUInt32 in_weight /*=DEFAULT_RANDOM_WEIGHT*/)
{
	AKRESULT eResult = AK_Success;

	if(in_weight!=DEFAULT_RANDOM_WEIGHT)
	{
		m_bIsUsingWeight = true;
	}

	if(m_eMode == ContainerMode_Sequence || !m_pPlayList->Exists(in_ElementID))
	{
		eResult = m_pPlayList->Add( in_ElementID, in_weight );
	}
	else
	{
		eResult = AK_ElementAlreadyInList;
	}

	if(eResult == AK_Success)
	{
		ResetSpecificInfo();
	}
	return eResult;
}

AKRESULT CAkRanSeqCntr::SetPlaylist( void* in_pvListBlock, AkUInt32 in_ulParamBlockSize )
{
    AKRESULT eResult = AK_Success;
    if( IsPlaylistDifferent((AkUInt8*)in_pvListBlock, in_ulParamBlockSize) )
    {
        //First Clear the old playlist if there was one
		m_pPlayList->RemoveAll();
		m_bIsUsingWeight = false;
		
        //Then replace it with the new one
        AkUInt8* ptr = (AkUInt8*)in_pvListBlock;
        eResult = SetPlaylistWithoutCheck( ptr, in_ulParamBlockSize );

        //Only then, We will force audio to stop.
        ResetSpecificInfo();
    }
    return eResult;
}

//---------------  Not to be exported to thd SDK  ---------------
void CAkRanSeqCntr::_SetItemWeight(AkUniqueID in_ID, AkUInt32 in_weight)
{
	AkUInt16 wPosition = 0;
	if(m_eMode == ContainerMode_Random && m_pPlayList->GetPosition(in_ID, wPosition))
	{
		SetItemWeight(wPosition,in_weight);
	}
	else
	{
		////////////////////////////////////////////////////////////////////////////////
		// You either tried to call this WAL reserved function on a Non-Random container 
		// or specified an ID that was not found in the Playlist.
		//AKASSERT(!"Invalid Try to Set the weight of a container playlist element");
		////////////////////////////////////////////////////////////////////////////////
	}
}

void CAkRanSeqCntr::SetItemWeight(AkUInt16 in_wPosition, AkUInt32 in_weight)
{
	AKASSERT(m_eMode == ContainerMode_Random);
	CAkPlayListRandom* pRandomPlaylist = static_cast<CAkPlayListRandom *>(m_pPlayList);
	AKASSERT(pRandomPlaylist);
	AKASSERT(in_weight && "Weight == 0 means unpredictable behavior, ignored");
	if(in_weight && pRandomPlaylist->GetWeight(in_wPosition) != in_weight)
	{
		pRandomPlaylist->SetWeight(in_wPosition,in_weight);
		m_bIsUsingWeight = true;
		ResetSpecificInfo();
	}
}

bool CAkRanSeqCntr::RestartBackward()
{
	return m_bIsRestartBackward;
}

void CAkRanSeqCntr::RestartBackward(const bool in_bRestartBackward)
{
	m_bIsRestartBackward = in_bRestartBackward;
}

bool CAkRanSeqCntr::Continuous()
{
	return m_bIsContinuous;
}

void CAkRanSeqCntr::Continuous(const bool in_bIsContinuous)
{
    if( m_bIsContinuous != in_bIsContinuous )
    {
	    m_bIsContinuous = in_bIsContinuous;
	    ResetSpecificInfo();
    }
}

bool CAkRanSeqCntr::IsGlobal()
{
	return m_bIsGlobal;
}

void CAkRanSeqCntr::IsGlobal(bool in_bIsGlobal)
{
    if( m_bIsGlobal != in_bIsGlobal )
    {
	    m_bIsGlobal = in_bIsGlobal;
	    ResetSpecificInfo();
    }
}

bool CAkRanSeqCntr::ResetPlayListAtEachPlay()
{
	return m_bResetPlayListAtEachPlay;
}

void CAkRanSeqCntr::ResetPlayListAtEachPlay(bool in_bResetPlayListAtEachPlay)
{
    if( m_bResetPlayListAtEachPlay != in_bResetPlayListAtEachPlay )
    {
	    m_bResetPlayListAtEachPlay = in_bResetPlayListAtEachPlay;
	    ResetSpecificInfo();
    }
}

AkTransitionMode CAkRanSeqCntr::TransitionMode()
{
	return (AkTransitionMode)m_eTransitionMode;
}

void CAkRanSeqCntr::TransitionMode(AkTransitionMode in_eTransitionMode)
{
    if( m_eTransitionMode != in_eTransitionMode )
    {
	    m_eTransitionMode = in_eTransitionMode;
	    ResetSpecificInfo();
    }
}

AkReal32 CAkRanSeqCntr::TransitionTime( const AkRTPCKey& in_rtpcKey )
{
	AkReal32 transitionTime = RandomizerModifier::GetModValue( m_TransitionTime );

	if ( HasRtpcOrState( RTPC_PlayMechanismSpecialTransitionsValue ) )
	{
		AkReal32 fRtpcState = GetRTPCAndState( RTPC_PlayMechanismSpecialTransitionsValue, in_rtpcKey ) * 1000.0f;
		transitionTime += fRtpcState;
	}

	return AkMath::Max( 0.0f, transitionTime );
}

void CAkRanSeqCntr::TransitionTime(AkReal32 in_TransitionTime,AkReal32 in_RangeMin/*=0*/, AkReal32 in_RangeMax/*=0*/)
{
	RandomizerModifier::SetModValue( m_TransitionTime, in_TransitionTime, in_RangeMin, in_RangeMax ) ;
}

AkRandomMode CAkRanSeqCntr::RandomMode() const
{
	return (AkRandomMode)m_eRandomMode;
}

void CAkRanSeqCntr::RandomMode(AkRandomMode in_eRandomMode)
{
    if( m_eRandomMode != in_eRandomMode )
    {
	    m_eRandomMode = in_eRandomMode;
	    ResetSpecificInfo();
    }
}

AkUInt16 CAkRanSeqCntr::AvoidRepeatingCount()
{
	return m_wAvoidRepeatCount;
}

void CAkRanSeqCntr::AvoidRepeatingCount(AkUInt16 in_wCount)
{
    if( m_wAvoidRepeatCount != in_wCount )
    {
	    m_wAvoidRepeatCount = in_wCount;
	    ResetSpecificInfo();
    }
}

AkContainerMode CAkRanSeqCntr::Mode()
{
	return (AkContainerMode)m_eMode;
}

AKRESULT CAkRanSeqCntr::Mode( AkContainerMode in_eMode )
{
	AKRESULT eResult = AK_Success;

	if(in_eMode != m_eMode)
	{
		m_eMode = in_eMode;
		if( m_pPlayList )
		{
			m_pPlayList->Destroy();
		}
		if(m_eMode == ContainerMode_Sequence)
		{
			m_pPlayList = AkNew(AkMemID_Structure,CAkPlayListSequence());
		}
		else
		{
			m_pPlayList = AkNew(AkMemID_Structure,CAkPlayListRandom());
		}
    
		if( m_pPlayList )
		{
			eResult = m_pPlayList->Init();
			if( eResult != AK_Success )
			{
				m_pPlayList->Destroy();
				m_pPlayList = NULL;
			}
		}
		else
		{
			eResult = AK_Fail;
		}

		ResetSpecificInfo();
	}

	return eResult;
}

void	CAkRanSeqCntr::Loop(
		bool  in_bIsLoopEnabled,
		bool  in_bIsInfinite,
		AkInt16 in_sLoopCount,
		AkInt16 in_sLoopModMin,
		AkInt16 in_sLoopModMax
		)
{
	if( in_bIsLoopEnabled )
	{
		if( in_bIsInfinite )
		{
			RandomizerModifier::SetModValue( m_LoopRanged, (AkInt16)AkLoopVal_Infinite, (AkInt16)0, (AkInt16)0 );
		}
		else
		{
			RandomizerModifier::SetModValue( m_LoopRanged, (AkInt16)in_sLoopCount, (AkInt16)in_sLoopModMin, (AkInt16)in_sLoopModMax );
		}
	}
	else
	{
		RandomizerModifier::SetModValue( m_LoopRanged, (AkInt16)AkLoopVal_NotLooping, (AkInt16)0, (AkInt16)0 );
	}
}

bool CAkRanSeqCntr::IsInfiniteLooping( CAkPBIAware* in_pInstigator )
{
	if ( m_bIsContinuous && m_LoopRanged.m_base == AkLoopVal_Infinite )
		return true;

	return CAkParameterNodeBase::IsInfiniteLooping( in_pInstigator );
}

void CAkRanSeqCntr::SafeResetSpecificInfo(CAkRegisteredObj * in_pGameObj)
{
	//No Reset in continuous mode, both dangerous and not handled.
	if (!m_bIsContinuous) //only step allowed.
	{
		if (in_pGameObj == NULL)
		{
			DestroySpecificInfo();
		}
		else //game object specific.
		{
			if (IsGlobal())
			{
				MONITOR_MSG(AKTEXT("Reset playlist action with a game object specific scope is ignored on a global container."));
			}
			else
			{
				CntrInfoEntry * pEntry = m_mapObjectCntrInfo.Exists(in_pGameObj);
				if (pEntry)
				{
					pEntry->pInfo->Destroy();
					m_mapObjectCntrInfo.Unset(in_pGameObj);
				}
			}
		}
	}
	else
	{
		MONITOR_MSG(AKTEXT("Reset playlist action on a continuous container is ignored."));
	}
}

void CAkRanSeqCntr::ResetSpecificInfo()
{
	DestroySpecificInfo();

	if(m_bContainerBeenPlayed)
	{
		if( g_pAudioMgr )
		{
			g_pAudioMgr->RemovePausedPendingAction( this );
			g_pAudioMgr->RemovePendingAction( this );
		}

		Stop();
	}
}

void CAkRanSeqCntr::DestroySpecificInfo()
{
	for ( AkMapObjectCntrInfo::Iterator it = m_mapObjectCntrInfo.Begin(); it != m_mapObjectCntrInfo.End(); ++it )
		(*it).pInfo->Destroy();

	m_mapObjectCntrInfo.RemoveAll();

	if(m_pGlobalContainerInfo)
	{
		m_pGlobalContainerInfo->Destroy();
		m_pGlobalContainerInfo = NULL;
	}
}

AkUInt32 CAkRanSeqCntr::GetPlaylistLength()
{ 
	if( m_pPlayList )
		return m_pPlayList->Length();
	return 0;
}

AkUInt32 CAkRanSeqCntr::CalculateTotalWeight()
{
	AKASSERT(m_eMode == ContainerMode_Random);
	CAkPlayListRandom* pRandomPlaylist = static_cast<CAkPlayListRandom *>(m_pPlayList);
	AKASSERT(pRandomPlaylist);
	return pRandomPlaylist->CalculateTotalWeight();
}

AkUInt16 CAkRanSeqCntr::SelectSequentially(CAkSequenceInfo* in_pSeqInfo, bool& out_bIsAnswerValid, AkLoop* io_pLoopCount)
{
	AKASSERT( in_pSeqInfo );
	out_bIsAnswerValid = true;
	if(in_pSeqInfo->m_bIsForward)
	{
		if(in_pSeqInfo->m_i16LastPositionChosen+1 == m_pPlayList->Length())//reached the end of the sequence
		{
			if(m_bIsRestartBackward)
			{
				--(in_pSeqInfo->m_i16LastPositionChosen);
				in_pSeqInfo->m_bIsForward = false;
			}
			else//Restart to zero
			{
				in_pSeqInfo->m_i16LastPositionChosen = 0;
				if(!CanContinueAfterCompleteLoop(io_pLoopCount))
				{
					out_bIsAnswerValid = false;
					return 0;
				}
			}
		}
		else//not finished sequence
		{
			++(in_pSeqInfo->m_i16LastPositionChosen);
		}
	}
	else//isgoingbackward
	{
		if(in_pSeqInfo->m_i16LastPositionChosen == 0)//reached the end of the sequence ( == 0)
		{
			in_pSeqInfo->m_i16LastPositionChosen = 1;
			in_pSeqInfo->m_bIsForward = true;
			if(!CanContinueAfterCompleteLoop(io_pLoopCount))
			{
				out_bIsAnswerValid = false;
				return 0;
			}
		}
		else//not finished sequence
		{
			--(in_pSeqInfo->m_i16LastPositionChosen);
		}
	}
	return in_pSeqInfo->m_i16LastPositionChosen;
}

AKRESULT CAkRanSeqCntr::UpdateNormalAvoidRepeat( CAkRandomInfo* in_pRandomInfo, AkUInt16 in_uPosition )
{
	AKASSERT(m_eMode == ContainerMode_Random);
	CAkPlayListRandom* pRandomPlaylist = static_cast<CAkPlayListRandom *>(m_pPlayList);

	if(m_wAvoidRepeatCount)
	{
		in_pRandomInfo->m_wRemainingItemsToPlay -= 1;
		if( in_pRandomInfo->m_listAvoid.AddLast(in_uPosition) == NULL )
		{
			// Reset counter( will force refresh on next play ).
			// and return position 0.
			in_pRandomInfo->m_wCounter = 0;
			return AK_Fail;
		}

		in_pRandomInfo->FlagAsBlocked(in_uPosition);
		in_pRandomInfo->m_ulRemainingWeight -= pRandomPlaylist->GetWeight(in_uPosition);
		AkUInt16 uVal = (AkUInt16)( m_pPlayList->Length()-1 );
		if(in_pRandomInfo->m_listAvoid.Length() > AkMin(m_wAvoidRepeatCount, uVal ))
		{
			AkUInt16 wToBeRemoved = in_pRandomInfo->m_listAvoid[0];
			in_pRandomInfo->FlagAsUnBlocked(wToBeRemoved);
			in_pRandomInfo->m_ulRemainingWeight += pRandomPlaylist->GetWeight(wToBeRemoved);
			in_pRandomInfo->m_wRemainingItemsToPlay += 1;
			in_pRandomInfo->m_listAvoid.Erase(0);
		}
	}
	return AK_Success;
}

AkUInt16 CAkRanSeqCntr::SelectRandomly(CAkRandomInfo* in_pRandomInfo, bool& out_bIsAnswerValid, AkLoop* io_pLoopCount)
{
	AKASSERT(in_pRandomInfo);
	out_bIsAnswerValid = true;
	AkUInt16 wPosition = 0;
	int iValidCount = -1;
	int iCycleCount = 0;

	AKASSERT(m_eMode == ContainerMode_Random);
	CAkPlayListRandom* pRandomPlaylist = static_cast<CAkPlayListRandom *>(m_pPlayList);

	if(!in_pRandomInfo->m_wCounter)
	{
		if(!CanContinueAfterCompleteLoop(io_pLoopCount))
		{
			out_bIsAnswerValid = false;
			return 0;
		}
		in_pRandomInfo->m_wCounter = (AkUInt16)m_pPlayList->Length();
		in_pRandomInfo->ResetFlagsPlayed(m_pPlayList->Length());

		if(RandomMode() == RandomMode_Shuffle)
		{	
			in_pRandomInfo->m_ulRemainingWeight = in_pRandomInfo->m_ulTotalWeight;
			for( CAkRandomInfo::AkAvoidList::Iterator iter = in_pRandomInfo->m_listAvoid.Begin(); iter != in_pRandomInfo->m_listAvoid.End(); ++iter )
			{
				in_pRandomInfo->m_ulRemainingWeight -= pRandomPlaylist->GetWeight(*iter);
			}
		}
		in_pRandomInfo->m_wRemainingItemsToPlay -= (AkUInt16)in_pRandomInfo->m_listAvoid.Length();
	}

	AKASSERT(in_pRandomInfo->m_wRemainingItemsToPlay);//Should never be here if empty...

	if(m_bIsUsingWeight)
	{
		int iRandomValue = in_pRandomInfo->GetRandomValue();
		while(iValidCount < iRandomValue)
		{
			if(CanPlayPosition(in_pRandomInfo, iCycleCount))
			{
				iValidCount += pRandomPlaylist->GetWeight(iCycleCount);
			}
			++iCycleCount;
			AKASSERT(((size_t)(iCycleCount-1)) < m_pPlayList->Length());
		}
	}
	else
	{
		int iRandomValue = (AkUInt16)(AKRANDOM::AkRandom() % in_pRandomInfo->m_wRemainingItemsToPlay);
		while(iValidCount < iRandomValue)
		{
			if(CanPlayPosition(in_pRandomInfo, iCycleCount))
			{
				++iValidCount;
			}
			++iCycleCount;
			AKASSERT(((size_t)(iCycleCount-1)) < m_pPlayList->Length());
		}
	}
	wPosition = iCycleCount - 1;
	
	if(RandomMode() == RandomMode_Normal)//Normal
	{
		if(!in_pRandomInfo->IsFlagSetPlayed(wPosition))
		{
			in_pRandomInfo->FlagSetPlayed(wPosition);
			in_pRandomInfo->m_wCounter -=1;
		}
		if( UpdateNormalAvoidRepeat( in_pRandomInfo, wPosition ) != AK_Success )
			return wPosition;
	}
	else//Shuffle
	{
		AkUInt16 wBlockCount = m_wAvoidRepeatCount?m_wAvoidRepeatCount:1;

		in_pRandomInfo->m_ulRemainingWeight -= pRandomPlaylist->GetWeight(wPosition);
		in_pRandomInfo->m_wRemainingItemsToPlay -= 1;
		in_pRandomInfo->m_wCounter -=1;
		in_pRandomInfo->FlagSetPlayed(wPosition);
		if( in_pRandomInfo->m_listAvoid.AddLast(wPosition) == NULL )
		{
			// Reset counter( will force refresh on next play ).
			// and return position 0.
			in_pRandomInfo->m_wCounter = 0;
			return wPosition;
		}
		in_pRandomInfo->FlagAsBlocked(wPosition);

		AkUInt16 uVal = (AkUInt16)( m_pPlayList->Length()-1 );
		if(in_pRandomInfo->m_listAvoid.Length() > AkMin( wBlockCount, uVal ))
		{
			AkUInt16 wToBeRemoved = in_pRandomInfo->m_listAvoid[0];
			in_pRandomInfo->m_listAvoid.Erase(0);
			in_pRandomInfo->FlagAsUnBlocked(wToBeRemoved);
			if(!in_pRandomInfo->IsFlagSetPlayed(wToBeRemoved))
			{
				in_pRandomInfo->m_ulRemainingWeight += pRandomPlaylist->GetWeight(wToBeRemoved);
				in_pRandomInfo->m_wRemainingItemsToPlay += 1;
			}
		}
	}
	return wPosition;
}

bool CAkRanSeqCntr::CanContinueAfterCompleteLoop(AkLoop* io_pLoopingInfo/*= NULL*/)
{
	bool bAnswer = false;
	if(!io_pLoopingInfo)
	{
		bAnswer = true;
	}
	else if(io_pLoopingInfo->bIsEnabled)
	{
		if(io_pLoopingInfo->bIsInfinite)
		{
			bAnswer = true;
		}
		else
		{
			--(io_pLoopingInfo->lLoopCount);
			bAnswer = (io_pLoopingInfo->lLoopCount)? true:false; 
		}
	}
	return bAnswer;
}

bool CAkRanSeqCntr::CanPlayPosition(const CAkRandomInfo* in_pRandomInfo, AkUInt16 in_wPosition) const
{
	if(RandomMode() == RandomMode_Normal)
	{
		if(m_wAvoidRepeatCount)
		{
			return !in_pRandomInfo->IsFlagBlocked(in_wPosition);
		}
		else
		{
			return true;
		}
	}
	else//Shuffle
	{
		return !in_pRandomInfo->IsFlagSetPlayed(in_wPosition) &&
			!in_pRandomInfo->IsFlagBlocked(in_wPosition);
	}
}

AKRESULT CAkRanSeqCntr::PlayInternal( AkPBIParams& in_rPBIParams )
{
	m_bContainerBeenPlayed = true;

#ifndef AK_OPTIMIZED
	// Set tracing data for Wwise profiler
	in_rPBIParams.SetPlayTargetID( ID() );
#endif

	if ( !m_bIsContinuous )
		return _Play(in_rPBIParams);

	ContParams continuousParams; // Must be outside the if because of: in_rPBIParams.pContinuousParams = &continuousParams; Must stay on the stack.
	if ( in_rPBIParams.eType == AkPBIParams::PBI )
	{ 
		// We are going from PBIParams to ContinuousPBIParams
		in_rPBIParams.eType = AkPBIParams::ContinuousPBI;
        in_rPBIParams.pInstigator = this;

		continuousParams.spContList.Attach(CAkContinuationList::Create());
		if (!continuousParams.spContList)
			return AK_Fail;
			
        in_rPBIParams.pContinuousParams = &continuousParams;
	}
	else if( in_rPBIParams.pContinuousParams && !(in_rPBIParams.pContinuousParams->spContList) )
	{
		// WG-20510 - Pad stuation where in_rPBIParams.pContinuousParams->spContList would not be allocated yet.
		in_rPBIParams.pContinuousParams->spContList.Attach( CAkContinuationList::Create() );
		if ( !(in_rPBIParams.pContinuousParams->spContList) )
			return AK_Fail;
	}

	if (TransitionMode() == Transition_TriggerRate)
		return _PlayTrigger(in_rPBIParams);

	return _PlayContinuous( in_rPBIParams );
}

AKRESULT CAkRanSeqCntr::_Play( AkPBIParams& in_rPBIParams )
{
	AKASSERT( !m_bIsContinuous );
	AKRESULT eResult( AK_Fail );

	AkUniqueID uSelectedNodeID;
	AkUInt16 wPositionSelected;
	CAkParameterNodeBase* pSelectedNode = GetNextToPlay( in_rPBIParams.pGameObj, wPositionSelected, uSelectedNodeID );

	if( pSelectedNode )
	{
		in_rPBIParams.playHistory.Add( wPositionSelected, false );
		eResult = static_cast<CAkParameterNode*>(pSelectedNode)->Play( in_rPBIParams );
		pSelectedNode->Release();
		return eResult;
	}
	else
	{
		MONITOR_ERROR_PARAM( AK::Monitor::ErrorCode_SelectedChildNotAvailable, uSelectedNodeID, in_rPBIParams.userParams.PlayingID(), in_rPBIParams.pGameObj->ID(), this->ID(), false );
		if ( in_rPBIParams.eType == AkPBIParams::PBI )
		{
			CAkCntrHist HistArray;
			MONITOR_OBJECTNOTIF( in_rPBIParams.userParams.PlayingID(), in_rPBIParams.pGameObj->ID(), in_rPBIParams.userParams.CustomParam(), AkMonitorData::NotificationReason_PlayFailed, HistArray, ID(), false, 0 );
			return eResult;
		}
	}

	return PlayAndContinueAlternate(in_rPBIParams);
}

AKRESULT CAkRanSeqCntr::_PlayContinuous( AkPBIParams& in_rPBIParams )
{
	AKASSERT( m_bIsContinuous &&  
	          in_rPBIParams.pContinuousParams && 
	          in_rPBIParams.pContinuousParams->spContList );

    AKRESULT eResult( AK_Fail );

	CAkContinueListItem * pItem = AddNewContListItem(in_rPBIParams);
	if ( pItem )
	{
		AkUniqueID uSelectedNodeID;
		AkUInt16 wPositionSelected;
		CAkParameterNodeBase* pSelectedNode = GetNextToPlayContinuous(in_rPBIParams.pGameObj,
			wPositionSelected,
			uSelectedNodeID,
			pItem->m_pContainerInfo,
			pItem->m_LoopingInfo );

		if( pSelectedNode )
		{
			in_rPBIParams.playHistory.Add( wPositionSelected, true );
			eResult = static_cast<CAkParameterNode*>(pSelectedNode)->Play( in_rPBIParams );
			pSelectedNode->Release();
			return eResult;
		}
		else
		{
			MONITOR_ERROR_PARAM( AK::Monitor::ErrorCode_SelectedChildNotAvailable, uSelectedNodeID, in_rPBIParams.userParams.PlayingID(), in_rPBIParams.pGameObj->ID(), this->ID(), false );
            in_rPBIParams.pContinuousParams->spContList->m_listItems.RemoveLast();
		}
	}
	else
	{
		MONITOR_ERROREX( AK::Monitor::ErrorCode_PlayFailed, in_rPBIParams.userParams.PlayingID(), in_rPBIParams.pGameObj->ID(), this->ID(), false );
	}

	return PlayAndContinueAlternate(in_rPBIParams);
}

CAkContinueListItem * CAkRanSeqCntr::AddNewContListItem(AkPBIParams& in_rPBIParams)
{
	CAkContinueListItem * pItem = in_rPBIParams.pContinuousParams->spContList->m_listItems.AddLast();
	if (pItem)
	{
		pItem->m_pContainer = this;

		pItem->m_LoopingInfo.bIsEnabled = (m_LoopRanged.m_base != AkLoopVal_NotLooping);
		pItem->m_LoopingInfo.bIsInfinite = (m_LoopRanged.m_base == AkLoopVal_Infinite);
		pItem->m_LoopingInfo.lLoopCount = AkLoopVal_NotLooping;

		if (pItem->m_LoopingInfo.bIsEnabled && !pItem->m_LoopingInfo.bIsInfinite)
		{
			AkInt16 loopCount = RandomizerModifier::GetModValue(m_LoopRanged);
			loopCount = AkMax(loopCount, AkLoopVal_NotLooping); //minimum 1, 0 would be infinite looping.
			pItem->m_LoopingInfo.lLoopCount = loopCount;
		}
	}
	return pItem;
}

AKRESULT CAkRanSeqCntr::_PlayTrigger(AkPBIParams& in_rPBIParams)
{
	CAkSmartPtr<CAkParameterNode> spNextSelectedNode;
	CAkSmartPtr<CAkParameterNode> spDelayedNode;

	// WG-42716: Trigger-rate children cannot be played as part of a sample-accurate VPL.
	// They must live in their own VPLs.
	if (in_rPBIParams.sequenceID != AK_INVALID_SEQUENCE_ID)
	{
		in_rPBIParams.sequenceID = AK_INVALID_SEQUENCE_ID;
		MONITOR_MSG(AKTEXT("Playing a trigger-rate container as part of a sample-accurate playlist is not supported and will sound out-of-sync."));
	}

	CAkContinueListItem * pItem = NULL;
	AkUniqueID uSelectedNodeID;
	if (in_rPBIParams.delayParams.uNextSelectedNodeID == AK_INVALID_UNIQUE_ID)
	{
		pItem = AddNewContListItem(in_rPBIParams);
		if (!pItem)
			return AK_Fail;

		AkUInt16 wPositionSelected;
		spNextSelectedNode.Attach( (CAkParameterNode*)GetNextToPlayContinuous(in_rPBIParams.pGameObj, wPositionSelected, uSelectedNodeID, pItem->m_pContainerInfo, pItem->m_LoopingInfo) );
		if (spNextSelectedNode)
			in_rPBIParams.playHistory.Add(wPositionSelected, false);
	}
	else
	{
		pItem = &(in_rPBIParams.pContinuousParams->spContList->m_listItems.Last());
		AKASSERT(pItem);
		uSelectedNodeID = in_rPBIParams.delayParams.uNextSelectedNodeID;
		spNextSelectedNode.Attach( (CAkParameterNode*)g_pIndex->GetNodePtrAndAddRef(uSelectedNodeID, AkNodeType_Default) );
		in_rPBIParams.delayParams.uNextSelectedNodeID = AK_INVALID_UNIQUE_ID;
	}

	// We need to keep a smart pointer on spContList because we need it to be set to NULL on the in_rPBIParams before the dry play and restore it after.
	CAkSmartPtr<CAkContinuationList> spLocalRefContList(in_rPBIParams.pContinuousParams->spContList);

	if (spNextSelectedNode)
	{
		AkUniqueID uDelayedSelectedNodeID;
		AkUInt16 wDelayedPosSelected;
		spDelayedNode.Attach((CAkParameterNode*)GetNextToPlayContinuous(in_rPBIParams.pGameObj, wDelayedPosSelected, uDelayedSelectedNodeID, pItem->m_pContainerInfo, pItem->m_LoopingInfo));

		if (spDelayedNode)
		{
			// First trigger a dry playback.
			in_rPBIParams.pContinuousParams->spContList = NULL; // The play we trigger is not allowed to use that, it will be restored right after.
			PlayHistory BackupPlayHistory = in_rPBIParams.playHistory;

			{ // Required bracket, scoping out eResult.
				// Ignore Error Code, Delay is the one to consider the Error sequence.
				AKRESULT eResult = spNextSelectedNode->Play(in_rPBIParams);
				if (eResult == AK_Success)
				{
					in_rPBIParams.bIsFirst = false;
				}
			}

			// RevertPlayStack before making the delay.
			in_rPBIParams.playHistory = BackupPlayHistory;
			in_rPBIParams.playHistory.RemoveLast();
			in_rPBIParams.pContinuousParams->spContList = spLocalRefContList;
			
			// Then Trigger the next delay.
			in_rPBIParams.delayParams.uNextSelectedNodeID = uDelayedSelectedNodeID;
			in_rPBIParams.playHistory.Add(wDelayedPosSelected, false);
			AkReal32 fTransitionTimeInSeconds = TransitionTime( AkRTPCKey( in_rPBIParams.pGameObj, in_rPBIParams.userParams.PlayingID() ) ) / 1000.f;
			if (fTransitionTimeInSeconds < g_kMinimumTriggerRateDelay)
			{
				fTransitionTimeInSeconds = g_kMinimumTriggerRateDelay;
			}
			// Unset transition parameters; these only apply to the first sound
			in_rPBIParams.pTransitionParameters = NULL;
			return DelayPlayback(fTransitionTimeInSeconds, in_rPBIParams);
		}
		else
		{
			// Last Playback.
			in_rPBIParams.pContinuousParams->spContList->m_listItems.RemoveLast();
			return spNextSelectedNode->Play(in_rPBIParams);
		}
	}

	AKASSERT(pItem);//It must either have retrieved the existing one or created a new one before getting here.
	in_rPBIParams.pContinuousParams->spContList->m_listItems.RemoveLast();

	MONITOR_ERROR_PARAM(AK::Monitor::ErrorCode_SelectedChildNotAvailable, uSelectedNodeID, in_rPBIParams.userParams.PlayingID(), in_rPBIParams.pGameObj->ID(), this->ID(), false);
	return  PlayAndContinueAlternate(in_rPBIParams);
}

CAkPBI* CAkRanSeqCntr::CreatePBI( CAkSoundBase*					in_pSound,
                                  CAkSource*					in_pSource,
                                  AkPBIParams&					in_rPBIParams,
                                  const PriorityInfoCurrent&	in_rPriority
								  ) const
{
    if ( in_rPBIParams.eType == AkPBIParams::ContinuousPBI )
    {
        return AkNew( AkMemID_Object, CAkContinuousPBI( in_rPBIParams,
                                                                  in_pSound,
                                                                  in_pSource,
                                                                  in_rPriority
																  ) );
    }

    return CAkPBIAware::CreatePBI( in_pSound, in_pSource, in_rPBIParams, in_rPriority );
    
}

AKRESULT CAkRanSeqCntr::SerializeHistory(
	AK::IWriteBytes * in_pBytes
	)
{
	AkUInt32 uHistoryItemCount;
	if (IsGlobal())
		uHistoryItemCount = m_pGlobalContainerInfo ? 1 : 0;
	else
		uHistoryItemCount = m_mapObjectCntrInfo.Length();

	if (!in_pBytes->Write<AkUInt8>(m_eMode)
		|| !in_pBytes->Write<AkUInt32>(uHistoryItemCount))
		return AK_Fail;

	AkUInt16 uPlayListLength = m_pPlayList ? (AkUInt16)m_pPlayList->Length() : 0;

	if (m_pGlobalContainerInfo)
	{
		if (!in_pBytes->Write<AkWwiseGameObjectID>(WWISE_INVALID_GAME_OBJECT))
			return AK_Fail;

		AKRESULT res = m_pGlobalContainerInfo->SerializeHistory(in_pBytes, uPlayListLength);
		if (res != AK_Success)
			return res;
	}
	else
	{
		for (AkMapObjectCntrInfo::Iterator it = m_mapObjectCntrInfo.Begin(); it != m_mapObjectCntrInfo.End(); ++it)
		{
			if (!in_pBytes->Write<AkWwiseGameObjectID>(it.pItem->key->ID()))
				return AK_Fail;

			AKRESULT res = it.pItem->pInfo->SerializeHistory(in_pBytes, uPlayListLength);
			if (res != AK_Success)
				return res;
		}
	}

	return AK_Success;
}

AKRESULT CAkRanSeqCntr::DeserializeHistory(
	AK::IReadBytes * in_pBytes
	)
{
	AkUInt32 uHistoryItemCount = 0;
	AkUInt8 eMode = 0;
	if (!in_pBytes->Read<AkUInt8>(eMode)
		|| !in_pBytes->Read<AkUInt32>(uHistoryItemCount))
		return AK_Fail;

	if (eMode != m_eMode)
		return AK_Fail;

	AkUInt16 uPlayListLength = m_pPlayList ? (AkUInt16) m_pPlayList->Length() : 0;

	for (AkUInt32 i = 0; i < uHistoryItemCount; ++i)
	{
		AkWwiseGameObjectID gameObject = WWISE_INVALID_GAME_OBJECT;
		if (!in_pBytes->Read<AkWwiseGameObjectID>(gameObject))
			return AK_Fail;

		CAkContainerBaseInfo * pInfo = NULL;

		CAkSmartPtr<CAkRegisteredObj> pGameObj;
		if (gameObject != WWISE_INVALID_GAME_OBJECT)
		{
			pGameObj.Attach(g_pRegistryMgr->GetObjAndAddref((AkGameObjectID)gameObject));
			if (!pGameObj)
			{
				// Game object not present: deserialize into temp object and throw the data away.

				if (m_eMode == ContainerMode_Sequence)
					pInfo = CreateSequenceInfo();
				else
					pInfo = CreateRandomInfo((AkUInt16)uPlayListLength);

				if (!pInfo)
					return AK_Fail;

				AKRESULT res = pInfo->DeserializeHistory(in_pBytes, uPlayListLength);

				AkDelete(AkMemID_Object, pInfo);
				
				if (res != AK_Success)
					return AK_Fail;

				continue;
			}
		}

		if (m_eMode == ContainerMode_Sequence)
			pInfo = GetExistingSequenceInfo(pGameObj);
		else
			pInfo = GetExistingRandomInfo((AkUInt16)m_pPlayList->Length(), pGameObj);

		if (!pInfo)
			return AK_Fail;

		if (pInfo->DeserializeHistory(in_pBytes, uPlayListLength) != AK_Success)
			return AK_Fail;
	}

	return AK_Success;
}

void CAkRanSeqCntr::UpdateResetPlayListSetup(CAkSequenceInfo* in_pSeqInfo, CAkRegisteredObj * in_pGameObj)
{
	AKASSERT(m_eMode == ContainerMode_Sequence);
	AKASSERT(in_pSeqInfo);
	CAkSequenceInfo* pSequenceInfo;
	if(IsGlobal())
	{
		AKASSERT(m_pGlobalContainerInfo);
		pSequenceInfo = static_cast<CAkSequenceInfo*>(m_pGlobalContainerInfo);
	}
	else//per object
	{
		CntrInfoEntry* pEntry = m_mapObjectCntrInfo.Exists(in_pGameObj);
		AKASSERT( pEntry );
		pSequenceInfo = static_cast<CAkSequenceInfo*>( pEntry->pInfo );
	}
	pSequenceInfo->m_bIsForward = in_pSeqInfo->m_bIsForward;

	// Always put the last one as the last selected, since the lastselected is never 
	// playing yet unless in crossfade.
	if(pSequenceInfo->m_bIsForward)
	{
		pSequenceInfo->m_i16LastPositionChosen = in_pSeqInfo->m_i16LastPositionChosen-1;
	}
	else
	{
		pSequenceInfo->m_i16LastPositionChosen = in_pSeqInfo->m_i16LastPositionChosen+1;
	}

	// Reset the continuous mode if reached the end of the list, otherwise, 
	// the next play will conclude that there is nothing more to play
	if(pSequenceInfo->m_i16LastPositionChosen + 1 == m_pPlayList->Length() && !m_bIsRestartBackward)
	{
		pSequenceInfo->m_i16LastPositionChosen = -1;
	}
	else if(pSequenceInfo->m_i16LastPositionChosen == 0 && !(pSequenceInfo->m_bIsForward) )
	{
		pSequenceInfo->m_bIsForward = true;
	}
}

#ifndef AK_OPTIMIZED

void CAkRanSeqCntr::ForceNextToPlay(AkInt16 in_iPosition, CAkRegisteredObj * in_pGameObj/*=NULL*/,AkPlayingID in_PlayingID/*= NO_PLAYING_ID*/)
{
	AKASSERT(g_pPlayingMgr);
	AKASSERT(in_iPosition >= 0);
	if((in_iPosition < static_cast<AkInt16>(m_pPlayList->Length())) && (m_eMode == ContainerMode_Sequence))
	{
		if(!m_bIsContinuous)
		{
			CAkSequenceInfo* pSeqInfo = GetExistingSequenceInfo( in_pGameObj );
			if( !pSeqInfo )
				return;

			pSeqInfo->m_i16LastPositionChosen = in_iPosition-1;
			pSeqInfo->m_bIsForward = true;
		}
		else if( in_PlayingID && g_pPlayingMgr->IsActive(in_PlayingID) )//It is continuous
		{
			CAkContinueListItem item;

			item.m_pContainer = this;

			item.m_LoopingInfo.bIsEnabled = ( m_LoopRanged.m_base != AkLoopVal_NotLooping );
			item.m_LoopingInfo.bIsInfinite = ( m_LoopRanged.m_base == AkLoopVal_Infinite );
				
			item.m_LoopingInfo.lLoopCount = AkLoopVal_NotLooping;

			CAkSequenceInfo* pSeqInfo = CreateSequenceInfo();
			if( !pSeqInfo )
				return;

			pSeqInfo->m_bIsForward = true;
			pSeqInfo->m_i16LastPositionChosen = in_iPosition;
			item.m_pContainerInfo = pSeqInfo;

			g_pPlayingMgr->StopAndContinue( in_PlayingID, in_pGameObj, item, m_pPlayList->ID( in_iPosition ), in_iPosition, this );

			item.m_pContainerInfo = NULL; // IMPORTANT: because we cheat and use a CAkContinueListItem on the stack,
										  // avoid destruction of m_pContainerInfo in ~CAkContinueListItem.
		}
		else//It is continuous
		{
			if(!m_pGlobalContainerInfo)
			{
				m_pGlobalContainerInfo =  CreateSequenceInfo();
				if( !m_pGlobalContainerInfo )
					return;
			}
			CAkSequenceInfo* pSeqInfo = static_cast<CAkSequenceInfo*>(m_pGlobalContainerInfo);
			pSeqInfo->m_i16LastPositionChosen = in_iPosition-1;
			pSeqInfo->m_bIsForward = true;
		}
	}
}

AkInt16 CAkRanSeqCntr::NextToPlay(CAkRegisteredObj * in_pGameObj/*=NULL*/)
{
	CAkSequenceInfo* pSeqInfo = NULL;
	AkInt16 i16Next = 0;
	if(m_eMode == ContainerMode_Sequence && m_pPlayList->Length() > 1)
	{			
		if(!m_bIsContinuous)
		{
			if(IsGlobal())
			{
				if(m_pGlobalContainerInfo)
				{
					pSeqInfo = static_cast<CAkSequenceInfo*>(m_pGlobalContainerInfo);
				}
			}
			else
			{
				CntrInfoEntry * pEntry = m_mapObjectCntrInfo.Exists(in_pGameObj);
				if( pEntry )
				{
					pSeqInfo = static_cast<CAkSequenceInfo*>( pEntry->pInfo );
				}
			}
		}
		else// Continuous
		{
			if( m_pGlobalContainerInfo )
			{
				pSeqInfo = static_cast<CAkSequenceInfo*>(m_pGlobalContainerInfo);
			}
		}
		if(pSeqInfo)
		{
			if(pSeqInfo->m_bIsForward)
			{
				if(pSeqInfo->m_i16LastPositionChosen >= static_cast<AkInt16>(m_pPlayList->Length()-1))
				{
					if(m_bIsRestartBackward)
					{
						i16Next = pSeqInfo->m_i16LastPositionChosen - 1;
					}
				}
				else
				{
					i16Next = pSeqInfo->m_i16LastPositionChosen+1;
				}
			}
			else
			{
				AKASSERT(pSeqInfo->m_i16LastPositionChosen >=0);
				if(pSeqInfo->m_i16LastPositionChosen > 0)
				{
					i16Next = pSeqInfo->m_i16LastPositionChosen - 1;
				}
				else
				{
					i16Next = 1;
				}
			}
		}
	}
	return i16Next;
}
#endif // AK_OPTIMIZED
