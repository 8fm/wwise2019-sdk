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

#include "BaseChannel.h"

class NotificationChannel : public BaseChannel
{
public:
	NotificationChannel(IChannelsHolder *in_pHolder) : BaseChannel(in_pHolder){}

	virtual void OnPeerConnected();

protected:
	virtual AkUInt16 GetRequestedPort() const;
	virtual const char* GetUniquePortName() const;
	virtual AkInt32 GetNumConnectionBacklog(){ return 1; }

private:
	DECLARE_BASECLASS(BaseChannel);
};
