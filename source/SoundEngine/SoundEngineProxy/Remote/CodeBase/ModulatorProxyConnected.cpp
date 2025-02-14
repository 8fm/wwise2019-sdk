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

#include "ModulatorProxyConnected.h"
#include "AkAudioLib.h"
#include "CommandDataSerializer.h"
#include "AkAttenuationMgr.h"
#include "IAttenuationProxy.h"
#include "IModulatorProxy.h"

ModulatorProxyConnected::ModulatorProxyConnected( AkUniqueID in_id, AkModulatorType in_type )
{
	CAkIndexable* pIndexable = AK::SoundEngine::GetIndexable( in_id, AkIdxType_Modulator );
	if ( !pIndexable )
		pIndexable = CAkModulator::Create( in_id, in_type );

	SetIndexable( pIndexable );
}

ModulatorProxyConnected::~ModulatorProxyConnected()
{
}

void ModulatorProxyConnected::HandleExecute( AkUInt16 in_uMethodID, CommandDataSerializer& in_rSerializer, CommandDataSerializer& /*out_rReturnSerializer*/ )
{
	CAkModulator * pModulator = static_cast<CAkModulator *>( GetIndexable() );

	switch( in_uMethodID )
	{
	case IModulatorProxy::MethodSetAkPropF:
		{
			ModulatorProxyCommandData::SetAkPropF setAkProp;
			if( in_rSerializer.Get( setAkProp ) )
				pModulator->SetAkProp( (AkModulatorPropID) setAkProp.m_param1, setAkProp.m_param2, setAkProp.m_param3, setAkProp.m_param4 );
			break;
		}

	case IModulatorProxy::MethodSetAkPropI:
		{
			ModulatorProxyCommandData::SetAkPropI setAkProp;
			if( in_rSerializer.Get( setAkProp ) )
				pModulator->SetAkProp( (AkModulatorPropID) setAkProp.m_param1, setAkProp.m_param2, setAkProp.m_param3, setAkProp.m_param4 );
			break;
		}

	case IModulatorProxy::MethodSetRTPC:
		{
			ModulatorProxyCommandData::SetRTPC setRTPC;
			if( in_rSerializer.Get( setRTPC ) )
				pModulator->SetRTPC( setRTPC.m_rtpcID, setRTPC.m_rtpcType, setRTPC.m_rtpcAccum, setRTPC.m_paramID, setRTPC.m_rtpcCurveID, setRTPC.m_eScaling, setRTPC.m_pArrayConversion, setRTPC.m_ulConversionArraySize );

			break;
		}

	case IModulatorProxy::MethodUnsetRTPC:
		{
			ModulatorProxyCommandData::UnsetRTPC unsetRTPC;
			if( in_rSerializer.Get( unsetRTPC ) )
				pModulator->UnsetRTPC( unsetRTPC.m_paramID, unsetRTPC.m_rtpcCurveID );

			break;
		}

	default:
		AKASSERT( false );
	}
}



ModulatorLfoProxyConnected::ModulatorLfoProxyConnected( AkUniqueID in_id )
: ModulatorProxyConnected( in_id, AkModulatorType_LFO )
{}

ModulatorLfoProxyConnected::~ModulatorLfoProxyConnected()
{}



ModulatorEnvelopeProxyConnected::ModulatorEnvelopeProxyConnected( AkUniqueID in_id )
: ModulatorProxyConnected( in_id, AkModulatorType_Envelope )
{}

ModulatorEnvelopeProxyConnected::~ModulatorEnvelopeProxyConnected()
{}



ModulatorTimeProxyConnected::ModulatorTimeProxyConnected( AkUniqueID in_id )
	: ModulatorProxyConnected( in_id, AkModulatorType_Time )
{}

ModulatorTimeProxyConnected::~ModulatorTimeProxyConnected()
{}


#endif // #ifndef AK_OPTIMIZED
