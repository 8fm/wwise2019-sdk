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
// AkLEngine.cpp
//
// Implementation of the IAkLEngine interface. Contains code that is
// common to multiple platforms built on the software pipeline.
//
//   - Code common with non-software-pipeline platforms can
//     be found in Common/AkLEngine_Common.cpp
//   - Platform-specific code can be found in <Platform>/AkLEngine.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "AkLEngine_SoftwarePipeline.h"

#include "AkAudioLibTimer.h"
#include "AkBankMgr.h"
#include "AkFxBase.h"
#include "AkAudioMgr.h"
#include "AkPlayingMgr.h"
#include "AkPositionRepository.h"
#include "AkSound.h"
#include "AkSrcPhysModel.h"
#include "AkVPLFilterNodeOutOfPlace.h"
#include "AkBus.h"
#include "AkMeterTools.h"
#include "AkOutputMgr.h"
#include "AkSpeakerPan.h"
#include "AkEffectsMgr.h"
#include "AkProfile.h"
#include "AkRegistryMgr.h"
#include "AkURenderer.h"
#include "../../AkMemoryMgr/Common/AkMemoryMgrBase.h"
#include "AkMixBusCtx.h"
#include "AkPipelineID.h"
#include "AkMonitor.h"
#include "AkCustomPluginDataStore.h"

extern AkPlatformInitSettings g_PDSettings;
extern AkInitSettings		g_settings;

AKRESULT CAkLEngine::SoftwareInit()
{
	CAkResampler::InitDSPFunctTable();
	AkMixer::InitMixerFunctTable();

	// Check memory manager.
	if ( !AK::MemoryMgr::IsInitialized() )
	{
		AKASSERT( !"Memory manager does not exist" );
		return AK_Fail;
	}
	// Check Stream manager.
	if ( AK::IAkStreamMgr::Get() == NULL )
	{
		AKASSERT( !"Stream manager does not exist" );
		return AK_Fail;
	}

	// Create Platform Context.
	AKRESULT eResult = InitPlatformContext();
	if ( eResult != AK_Success ) return eResult;

	// Effects Manager.
	eResult = CAkEffectsMgr::Init(g_settings.szPluginDLLPath);
	if( eResult != AK_Success ) return eResult;

	// Command queue
	eResult = CAkLEngineCmds::Init();
	if( eResult != AK_Success ) return eResult;

	// Create the speaker panner.
	CAkSpeakerPan::Init();	

	eResult = CAkOutputMgr::Init();
	if(eResult != AK_Success) return eResult;
	
	return eResult;
}

void CAkLEngine::SoftwareTerm()
{
	CAkSpeakerPan::Term();

	AkHdrBus::TermHdrWinTopMap();
	CAkLEngineCmds::Term();
	m_arrayVPLs.Term();
	m_arrayVPLDepthCnt.Term();
	m_arrayVPLDepthInterconnects.Term();
	m_Sources.Term();

	CAkEffectsMgr::Term();

	AkCustomPluginDataStore::TermPluginCustomGameData();

	TermPlatformContext();
}

AkAudioBuffer* CAkLEngine::TransferBuffer(AkVPL* in_pVPL)
{
	CAkVPLMixBusNode& mixBus = in_pVPL->m_MixBus;

	// Get the resulting buffer for this bus
	AkAudioBuffer* pBuffer = mixBus.GetResultingBuffer();

	// Add this buffer to the parent mix node
	// Call ConsumeBuffer() even if no frames in order to keep bus alive.
	if (mixBus.HasConnections())
	{
		return pBuffer;
	}
	else
	{
		if ( AK_EXPECT_FALSE( pBuffer->uValidFrames == 0 || !in_pVPL->IsTopLevelNode() ) )	// Do not push data to device if no frames ready.
			return NULL;

		// Push to device.
		AkDevice* pDevice = CAkOutputMgr::FindDevice(in_pVPL->GetBusContext());
		if(pDevice)
			pDevice->PushData(pBuffer, mixBus.GetBaseVolume());
	}
	return NULL;
}

#include <AK/SoundEngine/Common/AkSimd.h>

//-----------------------------------------------------------------------------
// Defines.
//-----------------------------------------------------------------------------

#define MINIMUM_STARVATION_NOTIFICATION_DELAY 8 //in buffers

//-----------------------------------------------------------------------------
//Static variables.
//-----------------------------------------------------------------------------
CAkLEngine::AkArrayVPL     CAkLEngine::m_arrayVPLs;
CAkLEngine::AkArrayVPLSrcs CAkLEngine::m_Sources;
CAkLEngine::AkArrayInt32   CAkLEngine::m_arrayVPLDepthCnt;
CAkLEngine::AkArrayBool    CAkLEngine::m_arrayVPLDepthInterconnects;
bool                       CAkLEngine::m_bVPLsHaveCycles = false;
bool                       CAkLEngine::m_bVPLsDirty = false;
bool                       CAkLEngine::m_bFullReevaluation = false;

void CAkLEngine::Stop()
{
	//Destroy all remaining playing sources
	for (CAkLEngine::AkArrayVPLSrcs::Iterator it = m_Sources.Begin(); it != m_Sources.End(); )
	{
		CAkVPLSrcCbxNode * pCbx = *it;
		it = m_Sources.Erase( it );
		CAkLEngine::VPLDestroySource( pCbx, false );
	}

	// Clean up all VPL mix busses.
	DestroyAllVPLMixBusses();

	CAkLEngineCmds::DestroyDisconnectedSources();
}

#ifndef AK_OPTIMIZED

void GetSrcOutputBusVolume( AkMixConnection* in_pConnection, AkUInt32 in_numInChan, AkUInt32 in_numOutChan, AkReal32* out_pfVolumes )
{
	if ( in_numOutChan != 0 )
	{
		// Take the maximum volume per output channel.
		memset( out_pfVolumes, 0, in_numOutChan * sizeof( AkReal32 ) );

		AK::SpeakerVolumes::MatrixPtr pMatrix = in_pConnection->mxDirect.Next();
		if ( pMatrix )
		{
			for ( AkUInt32 inChan = 0; inChan < in_numInChan; inChan++ )
			{
				AK::SpeakerVolumes::VectorPtr pChan = AK::SpeakerVolumes::Matrix::GetChannel( pMatrix, inChan, in_numOutChan );
				for ( AkUInt32 outChan = 0; outChan < in_numOutChan; outChan++ )
				{
					// Take absolute max because some panning matrices may use negative values.
					AkReal32 fCurValue = out_pfVolumes[outChan];
					AkReal32 fNewValue = (AkReal32)fabs(pChan[outChan]);
					out_pfVolumes[outChan] = (fNewValue > fCurValue) ? fNewValue : fCurValue;
				}
			}
		}		
	}
}

class AkFXMonitorPack
{
public:
	AkFXMonitorPack() :numFX(0){}

	AkForceInline bool Serialize( MonitorSerializer& in_rSerializer );
	AkForceInline void Add(AkPluginID in_pluginID, AkUniqueID in_realID);

	AkPluginID aFX[AK_NUM_EFFECTS_PER_OBJ];
	AkUniqueID aRealID[AK_NUM_EFFECTS_PER_OBJ];
	AkUInt8 numFX;
};

AkForceInline bool AkFXMonitorPack::Serialize( MonitorSerializer& in_rSerializer )
{
	bool bRc = in_rSerializer.Put(numFX);

	for (AkUInt32 i = 0; i < numFX; ++i)
	{
		bRc = bRc && in_rSerializer.Put(aFX[i]);
		bRc = bRc && in_rSerializer.Put(aRealID[i]);
	}

	return bRc;
}

AkForceInline void AkFXMonitorPack::Add(AkPluginID in_pluginID, AkUniqueID in_realID)
{
	aFX[numFX] = in_pluginID;
	aRealID[numFX] = in_realID;
	++numFX;
}

AkForceInline bool SerializeBusFx( MonitorSerializer& in_rSerializer, CAkVPLMixBusNode& in_rMixBus )
{
	const CAkBusCtx &l_BusCtx = in_rMixBus.GetBusContext();

	AkFXMonitorPack fxMonitorPack;

	for ( AkUInt8 i = 0; i < AK_NUM_EFFECTS_PER_OBJ; ++i )
	{
		AkPluginID fxID = in_rMixBus.GetFXID( i );
		AkUniqueID realID = AK_INVALID_UNIQUE_ID;
		if (fxID != AK_INVALID_PLUGINID)
		{
			AkFXDesc fxDesc;
			CAkParameterNodeBase* pNode = l_BusCtx.GetBus();
			if (pNode)
			{
				pNode->GetFX(i, fxDesc);
				realID = fxDesc.pFx->ID();
			}

			fxMonitorPack.Add(fxID, realID);
		}
	}

	return fxMonitorPack.Serialize(in_rSerializer);
}

//-----------------------------------------------------------------------------------------------

AkForceInline bool  SerializeVoiceFx(MonitorSerializer& in_rSerializer, CAkVPLSrcCbxNode* in_pCbx)
{
	const AkVPLSrcCbxRec& cbxRec = in_pCbx->GetVPLSrcCbx();
	CAkPBI * pCtx = in_pCbx->GetPBI();

	AkFXMonitorPack fxMonitorPack;

	for (AkUInt8 i = 0; i < AK_NUM_EFFECTS_PER_OBJ; ++i)
	{
		AkPluginID fxID = cbxRec.m_pFilter[i] ? cbxRec.m_pFilter[i]->GetFXID() : AK_INVALID_PLUGINID;
		AkUniqueID realID = AK_INVALID_UNIQUE_ID;
		if (fxID != AK_INVALID_PLUGINID)
		{
			AkFXDesc fxDesc;
			pCtx->GetFX(i, fxDesc);
			if (fxDesc.pFx)
			{
				realID = fxDesc.pFx->ID();
			}

			fxMonitorPack.Add(fxID, realID);
		}
	}

	return fxMonitorPack.Serialize(in_rSerializer);
}

//-----------------------------------------------------------------------------------------------

// This is done in a seperate function because of the call to AkAlloca (leads to stack overflow if done in a loop).
static AkNoInline bool _PackDevMapVolumes( MonitorSerializer& in_rSerializer, AkMixConnection* in_pConn )
{
	AkUInt8 numInChan = (AkUInt8)in_pConn->GetNumInputChannels();
	AkUInt8 numOutChan = (AkUInt8)in_pConn->GetOutputConfig().uNumChannels;
	bool bRc = in_rSerializer.Put( (AkUInt8)numOutChan );

	if ( numOutChan != 0 )
	{
		AkReal32* pVolumes = (AkReal32*)AkAlloca( numOutChan * sizeof(AkReal32) );
		GetSrcOutputBusVolume( in_pConn, numInChan, numOutChan, pVolumes );
		for ( AkUInt8 i = 0; i < numOutChan; ++i )
			bRc = bRc && in_rSerializer.PutFloat24( pVolumes[i] ); // voice.fOutputBusVolume
	}

	return bRc;
}

//-----------------------------------------------------------------------------------------------

bool CAkLEngine::SerializeConnections(MonitorSerializer& in_rSerializer, CAkVPL3dMixable* pCbx)
{
	bool bRc = true;

	// Pipeline to OutputDevice map used by monitoring UI
	AkUInt32 devmapPtr = in_rSerializer.Count();
	AkUInt8 cDevMap = 0;
	bRc = bRc && in_rSerializer.Put(cDevMap);

	for (AkMixConnectionList::Iterator itConnection = pCbx->BeginConnection(); itConnection != pCbx->EndConnection(); ++itConnection)
	{
		if (bRc)
		{
			AkDevice* pDevice = CAkOutputMgr::FindDevice((*itConnection)->GetOutputBusCtx());
			if(!pDevice)
				continue;

			// device map data will be sent..
			++cDevMap;

			bRc = bRc && in_rSerializer.Put((*itConnection)->GetOutputBusCtx().ID()); // DevMap.mixBusID

			AkOutputDeviceID deviceID = pDevice->uDeviceID;
			bRc = bRc && in_rSerializer.Put(deviceID); // DevMap.deviceID			
			bRc = bRc && in_rSerializer.Put((*itConnection)->GetListenerID()); // DevMap.busObjectID
			bRc = bRc && in_rSerializer.Put((*itConnection)->GetOutputBus()->GetContext() ? (*itConnection)->GetOutputBus()->GetContext()->GetPipelineID() : 0); // DevMap.pipelineID


			bRc = bRc && in_rSerializer.PutFloat24((*itConnection)->GetCumulativeDownstreamVolumeDB()); // DevMap.fMaxVolume

			bRc = bRc && in_rSerializer.PutFloat24((*itConnection)->GetWindowTopDB()); // DevMap.fHdrWindowTop

			// The connection volume that we sent to the profiler consists of the 'ray volume' which includes the attenuation, and the actual connection volume.
			bRc = bRc && in_rSerializer.PutFloat24((*itConnection)->rayVolume.fNext * (*itConnection)->GetVolume()); // DevMap.fConnectionVolume

			// LPF and HPF values
			bRc = bRc && in_rSerializer.PutFloat24( (*itConnection)->LastLPF() );
			bRc = bRc && in_rSerializer.PutFloat24( (*itConnection)->LastHPF() );

			bRc = bRc && in_rSerializer.Put((AkUInt8)(*itConnection)->GetType()); // DevMap.eConnectionType

			AkUInt8 uLatency = 0U;
			if ((*itConnection)->GetFeedback())
			{
				// Set uLatency to the log2 of AK_NUM_VOICE_REFILL_FRAMES.
				AkUInt32 uOnes = (AK_NUM_VOICE_REFILL_FRAMES - 1);
				while (uOnes != 0)
				{
					uOnes = uOnes >> 1U;
					uLatency++;
				}
			}
		
			bRc = bRc && in_rSerializer.Put(uLatency);

			AkUInt8 uMuteSolo = 0;
			if ((*itConnection)->IsMonitoringMute())
				uMuteSolo = 1;
			else if ((*itConnection)->IsMonitoringSolo())
				uMuteSolo = 2;

			bRc = bRc && in_rSerializer.Put(uMuteSolo);

			// Pack bus volumes.
			bRc = bRc && _PackDevMapVolumes(in_rSerializer, *itConnection);

		}
	}

	// Write final devmap count
	AkUInt32 savePtr = in_rSerializer.Count();
	in_rSerializer.SetCount(devmapPtr);
	bRc = bRc && in_rSerializer.Put(cDevMap);
	in_rSerializer.SetCount(savePtr);

	return bRc;
}

//-----------------------------------------------------------------------------------------------

void CAkLEngine::GetPipelineData( AkVarLenDataCreator& in_rCreator )
{
	MonitorSerializer& rSerializer = in_rCreator.GetSerializer();

	// Initialize data counters
	AkUInt16 cBus = 0;
	AkUInt16 cVoice = 0;

	bool bRc = true;

	// Create place holder for final counters
	AkUInt32 countPtr = rSerializer.Count();
	rSerializer.Put( cBus );
	rSerializer.Put( cVoice );
	
	// Get bus data...
	for (AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL)
	{
		if( bRc )
		{
			const CAkBusCtx &l_BusCtx = (*iterVPL)->m_MixBus.GetBusContext();
			if (!l_BusCtx.HasBus())
				continue; // skip listener->device mixing busses

			AkDevice* pDevice = CAkOutputMgr::FindDevice(l_BusCtx);
			if (!pDevice)
				continue;
			AkOutputDeviceID busDeviceID = pDevice->uDeviceID;

			// Bus data will be sent..
			++cBus;

			
			AkUniqueID l_BusID = l_BusCtx.ID();

			bRc = bRc && rSerializer.PutFloat24((*iterVPL)->m_MixBus.BehavioralVolume()); // bus.fBusVolume
			bRc = bRc && rSerializer.Put( (AkUInt16)(*iterVPL)->m_MixBus.GetMixingVoiceCount() ); // bus.uVoiceCount
			bRc = bRc && rSerializer.PutFloat24( (*iterVPL)->DownstreamGainDB() ); // bus.fDownstreamGain

			bRc = bRc && rSerializer.Put( l_BusID ); // bus.mixBusID

			bRc = bRc && rSerializer.Put(l_BusCtx.GameObjectID()); // bus.gameObjID			
			bRc = bRc && rSerializer.Put( busDeviceID ); // bus.deviceID
			bRc = bRc && rSerializer.Put((*iterVPL)->m_iDepth); // bus.deviceID
			bRc = bRc && rSerializer.Put((*iterVPL)->m_MixBus.GetContext()->GetPipelineID()); // bus.pipelineID


			AkChannelConfig channelConfig = (*iterVPL)->m_MixBus.GetMixConfig();
			bRc = bRc && rSerializer.Put( channelConfig.Serialize() ); // bus.channelConfig

			bRc = bRc && SerializeConnections(rSerializer, &(*iterVPL)->m_MixBus);

			bRc = bRc && rSerializer.Put( (*iterVPL)->m_MixBus.GetMixerID() ); // bus.mixerID

			bRc = bRc && SerializeBusFx(rSerializer, (*iterVPL)->m_MixBus);
		}
	}


	// Voice pipeline data
	for (AkArrayVPLSrcs::Iterator iterVPLSrc = m_Sources.Begin(); iterVPLSrc != m_Sources.End(); ++iterVPLSrc)
	{
		if( bRc )
		{
			CAkVPLSrcCbxNode * pCbx = *iterVPLSrc;
			CAkPBI * pCtx = pCbx->GetPBI();
			AKASSERT( pCtx );

			if ( pCtx )
			{
				// Voice data will be sent..
				++cVoice;

				// Voice pipeline data
				bRc = bRc && rSerializer.Put( pCtx->GetPipelineID() ); // voice.pipelineID
				bRc = bRc && rSerializer.Put( pCtx->GetPlayingID() ); // voice.playingID
				bRc = bRc && rSerializer.Put( pCtx->GetGameObjectPtr()->ID() ); // voice.gameObjID
				bRc = bRc && rSerializer.Put( pCtx->GetSoundID() ); // voice.soundID
				bRc = bRc && rSerializer.Put( pCtx->GetPlayTargetID() ); // voice.playTargetID

				bRc = bRc && SerializeVoiceFx(rSerializer, pCbx);

				bRc = bRc && rSerializer.Put( pCtx->GetPriority() ); // voice.priority

				bRc = bRc && rSerializer.PutFloat24( pCbx->BehavioralVolume() ); // voice.fBaseVolume

				bRc = bRc && rSerializer.PutFloat24( pCbx->GetEnvelope() ); // voice.fEnvelope
				bRc = bRc && rSerializer.PutFloat24( pCbx->GetNormalizationMonitoring() ); // voice.fNormalizationGain

				bRc = bRc && rSerializer.PutFloat24( pCbx->LastLPF() ); // voice.fVoiceLPF
				bRc = bRc && rSerializer.PutFloat24( pCbx->LastHPF() ); // voice.fVoiceHPF

				// Game-Defined
				const AkSoundParams & params = pCtx->GetEffectiveParamsForMonitoring();
				bRc = bRc && rSerializer.PutFloat24( params.GameAuxSendVolume() ); // voice.fGameAuxSendVolume

				AkUInt8 bIsStarted = pCbx->GetState() != NodeStateInit && pCtx->GetFrameOffset() < 0;
				AkVirtualQueueBehavior _unused; 
				AkUInt8 bIsVirtual = pCbx->IsVirtualVoice() && ( pCtx->GetVirtualBehavior( _unused ) == AkBelowThresholdBehavior_SetAsVirtualVoice );
				AkUInt8 bIsForcedVirtual = pCtx->IsForcedVirtualized();

				bRc = bRc && rSerializer.Put((AkUInt8)(bIsStarted | (bIsVirtual << 1) | (bIsForcedVirtual << 2))); // voice.bIsStarted				

				bRc = bRc && SerializeConnections(rSerializer, pCbx);

			}
		}
	}

	// Write final counters in placeholder
	if(  bRc )
	{
		AkUInt32 savePtr = rSerializer.Count();
		rSerializer.SetCount( countPtr );
		AKVERIFY(rSerializer.Put(cBus));
		AKVERIFY(rSerializer.Put(cVoice));
		rSerializer.SetCount( savePtr );
	}
	else // If something went wrong, pretend we never wrote anything!
	{
		AkUInt32 savePtr = rSerializer.Count();
		in_rCreator.OutOfMemory(savePtr - countPtr);
		rSerializer.SetCount( countPtr );
	}
}

bool PutRawMeterData( 
	const AkMonitorData::BusMeterDataType in_meterType,
	AkChannelConfig in_channelConfig,
	AK::SpeakerVolumes::ConstVectorPtr in_meterVolumes,
	MonitorSerializer& in_serializer )
{
	bool bRc = in_serializer.Put( (AkUInt8)in_meterType );

	bRc = bRc && in_serializer.Put( in_channelConfig.Serialize() );

	for( AkUInt32 i = 0; i < in_channelConfig.uNumChannels; ++i )
		bRc = bRc && in_serializer.PutFloat24( in_meterVolumes[i] );

	return bRc;
}

bool PutMeterData( 
	const AkMonitorData::BusMeterDataType in_meterType,
	AkChannelConfig in_channelConfig,
	AK::SpeakerVolumes::ConstVectorPtr in_meterVolumes,
	MonitorSerializer& in_serializer )
{
	bool bRc = in_serializer.Put( (AkUInt8)in_meterType );

	bRc = bRc && in_serializer.Put( in_channelConfig.Serialize() );
	for( AkUInt32 i = 0; i < in_channelConfig.uNumChannels; ++i )
		bRc = bRc && in_serializer.PutFloat24(in_meterVolumes[i] );

	return bRc;
}

bool BuildMeterMonitorData( AkUniqueID in_busID, AkVPL* in_pVPL, CAkBusFX* in_pMixBus, MonitorSerializer& in_serializer, AkUInt32& io_uNumBusses, bool& io_fHdrMeter )
{
	bool bRc = true;

	AkUInt8 types = AkMonitor::GetMeterWatchDataTypes( in_busID );
	AkUInt8 watches = 0;

	AkGameObjectID busObjectID = in_pVPL->GetBusContext().GameObjectID();

	AkInt32 writePtr = in_serializer.Count();
	bRc = bRc && in_serializer.Put(in_busID);
	bRc = bRc && in_serializer.Put(busObjectID);
	bRc = bRc && in_serializer.Put( watches ); // Not the correct value; will be patched later

	if( types & AkMonitorData::BusMeterDataType_HdrPeak && in_pVPL && in_pVPL->IsHDR() )
	{
		// Post the Hdr peak
		AkReal32 channels[1];
		channels[0] = ((AkHdrBus*)in_pVPL)->GetHdrWindowTop() - in_pVPL->DownstreamGainDB();

		AkChannelConfig channelConfig;
		channelConfig.SetAnonymous( 1 );	// 1 anonymous channel required to pass HDR window value.

		bRc = bRc && PutRawMeterData( AkMonitorData::BusMeterDataType_HdrPeak, channelConfig, channels, in_serializer );

		++watches;

		// Update the "at least one HDR peak meter data present" flag
		io_fHdrMeter = true;
	}

	// All meters except HDR depend on the AkMeterCtx.

#ifndef AK_OPTIMIZED

	const AkMeterCtx * pMetering = in_pMixBus->GetMeterCtx();
	if (pMetering)
	{
		AkChannelConfig chConfig = pMetering->GetChannelConfig();
		if ( types & AkMonitorData::BusMeterDataType_Peak )
		{
			// Post the Peak
			AK::SpeakerVolumes::ConstVectorPtr vMeterValues = pMetering->GetMeterPeak();
			if (vMeterValues)
			{
				bRc = bRc && PutMeterData(AkMonitorData::BusMeterDataType_Peak, chConfig, vMeterValues, in_serializer);
				++watches;
			}
		}

		if ( types & AkMonitorData::BusMeterDataType_RMS )
		{
			// Post the RMS
			AK::SpeakerVolumes::ConstVectorPtr vMeterValues = pMetering->GetMeterRMS();
			if (vMeterValues)
			{
				bRc = bRc && PutMeterData(AkMonitorData::BusMeterDataType_RMS, chConfig, vMeterValues, in_serializer);
				++watches;
			}
		}

		if( types & AkMonitorData::BusMeterDataType_TruePeak )
		{
			// Post the True peak
			AK::SpeakerVolumes::ConstVectorPtr vMeterValues = pMetering->GetMeterTruePeak();
			if (vMeterValues)
			{
				bRc = bRc && PutMeterData(AkMonitorData::BusMeterDataType_TruePeak, chConfig, vMeterValues, in_serializer);
				++watches;
			}
		}

        if (types & AkMonitorData::BusMeterDataType_3DMeter)
        {
            // Post the 3D Meter data. If there isn't any, send the packet anyway, with 0 channels.
			AK::SpeakerVolumes::ConstVectorPtr vMeterValues = pMetering->GetMeter3DPeaks(); 
			if (vMeterValues)
			{
				bRc = bRc && PutMeterData(AkMonitorData::BusMeterDataType_3DMeter, chConfig, vMeterValues, in_serializer);
			}
			else
			{
				bRc = bRc & in_serializer.Put((AkUInt8)AkMonitorData::BusMeterDataType_3DMeter);
				AkChannelConfig invalid;
				bRc = bRc && in_serializer.Put(invalid.Serialize());
			}
			++watches;
        }

		if( types & AkMonitorData::BusMeterDataType_KPower )
		{
			// Post the loudness
			AkChannelConfig channelConfig;
			channelConfig.SetAnonymous( 2 );	// channels required to pass K power: mean power, and slice duration.

			AkReal32 channels[2];
			channels[0] = pMetering->GetMeanPowerLoudness();
			channels[1] = (AkReal32)LE_MAX_FRAMES_PER_BUFFER / (AkReal32)AK_CORE_SAMPLERATE;

			bRc = bRc && PutRawMeterData( AkMonitorData::BusMeterDataType_KPower, channelConfig, channels, in_serializer );

			++watches;
		}
	}

#endif

	if( watches != 0 )
	{
		// Patch watch mask for this bus.
		AkInt32 savePtr = in_serializer.Count();
		in_serializer.SetCount( writePtr );
		bRc = bRc && in_serializer.Put(in_busID);
		bRc = bRc && in_serializer.Put(busObjectID);
		bRc = bRc && in_serializer.Put( watches );
		in_serializer.SetCount( savePtr );

		// Increment bus count
		++io_uNumBusses;
	}
	else
	{
		// No watches for this bus, so just pretend nothing was written...
		in_serializer.SetCount( writePtr );
	}

	return bRc;
}

void CAkLEngine::PostMeterWatches()
{
#if !defined (AK_EMSCRIPTEN)

	//TODO: provide profiling information about all device's endpoints.

	AkUInt32 uNumBusses = 0;
	bool fHdrMeter = false;

	bool bRc = true;

	AkVarLenDataCreator creator( AkMonitorData::MonitorDataMeter );

	MonitorSerializer& serializer = creator.GetSerializer();

	// Create place holder for final counter and flag
	AkUInt32 headerPtr = serializer.Count();
	bRc = bRc && serializer.Put( uNumBusses );
	bRc = bRc && serializer.Put( fHdrMeter );

	for( AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL )
	{
		AkVPL* pVPL = (*iterVPL);

		const CAkBusCtx busCtx = (*iterVPL)->m_MixBus.GetBusContext();
		if(!busCtx.HasBus())
			continue;

		AkDevice* pDevice = CAkOutputMgr::FindDevice(busCtx);
		if (!pDevice)
			continue;

		bRc = bRc && BuildMeterMonitorData( pVPL->m_MixBus.ID(), pVPL, &(pVPL->m_MixBus), serializer, uNumBusses, fHdrMeter );
		
		// If something went wrong, stop trying!
		if( ! bRc )
			break;
	}

	// Patch watch mask for this bus.
	if( bRc )
	{
		AkInt32 savePtr = serializer.Count();
		serializer.SetCount( headerPtr );
		bRc = bRc && serializer.Put( uNumBusses );
		bRc = bRc && serializer.Put( fHdrMeter );
		serializer.SetCount( savePtr );
	}

	// If something went wrong, pretend we never wrote anything!
	if( ! bRc )
	{
		AkInt32 writePtr = serializer.Count();
		creator.OutOfMemory( writePtr - headerPtr );
		serializer.SetCount( headerPtr );
	}

#endif
}

void _CountSends(CAkVPL3dMixable * in_pCbx, AkUInt32& io_ulNumSends)
{
	CAkBehavioralCtx * pCtx = in_pCbx->GetContext();
	if (pCtx)
		io_ulNumSends += in_pCbx->GetNumSends();
}

void _FillSendsData(CAkVPL3dMixable * in_pCbx, AkMonitorData::SendsData*& io_pSendsData)
{
	CAkBehavioralCtx * pCtx = in_pCbx->GetContext();
	if (pCtx)
	{
		const AkAuxSendArray & sends = in_pCbx->GetSendsArray();
		AkUInt32 uNumUser = 0, uNumGame = 0;

		for (AkAuxSendArray::Iterator it = sends.Begin(); it != sends.End(); ++it)
		{
			io_pSendsData->pipelineID = pCtx->GetPipelineID();
			io_pSendsData->gameObjID = pCtx->GetGameObjectPtr()->ID();
			io_pSendsData->soundID = pCtx->GetParameterNode() ? pCtx->GetParameterNode()->ID() : AK_INVALID_UNIQUE_ID;

			io_pSendsData->auxBusID = (*it).auxBusID;
			io_pSendsData->busObjID = (*it).listenerID;	// REVIEW This does not scale to multiple instances of aux busses. Sends should be dealt with with connections.
			io_pSendsData->fControlValue= (*it).fControlValue;

			if ((*it).eAuxType == ConnectionType_UserDefSend)
				io_pSendsData->eSendType = (AkMonitorData::SendType)(AkMonitorData::SendType_UserDefinedSend0 + (uNumUser++));
			else if ((*it).eAuxType == ConnectionType_GameDefSend)
				io_pSendsData->eSendType = (AkMonitorData::SendType)(AkMonitorData::SendType_GameDefinedSend0 + (uNumGame++));
			else if ((*it).eAuxType == ConnectionType_ReflectionsSend)
				io_pSendsData->eSendType = (AkMonitorData::SendType)(AkMonitorData::SendType_ReflectionsSend);

			io_pSendsData++;
		}
	}
}

void CAkLEngine::PostSendsData()
{
	AkUInt32 ulNumSends = 0;

	for (AkArrayVPLSrcs::Iterator iterVPLSrc = m_Sources.Begin(); iterVPLSrc != m_Sources.End(); ++iterVPLSrc)
	{
		CAkVPLSrcCbxNode * pCbx = *iterVPLSrc;
		_CountSends(pCbx, ulNumSends);
	}
	for (AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL)
	{
		AkVPL* pVPL = (*iterVPL);
		_CountSends(&pVPL->m_MixBus, ulNumSends);
	}

    AkInt32 sizeofItem = SIZEOF_MONITORDATA_TO( sendsData.sends )
						+ ulNumSends * sizeof( AkMonitorData::SendsData );

	AkProfileDataCreator creator(sizeofItem);
	if (!creator.m_pData)
		return;

	creator.m_pData->eDataType = AkMonitorData::MonitorDataSends;
	creator.m_pData->sendsData.ulNumSends = ulNumSends;

	AkMonitorData::SendsData* pSendsData = creator.m_pData->sendsData.sends;
	for (AkArrayVPLSrcs::Iterator iterVPLSrc = m_Sources.Begin(); iterVPLSrc != m_Sources.End(); ++iterVPLSrc)
	{
		_FillSendsData(*iterVPLSrc, pSendsData);
	}
	for (AkUInt32 iVPL = 0; iVPL < m_arrayVPLs.Length(); ++iVPL)
	{
		_FillSendsData(&m_arrayVPLs[iVPL]->m_MixBus, pSendsData);
	}
}

#ifndef AK_OPTIMIZED
void CAkLEngine::RecapParamDelta()
{
	for (AkUInt32 i = 0; i < m_arrayVPLs.Length(); ++i)
	{
		CAkBehavioralCtx* pCtx = m_arrayVPLs[i]->m_MixBus.GetContext();
		if (pCtx != nullptr)
		{
			pCtx->ForceParametersDirty();
			pCtx->CalcEffectiveParams();
			m_arrayVPLs[i]->m_MixBus.RecapPluginParamDelta();
		}
	}

	for (AkUInt32 i = 0; i < m_Sources.Length(); ++i)
	{
		if (m_Sources[i]->GetContext() != nullptr)
		{
			m_Sources[i]->RecapPluginParamDelta();
		}
	}
}
#endif

void CAkLEngine::ForAllMixContext(AkForAllMixCtxFunc in_funcForAll, void * in_pCookie)
{
	for (AkUInt32 i = 0; i < m_arrayVPLs.Length(); ++i)
	{
		CAkBehavioralCtx *pCtx = m_arrayVPLs[i]->m_MixBus.GetContext();
		if (pCtx)
		{
			in_funcForAll(pCtx, in_pCookie);
		}
	}
}

#endif

//-----------------------------------------------------------------------------
// Name: ParamNotification
// Desc: Generic update of parameters
//
// Parameters:
//	AkUniqueID   in_MixBusID     : ID of a specified bus.
//	NotifParams   in_rParams     : Notifications parameters struct.
//
// Return:
//	None.
//-----------------------------------------------------------------------------
void CAkLEngine::MixBusParamNotification( AkUniqueID in_MixBusID, NotifParams& in_rParams )
{
	bool in_bRTPCChange =
		in_rParams.eType == RTPC_OutputBusVolume ||
		in_rParams.eType == RTPC_OutputBusLPF ||
		in_rParams.eType == RTPC_OutputBusHPF;
	
	// Find the bus and set the volume.
	for ( AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL )
	{
		AkVPL * pVPL = *iterVPL;

		if ( pVPL->m_MixBus.ID() == in_MixBusID && pVPL->m_MixBus.GetContext() )
		{			
			if (in_bRTPCChange)
				pVPL->m_MixBus.GetContext()->UpdateTargetParam( in_rParams.eType, in_rParams.fValue, in_rParams.fDelta);
			else
			{
#ifndef AK_OPTIMIZED
				AkDeltaMonitor::LogUpdate(g_AkRTPCToPropID[in_rParams.eType], in_rParams.fValue, pVPL->m_MixBus.ID(), pVPL->m_MixBus.GetContext()->GetPipelineID());
#endif
				pVPL->m_MixBus.GetContext()->NotifyParamChanged(true, in_rParams.eType);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Name: ResetBusVolume
// Desc: Set the volume of a specified bus.
//
// Parameters:
//	AkUniqueID   in_MixBusID     : ID of a specified bus.
//	AkVolumeValue in_Volume : Volume to set.
//-----------------------------------------------------------------------------
void CAkLEngine::ResetBusVolume( AkUniqueID in_MixBusID )
{
	// Find the bus and set the volume.
	for( AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL )
	{
		CAkVPLMixBusNode& mixBus = (*iterVPL)->m_MixBus;
		if (mixBus.ID() == in_MixBusID && mixBus.GetContext())
		{
			mixBus.GetContext()->NotifyParamChanged(false, RTPC_BusVolume);
		}
	}

} // ResetBusVolume

void CAkLEngine::EnableVolumeCallback( AkUniqueID in_MixBusID, bool in_bEnable )
{
	// Find the bus and set the volume.
	for( AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL )
	{
		AkVPL * pVPL = *iterVPL;

		if( pVPL->m_MixBus.ID() == in_MixBusID )
		{
			pVPL->m_MixBus.EnableVolumeCallback( in_bEnable );
		}
	}
}

void CAkLEngine::EnableMeteringCallback( AkUniqueID in_MixBusID, AkMeteringFlags in_eMeteringFlags )
{
	// Find the bus and set the volume.
	for( AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL )
	{
		AkVPL * pVPL = *iterVPL;

		if( pVPL->m_MixBus.ID() == in_MixBusID )
		{
			pVPL->m_MixBus.EnableMeterCallback( in_eMeteringFlags );
		}
	}
}

//-----------------------------------------------------------------------------
// Name: BypassBusFx
// Desc: Bypass the effect of a specified bus.
//
// Parameters:
//	AkUniqueID in_MixBusID    : ID of a specified bus.
//	bool	   in_bIsBypassed : true=bypass effect, false=do not bypass.
//
// Return:
//	None.
//-----------------------------------------------------------------------------
void CAkLEngine::BypassBusFx( AkUniqueID in_MixBusID, AkUInt32 in_bitsFXBypass, AkUInt32 in_uTargetMask, CAkRegisteredObj * in_GameObj )
{
	// Find the bus and bypass the fx.
	for( AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL )
	{
		AkVPL * pVPL = *iterVPL;

		if( pVPL->m_MixBus.ID() == in_MixBusID && (!in_GameObj || pVPL->m_MixBus.GameObjectID() == in_GameObj->ID()))
		{
			pVPL->m_MixBus.SetInsertFxBypass( in_bitsFXBypass, in_uTargetMask );
		}
	}

} // BypassBusFx

void CAkLEngine::PositioningChangeNotification( AkUniqueID in_MixBusID, AkReal32 in_RTPCValue, AkRTPC_ParameterID in_ParameterID )
{
	for( AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL )
	{
		AkVPL * pVPL = *iterVPL;

		if (pVPL->m_MixBus.ID() == in_MixBusID && pVPL->m_MixBus.GetContext())
		{
			pVPL->m_MixBus.GetContext()->PositioningChangeNotification( in_RTPCValue, in_ParameterID );
		}
	}
}

#ifndef AK_OPTIMIZED
void CAkLEngine::InvalidatePositioning(AkUniqueID in_MixBusID)
{
	for (AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL)
	{
		AkVPL * pVPL = *iterVPL;

		if (pVPL->m_MixBus.ID() == in_MixBusID && pVPL->m_MixBus.GetContext())
		{
			pVPL->m_MixBus.GetContext()->InvalidatePositioning();
		}
	}
}

void CAkLEngine::RefreshMonitoringMuteSolo()
{
	bool bSoloActiveGameObj = g_pRegistryMgr->IsMonitoringSoloActive();
	bool bSoloActiveVoice = CAkParameterNodeBase::IsVoiceMonitoringSoloActive();
	bool bSoloActiveBus = CAkParameterNodeBase::IsBusMonitoringSoloActive();

	// Clear Voices
	for (AkArrayVPLSrcs::Iterator iterVPLSrc = m_Sources.Begin(); iterVPLSrc != m_Sources.End(); ++iterVPLSrc)
	{
		(*iterVPLSrc)->ClearMonitoringMuteSolo(bSoloActiveVoice || bSoloActiveBus || bSoloActiveGameObj);
	}

	// Clear Busses
	for (AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL)
	{
		if ((*iterVPL)->m_MixBus.GetContext() != NULL)
			((CAkMixBusCtx*)(*iterVPL)->m_MixBus.GetContext())->ClearMonitoringMuteSolo(bSoloActiveBus || bSoloActiveGameObj);
	}


	//** It is important to update buses before voices, because they propagate their solo state backwards to the voices.
	if (CAkParameterNodeBase::IsBusMonitoringMuteSoloActive() || g_pRegistryMgr->IsMonitoringMuteSoloActive())
	{
		for (AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL)
		{
			if ((*iterVPL)->m_MixBus.GetContext() != NULL)
				((CAkMixBusCtx*)(*iterVPL)->m_MixBus.GetContext())->RefreshMonitoringMuteSolo();
		}
	}
	
	if (CAkParameterNodeBase::IsAnyMonitoringMuteSoloActive() // Ctrl busses too!
		|| g_pRegistryMgr->IsMonitoringMuteSoloActive())
	{
		for (AkArrayVPLSrcs::Iterator iterVPLSrc = m_Sources.Begin(); iterVPLSrc != m_Sources.End(); ++iterVPLSrc)
		{
			if ((*iterVPLSrc)->GetContext() != NULL)
				((CAkPBI*)(*iterVPLSrc)->GetContext())->RefreshMonitoringMuteSolo();
		}
	}
}

#endif

void CAkLEngine::StopMixBussesUsingThisSlot( const CAkUsageSlot* in_pSlot )
{
	// Stop any bus currently using this slot.
	for( AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL )
	{
		AkVPL * pVPL = *iterVPL;
		if( pVPL->m_MixBus.IsUsingThisSlot( in_pSlot ) )
			pVPL->m_MixBus.DropFx();
	}
}

void CAkLEngine::ResetAllEffectsUsingThisMedia( const AkUInt8* in_pData )
{
	// Stop any bus currently using this slot.
	for( AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL )
	{
		if( (*iterVPL)->m_MixBus.IsUsingThisSlot( in_pData ) )
			(*iterVPL)->m_MixBus.SetAllInsertFx();
	}
}

void CAkLEngine::UpdateMixBusFX( AkUniqueID in_MixBusID, AkUInt32 in_uFXIndex )
{
	for( AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL )
	{
		AkVPL * l_pMixBus = *iterVPL;
		if( l_pMixBus->m_MixBus.ID() == in_MixBusID )
		{
			l_pMixBus->m_MixBus.SetInsertFx( in_uFXIndex );
		}
	}
}

void CAkLEngine::UpdateChannelConfig(AkUniqueID in_MixBusID)
{
	ReevaluateGraph(true);
}

//-----------------------------------------------------------------------------
// Name: ResolveCommandVPL
// Desc: Find a command's VPLSrc and VPL (for playing sounds, i.e. stop, pause, 
//       resume). Call only when resolution is needed (VPL and VPLSrc not set).
//       The command is resolved only if the source exists AND it is connected
//       to a bus.
//
// Parameter:
//	AkLECmd	& io_cmd: ( Stop, Pause, Resume ).
//-----------------------------------------------------------------------------
CAkVPLSrcCbxNode * CAkLEngine::ResolveCommandVPL( AkLECmd & io_cmd )
{
	// Check parameters. 
	AKASSERT( io_cmd.m_pCtx != NULL && 
		io_cmd.m_bSourceConnected == false && 
		(io_cmd.m_eType == AkLECmd::Type_Pause ||
		io_cmd.m_eType == AkLECmd::Type_Resume ||
		io_cmd.m_eType == AkLECmd::Type_StopLooping || 
		io_cmd.m_eType == AkLECmd::Type_Seek ) );

	CAkPBI * l_pCtx = io_cmd.m_pCtx;

	for( CAkLEngine::AkArrayVPLSrcs::Iterator iterVPLSrc = m_Sources.Begin(); iterVPLSrc != m_Sources.End();	++iterVPLSrc )
	{
		CAkVPLSrcCbxNode * pCbx = *iterVPLSrc;
		AKASSERT( pCbx != NULL );

		// Matching contexts found. WG-16144: only match with next source for stop looping command.
		if ( ( pCbx->m_pSources[ 0 ] && pCbx->m_pSources[ 0 ]->GetContext() == l_pCtx )
			|| ( io_cmd.m_eType == AkLECmd::Type_StopLooping && pCbx->m_pSources[ 1 ] && pCbx->m_pSources[ 1 ]->GetContext() == l_pCtx ) )
		{
			if( pCbx->GetState() != NodeStateInit )
			{
				// Set info.
				io_cmd.m_bSourceConnected = true;
			}
			return pCbx;
		}
	}

	// VPLSrc was not found. If it is unconnected, the PBI might still know about it;
	// if not, the command should be removed from the list.

	CAkVPLSrcCbxNode * pCbx = l_pCtx->GetCbx();

	// WG-16144: only match with next source for stop looping command.
	// WG-16743: Cbx may have been cleared if playback failed during this audio frame.
	if ( pCbx 
		&& ( ( pCbx->m_pSources[ 0 ] && pCbx->m_pSources[ 0 ]->GetContext() == l_pCtx )
			|| io_cmd.m_eType == AkLECmd::Type_StopLooping ) )
	{
		return pCbx;
	}

	return NULL;
} // ResolveCommandVPL

// IMPORTANT: Returns connected bus if and only if there was not already a connection pointing to it.
inline AkVPL * GetAndConnectBus(CAkBusCtx & in_ctxBus, CAkVPL3dMixable * in_pCbx, AkConnectionType in_eConnectionType)
{
	if (!in_pCbx->FindConnection(in_ctxBus))	// Connection search typically faster than bus search
	{
		AkVPL * pVPL = CAkLEngine::GetVPLMixBus(in_ctxBus);
		if (pVPL != NULL)
		{
			// Connect the source to VPL mix bus.
			in_pCbx->AddOutputBus(pVPL, in_eConnectionType);
		}
		return pVPL;
	}
	return NULL;
}

void CAkLEngine::GetAuxBus(CAkBus* in_pAuxBus, AkAuxSendValueEx & in_auxSend, CAkVPL3dMixable * in_pCbx)
{
	AKASSERT(in_auxSend.eAuxType != ConnectionType_Direct);
	CAkBusCtx busCtx(in_pAuxBus, in_auxSend.listenerID);
	GetAndConnectBus(busCtx, in_pCbx, in_auxSend.eAuxType);
}

AKRESULT CAkLEngine::EnsureVPLExists(CAkVPLSrcCbxNode * in_pCbx, CAkPBI *in_pCtx)
{
	AKRESULT eResult = AK_Success;

	if ( !in_pCbx->IsVirtualVoice() )
	{
		if ( in_pCbx->AddPipeline() != AK_Success )
			eResult = AK_Fail;
	}

	//The source list must be kept sorted by VPL.  Find where to insert.
	if ( eResult == AK_Success )
	{
		AkArrayVPLSrcs::Iterator it = m_Sources.Begin();
		while (it != m_Sources.End() && (*it)->SrcProcessOrder() < in_pCbx->SrcProcessOrder())
			++it;

		CAkVPLSrcCbxNode ** ppCbx = m_Sources.Insert(it - m_Sources.Begin());
		if (ppCbx)
			*ppCbx = in_pCbx;
		else
			eResult = AK_Fail;
	}

	// in_pCtx listeners are all listeners for the dry path, defined by the output bus. 
	if ( eResult == AK_Success )
		ConnectSourceToGraph(in_pCbx);
	else
		VPLDestroySource( in_pCbx, true );

	return eResult;
}

//-----------------------------------------------------------------------------
// Name: AddSound
// Desc: Add the sound to a new VPL or an existing VPL to play. 
//
// Parameters:
//	CAkPBI * in_pContext :	Pointer to a sound context.
//	AkUInt8	      in_bMode	  : Playing mode, in-game, or application.
//	bool		  in_bPaused  : true = sound is paused on play.
//
// Return: 
//  In general, remove the command from the list if the code is not AK_Success.
//	Ak_Success:          Sound was added.
//  AK_InvalidParameter: Invalid parameters.
//  AK_Fail:             Invalid pointer.
//  AK_AlreadyConnected: Sound is part of a sample accurate container, already
//                       part of a cbx node. The Play command should be removed.
//-----------------------------------------------------------------------------
AKRESULT CAkLEngine::AddSound( AkLECmd & io_cmd )
{
	// Check parameters.
	AKASSERT( io_cmd.m_pCtx != NULL && 
		io_cmd.m_bSourceConnected == false && 
		io_cmd.m_pCtx->GetCbx() == NULL );

	CAkPBI * l_pCtx	= io_cmd.m_pCtx;
	AKRESULT l_eResult = AK_Success;

	CAkVPLSrcCbxNode * pCbx = FindExistingVPLSrc( l_pCtx );
	if ( pCbx )
	{
		pCbx->AddSrc( l_pCtx, false );

		l_pCtx->NotifAddedAsSA();

		return AK_AlreadyConnected;
	}

	pCbx = AkNew(AkMemID_Processing, CAkVPLSrcCbxNode);
	if( pCbx == NULL )
	{
		l_pCtx->Destroy( CtxDestroyReasonPlayFailed );
		return AK_Fail;
	}

	l_eResult = pCbx->AddSrc( l_pCtx, true );
	if( l_eResult == AK_FormatNotReady )
	{
		// Format is not ready.
		// ProcessCommands() will resolve VPL connection when it can.
		// Store in list of sources not connected.
		CAkLEngineCmds::AddDisconnectedSource( pCbx );
		return AK_Success;
	}
	else if ( l_eResult != AK_Success)
	{
		// NOTE: on error the Context is destroyed in AddSrc();
		VPLDestroySource(pCbx, l_eResult != AK_PartialSuccess);
	}
	else
	{
 		l_eResult = EnsureVPLExists(pCbx, l_pCtx);
		io_cmd.m_bSourceConnected = (l_eResult == AK_Success);
	}

	return l_eResult;
} // AddSound

//-----------------------------------------------------------------------------
// Name: VPLTryConnectSource
// Desc: After having added a sound whose VPLCreateSource returned 
// 		 AK_FormatNotReady (pipeline not created), the renderer should call this
//		 function to complete the connection. 
//		 Tries adding pipeline, if successful, connects to bus.
//
// Parameters:
//	CAkPBI * in_pContext :	Pointer to a sound context.
//	AkVPLSrc * in_pVPLSrc	  : Incomplete VPL source.
//
// Return: 
//	Ak_Success:          VPL connection is complete.
//  AK_FormatNotReady: 	 Source not ready yet.
//  AK_Fail:             Error. Source is destroyed.
//-----------------------------------------------------------------------------
AKRESULT CAkLEngine::VPLTryConnectSource( CAkPBI * in_pContext, CAkVPLSrcCbxNode * in_pCbx )
{
	AKASSERT( !in_pContext->WasStopped() );

	AKRESULT l_eResult = in_pCbx->FetchStreamedData( in_pContext );
	if ( l_eResult == AK_FormatNotReady )
	{
		// Streamed source is not ready or 
		// Frame offset greater than an audio frame: keep it in list of sources not connected.
		return AK_FormatNotReady;
	}

	//---------------------------------------------------------------------------
	// Source ready: remove from non-connected srcs list, connect to bus.
	//---------------------------------------------------------------------------
	CAkLEngineCmds::RemoveDisconnectedSource( in_pCbx );

	// ...or there was an error.
	if ( l_eResult == AK_Success )
	{
		return EnsureVPLExists((CAkVPLSrcCbxNode*)in_pCbx, in_pContext);	
	}
	else
		VPLDestroySource( in_pCbx, true );

	return AK_Fail;
} // VPLTryConnectSource

AkVPL *	CAkLEngine::GetExistingVPLMixBus( const CAkBusCtx & in_ctxBus )
{
	for( AkArrayVPL::Iterator iterVPL = m_arrayVPLs.Begin(); iterVPL != m_arrayVPLs.End(); ++iterVPL )
	{
		CAkVPLMixBusNode & mixBus = (*iterVPL)->m_MixBus;

		if (mixBus.GetBusContext() == in_ctxBus && 
			(	mixBus.GetContext() == NULL || 
			mixBus.GetContext()->GetGameObjectPtr()->IsRegistered() )) // <- busses that are associated with unregistered objects can not be re-used by new voices.
		{
			return (*iterVPL);
		}
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Name: GetVPLMixBus
// Desc: Get the VPL mix bus that is associated with the source.
//
// Return: 
//	AkVPL * : Pointer to a AkVPL.
//-----------------------------------------------------------------------------
AkVPL *	CAkLEngine::GetVPLMixBus( CAkBusCtx & in_ctxBus )
{
	AkVPL *		l_pMixBus	= GetExistingVPLMixBus( in_ctxBus );

	if( l_pMixBus )
		return l_pMixBus;

	return GetVPLMixBusInternal( in_ctxBus );
}

AkVPL *	CAkLEngine::GetVPLMixBusInternal( CAkBusCtx& in_ctxBus )
{
	AkVPL * pVpl = NULL;
	CAkBusCtx parentCtx = in_ctxBus.FindParentCtx();
	if (parentCtx.HasBus())
	{
		if (in_ctxBus.GetBus()->HasListenerRelativeRouting())
		{
			CAkBusCtxRslvd resolvedBus = in_ctxBus;
			if (resolvedBus.IsValid())
			{
				const AkListenerSet& listenerSet = resolvedBus.GetGameObjPtr()->GetConnectedListeners().GetUserAssocs().GetListeners();
				if (!listenerSet.IsEmpty())
				{
					if ( resolvedBus.GetGameObjPtr()->CreateComponent<CAkEmitter>() )
					{

						AkUInt32 nParents = 0;
						AkVPL ** pParentBusses = (AkVPL **)alloca(listenerSet.Length()*sizeof(AkVPL *));
						for (AkListenerSet::Iterator itListener = listenerSet.Begin(); itListener != listenerSet.End(); ++itListener)
						{
							CAkBusCtx parentCtxForListener(parentCtx.GetBus(), *itListener);
							AkVPL* pBus = GetVPLMixBus(parentCtxForListener);
							if (pBus)
								pParentBusses[nParents++] = pBus;
						}

						if (nParents > 0)
						{
							pVpl = CreateVPLMixBus(in_ctxBus, pParentBusses[0]);
							if (pVpl != NULL)
							{
								for (AkUInt32 idx = 1; idx < nParents; ++idx)
									pVpl->m_MixBus.AddOutputBus(pParentBusses[idx], ConnectionType_Direct);
							}
						}
					}
				}
				else
				{
#ifndef AK_OPTIMIZED
					MONITOR_OBJECTBUSNOTIF(resolvedBus.GameObjectID(), AkMonitorData::NotificationReason_3dBusNoListener, resolvedBus.GetBus()->ID());
#endif
				}
			}
		}
		else
		{
			AkVPL * pParentBus = NULL;
			pParentBus = GetVPLMixBus(parentCtx);
			if (pParentBus != NULL)
				pVpl = CreateVPLMixBus(in_ctxBus, pParentBus);
		}
	}
	else
	{
		//Create top-level bus
		pVpl = CreateVPLMixBus(in_ctxBus, NULL);
	}
	return pVpl;
}

static AKRESULT AddVPLToArray(CAkLEngine::AkArrayVPL& in_array, AkVPL* in_pNewVpl)
{
	AKRESULT res = AK_Success;

	AkVPL** ppVPL;
	if (in_pNewVpl->IsTopLevelNode())
		ppVPL = in_array.Insert(0);
	else
		ppVPL = in_array.AddLast(); // Will be sorted later

	if (ppVPL != NULL)
	{
		*ppVPL = in_pNewVpl;
		CAkLEngine::SetVPLsDirty();
	}
	else
		res = AK_Fail;

	return res;
}

//-----------------------------------------------------------------------------
// Name: CreateVPLMixBus
// Desc: Create a bus of specified type.
//
// Parameters:
//	bool	   in_bEnviron : true = environmental bus, false = not an env bus.
//  AkUniqueID  in_BusID   : Bus id.
//
// Return: 
//	AkVPL * : Pointer to a AkVPL.
//-----------------------------------------------------------------------------
AkVPL * CAkLEngine::CreateVPLMixBus( CAkBusCtx in_BusCtx, AkVPL* in_pParentBus )
{
	if (in_pParentBus == NULL)
	{
		AkDevice* pDevice = CAkOutputMgr::CanConnectToDevice(in_BusCtx);
		if (!pDevice)
			return NULL; // Do not create the VPL if there is no output device.
		
		bool bAddVpl = false;
		in_pParentBus = pDevice->ConnectMix(bAddVpl);
		if (bAddVpl)
		{
			// A new top-level mix node was added in order to mix multiple hierarchies together - either primary+secondary ('merge-to-main') or 
			// multiple hierarchies for different listeners.  

			if (in_pParentBus == NULL)
				return NULL; //memory allocation failure.

			if (AddVPLToArray(m_arrayVPLs, in_pParentBus) != AK_Success)
			{
				pDevice->DeleteDeviceAndDisconnectMix();
				return NULL; //memory allocation failure.
			}

			// Find the the existing top level node (with the same device), and hook it up to the new parent node.
			for (AkUInt32 iVPL = 0; iVPL < m_arrayVPLs.Length(); ++iVPL)
			{
				AkVPL* pVpl = m_arrayVPLs[iVPL];
				if (pVpl != in_pParentBus && pVpl->IsTopLevelNode() && CAkOutputMgr::FindDevice(pVpl->m_MixBus.GetBusContext()) == pDevice)
				{
					pVpl->m_MixBus.AddOutputBus(in_pParentBus, ConnectionType_Direct);
					break;
				}
			}
		}
	}

	if (in_pParentBus && !in_BusCtx.GetBus())
		return in_pParentBus; //the anonymous 'device' bus returned from ConnectMix() was the bus we were trying to create all along.

	// Create the VPL mix bus.
	AkVPL * l_pMixBus;
	if ( in_BusCtx.HasBus() && in_BusCtx.GetBus()->IsHdrBus() )
	{
		l_pMixBus = AkNew(AkMemID_Processing, AkHdrBus(in_BusCtx));
	}
	else
	{
		l_pMixBus = AkNew(AkMemID_Processing, AkVPL);
	}

	if( l_pMixBus == NULL )
		return NULL;

	AKRESULT l_eResult = l_pMixBus->Init(in_BusCtx, in_pParentBus);

	if (l_eResult == AK_Success)
	{
		l_eResult = AddVPLToArray(m_arrayVPLs, l_pMixBus);
	}

	if( l_eResult != AK_Success)
	{
		AkDelete( AkMemID_Processing, l_pMixBus );
		l_pMixBus = NULL;
	}
	return l_pMixBus;
} // CreateVPLMixBus

void CAkLEngine::DestroyAllVPLMixBusses()
{
	// Destroy from end of array so that child busses are destroyed before their parents
	for ( int iVPL = m_arrayVPLs.Length() - 1; iVPL >= 0; --iVPL )
		AkDelete( AkMemID_Processing, m_arrayVPLs[ iVPL ] );

	m_arrayVPLs.RemoveAll();
}

//-----------------------------------------------------------------------------
// Name: FindExistingVPLSrc
// Desc: Find an existing VPLSrc for the given PBI. Used for sample-accurate transitions.
//-----------------------------------------------------------------------------
CAkVPLSrcCbxNode * CAkLEngine::FindExistingVPLSrc( CAkPBI * in_pCtx )
{
	AkUniqueID l_ID = in_pCtx->GetSequenceID();
	if( l_ID == AK_INVALID_SEQUENCE_ID )
		return NULL;

	for( CAkLEngine::AkArrayVPLSrcs::Iterator iterSrc = m_Sources.Begin(); iterSrc != m_Sources.End(); ++iterSrc )
	{
		// Should only need to look at the first sound to see if there is a match.
		CAkVPLSrcCbxNode * pCbx = *iterSrc;

		if (pCbx->GetPBI()->GetSequenceID() == l_ID)
		{
			AKASSERT(MAX_NUM_SOURCES > 1);
			if (pCbx->m_pSources[ (MAX_NUM_SOURCES-1)] != NULL)
				continue; // This cbx is already full, cannot stitch to this one. Breaking a sample accurate container in its transition area could make it impossible to stitch. see WG-39692.
			return pCbx;
		}
	}

	return NULL;
} // FindExistingVPLSrc

bool CAkLEngine::SortSiblingVPLs(AkArrayVPL & in_VPLs, AkInt32 in_iDepth, AkInt32 in_iRangeStart, AkInt32 in_iRangeLen)
{
	AkUInt32 * pNumConnections = (AkUInt32*)AkAlloca(in_iRangeLen * sizeof(AkUInt32));
	AkUInt32 uMaxConnections = 0;

	for (AkInt32 i = 0; i < in_iRangeLen; ++i)
	{
		AkUInt32 uNumConnections = in_VPLs[in_iRangeStart + i]->m_MixBus.GetNumForwardConnectionsToDepth(in_iDepth);
		pNumConnections[i] = uNumConnections;
		if (uNumConnections > uMaxConnections)
			uMaxConnections = uNumConnections;
	}

	if (uMaxConnections == 0) // early return for common case -- no sibling sort necessary.
		return false;
	
	AkVPL** pTmpVPLs = (AkVPL**)AkAlloca(in_iRangeLen * sizeof(AkVPL*));
	memcpy(pTmpVPLs, &in_VPLs[in_iRangeStart], in_iRangeLen * sizeof(AkVPL*));

	AkUInt32 iConnectionsNum = uMaxConnections + 1;
	AkInt32 * pConnectionsCnt = (AkInt32*)AkAlloca(iConnectionsNum * sizeof(AkInt32));
	AkInt32 * pConnectionsIdx = (AkInt32*)AkAlloca(iConnectionsNum * sizeof(AkInt32));

	memset(pConnectionsCnt, 0, iConnectionsNum * sizeof(AkInt32));

	for (AkInt32 i = 0; i < in_iRangeLen; ++i)
		pConnectionsCnt[pNumConnections[i]] += 1;

	pConnectionsIdx[0] = 0;
	for (AkUInt32 i = 1; i < iConnectionsNum; ++i)
		pConnectionsIdx[i] = pConnectionsIdx[i - 1] + pConnectionsCnt[i - 1];

	for (AkInt32 i = 0; i < in_iRangeLen; ++i)
	{
		AkVPL * pVPL = pTmpVPLs[i];
		in_VPLs[pConnectionsIdx[pNumConnections[i]]++ + in_iRangeStart] = pVPL;
	}

	return true;
}

//---------------------------------------------------------------------
// Check if the Voice is starving
//---------------------------------------------------------------------
void CAkLEngine::HandleStarvation()
{
	//Check if any device reported starvation.
	for(AkDeviceList::Iterator it = CAkOutputMgr::OutputBegin(); it != CAkOutputMgr::OutputEnd(); ++it)
	{
		if( (*it)->Sink()->IsStarved() )
		{
			(*it)->Sink()->ResetStarved();

			AkUInt32 uTimeNow = g_pAudioMgr->GetBufferTick();
			if( m_uLastStarvationTime == 0 || uTimeNow - m_uLastStarvationTime > MINIMUM_STARVATION_NOTIFICATION_DELAY )
			{
				MONITOR_ERROR( AK::Monitor::ErrorCode_VoiceStarving );
				m_uLastStarvationTime = uTimeNow;
				break;
			}
		}
	}
}

// bDestroyAll = false	:: Destroy mix busses that do not reach a device end point (and thier predecessors).
// bDestroyAll = true	:: Indiscriminately destry all mix busses and reconnect sources; for live edit changes.
void CAkLEngine::ReevaluateGraph(bool bDestroyAll)
{
	if (m_bFullReevaluation)
		return;

	m_bFullReevaluation = true;
	for (AkArrayVPL::Iterator it = m_arrayVPLs.Begin(); it != m_arrayVPLs.End(); )
	{
		AkVPL* pVpl = (*it);
		CAkVPLMixBusNode& mixBus = pVpl->m_MixBus;
		
		bool bDelete = true;
		
		if (!bDestroyAll) 
		{
			//Disconnect nodes to obsolete outputs from the top-down.
			DisconnectMixable(&mixBus);

			// Delete this node if it is a 'loose end',
			bDelete = (mixBus.NumDirectConnections() == 0);

			if (bDelete && pVpl->IsTopLevelNode())
			{
				// For top level nodes, delete if there is no device, or the config doesn't match.
				AkDevice* pDevice = CAkOutputMgr::FindDevice(mixBus.GetBusContext());
				if (pDevice)
					bDelete = (pDevice->GetSpeakerConfig() != mixBus.GetOutputConfig());
			}
		}
		
		if (bDelete)
		{
			AkDelete(AkMemID_Processing, pVpl);
			it = m_arrayVPLs.Erase(it);
			SetVPLsDirty();
		}
		else
		{
			++it;
		}
	}

	// Ensure all sources are connected (via direct path - aux connections are refreshed at each frame)
	for (AkArrayVPLSrcs::Iterator itSrc = m_Sources.Begin(); itSrc != m_Sources.End(); ++itSrc)
	{
		CAkVPLSrcCbxNode* pSrc = *itSrc;
		DisconnectMixable(pSrc);
		ConnectSourceToGraph(pSrc);
	}

	m_bFullReevaluation = false;
}

void CAkLEngine::DisconnectMixable(CAkVPL3dMixable* pCbx)
{
	CAkBehavioralCtx * pCtx = pCbx->GetContext();
	if (pCtx != NULL)
	{
		if (pCtx->HasListenerRelativeRouting())
		{
			const AkListenerSet& listenerSet = pCtx->GetGameObjectPtr()->GetConnectedListeners().GetUserAssocs().GetListeners();
			pCbx->DestroyObsoleteConnections(listenerSet);
		}
		else
		{
			AkGameObjectID gameObjectId = pCtx->GetGameObjectPtr()->ID();
			pCbx->DestroyObsoleteConnections(gameObjectId);
		}
	}
}

void CAkLEngine::ConnectSourceToGraph(CAkVPLSrcCbxNode* pCbx)
{
	CAkPBI * pCtx = pCbx->GetPBI();
	{
		CAkBus* pOutputBus = pCtx->GetOutputBusPtr();
		if (pOutputBus != NULL)
		{
			if (pCtx->HasListenerRelativeRouting())
			{
				// Positioning enabled.  Create a direct output connection to a bus instance for each listener.
				const AkListenerSet& listenerSet = pCtx->GetGameObjectPtr()->GetConnectedListeners().GetUserAssocs().GetListeners();
				for (AkListenerSet::Iterator itListener = listenerSet.Begin(); itListener != listenerSet.End(); ++itListener)
				{
					CAkBusCtx busCtx(pOutputBus, (*itListener));
					GetAndConnectBus(busCtx, pCbx, ConnectionType_Direct);
				}
			}
			else
			{
				// GameObject Positioning is not enabled.  The output bus will use the same game object as the source emitter.
				AkGameObjectID gameObjectId = pCtx->GetGameObjectPtr()->ID();
				CAkBusCtx busCtx(pOutputBus, gameObjectId);
				GetAndConnectBus(busCtx, pCbx, ConnectionType_Direct);
			}
		}
		else
		{			
			// pOutputBus is NULL - Create a direct connection to the MAIN device to support the "Play - bypass properties" feature.
			// This is used in Wave Viewer too
			AkGameObjectID gameObjectId = pCtx->GetGameObjectPtr()->ID();			
			AkDevice* pDevice = CAkOutputMgr::GetPrimaryDevice();
			if (pDevice)
			{
				CAkBusCtx busCtx(pOutputBus, gameObjectId);
				GetAndConnectBus(busCtx, pCbx, ConnectionType_Direct);
			}
		}
	}

#ifndef AK_OPTIMIZED
	if (!pCbx->HasConnections())
	{
		// Report if the voice was not able to connect to a graph.
		pCtx->Monitor( pCtx->GetGameObjectPtr()->GetListeners().IsEmpty()	? AkMonitorData::NotificationReason_VirtualNoListener 
																			: AkMonitorData::NotificationReason_VirtualNoDevice);
	}
#endif

}

void CAkLEngine::FinishRun( CAkVPLSrcCbxNode * in_pCbx, AkVPLState & io_state )
{
	// PBI state effective at end-of-frame.
	CAkPBI * pCtx = in_pCbx->m_pSources[ 0 ]->GetContext();
	bool bStop = in_pCbx->GetState() == NodeStateStop || pCtx->WasStopped();
	bool bPause = pCtx->WasPaused() && ( in_pCbx->GetState() == NodeStatePlay );

	if ( io_state.result == AK_NoMoreData && !bStop )
	{
		CAkVPLSrcNode * pNextSrc = in_pCbx->m_pSources[ 1 ];
		if ( pNextSrc )
		{
			// This sample-accurate voice could not be handled sample-accurately; do it here for
			// next frame.

			in_pCbx->m_pSources[ 1 ] = NULL;

			in_pCbx->RemovePipeline( CtxDestroyReasonFinished );

			AKRESULT eAddResult = in_pCbx->AddSrc( pNextSrc, true, false );

			if ( eAddResult == AK_Success )
				eAddResult = in_pCbx->AddPipeline();

			if ( eAddResult == AK_Success )
			{
				pNextSrc->Start();
			}
			else 
			{
				if ( eAddResult == AK_FormatNotReady )
				{
					// Missed the boat. Too bad. Return as NoMoreData (what we stitched so far)
					// REVIEW: this will cause this sound to be skipped, but we can't return DataReady
					// as the pipeline doesn't support running voices that are not IsIOReady()
					MONITOR_SOURCE_ERROR( AK::Monitor::ErrorCode_TransitionNotAccurateStarvation, pNextSrc->GetContext() );
				}

				in_pCbx->Stop();
			}
		}
		else
		{
			in_pCbx->Stop();
		}
	}
	else if ( io_state.result == AK_Fail || bStop )
	{
		in_pCbx->Stop();

	}
	else if ( bPause )
		in_pCbx->Pause();
}

void CAkLEngine::AnalyzeMixingGraph()
{
	WWISE_SCOPED_PROFILE_MARKER("CAkLEngine::AnalyzeMixingGraph");

	// First pass: Compute cumulative bus gains, setup voice volume rays and send values, create aux busses.
	{
#ifndef AK_OPTIMIZED

		if ( AkMonitor::GetAndClearMeterWatchesDirty() )
		{
			// Other mix nodes.
			AkArrayVPL::Iterator it = m_arrayVPLs.Begin();
			while ( it != m_arrayVPLs.End() )
			{
				(*it)->m_MixBus.RefreshMeterWatch();
				++it;
			}
		}
#endif
		// Compute volume for sources.  Aux Bus pertaining to each source will be updated too.
		for (CAkLEngine::AkArrayVPLSrcs::Iterator itSrc = m_Sources.Begin(); itSrc != m_Sources.End(); ++itSrc)
		{
			if ( (*itSrc)->GetState() == NodeStatePlay )
				(*itSrc)->ComputeVolumeRays();
		}

		// Evaluate all the busses that have aux sends.  This may create more vpl bus nodes or remove old ones.
		CAkMixBusCtx::ManageAuxRoutableBusses();

		SortVPLs();

		AkUInt32 uVPL = 0;
		while ( uVPL < m_arrayVPLs.Length() )
		{
			CAkVPLMixBusNode& mixBus = m_arrayVPLs[ uVPL ]->m_MixBus;

			mixBus.RefreshFx();
			mixBus.ComputeVolumeRays();

			if (mixBus.GetContext() && mixBus.HasConnections())
				mixBus.UpdateConnections(false);
			else
				mixBus.InitializeDownstreamGainDB();

#ifndef AK_OPTIMIZED
			mixBus.ResetMixingVoiceCount();
#endif
			++uVPL;
		}

		SortVPLs();
	}

	// Second pass, upwards: Voice setup (max volume) and HDR if applicable.

	// Process all sources
	for ( CAkLEngine::AkArrayVPLSrcs::Iterator itSrc = m_Sources.Begin(); itSrc != m_Sources.End(); ++itSrc)
	{
		if ( (*itSrc)->GetState() == NodeStatePlay )
			(*itSrc)->UpdateConnections(true);
	}

	for ( int iVPL = m_arrayVPLs.Length() - 1; iVPL >= 0; --iVPL )
	{
		AkVPL * pVPL = m_arrayVPLs[ iVPL ];
		if ( pVPL->IsHDR() ) 
			((AkHdrBus*)pVPL)->ComputeHdrAttenuation();
	}
	AkHdrBus::SendMaxHdrWindowTop();

	// Third pass
	// UpdateHDR() Has to be done begore Processing Limiters.
	for (CAkLEngine::AkArrayVPLSrcs::Iterator itSrc = m_Sources.Begin(); itSrc != m_Sources.End(); ++itSrc)
	{
		if ((*itSrc)->GetState() == NodeStatePlay)
			(*itSrc)->UpdateHDR();
	}

#ifndef AK_OPTIMIZED
	// Must be called each frame, because the routing of aux connections can change.
	CAkLEngine::RefreshMonitoringMuteSolo();
#endif
}

// -----------------------------------------------------------------
// AkVPL (software pipeline implementation)
// -----------------------------------------------------------------
AKRESULT AkVPL::Init(CAkBusCtx in_BusCtx, AkVPL* in_pParent)
{
	AkUInt16 usMaxFrames;
	AkChannelConfig channelConfig;

	usMaxFrames = LE_MAX_FRAMES_PER_BUFFER;
	AkDevice * pDevice = CAkOutputMgr::FindDevice(in_BusCtx);
	if(!pDevice)
		return AK_Fail;

	AkChannelConfig uParentConfig = in_pParent ? in_pParent->m_MixBus.GetMixConfig() : pDevice->GetSpeakerConfig();
	if (in_BusCtx.GetChannelConfig().IsValid())
	{
		channelConfig = in_BusCtx.GetChannelConfig();
		// Use bus channel config instead of device, for forced configs (e.g. WwiseLite)
		if (!in_pParent)
			uParentConfig = channelConfig;
	}
	else
		channelConfig = uParentConfig;
	
	if (in_pParent)
	{
		CAkBusFX* pParentBusFx = (CAkBusFX*)(&in_pParent->m_MixBus);
		AKRESULT l_eResult = m_MixBus.Init(this, channelConfig, usMaxFrames, in_BusCtx, pParentBusFx);
		if (l_eResult == AK_Success)
		{
			m_MixBus.AddOutputBus(in_pParent, ConnectionType_Direct);
		}
		
		return l_eResult;
	}
	else
	{
		// Master bus.
		return m_MixBus.Init(this, channelConfig, usMaxFrames, in_BusCtx, NULL);
	}
}

// -----------------------------------------------------------------
// AkHdrBus
// -----------------------------------------------------------------

AkHdrBus::MaxHdrWinTopMap AkHdrBus::m_mapMaxHdrWinTops;

AkHdrBus::AkHdrBus(CAkBusCtx in_BusCtx )
: m_fHdrMaxVoiceVolume( AK_SAFE_MINIMUM_VOLUME_LEVEL )
, m_fHdrWinTopState( AK_SAFE_MINIMUM_VOLUME_LEVEL )	// Use very small value to avoid releasing right from the start.
, m_fHdrWinTop(AK_SAFE_MINIMUM_VOLUME_LEVEL) // Initialize to something. Profiler may query this bus before ComputeHdrAttenuation() is called.
, m_fReleaseCoef( 0 )
{ 
	m_bIsHDR = true;
	
	CAkBus * pBus = in_BusCtx.GetBus();
	AKASSERT( pBus );

	// Cache gain computer the first time.
	AkReal32 fRatio;
	pBus->GetHdrGainComputer( m_fThreshold, fRatio );
	m_fGainFactor = ( 1 - ( 1.f / fRatio ) );

	// Cache ballistics the first time.
	AkReal32 fReleaseTime;
	bool bReleaseModeExponential;
	pBus->GetHdrBallistics( fReleaseTime, bReleaseModeExponential );
	m_bExpMode = bReleaseModeExponential;
	if ( fReleaseTime > 0 )
		m_fReleaseCoef = expf( -(AkInt32)AK_NUM_VOICE_REFILL_FRAMES / ( DEFAULT_NATIVE_FREQUENCY * fReleaseTime ) );
	else
		m_fReleaseCoef = 0;
}

#define AK_EPSILON_HDR_RELEASE	(0.5f)	// dBs
void AkHdrBus::ComputeHdrAttenuation()
{
	CAkBus * pBus = m_MixBus.GetBusContext().GetBus();
	AkGameObjectID busObjectID = m_MixBus.GetBusContext().GameObjectID();
	
	AkReal32 fDownstreamGainDB = DownstreamGainDB();
	// Derive gain computer only if necessary
	AkReal32 fHdrThreshold, fRatio;
	AkReal32 fGainFactor;
	if ( !pBus->GetHdrGainComputer( fHdrThreshold, fRatio ) )
		fGainFactor = m_fGainFactor;
	else
	{
		m_fThreshold = fHdrThreshold;
		fGainFactor = ( 1 - ( 1.f / fRatio ) );
		m_fGainFactor = fGainFactor;
	}

	// Compute window top target.
	// m_fHdrWinTopTarget is expressed globally (ie comprises downstream gain). Compute window target locally 
	// for this bus. Very important: we must not filter gain changes of downstream mix stages!
	AkReal32 fMaxVoiceVolume = m_fHdrMaxVoiceVolume - fDownstreamGainDB - fHdrThreshold;
	AkReal32 fWinTopTarget = fHdrThreshold;
	if ( fMaxVoiceVolume > 0 )
		fWinTopTarget += ( fGainFactor * fMaxVoiceVolume );

	// Derive ballistics only if necessary.
	AkReal32 fReleaseTime;
	bool bReleaseModeExponential;
	if ( pBus->GetHdrBallistics( fReleaseTime, bReleaseModeExponential ) )
	{
		if ( fReleaseTime > 0 )
			m_fReleaseCoef = expf( -(AkInt32)AK_NUM_VOICE_REFILL_FRAMES / ( DEFAULT_NATIVE_FREQUENCY * fReleaseTime ) );
		else
			m_fReleaseCoef = 0;

#ifndef AK_OPTIMIZED
		if ( bReleaseModeExponential != m_bExpMode )
		{
			// Release mode changed (live edit only). Ensure we take the "attack" path.
			m_bExpMode = bReleaseModeExponential;
			m_fHdrWinTopState = AK_SAFE_MINIMUM_VOLUME_LEVEL;
		}
#endif
	}
	AkReal32 fReleaseCoef = m_fReleaseCoef;
	
	// This is the target. Now, get the real one.
	AkReal32 fWindowTop;
	if ( bReleaseModeExponential )
	{
		// This is the target. Now, get the real one.
		if ( fWinTopTarget >= m_fHdrWinTopState )
		{
			// Instantaneous attack.
			m_fHdrWinTopState = fWinTopTarget;
		}
		else
		{
			// Releasing.
			m_fHdrWinTopState = fReleaseCoef * m_fHdrWinTopState + ( 1 - fReleaseCoef ) * fWinTopTarget;

			// Force us to remain alive if we're more than AK_EPSILON_HDR_RELEASE dB away from target value.
			if ( ( m_fHdrWinTopState - fWinTopTarget ) >= AK_EPSILON_HDR_RELEASE )
				m_bReferenced = true;
		}

		fWindowTop = m_fHdrWinTopState;
	}
	else
	{
		AkReal32 fWindowTopLin = AkMath::dBToLin( fWinTopTarget );

		if ( fWindowTopLin >= m_fHdrWinTopState )
		{
			// Instantaneous attack.
			m_fHdrWinTopState = fWindowTopLin;
			fWindowTop = fWinTopTarget;
		}
		else
		{
			// Releasing.
			m_fHdrWinTopState = fReleaseCoef * m_fHdrWinTopState + ( 1 - fReleaseCoef ) * fWindowTopLin;
			fWindowTop = AkMath::FastLinTodB( m_fHdrWinTopState );

			// Force us to remain alive if we're more than AK_EPSILON_HDR_RELEASE dB away from target value.
			if ( ( fWindowTop - fWinTopTarget ) >= AK_EPSILON_HDR_RELEASE )
				m_bReferenced = true;
		}
	}

	// Cache result for voices (global value). Push real window top to behavioral engine.
	m_fHdrWinTop = fWindowTop + fDownstreamGainDB;
	SaveMaxHdrWindowTop(fWindowTop, pBus, busObjectID);

	// Reset for next frame.
	m_fHdrMaxVoiceVolume = AK_SAFE_MINIMUM_VOLUME_LEVEL;
}

void AkHdrBus::SaveMaxHdrWindowTop(AkReal32 in_fWindowTop, CAkBus* in_pBus, AkGameObjectID in_BusObjectID)
{
	AkUniqueID gameParamID = in_pBus->GetGameParamID();
	if (gameParamID != AK_INVALID_UNIQUE_ID)
	{
		in_pBus->ClampWindowTop(in_fWindowTop);

		MaxHdrWinTopIDs currentIDs;
		currentIDs.gameParamID = gameParamID;
		currentIDs.gameObjectID = in_BusObjectID;

		MaxHdrWinTopInfo* pInfo = m_mapMaxHdrWinTops.Set(currentIDs);

		if (pInfo)
		{
			if (pInfo->fMaxHdrWindowTop < in_fWindowTop)
			{
				pInfo->fMaxHdrWindowTop = in_fWindowTop;
			}
		}
	}
}

void AkHdrBus::SendMaxHdrWindowTop()
{
	if (m_mapMaxHdrWinTops.IsEmpty())
		return;

	AkUniqueID currentGameParamID = m_mapMaxHdrWinTops.Begin().pItem->key.gameParamID;
	AkReal32 gameParamMaxHdrWinTop = -FLT_MAX;

	MaxHdrWinTopMap::Iterator it;
	for (it = m_mapMaxHdrWinTops.Begin(); it != m_mapMaxHdrWinTops.End(); ++it)
	{
		if (currentGameParamID != it.pItem->key.gameParamID)
		{
			// notify window top for the maximum of all game objects
			NotifyHdrWindowTop(gameParamMaxHdrWinTop, currentGameParamID, AK_INVALID_GAME_OBJECT);
		
			gameParamMaxHdrWinTop = -FLT_MAX;
			currentGameParamID = it.pItem->key.gameParamID;
		}

		AkGameObjectID currentGameObjectID = it.pItem->key.gameObjectID;
		AkReal32 currentMaxHdrWindowTop = it.pItem->fMaxHdrWindowTop;

		// notify window top for each game object with the same game param
		NotifyHdrWindowTop(currentMaxHdrWindowTop, currentGameParamID, currentGameObjectID);

		if (gameParamMaxHdrWinTop < currentMaxHdrWindowTop)
		{
			gameParamMaxHdrWinTop = currentMaxHdrWindowTop;
		}
	}

	// notify the last game param max HDR window top
	NotifyHdrWindowTop(gameParamMaxHdrWinTop, currentGameParamID, AK_INVALID_GAME_OBJECT);

	m_mapMaxHdrWinTops.RemoveAll();
}

void AkHdrBus::NotifyHdrWindowTop(AkReal32 in_fWindowTop, AkUniqueID in_GameParamID, AkGameObjectID in_BusObjectID)
{
	TransParams transParams;

	CAkRegisteredObj* pBusObject = NULL;
	if (in_BusObjectID != AK_INVALID_GAME_OBJECT)
	{
		pBusObject = g_pRegistryMgr->GetObjAndAddref(in_BusObjectID);
	}

	g_pRTPCMgr->SetRTPCInternal(in_GameParamID, in_fWindowTop, AkRTPCKey(pBusObject), transParams);

	if (pBusObject != NULL)
		pBusObject->Release();

#ifdef WWISE_AUTHORING
	CAkRegisteredObj * pTransportGameObj = g_pRegistryMgr->GetObjAndAddref(0); // GameObjects::GO_Transport
	if (pTransportGameObj)
	{
		g_pRTPCMgr->SetRTPCInternal(in_GameParamID, in_fWindowTop, pTransportGameObj, transParams);
		pTransportGameObj->Release();
	}
#endif
}

void AkHdrBus::TermHdrWinTopMap()
{
	m_mapMaxHdrWinTops.Term();
}

// -----------------------------------------------------------------

void CAkLEngine::RemoveMixBusses()
{
	// Destroy from end of array so that child busses are destroyed before their parents
	for ( int iVPL = m_arrayVPLs.Length() - 1; iVPL >= 0; --iVPL )
	{
		AkVPL * pVPL = m_arrayVPLs[ iVPL ];

		// Delete the bus if it is not in tail processing, has no actives sources and no child busses.
		if( pVPL->CanDestroy() )
		{
			m_arrayVPLs.Erase( iVPL );
			AkDelete(AkMemID_Processing, pVPL);
			SetVPLsDirty();
		}
		else
		{
			pVPL->OnFrameEnd();
		}
	}
}

// Optimized version of single-pipeline execution.
void CAkLEngine::RunVPL(CAkVPLSrcCbxNode * in_pCbx, AkVPLState & io_state)
{
	AkVPLSrcCbxRec & cbxRec = in_pCbx->m_cbxRec;

	AkUInt32 uFXIndex = AK_NUM_EFFECTS_PER_OBJ;

GetFilter:
	while ( uFXIndex > 0 )
	{
		CAkVPLFilterNodeBase * pFilter = cbxRec.m_pFilter[ --uFXIndex ];
		if ( pFilter )
		{
			pFilter->GetBuffer( io_state );
			if ( io_state.result != AK_DataNeeded )
			{
				if ( io_state.result == AK_DataReady
					|| io_state.result == AK_NoMoreData )
				{
					++uFXIndex;
					goto ConsumeFilter;
				}
				else
				{
					return;
				}
			}
		}
	}

GetSource:
	in_pCbx->GetBuffer(io_state);	
	if ( AK_EXPECT_FALSE(io_state.result != AK_DataReady && io_state.result != AK_NoMoreData) )
	{
		return;
	}	

ConsumeFilter:
#ifndef AK_OPTIMIZED
	if (g_settings.bDebugOutOfRangeCheckEnabled && !io_state.CheckValidSamples())
	{
		AK::Monitor::PostString("Wwise audio out of range: ConsumeFilter", AK::Monitor::ErrorLevel_Error, in_pCbx->GetPBI()->GetPlayingID(), in_pCbx->GetPBI()->GetGameObjectPtr()->ID(), in_pCbx->GetPBI()->GetSound()->ID());
	}
#endif

	while ( uFXIndex < AK_NUM_EFFECTS_PER_OBJ )
	{
		CAkVPLFilterNodeBase * pFilter = cbxRec.m_pFilter[ uFXIndex ];
		if ( pFilter )
		{
			pFilter->ConsumeBuffer( io_state );
			if ( io_state.result == AK_DataNeeded )
			{
				if ( uFXIndex > 0 )
				{
					goto GetFilter;
				}
				else
				{
					goto GetSource;
				}
			}
			else if ( io_state.result != AK_DataReady
 				&& io_state.result != AK_NoMoreData )
 			{
 				return;
			}
		}
		uFXIndex++;
	}

	cbxRec.m_BQF.ConsumeBuffer( io_state );
	AKASSERT( io_state.result == AK_DataReady
		|| io_state.result == AK_NoMoreData ); // LPF has no failure case

#ifndef AK_OPTIMIZED
	if (g_settings.bDebugOutOfRangeCheckEnabled && !io_state.CheckValidSamples())
	{
		AK::Monitor::PostString("Wwise audio out of range: after BQF", AK::Monitor::ErrorLevel_Error, in_pCbx->GetPBI()->GetPlayingID(), in_pCbx->GetPBI()->GetGameObjectPtr()->ID(), in_pCbx->GetPBI()->GetSound()->ID());
	}
#endif

	cbxRec.m_VolAutm.ConsumeBuffer( io_state );

#ifndef AK_OPTIMIZED
	if (g_settings.bDebugOutOfRangeCheckEnabled && !io_state.CheckValidSamples())
	{
		AK::Monitor::PostString("Wwise audio out of range: after volume automation", AK::Monitor::ErrorLevel_Error, in_pCbx->GetPBI()->GetPlayingID(), in_pCbx->GetPBI()->GetGameObjectPtr()->ID(), in_pCbx->GetPBI()->GetSound()->ID());
	}
#endif

	in_pCbx->ConsumeBuffer( io_state );

	if ( io_state.result != AK_DataReady
		&& io_state.result != AK_NoMoreData )
	{
		return;
	}

	// If we made it here, data needs to be mixed: set mixable buffer so that BusTask picks it up.
	io_state.eState = io_state.result;
	in_pCbx->SetMixableBuffer();

	in_pCbx->NotifyMarkers(&io_state);
}

void CAkLEngine::SoftwarePerform()
{
	WWISE_SCOPED_PROFILE_MARKER("CAkLEngine::SoftwarePerform");

	HandleStarvation();

	//If the engine was suspended without rendering, avoid doing so
	bool bRender = CAkOutputMgr::RenderIsActive();

	g_pPositionRepository->UpdateTime();

	CAkLEngineCmds::ProcessDisconnectedSources(LE_MAX_FRAMES_PER_BUFFER);

	AnalyzeMixingGraph();
	CAkURenderer::ProcessLimiters();// Has to be executed right after the Mix graph was analyzed so we know who was possibly audible.

	if (g_settings.taskSchedulerDesc.fcnParallelFor)
	{
		// Newer execution model, compatible with parallel_for
		ProcessGraph(bRender); 
	}
	else
	{
		// Older execution model, optimized for single-thread execution
		ProcessSources(bRender);
		PerformMixing(bRender);
	}

#ifndef AK_OPTIMIZED
	AkPerf::PostPipelineStats();
#endif

	// End of frame
	for (AkDeviceList::Iterator it = CAkOutputMgr::OutputBegin(); it != CAkOutputMgr::OutputEnd(); ++it)
		(*it)->FrameEnd();

	CAkOutputMgr::ResetMasterVolume(CAkOutputMgr::GetMasterVolume().fNext);	//Volume is now at target.

	RemoveMixBusses();
} // Perform

void CAkLEngine::VoiceRangeTask(void* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, AkTaskContext in_ctx, void* /*in_pUserData*/)
{
#if defined AK_CPU_X86 || defined AK_CPU_X86_64
#if defined (_MSC_VER) && (_MSC_VER < 1700)
	AkUInt32 uFlushZeroMode = _MM_GET_FLUSH_ZERO_MODE(dummy);
#else
	AkUInt32 uFlushZeroMode = _MM_GET_FLUSH_ZERO_MODE();
#endif
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif

	AkArrayVPLSrcs& srcs = *(AkArrayVPLSrcs*)in_pData;
	AkUInt32 iSrc = in_uIdxBegin;
	do
	{
		CAkVPLSrcCbxNode * pSrc = srcs[iSrc];
		if (pSrc->m_vplState.result == AK_DataNeeded)
			RunVPL(pSrc, pSrc->m_vplState);

#if defined(AK_HARDWARE_DECODING_SUPPORTED)
		if (pSrc->SrcProcessOrder() == SrcProcessOrder_HwVoice)
			pSrc->PrepareNextBuffer();
#endif
	} 
	while (++iSrc < in_uIdxEnd);

#if defined AK_CPU_X86 || defined AK_CPU_X86_64
	_MM_SET_FLUSH_ZERO_MODE(uFlushZeroMode);
#endif
}

void CAkLEngine::PreprocessSources(bool in_bRender, AkUInt32 &out_idxFirstHwVoice, AkUInt32 &out_idxFirstLLVoice)
{
	out_idxFirstHwVoice = ~0;
	out_idxFirstLLVoice = ~0;
	{
		WWISE_SCOPED_PROFILE_MARKER("CAkLEngine::PreprocessSources");

		// Voice pre-processing. StartRun on all startable voices and record whether they should run or not
		for (AkArrayVPLSrcs::Iterator itSrc = m_Sources.Begin(); itSrc != m_Sources.End(); ++itSrc)
		{
			CAkVPLSrcCbxNode * pCbx = *itSrc;
			AkVPLState & rVPLState = pCbx->m_vplState;

			rVPLState.result = AK_NoDataNeeded;

			AkUInt32 srcProcessOrder = pCbx->SrcProcessOrder();
			if (pCbx->GetState() == NodeStatePlay
				&& pCbx->StartRun(rVPLState)
				&& in_bRender)
			{
				// Clear only necessary fields of buffer
				// Keep request size ("max frames").
				AKASSERT(!rVPLState.HasData());
				rVPLState.uValidFrames = 0;
				rVPLState.ResetMarkers();
				rVPLState.posInfo.Invalidate();
				rVPLState.result = AK_DataNeeded;

#if defined(AK_HARDWARE_DECODING_SUPPORTED)
				if (srcProcessOrder == SrcProcessOrder_HwVoiceLowLatency)
				{
					pCbx->PrepareNextBuffer();
				}
#endif
			}

			// Check for sw/hw cutoff
			if (out_idxFirstHwVoice == ~0 && srcProcessOrder == SrcProcessOrder_HwVoice)
			{
				out_idxFirstHwVoice = itSrc - m_Sources.Begin();
			}
			if (out_idxFirstLLVoice == ~0 && srcProcessOrder == SrcProcessOrder_HwVoiceLowLatency)
			{
				out_idxFirstLLVoice = itSrc - m_Sources.Begin();
			}
		}
	}

#if defined(AK_HARDWARE_DECODING_SUPPORTED)
	// Send the decode commands to HW for low-latency voices immediately.
	if (out_idxFirstLLVoice != ~0)
	{
		WWISE_SCOPED_PROFILE_MARKER("CAkLEngine::DecodeHardwareSources::SameFrame");
		DecodeHardwareSources();
	}
#endif
}

void CAkLEngine::ProcessGraph(bool in_bRender)
{
	WWISE_SCOPED_PROFILE_MARKER("CAkLEngine::ProcessGraph");

	// Index of cutoff between sw and hw voices
	AkUInt32 idxFirstHwVoice = ~0;
	AkUInt32 idxFirstLLVoice = ~0;
	PreprocessSources(in_bRender, idxFirstHwVoice, idxFirstLLVoice);

	if ( idxFirstHwVoice == ~0 )
		idxFirstHwVoice = m_Sources.Length();

	// Process voices in parallel.
	if (m_Sources.Length() && in_bRender)
	{
		AkUInt32 hwPivot = AkMin(idxFirstHwVoice, idxFirstLLVoice);
		// Software voices are at the start (i.e. lower processing order)
		if ( hwPivot != 0 )
		{
			AK_INSTRUMENT_SCOPE("CAkLEngine::VoiceRangeTask::SwVoices");
			// We have some software voices, process them now
			g_settings.taskSchedulerDesc.fcnParallelFor(&m_Sources, 0, hwPivot, 1, VoiceRangeTask, NULL, "AK::Voice");
		}
		
		if ( hwPivot < m_Sources.Length() )
		{
			{
				AK_INSTRUMENT_SCOPE("CAkLEngine::WaitForHwVoices");
				PlatformWaitForHwVoices();
			}
			AK_INSTRUMENT_SCOPE("CAkLEngine::VoiceRangeTask::HwVoices");
			// We have some hardware voices, process them now
			g_settings.taskSchedulerDesc.fcnParallelFor(&m_Sources, hwPivot, m_Sources.Length(), 1, VoiceRangeTask, NULL, "AK::Voice");
		}
	}

#ifndef AK_OPTIMIZED
	if (AK::MemoryMgr::IsVoiceCheckEnabled())
	{
		AK::MemoryMgr::CheckForOverwrite();
	}
#endif

#if defined(AK_HARDWARE_DECODING_SUPPORTED)
	if (idxFirstHwVoice != ~0)
	{
		AK_INSTRUMENT_SCOPE("CAkLEngine::DecodeHardwareSources::NextFrame");
		DecodeHardwareSources();
	}
#endif

	// Bus DAG begin.
	if (in_bRender)
	{
		// Must test for sort dirty because sorted VPLs are absolutely necessary for proper mixing (i.e. bus topology must be respected)
		// Note: should NEVER be dirty at this point!  If dirty, then either:
		//	- a new codepath was introduced and SortVPLs was not called prior to this function
		//	- a memory allocation failure occured in SortVPLs.
		if (!m_arrayVPLs.IsEmpty() && !m_bVPLsDirty)
		{
			AkInt32 iDepthNum = m_arrayVPLDepthCnt.Length();
			int iMaxDepth = iDepthNum - 1;

			AkInt32 * pDepthIdx = (AkInt32*)AkAlloca(iDepthNum * sizeof(AkInt32));
			pDepthIdx[0] = 0;
			for (int i = 1; i < iDepthNum; ++i)
				pDepthIdx[i] = pDepthIdx[i - 1] + m_arrayVPLDepthCnt[i - 1];

			// All non-top-level busses can be executed in parallel.
			for (int i = iMaxDepth; i > 0; --i)
			{
				AkInt32 iRangeStart = pDepthIdx[i];
				AkInt32 iRangeLen = m_arrayVPLDepthCnt[i];
				if (iRangeLen > 1)
				{
					if (!(m_bVPLsHaveCycles && m_arrayVPLDepthInterconnects[i]))
					{
						g_settings.taskSchedulerDesc.fcnParallelFor(
							&m_arrayVPLs,
							iRangeStart,
							iRangeStart + iRangeLen,
							1,
							BusRangeTask,
							NULL,
							"AK::Bus");
					}
					else
					{
						AkInt32 iRangeLast = iRangeStart + iRangeLen - 1;
						for (AkInt32 i = iRangeLast; i >= iRangeStart; --i)
							BusTask(m_arrayVPLs[i]);
					}
				}
				else if (iRangeLen == 1)
				{
					BusTask(m_arrayVPLs[iRangeStart]);
				}
			}

			for (int i = m_arrayVPLDepthCnt[0] - 1; i >= 0; --i)
				BusTask(m_arrayVPLs[i]);

			for (int i = m_arrayVPLs.Length() - 1; i >= 0; --i)
			{
				m_arrayVPLs[i]->m_MixBus.ReleaseBuffer();
				if (m_bVPLsHaveCycles)
					FeedbackTask(m_arrayVPLs[i]);
			}
		}
	}

	// Process voice finalizations serially.
	for (AkArrayVPLSrcs::Iterator itSrc = m_Sources.Begin(); itSrc != m_Sources.End();)
	{
		CAkVPLSrcCbxNode * pCbx = *itSrc;
		AkVPLState & rVPLState = pCbx->m_vplState;

		if (rVPLState.result != AK_NoDataNeeded)
		{
			// Release buffer in all cases, except if the source has starved.
			// In such a case it is kept for next LEngine pass.
			if (rVPLState.result != AK_NoDataReady)
			{
				pCbx->ReleaseBuffer();
			}
			else
			{
				// We already computed new volumes, but did not use it because data is not ready.
				// Restore them for next time.
				pCbx->RestorePreviousVolumes(&rVPLState);
			}
		}

		FinishRun(pCbx, rVPLState);

		// Destroy voice when stopped
		if (pCbx->GetState() == NodeStateStop)
		{
			itSrc = m_Sources.Erase(itSrc);
			VPLDestroySource(pCbx);
		}
		else
		{
			++itSrc;
		}
	}

}

void CAkLEngine::ProcessSources(bool in_bRender)
{
	WWISE_SCOPED_PROFILE_MARKER("CAkLEngine::ProcessSources");
	
	AkUInt32 idxFirstHwVoice = ~0;
	AkUInt32 idxFirstLLVoice = ~0;
	PreprocessSources(in_bRender, idxFirstHwVoice, idxFirstLLVoice);

	// Voice processing.  Process each "sequence" as a whole.
	for ( AkArrayVPLSrcs::Iterator itSrc = m_Sources.Begin(); itSrc != m_Sources.End(); )
	{
		CAkVPLSrcCbxNode * pCbx = *itSrc;
		AkVPLState & rVPLState = pCbx->m_vplState;

		if (pCbx->m_vplState.result == AK_DataNeeded)
		{
			RunVPL(pCbx, rVPLState);

			// Release buffer in all cases, except if the source has starved.
			// In such a case it is kept for next LEngine pass.
			if (rVPLState.result != AK_NoDataReady)
			{
				AkAudioBuffer * pMixableBuffer = pCbx->m_pMixableBuffer;
				if (pMixableBuffer)
				{
					// Mix the voice into all output busses.
					for (AkMixConnectionList::Iterator it = pCbx->BeginConnection(); it != pCbx->EndConnection(); ++it)
					{
						AkMixConnection * pConnection = (*it);
						if (pConnection->IsAudible())
						{
							pConnection->GetOutputBus()->ConsumeBuffer(*pMixableBuffer, *pConnection);
						}
					}
				}
				pCbx->ReleaseBuffer();
			}
			else
			{
				// We already computed new volumes, but did not use it because data is not ready.
				// Restore them for next time.
				pCbx->RestorePreviousVolumes(&rVPLState);
			}
		}

#if defined(AK_HARDWARE_DECODING_SUPPORTED)
		if (pCbx->SrcProcessOrder() == SrcProcessOrder_HwVoice)
			pCbx->PrepareNextBuffer();
#endif

		FinishRun(pCbx, rVPLState);

		// Destroy voice when stopped
		if (pCbx->GetState() == NodeStateStop)
		{
			itSrc = m_Sources.Erase(itSrc);
			VPLDestroySource(pCbx);
		}
		else
		{
			++itSrc;
		}
	}

#ifndef AK_OPTIMIZED
	if (AK::MemoryMgr::IsVoiceCheckEnabled())
	{
		AK::MemoryMgr::CheckForOverwrite();
	}
#endif

#if defined(AK_HARDWARE_DECODING_SUPPORTED)
	if (idxFirstHwVoice != ~0)
	{
		WWISE_SCOPED_PROFILE_MARKER("CAkLEngine::DecodeHardwareSources::NextFrame");
		DecodeHardwareSources();
	}
#endif
}

void CAkLEngine::BusRangeTask(void* in_pData, AkUInt32 in_uIdxBegin, AkUInt32 in_uIdxEnd, AkTaskContext in_ctx, void* /*in_pUserData*/)
{
#if defined AK_CPU_X86 || defined AK_CPU_X86_64
#if defined (_MSC_VER) && (_MSC_VER < 1700)
	AkUInt32 uFlushZeroMode = _MM_GET_FLUSH_ZERO_MODE(dummy);
#else
	AkUInt32 uFlushZeroMode = _MM_GET_FLUSH_ZERO_MODE();
#endif
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif

    AkArrayVPL& sortedVPLs = *(AkArrayVPL*)in_pData;
	AkUInt32 iVPL = in_uIdxBegin;

	do
	{
		BusTask(sortedVPLs[iVPL]);
	} 
	while (++iVPL < in_uIdxEnd);

#if defined AK_CPU_X86 || defined AK_CPU_X86_64
	_MM_SET_FLUSH_ZERO_MODE(uFlushZeroMode);
#endif
}

void CAkLEngine::BusTask(AkVPL * in_pVPL)
{
	// For each dependency, try to consume it.
	const AkInputConnectionList & inputs = in_pVPL->m_MixBus.Inputs();
	for (AkInputConnectionList::Iterator it = inputs.Begin(); it != inputs.End(); ++it)
	{
		AkMixConnection * pThisConnection = *it;
		if (pThisConnection->IsAudible())
		{
			AkAudioBuffer * pMixableBuffer = pThisConnection->Owner()->m_pMixableBuffer;
			if (pMixableBuffer
				&& !pThisConnection->GetFeedback())
			{
				in_pVPL->m_MixBus.ConsumeBuffer(*pMixableBuffer, *pThisConnection);
			}
		}
	}

	// Push the normal bus buffer to the final mix or their parent mix.
	in_pVPL->m_MixBus.m_pMixableBuffer = CAkLEngine::TransferBuffer(in_pVPL);
}

void CAkLEngine::FeedbackTask(AkVPL * in_pVPL)
{
	// For each dependency, try to consume it.
	const AkInputConnectionList & inputs = in_pVPL->m_MixBus.Inputs();
	for (AkInputConnectionList::Iterator it = inputs.Begin(); it != inputs.End(); ++it)
	{
		AkMixConnection * pThisConnection = *it;
		if (pThisConnection->GetFeedback() 
			&& pThisConnection->IsAudible()
//			&& pThisConnection->Owner()->IsABus() is implied by GetFeedback
			)
		{
			AkAudioBuffer * pMixableBuffer = pThisConnection->Owner()->m_pMixableBuffer;
			if (pMixableBuffer)
			{
				in_pVPL->m_MixBus.ConsumeBuffer(*pMixableBuffer, *pThisConnection);
			}
		}
	}
}

void CAkLEngine::VPLRefreshDepth(AkVPL * in_pVPL, AkInt32 in_iDepth, AkInt32 & io_iMaxDepth, bool & io_bHasCycles)
{
	in_pVPL->m_bRecurseVisit = true;

	if (in_iDepth > io_iMaxDepth)
		io_iMaxDepth = in_iDepth;

	if (in_pVPL->m_iDepth == INT_MAX
		|| in_pVPL->m_iDepth < in_iDepth) // Max depth wins: 
	{
		in_pVPL->m_iDepth = in_iDepth;
	}

	const AkInputConnectionList & inputs = in_pVPL->m_MixBus.Inputs();
	for (AkInputConnectionList::Iterator it = inputs.Begin(); it != inputs.End(); ++it)
	{
		AkMixConnection * pThisConnection = *it;
		if (pThisConnection->Owner()->IsABus())
		{
			AkVPL * pInputVPL = (AkVPL *)((AkUInt8 *)pThisConnection->Owner() - offsetof(AkVPL, m_MixBus)); // FIXME: naughty casting
			pThisConnection->SetFeedback(pInputVPL->m_bRecurseVisit);
			if (!pInputVPL->m_bRecurseVisit)
			{
				VPLRefreshDepth(pInputVPL, in_iDepth + 1, io_iMaxDepth, io_bHasCycles);
			}
			else
			{
				io_bHasCycles = true;
			}
		}
	}

	in_pVPL->m_bRecurseVisit = false;
}

AKRESULT CAkLEngine::SortVPLs()
{
	if (!m_bVPLsDirty)
		return AK_Success;

	AkUInt32 cVPLs = m_arrayVPLs.Length();
	if (cVPLs > 0)
	{
		AkVPL ** pVPLs = m_arrayVPLs.Data();

		for (AkUInt32 iVPL = 0; iVPL < cVPLs; ++iVPL)
		{
			AkVPL * pVPL = pVPLs[iVPL];
			pVPL->m_iDepth = INT_MAX;
		}

		// Find primary set of VPLs,

		AkInt32 iMaxDepth = 0;
		m_bVPLsHaveCycles = false;

        for (AkUInt32 iVPL = 0; iVPL < cVPLs; ++iVPL)
        {
            AkVPL * pVPL = pVPLs[iVPL];
            if (pVPL->IsTopLevelNode())
                VPLRefreshDepth(pVPL, 0, iMaxDepth, m_bVPLsHaveCycles);
        }

		AkInt32 iDepthNum = iMaxDepth + 1;

		// Radix sort
		AkVPL ** pTmpVPLs = (AkVPL **)AkAlloca(cVPLs * sizeof(AkVPL *));

		if (!m_arrayVPLDepthCnt.Resize(iDepthNum)
			|| !m_arrayVPLDepthInterconnects.Resize(iDepthNum))
			return AK_Fail;

		AkInt32 * pVPLDepthCnt = m_arrayVPLDepthCnt.Data();
		memset(pVPLDepthCnt, 0, iDepthNum * sizeof(AkInt32));

		for (AkUInt32 iVPL = 0; iVPL < cVPLs; ++iVPL)
		{
			AkVPL * pVPL = pVPLs[iVPL];
			pTmpVPLs[iVPL] = pVPL;
			if (pVPL->m_iDepth != INT_MAX)
				pVPLDepthCnt[pVPL->m_iDepth] += 1;
		}

		AkInt32 * pDepthIdx = (AkInt32*)AkAlloca(iDepthNum * sizeof(AkInt32));
		pDepthIdx[0] = 0;
		for (AkInt32 i = 1; i < iDepthNum; ++i)
			pDepthIdx[i] = pDepthIdx[i - 1] + pVPLDepthCnt[i - 1];

		AkUInt32 iVPLMax = cVPLs;

		for (AkUInt32 iVPL = 0; iVPL < cVPLs; ++iVPL)
		{
			AkVPL * pVPL = pTmpVPLs[iVPL];
			if (pVPL->m_iDepth != INT_MAX)
				pVPLs[pDepthIdx[pVPL->m_iDepth]++] = pVPL;
			else
				pVPLs[--iVPLMax] = pVPL;
		}

		if (m_bVPLsHaveCycles)
		{
			m_arrayVPLDepthInterconnects[0] = false;

			for (AkInt32 i = 1; i < iDepthNum; ++i)
			{
				AkInt32 iRangeStart = pDepthIdx[i-1];
				AkInt32 iRangeLen = pVPLDepthCnt[i];
				if (iRangeLen > 1)
					m_arrayVPLDepthInterconnects[i] = SortSiblingVPLs(m_arrayVPLs, i, iRangeStart, iRangeLen);
				else
					m_arrayVPLDepthInterconnects[i] = false;
			}
		}
	}

	m_bVPLsDirty = false;
	return AK_Success;
}

void CAkLEngine::PerformMixing(bool in_bRender)
{
	WWISE_SCOPED_PROFILE_MARKER("CAkLEngine::PerformMixing");

	if (in_bRender)
	{
		// Bus DAG begin.

		// Must test for sort dirty because sorted VPLs are absolutely necessary for proper mixing (i.e. bus topology must be respected)
		// Note: should NEVER be dirty at this point!  If dirty, then either:
		//	- a new codepath was introduced and SortVPLs was not called prior to this function
		//	- a memory allocation failure occured in SortVPLs.
		if (!m_arrayVPLs.IsEmpty() && !m_bVPLsDirty)
		{
			for (int i = m_arrayVPLs.Length() - 1; i >= 0; --i)
			{
				AkVPL * pVPL = m_arrayVPLs[i];
				// For each dependency, try to consume it.
				const AkInputConnectionList & inputs = pVPL->m_MixBus.Inputs();
				for (AkInputConnectionList::Iterator it = inputs.Begin(); it != inputs.End(); ++it)
				{
					AkMixConnection * pThisConnection = *it;
					if (pThisConnection->IsAudible()
						&& pThisConnection->Owner()->IsABus())
					{
						AkAudioBuffer * pMixableBuffer = pThisConnection->Owner()->m_pMixableBuffer;
						if (pMixableBuffer
							&& !pThisConnection->GetFeedback())
						{
							pVPL->m_MixBus.ConsumeBuffer(*pMixableBuffer, *pThisConnection);
						}
					}
				}

				// Push the normal bus buffer to the final mix or their parent mix.
				pVPL->m_MixBus.m_pMixableBuffer = CAkLEngine::TransferBuffer(pVPL);
			}

			for (int i = m_arrayVPLs.Length() - 1; i >= 0; --i)
			{
				AkVPL * pVPL = m_arrayVPLs[i];
				pVPL->m_MixBus.ReleaseBuffer();
				if (m_bVPLsHaveCycles)
					FeedbackTask(pVPL);
			}
		}
	}
}

//--------------------------Resource Monitor--------------------------//

void CAkLEngine::UpdateResourceMonitorPipelineData() {
#ifndef AK_OPTIMIZED
	// Initialize data counters
	AkUInt16 cVoice = 0;
	AkUInt16 cVirtVoices = 0;

	// Voice pipeline data
	for (AkArrayVPLSrcs::Iterator iterVPLSrc = m_Sources.Begin(); iterVPLSrc != m_Sources.End(); ++iterVPLSrc)
	{
		CAkVPLSrcCbxNode * pCbx = *iterVPLSrc;

		//count total voices
		++cVoice;

		// count virtual voices
		if (pCbx->IsVirtualVoice())
			cVirtVoices++;
	}

	// Collecting data for exposed profiler monitoring
	AkResourceMonitor::SetTotalVoices(cVoice);
	AkResourceMonitor::SetVirtualVoices(cVirtVoices);
	AkResourceMonitor::SetPhysicalVoices(cVoice - cVirtVoices);
#endif // AK_OPTIMIZED
}