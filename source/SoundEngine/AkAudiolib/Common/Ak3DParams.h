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
// Ak3DParams.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _3D_PARAMETERS_H_
#define _3D_PARAMETERS_H_

#include "AkLEngineDefs.h"		
#include "AudiolibLimitations.h"
#include "AkMath.h"
#include "AkDefault3DParams.h"
#include <AK/Tools/Common/AkArray.h>

// Forward definition.
class CAkParameterNode;

struct ConeParams
{
	AkReal32		fInsideAngle;					// radians
	AkReal32		fOutsideAngle;					// radians
	AkReal32		fOutsideVolume;
	AkLPFType		LoPass;
	AkLPFType		HiPass;
};

// Game-driven ray properties
struct AkGameRayParams
{
	AkGameRayParams()
		:occlusion(0.f), obstruction(0.f), spread(0.f), focus(0.f) {}

	AkReal32 occlusion;		// OcclusionLevel: [0.0f..1.0f]
	AkReal32 obstruction;	// ObstructionLevel: [0.0f..1.0f]
	AkReal32 spread;		// Spread: 0-100
	AkReal32 focus;			// Focus: 0-100
};

// Volume data associated to a ray (emitter-listener pair).
class AkRayVolumeData : public AkEmitterListenerPair
{
public:
	AkRayVolumeData()
		: AkEmitterListenerPair()	
		, scalingFactor(1.f)
		, fConeInterp(1.f)
		, fLPF(0.f)
		, fHPF(0.f)
#ifndef AK_OPTIMIZED
		, bitsVolume(0)
		, bitsFilter(0)
#endif
	{}

	// Setters.
	AkForceInline void SetDistance(AkReal32 in_fDistance) { fDistance = in_fDistance; }
	AkForceInline void SetEmitterAngle(AkReal32 in_fAngle) { fEmitterAngle = in_fAngle; }
	AkForceInline void SetListenerAngle(AkReal32 in_fAngle) { fListenerAngle = in_fAngle; }
	AkForceInline void SetListenerID(AkGameObjectID in_uListenerID) 
	{ 
#ifndef AK_OPTIMIZED
		if (m_uListenerID != in_uListenerID)
		{
			bitsVolume = 0;
			bitsFilter = 0;
		}
#endif
		m_uListenerID = in_uListenerID; 
		
	}

	inline void UpdateRay(const AkTransform & in_posEmitter, const AkGameRayParams & in_gameRayParams, AkGameObjectID in_uListenerID, AkUInt32 in_uEmitterChannelMask)
	{
		// Emitter data
		emitter = in_posEmitter;		
		fObstruction = in_gameRayParams.obstruction * 100.f;
		fOcclusion = in_gameRayParams.occlusion * 100.f;
		fSpread = in_gameRayParams.spread;
		fFocus = in_gameRayParams.focus;

		uEmitterChannelMask = in_uEmitterChannelMask;
		SetListenerID(in_uListenerID);
	}	

	inline void CopyEmitterListenerData(const AkEmitterListenerPair &in_copy)
	{		
		((AkEmitterListenerPair&)(*this)) = in_copy;
	}
	
	AkReal32	scalingFactor;	// Combined scaling factor due to both emitter and listener.
	AkReal32	fConeInterp;	// 0-1

	AkReal32	fLPF;
	AkReal32	fHPF;

#ifndef AK_OPTIMIZED
	AkUInt8 bitsVolume;
	AkUInt8 bitsFilter;
#endif
};

typedef AkArray<AkRayVolumeData, const AkRayVolumeData &, ArrayPoolDefault> AkVolumeDataArray;

// NOTE: Definition of the following structures here in this file is questionable.



#endif //_3D_PARAMETERS_H_
