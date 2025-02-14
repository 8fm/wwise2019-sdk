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
// AkActionParams.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _ACTION_PARAMS_H_
#define _ACTION_PARAMS_H_

#include "AkActionExcept.h"
#include "PrivateStructures.h"

class CAkParameterNodeBase;
class CAkRegisteredObj;

struct ActionParams
{
	ActionParamType eType;
	CAkRegisteredObj * pGameObj;
	AkPlayingID		playingID;
	TransParams     transParams;
	bool			bIsFromBus;
	bool			bIsMasterCall;
	bool			bIsMasterResume;
	bool			bApplyToStateTransitions;
	bool			bApplyToDynamicSequence;
	CAkParameterNodeBase* targetNodePtr;

	ActionParams()
		: pGameObj( NULL )
		, bIsFromBus( false )
		, bIsMasterCall( false )
		, bIsMasterResume( false )
		, bApplyToStateTransitions( false )
		, bApplyToDynamicSequence( false )
		, targetNodePtr( NULL )
	{}
};

struct ActionParamsExcept : public ActionParams
{
	ExceptionList*  pExeceptionList;

	ActionParamsExcept()
		:pExeceptionList(NULL){}
};

struct SeekActionParams : public ActionParamsExcept
{
	union
	{
		AkTimeMs		iSeekTime;
		AkReal32		fSeekPercent;
	};
	AkUInt8			bIsSeekRelativeToDuration	:1;
	AkUInt8			bSnapToNearestMarker		:1;
};

#endif // _ACTION_PARAMS_H_
