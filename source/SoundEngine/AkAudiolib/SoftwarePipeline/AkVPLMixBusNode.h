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
// AkVPLMixBusNode.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_VPL_MIX_BUS_NODE_H_
#define _AK_VPL_MIX_BUS_NODE_H_

#include "AkLEngineDefs.h"
#include "AkLEngineStructs.h"
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/SoundEngine/Common/IAkMixerPlugin.h>
#include "AkFXContext.h"
#include "AkFxBase.h"

#include "AkMixer.h"
#include "AkBusCtx.h"
#include "AkVPL3dMixable.h"
#include "AkBehavioralCtx.h"
#include "AkBus.h"
#include "AkMeterTools.h"

class AkMixConnection;

class CAkBusVolumes : public CAkVPL3dMixable
{
public:
	CAkBusVolumes()
		: m_pOwner(NULL)
		, m_pUserData(NULL)
		, m_bAttachedPropCreated(false)
		, m_bVolumeCallbackEnabled(false)
		, m_eMeteringCallbackFlags(AK_NoMetering)
		, m_eMeteringPluginFlags(AK_NoMetering)
	{
		m_bIsABus = 1;
	}
	~CAkBusVolumes();

	CAkBus* GetBus() const { return m_BusContext.GetBus(); }

	AKRESULT InitPan( CAkParameterNodeBase* in_pBus, AkChannelConfig in_channelConfig, AkChannelConfig in_parentConfig);
	void Update2DParams( CAkParameterNodeBase* in_pBus );

	// Graph processing: Collapse downstream gain of parent with gain of this bus.
	inline void InitializeDownstreamGainDB()
	{
		m_fDownstreamGainDB = AkMath::FastLinTodB(m_fBehavioralVolume);
	}

	void _GetOutputID(AkUInt32 & out_uOutputID, AkUInt32 & out_uDeviceType) const;

	inline AkUniqueID			ID() const { return GetBus() ? GetBus()->ID() : AK_INVALID_UNIQUE_ID; }
	inline const AK::CAkBusCtx& GetBusContext() const { return m_BusContext; }
	inline AkGameObjectID	GameObjectID() const { return m_BusContext.GameObjectID(); }

	inline const AkRamp & GetBaseVolume() const { return m_baseVolume; }

	// Returns channel config of this bus.
	inline AkChannelConfig GetMixConfig() const { return m_MixBuffer.GetChannelConfig(); }
	virtual AkChannelConfig GetOutputConfig() const = 0;

	// True if mixing into parent requires an N in N mix (ignored if configs are different).
	inline bool IsVolumeCallbackEnabled() { return m_bVolumeCallbackEnabled; }
	inline void EnableVolumeCallback( bool in_bEnabled ){ m_bVolumeCallbackEnabled = in_bEnabled; }

	// IAkVoicePluginInfo interface
	virtual AkPlayingID GetPlayingID() const { return AK_INVALID_PLAYING_ID; }
	virtual AkPriority GetPriority() const { return 0; }
	virtual AkPriority ComputePriorityWithDistance(	AkReal32 in_fDistance) const  { return 0; }

	// 3D Mixable
	AkForceInline void ComputeVolumeRays()
	{
		if (m_pContext != NULL)
		{
			m_pContext->CalcEffectiveParams();
			_ComputeVolumeRays();
		}
		else
		{
			// "Merge" bus.
			m_fBehavioralVolume = 1.f;
		}
	}

#ifndef AK_OPTIMIZED
	// For monitoring.
	AkReal32 BehavioralVolume() const; //linear
#endif

protected:

	// Create attached props cloned parameter object if necessary and returns it.
	AK::IAkPluginParam * CreateAttachedPropsParam();

	AkVPL *					m_pOwner;
	AK::CAkBusCtx			m_BusContext;

protected:
	AkPipelineBufferBase	m_MixBuffer;			// mix output buffer.
#ifndef AK_OPTIMIZED
	AkUInt32				m_uMixingVoiceCount;
#endif

	void *					m_pUserData;			// User-defined data (used by mixer plugins).

	AkRamp					m_baseVolume;			// this is used for metering and the master bus.

	AkUInt8					m_bAttachedPropCreated : 1;

	AkUInt8					m_bVolumeCallbackEnabled:1;

	AkUInt8	/*AkMeteringFlags */	m_eMeteringCallbackFlags : AK_MAX_BITS_METERING_FLAGS;
	AkUInt8	/*AkMeteringFlags */	m_eMeteringPluginFlags : AK_MAX_BITS_METERING_FLAGS;
};

class CAkBusFX : public CAkBusVolumes
			   , public AK::IAkMixerPluginContext
{
public:

	CAkBusFX();
	~CAkBusFX();

	void FindFXByContext(
		CAkBusFXContext * in_pBusFXContext,
		AkPluginID & out_pluginID,
		AkUInt32 & out_uFXIndex
		)
	{
		for ( AkUInt32 uFX = 0; uFX<AK_NUM_EFFECTS_PER_OBJ; uFX++ )
		{
			if ( m_aFX[ uFX ].pBusFXContext == in_pBusFXContext )
			{
				out_pluginID = m_aFX[ uFX ].id;
				out_uFXIndex = uFX;
				return;
			}
		}
		AKASSERT( !"Invalid BusFXContext" );
	}

	bool IsUsingThisSlot( const CAkUsageSlot* in_pUsageSlot )
	{
		for ( AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex )
		{
			if( m_aFX[ uFXIndex ].pBusFXContext && m_aFX[ uFXIndex ].pBusFXContext->IsUsingThisSlot( in_pUsageSlot, m_aFX[ uFXIndex ].pEffect ) )
				return true;
		}

		return IsMixerPluginUsingThisSlot(in_pUsageSlot);
	}

	bool IsMixerPluginUsingThisSlot( const CAkUsageSlot* in_pUsageSlot )
	{
		if( m_BusContext.HasMixerPlugin())
		{
			for( AkDataReferenceArray::Iterator iter = m_dataArray.Begin(); iter != m_dataArray.End(); ++iter )
			{
				AkDataReference& ref = (*iter).item;
				if( ref.pUsageSlot == in_pUsageSlot )
				{
						AkFXDesc fxDesc;
						m_BusContext.GetMixerPlugin( fxDesc );
						AKASSERT( m_pMixerPlugin->pPlugin );
						if( !AkDataReferenceArray::FindAlternateMedia( in_pUsageSlot, ref, m_pMixerPlugin->pPlugin ) )
							return true;
					}
			}
		}

		return false;
	}

	bool IsUsingThisSlot( const AkUInt8* in_pData )
	{
		for ( AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex )
		{
			if( m_aFX[ uFXIndex ].pBusFXContext && m_aFX[ uFXIndex ].pBusFXContext->IsUsingThisSlot( in_pData ) )
				return true;
		}

		for( AkDataReferenceArray::Iterator iter = m_dataArray.Begin(); iter != m_dataArray.End(); ++iter )
		{
			AkDataReference& ref = (*iter).item;
			if( ref.pData == in_pData )
				return true;
		}

		return false;
	}

	void SetInsertFxMask(AkUInt32 in_uFxMask);
	void SetInsertFx(AkUInt32 in_uFXIndex)				{ SetInsertFxMask( (1U << in_uFXIndex)); }
	void SetInsertFxBypass( AkUInt32 in_bitsFXBypass, AkUInt32 in_uTargetMask );
	void DropFx( AkUInt32 in_uFXIndex );
	void DropFx();
	void SetMixerPlugin();
	void DropMixerPlugin();

	void				SetAllInsertFx() { SetInsertFxMask( (1U << AK_NUM_EFFECTS_PER_OBJ) - 1U); }
	void				UpdateBypassFx();

	inline bool			EffectsCreated() {return m_bEffectCreated;}

//	virtual void UpdateTargetParam( AkRTPC_ParameterID in_eParam, AkReal32 in_fValue, AkReal32 in_fDelta );

	AkForceInline AkPluginID GetFXID( AkUInt32 in_uFXIndex ) { return m_aFX[ in_uFXIndex ].id; }
				  AkPluginID GetMixerID();

	AkForceInline AkChannelConfig GetMixConfig() { return m_MixBuffer.GetChannelConfig(); }

	void RefreshMeterWatch();

	//With the presence of out-of-place effects, it is possible for the output config of this bus to be difference from the input (mix) config.
	//	Also be careful because this config can change depending on the bypass state of the bus.
	virtual AkChannelConfig GetOutputConfig() const;

	//And indeed, the channel config of the parent may once again be different than the output config.
	virtual AKRESULT GetParentChannelConfig(AkChannelConfig& out_channelConfig) const;

	inline const AkMeterCtx* GetMeterCtx() { return m_MeterCtxFactory.CreateMeterContext(); }

	void GetPluginCustomGameDataForInsertFx(
		AkUInt32 in_uFXIndex,
		void* &out_rpData,			///< Pointer to the data
		AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
		);

#ifndef AK_OPTIMIZED
	inline void				IncrementMixingVoiceCount(){ ++m_uMixingVoiceCount; }
	inline void				ResetMixingVoiceCount(){ m_uMixingVoiceCount = 0; }
	inline AkUInt32			GetMixingVoiceCount(){ return m_uMixingVoiceCount; }
	
	void RecapPluginParamDelta();
#endif

	virtual AKRESULT ComputeSpeakerVolumesDirect(
		AkChannelConfig		in_inputConfig,				///< Channel configuration of the input.
		AkChannelConfig		in_outputConfig,			///< Channel configuration of the mixer output.
		AkReal32			in_fCenterPerc,				///< Center percentage. Only applies to mono inputs to outputs that have no center.
		AK::SpeakerVolumes::MatrixPtr out_mxVolumes		///< Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services).
	);

	virtual AKRESULT Compute3DPositioning(
		AkReal32			in_fAngle,					///< Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are clockwise).
		AkReal32			in_fElevation,				///< Incident elevation angle, in radians [-pi/2,pi/2], where 0 is the horizon (positive values are above the horizon).
		AkReal32			in_fSpread,					///< Spread.
		AkReal32			in_fFocus,					///< Focus.
		AkChannelConfig		in_inputConfig,				///< Channel configuration of the input.
		AkChannelMask		in_uInputChanSel,			///< Mask of input channels selected for panning (excluded input channels don't contribute to the output).
		AkChannelConfig		in_outputConfig,			///< Desired output configuration.
		AkReal32			in_fCenterPerc,				///< Center percentage. Only applies to mono inputs to outputs that have a center.
		AK::SpeakerVolumes::MatrixPtr out_mxVolumes		///< Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services).
		);

	virtual AKRESULT Compute3DPositioning(
		const AkTransform & in_emitter,					// Emitter transform.
		const AkTransform & in_listener,				// Listener transform.
		AkReal32			in_fCenterPerc,				// Center percentage. Only applies to mono inputs to outputs that have a center.
		AkReal32			in_fSpread,					// Spread.
		AkReal32			in_fFocus,					// Focus.
		AkChannelConfig		in_inputConfig,				// Channel configuration of the input.
		AkChannelMask		in_uInputChanSel,			// Mask of input channels selected for panning (excluded input channels don't contribute to the output).
		AkChannelConfig		in_outputConfig,			// Desired output configuration.
		AK::SpeakerVolumes::MatrixPtr out_mxVolumes		// Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services). Assumes zeroed.
		);

	AKRESULT _Compute3DPositioning(
		const AkTransform & in_emitter,					// Emitter transform.
		AkReal32			in_fCenterPerc,				// Center percentage. Only applies to mono inputs to outputs that have a center.
		AkReal32			in_fSpread,					// Spread.
		AkReal32			in_fFocus,					// Focus.
		AK::SpeakerVolumes::MatrixPtr io_mxVolumes,		// Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services). Assumes zeroed.
		AkChannelConfig		in_inputConfig,				// Channel configuration of the input.
		AkChannelMask		in_uInputChanSel,			// Mask of input channels selected for panning (excluded input channels don't contribute to the output).
		AkChannelConfig		in_outputConfig,			// Desired output configuration.
		const AkVector &	in_listPosition,			// listener position
		const AkReal32		in_listRot[3][3]			// listener rotation
		);

		inline void EnableMeterCallback( AkMeteringFlags in_eMeteringFlags ){ m_eMeteringCallbackFlags = in_eMeteringFlags; RefreshMeterWatch(); }
		AkGameObjectID GetGameObjectID() const { return m_BusContext.GameObjectID(); }

protected:

	// Not safe to call these directly.
	void _SetInsertFx( AkUInt32 in_uFXIndex, AkAudioFormat& io_Format);
	void _DropFx(AkUInt32 in_uFXIndex);


	// AK::IAkPluginContextBase implementation
	virtual AK::IAkGlobalPluginContext* GlobalContext() const { return AK::SoundEngine::GetGlobalPluginContext(); }
	virtual AkUInt16 GetMaxBufferLength() const { return LE_MAX_FRAMES_PER_BUFFER; }
	virtual AKRESULT GetOutputID(AkUInt32 & out_uOutputID, AkUInt32 & out_uDeviceType) const;
	virtual void GetPluginMedia(
		AkUInt32 in_dataIndex,		///< Index of the plug-in media to be returned.
		AkUInt8* &out_rpData,		///< Pointer to the data
		AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
		);
	virtual void GetPluginCustomGameData(
		void* &out_rpData,			///< Pointer to the data
		AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
		);
	virtual AKRESULT PostMonitorData(
		void *		in_pData,		///< Blob of data.
		AkUInt32	in_uDataSize	///< Size of data.
		);
	virtual bool	 CanPostMonitorData();
	virtual AKRESULT PostMonitorMessage(
		const char* in_pszError,
		AK::Monitor::ErrorLevel in_eErrorLevel
		);
	virtual AkReal32 GetDownstreamGain();

#if (defined AK_CPU_X86 || defined AK_CPU_X86_64) && !(defined AK_IOS)
	virtual AK::IAkProcessorFeatures * GetProcessorFeatures();
#endif

	// IAkMixerPluginContext implementation
	virtual AkUniqueID GetBusID() { return ID(); }	
	virtual AkUInt32 GetBusType();
	virtual AKRESULT GetSpeakerAngles(
		AkReal32 *			io_pfSpeakerAngles,			///< Returned array of loudspeaker pair angles, in degrees relative to azimuth [0,180]. Pass NULL to get the required size of the array.
		AkUInt32 &			io_uNumAngles,				///< Returned number of angles in io_pfSpeakerAngles, which is the minimum between the value that you pass in, and the number of angles corresponding to the output configuration, or just the latter if io_pfSpeakerAngles is NULL.
		AkReal32 &			out_fHeightAngle			///< Elevation of the height layer, in degrees relative to the plane.
		);
	virtual AKRESULT ComputeSpeakerVolumesPanner(
		const AkVector &	in_position,				///< x,y,z panner position [-1,1]. Note that z has no effect at the moment.
		AkReal32			in_fCenterPct,				///< Center percentage.
		AkChannelConfig		in_inputConfig,				///< Channel configuration of the input.
		AkChannelConfig		in_outputConfig,			///< Channel configuration of the mixer output.
		AK::SpeakerVolumes::MatrixPtr out_mxVolumes		///< Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services).
		);
	virtual AKRESULT ComputePlanarVBAPGains(
		AkReal32		in_fAngle,						///< Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are clockwise)
		AkChannelConfig	in_outputConfig,				///< Desired output configuration.
		AkReal32		in_fCenterPerc,					///< Center percentage. Only applies to mono inputs to outputs that have no center.
		AK::SpeakerVolumes::VectorPtr out_vVolumes		///< Returned volumes (see AK::SpeakerVolumes::Vector services). Must be allocated prior to calling this function with the size returned by AK::SpeakerVolumes::Vector::GetRequiredSize() for the desired configuration.
		);
	virtual AKRESULT InitSphericalVBAP(
		AK::IAkPluginMemAlloc* in_pAllocator,			///< Memory allocator
		const AkSphericalCoord* in_SphericalPositions,	///< Array of points in spherical coordinate, representign the virtual position of each channels.
		const AkUInt32 in_NbPoints,						///< Number of points in the position array
		void *& out_pPannerData							///< Contains data relevant to the 3D panner that shoud be re-used accross plugin instances.
		);
	virtual AKRESULT ComputeSphericalVBAPGains(
		void*				in_pPannerData,				///< Contains data relevant to the 3D panner that shoud be re-used accross plugin instances.
		AkReal32			in_fAzimuth,				///< Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are clockwise)
		AkReal32			in_fElevation,				///< Incident angle, in radians [0,pi], where 0 is the elevation (positive values are clockwise)
		AkUInt32			in_uNumChannels,			///< Desired output configuration.
		AK::SpeakerVolumes::VectorPtr out_vVolumes		///< Returned volumes (see AK::SpeakerVolumes::Vector services). Must be allocated prior to calling this function with the size returned by AK::SpeakerVolumes::Vector::GetRequiredSize() for the desired configuration.
		);
	virtual AKRESULT TermSphericalVBAP(
		AK::IAkPluginMemAlloc* in_pAllocator,			///< Memory allocator
		void*				in_pPannerData				///< Contains data relevant to the 3D panner that shoud be re-used accross plugin instances.
		);

	virtual IAkGameObjectPluginInfo * GetGameObjectInfo() { return this; }

	// Metering.
	virtual void EnableMetering( AkMeteringFlags in_eFlags );

	inline bool IsInPlaceFx(AkUInt32 in_idx) const { return !m_aFxBuffer[in_idx].HasData(); }

protected:

	inline void UpdateBypass(AkUInt32 in_idx, AkInt16 in_bypassOffset)
	{
		AKASSERT( in_idx < AK_NUM_EFFECTS_PER_OBJ );
		SetFxBypass(in_idx, m_aFX[in_idx].iBypass + in_bypassOffset);
	}

	inline void SetFxBypass(AkUInt32 in_idx, AkInt16 in_bypass)
	{
		bool bBypass = m_aFX[in_idx].iBypass != 0;
		bool bNewBypass = in_bypass != 0;

		m_bBypassChanged = bBypass != bNewBypass;
		m_aFX[in_idx].iBypass = in_bypass;

		//In case we just toggled an out-of-place fx, we must force recomputation of the speaker matrix (2d).
		if ( m_bBypassChanged )
		{
			CAkBehavioralCtx* pCtx = GetContext();
			if (pCtx != NULL)
				pCtx->InvalidatePrevPosParams();
		}
	}

	inline AkInt16 GetFxBypassAll()
	{
		return m_iBypassAllFX;
	}

	inline void SetFxBypassAll(AkInt16 in_bypass)
	{
		bool bBypassAll = m_iBypassAllFX != 0;
		bool bNewBypassAll = in_bypass != 0;

		m_bBypassChanged = bBypassAll != bNewBypassAll;
		m_iBypassAllFX = in_bypass;

		//In case we just toggled an out-of-place fx, we must force recomputation of the speaker matrix (2d).
		if ( m_bBypassChanged )
		{
			CAkBehavioralCtx* pCtx = GetContext();
			if (pCtx != NULL)
				pCtx->InvalidatePrevPosParams();
		}
	}

	struct FX : public PluginRTPCSub
	{
		AkPluginID id;					// Effect unique type ID.
		AK::IAkEffectPlugin * pEffect;	// Pointer to a bus fx filter node.
		CAkBusFXContext * pBusFXContext;// Bus FX context
		AkInt16 iBypass;				// Bypass state
		AkInt16 iLastBypass;			// Bypass state on previous buffer

		FX()
			: id( 0 )
			, pEffect( NULL )
			, pBusFXContext( NULL )
			, iBypass( 0 )
			, iLastBypass( 0 )
		{}
	};

	FX		m_aFX[ AK_NUM_EFFECTS_PER_OBJ ];

	AkPipelineBufferBase	m_aFxBuffer[AK_NUM_EFFECTS_PER_OBJ];

	AkMeterCtxFactory m_MeterCtxFactory;

	/// Mixer plugin data
	struct MixerFX : public PluginRTPCSub
	{
		inline MixerFX()
		: pPlugin( NULL )
		, id( AK_INVALID_PLUGINID )
		{}

		AK::IAkMixerEffectPlugin *	pPlugin;
		AkPluginID					id;		// Effect unique type ID.
	};

	MixerFX *				m_pMixerPlugin;
	AkDataReferenceArray	m_dataArray;

	AkInt16					m_iBypassAllFX;
	AkInt16					m_iLastBypassAllFX;

	AkUInt8					m_bEffectCreated : 1;
	AkUInt8					m_bBypassChanged : 1;

	friend class CAkMixBusCtx;
};

class CAkVPLMixBusNode : public CAkBusFX
{
public:
	~CAkVPLMixBusNode();

	AKRESULT			Init( AkVPL * in_pOwner, AkChannelConfig in_channelConfig, AkUInt16 in_uMaxFrames, const AK::CAkBusCtx &in_busContext, CAkBusFX* in_parentBus );

	void				Connect( AK::IAkMixerInputContext * in_pVoice );
	void				Disconnect( AK::IAkMixerInputContext * in_pVoice );

	void				ConsumeBuffer(
							AkAudioBuffer &			io_rVPLState
							, AkMixConnection &	in_voice
							);

	AkAudioBuffer* 		GetResultingBuffer();
	void				ReleaseBuffer();
	AkPipelineBufferBase* ProcessAllFX();
	void				PostProcessFx(AkAudioBuffer* in_pBusOutputBuffer);

	inline VPLNodeState	GetState(){ return m_eState; }
	inline bool         HasInputs() const { return !m_inputs.IsEmpty(); }

	void ResetNextVolume( AkReal32 in_dBVolume );

	inline void RefreshFx()
	{
		if (!m_bEffectCreated || m_bBypassChanged)
		{
			// Init all effect if we have not already done so.  If the bypass state has changed, pass 0 to check for possible channel configuration changes.
			AkUInt32 uFxToSet = (!m_bEffectCreated) ? ((1U << AK_NUM_EFFECTS_PER_OBJ) - 1U) : 0;
			SetInsertFxMask(uFxToSet);
		}
	}

protected:

	//
	// Helpers.
	//
	void				ProcessFX(AkUInt32 in_fxIndex, AkPipelineBufferBase*& in_pAudioBuffer, bool & io_bfxProcessed);
	void				ResetStateForNextPass();

protected:
	VPLNodeState		m_eState;
};

#endif //_AK_VPL_MIX_BUS_NODE_H_
