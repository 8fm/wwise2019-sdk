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
// AkStmCacheMgmt.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _STM_CACHE_MGMT_H_
#define _STM_CACHE_MGMT_H_

#include <AK/SoundEngine/Common/AkTypes.h>

class CAkEvent;
class CAkRegisteredObj;

void PinFilesInStreamCache( CAkEvent* in_pEvent, CAkRegisteredObj* in_pObj, AkPriority in_uActivePriority, AkPriority in_uInactivePriority );

void UnpinFilesInStreamCache( CAkEvent* in_pEvent, CAkRegisteredObj* in_pObj );

void UnpinAllFilesInStreamCache( CAkEvent* in_pEvent );

AKRESULT GetBufferStatusForPinnedFiles( CAkEvent* in_pEvent, CAkRegisteredObj* in_pGameObj, AkReal32& out_fPercentBuffered, bool& out_bCacheMemFull );

#endif
