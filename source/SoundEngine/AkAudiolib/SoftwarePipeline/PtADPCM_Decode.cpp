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
#define PtADPCM_Assert(x)	AKASSERT(x)

#include <stdint.h>
#include "PtADPCM_Decode.h"
#include "AkFileParserBase.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

namespace PtADPCM{
	const int32_t SIZEOF_DIFFERENCE           = 16; // 4bit
	const int32_t SIZEOF_DIFFERENCE_LEVEL     = 12;
	
	struct DifferenceTblItem{
		int32_t diff;
		int32_t level;
	};
	const DifferenceTblItem DifferenceTbl[ SIZEOF_DIFFERENCE_LEVEL * SIZEOF_DIFFERENCE ] = {
			{   -14, 2},{   -10, 2},{    -7, 1},{    -5, 1},{    -3, 0},{    -2, 0},{    -1, 0},{     0, 0},{     0, 0},{     1, 0},{     2, 0},{     3, 0},{     5, 1},{     7, 1},{    10, 2},{    14, 2},
			{   -28, 3},{   -20, 3},{   -14, 2},{   -10, 2},{    -7, 1},{    -5, 1},{    -3, 1},{    -1, 0},{     1, 0},{     3, 1},{     5, 1},{     7, 1},{    10, 2},{    14, 2},{    20, 3},{    28, 3},
			{   -56, 4},{   -40, 4},{   -28, 3},{   -20, 3},{   -14, 2},{   -10, 2},{    -6, 2},{    -2, 1},{     2, 1},{     6, 2},{    10, 2},{    14, 2},{    20, 3},{    28, 3},{    40, 4},{    56, 4},
			{  -112, 5},{   -80, 5},{   -56, 4},{   -40, 4},{   -28, 3},{   -20, 3},{   -12, 3},{    -4, 2},{     4, 2},{    12, 3},{    20, 3},{    28, 3},{    40, 4},{    56, 4},{    80, 5},{   112, 5},
			{  -224, 6},{  -160, 6},{  -112, 5},{   -80, 5},{   -56, 4},{   -40, 4},{   -24, 4},{    -8, 3},{     8, 3},{    24, 4},{    40, 4},{    56, 4},{    80, 5},{   112, 5},{   160, 6},{   224, 6},
			{  -448, 7},{  -320, 7},{  -224, 6},{  -160, 6},{  -112, 5},{   -80, 5},{   -48, 5},{   -16, 4},{    16, 4},{    48, 5},{    80, 5},{   112, 5},{   160, 6},{   224, 6},{   320, 7},{   448, 7},
			{  -896, 8},{  -640, 8},{  -448, 7},{  -320, 7},{  -224, 6},{  -160, 6},{   -96, 6},{   -32, 5},{    32, 5},{    96, 6},{   160, 6},{   224, 6},{   320, 7},{   448, 7},{   640, 8},{   896, 8},
			{ -1792, 9},{ -1280, 9},{  -896, 8},{  -640, 8},{  -448, 7},{  -320, 7},{  -192, 7},{   -64, 6},{    64, 6},{   192, 7},{   320, 7},{   448, 7},{   640, 8},{   896, 8},{  1280, 9},{  1792, 9},
			{ -3584,10},{ -2560,10},{ -1792, 9},{ -1280, 9},{  -896, 8},{  -640, 8},{  -384, 8},{  -128, 7},{   128, 7},{   384, 8},{   640, 8},{   896, 8},{  1280, 9},{  1792, 9},{  2560,10},{  3584,10},
			{ -7168,11},{ -5120,11},{ -3584,10},{ -2560,10},{ -1792, 9},{ -1280, 9},{  -768, 9},{  -256, 8},{   256, 8},{   768, 9},{  1280, 9},{  1792, 9},{  2560,10},{  3584,10},{  5120,11},{  7168,11},
			{-14336,11},{-10240,11},{ -7168,11},{ -5120,11},{ -3584,10},{ -2560,10},{ -1536,10},{  -512, 9},{   512, 9},{  1536,10},{  2560,10},{  3584,10},{  5120,11},{  7168,11},{ 10240,11},{ 14336,11},
			{-28672,11},{-20480,11},{-14336,11},{-10240,11},{ -7168,11},{ -5120,11},{ -3072,11},{ -1024,10},{  1024,10},{  3072,11},{  5120,11},{  7168,11},{ 10240,11},{ 14336,11},{ 20480,11},{ 28672,11},
	};
	
	//inline int16_t DecodeOne( uint8_t code, int32_t& dLev, int32_t& prev1, int32_t& prev2 );
}

uint32_t PtADPCM::GetEncodedSize( uint32_t samples )
{
	// 1 sample == 4 bit
	return (samples + 1) / 2;
}

#define DecodeOne()	{\
	/* estimate */\
	int32_t esti = prev1 * 2 - prev2;\
	\
	/* difference & level */\
	const DifferenceTblItem& diff = DifferenceTbl[ dLev * SIZEOF_DIFFERENCE + c ];\
	dLev = diff.level;\
	\
	/* add & clipping */\
	int32_t comp = esti + diff.diff;\
	if( comp != static_cast<int32_t>(static_cast<int16_t>(comp)) ){\
		if( comp >= 0 ){\
			comp = 0x7fff;\
		}else{\
			comp = -0x8000;\
		}\
	}\
	\
	/* prev value */\
	prev2 = prev1;\
	prev1 = comp;\
	\
	*pDst = static_cast<int16_t>(comp);\
}

bool PtADPCM::Decode( int16_t* pDst, uint8_t* pSrc, uint32_t samples, uint32_t channels,
					  int16_t sample1, int16_t sample2, uint8_t diffLevel )
{
	int32_t prev2 = sample1;
	int32_t prev1 = sample2;
	int32_t dLev  = diffLevel;
	PtADPCM_Assert( dLev <= SIZEOF_DIFFERENCE_LEVEL );
	
	for( uint32_t i = samples / 2; i > 0; i-- ){
		uint8_t code = *pSrc;
		
		{
			uint8_t c = code & 0xf;
			
			DecodeOne();
		}
		pDst += channels;
		
		{
			uint8_t c = code >> 4;
			
			DecodeOne();
		}
		pDst += channels;
		
		pSrc++;
	}
	if( (samples % 2) != 0 ){
		{
			uint8_t c = *pSrc & 0xf;
			
			DecodeOne();
		}
	}
	
	return true;
}

#if 0
int16_t PtADPCM::DecodeOne( uint8_t code, int32_t& dLev, int32_t& prev1, int32_t& prev2 )
{
	// estimate
	int32_t esti = prev1 * 2 - prev2;
	
	// difference & level
	DifferenceTblItem diff = DifferenceTbl[ dLev * SIZEOF_DIFFERENCE + code ];
	dLev = diff.level;
	
	// add & clipping
	int32_t comp = esti + diff.diff;
	if( comp != static_cast<int32_t>(static_cast<int16_t>(comp)) ){
		if( comp >= 0 ){
			comp = 0x7fff;
		}else{
			comp = -0x8000;
		}
	}
	
	// prev value
	prev2 = prev1;
	prev1 = comp;
	
	return static_cast<int16_t>(comp);
}
#endif

bool PtADPCM::DecodeBlocks( int16_t* pDst, uint8_t* pSrc, uint32_t samples, uint32_t blockAlignment, uint32_t channels )
{
	while( samples > 0 ){
		uint32_t decodeSamples = samples; 
		if( decodeSamples > SamplePerBlock ){
			decodeSamples = SamplePerBlock;
		}
		samples -= decodeSamples;
		
		uint8_t* pBlock = pSrc;
		int16_t sample1 = *reinterpret_cast<int16_t*>(pBlock);
		pBlock += sizeof(int16_t);
		int16_t sample2 = *reinterpret_cast<int16_t*>(pBlock);
		pBlock += sizeof(int16_t);
		uint8_t diffLevel = *pBlock;
		pBlock += sizeof(uint8_t);
		
		*pDst = sample1;
		pDst += channels;
		decodeSamples--;
		
		if( decodeSamples <= 0 ){
			break;
		}
		
		*pDst = sample2;
		pDst += channels;
		decodeSamples--;
		
		Decode( pDst, pBlock, decodeSamples, channels, sample1, sample2, diffLevel );
		pDst += decodeSamples * channels;
		
		pSrc += blockAlignment;
	}
	
	return true;
}
// Validates a PtADPCM format structure.
bool PtADPCM::IsValidPtAdpcmFormat(WaveFormatEx & wfx)
{
	if (AK_WAVE_FORMAT_PTADPCM != wfx.wFormatTag)
	{
		return false;
	}
	if (wfx.nChannels < 1)
	{
		return false;
	}
	if ((wfx.nChannels * BlockSize) != wfx.nBlockAlign)
	{
		return false;
	}
	return true;
}

