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

#ifndef _AKLINEARRESAMPLER_H_
#define _AKLINEARRESAMPLER_H_

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>

namespace DSP
{
	class CAkLinearResampler
	{
	public:

		CAkLinearResampler() 
			: m_fInterpLoc( 0.f )
#ifndef AK_VOICE_MAX_NUM_CHANNELS
			, m_fPastVals( NULL )
#endif
		{
		}

		void Reset( AkUInt32 in_uNumChannels )
		{
			m_fInterpLoc = 0.f;
			AKPLATFORM::AkMemSet( m_fPastVals, 0, in_uNumChannels * sizeof(AkReal32) );
		}

		void Execute(	AkAudioBuffer * io_pInBuffer, 
						AkUInt32		in_uInOffset,
						AkAudioBuffer * io_pOutBuffer,
						AkReal32		in_fResamplingFactor);

	protected:
	
		AkReal32					m_fInterpLoc;
#ifdef AK_VOICE_MAX_NUM_CHANNELS
		AkReal32					m_fPastVals[AK_VOICE_MAX_NUM_CHANNELS];
#else
		AkReal32 *					m_fPastVals;
#endif
	};

} // namespace DSP


#endif // _AKLINEARRESAMPLER_H_
