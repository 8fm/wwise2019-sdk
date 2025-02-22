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
// AkSharedEnum.h
//
// Audiokinetic Data Type Definition (internal)
//
//////////////////////////////////////////////////////////////////////
#ifndef _SHARED_ENUM_H_
#define _SHARED_ENUM_H_

//-----------------------------------------------------------------------------
// Path
//-----------------------------------------------------------------------------
enum AkPathStepOrContinuous
{
	AkPathStep			= 0,
	AkPathContinuous	= 2
};

enum AkPathSequenceOrRandom
{
	AkPathSequence		= 0,
	AkPathRandom		= 1
};

enum AkPathStepOnNewSound
{
	AkPathStepNormal = 0,
	AkPathStepNewSound = 4
};

enum AkPathMode
{
	AkStepSequence			= AkPathStep | AkPathSequence,
	AkStepRandom			= AkPathStep | AkPathRandom,
	AkContinuousSequence	= AkPathContinuous | AkPathSequence,
	AkContinuousRandom		= AkPathContinuous | AkPathRandom
};

#endif

