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

#include "stdafx.h"

#include "DiscoveryChannel.h"
#include "GameSocketAddr.h"
#include <AK/Comm/AkCommunication.h>
#include "Network.h"
#include "ChannelSerializer.h"
#include "IChannelsHolder.h"

#include <string.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <stdio.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

DiscoveryChannel::DiscoveryChannel( IChannelsHolder* in_pHolder )
	: m_eState( StateAvailable )
	, m_pHolder( in_pHolder )
{
	AKPLATFORM::AkMemSet( m_szComputerName, 0, sizeof m_szComputerName );
	SetControllerName("Unknown");
}

DiscoveryChannel::~DiscoveryChannel()
{
}

AkUInt16 DiscoveryChannel::GetRequestedPort() const
{
	return  AK::Comm::GetCurrentSettings().ports.uDiscoveryBroadcast;
}

bool DiscoveryChannel::Init()
{
	AkInt32 stringSize = sizeof m_szComputerName;
	Network::GetMachineName( m_szComputerName, &stringSize );

	m_socket.Create( SOCK_DGRAM, IPPROTO_UDP );
	m_socket.ReuseAddress();

	GameSocketAddr addr(INADDR_ANY, GetRequestedPort(), "DiscoveryChn");

	AkInt32 res = m_socket.Bind( addr );

	if ( res == SOCKET_ERROR )
	{
		char szString[256];
		sprintf(szString, "AK::Comm -> DiscoveryChannel::Init() -> m_socket.Bind() failed, requested port == %d (AkCommSettings::ports.uDiscoveryBroadcast)\n", AK::Comm::GetCurrentSettings().ports.uDiscoveryBroadcast);
		AKPLATFORM::OutputDebugMsg( szString );

		return false;
	}

	return true;
}

void DiscoveryChannel::Term()
{
	if( m_socket.IsValid() )
		m_socket.Close();
}

DiscoveryChannel::ResponseState DiscoveryChannel::GetResponseState() const
{
	return m_eState;
}

void DiscoveryChannel::SetResponseState( DiscoveryChannel::ResponseState in_eState )
{
	m_eState = in_eState;
}

void DiscoveryChannel::SetControllerName( const char* in_pszControllerName )
{
	if( in_pszControllerName != NULL )
		::strncpy( m_szControllerName, in_pszControllerName, sizeof m_szControllerName );
	else
		::memset( m_szControllerName, 0, sizeof m_szControllerName );
}

extern char* g_pszCustomPlatformName;

AKRESULT DiscoveryChannel::Process()
{
	int selVal = m_socket.Poll( GameSocket::PollRead, 0 );

	if( selVal == SOCKET_ERROR )
	{
		// Socket closed by us
#if defined(DEBUG) && defined(WINSOCK_VERSION)
		if (WSAGetLastError() != 0)
			printf("Discovery socket error %i\n", WSAGetLastError());
#endif
	}
	else if( selVal != 0 )
	{
		GameSocketAddr hostAddr;

		AkUInt8 recvBuf[512] = { 0 };

		// Receiving on UDP returns only the first available datagram.
		int recvVal = m_socket.RecvFrom(recvBuf, sizeof recvBuf, 0, hostAddr);

		if (recvVal == SOCKET_ERROR)
		{
			// if m_socket claimed it had data to read (Poll) but RecvFrom threw an error,
			// then it has encountered an internal error that we cannot recover from
			return AK_Fail;
		}
		else if (recvVal == 0)
		{
			// Socket close by the host
		}
		else
		{
			AK::ChannelDeserializer deserializer(!Network::SameEndianAsNetwork());
			deserializer.Deserializing(recvBuf);

			const DiscoveryMessage::Type eType = DiscoveryMessage::PeekType(recvVal, deserializer);

			DiscoveryMessage* pResponse = NULL;

			if (eType == DiscoveryMessage::TypeDiscoveryRequest)
			{
				// Read the Discovery Request
				DiscoveryRequest msg;
				msg.Deserialize(deserializer);
				AKASSERT(msg.m_usDiscoveryResponsePort != 0);

				IChannelsHolder::Ports ports;
				m_pHolder->GetPorts(ports);

				// Prepare the response
				// MUST BE STATIC because pResponse will point to this structure!
				static DiscoveryResponse response;
				response.m_usCommandPort = ports.m_portCommand;
				response.m_usNotificationPort = ports.m_portNotification;
				response.m_uiProtocolVersion = AK_COMM_PROTOCOL_VERSION;
				response.m_eConsoleType = CommunicationDefines::g_eConsoleType;
				response.m_eConsoleState = ConvertStateToResponseType();
				response.m_pszConsoleName = m_szComputerName;
				response.m_pszCustomPlatformName = g_pszCustomPlatformName;
				response.m_pszControllerName = NULL;
				if (m_eState == StateBusy)
					response.m_pszControllerName = m_szControllerName;

				response.m_pszNetworkAppNameName = AK::Comm::GetCurrentSettings().szAppNetworkName;

				hostAddr.SetPort(msg.m_usDiscoveryResponsePort);

				pResponse = &response;
			}

			if (pResponse)
			{
				AK::ChannelSerializer serializer(!Network::SameEndianAsNetwork());

				{
					// Calculate the message length and set in our send message.
					pResponse->Serialize(serializer);
					pResponse->m_uiMsgLength = serializer.Count();
				}

				serializer.Clear();

				// Serialize the message for good.
				pResponse->Serialize(serializer);

				m_socket.SendTo((char*)serializer.Bytes(), serializer.Count(), 0, hostAddr);
			}
		}
	}
	return AK_Success;
}

DiscoveryResponse::ConsoleState DiscoveryChannel::ConvertStateToResponseType() const
{
	DiscoveryResponse::ConsoleState eState = DiscoveryResponse::ConsoleStateAvailable;

	switch( m_eState )
	{
	case StateAvailable:
		eState = DiscoveryResponse::ConsoleStateAvailable;
		break;

	case StateBusy:
		eState = DiscoveryResponse::ConsoleStateBusy;
		break;

	default:
		break;
		// Something went wrong
	}

	return eState;
}
