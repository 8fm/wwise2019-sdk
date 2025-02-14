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
// AkFXContext.h
//
// Implementation of FX context interface for source, insert and bus FX.
// These class essentially package up information into a more unified interface for the
// FX API.
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_FXCONTEXT_H_
#define _AK_FXCONTEXT_H_

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/SoundEngine/Common/AkSoundEngine.h>
#include <AK/Tools/Common/AkKeyArray.h>
#include "AkBusCtx.h"
#include "AkSettings.h"                                  // for LE_MAX_FRAMES_PER_BUFFER

class CAkUsageSlot;
class CAkBus;
class CAkVPLSrcCbxNode;
namespace AK { class IAkStreamMgr; }

struct AkDataReference
{
	AkUInt8* pData;
	AkUInt32 uSize;
	AkUInt32 uSourceID;
	CAkUsageSlot* pUsageSlot;

	~AkDataReference();
};

class AkDataReferenceArray
	: public CAkKeyArray<AkUInt32, AkDataReference>
{
public:
	~AkDataReferenceArray();

	static bool FindAlternateMedia( const CAkUsageSlot* in_pSlotToCheck, AkDataReference& io_rDataRef, AK::IAkPlugin* in_pCorrespondingFX );

	AkDataReference * AcquireData( AkUInt32 in_uDataIdx, AkUInt32 in_uSourceID );
};

//-----------------------------------------------------------------------------
// CAkEffectContextBase class.
//-----------------------------------------------------------------------------
class CAkEffectContextBase
	: public AK::IAkEffectPluginContext
{

public:
	CAkEffectContextBase( AkUInt32 in_uFXIndex );
	virtual ~CAkEffectContextBase();

	virtual AK::IAkStreamMgr * GetStreamMgr() const;
	virtual bool IsRenderingOffline() const;
	virtual AKRESULT PostMonitorMessage(const char* in_pszError, AK::Monitor::ErrorLevel in_eErrorLevel);

	bool IsUsingThisSlot( const CAkUsageSlot* in_pUsageSlot, AK::IAkPlugin* in_pCorrespondingFX );
	bool IsUsingThisSlot( const AkUInt8* in_pData );

#if (defined AK_CPU_X86 || defined AK_CPU_X86_64) && !(defined AK_IOS)
	// Return an interface to query processor specific features
	virtual AK::IAkProcessorFeatures * GetProcessorFeatures();
#endif

protected:

	AkUInt32 m_uFXIndex;
	AkDataReferenceArray m_dataArray;
};

//-----------------------------------------------------------------------------
// CAkInsertFXContext class.
//-----------------------------------------------------------------------------
class CAkInsertFXContext
	: public CAkEffectContextBase
{
public:
	CAkInsertFXContext( CAkVPLSrcCbxNode * in_pCbx, AkUInt32 in_uFXIndex );
	virtual ~CAkInsertFXContext();

	// Determine if our effect is Send Mode or Insert
	bool IsSendModeEffect() const;

	virtual AK::IAkGlobalPluginContext* GlobalContext() const{ return AK::SoundEngine::GetGlobalPluginContext(); }
	AkUInt16 GetMaxBufferLength( ) const
	{
		// Note: This should be EQUAL to the allocated buffer size that the inserted effect node sees (pitch node output)
		return LE_MAX_FRAMES_PER_BUFFER;
	}
	virtual AkUniqueID GetNodeID() const; 
	virtual AKRESULT GetOutputID(AkUInt32 & out_uOutputID, AkUInt32 & out_uDeviceType) const;

	virtual AKRESULT PostMonitorData(
		void *		in_pData,
		AkUInt32	in_uDataSize
		);

	virtual bool	 CanPostMonitorData();

	virtual AKRESULT PostMonitorMessage(
		const char* in_pszError,
		AK::Monitor::ErrorLevel in_eErrorLevel
		);

	virtual void GetPluginMedia(
		AkUInt32 in_dataIndex,		///< Index of the data to be returned.
		AkUInt8* &out_rpData,		///< Pointer to the data.
		AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
		);
	virtual void GetPluginCustomGameData(
		void* &out_rpData,			///< Pointer to the data
		AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
		);

	virtual AkReal32 GetDownstreamGain();

	virtual AKRESULT GetParentChannelConfig(AkChannelConfig& out_channelConfig) const;

	/// Obtain the interface to access voice info.
	virtual AK::IAkVoicePluginInfo * GetVoiceInfo();
	virtual AK::IAkGameObjectPluginInfo * GetGameObjectInfo();

protected:
	CAkVPLSrcCbxNode * m_pCbx;
};

//-----------------------------------------------------------------------------
// CAkBusFXContext class.
//-----------------------------------------------------------------------------
class CAkBusFX;

class CAkBusFXContext
	: public CAkEffectContextBase
{
public:
	CAkBusFXContext(
		CAkBusFX * in_pBusFX,
		AkUInt32 in_uFXIndex,
		const AK::CAkBusCtx& in_rBusContext
);
	virtual ~CAkBusFXContext();

	virtual AK::IAkGlobalPluginContext* GlobalContext() const{ return AK::SoundEngine::GetGlobalPluginContext(); }
	virtual bool IsSendModeEffect() const;
	virtual AkUInt16 GetMaxBufferLength( ) const
	{
		// Note: This should be EQUAL to the allocated buffer size that the inserted effect node sees (pitch node output)
		return LE_MAX_FRAMES_PER_BUFFER;
	}
	virtual AkUniqueID GetNodeID() const; 
	virtual AKRESULT GetOutputID(AkUInt32 & out_uOutputID, AkUInt32 & out_uDeviceType) const;

	virtual AKRESULT PostMonitorData(
		void *		in_pData,
		AkUInt32	in_uDataSize
		);

	virtual bool CanPostMonitorData();

	virtual AKRESULT PostMonitorMessage(
		const char* in_pszError,
		AK::Monitor::ErrorLevel in_eErrorLevel
		);

	virtual void GetPluginMedia(
		AkUInt32 in_dataIndex,		///< Index of the data to be returned.
		AkUInt8* &out_rpData,		///< Pointer to the data (refcounted, must be released by the plugin).
		AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
		);
	virtual void GetPluginCustomGameData(
		void* &out_rpData,			///< Pointer to the data
		AkUInt32 &out_rDataSize		///< size of the data returned in bytes.
		);

	virtual AkReal32 GetDownstreamGain();

	virtual AKRESULT GetParentChannelConfig(AkChannelConfig& out_channelConfig) const;

	// Obtain the interface to access voice info. NULL since the plugin is instantiated on a bus.
	virtual AK::IAkVoicePluginInfo * GetVoiceInfo() { return NULL; }

	virtual AK::IAkGameObjectPluginInfo * GetGameObjectInfo();

	CAkBus* GetBus(){ return m_BuxCtx.GetBus(); }
	
	virtual AKRESULT _Compute3DPositioning(
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

	virtual AKRESULT ComputeSpeakerVolumesDirect(
		AkChannelConfig		in_inputConfig,				///< Channel configuration of the input.
		AkChannelConfig		in_outputConfig,			///< Channel configuration of the mixer output.
		AkReal32			in_fCenterPerc,				///< Center percentage. Only applies to mono inputs with standard output configurations that have a center channel.
		AK::SpeakerVolumes::MatrixPtr out_mxVolumes		///< Returned volumes matrix. Must be preallocated using AK::SpeakerVolumes::Matrix::GetRequiredSize() (see AK::SpeakerVolumes::Matrix services).
	);

protected:
	CAkBusFX * m_pBusFX;
	AK::CAkBusCtx m_BuxCtx;
};

#endif // _AK_FXCONTEXT_H_
