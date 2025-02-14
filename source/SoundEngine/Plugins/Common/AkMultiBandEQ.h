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

#ifndef _AK_MULTIBANDEQ_H
#define _AK_MULTIBANDEQ_H

#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include "BiquadFilter.h"
#include <AK/Tools/Common/AkAssert.h>

namespace DSP
{
	class CAkMultiBandEQ
	{
	public:


		CAkMultiBandEQ();
		AKRESULT Init( AK::IAkPluginMemAlloc * in_pAllocator, AkUInt16 in_uNumChannels, AkUInt16 in_uNumBands );
		void Term( AK::IAkPluginMemAlloc * in_pAllocator );
		void Reset();

		// Compute filter coefficients for a given band
		void SetCoefficients( 
			AkUInt32 in_uBand,
			AkUInt32 in_uSampleRate, 
			::DSP::FilterType in_eCurve, 			
			AkReal32 in_fFreq, 
			AkReal32 in_fGain = 0.f, 
			AkReal32 in_fQ = 1.f );

		// Bypass/unbypass a given band
		void SetBandActive( AkUInt32 in_uBand, bool in_bActive );

		// Process all channels
		void ProcessBuffer(	AkAudioBuffer * io_pBuffer );

	protected:
		
		DSP::BiquadFilterMultiSIMD *	m_pFilters;
		AkUInt32 m_uEnabledBandMask;
		AkUInt16 m_uNumBands;
		AkUInt16 m_uNumChannels;
	};
} // namespace DSP
#endif // _AK_MULTIBANDEQ_H
