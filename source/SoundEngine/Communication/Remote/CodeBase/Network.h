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


/*	----------------------------------------------------------------------------------------
	Common file used by the communication for general network related functionalities.
	Location: \Wwise\Communication\Remote\CodeBase
	Implementation location: In the platform folders: Wwise\Communication\Remote\{Platform}
	
	See more information in the implementation file.
	-----------------------------------------------------------------------------------------
*/

#pragma once

#include "AkPrivateTypes.h"
#include <AK/Comm/AkCommunication.h>

namespace Network
{
	AK_EXTERNAPIFUNC(AKRESULT, Init( AkMemPoolId in_pool, bool in_bInitSystemLib ));
	AK_EXTERNAPIFUNC(void, Term(bool in_bTermSystemLib));
	AK_EXTERNAPIFUNC(AkInt32, GetLastError());
	
	// For some platforms, network resources are available only after a delay.
	// Returns true if Init can be called at this moment.
	AK_EXTERNAPIFUNC(bool, IsReady());

	void GetMachineName( char* out_pMachineName, AkInt32* io_pStringSize );

	AK_EXTERNAPIFUNC(bool, SameEndianAsNetwork());

	AkCommSettings::AkCommSystem GetCommunicationSystem();
	bool IsDiscoverySystemEnabled();
}
