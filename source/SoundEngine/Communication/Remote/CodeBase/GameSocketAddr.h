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


/*	
	----------------------------------------------------------------------------------------
	Common file used by the communication as a cross platform wrapper for the BSD sockets API.
	Location: \Wwise\Communication\Remote\CodeBase
	Implementation location: In the platform folders: Wwise\Communication\Remote\{Platform}
	
	See more information in the implementation file.
	-----------------------------------------------------------------------------------------
*/

#pragma once

#include "AkPrivateTypes.h"
#include "NetworkTypes.h"

class GameSocketAddr
{
public:
	GameSocketAddr();
	GameSocketAddr( const GameSocketAddr& in_rAddr );
	GameSocketAddr( AkUInt32 in_ip, AkUInt16 in_port, const char* in_OptionalPortName = NULL );
	~GameSocketAddr();

	void SetPort( AkUInt16 in_port );
	AkUInt16 GetPort() const;

	const SocketAddrType& GetInternalType() const;
	SocketAddrType& GetInternalType();

	static AK_FUNC( AkUInt32, ConvertIP )( const char* in_pszIP );

private:
	SocketAddrType m_sockAddr;
};
