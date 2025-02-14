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

//////////////////////////////////////////////////////////////////////
//
// AkBusCtx.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkBusCtx.h"
#include "AkBus.h"
#include "AkFxBase.h"
#include "AkRegistryMgr.h"

using namespace AK;

CAkBusCtx CAkBusCtx::FindParentCtx() const
{
	CAkBusCtx ctx;

	if ( m_pBus )
	{
		CAkParameterNodeBase * pParent = m_pBus->ParentBus();
		if ( pParent )
		{
			CAkBus * pBus = pParent->GetMixingBus();
			if ( pBus )
			{
				// TODO Implement merging policy. (make global as much as possible, unless this bus has game object and parent bus has 3d positioning?)
				AkGameObjectID objectID = /* ( pBus->IsPositioningEnabled() && pBus->GetPannerType() == Ak3D ) ? */ m_gameObjectID; // : "parent's instance id" ;

				ctx.SetBus( pBus, objectID );
			}
		}
	}
	
	return ctx;
}

// Sound parameters.
AkUniqueID CAkBusCtx::ID() const
{
	if (m_pBus)
	{
		return m_pBus->ID();
	}
	else
	{
		return AK_INVALID_UNIQUE_ID;
	}
}

bool CAkBusCtx::IsTopBusCtx() const
{
	if ( m_pBus )
		return m_pBus->IsTopBus();
	else
		return false;
}

AkChannelConfig CAkBusCtx::GetChannelConfig() const
{
	// if !m_pBus->ChannelConfig().IsValid(), the bus has the same config as its parent.
	if ( m_pBus )
		return m_pBus->ChannelConfig();

	// No m_pBus
	return AkChannelConfig();
}

// Effects access.
void CAkBusCtxRslvd::GetFX( AkUInt32 in_uFXIndex, AkFXDesc& out_rFXInfo ) const
{
	if( m_pBus )
	{
		m_pBus->GetFX( in_uFXIndex, out_rFXInfo, m_pGameObj );
	}
	else
	{
		out_rFXInfo.pFx = NULL;
		out_rFXInfo.iBypassed = 0;
	}
}

void CAkBusCtx::GetFXDataID( AkUInt32 in_uFXIndex, AkUInt32 in_uDataIndex, AkUniqueID& out_rDataID) const
{
	if( m_pBus )
	{
		m_pBus->GetFXDataID( in_uFXIndex, in_uDataIndex, out_rDataID );
	}
	else
	{
		out_rDataID = AK_INVALID_SOURCE_ID;
	}
}

bool CAkBusCtx::HasMixerPlugin() const
{
	return m_pBus ? m_pBus->HasMixerPlugin() : false;
}

void CAkBusCtx::GetMixerPlugin( AkFXDesc& out_rFXInfo ) const
{
	if( m_pBus )
	{
		m_pBus->GetMixerPlugin( out_rFXInfo );
	}
	else
	{
		out_rFXInfo.pFx = NULL;
		out_rFXInfo.iBypassed = 0;
	}
}

void CAkBusCtx::GetMixerPluginDataID(AkUInt32 in_uDataIndex, AkUInt32& out_rDataID) const
{
	if( m_pBus )
	{
		m_pBus->GetMixerPluginDataID( in_uDataIndex, out_rDataID );
	}
	else
	{
		out_rDataID = AK_INVALID_SOURCE_ID;
	}
}

void CAkBusCtx::GetAttachedPropFX( AkFXDesc& out_rFXInfo ) const
{
	if( m_pBus )
	{
		m_pBus->GetAttachedPropFX( out_rFXInfo );
	}
	else
	{
		out_rFXInfo.pFx = NULL;
		out_rFXInfo.iBypassed = 0;
	}
}

bool CAkBusCtx::IsAuxBus() const
{
	return m_pBus ? m_pBus->NodeCategory() ==  AkNodeCategory_AuxBus: false;
}

CAkBusCtxRslvd::~CAkBusCtxRslvd()
{
	if (m_pGameObj)
		m_pGameObj->Release();
}

void CAkBusCtxRslvd::Resolve()
{
	if (m_pGameObj)
		m_pGameObj->Release();

	m_pGameObj = NULL;

	if (m_gameObjectID != AK_INVALID_GAME_OBJECT)
	{
		m_pGameObj = g_pRegistryMgr->GetObjAndAddref(m_gameObjectID);
	}
}

void CAkBusCtxRslvd::Transfer(CAkRegisteredObj* in_pGameObj)
{
	if (m_pGameObj)
		m_pGameObj->Release();

	m_pGameObj = in_pGameObj;

	if (m_pGameObj)
		m_pGameObj->AddRef();
}