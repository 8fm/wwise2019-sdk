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

#include "stdafx.h"
#include "AkRecorderFX.h"
#include "AkRecorderFXHelpers.h"
#include "AkRecorderManager.h"
#include "AkMath.h"
#include "AkDSPUtils.h"
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include "AkSettings.h"
#include <AK/AkWwiseSDKVersion.h>

#if defined(AK_WIN) && defined(WWISE_AUTHORING)
#include <windows.h>
#include "Shlwapi.h"
#include "Shlobj.h"
#endif

namespace
{
	void ReportError( AK::IAkEffectPluginContext * in_pCtx )
	{
#if defined(AK_IOS) || defined(AK_ANDROID)
		char szError[] = "Recorder: Cannot create output file; was a writable path provided via <IOHookClass>::AddBasePath()?";
#else
		char szError[] = "Recorder: Cannot create output file";
#endif
		in_pCtx->PostMonitorMessage(szError, AK::Monitor::ErrorLevel_Error);
	}
}

// Plugin mechanism. Dynamic create function whose address must be registered to the FX manager.
AK::IAkPlugin* CreateAkRecorderFX( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AK::IAkPlugin* pPlugin = AK_PLUGIN_NEW( in_pAllocator, CAkRecorderFX( ) );
	return pPlugin;
}

// Constructor.
CAkRecorderFX::CAkRecorderFX()
	: m_pAllocator( NULL )
	, m_pParams( NULL )
	, m_pCtx( NULL )
	, m_pRecorderManager( NULL )
	, m_pStream( NULL )
	, m_pTempBuffer( NULL )
	, m_fDownstreamGain( 1.f )
	, m_bFirstDownstreamGain( true )
	, m_bStreamErrorReported( false )
{
}

// Destructor.
CAkRecorderFX::~CAkRecorderFX()
{
	if ( m_pRecorderManager )
	{
		if ( m_pStream ) 
			m_pRecorderManager->ReleaseStream( m_pStream );
		m_pRecorderManager->Release();
	}

	// m_pStream belongs to manager so we don't release it here
}

// Initializes and allocate memory for the effect
AKRESULT CAkRecorderFX::Init(
	AK::IAkPluginMemAlloc * in_pAllocator,		// Memory allocator interface.
	AK::IAkEffectPluginContext * in_pFXCtx,		// FX Context
	AK::IAkPluginParam * in_pParams,			// Effect parameters.
	AkAudioFormat &	in_rFormat					// Required audio input format.
	)
{
	m_pAllocator = in_pAllocator;
	m_pParams = static_cast<CAkRecorderFXParams*>(in_pParams);
	m_pCtx = in_pFXCtx;

	size_t len = AKPLATFORM::OsStrLen(m_pParams->NonRTPC.szFilename);
	if (len >= AK_MAX_PATH - 1)
		return AK_InvalidParameter;

	// 5.1 input channels are dumped in stereo if required.
	m_uSampleRate = in_rFormat.uSampleRate;
	m_configOut = in_rFormat.channelConfig;

	if (m_pParams->NonRTPC.bDownmixToStereo
		&& SupportsDownMix(in_rFormat.channelConfig))
	{
		m_configOut.SetStandard(AK_SPEAKER_SETUP_STEREO);
	}

	m_uNumChannels = m_configOut.uNumChannels;

	m_pRecorderManager = CAkRecorderManager::Instance(m_pAllocator, m_pCtx->GlobalContext());
	if (!m_pRecorderManager)
		return AK_Fail;

	m_pRecorderManager->AddRef();

	//If our file path is empty then quit now, gracefully
	const AkOSChar sz = (AkOSChar)'\0';
	if( m_pParams->NonRTPC.szFilename[0] == sz )
		return AK_Success;

#if defined(AK_WIN) && defined(WWISE_AUTHORING)

	//When recording in Authoring we want to use an absolute path.
	//If the retreived path is relative, make it absolute (starting from the users documents).
	if ( PathIsRelative(m_pParams->NonRTPC.szFilename) )
	{
		//Get the current directory
		TCHAR tcBaseDirectory[AK_MAX_PATH];
		HRESULT result = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, tcBaseDirectory);
		if (result != S_OK)
			GetCurrentDirectory(AK_MAX_PATH, tcBaseDirectory);

		//Combine the base directory with the relative input path
		TCHAR tcAbsolutePath[AK_MAX_PATH];
		PathCombine(tcAbsolutePath, tcBaseDirectory, m_pParams->NonRTPC.szFilename);

		//If filepath is empty when PathCombine cannot fill the output string
		const AkOSChar sz = (AkOSChar)'\0';
		if (tcAbsolutePath[0] == sz)
			return AK_InvalidParameter;

		AKPLATFORM::SafeStrCpy(m_pParams->NonRTPC.szFilename, tcAbsolutePath, AK_MAX_PATH);
	}

#endif

	m_pTempBuffer = (AkInt16 *)AK_PLUGIN_ALLOC(in_pAllocator, m_uNumChannels * sizeof(AkInt16)* in_pFXCtx->GlobalContext()->GetMaxBufferLength());
	if ( !m_pTempBuffer )
	{
		return AK_Fail;
	}

	return AK_Success;
}

// Terminates.
AKRESULT CAkRecorderFX::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	if ( m_pTempBuffer )
	{
		AK_PLUGIN_FREE( in_pAllocator, m_pTempBuffer );
	}

	AK_PLUGIN_DELETE( in_pAllocator, this );

	return AK_Success;
}

// Reset
AKRESULT CAkRecorderFX::Reset( )
{
	return AK_Success;
}

// Effect info query.
AKRESULT CAkRecorderFX::GetPluginInfo( AkPluginInfo & out_rPluginInfo )
{
	out_rPluginInfo.eType = AkPluginTypeEffect;
	out_rPluginInfo.bIsInPlace = true;
	out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
	return AK_Success;
}

bool CAkRecorderFX::InitializeStream()
{
	// If the file path is empty, then do nothing
	const AkOSChar sz = (AkOSChar)'\0';
	if( m_pParams->NonRTPC.szFilename[0] == sz )
		return false;

	// If we've already tried to create the stream and failed, then bother trying again.
	if( m_bStreamErrorReported )
		return false;

	AK::IAkStreamMgr * pStreamMgr = m_pCtx->GlobalContext()->GetStreamMgr();
	if ( pStreamMgr )
	{
		AkFileSystemFlags fsFlags;
		fsFlags.uCompanyID = AKCOMPANYID_AUDIOKINETIC;
		fsFlags.uCodecID = AKCODECID_PCM;
		fsFlags.bIsLanguageSpecific = false;
		fsFlags.pCustomParam = NULL;
		fsFlags.uCustomParamSize = 0;

		AKRESULT eResult = pStreamMgr->CreateStd(m_pParams->NonRTPC.szFilename, &fsFlags, AK_OpenModeWriteOvrwr, m_pStream, false);
		if (eResult == AK_Success)
		{
			if (m_pRecorderManager->AddStream(m_pStream, m_configOut, m_uSampleRate, m_pParams->NonRTPC.iFormat))
				return true;

			m_pStream->Destroy();
			m_pStream = NULL;
		}
	}

	ReportError( m_pCtx );
	m_bStreamErrorReported = true;

	return false;
}

bool CAkRecorderFX::SupportsDownMix(const AkChannelConfig& in_channelConfig)
{
	AkChannelMask uChannelMask = in_channelConfig.uChannelMask & AK_SPEAKER_SETUP_DEFAULT_PLANE;

	if( in_channelConfig.eConfigType == AK_ChannelConfigType_Standard )
	{
		if (
#ifdef AK_71AUDIO
			uChannelMask == AK_SPEAKER_SETUP_7 ||
			uChannelMask == AK_SPEAKER_SETUP_7POINT1 ||
			uChannelMask == AK_SPEAKER_SETUP_6 ||
			uChannelMask == AK_SPEAKER_SETUP_6POINT1 ||
#endif
			uChannelMask == AK_SPEAKER_SETUP_5 ||
			uChannelMask == AK_SPEAKER_SETUP_5POINT1 ||
			uChannelMask == AK_SPEAKER_SETUP_4 ||
			uChannelMask == AK_SPEAKER_SETUP_4POINT1 ||
			uChannelMask == AK_SPEAKER_SETUP_3_0 ||
			uChannelMask == AK_SPEAKER_SETUP_3POINT1 ||
			uChannelMask == AK_SPEAKER_SETUP_2_0 ||
			uChannelMask == AK_SPEAKER_SETUP_2POINT1 ||
			uChannelMask == AK_SPEAKER_SETUP_1POINT1 ||
			uChannelMask == AK_SPEAKER_SETUP_0_1
			)
		{
			return true;
		}
	}

	return false;
}

void CAkRecorderFX::ProcessDownMix(AkAudioBuffer * in_pBuffer, AkReal32 in_fGain, AkReal32 in_fGainInc)
{
	AkUInt32 cFrames = in_pBuffer->uValidFrames;
	AkChannelMask uChannelMask = in_pBuffer->GetChannelConfig().uChannelMask & AK_SPEAKER_SETUP_DEFAULT_PLANE;

	//Get the scaling values
	AkReal32 fLinCenter = AK::dBToLin(m_pParams->RTPC.fCenter);
	AkReal32 fLinFront = AK::dBToLin(m_pParams->RTPC.fFront);
	AkReal32 fLinSurround = AK::dBToLin(m_pParams->RTPC.fSurround);
	AkReal32 fLinRear = AK::dBToLin(m_pParams->RTPC.fRear);
	AkReal32 fLinLFE = AK::dBToLin(m_pParams->RTPC.fLFE);

	InitBuffer(m_pTempBuffer + 0, cFrames, 2);
	InitBuffer(m_pTempBuffer + 1, cFrames, 2);

	switch (uChannelMask)
	{
#ifdef AK_71AUDIO //The ifdef in AkSpeakerConfig includes both 7.x and 6.x setups, not sure if this is a mistake.
		case AK_SPEAKER_SETUP_7:
		case AK_SPEAKER_SETUP_7POINT1:
			// Push Center to Left-Right
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_7_CENTER), cFrames, fLinCenter, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_7_CENTER), cFrames, fLinCenter, in_fGain, in_fGainInc, 1, 2);

			// Push Front
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_7_FRONTLEFT), cFrames, fLinFront, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_7_FRONTRIGHT), cFrames, fLinFront, in_fGain, in_fGainInc, 1, 2);

			//Push Rear
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_7_REARLEFT), cFrames, fLinRear, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_7_REARRIGHT), cFrames, fLinRear, in_fGain, in_fGainInc, 1, 2);

			// Push Surround
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_7_SIDELEFT), cFrames, fLinSurround, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_7_SIDERIGHT), cFrames, fLinSurround, in_fGain, in_fGainInc, 1, 2);
			break;

		case AK_SPEAKER_SETUP_6:
		case AK_SPEAKER_SETUP_6POINT1:
			// Push Front
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_6_FRONTLEFT), cFrames, fLinFront, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_6_FRONTRIGHT), cFrames, fLinFront, in_fGain, in_fGainInc, 1, 2);

			//Push Rear
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_6_REARLEFT), cFrames, fLinRear, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_6_REARRIGHT), cFrames, fLinRear, in_fGain, in_fGainInc, 1, 2);

			// Push Surround
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_6_SIDELEFT), cFrames, fLinSurround, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_6_SIDERIGHT), cFrames, fLinSurround, in_fGain, in_fGainInc, 1, 2);
			break;
#endif
		case AK_SPEAKER_SETUP_5:
		case AK_SPEAKER_SETUP_5POINT1:
			// Push Center to Left-Right
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_5_CENTER), cFrames, fLinCenter, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_5_CENTER), cFrames, fLinCenter, in_fGain, in_fGainInc, 1, 2);

			// Push Front
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_5_FRONTLEFT), cFrames, fLinFront, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_5_FRONTRIGHT), cFrames, fLinFront, in_fGain, in_fGainInc, 1, 2);

			//Push Rear (note: using fLinSurround is not a bug)
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_5_REARLEFT), cFrames, fLinSurround, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_5_REARRIGHT), cFrames, fLinSurround, in_fGain, in_fGainInc, 1, 2);
			break;

		case AK_SPEAKER_SETUP_4:
		case AK_SPEAKER_SETUP_4POINT1:
			// Push Front
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_4_FRONTLEFT), cFrames, fLinFront, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_4_FRONTRIGHT), cFrames, fLinFront, in_fGain, in_fGainInc, 1, 2);

			//Push Rear (note: using fLinSurround is not a bug)
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_4_REARLEFT), cFrames, fLinSurround, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_4_REARRIGHT), cFrames, fLinSurround, in_fGain, in_fGainInc, 1, 2);
			break;

		case AK_SPEAKER_SETUP_3_0:
		case AK_SPEAKER_SETUP_3POINT1:
			// Push Center to Left-Right
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_3_CENTER), cFrames, fLinCenter, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_3_CENTER), cFrames, fLinCenter, in_fGain, in_fGainInc, 1, 2);

			// Push Front
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_3_LEFT), cFrames, fLinFront, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_3_RIGHT), cFrames, fLinFront, in_fGain, in_fGainInc, 1, 2);
			break;

		case AK_SPEAKER_SETUP_2_0:
		case AK_SPEAKER_SETUP_2POINT1:
			// Push Front
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_2_LEFT), cFrames, fLinFront, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_2_RIGHT), cFrames, fLinFront, in_fGain, in_fGainInc, 1, 2);
			break;

		case AK_SPEAKER_SETUP_0_1:
			// Push LFE
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_0_LFE), cFrames, fLinLFE, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_0_LFE), cFrames, fLinLFE, in_fGain, in_fGainInc, 1, 2);
			break;

		case AK_SPEAKER_SETUP_MONO:
		case AK_SPEAKER_SETUP_1POINT1:
			// Push Center
			AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetChannel(AK_IDX_SETUP_1_CENTER), cFrames, fLinCenter, in_fGain, in_fGainInc, 1, 2);
			AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetChannel(AK_IDX_SETUP_1_CENTER), cFrames, fLinCenter, in_fGain, in_fGainInc, 1, 2);
			break;

		default:
			AKASSERT(false); //Case missing?
		break;
	}

	if (in_pBuffer->HasLFE())
	{
		AddFloatsToShorts(m_pTempBuffer + 0, in_pBuffer->GetLFE(), cFrames, fLinLFE, in_fGain, in_fGainInc, 1, 2);
		AddFloatsToShorts(m_pTempBuffer + 1, in_pBuffer->GetLFE(), cFrames, fLinLFE, in_fGain, in_fGainInc, 1, 2);
	}

}

void CAkRecorderFX::Execute( AkAudioBuffer * io_pBuffer ) 
{
	AkUInt32 cFrames = io_pBuffer->uValidFrames;
	if ( !cFrames )
		return;

	//Initialize the stream on the first execute call.
	//This cannot be done in init, as it would create an empty WAV
	//even if the effect is bypassed.
	if (!m_pStream) 
	{
		if (!InitializeStream())
			return;
	}

	// Get applied gain ramp
	AkReal32 fGain = 1.f;
	AkReal32 fGainInc = 0.f;

	if( m_pParams->NonRTPC.bApplyDownstreamVolume )
	{
		if( m_bFirstDownstreamGain )
		{
			m_bFirstDownstreamGain = false;
			m_fDownstreamGain = m_pCtx->GetDownstreamGain();
			fGain = m_fDownstreamGain;
		}
		else
		{
			fGain = m_fDownstreamGain;
			m_fDownstreamGain = m_pCtx->GetDownstreamGain();
		}
		fGainInc = (m_fDownstreamGain - fGain) / cFrames;
	}

	AkChannelMask uChannelMask = io_pBuffer->GetChannelConfig().uChannelMask & AK_SPEAKER_SETUP_DEFAULT_PLANE;
	if (m_pParams->NonRTPC.bDownmixToStereo
		&& SupportsDownMix( io_pBuffer->GetChannelConfig() ) )
	{
		ProcessDownMix(io_pBuffer, fGain, fGainInc);
	}
	else
	{
		if ( uChannelMask & AK_SPEAKER_LOW_FREQUENCY
			&& io_pBuffer->GetChannelConfig().eConfigType == AK_ChannelConfigType_Standard )
		{
			// Has LFE. Switch LFE back to its rightful place, after C (if appl.)

			// Has center: 1 if 1.1, 3 if 3/5/7.1
			// No center: 0 if 0.1, 2 if 2/4/6.1
			AkUInt32 uOutputLFEPos = ( uChannelMask & AK_SPEAKER_FRONT_RIGHT ) ? 2 : 0;
			if ( uChannelMask & AK_SPEAKER_FRONT_CENTER )
				++uOutputLFEPos;

			// First channels, then LFE, then following channels until penultimate (LFE in)
			for ( AkUInt32 uChan = 0; uChan < uOutputLFEPos; uChan++ )
			{
				FloatsToShorts( m_pTempBuffer + uChan, io_pBuffer->GetChannel( uChan ), cFrames, fGain, fGainInc, 1, m_uNumChannels );
			}
			
			AkUInt32 uLFEIdx = io_pBuffer->NumChannels() - 1;
			FloatsToShorts( m_pTempBuffer + uOutputLFEPos, io_pBuffer->GetChannel( uLFEIdx ), cFrames, fGain, fGainInc, 1, m_uNumChannels );
			
			for ( AkUInt32 uChan = uOutputLFEPos; uChan < uLFEIdx; uChan++ )
			{
				FloatsToShorts( m_pTempBuffer + uChan + 1, io_pBuffer->GetChannel( uChan ), cFrames, fGain, fGainInc, 1, m_uNumChannels );
			}
		}
		else if (AkFileParser::IsFuMaCompatible(io_pBuffer->GetChannelConfig()) &&
				 (AmbisonicChannelOrder)m_pParams->NonRTPC.iAmbisonicChannelOrdering == FuMa)	// ignore iAmbisonicChannelOrdering with orders > 3. Never FuMa.
		{
			// Convert ACN+SN3D to FuMa.
			void * pTemp = AK_PLUGIN_ALLOC(m_pAllocator, cFrames * io_pBuffer->NumChannels() * sizeof(AkReal32));
			if (pTemp)
			{
				memset(pTemp, 0, cFrames * io_pBuffer->NumChannels() * sizeof(AkReal32));
				AkUInt32 uSizeMx = AK::SpeakerVolumes::Matrix::GetRequiredSize(io_pBuffer->GetChannelConfig().uNumChannels, io_pBuffer->GetChannelConfig().uNumChannels);
				AK::SpeakerVolumes::MatrixPtr mxConv = (AK::SpeakerVolumes::MatrixPtr)AkAlloca(uSizeMx);
				AkFileParser::GetWwiseToFuMaConversionMx(io_pBuffer->GetChannelConfig(), mxConv);
				AkAudioBuffer temp;
				temp.AttachContiguousDeinterleavedData(pTemp, (AkUInt16)cFrames, 0, io_pBuffer->GetChannelConfig());
				AkRamp ones;
				m_pCtx->GlobalContext()->MixNinNChannels(io_pBuffer, &temp, 1.f, 1.f, mxConv, mxConv);

				for (AkUInt32 uChan = 0; uChan < io_pBuffer->NumChannels(); uChan++)
				{
					FloatsToShorts(m_pTempBuffer + uChan, temp.GetChannel(uChan), cFrames, fGain, fGainInc, 1, m_uNumChannels);
				}

				AK_PLUGIN_FREE(m_pAllocator, pTemp);
			}
		}
		else
		{
			for ( AkUInt32 uChan = 0; uChan < io_pBuffer->NumChannels(); uChan++ )
			{
				FloatsToShorts( m_pTempBuffer + uChan, io_pBuffer->GetChannel( uChan ), cFrames, fGain, fGainInc, 1, m_uNumChannels );
			}
		}
	}

	if( ! m_pRecorderManager->Record(m_pStream, m_pTempBuffer, cFrames * sizeof(AkInt16) * m_uNumChannels) )
	{
		//Stream seems unresponsive
		if( ! m_bStreamErrorReported )
		{
			ReportError( m_pCtx );
			m_bStreamErrorReported = true;
		}
	}
}

