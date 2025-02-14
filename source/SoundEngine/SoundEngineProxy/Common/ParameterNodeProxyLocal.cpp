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

#include "ParameterNodeProxyLocal.h"
#include "AkParameterNode.h"
#include "AkCritical.h"


void ParameterNodeProxyLocal::OverrideFXParent( bool in_bIsFXOverrideParent )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		pIndexable->OverrideFXParent( in_bIsFXOverrideParent );
	}
}

void ParameterNodeProxyLocal::SetBelowThresholdBehavior( AkBelowThresholdBehavior in_eBelowThresholdBehavior )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetBelowThresholdBehavior( in_eBelowThresholdBehavior );
	}
}

void ParameterNodeProxyLocal::SetMaxNumInstancesOverrideParent( bool in_bOverride )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetMaxNumInstIgnoreParent( in_bOverride ); 
	}
}

void ParameterNodeProxyLocal::SetVVoicesOptOverrideParent( bool in_bOverride )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetVVoicesOptOverrideParent( in_bOverride );
	}
}

void ParameterNodeProxyLocal::SetMaxNumInstances( AkUInt16 in_u16MaxNumInstance )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetMaxNumInstances( in_u16MaxNumInstance );
	}
}

void ParameterNodeProxyLocal::SetIsGlobalLimit( bool in_bIsGlobalLimit )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetIsGlobalLimit( in_bIsGlobalLimit );
	}
}

void ParameterNodeProxyLocal::SetMaxReachedBehavior( bool in_bKillNewest )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetMaxReachedBehavior( in_bKillNewest );
	}
}

void ParameterNodeProxyLocal::SetOverLimitBehavior( bool in_bUseVirtualBehavior )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetOverLimitBehavior( in_bUseVirtualBehavior );
	}
}

void ParameterNodeProxyLocal::SetVirtualQueueBehavior( AkVirtualQueueBehavior in_eBehavior )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetVirtualQueueBehavior( in_eBehavior );
	}
}

void ParameterNodeProxyLocal::SetOverrideGameAuxSends( bool in_bOverride )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetOverrideGameAuxSends( in_bOverride );
	}
}

void ParameterNodeProxyLocal::SetOverrideUserAuxSends( bool in_bOverride )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetOverrideUserAuxSends( in_bOverride );
	}
}

void ParameterNodeProxyLocal::SetOverrideHdrEnvelope( bool in_bOverride )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetOverrideHdrEnvelope( in_bOverride );
	}
}

void ParameterNodeProxyLocal::SetOverrideAnalysis( bool in_bOverride )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetOverrideAnalysis( in_bOverride );
	}
}

void ParameterNodeProxyLocal::SetNormalizeLoudness( bool in_bNormalizeLoudness )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetNormalizeLoudness( in_bNormalizeLoudness );
	}
}

void ParameterNodeProxyLocal::SetEnableEnvelope( bool in_bEnableEnvelope )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetEnableEnvelope( in_bEnableEnvelope );
	}
}

void ParameterNodeProxyLocal::SetOverrideMidiEventsBehavior( bool in_bOverride )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetOverrideMidiEventsBehavior( in_bOverride );
	}
}

void ParameterNodeProxyLocal::SetOverrideMidiNoteTracking( bool in_bOverride )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetOverrideMidiNoteTracking( in_bOverride );
	}
}

void ParameterNodeProxyLocal::SetEnableMidiNoteTracking( bool in_bEnable )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetEnableMidiNoteTracking( in_bEnable );
	}
}

void ParameterNodeProxyLocal::SetIsMidiBreakLoopOnNoteOff( bool in_bBreak )
{
	CAkParameterNode* pIndexable = static_cast<CAkParameterNode*>( GetIndexable() );
	if( pIndexable )
	{
		return pIndexable->SetIsMidiBreakLoopOnNoteOff( in_bBreak );
	}
}

#endif
#endif // #ifndef AK_OPTIMIZED
