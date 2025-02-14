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

#ifdef AK_XAUDIO2

#include "xaudio2.h"
#include <mmreg.h>

#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/Tools/Common/AkLock.h>
#include "PlatformAudiolibDefs.h" // FT
#include "AkLEngineStructs.h"
#include "AkFileParserBase.h"
#include "AkProfile.h"
#include "AkCaptureMgr.h"
#include <AK/Tools/Common/AkListBareLight.h>
namespace AK
{
	class IAkStdStream;
};

class CAkSinkXAudio2;
typedef AkListBareLight<CAkSinkXAudio2> XAudio2Sinks;

class IXAudio27;
// Returned by IXAudio2::GetDeviceDetails
typedef struct XAUDIO2_DEVICE_DETAILS
{
	WCHAR DeviceID[256];                // String identifier for the audio device.
	WCHAR DisplayName[256];             // Friendly name suitable for display to a human.
	int Role;           // Roles that the device should be used for.
	WAVEFORMATEXTENSIBLE OutputFormat;  // The device's native PCM audio output format.
} XAUDIO2_DEVICE_DETAILS;


class CXAudio2Base
{
public:
	CXAudio2Base():m_pXAudio2(NULL), m_pXAudio27(NULL){};
	AKRESULT CreateMasteringVoice(IXAudio2MasteringVoice** ppMasteringVoice,
		UINT32 InputChannels,
		UINT32 InputSampleRate,
		UINT32 Flags, IMMDevice * Device);

	HRESULT RegisterForCallbacks(IXAudio2EngineCallback* pCallback);
	void UnregisterForCallbacks(IXAudio2EngineCallback* pCallback);
	HRESULT CreateSourceVoice(IXAudio2SourceVoice** ppSourceVoice,
		const WAVEFORMATEX* pSourceFormat,
		UINT32 Flags X2DEFAULT(0),
		float MaxFrequencyRatio X2DEFAULT(XAUDIO2_DEFAULT_FREQ_RATIO),
		IXAudio2VoiceCallback* pCallback X2DEFAULT(NULL));

	HRESULT GetDeviceDetails(UINT32 Index, XAUDIO2_DEVICE_DETAILS* pDeviceDetails);
	IUnknown* GetIUnknown();

	HRESULT Init(IXAudio2 *in_pXAudio);
	void Term();
	HMODULE FindLoadedXAudioDLL();
	HMODULE LoadXAudioDLL();

	enum XAudio2Version
	{
		XAudio2NotFound,
		XAudio27,
		XAudio28,
		XAudio29,
	};

	static XAudio2Version m_XAudioVersion;

private:
	AKRESULT FindXAudio27DeviceIndexForIMMDevice(IMMDevice * in_pDevice, UINT32 &out_deviceIndex);
	bool FindDevicePathForIMMDevice(IMMDevice* in_pDevice, LPWSTR out_szID, AkUInt32 in_uReserved);

	IXAudio2* m_pXAudio2;
	IXAudio27* m_pXAudio27;
	static HMODULE m_hlibXAudio2;
};

#define AK_MAX_NUM_VOICE_BUFFERS 	(8)

class CAkSinkXAudio2 
	: public CAkSink
	, public IXAudio2EngineCallback
	, public IXAudio2VoiceCallback
{
public:
	CAkSinkXAudio2();
	~CAkSinkXAudio2();

	virtual AKRESULT Init(IMMDevice* in_pDevice, AkPluginID in_uPlugin, AkPlatformInitSettings & in_settings, const AkOutputSettings &in_OutputSettings, bool in_bRecordable);
	virtual AKRESULT Term( AK::IAkPluginMemAlloc * in_pAllocator );

	virtual AKRESULT Reset();
	virtual bool IsStarved();
	virtual void ResetStarved();

	virtual AKRESULT IsDataNeeded( AkUInt32 & out_uBuffersNeeded );
	
	// CAkSinkBase
	virtual void PassData();
	virtual void PassSilence();

	virtual AkAudioAPI GetType(){ return AkAPI_XAudio2; }

	//=================================================
	//	IXAudio2EngineCallback interface implementation
	//=================================================	 

	// Called by XAudio2 just before an audio processing pass begins.
    STDMETHOD_(void, OnProcessingPassStart) (THIS);

    // Called just after an audio processing pass ends.
    STDMETHOD_(void, OnProcessingPassEnd) (THIS);

    // Called in the event of a critical system error which requires XAudio2
    // to be closed down and restarted.  The error code is given in Error.
    STDMETHOD_(void, OnCriticalError) (THIS_ HRESULT Error);

	//=================================================
	//	IXAudio2VoiceCallback interface implementation
	//=================================================
	
	// Called just before this voice's processing pass begins.
    STDMETHOD_(void, OnVoiceProcessingPassStart) (THIS_ UINT32 BytesRequired);

    // Called just after this voice's processing pass ends.
    STDMETHOD_(void, OnVoiceProcessingPassEnd) (THIS);

    // Called when this voice has just finished playing a buffer stream
    // (as marked with the XAUDIO2_END_OF_STREAM flag on the last buffer).
    STDMETHOD_(void, OnStreamEnd) (THIS);

    // Called when this voice is about to start processing a new buffer.
    STDMETHOD_(void, OnBufferStart) (THIS_ void* pBufferContext);

    // Called when this voice has just finished processing a buffer.
    // The buffer can now be reused or destroyed.
    STDMETHOD_(void, OnBufferEnd) (THIS_ void* pBufferContext);

    // Called when this voice has just reached the end position of a loop.
    STDMETHOD_(void, OnLoopEnd) (THIS_ void* pBufferContext);

    // Called in the event of a critical error during voice processing,
    // such as a failing XAPO or an error from the hardware XMA decoder.
    // The voice may have to be destroyed and re-created to recover from
    // the error.  The callback arguments report which buffer was being
    // processed when the error occurred, and its HRESULT code.
    STDMETHOD_(void, OnVoiceError) (THIS_ void* pBufferContext, HRESULT Error);


	//=================================================
	//  Sharing IXAudio2 with the outside world.
	//=================================================

	// Returns the current, NON-ADDREF'd, XAudio2 interface currently being used by the Wwise SoundEngine.
	// This should be called only once the SoundEngine has been successfully initialized, otherwise
	// the function will always return NULL pointers. This function should not be called with 
	// AK::SoundEngine::Init() nor AK::SoundEngine::Term().
	// It is the responsability of the caller to AddRef (and release) the interface if there is a risk the 
	// sound engine be terminated before the called is done with the interface.
	// This function is not thread safe with AK::SoundEnigne::Init() nor AK::SoundEnigne::Term().
	//
	// XAudio2 allows creating multiple instances of XAudio2 objects, so systems are not forced to 
	// share the same IXAudio2 object, but can be useful to share some ressources.
	//
	// Xbox 360 can not have more than XAUDIO2_MAX_INSTANCES (8 at the time of writing this) 
	// simultaneous instances of XAudio2.
	static IUnknown* GetWwiseXAudio2Interface();

	CAkSinkXAudio2*			pNextLightItem;

protected:
	
	AkUInt16 IsDataNeededRB();
	bool SubmitPacketRB();
	AkUInt32 GetChannelCount();
	
	AkUInt32				m_ulNumChannels;

	CXAudio2Base			m_XAudio2;

	// the XAudio source voice
	IXAudio2MasteringVoice* m_pMasteringVoice;
	IXAudio2SourceVoice*	m_pSourceVoice;
	
	
	static XAudio2Sinks		s_XAudio2List;

	// buffer things
    void *					m_pvAudioBuffer;
	AkUInt16				m_uWriteBufferIndex;	// Ring buffer write index: this member belongs to the EventMgr thread
	AkUInt16				m_uReadBufferIndex;		// Ring buffer read index: this member belong to the XAudio2 callback thread
	void *					m_ppvRingBuffer[AK_MAX_NUM_VOICE_BUFFERS];	// Ring buffer
	LONG volatile			m_nbBuffersRB;			// Number of full buffers in ring buffer, must respect range: [0, AK_NUM_VOICE_BUFFERS]
	AkUInt16				m_usBlockAlign;	

	// status
	bool					m_bStarved;
	bool					m_bCriticalError;
};

#endif
