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
// AkMidiDeviceCtx.h
//
// Manages the playout context of a midi source.
//
//////////////////////////////////////////////////////////////////////
#ifndef _MIDI_DEVICE_CTX_H_
#define _MIDI_DEVICE_CTX_H_

#include "AkMidiDeviceMgr.h"
#include "AkMidiBaseCtx.h"

class CAkMidiDeviceMgr;

class CAkMidiDeviceCtx : public CAkMidiBaseCtx
{
    friend class CAkMidiDeviceMgr;

public:

    virtual ~CAkMidiDeviceCtx();

	virtual AKRESULT Init();

	void WwiseEvent( AkMidiEventEx& in_rMidiEvent, AkUInt32 in_uTimestampMs );
	void AddEvent( AkMidiEventEx& in_rMidiEvent, AkUInt32 in_uOffsetTicks );

	void OnFrame( MidiFrameEventList& in_listEvents, AkUInt32 in_uNumSamples );
	void DetachAndRelease();

#ifndef AK_OPTIMIZED
	virtual AkUniqueID GetPlayTargetID( AkUniqueID in_playTargetID ) const { return in_playTargetID; }
#endif

protected:

    CAkMidiDeviceCtx(
		CAkMidiDeviceMgr *	in_pMidiMgr,
		CAkRegisteredObj*	in_pGameObj,		// Game object and channel association.
		TransParams &		in_rTransParams,
		UserParams &		in_rUserParams,
		AkUniqueID			in_uidRootTarget,
		bool				in_bMustAutoRelease = true
        );

	virtual bool ResolveMidiTarget();

	virtual CAkMidiBaseMgr* GetMidiMgr() const;

private:

	struct AkMidiDeviceEvent : public AkMidiEventEx
	{
		AkMidiDeviceEvent( const AkMidiEventEx& in_rEvent )
			: AkMidiEventEx( in_rEvent )
			, uOffset( 0 )
			, pNextItem( NULL )
		{}

		AkUInt32			uOffset; // offset in samples
		AkMidiDeviceEvent*	pNextItem;
	};
	typedef AkListBare< AkMidiDeviceEvent > DeviceEventList;

	CAkMidiDeviceMgr*	m_pMidiMgr;

	DeviceEventList	m_listDeviceEvents;

	AkUInt32	m_bWasStopped :1;
	AkUInt32	m_bMustAutoRelease :1;
	AkUInt32	m_bDidAutoRelease :1;
};

#endif
