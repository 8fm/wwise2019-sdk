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

////////////////////////////////////////////////////////////////////////////////
//
// ALBytesMem.cpp
//
// IReadBytes / IWriteBytes implementation on a memory buffer.
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "ALBytesMem.h"
#include <AK/SoundEngine/Common/AkMemoryMgr.h>

namespace AK
{
	//------------------------------------------------------------------------------
	// ALWriteBytesMem

	ALWriteBytesMem::ALWriteBytesMem()
		: WriteBytesMem()
	{
		SetMemPool(AkMemID_Profiler);
	}
}
