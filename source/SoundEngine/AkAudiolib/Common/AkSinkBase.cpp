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

//////////////////////////////////////////////////////////////////////
//
// AkSink.cpp
//
// Platform dependent part
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "AkSinkBase.h"
#include "AkOutputMgr.h"
#include "AkFileParserBase.h"
#include "AkProfile.h"

#include "AkMixer.h"

extern AkInitSettings g_settings;

void AkSinkServices::ConvertForCapture(
	AkAudioBuffer *	in_pInBuffer,		// Input audio buffer in pipeline core format.
	AkAudioBuffer *	in_pOutBuffer,		// Output audio buffer in capture-friendly format.
	AkRamp			in_gain				// Volume gain to apply to this input (prev corresponds to the beginning, next corresponds to the end of the buffer).
	)
{
#ifdef AK_PIPELINE_OUTPUT_INTERLEAVED_INT

	// Copy and "re-attach" in_pInBuffer to in_pOutBuffer. Vita HW may use a smaller frame size and in such a case we need to fix in_pOutBuffer->MaxFrames() as well.
	void * pData = in_pOutBuffer->GetInterleavedData();
	memcpy( pData, in_pInBuffer->GetInterleavedData(), in_pInBuffer->uValidFrames * in_pInBuffer->NumChannels() * sizeof(AkInt16) );
	in_pOutBuffer->AttachInterleavedData( pData, in_pInBuffer->uValidFrames, in_pInBuffer->uValidFrames, in_pInBuffer->GetChannelConfig() );

#else

	AkAudioBuffer temp;
	if (in_pOutBuffer->GetChannelConfig().eConfigType == AK_ChannelConfigType_Ambisonic)
	{
		// Convert ACN+SN3D to FuMa.
		void * pTemp = AkMalign(AkMemID_Processing, in_pInBuffer->MaxFrames() * in_pInBuffer->NumChannels() * sizeof(AkReal32), AK_SIMD_ALIGNMENT);
		if (pTemp)
		{
			memset(pTemp, 0, in_pInBuffer->MaxFrames() * in_pInBuffer->NumChannels() * sizeof(AkReal32));
			AkUInt32 uSizeMx = AK::SpeakerVolumes::Matrix::GetRequiredSize(in_pOutBuffer->GetChannelConfig().uNumChannels, in_pOutBuffer->GetChannelConfig().uNumChannels);
			AK::SpeakerVolumes::MatrixPtr mxConv = (AK::SpeakerVolumes::MatrixPtr)AkAlloca(uSizeMx);
			AkFileParser::GetWwiseToFuMaConversionMx(in_pOutBuffer->GetChannelConfig(), mxConv);
			temp.AttachContiguousDeinterleavedData(pTemp, in_pInBuffer->MaxFrames(), 0, in_pInBuffer->GetChannelConfig());
			AkRamp ones;	
			AkMixer::MixNinNChannels(in_pInBuffer, &temp, ones, mxConv, mxConv, 1.f / in_pInBuffer->MaxFrames(), in_pInBuffer->MaxFrames());
			in_pInBuffer = &temp;	// swap input.
		}
	}
#ifdef AK_CAPTURE_TYPE_FLOAT
	const bool convertToInt16 = false;
#else
	const bool convertToInt16 = true;
#endif
	AkMixer::ApplyGainAndInterleave(in_pInBuffer, in_pOutBuffer, in_gain, convertToInt16);

	if (temp.HasData())
	{
		AkFalign(AkMemID_Processing, temp.DetachContiguousDeinterleavedData());
	}
#endif
}

//Forward declaration of AkCreateDefaultSink.  Implemented per-platform
AK::IAkPlugin* AkCreateDefaultSink(AK::IAkPluginMemAlloc * in_pAllocator);
AK::IAkPluginParam* AkCreateBGMParam(AK::IAkPluginMemAlloc * in_pAllocator);

AK::IAkPlugin* AkCreateDummySink(AK::IAkPluginMemAlloc * in_pAllocator)
{
	return AK_PLUGIN_NEW(in_pAllocator, CAkSinkDummy());
}

AK::PluginRegistration DefaultSinkRegistration(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_DEFAULT_SINK, AkCreateDefaultSink, NULL);
AK::PluginRegistration DummySinkRegistration(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_DUMMY_SINK, AkCreateDummySink, NULL);

#if defined AK_WIN || defined AK_XBOX || defined AK_SONY || defined AK_GGP || defined AK_WIN
AK::PluginRegistration CommSinkRegistration(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_COMMUNICATION_SINK, AkCreateDefaultSink, NULL);
#endif

#if defined AK_DVR_BYPASS_SUPPORTED
AK::PluginRegistration BGMSinkRegistration(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_DVR_SINK, AkCreateDefaultSink, AkCreateBGMParam);
#endif

#if defined AK_XBOX || defined AK_SONY || defined AK_WIN
AK::PluginRegistration PersonalSinkRegistration(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_PERSONAL_SINK, AkCreateDefaultSink, NULL);
#endif

#if defined AK_SONY || defined AK_WIN
AK::PluginRegistration AuxSinkRegistration(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_AUX_SINK, AkCreateDefaultSink, NULL);
AK::PluginRegistration PadSinkRegistration(AkPluginTypeSink, AKCOMPANYID_AUDIOKINETIC, AKPLUGINID_PAD_SINK, AkCreateDefaultSink, NULL);
#endif

CAkSinkBase::CAkSinkBase()
	: m_bDataReady( false )
	, m_outputDataType( AK_FLOAT )
{
}

size_t CAkSinkBase::GetDataUnitSize()
{
	return m_outputDataType == AK_INT ? sizeof(AkInt16) : sizeof(AkReal32);
}


AKRESULT CAkSinkBase::Init(AK::IAkPluginMemAlloc * in_pAlloc, AK::IAkSinkPluginContext *in_pCtx, AK::IAkPluginParam * in_pParams, AkAudioFormat & io_format)
{
	AKASSERT( !"Not a plugin. Do not use this function" );
	return AK_NotImplemented;
}

AKRESULT CAkSinkBase::Term( AK::IAkPluginMemAlloc * in_pAllocator )
{
	AK_PLUGIN_DELETE(in_pAllocator, this);	
	return AK_Success;
}

AKRESULT CAkSinkBase::Reset()
{
	return AK_Success;
}

AKRESULT CAkSinkBase::GetPluginInfo( 
	AkPluginInfo & out_rPluginInfo	///< Reference to the plug-in information structure to be retrieved
	)
{
	out_rPluginInfo.eType		  = AkPluginTypeSink;
	out_rPluginInfo.bIsInPlace	  = false;
	out_rPluginInfo.uBuildVersion = AK_WWISESDK_VERSION_COMBINED;
	return AK_Success;
}

void CAkSinkBase::Consume(
	AkAudioBuffer *			in_pInputBuffer,		///< Input audio buffer data structure. Plugins should avoid processing data in-place.
	AkRamp					in_gain					///< Volume gain to apply to this input (prev corresponds to the beginning, next corresponds to the end of the buffer).
	)
{
	if ( in_pInputBuffer->uValidFrames > 0 )
	{
		// Do final pass on data (interleave, format conversion, and the like) before presenting to sink.
		AkAudioBuffer& rBuffer = NextWriteBuffer();
		if ( rBuffer.HasData() )
		{
			// Apply volumes and interleave
			AkMixer::ApplyGainAndInterleave(in_pInputBuffer, &rBuffer, in_gain, m_outputDataType == AK_INT);

			rBuffer.uValidFrames = in_pInputBuffer->uValidFrames;
		}

		m_bDataReady = true;
	}
}

void CAkSinkBase::OnFrameEnd()
{
	if ( m_bDataReady )
		PassData();
	else
	{
		// Perform NextWriteBuffer since it was not done. This is a NOP on some platforms/sinks, 
		// but not on those that perform in-place final mix onto their output ring buffer.
		NextWriteBuffer();
		PassSilence();
	}
	
	// Prepare for next frame.
	m_bDataReady = false;
}

AKRESULT CAkSinkDummy::Init(AK::IAkPluginMemAlloc *, AK::IAkSinkPluginContext *, AK::IAkPluginParam *, AkAudioFormat & io_format)
{
	if(!io_format.channelConfig.IsValid())
	{
		AK_IF_PERF_OFFLINE_RENDERING({
			io_format.channelConfig = AK_PERF_OFFLINE_SPEAKERCONFIG;
		})
		AK_ELSE_PERF_OFFLINE_RENDERING({
			io_format.channelConfig.SetStandard(AK_SPEAKER_SETUP_STEREO);
		})
	}
		
	return AK_Success;
}

AKRESULT CAkSinkDummy::Term( AK::IAkPluginMemAlloc * in_pAllocator)
{
	return CAkSinkBase::Term(in_pAllocator);
}
