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

//////////////////////////////////////////////////////////////////////
//
// AkModulatorParams.h
//
// creator : nharris
//
//////////////////////////////////////////////////////////////////////

#ifndef _MODULATOR_PARAMS_H_
#define _MODULATOR_PARAMS_H_

#include "AudiolibDefs.h"
#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkListBare.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include "LFO.h"
#include "LFOMono.h"
#include <limits.h>

#define INF_SUSTAIN INT_MAX

enum AkModulatorState
{
	AkModulatorState_Invalid = -1,
	AkModulatorState_New,
	AkModulatorState_Triggered,
	AkModulatorState_ReleaseTriggered,
	AkModulatorState_Finished
};

class CAkModulatorCtx;

inline void ValidateState( AkModulatorState in_eState )
{
	AKASSERT(in_eState >= AkModulatorState_New && in_eState <= AkModulatorState_Finished);
}

class CAkMidiNoteState;

namespace DSP
{
	struct ModulatorLFOOutputPolicy{ AkForceInline void OutputFromLFO(AkReal32 *pfBuf, AkReal32 fLFO, AkReal32 /*fAmp*/ ) { *pfBuf = fLFO; } };
	typedef MonoLFO< Unipolar, ModulatorLFOOutputPolicy > ModulatorLFO;
}

// Output data types
//
struct AkModulatorOutput
{
	AkModulatorOutput(): m_pBuffer(NULL), m_eNextState(AkModulatorState_Invalid), m_fOutput(0.0f), m_fPeak(0.0) {}
	
	AkReal32* m_pBuffer;

	AkModulatorState m_eNextState;
	AkReal32 m_fOutput;
	AkReal32 m_fPeak;
}AK_ALIGN_DMA;

struct AkEnvelopeOutput: public AkModulatorOutput 
{}AK_ALIGN_DMA;

struct AkLFOOutput: public AkModulatorOutput 
{
	DSP::ModulatorLFO m_lfo;
}AK_ALIGN_DMA;

struct AkTimeModOutput : public AkModulatorOutput
{}AK_ALIGN_DMA;


// Params data types
//
struct AkModulatorParams
{
	AkModulatorState m_eState;
	AkUInt32 m_uStartOffsetFrames;
	AkUInt32 m_uReleaseFrame;
	AkUInt32 m_uElapsedFrames;
	AkReal32 m_fPreviousOutput;
	AkUInt32 m_uBufferSize;
}AK_ALIGN_DMA;

struct AkEnvelopeParams: public AkModulatorParams
{
	AkReal32 m_fStartValue;
	AkUInt32 m_uAttack;
	AkReal32 m_fCurve;
	AkReal32 m_fSustain;
	AkUInt32 m_uDecay;
	AkUInt32 m_uRelease;

	void Fill(CAkModulatorCtx* in_pCtx);

} AK_ALIGN_DMA;

struct AkLFOParams: public AkModulatorParams
{
	AkReal32 m_fDepth;
	AkUInt32 m_uAttack;
	AkReal32 m_fInitialPhase;

	/*
	Waveform 	eWaveform;		// Waveform type.
	AkReal32 	fFrequency;		// LFO frequency (Hz).
	AkReal32	fSmooth;		// Waveform smoothing [0,1].
	AkReal32	fPWM;			// Pulse width modulation (applies to square wave only).
	*/
	DSP::LFO::Params m_DspParams;

	DSP::ModulatorLFO m_lfo;

	void Fill(CAkModulatorCtx* in_pCtx);

} AK_ALIGN_DMA;

struct AkTimeModParams : public AkModulatorParams
{
	AkUInt32 m_uDuration;
	AkUInt32 m_uLoopsDuration;
	AkReal32 m_fPlaybackSpeed;
	AkInt32 m_iInitialDelay;

	void Fill(CAkModulatorCtx* in_pCtx);

} AK_ALIGN_DMA;

#endif
