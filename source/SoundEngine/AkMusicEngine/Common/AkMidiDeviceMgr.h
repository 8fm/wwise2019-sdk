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
// AkMidiDeviceMgr.h
//
// Base context class for all parent contexts.
// Propagates commands to its children. Implements child removal.
//
// NOTE: Only music contexts are parent contexts, so this class is
// defined here. Move to AkAudioEngine if other standard nodes use them.
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_MIDI_DEVICE_MGR_H_
#define _AK_MIDI_DEVICE_MGR_H_

#include <AK/Tools/Common/AkListBare.h>
#include <AK/Tools/Common/AkArray.h>
#include <AK/MusicEngine/Common/AkMusicEngine.h>
#include <AK/SoundEngine/Common/IAkPlugin.h>
#include "AkMidiBaseMgr.h"
#include "AkMidiDeviceCtx.h"

class CAkLock;

//-------------------------------------------------------------------
// Class CAkMidiDeviceMgr: Singleton. Manages top-level music contexts,
// routes game actions to music contexts, acts as an external Behavioral 
// Extension and State Handler, and as a music PBI factory.
//-------------------------------------------------------------------
class CAkMidiDeviceMgr AK_FINAL : public CAkMidiBaseMgr
{
public:

    ~CAkMidiDeviceMgr();

	static CAkMidiDeviceMgr * Create();
    inline static CAkMidiDeviceMgr * Get() { return m_pMidiMgr; }
    void Destroy();

	static void GlobalCallback(AK::IAkGlobalPluginContext *, AkGlobalCallbackLocation, void *);

	CAkMidiDeviceCtx* AddTarget( AkUniqueID in_idTarget, AkGameObjectID in_idGameObj );
	void RemoveTarget( AkUniqueID in_idTarget, AkGameObjectID in_idGameObj );

	void WwiseEvent( AkUniqueID in_idTarget, AkGameObjectID in_idGameObj, AkMidiEventEx in_MidiEvent, AkUInt32 in_uTimestampMs );
	void PostEvents( AkUniqueID in_idTarget, AkGameObjectID in_idGameObj, AkMIDIPost* in_pPosts, AkUInt32 in_uNumPosts );
	void StopAll( AkUniqueID in_idTarget, AkGameObjectID in_idGameObj );

	void NextFrame( AkUInt32 in_uNumSamples );

protected:

	virtual void UpdateOnFirstNote( MidiFrameEventList& , AkMidiFrameEvent* ) { AKASSERT( false ); }

private:

    CAkMidiDeviceMgr();

	void DestroyCtx();
	CAkMidiDeviceCtx* AddCtx( AkUniqueID in_idTarget, CAkRegisteredObj* in_pGameObj, bool in_bMustAutoRelease = true );
	CAkMidiDeviceCtx* GetCtx( AkUniqueID in_idTarget, CAkRegisteredObj* in_pGameObj );

private:

    static CAkMidiDeviceMgr * m_pMidiMgr;
};

#endif
