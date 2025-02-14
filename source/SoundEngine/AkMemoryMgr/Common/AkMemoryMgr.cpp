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
// AkMemoryMgr.cpp
//
// Generic implementation
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h" 
#include "AkMemoryMgrBase.h"
#include <AK/SoundEngine/Common/AkModule.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include "rpmalloc/rpmalloc.h"

extern void AkMemDebugMallocDefault(AkMemPoolId poolId, size_t uSize, void* pAddress, char const* pszFile, AkUInt32 uLine);
extern void AkMemDebugMalignDefault(AkMemPoolId poolId, size_t uSize, AkUInt32 uAlignment, void* pAddress, char const* pszFile, AkUInt32 uLine);
extern void AkMemDebugDeviceMallocDefault(AkMemPoolId poolId, size_t uSize, AkUInt32 uAlignment, void* pAddress, char const* pszFile, AkUInt32 uLine);
extern void AkMemDebugReallocDefault(AkMemPoolId poolId, void* pOldAddress, size_t uSize, void* pNewAddress, char const* pszFile, AkUInt32 uLine);
extern void AkMemDebugFreeDefault(AkMemPoolId poolId, void* pAddress);

#define RPMALLOC_INSTANCE(_poolId) ((_poolId & AkMemType_Device) ? 1 : 0)

//RED
static void AkMemInitforThreadDefault( const char * in_szThreadName )
{
	rpmalloc_thread_initialize(0);
}

static void AkMemTermForThreadDefault()
{
	rpmalloc_thread_finalize(0);
#if defined(AK_DEVICE_MEMORY_SUPPORTED)
	rpmalloc_thread_finalize(1);
#endif
}

static void* AkMemMallocDefault( AkMemPoolId poolId, size_t uSize )
{
	void* context = rpmalloc_thread_initialize(RPMALLOC_INSTANCE(poolId));
	if ( !context )
		return NULL;
	return rpmalloc(RPMALLOC_INSTANCE(poolId), context, uSize);
}

static void* AkMemMalignDefault( AkMemPoolId poolId, size_t uSize, AkUInt32 uAlignment )
{
	void* context = rpmalloc_thread_initialize(RPMALLOC_INSTANCE(poolId));
	if ( !context )
		return NULL;
	return rpaligned_alloc(RPMALLOC_INSTANCE(poolId), context, uAlignment, uSize);
}

static void* AkMemReallocDefault( AkMemPoolId poolId, void* pAddress, size_t uSize )
{
	void* context = rpmalloc_thread_initialize(RPMALLOC_INSTANCE(poolId));
	if ( !context )
		return NULL;
	return rprealloc(RPMALLOC_INSTANCE(poolId), context, pAddress, uSize);
}

static void AkMemFreeDefault( AkMemPoolId poolId, void* pAddress )
{
	void* context = rpmalloc_thread_initialize(RPMALLOC_INSTANCE(poolId));
	if (context)
		rpfree(RPMALLOC_INSTANCE(poolId), context, pAddress);
}

static void AkMemFalignDefault( AkMemPoolId poolId, void* pAddress )
{
	void* context = rpmalloc_thread_initialize(RPMALLOC_INSTANCE(poolId));
	if (context)
		rpfree(RPMALLOC_INSTANCE(poolId), context, pAddress);
}

static size_t AkMemTotalReservedMemorySizeDefault() {
#if defined(AK_DEVICE_MEMORY_SUPPORTED)
	return ak_rpmalloc_total_reserved_size(0) + ak_rpmalloc_total_reserved_size(1);
#else
	return ak_rpmalloc_total_reserved_size(0);
#endif
}

static size_t AkMemSizeOfMemoryDefault( AkMemPoolId poolId, void* pAddress )
{
	void* context = rpmalloc_thread_initialize(RPMALLOC_INSTANCE(poolId));
	if ( !context )
		return 0;
	return rpmalloc_usable_size(RPMALLOC_INSTANCE(poolId), pAddress);
}

#ifndef AK_OPTIMIZED
static AKRESULT AkMemDebugCheckForOverwriteDefault()
{
	int result = ak_rpmalloc_thread_verify_integrity(0);
	return result? AK_Success : AK_Fail;
}
#endif

namespace AK
{

namespace MemoryMgr
{

extern AkMemSettings g_settings;
static bool g_bRpmallocInitialized = false;

AKRESULT Init( AkMemSettings * in_pSettings )
{	
	if ( IsInitialized() )
		return AK_Success;

    if ( in_pSettings == NULL )
    {
        AKASSERT( !"Invalid memory manager settings" );
		return AK_Fail;
    }

	g_settings.pfAllocVM = in_pSettings->pfAllocVM;
	g_settings.pfFreeVM = in_pSettings->pfFreeVM;

#if defined(AK_DEVICE_MEMORY_SUPPORTED)
	g_settings.pfAllocDevice = in_pSettings->pfAllocDevice;
	g_settings.pfFreeDevice = in_pSettings->pfFreeDevice;
#endif

	if ( !in_pSettings->pfMalloc && !in_pSettings->pfMalign && !in_pSettings->pfRealloc
			&& !in_pSettings->pfFree && !in_pSettings->pfFalign && !in_pSettings->pfSizeOfMemory )
	{
		// all NULL, use the default implementation
		g_settings.pfInitForThread				= AkMemInitforThreadDefault;
		g_settings.pfTermForThread				= AkMemTermForThreadDefault;
		g_settings.pfMalloc						= AkMemMallocDefault;
		g_settings.pfMalign						= AkMemMalignDefault;
		g_settings.pfRealloc					= AkMemReallocDefault;
		g_settings.pfFree						= AkMemFreeDefault;
		g_settings.pfFalign						= AkMemFalignDefault;
		g_settings.pfTotalReservedMemorySize	= AkMemTotalReservedMemorySizeDefault;
		g_settings.pfSizeOfMemory				= AkMemSizeOfMemoryDefault;

		rpmalloc_config_t rpmallocConfig;
		AKPLATFORM::AkMemSet( &rpmallocConfig, 0, sizeof( rpmalloc_config_t ) );
		rpmallocConfig.memory_map 	= g_settings.pfAllocVM;
		rpmallocConfig.memory_unmap = g_settings.pfFreeVM;
		rpmallocConfig.overall_memory_limit = (size_t)in_pSettings->uMemAllocationSizeLimit;
#ifdef AK_VM_PAGE_SIZE
		rpmallocConfig.page_size = AK_VM_PAGE_SIZE;
#endif
		rpmalloc_initialize_config(0, &rpmallocConfig);

#if defined(AK_DEVICE_MEMORY_SUPPORTED)
		AKPLATFORM::AkMemSet(&rpmallocConfig, 0, sizeof(rpmalloc_config_t));
		rpmallocConfig.memory_map = g_settings.pfAllocDevice;
		rpmallocConfig.memory_unmap = g_settings.pfFreeDevice;
		rpmallocConfig.page_size = AK_VM_DEVICE_PAGE_SIZE;
		rpmalloc_initialize_config(1, &rpmallocConfig);
#endif

		g_bRpmallocInitialized = true;
	}
	else if ( in_pSettings->pfMalloc && in_pSettings->pfMalign && in_pSettings->pfRealloc
		&& in_pSettings->pfFree && in_pSettings->pfFalign && in_pSettings->pfSizeOfMemory )
	{
		// all non-NULL, use the provided set of function pointers
		g_settings.pfInitForThread				= in_pSettings->pfInitForThread;
		g_settings.pfTermForThread				= in_pSettings->pfTermForThread;
		g_settings.pfMalloc						= in_pSettings->pfMalloc;
		g_settings.pfMalign						= in_pSettings->pfMalign;
		g_settings.pfRealloc					= in_pSettings->pfRealloc;
		g_settings.pfFree						= in_pSettings->pfFree;
		g_settings.pfFalign						= in_pSettings->pfFalign;
		g_settings.pfTotalReservedMemorySize    = in_pSettings->pfTotalReservedMemorySize;
		g_settings.pfSizeOfMemory				= in_pSettings->pfSizeOfMemory;
	}
	else
	{
		// some non-NULL, must use a complete matching set of functions
		AKASSERT( !"must use a complete matching set of memory functions" );
		return AK_Fail;
	}

	g_settings.uMemAllocationSizeLimit = in_pSettings->uMemAllocationSizeLimit;

#ifndef AK_OPTIMIZED
	g_settings.pfDebugMalloc			= in_pSettings->pfDebugMalloc;
	g_settings.pfDebugMalign			= in_pSettings->pfDebugMalign;
	g_settings.pfDebugRealloc			= in_pSettings->pfDebugRealloc;
	g_settings.pfDebugFree				= in_pSettings->pfDebugFree;
	g_settings.pfDebugFalign			= in_pSettings->pfDebugFalign;
	g_settings.pfDebugCheckForOverwrite	= in_pSettings->pfDebugCheckForOverwrite ? 
		in_pSettings->pfDebugCheckForOverwrite : AkMemDebugCheckForOverwriteDefault;
#endif // AK_OPTIMIZED

	return InitBase();
}

void Term()
{
	if (g_bRpmallocInitialized)
	{
		rpmalloc_finalize(0);
#if defined(AK_DEVICE_MEMORY_SUPPORTED)
		rpmalloc_finalize(1);
#endif
		g_bRpmallocInitialized = false;
	}
	return TermBase();
}

void GetDefaultSettings( AkMemSettings & out_pSettings )
{
	out_pSettings.pfInitForThread			= NULL;
	out_pSettings.pfTermForThread			= NULL;
	out_pSettings.pfMalloc					= NULL;
	out_pSettings.pfMalign					= NULL;
	out_pSettings.pfRealloc					= NULL;
	out_pSettings.pfFree					= NULL;
	out_pSettings.pfFalign					= NULL;
	out_pSettings.pfTotalReservedMemorySize = NULL;
	out_pSettings.pfSizeOfMemory			= NULL;
	out_pSettings.uMemAllocationSizeLimit	= 0;

	out_pSettings.pfAllocVM = AKPLATFORM::AllocVM;
	out_pSettings.pfFreeVM = AKPLATFORM::FreeVM;

#if defined(AK_DEVICE_MEMORY_SUPPORTED)
	out_pSettings.pfAllocDevice = AKPLATFORM::AllocDevice;
	out_pSettings.pfFreeDevice = AKPLATFORM::FreeDevice;
#endif

#ifdef AK_MEMDEBUG
	out_pSettings.pfDebugMalloc = AkMemDebugMallocDefault;
	out_pSettings.pfDebugMalign = AkMemDebugMalignDefault;
	out_pSettings.pfDebugRealloc = AkMemDebugReallocDefault;
	out_pSettings.pfDebugFree = AkMemDebugFreeDefault;
	out_pSettings.pfDebugFalign = AkMemDebugFreeDefault;
#else
	out_pSettings.pfDebugMalloc = NULL;
	out_pSettings.pfDebugMalign = NULL;
	out_pSettings.pfDebugRealloc = NULL;
	out_pSettings.pfDebugFree = NULL;
	out_pSettings.pfDebugFalign = NULL;
#endif

	out_pSettings.pfDebugCheckForOverwrite = NULL;
}

//RED
void InitForThread( const char * in_szThreadName )
{
	if ( g_settings.pfInitForThread )
		g_settings.pfInitForThread( in_szThreadName );
}

void TermForThread()
{
	if ( g_settings.pfTermForThread )
		g_settings.pfTermForThread();
}

} // namespace MemoryMgr

} // namespace AK
