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

#pragma once

#include <AK/SoundEngine/Common/AkMemoryMgr.h>
#include <AK/Tools/Common/AkObject.h>
#include <AK/Tools/Common/AkArray.h>
#include <AK/Tools/Common/AkBankReadHelpers.h>
#include "AkParameters.h"

//#include "AkMonitor.h"
#define AKDELTAMONITOR_MIN_INCREMENT 1024

class CAkParameterNodeBase;

//#define AK_DELTAMONITOR_RELEASE	//Uncomment to enable voice inspector data capture to file in Release.
#if !defined AK_OPTIMIZED || defined AK_DELTAMONITOR_RELEASE
#define AK_DELTAMONITOR_ENABLED
#endif


//Used to know when to send Ray volumes to profiler.
namespace AkMonitorData
{
	enum AkDeltaVolumeBits
	{
		DeltaRayDry = 0,
		DeltaRayGameAux,
		DeltaRayUserAux,
		DeltaRayCone,
		DeltaRayOcclusion,
		DeltaRayObstruction,		
		DeltaRayGameAuxUseDry,
		DeltaRayUserAuxUseDry,	//MAXIMUM 8 flags.  Order is important as it defines the serialization order.
	};

	enum AkDeltaFilterBits
	{
		DeltaRayLPF,
		DeltaRayHPF,
		DeltaRayConeLPF,
		DeltaRayConeHPF,
		DeltaRayOcclusionLPF,
		DeltaRayOcclusionHPF,
		DeltaRayObstructionLPF,
		DeltaRayObstructionHPF, //MAXIMUM 8 flags.  Order is important as it defines the serialization order.
	};
}

//This class implements the ring buffer used for parameter changes harvesting
//All parameter changes pertain to one instance of a sound, so each change is prefixed with the sound ID and the pipeline ID.
//Bus parameter changes are tagged with Bus ID and PipelineID.
//Parameter changes can occur at different levels in the hierarchy and this is indicated by the SourceID (source of the change).
//A brace system exists to group some changes together for efficiency.  For example, at the start of a sound, all starting parameters are grouped.
//There is a sister class AkDeltaMonitorDisabled that implements the same interface, but compiles to nothing in Release.
class AkDeltaMonitorEnabled
{
public:
	static AKRESULT Init();
	static void Term();

	//To be used by the AkProfiler when this data set is enabled/disabled in the profiler.
	static void Enable() { m_bActive = (m_pData != NULL); }
	static void Disable() { m_bActive = false; }
	static bool Enabled() { return m_bActive; }
	
	//Opens a brace for a Source of change.  These braces are stacked.
	static inline void OpenObjectBrace(AkUniqueID in_idObj) 
	{ 	
		if (m_bActive)
		{
			AkUniqueID *p = m_Stack.AddLast();
			if (p)
			{
				*p = m_idSource;
				m_idSource = in_idObj;
			}
			else
				m_bSerializeSuccess = false;
		}
	}
	
	static inline void CloseObjectBrace() 
	{ 		
		if (m_bActive)
		{
			AKASSERT(!m_Stack.IsEmpty());
			m_idSource = m_Stack.Last();
			m_Stack.RemoveLast();
		}
	}

	//Opens a brace for a full recomputation of a Sound's parameters.
	//Happens at the start of the sound, or if RefreshParameters is called.
	static inline void OpenSoundBrace(AkUniqueID in_idSound, AkPipelineID in_pipelineID)
	{	
		if (m_idSound == AK_INVALID_UNIQUE_ID)
		{
			m_idSound = in_idSound;
			m_pipelineID = in_pipelineID;

			AKASSERT(m_pWritePtr == m_pData);

			Put((AkUInt8)AkDelta_Pipeline);
			Put(m_pipelineID);
		}
		m_bSerializeSuccess = true;
		m_bRealDelta = false;

		OpenObjectBrace(in_idSound);
	}

	static inline bool CloseSoundBrace()
	{	
		bool bSuccess = true;
		CloseObjectBrace();
		if (m_Stack.IsEmpty())
		{
			if (m_bActive && m_bRealDelta)
			{
				if (m_bSerializeSuccess)
					bSuccess = Post();
				else
					bSuccess = false;
			}

			m_pWritePtr = m_pData;
			m_idSound = AK_INVALID_UNIQUE_ID;
		}
		return bSuccess;
	}

	static inline void OpenPrecomputedParamsBrace(AkUniqueID in_idObj)
	{
		if (m_idSound == AK_INVALID_UNIQUE_ID)
		{
			m_idSound = in_idObj;
			m_pipelineID = AK_INVALID_PIPELINE_ID;

			AKASSERT(m_pWritePtr == m_pData);

			Put((AkUInt8)AkDelta_CachePrecomputedParams);
			Put(in_idObj);
		}
		m_bSerializeSuccess = true;
		m_bRealDelta = false;

		OpenObjectBrace(in_idObj);
	}

	static inline bool ClosePrecomputedParamsBrace()
	{
		return CloseSoundBrace();
	}

	//RTPC updates are special as they affect multiple targets with the same value.
	//Tag this message with an invalid ID, we'll interpret that as an multi-target update
	static inline void OpenUpdateBrace(AkDeltaType in_eReason, AkUniqueID in_idSource = AK_INVALID_UNIQUE_ID, AkDeltaType in_updateType = AkDelta_MultiUpdate)
	{
		if (m_pData)
		{
			AKASSERT(m_pWritePtr == m_pData);			
			
			// We have come across some corner cases that lead to the above assert failing.  This is very dangerous and
			// leads to desynchronization with Wwise.exe.  Set as expected just to be safe.
			m_pWritePtr = m_pData;			
			
			Put((AkUInt8)in_updateType);
			Put((AkUInt8)in_eReason);

			if (in_idSource != AK_INVALID_UNIQUE_ID)
				OpenObjectBrace(in_idSource);
		}
	}

	static inline bool IsUpdateBraceOpen()
	{
		return m_pWritePtr != m_pData && m_pData != nullptr && *(AkUInt8*)m_pData == AkDelta_MultiUpdate;
	}

	static inline void CloseUpdateBrace(AkUniqueID in_idSource = AK_INVALID_UNIQUE_ID)
	{
		if (in_idSource != AK_INVALID_UNIQUE_ID)
		{
			CloseObjectBrace();
		}
		AKASSERT( m_Stack.IsEmpty() );

		if (m_bSerializeSuccess && m_bActive && m_bRealDelta)
			Post();

		m_pWritePtr = m_pData;		
	}

	//RTPC/States/Modulator updates are special as they affect multiple targets with the same value.
	//Tag this message with an invalid ID, we'll interpret that as an multi-target update	
	static inline void OpenMultiTargetBrace(AkDeltaType in_eReason, AkUniqueID in_idDriver, AkReal32 in_driverVal)
	{
		OpenUpdateBrace(in_eReason, AK_INVALID_UNIQUE_ID, AkDelta_MultiUpdate);		

		Put(in_idDriver);
		PutFloat24(in_driverVal);

		m_bSerializeSuccess = true;
		m_bRealDelta = false;
	}
	
	static inline void OpenMultiTargetBrace(AkDeltaType in_eReason, AkUniqueID in_idDriver, AkUniqueID in_driverVal)
	{
		OpenUpdateBrace(in_eReason, AK_INVALID_UNIQUE_ID, AkDelta_MultiUpdate);		

		Put(in_idDriver);
		Put(in_driverVal);

		m_bSerializeSuccess = true;
		m_bRealDelta = false;
	}

	static inline void CloseMultiTargetBrace()
	{
		CloseUpdateBrace();
	}

	static inline void OpenUnsetBrace(AkDeltaType in_eReason, AkUniqueID in_idSource, AkPropID in_propID)
	{
		OpenUpdateBrace(in_eReason, in_idSource, AkDelta_MultiUnset);
		
		Put(in_idSource);
		Put((AkUInt8)in_propID);

		m_bSerializeSuccess = true;
		m_bRealDelta = false;
	}

	static void LogStateUnset(CAkParameterNodeBase* in_pNode, AkUniqueID in_uStateGroup);

	static inline void LogPipelineID(AkPipelineID in_pipelineID)
	{
		if (CheckSize(sizeof(AkPipelineID))) // m_bActive already checked in LogStateUnset
		{
			Put(in_pipelineID);
			m_bRealDelta = true;
		}
	}
	
	static inline void CloseUnsetBrace(AkUniqueID in_idSource)
	{
		CloseUpdateBrace(in_idSource);
	}

	//Same as Sound brace, but without the stacking
	static inline void OpenRTPCTargetBrace(AkUniqueID in_idSound, AkPipelineID in_pipelineID)
	{		
		m_idSound = in_idSound;
		m_pipelineID = in_pipelineID;
	}

	static inline void CloseRTPCTargetBrace()
	{
		m_idSound = AK_INVALID_UNIQUE_ID;				
	}	

	static inline void OpenEmitterListenerBrace(AkGameObjectID in_emitter, AkDeltaType in_eReason, AkPropID in_PropID)
	{
		AKASSERT(!m_bActive || m_pWritePtr == m_pData); // No message should have been sent
		AKASSERT(!m_bActive || m_idSound == AK_INVALID_UNIQUE_ID); // We must not already be in a sound brace

		Put((AkUInt8)AkDelta_EmitterListener);
		Put(in_emitter);
		Put((AkUInt8)in_eReason);
		Put((AkUInt8)in_PropID);

		m_bSerializeSuccess = true;
		m_bRealDelta = false;
	}

	// Always send neutral values unless we specify that we don't want it (e.g. for recap)
	static inline void LogListenerDelta(AkGameObjectID in_listener, AkReal32 in_fDelta, AkReal32 in_fNeutralVal = 1.0f, bool logNeutral = true)
	{	
		AKASSERT(!m_bActive || *(AkUInt8*)m_pData == AkDelta_EmitterListener);

		bool bLog = m_bActive && (logNeutral || in_fDelta != in_fNeutralVal); //Don't send useless values.
		if (bLog && CheckSize(sizeof(AkGameObjectID) + sizeof(AkReal32)))
		{
			Put(in_listener);
			PutFloat24(in_fDelta);
			m_bRealDelta = true;
		}
	}

	static inline void CloseEmitterListenerBrace()
	{	
		if (m_bSerializeSuccess && m_bActive && m_bRealDelta)
			Post();

		m_pWritePtr = m_pData;
	}

	static inline void LogEarlyReflectionsVolume(AkGameObjectID in_emitter, AkReal32 in_fDelta)
	{
		if (m_bActive && CheckSize(sizeof(AkReal32)))
		{
			OpenEmitterListenerBrace(in_emitter, AkDelta_SetEarlyReflectionsVolume, AkPropID_ReflectionBusVolume);
			LogListenerDelta(in_emitter, in_fDelta, 1.0f, true); // emitter/listener are one and the same.
			m_bRealDelta = true;
			CloseEmitterListenerBrace();
		}
	}

	static inline void LogGameObjectDestroyed(AkGameObjectID in_gameObjectID, bool in_isListener)
	{	
		if (m_bActive && CheckSize(sizeof(AkUInt8) + sizeof(AkGameObjectID) + sizeof(AkUInt8)))
		{
			AKASSERT(m_pWritePtr == m_pData); // No message should have been sent
			AKASSERT(m_idSound == AK_INVALID_UNIQUE_ID); // We must not already be in a sound brace
			Put((AkUInt8)AkDelta_GameObjectDestroyed);
			Put(in_gameObjectID);
			Put((AkUInt8)in_isListener);

			if (m_bSerializeSuccess && m_bActive)
				Post();
		
			m_pWritePtr = m_pData;
		}
	}
	
	static inline void OpenLiveEditBrace(AkUniqueID in_idObj)
	{	
		if (m_bActive)
		{
			AKASSERT(m_pWritePtr == m_pData);
			AKASSERT(m_idSound == AK_INVALID_UNIQUE_ID);
			AKASSERT(m_idSource == AK_INVALID_UNIQUE_ID);

			OpenObjectBrace(in_idObj);

			Put((AkUInt8)AkDelta_LiveEdit);
			Put(in_idObj);

			m_bSerializeSuccess = true;
			m_bRealDelta = false;
		}
	}

	static inline void LogAkProp(AkPropID in_prop, AkReal32 in_fValue, AkReal32 in_fDelta)
	{
		AKASSERT(!m_bActive || m_pWritePtr != m_pData);
		AKASSERT(!m_bActive || m_idSource != AK_INVALID_UNIQUE_ID);
		AKASSERT(!m_bActive || *m_pData == AkDelta_LiveEdit);
		AKASSERT(CheckSize(7));

		Put((AkUInt8)in_prop);
		PutFloat24(in_fValue);
		PutFloat24(in_fDelta);
		m_bRealDelta = true;
	}

	static inline void CloseLiveEditBrace()
	{		
		const AkUInt32 c_MsgSize = 1 /*LiveEdit tag*/ + sizeof(AkUniqueID) + 1 /*Propid*/ + 3 /*Value, float24*/ + 3 /*OldValue, float24*/;
		CloseObjectBrace();
		if (m_bSerializeSuccess && m_bActive && m_bRealDelta)
		{
			//The brace might include a lot more unneeded data, on live sounds.  We don't need that.
			m_pWritePtr = m_pData + c_MsgSize;
			Post();
		}

		m_pWritePtr = m_pData;		
	}


	static inline void LogDriver(AkUniqueID in_idDriver, AkReal32 in_fDriverVal)
	{
		AKASSERT(!m_bActive || m_pWritePtr != m_pData);	//Something SHOULD have been written already		
	
		// LogDriver should be called right after OpenUpdateBrace.
		// Since we're at the start of the buffer, we assume
		// there's enough space and skip CheckSize().
		Put(in_idDriver);
		PutFloat24(in_fDriverVal);
	}

	static inline void LogDriver(AkUniqueID in_idDriver, AkUniqueID in_uDriverVal)
	{	
		AKASSERT(!m_bActive || m_pWritePtr != m_pData);	//Something SHOULD have been written already		

		// Skip CheckSize (see comment above)
		Put(in_idDriver);
		Put(in_uDriverVal);
	}

	static inline void LogBaseValue(AkUInt32 in_idSource, AkPropID in_PropID, AkReal32 in_fDelta)
	{
		AKASSERT(!m_bActive || m_idSound != AK_INVALID_UNIQUE_ID);
		LogShortMsg(in_idSource, AkDelta_BaseValue, in_PropID, in_fDelta, 0.f);
	}

	static inline void LogState(AkUniqueID in_ulStateGroup, AkRTPC_ParameterID in_PropID, AkUniqueID in_ulActualState, AkReal32 in_fValue)
	{
		AKASSERT(!m_bActive || m_idSound != AK_INVALID_UNIQUE_ID);
		AKASSERT(!m_bActive || m_idSource != AK_INVALID_UNIQUE_ID);
		LogLongMsg(m_idSource, AkDelta_State, g_AkRTPCToPropID[in_PropID], in_fValue, in_ulStateGroup, in_ulActualState );
	}

	static inline void LogRTPC(AkRtpcID in_RTPC, AkRTPC_ParameterID in_PropID, AkReal32 in_fRTPCVal, AkReal32 in_fValue, AkDeltaType in_eReason)
	{		
		AKASSERT(!m_bActive || m_idSound != AK_INVALID_UNIQUE_ID);
		//m_idSource can be invalid, for global RTPCs.
		LogLongMsg(m_idSource, in_eReason, g_AkRTPCToPropID[in_PropID], in_fValue, in_RTPC, in_fRTPCVal);
	}

	static inline void LogPluginRTPC(AkRtpcID in_RTPC, AkReal32 in_fRTPCVal, AkDeltaType in_eReason)
	{
		AKASSERT(!m_bActive || m_idSound != AK_INVALID_UNIQUE_ID);
		//m_idSource can be invalid, for global RTPCs.
		LogLongMsg(m_idSource, in_eReason, AkPropID_NUM, 0.0f, in_RTPC, in_fRTPCVal);
	}

	static inline void LogModulator(AkModulatorID in_mod, AkRTPC_ParameterID in_PropID, AkReal32 in_fValue, AkReal32 in_fModVal, AkUniqueID in_idSource, AkDeltaType in_eReason)
	{		
		if (m_bActive && CheckSize(AKDELTA_RTPC_PACKET_SIZE))
		{
			AKASSERT(m_idSource != AK_INVALID_UNIQUE_ID);
			Put((AkUInt8)in_eReason);
			Put((AkUInt8)g_AkRTPCToPropID[in_PropID]);
			PutFloat24(in_fValue);
			Put(in_idSource);
			Put(in_mod);
			PutFloat24(in_fModVal);
			m_bRealDelta = true;
		}
	}

	static inline void LogGameObjectOutputBusVolume(AkReal32 in_fGain)
	{
		AKASSERT(!m_bActive || m_idSource != AK_INVALID_UNIQUE_ID);
		LogShortMsg(m_idSource, AkDelta_SetGameObjectOutputBusVolume, AkPropID_OutputBusVolume, in_fGain, 1.0f);
	}

	static inline void LogUpdate(AkPropID in_PropID, AkReal32 in_fValue)
	{					
		if (m_bActive && CheckSize(AKDELTA_RTPC_PACKET_SIZE))
		{	
			AKASSERT(m_idSource != AK_INVALID_UNIQUE_ID);
			AKASSERT(m_idSound != AK_INVALID_UNIQUE_ID);
			AKASSERT(*(AkUInt8*)m_pData == AkDelta_MultiUpdate || *(AkUInt8*)m_pData == AkDelta_MultiUnset || *(AkUInt8*)m_pData == AkDelta_LiveEdit);
			Put(m_pipelineID);
			Put(m_idSource);
			Put((AkUInt8)in_PropID);
			PutFloat24(in_fValue);
			m_bRealDelta = true;			
		}
	}

	static inline void LogUpdate(AkPropID in_PropID, AkReal32 in_fValue, AkUniqueID in_idSound, AkPipelineID in_pipelineID)
	{		
		if (m_bActive && CheckSize(AKDELTA_RTPC_PACKET_SIZE))
		{
			AKASSERT(m_idSource != AK_INVALID_UNIQUE_ID);
			AKASSERT(in_idSound != AK_INVALID_UNIQUE_ID);
			AKASSERT(m_pData != m_pWritePtr);
			AKASSERT(*(AkUInt8*)m_pData == AkDelta_MultiUpdate || *(AkUInt8*)m_pData == AkDelta_MultiUnset || *(AkUInt8*)m_pData == AkDelta_LiveEdit);
			Put(in_pipelineID);
			Put(m_idSource);
			Put((AkUInt8)in_PropID);
			PutFloat24(in_fValue);
			m_bRealDelta = true;
		}
	}
	
	static inline void LogDelta(AkDeltaType in_eReason, AkPropID in_PropID, AkReal32 in_fDelta, const AkReal32 in_fNeutralVal = 0.0f)
	{		
		AKASSERT(!m_bActive || m_idSound != AK_INVALID_UNIQUE_ID);
		LogShortMsg(m_idSource, in_eReason, in_PropID, in_fDelta, in_fNeutralVal);	
	}

	static inline void StartAuxGameDefVolumes(AkUInt32 in_nbSends)
	{		
		#define AUX_GAME_DEF_VOLUME_HEADER_SIZE (sizeof(AkUInt8) + sizeof(AkUniqueID))
		#define AUX_GAME_DEF_VOLUME_SIZE (sizeof(AkUniqueID) + sizeof(AkGameObjectID) + 3 /*sizeof(float24)*/)

		bool bLog = m_bActive && m_idSound != AK_INVALID_UNIQUE_ID;
		if (bLog && CheckSize(AUX_GAME_DEF_VOLUME_HEADER_SIZE + in_nbSends * AUX_GAME_DEF_VOLUME_SIZE))
		{
			AKASSERT(m_idSource != AK_INVALID_UNIQUE_ID);
			Put((AkUInt8)AkDelta_SetGameObjectAuxSendValues);
			// AkPropID_GameAuxSendVolume is implicit
			Put(m_idSource);
		}
	}
	
	static inline void LogAuxGameDefVolume(AkUniqueID in_busID, AkGameObjectID in_listenerID, AkReal32 in_fGain)
	{		
		// Aux sends are not set if in_fGain < g_fVolumeTreshold
		// so no need to validate that it's not 0.0f
		if (m_bActive && m_bSerializeSuccess)
		{
			// We assume m_idSource was checked in StartAuxGameDefVolume()
			// and that CheckSize was also called according to the max number of sends.
			AKASSERT(in_busID != AK_INVALID_UNIQUE_ID);

			Put(in_busID);
			Put(in_listenerID);
			PutFloat24(in_fGain);
			m_bRealDelta = true;
		}
	}

	static inline void StartRoomSendVolumes()
	{
		bool bLog = m_bActive && m_idSound != AK_INVALID_UNIQUE_ID;
		if (bLog && CheckSize(AUX_GAME_DEF_VOLUME_HEADER_SIZE + 2 * AUX_GAME_DEF_VOLUME_SIZE))
		{
			AKASSERT(m_idSource != AK_INVALID_UNIQUE_ID);

			Put((AkUInt8)AkDelta_PortalCrossfade);
			// AkPropID_GameAuxSendVolume is implicit
			Put(m_idSource);
		}
	}

	static inline void LogRoomSendVolume(AkUniqueID in_busID, AkGameObjectID in_listenerID, AkReal32 in_fGain)
	{
		if (m_bActive && in_fGain != 1.0f)
		{
			AKASSERT(m_idSource != AK_INVALID_UNIQUE_ID);
			AKASSERT(in_busID != AK_INVALID_UNIQUE_ID);
			Put(in_busID);
			Put(in_listenerID);
			PutFloat24(in_fGain);

			m_bRealDelta = true;
		}
	}

	static inline void LogUsePrecomputedParams(AkUniqueID in_idParent)
	{
		AKASSERT(!m_bActive || m_idSound != AK_INVALID_UNIQUE_ID);

		// This should be called right after OpenSoundBrace or OpenPrecomputedParamsBrace
		// Since we're still at the start of the message, it should fit in the first 1024 bytes
		// so we skip CheckSize()
		Put((AkUInt8)AkDelta_UsePrecomputedParams);
		Put(in_idParent);
		m_bRealDelta = true;
	}	

#define AKDELTA_RAY_BASE_SIZE (sizeof(AkUInt8) + sizeof(AkUInt8)) // AkDelta_Rays + nbOfRays
#define AKDELTA_RAY_VALUES_DRIVERS_SIZE ((8 + 4)*3) // 8 components (see AkMonitor::DeltaRayXXX enum) + 4 possible driver (float24) values (Distance, Angle, Occlusion, Obstruction)
	static inline bool LogVolumeRays(AkUInt8 in_count)
	{
		// With volume rays, we know how many rays to send
		
		// AkDelta_Rays + nbRays * (rayIndex + listenerID + flags + (values + drivers))
		AkUInt32 sizeExpected = AKDELTA_RAY_BASE_SIZE + in_count * (sizeof(AkUInt8) + sizeof(AkGameObjectID) + sizeof(AkUInt8) + AKDELTA_RAY_VALUES_DRIVERS_SIZE);
		if (CheckSize(sizeExpected))
		{
			Put((AkUInt8)AkDelta_Rays);
			Put(in_count);
			return true;
		}
		return false;
	}

	static inline bool LogFilterRays(AkUInt8 in_count)
	{
		// With filter rays we check the size for each connection
		// AkDelta_RaysFilter + nbRays
		AkUInt32 sizeExpected = AKDELTA_RAY_BASE_SIZE;
		if (CheckSize(sizeExpected))
		{
			Put((AkUInt8)AkDelta_RaysFilter);
			Put(in_count);
			return true;
		}
		return false;
	}

	static inline void LogAttenuation(AkReal32 in_fVal, AkUInt8 in_eType, AkUInt8* AK_RESTRICT in_pCurrentFlags, AkUInt8 &io_RayHistory, AkReal32 in_fNeutral)
	{
		if (m_bSerializeSuccess && (in_fVal != in_fNeutral || (io_RayHistory & (1 << in_eType))))
		{
			PutFloat24(in_fVal);
			*in_pCurrentFlags |= (1 << in_eType);
			io_RayHistory |= (AkUInt8)(in_fVal != in_fNeutral) << in_eType;
		}
	}

	static inline AkUInt8* StartVolumeRay(AkUInt8 in_uCount, int in_rayIndex, AkGameObjectID in_idListener)
	{
		if (m_bSerializeSuccess)
		{
			if (in_uCount > 1)
			{
				Put((AkUInt8)in_rayIndex);
				Put(in_idListener);
			}

			*m_pWritePtr = 0;	//Init the flags
			m_pWritePtr += 1;	//Space for the Flags
			return (AkUInt8*)(m_pWritePtr - 1);
		}
		return NULL;
	}

	// Filtering rays are discriminated using the output bus pipeline ID, we don't need the listener ID
	static inline AkUInt8* StartFilteringRay(AkUInt8 in_uCount, int in_rayIndex)
	{
		if (m_bSerializeSuccess)
		{
			if (in_uCount > 1)
				Put((AkUInt8)in_rayIndex);

			*m_pWritePtr = 0;	//Init the flags
			m_pWritePtr += 1;	//Space for the Flags
			return (AkUInt8*)(m_pWritePtr - 1);
		}
		return NULL;	
	}

	static inline AkUInt8* StartConnectionFilteringRays(AkPipelineID in_connectionPipelineID, AkUInt8 in_uRays)
	{
		// connectionPipelineID + nbRaysForConnection + nbRaysForConnection * (rayIndex + flags + (values + drivers))
		AkUInt32 sizeExpected = sizeof(AkPipelineID) + sizeof(AkUInt8) + in_uRays * (sizeof(AkUInt8) + sizeof(AkUInt8) + AKDELTA_RAY_VALUES_DRIVERS_SIZE);

		if (m_bSerializeSuccess && CheckSize(sizeExpected))
		{
			Put(in_connectionPipelineID);

			*m_pWritePtr = 0; // Init the number of processed rays
			m_pWritePtr += 1; // Space for the number of processed rays
			return (AkUInt8*)(m_pWritePtr - 1);
		}
		return NULL;
	}

	static inline void FinalizeRay(AkUInt8* AK_RESTRICT in_pFlags)
	{
		m_bRealDelta |= (*in_pFlags != 0);
	}

#define AKDELTAMONITOR_WILL_WRITE_OVERRUN(SIZEOFWRITE) (((AkUInt32)(m_pWritePtr - m_pData) + SIZEOFWRITE) > m_cReserved)

	template<class T>
	static inline void Put(const T& val)
	{
		AKASSERT(AKDELTAMONITOR_WILL_WRITE_OVERRUN(sizeof(T)) == false);
		AK::WriteUnaligned<T>(m_pWritePtr, val);
		m_pWritePtr += sizeof(T);
	}

	//Specialization for floats.  In general we don't need the full precision.		
	static inline void PutFloat24(const AkReal32& val)
	{
		AKASSERT(AKDELTAMONITOR_WILL_WRITE_OVERRUN(3) == false);

#if defined(AK_ENDIANNESS_LITTLE)
		AkUInt8 *p = ((AkUInt8*)&val);
		m_pWritePtr[0] = p[1];
		m_pWritePtr[1] = p[2];
		m_pWritePtr[2] = p[3];		
		m_pWritePtr += 3;
#else
#error Big Endian not supported yet
#endif
	}

	static inline bool IsSoundBraceOpen() { return m_idSound != AK_INVALID_UNIQUE_ID; }

private:
	
	static bool Post();	
		
	//For better memory management and performance, we could use the brace information to send less data.
	//Note the absence of error checking.  We'll try to put everything, but drop the whole packet at the moment of the Post if an error occurred.

	static const AkUInt32 c_HeaderSize = sizeof(AkUInt32) + sizeof(AkUInt32);
	static const AkUInt32 c_ShortSize = sizeof(AkUInt32) + sizeof(AkUInt8) + sizeof(AkUInt8) + sizeof(AkReal32);
	static const AkUInt32 c_LongSize = c_ShortSize + sizeof(AkUInt32) + sizeof(AkUInt32);

	static AkForceInline bool CheckSize(AkUInt32 in_uSize)
	{
		if ((AkUInt32)(m_pWritePtr - m_pData) + in_uSize <= m_cReserved)
			return true;		
					
		return ReallocBuffer(in_uSize);		
	}

	static bool ReallocBuffer(AkUInt32 in_uSize);

	static inline void LogShortMsg(AkUInt32 in_idSource, AkDeltaType in_eReason, AkPropID in_PropID, AkReal32 in_fDelta, const AkReal32 in_fNeutralVal)
	{		
		bool bLog = m_bActive && m_idSound != AK_INVALID_UNIQUE_ID && in_fDelta != in_fNeutralVal; //Don't send useless values.
		if (bLog && CheckSize(c_ShortSize))
		{	
			Put((AkUInt8)in_eReason);
			Put((AkUInt8)in_PropID);
			PutFloat24(in_fDelta);
			Put(in_idSource);									
			m_bRealDelta = true;			
		}
	}
		
	static inline void LogLongMsg(AkUInt32 in_idSource, AkDeltaType in_eReason, AkPropID in_PropID, AkReal32 in_fDelta,  AkUInt32 in_extra1, AkReal32 in_extra2)
	{	
		bool bLog = m_bActive && m_idSound != AK_INVALID_UNIQUE_ID;
		if (bLog && CheckSize(c_LongSize)) /*Long MSG are only used for RTPC, and we send RTPC for ALL properties*/
		{			
			Put((AkUInt8)in_eReason);
			Put((AkUInt8)in_PropID);
			PutFloat24(in_fDelta);
			Put(in_idSource);
			Put(in_extra1);
			PutFloat24(in_extra2);			
			m_bRealDelta = true;
		}		
	}

	static inline void LogLongMsg(AkUInt32 in_idSource, AkDeltaType in_eReason, AkPropID in_PropID, AkReal32 in_fDelta, AkUInt32 in_extra1, AkUniqueID in_extra2)
	{		
		bool bLog = m_bActive && m_idSound != AK_INVALID_UNIQUE_ID;
		if (bLog && CheckSize(c_LongSize))
		{
			Put((AkUInt8)in_eReason);
			Put((AkUInt8)in_PropID);
			PutFloat24(in_fDelta);
			Put(in_idSource);
			Put(in_extra1);
			Put(in_extra2);
			m_bRealDelta = true;
		}
	}	
	
	static AkUInt8* m_pData;
	static AkUInt32 m_cReserved;
	static AkUInt8* m_pWritePtr;
	static AkUniqueID m_idSound;
	static AkUniqueID m_idSource;
	static AkPipelineID m_pipelineID;	
	static AkUniqueID m_idRTPC;	
	static AkArray<AkUniqueID, AkUniqueID> m_Stack;

	static bool m_bActive;
	static bool m_bSerializeSuccess;
	static bool m_bRealDelta;	

	friend struct AkDeltaMonitorObjBrace;
	friend struct AkDeltaMonitorUpdateBrace;
	friend class AkDeltaStackUpdateBrace;
};


class AkDeltaStackUpdateBrace
{
#ifndef AK_OPTIMIZED
public:
	AkDeltaStackUpdateBrace() : m_pOldData(NULL), m_pOldWrite(NULL), m_cReserved(0)
	{
		if (AkDeltaMonitorEnabled::m_pData != AkDeltaMonitorEnabled::m_pWritePtr)
		{
			AKASSERT(AkDeltaMonitorEnabled::m_pData[0] == AkDelta_MultiUpdate);
			AKASSERT(AkDeltaMonitorEnabled::m_Stack.IsEmpty());

			m_pOldData = AkDeltaMonitorEnabled::m_pData;
			m_pOldWrite = AkDeltaMonitorEnabled::m_pWritePtr;
			m_cReserved = AkDeltaMonitorEnabled::m_cReserved;

			AkDeltaMonitorEnabled::m_pData = m_Space;
			AkDeltaMonitorEnabled::m_pWritePtr = m_Space;
			AkDeltaMonitorEnabled::m_cReserved = sizeof(m_Space);
			m_bSerializeSuccess = AkDeltaMonitorEnabled::m_bSerializeSuccess;
			m_bRealDelta = AkDeltaMonitorEnabled::m_bRealDelta;			
		}			
	}

	~AkDeltaStackUpdateBrace()
	{
		if (m_pOldData)
		{			
			AKASSERT(AkDeltaMonitorEnabled::m_Stack.IsEmpty());
			AKASSERT(AkDeltaMonitorEnabled::m_pData == AkDeltaMonitorEnabled::m_pWritePtr);		
			AkDeltaMonitorEnabled::m_pData = m_pOldData;
			AkDeltaMonitorEnabled::m_pWritePtr = m_pOldWrite;
			AkDeltaMonitorEnabled::m_cReserved = m_cReserved;
			AkDeltaMonitorEnabled::m_bSerializeSuccess = m_bSerializeSuccess;
			AkDeltaMonitorEnabled::m_bRealDelta = m_bRealDelta;
		}
	}

private:
	AkUInt8 *m_pOldData;
	AkUInt8 *m_pOldWrite;
	AkUInt32 m_cReserved;
	AkUInt8 m_Space[4000];	//Why 4000?  Just a number.  High enough for large hierarchies, low enough to be on the stack.

	bool m_bSerializeSuccess;
	bool m_bRealDelta;

#endif
};

#ifndef AK_OPTIMIZED
#define AK_DELTA_MONITOR_STACK_UPDATE_SCOPE AkDeltaStackUpdateBrace __akDeltaStackUpdateBrace
#else
#define AK_DELTA_MONITOR_STACK_UPDATE_SCOPE
#endif

//This class is used in code paths where AkDeltaMonitor isn't active (untracked properties) or in Release.
class AkDeltaMonitorDisabled
{
public:
	static inline void OpenObjectBrace(AkUniqueID in_idObj) {}
	static inline void CloseObjectBrace() {}
	static inline void OpenSoundBrace(AkUniqueID in_idSound, AkPipelineID in_pipelineID) {}
	static inline bool CloseSoundBrace() { return true; }
	static inline void OpenPrecomputedParamsBrace(AkUniqueID in_idObj) {}
	static inline void ClosePrecomputedParamsBrace() {}
	static inline void OpenUpdateBrace(AkDeltaType in_eReason, AkUniqueID in_idSource = AK_INVALID_UNIQUE_ID, AkDeltaType in_updateType = AkDelta_MultiUpdate) {}
	static inline bool IsUpdateBraceOpen() { return false; }
	static inline void CloseUpdateBrace(AkUniqueID in_idSource = AK_INVALID_UNIQUE_ID) {}
	template<typename DRIVER_VAL>
	static inline void OpenMultiTargetBrace(AkDeltaType in_eReason, AkUniqueID in_idDriver, DRIVER_VAL in_fDriverVal) {}
	static inline void CloseMultiTargetBrace() {}
	static inline void OpenUnsetBrace(AkDeltaType in_eReason, AkUniqueID in_idSource, AkPropID in_propID) {}
	static inline void LogPipelineID(AkPipelineID in_pipelineID) {}
	static inline void CloseUnsetBrace(AkUniqueID in_idSource) {}
	static inline void OpenRTPCTargetBrace(AkUniqueID in_idSound, AkPipelineID in_pipelineID) {}
	static inline void CloseRTPCTargetBrace() {}
	static inline void OpenEmitterListenerBrace(AkGameObjectID in_emitter, AkDeltaType in_eReason, AkPropID in_PropID) {}
	static inline void LogListenerDelta(AkGameObjectID in_listener, AkReal32 in_fDelta, AkReal32 in_fNeutralVal = 1.0f, bool logNeutral = true) {}
	static inline void LogEarlyReflectionsVolume(AkGameObjectID in_emitter, AkReal32 in_fDelta) {}
	static inline void CloseEmitterListenerBrace() {}
	static inline void LogGameObjectDestroyed(AkGameObjectID in_gameObjectID, bool in_isListener) {}
	template<typename DRIVER_VAL>
	static inline void LogDriver(AkUniqueID in_idDriver, DRIVER_VAL in_fDriverVal) {}
	static inline void LogBaseValue(AkUInt32 in_idSource, AkPropID in_PropID, AkReal32 in_fDelta) {}
	static inline void LogState(AkUniqueID in_ulStateGroup, AkRTPC_ParameterID in_PropID, AkUniqueID in_ulActualState, AkReal32 in_fValue) {}
	static inline void LogRTPC(AkRtpcID in_RTPC, AkRTPC_ParameterID in_PropID, AkReal32 in_fRTPCVal, AkReal32 in_fValue, AkDeltaType in_eReason) {}
	static inline void LogPluginRTPC(AkRtpcID in_RTPC, AkReal32 in_fRTPCVal, AkDeltaType in_eReason) {}
	static inline void LogUpdate(AkPropID in_PropID, AkReal32 in_fValue) {}
	static inline void LogUpdate(AkPropID in_PropID, AkReal32 in_fValue, AkUniqueID in_idSound, AkPipelineID in_pipelineID) {}
	static inline void LogDelta(AkDeltaType in_eReason, AkPropID in_PropID, AkReal32 in_fDelta, const AkReal32 in_fNeutralVal = 0.0f) {}
	static inline void StartAuxGameDefVolumes(AkUInt32 in_nbSends) {}
	static inline void LogAuxGameDefVolume(AkUniqueID in_busID, AkGameObjectID in_listenerID, AkReal32 in_fGain) {}
	static inline void StartRoomSendVolumes() {}
	static inline void LogRoomSendVolume(AkUniqueID in_busID, AkGameObjectID in_listenerID, AkReal32 in_fGain) {}
	static inline void LogUsePrecomputedParams(AkUniqueID in_idParent) {}
	static inline bool LogVolumeRays(AkUInt8 in_count) { return false; }
	static inline bool LogFilterRays(AkUInt8 in_count) { return false; }
	static inline void LogAkProp(AkPropID in_prop, AkReal32 in_fValue, AkReal32 in_fDelta) {}
	static inline void LogAttenuation(AkReal32 in_fVal, AkUInt8 in_eType, AkUInt8* AK_RESTRICT in_pCurrentFlags, AkUInt8 &io_RayHistory, AkReal32 in_fNeutral) {}
	static inline AkUInt8* StartVolumeRay(AkUInt8 in_uCount, int in_rayIndex, AkGameObjectID in_idListener) { return NULL; }
	static inline AkUInt8* StartFilteringRay(AkUInt8 in_uCount, int in_rayIndex) { return NULL; }
	static inline void FinalizeRay(AkUInt8* AK_RESTRICT in_pFlags) {}
	static inline void LogModulator(AkModulatorID in_mod, AkRTPC_ParameterID in_PropID, AkReal32 in_fValue, AkReal32 in_fModVal, AkUniqueID in_idSource, AkDeltaType in_eReason) {}
	static inline void LogGameObjectOutputBusVolume(AkReal32 in_fGain) {}
	static inline void LogStateUnset(CAkParameterNodeBase* in_pNode, AkUniqueID in_uStateGroup) {}
	template<class T>
	static inline void Put(const T& val) {}
};

#ifdef AK_DELTAMONITOR_RELEASE
#include <stdio.h>

#define AK_DELTAMONITOR_BUFFER 8000
class AkDeltaMonitorPrint
{
public:
	static AKRESULT Init(char* in_fFile)
	{
		m_pFile = fopen(in_fFile, "w+");
		return AK_Success;
	}

	static void Term()
	{
		if (m_pFile)
		{
			fflush(m_pFile);
			fclose(m_pFile);
		}
	}

	static inline void Append(const char* in_fmt, ...)
	{
		if (!m_pFile)
			return;

		va_list args;
		va_start(args, in_fmt);
		size_t len = vsnprintf(NULL, 0, in_fmt, args);
		va_end(args);
		if (m_uUsed + len + 1 < AK_DELTAMONITOR_BUFFER)
		{
			va_start(args, in_fmt);
			vsnprintf(m_TmpBuffer+m_uUsed, len + 1, in_fmt, args);
			va_end(args);
			m_uUsed += (AkUInt32)len;
		}		
	}

	static inline void OpenObjectBrace(AkUniqueID in_idObj) 
	{
		Append( "OpenObjectBrace ID= %u\n", in_idObj);
	}

	static inline void CloseObjectBrace()
	{
		Append( "CloseObjectBrace\n");
	}

	static inline void OpenSoundBrace(AkUniqueID in_idSound, AkPipelineID in_pipelineID)
	{
		Append( "OpenSoundBrace ID= %u\tInstance= %u ------------------------\n", in_idSound, in_pipelineID);
	}

	static inline bool CloseSoundBrace()
	{
		if (m_bRealData)
		{					
			fprintf(m_pFile, m_TmpBuffer);			
			fprintf(m_pFile, "CloseSoundBrace ------------------------\n\n");
		}
		m_uUsed = 0;
		m_bRealData = false;
		return true;
	}

	static inline void OpenPrecomputedParamsBrace(AkUniqueID in_idObj) 		
	{
		Append( "OpenPrecomputedParamsBrace ID= %u\n", in_idObj);
	}

	static inline void ClosePrecomputedParamsBrace()
	{
		Append( "ClosePrecomputedParamsBrace\n");
	}

	static inline void OpenUpdateBrace(AkDeltaType in_eReason, AkUniqueID in_idSource = AK_INVALID_UNIQUE_ID, AkDeltaType in_updateType = AkDelta_MultiUpdate)
	{
		Append( "OpenUpdateBrace(%s) %s\tID= %u\n", AkDeltaReasons[in_updateType], AkDeltaReasons[in_eReason], in_idSource);
	}

	static inline bool IsUpdateBraceOpen()
	{
		Append("IsUpdateBraceOpen");
		return false;
	}

	static inline void CloseUpdateBrace(AkUniqueID in_idSource = AK_INVALID_UNIQUE_ID)	
	{
		Append( "CloseUpdateBrace ID= %u\n\n", in_idSource);
		if (m_bRealData)
		{
			fprintf(m_pFile, m_TmpBuffer);			
		}
		m_uUsed = 0;
		m_bRealData = false;
	}

	template<typename DRIVER_VAL>
	static inline void OpenMultiTargetBrace(AkDeltaType in_eReason, AkUniqueID in_idDriver, DRIVER_VAL in_fDriverVal)
	{
		Append( "OpenMultiTargetBrace(%s) Driver= %u\tValue=???\n", AkDeltaReasons[in_eReason], in_idDriver);
	}
	
	template<>
	static inline void OpenMultiTargetBrace(AkDeltaType in_eReason, AkUniqueID in_idDriver, AkReal32 in_fDriverVal)
	{
		Append( "OpenMultiTargetBrace(%s) Driver= %utValue= %f\n", AkDeltaReasons[in_eReason], in_idDriver, in_fDriverVal);
	}

	template<>
	static inline void OpenMultiTargetBrace(AkDeltaType in_eReason, AkUniqueID in_idDriver, AkUInt32 in_uDriverVal)
	{
		Append( "OpenMultiTargetBrace(%s) Driver= %utValue =%u\n", AkDeltaReasons[in_eReason], in_idDriver, in_uDriverVal);
	}

	static inline void CloseMultiTargetBrace()
	{
		Append( "CloseMultiTargetBrace\n\n");
		if (m_bRealData)
		{
			fprintf(m_pFile, m_TmpBuffer);			
		}
		m_uUsed = 0;
		m_bRealData = false;
	}

	static inline void OpenUnsetBrace(AkDeltaType in_eReason, AkUniqueID in_idSource, AkPropID in_propID)
	{
		Append( "OpenUnsetBrace %s\tID= %u\tProperty%u\n", AkDeltaReasons[in_eReason], in_idSource, in_propID);
	}

	static inline void LogPipelineID(AkPipelineID in_pipelineID)
	{
		Append( "LogPipelineID Instance= %u\n", in_pipelineID);
		m_bRealData = true;
	}

	static inline void CloseUnsetBrace(AkUniqueID in_idSource)
	{
		Append( "CloseUnsetBrace ID= %u\n", in_idSource);
	}

	static inline void OpenRTPCTargetBrace(AkUniqueID in_idSound, AkPipelineID in_pipelineID)
	{
		Append( "OpenRTPCTargetBrace ID= %u\tInstance= %u\n", in_idSound, in_pipelineID);
	}

	static inline void CloseRTPCTargetBrace()
	{
		Append( "CloseRTPCTargetBrace\n\n");
		if (m_bRealData)
		{
			fprintf(m_pFile, m_TmpBuffer);		
		}
		m_uUsed = 0;
		m_bRealData = false;
	}

	static inline void OpenEmitterListenerBrace(AkGameObjectID in_emitter, AkDeltaType in_eReason, AkPropID in_PropID){}
	static inline void LogListenerDelta(AkGameObjectID in_listener, AkReal32 in_fDelta, AkReal32 in_fNeutralVal = 1.0f, bool logNeutral = true) {}
	static inline void CloseEmitterListenerBrace() {}
	static inline void LogGameObjectDestroyed(AkGameObjectID in_gameObjectID, bool in_isListener) {}

	template<typename DRIVER_VAL>
	static inline void LogDriver(AkUniqueID in_idDriver, DRIVER_VAL in_fDriverVal)
	{
		Append( "LogDriver ???\n");
		m_bRealData = true;
	}

	template<>
	static inline void LogDriver(AkUniqueID in_idDriver, AkReal32 in_fDriverVal)
	{
		Append( "LogDriver ID= %u\tVal= %f\n", in_idDriver, in_fDriverVal);
		m_bRealData = true;
	}
	template<>
	static inline void LogDriver(AkUniqueID in_idDriver, AkUInt32 in_uDriverVal)
	{
		Append( "LogDriver ID= %u\tVal= %u\n", in_idDriver, in_uDriverVal);
		m_bRealData = true;
	}

	static inline void LogBaseValue(AkUInt32 in_idSource, AkPropID in_PropID, AkReal32 in_fDelta)
	{
		Append( "LogBaseValue ID= %u\tProp= %u\tVal= %f\n", in_idSource, in_PropID, in_fDelta);
		m_bRealData = true;
	}

	static inline void LogState(AkUniqueID in_ulStateGroup, AkRTPC_ParameterID in_PropID, AkUniqueID in_ulActualState, AkReal32 in_fValue)
	{
		Append( "LogState Group= %u\tState= %u\tProp= %u\tVal= %f\n", in_ulStateGroup, in_ulActualState, in_PropID, in_fValue);
		m_bRealData = true;
	}

	static inline void LogRTPC(AkRtpcID in_RTPC, AkRTPC_ParameterID in_PropID, AkReal32 in_fRTPCVal, AkReal32 in_fValue, AkDeltaType in_eReason)
	{
		Append( "LogRTPC RTPC= %u\tValue= %f\tRTPCPropID= %u\tVal= %f\n", in_RTPC, in_fRTPCVal, in_PropID, in_fValue);
		m_bRealData = true;
	}

	static inline void LogPluginRTPC(AkRtpcID in_RTPC, AkReal32 in_fRTPCVal, AkDeltaType in_eReason)
	{
		Append( "LogPluginRTPC RTPC= %u\tValue= %f\n", in_RTPC, in_fRTPCVal);
		m_bRealData = true;
	}

	static inline void LogUpdate(AkPropID in_PropID, AkReal32 in_fValue)
	{
		Append( "LogUpdate Prop= %u\tVal= %f\n", in_PropID, in_fValue);
		m_bRealData = true;
	}

	static inline void LogUpdate(AkPropID in_PropID, AkReal32 in_fValue, AkUniqueID in_idSound, AkPipelineID in_pipelineID)
	{
		Append( "LogUpdate Prop= %u\tVal= %f\tID= %u\tInstance= %u\n", in_PropID, in_fValue, in_idSound, in_pipelineID);
		m_bRealData = true;
	}

	static inline void LogDelta(AkDeltaType in_eReason, AkPropID in_PropID, AkReal32 in_fDelta, const AkReal32 in_fNeutralVal = 0.0f)
	{
		if (in_fNeutralVal != in_fDelta)
		{
			Append("LogDelta %s\tProp= %u\tVal= %f\n", AkDeltaReasons[in_eReason], in_PropID, in_fDelta);
			m_bRealData = true;
		}
	}

	static inline void StartAuxGameDefVolumes(AkUInt32 in_nbSends) {}
	static inline void LogAuxGameDefVolume(AkUniqueID in_busID, AkGameObjectID in_listenerID, AkReal32 in_fGain) {}
	static inline void StartRoomSendVolumes() {}
	static inline void LogRoomSendVolume(AkUniqueID in_busID, AkGameObjectID in_listenerID, AkReal32 in_fGain) {}
	static inline void LogEarlyReflectionsVolume(AkGameObjectID in_emitter, AkReal32 in_fDelta) {}
	static inline void LogUsePrecomputedParams(AkUniqueID in_idParent) {}
	static inline bool LogVolumeRays(AkUInt8 in_count) { return false; }
	static inline bool LogFilterRays(AkUInt8 in_count) { return false; }
	static inline void LogAkProp(AkPropID in_prop, AkReal32 in_fValue, AkReal32 in_fDelta) {}
	static inline void LogAttenuation(AkReal32 in_fVal, AkUInt8 in_eType, AkUInt8* AK_RESTRICT in_pCurrentFlags, AkUInt8 &io_RayHistory, AkReal32 in_fNeutral) {}
	static inline AkUInt8* StartVolumeRay(AkUInt8 in_uCount, int in_rayIndex, AkGameObjectID in_idListener) { return NULL; }
	static inline AkUInt8* StartFilteringRay(AkUInt8 in_uCount, int in_rayIndex) { return NULL; }
	static inline void FinalizeRay(AkUInt8* AK_RESTRICT in_pFlags) {}
	static inline void LogModulator(AkModulatorID in_mod, AkRTPC_ParameterID in_PropID, AkReal32 in_fValue, AkReal32 in_fModVal, AkUniqueID in_idSource, AkDeltaType in_eReason) {}
	static inline void LogGameObjectOutputBusVolume(AkReal32 in_fGain) {}
	static inline void LogStateUnset(CAkParameterNodeBase* in_pNode, AkUniqueID in_uStateGroup) {}
	template<class T>
	static inline void Put(const T& val) {}

	static FILE* m_pFile;
	static char *AkDeltaMonitorPrint::AkDeltaReasons[];
	static char m_TmpBuffer[AK_DELTAMONITOR_BUFFER];
	static AkUInt32 m_uUsed;
	static bool m_bRealData;
};
#endif

#ifndef AK_OPTIMIZED
typedef AkDeltaMonitorEnabled AkDeltaMonitor;
#elif defined AK_DELTAMONITOR_RELEASE
typedef AkDeltaMonitorPrint AkDeltaMonitor;
#else
typedef AkDeltaMonitorDisabled AkDeltaMonitor;
#endif


struct AkDeltaMonitorObjBrace
{
	AkDeltaMonitorObjBrace(AkUniqueID in_idObject)
	{
#ifdef AK_DELTAMONITOR_ENABLED
		AkDeltaMonitor::OpenObjectBrace(in_idObject);
#endif
	}
	~AkDeltaMonitorObjBrace()
	{
#ifdef AK_DELTAMONITOR_ENABLED
		AkDeltaMonitor::CloseObjectBrace();
#endif
	}
};

struct AkDeltaMonitorUpdateBrace
{
	// Open an update brace only if it's not already open
	AkDeltaMonitorUpdateBrace(AkDeltaType in_eReason, AkUniqueID in_idSource = AK_INVALID_UNIQUE_ID)
		: m_idSource(in_idSource)
	{
#ifdef AK_DELTAMONITOR_ENABLED
		if (AkDeltaMonitor::IsUpdateBraceOpen() == false)
		{
			AkDeltaMonitor::OpenUpdateBrace(in_eReason, m_idSource);
			wasOpened = true;
		}
		else
		{
			AkDeltaMonitor::OpenObjectBrace(m_idSource);
		}
#endif
	}

	// Close the object brace only if it was opened in the constructor
	~AkDeltaMonitorUpdateBrace()
	{
#ifdef AK_DELTAMONITOR_ENABLED
		if (wasOpened)
			AkDeltaMonitor::CloseUpdateBrace(m_idSource);
		else
			AkDeltaMonitor::CloseObjectBrace();
#endif
	}

	AkUniqueID m_idSource;
	bool wasOpened = false;
};
