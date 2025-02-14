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
// AkPlayingMgr.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _PLAYING_MGR_H_
#define _PLAYING_MGR_H_

#include <AK/Tools/Common/AkHashList.h>
#include <AK/Tools/Common/AkArray.h>
#include <AK/SoundEngine/Common/AkCallback.h>
#include "AkManualEvent.h"
#include "PrivateStructures.h"
#include "AkBusCallbackMgr.h"
#include "AkScopedRtpcObj.h"
#include "AkMidiStructs.h"
#include "AkMarkerStructs.h"

class CAkPBIAware;
class CAkTransportAware;
struct AkQueuedMsg_EventBase;

const AkUInt32 AK_WWISE_MARKER_NOTIFICATION_FLAGS = AK_Marker | AK_MusicSyncUserCue;

class CAkPlayingMgr
{
public:
	AKRESULT Init();
	void Term();

	// Add a playing ID in the "Under observation" event list
	AKRESULT AddPlayingID( 
		AkQueuedMsg_EventBase & in_event,
		AkCallbackFunc in_pfnCallback,
		void * in_pCookie,
		AkUInt32 in_uiRegisteredNotif,
		AkUniqueID in_id
		);

	// Ask to not get notified for the given cookie anymore.
	void CancelCallbackCookie( void* in_pCookie );

	// Ask to not get notified for the given game object anymore.
	void CancelCallbackGameObject( AkGameObjectID in_gameObjectID );

	// Ask to not get notified for the given Playing ID anymore.
	void CancelCallback( AkPlayingID in_playingID );

	// Set the actual referenced PBI
	AKRESULT SetPBI(
		AkPlayingID in_PlayingID,	// Playing ID
		CAkTransportAware* in_pPBI,	// PBI pointer, only to be stored
		AkUInt32 * out_puRegisteredNotif // out param: registered notifications
		);

	void Remove(
		AkPlayingID in_PlayingID,	// Playing ID
		CAkTransportAware* in_pPBI	// PBI pointer
		);

	bool IsActive(
		AkPlayingID in_PlayingID // Playing ID
		);

	void AddItemActiveCount(
		AkPlayingID in_PlayingID // Playing ID
		);

	void RemoveItemActiveCount(
		AkPlayingID in_PlayingID // Playing ID
		);

	AkUniqueID GetEventIDFromPlayingID( AkPlayingID in_playingID );

	AkGameObjectID GetGameObjectFromPlayingID( AkPlayingID in_playingID );

	AKRESULT GetPlayingIDsFromGameObject(
		AkGameObjectID in_GameObjId,
		AkUInt32& io_ruNumIDs,
		AkPlayingID* out_aPlayingIDs
		);

	void * GetCookie(
		AkPlayingID in_PlayingID // Playing ID
		);

#ifndef AK_OPTIMIZED
	void StopAndContinue(
		AkPlayingID			in_PlayingID,
		CAkRegisteredObj *		in_pGameObj,
		CAkContinueListItem&in_ContinueListItem,
		AkUniqueID			in_ItemToPlay,
		AkUInt16			in_wPosition,
        CAkPBIAware*        in_pInstigator
		);
#endif

	void NotifyEndOfDynamicSequenceItem(
		AkPlayingID in_PlayingID,
		AkUniqueID in_itemID,
		void* in_pCustomInfo
		);

	void NotifyMarker(
		CAkPBI* in_pPBI,					 // PBI pointer
		struct AkAudioMarker* in_pMarkerInfo // Marker being notified
		);

	void NotifyDuration(
		AkPlayingID in_PlayingID,
		AkReal32 in_fDuration,
		AkReal32 in_fEstimatedDuration,
		AkUniqueID in_idAudioNode,
		AkUniqueID in_idMedia,
		bool in_bStreaming
		);
		
	void NotifyMusicPlayStarted( AkPlayingID in_PlayingID );

	// io_uSelection and io_uItemDone must contain valid values before function call.
	void MusicPlaylistCallback( AkPlayingID in_PlayingID, AkUniqueID in_playlistID, AkUInt32 in_uNumPlaylistItems, AkUInt32& io_uSelection, AkUInt32& io_uItemDone );

	void NotifySpeakerVolumeMatrix(
		AkPlayingID in_PlayingID,
		AkSpeakerVolumeMatrixCallbackInfo* in_pInfo
		);

	void NotifyMarkers(AkMarkerNotification * pMarkers, AkUInt32 uNumMarkers);

	void NotifyMusic(AkPlayingID in_PlayingID, AkCallbackType in_NotifType, const AkSegmentInfo & in_segmentInfo);
	void NotifyMusicUserCues(AkPlayingID in_PlayingID, const AkSegmentInfo & in_segmentInfo, char * in_pszUserCueName);
	
	void NotifyStarvation( AkPlayingID in_PlayingID, AkUniqueID in_idAudioNode );

	void NotifyMidiEvent( AkPlayingID in_PlayingID, const AkMidiEventEx& in_midiEvent );

	inline unsigned int NumPlayingIDs() const { return m_PlayingMap.Length(); }

	bool AddedNewRtpcValue( AkPlayingID in_PlayingID, AkRtpcID in_rtpcId );
	bool AddedNewModulatorCtx( AkPlayingID in_PlayingID, AkRtpcID in_rtpcId );
private:
	
	struct PlayingMgrItem: public CAkScopedRtpcObj
	{
		// Hold a list of PBI with Wwise, only keep count otherwise
	#ifndef AK_OPTIMIZED
		typedef AkArray<CAkTransportAware*, CAkTransportAware*> AkPBIList;
		AkPBIList m_PBIList;
	#else
		int cPBI;
	#endif
	
		int cAction; // count of actions
	
		AkUniqueID eventID;
		AkGameObjectID GameObj;
		UserParams userParam;
		AkCallbackFunc pfnCallback;
		void* pCookie;
		AkUInt32 uiRegisteredNotif;
		PlayingMgrItem* pNextItem;
	};

	struct PlayingMgrItemPolicy
	{
		static AkPlayingID Key(const PlayingMgrItem* in_pItem) {return in_pItem->userParam.PlayingID();}
	};

	void CheckRemovePlayingID( AkPlayingID in_PlayingID, PlayingMgrItem* in_pItem );

	void PrepareMusicNotification(AkPlayingID in_PlayingID, PlayingMgrItem* in_pItem, AkCallbackType in_NotifType, const AkSegmentInfo & in_segmentInfo, char * in_pszUserCueName, AkMusicSyncCallbackInfo & out_info);

	typedef AkHashListBare<AkPlayingID, PlayingMgrItem, ArrayPoolDefault, PlayingMgrItemPolicy> AkPlayingMap;
	AkPlayingMap m_PlayingMap;
	CAkLock m_csMapLock;
	AkManualEvent m_CallbackEvent;

	CAkBusCallbackMgr m_BusCallbackMgr;
};

extern CAkPlayingMgr*			g_pPlayingMgr;
extern CAkBusCallbackMgr*		g_pBusCallbackMgr;

#endif
