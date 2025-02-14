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

#include "AkPitchShifterDSPProcess.h"
#include <AK/Tools/Common/AkAssert.h>
#include "Mix2Interp.h"
#include <AK/DSP/AkApplyGain.h>

void AkPitchShifterDSPProcess(	AkAudioBuffer * io_pBuffer, 
								AkPitchShifterFXInfo & io_FXInfo, 
								AkReal32 * in_pfBufferStorage,
								void * pTwoPassStorage
							  )
{
	io_FXInfo.FXTailHandler.HandleTail( io_pBuffer, io_FXInfo.uTailLength );
	if ( AK_EXPECT_FALSE(io_pBuffer->uValidFrames < 32) )	//Internal processes don't support less than 32.
		return;

	const AkUInt32 uNumFrames = io_pBuffer->uValidFrames;

	AkReal32 * pfWet = in_pfBufferStorage;

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
			if ((uCurrentChannelMask & AK_SPEAKER_LOW_FREQUENCY) == 0 || (!eBufferChannelMask))
				pfCurrentChan = io_pBuffer->GetChannel( uDryChannelIndex++ );
			else
				pfCurrentChan = io_pBuffer->GetLFE( );
			AKASSERT( pfCurrentChan );

			// Note: no distinction made to determine if center is true center (i.e not mono)
			// For processed channels (according to user parameter) only
			bool bProcessChannel = io_FXInfo.configProcessed.uNumChannels > 0 && ((uCurrentChannelMask & io_FXInfo.configProcessed.uChannelMask) || bProcessAllChannelsAnonymously);
			if (bProcessChannel)
			{
#ifdef AKDELAYPITCHSHIFT_USETWOPASSALGO
				AKASSERT( pTwoPassStorage != NULL );
				io_FXInfo.PitchShifter.ProcessChannel( pfCurrentChan, pfWet, pTwoPassStorage, uNumFrames, uWetProcessedChan);
#else
				io_FXInfo.PitchShifter.ProcessChannel( pfCurrentChan, pfWet, uNumFrames, uWetProcessedChan);
#endif

				if ( io_FXInfo.Params.Voice.Filter.eFilterType != AKFILTERTYPE_NONE )
					io_FXInfo.Filter.ProcessBufferMono( pfWet, uNumFrames, uWetProcessedChan );
			
				uWetProcessedChan++;
			}

			// Delay dry to sync with wet path (even unprocessed channels)
			if ( io_FXInfo.Params.bSyncDry )
			{
				io_FXInfo.DryDelay[uDryProcessedChan].ProcessBuffer( pfCurrentChan, uNumFrames );
				uDryProcessedChan++;
			}

			// For processed channels (according to user parameter) only
			if (bProcessChannel)
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
