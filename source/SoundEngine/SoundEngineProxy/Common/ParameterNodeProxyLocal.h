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
#include "IParameterNodeProxy.h"

class CAkParameterNode;

class ParameterNodeProxyLocal : public ParameterableProxyLocal
								, virtual public IParameterNodeProxy
{
public:
	// IParameterableProxy members
	virtual void OverrideFXParent( bool in_bIsFXOverrideParent );

	virtual void SetBelowThresholdBehavior( AkBelowThresholdBehavior in_eBelowThresholdBehavior );
	virtual void SetMaxNumInstancesOverrideParent( bool in_bOverride );
	virtual void SetVVoicesOptOverrideParent( bool in_bOverride );
	virtual void SetMaxNumInstances( AkUInt16 in_u16MaxNumInstance );
	virtual void SetIsGlobalLimit( bool in_bIsGlobalLimit );
	virtual void SetMaxReachedBehavior( bool in_bKillNewest );
	virtual void SetOverLimitBehavior( bool in_bUseVirtualBehavior );
	virtual void SetVirtualQueueBehavior( AkVirtualQueueBehavior in_eBehavior );
	
	virtual void SetOverrideGameAuxSends( bool in_bOverride );
	virtual void SetOverrideUserAuxSends( bool in_bOverride );
	
	virtual void SetOverrideHdrEnvelope( bool in_bOverride );
	virtual void SetOverrideAnalysis( bool in_bOverride );
	virtual void SetNormalizeLoudness( bool in_bNormalizeLoudness );
	virtual void SetEnableEnvelope( bool in_bEnableEnvelope );
	virtual void SetOverrideMidiEventsBehavior( bool in_bOverride );
	virtual void SetOverrideMidiNoteTracking( bool in_bOverride );
	virtual void SetEnableMidiNoteTracking( bool in_bEnable );
	virtual void SetIsMidiBreakLoopOnNoteOff( bool in_bBreak );
};
#endif
#endif // #ifndef AK_OPTIMIZED
