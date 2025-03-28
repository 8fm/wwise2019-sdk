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
// AkParametricEQFX.h
//
// ParametricEQ FX implementation.
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_PARAMETRICEQFX_H_
#define _AK_PARAMETRICEQFX_H_

#include "AkParametricEQFXParams.h"
#include "BiquadFilter.h"

//-----------------------------------------------------------------------------
// Name: class CAkParametricEQFX
//-----------------------------------------------------------------------------
class CAkParametricEQFX : public AK::IAkInPlaceEffectPlugin
{
public:

    // Constructor/destructor
    CAkParametricEQFX();
    ~CAkParametricEQFX();

	// Allocate memory needed by effect and other initializations
    AKRESULT Init(	AK::IAkPluginMemAlloc *	in_pAllocator,		// Memory allocator interface.
					AK::IAkEffectPluginContext * in_pFXCtx,		// FX Context
				    AK::IAkPluginParam * in_pParams,			// Effect parameters.
                    AkAudioFormat &	in_rFormat					// Required audio input format.
				);
    
	// Free memory used by effect and effect termination
	AKRESULT Term( AK::IAkPluginMemAlloc * in_pAllocator );

	// Reset or seek to start (looping).
	AKRESULT Reset( );

    // Effect type query.
    AKRESULT GetPluginInfo( AkPluginInfo & out_rPluginInfo );

    // Execute effect processing.
	void Execute( AkAudioBuffer * io_pBuffer );

	// Skips execution of some frames, when the voice is virtual.  Nothing special to do for this effect.
	AKRESULT TimeSkip(AkUInt32 in_uFrames) {return AK_DataReady;}	

private:

	// Compute biquad filter coefficients
	void ComputeBiquadCoefs(AkBandNumber in_eBandNum, EQModuleParams * in_sModuleParams);

private:

	::DSP::BiquadFilterMultiSIMD m_Biquad[NUMBER_FILTER_MODULES];

	// Shared parameter interface
    CAkParameterEQFXParams * m_pSharedParams;

	// Audio format information
	AkUInt32 m_uNumProcessedChannels;
	AkUInt32 m_uSampleRate;
	
	// Filter memories
	AkReal32 *		m_pfAllocatedMem;		

	// Current gain ramp status
	AkReal32		m_fCurrentGain;	
};

#endif // _AK_PARAMETRICEQFX_H_