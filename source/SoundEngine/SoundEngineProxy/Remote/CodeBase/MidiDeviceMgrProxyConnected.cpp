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
#ifndef AK_OPTIMIZED

#include "MidiDeviceMgrProxyConnected.h"
#include "CommandDataSerializer.h"

void MidiDeviceMgrProxyConnected::HandleExecute( CommandDataSerializer& in_rSerializer, CommandDataSerializer& /*out_rReturnSerializer*/ )
{
	MidiDeviceMgrProxyCommandData::CommandData cmdData;

	{
		CommandDataSerializer::AutoSetDataPeeking peekGate( in_rSerializer );
		in_rSerializer.Get( cmdData );
	}

	switch( cmdData.m_methodID )
	{
	case IMidiDeviceMgrProxy::MethodAddTarget:
		{
			MidiDeviceMgrProxyCommandData::AddTarget addTarget;
			in_rSerializer.Get( addTarget );

			//if( in_rSerializer.Get( addTarget ) )
			//	m_localProxy.AddTarget( addTarget.m_idTarget, addTarget.m_idGameObj );

			break;
		}
	case IMidiDeviceMgrProxy::MethodRemoveTarget:
		{
			MidiDeviceMgrProxyCommandData::RemoveTarget removeTarget;
			in_rSerializer.Get( removeTarget );

			//if( in_rSerializer.Get( removeTarget ) )
			//	m_localProxy.RemoveTarget( removeTarget.m_idTarget );

			break;
		}
	case IMidiDeviceMgrProxy::MethodMidiEvent:
		{
			MidiDeviceMgrProxyCommandData::MidiEvent midiEvent;
			in_rSerializer.Get( midiEvent );

			//if( in_rSerializer.Get( midiEvent ) )
			//	m_localProxy.MidiEvent( midiEvent.m_idTarget, midiEvent.m_uTimestampMs, midiEvent.m_uMidiEvent );

			break;
		}
	default:
		AKASSERT( !"Unsupported command." );
	}
	in_rSerializer.Reset();
}
#endif // #ifndef AK_OPTIMIZED
