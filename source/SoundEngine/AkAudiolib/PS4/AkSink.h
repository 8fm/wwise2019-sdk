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
#include <AK/Tools/Common/AkListBare.h>
#include "AkSettings.h"
#include "AkSinkBase.h"

namespace AK
{
	class IAkStdStream;
};

struct SceAudioOutOutputParam;

//----------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------

#define AkOutput_MAXCAPTURENAMELENGTH				(128)
#define	AK_PRIMARY_BUFFER_SAMPLE_TYPE			AkReal32 //AkReal32 //AkInt16
#define	AK_PRIMARY_BUFFER_BITS_PER_SAMPLE		(8 * sizeof(AK_PRIMARY_BUFFER_SAMPLE_TYPE))

//====================================================================================================
//====================================================================================================
class CAkSinkPS4 : public CAkSinkBase
{
public:

	CAkSinkPS4();

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

	virtual AkAudioBuffer& NextWriteBuffer()
	{
		m_MasterOut.AttachInterleavedData(
			GetRefillPosition(),
			AK_NUM_VOICE_REFILL_FRAMES,
			0,
			m_MasterOut.GetChannelConfig());
		return m_MasterOut;
	};

	virtual AKRESULT IsDataNeeded( AkUInt32 & out_uBuffersNeeded );

	virtual void PassData();	
	virtual void PassSilence();

	virtual void FillOutputParam(SceAudioOutOutputParam& out_rParam);

	// Helpers.
	void AdvanceReadPointer();
	void AdvanceWritePointer();

	void CheckBGMOverride();
	static bool IsBGMOverriden();
	virtual int GetPS4OutputHandle();
	
	CAkSinkPS4 *			pNextItem;	// List bare sibling.

protected:
	inline AkUInt8* GetRefillPosition() { return m_pvAudioBuffer + m_uWriteBufferIndex*m_uOneBufferSize; }
	void _Term();	// Internal/Deferred termination.
	inline AkUInt32 GetRefillSamples() { return m_MasterOut.GetChannelConfig().uNumChannels * sizeof( AkReal32 ) * AK_NUM_VOICE_REFILL_FRAMES; }

	int						m_port;									// sceAudioOut main port
	AkUInt32				m_uType;								// Port type
	AkUInt32				m_uUserId;								// User Id.
    	
	AkUInt8 *				m_pvAudioBuffer;						// Ring buffer memory	
	AkUInt8					m_uReadBufferIndex;						// Read index		
	AkUInt8					m_uWriteBufferIndex;					// Write index	
	AkUInt8					m_uBuffers;	
	bool					m_bStarved;								// Starvation flag.
	AkUInt32				m_uOneBufferSize;
	AkAtomic32				m_uBuffersReady;

	bool 					m_ManagedAudioOutInit;
	enum PortState {PortNotInit, PortOpen, PortClosing, PortClosed};
	PortState				m_ePortState;
	AkChannelConfig 		m_speakersConfig;	// speakers config, pipeline side.
	AkPipelineBufferBase	m_MasterOut;		// speakers config, system side. May differ on PS4.

	// Common audio output thread management.
	static AK_DECLARE_THREAD_ROUTINE(AudioOutThreadFunc);
	static void RegisterSink( CAkSinkPS4 * in_pSink );
	static bool UnregisterSink( CAkSinkPS4 * in_pSink, bool & out_bIsLast );	// returns true if sink was found and unregistered, false otherwise.

	typedef AkArray<CAkSinkPS4*, CAkSinkPS4*> LibAudioSinks;
	static LibAudioSinks	s_listSinks;
	static CAkLock			s_lockSinks;
	static AkThread			s_hAudioOutThread;						// Thread for all libAudio sinks.	
};
