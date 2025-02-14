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
// AkPeakLimiterFX.h
//
// PeakLimiter processing FX implementation.
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_PEAKLIMITERFX_H_
#define _AK_PEAKLIMITERFX_H_

#include "AkPeakLimiterFXParams.h"
#include <AK/Plugin/PluginServices/AkFXTailHandler.h>

struct AkPeakLimiterSideChain
{
	AkReal32	fGainDb;				// Current gain envelope value
	AkReal32	fCurrentPeak;			// Current peak in look ahead buffer
	AkReal32    fNextPeak;				// Next peak in look ahead buffer
	AkUInt32	uPeakTimer;				// Time before current peak value expires
};

//-----------------------------------------------------------------------------
// Name: class CAkPeakLimiterFX
//-----------------------------------------------------------------------------
class CAkPeakLimiterFX : public AK::IAkInPlaceEffectPlugin
{
public:

    // Constructor/destructor
    CAkPeakLimiterFX();
    ~CAkPeakLimiterFX();

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

	/// (Re-)Initialize delay line
	AKRESULT InitDelayLine();

	void Process( AkAudioBuffer * io_pBufferIn );
	void ProcessLinked( AkAudioBuffer * io_pBufferIn );
	void ProcessLinkedNoLFE( AkAudioBuffer * io_pBufferIn );

	// Function ptr to the appropriate DSP execution routine
	void (CAkPeakLimiterFX::*m_fpPerformDSP)( AkAudioBuffer * io_pBufferIn );

private:

	// parameter interface
    CAkPeakLimiterFXParams * m_pParams;
	AK::IAkPluginMemAlloc * m_pAllocator;
#ifndef AK_OPTIMIZED
	AK::IAkEffectPluginContext * m_pCtx;
#endif
	
	AkReal32	m_fCurrentGain;			// Current gain value			

	// Audio format information
	AkAudioFormat m_format; 
	AkUInt32	m_uNumPeakLimitedChannels;

	// Sidechain
	AkUInt32	m_uNumSideChain;
	AkUInt32	m_uLookAheadFrames;			// Number of sample frames in look-ahead buffer (all side chains)
	AkPeakLimiterSideChain * m_SideChains;	// Variable number of side chains

	// Channel delays
	AkReal32 *	m_pfDelayBuffer;		// Delay line storage
	AkUInt32	m_uFramePos;			// Current position within delay line

	AkFXTailHandler	m_FXTailHandler;	
	
	// Cached values for optimization
	AkReal32 m_fReleaseCoef;	
	AkReal32 m_fAttackCoef;		

	bool	m_bFirstTime;				// First time peak calculation, must go through look ahead buffer
};

#endif // _AK_PEAKLIMITERFX_H_