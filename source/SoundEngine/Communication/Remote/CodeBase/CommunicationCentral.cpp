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

#include "CommunicationCentral.h"
#include "Network.h"
#include "GameSocket.h"
#include "ICommunicationCentralNotifyHandler.h"
#include <AK/SoundEngine/Common/AkSoundEngine.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CommunicationCentral::CommunicationCentral()
	: m_discoveryChannel( this )
	, m_commandChannel( this )
	, m_notifChannel( this )
	, m_bInitialized( false )
	, m_pNotifyHandler( NULL )
	, m_pCmdHandler(NULL)
	, m_bInitSystemLib(false)
	, m_bNetworkReady(false)
	, m_bInternalNetworkInit(false)
	, m_bSuspended(false)
{
}

void CommunicationCentral::Destroy()
{
	AkDelete(AkMemID_Profiler, this );
}

bool CommunicationCentral::Init( AK::Comm::ICommunicationCentralNotifyHandler* in_pNotifyHandler, AK::Comm::ICommandChannelHandler* in_pCmdChannelHandler, bool in_bInitSystemLib )
{
	AKASSERT( ! m_bInitialized );

	m_pNotifyHandler = in_pNotifyHandler;
	m_pCmdHandler = in_pCmdChannelHandler;
	m_bInitSystemLib = in_bInitSystemLib;
	
	return TryInitChannels();
}

bool CommunicationCentral::TryInitChannels()
{
	if (m_bInitialized)
		return true; // already initialized, nothing to do

	m_bNetworkReady = Network::IsReady();
	if (m_bNetworkReady == false)
		return true; // will try again on next Process

	AKRESULT netResult = Network::Init(AkMemID_Profiler, m_bInitSystemLib);
	if (netResult != AK_Success && netResult != AK_PartialSuccess)
		return false;

	m_bInternalNetworkInit = (netResult == AK_Success);

	if (m_commandChannel.InitWithHandler(m_pCmdHandler)
		&& m_notifChannel.Init()
		&& m_discoveryChannel.Init()
		&& m_commandChannel.StartListening()
		&& m_notifChannel.StartListening() )
	{
		m_bInitialized = true;
	}

	return m_bInitialized;
}

void CommunicationCentral::PreTerm()
{
	m_bInitialized = false;
	
	if (m_bNetworkReady)
	{
		m_notifChannel.Term();
		m_commandChannel.Term();
	}
}

void CommunicationCentral::Term()
{
	if (m_bNetworkReady)
	{
		m_discoveryChannel.Term();

		Network::Term(m_bInternalNetworkInit);
	}
	m_bNetworkReady = false;
	m_bSuspended = false;
}

void CommunicationCentral::Suspend()
{
	if (m_bSuspended)
		return;

	// Release network resources
	PreTerm();
	Term();
	m_bSuspended = true;
}

bool CommunicationCentral::Resume()
{
	if (!m_bSuspended)
		return false;

	// Re-aquire network resources
	bool res = TryInitChannels();
	m_bSuspended = false;
	return res;
}

AKRESULT CommunicationCentral::Process()
{
	WWISE_SCOPED_PROFILE_MARKER("CommunicationCentral::Process");

	// Retry Init if network was not available during last Init
	if (m_bNetworkReady == false)
		TryInitChannels();

	if (!m_bInitialized)
		return AK_PartialSuccess;

	AKRESULT eResult;

	if( Network::IsDiscoverySystemEnabled() )
	{
		// Go through the channels and call process
		eResult = m_discoveryChannel.Process();
		if (eResult != AK_Success)
			return AK_Fail;
	}

	eResult = m_notifChannel.Process();
	if (eResult != AK_Success)
		return AK_Fail;

	eResult = m_commandChannel.Process();
	if (eResult != AK_Success)
		return AK_Fail;

	return AK_Success;
}

AK::Comm::INotificationChannel* CommunicationCentral::GetNotificationChannel()
{
	return &m_notifChannel;
}

void CommunicationCentral::PeerConnected()
{
	m_discoveryChannel.SetResponseState( DiscoveryChannel::StateBusy );
}

void CommunicationCentral::PeerDisconnected()
{
	m_notifChannel.Disconnect();
	m_commandChannel.Disconnect();

	if( m_pNotifyHandler != NULL )
	{
		m_pNotifyHandler->PeerDisconnected();
	}
	
	m_discoveryChannel.SetResponseState( DiscoveryChannel::StateAvailable );
	m_discoveryChannel.SetControllerName("Unknown");
}

void CommunicationCentral::GetPorts( Ports& out_ports ) const
{
	out_ports.m_portCommand = m_commandChannel.GetPort();
	out_ports.m_portNotification = m_notifChannel.GetPort();
}

void CommunicationCentral::SetNetworkControllerName(const char* in_pszNetworkAppName)
{
	m_discoveryChannel.SetControllerName(in_pszNetworkAppName);
}
