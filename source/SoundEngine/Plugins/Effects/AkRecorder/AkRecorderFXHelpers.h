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

#include "AudiolibDefs.h"

static void InitBuffer(AkInt16 * dst, int nFloats, int nStrideDst)
{
	do
	{
		*dst = (AkInt16)(0);
		dst += nStrideDst;
	} while (--nFloats);
}

static void FloatsToShorts( AkInt16 * dst, float * src, int nFloats, float fGain, float fGainInc, int nStrideSrc, int nStrideDst )
{
	do
	{
		AkReal32 fSample = *src * fGain;
		fGain += fGainInc;

		if ( fSample > AUDIOSAMPLE_FLOAT_MAX )
			fSample = AUDIOSAMPLE_FLOAT_MAX;
		else if ( fSample < AUDIOSAMPLE_FLOAT_MIN )
			fSample = AUDIOSAMPLE_FLOAT_MIN;

		*dst = (AkInt16) ( fSample * AUDIOSAMPLE_SHORT_MAX );

		src += nStrideSrc;
		dst += nStrideDst;
	}
	while( --nFloats );
}

static void AddFloatsToShorts( AkInt16 * dst, float * src, int nFloats, float fMul, float fGain, float fGainInc, int nStrideSrc, int nStrideDst )
{
	do
	{
		AkReal32 fSample = *src;

		AkInt32 iSample = (AkInt32) ( fMul * fGain * fSample * AUDIOSAMPLE_SHORT_MAX );
		iSample += *dst;

		fGain += fGainInc;

		if ( iSample > (AkInt32) AUDIOSAMPLE_SHORT_MAX )
			iSample = (AkInt32) AUDIOSAMPLE_SHORT_MAX;
		else if ( iSample < (AkInt32) AUDIOSAMPLE_SHORT_MIN )
			iSample = (AkInt32) AUDIOSAMPLE_SHORT_MIN;

		*dst = (AkInt16) iSample;

		src += nStrideSrc;
		dst += nStrideDst;
	}
	while( --nFloats );
}
