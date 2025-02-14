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

#include "AkAudioMix.h"

#include <AK/SoundEngine/Common/IAkPlugin.h>
#include <AK/Tools/Common/AkListBareLight.h>

#include "IAkRTPCSubscriberPlugin.h"
#include "AkFxBase.h"
#include "AkBusCtx.h"
#include "AkLEngineStructs.h"
#include "AkSrcLpFilter.h"
#include "IAkGlobalPluginContextEx.h"

extern AkReal32 g_fVolumeThresholdDB;

class CAkVPLMixBusNode;
class AkVPL;
class CAkVPL3dMixable;
class AkHdrBus;

// AkMixConnection contains the volume matrix and all other voice-related structures required uniquely for each mixing graph. 
class AkMixConnection : public AK::IAkMixerInputContextEx
						, public AK::IAkVoicePluginInfo
{
public:
	AkMixConnection( 
		CAkVPL3dMixable * in_pOwner, 
		AkVPL * in_pOutputBus, 
		AkHdrBus * in_pHdrBus,
		AkConnectionType in_eConnectionType,
		bool in_bStartSilent
		);
	virtual ~AkMixConnection();

	AkVPL* GetOutputVPL() const { return pMixBus; }
	AkForceInline AkChannelConfig GetInputConfig() const { return m_BQF.GetChannelConfig(); }
	AkChannelConfig GetOutputConfig() const;
	const AK::CAkBusCtx & GetOutputBusCtx() const;
	CAkVPLMixBusNode * GetOutputBus() const;
	inline AkHdrBus *	GetHdrBus() { return m_pHdrBus; }
	inline CAkVPL3dMixable * Owner() const { return m_pOwner; }

	// Get listener ID of connection: it is the game object ID of the bus pointed to by this connection.
	inline AkGameObjectID GetListenerID() const { return m_ListenerID; }

	AkForceInline AkConnectionType GetType() const { return m_eType; }
	AkForceInline bool IsTypeDirect() const { return (m_eType == ConnectionType_Direct); }
	AkForceInline bool IsNextSilent() const { return m_bNextSilent; }
	AkForceInline bool IsPrevSilent() const { return m_bPrevSilent; }
	AkForceInline bool IsAudible() const { return mxDirect.IsAllocated() && ( !m_bNextSilent || !m_bPrevSilent ); }

	// Connection volume.
	AkForceInline AkReal32 GetVolume() const { return m_fVolume; }
	AkForceInline void SetVolume(AkReal32 in_fBaseVolume) { m_fVolume = in_fBaseVolume; }
	
	// Logical volume (includes downstream node gains; used for virtual voice and HDR calculations).
	// Pass in_bEvaluateAudibility == true if the logical volume should be evaluated against threshold. 
	// False otherwise: when connections are not on voices GetCumulativeDownstreamVolumeDB is not sufficient to determine audibility!
	AkForceInline void SetCumulativeDownstreamVolumeDB(AkReal32 in_fCumulDownstreamVolumeDB, bool in_bEvaluateAudibility)
	{ 
		m_bNextSilent = in_bEvaluateAudibility && (in_fCumulDownstreamVolumeDB <= g_fVolumeThresholdDB);
		m_fCumulDownstreamVolumeDB = in_fCumulDownstreamVolumeDB;
	}

	// Prepare for new frame: "next" volumes become "previous".
	// LPF values remain the same.
	// Cumlative downstream volume includes downstream node gains (used for virtual voice and HDR calculations) and determines audibility (if in_bEvaluateAudibility).
	AkForceInline void UpdateVolumes(AkReal32 in_fRayVolume, AkReal32 in_fConnectionVolume, AkReal32 in_fCumulDownstreamVolumeDB, bool in_bEvaluateAudibility)
	{
		m_bPrevSilent = m_bNextSilent;
		SetCumulativeDownstreamVolumeDB(in_fCumulDownstreamVolumeDB, in_bEvaluateAudibility);
		m_fVolume = in_fConnectionVolume;
		mixVolume.fPrev = mixVolume.fNext;
		mixVolume.fNext = 0.f;
		rayVolume.fPrev = rayVolume.fNext;
		rayVolume.fNext = in_fRayVolume;
	}
	
	// Optimization, used for HDR only.
	AkForceInline AkReal32 GetCumulativeDownstreamVolumeDB() const { return m_fCumulDownstreamVolumeDB; }
	
#ifndef AK_OPTIMIZED
	AkForceInline AkReal32 GetWindowTopDB() const { return m_fWindowTopDB; }
	AkForceInline void SetWindowTopDB(AkReal32 in_fWindowTopDB) { m_fWindowTopDB = in_fWindowTopDB; }
	AkUInt32 GetNumInputChannels () const { return GetInputConfig().uNumChannels; }
#endif
	AkForceInline void SetPrevSilent( bool in_bPrevSilent ) { m_bPrevSilent = in_bPrevSilent; }
	AkForceInline bool IsInHdr() const { return (m_pHdrBus != NULL); }
	AkForceInline AK::IAkPluginParam * GetAttachedPropParam() { return m_attachedPropInfo.pParam; }
	AkForceInline IAkRTPCSubscriberPlugin * GetAttachedPropRTPCSubscriber()
	{
		if ( m_attachedPropInfo.pParam && m_attachedPropInfo.pFx )
			return &m_attachedPropInfo;
		return NULL;
	}

	AkForceInline AKRESULT ResetMatrixAndFilters(AkChannelConfig in_channelConfigIn)
	{
		AKASSERT(in_channelConfigIn.uNumChannels > 0);
		AkUInt32 uNumChannelsOut = GetOutputConfig().uNumChannels;
		// Allocate channel mix matrix if needed.
		if (AK_EXPECT_FALSE(in_channelConfigIn.uNumChannels != GetInputConfig().uNumChannels))
		{
			mxDirect.Free();
			if (mxDirect.Allocate(in_channelConfigIn.uNumChannels, uNumChannelsOut) != AK_Success)
				return AK_Fail;
			mxDirect.ClearPrevious(in_channelConfigIn.uNumChannels, uNumChannelsOut);
			mxDirect.ClearNext(in_channelConfigIn.uNumChannels, uNumChannelsOut);
			
			if (m_BQF.ChangeChannelConfig(in_channelConfigIn) != AK_Success)
				return AK_Fail;
		}

		if (mxDirect.IsAllocated())
		{
			// Move old "Next" volumes to "Previous".
			mxDirect.Swap(in_channelConfigIn.uNumChannels, uNumChannelsOut);
			return AK_Success;
		}
		return AK_Fail;
	}

	// Clear previous and next mix volume values to mute this voice.
	AkForceInline void Mute()
	{
		mixVolume.fPrev = 0;
		mixVolume.fNext = 0;
	}

	// Copy next base volume values to previous.
	AkForceInline void CopyNextToPrevMixVolumes()
	{
		mixVolume.fPrev	= mixVolume.fNext;
		rayVolume.fPrev	= rayVolume.fNext;
	}
	
	// Clear next spatialization matrix.
	AkForceInline void ClearNextSpatialization()
	{
		mxDirect.ClearNext(GetInputConfig().uNumChannels, GetOutputConfig().uNumChannels );
	}

	// Copy next channel matrix to previous.
	AkForceInline void CopyNextToPrevSpatialization()
	{
		mxDirect.CopyNextToPrev(GetInputConfig().uNumChannels, GetOutputConfig().uNumChannels );
	}	

	// Copy previous channel matrix to next.
	AkForceInline void CopyPrevToNextSpatialization()
	{
		mxDirect.CopyPrevToNext(GetInputConfig().uNumChannels, GetOutputConfig().uNumChannels );
	}

	// Get and set for m_BQF
	AkForceInline AkReal32 LastLPF() const { return m_BQF.GetLPFPar(); }
	AkForceInline AkReal32 LastHPF() const { return m_BQF.GetHPFPar(); }

	AkForceInline void UpdateLPF() { m_BQF.SetLPFPar( fLPF ); }
	AkForceInline void UpdateLPH() { m_BQF.SetHPFPar( fHPF ); }

	AkForceInline CAkSrcLpHpFilter* GetFilterContext()
	{
		return &m_BQF;
	}

	AkForceInline void ConsumeBufferBQF( AkVPLState &outOfPlaceCopy )
	{
		AKASSERT(outOfPlaceCopy.HasData());
		m_BQF.Execute(&outOfPlaceCopy);
	}

	virtual bool GetListeners(AkGameObjectID* out_aListenerIDs, AkUInt32& io_uSize) const;

	void SetFeedback(bool in_bFeedback) { m_bFeedback = in_bFeedback; }
	bool GetFeedback() const { return m_bFeedback; }

#ifndef AK_OPTIMIZED
	void SetMute()
	{ 
		m_bIsMonitoringSolo = false;
		m_bIsMonitoringMute = true; 
		m_bMuteSoloExplicit = true;
	}
	void SetSolo()
	{ 
		m_bIsMonitoringSolo = true; 
		m_bIsMonitoringMute = false;
		m_bMuteSoloExplicit = true;
	}
	void SetImplicitMute()
	{
		m_bIsMonitoringSolo = false;
		m_bIsMonitoringMute = true;
		m_bMuteSoloExplicit = false;
	}
	void SetImplicitSolo()
	{
		m_bIsMonitoringSolo = true;
		m_bIsMonitoringMute = false;
		m_bMuteSoloExplicit = false;
	}
	void ClearMuteSolo(bool bIsSoloActive)
	{
		m_bIsMonitoringSolo = false;
		m_bIsMonitoringMute = bIsSoloActive; // Default state is muted, if there is an active solo.
		m_bMuteSoloExplicit = false;
	}
	
	bool IsMonitoringMute() const { return m_bIsMonitoringMute; }
	bool IsMonitoringSolo() const { return m_bIsMonitoringSolo; }
	bool IsMuteSoloExplicit() const	{ return m_bMuteSoloExplicit; }
#endif

public:
	AkRamp		mixVolume;				// Mix volume: 2-point volume pushed to this connection by its owner, normally including the owner's gain plus this connection's gain.
	AkRamp		rayVolume;				// Ray specific volume (attributed to game object position). Set to 1 in the multiposition case, as rays volume is baked in each rays contribution to speaker volume matrix.
										// REVIEW: 2-point volume for convenience for mixer plugins.
	AkAudioMix	mxDirect;
	AkMixConnection * pNextInput;		// For AkInputConnectionList
	AkMixConnection * pNextLightItem;	// For AkMixConnectionList
protected:	// TODO protect all.
	CAkVPL3dMixable * m_pOwner;
	AkVPL*		pMixBus;

	AkHdrBus *		m_pHdrBus;

	void *			m_pUserData;			// User-defined data (used by mixer plugins).
	PluginRTPCSub	m_attachedPropInfo;
	AkGameObjectID  m_ListenerID;

public:

	AkReal32	fLPF;		    // Connection LPF ("output bus" LPF + occlusion + obstruction if direct).
	AkReal32	fHPF;		    // Connection HPF ("output bus" HPF + occlusion + obstruction if direct).

protected:

	CAkSrcLpHpFilter m_BQF;

	AkReal32	m_fVolume;					// Volume gain of this connection (linear).
	AkReal32	m_fCumulDownstreamVolumeDB;	// Cumulative downstream volume of this connection in dB (includes owner's gain). Maximum in all signal paths. Used to determine HDR window and audibility
	AkReal32	m_fWindowTopDB;				// Max window top (in dB).
	AkConnectionType	m_eType;			// Connection type (send, direct, ...)
	AkUInt32	m_bNextSilent				:1;
	AkUInt32	m_bPrevSilent				:1;
	AkUInt32	m_bAttachedPropCreated		:1;
	AkUInt32	m_bFeedback					:1; // sends audio upstream in the graph
#ifndef AK_OPTIMIZED
	AkUInt32	m_bIsMonitoringMute : 1;
	AkUInt32	m_bIsMonitoringSolo : 1;
	AkUInt32	m_bMuteSoloExplicit : 1;
#endif
protected:
	
	// AK::IAkVoicePluginInfo interface
	virtual AkPlayingID GetPlayingID() const;
	virtual AkPriority GetPriority() const;
	virtual AkPriority ComputePriorityWithDistance(
		AkReal32 in_fDistance
		) const;
	
	// AK::IAkGameObjectPluginInfo interface
	virtual AkGameObjectID GetGameObjectID() const;
	virtual AkUInt32 GetNumEmitterListenerPairs() const;
	virtual AKRESULT GetEmitterListenerPair( AkUInt32 in_uIndex, AkEmitterListenerPair & out_emitterListenerPair ) const;
	virtual AkUInt32 GetNumGameObjectPositions() const;
	virtual AK::SoundEngine::MultiPositionType GetGameObjectMultiPositionType() const;
	virtual AkReal32 GetGameObjectScaling() const;
	virtual AKRESULT GetGameObjectPosition( AkUInt32 in_uIndex, AkSoundPosition & out_position ) const;	
	virtual AKRESULT GetListenerData(AkGameObjectID in_uListener, AkListener & out_listener) const;

	// AK::IAkMixerInputContext interface
	virtual AK::IAkPluginParam * GetInputParam();
	virtual AK::IAkVoicePluginInfo * GetVoiceInfo();
	virtual AK::IAkGameObjectPluginInfo * GetGameObjectInfo();
	virtual AkConnectionType GetConnectionType();
	virtual AkUniqueID GetAudioNodeID();
	virtual void * GetUserData();
	virtual void SetUserData( void * in_pUserData );
	virtual AkReal32 GetCenterPerc();
	virtual AkSpeakerPanningType GetSpeakerPanningType();
	virtual void GetPannerPosition(AkVector & out_position);
	virtual bool HasListenerRelativeRouting();
	virtual Ak3DPositionType Get3DPositionType();
	virtual AkUInt32 GetNum3DPositions();
	virtual AKRESULT Get3DPosition( AkUInt32 in_uIndex, AkEmitterListenerPair & out_soundPosition );
	virtual AkReal32 GetSpread( AkUInt32 in_uIndex );
	virtual AkReal32 GetFocus( AkUInt32 in_uIndex );
	virtual bool GetMaxAttenuationDistance( AkReal32 & out_fMaxAttenuationDistance );
	virtual void GetSpatializedVolumes( AK::SpeakerVolumes::MatrixPtr out_mxPrevVolumes, AK::SpeakerVolumes::MatrixPtr out_mxNextVolumes );
	virtual Ak3DSpatializationMode Get3DSpatializationMode();

	// AK::IAkMixerInputContextEx interface
	virtual AkReal32 GetSpreadFullDistance(AkUInt32 in_uIndex);
	virtual AkReal32 GetFocusFullDistance(AkUInt32 in_uIndex);
};

template <class T> struct AkInputConnectionListNextItem
{
	static AkForceInline T *& Get(T * in_pItem)
	{
		return in_pItem->pNextInput;
	}
};

typedef AkListBare<AkMixConnection, AkInputConnectionListNextItem, AkCountPolicyNoCount, AkLastPolicyWithLast> AkInputConnectionList;
typedef AkListBare<AkMixConnection, AkListBareLightNextItem, AkCountPolicyNoCount, AkLastPolicyNoLast> AkMixConnectionList;

// CAkMixConnections wraps a list of volume matrices and other voice-related structures required uniquely for each mixing graph. 
class CAkMixConnections : public AkMixConnectionList
{
public:

	~CAkMixConnections()
	{
		while ( First() )
		{
			AkMixConnection * pConnection = First();
			RemoveFirst();
			AkDeleteAligned( AkMemID_Processing, pConnection );
		}
		Term();
	}

	AkMixConnection * Create(
		CAkVPL3dMixable * in_pOwner,
		AkVPL *	in_pOutputBus,
		AkHdrBus * in_pHdrBus,
		AkConnectionType in_eConnectionType,
		bool in_bStartSilent
		);

	AkForceInline IteratorEx FindEx( AkVPL * in_pOutputBus )
	{
		AkMixConnectionList::IteratorEx it = BeginEx();
		while ( it != End() )
		{
			if ((*it)->GetOutputVPL() == in_pOutputBus)
				break;
			++it;
		}
		return it;
	}
	
	/// LX REVIEW Might want to perform garbage collection of unused connection on HW platforms as well. Pass in an AkUniqueID.
	AkForceInline AkMixConnectionList::IteratorEx Remove( AkMixConnectionList::IteratorEx& io_rIter )
	{
		AkMixConnection *pToDelete = *io_rIter;
		io_rIter = Erase(io_rIter);
		AkDeleteAligned(AkMemID_Processing, pToDelete);
		return io_rIter;
	}

	// Find first connection that matches given output.
	AkForceInline AkMixConnection * Find(AK::CAkBusCtx & in_output)
	{
		// Search for this device.
		AkMixConnectionList::Iterator it = Begin();
		while (it != End())
		{
			if ((*it)->GetOutputBusCtx() == in_output)
				return (*it);
			++it;
		}
		return NULL;
	}
};

