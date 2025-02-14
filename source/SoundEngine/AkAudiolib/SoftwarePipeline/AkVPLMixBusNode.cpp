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

/////////////////////////////////////////////////////////////////////
//
// AkVPLMixBusNode.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkVPLMixBusNode.h"
#include "AkEffectsMgr.h"
#include "AkFXMemAlloc.h"
#include "AkAudioLibTimer.h"
#include "AkLEngine.h"
#include "AkMeterTools.h"
#include "AkPlayingMgr.h"
#include "AkRegistryMgr.h"
#include "AkRuntimeEnvironmentMgr.h"
#include "AkOutputMgr.h"
#include "AkVBAP.h"
#include "AkPipelineID.h"

#include "AkMixBusCtx.h"
#include "IAkGlobalPluginContextEx.h"
#include "AkCustomPluginDataStore.h"

extern AkInitSettings		g_settings;

static inline void ZeroPadBuffer(AkAudioBuffer * io_pAudioBuffer)
{
	// Extra frames should be padded with 0's
	AkUInt16 uValidFrames = io_pAudioBuffer->uValidFrames;
	AkUInt32 uPadFrames = io_pAudioBuffer->MaxFrames() - uValidFrames;
	if (uPadFrames)
	{
		// Do all channels
		AkUInt32 uNumChannels = io_pAudioBuffer->NumChannels();
		for (unsigned int uChanIter = 0; uChanIter < uNumChannels; ++uChanIter)
		{
			AkSampleType * pPadStart = io_pAudioBuffer->GetChannel(uChanIter) + uValidFrames;
			for (unsigned int uFrameIter = 0; uFrameIter < uPadFrames; ++uFrameIter)
			{
				pPadStart[uFrameIter] = 0.f;
			}
		}

		io_pAudioBuffer->uValidFrames = io_pAudioBuffer->MaxFrames();
	}
}

CAkBusVolumes::~CAkBusVolumes()
{
}

void CAkBusVolumes::_GetOutputID(AkUInt32 & out_uOutputID, AkUInt32 & out_uDeviceType) const
{
	out_uDeviceType = (AkUInt32)-1;
	out_uOutputID = (AkUInt32)-1;
	AkDevice * pDevice = CAkOutputMgr::FindDevice(m_BusContext);
	if (pDevice != NULL)
	{
		pDevice->GetOutputID(out_uOutputID, out_uDeviceType);		
	}
}

#ifndef AK_OPTIMIZED
AkReal32 CAkBusVolumes::BehavioralVolume() const
{
	return m_fBehavioralVolume;
}
#endif

//
// CAkBusFX
//
CAkBusFX::CAkBusFX()
: m_pMixerPlugin( NULL )
, m_iBypassAllFX( 0 )
, m_iLastBypassAllFX( 0 )
, m_bEffectCreated( false )
, m_bBypassChanged( false )
{
	for ( AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex )
	{
		FX & fx = m_aFX[ uFXIndex ];
		fx.id = AK_INVALID_PLUGINID;
		fx.pParam = NULL;
		fx.pEffect = NULL;
		fx.pBusFXContext = NULL;
		fx.iBypass = 0;
		fx.iLastBypass = 0;

#ifndef AK_OPTIMIZED
		if (CAkBehavioralCtx* pCtx = GetContext())
		{
			fx.UpdateContextID(pCtx->GetSoundID(), pCtx->GetPipelineID());
		}
#endif
	}
}

CAkBusFX::~CAkBusFX()
{
	DropFx();
	DropMixerPlugin();
	if ( m_pMixerPlugin )
	{
		AkDelete( AkMemID_Processing, m_pMixerPlugin );
		m_pMixerPlugin = NULL;
	}
}

void CAkBusFX::_SetInsertFx( AkUInt32 in_uFXIndex, AkAudioFormat& io_Format )
{
	_DropFx( in_uFXIndex );

	AkFXDesc fxDesc;

	CAkBusCtxRslvd resolvedBus = m_BusContext;
	resolvedBus.GetFX(in_uFXIndex, fxDesc);

	if( !fxDesc.pFx )
		return; // no new effect

	FX & fx = m_aFX[ in_uFXIndex ];

	fx.id = fxDesc.pFx->GetFXID();

	AK::IAkPluginParam * pParam = fxDesc.pFx->GetFXParam();
	if ( !pParam )
		return;

	AKRESULT eResult = AK_Fail;

	// Use temp format in case effect fails.
	AkAudioFormat format = io_Format;
	AkPluginInfo PluginInfo;
	CAkBusCtx* pBusCtx = (CAkBusCtx*)&m_BusContext;
	AK::Monitor::ErrorCode errCode;

#ifndef AK_OPTIMIZED
	fx.UpdateContextID(GetContext()->GetSoundID(), GetContext()->GetPipelineID());
#endif

	if (!fx.Clone(fxDesc.pFx, GetContext()->GetRTPCKey(), &GetContext()->GetModulatorData()))
	{
		MONITOR_BUS_PLUGIN_ERROR(AK::Monitor::ErrorCode_PluginAllocationFailed, pBusCtx, fx.id);
		goto check_failure;
	}

	fx.iBypass = fxDesc.iBypassed;

	fx.pBusFXContext = AkNew(AkMemID_Processing, CAkBusFXContext(this, in_uFXIndex, m_BusContext));
	if ( !fx.pBusFXContext )
	{
		MONITOR_BUS_PLUGIN_ERROR(AK::Monitor::ErrorCode_PluginAllocationFailed, pBusCtx, fx.id);
		goto check_failure;
	}

	if (CAkEffectsMgr::Alloc(fx.id, (IAkPlugin*&)fx.pEffect, PluginInfo) != AK_Success)
	{
		MONITOR_BUS_PLUGIN_ERROR(AK::Monitor::ErrorCode_PluginAllocationFailed, pBusCtx, fx.id);
		goto check_failure;
	}

	errCode = CAkEffectsMgr::ValidatePluginInfo(fxDesc.pFx->GetFXID(), AkPluginTypeEffect, PluginInfo);
	if (errCode != AK::Monitor::ErrorCode_NoError)
	{
		MONITOR_BUS_PLUGIN_ERROR(errCode, pBusCtx, fx.id);
		goto check_failure;
	}

	{
		AKRESULT eInitResult = fx.pEffect->Init(
			AkFXMemAlloc::GetLower(),		// Memory allocator.
			fx.pBusFXContext,			// Bus FX context.
			fx.pParam,
			format );
		if ( eInitResult != AK_Success )
		{
			switch ( eInitResult )
			{
			case AK_UnsupportedChannelConfig:
				MONITOR_BUS_PLUGIN_ERROR( AK::Monitor::ErrorCode_PluginUnsupportedChannelConfiguration, pBusCtx, fx.id );
				break;
			case AK_PluginMediaNotAvailable:
				MONITOR_BUS_PLUGIN_ERROR( AK::Monitor::ErrorCode_PluginMediaUnavailable, pBusCtx, fx.id );
				break;
			default:
				MONITOR_BUS_PLUGIN_ERROR( AK::Monitor::ErrorCode_PluginInitialisationFailed, pBusCtx, fx.id );
				break;
			}
			goto check_failure;
		}

		eResult = AK_Fail;

		if (PluginInfo.bIsInPlace)
		{
			// In-place: verify that plugin hasn't tried to change the format, per contract.
			// If it did then it probably is trying to do something wrong which will result in more problems later on.
			if (format != io_Format)
			{
				CAkBusCtx* pBusCtx = (CAkBusCtx*)&m_BusContext;
				AKASSERT(!"In-place effects cannot change format.");
				MONITOR_BUS_PLUGIN_ERROR(AK::Monitor::ErrorCode_PluginExecutionInvalid, pBusCtx, fx.id);
				goto check_failure;
			}
		}
		else
		{
			AkUInt16 uMaxFrames = LE_MAX_FRAMES_PER_BUFFER;

			m_aFxBuffer[in_uFXIndex].eState = AK_NoMoreData;
			AkUInt32 uBufferOutSize = uMaxFrames * format.GetNumChannels() * sizeof(AkReal32);
			void * pData = AkMalign(AkMemID_Processing, uBufferOutSize, AK_BUFFER_ALIGNMENT);
			if (!pData)
			{
				eResult = AK_InsufficientMemory;
				goto check_failure;
			}

			AkZeroMemLarge(pData, uBufferOutSize);

			m_aFxBuffer[in_uFXIndex].AttachContiguousDeinterleavedData(pData, uMaxFrames, 0, format.channelConfig);
		}
	}

	// Set eResult to AK_Success if this last operation succeeded.
	eResult = fx.pEffect->Reset();

#ifndef AK_OPTIMIZED
	AkDeltaMonitor::OpenSoundBrace(GetContext()->GetSoundID(), GetContext()->GetPipelineID());
	fx.RecapRTPCParams();
	AkDeltaMonitor::CloseSoundBrace();
#endif

check_failure:
	if (eResult == AK_Success)
	{
		// Success: can trust the effect.

		//*** Don't change the format if the effect is bypassed.
		if (!fx.iBypass && !m_iBypassAllFX)
			io_Format = format;
	}
	else
		_DropFx(in_uFXIndex);
}

void CAkBusFX::SetMixerPlugin()
{
	AKASSERT( m_pMixerPlugin );

	DropMixerPlugin();

	AkFXDesc fxDesc;
	m_BusContext.GetMixerPlugin( fxDesc );

	if( !fxDesc.pFx )
		return; // no new effect

	m_pMixerPlugin->id = fxDesc.pFx->GetFXID();

	AK::IAkPluginParam * pParamTemplate = fxDesc.pFx->GetFXParam();
	if ( !pParamTemplate )
		return;

	AKRESULT eResult = AK_Fail;
	AkPluginInfo pluginInfo;
	AK::Monitor::ErrorCode errCode;
	CAkBusCtx* pBusCtx = (CAkBusCtx*)&m_BusContext;

#ifndef AK_OPTIMIZED
	// Context must be set before Clone, we need it for profiling RTPC subscriptions
	m_pMixerPlugin->UpdateContextID(GetContext()->GetSoundID(), GetContext()->GetPipelineID());
#endif

	if (!m_pMixerPlugin->Clone(fxDesc.pFx, GetContext()->GetRTPCKey(), &GetContext()->GetModulatorData()))
	{
		MONITOR_BUS_PLUGIN_ERROR(AK::Monitor::ErrorCode_PluginAllocationFailed, pBusCtx, m_pMixerPlugin->id);
		goto check_failure;
	}

	if (CAkEffectsMgr::Alloc(m_pMixerPlugin->id, (IAkPlugin*&)m_pMixerPlugin->pPlugin, pluginInfo) != AK_Success)
	{
		MONITOR_BUS_PLUGIN_ERROR(AK::Monitor::ErrorCode_PluginAllocationFailed, pBusCtx, m_pMixerPlugin->id);
		goto check_failure;
	}

	errCode = CAkEffectsMgr::ValidatePluginInfo(m_pMixerPlugin->id, AkPluginTypeMixer, pluginInfo);
	if (errCode != AK::Monitor::ErrorCode_NoError)
	{
		// We have an old plug-in at run time and we will crash when trying to clear things up... Turning a sure crash into a possible memory leak... Allowing the user to notice the error message.
		if (errCode == AK::Monitor::ErrorCode_PluginVersionMismatch)
		{
			if (AKGETCOMPANYIDFROMCLASSID(m_pMixerPlugin->id) == 266)
			{
				// WG-33733
				// It was reported some plug-ins from previous version will make crash when used if we call Term() on them without calling Init().
				// We will ask them to fix that, but until then, we need to prevent this crash so people can see the error message and update the plug-in to a valid version.
				// Following recent changes, we cannot call Init on them, therefore will avoid calling term Too.
				// Standard Expectation to handle this is to go to the destructor of the object.
				AK_PLUGIN_DELETE(AkFXMemAlloc::GetLower(), m_pMixerPlugin->pPlugin);
			}
			m_pMixerPlugin->pPlugin = NULL;// Don't use this illegal interface. This could cause memory leak but prevents crashes.
		}
		MONITOR_BUS_PLUGIN_ERROR(errCode, pBusCtx, m_pMixerPlugin->id);
		goto check_failure;
	}

	{
		AkAudioFormat l_Format;
		l_Format.SetAll( AK_CORE_SAMPLERATE,
				m_MixBuffer.GetChannelConfig(),
				AK_LE_NATIVE_BITSPERSAMPLE,
				AK_LE_NATIVE_BITSPERSAMPLE/8*m_MixBuffer.GetChannelConfig().uNumChannels,
				AK_FLOAT,
				AK_NONINTERLEAVED );

		AKRESULT eInitResult = m_pMixerPlugin->pPlugin->Init(
			AkFXMemAlloc::GetLower(),		// Memory allocator.
			this,							// Mixer FX context.
			m_pMixerPlugin->pParam,
			l_Format );
		if ( eInitResult == AK_Success )
		{
			// Set eResult to AK_Success if this last operation succeeded.
			eResult = m_pMixerPlugin->pPlugin->Reset();
		}
		else
		{
			switch ( eInitResult )
			{
			case AK_UnsupportedChannelConfig:
				MONITOR_BUS_PLUGIN_ERROR( AK::Monitor::ErrorCode_PluginUnsupportedChannelConfiguration, pBusCtx, m_pMixerPlugin->id );
				break;
			case AK_PluginMediaNotAvailable:
				MONITOR_BUS_PLUGIN_ERROR( AK::Monitor::ErrorCode_PluginMediaUnavailable, pBusCtx, m_pMixerPlugin->id );
				break;
			default:
				MONITOR_BUS_PLUGIN_ERROR( AK::Monitor::ErrorCode_PluginInitialisationFailed, pBusCtx, m_pMixerPlugin->id );
				break;
			}
		}
	}

#ifndef AK_OPTIMIZED
	AkDeltaMonitor::OpenSoundBrace(GetContext()->GetSoundID(), GetContext()->GetPipelineID());
	m_pMixerPlugin->RecapRTPCParams();
	AkDeltaMonitor::CloseSoundBrace();
#endif

check_failure:
	if ( eResult != AK_Success )
		DropMixerPlugin();
}

void CAkBusFX::SetInsertFxBypass( AkUInt32 in_bitsFXBypass, AkUInt32 in_uTargetMask )
{
	for ( AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex )
	{
		if ( in_uTargetMask & ( 1 << uFXIndex ) )
		{
			SetFxBypass(uFXIndex, (( in_bitsFXBypass & ( 1 << uFXIndex ) ) != 0));
		}
	}

	if ( in_uTargetMask & ( 1 << AK_NUM_EFFECTS_BYPASS_ALL_FLAG ) )
	{
		SetFxBypassAll( ( in_bitsFXBypass & ( 1 << AK_NUM_EFFECTS_BYPASS_ALL_FLAG ) ) != 0 );
	}
}

void CAkBusFX::DropFx( )
{
	for ( AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex )
		_DropFx(uFXIndex);

	m_bEffectCreated = false;
	m_bBypassChanged = false;
}

void CAkBusFX::_DropFx( AkUInt32 in_uFXIndex )
{
	FX & fx = m_aFX[ in_uFXIndex ];

	// Delete any existing effect
	if( fx.pEffect != NULL )
	{
		fx.pEffect->Term( AkFXMemAlloc::GetLower() );
		fx.pEffect = NULL;
	}

	if ( fx.pBusFXContext )
	{
		AkDelete( AkMemID_Processing, fx.pBusFXContext );
		fx.pBusFXContext = NULL;
	}

	fx.id = AK_INVALID_PLUGINID;
	fx.Term();

	{
		AkPipelineBufferBase& effectBuffer = m_aFxBuffer[in_uFXIndex];
		if (effectBuffer.HasData())
		{
			AkFalign(AkMemID_Processing, effectBuffer.GetContiguousDeinterleavedData());
			effectBuffer.ClearData();
			effectBuffer.Clear();
			effectBuffer.SetChannelConfig(AkChannelConfig());
		}
	}
}


void CAkBusFX::DropMixerPlugin()
{
	// Delete any existing plugin
	if ( m_pMixerPlugin )
	{
		if ( m_pMixerPlugin->pPlugin )
		{
			m_pMixerPlugin->pPlugin->Term( AkFXMemAlloc::GetLower() );
			m_pMixerPlugin->pPlugin = NULL;
		}

		m_pMixerPlugin->Term();
		m_pMixerPlugin->id = AK_INVALID_PLUGINID;
	}
}

AkPluginID CAkBusFX::GetMixerID()
{
	if(m_pMixerPlugin)
	{
		return m_pMixerPlugin->id;
	}
	else
	{
		return AK_INVALID_PLUGINID;
	}
}

void CAkBusFX::RefreshMeterWatch()
{
#if !defined(AK_EMSCRIPTEN)
	AkUInt8 uMonitorMeterTypes =
#ifndef AK_OPTIMIZED
		(AkMonitor::GetMeterWatchDataTypes(ID()) & AkMonitorData::BusMeterMask_RequireContext);
#else
		0;
#endif

	// GetMeterWatchDataTypes() returns metering values required by monitoring.
	// "OR" it with metering values required by SDK.
	AkUInt8 uFlags = m_eMeteringCallbackFlags | m_eMeteringPluginFlags | uMonitorMeterTypes;

	AkPipelineBufferBase * pBuffer = &m_MixBuffer;

	for (AkInt32 i = AK_NUM_EFFECTS_PER_OBJ-1; i >= 0; --i)
	{
		if (m_aFxBuffer[i].HasData() && !(m_aFX[i].iBypass || m_iBypassAllFX))
		{
			pBuffer = &m_aFxBuffer[i];
			break;
		}
	}

	m_MeterCtxFactory.SetParameters(pBuffer->GetChannelConfig(), (AkMeteringFlags)uFlags);

#endif
}

AkChannelConfig CAkBusFX::GetOutputConfig() const
{
	AkChannelConfig chCfg = m_MixBuffer.GetChannelConfig();
	if (!m_iBypassAllFX)
	{
		for (AkInt32 i = AK_NUM_EFFECTS_PER_OBJ - 1; i >= 0; --i)
		{
			if (!m_aFX[i].iBypass && m_aFxBuffer[i].HasData())
			{
				chCfg = m_aFxBuffer[i].GetChannelConfig();
				break;
			}
		}
	}
	return chCfg;
}

//
// IAkPluginContextBase implementation
//
AKRESULT CAkBusFX::GetOutputID(AkUInt32 & out_uOutputID, AkUInt32 & out_uDeviceType) const
{
	_GetOutputID(out_uOutputID, out_uDeviceType);
	return AK_Success;
}

void CAkBusFX::GetPluginMedia(
	AkUInt32 in_dataIndex,		///< Index of the plug-in media to be returned.
	AkUInt8* &out_rpData,		///< Pointer to the data
	AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
	)
{
	// Search for already existing data pointers.
	AkDataReference* pDataReference = m_dataArray.Exists( in_dataIndex );
	if( !pDataReference )
	{
		// Get the source ID
		AkUInt32 l_dataSourceID = AK_INVALID_SOURCE_ID;
		m_BusContext.GetMixerPluginDataID( in_dataIndex, l_dataSourceID );

		// Get the pointers
		if( l_dataSourceID != AK_INVALID_SOURCE_ID )
			pDataReference = m_dataArray.AcquireData( in_dataIndex, l_dataSourceID );
	}

	if( pDataReference )
	{
		// Setting what we found.
		out_rDataSize = pDataReference->uSize;
		out_rpData = pDataReference->pData;
	}
	else
	{
		// Clean the I/Os variables, there is no data available.
		out_rpData = NULL;
		out_rDataSize = 0;
	}
}

// Implements AK::IAkMixerPluginContext::GetPluginCustomGameData(). Returns mixer plugin custom data.
void CAkBusFX::GetPluginCustomGameData(
		void* &out_rpData,			///< Pointer to the data
		AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
		)
{
	AkCustomPluginDataStore::GetPluginCustomGameData(ID(), m_BusContext.HasBus() ? m_BusContext.GameObjectID() : AK_INVALID_GAME_OBJECT, (m_pMixerPlugin) ? m_pMixerPlugin->GetPluginID() : AK_INVALID_PLUGINID, out_rpData, out_rDataSize);
}

void CAkBusFX::GetPluginCustomGameDataForInsertFx(
	AkUInt32 in_uFXIndex,
	void* &out_rpData,			///< Pointer to the data
	AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
	)
{
	// Get plugin ID on this slot.
	AkGameObjectID objectID = m_BusContext.HasBus() ? m_BusContext.GameObjectID() : AK_INVALID_GAME_OBJECT;
	AkCustomPluginDataStore::GetPluginCustomGameData(ID(), objectID, m_aFX[in_uFXIndex].GetPluginID(), out_rpData, out_rDataSize);
}

AKRESULT CAkBusFX::PostMonitorData(
	void *		in_pData,		///< Blob of data.
	AkUInt32	in_uDataSize	///< Size of data.
	)
{
#ifndef AK_OPTIMIZED
	if( m_pMixerPlugin )
	{
		MONITOR_PLUGINSENDDATA(in_pData, in_uDataSize, m_BusContext.ID(), m_BusContext.HasBus() ? m_BusContext.GameObjectID() : AK_INVALID_GAME_OBJECT, m_pMixerPlugin->id, 0 /** unused **/);
	}

	return AK_Success;
#else
	return AK_Fail;
#endif
}

bool CAkBusFX::CanPostMonitorData()
{
	// Same as bus effect
#ifndef AK_OPTIMIZED
	return AkMonitor::IsMonitoring();
#else
	return false;
#endif
}

AKRESULT CAkBusFX::PostMonitorMessage(const char* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel)
{
#ifndef AK_OPTIMIZED
	if( m_pMixerPlugin )
	{
		MONITOR_BUS_PLUGIN_MESSAGE( in_pszError, in_eErrorLevel, m_BusContext.ID(), m_pMixerPlugin->id, m_pMixerPlugin->pFx->ID() );
	}
	return AK_Success;
#else
	return AK_Fail;
#endif
}

AkReal32 CAkBusFX::GetDownstreamGain()
{
	return AK::dBToLin(m_fDownstreamGainDB);
}

AKRESULT CAkBusFX::GetParentChannelConfig(AkChannelConfig& out_channelConfig) const
{
	CAkBusFX* pThis = const_cast<CAkBusFX*>(this); // We should have const iterators, but we don't.
	for (AkMixConnectionList::Iterator it = pThis->BeginConnection();
		it != pThis->EndConnection(); ++it)
	{
		AkMixConnection* pConnection = *it;
		if (pConnection->IsTypeDirect())
		{
			// Return the channel config of the first direct output connection parent. 
			//	This is sufficient (they are all the same) until we add the ability to change configs on specific instances.
			out_channelConfig = pConnection->GetOutputBus()->GetMixConfig();
			break;
		}
		else
		{
			// If there are only aux connections, why not return it.
			out_channelConfig = pConnection->GetOutputBus()->GetMixConfig();
		}
	}
	return out_channelConfig.IsValid() ? AK_Success : AK_Fail;
}

#if (defined AK_CPU_X86 || defined AK_CPU_X86_64) && !(defined AK_IOS)
IAkProcessorFeatures * CAkBusFX::GetProcessorFeatures()
{
	return AkRuntimeEnvironmentMgr::Instance();
}
#endif

//
// IAkMixerPluginContext implementation
//
AkUInt32 CAkBusFX::GetBusType()
{
	//Obsolete, kept for compatibility.
	return 0;
}

AKRESULT CAkBusFX::GetSpeakerAngles(
	AkReal32 *			io_pfSpeakerAngles,			///< Returned array of loudspeaker pair angles, in degrees relative to azimuth [0,180]. Pass NULL to get the required size of the array.
	AkUInt32 &			io_uNumAngles,				///< Returned number of angles in io_pfSpeakerAngles, which is the minimum between the value that you pass in, and the number of angles corresponding to the output configuration, or just the latter if io_pfSpeakerAngles is NULL.
	AkReal32 &			out_fHeightAngle			///< Elevation of the height layer, in degrees relative to the plane.
	)
{
	AkDevice * pDevice = CAkOutputMgr::FindDevice(m_BusContext);
	if (pDevice != NULL)
		return pDevice->GetSpeakerAngles(io_pfSpeakerAngles, io_uNumAngles, out_fHeightAngle);
	else
		return AK_Fail;
}

/// Compute volume matrix with proper downmixing rules between two channel configurations.
AKRESULT CAkBusFX::ComputeSpeakerVolumesDirect(
	AkChannelConfig		in_inputConfig,				///< Channel configuration of the input.
	AkChannelConfig		in_outputConfig,			///< Channel configuration of the mixer output.
	AkReal32			in_fCenterPerc,				///< Center percentage. Only applies to mono inputs to outputs that have no center.
	AK::SpeakerVolumes::MatrixPtr out_mxVolumes		///< Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services).
	)
{
	((IAkGlobalPluginContextEx*)AK::SoundEngine::GetGlobalPluginContext())->ComputeSpeakerVolumesDirect(
		in_inputConfig,				///< Channel configuration of the input.
		in_outputConfig,			///< Channel configuration of the mixer output.
		in_fCenterPerc,				///< Center percentage. Only applies to mono inputs to outputs that have no center.
		out_mxVolumes		///< Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services).
		);
	return AK_Success;
}

AKRESULT CAkBusFX::ComputeSpeakerVolumesPanner(
	const AkVector &	in_position,				///< x,y,z panner position [-1,1]. Note that z has no effect at the moment.
	AkReal32			in_fCenterPct,				///< Center percentage.
	AkChannelConfig		in_inputConfig,				///< Channel configuration of the input.
	AkChannelConfig		in_outputConfig,			///< Channel configuration of the mixer output.
	AK::SpeakerVolumes::MatrixPtr out_mxVolumes		///< Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services).
	)
{
	// For 2D panning, it does not matter whether clients use side or back channels. However, our internal panner only support surround configs
	// defined with side channels. Fix it now.
	if ( in_inputConfig.eConfigType == AK_ChannelConfigType_Standard )
		in_inputConfig.uChannelMask = AK::BackToSideChannels( in_inputConfig.uChannelMask );
	if ( in_outputConfig.eConfigType == AK_ChannelConfigType_Standard )
		in_outputConfig.uChannelMask = AK::BackToSideChannels( in_outputConfig.uChannelMask );

	// Convert coordinates for internal 2D pan.
	AkReal32 x = AkClamp( in_position.X, -1.f, 1.f ) * 0.5f + 0.5f;
	AkReal32 y = AkClamp( in_position.Y, -1.f, 1.f ) * 0.5f + 0.5f;
	
	AkDevice* pDevice = CAkOutputMgr::FindDevice(m_BusContext);
	if (pDevice != NULL)
	{
		CAkSpeakerPan::GetSpeakerVolumes2DPan(
			x,			// [0..1] // 0 = full left, 1 = full right
			y,			// [0..1] // 0 = full rear, 1 = full front
			in_fCenterPct,	// [0..1]
			AK_BalanceFadeHeight,
			in_inputConfig,
			in_outputConfig,
			out_mxVolumes,
			pDevice);

		return AK_Success;
	}
	else
	{
		return AK_Fail;
	}
}

AKRESULT CAkBusFX::ComputePlanarVBAPGains(
	AkReal32		in_fAngle,					///< Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are clockwise)
	AkChannelConfig	in_outputConfig,			///< Desired output configuration.
	AkReal32		in_fCenterPerc,				///< Center percentage. Only applies to mono inputs to outputs that have no center.
	AK::SpeakerVolumes::VectorPtr out_vVolumes	///< Returned volumes (see AK::SpeakerVolumes::Vector services). Must be allocated prior to calling this function with the size returned by AK::SpeakerVolumes::Vector::GetRequiredSize() for the desired configuration.
	)
{
	AkDevice * pDevice = CAkOutputMgr::FindDevice(m_BusContext);
	if (pDevice != NULL)
		return CAkSpeakerPan::ComputePlanarVBAPGains(pDevice, in_fAngle, in_outputConfig, in_fCenterPerc, out_vVolumes);
	else
		return AK_Fail;
}

AKRESULT CAkBusFX::InitSphericalVBAP(
	AK::IAkPluginMemAlloc* in_pAllocator,					///< Memory allocator
	const AkSphericalCoord* in_SphericalPositions,			///< Array of points in spherical coordinate, representign the virtual position of each channels.
	const AkUInt32 in_NbPoints,								///< Number of points in the position array
	void *& out_pPannerData									///< Contains data relevant to the 3D panner that shoud be re-used accross plugin instances.
	)
{
	AkVBAP::PushVertices( in_pAllocator, in_SphericalPositions, in_NbPoints, out_pPannerData);
	if (!out_pPannerData)
		return AK_Fail;
	return AK_Success;
}

AKRESULT CAkBusFX::ComputeSphericalVBAPGains(
	void*				in_pPannerData,				///< Contains data relevant to the 3D panner that shoud be re-used accross plugin instances.
	AkReal32			in_fAzimuth,				///< Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are clockwise)
	AkReal32			in_fElevation,				///< Incident angle, in radians [0,pi], where 0 is the elevation (positive values are clockwise)
	AkUInt32			in_uNumChannels,			///< Desired output configuration.
	AK::SpeakerVolumes::VectorPtr out_vVolumes		///< Returned volumes (see AK::SpeakerVolumes::Vector services). Must be allocated prior to calling this function with the size returned by AK::SpeakerVolumes::Vector::GetRequiredSize() for the desired configuration.
	)
{
	AkVBAP::ComputeVBAP( in_pPannerData, in_fAzimuth, in_fElevation, in_uNumChannels, out_vVolumes );

	return AK_Success;
}

AKRESULT CAkBusFX::TermSphericalVBAP(
			AK::IAkPluginMemAlloc* in_pAllocator,	///< Memory allocator
			void*				in_pPannerData		///< Contains data relevant to the 3D panner that shoud be re-used accross plugin instances.
			)
{
	AkVBAP::Term( in_pAllocator,in_pPannerData );
	return AK_Success;
}

AKRESULT CAkBusFX::_Compute3DPositioning(
	const AkTransform & in_emitter,					// Emitter transform.
	AkReal32			in_fCenterPerc,				// Center percentage. Only applies to mono inputs to outputs that have a center.
	AkReal32			in_fSpread,					// Spread.
	AkReal32			in_fFocus,					// Focus.
	AK::SpeakerVolumes::MatrixPtr io_mxVolumes,		// Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services). Assumes zeroed.
	AkChannelConfig		in_inputConfig,				// Channel configuration of the input.
	AkChannelMask		in_uInputChanSel,			// Mask of input channels selected for panning (excluded input channels don't contribute to the output).
	AkChannelConfig		in_outputConfig,			// Desired output configuration.
	const AkVector &	in_listPosition,			// listener position
	const AkReal32		in_listRot[3][3]			// listener rotation
	)
{
	AKASSERT(in_outputConfig.uNumChannels > 0);
	AkDevice * pDevice = CAkOutputMgr::FindDevice(m_BusContext);
	if (pDevice == NULL)
		return AK_Fail;

	AKRESULT eResult = pDevice->EnsurePanCacheExists(in_outputConfig);
	if (eResult == AK_Success)
	{
		// Input config: Ignore LFE and height channels (TEMP height).
		AkChannelConfig supportedConfigIn;
#ifdef AK_71AUDIO
		if (in_inputConfig.eConfigType == AK_ChannelConfigType_Standard)
			supportedConfigIn.SetStandard((in_inputConfig.uChannelMask & AK_SPEAKER_SETUP_DEFAULT_PLANE) & ~AK_SPEAKER_LOW_FREQUENCY);
		else
			supportedConfigIn = in_inputConfig;
#else
		supportedConfigIn = in_inputConfig.RemoveLFE();
#endif

		if (supportedConfigIn.uNumChannels > 0)
		{
			CAkSpeakerPan::GetSpeakerVolumes(
				in_emitter,
				in_fCenterPerc,
				in_fSpread,
				in_fFocus,
				io_mxVolumes,
				supportedConfigIn,
				in_uInputChanSel,
				in_outputConfig,
				in_listPosition,
				in_listRot,
				pDevice);
		}

		// Handle LFE.
#ifdef AK_LFECENTER
		if (in_inputConfig.HasLFE() && in_outputConfig.HasLFE())
		{
			AK::SpeakerVolumes::VectorPtr pLFE = AK::SpeakerVolumes::Matrix::GetChannel(io_mxVolumes, in_inputConfig.uNumChannels - 1, in_outputConfig.uNumChannels);
			pLFE[in_outputConfig.uNumChannels - 1] = 1.f;
		}
#endif
	}
	return AK_Success;
}

AKRESULT CAkBusFX::Compute3DPositioning(
	AkReal32			in_fAngle,					///< Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are clockwise).
	AkReal32			in_fElevation,				///< Incident elevation angle, in radians [-pi/2,pi/2], where 0 is the horizon (positive values are above the horizon).
	AkReal32			in_fSpread,					///< Spread.
	AkReal32			in_fFocus,					///< Focus.
	AkChannelConfig		in_inputConfig,				///< Channel configuration of the input.
	AkChannelMask		in_uInputChanSel,			///< Mask of input channels selected for panning (excluded input channels don't contribute to the output).
	AkChannelConfig		in_outputConfig,			///< Desired output configuration.
	AkReal32			in_fCenterPerc,				///< Center percentage. Only applies to mono inputs to outputs that have a center.
	AK::SpeakerVolumes::MatrixPtr out_mxVolumes		///< Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services).
	)
{
	if(in_fSpread < 0 || in_fFocus < 0)	//Avoid infinite loop if bad inputs.
		return AK_InvalidParameter;

	AK::SpeakerVolumes::Matrix::Zero( out_mxVolumes, in_inputConfig.uNumChannels, in_outputConfig.uNumChannels );

	AkReal32 cosElevation = cosf(in_fElevation);
	AkTransform emitter;
	emitter.Set(sinf(in_fAngle) * cosElevation, sinf(in_fElevation), cosf(in_fAngle) * cosElevation, 0, 0, 1, 0, 1, 0);

	AkVector vListPos = { 0, 0, 0 };
	AkReal32 matIdentity[3][3] =
	{
		{ 1, 0, 0 },
		{ 0, 1, 0 },
		{ 0, 0, 1 }
	};

	return _Compute3DPositioning(
		emitter,					// Emitter transform.
		in_fCenterPerc,				// Center percentage. Only applies to mono inputs to outputs that have a center.
		in_fSpread,					// Spread.
		in_fFocus,					// Focus.
		out_mxVolumes,		// Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services). Assumes zeroed.
		in_inputConfig,				// Channel configuration of the input.
		in_uInputChanSel,			// Mask of input channels selected for panning (excluded input channels don't contribute to the output).
		in_outputConfig,			// Desired output configuration.
		vListPos,			// listener position
		matIdentity			// listener rotation
		);
}

AKRESULT CAkBusFX::Compute3DPositioning(
	const AkTransform & in_emitter,					// Emitter transform.
	const AkTransform & in_listener,				// Listener transform.
	AkReal32			in_fCenterPerc,				// Center percentage. Only applies to mono inputs to outputs that have a center.
	AkReal32			in_fSpread,					// Spread.
	AkReal32			in_fFocus,					// Focus.
	AkChannelConfig		in_inputConfig,				// Channel configuration of the input.
	AkChannelMask		in_uInputChanSel,			// Mask of input channels selected for panning (excluded input channels don't contribute to the output).
	AkChannelConfig		in_outputConfig,			// Desired output configuration.
	AK::SpeakerVolumes::MatrixPtr out_mxVolumes		// Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services). Assumes zeroed.
	)
{
	// Build up rotation matrix out of our 3 basic row vectors, stored in row major order.
	AkReal32 listenerRotationMatrix[3][3];
	AkReal32* pFloat = &(listenerRotationMatrix[0][0]);
	AkVector OrientationSide = AkMath::CrossProduct(in_listener.OrientationTop(), in_listener.OrientationFront());

	*pFloat++ = OrientationSide.X;
	*pFloat++ = OrientationSide.Y;
	*pFloat++ = OrientationSide.Z;

	*pFloat++ = in_listener.OrientationTop().X;
	*pFloat++ = in_listener.OrientationTop().Y;
	*pFloat++ = in_listener.OrientationTop().Z;

	*pFloat++ = in_listener.OrientationFront().X;
	*pFloat++ = in_listener.OrientationFront().Y;
	*pFloat++ = in_listener.OrientationFront().Z;

	AK::SpeakerVolumes::Matrix::Zero(out_mxVolumes, in_inputConfig.uNumChannels, in_outputConfig.uNumChannels);
	return _Compute3DPositioning(in_emitter, in_fCenterPerc, in_fSpread, in_fFocus, out_mxVolumes, in_inputConfig, in_uInputChanSel, in_outputConfig, in_listener.Position(), listenerRotationMatrix);
}

// Metering.
void CAkBusFX::EnableMetering( AkMeteringFlags in_eFlags )
{
	m_eMeteringPluginFlags = in_eFlags;
	RefreshMeterWatch();
}

//-----------------------------------------------------------------------------
// CAkVPLMixBusNode
//-----------------------------------------------------------------------------

AKRESULT CAkVPLMixBusNode::Init( AkVPL * in_pOwner, AkChannelConfig in_channelConfig, AkUInt16 in_uMaxFrames, const CAkBusCtx& in_busContext, CAkBusFX * in_parentBus )
{
	AKASSERT( in_pOwner );
	m_pOwner = in_pOwner;
	m_BusContext = in_busContext;

	m_bEffectCreated = !in_busContext.HasBus(); //don't bother trying to create effects if there is no bus.
	m_bBypassChanged = false;

#ifndef AK_OPTIMIZED
	m_uMixingVoiceCount = 0;	
#endif

	//Create a CAkMixBusCtx, if there is a valid CAkBus.  It is possible this is an anonymous 'mix-to-device' bus, in which case there is no behavioral bus.
	// - if the bus ctx does not have a bus, it does not have a game object (instead it has a device id)
	if (in_busContext.HasBus())
	{
		CAkRegisteredObj* pGameObject = NULL;

		if (in_busContext.GameObjectID() != AK_INVALID_GAME_OBJECT)
			pGameObject = g_pRegistryMgr->GetObjAndAddref(in_busContext.GameObjectID());

		// - if the bus ctx has a bus, it should also have a valid game object.  If not, return AK_fail.
		if (pGameObject == NULL)
			return AK_Fail; // expected a game object but it was not registered.
		
		m_pContext = AkNew(AkMemID_Processing, CAkMixBusCtx(pGameObject, in_busContext.GetBus()));
		if (pGameObject != NULL)
			pGameObject->Release();

		if (m_pContext == NULL)
			return AK_InsufficientMemory;

		m_bVolumeCallbackEnabled = g_pBusCallbackMgr->IsVolumeCallbackEnabled(m_pContext->GetSoundID());
		m_eMeteringCallbackFlags = g_pBusCallbackMgr->IsMeteringCallbackEnabled(m_pContext->GetSoundID());

		((CAkMixBusCtx*)m_pContext)->SetMixBusNode(this);

		AkMonitorData::NotificationReason reason;
		bool bIntiallyUnderThreshold = false;
		AKRESULT res = m_pContext->PreInit(-1.f, NULL, true, reason, NULL, bIntiallyUnderThreshold);

		if (res == AK_Success)
		{
			res = m_pContext->Init();
			if (res == AK_Success)
			{
				AkModulatorTriggerParams modTriggerParams;
				m_pContext->GetModulatorTriggerParams(modTriggerParams);
				m_pContext->GetParameterNode()->TriggerModulators(modTriggerParams, &m_pContext->GetModulatorData(), true);

				m_pContext->StartPath();
			}
		}

		if (res != AK_Success)
			return res;

	}

	m_eState							= NodeStateIdle;

	m_MixBuffer.Clear();
	m_MixBuffer.eState					= AK_NoMoreData;
	AkUInt32 uBufferOutSize				= in_uMaxFrames * in_channelConfig.uNumChannels * sizeof(AkReal32);
	void * pData = AkMalign( AkMemID_Processing, uBufferOutSize, AK_BUFFER_ALIGNMENT );
	if ( !pData )
		return AK_InsufficientMemory;

	AkZeroMemLarge( pData, uBufferOutSize );
	m_MixBuffer.AttachContiguousDeinterleavedData(
		pData,						// Buffer.
		in_uMaxFrames,				// Buffer size (in sample frames).
		0,							// Valid frames.
		in_channelConfig );			// Chan config.

	//InitVolumes();
	UpdateBypassFx();

	// Check if this bus has a mixer plugin. If it does, remember that it needs to be created.
	// If it does not, pretend it has already been created.
	if (in_busContext.HasMixerPlugin())
	{
		m_pMixerPlugin = AkNew( AkMemID_Processing, MixerFX() );
		if ( !m_pMixerPlugin )
			return AK_InsufficientMemory;
	}

	return AK_Success;
} // Init


//-----------------------------------------------------------------------------
// Name: Term
// Desc: Terminate.
//
// Parameters:
//
// Return:
//	AKRESULT
//		AK_Success : terminated successfully.
//		AK_Fail    : failed.
//-----------------------------------------------------------------------------
CAkVPLMixBusNode::~CAkVPLMixBusNode()
{
	//Disconnect all inputs before destroying this, as they'll end up calling methods of this.
	DisconnectAllInputs();

	//Destroy FX before Term()'ing the m_pContext and releasing the game object so they can unregister their rtpcs.
	DropFx();

	if (m_pContext)
	{
		m_pContext->Term(false);
		AkDelete(AkMemID_Processing, m_pContext);
		m_pContext = NULL;
	}

	if( m_MixBuffer.HasData() )
	{
		AkFalign( AkMemID_Processing, m_MixBuffer.GetContiguousDeinterleavedData() );
		m_MixBuffer.ClearData();
	}
} // Term

//-----------------------------------------------------------------------------
// Name: ReleaseBuffer
// Desc: Releases a processed buffer.
//
// Parameters:
//	AkAudioBuffer* io_pBuffer : Pointer to the buffer object to release.
//
// Return:
//	Ak_Success: Buffer was relesed.
//  AK_Fail:    Failed to release the buffer.
//-----------------------------------------------------------------------------
void CAkVPLMixBusNode::ReleaseBuffer()
{
	ResetStateForNextPass();

	// Also clean the data for next iteration.
	AkZeroMemLarge( m_MixBuffer.GetContiguousDeinterleavedData(), m_MixBuffer.MaxFrames() * GetMixConfig().uNumChannels * sizeof(AkReal32) );

} // ReleaseBuffer

void CAkVPLMixBusNode::ResetStateForNextPass()
{
	AKASSERT(m_MixBuffer.HasData());

	m_pMixableBuffer = NULL;
	m_eState = m_MixBuffer.eState == AK_NoMoreData ? NodeStateIdle : NodeStatePlay;
	m_MixBuffer.eState = AK_NoMoreData; // Reset state in case ConsumeBuffer() does not get called again
	m_MixBuffer.uValidFrames = 0;		// Assume output buffer was entirely consumed by client. Do not actually release the memory here.

	if (!m_iBypassAllFX)
	{
		for (AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex)
		{
			AkPipelineBufferBase& buffer = m_aFxBuffer[uFXIndex];
			if (!m_aFX[uFXIndex].iBypass && buffer.HasData())
			{
				m_eState = buffer.eState == AK_NoMoreData ? NodeStateIdle : NodeStatePlay;
				buffer.eState = AK_NoMoreData;
				buffer.uValidFrames = 0;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Name: Connect
// Desc: Connects the specified voice as input.
//-----------------------------------------------------------------------------
void CAkVPLMixBusNode::Connect( AK::IAkMixerInputContext * in_pInput )
{
//	InitVolumes();

	if ( m_pMixerPlugin )
	{
		// Ensure mixer plugin is created, if applicable.
		if ( AK_EXPECT_FALSE( (!m_pMixerPlugin->pPlugin) && (GetContext() != NULL) ) )
			SetMixerPlugin();

		// Notify mixer plugin.
		if ( m_pMixerPlugin->pPlugin )
			m_pMixerPlugin->pPlugin->OnInputConnected( in_pInput );
	}
} // Connect

//-----------------------------------------------------------------------------
// Name: Disconnect
// Desc: Disconnects the specified voice as input.
//-----------------------------------------------------------------------------
void CAkVPLMixBusNode::Disconnect(AK::IAkMixerInputContext * in_pInput)
{
	if ( m_pMixerPlugin && m_pMixerPlugin->pPlugin )
		m_pMixerPlugin->pPlugin->OnInputDisconnected( in_pInput );
} // Disconnect

void CAkBusFX::SetInsertFxMask(AkUInt32 in_uFXMask /*<- FX to set mask*/)
{
	AKASSERT(GetContext() != NULL);

	UpdateBypassFx();

	AkAudioFormat l_Format;
	l_Format.SetAll(AK_CORE_SAMPLERATE,
		m_MixBuffer.GetChannelConfig(),
		AK_LE_NATIVE_BITSPERSAMPLE,
		AK_LE_NATIVE_BITSPERSAMPLE / 8 * (m_MixBuffer.GetChannelConfig().uNumChannels),
		AK_FLOAT,
		AK_NONINTERLEAVED);

	AkChannelConfig oldChConfig = l_Format.channelConfig;
	bool bChangedChCfg = false;

	for (AkUInt32 iFx = 0; iFx < AK_NUM_EFFECTS_PER_OBJ; ++iFx)
	{
		if (!IsInPlaceFx(iFx) && !m_aFX[iFx].iLastBypass && !m_iLastBypassAllFX)
			oldChConfig = m_aFxBuffer[iFx].GetChannelConfig();

		if (bChangedChCfg || ((in_uFXMask & (1U << iFx)) != 0))
		{
			// Init the effects as indicated by the mask, or if a previous FX changed the input channel config.
			_SetInsertFx(iFx, l_Format);
		}
		else if (!IsInPlaceFx(iFx) && !m_aFX[iFx].iBypass && !m_iBypassAllFX )
		{
			// Update the ch cfg to feed into the next fx (or InitPan()) if this is an out-of-place effect.
			l_Format.channelConfig = m_aFxBuffer[iFx].GetChannelConfig();
		}

		bChangedChCfg = (l_Format.channelConfig != oldChConfig);
	}

	m_bEffectCreated = true;
	m_bBypassChanged = false;

	RefreshMeterWatch();
}

void CAkBusFX::UpdateBypassFx()
{
	AkInt16 iBypass = 0;

	if (m_BusContext.GetBus())
	{
		CAkBusCtxRslvd resolvedBus = m_BusContext;
		iBypass = resolvedBus.GetBus()->GetBypassAllFX(resolvedBus.GetGameObjPtr());
	}

	SetFxBypassAll(iBypass);
}

void CAkVPLMixBusNode::ConsumeBuffer(
	AkAudioBuffer &			io_rVPLState
	, AkMixConnection &	in_voice
)
{
	AKASSERT(in_voice.IsAudible());
	AKASSERT(in_voice.mxDirect.IsAllocated());

	if( io_rVPLState.uValidFrames > 0 )
	{
#ifndef AK_OPTIMIZED
		IncrementMixingVoiceCount();
#endif
		// Set plugin audio buffer eState before execution (it will get updated by the effect chain)
		m_MixBuffer.eState = AK_DataReady;
		if ( m_eState == NodeStateIdle )
			m_eState = NodeStatePlay; // Idle mode due to virtual voices ? bring it back to play

		// Mixer requires that buffer is zeroed out if buffer is not complete.
		ZeroPadBuffer( &io_rVPLState );

		in_voice.UpdateLPF();
		in_voice.UpdateLPH();
		
		if ( !m_pMixerPlugin || !m_pMixerPlugin->pPlugin )
		{
			// Apply ray volume now through a standard mix
			AkRamp baseVolume = in_voice.mixVolume * in_voice.rayVolume;
			CAkSrcLpHpFilter* pBqf = in_voice.GetFilterContext();
			AkUInt16 usMaxFrames = m_MixBuffer.MaxFrames();
			AkReal32 fOneOverMaxFrames = 1.f / (AkReal32)usMaxFrames;

			// Update the parameters here in case bypass has changed.
			if (pBqf->ManageBQFChange())
			{
				// If DC is removed, or filters are not init, bypass
				if (!pBqf->IsInitialized() || pBqf->IsDcRemoved())
				{
					// Does not allocate a temp buffer
					AkMixer::MixBypassNinNChannels(&io_rVPLState, &m_MixBuffer, baseVolume, in_voice.mxDirect.Prev(), in_voice.mxDirect.Next(), fOneOverMaxFrames,
										  usMaxFrames, pBqf);
				}
				else
				{
					// Call the filter function one more time (in bypass) to eliminate the dc click
					AkMixer::MixFilterNinNChannels(&io_rVPLState, &m_MixBuffer, baseVolume, in_voice.mxDirect.Prev(), in_voice.mxDirect.Next(), fOneOverMaxFrames,
										  usMaxFrames, pBqf);
				}
			}
			else
			{
				// Mix and filter
				AkMixer::MixFilterNinNChannels(&io_rVPLState, &m_MixBuffer, baseVolume, in_voice.mxDirect.Prev(), in_voice.mxDirect.Next(), fOneOverMaxFrames,
									  usMaxFrames, pBqf);
			}
			
#ifndef AK_OPTIMIZED
			if (g_settings.bDebugOutOfRangeCheckEnabled && !m_MixBuffer.CheckValidSamples())
			{
				AkOSChar msg[100];
				AK_OSPRINTF(msg, 100, AKTEXT("Wwise audio out of range: after Mix & Filter, bus ID %u"), GetBusID());
				AK::Monitor::PostString(msg, AK::Monitor::ErrorLevel_Error);
			}
#endif

			m_MixBuffer.uValidFrames = usMaxFrames;
		}
		else
		{
#ifndef AK_OPTIMIZED
			AkPluginID pluginID = m_pMixerPlugin->GetPluginID();
			AkAudiolibTimer::Item * pTimerItem = AK_START_TIMER(0, pluginID, GetContext()->GetPipelineID());
#endif

			AkVPLState outOfPlaceCopy;
			outOfPlaceCopy.SetChannelConfig( io_rVPLState.GetChannelConfig() );
			outOfPlaceCopy.SetRequestSize( io_rVPLState.MaxFrames() );
			AKRESULT eResult = outOfPlaceCopy.GetCachedBuffer( );
			if ( eResult == AK_Success )
			{
				memcpy( outOfPlaceCopy.GetChannel( 0 ), io_rVPLState.GetChannel( 0 ), io_rVPLState.GetChannelConfig().uNumChannels * io_rVPLState.MaxFrames() * sizeof( AkSampleType ) );
				outOfPlaceCopy.uValidFrames = io_rVPLState.MaxFrames();

				in_voice.ConsumeBufferBQF( outOfPlaceCopy );
			}
			else
			{
				// If allocation fails, do a pointer copy and don't filter. 
				outOfPlaceCopy.AttachContiguousDeinterleavedData( io_rVPLState.GetChannel( 0 ), io_rVPLState.MaxFrames(), io_rVPLState.MaxFrames(), io_rVPLState.GetChannelConfig() );
			}

#ifndef AK_OPTIMIZED
			if (g_settings.bDebugOutOfRangeCheckEnabled && !outOfPlaceCopy.CheckValidSamples())
			{
				AkOSChar msg[100];
				AK_OSPRINTF(msg, 100, AKTEXT("Wwise audio out of range: after BQF, bus ID %u"), GetBusID());
				AK::Monitor::PostString(msg, AK::Monitor::ErrorLevel_Error);
			}
#endif

			// Ray volume is left to mixer plugins to handle, by querying rays. Collapse send level with behavioral volume.
			m_pMixerPlugin->pPlugin->ConsumeInput( &in_voice, in_voice.mixVolume, in_voice.rayVolume, &outOfPlaceCopy, &m_MixBuffer );
			AK_STOP_TIMER(pTimerItem);

#ifndef AK_OPTIMIZED
			if (g_settings.bDebugOutOfRangeCheckEnabled && !m_MixBuffer.CheckValidSamples())
			{
				AkOSChar msg[100];
				AK_OSPRINTF(msg, 100, AKTEXT("Wwise audio out of range: after Mixer Plugin, bus ID %u"), GetBusID());
				AK::Monitor::PostString(msg, AK::Monitor::ErrorLevel_Error);
			}
#endif
			if ( eResult == AK_Success )
			{
				outOfPlaceCopy.ReleaseCachedBuffer();
			}
			else
			{
				outOfPlaceCopy.DetachContiguousDeinterleavedData();
			}
		}
	}
}

#ifndef AK_OPTIMIZED
void CAkBusFX::RecapPluginParamDelta()
{
	AkDeltaMonitor::OpenSoundBrace(ID(), GetContext()->GetPipelineID());

	for (int i = 0; i < AK_NUM_EFFECTS_PER_OBJ; ++i)
	{
		FX& fx = m_aFX[i];
		fx.RecapRTPCParams();
	}

	if (m_pMixerPlugin)
		m_pMixerPlugin->RecapRTPCParams();

	AkDeltaMonitor::CloseSoundBrace();
}
#endif

//-----------------------------------------------------------------------------
// Name: GetResultingBuffer
// Desc: Returns the resulting mixed buffer.
//
// Returns:
//	AkAudioBuffer* : Pointer to a buffer object.
//-----------------------------------------------------------------------------
AkAudioBuffer* CAkVPLMixBusNode::GetResultingBuffer()
{
	if (!m_bEffectCreated || m_bBypassChanged)
	{
		// Init all effect if we have not already done so.  If the bypass state has changed, pass 0 to check for possible channel configuration changes.
		AkUInt32 uFxToSet = (!m_bEffectCreated) ? ((1U << AK_NUM_EFFECTS_PER_OBJ) - 1U) : 0;
		SetInsertFxMask(uFxToSet);
	}

#ifndef AK_OPTIMIZED
	AkPluginID mixerPluginID = AK_INVALID_PLUGINID;
#endif
	bool bHasMixerPlugin = ( m_pMixerPlugin && m_pMixerPlugin->pPlugin );
	if (bHasMixerPlugin)
	{
#ifndef AK_OPTIMIZED
		mixerPluginID = m_pMixerPlugin->GetPluginID();
		AkAudiolibTimer::Item * pTimerItem = AK_START_TIMER(0, mixerPluginID, GetContext()->GetPipelineID());
#endif
		m_pMixerPlugin->pPlugin->OnMixDone(&m_MixBuffer);
		AK_STOP_TIMER(pTimerItem);
	}

	AkAudioBuffer * pOutputBuffer = ProcessAllFX();
	PostProcessFx(pOutputBuffer);

	// Post effect hook on mixer plugins.
	if (bHasMixerPlugin)
	{
#ifndef AK_OPTIMIZED
		AkAudiolibTimer::Item * pTimerItem = AK_START_TIMER(0, mixerPluginID, GetContext()->GetPipelineID());
#endif
		m_pMixerPlugin->pPlugin->OnEffectsProcessed(pOutputBuffer);
		AK_STOP_TIMER(pTimerItem);
	}

	AkMeterCtx * pMeter = m_MeterCtxFactory.CreateMeterContext();

	if (pMeter)	// Compute metering whenever there is a meter context.
	{
		MeterBuffer(m_baseVolume.fNext, pOutputBuffer, pMeter);
		if ( m_eMeteringCallbackFlags != AK_NoMetering )
			g_pBusCallbackMgr->DoMeteringCallback(GetBus()->ID(), pMeter, pMeter->GetChannelConfig());
	}

	// Post metering hook on mixer plugins.
	if (bHasMixerPlugin)
	{
#ifndef AK_OPTIMIZED
		AkAudiolibTimer::Item * pTimerItem = AK_START_TIMER(0, mixerPluginID, GetContext()->GetPipelineID());
#endif
		m_pMixerPlugin->pPlugin->OnFrameEnd(pOutputBuffer, pMeter);
		AK_STOP_TIMER(pTimerItem);
	}

	return pOutputBuffer;
} // GetResultingBuffer

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
AkPipelineBufferBase* CAkVPLMixBusNode::ProcessAllFX()
{
	AkPipelineBufferBase* pAudioBuffer = &m_MixBuffer;

	bool bFxProcessedUnused;
	if ( m_eState == NodeStatePlay )
	{
		for ( AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex )
		{
			ProcessFX(uFXIndex, pAudioBuffer, bFxProcessedUnused);
		}
	}
	else
	{
		if (!m_iBypassAllFX)
		{
			for (AkInt32 uFXIndex = AK_NUM_EFFECTS_PER_OBJ - 1; uFXIndex >= 0; --uFXIndex)
			{
				if (!m_aFX[uFXIndex].iBypass && m_aFxBuffer[uFXIndex].HasData())
				{
					pAudioBuffer = &m_aFxBuffer[uFXIndex];
					break;
				}
			}
		}

	}

	return pAudioBuffer;
}

//-----------------------------------------------------------------------------
void CAkVPLMixBusNode::ProcessFX( AkUInt32 in_fxIndex, AkPipelineBufferBase*& io_rpAudioBuffer, bool &io_bfxProcessed )
{
	AKASSERT(m_eState == NodeStatePlay);
	FX & fx = m_aFX[ in_fxIndex ];
	if( fx.pEffect != NULL )
	{
#ifndef AK_OPTIMIZED
		AkAudiolibTimer::Item * pTimerItem = AK_START_TIMER(0, fx.id, GetContext()->GetPipelineID());
#endif
		if ( !fx.iBypass && !m_iBypassAllFX )
		{
			// Ensure SIMD can be used without additional considerations
			AKASSERT(io_rpAudioBuffer->MaxFrames() % 4 == 0);

			if (IsInPlaceFx(in_fxIndex))
			{
				static_cast<AK::IAkInPlaceEffectPlugin*>(fx.pEffect)->Execute(io_rpAudioBuffer);
				AKASSERT(io_rpAudioBuffer->uValidFrames <= io_rpAudioBuffer->MaxFrames());	// Produce <= than requested
			}
			else
			{
				static_cast<AK::IAkOutOfPlaceEffectPlugin*>(fx.pEffect)->Execute(io_rpAudioBuffer, 0, &m_aFxBuffer[in_fxIndex] );
				io_rpAudioBuffer = &m_aFxBuffer[in_fxIndex];
			}

			AKSIMD_ASSERTFLUSHZEROMODE;

#ifndef AK_OPTIMIZED
			if (g_settings.bDebugOutOfRangeCheckEnabled && !io_rpAudioBuffer->CheckValidSamples())
			{
				AkOSChar msg[100];
				AK_OSPRINTF(msg, 100, AKTEXT("Wwise audio out of range: Bus Effect #%u on bus ID %u"), in_fxIndex, GetBusID());
				AK::Monitor::PostString(msg, AK::Monitor::ErrorLevel_Error);
			}
#endif
		}
		else
		{
			if ( !fx.iLastBypass && !m_iLastBypassAllFX )
			{
				fx.pEffect->Reset( );
			}
		}

		fx.iLastBypass = fx.iBypass;

		AK_STOP_TIMER(pTimerItem);
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CAkVPLMixBusNode::PostProcessFx(AkAudioBuffer* in_pBusOutputBuffer)
{
	if ( m_eState == NodeStatePlay )
	{
		m_iLastBypassAllFX = m_iBypassAllFX;
	}

	CAkBehavioralCtx* pCtx = GetContext();
	if (pCtx != NULL)
	{
		{
			AkReal32 fVolume = m_fBehavioralVolume;
#ifndef AK_OPTIMIZED
			if (((CAkMixBusCtx*)pCtx)->IsMonitoringMute())
				fVolume = 0.0f;
#endif 
			m_baseVolume.fPrev = m_bFirstBufferProcessed ? m_baseVolume.fNext : fVolume;
			m_baseVolume.fNext = fVolume;
		}
		{
			bool bAudible = true, bNextSilent = false;			
			SpeakerVolumeMatrixCallback cb(AK_INVALID_PLAYING_ID, in_pBusOutputBuffer->GetChannelConfig());
			GetVolumes(pCtx, in_pBusOutputBuffer->GetChannelConfig(), 1.f, false, bNextSilent, bAudible, !IsVolumeCallbackEnabled() ? NULL : &cb);
		}
		m_bFirstBufferProcessed = true;
	}
}
