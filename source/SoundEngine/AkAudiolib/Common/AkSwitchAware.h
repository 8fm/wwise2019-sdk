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

//////////////////////////////////////////////////////////////////////
//
// AkSwitchAware.h
// Basic interface and implementation for containers that are 
// switch/state dependent.
//
//////////////////////////////////////////////////////////////////////
#ifndef _SWITCH_AWARE_H_
#define _SWITCH_AWARE_H_

class AkRTPCExceptionChecker;
class AkRTPCKey;

// Switch containers base.
class CAkSwitchAware
{
public:

    CAkSwitchAware();
    virtual ~CAkSwitchAware();

	virtual void SetSwitch( 
        AkUInt32 in_Switch, 
        const AkRTPCKey& in_rtpcKey,
		AkRTPCExceptionChecker* in_pExCheck = NULL
        ) = 0;

    AkSwitchStateID GetSwitchToUse( 
		const AkRTPCKey& in_rtpcKey,
        AkUInt32 in_ulGroup, 
        AkGroupType in_eGroupType 
        );

protected:
	
    void	 UnsubscribeSwitches();

    AKRESULT SubscribeSwitch( 
        AkUInt32 in_ulGroup, 
        AkGroupType in_eGroupType 
        );
};
#endif //_SWITCH_AWARE_H_
