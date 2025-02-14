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

#include "ICommunicationCentral.h"
#include "IChannelsHolder.h"
#include "DiscoveryChannel.h"
#include "CommandChannel.h"
#include "NotificationChannel.h"

#include <AK/Tools/Common/AkObject.h>

class CommunicationCentral 
	: public AK::Comm::ICommunicationCentral
	, public IChannelsHolder
{
public:
	CommunicationCentral();

	// ICommunicationCentral members
	virtual void Destroy();

	virtual bool Init( AK::Comm::ICommunicationCentralNotifyHandler* in_pNotifyHandler, AK::Comm::ICommandChannelHandler* in_pCmdChannelHandler, bool in_bInitSystemLib );
	virtual void PreTerm();
	virtual void Term();
	virtual void Suspend();
	virtual bool Resume();

	// Returns Ak_Fail if an un-recoverable error occurs on one of the Channels
	// Ak_PartialSuccess if not initialized yet, and AK_Success if everything is fine.
	virtual AKRESULT Process();

	virtual AK::Comm::INotificationChannel* GetNotificationChannel();

	virtual void SetNetworkControllerName(const char* in_pszNetworkAppName);

	// IChannelsHolder members
	virtual void PeerConnected();
	virtual void PeerDisconnected();
	
	virtual void GetPorts( Ports& out_ports ) const;

private:
	bool TryInitChannels();

	DiscoveryChannel m_discoveryChannel;
	CommandChannel m_commandChannel;
	NotificationChannel m_notifChannel;

	bool m_bInitialized;

	AK::Comm::ICommunicationCentralNotifyHandler* m_pNotifyHandler;
	AK::Comm::ICommandChannelHandler* m_pCmdHandler;

	bool m_bInitSystemLib;
	bool m_bNetworkReady;
	bool m_bInternalNetworkInit;
	bool m_bSuspended;
};
