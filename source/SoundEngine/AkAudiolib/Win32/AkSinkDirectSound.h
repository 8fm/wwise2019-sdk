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

#pragma once

#include "AkSink.h"

#ifdef AK_DIRECTSOUND

#define DIRECTSOUND_VERSION 0x0800
#include <mmeapi.h>
#include <mmreg.h>
#include <dsound.h>
#include <ks.h>
#include <ksmedia.h>


struct BufferUpdateParams
{
	AkUInt32	ulOffset;
	AkUInt32	ulBytes;
	void*		pvAudioPtr1;
	AkUInt32	ulAudioBytes1;
	void*		pvAudioPtr2;
	AkUInt32	ulAudioBytes2;
	AkUInt32	ulFlags;
};


class CAkSinkDirectSound : public CAkSink
{
public:
	CAkSinkDirectSound();
	~CAkSinkDirectSound();

	AKRESULT Init( AkPlatformInitSettings & in_settings, AkChannelMask in_uChannelMask );

	virtual AKRESULT Term( 
		AK::IAkPluginMemAlloc * in_pAllocator 	// UNUSED interface to memory allocator to be used by the plug-in
		);

	// starts a voice according to what's in Params if possible
	virtual AKRESULT Reset();
	virtual bool IsStarved();
	virtual void ResetStarved();

	virtual AKRESULT IsDataNeeded( AkUInt32 & out_uBuffersNeeded );

	// CAkSinkBase
	virtual void PassData();
	virtual void PassSilence();

	virtual AkAudioAPI GetType(){ return AkAPI_DirectSound; }

	static IDirectSound8 * GetDirectSoundInstance() { return m_pDirectSound; }

	friend class CAkSink;
protected:
#if defined (_DEBUG) || (_PROFILE)
	void AssertHR(HRESULT hr);
#endif
	
	static LPDIRECTSOUND8	m_pDirectSound;			// Pointer to the DirectSound Object

private:
	static void PrepareFormat( AkChannelConfig in_channelConfig, WAVEFORMATEXTENSIBLE & out_wfExt );

	AKRESULT SetPrimaryBufferFormat();
	HRESULT CreateSecondaryBuffer();

	AKRESULT GetBuffer(
		BufferUpdateParams&	io_rParams
		);

	AkChannelMask GetDSSpeakerConfig();

	AKRESULT ReleaseBuffer(
		const BufferUpdateParams&	in_rParams
		);

	void DoRefill(
		AkPipelineBufferBase&	in_AudioBuffer,
		const BufferUpdateParams&	in_rParams
		);

	void DoRefillSilence(
		AkUInt32	in_uNumFrames,
		const BufferUpdateParams&	in_rParams
		);

	AKRESULT GetRefillSize(
		AkUInt32&   out_uRefillFrames
		);

	LPDIRECTSOUNDBUFFER		m_pPrimaryBuffer;			// pointer on the primary buffer
	LPDIRECTSOUNDBUFFER8	m_pDSBuffer;				// Corresponding Direct Sound Buffer

	AkUInt32				m_ulBufferSize;				// playing buffer size
	AkUInt32				m_ulRefillOffset;			// current offset from start
	AkInt32					m_lRefillToEnd;				// what's refill-able
	AkUInt32				m_uFreeRefillFrames;		// What's free when IsDataNeeded() is called
	AkUInt32				m_uPlay;					// play position when IsDataNeeded() is called
	AkUInt32				m_uWrite;					// write position when IsDataNeeded() is called

	AkUInt16				m_usBlockAlign;

	// status
	bool					m_bStarved;
};

#endif
