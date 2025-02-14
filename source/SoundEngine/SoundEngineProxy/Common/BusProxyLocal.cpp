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
#ifndef PROXYCENTRAL_CONNECTED

#include "BusProxyLocal.h"
#include "AkBus.h"
#include "AkAudioLib.h"
#include "AkCritical.h"


BusProxyLocal::BusProxyLocal()
{
}

BusProxyLocal::~BusProxyLocal()
{
}

void BusProxyLocal::Init( AkUniqueID in_id )
{
	CAkIndexable* pIndexable = AK::SoundEngine::GetIndexable( in_id, AkIdxType_BusNode );
	SetIndexable( pIndexable != NULL ? pIndexable : CAkBus::Create( in_id ) );
}

void BusProxyLocal::SetMaxNumInstancesOverrideParent( bool in_bOverride )
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->SetMaxNumInstIgnoreParent( in_bOverride );
	}
}

void BusProxyLocal::SetMaxNumInstances( AkUInt16 in_ulMaxNumInstance )
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->SetMaxNumInstances( in_ulMaxNumInstance );
	}
}

void BusProxyLocal::SetMaxReachedBehavior( bool in_bKillNewest )
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->SetMaxReachedBehavior( in_bKillNewest );
	}
}

void BusProxyLocal::SetOverLimitBehavior( bool in_bUseVirtualBehavior )
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->SetOverLimitBehavior( in_bUseVirtualBehavior );
	}
}

void BusProxyLocal::SetRecoveryTime(AkTimeMs in_recoveryTime)
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->SetRecoveryTime( AkTimeConv::MillisecondsToSamples( in_recoveryTime ) );
	}
}

void BusProxyLocal::SetMaxDuckVolume(AkReal32 in_fMaxDuckVolume)
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->SetMaxDuckVolume( in_fMaxDuckVolume );
	}
}

void BusProxyLocal::AddDuck(
		AkUniqueID in_busID,
		AkVolumeValue in_duckVolume,
		AkTimeMs in_fadeOutTime,
		AkTimeMs in_fadeInTime,
		AkCurveInterpolation in_eFadeCurve,
		AkPropID in_TargetProp
		)
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->AddDuck( in_busID, in_duckVolume, in_fadeOutTime, in_fadeInTime, in_eFadeCurve, in_TargetProp );
	}
}

void BusProxyLocal::RemoveDuck( AkUniqueID in_busID )
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->RemoveDuck( in_busID );
	}
}

void BusProxyLocal::RemoveAllDuck()
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->RemoveAllDuck();
	}
}

void BusProxyLocal::SetAsBackgroundMusic()
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->SetAsBackgroundMusicBus();
	}
}

void BusProxyLocal::UnsetAsBackgroundMusic()
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->UnsetAsBackgroundMusicBus();
	}
}

void BusProxyLocal::ChannelConfig( AkChannelConfig in_channelConfig )
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->ChannelConfig( in_channelConfig );
	}
} 

void BusProxyLocal::SetHdrBus( bool in_bIsHdrBus )
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->SetHdrBus( in_bIsHdrBus );
	}
}

void BusProxyLocal::SetHdrReleaseMode( bool in_bHdrReleaseModeExponential )
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->SetHdrReleaseMode( in_bHdrReleaseModeExponential );
	}
}

void BusProxyLocal::SetHdrCompressorDirty()
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->SetHdrCompressorDirty();
	}
}

void BusProxyLocal::SetMasterBus(AkUniqueID in_idDeviceShareset, AkUInt32 in_idDevice)
{
	CAkBus* pIndexable = static_cast<CAkBus*>( GetIndexable() );
	if( pIndexable )
	{
		CAkFunctionCritical SpaceSetAsCritical;

		pIndexable->SetMasterBus(in_idDeviceShareset, in_idDevice);
	}
}

#endif
#endif // #ifndef AK_OPTIMIZED
