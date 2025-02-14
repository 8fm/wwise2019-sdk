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
// AkSink.cpp
//
// Platform dependent part
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <AK/SoundEngine/Platforms/Pellegrino/AkPlatformContext.h>

#include "AkSink.h"

#include "AkAudioMgr.h"
#include "AkBus.h"
#include "AkCritical.h"
#include "AkFXMemAlloc.h"
#include "AkOutputMgr.h"
#include "AkSettings.h"
#include "BGMSinkParams.h"

#include <audio_out2.h>
#include <user_service.h>

extern AkPlatformInitSettings g_PDSettings;
CAkSinkPellegrino::LibAudioSinks	CAkSinkPellegrino::s_listSinks;

//////////////////////////////////////////////////////////////////////////

// Helper for posting errors from APIs
#if !defined(AK_OPTIMIZED)
#define AKSINK_POST_MONITOR_ERROR(...) {\
	AkOSChar msg[256]; \
	AK_OSPRINTF(msg, sizeof(msg)/sizeof(AkOSChar), __VA_ARGS__); \
	MONITOR_ERRORMSG(msg); \
}
#else
#define AKSINK_POST_MONITOR_ERROR(...)
#endif

AK::IAkPlugin* AkCreateDefaultSink(AK::IAkPluginMemAlloc * in_pAllocator)
{
	return AK_PLUGIN_NEW(in_pAllocator, CAkSinkPellegrino());
}

CAkSinkPellegrino::CAkSinkPellegrino()
	: CAkSinkBase()
	, m_pPlatformContext(NULL)
	, m_hCtx(SCE_AUDIO_OUT2_CONTEXT_HANDLE_INVALID)
	, m_hPort(SCE_AUDIO_OUT2_PORT_HANDLE_INVALID)
	, m_hUser(SCE_AUDIO_OUT2_USER_HANDLE_INVALID)
	, m_uType(0)
	, m_pvAudioBuffer(NULL)
	, m_uBufferSize(0)
{	
}

AKRESULT CAkSinkPellegrino::Init( 
	AK::IAkPluginMemAlloc *	in_pAllocator,				// Interface to memory allocator to be used by the effect.
	AK::IAkSinkPluginContext *	in_pSinkPluginContext,	// Interface to sink plug-in's context.
	AK::IAkPluginParam *		in_pParams,					// Interface to plug-in parameters.
	AkAudioFormat &			io_rFormat					// Audio data format of the input signal. 
	)
{
	AkInt32 ret = 0;

	m_pPlatformContext = static_cast<AK::IAkPellegrinoContext*>(in_pSinkPluginContext->GlobalContext()->GetPlatformContext());
	if (m_pPlatformContext == NULL)
	{
		return AK_Fail;
	}

	m_hCtx = m_pPlatformContext->GetAudioOutContextHandle();
	if (m_hCtx == SCE_AUDIO_OUT2_CONTEXT_HANDLE_INVALID)
	{
		return AK_Fail;
	}

	AkUInt32 idDevice;
	AkPluginID idPlugin;
	in_pSinkPluginContext->GetOutputID(idDevice, idPlugin);
	idPlugin = AKGETPLUGINIDFROMCLASSID(idPlugin);

	uint32_t recordingRestrictionParam = 0;

	AkChannelConfig defaultConfig = io_rFormat.channelConfig;
	AkUInt32 uType = SCE_AUDIO_OUT2_PORT_TYPE_MAIN;
	if (idPlugin == AKPLUGINID_DEFAULT_SINK)
	{
		// Default: always 7.1 on the system side.
		defaultConfig.SetStandard(AK_SPEAKER_SETUP_7_1);
		m_uType = SCE_AUDIO_OUT2_PORT_TYPE_MAIN;
		idDevice = SCE_USER_SERVICE_USER_ID_SYSTEM;
	}
	else if (idPlugin == AKPLUGINID_DVR_SINK)
	{
		// BGM: always 7.1 on the system side.
		defaultConfig.SetStandard(AK_SPEAKER_SETUP_7_1);
		m_uType = SCE_AUDIO_OUT2_PORT_TYPE_BGM;
		idDevice = SCE_USER_SERVICE_USER_ID_SYSTEM;
	}
	else if (idPlugin == AKPLUGINID_COMMUNICATION_SINK)
	{
		m_uType = SCE_AUDIO_OUT2_PORT_TYPE_VOICE;
		defaultConfig.SetStandard(AK_SPEAKER_SETUP_STEREO);
	}
	else if (idPlugin == AKPLUGINID_PERSONAL_SINK)
	{
		m_uType = SCE_AUDIO_OUT2_PORT_TYPE_PERSONAL;
		defaultConfig.SetStandard(AK_SPEAKER_SETUP_STEREO);
	}
	else if (idPlugin == AKPLUGINID_PAD_SINK)
	{
		m_uType = SCE_AUDIO_OUT2_PORT_TYPE_PADSPK;
		defaultConfig.SetStandard(AK_SPEAKER_SETUP_MONO);
	}
	else if (idPlugin == AKPLUGINID_AUX_SINK)
	{
		m_uType = SCE_AUDIO_OUT2_PORT_TYPE_AUX;
		defaultConfig.SetStandard(AK_SPEAKER_SETUP_STEREO);
		idDevice = SCE_USER_SERVICE_USER_ID_SYSTEM;
	}
	else
	{
		AKASSERT(!"Invalid AkAudioOutputType");
		return AK_Fail;
	}

	// if the idDevice was not set to SYSTEM or provided by the plugin,
	// we should fetch the 0th user and grab that, instead.
	if (idDevice == 0)
	{
		SceUserServiceLoginUserIdList list;
		if (sceUserServiceGetLoginUserIdList(&list) == SCE_OK)
		{
			idDevice = list.userId[0];
		}
	}

	ret = sceAudioOut2UserCreate(idDevice, &m_hUser);
	if (ret != SCE_OK)
	{
		AKSINK_POST_MONITOR_ERROR("%s: sceAudioOut2UserCreate with userId %u failed: %x", __func__, idDevice, ret);
		return AK_Fail;
	}

	// Allocate buffer to move audio into, and cached pcm/attribute structs
	m_uBufferSize = defaultConfig.uNumChannels * AK_NUM_VOICE_REFILL_FRAMES * sizeof(AkReal32);
	m_pvAudioBuffer = AkAlloc(AkMemID_SoundEngine, m_uBufferSize);
	if (m_pvAudioBuffer == NULL)
	{
		return AK_InsufficientMemory;
	}
	memset(m_pvAudioBuffer, 0, m_uBufferSize);
	m_audioOutPcm.data = m_pvAudioBuffer;
	m_audioOutPcmAttribute.attributeId = SCE_AUDIO_OUT2_PORT_ATTRIBUTE_ID_PCM;
	m_audioOutPcmAttribute.value = &m_audioOutPcm;
	m_audioOutPcmAttribute.valueSize = sizeof(SceAudioOut2Pcm);

	// Master out buffer channel config must be consistent with what the hardware expects
	m_MasterOut.Clear();
	m_MasterOut.AttachInterleavedData(m_pvAudioBuffer, AK_NUM_VOICE_REFILL_FRAMES, 0, defaultConfig);

	// From documentation: two 7.1 formats available:
	// SCE_AUDIO_OUT2_DATA_FORMAT_FLOAT_8CH ==>		Float 32-bit 7.1 multi-channel (L-R-C-LFE-Lsurround-Rsurround-Lextend-Rextend interleaved)
	// SCE_AUDIO_OUT2_DATA_FORMAT_FLOAT_8CH_STD ==> Float 32-bit 7.1 multi-channel (L-R-C-LFE-Lextend-Rextend-Lsurround-Rsurround interleaved)
	const size_t NumGainChannels = 8;
	float gain[NumGainChannels];
	for (int i = 0; i < NumGainChannels; i++)
	{
		gain[i] = 1.0f;
	}

	uint32_t channelParam = 0;
	switch(defaultConfig.uNumChannels)
	{
	case 1: 
		/// REVIEW: Compatibility with main.
		channelParam = SCE_AUDIO_OUT2_DATA_FORMAT_FLOAT_MONO;
		break;
	case 2: 
		channelParam = SCE_AUDIO_OUT2_DATA_FORMAT_FLOAT_STEREO;
		break;
#if defined( AK_71FROM51MIXER )
	case 6:
		// Sink does not support 6 channels, just 8; set up output in SCE_AUDIO_OUT2_DATA_FORMAT_FLOAT_8CH
		// and let the final mix node fill the "extend" channels with zeros.
		channelParam = SCE_AUDIO_OUT2_DATA_FORMAT_FLOAT_8CH; 
		gain[6] = 0;
		gain[7] = 0;
		break;
#endif	
	case 8: 
		channelParam = SCE_AUDIO_OUT2_DATA_FORMAT_FLOAT_8CH_STD; 
		break;
	default:
		AKASSERT(!"Unsupported channel config");
		return AK_Fail;
	}

	SceAudioOut2PortParam portParams;
	portParams.portType = m_uType;
	portParams.dataFormat = channelParam;
	portParams.samplingFreq = 48000;
	portParams.userHandle = m_hUser;
	portParams.flags = 0;
	portParams.pad = 0;
	memset(portParams.reserved, 0, sizeof(portParams.reserved));

	// For DVR sinks, update the portParams flag to SCE_AUDIO_OUT2_PORT_PARAM_FLAG_RESTRICTED as needed
	if (idPlugin == AKPLUGINID_DVR_SINK && in_pParams && !((BGMSinkParams*)in_pParams)->m_bDVRRecordable)
	{
		portParams.flags |= SCE_AUDIO_OUT2_PORT_PARAM_FLAG_RESTRICTED;
	}

	// Get an audio out port
	ret = sceAudioOut2PortCreate(m_hCtx, &portParams, &m_hPort);
	if (ret != SCE_OK)
	{
		AKSINK_POST_MONITOR_ERROR("%s: Failed sceAudioOut2PortCreate: %x", __func__, ret);
		return AK_Fail;
	}

	// Check _real_ output config.
	m_speakersConfig = defaultConfig;	
	SceAudioOut2PortState state;
	ret = sceAudioOut2PortGetState(m_hPort, &state);
	if (ret != SCE_OK)
	{
		AKSINK_POST_MONITOR_ERROR("%s: Failed sceAudioOut2PortGetState: %x", __func__, ret);
		return AK_Fail;
	}
	
	if (m_uType == SCE_AUDIO_OUT2_PORT_TYPE_BGM && state.output == SCE_AUDIO_OUT2_PORT_STATE_OUTPUT_UNKNOWN)
	{
		//This means the output is already overriden by the user-music
		//Mute the appropriate busses.
		CAkBus::MuteBackgroundMusic();
	}

	// Use state.channel if DETECT (!IsValid()), otherwise force number of channels.
	AkUInt32 uNumChannelsWwise = (!io_rFormat.channelConfig.IsValid()) ? state.numChannels : io_rFormat.channelConfig.uNumChannels;
	switch ( uNumChannelsWwise )
	{
	case 1:
		// Mono to 7.1 not supported. This should only be used for mono controllers, which is already mono.
		AKASSERT( m_speakersConfig.uNumChannels == 1 );
		break;
	case 2:
		m_speakersConfig.SetStandard( AK_SPEAKER_SETUP_STEREO );
		break;
	case 6:
		m_speakersConfig.SetStandard( AK_SPEAKER_SETUP_5POINT1 );
		break;
	case 8:
		// Default case.
		AKASSERT( m_speakersConfig.uChannelMask == AK_SPEAKER_SETUP_7POINT1 );
		break;
	default:
		AKASSERT( !"Invalid channel config" );
		return AK_Fail;
	}

	io_rFormat.channelConfig = m_speakersConfig;

	// Apply the gain to the output port
	{
		for (int i = uNumChannelsWwise; i < NumGainChannels; ++i)
		{
			gain[i] = 0;
		}

		SceAudioOut2Attribute applyGain;
		applyGain.attributeId = SCE_AUDIO_OUT2_PORT_ATTRIBUTE_ID_GAIN;
		applyGain.value = gain;
		applyGain.valueSize = sizeof(gain);
		ret = sceAudioOut2PortSetAttributes(m_hPort, &applyGain, 1);
		if (ret != SCE_OK)
		{
			AKSINK_POST_MONITOR_ERROR("%s: Failed sceAudioOut2PortSetAttributes: %x", __func__, ret);
		}
	}

	RegisterSink( this );

	return AK_Success;
}

AKRESULT CAkSinkPellegrino::Reset()
{
	return AK_Success;
}

AKRESULT CAkSinkPellegrino::Term( AK::IAkPluginMemAlloc * )
{
	UnregisterSink(this);

	// Close port.
	if (m_hPort != SCE_AUDIO_OUT2_PORT_HANDLE_INVALID)
	{
		AkInt32 ret = sceAudioOut2PortDestroy(m_hPort);
		if (ret != SCE_OK)
		{
			AKSINK_POST_MONITOR_ERROR("%s: Failed sceAudioOut2PortDestroy: %x", __func__, ret);
		}
		m_hPort = SCE_AUDIO_OUT2_PORT_HANDLE_INVALID;
	}
	
	// destroy user
	if (m_hUser != SCE_AUDIO_OUT2_USER_HANDLE_INVALID)
	{
		AkInt32 ret = sceAudioOut2UserDestroy(m_hUser);
		// AK_PELLEGRINO_TODO sceAudioOut2UserDestroy may return ERROR_BUSY because userCreate w/ same id could have been called elsewhere (and same handle is returned)
		if (ret != SCE_OK && ret != SCE_AUDIO_OUT2_ERROR_BUSY)
		{
			AKSINK_POST_MONITOR_ERROR("%s: Failed sceAudioOut2UserDestroy: %x", __func__, ret);
		}
		m_hUser = SCE_AUDIO_OUT2_USER_HANDLE_INVALID;
	}

	if (m_pvAudioBuffer)
	{
		AkFree(AkMemID_SoundEngine, m_pvAudioBuffer);
		m_pvAudioBuffer = NULL;
	}

	CAkSinkBase::Term(AkFXMemAlloc::GetLower());

	return AK_Success;
}

bool CAkSinkPellegrino::IsStarved()
{
	AkUInt32 queueLevel = 0;
	AkUInt32 availableQueues = 0;
	AkInt32 ret = sceAudioOut2ContextGetQueueLevel(m_hCtx, &queueLevel, &availableQueues);
	if (ret != SCE_OK && ret != SCE_AUDIO_OUT2_ERROR_NOT_READY)
	{
		AKSINK_POST_MONITOR_ERROR("%s: Failed sceAudioOut2ContextGetQueueLevel: %x", __func__, ret);
	}
	return queueLevel == 0;
}

void CAkSinkPellegrino::ResetStarved()
{	
}

AKRESULT CAkSinkPellegrino::IsDataNeeded( AkUInt32 & out_uBuffersNeeded )
{
	AkUInt32 queueLevel = 0;
	AkInt32 ret = sceAudioOut2ContextGetQueueLevel(m_hCtx, &queueLevel, &out_uBuffersNeeded);
	if (ret != SCE_OK && ret != SCE_AUDIO_OUT2_ERROR_NOT_READY)
	{
		AKSINK_POST_MONITOR_ERROR("%s: Failed sceAudioOut2ContextGetQueueLevel: %x", __func__, ret);
		return AK_Fail;
	}
	return AK_Success;
}

void CAkSinkPellegrino::PassData()
{
	AKASSERT( m_hPort != SCE_AUDIO_OUT2_PORT_HANDLE_INVALID);
	AKASSERT( m_MasterOut.HasData() && m_MasterOut.uValidFrames == AK_NUM_VOICE_REFILL_FRAMES );

	CheckBGMOverride();

	AkInt32 ret = sceAudioOut2PortSetAttributes(m_hPort, &m_audioOutPcmAttribute, 1);
	if (ret != SCE_OK)
	{
		AKSINK_POST_MONITOR_ERROR("%s: Failed sceAudioOut2PortSetAttributes: %x", __func__, ret);
	}
}

void CAkSinkPellegrino::PassSilence()
{
	CheckBGMOverride();
}

void CAkSinkPellegrino::RegisterSink( CAkSinkPellegrino * in_pSink )
{
	s_listSinks.AddLast( in_pSink );	
}

void CAkSinkPellegrino::UnregisterSink( CAkSinkPellegrino * in_pSink )
{
	for (AkInt32 i = 0; i < s_listSinks.Length(); i++)
	{
		if (s_listSinks[i] == in_pSink)
		{
			s_listSinks.Erase(i);
			break;
		}
	}

	if (s_listSinks.Length() == 0)
	{
		s_listSinks.Term();
	}
}

void CAkSinkPellegrino::CheckBGMOverride()
{
	if (m_uType == SCE_AUDIO_OUT2_PORT_TYPE_BGM)
	{
		SceAudioOut2PortState state;
		AkInt32 ret = sceAudioOut2PortGetState(m_hPort, &state);
		if (ret != SCE_OK)
		{
			AKSINK_POST_MONITOR_ERROR("%s: Failed sceAudioOut2PortGetState: %x", __func__, ret);
		}
		if (CAkBus::IsBackgroundMusicMuted() && state.output != SCE_AUDIO_OUT2_PORT_STATE_OUTPUT_UNKNOWN)
		{
			//This means the output isn't overridden by the user music anymore.
			//Unmute the appropriate buses.
			CAkBus::UnmuteBackgroundMusic();
		}
		else if (!CAkBus::IsBackgroundMusicMuted() && state.output == SCE_AUDIO_OUT2_PORT_STATE_OUTPUT_UNKNOWN)
		{
			//This means the output is overridden by the user-music
			//Mute the appropriate busss.
			CAkBus::MuteBackgroundMusic();
		}
	}
}

bool CAkSinkPellegrino::IsBGMOverriden()
{
	LibAudioSinks::Iterator it = s_listSinks.Begin(); 
	while ( it != s_listSinks.End())
	{
		if ((*it)->m_uType == SCE_AUDIO_OUT2_PORT_TYPE_BGM)
		{
			SceAudioOut2PortState state;
			AkInt32 ret = sceAudioOut2PortGetState((*it)->m_hPort, &state);
			if (ret != SCE_OK)
			{
				AKSINK_POST_MONITOR_ERROR("%s: Failed sceAudioOut2PortGetState: %x", __func__, ret);
			}
			return state.output == SCE_AUDIO_OUT2_PORT_STATE_OUTPUT_UNKNOWN;
		}
		++it;
	}
	return false;
}

SceAudioOut2PortHandle CAkSinkPellegrino::GetAudioOut2PortHandle()
{
	return m_hPort;
}

