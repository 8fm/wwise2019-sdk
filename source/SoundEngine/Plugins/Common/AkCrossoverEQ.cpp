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

#include "AkCrossoverEQ.h"
#include <AK/Tools/Common/AkObject.h> // Placement new
#include <AK/Tools/Common/AkAssert.h>
#include <stdlib.h>

namespace DSP
{
	// Number of filters = 2 * ((in_uNumBands * (in_uNumBands-1))/2 - 1)
	AKRESULT CAkCrossoverEQ::Init( AK::IAkPluginMemAlloc * in_pAllocator,
								   AkUInt16 in_uNumChannels,
								   AkUInt16 in_uMaxBands,
								   bool in_LR4 )
	{
		// Restrict to 5 bands at the moment
		AKASSERT( in_uMaxBands == MAX_CROSSOVER_BANDS );

		m_LR4 = in_LR4;

		if ( m_LR4 )
		{
			m_fpPerformDSP = &CAkCrossoverEQ::ProcessBufferInternalLR4;
		}
		else
		{
			m_fpPerformDSP = &CAkCrossoverEQ::ProcessBufferInternalLR2;
		}

		// Tree of filters, + LP/HP SOS cascade
		AkUInt16 numFilters = (in_uMaxBands * (in_uMaxBands + 1)) / 2 - 1;

		// Add the cascade sections for LR4
		if ( m_LR4 )
		{
			numFilters += 2 * (in_uMaxBands - 1);
		}


		return CAkMultiBandEQ::Init( in_pAllocator, in_uNumChannels, numFilters );
	}

	// Compute filter coefficients for a given band edge
	void CAkCrossoverEQ::SetCoefficients( AkUInt32 in_uEdge, AkUInt32 in_uSampleRate, AkReal32 in_fFreq )
	{
		// Index limit - edges = nBands-1
		AKASSERT( in_uEdge < MAX_CROSSOVER_BANDS-1 );

		AkUInt32 filterIndex = 0;
		
		// TODO: Make this a loop over numEdges
		// Repeat every filter sections for SOS cascades
		if ( m_LR4 )
		{
			switch ( in_uEdge )
			{
			// SUB
			case 0:
			{
				// Butterworth design 
				// LP
				CAkMultiBandEQ::SetCoefficients( 0, in_uSampleRate, DSP::FilterType_LowPass, in_fFreq );
				CAkMultiBandEQ::SetCoefficients( 1, in_uSampleRate, DSP::FilterType_LowPass, in_fFreq );

				// HP
				CAkMultiBandEQ::SetCoefficients( 5, in_uSampleRate, DSP::FilterType_HighPass, in_fFreq );
				CAkMultiBandEQ::SetCoefficients( 6, in_uSampleRate, DSP::FilterType_HighPass, in_fFreq );
				break;
			}
			// LOW
			case 1:
			{
				// AP LOW
				SetAllPassCoefs( 2, (AkReal32)in_uSampleRate, in_fFreq, m_LR4 );

				// Butterworth design 
				// LP
				CAkMultiBandEQ::SetCoefficients( 7, in_uSampleRate, DSP::FilterType_LowPass, in_fFreq );
				CAkMultiBandEQ::SetCoefficients( 8, in_uSampleRate, DSP::FilterType_LowPass, in_fFreq );

				// HP
				CAkMultiBandEQ::SetCoefficients( 11, in_uSampleRate, DSP::FilterType_HighPass, in_fFreq );
				CAkMultiBandEQ::SetCoefficients( 12, in_uSampleRate, DSP::FilterType_HighPass, in_fFreq );
				break;
			}
			// MID
			case 2:
			{
				// AP MID
				SetAllPassCoefs( 3, (AkReal32)in_uSampleRate, in_fFreq, m_LR4 );
				SetAllPassCoefs( 9, (AkReal32)in_uSampleRate, in_fFreq, m_LR4 );

				// Butterworth design 
				// LP
				CAkMultiBandEQ::SetCoefficients( 13, in_uSampleRate, DSP::FilterType_LowPass, in_fFreq );
				CAkMultiBandEQ::SetCoefficients( 14, in_uSampleRate, DSP::FilterType_LowPass, in_fFreq );

				// HP
				CAkMultiBandEQ::SetCoefficients( 16, in_uSampleRate, DSP::FilterType_HighPass, in_fFreq );
				CAkMultiBandEQ::SetCoefficients( 17, in_uSampleRate, DSP::FilterType_HighPass, in_fFreq );
				break;
			}
			// HIGH
			case 3:
			{
				// AP HIGH
				SetAllPassCoefs( 4, (AkReal32)in_uSampleRate, in_fFreq, m_LR4 );
				SetAllPassCoefs( 10, (AkReal32)in_uSampleRate, in_fFreq, m_LR4 );
				SetAllPassCoefs( 15, (AkReal32)in_uSampleRate, in_fFreq, m_LR4 );

				// Butterworth design 
				// LP
				CAkMultiBandEQ::SetCoefficients( 18, in_uSampleRate, DSP::FilterType_LowPass, in_fFreq );
				CAkMultiBandEQ::SetCoefficients( 19, in_uSampleRate, DSP::FilterType_LowPass, in_fFreq );

				// HP
				CAkMultiBandEQ::SetCoefficients( 20, in_uSampleRate, DSP::FilterType_HighPass, in_fFreq );
				CAkMultiBandEQ::SetCoefficients( 21, in_uSampleRate, DSP::FilterType_HighPass, in_fFreq );
				break;
			}
			}
		}
		else // LR2
		{
			switch ( in_uEdge )
			{
			// SUB
			case 0:
			{
				// LP
				SetLR2Coefs( 0, (AkReal32)in_uSampleRate, DSP::FilterType_LowPass, in_fFreq );
				// HP
				SetLR2Coefs( 4, (AkReal32)in_uSampleRate, DSP::FilterType_HighPass, in_fFreq );
				break;
			}
			// LOW
			case 1:
			{
				// AP LOW
				SetAllPassCoefs( 1, (AkReal32)in_uSampleRate, in_fFreq, m_LR4 );

				// LP
				SetLR2Coefs( 5, (AkReal32)in_uSampleRate, DSP::FilterType_LowPass, in_fFreq );
				// HP
				SetLR2Coefs( 8, (AkReal32)in_uSampleRate, DSP::FilterType_HighPass, in_fFreq );
				break;
			}
			// MID
			case 2:
			{
				// AP MID
				SetAllPassCoefs( 2, (AkReal32)in_uSampleRate, in_fFreq, m_LR4 );
				SetAllPassCoefs( 6, (AkReal32)in_uSampleRate, in_fFreq, m_LR4 );

				// LP
				SetLR2Coefs( 9, (AkReal32)in_uSampleRate, DSP::FilterType_LowPass, in_fFreq );
				// HP
				SetLR2Coefs( 11, (AkReal32)in_uSampleRate, DSP::FilterType_HighPass, in_fFreq );
				break;
			}
			// HIGH
			case 3:
			{
				// AP HIGH
				SetAllPassCoefs( 3, (AkReal32)in_uSampleRate, in_fFreq, m_LR4 );
				SetAllPassCoefs( 7, (AkReal32)in_uSampleRate, in_fFreq, m_LR4 );
				SetAllPassCoefs( 10, (AkReal32)in_uSampleRate, in_fFreq, m_LR4 );

				// LP
				SetLR2Coefs( 12, (AkReal32)in_uSampleRate, DSP::FilterType_LowPass, in_fFreq );
				// HP
				SetLR2Coefs( 13, (AkReal32)in_uSampleRate, DSP::FilterType_HighPass, in_fFreq );
				break;
			}
			}
		}
	}

	void CAkCrossoverEQ::SetLR2Coefs( AkUInt32 in_uEdge, AkReal32 in_fSampleRate, DSP::FilterType in_eCurve, AkReal32 in_fFreq )
	{
		// After BiquadFilter.ComputeCoefs()..
		AkReal32 fb0 = 0.f;
		AkReal32 fb1 = 0.f;
		AkReal32 fb2 = 0.f;	// Feed forward coefficients
		AkReal32 fa0 = 0.f;
		AkReal32 fa1 = 0.f;
		AkReal32 fa2 = 0.f;	// Feed back coefficients

		AkReal32 fFrequency = in_fFreq;

		// Frequency must be less or equal to the half of the sample rate
		// 0.9 is there because of bilinear transform behavior at frequencies close to Nyquist
		const AkReal32 fMaxFrequency = in_fSampleRate * 0.5f * 0.9f;
		fFrequency = AkMin( fMaxFrequency, fFrequency );

		AkReal32 PiFcOSr = PI * fFrequency / in_fSampleRate;
		AkReal32 fSinWcPlusPiOFour = sinf( PiFcOSr + PI * 0.25f );
		AkReal32 fSinPlusSqVal = fSinWcPlusPiOFour * fSinWcPlusPiOFour;
		AkReal32 fOneOSinSqVal = 1.0f / fSinPlusSqVal;
		AkReal32 fCosTwoWc = -cosf( 2 * PiFcOSr );
		AkReal32 fCosWcPlusPiOFour = cosf( PiFcOSr + PI * 0.25f );
		AkReal32 fCosPlusSqVal = fCosWcPlusPiOFour * fCosWcPlusPiOFour;

		if ( in_eCurve == DSP::FilterType_LowPass )
		{
			AkReal32 fSinWc = sinf( PiFcOSr );
			AkReal32 fSinSqVal = fSinWc * fSinWc;
			AkReal32 fSinSqValOSinSqPlusVal = fSinSqVal * fOneOSinSqVal;
			fb0 = 0.5f * fSinSqValOSinSqPlusVal;
			fb1 = fSinSqValOSinSqPlusVal;
			fb2 = 0.5f * fSinSqValOSinSqPlusVal;

		}
		else // if ( in_eCurve == DSP::FilterType_HighPass )
		{
			AkReal32 fCosWc = cosf( PiFcOSr );
			AkReal32 fCosSqVal = fCosWc * fCosWc;
			AkReal32 fCosSqValOSinSqVal = fCosSqVal * fOneOSinSqVal;
			fb0 = -0.5f * fCosSqValOSinSqVal;
			fb1 = fCosSqValOSinSqVal;
			fb2 = -0.5f * fCosSqValOSinSqVal;
		}
		
		fa1 = fCosTwoWc * fOneOSinSqVal;
		fa2 = fCosPlusSqVal * fOneOSinSqVal;


		// Set the filter coefficients
		for ( AkUInt32 uChannel = 0; uChannel < m_uNumChannels; uChannel++ )
		{
			AkUInt32 uFilterIndex = (uChannel*m_uNumBands) + in_uEdge;
			m_pFilters[uFilterIndex].SetCoefs( fb0, fb1, fb2, fa1, fa2 );
		}
	}

	void CAkCrossoverEQ::SetAllPassCoefs( AkUInt32 in_uEdge, AkReal32 in_fSampleRate, AkReal32 in_fFreq, bool in_LR4 )
	{
		// After BiquadFilter.ComputeCoefs()..
#define SQRT2 1.41421356237309504880f

		AkReal32 fb0 = 0.f;
		AkReal32 fb1 = 0.f;
		AkReal32 fb2 = 0.f;	// Feed forward coefficients
		AkReal32 fa0 = 0.f;
		AkReal32 fa1 = 0.f;
		AkReal32 fa2 = 0.f;	// Feed back coefficients

		AkReal32 fFrequency = in_fFreq;

		// Frequency must be less or equal to the half of the sample rate
		// 0.9 is there because of bilinear transform behavior at frequencies close to Nyquist
		const AkReal32 fMaxFrequency = in_fSampleRate * 0.5f * 0.9f;
		fFrequency = AkMin( fMaxFrequency, fFrequency );

		if ( in_LR4 )
		{
			// 2nd order all pass filter
			AkReal32 PiFcOSr = PI * fFrequency / in_fSampleRate;
			AkReal32 fTanPiFcSr = tanf( PiFcOSr );
			AkReal32 fIntVal = 1.0f / fTanPiFcSr;
			AkReal32 fRootTwoxIntVal = SQRT2 * fIntVal;
			AkReal32 fSqIntVal = fIntVal * fIntVal;
			AkReal32 fOnePlusSqIntVal = 1.f + fSqIntVal;
			AkReal32 fOneOSum = 1 / (fRootTwoxIntVal + fOnePlusSqIntVal);
			// Coefficient formulas
			fb0 = (fOnePlusSqIntVal - fRootTwoxIntVal) * fOneOSum;
			fb1 = -2 * (fSqIntVal - 1) * fOneOSum;
			fb2 = 1.0f;
			fa0 = 1.0f;
			fa1 = fb1;
			fa2 = fb0;
		}
		else
		{
			// 1st order all pass filter
			AkReal32 PiFcOSr = PI * fFrequency / in_fSampleRate;
			AkReal32 fTanPiFcSr = tanf( PiFcOSr + PI * 0.25f );
			AkReal32 fIntVal = 1.0f / fTanPiFcSr;
			// Coefficient formulas
			fb0 = -fIntVal;
			fb1 = 1.0f;
			fb2 = 0.0f;
			fa0 = 1.0f;
			fa1 = fb0;
			fa2 = 0.0f;
		}


		// Set the filter coefficients
		for ( AkUInt32 uChannel = 0; uChannel < m_uNumChannels; uChannel++ )
		{
			AkUInt32 uFilterIndex = (uChannel*m_uNumBands) + in_uEdge;
			m_pFilters[uFilterIndex].SetCoefs( fb0 , fb1 , fb2 , fa1 , fa2 );
		}
	}

	// Override to return subbands
	// Gain is set outside of the function
	void CAkCrossoverEQ::ProcessBuffer( AkAudioBuffer * io_pBuffers, AkUInt32 in_uNumBands )
	{
		(this->*m_fpPerformDSP)(m_pFilters, io_pBuffers, in_uNumBands);
	}

	void CAkCrossoverEQ::ProcessBufferInternalLR4( DSP::BiquadFilterMultiSIMD * pFilter, AkAudioBuffer * io_pBuffers, AkUInt32 in_uNumBands )
	{
		const AkUInt32 uNumChannels = io_pBuffers[0].NumChannels();
		const AkUInt32 uNumFrames = io_pBuffers[0].uValidFrames;

		AKASSERT( uNumChannels <= m_uNumChannels );

		for ( AkUInt32 i = 0; i < uNumChannels; ++i )
		{
			// There should be 5 io_pBuffers
			AkReal32 * pfChannels[5];
			pfChannels[0] = io_pBuffers[0].GetChannel( i );
			pfChannels[1] = io_pBuffers[1].GetChannel( i );
			pfChannels[2] = io_pBuffers[2].GetChannel( i );
			pfChannels[3] = io_pBuffers[3].GetChannel( i );
			pfChannels[4] = io_pBuffers[4].GetChannel( i );

			// Channel offset
			const AkUInt32 uFilterIndexOffset = i*m_uNumBands;
			AkUInt32 filterIndex = 0;

			// Band 0 - Filters 0 1 2 3 4
			// Band 1 - Filters 5 6 7 8 9 10
			// Band 2 - Filters (5 6) 11 12 13 14 15
			// Band 3 - Filters (5 6 11 12) 16 17 18 19
			// Band 4 - Filters (5 6 11 12 16 17) 20 21
			for ( AkUInt32 j = 0; j < in_uNumBands-1; j++ )
			{
				AKPLATFORM::AkMemCpy( pfChannels[j+1], pfChannels[j], sizeof( AkReal32 ) * uNumFrames );
				for ( AkUInt32 k = 0; k < in_uNumBands - j; k++ )
				{
					pFilter[uFilterIndexOffset + filterIndex++].ProcessBuffer( pfChannels[j], uNumFrames );
				}
				// Skip indices that aren't used when running less bands
				filterIndex += 5 - in_uNumBands;
				
				pFilter[uFilterIndexOffset + filterIndex++].ProcessBuffer( pfChannels[j + 1], uNumFrames );
				pFilter[uFilterIndexOffset + filterIndex++].ProcessBuffer( pfChannels[j + 1], uNumFrames );
			}
		}
	}
	void CAkCrossoverEQ::ProcessBufferInternalLR2( DSP::BiquadFilterMultiSIMD * pFilter, AkAudioBuffer * io_pBuffers, AkUInt32 in_uNumBands )
	{
		const AkUInt32 uNumChannels = io_pBuffers[0].NumChannels();
		const AkUInt32 uNumFrames = io_pBuffers[0].uValidFrames;
		AKASSERT( uNumChannels <= m_uNumChannels );
		for ( AkUInt32 i = 0; i < uNumChannels; ++i )
		{
			// There should be 5 io_pBuffers
			AkReal32 * pfChannels[5];
			pfChannels[0] = io_pBuffers[0].GetChannel( i );
			pfChannels[1] = io_pBuffers[1].GetChannel( i );
			pfChannels[2] = io_pBuffers[2].GetChannel( i );
			pfChannels[3] = io_pBuffers[3].GetChannel( i );
			pfChannels[4] = io_pBuffers[4].GetChannel( i );

			// Channel offset
			const AkUInt32 uFilterIndexOffset = i*m_uNumBands;
			AkUInt32 filterIndex = 0;

			// Band 0 - Filters 0 1 2 3
			// Band 1 - Filters 4 5 6 7
			// Band 2 - Filters (4) 8 9 10
			// Band 3 - Filters (4 8) 11 12
			// Band 4 - Filters (4 8 11) 13
			for ( AkUInt32 j = 0; j < in_uNumBands - 1; j++ )
			{
				AKPLATFORM::AkMemCpy( pfChannels[j + 1], pfChannels[j], sizeof( AkReal32 ) * uNumFrames );
				for ( AkUInt32 k = 0; k < in_uNumBands - j - 1; k++ )
				{
					pFilter[uFilterIndexOffset + filterIndex++].ProcessBuffer( pfChannels[j], uNumFrames );
				}
				// Skip indices that aren't used when running less bands
				filterIndex += 5 - in_uNumBands;

				pFilter[uFilterIndexOffset + filterIndex++].ProcessBuffer( pfChannels[j + 1], uNumFrames );
			}
		}

	}
}