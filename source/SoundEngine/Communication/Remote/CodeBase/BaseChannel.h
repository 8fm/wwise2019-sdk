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

#include "GameSocket.h"
#include "IChannelsHolder.h"
#include "INotificationChannel.h"
#include "ChannelSerializer.h"

class BaseChannel : public AK::Comm::INotificationChannel
{
public:

	BaseChannel(IChannelsHolder *in_pHolder);

	virtual bool Init(){ return true; }
	virtual void Term();

	virtual void OnPeerConnected();
	bool StartListening();

	// Returns Ak_Fail if an error occurs on the serverSocket and should be re-created.
	// Errors on connSocket can be recovered with new connections discovered via serverSocket.
	AKRESULT Process();

	void Disconnect();
	AkUInt16 GetPort() const;

	bool SocketError() const { return m_bErrorProcessingConnection; }

	// INotificationChannel members
	virtual void SendNotification(const AkUInt8* in_pNotificationData, int in_dataSize, bool in_bAccumulate);
	virtual void SendAccumulatedData();

protected:

	void HandleConnectionError();
	AKRESULT HandleConnectionRequest();

	void Send(const AkUInt8* in_pData, int in_dataLength);
	bool IsConnectedAndValid();
	void SignalSocketError(){ m_bErrorProcessingConnection = true; }

	virtual AkUInt16 GetRequestedPort() const = 0;
	virtual const char* GetUniquePortName() const = 0;
	virtual AkInt32 GetNumConnectionBacklog(){ return 5; }// only because 5 is the default value in the Socket API for consistency

protected:
	AK::ChannelSerializer m_serializer; // Having it as member to reuse for performance reasons.
	IChannelsHolder* m_pHolder;
	GameSocket m_connSocket;

private:

	void SendSerializer();
	void Accumulate(const AkUInt8* in_pData, int in_dataLength);
	void StopListening();

	GameSocket m_serverSocket;

	bool m_bIsPeerConnected;
	volatile bool m_bErrorProcessingConnection;
};
