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
 
#ifndef _AK_PITCHSHIFTERDSPPROCESS_H_
#define _AK_PITCHSHIFTERDSPPROCESS_H_

#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include "AkPitchShifterFXInfo.h"

void AkPitchShifterDSPProcess(  AkAudioBuffer * io_pBuffer, 
								AkPitchShifterFXInfo & io_FXInfo, 
								AkReal32 * in_pfBufferStorage,
								void * pTwoPassStorage
								);

#endif // _AK_PITCHSHIFTERDSPPROCESS_H_