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

////////////////////////////////////////////////////////////////////////////////
//
// AkBytesMem.cpp
//
// IReadBytes / IWriteBytes implementation on a memory buffer.
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <AK/SoundEngine/Common/AkBytesMem.h>
#include <AK/Tools/Common/AkObject.h>

namespace AK
{
	//------------------------------------------------------------------------------
	// ReadBytesMem

	ReadBytesMem::ReadBytesMem()
		: m_cBytes(0)
		, m_pBytes(NULL)
		, m_cPos(0)
	{
	}

	ReadBytesMem::ReadBytesMem(
		const void * in_pBytes,
		AkInt32 in_cBytes
		)
	{
		Attach(in_pBytes, in_cBytes);
	}

	ReadBytesMem::~ReadBytesMem()
	{
	}

	// IReadBytes implementation

	bool ReadBytesMem::ReadBytes(
		void * in_pData,
		AkInt32 in_cBytes,
		AkInt32 & out_cRead
		)
	{
		if (m_pBytes == NULL)
			return false;

		AkInt32 cRemaining = m_cBytes - m_cPos;
		AkInt32 cToRead = AkMin(in_cBytes, cRemaining);

		memcpy(in_pData, m_pBytes + m_cPos, cToRead);

		m_cPos += in_cBytes;
		out_cRead = cToRead;

		return out_cRead == in_cBytes;
	}

	// Public methods

	void ReadBytesMem::Attach(
		const void * in_pBytes,
		AkInt32 in_cBytes
		)
	{
		m_cBytes = in_cBytes;
		m_pBytes = reinterpret_cast<const AkUInt8*>(in_pBytes);
		m_cPos = 0;
	}

	//------------------------------------------------------------------------------
	// WriteBytesMem

	WriteBytesMem::WriteBytesMem()
		: m_cBytes( 0 )
		, m_pBytes( NULL )
		, m_cPos( 0 )
		, m_pool( AK_INVALID_POOL_ID )
	{
	}

	WriteBytesMem::~WriteBytesMem()
	{
		if ( m_pBytes )
			AkFree( m_pool, m_pBytes );
	}

	bool WriteBytesMem::WriteBytes( const void * in_pData, AkInt32 in_cBytes, AkInt32& out_cWritten )
	{
		AkInt32 cPos = m_cPos;
		AkInt32 cNewPos = cPos + in_cBytes;

		if ((m_cBytes >= cNewPos) || Grow(cNewPos))
		{
			AKPLATFORM::AkMemCpy(m_pBytes + cPos, (void *)in_pData, in_cBytes);

			m_cPos = cNewPos;
			out_cWritten = in_cBytes;

			return true;
		}
		else
			return false;
	}

	bool WriteBytesMem::Reserve(AkInt32 in_cBytes)
	{
		return (m_cBytes >= in_cBytes) || Grow(in_cBytes);
	}

	bool WriteBytesMem::Grow(AkInt32 in_cBytes)
	{
		static const int kGrowBy = 1024;

		AkInt32 cBytesOld = m_cBytes;

		int cGrowBlocks = (in_cBytes + kGrowBy - 1) / kGrowBy;
		m_cBytes = cGrowBlocks * kGrowBy;

		AkUInt8 * pNewBytes = (AkUInt8 *)AkRealloc(m_pool, m_pBytes, m_cBytes);
		if (pNewBytes)
		{
			m_pBytes = pNewBytes;
		}
		else
		{
			// reverting changes and returning false.
			m_cBytes = cBytesOld;
			return false;
		}

		return true;
	}

	AkInt32 WriteBytesMem::Count() const 
	{ 
		return m_cPos; 
	}

	void WriteBytesMem::SetCount(
		AkInt32 in_cBytes) 
	{ 
		m_cPos = in_cBytes;
	}

	AkUInt8 * WriteBytesMem::Bytes() const 
	{ 
		return m_pBytes;
	}

	AkInt32 WriteBytesMem::Size() const
	{
		return m_cBytes;
	}
	
	AkUInt8 * WriteBytesMem::Detach()
	{
		AkUInt8* pByte = m_pBytes;

		m_pBytes = NULL;
		m_cBytes = 0;
		m_cPos = 0;

		return pByte;
	}

	void WriteBytesMem::Clear()
	{ 
		m_cPos = 0; 
	}

	void WriteBytesMem::SetMemPool( AkMemPoolId in_pool )
	{
		if( m_pool != AK_INVALID_POOL_ID && m_pBytes != NULL )
		{
			AkFree( m_pool, m_pBytes );
			m_pBytes = NULL;
		}
		m_pool = in_pool;
	}
}
