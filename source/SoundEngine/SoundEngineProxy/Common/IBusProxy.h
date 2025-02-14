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

#include "IParameterableProxy.h"

class IBusProxy : virtual public IParameterableProxy
{
	DECLARE_BASECLASS( IParameterableProxy );
public:
	virtual void SetMaxNumInstancesOverrideParent( bool in_bOverride ) = 0;
	virtual void SetMaxNumInstances( AkUInt16 in_ulMaxNumInstance ) = 0;
	virtual void SetMaxReachedBehavior( bool in_bKillNewest ) = 0;
	virtual void SetOverLimitBehavior( bool in_bUseVirtualBehavior ) = 0;

	virtual void SetRecoveryTime(AkTimeMs in_recoveryTime) = 0;
	virtual void SetMaxDuckVolume(AkReal32 in_fMaxDuckVolume) = 0;

	virtual void AddDuck(
		AkUniqueID in_busID,
		AkVolumeValue in_duckVolume,
		AkTimeMs in_fadeOutTime,
		AkTimeMs in_fadeInTime,
		AkCurveInterpolation in_eFadeCurve,
		AkPropID in_TargetProp
		) = 0;

	virtual void RemoveDuck(
		AkUniqueID in_busID
		) = 0;

	virtual void RemoveAllDuck() = 0;

	virtual void SetAsBackgroundMusic() = 0;
	virtual void UnsetAsBackgroundMusic() = 0;

	virtual void ChannelConfig( AkChannelConfig in_channelConfig ) = 0;

	virtual void SetHdrBus( bool in_bIsHdrBus ) = 0;
	virtual void SetHdrReleaseMode( bool in_bHdrReleaseModeExponential ) = 0;
	virtual void SetHdrCompressorDirty() = 0;

	virtual void SetMasterBus(AkUniqueID in_idDeviceShareset, AkUInt32 in_idDevice) = 0;

	enum MethodIDs
	{
		MethodSetMaxNumInstancesOverrideParent = __base::LastMethodID,
		MethodSetMaxNumInstances,
		MethodSetMaxReachedBehavior,
		MethodSetOverLimitBehavior,

		MethodSetRecoveryTime,
		MethodSetMaxDuckVolume,
		MethodAddDuck,
		MethodRemoveDuck,
		MethodRemoveAllDuck,

		MethodSetAsBackgroundMusic,
		MethodUnsetAsBackgroundMusic,

		MethodChannelConfig,
		
		MethodSetHdrBus,
		MethodSetHdrReleaseMode,
		MethodSetHdrCompressorDirty,
		
		MethodSetMasterBus,

		LastMethodID
	};

};

namespace BusProxyCommandData
{
	typedef ObjectProxyCommandData::CommandData1
	< 
		IBusProxy::MethodSetMaxNumInstancesOverrideParent,
		bool
	> SetMaxNumInstancesOverrideParent;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IBusProxy::MethodSetMaxNumInstances,
		AkUInt16
	> SetMaxNumInstances;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IBusProxy::MethodSetMaxReachedBehavior,
		bool
	> SetMaxReachedBehavior;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IBusProxy::MethodSetOverLimitBehavior,
		bool
	> SetOverLimitBehavior;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IBusProxy::MethodSetRecoveryTime,
		AkTimeMs
	> SetRecoveryTime;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IBusProxy::MethodSetMaxDuckVolume,
		AkReal32
	> SetMaxDuckVolume;

	typedef ObjectProxyCommandData::CommandData6
	< 
		IBusProxy::MethodAddDuck,
		AkUniqueID,
		AkVolumeValue,
		AkTimeMs,
		AkTimeMs,
		AkUInt32,
		AkUInt32
	> AddDuck;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IBusProxy::MethodRemoveDuck,
		AkUniqueID
	> RemoveDuck;

	typedef ObjectProxyCommandData::CommandData0
	< 
		IBusProxy::MethodRemoveAllDuck
	> RemoveAllDuck;

	typedef ObjectProxyCommandData::CommandData0
	< 
		IBusProxy::MethodSetAsBackgroundMusic
	> SetAsBackgroundMusic;

	typedef ObjectProxyCommandData::CommandData0
	< 
		IBusProxy::MethodUnsetAsBackgroundMusic
	> UnsetAsBackgroundMusic;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IBusProxy::MethodChannelConfig,
		AkChannelConfig
	> ChannelConfig;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IBusProxy::MethodSetHdrBus,
		bool
	> SetHdrBus;

	typedef ObjectProxyCommandData::CommandData1
	< 
		IBusProxy::MethodSetHdrReleaseMode,
		bool
	> SetHdrReleaseMode;

	typedef ObjectProxyCommandData::CommandData0
	< 
		IBusProxy::MethodSetHdrCompressorDirty
	> SetHdrCompressorDirty;

	typedef ObjectProxyCommandData::CommandData2
	< 
		IBusProxy::MethodSetMasterBus,
		AkUniqueID,
		AkUInt32
	> SetMasterBus;
}

#endif // #ifndef AK_OPTIMIZED
