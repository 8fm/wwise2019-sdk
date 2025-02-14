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

#include <AK/SoundEngine/Common/AkTypes.h>

#include "CommandData.h"

class IMidiDeviceMgrProxy
{
public:
	virtual void AddTarget( AkUniqueID in_idTarget, AkWwiseGameObjectID in_idGameObj ) const = 0;
	virtual void RemoveTarget( AkUniqueID in_idTarget, AkWwiseGameObjectID in_idGameObj ) const = 0;
	virtual void MidiEvent( AkUniqueID in_idTarget, AkWwiseGameObjectID in_idGameObj, AkUInt32 in_uTimestamp, AkUInt32 in_uMidiEvent ) const = 0;

	enum MethodIDs
	{
		MethodAddTarget = 1,
		MethodRemoveTarget,
		MethodMidiEvent,

		LastMethodID
	};
};

namespace MidiDeviceMgrProxyCommandData
{
	struct CommandData : public ProxyCommandData::CommandData
	{
		CommandData();
		CommandData( AkUInt16 in_methodID );

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		DECLARE_BASECLASS( ProxyCommandData::CommandData );
	};

	struct AddTarget : public CommandData
	{
		AddTarget();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUniqueID m_idTarget;
		AkWwiseGameObjectID m_idGameObj;

		DECLARE_BASECLASS( CommandData );
	};

	struct RemoveTarget : public CommandData
	{
		RemoveTarget();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUniqueID m_idTarget;
		AkWwiseGameObjectID m_idGameObj;

		DECLARE_BASECLASS( CommandData );
	};

	struct MidiEvent : public CommandData
	{
		MidiEvent();

		bool Serialize( CommandDataSerializer& in_rSerializer ) const;
		bool Deserialize( CommandDataSerializer& in_rSerializer );

		AkUniqueID m_idTarget;
		AkWwiseGameObjectID m_idGameObj;
		AkUInt32 m_uTimestampMs;
		AkUInt32 m_uMidiEvent;

		DECLARE_BASECLASS( CommandData );
	};
}

#endif // #ifndef AK_OPTIMIZED
