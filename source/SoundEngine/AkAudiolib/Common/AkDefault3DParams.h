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
// AkDefault3DParams.h
//
// Platform "independent" default 3D parameters defines.
// (Different coordinates systems forces us to #ifdef).
//
//////////////////////////////////////////////////////////////////////
#ifndef _DEFAULT_3DPARAMS_H_
#define _DEFAULT_3DPARAMS_H_

// Common.
#define AK_DEFAULT_TOP_X						(0.0f)
#define AK_DEFAULT_TOP_Y						(1.0f)
#define AK_DEFAULT_TOP_Z						(0.0f)

// Sound position.
//////////////////////////////////////////////////////////////////////

#define AK_DEFAULT_SOUND_POSITION_X				(0.0f)				// at coordinate system's origin
#define AK_DEFAULT_SOUND_POSITION_Y				(0.0f)				
#define AK_DEFAULT_SOUND_POSITION_Z				(0.0f)				
#define AK_DEFAULT_SOUND_ORIENTATION_X			(0.0f)				
#define AK_DEFAULT_SOUND_ORIENTATION_Y			(0.0f)				
#define AK_DEFAULT_SOUND_ORIENTATION_Z			(1.0f)				// looking toward Z

// Listener position.
//////////////////////////////////////////////////////////////////////

#define AK_DEFAULT_LISTENER_POSITION_X			(0.0f)				// at coordinate system's origin
#define AK_DEFAULT_LISTENER_POSITION_Y			(0.0f)
#define AK_DEFAULT_LISTENER_POSITION_Z			(0.0f)
#define AK_DEFAULT_LISTENER_FRONT_X				(0.0f)				// looking toward Z
#define AK_DEFAULT_LISTENER_FRONT_Y				(0.0f)
#define AK_DEFAULT_LISTENER_FRONT_Z				(1.0f)

#endif // _DEFAULT_3DPARAMS_H_

