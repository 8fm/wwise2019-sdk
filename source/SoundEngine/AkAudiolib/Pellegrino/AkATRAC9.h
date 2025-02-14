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

#ifndef AK_ATRAC9_H_
#define AK_ATRAC9_H_

#include "ajm.h"
#include "ajm/at9_decoder.h"
#include "AkFileParserBase.h"

// Wwise/ATRAC9-specific WaveFormatExtensible format (actually
// a mix of things from the ATRAC9 fmt chunk and ATRAC9
// fact chunk)
struct At9WaveFormatExtensible
	: public WaveFormatExtensible
{
	AkUInt8 at9ConfigData[SCE_AJM_DEC_AT9_CONFIG_DATA_SIZE]; // ATRAC9 setting information
	AkUInt32 at9TotalSamplesPerChannel;                      // Total samples per channel
	AkUInt16 at9DelaySamplesInputOverlapEncoder;             // Number of extra samples to decode before getting valid output from the decoder. This number of extra samples is added by the encoder at the beginning of the data. They should be decoded and discarded.
};

#define AK_ATRAC9_FULLPLUGINID				CAkEffectsMgr::GetMergedID( AkPluginTypeCodec, AKCOMPANYID_AUDIOKINETIC, AKCODECID_ATRAC9 )

#endif // AK_ATRAC9_H_
