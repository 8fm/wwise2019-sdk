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
// AkMemoryMgrBase.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _MEMORY_MGR_BASE_H_
#define _MEMORY_MGR_BASE_H_

#include <AK/SoundEngine/Common/AkMemoryMgr.h>

namespace AK
{
	namespace MemoryMgr
	{
		extern AKRESULT InitBase();
		extern void TermBase();

		extern void	SetFrameCheckEnabled( bool in_bEnabled );
		extern bool IsFrameCheckEnabled();
		extern void SetVoiceCheckEnabled( bool in_bEnabled );
		extern bool IsVoiceCheckEnabled();
		extern AKRESULT CheckForOverwrite();
	}
}

#endif // _MEMORY_MGR_BASE_H_
