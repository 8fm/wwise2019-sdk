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
// AkMidiBaseCtx.h
//
// Manages the playout context of a midi source.
//
//////////////////////////////////////////////////////////////////////
#ifndef _MIDI_BASE_CTX_H_
#define _MIDI_BASE_CTX_H_

#include <AK/Tools/Common/AkListBare.h>
#include "AkMidiStructs.h"
#include "AkRegisteredObj.h"
#include "PrivateStructures.h" 

class CAkParameterNodeBase;

class CAkMidiBaseMgr;
struct AkMidiFrameEvent;

typedef AkListBare< AkMidiFrameEvent > MidiFrameEventList;

//-------------------------------------------------------------------
// Class handles fetching of MIDI events.
//-------------------------------------------------------------------
class CAkMidiBaseCtx
{
public:

    virtual ~CAkMidiBaseCtx();

	virtual AKRESULT Init();
	virtual AkUniqueID GetTrackID() const {return AK_INVALID_UNIQUE_ID;}

public:

	CAkMidiBaseCtx*	pNextLightItem; // List bare light sibling.

	// CAkCommonBase
	AkUInt32 AddRef();
	AkUInt32 Release();

#ifndef AK_OPTIMIZED
	virtual AkUniqueID GetPlayTargetID( AkUniqueID in_playTargetID ) const = 0;
#endif

	inline CAkRegisteredObj* GetGameObj() { return m_pGameObj; }
	inline AkGameObjectID GetGameObjID() { return m_pGameObj ? m_pGameObj->ID() : AK_INVALID_GAME_OBJECT; }
	inline UserParams& GetUserParams() { return m_UserParams; }
	inline CAkParameterNodeBase* MidiTargetNode() { return m_pRootTargetNode; }
	inline AkUniqueID MidiTargetId() { return m_uRootTargetId; }
	inline bool IsMidiTargetBus() { return m_bIsRootTargetBus; }

	virtual bool GetAbsoluteStop() const { return false; }

#ifndef AK_OPTIMIZED
	virtual void GetMonitoringMuteSoloState( bool &out_bSolo, bool &out_bMute )
	{
		out_bSolo = false;
		out_bMute = false;
	}
#endif

protected:

    CAkMidiBaseCtx(
		CAkRegisteredObj*	in_pGameObj,		// Game object and channel association.
		TransParams&		in_rTransParams,
		UserParams&			in_rUserParams,
		AkUniqueID			in_uidRootTarget
        );

	void CleanupActions();

	void AddMidiEvent( MidiFrameEventList& in_listEvents, const AkMidiEventEx& in_rEvent, const AkUInt32 in_uFrameOffset, const AkUInt32 in_uEventIdx = 0, bool in_bWeakEvent = false, bool in_bFirstNote = false );

	// Returns whether MIDI target was found (or not).
	virtual bool ResolveMidiTarget() = 0;

	CAkRegisteredObj*	m_pGameObj;
	UserParams			m_UserParams;
	TransParams			m_TransParams;

	AkUniqueID				m_uRootTargetId;
	CAkParameterNodeBase*	m_pRootTargetNode;
	bool					m_bIsRootTargetBus;

	virtual CAkMidiBaseMgr* GetMidiMgr() const = 0;

	AkInt32		m_lRef;

	bool		m_bMuted;
};


//-------------------------------------------------------------------
// Struct used to create sorted list of NEW MIDI events.
//-------------------------------------------------------------------
struct AkMidiFrameEvent
{
	AkMidiFrameEvent( CAkMidiBaseCtx* in_pMidiCtx, const AkMidiEventEx& in_rEvent, const AkUInt32 in_uFrameOffset, const AkUInt32 in_uEventIdx, bool in_bWeakEvent, bool in_bFirstNote )
		: m_pMidiCtx( in_pMidiCtx )
		, m_midiEvent( in_rEvent )
		, m_frameOffset( in_uFrameOffset )
		, m_eventIdx( in_uEventIdx )
		, m_bWeakEvent( in_bWeakEvent )
		, m_bFirstNote( in_bFirstNote )
		, pNextItem( NULL )
		{ in_pMidiCtx->AddRef(); }

	~AkMidiFrameEvent()
		{ m_pMidiCtx->Release(); }

	CAkMidiBaseCtx*		m_pMidiCtx;
	AkMidiEventEx		m_midiEvent;
	AkUInt32			m_frameOffset;
	AkUInt32			m_eventIdx:30;
	AkUInt32			m_bWeakEvent:1;
	AkUInt32			m_bFirstNote:1;
	AkMidiFrameEvent*	pNextItem;
};


#endif
