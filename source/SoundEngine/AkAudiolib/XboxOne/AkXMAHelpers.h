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

#ifndef _AK_XMA_HELPERS_H_
#define _AK_XMA_HELPERS_H_

#include <xma2defs.h>
#include <shapecontexts.h>

#define XMADECODER_FRAME_SIZE_IN_SUBFRAMES 4
#define XMA_PACKET_SIZE     (2048)			// Note. Not defined anywhere in XAudio headers.
#define SUBFRAMES_TO_DECODE 8				// Number of subframes decoded in one pass by the hardware
// Note: There are technically 320 XMA hardware contexts, but the XDK documentation states that a title 
// may only use 316.
#define MAX_XMA_CONTEXTS		(316)

#ifdef _DEBUG
#define INCREMENT_XMA_CONTEXTS()	++AK::XMAHELPERS::g_uNumXMAContexts
#define DECREMENT_XMA_CONTEXTS()	--AK::XMAHELPERS::g_uNumXMAContexts
#define CAN_CREATE_XMA_CONTEXT()	( AK::XMAHELPERS::g_uNumXMAContexts < MAX_XMA_CONTEXTS )
#else
#define INCREMENT_XMA_CONTEXTS()
#define DECREMENT_XMA_CONTEXTS()
#define CAN_CREATE_XMA_CONTEXT()	( true )
#endif

#define AK_MAX_XMASTREAMS 4

#pragma pack(push, 4)
// IMPORTANT! Keep in sync with AudioFileConversion/XMAPlugin
struct AkXMACustHWLoopData
{
	DWORD   LoopStart;          // Loop start offset (in bits).
    DWORD   LoopEnd;            // Loop end offset (in bits).

    // Format for SubframeData: eeee ssss.
    // e: Subframe number of loop end point [0,3].
    // s: Number of subframes to skip before decoding and outputting at the loop start point [1,4].

    BYTE    SubframeData;       // Data for decoding subframes.  See above.
};

struct AkXMA2WaveFormat : public XMA2WAVEFORMATEX
{
	AkXMACustHWLoopData loopData[AK_MAX_XMASTREAMS];
};
#pragma pack(pop)

namespace AK
{
    namespace XMAHELPERS
    {

		// This counter is used to limit the number of XMA contexts in debug.
		// XMAPlaybackCreate() asserts internally when the limit is reached.
#ifdef _DEBUG
		extern AkUInt32 g_uNumXMAContexts;
#endif

        // This helper is a copy of GetXmaFrameBitPosition() implemented in xma2defs.h,
		// with slight modifications.
		// 
		// "GetXmaFrameBitPosition: Calculates the bit offset of a given frame within
		// an XMA block or set of blocks.  Returns 0 on failure."
		//
		// The difference is that it returns the number of frames skipped within the 
		// buffer. This is useful for us, as we may submit a buffer that is smaller 
		// than an XMA block (in streamed sources). In such a case, we use the frame 
		// count that was skipped in order to update the desired frame accordingly for 
		// next attempt.
		__inline AkUInt32 AK_GetXmaFrameBitPosition
		(
			const BYTE* pXmaData,  // Pointer to beginning of the XMA block[s]
			DWORD nXmaDataBytes,   // Size of pXmaData in bytes
			DWORD nStreamIndex,    // Stream within which to seek
			AkUInt32 nDesiredFrame,   // Frame sought
			AkUInt32 & out_nFrameCountSoFar,
			bool & io_bDataOutOfBounds
		)
		{
			const BYTE* pCurrentPacket;
			AkUInt32 nPacketsExamined = 0;
			out_nFrameCountSoFar = 0;	// AK: This value is returned.
			AkUInt32 nFramesToSkip;
			AkUInt32 nFrameBitOffset;

			XMA2DEFS_ASSERT(pXmaData);
			XMA2DEFS_ASSERT(nXmaDataBytes % XMA_BYTES_PER_PACKET == 0);

			// Get the first XMA packet belonging to the desired stream, relying on the
			// fact that the first packets for each stream are in consecutive order at
			// the beginning of an XMA block.
			pCurrentPacket = pXmaData + nStreamIndex * XMA_BYTES_PER_PACKET;
			for (;;)
			{
				// If we have exceeded the size of the XMA data, return failure
				// normal for stream
				if (pCurrentPacket + XMA_BYTES_PER_PACKET > pXmaData + nXmaDataBytes)
				{
					io_bDataOutOfBounds = true;
					break;
				}

				// If the current packet contains the frame we are looking for...

				AkUInt32 packetFrameCount = GetXmaPacketFrameCount(pCurrentPacket);
				if (out_nFrameCountSoFar + packetFrameCount > nDesiredFrame)
				{
					// See how many frames in this packet we need to skip to get to it
					XMA2DEFS_ASSERT(nDesiredFrame >= out_nFrameCountSoFar);
					nFramesToSkip = nDesiredFrame - out_nFrameCountSoFar;

					// AK: Update out_nFrameCountSoFar before we leave.
					out_nFrameCountSoFar = nDesiredFrame;

					// Get the bit offset of the first frame in this packet
					nFrameBitOffset = XMA_PACKET_HEADER_BITS + GetXmaPacketFirstFrameOffsetInBits(pCurrentPacket);

					// Advance nFrameBitOffset to the frame of interest
					while (nFramesToSkip--)
					{
						nFrameBitOffset += GetXmaFrameLengthInBits(pCurrentPacket, nFrameBitOffset);
					}
					
					// The bit offset to return is the number of bits from pXmaData to
					// pCurrentPacket plus the bit offset of the frame of interest
					return (AkUInt32)(pCurrentPacket - pXmaData) * 8 + nFrameBitOffset;
				}

				// If we haven't found the right packet yet, advance our counters
				++nPacketsExamined;
				out_nFrameCountSoFar += GetXmaPacketFrameCount(pCurrentPacket);

				// And skip to the next packet belonging to the same stream
				AkUInt32 skipCount = GetXmaPacketSkipCount(pCurrentPacket) + 1;
				pCurrentPacket += XMA_BYTES_PER_PACKET * skipCount;
			}

			return 0;
		}

        // Parses the XMA data and finds the position to which the decoder should be set provided the total 
        // number of samples.
        // in_pData + io_dwBitOffset must point at the beginning of a packet.
		// Returned io_dwBitOffset (+in_pData) points at the beginning of a frame (to feed to XMAPlaybackSetDecodePosition()).
		// This has a 512 sample granularity. Returned out_uSubframesSkip should also be passed to 
		// XMAPlaybackSetDecodePosition to achieve 128 sample granularity.
		// If the bit offset could not be found in this buffer (out_uSubframesSkip >= 4), the method returns AK_Fail.
		// In such a case, substract the number of samples to skip by the number of subframes that were skipped 
		// (*128) and call this with the next buffer.
		inline AKRESULT FindDecodePosition( 
            AkUInt8 * in_pData,				// Buffer. Must be aligned on an XMA PACKET (every 2KB of the XMA stream).
            AkUInt32 in_uiDataSize,			// Buffer size.
            AkUInt32 & io_uNumSamples,		// Arbitrary number of samples to skip. Returned as the number of samples left to skip (<128 if frame was found within this buffer).
            AkUInt32 in_uNbStreams,
			AkUInt32 * out_dwBitOffset,		// Returned bit offset (relative to in_pData) to start playback (points on a frame header), if frame was found within this buffer.
            AkUInt32 & out_uSubframesSkip,	// Number of subframes to skip in frame pointed by in_pData + io_dwBitOffset (0-3) if frame was found within this buffer.
			bool & out_bDataOutOfBounds )
        { 
			AkUInt32 uDesiredFrame = io_uNumSamples / XMA_SAMPLES_PER_FRAME;
			AkUInt32 * arFrameCountSoFar = (AkUInt32*)AkAlloca(in_uNbStreams*sizeof(AkUInt32));
			AKRESULT eResult = AK_Success;

			AkUInt32 uSubframesSkip = (io_uNumSamples % XMA_SAMPLES_PER_FRAME) / XMA_SAMPLES_PER_SUBFRAME;
			AKASSERT( uSubframesSkip< XMADECODER_FRAME_SIZE_IN_SUBFRAMES );
			AKASSERT( io_uNumSamples >= uSubframesSkip * XMA_SAMPLES_PER_SUBFRAME );
			io_uNumSamples -= uSubframesSkip * XMA_SAMPLES_PER_SUBFRAME;
			out_uSubframesSkip = uSubframesSkip;
			out_bDataOutOfBounds = false;
			for (AkUInt32 i = 0; (i < in_uNbStreams); i++)
			{
				out_dwBitOffset[i] = AK_GetXmaFrameBitPosition(
					in_pData,			// Pointer to beginning of the XMA block[s]
					in_uiDataSize,		// Size of pXmaData in bytes
					i,					// Stream within which to seek
					uDesiredFrame,		// Frame sought
					arFrameCountSoFar[i],
					out_bDataOutOfBounds );

				if ( !out_dwBitOffset[i] )
					return AK_Fail;
				
				AKASSERT( uDesiredFrame >= arFrameCountSoFar[i] 
						&& io_uNumSamples >= arFrameCountSoFar[i] * XMA_SAMPLES_PER_FRAME );
			}
				
			// Frame count should be identical for all streams, otherwise the XMA block size is too large for us to work with.
			for (AkUInt32 i = 1; i < in_uNbStreams; i++)
			{
				if (arFrameCountSoFar[i] != arFrameCountSoFar[i-1])
					return AK_Fail;
			}
			
			// Decrement io_uNumSamples so that it reflects remaining samples to skip (if any) to reach the 
			// desired seek point.
			io_uNumSamples -= ( arFrameCountSoFar[0] * XMA_SAMPLES_PER_FRAME );			

			return eResult;
        }

		inline void MoveLFEBuffer( AkReal32* in_pCurrentDmaBuffer, AkUInt16 uNumSamplesRefill, AkUInt16 in_lfeOriginalPosition, AkUInt16 in_lfeTargetPosition )
		{
			AKASSERT( in_lfeOriginalPosition <= in_lfeTargetPosition );

			AkReal32* temp = (AkReal32*)AkAlloca(uNumSamplesRefill*sizeof(AkReal32)*(in_lfeTargetPosition-in_lfeOriginalPosition));

			// ex: for 7.1, start with "L R C LFE LS RS LB RB"
			// copy LS RS LB RB into temp (LFE position + 1)
			// copy LFE into last position
			// copy temp into LFE original position
			AKPLATFORM::AkMemCpy(temp,															in_pCurrentDmaBuffer+((in_lfeOriginalPosition+1)*uNumSamplesRefill),	uNumSamplesRefill*sizeof(AkReal32)*(in_lfeTargetPosition-in_lfeOriginalPosition));
			AKPLATFORM::AkMemCpy(in_pCurrentDmaBuffer+(in_lfeTargetPosition*uNumSamplesRefill),	in_pCurrentDmaBuffer+(in_lfeOriginalPosition*uNumSamplesRefill),		uNumSamplesRefill*sizeof(AkReal32));
			AKPLATFORM::AkMemCpy(in_pCurrentDmaBuffer+(in_lfeOriginalPosition*uNumSamplesRefill), temp,																	uNumSamplesRefill*sizeof(AkReal32)*(in_lfeTargetPosition-in_lfeOriginalPosition));
		}
   

		//  Determine whether there is sufficient data remaining in the provided XMA buffer for the
		//  specified number of XMA frames (512 samples).
		//	Version 1.1
		inline BOOL GetNumXmaFramesAvailable( DWORD nFramesNeeded, _In_bytecount_(nXmaDataBytes) const BYTE *pXmaData, DWORD nXmaDataBytes, DWORD& nBitOffset)
		{

	#define XMA_PACKET_OFFSET_BITS  14
	#define XMA_PACKET_OFFSET_MASK  ( XMA_BITS_PER_PACKET - 1 )
			
			DWORD nFullFrameCount = 0;

			const DWORD nXmaDataBits = ( nXmaDataBytes * 8u );

			DWORD nPacketIndex = nBitOffset >> XMA_PACKET_OFFSET_BITS;
			DWORD nPacketBitOffset = nBitOffset & XMA_PACKET_OFFSET_MASK;

			const BYTE *pCurrentPacket = pXmaData + ( XMA_BYTES_PER_PACKET * nPacketIndex );

			nBitOffset = (DWORD)( pCurrentPacket - pXmaData ) * 8u + nPacketBitOffset;

			while ( ( nBitOffset < nXmaDataBits ) && ( nFullFrameCount < nFramesNeeded ) )
			{
				DWORD nFrameLength = GetXmaFrameLengthInBits( pCurrentPacket, nPacketBitOffset );

				if ( nFrameLength == XMA_FINAL_FRAME_MARKER )
				{
					pCurrentPacket += XMA_BYTES_PER_PACKET * ( GetXmaPacketSkipCount( pCurrentPacket ) + 1 );
					nPacketBitOffset = XMA_PACKET_HEADER_BITS;
					nBitOffset = (DWORD)( pCurrentPacket - pXmaData ) * 8u + nPacketBitOffset;
				}
				else
				{
					DWORD nNextPacketBitOffset = nPacketBitOffset + nFrameLength;

					if ( nNextPacketBitOffset >= XMA_BITS_PER_PACKET )
					{
						pCurrentPacket += XMA_BYTES_PER_PACKET * ( GetXmaPacketSkipCount( pCurrentPacket ) + 1 );
						nNextPacketBitOffset -= ( XMA_BITS_PER_PACKET - XMA_PACKET_HEADER_BITS );
					}

					nPacketBitOffset = nNextPacketBitOffset;
					nBitOffset = (DWORD)( pCurrentPacket - pXmaData ) * 8u + nPacketBitOffset;
					if ( nBitOffset <= nXmaDataBits )
					{
						++nFullFrameCount;
					}
				}
			}

			if (nBitOffset > nXmaDataBits)
			{
				//bit offset for next buffer relative to end of this one (start of next one)
				nBitOffset -= nXmaDataBits;
			}

			return nFullFrameCount;
		}

	}

}

#endif // _AK_XMA_HELPERS_H_
