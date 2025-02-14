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

#include "stdafx.h"
#include <AK/Tools/Common/AkPlatformFuncs.h>

#include "AkRuntimeEnvironmentMgr.h"

#if (defined(AK_CPU_X86_64) || defined(AK_CPU_X86)) && !defined AK_IOS
	#define AK_USE_X86_CPUID
#endif

namespace AK
{
#define EDX_SSE_bit			0x2000000	// 25 bit
#define EDX_SSE2_bit		0x4000000	// 26 bit
#define ECX_SSE3_bit		0x1			// 0 bit
#define ECX_SSSE3_bit		0x200		// 9 bit
#define ECX_SSE41_bit		0x80000		// 19 bit
#define ECX_AVX_bit			1<<28		// 28 bit
#define ECX_F16C_bit		1<<29		// 29 bit
#define EBX_AVX2_bit		0x20		// 5 bit

	AkRuntimeEnvironmentMgr * AkRuntimeEnvironmentMgr::Instance()
	{
		static AkRuntimeEnvironmentMgr g_RuntimeEnvMgr;
		return &g_RuntimeEnvMgr;
	}

	AkRuntimeEnvironmentMgr::AkRuntimeEnvironmentMgr()
	{
		Initialize();
	}

	void AkRuntimeEnvironmentMgr::Initialize()
	{
#if defined AK_USE_X86_CPUID
		unsigned int CPUFeatures[4];
		unsigned int CPUFeaturesExt[4];
		AkUInt32 uEBX = 0;
		AkUInt32 uECX = 0;
		AkUInt32 uEDX = 0;

		AKPLATFORM::CPUID(0, 0, CPUFeatures);
		if (CPUFeatures[0] >= 1)
		{
			AKPLATFORM::CPUID(1, 0, CPUFeatures);
			uECX = CPUFeatures[2];
			uEDX = CPUFeatures[3];

			AKPLATFORM::CPUID(7, 0, CPUFeatures);
			uEBX = CPUFeatures[1];
		}

		AKPLATFORM::CPUID(0x80000000, 0, CPUFeatures);
		unsigned int uExtIds = CPUFeatures[0];
		if (uExtIds >= 0x80000006)
		{
			AKPLATFORM::CPUID(0x80000006, 0, CPUFeaturesExt);
			// Cache info
			ProcessorInfo.uCacheLineSize = CPUFeaturesExt[2] & 0xFF;
			ProcessorInfo.uL2CacheSize = (CPUFeaturesExt[2] >> 16) & 0xFFFF;
		}
		else
		{
			ProcessorInfo.uCacheLineSize = 64;
			ProcessorInfo.uL2CacheSize = 1024;
		}

		// SIMD support
		if (EDX_SSE_bit & uEDX)
			ProcessorInfo.uSIMDProcessorSupport |= AK_SIMD_SSE;
		if (EDX_SSE2_bit & uEDX)
			ProcessorInfo.uSIMDProcessorSupport |= AK_SIMD_SSE2;
		if (ECX_SSE3_bit & uECX)
			ProcessorInfo.uSIMDProcessorSupport |= AK_SIMD_SSE3;
		if (ECX_SSSE3_bit & uECX)
			ProcessorInfo.uSIMDProcessorSupport |= AK_SIMD_SSSE3;
		if (ECX_SSE41_bit & uECX)
			ProcessorInfo.uSIMDProcessorSupport |= AK_SIMD_SSE41;
		if (ECX_AVX_bit & uECX)
			ProcessorInfo.uSIMDProcessorSupport |= AK_SIMD_AVX;
		if (ECX_F16C_bit & uECX)
			ProcessorInfo.uSIMDProcessorSupport |= AK_SIMD_F16C;
		if (EBX_AVX2_bit & uEBX)
			ProcessorInfo.uSIMDProcessorSupport |= AK_SIMD_AVX2;
#endif
	}

	// Provide a bitfield of flags to explicitly disable runtime detection for
	void AkRuntimeEnvironmentMgr::DisableSIMDSupport(AkSIMDProcessorSupport in_eSIMD)
	{
		ProcessorInfo.uSIMDProcessorSupport &= ~in_eSIMD;
	}

	bool AkRuntimeEnvironmentMgr::GetSIMDSupport(AkSIMDProcessorSupport in_eSIMD)
	{
		return (ProcessorInfo.uSIMDProcessorSupport & in_eSIMD) != 0;
	}

	AkUInt32 AkRuntimeEnvironmentMgr::GetCacheSize()
	{
		return ProcessorInfo.uL2CacheSize;
	}

	AkUInt32 AkRuntimeEnvironmentMgr::GetCacheLineSize()
	{
		return ProcessorInfo.uCacheLineSize;
	}
}
