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

#include <stdint.h>

struct WaveFormatEx;

namespace PtADPCM{
	//	Codec
	uint32_t GetEncodedSize( uint32_t sample );
	bool Decode( int16_t* pDst, uint8_t* pSrc, uint32_t samples, uint32_t channels, int16_t sample1, int16_t sample2, uint8_t diffLevel );

	// Format descriptions verification
	bool IsValidPtAdpcmFormat(WaveFormatEx & wfxFormat);
	
	//	Block
	const uint32_t SamplePerBlock = 64;
	const uint32_t BlockSize = 36; // 5 + (64 - 2) / 2
	bool DecodeBlocks( int16_t* pDst, uint8_t* pSrc, uint32_t samples, uint32_t blockAlignment, uint32_t channels );
}