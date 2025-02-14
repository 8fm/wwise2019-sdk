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
// Ak3DListener.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkMath.h"
#include "Ak3DListener.h"
#include "AkDefault3DParams.h"
#include "AkSpeakerPan.h"
#include "AkEnvironmentsMgr.h"
#include "AkGen3DParams.h"
#include "AkMixConnection.h"
#include "AkPlayingMgr.h"
#include "AkOutputMgr.h"
#include "AkRegistryMgr.h"
#include "AkRegisteredObj.h"
#include "AkConnectedListeners.h"
#include "AkVPLMixBusNode.h"
#include "AkAudioLib.h"
#include "AkMonitor.h"

extern AkReal32 g_fVolumeThreshold;
extern AkReal32 g_fVolumeThresholdDB;

namespace
{
	// Approximate the loudness (linear) given a particular LPF filter value.
	AkReal32 GetLPFLoudness(AkReal32 in_lpfParam)
	{
		// White noise curve
		//AkReal32 lpfLoudness = 1.00023207e+00f + in_lpfParam * (-2.10334490e-02f + in_lpfParam * (1.02433311e-04f + in_lpfParam * (8.74901712e-08f)));

		// Pink noise curve
		AkReal32 lpfLoudness = 9.84305195e-01f + in_lpfParam * (-1.15175902e-03f + in_lpfParam * (-1.75465436e-04f + in_lpfParam * (9.30272209e-07f)));

		lpfLoudness = AkClamp(lpfLoudness, 0.0f, 1.0f);
		return lpfLoudness;
	}

	// Approximate the loudness (linear) given a particular HPF filter value.
	AkReal32 GetHPFLoudness(AkReal32 in_hpfParam)
	{
		AkReal32 hpfParam2 = in_hpfParam * in_hpfParam;
		AkReal32 hpfParam3 = hpfParam2 * in_hpfParam;
		
		// White noise curve
		//AkReal32 hpfLoudness = 9.99405535e-01f + in_hpfParam * (2.69540221e-04f + in_hpfParam * (2.65565606e-05f * in_hpfParam * (-8.56060458e-07f)));

		// Pink noise curve
		AkReal32 hpfLoudness = 9.82247933e-01f + in_hpfParam * (3.16785826e-03f + in_hpfParam * (-1.82098595e-04f * in_hpfParam * (7.24402930e-07f)));

		hpfLoudness = AkClamp(hpfLoudness, 0.0f, 1.0f);
		return hpfLoudness;
	}
}

AkListenerSet CAkListener::m_dirty;
AkReal32 CAkListener::m_matIdentity[3][3] = 
{
	{ 1, 0, 0 },
	{ 0, 1, 0 },
	{ 0, 0, 1 }
};

// AkListenerData.
// ---------------------------------------

CAkListener::CAkListener()
	: pUserDefinedSpeakerGainsMem( NULL )
	, pUserDefinedSpeakerGainsDB( NULL )
	, pUserDefinedSpeakerGains( NULL )	
{
	AkTransform listenerTransform;
	static const AkVector pos = { AK_DEFAULT_LISTENER_POSITION_X, AK_DEFAULT_LISTENER_POSITION_Y, AK_DEFAULT_LISTENER_POSITION_Z };
	static const AkVector orient = { AK_DEFAULT_LISTENER_FRONT_X, AK_DEFAULT_LISTENER_FRONT_Y, AK_DEFAULT_LISTENER_FRONT_Z };
	static const AkVector orientTop = { AK_DEFAULT_TOP_X, AK_DEFAULT_TOP_Y, AK_DEFAULT_TOP_Z };
	listenerTransform.Set(pos, orient, orientTop);
	SetTransform(listenerTransform);
}

AKRESULT CAkListener::Init(AkGameObjectID in_GameObjectID)
{	
	// Initialize the listener position from the emitter position, if we have one.
	CAkEmitter* pEmitter = GetOwner()->GetComponent<CAkEmitter>();
	if (pEmitter != NULL && pEmitter->GetPosition().GetNumPosition() > 0)
	{
		SetPosition(pEmitter->GetPosition().GetPositions()[0].position);
	}

	return AK_Success;
}

CAkListener::~CAkListener()
{
	if ( pUserDefinedSpeakerGainsMem )
	{
		AK::SpeakerVolumes::HeapFree(pUserDefinedSpeakerGainsMem, AkMemID_GameObject);
		pUserDefinedSpeakerGainsMem = NULL;
	}
}


void CAkListener::SetTransform(const AkTransform & in_Position)
{
	AKASSERT(AkMath::IsVectorNormalized(in_Position.OrientationTop()));
	AKASSERT(AkMath::IsVectorNormalized(in_Position.OrientationFront()));

#ifndef AK_OPTIMIZED
	bool bChanged = (AkMonitor::GetNotifFilter() & AKMONITORDATATYPE_TOMASK(AkMonitorData::MonitorDataGameObjPosition)) && !AkMath::CompareAkTransform(data.position, in_Position);
#endif
	data.position = in_Position;

	AkVector OrientationSide = AkMath::CrossProduct(in_Position.OrientationTop(), in_Position.OrientationFront());

	AkReal32* pFloat = &(Matrix[0][0]);

	// Build up rotation matrix out of our 3 basic row vectors, stored in row major order.
	*pFloat++ = OrientationSide.X;
	*pFloat++ = OrientationSide.Y;
	*pFloat++ = OrientationSide.Z;

	*pFloat++ = data.position.OrientationTop().X;
	*pFloat++ = data.position.OrientationTop().Y;
	*pFloat++ = data.position.OrientationTop().Z;

	*pFloat++ = data.position.OrientationFront().X;
	*pFloat++ = data.position.OrientationFront().Y;
	*pFloat++ = data.position.OrientationFront().Z;

#ifndef AK_OPTIMIZED
	if (bChanged && GetOwner())
		AkMonitor::AddChangedGameObject(GetOwner()->ID());
#endif
}

//====================================================================================================
//====================================================================================================
void CAkListener::Term()
{
	//Term() can only be called after all game object have been destroyed.
	AKASSERT(List().Length() == 0);

	m_dirty.Term();

	CAkConnectedListeners::TermDefault();
}

const CAkListener* CAkListener::GetListenerData(AkGameObjectID in_uListenerId)
{
	return g_pRegistryMgr->GetGameObjComponent<CAkListener>(in_uListenerId);
}

void CAkListener::OnBeginFrame()
{
	if (!m_dirty.IsEmpty())
	{
		// Notify game objects.
		g_pRegistryMgr->NotifyListenerPosChanged( m_dirty );
		m_dirty.RemoveAll();
	}
}

void CAkListener::ResetListenerData()
{
	// Force Recomputation of the cached matrix
	for (CAkListener::tList::Iterator it = CAkListener::List().Begin(); it != CAkListener::List().End(); ++it)
	{
		CAkListener* pData = ((CAkListener*)*it);
		pData->SetPosition(pData->data.position);
	}
}

AkReal32 * CAkListener::GetListenerMatrix(AkGameObjectID in_uListenerID)
{
	CAkListener* p = g_pRegistryMgr->GetGameObjComponent<CAkListener>(in_uListenerID);
	if (p != NULL)
		return &(p->Matrix[0][0]);
	return NULL;
}

AKRESULT CAkListener::SetPosition( const AkListenerPosition & in_Position )
{
	SetTransform(in_Position);
	m_dirty.Set(ID());
	return AK_Success;
}

AKRESULT CAkListener::SetScalingFactor( AkReal32 in_fScalingFactor	)
{
	AKASSERT( in_fScalingFactor > 0.0f );
	data.fScalingFactor = in_fScalingFactor;
	m_dirty.Set(ID());
	return AK_Success;
}

AKRESULT CAkListener::SetListenerSpatialization( AkGameObjectID in_uListener, bool in_bSpatialized, AkChannelConfig in_channelConfig, AK::SpeakerVolumes::VectorPtr in_pVolumeOffsets)
{
	CAkListener* pData = g_pRegistryMgr->GetGameObjComponent<CAkListener>(in_uListener);
	if (pData == NULL)
	{
		CAkGameObject* pGameObj = g_pRegistryMgr->GetObjAndAddref(in_uListener);
		
		if (pGameObj == nullptr)
			return AK_InvalidParameter;

		pData = pGameObj->CreateComponent<CAkListener>();
		pGameObj->Release();

		if (pData == nullptr)
			return AK_InsufficientMemory;
	}

	CAkListener& listener = *pData;

	listener.data.bSpatialized = in_bSpatialized;

	if ( listener.pUserDefinedSpeakerGainsMem )
	{
		AK::SpeakerVolumes::HeapFree(listener.pUserDefinedSpeakerGainsMem, AkMemID_GameObject);
		listener.pUserDefinedSpeakerGainsMem = NULL;
		listener.pUserDefinedSpeakerGainsDB = NULL;
		listener.pUserDefinedSpeakerGains = NULL;
	}

	if ( in_pVolumeOffsets )
	{
		if ( in_channelConfig.uNumChannels == 0 )
			return AK_UnsupportedChannelConfig;

		AkUInt32 uSizePerVector = AK::SpeakerVolumes::Vector::GetRequiredSize( in_channelConfig.uNumChannels );
		listener.pUserDefinedSpeakerGainsMem = AK::SpeakerVolumes::HeapAlloc(2 * uSizePerVector, AkMemID_GameObject);
		if ( !listener.pUserDefinedSpeakerGainsMem )
			return AK_Fail;

		listener.pUserDefinedSpeakerGainsDB = (AK::SpeakerVolumes::VectorPtr)listener.pUserDefinedSpeakerGainsMem;
		listener.pUserDefinedSpeakerGains = (AK::SpeakerVolumes::VectorPtr)( (AkUInt8*)listener.pUserDefinedSpeakerGainsMem + uSizePerVector );
		listener.userDefinedConfig = in_channelConfig;
		
		AK::SpeakerVolumes::Vector::Copy( listener.pUserDefinedSpeakerGainsDB, in_pVolumeOffsets, in_channelConfig.uNumChannels );
		AK::SpeakerVolumes::Vector::Copy( listener.pUserDefinedSpeakerGains, listener.pUserDefinedSpeakerGainsDB, in_channelConfig.uNumChannels );
		AK::SpeakerVolumes::Vector::dBToLin( listener.pUserDefinedSpeakerGains, in_channelConfig.uNumChannels );		
	}

	return AK_Success;
}

AKRESULT CAkListener::GetListenerSpatialization(AkGameObjectID in_uListener, bool& out_rbSpatialized, AK::SpeakerVolumes::VectorPtr& out_pVolumeOffsets, AkChannelConfig &out_channelConfig)
{
	CAkListener* pData = g_pRegistryMgr->GetGameObjComponent<CAkListener>(in_uListener);
	if (pData == NULL)
		return AK_InvalidParameter;

	CAkListener& listener = *pData;

	out_rbSpatialized  = listener.data.bSpatialized;
	out_pVolumeOffsets = listener.pUserDefinedSpeakerGainsDB;
	out_channelConfig = listener.userDefinedConfig;

	return AK_Success;
}

// Compute speaker distribution with 2D positioning -> Device volume.
void CAkListener::ComputeSpeakerMatrix2D(
	CAkBehavioralCtx*	AK_RESTRICT	in_pContext,
	AkChannelConfig		in_inputConfig,
	AkMixConnection * in_pConnection
	)
{
	const BaseGenParams& basePosParams = in_pContext->GetBasePosParams();

	AkMixConnection * AK_RESTRICT pConnection = in_pConnection;

	// Force panning if connection was silent in previous frame.
	if (in_pContext->GetPrevPosParams() != basePosParams
		|| pConnection->IsPrevSilent() ) 
	{
		AkDevice * pDevice = CAkOutputMgr::FindDevice(pConnection->GetOutputBusCtx());
		//	AKASSERT(SetContains(pDevice->listenersSet, pDevice->eListenerSetType, rVolumeData.ListenerID()));	// Otherwise need to fix garage collection of connections.
		AkPanningConversion Pan( basePosParams );
		CAkSpeakerPan::GetSpeakerVolumes2DPan( 
			Pan.fX, Pan.fY, Pan.fCenter, 
			(AkSpeakerPanningType)basePosParams.ePannerType, 
			in_inputConfig, 
			pConnection->GetOutputConfig(),
			pConnection->mxDirect.Next(),
			pDevice
			);		
	}
	else
	{
		// 2D positioning parameters have not changed: reuse previous ones.
		pConnection->CopyPrevToNextSpatialization();
	}
}

void CAkListener::ComputeSpeakerMatrix3D( 
	CAkBehavioralCtx* AK_RESTRICT in_pContext,
	const AkVolumeDataArray & in_arVolumeData,
	AkChannelConfig		in_inputConfig,
	AkMixConnection * in_pMixConnection
	)
{

	AKASSERT( !in_arVolumeData.IsEmpty() );
	AKASSERT( in_pMixConnection );

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

#ifdef AK_LFECENTER
	AkReal32 fDivergenceCenter = in_pContext->GetDivergenceCenter();
#else
	AkReal32 fDivergenceCenter = 0.0f;
#endif

	const AkChannelConfig channelConfigOut = in_pMixConnection->GetOutputConfig();

	// Get listener and device (for pan caches - REVIEW consider moving to avoid this search.)
	const CAkListener* pListenerData = GetListenerData(in_pMixConnection->GetListenerID());
	if (!pListenerData)
		return;	// REVIEW Is this possible?

	// Build the array of relevant rays. 
	AkUInt32 numRays = 0;	// init to 0, and count the relevant rays.
	AkRayVolumeData ** rays = (AkRayVolumeData**)AkAlloca(sizeof(AkRayVolumeData*) * in_arVolumeData.Length());
	AkReal32 * weights = (AkReal32*)AkAlloca(sizeof(AkReal32) * in_arVolumeData.Length());	// associated
	{
		// Use the connection's ray volume as the common value (should be the largest) across all rays. Compute its reciprocal with which we will weigh each matrix.
		// In the case where there is only one relevant ray, the matrix weighting should be one.
		// Careful with numerical unstability if (loudest) ray volume is under threshold. 
		AkReal32 fCommonRayVolumeReciprocal = (in_pMixConnection->rayVolume.fNext > g_fVolumeThreshold) ? 1.f / in_pMixConnection->rayVolume.fNext : 1.f;

		AkVolumeDataArray::Iterator it = in_arVolumeData.Begin();
		do //Loop on rays
		{
			if (in_pMixConnection->GetListenerID() == (*it).ListenerID())
			{
				rays[numRays] = &(*it);
				weights[numRays] = fCommonRayVolumeReciprocal * (*it).GetGainForConnectionType(in_pMixConnection->GetType());
				++numRays;
			}
			++it;
		} while (it != in_arVolumeData.End());
	}
	if (numRays == 0)
		return;	// nothing to do
	
	// Allocate as many volume matrices as there are relevant rays.
	AkUInt32 uMatrixAllocSize = AK::SpeakerVolumes::Matrix::GetRequiredSize( in_inputConfig.uNumChannels, channelConfigOut.uNumChannels );
	AkUInt32 uMatrixSize = uMatrixAllocSize / sizeof(AkReal32);
	AkUInt32 uAllocSize = numRays * uMatrixAllocSize;
	AK::SpeakerVolumes::MatrixPtr arVolumesMatrix = (AK::SpeakerVolumes::MatrixPtr)AkAlloca( uAllocSize );
	memset( arVolumesMatrix, 0, uAllocSize );

#define GET_VOLUME_MATRIX( _volume_matrices_, _ray_ ) ( _volume_matrices_ + _ray_ * uMatrixSize )
#define GET_INPUT_CHANNEL( _matrix_, _input_channel_index_ ) ( AK::SpeakerVolumes::Matrix::GetChannel( _matrix_, _input_channel_index_, channelConfigOut.uNumChannels ) );

	AkReal32 positioningTypeBlend = in_pContext->GetPositioningTypeBlend();
		
	// Full-band channels
	if (supportedConfigIn.uNumChannels) //Skip if this listener is tied only to motion.
	{
		if (pListenerData->data.bSpatialized)
		{
			CAkAttenuation *	pAttenuation = in_pContext->GetActiveAttenuation();

			// get a set of volumes relative to the location of the sound
			CAkAttenuation::AkAttenuationCurve* pSpreadCurve = NULL;
			CAkAttenuation::AkAttenuationCurve* pFocusCurve = NULL;

			if (pAttenuation)
			{
				pSpreadCurve = pAttenuation->GetCurve(AttenuationCurveID_Spread);
				pFocusCurve = pAttenuation->GetCurve(AttenuationCurveID_Focus);
			}

			if (in_pContext->GetSpatializationMode() == AK_SpatializationMode_None)
			{
				// Spatialization is not dependent on rays. Just get / compute it for first ray and copy over.
				AK::SpeakerVolumes::MatrixPtr volumes = GET_VOLUME_MATRIX(arVolumesMatrix, 0);

				// Compute panning only if params changed or if connection was silent in previous frame.
				const BaseGenParams& basePosParams = in_pContext->GetBasePosParams();
				if (in_pContext->GetPrevPosParams() != basePosParams
					|| in_pMixConnection->IsPrevSilent()
					|| numRays > 1)	// WG-40372 Force recomputation of panning volumes in the multiposition case without spatialization (uncommon), because multiposition interferes destructively with connection matrix.
				{
					AkDevice * pDevice = CAkOutputMgr::FindDevice(in_pMixConnection->GetOutputBusCtx());
					AkPanningConversion Pan(basePosParams);
					CAkSpeakerPan::GetSpeakerVolumes2DPan(
						Pan.fX, Pan.fY, Pan.fCenter,
						(AkSpeakerPanningType)basePosParams.ePannerType,
						in_inputConfig,
						channelConfigOut,
						volumes,
						pDevice);
				}
				else
				{
					// 2D positioning parameters have not changed: reuse previous ones.
					AK::SpeakerVolumes::Matrix::Copy(volumes, in_pMixConnection->mxDirect.Prev(), in_inputConfig.uNumChannels, channelConfigOut.uNumChannels);
				}

				AkUInt32 uRay = 0;
				while (++uRay < numRays)
				{
					AK::SpeakerVolumes::Matrix::Copy(GET_VOLUME_MATRIX(arVolumesMatrix, uRay), volumes, in_inputConfig.uNumChannels, channelConfigOut.uNumChannels);
				}
			}
			else
			{
				// 3D spatialization.
				AkDevice * pDevice = CAkOutputMgr::FindDevice(in_pMixConnection->GetOutputBusCtx());
				if (pDevice)
				{
					AkUInt32 uRay = 0;
					do
					{
						AK::SpeakerVolumes::MatrixPtr volumes = GET_VOLUME_MATRIX(arVolumesMatrix, uRay);
						const AkRayVolumeData * pCurrentRay = rays[uRay];
						AkReal32 fSpread = pCurrentRay->fSpread;
						AkReal32 fFocus = pCurrentRay->fFocus;

						if (pSpreadCurve || pFocusCurve)
						{
							/// WG-21599 Project distance on the plane first.
							/// REVIEW even for 3D configs?
							AkReal32 fDistanceOnPlane = pListenerData->ComputeDistanceOnPlane(*pCurrentRay);

							if (pSpreadCurve)
								fSpread = pSpreadCurve->Convert(fDistanceOnPlane);

							if (pFocusCurve)
								fFocus = pFocusCurve->Convert(fDistanceOnPlane);
						}

						AkTransform emitter = pCurrentRay->emitter;
						if (in_pContext->GetSpatializationMode() == AK_SpatializationMode_PositionOnly)
							emitter.SetOrientation(pListenerData->GetTransform().OrientationFront(), pListenerData->GetTransform().OrientationTop());
						CAkSpeakerPan::GetSpeakerVolumes(
							emitter,
							fDivergenceCenter,
							fSpread,
							fFocus,
							volumes,
							supportedConfigIn,
							pCurrentRay->uEmitterChannelMask,
							channelConfigOut,
							pListenerData->GetTransform().Position(),
							pListenerData->GetMatrix(),
							pDevice);

					} while (++uRay < numRays);

					if (positioningTypeBlend < 1.f)
					{
						// Blend. Pan on a temp matrix, which does not depend on rays.
						AK::SpeakerVolumes::MatrixPtr volumesTemp = (AK::SpeakerVolumes::MatrixPtr)AkAlloca(uMatrixAllocSize);

						// Make sure we compute panning all the time. Managing this optimization with different values of blend is error-prone and questionable.
						const BaseGenParams& basePosParams = in_pContext->GetBasePosParams();
						AkPanningConversion Pan(basePosParams);
						CAkSpeakerPan::GetSpeakerVolumes2DPan(
							Pan.fX, Pan.fY, Pan.fCenter,
							(AkSpeakerPanningType)basePosParams.ePannerType,
							in_inputConfig,
							channelConfigOut,
							volumesTemp,
							pDevice);

						// For all rays, scale spatialization matrix and blend scaled version of the panning matrix.
						AkReal32 panningGain = 1.f - positioningTypeBlend;
						AkUInt32 uRay = 0;
						do
						{
							AK::SpeakerVolumes::MatrixPtr volumes = GET_VOLUME_MATRIX(arVolumesMatrix, uRay);
							AK::SpeakerVolumes::Matrix::Mul(volumes, positioningTypeBlend, in_inputConfig.uNumChannels, channelConfigOut.uNumChannels);
							AK::SpeakerVolumes::Matrix::MAdd(volumes, volumesTemp, in_inputConfig.uNumChannels, channelConfigOut.uNumChannels, panningGain);
						} while (++uRay < numRays);
					}
				}
			}
		}
		else // if (pListenerData->data.bSpatialized)
		{
			// Custom listener spatialization (driven via SDK)
			AkUInt32 uRay = 0;
			do
			{
				AK::SpeakerVolumes::MatrixPtr volumes = GET_VOLUME_MATRIX(arVolumesMatrix, uRay);

				// WG-21804: Custom listener spatialization is not constant power across different input configs.
				AkReal32 fChannelCompensation = AkInvSqrtEstimate((AkReal32)supportedConfigIn.uNumChannels);
				if (channelConfigOut.eConfigType == AK_ChannelConfigType_Standard || channelConfigOut.eConfigType == AK_ChannelConfigType_Anonymous)
				{
					AkChannelConfig configOutNoLfe = channelConfigOut.RemoveLFE();
					// Needs to be constant power across different output configs too.
					fChannelCompensation /= configOutNoLfe.uNumChannels;
					for (AkUInt32 uChan = 0; uChan < configOutNoLfe.uNumChannels; uChan++)
						volumes[uChan] = fChannelCompensation;

					// Copy to all other input channels.						
					for (AkUInt32 iChannel = 1; iChannel < supportedConfigIn.uNumChannels; iChannel++)
					{
						AK::SpeakerVolumes::VectorPtr pPrevChanIn = AK::SpeakerVolumes::Matrix::GetChannel(volumes, iChannel - 1, channelConfigOut.uNumChannels);
						AK::SpeakerVolumes::VectorPtr pChanIn = AK::SpeakerVolumes::Matrix::GetChannel(volumes, iChannel, channelConfigOut.uNumChannels);
						AK::SpeakerVolumes::Vector::Copy(pChanIn, pPrevChanIn, channelConfigOut.uNumChannels); //results in mono sound
					}
				}
				else
				{
					// Ambisonics. Just set W.
					for (AkUInt32 iChannel = 0; iChannel < supportedConfigIn.uNumChannels; iChannel++)
					{
						AK::SpeakerVolumes::Matrix::GetChannel(volumes, iChannel, channelConfigOut.uNumChannels)[0] = fChannelCompensation;
					}
				}
			} while (++uRay < numRays);
		}	// listener spatialized
	}	// fullband channels

	// Handle LFE separately.
#ifdef AK_LFECENTER
	if (in_inputConfig.HasLFE() && channelConfigOut.HasLFE())
	{
		AkUInt32 uRay = 0;
		do
		{
			AK::SpeakerVolumes::MatrixPtr volumes = GET_VOLUME_MATRIX(arVolumesMatrix, uRay);
			AK::SpeakerVolumes::VectorPtr pLFE = GET_INPUT_CHANNEL(volumes, in_inputConfig.uNumChannels - 1);
			pLFE[channelConfigOut.uNumChannels - 1] = 1.f;
		} while (++uRay < numRays);
	}
#endif
	
	// Offset by custom speaker gain.
	/// TODO Other config types.
	if (pListenerData->pUserDefinedSpeakerGains
		&& in_pContext->GetSpatializationMode() != AK_SpatializationMode_None
		&& in_inputConfig.eConfigType == AK_ChannelConfigType_Standard
		&& channelConfigOut.eConfigType == AK_ChannelConfigType_Standard)
	{
		AkUInt32 uRay = 0;
		do
		{
			AK::SpeakerVolumes::MatrixPtr volumes = GET_VOLUME_MATRIX(arVolumesMatrix, uRay);

			AkUInt32 uInOutVectorSize = AK::SpeakerVolumes::Vector::GetRequiredSize(channelConfigOut.uNumChannels);
			AK::SpeakerVolumes::VectorPtr pSpeakerGains = (AK::SpeakerVolumes::VectorPtr)AkAlloca(uInOutVectorSize);
			AkChannelMask uMask = 1;
			AkUInt32 uChanOut = 0;
			AkUInt32 uChanUserOffset = 0;
			while (uChanOut < channelConfigOut.uNumChannels)
			{
				if (uMask & channelConfigOut.uChannelMask)
				{
					if (uMask & pListenerData->userDefinedConfig.uChannelMask)
					{
						pSpeakerGains[uChanOut] = pListenerData->pUserDefinedSpeakerGains[uChanUserOffset];
						++uChanUserOffset;
					}
					else
					{
						pSpeakerGains[uChanOut] = 1.f;
					}

					++uChanOut;
				}
				else if (uMask & pListenerData->userDefinedConfig.uChannelMask)
				{
					++uChanUserOffset;
				}
				uMask <<= 1;
			}

			// Apply to all input channels of matrix that has already been computed.
			for (AkUInt32 iChannel = 0; iChannel<in_inputConfig.uNumChannels; iChannel++)
			{
				AK::SpeakerVolumes::VectorPtr pChannel = GET_INPUT_CHANNEL(volumes, iChannel);
				AK::SpeakerVolumes::Vector::Mul(pChannel, pSpeakerGains, channelConfigOut.uNumChannels);
			}
		} while (++uRay < numRays);
	}

	// Collapse rays into an array of devices. Corresponding volume will be needed to mix the voice
	// into each device bus tree.
	{
		AkUInt32 uNumChannelsOut = channelConfigOut.uNumChannels;

		if (numRays == 1)
		{
			// Only one ray. Matrix weight should be one, unless ray volume was under threshold, in which case it really should be 1 but it isn't.
			AKASSERT((weights[0] > 0.99f && weights[0] < 1.01f) || (in_pMixConnection->rayVolume.fNext <= g_fVolumeThreshold));
			AK::SpeakerVolumes::Matrix::Copy(in_pMixConnection->mxDirect.Next(), GET_VOLUME_MATRIX(arVolumesMatrix, 0), in_inputConfig.uNumChannels, uNumChannelsOut);
		}
		else
		{
			// Multiple rays.
			in_pMixConnection->ClearNextSpatialization();

			if( in_pContext->IsMultiPositionTypeMultiSources() )
			{
				AkUInt32 uVolume = 0;
				AK::SpeakerVolumes::MatrixPtr pTempDry = (AK::SpeakerVolumes::MatrixPtr)AkAlloca(AK::SpeakerVolumes::Matrix::GetRequiredSize(in_inputConfig.uNumChannels, uNumChannelsOut));

				do //Loop on rays
				{
					// Multiply all object positions with their respective attenuation (ray volume), and add them onto "temp".
					AK::SpeakerVolumes::MatrixPtr volumes = GET_VOLUME_MATRIX(arVolumesMatrix, uVolume);
					AK::SpeakerVolumes::Matrix::Copy(pTempDry, volumes, in_inputConfig.uNumChannels, uNumChannelsOut, weights[uVolume] );
					AK::SpeakerVolumes::Matrix::Add(in_pMixConnection->mxDirect.Next(), pTempDry, in_inputConfig.uNumChannels, uNumChannelsOut);
				}
				while (++uVolume < numRays);
			}
			else
			{
				if (in_pContext->UseSpatialAudioPanningMode())
				{
					// In spatial audio panning mode, we scale the rays according to the relative difference in loudness between the connection's filter (actual filter) and the
					// ray's filter value (desired filter value).

					AkReal32 fConnectionLPFLoudness = GetLPFLoudness(in_pMixConnection->fLPF);
					AkReal32 fConnectionHPFLoudness = GetHPFLoudness(in_pMixConnection->fHPF);

					for (AkUInt32 i = 0; i< numRays; ++i)
					{
						AkReal32 fLpfLoudnessRatio = GetLPFLoudness(rays[i]->fLPF) / fConnectionLPFLoudness;
						AkReal32 fHpfLoudnessRatio = GetHPFLoudness(rays[i]->fHPF) / fConnectionHPFLoudness;
						weights[i] *= fLpfLoudnessRatio * fHpfLoudnessRatio;
					}
				}

				AkUInt32 uVolume = 0;
				if (channelConfigOut.eConfigType == AK_ChannelConfigType_Ambisonic)
				{
					// For ambisonics, we need to collapse matrices in the angular domain. 
					CAkSpeakerPan::MultiDirectionAmbisonicPan(in_inputConfig, channelConfigOut, arVolumesMatrix, uMatrixSize, weights, numRays, in_pMixConnection->mxDirect.Next());
				}
				else
				{
					AK::SpeakerVolumes::MatrixPtr pTempDry = (AK::SpeakerVolumes::MatrixPtr)AkAlloca(AK::SpeakerVolumes::Matrix::GetRequiredSize(in_inputConfig.uNumChannels, uNumChannelsOut));
					do //Loop on rays
					{
						// Multiply all object positions with their respective attenuation (ray volume), and take the max of each matrix element.
						AK::SpeakerVolumes::MatrixPtr volumes = GET_VOLUME_MATRIX(arVolumesMatrix, uVolume);
						AK::SpeakerVolumes::Matrix::Copy(pTempDry, volumes, in_inputConfig.uNumChannels, uNumChannelsOut, weights[uVolume] );
						AK::SpeakerVolumes::Matrix::AbsMax(in_pMixConnection->mxDirect.Next(), pTempDry, in_inputConfig.uNumChannels, uNumChannelsOut);

					} while (++uVolume < numRays);
				}
			}
		}
	}
}

// Returns volume attenuation due to cone,
// and out_fCone is the interpolated cone value [0,1] for LPF evaluation, later.
static inline AkReal32 ComputeConeAttenuation(
	const AkReal32	in_fInsideAngle,
	const AkReal32	in_fOutsideAngle,
	const AkReal32	in_fAngle,
	const AkReal32	in_fOutsideVolume,
	AkReal32 & out_fInterp)
{
	// are we below the min ?
	if (in_fAngle <= in_fInsideAngle)
	{
		out_fInterp = 0.f;
		return 1.0f;	//Not attenuated.  Avoid the DBtoLin!
	}
	// are we above the max ?
	else if (in_fAngle >= in_fOutsideAngle)
	{
		out_fInterp = 1.f;
	}
	// we're in between
	else
	{
		out_fInterp = (in_fAngle - in_fInsideAngle) / (in_fOutsideAngle - in_fInsideAngle);
	}
	return AkMath::dBToLin(out_fInterp * in_fOutsideVolume);
}

#ifndef AK_OPTIMIZED
struct LogAttenuationDelta
{
	static inline void InitVolume(const CAkBehavioralCtx *in_pContext, AkUInt8 in_uRays)
	{
		in_pContext->OpenSoundBrace();
		AkDeltaMonitor::LogVolumeRays(in_uRays);
	}

	inline void StartRay(AkUInt8 uNumRays, AkRayVolumeData& in_Ray, int in_rayIndex, AkDeltaType in_eType)
	{
		if (in_eType == AkDelta_Rays)
		{
			pHistory = &in_Ray.bitsVolume;
			pFlags = AkDeltaMonitor::StartVolumeRay(uNumRays, in_rayIndex, in_Ray.ListenerID());
		}
		else // AkDelta_RaysFilter
		{
			pHistory = &in_Ray.bitsFilter;
			pFlags = AkDeltaMonitor::StartFilteringRay(uNumRays, in_rayIndex);
		}

		// In case of memory failure
		if (pFlags == NULL)
			pFlags = &backupFlags; // save on the stack to prevent crash
	}

	inline void LogAttenuation(AkReal32 in_fVolume, AkUInt8 in_eRayBits, AkReal32 in_fNeutral)
	{
		AkDeltaMonitor::LogAttenuation(in_fVolume, in_eRayBits, pFlags, *pHistory, in_fNeutral);
	}

	inline void FinalizeVolumeRay(AkRayVolumeData &in_ray)
	{	
		if (*pFlags & ((1 << AkMonitorData::DeltaRayDry) | (1 << AkMonitorData::DeltaRayGameAux) | (1 << AkMonitorData::DeltaRayUserAux)))
			AkDeltaMonitor::PutFloat24(in_ray.Distance());

		if (*pFlags & (1 << AkMonitorData::DeltaRayCone))
			AkDeltaMonitor::PutFloat24(in_ray.EmitterAngle());

		if (*pFlags & (1 << AkMonitorData::DeltaRayOcclusion))
			AkDeltaMonitor::PutFloat24(in_ray.Occlusion());

		if (*pFlags & (1 << AkMonitorData::DeltaRayObstruction))
			AkDeltaMonitor::PutFloat24(in_ray.Obstruction());

		AkDeltaMonitor::FinalizeRay(pFlags);
	}

	inline void FinalizeFilterRay(AkRayVolumeData &in_ray)
	{
		if (*pFlags & ((1 << AkMonitorData::DeltaRayLPF) | (1 << AkMonitorData::DeltaRayHPF)))
			AkDeltaMonitor::PutFloat24(in_ray.Distance());

		if (*pFlags & ((1 << AkMonitorData::DeltaRayConeLPF) | (1 << AkMonitorData::DeltaRayConeHPF)))
			AkDeltaMonitor::PutFloat24(in_ray.EmitterAngle());

		if (*pFlags & ((1 << AkMonitorData::DeltaRayOcclusionLPF) | (1 << AkMonitorData::DeltaRayOcclusionHPF)))
			AkDeltaMonitor::PutFloat24(in_ray.Occlusion());

		if (*pFlags & ((1 << AkMonitorData::DeltaRayObstructionLPF) | (1 << AkMonitorData::DeltaRayObstructionHPF)))
			AkDeltaMonitor::PutFloat24(in_ray.Obstruction());
		
		*pNbRays += 1;
		
		AkDeltaMonitor::FinalizeRay(pFlags);
	}

	inline void StartConnectionFilterRays(AkPipelineID in_outputBusPipelineID, AkUInt8 in_uRays)
	{
		pNbRays = AkDeltaMonitor::StartConnectionFilteringRays(in_outputBusPipelineID, in_uRays);
		
		// In case of memory failure
		if (pNbRays == NULL)
			pNbRays = &backupNbRays; // save on the stack to prevent crash
	}

	static inline void Finalize(CAkBehavioralCtx *in_pContext)
	{
		in_pContext->CloseSoundBrace();
	}

	inline void SetUseDryBit(AkUInt8 in_iBit)
	{
		AKASSERT(AkMonitorData::DeltaRayDry == 0);
		*pFlags |= (*pFlags & 1) << in_iBit;
	}
		
	// Used for volume + filtering
	AkUInt8 *AK_RESTRICT pFlags;
	AkUInt8 *AK_RESTRICT pHistory;

	// Used only for filtering
	AkUInt8 *AK_RESTRICT pNbRays;

	// Used in case of memory failure
	AkUInt8 backupFlags;
	AkUInt8 backupNbRays;
};
#endif

struct NoLogAttenuationDelta
{
	static inline void InitVolume(const CAkBehavioralCtx *in_pContext, AkUInt8 in_uRays) {}
	inline void StartRay(AkUInt8 uNumRays, AkRayVolumeData& in_Ray, int in_rayIndex, AkDeltaType in_eType) {}
	inline void LogAttenuation(AkReal32 in_fVolume, AkUInt8 in_eRayBits, AkReal32 in_fNeutral){}
	inline void FinalizeVolumeRay(AkRayVolumeData &in_ray) {}
	inline void FinalizeFilterRay(AkRayVolumeData &in_ray) {}
	inline void StartConnectionFilterRays(AkPipelineID in_outputBusPipelineID, AkUInt8 in_uRays) {}
	static inline void Finalize(const CAkBehavioralCtx *in_pContext) {}
	inline void SetUseDryBit(AkUInt8 in_iBit) {}
};


template <class Logging>
void ComputeFiltering3D_Template(
	CAkBehavioralCtx* AK_RESTRICT in_pContext,
	const AkVolumeDataArray & in_arVolumeData,
	AkMixConnection * in_pConnection)
{
	CAkAttenuation * pAttenuation = in_pContext->GetActiveAttenuation();
	CAkAttenuation::AkAttenuationCurve* pLPFCurve = NULL;
	CAkAttenuation::AkAttenuationCurve* pHPFCurve = NULL;
	AkReal32 fMaxConeLPF = 0.f;
	AkReal32 fMaxConeHPF = 0.f;
	if (pAttenuation)
	{
		pLPFCurve = pAttenuation->GetCurve(AttenuationCurveID_LowPassFilter);
		pHPFCurve = pAttenuation->GetCurve(AttenuationCurveID_HighPassFilter);
		if (pAttenuation->m_bIsConeEnabled)
		{
			fMaxConeLPF = in_pContext->GetConeLPF();
			fMaxConeHPF = in_pContext->GetConeHPF();
		}
	}

	// Compute the effective filter value per ray, and then take the minimum (less filtered) among rays.
	// The effective value per ray is the max value of all contributors: distance, cone, occ and obs (if direct). This approximates cascading of all filters in the ray.

	const AkGameObjectID idConnectionListener = in_pConnection->GetListenerID();
	const AkUInt32 uNumRays = in_arVolumeData.Length();

	Logging logState;
#ifndef AK_OPTIMIZED
	// AkDeltaMonitor::LogFilterRays already done in CAkVPL3dMixable::UpdateConnections
	AkPipelineID outputBusId = AK_INVALID_PIPELINE_ID;
	{
		CAkBehavioralCtx* pOutputBusCtx = in_pConnection->GetOutputBus()->GetContext();
		if (pOutputBusCtx)
			outputBusId = pOutputBusCtx->GetPipelineID();
	}
	logState.StartConnectionFilterRays(outputBusId, (AkUInt8)uNumRays);
#endif

	AkReal32 fPositionDependentLPF = AK_MAX_LOPASS_VALUE;
	AkReal32 fPositionDependentHPF = AK_MAX_LOPASS_VALUE;
	AkReal32 fDenom = 0.f;

	bool bUseWeightedAverage = in_pContext->UseSpatialAudioPanningMode();
	if (bUseWeightedAverage)
	{
		fPositionDependentLPF = 0.f;
		fPositionDependentHPF = 0.f;
	}

	AkVolumeDataArray::Iterator it = in_arVolumeData.Begin();
	do
	{
		// Consider only rays that apply to this connection. 
		AkGameObjectID uListener = (*it).ListenerID();
		if (idConnectionListener != uListener)
		{
			++it;
			continue;
		}

		logState.StartRay((AkUInt8)uNumRays, (*it), (int)(it.pItem - in_arVolumeData.Begin().pItem), AkDelta_RaysFilter);

		// Combine (ie. max) all filters for the same ray.
		/// REVIEW Current behavior ignores obs/occ with user-defined positioned sounds.
		
		AkReal32 fRayLPF = 0.f;
		AkReal32 fRayHPF = 0.f;	

		// Note: Could take checks out of the loop but common case is one ray.

		// Distance.
		{
			AkReal32 fDistance = (*it).Distance();

			if (pLPFCurve)
			{
				fRayLPF = pLPFCurve->Convert(fDistance);				
				logState.LogAttenuation(fRayLPF, AkMonitorData::DeltaRayLPF, 0.f);
			}

			if (pHPFCurve)
			{
				fRayHPF = pHPFCurve->Convert(fDistance);
				logState.LogAttenuation(fRayHPF, AkMonitorData::DeltaRayHPF, 0.f);
			}
		}

		AkReal32 fConeLPF = (*it).fConeInterp * fMaxConeLPF;
		AkReal32 fConeHPF = (*it).fConeInterp * fMaxConeHPF;			
		fRayLPF = AkMath::Max(fRayLPF, fConeLPF);
		fRayHPF = AkMath::Max(fRayHPF, fConeHPF);

		if (pAttenuation && pAttenuation->m_bIsConeEnabled)
		{
			logState.LogAttenuation(fConeLPF, AkMonitorData::DeltaRayConeLPF, 0.f);
			logState.LogAttenuation(fConeHPF, AkMonitorData::DeltaRayConeHPF, 0.f);
		}

		// Occlusion
		{
			AkReal32 fOccCtrlValue = (*it).Occlusion();

			// Evaluate occlusion curves if applicable.
			if (g_pEnvironmentMgr->IsCurveEnabled(CAkEnvironmentsMgr::CurveOcc, CAkEnvironmentsMgr::CurveLPF))
			{
				AkReal32 fOccLPF = 0.0f;
				if (fOccCtrlValue != 0.0f)
				{
					fOccLPF = g_pEnvironmentMgr->GetCurveValue(CAkEnvironmentsMgr::CurveOcc, CAkEnvironmentsMgr::CurveLPF, fOccCtrlValue);
					fRayLPF = AkMath::Max(fRayLPF, fOccLPF);
				}
				logState.LogAttenuation(fOccLPF, AkMonitorData::DeltaRayOcclusionLPF, 0.f);
			}

			if (g_pEnvironmentMgr->IsCurveEnabled(CAkEnvironmentsMgr::CurveOcc, CAkEnvironmentsMgr::CurveHPF))
			{
				AkReal32 fOccHPF = 0.0f;
				if (fOccCtrlValue != 0.0f)
				{
					fOccHPF = g_pEnvironmentMgr->GetCurveValue(CAkEnvironmentsMgr::CurveOcc, CAkEnvironmentsMgr::CurveHPF, fOccCtrlValue);
					fRayHPF = AkMath::Max(fRayHPF, fOccHPF);
				}
				logState.LogAttenuation(fOccHPF, AkMonitorData::DeltaRayOcclusionHPF, 0.f);
			}
		}

		// Obstruction
		if (in_pConnection->IsTypeDirect())
		{
			AkReal32 fObsCtrlValue = (*it).Obstruction();

			if (g_pEnvironmentMgr->IsCurveEnabled(CAkEnvironmentsMgr::CurveObs, CAkEnvironmentsMgr::CurveLPF))
			{
				// We now have our collapsed obstruction LPF value.
				AkReal32 fObsLPF = 0.0f;
				if (fObsCtrlValue != 0.0f)
				{
					fObsLPF = g_pEnvironmentMgr->GetCurveValue(CAkEnvironmentsMgr::CurveObs, CAkEnvironmentsMgr::CurveLPF, fObsCtrlValue);
					fRayLPF = AkMath::Max(fRayLPF, fObsLPF);
				}
				logState.LogAttenuation(fObsLPF, AkMonitorData::DeltaRayObstructionLPF, 0.f);
			}

			if (g_pEnvironmentMgr->IsCurveEnabled(CAkEnvironmentsMgr::CurveObs, CAkEnvironmentsMgr::CurveHPF))
			{
				// We now have our collapsed obstruction LPF value.
				AkReal32 fObsHPF = 0.0f;
				if (fObsCtrlValue != 0.0f)
				{
					fObsHPF = g_pEnvironmentMgr->GetCurveValue(CAkEnvironmentsMgr::CurveObs, CAkEnvironmentsMgr::CurveHPF, fObsCtrlValue);
					fRayHPF = AkMath::Max(fRayHPF, fObsHPF);
				}
				logState.LogAttenuation(fObsHPF, AkMonitorData::DeltaRayObstructionHPF, 0.f);
			}
		}

		logState.FinalizeFilterRay(*it);

		// Store the per-ray filtering values.
		(*it).fLPF = fRayLPF;
		(*it).fHPF = fRayHPF;

		if (bUseWeightedAverage)
		{
			// Take a weighted average of all the filter values.

			AkReal32 volWeight = (*it).GetGainForConnectionType(in_pConnection->GetType());
			volWeight *= GetLPFLoudness(fRayLPF);
			volWeight *= GetHPFLoudness(fRayHPF);
			volWeight *= volWeight; // square weight to put more emphasis on loudest.
			fDenom += volWeight;

			fPositionDependentLPF += fRayLPF * volWeight;
			fPositionDependentHPF += fRayHPF * volWeight;
		}
		else
		{
			// Take the smallest filtering values among all rays (approximate parallel filters)
			fPositionDependentLPF = AkMath::Min(fPositionDependentLPF, fRayLPF);
			fPositionDependentHPF = AkMath::Min(fPositionDependentHPF, fRayHPF);
		}

		++it;

	} while (it != in_arVolumeData.End());

	if (bUseWeightedAverage && fDenom > 0)
	{
		fPositionDependentHPF /= fDenom;
		fPositionDependentLPF /= fDenom;
	}

	// "Cascade" with filter value already computed from hierarchy (common to all rays).
	in_pConnection->fLPF = AkMath::Max(in_pConnection->fLPF, fPositionDependentLPF);
	in_pConnection->fHPF = AkMath::Max(in_pConnection->fHPF, fPositionDependentHPF);
}

// Compute effective LPF/HPF with distance and azimuth (cone) for a given connection.
void CAkListener::ComputeFiltering3D(
	CAkBehavioralCtx* AK_RESTRICT in_pContext,
	const AkVolumeDataArray & in_arVolumeData,
	AkMixConnection * in_pConnection
)
{
#ifndef AK_OPTIMIZED
	if (AkDeltaMonitor::Enabled())
	{
		ComputeFiltering3D_Template<LogAttenuationDelta>(in_pContext, in_arVolumeData, in_pConnection);
	}
	else
#endif
	{
		ComputeFiltering3D_Template<NoLogAttenuationDelta>(in_pContext, in_arVolumeData, in_pConnection);
	}
}

//One version of ComputeRayVolumes with logging and one without.  
//There sufficient overhead to justify 2 versions.
template<class Logging>
void ComputeVolumeRays_Template(CAkBehavioralCtx * AK_RESTRICT in_pContext, AkVolumeDataArray &io_Rays)
{
	AkUInt32 uNumRays = io_Rays.Length();

	// Get volumes due to distance attenuation.		
	CAkAttenuation * pAttenuation = in_pContext->GetActiveAttenuation();
	CAkAttenuation::AkAttenuationCurve* pAttenuationCurveDry = NULL;
	CAkAttenuation::AkAttenuationCurve* pAttenuationCurveAuxGameDef = NULL;
	CAkAttenuation::AkAttenuationCurve* pAttenuationCurveAuxUserDef = NULL;

	if (pAttenuation)
	{
		pAttenuationCurveDry = pAttenuation->GetCurve(AttenuationCurveID_VolumeDry);
		pAttenuationCurveAuxGameDef = pAttenuation->GetCurve(AttenuationCurveID_VolumeAuxGameDef);
		pAttenuationCurveAuxUserDef = pAttenuation->GetCurve(AttenuationCurveID_VolumeAuxUserDef);
	}

	AkVolumeDataArray::Iterator it = io_Rays.Begin();
	const bool bOccEnabled = g_pEnvironmentMgr->IsCurveEnabled(CAkEnvironmentsMgr::CurveOcc, CAkEnvironmentsMgr::CurveVol);
	const bool bObsEnabled = g_pEnvironmentMgr->IsCurveEnabled(CAkEnvironmentsMgr::CurveObs, CAkEnvironmentsMgr::CurveVol);
	const bool bGameUseDry = pAttenuationCurveAuxGameDef == pAttenuationCurveDry;
	const bool bUserUseDry = pAttenuationCurveAuxUserDef == pAttenuationCurveDry;
	const bool bCone = pAttenuation && pAttenuation->m_bIsConeEnabled;

	Logging::InitVolume(in_pContext, (AkUInt8)uNumRays);
	do
	{
		Logging logState;
		logState.StartRay((AkUInt8)uNumRays, (*it), (int)(it.pItem - io_Rays.Begin().pItem), AkDelta_Rays);
		
		AkReal32 fListenerVolumeDry = 1.0f;
		if (pAttenuation)
		{			
			if (pAttenuationCurveDry)
			{
				fListenerVolumeDry = pAttenuationCurveDry->Convert((*it).Distance());
				(*it).fDryMixGain = fListenerVolumeDry;
				logState.LogAttenuation(fListenerVolumeDry, AkMonitorData::DeltaRayDry, 1.f);
			}
			else
				AKASSERT((*it).fDryMixGain == 1.0f);
			
			if (pAttenuationCurveAuxGameDef)
			{
				if (bGameUseDry) //optimize when Wet is UseCurveDry
				{
					(*it).fGameDefAuxMixGain = fListenerVolumeDry;					
					logState.SetUseDryBit(AkMonitorData::DeltaRayGameAuxUseDry);	//Only valid if we transmitted the DryVolume
				}
				else
				{
					(*it).fGameDefAuxMixGain = pAttenuationCurveAuxGameDef->Convert((*it).Distance());
					logState.LogAttenuation((*it).fGameDefAuxMixGain, AkMonitorData::DeltaRayGameAux, 1.f);
				}
			}
			else
				AKASSERT((*it).fGameDefAuxMixGain == 1.0f);

			if (pAttenuationCurveAuxUserDef)
			{
				if (bUserUseDry) //optimize when Wet is UseCurveDry
				{
					(*it).fUserDefAuxMixGain = fListenerVolumeDry;					
					logState.SetUseDryBit(AkMonitorData::DeltaRayUserAuxUseDry);
				}
				else
				{
					(*it).fUserDefAuxMixGain = pAttenuationCurveAuxUserDef->Convert((*it).Distance());
					logState.LogAttenuation((*it).fUserDefAuxMixGain, AkMonitorData::DeltaRayUserAux, 1.f);
				}
			}
			else
				AKASSERT((*it).fUserDefAuxMixGain == 1.0f);

			if (bCone)
			{
				AkReal32 fGain = ComputeConeAttenuation(pAttenuation->m_ConeParams.fInsideAngle, pAttenuation->m_ConeParams.fOutsideAngle, (*it).EmitterAngle(), in_pContext->GetOutsideConeVolume(), (*it).fConeInterp);
				(*it).fDryMixGain *= fGain;
				logState.LogAttenuation(fGain, AkMonitorData::DeltaRayCone, 1.f);
			}
		}

		if (bOccEnabled)
		{
			AkReal32 fOccVolume = 1.0f;
			if ((*it).fOcclusion != 0.0f)
			{
				fOccVolume = g_pEnvironmentMgr->GetCurveValue(CAkEnvironmentsMgr::CurveOcc, CAkEnvironmentsMgr::CurveVol, (*it).fOcclusion);
				(*it).fDryMixGain *= fOccVolume;
				(*it).fGameDefAuxMixGain *= fOccVolume;
				(*it).fUserDefAuxMixGain *= fOccVolume;
			}
			logState.LogAttenuation(fOccVolume, AkMonitorData::DeltaRayOcclusion, 1.f);
		}

		if (bObsEnabled)
		{
			AkReal32 fObsVolume = 1.0f;
			if ((*it).fObstruction != 0.0f)
			{
				fObsVolume = g_pEnvironmentMgr->GetCurveValue(CAkEnvironmentsMgr::CurveObs, CAkEnvironmentsMgr::CurveVol, (*it).fObstruction);
				(*it).fDryMixGain *= fObsVolume;
			}
			logState.LogAttenuation(fObsVolume, AkMonitorData::DeltaRayObstruction, 1.f);
		}

		logState.FinalizeVolumeRay(*it);

		++it;
	} while (it != io_Rays.End());

	Logging::Finalize(in_pContext);
}

void CAkListener::ComputeVolumeRays(CAkBehavioralCtx * AK_RESTRICT in_pContext, AkVolumeDataArray &io_Rays)
{
#ifndef AK_OPTIMIZED
	if (AkDeltaMonitor::Enabled())	//Splitting the code into profilable and non-profilable versions.  Much more efficient, albeit very verbose.
	{
		ComputeVolumeRays_Template<LogAttenuationDelta>(in_pContext, io_Rays);
	}
	else
#endif	
	{
		ComputeVolumeRays_Template<NoLogAttenuationDelta>(in_pContext, io_Rays);
	}
}

void SpeakerVolumeMatrixCallback::operator()(AkMixConnection & io_rMixConnection)
{
	AkSpeakerVolumeMatrixCallbackInfo info;
	info.pVolumes = io_rMixConnection.mxDirect.Next();
	info.pfBaseVolume = &io_rMixConnection.mixVolume.fNext;
	info.pfEmitterListenerVolume = &io_rMixConnection.rayVolume.fNext;
	info.inputConfig				= inputConfig;
	info.outputConfig = io_rMixConnection.GetOutputConfig();
	info.pContext = &io_rMixConnection;
	info.pMixerContext = io_rMixConnection.GetOutputBus();
	info.gameObjID = io_rMixConnection.Owner()->GetGameObjectID();

	if (playingID != AK_INVALID_PLAYING_ID)
		g_pPlayingMgr->NotifySpeakerVolumeMatrix(playingID, &info);
	else
		g_pBusCallbackMgr->DoVolumeCallback(io_rMixConnection.Owner()->GetAudioNodeID(), info);
}


// Accepts a listener and an absolute emitter position, and returns the emitter position relative to the listener.
static void EmitterPositionInListenerFrame(const CAkListener * in_pListener, const AkVector & in_vEmitter, AkVector & out_vRelEmitter)
{
	AkVector vPosFromListener;	// Position in the referential of the listener.
	const AkListenerPosition & listenerPosition = in_pListener->GetData().position;
	const AkVector & listenerPositionVector = listenerPosition.Position();
	vPosFromListener.X = in_vEmitter.X - listenerPositionVector.X;
	vPosFromListener.Y = in_vEmitter.Y - listenerPositionVector.Y;
	vPosFromListener.Z = in_vEmitter.Z - listenerPositionVector.Z;

	// Compute angles relative to listener: rotate the vector from listener to emitter
	// according to the inverse of listener orientation. The xz-plane (phi=0) corresponds to no elevation.
	const AkReal32 * pRotationMatrix = &in_pListener->GetMatrix()[0][0];
	AkMath::UnrotateVector(vPosFromListener, pRotationMatrix, out_vRelEmitter);
}

// Helper: Computes ray's spherical coordinates for given emitter position and listener data.
AkReal32 CAkListener::ComputeDistanceOnPlane(
	const AkRayVolumeData & in_pair
	) const
{
	AkReal32 fDistanceOnPlane = 0.f;
	if (in_pair.Distance() > 0.f)
	{
		AkVector vRelEmitter;
		EmitterPositionInListenerFrame(this, in_pair.emitter.Position(), vRelEmitter);
		fDistanceOnPlane = sqrtf(vRelEmitter.X*vRelEmitter.X + vRelEmitter.Z*vRelEmitter.Z) / in_pair.scalingFactor;
	}
	return fDistanceOnPlane;
}

// Helper: Computes ray's spherical coordinates for given emitter position and listener data.
void CAkListener::ComputeSphericalCoordinates(
	const AkEmitterListenerPair & in_pair,
	AkReal32 & out_fAzimuth,
	AkReal32 & out_fElevation
	) const
{
	out_fAzimuth = 0.f;
	out_fElevation = 0.f;

	if (in_pair.Distance() > 0.f)
	{
		AkVector vRelEmitter;
		EmitterPositionInListenerFrame(this, in_pair.emitter.Position(), vRelEmitter);
		CAkSpeakerPan::CartesianToSpherical(vRelEmitter.X, vRelEmitter.Z, vRelEmitter.Y, in_pair.Distance(), out_fAzimuth, out_fElevation);
	}
}

// Helper: Computes ray for given emitter position and listener data.
AkReal32  CAkListener::ComputeRayDistanceAndAngles( 
	AkReal32 in_fGameObjectScalingFactor,
	AkRayVolumeData & io_ray
	) const
{
	const AkTransform & in_emitter = io_ray.emitter;

	AkVector vPosFromListener;	// Position in the referential of the listener.
	const AkListenerPosition & listenerPosition = data.position;
	const AkVector & listenerPositionVector = listenerPosition.Position();
	const AkVector & positionVector = in_emitter.Position();
	vPosFromListener.X = positionVector.X - listenerPositionVector.X;
	vPosFromListener.Y = positionVector.Y - listenerPositionVector.Y;
	vPosFromListener.Z = positionVector.Z - listenerPositionVector.Z;

	AkReal32 fDistance = AkMath::Magnitude( vPosFromListener );
	// Distance is scaled by both the listener and game object scaling factor.
	AkReal32 combinedScalingFactor = (in_fGameObjectScalingFactor * data.fScalingFactor);
	io_ray.scalingFactor = combinedScalingFactor;
	AkReal32 fScaledDistance = fDistance / combinedScalingFactor;
	io_ray.SetDistance(fScaledDistance);
	

	// Compute position angle
	{
		AkReal32 fAngle = 0.f;	// front
		if ( fDistance > 0.f )
		{
			// Compute angle between direction of sound and the [listener,sound] vector for cone attenuation.
			// Note: Negate projection value since vPosFromListener is the vector that goes
			// away from the listener.
			fAngle = -AkMath::DotProduct( vPosFromListener, in_emitter.OrientationFront() ) / fDistance;

			// we can sometime get something slightly over 1.0
			fAngle = AkMath::Min(fAngle, 1.0f);
			fAngle = AkMath::Max(fAngle, -1.0f);

			fAngle = AkMath::FastACos( fAngle );
		}

		io_ray.SetEmitterAngle(fAngle);
	}
	// Compute listener angle
	{
		AkReal32 fAngle = 0.f;
		if ( fDistance > 0.0f )
		{
			// Compute angle between direction of listener and the [listener,sound] vector for cone attenuation.
			fAngle = AkMath::DotProduct( vPosFromListener, listenerPosition.OrientationFront() ) / fDistance;

			// we can sometime get something slightly over 1.0
			fAngle = AkMath::Min( fAngle, 1.0f );
			fAngle = AkMath::Max( fAngle, -1.0f );

			fAngle = AkMath::FastACos( fAngle );
		}
		io_ray.SetListenerAngle( fAngle );
	}

	return fScaledDistance;
}

