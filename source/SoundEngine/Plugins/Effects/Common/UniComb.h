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
//
// ------------------------------------------------------------------------
// Unicomb - uses an interpolated delay with a feedback and feedfwd element.
// ------------------------------------------------------------------------
//
//                                          |--------------|
//								|-----------| x fFfwdGain  |-----|
//								|			|--------------|     |
//		|-----|   x[n]   |-----------|                        |-----|  y[n]
// ---->|  +  |----------|  Z[-D-d]  |------------------------|  +  |------>
//		|-----|  		 |-----------|						  |-----|
//		   |				    | 				    				          
//		   |                    |
//         |              |--------------|
//	       |--------------| x fFBackGain |
//		 				  |--------------|
//
//
//
#ifndef _AKUNICOMB_H_
#define _AKUNICOMB_H_

#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkAssert.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
// This needs to be the maximum buffer size of all supported platforms (but not more in order to allow local better storage optimizations on PS3)
// Using this rather than the actual platform MaxFrames allows more consistency between platforms 
#define MAX_SUPPORTED_BUFFER_MAXFRAMES (1024)

namespace DSP
{

	class UniComb
	{
	public:

		UniComb() : 
					m_uDelayLength( 0 ),
					m_uMaxModWidth( 0 ),
					m_uAllocatedDelayLength( 0 ),
					m_pfDelay( 0 ),
					m_uWritePos( 0 ),
					m_fPrevFeedbackGain( 0.f ),
					m_fPrevFeedforwardGain( 0.f ),
					m_fPrevDryGain( 0.f ),
					m_fFeedbackGain( 0.f ),
					m_fFeedforwardGain( 0.f ),
					m_fDryGain( 0.f )
		{}

		AKRESULT Init(	AK::IAkPluginMemAlloc * in_pAllocator, 
						AkUInt32 in_uDelayLength,
						AkUInt32 in_uMaxBufferLength,
						AkReal32 in_fFeedbackGain,	  
						AkReal32 in_fFeedforwardGain, 
						AkReal32 in_fDryGain,
						AkReal32 in_fMaxModDepth );

		void Term( AK::IAkPluginMemAlloc * in_pAllocator );

		void Reset( );

		void SetParams(		AkReal32 in_fFeedbackGain,
							AkReal32 in_fFeedforwardGain,
							AkReal32 in_fDryGain,
							AkReal32 in_fMaxModDepth );

		// Process delay buffer in place.
		void ProcessBuffer( 
			AkReal32 * io_pfBuffer, 
			AkUInt32 in_uNumFrames, 
			AkReal32 * in_pfLFOBuf  ); 

	protected:

	// Process in-place without LFO modulation. 
	void ProcessBufferNoLFO(	
		AkReal32 * io_pfBuffer, 
		AkUInt32 in_uNumFrames, 
		AkReal32 * io_pfDelay // Scratch mem on SPU
		);

	// Process modulated delay buffer in place. 
	void ProcessBufferLFO(	
		AkReal32 * io_pfBuffer, 
		AkUInt32 in_uNumFrames, 
		AkReal32 * AK_RESTRICT in_pfLFOBuf,
		AkReal32 * io_pfDelay // Scratch mem on SPU
		);

		void InterpolationRampsDone()
		{
			m_fPrevFeedbackGain    =	m_fFeedbackGain;
			m_fPrevFeedforwardGain =	m_fFeedforwardGain;
			m_fPrevDryGain         =	m_fDryGain;
		}

	protected:

		AkUInt32 m_uDelayLength;			// InterpolatedDelayLine line nominal length
		AkUInt32 m_uMaxModWidth;			// Maximum expected variation of the delay length
		AkUInt32 m_uAllocatedDelayLength;	// InterpolatedDelayLine line total allocated length
		AkReal32 *m_pfDelay;				// InterpolatedDelayLine line start position

		AkUInt32 m_uWritePos;				// Current write position in the delay line.

		AkReal32 m_fPrevFeedbackGain;
		AkReal32 m_fPrevFeedforwardGain;
		AkReal32 m_fPrevDryGain;

		AkReal32 m_fFeedbackGain;
		AkReal32 m_fFeedforwardGain;
		AkReal32 m_fDryGain;

	};

} // namespace DSP

#endif // _AKUNICOMB_H_
