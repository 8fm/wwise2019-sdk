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

#include "AkStereoDelayLine.h"
#include <AK/SoundEngine/Common/AkSimd.h>
#include <AK/Tools/Common/AkAssert.h>
#include <AK/Plugin/PluginServices/AkVectorValueRamp.h>
#include <math.h>

namespace AK
{
	namespace DSP
	{		

		AKRESULT CStereoDelayLine::Init( 
			AK::IAkPluginMemAlloc * in_pAllocator, 
			AkReal32 in_fDelayTimes[2], 
			AkUInt32 in_uSampleRate )
		{
			AKASSERT( in_fDelayTimes[0] >= 0.f && in_fDelayTimes[1] >= 0.f );
			m_uSampleRate = in_uSampleRate;
			AkUInt32 uDelayLength = (AkUInt32)floor(in_fDelayTimes[0]*in_uSampleRate);
			AKRESULT eResult = m_DelayLines[0].Init( in_pAllocator, uDelayLength, 1 );
			if ( eResult == AK_Success )
			{
				uDelayLength = (AkUInt32)floor(in_fDelayTimes[1]*in_uSampleRate);
				eResult = m_DelayLines[1].Init( in_pAllocator, uDelayLength, 1 );
			}

			m_FeedbackFilter.Init(in_pAllocator, 2);

			return eResult;
		}

		void CStereoDelayLine::Term( AK::IAkPluginMemAlloc * in_pAllocator )
		{
			m_DelayLines[0].Term( in_pAllocator );
			m_DelayLines[1].Term( in_pAllocator );
			m_FeedbackFilter.Term(in_pAllocator);
		}

		void CStereoDelayLine::Reset( )
		{
			m_DelayLines[0].Reset( );
			m_DelayLines[1].Reset( );
			m_FeedbackFilter.Reset( );			
		}

		void CStereoDelayLine::ProcessBuffer(	
			AkAudioBuffer * in_pBuffer,					
			AkAudioBuffer * out_pBuffer,
			AkStereoDelayChannelParams in_PrevStereoDelayParams[2],
			AkStereoDelayChannelParams in_CurStereoDelayParams[2],
			AkStereoDelayFilterParams & in_FilterParams,
			bool in_bRecomputeFilterParams
			)
		{
			// Note: This algorithm assumes that the delay lines are at least as long as one buffer.
			// Shorter delays would require special handling to ensure that the feedback is considered properly
			// This assumption means that delay lines will not wrap more than once (each)
			const AkUInt32 uNumFrames = in_pBuffer->uValidFrames;
			AKASSERT( in_pBuffer->NumChannels() == 2 );
			AKASSERT( uNumFrames <= m_DelayLines[0].GetDelayLength() && uNumFrames <= m_DelayLines[1].GetDelayLength() );
			AKASSERT( (uNumFrames % 4) == 0 );	// must be a multiple of 4 for SIMD

			// Figure out wrapping delay lines
			const AkUInt32 uDelayLengthL = m_DelayLines[0].GetDelayLength();
			const AkUInt32 uDelayLengthR = m_DelayLines[1].GetDelayLength();

			AkUInt32 uCurOffsetL = m_DelayLines[0].GetCurrentOffset();
			AkUInt32 uCurOffsetR = m_DelayLines[1].GetCurrentOffset();
			AkUInt32 uFramesBeforeWrapL = uDelayLengthL - uCurOffsetL;					
			AkUInt32 uFramesBeforeWrapR = uDelayLengthR - uCurOffsetR;
			AkUInt32 uFramesBeforeWrap = AkMin( uFramesBeforeWrapL, uFramesBeforeWrapR );
			AKASSERT( (uFramesBeforeWrap % 4) == 0 );	// must be a multiple of 4 for SIMD
			
#ifdef AKSIMD_V2F32_SUPPORTED
			AKSIMD_V2F32 * AK_RESTRICT pvDelayPtrL = (AKSIMD_V2F32 * ) m_DelayLines[0].GetCurrentPointer(uCurOffsetL,0);
			AKSIMD_V2F32 * AK_RESTRICT pvDelayPtrR = (AKSIMD_V2F32 * ) m_DelayLines[1].GetCurrentPointer(uCurOffsetR,0);
#else
			AKSIMD_V4F32 * AK_RESTRICT pvDelayPtrL = (AKSIMD_V4F32 * ) m_DelayLines[0].GetCurrentPointer(uCurOffsetL,0);
			AKSIMD_V4F32 * AK_RESTRICT pvDelayPtrR = (AKSIMD_V4F32 * ) m_DelayLines[1].GetCurrentPointer(uCurOffsetR,0);
#endif

#ifdef AKSIMD_V2F32_SUPPORTED
	#define VECTOR_SIZE (2)
			// I/O pointers
			AKSIMD_V2F32 * AK_RESTRICT pvInPtrL = (AKSIMD_V2F32 * ) in_pBuffer->GetChannel(0);
			AKSIMD_V2F32 * AK_RESTRICT pvOutPtrL = (AKSIMD_V2F32 * ) out_pBuffer->GetChannel(0);
			AKSIMD_V2F32 * AK_RESTRICT pvInPtrR = (AKSIMD_V2F32 * ) in_pBuffer->GetChannel(1);
			AKSIMD_V2F32 * AK_RESTRICT pvOutPtrR = (AKSIMD_V2F32 * ) out_pBuffer->GetChannel(1);
			// Gain ramps
			CAkVectorValueRampV2 FeedbackRampL, FeedbackRampR, CrossFeedRampL, CrossFeedRampR;
			AKSIMD_V2F32 vFeedbackLGain = FeedbackRampL.Setup(in_PrevStereoDelayParams[0].fFeedback,in_CurStereoDelayParams[0].fFeedback,uNumFrames);
			AKSIMD_V2F32 vFeedbackRGain = FeedbackRampR.Setup(in_PrevStereoDelayParams[1].fFeedback,in_CurStereoDelayParams[1].fFeedback,uNumFrames);
			AKSIMD_V2F32 vCrossFeedLGain = CrossFeedRampL.Setup(in_PrevStereoDelayParams[0].fCrossFeed,in_CurStereoDelayParams[0].fCrossFeed,uNumFrames);
			AKSIMD_V2F32 vCrossFeedRGain = CrossFeedRampR.Setup(in_PrevStereoDelayParams[1].fCrossFeed,in_CurStereoDelayParams[1].fCrossFeed,uNumFrames);
#else
	#define VECTOR_SIZE (4)
			// I/O pointers
			AKSIMD_V4F32 * AK_RESTRICT pvInPtrL = (AKSIMD_V4F32 * ) in_pBuffer->GetChannel(0);
			AKSIMD_V4F32 * AK_RESTRICT pvOutPtrL = (AKSIMD_V4F32 * ) out_pBuffer->GetChannel(0);
			AKSIMD_V4F32 * AK_RESTRICT pvInPtrR = (AKSIMD_V4F32 * ) in_pBuffer->GetChannel(1);
			AKSIMD_V4F32 * AK_RESTRICT pvOutPtrR = (AKSIMD_V4F32 * ) out_pBuffer->GetChannel(1);
			// Gain ramps
			CAkVectorValueRamp FeedbackRampL, FeedbackRampR, CrossFeedRampL, CrossFeedRampR;
			AKSIMD_V4F32 vFeedbackLGain = FeedbackRampL.Setup(in_PrevStereoDelayParams[0].fFeedback,in_CurStereoDelayParams[0].fFeedback,uNumFrames);
			AKSIMD_V4F32 vFeedbackRGain = FeedbackRampR.Setup(in_PrevStereoDelayParams[1].fFeedback,in_CurStereoDelayParams[1].fFeedback,uNumFrames);
			AKSIMD_V4F32 vCrossFeedLGain = CrossFeedRampL.Setup(in_PrevStereoDelayParams[0].fCrossFeed,in_CurStereoDelayParams[0].fCrossFeed,uNumFrames);
			AKSIMD_V4F32 vCrossFeedRGain = CrossFeedRampR.Setup(in_PrevStereoDelayParams[1].fCrossFeed,in_CurStereoDelayParams[1].fCrossFeed,uNumFrames);
#endif

			// Minimum number of wraps to avoid branch in computation loop
			// Won't wrap
			AkUInt32 uFramesRemainingToProcess = uNumFrames;
			while ( uFramesRemainingToProcess )
			{
				AkUInt32 uFramesToProcess = AkMin(uFramesRemainingToProcess,uFramesBeforeWrap);
				AKASSERT( (uFramesToProcess % VECTOR_SIZE) == 0 );	// must be a multiple of 4 for SIMD

				// Dequeue delay lines, copy to output and mix input, feedback, and crossfeed together in vectorized loop
				for (AkUInt32 uVectors = 0; uVectors < uFramesToProcess/VECTOR_SIZE ; uVectors++)
				{
#ifdef AKSIMD_V2F32_SUPPORTED
					AKSIMD_V2F32 vDelayL = AKSIMD_LOAD_V2F32_OFFSET((AkReal32*) pvDelayPtrL, 0);
					++pvDelayPtrL;
					AKSIMD_V2F32 vDelayR = AKSIMD_LOAD_V2F32_OFFSET((AkReal32*) pvDelayPtrR, 0);
					++pvDelayPtrR;
					AKSIMD_V2F32 vInL = AKSIMD_LOAD_V2F32_OFFSET((AkReal32*) pvInPtrL, 0);	
					AKSIMD_V2F32 vInR = AKSIMD_LOAD_V2F32_OFFSET((AkReal32*) pvInPtrR, 0);
					AKSIMD_V2F32 vScaledDelayL = AKSIMD_MUL_V2F32( vDelayL, vFeedbackLGain );
					AKSIMD_V2F32 vScaledDelayR = AKSIMD_MUL_V2F32( vDelayR, vFeedbackRGain );
					AKSIMD_STORE_V2F32_OFFSET((AkReal32*) pvOutPtrL, 0, vDelayL);
					++pvOutPtrL;
					AKSIMD_STORE_V2F32_OFFSET((AkReal32*) pvOutPtrR, 0, vDelayR);	
					++pvOutPtrR;
					AKSIMD_V2F32 vFeedbackL = AKSIMD_ADD_V2F32(	vScaledDelayL, AKSIMD_MADD_V2F32( vDelayR, vCrossFeedRGain, vInL ) );
					AKSIMD_V2F32 vFeedbackR = AKSIMD_ADD_V2F32(	vScaledDelayR, AKSIMD_MADD_V2F32( vDelayL, vCrossFeedLGain, vInR ) ); 
					AKSIMD_STORE_V2F32_OFFSET((AkReal32*) pvInPtrL, 0, vFeedbackL);
					++pvInPtrL;
					AKSIMD_STORE_V2F32_OFFSET((AkReal32*) pvInPtrR, 0, vFeedbackR);	
					++pvInPtrR;
					vFeedbackLGain = FeedbackRampL.Tick();
					vFeedbackRGain = FeedbackRampR.Tick();
					vCrossFeedLGain = CrossFeedRampL.Tick();
					vCrossFeedRGain = CrossFeedRampR.Tick();
#else
					AKSIMD_V4F32 vDelayL = AKSIMD_LOAD_V4F32((AkReal32*) pvDelayPtrL);
					++pvDelayPtrL;
					AKSIMD_V4F32 vDelayR = AKSIMD_LOAD_V4F32((AkReal32*) pvDelayPtrR);
					++pvDelayPtrR;
					AKSIMD_V4F32 vInL = AKSIMD_LOAD_V4F32((AkReal32*) pvInPtrL);	
					AKSIMD_V4F32 vInR = AKSIMD_LOAD_V4F32((AkReal32*) pvInPtrR);
					AKSIMD_V4F32 vScaledDelayL = AKSIMD_MUL_V4F32( vDelayL, vFeedbackLGain );
					AKSIMD_V4F32 vScaledDelayR = AKSIMD_MUL_V4F32( vDelayR, vFeedbackRGain );
					AKSIMD_STORE_V4F32((AkReal32*) pvOutPtrL, vDelayL);
					++pvOutPtrL;
					AKSIMD_STORE_V4F32((AkReal32*) pvOutPtrR, vDelayR);	
					++pvOutPtrR;
					AKSIMD_V4F32 vFeedbackL = AKSIMD_ADD_V4F32(	vScaledDelayL, AKSIMD_MADD_V4F32( vDelayR, vCrossFeedRGain, vInL ) );
					AKSIMD_V4F32 vFeedbackR = AKSIMD_ADD_V4F32(	vScaledDelayR, AKSIMD_MADD_V4F32( vDelayL, vCrossFeedLGain, vInR ) ); 
					AKSIMD_STORE_V4F32((AkReal32*) pvInPtrL, vFeedbackL);
					++pvInPtrL;
					AKSIMD_STORE_V4F32((AkReal32*) pvInPtrR, vFeedbackR);	
					++pvInPtrR;
					vFeedbackLGain = FeedbackRampL.Tick();
					vFeedbackRGain = FeedbackRampR.Tick();
					vCrossFeedLGain = CrossFeedRampL.Tick();
					vCrossFeedRGain = CrossFeedRampR.Tick();
#endif
				}

				// Advance and wrap delay lines
				uCurOffsetL += uFramesToProcess;
				uCurOffsetR += uFramesToProcess;
				if ( uCurOffsetL == uDelayLengthL )
				{
#ifdef AKSIMD_V2F32_SUPPORTED
					pvDelayPtrL = (AKSIMD_V2F32 * ) m_DelayLines[0].GetCurrentPointer(0,0);
#else
					pvDelayPtrL = (AKSIMD_V4F32 * ) m_DelayLines[0].GetCurrentPointer(0,0);
#endif
					uCurOffsetL = 0;
				}
				if ( uCurOffsetR == uDelayLengthR )
				{
#ifdef AKSIMD_V2F32_SUPPORTED
					pvDelayPtrR = (AKSIMD_V2F32 * ) m_DelayLines[1].GetCurrentPointer(0,0);
#else
					pvDelayPtrR = (AKSIMD_V4F32 * ) m_DelayLines[1].GetCurrentPointer(0,0);
#endif
					uCurOffsetR = 0;
				}		
				AKASSERT( uCurOffsetL < uDelayLengthL );
				AKASSERT( uCurOffsetR < uDelayLengthR );

				uFramesRemainingToProcess -= uFramesToProcess;
				uFramesBeforeWrapL = uDelayLengthL - uCurOffsetL;
				uFramesBeforeWrapR = uDelayLengthR - uCurOffsetR;
				uFramesBeforeWrap = AkMin( uFramesBeforeWrapL, uFramesBeforeWrapR );
			}

			// Apply biquad filters inside feedback path (include input)
			AkReal32 * pfFeedbackL = in_pBuffer->GetChannel(0);
			AkReal32 * pfFeedbackR = in_pBuffer->GetChannel(1);	
			if ( in_FilterParams.eFilterType != AKFILTERTYPE_NONE )
			{
				if ( in_bRecomputeFilterParams )
				{
					m_FeedbackFilter.ComputeCoefs( (::DSP::FilterType)(in_FilterParams.eFilterType-1),
														(AkReal32)m_uSampleRate,
														in_FilterParams.fFilterFrequency,
														in_FilterParams.fFilterGain,
														in_FilterParams.fFilterQFactor );					
				}
				m_FeedbackFilter.ProcessBuffer( pfFeedbackL, uNumFrames, in_pBuffer->MaxFrames() );				
			}
			
			// Retrieve original offset (before we incremented in processing loops)
			uCurOffsetL = m_DelayLines[0].GetCurrentOffset();
			uCurOffsetR = m_DelayLines[1].GetCurrentOffset();
			uFramesBeforeWrapL = uDelayLengthL - uCurOffsetL;
			uFramesBeforeWrapR = uDelayLengthR - uCurOffsetR;
			// Enqueue new inputs to both delay lines
			AKPLATFORM::AkMemCpy( m_DelayLines[0].GetCurrentPointer(uCurOffsetL,0), pfFeedbackL, AkMin(uNumFrames,uFramesBeforeWrapL)*sizeof(AkReal32) );
			if ( uFramesBeforeWrapL < uNumFrames )
				AKPLATFORM::AkMemCpy( m_DelayLines[0].GetCurrentPointer(0,0), pfFeedbackL+uFramesBeforeWrapL, (uNumFrames-uFramesBeforeWrapL)*sizeof(AkReal32) );
			AKPLATFORM::AkMemCpy( m_DelayLines[1].GetCurrentPointer(uCurOffsetR,0), pfFeedbackR, AkMin(uNumFrames,uFramesBeforeWrapR)*sizeof(AkReal32) );
			if ( uFramesBeforeWrapR < uNumFrames )
				AKPLATFORM::AkMemCpy( m_DelayLines[1].GetCurrentPointer(0,0), pfFeedbackR+uFramesBeforeWrapR, (uNumFrames-uFramesBeforeWrapR)*sizeof(AkReal32) );
			m_DelayLines[0].SetCurrentOffset( (uCurOffsetL + uNumFrames) % uDelayLengthL );
			m_DelayLines[1].SetCurrentOffset( (uCurOffsetR + uNumFrames) % uDelayLengthR );
		}	

	} // namespace DSP
} // namespace AK
