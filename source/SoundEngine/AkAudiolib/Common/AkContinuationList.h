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
// AkContinuationList.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _CONTINUATION_LIST_H_
#define _CONTINUATION_LIST_H_

#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkSmartPtr.h>
#include <AK/IBytes.h>

class CAkRanSeqCntr;
class CAkMultiPlayNode;
class CAkContinuationList;
class CAkContainerBaseInfo;

struct AkLoop
{
	AkInt16 lLoopCount;		// Number of loop before continue
	AkUInt8 bIsEnabled : 1;	// Is Looping enabled
	AkUInt8 bIsInfinite : 1;	// Is looping infinite
	AkLoop()
		:lLoopCount(0)
		, bIsEnabled(false)
		, bIsInfinite(false)
	{}

	AKRESULT Serialize( AK::IWriteBytes* in_writer )
	{
		AkUInt8 bitField = (bIsEnabled << 0) | (bIsInfinite << 1);

		if ( !in_writer->Write<AkInt16>( lLoopCount )
			|| !in_writer->Write<AkUInt8>( bitField ) )
			return AK_Fail;

		return AK_Success;
	}

	AKRESULT Deserialize( AK::IReadBytes* in_reader )
	{
		AkUInt8 bitField = 0;

		if ( !in_reader->Read<AkInt16>( lLoopCount )
			|| !in_reader->Read<AkUInt8>( bitField ) )
			return AK_Fail;

		bIsEnabled = (bitField >> 0) & 0x1;
		bIsInfinite = (bitField >> 1) & 0x1;

		return AK_Success;
	}
};

// This is the number of buffers we wait in a continuation list before
// attempting to play the next item when an item fails. The goal is to
// avoid making lots of very rapid attempts that keep failing.
#define AK_WAIT_BUFFERS_AFTER_PLAY_FAILED		(10)

class CAkContinueListItem
{
public:
	//Constructor
	CAkContinueListItem();

	//Destructor
	~CAkContinueListItem();

	const CAkContinueListItem& operator=(const CAkContinueListItem & in_listItem);

// Members are public since they all have to be used by everybody
public:
	CAkSmartPtr<CAkRanSeqCntr> m_pContainer;			// Pointer to the container
	CAkContainerBaseInfo* m_pContainerInfo;	// Container info
											// Owned by this class
	AkLoop m_LoopingInfo;					// Looping info
	CAkSmartPtr<CAkMultiPlayNode> m_pMultiPlayNode;
	CAkContinuationList* m_pAlternateContList;
};

class CAkContinuationList
{
protected:
	CAkContinuationList();

public:
	//MUST Assign the returned value of create into a CAkSmartPtr.
	static CAkContinuationList* Create();

	typedef AkArray<CAkContinueListItem, const CAkContinueListItem&, ArrayPoolDefault> AkContinueListItem;
	AkContinueListItem m_listItems;
	void AddRef();
	void Release();

private:
	void Term();
	AkInt32 m_iRefCount;
};

#endif
