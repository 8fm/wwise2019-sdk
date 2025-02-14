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

#pragma once

#include <AK/SoundEngine/Common/IAkStreamMgr.h>
#include "AkCommon.h"
#include "AkSource.h"

#define AK_WORST_CASE_MIN_STREAM_BUFFER_SIZE	(2048)	// Totally arbitrary. Most sources (that use the default implementation of StartStream()) should not need more.

namespace AK
{
	namespace SrcMedia
	{
		namespace Stream
		{
			enum Prefetch
			{
				None = 0,
				Available = 1,
				Read = 2,
				Released = 3
				// Careful, this enum is held in 3 bits only!
			};
			enum PrefetchCommit
			{
				NotCommitted = 0,
				CurrentlyCommitted = 1,
				Past = 2,
				// Careful, this enum is held in 2 bits only!
			};
			struct State
			{
				AK::IAkAutoStream * pStream;

				AkPriority  priority;

				AkUInt8 *   pPrefetch;		  // Address of the prefetch data, NULL if none.
				AkUInt8 *   pNextAddress;     // Address of _current_ stream buffer. NULL if none.
				AkUInt32    uSizeLeft;        // Size left in stream buffer.
				AkUInt32    uPrefetchSize;    // Size of the in-memory area in the case of streaming media.
				AkUInt32    uCurFileOffset;   // Offset relative to beginning of file of _current_ stream buffer (corresponding to pState->pNextAddress).
				AkUInt32    uFileOffset;      // Offset relative to beginning of file of _next_ stream buffer.
				AkUInt32    uCorrection;      // Correction amount (when loop start not on sector boundary).

				AkUInt32    uFileSize;        // Total size of media file (including headers) in bytes

				AkUInt32    uLoopStart;      // Loop start position: Byte offset from beginning of stream.
				AkUInt32    uLoopEnd;        // Loop back boundary from beginning of stream.
				AkUInt16    uLoopCnt;        // Stream-side loop count

				bool        bIsLastStmBuffer : 1;       // True when last stream buffer
				bool        bIsMemoryStream : 1;        // true when reading off a buffer in memory
				bool        bNextFetchWillLoop : 1;     // true when stream has been pre-emptively set back to start of loop for the next fetch

				// Prefetch state machine
				AkUInt8     ePrefetch       : 3;
				AkUInt8     ePrefetchCommit : 2;
			};

			struct InitParams
			{
				AkSrcTypeInfo * pSrcType;     // Usually obtained via a PBI

				AkUInt16 uLoopCnt;            // Initial loop count

				AkPriority priority;          // Initial stream I/O priority

				AkUInt8*  pBuffer;            // A portion of the media already in-memory
				AkUInt32  uBufferSize;        // Size of the above buffer
			};

			AkFileSystemFlags CreateFileSystemFlags(AkSrcTypeInfo * pSrcType);

			AKRESULT Init(State * pState, const InitParams &params);
			void Term(State * pState);

			/// Read a buffer from the stream, then advance the stream state and loop if appropriate.
			AKRESULT FetchStreamBuffer(State *pState);

			/// Release a buffer that was read from the stream.
			inline void ReleaseStreamBuffer(State *pState)
			{
				if (pState->ePrefetch == Prefetch::Read)
				{
					pState->ePrefetch = Prefetch::Released;
				}
				else
				{
					pState->pStream->ReleaseBuffer();
				}
			}

			/// Returns false if and only if the next ReadStreamBuffer() will return AK_NoMoreData.
			inline bool HasNoMoreStreamData(State *pState) { return pState->bIsLastStmBuffer; }

			/// Update file size using value that could not be obtained during Init
			inline void UpdateFileSize(State *pState, AkUInt32 uFileSize)
			{
				pState->uFileSize = uFileSize;
			}

			/// Returns whether this stream has a prefetched area already in memory
			inline bool HasPrefetch(State *pState)
			{
				return pState->ePrefetch != Prefetch::None;
			}

			inline bool WillLoop(State *pState)
			{
				return pState->uLoopCnt != LOOPING_ONE_SHOT;
			}

			inline AkUInt32 GetPosition(State *pState)
			{
				return (AkUInt32)pState->pStream->GetPosition(NULL) + pState->uCorrection;
			}

			/// Update looping information
			inline void UpdateLooping(State* pState, AkUInt32 uLoopStart, AkUInt32 uLoopEnd, AkUInt16 uLoopCnt)
			{
				pState->uLoopStart = uLoopStart;
				pState->uLoopEnd = uLoopEnd;
				pState->uLoopCnt = uLoopCnt;

				AkAutoStmHeuristics heuristics;
				pState->pStream->GetHeuristics(heuristics);
				if (WillLoop(pState))
				{
					heuristics.uLoopStart = pState->uLoopStart;
					heuristics.uLoopEnd = pState->uLoopEnd;
				}
				else
				{
					heuristics.uLoopStart = 0;
					heuristics.uLoopEnd = 0;
				}
				pState->pStream->SetHeuristics(heuristics);
			}

			AKRESULT StopLooping(State * pState);

			inline bool NextFetchWillLoop(State *pState)
			{
				return pState->bNextFetchWillLoop;
			}

			inline void ConsumeData(State *pState, AkUInt32 uSizeConsumed)
			{
				AKASSERT(uSizeConsumed <= pState->uSizeLeft);
				pState->pNextAddress += uSizeConsumed;
				pState->uSizeLeft -= uSizeConsumed;
				pState->uCurFileOffset += uSizeConsumed;
			}

			inline AKRESULT EnsureStreamIsRunning(State *pState)
			{
				return pState->pStream->Start();
			}

			AKRESULT Seek(State * pState, AkUInt32 in_uPosition, AkUInt16 in_uLoopCnt);

			AKRESULT RelocateMedia(State * pState, AkUInt8* in_pNewMedia, AkUInt8* in_pOldMedia);

			// Changes position of the stream for the next read
			AKRESULT SetStreamPosition(State * pState, AkUInt32 in_uPosition);

			// Read a buffer from the stream and return it. This is a low-level function.
			AKRESULT ReadStreamBuffer(State * pState, AkUInt8 ** out_ppBuffer, AkUInt32 * out_pSize);

			// Advance stream state using the provided buffer. This is a low-level function.
			AKRESULT CommitStreamBuffer(State * pState, AkUInt8 * in_pBuffer, AkUInt32 uSize);
		}
	}
}
