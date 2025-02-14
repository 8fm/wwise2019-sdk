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
// AkBankReader.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include "AkBankReader.h"
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/SoundEngine/Common/IAkStreamMgr.h>
#include <AK/Tools/Common/AkObject.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

extern AkInitSettings	g_settings;

// The bank manager does allocate its own buffer to read IO, and this buffer must be aligned too.
#define AK_BANK_READER_BUFFER_ALIGNMENT (32)

CAkBankReader::CAkBankReader()
	: m_pStreamReadBuffer(NULL)
	, m_uCurrentBufferIdx(0)
	, m_uStreamBufferSize(0)
	, m_pStreamBufferMemory(NULL)
	, m_uDeviceBlockSize(0)
	, m_pUserReadBuffer(NULL)
	, m_pStream(NULL)
	, m_pMemoryFile(NULL)
	, m_bIsInitDone(false)
{
	Reset();
}

CAkBankReader::~CAkBankReader()
{
}

AKRESULT CAkBankReader::Init()
{
	AKASSERT(!m_bIsInitDone);
	if (m_bIsInitDone)
	{
		return AK_Fail;
	}

	m_uStreamBufferSize = 0;
	Reset();

	m_bIsInitDone = true;

	m_fThroughput = AK_DEFAULT_BANK_THROUGHPUT;
	m_priority = AK_DEFAULT_BANK_IO_PRIORITY;

	return AK_Success;
}

AKRESULT CAkBankReader::Term()
{
	Reset();
	if (m_pStreamBufferMemory)
	{
		AkFalign(AkMemID_Object, m_pStreamBufferMemory);
		m_pStreamBufferMemory = NULL;
	}
	m_bIsInitDone = false;
	return AK_Success;
}

void CAkBankReader::Reset()
{
	AKASSERT(!m_pUserReadBuffer);
	for (AkUInt32 i = 0; i < NumStreamBuffers; ++i)
	{
		m_pStreamBuffers[i] = NULL;
		m_uBufferBytesRemaining[i] = 0;
	}
	m_pStreamReadBuffer = NULL;
}

AKRESULT CAkBankReader::SetBankLoadIOSettings(AkReal32 in_fThroughput, AkPriority in_priority)
{
	if (in_fThroughput < 0 || in_priority < AK_MIN_PRIORITY || in_priority > AK_MAX_PRIORITY)
	{
		AKASSERT(!"Invalid bank I/O settings");
		return AK_InvalidParameter;
	}

	m_fThroughput = in_fThroughput;
	m_priority = in_priority;
	return AK_Success;
}

// Set file based on preloaded memory - just set our memory buffers to the provided file, and the bytes remaining to file size
AKRESULT CAkBankReader::SetFile(const void* in_pInMemoryFile, AkUInt32 in_ui32FileSize)
{
	AKASSERT(m_pStream == NULL && m_pMemoryFile == NULL);

	m_pMemoryFile = in_pInMemoryFile;
	m_uBufferBytesRemaining[0] = in_ui32FileSize;
	m_uCurrentBufferIdx = 0;
	return AK_Success;
}

// Set file based on provided filename - load up a stream, and finalize buffer initialization given the stream
AKRESULT CAkBankReader::SetFile(const char* in_pszFilename, AkUInt32 in_uFileOffset, void* in_pCookie)
{
	AKASSERT(m_pStream == NULL && m_pMemoryFile == NULL);
	
	// Try open the file in the language specific directory.
	AkFileSystemFlags fsFlags;
	fsFlags.uCompanyID = AKCOMPANYID_AUDIOKINETIC;
	fsFlags.uCodecID = AKCODECID_BANK;
	fsFlags.bIsLanguageSpecific = true;
	fsFlags.pCustomParam = in_pCookie;
	fsFlags.uCustomParamSize = 0;

	AkOSChar* pszFileName;
	CONVERT_CHAR_TO_OSCHAR(in_pszFilename, pszFileName);
	AKRESULT eResult = AK::IAkStreamMgr::Get()->CreateStd(pszFileName, &fsFlags, AK_OpenModeRead, m_pStream,
														  true);  // Force synchronous open.
	if (eResult != AK_Success)
	{
		// Perhaps file was not found. Try open the bank in the common (non-language specific) section.
		fsFlags.bIsLanguageSpecific = false;
		eResult = AK::IAkStreamMgr::Get()->CreateStd(pszFileName, &fsFlags, AK_OpenModeRead, m_pStream,
													 true);  // Force synchronous open.
	}

	if (eResult == AK_Success)
	{
#ifndef AK_OPTIMIZED
		// If profiling, name the stream with the file name.
		m_pStream->SetStreamName(pszFileName);
#endif
		eResult = FinalizeStreamInit(in_uFileOffset);
	}
	return eResult;
}

// Set file based on provided fileId - load up a stream, and finalize buffer initialization given the stream
AKRESULT CAkBankReader::SetFile(AkFileID in_FileID, AkUInt32 in_uFileOffset, AkUInt32 in_codecID, void* in_pCookie, bool in_bIsLanguageSpecific /*= true*/)
{
	AKASSERT(m_pStream == NULL && m_pMemoryFile == NULL);

	// Try open the file in the language specific directory.
	AkFileSystemFlags fsFlags;
	fsFlags.uCompanyID = AKCOMPANYID_AUDIOKINETIC;
	fsFlags.uCodecID = in_codecID;
	fsFlags.bIsLanguageSpecific = in_bIsLanguageSpecific;
	fsFlags.pCustomParam = in_pCookie;
	fsFlags.uCustomParamSize = 0;

	AKRESULT eResult = AK::IAkStreamMgr::Get()->CreateStd(in_FileID, &fsFlags, AK_OpenModeRead, m_pStream,
														  true);  // Force synchronous open.
	if (eResult != AK_Success && in_bIsLanguageSpecific)
	{
		// Perhaps file was not found. Try open the bank in the common (non-language-specific) section.
		fsFlags.bIsLanguageSpecific = false;
		eResult = AK::IAkStreamMgr::Get()->CreateStd(in_FileID, &fsFlags, AK_OpenModeRead, m_pStream,
													 true);  // Force synchronous open.
	}
	if (eResult == AK_Success)
	{
		// If profiling, compute a name for the stream.
#ifndef AK_OPTIMIZED
		AkOSChar szStreamName[32];
		AK_OSPRINTF(szStreamName, 32, AKTEXT("FileID: %u"), in_FileID);
		m_pStream->SetStreamName(szStreamName);
#endif
		eResult = FinalizeStreamInit(in_uFileOffset);
	}
	return eResult;
}

// Helper function to finish up preparation of a new stream, e.g. set up appropriate buffers
AKRESULT CAkBankReader::FinalizeStreamInit(AkUInt32 in_uFileOffset)
{
	// Adapt to low-level I/O block size.
	m_uDeviceBlockSize = m_pStream->GetBlockSize();
	if (m_uDeviceBlockSize == 0)
	{
		AKASSERT(!"Invalid IO block size");
		return AK_Fail;
	}

	// Reallocate buffer in the extreme case where it is smaller than the device block size.
	if (m_uStreamBufferSize < m_uDeviceBlockSize)
	{
		if (m_pStreamBufferMemory)
		{
			AkFalign(AkMemID_Object, m_pStreamBufferMemory);
		}

		m_uStreamBufferSize = AkMax(m_uDeviceBlockSize, g_settings.uBankReadBufferSize);
		m_pStreamBufferMemory = AkMalign(AkMemID_Object, m_uStreamBufferSize * NumStreamBuffers,
			AK_BANK_READER_BUFFER_ALIGNMENT);  // Must be aligned on 32 bytes since it will be used to stream RVL_OS I/O
		if (!m_pStreamBufferMemory)
		{
			return AK_InsufficientMemory;
		}
	}
	
	// Initialize the streamBuffer segments to point to the appropriate location in m_pStreamBufferMemory
	for (AkUInt32 i = 0; i < NumStreamBuffers; ++i)
	{
		m_pStreamBuffers[i] = (void*)((uintptr_t)m_pStreamBufferMemory + m_uStreamBufferSize * i);
	}

	AKRESULT eResult = AK_Success;
	if (in_uFileOffset != 0)
	{
		AkInt64 lRealOffset = 0;
		eResult = m_pStream->SetPosition(in_uFileOffset, AK_MoveBegin, &lRealOffset);
		if (eResult == AK_Success)
		{
			AkUInt32 correction = in_uFileOffset - (AkUInt32)(lRealOffset);

			AkUInt32 uSkipped;
			eResult = Skip(correction, uSkipped);

			if (uSkipped != correction)
				eResult = AK_Fail;
		}
	}
	return eResult;
}

AKRESULT CAkBankReader::CloseFile()
{
	if (m_pStream != NULL)
	{
		// Note. Unsafe close, because we are sure there is no pending IO.
		m_pStream->Destroy();
		m_pStream = NULL;
	}
	m_pMemoryFile = NULL;
	Reset();
	return AK_Success;
}

const void* CAkBankReader::GetData(AkUInt32 in_uSize)
{
	AKASSERT(m_pStreamReadBuffer != NULL || m_pMemoryFile != NULL);
	AKASSERT(!m_pUserReadBuffer);

	if (in_uSize <= m_uBufferBytesRemaining[m_uCurrentBufferIdx])
	{
		m_uBufferBytesRemaining[m_uCurrentBufferIdx] -= in_uSize;
		const void* pOut = m_pMemoryFile ? m_pMemoryFile : m_pStreamReadBuffer;
		if (m_pMemoryFile)
		{
			m_pMemoryFile = (const void*)((uintptr_t)(m_pMemoryFile)+in_uSize);
		}
		else
		{
			m_pStreamReadBuffer = (void*)((uintptr_t)(m_pStreamReadBuffer)+in_uSize);
		}
		return pOut;
	}
	else
	{
		AKASSERT(m_pMemoryFile == NULL); // we should never be trying to enter here if we're using a memory file - we should only be asked for as many bytes as we had in the memory file

		// Note (WG-16340): Need to respect AK_BANK_READER_BUFFER_ALIGNMENT. This can occur if we are reading
		// a big chunk, that is not aligned to AK_BANK_READER_BUFFER_ALIGNMENT in the bank file, like
		// hierarchy chunks and the like.
		AkUInt32 uAllocSize = in_uSize;
		AkUInt32 uAlignOffset = 0;
		if (m_uBufferBytesRemaining[m_uCurrentBufferIdx] % AK_BANK_READER_BUFFER_ALIGNMENT)
		{
			// Allocate just a bit more in order to let FillData() handle the misalignment
			uAllocSize += (AK_BANK_READER_BUFFER_ALIGNMENT - 1);
			uAlignOffset = AK_BANK_READER_BUFFER_ALIGNMENT - (m_uBufferBytesRemaining[m_uCurrentBufferIdx] % AK_BANK_READER_BUFFER_ALIGNMENT);
		}

		m_pUserReadBuffer = AkMalign(AkMemID_Object, uAllocSize, AK_BANK_READER_BUFFER_ALIGNMENT);
		if (m_pUserReadBuffer)
		{
			AkUInt32 uSizeRead;
			// Ask reader to fill data in m_pUserReadBuffer, offset with the alignment constraint.
			void* pOut = (AkUInt8*)m_pUserReadBuffer + uAlignOffset;
			if (FillData(pOut, in_uSize, uSizeRead) == AK_Success && uSizeRead == in_uSize)
			{
				return pOut;
			}
			else
			{
				// Free user buffer now.
				AkFalign(AkMemID_Object, m_pUserReadBuffer);
				m_pUserReadBuffer = NULL;
			}
		}

		return m_pUserReadBuffer;
	}
}

void CAkBankReader::ReleaseData()
{
	if (m_pUserReadBuffer)
	{
		AkFalign(AkMemID_Object, m_pUserReadBuffer);
		m_pUserReadBuffer = NULL;
	}
}

AKRESULT CAkBankReader::FillData(void* in_pDest, AkUInt32 in_uBytesToRead, AkUInt32& out_ruNumBytesRead)
{
	AKRESULT eResult = AK_Success;
	AkUInt32 uTotalBytesRead = 0;
	AkUInt32 uNumBytesRemaining = in_uBytesToRead;
	
	// Each bit we either memcpy out until the current memory buffer is empty,
	// Or we attempt to load in more data from disk (or flip to a preloaded buffer)
	while (uTotalBytesRead < in_uBytesToRead)
	{
		AKASSERT(uTotalBytesRead + uNumBytesRemaining == in_uBytesToRead);

		// If we have bytes remaining in memory that we can refer to, memcpy them out
		if (m_uBufferBytesRemaining[m_uCurrentBufferIdx])
		{
			AKASSERT(m_pStreamReadBuffer != NULL || m_pMemoryFile != NULL);

			void* pDest = (void*)((uintptr_t)in_pDest + uTotalBytesRead);
			const void* pSrc = m_pMemoryFile ? m_pMemoryFile : m_pStreamReadBuffer;
			AkUInt32 uNumToRead = AkMin(m_uBufferBytesRemaining[m_uCurrentBufferIdx], uNumBytesRemaining);
			AKPLATFORM::AkMemCpy(pDest, pSrc, uNumToRead);
			
			if (m_pMemoryFile)
			{
				m_pMemoryFile = (const void*)((uintptr_t)(m_pMemoryFile)+uNumToRead);
			}
			else
			{
				m_pStreamReadBuffer = (void*)((uintptr_t)(m_pStreamReadBuffer)+uNumToRead);
			}
			uTotalBytesRead += uNumToRead;
			uNumBytesRemaining -= uNumToRead;
			m_uBufferBytesRemaining[m_uCurrentBufferIdx] -= uNumToRead;
		}
		else // We ran out of memory, so either advance the buffer or read in more data
		{
			if (m_pStream)
			{
				// If the stream buffer we want to advance to has some memory allocated, then there is some read
				// that is pending (or complete) on it so finish the read, and advance the currentStreamBuffer
				AkUInt32 uNextStreamBufferIdx = GetNextBufferIndex(m_uCurrentBufferIdx);
				if (m_uBufferBytesRemaining[uNextStreamBufferIdx])
				{
					if (m_pStream->WaitForPendingOperation() == AK_StmStatusError)
					{
						AKASSERT(!"IO Error");
						eResult = AK_BankReadError;
						break;
					}

					// Now that the read is complete, update values related to reading from the streaming buffers
					m_uCurrentBufferIdx = uNextStreamBufferIdx;
					m_pStreamReadBuffer = (void*)m_pStreamBuffers[uNextStreamBufferIdx];
				}

				// since we just finished a read, let's try to queue up a new one if we think we're still chewing through the stream bit-by-bit.
				// it is not possible to look into what future reads will be, so this is a bit of a guess.
				AkUInt32 uActualSizeRead = 0;
				if (uNumBytesRemaining < m_uStreamBufferSize)
				{
					// Preload stream into the "next" buffer
					uNextStreamBufferIdx = GetNextBufferIndex(m_uCurrentBufferIdx);
					AkUInt32 uSizeToRead = AkUInt32(m_uStreamBufferSize / m_uDeviceBlockSize) * m_uDeviceBlockSize;
					eResult = m_pStream->Read(m_pStreamBuffers[uNextStreamBufferIdx], uSizeToRead, false, m_priority,
						(uSizeToRead / m_fThroughput),  // deadline (s) = size (bytes) / throughput (bytes/s).
						uActualSizeRead);
					if (eResult != AK_Success)
					{
						AKASSERT(!"IO error");
						break;
					}

					// If the stream is going to read some data, set the number of bytes remaining on our "next" buffer to that value
					if (uActualSizeRead > 0)
					{
						m_uBufferBytesRemaining[uNextStreamBufferIdx] = uActualSizeRead;
					}
				}
				// If we're being asked to do a large read, don't proceed until until we have copied out everything else we have loaded (or pre-loaded)
				// That is, let the loop run to the next iteration(s), where memcpy's will be performed, and then we'll eventually do a big read here
				else if (m_uBufferBytesRemaining[m_uCurrentBufferIdx] == 0)
				{
					AkUInt32 uSizeToRead = AkUInt32(uNumBytesRemaining / m_uDeviceBlockSize) * m_uDeviceBlockSize;
					eResult = m_pStream->Read((void*)((uintptr_t)in_pDest + uTotalBytesRead), uSizeToRead, true, m_priority,
						(uSizeToRead / m_fThroughput),  // deadline (s) = size (bytes) / throughput (bytes/s).
						uActualSizeRead);
					if (eResult != AK_Success || m_pStream->GetStatus() != AK_StmStatusCompleted)
					{
						AKASSERT(!"IO error");
						eResult = AK_BankReadError;
						break;
					}

					// advance the number of bytes read accordingly
					uTotalBytesRead += uActualSizeRead;
					uNumBytesRemaining -= uActualSizeRead;
				}

				// If we did not read any bytes and we know that we have no more bytes left to consume in current buffer, then we're done
				if (uActualSizeRead == 0 && m_uBufferBytesRemaining[m_uCurrentBufferIdx] == 0)
				{
					break;
				}
			}
			else
			{
				// No more bytes to read, so we're done
				break;
			}
		}
	}
	out_ruNumBytesRead = uTotalBytesRead;
	return eResult;
}

AKRESULT CAkBankReader::FillDataEx(void* in_pBufferToFill, AkUInt32 in_uSizeToRead)
{
	AkUInt32 ulSizeRead = 0;

	AKRESULT eResult = FillData(in_pBufferToFill, in_uSizeToRead, ulSizeRead);
	if (eResult == AK_Success && in_uSizeToRead != ulSizeRead)
	{
		eResult = AK_BankReadError;
	}
	return eResult;
}

AKRESULT CAkBankReader::Skip(AkUInt32 in_uNumBytesToSkip, AkUInt32& out_ruSizeSkipped)
{
	AKRESULT eResult = AK_Success;
	AkUInt32 uTotalBytesSkipped = 0;
	AkUInt32 uNumBytesRemaining = in_uNumBytesToSkip;

	// Each bit we either advance the pointer through the current memory buffer until it is exhausted,
	// Or we advance the pointer through the preloaded memory buffer, or we just attempt to move the stream position forward
	while (uTotalBytesSkipped < in_uNumBytesToSkip)
	{
		AKASSERT(uTotalBytesSkipped + uNumBytesRemaining == in_uNumBytesToSkip);

		// If we have bytes remaining in memory that we can refer to, step over them as much as possible
		if (m_uBufferBytesRemaining[m_uCurrentBufferIdx])
		{
			AkUInt32 uNumToSkip = AkMin(m_uBufferBytesRemaining[m_uCurrentBufferIdx], uNumBytesRemaining);
			uTotalBytesSkipped += uNumToSkip;
			uNumBytesRemaining -= uNumToSkip;

			if (m_pMemoryFile)
			{
				m_pMemoryFile = (const void*)((uintptr_t)(m_pMemoryFile)+uNumToSkip);
			}
			else
			{
				m_pStreamReadBuffer = (void*)((uintptr_t)(m_pStreamReadBuffer)+uNumToSkip);
			}
			m_uBufferBytesRemaining[m_uCurrentBufferIdx] -= uNumToSkip;
		}
		else
		{
			// We ran out of memory in the current buffer, so either advance the buffer or read in more data
			if (m_pStream)
			{
				// We need to update the stream position (which mandates no active read)
				// or we are about to update the pointers to refer to the data being loaded in.
				// Either way, we need to wait for the stream to finish any pending read, first.
				if (m_pStream->WaitForPendingOperation() == AK_StmStatusError)
				{
					AKASSERT(!"IO Error");
					eResult = AK_BankReadError;
					break;
				}

				// If the next streaming buffer has some memory loaded, advance pointer into that - next iteration of loop will move pointer forward as needed
				AkUInt32 uNextStreamBufferIdx = GetNextBufferIndex(m_uCurrentBufferIdx);
				if (m_uBufferBytesRemaining[uNextStreamBufferIdx])
				{
					m_uCurrentBufferIdx = uNextStreamBufferIdx;
					m_pStreamReadBuffer = (void*)m_pStreamBuffers[uNextStreamBufferIdx];
				}
				else // If the next buffer is empty (e.g. there actually wasn't a pending read)
					// then we need to just set a new position on the stream - next read will kick off from here
				{
					// Since we opened the stream with unbuffered IO flag, we need to
					// monitor the number of bytes actually skipped (might have snapped to
					// a sector boundary).
					AkInt64 lRealOffset;
					AkInt64 lNumBytesToSkip = { (AkInt64)uNumBytesRemaining };
					eResult = m_pStream->SetPosition(lNumBytesToSkip, AK_MoveCurrent, &lRealOffset);
					if (eResult != AK_Success)
					{
						AKASSERT(!"Bank load: End of file unexpectedly reached");
						break;
					}
					AKASSERT(lRealOffset < AK_UINT_MAX);
					uTotalBytesSkipped += (AkUInt32)lRealOffset;
					uNumBytesRemaining -= (AkUInt32)lRealOffset;
				}
			}
			else
			{
				// No more bytes to skip, so we're done
				break;
			}
		}
	}
	out_ruSizeSkipped = uTotalBytesSkipped;
	return eResult;
}

// Small helper to flip active buffer index
AkUInt32 CAkBankReader::GetNextBufferIndex(AkUInt32 streamBufferIdx)
{
	return (streamBufferIdx == 0) ? 1 : 0;
}
