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
// AkMemoryMgrBase.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h" 

#include "AkMemoryMgrBase.h"
#include "AkMonitor.h"
#include <AK/SoundEngine/Common/AkModule.h>

#ifdef MSTC_SYSTEMATIC_MEMORY_STRESS
#include <map>

#define MSTC_GRACE_DELAY 175
AkAtomic32 g_MSTC_AllocCounter;
AkAtomic32 g_MSTC_denyAllocFailure;
extern AkUInt32 g_Loops;

AkAutoDenyAllocFailure::AkAutoDenyAllocFailure() {
	AkAtomicInc32( &g_MSTC_denyAllocFailure );
}

AkAutoDenyAllocFailure::~AkAutoDenyAllocFailure() {
	AkAtomicDec32( &g_MSTC_denyAllocFailure );
}
#endif // MSTC_SYSTEMATIC_MEMORY_STRESS

#ifdef AK_MEMDEBUG
#include "AkMemTracker.h"

AkMemTracker * g_pMemTracker;

void AkMemDebugMallocDefault( AkMemPoolId poolId, size_t uSize, void* pAddress, char const* pszFile, AkUInt32 uLine )
{
	if ( g_pMemTracker )
		g_pMemTracker->Add( poolId, pAddress, uSize, pszFile, uLine );
}

void AkMemDebugMalignDefault( AkMemPoolId poolId, size_t uSize, AkUInt32 uAlignment, void* pAddress, char const* pszFile, AkUInt32 uLine )
{
	if ( g_pMemTracker )
		g_pMemTracker->Add( poolId, pAddress, uSize, pszFile, uLine );
}

void AkMemDebugReallocDefault( AkMemPoolId poolId, void* pOldAddress, size_t uSize, void* pNewAddress, char const* pszFile, AkUInt32 uLine )
{
	if (g_pMemTracker)
	{
		g_pMemTracker->Remove(poolId, pOldAddress);
		g_pMemTracker->Add(poolId, pNewAddress, uSize, pszFile, uLine);
	}
}
void AkMemDebugFreeDefault( AkMemPoolId poolId, void* pAddress )
{
	if (g_pMemTracker)
		g_pMemTracker->Remove(poolId, pAddress);
}
#endif // AK_MEMDEBUG

namespace AK
{

namespace MemoryMgr
{

AkMemSettings	g_settings;
static bool		s_bInitialized = false;

#ifndef AK_OPTIMIZED

static bool		s_bFrameCheckEnabled = false;
static bool		s_bVoiceCheckEnabled = false;

char const* g_aPoolNames[AkMemID_NUM] =
{
	"Object",
	"Event",
	"Structure",
	"Media",
	"Game Object",
	"Processing",
	"Processing (Plug-in)",
	"Streaming",
	"Streaming I/O",
	"Spatial Audio",
	"Geometry",
	"Reflection/Diffraction Paths",
	"Game Simulator",
	"Monitor Queue",
	"Profiler",
	"File Package",
	"Sound Engine"
};

AK_ALIGN( struct AllocationCount{
	AkAtomic32 count;
}, 64 );

AK_ALIGN( struct AllocationSize{
	AkAtomic64 size;
}, 64 );

static AllocationCount		allocCounts[AkMemID_NUM * 2];
static AllocationCount		freeCounts[AkMemID_NUM * 2];
static AllocationSize		allocationSizes[AkMemID_NUM * 2];
static AllocationSize		peakAllocationSizes[AkMemID_NUM * 2];
static AkThreadID			profileThreadID = AK_NULL_THREAD;
static AllocationSize		profileThreadUsage;

static void TrackAllocation( AkMemPoolId in_poolId, AkUInt64 uSize )
{
	if (in_poolId & AkMemType_Device)
		in_poolId = (in_poolId & AkMemID_MASK) + AkMemID_NUM;

	AkAtomicInc32( &allocCounts[ in_poolId ].count );

	AkInt64 categorySize = AkAtomicAdd64( &allocationSizes[ in_poolId ].size, uSize );

	for ( ; ; )
	{
		AkInt64 expectedPeak = AkAtomicLoad64( &peakAllocationSizes[ in_poolId ].size );
		if ( expectedPeak >= categorySize )
			break;
		if ( AkAtomicCas64( &peakAllocationSizes[ in_poolId ].size, categorySize, expectedPeak ) )
			break;
	}

	if ( AKPLATFORM::CurrentThread() == profileThreadID )
	{
		AkAtomicAdd64( &profileThreadUsage.size, uSize );
	}
}

static void TrackFree( AkMemPoolId in_poolId, AkUInt64 uSize )
{
	if (in_poolId & AkMemType_Device)
		in_poolId = (in_poolId & AkMemID_MASK) + AkMemID_NUM;

	AkAtomicInc32( &freeCounts[ in_poolId ].count );
	AkAtomicSub64( &allocationSizes[ in_poolId ].size, uSize );

	if ( AKPLATFORM::CurrentThread() == profileThreadID )
	{
		AkAtomicSub64( &profileThreadUsage.size, uSize );
	}
}

#endif // AK_OPTIMIZED

static void ReportAllocationFailure(AkMemPoolId in_poolId, size_t in_uSize)
{
#ifndef AK_OPTIMIZED
	AkMonitor::MonitorAllocFailure(g_aPoolNames[in_poolId & AkMemID_MASK], in_uSize, g_settings.pfTotalReservedMemorySize ? g_settings.pfTotalReservedMemorySize() : 0, g_settings.uMemAllocationSizeLimit);
#endif // AK_OPTIMIZED
}


#ifdef MSTC_SYSTEMATIC_MEMORY_STRESS

typedef std::map< AkUInt32, AkUInt32 > FailureMap;
static FailureMap failedAllocations;
#define MEM_FAIL_COVERAGE_DATA_FILENAME "MemFailCoverageData.dat"
static AkUInt32 g_uLoadedAllocs = 0;

static void LoadFailureCoverageMap()
{
	FILE* file = fopen( MEM_FAIL_COVERAGE_DATA_FILENAME, "rb" );
	if ( !file )
	{
		char szMsg[ 1024 ];
		sprintf( szMsg, "*** Unable to load failure coverage data.  Can't open file: %s\n", MEM_FAIL_COVERAGE_DATA_FILENAME );
		AKPLATFORM::OutputDebugMsg( szMsg );
		return;
	}

	AkUInt32 hashAndCount[ 2 ];
	while ( fread( &hashAndCount[ 0 ], sizeof( AkUInt32 ), 2, file ) )
		failedAllocations[ hashAndCount[ 0 ] ] = hashAndCount[ 1 ];
	fclose( file );
	g_uLoadedAllocs = ( AkUInt32 )failedAllocations.size();
}

static void SaveFailureCoverageMap()
{
	FILE* file = fopen( MEM_FAIL_COVERAGE_DATA_FILENAME, "wb" );
	if ( !file )
	{
		char szMsg[ 1024 ];
		sprintf( szMsg, "*** Unable to save failure coverage data.  Can't open file: %s\n", MEM_FAIL_COVERAGE_DATA_FILENAME );
		AKPLATFORM::OutputDebugMsg( szMsg );
		return;
	}

	char szMsg[ 1024 ];
	sprintf( szMsg, "### Saving %u allocations to file: %s\n", ( AkUInt32 )failedAllocations.size(), MEM_FAIL_COVERAGE_DATA_FILENAME );
	AKPLATFORM::OutputDebugMsg( szMsg );

	if ( g_uLoadedAllocs != failedAllocations.size() )
	{
		AkAtomicStore32( &g_MSTC_AllocCounter, 0 );
		g_Loops = 1; //loop again!
	}

	FailureMap::iterator end = failedAllocations.end();
	for ( FailureMap::iterator it = failedAllocations.begin(); it != end; ++it )
	{
		AkUInt32 hash = it->first;
		AkUInt32 count = it->second;
		fwrite( &hash, sizeof( AkUInt32 ), 1, file );
		fwrite( &count, sizeof( AkUInt32 ), 1, file );
	}

	fclose( file );
}

static bool FailedAlready()
{
	const AkUInt32 framesToSkip = 4;
	const AkUInt32 framesToCapture = 4;
	PVOID backtrace[ framesToCapture ];
	memset( &backtrace, 0, sizeof( backtrace ) );
	DWORD hash;
	CaptureStackBackTrace( framesToSkip, framesToCapture, &backtrace[ 0 ], &hash );

	FailureMap::iterator found = failedAllocations.find( hash );
	if ( found == failedAllocations.end() )
	{
		failedAllocations[ hash ] = 1;
		return false;
	}
	else
	{
		found->second++;
		return true;
	}
}

static bool MSTC_MustFail( AkMemPoolId in_poolId, size_t in_ulSize ) // return true if allocation is supposed to fail.
{
	//We first let the grace delay before starting the memory issues, allowing the system to properly init.
	if ( AkAtomicLoad32( &g_MSTC_AllocCounter ) > MSTC_GRACE_DELAY && !FailedAlready() && AkAtomicLoad32( &g_MSTC_denyAllocFailure ) == 0 )
	{
#ifndef AK_OPTIMIZED
		ReportAllocationFailure(in_poolId, in_ulSize);
#endif
		return true;
	}
	return false;
}

#define MSTC_HANDLE( _pool, _size ) if ( MSTC_MustFail( _pool, _size ) ) return NULL
#else
#define MSTC_HANDLE( _pool, _size )

#endif //MSTC_SYSTEMATIC_MEMORY_STRESS

AKRESULT InitBase()
{
	s_bInitialized = true;

#ifdef AK_MEMDEBUG	
	g_pMemTracker = new AkMemTracker;
#endif

#ifdef MSTC_SYSTEMATIC_MEMORY_STRESS
	LoadFailureCoverageMap();
#endif

	return AK_Success;
}

bool IsInitialized()
{
	return s_bInitialized;
}

void TermBase()
{
	if ( IsInitialized() )
	{
		memset(&g_settings, 0, sizeof(g_settings));

		s_bInitialized = false;

#ifdef AK_MEMDEBUG	
		if ( g_pMemTracker )
		{
			g_pMemTracker->PrintMemoryLabels();
			delete g_pMemTracker;
			g_pMemTracker = NULL;
		}
#endif

#ifndef AK_OPTIMIZED
		for ( int i = 0; i < AkMemID_NUM; ++i )
			AKASSERT( AkAtomicLoad64( &allocationSizes[ i ].size ) == 0 );
#endif // AK_OPTIMIZED

#ifdef MSTC_SYSTEMATIC_MEMORY_STRESS
		SaveFailureCoverageMap();
#endif
	}
}

void SetFrameCheckEnabled( bool in_bEnabled ) {
#ifndef AK_OPTIMIZED
	s_bFrameCheckEnabled = in_bEnabled;
#endif // AK_OPTIMIZED
}

bool IsFrameCheckEnabled() {
#ifndef AK_OPTIMIZED
	return s_bFrameCheckEnabled;
#else
	return false;
#endif // AK_OPTIMIZED
}
void SetVoiceCheckEnabled( bool in_bEnabled ) {
#ifndef AK_OPTIMIZED
	s_bVoiceCheckEnabled = in_bEnabled;
#endif // AK_OPTIMIZED
}

bool IsVoiceCheckEnabled() {
#ifndef AK_OPTIMIZED
	return s_bVoiceCheckEnabled;
#else
	return false;
#endif // AK_OPTIMIZED
}

AKRESULT CheckForOverwrite() {
#ifndef AK_OPTIMIZED
	return g_settings.pfDebugCheckForOverwrite ? g_settings.pfDebugCheckForOverwrite() : AK_Success;
#else
	return AK_Success;
#endif // AK_OPTIMIZED
}

#ifdef AK_MEMDEBUG
void* dMalloc( AkMemPoolId in_poolId, size_t in_uSize, char const* in_pszFile, AkUInt32 in_uLine )
{
	MSTC_HANDLE( in_poolId, in_uSize );

	void* pAddress = g_settings.pfMalloc( in_poolId, in_uSize );
	if ( !pAddress )
	{
		ReportAllocationFailure(in_poolId, in_uSize);
		return NULL;
	}

#ifndef AK_OPTIMIZED
	if ( g_settings.pfDebugMalloc ) 
		g_settings.pfDebugMalloc( in_poolId, in_uSize, pAddress, in_pszFile, in_uLine );

	if ( g_settings.pfSizeOfMemory )
		TrackAllocation( in_poolId, g_settings.pfSizeOfMemory( in_poolId, pAddress ) );
#endif // AK_OPTIMIZED

#ifdef MSTC_SYSTEMATIC_MEMORY_STRESS
	AkAtomicInc32( &g_MSTC_AllocCounter );
#endif

	return pAddress;
}
#endif // AK_MEMDEBUG

void* Malloc( AkMemPoolId in_poolId, size_t in_uSize )
{
	MSTC_HANDLE( in_poolId, in_uSize );

	void* pAddress = g_settings.pfMalloc( in_poolId, in_uSize );
	if ( !pAddress )
	{
		ReportAllocationFailure(in_poolId, in_uSize);
		return NULL;
	}

#ifndef AK_OPTIMIZED
	if ( g_settings.pfDebugMalloc ) 
		g_settings.pfDebugMalloc( in_poolId, in_uSize, pAddress, NULL, 0 );

	if ( g_settings.pfSizeOfMemory )
		TrackAllocation( in_poolId, g_settings.pfSizeOfMemory( in_poolId, pAddress ) );
#endif // AK_OPTIMIZED

	return pAddress;
}

#ifdef AK_MEMDEBUG
void* dMalign( AkMemPoolId in_poolId, size_t in_uSize, AkUInt32 in_uAlignment, char const* in_pszFile, AkUInt32 in_uLine )
{
	MSTC_HANDLE( in_poolId, in_uSize );

	void* pAddress = g_settings.pfMalign( in_poolId, in_uSize, in_uAlignment );
	if ( !pAddress )
	{
		ReportAllocationFailure(in_poolId, in_uSize);
		return NULL;
	}

#ifndef AK_OPTIMIZED
	if ( g_settings.pfDebugMalign )
		g_settings.pfDebugMalign( in_poolId, in_uSize, in_uAlignment, pAddress, in_pszFile, in_uLine );

	if ( g_settings.pfSizeOfMemory )
		TrackAllocation( in_poolId, g_settings.pfSizeOfMemory( in_poolId, pAddress ) );
#endif // AK_OPTIMIZED

#ifdef MSTC_SYSTEMATIC_MEMORY_STRESS
	AkAtomicInc32( &g_MSTC_AllocCounter );
#endif

	return pAddress;
}
#endif // AK_MEMDEBUG

void* Malign( AkMemPoolId in_poolId, size_t in_uSize, AkUInt32 in_uAlignment )
{
	MSTC_HANDLE( in_poolId, in_uSize );

	void* pAddress = g_settings.pfMalign( in_poolId, in_uSize, in_uAlignment );
	if ( !pAddress )
	{
		ReportAllocationFailure(in_poolId, in_uSize);
		return NULL;
	}

#ifndef AK_OPTIMIZED
	if ( g_settings.pfDebugMalign )
		g_settings.pfDebugMalign( in_poolId, in_uSize, in_uAlignment, pAddress, NULL, 0 );

	if ( g_settings.pfSizeOfMemory )
		TrackAllocation( in_poolId, g_settings.pfSizeOfMemory( in_poolId, pAddress ) );
#endif // AK_OPTIMIZED

#ifdef MSTC_SYSTEMATIC_MEMORY_STRESS
	AkAtomicInc32( &g_MSTC_AllocCounter );
#endif

	return pAddress;
}

#ifdef AK_MEMDEBUG
void* dRealloc( AkMemPoolId in_poolId, void *in_pAlloc, size_t in_uSize, const char *in_pszFile, AkUInt32 in_uLine )
{
#ifndef AK_OPTIMIZED
	if ( in_pAlloc && g_settings.pfSizeOfMemory )
		TrackFree( in_poolId, g_settings.pfSizeOfMemory( in_poolId, in_pAlloc ) );
#endif // AK_OPTIMIZED

	//MSTC_HANDLE( in_poolId, in_uSize );

	void* pNewAddress = g_settings.pfRealloc( in_poolId, in_pAlloc, in_uSize );
	if ( !pNewAddress )
	{
#ifndef AK_OPTIMIZED
		if ( in_pAlloc && g_settings.pfSizeOfMemory )
			TrackAllocation( in_poolId, g_settings.pfSizeOfMemory( in_poolId, in_pAlloc ) );
#endif
		ReportAllocationFailure(in_poolId, in_uSize);
		return NULL;
	}

#ifndef AK_OPTIMIZED
	if ( g_settings.pfDebugRealloc )
		g_settings.pfDebugRealloc( in_poolId, in_pAlloc, in_uSize, pNewAddress, in_pszFile, in_uLine );

	if ( g_settings.pfSizeOfMemory )
		TrackAllocation( in_poolId, g_settings.pfSizeOfMemory( in_poolId, pNewAddress ) );
#endif // AK_OPTIMIZED

#ifdef MSTC_SYSTEMATIC_MEMORY_STRESS
	AkAtomicInc32( &g_MSTC_AllocCounter );
#endif

	return pNewAddress;
}
#endif // AK_MEMDEBUG

void* Realloc( AkMemPoolId in_poolId, void* in_pAlloc, size_t in_uSize )
{
#ifndef AK_OPTIMIZED
	if ( in_pAlloc && g_settings.pfSizeOfMemory )
		TrackFree( in_poolId, g_settings.pfSizeOfMemory( in_poolId, in_pAlloc ) );
#endif // AK_OPTIMIZED

	//MSTC_HANDLE( in_poolId, in_uSize );

	void* pNewAddress = g_settings.pfRealloc( in_poolId, in_pAlloc, in_uSize );
	if ( !pNewAddress )
	{
#ifndef AK_OPTIMIZED
		if ( in_pAlloc && g_settings.pfSizeOfMemory )
			TrackAllocation( in_poolId, g_settings.pfSizeOfMemory( in_poolId, in_pAlloc ) );
#endif
		ReportAllocationFailure(in_poolId, in_uSize);
		return NULL;
	}

#ifndef AK_OPTIMIZED
	if ( g_settings.pfDebugRealloc )
		g_settings.pfDebugRealloc( in_poolId, in_pAlloc, in_uSize, pNewAddress, NULL, 0 );

	if ( g_settings.pfSizeOfMemory )
		TrackAllocation( in_poolId, g_settings.pfSizeOfMemory( in_poolId, pNewAddress ) );
#endif // AK_OPTIMIZED

#ifdef MSTC_SYSTEMATIC_MEMORY_STRESS
	AkAtomicInc32( &g_MSTC_AllocCounter );
#endif

	return pNewAddress;
}

void Free( AkMemPoolId in_poolId, void* in_pMemAddress )
{
#ifndef AK_OPTIMIZED
	if ( in_pMemAddress )
	{
		if ( g_settings.pfSizeOfMemory )
			TrackFree( in_poolId, g_settings.pfSizeOfMemory( in_poolId, in_pMemAddress ) );

		if ( g_settings.pfDebugFree )
			g_settings.pfDebugFree( in_poolId, in_pMemAddress );
	}
#endif // AK_OPTIMIZED

	g_settings.pfFree( in_poolId, in_pMemAddress );
}

void Falign( AkMemPoolId in_poolId, void* in_pMemAddress )
{
#ifndef AK_OPTIMIZED
	if ( in_pMemAddress )
	{
		if (  g_settings.pfSizeOfMemory )
			TrackFree( in_poolId, g_settings.pfSizeOfMemory( in_poolId, in_pMemAddress ) );

		if ( g_settings.pfDebugFalign )
			g_settings.pfDebugFalign( in_poolId, in_pMemAddress );
	}
#endif // AK_OPTIMIZED

	g_settings.pfFalign( in_poolId, in_pMemAddress );
}

void GetCategoryStats( AkMemPoolId in_poolId, CategoryStats& out_poolStats )
{
#ifndef AK_OPTIMIZED
	in_poolId &= AkMemID_MASK;

	out_poolStats.uUsed = AkAtomicLoad64(&allocationSizes[in_poolId].size);
	out_poolStats.uAllocs = AkAtomicLoad32(&allocCounts[in_poolId].count);
	out_poolStats.uFrees = AkAtomicLoad32(&freeCounts[in_poolId].count);
	out_poolStats.uPeakUsed = AkAtomicLoad64(&peakAllocationSizes[in_poolId].size);
#endif // AK_OPTIMIZED
}

void GetGlobalStats(GlobalStats& out_stats)
{
#ifndef AK_OPTIMIZED
	AkUInt64 uUsed = 0;
	for (int i = 0; i < AkMemID_NUM; ++i)
	{
		uUsed += AkAtomicLoad64(&allocationSizes[i].size);
	}
	out_stats.uUsed = uUsed;

	AkUInt64 uDeviceUsed = 0;
	for (int i = AkMemID_NUM; i < AkMemID_NUM*2; ++i)
	{
		uDeviceUsed += AkAtomicLoad64(&allocationSizes[i].size);
	}
	out_stats.uDeviceUsed = uDeviceUsed;

	out_stats.uReserved = g_settings.pfTotalReservedMemorySize ? g_settings.pfTotalReservedMemorySize() : 0;
	out_stats.uMax = g_settings.uMemAllocationSizeLimit;
#endif // AK_OPTIMIZED
}

void StartProfileThreadUsage()
{
#ifndef AK_OPTIMIZED
	AKASSERT( ( AkThreadID )profileThreadID == AK_NULL_THREAD );
	profileThreadID = AKPLATFORM::CurrentThread();
	AkAtomicExchange64( &profileThreadUsage.size, 0 );
#endif // AK_OPTIMIZED
}

AkUInt64 StopProfileThreadUsage()
{
#ifndef AK_OPTIMIZED
	AKASSERT( ( AkThreadID )profileThreadID == AKPLATFORM::CurrentThread() );
	profileThreadID = AK_NULL_THREAD;
	return AkAtomicExchange64( &profileThreadUsage.size, 0 );
#else
	return 0;
#endif // AK_OPTIMIZED
}

} // namespace MemoryMgr

} // namespace AK
