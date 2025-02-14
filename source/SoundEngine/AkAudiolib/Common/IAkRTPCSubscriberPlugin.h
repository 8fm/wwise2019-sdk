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

//////////////////////////////////////////////////////////////////////
//
// IAkRTPCSubscriberPlugin.h
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_RTPC_SUBSCRIBER_PLUGIN_H_
#define _AK_RTPC_SUBSCRIBER_PLUGIN_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>

#include "AkRTPC.h"

class CAkBehavioralCtx;

class IAkRTPCSubscriberPlugin
{
protected:

	virtual ~IAkRTPCSubscriberPlugin() {}

public:

	// Get plugin ID.
	virtual AkPluginID GetPluginID() = 0;

	// Update parameter from RTPC change
	virtual void UpdateTargetRtpcParam( AkRTPC_ParameterID in_paramId, AkUniqueID in_rtpcId, AkRtpcAccum in_rtpcAccum, AkReal32 in_fValue ) = 0;

	// Notification of RTPC subscription change;
	virtual void NotifyRtpcParamChanged( AkRTPC_ParameterID in_paramId, AkUniqueID in_rtpcId, AkRtpcAccum in_rtpcAccum, AkReal32 in_fValue, bool in_bRecalculate ) = 0;

#ifndef AK_OPTIMIZED
	virtual AkUniqueID GetContextID() = 0; // ContextID to assist with monitoring plug-in RTPCs
#endif
};

#endif // _AK_RTPC_SUBSCRIBER_PLUGIN_H_
