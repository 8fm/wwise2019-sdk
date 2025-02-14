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

#include "stdafx.h"
#if !defined(AK_OPTIMIZED) && defined(WWISE_AUTHORING)



#include "MidiDeviceMgrProxyLocal.h"

#include "AkAudioLib.h"
#include "AkCritical.h"
#include "AkMidiDeviceMgr.h"
#include "AkMidiStructs.h"


MidiDeviceMgrProxyLocal::MidiDeviceMgrProxyLocal()
{
}

MidiDeviceMgrProxyLocal::~MidiDeviceMgrProxyLocal()
{
}

void MidiDeviceMgrProxyLocal::AddTarget( AkUniqueID in_idTarget, AkWwiseGameObjectID in_idGameObj ) const
{
	CAkFunctionCritical SpaceSetAsCritical;
	CAkMidiDeviceMgr* pMgr = CAkMidiDeviceMgr::Get();
	if ( pMgr )
		pMgr->AddTarget( in_idTarget, (AkGameObjectID)in_idGameObj );
}

void MidiDeviceMgrProxyLocal::RemoveTarget( AkUniqueID in_idTarget, AkWwiseGameObjectID in_idGameObj ) const
{
	CAkFunctionCritical SpaceSetAsCritical;
	CAkMidiDeviceMgr* pMgr = CAkMidiDeviceMgr::Get();
	if ( pMgr )
		pMgr->RemoveTarget( in_idTarget, (AkGameObjectID)in_idGameObj );
}

void MidiDeviceMgrProxyLocal::MidiEvent( AkUniqueID in_idTarget, AkWwiseGameObjectID in_idGameObj, AkUInt32 in_uTimestamp, AkUInt32 in_uMidiEvent ) const
{
	AkMidiEventEx midiEvent;
	midiEvent.byType = AK_MIDI_EVENT_GET_TYPE( in_uMidiEvent );
	midiEvent.byChan = AK_MIDI_EVENT_GET_CHANNEL( in_uMidiEvent );
	midiEvent.Gen.byParam1 = AK_MIDI_EVENT_GET_PARAM1( in_uMidiEvent );
	midiEvent.Gen.byParam2 = AK_MIDI_EVENT_GET_PARAM2( in_uMidiEvent );

	CAkFunctionCritical SpaceSetAsCritical;
	CAkMidiDeviceMgr* pMgr = CAkMidiDeviceMgr::Get();
	if ( pMgr )
		pMgr->WwiseEvent( in_idTarget, (AkGameObjectID)in_idGameObj, midiEvent, in_uTimestamp );
}

#endif // !defined(AK_OPTIMIZED) && defined(WWISE_AUTHORING)
