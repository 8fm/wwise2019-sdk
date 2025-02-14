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

#include "stdafx.h"
#include "AkMixConnection.h"
#include "AkOutputMgr.h"
#include "AkVPL.h"

AkMixConnection::AkMixConnection( 
	CAkVPL3dMixable * in_pOwner, 
	AkVPL * in_pOutputBus, 
	AkHdrBus * in_pHdrBus,
	AkConnectionType in_eConnectionType,
	bool in_bStartSilent
	) 
	: m_pOwner( in_pOwner )
	, pMixBus( in_pOutputBus )
	, m_pHdrBus(in_pHdrBus)
	, m_pUserData( NULL )
	, m_attachedPropInfo()
	, fLPF( 0.f )
	, fHPF( 0.f )
	, m_fVolume( 1.f )
	, m_fCumulDownstreamVolumeDB(AK_SAFE_MINIMUM_VOLUME_LEVEL)
	, m_fWindowTopDB(AK_SAFE_MINIMUM_VOLUME_LEVEL)
	, m_eType(in_eConnectionType)
	, m_bNextSilent(in_bStartSilent)
	, m_bPrevSilent(in_bStartSilent)
	, m_bAttachedPropCreated( false )
	, m_bFeedback( false )
#ifndef AK_OPTIMIZED
	, m_bIsMonitoringMute( false )
	, m_bIsMonitoringSolo( false )
	, m_bMuteSoloExplicit( false )
#endif
{
	AKASSERT( in_pOutputBus );
	// A bit of logic is used to determine how to set the connection's listener ID:
	const AK::CAkBusCtx& outputBusCtx = in_pOutputBus->m_MixBus.GetBusContext();
	if (outputBusCtx.HasBus())
		m_ListenerID = outputBusCtx.GameObjectID(); // The normal case - use the output bus' game object ID.
	else
		m_ListenerID = in_pOwner->GetGameObjectID();// We are connecting to an anonymous 'device' bus.  Use the owner's object ID.
		
#ifndef AK_OPTIMIZED
	if (CAkBehavioralCtx* ownerCtx = in_pOwner->GetContext())
	{
		m_attachedPropInfo.UpdateContextID(ownerCtx->GetSoundID(), ownerCtx->GetPipelineID());
	}
#endif

	in_pOutputBus->m_MixBus.Connect( this );
}

AkMixConnection::~AkMixConnection()
{
	m_attachedPropInfo.Term();

	if ( pMixBus )
		pMixBus->m_MixBus.Disconnect( this );

	mxDirect.Free();

	m_BQF.Term();
}

// Create a device and its speaker volume matrix. 
AkMixConnection* CAkMixConnections::Create(
	CAkVPL3dMixable * in_pOwner,
	AkVPL *	in_pOutputBus,
	AkHdrBus * in_pHdrBus,
	AkConnectionType in_eConnectionType,
	bool in_bStartSilent
	)
{
	/// LX Review necessity of AkNewAligned if volumes are allocated elsewhere.
	AkMixConnection * pConnection = AkNewAligned(AkMemID_Processing, AkMixConnection(in_pOwner, in_pOutputBus, in_pHdrBus, in_eConnectionType, in_bStartSilent), AK_SIMD_ALIGNMENT);
	if (pConnection)
		AddFirst(pConnection);
	return pConnection;
}

AkChannelConfig AkMixConnection::GetOutputConfig() const 
{ 
	return pMixBus->m_MixBus.GetMixConfig(); 
}

const AK::CAkBusCtx & AkMixConnection::GetOutputBusCtx() const
{ 
	return GetOutputBus()->GetBusContext(); 
}

CAkVPLMixBusNode * AkMixConnection::GetOutputBus() const 
{ 
	return &pMixBus->m_MixBus; 
}

//
// AK::IAkVoicePluginInfo interface
//
AkPlayingID AkMixConnection::GetPlayingID() const 
{ 
	return m_pOwner->GetPlayingID(); 
}

/// Get priority value associated to this sound [AK_MIN_PRIORITY,AK_MAX_PRIORITY].
AkPriority AkMixConnection::GetPriority() const
{ 
	return m_pOwner->GetPriority(); 
}

/// Get priority value associated to this voice for a specified distance.
AkPriority AkMixConnection::ComputePriorityWithDistance(
	AkReal32 in_fDistance
	) const
{
	return m_pOwner->ComputePriorityWithDistance(in_fDistance);
}

//
// AK::IAkGameObjectPluginInfo interface
//
AkGameObjectID AkMixConnection::GetGameObjectID() const 
{ 
	return m_pOwner->GetGameObjectID(); 
}

AkUInt32 AkMixConnection::GetNumEmitterListenerPairs() const 
{
	// The number of emitter-listener pairs for a single listener is equal to the number of sound positions!
	return m_pOwner->GetContext()->GetEmitter()->GetPosition().GetNumPosition();
}

AKRESULT AkMixConnection::GetEmitterListenerPair(
	AkUInt32 in_uIndex,
	AkEmitterListenerPair & out_emitterListenerPair
	) const 
{
	return m_pOwner->GetContext()->GetEmitterListenerPair(in_uIndex, GetListenerID(), out_emitterListenerPair);
}

AkUInt32 AkMixConnection::GetNumGameObjectPositions() const 
{
	return m_pOwner->GetNumGameObjectPositions(); 
}

AK::SoundEngine::MultiPositionType AkMixConnection::GetGameObjectMultiPositionType() const 
{ 
	return m_pOwner->GetGameObjectMultiPositionType(); 
}

AkReal32 AkMixConnection::GetGameObjectScaling() const 
{ 
	return m_pOwner->GetGameObjectScaling(); 
}

AKRESULT AkMixConnection::GetGameObjectPosition(
	AkUInt32 in_uIndex,
	AkSoundPosition & out_position
	) const 
{ 
	return m_pOwner->GetGameObjectPosition( in_uIndex, out_position ); 
}

bool AkMixConnection::GetListeners(AkGameObjectID* out_aListenerIDs, AkUInt32& io_uSize) const
{
	if (!out_aListenerIDs)
	{
		io_uSize = 1;
		return true;
	}
	else if (io_uSize > 0)
	{
		*out_aListenerIDs = GetListenerID();
		io_uSize = 1;
		return true;
	}
	return false;
}

AKRESULT AkMixConnection::GetListenerData(
	AkGameObjectID in_uListener,
	AkListener & out_listener
	) const 
{
	/// REVIEW Prevent callers from accessing a listener if it isn't in this connection's path?
	return m_pOwner->GetContext()->GetListenerData(in_uListener, out_listener);
}

AK::IAkPluginParam * AkMixConnection::GetInputParam()
{
	if ( m_bAttachedPropCreated )
		return m_attachedPropInfo.pParam;

	m_bAttachedPropCreated = true;	// so that we don't try to recreate it if none.

	// Create it, if applicable.
	AkFXDesc fxDesc;
	m_pOwner->GetContext()->GetParameterNode()->GetAttachedPropFX(fxDesc);
	if ( fxDesc.pFx )
	{
		return m_attachedPropInfo.Clone(fxDesc.pFx, (CAkPBI*)m_pOwner->GetContext());
	}
	return NULL;
}

// Obtain the interface to access voice info.
AK::IAkVoicePluginInfo * AkMixConnection::GetVoiceInfo()
{ 
	return this; 
}

AK::IAkGameObjectPluginInfo * AkMixConnection::GetGameObjectInfo()
{
	return this;
}

AkConnectionType AkMixConnection::GetConnectionType()
{
	return GetType();
}

AkUniqueID AkMixConnection::GetAudioNodeID()
{ 
	return m_pOwner->GetAudioNodeID(); 
}

void * AkMixConnection::GetUserData()
{
	return m_pUserData;
}

void AkMixConnection::SetUserData( void * in_pUserData )
{
	m_pUserData = in_pUserData;
}

bool AkMixConnection::HasListenerRelativeRouting()
{
	return m_pOwner->GetContext()->HasListenerRelativeRouting();
}

AkSpeakerPanningType AkMixConnection::GetSpeakerPanningType()
{
	return m_pOwner->GetContext()->GetSpeakerPanningType();
}

Ak3DPositionType AkMixConnection::Get3DPositionType()
{
	return m_pOwner->GetContext()->Get3DPositionType();
}

AkReal32 AkMixConnection::GetCenterPerc()
{
	const BaseGenParams& pannerParams = m_pOwner->GetContext()->GetBasePosParams();
	return pannerParams.m_fCenterPCT * 0.01f;
}

void AkMixConnection::GetPannerPosition( AkVector & out_position )
{
	const BaseGenParams& pannerParams = m_pOwner->GetContext()->GetBasePosParams();
	// Note: Not using AkPanningConversion because expected range is [-1,1], which is more appropriate
	// for an SDK function.
	AkReal32 fX = pannerParams.m_fPAN_X_2D * 0.01f;
	out_position.X = AkClamp( fX, -1.f, 1.f );
	AkReal32 fY = pannerParams.m_fPAN_Y_2D * 0.01f;
	out_position.Y = AkClamp( fY, -1.f, 1.f );
	out_position.Z = 0.f;
}

AkUInt32 AkMixConnection::GetNum3DPositions()
{
	AkAutoTermListenerSet setListener;
	if (!setListener.Add(GetListenerID()))
		return 0;
	return m_pOwner->GetContext()->GetNumRays(setListener, SetType_Inclusion);
}

AKRESULT AkMixConnection::Get3DPosition( AkUInt32 in_uIndex, AkEmitterListenerPair & out_soundPosition )
{
	AkAutoTermListenerSet setListener;
	if (!setListener.Add(GetListenerID()))
		return AK_Fail;
	const AkRayVolumeData * pRay = m_pOwner->GetContext()->GetRay(setListener, SetType_Inclusion, in_uIndex);
	if (pRay)
	{
		out_soundPosition = *pRay;
		return AK_Success;
	}
	return AK_Fail;
}

AkReal32 AkMixConnection::GetSpread(
	AkUInt32 in_uIndex				///< Index of the pair, [0, GetNumEmitterListenerPairs()[
	) 
{
	// Mask desired rays with listeners that are relevant to the mix graph to which we are connected.
	AkAutoTermListenerSet setListener;
	if (!setListener.Add(GetListenerID()))
		return AK_Fail;
	const AkRayVolumeData * pRay = m_pOwner->GetContext()->GetRay(setListener, SetType_Inclusion, in_uIndex);
	if (pRay)
	{
		const CAkListener* pListenerData = CAkListener::GetListenerData(GetListenerID());
		if (pListenerData != NULL)
		{
			AkReal32 fDistanceOnPlane = pListenerData->ComputeDistanceOnPlane(*pRay);
			return m_pOwner->GetContext()->EvaluateSpread(fDistanceOnPlane);
		}
	}
	return 0.f;
}

AkReal32 AkMixConnection::GetFocus(
	AkUInt32 in_uIndex				///< Index of the pair, [0, GetNumEmitterListenerPairs()[
	) 
{
	// Mask desired rays with listeners that are relevant to the mix graph to which we are connected.
	AkAutoTermListenerSet setListener;
	if (!setListener.Add(GetListenerID()))
		return AK_Fail;
	const AkRayVolumeData * pRay = m_pOwner->GetContext()->GetRay(setListener, SetType_Inclusion, in_uIndex);
	if (pRay)
	{
		const CAkListener* pListenerData = CAkListener::GetListenerData(GetListenerID());
		AkReal32 fDistanceOnPlane = pListenerData->ComputeDistanceOnPlane(*pRay);
		return m_pOwner->GetContext()->EvaluateFocus(fDistanceOnPlane);
	}
	return 0.f;
}

bool AkMixConnection::GetMaxAttenuationDistance( AkReal32 & out_fMaxAttenuationDistance )
{
	return m_pOwner->GetContext()->GetParameterNode()->GetMaxRadius( out_fMaxAttenuationDistance );
}

// Get next volumes as computed by the sound engine for this input (for current device).
void AkMixConnection::GetSpatializedVolumes(
	AK::SpeakerVolumes::MatrixPtr out_mxPrevVolumes,	///< Returned in/out channel volume distribution corresponding to the beginning of the buffer. Must be preallocated (see AK::SpeakerVolumes::Matrix services).
	AK::SpeakerVolumes::MatrixPtr out_mxNextVolumes		///< Returned in/out channel volume distribution corresponding to the end of the buffer. Must be preallocated (see AK::SpeakerVolumes::Matrix services).
	)
{
	AkUInt32 uNumChannels = GetInputConfig().uNumChannels;
	AkUInt32 uNumChannelsOut = GetOutputConfig().uNumChannels;
	if ( mxDirect.IsAllocated() )
	{
		AK::SpeakerVolumes::Matrix::Copy( out_mxPrevVolumes, mxDirect.Prev(), uNumChannels, uNumChannelsOut );
		AK::SpeakerVolumes::Matrix::Copy( out_mxNextVolumes, mxDirect.Next(), uNumChannels, uNumChannelsOut );
	}
	else
	{
		AK::SpeakerVolumes::Matrix::Zero( out_mxPrevVolumes, uNumChannels, uNumChannelsOut );
		AK::SpeakerVolumes::Matrix::Zero( out_mxNextVolumes, uNumChannels, uNumChannelsOut );
	}
}

/// Query the 3D spatialization mode used by this input.
/// \return The 3D spatialization mode (see Ak3DSpatializationMode). AK_SpatializationMode_None if not set (2D).
Ak3DSpatializationMode AkMixConnection::Get3DSpatializationMode()
{
	return m_pOwner->GetContext()->GetSpatializationMode();
}

// AK::IAkMixerInputContextEx interface
AkReal32 AkMixConnection::GetSpreadFullDistance(AkUInt32 in_uIndex)
{
	// Mask desired rays with listeners that are relevant to the mix graph to which we are connected.
	AkAutoTermListenerSet setListener;
	if (!setListener.Add(GetListenerID()))
		return AK_Fail;
	const AkRayVolumeData * pRay = m_pOwner->GetContext()->GetRay(setListener, SetType_Inclusion, in_uIndex);
	if (pRay)
	{
		const CAkListener* pListenerData = CAkListener::GetListenerData(GetListenerID());
		if (pListenerData != NULL)
		{
			AkReal32 distance = AkMath::Distance(pListenerData->GetTransform().Position(), pRay->emitter.Position());
			return m_pOwner->GetContext()->EvaluateSpread(distance);
		}
	}
	return 0.f;
}

AkReal32 AkMixConnection::GetFocusFullDistance(AkUInt32 in_uIndex)
{
	// Mask desired rays with listeners that are relevant to the mix graph to which we are connected.
	AkAutoTermListenerSet setListener;
	if (!setListener.Add(GetListenerID()))
		return AK_Fail;
	const AkRayVolumeData * pRay = m_pOwner->GetContext()->GetRay(setListener, SetType_Inclusion, in_uIndex);
	if (pRay)
	{
		const CAkListener* pListenerData = CAkListener::GetListenerData(GetListenerID());
		if (pListenerData != NULL)
		{
			AkReal32 distance = AkMath::Distance(pListenerData->GetTransform().Position(), pRay->emitter.Position());
			return m_pOwner->GetContext()->EvaluateFocus(distance);
		}
	}
	return 0.f;
}
