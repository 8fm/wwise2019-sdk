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

#include "ParameterNodeProxyConnected.h"
#include "AkParameterNode.h"
#include "CommandDataSerializer.h"
#include "IParameterNodeProxy.h"

ParameterNodeProxyConnected::ParameterNodeProxyConnected()
{
}

ParameterNodeProxyConnected::~ParameterNodeProxyConnected()
{
}

void ParameterNodeProxyConnected::HandleExecute( AkUInt16 in_uMethodID, CommandDataSerializer& in_rSerializer, CommandDataSerializer& out_rReturnSerializer )
{
	CAkParameterNode * pNode = static_cast<CAkParameterNode *>( GetIndexable() );

	switch( in_uMethodID )
	{
	case IParameterNodeProxy::MethodOverrideFXParent:
		{
			ParameterNodeProxyCommandData::OverrideFXParent overrideFXParent;
			if( in_rSerializer.Get( overrideFXParent ) )
					pNode->OverrideFXParent( overrideFXParent.m_param1 );

			break;
		}
	case IParameterNodeProxy::MethodSetBelowThresholdBehavior:
		{
			ParameterNodeProxyCommandData::SetBelowThresholdBehavior setBelowThresholdBehavior;
			if( in_rSerializer.Get( setBelowThresholdBehavior ) )
					pNode->SetBelowThresholdBehavior( (AkBelowThresholdBehavior) setBelowThresholdBehavior.m_param1 );

			break;
		}
	case IParameterNodeProxy::MethodSetMaxNumInstancesOverrideParent:
		{
			ParameterNodeProxyCommandData::SetMaxNumInstancesOverrideParent setMaxNumInstancesOverrideParent;
			if( in_rSerializer.Get( setMaxNumInstancesOverrideParent ) )
					pNode->SetMaxNumInstIgnoreParent( setMaxNumInstancesOverrideParent.m_param1 );

			break;
		}
	case IParameterNodeProxy::MethodSetVVoicesOptOverrideParent:
		{
			ParameterNodeProxyCommandData::SetVVoicesOptOverrideParent setVVoicesOptOverrideParent;
			if( in_rSerializer.Get( setVVoicesOptOverrideParent ) )
					pNode->SetVVoicesOptOverrideParent( setVVoicesOptOverrideParent.m_param1 );

			break;
		}
	case IParameterNodeProxy::MethodSetMaxNumInstances:
		{
			ParameterNodeProxyCommandData::SetMaxNumInstances setMaxNumInstances;
			if( in_rSerializer.Get( setMaxNumInstances ) )
					pNode->SetMaxNumInstances( setMaxNumInstances.m_param1 );

			break;
		}
	case IParameterNodeProxy::MethodSetMaxReachedBehavior:
		{
			ParameterNodeProxyCommandData::SetMaxReachedBehavior setMaxReachedBehavior;
			if( in_rSerializer.Get( setMaxReachedBehavior ) )
					pNode->SetMaxReachedBehavior( setMaxReachedBehavior.m_param1 );

			break;
		}
	case IParameterNodeProxy::MethodSetIsGlobalLimit:
		{
			ParameterNodeProxyCommandData::SetIsGlobalLimit setIsGlobalLimit;
			if( in_rSerializer.Get( setIsGlobalLimit ) )
					pNode->SetIsGlobalLimit( setIsGlobalLimit.m_param1 );

			break;
		}
	case IParameterNodeProxy::MethodSetOverLimitBehavior:
		{
			ParameterNodeProxyCommandData::SetOverLimitBehavior setOverLimitBehavior;
			if( in_rSerializer.Get( setOverLimitBehavior ) )
					pNode->SetOverLimitBehavior( setOverLimitBehavior.m_param1 );

			break;
		}
	case IParameterNodeProxy::MethodSetVirtualQueueBehavior:
		{
			ParameterNodeProxyCommandData::SetVirtualQueueBehavior setVirtualQueueBehavior;
			if( in_rSerializer.Get( setVirtualQueueBehavior ) )
					pNode->SetVirtualQueueBehavior( (AkVirtualQueueBehavior) setVirtualQueueBehavior.m_param1 );

			break;
		}
	case IParameterNodeProxy::MethodSetOverrideGameAuxSends:
		{
			ParameterNodeProxyCommandData::SetOverrideGameAuxSends setOverrideGameAuxSends;
			if( in_rSerializer.Get( setOverrideGameAuxSends ) )
				pNode->SetOverrideGameAuxSends( (bool) setOverrideGameAuxSends.m_param1 );

			break;
		}		
	case IParameterNodeProxy::MethodSetOverrideUserAuxSends:
		{
			ParameterNodeProxyCommandData::SetOverrideUserAuxSends setOverrideUserAuxSends;
			if( in_rSerializer.Get( setOverrideUserAuxSends ) )
				pNode->SetOverrideUserAuxSends( (bool) setOverrideUserAuxSends.m_param1 );

			break;
		}	
	case IParameterNodeProxy::MethodSetOverrideHdrEnvelope:
		{
			ParameterNodeProxyCommandData::SetOverrideHdrEnvelope setOverrideHdrEnvelope;
			if( in_rSerializer.Get( setOverrideHdrEnvelope ) )
				pNode->SetOverrideHdrEnvelope( (bool) setOverrideHdrEnvelope.m_param1 );

			break;
		}	
	case IParameterNodeProxy::MethodSetOverrideAnalysis:
		{
			ParameterNodeProxyCommandData::SetOverrideAnalysis setOverrideAnalysis;
			if( in_rSerializer.Get( setOverrideAnalysis ) )
				pNode->SetOverrideAnalysis( (bool) setOverrideAnalysis.m_param1 );

			break;
		}	
	case IParameterNodeProxy::MethodSetNormalizeLoudness:
		{
			ParameterNodeProxyCommandData::SetNormalizeLoudness setNormalizeLoudness;
			if( in_rSerializer.Get( setNormalizeLoudness ) )
				pNode->SetNormalizeLoudness( (bool) setNormalizeLoudness.m_param1 );

			break;
		}
	case IParameterNodeProxy::MethodSetEnableEnvelope:
		{
			ParameterNodeProxyCommandData::SetEnableEnvelope setEnableEnvelope;
			if( in_rSerializer.Get( setEnableEnvelope ) )
				pNode->SetEnableEnvelope( (bool) setEnableEnvelope.m_param1 );

			break;
		}

	case IParameterNodeProxy::MethodSetOverrideMidiEventsBehavior:
		{
			ParameterNodeProxyCommandData::SetOverrideMidiEventsBehavior setOverrideMidiEventsBehavior;
			if( in_rSerializer.Get( setOverrideMidiEventsBehavior ) )
				pNode->SetOverrideMidiEventsBehavior( (bool) setOverrideMidiEventsBehavior.m_param1 );

			break;
		}

	case IParameterNodeProxy::MethodSetOverrideMidiNoteTracking:
		{
			ParameterNodeProxyCommandData::SetOverrideMidiNoteTracking setOverrideMidiNoteTracking;
			if( in_rSerializer.Get( setOverrideMidiNoteTracking ) )
				pNode->SetOverrideMidiNoteTracking( (bool) setOverrideMidiNoteTracking.m_param1 );

			break;
		}

	case IParameterNodeProxy::MethodSetEnableMidiNoteTracking:
		{
			ParameterNodeProxyCommandData::SetEnableMidiNoteTracking setEnableMidiNoteTracking;
			if( in_rSerializer.Get( setEnableMidiNoteTracking ) )
				pNode->SetEnableMidiNoteTracking( (bool) setEnableMidiNoteTracking.m_param1 );

			break;
		}

	case IParameterNodeProxy::MethodSetIsMidiBreakLoopOnNoteOff:
		{
			ParameterNodeProxyCommandData::SetIsMidiBreakLoopOnNoteOff setIsMidiBreakLoopOnNoteOff;
			if( in_rSerializer.Get( setIsMidiBreakLoopOnNoteOff ) )
				pNode->SetIsMidiBreakLoopOnNoteOff( (bool) setIsMidiBreakLoopOnNoteOff.m_param1 );

			break;
		}

	default:
		__base::HandleExecute( in_uMethodID, in_rSerializer, out_rReturnSerializer );
	}
}
#endif // #ifndef AK_OPTIMIZED
