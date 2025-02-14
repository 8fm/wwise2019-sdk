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

#ifndef _AK_PITCHSHIFTERFX_H_
#define _AK_PITCHSHIFTERFX_H_

#include "AkPitchShifterFXParams.h"
#include "AkPitchShifterFXInfo.h"

//-----------------------------------------------------------------------------
// Name: class CAkPitchShifterFX
//-----------------------------------------------------------------------------
class CAkPitchShifterFX : public AK::IAkInPlaceEffectPlugin
{
public:

    // Constructor/destructor
    CAkPitchShifterFX();
    ~CAkPitchShifterFX();

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

	// Execution processing when the voice is virtual. Nothing special to do for this effect.
	AKRESULT TimeSkip(AkUInt32 in_uFrames){ return AK_DataReady; }	

private:

	void ComputeNumProcessedChannels( AkChannelConfig in_channelConfig );
	void ComputeTailLength();
	AKRESULT InitPitchVoice();
	void TermPitchVoice();
	void ResetPitchVoice();
	AKRESULT InitDryDelay();
	void TermDryDelay();
	void ResetDryDelay();

	CAkPitchShifterFXParams *	m_pParams;
	AK::IAkPluginMemAlloc *		m_pAllocator;
	AkPitchShifterFXInfo		m_FXInfo;
};

#endif // _AK_PITCHSHIFTERFX_H_
