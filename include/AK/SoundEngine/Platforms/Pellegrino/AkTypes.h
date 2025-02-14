/*******************************************************************************
The content of this file includes portions of the AUDIOKINETIC Wwise Technology
released in source code form as part of the SDK installer package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use this file in accordance with the end user license agreement provided 
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Apache License Usage

Alternatively, this file may be used under the Apache License, Version 2.0 (the 
"Apache License"); you may not use this file except in compliance with the 
Apache License. You may obtain a copy of the Apache License at 
http://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software distributed
under the Apache License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES
OR CONDITIONS OF ANY KIND, either express or implied. See the Apache License for
the specific language governing permissions and limitations under the License.

  Version:  Build: 
  Copyright (c) 2006-2019 Audiokinetic Inc.
*******************************************************************************/

// AkTypes.h

/// \file 
/// Data type definitions.

#pragma once

#ifdef __PROSPERO__

#include <acm.h>
#include <kernel\eventflag.h>
#include <kernel.h>
#include <libdbg.h>
#include <limits.h>
#include <sceconst.h>
#include <scetypes.h>
#include <stddef.h>
#include <stdint.h>

#define AK_PELLEGRINO							///< Compiling for Pellegrino
#define AK_SONY									///< Sony platform

#define AK_SCE_AJM_SUPPORTED                ///< Supports SCE AJM library
#define AK_ATRAC9_SUPPORTED						///< Supports ATRAC9 codec
#define AK_DVR_BYPASS_SUPPORTED					///< Supports feature which blocks DVRs from recording BGM
#define AK_HARDWARE_DECODING_SUPPORTED          ///< Supports LEngine callbacks for hardware decoding
#define AK_WEM_OPUS_HW_SUPPORTED                ///< Supports hardware decoding of WEM Opus codec
#define AK_DEVICE_MEMORY_SUPPORTED              ///< Supports special memory allocations shared with the audio co-processor

#define AK_LFECENTER							///< Internal use
#define AK_REARCHANNELS							///< Internal use
#define AK_SUPPORT_WCHAR						///< Can support wchar
#define AK_71FROM51MIXER						///< Internal use
#define AK_71FROMSTEREOMIXER					///< Internal use
#define AK_71AUDIO								///< Internal use
//#define AK_OS_WCHAR	

//#define AK_ENABLE_RAZOR_PROFILING

#define AK_CPU_X86_64

#define AK_RESTRICT				__restrict								///< Refers to the __restrict compilation flag available on some platforms
#define AK_EXPECT_FALSE( _x )	( _x )
#define AkRegister				
#define AkForceInline			inline __attribute__((always_inline))	///< Force inlining
#define AkNoInline				__attribute__((noinline))

#define AK_SIMD_ALIGNMENT	16					///< Platform-specific alignment requirement for SIMD data
#define AK_ALIGN_SIMD( __Declaration__ ) __Declaration__ __attribute__((aligned(AK_SIMD_ALIGNMENT))) ///< Platform-specific alignment requirement for SIMD data
#define AK_ALIGN_DMA							///< Platform-specific data alignment for DMA transfers
#define AK_ALIGN_FASTDMA 						///< Platform-specific data alignment for faster DMA transfers
#define AK_ALIGN_SIZE_FOR_DMA( __Size__ ) (__Size__) ///< Used to align sizes to next 16 byte boundary on platfroms that require it
#define AK_BUFFER_ALIGNMENT SCE_ACM_DATA_ALIGNMENT
#define AK_ALIGN_ACM( __Declaration__ ) __Declaration__ __attribute__((aligned(SCE_ACM_DATA_ALIGNMENT))) ///< Platform-specific alignment requirement for ACM data

#define AKSIMD_V4F32_SUPPORTED
#define AKSIMD_SSE2_SUPPORTED
#define AKSIMD_AVX_SUPPORTED
#define AKSIMD_AVX2_SUPPORTED

#define AK_DLLEXPORT __declspec(dllexport)
#define AK_DLLIMPORT __declspec(dllimport)	

typedef uint8_t			AkUInt8;				///< Unsigned 8-bit integer
typedef uint16_t		AkUInt16;				///< Unsigned 16-bit integer
typedef uint32_t		AkUInt32;				///< Unsigned 32-bit integer
typedef uint64_t		AkUInt64;				///< Unsigned 64-bit integer

#ifdef AK_CPU_X86_64
typedef int64_t			AkIntPtr;
typedef uint64_t		AkUIntPtr;
#else 
typedef int				AkIntPtr;
typedef unsigned int	AkUIntPtr;
#endif

typedef int8_t			AkInt8;					///< Signed 8-bit integer
typedef int16_t			AkInt16;				///< Signed 16-bit integer
typedef int32_t   		AkInt32;				///< Signed 32-bit integer
typedef int64_t			AkInt64;				///< Signed 64-bit integer

typedef char			AkOSChar;				///< Generic character string

typedef float			AkReal32;				///< 32-bit floating point
typedef double          AkReal64;				///< 64-bit floating point

typedef ScePthread				AkThread;				///< Thread handle
typedef ScePthread				AkThreadID;				///< Thread ID
typedef void* 					(*AkThreadRoutine)(	void* lpThreadParameter	);		///< Thread routine
typedef SceKernelEventFlag		AkEvent;				///< Event handle

typedef int				AkFileHandle;			///< File handle to be used with sceKernel file system calls

typedef wchar_t					AkUtf16;				///< Type for 2 byte chars. Used for communication
														///< with the authoring tool.

#define AK_UINT_MAX				UINT_MAX

// For strings.
#define AK_MAX_PATH				SCE_KERNEL_PATH_MAX						///< Maximum path length (each file/dir name is max 255 char)

typedef AkUInt32				AkFourcc;				///< Riff chunk

/// Create Riff chunk
#define AkmmioFOURCC( ch0, ch1, ch2, ch3 )									    \
		( (AkFourcc)(AkUInt8)(ch0) | ( (AkFourcc)(AkUInt8)(ch1) << 8 ) |		\
		( (AkFourcc)(AkUInt8)(ch2) << 16 ) | ( (AkFourcc)(AkUInt8)(ch3) << 24 ) )

#define AK_BANK_PLATFORM_DATA_ALIGNMENT				(256)	///< Required memory alignment for bank loading by memory address for ATRAC9 (see LoadBank())
#define AK_BANK_PLATFORM_DATA_NON_ATRAC9_ALIGNMENT	(16)	///< Required memory alignment for bank loading by memory address for non-ATRAC9 formats (see LoadBank())

#define AK_BANK_PLATFORM_ALLOC_TYPE		AkMalloc

#define AK_COMM_CONSOLE_TYPE ConsolePellegrino

/// Macro that takes a string litteral and changes it to an AkOSChar string at compile time
/// \remark This is similar to the TEXT() and _T() macros that can be used to turn string litterals into Unicode strings
/// \remark Usage: AKTEXT( "Some Text" )
#define AKTEXT(x) x

#endif // __PROSPERO__
