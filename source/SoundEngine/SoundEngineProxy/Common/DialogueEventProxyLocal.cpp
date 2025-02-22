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

#include "stdafx.h"
#ifndef AK_OPTIMIZED
#ifndef PROXYCENTRAL_CONNECTED

#include "DialogueEventProxyLocal.h"
#include "AkDialogueEvent.h"
#include "AkAudioLib.h"
#include "AkCritical.h"
#include "AkDialogueEvent.h"

DialogueEventProxyLocal::DialogueEventProxyLocal( AkUniqueID in_id ): MultiSwitchProxyLocal<ObjectProxyLocal, CAkDialogueEvent, AkIdxType_DialogueEvent>(in_id)
{
}

DialogueEventProxyLocal::~DialogueEventProxyLocal()
{
}


#endif // #ifndef PROXYCENTRAL_CONNECTED
#endif // #ifndef AK_OPTIMIZED
