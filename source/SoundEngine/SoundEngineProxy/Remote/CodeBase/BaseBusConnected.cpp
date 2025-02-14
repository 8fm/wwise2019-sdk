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

#include "BaseBusConnected.h"

#include "AkBus.h"
#include "CommandDataSerializer.h"
#include "IBusProxy.h"

BaseBusConnected::BaseBusConnected()
{
}

BaseBusConnected::~BaseBusConnected()
{
}

void BaseBusConnected::HandleExecute( AkUInt16 in_uMethodID, CommandDataSerializer& in_rSerializer, CommandDataSerializer& out_rReturnSerializer )
{
	CAkBus * pBus = static_cast<CAkBus *>( GetIndexable() );

	switch( in_uMethodID )
	{
	case IBusProxy::MethodSetMaxNumInstancesOverrideParent:
		{
			BusProxyCommandData::SetMaxNumInstancesOverrideParent setMaxNumInstancesOverrideParent;
			if( in_rSerializer.Get( setMaxNumInstancesOverrideParent ) )
				pBus->SetMaxNumInstIgnoreParent( setMaxNumInstancesOverrideParent.m_param1 );
			break;
		}

	case IBusProxy::MethodSetMaxNumInstances:
		{
			BusProxyCommandData::SetMaxNumInstances setMaxNumInstances;
			if( in_rSerializer.Get( setMaxNumInstances ) )
				pBus->SetMaxNumInstances( setMaxNumInstances.m_param1 );
			break;
		}
		
	case IBusProxy::MethodSetMaxReachedBehavior:
		{
			BusProxyCommandData::SetMaxReachedBehavior setMaxReachedBehavior;
			if( in_rSerializer.Get( setMaxReachedBehavior ) )
				pBus->SetMaxReachedBehavior( setMaxReachedBehavior.m_param1 );
			break;
		}

	case IBusProxy::MethodSetOverLimitBehavior:
		{
			BusProxyCommandData::SetOverLimitBehavior setOverLimitBehavior;
			if( in_rSerializer.Get( setOverLimitBehavior ) )
				pBus->SetOverLimitBehavior( setOverLimitBehavior.m_param1 );
			break;
		}

	case IBusProxy::MethodSetRecoveryTime:
		{
			BusProxyCommandData::SetRecoveryTime setRecoveryTime;
			if( in_rSerializer.Get( setRecoveryTime ) )
				pBus->SetRecoveryTime( AkTimeConv::MillisecondsToSamples( setRecoveryTime.m_param1 ) );
			break;
		}

	case IBusProxy::MethodSetMaxDuckVolume:
		{
			BusProxyCommandData::SetMaxDuckVolume setMaxDuckVolume;
			if( in_rSerializer.Get( setMaxDuckVolume ) )
				pBus->SetMaxDuckVolume( setMaxDuckVolume.m_param1 );
			break;
		}

	case IBusProxy::MethodAddDuck:
		{
			BusProxyCommandData::AddDuck addDuck;
			if (in_rSerializer.Get( addDuck ))
				pBus->AddDuck(	addDuck.m_param1, 
										addDuck.m_param2, 
										addDuck.m_param3, 
										addDuck.m_param4, 
										(AkCurveInterpolation) addDuck.m_param5,
										(AkPropID) addDuck.m_param6 );

			break;
		}

	case IBusProxy::MethodRemoveDuck:
		{
			BusProxyCommandData::RemoveDuck removeDuck;
			if (in_rSerializer.Get( removeDuck ))
				pBus->RemoveDuck( removeDuck.m_param1 );

			break;
		}

	case IBusProxy::MethodRemoveAllDuck:
		{
			BusProxyCommandData::RemoveAllDuck removeAllDuck;
			if (in_rSerializer.Get( removeAllDuck ))
				pBus->RemoveAllDuck();

			break;
		}

	case IBusProxy::MethodSetAsBackgroundMusic:
		{
			BusProxyCommandData::SetAsBackgroundMusic setAsBackgroundMusic;
			if (in_rSerializer.Get( setAsBackgroundMusic ))
			{
				pBus->SetAsBackgroundMusicBus();
			}

			break;
		}
		
	case IBusProxy::MethodUnsetAsBackgroundMusic:
		{
			BusProxyCommandData::UnsetAsBackgroundMusic unsetAsBackgroundMusic;
			if (in_rSerializer.Get( unsetAsBackgroundMusic ))
			{
				pBus->UnsetAsBackgroundMusicBus();
			}

			break;
		}

	case IBusProxy::MethodChannelConfig:
		{
			BusProxyCommandData::ChannelConfig cmd;
			if (in_rSerializer.Get( cmd ))
				pBus->ChannelConfig( cmd.m_param1 );
			break;
		}

	case IBusProxy::MethodSetHdrBus:
		{
			BusProxyCommandData::SetHdrBus cmd;
			if (in_rSerializer.Get( cmd ))
				pBus->SetHdrBus( cmd.m_param1 );
			break;
		}

	case IBusProxy::MethodSetHdrReleaseMode:
		{
			BusProxyCommandData::SetHdrReleaseMode cmd;
			if (in_rSerializer.Get( cmd ))
				pBus->SetHdrReleaseMode( cmd.m_param1 );
			break;
		}

	case IBusProxy::MethodSetHdrCompressorDirty:
		{
			BusProxyCommandData::SetHdrCompressorDirty cmd;
			if (in_rSerializer.Get( cmd ))
				pBus->SetHdrCompressorDirty();
			break;
		}

	case IBusProxy::MethodSetMasterBus:
		break;	//Do not override the game's bus-device relation.

	default:
		__base::HandleExecute( in_uMethodID, in_rSerializer, out_rReturnSerializer );
	}
}
#endif // #ifndef AK_OPTIMIZED
