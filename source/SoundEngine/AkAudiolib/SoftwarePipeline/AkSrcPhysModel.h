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

//////////////////////////////////////////////////////////////////////
//
// AkSrcPhysModel.cpp
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_SRC_PHYSICAL_MODEL_H_
#define _AK_SRC_PHYSICAL_MODEL_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/SoundEngine/Common/AkSoundEngine.h>

#include "AkFXContext.h"
#include "AkFxBase.h"
#include "AkVPLSrcNode.h"

class IAkSourcePluginContextEx : public AK::IAkSourcePluginContext
{
protected:
	virtual ~IAkSourcePluginContextEx() {};
public:
	virtual void* GetOutputBusNode() = 0;

	virtual IAkGrainCodec* AllocGrainCodec() = 0;
	virtual void FreeGrainCodec(IAkGrainCodec * in_Codec) = 0;
};

class CAkSrcPhysModel : public CAkVPLSrcNode
					  , public IAkSourcePluginContextEx
					  , public AK::IAkVoicePluginInfo
{
public:

	//Constructor and destructor
	CAkSrcPhysModel( CAkPBI * in_pCtx );
	virtual ~CAkSrcPhysModel();

	// Data access.
	virtual AKRESULT StartStream( AkUInt8 * in_pBuffer, AkUInt32 ulBufferSize );
	virtual void	 StopStream();
	virtual void	 GetBuffer( AkVPLState & io_state );
	virtual void	 ReleaseBuffer();
	virtual AKRESULT TimeSkip( AkUInt32 & io_uFrames );
	virtual void     VirtualOn( AkVirtualQueueBehavior eBehavior );

	virtual AkReal32 GetDuration() const;
	virtual AKRESULT StopLooping();

	virtual bool IsUsingThisSlot(const CAkUsageSlot* in_pUsageSlot);
	virtual bool IsUsingThisSlot(const AkUInt8* in_pData);

	virtual AKRESULT ChangeSourcePosition();

	// Returns estimate of relative loudness at current position, compared to the loudest point of the sound, in dBs (always negative).
	virtual AkReal32 GetAnalyzedEnvelope( AkUInt32 in_uBufferedFrames );

	virtual void* GetOutputBusNode();
	virtual IAkGrainCodec* AllocGrainCodec();
	virtual void FreeGrainCodec(IAkGrainCodec* in_Codec);

	virtual void RestartSourcePlugin();

	inline AkPluginID GetFxID() { return m_pluginParams.fxID; }

#ifndef AK_OPTIMIZED
	virtual void RecapPluginParamDelta()
	{
		m_pluginParams.RecapRTPCParams();
	}
#endif

protected:

	// AK::IAkSourcePluginContext implementation.
	virtual AkUInt16 GetNumLoops() const { return m_pCtx->GetLooping(); }
	virtual void * GetCookie() const;
    
	virtual AkMIDIEvent GetMidiEvent() const { return *reinterpret_cast<const AkMIDIEvent*>( &(m_pCtx->GetMidiEvent()) ); }

	// AK::IAkPluginContextBase implementation.
	virtual IAkGlobalPluginContext* GlobalContext() const{ return AK::SoundEngine::GetGlobalPluginContext(); }
	virtual AkUInt16 GetMaxBufferLength( ) const
	{
		// Note: This should be EQUAL to the allocated buffer size that the inserted effect node sees (pitch node output)
		return LE_MAX_FRAMES_PER_BUFFER;
	}
	virtual AkUniqueID GetNodeID() const;
	virtual AKRESULT GetOutputID(AkUInt32 & , AkUInt32 & ) const { return AK_NotCompatible; }
	virtual void GetPluginMedia(
		AkUInt32 in_dataIndex,		///< Index of the data to be returned.
		AkUInt8* &out_rpData,		///< Pointer to the data.
		AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
		);
	virtual void GetPluginCustomGameData(
		void* &out_rpData,			///< Pointer to the data
		AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
		);

	virtual AKRESULT PostMonitorData(
		void *		in_pData,
		AkUInt32	in_uDataSize
		);
	virtual bool CanPostMonitorData();
	virtual AKRESULT PostMonitorMessage(
		const char* in_pszError,
		AK::Monitor::ErrorLevel in_eErrorLevel
		);
	virtual AkReal32 GetDownstreamGain();
	virtual AKRESULT GetParentChannelConfig(AkChannelConfig& out_channelConfig) const;
	virtual AK::IAkVoicePluginInfo * GetVoiceInfo() { return this; }
	virtual AK::IAkGameObjectPluginInfo * GetGameObjectInfo() { return this; }
#if (defined AK_CPU_X86 || defined AK_CPU_X86_64) && !(defined AK_IOS)
	// Return an interface to query processor specific features
	virtual IAkProcessorFeatures * GetProcessorFeatures();
#endif

	// AK::IAkVoicePluginInfo implementation.
	virtual AkPlayingID GetPlayingID() const { return m_pCtx->GetPlayingID(); }
	virtual AkPriority GetPriority() const { return m_pCtx->GetPriority(); }
	virtual AkPriority ComputePriorityWithDistance(AkReal32 in_fDistance) const { return m_pCtx->ComputePriorityWithDistance(in_fDistance); }

	// AK::IAkGameObjectPluginInfo implementation.
	virtual AkGameObjectID GetGameObjectID() const { return m_pCtx->GetGameObjectPtr()->ID(); }
	virtual AkUInt32 GetNumEmitterListenerPairs() const { return m_pCtx->GetGameObjectPtr()->GetNumEmitterListenerPairs(); }
	virtual AKRESULT GetEmitterListenerPair(
		AkUInt32 in_uIndex,
		AkEmitterListenerPair & out_emitterListenerPair
		) const { return m_pCtx->GetEmitterListenerPair( in_uIndex, out_emitterListenerPair ); }
	virtual AkUInt32 GetNumGameObjectPositions() const { return m_pCtx->GetNumGameObjectPositions(); }
	virtual SoundEngine::MultiPositionType GetGameObjectMultiPositionType() const { return m_pCtx->GetGameObjectMultiPositionType(); }
	virtual AkReal32 GetGameObjectScaling() const { return m_pCtx->GetGameObjectScaling(); }
	virtual AKRESULT GetGameObjectPosition(
		AkUInt32 in_uIndex,
		AkSoundPosition & out_position
		) const { return m_pCtx->GetGameObjectPosition( in_uIndex, out_position ); }
	
	virtual bool GetListeners(AkGameObjectID* out_aListenerIDs, AkUInt32& io_uSize) const
	{
		return m_pCtx->GetListeners(out_aListenerIDs, io_uSize);
	}
	virtual AKRESULT GetListenerData(
		AkGameObjectID in_uListenerID,
		AkListener & out_listener
		) const { return m_pCtx->GetListenerData( in_uListenerID, out_listener ); }

private:

	struct PluginParams : public PluginRTPCSub
	{
		AkPluginID				fxID;				// Effect unique type ID.
		IAkSourcePlugin* 		pEffect;			// Pointer to a Physical Modelling effect.

		PluginParams()
			: fxID( AK_INVALID_PLUGINID )
			, pEffect( NULL )
		{}

		void Term();
	};

	AkAudioFormat			m_AudioFormat;		// Audio format output by source.
	AkDataReferenceArray	m_dataArray;		// For plugin media.
	PluginParams			m_pluginParams;

	void*		m_pOutputBuffer;
};

#endif // _AK_SRC_PHYSICAL_MODEL_H_
