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
// AkLEngineDefs.h
//
// Defines, enums, and structs for inter-engine interface.
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_LENGINE_DEFS_H_
#define _AK_LENGINE_DEFS_H_

#include <AK/Tools/Common/AkSmartPtr.h>

//-----------------------------------------------------------------------------
// Name: enum AkSrcType
// Desc: Enumeration of types of sources.
//-----------------------------------------------------------------------------
enum AkSrcType
{
    SrcTypeNone     = 0,
	SrcTypeFile     = 1,
    SrcTypeModelled = 2,
    SrcTypeMemory   = 3	
#define SRC_TYPE_NUM_BITS   (5)
};

//-----------------------------------------------------------------------------
// Name: struct AkFXDesc
// Desc: Effect passing info
//-----------------------------------------------------------------------------
struct AkFXDesc
{
	CAkSmartPtr<class CAkFxBase> pFx;
	AkInt16 iBypassed;
};

#endif // _AK_LENGINE_DEFS_H_
