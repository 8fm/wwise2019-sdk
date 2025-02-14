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

#include <AK/Tools/Common/AkAssert.h>
#include <AK/Tools/Common/AkObject.h> // Placement new
#include "AkFlangerFX.h"
#include "Mix2Interp.h"
#include "UniComb.h"
#include "LFO.h"
#include <AK/AkWwiseSDKVersion.h>

// Plugin mechanism. Dynamic create function whose address must be registered to the FX manager.
AK::IAkPlugin* CreateAkFlangerFX( AK::IAkPluginMemAlloc * in_pAllocator )
{
	return AK_PLUGIN_NEW( in_pAllocator, CAkFlangerFX( ) );
}

// Constructor.
CAkFlangerFX::CAkFlangerFX() 
: m_pUniCombs(0)
, m_pLFO(0)
, m_pSharedParams(0)
, m_pAllocator(0)
, m_pFXCtx(0)
{
	AKPLATFORM::AkMemSet( &m_FXInfo.Params, 0, sizeof(m_FXInfo.Params) );
	AKPLATFORM::AkMemSet( &m_FXInfo.PrevParams, 0, sizeof(m_FXInfo.PrevParams) );
}

// Destructor.
CAkFlangerFX::~CAkFlangerFX()
{
	
}


#define RETURNIFNOTSUCCESS( __FONCTIONEVAL__ )	\
{												\
	AKRESULT __result__ = (__FONCTIONEVAL__);	\
	if ( __result__ != AK_Success )				\
		return __result__;						\
}												\

// Initializes and allocate memory for the effect
AKRESULT CAkFlangerFX::Init(	AK::IAkPluginMemAlloc * in_pAllocator,		// Memory allocator interface.
								AK::IAkEffectPluginContext * in_pFXCtx,		// FX Context
								AK::IAkPluginParam * in_pParams,			// Effect parameters.
								AkAudioFormat &	in_rFormat					// Required audio input format.
							   )
{	
	// get parameter state
	m_pSharedParams = static_cast<CAkFlangerFXParams*>(in_pParams);
	m_pSharedParams->GetParams( &m_FXInfo.Params );
	if ( !m_FXInfo.Params.NonRTPC.bEnableLFO )
		m_FXInfo.Params.RTPC.fModDepth = 0.f;
	m_FXInfo.PrevParams = m_FXInfo.Params;
	m_pFXCtx = in_pFXCtx;

	// LFE and true center (not mono) may or may not be processed.
	AkChannelConfig config = AdjustEffectiveChannelConfig( in_rFormat.channelConfig );
	m_FXInfo.uNumProcessedChannels = config.uNumChannels;
	m_FXInfo.uSampleRate = in_rFormat.uSampleRate;
	m_pAllocator = in_pAllocator;

	RETURNIFNOTSUCCESS( InitUniCombs( config ) );
	RETURNIFNOTSUCCESS( InitLFO( config ) );

	AK_PERF_RECORDING_RESET();

	// all set. RETURNIFNOTSUCCESS would have earlied us out if we didn't get here.
	return AK_Success;
}

AkChannelConfig CAkFlangerFX::AdjustEffectiveChannelConfig( AkChannelConfig in_channelConfig )
{
	if ( !m_FXInfo.Params.NonRTPC.bProcessLFE )
		in_channelConfig = in_channelConfig.RemoveLFE();
	
	if ( ((in_channelConfig.uChannelMask & AK_SPEAKER_SETUP_3_0) == AK_SPEAKER_SETUP_3_0) && !m_FXInfo.Params.NonRTPC.bProcessCenter )
		in_channelConfig = in_channelConfig.RemoveCenter();

	return in_channelConfig;
}

AKRESULT CAkFlangerFX::InitUniCombs( AkChannelConfig in_channelConfig )
{
	if ( in_channelConfig.uNumChannels )
	{
		// allocate per-channel unicombs
		m_pUniCombs = (DSP::UniComb *) AK_PLUGIN_ALLOC( m_pAllocator,  AK_ALIGN_SIZE_FOR_DMA( sizeof(DSP::UniComb) * in_channelConfig.uNumChannels ) );
		if ( !m_pUniCombs )
			return AK_InsufficientMemory;
		for(AkUInt32 i = 0; i < in_channelConfig.uNumChannels; ++i)
		{
			AkPlacementNew( &m_pUniCombs[i] ) DSP::UniComb() ;
		}

		// compute delay line length from delay time (ms)
		AkUInt32 uDelayLength = (AkUInt32)( m_FXInfo.Params.NonRTPC.fDelayTime / 1000.f * m_FXInfo.uSampleRate );

		// initialize unicombs for each processed channel
		for(AkUInt32 i = 0; i < in_channelConfig.uNumChannels; ++i)
		{
			RETURNIFNOTSUCCESS( m_pUniCombs[i].Init(	
				m_pAllocator, 
				uDelayLength,
				m_pFXCtx->GlobalContext()->GetMaxBufferLength(),
				m_FXInfo.Params.RTPC.fFbackLevel,
				m_FXInfo.Params.RTPC.fFfwdLevel,
				m_FXInfo.Params.RTPC.fDryLevel,
				m_FXInfo.Params.RTPC.fModDepth) );
		}
	}

	return AK_Success;
}

AKRESULT CAkFlangerFX::InitLFO( AkChannelConfig in_channelConfig )
{
	AKRESULT eResult = AK_Success;

	if ( m_FXInfo.Params.NonRTPC.bEnableLFO && in_channelConfig.uNumChannels )
	{
		// allocate lfo module
		m_pLFO = (FlangerLFO *) AK_PLUGIN_NEW( m_pAllocator, FlangerLFO() );
		if ( !m_pLFO )
			return AK_InsufficientMemory;

		eResult = m_pLFO->Setup( 
			m_pAllocator,
			m_pFXCtx->GlobalContext(),
			in_channelConfig,
			m_FXInfo.uSampleRate,
			m_FXInfo.Params.RTPC.modParams
			);
	}

	return eResult;
}

AKRESULT CAkFlangerFX::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	TermUniCombs();
	TermLFO();

	AK_PLUGIN_DELETE( in_pAllocator, this );
	return AK_Success;
}

void CAkFlangerFX::TermUniCombs( )
{
	if ( m_pUniCombs )
	{
		for(AkUInt32 i = 0; i < m_FXInfo.uNumProcessedChannels; ++i)
			m_pUniCombs[i].Term( m_pAllocator );

		AK_PLUGIN_FREE( m_pAllocator, m_pUniCombs ); // DSP::Unicomb::~Unicomb is trivial.
		m_pUniCombs = NULL;
	}
}

void CAkFlangerFX::TermLFO( )
{
	if ( m_pLFO )
	{
		m_pLFO->Release( m_pAllocator );
		AK_PLUGIN_DELETE( m_pAllocator, m_pLFO );
		m_pLFO = NULL;
	}
}

// Effect info query.
AKRESULT CAkFlangerFX::GetPluginInfo( AkPluginInfo & out_rPluginInfo )
{
	out_rPluginInfo.eType = AkPluginTypeEffect;
	out_rPluginInfo.bIsInPlace = true;
	out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
	return AK_Success;
}

// parameter updates from wwise
AKRESULT CAkFlangerFX::LiveParametersUpdate( AkAudioBuffer* io_pBuffer )
{
	AkChannelConfig config = AdjustEffectiveChannelConfig( io_pBuffer->GetChannelConfig() );
	bool bNumProcessChannelsChanged = config.uNumChannels != m_FXInfo.uNumProcessedChannels;
	if ( (m_FXInfo.PrevParams.NonRTPC.bEnableLFO != m_FXInfo.Params.NonRTPC.bEnableLFO) || bNumProcessChannelsChanged )
	{
		TermLFO();
		AKRESULT eResult = InitLFO( config );
		if ( eResult != AK_Success )
			return eResult;
	}

	if ( (m_FXInfo.PrevParams.NonRTPC.fDelayTime != m_FXInfo.Params.NonRTPC.fDelayTime) || bNumProcessChannelsChanged )
	{
		TermUniCombs();
		AKRESULT eResult = InitUniCombs( config );
		if ( eResult != AK_Success )
			return eResult;
		ResetUniCombs( config.uNumChannels );
	}

	m_FXInfo.uNumProcessedChannels = config.uNumChannels;

	return AK_Success; // No errors
}

// parameter updates from the game engine
void CAkFlangerFX::RTPCParametersUpdate( )
{
	for(AkUInt32 i = 0; i < m_FXInfo.uNumProcessedChannels; ++i)
	{
		m_pUniCombs[i].SetParams(	m_FXInfo.Params.RTPC.fFbackLevel,
									m_FXInfo.Params.RTPC.fFfwdLevel,
									m_FXInfo.Params.RTPC.fDryLevel,
									m_FXInfo.Params.RTPC.fModDepth );
	}

	if ( m_pLFO && m_FXInfo.Params.NonRTPC.bEnableLFO )
	{
		m_pLFO->SetParams( 
			m_FXInfo.uSampleRate,
			m_FXInfo.Params.RTPC.modParams.lfoParams
			);
	}
}

// main processing routine
void CAkFlangerFX::Execute( AkAudioBuffer * io_pBuffer )
{
	m_pSharedParams->GetParams( &m_FXInfo.Params );
	if ( !m_FXInfo.Params.NonRTPC.bEnableLFO )
		m_FXInfo.Params.RTPC.fModDepth = 0.f;

	// Support live change of non-RTPC parameters when connected to Wwise
	if(m_FXInfo.Params.NonRTPC.bHasChanged)
	{
		AKRESULT bError = LiveParametersUpdate( io_pBuffer );
		if ( bError != AK_Success )
			return;
		m_FXInfo.Params.NonRTPC.bHasChanged = false;
	}

	if(m_FXInfo.Params.RTPC.bHasChanged)
	{
		// RTPC parameter updates
		RTPCParametersUpdate( );
		m_FXInfo.Params.RTPC.bHasChanged = false;
	}

	if ( m_FXInfo.uNumProcessedChannels == 0 )
		return;

	AK_PERF_RECORDING_START( "Flanger", 25, 30 );

	AkUInt32 uDelayLength = (AkUInt32)( m_FXInfo.Params.NonRTPC.fDelayTime / 1000.f * m_FXInfo.uSampleRate );
	m_FXInfo.FXTailHandler.HandleTail( io_pBuffer, uDelayLength );

	if ( AK_EXPECT_FALSE(io_pBuffer->uValidFrames < 32) )	//Internal processes don't support less than 32.
		return;

	const AkUInt32 uNumFrames = io_pBuffer->uValidFrames;
	AkUInt32 uChanIndex = 0;

	AkChannelConfig config = io_pBuffer->GetChannelConfig();
	if ( !m_FXInfo.Params.NonRTPC.bProcessLFE )
		config = config.RemoveLFE();

	// Skip center channel if required.
	bool bSkipCenterChannel = ( !m_FXInfo.Params.NonRTPC.bProcessCenter && ((config.uChannelMask & AK_SPEAKER_SETUP_3_0) == AK_SPEAKER_SETUP_3_0) );
		
	AkReal32 * pfDryBuf = (AkReal32*)AK_PLUGIN_ALLOC( m_pAllocator, sizeof(AkReal32) * io_pBuffer->MaxFrames() );
	if ( pfDryBuf == NULL )
		return;

	const AkReal32 fCurrentDryGain = 1.f-m_FXInfo.PrevParams.RTPC.fWetDryMix;
	const AkReal32 fTargetDryGain = 1.f-m_FXInfo.Params.RTPC.fWetDryMix;
	const AkReal32 fCurrentWetGain = 1.f-fCurrentDryGain;
	const AkReal32 fTargetWetGain = 1.f-fTargetDryGain;

	if ( m_FXInfo.Params.NonRTPC.bEnableLFO )
	{
		// With LFO
		AkReal32 * pfLFOBuf = (AkReal32*)AK_PLUGIN_ALLOC( m_pAllocator,  sizeof(AkReal32) * uNumFrames );
		// Note: If allocation failed, proceed, but just avoid calling LFO::ProduceBuffer().
		
		const AkReal32 fPWM = m_FXInfo.Params.RTPC.modParams.lfoParams.fPWM;
		const AkReal32 fLFODepth = m_FXInfo.Params.RTPC.fModDepth;
		const AkReal32 fPrevLFODepth = m_FXInfo.PrevParams.RTPC.fModDepth;

		FlangerOutputPolicy dummy;	//Empty obj.

		for(AkUInt32 i = 0; i < config.uNumChannels; ++i)
		{
			// Skip over (true) center channel if present and not being processed
			if ( bSkipCenterChannel && i == AK_IDX_SETUP_3_CENTER )
				continue;

			// Produce a buffer of LFO data.
			if ( pfLFOBuf )
			{
				m_pLFO->GetChannel(uChanIndex).ProduceBuffer( 
					pfLFOBuf, 
					uNumFrames, 
					fLFODepth, 
					fPrevLFODepth,
					fPWM,
					dummy);
			}

			AkReal32 * pfChan = io_pBuffer->GetChannel(i);

			// Copy dry path
			AKPLATFORM::AkMemCpy(pfDryBuf, pfChan, uNumFrames*sizeof(AkReal32) ); 

			// Process unicomb, passing it the buffer with LFO data.
			m_pUniCombs[uChanIndex].ProcessBuffer(pfChan, uNumFrames, pfLFOBuf);
			uChanIndex++;

			// Wet/dry mix
			DSP::Mix2Interp(	
				pfChan, pfDryBuf, 
				fCurrentWetGain*m_FXInfo.PrevParams.RTPC.fOutputLevel, fTargetWetGain*m_FXInfo.Params.RTPC.fOutputLevel, 
				fCurrentDryGain*m_FXInfo.PrevParams.RTPC.fOutputLevel, fTargetDryGain*m_FXInfo.Params.RTPC.fOutputLevel, 
				uNumFrames );

///#define TEST_LFO
#ifdef TEST_LFO
			memcpy( pfChan, pfLFOBuf, uNumFrames * sizeof( AkReal32 ) );
#endif
		}

		if ( pfLFOBuf )
		{
			AK_PLUGIN_FREE( m_pAllocator, pfLFOBuf );
			pfLFOBuf = NULL;
		}
	}
	else
	{
		// No LFO (static filtering)
		for(AkUInt32 i = 0; i < config.uNumChannels; ++i)
		{
			// Skip over (true) center channel if present and not being processed
			if ( bSkipCenterChannel && i == AK_IDX_SETUP_3_CENTER )
				continue;

			AkReal32 * pfChan = io_pBuffer->GetChannel(i);

			// Copy dry path
			AKPLATFORM::AkMemCpy(pfDryBuf, pfChan, uNumFrames*sizeof(AkReal32) ); 

			m_pUniCombs[uChanIndex].ProcessBuffer(pfChan, uNumFrames, NULL);
			uChanIndex++;

			// Wet/dry mix
			DSP::Mix2Interp(	
				pfChan, pfDryBuf, 
				fCurrentWetGain*m_FXInfo.PrevParams.RTPC.fOutputLevel, fTargetWetGain*m_FXInfo.Params.RTPC.fOutputLevel, 
				fCurrentDryGain*m_FXInfo.PrevParams.RTPC.fOutputLevel, fTargetDryGain*m_FXInfo.Params.RTPC.fOutputLevel, 
				uNumFrames );
		}
	}

	AK_PLUGIN_FREE( m_pAllocator, pfDryBuf );
	pfDryBuf = NULL;

	m_FXInfo.PrevParams = m_FXInfo.Params;

	AK_PERF_RECORDING_STOP( "Flanger", 25, 30 );
}

AKRESULT CAkFlangerFX::Reset( )
{
	ResetUniCombs( m_FXInfo.uNumProcessedChannels );
	return AK_Success;
}

void CAkFlangerFX::ResetUniCombs( AkUInt32 in_uNumProcessedChannels )
{
	if ( m_pUniCombs )
	{
		for(AkUInt32 i = 0; i < in_uNumProcessedChannels; ++i)
			m_pUniCombs[i].Reset();
	}
}
