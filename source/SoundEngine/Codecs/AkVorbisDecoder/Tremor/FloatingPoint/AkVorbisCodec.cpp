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

#include "AkVorbisCodec.h"
#include <AK/SoundEngine/Common/AkCommonDefs.h>

//================================================================================
// decode a bunch of vorbis packets
//================================================================================

void DecodeVorbis( AkTremorInfo* in_pTremorInfo, AkUInt16 in_uMaxPacketSize, AkUInt8* in_pInputBuf, AkReal32*& in_pOutputBuf, AkUInt32 & io_uOutputBufSize)
{
	AKASSERT(in_pInputBuf != NULL && in_pTremorInfo->uInputDataSize > 0);	

	AkVorbisPacketReader PacketReader( in_pInputBuf, in_pTremorInfo->uInputDataSize );

	//Init in case of error.
	int nDecoded = 0;
	in_pTremorInfo->ReturnInfo.uFramesProduced = 0;

	int WorkSize = in_pTremorInfo->VorbisDSPState.GetWorkSize();	//Need twice the channel space because of real and imaginary parts of the spectrum.
	char * pWork = (char *)AkAllocaSIMD(WorkSize);

	do
	{
		//Read and decode one packet.
		ogg_packet Packet;
		AKRESULT eResult = PacketReader.ReadPacket( in_pTremorInfo->ReturnInfo.eDecoderState, in_uMaxPacketSize, in_pTremorInfo->bNoMoreInputPackets, Packet );
		if ( eResult == AK_Fail )
		{
			in_pTremorInfo->ReturnInfo.uFramesProduced = 0;
			in_pTremorInfo->ReturnInfo.eDecoderStatus = AK_Fail;
			return;
		}			
		else if ( eResult == AK_DataNeeded )
		{
			// No more input packets in buffer.  Need more!
			in_pTremorInfo->ReturnInfo.eDecoderStatus = AK_NoDataReady;
			in_pTremorInfo->ReturnInfo.uInputBytesConsumed = PacketReader.GetSizeConsumed();
			if (in_pTremorInfo->VorbisDSPState.work[0])
				vorbis_shift_dct(&in_pTremorInfo->VorbisDSPState);
			return;
		}

		vorbis_dsp_synthesis(&in_pTremorInfo->VorbisDSPState, &Packet, pWork, WorkSize);
		
		nDecoded = in_pTremorInfo->VorbisDSPState.state.out_end - in_pTremorInfo->VorbisDSPState.state.out_begin;
	} while (nDecoded == 0);

	in_pTremorInfo->ReturnInfo.uInputBytesConsumed = PacketReader.GetSizeConsumed();

	//Allocate PCM output buffer according to what is available.
	AkUInt32 uOutputBufferSize = in_pTremorInfo->VorbisDSPState.channels * nDecoded * sizeof(AkReal32);
	if (in_pOutputBuf == NULL || io_uOutputBufSize < uOutputBufferSize)
	{
		if (in_pOutputBuf)
			AkFalign(AkMemID_Processing, in_pOutputBuf);
		in_pOutputBuf = (AkReal32 *)AkMalign(AkMemID_Processing, uOutputBufferSize, AK_BUFFER_ALIGNMENT);
		if (in_pOutputBuf == NULL)
		{
			in_pTremorInfo->ReturnInfo.eDecoderStatus = AK_Fail;
			in_pTremorInfo->uRequestedFrames = 0;
			return;
		}
		else
		{
			io_uOutputBufSize = uOutputBufferSize;
		}
	}

	AkUInt32 idxLFE = in_pTremorInfo->VorbisDSPState.channels +1; //No lfe
	if (in_pTremorInfo->channelConfig.HasLFE())
	{
		idxLFE = 0;
		AkUInt32 c = 0;
		do 
		{
			idxLFE += (in_pTremorInfo->channelConfig.uChannelMask >> c) & 1;
			c++;
		} while (c < 3);	//3rd bit is the AK_SPEAKER_LOW_FREQUENCY bit index
	}
	
	AkUInt32 uFramesWritten = vorbis_dsp_pcmout( &in_pTremorInfo->VorbisDSPState, in_pOutputBuf, nDecoded, idxLFE );
	AKASSERT(in_pTremorInfo->VorbisDSPState.state.bShiftedDCT);
	AKASSERT(uFramesWritten == nDecoded);

	in_pTremorInfo->uRequestedFrames = nDecoded;
	in_pTremorInfo->ReturnInfo.uFramesProduced = nDecoded;
	
	// is it the end of the stream ?
	if( in_pTremorInfo->ReturnInfo.eDecoderState == END_OF_STREAM && !vorbis_dsp_pcmout( &in_pTremorInfo->VorbisDSPState, NULL, 0, 0 ) )
	{
		in_pTremorInfo->ReturnInfo.eDecoderStatus = AK_NoMoreData;
	}
	else
	{
		in_pTremorInfo->ReturnInfo.eDecoderStatus = in_pTremorInfo->ReturnInfo.uFramesProduced ? AK_DataReady : AK_NoDataReady;
	}

	AKASSERT(in_pTremorInfo->VorbisDSPState.state.bShiftedDCT);
}
