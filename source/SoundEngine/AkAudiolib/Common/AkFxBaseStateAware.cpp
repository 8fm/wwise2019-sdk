/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

//////////////////////////////////////////////////////////////////////
//
// AkParameterNodeBase.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "AkFxBase.h"
#include "AkFxBaseStateAware.h"

CAkFxBaseStateAware::CAkFxBaseStateAware()
	: m_pData( NULL )
{}

CAkFxBaseStateAware::~CAkFxBaseStateAware()
{
	if ( m_pData )
	{
		if( !m_pData->m_stateChunks.IsEmpty() )
		{
			FlushStateTransitions();
			RemoveStateGroups( false );
		}

		AkDelete(AkMemID_Structure, m_pData );
		m_pData = NULL;
	}
}

bool CAkFxBaseStateAware::EnsureStateData()
{
	if ( ! m_pData )
		m_pData = AkNew(AkMemID_Structure, FxBaseStateAwareData() );

	return m_pData != NULL;
}

void CAkFxBaseStateAware::SetStateProperties( AkUInt32 in_uProperties, AkStatePropertyUpdate* in_pProperties, bool in_bNotify )
{
	CAkStateAware::SetStateProperties( in_uProperties, in_pProperties, false );
	AKASSERT( m_pData );
	if ( ! m_pData )
		return;

	CAkFxBase* fxBase = static_cast<CAkFxBase*>( this );

	// Need to iterate new list of state properties and update FxBase!
	// FxBase needs to know which properties support controllers so NOT to update properties that shouldn't be
	StatePropertyArray::Iterator ip = m_pData->m_stateProperties.Begin();
	for ( ; ip != m_pData->m_stateProperties.End(); ++ip )
	{
		PluginPropertyValues::Iterator iv = fxBase->m_propertyValues.Begin();
		for ( ; iv != fxBase->m_propertyValues.End(); ++iv )
		{
			if ( (*iv).propertyId == (*ip).propertyId )
			{
				(*iv).rtpcAccum = (*ip).accumType;
				break;
			}
		}
	}

	if (in_bNotify)
		NotifyStateParametersModified();
}

void CAkFxBaseStateAware::PushStateParamUpdate( AkStatePropertyId in_propertyId, AkRtpcAccum in_rtpcAccum, AkStateGroupID in_stateGroup, AkReal32 /*in_oldValue*/, AkReal32 in_newValue, bool /*in_transitionDone*/ )
{
	AKASSERT( m_pData );

	AkDeltaMonitorObjBrace objBrace(static_cast<CAkFxBase*>(this)->key);

	// Update all instances
	CAkFxBase::Instances::Iterator it = static_cast<CAkFxBase*>( this )->m_instances.Begin();
	CAkFxBase::Instances::Iterator end = static_cast<CAkFxBase*>( this )->m_instances.End();

	for ( ; it != end; ++it )
	{
		(*it)->UpdateTargetParam( PluginPropertyUpdateType_State, (AkUniqueID)in_stateGroup, (AkRTPC_ParameterID)in_propertyId, in_rtpcAccum, in_newValue );
	}
}

void CAkFxBaseStateAware::UpdateStateParamTargets()
{
	static_cast<CAkFxBase*>(this)->RecalculatePropertyValues();
}

void CAkFxBaseStateAware::NotifyStateParamTargets()
{
	static_cast<CAkFxBase*>(this)->RecalculatePropertyValues();
}
