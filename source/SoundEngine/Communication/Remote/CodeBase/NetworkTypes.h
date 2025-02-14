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

#pragma once

#if defined WINSOCK_VERSION

typedef sockaddr_in SocketAddrType;

#elif defined AK_USE_UWP_API

typedef Windows::Networking::HostName^ SocketAddrType;

struct SOCKET
{
	int type;
	Platform::Object^ socket;
};

#define SOCKET_ERROR -1
#define INADDR_ANY 0

// Socket creation
#define SOCK_DGRAM 0
#define SOCK_STREAM 1
#define IPPROTO_TCP 0
#define IPPROTO_UDP 0

#define SD_BOTH 0

#elif defined (AK_APPLE)  || defined(AK_ANDROID) || defined(AK_LINUX) || defined(AK_QNX) || defined(AK_EMSCRIPTEN)

#if defined(AK_APPLE) || defined(AK_ANDROID) || defined(AK_LINUX) || defined(AK_QNX) || defined(AK_EMSCRIPTEN)
#include <unistd.h> 
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>

typedef sockaddr_in SocketAddrType;

#define SOCKET int
#define INVALID_SOCKET 0xffffffff
#define SOCKET_ERROR -1

#define SD_BOTH SHUT_RDWR

#elif defined AK_NX

	#include <nn/socket.h>

	#define INVALID_SOCKET 0xffffffff
	#define SOCKET_ERROR -1

	#define SD_BOTH SHUT_RDWR

	#define AK_SETUP_NX_HTCS	// Comment out to remove all HTCS code from the run time.
	#define AK_SETUP_NX_SOCKET	// Comment out to remove all Socket code from the run time. (Socket is needed for some systems, but is marked as deprecated, so it is just in case this support gets removed)

	#ifdef AK_SETUP_NX_HTCS
		#include <nn/htcs.h>
		#include <nn/os.h>
	#endif

	#ifdef AK_SETUP_NX_SOCKET
		#include <nn/ldn.h>
	#endif

	struct CombinedSockAddr
	{
		#ifdef AK_SETUP_NX_HTCS
			nn::htcs::SockAddrHtcs htcsAddr;
		#endif
		#ifdef AK_SETUP_NX_SOCKET
			sockaddr_in socketAddr;
		#endif
	};
	typedef CombinedSockAddr SocketAddrType;

	#define SOCKET int

#elif defined AK_SONY

#include <net.h>

typedef SceNetSockaddrIn SocketAddrType;

#define SOCKET SceNetId
#define INVALID_SOCKET 0xffffffff
#define SOCKET_ERROR -1
#define INADDR_ANY SCE_NET_INADDR_ANY

// Socket creation
#define AF_INET SCE_NET_AF_INET
#define SOCK_DGRAM SCE_NET_SOCK_DGRAM
#define SOCK_STREAM SCE_NET_SOCK_STREAM
#define IPPROTO_TCP SCE_NET_IPPROTO_TCP
#define IPPROTO_UDP SCE_NET_IPPROTO_UDP

// Socket options
#define SOL_SOCKET SCE_NET_SOL_SOCKET
#define SO_REUSEADDR SCE_NET_SO_REUSEADDR

#define SD_BOTH SCE_NET_SHUT_RDWR

#endif
