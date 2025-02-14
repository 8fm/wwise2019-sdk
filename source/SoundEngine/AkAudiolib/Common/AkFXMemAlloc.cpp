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

#include "stdafx.h" 

#include "AkFXMemAlloc.h"

#include <AK/SoundEngine/Common/AkMemoryMgr.h>

AkFXMemAlloc AkFXMemAlloc::m_instanceUpper(AkMemID_Structure);
AkFXMemAlloc AkFXMemAlloc::m_instanceLower(AkMemID_ProcessingPlugin);

#if defined (AK_MEMDEBUG)

// debug malloc
void * AkFXMemAlloc::dMalloc( 
    size_t   in_uSize,		// size
    const char*	 in_pszFile,// file name
	AkUInt32 in_uLine		// line number
	)
{
	return AK::MemoryMgr::dMalign( m_poolId, in_uSize, 16, in_pszFile, in_uLine );
}

void * AkFXMemAlloc::dMalign(
	size_t in_uSize,	// size
	size_t in_uAlign,	// alignment
	const char* in_pszFile, // file name
	AkUInt32 in_uLine // line number
)
{
	return AK::MemoryMgr::dMalign(m_poolId, in_uSize, (AkUInt32)in_uAlign, in_pszFile, in_uLine);

}

#endif

// release malloc
void * AkFXMemAlloc::Malloc( 
    size_t in_uSize		// size
    )
{
	return AK::MemoryMgr::Malign( m_poolId, in_uSize, 16 );
}

void * AkFXMemAlloc::Malign(
	size_t in_uSize,	// size
	size_t in_uAlignemt	// alignment
	)
{
	return AK::MemoryMgr::Malign(m_poolId, in_uSize, (AkUInt32)in_uAlignemt);
}

void AkFXMemAlloc::Free(
    void * in_pMemAddress	// starting address
    )
{
	AK::MemoryMgr::Falign( m_poolId, in_pMemAddress );
}
