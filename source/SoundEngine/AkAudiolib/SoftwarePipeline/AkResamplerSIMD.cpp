/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

//////////////////////////////////////////////////////////////////////
//
// AkResamplerSIMD.cpp
// 
// SSE, SSE2, and NEON-specific implementations of resampler functions
//
/////////////////////////////////////////////////////////////////////

#include "stdafx.h" 
#include "AkResamplerCommon.h"
#include <AK/SoundEngine/Common/AkSimd.h>
#include <AK/Tools/Common/AkPlatformFuncs.h> 

/********************* BYPASS DSP ROUTINES **********************/

// Bypass (no pitch or resampling) with signed 16-bit samples vectorized for 1 channels.
// As opposed to other routines, this only does the format conversion and deinterleaving is performed once 
// whole buffer is ready to be sent downstream to the pipeline
#if defined( AK_CPU_X86 )
AKRESULT Bypass_I16_1ChanSSE(	AkAudioBuffer * io_pInBuffer,
								AkAudioBuffer * io_pOutBuffer,
								AkUInt32 uRequestedSize,
								AkInternalPitchState * io_pPitchState)
{
	PITCH_BYPASS_DSP_SETUP();

	AkInt16 * AK_RESTRICT pIn = (AkInt16 * AK_RESTRICT)(io_pInBuffer->GetInterleavedData()) + io_pPitchState->uInFrameOffset;
	AkReal32 * AK_RESTRICT pOut = (AkReal32 * AK_RESTRICT)(io_pOutBuffer->GetChannel(0)) + io_pPitchState->uOutFrameOffset;

	// Note: No input data constraint (MMX)
	// Note: Cannot assume output data alignment is on 16 byte boundaries
	const AkUInt32 uSamplesToCopy = uFramesToCopy;
	const AkUInt32 uNumIter = uSamplesToCopy / 16;
	AkUInt32 uRemaining = uSamplesToCopy - (uNumIter * 16);

	AKSIMD_V2F32 * AK_RESTRICT pm64In = (AKSIMD_V2F32 * AK_RESTRICT) pIn;
	AKSIMD_V4F32 * AK_RESTRICT pm128Out = (AKSIMD_V4F32 * AK_RESTRICT) pOut;
	const AKSIMD_V2F32 * AK_RESTRICT pm64InEnd = pm64In + 4 * uNumIter;

	const AKSIMD_V4F32 m128Scale = AKSIMD_SET_V4F32(NORMALIZEFACTORI16);
	const AKSIMD_V2F32 m64Zero = AKSIMD_SETZERO_V2F32();
	const AKSIMD_V4F32 m128Zero = AKSIMD_SETZERO_V4F32();

	// Process blocks of 16 frames
	while (pm64In < pm64InEnd)
	{
		AKSIMD_V2F32 m64InSamples1 = pm64In[0];
		AKSIMD_V2F32 m64InSamples2 = pm64In[1];
		AKSIMD_V2F32 m64InSamples3 = pm64In[2];
		AKSIMD_V2F32 m64InSamples4 = pm64In[3];
		pm64In += 4;

		AKSIMD_V2F32 m64Sign = AKSIMD_CMPGT_V2I32(m64Zero, m64InSamples1);									// Retrieve sign for proper sign extension	
		AKSIMD_V4F32 m128Tmp = _mm_cvtpi32_ps(m128Zero, AKSIMD_UNPACKHI_VECTOR4I16(m64InSamples1, m64Sign));	// Interleave to 32 bits and convert (hi)
		AKSIMD_V4F32 m128Out1 = _mm_cvtpi32_ps(AKSIMD_MOVELH_V4F32(m128Tmp, m128Tmp), AKSIMD_UNPACKLO_VECTOR4I16(m64InSamples1, m64Sign)); // Interleave to 32 bits and convert (lo) and merge with previous result by shifting up high part

		m64Sign = AKSIMD_CMPGT_V2I32(m64Zero, m64InSamples2);
		m128Tmp = _mm_cvtpi32_ps(m128Zero, AKSIMD_UNPACKHI_VECTOR4I16(m64InSamples2, m64Sign));
		AKSIMD_V4F32 m128Out2 = _mm_cvtpi32_ps(AKSIMD_MOVELH_V4F32(m128Tmp, m128Tmp), AKSIMD_UNPACKLO_VECTOR4I16(m64InSamples2, m64Sign));

		m64Sign = AKSIMD_CMPGT_V2I32(m64Zero, m64InSamples3);
		m128Tmp = _mm_cvtpi32_ps(m128Zero, AKSIMD_UNPACKHI_VECTOR4I16(m64InSamples3, m64Sign));
		AKSIMD_V4F32 m128Out3 = _mm_cvtpi32_ps(AKSIMD_MOVELH_V4F32(m128Tmp, m128Tmp), AKSIMD_UNPACKLO_VECTOR4I16(m64InSamples3, m64Sign));

		m64Sign = AKSIMD_CMPGT_V2I32(m64Zero, m64InSamples4);
		m128Tmp = _mm_cvtpi32_ps(m128Zero, AKSIMD_UNPACKHI_VECTOR4I16(m64InSamples4, m64Sign));
		AKSIMD_V4F32 m128Out4 = _mm_cvtpi32_ps(AKSIMD_MOVELH_V4F32(m128Tmp, m128Tmp), AKSIMD_UNPACKLO_VECTOR4I16(m64InSamples4, m64Sign));

		AKSIMD_STOREU_V4F32((AkReal32 *)&pm128Out[0], AKSIMD_MUL_V4F32(m128Out1, m128Scale));
		AKSIMD_STOREU_V4F32((AkReal32 *)&pm128Out[1], AKSIMD_MUL_V4F32(m128Out2, m128Scale));
		AKSIMD_STOREU_V4F32((AkReal32 *)&pm128Out[2], AKSIMD_MUL_V4F32(m128Out3, m128Scale));
		AKSIMD_STOREU_V4F32((AkReal32 *)&pm128Out[3], AKSIMD_MUL_V4F32(m128Out4, m128Scale));
		pm128Out += 4;
	}

	AKSIMD_MMX_EMPTY;

	// Advance data pointers for remaining samples
	pIn = (AkInt16 * AK_RESTRICT) pm64In;
	pOut = (AkReal32 * AK_RESTRICT) pm128Out;

	// Deal with remaining samples
	while (uRemaining--)
	{
		*pOut++ = INT16_TO_FLOAT(*pIn++);
	}

	// Need to keep the last buffer value in case we start the pitch algo next buffer for 1 channel
	pIn -= 1;
	io_pPitchState->iLastValue[0] = pIn[0];

	PITCH_BYPASS_DSP_TEARDOWN();
}
#endif
#if defined ( AKSIMD_V4F32_SUPPORTED )
AKRESULT Bypass_I16_1ChanV4F32(	AkAudioBuffer * io_pInBuffer,
								AkAudioBuffer * io_pOutBuffer,
								AkUInt32 uRequestedSize,
								AkInternalPitchState * io_pPitchState)
{
	PITCH_BYPASS_DSP_SETUP();

	AkInt16 * AK_RESTRICT pIn = (AkInt16 * AK_RESTRICT)(io_pInBuffer->GetInterleavedData()) + io_pPitchState->uInFrameOffset;
	AkReal32 * AK_RESTRICT pOut = (AkReal32 * AK_RESTRICT)(io_pOutBuffer->GetChannel(0)) + io_pPitchState->uOutFrameOffset;
	// Note: No input data constraint (MMX)
	// Note: Cannot assume output data alignment is on 16 byte boundaries
	const int FramesPerUnroll = 8;
	const int UnrollsPerIter = 2;
	const AkUInt32 uNumIter = uFramesToCopy / (FramesPerUnroll * UnrollsPerIter);
	AkUInt32 uRemaining = uFramesToCopy - (uNumIter * (FramesPerUnroll * UnrollsPerIter));

	const AkInt16 * AK_RESTRICT pInEnd = pIn + (FramesPerUnroll * UnrollsPerIter)*uNumIter;
	const AKSIMD_V4F32 m128Scale = AKSIMD_SET_V4F32(NORMALIZEFACTORI16);
	const AKSIMD_V4I32 m128iZero = AKSIMD_SETZERO_V4I32();

	// Process blocks of 16 frames
	while (pIn < pInEnd)
	{
		AKSIMD_V4I32 m128InSamples1 = AKSIMD_LOADU_V4I32((AKSIMD_V4I32 AK_UNALIGNED *) (pIn + 0));
		AKSIMD_V4I32 m128InSamples2 = AKSIMD_LOADU_V4I32((AKSIMD_V4I32 AK_UNALIGNED *) (pIn + FramesPerUnroll));

		pIn += (FramesPerUnroll * UnrollsPerIter);

		AKSIMD_V4I32 m128Sign = AKSIMD_CMPGT_V8I16(m128iZero, m128InSamples1);					// Retrieve sign for proper sign extension	
		AKSIMD_V4I32 m128InSamplesLo = AKSIMD_UNPACKLO_VECTOR8I16(m128InSamples1, m128Sign);	// Interleave (lo) sign and int16 to int 32 bits
		AKSIMD_V4I32 m128InSamplesHi = AKSIMD_UNPACKHI_VECTOR8I16(m128InSamples1, m128Sign);	// Interleave (hi) sign and int16 to int 32 bits
		AKSIMD_V4F32 m128out1 = AKSIMD_CONVERT_V4I32_TO_V4F32(m128InSamplesLo);					// Convert int 32 bits to floats
		AKSIMD_V4F32 m128out2 = AKSIMD_CONVERT_V4I32_TO_V4F32(m128InSamplesHi);					// Convert int 32 bits to floats

		m128Sign = AKSIMD_CMPGT_V8I16(m128iZero, m128InSamples2);								// Retrieve sign for proper sign extension	
		m128InSamplesLo = AKSIMD_UNPACKLO_VECTOR8I16(m128InSamples2, m128Sign);					// Interleave (lo) sign and int16 to int 32 bits
		m128InSamplesHi = AKSIMD_UNPACKHI_VECTOR8I16(m128InSamples2, m128Sign);					// Interleave (hi) sign and int16 to int 32 bits
		AKSIMD_V4F32 m128out3 = AKSIMD_CONVERT_V4I32_TO_V4F32(m128InSamplesLo);					// Convert int 32 bits to floats
		AKSIMD_V4F32 m128out4 = AKSIMD_CONVERT_V4I32_TO_V4F32(m128InSamplesHi);					// Convert int 32 bits to floats

		AKSIMD_STOREU_V4F32(pOut + FramesPerUnroll / 2 * 0, AKSIMD_MUL_V4F32(m128out1, m128Scale));
		AKSIMD_STOREU_V4F32(pOut + FramesPerUnroll / 2 * 1, AKSIMD_MUL_V4F32(m128out2, m128Scale));
		AKSIMD_STOREU_V4F32(pOut + FramesPerUnroll / 2 * 2, AKSIMD_MUL_V4F32(m128out3, m128Scale));
		AKSIMD_STOREU_V4F32(pOut + FramesPerUnroll / 2 * 3, AKSIMD_MUL_V4F32(m128out4, m128Scale));

		pOut += (FramesPerUnroll * UnrollsPerIter);
	}

	// Deal with remaining samples
	while (uRemaining--)
	{
		*pOut++ = INT16_TO_FLOAT(*pIn++);
	}

	// Need to keep the last buffer value in case we start the pitch algo next buffer for each channel
	pIn -= 1;
	io_pPitchState->iLastValue[0] = pIn[0];

	PITCH_BYPASS_DSP_TEARDOWN();
}
#endif

// Bypass (no pitch or resampling) with INTERLEAVED signed 16-bit samples optimized for 2 channel signals. 
#if defined( AK_CPU_X86 )
AKRESULT Bypass_I16_2ChanSSE(	AkAudioBuffer * io_pInBuffer, 
								AkAudioBuffer * io_pOutBuffer,
								AkUInt32 uRequestedSize,
								AkInternalPitchState * io_pPitchState )
{
	PITCH_BYPASS_DSP_SETUP( );

	AkInt32 uLastSample = static_cast<AkInt32>(uFramesToCopy - 1);

	// IMPORTANT: Second channel access relies on the fact that at least the 
	// first 2 channels of AkAudioBuffer are contiguous in memory.
	AkInt16 * AK_RESTRICT pIn = (AkInt16 * AK_RESTRICT)( io_pInBuffer->GetInterleavedData() ) + 2*io_pPitchState->uInFrameOffset;
	AkReal32 * AK_RESTRICT pOut = (AkReal32 * AK_RESTRICT)( io_pOutBuffer->GetChannel( 0 ) ) + io_pPitchState->uOutFrameOffset;

	// Need to keep the last buffer value in case we start the pitch algo next buffer
	io_pPitchState->iLastValue[0] = pIn[2*uLastSample];
	io_pPitchState->iLastValue[1] = pIn[2*uLastSample+1];

	AkUInt32 uNumIter = uFramesToCopy / 16;
	AkUInt32 uRemaining = uFramesToCopy - (uNumIter*16);

	AKSIMD_V2F32 * AK_RESTRICT pm64In = (AKSIMD_V2F32* AK_RESTRICT) pIn;
	AKSIMD_V4F32 * AK_RESTRICT pm128Out = (AKSIMD_V4F32* AK_RESTRICT) pOut;

	const AKSIMD_V2F32 * AK_RESTRICT pm64InEnd = pm64In + 8*uNumIter;
	const AKSIMD_V4F32 m128Scale = AKSIMD_SET_V4F32( NORMALIZEFACTORI16 );
	const AKSIMD_V4F32 m128Zero = AKSIMD_SETZERO_V4F32();

	AkUInt32 uMaxFrames = io_pOutBuffer->MaxFrames()/4;

	// Process blocks first
	while ( pm64In < pm64InEnd )
	{
		AKSIMD_V2F32 m64InSamples1 = pm64In[0];	
		AKSIMD_V2F32 m64InSamples2 = pm64In[1];
		AKSIMD_V2F32 m64InSamplesLLow = AKSIMD_SHIFTLEFT_V2I32( m64InSamples1, 16 );  // Prepare left samples for conversion
		AKSIMD_V2F32 m64InSamplesRLow = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamples1, 16 );  // Prepare right samples for conversion
		m64InSamplesLLow = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamplesLLow, 16 ); 		
		AKSIMD_V2F32 m64InSamplesLHi = AKSIMD_SHIFTLEFT_V2I32( m64InSamples2, 16 );  // Prepare left samples for conversion
		AKSIMD_V2F32 m64InSamplesRHi = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamples2, 16 );  // Prepare right samples for conversion
		m64InSamplesLHi = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamplesLHi, 16 ); 
		AKSIMD_V4F32 m128OutL = _mm_cvtpi32_ps(m128Zero, m64InSamplesLHi );
		AKSIMD_V4F32 m128OutR = _mm_cvtpi32_ps(m128Zero, m64InSamplesRHi );
		m128OutL = _mm_cvtpi32_ps(AKSIMD_MOVELH_V4F32( m128OutL, m128OutL ), m64InSamplesLLow );
		m128OutR = _mm_cvtpi32_ps(AKSIMD_MOVELH_V4F32( m128OutR, m128OutR ), m64InSamplesRLow );
		AKSIMD_STOREU_V4F32( (AkReal32 *)pm128Out, AKSIMD_MUL_V4F32( m128OutL, m128Scale ) ); 
		AKSIMD_STOREU_V4F32( (AkReal32 *)&pm128Out[uMaxFrames], AKSIMD_MUL_V4F32( m128OutR, m128Scale ) ); 

		m64InSamples1 = pm64In[2];	
		m64InSamples2 = pm64In[3];
		m64InSamplesLLow = AKSIMD_SHIFTLEFT_V2I32( m64InSamples1, 16 );  // Prepare left samples for conversion
		m64InSamplesRLow = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamples1, 16 );  // Prepare right samples for conversion
		m64InSamplesLLow = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamplesLLow, 16 ); 		
		m64InSamplesLHi = AKSIMD_SHIFTLEFT_V2I32( m64InSamples2, 16 );  // Prepare left samples for conversion
		m64InSamplesRHi = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamples2, 16 );  // Prepare right samples for conversion
		m64InSamplesLHi = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamplesLHi, 16 ); 
		m128OutL = _mm_cvtpi32_ps(m128Zero, m64InSamplesLHi );
		m128OutR = _mm_cvtpi32_ps(m128Zero, m64InSamplesRHi );
		m128OutL = _mm_cvtpi32_ps(AKSIMD_MOVELH_V4F32( m128OutL, m128OutL ), m64InSamplesLLow );
		m128OutR = _mm_cvtpi32_ps(AKSIMD_MOVELH_V4F32( m128OutR, m128OutR ), m64InSamplesRLow );
		AKSIMD_STOREU_V4F32( (AkReal32 *)&pm128Out[1], AKSIMD_MUL_V4F32( m128OutL, m128Scale ) ); 
		AKSIMD_STOREU_V4F32( (AkReal32 *)&pm128Out[1+uMaxFrames], AKSIMD_MUL_V4F32( m128OutR, m128Scale ) ); 

		m64InSamples1 = pm64In[4];	
		m64InSamples2 = pm64In[5];
		m64InSamplesLLow = AKSIMD_SHIFTLEFT_V2I32( m64InSamples1, 16 );  // Prepare left samples for conversion
		m64InSamplesRLow = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamples1, 16 );  // Prepare right samples for conversion
		m64InSamplesLLow = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamplesLLow, 16 ); 		
		m64InSamplesLHi = AKSIMD_SHIFTLEFT_V2I32( m64InSamples2, 16 );  // Prepare left samples for conversion
		m64InSamplesRHi = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamples2, 16 );  // Prepare right samples for conversion
		m64InSamplesLHi = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamplesLHi, 16 ); 
		m128OutL = _mm_cvtpi32_ps(m128Zero, m64InSamplesLHi );
		m128OutR = _mm_cvtpi32_ps(m128Zero, m64InSamplesRHi );
		m128OutL = _mm_cvtpi32_ps(AKSIMD_MOVELH_V4F32( m128OutL, m128OutL ), m64InSamplesLLow );
		m128OutR = _mm_cvtpi32_ps(AKSIMD_MOVELH_V4F32( m128OutR, m128OutR ), m64InSamplesRLow );
		AKSIMD_STOREU_V4F32( (AkReal32 *)&pm128Out[2], AKSIMD_MUL_V4F32( m128OutL, m128Scale ) ); 
		AKSIMD_STOREU_V4F32( (AkReal32 *)&pm128Out[2+uMaxFrames], AKSIMD_MUL_V4F32( m128OutR, m128Scale ) ); 

		m64InSamples1 = pm64In[6];	
		m64InSamples2 = pm64In[7];
		m64InSamplesLLow = AKSIMD_SHIFTLEFT_V2I32( m64InSamples1, 16 );  // Prepare left samples for conversion
		m64InSamplesRLow = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamples1, 16 );  // Prepare right samples for conversion
		m64InSamplesLLow = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamplesLLow, 16 ); 		
		m64InSamplesLHi = AKSIMD_SHIFTLEFT_V2I32( m64InSamples2, 16 );  // Prepare left samples for conversion
		m64InSamplesRHi = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamples2, 16 );  // Prepare right samples for conversion
		m64InSamplesLHi = AKSIMD_SHIFTRIGHTARITH_V2I32( m64InSamplesLHi, 16 ); 
		m128OutL = _mm_cvtpi32_ps(m128Zero, m64InSamplesLHi );
		m128OutR = _mm_cvtpi32_ps(m128Zero, m64InSamplesRHi );
		m128OutL = _mm_cvtpi32_ps(AKSIMD_MOVELH_V4F32( m128OutL, m128OutL ), m64InSamplesLLow );
		m128OutR = _mm_cvtpi32_ps(AKSIMD_MOVELH_V4F32( m128OutR, m128OutR ), m64InSamplesRLow );
		AKSIMD_STOREU_V4F32( (AkReal32 *)&pm128Out[3], AKSIMD_MUL_V4F32( m128OutL, m128Scale ) ); 
		AKSIMD_STOREU_V4F32( (AkReal32 *)&pm128Out[3+uMaxFrames], AKSIMD_MUL_V4F32( m128OutR, m128Scale ) ); 

		pm64In += 8;
		pm128Out += 4;
	}
	AKSIMD_MMX_EMPTY;

	pIn = (AkInt16 * AK_RESTRICT) pm64In;
	pOut = (AkReal32* AK_RESTRICT) pm128Out;

	// Process blocks of 4 first
	uMaxFrames = io_pOutBuffer->MaxFrames();
	while ( uRemaining-- )
	{	
		*pOut = INT16_TO_FLOAT( *pIn++ );
		pOut[uMaxFrames] = INT16_TO_FLOAT( *pIn++ );
		++pOut;
	}

	PITCH_BYPASS_DSP_TEARDOWN( );
}
#endif

#if defined ( AKSIMD_V4F32_SUPPORTED )
// Bypass (no pitch or resampling) with INTERLEAVED signed 16-bit samples optimized for 2 channel signals. 
AKRESULT Bypass_I16_2ChanV4F32(	AkAudioBuffer * io_pInBuffer, 
								AkAudioBuffer * io_pOutBuffer,
								AkUInt32 uRequestedSize,
								AkInternalPitchState * io_pPitchState )
{
	
	PITCH_BYPASS_DSP_SETUP( );

	AkInt32 uLastSample = static_cast<AkInt32>(uFramesToCopy - 1);

	// IMPORTANT: Second channel access relies on the fact that at least the 
	// first 2 channels of AkAudioBuffer are contiguous in memory.
	AkInt16 * AK_RESTRICT pIn = (AkInt16 * AK_RESTRICT)( io_pInBuffer->GetInterleavedData() ) + 2*io_pPitchState->uInFrameOffset;
	AkReal32 * AK_RESTRICT pOut = (AkReal32 * AK_RESTRICT)( io_pOutBuffer->GetChannel( 0 ) ) + io_pPitchState->uOutFrameOffset;

	// Need to keep the last buffer value in case we start the pitch algo next buffer
	io_pPitchState->iLastValue[0] = pIn[2*uLastSample];
	io_pPitchState->iLastValue[1] = pIn[2*uLastSample+1];

	const int SamplesPerUnroll = 4;
	const int UnrollCountPerIter = 4;

	AkUInt32 uNumIter = uFramesToCopy / (SamplesPerUnroll * UnrollCountPerIter);
	AkUInt32 uRemaining = uFramesToCopy - (uNumIter* (SamplesPerUnroll * UnrollCountPerIter));

	const AkInt16 * AK_RESTRICT pInEnd = pIn + 2 * SamplesPerUnroll * UnrollCountPerIter * uNumIter;
	const AKSIMD_V4F32 m128Scale = AKSIMD_SET_V4F32( NORMALIZEFACTORI16 );
	
	AkUInt32 uMaxFrames = io_pOutBuffer->MaxFrames();

	// Process blocks first
	while ( pIn < pInEnd )
	{
		AKSIMD_V4I32 m128InSamples = AKSIMD_LOADU_V4I32( (AKSIMD_V4I32 AK_UNALIGNED *) ( pIn + 0 ) );
		AKSIMD_V4I32 m128InSamplesL = AKSIMD_SHIFTRIGHTARITH_V4I32(AKSIMD_SHIFTLEFT_V4I32( m128InSamples, 16), 16);	// Prepare left samples for conversion
		AKSIMD_V4I32 m128InSamplesR = AKSIMD_SHIFTRIGHTARITH_V4I32( m128InSamples, 16);						// Prepare right samples for conversion
		AKSIMD_V4F32 m128OutL = AKSIMD_CONVERT_V4I32_TO_V4F32(m128InSamplesL);									// Convert Int32 to floats
		AKSIMD_V4F32 m128OutR = AKSIMD_CONVERT_V4I32_TO_V4F32(m128InSamplesR);									// Convert Int32 to floats
		AKSIMD_STOREU_V4F32( pOut + 0, AKSIMD_MUL_V4F32( m128OutL, m128Scale ) ); 
		AKSIMD_STOREU_V4F32( pOut + 0 + uMaxFrames, AKSIMD_MUL_V4F32( m128OutR, m128Scale ) ); 

		m128InSamples = AKSIMD_LOADU_V4I32( (AKSIMD_V4I32 AK_UNALIGNED *) ( pIn + SamplesPerUnroll * 2 ) );
		m128InSamplesL = AKSIMD_SHIFTRIGHTARITH_V4I32(AKSIMD_SHIFTLEFT_V4I32( m128InSamples, 16), 16);			// Prepare left samples for conversion
		m128InSamplesR = AKSIMD_SHIFTRIGHTARITH_V4I32( m128InSamples, 16);								// Prepare right samples for conversion
		m128OutL = AKSIMD_CONVERT_V4I32_TO_V4F32(m128InSamplesL);											// Convert Int32 to floats
		m128OutR = AKSIMD_CONVERT_V4I32_TO_V4F32(m128InSamplesR);											// Convert Int32 to floats
		AKSIMD_STOREU_V4F32( pOut + SamplesPerUnroll, AKSIMD_MUL_V4F32( m128OutL, m128Scale ) );
		AKSIMD_STOREU_V4F32( pOut + SamplesPerUnroll + uMaxFrames, AKSIMD_MUL_V4F32( m128OutR, m128Scale ) );

		m128InSamples = AKSIMD_LOADU_V4I32( (AKSIMD_V4I32 AK_UNALIGNED *) ( pIn + SamplesPerUnroll * 4) );
		m128InSamplesL = AKSIMD_SHIFTRIGHTARITH_V4I32(AKSIMD_SHIFTLEFT_V4I32( m128InSamples, 16), 16);			// Prepare left samples for conversion
		m128InSamplesR = AKSIMD_SHIFTRIGHTARITH_V4I32( m128InSamples, 16);								// Prepare right samples for conversion
		m128OutL = AKSIMD_CONVERT_V4I32_TO_V4F32(m128InSamplesL);											// Convert Int32 to floats
		m128OutR = AKSIMD_CONVERT_V4I32_TO_V4F32(m128InSamplesR);											// Convert Int32 to floats
		AKSIMD_STOREU_V4F32( pOut + SamplesPerUnroll * 2 , AKSIMD_MUL_V4F32( m128OutL, m128Scale ) );
		AKSIMD_STOREU_V4F32( pOut + SamplesPerUnroll * 2 + uMaxFrames, AKSIMD_MUL_V4F32( m128OutR, m128Scale ) );

		m128InSamples = AKSIMD_LOADU_V4I32( (AKSIMD_V4I32 AK_UNALIGNED *) ( pIn + SamplesPerUnroll * 6) );
		m128InSamplesL = AKSIMD_SHIFTRIGHTARITH_V4I32(AKSIMD_SHIFTLEFT_V4I32( m128InSamples, 16), 16);			// Prepare left samples for conversion
		m128InSamplesR = AKSIMD_SHIFTRIGHTARITH_V4I32( m128InSamples, 16);								// Prepare right samples for conversion
		m128OutL = AKSIMD_CONVERT_V4I32_TO_V4F32(m128InSamplesL);											// Convert Int32 to floats
		m128OutR = AKSIMD_CONVERT_V4I32_TO_V4F32(m128InSamplesR);											// Convert Int32 to floats
		AKSIMD_STOREU_V4F32( pOut + SamplesPerUnroll * 3, AKSIMD_MUL_V4F32( m128OutL, m128Scale ) );
		AKSIMD_STOREU_V4F32( pOut + SamplesPerUnroll * 3 + uMaxFrames, AKSIMD_MUL_V4F32( m128OutR, m128Scale ) );

		pIn += 2 * SamplesPerUnroll * UnrollCountPerIter;
		pOut += SamplesPerUnroll * UnrollCountPerIter;
	}

	// Process blocks of 4 first
	while ( uRemaining-- )
	{	
		*pOut = INT16_TO_FLOAT( *pIn++ );
		pOut[uMaxFrames] = INT16_TO_FLOAT( *pIn++ );
		++pOut;
	}

	PITCH_BYPASS_DSP_TEARDOWN( );
}
#endif

// Bypass (no pitch or resampling) with INTERLEAVED signed 16-bit samples vectorized for any number of channels.
#if defined ( AK_CPU_X86 )
AKRESULT Bypass_I16_NChanSSE(	AkAudioBuffer * io_pInBuffer, 
								AkAudioBuffer * io_pOutBuffer,
								AkUInt32 uRequestedSize,
								AkInternalPitchState * io_pPitchState )
{
	PITCH_BYPASS_DSP_SETUP( );

	const AkUInt32 uNumChannels = io_pInBuffer->NumChannels();
	AkInt16 * AK_RESTRICT pIn = (AkInt16 * AK_RESTRICT)( io_pInBuffer->GetInterleavedData( ) ) + io_pPitchState->uInFrameOffset*uNumChannels;

	// Load up all of the output buffers we need (we handle pairs of outputs in the main body, so round up the number of channels)
	AkUInt32 uNumChannelsRounded = (uNumChannels + 1) & ~1;
	AkReal32** AK_RESTRICT ppOut = (AkReal32**)alloca(sizeof(AkReal32*) * uNumChannelsRounded);
	for (AkUInt32 i = 0; i < uNumChannels; ++i)
	{
		ppOut[i] = (AkReal32 * AK_RESTRICT) io_pOutBuffer->GetChannel(AkChannelRemap(i, io_pPitchState)) + io_pPitchState->uOutFrameOffset;
	}

	// if we have an odd number of channels, make sure our fake output matches the last real one
	if (uNumChannels != uNumChannelsRounded)
	{
		ppOut[uNumChannels] = ppOut[uNumChannels - 1];
	}

	// Determine number of iterations remaining
	// Note: No input data constraint (MMX)
	// Note: Cannot assume output data alignment is on 16 byte boundaries
	const int FramesPerIter = 4;
	AkUInt32 uNumIter = uFramesToCopy / (FramesPerIter);
	AkUInt32 uNumRemainderFrames = uFramesToCopy % FramesPerIter;

	const AKSIMD_V4F32 vfScaleInt16 = AKSIMD_SET_V4F32( NORMALIZEFACTORI16 );
	const AKSIMD_V2F32 m64Zero = AKSIMD_SETZERO_V2F32();
	const AKSIMD_V4F32 m128Zero = AKSIMD_SETZERO_V4F32();

	// Process blocks of 4 frames
	while ( uNumIter-- )
	{
		for (AkUInt32 uChanIter = 0; uChanIter < uNumChannelsRounded; uChanIter += 2)
		{
			AkInt16 * AK_RESTRICT pInRead = pIn + uChanIter;
			AKSIMD_V2F32 iSamples[4] = {
				*(AKSIMD_V2F32*)(pInRead + 0 * uNumChannels),
				*(AKSIMD_V2F32*)(pInRead + 1 * uNumChannels),
				*(AKSIMD_V2F32*)(pInRead + 2 * uNumChannels),
				*(AKSIMD_V2F32*)(pInRead + 3 * uNumChannels)
			};

			// Load and prepare the last part of hte output first
			// (Shuffle so that lower 32b are L, higher 32b are R)
			AKSIMD_V2F32 viSamples16 = _m_punpcklwd(iSamples[2], iSamples[3]);

			// Retrieve sign for proper sign extension
			AKSIMD_V2F32 viSign = AKSIMD_CMPGT_V2I32(m64Zero, viSamples16);

			// Interleave to 32 bits and convert (hi)
			AKSIMD_V4F32 vfSamplesHi[2] = {
				_mm_cvtpi32_ps(m128Zero, AKSIMD_UNPACKLO_VECTOR4I16(viSamples16, viSign)),
				_mm_cvtpi32_ps(m128Zero, AKSIMD_UNPACKHI_VECTOR4I16(viSamples16, viSign))
			};

			// Repeat for the first part of the output
			viSamples16 = _m_punpcklwd(iSamples[0], iSamples[1]);
			viSign = AKSIMD_CMPGT_V2I32(m64Zero, viSamples16);

			// Interleave to 32 bits and convert (lo) and merge with previous result by shifting up high part
			AKSIMD_V4F32 vfSamples[2] = {
				_mm_cvtpi32_ps(AKSIMD_MOVELH_V4F32(vfSamplesHi[0], vfSamplesHi[0]), AKSIMD_UNPACKLO_VECTOR4I16(viSamples16, viSign)),
				_mm_cvtpi32_ps(AKSIMD_MOVELH_V4F32(vfSamplesHi[1], vfSamplesHi[1]), AKSIMD_UNPACKHI_VECTOR4I16(viSamples16, viSign))
			};

			// Note that we write to the 1th result first - this is because 
			// if we have an odd number of channels, then the 1th vfResult is 
			// full of irrelevant data, and we want to toss it. We do so
			// by unconditionally writing to the 1th buffer first - which should
			// be the same location as the 0th buffer - and then we clobber 
			// that write with the 0th result
			AKSIMD_STOREU_V4F32(ppOut[uChanIter + 1], AKSIMD_MUL_V4F32(vfSamples[1], vfScaleInt16));
			ppOut[uChanIter + 1] += FramesPerIter;
			AKSIMD_STOREU_V4F32(ppOut[uChanIter + 0], AKSIMD_MUL_V4F32(vfSamples[0], vfScaleInt16));
			ppOut[uChanIter + 0] += FramesPerIter;
		}
		pIn += uNumChannels * FramesPerIter;
	}

	AKSIMD_MMX_EMPTY;

	// Deal with remaining samples
	while (uNumRemainderFrames--)
	{
		for (AkUInt32 i = 0; i < uNumChannels; ++i)
		{
			(*ppOut[i]++) = INT16_TO_FLOAT(*pIn++);
		}
	}


	// Need to keep the last buffer value in case we start the pitch algo next buffer for each channel
	pIn -= uNumChannels;
	for ( AkUInt32 i = 0; i < uNumChannels; ++i )
	{
		io_pPitchState->iLastValue[i] = pIn[i];
	}

	PITCH_BYPASS_DSP_TEARDOWN( );
}
#endif
#if defined ( AKSIMD_V4F32_SUPPORTED )
AKRESULT Bypass_I16_NChanV4F32(AkAudioBuffer * io_pInBuffer,
								AkAudioBuffer * io_pOutBuffer,
								AkUInt32 uRequestedSize,
								AkInternalPitchState * io_pPitchState )
{
	PITCH_BYPASS_DSP_SETUP();

	const AkUInt32 uNumChannels = io_pInBuffer->NumChannels();
	AkInt16 * AK_RESTRICT pIn = (AkInt16 * AK_RESTRICT)(io_pInBuffer->GetInterleavedData()) + io_pPitchState->uInFrameOffset*uNumChannels;

	// Load up all of the output buffers we need (we handle pairs of outputs in the main body, so round up the number of channels)
	AkUInt32 uNumChannelsRounded = (uNumChannels + 1) & ~1;
	AkReal32** AK_RESTRICT ppOut = (AkReal32**)alloca(sizeof(AkReal32*) * uNumChannelsRounded);
	for (AkUInt32 i = 0; i < uNumChannels; ++i)
	{
		ppOut[i] = (AkReal32 * AK_RESTRICT) io_pOutBuffer->GetChannel(AkChannelRemap(i, io_pPitchState )) + io_pPitchState->uOutFrameOffset;
	}

	// if we have an odd number of channels, make sure our fake output matches the last real one
	if (uNumChannels != uNumChannelsRounded)
	{
		ppOut[uNumChannels] = ppOut[uNumChannels - 1];
	}

	// Determine number of iterations remaining
	const int FramesPerIter = 4;
	AkUInt32 uNumIter = uFramesToCopy / (FramesPerIter);
	AkUInt32 uNumRemainderFrames = uFramesToCopy % FramesPerIter;

	const AKSIMD_V4F32 vfScaleInt16 = AKSIMD_SET_V4F32(NORMALIZEFACTORI16);

	// Main body for processing frames
	while (uNumIter--)
	{
		for (AkUInt32 uChanIter = 0; uChanIter < uNumChannelsRounded; uChanIter += 2)
		{
			AkInt16 * pInRead = pIn + uChanIter;

			// Do a gathered read of 8 2-channel samples (32b of data - uChanIter and uChanIter+1)
			// This way, we will operate and store a pair of vectors for all of the following calculations, one for each channel
			AKSIMD_V4I32 viSamples16 = AKSIMD_SETV_V4I32(
				*(AkInt32*)(pInRead + 3 * uNumChannels),
				*(AkInt32*)(pInRead + 2 * uNumChannels),
				*(AkInt32*)(pInRead + 1 * uNumChannels),
				*(AkInt32*)(pInRead + 0 * uNumChannels)
			);

			AKSIMD_V4I32 viSamples32[2] = {
				AKSIMD_SHIFTRIGHTARITH_V4I32(AKSIMD_SHIFTLEFT_V4I32(viSamples16, 16),16),
				AKSIMD_SHIFTRIGHTARITH_V4I32(viSamples16, 16)
			};
			AKSIMD_V4F32 vfSamples[2] = {
				AKSIMD_CONVERT_V4I32_TO_V4F32(viSamples32[0]),
				AKSIMD_CONVERT_V4I32_TO_V4F32(viSamples32[1])
			};

			// Note that we write to the 1th result first - this is because 
			// if we have an odd number of channels, then the 1th vfResult is 
			// full of irrelevant data, and we want to toss it. We do so
			// by unconditionally writing to the 1th buffer first - which should
			// be the same location as the 0th buffer - and then we clobber 
			// that write with the 0th result
			AKSIMD_STOREU_V4F32(ppOut[uChanIter + 1], AKSIMD_MUL_V4F32(vfSamples[1], vfScaleInt16));
			AKSIMD_STOREU_V4F32(ppOut[uChanIter + 0], AKSIMD_MUL_V4F32(vfSamples[0], vfScaleInt16));

			ppOut[uChanIter + 0] += FramesPerIter;
			ppOut[uChanIter + 1] += FramesPerIter;
		}
		pIn += uNumChannels * FramesPerIter;
	}

	// Deal with remaining samples
	while (uNumRemainderFrames--)
	{
		for (AkUInt32 i = 0; i < uNumChannels; ++i)
		{
			(*ppOut[i]++) = INT16_TO_FLOAT(*pIn++);
		}
	}

	// Need to keep the last buffer value in case we start the pitch algo next buffer for each channel
	pIn -= uNumChannels;
	for (AkUInt32 i = 0; i < uNumChannels; ++i)
	{
		io_pPitchState->iLastValue[i] = pIn[i];
	}

	PITCH_BYPASS_DSP_TEARDOWN();
}
#endif

///********************* FIXED RESAMPLING DSP ROUTINES **********************/

// Fixed resampling (no pitch changes) with INTERLEAVED signed 16-bit samples optimized for one channel signals.

#if defined(AKSIMD_V4F32_SUPPORTED)

AKRESULT Fixed_I16_1ChanV4F32(	AkAudioBuffer * io_pInBuffer, 
								AkAudioBuffer * io_pOutBuffer,
								AkUInt32 uRequestedSize,
								AkInternalPitchState * io_pPitchState )
{
	PITCH_FIXED_DSP_SETUP( );
	
	// Minus one to compensate for offset of 1 due to zero == previous
	AkInt16 * AK_RESTRICT pInBuf = (AkInt16 * AK_RESTRICT) io_pInBuffer->GetInterleavedData( ) + io_pPitchState->uInFrameOffset - 1; 

	// Retrieve output buffer information
	AkReal32 * AK_RESTRICT pfOutBuf = (AkReal32 * AK_RESTRICT) io_pOutBuffer->GetChannel( 0 ) + io_pPitchState->uOutFrameOffset;

	// Use stored value as left value, while right index is on the first sample
	AkInt16 iPreviousFrame = *io_pPitchState->iLastValue;
	AkUInt32 uIterFrames = uNumIterPreviousFrame;
 	while ( uIterFrames-- )
	{
		AkInt32 iSampleDiff = pInBuf[1] - iPreviousFrame;
		*pfOutBuf++ = LINEAR_INTERP_I16( iPreviousFrame, iSampleDiff );
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	// Determine number of iterations remaining
	AkUInt32 uPredNumIterFrames = ((uInBufferFrames << FPBITS) - uIndexFP + (uFrameSkipFP-1)) / uFrameSkipFP;
	AkUInt32 uNumIterThisFrame = AkMin( uOutBufferFrames-uNumIterPreviousFrame, uPredNumIterFrames );

	const int FramesPerIter = 4;
	uIterFrames = uNumIterThisFrame;
	AkUInt32 uNumIter = uIterFrames / (FramesPerIter);
	AkUInt32 uNumRemainderFrames = uIterFrames % FramesPerIter;

	const AKSIMD_V4F32 vfScaleFPMUL = AKSIMD_SET_V4F32(1.0f / (float)FPMUL);
	const AKSIMD_V4F32 vfScaleInt16 = AKSIMD_SET_V4F32(NORMALIZEFACTORI16);

	// Prep viIndexFp with fixed offsets in each index - then we can advance each value by each value in viFrameSkipFP
	AKSIMD_V4I32 viFrameSkipFP = AKSIMD_SET_V4I32(uFrameSkipFP * FramesPerIter);
	AKSIMD_V4I32 viIndexFp = AKSIMD_SET_V4I32(uIndexFP);
	AKSIMD_V4I32 viFrameSkipBase = AKSIMD_MULLO16_V4I32(AKSIMD_SET_V4I32(uFrameSkipFP), AKSIMD_SETV_V4I32(3, 2, 1, 0));
	viIndexFp = AKSIMD_ADD_V4I32(viIndexFp, viFrameSkipBase);

	while (uNumIter--)
	{
		// gather the frames - this function will gather pairs of 16-bit data as 32-bit,
		// and deinterleave, giving us viFrames.val[0] and val[1] - the current frame and the next one.
		AKSIMD_V4I32X2 viFrames = AKSIMD_GATHER_V4I32_AND_DEINTERLEAVE_V4I32X2(
			pInBuf + ((uIndexFP + uFrameSkipFP * 3) >> FPBITS),
			pInBuf + ((uIndexFP + uFrameSkipFP * 2) >> FPBITS),
			pInBuf + ((uIndexFP + uFrameSkipFP * 1) >> FPBITS),
			pInBuf + ((uIndexFP + uFrameSkipFP * 0) >> FPBITS)
			);

		// Calculate their delta and convert to float
		AKSIMD_V4I32 viDiff = AKSIMD_SUB_V4I32(viFrames.val[1], viFrames.val[0]);
		AKSIMD_V4F32 vfPrevFrames = AKSIMD_CONVERT_V4I32_TO_V4F32(viFrames.val[0]);
		AKSIMD_V4F32 vfDiff = AKSIMD_CONVERT_V4I32_TO_V4F32(viDiff);

		// load up the interp factor
		AKSIMD_V4I32 viInterpFactor = AKSIMD_AND_V4I32(viIndexFp, AKSIMD_SET_V4I32(FPMASK));
		AKSIMD_V4F32 vfInterpFactor = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(viInterpFactor), vfScaleFPMUL);
		viIndexFp = AKSIMD_ADD_V4I32(viIndexFp, viFrameSkipFP);

		// Interpolate between prev and next frames
		AKSIMD_V4F32 vfResult = AKSIMD_MADD_V4F32(vfDiff, vfInterpFactor, vfPrevFrames);

		// write out the result
		AKSIMD_STOREU_V4F32(pfOutBuf, AKSIMD_MUL_V4F32(vfResult, vfScaleInt16));

		pfOutBuf += FramesPerIter;
		uIndexFP += uFrameSkipFP * FramesPerIter;
	}

	// Finalize other values based on final uIndexFP
	uPreviousFrameIndex = uIndexFP >> FPBITS;
	uInterpLocFP = uIndexFP & FPMASK;

	while (uNumRemainderFrames-- )
	{
		iPreviousFrame = pInBuf[uPreviousFrameIndex];
		AkInt32 iSampleDiff = pInBuf[uPreviousFrameIndex+1] - iPreviousFrame;	
		*pfOutBuf++ = LINEAR_INTERP_I16( iPreviousFrame, iSampleDiff );
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	PITCH_SAVE_NEXT_I16_1CHAN();
	PITCH_FIXED_DSP_TEARDOWN( uIndexFP );
}

AKRESULT Fixed_I16_2ChanV4F32(	AkAudioBuffer * io_pInBuffer,
								AkAudioBuffer * io_pOutBuffer,
								AkUInt32 uRequestedSize,
								AkInternalPitchState * io_pPitchState )
{
	PITCH_FIXED_DSP_SETUP( );
	AkUInt32 uMaxFrames = io_pOutBuffer->MaxFrames();

	// Minus one to compensate for offset of 1 due to zero == previous
	AkInt16 * AK_RESTRICT pInBuf = (AkInt16 * AK_RESTRICT) io_pInBuffer->GetInterleavedData() + 2*io_pPitchState->uInFrameOffset - 2; 
	
	AkReal32 * AK_RESTRICT pfOutBuf = (AkReal32 * AK_RESTRICT) io_pOutBuffer->GetChannel( 0 ) + io_pPitchState->uOutFrameOffset;

	// Use stored value as left value, while right index is on the first sample
	AkInt16 iPreviousFrameL = io_pPitchState->iLastValue[0];
	AkInt16 iPreviousFrameR = io_pPitchState->iLastValue[1];
	AkUInt32 uIterFrames = uNumIterPreviousFrame;
	while ( uIterFrames-- )
	{
		AkInt32 iSampleDiffL = pInBuf[2] - iPreviousFrameL;
		AkInt32 iSampleDiffR = pInBuf[3] - iPreviousFrameR;
		*pfOutBuf = LINEAR_INTERP_I16( iPreviousFrameL, iSampleDiffL );
		pfOutBuf[uMaxFrames] = LINEAR_INTERP_I16( iPreviousFrameR, iSampleDiffR );
		++pfOutBuf;
		FP_INDEX_ADVANCE();	
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	// Determine number of iterations remaining
	AkUInt32 uPredNumIterFrames = ((uInBufferFrames << FPBITS) - uIndexFP + (uFrameSkipFP-1)) / uFrameSkipFP;
	AkUInt32 uNumIterThisFrame = AkMin( uOutBufferFrames-uNumIterPreviousFrame, uPredNumIterFrames );

	// For all other sample frames
	const int FramesPerIter = 4;
	uIterFrames = uNumIterThisFrame;
	AkUInt32 uNumIter = uIterFrames / (FramesPerIter);
	AkUInt32 uNumRemainderFrames = uIterFrames % FramesPerIter;

	const AKSIMD_V4F32 vfScaleFPMUL = AKSIMD_SET_V4F32(1.0f / (float)FPMUL);
	const AKSIMD_V4F32 vfScaleInt16 = AKSIMD_SET_V4F32(NORMALIZEFACTORI16);

	// Prep viIndexFp with fixed offsets in each index - then we can advance each value by each value in viFrameSkipFP
	AKSIMD_V4I32 viFrameSkipFP = AKSIMD_SET_V4I32(uFrameSkipFP * FramesPerIter);
	AKSIMD_V4I32 viIndexFp = AKSIMD_SET_V4I32(uIndexFP);
	AKSIMD_V4I32 viFrameSkipBase = AKSIMD_MULLO16_V4I32(AKSIMD_SET_V4I32(uFrameSkipFP), AKSIMD_SETV_V4I32(3, 2, 1, 0));
	viIndexFp = AKSIMD_ADD_V4I32(viIndexFp, viFrameSkipBase);

	while (uNumIter--)
	{
		// gather the frames. We do a 64-bit load against 16-bit data
		// so we load the prevL, then prevR, then nextL, then nextR frames in each load
		AKSIMD_V4I32X4 viFrames = AKSIMD_GATHER_V4I64_AND_DEINTERLEAVE_V4I32X4(
			pInBuf + ((uIndexFP + uFrameSkipFP * 3) >> FPBITS) * 2,
			pInBuf + ((uIndexFP + uFrameSkipFP * 2) >> FPBITS) * 2,
			pInBuf + ((uIndexFP + uFrameSkipFP * 1) >> FPBITS) * 2,
			pInBuf + ((uIndexFP + uFrameSkipFP * 0) >> FPBITS) * 2
		);

		AKSIMD_V4I32 viDiffL = AKSIMD_SUB_V4I32(viFrames.val[2], viFrames.val[0]);
		AKSIMD_V4I32 viDiffR = AKSIMD_SUB_V4I32(viFrames.val[3], viFrames.val[1]);
		AKSIMD_V4F32 vfDiffL = AKSIMD_CONVERT_V4I32_TO_V4F32(viDiffL);
		AKSIMD_V4F32 vfDiffR = AKSIMD_CONVERT_V4I32_TO_V4F32(viDiffR);

		AKSIMD_V4F32 vfPrevL = AKSIMD_CONVERT_V4I32_TO_V4F32(viFrames.val[0]);
		AKSIMD_V4F32 vfPrevR = AKSIMD_CONVERT_V4I32_TO_V4F32(viFrames.val[1]);

		// load up the interp factor
		AKSIMD_V4I32 viInterpFactor = AKSIMD_AND_V4I32(viIndexFp, AKSIMD_SET_V4I32(FPMASK));
		AKSIMD_V4F32 vfInterpFactor = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(viInterpFactor), vfScaleFPMUL);
		viIndexFp = AKSIMD_ADD_V4I32(viIndexFp, viFrameSkipFP);

		// Interpolate between prev and next frames
		AKSIMD_V4F32 vfResultL = AKSIMD_MADD_V4F32(vfDiffL, vfInterpFactor, vfPrevL);
		AKSIMD_V4F32 vfResultR = AKSIMD_MADD_V4F32(vfDiffR, vfInterpFactor, vfPrevR);

		// write out the result
		AKSIMD_STOREU_V4F32(pfOutBuf, AKSIMD_MUL_V4F32(vfResultL, vfScaleInt16));
		AKSIMD_STOREU_V4F32(pfOutBuf + uMaxFrames, AKSIMD_MUL_V4F32(vfResultR, vfScaleInt16));

		pfOutBuf += FramesPerIter;
		uIndexFP += uFrameSkipFP * FramesPerIter;
	}

	uInterpLocFP = uIndexFP & FPMASK;
	uPreviousFrameIndex = uIndexFP >> FPBITS;

	while (uNumRemainderFrames--)
	{
		AkUInt32 uPreviousFrameSamplePosL = uPreviousFrameIndex*2;
		iPreviousFrameL = pInBuf[uPreviousFrameSamplePosL];
		iPreviousFrameR = pInBuf[uPreviousFrameSamplePosL+1];
		AkInt32 iSampleDiffL = pInBuf[uPreviousFrameSamplePosL+2] - iPreviousFrameL;
		AkInt32 iSampleDiffR = pInBuf[uPreviousFrameSamplePosL+3] - iPreviousFrameR;
		*pfOutBuf = LINEAR_INTERP_I16( iPreviousFrameL, iSampleDiffL );
		pfOutBuf[uMaxFrames] = LINEAR_INTERP_I16( iPreviousFrameR, iSampleDiffR );
		++pfOutBuf;
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	PITCH_SAVE_NEXT_I16_2CHAN();
	PITCH_FIXED_DSP_TEARDOWN( uIndexFP );	
}

// Fixed resampling (no pitch changes) with INTERLEAVED signed 16-bit samples for any number of channels.
AKRESULT Fixed_I16_NChanV4F32(AkAudioBuffer * io_pInBuffer,
	AkAudioBuffer * io_pOutBuffer,
	AkUInt32 uRequestedSize,
	AkInternalPitchState * io_pPitchState)
{
	PITCH_FIXED_DSP_SETUP();

	const AkUInt32 uNumChannels = io_pInBuffer->NumChannels();
	// Minus one to compensate for offset of 1 due to zero == previous
	AkInt16 * AK_RESTRICT pInBuf = (AkInt16 * AK_RESTRICT) io_pInBuffer->GetInterleavedData() + uNumChannels * io_pPitchState->uInFrameOffset - uNumChannels;
	AkUInt32 uNumIterThisFrame;

	// Load up all of the output buffers we need (we handle pairs of outputs in the main body, so round up the number of channels)
	AkReal32** AK_RESTRICT ppfOutBuf = (AkReal32**)AkAlloca(sizeof(AkReal32*) * uNumChannels);
	for (AkUInt32 i = 0; i < uNumChannels; ++i)
	{
		ppfOutBuf[i] = (AkReal32 * AK_RESTRICT) io_pOutBuffer->GetChannel(AkChannelRemap(i, io_pPitchState )) + io_pPitchState->uOutFrameOffset;
	}

	// Handle the previous values first
	AkUInt32 uIterFrames = uNumIterPreviousFrame;
	while (uIterFrames--)
	{
		for (AkUInt32 i = 0; i < uNumChannels; ++i)
		{
			// Use stored value as left value, while right index is on the first sample
			AkInt16 iPreviousFrame = io_pPitchState->iLastValue[i];
			AkInt32 iSampleDiff = pInBuf[i + uNumChannels] - iPreviousFrame;
			*ppfOutBuf[i]++ = LINEAR_INTERP_I16(iPreviousFrame, iSampleDiff);
		}
		FP_INDEX_ADVANCE();
	}
	FP_INDEX_ADVANCE_COMPILER_PATCH();

	// Determine number of iterations remaining
	const AkUInt32 uPredNumIterFrames = ((uInBufferFrames << FPBITS) - uIndexFP + (uFrameSkipFP - 1)) / uFrameSkipFP;
	uNumIterThisFrame = AkMin(uOutBufferFrames - uNumIterPreviousFrame, uPredNumIterFrames);
	uIterFrames = uNumIterThisFrame;

	const int FramesPerIter = 4;

	AkUInt32 uNumIter = uIterFrames / (FramesPerIter);
	AkUInt32 uNumRemainderFrames = uIterFrames % FramesPerIter;

	const AKSIMD_V4F32 vfScaleFPMUL = AKSIMD_SET_V4F32(1.0f / (float)FPMUL);
	const AKSIMD_V4F32 vfScaleInt16 = AKSIMD_SET_V4F32(NORMALIZEFACTORI16);
	AKSIMD_V4I32 viIndexFPBase = AKSIMD_MULLO16_V4I32(AKSIMD_SET_V4I32(uFrameSkipFP), AKSIMD_SETV_V4I32(3, 2, 1, 0));
	AKSIMD_V4I32 viIndexFP = AKSIMD_ADD_V4I32(AKSIMD_SET_V4I32(uIndexFP), viIndexFPBase);

	// Main body for processing frames
	while (uNumIter--)
	{
		// load up the interp factor
		AKSIMD_V4I32 viInterpFactor = AKSIMD_AND_V4I32(viIndexFP, AKSIMD_SET_V4I32(FPMASK));
		AKSIMD_V4F32 vfInterpFactor = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(viInterpFactor), vfScaleFPMUL);

		// Handle two channels at a time, by doing 32b loads.
		// Similarly, advance the channel idx two channels at a time, except when uNumChannels is odd --
		// in that scenario, we need to make sure step by only 1 channel for the last stretch.
		// This produces some slightly redundant work, as the second-to-last channel is technically processed twice, but the gain of 32b loads is preferable, anyway
		for (AkUInt32 uChanIter = 0; uChanIter < uNumChannels; uChanIter += (uChanIter == (uNumChannels - 3)) ? 1 : 2)
		{
			AkInt16 * pInBufRead = pInBuf + uChanIter;

			// Do a gathered read of 4 2-channel samples (32b of data - uChanIter and uChanIter+1)
			// This way, we will operate and store a pair of vectors for all of the following calculations, one for each channel
			AKSIMD_V4I32X2 viPrev = AKSIMD_GATHER_V4I32_AND_DEINTERLEAVE_V4I32X2(
				pInBufRead + ((uIndexFP + uFrameSkipFP * 3) >> FPBITS) * uNumChannels,
				pInBufRead + ((uIndexFP + uFrameSkipFP * 2) >> FPBITS) * uNumChannels,
				pInBufRead + ((uIndexFP + uFrameSkipFP * 1) >> FPBITS) * uNumChannels,
				pInBufRead + ((uIndexFP + uFrameSkipFP * 0) >> FPBITS) * uNumChannels
			);

			// Advance pInBufRead by however many channels we have to fetch a matching "next" set of samples
			pInBufRead += uNumChannels;
			AKSIMD_V4I32X2 viNext = AKSIMD_GATHER_V4I32_AND_DEINTERLEAVE_V4I32X2(
				pInBufRead + ((uIndexFP + uFrameSkipFP * 3) >> FPBITS) * uNumChannels,
				pInBufRead + ((uIndexFP + uFrameSkipFP * 2) >> FPBITS) * uNumChannels,
				pInBufRead + ((uIndexFP + uFrameSkipFP * 1) >> FPBITS) * uNumChannels,
				pInBufRead + ((uIndexFP + uFrameSkipFP * 0) >> FPBITS) * uNumChannels
			);

			AKSIMD_V4I32 viDiff[2] = {
				AKSIMD_SUB_V4I32(viNext.val[0], viPrev.val[0]),
				AKSIMD_SUB_V4I32(viNext.val[1], viPrev.val[1])
			};
			AKSIMD_V4F32 vfPrev[2] = {
				AKSIMD_CONVERT_V4I32_TO_V4F32(viPrev.val[0]),
				AKSIMD_CONVERT_V4I32_TO_V4F32(viPrev.val[1])
			};
			AKSIMD_V4F32 vfDiff[2] = {
				AKSIMD_CONVERT_V4I32_TO_V4F32(viDiff[0]),
				AKSIMD_CONVERT_V4I32_TO_V4F32(viDiff[1])
			};

			AKSIMD_V4F32 vfResult[2] = {
				AKSIMD_MADD_V4F32(vfDiff[0], vfInterpFactor, vfPrev[0]),
				AKSIMD_MADD_V4F32(vfDiff[1], vfInterpFactor, vfPrev[1])
			};

			AKSIMD_STOREU_V4F32(ppfOutBuf[uChanIter + 0], AKSIMD_MUL_V4F32(vfResult[0], vfScaleInt16));
			AKSIMD_STOREU_V4F32(ppfOutBuf[uChanIter + 1], AKSIMD_MUL_V4F32(vfResult[1], vfScaleInt16));
		}

		// Advance everything, now that writes are complete
		for (AkUInt32 uChanIter = 0; uChanIter < uNumChannels; uChanIter++)
		{
			ppfOutBuf[uChanIter] += FramesPerIter;
		}

		uIndexFP += uFrameSkipFP * FramesPerIter;
		viIndexFP = AKSIMD_ADD_V4I32(viIndexFP, AKSIMD_SET_V4I32(uFrameSkipFP * FramesPerIter));
	}
	uInterpLocFP = uIndexFP & FPMASK;
	uPreviousFrameIndex = uIndexFP >> FPBITS;

	// For all other sample frames
	while (uNumRemainderFrames--)
	{
		for (AkUInt32 i = 0; i < uNumChannels; ++i)
		{
			AkInt16 * AK_RESTRICT pInBufChan = pInBuf + i;
			AkUInt32 uPreviousFrameSamplePos = uPreviousFrameIndex * uNumChannels;
			AkInt16 iPreviousFrame = pInBufChan[uPreviousFrameSamplePos];
			AkInt32 iSampleDiff = pInBufChan[uPreviousFrameSamplePos + uNumChannels] - iPreviousFrame;
			(*ppfOutBuf[i]++) = LINEAR_INTERP_I16(iPreviousFrame, iSampleDiff);
		}
		FP_INDEX_ADVANCE();
	}
	FP_INDEX_ADVANCE_COMPILER_PATCH();

	PITCH_SAVE_NEXT_I16_NCHAN(uIndexFP);
	PITCH_FIXED_DSP_TEARDOWN(uIndexFP);
}

#endif

AKRESULT Fixed_I16_1Chan(	AkAudioBuffer * io_pInBuffer, 
							AkAudioBuffer * io_pOutBuffer,
							AkUInt32 uRequestedSize,
							AkInternalPitchState * io_pPitchState )
{
	PITCH_FIXED_DSP_SETUP( );
	
	// Minus one to compensate for offset of 1 due to zero == previous
	AkInt16 * AK_RESTRICT pInBuf = (AkInt16 * AK_RESTRICT) io_pInBuffer->GetInterleavedData( ) + io_pPitchState->uInFrameOffset - 1; 

	// Retrieve output buffer information
	AkReal32 * AK_RESTRICT pfOutBuf = (AkReal32 * AK_RESTRICT) io_pOutBuffer->GetChannel( 0 ) + io_pPitchState->uOutFrameOffset;

	// Use stored value as left value, while right index is on the first sample
	AkInt16 iPreviousFrame = *io_pPitchState->iLastValue;
	AkUInt32 uIterFrames = uNumIterPreviousFrame;
 	while ( uIterFrames-- )
	{
		AkInt32 iSampleDiff = pInBuf[1] - iPreviousFrame;
		*pfOutBuf++ = LINEAR_INTERP_I16( iPreviousFrame, iSampleDiff );
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	// Determine number of iterations remaining
	AkUInt32 uPredNumIterFrames = ((uInBufferFrames << FPBITS) - uIndexFP + (uFrameSkipFP-1)) / uFrameSkipFP;
	AkUInt32 uNumIterThisFrame = AkMin( uOutBufferFrames-uNumIterPreviousFrame, uPredNumIterFrames );

	// For all other sample frames
	uIterFrames = uNumIterThisFrame;
	while ( uIterFrames-- )
	{
		iPreviousFrame = pInBuf[uPreviousFrameIndex];
		AkInt32 iSampleDiff = pInBuf[uPreviousFrameIndex+1] - iPreviousFrame;	
		*pfOutBuf++ = LINEAR_INTERP_I16( iPreviousFrame, iSampleDiff );
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	PITCH_SAVE_NEXT_I16_1CHAN();
	PITCH_FIXED_DSP_TEARDOWN( uIndexFP );
}

// Fixed resampling (no pitch changes) with INTERLEAVED signed 16-bit samples optimized for 2 channel signals.
AKRESULT Fixed_I16_2Chan(	AkAudioBuffer * io_pInBuffer, 
							AkAudioBuffer * io_pOutBuffer,
							AkUInt32 uRequestedSize,
							AkInternalPitchState * io_pPitchState )
{
	PITCH_FIXED_DSP_SETUP( );
	AkUInt32 uMaxFrames = io_pOutBuffer->MaxFrames();

	// Minus one to compensate for offset of 1 due to zero == previous
	AkInt16 * AK_RESTRICT pInBuf = (AkInt16 * AK_RESTRICT) io_pInBuffer->GetInterleavedData() + 2*io_pPitchState->uInFrameOffset - 2; 
	
	AkReal32 * AK_RESTRICT pfOutBuf = (AkReal32 * AK_RESTRICT) io_pOutBuffer->GetChannel( 0 ) + io_pPitchState->uOutFrameOffset;

	// Use stored value as left value, while right index is on the first sample
	AkInt16 iPreviousFrameL = io_pPitchState->iLastValue[0];
	AkInt16 iPreviousFrameR = io_pPitchState->iLastValue[1];
	AkUInt32 uIterFrames = uNumIterPreviousFrame;
	while ( uIterFrames-- )
	{
		AkInt32 iSampleDiffL = pInBuf[2] - iPreviousFrameL;
		AkInt32 iSampleDiffR = pInBuf[3] - iPreviousFrameR;
		*pfOutBuf = LINEAR_INTERP_I16( iPreviousFrameL, iSampleDiffL );
		pfOutBuf[uMaxFrames] = LINEAR_INTERP_I16( iPreviousFrameR, iSampleDiffR );
		++pfOutBuf;
		FP_INDEX_ADVANCE();	
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	// Determine number of iterations remaining
	AkUInt32 uPredNumIterFrames = ((uInBufferFrames << FPBITS) - uIndexFP + (uFrameSkipFP-1)) / uFrameSkipFP;
	AkUInt32 uNumIterThisFrame = AkMin( uOutBufferFrames-uNumIterPreviousFrame, uPredNumIterFrames );

	// For all other sample frames
	uIterFrames = uNumIterThisFrame;
	while ( uIterFrames-- )
	{
		AkUInt32 uPreviousFrameSamplePosL = uPreviousFrameIndex*2;
		iPreviousFrameL = pInBuf[uPreviousFrameSamplePosL];
		iPreviousFrameR = pInBuf[uPreviousFrameSamplePosL+1];
		AkInt32 iSampleDiffL = pInBuf[uPreviousFrameSamplePosL+2] - iPreviousFrameL;
		AkInt32 iSampleDiffR = pInBuf[uPreviousFrameSamplePosL+3] - iPreviousFrameR;
		*pfOutBuf = LINEAR_INTERP_I16( iPreviousFrameL, iSampleDiffL );
		pfOutBuf[uMaxFrames] = LINEAR_INTERP_I16( iPreviousFrameR, iSampleDiffR );
		++pfOutBuf;
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	PITCH_SAVE_NEXT_I16_2CHAN();
	PITCH_FIXED_DSP_TEARDOWN( uIndexFP );	
}

// Fixed resampling (no pitch changes) with DEINTERLEAVED floating point samples, optimized for one channel signals.
AKRESULT Fixed_Native_1Chan(	AkAudioBuffer * io_pInBuffer, 
								AkAudioBuffer * io_pOutBuffer,
								AkUInt32 uRequestedSize,
								AkInternalPitchState * io_pPitchState )
{
	PITCH_FIXED_DSP_SETUP( );

	// Minus one to compensate for offset of 1 due to zero == previous
	AkReal32 * AK_RESTRICT pInBuf = (AkReal32 * AK_RESTRICT) io_pInBuffer->GetChannel( 0 ) + io_pPitchState->uInFrameOffset - 1; 
	AkReal32 * AK_RESTRICT pfOutBuf = (AkReal32 * AK_RESTRICT) io_pOutBuffer->GetChannel( 0 ) + io_pPitchState->uOutFrameOffset;

	// Use stored value as left value, while right index is on the first sample
	static const AkReal32 fScale = 1.f / SINGLEFRAMEDISTANCE;
	AkReal32 fLeftSample = *io_pPitchState->fLastValue;
	AkUInt32 uIterFrames = uNumIterPreviousFrame;
	while ( uIterFrames-- )
	{
		AkReal32 fSampleDiff = pInBuf[1] - fLeftSample;
		*pfOutBuf++ = LINEAR_INTERP_NATIVE( fLeftSample, fSampleDiff );
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	// Determine number of iterations remaining
	AkUInt32 uPredNumIterFrames = ((uInBufferFrames << FPBITS) - uIndexFP + (uFrameSkipFP-1)) / uFrameSkipFP;
	AkUInt32 uNumIterThisFrame = AkMin( uOutBufferFrames-uNumIterPreviousFrame, uPredNumIterFrames );

	// For all other sample frames
	uIterFrames = uNumIterThisFrame;
	while ( uIterFrames-- )
	{
		fLeftSample = pInBuf[uPreviousFrameIndex];
		AkReal32 fSampleDiff = pInBuf[uPreviousFrameIndex+1] - fLeftSample;
		*pfOutBuf++ = LINEAR_INTERP_NATIVE( fLeftSample, fSampleDiff );
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	PITCH_SAVE_NEXT_NATIVE_1CHAN();
	PITCH_FIXED_DSP_TEARDOWN( uIndexFP );	
}

// Fixed resampling (no pitch changes) with DEINTERLEAVED floating point samples, optimized for 2 channel signals.
AKRESULT Fixed_Native_2Chan(	AkAudioBuffer * io_pInBuffer, 
								AkAudioBuffer * io_pOutBuffer,
								AkUInt32 uRequestedSize,
								AkInternalPitchState * io_pPitchState )
{
	PITCH_FIXED_DSP_SETUP( );
	AkUInt32 uMaxFramesIn = io_pInBuffer->MaxFrames();
	AkUInt32 uMaxFramesOut = io_pOutBuffer->MaxFrames();

	// Minus one to compensate for offset of 1 due to zero == previous
	AkReal32 * AK_RESTRICT pInBuf = (AkReal32 * AK_RESTRICT) io_pInBuffer->GetChannel( 0 ) + io_pPitchState->uInFrameOffset - 1; 
	AkReal32 * AK_RESTRICT pfOutBuf = (AkReal32 * AK_RESTRICT) io_pOutBuffer->GetChannel( 0 ) + io_pPitchState->uOutFrameOffset;

	// Use stored value as left value, while right index is on the first sample
	AkReal32 fPreviousFrameL = io_pPitchState->fLastValue[0];
	AkReal32 fPreviousFrameR = io_pPitchState->fLastValue[1];
	static const AkReal32 fScale = 1.f / SINGLEFRAMEDISTANCE;
	AkUInt32 uIterFrames = uNumIterPreviousFrame;
	while ( uIterFrames-- )
	{
		AkReal32 fSampleDiffL = pInBuf[1] - fPreviousFrameL;
		AkReal32 fSampleDiffR = pInBuf[1+uMaxFramesIn] - fPreviousFrameR;
		*pfOutBuf = LINEAR_INTERP_NATIVE( fPreviousFrameL, fSampleDiffL );
		pfOutBuf[uMaxFramesOut] = LINEAR_INTERP_NATIVE( fPreviousFrameR, fSampleDiffR );
		++pfOutBuf;
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	// Determine number of iterations remaining
	AkUInt32 uPredNumIterFrames = ((uInBufferFrames << FPBITS) - uIndexFP + (uFrameSkipFP-1)) / uFrameSkipFP;
	AkUInt32 uNumIterThisFrame = AkMin( uOutBufferFrames-uNumIterPreviousFrame, uPredNumIterFrames );

	// For all other sample frames
	uIterFrames = uNumIterThisFrame;
	while ( uIterFrames-- )
	{
		AkUInt32 uPreviousFrameR = uPreviousFrameIndex+uMaxFramesIn;
		fPreviousFrameL = pInBuf[uPreviousFrameIndex];
		AkReal32 fSampleDiffL = pInBuf[uPreviousFrameIndex+1] - fPreviousFrameL;
		fPreviousFrameR = pInBuf[uPreviousFrameR];	
		AkReal32 fSampleDiffR = pInBuf[uPreviousFrameR+1] - fPreviousFrameR;	
		*pfOutBuf = LINEAR_INTERP_NATIVE( fPreviousFrameL, fSampleDiffL );
		pfOutBuf[uMaxFramesOut] = LINEAR_INTERP_NATIVE( fPreviousFrameR, fSampleDiffR );
		++pfOutBuf;
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	PITCH_SAVE_NEXT_NATIVE_2CHAN();
	PITCH_FIXED_DSP_TEARDOWN( uIndexFP );	
}

/********************* INTERPOLATING RESAMPLING DSP ROUTINES **********************/

// Interpolating resampling (pitch changes) with INTERLEAVED signed 16-bit samples, optimized for one channel signals.
AKRESULT Interpolating_I16_1Chan(	AkAudioBuffer * io_pInBuffer, 
									AkAudioBuffer * io_pOutBuffer,
									AkUInt32 uRequestedSize,
									AkInternalPitchState * io_pPitchState )
{
	PITCH_INTERPOLATING_DSP_SETUP( );
	
	// Minus one to compensate for offset of 1 due to zero == previous
	AkInt16 * AK_RESTRICT pInBuf = (AkInt16 * AK_RESTRICT) io_pInBuffer->GetInterleavedData( ) + io_pPitchState->uInFrameOffset - 1; 
	AkReal32 * AK_RESTRICT pfOutBuf = (AkReal32 * AK_RESTRICT) io_pOutBuffer->GetChannel( 0 ) + io_pPitchState->uOutFrameOffset;
	const AkReal32 * pfOutBufStart = pfOutBuf;
	const AkReal32 * pfOutBufEnd = pfOutBuf + uOutBufferFrames;

	PITCH_INTERPOLATION_SETUP( );

	// Use stored value as left value, while right index is on the first sample
	AkInt16 iPreviousFrame = *io_pPitchState->iLastValue;
	AkUInt32 uMaxNumIter = (AkUInt32) (pfOutBufEnd - pfOutBuf);		// Not more than output frames
	uMaxNumIter = AkMin( uMaxNumIter, (PITCHRAMPLENGTH-uRampCount)/uRampInc );	// Not longer than interpolation ramp length
	while ( uPreviousFrameIndex == 0 && uMaxNumIter-- )
	{
		AkInt32 iSampleDiff = pInBuf[1] - iPreviousFrame;
		*pfOutBuf++ = LINEAR_INTERP_I16( iPreviousFrame, iSampleDiff );	
		RESAMPLING_FACTOR_INTERPOLATE();
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	// For all other sample frames that need interpolation
	const AkUInt32 uLastValidPreviousIndex = uInBufferFrames-1;
	AkUInt32 uIterFrames = (AkUInt32) (pfOutBufEnd - pfOutBuf);			// No more than the output buffer length
	uIterFrames = AkMin( uIterFrames, (PITCHRAMPLENGTH-uRampCount)/uRampInc );	// No more than the interpolation ramp length
	while ( uPreviousFrameIndex <= uLastValidPreviousIndex && uIterFrames-- )
	{
		iPreviousFrame = pInBuf[uPreviousFrameIndex];
		AkInt32 iSampleDiff = pInBuf[uPreviousFrameIndex+1] - iPreviousFrame;	
		*pfOutBuf++ = LINEAR_INTERP_I16( iPreviousFrame, iSampleDiff );
		RESAMPLING_FACTOR_INTERPOLATE();	
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	PITCH_INTERPOLATION_TEARDOWN( );
	PITCH_SAVE_NEXT_I16_1CHAN();
	AkUInt32 uFramesProduced = (AkUInt32)(pfOutBuf - pfOutBufStart);
	PITCH_INTERPOLATING_DSP_TEARDOWN( uIndexFP );
}

// Interpolating resampling (pitch changes) with INTERLEAVED signed 16-bit samples optimized for 2 channel signals.
AKRESULT Interpolating_I16_2Chan(	AkAudioBuffer * io_pInBuffer, 
									AkAudioBuffer * io_pOutBuffer,
									AkUInt32 uRequestedSize,
									AkInternalPitchState * io_pPitchState )
{
	PITCH_INTERPOLATING_DSP_SETUP( );
	AkUInt32 uMaxFrames = io_pOutBuffer->MaxFrames();

	// Minus one to compensate for offset of 1 due to zero == previous
	AkInt16 * AK_RESTRICT pInBuf = (AkInt16 * AK_RESTRICT) io_pInBuffer->GetInterleavedData() + 2*io_pPitchState->uInFrameOffset - 2; 
	AkReal32 * AK_RESTRICT pfOutBuf = (AkReal32 * AK_RESTRICT) io_pOutBuffer->GetChannel( 0 ) + io_pPitchState->uOutFrameOffset;
	const AkReal32 * pfOutBufStart = pfOutBuf;
	const AkReal32 * pfOutBufEnd = pfOutBuf + uOutBufferFrames;

	PITCH_INTERPOLATION_SETUP( );

	// Use stored value as left value, while right index is on the first sample
	AkInt16 iPreviousFrameL = io_pPitchState->iLastValue[0];
	AkInt16 iPreviousFrameR = io_pPitchState->iLastValue[1];
	AkUInt32 uMaxNumIter = (AkUInt32) (pfOutBufEnd - pfOutBuf);		// Not more than output frames
	uMaxNumIter = AkMin( uMaxNumIter, (PITCHRAMPLENGTH-uRampCount)/uRampInc );	// Not longer than interpolation ramp length
	while ( uPreviousFrameIndex == 0 && uMaxNumIter-- )
	{
		AkInt32 iSampleDiffL = pInBuf[2] - iPreviousFrameL;
		AkInt32 iSampleDiffR = pInBuf[3] - iPreviousFrameR;
		*pfOutBuf = LINEAR_INTERP_I16( iPreviousFrameL, iSampleDiffL );
		pfOutBuf[uMaxFrames] = LINEAR_INTERP_I16( iPreviousFrameR, iSampleDiffR );
		++pfOutBuf;
		RESAMPLING_FACTOR_INTERPOLATE();
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	// Avoid inner branch by splitting in 2 cases
	// For all other sample frames that need interpolation
	const AkUInt32 uLastValidPreviousIndex = uInBufferFrames-1;
	AkUInt32 uIterFrames = (AkUInt32) (pfOutBufEnd - pfOutBuf);
	uIterFrames = AkMin( uIterFrames, (PITCHRAMPLENGTH-uRampCount)/uRampInc );	// No more than the interpolation ramp length
	while ( uPreviousFrameIndex <= uLastValidPreviousIndex && uIterFrames-- )
	{
		AkUInt32 uPreviousFrameSamplePosL = uPreviousFrameIndex*2;
		iPreviousFrameL = pInBuf[uPreviousFrameSamplePosL];
		iPreviousFrameR = pInBuf[uPreviousFrameSamplePosL+1];
		AkInt32 iSampleDiffL = pInBuf[uPreviousFrameSamplePosL+2] - iPreviousFrameL;
		AkInt32 iSampleDiffR = pInBuf[uPreviousFrameSamplePosL+3] - iPreviousFrameR;
		*pfOutBuf = LINEAR_INTERP_I16( iPreviousFrameL, iSampleDiffL );
		pfOutBuf[uMaxFrames] = LINEAR_INTERP_I16( iPreviousFrameR, iSampleDiffR );
		++pfOutBuf;
		RESAMPLING_FACTOR_INTERPOLATE();
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	PITCH_INTERPOLATION_TEARDOWN( );
	PITCH_SAVE_NEXT_I16_2CHAN();
	AkUInt32 uFramesProduced = (AkUInt32)(pfOutBuf - pfOutBufStart);
	PITCH_INTERPOLATING_DSP_TEARDOWN( uIndexFP );
}

// Interpolating resampling (pitch changes) with DEINTERLEAVED floating point samples optimized for one channel signals.
AKRESULT Interpolating_Native_1Chan(	AkAudioBuffer * io_pInBuffer, 
										AkAudioBuffer * io_pOutBuffer,
										AkUInt32 uRequestedSize,
										AkInternalPitchState * io_pPitchState )
{
	PITCH_INTERPOLATING_DSP_SETUP( );

	// Minus one to compensate for offset of 1 due to zero == previous
	AkReal32 * AK_RESTRICT pInBuf = (AkReal32 * AK_RESTRICT) io_pInBuffer->GetChannel( 0 ) + io_pPitchState->uInFrameOffset - 1; 
	AkReal32 * AK_RESTRICT pfOutBuf = (AkReal32 * AK_RESTRICT) io_pOutBuffer->GetChannel( 0 ) + io_pPitchState->uOutFrameOffset;
	const AkReal32 * pfOutBufStart = pfOutBuf;
	const AkReal32 * pfOutBufEnd = pfOutBuf + uOutBufferFrames;

	PITCH_INTERPOLATION_SETUP( );

	// Use stored value as left value, while right index is on the first sample
	// Note: No interpolation necessary for the first few frames
	static const AkReal32 fScale = 1.f / SINGLEFRAMEDISTANCE;
	AkReal32 fLeftSample = *io_pPitchState->fLastValue;
	AkUInt32 uMaxNumIter = (AkUInt32) (pfOutBufEnd - pfOutBuf);		// Not more than output frames
	uMaxNumIter = AkMin( uMaxNumIter, (PITCHRAMPLENGTH-uRampCount)/uRampInc );	// Not longer than interpolation ramp length
	while ( uPreviousFrameIndex == 0 && uMaxNumIter-- )
	{
		AkReal32 fSampleDiff = pInBuf[1] - fLeftSample;
		*pfOutBuf++ = LINEAR_INTERP_NATIVE( fLeftSample, fSampleDiff );
		RESAMPLING_FACTOR_INTERPOLATE();
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	// Avoid inner branch by splitting in 2 cases
	// For all other sample frames that need interpolation
	const AkUInt32 uLastValidPreviousIndex = uInBufferFrames-1;
	AkUInt32 uIterFrames = (AkUInt32) (pfOutBufEnd - pfOutBuf);
	uIterFrames = AkMin( uIterFrames, (PITCHRAMPLENGTH-uRampCount)/uRampInc );	// No more than the interpolation ramp length
	while ( uPreviousFrameIndex <= uLastValidPreviousIndex && uIterFrames-- )
	{
		fLeftSample = pInBuf[uPreviousFrameIndex];
		AkReal32 fSampleDiff = pInBuf[uPreviousFrameIndex+1] - fLeftSample;
		*pfOutBuf++ = LINEAR_INTERP_NATIVE( fLeftSample, fSampleDiff );
		RESAMPLING_FACTOR_INTERPOLATE();
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	PITCH_INTERPOLATION_TEARDOWN( );
	PITCH_SAVE_NEXT_NATIVE_1CHAN();
	AkUInt32 uFramesProduced = (AkUInt32)(pfOutBuf - pfOutBufStart);
	PITCH_INTERPOLATING_DSP_TEARDOWN( uIndexFP );
}

//  Interpolating resampling (pitch changes) with DEINTERLEAVED floating point samples optimized for 2 channel signals.
AKRESULT Interpolating_Native_2Chan(	AkAudioBuffer * io_pInBuffer, 
										AkAudioBuffer * io_pOutBuffer,
										AkUInt32 uRequestedSize,
										AkInternalPitchState * io_pPitchState )
{
	PITCH_INTERPOLATING_DSP_SETUP( );
	AkUInt32 uMaxFramesIn = io_pInBuffer->MaxFrames();
	AkUInt32 uMaxFramesOut = io_pOutBuffer->MaxFrames();

	// Minus one to compensate for offset of 1 due to zero == previous
	AkReal32 * AK_RESTRICT pInBuf = (AkReal32 * AK_RESTRICT) io_pInBuffer->GetChannel( 0 ) + io_pPitchState->uInFrameOffset - 1; 
	AkReal32 * AK_RESTRICT pfOutBuf = (AkReal32 * AK_RESTRICT) io_pOutBuffer->GetChannel( 0 ) + io_pPitchState->uOutFrameOffset;
	const AkReal32 * pfOutBufStart = pfOutBuf;
	const AkReal32 * pfOutBufEnd = pfOutBuf + uOutBufferFrames;

	PITCH_INTERPOLATION_SETUP( );

	// Use stored value as left value, while right index is on the first sample
	// Note: No interpolation necessary for the first few frames
	AkReal32 fPreviousFrameL = io_pPitchState->fLastValue[0];
	AkReal32 fPreviousFrameR = io_pPitchState->fLastValue[1];
	static const AkReal32 fScale = 1.f / SINGLEFRAMEDISTANCE;
	AkUInt32 uMaxNumIter = (AkUInt32) (pfOutBufEnd - pfOutBuf);		// Not more than output frames
	uMaxNumIter = AkMin( uMaxNumIter, (PITCHRAMPLENGTH-uRampCount)/uRampInc );	// Not longer than interpolation ramp length
	while ( uPreviousFrameIndex == 0 && uMaxNumIter-- )
	{
		AkReal32 fSampleDiffL = pInBuf[1] - fPreviousFrameL;
		AkReal32 fSampleDiffR = pInBuf[1+uMaxFramesIn] - fPreviousFrameR;
		*pfOutBuf = LINEAR_INTERP_NATIVE( fPreviousFrameL, fSampleDiffL );
		pfOutBuf[uMaxFramesOut] = LINEAR_INTERP_NATIVE( fPreviousFrameR, fSampleDiffR );
		++pfOutBuf;
		RESAMPLING_FACTOR_INTERPOLATE();
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	// For all other sample frames that need interpolation
	const AkUInt32 uLastValidPreviousIndex = uInBufferFrames-1;
	AkUInt32 uIterFrames = (AkUInt32) (pfOutBufEnd - pfOutBuf);
	uIterFrames = AkMin( uIterFrames, (PITCHRAMPLENGTH-uRampCount)/uRampInc );	// No more than the interpolation ramp length
	while ( uPreviousFrameIndex <= uLastValidPreviousIndex && uIterFrames-- )
	{
		// Linear interpolation and index advance
		AkUInt32 uPreviousFrameR = uPreviousFrameIndex+uMaxFramesIn;
		fPreviousFrameL = pInBuf[uPreviousFrameIndex];
		AkReal32 fSampleDiffL = pInBuf[uPreviousFrameIndex+1] - fPreviousFrameL;
		fPreviousFrameR = pInBuf[uPreviousFrameR];	
		AkReal32 fSampleDiffR = pInBuf[uPreviousFrameR+1] - fPreviousFrameR;	
		*pfOutBuf = LINEAR_INTERP_NATIVE( fPreviousFrameL, fSampleDiffL );
		pfOutBuf[uMaxFramesOut] = LINEAR_INTERP_NATIVE( fPreviousFrameR, fSampleDiffR );
		++pfOutBuf;
		RESAMPLING_FACTOR_INTERPOLATE();
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	PITCH_INTERPOLATION_TEARDOWN( );
	PITCH_SAVE_NEXT_NATIVE_2CHAN();
	AkUInt32 uFramesProduced = (AkUInt32)(pfOutBuf - pfOutBufStart);
	PITCH_INTERPOLATING_DSP_TEARDOWN( uIndexFP );
}
