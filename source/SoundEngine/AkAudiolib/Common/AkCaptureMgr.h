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


#ifndef _AK_CAPTURE_MGR_H_
#define _AK_CAPTURE_MGR_H_
#include "AkFileParserBase.h"
#include <AK/Tools/Common/AkArray.h>
#include "AkMarkerStructs.h"

#define CAPTURE_MAX_WRITERS 4
#define	MAXCAPTURENAMELENGTH 256

class IAkCaptureWriter
{
public:
	virtual AKRESULT StartCapture(const AkOSChar* in_CaptureFileName, AkFileParser::Header * in_pHeader, AkUInt32 in_uHeaderSize) = 0;
	virtual AKRESULT StopCapture(AkFileParser::Header * in_pHeader, AkUInt32 in_uHeaderSize) = 0;
	virtual AKRESULT PassAudioData( void* in_pData, AkUInt32 in_size ) = 0;
	virtual AKRESULT WriteGenericData( void* in_pData, AkUInt32 in_size ) = 0;
	virtual void Destroy() = 0;
};

class IAkCaptureWriterFactory
{
public:
	virtual IAkCaptureWriter* CreateWriter() = 0;
};

class AkCaptureFile
{
public:
	AkCaptureFile(IAkCaptureWriter* in_pWriter);
	~AkCaptureFile();
	
	AKRESULT StartCapture(const AkOSChar* in_Name, 
		AkUInt32 in_uSampleRate, 
		AkUInt32 in_uBitsPerSample, 
		AkFileParser::FormatTag in_formatTag,
		AkChannelConfig in_channelConfig);
	
	AKRESULT StopCapture();
	AKRESULT PassSampleData(void* in_pData, AkUInt32 in_size);

	AKRESULT AddOutputCaptureMarker(const char* in_MarkerText);

private:
	AkFileParser::Header									 m_Header;
	
	typedef AkArray<AkAudioMarker, const AkAudioMarker&>	 MarkerList;
	MarkerList												 m_lstMarkers;
	AkUInt32												 m_uMarkerCount;
	
	AkUInt32												 m_uDataSize;
	IAkCaptureWriter*										 m_pWriter;

	AkUInt32	 AddMarkerData();
	AkUInt32	 ComputeMarkerTextSizes(AkUInt32* in_pTextSizes);
	AkUInt32	 PassCuesHeader();
	AkUInt32	 PassCues();
	AkUInt32	 PassLabelsHeader(AkUInt32 in_uTotalMarkerTextSize);
	AkUInt32	 PassLabels(AkUInt32* in_pTextSizes);
};

class AkCaptureMgr
{
public:
	static AK_FUNC( AkCaptureMgr*, Instance )();

	void SetWriter( IAkCaptureWriterFactory* in_pWriter );
	IAkCaptureWriterFactory* GetWriter();
	
	virtual AkCaptureFile* StartCapture(const AkOSChar* in_CaptureFileName, 
								AkUInt32 in_uSampleRate, 
								AkUInt32 in_uBitsPerSample, 
								AkFileParser::FormatTag in_formatTag,
								AkChannelConfig in_channelConfig);

private:

	AkCaptureMgr();
	~AkCaptureMgr();
	
	IAkCaptureWriterFactory *m_pWriterFactory;
};

#endif
