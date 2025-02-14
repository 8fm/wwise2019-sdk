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

#pragma once

#include "AudiolibDefs.h"
#include "AkLEngineStructs.h"
#include <AK/SoundEngine/Common/IAkPlugin.h>

namespace AkSinkServices
{
	extern void ConvertForCapture(
		AkAudioBuffer *	in_pInBuffer,		// Input audio buffer in pipeline core format.
		AkAudioBuffer *	in_pOutBuffer,		// Output audio buffer in capture-friendly format.
		AkRamp			in_gain				// Volume gain to apply to this input (prev corresponds to the beginning, next corresponds to the end of the buffer).
		);
}

struct AkOutputSettings;
struct AkSinkPluginParams;

//////////////////////////////////////////////////////////////////////
//
// CAkSinkBase
//
// Base class for built-in sinks. 
//
//////////////////////////////////////////////////////////////////////
class CAkSinkBase : public AK::IAkSinkPlugin
{
public:

	//
	// AK::IAkSinkPlugin base implementation.
	//
	virtual AKRESULT Init( 
		AK::IAkPluginMemAlloc *	in_pAllocator,				// Interface to memory allocator to be used by the effect.
		AK::IAkSinkPluginContext *	in_pSinkPluginContext,	// Interface to sink plug-in's context.
		AK::IAkPluginParam *		in_pParams,					// Interface to plug-in parameters.
		AkAudioFormat &			in_rFormat					// Audio data format of the input signal. 
		);
	virtual AKRESULT Term( 
		AK::IAkPluginMemAlloc * in_pAllocator 	// UNUSED interface to memory allocator to be used by the plug-in
		);
	virtual AKRESULT Reset();
	virtual AKRESULT GetPluginInfo( 
		AkPluginInfo & out_rPluginInfo	// Reference to the plug-in information structure to be retrieved
		);
	virtual void Consume(
		AkAudioBuffer *			in_pInputBuffer,		///< Input audio buffer data structure. Plugins should avoid processing data in-place.
		AkRamp					in_gain					///< Volume gain to apply to this input (prev corresponds to the beginning, next corresponds to the end of the buffer).
		);
	virtual void OnFrameEnd();

	//
	// Definition of interface for default sound engine sinks.
	//
	virtual AkAudioBuffer &NextWriteBuffer() = 0;
	virtual void PassData() = 0;
	virtual void PassSilence() = 0;

protected:
	CAkSinkBase();
	
	size_t GetDataUnitSize();

	bool				m_bDataReady;	
	AkDataTypeID 		m_outputDataType;
};

class CAkSinkDummy : public CAkSinkBase
{
public:
	virtual AKRESULT Init(AK::IAkPluginMemAlloc *, AK::IAkSinkPluginContext *, AK::IAkPluginParam *	in_pParams,	AkAudioFormat & io_format);
	virtual AKRESULT Term( AK::IAkPluginMemAlloc * in_pAllocator);
	
	virtual AKRESULT IsDataNeeded(AkUInt32 & out_uNumFramesNeeded)
	{
		out_uNumFramesNeeded = 1;
		return AK_Success;
	}

	virtual void Consume(AkAudioBuffer * in_pInputBuffer, AkRamp in_gain)
	{
	}

	virtual void OnFrameEnd() {}
	virtual bool IsStarved() { return false; }
	virtual void ResetStarved() {}
	virtual AkAudioBuffer &NextWriteBuffer(){ return m_Dummy; };
	virtual void PassData(){};
	virtual void PassSilence(){};

private:
	AkAudioBuffer m_Dummy;
};

