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
// AkRSIterator.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _MUSIC_RS_ITERATOR_H_
#define _MUSIC_RS_ITERATOR_H_

#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkKeyArray.h>
#include "AkRanSeqBaseInfo.h"
#include "AkMusicStructs.h"
#include "AkMusicPlaybackHistory.h"

class CAkMusicRanSeqCntr;
class CAkRSNode;
typedef AkArray<CAkRSNode*, CAkRSNode*, ArrayPoolDefault> AkRSList;

//////////////////////////////
//
//////////////////////////////
class CAkRSNode
{
public:
	CAkRSNode( CAkRSNode* in_Parent )
		:m_pParent( in_Parent )
		, m_ID( AK_INVALID_UNIQUE_ID )
		, m_Loop( 1 )
		, m_LoopMin( 0 )
		, m_LoopMax( 0 )
		, m_Weight( DEFAULT_RANDOM_WEIGHT )
	{}

	virtual ~CAkRSNode(){}
	AkInt16 GetLoop()    { return m_Loop; }
	AkInt16 GetLoopMin()    { return m_LoopMin; }
	AkInt16 GetLoopMax()    { return m_LoopMax; }
	void SetLoop( AkInt16 in_Loop, AkInt16 in_LoopMin, AkInt16 in_LoopMax )  { m_Loop = in_Loop; m_LoopMin = in_LoopMin; m_LoopMax = in_LoopMax; }
	AkInt16 GetInitialLoopCountLoop();

	virtual bool IsSegment(){ return false; }

	AkUInt32 GetWeight(){ return m_Weight; }
	void SetWeight( AkUInt32 in_Weight ){ m_Weight = in_Weight; }

	CAkRSNode* Parent(){ return m_pParent; }

	void PlaylistID( AkUniqueID in_ID ){ m_ID = in_ID; }
	AkUniqueID PlaylistID(){ return m_ID; }

private:
	CAkRSNode* m_pParent;
	AkUniqueID m_ID;		// for jump to direct item in sequence after switch.
	AkInt16 m_Loop;
	AkInt16 m_LoopMin;
	AkInt16 m_LoopMax;
	AkUInt32 m_Weight;
};

//////////////////////////////
//
//////////////////////////////
class CAkRSSub : public CAkRSNode
{
	
public:
	CAkRSSub( CAkRSNode* in_Parent )
		:CAkRSNode( in_Parent )
		,m_eRSType( RSType_ContinuousSequence )
		,m_bIsUsingWeight( false )
		,m_bIsShuffle( true )
		,m_bHasSegmentLeaves( false )
		,m_wAvoidRepeatCount( 0 )		
	{}

	virtual ~CAkRSSub();

	RSType GetType(){ return m_eRSType; }
	void SetType( RSType in_Type ){ m_eRSType = in_Type; }

	bool HasSegmentLeaves() { return m_bHasSegmentLeaves; }
	void WasSegmentLeafFound();

	bool IsContinuous()
	{ 
		return m_eRSType == RSType_ContinuousSequence
			|| m_eRSType == RSType_ContinuousRandom;
	}

	bool IsGlobal()
	{ 
		return m_eRSType == RSType_StepSequence
			|| m_eRSType == RSType_StepRandom;
	}

	bool IsUsingWeight(){ return m_bIsUsingWeight; }
	void IsUsingWeight( bool in_UseWeight ){ m_bIsUsingWeight = in_UseWeight; }

	AkUInt16 AvoidRepeatCount(){ return m_wAvoidRepeatCount; }
	void AvoidRepeatCount( AkUInt16 in_wAvoidRepeatCount ){ m_wAvoidRepeatCount = in_wAvoidRepeatCount; }

	AkRandomMode RandomMode();
	void RandomMode( bool in_bIsShuffle ){m_bIsShuffle = in_bIsShuffle;}

	AkUInt32 CalculateTotalWeight();
	
	const AkRSList& GetChildren() const { return m_listChildren; }

	AkRSList m_listChildren;

private:
	
	RSType m_eRSType;
	bool m_bIsUsingWeight;
	bool m_bIsShuffle;
	bool m_bHasSegmentLeaves;
	AkUInt16 m_wAvoidRepeatCount;
};

//////////////////////////////
//
//////////////////////////////
class CAkRSSegment : public CAkRSNode
{
public:

	CAkRSSegment( CAkRSNode* in_Parent )
		:CAkRSNode( in_Parent )
	{}

	virtual ~CAkRSSegment()
	{}

	virtual bool IsSegment(){ return true; }

	AkUniqueID GetSegmentID(){ return m_SegmentID; }
	void SetSegmentID( AkUniqueID in_uSegmentID ){ m_SegmentID = in_uSegmentID; }

private:
	AkUniqueID m_SegmentID;
};

//////////////////////////////
//
//////////////////////////////
class AkRSIterator;

class RSStackItem
{
public:
	RSStackItem()
		: m_pLocalRSInfo( NULL )
	{}

	AKRESULT Init( CAkRSSub * in_pSub, AkRSIterator * in_pRSit );
	void Clear();

	CAkContainerBaseInfo * GetRSInfo() const { return m_pLocalRSInfo; }

	AKRESULT Serialize( AK::IWriteBytes* in_writer );
	AKRESULT Deserialize( AK::IReadBytes* in_reader, CAkRSSub*& io_pRSSub, AkRSIterator& in_rsIt );

	CAkRSSub* m_pRSSub;
	AkLoop m_Loop;

private:
	RSStackItem( RSStackItem& );

	bool IsGlobal() { return IsGlobal( m_pRSSub->GetType() ); }

	static bool IsGlobal( RSType in_type )
	{
		return in_type == RSType_StepRandom
			|| in_type == RSType_StepSequence;
	}

	CAkContainerBaseInfo* m_pLocalRSInfo;
};


//////////////////////////////
//
//////////////////////////////
class AkRSIterator
{
public:
	AkRSIterator();
	~AkRSIterator();

	AKRESULT Init( CAkMusicRanSeqCntr* in_pRSCntr, AkPlayingID in_playingID, const CAkMusicPlaybackHistory* in_history );
	void EndInit();	// Call when no more JumpTo will be called.

	void Term();

	AKRESULT JumpTo( AkUniqueID in_playlistElementID, const CAkMusicPlaybackHistory* in_pHistory );

	// IsValid() returns false if there was a memory error OR if the end of the playlist was reached.
	inline bool IsValid() const { return m_bIsSegmentValid; }
	
	// IMPORTANT. This should only be used when an error occurs.
	void SetAsInvalid(){ m_bIsSegmentValid = false; }

	AkUniqueID GetCurrentSegment() const { return m_actualSegment; }
	AkUniqueID GetPlaylistItem() const { return m_actualPlaylistID; }

	void MoveToNextSegment();	// Move iterator forward; the current segment will be played, so keep iterator in history
	void SkipToNextSegment();	// Skip iterator forward; the current segment will NOT be played, so DON'T keep iterator in history
	CAkMusicPackedHistory NotifySegmentPlay();

	static CAkContainerBaseInfo* CreateRSInfo( CAkRSSub* in_pSub );
	static CAkContainerBaseInfo* CreateRSInfo( RSType in_type, AkUInt16 in_numChildren, AkUInt16 in_avoidRepeat );

	CAkContainerBaseInfo* GetGlobalRSInfo( CAkRSSub* in_pSub );
	CAkContainerBaseInfo* FindGlobalRSInfo( AkUniqueID in_playlistId );

	static CAkContainerBaseInfo* AllocRSInfo( RSType in_type, AkUInt16 in_numItems );

	// Function pushes global RS Info to RS container
	bool PushGlobalRSInfoToCntr();

private:

	static CAkRandomInfo* CreateRandomInfo( AkUInt16 in_numChildren, AkUInt16 in_avoidRepeat );
	static CAkSequenceInfo* CreateSequenceInfo( AkUInt16 in_numChildren );

	void FlushGlobalRSInfo();

private:

	void FlushStack();
	void PopLast();

	typedef AkArray< CAkRSNode*, CAkRSNode*,ArrayPoolDefault> JumpToList;

	void JumpNext();

	AKRESULT JumpNextInternal();

	AKRESULT FindAndSelect( CAkRSNode* in_pNode, AkUniqueID in_playlistElementID, JumpToList& io_jmpList, bool& io_bFound );

	CAkRSNode* PopObsoleteStackedItems( CAkRSNode* in_pNode );

	AKRESULT StackItem( CAkRSSub* in_pSub );

	AkUInt16 Select( RSStackItem & in_rStackItem, bool & out_bIsEnd );

	//Used in JumpTo feature.
	void ForceSelect( CAkRSNode* in_pForcedNode );

	// Select Randomly a sound to play from the specified set of parameters
	//
	// Return - AkUInt16 - Position in the playlist to play
	AkUInt16 SelectRandomly( RSStackItem & in_rStackItem, bool & out_bIsEnd );

	// Select SelectSequentially a sound to play from the specified set of parameters
	//
	// Return - AkUInt16 - Position in the playlist to play
	AkUInt16 SelectSequentially( RSStackItem & in_rStackItem, bool & out_bIsEnd );

	//Used in JumpTo feature.
	void ForceSelectRandomly( CAkRSNode* in_pForcedNode );
	void ForceSelectSequentially( CAkRSNode* in_pForcedNode );

	void UpdateRandomItem( CAkRSSub* in_pSub, AkUInt16 in_wPosition, AkRSList* in_pRSList, CAkRandomInfo* in_pRanInfo );

	// Function called once a complete loop is completed.
	// 
	// Return - bool - true if the loop must continue, false if nothing else has to be played
	bool CanContinueAfterCompleteLoop(
		AkLoop& io_loopingInfo			// Looping information (not const)
		);

	// Gets if the playlist position can be played(already played and avoid repeat)
	//
	// Return - bool - true if can play it, false otherwise
	bool CanPlayPosition(
		CAkRSSub* in_pSub,
		CAkRandomInfo* in_pRandomInfo,	// Container set of parameters
		AkUInt16 in_wPosition				// Position in the list
		);

	AKRESULT SetCurrentSegmentToNode( CAkRSNode* in_pNode );

public:

	CAkMusicPackedHistory PackIterator();
	bool UnpackIterator( const CAkMusicPackedHistory& in_packed );

private:
	bool UnpackGlobalRSInfo( const CAkMusicPackedHistory& in_history );

	AKRESULT SerializeIterator( AK::IWriteBytes* in_writer );
	AKRESULT DeserializeIterator( AK::IReadBytes* in_reader );

	AKRESULT SerializeSegmentInfo( AK::IWriteBytes* in_writer );
	AKRESULT DeserializeSegmentInfo( AK::IReadBytes* in_reader );

	AKRESULT SerializeGlobalRSInfo( AK::IWriteBytes* in_writer );
	AKRESULT DeserializeGlobalRSInfo( AK::IReadBytes* in_reader );

	AKRESULT SerializeRSItemStack( AK::IWriteBytes* in_writer );
	AKRESULT DeserializeRSItemStack( AK::IReadBytes* in_reader );

private:

	void SetRanSeqCntr( CAkMusicRanSeqCntr* in_pRSCntr ) { m_pRSCntr = in_pRSCntr; }
	CAkMusicRanSeqCntr* m_pRSCntr; // required to access globally scoped items.

	AkPlayingID m_playingID;

	typedef AkArray<RSStackItem, const RSStackItem&, ArrayPoolDefault> IteratorStack;
	IteratorStack m_stack;
	AkUniqueID m_actualSegment;
	AkUniqueID m_actualPlaylistID;
	bool m_bIsSegmentValid;
	AkInt16 m_uSegmentLoopCount;	

	// Global RS step info, mapped by playlist item ID.  The map items are used directly by
	// this iterator.  If a global RSInfo is needed by the iterator:
	//	- we will use the RSInfo in the map if it exists
	//	- if not in the map, we will look to CAkMusicRanSeqCntr, and clone it if it exists
	//	- else, we'll create a new RSInfo and add it to the map
	// Point is, we use our own instance of global RSInfo.  The map's contents are pushed
	// back (i.e. cloned) to CAkMusicRanSeqCntr via the iterator history.
	struct GlobalRSInfoItem
	{
		GlobalRSInfoItem()
			: pRSInfo( NULL )
			, numItems( 0 )
		{}
		GlobalRSInfoItem( CAkContainerBaseInfo* in_pRSInfo, AkUInt16 in_numItems )
			: pRSInfo( in_pRSInfo )
			, numItems( in_numItems )
		{}
		GlobalRSInfoItem( const GlobalRSInfoItem& in_other )
			: pRSInfo( in_other.pRSInfo )
			, numItems( in_other.numItems )
		{}

		CAkContainerBaseInfo* pRSInfo;
		AkUInt16 numItems;
	};
	typedef CAkKeyArray<AkUniqueID, GlobalRSInfoItem> GlobalRSInfoMap;
	GlobalRSInfoMap m_globalRSInfo;

	// History of past iterators, byte packed
	typedef AkArray<CAkMusicPackedHistory, CAkMusicPackedHistory, ArrayPoolDefault> IteratorHistoryList;

	// List of packed iterators.  The is modified when:
	//  - new segment is scheduled; the iterator is packed BEFORE beging modified, and added.
	//  - segment playback begins; the front item of the list is removed.
	IteratorHistoryList m_history;
};

#endif //_MUSIC_RS_ITERATOR_H_
