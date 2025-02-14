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


/*
	----------------------------------------------------------------------------------------
	POSIX implementation.
	Location: \Wwise\Communication\Remote\POSIX
	Header location: \Wwise\Communication\Remote\CodeBase
	-----------------------------------------------------------------------------------------
*/

#include "GameSocket.h"
#include "GameSocketAddr.h"
#include <fcntl.h>


GameSocket::GameSocket()
	: m_socket( INVALID_SOCKET )
{
}

GameSocket::~GameSocket()
{
}

bool GameSocket::Create( AkInt32 in_type, AkInt32 in_protocol )
{
	m_socket = ::socket( AF_INET, in_type, in_protocol );
	int set = 1;
	int option = 0;

#if defined(AK_ANDROID) || defined(AK_LINUX) || defined(AK_QNX) || defined(AK_EMSCRIPTEN) || defined(AK_LUMIN)
	option = MSG_NOSIGNAL;
#else
	option = SO_NOSIGPIPE;
#endif
	
	// never generates SIGPIPE on write
	setsockopt(m_socket, SOL_SOCKET, option, (void *)&set, sizeof(int));

	return m_socket != INVALID_SOCKET;
}

void GameSocket::ReuseAddress()
{
	int bReuseAddr = 1;
	::setsockopt( m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&bReuseAddr, sizeof bReuseAddr );
}

void GameSocket::NoDelay()
{
}

bool GameSocket::NonBlocking() const
{
	int flags = fcntl(m_socket, F_GETFL, 0);
#ifdef O_NONBLOCK
	flags |= O_NONBLOCK;
#else
	flags |= SOCK_NONBLOCK;
#endif
	return fcntl(m_socket, F_SETFL, flags) == 0;
}

AkInt32 GameSocket::Connect( const GameSocketAddr& in_rAddr )
{
	return ::connect( m_socket, (sockaddr*)&in_rAddr.GetInternalType(), sizeof( in_rAddr.GetInternalType() ) );
}

AkInt32 GameSocket::Bind( const GameSocketAddr& in_rAddr )
{
	return ::bind( m_socket, (sockaddr*)&in_rAddr.GetInternalType(), sizeof( in_rAddr.GetInternalType() ) );
}

AkInt32 GameSocket::Listen( AkInt32 in_backlog ) const
{
	return ::listen( m_socket, in_backlog );
}

AkInt32 GameSocket::Accept( GameSocketAddr& out_rAddr, GameSocket & out_targetSocket )
{
	int addrSize = sizeof( out_rAddr.GetInternalType() );
	out_targetSocket.m_socket = ::accept( m_socket, (sockaddr*)&out_rAddr.GetInternalType(), (socklen_t*)&addrSize );

	return out_targetSocket.IsValid() ? 0 : SOCKET_ERROR;
}

AkInt32 GameSocket::Send( const void* in_pBuf, AkInt32 in_length, AkInt32 in_flags ) const
{
	// Ensure that the whole buffer is sent.
	const char* pBuf = (const char*)in_pBuf;

	AkInt32 toSend = in_length;

	while( toSend > 0 )
	{
		AkInt32 sendVal = (AkInt32) ::send( m_socket, pBuf, toSend, in_flags );

		// If there's an error or the socket has been closed by peer.
		if( sendVal == SOCKET_ERROR || sendVal == 0 )
			return sendVal;

		pBuf += sendVal;
		toSend -= sendVal;
	}

	return in_length - toSend;
}

AkInt32 GameSocket::Recv( void* in_pBuf, AkInt32 in_length, AkInt32 in_flags ) const
{
	return (AkInt32) ::recv( m_socket, in_pBuf, in_length, in_flags);
}

AkInt32 GameSocket::RecvAll(void* in_pBuf, AkInt32 in_length, AkInt32 in_flags) const
{
	return Recv( in_pBuf, in_length, MSG_WAITALL);
}

AkInt32 GameSocket::SendTo( const void* in_pBuf, AkInt32 in_length, AkInt32 in_flags, const GameSocketAddr& in_rAddr ) const
{
	return (AkInt32) ::sendto( m_socket, (const char*)in_pBuf, in_length, in_flags, (sockaddr*)&in_rAddr.GetInternalType(), sizeof( in_rAddr.GetInternalType() ) );
}

AkInt32 GameSocket::RecvFrom( void* in_pBuf, AkInt32 in_length, AkInt32 in_flags, GameSocketAddr& out_rAddr ) const
{
	int addrSize = sizeof( out_rAddr.GetInternalType() );	
	return (AkInt32) ::recvfrom( m_socket, (char*)in_pBuf, in_length, in_flags, (sockaddr*)&out_rAddr.GetInternalType(), (socklen_t*)&addrSize );
}

AkInt32 GameSocket::Poll( PollType in_ePollType, AkUInt32 in_timeout ) const
{
	fd_set fds = { 0 };
	FD_SET( m_socket, &fds );
	timeval tv = { 0 };
	tv.tv_usec = in_timeout * 1000;

	return ::select( FD_SETSIZE,
							in_ePollType == PollRead ? &fds : NULL, 
							in_ePollType == PollWrite ? &fds : NULL, 
							NULL, 
							&tv );
}

AkInt32 GameSocket::Shutdown( AkInt32 in_how ) const
{
	return shutdown( m_socket, in_how );
}

AkInt32 GameSocket::Close()
{
	AkInt32 result = ::close( m_socket );
	m_socket = INVALID_SOCKET;

	return result;
}

bool GameSocket::IsValid() const
{
	return m_socket != INVALID_SOCKET;
}

AkUInt16 GameSocket::GetPort() const
{
	sockaddr_in localAddr = { 0 };
	socklen_t addr_size = sizeof(localAddr);
	int ret = getsockname( m_socket, (sockaddr*)&localAddr, &addr_size );
	if ( ret == 0 )
	{
		return ntohs( localAddr.sin_port );
	}

	return 0;
}

/*
AkInt32 GameSocket::Select( fd_set* in_readfds, fd_set* in_writefds, fd_set* exceptfds, const timeval* in_timeout )
{
	return ::socketselect( FD_SETSIZE, in_readfds, in_writefds, exceptfds, (timeval*)in_timeout );
}
*/
