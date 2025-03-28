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

#include "AkAudioLib.h"
#include "IDialogueEventProxy.h"
#include "MultiSwitchProxyConnected.h"
#include "ObjectProxyConnected.h"
#include "AkDialogueEvent.h"

class DialogueEventProxyConnectedBase : public ObjectProxyConnected
{
public:
	DialogueEventProxyConnectedBase(){};
	virtual ~DialogueEventProxyConnectedBase(){};
	virtual void HandleExecute( AkUInt16, CommandDataSerializer&, CommandDataSerializer&){};
};

class DialogueEventProxyConnected : 
	public MultiSwitchProxyConnected<DialogueEventProxyConnectedBase, CAkDialogueEvent, AkIdxType_DialogueEvent>
{
public:
	DialogueEventProxyConnected( AkUniqueID in_id );
	virtual ~DialogueEventProxyConnected();

private:
	typedef MultiSwitchProxyConnected<DialogueEventProxyConnectedBase, CAkDialogueEvent, AkIdxType_DialogueEvent> __base;
};

#endif // #ifndef AK_OPTIMIZED
