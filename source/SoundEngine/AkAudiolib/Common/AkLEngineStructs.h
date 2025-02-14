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

#ifndef _AK_LENGINE_STRUCTS_COMMON_H_
#define _AK_LENGINE_STRUCTS_COMMON_H_

#include "AkCommon.h"

enum VPLNodeState
{
	NodeStateInit			= 0
	,NodeStatePlay			= 1
	,NodeStateStop			= 2
	,NodeStatePause			= 3

	,NodeStateIdle			= 4 // only used for busses for idle time due to virtual voices / wait for first output
};

struct AkVPLState : public AkPipelineBuffer
{	
	AKRESULT result;
};

#endif //_AK_LENGINE_STRUCTS_COMMON_H_
