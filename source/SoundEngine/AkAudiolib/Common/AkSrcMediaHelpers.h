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

#pragma once

#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/SoundEngine/Common/IAkStreamMgr.h>
#include "AkCommon.h"
#include "AkFileParserBase.h"
#include "AkSrcMediaStream.h"

#define AK_CODEC_ATTR_HARDWARE       (1 << 0)   // Codec uses an (asynchronous) hardware decoding unit
#define AK_CODEC_ATTR_RELOCATE_PITCH (1 << 1)   // When relocating the media for this codec, pitch node buffers must also be relocated
#define AK_CODEC_ATTR_PITCH          (1 << 2)   // Codec can resample without the help of the pitch node
#define AK_CODEC_ATTR_SA_TRANS       (1 << 3)   // Codec can handle sample-accurate transitions without the help of the pitch node
#define AK_CODEC_ATTR_HARDWARE_LL    (1 << 4)   // Codec uses an (asynchronous) hardware decoding unit AND must not incur a frame of latency

namespace AK
{
	namespace SrcMedia
	{
		enum ChannelOrdering
		{
			ChannelOrdering_Auto = 0,     // Auto means: Wave for int16 buffers, Runtime for float buffers
			ChannelOrdering_Pipeline = 1, // Save as Wave with the LFE shifted to the end
			ChannelOrdering_Wave = 2,
			ChannelOrdering_Vorbis = 3    // Vorbis channel mapping family 1: L-C-R-SL-SR-RL-RR-LFE
#define AK_SRCMEDIA_CHANNEL_ORDERING_NUM_STORAGE_BIT 3
		};

		struct Header
		{
			AkFileParser::FormatInfo FormatInfo; // WAVE format info

			AkUInt32     uDataSize;         // Size of media data
			AkUInt32     uDataOffset;       // Offset of media data from file start

			AkUInt32     uPCMLoopStart;     // Loop start frame number (at the source sample rate)
			AkUInt32     uPCMLoopEnd;       // Frame number of last frame to play before looping (at the source sample rate)

			AkFileParser::AnalysisDataChunk AnalysisData;
			AkFileParser::SeekInfo          SeekInfo;
		};

		struct CodecInfo
		{
			AkAudioFormat format;           // Format of PCM data that will be returned by this codec

			ChannelOrdering eOrdering;      // Ordering of channels in the codec output

			AkUInt32 uSourceSampleRate;     // Sample rate of the SOURCE (prior to any resampling performed by the codec)
			AkUInt32 uTotalSamples;         // Total number of samples in PCM.

			AkUInt32 uMinimalBufferSize;    // Minimum granularity of a *stream* buffer
			AkAutoStmHeuristics heuristics; // Heuristics to influence behavior of the media stream

			AkUInt16 uAttributes;           // Flags documenting the behavior of certain codecs
		};

		namespace Position
		{
			struct State {
				AkUInt32     uCurSample;        // Current PCM sample position
				AkUInt32     uTotalSamples;     // Total number of samples in PCM.

				AkUInt32     uPCMLoopStart;     // Loop start frame number (at the source sample rate)
				AkUInt32     uPCMLoopEnd;       // Frame number of last frame to play before looping (at the source sample rate)

				AkUInt16     uLoopCnt;          // Number of remaining loops.
			};

			void Init(State* pState, const Header &header, const CodecInfo &codec, AkUInt16 uLoopCnt);

			inline bool WillLoop(State *pState)
			{
				return pState->uLoopCnt != LOOPING_ONE_SHOT;
			}

			inline void ClampFrames(State *pState, AkUInt32 &io_uFrames)
			{
				AkUInt32 uLimit = WillLoop(pState) ? pState->uPCMLoopEnd + 1 : pState->uTotalSamples;
				if (pState->uCurSample + io_uFrames > uLimit)
				{
					AKASSERT(uLimit > pState->uCurSample);
					io_uFrames = uLimit - pState->uCurSample;
				}
			}

			AKRESULT Forward(State* pState, AkUInt32 in_uFrames, bool &out_bLoop);
		}

		namespace EnvelopeAnalyzer
		{
			struct State {
				AkUInt32 uLastEnvelopePtIdx;    // Index of last envelope point (optimizes search).
			};

			inline void Init(State *pState)
			{
				pState->uLastEnvelopePtIdx = 0;
			}

			AkReal32 GetEnvelope(State* pState, AkFileParser::AnalysisData * in_pAnalysisData, AkUInt32 in_uCurSample);
		}

		namespace ResamplingRamp
		{
			struct State {
				AkReal32 fSampleRateConvertRatio;
				AkReal32 fLastRatio;
				AkReal32 fTargetRatio;
			};
			void Init(State* pState, AkUInt32 uSrcSampleRate);

			inline AkReal32 PitchToRatio(AkReal32 fPitch)
			{
				return powf(2.f, fPitch / 1200.f);
			}

			inline void SetRampTargetPitch(State* pState, AkReal32 fTargetPitch)
			{
				pState->fTargetRatio = pState->fSampleRateConvertRatio * PitchToRatio(fTargetPitch);
			}

			inline AkReal32 RampStart(State* pState)
			{
				return pState->fLastRatio;
			}

			/// Compute the ratio step increments of individual steps in the ramp
			inline AkReal32 RampStep(State* pState, AkUInt32 uNumSteps)
			{
				AKASSERT(uNumSteps != 0);
				return (pState->fTargetRatio - pState->fLastRatio) / uNumSteps;
			}

			/// Compute the maximum number of src frames needed to obtain the specified number of target frames
			inline AkUInt32 MaxSrcFrames(State* pState, AkUInt32 uTargetFrames)
			{
				AkReal32 fWorstCaseRatio = AkMax(pState->fLastRatio, pState->fTargetRatio);
				return (AkUInt32)ceil(uTargetFrames * fWorstCaseRatio);
			}

			/// Compute the maximum number of resampled frames that will be output if given this many src frames for the ramp
			inline AkUInt32 MaxOutputFrames(State* pState, AkUInt32 uSrcFrames)
			{
				AkReal32 fWorstCaseRatio = AkMin(pState->fLastRatio, pState->fTargetRatio);
				return (AkUInt32)ceil(uSrcFrames / fWorstCaseRatio);
			}
		}

		AkReal32 Duration(AkUInt32 uTotalSamples, AkUInt32 uSampleRate, AkUInt32 uPCMLoopStart, AkUInt32 uPCMLoopEnd, AkUInt16 uNumLoops);

		inline AkReal32 DurationNoLoop(AkUInt32 uTotalSamples, AkUInt32 uSampleRate)
		{
			return (uTotalSamples * 1000.f) / (AkReal32)uSampleRate;	// mSec.
		}

		/// Helper: Converts an absolute source position (which takes looping region into account) into a value that is relative to the beginning of the file, and the number of loops remaining.
		void AbsoluteToRelativeSourceOffset(
			AkUInt32 in_uAbsoluteSourcePosition,     // Frame offset with looping unrolled
			AkUInt32 in_uLoopStart,                  // Frame offset of the loop start point
			AkUInt32 in_uLoopEnd,                    // Frame offset of the loop end point (inclusive)
			AkUInt16 in_usLoopCount,                 // How many times the source was set to loop originally
			AkUInt32 & out_uRelativeSourceOffset,    // Frame offset of the source after rolling over loop points to reach absolute offset
			AkUInt16 & out_uRemainingLoops           // Loops remaining after rolling over loop points to reach absolute offset
		);

		struct WEMVBRStitcherState
		{
			static inline void LoopToStartPacket() {};//Nothing todo
			static inline AkUInt16 GetNextPacketSize(void* in_pData, AkUInt32 in_uAvailable)
			{
				return in_uAvailable >= 2 ? (*(AkUInt16*)in_pData + 2) : 2;//Normally includes the 2 bytes of the length too
			}

			static inline AkUInt16 GetMinSize() { return 2; }

			//This policy doesn't have any state.
		};

		struct ManualStitcherState
		{
			static inline void LoopToStartPacket() {};//Nothing todo
			inline AkUInt16 GetNextPacketSize(void* in_pData, AkUInt32 in_uAvailable)
			{
				return uNextPacketSize;
			}

			static inline AkUInt16 GetMinSize() { return 0; }
			
			AkUInt16 uNextPacketSize;
		};

		// GetNextPacket ensures that there is at least one full packet. There may be more data to process though.
		template <class PACKET_STATE>
		class PacketStitcher
		{
		public:
			PacketStitcher()
				: m_pPacketData (nullptr)
				, m_uPacketDataGathered(0)
				, m_uAllocated (0)
				, m_bStitching (false)
			{}

			AKRESULT GetNextPacket(AK::SrcMedia::Stream::State* pState, void *&out_pPacket, AkUInt32& out_uSize)
			{
				while (true)
				{
					if (pState->uSizeLeft == 0)
					{
						// If we don't have data unconsumed and we will never get any more we are done decoding
						if (HasNoMoreStreamData(pState))
							return AK_NoMoreData;

						// See if we can get more data from stream manager.
						ReleaseStreamBuffer(pState);
						AKRESULT eStmResult = FetchStreamBuffer(pState);
						if (eStmResult != AK_DataReady)
							return eStmResult;

						if (pState->uCurFileOffset == pState->uLoopStart)
						{
							m_PacketState.LoopToStartPacket();
						}
					}

					AkUInt8* pData = pState->pNextAddress;
					AkUInt32 uAvailable = pState->uSizeLeft;
					if (AK_EXPECT_FALSE(m_bStitching))
					{
						pData = m_pPacketData;
						if(m_uPacketDataGathered < m_PacketState.GetMinSize())
						{
							AKASSERT(m_uAllocated >= m_PacketState.GetMinSize());
							AkUInt32 uSize = m_PacketState.GetMinSize() - m_uPacketDataGathered;
							memcpy(pData + m_uPacketDataGathered, pState->pNextAddress, uSize);
							ConsumeData(pState, uSize);
							m_uPacketDataGathered += uSize;
						}
						uAvailable = m_uPacketDataGathered;
					}

					AkUInt32 packetSize = m_PacketState.GetNextPacketSize(pData, uAvailable);

					// Gather packet data
					if ((pState->uSizeLeft < packetSize && !m_bStitching) || (m_bStitching && m_uAllocated < packetSize))
					{
						//We'll need to stitch, allocate a stitch buffer.
						if (m_pPacketData)
							m_pPacketData = (AkUInt8*)AkRealloc(AkMemID_Processing, m_pPacketData, packetSize);
						else
							m_pPacketData = (AkUInt8*)AkAlloc(AkMemID_Processing, packetSize);

						if (m_pPacketData == NULL)
						{
							return AK_InsufficientMemory;
						}
						m_bStitching = true;
						m_uAllocated = packetSize;
					}

					// Accumulate data into packet
					AkUInt32 uCopySize = AkMin(pState->uSizeLeft, packetSize - m_uPacketDataGathered);
					if (m_bStitching)
						memcpy(m_pPacketData + m_uPacketDataGathered, pState->pNextAddress, uCopySize);
					else
						m_pPacketData = pState->pNextAddress;

					m_uPacketDataGathered += uCopySize;
					ConsumeData(pState, uCopySize);

					if (m_uPacketDataGathered == packetSize)
					{
						out_pPacket = m_pPacketData;
						out_uSize = m_uPacketDataGathered;
						return AK_DataReady;	// Packet ready
					}

					// Not a full packet yet, keep gathering
				}
			}

			void FreeStitchBuffer()
			{
				if (m_bStitching && m_pPacketData)
				{
					AkFree(AkMemID_Processing, m_pPacketData);
					m_bStitching = false;
				}

				m_pPacketData = nullptr;
				m_uPacketDataGathered = 0;
				m_uAllocated = 0;
			}

			PACKET_STATE m_PacketState;

		private:

			AkUInt8* m_pPacketData;
			AkUInt32 m_uPacketDataGathered;
			AkUInt32 m_uAllocated;
			bool m_bStitching;
			bool m_bHeaderStitching;
		};

		class ConstantFrameSeekTable
		{
		public:
			~ConstantFrameSeekTable() { AKASSERT(m_pSeekTable == nullptr); }
			AKRESULT Init(AkUInt32 in_uEntries, void* in_pSeekData, AkUInt16 in_uSamplesPerPacket);
			void Term();
			AkUInt16 GetPacketSize(AkUInt32 uPacketIndex);
			AkUInt32 GetPacketDataOffset(AkUInt32 uDesiredSample);
			AkUInt32 GetPacketDataOffsetFromIndex(AkUInt32 uPacketIndex);
		private:			
			// Seek table			
			AkUInt32    m_uSeekTableLength = 0;
			AkUInt16*	m_pSeekTable = nullptr;
			AkUInt16    m_uSamplesPerPacket = 0;
		};
	}
}
