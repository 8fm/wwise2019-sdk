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
// AkActionEvent.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _ACTION_EVENT_H_
#define _ACTION_EVENT_H_

#include "AkAction.h"

class CAkActionEvent : public CAkAction
{

public:
	//Thread safe version of the constructor
	static CAkActionEvent* Create(AkActionType in_eActionType, AkUniqueID in_ulID = 0);

	virtual AKRESULT Execute(
		AkPendingAction * in_pAction
		);

	// Set the element ID associated to the Action
	// Overload to set the Event ID
	virtual void SetElementID(
		WwiseObjectIDext in_Id//Element ID set as action target
		);

protected:

	//Constructor
	CAkActionEvent(
		AkActionType in_eActionType,	//Type of action
		AkUniqueID in_ulID
		);

	//Destructor
	virtual ~CAkActionEvent();


protected:

	AkUniqueID m_ulTargetEventID;	// Associated element

};
#endif
