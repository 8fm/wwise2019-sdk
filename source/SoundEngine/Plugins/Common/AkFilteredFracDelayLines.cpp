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

#include "AkFilteredFracDelayLines.h"
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/SoundEngine/Common/AkSimd.h>
#include <AK/Plugin/PluginServices/AkVectorValueRamp.h>

#include <AK/SoundEngine/Common/AkMemoryMgr.h>

#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/Tools/Common/AkObject.h>
//#include <AK/Tools/Common/AkListBare.h>
#include <stdio.h>

#include "AkMath.h"

//#define _AK_SS_SIMD_ 1
#define _AK_SCALAR_ 1

// Ensure that none of these values are negative. Rounded first and last coefficents to 0 (from e-19)
const AkReal32 CAkFilteredFracDelayLines::m_paramFIRCoeffs[] = { 0.0, 2.07800094833e-06, 8.57549859582e-06, 1.99237271501e-05,
3.66054614293e-05, 5.91586599145e-05, 8.81790456937e-05, 0.000124321513639, 0.000168300268937, 0.000220887620768, 0.000282911375289, 0.000355250793762,
0.000438831104452, 0.000534616580388, 0.00064360221891, 0.00076680408282, 0.000905248386402, 0.00105995943239, 0.00123194652761, 0.0014221900254,
0.00163162666118, 0.00186113436437, 0.00211151674349, 0.00238348745324, 0.00267765466066, 0.00299450583347, 0.00333439307607, 0.00369751923818, 0.00408392501725,
0.00449347726848, 0.00492585872622, 0.00538055932688, 0.00585686930733, 0.00635387423339, 0.00687045209167, 0.00740527255381, 0.00795679849645, 0.00852328983278,
0.00910280968267, 0.00969323287893, 0.010292256777, 0.0108974143053, 0.0115060891638, 0.0121155330495, 0.0127228847586, 0.0133251909912, 0.0139194286578,
0.0145025284654, 0.0150713995433, 0.0156229548504, 0.0161541370928, 0.0166619448712, 0.017143458772, 0.0175958671104, 0.0180164910387, 0.0184028087334,
0.0187524783874, 0.0190633597426, 0.019333533913, 0.0195613212711, 0.0197452971868, 0.0198843054365, 0.0199774691241, 0.0200241989868, 0.0200241989868,
0.0199774691241, 0.0198843054365, 0.0197452971868, 0.0195613212711, 0.019333533913, 0.0190633597426, 0.0187524783874, 0.0184028087334, 0.0180164910387,
0.0175958671104, 0.017143458772, 0.0166619448712, 0.0161541370928, 0.0156229548504, 0.0150713995433, 0.0145025284654, 0.0139194286578, 0.0133251909912,
0.0127228847586, 0.0121155330495, 0.0115060891638, 0.0108974143053, 0.010292256777, 0.00969323287893, 0.00910280968267, 0.00852328983278, 0.00795679849645,
0.00740527255381, 0.00687045209167, 0.00635387423339, 0.00585686930733, 0.00538055932688, 0.00492585872622, 0.00449347726848, 0.00408392501725, 0.00369751923818,
0.00333439307607, 0.00299450583347, 0.00267765466066, 0.00238348745324, 0.00211151674349, 0.00186113436437, 0.00163162666118, 0.0014221900254, 0.00123194652761,
0.00105995943239, 0.000905248386402, 0.00076680408282, 0.00064360221891, 0.000534616580388, 0.000438831104452, 0.000355250793762, 0.000282911375289, 0.000220887620768,
0.000168300268937, 0.000124321513639, 8.81790456937e-05, 5.91586599145e-05, 3.66054614293e-05, 1.99237271501e-05, 8.57549859582e-06, 2.07800094833e-06, 0.0 };

void CAkFilteredFracDelayLines::Init(AK::IAkPluginMemAlloc* in_pAllocator, AkUInt16 in_uMaxBufferLength)
{
	AKASSERT(in_pAllocator);

	m_uMaxBufferLength = in_uMaxBufferLength;
	m_pAllocator = in_pAllocator;
}

void CAkFilteredFracDelayLines::Term()
{
	m_DelayMem.Term(m_pAllocator);
}

void CAkFilteredFracDelayLines::Reset()
{
	m_DelayMem.Reset();
}

CAkFilteredFracDelayLines::Tap * CAkFilteredFracDelayLines::RegisterTap(AkReal32 delay)
{
	return m_DelayMem.RegisterTap(m_pAllocator, delay);
}

void CAkFilteredFracDelayLines::UnregisterTap(CAkFilteredFracDelayLines::Tap & in_tap)
{
	m_DelayMem.UnregisterTap(m_pAllocator, in_tap);
}

AKRESULT CAkFilteredFracDelayLines::Resize(AkReal32 in_fTargetDelayLen)
{
	AKRESULT res = AK_Success;

	AkReal32 targetDelayLen = in_fTargetDelayLen;

	AkUInt32 uMaxDelayLength = m_DelayMem.GetMaxDelayLength();
	
	AkReal32 fceilTargetDelayLen = ceilf(targetDelayLen);
	AkUInt32 uceilTargetDelayLen = (AkUInt32)fceilTargetDelayLen;

	if ((fceilTargetDelayLen + m_uMaxBufferLength) > AK_DELAYLINE_MAX_ALLOC)
		uceilTargetDelayLen = AK_DELAYLINE_MAX_ALLOC - m_uMaxBufferLength;

	if ((uceilTargetDelayLen + m_uMaxBufferLength) <= uMaxDelayLength)
		return res;

	// keep some provision in the delay line so that read/write offset do not overlap
	res = m_DelayMem.Realloc(m_pAllocator, (uceilTargetDelayLen + m_uMaxBufferLength));

	return res;
}

void CAkFilteredFracDelayLines::RefillDelayLine(AkSampleType * io_pfBuffer, AkSampleType * in_pfBuffer, AkUInt32 in_uNumFrames)
{
	AKASSERT(io_pfBuffer != NULL);
	AKPLATFORM::AkMemCpy(io_pfBuffer, in_pfBuffer, sizeof(AkSampleType)* in_uNumFrames);
}

void CAkFilteredFracDelayLines::Mix(AkSampleType * io_pfBuffer, AkSampleType * in_pfBuffer, AkUInt32 in_uNumFrames)
{
	AKASSERT(io_pfBuffer != NULL);
	for (AkUInt32 i = 0; i < in_uNumFrames; i++)
		io_pfBuffer[i] += (in_pfBuffer[i]);
}

AKRESULT CAkFilteredFracDelayLines::ProcessWrite(AkSampleType * in_pBuffer, AkUInt16 in_uNumFrames, bool bPurging)
{
	if (!m_DelayMem.IsValid())
	{
		// This delay line isn't ready, perhaps we ran out of memory, or invalid reader index, or no tap was registered.
		return AK_Fail;
	}
	AKRESULT res = AK_Success;

	AkUInt32 uWritePos = 0;
	AkReal32 *pfDelayRW = NULL;

	uWritePos = m_DelayMem.GetCurrentWriteOffset();
	pfDelayRW = (AkReal32 *)m_DelayMem.GetCurrentPointer(uWritePos);
	AKASSERT(pfDelayRW != NULL);

	AkUInt32 uMaxDelayLength = m_DelayMem.GetMaxDelayLength();

	if ((uWritePos + in_uNumFrames) <= uMaxDelayLength)
	{
		if ( bPurging )
		{
			AKPLATFORM::AkMemSet( pfDelayRW, 0, in_uNumFrames * sizeof(AkReal32) );
		}
		else
		{
			RefillDelayLine( pfDelayRW, in_pBuffer, in_uNumFrames );
		}
	}
	else
	{
		if ( bPurging )
		{
			AkUInt32 preRingFrames = (uMaxDelayLength - uWritePos);

			AKPLATFORM::AkMemSet( pfDelayRW, 0, preRingFrames * sizeof( AkReal32 ) );

			pfDelayRW = (AkReal32 *)m_DelayMem.GetCurrentPointer( 0 );
			AKPLATFORM::AkMemSet( pfDelayRW, 0, (in_uNumFrames - preRingFrames) * sizeof( AkReal32 ) );
		}
		else
		{
			AkUInt32 preRingFrames = (uMaxDelayLength - uWritePos);

			RefillDelayLine( pfDelayRW, in_pBuffer, preRingFrames );

			pfDelayRW = (AkReal32 *)m_DelayMem.GetCurrentPointer( 0 );
			RefillDelayLine( pfDelayRW, in_pBuffer + preRingFrames, (in_uNumFrames - preRingFrames) );
		}		
	}

	m_DelayMem.SetCurrentWriteOffset((uWritePos + in_uNumFrames) % uMaxDelayLength);

	return res;
}

void CAkFilteredFracDelayLines::EvaluateFIRParams( AkReal32& io_fParamFilterCutoff )
{
	// Minimum cutoff
	AkReal32 fDecimateFactor = 1.f;
	// 0
	if ( io_fParamFilterCutoff < 0.01f )
	{
		// Maximum cutoff - We don't need to update the length because this will bypass
		io_fParamFilterCutoff = 0.f;
		fDecimateFactor = (AkReal32)AK_PARAM_FIR_ORDER;
	}
	// 1
	else if ( io_fParamFilterCutoff > 0.99f )
	{
		// Minimum cutoff - don't resample
		io_fParamFilterCutoff = 1.f;
		m_FIRparamFilterLen = AK_PARAM_FIR_ORDER; // Update the filter length back to maximum
		AKPLATFORM::AkMemCpy( m_resampledParamFIRCoeffs, m_paramFIRCoeffs, sizeof( AkReal32 ) * AK_PARAM_FIR_ORDER );
	}
	else
	{
		// Clamp the cutoff to the useful values;
		io_fParamFilterCutoff = AkMath::Max( io_fParamFilterCutoff, 0.03f );

		fDecimateFactor = 1.f / io_fParamFilterCutoff;
		AkUInt32 newFIRparamLen = (AkUInt32)ceil( (AkReal32)AK_PARAM_FIR_ORDER / fDecimateFactor );

		if ( newFIRparamLen != m_FIRparamFilterLen )
		{
			m_FIRparamFilterLen = newFIRparamLen;
			decimateFIRCoeffs( m_paramFIRCoeffs, m_resampledParamFIRCoeffs, m_FIRparamFilterLen, fDecimateFactor );
		}
	}
}
void CAkFilteredFracDelayLines::EvaluateIIRParams( AkReal32& io_fParamFilterCutoff, AkUInt16 in_uNumFrames, AkUInt32 in_uSampleRate )
{
	if ( io_fParamFilterCutoff == 0.f || io_fParamFilterCutoff == 1.f )
	{
		io_fParamFilterCutoff = AkMath::Min( 0.99, io_fParamFilterCutoff );
		m_fParamFilterb0 = 1.f - io_fParamFilterCutoff;
	}
	else
	{

		// Clamp the cutoff to non negative values;
		io_fParamFilterCutoff = AkMath::Max( io_fParamFilterCutoff, 0.01f );

		// Maximum decay time - 5 seconds
		// This is a reflected exponential decay ramp, so that 0 is the maximum amount of decay
		static const AkReal32 fMaxIIRDecay = 5.f;
		static const AkReal32 paramFs = (AkReal32)in_uSampleRate / (AkReal32)in_uNumFrames;
		m_fParamFilterb0 = 1.f - AkMath::FastExp( -1.f / (fMaxIIRDecay * io_fParamFilterCutoff *  paramFs) );
	}
}

AkReal32 CAkFilteredFracDelayLines::FilterParams(
	AkReal32 in_fParamFilterCutoff,
	const AkReal32 in_fTargetDelayLen,
	AkUInt32 in_uParamFilterIsFIR,
	CAkFilteredFracDelayLines::Tap & in_tap)
{
	AkReal32 prevDelayTarget = in_tap.paramMemoryIIR;

	AkReal32 *paramMemory = in_tap.paramMemoryFIR;

	AkReal32 targetDelayLen = 0.f;

	// Bypass param filtering
	if ( in_fParamFilterCutoff == 0.0f )
	{
		// Pass through
		targetDelayLen = in_fTargetDelayLen;

		// Maintain the parameter filter through bypass
		if (in_uParamFilterIsFIR)
		{
			if ( in_fParamFilterCutoff == 0.0f )
			{
				if ( m_FIRparamFilterLen >= 2 )
				{
					for ( AkUInt32 i = m_FIRparamFilterLen - 2; i > 0; --i )
					{
						paramMemory[i] = paramMemory[i - 1];
					}
				}
				paramMemory[0] = targetDelayLen;
			}
		}
		else
		{
			// IIR
			in_tap.paramMemoryIIR = targetDelayLen;
		}
	}
	// Filter even if muted for proper threshold detection
	else
	{
		// FIR
		if (in_uParamFilterIsFIR)
		{
			// Update the IIR memory as well - param memory is updated internally
			targetDelayLen = paramLowPassFilter2( in_fTargetDelayLen, paramMemory );
		}
		// IIR
		else
		{
			// Upate the FIR memory as well
			targetDelayLen = paramLowPassFilter( in_fTargetDelayLen, prevDelayTarget );

			// Update the prev delay target 
			in_tap.paramMemoryIIR = prevDelayTarget;
		}
	}

	return targetDelayLen;
}


AKRESULT CAkFilteredFracDelayLines::ProcessRead(
	AkAudioBuffer* out_pBuffer,
	AkUInt16 in_uNumFrames,
	AkUInt32 in_uSampleRate,
	AkReal32 in_fTargetDelayLen,
	bool bPurging,
	CAkFilteredFracDelayLines::Tap & in_tap,
	AkFilteredFracDelayLineParams* in_FracDelayParams)
{
	if ( !m_DelayMem.IsValid() )
	{
		// This delay line isn't ready, perhaps we ran out of memory, or invalid reader index.
		return AK_Fail;
	}

	AKRESULT res = AK_Success;

	// Mono only atm
	AKASSERT(out_pBuffer->GetChannelConfig().uNumChannels == 1);
	AkSampleType* out = out_pBuffer->GetChannel(0);

	AkReal32 paramCutoff = in_FracDelayParams->fParamFilterCutoff;
	/* --------------------------------------FIR PARAMETERS------------------------------------- */
	if (in_FracDelayParams->uParamFilterIsFIR)
	{
		EvaluateFIRParams( paramCutoff );
	}
	/* --------------------------------------IIR PARAMETERS------------------------------------- */
	else
	{
		EvaluateIIRParams( paramCutoff, in_uNumFrames, in_uSampleRate );
	}

	/* -------------------------------------- Param Filtering ------------------------------------- */
	AkReal32 targetDelayLen = FilterParams( paramCutoff, in_fTargetDelayLen, in_FracDelayParams->uParamFilterIsFIR, in_tap );
	
	in_tap.currentSmoothDistance = (targetDelayLen / (AkReal32)in_uSampleRate) * in_FracDelayParams->fSpeedOfSound;

	/* -------------------------------------- Growth Ramp ----------------------------------------- */

	AkReal32 currentDelay = in_tap.currentDelay;

	// Set the growth size over 1 frame

	// To prevent infinite percent-wise sliding of the IIR, stop if we are relatively close
	// An absolute difference here is bad when dealing with larger maginitude units like cm
	if ( AkMath::Abs( 1.f - (currentDelay/targetDelayLen) ) > 1e-7f )
	{
		in_tap.growthSizeFrame = targetDelayLen - currentDelay;
	}
	else
	{
		// stop infinite percentwise sliding when stationary
		in_tap.growthSizeFrame = 0.0f;
	}

	/* -------------------------------------- Purging ----------------------------------------------*/
	
	// current slew increment
	AkReal32 fDelayGrowthSizeFrame = in_tap.growthSizeFrame;

	if ( bPurging )
	{
		AkReal32 remainingDelay = in_tap.purgeDelay;
		bool isPurging = in_tap.isPurging;
		if ( !isPurging )
		{			
			remainingDelay = currentDelay;
			in_tap.purgeDelay = remainingDelay;
			in_tap.isPurging = true;
		}
		// If we reach exactly the remaining delay then we will be done next frame
		if ( in_uNumFrames - fDelayGrowthSizeFrame > remainingDelay )
		{
			// Clamp the maximum frame size to one frame, in case the remaining delay is > uNumFrames
			in_uNumFrames = AkMin((AkUInt16)ceil( remainingDelay ), in_uNumFrames);
			res = AK_NoMoreData;
			in_tap.purgeDelay = 0.0f;
			in_tap.isPurging = false;
		}
		else
		{
			remainingDelay -= in_uNumFrames - fDelayGrowthSizeFrame;
			in_tap.purgeDelay = remainingDelay;
		}
		// The write pointer is offset by processWrite
	}
	else
	{
		// if purging is ever stopped, reset PurgeDelay
		in_tap.purgeDelay = 0.0f;
		in_tap.isPurging = false;
	}

	AkUInt32 uMaxDelayLength = m_DelayMem.GetMaxDelayLength();

	AKASSERT( uMaxDelayLength > 0 && uMaxDelayLength >= in_uNumFrames );

	// If the resize is checked properly, than just check the delay time isn't too big
	// The subtraction can not be < 0 because uMaxDelayLength is at least uNumFrames
	AKASSERT( targetDelayLen >= 0 && targetDelayLen <= (uMaxDelayLength - in_uNumFrames) );

	in_tap.currentPitch = 0.f;
	in_tap.currentDisplacement = 0.f;

	/* --------------------------------------Step mode--------------------------------------------*/
	if (in_FracDelayParams->uThresholdMode )
	{
		// Crossfade on every movement
		if (in_FracDelayParams->fDistanceThreshold == 0.f )
		{
			if ( AkMath::Abs( fDelayGrowthSizeFrame ) > 0.f )
			{
				// Crossfade
				currentDelay = ProcessCrossfadeDelayRead( out, in_uNumFrames, targetDelayLen, currentDelay);
			}
			else
			{
				// stationary
				currentDelay = ProcessStationaryDelayRead( out, in_uNumFrames, targetDelayLen);
			}
		}
		// Displacement threshold
		else
		{
			AkReal32 fAccumDelay = in_tap.accumulatedDelay;
			
			AkReal32 fGrowthMeters = AkMath::Abs( fAccumDelay ) / (AkReal32)in_uSampleRate * in_FracDelayParams->fSpeedOfSound;
			in_tap.currentDisplacement = fGrowthMeters;

			if ( fGrowthMeters > in_FracDelayParams->fDistanceThreshold )
			{
				// Threshhold decision making
				//don't move until the difference is THRESH big, then duck in one frame to new location, reset difference accumulation

				currentDelay = ProcessCrossDuckingDelayRead( out, in_uNumFrames, targetDelayLen, currentDelay);
				in_tap.accumulatedDelay = 0.f;
			}
			else
			{
				// stationary
				// Save the accumulated delay
				in_tap.accumulatedDelay = fDelayGrowthSizeFrame;

				currentDelay = ProcessStationaryDelayRead( out, in_uNumFrames, currentDelay);
			}
		}
	}
	/* --------------------------------------Continuous mode--------------------------------------------*/
	else
	{
		// Volume Ducking
		if (in_FracDelayParams->fPitchThreshold == 0.f )
		{
			// Detect any movement whatsoever
			if ( AkMath::Abs( currentDelay - targetDelayLen ) > 0.f )
			{
				if (in_tap.isMuted)
				{
					// silence
					currentDelay = targetDelayLen;
					AKPLATFORM::AkMemSet( out, 0, sizeof( AkSampleType ) * in_uNumFrames );
				}
				else
				{
					// fade out
					currentDelay = ProcessVolumeDuckingDelayRead( out, in_uNumFrames, targetDelayLen, currentDelay, true );
					in_tap.isMuted = true;
				}
			}
			else
			{
				if (in_tap.isMuted)
				{
					// fade in
					currentDelay = ProcessVolumeDuckingDelayRead( out, in_uNumFrames, targetDelayLen, currentDelay, false );
					in_tap.isMuted = false;
				}
				else
				{
					// stationary
					currentDelay = ProcessStationaryDelayRead( out, in_uNumFrames, targetDelayLen);
				}
			}
		}
		// Linear interp
		else if (in_FracDelayParams->fPitchThreshold >= 9600.f )
		{
			// Store the pitch for display
			AkReal32 fDelayGrowthSizeInc = fDelayGrowthSizeFrame / (AkReal32)in_uNumFrames;
			AkReal32 fFactor = AkMath::Abs(1.f - fDelayGrowthSizeInc);
			AkReal32 fPitch = AK_FACTOR2PITCH( fFactor );
			fPitch = fDelayGrowthSizeInc > 1.f ? -fPitch : fPitch;
			in_tap.currentPitch = fPitch;

			if ( AkMath::Abs( fDelayGrowthSizeFrame ) > 0.f )
			{
				// Doppler
				currentDelay = ProcessVariableDelayRead( out, in_uNumFrames, targetDelayLen, currentDelay, fDelayGrowthSizeFrame);
			}
			else
			{
				// stationary
				currentDelay = ProcessStationaryDelayRead( out, in_uNumFrames, targetDelayLen);
			}
		}
		// Velocity threshold
		else
		{
			// Volume Ducking
			AkReal32 fDelayGrowthSizeInc = fDelayGrowthSizeFrame / (AkReal32)in_uNumFrames;
			AkReal32 fFactor = AkMath::Abs( 1.f - fDelayGrowthSizeInc );
			AkReal32 fPitch = AK_FACTOR2PITCH( fFactor );
			fPitch = fDelayGrowthSizeInc > 1.f ? -fPitch : fPitch;
			in_tap.currentPitch = fPitch;

			if ( AkMath::Abs(fPitch) > in_FracDelayParams->fPitchThreshold )
			{
				// Threshhold decision making
				// stay in doppler / cross mode change rate is THRESH big, then duck in one frame, remain silent until a future frame where change rate is smaller again
				if (in_tap.isMuted)
				{
					// silence
					currentDelay = targetDelayLen;
					AKPLATFORM::AkMemSet( out, 0, sizeof( AkSampleType ) * in_uNumFrames );
				}
				else
				{
					// fade out
					currentDelay = ProcessVolumeDuckingDelayRead( out, in_uNumFrames, targetDelayLen, currentDelay, true );
					in_tap.isMuted = true;
				}
			}
			else
			{
				if (in_tap.isMuted)
				{
					// fade in
					currentDelay = ProcessVolumeDuckingDelayRead( out, in_uNumFrames, targetDelayLen, currentDelay, false );
					in_tap.isMuted = false;
				}
				else
				{
					// default to doppler
					if ( AkMath::Abs( fDelayGrowthSizeFrame ) > 0.f )
					{
						// Doppler
						currentDelay = ProcessVariableDelayRead( out, in_uNumFrames, targetDelayLen, currentDelay, fDelayGrowthSizeFrame);
					}
					else
					{
						// stationary
						currentDelay = ProcessStationaryDelayRead( out, in_uNumFrames, targetDelayLen);
					}
				}
			}
		}
	}

	// Check the delay is not < 0 or > MAX
	AKASSERT( currentDelay >= 0 && currentDelay <= (uMaxDelayLength - in_uNumFrames) );
	in_tap.currentDelay = currentDelay;


	if (res == AK_InsufficientMemory)
	{
		Term();
		return res;
	}

	return res;
}

AkReal32 CAkFilteredFracDelayLines::ProcessCrossDuckingDelayRead(
	AkSampleType* out_pBuffer,
	AkUInt16 in_uNumFrames,
	const AkReal32 in_fTargetDelayLen,
	AkReal32 currentDelay
	)
{
	AkUInt32 uMaxDelayLength = m_DelayMem.GetMaxDelayLength();
	AkUInt32 uNumFrames = in_uNumFrames;

	// ----------------------------------------------------------

	// memory pointer and write offset
	AkReal32 *pfDelayR = (AkReal32 *)m_DelayMem.GetCurrentPointer( 0 );
	AkReal32 fWritePos = (AkReal32)m_DelayMem.GetCurrentWriteOffset();

	AkReal32 fIndex = (fWritePos - uNumFrames) - currentDelay;
	// Do the round up before wrapping
	fIndex += 0.5;

	AK_FPSetValLT( fIndex, 0.f, fIndex, fIndex + uMaxDelayLength );
	AK_FPSetValGTE( fIndex, (AkReal32)uMaxDelayLength, fIndex, fIndex - uMaxDelayLength );

	AkUInt32 iIndex = (AkUInt32)floor( fIndex );

	AKASSERT( iIndex >= 0 );
	AKASSERT( iIndex < uMaxDelayLength );

	AkUInt32 uHalfFrame = uNumFrames / 2;

	if (iIndex + uHalfFrame < uMaxDelayLength)
	{
		// 1 loop
		for (AkUInt32 i = 0; i < uHalfFrame; i++)
		{
			AkReal32 sample = pfDelayR[iIndex];
			// ramp down
			AkReal32 mixValue = (AkReal32)i / (AkReal32)(uHalfFrame - 1);
			out_pBuffer[i] = (1.f - mixValue) * sample;

			iIndex++;
		}
	}
	else
	{
		// 2 loops
		AkUInt32 uStartFrame = uMaxDelayLength - iIndex;
		for (AkUInt32 i = 0; i < uStartFrame; i++)
		{
			AkReal32 sample = pfDelayR[iIndex];
			// ramp down
			AkReal32 mixValue = (AkReal32)i / (AkReal32)(uHalfFrame - 1);
			out_pBuffer[i] = (1.f - mixValue) * sample;

			iIndex++;
		}
		iIndex = 0;
		for (AkUInt32 i = uStartFrame; i < uHalfFrame; i++)
		{
			AkReal32 sample = pfDelayR[iIndex];
			// ramp down
			AkReal32 mixValue = (AkReal32)i / (AkReal32)(uHalfFrame - 1);
			out_pBuffer[i] = (1.f - mixValue) * sample;

			iIndex++;
		}
	}

	AKASSERT(iIndex <= uMaxDelayLength);

	
	fIndex = (fWritePos - uNumFrames/2) - in_fTargetDelayLen;

	// Do the round up before wrapping
	fIndex += 0.5;

	AK_FPSetValLT( fIndex, 0.f, fIndex, fIndex + uMaxDelayLength );
	AK_FPSetValGTE( fIndex, (AkReal32)uMaxDelayLength, fIndex, fIndex - uMaxDelayLength );

	iIndex = (AkUInt32)floor( fIndex );

	AKASSERT( iIndex >= 0 );
	AKASSERT( iIndex < uMaxDelayLength );

	if (iIndex + uHalfFrame < uMaxDelayLength)
	{
		// 1 loop
		for (AkUInt32 i = 0; i < uHalfFrame; i++)
		{
			AkUInt32 j = i + uHalfFrame;

			AkReal32 sample = pfDelayR[iIndex];
			// ramp down
			AkReal32 mixValue = (AkReal32)i / (AkReal32)(uHalfFrame - 1);
			out_pBuffer[j] = mixValue * sample;

			iIndex++;
		}
	}
	else
	{
		// 2 loops
		AkUInt32 uStartFrame = uMaxDelayLength - iIndex;
		for (AkUInt32 i = 0; i < uStartFrame; i++)
		{
			AkUInt32 j = i + uHalfFrame;

			AkReal32 sample = pfDelayR[iIndex];
			// ramp down
			AkReal32 mixValue = (AkReal32)i / (AkReal32)(uHalfFrame - 1);
			out_pBuffer[j] = mixValue * sample;

			iIndex++;
		}
		iIndex = 0;
		for (AkUInt32 i = uStartFrame; i < uHalfFrame; i++)
		{
			AkUInt32 j = i + uHalfFrame;

			AkReal32 sample = pfDelayR[iIndex];
			// ramp down
			AkReal32 mixValue = (AkReal32)i / (AkReal32)(uHalfFrame - 1);
			out_pBuffer[j] = mixValue * sample;

			iIndex++;
		}
	}

	AKASSERT(iIndex <= uMaxDelayLength);

	// ----------------------------------------------------------------------------------------------------------
	// Update new delay
	currentDelay = in_fTargetDelayLen;
	return currentDelay;
}

AkReal32 CAkFilteredFracDelayLines::ProcessVolumeDuckingDelayRead(
	AkSampleType* out_pBuffer,
	AkUInt16 in_uNumFrames,
	const AkReal32 in_fTargetDelayLen, 
	AkReal32 currentDelay,
	bool isMuting
	)
{
	AkUInt32 uMaxDelayLength = m_DelayMem.GetMaxDelayLength();
	AkUInt32 uNumFrames = in_uNumFrames;

	// ----------------------------------------------------------

	// memory pointer and write offset
	AkReal32 *pfDelayR = (AkReal32 *)m_DelayMem.GetCurrentPointer( 0 );
	AkReal32 fWritePos = (AkReal32)m_DelayMem.GetCurrentWriteOffset();

	AkReal32 delayOffset = 0.f;
	if ( isMuting )
	{
		delayOffset = currentDelay;
	}
	else
	{
		delayOffset = in_fTargetDelayLen;
	}

	AkReal32 fIndex = (fWritePos - uNumFrames) - delayOffset;
	// Do the round up before wrapping
	fIndex += 0.5;

	AK_FPSetValLT( fIndex, 0.f, fIndex, fIndex + uMaxDelayLength );
	AK_FPSetValGTE( fIndex, (AkReal32)uMaxDelayLength, fIndex, fIndex - uMaxDelayLength );

	AkUInt32 iIndex = (AkUInt32)floor( fIndex );

	AKASSERT( iIndex >= 0 );
	AKASSERT( iIndex < uMaxDelayLength );

	if (isMuting)
	{
		if (iIndex + uNumFrames < uMaxDelayLength)
		{
			// 1 loop
			for (AkUInt32 i = 0; i < uNumFrames; i++)
			{
				AkReal32 sample = pfDelayR[iIndex];

				AkReal32 mixValue = (AkReal32)i / (AkReal32)(uNumFrames - 1);

				// ramp down
				out_pBuffer[i] = (1.f - mixValue) * sample;

				iIndex++;
			}
		}
		else
		{
			// 2 loops
			AkUInt32 uStartFrame = uMaxDelayLength - iIndex;
			for (AkUInt32 i = 0; i < uStartFrame; i++)
			{
				AkReal32 sample = pfDelayR[iIndex];

				AkReal32 mixValue = (AkReal32)i / (AkReal32)(uNumFrames - 1);

				// ramp down
				out_pBuffer[i] = (1.f - mixValue) * sample;

				iIndex++;
			}
			iIndex = 0;
			for (AkUInt32 i = uStartFrame; i < uNumFrames; i++)
			{
				AkReal32 sample = pfDelayR[iIndex];

				AkReal32 mixValue = (AkReal32)i / (AkReal32)(uNumFrames - 1);

				// ramp down
				out_pBuffer[i] = (1.f - mixValue) * sample;

				iIndex++;
			}
		}
	}
	else
	{
		if (iIndex + uNumFrames < uMaxDelayLength)
		{
			// 1 loop
			for (AkUInt32 i = 0; i < uNumFrames; i++)
			{
				AkReal32 sample = pfDelayR[iIndex];

				AkReal32 mixValue = (AkReal32)i / (AkReal32)(uNumFrames - 1);

				// ramp up
				out_pBuffer[i] = (mixValue)* sample;

				iIndex++;
			}
		}
		else
		{
			// 2 loops
			AkUInt32 uStartFrame = uMaxDelayLength - iIndex;
			for (AkUInt32 i = 0; i < uStartFrame; i++)
			{
				AkReal32 sample = pfDelayR[iIndex];

				AkReal32 mixValue = (AkReal32)i / (AkReal32)(uNumFrames - 1);

				// ramp up
				out_pBuffer[i] = (mixValue)* sample;

				iIndex++;
			}
			iIndex = 0;
			for (AkUInt32 i = uStartFrame; i < uNumFrames; i++)
			{
				AkReal32 sample = pfDelayR[iIndex];

				AkReal32 mixValue = (AkReal32)i / (AkReal32)(uNumFrames - 1);

				// ramp up
				out_pBuffer[i] = (mixValue)* sample;

				iIndex++;
			}
		}
	}

	AKASSERT(iIndex <= uMaxDelayLength);
	
	// ----------------------------------------------------------------------------------------------------------
	// Update new delay
	currentDelay = in_fTargetDelayLen;
	return currentDelay;
}

AkReal32 CAkFilteredFracDelayLines::ProcessStationaryDelayRead(
	AkSampleType* out_pBuffer,
	AkUInt16 in_uNumFrames,
	const AkReal32 in_fTargetDelayLen
	)
{
	AkUInt32 uMaxDelayLength = m_DelayMem.GetMaxDelayLength();
	AkUInt32 uNumFrames = in_uNumFrames;

	// ----------------------------------------------------------

	// memory pointer and write offset
	AkReal32 *pfDelayR = (AkReal32 *)m_DelayMem.GetCurrentPointer( 0 );
	AkReal32 fWritePos = (AkReal32)m_DelayMem.GetCurrentWriteOffset();

	AkReal32 fIndex = (fWritePos - uNumFrames) - in_fTargetDelayLen;
	// Do the round up before wrapping
	fIndex += 0.5;

	AK_FPSetValLT( fIndex, 0.f, fIndex, fIndex + uMaxDelayLength );
	AK_FPSetValGTE( fIndex, (AkReal32)uMaxDelayLength, fIndex, fIndex - uMaxDelayLength );
	
	AkUInt32 iIndex = (AkUInt32)floor( fIndex );

	AKASSERT( iIndex >= 0 );
	AKASSERT( iIndex < uMaxDelayLength );

	if (iIndex + uNumFrames < uMaxDelayLength)
	{
		// 1 loop
		for (AkUInt32 i = 0; i < uNumFrames; i++)
		{
			out_pBuffer[i] = pfDelayR[iIndex];
			iIndex++;
		}
	}
	else
	{
		// 2 loops
		AkUInt32 uStartFrame = uMaxDelayLength - iIndex;
		for (AkUInt32 i = 0; i < uStartFrame; i++)
		{
			out_pBuffer[i] = pfDelayR[iIndex];
			iIndex++;
		}
		iIndex = 0;
		for (AkUInt32 i = uStartFrame; i < uNumFrames; i++)
		{
			out_pBuffer[i] = pfDelayR[iIndex];
			iIndex++;
		}
	}
	AKASSERT(iIndex <= uMaxDelayLength);

	return in_fTargetDelayLen;
}

AkReal32 CAkFilteredFracDelayLines::ProcessCrossfadeDelayRead(
	AkSampleType* out_pBuffer,
	AkUInt16 in_uNumFrames,
	const AkReal32 in_fTargetDelayLen,
	AkReal32 currentDelay)
{
	AkUInt32 uMaxDelayLength = m_DelayMem.GetMaxDelayLength();
	AkUInt32 uNumFrames = in_uNumFrames;

	// ----------------------------------------------------------

	// memory pointer and write offset
	AkReal32 *pfDelayR = (AkReal32 *)m_DelayMem.GetCurrentPointer( 0 );
	AkReal32 fWritePos = (AkReal32)m_DelayMem.GetCurrentWriteOffset();

	AkReal32 fIndex1 = (fWritePos - uNumFrames) - currentDelay;

	// Do the round up before wrapping
	fIndex1 += 0.5;

	AK_FPSetValLT( fIndex1, 0.f, fIndex1, fIndex1 + uMaxDelayLength );
	AK_FPSetValGTE( fIndex1, (AkReal32)uMaxDelayLength, fIndex1, fIndex1 - uMaxDelayLength );

	AkUInt32 iIndex1 = (AkUInt32)floor( fIndex1 );

	AKASSERT( iIndex1 >= 0 );
	AKASSERT( iIndex1 < uMaxDelayLength );

	AkReal32 fIndex2 = (fWritePos - uNumFrames) - in_fTargetDelayLen;

	// Do the round up before wrapping
	fIndex2 += 0.5;

	AK_FPSetValLT( fIndex2, 0.f, fIndex2, fIndex2 + uMaxDelayLength );
	AK_FPSetValGTE( fIndex2, (AkReal32)uMaxDelayLength, fIndex2, fIndex2 - uMaxDelayLength );

	AkUInt32 iIndex2 = (AkUInt32)floor( fIndex2 );

	AKASSERT( iIndex2 >= 0 );
	AKASSERT( iIndex2 < uMaxDelayLength );

	for ( AkUInt32 i = 0; i < uNumFrames; i++ )
	{
		// Current read head
		AkReal32 sample1 = pfDelayR[iIndex1];

		// Target read head
		AkReal32 sample2 = pfDelayR[iIndex2];

		// crossfade samples
		AkReal32 mixValue = (AkReal32)i / (AkReal32)(uNumFrames - 1);
		out_pBuffer[i] = (1.f - mixValue) * sample1 + mixValue * sample2;

		iIndex1++;
		if ( iIndex1 >= uMaxDelayLength )
			iIndex1 = 0;

		iIndex2++;
		if ( iIndex2 >= uMaxDelayLength )
			iIndex2 = 0;
	}

	// ----------------------------------------------------------------------------------------------------------
	// Update new delay
	currentDelay = in_fTargetDelayLen;
	return currentDelay;
}

AkReal32 CAkFilteredFracDelayLines::ProcessVariableDelayRead(
	AkSampleType* out_pBuffer,
	AkUInt16 in_uNumFrames,
	const AkReal32 in_fTargetDelayLen,
	AkReal32 currentDelay,
	AkReal32 fDelayGrowthSizeFrame)
{
	AkUInt32 uMaxDelayLength = m_DelayMem.GetMaxDelayLength();
	AkUInt32 uNumFrames = in_uNumFrames;

	// ----------------------------------------------------------

	// memory pointer and write offset
	AkReal32 *pfDelayR = (AkReal32 *)m_DelayMem.GetCurrentPointer( 0 );
	AkReal32 fWritePos = (AkReal32)m_DelayMem.GetCurrentWriteOffset();

	// current slew increment
	AkReal32 fDelayGrowthSizeInc = fDelayGrowthSizeFrame / (AkReal32)uNumFrames;

	for ( AkUInt32 i = 0; i < uNumFrames; i++ )
	{
		// accumulation of currentDelay causes drift, so multiply
		// the read offset is fWriteOffset + currentDelay
		// the write pointer was incremented in processWrite so subtract numFrames
		AkReal32 fIndex = (fWritePos - uNumFrames) - (currentDelay + i * fDelayGrowthSizeInc) + i;

		out_pBuffer[i] = linearInterp( pfDelayR, fIndex, uMaxDelayLength );
	}

	// ----------------------------------------------------------------------------------------------------------
	// Update new delay (avoids accumulation precision issues)
	currentDelay += fDelayGrowthSizeFrame;

	return currentDelay;
}
