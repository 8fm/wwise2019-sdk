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

#include "CommandChannel.h"
#include <AK/Comm/AkCommunication.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CommandChannel::CommandChannel( IChannelsHolder* in_pHolder )
	: IncomingChannel(in_pHolder)
	, m_pCmdChannelHandler( NULL )
{
}

bool CommandChannel::InitWithHandler( AK::Comm::ICommandChannelHandler* in_pCmdChannelHandler )
{
	m_pCmdChannelHandler = in_pCmdChannelHandler;

	return Init();// Call the real init
}

bool CommandChannel::ProcessCommand( AkUInt8* in_pData, AkUInt32 /*in_uDataLength*/ )
{	
	if( !m_pCmdChannelHandler )
	{
		return false;
	}

	AkUInt32 uReturnDataSize = 0;
	const AkUInt8* pReturnData = m_pCmdChannelHandler->HandleExecute( in_pData, uReturnDataSize );

    if( uReturnDataSize )
	{
	    AKASSERT( pReturnData );
	
		Send( pReturnData, uReturnDataSize );
	}

	return true;
}

AkUInt16 CommandChannel::GetRequestedPort() const
{
	return AK::Comm::GetCurrentSettings().ports.uCommand;
}

const char* CommandChannel::GetUniquePortName() const
{
	return "WwiseCmndCh";
}
