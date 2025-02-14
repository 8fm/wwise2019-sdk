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

#include "stdafx.h"
#include "AkSrcSimpleStream.h"

#include <AK/SoundEngine/Common/IAkStreamMgr.h>
#include <AK/Tools/Common/AkObject.h>

AKRESULT CAkSrcSimpleStream::Read(
	AkUInt32        in_uReqSize,        ///< Requested read size
	void *&         out_pBuffer,        ///< Buffer address
	AkUInt32 &      out_uSize           ///< The size that was actually read
	)
{
	if (AK_EXPECT_FALSE(m_bStitchConsumed))
	{
		if (m_pSrcFile->m_pStitchBuffer)
		{
			AkFree(AkMemID_Processing, m_pSrcFile->m_pStitchBuffer);
			m_pSrcFile->m_pStitchBuffer = NULL;
			m_pSrcFile->m_uNumBytesBuffered = 0;
		}
		m_bStitchConsumed = false;
	}

	if (m_pSrcFile->m_pStitchBuffer == NULL)
	{
		if (in_uReqSize <= m_pSrcFile->m_ulSizeLeft)
		{
			out_pBuffer = m_pSrcFile->m_pNextAddress;
			out_uSize = in_uReqSize;

			m_pSrcFile->ConsumeData(in_uReqSize);

			return AK_DataReady;
		}
		else
		{
			m_pSrcFile->m_pStitchBuffer = (AkUInt8*)AkAlloc(AkMemID_Processing, in_uReqSize);
			if (m_pSrcFile->m_pStitchBuffer == NULL)
				return AK_InsufficientMemory;

			AkMemCpy(m_pSrcFile->m_pStitchBuffer, m_pSrcFile->m_pNextAddress, m_pSrcFile->m_ulSizeLeft);

			m_pSrcFile->m_uNumBytesBuffered = m_pSrcFile->m_ulSizeLeft;
			m_pSrcFile->ConsumeData(m_pSrcFile->m_ulSizeLeft);

			return AK_NoDataReady;
		}
	}
	else
	{
		if (m_pSrcFile->m_uNumBytesBuffered < in_uReqSize)
		{
			AkUInt32 uCopySize = AkMin(m_pSrcFile->m_ulSizeLeft, in_uReqSize - m_pSrcFile->m_uNumBytesBuffered);
			if (uCopySize)
			{
				AkMemCpy(m_pSrcFile->m_pStitchBuffer + m_pSrcFile->m_uNumBytesBuffered, m_pSrcFile->m_pNextAddress, uCopySize);

				m_pSrcFile->m_uNumBytesBuffered += uCopySize;
				m_pSrcFile->ConsumeData(uCopySize);
			}
		}

		if (m_pSrcFile->m_uNumBytesBuffered == in_uReqSize)
		{
			out_pBuffer = m_pSrcFile->m_pStitchBuffer;
			out_uSize = in_uReqSize;
			m_bStitchConsumed = true;

			return AK_DataReady;
		}
		else
		{
			return AK_NoDataReady;
		}
	}
}

AkUInt64 CAkSrcSimpleStream::GetPosition()
{
	return m_pSrcFile->m_uCurFileOffset;
}

AKRESULT CAkSrcSimpleStream::SetPosition(
	AkInt64         in_iPosition     ///< Position, from beginning
	)
{
	if (in_iPosition >= m_pSrcFile->m_uCurFileOffset
		&& in_iPosition < (m_pSrcFile->m_uCurFileOffset + m_pSrcFile->m_ulSizeLeft))
	{
		// Simulate a seek
		m_pSrcFile->ConsumeData((AkUInt32)(in_iPosition - m_pSrcFile->m_uCurFileOffset));
		return AK_Success;
	}
	else
	{
		AKRESULT res = m_pSrcFile->SetStreamPosition((AkUInt32)in_iPosition);
		m_pSrcFile->m_ulSizeLeft = 0;
		return res;
	}		
}

AKRESULT CAkSrcSimpleStream::GetFileSize(
	AkUInt64 & out_uSize
	)
{
	AkStreamInfo info;
	m_pSrcFile->m_pStream->GetInfo(info);
	out_uSize = info.uSize;

	return AK_Success;
}

void CAkSrcSimpleStream::SetBuffer(AkUInt8 * in_pBuffer)
{
	// Set next pointer.
	m_pSrcFile->m_uCurFileOffset = m_pSrcFile->m_ulFileOffset;
	m_pSrcFile->m_ulFileOffset += m_pSrcFile->m_ulSizeLeft;
	m_pSrcFile->m_pNextAddress = in_pBuffer + m_pSrcFile->m_uiCorrection;

	// Update size left.
	m_pSrcFile->m_ulSizeLeft -= m_pSrcFile->m_uiCorrection;
}
