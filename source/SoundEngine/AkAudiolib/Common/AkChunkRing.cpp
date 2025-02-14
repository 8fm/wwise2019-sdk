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

#include "AkChunkRing.h"
#include <AK/Tools/Common/AkObject.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

AKRESULT AkChunkRing::Init( AkMemPoolId in_PoolId, AkUInt32 in_ulSize )
{
	m_pStart = (AkUInt8 *)AkAlloc( in_PoolId, in_ulSize );
	if ( !m_pStart )
		return AK_Fail;

	m_pRead = m_pStart;
	m_pWrite = m_pStart;

	m_pEnd = m_pStart + in_ulSize;
	m_pVirtualEnd = m_pEnd;

	return AK_Success;
}

void AkChunkRing::Term( AkMemPoolId in_PoolId )
{
	if ( m_pStart )
	{
		AkFree( in_PoolId, m_pStart );
		m_pStart = NULL;
	}
}

void * AkChunkRing::BeginWrite( AkInt32 in_lSize )
{
	in_lSize += 4;    // so that pRead == pWrite never means a full buffer.
	in_lSize &= ~0x03; 

	m_writeLock.Lock();

	AkUInt8 * pRead = m_pRead; // ensure consistency if m_pRead changes while we're doing stuff

	if ( pRead > m_pWrite ) // simple case : contiguous free space
	{
		// NOTE: in theory the empty space goes to m_pEnd if pRead == m_pVirtualEnd, but
		// this breaks our lock-free model.

		if ( in_lSize < ( pRead - m_pWrite ) )
			return m_pWrite;
	}
	else
	{
		if ( in_lSize < ( m_pEnd - m_pWrite ) ) // fits in the remaining space before the end ?
			return m_pWrite;

		if ( pRead != m_pWrite )
		{
			if ( pRead == m_pVirtualEnd )
			{
				m_writeLock.Unlock();
				return NULL;
			}
		}

		if (in_lSize < (pRead - m_pStart)) // fits in the space before the read ptr ?
			return m_pStart;
	}

	m_writeLock.Unlock();
	return NULL;
}

void AkChunkRing::EndWrite( void * in_pWritePtr, AkInt32 in_lSize, AkEvent& in_hToSignal)
{
	AKASSERT( in_pWritePtr );
	AKASSERT( in_lSize != 0 ); // algorithm depends on this

	in_lSize += 3;
	in_lSize &= ~0x03; // Data is always aligned to 4 bytes.

	AkUInt8 * pOldWrite = m_pWrite;	
	m_readLock.Lock();

	bool bWasEmpty = IsEmpty();

	m_pWrite = (AkUInt8 *) in_pWritePtr + in_lSize;

	if ( in_pWritePtr == m_pStart ) // wrapped around ? set the new virtual end to last position
	{
		if ( pOldWrite != in_pWritePtr ) // exclude starting condition
			m_pVirtualEnd = pOldWrite;
	}
	else if ( m_pWrite > m_pVirtualEnd ) // went past virtual end ? move it
	{
		m_pVirtualEnd = m_pWrite + 4;
	}
	
	m_readLock.Unlock();

	if (m_bSignalOnWrite && bWasEmpty)
		AKPLATFORM::AkSignalEvent(in_hToSignal);

	m_writeLock.Unlock();	
}
