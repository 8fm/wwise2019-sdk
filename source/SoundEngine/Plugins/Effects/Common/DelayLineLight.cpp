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

// Delay that must be given a pointer to it's memory, useful for effects that have a process class. 
// Length is expected to be 4 aligned for it to work 

#include "DelayLineLight.h"
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include <AK/SoundEngine/Common/AkSimd.h>
#include <AK/Tools/Common/AkAssert.h>

namespace DSP
{		
	AKRESULT CDelayLight::Init( AK::IAkPluginMemAlloc * in_pAllocator, AkUInt32 in_uDelayLineLength )
	{
		m_uDelayLineLength = in_uDelayLineLength/4*4; 
		if ( m_uDelayLineLength )
		{
			m_pfDelay = (AkReal32*)AK_PLUGIN_ALLOC( in_pAllocator, AK_ALIGN_SIZE_FOR_DMA( sizeof(AkReal32) * m_uDelayLineLength ) );
			if ( m_pfDelay == NULL )
				return AK_InsufficientMemory;
		}

		m_uCurrOffset = 0;
		return AK_Success;
	}

	void CDelayLight::Term( AK::IAkPluginMemAlloc * in_pAllocator )
	{
		if ( m_pfDelay )
		{
			AK_PLUGIN_FREE( in_pAllocator, m_pfDelay );
			m_pfDelay = NULL;
		}
		m_uDelayLineLength = 0;
	}

	void CDelayLight::Reset( )
		{
			if (m_pfDelay)
				AkZeroMemLarge( (void*) m_pfDelay, m_uDelayLineLength*sizeof(AkReal32) );
			m_uCurrOffset = 0;
		}

	// In-place
	void CDelayLight::ProcessBuffer( AkReal32 * io_pfInOutBuf, AkUInt32 in_uNumFrames )
	{
		if ( !m_pfDelay ) // we have no delay line, bail out
			return;

		AKASSERT( (in_uNumFrames % 4) == 0 );	// must be a multiple of 4 and larger than 0
		AKSIMD_V4F32 * AK_RESTRICT pvDelayPtr = (AKSIMD_V4F32 * ) (&m_pfDelay[m_uCurrOffset]);
		AKSIMD_V4F32 * AK_RESTRICT pvIOPtr = (AKSIMD_V4F32 * ) io_pfInOutBuf;
		AkUInt32 uFramesBeforeWrap = m_uDelayLineLength - m_uCurrOffset;
		if ( uFramesBeforeWrap > in_uNumFrames )
		{
			// Not wrapping this time
			for (AkUInt32 uVectors = 0; uVectors < in_uNumFrames/4 ; uVectors++)
			{
				AKSIMD_V4F32 vIn = AKSIMD_LOAD_V4F32((AkReal32*) pvIOPtr);
				AKSIMD_V4F32 vDelay = AKSIMD_LOAD_V4F32((AkReal32*) pvDelayPtr);
				AKSIMD_STORE_V4F32((AkReal32*) pvDelayPtr, vIn);
				++pvDelayPtr;
				AKSIMD_STORE_V4F32((AkReal32*) pvIOPtr, vDelay);
				++pvIOPtr;
			}
			m_uCurrOffset += in_uNumFrames;
			AKASSERT( m_uCurrOffset < m_uDelayLineLength );
		}
		else
		{
			AkUInt32 uVectorsRemainingToProcess = in_uNumFrames/4;
			AkUInt32 uVectorsBeforeWrap = uFramesBeforeWrap/4;
			while ( uVectorsRemainingToProcess )
			{
				AkUInt32 uVectorsToProcess = AkMin(uVectorsRemainingToProcess,uVectorsBeforeWrap);
				AkUInt32 i = uVectorsToProcess;
				
				while( i > 0 )
				{
					AKSIMD_V4F32 vIn = AKSIMD_LOAD_V4F32((AkReal32*) pvIOPtr);
					AKSIMD_V4F32 vDelay = AKSIMD_LOAD_V4F32((AkReal32*) pvDelayPtr);
					AKSIMD_STORE_V4F32((AkReal32*) pvDelayPtr, vIn);
					++pvDelayPtr;
					AKSIMD_STORE_V4F32((AkReal32*) pvIOPtr, vDelay);
					++pvIOPtr;
					--i;
				}

				m_uCurrOffset += uVectorsToProcess * 4;	

				if ( m_uCurrOffset == m_uDelayLineLength )
				{
					pvDelayPtr = (AKSIMD_V4F32 * ) m_pfDelay;
					m_uCurrOffset = 0;
				}
				AKASSERT( m_uCurrOffset < m_uDelayLineLength );

				uVectorsRemainingToProcess -= uVectorsToProcess;
				uVectorsBeforeWrap = (m_uDelayLineLength - m_uCurrOffset)/4;
			}
		}
	}	

	// Out of place
	void CDelayLight::ProcessBuffer( AkReal32 * in_pfInBuf, AkReal32 * out_pfOutBuf, AkUInt32 in_uNumFrames )
	{
		if ( !m_pfDelay ) // we have no delay line, bail out
			return;

		AKASSERT( (in_uNumFrames % 4) == 0 );	// must be a multiple of 4 and larger than 0
		AKSIMD_V4F32 * AK_RESTRICT pvDelayPtr = (AKSIMD_V4F32 * ) (&m_pfDelay[m_uCurrOffset]);
		AKSIMD_V4F32 * AK_RESTRICT pvInPtr = (AKSIMD_V4F32 * ) in_pfInBuf;
		AKSIMD_V4F32 * AK_RESTRICT pvOutPtr = (AKSIMD_V4F32 * ) out_pfOutBuf;
		AkUInt32 uFramesBeforeWrap = m_uDelayLineLength - m_uCurrOffset;
		if ( uFramesBeforeWrap > in_uNumFrames )
		{
			// Not wrapping this time
			for (AkUInt32 uVectors = 0; uVectors < in_uNumFrames/4 ; uVectors++)
			{
				AKSIMD_V4F32 vIn = AKSIMD_LOAD_V4F32((AkReal32*) pvInPtr++);
				AKSIMD_V4F32 vDelay = AKSIMD_LOAD_V4F32((AkReal32*) pvDelayPtr);
				AKSIMD_STORE_V4F32((AkReal32*) pvDelayPtr, vIn);
				++pvDelayPtr;
				AKSIMD_STORE_V4F32((AkReal32*) pvOutPtr, vDelay);
				++pvOutPtr;
			}
			m_uCurrOffset += in_uNumFrames;
			AKASSERT( m_uCurrOffset < m_uDelayLineLength );
		}
		else
		{
			AkUInt32 uVectorsRemainingToProcess = in_uNumFrames/4;
			AkUInt32 uVectorsBeforeWrap = uFramesBeforeWrap/4;
			while ( uVectorsRemainingToProcess )
			{
				AkUInt32 uVectorsToProcess = AkMin(uVectorsRemainingToProcess,uVectorsBeforeWrap);
				AkUInt32 i = uVectorsToProcess;
				
				while( i > 0 )
				{
					AKSIMD_V4F32 vIn = AKSIMD_LOAD_V4F32((AkReal32*) pvInPtr++);
					AKSIMD_V4F32 vDelay = AKSIMD_LOAD_V4F32((AkReal32*) pvDelayPtr);
					AKSIMD_STORE_V4F32((AkReal32*) pvDelayPtr, vIn);
					++pvDelayPtr;
					AKSIMD_STORE_V4F32((AkReal32*) pvOutPtr, vDelay);		
					++pvOutPtr;
					--i;
				}

				m_uCurrOffset += uVectorsToProcess * 4;	

				if ( m_uCurrOffset == m_uDelayLineLength )
				{
					pvDelayPtr = (AKSIMD_V4F32 * ) m_pfDelay;
					m_uCurrOffset = 0;
				}
				AKASSERT( m_uCurrOffset < m_uDelayLineLength );

				uVectorsRemainingToProcess -= uVectorsToProcess;
				uVectorsBeforeWrap = (m_uDelayLineLength - m_uCurrOffset)/4;
			}
		}
	}	
} // namespace DSP
