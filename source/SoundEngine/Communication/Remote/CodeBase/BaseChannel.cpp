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
#include <AK/SoundEngine/Common/AkTypes.h>
#include "BaseChannel.h"
#include "GameSocketAddr.h"
#include "NotificationChannel.h"
#include "Network.h"
#include <stdio.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BaseChannel::BaseChannel(IChannelsHolder *in_pHolder)
	: m_serializer(false)
	, m_pHolder(in_pHolder)
	, m_bIsPeerConnected(false)
	, m_bErrorProcessingConnection(false)
{
}

void BaseChannel::Term()
{
	Disconnect();
	StopListening();
}

void BaseChannel::Disconnect()
{
	if (m_connSocket.IsValid())
	{
		m_connSocket.Shutdown(SD_BOTH);
		m_connSocket.Close();
	}
}

void BaseChannel::StopListening()
{
	if (m_serverSocket.IsValid())
	{
		m_serverSocket.Shutdown(SD_BOTH);
		m_serverSocket.Close();
	}
}

void BaseChannel::HandleConnectionError()
{
	if (m_bErrorProcessingConnection)
	{
		m_bIsPeerConnected = false;
		m_pHolder->PeerDisconnected();
		m_bErrorProcessingConnection = false;
	}
}

void BaseChannel::OnPeerConnected()
{
	m_bIsPeerConnected = true;
}

bool BaseChannel::IsConnectedAndValid()
{
	return (m_bIsPeerConnected && m_connSocket.IsValid() && !m_bErrorProcessingConnection);
}

AKRESULT BaseChannel::HandleConnectionRequest()
{
#ifdef AK_REVERSE_COMM
	/*if (!m_connSocket.IsValid())
	{
		const char* szConnectAddress = "127.0.0.1";
		AkUInt32 localIP = GameSocketAddr::ConvertIP(szConnectAddress); // There is no place like 127.0.0.1
		AkUInt16 requestedPort = GetRequestedPort();
		GameSocketAddr addr(localIP, requestedPort);
		printf("Attempting a connection on :%s Port %u\n", szConnectAddress, (AkUInt32)requestedPort);
		m_connSocket.Connect(addr);
	}*/
#else
	if (m_serverSocket.IsValid())
	{
		int selVal = m_serverSocket.Poll(GameSocket::PollRead, 0);

		if (selVal == SOCKET_ERROR)
		{
			// Socket closed by us
		}
		else if (selVal != 0)
		{
			GameSocketAddr hostAddr;
			selVal = m_serverSocket.Accept(hostAddr, m_connSocket);

			if (m_connSocket.IsValid())
			{
				OnPeerConnected();
			}
			else
			{
				if (selVal != SOCKET_ERROR)
					return AK_Success;
				else
					return AK_Fail;
			}
		}
	}
#endif
	return AK_Success;
}

AKRESULT BaseChannel::Process()
{
	HandleConnectionError();
	AKRESULT result = HandleConnectionRequest();
	return result;
}

bool BaseChannel::StartListening()
{
#ifdef AK_REVERSE_COMM
	// We do not listen, we connect!
	return true;
#else
	m_serverSocket.Create(SOCK_STREAM, IPPROTO_TCP);
	m_serverSocket.ReuseAddress();

	GameSocketAddr localAddr(INADDR_ANY, GetRequestedPort(), GetUniquePortName());

	AkInt32 res = m_serverSocket.Bind(localAddr);

	if (res == SOCKET_ERROR)
	{
		char szString[256];
		sprintf(szString, "AK::Comm -> StartListening() -> m_serverSocket.Bind() failed, requested port == %d (%s)\n", GetRequestedPort(), GetUniquePortName());
		AKPLATFORM::OutputDebugMsg(szString);

		return false;
	}

	return m_serverSocket.Listen(GetNumConnectionBacklog()) != SOCKET_ERROR;
#endif
}

AkUInt16 BaseChannel::GetPort() const
{
	if (GetRequestedPort() != 0)
		return GetRequestedPort();

	return m_serverSocket.GetPort();
}

void BaseChannel::SendNotification(const AkUInt8* in_pNotificationData, int in_dataSize, bool in_bAccumulate)
{
	if (IsConnectedAndValid())
	{
		if (in_bAccumulate)
		{
			Accumulate(in_pNotificationData, in_dataSize);
		}
		else
		{
			Send(in_pNotificationData, in_dataSize);
		}
	}
}

void BaseChannel::SendAccumulatedData()
{
	if (IsConnectedAndValid())
	{
		SendSerializer();
	}
	else
	{
		m_serializer.Clear();
	}
}

void BaseChannel::Accumulate(const AkUInt8* in_pData, int in_dataLength)
{
	AkInt32 iPrevSize = m_serializer.Count();
	bool bOK = m_serializer.Put(in_pData, in_dataLength);
	if (!bOK)
	{
		// If there is no space left in accumulation buffer, send all previously accumulated data and try again.
		m_serializer.SetCount(iPrevSize);
		SendAccumulatedData();
		bOK = m_serializer.Put(in_pData, in_dataLength);
		if (!bOK)
			m_serializer.Clear(); // Erase all traces of this notification, drop it.
	}
}

void BaseChannel::Send(const AkUInt8* in_pData, int in_dataLength)
{
	// When serializing, the serializer puts the length of the data in front of the data
	// so it already fits our protocol { msg lenght | msg } pattern.
	m_serializer.Put(in_pData, in_dataLength);

	SendSerializer();
}

void BaseChannel::SendSerializer()
{
	if (m_connSocket.Send((const char*)m_serializer.Bytes(), m_serializer.Count(), 0) == SOCKET_ERROR)
	{
		//Lost the connection.
		SignalSocketError();
	}
	m_serializer.Clear();
}

