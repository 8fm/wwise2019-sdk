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

#include "AkHarmonizerDSPProcess.h"
#include <AK/Tools/Common/AkAssert.h>
#include "Mix2Interp.h"
#include <AK/DSP/AkApplyGain.h>

void AkHarmonizerDSPProcessVoice( 
	AkReal32 * in_pfCurChan,
	AkHarmonizerFXInfo & io_FXInfo, 
	AkUInt32 in_uChannelIndex,
	AkUInt32 in_uVoiceIndex,
	AkReal32 * in_pfMonoPitchedVoice,
	AkReal32 * in_pfWet,
	AkUInt32 in_uNumFrames,
	bool	 in_bNoMoreData,
	AkReal32 in_fResamplingFactor,
	AkReal32 * in_pfPVTDWindow
	)
{
	AKASSERT( in_uVoiceIndex < AKHARMONIZER_NUMVOICES );
	if ( io_FXInfo.Params.Voice[in_uVoiceIndex].bEnable )
	{
		io_FXInfo.PhaseVocoder[in_uVoiceIndex].ProcessPitchChannel( 
			in_pfCurChan, 
			in_uNumFrames, 
			in_bNoMoreData, 
			in_uChannelIndex, 
			in_pfMonoPitchedVoice, 
			in_fResamplingFactor, 
			in_pfPVTDWindow
			);

		if ( io_FXInfo.Params.Voice[in_uVoiceIndex].Filter.eFilterType != AKFILTERTYPE_NONE )
			io_FXInfo.Filter[in_uVoiceIndex].ProcessBufferMono(in_pfMonoPitchedVoice, in_uNumFrames, in_uChannelIndex);
		::DSP::Mix2Interp( in_pfWet, in_pfMonoPitchedVoice, 
							1.f, 1.f,
							io_FXInfo.PrevParams.Voice[in_uVoiceIndex].fGain, io_FXInfo.Params.Voice[in_uVoiceIndex].fGain,
							in_uNumFrames );
	}
}

void AkHarmonizerDSPProcess(	AkAudioBuffer * io_pBuffer, 
								AkHarmonizerFXInfo & io_FXInfo, 
								AkReal32 * in_pfTempStorage
								)
{
// Maximum resampling of input buffer is 4x (2 octaves), consider input buffer 5/4 window size and output buffer 1 window size (rounded upwards)
	io_FXInfo.FXTailHandler.HandleTail( io_pBuffer, 4*3*io_FXInfo.Params.uWindowSize );
	if ( AK_EXPECT_FALSE(io_pBuffer->uValidFrames < 32) )	//Internal processes don't support less than 32.
		return;
	const AkUInt32 uNumFrames = io_pBuffer->uValidFrames;
	AkReal32 * pfMonoPitchedVoice = in_pfTempStorage;
	AkReal32 * pfWet = &in_pfTempStorage[uNumFrames];
	AkReal32 * pfPhaseVocoderWindow = &in_pfTempStorage[2*uNumFrames];

	// Anonymous configs are processed in ASINPUT mode only (in which case io_FXInfo.configProcessed == io_pBuffer->GetChannelConfig())
	bool bProcessAllChannelsAnonymously = io_FXInfo.configProcessed == io_pBuffer->GetChannelConfig();
	const AkChannelMask eBufferChannelMask = io_pBuffer->GetChannelConfig().uChannelMask;
	AkUInt32 uDryChannelIndex = 0;
	AkUInt32 uDryProcessedChan = 0;
	AkUInt32 uWetProcessedChan = 0;
	AkChannelMask eChannelMaskToProcess = eBufferChannelMask;
	AkUInt32 iChannelIn = 0;
	AkUInt32 iChannelMaskIn = 0;
	while (iChannelIn < io_pBuffer->GetChannelConfig().uNumChannels)
	{
		AkUInt32 uCurrentChannelMask = 1 << iChannelMaskIn;
		
		// if existent channels in AkAudiobuffer only (if config is anonymous, eBufferChannelMask is 0 and the channel obviously exists in AkAudioBuffer).
		if (uCurrentChannelMask & eBufferChannelMask
			|| !eBufferChannelMask)
		{
			AkReal32 * pfCurrentChan;
			if (((uCurrentChannelMask & AK_SPEAKER_LOW_FREQUENCY) == 0) || (!eBufferChannelMask))
				pfCurrentChan = io_pBuffer->GetChannel( uDryChannelIndex++ );
			else
				pfCurrentChan = io_pBuffer->GetLFE( );
			AKASSERT( pfCurrentChan );
			 
			// Note: no distinction made to determine if center is true center (i.e not mono)
			// For processed channels (according to user parameter) only
			bool bProcessChannel = (uCurrentChannelMask & io_FXInfo.configProcessed.uChannelMask) || bProcessAllChannelsAnonymously;
			if ( io_FXInfo.bWetPathEnabled && 
				bProcessChannel)
			{
				AkZeroMemLarge( pfWet, uNumFrames*sizeof(AkReal32) );

				AkHarmonizerDSPProcessVoice(	pfCurrentChan,
												io_FXInfo, 
												uWetProcessedChan, 
												0, 
												pfMonoPitchedVoice, 
												pfWet, 
												uNumFrames, 
												io_pBuffer->eState == AK_NoMoreData,
												io_FXInfo.Params.Voice[0].fPitchFactor,
												pfPhaseVocoderWindow
												);
				
				AkHarmonizerDSPProcessVoice(	pfCurrentChan,
												io_FXInfo, 
												uWetProcessedChan, 
												1, 
												pfMonoPitchedVoice, 
												pfWet, 
												uNumFrames, 
												io_pBuffer->eState == AK_NoMoreData,
												io_FXInfo.Params.Voice[1].fPitchFactor,
												pfPhaseVocoderWindow
												);
				uWetProcessedChan++;
			}

			// Delay dry to sync with wet path (even unprocessed channels)
			if ( io_FXInfo.Params.bSyncDry )
			{
				io_FXInfo.DryDelay[uDryProcessedChan].ProcessBuffer( pfCurrentChan, uNumFrames
					);
				uDryProcessedChan++;
			}
			
			// For processed channels (according to user parameter) only
			if ((io_FXInfo.bWetPathEnabled && bProcessChannel) ||
				 ((io_FXInfo.Params.eInputType == AKINPUTTYPE_LEFTONLY) && 
					(uCurrentChannelMask == AK_SPEAKER_FRONT_RIGHT) && 
					 (eBufferChannelMask & AK_SPEAKER_FRONT_RIGHT)) )
			{
				// Apply wet level and mix with dry
				DSP::Mix2Interp( 
					pfCurrentChan, pfWet, 
					io_FXInfo.PrevParams.fDryLevel, io_FXInfo.Params.fDryLevel, 
					io_FXInfo.PrevParams.fWetLevel, io_FXInfo.Params.fWetLevel,
					uNumFrames );
			}
			else
			{	
				// Channel exist but no associated wet path, just apply dry gain
				AK::DSP::ApplyGain( pfCurrentChan, io_FXInfo.PrevParams.fDryLevel, io_FXInfo.Params.fDryLevel, uNumFrames );
			}

			eChannelMaskToProcess = eChannelMaskToProcess & ~uCurrentChannelMask;
			++iChannelIn;
		}	// if existent channels in AkAudiobuffer only
		++iChannelMaskIn;
	} // for maximum channels


	io_FXInfo.PrevParams = io_FXInfo.Params;
}

