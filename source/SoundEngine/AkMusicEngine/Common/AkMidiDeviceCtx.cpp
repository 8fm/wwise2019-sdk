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
// CAkMidiDeviceCtx.cpp
//
// Manages the playout context of a midi source.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkAudioMgr.h"
#include "AkMidiDeviceMgr.h"
#include "AkMidiDeviceCtx.h"

extern CAkAudioMgr* g_pAudioMgr;

#define MAX_TIMESTAMP_DIFF_MS ( ~((AkUInt32)0) >> 1 )
#define MAX_SAMPLE_OFFSET ( ~((AkUInt32)0) >> 1 )

CAkMidiDeviceCtx::CAkMidiDeviceCtx
	(
		CAkMidiDeviceMgr *	in_pMidiMgr,
		CAkRegisteredObj*		in_pGameObj,		// Game object and channel association.
		TransParams&		in_rTransParams,
		UserParams&			in_rUserParams,
		AkUniqueID			in_uidRootTarget,
		bool				in_bMustAutoRelease
	)
: CAkMidiBaseCtx( in_pGameObj, in_rTransParams, in_rUserParams, in_uidRootTarget )
, m_pMidiMgr( in_pMidiMgr )
, m_bWasStopped( false )
, m_bMustAutoRelease( in_bMustAutoRelease )
, m_bDidAutoRelease( false )
{
	AKASSERT( m_pMidiMgr );
}

CAkMidiDeviceCtx::~CAkMidiDeviceCtx()
{
	// Empty device event list
	while( ! m_listDeviceEvents.IsEmpty() )
	{
		AkMidiDeviceEvent* pTimedEvent = *m_listDeviceEvents.Begin();
		m_listDeviceEvents.RemoveFirst();
		AkDelete( AkMemID_Object, pTimedEvent );
	}

	// Detach from mgr; this must be done before any destructive operations!!
	if( m_pMidiMgr )
		m_pMidiMgr->DetachCtx( this );
}

AKRESULT CAkMidiDeviceCtx::Init()
{
	AKRESULT eRes = CAkMidiBaseCtx::Init();
	if ( eRes == AK_Success )
		m_pMidiMgr->AttachCtx( this );
	return eRes;
}

CAkMidiBaseMgr* CAkMidiDeviceCtx::GetMidiMgr() const
{
	return m_pMidiMgr;
}

bool CAkMidiDeviceCtx::ResolveMidiTarget()
{
	m_bIsRootTargetBus = false;
	m_pRootTargetNode = g_pIndex->GetNodePtrAndAddRef( m_uRootTargetId, AkNodeType_Default );
	return m_pRootTargetNode != NULL;
}

//-----------------------------------------------------------------------------
// Name: AddEvent
// Desc: Add a MIDI event with an absolute timestamp, in ms.
//-----------------------------------------------------------------------------
void CAkMidiDeviceCtx::WwiseEvent( AkMidiEventEx& in_rMidiEvent, AkUInt32 /*in_uTimestampMs*/ )
{
	if( ! m_pMidiMgr )
		return;

	// Add event to list
	AddEvent( in_rMidiEvent, 0 );
}

//-----------------------------------------------------------------------------
// Name: AddEvent
// Desc: Add a MIDI event with a delta relative to previous event, in samples.
//-----------------------------------------------------------------------------
void CAkMidiDeviceCtx::AddEvent( AkMidiEventEx& in_rMidiEvent, AkUInt32 in_uOffset )
{
	// Don't add a new note if we've stopped, or if we've already added absolute events
	if( m_bWasStopped || m_bDidAutoRelease )
		return;

	if( AkMidiDeviceEvent* pListEvent = AkNew( AkMemID_Object, AkMidiDeviceEvent( in_rMidiEvent ) ) )
	{
		pListEvent->uOffset = in_uOffset;

		DeviceEventList::IteratorEx it = m_listDeviceEvents.BeginEx();
		for( ; it != m_listDeviceEvents.End(); ++it )
		{
			if( (*it)->uOffset > in_uOffset )
				break;
		}

		m_listDeviceEvents.Insert( it, pListEvent );
	}
}

//-----------------------------------------------------------------------------
// Name: OnFrame
// Desc: Called to process one frame of data.
//-----------------------------------------------------------------------------
void CAkMidiDeviceCtx::OnFrame( MidiFrameEventList& in_listEvents, AkUInt32 in_uNumSamples ) // Number of samples to process. 
{
	// This function may delete the object; make sure that only happens at the end!
	AddRef();

	// Finally, check for new events, if required
	if ( in_uNumSamples != 0 && !m_bWasStopped )
	{
		DeviceEventList::IteratorEx it = m_listDeviceEvents.BeginEx();
		while( it != m_listDeviceEvents.End() )
		{
			AkMidiDeviceEvent* pListEvent = *it;

			if( pListEvent->uOffset < in_uNumSamples )
			{
				it = m_listDeviceEvents.Erase( it );
				AddMidiEvent( in_listEvents, *pListEvent, pListEvent->uOffset );
				AkDelete( AkMemID_Object, pListEvent );
			}
			else
			{
				pListEvent->uOffset -= in_uNumSamples;
				++it;
			}
		}
	}

	// If this ctx must die then it has our blessing.
	AkUInt32 refCnt = Release();

	// Auto-release if required and done, and the mgr is the only one holding on.
	if ( refCnt == 1 && m_bMustAutoRelease && !m_bDidAutoRelease && m_listDeviceEvents.IsEmpty() )
	{
		m_bDidAutoRelease = true;
		Release();
	}

	// Don't add any code after release (ctx may have died)!!
}

//-----------------------------------------------------------------------------
// Name: DetachAndRelease
// Desc: Detaches the ctx from the mgr.  Also, stops the ctx from generating
//			any more events; ctx attempts to destroy
//-----------------------------------------------------------------------------
void CAkMidiDeviceCtx::DetachAndRelease()
{
	if ( ! m_bWasStopped )
	{
		// Flag as stopped
		m_bWasStopped = true;

		// Cleanup all pending notes
		if( m_pMidiMgr )
		{
			m_pMidiMgr->CleanupActions( this );
			m_pMidiMgr->DetachCtx( this );
			m_pMidiMgr = NULL;
		}

		// Attempt to destroy (if not now, then will destroy once the last note is done)
		Release();
	}

	// Don't add any code after release (ctx may have died)!!
}
