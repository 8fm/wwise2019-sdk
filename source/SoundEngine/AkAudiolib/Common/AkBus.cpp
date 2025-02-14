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
// AkBus.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkBus.h"
#include "AkAudioMgr.h"
#include "AkTransitionManager.h"
#include "AkRegistryMgr.h"
#include "AkActionDuck.h"
#include "AkLEngine.h"
#include "AkBankMgr.h"
#include "AkDuckItem.h"
#include "AkOutputMgr.h"

AkBusArray CAkBus::s_BGMBusses;
AkBusArray CAkBus::s_TopBusses;
bool CAkBus::s_bIsBackgroundMusicMuted = false;
bool CAkBus::s_bIsSharesetInitialized = false;

CAkBus::CAkBus(AkUniqueID in_ulID)
:CAkActiveParent<CAkParameterNodeBase>(in_ulID)
,m_pMixerPlugin(NULL)
,m_RecoveryTime(0)
,m_fMaxDuckVolume(AK_DEFAULT_MAX_BUS_DUCKING)
,m_idDeviceShareset(AK_INVALID_DEVICE_ID)
,m_eDuckingState(DuckState_OFF)
,m_bHdrReleaseModeExponential( false )
,m_bHdrReleaseTimeDirty( true )
,m_bHdrGainComputerDirty( true )
, m_bIsBackgroundMusicBus(false)
#ifdef  WWISE_AUTHORING
, m_idDevice(0)
#endif //  WWISE_AUTHORING
{
}

CAkBus::~CAkBus()
{
	UnsetAsBackgroundMusicBus();

	for( AkDuckedVolumeList::Iterator iter = m_DuckedVolumeList.Begin(); iter != m_DuckedVolumeList.End(); ++iter )
	{
		(*iter).item.Term(); //Terminating the CAkDuckItem in the list
	}
	m_DuckedVolumeList.Term();

	for( AkDuckedVolumeList::Iterator iter = m_DuckedBusVolumeList.Begin(); iter != m_DuckedBusVolumeList.End(); ++iter )
	{
		(*iter).item.Term(); //Terminating the CAkDuckItem in the list
	}
	m_DuckedBusVolumeList.Term();

	m_ToDuckList.Term();

	g_csMain.Lock();
	for(AkBusArray::Iterator it = s_TopBusses.Begin(); it != s_TopBusses.End(); ++it)
	{
		if(*it.pItem == this)
		{
			s_TopBusses.Erase(it);
			if(s_TopBusses.Length() == 0)
			{
				s_TopBusses.Term();
				s_bIsSharesetInitialized = false;
				CAkBankMgr::SignalLastBankUnloaded();
			}
			break;
		}
	}	
	g_csMain.Unlock();
	
	m_mapBusChildId.Term();

	if ( m_pMixerPlugin )
	{
		AkDelete(AkMemID_Structure, m_pMixerPlugin );
	}
}

CAkBus* CAkBus::Create( AkUniqueID in_ulID )
{
	CAkBus* pBus = AkNew(AkMemID_Structure, CAkBus(in_ulID));
	if( pBus )
	{
		if( pBus->Init() != AK_Success )
		{
			pBus->Release();
			pBus = NULL;
		}
	}

	return pBus;
}

AkNodeCategory CAkBus::NodeCategory()
{
	return AkNodeCategory_Bus;
}

void CAkBus::ExecuteAction( ActionParams& in_rAction )
{
	if( IsActiveOrPlaying() )
	{
		// Process children in reverse order in order to avoid breaking the array if 
		// they self-destruct within ExecuteAction().
		in_rAction.bIsFromBus = true;
		AkUInt32 uIndex = m_mapChildId.Length();
		while ( uIndex > 0 )
		{
			m_mapChildId[--uIndex]->ExecuteAction( in_rAction );
			if( uIndex > m_mapChildId.Length() )
			{
				// SYU-98386-513 - WG-24620
				// Patch: During the iteration some of the children were possibly deleted and the index must be reset to its maximum logical value.
				// Without this patch, this code would definitely crash on next iteration, so it is apparently a good thing to do.
				// We at the moment have no idea of how exactly it hapenned, but so far 2 users reported this issue and this patch worked for them...
				// Better send some useless notifications than crashing systematically
				uIndex = m_mapChildId.Length();
			}
		}
		uIndex = m_mapBusChildId.Length();
		while ( uIndex > 0 )
		{
			m_mapBusChildId[--uIndex]->ExecuteAction( in_rAction );
			if( uIndex > m_mapBusChildId.Length() )
			{
				// SYU-98386-513 - WG-24620
				// Patch: During the iteration some of the children were possibly deleted and the index must be reset to its maximum logical value.
				// Without this patch, this code would definitely crash on next iteration, so it is apparently a good thing to do.
				// We at the moment have no idea of how exactly it hapenned, but so far 2 users reported this issue and this patch worked for them...
				// Better send some useless notifications than crashing systematically
				uIndex = m_mapBusChildId.Length();
			}
		}

		// WARNING: "this" may have self-destructed during last call to pNode->ExecuteAction( in_rAction );
	}
}

void CAkBus::ExecuteActionExcept( ActionParamsExcept& in_rAction )
{
	CheckPauseTransition(in_rAction);

	in_rAction.bIsFromBus = true;

	// Process children in reverse order in order to avoid breaking the array if 
	// they self-destruct within ExecuteAction().
	in_rAction.bIsFromBus = true;
	AkUInt32 uIndex = m_mapChildId.Length();
	while ( uIndex > 0 )
	{
		CAkParameterNodeBase* pChild = m_mapChildId[--uIndex];
		if(!IsException( pChild, *(in_rAction.pExeceptionList) ) )
			pChild->ExecuteActionExceptParentCheck( in_rAction );
		if( uIndex > m_mapChildId.Length() )
		{
			// SYU-98386-513 - WG-24620
			// Patch: During the iteration some of the children were possibly deleted and the index must be reset to its maximum logical value.
			// Without this patch, this code would definitely crash on next iteration, so it is apparently a good thing to do.
			// We at the moment have no idea of how exactly it hapenned, but so far 2 users reported this issue and this patch worked for them...
			// Better send some useless notifications than crashing systematically
			uIndex = m_mapChildId.Length();
		}
	}

	uIndex = m_mapBusChildId.Length();
	while ( uIndex > 0 )
	{
		CAkParameterNodeBase* pChild = m_mapBusChildId[--uIndex];
		if(!IsException( pChild, *(in_rAction.pExeceptionList) ) )
			pChild->ExecuteActionExceptParentCheck( in_rAction );
		if( uIndex > m_mapBusChildId.Length() )
		{
			// SYU-98386-513 - WG-24620
			// Patch: During the iteration some of the children were possibly deleted and the index must be reset to its maximum logical value.
			// Without this patch, this code would definitely crash on next iteration, so it is apparently a good thing to do.
			// We at the moment have no idea of how exactly it hapenned, but so far 2 users reported this issue and this patch worked for them...
			// Better send some useless notifications than crashing systematically
			uIndex = m_mapBusChildId.Length();
		}
	}
}

void CAkBus::ExecuteMasterBusAction( ActionParams& in_params )
{
	if( in_params.eType == ActionParamType_Pause )
	{
		g_pTransitionManager->PauseAllStateTransitions();
	}
	else if( in_params.eType == ActionParamType_Resume )
	{
		g_pTransitionManager->ResumeAllStateTransitions();
	}

// Enable this code to make notifications go straight to the Renderer PBI list instead of going through the whole hierarchy.
// But cannot enable it untill All Switch Containers and Music nodes that are doing things on *::ExecuteAction( ... ) calls are not handled in another way.

// #define SUPER_FAST_EXECUTE_ALL_NOTIF

#ifdef SUPER_FAST_EXECUTE_ALL_NOTIF

	CAkURenderer::ProcessCommandAnyNode( in_params );

#else //SUPER_FAST_EXECUTE_ALL_NOTIF

	//Apply this to the Master Busses.  Need to copy because busses could disappear during termination time.
	AkBusArray copy;
	copy.Reserve(s_TopBusses.Length());
	for(AkBusArray::Iterator it = s_TopBusses.Begin(); it != s_TopBusses.End(); ++it)
	{
		copy.AddLast((*it));
		(*it)->AddRef();
	}

	for(AkBusArray::Iterator it = copy.Begin(); it != copy.End(); ++it)
	{
		(*it)->ExecuteAction(in_params);
		(*it)->Release();
	}	

	copy.Term();

#endif //SUPER_FAST_EXECUTE_ALL_NOTIF

}

void CAkBus::ExecuteMasterBusActionExcept( ActionParamsExcept& in_params )
{
	//Apply this to the Master Busses.  Need to copy because busses could disappear during termination time.
	AkBusArray copy;
	copy.Reserve(s_TopBusses.Length());
	for(AkBusArray::Iterator it = s_TopBusses.Begin(); it != s_TopBusses.End(); ++it)
	{
		copy.AddLast((*it));
		(*it)->AddRef();
	}

	for(AkBusArray::Iterator it = copy.Begin(); it != copy.End(); ++it)
	{
		(*it)->ExecuteActionExcept(in_params);
		(*it)->Release();
	}

	copy.Term();
}

void CAkBus::PlayToEnd( CAkRegisteredObj * in_pGameObj , CAkParameterNodeBase* in_NodePtr, AkPlayingID in_PlayingID /* = AK_INVALID_PLAYING_ID */ )
{
	for( ChildrenIterator iter(m_mapBusChildId,m_mapChildId); !iter.End(); ++iter )
	{
		CAkParameterNodeBase* pNode = (*iter);
		pNode->PlayToEnd( in_pGameObj, in_NodePtr, in_PlayingID);
	}
}

void CAkBus::ForAllPBI( 
		AkForAllPBIFunc in_funcForAll,
		const AkRTPCKey & in_rtpcKey,
		void * in_pCookie )
{
	if( IsActivityChunkEnabled() )
	{
		for (ChildrenIterator iter(m_mapBusChildId, m_mapChildId); !iter.End(); ++iter)
		{
			if( (*iter)->IsPlaying() )
			{
				(*iter)->ForAllPBI( in_funcForAll, in_rtpcKey, in_pCookie );
			}
		}
	}
}

void CAkBus::PropagatePositioningNotification(
		AkReal32				in_RTPCValue,	// 
		AkRTPC_ParameterID		in_ParameterID,	// RTPC ParameterID, must be a Positioning ID.
		CAkRegisteredObj*		in_GameObj,		// Target Game Object
		AkRTPCExceptionChecker*	in_pExceptCheck /*= NULL*/
		)
{
	if ( !IsTopBus() )
		CAkLEngine::PositioningChangeNotification( ID(), in_RTPCValue, in_ParameterID );
}

AKRESULT CAkBus::GetAudioParameters(AkSoundParams &io_Parameters, AkMutedMap& io_rMutedMap, const AkRTPCKey& in_rtpcKey, AkPBIModValues* io_pRanges, AkModulatorsToTrigger* in_pTriggerModulators, bool in_bDoBusCheck /*= true*/, CAkParameterNodeBase* in_pStopAtNode)
{	
	AkDeltaMonitorObjBrace braceDelta(key);
	if (io_Parameters.Request.IsSet(AkPropID_Volume))			
		io_Parameters.AddToVolume(GetDuckedVolume(AkPropID_Volume), AkDelta_Ducking);

	GetPropAndRTPCAndState(io_Parameters, in_rtpcKey);
	
	ApplyTransitionsOnProperties(io_Parameters, io_rMutedMap, in_rtpcKey);
	
	//Gather modulators to trigger
	if (in_pTriggerModulators != NULL && HasModulator())
	{
		AkModulatorSubscriberInfo subcrbrInfo;
		subcrbrInfo.pSubscriber = GetSubscriberPtr();
		subcrbrInfo.eSubscriberType = CAkRTPCMgr::SubscriberType_CAkParameterNodeBase;

		// Buses do not support game-obj, Note-scoped, or voice-scoped modulators, because they are shared.
		//subcrbrInfo.eNarrowestSupportedScope = AkModulatorScope_Global;

		g_pModulatorMgr->GetModulators(subcrbrInfo, *in_pTriggerModulators);
	}

	if (s_bIsBackgroundMusicMuted && m_bIsBackgroundMusicBus)
	{
		AkMutedMapItem item;
		item.m_bIsPersistent = false;
		item.m_bIsGlobal = true;
		item.m_Identifier = this;		
		item.m_eReason = AkDelta_BGMMute;
		io_rMutedMap.Set(item, AK_MUTED_RATIO);//This override possible other mute level		
	}

	io_Parameters.Request.UnsetBit(AkPropID_BusVolume);
	if (m_pBusOutputNode != NULL && in_pStopAtNode != m_pBusOutputNode)
	{
		m_pBusOutputNode->GetAudioParameters(io_Parameters, io_rMutedMap, in_rtpcKey, io_pRanges, in_pTriggerModulators, false, in_pStopAtNode);
	}
	return AK_Success;
}

void CAkBus::GetNonMixingBusParameters(AkSoundParams &io_Params, const AkRTPCKey & in_rtpcKey)
{
	if (this->IsMixingBus())
		return;

	io_Params.Request.ClearBits();
	io_Params.Request.SetBit(AkPropID_BusVolume);
	io_Params.Request.SetBit(AkPropID_OutputBusVolume);
	io_Params.Request.SetBit(AkPropID_OutputBusLPF);
	io_Params.Request.SetBit(AkPropID_OutputBusHPF);

	GetControlBusParams(io_Params, in_rtpcKey);
}

void CAkBus::GetMixingBusParameters(AkSoundParams &io_Parameters, const AkRTPCKey& in_rtpcKey )
{
	AKASSERT(IsMixingBus());

	io_Parameters.Request.SetBit(AkPropID_BusVolume);
	io_Parameters.Request.AddUserAuxProps();
	io_Parameters.Request.AddGameDefProps();
	io_Parameters.Request.AddOutputBusProps();
	io_Parameters.Request.AddSpatialAudioProps();
	GetControlBusParams(io_Parameters, in_rtpcKey);
	io_Parameters.Accumulate(AkPropID_Volume, io_Parameters.BusVolume(), AkDelta_None);	//No logging, already done in GetControlBusParams
	
	// User Defined
	if (m_pAuxChunk)
	{	
		for (int i = 0; i < AK_NUM_USER_AUX_SEND_PER_OBJ; ++i)
		{
			io_Parameters.aAuxSend[i] = m_pAuxChunk->aAux[i];
		}
	}
	
	// Game Defined
	io_Parameters.bGameDefinedAuxEnabled = m_bUseGameAuxSends;
	
	// Spatial Audio
	io_Parameters.reflectionsAuxBus = m_reflectionsAuxBus;
}

void CAkBus::TriggerModulators( const AkModulatorTriggerParams& in_params, CAkModulatorData* out_pModPBIData, bool in_bStopAtMixingBus ) const
{
	if (HasModulator())
	{
		AkModulatorSubscriberInfo subcrbrInfo;
		subcrbrInfo.pSubscriber = GetSubscriberPtr();
		subcrbrInfo.eSubscriberType = CAkRTPCMgr::SubscriberType_CAkParameterNodeBase;

		subcrbrInfo.pTargetNode = const_cast<CAkBus*>(this);
		
		// #REVIEW  - support game object scoped modulators on voice properties?
		subcrbrInfo.eNarrowestSupportedScope = AkModulatorScope_GameObject;

		g_pModulatorMgr->Trigger(subcrbrInfo, in_params, out_pModPBIData);
	}

	if (m_pBusOutputNode && (!in_bStopAtMixingBus || !((CAkBus*)(m_pBusOutputNode))->IsMixingBus()))
	{
		m_pBusOutputNode->TriggerModulators(in_params, out_pModPBIData);
	}
}

void CAkBus::GetControlBusParams(AkSoundParams &io_Params, const AkRTPCKey& in_rtpcKey)	
{
	AkDeltaMonitorObjBrace braceDelta(key);
	GetPropAndRTPCAndState(io_Params, in_rtpcKey);

	AkMutedMap dummy;
	ApplyTransitionsOnProperties(io_Params, dummy, in_rtpcKey);
	dummy.Term();
	
	io_Params.AddToBusVolume(GetDuckedVolume(AkPropID_BusVolume), AkDelta_Ducking);

	if (m_pBusOutputNode != NULL && !((CAkBus*)(m_pBusOutputNode))->IsMixingBus())
	{
		AkSoundParams paramsParent;
		paramsParent.Request = io_Params.Request;
		paramsParent.Request.RemoveUserAuxProps();
		paramsParent.Request.RemoveGameDefProps();				
		static_cast<CAkBus*>(m_pBusOutputNode)->GetControlBusParams(paramsParent, in_rtpcKey);
		io_Params.Accumulate(AkPropID_OutputBusVolume, paramsParent.BusVolume(), AkDelta_None);
		io_Params.Accumulate(AkPropID_OutputBusVolume, paramsParent.OutputBusVolume(), AkDelta_None);
		io_Params.Accumulate(AkPropID_OutputBusLPF, paramsParent.OutputBusLPF(), AkDelta_None);
		io_Params.Accumulate(AkPropID_OutputBusHPF, paramsParent.OutputBusHPF(), AkDelta_None);
	}
}

void CAkBus::SetAkProp( AkPropID in_eProp, AkReal32 in_fValue, AkReal32 in_fMin, AkReal32 in_fMax )
{
	AkDeltaMonitorObjBrace braceObj(ID());
#ifdef AKPROP_TYPECHECK
	AKASSERT( typeid(AkReal32) == *g_AkPropTypeInfo[ in_eProp ] );
#endif

	if ( in_eProp >= AkPropID_Volume && in_eProp <= AkPropID_BusVolume )
	{
		AkReal32 fDelta = in_fValue - m_props.GetAkProp( in_eProp, 0.0f ).fValue;
		if ( fDelta != 0.0f )
		{
			AkRTPC_ParameterID rtpcID = g_AkPropRTPCID[in_eProp];
			if (Parent() == NULL || rtpcID < RTPC_OVERRIDABLE_PARAMS_START || (rtpcID >= RTPC_OVERRIDABLE_PARAMS_START && rtpcID != RTPC_MaxNumRTPC && m_overriddenParams.IsSet(rtpcID)))
				AkDeltaMonitor::LogAkProp(in_eProp, in_fValue, fDelta);
			Notification(rtpcID, fDelta, in_fValue);
			m_props.SetAkProp( in_eProp, in_fValue );
		}
	}
	else if ( in_eProp >= AkPropID_PAN_LR && in_eProp <= AkPropID_CenterPCT )
	{
		AkReal32 fDelta = in_fValue - m_props.GetAkProp( in_eProp, 0.0f ).fValue;
		if ( fDelta != 0.0f )
		{
			PositioningChangeNotification( in_fValue, g_AkPropRTPCID[ in_eProp ], NULL );
			m_props.SetAkProp( in_eProp, in_fValue );
		}
	}
	else
	{
		CAkParameterNodeBase::SetAkProp( in_eProp, in_fValue, in_fMin, in_fMax );
	}

}

void CAkBus::PushParamUpdate( AkRTPC_ParameterID in_ParamID, const AkRTPCKey& in_rtpcKey, AkReal32 in_fValue, AkReal32 in_fDeltaValue, AkRTPCExceptionChecker* in_pExceptCheck )
{
	if ( in_ParamID == RTPC_HDRBusReleaseTime )
	{
		// HDR release time does not need to be notified to children, but dirty flag must be 
		// set to have lower engine recompute filter coefficient.
		m_bHdrReleaseTimeDirty = true;
	}
	else if ( in_ParamID == RTPC_HDRBusThreshold || in_ParamID == RTPC_HDRBusRatio )
	{
		// HDR gain computer settings do not need to be notified to children, but dirty flag must be 
		// set to have lower engine cache new values.
		m_bHdrGainComputerDirty = true;
	}
	else
	{
		CAkRTPCSubscriberNode::PushParamUpdate(in_ParamID, in_rtpcKey, in_fValue, in_fDeltaValue, in_pExceptCheck);
	}
}

void CAkBus::NotifyParamChanged( bool in_bLiveEdit, AkRTPC_ParameterID in_rtpcID )
{
	m_bHdrReleaseTimeDirty = true;
	m_bHdrGainComputerDirty = true;

	CAkParameterNodeBase::NotifyParamChanged(in_bLiveEdit, in_rtpcID);
}

void CAkBus::NotifyParamsChanged(bool in_bLiveEdit, const AkRTPCBitArray& in_bitArray)
{
	m_bHdrReleaseTimeDirty = true;
	m_bHdrGainComputerDirty = true;

	CAkParameterNodeBase::NotifyParamsChanged(in_bLiveEdit, in_bitArray);
}


void CAkBus::ParamNotification( NotifParams& in_rParams )
{
	AKASSERT( in_rParams.rtpcKey.GameObj() == NULL );

	// Note: master bus and bus volumes are applied lower in the hierarchy when the is no effect, 
	// otherwise they are applied at the proper level to avoid having pre-effect volumes
	in_rParams.bIsFromBus = true;
	if ( IsMixingBus() )
	{
		if ( in_rParams.eType == RTPC_HDRBusReleaseTime )
		{
			// HDR release time does not need to be notified to children, but dirty flag must be 
			// set to have lower engine recompute filter coefficient.
			m_bHdrReleaseTimeDirty = true;
		}
		else if ( in_rParams.eType == RTPC_HDRBusThreshold
				  || in_rParams.eType == RTPC_HDRBusRatio )
		{
			// HDR gain computer settings do not need to be notified to children, but dirty flag must be 
			// set to have lower engine cache new values.
			m_bHdrGainComputerDirty = true;
		}
		else if (in_rParams.eType == RTPC_Volume // NOTE: These are the 'voice properties' on busses.
				 || in_rParams.eType == RTPC_Pitch
				 || in_rParams.eType == RTPC_LPF
				 || in_rParams.eType == RTPC_HPF
				 || in_rParams.eType == RTPC_MakeUpGain)
		{
			if (IsActiveOrPlaying())
			{
				// Propagate notification to all children
				AKASSERT(IsActivityChunkEnabled());
				{
					for (ChildrenIterator iter(m_mapBusChildId, m_mapChildId); !iter.End(); ++iter)
					{
						// Checking IsActiveOrPlaying instead of IsPlaying to also include Sub Busses tails.
						if ((*iter)->IsActiveOrPlaying())
						{
							(*iter)->ParamNotification(in_rParams);
						}
					}
				}
			}
		}
		else 
		{
			CAkLEngine::MixBusParamNotification(ID(), in_rParams);
		}		
	}
	else
	{
		AKASSERT( in_rParams.pExceptCheck == NULL );
		{
			if ( IsActiveOrPlaying() )
			{

				if ( in_rParams.eType == RTPC_Volume // NOTE: These are the 'voice properties' on busses.
					 || in_rParams.eType == RTPC_Pitch
					 || in_rParams.eType == RTPC_LPF
					 || in_rParams.eType == RTPC_HPF
					 || in_rParams.eType == RTPC_MakeUpGain
					 || in_rParams.eType == RTPC_BusVolume	// Plus the special collapsable properties, which require notifying children in case they get collapsed onto them.
					 || in_rParams.eType == RTPC_OutputBusVolume
					 || in_rParams.eType == RTPC_OutputBusLPF
					 || in_rParams.eType == RTPC_OutputBusHPF
					 )
				{
					// Propagate notification to all children
					AKASSERT( IsActivityChunkEnabled() );
					{
						for ( ChildrenIterator iter( m_mapBusChildId, m_mapChildId ); !iter.End(); ++iter )
						{
							// Checking IsActiveOrPlaying instead of IsPlaying to also include Sub Busses tails.
							if ( (*iter)->IsActiveOrPlaying() )
							{
								(*iter)->ParamNotification( in_rParams );
							}
						}
					}
				}
			}
		}
	}
}

void CAkBus::MuteNotification(AkReal32 in_fMuteRatio, AkMutedMapItem& in_rMutedItem, bool in_bIsFromBus /*= false*/)
{
	if( IsActivityChunkEnabled() )
	{
		for (ChildrenIterator iter(m_mapBusChildId, m_mapChildId); !iter.End(); ++iter)
		{
			if( (*iter)->IsPlaying() )
			{
				if( s_bIsBackgroundMusicMuted && m_bIsBackgroundMusicBus && in_rMutedItem.m_Identifier == this )
				{
					in_fMuteRatio = AK_MUTED_RATIO;
				}
				(*iter)->MuteNotification(in_fMuteRatio, in_rMutedItem, true);
			}
		}
	}
}

AKRESULT CAkBus::SetRTPC(
	AkRtpcID			in_RTPC_ID,
	AkRtpcType			in_RTPCType,
	AkRtpcAccum			in_RTPCAccum,
	AkRTPC_ParameterID	in_ParamID,
	AkUniqueID			in_RTPCCurveID,
	AkCurveScaling		in_eScaling,
	AkRTPCGraphPoint*	in_pArrayConversion,		// NULL if none
	AkUInt32			in_ulConversionArraySize,	// 0 if none
	bool				/*in_bNotify*/
	)
{
	AKASSERT(g_pRTPCMgr);

	AKASSERT((unsigned int)in_ParamID < sizeof(AkUInt64) * 8);

	bool bWasMixing = IsMixingBus();

	AKRESULT eResult = CAkParameterNodeBase::SetRTPC(in_RTPC_ID, in_RTPCType, in_RTPCAccum, in_ParamID, in_RTPCCurveID, in_eScaling, in_pArrayConversion, in_ulConversionArraySize);

	if (bWasMixing != IsMixingBus() && IsActivityChunkEnabled())
		CAkLEngine::ReevaluateGraph(true);

	return eResult;
}

void CAkBus::UnsetRTPC(
	AkRTPC_ParameterID in_ParamID,
	AkUniqueID in_RTPCCurveID
	)
{
	bool bWasMixing = IsMixingBus();

	CAkParameterNodeBase::UnsetRTPC(in_ParamID, in_RTPCCurveID);

	if (bWasMixing != IsMixingBus() && IsActivityChunkEnabled())
		CAkLEngine::ReevaluateGraph(true);
}

void CAkBus::SetStateProperties(
	AkUInt32 in_uProperties,
	AkStatePropertyUpdate* in_pProperties,
	bool in_bNotify
	)
{
	bool bWasMixing = IsMixingBus();

	CAkParameterNodeBase::SetStateProperties(in_uProperties, in_pProperties, in_bNotify);

	if (bWasMixing != IsMixingBus() && IsActivityChunkEnabled())
		CAkLEngine::ReevaluateGraph(true);
}

AkStateGroupChunk* CAkBus::AddStateGroup(
	AkStateGroupID in_ulStateGroupID,
	bool in_bNotify
	)
{
	bool bWasMixing = IsMixingBus();

	AkStateGroupChunk* chunk = CAkParameterNodeBase::AddStateGroup(in_ulStateGroupID, in_bNotify);

	if (bWasMixing != IsMixingBus() && IsActivityChunkEnabled())
		CAkLEngine::ReevaluateGraph(true);

	return chunk;
}

void CAkBus::RemoveStateGroup(
	AkStateGroupID in_ulStateGroupID,
	bool in_bNotify
	)
{
	bool bWasMixing = IsMixingBus();

	CAkParameterNodeBase::RemoveStateGroup(in_ulStateGroupID, in_bNotify);

	if (bWasMixing != IsMixingBus() && IsActivityChunkEnabled())
		CAkLEngine::ReevaluateGraph(true);
}

AKRESULT CAkBus::UpdateStateGroups(
	AkUInt32 in_uGroups,
	AkStateGroupUpdate* in_pGroups,
	AkStateUpdate* in_pUpdates
	)
{
	bool bWasMixing = IsMixingBus();

	AKRESULT result = CAkParameterNodeBase::UpdateStateGroups(in_uGroups, in_pGroups, in_pUpdates);

	if (bWasMixing != IsMixingBus() && IsActivityChunkEnabled())
		CAkLEngine::ReevaluateGraph(true);

	return result;
}

void CAkBus::NotifyBypass(
		AkUInt32 in_bitsFXBypass,
		AkUInt32 in_uTargetMask,
		CAkRegisteredObj * in_GameObj,
		AkRTPCExceptionChecker* in_pExceptCheck /* = NULL */)
{
	CAkLEngine::BypassBusFx( ID(), in_bitsFXBypass, in_uTargetMask, in_GameObj );
}

AKRESULT CAkBus::CanAddChild( CAkParameterNodeBase * in_pAudioNode )
{
	AKASSERT(in_pAudioNode);

	AKRESULT eResult = AK_Success;
	bool bConnectingBus = in_pAudioNode->IsBusCategory();
	if(in_pAudioNode->ParentBus() != NULL)
	{
		eResult = AK_ChildAlreadyHasAParent;
	}
	else if( !bConnectingBus && m_mapChildId.Exists( in_pAudioNode->ID()) )
	{
		eResult = AK_AlreadyConnected;
	}
	else if( bConnectingBus && m_mapBusChildId.Exists( in_pAudioNode->ID()) )
	{
		eResult = AK_AlreadyConnected;
	}
	else if( bConnectingBus && ID() == in_pAudioNode->ID() )
	{
		eResult = AK_CannotAddItseflAsAChild;
	}
	return eResult;	
}

AKRESULT CAkBus::AddChildInternal( CAkParameterNodeBase* pAudioNode )
{
	// Need to support on-the-fly rerouting.
	if (pAudioNode->ParentBus() != NULL)
	{
		if (pAudioNode->ParentBus() == this)
		{
			// Already connected.
			pAudioNode->Release();
			return AK_Success;
		}
		pAudioNode->ParentBus()->RemoveChild(pAudioNode);
	}

	AKRESULT eResult = CanAddChild(pAudioNode);
	if(eResult == AK_Success)
	{
		CAkParameterNodeBase** ppNode = NULL;
		if( pAudioNode->IsBusCategory() )
		{
			ppNode = m_mapBusChildId.AddNoSetKey( pAudioNode->ID() );					
		}
		else
		{
			ppNode = m_mapChildId.AddNoSetKey( pAudioNode->ID() );
		}
		if( !ppNode )
		{
			eResult = AK_Fail;
		}
		else
		{
			*ppNode = pAudioNode;
			pAudioNode->ParentBus(this);
			this->AddRef();			
		}
	}

	pAudioNode->Release();

	return eResult;
}

AKRESULT CAkBus::AddChild( WwiseObjectIDext in_ulID )
{
	if(!in_ulID.id)
	{
		return AK_InvalidID;
	}
	
	CAkParameterNodeBase* pAudioNode = g_pIndex->GetNodePtrAndAddRef( in_ulID );
	if ( !pAudioNode )
		return AK_IDNotFound;

	return AddChildInternal( pAudioNode );
}

void CAkBus::RemoveChild( CAkParameterNodeBase* in_pChild )
{
	AKASSERT(in_pChild);

	if( in_pChild->ParentBus() == this )
	{
		in_pChild->ParentBus(NULL);
		if( in_pChild->IsBusCategory() )
		{
			m_mapBusChildId.Unset(in_pChild->ID());
		}
		else
		{
			m_mapChildId.Unset(in_pChild->ID());
		}
		this->Release();
	}
}

void CAkBus::RemoveChild( WwiseObjectIDext in_ulID )
{
	AKASSERT(in_ulID.id);

	CAkParameterNodeBase** l_pANPtr = NULL;
	if( in_ulID.bIsBus )
	{
		l_pANPtr = m_mapBusChildId.Exists(in_ulID.id);
	}
	else
	{
		l_pANPtr = m_mapChildId.Exists(in_ulID.id);
	}
	if( l_pANPtr )
	{
		RemoveChild(*l_pANPtr);
	}
}

///////////////////////////////////////////////////////////////////////////////
//                       DUCKING FUNCTIONS
///////////////////////////////////////////////////////////////////////////////
AKRESULT CAkBus::AddDuck(
		AkUniqueID in_BusID,
		AkVolumeValue in_DuckVolume,
		AkTimeMs in_FadeOutTime,
		AkTimeMs in_FadeInTime,
		AkCurveInterpolation in_eFadeCurve,
		AkPropID in_TargetProp
		)
{
	AKASSERT(in_BusID);
	
	AkDuckInfo * pDuckInfo = m_ToDuckList.Set( in_BusID );
	if ( pDuckInfo )
	{
		AKRESULT res = AK_Success;

		pDuckInfo->DuckVolume = in_DuckVolume;
		pDuckInfo->FadeCurve = in_eFadeCurve;
		pDuckInfo->FadeInTime = in_FadeInTime;
		pDuckInfo->FadeOutTime = in_FadeOutTime;
		pDuckInfo->TargetProp = in_TargetProp;

#ifndef AK_OPTIMIZED
		if ( m_eDuckingState == DuckState_ON 
			||  m_eDuckingState == DuckState_PENDING )
		{
			CAkBus* pBus = static_cast<CAkBus*>(g_pIndex->GetNodePtrAndAddRef( in_BusID, AkNodeType_Bus ));
			if( pBus )
			{
				// Use 0 fade-time to handle case where volume is modified in app (which sends a AddDuck)
				pBus->Duck( ID(), in_DuckVolume, 0, in_eFadeCurve, in_TargetProp );
				pBus->Release();
			}
		}
#endif
		CAkBus* pBus = static_cast<CAkBus*>(g_pIndex->GetNodePtrAndAddRef( in_BusID, AkNodeType_Bus ));
		if( pBus )
		{
			res = pBus->m_DuckerNode.EnableParam(g_AkPropRTPCID[in_TargetProp]);
			pBus->Release();
		}

		return res;
	}
	
	return AK_Fail;
}

AKRESULT CAkBus::RemoveDuck(
		AkUniqueID in_BusID
		)
{
	AKASSERT( g_pIndex );

#ifndef AK_OPTIMIZED
	AkDuckInfo* pDuckInfo = m_ToDuckList.Exists( in_BusID );
	if( pDuckInfo )
	{
		CAkBus* pBus = static_cast<CAkBus*>(g_pIndex->GetNodePtrAndAddRef( in_BusID, AkNodeType_Bus ));
		if( pBus )
		{
			// Remove potential duck that may have occured on the Ducked Bus
			pBus->Unduck( ID(), 0, pDuckInfo->FadeCurve, pDuckInfo->TargetProp );
			pBus->Release();
		}
	}
#endif //AK_OPTIMIZED
	m_ToDuckList.Unset(in_BusID);
	return AK_Success;
}

AKRESULT CAkBus::RemoveAllDuck()
{
	m_ToDuckList.RemoveAll();
	return AK_Success;
}

void CAkBus::SetRecoveryTime(AkUInt32 in_RecoveryTime)
{
	//Here recovery time received is already in samples
	if( in_RecoveryTime <= AK_NUM_VOICE_REFILL_FRAMES ) // WG-19832
		m_RecoveryTime = 0;
	else
		m_RecoveryTime = in_RecoveryTime;
}

void CAkBus::SetMaxDuckVolume( AkReal32 in_fMaxDuckVolume )
{
	if ( in_fMaxDuckVolume == m_fMaxDuckVolume )
		return;

	AkReal32 fOldDuckedBusVolume = GetDuckedVolume( AkPropID_BusVolume );
	AkReal32 fOldDuckedVoiceVolume = GetDuckedVolume( AkPropID_Volume );
	m_fMaxDuckVolume = in_fMaxDuckVolume;

	AkReal32 fNewDuckedBusVolume = GetDuckedVolume( AkPropID_BusVolume );
	AkReal32 fNewDuckedVoiceVolume = GetDuckedVolume( AkPropID_Volume );

	AkDeltaMonitor::OpenUpdateBrace(AkDelta_Ducking, key);
	if (fNewDuckedBusVolume != fOldDuckedBusVolume)
	{
		m_DuckerNode.PushParamUpdate_All( g_AkPropRTPCID[AkPropID_BusVolume], fNewDuckedBusVolume, fNewDuckedBusVolume - fOldDuckedBusVolume );
	}

	if (fNewDuckedVoiceVolume != fOldDuckedVoiceVolume )
	{
		m_DuckerNode.PushParamUpdate_All( g_AkPropRTPCID[AkPropID_Volume], fNewDuckedVoiceVolume, fNewDuckedVoiceVolume - fOldDuckedVoiceVolume );
	}
	AkDeltaMonitor::CloseUpdateBrace(key);
}

void CAkBus::Duck(
		AkUniqueID in_BusID,
		AkVolumeValue in_DuckVolume,
		AkTimeMs in_FadeOutTime,
		AkCurveInterpolation in_eFadeCurve,
		AkPropID in_PropID
		)
{
	AKRESULT eResult = AK_Success;
	CAkDuckItem* pDuckItem = NULL;
	AkDuckedVolumeList* pDuckedList = NULL;
	switch( in_PropID )
	{
	case AkPropID_Volume:
		pDuckedList = &m_DuckedVolumeList;
		break;
	case AkPropID_BusVolume:
		pDuckedList = &m_DuckedBusVolumeList;
		break;
	default:
		break;
	}
	AKASSERT( pDuckedList );

	pDuckItem = pDuckedList->Exists(in_BusID);
	if(!pDuckItem)
	{
		pDuckItem = pDuckedList->Set(in_BusID);
		if ( pDuckItem )
		{
			pDuckItem->Init( this );
		}
		else
		{
			eResult = AK_Fail;
		}
	}
	
	MONITOR_BUSNOTIFICATION( ID(), AkMonitorData::BusNotification_Ducked, 0, 0);

	if(eResult == AK_Success)
	{
		eResult = m_DuckerNode.EnableParam(g_AkPropRTPCID[in_PropID]);

		if (eResult == AK_Success)
		{
			pDuckItem->m_clearOnTransitionTermination = false;
			StartDuckTransitions(pDuckItem, in_DuckVolume, AkValueMeaning_Offset, in_eFadeCurve, in_FadeOutTime, in_PropID);
		}
	}
}

void CAkBus::Unduck(
		AkUniqueID in_BusID,
		AkTimeMs in_FadeInTime,
		AkCurveInterpolation in_eFadeCurve,
		AkPropID in_PropID
		)
{
	AkDuckedVolumeList* pDuckedList = NULL;
	switch( in_PropID )
	{
	case AkPropID_Volume:
		pDuckedList = &m_DuckedVolumeList;
		break;
	case AkPropID_BusVolume:
		pDuckedList = &m_DuckedBusVolumeList;
		break;
	default:
		break;
	}
	AKASSERT( pDuckedList );

	CAkDuckItem* pDuckItem = pDuckedList->Exists(in_BusID);
	if(pDuckItem)
	{
		pDuckItem->m_clearOnTransitionTermination = true;
		StartDuckTransitions(pDuckItem, 0, AkValueMeaning_Default, in_eFadeCurve, in_FadeInTime, in_PropID);
		CheckDuck();
	}
}

void CAkBus::PauseDuck(
		AkUniqueID in_BusID
		)
{
	CAkDuckItem* pDuckItem = m_DuckedVolumeList.Exists(in_BusID);
	if(pDuckItem)
	{
		//Setting the transition time to zero with initial value, which will freeze the duck value while waiting
		pDuckItem->m_clearOnTransitionTermination = false;
		StartDuckTransitions(pDuckItem, pDuckItem->m_EffectiveVolumeOffset, AkValueMeaning_Independent, AkCurveInterpolation_Linear, 0, AkPropID_Volume );
	}
	pDuckItem = m_DuckedBusVolumeList.Exists(in_BusID);
	if(pDuckItem)
	{
		//Setting the transition time to zero with initial value, which will freeze the duck value while waiting
		pDuckItem->m_clearOnTransitionTermination = false;
		StartDuckTransitions(pDuckItem, pDuckItem->m_EffectiveVolumeOffset, AkValueMeaning_Independent, AkCurveInterpolation_Linear, 0, AkPropID_BusVolume);
	}
}

void CAkBus::StartDuckTransitions(CAkDuckItem*		in_pDuckItem,
										AkReal32			in_fTargetValue,
										AkValueMeaning	in_eValueMeaning,
										AkCurveInterpolation		in_eFadeCurve,
										AkTimeMs		in_lTransitionTime,
										AkPropID		in_ePropID)
{
	AKASSERT(g_pTransitionManager);

	// have we got one running already ?
	if(in_pDuckItem->m_pvVolumeTransition != NULL)
	{
		// yup, let's change the direction it goes
		g_pTransitionManager->ChangeParameter(in_pDuckItem->m_pvVolumeTransition,
													in_ePropID,
													in_fTargetValue,
													in_lTransitionTime,
													in_eFadeCurve,
													in_eValueMeaning);
	}
	else
	{
		AkReal32 fStartValue = in_pDuckItem->m_EffectiveVolumeOffset;
		AkReal32 fTargetValue = 0.0f;
		switch(in_eValueMeaning)
		{
		case AkValueMeaning_Offset:
		case AkValueMeaning_Independent:
			fTargetValue = in_fTargetValue;
			break;
		case AkValueMeaning_Default:
			break;
		default:
			AKASSERT(!"Invalid Meaning type");
			break;
		}

		// do we really need to start a transition ?
		if((fStartValue == fTargetValue)
			|| (in_lTransitionTime == 0))
		{
			AkDeltaMonitor::OpenUpdateBrace(AkDelta_Ducking);
			in_pDuckItem->TransUpdateValue( in_ePropID, fTargetValue, true );
			AkDeltaMonitor::CloseUpdateBrace();
		}
		// start it
		else
		{
			TransitionParameters VolumeParams(
				in_pDuckItem,
				in_ePropID,
				fStartValue,
				fTargetValue,
				in_lTransitionTime,
				in_eFadeCurve,
				AkDelta_Ducking,
				true,
				true );
			in_pDuckItem->m_pvVolumeTransition = g_pTransitionManager->AddTransitionToList(VolumeParams);
		}
	}
}

void CAkBus::ApplyMaxNumInstances( AkUInt16 in_u16MaxNumInstance )
{
	UpdateMaxNumInstanceGlobal( in_u16MaxNumInstance );

	m_u16MaxNumInstance = in_u16MaxNumInstance;
}

void CAkBus::StartDucking()
{
	m_eDuckingState = DuckState_ON;
	UpdateDuckedBus();
}

void CAkBus::StopDucking()
{
	//Must stop Ducking
	if( m_RecoveryTime && !m_ToDuckList.IsEmpty() )
	{
		AKRESULT eResult = RequestDuckNotif();
		if(eResult == AK_Success)
		{
			m_eDuckingState = DuckState_PENDING;
		}
		else
		{
			// If we cannot get a notif, then just skip the notif and ask for fade back right now.
			// Better doing it now than never
			m_eDuckingState = DuckState_OFF;
		}
	}
	else
	{
		m_eDuckingState = DuckState_OFF;
	}

	UpdateDuckedBus();
}

AKRESULT CAkBus::IncrementPlayCount( CounterParameters& io_params, bool in_bNewInstance, bool in_bSwitchingOutputbus )
{
	AKRESULT eResult = AK_Success;
	
	if ( in_bNewInstance )
		eResult = IncrementPlayCountValue(true);

	if ( !in_bSwitchingOutputbus && !io_params.bMaxConsidered )
	{
		ProcessGlobalLimiter( io_params, in_bNewInstance );
		io_params.bMaxConsidered = MaxNumInstIgnoreParent();
	}

	if(m_pBusOutputNode)
	{
		AKRESULT newResult = m_pBusOutputNode->IncrementPlayCount( io_params, in_bNewInstance, in_bSwitchingOutputbus );
		eResult = GetNewResultCodeForVirtualStatus( eResult, newResult );
	}

	if(GetPlayCount() == 1)
	{
		//Must start Ducking
		StartDucking();
	}

	return eResult;
}

void CAkBus::DecrementPlayCount( 
	CounterParameters& io_params
	)
{
	DecrementPlayCountValue(true);

	CheckAndDeleteActivityChunk();

	if(m_pBusOutputNode)
	{
		m_pBusOutputNode->DecrementPlayCount( io_params );
	}

	AKASSERT( !m_pParentNode ); // Should never happen as actually a bus never has parents, just making sure.

	if(GetPlayCount() == 0)
	{
		StopDucking();
	}
}

bool CAkBus::IncrementActivityCount( AkUInt16 in_flagForwardToBus)
{
	bool bIsSuccessful = IncrementActivityCountValue(true);

	if( m_pBusOutputNode )
	{
		bIsSuccessful &= m_pBusOutputNode->IncrementActivityCount();
	}

	return bIsSuccessful;
}

void CAkBus::DecrementActivityCount(AkUInt16 in_flagForwardToBus)
{
	DecrementActivityCountValue(true);

	if( m_pBusOutputNode )
	{
		m_pBusOutputNode->DecrementActivityCount();
	}
}

bool CAkBus::IsOrIsChildOf( CAkParameterNodeBase * in_pNodeToTest )
{
	CAkBus* pBus = this;
	while( pBus )
	{
		if( in_pNodeToTest == pBus )
			return true;
		pBus = static_cast<CAkBus*>( pBus->ParentBus() );
	}
	return false;
}

void CAkBus::DuckNotif()
{
	if(m_eDuckingState == DuckState_PENDING)
	{
		m_eDuckingState = DuckState_OFF;
		UpdateDuckedBus();
	}
}

void CAkBus::UpdateDuckedBus()
{
	for( AkToDuckList::Iterator iter = m_ToDuckList.Begin(); iter != m_ToDuckList.End(); ++iter )
	{
		MapStruct<AkUniqueID, AkDuckInfo>&  rItem = *iter;
		CAkBus* pBus = static_cast<CAkBus*>(g_pIndex->GetNodePtrAndAddRef( rItem.key, AkNodeType_Bus ));
		if(pBus)
		{
			switch(m_eDuckingState)
			{
			case DuckState_OFF:
				pBus->Unduck(ID(),rItem.item.FadeInTime, rItem.item.FadeCurve, rItem.item.TargetProp);
				break;

			case DuckState_ON:
				pBus->Duck(ID(),rItem.item.DuckVolume, rItem.item.FadeOutTime, rItem.item.FadeCurve, rItem.item.TargetProp);
				break;

			case DuckState_PENDING:
				pBus->PauseDuck(ID());
				break;

			default:
				AKASSERT(!"Unknown Ducking State");
			}
			pBus->Release();
		}
	}
}

AKRESULT CAkBus::RequestDuckNotif()
{
	AKRESULT eResult = AK_Fail; 
	CAkActionDuck* pActionDuck = AkNew(AkMemID_Event, CAkActionDuck(AkActionType_Duck,0));
	if(pActionDuck)
	{
		if ( pActionDuck->SetAkProp( AkPropID_DelayTime, (AkInt32) m_RecoveryTime, 0, 0 ) == AK_Success )
		{
			WwiseObjectID wwiseId( ID(), AkNodeType_Bus );
			pActionDuck->SetElementID( wwiseId );

			AkPendingAction* pPendingAction = AkNew(AkMemID_Object, AkPendingAction( NULL ) );
			if(pPendingAction)
			{
				pPendingAction->pAction = pActionDuck;
				g_pAudioMgr->EnqueueOrExecuteAction(pPendingAction);
				eResult = AK_Success;
			}
		}

		pActionDuck->Release();
	}

	return eResult;
}

void CAkBus::CheckDuck()
{
	bool bIsDucked = false;
	for( AkDuckedVolumeList::Iterator iter = m_DuckedVolumeList.Begin(); iter != m_DuckedVolumeList.End(); ++iter )
	{
		// Should be != 0.0f, but the transition result on a DB value, after conversion is innacurate.
		if( (*iter).item.m_EffectiveVolumeOffset < -0.01f )
		{
			bIsDucked = true;
			break;
		}
	}
	if(!bIsDucked)
	{
		for( AkDuckedVolumeList::Iterator iter = m_DuckedBusVolumeList.Begin(); iter != m_DuckedBusVolumeList.End(); ++iter )
		{
			// Should be != 0.0f, but the transition result on a DB value, after conversion is innacurate.
			if( (*iter).item.m_EffectiveVolumeOffset < -0.01f )
			{
				bIsDucked = true;
				break;
			}
		}
	}

	if(!bIsDucked)
	{
		MONITOR_BUSNOTIFICATION( ID(), AkMonitorData::BusNotification_Unducked, 0, 0 );
	}
}

AkVolumeValue CAkBus::GetDuckedVolume( AkPropID in_eProp )
{
	AkDuckedVolumeList* pDuckedList = NULL;
	switch(in_eProp)
	{
	case AkPropID_Volume:
		pDuckedList = &m_DuckedVolumeList;
		break;
	case AkPropID_BusVolume:
		pDuckedList = &m_DuckedBusVolumeList;
		break;
	default:
		break;
	}
	AkVolumeValue l_DuckedVolume = 0;
	for( AkDuckedVolumeList::Iterator iter = pDuckedList->Begin(); iter != pDuckedList->End(); ++iter )
	{
		l_DuckedVolume += (*iter).item.m_EffectiveVolumeOffset;
	}

	if( l_DuckedVolume < m_fMaxDuckVolume )
	{
		l_DuckedVolume = m_fMaxDuckVolume;
	}

	return l_DuckedVolume;
}

#ifndef AK_OPTIMIZED
void CAkBus::InvalidatePositioning()
{
	//Busses do NOT invalidate children path, they have their own.
	
	CAkLEngine::InvalidatePositioning(ID());
}
#endif

void CAkBus::ChannelConfig( AkChannelConfig in_channelConfig )
{
	AkChannelConfig oldConfig = m_channelConfig;

	if ( in_channelConfig.eConfigType == AK_ChannelConfigType_Standard )
	{
		// Clamp standard channel config to what platform supports.
		m_channelConfig.SetStandard( in_channelConfig.uChannelMask & AK_SUPPORTED_STANDARD_CHANNEL_MASK );
	}
	else
	{
#ifdef AK_71AUDIO
		m_channelConfig = in_channelConfig;
#else
		// Esoteric configs not supported on platforms that don't support 7.1.
		m_channelConfig.Clear();
#endif
	}

	if ( oldConfig != m_channelConfig )
	{
		CAkLEngine::UpdateChannelConfig(ID());
	}
}

CAkBus* CAkBus::GetMixingBus()
{
	if( IsMixingBus() )
	{
		return this;
	}
	return CAkParameterNodeBase::GetMixingBus();
}

void CAkBus::UpdateFx(
		AkUInt32 in_uFXIndex
		)
{
	CAkLEngine::UpdateMixBusFX( ID(), in_uFXIndex );
}

bool CAkBus::HasEffect()
{
	if ( m_pFXChunk )
	{
		for ( AkUInt32 i = 0; i < AK_NUM_EFFECTS_PER_OBJ; ++i )
			if ( m_pFXChunk->aFX[i].id != AK_INVALID_UNIQUE_ID )
				return true;
	}

	return false;
}

bool CAkBus::IsMixingBus()
{
	// Important note: 
	// Modifying this function?
	// Make sure you do the equivalent change on Authoring side of thing in BusHelpers::IsMixingBus(...)
	return HasEffect()
		|| (NodeCategory() == AkNodeCategory_AuxBus)
		|| ChannelConfig().IsValid()
		|| m_bHasListenerRelativeRouting
		|| (m_ePannerType != AK_DirectSpeakerAssignment)
		|| IsTopBus()
		|| IsHdrBus()
		|| m_pMixerPlugin
		|| m_pAuxChunk
		|| m_bUseGameAuxSends
		|| HasRTPC(RTPC_BusVolume) // Because listeners can have different rtpc-values for different bus instances. Not states.
		|| HasRTPC(RTPC_OutputBusVolume)
		|| HasRTPC(RTPC_OutputBusLPF)
		|| HasRTPC(RTPC_OutputBusHPF)
		;
}

bool CAkBus::IsTopBus() 
{ 
	return ParentBus() == NULL;
}

AKRESULT CAkBus::SetInitialParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize )
{
	AKRESULT eResult = m_props.SetInitialParams( io_rpData, io_rulDataSize );
	if (eResult != AK_Success)
		return eResult;

	eResult = SetPositioningParams(io_rpData, io_rulDataSize);
	if (eResult != AK_Success)
		return eResult;

	eResult = SetAuxParams(io_rpData, io_rulDataSize);
	if (eResult != AK_Success)
		return eResult;

	AkUInt8 byBitVector = READBANKDATA(AkUInt8, io_rpData, io_rulDataSize);
	bool bKillNewest					= GETBANKDATABIT( byBitVector, BANK_BITPOS_BUSADVSETTINGS_KILL_NEWEST ) != 0;
	bool bUseVirtualBehavior			= GETBANKDATABIT( byBitVector, BANK_BITPOS_BUSADVSETTINGS_USE_VIRTUAL ) != 0;
	SetMaxReachedBehavior( bKillNewest );
	SetOverLimitBehavior( bUseVirtualBehavior );

	m_u16MaxNumInstance					= READBANKDATA( AkUInt16, io_rpData, io_rulDataSize );
	bool bIsMaxNumInstIgnoreParent		= GETBANKDATABIT( byBitVector, BANK_BITPOS_BUSADVSETTINGS_MAXNUMINST_IGNORE );
	SetMaxNumInstIgnoreParent(bIsMaxNumInstIgnoreParent);
	AkUInt32 uChannelConfig				= READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
	AkChannelConfig channelConfig;
	channelConfig.Deserialize( uChannelConfig );
	ChannelConfig( channelConfig );

	bool bIsBackgroundMusic = GETBANKDATABIT( byBitVector, BANK_BITPOS_BUSADVSETTINGS_BACKGROUND_MUSIC ) != 0;

	byBitVector = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
	bool bIsHdrBus = GETBANKDATABIT( byBitVector, BANK_BITPOS_BUSHDR_ENABLE );
	SetHdrBus(bIsHdrBus);
	m_bHdrReleaseModeExponential = GETBANKDATABIT( byBitVector, BANK_BITPOS_BUSHDR_RELEASE_MODE_EXP );

	if( bIsBackgroundMusic )
	{
		SetAsBackgroundMusicBus();
	}

	return AK_Success;
}

AKRESULT CAkBus::SetInitialValues(AkUInt8* in_pData, AkUInt32 in_ulDataSize)
{
	AkNodeCategory eCat = NodeCategory();
	if (eCat != AkNodeCategory_Bus && eCat != AkNodeCategory_AuxBus)
	{
		g_pBankManager->ReportDuplicateObject(key, AkNodeCategory_Bus, eCat);
		return AK_DuplicateUniqueID;
	}

	AKRESULT eResult = AK_Success;

	bool bAddAsTop = false;	
	//Read ID
	// We don't care about the ID, just skip it
	SKIPBANKDATA(AkUInt32, in_pData, in_ulDataSize);

	AkUniqueID OverrideBusId = READBANKDATA(AkUInt32, in_pData, in_ulDataSize);

	if(OverrideBusId)
	{
		CAkParameterNodeBase* pBus = g_pIndex->GetNodePtrAndAddRef( OverrideBusId, AkNodeType_Bus );
		if(pBus)
		{
			eResult = pBus->AddChildByPtr( this );
			pBus->Release();
		}
		else
		{
			// It is now an error to not load the bank content in the proper order.
			MONITOR_ERRORMSG( AKTEXT("Master bus structure not loaded: make sure that the first bank to be loaded contains the master bus information") );
			eResult = AK_Fail;
		}
	}
	else
	{
		bAddAsTop = true;		
		m_idDeviceShareset = READBANKDATA(AkUInt32, in_pData, in_ulDataSize);
	}	

	if(eResult == AK_Success)
	{
		eResult = SetInitialParams( in_pData, in_ulDataSize );
	}

	if(eResult == AK_Success)
	{
		SetRecoveryTime( AkTimeConv::MillisecondsToSamples( READBANKDATA( AkTimeMs, in_pData, in_ulDataSize ) ) );
		m_fMaxDuckVolume = READBANKDATA( AkReal32, in_pData, in_ulDataSize );

		//Process Child list
		AkUInt32 ulDucks = READBANKDATA( AkUInt32, in_pData, in_ulDataSize );
		for(AkUInt32 i = 0; i < ulDucks; ++i)
		{
			AkUniqueID BusID	= READBANKDATA( AkUInt32, in_pData, in_ulDataSize );
			AkReal32 DuckVolume	= READBANKDATA( AkReal32, in_pData, in_ulDataSize );
			AkTimeMs FadeOut	= READBANKDATA( AkTimeMs, in_pData, in_ulDataSize );
			AkTimeMs FadeIn		= READBANKDATA( AkTimeMs, in_pData, in_ulDataSize );
			AkUInt8 FadeCurve	= READBANKDATA( AkUInt8, in_pData, in_ulDataSize );
			AkUInt8 TargetProp	= READBANKDATA( AkUInt8, in_pData, in_ulDataSize );

			eResult = AddDuck( BusID, DuckVolume, FadeOut, FadeIn, (AkCurveInterpolation)FadeCurve, (AkPropID)TargetProp );

			if(eResult != AK_Success)
			{
				break;
			}
		}

	}

	if(eResult == AK_Success)
	{
		eResult = SetInitialFxParams(in_pData, in_ulDataSize, false);
	}

	// Read attachable property override.
	m_bOverrideAttachmentParams = READBANKDATA(AkUInt8, in_pData, in_ulDataSize);

	if(eResult == AK_Success)
	{
		eResult = SetInitialRTPC(in_pData, in_ulDataSize, this);
	}

	if(eResult == AK_Success)
	{
		eResult = ReadStateChunk(in_pData, in_ulDataSize);
	}

	CHECKBANKDATASIZE( in_ulDataSize, eResult );	
	if(eResult == AK_Success)
	{
		if(bAddAsTop)
		{
			AKASSERT(ParentBus() == NULL);			
			CAkFunctionCritical SpaceSetAsCritical;
			eResult = SetMasterBus();
		}
		else
		{
			m_idDeviceShareset = ((CAkBus*)ParentBus())->m_idDeviceShareset;
		}
	}
	return eResult;
}

AKRESULT CAkBus::SetInitialFxParams( AkUInt8*& io_rpData, AkUInt32& io_rulDataSize, bool /*in_bPartialLoadOnly*/ )
{
	AKRESULT eResult = AK_Success;

	// Read Num Fx
	AkUInt32 uNumFx = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
	AKASSERT(uNumFx <= AK_NUM_EFFECTS_PER_OBJ);
	if ( uNumFx )
	{
		AkUInt32 bitsFXBypass = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );

		for ( AkUInt32 uFX = 0; uFX < uNumFx; ++uFX )
		{
			AkUInt32 uFXIndex = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
			AkUniqueID fxID = READBANKDATA( AkUniqueID, io_rpData, io_rulDataSize);
			AkUInt8 bIsShareSet = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
			SKIPBANKDATA( AkUInt8, io_rpData, io_rulDataSize ); // bIsRendered

			if(fxID != AK_INVALID_UNIQUE_ID )
			{
				eResult = SetFX( uFXIndex, fxID, bIsShareSet != 0, SharedSetOverride_Bank );
			}

			if( eResult != AK_Success )
				break;
		}

		MainBypassFX( bitsFXBypass );
	}

	// Mixer plugin.
	if( eResult == AK_Success )
	{
		AkUniqueID fxID = READBANKDATA( AkUniqueID, io_rpData, io_rulDataSize);
		AkUInt8 bIsShareSet = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
		eResult = SetMixerPlugin( fxID, bIsShareSet != 0, SharedSetOverride_Bank );
	}

	//Set the override fx mask for busses, which will always need to subscribe to FX.
	m_overriddenParams.Mask(RTPC_FX_PARAMS_BITFIELD, true);

	return eResult;
}

void CAkBus::Parent(CAkParameterNodeBase* in_pParent)
{
	AKASSERT( !"Parent of a bus should always be NULL" );
}

bool CAkBus::GetStateSyncTypes( AkStateGroupID in_stateGroupID, CAkStateSyncArray* io_pSyncTypes )
{
	if( CheckSyncTypes( in_stateGroupID, io_pSyncTypes ) )
		return true;

	if( ParentBus() )
	{
		return static_cast<CAkBus*>( ParentBus() )->GetStateSyncTypes( in_stateGroupID, io_pSyncTypes );
	}
	return false;
}

void CAkBus::GetFX( AkUInt32 in_uFXIndex, AkFXDesc& out_rFXInfo, CAkRegisteredObj *	in_GameObj)
{
	AKASSERT( in_uFXIndex <  AK_NUM_EFFECTS_PER_OBJ );

	if ( m_pFXChunk )
	{
		FXStruct & fx = m_pFXChunk->aFX[ in_uFXIndex ];
		if ( fx.id != AK_INVALID_UNIQUE_ID )
		{
			if ( fx.bShareSet )
				out_rFXInfo.pFx.Attach( g_pIndex->m_idxFxShareSets.GetPtrAndAddRef( fx.id ) );
			else
				out_rFXInfo.pFx.Attach( g_pIndex->m_idxFxCustom.GetPtrAndAddRef( fx.id ) );
		}
		else
		{
			out_rFXInfo.pFx = NULL;
		}

		out_rFXInfo.iBypassed = GetBypassFX( in_uFXIndex, in_GameObj );
	}
	else
	{
		out_rFXInfo.pFx = NULL;
		out_rFXInfo.iBypassed = 0;
	}
}

void CAkBus::GetFXDataID(
		AkUInt32	in_uFXIndex,
		AkUInt32	in_uDataIndex,
		AkUInt32&	out_rDataID
		)
{
	AKASSERT( in_uFXIndex <  AK_NUM_EFFECTS_PER_OBJ );

	out_rDataID = AK_INVALID_SOURCE_ID;

	if ( m_pFXChunk )
	{
		FXStruct & fx = m_pFXChunk->aFX[ in_uFXIndex ];

		CAkFxBase * pFx;
		if ( fx.bShareSet )
			pFx = g_pIndex->m_idxFxShareSets.GetPtrAndAddRef( fx.id );
		else
			pFx = g_pIndex->m_idxFxCustom.GetPtrAndAddRef( fx.id );

		if ( pFx )
		{
			out_rDataID = pFx->GetMediaID( in_uDataIndex );
			pFx->Release();
		}
	}
}

AkInt16 CAkBus::GetBypassAllFX(CAkRegisteredObj * in_GameObjPtr)
{
	return _GetBypassAllFX(in_GameObjPtr);
}

void CAkBus::GetMixerPlugin( AkFXDesc& out_rFXInfo )
{
	out_rFXInfo.pFx = NULL;
	out_rFXInfo.iBypassed = 0;

	if ( m_pMixerPlugin )
	{
		FXStruct & fx = m_pMixerPlugin->plugin;
		if ( fx.id != AK_INVALID_UNIQUE_ID )
		{
			if ( fx.bShareSet )
				out_rFXInfo.pFx.Attach( g_pIndex->m_idxFxShareSets.GetPtrAndAddRef( fx.id ) );
			else
				out_rFXInfo.pFx.Attach( g_pIndex->m_idxFxCustom.GetPtrAndAddRef( fx.id ) );
		}
	}
}

void CAkBus::GetMixerPluginDataID(
		AkUInt32	in_uDataIndex,
		AkUInt32&	out_rDataID
		)
{
	out_rDataID = AK_INVALID_SOURCE_ID;

	if ( m_pMixerPlugin )
	{
		FXStruct & fx = m_pMixerPlugin->plugin;

		CAkFxBase * pFx;
		if ( fx.bShareSet )
			pFx = g_pIndex->m_idxFxShareSets.GetPtrAndAddRef( fx.id );
		else
			pFx = g_pIndex->m_idxFxCustom.GetPtrAndAddRef( fx.id );

		if ( pFx )
		{
			out_rDataID = pFx->GetMediaID( in_uDataIndex );
			pFx->Release();
		}
	}
}

AKRESULT CAkBus::SetMixerPlugin( 
	AkUniqueID in_uID,					// Unique id of CAkFxShareSet or CAkFxCustom
	bool in_bShareSet,					// Shared?
	SharedSetOverride in_eSharedSetOverride	// For example, Proxy cannot override FX set by API.
	)
{
	if ( in_uID != AK_INVALID_UNIQUE_ID
		&& !m_pMixerPlugin )
	{
		m_pMixerPlugin = AkNew(AkMemID_Structure, MixerPluginChunk());
		if (!m_pMixerPlugin)
			return AK_Fail;
	}

	if( m_pMixerPlugin && !m_pMixerPlugin->AcquireOverride( in_eSharedSetOverride ) )
	{
		// You are not allowed to modify this object Effect list when connecting and live editing, most likely because the SDK API call overrided what you are trying to change. 
		return AK_Success;
	}

	if ( m_pMixerPlugin &&
			( m_pMixerPlugin->plugin.bShareSet != in_bShareSet 
			|| m_pMixerPlugin->plugin.id != in_uID ) )
	{
		m_pMixerPlugin->plugin.bShareSet = in_bShareSet;
		m_pMixerPlugin->plugin.id = in_uID;

		// No live editing of mixing plugin.  
		Stop();
	}

	if ( m_pMixerPlugin 
		&& in_uID == AK_INVALID_UNIQUE_ID )
	{
		AkDelete(AkMemID_Structure, m_pMixerPlugin );
		m_pMixerPlugin = NULL;
	}
	
	return AK_Success;
}

void CAkBus::SetAkProp(
	AkPropID in_eProp, 
	CAkRegisteredObj * in_pGameObj,
	AkValueMeaning in_eValueMeaning,
	AkReal32 in_fTargetValue,
	AkCurveInterpolation in_eFadeCurve,
	AkTimeMs in_lTransitionTime
	)
{
#ifndef AK_OPTIMIZED
	switch ( in_eProp )
	{
	case AkPropID_Pitch:
		{
			MONITOR_SETPARAMNOTIF_FLOAT( AkMonitorData::NotificationReason_PitchChanged, ID(), true, AK_INVALID_GAME_OBJECT , in_fTargetValue, in_eValueMeaning, in_lTransitionTime );

			if(    ( in_eValueMeaning == AkValueMeaning_Offset && in_fTargetValue != 0 )
				|| ( in_eValueMeaning == AkValueMeaning_Independent && ( in_fTargetValue != m_props.GetAkProp( AkPropID_Pitch, 0.0f ).fValue ) ) ) 
			{
				MONITOR_PARAMCHANGED(AkMonitorData::NotificationReason_PitchChanged, ID(), true, AK_INVALID_GAME_OBJECT );
			}
		}
		break;
	case AkPropID_Volume:
		{
			MONITOR_SETPARAMNOTIF_FLOAT( AkMonitorData::NotificationReason_VolumeChanged, ID(), true, AK_INVALID_GAME_OBJECT , in_fTargetValue, in_eValueMeaning, in_lTransitionTime );

			if(    ( in_eValueMeaning == AkValueMeaning_Offset && in_fTargetValue != 0 )
				|| ( in_eValueMeaning == AkValueMeaning_Independent && in_fTargetValue != m_props.GetAkProp( AkPropID_Volume, 0.0f ).fValue ) )
			{
				MONITOR_PARAMCHANGED( AkMonitorData::NotificationReason_VolumeChanged, ID(), true, AK_INVALID_GAME_OBJECT );
			}
		}
		break;
	case AkPropID_LPF:
		{
			MONITOR_SETPARAMNOTIF_FLOAT( AkMonitorData::NotificationReason_LPFChanged, ID(), true, AK_INVALID_GAME_OBJECT , in_fTargetValue, in_eValueMeaning, in_lTransitionTime );

			if(    ( in_eValueMeaning == AkValueMeaning_Offset && in_fTargetValue != 0 )
				|| ( in_eValueMeaning == AkValueMeaning_Independent && ( in_fTargetValue != m_props.GetAkProp( AkPropID_LPF, 0.0f ).fValue ) ) ) 
			{
				MONITOR_PARAMCHANGED( AkMonitorData::NotificationReason_LPFChanged, ID(), true, AK_INVALID_GAME_OBJECT );
			}
		}
		break;
	case AkPropID_HPF:
		{
			MONITOR_SETPARAMNOTIF_FLOAT( AkMonitorData::NotificationReason_HPFChanged, ID(), true, AK_INVALID_GAME_OBJECT , in_fTargetValue, in_eValueMeaning, in_lTransitionTime );

			if(    ( in_eValueMeaning == AkValueMeaning_Offset && in_fTargetValue != 0 )
				|| ( in_eValueMeaning == AkValueMeaning_Independent && ( in_fTargetValue != m_props.GetAkProp( AkPropID_HPF, 0.0f ).fValue ) ) ) 
			{
				MONITOR_PARAMCHANGED( AkMonitorData::NotificationReason_HPFChanged, ID(), true, AK_INVALID_GAME_OBJECT );
			}
		}
		break;
	case AkPropID_BusVolume:
		{
			AkGameObjectID gameObjectID = AK_INVALID_GAME_OBJECT;
			if (in_pGameObj)
			{
				gameObjectID = in_pGameObj->ID();
			}

			MONITOR_SETPARAMNOTIF_FLOAT( AkMonitorData::NotificationReason_VolumeChanged, ID(), true, gameObjectID, in_fTargetValue, in_eValueMeaning, in_lTransitionTime );

			if(    ( in_eValueMeaning == AkValueMeaning_Offset && in_fTargetValue != 0 )
				|| ( in_eValueMeaning == AkValueMeaning_Independent && in_fTargetValue != m_props.GetAkProp( AkPropID_BusVolume, 0.0f ).fValue ) )
			{
				MONITOR_PARAMCHANGED( AkMonitorData::NotificationReason_VolumeChanged, ID(), true, gameObjectID );
			}
		}
		break;
	default:
		AKASSERT( false );
	}
#endif
	CAkRegisteredObj* pGameObj = NULL;
	if (in_eProp == AkPropID_BusVolume)
		pGameObj = in_pGameObj;

	CAkSIS * pSIS = GetSIS(pGameObj);
	if ( pSIS )
		StartSISTransition( pSIS, in_eProp, in_fTargetValue, in_eValueMeaning, in_eFadeCurve, in_lTransitionTime );
}

void CAkBus::Mute(
		CAkRegisteredObj *	in_pGameObj,
		AkCurveInterpolation		in_eFadeCurve /*= AkCurveInterpolation_Linear*/,
		AkTimeMs		in_lTransitionTime /*= 0*/
		)
{
	AKASSERT( g_pRegistryMgr );

	MONITOR_SETPARAMNOTIF_FLOAT( AkMonitorData::NotificationReason_Muted, ID(), true, AK_INVALID_GAME_OBJECT, 0, AkValueMeaning_Default, in_lTransitionTime );
	
	MONITOR_PARAMCHANGED( AkMonitorData::NotificationReason_Muted, ID(), true, AK_INVALID_GAME_OBJECT );

	MONITOR_BUSNOTIFICATION( ID(), AkMonitorData::BusNotification_Muted, 0, 0 );

	if( in_pGameObj == NULL )
	{
		CAkSIS* pSIS = GetSIS();
		if ( pSIS )
			StartSisMuteTransitions( pSIS, AK_MUTED_RATIO, in_eFadeCurve, in_lTransitionTime );
	}
}

void CAkBus::Unmute(CAkRegisteredObj * in_pGameObj, AkCurveInterpolation in_eFadeCurve, AkTimeMs in_lTransitionTime)
{
	MONITOR_BUSNOTIFICATION( ID(), AkMonitorData::BusNotification_Unmuted, 0, 0 );
	
	CAkParameterNodeBase::Unmute(in_pGameObj, in_eFadeCurve, in_lTransitionTime);
}

void CAkBus::UnmuteAll(AkCurveInterpolation in_eFadeCurve,AkTimeMs in_lTransitionTime)
{
	Unmute(NULL,in_eFadeCurve,in_lTransitionTime);
}

CAkSIS* CAkBus::GetSIS( CAkRegisteredObj * in_GameObj )
{
	if (in_GameObj)
	{
		return CAkParameterNodeBase::GetSIS(in_GameObj);
	}
	else
	{
		AKASSERT( g_pRegistryMgr );
		g_pRegistryMgr->SetNodeIDAsModified(this);
		if(!m_pGlobalSIS)
		{
			AkUInt8 bitsBypass = 0;
			for ( AkUInt32 uFXIndex = 0; uFXIndex < AK_NUM_EFFECTS_PER_OBJ; ++uFXIndex )
				bitsBypass |= ( GetBypassFX( uFXIndex, in_GameObj ) ? 1 : 0 ) << uFXIndex;
			m_pGlobalSIS = AkNew(AkMemID_GameObject, CAkSIS(this, bitsBypass));
		}

		return m_pGlobalSIS;
	}
}

void CAkBus::RecalcNotification( bool in_bLiveEdit, bool in_bLog )
{
	if( IsMixingBus() )
	{	
		m_bHdrReleaseTimeDirty = true;

		CAkLEngine::ResetBusVolume( ID() );
	}

	if( IsActivityChunkEnabled() )
	{
		for (ChildrenIterator iter(m_mapBusChildId, m_mapChildId); !iter.End(); ++iter)
		{
			if( (*iter)->IsActiveOrPlaying() )
			{
				(*iter)->RecalcNotification( in_bLiveEdit, in_bLog );
			}
		}
	}
}

void CAkBus::ClearLimiters()
{
	if ( IsActivityChunkEnabled() )
	{
		for ( ChildrenIterator iter( m_mapBusChildId, m_mapChildId ); !iter.End(); ++iter )
		{
			if ( (*iter)->IsActiveOrPlaying() )
			{
				(*iter)->ClearLimiters();
			}
		}
	}
}

#ifndef AK_OPTIMIZED
void CAkBus::UpdateRTPC()
{
	if( IsActivityChunkEnabled() )
	{
		for (ChildrenIterator iter(m_mapBusChildId, m_mapChildId); !iter.End(); ++iter)
		{
			if( (*iter)->IsActiveOrPlaying() )
			{
				(*iter)->UpdateRTPC();
			}
		}
	}
}

void CAkBus::ResetRTPC()
{
	if (IsActivityChunkEnabled())
	{
		for (ChildrenIterator iter(m_mapBusChildId, m_mapChildId); !iter.End(); ++iter)
		{
			if ((*iter)->IsActiveOrPlaying())
			{
				(*iter)->ResetRTPC();
			}
		}
	}
}
#endif

AkUniqueID CAkBus::GetGameParamID()
{
	return (AkUniqueID)m_props.GetAkProp(AkPropID_HDRBusGameParam, (AkInt32)AK_INVALID_UNIQUE_ID).iValue;
}

void CAkBus::ClampWindowTop(AkReal32 &in_fWindowTop)
{
	AkReal32 fMin = m_props.GetAkProp(AkPropID_HDRBusGameParamMin, AK_DEFAULT_HDR_BUS_GAME_PARAM_MIN).fValue;
	AkReal32 fMax = m_props.GetAkProp(AkPropID_HDRBusGameParamMax, AK_DEFAULT_HDR_BUS_GAME_PARAM_MAX).fValue;
	in_fWindowTop = AkClamp(in_fWindowTop, fMin, fMax);
}

#ifndef AK_OPTIMIZED
void CAkBus::MonitoringSolo( bool in_bSolo )
{
	CAkParameterNodeBase::_MonitoringSolo( in_bSolo, g_uSoloCount_bus );
}

void CAkBus::MonitoringMute( bool in_bMute )
{
	CAkParameterNodeBase::_MonitoringMute( in_bMute, g_uMuteCount_bus );
}

#endif

CAkLock CAkBus::m_BackgroundMusicLock;

void CAkBus::MuteBackgroundMusic()
{
	// Only mute on edge of user music "on".
	// s_bIsBackgroundMusicMuted ~= "user music was on".
	if ( ! s_bIsBackgroundMusicMuted )
	{
		s_bIsBackgroundMusicMuted = true;
		
		AkAutoLock<CAkLock> gate( m_BackgroundMusicLock );
		for (AkUInt32 i = 0; i < s_BGMBusses.Length(); i++)
			s_BGMBusses[i]->BackgroundMusic_Mute();
		
		if (g_settings.BGMCallback)
		{
			g_settings.BGMCallback(s_bIsBackgroundMusicMuted, g_settings.BGMCallbackCookie);
		}
	}

}
void CAkBus::UnmuteBackgroundMusic()
{
	// Only unmute on edge of user music "off".
	// s_bIsBackgroundMusicMuted ~= "user music was on".
	if ( s_bIsBackgroundMusicMuted )
	{
		s_bIsBackgroundMusicMuted = false;
		
		AkAutoLock<CAkLock> gate( m_BackgroundMusicLock );
		for (AkUInt32 i = 0; i < s_BGMBusses.Length(); i++)
			s_BGMBusses[i]->BackgroundMusic_Unmute();
		
		if(g_settings.BGMCallback)
		{
			g_settings.BGMCallback(s_bIsBackgroundMusicMuted, g_settings.BGMCallbackCookie);
		}
	}
}

bool CAkBus::IsBackgroundMusicMuted()
{
	return s_bIsBackgroundMusicMuted;	
}

void CAkBus::BackgroundMusic_Mute()
{	
	AkDeltaMonitor::OpenUpdateBrace(AkDelta_BGMMute, key);

	AkMutedMapItem item;
    item.m_bIsPersistent = false;
	item.m_bIsGlobal = true;
	item.m_Identifier = this;
	item.m_eReason = AkDelta_BGMMute;
	MuteNotification( AK_MUTED_RATIO, item, true ); 	

	AkDeltaMonitor::CloseUpdateBrace(key);
}

void CAkBus::BackgroundMusic_Unmute()
{	
	AkMutedMapItem item;
    item.m_bIsPersistent = false;
	item.m_bIsGlobal = true;
	item.m_Identifier = this;
	item.m_eReason = AkDelta_BGMMute;

	AkReal32 fMuteRatio = AK_UNMUTED_RATIO;

	if(m_pGlobalSIS)
	{
		AkSISValue * pValue = m_pGlobalSIS->m_values.FindProp( AkPropID_MuteRatio );
		if( pValue )
			fMuteRatio = pValue->fValue;
	}

	AkDeltaMonitor::OpenUpdateBrace(AkDelta_BGMMute, key);
	MuteNotification( fMuteRatio, item, true );	
	AkDeltaMonitor::CloseUpdateBrace(key);
}

void CAkBus::SetAsBackgroundMusicBus()
{
	AkAutoLock<CAkLock> gate( m_BackgroundMusicLock );

	if( !m_bIsBackgroundMusicBus )
	{
		CAkBus** pItem = s_BGMBusses.AddLast();
		if (pItem == NULL)
			return;	//This is NOT a critical error.  The music won't be muted, but so what?

		*pItem = this;		
		m_bIsBackgroundMusicBus = true;
		if( s_bIsBackgroundMusicMuted )
			BackgroundMusic_Mute(); 
	}	
}

void CAkBus::UnsetAsBackgroundMusicBus()
{
	AkAutoLock<CAkLock> gate( m_BackgroundMusicLock );

	if( m_bIsBackgroundMusicBus )
	{
		// If this assert pops, it means somebody else registered an XMP bus		
		s_BGMBusses.RemoveSwap(this);
		m_bIsBackgroundMusicBus = false;
		BackgroundMusic_Unmute();

		if (s_BGMBusses.Length() == 0)
			s_BGMBusses.Term();
	}
}

void CAkBus::SetBusDevice(AkUniqueID in_idDeviceShareset)
{
	if (m_idDeviceShareset == in_idDeviceShareset)
		return;

	m_idDeviceShareset = in_idDeviceShareset;

	for (AkUInt32 i = 0; i < m_mapBusChildId.Length(); i++)
		((CAkBus*)m_mapBusChildId[i])->SetBusDevice(in_idDeviceShareset);

	if (ParentBus() == NULL)
	{
		//If this is a top bus, then we need to destroy the associated VPL to make sure the right number of channels are used.		
		CAkLEngine::ReevaluateGraph(true);
	}
}

void CAkBus::ReplaceTopBusSharesets(AkUniqueID in_oldSharesetId, AkUniqueID in_newSharesetId)
{
	for (AkBusArray::Iterator it = s_TopBusses.Begin(); it != s_TopBusses.End(); ++it)
	{
		if ((*it)->GetDeviceShareset() == in_oldSharesetId)
		{
			(*it)->SetBusDevice(in_newSharesetId);
		}
	}
}
