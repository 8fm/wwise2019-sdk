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

#include "CommunicationDefines.h"
#include "AkPrivateTypes.h"
#include "ChannelSerializer.h"

struct DiscoveryMessage
{
	enum Type
	{
		TypeDiscoveryInvalid = -1,
		TypeDiscoveryRequest = 0,
		TypeDiscoveryResponse,
		TypeDiscoveryChannelsInitRequest, // This message is currently useless, but you cannot remove it!!!!!! it would break compatibility of future messages when older version of wwise and newer versions are running on same subnet.
		TypeDiscoveryChannelsInitResponse // This message is currently useless, but you cannot remove it!!!!!!
	};

	DiscoveryMessage( Type in_eType );
	virtual ~DiscoveryMessage() {}

	virtual bool Serialize(AK::ChannelSerializer& in_rSerializer ) const;
	virtual bool Deserialize(AK::ChannelDeserializer& in_rSerializer );

	static Type PeekType( int in_nBufferSize, AK::ChannelDeserializer& in_rSerializer );

	AkUInt32 m_uiMsgLength;
	Type m_type;
};

struct DiscoveryRequest : public DiscoveryMessage
{
	DiscoveryRequest( AkUInt16 in_usDiscoveryResponsePort = 0 );

	virtual bool Serialize(AK::ChannelSerializer& in_rSerializer ) const;
	virtual bool Deserialize(AK::ChannelDeserializer& in_rSerializer );


	// Port on the authoring app side where we can respond to this request
	AkUInt16 m_usDiscoveryResponsePort;

	DECLARE_BASECLASS( DiscoveryMessage );
};

struct DiscoveryResponse : public DiscoveryMessage
{
	enum ConsoleState
	{
		ConsoleStateUnknown = -1,
		ConsoleStateBusy = 0,
		ConsoleStateAvailable
	};

	DiscoveryResponse();

	virtual bool Serialize(AK::ChannelSerializer& in_rSerializer ) const;
	virtual bool Deserialize(AK::ChannelDeserializer& in_rSerializer );

	AkUInt16 m_usCommandPort;
	AkUInt16 m_usNotificationPort;

	AkUInt32 m_uiProtocolVersion;
	CommunicationDefines::ConsoleType m_eConsoleType;
	const char* m_pszConsoleName;
	const char* m_pszCustomPlatformName;
	ConsoleState m_eConsoleState;
	const char* m_pszControllerName;
	const char* m_pszNetworkAppNameName;
	AkUInt8 m_bits;

	DECLARE_BASECLASS( DiscoveryMessage );
};

