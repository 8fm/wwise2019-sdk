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
#pragma once

#include <AK/Tools/Common/AkAssert.h>
#include "AkParameters.h"
#include "AkBitArray.h"

#define MASK_FROM_PROPID(__PROP) (1 << (__PROP - (__PROP/32)*32))


#define DECLARE_SOUNDPARAM_MEMBER(__MEMBER)\
private: AkReal32 f##__MEMBER; public:\
AkForceInline const AkReal32 __MEMBER() const { return f##__MEMBER; } \
AkForceInline AkReal32 __MEMBER() { return f##__MEMBER; } \
AkForceInline void AddTo##__MEMBER(AkReal32 in_fVal, AkDeltaType in_eReason) { f##__MEMBER += in_fVal; AkDeltaMonitor::LogDelta(in_eReason, AkPropID_##__MEMBER, in_fVal); }\
AkForceInline void AddNoLog##__MEMBER(AkReal32 in_fVal) { f##__MEMBER += in_fVal; }\
AkForceInline void Set##__MEMBER(AkReal32 in_fVal, AkDeltaType in_eReason) { f##__MEMBER = in_fVal; AkDeltaMonitor::LogDelta(in_eReason, AkPropID_##__MEMBER, in_fVal); } \
AkForceInline void SetNoLog##__MEMBER(AkReal32 in_fVal) { f##__MEMBER = in_fVal; }

//AKSTATICASSERT(AK_NUM_USER_AUX_SEND_PER_OBJ == 4, "AK_NUM_USER_AUX_SEND_PER_OBJ is more than 4, extend AkSoundParams");

class AkSoundParams
{
#define AkSoundParams_PROPCOUNT 27
public:	
	AkSoundParams()
	{
		ClearEx();
	}
	
	DECLARE_SOUNDPARAM_MEMBER(Volume);
	DECLARE_SOUNDPARAM_MEMBER(Pitch);
	DECLARE_SOUNDPARAM_MEMBER(LPF);
	DECLARE_SOUNDPARAM_MEMBER(HPF);
	DECLARE_SOUNDPARAM_MEMBER(BusVolume);
	DECLARE_SOUNDPARAM_MEMBER(MakeUpGain);
	DECLARE_SOUNDPARAM_MEMBER(UserAuxSendVolume0);
	DECLARE_SOUNDPARAM_MEMBER(UserAuxSendVolume1);
	DECLARE_SOUNDPARAM_MEMBER(UserAuxSendVolume2);
	DECLARE_SOUNDPARAM_MEMBER(UserAuxSendVolume3);
	DECLARE_SOUNDPARAM_MEMBER(GameAuxSendVolume);
	DECLARE_SOUNDPARAM_MEMBER(OutputBusVolume);
	DECLARE_SOUNDPARAM_MEMBER(OutputBusHPF);
	DECLARE_SOUNDPARAM_MEMBER(OutputBusLPF);
	DECLARE_SOUNDPARAM_MEMBER(HDRActiveRange);
	DECLARE_SOUNDPARAM_MEMBER(UserAuxSendLPF0);
	DECLARE_SOUNDPARAM_MEMBER(UserAuxSendLPF1);
	DECLARE_SOUNDPARAM_MEMBER(UserAuxSendLPF2);
	DECLARE_SOUNDPARAM_MEMBER(UserAuxSendLPF3);
	DECLARE_SOUNDPARAM_MEMBER(UserAuxSendHPF0);
	DECLARE_SOUNDPARAM_MEMBER(UserAuxSendHPF1);
	DECLARE_SOUNDPARAM_MEMBER(UserAuxSendHPF2);
	DECLARE_SOUNDPARAM_MEMBER(UserAuxSendHPF3);
	DECLARE_SOUNDPARAM_MEMBER(GameAuxSendLPF);
	DECLARE_SOUNDPARAM_MEMBER(GameAuxSendHPF);
	DECLARE_SOUNDPARAM_MEMBER(ReflectionBusVolume);

	AkForceInline const AkReal32 *Params() { return &fVolume; }

	AkForceInline void Set(const AkPropID in_idProp, const AkReal32 in_fVal, const AkDeltaType in_eReason)
	{
		const AkUInt32 idx = s_PropToIdx[in_idProp];
		AKASSERT(idx != AkSoundParams_PROPCOUNT);
		(&fVolume)[idx] = in_fVal;
		AkDeltaMonitor::LogDelta(in_eReason, in_idProp, in_fVal);
	}

	AkForceInline void Accumulate(const AkPropID in_idProp, const AkReal32 in_fVal, const AkDeltaType in_eReason)
	{
		const AkUInt32 idx = s_PropToIdx[in_idProp];
		if (idx != AkSoundParams_PROPCOUNT)
		{
			(&fVolume)[idx] += in_fVal;
			if (in_eReason != AkDelta_None)	//Normally optimized away
				AkDeltaMonitor::LogDelta(in_eReason, in_idProp, in_fVal);
		}
	}

	AkForceInline const AkReal32 GetUserAuxSendVolume(const AkUInt32 i) { return Params()[i + s_PropToIdx[AkPropID_UserAuxSendVolume0]]; }
	AkForceInline const AkReal32 GetUserAuxSendLPF(const AkUInt32 i) { return Params()[i + s_PropToIdx[AkPropID_UserAuxSendLPF0]]; }
	AkForceInline const AkReal32 GetUserAuxSendHPF(const AkUInt32 i) { return Params()[i + s_PropToIdx[AkPropID_UserAuxSendHPF0]]; }

	void ResetUserAuxValues()
	{
		for (AkUInt32 i = 0; i < AK_NUM_USER_AUX_SEND_PER_OBJ; i++)
		{
			(&fUserAuxSendVolume0)[i] = 0.f;
			(&fUserAuxSendLPF0)[i] = 0.f;
			(&fUserAuxSendHPF0)[i] = 0.f;
		}
	}

	class PropertyBitArray : public CAkLargeBitArray<AkPropID_NUM>
	{
	public:	
		AkForceInline void EnableDefaultProps()
		{
			m_iBitArray[0] = MASK_FROM_PROPID(AkPropID_Volume) |
				MASK_FROM_PROPID(AkPropID_Pitch) |
				MASK_FROM_PROPID(AkPropID_LPF) |
				MASK_FROM_PROPID(AkPropID_HPF) |
				MASK_FROM_PROPID(AkPropID_MakeUpGain);

			m_iBitArray[1] = MASK_FROM_PROPID(AkPropID_HDRActiveRange);
			m_iBitArray[2] = 0;
		}
		
		AkForceInline void AddUserAuxProps()
		{
			m_iBitArray[0] |= (0xF << (AkPropID_UserAuxSendVolume0 - 0));
			m_iBitArray[1] |= (0xF << (AkPropID_UserAuxSendLPF0 - 32));
			m_iBitArray[2] |= (0xF << (AkPropID_UserAuxSendHPF0 - 64));
		}

		AkForceInline void RemoveUserAuxProps()
		{
			m_iBitArray[0] &= ~(0xF << (AkPropID_UserAuxSendVolume0 - 0));
			m_iBitArray[1] &= ~(0xF << (AkPropID_UserAuxSendLPF0 - 32));
			m_iBitArray[2] &= ~(0xF << (AkPropID_UserAuxSendHPF0 - 64));
		}

		AkForceInline void AddGameDefProps()
		{
			m_iBitArray[0] |= MASK_FROM_PROPID(AkPropID_GameAuxSendVolume);
			m_iBitArray[2] |= MASK_FROM_PROPID(AkPropID_GameAuxSendLPF) | MASK_FROM_PROPID(AkPropID_GameAuxSendHPF);
		}

		AkForceInline void RemoveGameDefProps()
		{
			m_iBitArray[0] &= ~MASK_FROM_PROPID(AkPropID_GameAuxSendVolume);
			m_iBitArray[2] &= ~(MASK_FROM_PROPID(AkPropID_GameAuxSendLPF) | MASK_FROM_PROPID(AkPropID_GameAuxSendHPF));
		}

		AkForceInline void AddSpatialAudioProps()
		{
			m_iBitArray[2] |= MASK_FROM_PROPID(AkPropID_ReflectionBusVolume);
		}

		AkForceInline void RemoveSpatialAudioProps()
		{
			m_iBitArray[2] &= ~MASK_FROM_PROPID(AkPropID_ReflectionBusVolume);
		}

		AkForceInline void AddOutputBusProps()
		{
			m_iBitArray[0] |= MASK_FROM_PROPID(AkPropID_OutputBusVolume) |
				MASK_FROM_PROPID(AkPropID_OutputBusHPF) |
				MASK_FROM_PROPID(AkPropID_OutputBusLPF);
		}

		AkForceInline void RemoveServicedProps()
		{
			m_iBitArray[0] &= ~((0xF << (AkPropID_UserAuxSendVolume0 - 0)) |
				MASK_FROM_PROPID(AkPropID_OutputBusVolume) |
				MASK_FROM_PROPID(AkPropID_OutputBusLPF) |
				MASK_FROM_PROPID(AkPropID_OutputBusHPF) |
				MASK_FROM_PROPID(AkPropID_GameAuxSendVolume));

			m_iBitArray[1] &= ~((0xF << (AkPropID_UserAuxSendLPF0 - 32)));
			m_iBitArray[2] &= ~((0xF << (AkPropID_UserAuxSendHPF0 - 64)) |
				MASK_FROM_PROPID(AkPropID_GameAuxSendLPF) |
				MASK_FROM_PROPID(AkPropID_GameAuxSendHPF) |
				MASK_FROM_PROPID(AkPropID_ReflectionBusVolume)
				);
		}
	};	

	AkReal32 fFadeRatio;		// [0,1]
	AkUniqueID aAuxSend[AK_NUM_USER_AUX_SEND_PER_OBJ];	
	AkUniqueID reflectionsAuxBus;

	PropertyBitArray Request;
	
	AkInt16 iBypassAllFX;
	AkUInt8 bGameDefinedAuxEnabled : 1;
	AkUInt8 bGameDefinedServiced : 1;
	AkUInt8 bUserDefinedServiced : 1;
	AkUInt8 bOutputBusParamServiced : 1;
	AkUInt8 bSpatialAudioServiced : 1;

	AkUInt8 /*bool*/	bNormalizeLoudness : 1;
	AkUInt8				bNormalizationServiced : 1;		// Autonormalization is exclusive/overridable.
	AkUInt8				bHdrServiced : 1;
	AkUInt8 			bEnableEnvelope : 1;
	AkUInt8				bBypassServiced : 1;	

	void ClearEx()
	{
		//Init all properties to 0.
		memset(&fVolume, 0, sizeof(fVolume)*AkSoundParams_PROPCOUNT);

		ResetServicedFlags();
		for (int i = 0; i < AK_NUM_USER_AUX_SEND_PER_OBJ; ++i)
		{
			aAuxSend[i] = AK_INVALID_UNIQUE_ID;
		}

		reflectionsAuxBus = AK_INVALID_UNIQUE_ID;

		iBypassAllFX = 0;		
		bEnableEnvelope = false;
		bNormalizeLoudness = false;			
		fFadeRatio = AK_UNMUTED_RATIO;
		bGameDefinedAuxEnabled = false;
	}

	void ResetServicedFlags()
	{
		bGameDefinedServiced = false;
		bUserDefinedServiced = false;
		bOutputBusParamServiced = false;
		bNormalizationServiced = false;
		bHdrServiced = false;
		bBypassServiced = false;
		bSpatialAudioServiced = false;
	}

	const AkSoundParams& operator=(const AkSoundParams &in_rCopy)
	{
		memcpy(this, &in_rCopy, sizeof(AkSoundParams));
		return *this;
	}

	static const AkUInt32 s_PropToIdx[AkPropID_NUM];
};
