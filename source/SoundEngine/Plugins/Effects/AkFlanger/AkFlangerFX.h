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

#ifndef _AK_FLANGERFX_H_
#define _AK_FLANGERFX_H_

#include "AkFlangerFXParams.h"
#include "AkFlangerFXInfo.h"
#include "LFOMultiChannel.h"

namespace DSP
{
	class UniComb;
}

//-----------------------------------------------------------------------------
// Name: class CAkFlangerFX
//-----------------------------------------------------------------------------
class CAkFlangerFX : public AK::IAkInPlaceEffectPlugin
{
public:

    // Constructor/destructor
    CAkFlangerFX(); 
    ~CAkFlangerFX();

	// Allocate memory needed by effect and other initializations
    AKRESULT Init(	AK::IAkPluginMemAlloc *	in_pAllocator,		// Memory allocator interface.
					AK::IAkEffectPluginContext * in_pFXCtx,		// FX Context
                    AK::IAkPluginParam * in_pParams,			// Effect parameters.
                    AkAudioFormat &	in_rFormat					// Required audio input format.
					);
    
	// Free memory used by effect and effect termination
	AKRESULT Term( AK::IAkPluginMemAlloc * in_pAllocator );

	// Reset
	AKRESULT Reset( );

    // Effect type query.
    AKRESULT GetPluginInfo( AkPluginInfo & out_rPluginInfo );

    // Execute effect processing.
	void Execute( AkAudioBuffer * io_pBuffer );

	// Skips execution of some frames, when the voice is virtual.  Nothing special to do for this effect.
	AKRESULT TimeSkip(AkUInt32 in_uFrames) {return AK_DataReady;}	

private:
	AkChannelConfig AdjustEffectiveChannelConfig( AkChannelConfig in_channelConfig );
	// Init helpers
	AKRESULT InitUniCombs( AkChannelConfig in_channelConfig );
	AKRESULT InitLFO( AkChannelConfig in_channelConfig );
	// Term helpers
	void TermUniCombs();
	void TermLFO();
	void ResetUniCombs( AkUInt32 in_uNumProcessedChannels );
	// Parameter updates
	AKRESULT LiveParametersUpdate( AkAudioBuffer* io_pBuffer );
	void RTPCParametersUpdate( );
private:

	DSP::UniComb *				m_pUniCombs;	// Modulated fractional delay lines
	FlangerLFO *				m_pLFO;		// Multichannel LFO generator.
	CAkFlangerFXParams *		m_pSharedParams;
	AK::IAkPluginMemAlloc *		m_pAllocator;
	AK::IAkEffectPluginContext * m_pFXCtx;
	AkFlangerFXInfo				m_FXInfo;
};

#endif // _AK_FLANGERFX_H_