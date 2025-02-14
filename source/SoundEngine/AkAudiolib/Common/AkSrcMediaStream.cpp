/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

#include "stdafx.h"

#include "AkSrcMediaStream.h"
#include "AkProfile.h"

namespace AK
{
namespace SrcMedia
{
namespace Stream
{

AkFileSystemFlags CreateFileSystemFlags(AkSrcTypeInfo * pSrcType)
{
	// Generation of complete file name from bank encoded name and global audio source path settings
	// is done at the file system level. Just fill up custom FS parameters to give it a clue.
	AkFileSystemFlags fileSystemFlags(
		AKCOMPANYID_AUDIOKINETIC, // Company ID. 
		CODECID_FROM_PLUGINID(pSrcType->dwID), // File/Codec type ID (defined in AkTypes.h).
		0,					// User parameter size.
		0,                  // User parameter.
		((bool)pSrcType->mediaInfo.bIsLanguageSpecific), // True when file location depends on current language.
		pSrcType->GetCacheID()
	);

	if (pSrcType->mediaInfo.bExternallySupplied)
	{
		//This stream was started through the external source mechanism.
		//Tell the low level IO that we are only the middleman; we don't know this file, only the game knows about it.	
		//Also, need to disable cache because a different file may be used each time with same source.
		fileSystemFlags.uCompanyID = AKCOMPANYID_AUDIOKINETIC_EXTERNAL;
	}
	return fileSystemFlags;
}

AKRESULT Init(State * pState, const InitParams &params)
{
	AkZeroMemSmall(pState, sizeof(State));

	// File name.
	const AkOSChar * pFileName = NULL;
	AkSrcTypeInfo * pSrcType = params.pSrcType;
	if (pSrcType->mediaInfo.Type == SrcTypeFile &&
		pSrcType->GetFilename() == NULL &&
		pSrcType->GetFileID() == AK_INVALID_FILE_ID)
	{
		return AK_Fail;
	}

	// Stream heuristics.
	AkAutoStmHeuristics heuristics;
	heuristics.uMinNumBuffers = 0;
	heuristics.fThroughput = 1;
	heuristics.uLoopStart = 0;
	heuristics.uLoopEnd = 0;
	heuristics.priority = params.priority;

	// Default buffering settings
	AkAutoStmBufSettings bufSettings;
	bufSettings.uBufferSize = 0;		// No constraint.
	bufSettings.uMinBufferSize = AK_WORST_CASE_MIN_STREAM_BUFFER_SIZE;
	bufSettings.uBlockSize = 0;

	pState->uLoopCnt = params.uLoopCnt;
	pState->priority = params.priority;
	pState->bIsMemoryStream = pSrcType->mediaInfo.Type == SrcTypeMemory;

	// Create stream.
	AKRESULT eResult;
	if (pState->bIsMemoryStream)
	{
		AkUInt8 * pBuffer = params.pBuffer;
		AkUInt32 uSize = params.uBufferSize;

		if (pBuffer == nullptr)
			return AK_NoDataReady;

		eResult = AK::IAkStreamMgr::Get()->CreateAuto(
			pBuffer,
			uSize,
			heuristics,
			pState->pStream);
	}
	else if (!pSrcType->UseFilenameString())
	{
		AKASSERT(pSrcType->GetFileID() != AK_INVALID_FILE_ID);
		AkFileSystemFlags flags = CreateFileSystemFlags(pSrcType);
		pFileName = pSrcType->GetFilename();
		eResult = AK::IAkStreamMgr::Get()->CreateAuto(
			pSrcType->GetFileID(),  // Application defined ID.
			&flags,                 // File system special parameters.
			heuristics,             // Auto stream heuristics.
			&bufSettings,           // Stream buffer constraints.
			pState->pStream,
			AK_PERF_OFFLINE_RENDERING);
	}
	else
	{
		AkFileSystemFlags flags = CreateFileSystemFlags(pSrcType);
		pFileName = pSrcType->GetFilename();
		eResult = AK::IAkStreamMgr::Get()->CreateAuto(
			pFileName,          // Application defined string (title only, or full path, or code...).
			&flags,             // File system special parameters.
			heuristics,         // Auto stream heuristics.
			&bufSettings,       // Stream buffer constraints.
			pState->pStream,
			AK_PERF_OFFLINE_RENDERING);
	}

	if (AK_EXPECT_FALSE(eResult != AK_Success))
	{
		AKASSERT(pState->pStream == NULL);
		return eResult;
	}

	AKASSERT(pState->pStream != NULL);

	// Handle prefetch area
	bool hasPrefetch = !pState->bIsMemoryStream && params.pBuffer != nullptr && params.uBufferSize > 0;
	if (hasPrefetch)
	{
		// Get the pointer to the prefetch memory buffer while we have a handle on the PBI
		pState->pPrefetch = params.pBuffer;
		pState->uPrefetchSize = params.uBufferSize;
		pState->ePrefetch = Prefetch::Available;		
	}
	else
	{
		pState->ePrefetch = Prefetch::None;
		// We start streaming from the beginning of the media
		eResult = pState->pStream->Start();
		if (AK_EXPECT_FALSE(eResult != AK_Success))
			return eResult;
	}
	pState->ePrefetchCommit = PrefetchCommit::NotCommitted;

	// In profiling mode, name the stream.
	// Profiling: create a string out of FileID.
#ifndef AK_OPTIMIZED
	if (pFileName != NULL)
	{
#if defined AK_WIN
		// Truncate first characters. GetFilename() is the whole file path pushed by the WAL, and it may be longer
		// than AK_MONITOR_STREAMNAME_MAXLENGTH.
		size_t len = AKPLATFORM::OsStrLen(pFileName);
		if (len < AK_MONITOR_STREAMNAME_MAXLENGTH)
			pState->pStream->SetStreamName(pFileName);
		else
		{
			AkOSChar szName[AK_MONITOR_STREAMNAME_MAXLENGTH];
			const AkOSChar * pSrcStr = pFileName + len - AK_MONITOR_STREAMNAME_MAXLENGTH + 1;
			AKPLATFORM::SafeStrCpy(szName, pSrcStr, AK_MONITOR_STREAMNAME_MAXLENGTH);
			szName[0] = '.';
			szName[1] = '.';
			szName[2] = '.';
			pState->pStream->SetStreamName(szName);
		}
#else
		{
			pState->pStream->SetStreamName(pFileName);
		}
#endif
	}
	else if (!pState->bIsMemoryStream)
	{
		const unsigned long MAX_NUMBER_STR_SIZE = 11;
		AkOSChar szName[MAX_NUMBER_STR_SIZE];
		AK_OSPRINTF(szName, MAX_NUMBER_STR_SIZE, AKTEXT("%u"), pSrcType->GetFileID());
		pState->pStream->SetStreamName(szName);
	}
#endif

	return eResult;
}

void Term(State * pState)
{
	if (pState->pStream != nullptr)
	{
		pState->pStream->Destroy();
		pState->pStream = nullptr;
	}
}

AKRESULT FetchStreamBuffer(State * pState)
{
	AkUInt8 * pBuffer = NULL;
	AkUInt32 uSize = 0;
	AKRESULT eResult = ReadStreamBuffer(pState, &pBuffer, &uSize);

	if ( eResult == AK_DataReady || eResult == AK_NoMoreData )
	{
		pState->bNextFetchWillLoop = false;
		if(uSize != 0)
		{
			eResult = CommitStreamBuffer(pState, pBuffer, uSize);
			if ( eResult == AK_Success )
				eResult = AK_DataReady;
		}
		else
		{
			AKASSERT( !"Unexpected end of streamed audio file" );
			eResult = AK_Fail;
		}
	}

	AKASSERT( eResult == AK_NoDataReady || eResult == AK_DataReady || eResult == AK_Fail );
	return eResult;
}

AKRESULT StopLooping(State * pState)
{
	pState->uLoopCnt = 1;

	if (NextFetchWillLoop(pState))
	{
		// Stream was going to loop; its position was already set back to uLoopStart, which is wrong.
		if (pState->bIsMemoryStream && pState->pStream)
		{
			// For memory streams, we can 'repair' the state because we know stream seeking is instantaneous.
			if (AK_EXPECT_FALSE(SetStreamPosition(pState, pState->uLoopEnd) != AK_Success))
				return AK_Fail;

			pState->bIsLastStmBuffer = pState->uLoopEnd == pState->uFileSize;
			pState->bNextFetchWillLoop = false;
		}
	}

	if (pState->pStream)
	{
		// Set stream's heuristics to non-looping.
		AkAutoStmHeuristics heuristics;
		pState->pStream->GetHeuristics(heuristics);
		heuristics.uLoopEnd = 0;
		pState->pStream->SetHeuristics(heuristics);
	}

	return AK_Success;
}

AKRESULT Seek(State * pState, AkUInt32 in_uPosition, AkUInt16 in_uLoopCnt)
{	
	AKRESULT eResult = AK_Fail;
	//Check if we are seeking just a little bit further in the same buffer.
	if (pState->pNextAddress && //Have data
		in_uPosition >= pState->uCurFileOffset && in_uPosition < pState->uCurFileOffset + pState->uSizeLeft &&	//Within the boundaries of the buffer
		!AK::SrcMedia::Stream::NextFetchWillLoop(pState) && in_uLoopCnt == pState->uLoopCnt)	//For the same conceptual lopp pass.
	{
		AK::SrcMedia::Stream::ConsumeData(pState, in_uPosition - pState->uCurFileOffset);
		eResult = AK_PartialSuccess;
	}
	else if (pState->pPrefetch && in_uPosition < pState->uPrefetchSize)
	{
		// The seek point is inside the prefetch, just use the in-memory data.
		// Seek the file stream to the end of the prefetch data, to be read what comes next.
		pState->ePrefetch = AK::SrcMedia::Stream::Prefetch::Available;
		pState->ePrefetchCommit = AK::SrcMedia::Stream::PrefetchCommit::NotCommitted;
		pState->uFileOffset = in_uPosition;

		// Next fetch will automatically seek the stream to the end of the prefetch area
		eResult = AK_Success;
	}
	else
	{
		if (pState->pPrefetch && pState->ePrefetch != AK::SrcMedia::Stream::Prefetch::None)		
			pState->ePrefetch = AK::SrcMedia::Stream::Prefetch::Released;	//We're out of the prefetch, make sure it is marked as such.
		
		eResult = SetStreamPosition(pState, in_uPosition);
		if (AK_EXPECT_FALSE(eResult != AK_Success))
			return eResult;
	}

	if (eResult != AK_PartialSuccess)
	{
		pState->bIsLastStmBuffer = false;
		pState->bNextFetchWillLoop = false;
	}

	// Reset stream-side loop count and update heuristics based on that.
	UpdateLooping(pState, pState->uLoopStart, pState->uLoopEnd, in_uLoopCnt);

	AKRESULT finalRes = EnsureStreamIsRunning(pState);
	return finalRes != AK_Success ? finalRes : eResult;	//Return PartialSuccess if the same buffer is used.
}

AKRESULT RelocateMedia(State * pState, AkUInt8 * in_pNewMedia, AkUInt8 * in_pOldMedia)
{
	if (pState->pPrefetch == in_pOldMedia)
		pState->pPrefetch = in_pNewMedia;

	if (pState->bIsMemoryStream || pState->ePrefetchCommit == PrefetchCommit::CurrentlyCommitted)
	{
		pState->pNextAddress = pState->pNextAddress + (AkUIntPtr)in_pNewMedia - (AkUIntPtr)in_pOldMedia;		
	}
	if (pState->bIsMemoryStream)
	{
		return AK::IAkStreamMgr::Get()->RelocateMemoryStream(pState->pStream, in_pNewMedia);
	}
	return AK_Success;
}

AKRESULT SetStreamPosition(State * pState, AkUInt32 in_uPosition)
{
	AkInt64 lRealOffset;
	
	// Set stream position now for next read.
	AKRESULT eResult = pState->pStream->SetPosition( in_uPosition, AK_MoveBegin, &lRealOffset );
	if ( AK_EXPECT_FALSE( eResult != AK_Success ) )
		return AK_Fail;

	// Keep track of offset caused by unbuffered IO constraints.
	// Set file offset to true value.
	pState->uCorrection = in_uPosition - (AkUInt32)lRealOffset;
	pState->uFileOffset = (AkUInt32)lRealOffset;

	return AK_Success;
}

AKRESULT ReadStreamBuffer(State * pState, AkUInt8 ** out_ppBuffer, AkUInt32 * out_pSize)
{
	AKRESULT eResult;	
	if (pState->ePrefetch == Prefetch::Available)
	{
		AKASSERT(0 <= pState->uFileOffset && pState->uFileOffset < pState->uPrefetchSize);
		eResult = AK_DataReady;
		*out_ppBuffer = pState->pPrefetch + pState->uFileOffset;
		*out_pSize = pState->uPrefetchSize - pState->uFileOffset;
		pState->pNextAddress = nullptr;
		pState->uSizeLeft = 0;
		pState->ePrefetch = Prefetch::Read;
	}
	else
	{
		// Stream management members must be clear.
		AKASSERT(pState->uSizeLeft == 0 && !HasNoMoreStreamData(pState));
		pState->pNextAddress = nullptr;

		// Update priority heuristic.
		AkAutoStmHeuristics heuristics;
		pState->pStream->GetHeuristics(heuristics);
		heuristics.priority = pState->priority;
		pState->pStream->SetHeuristics(heuristics);

		// Get stream buffer.
		eResult = pState->pStream->GetBuffer(
			*(void**)out_ppBuffer,      // Address of granted data space.
			*out_pSize,                 // Size of granted data space.
			AK_PERF_OFFLINE_RENDERING); // Block until data is ready.
	}
	return eResult;
}

AKRESULT CommitStreamBuffer(State * pState, AkUInt8 * in_pBuffer, AkUInt32 uSize)
{
	AKASSERT( pState->pStream );

	// Set next pointer.
	AKASSERT(in_pBuffer != NULL);
	pState->pNextAddress = in_pBuffer;
	pState->uSizeLeft = uSize;

	// Update offset in file.
	pState->uCurFileOffset = pState->uFileOffset;
	pState->uFileOffset += pState->uSizeLeft;

	// "Consume" correction due to seeking on block boundary. This will move pState->pNextAddress, pState->uCurFileOffset and pStateuSizeLeft in their respective directions.
	ConsumeData(pState, pState->uCorrection);

	bool bInPrefetch = pState->ePrefetch == Prefetch::Read && pState->ePrefetchCommit == PrefetchCommit::NotCommitted;

	// Will we hit the loopback boundary?
	AKASSERT(pState->uLoopEnd > 0);
	AKASSERT(pState->uFileSize > 0);
	AkUInt32 ulEndLimit = WillLoop(pState) ? pState->uLoopEnd : pState->uFileSize;
	if ( pState->uFileOffset >= ulEndLimit )
	{
		// Yes. Correct size left.
		AkUInt32 ulCorrectionAmount = pState->uFileOffset - ulEndLimit;
		AKASSERT( pState->uSizeLeft >= ulCorrectionAmount ||
			!"Missed the position change at last stream read" );
		pState->uSizeLeft -= ulCorrectionAmount;

		if ( WillLoop(pState) )
		{
			// Change stream position for next read.
			if ( AK_EXPECT_FALSE(SetStreamPosition(pState, pState->uLoopStart) != AK_Success ))
				return AK_Fail;
			
			pState->uLoopCnt = pState->uLoopCnt == 0 ? 0 : pState->uLoopCnt - 1;
			pState->bNextFetchWillLoop = true;

			// Update heuristics to end of file if last loop.
			if ( !WillLoop(pState) ) 
			{
				// Set stream's heuristics to non-looping.
				AkAutoStmHeuristics heuristics;
				pState->pStream->GetHeuristics( heuristics );
				heuristics.uLoopEnd = 0;
				pState->pStream->SetHeuristics( heuristics );
			}
		}
		else
		{
			// Hit the loop release (end of file) boundary: will be NoMoreData.
			// Don't care about correction.
			// Set this flag to notify output.
			pState->bIsLastStmBuffer = true;
		}
	}
	else
	{
		if (bInPrefetch)
		{
			// We need to position the stream at the area where prefetch ends for the next fetch to be accurate
			if (AK_EXPECT_FALSE(SetStreamPosition(pState, pState->uFileOffset) != AK_Success))
				return AK_Fail;
		}
		else
		{
			// Stream is already positioned correctly for next fetch, reset the offset.
			pState->uCorrection = 0;
		}
	}

	if (bInPrefetch)
	{
		if (AK_EXPECT_FALSE(EnsureStreamIsRunning(pState) != AK_Success))
			return AK_Fail;
		pState->ePrefetchCommit = PrefetchCommit::CurrentlyCommitted;
	}
	else if (pState->ePrefetchCommit == PrefetchCommit::CurrentlyCommitted)
	{
		pState->ePrefetchCommit = PrefetchCommit::Past;
	}

	return AK_Success;
}

} // Stream
} // SrcMedia
} // AK
