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

// Interpolated delay y[n] = x[n - D - d] ; 0 < d < 1

#include "UniComb.h"

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <math.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/Tools/Common/AkAssert.h>

#define AK_ALIGN_TO_NEXT_BOUNDARY( __num__, __boundary__ ) (((__num__) + ((__boundary__)-1)) & ~((__boundary__)-1))
#define MAXTRANSFERSIZE_SAMPLES (4*1024) // 16kB DMA transfers on PS3
namespace DSP
{
	AKRESULT UniComb::Init(	AK::IAkPluginMemAlloc * in_pAllocator, 
							AkUInt32 in_uDelayLength,
							AkUInt32 in_uMaxBufferLength,
							AkReal32 in_fFeedbackGain,	 
							AkReal32 in_fFeedforwardGain,
							AkReal32 in_fDryGain,
							AkReal32 in_fMaxModDepth )
	{
		const AkUInt32 s_uMIN_SAMPLE_DELAY = 8; 
		in_uDelayLength = AkMax( in_uDelayLength, s_uMIN_SAMPLE_DELAY );
		m_uDelayLength = AK_ALIGN_TO_NEXT_BOUNDARY( in_uDelayLength, 4 ); // painless SPU stitching on SPU requires alignment

		SetParams(	in_fFeedbackGain,
					in_fFeedforwardGain,
					in_fDryGain,
					in_fMaxModDepth );

		InterpolationRampsDone(); // Will set previous values to current ones to avoid initial interpolation

		// Avoid reading memory in write location to simplify things on SPU
		// Maximum sample excursion from nominal delay position can be full delay line length	
		m_uAllocatedDelayLength = MAX_SUPPORTED_BUFFER_MAXFRAMES+2*m_uDelayLength;
		m_pfDelay = (AkReal32*)AK_PLUGIN_ALLOC( in_pAllocator,  AK_ALIGN_SIZE_FOR_DMA( sizeof(AkReal32) * m_uAllocatedDelayLength ) );
		if ( m_pfDelay == NULL )
			return AK_InsufficientMemory;

		m_uWritePos = 0;

		return AK_Success;
	}

	void UniComb::Term( AK::IAkPluginMemAlloc * in_pAllocator )
	{
		if ( m_pfDelay )
		{
			AK_PLUGIN_FREE( in_pAllocator, m_pfDelay );
			m_pfDelay = NULL;
			m_uAllocatedDelayLength = 0;
		}
	}

	void UniComb::Reset( )
	{
		if ( m_pfDelay )
		{
			AkZeroMemLarge( m_pfDelay, AK_ALIGN_SIZE_FOR_DMA( sizeof(AkReal32) * m_uAllocatedDelayLength ) );
		}
		// Setup initial delay line offsets (pointers)
		m_uWritePos = 0;
	}

	void UniComb::SetParams(		
		AkReal32 in_fFeedbackGain,
		AkReal32 in_fFeedforwardGain,
		AkReal32 in_fDryGain,
		AkReal32 in_fMaxModDepth )
	{
		m_fFeedbackGain		= in_fFeedbackGain;
		m_fFeedforwardGain	= in_fFeedforwardGain;
		m_fDryGain			= in_fDryGain;
		m_uMaxModWidth		= (AkUInt32)(in_fMaxModDepth*m_uDelayLength)/4*4;

		// Note: The restriction below is only necessary on PS3 platform. Other platform could unblock a larger range of possible depths 
		// (when using custom modification allowing longer delay times). 
		// This restriction is used on all platform only for consistent behavior accross platforms.

		// In (unlikely) case of large delay lengths used with large modulation depth, the optimized PIC storage algorithm cannot be used
		// and the effect requires too much local storage memory. Deal with this by hard capping the maximum size of the delay modulation extent.
		// Always preserve specified delay time but reduce the modulation depth in this case.
		// The value used below means that there is no depth reduction until delay time is above 250 ms
		// Performance note: Using large delay and large modulation may yield poor performance due to large memory tranfers required 
		const AkUInt32 uMaxReadMemLSSize = 100*1024; // 100 kB ~= 250 ms delay time @ sr = 48 kHz
		AkUInt32 uReadMemSize = AK_ALIGN_TO_NEXT_BOUNDARY( (MAX_SUPPORTED_BUFFER_MAXFRAMES+2*m_uMaxModWidth)*sizeof(AkReal32), 16 );
		if ( uReadMemSize >= uMaxReadMemLSSize )
		{
			// Reduce modulation depth enough to be able to use optimized LS algorithm for very large delays
			// Note: The formula below is simplified and does not consider alignment and rounding considerations as above.
			// It does not matter as the hard cut limit is arbitrary and accounts for a reasonable margin of errors already
			AkReal32 fSafeModDepth = AkReal32(uMaxReadMemLSSize - MAX_SUPPORTED_BUFFER_MAXFRAMES*sizeof(AkReal32))/(2.f*m_uDelayLength*sizeof(AkReal32));
			m_uMaxModWidth = (AkUInt32)(fSafeModDepth*m_uDelayLength)/4*4;
			AKASSERT( m_uMaxModWidth <= uMaxReadMemLSSize+1024 ); // Within a kilobyte of the set limit
		}

		// Avoid full range modulation and possible resulting limit conditions
		if ( m_uMaxModWidth >= m_uDelayLength )
			m_uMaxModWidth -= 4;
	}

#define DEFINE_VALUE_INCREMENT( __gainInc__, __gainCurr__, __tgtGain__, __prevGain__ ) const AkReal32 __gainInc__ = (__tgtGain__ - __prevGain__ )/in_uNumFrames; \
	AkReal32 __gainCurr__ = __prevGain__ ;
#define UPDATE_VALUE_BY_INCREMENT( __gainCurr__, __gainInc__ ) __gainCurr__ += __gainInc__;

	// Process delay buffer in place.
	void UniComb::ProcessBuffer(	AkReal32 * io_pfBuffer, 
									AkUInt32 in_uNumFrames, 
									AkReal32 * in_pfLFOBuf )
	{
		if ( in_pfLFOBuf )
			ProcessBufferLFO( io_pfBuffer, in_uNumFrames, in_pfLFOBuf, m_pfDelay  );
		else
			ProcessBufferNoLFO( io_pfBuffer, in_uNumFrames, m_pfDelay );
	}

	// Process in-place without LFO modulation.
	void UniComb::ProcessBufferNoLFO(	
		AkReal32 * io_pfBuffer, 
		AkUInt32 in_uNumFrames, 
		AkReal32 * io_pfDelay
		)
	{
		AkReal32 * const AK_RESTRICT pfDelay = (AkReal32 * ) io_pfDelay;
		AkReal32 * AK_RESTRICT pfBuf = (AkReal32 * ) io_pfBuffer;
		
		DEFINE_VALUE_INCREMENT(fFeedbackGainInc,	fCurrFeedbackGain,		m_fFeedbackGain,		m_fPrevFeedbackGain		);
		DEFINE_VALUE_INCREMENT(fFeedforwardGainInc,	fCurrFeedforwardGain,	m_fFeedforwardGain,		m_fPrevFeedforwardGain	);
		DEFINE_VALUE_INCREMENT(fDryGainInc,			fCurrDryGain,			m_fDryGain,				m_fPrevDryGain			);

		AkUInt32 uWritePos = m_uWritePos;
		const AkUInt32 uAllocatedDelayLength = m_uAllocatedDelayLength;
		const AkUInt32 uNominalDelayPos = uAllocatedDelayLength-m_uDelayLength;

		while(in_uNumFrames)
		{
			//A large part of the LFO buffer can be processed without wrap around.  Compute the actual limits.  
			//Computing MOD takes lots of time, it is worth avoiding it.  From profiling, this version of the code is 130% faster (yes, more than twice) 
			//than simply processing the loop with the MODs.	
			AkUInt32 uReadPos = (uWritePos + uNominalDelayPos) % uAllocatedDelayLength;
			AkUInt32 uFramesBeforeWritePosWraps = uAllocatedDelayLength - uWritePos;
			AkUInt32 uFramesBeforeReadPosWraps = uAllocatedDelayLength - uReadPos;
			AkUInt32 uFramesToProcess = AkMin(AkMin(uFramesBeforeWritePosWraps, uFramesBeforeReadPosWraps), in_uNumFrames);
			in_uNumFrames -= uFramesToProcess;

			// No modulation
			while(uFramesToProcess)
			{
				uFramesToProcess--;
				// This tap corresponds the center position for the modulated read tap
				AkReal32 fReadTapOut = pfDelay[uReadPos];

				UPDATE_VALUE_BY_INCREMENT(	fCurrFeedbackGain,		fFeedbackGainInc );
				UPDATE_VALUE_BY_INCREMENT(	fCurrFeedforwardGain,	fFeedforwardGainInc	);
				UPDATE_VALUE_BY_INCREMENT(	fCurrDryGain,			fDryGainInc	);

				// Process delay line
				AkReal32 fDelayInput = *pfBuf + fReadTapOut*fCurrFeedbackGain;
				AkReal32 fWetOutput = fCurrFeedforwardGain*fReadTapOut + fCurrDryGain*fDelayInput;

				// Process delay line: DelayLine(WritePos+1,:) = DelayInput;
				pfDelay[uWritePos] = fDelayInput;
				*pfBuf++ = fWetOutput;

				// Increment read & write pos.
				uWritePos++;
				uReadPos++;				
			}

			uWritePos %= uAllocatedDelayLength;
		}

		m_uWritePos = uWritePos;

		InterpolationRampsDone();
	}

#define PROCESS_ITERATION																			\
	fLFOVal = *in_pfLFOBuf++;																		\
	fModTapPosFloat = uModTapNominalPos + uMaxModWidth * fLFOVal;									\
	uModTapPosInt = AkUInt32(fModTapPosFloat);														\
	fModPosFracPart = fModTapPosFloat - uModTapPosInt;												\
	uModTapPosPrev = uModTapPosInt;																	\
	uModTapPosNext = uModTapPosInt+1;																\
	fModTapValPrevLeft = pfDelay[uModTapPosPrev];													\
	fModTapValNextLeft = pfDelay[uModTapPosNext];													\
	fModTapOut = fModTapValNextLeft*fModPosFracPart + (1.f-fModPosFracPart)*fModTapValPrevLeft;		\
	UPDATE_VALUE_BY_INCREMENT(	fCurrFeedbackGain,		fFeedbackGainInc );							\
	UPDATE_VALUE_BY_INCREMENT(	fCurrFeedforwardGain,	fFeedforwardGainInc	);						\
	UPDATE_VALUE_BY_INCREMENT(	fCurrDryGain,			fDryGainInc	);								\
	fDelayInput = *pfBuf + fModTapOut*fCurrFeedbackGain;											\
	fWetOutput = fCurrFeedforwardGain*fModTapOut + fCurrDryGain*fDelayInput;						\
	pfDelay[uWritePos] = fDelayInput;																\
	*pfBuf++ = fWetOutput;																			\
	uWritePos++;																					\
	uModTapNominalPos++;

#define NUM_ITERATIONS 8
																									
	// Process modulated delay buffer in place. 													
	void UniComb::ProcessBufferLFO(	
		AkReal32 * io_pfBuffer, 
		AkUInt32 in_uNumFrames, 
		AkReal32 * AK_RESTRICT in_pfLFOBuf,
		AkReal32 * io_pfDelay
		)
	{
		AkReal32 * const AK_RESTRICT pfDelay = (AkReal32 * ) io_pfDelay;
		AkReal32 * AK_RESTRICT pfBuf = (AkReal32 * ) io_pfBuffer;

		DEFINE_VALUE_INCREMENT(fFeedbackGainInc,	fCurrFeedbackGain,		m_fFeedbackGain,		m_fPrevFeedbackGain		);
		DEFINE_VALUE_INCREMENT(fFeedforwardGainInc,	fCurrFeedforwardGain,	m_fFeedforwardGain,		m_fPrevFeedforwardGain	);
		DEFINE_VALUE_INCREMENT(fDryGainInc,			fCurrDryGain,			m_fDryGain,				m_fPrevDryGain			);

		AkUInt32 uWritePos = m_uWritePos;
		const AkUInt32 uAllocatedDelayLength = m_uAllocatedDelayLength;
		const AkUInt32 uNominalDelayPos = uAllocatedDelayLength-m_uDelayLength;
		const AkUInt32 uMaxModWidth = m_uMaxModWidth;
		const AkUInt32 uLastLFOSampleNoWrap = uAllocatedDelayLength - uMaxModWidth - 1;

		AkUInt32 uModTapNominalPos = (uWritePos + uNominalDelayPos) % uAllocatedDelayLength;

		AkReal32 fLFOVal, fModTapPosFloat, fModPosFracPart, fModTapOut, fModTapValPrevLeft, fModTapValNextLeft, fDelayInput, fWetOutput;
		AkUInt32 uModTapPosInt, uModTapPosPrev, uModTapPosNext;

		//A large part of the LFO buffer can be processed without wrap around.  Compute the actual limits.  
		//Computing MOD takes lots of time, it is worth avoiding it.  From profiling, this version of the code is 21% faster than simply processing the loop with the MODs.	
		while(in_uNumFrames)
		{ 
			//Is the current part we're about to process straddling one of the two boundaries?
			AkUInt32 uFramesToProcess = AkMin(uLastLFOSampleNoWrap-uModTapNominalPos, uAllocatedDelayLength-1 - uWritePos);
			uFramesToProcess = AkMin(uFramesToProcess, in_uNumFrames);
			while(in_uNumFrames && (uModTapNominalPos >= uLastLFOSampleNoWrap || uModTapNominalPos < m_uMaxModWidth || uWritePos == uAllocatedDelayLength-1 || uFramesToProcess < NUM_ITERATIONS))
			{
				fLFOVal = *in_pfLFOBuf++;

				// Compute fractional delay line output
				// This tap corresponds the center position for the modulated read tap
				uModTapNominalPos = uWritePos + uNominalDelayPos;

				// Read modulated tap (fractional delay line)
				fModTapPosFloat = uModTapNominalPos + uMaxModWidth * fLFOVal;
				uModTapPosInt = AkUInt32(fModTapPosFloat);
				fModPosFracPart = fModTapPosFloat - uModTapPosInt;

				// Previous and next integral delay positions bounding fractional delay.
				uModTapPosPrev = uModTapPosInt % uAllocatedDelayLength;
				uModTapPosNext = (uModTapPosInt+1) % uAllocatedDelayLength;

				// Signal values at previous and next integral delay positions bounding fractional delay.
				fModTapValPrevLeft = pfDelay[uModTapPosPrev];
				fModTapValNextLeft = pfDelay[uModTapPosNext];

				//  Linearly interpolated value for the signal value at fractional delay.
				fModTapOut = fModTapValNextLeft*fModPosFracPart + (1.f-fModPosFracPart)*fModTapValPrevLeft;

				UPDATE_VALUE_BY_INCREMENT(	fCurrFeedbackGain,		fFeedbackGainInc );
				UPDATE_VALUE_BY_INCREMENT(	fCurrFeedforwardGain,	fFeedforwardGainInc	);
				UPDATE_VALUE_BY_INCREMENT(	fCurrDryGain,			fDryGainInc	);

				// Process delay line
				fDelayInput = *pfBuf + fModTapOut*fCurrFeedbackGain;
				fWetOutput = fCurrFeedforwardGain*fModTapOut + fCurrDryGain*fDelayInput;

				// Process delay line: DelayLine(WritePos+1,:) = DelayInput;
				pfDelay[uWritePos] = fDelayInput;
				*pfBuf++ = fWetOutput;

				// Increment and wrap write position.
				uWritePos = (uWritePos + 1) % uAllocatedDelayLength;
				uModTapNominalPos = (uModTapNominalPos+1) % uAllocatedDelayLength;
				in_uNumFrames--;

 				uFramesToProcess = AkMin(uLastLFOSampleNoWrap-uModTapNominalPos, uAllocatedDelayLength-1 - uWritePos);
 				uFramesToProcess = AkMin(uFramesToProcess, in_uNumFrames);
			}

			uModTapNominalPos = (uWritePos + uNominalDelayPos) % uAllocatedDelayLength;

			//Now process a part without wrapping, if possible
			uFramesToProcess = AkMin(uLastLFOSampleNoWrap-uModTapNominalPos, uAllocatedDelayLength-1 - uWritePos);
			uFramesToProcess = AkMin(uFramesToProcess, in_uNumFrames);
			AkUInt32 u8xIterations = uFramesToProcess/NUM_ITERATIONS;
			in_uNumFrames -= u8xIterations * NUM_ITERATIONS;
			while(u8xIterations)
			{
				PROCESS_ITERATION;
				PROCESS_ITERATION;
				PROCESS_ITERATION;
				PROCESS_ITERATION;
				PROCESS_ITERATION;
				PROCESS_ITERATION;
				PROCESS_ITERATION;
				PROCESS_ITERATION;
				u8xIterations--;
			}
		}
	
		m_uWritePos = uWritePos;

		InterpolationRampsDone();
	}
}
