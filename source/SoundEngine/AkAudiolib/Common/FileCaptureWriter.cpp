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
#include "FileCaptureWriter.h"
#include <AK/Tools/Common/AkObject.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

FileCaptureWriter::FileCaptureWriter()
:m_pCaptureStream(NULL)
{
}

FileCaptureWriter::~FileCaptureWriter()
{
	if (m_pCaptureStream != NULL)
	{
		m_pCaptureStream->Destroy();
		m_pCaptureStream = NULL;
	}
}

AKRESULT FileCaptureWriter::StartCapture(const AkOSChar* in_CaptureFileName, AkFileParser::Header * in_pHeader, AkUInt32 in_uHeaderSize)
{
	if (m_pCaptureStream)
		return AK_Success;

	AKRESULT eResult;
	AkFileSystemFlags fsFlags;
	fsFlags.uCompanyID = AKCOMPANYID_AUDIOKINETIC;
	fsFlags.uCodecID = AKCODECID_PCM;
	fsFlags.bIsLanguageSpecific = false;
	fsFlags.pCustomParam = NULL;
	fsFlags.uCustomParamSize = 0;

	eResult = AK::IAkStreamMgr::Get()->CreateStd( in_CaptureFileName, &fsFlags, AK_OpenModeWriteOvrwr, m_pCaptureStream, true);
	if(eResult != AK_Success)
		return eResult;

#ifndef AK_OPTIMIZED
	m_pCaptureStream->SetStreamName( in_CaptureFileName );
#endif

	// write dummy WAVE header block out 
	// we will be going back to re-write the block when sizes have been resolved
	AkUInt32 uOutSize;
	eResult = m_pCaptureStream->Write(in_pHeader, in_uHeaderSize, true, AK_MAX_PRIORITY, 0, uOutSize);
	if ( eResult != AK_Success )
	{
		m_pCaptureStream->Destroy();
		m_pCaptureStream = NULL;
	}

	return eResult;
}

AKRESULT FileCaptureWriter::StopCapture(AkFileParser::Header * in_pHeader, AkUInt32 in_uHeaderSize)
{
	if (!m_pCaptureStream)
		return AK_Fail;

	// Seek back to start of stream 
	AKRESULT eResult = m_pCaptureStream->SetPosition(0, AK_MoveBegin, NULL);
	if(eResult == AK_Success)
	{
		AkUInt32 uOutSize;
		eResult = m_pCaptureStream->Write(in_pHeader, in_uHeaderSize, true, 0, 0, uOutSize);
	}

	// Kill stream, whether or not we succeeded to write updated header
	m_pCaptureStream->Destroy();
	m_pCaptureStream = NULL;
	return eResult;
}

AKRESULT FileCaptureWriter::WriteGenericData( void* in_pData, AkUInt32 in_size )
{
	AkUInt32 uOutSize;
	AKRESULT eResult = m_pCaptureStream->Write(in_pData, in_size, true, 0, 0,uOutSize);
	AKASSERT( uOutSize == in_size ); 
	return eResult;
}

AKRESULT FileCaptureWriter::PassAudioData( void* in_pData, AkUInt32 in_size )
{
	return WriteGenericData(in_pData, in_size);
}

void FileCaptureWriter::Destroy()
{
	AkDelete(AkMemID_Object, this);
}

FileCaptureWriterFactory *FileCaptureWriterFactory::Instance()
{
	static FileCaptureWriterFactory g_Factory;
	return &g_Factory;
}

IAkCaptureWriter* FileCaptureWriterFactory::CreateWriter()
{
	IAkCaptureWriter* pWriter = AkNew(AkMemID_Object, FileCaptureWriter());
	return pWriter;
}
