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

#ifndef _AK_PHASEVOCODER_H_
#define _AK_PHASEVOCODER_H_

#include <AK/SoundEngine/Common/IAkPluginMemAlloc.h>

#include <AK/SoundEngine/Common/AkTypes.h>
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include "ak_fftr.h"
#include "CircularBuffer.h"
#include "OLACircularBuffer.h"
#include "TimeWindow.h"
#include "FreqWindow.h"

namespace DSP
{

namespace BUTTERFLYSET_NAMESPACE
{

// Some macros to handle spectral window switching
#define FREQWIN_PREV		(channel.m_uFreqWindowIndex & 1)
#define FREQWIN_CUR			((channel.m_uFreqWindowIndex+1) & 1)

static const AkUInt32 PV_OVERLAP_FACTOR = 4;

	class CAkPhaseVocoder
	{
	public:

		CAkPhaseVocoder();

		~CAkPhaseVocoder();

		AKRESULT Init(	
			AK::IAkPluginMemAlloc *	in_pAllocator,	
			AkUInt32				in_uNumChannels,
			AkUInt32				in_uSampleRate,
			AkUInt32				in_uFFTSize,	
			bool					in_bUseInputBuffer = true
			);   

		void Term( AK::IAkPluginMemAlloc * in_pAllocator );

		void Reset( );

		// Size of required time domain buffer required by Execute (allocated outside) 
		AkUInt32 GetTDStorageSize()
		{
			return AK_ALIGN_SIZE_FOR_DMA(m_uFFTSize*sizeof(AkReal32));
		}

		 void Execute( 
			AkAudioBuffer * io_pInBuffer, 
			AkUInt32		in_uInOffset,
			AkAudioBuffer * io_pOutBuffer,
			AkReal32		in_fTSFactor,
			bool			in_bEnterNoTSMode,
			AkReal32 *		in_pfTempStorage			
			);

		AkUInt32 GetNumChannels() const {return m_uNumChannels;}
	
		struct Channel
		{
			Channel() : m_pfPrevSynthesisPhase( NULL ), m_uFreqWindowIndex( 0 ) {}
			DSP::CAkCircularBuffer		m_InputAccumBuf;
			DSP::CAkOLACircularBuffer	m_OLAOutCircBuf;
			DSP::BUTTERFLYSET_NAMESPACE::CAkFreqWindow	m_FreqWindow[2];
			DSP::BUTTERFLYSET_NAMESPACE::CAkFreqWindow	m_VocoderWindow;
			DSP::PhaseProcessingType *	m_pfPrevSynthesisPhase;
			AkUInt8						m_uFreqWindowIndex;
		}
		AK_ALIGN_DMA;
		
#ifdef AK_VOICE_MAX_NUM_CHANNELS
		Channel						m_pChannels[AK_VOICE_MAX_NUM_CHANNELS];
#else
		Channel *					m_pChannels;
#endif

		DSP::CAkTimeWindow			m_TimeWindow;
		ak_fftr_state *				m_pFFTState;
		ak_fftr_state *				m_pIFFTState;
		size_t						m_uFFTSpaceRequirements;
		size_t						m_uIFFTSpaceRequirements;
		AkUInt32					m_uNumChannels;
		AkUInt32					m_uSampleRate;
		AkUInt32					m_uFFTSize;
		AkReal32					m_fInterpPos;
		AkUInt32					m_uInputFramesToDiscard;
		bool						m_bInitPhases;
		bool						m_bInputStartFill;
		bool						m_bUseInputBuffer;
	};

} // namespace BUTTERFLYSET_NAMESPACE

} // namespace DSP

#endif // _AK_PHASEVOCODER_H_
