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
// AkEffectsMgr.cpp
//
// Implementation of the effects manager.
// The effects manager takes care of allocating, deallocating and 
// storing the effect instances. 
// The initial effect parameters are kept at the node level. They are
// modified in run-time through RTPC, but the renderer sees them as
// anonymous blocks of data.
// The effects manager lives as a singleton in the AudioLib.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkEffectsMgr.h"
#include "AkFXMemAlloc.h"
#include "AkPBI.h"
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include "AkPluginCodec.h"

//-----------------------------------------------------------------------------
// Variables.
//-----------------------------------------------------------------------------

CAkEffectsMgr::FXList CAkEffectsMgr::m_RegisteredFXList;  // list keyed by the plugin unique type ID.
CAkEffectsMgr::CodecList CAkEffectsMgr::m_RegisteredCodecList;  // list keyed by the codec unique type ID.
AkOSChar * CAkEffectsMgr::m_szPluginDLLPath = NULL; // Path specifying DLLs location when using DLLs for plugins. Use this instead of g_settings.szPluginDLLPath

AK_DLLEXPORT AK::PluginRegistration * g_pAKPluginList = NULL;	//This is the list of plugins statically linked with this link unit.

#include "AkSrcMediaCodecPCM.h"

IAkSrcMediaCodec* CreateSrcMediaCodecPCM(const AK::SrcMedia::Header* in_pHeader)
{
	return AkNew(AkMemID_Processing, CAkSrcMediaCodecPCM());
}

IAkSoftwareCodec* CreateSrcMediaNodePCM(void* in_pCtx)
{
	return AkNew(AkMemID_Processing, CAkSrcMedia((CAkPBI*)in_pCtx, CreateSrcMediaCodecPCM));
}

AkCodecDescriptor g_AkCodecPCM{
	CreateSrcMediaNodePCM,
	CreateSrcMediaNodePCM,
	nullptr,
	nullptr
};

//-----------------------------------------------------------------------------
// Name: Init
// Desc: Initialise lists.
//-----------------------------------------------------------------------------
AKRESULT CAkEffectsMgr::Init(AkOSChar* in_szPluginDLLPath)
{
	AKRESULT eResult = AK_Success;
	if (in_szPluginDLLPath != NULL)
	{
		//Take our own copy.  Add one for the 0 and one for the potentially missing slash.
		AkUInt32 len = (AkUInt32)OsStrLen(in_szPluginDLLPath) + 2;

		// WG-37065 - Store the copy of the DLL path into the CAkEffectsMgr. 
		// Do not use g_settings.szPluginDLLPath direclty. Instead, use CAkEffectsMgr::szPluginDLLPath.
		AKASSERT(m_szPluginDLLPath == NULL);
		m_szPluginDLLPath = (AkOSChar *)AkAlloc(AkMemID_Object, len * sizeof(AkOSChar));
		if (m_szPluginDLLPath == NULL)
		{
			eResult = AK_InsufficientMemory;
		}
		else
		{
			SafeStrCpy(m_szPluginDLLPath, in_szPluginDLLPath, len - 1);
			if (m_szPluginDLLPath[len - 3] != AK_PATH_SEPARATOR[0])
			{
				m_szPluginDLLPath[len - 2] = AK_PATH_SEPARATOR[0];
				m_szPluginDLLPath[len - 1] = 0;
			}
		}
	}

	if (eResult == AK_Success)
	{
		// These plug-ins are always present no matter what.
		AKVERIFY(AK_Success == RegisterCodec(AKCOMPANYID_AUDIOKINETIC, AKCODECID_PCM, g_AkCodecPCM));

		eResult = RegisterPluginList(g_pAKPluginList);
	}

	return eResult;
}

AKRESULT CAkEffectsMgr::RegisterPluginList(AK::PluginRegistration * in_pList)
{    
	if (in_pList == NULL)
		return AK_Success;

	//Go through the static registration list.		
	//None of those are critical failures.
	AK::PluginRegistration * pCurrent = in_pList;
	while (pCurrent != NULL)
	{
		AKRESULT eResult;
		if (pCurrent->m_eType == AkPluginTypeCodec)
			eResult = RegisterCodec(pCurrent->m_ulCompanyID, pCurrent->m_ulPluginID, pCurrent->m_CodecDescriptor);
		else
			eResult = RegisterPlugin(pCurrent->m_eType, pCurrent->m_ulCompanyID, pCurrent->m_ulPluginID, pCurrent->m_pCreateFunc, pCurrent->m_pCreateParamFunc, pCurrent->m_pGetDeviceListFunc);

		if (eResult == AK_Success
			&& pCurrent->m_pRegisterCallback)
		{
			pCurrent->m_pRegisterCallback(AK::SoundEngine::GetGlobalPluginContext(), AkGlobalCallbackLocation_Register, pCurrent->m_pRegisterCallbackCookie);
		}

		pCurrent = pCurrent->pNext;
	}
    return AK_Success;
}

//-----------------------------------------------------------------------------
// Name: Term
// Desc: Terminate device manager.
//-----------------------------------------------------------------------------
AKRESULT CAkEffectsMgr::Term( )
{
    // Terminate registered FX list.
    m_RegisteredFXList.Term();
	m_RegisteredCodecList.Term();

	if (m_szPluginDLLPath)
	{
		AkFree(AkMemID_Object, m_szPluginDLLPath);
		m_szPluginDLLPath = NULL;
	}

    return AK_Success;
}

// Registration-time

//-----------------------------------------------------------------------------
// Name: RegisterPlugin
// Desc: Registers an effect with its unique ID. Derived CAkRTEffect classes
//       must register themselves in order to be used as plugins.
//       The effect type ID must be unique, otherwise this function will fail.
// Note: This function is NOT thread-safe.
//-----------------------------------------------------------------------------
AKRESULT CAkEffectsMgr::RegisterPlugin( 
	AkPluginType in_eType,
	AkUInt32 in_ulCompanyID,						// Company identifier (as declared in plugin description XML)
	AkUInt32 in_ulPluginID,							// Plugin identifier (as declared in plugin description XML)
    AkCreatePluginCallback	in_pCreateFunc,			// Pointer to the effect's Create() function.
    AkCreateParamCallback	in_pCreateParamFunc,	// Pointer to the effect param's Create() function.
	AkGetDeviceListCallback in_pGetDeviceListFunc	// Pointer to the effect GetDeviceList() function.
    )
{
	AKASSERT( in_eType );
	AkUInt32 ulPluginID = GetMergedID(in_eType, in_ulCompanyID, in_ulPluginID);

	// Verify that the Effect type ID doesn't exist.
    if ( m_RegisteredFXList.Exists( ulPluginID ) )
    {
        // An effect is already registered with this ID.
		return AK_Success;
    }    

    EffectTypeRecord NewTypeRecord = { in_pCreateFunc, in_pCreateParamFunc, in_pGetDeviceListFunc };
    
	return m_RegisteredFXList.Set( ulPluginID, NewTypeRecord ) ? AK_Success : AK_Fail;
}

//-----------------------------------------------------------------------------
// Name: RegisterCodec
// Desc: Registers a codec with its unique ID. 
// Note: This function is NOT thread-safe.
//-----------------------------------------------------------------------------
AKRESULT CAkEffectsMgr::RegisterCodec(AkUInt32 in_ulCompanyID, AkUInt32 in_ulPluginID, const AkCodecDescriptor & in_CodecDescriptor)
{
    AkUInt32 ulPluginID = GetMergedID( AkPluginTypeCodec, in_ulCompanyID, in_ulPluginID );

	// Verify that the Effect type ID doesn't exist.
    if ( m_RegisteredCodecList.Exists( ulPluginID ) )
    {
        // A codec is already registered with this ID.
        return AK_Fail;
    }

    AKASSERT( in_CodecDescriptor.pFileSrcCreateFunc != NULL );
	AKASSERT( in_CodecDescriptor.pBankSrcCreateFunc != NULL );
    if ( in_CodecDescriptor.pFileSrcCreateFunc == NULL || in_CodecDescriptor.pBankSrcCreateFunc == NULL )
    {
        return AK_InvalidParameter;
    }

	return m_RegisteredCodecList.Set( ulPluginID, in_CodecDescriptor ) ? AK_Success : AK_Fail;
}

// Run-time

//-----------------------------------------------------------------------------
// Name: Alloc
// Desc: Effects object factory. The appropriate registered 
//       CreateEffect function is called herein.
//-----------------------------------------------------------------------------
AKRESULT CAkEffectsMgr::Alloc( 
	AkPluginID in_EffectTypeID,                     // Effect type ID.
	AK::IAkPlugin* & out_pEffect,                   // Effect instance.
	AkPluginInfo & out_pluginInfo
    )
{
    out_pEffect = NULL;

    // See if the effect type is registered.
    EffectTypeRecord * pTypeRec = m_RegisteredFXList.Exists( in_EffectTypeID );
    if ( !pTypeRec )
    {
        MONITOR_ERROR_PARAM( AK::Monitor::ErrorCode_PluginNotRegistered, in_EffectTypeID, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, AK_INVALID_UNIQUE_ID, false );
		return AK_PluginNotRegistered;
    }

    AKASSERT( pTypeRec->pCreateFunc != NULL );

    // Call the effect creation function.
	out_pEffect = (*pTypeRec->pCreateFunc)(AkFXMemAlloc::GetLower());

	if (out_pEffect)
	{
		AKVERIFY(out_pEffect->GetPluginInfo(out_pluginInfo) == AK_Success);
		return AK_Success; // REVIEW: CAkEffectsMgr::Alloc needs to return success if out_pEffect has been allocated, to prevent mem leaks
	}
	else
	{
		return AK_Fail;
	}
}

//-----------------------------------------------------------------------------
// Name: AllocParams
// Desc: Effects object factory. The appropriate registered 
//       CreateEffect function is called herein.
//-----------------------------------------------------------------------------
AKRESULT CAkEffectsMgr::AllocParams(
	AK::IAkPluginMemAlloc * in_pAllocator,
	AkPluginID in_EffectTypeID,               // Effect type ID.
    AK::IAkPluginParam * & out_pEffectParam  // Effect param instance.
    )
{
    out_pEffectParam = NULL;

    // See if the effect type is registered.
    EffectTypeRecord * pTypeRec = m_RegisteredFXList.Exists( in_EffectTypeID );
    if ( !pTypeRec )
    {
        // Plug-in not registered.
		MONITOR_ERROR_PARAM( AK::Monitor::ErrorCode_PluginNotRegistered, in_EffectTypeID, AK_INVALID_PLAYING_ID, AK_INVALID_GAME_OBJECT, AK_INVALID_UNIQUE_ID, false );
		return AK_PluginNotRegistered;
    }

    if ( NULL == pTypeRec->pCreateParamFunc )
    {
        // Registered, but no EffectParam object.
        return AK_Success;
    }

    // Call the effect creation function.
    out_pEffectParam = (*pTypeRec->pCreateParamFunc)( in_pAllocator );

	return out_pEffectParam ? AK_Success : AK_Fail;
}

//-----------------------------------------------------------------------------
// Name: AllocCodec
// Desc: Codec object factory. The appropriate registered creation function is called herein.
//-----------------------------------------------------------------------------
IAkSoftwareCodec * CAkEffectsMgr::AllocCodecSrc( 
		CAkPBI * in_pCtx,						// Source context.
		AkUInt32 in_uSrcType,
		AkCodecID in_CodecID					// Codec type ID.
    )
{
    // See if the codec type is registered.
    AkCodecDescriptor * pTypeRec = m_RegisteredCodecList.Exists( in_CodecID );
    if ( !pTypeRec )
    {
		MONITOR_ERROR_PARAM( AK::Monitor::ErrorCode_CodecNotRegistered, in_CodecID, in_pCtx->GetPlayingID(), in_pCtx->GetGameObjectPtr()->ID(), in_pCtx->GetSoundID(), false );
        return NULL;
    }
	
	IAkSoftwareCodec * pSrc = NULL;

	if ( in_uSrcType == SrcTypeFile )
	{
		AKASSERT( pTypeRec->pFileSrcCreateFunc != NULL );
		pSrc = (*pTypeRec->pFileSrcCreateFunc)( (void*)in_pCtx );
	} 
	else
	{
		AKASSERT( pTypeRec->pBankSrcCreateFunc != NULL );
		pSrc = (*pTypeRec->pBankSrcCreateFunc)( (void*)in_pCtx );
	}

	return pSrc;
}

IAkFileCodec * CAkEffectsMgr::AllocFileCodec(AkCodecID in_CodecID)
{
    // See if the codec type is registered.
    AkCodecDescriptor * pTypeRec = m_RegisteredCodecList.Exists( in_CodecID );
    if ( !pTypeRec )
    {
		MONITOR_ERROR(AK::Monitor::ErrorCode_CodecNotRegistered);
        return nullptr;
    }
	
	IAkFileCodec * pDec = nullptr;
	if (pTypeRec->pFileCodecCreateFunc != nullptr)
	{
		pDec = (*pTypeRec->pFileCodecCreateFunc)();
	}
	return pDec;
}

void CAkEffectsMgr::FreeFileCodec(AkCodecID in_CodecID, IAkFileCodec * in_pDecoder)
{
	AkDelete(AkMemID_Processing, in_pDecoder);
}

IAkGrainCodec * CAkEffectsMgr::AllocGrainCodec(AkCodecID in_CodecID)
{
    // See if the codec type is registered.
    AkCodecDescriptor * pTypeRec = m_RegisteredCodecList.Exists( in_CodecID );
    if ( !pTypeRec )
    {
		MONITOR_ERROR(AK::Monitor::ErrorCode_CodecNotRegistered);
        return nullptr;
    }
	
	IAkGrainCodec * pDec = nullptr;
	if (pTypeRec->pGrainCodecCreateFunc != nullptr)
	{
		pDec = (*pTypeRec->pGrainCodecCreateFunc)();
	}
	return pDec;
}

void CAkEffectsMgr::FreeGrainCodec(IAkGrainCodec * in_pCodec)
{
	AkDelete(AkMemID_Processing, in_pCodec);
}

CAkEffectsMgr::EffectTypeRecord* CAkEffectsMgr::GetEffectTypeRecord(AkPluginID in_idPlugin)
{
	EffectTypeRecord * pFactory = m_RegisteredFXList.Exists(in_idPlugin);
	if (pFactory == NULL)
		return NULL;

	return pFactory;
}

AK::Monitor::ErrorCode CAkEffectsMgr::ValidatePluginInfo(AkUInt32 in_uPluginID, AkPluginType in_eType, const AkPluginInfo & in_info)
{
	if (in_info.uBuildVersion < AK_OLDEST_SUPPORTED_WWISESDK_VERSION || in_info.uBuildVersion > AK_WWISESDK_VERSION_COMBINED)
		return AK::Monitor::ErrorCode_PluginVersionMismatch;

	if (in_info.eType != in_eType)
		return AK::Monitor::ErrorCode_PluginExecutionInvalid;

	return AK::Monitor::ErrorCode_NoError;
}
