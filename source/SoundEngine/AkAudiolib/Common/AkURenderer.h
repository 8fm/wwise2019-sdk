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
// AkURenderer.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _UPPER_RENDERER_H_
#define _UPPER_RENDERER_H_

#include "ActivityChunk.h"
#include "PrivateStructures.h"
#include "AkList2.h"
#include <AK/Tools/Common/AkListBare.h>
#include <AK/SoundEngine/Common/AkQueryParameters.h>

//-----------------------------------------------------------------------------
// CAkURenderer class.
//-----------------------------------------------------------------------------
class CAkURenderer
{
public:
    // Initialise the renderer
    //
    // Return - AKRESULT - AK_Success: everything succeed
    //                     AK_Fail: Was not able to initialize the renderer
    static AKRESULT Init();

    // Uninit the renderer
    static void Term();

	// Call a play on the definition directly
    //
    // Return - AKRESULT - Ak_Success if succeeded
	static AKRESULT Play(  CAkSoundBase*    in_pSound,          // Node instanciating the play
						   CAkSource*		in_pSource,			// Source
		                   AkPBIParams&     in_rPBIParams );

	static AKRESULT Play(	CAkPBI *		in_pContext, 
                    TransParams&    in_rTparameters,
					AkPlaybackState	in_ePlaybackState
					);

	static void StopAllPBIs( const CAkUsageSlot* in_pUsageSlot );
	static void StopAll(ActionParams& in_actionParam);
	static void ResetAllEffectsUsingThisMedia( const AkUInt8* in_pOldDataPtr);

    static void EnqueueContext( CAkPBI * in_pContext );

	// Should only be called by CAkParameterNodeBase
	static void AddToActiveNodes(CAkParameterNodeBase * in_pNode);
	static void RemoveFromActiveNodes(CAkParameterNodeBase * in_pNode);

	static AkForceInline void IncrementVirtualCount() { ++m_uNumVirtualizedSounds; }
	static void DecrementVirtualCount();

	static void IncrementDangerousVirtualCount();
	static void DecrementDangerousVirtualCount();

	// Asks to kick out the Oldest sound responding to the given IDs.
	// Theorically called to apply the Max num of instances per Node.
    //
    // Return - AKRESULT - 
	//      AK_SUCCESS				= caller survives.
	//      AK_FAIL					= caller should kick himself.
	//		AK_MustBeVirtualized	- caller should start as a virtual voice. (if it fails, it must kick itself)
	static AKRESULT Kick( CAkLimiter * in_pLimiter, AkUInt16 in_uMaxInstances, AkReal32 in_fPriority, CAkRegisteredObj * in_pGameObj, bool in_bKickNewest, bool in_bUseVirtualBehavior, CAkParameterNodeBase*& out_pKicked, KickFrom in_eReason );
	static AKRESULT Kick( AkReal32 in_fPriority, CAkRegisteredObj * in_pGameObj, bool in_bKickNewest, bool in_bUseVirtualBehavior, CAkParameterNodeBase*& out_pKicked, KickFrom in_eReason );

	// Return - AKRESULT - 
	//      AK_SUCCESS				= caller survives.
	//      AK_FAIL					= caller should kick himself.
	//		AK_MustBeVirtualized	- caller should start as a virtual voice. (if it fails, it must kick itself)
	static AKRESULT ValidateMaximumNumberVoiceLimit( AkReal32 in_fPriority );

	static AKRESULT ValidateLimits( AkReal32 in_fPriority, AkMonitorData::NotificationReason& out_eReasonOfFailure );

	static bool GetVirtualBehaviorAction(CAkSoundBase* in_pSound, CAkPBIAware* in_pInstigator, AkBelowThresholdBehavior& out_eBelowThresholdBehavior);
	static AKRESULT IncrementPlayCountAndInit(CAkSoundBase* in_pSound, CAkRegisteredObj * in_pGameObj, AkReal32 in_fPriority, AKRESULT in_eValidateLimitsResult, bool in_bAllowedToPlayIfUnderThreshold, AkMonitorData::NotificationReason& io_eReason, CAkPBI* in_pPBI, bool in_bAllowKick);
	static void ClearPBIAndDecrement( CAkSoundBase* in_pSound, CAkPBI*& io_pPBI, bool in_bCalledIncrementPlayCount, CAkRegisteredObj * in_pGameObj );

	struct ContextNotif
	{
		CAkPBI* pPBI;
		AkCtxState state;
		AkCtxDestroyReason DestroyReason;
		AkReal32 fEstimatedLength;
	};

	static void	EnqueueContextNotif( CAkPBI* in_pPBI, AkCtxState in_State, AkCtxDestroyReason in_eDestroyReason = CtxDestroyReasonFinished, AkReal32 in_fEstimatedTime = 0.0f );

	typedef CAkList2<ContextNotif, const ContextNotif&, AkAllocAndFree> AkContextNotifQueue;
	typedef AkListBare<CAkPBI,AkListBareNextItem,AkCountPolicyWithCount> AkListCtxs;
	typedef AkListBare<CAkParameterNodeBase, AkListBareNextItem, AkCountPolicyWithCount> AkListNodes;

	static void PerformContextNotif();

	static PriorityInfoCurrent _CalcInitialPriority( CAkSoundBase * in_pSound, CAkRegisteredObj * in_pGameObj, AkReal32& out_fMaxRadius );
	static AkReal32 GetMinDistance(const AkSoundPositionRef& in_rPosRef, const AkListenerSet& in_listeners);

	static AkReal32 GetMaxRadius( AkGameObjectID in_GameObjId );

	static AKRESULT GetMaxRadius( AK::SoundEngine::Query::AkRadiusList & io_RadiusList );

	static void AddLimiter( CAkLimiter* in_pLimiter );
	static void RemoveLimiter( CAkLimiter* in_pLimiter );

	static void ProcessLimiters();

	static AkForceInline CAkLimiter& GetGlobalLimiter(){ return m_GlobalLimiter; }

	static AkForceInline void SetMaxNumDangerousVirtVoices( AkUInt16 in_uMaxNumVoices ) { m_uMaxNumDangerousVirtVoices = in_uMaxNumVoices; }

	static void	ProcessCommandAnyNode( ActionParams& in_rAction );

	static void RecapParamDelta();
	
	static bool m_bLimitersDirty;
	static bool m_bBusInvalid;

private:
	static void	DestroyPBI( CAkPBI * in_pPBI );
	static void	DestroyAllPBIs();

private:	 
	static AkListCtxs				m_listCtxs;					// List of PBIs/Contexts.	 
	static AkListNodes				m_listActiveNodes;
	static AkContextNotifQueue		m_CtxNotifQueue;
	static CAkLock					m_CtxNotifLock;
	static AkUInt32					m_uNumVirtualizedSounds;
	static AkUInt32					m_uNumDangerousVirtualizedSounds;
	static AkUInt32					m_uMaxNumDangerousVirtVoices;

	typedef AkSortedKeyArray<AkLimiterKey, CAkLimiter*, ArrayPoolDefault, CAkLimiter::AkSortedLimiterGetKey> AkSortedLimiters;
	static AkSortedLimiters m_Limiters;
	static CAkLimiter m_GlobalLimiter;
};

#endif //#define _UPPER_RENDERER_H_
