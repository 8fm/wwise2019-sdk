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
// AkFDNReverbFXParams.h
//
// Shared parameter implementation for FDN reverb FX.
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_FDNREVERBFXPARAMS_H_
#define _AK_FDNREVERBFXPARAMS_H_

#include <AK/SoundEngine/Common/AkTypes.h>

#define MAXNUMDELAYS (16)

/// Delay length mode
enum AkDelayLengthsMode
{
	AKDELAYLENGTHSMODE_DEFAULT = 0,	///< Default settings
	AKDELAYLENGTHSMODE_CUSTOM = 1	///< Custom settings
};

/// Delay times used when in default mode
static const float g_fDefaultDelayLengths[16] = { 13.62f, 15.66f, 17.52f, 19.02f, 20.83f, 22.60f, 24.05f, 24.78f, 25.60f, 26.09f, 26.55f, 26.91f, 28.04f, 29.09f, 29.90f, 30.86f };

// Structure of parameters that remain true for the whole lifespan of the tone generator.

struct AkFDNReverbRTPCParams
{
	AkReal32	fReverbTime;
	AkReal32	fHFRatio;
	AkReal32	fDryLevel;
	AkReal32	fWetLevel;
};

struct AkFDNReverbNonRTPCParams
{
	AkUInt32	uNumberOfDelays;
	AkReal32	fPreDelay;
	AkUInt32	uProcessLFE;
	AkUInt32	uDelayLengthsMode;
	AkReal32	fDelayTime[MAXNUMDELAYS];
	bool		bDirty;
};

struct AkFDNReverbFXParams
{
	AkFDNReverbRTPCParams RTPC;
	AkFDNReverbNonRTPCParams NonRTPC;
} AK_ALIGN_DMA;

#include <AK/Tools/Common/AkAssert.h>
#include <math.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>

// Parameters IDs for Wwise or RTPC.
static const AkPluginParamID AK_FDNREVERBFXPARAM_REVERBTIME_ID				= 0;	// RTPC
static const AkPluginParamID AK_FDNREVERBFXPARAM_HFRATIO_ID					= 1;	// RTPC
static const AkPluginParamID AK_FDNREVERBFXPARAM_NUMBEROFDELAYS_ID			= 2;	
static const AkPluginParamID AK_FDNREVERBFXPARAM_DRYLEVEL_ID				= 3;	// RTPC
static const AkPluginParamID AK_FDNREVERBFXPARAM_WETLEVEL_ID				= 4;	// RTPC
static const AkPluginParamID AK_FDNREVERBFXPARAM_PREDELAY_ID				= 5;	
static const AkPluginParamID AK_FDNREVERBFXPARAM_PROCESSLFE_ID				= 6;	
static const AkPluginParamID AK_FDNREVERBFXPARAM_DELAYLENGTHSMODE_ID		= 7;
static const AkPluginParamID AK_FDNREVERBFXPARAM_FIRSTDELAYTIME_ID			= 8;
static const AkPluginParamID AK_FDNREVERBFXPARAM_LASTDELAYTIME_ID			= 23;

// Default values in case a bad parameter block is provided
#define AK_FDNREVERB_REVERBTIME_DEF				(4.f)
#define AK_FDNREVERB_HFRATIO_DEF				(2.f)
#define AK_FDNREVERB_NUMBEROFDELAYS_DEF			(8)
#define AK_FDNREVERB_DRYLEVEL_DEF				(-3.f)
#define AK_FDNREVERB_WETLEVEL_DEF				(-10.f)
#define AK_FDNREVERB_PREDELAY_DEF				(0.f)
#define AK_FDNREVERB_PROCESSLFE_DEF				(1)
#define AK_FDNREVERB_DELAYLENGTHSMODE_DEF		(AKDELAYLENGTHSMODE_DEFAULT)

//-----------------------------------------------------------------------------
// Name: class CAkFDNReverbFXParams
// Desc: Shared FDN reverb FX parameters implementation.
//-----------------------------------------------------------------------------
class CAkFDNReverbFXParams 
	: public AK::IAkPluginParam
	, public AkFDNReverbFXParams
{
public:

    // Constructor/destructor.
    CAkFDNReverbFXParams( );
    ~CAkFDNReverbFXParams();
	CAkFDNReverbFXParams( const CAkFDNReverbFXParams & in_rCopy );

    // Create duplicate.
    IAkPluginParam * Clone( AK::IAkPluginMemAlloc * in_pAllocator );

    // Init/Term.
    AKRESULT Init( AK::IAkPluginMemAlloc *	in_pAllocator,						    
                   const void *				in_pParamsBlock, 
                   AkUInt32					in_ulBlockSize 
                   );
    AKRESULT Term( AK::IAkPluginMemAlloc * in_pAllocator );

    // Blob set.
    AKRESULT SetParamsBlock( const void * in_pParamsBlock, 
                             AkUInt32 in_ulBlockSize
                             );

    // Update one parameter.
    AKRESULT SetParam(	AkPluginParamID in_ParamID,
                        const void * in_pValue, 
                        AkUInt32 in_ulParamSize
                        );
};

#endif // _AK_FDNREVERBFXPARAMS_H_
