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
// AkSoundBase.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _AKSOUND_BASE_H_
#define _AKSOUND_BASE_H_

#include "AkParameterNode.h"

class CAkSoundBase : public CAkParameterNode
{
public:

	void ProcessCommand( ActionParams& in_rAction );
	virtual void PlayToEnd( CAkRegisteredObj * in_pGameObj, CAkParameterNodeBase* in_pNodePtr, AkPlayingID in_PlayingID = AK_INVALID_PLAYING_ID );

	virtual void ParamNotification( NotifParams& in_rParams );

	// Notify the children PBI that a mute variation occured
	virtual void MuteNotification(
		AkReal32		in_fMuteRatio,		// New muting ratio
		AkMutedMapItem& in_rMutedItem,		// Instigator's unique key
		bool			in_bIsFromBus = false
		);

	// Notify the children PBI that a mute variation occured
	virtual void MuteNotification(
		AkReal32 in_fMuteRatio,				// New muting ratio
		CAkRegisteredObj *	in_pGameObj,	// Target Game Object
		AkMutedMapItem& in_rMutedItem,		// Instigator's unique key
		bool in_bPrioritizeGameObjectSpecificItems
		);

	virtual void ForAllPBI( 
		AkForAllPBIFunc in_funcForAll,
		const AkRTPCKey & in_rtpcKey, // If key field is invalid, target PBIs regardless of field
		void * in_pCookie );

	// Notify the children PBI that a change int the Positioning parameters occured from RTPC
	virtual void PropagatePositioningNotification(
		AkReal32			in_RTPCValue,	// 
		AkRTPC_ParameterID	in_ParameterID,	// RTPC ParameterID, must be a Positioning ID.
		CAkRegisteredObj *		in_GameObj,		// Target Game Object
		AkRTPCExceptionChecker * in_pExceptCheck = NULL
		);

	// Notify the children PBI that a major change occurred and that the 
	// Parameters must be recalculated
	virtual void RecalcNotification( bool in_bLiveEdit, bool in_bLog = false );

	virtual void ClearLimiters();

	virtual void NotifyBypass(
		AkUInt32 in_bitsFXBypass,
		AkUInt32 in_uTargetMask,
		CAkRegisteredObj * in_pGameObj,
		AkRTPCExceptionChecker* in_pExceptCheck = NULL
		);

	virtual void UpdateFx(
		AkUInt32	   	in_uFXIndex
		);

#ifndef AK_OPTIMIZED
	virtual void InvalidatePositioning();
#endif
	virtual bool IsInfiniteLooping( CAkPBIAware* in_pInstigator );
	AkInt16 Loop();
	AkReal32 LoopStartOffset() const;
	AkReal32 LoopEndOffset() const;
	AkReal32 LoopCrossfadeDuration() const;

	void LoopCrossfadeCurveShape( AkCurveInterpolation& out_eCrossfadeUpType,  AkCurveInterpolation& out_eCrossfadeDownType) const;

	void GetTrim( AkReal32& out_fBeginTrimOffsetSec, AkReal32& out_fEndTrimOffsetSec ) const;
	void GetFade(	AkReal32& out_fBeginFadeOffsetSec, AkCurveInterpolation& out_eBeginFadeCurveType, 
					AkReal32& out_fEndFadeOffsetSec, AkCurveInterpolation& out_eEndFadeCurveType  ) const;

	void MonitorNotif(AkMonitorData::NotificationReason in_eNotifReason, AkGameObjectID in_GameObjID, UserParams& in_rUserParams, PlayHistory& in_rPlayHistory);

protected:
	CAkSoundBase( AkUniqueID in_ulID );
	virtual ~CAkSoundBase();
};

#endif //_AKSOUND_BASE_H_
