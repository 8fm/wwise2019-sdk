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
// AkActionActive.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _ACTION_ACTIVE_H_
#define _ACTION_ACTIVE_H_

#include "AkAction.h"
#include "AkActionExcept.h"
#include "PrivateStructures.h"                         // for ActionParamType

class CAkRegisteredObj;

struct ActionParams;

class CAkActionActive : public CAkActionExcept
{

protected:

	//Constructor
	CAkActionActive(AkActionType in_eActionType, AkUniqueID in_ulID);

	//Destructor
	virtual ~CAkActionActive();


protected:
	//Execute object specific execution
	AKRESULT Exec(
		ActionParamType in_eType,
		CAkRegisteredObj * in_pGameObj,		//Game object pointer
		AkPlayingID in_TargetPlayingID
		);

	void AllExecExcept(
		ActionParamType in_eType,
		CAkRegisteredObj * in_pGameObj,
		AkPlayingID in_TargetPlayingID
		);

	void ExecOnDynamicSequences(ActionParams in_params);

	virtual AKRESULT SetActionParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize );

	virtual void SetActionActiveParams( ActionParams& io_params ) const {}
};
#endif
