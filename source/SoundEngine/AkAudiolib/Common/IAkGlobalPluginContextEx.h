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

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include "AkFileParserBase.h"

class AkCaptureFile;  // Forward define

namespace AK
{
	class IAkGlobalPluginContextEx
		: public AK::IAkGlobalPluginContext
	{
	public:
		virtual AkInt32 Random() = 0; // dispatch to AKRANDOM::AkRandom()

		// Used by AkRumble to send data to the Authoring
		virtual AKRESULT PassSampleData(void* in_pData, AkUInt32 in_size, AkCaptureFile* in_pCaptureFile) = 0;
		virtual AkCaptureFile* StartCapture(const AkOSChar* in_CaptureFileName,
								  AkUInt32 in_uSampleRate,
								  AkUInt32 in_uBitsPerSample,
								  AkFileParser::FormatTag in_formatTag,
								  AkChannelConfig in_channelConfig) = 0;
		virtual void StopCapture(AkCaptureFile*& io_pCaptureFile) = 0;
		virtual bool IsOfflineRendering() = 0;
		virtual AkTimeMs GetmsPerBufferTick() = 0;
		virtual AkUInt32 GetPipelineCoreFrequency() = 0;
		virtual AkUInt32 GetNumOfSamplesPerFrame() = 0;
		virtual AKRESULT PostMonitorMessage(const char* in_pszError,				// Message or error string to be displayed
											AK::Monitor::ErrorLevel in_eErrorLevel) = 0;	// Specifies whether it should be displayed as a message or an error

		// Global panning services.
		// Compute a direct speaker assignment volume matrix with proper downmixing rules between two channel configurations.
		virtual void ComputeSpeakerVolumesDirect(
			AkChannelConfig		in_inputConfig,				///< Channel configuration of the input.
			AkChannelConfig		in_outputConfig,			///< Channel configuration of the mixer output.
			AkReal32			in_fCenterPerc,				///< Center percentage. Only applies to mono inputs with standard output configurations that have a center channel.
			AK::SpeakerVolumes::MatrixPtr out_mxVolumes		///< Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services).
			) = 0;

		// Set rtpc from audio thread skipping the message queue
		virtual void SetRTPCValueSync(
			AkRtpcID in_RTPCid,
			AkReal32 in_fValue,
			AkGameObjectID in_gameObjectID,
			AkTimeMs in_uValueChangeDuration = 0,
			AkCurveInterpolation in_eFadeCurve = AkCurveInterpolation_Linear,
			bool in_bBypassInternalValueInterpolation = false
		) = 0;
	};

	// Private interfaces.
	class IAkMixerInputContextEx : public AK::IAkMixerInputContext
	{
	public:
		virtual AkReal32 GetSpreadFullDistance(AkUInt32 in_uIndex) = 0;
		virtual AkReal32 GetFocusFullDistance(AkUInt32 in_uIndex) = 0;
	};
}
