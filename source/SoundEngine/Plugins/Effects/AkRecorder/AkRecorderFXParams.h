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

#include <AK/SoundEngine/Common/IAkPlugin.h>

// NOTE: not currently RTPCable due to lack of gain ramp
struct AkRecorderRTPCParams
{
	AkReal32		fCenter;
	AkReal32		fFront;
	AkReal32		fSurround;
	AkReal32		fRear;
	AkReal32		fLFE;

};

struct AkRecorderNonRTPCParams
{
	AkInt16			iFormat;
	AkInt16			iAmbisonicChannelOrdering;
	AkOSChar		szFilename[AK_MAX_PATH];
	bool			bDownmixToStereo;
	bool			bApplyDownstreamVolume;
};

// Structure of RecorderFX parameters
struct AkRecorderFXParams
{
	AkRecorderRTPCParams	RTPC;
	AkRecorderNonRTPCParams	NonRTPC;
} AK_ALIGN_DMA;

// Parameters IDs for the Wwise or RTPC.
static const AkPluginParamID AK_RECORDERFXPARAM_CENTER_ID	= 0;	
static const AkPluginParamID AK_RECORDERFXPARAM_REAR_ID	= 1;
static const AkPluginParamID AK_RECORDERFXPARAM_AUTHORINGFILENAME_ID = 2;
static const AkPluginParamID AK_RECORDERFXPARAM_DOWNMIXTOSTEREO_ID = 3;
static const AkPluginParamID AK_RECORDERFXPARAM_FRONT_ID = 4;
static const AkPluginParamID AK_RECORDERFXPARAM_SURROUND_ID = 5;
static const AkPluginParamID AK_RECORDERFXPARAM_LFE_ID = 6;
static const AkPluginParamID AK_RECORDERFXPARAM_FORMAT_ID = 7;
static const AkPluginParamID AK_RECORDERFXPARAM_APPLYDOWNSTREAMVOLUME_ID = 8;
static const AkPluginParamID AK_RECORDERFXPARAM_GAMEFILENAME_ID = 9;
static const AkPluginParamID AK_RECORDERFXPARAM_AMBCHANNELORDERING = 10;

// Default values in case a bad parameter block is provided
#define AK_RECORDER_CENTER_DEF					(-3.0f)
#define AK_RECORDER_FRONT_DEF					(0.0f)
#define AK_RECORDER_SURROUND_DEF				(-3.0f)
#define AK_RECORDER_REAR_DEF					(-3.0f)
#define AK_RECORDER_LFE_DEF						(-96.3f)
#define AK_RECORDER_DOWNMIXTOSTEREO_DEF			(true)
#define AK_RECORDER_FORMAT_DEF					(0)
#define AK_RECORDER_APPLYDOWNSTREAMVOLUME_DEF	(false)
#define AK_RECORDER_AMBCHANNELSORDERING_DEF		(1)

class CAkRecorderFXParams 
	: public AK::IAkPluginParam
	, public AkRecorderFXParams
{
public:

	friend class CAkRecorderFX;
    
    // Constructor/destructor.
    CAkRecorderFXParams( );
    ~CAkRecorderFXParams( );
	CAkRecorderFXParams( const CAkRecorderFXParams & in_rCopy );

    // Create duplicate.
    IAkPluginParam * Clone( AK::IAkPluginMemAlloc * in_pAllocator );

    // Init/Term.
    AKRESULT Init(	AK::IAkPluginMemAlloc *	in_pAllocator,						    
					const void *			in_pParamsBlock, 
					AkUInt32				in_ulBlockSize 
                         );
    AKRESULT Term( AK::IAkPluginMemAlloc * in_pAllocator );

    // Blob set.
    AKRESULT SetParamsBlock(	const void * in_pParamsBlock, 
								AkUInt32 in_ulBlockSize
                                );

    // Update one parameter.
    AKRESULT SetParam(	AkPluginParamID in_ParamID,
						const void * in_pValue, 
						AkUInt32 in_ulParamSize
                        );
};

