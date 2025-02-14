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

#ifndef AK_DELAYPITCHSHIFT_H_
#define AK_DELAYPITCHSHIFT_H_

// Pitch shifting using fractional delay line implementation with crossfaded taps read at a different rate than written
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>
#include <AK/DSP/AkDelayLineMemory.h>
#include <AK/SoundEngine/Common/AkSimd.h>
#include <math.h>

namespace AK
{
	namespace DSP 
	{
		class AkDelayPitchShift 
		{
		public:

			AkDelayPitchShift()
				: 
#ifndef AK_VOICE_MAX_NUM_CHANNELS
				  m_DelayLines( NULL )
				, m_fFractDelay( NULL ) ,
#endif
				  m_fReadWriteRateDelta(0.f)
				, m_uNumChannels(0)
				, m_uDelayLength(0)
			{
#ifdef AK_VOICE_MAX_NUM_CHANNELS
				for ( AkUInt32 i = 0; i < AK_VOICE_MAX_NUM_CHANNELS; i++ )
					m_fFractDelay[i] = 0.f;
#endif
			}

			AKRESULT Init( 
				AK::IAkPluginMemAlloc * in_pAllocator, 
				AkReal32 in_MaxDelayTime, 
				AkUInt32 in_uNumChannels, 
				AkUInt32 in_uSampleRate 
				);
			void Term( AK::IAkPluginMemAlloc * in_pAllocator );
			void Reset();

			void SetPitchFactor( AkReal32 in_fPitchFactor );

#ifndef AKDELAYPITCHSHIFT_USETWOPASSALGO
			void ProcessChannel( 
				AkReal32 * in_pfInBuf, 
				AkReal32 * out_pfOutBuf, 
				AkUInt32 in_uNumFrames, 
				AkUInt32 in_uChanIndex	
				);
#else
			void ProcessChannel( 
				AkReal32 * in_pfInBuf, 
				AkReal32 * out_pfOutBuf, 
				void * in_pTempStorage,
				AkUInt32 in_uNumFrames, 
				AkUInt32 in_uChanIndex	
				);
#endif // AKDELAYPITCHSHIFT_USETWOPASSALGO
	
		protected:

#ifdef AK_VOICE_MAX_NUM_CHANNELS
			AK::DSP::CAkDelayLineMemory<AkReal32, 1> m_DelayLines[AK_VOICE_MAX_NUM_CHANNELS];	
			AkReal32 m_fFractDelay[AK_VOICE_MAX_NUM_CHANNELS];  // Master delay, other is slave and half delay length away
#else
			AK::DSP::CAkDelayLineMemory<AkReal32> * m_DelayLines;	
			AkReal32 * m_fFractDelay;  // Master delay, other is slave and half delay length away
#endif
			AkReal32 m_fReadWriteRateDelta;
			AkUInt32 m_uNumChannels;
			AkUInt32 m_uDelayLength; // Different from allocated length for read chase write
		};
	} // namespace DSP 
} // namespace AK

#endif // AK_DELAYPITCHSHIFT_H_

