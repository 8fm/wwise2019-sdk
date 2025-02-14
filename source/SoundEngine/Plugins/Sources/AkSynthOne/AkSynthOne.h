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

#ifndef _AK_SYNTHONE_H_
#define _AK_SYNTHONE_H_


#include "AkSynthOneDsp.h"

#include "AkMidiStructs.h"


class CAkSynthOneParams;

//-----------------------------------------------------------------------------
// Name: class CAkSynthOne
// Desc: Tone generator implementation (source effect).
//-----------------------------------------------------------------------------
class CAkSynthOne : public AK::IAkSourcePlugin
{
public:

    // Constructor.
    CAkSynthOne();

	// Destructor.
    virtual ~CAkSynthOne();

    // Effect initialization.
    virtual AKRESULT Init(
					AK::IAkPluginMemAlloc *			in_pAllocator,    	// Memory allocator interface.
                    AK::IAkSourcePluginContext *	in_pSourceCtx,		// Source FX context
                    AK::IAkPluginParam *			in_pParams,         // Effect parameters.
                    AkAudioFormat &					io_rFormat			// Supported audio output format.
                    );

    // Effect termination.
    virtual AKRESULT Term( AK::IAkPluginMemAlloc * in_pAllocator );

	// Reset.
	virtual AKRESULT Reset( );

    // Effect information query.
    virtual AKRESULT GetPluginInfo( AkPluginInfo & out_rPluginInfo );

    // Execute effect processing.
	virtual void Execute(	AkAudioBuffer * io_pBuffer );

	// Get the duration of the source in mSec.
	virtual AkReal32 GetDuration() const { return 0.f; }

private:

	// Scratch buffer management
	bool AllocateScratchBuf( AkUInt32 in_uNumSample, bool in_bIsOverSampling );
	void ReleaseScratchBuf();

	//-----------------------------------------------------------------------------

	// Shared parameters structure (modified by Wwise or RTPC)
	CAkSynthOneParams * m_pParams;

	// Allocator passed during init.
	AK::IAkPluginMemAlloc*	m_pAllocator;

	// Source FX context interface
	AK::IAkSourcePluginContext * m_pSourceCtx;

	// MIDI note used for frequency configuration (eFreqMode = AkSynthOneFrequencyMode_MidiNote).
	AkMidiEventEx	m_midiEvent;

    // Internal state variables.
	bool				m_bOverSampling;
	AkReal32			m_fSampleRate;			// Sample rate

	AkUInt32			m_uScratchSize;
	AkReal32*			m_pScratchBuf;

	CAkSynthOneDsp		m_SynthOneDsp;
};

#endif // _AK_SYNTHONE_H_
