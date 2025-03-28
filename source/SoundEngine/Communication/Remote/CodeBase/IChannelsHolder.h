/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2019 Audiokinetic Inc.
 ***********************************************************************/
#pragma once

class IChannelsHolder
{
public:
	virtual void PeerConnected() = 0;
	virtual void PeerDisconnected() = 0;

	struct Ports
	{
		Ports()
			: m_portCommand( 0 )
			, m_portNotification( 0 )
		{}

		AkUInt16 m_portCommand;
		AkUInt16 m_portNotification;
	};

	virtual void GetPorts( Ports& out_ports ) const = 0;
};
