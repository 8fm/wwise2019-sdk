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
// AkMidiDeviceMgr.cpp
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkAudioMgr.h"
#include "AkMidiDeviceMgr.h"
#include "AkRegistryMgr.h"

#include <AK/SoundEngine/Common/AkSoundEngine.h>

#include <AK/Tools/Common/AkLock.h>
#include <AK/Tools/Common/AkAutoLock.h>

extern CAkAudioMgr* g_pAudioMgr;

//-----------------------------------------------------------------------------
// Default values defines.
//-----------------------------------------------------------------------------

//------------------------------------------------------------------
// Global variables.
//------------------------------------------------------------------
CAkMidiDeviceMgr* CAkMidiDeviceMgr::m_pMidiMgr = NULL;

namespace AK
{
	namespace SoundEngine
	{
		extern AkAtomic32 g_PlayingID;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CAkMidiDeviceMgr * CAkMidiDeviceMgr::Create()
{
    if ( m_pMidiMgr )
    {
        AKASSERT( !"Should be called only once" );
        return m_pMidiMgr;
    }

    AkNew( AkMemID_Object, CAkMidiDeviceMgr() );

	if ( m_pMidiMgr )
	{
		AK::SoundEngine::RegisterGlobalCallback( GlobalCallback, AkGlobalCallbackLocation_BeginRender | AkGlobalCallbackLocation_Term);
	}

    return m_pMidiMgr;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void CAkMidiDeviceMgr::Destroy()
{
	AK::SoundEngine::UnregisterGlobalCallback( GlobalCallback, AkGlobalCallbackLocation_BeginRender | AkGlobalCallbackLocation_Term);
	DestroyCtx();
    AkDelete(AkMemID_Object, this );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CAkMidiDeviceMgr::CAkMidiDeviceMgr()
{
	AKASSERT(m_pMidiMgr == NULL);
    m_pMidiMgr = this;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CAkMidiDeviceMgr::~CAkMidiDeviceMgr()
{
	m_pMidiMgr = NULL;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void CAkMidiDeviceMgr::DestroyCtx()
{
	// Delete the device ctx list
	MidiCtxList::Iterator it = m_listMidiCtx.Begin();
	while( it != m_listMidiCtx.End() )
	{
		CAkMidiDeviceCtx* pCtx = static_cast<CAkMidiDeviceCtx*>( *it );
		++it; // Move to next ctx first because the current may be removed
		pCtx->DetachAndRelease();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CAkMidiDeviceCtx* CAkMidiDeviceMgr::AddCtx( AkUniqueID in_idTarget, CAkRegisteredObj* in_pGameObj, bool in_bMustAutoRelease )
{
	// Make sure everything is in order
	AKASSERT( m_pMidiMgr && in_pGameObj );
	if( ! m_pMidiMgr || ! in_pGameObj )
		return NULL;

	// Check parameters.
	AKASSERT( in_idTarget != AK_INVALID_UNIQUE_ID );
	if( in_idTarget == AK_INVALID_UNIQUE_ID )
		return NULL;

	UserParams userParams;
	userParams.SetPlayingID((AkPlayingID)AkAtomicInc32(&AK::SoundEngine::g_PlayingID));
	TransParams transParams;
	transParams.TransitionTime = 0;
	transParams.eFadeCurve = AkCurveInterpolation_Linear;

	// Create new ctx
	CAkMidiDeviceCtx* pCtx = AkNew( AkMemID_Object, CAkMidiDeviceCtx
			(
				this,
				in_pGameObj,
				transParams,
				userParams,
				in_idTarget,
				in_bMustAutoRelease
			) );

	// Check if everything completed correctly
	if( pCtx != NULL )
	{
		if( pCtx->Init() == AK_Success )
			return pCtx;
		else
			pCtx->Release();
	}

	return NULL;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CAkMidiDeviceCtx* CAkMidiDeviceMgr::GetCtx( AkUniqueID in_idTarget, CAkRegisteredObj* in_pGameObj )
{
	if( m_pMidiMgr != NULL && in_idTarget != AK_INVALID_UNIQUE_ID && in_pGameObj != NULL )
	{
		// Find the ctx for the given midi channel
		MidiCtxList::Iterator it = m_listMidiCtx.Begin();
		for( ; it != m_listMidiCtx.End(); ++it )
		{
			CAkMidiDeviceCtx* pCtx = static_cast<CAkMidiDeviceCtx*>(*it);
			if( pCtx->MidiTargetId() == in_idTarget && pCtx->GetGameObj() == in_pGameObj )
				return pCtx;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CAkMidiDeviceCtx* CAkMidiDeviceMgr::AddTarget( AkUniqueID in_idTarget, AkGameObjectID in_idGameObj )
{
	if( m_pMidiMgr == NULL || in_idTarget == AK_INVALID_UNIQUE_ID || in_idGameObj == AK_INVALID_GAME_OBJECT )
		return NULL;

	CAkRegisteredObj* pGameObj = g_pRegistryMgr->GetObjAndAddref( in_idGameObj );
	if( ! pGameObj )
		return NULL;

	CAkMidiDeviceCtx* pCtx = GetCtx( in_idTarget, pGameObj );
	if( pCtx == NULL )
		pCtx = AddCtx( in_idTarget, pGameObj, false );

	pGameObj->Release();

	return pCtx;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void CAkMidiDeviceMgr::RemoveTarget( AkUniqueID in_idTarget, AkGameObjectID in_idGameObj )
{
	if( m_pMidiMgr == NULL || in_idTarget == AK_INVALID_UNIQUE_ID || in_idGameObj == AK_INVALID_GAME_OBJECT )
		return;

	CAkRegisteredObj* pGameObj = g_pRegistryMgr->GetObjAndAddref( in_idGameObj );
	if( ! pGameObj )
		return;

	// Find the ctx for the given midi channel
	CAkMidiDeviceCtx* pCtx = GetCtx( in_idTarget, pGameObj );
	if( pCtx != NULL )
		pCtx->DetachAndRelease();

	pGameObj->Release();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void CAkMidiDeviceMgr::WwiseEvent( AkUniqueID in_idTarget, AkGameObjectID in_idGameObj, AkMidiEventEx in_MidiEvent, AkUInt32 in_uTimestampMs )
{
	if( m_pMidiMgr == NULL || in_idTarget == AK_INVALID_UNIQUE_ID || in_idGameObj == AK_INVALID_GAME_OBJECT )
		return;

	CAkRegisteredObj* pGameObj = g_pRegistryMgr->GetObjAndAddref( in_idGameObj );
	if( ! pGameObj )
		return;

	CAkMidiDeviceCtx* pCtx = GetCtx( in_idTarget, pGameObj );
	if( pCtx != NULL )
		pCtx->WwiseEvent( in_MidiEvent, in_uTimestampMs );

	pGameObj->Release();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void CAkMidiDeviceMgr::PostEvents( AkUniqueID in_idTarget, AkGameObjectID in_idGameObj, AkMIDIPost* in_pPosts, AkUInt32 in_uNumPosts )
{
	if( m_pMidiMgr == NULL || in_idTarget == AK_INVALID_UNIQUE_ID || in_idGameObj == AK_INVALID_GAME_OBJECT )
		return;

	CAkRegisteredObj* pGameObj = g_pRegistryMgr->GetObjAndAddref( in_idGameObj );
	if( ! pGameObj )
		return;

	CAkMidiDeviceCtx* pCtx = GetCtx( in_idTarget, pGameObj );
	if( pCtx == NULL )
		pCtx = AddCtx( in_idTarget, pGameObj, true );

	pGameObj->Release();

	if( pCtx != NULL )
	{
		for( AkUInt16 i = 0; i < in_uNumPosts; ++i )
			pCtx->AddEvent( reinterpret_cast<AkMidiEventEx&>( in_pPosts[i] ), in_pPosts[i].uOffset );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void CAkMidiDeviceMgr::StopAll( AkUniqueID in_idTarget, AkGameObjectID in_idGameObj )
{
	if( m_pMidiMgr == NULL )
		return;

	CAkMidiDeviceCtx* pCtx = NULL;

	// Find the ctx for the given midi channel
	MidiCtxList::Iterator it = m_listMidiCtx.Begin();
	while( it != m_listMidiCtx.End() )
	{
		pCtx = static_cast<CAkMidiDeviceCtx*>( *it );
		++it;

		if( ( pCtx->MidiTargetId() == in_idTarget || in_idTarget == AK_INVALID_UNIQUE_ID )
			&& ( pCtx->GetGameObjID() == in_idGameObj || in_idGameObj == AK_INVALID_GAME_OBJECT ) )
		{
			pCtx->DetachAndRelease();
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void CAkMidiDeviceMgr::NextFrame( AkUInt32 in_uNumSamples )
{
	if( m_pMidiMgr == NULL )
		return;

	// Cycle through all midi device ctx
	MidiFrameEventList listEvents;
	MidiCtxList::Iterator it = m_listMidiCtx.BeginEx();
	while( it != m_listMidiCtx.End() )
	{
		CAkMidiDeviceCtx* pCtx = static_cast<CAkMidiDeviceCtx*>( *it );
		++it;
		pCtx->OnFrame( listEvents, in_uNumSamples );
	}

	ScheduleMidiEvents( listEvents, in_uNumSamples );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void CAkMidiDeviceMgr::GlobalCallback(AK::IAkGlobalPluginContext * in_pContext, AkGlobalCallbackLocation, void *)
{
	if( m_pMidiMgr )
		m_pMidiMgr->NextFrame(in_pContext->GetMaxBufferLength());
}
