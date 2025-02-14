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

#ifndef AK_OPTIMIZED

#include "IProxyFrameworkConnected.h"
#include "ICommandChannelHandler.h"
#include "ICommunicationCentralNotifyHandler.h"
#include "RendererProxyConnected.h"
#include "StateMgrProxyConnected.h"
#include "MidiDeviceMgrProxyConnected.h"
#include "ALMonitorProxyConnected.h"
#include "ObjectProxyConnected.h"
#include "CommandDataSerializer.h"

#include <AK/Tools/Common/AkHashList.h>
#include <AK/Tools/Common/AkObject.h>
#include "AkPrivateTypes.h"
#include "AkAction.h"

class CommandDataSerializer;

class ProxyFrameworkConnected
	: public AK::Comm::IProxyFrameworkConnected
{
public:
	ProxyFrameworkConnected();
	virtual ~ProxyFrameworkConnected();

	// IProxyFramework members
	virtual void Destroy();

	virtual void Init();
	virtual void Term();

	virtual void SetNotificationChannel( AK::Comm::INotificationChannel* in_pNotificationChannel );
	
	virtual AK::Comm::IRendererProxy* GetRendererProxy();

	// ICommandChannelHandler members
	virtual const AkUInt8* HandleExecute( const AkUInt8* in_pData, AkUInt32 & out_uReturnDataSize );

	// ICommunicationCentralNotifyHandler members
	virtual void PeerDisconnected();

	typedef AkHashList< AkUInt64, ObjectProxyConnectedWrapper, ProxyCommandData::ArrayPool > ID2ProxyConnected;

private:
	void ProcessProxyStoreCommand( CommandDataSerializer& io_rSerializer );
	ID2ProxyConnected::Item * CreateAction( AkUniqueID in_actionID, AkActionType in_eActionType );

	ID2ProxyConnected m_id2ProxyConnected;

	RendererProxyConnected m_rendererProxy;
	StateMgrProxyConnected m_stateProxy;
	MidiDeviceMgrProxyConnected m_midiDeviceMgrProxy;
	ALMonitorProxyConnected m_monitorProxy;

    CommandDataSerializer m_returnData;//Making it a member, avoiding millions of allocations.
};

namespace ObjectProxyStoreCommandData
{
	struct Create;
}

typedef void (*AkExternalProxyHandlerCallback)( ObjectProxyStoreCommandData::Create& in_Create, ProxyFrameworkConnected::ID2ProxyConnected::Item *& out_pProxyItem, const long in_lProxyItemOffset, AkMemPoolId in_PoolID );
#endif // #ifndef AK_OPTIMIZED
