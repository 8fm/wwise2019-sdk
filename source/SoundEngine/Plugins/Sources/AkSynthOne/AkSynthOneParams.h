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

#ifndef _AK_SYNTHONEPARAMS_H_
#define _AK_SYNTHONEPARAMS_H_

#include <AK/Tools/Common/AkAssert.h>
#include <AK/SoundEngine/Common/AkMidiTypes.h>
#include <math.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>

// Parameters IDs for the Wwise or RTPC.

static const AkPluginParamID AK_SYNTHONE_FXPARAM_FREQUENCYMODE_ID		= 1;
static const AkPluginParamID AK_SYNTHONE_FXPARAM_BASEFREQUENCY_ID		= 2;	// RTPCable

static const AkPluginParamID AK_SYNTHONE_FXPARAM_OPERATIONMODE_ID		= 3;
static const AkPluginParamID AK_SYNTHONE_FXPARAM_OUTPUTLEVEL_ID			= 4;	// RTPCable

static const AkPluginParamID AK_SYNTHONE_FXPARAM_NOISESHAPE_ID			= 5;
static const AkPluginParamID AK_SYNTHONE_FXPARAM_NOISELEVEL_ID			= 6;	// RTPCable

static const AkPluginParamID AK_SYNTHONE_FXPARAM_OSC1_WAVEFORM_ID		= 7;
static const AkPluginParamID AK_SYNTHONE_FXPARAM_OSC1_TRANSPOSE_ID		= 8;	// RTPCable
static const AkPluginParamID AK_SYNTHONE_FXPARAM_OSC1_LEVEL_ID			= 9;	// RTPCable
static const AkPluginParamID AK_SYNTHONE_FXPARAM_OSC1_PWM_ID			= 10;	// RTPCable
static const AkPluginParamID AK_SYNTHONE_FXPARAM_OSC1_INVERT_ID			= 11;	// RTPCable

static const AkPluginParamID AK_SYNTHONE_FXPARAM_OSC2_WAVEFORM_ID		= 12;
static const AkPluginParamID AK_SYNTHONE_FXPARAM_OSC2_TRANSPOSE_ID		= 13;	// RTPCable
static const AkPluginParamID AK_SYNTHONE_FXPARAM_OSC2_LEVEL_ID			= 14;	// RTPCable
static const AkPluginParamID AK_SYNTHONE_FXPARAM_OSC2_PWM_ID			= 15;	// RTPCable
static const AkPluginParamID AK_SYNTHONE_FXPARAM_OSC2_INVERT_ID			= 16;	// RTPCable

static const AkPluginParamID AK_SYNTHONE_FXPARAM_FMAMOUNT_ID			= 17;	// RTPCable

static const AkPluginParamID AK_SYNTHONE_FXPARAM_OVERSAMPLING_ID		= 18;	// RTPCable

static const AkPluginParamID AK_NUM_SYNTHONE_FXPARAM					= 19;

// Tone generator waveform type
enum AkSynthOneWaveType
{
	AkSynthOneWaveType_Sine = 0,
	AkSynthOneWaveType_Triangle,
	AkSynthOneWaveType_Square,
	AkSynthOneWaveType_Sawtooth
};

enum AkSynthOneNoiseType
{
	AkSynthOneNoiseType_White =	0,
	AkSynthOneNoiseType_Pink,
	AkSynthOneNoiseType_Red,
	AkSynthOneNoiseType_Purple
};

enum AkSynthOneOperationMode
{
	AkSynthOneOperationMode_Mix = 0,	// Add Osc1 & Osc2 outputs
	AkSynthOneOperationMode_Ring		// Mutliply Osc1 & Osc2 outputs
};

enum AkSynthOneFrequencyMode
{
	AkSynthOneFrequencyMode_Specify = 0,	// Specify base frequency
	AkSynthOneFrequencyMode_MidiNote		// Use supplied MIDI note
};

enum AkSynthOneSel
{
	AkSynthOneSel_Noise = 0,
	AkSynthOneSel_Osc1,
	AkSynthOneSel_Osc2,
	AkSynthOneSel_Output
};

// Valid parameter ranges
#define SYNTHONE_LEVEL_MIN				(-96.0f)
#define SYNTHONE_LEVEL_MAX				(24.f)
#define SYNTHONE_FREQUENCY_MIN			(8.f)
#define SYNTHONE_FREQUENCY_MAX			(20000.f)
#define SYNTHONE_TRANSPOSE_MIN			(-3600)
#define SYNTHONE_TRANSPOSE_MAX			(3600)
#define SYNTHONE_WAVETYPE_MIN			(AkSynthOneWaveType_Sine)
#define SYNTHONE_WAVETYPE_MAX			(AkSynthOneWaveType_Sawtooth)
#define SYNTHONE_NOISETYPE_MIN			(AkSynthOneNoiseType_White)
#define SYNTHONE_NOISETYPE_MAX			(AkSynthOneNoiseType_Pink)
#define SYNTHONE_OPERATIONMODE_MIN		(AkSynthOneOperationMode_Mix)
#define SYNTHONE_OPERATIONMODE_MAX		(AkSynthOneOperationMode_Ring)
#define SYNTHONE_FREQUENCYMODE_MIN		(AkSynthOneFrequencyMode_Specify)
#define SYNTHONE_FREQUENCYMODE_MAX		(AkSynthOneFrequencyMode_MidiNote)
#define SYNTHONE_FMAMOUNT_MIN			(0.f)
#define SYNTHONE_FMAMOUNT_MAX			(100.f)
#define SYNTHONE_PWM_MIN				(0.f)
#define SYNTHONE_PWM_MAX				(100.f)


// Structure of parameters that remain true for the whole lifespan of the tone generator (1 playback).

// Parameters struture for this effect.
struct AkSynthOneParams
{
	// Operation mode (mixing type)
	AkSynthOneOperationMode	eOpMode;

	// Frequency mode
	AkSynthOneFrequencyMode	eFreqMode;

	// Base frequency (when using specified frequency)
	AkReal32	fBaseFreq;

	// Final output level
	AkReal32	fOutputLevel;

	// Noise output level
	AkSynthOneNoiseType	eNoiseType;
	AkReal32	fNoiseLevel;

	// Frequency modulation
	AkReal32	fFmAmount;

	// Apply over-sampling;
	bool		bOverSampling;

	// Osc1 parameters; these can all be RTPCed.
	AkSynthOneWaveType	eOsc1Waveform;
	bool		bOsc1Invert;
	AkInt32		iOsc1Transpose;
	AkReal32	fOsc1Level;
	AkReal32	fOsc1Pwm;

	// Osc2 parameters; these can all be RTPCed.
	AkSynthOneWaveType	eOsc2Waveform;
	bool		bOsc2Invert;
	AkInt32		iOsc2Transpose;
	AkReal32	fOsc2Level;
	AkReal32	fOsc2Pwm;
};


//-----------------------------------------------------------------------------
// Name: class CAkSynthOneParam
// Desc: Implementation of tone generator's shared parameters.
//-----------------------------------------------------------------------------
class CAkSynthOneParams : public AK::IAkPluginParam
{
public:

	// Allow effect to call accessor functions for retrieving parameter values.
	friend class CAkSynthOne;
	friend class CAkSynthOneDsp;
	
    // Constructor.
    CAkSynthOneParams();

	// Copy constructor.
    CAkSynthOneParams( const CAkSynthOneParams & in_rCopy );

	// Destructor.
    virtual ~CAkSynthOneParams();

    // Create duplicate.
    virtual IAkPluginParam * Clone( AK::IAkPluginMemAlloc * in_pAllocator );

    // Initialize.
    virtual AKRESULT Init(	AK::IAkPluginMemAlloc *	in_pAllocator,						   
					const void *			in_pParamsBlock, 
                    AkUInt32				in_uBlockSize 
					);
	// Terminate.
    virtual AKRESULT Term( AK::IAkPluginMemAlloc * in_pAllocator );

    // Set all parameters at once.
    virtual AKRESULT SetParamsBlock( const void * in_pParamsBlock,
                             AkUInt32 in_uBlockSize
                             );

    // Update one parameter.
    virtual AKRESULT SetParam( AkPluginParamID	in_ParamID,
                       const void *			in_pValue,
                       AkUInt32			in_uParamSize
                       );

private: 

	////// Synth One internal API ///// 

	// Safely retrieve base frequency RTPC value.
	AkReal32 GetBaseFrequency() const;
	bool GetOverSampling() const;
	AkReal32 GetFmAmount() const;
	AkReal32 GetPwm( const AkSynthOneSel in_eSel ) const;
	AkReal32 GetLevel( const AkSynthOneSel in_eSel ) const;
	AkReal32 GetTranspose( const AkSynthOneSel in_eSel ) const;
	AkSynthOneWaveType GetOscWaveform( const AkSynthOneSel in_eSel ) const;
	bool GetOscInvert( const AkSynthOneSel in_eSel ) const;

	// Hide (not implemented) assignment operator
	CAkSynthOneParams & operator=( const CAkSynthOneParams & in_rCopy );

private:

    // Parameter structure. (Includes static and RTPC parameters).
    AkSynthOneParams m_Params;
};

// Retrieve current base frequency RTPC value.
AkForceInline AkReal32 CAkSynthOneParams::GetBaseFrequency() const
{
	AkReal32 fFrequency;
	fFrequency = m_Params.fBaseFreq;
	fFrequency = AkClamp( fFrequency, SYNTHONE_FREQUENCY_MIN, SYNTHONE_FREQUENCY_MAX );
	return fFrequency;
}

AkForceInline bool CAkSynthOneParams::GetOverSampling() const
{
	return m_Params.bOverSampling;
}

// Retrieve current gain RTPC value (linear value).
AkForceInline AkReal32 CAkSynthOneParams::GetLevel( const AkSynthOneSel in_eSel ) const
{
	AkReal32 fLevel;

	switch( in_eSel )
	{
	case AkSynthOneSel_Noise: fLevel = m_Params.fNoiseLevel; break;
	case AkSynthOneSel_Osc1: fLevel = m_Params.fOsc1Level; break;
	case AkSynthOneSel_Osc2: fLevel = m_Params.fOsc2Level; break;
	case AkSynthOneSel_Output: fLevel = m_Params.fOutputLevel; break;
	default: AKASSERT( false );
	}
	fLevel = AkClamp( fLevel, SYNTHONE_LEVEL_MIN, SYNTHONE_LEVEL_MAX );

	// Make it a linear value
	return powf( 10.f, ( fLevel / 20.f ) );
}

// Retrieve current gain RTPC value (linear value).
AkForceInline AkReal32 CAkSynthOneParams::GetPwm( const AkSynthOneSel in_eSel ) const
{
	AkReal32 fPwm;

	switch( in_eSel )
	{
	case AkSynthOneSel_Osc1: fPwm = m_Params.fOsc1Pwm; break;
	case AkSynthOneSel_Osc2: fPwm = m_Params.fOsc2Pwm; break;
	default: AKASSERT( false ); fPwm = 0;
	}
	AKASSERT( fPwm >= SYNTHONE_PWM_MIN && fPwm <= SYNTHONE_PWM_MAX );

	return fPwm;
}

// Retrieve current gain RTPC value (linear value).
AkForceInline AkReal32 CAkSynthOneParams::GetTranspose( const AkSynthOneSel in_eSel ) const
{
	AkInt32 iTranspose;

	switch( in_eSel )
	{
	case AkSynthOneSel_Osc1: iTranspose = m_Params.iOsc1Transpose; break;
	case AkSynthOneSel_Osc2: iTranspose = m_Params.iOsc2Transpose; break;
	default: AKASSERT( false ); iTranspose = 0;
	}
	iTranspose = AkClamp( iTranspose, SYNTHONE_TRANSPOSE_MIN, SYNTHONE_TRANSPOSE_MAX );

	// Make it a frequency value
	return powf( 2.f, ( static_cast<AkReal32>( iTranspose ) / 1200.f ) );
}

AkForceInline AkReal32 CAkSynthOneParams::GetFmAmount() const
{
	AkReal32 fFmAmount = AkClamp( m_Params.fFmAmount, SYNTHONE_FMAMOUNT_MIN, SYNTHONE_FMAMOUNT_MAX );
	return fFmAmount;
}

AkForceInline AkSynthOneWaveType CAkSynthOneParams::GetOscWaveform( const AkSynthOneSel in_eSel ) const
{
	AkSynthOneWaveType eWaveform;

	switch( in_eSel )
	{
	case AkSynthOneSel_Osc1: eWaveform = m_Params.eOsc1Waveform; break;
	case AkSynthOneSel_Osc2: eWaveform = m_Params.eOsc2Waveform; break;
	default: AKASSERT( false ); eWaveform = AkSynthOneWaveType_Sine;
	}
	AKASSERT( eWaveform >= SYNTHONE_WAVETYPE_MIN && eWaveform <= SYNTHONE_WAVETYPE_MAX );

	// Make it a frequency value
	return eWaveform;
}

AkForceInline bool CAkSynthOneParams::GetOscInvert( const AkSynthOneSel in_eSel ) const
{
	bool bInvert;

	switch( in_eSel )
	{
	case AkSynthOneSel_Osc1: bInvert = m_Params.bOsc1Invert; break;
	case AkSynthOneSel_Osc2: bInvert = m_Params.bOsc2Invert; break;
	default: AKASSERT( false ); bInvert = false;
	}

	if( GetOscWaveform( in_eSel ) == AkSynthOneWaveType_Sawtooth )
		bInvert = ! bInvert;

	// Make it a frequency value
	return bInvert;
}

#endif
