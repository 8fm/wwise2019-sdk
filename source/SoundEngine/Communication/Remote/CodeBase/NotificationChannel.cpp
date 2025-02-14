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

#include <AK/Comm/AkCommunication.h>
#include "NotificationChannel.h"

void NotificationChannel::OnPeerConnected()
{
	__base::OnPeerConnected();

	m_connSocket.NoDelay();
}

AkUInt16 NotificationChannel::GetRequestedPort() const
{
	return AK::Comm::GetCurrentSettings().ports.uNotification;
}

const char* NotificationChannel::GetUniquePortName() const
{
	return "WwiseNtfyCh";
}
