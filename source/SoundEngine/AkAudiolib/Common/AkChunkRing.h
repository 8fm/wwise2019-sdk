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

#ifndef _AK_CHUNK_RING_H_
#define _AK_CHUNK_RING_H_

#include <AK/Tools/Common/AkLock.h>
#include <AK/Tools/Common/AkAutoLock.h>

#include <AK/Tools/Common/AkArray.h>

// Ring Buffer class. This is a fairly task-specific class, it is optimized for the case where:
// - Reading from a single thread.
// - Writing from multiple threads.
// - Data is made of chunks that need to be contiguous in memory (for example, structs).
// - Data chunks are of varying size (reader MUST be able to tell the size of a chunk by peeking in it).
// - Data chunks are multiple of align size.
// Due to the 'contiguous chunk' requirement, there is usually a bit of wasted space near the end of the
// buffer (equal to m_pEnd - m_pVirtualEnd) -- but this is nothing in comparison to the overhead of a
// linked list and fixed block memory allocator managing the same buffer in this case.

class AkChunkRing
{
public:
	AkChunkRing()
		: m_pStart( NULL )
		,m_bSignalOnWrite(true)
	{
	}

	~AkChunkRing()
	{
		AKASSERT( m_pStart == NULL );
	}

	AKRESULT Init( AkMemPoolId in_PoolId, AkUInt32 in_ulSize );
	void Term( AkMemPoolId in_PoolId );

	bool IsEmpty()
	{
		return m_pRead == m_pWrite;
	}

	void Reset()
	{
		if (IsEmpty())
		{
			m_pRead = m_pWrite = m_pStart;
		}
	}

	void SignalOnWrite(bool in_bSignal) { m_bSignalOnWrite = in_bSignal; }

	void * BeginRead()
	{
		AkAutoLock<CAkLock> lock( m_readLock );
		AKASSERT( !IsEmpty() );

		return ( m_pRead == m_pVirtualEnd ) ? m_pStart : m_pRead;
	}

	void EndRead( void * in_pReadPtr, AkUInt32 in_ulSize )
	{
		in_ulSize += 3;
		in_ulSize &= ~0x03; // Data is always aligned to 4 bytes.

		m_pRead = (AkUInt8 *) in_pReadPtr + in_ulSize;
		AKASSERT( m_pRead <= m_pVirtualEnd );
	}

	// Start writing. Input size is for reservation purpose; final size (passed to EndWrite)
	// needs to be equal or less. If this method returns a buffer, you NEED to call EndWrite
	// as the write lock is being held.
	void *	BeginWrite( AkInt32 in_lSize );

	// Stop writing. Need to pass pointer returned by BeginWrite().
	void EndWrite( void * in_pWritePtr, AkInt32 in_lSize, AkEvent& in_hToSignal );

	AkForceInline void LockWrite() { m_writeLock.Lock(); }
	AkForceInline void UnlockWrite() { m_writeLock.Unlock(); }

private:
	AkUInt8 * volatile m_pRead;			// Read position in buffer
	AkUInt8 * volatile m_pWrite;		// Write position in buffer

	AkUInt8 * volatile m_pStart;		// Memory start of buffer
	AkUInt8 * volatile m_pVirtualEnd;	// Memory end of used buffer -- changes each time 'write' wraps around
	AkUInt8 * volatile m_pEnd;			// Memory end of buffer

	CAkLock m_readLock;					// Read lock; only necessary because m_pWrite and m_pVirtualEnd need to be 
										// changed 'together' atomically from the reader's point of view.
	CAkLock m_writeLock;
	bool m_bSignalOnWrite;
};

#endif
