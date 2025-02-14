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
// AkFXContext.cpp
//
// Implementation of FX context interface for source, insert and bus FX.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkFXContext.h"
#include "AkVPLMixBusNode.h"
#include "AkVPLSrcCbxNode.h"
#include "AkSoundBase.h"
#include "AkBankMgr.h"
#include "AkRuntimeEnvironmentMgr.h"
#include "AkProfile.h"

extern AkInitSettings g_settings;

AkDataReference::~AkDataReference()
{
	if (pData && uSourceID != AK_INVALID_SOURCE_ID)
	{
		g_pBankManager->ReleaseMedia( uSourceID );
		if (pUsageSlot)
			pUsageSlot->Release( false );
	}
}

AkDataReferenceArray::~AkDataReferenceArray()
{
	Term();
}

AkDataReference * AkDataReferenceArray::AcquireData( AkUInt32 in_uDataIdx, AkUInt32 in_uSourceID )
{
	AkDataReference * pDataReference = NULL;
	CAkUsageSlot* pReturnedMediaSlot = NULL;
	AkMediaInfo mediaInfo = g_pBankManager->GetMedia( in_uSourceID, pReturnedMediaSlot );

	if( mediaInfo.pInMemoryData )
	{
		pDataReference = Set( in_uDataIdx );
		if( pDataReference )
		{
			pDataReference->pData = mediaInfo.pInMemoryData;
			pDataReference->uSize = mediaInfo.uInMemoryDataSize;
			pDataReference->uSourceID = in_uSourceID;
			pDataReference->pUsageSlot = pReturnedMediaSlot;
		}
		else
		{
			//not enough memory, releasing the data now.
			g_pBankManager->ReleaseMedia( in_uSourceID );
			if( pReturnedMediaSlot )
			{
				pReturnedMediaSlot->Release( false );
			}
		}
	}

	return pDataReference;
}

//-----------------------------------------------------------------------------
// CAkEffectContextBase class.
//-----------------------------------------------------------------------------
CAkEffectContextBase::CAkEffectContextBase( AkUInt32 in_uFXIndex )
	: m_uFXIndex( in_uFXIndex )
{
}

CAkEffectContextBase::~CAkEffectContextBase()
{
}

AK::IAkStreamMgr * CAkEffectContextBase::GetStreamMgr( ) const
{
	return AK::IAkStreamMgr::Get();
}

bool CAkEffectContextBase::IsRenderingOffline() const
{
	return AK_PERF_OFFLINE_RENDERING;
}

AKRESULT CAkEffectContextBase::PostMonitorMessage(const char* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel)
{
	return AK::Monitor::PostString(in_pszError, in_eErrorLevel);
}

bool AkDataReferenceArray::FindAlternateMedia( const CAkUsageSlot* in_pSlotToCheck, AkDataReference& io_rDataRef, AK::IAkPlugin* in_pCorrespondingFX )
{
	AKASSERT( in_pCorrespondingFX );
	if( in_pCorrespondingFX->SupportMediaRelocation() )
	{
		CAkUsageSlot* pReturnedMediaSlot = NULL;
		AkMediaInfo mediaInfo = g_pBankManager->GetMedia( io_rDataRef.uSourceID, pReturnedMediaSlot );

		if( mediaInfo.pInMemoryData )
		{
			if( in_pCorrespondingFX->RelocateMedia( mediaInfo.pInMemoryData, io_rDataRef.pData ) == AK_Success )
			{
				// Here Release old data and do swap in PBI to.
				if( io_rDataRef.pData && io_rDataRef.uSourceID != AK_INVALID_SOURCE_ID )
				{
					g_pBankManager->ReleaseMedia( io_rDataRef.uSourceID );
					if( io_rDataRef.pUsageSlot )
					{
						io_rDataRef.pUsageSlot->Release( false );
					}
				}

				io_rDataRef.pData = mediaInfo.pInMemoryData;
				io_rDataRef.pUsageSlot = pReturnedMediaSlot;

				return true;
			}
			else
			{
				//It is quite unexpected that a call to RelocateMedia would fail, but just in case we still need to release.
				if ( mediaInfo.pInMemoryData )
				{
					g_pBankManager->ReleaseMedia( io_rDataRef.uSourceID );
					if( pReturnedMediaSlot )
						pReturnedMediaSlot->Release( false );
				}

			}
		}
	}

	return false;
}

bool CAkEffectContextBase::IsUsingThisSlot( const CAkUsageSlot* in_pUsageSlot, AK::IAkPlugin* in_pCorrespondingFX )
{
	for( AkDataReferenceArray::Iterator iter = m_dataArray.Begin(); iter != m_dataArray.End(); ++iter )
	{
		AkDataReference& ref = (*iter).item;
		if( ref.pUsageSlot == in_pUsageSlot )
		{
			if( !AkDataReferenceArray::FindAlternateMedia( in_pUsageSlot, ref, in_pCorrespondingFX ) )
				return true;
		}
	}

	return false;
}

bool CAkEffectContextBase::IsUsingThisSlot( const AkUInt8* in_pData )
{
	for( AkDataReferenceArray::Iterator iter = m_dataArray.Begin(); iter != m_dataArray.End(); ++iter )
	{
		AkDataReference& ref = (*iter).item;
		if( ref.pData == in_pData )
			return true;
	}

	return false;
}

#if (defined AK_CPU_X86 || defined AK_CPU_X86_64) && !(defined AK_IOS)
IAkProcessorFeatures * CAkEffectContextBase::GetProcessorFeatures()
{
	return AkRuntimeEnvironmentMgr::Instance();
}
#endif

//-----------------------------------------------------------------------------
// CAkInsertFXContext class.
//-----------------------------------------------------------------------------
CAkInsertFXContext::CAkInsertFXContext( CAkVPLSrcCbxNode * in_pCbx, AkUInt32 in_uFXIndex )
	: CAkEffectContextBase( in_uFXIndex )
	, m_pCbx( in_pCbx )
{
}

CAkInsertFXContext::~CAkInsertFXContext( )
{
}

bool CAkInsertFXContext::IsSendModeEffect( ) const
{
	return false;	// Always insert FX mode
}

AKRESULT CAkInsertFXContext::GetOutputID(AkUInt32 & out_uOutputID, AkUInt32 & out_uDeviceType) const
{
	return AK_NotCompatible;
}

AkUniqueID CAkInsertFXContext::GetNodeID() const
{ 
	return m_pCbx->GetAudioNodeID(); 
}

AKRESULT CAkInsertFXContext::PostMonitorData(
	void *		/*in_pData*/,
	AkUInt32	/*in_uDataSize*/
	)
{
	// Push data not supported on insert effects, simply because we have no way of representing
	// a single instance of an effect in the UI.
	return AK_Fail;
}

bool CAkInsertFXContext::CanPostMonitorData()
{
	// Push data not supported on insert effects, simply because we have no way of representing
	// a single instance of an effect in the UI.
	return false;
}

AKRESULT CAkInsertFXContext::PostMonitorMessage(const char* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel)
{
#ifndef AK_OPTIMIZED
	CAkPBI* pCtx = m_pCbx->GetPBI();
	CAkParameterNode* pParam = pCtx ? pCtx->GetSound() : NULL;
	AkPluginID pluginTypeID = AK_INVALID_PLUGINID;
	AkUniqueID pluginUniqueID = AK_INVALID_UNIQUE_ID;
	AkFXDesc fxDesc;
	if( pParam )
	{
		pParam->GetFX( m_uFXIndex, fxDesc );
		pluginTypeID = fxDesc.pFx->GetFXID();
		pluginUniqueID = fxDesc.pFx->ID();
		MONITOR_PLUGIN_MESSAGE( in_pszError, in_eErrorLevel, pCtx->GetPlayingID(), pCtx->GetGameObjectPtr()->ID(), pCtx->GetSoundID(), pluginTypeID, pluginUniqueID );
	}
	return AK_Success;
#else
	return AK_Fail;
#endif
}

void CAkInsertFXContext::GetPluginMedia(
		AkUInt32 in_dataIndex,		///< Index of the data to be returned.
		AkUInt8* &out_rpData,		///< Pointer to the data (refcounted, must be released by the plugin).
		AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
		)
{
	// Search for already existing data pointers.
	AkDataReference* pDataReference = m_dataArray.Exists( in_dataIndex );
	if( !pDataReference )
	{
		// Get the source ID
		AkUInt32 l_dataSourceID = AK_INVALID_SOURCE_ID;
		// WG-23713: Now solved because GetContext() always return the proper PBI.
		m_pCbx->GetPBI()->GetFXDataID(m_uFXIndex, in_dataIndex, l_dataSourceID);

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

void CAkInsertFXContext::GetPluginCustomGameData( void* &out_rpData, AkUInt32 &out_rDataSize )
{
	out_rpData = NULL;
	out_rDataSize = 0;
}

AkReal32 CAkInsertFXContext::GetDownstreamGain()
{
	return AK::dBToLin(m_pCbx->GetDownstreamGainDB());
}

AKRESULT CAkInsertFXContext::GetParentChannelConfig(AkChannelConfig& out_channelConfig) const
{
	out_channelConfig.Clear();
	const AkMixConnection * pConnection = m_pCbx->GetFirstDirectConnection();
	if (pConnection)
	{
		out_channelConfig = pConnection->GetOutputConfig();
		return AK_Success;
	}
	return AK_Fail;
}

/// Obtain the interface to access voice data.
AK::IAkVoicePluginInfo * CAkInsertFXContext::GetVoiceInfo()
{
	return m_pCbx;
}

AK::IAkGameObjectPluginInfo * CAkInsertFXContext::GetGameObjectInfo()
{
	return m_pCbx;
}

//-----------------------------------------------------------------------------
// CAkBusFXContext class.
//-----------------------------------------------------------------------------
CAkBusFXContext::CAkBusFXContext(
		CAkBusFX * in_pBusFX,
		AkUInt32 in_uFXIndex,
		const AK::CAkBusCtx& in_rBusContext
		)
	: CAkEffectContextBase( in_uFXIndex )
	, m_pBusFX( in_pBusFX )
	, m_BuxCtx( in_rBusContext )
{
	AKASSERT( m_pBusFX );
}

CAkBusFXContext::~CAkBusFXContext( )
{
}

bool CAkBusFXContext::IsSendModeEffect() const
{
	return m_BuxCtx.IsAuxBus();
}

AkUniqueID CAkBusFXContext::GetNodeID() const 
{ 
	return m_BuxCtx.ID(); 
}

AKRESULT CAkBusFXContext::GetOutputID(AkUInt32 & out_uOutputID, AkUInt32 & out_uDeviceType) const
{
	m_pBusFX->_GetOutputID(out_uOutputID, out_uDeviceType);
	return AK_Success;
}

AKRESULT CAkBusFXContext::PostMonitorData(
	void *		in_pData,
	AkUInt32	in_uDataSize
	)
{
#ifndef AK_OPTIMIZED
	AkPluginID	pluginID;
	AkUInt32	uFXIndex;
	m_pBusFX->FindFXByContext( this, pluginID, uFXIndex );
	MONITOR_PLUGINSENDDATA(in_pData, in_uDataSize, m_BuxCtx.ID(), m_BuxCtx.HasBus() ? m_BuxCtx.GameObjectID() : AK_INVALID_GAME_OBJECT, pluginID, uFXIndex);
	return AK_Success;
#else
	return AK_Fail;
#endif
}

bool CAkBusFXContext::CanPostMonitorData()
{
#ifndef AK_OPTIMIZED
	return AkMonitor::IsMonitoring();
#else
	return false;
#endif
}

AKRESULT CAkBusFXContext::PostMonitorMessage(const char* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel)
{
#ifndef AK_OPTIMIZED
	AkPluginID pluginTypeID;
	AkUniqueID pluginUniqueID;
	AkUInt32 uFXIndex;
	m_pBusFX->FindFXByContext( this, pluginTypeID, uFXIndex );

	AkFXDesc fxDesc;
	CAkBusCtxRslvd resolvedBus = m_BuxCtx;
	resolvedBus.GetFX(m_uFXIndex, fxDesc);
	pluginUniqueID = fxDesc.pFx->ID();

	MONITOR_BUS_PLUGIN_MESSAGE( in_pszError, in_eErrorLevel, m_BuxCtx.ID(), pluginTypeID, pluginUniqueID );
	return AK_Success;
#else
	return AK_Fail;
#endif
}

void CAkBusFXContext::GetPluginMedia(
		AkUInt32 in_dataIndex,		///< Index of the data to be returned.
		AkUInt8* &out_rpData,		///< Pointer to the data (refcounted, must be released by the plugin).
		AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
		)
{
	// Search for already existing data pointers.
	AkDataReference* pDataReference = m_dataArray.Exists( in_dataIndex );
	if( !pDataReference )
	{
		// Get the source ID
		AkUInt32 l_dataSourceID = AK_INVALID_SOURCE_ID;
		m_BuxCtx.GetFXDataID( m_uFXIndex, in_dataIndex, l_dataSourceID );

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

void CAkBusFXContext::GetPluginCustomGameData( void* &out_rpData, AkUInt32 &out_rDataSize)
{
	m_pBusFX->GetPluginCustomGameDataForInsertFx(m_uFXIndex, out_rpData, out_rDataSize);
}

AkReal32 CAkBusFXContext::GetDownstreamGain()
{
	return AK::dBToLin(m_pBusFX->GetDownstreamGainDB());
}

AKRESULT CAkBusFXContext::GetParentChannelConfig(AkChannelConfig& out_channelConfig) const
{
	return m_pBusFX->GetParentChannelConfig(out_channelConfig);
}

AK::IAkGameObjectPluginInfo * CAkBusFXContext::GetGameObjectInfo()
{
	return m_pBusFX;
}

AKRESULT CAkBusFXContext::_Compute3DPositioning(
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
	return m_pBusFX->_Compute3DPositioning(in_emitter, in_fCenterPerc, in_fSpread, in_fFocus, io_mxVolumes, in_inputConfig, in_uInputChanSel, in_outputConfig, in_listPosition, in_listRot);
}

AKRESULT CAkBusFXContext::ComputeSpeakerVolumesDirect(
	AkChannelConfig		in_inputConfig,				///< Channel configuration of the input.
	AkChannelConfig		in_outputConfig,			///< Channel configuration of the mixer output.
	AkReal32			in_fCenterPerc,				///< Center percentage. Only applies to mono inputs with standard output configurations that have a center channel.
	AK::SpeakerVolumes::MatrixPtr out_mxVolumes		///< Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services).
	)
{
	return m_pBusFX->ComputeSpeakerVolumesDirect(in_inputConfig, in_outputConfig, in_fCenterPerc, out_mxVolumes);
}