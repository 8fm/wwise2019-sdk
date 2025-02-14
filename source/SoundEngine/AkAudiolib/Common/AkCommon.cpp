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
// AkCommon.cpp
//
// Implementation of public AkPipelineBuffer structure.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkCommon.h"
#include "AudiolibDefs.h"
#include "AkMath.h"
#include "AkSettings.h"
#include <AK/SoundEngine/Common/AkSoundEngine.h>
extern AkInitSettings g_settings;

#define AK_PIPELINE_BUFFER_FREELIST_MAX_CHANNELS 8

static AkAtomicPtr sBufferFreeListBuckets[ AK_PIPELINE_BUFFER_FREELIST_MAX_CHANNELS ];

void AkPipelineBufferBase::InitFreeListBuckets() {
	AKPLATFORM::AkMemSet( sBufferFreeListBuckets, 0, sizeof( sBufferFreeListBuckets ) );
}

void AkPipelineBufferBase::ClearFreeListBuckets() {
	for ( AkInt32 i = 0; i < AK_PIPELINE_BUFFER_FREELIST_MAX_CHANNELS; ++i ) {
		void * pBuffer = ( void* )sBufferFreeListBuckets[ i ];
		while ( pBuffer ) {
			void* pNextBuffer = ( void * )*( uintptr_t * )pBuffer;
			AkFalign( AkMemID_Processing, pBuffer );
			pBuffer = pNextBuffer;
		}
		sBufferFreeListBuckets[ i ] = NULL;
	}
}

AKRESULT AkPipelineBufferBase::GetCachedBuffer( )
{
	AkUInt32 uNumChannels = NumChannels();
	AKASSERT( !pData || !"When the buffer is consumed, it must be set to null" );
	AKASSERT( uNumChannels || !"Channel mask must be set before allocating audio buffer" );
	
	if (uMaxFrames < AkAudioLibSettings::g_uNumSamplesPerFrame)
	{
		uMaxFrames = (AkUInt16)AkAudioLibSettings::g_uNumSamplesPerFrame;
	}

	void * pBuffer = NULL;

	if ( uMaxFrames == AkAudioLibSettings::g_uNumSamplesPerFrame && uNumChannels <= AK_PIPELINE_BUFFER_FREELIST_MAX_CHANNELS ) {
		AkUInt32 uFreeListBucketIndex = uNumChannels - 1;
		AkAtomicPtr * pBucketFreeListHead = &sBufferFreeListBuckets[ uFreeListBucketIndex ];

		for ( ; ; ) {
			void * pFreeBuffer = AkAtomicLoadPtr( pBucketFreeListHead );
			if ( !pFreeBuffer ) {
				break;
			}
			void* pNextBuffer = ( void* )*( uintptr_t* )AkAtomicLoadPtr( ( AkAtomicPtr * )&pFreeBuffer );
			if ( AkAtomicCasPtr( pBucketFreeListHead, pNextBuffer, pFreeBuffer ) ) {
				pBuffer = pFreeBuffer;
				break;
			}
		}
	}

	if ( !pBuffer ) {
		AkUInt32 uAllocSize = uMaxFrames * sizeof( AkReal32 ) * uNumChannels;
		pBuffer = AkMalign( AkMemID_Processing, uAllocSize, AK_BUFFER_ALIGNMENT );
		if ( !pBuffer ) {
			return AK_InsufficientMemory;
		}
	}

	pData = pBuffer;
	uValidFrames = 0;
	return AK_Success;
}

void AkPipelineBufferBase::ReleaseCachedBuffer()
{
	AkUInt32 uNumChannels = NumChannels();
	AKASSERT( pData && uNumChannels > 0 );
	if (uMaxFrames == AkAudioLibSettings::g_uNumSamplesPerFrame && uNumChannels <= AK_PIPELINE_BUFFER_FREELIST_MAX_CHANNELS ) {
		AkUInt32 uFreeListBucketIndex = uNumChannels - 1;
		AkAtomicPtr * pBucketFreeListHead = &sBufferFreeListBuckets[ uFreeListBucketIndex ];

		for ( ; ; ) {
			void * pNextBuffer = AkAtomicLoadPtr( pBucketFreeListHead );
			AkAtomicStorePtr( ( AkAtomicPtr * )pData, pNextBuffer );
			if ( AkAtomicCasPtr( pBucketFreeListHead, pData, pNextBuffer ) ) {
				break;
			}
		}
	} else {
		AkFalign( AkMemID_Processing, pData );
	}

	pData = NULL;
	uMaxFrames = 0;
}


bool AkAudioBuffer::CheckValidSamples()
{
	// Zero out all channels.
	const AkUInt32 uNumChannels = NumChannels();
	for (AkUInt32 i = 0; i < uNumChannels; ++i)
	{
		AkSampleType * AK_RESTRICT pfChan = GetChannel(i);
		if (pfChan)
		{
			for (AkUInt32 j = 0; j < uValidFrames; j++)
			{
				AkSampleType fSample = *pfChan++;
				if (fSample > g_settings.fDebugOutOfRangeLimit ||
					fSample < -g_settings.fDebugOutOfRangeLimit ||
					AkMath::IsNotFinite(fSample))
					return false;
			}
		}
	}

	return true;
}
AkUInt8 AkPipelineChannelMappingFunc(const AkChannelConfig &config, AkUInt8 idx)
{
	return idx;
}
AkUInt8 AkWaveChannelMappingFunc(const AkChannelConfig &config, AkUInt8 idx)
{
	return (AkUInt8)AkAudioBuffer::StandardToPipelineIndex(config, idx);
}
AkUInt8 AkVorbisChannelMappingFunc(const AkChannelConfig &config, AkUInt8 idx)
{
	static unsigned char vorbis_mapping_table[8][8] =
	{
		{0},                     // 1.0 mono
		{0, 1},                  // 2.0 L,R
		{0, 2, 1},               // 3.0 L,C,R
		{0, 1, 2, 3},            // 4.0 FL,FR,RL,RR
		{0, 2, 1, 3, 4},         // 5.0 FL,C,FR,RL,RR
		{0, 2, 1, 3, 4, 5},      // 5.1 FL,C,FR,RL,RR,LFE
		{0, 2, 1, 3, 4, 5, 6},   // 6.1 FL,C,FR,SL,SR,RC,LFE
		{0, 2, 1, 5, 6, 3, 4, 7} // 7.1 FL,C,FR,SL,SR,RL,RR,LFE
	};
	AKASSERT(idx < 8 || !"Vorbis channel ordering is limited to 8 channels!");
	return vorbis_mapping_table[config.uNumChannels - 1][idx];
}
