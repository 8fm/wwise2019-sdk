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
// AkRSIterator.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkRSIterator.h"
#include "AkMusicRanSeqCntr.h"
#include "AkPlayingMgr.h"
#include <AK/SoundEngine/Common/AkBytesMem.h>
#include <AK/SoundEngine/Common/AkBytesCount.h>

///////////////
// CAkRSNode Class
//////////////
AkInt16 CAkRSNode::GetInitialLoopCountLoop()
{
	AkInt16 loopCount = m_Loop;
	if( m_Loop != 0 && ( m_LoopMin || m_LoopMax ) )// If randomization is required.
	{
		AkInt16 minimal = AkMax( 1, m_Loop + m_LoopMin );
		AkInt16 maximal = m_Loop + m_LoopMax;
		if( minimal != maximal )
		{
			loopCount = minimal + ( AKRANDOM::AkRandom() % (maximal-minimal+1) );
		}
	}
	return loopCount;
}

///////////////
// CAkRSSub Class
//////////////

CAkRSSub::~CAkRSSub()
{
	for( AkRSList::Iterator iter = m_listChildren.Begin(); iter != m_listChildren.End(); ++iter )
	{
		if( (*iter) )
		{
			AkDelete(AkMemID_Structure, (*iter));
		}
	}
	m_listChildren.Term();
}

void CAkRSSub::WasSegmentLeafFound()
{
	if ( !m_bHasSegmentLeaves && Parent() )
	{
		AKASSERT( !Parent()->IsSegment() );
		static_cast<CAkRSSub*>(Parent())->WasSegmentLeafFound();
	}
	m_bHasSegmentLeaves = true;
}

AkRandomMode CAkRSSub::RandomMode()
{
	return m_bIsShuffle ? RandomMode_Shuffle : RandomMode_Normal;
}

AkUInt32 CAkRSSub::CalculateTotalWeight()
{
	AkUInt32 TotalWeigth = 0;
	for( AkRSList::Iterator iter = m_listChildren.Begin(); iter != m_listChildren.End(); ++iter )
	{
		TotalWeigth += (*iter)->GetWeight();
	}
	return TotalWeigth;
}

//////////////////////////////
// RSStackItem
//////////////////////////////
AKRESULT RSStackItem::Init( CAkRSSub * in_pSub, AkRSIterator * in_pRSit )
{
	m_pRSSub = in_pSub;
	m_Loop.bIsInfinite = ( in_pSub->GetLoop() == 0 ); 
	m_Loop.bIsEnabled = true;
	m_Loop.lLoopCount = in_pSub->GetInitialLoopCountLoop();

	if ( IsGlobal() )
	{
		m_pLocalRSInfo = in_pRSit->GetGlobalRSInfo( in_pSub );
		m_Loop.bIsInfinite = true;
	}
	else
		m_pLocalRSInfo = AkRSIterator::CreateRSInfo( in_pSub );

	return m_pLocalRSInfo ? AK_Success : AK_Fail;
}

void RSStackItem::Clear()
{
	if ( m_pLocalRSInfo && !IsGlobal() )
		m_pLocalRSInfo->Destroy();
	m_pLocalRSInfo = NULL;
}

AKRESULT RSStackItem::Serialize( AK::IWriteBytes* in_writer )
{
	AkUniqueID playlistId = m_pRSSub->PlaylistID();
	RSType rsType = m_pRSSub->GetType();

	if ( !in_writer->Write<AkUniqueID>( playlistId )
		|| !in_writer->Write<AkUInt8>( (AkUInt8)rsType )
		)
		return AK_Fail;

	// Pack RSInfo if NOT global
	if ( !IsGlobal( rsType ) )
	{
		if ( !in_writer->Write<AkUInt16>( (AkUInt16)m_pRSSub->GetChildren().Length() ) )
			return AK_Fail;
		if ( AK_Success != m_pLocalRSInfo->SerializeHistory( in_writer, (AkUInt16)m_pRSSub->GetChildren().Length() ) )
			return AK_Fail;
	}

	// Serialize loop info
	if ( AK_Success != m_Loop.Serialize( in_writer ) )
		return AK_Fail;

	return AK_Success;
}

AKRESULT RSStackItem::Deserialize( AK::IReadBytes* in_reader, CAkRSSub*& io_pRSSub, AkRSIterator& in_rsIt )
{
	AkUniqueID playlistId;
	AkUInt8 rsType;

	if ( !in_reader->Read<AkUniqueID>( playlistId )
		|| !in_reader->Read<AkUInt8>( rsType )
		)
		return AK_Fail;

	// Find associated RSSub
	if ( io_pRSSub->PlaylistID() != playlistId )
	{
		const AkRSList& children = io_pRSSub->GetChildren();
		AkRSList::Iterator it = children.Begin();
		for ( ; it != children.End(); ++it )
		{
			if ( (*it)->PlaylistID() == playlistId )
				break;
		}
		if ( it == children.End() )
			return AK_Fail;

		io_pRSSub = static_cast<CAkRSSub*>( *it );
	}

	// Remember our associated RSSub
	m_pRSSub = io_pRSSub;
	AKASSERT( m_pRSSub->GetType() == rsType );

	// Unpack RSInfo if NOT global
	if ( !IsGlobal( (RSType)rsType ) )
	{
		AkUInt16 numItems;
		if ( !in_reader->Read<AkUInt16>( numItems ) )
			return AK_Fail;

		// Gonna create the RSInfo without knowing the avoid count; it's ok, the deserialize will take care of it
		CAkContainerBaseInfo* rsInfo = AkRSIterator::CreateRSInfo( (RSType)rsType, numItems, 0 );
		if ( NULL == rsInfo )
			return AK_Fail;

		if ( AK_Success != rsInfo->DeserializeHistory( in_reader, numItems ) )
			return AK_Fail;

		m_pLocalRSInfo = rsInfo;
	}
	else
	{
		// RSInfo should now be in global map
		m_pLocalRSInfo = in_rsIt.FindGlobalRSInfo( playlistId );
		if ( NULL == m_pLocalRSInfo )
			return AK_Fail;
	}

	// Serialize loop info
	if ( AK_Success != m_Loop.Deserialize( in_reader ) )
		return AK_Fail;

	return AK_Success;
}

///////////////
//AkRSIterator Class
//////////////
AkRSIterator::AkRSIterator()
	:m_pRSCntr( NULL )
	,m_playingID( AK_INVALID_UNIQUE_ID )
	,m_actualSegment( AK_INVALID_UNIQUE_ID )
	,m_actualPlaylistID( AK_INVALID_UNIQUE_ID )
	,m_bIsSegmentValid( false )
	,m_uSegmentLoopCount( 0 )
{}

AkRSIterator::~AkRSIterator()
{}

AKRESULT AkRSIterator::Init( CAkMusicRanSeqCntr* in_pRSCntr, AkPlayingID in_playingID, const CAkMusicPlaybackHistory* in_history )
{
	// Extract global info from provided history
	if ( in_history && in_history->m_playlistHistory.Size() != 0 )
		UnpackGlobalRSInfo( in_history->m_playlistHistory );

	m_pRSCntr = in_pRSCntr;
	m_playingID = in_playingID;
	m_actualSegment = AK_MUSIC_TRANSITION_RULE_ID_NONE;
	m_bIsSegmentValid = true;

	CAkRSNode* pNode = m_pRSCntr->GetPlaylistRoot();
	while( pNode && !pNode->IsSegment() )
	{
		CAkRSSub* pSub = static_cast<CAkRSSub*>(pNode);
		if( !pSub->m_listChildren.Length()
			|| !pSub->HasSegmentLeaves() )
		{
			// The list item is empty, we Go back to parent and we won't stack it again.
			pNode = pNode->Parent();
			pSub = static_cast<CAkRSSub*>(pNode);
			if( !pNode )
				break;// Apparently nothing playable, keep AK_MUSIC_TRANSITION_RULE_ID_NONE
		}
		else if( StackItem( pSub ) != AK_Success )
		{
			Term();
			return AK_Fail;
		}

		bool bIsEnd = false;
		do
		{
			AkUInt16 newIndex = Select( m_stack.Last(), bIsEnd );
			
			if( !bIsEnd )
			{
				pNode = ( pSub->m_listChildren )[newIndex];
			}
			else
			{
				// We must go back one step, and take back the parent as curent node, if there is no parent, there is nothing to play.
				// If we are in step mode, no next.
				pNode = pSub->Parent();
				PopLast();
			}
		}while( bIsEnd && pNode );
	}

	return SetCurrentSegmentToNode( pNode );
}

// Call when no more JumpTo will be called.
// Flushes the map of original global RanInfo that were saved.
void AkRSIterator::EndInit()
{}


void AkRSIterator::Term()
{	
	FlushStack();
	m_stack.Term();

	FlushGlobalRSInfo();
	m_globalRSInfo.Term();

	m_history.Term();

	GlobalRSInfoMap::Iterator it = m_globalRSInfo.Begin();
	while ( it != m_globalRSInfo.End() )
	{
		AKASSERT( (*it).item.pRSInfo );
		(*it).item.pRSInfo->Destroy();
		++it;
	}
	m_globalRSInfo.Term();
}

void AkRSIterator::FlushStack()
{
	for( IteratorStack::Iterator iter = m_stack.Begin(); iter != m_stack.End(); ++iter )
	{
		(*iter).Clear();
	}
	m_stack.RemoveAll();
}

void AkRSIterator::FlushGlobalRSInfo()
{
	for ( GlobalRSInfoMap::Iterator it = m_globalRSInfo.Begin(); it != m_globalRSInfo.End(); ++it )
	{
		(*it).item.pRSInfo->Destroy();
	}
	m_globalRSInfo.RemoveAll();
}

void AkRSIterator::PopLast()
{	
	RSStackItem & l_Item = m_stack.Last();
	l_Item.Clear();
	m_stack.RemoveLast();
}

void AkRSIterator::JumpNext()
{
	if( m_uSegmentLoopCount > 1 )
	{
		--m_uSegmentLoopCount;
		return;
	}
	if( m_uSegmentLoopCount == 0 )//Infinite means we stay stuck at this position until something happpens.
	{
		return;
	}
	if( m_actualSegment == AK_MUSIC_TRANSITION_RULE_ID_NONE )
	{
		m_bIsSegmentValid = false;
		return;
	}
	m_actualSegment = AK_MUSIC_TRANSITION_RULE_ID_NONE;
	m_bIsSegmentValid = true;

	CAkRSNode* pNode = NULL;

	if( !m_stack.IsEmpty() )
		pNode = m_stack.Last().m_pRSSub;
	else
		return;

	bool bIsEnd = true;

	pNode = PopObsoleteStackedItems( pNode );

	while( bIsEnd && pNode )
	{
		AkUInt16 newIndex = Select( m_stack.Last(), bIsEnd );
		
		if( !bIsEnd )
		{
			pNode = ( static_cast<CAkRSSub*>(pNode)->m_listChildren )[newIndex];
			AKASSERT( pNode );
			if( pNode->IsSegment() )
			{
				break;
			}
			else 
			{
				CAkRSSub* pSub = static_cast<CAkRSSub*>(pNode);
				if( !pSub->m_listChildren.Length()
					|| !pSub->HasSegmentLeaves() )
				{
					pNode = pNode->Parent();
				}
				else if( StackItem( pSub ) != AK_Success )
				{
					Term();
					return;
				}
			}
			bIsEnd = true; // Reset since we are on a new node
		}
		else
		{
			// We must go back one step, and take back the parent as curent node, if there is no parent, there is nothing to play.
			// If we are in step mode, no next.
			pNode = static_cast<CAkRSSub*>(pNode)->Parent();
			PopLast();
			pNode = PopObsoleteStackedItems( pNode );
		}
	}

	SetCurrentSegmentToNode( pNode );
}

AKRESULT AkRSIterator::JumpNextInternal()
{
	m_actualSegment = AK_MUSIC_TRANSITION_RULE_ID_NONE;
	m_bIsSegmentValid = true;

	CAkRSNode* pNode = NULL;

	if( !m_stack.IsEmpty() )
		pNode = m_stack.Last().m_pRSSub;
	else
		return AK_Fail;

	bool bIsEnd = true;

	while( bIsEnd && pNode )
	{
		AkUInt16 newIndex = Select( m_stack.Last(), bIsEnd );
		
		if( !bIsEnd )
		{
			pNode = ( static_cast<CAkRSSub*>(pNode)->m_listChildren )[newIndex];
			AKASSERT( pNode );
			if( pNode->IsSegment() )
			{
				break;
			}
			else if( StackItem( static_cast<CAkRSSub*>(pNode) ) != AK_Success )
			{
				Term();
				return AK_Fail;
			}
			bIsEnd = true; // Reset since we are on a new node
		}
		else
		{
			// We must go back one step, and take back the parent as curent node, if there is no parent, there is nothing to play.
			// If we are in step mode, no next.
			pNode = static_cast<CAkRSSub*>(pNode)->Parent();
			PopLast();
			pNode = PopObsoleteStackedItems( pNode );
		}
	}

	return SetCurrentSegmentToNode( pNode );
}

AKRESULT AkRSIterator::JumpTo( AkUniqueID in_playlistElementID, const CAkMusicPlaybackHistory* in_pHistory )
{
	FlushStack();
	FlushGlobalRSInfo();

	if ( in_pHistory && in_pHistory->m_playlistHistory.Size() != 0 )
		UnpackGlobalRSInfo( in_pHistory->m_playlistHistory );

	m_actualSegment = AK_MUSIC_TRANSITION_RULE_ID_NONE;
	m_bIsSegmentValid = true;

	JumpToList jumpList;

	bool bFound = false;
	
	AKRESULT eResult = FindAndSelect( m_pRSCntr->GetPlaylistRoot(), in_playlistElementID, jumpList, bFound );

	if( bFound && eResult == AK_Success )
	{
		CAkRSNode* pNode = NULL;
		JumpToList::Iterator iter = jumpList.Begin();
		while( iter != jumpList.End() )
		{
			pNode = *iter;
			AKASSERT( pNode );
			if( pNode->IsSegment() )
			{
				break;
			}
			else
			{
				CAkRSSub* pSub = static_cast<CAkRSSub*>(pNode);

				if( StackItem( pSub ) != AK_Success )
				{
					Term();
					eResult = AK_Fail;
					break;
				}
				++iter;//We increment before the end of the loop since we will need the next iterator in the curent loop.

				if( iter != jumpList.End() )
				{
					// We will simulate a fisrt selection on the next item
					ForceSelect( *iter );
				}
				else
				{
					// The targeted item was a Sub
					eResult = JumpNextInternal();
					jumpList.Term();
					return eResult;
				}
			}
		}
		
		if( eResult == AK_Success )
		{
			eResult = SetCurrentSegmentToNode( pNode );
		}
	}
	else
	{
		eResult = AK_Fail;
	}

	jumpList.Term();

	return eResult;
}

AKRESULT AkRSIterator::FindAndSelect( 
						CAkRSNode* in_pNode, 
						AkUniqueID in_playlistElementID, 
						JumpToList& io_jmpList, 
						bool& io_bFound 
						)
{
	AKRESULT eResult = AK_Success;
	if( !io_jmpList.AddLast( in_pNode ) )
	{
		return AK_Fail;
	}
	else if( in_pNode->PlaylistID() == in_playlistElementID )
	{
		io_bFound = true;
	}
	else if( !in_pNode->IsSegment() )
	{
		// check on children
		CAkRSSub* pSub = static_cast<CAkRSSub*>( in_pNode );
		AkRSList::Iterator iter = pSub->m_listChildren.Begin();
		while( !io_bFound && iter != pSub->m_listChildren.End() )
		{
			eResult = FindAndSelect( *iter, in_playlistElementID, io_jmpList, io_bFound );
			if( eResult != AK_Success )
				return eResult;
			++iter;
		}
	}
	if( !io_bFound )
	{
		io_jmpList.RemoveLast();
	}
	return eResult;
}

CAkRSNode* AkRSIterator::PopObsoleteStackedItems( CAkRSNode* in_pNode )
{
	while( in_pNode && !static_cast<CAkRSSub*>(in_pNode)->IsContinuous() )
	{
		if( m_stack.Last().m_Loop.lLoopCount == 0 ) // Step looping infinitely, we keep it
		{
			break;
		}
		else if( m_stack.Last().m_Loop.lLoopCount > 1 )
		{
			--(m_stack.Last().m_Loop.lLoopCount);
			break;
		}
		else
		{
			in_pNode = static_cast<CAkRSSub*>(in_pNode)->Parent();
			PopLast();
		}
	}
	return in_pNode;
}

AKRESULT AkRSIterator::StackItem( CAkRSSub* in_pSub )
{ 
	RSStackItem item;
	if ( item.Init( in_pSub, this ) != AK_Success
		|| !m_stack.AddLast( item ) )
	{
		item.Clear();
		return AK_Fail;
	}
	return AK_Success;
}

AkUInt16 AkRSIterator::Select( RSStackItem & in_rStackItem, bool & out_bIsEnd )
{
	AkUInt16 newIndex = 0;

	CAkRSSub* pSub = in_rStackItem.m_pRSSub;

	switch( pSub->GetType() )
	{
	case RSType_ContinuousRandom:
	case RSType_StepRandom:
		newIndex = SelectRandomly( in_rStackItem, out_bIsEnd );
		break;
	case RSType_ContinuousSequence:
	case RSType_StepSequence:
		newIndex = SelectSequentially( in_rStackItem, out_bIsEnd );
		break;

	default:
		AKASSERT( !"Unhandled RSType" );
		break;
	}

	if ( pSub && !out_bIsEnd )
	{
		AkUInt32 numPlaylistItems = pSub->m_listChildren.Length();
		AkUInt32 uSelection = newIndex, uItemDone = out_bIsEnd;
		g_pPlayingMgr->MusicPlaylistCallback( m_playingID, pSub->PlaylistID(), numPlaylistItems, uSelection, uItemDone );
		if ( uSelection < numPlaylistItems )
			newIndex = (AkUInt16)uSelection;
		out_bIsEnd = ( uItemDone != 0 );
	}

	return newIndex;
}

void AkRSIterator::ForceSelect( CAkRSNode* in_pForcedNode )
{
	switch( static_cast<CAkRSSub*>( in_pForcedNode->Parent() )->GetType() )
	{
	case RSType_ContinuousRandom:
	case RSType_StepRandom:
		ForceSelectRandomly( in_pForcedNode );
		break;
	case RSType_ContinuousSequence:
	case RSType_StepSequence:
		ForceSelectSequentially( in_pForcedNode );
		break;

	default:
		AKASSERT( !"Unhandled RSType" );
		break;
	}
}

AkUInt16 AkRSIterator::SelectSequentially( RSStackItem & in_rStackItem, bool & out_bIsEnd )
{
	out_bIsEnd = false;
	CAkSequenceInfo* pSeqInfo = static_cast<CAkSequenceInfo*>( in_rStackItem.GetRSInfo() );
	if ( !pSeqInfo )
	{
		out_bIsEnd = true;
		return 0;
	}

	// Save a copy of the RS info if required.
	CAkRSSub* pSub = in_rStackItem.m_pRSSub;

	if( ( pSeqInfo->m_i16LastPositionChosen + 1 ) == pSub->m_listChildren.Length() )//reached the end of the sequence
	{
		pSeqInfo->m_i16LastPositionChosen = 0;
		if(!CanContinueAfterCompleteLoop( in_rStackItem.m_Loop ))
		{
			out_bIsEnd = true;
			return 0;
		}
	}
	else//not finished sequence
	{
		++( pSeqInfo->m_i16LastPositionChosen );
	}

	return pSeqInfo->m_i16LastPositionChosen;
}

AkUInt16 AkRSIterator::SelectRandomly( RSStackItem & in_rStackItem, bool & out_bIsEnd )
{
	out_bIsEnd = false;
	CAkRandomInfo* pRanInfo = static_cast<CAkRandomInfo*>( in_rStackItem.GetRSInfo() );
	if ( !pRanInfo )
	{
		out_bIsEnd = true;
		return 0;
	}

	AkUInt16 wPosition = 0;
	int iValidCount = -1;
	int iCycleCount = 0;

	CAkRSSub* pSub = in_rStackItem.m_pRSSub;
	AkRSList* pRSList = &( pSub->m_listChildren );

	if(!pRanInfo->m_wCounter)
	{
		if(!CanContinueAfterCompleteLoop( in_rStackItem.m_Loop ))
		{
			out_bIsEnd = true;
			return 0;
		}
		pRanInfo->m_wCounter = (AkUInt16)pRSList->Length();
		pRanInfo->ResetFlagsPlayed(pRSList->Length());

		pRanInfo->m_ulRemainingWeight = pRanInfo->m_ulTotalWeight;
		for( CAkRandomInfo::AkAvoidList::Iterator iter = pRanInfo->m_listAvoid.Begin(); iter != pRanInfo->m_listAvoid.End(); ++iter )
		{
			pRanInfo->m_ulRemainingWeight -= ((*pRSList)[*iter])->GetWeight();
		}
		pRanInfo->m_wRemainingItemsToPlay -= (AkUInt16)pRanInfo->m_listAvoid.Length();
	}

	AKASSERT(pRanInfo->m_wRemainingItemsToPlay);//Should never be here if empty...

	if( pSub->IsUsingWeight() )
	{
		if( pRanInfo->m_ulRemainingWeight == 0 )
		{
			// This is and If and Assert Situation.
			// The reason: This crashed once on a divide by 0, but apparently no repro, and the callstack 
			// does not help identifying the problem.
			// Just in case it happens again, don't crash.
			AKASSERT( pRanInfo->m_ulRemainingWeight != 0 );
			pRanInfo->m_wCounter = 0; //Force reset next time
			return 0;
		}
		
		int iRandomValue = pRanInfo->GetRandomValue();
		while(iValidCount < iRandomValue)
		{
			if(CanPlayPosition( pSub, pRanInfo, iCycleCount ))
			{
				iValidCount += ((*pRSList)[iCycleCount])->GetWeight();
			}
			++iCycleCount;
			AKASSERT(((size_t)(iCycleCount-1)) < pRSList->Length());
		}
	}
	else
	{
		if( pRanInfo->m_wRemainingItemsToPlay == 0 )
		{
			// This is an If and Assert Situation.
			// The reason: This crashed once on a divide by 0, but apparently no repro, and the callstack
			// does not help identifying the problem.
			// Just in case it happens again, don't crash.
			AKASSERT( pRanInfo->m_wRemainingItemsToPlay != 0 );
			pRanInfo->m_wCounter = 0; //Force reset next time
			return 0;
		}

		int iRandomValue = (AkUInt16)(AKRANDOM::AkRandom() % pRanInfo->m_wRemainingItemsToPlay);
		while(iValidCount < iRandomValue)
		{
			if( CanPlayPosition( pSub, pRanInfo, iCycleCount ) )
			{
				++iValidCount;
			}
			++iCycleCount;
			AKASSERT(((size_t)(iCycleCount-1)) < pRSList->Length());
		}
	}
	wPosition = iCycleCount - 1;
	
	UpdateRandomItem( pSub, wPosition, pRSList, pRanInfo );

	return wPosition;
}

void AkRSIterator::ForceSelectRandomly( CAkRSNode* in_pForcedNode )
{
	CAkRSSub* pSub = static_cast<CAkRSSub*>( in_pForcedNode->Parent() );
	AkUInt16 wPosition = 0;
	for( AkRSList::Iterator iter = pSub->m_listChildren.Begin(); iter != pSub->m_listChildren.End(); ++iter )
	{
		if( *iter == in_pForcedNode )
		{
			break;
		}
		++wPosition;
	}

	CAkRandomInfo* pRanInfo = static_cast<CAkRandomInfo*>( m_stack.Last().GetRSInfo() );
	if ( !pRanInfo )
		return;	// Error failed to get RS info (out-of-memory).

	AkRSList* pRSList = &( pSub->m_listChildren );


	// Reset random info:
	// "Forcing Random" is arguably not legal: nothing prevents the caller
	// from selecting an node that should not have been selected through the
	// shuffle logic. The only thing we can do is to reset it...
	// But let's keep the avoid list, though.
	
	// Remove from Avoid List if applicable.
	pRanInfo->FlagAsUnBlocked( wPosition );
	pRanInfo->m_listAvoid.Remove( wPosition );
	
	pRanInfo->m_wCounter = (AkUInt16)pRSList->Length();
	pRanInfo->ResetFlagsPlayed(pRSList->Length());

	pRanInfo->m_ulRemainingWeight = pRanInfo->m_ulTotalWeight;
	for( CAkRandomInfo::AkAvoidList::Iterator iter = pRanInfo->m_listAvoid.Begin(); iter != pRanInfo->m_listAvoid.End(); ++iter )
	{
		pRanInfo->m_ulRemainingWeight -= ((*pRSList)[*iter])->GetWeight();
	}
	pRanInfo->m_wRemainingItemsToPlay -= (AkUInt16)pRanInfo->m_listAvoid.Length();
	
	UpdateRandomItem( pSub, wPosition, pRSList, pRanInfo );
}

void AkRSIterator::UpdateRandomItem( CAkRSSub* in_pSub, AkUInt16 in_wPosition, AkRSList* in_pRSList, CAkRandomInfo* in_pRanInfo )
{
	if( in_pSub->RandomMode() == RandomMode_Normal )//Normal
	{
		if(!in_pRanInfo->IsFlagSetPlayed(in_wPosition))
		{
			in_pRanInfo->FlagSetPlayed( in_wPosition );
			in_pRanInfo->m_wCounter -=1;
		}
		if( in_pSub->AvoidRepeatCount() )
		{
			in_pRanInfo->m_wRemainingItemsToPlay -= 1;
			if( in_pRanInfo->m_listAvoid.AddLast(in_wPosition) == NULL )
			{
				// Reset counter( will force refresh on next play ).
				// and return position 0.
				in_pRanInfo->m_wCounter = 0;
				return;
			}
			in_pRanInfo->FlagAsBlocked( in_wPosition );
			in_pRanInfo->m_ulRemainingWeight -= ((*in_pRSList)[in_wPosition])->GetWeight();
			AkUInt16 uVal = (AkUInt16)( in_pRSList->Length()-1 );
			if(in_pRanInfo->m_listAvoid.Length() > AkMin( in_pSub->AvoidRepeatCount(), uVal ))
			{
				AkUInt16 wToBeRemoved = in_pRanInfo->m_listAvoid[0];
				in_pRanInfo->FlagAsUnBlocked(wToBeRemoved);
				in_pRanInfo->m_ulRemainingWeight += ((*in_pRSList)[wToBeRemoved])->GetWeight();
				in_pRanInfo->m_wRemainingItemsToPlay += 1;
				in_pRanInfo->m_listAvoid.Erase(0);
			}
		}
	}
	else//Shuffle
	{
		AkUInt16 wBlockCount = in_pSub->AvoidRepeatCount() ? in_pSub->AvoidRepeatCount() : 1 ;

		in_pRanInfo->m_ulRemainingWeight -= ((*in_pRSList)[in_wPosition])->GetWeight();
		in_pRanInfo->m_wRemainingItemsToPlay -= 1;
		in_pRanInfo->m_wCounter -=1;
		in_pRanInfo->FlagSetPlayed(in_wPosition);
		if( in_pRanInfo->m_listAvoid.AddLast( in_wPosition ) == NULL )
		{
			// Reset counter( will force refresh on next play ).
			// and return position 0.
			in_pRanInfo->m_wCounter = 0;
			return;
		}
		in_pRanInfo->FlagAsBlocked( in_wPosition );

		AkUInt16 uVal = (AkUInt16)( in_pRSList->Length()-1 );
		if(in_pRanInfo->m_listAvoid.Length() > AkMin( wBlockCount, uVal ))
		{
			AkUInt16 wToBeRemoved = in_pRanInfo->m_listAvoid[0];
			in_pRanInfo->m_listAvoid.Erase(0);
			in_pRanInfo->FlagAsUnBlocked(wToBeRemoved);
			if(!in_pRanInfo->IsFlagSetPlayed(wToBeRemoved))
			{
				in_pRanInfo->m_ulRemainingWeight += ((*in_pRSList)[wToBeRemoved])->GetWeight();
				in_pRanInfo->m_wRemainingItemsToPlay += 1;
			}
		}
	}
}

void AkRSIterator::ForceSelectSequentially( CAkRSNode* in_pForcedNode )
{
	CAkRSSub* pParentSub = static_cast<CAkRSSub*>( in_pForcedNode->Parent() );
	AkUInt16 uIndex = 0;
	for( AkRSList::Iterator iter = pParentSub->m_listChildren.Begin(); iter != pParentSub->m_listChildren.End(); ++iter )
	{
		if( *iter == in_pForcedNode )
		{
			break;
		}
		++uIndex;
	}

	CAkSequenceInfo* pSeqInfo = static_cast<CAkSequenceInfo*>( m_stack.Last().GetRSInfo() );
	if ( !pSeqInfo )
		return;	// Error failed to get RS info (out-of-memory).

	pSeqInfo->m_i16LastPositionChosen = uIndex;
}

bool AkRSIterator::CanContinueAfterCompleteLoop( AkLoop& io_loopingInfo/*= NULL*/ )
{
	bool bAnswer = false;
	if(io_loopingInfo.bIsEnabled)
	{
		if(io_loopingInfo.bIsInfinite)
		{
			bAnswer = true;
		}
		else
		{
			--(io_loopingInfo.lLoopCount);
			bAnswer = (io_loopingInfo.lLoopCount)? true:false; 
		}
	}
	return bAnswer;
}

bool AkRSIterator::CanPlayPosition( CAkRSSub* in_pSub, CAkRandomInfo* in_pRandomInfo, AkUInt16 in_wPosition )
{
	if( in_pSub->RandomMode() == RandomMode_Normal )
	{
		if( in_pSub->AvoidRepeatCount() )
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

AKRESULT AkRSIterator::SetCurrentSegmentToNode( CAkRSNode* in_pNode )
{
	if( in_pNode )
	{
		m_actualSegment = static_cast<CAkRSSegment*>( in_pNode )->GetSegmentID();
		m_bIsSegmentValid = ( m_actualSegment != AK_INVALID_UNIQUE_ID );
		m_actualPlaylistID = in_pNode->PlaylistID();
		m_uSegmentLoopCount = in_pNode->GetInitialLoopCountLoop();
		return AK_Success;
	}
	else
	{
		return AK_Fail;
	}
}



CAkContainerBaseInfo* AkRSIterator::CreateRSInfo( CAkRSSub* in_pSub )
{
	CAkContainerBaseInfo* pRSInfo = NULL;

	AkUInt16 numChildren = (AkUInt16)in_pSub->GetChildren().Length();
	AkUInt16 avoidRepeat = in_pSub->AvoidRepeatCount();

	switch( in_pSub->GetType() )
	{
	case RSType_ContinuousRandom:
	case RSType_StepRandom:
		pRSInfo = CreateRandomInfo( numChildren, avoidRepeat );
		if ( pRSInfo && in_pSub->IsUsingWeight() )
		{
			CAkRandomInfo* pRandomInfo = static_cast<CAkRandomInfo*>( pRSInfo );
			pRandomInfo->m_ulTotalWeight = in_pSub->CalculateTotalWeight();
			pRandomInfo->m_ulRemainingWeight = pRandomInfo->m_ulTotalWeight;
		}
		break;

	case RSType_ContinuousSequence:
	case RSType_StepSequence:
		pRSInfo = CreateSequenceInfo( numChildren );
		break;
	}

	return pRSInfo;
}

CAkContainerBaseInfo* AkRSIterator::CreateRSInfo( RSType in_type, AkUInt16 in_numChildren, AkUInt16 in_avoidRepeat )
{
	CAkContainerBaseInfo* pRSInfo = NULL;

	switch( in_type )
	{
	case RSType_ContinuousRandom:
	case RSType_StepRandom:
		pRSInfo = CreateRandomInfo( in_numChildren, in_avoidRepeat );
		break;

	case RSType_ContinuousSequence:
	case RSType_StepSequence:
		pRSInfo = CreateSequenceInfo( in_numChildren );
		break;
	}

	return pRSInfo;
}

CAkContainerBaseInfo* AkRSIterator::GetGlobalRSInfo( CAkRSSub* in_pSub )
{
	const AkUniqueID playlistId = in_pSub->PlaylistID();

	// First check if we have the info in our global map
	GlobalRSInfoItem* pit = m_globalRSInfo.Exists( playlistId );
	if ( NULL != pit )
		return pit->pRSInfo;

	// We'll have to create our own RSInfo; assume we fail for now.
	CAkContainerBaseInfo* pRSInfo = NULL;

	// Check if the RanSeqCntr has the info.  If so, clone it!
	if ( CAkContainerBaseInfo* pSubRSInfo = m_pRSCntr->GetGlobalRSInfo( playlistId ) )
	{
		pRSInfo = pSubRSInfo->Clone( (AkUInt16)in_pSub->GetChildren().Length() );
	}
	else
	{
		pRSInfo = CreateRSInfo( in_pSub );
	}

	if ( NULL != pRSInfo )
	{
		GlobalRSInfoItem mapItem( pRSInfo, (AkUInt16)in_pSub->GetChildren().Length() );
		if ( !m_globalRSInfo.Set( playlistId, mapItem ) )
		{
			pRSInfo->Destroy();
			pRSInfo = NULL;
		}
	}

	return pRSInfo;
}

CAkContainerBaseInfo* AkRSIterator::FindGlobalRSInfo( AkUniqueID in_playlistId )
{
	if ( GlobalRSInfoItem* mapItem = m_globalRSInfo.Exists( in_playlistId ) )
		return mapItem->pRSInfo;

	return NULL;
}

CAkRandomInfo* AkRSIterator::CreateRandomInfo( AkUInt16 in_numChildren, AkUInt16 in_avoidRepeat )
{
	CAkRandomInfo* pRandomInfo = NULL;

	if( in_numChildren != 0 )
	{
		pRandomInfo = AkNew(AkMemID_Object, CAkRandomInfo( in_numChildren ) );
	}
	if( pRandomInfo && pRandomInfo->Init( in_avoidRepeat ) != AK_Success )
	{
		pRandomInfo->Destroy();
		pRandomInfo = NULL;
	}

	return pRandomInfo;
}

CAkSequenceInfo* AkRSIterator::CreateSequenceInfo( AkUInt16 in_numChildren )
{
	CAkSequenceInfo* pSequenceInfo = NULL;

	if( in_numChildren != 0 )
	{
		pSequenceInfo = AkNew(AkMemID_Object, CAkSequenceInfo() );
	}

	return pSequenceInfo;
}

CAkContainerBaseInfo* AkRSIterator::AllocRSInfo( RSType in_type, AkUInt16 in_numItems )
{
	CAkContainerBaseInfo* pRSInfo = NULL;

	switch( in_type )
	{
	case RSType_ContinuousRandom:
	case RSType_StepRandom:
		pRSInfo = AkNew(AkMemID_Object, CAkRandomInfo( in_numItems ) );
		break;

	case RSType_ContinuousSequence:
	case RSType_StepSequence:
		pRSInfo = AkNew(AkMemID_Object, CAkSequenceInfo() );
		break;

	default:
		AKASSERT( !"Unknown RSType!" );
	}

	return pRSInfo;
}

// Move iterator forward; the current segment will be played, so keep iterator in history
void AkRSIterator::MoveToNextSegment()
{
	// Pack iterator and at to list, before we jump to the next segment
	CAkMusicPackedHistory packedIt = PackIterator();
	m_history.AddLast( packedIt );

	// Jump to next segment
	JumpNext();
}

// Skip iterator forward; the current segment will NOT be played, so DON'T keep iterator in history
void AkRSIterator::SkipToNextSegment()
{
	JumpNext();
}

CAkMusicPackedHistory AkRSIterator::NotifySegmentPlay()
{
	CAkMusicPackedHistory packedIt;

	if ( m_history.Length() > 0 )
	{
		// Use the front of the packed iterator queue to update global RSInfo in RanSeqCntr
		packedIt = *m_history.Begin();
		m_history.Erase( 0 );

		// Update the container's global RS info now that the associated segment has actually played
		AkRSIterator tempIt;
		tempIt.SetRanSeqCntr( m_pRSCntr );
		tempIt.UnpackGlobalRSInfo( packedIt );
		tempIt.PushGlobalRSInfoToCntr();
		tempIt.Term();
	}

	// Return the packed history; it will be saved by caller!
	return packedIt;
}

CAkMusicPackedHistory AkRSIterator::PackIterator()
{
	CAkMusicPackedHistory history;

	// First determine how much memory is required
	AK::WriteBytesCount counter;
	if ( AK_Success == SerializeIterator( &counter ) )
	{
		AK::WriteBytesMem writer;
		writer.SetMemPool(AkMemID_Object);
		if ( writer.Reserve( counter.Count() ) )
		{
			if ( AK_Success == SerializeIterator( &writer ) )
			{
				AkInt32 count = writer.Count();
				AkUInt8* data = writer.Detach();
				history.Attach( data, count, AkMemID_Object);
			}
		}
	}

	return history;
}

bool AkRSIterator::UnpackIterator( const CAkMusicPackedHistory& in_history )
{
	AK::ReadBytesMem reader( in_history.Data(), in_history.Size() );
	return AK_Success == DeserializeIterator( &reader );
}

bool AkRSIterator::UnpackGlobalRSInfo( const CAkMusicPackedHistory& in_history )
{
	// Skip over segment info
	AK::ReadBytesSkip skipper( in_history.Data(), in_history.Size() );
	if ( AK_Success != DeserializeSegmentInfo( &skipper ) )
		return false;

	// Read in global RS info
	AK::ReadBytesMem reader( in_history.Data() + skipper.Count(), in_history.Size() - skipper.Count() );
	if ( AK_Success != DeserializeGlobalRSInfo( &reader ) )
		return false;

	return true;
}

bool AkRSIterator::PushGlobalRSInfoToCntr()
{
	// Check for a match
	for ( GlobalRSInfoMap::Iterator it = m_globalRSInfo.Begin(); it != m_globalRSInfo.End(); ++it )
	{
		if ( ! m_pRSCntr->CloneGlobalRSInfo( (*it).key, (*it).item.pRSInfo, (*it).item.numItems ) )
			return false;
	}

	return true;
}

AKRESULT AkRSIterator::SerializeIterator( AK::IWriteBytes* in_writer )
{
	if ( AK_Success != SerializeSegmentInfo( in_writer )
		|| AK_Success != SerializeGlobalRSInfo( in_writer )
		|| AK_Success != SerializeRSItemStack( in_writer ) )
		return AK_Fail;

	return AK_Success;
}

AKRESULT AkRSIterator::SerializeSegmentInfo( AK::IWriteBytes* in_writer )
{
	// Write segment info
	if ( !in_writer->Write<AkUInt32>( 1 ) // refCount
		|| !in_writer->Write<AkUniqueID>( m_actualSegment )
		|| !in_writer->Write<AkUniqueID>( m_actualPlaylistID )
		|| !in_writer->Write<AkUInt8>( m_bIsSegmentValid ? 1 : 0 )
		|| !in_writer->Write<AkInt16>( m_uSegmentLoopCount )
		)
		return AK_Fail;

	return AK_Success;
}

AKRESULT AkRSIterator::SerializeGlobalRSInfo( AK::IWriteBytes* in_writer )
{
	// Write global RS info map
	if ( !in_writer->Write<AkUInt16>( (AkUInt16)m_globalRSInfo.Length() ) )
		return AK_Fail;

	for ( GlobalRSInfoMap::Iterator it = m_globalRSInfo.Begin(); it != m_globalRSInfo.End(); ++it )
	{
		AkUniqueID playlistId = (*it).key;
		GlobalRSInfoItem& mapItem = (*it).item;

		RSType rsType = mapItem.pRSInfo->Type() == ContainerMode_Random
			? RSType_StepRandom : RSType_StepSequence;

		if ( !in_writer->Write<AkUniqueID>( playlistId )
			|| !in_writer->Write<AkUInt8>( (AkUInt8)rsType )
			|| !in_writer->Write<AkUInt16>( mapItem.numItems )
			)
			return AK_Fail;

		CAkContainerBaseInfo* pRSInfo = mapItem.pRSInfo;
		if ( AK_Success != pRSInfo->SerializeHistory( in_writer, mapItem.numItems ) )
			return AK_Fail;
	}

	return AK_Success;
}

AKRESULT AkRSIterator::SerializeRSItemStack( AK::IWriteBytes* in_writer )
{
	// Write stack items
	if ( !in_writer->Write<AkUInt16>( (AkUInt16)m_stack.Length() ) )
		return AK_Fail;

	for ( IteratorStack::Iterator it = m_stack.Begin(); it != m_stack.End(); ++it )
	{
		if ( (*it).Serialize( in_writer ) != AK_Success )
			return AK_Fail;
	}

	return AK_Success;
}

AKRESULT AkRSIterator::DeserializeIterator( AK::IReadBytes* in_reader )
{
	if ( AK_Success != DeserializeSegmentInfo( in_reader )
		|| AK_Success != DeserializeGlobalRSInfo( in_reader )
		|| AK_Success != DeserializeRSItemStack( in_reader )
		)
		return AK_Fail;

	return AK_Success;
}

AKRESULT AkRSIterator::DeserializeSegmentInfo( AK::IReadBytes* in_reader )
{
	AkUInt32 refCount;
	AkUInt8 byte;

	// Read segment info
	if ( !in_reader->Read<AkUInt32>( refCount )
		|| !in_reader->Read<AkUniqueID>( m_actualSegment )
		|| !in_reader->Read<AkUniqueID>( m_actualPlaylistID )
		|| !in_reader->Read<AkUInt8>( byte )
		|| !in_reader->Read<AkInt16>( m_uSegmentLoopCount )
		)
		return AK_Fail;

	m_bIsSegmentValid = byte ? 1 : 0;

	return AK_Success;
}

AKRESULT AkRSIterator::DeserializeGlobalRSInfo( AK::IReadBytes* in_reader )
{
	AkUInt16 length;

	// Read global RS info map
	AKASSERT( m_globalRSInfo.Length() == 0 );
	if ( !in_reader->Read<AkUInt16>( length ) )
		return AK_Fail;

	for ( AkUInt16 i = 0; i < length; ++i )
	{
		AkUniqueID playlistId;
		AkUInt8 rsType;
		AkUInt16 numItems;

		if ( !in_reader->Read<AkUniqueID>( playlistId )
			|| !in_reader->Read<AkUInt8>( rsType )
			|| !in_reader->Read<AkUInt16>( numItems )
			)
		return AK_Fail;

		// Gonna create the RSInfo without knowing the avoid count; it's ok, the deserialize will take care of it
		CAkContainerBaseInfo* rsInfo = CreateRSInfo( (RSType)rsType, numItems, 0 );
		if ( NULL == rsInfo )
			return AK_Fail;

		GlobalRSInfoItem mapItem( rsInfo, numItems );
		if ( !m_globalRSInfo.Set( playlistId, mapItem ) )
			return AK_Fail;

		if ( AK_Success != rsInfo->DeserializeHistory( in_reader, numItems ) )
			return AK_Fail;
	}

	return AK_Success;
}

AKRESULT AkRSIterator::DeserializeRSItemStack( AK::IReadBytes* in_reader )
{
	AkUInt16 length;

	// Read stack items
	AKASSERT( m_stack.Length() == 0 );
	if ( !in_reader->Read<AkUInt16>( length ) )
		return AK_Fail;

	if ( length != 0 )
	{
		if ( AK_Success != m_stack.Reserve( length ) )
			return AK_Fail;

		CAkRSSub* pRSSub = static_cast<CAkRSSub*>( m_pRSCntr->GetPlaylistRoot() );

		for ( AkUInt16 i = 0; i < length; ++i )
		{
			RSStackItem* item = m_stack.AddLast();
			if ( NULL == item || AK_Success != m_stack[i].Deserialize( in_reader, pRSSub, *this ) )
				return AK_Fail;
		}
	}

	return AK_Success;
}
