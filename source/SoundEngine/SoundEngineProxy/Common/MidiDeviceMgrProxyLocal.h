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
#pragma once

#ifndef AK_OPTIMIZED

#include "IMidiDeviceMgrProxy.h"

class MidiDeviceMgrProxyLocal : public IMidiDeviceMgrProxy
{
public:
	MidiDeviceMgrProxyLocal();
	virtual ~MidiDeviceMgrProxyLocal();

	// IMidiDeviceMgrProxy members
	virtual void AddTarget( AkUniqueID in_idTarget, AkWwiseGameObjectID in_idGameObj ) const;
	virtual void RemoveTarget( AkUniqueID in_idTarget, AkWwiseGameObjectID in_idGameObj ) const;
	virtual void MidiEvent( AkUniqueID in_idTarget, AkWwiseGameObjectID in_idGameObj, AkUInt32 in_uTimestamp, AkUInt32 in_uMidiEvent ) const;

private:
};
#endif // #ifndef AK_OPTIMIZED
