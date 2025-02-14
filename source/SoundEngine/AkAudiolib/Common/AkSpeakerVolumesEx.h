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
// AkSpeakerVolumesEx.h
// 
// Extends public AkSpeakerVolumes with helpers for performing a few 
// optimized (if applicable) math operations, as well as a consistent 
// memory management.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <AK/SoundEngine/Common/AkSimd.h>
#include <AK/SoundEngine/Common/AkSpeakerVolumes.h>
#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/Tools/Common/AkObject.h>
#include "AkMath.h"

#if defined(AK_SPEAKER_VOLUMES_OVERRIDE)

#include "AkPlatformSpeakerVolumes.h"

#elif defined(AKSIMD_SPEAKER_VOLUME)		// This platform uses the default AKSIMD implementation and requires aligned allocations

// Extend AK::SpeakerVolumes
namespace AK
{
namespace SpeakerVolumes
{
	// Raw allocation on heap. Pass in the memory size obtained from a call to GetRequiredSize().
	AkForceInline AkReal32 * HeapAlloc(AkUInt32 in_uSize, AkMemPoolId in_poolId)
	{
		// Allocate with AK_SIMD_ALIGNMENT in order to be compatible with functions using AkSIMD herein.
		return (AkReal32*)AkMalign(in_poolId, in_uSize, AK_SIMD_ALIGNMENT);
	}
	
	AkForceInline void HeapFree(void * in_pVolumes, AkMemPoolId in_poolId)
	{
		if ( in_pVolumes )
		{
			AkFalign(in_poolId, in_pVolumes);
		}
	}

	namespace Vector
	{
		// Allocation of a volume vector on heap. 
		AkForceInline VectorPtr Allocate(AkUInt32 in_uNumChannels, AkMemPoolId in_poolId = AkMemID_Processing)
		{
			return (VectorPtr)AkMalign(in_poolId, GetRequiredSize(in_uNumChannels), AK_SIMD_ALIGNMENT);
		}

		AkForceInline void Free(VectorPtr& io_pVolumes, AkMemPoolId in_poolId = AkMemID_Processing)
		{
			if ( io_pVolumes )
			{
				AkFalign(in_poolId, io_pVolumes);
				io_pVolumes = NULL;
			}
		}

		AkForceInline void Sqrt( VectorPtr in_pVolumes, AkUInt32 in_uNumChannels )
		{
			AKASSERT( in_pVolumes || in_uNumChannels == 0 );
#ifdef AKSIMD_V4F32_SUPPORTED
			AKSIMD_V4F32 * pVolumes = (AKSIMD_V4F32*)in_pVolumes;
			AKSIMD_V4F32 * pVolumesEnd = pVolumes + GetNumV4F32( in_uNumChannels );
			while ( pVolumes < pVolumesEnd )
			{
				AKSIMD_V4F32 vValue = AKSIMD_LOAD_V4F32( pVolumes );
				vValue = AKSIMD_SQRT_V4F32( vValue );
				AKSIMD_STORE_V4F32( pVolumes, vValue );
				++pVolumes;
			}
#elif defined (AKSIMD_V2F32_SUPPORTED)
			AKSIMD_V2F32 * pVolumes = (AKSIMD_V2F32*)in_pVolumes;
			AKSIMD_V2F32 * pVolumesEnd = pVolumes + GetNumV2F32( in_uNumChannels );
			while ( pVolumes < pVolumesEnd )
			{
				AKSIMD_V2F32 vValue = AKSIMD_LOAD_V2F32( pVolumes );
				vValue = AKSIMD_SQRT_V2F32( vValue );
				AKSIMD_STORE_V2F32( pVolumes, vValue );
				++pVolumes;
			}
#else
	#error Should use scalar implementation.
#endif
		}
		
		AkForceInline void dBToLin( AK::SpeakerVolumes::VectorPtr in_pVolumes, AkUInt32 in_uNumChannels )
		{
			AKASSERT( in_pVolumes || in_uNumChannels == 0 );
			for ( AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++ )
			{
				in_pVolumes[uChan] = AkMath::dBToLin( in_pVolumes[uChan] );
			}
		}

		AkForceInline void FastLinTodB( AK::SpeakerVolumes::VectorPtr in_pVolumes, AkUInt32 in_uNumChannels )
		{
			AKASSERT( in_pVolumes || in_uNumChannels == 0 );
			for ( AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++ )
			{
				in_pVolumes[uChan] = AkMath::FastLinTodB( in_pVolumes[uChan] );
			}
		}
	}

	// 2D
	namespace Matrix
	{
		AkForceInline MatrixPtr Allocate(AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut, AkMemPoolId in_poolId = AkMemID_Processing)
		{
			// Allocate with AK_SIMD_ALIGNMENT in order to be compatible with functions using AkSIMD herein.
			return (MatrixPtr)AkMalign(in_poolId, GetRequiredSize(in_uNumChannelsIn, in_uNumChannelsOut), AK_SIMD_ALIGNMENT);
		}

		AkForceInline void Free(MatrixPtr& io_pVolumes, AkMemPoolId in_poolId = AkMemID_Processing)
		{
			if ( io_pVolumes )
			{
				AkFalign(in_poolId, io_pVolumes);
				io_pVolumes = NULL;
			}
		}
	}
}
}

#else	// This platform uses the default scalar implementation

// Extend AK::SpeakerVolumes
namespace AK
{
namespace SpeakerVolumes
{
	// Raw allocation on heap. Pass in the memory size obtained from a call to GetRequiredSize().
	AkForceInline AkReal32 * HeapAlloc( AkUInt32 in_uSize, AkMemPoolId in_poolId ) 
	{
		return (AkReal32*)AkAlloc( in_poolId, in_uSize );
	}
	
	AkForceInline void HeapFree( void * in_pVolumes, AkMemPoolId in_poolId ) 
	{
		if ( in_pVolumes )
		{
			AkFree( in_poolId, in_pVolumes );
		}
	}

	namespace Vector
	{
		// Allocation of a volume vector on heap. 
		AkForceInline VectorPtr Allocate( AkUInt32 in_uNumChannels, AkMemPoolId in_poolId = AkMemID_Processing ) 
		{
			return (VectorPtr)AkAlloc( in_poolId, GetRequiredSize( in_uNumChannels ) );
		}

		AkForceInline void Free( VectorPtr& io_pVolumes, AkMemPoolId in_poolId = AkMemID_Processing ) 
		{
			if ( io_pVolumes )
			{
				AkFree( in_poolId, io_pVolumes );
				io_pVolumes = NULL;
			}
		}

		AkForceInline void Sqrt( VectorPtr in_pVolumes, AkUInt32 in_uNumChannels )
		{
			AKASSERT( in_pVolumes || in_uNumChannels == 0 );
			for ( AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++ )
			{
				in_pVolumes[uChan] = sqrtf( in_pVolumes[uChan] );
			}
		}

		AkForceInline void dBToLin( AK::SpeakerVolumes::VectorPtr in_pVolumes, AkUInt32 in_uNumChannels )
		{
			AKASSERT( in_pVolumes || in_uNumChannels == 0 );
			for ( AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++ )
			{
				in_pVolumes[uChan] = AkMath::dBToLin( in_pVolumes[uChan] );
			}
		}

		AkForceInline void FastLinTodB( AK::SpeakerVolumes::VectorPtr in_pVolumes, AkUInt32 in_uNumChannels )
		{
			AKASSERT( in_pVolumes || in_uNumChannels == 0 );
			for ( AkUInt32 uChan = 0; uChan < in_uNumChannels; uChan++ )
			{
				in_pVolumes[uChan] = AkMath::FastLinTodB( in_pVolumes[uChan] );
			}
		}
	}

	// 2D
	namespace Matrix
	{
		AkForceInline MatrixPtr Allocate( AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut, AkMemPoolId in_poolId = AkMemID_Processing ) 
		{
			return (MatrixPtr)AkAlloc( in_poolId, GetRequiredSize( in_uNumChannelsIn, in_uNumChannelsOut ) );
		}

		AkForceInline void Free( MatrixPtr& io_pVolumes, AkMemPoolId in_poolId = AkMemID_Processing ) 
		{
			if ( io_pVolumes )
			{
				AkFree( in_poolId, io_pVolumes );
				io_pVolumes = NULL;
			}
		}
	}
}
}

#endif
