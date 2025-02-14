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
#include "AkCaptureMgr.h"
#include "string.h"
#include <AK/Tools/Common/AkObject.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include "FileCaptureWriter.h"
#include "PlatformAudiolibDefs.h"

AkCaptureMgr* AkCaptureMgr::Instance()
{
	// Classic singleton
	static AkCaptureMgr sTheCaptureMgr;	
	return &sTheCaptureMgr;
}

AkCaptureMgr::AkCaptureMgr():
m_pWriterFactory(NULL)
{
}

AkCaptureMgr::~AkCaptureMgr()
{
}

void AkCaptureMgr::SetWriter( IAkCaptureWriterFactory* in_pWriter )
{
	m_pWriterFactory = in_pWriter;
}

IAkCaptureWriterFactory* AkCaptureMgr::GetWriter()
{
	if (m_pWriterFactory == NULL)
	{
		//If there is no writer factory, default to disk
		m_pWriterFactory = FileCaptureWriterFactory::Instance();
	}
	return m_pWriterFactory;
}

AkCaptureFile* AkCaptureMgr::StartCapture(const AkOSChar* in_CaptureFileName, AkUInt32 in_uSampleRate, AkUInt32 in_uBitsPerSample, AkFileParser::FormatTag in_formatTag, AkChannelConfig in_channelConfig)
{
	IAkCaptureWriter *pWriter = GetWriter()->CreateWriter();
	if (pWriter == NULL)
		return NULL;

	AkCaptureFile *pFile = AkNew(AkMemID_Object, AkCaptureFile(pWriter));
	if (pFile == NULL)
	{
		pWriter->Destroy();
		return NULL;
	}
	
	AKRESULT eResult = pFile->StartCapture(in_CaptureFileName, in_uSampleRate, in_uBitsPerSample, in_formatTag, in_channelConfig);
	if (eResult != AK_Success)
	{
		AkDelete(AkMemID_Object, pFile);
		pWriter->Destroy();
		pFile = NULL;
	}

	return pFile;
}

AkCaptureFile::AkCaptureFile(IAkCaptureWriter* in_pWriter)
	: m_uMarkerCount(0)
	, m_uDataSize(0)
	, m_pWriter(in_pWriter)
{
	memset((void*)&m_Header, 0, sizeof(AkWAVEFileHeaderStd));
}

AkCaptureFile::~AkCaptureFile()
{
	for(AkUInt32 i = 0; i < m_lstMarkers.Length(); ++i)
		AkFree(AkMemID_Object, m_lstMarkers[i].strLabel);
	
	m_lstMarkers.Term();
}

AKRESULT AkCaptureFile::StartCapture(const AkOSChar* in_Name, 
									 AkUInt32 in_uSampleRate, 
									 AkUInt32 in_uBitsPerSample, 
									 AkFileParser::FormatTag in_formatTag,
									 AkChannelConfig in_channelConfig)
{
	size_t uHeaderSize = AkFileParser::FillHeader(in_uSampleRate, in_formatTag, AkFileParser::WAV, in_channelConfig, m_Header);
	return m_pWriter->StartCapture(in_Name, &m_Header, (AkUInt32)uHeaderSize);
}

AKRESULT AkCaptureFile::StopCapture()
{
	if (m_pWriter == NULL)
		return AK_Success;	//Not capturing, so not an error.

	AkUInt32 uMarkerDataSize = 0;
	if(!m_lstMarkers.IsEmpty())
		uMarkerDataSize += AddMarkerData();

	m_Header.wav.RIFF.dwChunkSize = sizeof(AkWAVEFileHeaderStd) + m_uDataSize + uMarkerDataSize - 8;
	m_Header.wav.data.dwChunkSize = m_uDataSize;

	AKRESULT eResult = m_pWriter->StopCapture(&m_Header, sizeof(AkWAVEFileHeaderStd));
	m_pWriter->Destroy();
	m_pWriter = NULL;

	AkDelete(AkMemID_Object, this);
	return eResult;
}

AKRESULT AkCaptureFile::PassSampleData( void* in_pData, AkUInt32 in_size )
{
	if (m_pWriter == NULL)
		return AK_Fail;
	AKRESULT eResult = m_pWriter->PassAudioData(in_pData, in_size);
	if (eResult == AK_Success)
		m_uDataSize += in_size;	

	return eResult;
}

AKRESULT AkCaptureFile::AddOutputCaptureMarker(const char* in_MarkerText)
{
	if(m_pWriter == NULL)
		return AK_Fail;

	AkUInt32 uCurrentMarkerPos = m_uDataSize / m_Header.wav.waveFmtExt.nBlockAlign;

	// If current marker is at the same position as the last marker, add the current string to the other one with a \n between the two
	if(!m_lstMarkers.IsEmpty() && (uCurrentMarkerPos == m_lstMarkers.Last().dwPosition))
	{
		AkAudioMarker* lastMarker = &m_lstMarkers.Last();

		const AkUInt32 uOldSize = (AkUInt32) strlen(lastMarker->strLabel);
			
		// Allocate new buffer to include old and new string and \0 at the end
		const AkUInt32 uTotalBufferSize = (AkUInt32) (uOldSize + (strlen(in_MarkerText) * sizeof(char)) + 1);
		char* pBufferConcatenatedText = (char*) AkAlloc(AkMemID_Object, uTotalBufferSize);

		if(pBufferConcatenatedText == NULL)
			return AK_InsufficientMemory;

		AKPLATFORM::SafeStrCpy(pBufferConcatenatedText, lastMarker->strLabel, uTotalBufferSize);

		AkFree(AkMemID_Object, lastMarker->strLabel);

		AKPLATFORM::SafeStrCat(pBufferConcatenatedText, in_MarkerText, uTotalBufferSize);
		lastMarker->strLabel = pBufferConcatenatedText;

		return AK_Success;
	}

	AkAudioMarker marker;
	marker.dwPosition = uCurrentMarkerPos;
	marker.dwIdentifier = ++m_uMarkerCount;

	const AkUInt32 uMarkerTextSize = (AkUInt32) (strlen(in_MarkerText) * sizeof(char)) + 1; // Includes '\0'
	char* pBufferText = (char*) AkAlloc(AkMemID_Object, uMarkerTextSize);

	//// If we can't allocate memory, fail
	if(pBufferText == NULL)
		return AK_InsufficientMemory;

	memcpy(pBufferText, in_MarkerText, uMarkerTextSize);
	marker.strLabel = pBufferText;

	if(!m_lstMarkers.AddLast(marker))
	{
		AkFree(AkMemID_Object, pBufferText);
		return AK_InsufficientMemory;
	}

	return AK_Success;
}

AkUInt32 AkCaptureFile::AddMarkerData()
{
	const AkUInt32 numberOfMarkers = m_lstMarkers.Length();
	AkUInt32* pMarkerTextSizes = (AkUInt32*) AkAlloca(sizeof(AkUInt32) * numberOfMarkers);
	AkUInt32  uMarkerDataSize = 0;

	// Write cues
	uMarkerDataSize += PassCuesHeader();
	uMarkerDataSize += PassCues();

	// Write labels
	uMarkerDataSize += PassLabelsHeader(ComputeMarkerTextSizes(pMarkerTextSizes));
	uMarkerDataSize += PassLabels(pMarkerTextSizes);

	return uMarkerDataSize;
}

AkUInt32 AkCaptureFile::ComputeMarkerTextSizes(AkUInt32* in_pTextSizes)
{
	const AkUInt32 numberOfMarkers = m_lstMarkers.Length();
	AkUInt32 uTotalTextSize = 0;

	for(AkUInt32 i = 0; i < numberOfMarkers; ++i)
	{
		in_pTextSizes[i] = (AkUInt32) strlen(m_lstMarkers[i].strLabel) + 1; // Include '\0'

		// If the current text's length is odd, add 1 to it's size because we need to add a padding byte when inserting labels
		if(in_pTextSizes[i] & 1)
			uTotalTextSize += 1;

		uTotalTextSize += in_pTextSizes[i];
	}

	return uTotalTextSize;
}

AkUInt32 AkCaptureFile::PassCuesHeader()
{
	const AkUInt32 numberOfMarkers = m_lstMarkers.Length();
	AkUInt32 uPassedDataSize = 0;

	// Add Cue chunk
	AkChunkHeader cueChunk;
	cueChunk.ChunkId = cueChunkId;
	cueChunk.dwChunkSize = (sizeof(CuePoint) * numberOfMarkers) + sizeof(AkUInt32); // We exclude ChunkId and chunkSize(according to specification)
	m_pWriter->WriteGenericData((void*)&cueChunk, sizeof(AkChunkHeader));

	uPassedDataSize += sizeof(AkChunkHeader);

	// Add number of cue points(which is part of chunk header)
	m_pWriter->WriteGenericData((void*)&numberOfMarkers, sizeof(AkUInt32));

	uPassedDataSize += sizeof(AkUInt32);

	return uPassedDataSize;
}

AkUInt32 AkCaptureFile::PassCues()
{
	const AkUInt32 numberOfMarkers = m_lstMarkers.Length();
	AkUInt32 uPassedDataSize = 0;

	for(AkUInt32 i = 0; i < numberOfMarkers; ++i)
	{
		// Create cue point
		CuePoint cue;
		cue.dwIdentifier = m_lstMarkers[i].dwIdentifier;
		cue.dwPosition = m_lstMarkers[i].dwPosition;
		cue.fccChunk = dataChunkId;
		cue.dwChunkStart = 0;
		cue.dwBlockStart = 0;
		cue.dwSampleOffset = m_lstMarkers[i].dwPosition;
		
		m_pWriter->WriteGenericData((void*)&cue, sizeof(CuePoint));

		uPassedDataSize += sizeof(CuePoint);
	}

	return uPassedDataSize;
}

AkUInt32 AkCaptureFile::PassLabelsHeader(AkUInt32 in_uTotalMarkerTextSize)
{
	const AkUInt32 numberOfMarkers = m_lstMarkers.Length();
	AkUInt32 uPassedDataSize = 0;

	// Create associated data list header
	AkChunkHeader listChunk;
	listChunk.ChunkId = LISTChunkId;

	// We ignore the size of the pointer to the variable-sized marker text because we only need the length of the text itself
	listChunk.dwChunkSize = (numberOfMarkers * (sizeof(LabelChunk) - sizeof(char*) )) + in_uTotalMarkerTextSize + sizeof(AkFourcc);
	m_pWriter->WriteGenericData((void*)&listChunk, sizeof(AkChunkHeader));
	uPassedDataSize += sizeof(AkChunkHeader);

	// 'adtl' fourcc is part of the header
	AkFourcc adtlId = adtlChunkId;
	m_pWriter->WriteGenericData((void*)&adtlId, sizeof(AkFourcc));
	uPassedDataSize += sizeof(AkFourcc);

	return uPassedDataSize;
}

AkUInt32 AkCaptureFile::PassLabels(AkUInt32* in_pTextSizes)
{
	const AkUInt32 numberOfMarkers = m_lstMarkers.Length();
	AkUInt32 uPassedDataSize = 0;

	for(AkUInt32 i = 0; i < numberOfMarkers; ++i)
	{
		// Create label('labl') chunks
		LabelChunk label;
		label.fccChunk = lablChunkId;
		label.dwChunkSize = sizeof(AkUInt32) + in_pTextSizes[i];
		label.dwIdentifier = m_lstMarkers[i].dwIdentifier;
		label.strVariableSizeText = m_lstMarkers[i].strLabel;
		
		// Pass all header data except the text first
		m_pWriter->WriteGenericData((void*)&label, sizeof(LabelChunk) - sizeof(char*));
		uPassedDataSize += (sizeof(LabelChunk)- sizeof(char*));

		// Finally, pass the text itself
		m_pWriter->WriteGenericData((void*)label.strVariableSizeText, in_pTextSizes[i] * sizeof(char));
		uPassedDataSize += (in_pTextSizes[i] * sizeof(char));

		// If number of characters is odd, we need to add a null padding byte
		if(in_pTextSizes[i] & 1)
		{
			AkUInt8 nullByte = 0;
			m_pWriter->WriteGenericData((void*)&nullByte, sizeof(AkUInt8));
			uPassedDataSize += sizeof(AkUInt8);
		}
	}
	
	return uPassedDataSize;
}
