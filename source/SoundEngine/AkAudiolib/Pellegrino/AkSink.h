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
// AkSink.h
//
// Platform dependent part
//
//////////////////////////////////////////////////////////////////////
#pragma once

#include <AK/SoundEngine/Common/AkSoundEngine.h>

#include "AkSettings.h"
#include "AkSinkBase.h"

#include <audio_out2.h>

namespace AK
{
	class IAkPellegrinoContext;
}

class CAkSinkPellegrino : public CAkSinkBase
{
public:

	CAkSinkPellegrino();

	virtual AKRESULT Init( 
		AK::IAkPluginMemAlloc *	in_pAllocator,				// Interface to memory allocator to be used by the effect.
		AK::IAkSinkPluginContext *	in_pSinkPluginContext,	// Interface to sink plug-in's context.
		AK::IAkPluginParam *		in_pParams,					// Interface to plug-in parameters.
		AkAudioFormat &			in_rFormat					// Audio data format of the input signal. 
		);

	virtual AKRESULT Term( AK::IAkPluginMemAlloc * in_pAllocator );

	virtual AKRESULT Reset();

	virtual bool IsStarved();
	virtual void ResetStarved();

	virtual AkAudioBuffer& NextWriteBuffer() { return m_MasterOut; };

	virtual AKRESULT IsDataNeeded( AkUInt32 & out_uBuffersNeeded );

	virtual void PassData();
	virtual void PassSilence();

	static bool IsBGMOverriden();
	SceAudioOut2PortHandle GetAudioOut2PortHandle();

protected:
	void CheckBGMOverride();

	AK::IAkPellegrinoContext* m_pPlatformContext;
	SceAudioOut2ContextHandle m_hCtx;	// Handle to the audioOut2 context the engine is using
	SceAudioOut2PortHandle	m_hPort;	// handle to the port
	SceAudioOut2UserHandle  m_hUser;	// User Id.
	AkUInt32				m_uType;	// Port type

	void *				m_pvAudioBuffer; // audio buffer to push data into
	AkUInt32			m_uBufferSize;	// size of audio buffer in bytes
	
	SceAudioOut2Pcm m_audioOutPcm;
	SceAudioOut2Attribute m_audioOutPcmAttribute;

	AkChannelConfig 		m_speakersConfig;
	AkPipelineBufferBase	m_MasterOut;		// buffer to write out to pcm

	static void RegisterSink( CAkSinkPellegrino * in_pSink );
	static void UnregisterSink( CAkSinkPellegrino * in_pSink );	// returns true if sink was found and unregistered, false otherwise.

	typedef AkArray<CAkSinkPellegrino*, CAkSinkPellegrino*> LibAudioSinks;
	static LibAudioSinks	s_listSinks;
};
