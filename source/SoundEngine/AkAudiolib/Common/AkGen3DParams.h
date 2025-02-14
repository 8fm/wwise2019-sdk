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
// AkGen3DParams.h
//
// alessard
//
//////////////////////////////////////////////////////////////////////
#ifndef _GEN_3D_PARAMS_H_
#define _GEN_3D_PARAMS_H_

#include "AkRTPC.h"
#include "AkAudioLibIndex.h"
#include "AkSharedEnum.h"
#include "AkPath.h"

struct AkPathState;

// IDs of the AudioLib known RTPC capable parameters
enum AkPositioning_ParameterID
{
	POSID_Positioning_Divergence_Center_PCT			= RTPC_Positioning_Divergence_Center_PCT,
	POSID_Positioning_Cone_Attenuation_ON_OFF		= RTPC_Positioning_Cone_Attenuation_ON_OFF,
	POSID_Positioning_Cone_Attenuation				= RTPC_Positioning_Cone_Attenuation,
	POSID_Positioning_Cone_LPF						= RTPC_Positioning_Cone_LPF,
	POSID_Positioning_Cone_HPF						= RTPC_Positioning_Cone_HPF,
	POSID_Position_PAN_X_2D							= RTPC_Position_PAN_X_2D,
	POSID_Position_PAN_Y_2D							= RTPC_Position_PAN_Y_2D,
	POSID_PositioningTypeBlend						= RTPC_PositioningTypeBlend,
	POSID_Position_PAN_X_3D							= RTPC_Position_PAN_X_3D,
	POSID_Position_PAN_Y_3D							= RTPC_Position_PAN_Y_3D,
	POSID_Position_PAN_Z_3D							= RTPC_Position_PAN_Z_3D,
	POSID_PositioningEnableAttenuation				= RTPC_Positioning_EnableAttenuation,
	

//ASSERT that  RTPC_MaxNumRTPC == 32 == sizeof(AkUInt64)*8

	POSID_PanningType								= sizeof(AkUInt64)*8,
	POSID_IsPositionDynamic,
	POSID_IsLooping,
	POSID_Transition,
	POSID_PathMode,
	POSID_ListenerRelativeRouting,
	POSID_Attenuation
};

// pan normally range from 0 to 100, 101 would be invalid.
#define INVALID_PAN_VALUE (101)

struct BaseGenParams
{
	AkReal32 m_fPAN_X_2D;
	AkReal32 m_fPAN_Y_2D;
	AkReal32 m_fCenterPCT;
	AkUInt8/*AkSpeakerPanningType*/	ePannerType : AK_PANNER_NUM_STORAGE_BITS;
	AkUInt8/*bool*/ bHasListenerRelativeRouting : 1;

	void Invalidate()
	{
		m_fPAN_X_2D = INVALID_PAN_VALUE;
	}

	void Init()
	{
		m_fPAN_X_2D = 0.5f;
		m_fPAN_Y_2D = 1.f;
		m_fCenterPCT = 100.f;
		ePannerType = AK_DirectSpeakerAssignment;
		bHasListenerRelativeRouting = false;
	}

	bool operator !=(const BaseGenParams& in_Op) const
	{
		return ( m_fPAN_X_2D		!= in_Op.m_fPAN_X_2D ) 
			|| ( m_fPAN_Y_2D		!= in_Op.m_fPAN_Y_2D ) 
			|| ( m_fCenterPCT	!= in_Op.m_fCenterPCT ) 
			|| (ePannerType != in_Op.ePannerType)
			|| (bHasListenerRelativeRouting != in_Op.bHasListenerRelativeRouting);
	}
};

struct AkPanningConversion
{
	AkReal32 fX;
	AkReal32 fY;
	AkReal32 fCenter;

	AkPanningConversion( const BaseGenParams& in_rBaseParams )
	{
		//transform -100 +100 values to 0..1 float
		fX = ( in_rBaseParams.m_fPAN_X_2D + 100.0f ) * 0.005f;// /200
		fX = AkClamp( fX, 0.f, 1.f );

#if defined( AK_REARCHANNELS )
		fY = ( in_rBaseParams.m_fPAN_Y_2D + 100.0f ) * 0.005f;// /200
		fY = AkClamp( fY, 0.f, 1.f );
#else
		fY = 0.0f;
#endif
# if defined( AK_LFECENTER )
		fCenter = in_rBaseParams.m_fCenterPCT / 100.0f;
		/// No need to clamp fCenter because RTPC is not available for this property. fCenter = AkClamp( fCenter, 0.f, 1.f );
#else
		fCenter = 0.0f;
#endif

	}
};

// As stored in nodes
struct AkPositioningSettings
{
	AkUInt8/*Ak3DPositionType*/	m_e3DPositionType : AK_POSSOURCE_NUM_STORAGE_BITS;
	AkUInt8/*Ak3DSpatializationMode*/ m_eSpatializationMode : AK_SPAT_NUM_STORAGE_BITS;	// 0: none, 1: position only, 2: full (position and orientation)
	AkUInt8				m_bEnableAttenuation : 1;// enable attenuations.
	AkUInt8				m_bHoldEmitterPosAndOrient : 1;			// set position continuously
	AkUInt8				m_bHoldListenerOrient : 1;// follow listener orientation (used with listener-centric automation only).
	AkUInt8				m_bEnableDiffraction : 1;

	inline bool HasAutomation() const
	{
		return (m_e3DPositionType == AK_3DPositionType_EmitterWithAutomation || m_e3DPositionType == AK_3DPositionType_ListenerWithAutomation);
	}
};

// Run-time listener-based positioning params 
struct AkPositioningParams
{
	AkPositioningParams();
	~AkPositioningParams();

	AkReal32			positioningTypeBlend;	// Crossfade value between panning (0) and 3D spatialization (1)
	
	// Attenuation stuff

	// Global members
	AkUniqueID			m_uAttenuationID;		// Attenuation short ID

	// Shared members
	AkReal32			m_fConeOutsideVolume;
	AkReal32			m_fConeLoPass;
	AkReal32			m_fConeHiPass;

	// Settings.
	AkPositioningSettings settings;

	inline CAkAttenuation *	GetRawAttenuation()
	{
		if (!m_pAttenuation && m_uAttenuationID != AK_INVALID_UNIQUE_ID)
			m_pAttenuation = g_pIndex->m_idxAttenuations.GetPtrAndAddRef(m_uAttenuationID);

		return m_pAttenuation;
	}

	void ReleaseAttenuation();

private:
	// private member, refcounted pointer
	CAkAttenuation*		m_pAttenuation;
};

struct Ak3DAutomationParams
{
public:
	Ak3DAutomationParams();
	~Ak3DAutomationParams();

	AkUniqueID			m_ID;					// Id of the owner

	// RTPC offset on 3D automation
	AkVector			m_Position;				// position parameters

	// Pre-defined specific params 
	AkPathMode			m_ePathMode;			// sequence/random & continuous/step
	AkTimeMs			m_TransitionTime;		// 

	// for the paths
	AkPathVertex*		m_pArrayVertex;			// the current path being used
	AkUInt32			m_ulNumVertices;		// how many vertices in m_pArrayVertex
	AkPathListItem*		m_pArrayPlaylist;		// the play list
	AkUInt32            m_ulNumPlaylistItem :31;// own many paths in the play list

	AkUInt32            m_bIsLooping : 1;			// 
};

// Instantiated for playback.
class CAk3DAutomationParams
{
public:
	// position related things
	inline void SetPositionFromPath(const AkVector & in_rPositionDelta){ m_Params.m_Position = m_Params.m_Position + in_rPositionDelta; }

	inline void SetIsLooping( bool in_bIsLooping ) { m_Params.m_bIsLooping = in_bIsLooping; }
	inline AkTimeMs GetTransitiontime(){ return m_Params.m_TransitionTime; }
	void SetTransition(AkTimeMs in_TransitionTime);

	inline void SetPathMode( AkPathMode in_ePathMode ) { m_Params.m_ePathMode = in_ePathMode; }
	
	AKRESULT SetPathPlayList(
		CAkPath*		in_pPath,
		AkPathState*	in_pState
		);

	bool HasPathPlayList()
	{
		return m_Params.m_pArrayPlaylist != NULL &&
			m_Params.m_ulNumPlaylistItem > 0;
	}

	AKRESULT StartPath();
	AKRESULT StopPath();

	bool PathIsDifferent(
		AkPathVertex*           in_pArrayVertex,
		AkUInt32                 in_ulNumVertices,
		AkPathListItemOffset*   in_pArrayPlaylist,
		AkUInt32                 in_ulNumPlaylistItem
		);

	void SetPathOwner(AkUniqueID in_PathOwner) { m_Params.m_ID = in_PathOwner; }

	AkUniqueID GetPathOwner() { return m_Params.m_ID; }

	inline Ak3DAutomationParams & GetParams() { return m_Params; }

#ifndef AK_OPTIMIZED
	void InvalidatePaths();
#endif

protected:
	void UpdateTransitionTimeInVertex();

	Ak3DAutomationParams m_Params;
};

struct AkPathState
{
	AkForceInline AkPathState()
		:ulCurrentListIndex(0)
	{}
	AkUInt32	ulCurrentListIndex;
	AkSafePathStatePtr pbPlayed;
};

// Instantiated for project data (CAkParameterNodeBase): owns the m_pArrayVertex, m_pArrayPlaylist memory.
class CAk3DAutomationParamsEx : public CAk3DAutomationParams
{
public:
	~CAk3DAutomationParamsEx();

	AKRESULT SetPath(
		AkPathVertex*           in_pArrayVertex,
		AkUInt32                 in_ulNumVertices,
		AkPathListItemOffset*   in_pArrayPlaylist,
		AkUInt32                 in_ulNumPlaylistItem
	);

	void FreePathInfo();

	AKRESULT UpdatePathPoint(
		AkUInt32 in_ulPathIndex,
		AkUInt32 in_ulVertexIndex,
		AkVector in_newPosition,
		AkTimeMs in_DelayToNext
	);

	AkPathState	m_PathState;

private:
	void ClearPaths();
};

#endif //_GEN_3D_PARAMS_H_
