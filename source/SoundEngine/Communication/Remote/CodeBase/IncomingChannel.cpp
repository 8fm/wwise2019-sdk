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

#include "IncomingChannel.h"
#include "Network.h"
#include <AK/Comm/AkCommunication.h>

#include <AK/Tools/Common/AkObject.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/Tools/Common/AkBankReadHelpers.h>

#define INITIAL_BUFFER_SIZE 16*1024

using namespace AKPLATFORM;

IncomingChannel::IncomingChannel(IChannelsHolder *in_pHolder)
	: BaseChannel( in_pHolder )
	, m_pRecvBuf(NULL)
	, m_iBufSize(0)
	, m_iBufUsed(0)
{
}

bool IncomingChannel::Init()
{
	if (!__base::Init())
		return false;

	AllocateRecvBuf(INITIAL_BUFFER_SIZE);
	return m_pRecvBuf != NULL;
}

void IncomingChannel::Term()
{
	__base::Term();

	StopReceiving();

	FreeRecvBuf();
}

bool IncomingChannel::AllocateRecvBuf(AkUInt32 in_AllocSize)
{	
	void *pNew = AkRealloc(AkMemID_Profiler, m_pRecvBuf, in_AllocSize);
	if (pNew != NULL)
	{	
		m_pRecvBuf = (AkUInt8*)pNew;	//Previous was released already.
		m_iBufSize = in_AllocSize;
		return true;
	}
	return false;
}

void IncomingChannel::FreeRecvBuf()
{
	if (m_pRecvBuf)
	{
		AkFree(AkMemID_Profiler, m_pRecvBuf);
		m_pRecvBuf = NULL;
		m_iBufSize = 0;
	}
}

void IncomingChannel::OnPeerConnected()
{
	__base::OnPeerConnected();

	m_pHolder->PeerConnected();

	StartReceiving();
}

AKRESULT IncomingChannel::Process()
{
	// Only return non-Ak_Success results on connection request. BaseChannel's connSocket can be recovered
	// if it errors, but if HandleConnectionRequest encounters an error with serverSocket, the channel
	// enters an unrecoverable state.
	AKRESULT eResult = HandleConnectionRequest();
	if (eResult != AK_Success)
		return eResult;
	
	ProcessReceiving();
	HandleConnectionError();
	return AK_Success;
}

void IncomingChannel::ReceiveCommand()
{
	bool bMoreAvailable = true;
	AkUInt32 msgLen = 0;
	int recvSize = 0;
	do 
	{
		//Grab a large chunk.
		AkUInt8 *pBuf = m_pRecvBuf + m_iBufUsed;
		recvSize = m_connSocket.Recv(pBuf, m_iBufSize - m_iBufUsed, 0);		
		if (recvSize > 0)
		{
			//Process all complete messages in that chunk.
			recvSize += m_iBufUsed;
			bMoreAvailable = recvSize == m_iBufSize;

			pBuf = m_pRecvBuf;	//Start at the beginning, the cutoff is always on a message boundary.
			while (recvSize > sizeof(AkUInt32))
			{
				// Receive the message length		
				msgLen = AK::ReadUnaligned<AkUInt32>(pBuf);		//No swap, Wwise is providing data in the right order.								
				if (msgLen <= (AkUInt32)recvSize - sizeof(AkUInt32))
				{
					pBuf += sizeof(AkUInt32);
					ProcessCommand(pBuf, msgLen);
					pBuf += msgLen;
					recvSize -= msgLen + sizeof(AkUInt32);
				}				
				else
					break;
			}

			AkUInt32 uReminingIdx = (AkUInt32)(pBuf - m_pRecvBuf);
			msgLen += sizeof(AkUInt32);	//Add the size of the packet size to check if the buffer is large enough.  
			if (msgLen > m_iBufSize)
			{
				//Grow the buffer				
				if (!AllocateRecvBuf(msgLen))
				{
					//Out of memory (already reported)
					//Receive this message completely and discard.  This means Wwise & the game is not synchronised anymore.
					msgLen -= recvSize - sizeof(AkUInt32);
					while (msgLen)
					{
						AkInt32 read = m_connSocket.RecvAll(m_pRecvBuf, AkMin(m_iBufSize, msgLen), 0);
						if (read <= 0)
						{
							SignalSocketError();
							m_iBufUsed = 0;
							return;
						}
						msgLen -= read;
					}
					//Restart receiving at the beginning.
					m_iBufUsed = 0;
					recvSize = 0;
				}				
			}
			
			//Copy reminder at beginning of the buffer, so we stitch with next pass.
			memmove(m_pRecvBuf, m_pRecvBuf+ uReminingIdx, recvSize);
			m_iBufUsed = recvSize;			
		}
		else if (recvSize <= 0) 
		{
			SignalSocketError();
			m_iBufUsed = 0;
			return;
		}		
	} while (bMoreAvailable && recvSize > 0);	
}

void IncomingChannel::ProcessReceiving()
{
	if (IsConnectedAndValid())
	{
		int selVal = m_connSocket.Poll(GameSocket::PollRead, 0);

		if (selVal == SOCKET_ERROR)
		{
			SignalSocketError();
		}
		else if (selVal > 0)
		{
			ReceiveCommand();
		}
	}
}

void IncomingChannel::StartReceiving()
{
	m_connSocket.NonBlocking();
	m_iBufUsed = 0;
}

void IncomingChannel::StopReceiving()
{
	m_iBufUsed = 0;
}

