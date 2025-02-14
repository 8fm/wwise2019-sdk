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
// AkParameterNodeBase.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#define AK_DEFINE_AKPROPDEFAULT

#include "AkPBI.h"
#include "AkParameterNodeBase.h"
#include "AkTransitionManager.h"
#include "AkURenderer.h"
#include "AkBankMgr.h"
#include "AkAudioMgr.h"
#include "AkLEngine.h"
#include "AkRegistryMgr.h"

// -------------------------------------------------------------
// Globals
// -------------------------------------------------------------
#ifndef AK_OPTIMIZED
AkUInt32 CAkParameterNodeBase::g_uSoloCount = 0;
AkUInt32 CAkParameterNodeBase::g_uMuteCount = 0;
AkUInt32 CAkParameterNodeBase::g_uSoloCount_bus = 0;
AkUInt32 CAkParameterNodeBase::g_uMuteCount_bus = 0;
bool CAkParameterNodeBase::g_bIsMonitoringMuteSoloDirty = false;
#endif

#include <AK/Tools/Common/AkDynaBlkPool.h>
static AkDynaBlkPool<AkActivityChunk,128,ArrayPoolDefault> g_ActivityChunkPool;

const bool g_AkPropDecibel[AkPropID_NUM] =
{
	true, // AkPropID_Volume
	true, // AkPropID_LFE
	false, // AkPropID_Pitch
	false, // AkPropID_LPF
	false, // AkPropID_HPF
	true, // AkPropID_BusVolume
	true, // AkPropID_MakeUpGain
	false, // AkPropID_Priority
	false, // AkPropID_PriorityDistanceOffset
	true, // AkPropID_FeedbackVolume
	false, // AkPropID_FeedbackLPF
	false, // AkPropID_MuteRatio
	false, // AkPropID_PAN_LR
	false, // AkPropID_PAN_FR
	false, // AkPropID_CenterPCT
	false, // AkPropID_DelayTime
	false, // AkPropID_TransitionTime
	false, // AkPropID_Probability
	false, // AkPropID_DialogueMode
	true, // AkPropID_UserAuxSendVolume0
	true, // AkPropID_UserAuxSendVolume1
	true, // AkPropID_UserAuxSendVolume2
	true, // AkPropID_UserAuxSendVolume3
	true, // AkPropID_GameAuxSendVolume
	true, // AkPropID_OutputBusVolume
	false, // AkPropID_OutputBusHPF
	false, // AkPropID_OutputBusLPF
	true, // AkPropID_HDRBusThreshold
	false, // AkPropID_HDRBusRatio
	false, // AkPropID_HDRBusReleaseTime	
	false, // AkPropID_HDRBusGameParam
	false, // AkPropID_HDRBusGameParamMin
	false, // AkPropID_HDRBusGameParamMax
	true, // AkPropID_HDRActiveRange

	false,  // AkPropID_LoopStart
	false,  // AkPropID_LoopEnd
	false,  // AkPropID_TrimInTime
	false,  // AkPropID_TrimOutTime
	false,  // AkPropID_FadeInTime	
	false,  // AkPropID_FadeOutTime
	false,  // AkPropID_FadeInCurve
	false,  // AkPropID_FadeOutCurve
	false,  // AkPropID_LoopCrossfadeDuration
	false,  // AkPropID_CrossfadeUpCurve
	false,  // AkPropID_CrossfadeDownCurve

	false,	//AkPropID_MidiTrackingRootNote,
	false,	//AkPropID_MidiPlayOnNoteType,
	false,	//AkPropID_MidiTransposition,
	false,	//AkPropID_MidiVelocityOffset,
	false,	//AkPropID_MidiKeyRangeMin,
	false,	//AkPropID_MidiKeyRangeMax,
	false,	//AkPropID_MidiVelocityRangeMin,
	false,	//AkPropID_MidiVelocityRangeMax,
	false,	//AkPropID_MidiChannelMask,
	false,	//AkPropID_PlaybackSpeed

	false,	//AkPropID_MidiTempoSource
	false,	//AkPropID_MidiTargetNode

	false,	// AkPropID_AttachedPluginFXID
	false,	// AkPropID_Loop
	false,	// AkPropID_InitialDelay

	false, // AkPropID_UserAuxSendLPF0
	false, // AkPropID_UserAuxSendLPF1
	false, // AkPropID_UserAuxSendLPF2
	false, // AkPropID_UserAuxSendLPF3

	false, // AkPropID_UserAuxSendHPF0
	false, // AkPropID_UserAuxSendHPF1
	false, // AkPropID_UserAuxSendHPF2
	false, // AkPropID_UserAuxSendHPF3

	false, // AkPropID_GameAuxSendLPF
	false, // AkPropID_GameAuxSendHPF

	false, // AkPropID_AttenuationID
	false, // AkPropID_PositioningTypeBlend

	true // AkPropID_ReflectionBusVolume
};



///////////////////////////////////////////////////////////////////
// CAkLimiter
///////////////////////////////////////////////////////////////////
CAkLimiter::CAkLimiter( AkUInt16 in_u16LimiterMax, bool in_bDoesKillNewest, bool in_bAllowUseVirtualBehavior )
						  :m_u16LimiterMax(in_u16LimiterMax)
						  ,m_bDoesKillNewest( in_bDoesKillNewest )
						  ,m_bAllowUseVirtualBehavior( in_bAllowUseVirtualBehavior )
						  ,m_u16Current(0)
						  ,m_u16CurrentVirtual(0)
						  {}

void CAkParamTargetLimiter::Init( CAkParameterNodeBase* in_pNode, CAkRegisteredObj* in_pGameObj, AkUInt16 in_u16LimiterMax, bool in_bDoesKillNewest, bool in_bAllowUseVirtualBehavior )
{
	m_u16LimiterMax = in_u16LimiterMax;
	m_bDoesKillNewest = in_bDoesKillNewest;
	m_bAllowUseVirtualBehavior = in_bAllowUseVirtualBehavior;

	SetRTPCKey(in_pGameObj);

	// This object still gets created for non-limiting nodes.  Make sure not to register it, if we are on a non-limiting node.
	if ( in_u16LimiterMax > 0 && in_pNode != NULL )
	{
		in_pNode->CAkRTPCSubscriberNode::RegisterParameterTarget(this, 1ULL << RTPC_MaxNumInstances);
		SetRootNode(in_pNode);
	}

	AkLimiterKey::LimiterType limiterType;
	if ( in_pNode->IsBusCategory() )
		limiterType = AkLimiterKey::LimiterType_Bus;
	else
		limiterType = AkLimiterKey::LimiterType_Default;

	// Set key composed of the limiter's type, the depth of the object and its shortid
	SetKey( limiterType, in_pNode->GetDepth(), in_pNode->ID() );
}

AKRESULT CAkLimiter::Add( CAkPBI* in_pPBI )
{
	// Must call AddNoSetKey to avoid the key to be changed now.
	// Multiple limitors would all try to update the same key otherwise.
	bool bAddInSystemChecker = IsEmpty();
	CAkPBI** ppPBI = NULL;
	ppPBI = AddNoSetKey(in_pPBI->GetPriorityKey());	
	if( ppPBI )
	{
		if( bAddInSystemChecker )
		{
			CAkURenderer::AddLimiter( this );
		}
		*ppPBI = in_pPBI;
		m_u16Current++;
		return AK_Success;
	}
	return AK_Fail;
}

void CAkLimiter::Remove( CAkPBI* in_pPBI )
{
	bool bFound = Unset(in_pPBI->GetPriorityKey());	
	if( IsEmpty() )
	{
		CAkURenderer::RemoveLimiter( this );
	}
	
	if ( bFound )
	{
		m_u16Current--;
	}
}

void CAkLimiter::Update( AkReal32 in_NewPriority, CAkPBI* in_pPBI )
{
	AkPriorityStruct newKey;
	newKey.SetPriority(in_NewPriority);
	newKey.pipelineID = in_pPBI->GetPriorityKey().pipelineID;
	newKey.seqID = in_pPBI->GetPriorityKey().seqID;
	Reorder(in_pPBI->GetPriorityKey(), newKey, in_pPBI);
}

void CAkLimiter::UpdateFlags()
{
	AkUInt16 u16Max = GetMaxInstances();

	if( u16Max != 0 && Length() > u16Max )
	{
		AkUInt32 uAccepted = 0;
		AkSortedPBIPriorityList::Iterator it = Begin();
		AkSortedPBIPriorityList::Iterator itEnd = End();
		while( it != itEnd && uAccepted < u16Max )
		{
			CAkPBI* pPBI = *it;
			if (!pPBI->IsExemptedFromLimiter() && !pPBI->WasKicked() && !pPBI->IsForcedVirtualized())// don't count if it was already kicked
			{
				if ((pPBI->GetCbx() && pPBI->GetCbx()->IsAudible()) || pPBI->GetUnsafeUnderThresholdBehavior() == AkBelowThresholdBehavior_ContinueToPlay )
				{
					++uAccepted;
				}
			}

			++it;
		}

		// The remaining are not allowed to play.
		while( it != itEnd )
		{
			CAkPBI* pPBI = *it;
			if (!pPBI->WasKicked() && !pPBI->IsExemptedFromLimiter())// No use to virtualize a sound that was kicked: let it die.
			{
				KickFrom eReason = KickFrom_OverNodeLimit;
				if( this == &CAkURenderer::GetGlobalLimiter() )
					eReason = KickFrom_OverGlobalLimit;

				if( m_bAllowUseVirtualBehavior )
				{
					pPBI->ForceVirtualize( eReason );
				}
				else
				{
					pPBI->Kick( eReason );
				}
			}
			++it;
		}
	}
}

void CAkLimiter::SwapOrdering()
{
	m_bDoesKillNewest = !m_bDoesKillNewest;
	ReSortArray();	
}

///////////////////////////////////////////////////////////////////
// CAkParameterNodeBase::FXChunk
///////////////////////////////////////////////////////////////////

CAkParameterNodeBase::FXChunk::FXChunk()
{
	for ( AkUInt32 i = 0; i < AK_NUM_EFFECTS_PER_OBJ; ++i )
	{
		aFX[i].bRendered = false;
		aFX[i].bShareSet = false;
		aFX[i].id = AK_INVALID_UNIQUE_ID;
	}

	bitsMainFXBypass = 0;
}

CAkParameterNodeBase::FXChunk::~FXChunk()
{
}
		
///////////////////////////////////////////////////////////////////
// CAkParameterNodeBase
///////////////////////////////////////////////////////////////////

CAkParameterNodeBase::CAkParameterNodeBase(AkUniqueID in_ulID)
: CAkPBIAware( in_ulID )
, pNextItem(NULL)
, m_pGlobalSIS(NULL)
, m_pMapSIS(NULL)
,m_pFXChunk( NULL )
,m_p3DAutomationParams( NULL )
,m_pActivityChunk( NULL )
,m_pParentNode(NULL)
,m_pBusOutputNode(NULL)
,m_pAuxChunk(NULL)
,m_u16MaxNumInstance( 0 ) //0 means no max
,m_bKillNewest( false )
,m_bUseVirtualBehavior( false )
,m_bIsVVoicesOptOverrideParent( false )
,m_bOverrideAttachmentParams( false )
,m_bIsGlobalLimit( true )
,m_bPriorityApplyDistFactor(false)
,m_bIsBusCategory( false )
,m_bUseGameAuxSends( false )
,m_ePannerType(AK_DirectSpeakerAssignment)
,m_bOverrideMidiEventsBehavior(false)
,m_bOverrideMidiNoteTracking(false)
,m_bEnableMidiNoteTracking(false)
,m_bMidiBreakLoopOnNoteOff(false)
,m_bIsContinuousValidation( false )
, m_bIgnoreParentMaxNumInst(false)
#ifndef AK_OPTIMIZED
,m_bIsSoloed( false )
,m_bIsMuted( false )
#endif
{

}

CAkParameterNodeBase::~CAkParameterNodeBase()
{
	if( m_pMapSIS )
	{
		for( AkMapSIS::Iterator iter = m_pMapSIS->Begin(); iter != m_pMapSIS->End(); ++iter )
		{
			AkDelete(AkMemID_GameObject, (*iter).item);
		}
		m_pMapSIS->Term();
		AkDelete(AkMemID_GameObject, m_pMapSIS);
		m_pMapSIS = NULL; // Just to make sure nobody lower will use it.
	}

	if (m_pAuxChunk)
	{
		AkDelete(AkMemID_Structure, m_pAuxChunk);
	}

	Free3DAutomation();

	if ( m_pFXChunk )
	{
		AkDelete(AkMemID_Structure, m_pFXChunk);
	}

	if(m_pGlobalSIS)
	{
		AkDelete(AkMemID_GameObject,m_pGlobalSIS);
	}

	AKASSERT( !IsActivityChunkEnabled() ); //Should have been freed by now...
	if( IsActivityChunkEnabled() )
	{
		DeleteActivityChunk();// But just in case, we will delete it anyway...
	}

	CAkRTPCSubscriberNode::UnregisterAllParameterTargets();
}

void CAkParameterNodeBase::AddToIndex()
{
	AKASSERT( g_pIndex );
	g_pIndex->GetNodeIndex( IsBusCategory() ? AkNodeType_Bus : AkNodeType_Default ).SetIDToPtr(this);
}

void CAkParameterNodeBase::RemoveFromIndex()
{
	AKASSERT(g_pIndex);
	g_pIndex->GetNodeIndex( IsBusCategory() ? AkNodeType_Bus : AkNodeType_Default ).RemoveID(ID());
}

AkUInt32 CAkParameterNodeBase::AddRef() 
{ 
	AkAutoLock<CAkLock> IndexLock( g_pIndex->GetNodeLock( IsBusCategory() ? AkNodeType_Bus : AkNodeType_Default ) ); 
    return ++m_lRef; 
} 

AkUInt32 CAkParameterNodeBase::Release() 
{ 
    AkAutoLock<CAkLock> IndexLock( g_pIndex->GetNodeLock( IsBusCategory() ? AkNodeType_Bus : AkNodeType_Default ) ); 
    AkInt32 lRef = --m_lRef; 
    AKASSERT( lRef >= 0 ); 
    if ( !lRef ) 
    { 
        RemoveFromIndex();
		OnPreRelease();

		SafeDisconnectActivityChunk();

		if(m_pParentNode != NULL)
		{
			m_pParentNode->RemoveChild(this);
		}

		if(m_pBusOutputNode != NULL)
		{
			m_pBusOutputNode->RemoveChild(this);
		}

		AkDelete(AkMemID_Structure, this);
    } 
    return lRef; 
}

void CAkParameterNodeBase::Parent(CAkParameterNodeBase* in_pParent)
{
	m_pParentNode = in_pParent;
}

void CAkParameterNodeBase::ParentBus(CAkParameterNodeBase* in_pParent)
{
	AdjustCount( in_pParent );
	bool bValid = in_pParent != NULL;
	CAkURenderer::m_bBusInvalid = !bValid;
	m_overriddenParams.Set( RTPC_OutputBusVolume, bValid );
	m_overriddenParams.Set( RTPC_OutputBusLPF, bValid );
	m_overriddenParams.Set( RTPC_OutputBusHPF, bValid );

	if (m_pBusOutputNode != in_pParent)
	{
#ifndef AK_OPTIMIZED
		// Live editing the output bus, must reset RTPC targets, then change parents, then compute them again.
		ResetRTPC();
#endif
		m_pBusOutputNode = in_pParent;
		if (IsActivityChunkEnabled())
		{
			CAkLEngine::ReevaluateGraph(true);
			RecalcNotification(true);
#ifndef AK_OPTIMIZED
			UpdateRTPC();
#endif
		}
	}
}

void CAkParameterNodeBase::AdjustCount( CAkParameterNodeBase*& io_pParent )
{
	if (IsActivityChunkEnabled() && m_pBusOutputNode != io_pParent)
	{
		ClearLimitersAndMakeDirty();
		Counts counts(GetRoutedToBusActivityCount(), GetRoutedToBusPlayCount());
		DecrementAllCountCurrentNodeBus(counts);
		
		//////////////////////////////////////////////////////////////////////////////////////////////////////
		//                                 Important note:
		// Decrement and Increment process are NOT symetrical for this reason:
		// The m_pBusOutputNode was NOT YET updated to the new bus, therefore we cannot rely on it in the Parents iterations.
		//////////////////////////////////////////////////////////////////////////////////////////////////////
		if (io_pParent)
		{
			if ( !IncrementAllCountBus(io_pParent, counts) )
			{
				// TODO: should be only in one branch over there...
				io_pParent = NULL;
			}
		}
		else if (m_pParentNode)
		{
			m_pParentNode->IncrementAllCountParentBus( counts );
		}
	}
}

bool CAkParameterNodeBase::IncrementAllCountBus( CAkParameterNodeBase* in_pBus, Counts counts )
{
	if ( !in_pBus )
		return true;

	for ( AkUInt16 i = 0; i < counts.m_activityCount; i++ )
	{
		if ( !in_pBus->IncrementActivityCount() )
		{
			// If incrementing failed, reverse it
			Counts reverseCounts( i, 0 );
			DecrementAllCountBus( in_pBus, reverseCounts );
			return false;
		}
		
	}
	CounterParameters params;
	for ( AkUInt16 i = 0; i < counts.m_playCount; i++ )
	{
		params.bMaxConsidered = false;
		if ( !in_pBus->IncrementPlayCount( params, true, true ) )
		{
			// If incrementing failed, reverse it
			Counts reverseCounts( counts.m_activityCount, i );
			DecrementAllCountBus( in_pBus, reverseCounts );
			return false;
		}
	}
	return true;
}

bool CAkParameterNodeBase::IncrementAllCountParentBus(Counts counts)
{
	// First increment on This the routed count
	IncrementRoutedToBusActivityCount( counts.m_activityCount );
	IncrementRoutedToBusPlayCount( counts.m_playCount );

	if ( !m_pBusOutputNode )
	{
		if ( m_pParentNode )
			return m_pParentNode->IncrementAllCountParentBus(counts);
		return false;
	}

	return IncrementAllCountBus( m_pBusOutputNode, counts );
}

void CAkParameterNodeBase::DecrementAllCountBus( CAkParameterNodeBase* in_pBus, Counts counts )
{
	if ( !in_pBus )
		return;

	for ( AkUInt16 i = 0; i < counts.m_activityCount; i++ )
	{
		in_pBus->DecrementActivityCount();
	}
	CounterParameters params;
	for ( AkUInt16 i = 0; i < counts.m_playCount; i++ )
	{
		params.bMaxConsidered = false;
		in_pBus->DecrementPlayCount( params );
	}
}

void CAkParameterNodeBase::DecrementAllCountCurrentNodeBus( Counts counts )
{
	if ( !m_pBusOutputNode )
	{
		if (m_pParentNode)
		{
			m_pParentNode->DecrementRoutedToBusActivityCount(counts.m_activityCount);
			m_pParentNode->DecrementRoutedToBusPlayCount(counts.m_playCount);

			m_pParentNode->DecrementAllCountCurrentNodeBus(counts);
		}
		return;
	}

	DecrementAllCountBus( m_pBusOutputNode, counts );
}

void CAkParameterNodeBase::IncrementRoutedToBusPlayCount(AkInt16 in_count)
{
	if(m_pActivityChunk)
		m_pActivityChunk->IncrementPlayCountRouted(in_count);
}

void CAkParameterNodeBase::IncrementRoutedToBusActivityCount(AkInt16 in_count)
{
	if (m_pActivityChunk)
		m_pActivityChunk->IncrementActivityCountRouted(in_count);
}

void CAkParameterNodeBase::DecrementRoutedToBusPlayCount(AkInt16 in_count)
{
	if (m_pActivityChunk)
		m_pActivityChunk->DecrementPlayCountRouted(in_count);
}

void CAkParameterNodeBase::DecrementRoutedToBusActivityCount(AkInt16 in_count)
{
	if (m_pActivityChunk)
		m_pActivityChunk->DecrementActivityCountRouted(in_count);
}

void CAkParameterNodeBase::Unregister(CAkRegisteredObj * in_GameObjPtr)
{
	if (m_pMapSIS)
	{
		AkMapSIS::Iterator iter = m_pMapSIS->Begin();
		while (iter != m_pMapSIS->End())
		{
			if ((*iter).key == in_GameObjPtr)
			{
				if ((*iter).item)
				{
					AkDelete(AkMemID_GameObject, (*iter).item);
				}
				iter = m_pMapSIS->Erase(iter);
			}
			else
			{
				++iter;
			}
		}
	}
}

AKRESULT CAkParameterNodeBase::AddChildInternal(
        CAkParameterNodeBase* /*in_pChild*/
		)
{
	AKASSERT(!"Addchild/removechild not defined for this node type");
	return AK_NotImplemented;
}

void CAkParameterNodeBase::RemoveChild(
        CAkParameterNodeBase* /*in_pChild*/
		)
{
	AKASSERT(!"Addchild/removechild not defined for this node type");
}

void CAkParameterNodeBase::RemoveOutputBus()
{
	if (m_pBusOutputNode)
	{
		m_pBusOutputNode->RemoveChild(this);
	}
}

void CAkParameterNodeBase::GetChildren( AkUInt32&, AkObjectInfo*, AkUInt32&, AkUInt32 )
{
	//no children by default. Function will be overridden in AkParentNode.h
}

void CAkParameterNodeBase::Stop( 
	CAkRegisteredObj * in_pGameObj, 
	AkPlayingID in_PlayingID /* = AK_INVALID_PLAYING_ID */, 
	AkTimeMs in_uTransitionDuration /*= 0*/,
	AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/)
{
	ActionParams l_Params;
	l_Params.bIsFromBus = false;
	l_Params.bIsMasterResume = false;
	l_Params.transParams.eFadeCurve = in_eFadeCurve;
	l_Params.eType = ActionParamType_Stop;
	l_Params.pGameObj = in_pGameObj;
	l_Params.playingID = in_PlayingID;
	l_Params.transParams.TransitionTime = in_uTransitionDuration;
	l_Params.bIsMasterCall = false;
	ExecuteAction( l_Params );
}

void CAkParameterNodeBase::Pause( 
	CAkRegisteredObj * in_pGameObj, 
	AkPlayingID in_PlayingID /* = AK_INVALID_PLAYING_ID */,
	AkTimeMs in_uTransitionDuration /*= 0*/,
	AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/ )
{
	ActionParams l_Params;
	l_Params.bIsFromBus = false;
	l_Params.bIsMasterResume = false;
	l_Params.transParams.eFadeCurve = in_eFadeCurve;
	l_Params.eType = ActionParamType_Pause;
	l_Params.pGameObj = in_pGameObj;
	l_Params.playingID = in_PlayingID;
	l_Params.transParams.TransitionTime = in_uTransitionDuration;
	l_Params.bIsMasterCall = false;
	ExecuteAction( l_Params );
}

void CAkParameterNodeBase::Resume( 
	CAkRegisteredObj * in_pGameObj, 
	AkPlayingID in_PlayingID /* = AK_INVALID_PLAYING_ID */,
	AkTimeMs in_uTransitionDuration /*= 0*/,
	AkCurveInterpolation in_eFadeCurve /*= AkCurveInterpolation_Linear*/ )
{
	ActionParams l_Params;
	l_Params.bIsFromBus = false;
	l_Params.bIsMasterResume = false;
	l_Params.transParams.eFadeCurve = in_eFadeCurve;
	l_Params.eType = ActionParamType_Resume;
	l_Params.pGameObj = in_pGameObj;
	l_Params.playingID = in_PlayingID;
	l_Params.transParams.TransitionTime = in_uTransitionDuration;
	l_Params.bIsMasterCall = false;
	ExecuteAction( l_Params );
}

void CAkParameterNodeBase::Notification(
		AkRTPC_ParameterID in_ParamID, 
		AkReal32 in_fDelta,				// Param variation
		AkReal32 in_fValue,				// Param value
		const AkRTPCKey& in_rtpcKey,
		AkRTPCExceptionChecker* in_pExceptCheck
		)
{
	if( ParamMustNotify( in_ParamID ) )
	{		
		NotifParams l_Params;
		l_Params.eType = in_ParamID;
		l_Params.bIsFromBus = false;
		l_Params.rtpcKey = in_rtpcKey;
		l_Params.fDelta = in_fDelta;
		l_Params.fValue = in_fValue;
		l_Params.pExceptCheck = in_pExceptCheck;
		ParamNotification( l_Params );
	}
}

bool CAkParameterNodeBase::IsException( CAkParameterNodeBase* in_pNode, ExceptionList& in_rExceptionList )
{
	WwiseObjectID wwiseId( in_pNode->ID(), in_pNode->IsBusCategory() );
	return ( in_rExceptionList.Exists( wwiseId ) != NULL );
}

CAkBus* CAkParameterNodeBase::GetMixingBus()
{
	if(m_pBusOutputNode)
	{
		return m_pBusOutputNode->GetMixingBus();
	}
	else if(m_pParentNode)
	{
		return m_pParentNode->GetMixingBus();
	}
	return NULL;
}

CAkBus* CAkParameterNodeBase::GetControlBus()
{
	if( m_pBusOutputNode )
	{
		return (CAkBus*)m_pBusOutputNode;
	}
	else
	{
		if( m_pParentNode )
		{
			return m_pParentNode->GetControlBus();
		}
		else
		{
			return NULL;
		}
	}
}

AKRESULT CAkParameterNodeBase::PrepareNodeData( AkUniqueID in_NodeID )
{
	AKRESULT eResult = AK_Fail;
	CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef( in_NodeID, AkNodeType_Default );

	if( pNode )
	{
		eResult = pNode->PrepareData();
		if(eResult != AK_Success)
		{
			pNode->Release();
		}
		//pNode->Release();// must keep the AudionodeAlive as long as it is prepared, an the node is required to unprepare the data.
	}

	return eResult;
}

void CAkParameterNodeBase::UnPrepareNodeData( AkUniqueID in_NodeID )
{
	CAkParameterNodeBase* pNode = g_pIndex->GetNodePtrAndAddRef( in_NodeID, AkNodeType_Default );

	if( pNode )
	{
		pNode->UnPrepareData();
		pNode->Release();
		pNode->Release();// This is to compensate for the release that have not been done in PrepareData()
	}
}

void CAkParameterNodeBase::TriggerModulators( const AkModulatorTriggerParams& in_params, CAkModulatorData* out_pModPBIData, bool in_bDoBusCheck ) const
{
	{
		AkModulatorSubscriberInfo subsrInfo;
		subsrInfo.pSubscriber = GetSubscriberPtr();
		subsrInfo.pTargetNode = const_cast<CAkParameterNodeBase*>(this);
		subsrInfo.eSubscriberType = CAkRTPCMgr::SubscriberType_CAkParameterNodeBase;
		g_pModulatorMgr->Trigger(subsrInfo, in_params, out_pModPBIData);
	}

	CAkParameterNodeBase* pParentNode = (CAkParameterNode*)( this->Parent() );
	if(in_bDoBusCheck && m_pBusOutputNode)
	{
		if(pParentNode != NULL)
		{
			pParentNode->TriggerModulators(in_params, out_pModPBIData, false);
		}

		m_pBusOutputNode->TriggerModulators(in_params, out_pModPBIData, false);
	}
	else
	{
		if(pParentNode != NULL)
		{
			pParentNode->TriggerModulators(in_params, out_pModPBIData, in_bDoBusCheck);
		}
	}

}

AKRESULT CAkParameterNodeBase::GetModulatorParamXfrms(
	AkModulatorParamXfrmArray&	io_paramsXforms,
	AkRtpcID					in_modulatorID,		
	const CAkRegisteredObj *	in_GameObject,
	bool						in_bDoBusCheck
	) const
{
	GetModulatorXfrmForPropID( io_paramsXforms, AkPropID_Volume, in_modulatorID, in_GameObject );

	GetModulatorXfrmForPropID( io_paramsXforms, AkPropID_MakeUpGain, in_modulatorID, in_GameObject );

	// Call GetModulatorXfrmForPropID() for any additional audio-rate properties here.

	return AK_Success;
}

bool CAkParameterNodeBase::EnableActivityChunk()
{
	if (!IsActivityChunkEnabled())
	{
		m_pActivityChunk = g_ActivityChunkPool.New();
		if (m_pActivityChunk)
		{
			m_pActivityChunk->Init(this, GetMaxNumInstances(), m_bIsGlobalLimit, DoesKillNewest(), m_bUseVirtualBehavior);
			return OnNewActivityChunk();
		}
		else
			return false;
	}

	return true;
}

void CAkParameterNodeBase::DisableActivityChunk()
{
	if (IsActivityChunkEnabled())
	{
		if (GetActivityChunk()->ChunkIsUseless())
		{
			DeleteActivityChunk();
		}
	}
}

void CAkParameterNodeBase::SafeDisconnectActivityChunk()
{
	if (m_pActivityChunk)// Because adding it while allocation failed would result in it never being added
	{
		if (m_pParentNode)
			m_pParentNode->DisableActivityChunk();

		if (m_pBusOutputNode)
			m_pBusOutputNode->DisableActivityChunk();
	}
}

void CAkParameterNodeBase::DeleteActivityChunk()
{
	// Not paranoid code!  Because of interactive music, it may be that this node is still in the URenderer's active node list.
	//   This call will force a removal of the node.
	if (GetActivityChunk()->InURendererList())
		CAkURenderer::RemoveFromActiveNodes(this);

	SafeDisconnectActivityChunk();
	g_ActivityChunkPool.Delete(GetActivityChunk());
	m_pActivityChunk = NULL;
}

bool CAkParameterNodeBase::OnNewActivityChunk()
{
	bool bRet = true;

	if( m_pBusOutputNode ) 
	{
		if (!m_pBusOutputNode->EnableActivityChunk())
		{
			bRet = false;
		}
	}

	if (m_pParentNode && !m_pParentNode->EnableActivityChunk())
		bRet = false;

	return bRet;
}

void CAkParameterNodeBase::AddToURendererActiveNodes()
{
	if (GetActivityChunk() && !GetActivityChunk()->InURendererList())
	{
		CAkURenderer::AddToActiveNodes(this);
		GetActivityChunk()->SetInURendererList(true);
	}
}

void CAkParameterNodeBase::RemoveFromURendererActiveNodes()
{
	if (GetActivityChunk() && GetActivityChunk()->InURendererList())
	{
		if (GetActivityChunk()->GetActivityCount() == 0)
		{
			CAkURenderer::RemoveFromActiveNodes(this);
			GetActivityChunk()->SetInURendererList(false);
		}
	}
}

void CAkParameterNodeBase::StartSISTransition(
		CAkSIS * in_pSIS,
		AkPropID in_ePropID,
		AkReal32 in_fTargetValue,
		AkValueMeaning in_eValueMeaning,
		AkCurveInterpolation in_eFadeCurve,
		AkTimeMs in_lTransitionTime )
{
	AKASSERT( in_eValueMeaning != AkValueMeaning_Default || in_fTargetValue == 0.0f );

	AkSISValue * pSISValue = in_pSIS->GetSISValue( in_ePropID );
	if ( !pSISValue )
		return;

	AkDeltaMonitorObjBrace deltaBrace(key);
	
	// reuse existing transition ?
	if( pSISValue->pTransition != NULL )
	{
		AkReal32 fNewTarget = in_fTargetValue;
		if( in_eValueMeaning == AkValueMeaning_Independent )
		{
			fNewTarget -= m_props.GetAkProp( in_ePropID, g_AkPropDefault[in_ePropID] ).fValue;
		}

		g_pTransitionManager->ChangeParameter(
			pSISValue->pTransition,
			in_ePropID,
			fNewTarget,
			in_lTransitionTime,
			in_eFadeCurve,
			in_eValueMeaning );
	}
	else // new transition
	{
		AkReal32 fStartValue = pSISValue->fValue;
		AkReal32 fTargetValue = 0.0f;
		switch(in_eValueMeaning)
		{
		case AkValueMeaning_Independent:
			fTargetValue = in_fTargetValue - m_props.GetAkProp( in_ePropID, g_AkPropDefault[in_ePropID] ).fValue;
			break;
		case AkValueMeaning_Offset:
			fTargetValue = pSISValue->fValue + in_fTargetValue;
			break;
		case AkValueMeaning_Default:
			break;
		default:
			AKASSERT(!"Invalid Meaning type");
			break;
		}

		// do we really need to start a transition ?
		if((fStartValue == fTargetValue) || (in_lTransitionTime == 0))
		{
			// no need to
			AkReal32 fNotifValue = pSISValue->fValue;
			pSISValue->fValue = fTargetValue;
//			AkDeltaMonitor::OpenUpdateBrace(AkDelta_Transition, key);	//Maybe need a AkDelta_Action?
			m_SISNode.PushParamUpdate_Scoped( g_AkPropRTPCID[ in_ePropID ], in_pSIS->m_pGameObj, pSISValue->fValue, pSISValue->fValue - fNotifValue );
			//AkDeltaMonitor::CloseUpdateBrace(key);
		}
		// start it
		else
		{
			TransitionParameters trans(
				in_pSIS,
				in_ePropID,
				fStartValue,
				fTargetValue,
				in_lTransitionTime,
				in_eFadeCurve,
				AkDelta_Event,				
				g_AkPropDecibel[ in_ePropID ],
				true );

			pSISValue->pTransition = g_pTransitionManager->AddTransitionToList( trans );
		}
	}
}

void CAkParameterNodeBase::StartSisMuteTransitions(CAkSIS*	in_pSIS,
													AkReal32	in_fTargetValue,
													AkCurveInterpolation	in_eFadeCurve,
													AkTimeMs	in_lTransitionTime)
{
	AKASSERT( in_pSIS );

	AkSISValue * pSISValue = in_pSIS->GetSISValue( AkPropID_MuteRatio, AK_UNMUTED_RATIO );
	if ( !pSISValue )
		return;

	// reuse existing transition ?
	if( pSISValue->pTransition != NULL )
	{
		g_pTransitionManager->ChangeParameter(
			pSISValue->pTransition,
			AkPropID_MuteRatio,
			in_fTargetValue,
			in_lTransitionTime,
			in_eFadeCurve,
			AkValueMeaning_Default );
	}
	else // new transition
	{
		if( in_lTransitionTime != 0 )
		{
			TransitionParameters MuteParams(
				in_pSIS,
				AkPropID_MuteRatio,
				pSISValue->fValue,
				in_fTargetValue,
				in_lTransitionTime,
				in_eFadeCurve,
				AkDelta_Mute,				
				false,
				true );
			pSISValue->pTransition = g_pTransitionManager->AddTransitionToList(MuteParams);
		}
		else
		{
			//Apply it directly, so there will be no delay, avoiding an annoying glitch that may apears in the worst cases.
			AkDeltaMonitor::OpenUpdateBrace(AkDelta_Mute, key);
			in_pSIS->TransUpdateValue( AkPropID_MuteRatio, in_fTargetValue, true );
			AkDeltaMonitor::CloseUpdateBrace(key);
		}
	}
}

//Adds one effect. Maintain in sync with RemoveFX and UpdateEffects
AKRESULT CAkParameterNodeBase::SetFX( 
	AkUInt32 in_uFXIndex,				// Position of the FX in the array
	AkUniqueID in_uID,					// Unique id of CAkFxShareSet or CAkFxCustom
	bool in_bShareSet,					// Shared?
	SharedSetOverride in_eSetter
	)
{
	AKASSERT( in_uFXIndex < AK_NUM_EFFECTS_PER_OBJ );
	if( in_uFXIndex >= AK_NUM_EFFECTS_PER_OBJ )
	{
		return AK_InvalidParameter;
	}

	if ( !m_pFXChunk )
	{
		m_pFXChunk = AkNew(AkMemID_Structure, FXChunk());
		if (!m_pFXChunk)
			return AK_InsufficientMemory;
	}

	if( !m_pFXChunk->AcquireOverride( in_eSetter ) )
	{
		// You are not allowed to modify this object Effect list when connecting and live editing, most likely because the SDK API call overrided what you are trying to change. 
		return AK_Success;
	}

	FXStruct & fx = m_pFXChunk->aFX[ in_uFXIndex ];

#ifndef AK_OPTIMIZED
	if ( fx.bRendered ) // Do not set an effect if effect has been rendered.
	{
		return AK_RenderedFX;
	}
#endif

	if ( fx.bShareSet != in_bShareSet 
		|| fx.id != in_uID )
	{
		fx.bShareSet = in_bShareSet;
		fx.id = in_uID;

		//Required only so that the volume gets calculated correctly after/before the BUS FX
		RecalcNotification( false );
		UpdateFx( in_uFXIndex );
	}

	return AK_Success;
}

//Updates all effects in one operation.  Maintain in sync with SetFX and RemoveFX above.
AKRESULT CAkParameterNodeBase::UpdateEffects( AkUInt32 in_uCount, AkEffectUpdate* in_pUpdates, SharedSetOverride in_eSharedSetOverride )
{
	if ( !m_pFXChunk )
	{
		m_pFXChunk = AkNew(AkMemID_Structure, FXChunk());
		if (!m_pFXChunk)
			return AK_InsufficientMemory;
	}

	if( !m_pFXChunk->AcquireOverride( in_eSharedSetOverride ) )
	{
		// You are not allowed to modify this object Effect list when connecting and live editing, most likely because the SDK API call overrided what you are trying to change. 
		return AK_Success;
	}

	bool bRecalc = false;
	bool bUpdates[AK_NUM_EFFECTS_PER_OBJ];
	for(AkUInt32 i = 0; i < AK_NUM_EFFECTS_PER_OBJ; i++)
		bUpdates[i] = false;

	if (in_uCount == 0)
	{
		for(AkUInt32 i = 0; i < AK_NUM_EFFECTS_PER_OBJ; i++)
		{
			bUpdates[i] = m_pFXChunk->aFX[i].id != 0;
			bRecalc |= bUpdates[i];
			m_pFXChunk->aFX[i].id = 0;
		}
	}
	else
	{
		for(AkUInt32 uIndex = 0; uIndex < AK_NUM_EFFECTS_PER_OBJ; uIndex++)
		{
			FXStruct & fx = m_pFXChunk->aFX[uIndex];

			AkUInt32 i = 0;
			for(; i < in_uCount && in_pUpdates[i].uiIndex != uIndex; i++) 
				/*empty*/;

			if (i == in_uCount)
			{
				//Not found, effect deleted or not present.
				if (fx.id != AK_INVALID_UNIQUE_ID)
				{
					fx.bShareSet = false;
					fx.id = AK_INVALID_UNIQUE_ID;
					bUpdates[uIndex] = true;
					bRecalc = true;
				}
				continue;
			}
			
			AkEffectUpdate* pEffect = in_pUpdates + i;

			if ( fx.bShareSet != pEffect->bShared || fx.id != pEffect->ulEffectID )
			{
				fx.bShareSet = pEffect->bShared;
				fx.id = pEffect->ulEffectID;
				bUpdates[uIndex] = true;
				bRecalc = true;
			}
			else if ( pEffect->ulEffectID == AK_INVALID_UNIQUE_ID && (fx.bShareSet || fx.id != AK_INVALID_UNIQUE_ID) )
			{
				fx.bShareSet = false;
				fx.id = AK_INVALID_UNIQUE_ID;
				bUpdates[uIndex] = true;
				bRecalc = true;
			}
		}
	}

	//Required only so that the volume gets calculated correctly after/before the BUS FX
	if (bRecalc)
	{
		RecalcNotification( false );

		for(AkUInt32 i = 0; i < AK_NUM_EFFECTS_PER_OBJ; i++)
		{
			if (bUpdates[i])
				UpdateFx(i);
		}
	}
	return AK_Success;
}

AKRESULT CAkParameterNodeBase::RenderedFX( AkUInt32 in_uFXIndex, bool in_bRendered )
{
	AKASSERT( in_uFXIndex < AK_NUM_EFFECTS_PER_OBJ );

	if ( !m_pFXChunk )
	{
		if ( in_bRendered )
		{
			m_pFXChunk = AkNew(AkMemID_Structure, FXChunk());
			if (!m_pFXChunk)
				return AK_InsufficientMemory;
		}
		else
		{
			return AK_Success;
		}
	}

	m_pFXChunk->aFX[ in_uFXIndex ].bRendered = in_bRendered;

	FXStruct & fx = m_pFXChunk->aFX[ in_uFXIndex ];
	if( in_bRendered && fx.id != AK_INVALID_UNIQUE_ID )
	{
		MONITOR_ERRORMSG( AKTEXT("Warning: Bank contains rendered source effects which can't be edited in Wwise") );

		// Remove it
		fx.bShareSet = false;
		fx.id = AK_INVALID_UNIQUE_ID;

		//Required only so that the volume gets calculated correctly after/before the BUS FX
		RecalcNotification( false );
		UpdateFx( in_uFXIndex );
	}

	return AK_Success;
}

AKRESULT CAkParameterNodeBase::MainBypassFX( 
	AkUInt32 in_bitsFXBypass,
	AkUInt32 in_uTargetMask /* = 0xFFFFFFFF */ )
{
	if( NodeCategory() == AkNodeCategory_Bus || NodeCategory() == AkNodeCategory_AuxBus)
	{
		MONITOR_BUSNOTIFICATION(
			ID(), 
			AkMonitorData::BusNotification_FXBypass,
			in_bitsFXBypass, in_uTargetMask	);
	}

	if ( !m_pFXChunk )
	{
		if ( in_bitsFXBypass )
		{
			m_pFXChunk = AkNew(AkMemID_Structure, FXChunk());
			if (!m_pFXChunk)
				return AK_InsufficientMemory;
		}
		else
		{
			return AK_Success;
		}
	}

	m_pFXChunk->bitsMainFXBypass = (AkUInt8) ( ( m_pFXChunk->bitsMainFXBypass & ~in_uTargetMask ) | (in_bitsFXBypass & in_uTargetMask) );

	//CheckBox prevail over the rest, should not happen when bound on RTPC since the UI will disable it.
	ResetFXBypass( in_bitsFXBypass, in_uTargetMask );
	
	NotifyBypass( in_bitsFXBypass, in_uTargetMask );

	return AK_Success;
}

void CAkParameterNodeBase::BypassFX(
	AkUInt32			in_bitsFXBypass,
	AkUInt32			in_uTargetMask,
	CAkRegisteredObj *	in_pGameObj /* = NULL */,
	bool			in_bIsFromReset /* = false */ )
{
	CAkSIS* pSIS = GetSIS( in_pGameObj );

	if( pSIS )
	{
		AkUInt8 prev = pSIS->m_bitsFXBypass;
		pSIS->m_bitsFXBypass = (AkUInt8) ( ( pSIS->m_bitsFXBypass & ~in_uTargetMask ) | ( in_bitsFXBypass & in_uTargetMask )  );
		if( prev != pSIS->m_bitsFXBypass && !in_bIsFromReset )
		{
			MONITOR_PARAMCHANGED( AkMonitorData::NotificationReason_BypassFXChanged, ID(), IsBusCategory(), in_pGameObj?in_pGameObj->ID():AK_INVALID_GAME_OBJECT );
		}
	}

	if( ( NodeCategory() == AkNodeCategory_Bus || NodeCategory() == AkNodeCategory_AuxBus ) 
		&& in_pGameObj == NULL )
	{
		MONITOR_BUSNOTIFICATION(
			ID(), 
			AkMonitorData::BusNotification_FXBypass,
			in_bitsFXBypass, in_uTargetMask );
	}
	if( in_pGameObj == NULL )
	{
		ResetFXBypass( in_bitsFXBypass, in_uTargetMask );
	}

	// Mask out those that have controllers (RTPC and/or State)
	bool ignored = false;

	CAkParameterNodeBase* pNode = this;
	if ( NodeCategory() != AkNodeCategory_Bus && NodeCategory() != AkNodeCategory_AuxBus )
	{
		while ( pNode->Parent() && (pNode->m_overriddenParams & (RTPC_FX_PARAMS_BITFIELD)) == 0 )
			pNode = pNode->Parent();
	}
	for ( AkUInt32 fx = 0; fx < AK_NUM_EFFECTS_PER_OBJ; ++fx )
	{
		AkRTPC_ParameterID rtpcId = (AkRTPC_ParameterID)((AkUInt32)RTPC_BypassFX0 + fx);
		if ( pNode->HasRtpcOrState( rtpcId ) )
		{
			if ( in_uTargetMask & ( 1 << fx ) )
				ignored = true;
			in_uTargetMask &= ~( 1 << fx );
		}
	}
	{
		AkUInt32 fx = AK_NUM_EFFECTS_PER_OBJ;
		AkRTPC_ParameterID rtpcId = RTPC_BypassAllFX;
		if ( pNode->HasRtpcOrState( rtpcId ) )
		{
			if ( in_uTargetMask & ( 1 << fx ) )
				ignored = true;
			in_uTargetMask &= ~( 1 << fx );
		}
	}

	if ( ignored )
	{
		MONITOR_PARAMCHANGED( AkMonitorData::NotificationReason_BypassFXChangeIgnored, ID(), IsBusCategory(), in_pGameObj?in_pGameObj->ID():AK_INVALID_GAME_OBJECT );
	}

	// Notify playing FXs
	NotifyBypass( in_bitsFXBypass, in_uTargetMask, in_pGameObj );
}

void CAkParameterNodeBase::ResetBypassFX(
		AkUInt32			in_uTargetMask,
		CAkRegisteredObj *	in_pGameObj /*= NULL*/
		)
{
	BypassFX( m_pFXChunk ? m_pFXChunk->bitsMainFXBypass : 0, in_uTargetMask, in_pGameObj, true );
}

void CAkParameterNodeBase::GetPropAndRTPCAndState(AkSoundParams& io_params, const AkRTPCKey& in_rtpcKey)
{
	//Go through base values that are different than the default.
	//This loop assumes the default value is already set in AkSoundParams
	for (AkPropBundle<AkPropValue>::Iterator it = m_props.Begin(); it != m_props.End(); ++it)
	{
		const AkPropID propId = (AkPropID)*it.pID;
		if ( propId < AkPropID_NUM && io_params.Request.IsSet(propId))
			io_params.Accumulate((AkPropID)*it.pID, it.pValue->fValue, AkDelta_BaseValue);		
	}
	
	//Go through RTPC related to this node and modify affected values.	
	AkUInt64 rtpcBits = GetAllRtpcBits();
#if defined _MSC_VER && defined AK_CPU_X86_64
	AkUInt32 id = 0;
	while (rtpcBits)
	{
		AkUInt32 nextBit = 0;
		_BitScanForward64(&nextBit, rtpcBits);
		id += nextBit;
		rtpcBits >>= nextBit + 1;
		const AkPropID propId = g_AkRTPCToPropID[id];
		if (propId < AkPropID_NUM && io_params.Request.IsSet(propId))
			io_params.Accumulate(propId, GetRTPCConvertedValue<AkDeltaMonitor>(id, in_rtpcKey), AkDelta_None); //Logging done in GetRTPCConvertedValue
		id++;
	}

#elif defined __clang__ || defined __GNUG__
	AkUInt32 id = 0;
	while (rtpcBits)
	{
		AkUInt32 nextBit = __builtin_ctzll(rtpcBits);
		id += nextBit;
		rtpcBits >>= nextBit + 1;
		const AkPropID propId = g_AkRTPCToPropID[id];
		if (propId < AkPropID_NUM && io_params.Request.IsSet(propId))
			io_params.Accumulate(propId, GetRTPCConvertedValue(id, in_rtpcKey), AkDelta_None); //Logging done in GetRTPCConvertedValue
		id++;
	}		
#else
	for (AkUInt32 id = RTPC_Volume; rtpcBits != 0; id++, rtpcBits >>= 1)
	{
		while (rtpcBits && !(rtpcBits & 1))
		{
			id++;
			rtpcBits >>= 1;
		}

		const AkPropID propId = g_AkRTPCToPropID[id];
		if (propId < AkPropID_NUM && io_params.Request.IsSet(propId))
			io_params.Accumulate(propId, GetRTPCConvertedValue(id, in_rtpcKey), AkDelta_None); //Logging done in GetRTPCConvertedValue
	}
#endif

	//Go through State information and modify values affected by States.	
	if (UseState())
	{						
		StateGroupChunkList* pStateChunks = GetStateChunks();
		if (pStateChunks)
		{
			for (StateGroupChunkList::Iterator iter = pStateChunks->Begin(); iter != pStateChunks->End(); ++iter)
			{
				AkStateGroupChunk* pStateGroupChunk = *iter;
				for (AkStateValues::Iterator it = pStateGroupChunk->m_values.Begin(); it != pStateGroupChunk->m_values.End(); ++it)
				{
					AkPropID id = g_AkRTPCToPropID[*it.pID];
					if (id < AkPropID_NUM && io_params.Request.IsSet(id))
					{
						AkDeltaMonitor::LogState(pStateGroupChunk->m_ulStateGroup, (AkRTPC_ParameterID)*it.pID, pStateGroupChunk->m_ulActualState, it.pValue->fValue);						
						io_params.Accumulate(id, it.pValue->fValue, AkDelta_None);
					}
				}
			}
		}
	}
	
}

void CAkParameterNodeBase::ApplyAllSIS(const CAkSIS & in_rSIS, AkMutedMap& io_rMutedMap, AkSoundParams &io_Parameters)
{
	for (AkSISValues::Iterator it = in_rSIS.m_values.Begin(); it != in_rSIS.m_values.End(); ++it)
	{
		AkPropID idProp = (AkPropID)*(it.pID);
		if (idProp != AkPropID_MuteRatio)
		{
			if (io_Parameters.Request.IsSet(idProp))
				io_Parameters.Accumulate((AkPropID)*(it.pID), it.pValue->fValue, AkDelta_Event);
		}
		else
		{
			if (it.pValue->fValue != AK_UNMUTED_RATIO)
			{
				AkMutedMapItem item;
				item.m_bIsPersistent = false;
				item.m_bIsGlobal = m_pGlobalSIS == &in_rSIS;
				item.m_Identifier = this;
				item.m_eReason = AkDelta_Mute;
				io_rMutedMap.Set(item, it.pValue->fValue);				
			}
		}
	}
}

void CAkParameterNodeBase::ApplyTransitionsOnProperties(AkSoundParams &io_Parameters, AkMutedMap& io_rMutedMap, const AkRTPCKey& in_rtpcKey)
{	
	if (m_pGlobalSIS)
	{
		ApplyAllSIS(*m_pGlobalSIS, io_rMutedMap, io_Parameters);
	}
	if (m_pMapSIS)
	{
		CAkSIS** l_ppSIS = m_pMapSIS->Exists(in_rtpcKey.GameObj());
		if (l_ppSIS)
		{
			CAkSIS* pSIS = *l_ppSIS;
			ApplyAllSIS(*pSIS, io_rMutedMap, io_Parameters);
		}
	}
}

AkInt16 CAkParameterNodeBase::_GetBypassAllFX(CAkRegisteredObj * in_GameObjPtr)
{
	AkInt16 iBypass = (m_pFXChunk && ((m_pFXChunk->bitsMainFXBypass >> AK_NUM_EFFECTS_BYPASS_ALL_FLAG) & 1)) ? 1 : 0;

	if (HasRtpcOrState(RTPC_BypassAllFX))
	{
		//Getting RTPC for AK_INVALID_GAME_OBJECT since we are in a Bus.
		iBypass = (AkInt16)GetRTPCAndState(RTPC_BypassAllFX, AkRTPCKey(in_GameObjPtr));
	}
	else
	{
		CAkSIS** l_ppSIS = NULL;

		if (m_pMapSIS)
		{
			l_ppSIS = m_pMapSIS->Exists(in_GameObjPtr);
		}

		if (l_ppSIS)
		{
			iBypass = (AkInt16)(((*l_ppSIS)->m_bitsFXBypass >> AK_NUM_EFFECTS_BYPASS_ALL_FLAG) & 1);
		}
		else if (m_pGlobalSIS)
		{
			iBypass = (AkInt16)((m_pGlobalSIS->m_bitsFXBypass >> AK_NUM_EFFECTS_BYPASS_ALL_FLAG) & 1);
		}
	}

	return iBypass;
}

void CAkParameterNodeBase::GetAttachedPropFX( 
	AkFXDesc& out_rFXInfo 
	)
{
	// We must check both Parent() AND ParentBus(), because if we have overriden the parent bus we are not subject to the same Mixer plug-in than our parent (possibly).
	if ( m_bOverrideAttachmentParams || Parent() == NULL || ParentBus() )
	{
		AkUniqueID id = m_props.GetAkProp( AkPropID_AttachedPluginFXID, g_AkPropDefault[AkPropID_AttachedPluginFXID].iValue ).iValue;
		if ( id != AK_INVALID_PLUGINID )
			out_rFXInfo.pFx.Attach( g_pIndex->m_idxFxCustom.GetPtrAndAddRef( id ) );
		else
			out_rFXInfo.pFx = NULL;
		out_rFXInfo.iBypassed = 0;
	}
	else
	{
		Parent()->GetAttachedPropFX( out_rFXInfo );
	}
}

void CAkParameterNodeBase::SetMaxReachedBehavior( bool in_bKillNewest )
{
	if( m_bKillNewest != in_bKillNewest )
	{
		m_bKillNewest = in_bKillNewest;
		if( IsActivityChunkEnabled() )
		{
			GetActivityChunk()->m_Limiter.SwapOrdering();
			for( AkPerObjPlayCount::Iterator iterMax = GetActivityChunk()->m_ListPlayCountPerObj.Begin(); 
				iterMax != GetActivityChunk()->m_ListPlayCountPerObj.End(); 
				++iterMax )
			{
				iterMax.pItem->item.SwapOrdering();
			}
		}
	}
}

void CAkParameterNodeBase::SetOverLimitBehavior( bool in_bUseVirtualBehavior )
{
	if( m_bUseVirtualBehavior != in_bUseVirtualBehavior )
	{
		m_bUseVirtualBehavior = in_bUseVirtualBehavior;
		if( IsActivityChunkEnabled() )
		{
			GetActivityChunk()->m_Limiter.SetUseVirtualBehavior( in_bUseVirtualBehavior );
			for( AkPerObjPlayCount::Iterator iterMax = GetActivityChunk()->m_ListPlayCountPerObj.Begin(); 
				iterMax != GetActivityChunk()->m_ListPlayCountPerObj.End(); 
				++iterMax )
			{
				iterMax.pItem->item.SetUseVirtualBehavior( in_bUseVirtualBehavior );
			}
		}
	}
}

void CAkParameterNodeBase::SetMaxNumInstances( AkUInt16 in_u16MaxNumInstance )
{
	if ( !HasRTPC( RTPC_MaxNumInstances ) )
	{
		ApplyMaxNumInstances( in_u16MaxNumInstance );
		ClearLimitersAndMakeDirty();
	}
	else
		m_u16MaxNumInstance = in_u16MaxNumInstance;
}

void CAkParameterNodeBase::SetIsGlobalLimit( bool in_bIsGlobalLimit )
{
	if ( m_bIsGlobalLimit == in_bIsGlobalLimit )
		return;


	if ( IsActivityChunkEnabled() )
	{
		ClearLimiters();
		m_bIsGlobalLimit = in_bIsGlobalLimit;
		GetActivityChunk()->SetIsGlobalLimit( in_bIsGlobalLimit );
		SetMaxNumInstances(m_u16MaxNumInstance);
		if ( in_bIsGlobalLimit )
		{
			AkPerObjPlayCount& list = GetActivityChunk()->m_ListPlayCountPerObj;
			while ( list.Length() > 0 )
			{
				list[0].item.DisableLimiter();
				list.Unset( list[0].key );
			}
		}
		CAkURenderer::m_bLimitersDirty = true;
	}
	else
	{
		m_bIsGlobalLimit = in_bIsGlobalLimit;
	}
}

CAkParameterNodeBase::AuxChunk::AuxChunk()
{
	for (int i = 0; i < AK_NUM_USER_AUX_SEND_PER_OBJ; ++i)
	{
		aAux[i] = AK_INVALID_UNIQUE_ID;
	}
}

AKRESULT CAkParameterNodeBase::SetAuxBusSend(AkUniqueID in_AuxBusID, AkUInt32 in_ulIndex)
{
	if (!m_pAuxChunk && in_AuxBusID != AK_INVALID_UNIQUE_ID)
	{
		m_pAuxChunk = AkNew(AkMemID_Structure, AuxChunk());
		if (!m_pAuxChunk)
			return AK_InsufficientMemory;
	}

	if (m_pAuxChunk)
	{
		m_pAuxChunk->aAux[in_ulIndex] = in_AuxBusID;
		RecalcNotification(false);
	}

	return AK_Success;
}

void CAkParameterNodeBase::SetOverrideReflectionsAuxBus(bool in_bOverride)
{
	m_overriddenParams.Mask(RTPC_SPATIAL_AUDIO_BITFIELD, in_bOverride);
	RecalcNotification(false);
}

void CAkParameterNodeBase::SetOverrideGameAuxSends(bool in_bOverride)
{
	m_overriddenParams.Mask( RTPC_GAME_AUX_SEND_PARAPM_BITFIELD, in_bOverride );
	RecalcNotification(false);
}

void CAkParameterNodeBase::SetUseGameAuxSends(bool in_bUse)
{
	m_bUseGameAuxSends = in_bUse;
	RecalcNotification(false);
}
void CAkParameterNodeBase::SetOverrideUserAuxSends(bool in_bOverride)
{
	m_overriddenParams.Mask( RTPC_USER_AUX_SEND_PARAM_BITFIELD, in_bOverride );
	RecalcNotification(false);
}

void CAkParameterNodeBase::SetEnableDiffraction(bool in_bEnable)
{
	if (m_posSettings.m_bEnableDiffraction != in_bEnable)
	{
		m_posSettings.m_bEnableDiffraction = in_bEnable;
#ifndef AK_OPTIMIZED
		if (IsActivityChunkEnabled())
		{
			InvalidatePositioning();
		}
#endif
	}
}

void CAkParameterNodeBase::SetReflectionsAuxBus(AkUniqueID in_AuxBusID)
{
	m_reflectionsAuxBus = in_AuxBusID;
	RecalcNotification(false);
}

void CAkParameterNodeBase::SetOverrideAttachmentParams( bool in_bOverride )
{
	m_bOverrideAttachmentParams = in_bOverride;
}

void CAkParameterNodeBase::SetVVoicesOptOverrideParent( bool in_bOverride )
{
	m_bIsVVoicesOptOverrideParent = in_bOverride;
}

void CAkParameterNodeBase::SetOverrideMidiEventsBehavior( bool in_bOverride )
{
	m_bOverrideMidiEventsBehavior = in_bOverride;
}

void CAkParameterNodeBase::SetOverrideMidiNoteTracking( bool in_bOverride )
{
	m_bOverrideMidiNoteTracking = in_bOverride;
}

void CAkParameterNodeBase::SetEnableMidiNoteTracking( bool in_bEnable )
{
	m_bEnableMidiNoteTracking = in_bEnable;
}

void CAkParameterNodeBase::SetIsMidiBreakLoopOnNoteOff( bool in_bBreak )
{
	m_bMidiBreakLoopOnNoteOff = in_bBreak;
}

PriorityInfo CAkParameterNodeBase::GetPriority( CAkRegisteredObj * in_GameObjPtr )
{
	if( Parent() && !PriorityOverrideParent())
		return Parent()->GetPriority( in_GameObjPtr );
	else
	{
		PriorityInfo prInfo;
		AkReal32 tempPriority = 0.f;
		GetPropAndRTPCAndState(tempPriority, AkPropID_Priority, AkRTPCKey(in_GameObjPtr));
		prInfo.SetPriority(tempPriority);
		prInfo.SetDistanceOffset( m_bPriorityApplyDistFactor ? m_props.GetAkProp( AkPropID_PriorityDistanceOffset, g_AkPropDefault[AkPropID_PriorityDistanceOffset] ).fValue : 0.0f );
		return prInfo;
	}
}

void CAkParameterNodeBase::SetAkProp( AkPropID in_eProp, AkReal32 in_fValue, AkReal32 /*in_fMin*/, AkReal32 /*in_fMax*/ )
{
	AkDeltaMonitorObjBrace braceObj(ID());
	if ((in_eProp >= AkPropID_Volume && in_eProp <= AkPropID_MakeUpGain)
		|| (in_eProp >= AkPropID_UserAuxSendVolume0 && in_eProp <= AkPropID_OutputBusLPF)
		|| (in_eProp >= AkPropID_UserAuxSendLPF0 && in_eProp <= AkPropID_GameAuxSendHPF)
		|| (in_eProp == AkPropID_ReflectionBusVolume)
		)
	{
		AkReal32 fDelta = in_fValue - m_props.GetAkProp(in_eProp, 0.0f).fValue;
		if (fDelta != 0.0f)
		{	
			AkRTPC_ParameterID rtpcID = g_AkPropRTPCID[in_eProp];
			if (Parent() == NULL || rtpcID < RTPC_OVERRIDABLE_PARAMS_START || (rtpcID >= RTPC_OVERRIDABLE_PARAMS_START && rtpcID != RTPC_MaxNumRTPC && m_overriddenParams.IsSet(rtpcID)))
				AkDeltaMonitor::LogAkProp(in_eProp, in_fValue, fDelta);

			Notification(rtpcID, fDelta, in_fValue);
			m_props.SetAkProp(in_eProp, in_fValue);
		}
	}
	else if (in_eProp >= AkPropID_PAN_LR && in_eProp <= AkPropID_PAN_FR)
	{
		//PAN_X and PAN_Y need to notify as delta.
		AkReal32 fDelta = in_fValue - m_props.GetAkProp(in_eProp, 0.0f).fValue;
		if (fDelta != 0.0f)
		{
			PositioningChangeNotification(fDelta, g_AkPropRTPCID[in_eProp], NULL);
			m_props.SetAkProp(in_eProp, in_fValue);
		}
	}
	else if (in_eProp == AkPropID_CenterPCT)
	{
		AkReal32 fDelta = in_fValue - m_props.GetAkProp(in_eProp, 0.0f).fValue;
		if (fDelta != 0.0f)
		{
			PositioningChangeNotification(in_fValue, g_AkPropRTPCID[in_eProp], NULL);
			m_props.SetAkProp(in_eProp, in_fValue);
		}
	}
	else
	{
		AkReal32 fProp = m_props.GetAkProp(in_eProp, g_AkPropDefault[in_eProp]).fValue;
		if (fProp != in_fValue)
		{
			m_props.SetAkProp(in_eProp, in_fValue);
			RecalcNotification(false);
		}
	}
}

void CAkParameterNodeBase::SetAkProp( AkPropID in_eProp, AkInt32 in_iValue, AkInt32 /*in_iMin*/, AkInt32 /*in_iMax*/ )
{
	AkInt32 iProp = m_props.GetAkProp( in_eProp, g_AkPropDefault[in_eProp]).iValue;
	if( iProp != in_iValue )
	{
		m_props.SetAkProp( in_eProp, in_iValue );
		RecalcNotification( false );
	}
}

AkPropValue * CAkParameterNodeBase::FindCustomProp( AkUInt32 in_uPropID )
{
	return m_props.FindProp( (AkPropID) ( in_uPropID + AkPropID_NUM ) );
}

void CAkParameterNodeBase::SetPriorityApplyDistFactor( bool in_bApplyDistFactor )
{
	if( m_bPriorityApplyDistFactor != in_bApplyDistFactor )
	{
		m_bPriorityApplyDistFactor = in_bApplyDistFactor;
		RecalcNotification( false );
	}
}

void CAkParameterNodeBase::SetPriorityOverrideParent( bool in_bOverrideParent )
{
	if( PriorityOverrideParent() != in_bOverrideParent )
	{
		m_overriddenParams.Set( RTPC_Priority, in_bOverrideParent );
		RecalcNotification( false );
#ifndef AK_OPTIMIZED
		ResetRTPC();
		UpdateRTPC();
#endif
	}
}

void CAkParameterNodeBase::SetPositioningInfoOverrideParent(bool in_bSet)
{
#ifndef AK_OPTIMIZED
	ResetRTPC();
#endif
	m_overriddenParams.Mask(RTPC_POSITIONING_BITFIELD, in_bSet);
#ifndef AK_OPTIMIZED
	UpdateRTPC();
#endif
}

#ifndef AK_OPTIMIZED
void CAkParameterNodeBase::UpdateRTPC()
{
	if (GetActivityChunk())
	{
		for (AkActivityChunk::AkListLightCtxs::Iterator iterMax = GetActivityChunk()->m_listPBI.Begin();
			iterMax != GetActivityChunk()->m_listPBI.End();
			++iterMax)
		{
			CAkBehavioralCtx* l_PBI = *iterMax;
			l_PBI->UpdateRTPC();
		}
	}
}

void CAkParameterNodeBase::ResetRTPC()
{
	if (GetActivityChunk())
	{
		for (AkActivityChunk::AkListLightCtxs::Iterator iterMax = GetActivityChunk()->m_listPBI.Begin();
			iterMax != GetActivityChunk()->m_listPBI.End();
			++iterMax)
		{
			CAkBehavioralCtx* l_PBI = *iterMax;
			l_PBI->ResetRTPC();
		}
	}
}
#endif

void CAkParameterNodeBase::ClearLimitersAndMakeDirty()
{
	if ( IsActivityChunkEnabled() )
	{
		ClearLimiters();
		CAkURenderer::m_bLimitersDirty = true;
	}
}

void CAkParameterNodeBase::PosSetPanningType(AkSpeakerPanningType in_ePannerType)
{
	m_ePannerType = in_ePannerType;

	if (IsActivityChunkEnabled())
	{
		PositioningChangeNotification((AkReal32)in_ePannerType, (AkRTPC_ParameterID)POSID_PanningType, NULL);

		// - a bus may become mixing or non-mixing, so we need to fix links in the graph.
		// - passing bDestroyAll = 'true' is necessary because 'false' only works right now for changes wrt listeners and devices.
		CAkLEngine::ReevaluateGraph(true);
	}
}

void CAkParameterNodeBase::SetListenerRelativeRouting(bool in_bHasListenerRelativeRouting)
{
	m_bHasListenerRelativeRouting = in_bHasListenerRelativeRouting;
	
	if (IsActivityChunkEnabled())
	{
		CAkParameterNodeBase* pNode = this;
		while (!(pNode->PositioningInfoOverrideParent() || pNode->Parent() == NULL))
			pNode = pNode->Parent();

		PropagatePositioningNotification(pNode->HasListenerRelativeRouting(), (AkRTPC_ParameterID)POSID_ListenerRelativeRouting, NULL);
		CAkLEngine::ReevaluateGraph();
	}
}

bool CAkParameterNodeBase::HasListenerRelativeRouting() const
{
	//NOTE: It is probably only appropriate to call this directly for AkBus's.  AkParameterNode's must respect the positioning override flag.
	return m_bHasListenerRelativeRouting;
}

AKRESULT CAkParameterNodeBase::SetPositioningParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize)
{
	AKRESULT eResult = AK_Success;

	AkUInt8 uBitsPositioning = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
	
	bool bPositioningInfoOverrideParent = GETBANKDATABIT( uBitsPositioning, BANK_BITPOS_POSITIONING_OVERRIDE_PARENT );
	SetPositioningInfoOverrideParent(bPositioningInfoOverrideParent);
	bool bHasListenerRelativeRouting = GETBANKDATABIT(uBitsPositioning, BANK_BITPOS_POSITIONING_LISTENER_RELATIVE_ROUTING);
	m_bHasListenerRelativeRouting = bHasListenerRelativeRouting;

	if( bPositioningInfoOverrideParent )
	{
		m_ePannerType = ((uBitsPositioning >> BANK_BITPOS_POSITIONING_PANNER) & AK_PANNER_NUM_STORAGE_BITS);
		m_posSettings.m_e3DPositionType = ((uBitsPositioning >> BANK_BITPOS_POSITIONING_3D_POS_SOURCE) & AK_POSSOURCE_NUM_STORAGE_BITS);

		if (bHasListenerRelativeRouting)
		{
			AkUInt8 uBits3d = READBANKDATA(AkUInt8, io_rpData, io_rulDataSize);

			m_posSettings.m_eSpatializationMode = ((uBits3d >> BANK_BITPOS_POSITIONING_3D_SPATIALIZATION_MODE) & AK_SPAT_NUM_STORAGE_BITS);
			m_posSettings.m_bEnableAttenuation = GETBANKDATABIT(uBits3d, BANK_BITPOS_POSITIONING_3D_ENABLE_ATTENUATION);
			m_posSettings.m_bHoldEmitterPosAndOrient = GETBANKDATABIT(uBits3d, BANK_BITPOS_POSITIONING_3D_HOLD_EMITTER);
			m_posSettings.m_bHoldListenerOrient = GETBANKDATABIT(uBits3d, BANK_BITPOS_POSITIONING_3D_HOLD_LISTENER_ORIENT);
			m_posSettings.m_bEnableDiffraction = GETBANKDATABIT(uBits3d, BANK_BITPOS_POSITIONING_3D_ENABLE_DIFFRACTION);

			if (m_posSettings.HasAutomation())
			{
				//3D
				AKASSERT( m_p3DAutomationParams == NULL );
				eResult = Ensure3DAutomationAllocated();
				if(eResult == AK_Success)
				{
					Ak3DAutomationParams & automation = m_p3DAutomationParams->GetParams();
					automation.m_ePathMode = (AkPathMode)READBANKDATA(AkUInt8, io_rpData, io_rulDataSize);
					automation.m_bIsLooping = GETBANKDATABIT(uBits3d, BANK_BITPOS_POSITIONING_3D_LOOPING);
					automation.m_TransitionTime = READBANKDATA(AkTimeMs, io_rpData, io_rulDataSize);
					
					// Paths
					AkPathVertex * pVertices = NULL;

					AkUInt32 ulNumVertices = READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
					if ( ulNumVertices )
					{
						pVertices = (AkPathVertex *) io_rpData;
						SKIPBANKBYTES( sizeof(AkPathVertex) * ulNumVertices, io_rpData, io_rulDataSize );
					}

					AkUInt32 ulNumPlayListItem = READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );
					if ( ulNumPlayListItem )
					{
						AkPathListItemOffset * pPlayListItems = (AkPathListItemOffset*) io_rpData;
						SKIPBANKBYTES( sizeof( AkPathListItemOffset ) * ulNumPlayListItem, io_rpData, io_rulDataSize );

						if ( ulNumVertices ) 
							eResult = PosSetPath( pVertices, ulNumVertices, pPlayListItems, ulNumPlayListItem );
					}

					for(AkUInt32 iPath = 0; iPath < ulNumPlayListItem; iPath++)
					{
						AkReal32 fRangeX = READBANKDATA( AkReal32, io_rpData, io_rulDataSize );
						AkReal32 fRangeY = READBANKDATA( AkReal32, io_rpData, io_rulDataSize );
						AkReal32 fRangeZ = READBANKDATA( AkReal32, io_rpData, io_rulDataSize );
						PosSetPathRange(iPath, fRangeX, fRangeY, fRangeZ);
					}
				}
			}
		}
	}

	return eResult;
}

AKRESULT CAkParameterNodeBase::SetAuxParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize)
{
	AkUInt8 byBitVector = READBANKDATA(AkUInt8, io_rpData, io_rulDataSize);
	bool bOverrideGameAuxSends = GETBANKDATABIT(byBitVector, BANK_BITPOS_AUXINFO_OVERRIDE_GAME_AUX);
	SetOverrideGameAuxSends(bOverrideGameAuxSends);
	m_bUseGameAuxSends = GETBANKDATABIT(byBitVector, BANK_BITPOS_AUXINFO_USE_GAME_AUX);
	bool bOverrideUserAuxSends = GETBANKDATABIT(byBitVector, BANK_BITPOS_AUXINFO_OVERRIDE_USER_AUX);
	SetOverrideUserAuxSends(bOverrideUserAuxSends);
	bool bHasAux = GETBANKDATABIT(byBitVector, BANK_BITPOS_AUXINFO_HAS_AUX) != 0;

	bool bOverrideEarlyReflections = GETBANKDATABIT(byBitVector, BANK_BITPOS_AUXINFO_OVERRIDE_REFLECTIONS);
	SetOverrideReflectionsAuxBus(bOverrideEarlyReflections);

	AKRESULT eResult = AK_Success;
	for (int i = 0; i < AK_NUM_USER_AUX_SEND_PER_OBJ; ++i)
	{
		AkUniqueID auxID = AK_INVALID_UNIQUE_ID;
		if (bHasAux)
		{
			auxID = READBANKDATA(AkUniqueID, io_rpData, io_rulDataSize);
		}
		eResult = SetAuxBusSend(auxID, i);
		if (eResult != AK_Success)
			break;
	}

	m_reflectionsAuxBus = READBANKDATA(AkUniqueID, io_rpData, io_rulDataSize);

	return eResult;
}

AKRESULT CAkParameterNodeBase::SetAdvSettingsParams( AkUInt8*& /*io_rpData*/, AkUInt32& /*io_rulDataSize*/ )
{
	AKASSERT(!"Dummy  virtual function");
	return AK_NotImplemented;
}

AKRESULT CAkParameterNodeBase::SetNodeBaseParams(AkUInt8*& io_rpData, AkUInt32& io_rulDataSize, bool in_bPartialLoadOnly )
{
	AKRESULT eResult = AK_Success;

	//Read FX
	eResult = SetInitialFxParams(io_rpData, io_rulDataSize, in_bPartialLoadOnly);
	if( eResult != AK_Success || in_bPartialLoadOnly )
	{
		return eResult;
	}

	// Read attachable property override.
	m_bOverrideAttachmentParams = READBANKDATA(AkUInt8, io_rpData, io_rulDataSize);

	AkUniqueID OverrideBusId = READBANKDATA(AkUInt32, io_rpData, io_rulDataSize);

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

		if( eResult != AK_Success )
		{	
			return eResult;
		}
	}

	AkUniqueID DirectParentID = READBANKDATA( AkUInt32, io_rpData, io_rulDataSize );

	if( DirectParentID != AK_INVALID_UNIQUE_ID )
	{
		CAkParameterNodeBase* pParent = g_pIndex->GetNodePtrAndAddRef( DirectParentID, AkNodeType_Default );
		if( pParent )
		{
			eResult = pParent->AddChildByPtr( this );
			pParent->Release();
			if( eResult != AK_Success )
			{	
				return eResult;
			}
		}
	}

	// Get override/enable booleans
	{
		AkUInt8 byBitVector = READBANKDATA( AkUInt8, io_rpData, io_rulDataSize );
		SetPriorityOverrideParent( GETBANKDATABIT( byBitVector, BANK_BITPOS_PARAMNODEBASE_PRIORITY_OVERRIDE_PARENT ) != 0 );
		SetPriorityApplyDistFactor( GETBANKDATABIT( byBitVector, BANK_BITPOS_PARAMNODEBASE_PRIORITY_APPLY_DIST_FACTOR ) != 0 );
		SetOverrideMidiEventsBehavior( GETBANKDATABIT( byBitVector, BANK_BITPOS_PARAMNODEBASE_OVERRIDE_MIDI_EVENTS_BEHAVIOR ) != 0 );
		SetOverrideMidiNoteTracking( GETBANKDATABIT( byBitVector, BANK_BITPOS_PARAMNODEBASE_OVERRIDE_MIDI_NOTE_TRACKING ) != 0 );
		SetEnableMidiNoteTracking( GETBANKDATABIT( byBitVector, BANK_BITPOS_PARAMNODEBASE_ENABLE_MIDI_NOTE_TRACKING ) != 0 );
		SetIsMidiBreakLoopOnNoteOff( GETBANKDATABIT( byBitVector, BANK_BITPOS_PARAMNODEBASE_MIDI_BREAK_LOOP_NOTE_OFF ) != 0 );
	}

	if(eResult == AK_Success)
	{
		eResult = SetInitialParams( io_rpData, io_rulDataSize );
	}

	if(eResult == AK_Success)
	{
		eResult = SetPositioningParams( io_rpData, io_rulDataSize );
	}

	if(eResult == AK_Success)
	{
		eResult = SetAuxParams( io_rpData, io_rulDataSize );
	}

	if(eResult == AK_Success)
	{
		eResult = SetAdvSettingsParams( io_rpData, io_rulDataSize );
	}

	if(eResult == AK_Success)
	{
		eResult = ReadStateChunk(io_rpData, io_rulDataSize);
	}

	if(eResult == AK_Success)
	{
		eResult = SetInitialRTPC(io_rpData, io_rulDataSize, this);
	}

	return eResult;
}

CAkSIS* CAkParameterNodeBase::GetSIS( CAkRegisteredObj * in_GameObjPtr )
{
	AKASSERT(g_pRegistryMgr);
	CAkSIS* l_pSIS = NULL;
	if (in_GameObjPtr != NULL)
	{
		if (!m_pMapSIS)
		{
			m_pMapSIS = AkNew(AkMemID_GameObject, AkMapSIS());
			if (!m_pMapSIS)
				return NULL;
		}

		CAkSIS** l_ppSIS = m_pMapSIS->Exists(in_GameObjPtr);
		if (l_ppSIS)
		{
			l_pSIS = *l_ppSIS;
		}
		else
		{
			AkUInt8 bitsFXBypass = m_pFXChunk ? m_pFXChunk->bitsMainFXBypass : 0;

			l_pSIS = AkNew(AkMemID_GameObject, CAkSIS(this, bitsFXBypass, in_GameObjPtr));
			if (l_pSIS)
			{
				if (!m_pMapSIS->Set(in_GameObjPtr, l_pSIS))
				{
					AkDelete(AkMemID_GameObject, l_pSIS);
					l_pSIS = NULL;
				}
				else
				{
					CAkModifiedNodes* pModNodes = in_GameObjPtr->CreateComponent<CAkModifiedNodes>();
					if (!pModNodes || pModNodes->SetNodeAsModified(this) != AK_Success)
					{
						m_pMapSIS->Unset(in_GameObjPtr);
						AkDelete(AkMemID_GameObject, l_pSIS);
						l_pSIS = NULL;
					}
				}
			}
		}
	}
	else
	{
		g_pRegistryMgr->SetNodeIDAsModified(this);
		if(!m_pGlobalSIS)
		{
			AkUInt8 bitsFXBypass = m_pFXChunk ? m_pFXChunk->bitsMainFXBypass : 0;

			m_pGlobalSIS = AkNew(AkMemID_GameObject, CAkSIS(this, bitsFXBypass));
		}
		l_pSIS = m_pGlobalSIS;
	}
	return l_pSIS;
}

AkInt16 CAkParameterNodeBase::GetBypassFX(
	AkUInt32	in_uFXIndex,
	CAkRegisteredObj * in_GameObjPtr)
{
	if (!m_pFXChunk)
		return false;

	AkInt16 iBypass;

	AkRTPC_ParameterID rtpcId = (AkRTPC_ParameterID)(RTPC_BypassFX0 + in_uFXIndex);

	if (m_pFXChunk->aFX[in_uFXIndex].id != AK_INVALID_UNIQUE_ID && HasRtpcOrState(rtpcId))
	{
		//Getting RTPC for AK_INVALID_GAME_OBJECT since we are in a Bus.
		iBypass = (AkInt16)GetRTPCAndState( rtpcId, AkRTPCKey(in_GameObjPtr) );
	}
	else
	{
		CAkSIS** l_ppSIS = NULL;

		if (m_pMapSIS)
		{
			l_ppSIS = m_pMapSIS->Exists(in_GameObjPtr);
		}

		if (l_ppSIS)
		{
			iBypass = (AkInt16)(((*l_ppSIS)->m_bitsFXBypass >> in_uFXIndex) & 1);
		}
		else if (m_pGlobalSIS)
		{
			iBypass = (AkInt16)((m_pGlobalSIS->m_bitsFXBypass >> in_uFXIndex) & 1);
		}
		else
		{
			iBypass = (AkInt16)((m_pFXChunk->bitsMainFXBypass >> in_uFXIndex) & 1);
		}
	}

	return iBypass;
}

void CAkParameterNodeBase::ResetFXBypass(
	AkUInt32		in_bitsFXBypass,
	AkUInt32        in_uTargetMask /* = 0xFFFFFFFF */)
{
	if (m_pGlobalSIS)
	{
		m_pGlobalSIS->m_bitsFXBypass = (AkUInt8)((m_pGlobalSIS->m_bitsFXBypass & ~in_uTargetMask) | (in_bitsFXBypass & in_uTargetMask));
	}

	if (m_pMapSIS)
	{
		for (AkMapSIS::Iterator iter = m_pMapSIS->Begin(); iter != m_pMapSIS->End(); ++iter)
		{
			(*iter).item->m_bitsFXBypass = (AkUInt8)(((*iter).item->m_bitsFXBypass & ~in_uTargetMask) | (in_bitsFXBypass & in_uTargetMask));
		}
	}
}

void CAkParameterNodeBase::ResetAkProp(
	AkPropID in_eProp,
	AkCurveInterpolation in_eFadeCurve,
	AkTimeMs in_lTransitionTime
)
{
	if (m_pMapSIS)
	{
		for (AkMapSIS::Iterator iter = m_pMapSIS->Begin(); iter != m_pMapSIS->End(); ++iter)
		{
			AkSISValue * pValue = (*iter).item->m_values.FindProp(in_eProp);
			if (pValue && (pValue->fValue != 0.0f || pValue->pTransition != NULL))
			{
				SetAkProp(in_eProp, (*iter).item->m_pGameObj, AkValueMeaning_Default);
			}
		}
	}

	if (m_pGlobalSIS)
	{
		AkSISValue * pValue = m_pGlobalSIS->m_values.FindProp(in_eProp);
		if (pValue && (pValue->fValue != 0.0f || pValue->pTransition != NULL))
		{
			SetAkProp(in_eProp, NULL, AkValueMeaning_Default, 0, in_eFadeCurve, in_lTransitionTime);
		}
	}
}

void CAkParameterNodeBase::Unmute(CAkRegisteredObj * in_GameObjPtr, AkCurveInterpolation in_eFadeCurve, AkTimeMs in_lTransitionTime)
{
	AKASSERT(g_pRegistryMgr);

	MONITOR_SETPARAMNOTIF_FLOAT(AkMonitorData::NotificationReason_Unmuted, ID(), false, in_GameObjPtr ? in_GameObjPtr->ID() : AK_INVALID_GAME_OBJECT, 0, AkValueMeaning_Default, in_lTransitionTime);

	CAkSIS* pSIS = NULL;
	if (in_GameObjPtr != NULL)
	{
		if (m_pMapSIS)
		{
			CAkSIS** l_ppSIS = m_pMapSIS->Exists(in_GameObjPtr);
			if (l_ppSIS)
			{
				pSIS = *l_ppSIS;
			}
		}
	}
	else
	{
		if (m_pGlobalSIS)
		{
			AkSISValue * pValue = m_pGlobalSIS->m_values.FindProp(AkPropID_MuteRatio);
			if (pValue && (pValue->fValue != AK_UNMUTED_RATIO || pValue->pTransition != NULL))
			{
				g_pRegistryMgr->SetNodeIDAsModified(this);
				pSIS = m_pGlobalSIS;
			}
		}
	}
	if (pSIS)
	{
		StartSisMuteTransitions(pSIS, AK_UNMUTED_RATIO, in_eFadeCurve, in_lTransitionTime);
	}
}

AKRESULT CAkParameterNodeBase::SetRTPC(
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

	return CAkRTPCSubscriberNode::SetRTPC(in_RTPC_ID, in_RTPCType, in_RTPCAccum, in_ParamID, in_RTPCCurveID, in_eScaling, in_pArrayConversion, in_ulConversionArraySize);

}

void CAkParameterNodeBase::UnsetRTPC( AkRTPC_ParameterID in_ParamID, AkUniqueID in_RTPCCurveID )
{
	AKASSERT( (unsigned int)in_ParamID < sizeof(AkUInt64)*8 );

	CAkRTPCSubscriberNode::UnsetRTPC( in_ParamID, in_RTPCCurveID );

	AkDeltaMonitor::OpenUnsetBrace(AkDelta_RTPC, ID(), (AkPropID)g_AkRTPCToPropID[in_ParamID]);
	RecalcNotification( true, true ); // liveEdit, log
	AkDeltaMonitor::CloseUnsetBrace(ID());

	// Notify music if necessary.
	MusicNotification( 0.f, in_ParamID, NULL );

	// Notify Positioning if required.
	{
		AkPropID id;

		switch( in_ParamID )
		{
		case RTPC_Position_PAN_X_2D:
			id = AkPropID_PAN_LR;
			break;
		case RTPC_Position_PAN_Y_2D:
			id = AkPropID_PAN_FR;
			break;
		case RTPC_Position_PAN_X_3D:
		case RTPC_Position_PAN_Y_3D:
		case RTPC_Position_PAN_Z_3D:
			PositioningChangeNotification( 0.0f, in_ParamID, NULL ); // There is no actual property for 3d panning
			return;
		default:
			return;
		}

		AkReal32 fValue = m_props.GetAkProp( id, g_AkPropDefault[id] ).fValue;

		PositioningChangeNotification( fValue, in_ParamID, NULL );
	}

}


void CAkParameterNodeBase::RegisterParameterTarget(CAkParameterTarget*	in_pTarget, const AkRTPCBitArray& in_paramsRequested, bool in_bPropagateToBusHier )
{
	//params requested by target to receive notifications for.
	AkRTPCBitArray excludedParams;	//params that have already been registered by child nodes, or otherwise excluded.

	CAkBus* pBus = NULL;
	bool bPastMixingBus = false;//Do not subscribe to bus volume notifications above the first mixing bus
								//	this is because control bus volumes are collapsed down to the mix bus nodes / source nodes.
	{
		CAkParameterNodeBase* pNode = this;
		if (IsBusCategory())
		{
			pBus = static_cast<CAkBus*>(this);
			pNode = NULL;
		}

		// Actor-Mixer Hierarchy
		while (pNode != NULL && (in_paramsRequested & (~excludedParams | RTPC_AM_NODE_ADDITIVE_PARAMS_BITFIELD)) != 0)
		{
			AkRTPCBitArray overriddenParams = pNode->m_overriddenParams;
			
			if (pNode->Parent() == NULL)
				overriddenParams |= (RTPC_COMMON_OVERRIDABLE_PARAMS_BF | RTPC_AM_OVERRIDE_WITH_NO_PARENT_AM_NODE_BF);

			AkRTPCBitArray paramsToReg = in_paramsRequested & (overriddenParams | RTPC_AM_NODE_ADDITIVE_PARAMS_BITFIELD) & ~excludedParams;

			pNode->CAkRTPCSubscriberNode::RegisterParameterTarget(in_pTarget, paramsToReg);
			pNode->CAkParamNodeStateAware::RegisterParameterTarget(in_pTarget, paramsToReg);
			pNode->m_SISNode.RegisterParameterTarget(in_pTarget, paramsToReg);

			excludedParams |= overriddenParams;

			if (in_bPropagateToBusHier && 
				pBus == NULL && pNode->m_pBusOutputNode != NULL)
			{
				pBus = static_cast<CAkBus*>(pNode->m_pBusOutputNode);
				if (pBus->IsMixingBus())
				{
					excludedParams |= RTPC_BUS_NODE_COLLAPSABLE_PARAMS_BITFIELD;
					bPastMixingBus = true;
				}
			}

			pNode = pNode->m_pParentNode;
		}
	}

	if(pBus != NULL)
	{
		// Bus Hierarchy
		while ((in_paramsRequested & (~excludedParams | RTPC_BUS_NODE_ADDITIVE_PARAMS_BITFIELD)) != 0)
		{
			if (!bPastMixingBus)
				excludedParams &= ~RTPC_BUS_NODE_COLLAPSABLE_PARAMS_BITFIELD;

			AkRTPCBitArray overriddenParams = (pBus->ParentBus() == NULL ? RTPC_COMMON_OVERRIDABLE_PARAMS_BF : pBus->m_overriddenParams);
			AkRTPCBitArray paramsToReg = in_paramsRequested & (overriddenParams | RTPC_BUS_NODE_ADDITIVE_PARAMS_BITFIELD | RTPC_BUS_CHAINED_PARAMS_BF ) & ~excludedParams;

			pBus->CAkRTPCSubscriberNode::RegisterParameterTarget(in_pTarget, paramsToReg);
			pBus->CAkParamNodeStateAware::RegisterParameterTarget(in_pTarget, paramsToReg);
			pBus->GetDuckerNode().RegisterParameterTarget(in_pTarget, paramsToReg);
			pBus->m_SISNode.RegisterParameterTarget(in_pTarget, paramsToReg);

			excludedParams |= overriddenParams;

			pBus = static_cast<CAkBus*>(pBus->m_pBusOutputNode);

			if (pBus)
			{
				if (!bPastMixingBus && pBus->IsMixingBus())
				{
					excludedParams |= RTPC_BUS_NODE_COLLAPSABLE_PARAMS_BITFIELD;
					bPastMixingBus = true;
				}
			}
			else
				break;
		}
	}
}


void CAkParameterNodeBase::UnregisterParameterTarget(CAkParameterTarget* in_pTarget, const AkRTPCBitArray& in_paramsRequested, bool in_bPropagateToBusHier)
{	
	AkRTPCBitArray excludedParams;	//overridden params that have already been un-registered by child nodes, or otherwise excluded.

	CAkParameterNodeBase* pNode = this;
	CAkBus* pBus = NULL;

	if (IsBusCategory())
	{
		pBus = static_cast<CAkBus*>(this);
		pNode = NULL;
	}

	// Actor-Mixer Hierarchy
	while (pNode != NULL && (in_paramsRequested & (~excludedParams | RTPC_AM_NODE_ADDITIVE_PARAMS_BITFIELD)) != 0)
	{
		pNode->CAkRTPCSubscriberNode::UnregisterParameterTarget(in_pTarget);
		pNode->CAkParamNodeStateAware::UnregisterParameterTarget(in_pTarget);
		pNode->m_SISNode.UnregisterParameterTarget(in_pTarget);

		excludedParams |= pNode->m_overriddenParams;

		if (in_bPropagateToBusHier && pBus == NULL)
			pBus = static_cast<CAkBus*>(pNode->m_pBusOutputNode);

		pNode = pNode->m_pParentNode;
	}

	// Bus Hierarchy
	while (pBus != NULL && (in_paramsRequested & (~excludedParams | RTPC_BUS_NODE_ADDITIVE_PARAMS_BITFIELD)) != 0)
	{
		pBus->CAkRTPCSubscriberNode::UnregisterParameterTarget(in_pTarget);
		pBus->CAkParamNodeStateAware::UnregisterParameterTarget(in_pTarget);
		pBus->GetDuckerNode().UnregisterParameterTarget(in_pTarget);
		pBus->m_SISNode.UnregisterParameterTarget(in_pTarget);
			
		excludedParams |= pBus->m_overriddenParams;
			
		pBus = static_cast<CAkBus*>(pBus->m_pBusOutputNode);
	}
}

bool CAkParameterNodeBase::GetStateSyncTypes( AkStateGroupID in_stateGroupID, CAkStateSyncArray* io_pSyncTypes, bool in_bBusChecked /*=false*/ )
{
	if( CheckSyncTypes( in_stateGroupID, io_pSyncTypes ) )
		return true;

	if( !in_bBusChecked && ParentBus() )
	{
		in_bBusChecked = true;
		if( static_cast<CAkBus*>( ParentBus() )->GetStateSyncTypes( in_stateGroupID, io_pSyncTypes ) )
		{
			return true;
		}
	}
	if( Parent() )
	{
		return Parent()->GetStateSyncTypes( in_stateGroupID, io_pSyncTypes, in_bBusChecked );
	}
	return false;
}


bool CAkParameterNodeBase::IncrementActivityCount( AkUInt16 in_flagForwardToBus /*= AK_ForwardToBusType_ALL*/ )
{
	bool bIsSuccessful = IncrementActivityCountValue( in_flagForwardToBus & AK_ForwardToBusType_Normal );
	// We must continue and go up to the top even if bIsSuccessful failed.
	// Decrement will soon be called and will go to the top too, so it must be done all the way up even in case of failure.

	if( in_flagForwardToBus & AK_ForwardToBusType_Normal )
	{
		if( m_pBusOutputNode )
		{
			in_flagForwardToBus &= ~AK_ForwardToBusType_Normal;
			bIsSuccessful &= m_pBusOutputNode->IncrementActivityCount();
		}
	}

	if( m_pParentNode )
	{
		bIsSuccessful &= m_pParentNode->IncrementActivityCount( in_flagForwardToBus );
	}

	return bIsSuccessful;
}

void CAkParameterNodeBase::DecrementActivityCount( AkUInt16 in_flagForwardToBus /*= AK_ForwardToBusType_ALL*/ )
{
	DecrementActivityCountValue( in_flagForwardToBus & AK_ForwardToBusType_Normal );

	if( in_flagForwardToBus & AK_ForwardToBusType_Normal )
	{
		if( m_pBusOutputNode )
		{
			in_flagForwardToBus &= ~AK_ForwardToBusType_Normal;
			m_pBusOutputNode->DecrementActivityCount();
		}
	}

	if( m_pParentNode )
	{
		m_pParentNode->DecrementActivityCount( in_flagForwardToBus );
	}
}

AkUInt16 CAkParameterNodeBase::GetMaxNumInstances( CAkRegisteredObj * in_GameObjPtr )
{ 
	AkUInt16 u16Max = m_u16MaxNumInstance;

	if( HasRTPC( RTPC_MaxNumInstances ) 
		&& u16Max != 0 // if u16Max == 0, the RTPC must be ignored and there is no limit.
		)
	{
		// Replace with the real value based on RTPCs.
		u16Max = (AkUInt16)GetRTPCConvertedValue( RTPC_MaxNumInstances, AkRTPCKey( in_GameObjPtr ) );
	}

	return u16Max;
}

AKRESULT CAkParameterNodeBase::ProcessGlobalLimiter( CounterParameters& io_params, bool in_bNewInstance )
{
	AkUInt16 u16Max = GetMaxNumInstances();
	if ( !IsActivityChunkEnabled() )
		EnableActivityChunk();

	if ( u16Max )
	{
		if (io_params.pLimiters && GetActivityChunk())
		{
			io_params.pLimiters->AddLast(&GetActivityChunk()->m_Limiter);
		}

		if ( in_bNewInstance )
			return KickIfOverGlobalLimit( io_params, u16Max );
	}
	return AK_Success;
}

AKRESULT CAkParameterNodeBase::KickIfOverGlobalLimit( CounterParameters& io_params, AkUInt16 in_u16Max )
{
	AKRESULT eResult = AK_Success;

	if ( io_params.bAllowKick && io_params.ui16NumKicked == 0 && (GetPlayCountValid() - GetVirtualCountValid() >= in_u16Max) )
	{
		CAkParameterNodeBase* pKicked = NULL;
		eResult = CAkURenderer::Kick( &GetActivityChunk()->m_Limiter, in_u16Max, io_params.fPriority, NULL, m_bKillNewest, m_bUseVirtualBehavior, pKicked, KickFrom_OverNodeLimit );
		++(io_params.ui16NumKicked);
	}

	return eResult;
}

void CAkParameterNodeBase::CheckAndDeleteActivityChunk()
{
	if( IsActivityChunkEnabled() )
	{
		if( GetActivityChunk()->ChunkIsUseless() )
		{
			DeleteActivityChunk();
		}
	}
}

AKRESULT CAkParameterNodeBase::ProcessGameObjectLimiter( CounterParameters& io_params, bool in_bNewInstance )
{
	AKRESULT eResult = AK_Success;

	AkUInt16 u16Max = GetMaxNumInstances( io_params.pGameObj );

	if (GetActivityChunk())
	{
		StructMaxInst* pPerObjCount = GetActivityChunk()->m_ListPlayCountPerObj.Exists(io_params.pGameObj);
		if (pPerObjCount && in_bNewInstance)
		{
			u16Max = pPerObjCount->GetMax();
			eResult = KickIfOverLimit(pPerObjCount, io_params, u16Max);
		}
		else if (!pPerObjCount)
		{
			u16Max = GetMaxNumInstances(io_params.pGameObj);
			eResult = CreateGameObjectLimiter(io_params.pGameObj, pPerObjCount, u16Max);
		}

		if (pPerObjCount && u16Max && io_params.pLimiters)
		{
			io_params.pLimiters->AddLast(pPerObjCount->m_pLimiter);
		}
	}

	return eResult;
}

AKRESULT CAkParameterNodeBase::KickIfOverLimit( StructMaxInst*& in_pPerObjCount, CounterParameters& io_params, AkUInt16& in_u16Max )
{
	AKASSERT( NodeCategory() != AkNodeCategory_Bus );// fix if someday bus supports game object limiters.
	AKRESULT eResult = AK_Success;
	if ( io_params.bAllowKick && in_pPerObjCount->IsMaxNumInstancesActivated() && ((in_pPerObjCount->GetCurrent() - in_pPerObjCount->GetCurrentVirtual()) - io_params.ui16NumKicked) >= in_u16Max )
	{
		CAkParameterNodeBase* pKicked = NULL;
		eResult = CAkURenderer::Kick( in_pPerObjCount->m_pLimiter, in_u16Max, io_params.fPriority, io_params.pGameObj, m_bKillNewest, m_bUseVirtualBehavior, pKicked, KickFrom_OverNodeLimit );
		++(io_params.ui16NumKicked);
	}

	return eResult;
}

AKRESULT CAkParameterNodeBase::CreateGameObjectLimiter( CAkRegisteredObj* in_pGameObj, StructMaxInst*& out_pPerObjCount, AkUInt16 in_u16Max )
{
	AKASSERT(in_pGameObj);
		
	AKRESULT eResult = AK_Success;

	StructMaxInst structMaxInst;
	
	if ( structMaxInst.Init(this, in_pGameObj, in_u16Max, DoesKillNewest(), m_bUseVirtualBehavior) )
	{
		out_pPerObjCount = GetActivityChunk()->m_ListPlayCountPerObj.Set( 
			in_pGameObj,
			structMaxInst 
			);

		if( !out_pPerObjCount )
		{
			structMaxInst.DisableLimiter();
			//Don't allow this sound to play in low memory situations if you cannot limit it.
			eResult = AK_Fail;
		}
	}
	else
		eResult = AK_Fail;

	return eResult;
}

void CAkParameterNodeBase::CheckAndDeleteGameObjLimiter( CAkRegisteredObj * in_pGameObj )
{
	StructMaxInst* pPerObjCount = GetActivityChunk()->m_ListPlayCountPerObj.Exists( in_pGameObj );
	if( pPerObjCount )
	{
		if( pPerObjCount->GetCurrent() == 0 && pPerObjCount->GetCurrentVirtual() == 0 )
		{
			pPerObjCount->DisableLimiter();
			GetActivityChunk()->m_ListPlayCountPerObj.Unset( in_pGameObj );
		}

		CheckAndDeleteActivityChunk();
	}
}

AKRESULT CAkParameterNodeBase::IncrementPlayCountValue(bool in_bIsRoutedToBus)
{
	bool bRet = EnableActivityChunk();
	if( IsActivityChunkEnabled() )
	{
		GetActivityChunk()->IncrementPlayCount(in_bIsRoutedToBus);
	}
	return bRet ? AK_Success : AK_Fail;
}

void CAkParameterNodeBase::DecrementPlayCountValue(bool in_bIsRoutedToBus)
{
	if( IsActivityChunkEnabled() )
	{
		AKASSERT( GetActivityChunk()->GetPlayCount() > 0 );
		GetActivityChunk()->DecrementPlayCount(in_bIsRoutedToBus);
		if( GetActivityChunk()->ChunkIsUseless() )
		{
			DeleteActivityChunk();
		}
	}
}

bool CAkParameterNodeBase::IncrementActivityCountValue(bool in_bIsRoutedToBus)
{
	bool ret = EnableActivityChunk();
	if( IsActivityChunkEnabled() )
	{
		GetActivityChunk()->IncrementActivityCount(in_bIsRoutedToBus);
	}
	return ret;
}

void CAkParameterNodeBase::DecrementActivityCountValue(bool in_bIsRoutedToBus)
{
	if( IsActivityChunkEnabled() )
	{
		GetActivityChunk()->DecrementActivityCount(in_bIsRoutedToBus);

		if( GetActivityChunk()->ChunkIsUseless() )
		{
			DeleteActivityChunk();
		}
	}
}

bool CAkParameterNodeBase::IsMidiNoteTracking( AkInt32& out_iRootNote ) const
{
	CAkParameterNodeBase* pParent = Parent();
	if( m_bOverrideMidiNoteTracking || ! pParent )
	{
		AkPropID propID = AkPropID_MidiTrackingRootNote;
		out_iRootNote = m_props.GetAkProp( propID, g_AkPropDefault[propID] ).iValue;
		return m_bEnableMidiNoteTracking;
	}
	return pParent->IsMidiNoteTracking( out_iRootNote );
}

bool CAkParameterNodeBase::IsMidiBreakLoopOnNoteOff() const
{
	CAkParameterNodeBase* pParent = Parent();
	if( m_bOverrideMidiEventsBehavior || ! pParent )
	{
		return m_bMidiBreakLoopOnNoteOff;
	}
	return pParent->IsMidiBreakLoopOnNoteOff();
}

bool CAkParameterNodeBase::IsInfiniteLooping( CAkPBIAware* in_pInstigator )
{
	if ( this == in_pInstigator )
		return false;

	if ( m_pParentNode )
		return m_pParentNode->IsInfiniteLooping( in_pInstigator );
	else
		return false;
}

void CAkParameterNodeBase::Get2DParams( const AkRTPCKey& in_rtpcKey, BaseGenParams* out_pBasePosParams)
{
	AKASSERT( out_pBasePosParams );

	out_pBasePosParams->m_fPAN_X_2D = m_props.GetAkProp(AkPropID_PAN_LR, 0.0f).fValue;
	out_pBasePosParams->m_fPAN_Y_2D = m_props.GetAkProp(AkPropID_PAN_FR, 0.0f).fValue;

	if (HasRTPC(RTPC_Position_PAN_X_2D))
	{
		//TODO use state getter too here
		out_pBasePosParams->m_fPAN_X_2D += GetRTPCConvertedValue( RTPC_Position_PAN_X_2D, in_rtpcKey );
	}

	if (HasRTPC(RTPC_Position_PAN_Y_2D))
	{
		//TODO use state getter too here
		out_pBasePosParams->m_fPAN_Y_2D += GetRTPCConvertedValue( RTPC_Position_PAN_Y_2D, in_rtpcKey );
	}

	if ( HasRTPC( RTPC_Positioning_Divergence_Center_PCT ) )
		out_pBasePosParams->m_fCenterPCT = GetRTPCConvertedValue( RTPC_Positioning_Divergence_Center_PCT, in_rtpcKey );
	else
		out_pBasePosParams->m_fCenterPCT = m_props.GetAkProp( AkPropID_CenterPCT, 0.0f ).fValue;

	out_pBasePosParams->ePannerType = m_ePannerType;
	out_pBasePosParams->bHasListenerRelativeRouting = m_bHasListenerRelativeRouting;
}

// Review: Inefficient. Should work like other RTPCs
void CAkParameterNodeBase::Get3DPanning( const AkRTPCKey& in_rtpcKey, AkVector & out_posPan )
{
	AKASSERT(m_p3DAutomationParams != NULL);

	bool bRTPC_Position_PAN_X = HasRTPC( RTPC_Position_PAN_X_3D );
	bool bRTPC_Position_PAN_Y = HasRTPC( RTPC_Position_PAN_Y_3D );
	bool bRTPC_Position_PAN_Z = HasRTPC( RTPC_Position_PAN_Z_3D );

	AkReal32 fMaxRadius = 0.0f;
	if (bRTPC_Position_PAN_X | bRTPC_Position_PAN_Y | bRTPC_Position_PAN_Z)
	{
		out_posPan.X = AK_DEFAULT_SOUND_POSITION_X;
		out_posPan.Z = AK_DEFAULT_SOUND_POSITION_Z;
		out_posPan.Y = AK_DEFAULT_SOUND_POSITION_Y;

		if( bRTPC_Position_PAN_X )
		{
			//TODO change function to use a return type, no io required here.
			GetRTPCAndState(out_posPan.X, RTPC_Position_PAN_X_3D, in_rtpcKey);
			if( GetMaxRadius( fMaxRadius ) )
				out_posPan.X *= fMaxRadius / 100.0f;
		}

		if( bRTPC_Position_PAN_Y )
		{
			GetRTPCAndState(out_posPan.Z, RTPC_Position_PAN_Y_3D, in_rtpcKey);
			if( fMaxRadius != 0.0f || GetMaxRadius( fMaxRadius ) )
				out_posPan.Z *= fMaxRadius / 100.0f;
		}

		if ( bRTPC_Position_PAN_Z )
		{
			GetRTPCAndState(out_posPan.Y, RTPC_Position_PAN_Z_3D, in_rtpcKey);
			if (fMaxRadius != 0.0f || GetMaxRadius(fMaxRadius))
				out_posPan.Y *= fMaxRadius / 100.0f;
		}
	}
}


//////////////////////////////////////////////////////////////////////////////
//Positioning information setting
//////////////////////////////////////////////////////////////////////////////

AKRESULT CAkParameterNodeBase::Ensure3DAutomationAllocated()
{
	AKRESULT eResult = AK_Success;
	if (!m_p3DAutomationParams)
	{
		SetPositioningInfoOverrideParent(true);
		CAk3DAutomationParamsEx * p3DParams = AkNew(AkMemID_Structure, CAk3DAutomationParamsEx());
		if (!p3DParams)
		{
			eResult = AK_InsufficientMemory;
		}
		else
		{
			p3DParams->SetPathOwner(ID());
			m_p3DAutomationParams = p3DParams;
		}
	}
	return eResult;
}

void CAkParameterNodeBase::Free3DAutomation()
{
	if (m_p3DAutomationParams)
	{
#ifndef AK_OPTIMIZED
		InvalidateAllPaths();
#endif
		FreePathInfo();
		AkDelete(AkMemID_Structure, m_p3DAutomationParams);
		m_p3DAutomationParams = NULL;
	}
}

#ifndef AK_OPTIMIZED
// WAL entry point for positioning type.
void CAkParameterNodeBase::PosSetPositioningOverride(bool in_bOverride)
{
	bool bIsPositioningDirty = false;
	if (PositioningInfoOverrideParent() != in_bOverride)
	{
		SetPositioningInfoOverrideParent(in_bOverride);
		if (!in_bOverride)
			Free3DAutomation();
		bIsPositioningDirty = true;
	}

	if (in_bOverride)
	{
		// The authoring tool cannot consistently keep track of RTPC curves in order to push 3D values
		// only when needed. If this function is called, we can assume that they _might be needed_.
		// So unless this node is overriden, it HAS to be ready to accept 3D data.
		bIsPositioningDirty = true;
	}

	// IMPORTANT: Do not notify if property is RTPC'd! If sound is playing, notification will be issued
	// from RTPC Mgr; if not playing, it will be fetched on startup.
	if (IsActivityChunkEnabled() && bIsPositioningDirty)
	{
#ifndef AK_OPTIMIZED
		InvalidatePositioning();
#endif
	}
}

void CAkParameterNodeBase::PosSetSpatializationMode(Ak3DSpatializationMode in_eMode)
{
	if (m_posSettings.m_eSpatializationMode != in_eMode)
	{
		m_posSettings.m_eSpatializationMode = in_eMode;
		if (IsActivityChunkEnabled())
		{
			InvalidatePositioning();
		}
	}
}

void CAkParameterNodeBase::PosSet3DPositionType(Ak3DPositionType in_e3DPositionType)
{
	if (m_posSettings.m_e3DPositionType != in_e3DPositionType)
	{
		m_posSettings.m_e3DPositionType = in_e3DPositionType;
		if (m_posSettings.HasAutomation())
			Ensure3DAutomationAllocated();

		if (IsActivityChunkEnabled())
		{
			InvalidatePositioning();
		}
	}
}

void CAkParameterNodeBase::PosSetAttenuationID(AkUniqueID in_AttenuationID)
{
	if ((AkUniqueID)m_props.GetAkProp(AkPropID_AttenuationID, (AkInt32)AK_INVALID_UNIQUE_ID).iValue != in_AttenuationID)
	{
		m_props.SetAkProp(AkPropID_AttenuationID, (AkInt32)in_AttenuationID);

		InvalidatePositioning();
		PositioningChangeNotification((AkReal32)in_AttenuationID, (AkRTPC_ParameterID)POSID_Attenuation, NULL);
	}
}

void CAkParameterNodeBase::PosEnableAttenuation(bool in_bEnableAttenuation)
{
	if (m_posSettings.m_bEnableAttenuation != in_bEnableAttenuation)
	{
		m_posSettings.m_bEnableAttenuation = in_bEnableAttenuation;
		
		if (IsActivityChunkEnabled())
		{
			InvalidatePositioning();
		}
	}
}
#endif

void CAkParameterNodeBase::PosSetHoldEmitterPosAndOrient(bool in_bHoldEmitterPosAndOrient)
{
	m_posSettings.m_bHoldEmitterPosAndOrient = in_bHoldEmitterPosAndOrient;
	PositioningChangeNotification((AkReal32)in_bHoldEmitterPosAndOrient, (AkRTPC_ParameterID)POSID_IsPositionDynamic, NULL);
}

void CAkParameterNodeBase::PosSetHoldListenerOrient(bool in_bHoldListenerOrient)
{
	m_posSettings.m_bHoldListenerOrient = in_bHoldListenerOrient;
}

AKRESULT CAkParameterNodeBase::PosSetPathMode(AkPathMode in_ePathMode)
{
	AKRESULT eResult = AK_Fail;
	Ensure3DAutomationAllocated();
	if (m_p3DAutomationParams)
	{
		// get rid of the path played flags if any
		FreePathInfo();

		m_p3DAutomationParams->SetPathMode(in_ePathMode);
		PositioningChangeNotification((AkReal32)in_ePathMode, (AkRTPC_ParameterID)POSID_PathMode, NULL);
		eResult = AK_Success;
	}
	return eResult;
}

AKRESULT CAkParameterNodeBase::PosSetIsLooping(bool in_bIsLooping)
{
	AKRESULT eResult = AK_Fail;
	Ensure3DAutomationAllocated();
	if (m_p3DAutomationParams)
	{
		m_p3DAutomationParams->SetIsLooping(in_bIsLooping);
		PositioningChangeNotification((AkReal32)in_bIsLooping, (AkRTPC_ParameterID)POSID_IsLooping, NULL);
		eResult = AK_Success;
	}
	return eResult;
}

AKRESULT CAkParameterNodeBase::PosSetTransition(AkTimeMs in_TransitionTime)
{
	AKRESULT eResult = AK_Fail;
	Ensure3DAutomationAllocated();
	if (m_p3DAutomationParams)
	{
		if (m_p3DAutomationParams->GetTransitiontime() != in_TransitionTime)
		{
			m_p3DAutomationParams->SetTransition(in_TransitionTime);
			PositioningChangeNotification((AkReal32)in_TransitionTime, (AkRTPC_ParameterID)POSID_Transition, NULL);
		}
		eResult = AK_Success;
	}
	return eResult;
}

AKRESULT CAkParameterNodeBase::PosSetPath(
	AkPathVertex*           in_pArrayVertex,
	AkUInt32                 in_ulNumVertices,
	AkPathListItemOffset*   in_pArrayPlaylist,
	AkUInt32                 in_ulNumPlaylistItem
	)
{
	AKRESULT eResult = AK_Success;
	Ensure3DAutomationAllocated();
	if (m_p3DAutomationParams)
	{
		if (m_p3DAutomationParams->PathIsDifferent(in_pArrayVertex, in_ulNumVertices, in_pArrayPlaylist, in_ulNumPlaylistItem))
		{
#ifndef AK_OPTIMIZED
			InvalidateAllPaths();
#endif
			eResult = m_p3DAutomationParams->SetPath(in_pArrayVertex, in_ulNumVertices, in_pArrayPlaylist, in_ulNumPlaylistItem);
		}
	}
	return eResult;
}

#ifndef AK_OPTIMIZED
void CAkParameterNodeBase::PosUpdatePathPoint(
	AkUInt32 in_ulPathIndex,
	AkUInt32 in_ulVertexIndex,
	AkVector in_newPosition,
	AkTimeMs in_DelayToNext
	)
{
	Ensure3DAutomationAllocated();
	if (m_p3DAutomationParams)
	{
		m_p3DAutomationParams->UpdatePathPoint(in_ulPathIndex, in_ulVertexIndex, in_newPosition, in_DelayToNext);
	}
}
#endif

void CAkParameterNodeBase::PosSetPathRange(AkUInt32 in_ulPathIndex, AkReal32 in_fXRange, AkReal32 in_fYRange, AkReal32 in_fZRange)
{
	Ensure3DAutomationAllocated();
	if (m_p3DAutomationParams && in_ulPathIndex < m_p3DAutomationParams->GetParams().m_ulNumPlaylistItem)
	{
		m_p3DAutomationParams->GetParams().m_pArrayPlaylist[in_ulPathIndex].fRangeX = in_fXRange;
		m_p3DAutomationParams->GetParams().m_pArrayPlaylist[in_ulPathIndex].fRangeY = in_fYRange;
		m_p3DAutomationParams->GetParams().m_pArrayPlaylist[in_ulPathIndex].fRangeZ = in_fZRange;
	}
}

#ifndef AK_OPTIMIZED
void CAkParameterNodeBase::InvalidatePositioning()
{
	//This function is not useless, useful to keep virtual table up when called on destructor
}

void CAkParameterNodeBase::InvalidateAllPaths()
{
	if (g_pAudioMgr)
		g_pAudioMgr->InvalidatePendingPaths(ID());
	InvalidatePositioning();
}
#endif


void CAkParameterNodeBase::FreePathInfo()
{
	if (m_p3DAutomationParams)
	{
		m_p3DAutomationParams->FreePathInfo();
	}
}

AkPathState* CAkParameterNodeBase::GetPathState()
{
	if (m_p3DAutomationParams)
	{
		return &(m_p3DAutomationParams->m_PathState);
	}
	else if (Parent())
	{
		return Parent()->GetPathState();
	}
	else
	{
		AKASSERT(!"Path not available");
		return NULL;
	}
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

//====================================================================================================
// check parents for 3D params
//====================================================================================================
void CAkParameterNodeBase::GetPositioningParams(
	const AkRTPCKey& in_rtpcKey,
	BaseGenParams & out_basePosParams,
	AkPositioningParams & out_posParams
	)
{
	AKASSERT(in_rtpcKey.GameObj());

	// Call parent until m_bPositioningInfoOverrideParent is true.
	// Normally m_bPositioningInfoOverrideParent is set on top-level but maybe some odd soundbank
	// design may decide otherwise.
	CAkParameterNode* pParent = static_cast<CAkParameterNode*>(Parent());
	if (PositioningInfoOverrideParent() || !Parent())
	{
		Get2DParams(in_rtpcKey, &out_basePosParams);
		out_posParams.settings = m_posSettings;
		if (HasRtpcOrState(RTPC_Positioning_EnableAttenuation))
			out_posParams.settings.m_bEnableAttenuation = (GetRTPCAndState(RTPC_Positioning_EnableAttenuation, in_rtpcKey) > 0);
		out_posParams.m_uAttenuationID = m_props.GetAkProp(AkPropID_AttenuationID, (AkInt32)AK_INVALID_UNIQUE_ID).iValue;
		if (HasRTPC(RTPC_PositioningTypeBlend))
			out_posParams.positioningTypeBlend = GetRTPCConvertedValue(RTPC_PositioningTypeBlend, in_rtpcKey); 
		else
			out_posParams.positioningTypeBlend = m_props.GetAkProp(AkPropID_PositioningTypeBlend, 100.f).fValue;
		// Leave cone stuff uninitialized. It will get refreshed by caller...
	}
	else
	{
		pParent->GetPositioningParams(in_rtpcKey, out_basePosParams, out_posParams);
	}
}
void CAkParameterNodeBase::Get3DAutomationParams(
	CAk3DAutomationParams*& io_rp3DParams
	)
{
	AKASSERT(io_rp3DParams == NULL);

	// Call parent until m_bPositioningInfoOverrideParent is true.
	// Normally m_bPositioningInfoOverrideParent is set on top-level but maybe some odd soundbank
	// design may decide otherwise.
	CAkParameterNode* pParent = static_cast<CAkParameterNode*>(Parent());
	if (PositioningInfoOverrideParent() || !Parent())
	{
		io_rp3DParams = Get3DCloneForObject();
	}
	else
	{
		pParent->Get3DAutomationParams(io_rp3DParams);
	}
}

AKRESULT CAkParameterNodeBase::GetStatic3DParams(AkPositioningInfo& out_rPosInfo)
{
	CAkParameterNodeBase* pAudioNode = this;

	memset(&out_rPosInfo, 0, sizeof(AkPositioningInfo)); //clean output structure

	while (pAudioNode != NULL)
	{
		CAkParameterNode* pParameterNode = static_cast<CAkParameterNode*>(pAudioNode);

		pAudioNode = pAudioNode->Parent();

		if (pParameterNode->PositioningInfoOverrideParent() || pAudioNode == NULL)
		{
			out_rPosInfo.pannerType = (AkSpeakerPanningType)pParameterNode->m_ePannerType;
			
			//Copy 3D params
			{
				out_rPosInfo.e3dPositioningType = (Ak3DPositionType)m_posSettings.m_e3DPositionType;
				out_rPosInfo.bHoldEmitterPosAndOrient = m_posSettings.m_bHoldEmitterPosAndOrient;
				out_rPosInfo.e3DSpatializationMode = (Ak3DSpatializationMode)m_posSettings.m_eSpatializationMode;

				//attenuation info
				AkUniqueID AttenuationID = m_props.GetAkProp(AkPropID_AttenuationID, (AkInt32)AK_INVALID_UNIQUE_ID).iValue;
				CAkAttenuation* pAttenuation = g_pIndex->m_idxAttenuations.GetPtrAndAddRef(AttenuationID);
				if (pAttenuation)
				{
					out_rPosInfo.bEnableAttenuation = m_posSettings.m_bEnableAttenuation;
					out_rPosInfo.bUseConeAttenuation = pAttenuation->m_bIsConeEnabled;
					if (pAttenuation->m_bIsConeEnabled)
					{
						out_rPosInfo.fInnerAngle = pAttenuation->m_ConeParams.fInsideAngle;
						out_rPosInfo.fOuterAngle = pAttenuation->m_ConeParams.fOutsideAngle;  //convert to degrees?
						out_rPosInfo.fConeMaxAttenuation = pAttenuation->m_ConeParams.fOutsideVolume; //convert to degrees?
						out_rPosInfo.LPFCone = pAttenuation->m_ConeParams.LoPass;
						out_rPosInfo.HPFCone = pAttenuation->m_ConeParams.HiPass;
					}

					CAkAttenuation::AkAttenuationCurve* pVolumeDryCurve = pAttenuation->GetCurve(AttenuationCurveID_VolumeDry);
					if (pVolumeDryCurve)
					{
						out_rPosInfo.fMaxDistance = pVolumeDryCurve->GetLastPoint().From;
						out_rPosInfo.fVolDryAtMaxDist = pVolumeDryCurve->GetLastPoint().To;
					}

					CAkAttenuation::AkAttenuationCurve* pVolumeAuxGameDefCurve = pAttenuation->GetCurve(AttenuationCurveID_VolumeAuxGameDef);
					if (pVolumeAuxGameDefCurve)
						out_rPosInfo.fVolAuxGameDefAtMaxDist = pVolumeAuxGameDefCurve->GetLastPoint().To;

					CAkAttenuation::AkAttenuationCurve* pVolumeAuxUserDefCurve = pAttenuation->GetCurve(AttenuationCurveID_VolumeAuxUserDef);
					if (pVolumeAuxUserDefCurve)
						out_rPosInfo.fVolAuxUserDefAtMaxDist = pVolumeAuxUserDefCurve->GetLastPoint().To;

					CAkAttenuation::AkAttenuationCurve* pLPFCurve = pAttenuation->GetCurve(AttenuationCurveID_LowPassFilter);
					if (pLPFCurve)
						out_rPosInfo.LPFValueAtMaxDist = pLPFCurve->GetLastPoint().To;

					CAkAttenuation::AkAttenuationCurve* pHPFCurve = pAttenuation->GetCurve(AttenuationCurveID_HighPassFilter);
					if (pHPFCurve)
						out_rPosInfo.HPFValueAtMaxDist = pHPFCurve->GetLastPoint().To;

					pAttenuation->Release();
				}
			}

			//Copy base params
			out_rPosInfo.fCenterPct = pParameterNode->m_props.GetAkProp(AkPropID_CenterPCT, 0.0f).fValue / 100.0f;
			return AK_Success;
		}
	}

	return AK_IDNotFound;
}

void CAkParameterNodeBase::UpdateBaseParams(const AkRTPCKey& in_rtpcKey, BaseGenParams * io_pBasePosParams, CAk3DAutomationParams * io_p3DParams)
{
	CAkParameterNodeBase* pAudioNode = this;

	do
	{
		CAkParameterNode* pParameterNode = static_cast<CAkParameterNode*>(pAudioNode);

		pAudioNode = pAudioNode->Parent();

		if (pParameterNode->PositioningInfoOverrideParent() || pAudioNode == NULL)
		{
			pParameterNode->Get2DParams(in_rtpcKey, io_pBasePosParams);

			if (io_p3DParams)
			{
				pParameterNode->Get3DPanning(in_rtpcKey, io_p3DParams->GetParams().m_Position);
			}

			return;
		}
	} while (pAudioNode != NULL);
}

bool CAkParameterNodeBase::GetMaxRadius(AkReal32 & out_fRadius)
{
	CAkParameterNodeBase* pAudioNode = this;
	out_fRadius = 0.0f;

	do
	{
		CAkParameterNode* pParameterNode = static_cast<CAkParameterNode*>(pAudioNode);

		pAudioNode = pAudioNode->Parent();

		if (pParameterNode->PositioningInfoOverrideParent() || pAudioNode == NULL)
		{
			bool bReturnValue = false;

			AkUniqueID AttenuationID = pParameterNode->m_props.GetAkProp(AkPropID_AttenuationID, (AkInt32)AK_INVALID_UNIQUE_ID).iValue;
			CAkAttenuation* pAttenuation = g_pIndex->m_idxAttenuations.GetPtrAndAddRef(AttenuationID);
			if (pAttenuation)
			{
				CAkAttenuation::AkAttenuationCurve* pVolumeDryCurve = pAttenuation->GetCurve(AttenuationCurveID_VolumeDry);
				if (pVolumeDryCurve)
				{
					out_fRadius = pVolumeDryCurve->GetLastPoint().From;
					bReturnValue = true; //has attenuation
				}
				pAttenuation->Release();
			}
			
			return bReturnValue;
		}
	} while (pAudioNode != NULL);

	return false;
}

CAk3DAutomationParams* CAkParameterNodeBase::Get3DCloneForObject()
{
	CAk3DAutomationParams* p3DParams = NULL;
	if (m_p3DAutomationParams)
	{
		p3DParams = AkNew(AkMemID_Processing, CAk3DAutomationParams());
		if (p3DParams)
		{
			// Copying parameters. Will copy the array pointer for the path.
			*p3DParams = *m_p3DAutomationParams;
		}
	}
	return p3DParams;
}

// Monitoring solo/mute (profiling only)
#ifndef AK_OPTIMIZED
void CAkParameterNodeBase::MonitoringSolo( bool in_bSolo )
{
	_MonitoringSolo( in_bSolo, g_uSoloCount );
}

void CAkParameterNodeBase::MonitoringMute( bool in_bMute )
{
	_MonitoringMute( in_bMute, g_uMuteCount );
}

void CAkParameterNodeBase::_MonitoringSolo( bool in_bSolo, AkUInt32& io_ruSoloCount )
{
	if (in_bSolo && !m_bIsSoloed)
		++io_ruSoloCount;
	else if (!in_bSolo && m_bIsSoloed)
		--io_ruSoloCount;

	m_bIsSoloed = in_bSolo;

	g_bIsMonitoringMuteSoloDirty = true;
}

void CAkParameterNodeBase::_MonitoringMute( bool in_bMute, AkUInt32& io_ruMuteCount )
{
	if (in_bMute && !m_bIsMuted)
		++io_ruMuteCount;
	else if (!in_bMute && m_bIsMuted)
		--io_ruMuteCount;
	m_bIsMuted = in_bMute;

	g_bIsMonitoringMuteSoloDirty = true;
}

bool CAkParameterNodeBase::IsRefreshMonitoringMuteSoloNeeded() 
{ 
	bool bDirty = g_bIsMonitoringMuteSoloDirty; 
	g_bIsMonitoringMuteSoloDirty = false;
	return bDirty; 
}

void CAkParameterNodeBase::GetMonitoringMuteSoloState( 
	bool & io_bSolo,	// Pass false. Bit is OR'ed against each node of the signal flow.
	bool & io_bMute		// Pass false. Bit is OR'ed against each node of the signal flow.
	)
{
	io_bSolo = io_bSolo || m_bIsSoloed;
	io_bMute = io_bMute || m_bIsMuted;
	if ( io_bMute )	// Mute wins. Bail out.
		return;

	// Ask parent of the actor-mixer hierarchy.
	if ( m_pParentNode )
		m_pParentNode->GetMonitoringMuteSoloState( io_bSolo, io_bMute );
}

void CAkParameterNodeBase::GetCtrlBusMonitoringMuteSoloState(
	bool & io_bSolo,	// Pass false. Bit is OR'ed against each node of the signal flow.
	bool & io_bMute		// Pass false. Bit is OR'ed against each node of the signal flow.
	)
{
	CAkBus* pBus = GetControlBus();

	while (!io_bMute && pBus != NULL && !pBus->IsMixingBus())
	{
		io_bSolo = io_bSolo || pBus->m_bIsSoloed;
		io_bMute = io_bMute || pBus->m_bIsMuted;
		pBus = (CAkBus*)(pBus->m_pBusOutputNode);
	}
}

#endif



AkUInt32 CAkParameterNodeBase::GetDepth()
{
	if ( m_pParentNode )
		return m_pParentNode->GetDepth() + 1;
	
	return 0;
}

