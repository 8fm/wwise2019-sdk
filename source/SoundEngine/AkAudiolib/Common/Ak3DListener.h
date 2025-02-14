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
// Ak3DListener.h
//
//////////////////////////////////////////////////////////////////////
#pragma once

#include <AK/SoundEngine/Common/AkSpeakerVolumes.h>
#include "AkConnectedListeners.h"
#include "AkGameObject.h"
#include "Ak3DParams.h"
#include "AkPositionKeeper.h"

class AkMixConnection;
class CAkAttenuation;
class CAkBehavioralCtx;

class CAkListener : public CAkTrackedGameObjComponent < GameObjComponentIdx_Listener >
{
public:
	typedef AkReal32 (&Matrix3x3)[3][3];
	typedef const AkReal32(&const_Matrix3x3)[3][3];

	CAkListener();
	~CAkListener();

	virtual AKRESULT Init(AkGameObjectID in_GameObjectID);

	static void Term();

	static const CAkListener* GetListenerData(AkGameObjectID in_uListenerId);

	static AKRESULT SetListenerSpatialization(
		AkGameObjectID in_uListener,
		bool in_bSpatialized,
		AkChannelConfig in_channelConfig,
		AK::SpeakerVolumes::VectorPtr in_pVolumeOffsets
		);

	static AKRESULT GetListenerSpatialization(
		AkGameObjectID in_uListener,
		bool& out_rbSpatialized,
		AK::SpeakerVolumes::VectorPtr& out_pVolumeOffsets,
		AkChannelConfig &out_channelConfig
		);

	// Compute speaker distribution with 2D positioning.
	static void ComputeSpeakerMatrix2D(
		CAkBehavioralCtx*	AK_RESTRICT	in_pContext,
		AkChannelConfig		in_inputConfig,
		class AkMixConnection * in_pConnection
		);
	
	// Compute speaker distribution with 3D positioning.
	static void ComputeSpeakerMatrix3D(
		CAkBehavioralCtx*	AK_RESTRICT	in_pContext,
	    const AkVolumeDataArray & in_arVolumeData,
		AkChannelConfig		in_inputConfig,
		class AkMixConnection * in_pMixConnection
		);

	static void ComputeVolumeRays(
		CAkBehavioralCtx * AK_RESTRICT in_pContext,
		AkVolumeDataArray &io_Rays);

	// Compute effective LPF/HPF with distance and azimuth (cone) for a given connection.
	static void ComputeFiltering3D(
		CAkBehavioralCtx*	AK_RESTRICT	in_pContext,
		const AkVolumeDataArray & in_arVolumeData,		
		class AkMixConnection * in_pConnection
		);

	static void OnBeginFrame();

	static void ResetListenerData();

	static AkReal32 * GetListenerMatrix(AkGameObjectID in_uListenerID);
	

	//
	//	
	//

	AKRESULT SetPosition(const AkListenerPosition & in_Position	);

	const_Matrix3x3 GetMatrix() const { return Matrix; }

	AKRESULT SetScalingFactor(AkReal32 in_fScalingFactor);
	AkReal32 GetScalingFactor() const { return data.fScalingFactor; }

	bool IsSpatialized() const { return data.bSpatialized; }
	void SetSpatialized(bool in_bSpatialized) { data.bSpatialized = in_bSpatialized; }
	const AK::SpeakerVolumes::VectorPtr GetUserDefinedSpeakerGains() const { return pUserDefinedSpeakerGains; }
	const AK::SpeakerVolumes::VectorPtr GetUserDefinedSpeakerGainsDB() const { return pUserDefinedSpeakerGainsDB; }
	AkChannelConfig GetUserDefinedConfig() const { return userDefinedConfig; }

	const AkTransform& GetTransform() const { return data.position; }
	const AkListener& GetData() const { return data; }

	AkReal32 ComputeDistanceOnPlane(
		const AkRayVolumeData & in_pair
		) const;

	// Helper: Computes ray's spherical coordinates for given emitter position and listener data.
	void ComputeSphericalCoordinates(
		const AkEmitterListenerPair & in_pair,
		AkReal32 & out_fAzimuth,
		AkReal32 & out_fElevation
		) const;	
	
	AkReal32 ComputeRayDistanceAndAngles(
		AkReal32 in_fGameObjectScalingFactor,
		AkRayVolumeData & out_ray
		) const;

private:


	static AkListenerSet m_dirty;

	static AkReal32 m_matIdentity[3][3];

	void SetTransform(const AkTransform & in_Position);

private:
	void *				pUserDefinedSpeakerGainsMem;	// user-specified per-speaker volume offset memory. pUserDefinedSpeakerGainsDB and pUserDefinedSpeakerGains point to it.
	AK::SpeakerVolumes::VectorPtr pUserDefinedSpeakerGainsDB;	// user-specified per-speaker volume offset (use AK::SpeakerVolumes::Vector to handle it), in decibels (as set/get by user).
	AK::SpeakerVolumes::VectorPtr pUserDefinedSpeakerGains;		// user-specified per-speaker volume offset (use AK::SpeakerVolumes::Vector to handle it), in linear values.
	AkChannelConfig		userDefinedConfig;	// channel configuration associated with pUserDefinedSpeakerGains. This _does not_ necessarily correspond to the destination (bus) config for any sound. Down/up mix needs to be performed at run-time.
	AkReal32			Matrix[3][3];		// world to listener coordinates transformation	
	//bool				bPositionDirty;		// True when position (or scaling factor) is changed, reset after game objects have been notified.

	AkListener data;
};

struct SpeakerVolumeMatrixCallback
{
	SpeakerVolumeMatrixCallback(AkPlayingID in_playingID, AkChannelConfig in_inputConfig)
		: playingID(in_playingID), inputConfig(in_inputConfig) {}
	
	AkPlayingID playingID;
	AkChannelConfig inputConfig;

	void operator()(AkMixConnection & io_rMixConnection);
};