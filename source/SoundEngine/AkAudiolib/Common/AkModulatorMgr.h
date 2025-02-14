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
// AkModulatorMgr.h
//
// creator : nharris
//
//////////////////////////////////////////////////////////////////////

#ifndef _MODULATOR_MGR_H_
#define _MODULATOR_MGR_H_

#include "AkRTPCMgr.h"
#include "AkIndexable.h"

class CAkPBI;
class CAkParameterNodeBase;
struct AkModulatorParamXfrm;

class CAkRegisteredObj;
class CAkModulatorEngine;
class CAkModulatorData;
class CAkMidiNoteState;
class CAkModulator;

struct AkModulatorSubscriberInfo
{
	AkModulatorSubscriberInfo(const void* in_pSubscriber, CAkRTPCMgr::SubscriberType in_eSubscriberType) : pSubscriber(in_pSubscriber)
		, pTargetNode(NULL)
		, eSubscriberType(in_eSubscriberType)
		, eNarrowestSupportedScope(AkModulatorScope_Voice /* ie no restrictions on scope*/)
	{}

	AkModulatorSubscriberInfo() : pSubscriber(NULL)
		, pTargetNode(NULL)
		, eSubscriberType(CAkRTPCMgr::SubscriberType_Plugin)
		, eNarrowestSupportedScope(AkModulatorScope_Voice /* ie no restrictions on scope*/)
	{}

	const void*					pSubscriber;
	CAkParameterNodeBase*		pTargetNode;
	CAkRTPCMgr::SubscriberType	eSubscriberType;
	AkModulatorScope			eNarrowestSupportedScope;
};

struct AkModulatorTriggerParams
{
	enum TriggerMode
	{
		TriggerMode_SubsequentPlay = 0, // was triggered in a subsequent play call of a container or event, ie not the first
		TriggerMode_FirstPlay,			// was triggered in the first play call of a container or event.
		TriggerMode_ParameterUpdated,	// play was not called, but the modulator changed mid-sound and may need to be retriggered.
		TriggerMode_EndOfNoteOn			// trigger modulators for sounds played on note-on, at reception of note-off.
	};

	AkModulatorTriggerParams(): pGameObj(NULL)
		,midiEvent()
		,pMidiNoteState(NULL)
		,midiTargetId(AK_INVALID_UNIQUE_ID)
		,playingId(AK_INVALID_PLAYING_ID)
		,uFrameOffset(0)
		,eTriggerMode(TriggerMode_FirstPlay)
		,pPbi(NULL)
	{}

	CAkRegisteredObj *			pGameObj;
	AkMidiEventEx				midiEvent;
	CAkMidiNoteState*			pMidiNoteState;
	AkUniqueID					midiTargetId;
	AkPlayingID					playingId;
	AkUInt32					uFrameOffset;
	TriggerMode					eTriggerMode;
	CAkPBI*						pPbi;

	// Helper function to fill out all the fields in a AkRTPCKey for AkModulatorTriggerParams.
	AkRTPCKey GetRTPCKey() const;
};

class AkModulatorArray : public AkArray < CAkModulator*, CAkModulator*, ArrayPoolDefault >
{
public:
	AkUIntPtr key;
	AkModulatorArray* pNextItem;
};

struct AkModulatorToTrigger
{
	AkModulatorToTrigger(const AkModulatorSubscriberInfo& in_info, AkModulatorArray* in_pModulators) : subscriberInfo(in_info), pModulators(in_pModulators) {}
	AkModulatorToTrigger() : pModulators(NULL) {}

	AkModulatorSubscriberInfo	subscriberInfo;
	AkModulatorArray*			pModulators;
};

typedef AkArray< AkModulatorToTrigger, const AkModulatorToTrigger&, AkHybridAllocator< sizeof( AkModulatorToTrigger ) * 8 > > AkModulatorsToTrigger;


class CAkModulatorMgr
{
public:

	CAkModulatorMgr();
	~CAkModulatorMgr();

	AKRESULT Init();
	void Term();

	void GetModulators(const AkModulatorSubscriberInfo& in_info, AkModulatorsToTrigger& out_modulators);

	AKRESULT Trigger(const AkModulatorsToTrigger& in_modulators, const AkModulatorTriggerParams& in_params, CAkModulatorData* io_pModPBIData);
	AKRESULT Trigger(const AkModulatorSubscriberInfo& in_info, const AkModulatorTriggerParams& in_params, CAkModulatorData* io_pModPBIData);

	AKRESULT AddSubscription( AkRtpcID in_rtpcID, CAkRTPCMgr::AkRTPCSubscription * in_pSubscription );

	//Removes all subscriptions, or only the subscriptions to a particular modulator.
	void RemoveSubscription( CAkRTPCMgr::AkRTPCSubscription * in_pSubscription, AkRtpcID in_RTPCid = AK_INVALID_UNIQUE_ID );

	//Retrieve the current modulator output value for the game object and note/channel pair.
	bool GetCurrentModulatorValue(
		AkRtpcID in_RTPC_ID,
		AkRTPCKey& io_rtpcKey,
		AkReal32& out_value
		);

	//Does the associated modulator support processing of automated properties.
	bool SupportsAutomatedParams(
		AkRtpcID in_RTPC_ID
		);

	//Get the conversion transform for an audio rate parameter
	//	returns true if the call succeeds, and "in_pSubscriber" is subscribed to "in_modulatorID" for modulating "in_ParamID".
	//	returns false otherwise and "out_paramXfrm" is not modified.
	bool GetParamXfrm( 
		const void* in_pSubscriber, 
		AkRTPC_ParameterID in_ParamID, 
		AkRtpcID in_modulatorID, 
		const CAkRegisteredObj* in_GameObj,
		AkModulatorParamXfrm& out_paramXfrm
		);

	void ProcessModulators();

	void TermModulatorEngine();

	void RemovedScopedRtpcObj( AkRtpcID in_RTPCid, const AkRTPCKey& in_rtpcKey );

	void CleanUpFinishedCtxs();

private:

	//Maps a modulator ID to a modulator.
	typedef AkHashListBare< AkRtpcID, CAkIndexable > AkMapModulatorRecords;

	//Maps a void* pointer-to-subscriber to an array of modulators that the subscriber subscribes to.
	//	The type that the pointer points to is different depending on the subscription type, as it is in CAkRTPCMgr.
	//	In our case we are just using the pointer as a key anyways.
	//Used to identify which modulators need to be triggered, given a parameter node.
	typedef AkHashListBare< AkUIntPtr, AkModulatorArray > AkMapModulatorSubscribers;
	AkMapModulatorSubscribers m_ModulatorSubscribers;

	CAkModulatorEngine* m_pEngine;
};

extern CAkModulatorMgr* g_pModulatorMgr;


#endif
