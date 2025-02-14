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

//////////////////////////////////////////////////////////////////////
//
// AkURenderer.cpp
//
// Implementation of the Audio renderer
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "AkURenderer.h"
#include "AkLEngine.h"
#include "AkSoundBase.h"
#include "Ak3DListener.h"
#include "AkAudioMgr.h"
#include "AkPlayingMgr.h"
#include "AkRegistryMgr.h"

//-----------------------------------------------------------------------------
// External variables.
//-----------------------------------------------------------------------------
extern CAkPlayingMgr* g_pPlayingMgr;

//-----------------------------------------------------------------------------
// Defines.
//-----------------------------------------------------------------------------
// List sizes.
#define MIN_NUM_CTX					64

#define MIN_NUM_PLAY_EVENT			16
#define MIN_NUM_RENDER_EVENT		64 // must not be zero

#define HARDCODED_MAX_NUM_SOUNDS_UNLIMITED	(0)
#define DEFAULT_MAX_NUM_SOUNDS	(256)

//-----------------------------------------------------------------------------
//Static variables.
//-----------------------------------------------------------------------------
CAkURenderer::AkListCtxs			CAkURenderer::m_listCtxs;
CAkURenderer::AkListNodes			CAkURenderer::m_listActiveNodes;
CAkURenderer::AkContextNotifQueue	CAkURenderer::m_CtxNotifQueue;
CAkLock								CAkURenderer::m_CtxNotifLock;
AkUInt32							CAkURenderer::m_uNumVirtualizedSounds = 0;
AkUInt32							CAkURenderer::m_uNumDangerousVirtualizedSounds = 0;
AkUInt32							CAkURenderer::m_uMaxNumDangerousVirtVoices = 0;

CAkURenderer::AkSortedLimiters		CAkURenderer::m_Limiters;
CAkLimiter							CAkURenderer::m_GlobalLimiter( DEFAULT_MAX_NUM_SOUNDS, true, true );
bool								CAkURenderer::m_bLimitersDirty( false );
bool								CAkURenderer::m_bBusInvalid( false );

//-----------------------------------------------------------------------------
// Name: Init
// Desc: Initialise the object.
//
// Return: 
//	Ak_Success:          Object was initialised correctly.
//  AK_InvalidParameter: Invalid parameters.
//  AK_Fail:             Failed to initialise the object correctly.
//-----------------------------------------------------------------------------
AKRESULT CAkURenderer::Init()
{
	m_uNumVirtualizedSounds = 0;
	m_uNumDangerousVirtualizedSounds = 0;
	m_uMaxNumDangerousVirtVoices = 0;

	AKRESULT l_eResult = m_CtxNotifQueue.Reserve( MIN_NUM_RENDER_EVENT );
	if( l_eResult != AK_Success ) return l_eResult;
	m_GlobalLimiter.SetKey( AkLimiterKey::LimiterType_Global, 0, 0 );
	// Initialize the Lower Audio Engine. 
	l_eResult = CAkLEngine::Init();

    return l_eResult;
} // Init

// Should only be called by CAkParameterNodeBase
void CAkURenderer::AddToActiveNodes(CAkParameterNodeBase * in_pNode)
{
	AKASSERT(in_pNode);
	CAkURenderer::AkListNodes::Iterator it = m_listActiveNodes.FindEx(in_pNode);
	if (it == m_listActiveNodes.End())
		m_listActiveNodes.AddLast(in_pNode);
}

// Should only be called by CAkParameterNodeBase
void CAkURenderer::RemoveFromActiveNodes(CAkParameterNodeBase * in_pNode)
{
	AKASSERT(in_pNode);
	m_listActiveNodes.Remove(in_pNode);
}

//-----------------------------------------------------------------------------
// Name: Term
// Desc: Terminate the object.
//-----------------------------------------------------------------------------
void CAkURenderer::Term()
{
	CAkLEngine::Term();

	DestroyAllPBIs();

	m_listCtxs.Term();

	m_CtxNotifQueue.Term();

	AKASSERT( m_uNumVirtualizedSounds == 0 );

	m_GlobalLimiter.Term(); // Must be terminated before m_Limiters.
	m_Limiters.Term();
} // Term

PriorityInfoCurrent CAkURenderer::_CalcInitialPriority( CAkSoundBase * in_pSound, CAkRegisteredObj * in_pGameObj, AkReal32& out_fMaxRadius )
{
	out_fMaxRadius = 0.f;
	PriorityInfoCurrent priorityInfoCurrent( in_pSound->GetPriority( in_pGameObj ) );
	if ( in_pSound->GetMaxRadius( out_fMaxRadius ) )
	{
		CAkEmitter* pEmitter = in_pGameObj->GetComponent<CAkEmitter>();
		out_fMaxRadius *= pEmitter->GetScalingFactor();
		if( priorityInfoCurrent.DistanceOffset() )
		{
			AkReal32 fMinDistance = GetMinDistance(pEmitter->GetPosition(), in_pGameObj->GetListeners());

			AkReal32 priorityTemp;
			if ( fMinDistance < out_fMaxRadius )
				priorityTemp = priorityInfoCurrent.BasePriority() + ( fMinDistance/out_fMaxRadius * priorityInfoCurrent.DistanceOffset() );
			else
				priorityTemp = priorityInfoCurrent.BasePriority() + priorityInfoCurrent.DistanceOffset();

			priorityInfoCurrent.SetCurrent(priorityTemp);
		}
	}

	return priorityInfoCurrent;
}

AkReal32 CAkURenderer::GetMinDistance(const AkSoundPositionRef& in_rPosRef, const AkListenerSet& listeners)
{
	AkReal32 fMinDistance = AK_UPPER_MAX_DISTANCE;

	for (AkListenerSet::Iterator it = listeners.Begin(); it != listeners.End(); ++it)
	{
		const CAkListener* pListener = CAkListener::GetListenerData( *it );
		if (pListener != NULL)
		{
			for (AkUInt32 i = 0; i < in_rPosRef.GetNumPosition(); ++i)
			{
				AkReal32 fDistance = AkMath::Distance(pListener->GetTransform().Position(), in_rPosRef.GetPositions()[i].position.Position());
				fDistance = fDistance / pListener->GetScalingFactor();// Apply listener scaling factor.
				fMinDistance = AkMath::Min(fMinDistance, fDistance);
			}
		}
	}

	return fMinDistance;
}

static AKRESULT ResolveExternalSource(CAkSource* &io_pSource, AkPBIParams& in_rPBIParams)
{
	AkExternalSourceArray* pSourceArray = in_rPBIParams.userParams.CustomParam().pExternalSrcs;
	if (pSourceArray == NULL)
	{
		io_pSource = NULL;
		return AK_Fail;
	}

	//Clone the source for this instance of the template.
	io_pSource = io_pSource->Clone();
	if (io_pSource == NULL)
		return AK_InsufficientMemory;

	const AkExternalSourceInfo *pInfo = pSourceArray->Sources();
	for(AkUInt32 i = 0; i < pSourceArray->Count(); i++)
	{
		if (io_pSource->GetExternalSrcCookie() == pInfo->iExternalSrcCookie)
		{
			AkUInt32 pluginID = ( AkPluginTypeCodec + ( (pInfo->idCodec) << ( 4 + 12 ) ) );
			if (pInfo->idFile != 0)
			{
				io_pSource->SetSource(
					io_pSource->GetSourceID(),
					pluginID,
					pInfo->szFile,
					pInfo->idFile,
					false, // Do not use filename to open (but do use it for stream name)
					true);
				return AK_Success;
			}
			else if (pInfo->szFile != NULL)
			{
				// External source.
				io_pSource->SetSource(
					io_pSource->GetSourceID(), 
					pluginID, 
					pInfo->szFile, 
					AK_INVALID_FILE_ID,	// No caching for external sources
					true, // Use filename to open 
					true );
				return AK_Success;
			}
			else if (pInfo->uiMemorySize > 0 && pInfo->pInMemory != NULL)
			{
				AkMediaInformation mediaInfo;
				mediaInfo.SetDefault( SrcTypeMemory, io_pSource->GetSourceID() );
				mediaInfo.bHasSource = true;
				mediaInfo.uInMemoryMediaSize = pInfo->uiMemorySize;
				mediaInfo.bExternallySupplied = true;
				io_pSource->SetSource(pluginID, pInfo->pInMemory, mediaInfo);
				return AK_Success;
			}
		}

		pInfo++;
	}

	//No replacement for this source.
	AkDelete(AkMemID_Object, io_pSource);
	io_pSource = NULL;

	return AK_Fail;
}

static AKRESULT HandleResolveExternalSource(CAkSoundBase*  in_pSound, CAkSource* &io_pSource, AkPBIParams& in_rPBIParams)
{
	// Helper created to make code more readable on PBI creation.
	AKRESULT eResult = ResolveExternalSource( io_pSource, in_rPBIParams );
#ifdef AK_OPTIMIZED
	if (eResult != AK_Success)
	{
		return eResult;
	}
#else
	if (eResult != AK_Success)
	{		
		MONITOR_ERROREX(AK::Monitor::ErrorCode_ExternalSourceNotResolved, in_rPBIParams.userParams.PlayingID(), in_rPBIParams.pGameObj->ID(), in_pSound->ID(), false);
		return eResult;
	}
	else
	{
		if (io_pSource->GetSrcTypeInfo()->mediaInfo.Type == SrcTypeMemory)
		{
			AkOSChar szMsg[] = AKTEXT("0x00000000000000000(memory block)");	//The string is there only to size the array properly.
			AK_OSPRINTF(szMsg, AKPLATFORM::OsStrLen(szMsg)+1, AKTEXT("%p(memory block)"), io_pSource->GetSrcTypeInfo()->GetMedia());
			MONITOR_TEMPLATESOURCE( in_rPBIParams.userParams.PlayingID(), in_rPBIParams.pGameObj->ID(), io_pSource->GetSourceID(), szMsg);
		}
		// or else we have a file source, specified by string or by file ID.
		else if (io_pSource->GetSrcTypeInfo()->GetFilename() != NULL)
		{
			MONITOR_TEMPLATESOURCE( in_rPBIParams.userParams.PlayingID(), in_rPBIParams.pGameObj->ID(), io_pSource->GetSourceID(), io_pSource->GetSrcTypeInfo()->GetFilename());
		}
		else if (io_pSource->GetSrcTypeInfo()->GetFileID() != AK_INVALID_FILE_ID)
		{
			AkOSChar szMsg[] = AKTEXT("1234567890(FileID)");
			AK_OSPRINTF(szMsg, AKPLATFORM::OsStrLen(szMsg)+1, AKTEXT("%u(FileID)"), io_pSource->GetSrcTypeInfo()->GetFileID() );
			MONITOR_TEMPLATESOURCE( in_rPBIParams.userParams.PlayingID(), in_rPBIParams.pGameObj->ID(), io_pSource->GetSourceID(), szMsg);
		}
	}
#endif

	return eResult;
}

bool CAkURenderer::GetVirtualBehaviorAction(CAkSoundBase* in_pSound, CAkPBIAware* in_pInstigator, AkBelowThresholdBehavior& out_eBelowThresholdBehavior)
{
	AkVirtualQueueBehavior _unused; 
	out_eBelowThresholdBehavior = in_pSound->GetVirtualBehavior(_unused);

	if (out_eBelowThresholdBehavior == AkBelowThresholdBehavior_KillIfOneShotElseVirtual)
	{
		if ( !in_pInstigator || in_pSound->Category() == ObjCategory_MusicNode )
			out_eBelowThresholdBehavior = AkBelowThresholdBehavior_SetAsVirtualVoice;
		else if ( in_pSound->IsInfiniteLooping( in_pInstigator ) )
			out_eBelowThresholdBehavior = AkBelowThresholdBehavior_SetAsVirtualVoice;
		else
			out_eBelowThresholdBehavior = AkBelowThresholdBehavior_KillVoice;
	}


	bool bAllowedToPlay = true;

	switch (out_eBelowThresholdBehavior)
	{
	case AkBelowThresholdBehavior_KillVoice:
		bAllowedToPlay = false;
		break;

	case AkBelowThresholdBehavior_ContinueToPlay:
		// Let it continue to play, this sound is exempted.
		break;

	case AkBelowThresholdBehavior_SetAsVirtualVoice:
		break;

	default:
		AKASSERT( !"Unhandled below threshold type" );
		break;
	}

	return bAllowedToPlay;
}

AKRESULT CAkURenderer::IncrementPlayCountAndInit(CAkSoundBase* in_pSound, CAkRegisteredObj * in_pGameObj, AkReal32 in_fPriority, AKRESULT in_eValidateLimitsResult, bool in_bAllowedToPlayIfUnderThreshold, AkMonitorData::NotificationReason& io_eReason, CAkPBI* in_pPBI, bool in_bAllowKick)
{
	AKRESULT eResult = AK_Success;
	CounterParameters counterParams;
	counterParams.fPriority = in_fPriority;
	counterParams.pGameObj = in_pGameObj;
	counterParams.bAllowKick = !in_pPBI->IsExemptedFromLimiter() && in_bAllowKick;
	counterParams.pLimiters = in_pPBI->PlayDirectly() ? NULL : &(in_pPBI->m_LimiterArray);
	AKRESULT eIncrementPlayCountResult = in_pSound->IncrementPlayCount( counterParams, true );

	bool bAllowedToPlay = eIncrementPlayCountResult != AK_Fail;

	if (bAllowedToPlay && (eIncrementPlayCountResult == AK_MustBeVirtualized || in_eValidateLimitsResult == AK_MustBeVirtualized) )
	{
		bAllowedToPlay = in_bAllowedToPlayIfUnderThreshold;
		in_pPBI->ForceVirtualize();
	}
	if( !bAllowedToPlay )
	{
		eResult = AK_PartialSuccess;// That mean is not an error, the sound did not play simply because the max num instance made it abort.
		io_eReason = AkMonitorData::NotificationReason_PlayFailedLimit;
	}
	else
	{
		eResult = in_pPBI->Init();
	}

	return eResult;
}

void CAkURenderer::ClearPBIAndDecrement( CAkSoundBase* in_pSound, CAkPBI*& io_pPBI, bool in_bCalledIncrementPlayCount, CAkRegisteredObj * in_pGameObj )
{
	if( io_pPBI )
	{
		if( !in_bCalledIncrementPlayCount )
		{
			io_pPBI->SkipDecrementPlayCount();
		}
		io_pPBI->Term( true ); // does call DecrementPlayCount()
		AkDelete( AkMemID_Object, io_pPBI );
		io_pPBI = NULL;
	}
	else if( in_bCalledIncrementPlayCount )//and !l_pContext
	{
		CounterParameters counterParams;
		counterParams.pGameObj = in_pGameObj;
		in_pSound->DecrementPlayCount( counterParams );
	}
}

AKRESULT CAkURenderer::Play( CAkSoundBase*  in_pSound,
							 CAkSource*		in_pSource,
                             AkPBIParams&   in_rPBIParams )
{
	AKRESULT eResult = AK_Fail;
	CAkPBI * l_pContext = NULL;
	bool bCalledIncrementPlayCount = false;
	bool bPlayingIDIncremented = false;
	AkMonitorData::NotificationReason eReason;
	AkBelowThresholdBehavior eBelowThresholdBehavior;
	bool bAllowedToPlayIfUnderThreshold = GetVirtualBehaviorAction(in_pSound, in_rPBIParams.pInstigator, eBelowThresholdBehavior);
	
	AkReal32 fMaxRadius;
    PriorityInfoCurrent priority = _CalcInitialPriority( in_pSound, in_rPBIParams.pGameObj, fMaxRadius );

	AKRESULT eValidateLimitsResult = ValidateLimits( priority.GetCurrent(), eReason );
	if( eValidateLimitsResult == AK_Fail )
	{
		eResult = AK_PartialSuccess;// That mean is not an error, the sound did not play simply because the max num instance made it abort.
	}
	else
	{
		bool bWeOwnClonedSource = false;

		if ( AK_EXPECT_FALSE(in_pSource->IsExternal()) )
		{
			eResult = HandleResolveExternalSource( in_pSound, in_pSource, in_rPBIParams );
			// in_pSource has now been cloned. This function owns that memory until the pbi is created.
			bWeOwnClonedSource = (eResult == AK_Success);
		}
		else
		{
			eResult = AK_Success;
		}

		bool bIsInitiallyUnderThreshold = false;
		if( eResult == AK_Success )
		{
			// Just create the PBI, don't Init it yet.
			l_pContext = in_rPBIParams.pInstigator->CreatePBI( in_pSound, in_pSource, in_rPBIParams, priority );

			if( l_pContext )
			{
				if ( in_rPBIParams.eType == AkPBIParams::PBI )
				{
					eResult = l_pContext->PreInit(fMaxRadius, NULL, bAllowedToPlayIfUnderThreshold, eReason, &in_rPBIParams.initialSoundParams, bIsInitiallyUnderThreshold);
				}
				else
				{
					eResult = l_pContext->PreInit(fMaxRadius, in_rPBIParams.pContinuousParams->pPathInfo, bAllowedToPlayIfUnderThreshold, eReason, &in_rPBIParams.initialSoundParams, bIsInitiallyUnderThreshold);
				}
			}
			else
			{
				if ( bWeOwnClonedSource )
				{
					AkDelete(AkMemID_Object, in_pSource);
					in_pSource = NULL;
				}
				eResult = AK_Fail;
			}

			// Regardless of whether the pbi was created or not, we're no longer responsible for the cloned memory.
			// There's no real need to set this, it's just for clarity.
			bWeOwnClonedSource = false;
		}

		if( eResult == AK_Success )
		{
			bool bAllowKicking = !bIsInitiallyUnderThreshold || eBelowThresholdBehavior == AkBelowThresholdBehavior_ContinueToPlay;
			eResult = IncrementPlayCountAndInit(in_pSound, in_rPBIParams.pGameObj, priority.GetCurrent(), eValidateLimitsResult, bAllowedToPlayIfUnderThreshold, eReason, l_pContext, bAllowKicking);
			bCalledIncrementPlayCount = true;
			bPlayingIDIncremented = (eResult == AK_Success);
		}

		if( eResult == AK_Success )
		{
			l_pContext->CalcEffectiveParams(&in_rPBIParams.initialSoundParams);
			l_pContext->TriggerModulators(in_rPBIParams.initialSoundParams.modulators, true);
			
			//Pass on the modulator data and pathInfo for any subsequent play actions (eg. trigger rate container)
			if (in_rPBIParams.pContinuousParams != NULL)
			{
				l_pContext->GetModulatorData().GetModulatorCtxs(AkModulatorScope_Note, in_rPBIParams.pContinuousParams->triggeredModulators);
				in_rPBIParams.pContinuousParams->pPathInfo = l_pContext->GetPathInfo();
			}
			
			eResult = Play( l_pContext, *in_rPBIParams.pTransitionParameters, in_rPBIParams.ePlaybackState );
			if (in_rPBIParams.pContinuousParams != NULL)
			{
				// Continuous transition info transferred back to PBI params so containers can access them (e.g. RanSeqCntr::_PlayTrigger)
				in_rPBIParams.pContinuousParams->pPauseResumeTransition = l_pContext->GetPauseResumeTransition();
				in_rPBIParams.pContinuousParams->pPlayStopTransition = l_pContext->GetPlayStopTransition();
			}
		}
	}

	///////////////////////////
	// ERROR Handling
	///////////////////////////

	if( eResult != AK_Success )
	{
		if (!bPlayingIDIncremented && in_rPBIParams.userParams.PlayingID() && l_pContext)
		{			
			//Make sure the PlayingManager can manage the Event count properly.  Normally this should have been done in IncrementPlayCount, 
			//but in case of error it could have been skipped.
			AkUInt32 uDontCare;
			g_pPlayingMgr->SetPBI( in_rPBIParams.userParams.PlayingID(), l_pContext, &uDontCare );		
		}
		ClearPBIAndDecrement( in_pSound, l_pContext, bCalledIncrementPlayCount, in_rPBIParams.pGameObj );

		if( eResult == AK_PartialSuccess )
		{
			if( !in_rPBIParams.bIsFirst )
			{
				switch( eReason )
				{
				case AkMonitorData::NotificationReason_PlayFailedMemoryThreshold:
					eReason = AkMonitorData::NotificationReason_ContinueAbortedMemoryThreshold;
					break;
				case AkMonitorData::NotificationReason_PlayFailedLimit:
					eReason = AkMonitorData::NotificationReason_ContinueAbortedLimit;
					break;
				case AkMonitorData::NotificationReason_PlayFailedGlobalLimit:
					eReason = AkMonitorData::NotificationReason_ContinueAbortedGlobalLimit;
					break;
				default:
					break;
				}
			}
		}
		else
		{
			eReason = in_rPBIParams.bIsFirst ? AkMonitorData::NotificationReason_PlayFailed : AkMonitorData::NotificationReason_ContinueAborted;
			MONITOR_ERROREX( AK::Monitor::ErrorCode_PlayFailed, in_rPBIParams.userParams.PlayingID(), in_rPBIParams.pGameObj->ID(), in_pSound->ID(), false );
		}
		in_pSound->MonitorNotif( 
			eReason,
			in_rPBIParams.pGameObj->ID(),
			in_rPBIParams.userParams,
			in_rPBIParams.playHistory );
	}

	return eResult;
}

//-----------------------------------------------------------------------------
// Name: Play
// Desc: Play a specified sound.
//
// Parameters:
//	CAkPBI * in_pContext				: Pointer to context to play.
//	AkPlaybackState in_ePlaybackState	: Play may be paused.
//-----------------------------------------------------------------------------
AKRESULT CAkURenderer::Play( CAkPBI *		 in_pContext, 
                             TransParams&    in_rTparameters,
							 AkPlaybackState in_ePlaybackState
						    )
{
	in_pContext->_InitPlay();
	
	bool l_bPaused = false;
	// Check if the play command is actually a play-pause.
	if( in_ePlaybackState == PB_Paused )
	{
		l_bPaused = true;
	}

	AKRESULT eResult = in_pContext->_Play( in_rTparameters, l_bPaused );

	if ( eResult == AK_Success )
	{
		EnqueueContext( in_pContext ); // Add PBI context to list.
	}

	return eResult;
} // Play

void CAkURenderer::ProcessCommandAnyNode( ActionParams& in_rAction )
{
	for( AkListCtxs::Iterator iter = m_listCtxs.Begin(); iter != m_listCtxs.End(); ++iter )
    {
		(*iter)->ProcessCommand( in_rAction );
	}
}

//-----------------------------------------------------------------------------
// Name: EnqueueContext
// Desc: Enqueues a PBI that was created elsewhere (the Upper Renderer creates
//       all standalone PBIs, but this is used for linked contexts (Music Renderer)
//
// Parameters:
//	PBI*
//
// Return:
//	AKRESULT
//-----------------------------------------------------------------------------
void CAkURenderer::EnqueueContext( CAkPBI * in_pContext )
{
    AKASSERT( in_pContext );
    m_listCtxs.AddLast( in_pContext );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAkURenderer::DecrementVirtualCount()
{
	AKASSERT( m_uNumVirtualizedSounds != 0 );
	--m_uNumVirtualizedSounds;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CAkURenderer::IncrementDangerousVirtualCount()
{
	if (++m_uNumDangerousVirtualizedSounds >= m_uMaxNumDangerousVirtVoices && m_uMaxNumDangerousVirtVoices)
	{
		MONITOR_ERROR(AK::Monitor::ErrorCode_VirtualVoiceLimit);
	}
}

void CAkURenderer::DecrementDangerousVirtualCount()
{
	AKASSERT(m_uNumDangerousVirtualizedSounds != 0);
	--m_uNumDangerousVirtualizedSounds;
}

// Class created to avoid further bugs associated to the weakest parameters were being separately handled leading to bugs... will prevent this.
class WeakestPBI
{
public:
	WeakestPBI()
		: m_pWeakest( NULL )
		, m_eBelowThresholdBehavior( AkBelowThresholdBehavior_SetAsVirtualVoice )
		, m_fPriority( (AK_MAX_PRIORITY + 1) ) //Max priority(100) + 1 to make it the weakest
	{}

	void Set( CAkPBI* in_pWeakest, AkBelowThresholdBehavior in_eBelowThresholdBehavior, AkReal32 in_fPriority )
	{
		m_pWeakest = in_pWeakest;
		m_eBelowThresholdBehavior = in_eBelowThresholdBehavior;
		m_fPriority = in_fPriority;
	}

	CAkPBI* Get(){ return m_pWeakest; }
	AkBelowThresholdBehavior BelowThresholdBehavior(){ return m_eBelowThresholdBehavior; }
	AkReal32 Priority(){ return m_fPriority; }

	void Reset(){ m_pWeakest = NULL; m_eBelowThresholdBehavior = AkBelowThresholdBehavior_SetAsVirtualVoice; m_fPriority = (AK_MAX_PRIORITY + 1); }

private:
	CAkPBI* m_pWeakest;
	AkBelowThresholdBehavior m_eBelowThresholdBehavior;
	AkReal32 m_fPriority;
};

inline AKRESULT KickEnd(
			AkReal32 in_fPriority,
			WeakestPBI in_Weakest,
			bool in_bKickNewest,
			bool in_bUseVirtualBehavior,
			KickFrom in_eReason,
			CAkParameterNodeBase*& out_pKicked,
			bool in_bIAmHigherPriorityThanAContinueToPlay)
{
	if( in_fPriority < in_Weakest.Priority() || (in_bKickNewest && in_fPriority == in_Weakest.Priority()) )
	{
		in_Weakest.Reset();
	}
	else if ( in_Weakest.Get() )
	{
		out_pKicked = in_Weakest.Get()->GetSound();
		if( in_bUseVirtualBehavior )
		{
			switch( in_Weakest.BelowThresholdBehavior() )
			{
			case AkBelowThresholdBehavior_KillVoice:
				in_Weakest.Get()->Kick( in_eReason );
				break;

			case AkBelowThresholdBehavior_SetAsVirtualVoice:
				break;

			case AkBelowThresholdBehavior_ContinueToPlay:
			default:
				AKASSERT( !"Unhandled below threshold type" );
				break;
			}
		}
		else
		{
			in_Weakest.Get()->Kick( in_eReason );
		}
	}

	if (in_Weakest.Get() == NULL && !in_bIAmHigherPriorityThanAContinueToPlay)
	{
		if( in_bUseVirtualBehavior )
		{
			return AK_MustBeVirtualized;
		}
		else
		{
			return AK_Fail;
		}
	}
	else
		return AK_Success;
}// Kick

//-----------------------------------------------------------------------------
// Name: Kick
// Desc: Asks to kick out the Oldest sound responding to the given IDs.
//
// Parameters:
//	CAkRegisteredObj *	in_pGameObj : GameObject that must match before kicking out
//	AkUniqueID		in_NodeID  : Node to check if the sound comes from ( Excluding OverrideParent Exceptions )
//
// Return - AKRESULT - 
//      AK_SUCCESS				= caller survives.
//      AK_FAIL					= caller should kick himself.
//		AK_MustBeVirtualized	- caller should start as a virtual voice. (if it fails, it must kick itself)
//-----------------------------------------------------------------------------
AKRESULT CAkURenderer::Kick(
							CAkLimiter * in_pLimiter,
							AkUInt16 in_uMaxInstances,
							AkReal32 in_fPriority, 
							CAkRegisteredObj * in_pGameObj, 
							bool in_bKickNewest, 
							bool in_bUseVirtualBehavior, 
							CAkParameterNodeBase*& out_pKicked,
							KickFrom in_eReason
							)
{
	if( in_pLimiter == NULL )
		return AK_Success;

	WeakestPBI l_Weakest;

	AkUInt16 uPlayingCount = 0;
	bool bIAmHigherPriorityThanAContinueToPlay = false;

	// Cycle through PBI list; PBIs are orderer by priority, then by oldest/newest
	CAkLimiter::Iterator itPBI = in_pLimiter->Begin();
	CAkLimiter::Iterator itEnd = in_pLimiter->End();
	for( ; itPBI != itEnd; ++itPBI )
	{
		CAkPBI* pPBI = *itPBI;

		if( in_pGameObj == NULL || pPBI->GetGameObjectPtr() == in_pGameObj )
		{
			if (!pPBI->WasKicked() && !pPBI->VoiceNotLimited())
			{
				// Update the PBI playing count.
				++uPlayingCount;

				AkReal32 pbiPriority = pPBI->GetPriorityFloat();
				if( pbiPriority <= in_fPriority )
				{
					// Remember last PBI, if it can be kicked.
					AkBelowThresholdBehavior virtualBehavior = AkBelowThresholdBehavior_SetAsVirtualVoice;
					if( in_bUseVirtualBehavior )
					{
						AkVirtualQueueBehavior _unused; 
						virtualBehavior = pPBI->GetVirtualBehavior( _unused );
						if( AkBelowThresholdBehavior_ContinueToPlay == virtualBehavior )
						{
							if( uPlayingCount <= in_uMaxInstances)
								bIAmHigherPriorityThanAContinueToPlay = true;
							// Not a candidate to be kicked, continue.
							continue;
						}
					}
					// Found a new weakest, remember it.
					l_Weakest.Set( pPBI, virtualBehavior, pbiPriority );
				}
			}
		}
	}

	// If there aren't enough playing PBIs then there's no reason to kick anyone!
	if( (uPlayingCount + 1) <= in_uMaxInstances )
		return AK_Success;

	return KickEnd( in_fPriority, l_Weakest, in_bKickNewest, in_bUseVirtualBehavior, in_eReason, out_pKicked, bIAmHigherPriorityThanAContinueToPlay);
}// Kick

AKRESULT CAkURenderer::Kick(
	AkReal32 in_fPriority, 
	CAkRegisteredObj * in_pGameObj, 
	bool in_bKickNewest, 
	bool in_bUseVirtualBehavior, 
	CAkParameterNodeBase*& out_pKicked,
	KickFrom in_eReason
	)
{
	WeakestPBI l_Weakest;

	// Cycle through PBI list; PBIs are in order of creation!
	AkListCtxs::Iterator itPBI = m_listCtxs.Begin();
	AkListCtxs::Iterator itEnd = m_listCtxs.End();
	for( ; itPBI != itEnd; ++itPBI )
	{
		CAkPBI* pPBI = *itPBI;

		if( in_pGameObj == NULL || pPBI->GetGameObjectPtr() == in_pGameObj )
		{
			if (!pPBI->WasKicked() && !pPBI->VoiceNotLimited())
			{
				AkReal32 priority = pPBI->GetPriorityFloat();
				if( priority < l_Weakest.Priority()
					|| ( in_bKickNewest && priority == l_Weakest.Priority() )
					)
				{
					AkBelowThresholdBehavior virtualBehavior = AkBelowThresholdBehavior_SetAsVirtualVoice;
					if( in_bUseVirtualBehavior )
					{
						AkVirtualQueueBehavior _unused; 
						virtualBehavior = pPBI->GetVirtualBehavior( _unused );
						if( AkBelowThresholdBehavior_ContinueToPlay == virtualBehavior )
						{
							// Not a candidate to be kicked, continue.
							continue;
						}
					}
					// Found a new weakest, remember it.
					l_Weakest.Set( pPBI, virtualBehavior, priority );
				}
			}
		}
	}

	return KickEnd( in_fPriority, l_Weakest, in_bKickNewest, in_bUseVirtualBehavior, in_eReason, out_pKicked, false );
}// Kick

// Return - bool - true = caller survives | false = caller should kick himself
AKRESULT CAkURenderer::ValidateMaximumNumberVoiceLimit( AkReal32 in_fPriority )
{
	AkUInt32 uCurrent = m_listCtxs.Length() - m_uNumVirtualizedSounds;
	// Ok que faire avec cela...
	// Le nombre de virtuels est difficile a dire, on doit compter ceux qui sont "to be virtualized" dans le tas...

	AkUInt16 uMaxInstances = CAkURenderer::GetGlobalLimiter().GetMaxInstances();
	bool bIsOverLimit = uCurrent + 1 > uMaxInstances; //(+1 since we are about to add one)

	if( bIsOverLimit )
	{
		CAkParameterNodeBase* pKickedUnused = NULL;
		return Kick(
			&CAkURenderer::GetGlobalLimiter(),
			uMaxInstances,
			in_fPriority,
			NULL, 
			true, 
			true, // In this case, we allow virtualization.
			pKickedUnused,
			KickFrom_OverGlobalLimit
			);
	}
	else
	{
		return AK_Success;
	}
}

AKRESULT CAkURenderer::ValidateLimits( AkReal32 in_fPriority, AkMonitorData::NotificationReason& out_eReasonOfFailure )
{
	out_eReasonOfFailure = AkMonitorData::NotificationReason_PlayFailedGlobalLimit; //Setting it anyway, will not be used otherwise.
	return ValidateMaximumNumberVoiceLimit( in_fPriority );
}

void CAkURenderer::EnqueueContextNotif( CAkPBI* in_pPBI, AkCtxState in_eState, AkCtxDestroyReason in_eDestroyReason, AkReal32 in_fEstimatedTime /*= 0*/ )
{
	m_CtxNotifLock.Lock();
	ContextNotif* pCtxNotif = m_CtxNotifQueue.AddLast();

	if( !pCtxNotif )
	{
		// Not enough memory to send the notification. 
		// But we cannot simply drop it since it may cause game inconsistency and leaks.
		// So we will do right now the job we are supposed to be doing after the perform of the lower engine.
		
		PerformContextNotif();

		// once context notifs have been performed, the m_CtxNotifQueue queue will be empty, so the nest addlast will succeed.		
		pCtxNotif = m_CtxNotifQueue.AddLast();

		AKASSERT( pCtxNotif && MIN_NUM_RENDER_EVENT );// Assert, because if it fails that means the minimal size of m_CtxNotifQueue of 64
		// MIN_NUM_RENDER_EVENT just cannot be 0.
	}

	pCtxNotif->pPBI = in_pPBI;
	pCtxNotif->state = in_eState;
	pCtxNotif->DestroyReason = in_eDestroyReason;
	pCtxNotif->fEstimatedLength = in_fEstimatedTime;

	m_CtxNotifLock.Unlock();
}

void CAkURenderer::PerformContextNotif()
{
	m_CtxNotifLock.Lock();
	while( !m_CtxNotifQueue.IsEmpty() )
	{
		ContextNotif& pCtxNotif = m_CtxNotifQueue.First();
		pCtxNotif.pPBI->ProcessContextNotif( pCtxNotif.state, pCtxNotif.DestroyReason, pCtxNotif.fEstimatedLength );
		if( pCtxNotif.state == CtxStateToDestroy )
		{
			m_listCtxs.Remove( pCtxNotif.pPBI );
			DestroyPBI( pCtxNotif.pPBI );
		}
		m_CtxNotifQueue.RemoveFirst();
	}
	m_CtxNotifLock.Unlock();
}

//-----------------------------------------------------------------------------
// Name: DestroyPBI
// Desc: Destroy a specified PBI.
//
// Parameters:
//	CAkPBI * in_pPBI  : Pointer to a PBI to destroy.
//-----------------------------------------------------------------------------
void CAkURenderer::DestroyPBI( CAkPBI * in_pPBI )
{
	CAkLEngineCmds::DequeuePBI( in_pPBI );
	in_pPBI->Term( false );
	AkDelete( AkMemID_Object, in_pPBI );
} // DestroyPBI

void CAkURenderer::DestroyAllPBIs()
{
	while ( CAkPBI * pPBI = m_listCtxs.First() )
	{
		m_listCtxs.RemoveFirst();
		pPBI->_Stop( AkPBIStopMode_Normal, true ); // necessary to avoid infinitely regenerating continuous PBIs
        DestroyPBI( pPBI );
	}
} // DestroyAllPBIs

void CAkURenderer::StopAllPBIs( const CAkUsageSlot* in_pUsageSlot )
{
	for( AkListCtxs::Iterator iter = m_listCtxs.Begin(); iter != m_listCtxs.End(); ++iter )
    {
		CAkPBI* l_pPBI = *iter;
		if( l_pPBI->IsUsingThisSlot( in_pUsageSlot ) )
		{
			l_pPBI->_Stop( TransParams(), true ); // Immediate stop (with min fade-out)

			g_pAudioMgr->StopPendingAction( l_pPBI->GetSound(), NULL, AK_INVALID_PLAYING_ID );	
		}
	}

	CAkLEngine::StopMixBussesUsingThisSlot( in_pUsageSlot );
}

void CAkURenderer::StopAll(ActionParams& in_actionParam)
{
	for (AkListNodes::Iterator iter = m_listActiveNodes.Begin(); iter != m_listActiveNodes.End(); ++iter)
		(*iter)->ExecuteActionNoPropagate(in_actionParam);

	for (AkListCtxs::Iterator iter = m_listCtxs.Begin(); iter != m_listCtxs.End(); ++iter)
		(*iter)->ProcessCommand(in_actionParam);
}


void CAkURenderer::ResetAllEffectsUsingThisMedia( const AkUInt8* in_pOldDataPtr )
{
	for( AkListCtxs::Iterator iter = m_listCtxs.Begin(); iter != m_listCtxs.End(); ++iter )
	{
		CAkPBI* l_pPBI = *iter;
		if( l_pPBI->IsUsingThisSlot( in_pOldDataPtr ) )
		{
			for( int index = 0; index < AK_NUM_EFFECTS_PER_OBJ; ++index )
			{
				l_pPBI->UpdateFx( index );
			}

			// Also update source plugins
			l_pPBI->UpdateSource( in_pOldDataPtr );
		}
	}

	CAkLEngine::ResetAllEffectsUsingThisMedia( in_pOldDataPtr );
}

AkReal32 CAkURenderer::GetMaxRadius( AkGameObjectID in_GameObjId )
{
	AkReal32 MaxDistance = -1.0f;

	CAkRegisteredObj* pGameObj = g_pRegistryMgr->GetObjAndAddref( in_GameObjId );
	if( pGameObj )
	{
		for( AkListCtxs::Iterator iter = m_listCtxs.Begin(); iter != m_listCtxs.End(); ++iter )
		{
			CAkPBI* l_pPBI = *iter;
			if( l_pPBI->GetGameObjectPtr() == pGameObj )
			{
				MaxDistance = AkMax( MaxDistance, l_pPBI->GetMaxDistance() );
			}
		}
		pGameObj->Release();
	}
	return MaxDistance;
}

AKRESULT CAkURenderer::GetMaxRadius( AK::SoundEngine::Query::AkRadiusList & io_RadiusList )
{
	// for performance reasons, we are using an hashList internally.
	typedef AkHashList< AkGameObjectID,AkReal32 > AkGameObjToMaxDst;
	AkGameObjToMaxDst m_localTempHash;

	AKRESULT eResult = AK_Success;

	for( AkListCtxs::Iterator iter = m_listCtxs.Begin(); iter != m_listCtxs.End(); ++iter )
	{
		CAkPBI* l_pPBI = *iter;
		AkGameObjectID gameObjID = l_pPBI->GetGameObjectPtr()->ID();
		
		// Find they key in the array
		AkGameObjToMaxDst::IteratorEx iterMax = m_localTempHash.FindEx( gameObjID );

		if( iterMax != m_localTempHash.End() )
		{
			(*iterMax).item = AkMax( (*iterMax).item, l_pPBI->GetMaxDistance() );
		}
		else
		{
			AkReal32* pMaxDst = m_localTempHash.Set( gameObjID );
			if( !pMaxDst )
			{
				eResult = AK_InsufficientMemory;
				break;
			}
			*pMaxDst = l_pPBI->GetMaxDistance();
		}
	}

	if( eResult == AK_Success )
	{
		// Note : Calling term to "reset" the list
		// So that if the list must be sorted, we have to make sure the list was empty with non sorted items.
		io_RadiusList.Term(); // Just in case the user provided a non empty list.
		// Allocate minimal memory for the array.
		AkUInt32 uArraySize = m_localTempHash.Length();
		eResult = io_RadiusList.Reserve( uArraySize );
		if( eResult == AK_Success )
		{
			// Copy the list over in a format the user will be able to read.
			for( AkGameObjToMaxDst::Iterator iterHash = m_localTempHash.Begin(); iterHash != m_localTempHash.End(); ++iterHash )
			{
				io_RadiusList.AddLast( AK::SoundEngine::Query::GameObjDst( (*iterHash).key, (*iterHash).item ) );
			}
		}
	}

	m_localTempHash.Term();

	return eResult;
}

void CAkURenderer::AddLimiter( CAkLimiter* in_pLimiter )
{
	CAkLimiter** ppLimiter = m_Limiters.AddNoSetKey( in_pLimiter->GetLimiterKey() );
	if ( ppLimiter )
		*ppLimiter = in_pLimiter;
}

void CAkURenderer::RemoveLimiter( CAkLimiter* in_pLimiter )
{
	m_Limiters.Remove( in_pLimiter );
}

void CAkURenderer::ProcessLimiters()
{
	bool bProcessLimiters = m_bLimitersDirty && !m_bBusInvalid;
	if (bProcessLimiters)
	{
		for ( AkListCtxs::Iterator iter = m_listCtxs.Begin(); iter != m_listCtxs.End(); ++iter )
		{
			CAkPBI * l_pPBI = *iter;
			l_pPBI->UpdateLimiters();
			l_pPBI->ForceDevirtualize();
		}

		m_bLimitersDirty = false;
	}
	else
	{
		// Clear all forced virtual flags.
		for (AkListCtxs::Iterator iter = m_listCtxs.Begin(); iter != m_listCtxs.End(); ++iter)
		{
			CAkPBI* l_pPBI = *iter;
			l_pPBI->ForceDevirtualize();
		}
	}

	// Recompute them all.
	for (AkSortedLimiters::Iterator iter = m_Limiters.Begin(); iter != m_Limiters.End(); ++iter)
	{
		CAkLimiter* pLimiter = *iter;
		AKASSERT(pLimiter);	//A NULL limiter should not have been added to the list!
		if (pLimiter)
			pLimiter->UpdateFlags();
	}
}

void CAkURenderer::RecapParamDelta()
{
	for (AkListCtxs::Iterator it = m_listCtxs.Begin(); it != m_listCtxs.End(); ++it)
	{
		(*it)->ForceParametersDirty();		
		(*it)->CalcEffectiveParams();
	}
}