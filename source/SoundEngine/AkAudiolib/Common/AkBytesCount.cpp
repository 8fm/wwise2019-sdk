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

#include <AK/SoundEngine/Common/AkBytesCount.h>
#include <AK/Tools/Common/AkObject.h>

namespace AK
{
	//------------------------------------------------------------------------------
	// ReadBytesSkip

	ReadBytesSkip::ReadBytesSkip()
		: m_cBytes(0)
		, m_pBytes(NULL)
		, m_cPos(0)
	{
	}

	ReadBytesSkip::ReadBytesSkip(
		const void * in_pBytes,
		AkInt32 in_cBytes
		)
	{
		Attach(in_pBytes, in_cBytes);
	}

	ReadBytesSkip::~ReadBytesSkip()
	{
	}

	// IReadBytes implementation

	bool ReadBytesSkip::ReadBytes(
		void * in_pData,
		AkInt32 in_cBytes,
		AkInt32 & out_cRead
		)
	{
		if (m_pBytes == NULL)
			return false;

		AkInt32 cRemaining = m_cBytes - m_cPos;
		AkInt32 cToRead = AkMin(in_cBytes, cRemaining);

		m_cPos += in_cBytes;
		out_cRead = cToRead;

		return out_cRead == in_cBytes;
	}

	// Public methods

	void ReadBytesSkip::Attach(
		const void * in_pBytes,
		AkInt32 in_cBytes
		)
	{
		m_cBytes = in_cBytes;
		m_pBytes = reinterpret_cast<const AkUInt8*>(in_pBytes);
		m_cPos = 0;
	}

	AkInt32 ReadBytesSkip::Count()
	{
		return m_cPos;
	}

	//------------------------------------------------------------------------------
	// WriteBytesCount

	WriteBytesCount::WriteBytesCount()
		: m_cPos( 0 )
	{}

	WriteBytesCount::~WriteBytesCount()
	{}

	bool WriteBytesCount::WriteBytes( const void * in_pData, AkInt32 in_cBytes, AkInt32& out_cWritten )
	{
		m_cPos += in_cBytes;
		return true;
	}

	bool WriteBytesCount::Reserve(AkInt32 in_cBytes)
	{
		return false;
	}

	bool WriteBytesCount::Grow(AkInt32 in_cBytes)
	{
		return false;
	}

	AkInt32 WriteBytesCount::Count() const 
	{ 
		return m_cPos; 
	}

	void WriteBytesCount::SetCount(AkInt32 in_cBytes) 
	{ 
		m_cPos = in_cBytes;
	}

	AkUInt8 * WriteBytesCount::Bytes() const 
	{ 
		return NULL; 
	}
	
	AkUInt8 * WriteBytesCount::Detach()
	{
		return NULL;
	}

	void WriteBytesCount::Clear()
	{ 
		m_cPos = 0; 
	}
}
