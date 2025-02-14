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

#include "IncomingChannel.h"
#include "ICommandChannelHandler.h"

class IChannelsHolder;

class CommandChannel : public IncomingChannel
{
public:
	CommandChannel( IChannelsHolder* in_pHolder );

	bool InitWithHandler(AK::Comm::ICommandChannelHandler* in_pCmdChannelHandler);

protected:
	virtual bool ProcessCommand( AkUInt8* in_pData, AkUInt32 in_uDataLength );

	virtual AkUInt16 GetRequestedPort() const;
	virtual const char* GetUniquePortName() const;

private:
	AK::Comm::ICommandChannelHandler* m_pCmdChannelHandler;
	DECLARE_BASECLASS( IncomingChannel );
};
