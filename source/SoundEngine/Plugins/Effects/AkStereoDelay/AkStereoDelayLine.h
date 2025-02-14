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

// Stereo delay line with feedback and crossfeed between channels (each channel with distinct delay time)
// Length of delay line is aligned on 4 frames boundary (i.e. may not be suited for reverberation for example)
// Processing frames also must always be aligned on 4 frames boundary
// Vector processing (SIMD)

#ifndef _AKSTEREODELAYLINE_
#define _AKSTEREODELAYLINE_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>
#include <AK/DSP/AkDelayLineMemory.h>
#include "BiquadFilter.h"
#include "AkStereoDelayFXParams.h"

namespace AK
{
	namespace DSP
	{
		class CStereoDelayLine
		{
		public:
			CStereoDelayLine( ){}

			// Delay line will be rounded to 4 frame boundary
			AKRESULT Init( AK::IAkPluginMemAlloc * in_pAllocator, AkReal32 in_fDelayTimes[2], AkUInt32 in_uSampleRate );
			void Term( AK::IAkPluginMemAlloc * in_pAllocator );
			void Reset( );	

			void ProcessBuffer( 
				AkAudioBuffer * in_pBuffer,					
				AkAudioBuffer * out_pBuffer,
				AkStereoDelayChannelParams in_PrevStereoDelayParams[2],
				AkStereoDelayChannelParams in_CurStereoDelayParams[2],
				AkStereoDelayFilterParams & in_FilterParams,
				bool in_bRecomputeFilterParams
				);

		protected:
#ifdef AK_VOICE_MAX_NUM_CHANNELS
			AK::DSP::CAkDelayLineMemory<AkReal32,1> m_DelayLines[2];	
#else
			AK::DSP::CAkDelayLineMemory<AkReal32> m_DelayLines[2];
#endif
			::DSP::BiquadFilterMultiSIMD m_FeedbackFilter;
			AkUInt32 m_uSampleRate;
		};

	} // namespace DSP
} // namespace AK

#endif // _AKSTEREODELAYLINE_