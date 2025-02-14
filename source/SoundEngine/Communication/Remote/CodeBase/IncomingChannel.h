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
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/Tools/Common/AkLock.h>

class IncomingChannel : public BaseChannel
{
public:
	IncomingChannel(IChannelsHolder *in_pHolder);

	virtual bool Init();
	virtual void Term();
	virtual void OnPeerConnected();

	// Returns Ak_Fail if an error occurs on the serverSocket and should be re-created.
	// Errors on connSocket can be recovered with new connections discovered via serverSocket.
	AKRESULT Process();

protected:
	virtual bool ProcessCommand( AkUInt8* in_pData, AkUInt32 in_uDataLength ) = 0;

private:
	void ReceiveCommand();
	void ProcessReceiving();
	void StartReceiving();
	void StopReceiving();

	// Helper
	bool AllocateRecvBuf(AkUInt32 in_AllocSize);
	void FreeRecvBuf();

	AkUInt8* m_pRecvBuf;
	AkUInt32 m_iBufSize;	
	AkUInt32 m_iBufUsed;

	DECLARE_BASECLASS(BaseChannel);
};
