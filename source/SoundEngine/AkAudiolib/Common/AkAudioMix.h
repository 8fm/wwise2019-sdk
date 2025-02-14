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

#include "AkSpeakerVolumesEx.h"

//-----------------------------------------------------------------------------
// Name: struct AkAudioMix
// Desc: multi-channel volume distributions used for mixing.
//-----------------------------------------------------------------------------
struct AkAudioMix
{
public:
	AkAudioMix() 
		: pMemory( NULL )
		, uAllocdMemSize(0)
		, pNext( NULL )
		, pPrevious( NULL )
	{}

	AkAudioMix( void * in_pMem, AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut ) 
		: pMemory( in_pMem )
		, uAllocdMemSize( 0 ) //<- 0 because we dont own memory.
		, pNext( (AK::SpeakerVolumes::MatrixPtr)in_pMem )
		, pPrevious( (AK::SpeakerVolumes::MatrixPtr)( (AkUInt8*)in_pMem + AK::SpeakerVolumes::Matrix::GetRequiredSize( in_uNumChannelsIn, in_uNumChannelsOut ) ) )
	{}

	static inline AkUInt32 GetRequiredSize( AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut ) 
	{
		return 2 * AK::SpeakerVolumes::Matrix::GetRequiredSize( in_uNumChannelsIn, in_uNumChannelsOut );
	}

	// Get channel matrix.
	AkForceInline AK::SpeakerVolumes::ConstMatrixPtr Next() const { return pNext; }
	AkForceInline AK::SpeakerVolumes::ConstMatrixPtr Prev() const { return pPrevious; }
	AkForceInline AK::SpeakerVolumes::MatrixPtr Next() { return pNext; }
	AkForceInline AK::SpeakerVolumes::MatrixPtr Prev() { return pPrevious; }

	AkForceInline void ClearNext( AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut )
	{
		AK::SpeakerVolumes::Matrix::Zero( pNext, in_uNumChannelsIn, in_uNumChannelsOut );
	}
	AkForceInline void ClearPrevious( AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut )
	{
		AK::SpeakerVolumes::Matrix::Zero( pPrevious, in_uNumChannelsIn, in_uNumChannelsOut );
	}

	AkForceInline void Swap( 
		AkUInt32, AkUInt32
		)
	{
		// Move pointers instead of copying data. 
		AK::SpeakerVolumes::MatrixPtr pTemp = pNext;
		pNext = pPrevious;
		pPrevious = pTemp;
	}

	AkForceInline void CopyPrevToNext( AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut )
	{
		AK::SpeakerVolumes::Matrix::Copy( pNext, pPrevious, in_uNumChannelsIn, in_uNumChannelsOut );
	}
	
	AkForceInline void CopyNextToPrev( AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut )
	{
		AK::SpeakerVolumes::Matrix::Copy( pPrevious, pNext, in_uNumChannelsIn, in_uNumChannelsOut );
	}

	AkForceInline bool IsAllocated() const { return ( pMemory != NULL ); }
	AkForceInline void * GetMemory() { return pMemory; }
	AKRESULT Allocate( AkUInt32 in_uNumChannelsIn, AkUInt32 in_uNumChannelsOut );
	void Free();

private:
	// all values : [0, 1]
	void * pMemory;
	AkUInt32 uAllocdMemSize;
	AK::SpeakerVolumes::MatrixPtr pNext;
	AK::SpeakerVolumes::MatrixPtr pPrevious;

};
