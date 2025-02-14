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
// AkResamplerAvx2.cpp
// 
// AVX2 specific implementations of Resampler functions
//
/////////////////////////////////////////////////////////////////////

#include "stdafx.h" 
#include "AkResamplerCommon.h"
#include <AK/SoundEngine/Common/AkSimd.h>
#include <AK/SoundEngine/Platforms/SSE/AkSimdAvx2.h>

// Bypass (no pitch or resampling) with INTERLEAVED signed 16-bit samples optimized for 1 channel signals. 
AKRESULT Bypass_I16_1ChanAVX2(	AkAudioBuffer * io_pInBuffer,
								AkAudioBuffer * io_pOutBuffer,
								AkUInt32 uRequestedSize,
								AkInternalPitchState * io_pPitchState)
{
	PITCH_BYPASS_DSP_SETUP();

	const AkUInt32 uNumChannels = io_pInBuffer->NumChannels();
	AkInt16 * AK_RESTRICT pIn = (AkInt16 * AK_RESTRICT)(io_pInBuffer->GetInterleavedData()) + io_pPitchState->uInFrameOffset;
	AkReal32 * AK_RESTRICT pOut = (AkReal32 * AK_RESTRICT)(io_pOutBuffer->GetChannel(0)) + io_pPitchState->uOutFrameOffset;

	const int FramesPerUnroll = 8;
	const int UnrollsPerIter = 4;

	const AkUInt32 uNumIter = uFramesToCopy / (FramesPerUnroll * UnrollsPerIter);
	AkUInt32 uRemaining = uFramesToCopy - (uNumIter * (FramesPerUnroll * UnrollsPerIter));

	const AkInt16 * AK_RESTRICT pInEnd = pIn + (FramesPerUnroll * UnrollsPerIter)*uNumIter;
	const AKSIMD_V8F32 m256Scale = AKSIMD_SET_V8F32(NORMALIZEFACTORI16);
	const AKSIMD_V8I32 m256iZero = AKSIMD_SETZERO_V8I32();

	// Process blocks of 8*4 frames
	while (pIn < pInEnd)
	{
		for (int i = 0; i < UnrollsPerIter; ++i)
		{
			AKSIMD_V4I32 m128InFrames = AKSIMD_LOADU_V4I32((AKSIMD_V4I32 AK_UNALIGNED *) (pIn + FramesPerUnroll * i));
			AKSIMD_V8I32 m256InFrames = AKSIMD_CONVERT_V8I16_TO_V8I32(m128InFrames);
			AKSIMD_V8F32 m256out = AKSIMD_CONVERT_V8I32_TO_V8F32(m256InFrames);
			AKSIMD_STORE_V8F32(pOut + FramesPerUnroll * i, AKSIMD_MUL_V8F32(m256out, m256Scale));
		}
		pIn += (FramesPerUnroll * UnrollsPerIter);
		pOut += (FramesPerUnroll * UnrollsPerIter);
	}

	// Deal with remaining Frames
	while (uRemaining--)
	{
		*pOut++ = INT16_TO_FLOAT(*pIn++);
	}

	// Need to keep the last buffer value in case we start the pitch algo next buffer
	pIn -= 1;
	io_pPitchState->iLastValue[0] = pIn[0];

	PITCH_BYPASS_DSP_TEARDOWN();
}

// Bypass (no pitch or resampling) with INTERLEAVED signed 16-bit samples optimized for 2 channel signals. 
AKRESULT Bypass_I16_2ChanAVX2(	AkAudioBuffer * io_pInBuffer,
								AkAudioBuffer * io_pOutBuffer,
								AkUInt32 uRequestedSize,
								AkInternalPitchState * io_pPitchState)
{
	PITCH_BYPASS_DSP_SETUP();

	AkInt32 uLastSample = static_cast<AkInt32>(uFramesToCopy - 1);

	// IMPORTANT: Second channel access relies on the fact that at least the 
	// first 2 channels of AkAudioBuffer are contiguous in memory.
	AkInt16 * AK_RESTRICT pIn = (AkInt16 * AK_RESTRICT)(io_pInBuffer->GetInterleavedData()) + 2 * io_pPitchState->uInFrameOffset;
	AkReal32 * AK_RESTRICT pOut = (AkReal32 * AK_RESTRICT)(io_pOutBuffer->GetChannel(0)) + io_pPitchState->uOutFrameOffset;

	// Need to keep the last buffer value in case we start the pitch algo next buffer
	io_pPitchState->iLastValue[0] = pIn[2 * uLastSample];
	io_pPitchState->iLastValue[1] = pIn[2 * uLastSample + 1];

	const int SamplesPerUnroll = 8;
	const int UnrollsPerIter = 4;

	AkUInt32 uNumIter = uFramesToCopy / (SamplesPerUnroll * UnrollsPerIter);
	AkUInt32 uRemaining = uFramesToCopy - (uNumIter * (SamplesPerUnroll * UnrollsPerIter));
	AkUInt32 uBlockRemaining = uRemaining / SamplesPerUnroll;
	AkUInt32 uIndividualRemaining = uRemaining % SamplesPerUnroll;

	const AkInt16 * AK_RESTRICT pInEnd = pIn + 2 * SamplesPerUnroll * UnrollsPerIter * uNumIter;
	const AKSIMD_V8F32 m256Scale = AKSIMD_SET_V8F32(NORMALIZEFACTORI16);

	AkUInt32 uMaxFrames = io_pOutBuffer->MaxFrames();

	// Process blocks of 8*4 first
	while (pIn < pInEnd)
	{
		for (int i = 0; i < UnrollsPerIter; ++i)
		{
			AKSIMD_V8I32 m256InSamples = AKSIMD_LOAD_V8I32((AKSIMD_V8I32 AK_UNALIGNED *) (pIn + SamplesPerUnroll * i * 2));
			AKSIMD_V8I32 m256InSamplesL = AKSIMD_SHIFTRIGHTARITH_V8I32(AKSIMD_SHIFTLEFT_V8I32(m256InSamples, 16), 16);	// Prepare left samples for conversion
			AKSIMD_V8I32 m256InSamplesR = AKSIMD_SHIFTRIGHTARITH_V8I32(m256InSamples, 16);						// Prepare right samples for conversion
			AKSIMD_V8F32 m256OutL = AKSIMD_CONVERT_V8I32_TO_V8F32(m256InSamplesL);									// Convert Int32 to floats
			AKSIMD_V8F32 m256OutR = AKSIMD_CONVERT_V8I32_TO_V8F32(m256InSamplesR);									// Convert Int32 to floats
			AKSIMD_STORE_V8F32(pOut + SamplesPerUnroll * i, AKSIMD_MUL_V8F32(m256OutL, m256Scale));
			AKSIMD_STORE_V8F32(pOut + SamplesPerUnroll * i + uMaxFrames, AKSIMD_MUL_V8F32(m256OutR, m256Scale));
		}
		pIn += 2 * SamplesPerUnroll * UnrollsPerIter;
		pOut += SamplesPerUnroll * UnrollsPerIter;
	}

	// Process remainder in blocks of 8,
	while (uBlockRemaining--)
	{
		AKSIMD_V8I32 m256InSamples = AKSIMD_LOAD_V8I32((AKSIMD_V8I32 AK_UNALIGNED *) (pIn));
		AKSIMD_V8I32 m256InSamplesL = AKSIMD_SHIFTRIGHTARITH_V8I32(AKSIMD_SHIFTLEFT_V8I32(m256InSamples, 16), 16);	// Prepare left samples for conversion
		AKSIMD_V8I32 m256InSamplesR = AKSIMD_SHIFTRIGHTARITH_V8I32(m256InSamples, 16);						// Prepare right samples for conversion
		AKSIMD_V8F32 m256OutL = AKSIMD_CONVERT_V8I32_TO_V8F32(m256InSamplesL);									// Convert Int32 to floats
		AKSIMD_V8F32 m256OutR = AKSIMD_CONVERT_V8I32_TO_V8F32(m256InSamplesR);									// Convert Int32 to floats
		AKSIMD_STORE_V8F32(pOut, AKSIMD_MUL_V8F32(m256OutL, m256Scale));
		AKSIMD_STORE_V8F32(pOut + uMaxFrames, AKSIMD_MUL_V8F32(m256OutR, m256Scale));

		pIn += 2 * SamplesPerUnroll;
		pOut += SamplesPerUnroll;
	}

	// Then process remainder 1-by-1
	while (uIndividualRemaining--)
	{
		*pOut = INT16_TO_FLOAT(*pIn++);
		pOut[uMaxFrames] = INT16_TO_FLOAT(*pIn++);
		++pOut;
	}

	PITCH_BYPASS_DSP_TEARDOWN();
}

// Bypass (no pitch or resampling) with signed 16-bit samples vectorized for any number of channels.
AKRESULT Bypass_I16_NChanAVX2(	AkAudioBuffer * io_pInBuffer,
								AkAudioBuffer * io_pOutBuffer,
								AkUInt32 uRequestedSize,
								AkInternalPitchState * io_pPitchState)
{
	PITCH_BYPASS_DSP_SETUP();

	const AkUInt32 uNumChannels = io_pInBuffer->NumChannels();
	AkInt16 * AK_RESTRICT pIn = (AkInt16 * AK_RESTRICT)(io_pInBuffer->GetInterleavedData()) + io_pPitchState->uInFrameOffset*uNumChannels;
	
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
	const int FramesPerIter = 8;
	AkUInt32 uNumIter = uFramesToCopy / (FramesPerIter);
	AkUInt32 uNumRemainderFrames = uFramesToCopy % FramesPerIter;

	const AKSIMD_V8F32 vfScaleInt16 = AKSIMD_SET_V8F32(NORMALIZEFACTORI16);

	// Main body for processing frames
	while (uNumIter--)
	{
		for (AkUInt32 uChanIter = 0; uChanIter < uNumChannelsRounded; uChanIter += 2)
		{
			AkInt16 * pInRead = pIn + uChanIter;

			// Do a gathered read of 8 2-channel samples (32b of data - uChanIter and uChanIter+1)
			// This way, we will operate and store a pair of vectors for all of the following calculations, one for each channel
			AKSIMD_V8I32 viSamples16 = AKSIMD_GATHER_EPI32(pInRead,
				[uNumChannels](int i) { return i * uNumChannels; });

			AKSIMD_V8I32 viSamples32[2] = {
				AKSIMD_SHIFTRIGHTARITH_V8I32(AKSIMD_SHIFTLEFT_V8I32(viSamples16, 16),16),
				AKSIMD_SHIFTRIGHTARITH_V8I32(viSamples16, 16)
			};
			AKSIMD_V8F32 vfSamples[2] = {
				AKSIMD_CONVERT_V8I32_TO_V8F32(viSamples32[0]),
				AKSIMD_CONVERT_V8I32_TO_V8F32(viSamples32[1])
			};

			// Note that we write to the 1th result first - this is because 
			// if we have an odd number of channels, then the 1th vfResult is 
			// full of irrelevant data, and we want to toss it. We do so
			// by unconditionally writing to the 1th buffer first - which should
			// be the same location as the 0th buffer - and then we clobber 
			// that write with the 0th result
			AKSIMD_STORE_V8F32(ppOut[uChanIter + 1], AKSIMD_MUL_V8F32(vfSamples[1], vfScaleInt16));
			AKSIMD_STORE_V8F32(ppOut[uChanIter + 0], AKSIMD_MUL_V8F32(vfSamples[0], vfScaleInt16));

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

// Fixed resampling (no pitch changes) with signed 16-bit samples optimized for one channel signals.
AKRESULT Fixed_I16_1ChanAVX2(	AkAudioBuffer * io_pInBuffer,
								AkAudioBuffer * io_pOutBuffer,
								AkUInt32 uRequestedSize,
								AkInternalPitchState * io_pPitchState)
{
	PITCH_FIXED_DSP_SETUP();

	// Minus one to compensate for offset of 1 due to zero == previous
	AkInt16 * AK_RESTRICT pInBuf = (AkInt16 * AK_RESTRICT) io_pInBuffer->GetInterleavedData() + io_pPitchState->uInFrameOffset - 1;

	// Retrieve output buffer information
	AkReal32 * AK_RESTRICT pfOutBuf = (AkReal32 * AK_RESTRICT) io_pOutBuffer->GetChannel(0) + io_pPitchState->uOutFrameOffset;

	// Use stored value as left value, while right index is on the first sample
	AkInt16 iPreviousFrame = *io_pPitchState->iLastValue;
	AkUInt32 uIterFrames = uNumIterPreviousFrame;
	while (uIterFrames--)
	{
		AkInt32 iSampleDiff = pInBuf[1] - iPreviousFrame;
		*pfOutBuf++ = LINEAR_INTERP_I16(iPreviousFrame, iSampleDiff);
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	// Determine number of iterations remaining
	AkUInt32 uPredNumIterFrames = ((uInBufferFrames << FPBITS) - uIndexFP + (uFrameSkipFP - 1)) / uFrameSkipFP;
	AkUInt32 uNumIterThisFrame = AkMin(uOutBufferFrames - uNumIterPreviousFrame, uPredNumIterFrames);

	const int FramesPerIter = 8;
	uIterFrames = uNumIterThisFrame;
	AkUInt32 uNumIter = uIterFrames / (FramesPerIter);
	AkUInt32 uNumRemainderFrames = uIterFrames % FramesPerIter;

	const AKSIMD_V8F32 vfScaleFPMUL = AKSIMD_SET_V8F32(1.0f / (float)FPMUL);
	const AKSIMD_V8F32 vfScaleInt16 = AKSIMD_SET_V8F32(NORMALIZEFACTORI16);

	// Prep viIndexFp with fixed offsets in each index - then we can advance each value by each value in viFrameSkipFP
	AKSIMD_V8I32 viFrameSkipFP = AKSIMD_SET_V8I32(uFrameSkipFP * FramesPerIter);
	AKSIMD_V8I32 viIndexFp = AKSIMD_SET_V8I32(uIndexFP);
	AKSIMD_V8I32 viFrameSkipBase = AKSIMD_MULLO_V8I32(AKSIMD_SET_V8I32(uFrameSkipFP), AKSIMD_SETV_V8I32(7, 6, 5, 4, 3, 2, 1, 0));
	viIndexFp = AKSIMD_ADD_V8I32(viIndexFp, viFrameSkipBase);

	while (uNumIter--)
	{
		// gather the frames. We do a 32-bit load against 16-bit data, so we load the current frame and the next one.
		AKSIMD_V8I32 viFrames = AKSIMD_GATHER_EPI32(pInBuf, [uIndexFP, uFrameSkipFP](AkInt32 i) {
			return ((uIndexFP + uFrameSkipFP * i) >> FPBITS); 
		});

		// Extract the pair of 16b frames from each 32b element loaded and calculate their delta
		AKSIMD_V8I32 viPrevFrames = AKSIMD_SHIFTRIGHTARITH_V8I32(AKSIMD_SHIFTLEFT_V8I32(viFrames, 16), 16);
		AKSIMD_V8I32 viNextFrames = AKSIMD_SHIFTRIGHTARITH_V8I32(viFrames, 16);
		AKSIMD_V8I32 viDiff = AKSIMD_SUB_V8I32(viNextFrames, viPrevFrames);

		AKSIMD_V8F32 vfPrevFrames = AKSIMD_CONVERT_V8I32_TO_V8F32(viPrevFrames);
		AKSIMD_V8F32 vfDiff = AKSIMD_CONVERT_V8I32_TO_V8F32(viDiff);

		// load up the interp factor
		AKSIMD_V8I32 viInterpFactor = AKSIMD_AND_V8I32(viIndexFp, AKSIMD_SET_V8I32(FPMASK));
		AKSIMD_V8F32 vfInterpFactor = AKSIMD_MUL_V8F32(AKSIMD_CONVERT_V8I32_TO_V8F32(viInterpFactor), vfScaleFPMUL);
		viIndexFp = AKSIMD_ADD_V8I32(viIndexFp, viFrameSkipFP);

		// Interpolate between prev and next frames
		AKSIMD_V8F32 vfResult = AKSIMD_MADD_V8F32(vfDiff, vfInterpFactor, vfPrevFrames);

		// write out the result
		AKSIMD_STORE_V8F32(pfOutBuf, AKSIMD_MUL_V8F32(vfResult, vfScaleInt16));

		pfOutBuf += FramesPerIter;
		uIndexFP += uFrameSkipFP * FramesPerIter;
	}

	// Finalize other values based on final uIndexFP
	uPreviousFrameIndex = uIndexFP >> FPBITS; 
	uInterpLocFP = uIndexFP & FPMASK;

	while (uNumRemainderFrames--)
	{
		iPreviousFrame = pInBuf[uPreviousFrameIndex];
		AkInt32 iSampleDiff = pInBuf[uPreviousFrameIndex + 1] - iPreviousFrame;
		*pfOutBuf++ = LINEAR_INTERP_I16(iPreviousFrame, iSampleDiff);
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();

	PITCH_SAVE_NEXT_I16_1CHAN();
	PITCH_FIXED_DSP_TEARDOWN(uIndexFP);
}

// Fixed resampling (no pitch changes) with signed 16-bit samples optimized for two-channel signals.
AKRESULT Fixed_I16_2ChanAVX2(	AkAudioBuffer * io_pInBuffer,
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
	const int FramesPerIter = 8;
	uIterFrames = uNumIterThisFrame;
	AkUInt32 uNumIter = uIterFrames / (FramesPerIter);
	AkUInt32 uNumRemainderFrames = uIterFrames % FramesPerIter;

	const AKSIMD_V8F32 vfScaleFPMUL = AKSIMD_SET_V8F32( 1.0f / (float) FPMUL );
	const AKSIMD_V8F32 vfScaleInt16 = AKSIMD_SET_V8F32( NORMALIZEFACTORI16 );

	// Prep viIndexFp with fixed offsets in each index - then we can advance each value by each value in viFrameSkipFP
	AKSIMD_V8I32 viFrameSkipFP = AKSIMD_SET_V8I32(uFrameSkipFP * FramesPerIter);
	AKSIMD_V8I32 viIndexFp = AKSIMD_SET_V8I32(uIndexFP);
	AKSIMD_V8I32 viFrameSkipBase = AKSIMD_MULLO_V8I32(AKSIMD_SET_V8I32(uFrameSkipFP), AKSIMD_SETV_V8I32(7, 6, 5, 4, 3, 2, 1, 0));
	viIndexFp = AKSIMD_ADD_V8I32(viIndexFp, viFrameSkipBase);

	while (uNumIter--)
	{
		// gather the frames. We do a 64-bit load against 16-bit data
		// so we load the prevL, then prevR, then nextL, then nextR frames in each load

		// The transform to i is to save a couple permutes when rearranging data later.
		// Instead of 4 sequential loads to form data like:		0: {d c b a} 1: {h g f e}
		// the data arrangement is instead permuted to:			0: {f e b a} 1: {h g d c}  
		AKSIMD_V8I32 viFrames[2] = {
				AKSIMD_GATHER_EPI64(pInBuf, [uIndexFP, uFrameSkipFP](AkInt32 i) {
					AkInt32 idx[4] = { 0, 1, 4, 5 };
					return ((uIndexFP + uFrameSkipFP * idx[i]) >> FPBITS) * 2;
				}),
				AKSIMD_GATHER_EPI64(pInBuf, [uIndexFP, uFrameSkipFP](AkInt32 i) {
					AkInt32 idx[4] = { 2, 3, 6, 7 };
					return ((uIndexFP + uFrameSkipFP * idx[i]) >> FPBITS) * 2;
				})
		};


		//////////////////////////////////////////////////////////////////////////
		// Extract the quartet of 16b stereo prev and next frames from each 64b element loaded.
		// a-h is the 0th through 7th frame quartet; l = prevL, r = prevR, m = nextL, s = nextR
		//
		//	viFrames[0]			:=	fs	fm	fr	fl	es	em	er	el	bs	bm	br	bl	as	am	ar	al
		//	viFrames[1]			:=	hs	hm	hr	hl	gs	gm	gr	gl	ds	dm	dr	dl	cs	cm	cr	cl
		//
		//	viFullPrevFrames[0] :=	fr	fl	er	el	fr	fl	er	el	br	bl	ar	al	br	bl	ar	al
		//	viFullPrevFrames[1] :=	hr	hl	gr	gl	hr	hl	gr	gl	dr	dl	cr	cl	dr	dl	cr	cl
		//	viFullPrev			:=	hr	hl	gr	gl	fr	fl	er	el	dr	dl	cr	cl	br	bl	ar	al
		//	viPrevL				:=		hl		gl		fl		el		dl		cl		bl		al
		//	viPrevR				:=		hr		gr		fr		er		dr		cr		br		ar
		AKSIMD_V8I32 viFullPrevFrames[2] = {
			AKSIMD_SHUFFLEB_V8I32(viFrames[0], AKSIMD_SETV_V8I32(0x0B0A0908, 0x03020100, 0x0B0A0908, 0x03020100, 0x0B0A0908, 0x03020100, 0x0B0A0908, 0x03020100)),
			AKSIMD_SHUFFLEB_V8I32(viFrames[1], AKSIMD_SETV_V8I32(0x0B0A0908, 0x03020100, 0x0B0A0908, 0x03020100, 0x0B0A0908, 0x03020100, 0x0B0A0908, 0x03020100))
		};
		AKSIMD_V8I32 viFullPrev = AKSIMD_BLEND_V16I16(viFullPrevFrames[0], viFullPrevFrames[1], 0xF0);
		AKSIMD_V8I32 viPrevL = AKSIMD_SHIFTRIGHTARITH_V8I32(AKSIMD_SHIFTLEFT_V8I32(viFullPrev, 16), 16);
		AKSIMD_V8I32 viPrevR = AKSIMD_SHIFTRIGHTARITH_V8I32(viFullPrev, 16);
	
		//	viFullNextFrames[0] :=	fs	fm	es	em	fs	fm	es	em	bs	bm	as	am	bs	bm	as	am
		//	viFullNextFrames[1] :=	hs	hm	gs	gm	hs	hm	gs	gm	ds	dm	cs	cm	ds	dm	cs	cm
		//	viFullNext			:=	hs	hm	gs	gm	fs	fm	es	em	ds	dm	cs	cm	bs	bm	as	am
		AKSIMD_V8I32 viFullNextFrames[2] = {
			AKSIMD_SHUFFLEB_V8I32(viFrames[0], AKSIMD_SETV_V8I32(0x0F0E0D0C, 0x07060504, 0x0F0E0D0C, 0x07060504, 0x0F0E0D0C, 0x07060504, 0x0F0E0D0C, 0x07060504)),
			AKSIMD_SHUFFLEB_V8I32(viFrames[1], AKSIMD_SETV_V8I32(0x0F0E0D0C, 0x07060504, 0x0F0E0D0C, 0x07060504, 0x0F0E0D0C, 0x07060504, 0x0F0E0D0C, 0x07060504))
		};
		AKSIMD_V8I32 viFullNext = AKSIMD_BLEND_V16I16(viFullNextFrames[0], viFullNextFrames[1], 0xF0);
		AKSIMD_V8I32 viNextL = AKSIMD_SHIFTRIGHTARITH_V8I32(AKSIMD_SHIFTLEFT_V8I32(viFullNext, 16), 16);
		AKSIMD_V8I32 viNextR = AKSIMD_SHIFTRIGHTARITH_V8I32(viFullNext, 16);

		AKSIMD_V8I32 viDiffL = AKSIMD_SUB_V8I32(viNextL, viPrevL);
		AKSIMD_V8I32 viDiffR = AKSIMD_SUB_V8I32(viNextR, viPrevR);

		AKSIMD_V8F32 vfPrevL = AKSIMD_CONVERT_V8I32_TO_V8F32(viPrevL);
		AKSIMD_V8F32 vfPrevR = AKSIMD_CONVERT_V8I32_TO_V8F32(viPrevR);
		AKSIMD_V8F32 vfDiffL = AKSIMD_CONVERT_V8I32_TO_V8F32(viDiffL);
		AKSIMD_V8F32 vfDiffR = AKSIMD_CONVERT_V8I32_TO_V8F32(viDiffR);

		// load up the interp factor
		AKSIMD_V8I32 viInterpFactor = AKSIMD_AND_V8I32(viIndexFp, AKSIMD_SET_V8I32(FPMASK));
		AKSIMD_V8F32 vfInterpFactor = AKSIMD_MUL_V8F32(AKSIMD_CONVERT_V8I32_TO_V8F32(viInterpFactor), vfScaleFPMUL);
		viIndexFp = AKSIMD_ADD_V8I32(viIndexFp, viFrameSkipFP);

		// Interpolate between prev and next frames
		AKSIMD_V8F32 vfResultL = AKSIMD_MADD_V8F32(vfDiffL, vfInterpFactor, vfPrevL);
		AKSIMD_V8F32 vfResultR = AKSIMD_MADD_V8F32(vfDiffR, vfInterpFactor, vfPrevR);

		// write out the result
		AKSIMD_STORE_V8F32( pfOutBuf,				AKSIMD_MUL_V8F32( vfResultL, vfScaleInt16 ) ); 
		AKSIMD_STORE_V8F32( pfOutBuf + uMaxFrames, AKSIMD_MUL_V8F32( vfResultR, vfScaleInt16 ) ); 
		
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
		pfOutBuf[0] = LINEAR_INTERP_I16( iPreviousFrameL, iSampleDiffL );
		pfOutBuf[uMaxFrames] = LINEAR_INTERP_I16( iPreviousFrameR, iSampleDiffR );
		++pfOutBuf;
		FP_INDEX_ADVANCE();
	}

	FP_INDEX_ADVANCE_COMPILER_PATCH();
	PITCH_SAVE_NEXT_I16_2CHAN();
	PITCH_FIXED_DSP_TEARDOWN( uIndexFP );	
}

// Fixed resampling (no pitch changes) with INTERLEAVED signed 16-bit samples for any number of channels.
AKRESULT Fixed_I16_NChanAVX2(AkAudioBuffer * io_pInBuffer,
	AkAudioBuffer * io_pOutBuffer,
	AkUInt32 uRequestedSize,
	AkInternalPitchState * io_pPitchState)
{
	PITCH_FIXED_DSP_SETUP();

	const AkUInt32 uNumChannels = io_pInBuffer->NumChannels();
	// Minus one to compensate for offset of 1 due to zero == previous
	AkInt16 * AK_RESTRICT pInBuf = (AkInt16 * AK_RESTRICT) io_pInBuffer->GetInterleavedData() + uNumChannels * io_pPitchState->uInFrameOffset - uNumChannels;
	AkUInt32 uNumIterThisFrame;

	// Load up all of the output buffers we need (we handle pairs of outputs in the main body, so round down the number of channels)
	AkReal32** AK_RESTRICT ppfOutBuf = (AkReal32**)AkAlloca(sizeof(AkReal32*) * uNumChannels);
	for (AkUInt32 i = 0; i < uNumChannels; ++i)
	{
		ppfOutBuf[i] = (AkReal32 * AK_RESTRICT) io_pOutBuffer->GetChannel(AkChannelRemap(i, io_pPitchState)) + io_pPitchState->uOutFrameOffset;
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

	const int FramesPerIter = 8;

	AkUInt32 uNumIter = uIterFrames / (FramesPerIter);
	AkUInt32 uNumRemainderFrames = uIterFrames % FramesPerIter;

	const AKSIMD_V8F32 vfScaleFPMUL = AKSIMD_SET_V8F32(1.0f / (float)FPMUL);
	const AKSIMD_V8F32 vfScaleInt16 = AKSIMD_SET_V8F32(NORMALIZEFACTORI16);
	AKSIMD_V8I32 viIndexFPBase = AKSIMD_MULLO_V8I32(AKSIMD_SET_V8I32(uFrameSkipFP), AKSIMD_SETV_V8I32(7, 6, 5, 4, 3, 2, 1, 0));
	AKSIMD_V8I32 viIndexFP = AKSIMD_ADD_V8I32(AKSIMD_SET_V8I32(uIndexFP), viIndexFPBase);

	// Main body for processing frames
	const AkUInt32 uNumChannelsRounded = (uNumChannels / 2) * 2;
	while (uNumIter--)
	{
		// load up the interp factor
		AKSIMD_V8I32 viInterpFactor = AKSIMD_AND_V8I32(viIndexFP, AKSIMD_SET_V8I32(FPMASK));
		AKSIMD_V8F32 vfInterpFactor = AKSIMD_MUL_V8F32(AKSIMD_CONVERT_V8I32_TO_V8F32(viInterpFactor), vfScaleFPMUL);

		// Handle two channels at a time, by doing 32b loads.
		// Similarly, advance the channel idx two channels at a time, except when uNumChannels is odd --
		// in that scenario, we need to make sure step by only 1 channel for the last stretch.
		// This produces some slightly redundant work, as the second-to-last channel is technically processed twice, but the gain of 32b loads is preferable, anyway
		for (AkUInt32 uChanIter = 0; uChanIter < uNumChannels; uChanIter += (uChanIter == (uNumChannels - 3)) ? 1 : 2)
		{
			AkInt16 * pInBufRead = pInBuf + uChanIter;

			// Do a gathered read of 8 2-channel samples (32b of data - uChanIter and uChanIter+1)
			// This way, we will operate and store a pair of vectors for all of the following calculations, one for each channel
			AKSIMD_V8I32 viPrev16 = AKSIMD_GATHER_EPI32(pInBufRead,
				[uIndexFP, uFrameSkipFP, uNumChannels](int i) {
				return ((uIndexFP + uFrameSkipFP * i) >> FPBITS) * uNumChannels;
			});
			// Advance pInBufRead by however many channels we have to fetch a matching "next" set of samples
			pInBufRead += uNumChannels;
			AKSIMD_V8I32 viNext16 = AKSIMD_GATHER_EPI32(pInBufRead,
				[uIndexFP, uFrameSkipFP, uNumChannels](int i) {
				return ((uIndexFP + uFrameSkipFP * i) >> FPBITS) * uNumChannels;
			});

			AKSIMD_V8I32 viPrev[2] = {
				AKSIMD_SHIFTRIGHTARITH_V8I32(AKSIMD_SHIFTLEFT_V8I32(viPrev16, 16),16),
				AKSIMD_SHIFTRIGHTARITH_V8I32(viPrev16, 16)
			};
			AKSIMD_V8I32 viNext[2] = {
				AKSIMD_SHIFTRIGHTARITH_V8I32(AKSIMD_SHIFTLEFT_V8I32(viNext16, 16),16),
				AKSIMD_SHIFTRIGHTARITH_V8I32(viNext16, 16)
			};

			AKSIMD_V8I32 viDiff[2] = {
				AKSIMD_SUB_V8I32(viNext[0], viPrev[0]),
				AKSIMD_SUB_V8I32(viNext[1], viPrev[1])
			};
			AKSIMD_V8F32 vfPrev[2] = {
				AKSIMD_CONVERT_V8I32_TO_V8F32(viPrev[0]),
				AKSIMD_CONVERT_V8I32_TO_V8F32(viPrev[1])
			};
			AKSIMD_V8F32 vfDiff[2] = {
				AKSIMD_CONVERT_V8I32_TO_V8F32(viDiff[0]),
				AKSIMD_CONVERT_V8I32_TO_V8F32(viDiff[1])
			};

			AKSIMD_V8F32 vfResult[2] = {
				AKSIMD_MADD_V8F32(vfDiff[0], vfInterpFactor, vfPrev[0]),
				AKSIMD_MADD_V8F32(vfDiff[1], vfInterpFactor, vfPrev[1])
			};

			AKSIMD_STORE_V8F32(ppfOutBuf[uChanIter + 0], AKSIMD_MUL_V8F32(vfResult[0], vfScaleInt16));
			AKSIMD_STORE_V8F32(ppfOutBuf[uChanIter + 1], AKSIMD_MUL_V8F32(vfResult[1], vfScaleInt16));
		}

		// Advance everything, now that writes are complete
		for (AkUInt32 uChanIter = 0; uChanIter < uNumChannels; uChanIter++)
		{
			ppfOutBuf[uChanIter] += FramesPerIter;
		}

		uIndexFP += uFrameSkipFP * FramesPerIter;
		viIndexFP = AKSIMD_ADD_V8I32(viIndexFP, AKSIMD_SET_V8I32(uFrameSkipFP * FramesPerIter));
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
