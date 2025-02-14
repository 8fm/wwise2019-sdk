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

//////////////////////////////////////////////////////////////////////
//
// AkRandom.h
//
//////////////////////////////////////////////////////////////////////

#pragma once

// LCG with Newlib/Musl characteristics: 64-bit seed, 31-bit output (see http://en.wikipedia.org/wiki/Linear_congruential_generator)
#define AK_RANDOM_A	6364136223846793005ULL
#define AK_RANDOM_C	1

namespace AKRANDOM
{	
	static const AkUInt32 AK_RANDOM_MAX = 0x7FFFFFFF; // 31 bits

	AK_EXTERNFUNC(void, AkRandomInit)(AkUInt64 in_uSeed = 0);

	extern AkUInt64 g_uSeed;

	inline AkInt32 SeedToOutputValue(AkUInt64 in_uSeed)
	{
		return (AkInt32)(in_uSeed >> 33);
	}

	// Randomize using local seed
	inline AkInt32 AkRandom(AkUInt64 &io_uSeed)
	{
		io_uSeed = io_uSeed*AK_RANDOM_A + AK_RANDOM_C;
		return SeedToOutputValue(io_uSeed);
	}

	// Randomize using global seed
	inline AkInt32 AkRandom()
	{
		return AkRandom(g_uSeed);
	}
}
