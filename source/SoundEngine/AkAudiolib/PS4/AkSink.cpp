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

#include "AkSink.h"

#include "AkSettings.h"
#include "AkCritical.h"
#include "BGMSinkParams.h"
#include "AkAudioMgr.h"
#include <audioout.h>
#include <user_service.h>
#include "AkBus.h"
#include "AkOutputMgr.h"
#include "AkFXMemAlloc.h"


extern AkPlatformInitSettings g_PDSettings;

AkThread					CAkSinkPS4::s_hAudioOutThread = AK_NULL_THREAD;		// Thread for all libAudio sinks.
CAkSinkPS4::LibAudioSinks	CAkSinkPS4::s_listSinks;
CAkLock						CAkSinkPS4::s_lockSinks;

//====================================================================================================
//====================================================================================================

AK::IAkPlugin* AkCreateDefaultSink(AK::IAkPluginMemAlloc * in_pAllocator)
{
	return AK_PLUGIN_NEW(in_pAllocator, CAkSinkPS4());
}

CAkSinkPS4::CAkSinkPS4()
	: CAkSinkBase()
	, m_port( 0 )
	, m_uType(0)	
	, m_uUserId(0)
	, m_pvAudioBuffer( NULL )
	, m_uReadBufferIndex (0)
	, m_uWriteBufferIndex( 0 )
	, m_uBuffers(0)
	, m_bStarved(false)
	, m_uBuffersReady(0)	
	, m_ManagedAudioOutInit( true )		
	, m_ePortState(PortNotInit)
{	
}

AKRESULT CAkSinkPS4::Init( 
	AK::IAkPluginMemAlloc *	in_pAllocator,				// Interface to memory allocator to be used by the effect.
	AK::IAkSinkPluginContext *	in_pSinkPluginContext,	// Interface to sink plug-in's context.
	AK::IAkPluginParam *		in_pParams,					// Interface to plug-in parameters.
	AkAudioFormat &			in_rFormat					// Audio data format of the input signal. 
	)
{
	// Determine audioOutPort Type and UserId
	AkUInt32 idDevice;
	AkPluginID idPlugin;
	in_pSinkPluginContext->GetOutputID(idDevice, idPlugin);
	idPlugin = AKGETPLUGINIDFROMCLASSID(idPlugin);

	uint32_t channelParam = 0;
	uint32_t recordingRestrictionParam = 0;
	AkUInt32 uType = SCE_AUDIO_OUT_PORT_TYPE_MAIN;
	AkChannelConfig defaultConfig; // defaultConfig will be used if the provided in_rFormat has an invalid channel config
	if (idPlugin == AKPLUGINID_DEFAULT_SINK)
	{
		m_uType = SCE_AUDIO_OUT_PORT_TYPE_MAIN; 
		idDevice = SCE_USER_SERVICE_USER_ID_SYSTEM;
		channelParam = SCE_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH_STD;
		defaultConfig.SetStandard(AK_SPEAKER_SETUP_7_1);
	}
	else if(idPlugin == AKPLUGINID_DVR_SINK)
	{
		if(in_pParams)
		{
			BGMSinkParams* pParams = (BGMSinkParams*)in_pParams;
			recordingRestrictionParam = pParams->m_bDVRRecordable ? 0 : SCE_AUDIO_OUT_PARAM_ATTR_RESTRICTED;
		}

		m_uType = SCE_AUDIO_OUT_PORT_TYPE_BGM;
		idDevice = SCE_USER_SERVICE_USER_ID_SYSTEM;
		channelParam = SCE_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH_STD;
		defaultConfig.SetStandard(AK_SPEAKER_SETUP_7_1);
	}
	else if(idPlugin == AKPLUGINID_COMMUNICATION_SINK)
	{
		m_uType = SCE_AUDIO_OUT_PORT_TYPE_VOICE; 
		channelParam = SCE_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO;
		defaultConfig.SetStandard(AK_SPEAKER_SETUP_STEREO);
	}
	else if (idPlugin == AKPLUGINID_PERSONAL_SINK)
	{
		m_uType = SCE_AUDIO_OUT_PORT_TYPE_PERSONAL; 
		channelParam = SCE_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO;
		defaultConfig.SetStandard(AK_SPEAKER_SETUP_STEREO);
	}
	else if (idPlugin == AKPLUGINID_PAD_SINK)
	{
		m_uType = SCE_AUDIO_OUT_PORT_TYPE_PADSPK;
		channelParam = SCE_AUDIO_OUT_PARAM_FORMAT_FLOAT_MONO;
		defaultConfig.SetStandard(AK_SPEAKER_SETUP_MONO);
	}
	else if(idPlugin == AKPLUGINID_AUX_SINK)
	{
		m_uType = SCE_AUDIO_OUT_PORT_TYPE_AUX;
		idDevice = SCE_USER_SERVICE_USER_ID_SYSTEM;
		channelParam = SCE_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO;
		defaultConfig.SetStandard(AK_SPEAKER_SETUP_STEREO);
	}
	else
	{
		AKASSERT( !"Invalid AkAudioOutputType" );
		return AK_Fail;
	}
	m_uUserId = idDevice;

	// masterOut buffer must be consistent with the number of channels the port is created with
	m_MasterOut.Clear();
	m_MasterOut.SetChannelConfig( defaultConfig );

	// Allocate ringBuffer
	m_uBuffers = in_pSinkPluginContext->GetNumRefillsInVoice();
	if (m_uBuffers == 0)
	{
		m_uBuffers == 2;
	}

	m_uOneBufferSize = GetRefillSamples() * sizeof(AkReal32);
    m_pvAudioBuffer = (AkUInt8*)AkAlloc( AkMemID_Processing, m_uBuffers * m_uOneBufferSize);
    if ( m_pvAudioBuffer == NULL )
    {
        return AK_Fail;
    }

	memset( m_pvAudioBuffer, 0, m_uBuffers*m_uOneBufferSize);

	// Baseline sceAudioOutInit
	int ret = sceAudioOutInit();
	if( ret == SCE_AUDIO_OUT_ERROR_ALREADY_INIT )
	{
		m_ManagedAudioOutInit = false;
	}
	else if( ret != SCE_OK )
	{
		return AK_Fail; 
	}


	// Create audioOutPort
	m_port = sceAudioOutOpen(
		m_uUserId,
		m_uType,
		0,
		AK_NUM_VOICE_REFILL_FRAMES,
		48000, // Must always be 48000 for SCE_AUDIO_OUT_PORT_TYPE_MAIN
		channelParam | recordingRestrictionParam );

	if ( m_port < 1 )
		return AK_Fail; // Keep m_port as-is, so you can look at the error value later in the debugger

	m_ePortState = PortOpen;

	// Double-check what the port's real configuration is, as we may need to fix up the requested channel config
	SceAudioOutPortState state;
	AKVERIFY( sceAudioOutGetPortState( m_port, &state) == SCE_OK );

	// Check of the "headphones disconnected" state (as per SceAudioOutPortState doc)
	// In this case, we'll simply init with the expected channel count
	if ( (state.output & SCE_AUDIO_OUT_STATE_OUTPUT_CONNECTED_EXTERNAL) && state.channel == SCE_AUDIO_OUT_STATE_CHANNEL_DISCONNECTED)
	{
		state.channel = defaultConfig.uNumChannels;
	}
	
	if (m_uType == SCE_AUDIO_OUT_PORT_TYPE_BGM && state.output == SCE_AUDIO_OUT_STATE_OUTPUT_UNKNOWN)
	{
		//This means the output is already overriden by the user-music
		//Mute the appropriate busses.
		CAkBus::MuteBackgroundMusic();
	}

	// If the requested channel config is not valid, or not compatible with the sceAudioOutPort's actual setup, override it as needed
	if (!in_rFormat.channelConfig.IsValid() || in_rFormat.channelConfig.uNumChannels == 1 ||
		defaultConfig.uNumChannels < in_rFormat.channelConfig.uNumChannels)
	{
		switch (state.channel)
		{
		case SCE_AUDIO_OUT_STATE_CHANNEL_1:
			in_rFormat.channelConfig.SetStandard(AK_SPEAKER_SETUP_MONO);
			break;
		case SCE_AUDIO_OUT_STATE_CHANNEL_2:
			in_rFormat.channelConfig.SetStandard(AK_SPEAKER_SETUP_STEREO);
			break;
		case SCE_AUDIO_OUT_STATE_CHANNEL_6:
			in_rFormat.channelConfig.SetStandard(AK_SPEAKER_SETUP_5POINT1);
			break;
		case SCE_AUDIO_OUT_STATE_CHANNEL_8:
			in_rFormat.channelConfig.SetStandard(AK_SPEAKER_SETUP_7POINT1);
			break;
		default:
			AKASSERT(!"Invalid channel config");
			return AK_Fail;
		}
	}
	m_speakersConfig = in_rFormat.channelConfig;

	// Silence every channel we're not outputting
	AkUInt32 uNumChannelsWwise = in_rFormat.channelConfig.uNumChannels;
	int32_t vol[SCE_AUDIO_OUT_CHANNEL_MAX];
	for (AkUInt32 i = 0; i < uNumChannelsWwise; i++)
	{
		vol[i] = SCE_AUDIO_VOLUME_0DB;
	}
	for (AkUInt32 i = uNumChannelsWwise; i < SCE_AUDIO_OUT_CHANNEL_MAX; ++i)
	{
		vol[i] = 0;
	}

	AKVERIFY(sceAudioOutSetVolume(m_port, (SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH
											| SCE_AUDIO_VOLUME_FLAG_C_CH | SCE_AUDIO_VOLUME_FLAG_LFE_CH
											| SCE_AUDIO_VOLUME_FLAG_LS_CH | SCE_AUDIO_VOLUME_FLAG_RS_CH
											| SCE_AUDIO_VOLUME_FLAG_LE_CH | SCE_AUDIO_VOLUME_FLAG_RE_CH), vol) == SCE_OK);

	// Important: must register first sink to thread before creating thread.
	RegisterSink( this );

	return AK_Success;
}

AKRESULT CAkSinkPS4::Reset()
{
	// Ensure LibAudio thread has been created.
	if ( !AKPLATFORM::AkIsValidThread(&s_hAudioOutThread) )
	{
		// Thread does not exist yet. Create it.
		AKPLATFORM::AkCreateThread(	
			AudioOutThreadFunc,					// Start address
			this,								// Parameter
			g_PDSettings.threadLEngine,			// Properties 
			&s_hAudioOutThread,					// AkHandle
			"AK::LibAudioOut" );				// Debug name
		if ( !AKPLATFORM::AkIsValidThread(&s_hAudioOutThread) )
		{
			// Thread creation failed. 
			return AK_Fail;
		}
	}
	return AK_Success;
}

AKRESULT CAkSinkPS4::Term( AK::IAkPluginMemAlloc * )
{
	bool bIsLast;
	bool bUnregistered = UnregisterSink( this, bIsLast );

	// At this point, 'this' may have been destroyed by output thread.

	if ( !bUnregistered )
	{
		// Port was opened but ownership was not transferred to output thread. 
		_Term();
	}

	// If we were the last sink and the audio out thread was running, wait until it joins.
	if ( bIsLast && AKPLATFORM::AkIsValidThread( &s_hAudioOutThread ) )
	{
		// No more sinks. Bail out.
		AKPLATFORM::AkWaitForSingleThread( &s_hAudioOutThread );
		AKPLATFORM::AkCloseThread( &s_hAudioOutThread );
		AKPLATFORM::AkClearThread( &s_hAudioOutThread );
	}

	return AK_Success;
}

void CAkSinkPS4::_Term()
{
	// Close port.
	if ( m_port > 0 )
	{
		// We know that we are not waiting for output, because this function is either called at the proper time
		// by the audio out thread, or there was an error and no data was ever posted to output.
		int ret = sceAudioOutClose( m_port );
		AKASSERT( ret >= SCE_OK );
	}

	if ( m_pvAudioBuffer )
	{
		AkFree( AkMemID_Processing, m_pvAudioBuffer );
		m_pvAudioBuffer = NULL;
	}

	CAkSinkBase::Term(AkFXMemAlloc::GetLower());
}

bool CAkSinkPS4::IsStarved()
{
	return m_bStarved;
}

void CAkSinkPS4::ResetStarved()
{	
	m_bStarved = false;
}

AKRESULT CAkSinkPS4::IsDataNeeded( AkUInt32 & out_uBuffersNeeded )
{
	out_uBuffersNeeded = m_uBuffers - m_uBuffersReady;
	return AK_Success;
}

void CAkSinkPS4::PassData()
{
	AKASSERT( m_port > 0 );
	AKASSERT( m_MasterOut.HasData() && m_MasterOut.uValidFrames == AK_NUM_VOICE_REFILL_FRAMES );

	// Increment write buffer index.
	AdvanceWritePointer();

	CheckBGMOverride();
}

void CAkSinkPS4::PassSilence()
{
	AKASSERT( m_port >= 1 );
	
	memset(GetRefillPosition(), 0, GetRefillSamples() * sizeof(AkReal32));

	// Increment write buffer index.
	AdvanceWritePointer();
	CheckBGMOverride();
}

void CAkSinkPS4::AdvanceReadPointer()
{
	if (m_uBuffersReady > 0)

	{
		m_uReadBufferIndex++;
		if (m_uReadBufferIndex >= m_uBuffers)
			m_uReadBufferIndex = 0;

		AkAtomicDec32(&m_uBuffersReady);
	}
	else
		m_bStarved = true;
}

void CAkSinkPS4::AdvanceWritePointer()
{	
	AKASSERT(m_uBuffersReady < m_uBuffers);
	
	m_uWriteBufferIndex++;
	if (m_uWriteBufferIndex >= m_uBuffers)
		m_uWriteBufferIndex = 0;

	AkAtomicInc32(&m_uBuffersReady);
}

void CAkSinkPS4::FillOutputParam( SceAudioOutOutputParam& out_rParam )
{			
	out_rParam.handle = m_port;
	if (m_ePortState == PortClosing)
	{
		out_rParam.ptr = NULL;
		m_ePortState = PortClosed;
	}
	else
	{
		out_rParam.ptr = m_pvAudioBuffer + m_uReadBufferIndex*m_uOneBufferSize;
	}
}

void CAkSinkPS4::RegisterSink( CAkSinkPS4 * in_pSink )
{
	AkAutoLock<CAkLock> lock( s_lockSinks );
	s_listSinks.AddLast( in_pSink );	
}

bool CAkSinkPS4::UnregisterSink( CAkSinkPS4 * in_pSink, bool & out_bIsLast )
{
	out_bIsLast = true;
	bool bFound = false;
	AkAutoLock<CAkLock> lock( s_lockSinks );
	LibAudioSinks::Iterator it = s_listSinks.Begin();
	while ( it != s_listSinks.End() )
	{		
		CAkSinkPS4 * pSink = (*it);
		if (in_pSink == pSink && pSink->m_ePortState == PortOpen)		// Found it. Mark as ready to close.	
		{
			bFound = true;
			pSink->m_ePortState = PortClosing;		
		}

		out_bIsLast = out_bIsLast && pSink->m_ePortState != PortOpen;	
		++it;
	}
	return bFound;
}

AK_DECLARE_THREAD_ROUTINE(CAkSinkPS4::AudioOutThreadFunc)
{
	AK_INSTRUMENT_THREAD_START( "CAkSinkPS4::AudioOutThreadFunc" );

	//Send all the buffers to the HW in one pass.
	// get our info from the parameter
	AkUInt32 uMaxOutputs = 0;
	do
	{
		s_lockSinks.Lock();
		uMaxOutputs = s_listSinks.Length();
		SceAudioOutOutputParam pOutputs[uMaxOutputs];

		for (int i = 0; i < uMaxOutputs; i++)
			s_listSinks[i]->FillOutputParam(pOutputs[i]);
		s_lockSinks.Unlock();

		if ( uMaxOutputs > 0 )
		{
			int res = sceAudioOutOutputs(pOutputs, uMaxOutputs);
			AKASSERT( res >= SCE_OK );

			AkAutoLock<CAkLock> lock( s_lockSinks );			
			for( int i = 0; i < s_listSinks.Length(); i++ )
			{
				if ( s_listSinks[i]->m_ePortState == PortClosed )
				{
					s_listSinks[i]->_Term();
					s_listSinks.Erase( i );
					i--;
				}
				else
					s_listSinks[i]->AdvanceReadPointer();
			}
			
			if ( g_pAudioMgr )
			{
				g_pAudioMgr->WakeupEventsConsumer();
			}
		}		
	}
	while ( uMaxOutputs > 0 );
	
	s_listSinks.Term();
	AkExitThread( AK_RETURN_THREAD_OK );
	
} // Perform

void CAkSinkPS4::CheckBGMOverride()
{
	if (m_uType == SCE_AUDIO_OUT_PORT_TYPE_BGM)
	{
		SceAudioOutPortState state;
		AKVERIFY( sceAudioOutGetPortState( m_port, &state) == SCE_OK );
		if (CAkBus::IsBackgroundMusicMuted() && state.output != SCE_AUDIO_OUT_STATE_OUTPUT_UNKNOWN)
		{
			//This means the output isn't overriden by the user music anymore.
			//Unmute the appropriate busses.
			CAkBus::UnmuteBackgroundMusic();
		}
		else if (!CAkBus::IsBackgroundMusicMuted() && state.output == SCE_AUDIO_OUT_STATE_OUTPUT_UNKNOWN)
		{
			//This means the output is overriden by the user-music
			//Mute the appropriate busses.
			CAkBus::MuteBackgroundMusic();
		}
	}
}

bool CAkSinkPS4::IsBGMOverriden()
{
	AkAutoLock<CAkLock> lock (s_lockSinks);
	LibAudioSinks::Iterator it = s_listSinks.Begin(); 
	while ( it != s_listSinks.End())
	{
		if ((*it)->m_uType == SCE_AUDIO_OUT_PORT_TYPE_BGM)
		{
			SceAudioOutPortState state;
			AKVERIFY( sceAudioOutGetPortState( (*it)->m_port, &state) == SCE_OK );
			return state.output == SCE_AUDIO_OUT_STATE_OUTPUT_UNKNOWN;
		}
		++it;
	}
	return false;
}

int CAkSinkPS4::GetPS4OutputHandle()
{
	return m_port;
}
