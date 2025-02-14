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

//////////////////////////////////////////////////////////////////////
//
// AkSrcPhysModel.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include "AkSrcPhysModel.h"

#include "AkEffectsMgr.h"
#include "AkFXMemAlloc.h"
#include "AkAudioLibTimer.h"
#include "AkRuntimeEnvironmentMgr.h"
#include "AkPlayingMgr.h"

#include "AkVPLSrcCbxNode.h"

//-----------------------------------------------------------------------------
// Defines.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Name: CAkSrcPhysModel
// Desc: Constructor.
//
// Return: None.
//-----------------------------------------------------------------------------
CAkSrcPhysModel::CAkSrcPhysModel( CAkPBI * in_pCtx )
	: CAkVPLSrcNode( in_pCtx )
	, m_pOutputBuffer(NULL)
{
#ifndef AK_OPTIMIZED
	m_pluginParams.UpdateContextID(in_pCtx->GetSoundID(), in_pCtx->GetPipelineID());
#endif
}

//-----------------------------------------------------------------------------
// Name: ~CAkSrcPhysModel
// Desc: Destructor.
//
// Return: None.
//-----------------------------------------------------------------------------
CAkSrcPhysModel::~CAkSrcPhysModel()
{
	StopStream();
}

//-----------------------------------------------------------------------------
// Name: StartStream
// Desc: Start to stream data.
//
// Return: Ak_Success:          Stream was started.
//         AK_InvalidParameter: Invalid parameters.
//         AK_Fail:             Failed to start streaming data.
//-----------------------------------------------------------------------------
AKRESULT CAkSrcPhysModel::StartStream( AkUInt8 * in_pBuffer, AkUInt32 ulBufferSize )
{
	AK::Monitor::ErrorCode errCode = AK::Monitor::ErrorCode_PluginInitialisationFailed;
	AKRESULT l_eResult = AK_Fail;
	AK::IAkPlugin * l_pIEffect = NULL;
	AkPluginInfo PluginInfo;
	CAkFxCustom* pFxCustom = NULL;

	// Set source info
	{
		AkSrcTypeInfo * pSrcType = m_pCtx->GetSrcTypeInfo();

		// Source plug-in are always custom for now
		pFxCustom = g_pIndex->m_idxFxCustom.GetPtrAndAddRef(pSrcType->mediaInfo.sourceID);
		if (!pFxCustom)
		{
			errCode = AK::Monitor::ErrorCode_SourcePluginNotFound;
			goto failure;
		}
	}

	m_pluginParams.fxID = pFxCustom->GetFXID();

	// Get an instance of the physical modelling source plugin.
	l_eResult = CAkEffectsMgr::Alloc(m_pluginParams.fxID, l_pIEffect, PluginInfo);
	if( l_eResult != AK_Success )
	{
		errCode = AK::Monitor::ErrorCode_PluginAllocationFailed;
		goto failure;
	}

	m_pluginParams.pEffect = reinterpret_cast<IAkSourcePlugin*>(l_pIEffect);

	errCode = CAkEffectsMgr::ValidatePluginInfo(m_pluginParams.fxID, AkPluginTypeSource, PluginInfo);
	if (errCode != AK::Monitor::ErrorCode_NoError)
	{
		l_eResult = AK_Fail;
		goto failure;
	}

	if (!m_pluginParams.Clone(pFxCustom, m_pCtx))
	{
		errCode = AK::Monitor::ErrorCode_PluginAllocationFailed;
		l_eResult = AK_Fail;
		goto failure;
	}

	// Suggested format for sources is mono native for platform, they can change it if they wish
	m_AudioFormat.SetAll(
		AK_CORE_SAMPLERATE,
		AkChannelConfig(1, AK_SPEAKER_SETUP_MONO),
		AK_LE_NATIVE_BITSPERSAMPLE,
		sizeof(AkReal32),
		AK_LE_NATIVE_SAMPLETYPE,
		AK_LE_NATIVE_INTERLEAVE
		);

    // Initialise.
    l_eResult = m_pluginParams.pEffect->Init(
		AkFXMemAlloc::GetLower(),
		this,
		m_pluginParams.pParam,
		m_AudioFormat
		);

	AKASSERT( ((m_AudioFormat.GetInterleaveID() == AK_INTERLEAVED) ^ (m_AudioFormat.GetTypeID() == AK_FLOAT))
		|| !"Invalid output format" );

	if (m_AudioFormat.GetInterleaveID() == AK_NONINTERLEAVED)
	{
		// Many source plug-ins do not update uBlockAlign according to channel count: enforce the value here.
		m_AudioFormat.uBlockAlign = sizeof(AkReal32) * m_AudioFormat.GetNumChannels();
	}

	if ( !m_AudioFormat.IsChannelConfigSupported() )
	{
		errCode = AK::Monitor::ErrorCode_PluginUnsupportedChannelConfiguration;
		l_eResult = AK_Fail;
		goto failure;
	}

    if( l_eResult != AK_Success )
	{
		errCode = AK::Monitor::ErrorCode_PluginInitialisationFailed;
		goto failure;
	}

	l_eResult = m_pluginParams.pEffect->Reset( );
	if( l_eResult != AK_Success )
	{
		errCode = AK::Monitor::ErrorCode_PluginInitialisationFailed;
		goto failure;
	}

	m_pCtx->SetMediaFormat( m_AudioFormat );

failure:
	if (l_eResult != AK_Success)
	{
		MONITOR_PLUGIN_ERROR(errCode, m_pCtx, m_pluginParams.fxID);
		StopStream();
	}

	// Clearing
	if (pFxCustom)
		pFxCustom->Release();

	return l_eResult;
}

//-----------------------------------------------------------------------------
// Name: StopStream
// Desc: Stop streaming data.
//
// Return: Ak_Success:          Stream was stopped.
//         AK_InvalidParameter: Invalid parameters.
//         AK_Fail:             Failed to stop the stream.
//-----------------------------------------------------------------------------
void CAkSrcPhysModel::StopStream()
{
	ReleaseBuffer();
	m_pluginParams.Term();
}

void CAkSrcPhysModel::GetBuffer( AkVPLState & io_state )
{
	if( m_pluginParams.pEffect != NULL )
	{
		if( io_state.MaxFrames() == 0 )
		{
			AKASSERT( !"Physical modeling source called with zeros size request" );
			io_state.result = AK_NoMoreData;
			return;
		}

		AkChannelConfig uChannelConfig = m_AudioFormat.channelConfig;
		if (!m_pOutputBuffer)
		{
			AkUInt32 uSampleFrameSize = m_AudioFormat.GetBlockAlign();
			AKASSERT( uSampleFrameSize >= uChannelConfig.uNumChannels * m_AudioFormat.GetBitsPerSample() / 8 );
			m_pOutputBuffer = AkMalign( AkMemID_Processing, uSampleFrameSize * io_state.MaxFrames(), AK_BUFFER_ALIGNMENT );
			if (!m_pOutputBuffer)
			{
				MONITOR_PLUGIN_ERROR(AK::Monitor::ErrorCode_PluginProcessingFailed, m_pCtx, m_pluginParams.fxID);
				io_state.result = AK_Fail;
				return;
			}
		}

		io_state.AttachInterleavedData(m_pOutputBuffer, io_state.MaxFrames(), 0, uChannelConfig);

		// Ensure SIMD processing is possible without extra considerations
		AKASSERT( io_state.MaxFrames() % 4 == 0 );

		io_state.eState = AK_DataNeeded;

		AkPrefetchZero(io_state.GetInterleavedData(), io_state.MaxFrames() * m_AudioFormat.GetBlockAlign() );

		m_pluginParams.pEffect->Execute( &io_state );
		io_state.result = io_state.eState;

		AKSIMD_ASSERTFLUSHZEROMODE;
	}
	else
	{
		io_state.Clear();
		io_state.result = AK_Fail;
	}

	// Graceful plugin error handling.
	if ( io_state.result == AK_Fail )
	{
		MONITOR_PLUGIN_ERROR( AK::Monitor::ErrorCode_PluginProcessingFailed, m_pCtx, m_pluginParams.fxID );
		return;
	}
}

//-----------------------------------------------------------------------------
// Name: ReleaseBuffer
// Desc: Release a specified buffer.
//-----------------------------------------------------------------------------
void CAkSrcPhysModel::ReleaseBuffer()
{
	if(m_pOutputBuffer)
	{
		AkFalign( AkMemID_Processing, m_pOutputBuffer);
		m_pOutputBuffer = NULL;
	}
}

void CAkSrcPhysModel::VirtualOn( AkVirtualQueueBehavior eBehavior )
{
	if( eBehavior == AkVirtualQueueBehavior_FromBeginning )
	{
		if ( m_pluginParams.pEffect != NULL )
			m_pluginParams.pEffect->Reset( );
	}
}

AKRESULT CAkSrcPhysModel::TimeSkip( AkUInt32 & io_uFrames )
{
	AKRESULT eResult = AK_DataReady;
	if ( m_pluginParams.pEffect != NULL )
	{
		eResult = m_pluginParams.pEffect->TimeSkip( io_uFrames );
		if ( eResult == AK_NotImplemented )
		{
			return CAkVPLSrcNode::TimeSkip( io_uFrames );
		}
	}
	return eResult;
}

//-----------------------------------------------------------------------------
// Name: GetDuration()
// Desc: Get the duration of the source.
//
// Return: AkReal32 : duration of the source.
//
//-----------------------------------------------------------------------------
AkReal32 CAkSrcPhysModel::GetDuration() const
{
	if ( m_pluginParams.pEffect )
		return m_pluginParams.pEffect->GetDuration();
	else
		return 0;
}

// Returns estimate of relative loudness at current position, compared to the loudest point of the sound, in dBs (always negative).
AkReal32 CAkSrcPhysModel::GetAnalyzedEnvelope( AkUInt32 /*in_uBufferedFrames*/ )
{
	if ( m_pluginParams.pEffect )
		return AkMath::FastLinTodB( m_pluginParams.pEffect->GetEnvelope() + AK_EPSILON );
	else
		return 0;
}

void* CAkSrcPhysModel::GetOutputBusNode()
{
	// fail gracefully to direct assignment
	if ( m_pCtx->GetCbx()->HasConnections() && m_pCtx->GetCbx()->NumDirectConnections() )
	{
		return (void*)m_pCtx->GetCbx()->GetFirstDirectConnection()->GetOutputBus();
	}
	else
	{
		return NULL;
	}
}

IAkGrainCodec * CAkSrcPhysModel::AllocGrainCodec()
{
	return CAkEffectsMgr::AllocGrainCodec(CAkEffectsMgr::GetMergedID(AkPluginTypeCodec, AKCOMPANYID_AUDIOKINETIC, AKCODECID_VORBIS));
}

void CAkSrcPhysModel::FreeGrainCodec(IAkGrainCodec * in_Codec)
{
	CAkEffectsMgr::FreeGrainCodec(in_Codec);
}

//-----------------------------------------------------------------------------
// Name: StopLooping()
// Desc: Called on actions of type break
//-----------------------------------------------------------------------------
AKRESULT CAkSrcPhysModel::StopLooping()
{
	if ( m_pluginParams.pEffect )
		return m_pluginParams.pEffect->StopLooping();
	else
		return AK_Fail;
}

//-----------------------------------------------------------------------------
// Name: ChangeSourcePosition()
// Desc: Called on actions of type Seek.
//-----------------------------------------------------------------------------
AKRESULT CAkSrcPhysModel::ChangeSourcePosition()
{
	// Get the source offset from the PBI and transform it to the plugin's sample rate.

	AKASSERT( m_pCtx->RequiresSourceSeek() );

	AkUInt32 uSourceOffset;
	bool bSnapToMarker;	// Ignored with source plugins.
	AkUInt32 uSampleRateSource = m_pCtx->GetMediaFormat().uSampleRate;
	if ( m_pCtx->IsSeekRelativeToDuration() )
	{
		uSourceOffset = m_pCtx->GetSeekPosition(
			uSampleRateSource,
			m_pluginParams.pEffect->GetDuration(),
			bSnapToMarker );
	}
	else
	{
		uSourceOffset = m_pCtx->GetSeekPosition( uSampleRateSource, bSnapToMarker );
	}

	// Consume seeking value on PBI.
	m_pCtx->SetSourceOffsetRemainder( 0 );

	return m_pluginParams.pEffect->Seek( uSourceOffset );
}

//-----------------------------------------------------------------------------
// IAkPluginContextBase implementation
//-----------------------------------------------------------------------------
AkUniqueID CAkSrcPhysModel::GetNodeID() const
{
	return m_pCtx->GetSoundID();
}

bool CAkSrcPhysModel::IsUsingThisSlot( const CAkUsageSlot* in_pUsageSlot )
{
	for( AkDataReferenceArray::Iterator iter = m_dataArray.Begin(); iter != m_dataArray.End(); ++iter )
	{
		AkDataReference& ref = (*iter).item;
		if( ref.pUsageSlot == in_pUsageSlot )
		{
			if (!AkDataReferenceArray::FindAlternateMedia(in_pUsageSlot, ref, m_pluginParams.pEffect))
				return true;
		}
	}

	return false;
}

bool CAkSrcPhysModel::IsUsingThisSlot( const AkUInt8* in_pData )
{
	for( AkDataReferenceArray::Iterator iter = m_dataArray.Begin(); iter != m_dataArray.End(); ++iter )
	{
		AkDataReference& ref = (*iter).item;
		if( ref.pData == in_pData )
			return true;
	}

	return false;
}

void CAkSrcPhysModel::GetPluginMedia(
	AkUInt32 in_dataIndex,		///< Index of the data to be returned.
	AkUInt8* &out_rpData,		///< Pointer to the data.
	AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
	)
{
	// Search for already existing data pointers.
	AkDataReference* pDataReference = m_dataArray.Exists( in_dataIndex );
	if( !pDataReference )
	{
		// Get the source ID
		AkUInt32 l_dataSourceID = AK_INVALID_SOURCE_ID;
		{
			AkSrcTypeInfo * pSrcType = m_pCtx->GetSrcTypeInfo();

			// Source plug-in are always custom for now
			CAkFxBase * pFx = g_pIndex->m_idxFxCustom.GetPtrAndAddRef( pSrcType->mediaInfo.sourceID );
			if ( pFx )
			{
				l_dataSourceID = pFx->GetMediaID( in_dataIndex );
				pFx->Release();
			}
		}

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

void CAkSrcPhysModel::GetPluginCustomGameData( void* &out_rpData, AkUInt32 &out_rDataSize)
{
	out_rpData = NULL;
	out_rDataSize = 0;
}

AKRESULT CAkSrcPhysModel::PostMonitorData(
	void *		in_pData,
	AkUInt32	in_uDataSize
	)
{
#ifndef AK_OPTIMIZED
	MONITOR_PLUGINSENDDATA(in_pData, in_uDataSize, GetNodeID(), m_pCtx->GetGameObjectPtr()->ID(), m_pluginParams.GetPluginID(), 0);
	return AK_Success;
#else
	return AK_Fail;
#endif
}

bool CAkSrcPhysModel::CanPostMonitorData()
{
#ifndef AK_OPTIMIZED
	return AkMonitor::IsMonitoring();
#else
	return false;
#endif
}

AKRESULT CAkSrcPhysModel::PostMonitorMessage(
	const char* in_pszError,
	AK::Monitor::ErrorLevel in_eErrorLevel
	)
{
#ifndef AK_OPTIMIZED
	AkPluginID pluginTypeID = m_pluginParams.fxID;
	AkUniqueID pluginUniqueID = m_pluginParams.pFx->ID();
	MONITOR_PLUGIN_MESSAGE( in_pszError, in_eErrorLevel, m_pCtx->GetPlayingID(), m_pCtx->GetGameObjectPtr()->ID(), m_pCtx->GetSoundID(), pluginTypeID, pluginUniqueID );
	return AK_Success;
#else
	return AK_Fail;
#endif
}

AkReal32 CAkSrcPhysModel::GetDownstreamGain()
{
	return 1.f;
}

AKRESULT CAkSrcPhysModel::GetParentChannelConfig(AkChannelConfig& out_channelConfig) const
{
	out_channelConfig.Clear();
	return AK_Fail;
}

void * CAkSrcPhysModel::GetCookie() const
{
	if (g_pPlayingMgr && m_pCtx)
	{
		return g_pPlayingMgr->GetCookie(m_pCtx->GetPlayingID());
	}
	return NULL;
}

void CAkSrcPhysModel::PluginParams::Term()
{
	if (pEffect != NULL)
	{
		pEffect->Term(AkFXMemAlloc::GetLower());
		pEffect = NULL;
	}

	PluginRTPCSub::Term();
}

#if (defined AK_CPU_X86 || defined AK_CPU_X86_64) && !(defined AK_IOS)
IAkProcessorFeatures * CAkSrcPhysModel::GetProcessorFeatures()
{
	return AkRuntimeEnvironmentMgr::Instance();
}
#endif

void CAkSrcPhysModel::RestartSourcePlugin()
{
	StopStream();
	m_dataArray.RemoveAll();
	StartStream(NULL, 0);
}
