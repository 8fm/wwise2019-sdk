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
// AkCommon.h
//
// AudioLib common defines, enums, and structs.
//
//////////////////////////////////////////////////////////////////////

#ifndef _COMMON_H_
#define _COMMON_H_

// Additional "AkCommon" definition made public.
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/Tools/Common/AkArray.h>

#include "AkRTPC.h"

#define AK_UNREFERENCED_PARAMETER( _x ) (void(_x))

//-----------------------------------------------------------------------------
// Name: struct AkModulatorXfrm
// Desc: Defines the transform to be applied to a automation curve
//-----------------------------------------------------------------------------
struct AkModulatorXfrm
{
	AkModulatorXfrm(): m_fOffset(0.0f), m_fScale(1.0f) {}

	AkReal32 m_fOffset;
	AkReal32 m_fScale;
};

struct AkModulatorParamXfrm : public AkModulatorXfrm
{
	AkModulatorParamXfrm(): m_rtpcParamID( RTPC_MaxNumRTPC ) {}

	AkRTPC_ParameterID m_rtpcParamID;
};

typedef AkArray< AkModulatorParamXfrm, const AkModulatorParamXfrm& > AkModulatorParamXfrmArray;

//-----------------------------------------------------------------------------
// Name: struct AkModulatorGain
// Desc: Defines the interpolated gain to be applied to an automation curve
//-----------------------------------------------------------------------------
struct AkModulatorGain
{
	AkModulatorGain(): m_fStart( 0.f ), m_fEnd(0.0f) {}
	AkModulatorGain( AkReal32 in_fGain ): m_fStart( in_fGain ), m_fEnd( in_fGain ) {}

	AkReal32 m_fStart;
	AkReal32 m_fEnd;
};

//-----------------------------------------------------------------------------
// Name: struct AkBufferPosInformation
// Desc: Defines the position info for a source used by GetSourcePlayPosition()
//-----------------------------------------------------------------------------
struct AkBufferPosInformation
{
	AkUInt32	uStartPos;		//start position for data contained in the buffer
	AkReal32	fLastRate;		//last known pitch rate
	AkUInt32	uFileEnd;		//file end position
	AkUInt32	uSampleRate;	//file sample rate
	
	inline void Clear()
	{
		uStartPos	= 0xFFFFFFFF;
		fLastRate	= 1.0f;
		uFileEnd	= 0xFFFFFFFF;
		uSampleRate	= 1;
	}

	inline void Invalidate()
	{
		uStartPos = (AkUInt32)-1;
	}

	inline bool IsValid()
	{
		return ( uStartPos != (AkUInt32)-1 );
	}
};

struct AkBufferingInformation
{
	AkTimeMs	uBuffering;		//Buffering amount (in milliseconds).
	AKRESULT	eBufferingState;//Buffering state (AK_Success, AK_NoMoreData, AK_Fail).

	inline void Clear()
	{
		uBuffering	= 0;
		eBufferingState = AK_Success;
	}
};

//-----------------------------------------------------------------------------
// Name: class AkPipelineBufferBase
//-----------------------------------------------------------------------------
class AkPipelineBufferBase : public AkAudioBuffer
{
public:
	static void InitFreeListBuckets();
	static void ClearFreeListBuckets();

	// Buffer management.
	// ----------------------------

	// Logical.
	inline void SetRequestSize( AkUInt16 in_uMaxFrames )
	{
		AKASSERT( !pData || uMaxFrames == 0 || uMaxFrames == in_uMaxFrames );
		uMaxFrames = in_uMaxFrames;
	}

	// Detach data from buffer, after it has been freed from managing instance.
	inline void DetachData()
	{
		AKASSERT( pData && uMaxFrames > 0 );
		pData = NULL;
		uMaxFrames = 0;
		uValidFrames = 0;
	}

	AkForceInline void * GetContiguousDeinterleavedData()
	{
		return pData;
	}

	// Deinterleaved (pipeline API). Allocation is performed inside.
	AKRESULT GetCachedBuffer();
	void ReleaseCachedBuffer();

	AkForceInline void SetChannelConfig( AkChannelConfig in_channelConfig )
	{
		AKASSERT( !pData || channelConfig.uNumChannels == 0 || channelConfig.uNumChannels == in_channelConfig.uNumChannels );
		channelConfig = in_channelConfig;
	}
};

class AkPipelineBuffer
	: public AkPipelineBufferBase
{
public:
	AkPipelineBuffer()
		: uPendingMarkerIndex(0)
		, uPendingMarkerLength(0)
	{}

	inline void ResetMarkers()
	{
		uPendingMarkerIndex = 0;
		uPendingMarkerLength = 0;
	}

	AkForceInline void ClearPosInfo()
	{
		posInfo.Clear();
	}

	inline void Clear()
	{
		AkPipelineBufferBase::Clear();
		ResetMarkers();
		posInfo.Clear();
	}

	AkUInt32 uPendingMarkerIndex;       // Index of the first pending marker for this buffer in the queue
	AkUInt32 uPendingMarkerLength;      // How many markers associated with this buffer in the queue
	AkBufferPosInformation posInfo;     // Position information for GetSourcePlayPosition
};

namespace AkMonitorData
{
	enum BusMeterDataType
	{
		BusMeterDataType_Peak		   = AK_EnableBusMeter_Peak,
		BusMeterDataType_TruePeak	   = AK_EnableBusMeter_TruePeak,
		BusMeterDataType_RMS		   = AK_EnableBusMeter_RMS,
		BusMeterDataType_HdrPeak	   = 1 << 3,
		BusMeterDataType_KPower		   = AK_EnableBusMeter_KPower,
        BusMeterDataType_3DMeter       = AK_EnableBusMeter_3DMeter,
		
        BusMeterMask_RequireContext    = ~BusMeterDataType_HdrPeak
	};
}

struct AkAuxSendValueEx: public AkAuxSendValue
{
	AkConnectionType eAuxType;
	AkReal32 fLPFValue;			///< Value in the range [0.0f:1.0f], LPF parameter to auxiliary bus.
	AkReal32 fHPFValue;			///< Value in the range [0.0f:1.0f], HPF parameter to auxiliary bus.
};

typedef AkArray<AkAuxSendValueEx, const AkAuxSendValueEx&> AkAuxSendArray;

//-----------------------------------------------------------------------------
// Looping constants.
//-----------------------------------------------------------------------------
const AkInt16 LOOPING_INFINITE  = 0;
const AkInt16 LOOPING_ONE_SHOT	= 1;

#define IS_LOOPING( in_Value ) ( in_Value == LOOPING_INFINITE || in_Value > LOOPING_ONE_SHOT )

AkUInt8 AkPipelineChannelMappingFunc(const AkChannelConfig &config, AkUInt8 idx);
AkUInt8 AkWaveChannelMappingFunc(const AkChannelConfig &config, AkUInt8 idx);
AkUInt8 AkVorbisChannelMappingFunc(const AkChannelConfig &config, AkUInt8 idx);
#endif // _COMMON_H_
