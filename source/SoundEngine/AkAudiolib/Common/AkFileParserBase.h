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
#ifndef _AK_FILE_PARSER_BASE_H_
#define _AK_FILE_PARSER_BASE_H_

#include "AkMath.h"
#include <AK/Tools/Common/AkMonitorError.h>
#include <AK/SoundEngine/Common/AkSpeakerConfig.h>
#include <AK/SoundEngine/Common/AkSpeakerVolumes.h>

//-----------------------------------------------------------------------------
// Constants.
//-----------------------------------------------------------------------------

// Note: Both RIFF and RIFX exist (they inform us on the endianness of the file),
// but this parser uses the platform defined AkPlatformRiffChunkId (which translates
// to one of them according to the endianness of the platform).
static const AkFourcc RIFXChunkId = AkmmioFOURCC('R', 'I', 'F', 'X');
static const AkFourcc RIFFChunkId = AkmmioFOURCC('R', 'I', 'F', 'F');
static const AkFourcc WAVEChunkId = AkmmioFOURCC('W', 'A', 'V', 'E');
static const AkFourcc OGGSChunkId = AkmmioFOURCC('O', 'g', 'g', 'S');
///
static const AkFourcc fmtChunkId  = AkmmioFOURCC('f', 'm', 't', ' ');
// AK-specific chunks.
static const AkFourcc analysisDataChunkId = AkmmioFOURCC('a', 'k', 'd', ' ');

// Other, standard chunks.
static const AkFourcc dataChunkId = AkmmioFOURCC('d', 'a', 't', 'a');
static const AkFourcc cueChunkId  = AkmmioFOURCC('c', 'u', 'e', ' ');
static const AkFourcc LISTChunkId = AkmmioFOURCC('L', 'I', 'S', 'T');
static const AkFourcc adtlChunkId = AkmmioFOURCC('a', 'd', 't', 'l');
static const AkFourcc lablChunkId = AkmmioFOURCC('l', 'a', 'b', 'l');
static const AkFourcc smplChunkId = AkmmioFOURCC('s', 'm', 'p', 'l');

static const AkUInt8 HAVE_FMT	= 0x01;
static const AkUInt8 HAVE_DATA	= 0x02;
static const AkUInt8 HAVE_CUES	= 0x04;
static const AkUInt8 HAVE_SMPL	= 0x08;
static const AkUInt8 HAVE_SEEK	= 0x10;

#define AK_WAVE_FORMAT_PCM			1
#define	AK_WAVE_FORMAT_ADPCM		2
#define	AK_WAVE_FORMAT_PTADPCM		0x8311  // Chosen to be unique among AK_WAVE_FORMAT_* and WAVE_FORMAT_* (see mmreg.h, Microsoft's multimedia registration)
#define	AK_WAVE_FORMAT_EXTENSIBLE	0xFFFE

//-----------------------------------------------------------------------------
// Structs.
//-----------------------------------------------------------------------------
#pragma pack(push, 1)
struct AkChunkHeader
{
	AkFourcc	ChunkId;
	AkUInt32	dwChunkSize;
};

// This is a copy of WAVEFORMATEX
struct WaveFormatEx
{	
	AkUInt16  	wFormatTag;
	AkUInt16  	nChannels;
	AkUInt32  	nSamplesPerSec;
	AkUInt32  	nAvgBytesPerSec;
	AkUInt16  	nBlockAlign;
	AkUInt16  	wBitsPerSample;
	AkUInt16    cbSize;	// size of extra chunk of data, after end of this struct
};

// WAVEFORMATEXTENSIBLE without the format GUID
// Codecs that require format-specific chunks should extend this structure.
struct WaveFormatExtensible : public WaveFormatEx
{
	AkUInt16    wSamplesPerBlock;
	AkUInt32    uChannelConfig;		// Serialized AkChannelConfig

	inline AkChannelConfig GetChannelConfig() const
	{
		AkChannelConfig channelConfig;
		channelConfig.Deserialize(uChannelConfig);
		return channelConfig;
	}
};
static_assert(sizeof(WaveFormatExtensible) == 24, "Incorrect padding for WaveFormatExtensible");

struct LabelChunk
{
	AkFourcc	fccChunk;
	AkUInt32	dwChunkSize;
	AkUInt32	dwIdentifier;
	char*		strVariableSizeText;
};

struct AkWAVEFileHeaderWem
{
	AkChunkHeader		RIFF;		// AkFourcc	ChunkId: FOURCC('RIFF')
	// AkUInt32	dwChunkSize: Size of file (in bytes) - 8
	AkFourcc			uWAVE;		// FOURCC('WAVE')
	AkChunkHeader		fmt;		// AkFourcc	ChunkId: FOURCC('fmt ')
	// AkUInt32	dwChunkSize: Total size (in bytes) of fmt chunk's content
	WaveFormatExtensible	waveFmtExt;	// AkUInt16	wFormatTag: 0x0001 for linear PCM etc.
	// AkUInt16	nChannels: Number of channels (1=mono, 2=stereo etc.)
	// AkUInt32	nSamplesPerSec: Sample rate (e.g. 44100)
	// AkUInt32	nAvgBytesPerSec: nSamplesPerSec * nBlockAlign
	// AkUInt16 nBlockAlign: nChannels * nBitsPerSample / 8 for PCM
	// AkUInt16 wBitsPerSample: 8, 16, 24 or 32
	// AkUInt16 cbSize:	Size of extra chunk of data, after end of this struct
	// AkUInt16 wSamplesPerBlock: unused except to pad cbSize...
	// AkUInt32	dwChannelMask
	AkChunkHeader		data;		// AkFourcc	ChunkId: FOURCC('data')
	// AkUInt32	dwChunkSize: Total size (in bytes) of data chunk's content
	// Sample data goes after this..
};
struct AkWAVEFileHeaderStd
{
	AkChunkHeader		RIFF;			// AkFourcc	ChunkId: FOURCC('RIFF')
	// AkUInt32	dwChunkSize: Size of file (in bytes) - 8
	AkFourcc			uWAVE;			// FOURCC('WAVE')
	AkChunkHeader		fmt;			// AkFourcc	ChunkId: FOURCC('fmt ')
	// AkUInt32	dwChunkSize: Total size (in bytes) of fmt chunk's content
	WaveFormatExtensible	waveFmtExt;	// AkUInt16	wFormatTag: 0x0001 for linear PCM etc.
	// AkUInt16	nChannels: Number of channels (1=mono, 2=stereo etc.)
	// AkUInt32	nSamplesPerSec: Sample rate (e.g. 44100)
	// AkUInt32	nAvgBytesPerSec: nSamplesPerSec * nBlockAlign
	// AkUInt16 nBlockAlign: nChannels * nBitsPerSample / 8 for PCM
	// AkUInt16 wBitsPerSample: 8, 16, 24 or 32
	// AkUInt16 cbSize:	Size of extra chunk of data, after end of this struct
	// AkUInt16 wSamplesPerBlock: unused except to pad cbSize...
	// AkUInt32	dwChannelMask
	struct GUID
	{
		AkUInt32 Data1;
		AkUInt16 Data2;
		AkUInt16 Data3;
		AkUInt8 Data4[8];
	} SubFormat;						//WAV GUID
	AkChunkHeader		data;			// AkFourcc	ChunkId: FOURCC('data')
	// AkUInt32	dwChunkSize: Total size (in bytes) of data chunk's content
	// Sample data goes after this..
};

#pragma pack(pop)

struct CuePoint
{
	AkUInt32   dwIdentifier;
	AkUInt32   dwPosition;
	AkFourcc  fccChunk;       // Unused. Wav lists are not supported.
	AkUInt32   dwChunkStart;   // Unused. Wav lists are not supported.
	AkUInt32   dwBlockStart;   // Unused. Wav lists are not supported.
	AkUInt32   dwSampleOffset;
};

struct LabelCuePoint
{
	AkUInt32   dwCuePointID;
	char	  strLabel[1]; // variable-size string
};

struct Segment 
{
	AkUInt32    dwIdentifier;
	AkUInt32    dwLength;
	AkUInt32    dwRepeats;
};

struct SampleChunk
{
	AkUInt32     dwManufacturer;
	AkUInt32     dwProduct;
	AkUInt32     dwSamplePeriod;
	AkUInt32     dwMIDIUnityNote;
	AkUInt32     dwMIDIPitchFraction;
	AkUInt32     dwSMPTEFormat;
	AkUInt32     dwSMPTEOffset;
	AkUInt32     dwSampleLoops;
	AkUInt32     cbSamplerData;
};

struct SampleLoop
{
	AkUInt32     dwIdentifier;
	AkUInt32     dwType;
	AkUInt32     dwStart;
	AkUInt32     dwEnd;
	AkUInt32     dwFraction;
	AkUInt32     dwPlayCount;
};

class CAkMarkers;

namespace AkFileParser
{
	struct FormatInfo
	{
		AkUInt32				uFormatSize;	// WaveFormatExtensible size.
		WaveFormatExtensible *	pFormat;		// Pointer to format data (WaveFormatExtensible or extended struct).
	};

	struct AnalysisData
	{
		// Max loudness.
		AkReal32 		fLoudnessNormalizationGain;	// Normalization gain due to loudness analysis. Used iif "Enable Normalization" is on.
		AkReal32 		fDownmixNormalizationGain;	// Normalization gain due to downmix. Used all the time.

		// Envelope.
		AkUInt32		uNumEnvelopePoints;	// Number of entries in envelope table.
		AkReal32 		fEnvelopePeak;		// Largest envelope point (dB).
		EnvelopePoint	arEnvelope[1];		// Variable array of envelope points.
	};

	struct AnalysisDataChunk
	{
		AnalysisDataChunk(): uDataSize(0), pData(NULL) {}

		AkUInt32		uDataSize;	// Size of the data pointed by pData.
		AnalysisData *	pData;		// Data. 
	};

	struct SeekInfo
	{
		AkUInt32	uSeekChunkSize;	// (Format-specific) seek table size.
		void *		pSeekTable;		// (Format-specific) seek table content.
	};		

	extern AKRESULT Parse(
		const void *	in_pvBuffer,				// Buffer to be parsed.
		AkUInt32		in_ulBufferSize,			// Buffer size.
		FormatInfo &	out_pFormatInfo,			// Returned audio format info.
		CAkMarkers *	out_pMarkers,				// Markers. NULL if not wanted. (Mandatory for markers).
		AkUInt32 *		out_pulLoopStart,			// Loop start position (in sample frames).
		AkUInt32 *		out_pulLoopEnd,				// Loop end position (in sample frames).
		AkUInt32 *		out_pulDataSize,			// Size of data portion of the file.
		AkUInt32 *		out_pulDataOffset,			// Offset in file to the data.
		AnalysisDataChunk * out_pAnalysisData,		// Returned analysis data chunk (pass NULL if not wanted).
		SeekInfo *		out_pSeekTableInfo,			// (Format-specific) seek table info (pass NULL if not wanted).
		bool			in_bStandardChannelFormat = false // Is the channel config member a standard bitfield or the Wwise format?
		);

	// choose appropriate monitoring message given result returned by above function 
	extern AK::Monitor::ErrorCode ParseResultToMonitorMessage(AKRESULT);

	enum FormatTag { Int16 = 1, Float = 3 };	//From WAV format definitions	
	enum FormatType { WAV = 0, WEM = 1 };

	union Header
	{
		AkWAVEFileHeaderWem wem;
		AkWAVEFileHeaderStd wav;
	};

	// Fills standard WAVEFORMATEXTENSIBLE GUID according to channel configuration type.
	inline void FillGuid(
		AkChannelConfig in_channelConfig,
		AkWAVEFileHeaderStd::GUID & out_guid
		)
	{
		if (in_channelConfig.eConfigType == AK_ChannelConfigType_Ambisonic)
		{
			// Special subformat GUID for Ambisonics.
			//static const GUID s_ambisonic = { 0x00000001, 0x0721, 0x11D3, 0x86, 0x44, 0xC8, 0xC1, 0xCA, 0x00, 0x00, 0x00 };
			out_guid.Data1 = 0x00000001;
			out_guid.Data2 = 0x0721;
			out_guid.Data3 = 0x11D3;
			out_guid.Data4[0] = 0x86;
			out_guid.Data4[1] = 0x44;
			out_guid.Data4[2] = 0xC8;
			out_guid.Data4[3] = 0xC1;
			out_guid.Data4[4] = 0xCA;
			out_guid.Data4[5] = 0x00;
			out_guid.Data4[6] = 0x00;
			out_guid.Data4[7] = 0x00;
		}
		else
		{
			// Standard PCM
			out_guid.Data1 = 0x00000001;
			out_guid.Data2 = 0x0000;
			out_guid.Data3 = 0x0010;
			out_guid.Data4[0] = 0x80;
			out_guid.Data4[1] = 0x00;
			out_guid.Data4[2] = 0x00;
			out_guid.Data4[3] = 0xaa;
			out_guid.Data4[4] = 0x00;
			out_guid.Data4[5] = 0x38;
			out_guid.Data4[6] = 0x9b;
			out_guid.Data4[7] = 0x71;
		}
	}

	// Returns header size.
	inline size_t FillHeader(
		AkUInt32 in_uSampleRate,
		FormatTag in_eFormatTag,
		FormatType in_eFormatType,
		AkChannelConfig in_channelConfig,
		Header & out_header
		)
	{
		AkUInt32 uNumChannels = in_channelConfig.uNumChannels;
		AkUInt32 uBytesPerSample = ( in_eFormatTag == Int16 ) ? sizeof(AkInt16) : sizeof(AkReal32);
		AkUInt32 uBlockAlign = uNumChannels * uBytesPerSample;

		if (in_eFormatType == WAV)
		{
#if defined(AK_ENDIANNESS_LITTLE)
			out_header.wav.RIFF.ChunkId = RIFFChunkId;
#else
			out_header.wav.RIFF.ChunkId = RIFXChunkId; // Only byte swap platforms
#endif
			out_header.wav.RIFF.dwChunkSize = 0XFFFFFFFF;//sizeof(AkWAVEFileHeader) + in_uDataSize - 8;
			out_header.wav.uWAVE = WAVEChunkId;
			out_header.wav.fmt.ChunkId = fmtChunkId;
			out_header.wav.fmt.dwChunkSize = sizeof(WaveFormatExtensible) + 16; //The format GUID is 16 bytes long
			out_header.wav.waveFmtExt.wFormatTag = AK_WAVE_FORMAT_EXTENSIBLE;
			out_header.wav.waveFmtExt.nChannels = (AkUInt16)uNumChannels;
			out_header.wav.waveFmtExt.nSamplesPerSec = in_uSampleRate;
			out_header.wav.waveFmtExt.nAvgBytesPerSec = in_uSampleRate * uBlockAlign;
			out_header.wav.waveFmtExt.nBlockAlign = (AkUInt16)uBlockAlign;
			out_header.wav.waveFmtExt.wBitsPerSample = (AkUInt16)uBytesPerSample*8;
			out_header.wav.waveFmtExt.cbSize = 22;
			out_header.wav.waveFmtExt.wSamplesPerBlock = 0;
			out_header.wav.waveFmtExt.uChannelConfig = in_channelConfig.uChannelMask;

			FillGuid(in_channelConfig, out_header.wav.SubFormat);

			out_header.wav.data.ChunkId = dataChunkId;
			out_header.wav.data.dwChunkSize = 0;

			return sizeof(AkWAVEFileHeaderStd);
		}
		else
		{
			AKASSERT(in_eFormatType == WEM);
#if defined(AK_ENDIANNESS_LITTLE)
			out_header.wem.RIFF.ChunkId = RIFFChunkId;
#else
			out_header.wem.RIFF.ChunkId = RIFXChunkId; // Only byte swap platforms
#endif
			out_header.wem.RIFF.dwChunkSize = 0XFFFFFFFF;//sizeof(AkWAVEFileHeader) + in_uDataSize - 8;
			out_header.wem.uWAVE = WAVEChunkId;
			out_header.wem.fmt.ChunkId = fmtChunkId;
			out_header.wem.fmt.dwChunkSize = sizeof(WaveFormatExtensible);
			out_header.wem.waveFmtExt.wFormatTag = AK_WAVE_FORMAT_EXTENSIBLE;
			out_header.wem.waveFmtExt.nChannels = (AkUInt16)uNumChannels;
			out_header.wem.waveFmtExt.nSamplesPerSec = in_uSampleRate;
			out_header.wem.waveFmtExt.nAvgBytesPerSec = in_uSampleRate * uBlockAlign;
			out_header.wem.waveFmtExt.nBlockAlign = (AkUInt16)uBlockAlign;
			out_header.wav.waveFmtExt.wBitsPerSample = (AkUInt16)uBytesPerSample * 8;
			out_header.wem.waveFmtExt.cbSize = 0;
			out_header.wem.waveFmtExt.wSamplesPerBlock = 0;
			out_header.wem.waveFmtExt.uChannelConfig = in_channelConfig.Serialize();
			out_header.wem.data.ChunkId = dataChunkId;
			out_header.wem.data.dwChunkSize = 0;

			return sizeof(AkWAVEFileHeaderWem);
		}
	}

	//
	// Ambisonics format conversion.
	//

	// Get FuMa index from ACN index (of a complete configuration).
	static const AkUInt8 s_uAcnToFumaIndices[] =
	{
		0,	// W
		2,	// Y
		3,	// Z
		1,	// X
		8,	// V
		6,
		4,
		5,
		7,
		15,
		13,
		11,
		9,
		10,
		12,
		14
	};

	/* use s_uFumaConfigsToAcnOrdering[15] instead
	static const AkUInt8 s_uFumaToAcnIndices[] =
	*/

#define AK_MAX_FUMA_CHANNELS	(16)

	/// Channel swapping of FuMa to ACN ordering for supported .amb configurations.
	static const AkUInt8 s_uFumaConfigsToAcnOrdering[AK_MAX_FUMA_CHANNELS][AK_MAX_FUMA_CHANNELS] =
	{
		{ 0 },														// 1: W #0
		{ 0, 1 },													// 2: WY <- WY #.5#0
		{ 0, 2, 1 },												// 3: WYX <- WXY #1#0
		{ 0, 3, 1, 2 },												// 4: WYZX <- WXYZ #1#1	- 1st order full-sphere
		{ 0, 2, 1, 4, 3 },											// 5: WYXVU <- WXYUV #2#0
		{ 0, 3, 1, 2, 5, 4 },										// 6: WYZXVU <- WXYZUV #2#1
		{ 0, 2, 1, 4, 3, 6, 5 },									// 7: WYXVUQP <- WXYUVPQ #3#0
		{ 0, 3, 1, 2, 5, 4, 7, 6 },									// 8: WYZXVUQP <- WXYZUVPQ #3#1
		{ 0, 3, 1, 2, 6, 7, 5, 8, 4 },								// 9: WYZXVTRSU <- WXYZRSTUV #2#2 - 2nd order full-sphere
		{ 0, },														// 10 channels not supported
		{ 0, 3, 1, 2, 6, 7, 5, 8, 4, 10, 9 },						// 11: WYZXVTRSUQP <- WXYZRSTUVPQ #3#2
		{ 0, },														// 12 channels not supported
		{ 0, },														// 13 channels not supported
		{ 0, },														// 14 channels not supported
		{ 0, },														// 15 channels not supported
		{ 0, 3, 1, 2, 6, 7, 5, 8, 4, 12, 13, 11, 14, 10, 15, 9 }	// 16: WYZXVTRSUQMKLNP <- WXYZRSTUVKLMNOPQ #3#3 - 3rd order full-sphere
	};

	// ACN index to ACN number of all configurations (including incomplete/mixed-orders). Full-sphere configs thus yield to identity.
	static const AkUInt8 s_uAcnConfigsToNumber[][AK_MAX_FUMA_CHANNELS] =
	{
		{ 0 },		// 1: W
		{ 0, 1 },	// 2: WY
		{ 0, 1, 3 },// 3: WYX
		{ 0, 1, 2, 3 },// 4: WYZX
		{ 0, 1, 3, 4, 8 },// 5: WYXVU
		{ 0, 1, 2, 3, 4, 8 },// 6: WYZXVU
		{ 0, 1, 3, 4, 8, 9, 15 },// 7: WYXVUQP
		{ 0, 1, 2, 3, 4, 8, 9, 15 },// 8: WYZXVUQP
		{ 0, 1, 2, 3, 4, 5, 6, 7, 8 },// 9: WYZXVTRSU
		{ 0, },// 10: not supported
		{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15 },// 11: WYZXVTRSUQP
		{ 0, },// 12: not supported
		{ 0, },// 13: not supported
		{ 0, },// 14: not supported
		{ 0, },// 15: not supported
		{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }// 16: WYZXVTRSUQMKLNP
	};

	// Gains for converting MaxN to SN3D normalization, indexed by ACN number.
	// All scaled down by 1/sqrt(2) to avoid clipping.
	static const AkReal32 s_fToSN3D[AK_MAX_FUMA_CHANNELS] =
	{
		1.f,									// W
		ONE_OVER_SQRT_OF_TWO,					// Y
		ONE_OVER_SQRT_OF_TWO,					// Z
		ONE_OVER_SQRT_OF_TWO,					// X
		0.61237243569579452454932101867648f,	// V
		0.61237243569579452454932101867648f,	// T
		ONE_OVER_SQRT_OF_TWO,					// R
		0.61237243569579452454932101867648f,	// S
		0.61237243569579452454932101867648f,	// U
		0.55901699437494742410229341718282f,	// Q
		0.52704627669472988866648225740545f,	// O
		0.59628479399994391904244631166167f,	// M
		ONE_OVER_SQRT_OF_TWO,					// K
		0.59628479399994391904244631166167f,	// L
		0.52704627669472988866648225740545f,	// N
		0.55901699437494742410229341718282f		// P
	};

	inline bool IsFuMaCompatible(
		AkChannelConfig in_channelConfig
	)
	{
		return in_channelConfig.eConfigType == AK_ChannelConfigType_Ambisonic && in_channelConfig.uNumChannels <= AK_MAX_FUMA_CHANNELS;
	}

	// Get conversion matrix from ACN-SN3D (used in Wwise run-time) to FuMa-MaxN
	// Supports full-sphere configs only.
	inline void GetWwiseToFuMaConversionMx(
		AkChannelConfig in_channelConfig,
		AK::SpeakerVolumes::MatrixPtr out_mx	// Returned mixing matrix. Must be allocated prior to calling this function with the size returned by AK::SpeakerVolumes::Matrix::GetRequiredSize() for the desired configuration.
		)
	{
		AKASSERT(IsFuMaCompatible(in_channelConfig));
		AK::SpeakerVolumes::Matrix::Zero(out_mx, in_channelConfig.uNumChannels, in_channelConfig.uNumChannels);

		for (AkUInt32 uAcn = 0; uAcn < in_channelConfig.uNumChannels; uAcn++)
		{
			AK::SpeakerVolumes::Matrix::GetChannel(out_mx, uAcn, in_channelConfig.uNumChannels)[s_uAcnToFumaIndices[uAcn]] = 1.f / (s_fToSN3D[uAcn] * ROOTTWO);
		}
	}

	// Convert FuMa-MaxN ambisonics convention to Wwise (ACN-SN3D).
	// All scaled down by 1/sqrt(2) to avoid clipping. Should be compensated by other means.
	template <class T>
	inline void ConvertFuMaToWwiseInterleaved(
		T * io_pInterleaved,
		AkChannelConfig in_channelConfig,
		AkUInt32 in_uNumFrames
		)
	{
		AKASSERT(IsFuMaCompatible(in_channelConfig));
		
		const AkUInt32 uNumChannels = in_channelConfig.uNumChannels;
		const AkUInt32 uFrameSize = uNumChannels * sizeof(T);
		T * pFrameTemp = (T*)AkAlloca(uFrameSize);

		T * pFrame = io_pInterleaved;
		for (AkUInt32 uFrame = 0; uFrame < in_uNumFrames; uFrame++)
		{
			for (AkUInt32 uFuma = 0; uFuma < uNumChannels; uFuma++)
			{
				AkUInt32 uAcnIdx = s_uFumaConfigsToAcnOrdering[uNumChannels - 1][uFuma];
				pFrameTemp[uAcnIdx] = (T)(pFrame[uFuma] * s_fToSN3D[s_uAcnConfigsToNumber[uNumChannels - 1][uAcnIdx]]);
			}

			memcpy(pFrame, pFrameTemp, uFrameSize);
			pFrame += uNumChannels;
		}
	}

	template <class T>
	inline void ConvertFilmToStdInterleaved(
		T * io_pInterleaved,
		AkChannelConfig in_channelConfig,
		AkUInt32 in_uNumFrames
		)
	{
		AKASSERT(in_channelConfig.eConfigType == AK_ChannelConfigType_Standard);

		AkUInt32 uFrontMask = in_channelConfig.uChannelMask & AK_SPEAKER_SETUP_FRONT;
		if (uFrontMask == AK_SPEAKER_SETUP_FRONT)
		{
			for (T * pBuf = io_pInterleaved, *pBufEnd = io_pInterleaved + in_channelConfig.uNumChannels * in_uNumFrames; pBuf < pBufEnd; pBuf += in_channelConfig.uNumChannels)
			{
				T c = pBuf[AK_IDX_SETUP_5_CENTER];
				T r = pBuf[AK_IDX_SETUP_5_FRONTRIGHT];
				pBuf[AK_IDX_SETUP_5_CENTER] = r;
				pBuf[AK_IDX_SETUP_5_FRONTRIGHT] = c;
			}
		}

		AkUInt32 uSurroundLFEMask = in_channelConfig.uChannelMask & (AK_SPEAKER_SIDE_LEFT | AK_SPEAKER_SIDE_RIGHT | AK_SPEAKER_LOW_FREQUENCY);
		if (uSurroundLFEMask == (AK_SPEAKER_SIDE_LEFT | AK_SPEAKER_SIDE_RIGHT | AK_SPEAKER_LOW_FREQUENCY))
		{
			unsigned int idx = AK::ChannelMaskToNumChannels(uFrontMask);

			for (T * pBuf = io_pInterleaved, *pBufEnd = io_pInterleaved + in_channelConfig.uNumChannels * in_uNumFrames; pBuf < pBufEnd; pBuf += in_channelConfig.uNumChannels)
			{
				T ls = pBuf[idx + 0];
				T rs = pBuf[idx + 1];
				T lfe = pBuf[idx + 2];
				pBuf[idx + 0] = lfe;
				pBuf[idx + 1] = ls;
				pBuf[idx + 2] = rs;
			}
		}
	}
}

// Helpers moved from SDK, used by sound engine and authoring tools such as source editor, meters, ...
namespace AK
{
	/// Convert FuMa (ChannelOrdering_Standard) or ACN (ChannelOrdering_RunTime)-ordered channel indices into display indices.
	static inline unsigned int AmbisonicsIndexToDisplayIndex(AkChannelOrdering in_eOrdering, AkUInt32 in_uNumChannels, AkUInt32 in_uChannelIdx)
	{
		if (in_eOrdering == ChannelOrdering_Standard)
		{
			if (in_uNumChannels <= AK_MAX_FUMA_CHANNELS)
				return AkFileParser::s_uFumaConfigsToAcnOrdering[in_uNumChannels - 1][in_uChannelIdx];
		}
		else
		{
			// ACN ordering.
		}

		return in_uChannelIdx;
	}

	static inline unsigned int ChannelIndexToDisplayIndex(AkChannelOrdering in_eOrdering, AkChannelConfig in_channelConfig, unsigned int in_uChannelIdx)
	{
		if (in_channelConfig.eConfigType == AK_ChannelConfigType_Standard)
			return StdChannelIndexToDisplayIndex(in_eOrdering, in_channelConfig.uChannelMask, in_uChannelIdx);
		else if (in_channelConfig.eConfigType == AK_ChannelConfigType_Ambisonic)
			return AmbisonicsIndexToDisplayIndex(in_eOrdering, in_channelConfig.uNumChannels, in_uChannelIdx);
		return in_uChannelIdx;
	}
}

#endif //_AK_FILE_PARSER_BASE_H_
