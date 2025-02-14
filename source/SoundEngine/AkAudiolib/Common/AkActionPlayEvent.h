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
// AkActionPlayEvent.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _ACTION_PLAYEVENT_H_
#define _ACTION_PLAYEVENT_H_

#include "AkAction.h"

struct AkCntrHistArray;
class CAkTransition;

class CAkActionPlayEvent : public CAkAction
{
public:
	//Thread safe version of the constructor
	static CAkActionPlayEvent* Create(AkActionType in_eActionType, AkUniqueID in_ulID = 0);

protected:
	CAkActionPlayEvent(AkActionType in_eActionType, AkUniqueID in_ulID);
	virtual ~CAkActionPlayEvent();

public:
	//Execute the action
	//Must be called only by the audiothread
	//
	//Return - AKRESULT - AK_Success if all succeeded
	virtual AKRESULT Execute(
		AkPendingAction * in_pAction
		);

	virtual AKRESULT SetActionParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize );
};

#endif //_ACTION_PLAYEVENT_H_
