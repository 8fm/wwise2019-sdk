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

#pragma once

#include "AkMeterFXParams.h"
#include "AkMath.h"
#include "AkPrivateTypes.h"
#include "AudiolibDefs.h"
#include <limits.h>

static AkReal32 AkMeterGetValue(
	AkAudioBuffer * in_pBuffer,
	AkMeterFXParams * in_pParams,
	AkReal32 * in_pfInChannel,
	AkReal32 in_fGain,
	AkReal32 in_fGainInc )
{
	AkUInt32 uNumChannels = in_pBuffer->NumChannels();
	AkReal32 fValue = 0;

	if ( in_pBuffer->uValidFrames )
	{
		if ( in_pParams->NonRTPC.eMode == AkMeterMode_Peak )
		{
			AkReal32 fMin = (AkReal32) INT_MAX;
			AkReal32 fMax = (AkReal32) INT_MIN;

			for ( unsigned int uChan = 0; uChan < uNumChannels; ++uChan )
			{
				AkReal32 * AK_RESTRICT pfBuf = in_pBuffer->GetChannel(uChan);
				AkReal32 * AK_RESTRICT pfEnd = pfBuf + in_pBuffer->uValidFrames;

				AkReal32 fChannelMin = (AkReal32) INT_MAX;
				AkReal32 fChannelMax = (AkReal32) INT_MIN;

				AkReal32 fGain = in_fGain;

				while ( pfBuf < pfEnd )
				{
					AkReal32 fSample = *pfBuf;
					++pfBuf;

					fSample *= fGain;
					fGain += in_fGainInc;

					fChannelMin = AK_FPMin( fChannelMin, fSample );
					fChannelMax = AK_FPMax( fChannelMax, fSample );
				}

				if ( in_pfInChannel )
				{
					AkReal32 fValueMax = AK_FPMax(fabsf(fChannelMin), fChannelMax);
					if (fValueMax > 0.f)
					{
						in_pfInChannel[uChan] = 20.f*log10f(fValueMax);
					}
					else
					{
						in_pfInChannel[uChan] = -200.f;
					}
				}

				fMin = AK_FPMin( fMin, fChannelMin );
				fMax = AK_FPMax( fMax, fChannelMax );
			}

			fValue = AK_FPMax( fabsf( fMin ), fMax );
		}
		else // RMS
		{
			AkReal32 fMax = (AkReal32) INT_MIN;
			for ( unsigned int uChan = 0; uChan < uNumChannels; ++uChan )
			{
				AkReal32 * AK_RESTRICT pfBuf = in_pBuffer->GetChannel(uChan);
				AkReal32 * AK_RESTRICT pfEnd = pfBuf + in_pBuffer->uValidFrames;

				AkReal32 fGain = in_fGain;

				AkReal32 fSum = 0.0f;
				while ( pfBuf < pfEnd )
				{
					AkReal32 fSample = *pfBuf;
					++pfBuf;

					fSample *= fGain;
					fGain += in_fGainInc;

					fSum += fSample * fSample;
				}

				fSum /= in_pBuffer->uValidFrames;

				if ( in_pfInChannel )
				{
					in_pfInChannel[ uChan ] = 10.f*log10f( fSum );
				}

				fMax = AK_FPMax( fMax, fSum );
			}

			fValue = sqrtf( fMax );
		}
	}
	else if ( in_pfInChannel )
	{
		for ( unsigned int uChan = 0; uChan < uNumChannels; ++uChan )
			in_pfInChannel[ uChan ] = AK_MINIMUM_VOLUME_DBFS;
	}

	if (fValue > 0.f) // WG-30561
	{
		fValue = 20.f*log10f( fValue ); // convert to dB
	}
	else
	{
		fValue = -200.f;
	}
	fValue = AK_FPMax( in_pParams->RTPC.fMin, fValue );
	fValue = AK_FPMin( in_pParams->RTPC.fMax, fValue );

	return fValue;
}

static void AkMeterApplyBallistics(
	AkReal32 in_fValue,
	AkUInt32 in_uElapsed, // in audio frames
	AkMeterFXParams * in_pParams,
	AkMeterState * in_pState )
{
	if ( in_fValue > in_pState->fLastValue )
	{
		in_pState->fHoldTime = 0.0f;
		for ( int i = 0; i < HOLD_MEMORY_SIZE; ++i )
			in_pState->fHoldMemory[ i ] = in_pParams->RTPC.fMin;

		if ( in_pParams->RTPC.fAttack == 0.0f )
			in_pState->fLastValue = in_fValue;
		else
		{
			AkReal32 fTimeDelta = (AkReal32) in_uElapsed / in_pState->uSampleRate;
			AkReal32 fMaxRamp = ( fTimeDelta / in_pParams->RTPC.fAttack ) * 10.0f;
			in_pState->fLastValue = AK_FPMin( in_fValue, in_pState->fLastValue + fMaxRamp );
		}

		in_pState->fReleaseTarget = in_pState->fLastValue;
	}
	else
	{
		AkReal32 fTimeDelta = (AkReal32) in_uElapsed / in_pState->uSampleRate;
		in_pState->fHoldTime += fTimeDelta;

		if ( in_pState->fHoldTime >= in_pParams->RTPC.fHold )
		{
			int iMaxMemorySlot = HOLD_MEMORY_SIZE;
			AkReal32 fMemoryValue = in_fValue;
			for ( int i = 0; i < HOLD_MEMORY_SIZE; ++i )
			{
				if ( in_pState->fHoldMemory[ i ] >= fMemoryValue )
				{
					fMemoryValue = in_pState->fHoldMemory[ i ];
					iMaxMemorySlot = i;
				}
			}

			in_pState->fHoldTime = in_pParams->RTPC.fHold / ( HOLD_MEMORY_SIZE + 1 ) * (HOLD_MEMORY_SIZE - iMaxMemorySlot );

			int j = 0;
			for ( int i = iMaxMemorySlot + 1; i < HOLD_MEMORY_SIZE ; ++i )
				in_pState->fHoldMemory[ j++ ] = in_pState->fHoldMemory[ i ];
			for ( ; j < HOLD_MEMORY_SIZE; ++j )
				in_pState->fHoldMemory[ j ] = in_pParams->RTPC.fMin;

			in_pState->fReleaseTarget = fMemoryValue;
		}
		else
		{
			int iHoldSlot = (int) ( in_pState->fHoldTime / in_pParams->RTPC.fHold * HOLD_MEMORY_SIZE + 0.5 ) - 1;
			if ( iHoldSlot >= 0 )
			{
				if ( in_fValue > in_pState->fHoldMemory[ iHoldSlot ] )
					in_pState->fHoldMemory[ iHoldSlot ] = in_fValue;
			}

			if ( in_fValue > in_pState->fReleaseTarget )
				in_pState->fReleaseTarget = in_fValue;
		}

		if ( in_pParams->RTPC.fRelease == 0.0f )
			in_pState->fLastValue = in_pState->fReleaseTarget;
		else
		{
			AkReal32 fMaxRamp = ( fTimeDelta / in_pParams->RTPC.fRelease ) * 10.0f;
			in_pState->fLastValue = AK_FPMax( in_pState->fReleaseTarget, in_pState->fLastValue - fMaxRamp );
		}
	}
}