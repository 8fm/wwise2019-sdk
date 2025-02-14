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
// ALBytesMem.h
//
// IReadBytes / IWriteBytes implementation on a growing memory buffer.
// This version uses the AkMemID_Profiler category.
//
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <AK/SoundEngine/Common/AkBytesMem.h>

namespace AK
{
	class ALWriteBytesMem
		: public AK::WriteBytesMem
	{
	public:
		ALWriteBytesMem();
	};
}
