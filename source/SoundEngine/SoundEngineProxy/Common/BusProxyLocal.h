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
#ifndef PROXYCENTRAL_CONNECTED

#include "ParameterableProxyLocal.h"
#include "IBusProxy.h"

class BusProxyLocal : public ParameterableProxyLocal
							, virtual public IBusProxy
{
public:
	BusProxyLocal();
	virtual ~BusProxyLocal();

	virtual void Init( AkUniqueID in_id );

	virtual void SetMaxNumInstancesOverrideParent( bool in_bOverride );
	virtual void SetMaxNumInstances( AkUInt16 in_ulMaxNumInstance );
	virtual void SetMaxReachedBehavior( bool in_bKillNewest );
	virtual void SetOverLimitBehavior( bool in_bUseVirtualBehavior );

	virtual void SetRecoveryTime(AkTimeMs in_recoveryTime);
	virtual void SetMaxDuckVolume(AkReal32 in_fMaxDuckVolume);

	virtual void AddDuck(
		AkUniqueID in_busID,
		AkVolumeValue in_duckVolume,
		AkTimeMs in_fadeOutTime,
		AkTimeMs in_fadeInTime,
		AkCurveInterpolation in_eFadeCurve,
		AkPropID in_TargetProp
		);

	virtual void RemoveDuck(
		AkUniqueID in_busID
		);

	virtual void RemoveAllDuck();

	virtual void SetAsBackgroundMusic();
	virtual void UnsetAsBackgroundMusic();

	virtual void ChannelConfig( AkChannelConfig in_channelConfig );

	virtual void SetHdrBus( bool in_bIsHdrBus );
	virtual void SetHdrReleaseMode( bool in_bHdrReleaseModeExponential );
	virtual void SetHdrCompressorDirty();

	virtual void SetMasterBus(AkUniqueID in_idDeviceShareset, AkUInt32 in_idDevice);
};

#endif
#endif // #ifndef AK_OPTIMIZED
