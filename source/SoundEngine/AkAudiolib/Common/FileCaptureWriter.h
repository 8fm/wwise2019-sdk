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

#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>
#include "AkCaptureMgr.h"
#include <AK/SoundEngine/Common/IAkStreamMgr.h>

class FileCaptureWriterFactory : public IAkCaptureWriterFactory
{
public:
	static FileCaptureWriterFactory* Instance();
	virtual IAkCaptureWriter* CreateWriter();
};

class FileCaptureWriter AK_FINAL : public IAkCaptureWriter
{
public:
	FileCaptureWriter();
	~FileCaptureWriter();

	virtual AKRESULT StartCapture(const AkOSChar* in_CaptureFileName, AkFileParser::Header * in_pHeader, AkUInt32 in_uHeaderSize);
	virtual AKRESULT StopCapture(AkFileParser::Header * in_pHeader, AkUInt32 in_uHeaderSize);
	virtual AKRESULT PassAudioData( void* in_pData, AkUInt32 in_size );
	virtual AKRESULT WriteGenericData( void* in_pData, AkUInt32 in_size );
	virtual void Destroy();

private:
	AK::IAkStdStream *m_pCaptureStream;			// Output stream for capture
};
