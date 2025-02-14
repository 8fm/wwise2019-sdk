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
// AkMeterFX.h
//
// Meter processing FX implementation.
//
// Copyright 2010 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_METERFX_H_
#define _AK_METERFX_H_

#include "AkMeterFXParams.h"

//-----------------------------------------------------------------------------
// Name: class CAkMeterFX
//-----------------------------------------------------------------------------
class CAkMeterFX : public AK::IAkInPlaceEffectPlugin
{
public:
	CAkMeterFX * pNextItem; // for CAkMeterManager::Meters

    // Constructor/destructor
    CAkMeterFX();
    ~CAkMeterFX();

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
	friend class CAkMeterManager;
	friend struct MeterKey;

    CAkMeterFXParams * m_pParams;
	AK::IAkEffectPluginContext * m_pCtx;
	AK::IAkPluginMemAlloc * m_pAllocator;
	class CAkMeterManager * m_pMeterManager;

	AkMeterState m_state;

	AkReal32 m_fMin;
	AkUniqueID m_uGameParamID;
	AkGameObjectID m_uGameObjectID;
	AkUniqueID m_uNodeID;
	AkMeterMode m_eMode;
	bool m_bTerminated;
	AkReal32 m_fDownstreamGain;
	bool m_bFirstDownstreamGain;
};

#endif // _AK_METERFX_H_