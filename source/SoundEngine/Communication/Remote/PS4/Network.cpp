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
	PS3 implementation.
	Location: \Wwise\Communication\Remote\PS3
	Header location: \Wwise\Communication\Remote\CodeBase
	-----------------------------------------------------------------------------------------
*/


#include "Network.h"

#include <net.h>
#include <libnetctl.h>
#include <string.h>

#include "NetworkTypes.h"

namespace Network
{
	AKRESULT Init( AkMemPoolId in_pool, bool in_bInitSystemLib )
	{
		return AK_Success;
	}

	void Term(bool in_bTermSystemLib)
	{
	}

	AkInt32 GetLastError()
	{
		return sce_net_errno;
	}

	bool IsReady() { return true; }

	static int ResolveDevkitIpToDnsName(const SceNetInAddr *addr, char *hostname, int len)
	{
		SceNetId rid = -1;
		int memid = -1;
		int ret;

		ret = sceNetPoolCreate(__FUNCTION__, 4 * 1024, 0);
		if (ret < 0) 
		{
			goto failed;
		}

		memid = ret;
		ret = sceNetResolverCreate("resolver", memid, 0);
		if (ret < 0)
		{
			goto failed;
		}

		rid = ret;
		ret = sceNetResolverStartAton(rid, addr, hostname, len, 1, 0, 0);
		if (ret < 0) 
		{
			goto failed;
		}

		ret = sceNetResolverDestroy(rid);
		if (ret < 0)
		{
			goto failed;
		}

		ret = sceNetPoolDestroy(memid);
		if (ret < 0) 
		{
			goto failed;
		}
		return 0;

	failed:
		sceNetResolverDestroy(rid);
		sceNetPoolDestroy(memid);
		return ret;
	}

	void GetMachineName( char* out_pMachineName, AkInt32* io_pStringSize )
	{		
		char sHostname[SCE_NET_RESOLVER_HOSTNAME_LEN_MAX + 1];

		SceNetSockaddrIn sin;
		memset(&sin, 0, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = SCE_NET_AF_INET;

		union SceNetCtlInfo info;
		memset(&info, 0, sizeof(info));
		int ret = sceNetCtlGetInfo(SCE_NET_CTL_INFO_IP_ADDRESS, &info);

		bool nameFound = false;

		if (sceNetInetPton(SCE_NET_AF_INET, info.ip_address, &sin.sin_addr) >= 0)
		{
			if ( ResolveDevkitIpToDnsName(&sin.sin_addr, sHostname, sizeof(sHostname)) >= 0 )
			{
				*io_pStringSize = strlen( sHostname ) + 1;
				strncpy( out_pMachineName, sHostname, *io_pStringSize );
				out_pMachineName[*io_pStringSize-1] = 0;
				nameFound = true;
			}
		}
		
		if (!nameFound)
		{
			static const char* pszUnnamed = { "Unnamed" };
			strncpy( out_pMachineName, pszUnnamed, *io_pStringSize );
			out_pMachineName[*io_pStringSize-1] = 0;
			*io_pStringSize = strlen( pszUnnamed );
		}		
		
		/*
		union SceNetCtlInfo info;
		memset(&info, 0, sizeof(info));
		int ret = sceNetCtlGetInfo(SCE_NET_CTL_INFO_DHCP_HOSTNAME, &info);
		if(ret < 0)
		{
			strncpy( out_pMachineName, pszUnnamed, *io_pStringSize );
			out_pMachineName[*io_pStringSize-1] = 0;
			*io_pStringSize = strlen( pszUnnamed );
		}
		else	
		{
			strncpy( out_pMachineName, info.dhcp_hostname, *io_pStringSize );
			out_pMachineName[*io_pStringSize-1] = 0;
			*io_pStringSize = strlen( info.dhcp_hostname );
		}*/

	}

	bool SameEndianAsNetwork()
	{
		return sceNetHtons( 12345 ) == 12345;
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
