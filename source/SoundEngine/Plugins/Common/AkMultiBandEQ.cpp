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

#include "AkMultiBandEQ.h"
#include <AK/Tools/Common/AkObject.h> // Placement new
#include <AK/Tools/Common/AkAssert.h>
#include <stdlib.h>

namespace DSP
{

CAkMultiBandEQ::CAkMultiBandEQ():
	m_pFilters(NULL),
	m_uEnabledBandMask( 0xFFFFFFFF ),
	m_uNumBands(0),
	m_uNumChannels(0)
{

}

AKRESULT CAkMultiBandEQ::Init( AK::IAkPluginMemAlloc * in_pAllocator, AkUInt16 in_uNumChannels, AkUInt16 in_uNumBands )
{
	m_uNumBands = in_uNumBands;
	m_uNumChannels = in_uNumChannels;
	AKASSERT( in_uNumBands < sizeof(m_uEnabledBandMask)*8 );
	if (in_uNumBands)
	{
		m_pFilters = (DSP::BiquadFilterMultiSIMD*) AK_PLUGIN_ALLOC(in_pAllocator, AK_ALIGN_SIZE_FOR_DMA(in_uNumBands*sizeof(DSP::BiquadFilterMultiSIMD)));
		if ( !m_pFilters )
			return AK_InsufficientMemory;
		for ( AkUInt32 i = 0; i < in_uNumBands; ++i )
		{
			AkPlacementNew(&m_pFilters[i]) DSP::BiquadFilterMultiSIMD();

			if(m_pFilters[i].Init(in_pAllocator, in_uNumChannels) != AK_Success)
				return AK_InsufficientMemory;
		}		
	}
	return AK_Success;
}

void CAkMultiBandEQ::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	if ( m_pFilters )
	{
		for(AkUInt32 i = 0; i < m_uNumBands; ++i)
			m_pFilters[i].Term(in_pAllocator);

		AK_PLUGIN_FREE( in_pAllocator, m_pFilters );
		// No need to call filter's destructor
		m_pFilters = NULL;
	}
}

void CAkMultiBandEQ::Reset()
{
	for (AkUInt32 i = 0; i < m_uNumBands; i++)
	{
		m_pFilters[i].Reset( );
	}
}

// Compute filter coefficients for a given band
void CAkMultiBandEQ::SetCoefficients( 
									 AkUInt32 in_uBand,
									 AkUInt32 in_uSampleRate, 
									 ::DSP::FilterType in_eCurve, 			
									 AkReal32 in_fFreq, 
									 AkReal32 in_fGain /*= 0.f*/, 
									 AkReal32 in_fQ /*= 1.f*/ )
{	
	m_pFilters[in_uBand].ComputeCoefs(in_eCurve, (AkReal32)in_uSampleRate, in_fFreq, in_fGain, in_fQ);	
}

// Bypass/unbypass a given band
void CAkMultiBandEQ::SetBandActive( AkUInt32 in_uBand, bool in_bActive )
{
	AkUInt32 BandMask = (1<<in_uBand);
	if ( in_bActive )
		m_uEnabledBandMask |= BandMask;
	else
		m_uEnabledBandMask &= ~BandMask;
}

// All channels
void CAkMultiBandEQ::ProcessBuffer(	AkAudioBuffer * io_pBuffer ) 
{	
	const AkUInt32 uNumFrames = io_pBuffer->uValidFrames;
	for(AkUInt32 uTCBands = 0; uTCBands < m_uNumBands; uTCBands++)
	{
		if((1 << uTCBands) & m_uEnabledBandMask)
			m_pFilters[uTCBands].ProcessBuffer(io_pBuffer->GetChannel(0), uNumFrames, io_pBuffer->MaxFrames());
	}
}

} // namespace DSP
