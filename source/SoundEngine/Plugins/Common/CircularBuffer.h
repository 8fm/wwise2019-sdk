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

#ifndef _CIRCULARBUFFER_H_
#define _CIRCULARBUFFER_H_

#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>

#include <AK/SoundEngine/Common/AkTypes.h>

namespace DSP
{
	class CAkCircularBuffer
	{
	public:

		CAkCircularBuffer() 
			: m_uSize(0),
			m_uWriteOffset(0), 
			m_uReadOffset(0), 
			m_uFramesReady(0),
			m_pfData( NULL ){}

		AKRESULT Init(	
			AK::IAkPluginMemAlloc *	in_pAllocator,
			AkUInt32 in_uSize );

		void Term( AK::IAkPluginMemAlloc * in_pAllocator );

		void Reset();

		AkUInt32 FramesReady( )
		{
			return m_uFramesReady;
		}

		AkUInt32 FramesEmpty()
		{
			return m_uSize - m_uFramesReady;
		}

		// Invalidates frames that won't need to be read anymore
		// Frees up space for more frames to be pushed later by advancing the read position in the circular buffer
		// Actual number of frames skipped returned
		AkUInt32 AdvanceFrames( AkUInt32 in_uNumFrames );

		AkReal32 * Get( AkUInt32 * out_puDataSize = NULL )
		{
			if ( out_puDataSize )
				*out_puDataSize = AK_ALIGN_SIZE_FOR_DMA(m_uSize*sizeof(AkReal32));
			return m_pfData;
		}

		// Tries to push all frames from a given buffer into circular buffer without overwriting data not yet read.
		// Actual number of frames pushed returned
		AkUInt32 PushFrames( 
			AkReal32 * in_pfBuffer, 
			AkUInt32 in_NumFrames );
		// Tries to fill provided buffer with all frames ready in the circular buffer (if any). 
		// Actual number of frames filled returned.
		// Frees up space for more frames to be pushed later by advancing the read position in the circular buffer
		AkUInt32 PopFrames( 
			AkReal32 * out_pfBuffer, 
			AkUInt32 in_RequestedFrames );
		// Fills the provided buffer if enough valid frames are available
		// Does not advance the read position in case this location is required to be read again
		bool ReadFrameBlock( 
			AkReal32 * out_pfBuffer, 
			AkUInt32 in_uBlockSize,
			bool in_bNoMoreInputData );

	protected:

		// Low-level version that take a pointer to its own data (possibly local storage address on SPU

		// Tries to push all frames from a given buffer into circular buffer without overwriting data not yet read.
		// Actual number of frames pushed returned
		AkUInt32 PushFrames( 
			AkReal32 * in_pfBuffer, 
			AkUInt32 in_NumFrames,
			AkReal32 * io_pfData );
		// Tries to fill provided buffer with all frames ready in the circular buffer (if any). 
		// Actual number of frames filled returned.
		// Frees up space for more frames to be pushed later by advancing the read position in the circular buffer
		AkUInt32 PopFrames( 
			AkReal32 * out_pfBuffer, 
			AkUInt32 in_RequestedFrames,
			AkReal32 * io_pfData );
		// Fills the provided buffer if enough valid frames are available
		// Does not advance the read position in case this location is required to be read again
		bool ReadFrameBlock( 
			AkReal32 * out_pfBuffer, 
			AkUInt32 in_uBlockSize,
			bool in_bNoMoreInputData,
			AkReal32 * io_pfData );
		
	protected:
	
		AkUInt32		m_uSize;
		AkUInt32		m_uWriteOffset;
		AkUInt32		m_uReadOffset;
		AkUInt32		m_uFramesReady;
		AkReal32 *		m_pfData;

#ifdef CIRCULARBUFFER_DEBUGINFO
		AkUInt32		m_uTotalFramesPushed;
		AkUInt32		m_uTotalFramesPopped;
#endif
	};

} // namespace DSP


#endif // _CIRCULARBUFFER_H_
