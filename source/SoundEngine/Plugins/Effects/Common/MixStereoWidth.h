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

// Simple mix of 2 signals with interpolating volume
// Can be used as wet/dry mix
// To be used on mono signals, create as many instances as there are channels if need be

#ifndef _AKMIXSTEREOWIDTH_H_
#define _AKMIXSTEREOWIDTH_H_

#include <AK/SoundEngine/Common/AkCommonDefs.h>

namespace DSP
{
	void ComputeLRMixGains(
		AkReal32 in_fStereoWidthPrev,
		AkReal32 in_fStereoWidthNext,
		AkReal32 & out_fGain1,
		AkReal32 & out_fGain2,
		AkReal32 & out_fPrevGain1,
		AkReal32 & out_fPrevGain2
		);

	// In-place processing routine ( io_pfIn1Out == Input1 and output buffer )
	void MixStereoWidth(AkReal32 * AK_RESTRICT io_pfIn1Out, 
						AkReal32 * AK_RESTRICT io_pfIn2Out, 
						AkUInt32 in_uNumFrames,
						AkReal32 in_fPrevStereoWidth, 
						AkReal32 in_fStereoWidth );

	// Out-of-place processing routine 
	void MixStereoWidth(AkReal32 * AK_RESTRICT in_pfIn1, 
						AkReal32 * AK_RESTRICT in_pfIn2, 
						AkReal32 * AK_RESTRICT out_pfOut1, 
						AkReal32 * AK_RESTRICT out_pfOut2, 
						AkUInt32 in_uNumFrames,
						AkReal32 in_fPrevStereoWidth, 
						AkReal32 in_fStereoWidth );
						
	// In-place processing routine 
	void MixStereoWidth(AkAudioBuffer *io_pBuffer, 
						AkReal32 in_fPrevStereoWidth, 
						AkReal32 in_fStereoWidth );

} // namespace DSP

#endif // _AKMIXSTEREOWIDTH_H_
