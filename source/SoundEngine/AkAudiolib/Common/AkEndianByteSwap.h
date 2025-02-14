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
#include <stdlib.h>


namespace AK
{
	namespace EndianByteSwap
	{
		inline void Swap( const AkUInt8* in_pDataSrc, int in_dataSize, AkUInt8* in_pDataDest )
		{
			AkRegister int i = 0;
			AkRegister int j = in_dataSize-1;

			while( i < in_dataSize )
			{
				in_pDataDest[j] = in_pDataSrc[i];
				++i; --j;
			}
		}

		inline AkUInt16 WordSwap( AkUInt16 in_wValue )
		{
// _byteswap_ushort is compiler dependent
#if defined (AK_WIN)
			return _byteswap_ushort( in_wValue );
#else
			return (in_wValue >> 8) | (in_wValue << 8);
#endif
		}

		inline AkUInt32 DWordSwap( AkUInt32 in_dwValue )
		{
// _byteswap_ulong is compiler dependent
#if defined (AK_WIN)
			return _byteswap_ulong( in_dwValue );
#else
			return (((in_dwValue & 0x000000FF) << 24) + ((in_dwValue & 0x0000FF00) << 8) +
					((in_dwValue & 0x00FF0000) >> 8) + ((in_dwValue & 0xFF000000) >> 24));
#endif
		}

		inline AkUInt64 Int64Swap( AkUInt64 x )
		{
// _byteswap_uint64 is compiler dependent
#if defined (AK_WIN)
			return _byteswap_uint64( x );
#else
			x = (x & 0x00000000FFFFFFFF) << 32 | (x & 0xFFFFFFFF00000000) >> 32;
			x = (x & 0x0000FFFF0000FFFF) << 16 | (x & 0xFFFF0000FFFF0000) >> 16;
			x = (x & 0x00FF00FF00FF00FF) << 8 | (x & 0xFF00FF00FF00FF00) >> 8;
			return x;			
#endif
		}
	}
}
