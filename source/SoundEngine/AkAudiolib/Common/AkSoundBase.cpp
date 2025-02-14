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
// AkSoundBase.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkSoundBase.h"

CAkSoundBase::CAkSoundBase( 
    AkUniqueID in_ulID
    )
:CAkParameterNode(in_ulID)
{
}

CAkSoundBase::~CAkSoundBase()
{
}

void CAkSoundBase::ProcessCommand( ActionParams& in_rAction )
{
	AKASSERT( IsActivityChunkEnabled() );
	AkActivityChunk::AkListLightCtxs& rListPBI = GetActivityChunk()->m_listPBI;
	for( AkActivityChunk::AkListLightCtxs::Iterator iter = rListPBI.Begin(); iter != rListPBI.End(); ++iter )
	{
		CAkPBI* l_pPBI = (CAkPBI*)*iter;
		l_pPBI->ProcessCommand( in_rAction );
	}
}

void CAkSoundBase::PlayToEnd( CAkRegisteredObj * in_pGameObj, CAkParameterNodeBase* in_pNodePtr, AkPlayingID in_PlayingID )
{	
	if( IsActivityChunkEnabled() )
	{
		AkActivityChunk::AkListLightCtxs& rListPBI = GetActivityChunk()->m_listPBI;
		for( AkActivityChunk::AkListLightCtxs::Iterator iter = rListPBI.Begin(); iter != rListPBI.End(); ++iter )
		{
			CAkPBI * l_pPBI = (CAkPBI*)*iter;
			if( ( !in_pGameObj || l_pPBI->GetGameObjectPtr() == in_pGameObj ) &&
				( in_PlayingID == AK_INVALID_PLAYING_ID || in_PlayingID == l_pPBI->GetPlayingID() ) )
			{
				l_pPBI->PlayToEnd( in_pNodePtr );
			}
		}
	}
}

void CAkSoundBase::ParamNotification( NotifParams& in_rParams )
{
	CAkPBI* l_pPBI    = NULL;

	if( IsActivityChunkEnabled() )
	{
		AkActivityChunk::AkListLightCtxs& rListPBI = GetActivityChunk()->m_listPBI;
		for( AkActivityChunk::AkListLightCtxs::Iterator iter = rListPBI.Begin(); iter != rListPBI.End(); ++iter )
		{
			l_pPBI = (CAkPBI*)*iter;
			AKASSERT( l_pPBI != NULL );

			if( in_rParams.pExceptCheck == NULL || !in_rParams.pExceptCheck->IsException( l_pPBI->GetRTPCKey() ) )
			{
				// Update Behavioural side.
				l_pPBI->ParamNotification( in_rParams );
			}
		}
	}
}

void CAkSoundBase::MuteNotification(AkReal32 in_fMuteRatio, AkMutedMapItem& in_rMutedItem, bool /*in_bIsFromBus = false*/ )
{
	if( IsActivityChunkEnabled() )
	{
		AkActivityChunk::AkListLightCtxs& rListPBI = GetActivityChunk()->m_listPBI;
		for( AkActivityChunk::AkListLightCtxs::Iterator iter = rListPBI.Begin(); iter != rListPBI.End(); ++iter )
		{
			((CAkPBI*)*iter)->MuteNotification(in_fMuteRatio, in_rMutedItem, false);
		}
	}
}

void CAkSoundBase::MuteNotification( AkReal32 in_fMuteRatio, CAkRegisteredObj * in_pGameObj, AkMutedMapItem& in_rMutedItem, bool in_bPrioritizeGameObjectSpecificItems /* = false */ )
{
	const bool bForceNotify = !in_pGameObj || ( in_bPrioritizeGameObjectSpecificItems && ! in_pGameObj );

	if( IsActivityChunkEnabled() )
	{
		AkActivityChunk::AkListLightCtxs& rListPBI = GetActivityChunk()->m_listPBI;
		for( AkActivityChunk::AkListLightCtxs::Iterator iter = rListPBI.Begin(); iter != rListPBI.End(); ++iter )
		{
			CAkPBI * l_pPBI = (CAkPBI*)*iter;

			if( bForceNotify || l_pPBI->GetGameObjectPtr() == in_pGameObj )
			{
				// Update Behavioural side.
				l_pPBI->MuteNotification( in_fMuteRatio, in_rMutedItem, in_bPrioritizeGameObjectSpecificItems );
			}
		}
	}
}

void CAkSoundBase::ForAllPBI( 
	AkForAllPBIFunc in_funcForAll,
	const AkRTPCKey & in_rtpcKey,
	void * in_pCookie )
{
	if( IsActivityChunkEnabled() )
	{
		AkActivityChunk::AkListLightCtxs& rListPBI = GetActivityChunk()->m_listPBI;
		for( AkActivityChunk::AkListLightCtxs::Iterator iter = rListPBI.Begin(); iter != rListPBI.End(); ++iter )
		{
			if( (*iter)->GetRTPCKey().MatchValidFields( in_rtpcKey ) )
				in_funcForAll((CAkPBI*)*iter, in_rtpcKey, in_pCookie);
		}
	}
}

// Notify the children PBI that a change int the Positioning parameters occured from RTPC
void CAkSoundBase::PropagatePositioningNotification( AkReal32 in_RTPCValue, AkRTPC_ParameterID in_ParameterID, CAkRegisteredObj * in_pGameObj, AkRTPCExceptionChecker*	in_pExceptCheck )
{
	if( IsActivityChunkEnabled() )
	{
		AkActivityChunk::AkListLightCtxs& rListPBI = GetActivityChunk()->m_listPBI;
		for( AkActivityChunk::AkListLightCtxs::Iterator iter = rListPBI.Begin(); iter != rListPBI.End(); ++iter )
		{
			CAkBehavioralCtx * l_pPBI = *iter;

			if( in_pExceptCheck == NULL || !in_pExceptCheck->IsException( l_pPBI->GetRTPCKey() ) )
			{
				if( in_pGameObj == NULL || l_pPBI->GetGameObjectPtr() == in_pGameObj )
				{
					l_pPBI->PositioningChangeNotification( in_RTPCValue, in_ParameterID );
				}
			}
		}
	}
}

void CAkSoundBase::RecalcNotification( bool in_bLiveEdit, bool in_bLog )
{
	if( IsActivityChunkEnabled() )
	{
		AkActivityChunk::AkListLightCtxs& rListPBI = GetActivityChunk()->m_listPBI;
		for( AkActivityChunk::AkListLightCtxs::Iterator iter = rListPBI.Begin(); iter != rListPBI.End(); ++iter )
		{
			(*iter)->RecalcNotification(in_bLiveEdit, in_bLog);
		}
	}
}

void CAkSoundBase::ClearLimiters()
{
	if( IsActivityChunkEnabled() )
	{
		AkActivityChunk::AkListLightCtxs& rListPBI = GetActivityChunk()->m_listPBI;
		for( AkActivityChunk::AkListLightCtxs::Iterator iter = rListPBI.Begin(); iter != rListPBI.End(); ++iter )
		{
			CAkPBI* pPBI = (CAkPBI*)*iter;
			pPBI->ClearLimiters();
		}
	}
}

void CAkSoundBase::NotifyBypass(
	AkUInt32 in_bitsFXBypass,
	AkUInt32 in_uTargetMask,
	CAkRegisteredObj * in_pGameObj,
	AkRTPCExceptionChecker* in_pExceptCheck /* = NULL */ )
{
	if( IsActivityChunkEnabled() )
	{
		AkActivityChunk::AkListLightCtxs& rListPBI = GetActivityChunk()->m_listPBI;
		for( AkActivityChunk::AkListLightCtxs::Iterator iter = rListPBI.Begin(); iter != rListPBI.End(); ++iter )
		{
			CAkPBI* l_pPBI = (CAkPBI*)*iter;
			if( ( in_pExceptCheck == NULL || !in_pExceptCheck->IsException(l_pPBI->GetRTPCKey()) ) 
				&&
				( in_pGameObj == NULL || l_pPBI->GetGameObjectPtr() == in_pGameObj ) 
				)
			{
				// Update Behavioural side.
				l_pPBI->NotifyBypass( in_bitsFXBypass, in_uTargetMask );
			}
		}
	}
}

void CAkSoundBase::UpdateFx(
		AkUInt32	   	in_uFXIndex
		)
{
	if( IsActivityChunkEnabled() )
	{
		AkActivityChunk::AkListLightCtxs& rListPBI = GetActivityChunk()->m_listPBI;
		for( AkActivityChunk::AkListLightCtxs::Iterator iter = rListPBI.Begin(); iter != rListPBI.End(); ++iter )
		{
			((CAkPBI*)*iter)->UpdateFx(in_uFXIndex);
		}
	}
}

#ifndef AK_OPTIMIZED

void CAkSoundBase::InvalidatePositioning()
{
	if( IsActivityChunkEnabled() )
	{
		AkActivityChunk::AkListLightCtxs& rListPBI = GetActivityChunk()->m_listPBI;
		for( AkActivityChunk::AkListLightCtxs::Iterator iter = rListPBI.Begin(); iter != rListPBI.End(); ++iter )
		{
			(*iter)->InvalidatePositioning();
		}
	}
}

#endif
bool CAkSoundBase::IsInfiniteLooping( CAkPBIAware* in_pInstigator )
{
	if ( Loop() == 0 )
		return true;

	return CAkParameterNodeBase::IsInfiniteLooping( in_pInstigator );
}

AkInt16 CAkSoundBase::Loop()
{
	AkInt32 iLoop = m_props.GetAkProp( AkPropID_Loop, (AkInt32) AkLoopVal_NotLooping ).iValue;
	ApplyRange<AkInt32>( AkPropID_Loop, iLoop );
	return (AkInt16) iLoop;
}

AkReal32 CAkSoundBase::LoopStartOffset() const
{
	AkReal32 fLoopStart = m_props.GetAkProp( AkPropID_LoopStart, 0.0f ).fValue;
	return fLoopStart;
}

AkReal32 CAkSoundBase::LoopEndOffset() const
{
	AkReal32 fLoopEnd = m_props.GetAkProp( AkPropID_LoopEnd, 0.0f ).fValue;
	return fLoopEnd;
}

AkReal32 CAkSoundBase::LoopCrossfadeDuration() const
{
	AkReal32 fLoopCrossfadeDuration = m_props.GetAkProp( AkPropID_LoopCrossfadeDuration, 0.0f ).fValue;
	return fLoopCrossfadeDuration;
}

void CAkSoundBase::LoopCrossfadeCurveShape( AkCurveInterpolation& out_eCrossfadeUpType,  AkCurveInterpolation& out_eCrossfadeDownType) const
{
	out_eCrossfadeUpType = (AkCurveInterpolation)m_props.GetAkProp( AkPropID_CrossfadeUpCurve, (AkInt32)AkCurveInterpolation_Sine ).iValue;
	out_eCrossfadeDownType = (AkCurveInterpolation)m_props.GetAkProp( AkPropID_CrossfadeDownCurve, (AkInt32)AkCurveInterpolation_SineRecip ).iValue;
}

void CAkSoundBase::GetTrim( AkReal32& out_fBeginTrimOffsetSec, AkReal32& out_fEndTrimOffsetSec ) const
{
	out_fBeginTrimOffsetSec = m_props.GetAkProp( AkPropID_TrimInTime, 0.0f ).fValue;
	out_fEndTrimOffsetSec = m_props.GetAkProp( AkPropID_TrimOutTime, 0.0f ).fValue;
}

void CAkSoundBase::GetFade(	AkReal32& out_fBeginFadeOffsetSec, AkCurveInterpolation& out_eBeginFadeCurveType, 
				AkReal32& out_fEndFadeOffsetSec, AkCurveInterpolation& out_eEndFadeCurveType  ) const
{
	out_fBeginFadeOffsetSec = m_props.GetAkProp( AkPropID_FadeInTime, 0.0f ).fValue;
	out_fEndFadeOffsetSec = m_props.GetAkProp( AkPropID_FadeOutTime, 0.0f ).fValue;

	out_eBeginFadeCurveType = (AkCurveInterpolation)m_props.GetAkProp( AkPropID_FadeInCurve, (AkInt32)AkCurveInterpolation_Sine ).iValue;
	out_eEndFadeCurveType = (AkCurveInterpolation)m_props.GetAkProp( AkPropID_FadeOutCurve, (AkInt32)AkCurveInterpolation_Sine ).iValue;
}

void CAkSoundBase::MonitorNotif(AkMonitorData::NotificationReason in_eNotifReason, AkGameObjectID in_GameObjID, UserParams& in_rUserParams, PlayHistory& in_rPlayHistory)
{
	MONITOR_OBJECTNOTIF( in_rUserParams.PlayingID(), in_GameObjID, in_rUserParams.CustomParam(), in_eNotifReason, in_rPlayHistory.HistArray, ID(), false, 0 );
}
