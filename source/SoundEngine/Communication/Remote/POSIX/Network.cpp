/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/


/*	----------------------------------------------------------------------------------------
	POSIX implementation.
	Location: \Wwise\Communication\Remote\POSIX
	Header location: \Wwise\Communication\Remote\CodeBase
	-----------------------------------------------------------------------------------------
*/


#include "Network.h"

#include <unistd.h> 
#include <string.h>
#if defined(AK_ANDROID)
	#include <sys/endian.h> 
#endif
#if defined(AK_LINUX) || defined(AK_EMSCRIPTEN)
	#include <arpa/inet.h>  /* htons() declaration */
#endif
#if defined(AK_QNX)
	#include <net/netbyte.h>
#endif

namespace Network
{
	AKRESULT Init( AkMemPoolId in_pool, bool /*in_bInitSystemLib*/ )
	{
		return AK_Success;
	}

	void Term(bool /*in_bTermSystemLib*/)
	{
	}

	AkInt32 GetLastError()
	{
		return 0;
	}

	bool IsReady() { return true; }

	void GetMachineName( char* out_pMachineName, AkInt32* io_pStringSize )
	{
		if( gethostname( out_pMachineName, *io_pStringSize) )
		{
			static const char* pszUnnamed = { "Unnamed" };
			
			strncpy( out_pMachineName, pszUnnamed, *io_pStringSize );
			*io_pStringSize = (AkInt32) strlen( pszUnnamed );
		}
	}

	bool SameEndianAsNetwork()
	{
		return htons( 12345 ) == 12345;
	}

	AkCommSettings::AkCommSystem GetCommunicationSystem()
	{
		return AK::Comm::GetCurrentSettings().commSystem;
	}

	bool IsDiscoverySystemEnabled()
	{
		return true;
	}
}
