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
// AkEffectsMgr.h
//
// Windows implementation of the effects manager.
// The effects manager takes care of allocating, deallocating and 
// storing the effect instances. 
// The initial effect parameters are kept at the node level. They are
// modified in run-time through RTPC, but the renderer sees them as
// anonymous blocks of data.
// The effects manager lives as a singleton in the AudioLib and supports
// multithreading.
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_EFFECTS_MGR_H_
#define _AK_EFFECTS_MGR_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/Tools/Common/AkKeyArray.h>

namespace AK { class IAkPluginMemAlloc; }
struct AkPlatformInitSettings;
class CAkPBI;
class IAkSoftwareCodec;
class IAkGrainCodec;

//-----------------------------------------------------------------------------
// Name: Class CAkEffectsMgr
// Desc: Implementation of the effects manager.
//-----------------------------------------------------------------------------
class CAkEffectsMgr
{
public:

	// Effect record definitions.
	//-------------------------------------
	//
	struct EffectTypeRecord
	{
		AkCreatePluginCallback pCreateFunc;           // Plugin creation function address.
		AkCreateParamCallback pCreateParamFunc;       // Plugin parameter object creation function address.
		AkGetDeviceListCallback pGetDeviceListFunc;   // Plugin device enumeration function address
	};

	// Initialise/Terminate.
	static AKRESULT Init(AkOSChar* in_szPluginDLLPath);
	static AKRESULT Term();

	// Registration-time.
	// Source plug-ins plus effect plug-ins
	static AKRESULT RegisterPlugin(
		AkPluginType in_eType,
		AkUInt32 in_ulCompanyID,                    // Company identifier (as declared in plugin description XML)
		AkUInt32 in_ulPluginID,                     // Plugin identifier (as declared in plugin description XML)
		AkCreatePluginCallback in_pCreateFunc,       // Pointer to the effect's Create() function.
		AkCreateParamCallback in_pCreateParamFunc,    // Pointer to the effect param's Create() function.
		AkGetDeviceListCallback in_pGetDeviceListFunc = NULL // Pointer to the effect GetDeviceList() function.
		);

	// Codec plug-ins
	static AKRESULT RegisterCodec(
		AkUInt32 in_ulCompanyID,                        // Company identifier (as declared in XML)
		AkUInt32 in_ulPluginID,                         // Plugin identifier (as declared in XML)
		const AkCodecDescriptor &in_CodecDescriptor     // Codec descriptor
		);

	//
	// Run-time API.
	// 
	// Returns a pointer to the instance of an effect given the ID. Allocates it if needed.
	static AKRESULT AllocParams(
		AK::IAkPluginMemAlloc * in_pAllocator,
		AkPluginID in_EffectTypeID,				// Effect type ID.
		AK::IAkPluginParam * & out_pEffectParam	// Effect param instance.
		);

	// Creates an effect instance given the FX ID. 
	static AKRESULT Alloc(
		AkPluginID in_EffectTypeID,             // Effect type ID.
		AK::IAkPlugin* & out_pEffect,           // Effect instance.
		AkPluginInfo & out_pluginInfo
		);

	// Creates a codec (bank or file) instance given the Codec ID. 
	static IAkSoftwareCodec * AllocCodecSrc(
		CAkPBI * in_pCtx,						// Source context.
		AkUInt32 in_uSrcType,
		AkCodecID in_CodecID					// Codec type ID.
		);

	static IAkFileCodec * AllocFileCodec(
		AkCodecID in_CodecID
	);

	static void FreeFileCodec(
		AkCodecID in_CodecID,
		IAkFileCodec * in_pDecoder
	);

	static IAkGrainCodec * AllocGrainCodec(
		AkCodecID in_CodecID
	);
	static void FreeGrainCodec(
		IAkGrainCodec * in_pCodec
	);

	static EffectTypeRecord* GetEffectTypeRecord(AkPluginID in_idPlugin);
	static AKRESULT RegisterPluginList(AK::PluginRegistration * in_pList);

	inline static AkUInt32 GetMergedID(AkUInt32 in_type, AkUInt32 in_companyID, AkUInt32 in_PluginID)
	{
		return (in_type & AkPluginTypeMask)
			+ (in_companyID << 4)
			+ (in_PluginID << (4 + 12));
	}

	inline static bool IsPluginRegistered(AkPluginID in_plugin)
	{
		//Check all sub types.
		if (m_RegisteredFXList.Exists(in_plugin) != NULL)
			return true;

		if (m_RegisteredCodecList.Exists(in_plugin) != NULL)
			return true;

		return false;
	}

	static AK::Monitor::ErrorCode ValidatePluginInfo(AkUInt32 in_uPluginID, AkPluginType in_eType, const AkPluginInfo & in_info);

	static const AkOSChar* GetPluginDLLPath(){ return m_szPluginDLLPath; }

private:
    
    // Singleton functions.
    CAkEffectsMgr();                          // Constructor.
    // Destructor (made public to please the memory manager).

private:
    // Factory
	typedef CAkKeyArray<AkPluginID, EffectTypeRecord> FXList;
    static FXList m_RegisteredFXList;  // list keyed by the plugin unique type ID.

	typedef CAkKeyArray<AkCodecID, AkCodecDescriptor> CodecList;
    static CodecList m_RegisteredCodecList;  // list keyed by the codec unique type ID.
	
	// Path specifying DLLs location when using DLLs for plugins. In the sound engine, use this instead of g_settings.szPluginDLLPath
	static AkOSChar * m_szPluginDLLPath;
};

#endif //_AK_EFFECTS_MGR_H_
