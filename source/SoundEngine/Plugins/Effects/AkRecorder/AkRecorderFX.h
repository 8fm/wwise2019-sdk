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

#include "AkRecorderFXParams.h"
#include <AK/SoundEngine/Common/IAkStreamMgr.h>

class CAkRecorderFX 
	: public AK::IAkInPlaceEffectPlugin
{
public:

    CAkRecorderFX();
    ~CAkRecorderFX();

    virtual AKRESULT Init(
		AK::IAkPluginMemAlloc *	in_pAllocator,
		AK::IAkEffectPluginContext * in_pFXCtx,
		AK::IAkPluginParam * in_pParams,
        AkAudioFormat &	in_rFormat
		);
    
	virtual AKRESULT Term( AK::IAkPluginMemAlloc * in_pAllocator );

	virtual AKRESULT Reset();

	virtual AKRESULT GetPluginInfo( AkPluginInfo & out_rPluginInfo );

	virtual void Execute( AkAudioBuffer * io_pBuffer );		

	virtual AKRESULT TimeSkip(AkUInt32 in_uFrames) {return AK_DataReady;}

private:
	AK::IAkPluginMemAlloc * m_pAllocator;
    CAkRecorderFXParams * m_pParams;
	AK::IAkEffectPluginContext * m_pCtx;
	class CAkRecorderManager * m_pRecorderManager;
	AK::IAkStdStream * m_pStream;
	AkInt16 * m_pTempBuffer;
	AkUInt32 m_uNumChannels;
	AkChannelConfig m_configOut;
	AkUInt32	m_uSampleRate;
	AkReal32	m_fDownstreamGain;
	bool		m_bFirstDownstreamGain;
	bool		m_bStreamErrorReported;

	bool InitializeStream();
	bool SupportsDownMix(const AkChannelConfig& in_channelConfig);
	void ProcessDownMix(AkAudioBuffer * in_pBuffer, AkReal32 in_fGain, AkReal32 in_fGainInc);

	enum AmbisonicChannelOrder { AmbiX = 0, FuMa };
};
