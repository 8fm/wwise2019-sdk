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
// AkMidiNoteCtx.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkMidiNoteCtx.h"
#include "AkMidiBaseCtx.h"
#include "AkMidiStructs.h"
#include "AkAudioMgr.h"

extern CAkAudioMgr* g_pAudioMgr;

//
//  CAkMidiNoteState
//

// Define the minimum size of the list. 
//	(Ideally we want to avoid additional allocations after creating the list.)
#define MIDI_NOTE_DEFAULT_NUM_SOUNDS	4
#define MIDI_NOTE_DEFAULT_NUM_PBIS		4
#define MIDI_NOTE_DEFAULT_NUM_ACTIONS	0

AKRESULT CAkMidiNoteState::Init()
{
	AKRESULT res = m_sounds.Reserve(MIDI_NOTE_DEFAULT_NUM_SOUNDS);
	
	if (res == AK_Success)
		res = m_PBIs.Reserve(MIDI_NOTE_DEFAULT_NUM_PBIS);
	
	if (res == AK_Success)
		res = m_actions.Reserve(MIDI_NOTE_DEFAULT_NUM_ACTIONS);
	
	return res;
}

CAkMidiNoteState::~CAkMidiNoteState()
{
	AKASSERT( !m_lRef );
	AKASSERT( !m_pNoteAware );

	for( AkMidiNoteSoundList::Iterator it = m_sounds.Begin(); it != m_sounds.End(); ++it )
	{
		MidiActionStruct<TransformedSound>& midiAction= *it;
		midiAction.data.pSound->Release();
	}

	m_sounds.Term();
	m_PBIs.Term();
	m_actions.Term();

#ifndef AK_OPTIMIZED
	if (m_pSourceCtx)
		m_pSourceCtx->Release();
#endif
}

AkUInt32 CAkMidiNoteState::AddRef()
{
	return ++m_lRef;
}

AkUInt32 CAkMidiNoteState::Release()
{
	AkInt32 lRef = --m_lRef;
	AKASSERT( lRef >= 0 ); 
	if ( !lRef ) 
		AkDelete(AkMemID_Object, this);;
	return lRef; 
}

#ifndef AK_OPTIMIZED
void CAkMidiNoteState::ReleaseMidiCtx()
{
	if (m_pSourceCtx)
	{
		m_pSourceCtx->Release();
		m_pSourceCtx = NULL;
	}
}
#endif

//
//	CAkMidiNoteEvent
//
CAkMidiNoteEvent::CAkMidiNoteEvent( CAkMidiBaseCtx* in_pMidiCtx, CAkParameterNodeBase * in_pTargetNode )
	: m_lRef(1)
	, m_pNoteState(NULL)
	, m_pMidiCtx( in_pMidiCtx )
	, m_pTargetNode( in_pTargetNode )
	, m_bPlayExecuted(false)
	, m_bActionExecuted(false)
	, m_bNoteOffScheduled(false)
{
	AKASSERT( in_pMidiCtx && in_pTargetNode );

	if ( m_pMidiCtx )
		m_pMidiCtx->AddRef();
	if ( m_pTargetNode )
		m_pTargetNode->AddRef();
}

CAkMidiNoteEvent::~CAkMidiNoteEvent()
{
	AKASSERT(!m_lRef);

	if ( m_pMidiCtx )
		m_pMidiCtx->Release();

	if ( m_pTargetNode )
		m_pTargetNode->Release();

	if ( m_pNoteState )
		m_pNoteState->Release();
}

AkUInt32 CAkMidiNoteEvent::AddRef()
{
	return ++m_lRef;
} 
AkUInt32 CAkMidiNoteEvent::Release()
{
	AkInt32 lRef = --m_lRef;
	AKASSERT( lRef >= 0 ); 
	if (!lRef)
		AkDelete(AkMemID_Object, this);
	return lRef; 
}

bool CAkMidiNoteEvent::ScheduleMidiEvent( const AkMidiEventEx& in_midiEvent, AkUInt32 in_uFrameOffset )
{
	bool res = true;

	m_bActionExecuted = false;

	if ( in_midiEvent.IsNoteOn() )
	{
		AKASSERT(m_pNoteState == NULL);
		m_pNoteState = AkNew(AkMemID_Object, CAkMidiNoteState(in_midiEvent, m_pMidiCtx->GetGameObj()));
		if ( ! m_pNoteState || m_pNoteState->Init() != AK_Success )
			res = false;
		else
		{
#ifndef AK_OPTIMIZED
			m_pNoteState->m_pSourceCtx = m_pMidiCtx;
			m_pNoteState->m_pSourceCtx->AddRef();
#endif
		}
	}
	else if ( in_midiEvent.IsNoteOff() )
	{	
		m_bNoteOffScheduled = true;
	}
	else
	{
		// To ensure note-on/note-off code is not executed (i.e. cc event).
		m_bPlayExecuted = true;
		m_bActionExecuted = true;
		m_bNoteOffScheduled = true;
	}

	m_MidiEvent = in_midiEvent;
	m_iFrameOffset = in_uFrameOffset;

	return res;
}

bool CAkMidiNoteEvent::PairWithExistingNote( CAkMidiNoteEvent* pOldCtx )
{
	AkMidiEventEx& oldMidiEvent = pOldCtx->m_MidiEvent;

	if ( m_MidiEvent.IsSameChannelAndNote( oldMidiEvent ) && ! pOldCtx->m_bNoteOffScheduled )
	{
		if (m_MidiEvent.IsNoteOff() && oldMidiEvent.IsNoteOn())
		{
			//Inherit velocity from old note on
			m_MidiEvent.byType = AK_MIDI_EVENT_TYPE_NOTE_OFF;
			m_MidiEvent.NoteOnOff.byVelocity = pOldCtx->m_MidiEvent.NoteOnOff.byVelocity;
			AKASSERT(m_pNoteState == NULL);
			m_pNoteState = pOldCtx->m_pNoteState;
			m_pNoteState->AddRef();
		}

		pOldCtx->m_bNoteOffScheduled = m_MidiEvent.IsNoteOff();

		return true;
	}

	return false;
}

bool CAkMidiNoteEvent::Update( AkUInt32 uElapsedFrames, bool in_bExecute )
{
	bool result = true;

	if ( IsNoteOn() || IsNoteOff() )
	{
		if (GetGameObj() && in_bExecute)
		{
			// !!IMPORTANT!!  Be sure to do the note-off action BEFORE the note-on;
			//		The order is important for operations done on associated modulators
			//		(envelopes need to have release triggered before new ctx are created).
			if (!m_bActionExecuted && m_iFrameOffset < (AkInt32)AK_NUM_VOICE_REFILL_FRAMES)
			{
				if (m_MidiEvent.IsNoteOff())
					_ExecuteNoteOffAction();

				m_bActionExecuted = true;
			}

			if (!m_bPlayExecuted)
			{
				_ExecutePlay( m_pMidiCtx->GetGameObj(), m_pMidiCtx->GetUserParams() );
				m_bPlayExecuted = true;
			}
		}

		m_iFrameOffset = AkMax(0,m_iFrameOffset-(AkInt32)uElapsedFrames);

		result = ( m_bActionExecuted && m_bNoteOffScheduled );
	}

	return result;
}

//-----------------------------------------------------------------------------
// Name: ExecuteAction
// Desc: Executes play.
//-----------------------------------------------------------------------------
void CAkMidiNoteEvent::_ExecutePlay( CAkRegisteredObj* in_pGameObj, const UserParams& in_UserParams )
{
	AKASSERT(m_pNoteState);

	if ( m_MidiEvent.IsNoteOn() )
	{	
		PlayNode( GetTargetNode(), GetTargetNode(), in_pGameObj, in_UserParams, m_MidiEvent );
	}
	else
	{
		AKASSERT( IsNoteOff() );

		// Check for note-off sounds that must be played
		for( AkMidiNoteSoundList::Iterator it = m_pNoteState->GetSoundList().Begin(); it != m_pNoteState->GetSoundList().End(); ++it )
		{
			MidiActionStruct<TransformedSound>& midiAction= *it;

			//Apply the same transformation to the event that was applied when we resolved the node.
			if (midiAction.eAction == MidiEventActionType_Play)
			{
				AkMidiEventEx transformedMidiEvent = m_MidiEvent;
				transformedMidiEvent.byChan = midiAction.data.noteAndChannel.channel;
				transformedMidiEvent.NoteOnOff.byNote = midiAction.data.noteAndChannel.note;

				PlayNode(midiAction.data.pSound, GetTargetNode(), in_pGameObj, in_UserParams, transformedMidiEvent);
			}
		}

		// Check for note-on sounds with no stop action: they may need to trigger modulators
		for( AkMidiNotePBIList::Iterator it = m_pNoteState->GetPBIList().Begin(); it != m_pNoteState->GetPBIList().End(); ++it )
		{
			MidiActionStruct<CAkPBI*>& midiAction= *it;

			if (midiAction.eAction != MidiEventActionType_Play)
			{
				TriggerNoteOffMods( in_pGameObj, in_UserParams, midiAction.data );
			}
		}
	}
}

void CAkMidiNoteEvent::PlayNode( CAkParameterNodeBase* in_pNodeToPlay, CAkParameterNodeBase* in_pMidiTarget, CAkRegisteredObj* in_pGameObj, const UserParams& in_UserParams, const AkMidiEventEx& in_midiEvent )
{
	AKASSERT( IsNoteOn() || IsNoteOff() );
	AKASSERT( in_pNodeToPlay->Category() != ObjCategory_Bus );
	AKASSERT( in_pMidiTarget->Category() != ObjCategory_Bus );

	if( in_pNodeToPlay->Category() != ObjCategory_Bus )
	{
		AkPBIParams pbiParams;

		pbiParams.eType = AkPBIParams::PBI;
		pbiParams.pInstigator = in_pMidiTarget;
		pbiParams.sequenceID = AK_INVALID_SEQUENCE_ID;
		pbiParams.userParams = in_UserParams;
		pbiParams.ePlaybackState = PB_Playing;
		pbiParams.uFrameOffset = (AkUInt32)m_iFrameOffset;
		pbiParams.bIsFirst = true;
		pbiParams.pGameObj = in_pGameObj;
		pbiParams.pContinuousParams = NULL;

#ifndef AK_OPTIMIZED
		if( m_pMidiCtx )
			pbiParams.playTargetID = m_pMidiCtx->GetPlayTargetID( AK_INVALID_UNIQUE_ID );
#endif

		TransParams transParams;
		transParams.TransitionTime = 0;
		transParams.eFadeCurve = AkCurveInterpolation_Linear;
		pbiParams.pTransitionParameters = &transParams;

		pbiParams.midiEvent = in_midiEvent;
		pbiParams.pMidiNoteState = m_pNoteState;

		static_cast<CAkParameterNode*>( in_pNodeToPlay )->Play( pbiParams );
		
		CAkParameterNode::IncrementLESyncCount();
	}
}

void CAkMidiNoteEvent::TriggerNoteOffMods( CAkRegisteredObj* in_pGameObj, const UserParams& in_userParams, CAkPBI* in_pCtx )
{
	if( in_pCtx->GetSound() )
	{
		AkMidiEventEx transformedMidiEvent = m_MidiEvent;
		transformedMidiEvent.byChan = in_pCtx->GetMidiEvent().byChan;
		transformedMidiEvent.NoteOnOff.byNote = in_pCtx->GetMidiEvent().NoteOnOff.byNote;

		// Trigger the modulators
		AkModulatorTriggerParams triggerParams;
		triggerParams.midiEvent			= transformedMidiEvent;
		triggerParams.pMidiNoteState	= in_pCtx->GetMidiNoteState();
		triggerParams.midiTargetId		= in_pCtx->GetMidiTargetID();
		triggerParams.pGameObj			= in_pGameObj;
		triggerParams.uFrameOffset		= (AkUInt32)m_iFrameOffset;
		triggerParams.playingId			= in_userParams.PlayingID();
		triggerParams.eTriggerMode		= AkModulatorTriggerParams::TriggerMode_EndOfNoteOn;
		triggerParams.pPbi				= in_pCtx;

		in_pCtx->GetSound()->TriggerModulators( triggerParams, &in_pCtx->GetModulatorData() );
	}
}

//-----------------------------------------------------------------------------
// Name: ExecuteAction
// Desc: Executes stop, pause or resume action for a note off event
//-----------------------------------------------------------------------------
void CAkMidiNoteEvent::_ExecuteNoteOffAction() const
{
	AKASSERT( g_pAudioMgr );
	AKASSERT( GetTargetNode() );
	AKASSERT( m_pNoteState );
	
	for( AkMidiNotePBIList::Iterator it = m_pNoteState->GetPBIList().Begin(); it != m_pNoteState->GetPBIList().End(); ++it )
	{
		MidiActionStruct<CAkPBI*>& midiAction= *it;

		CAkPBI* pPbi = midiAction.data;
		AKASSERT(pPbi != NULL);

		// Get stop offset; this may be overriden if we want to stop immediately.
		AkUInt32 uStopOffset = m_iFrameOffset;

		//
		// Release all the envelopes that are associated with the PBI
		//	NOTE: When we add support differently-scoped envelopes, we will need to 
		//		make sure that we only release note/container scoped modulators.  For now,
		//		all envelopes are note scoped.
		pPbi->GetModulatorData().TriggerRelease( uStopOffset );

		// If context has been set to absolute stop, then we want to end ASAP!
		if( m_pMidiCtx->GetAbsoluteStop() )
		{
			midiAction.eAction = MidiEventActionType_Stop;
			uStopOffset = AK_NO_IN_BUFFER_STOP_REQUESTED;
		}

		switch (midiAction.eAction)
		{
		case MidiEventActionType_Stop:
			{
				pPbi->SetStopOffset(uStopOffset);

				TransParams transParams;
				transParams.TransitionTime = 0;
				transParams.eFadeCurve = AkCurveInterpolation_Linear;

				pPbi->_Stop(transParams, uStopOffset == AK_NO_IN_BUFFER_STOP_REQUESTED);
				break;
			}
		case MidiEventActionType_Break:
			{
				pPbi->PlayToEnd(m_pTargetNode);
				break;
			}
		default:
			break;
		}
	}

	for( AkMidiNoteActionList::Iterator it = m_pNoteState->GetActionList().Begin(); it != m_pNoteState->GetActionList().End(); ++it )
	{
		MidiActionStruct<CAkActionPlayAndContinue*>& midiAction= *it;
		g_pAudioMgr->MidiNoteOffExecuted( midiAction.data, m_pTargetNode, midiAction.eAction );
	}
}

//-----------------------------------------------------------------------------
// Name: StopPBIsNoFade
// Desc: Must be called on a note-off.  The note-off action has been performed
//		before.  This function is called to stop PBI(s), possibly again, but
//		with NO FADE.
//-----------------------------------------------------------------------------
void CAkMidiNoteEvent::StopPBIsNoFade() const
{
	AKASSERT( g_pAudioMgr );
	AKASSERT( GetTargetNode() );
	AKASSERT( m_pNoteState );

	for( AkMidiNotePBIList::Iterator it = m_pNoteState->GetPBIList().Begin(); it != m_pNoteState->GetPBIList().End(); ++it )
	{
		MidiActionStruct<CAkPBI*>& midiAction= *it;

		CAkPBI* pPbi = midiAction.data;
		AKASSERT(pPbi != NULL);

		pPbi->SetStopOffset( AK_NO_IN_BUFFER_STOP_REQUESTED );

		TransParams transParams;
		transParams.TransitionTime = 0;
		transParams.eFadeCurve = AkCurveInterpolation_Linear;

		pPbi->_Stop(transParams, true);
	}
}

//-----------------------------------------------------------------------------
// Name: _ExecutePause
// Desc: Executes pause for a note.
//-----------------------------------------------------------------------------
void CAkMidiNoteEvent::_ExecutePause() const
{
	AKASSERT( g_pAudioMgr );
	AKASSERT( GetTargetNode() );
	AKASSERT( m_pNoteState );

	for( AkMidiNotePBIList::Iterator it = m_pNoteState->GetPBIList().Begin(); it != m_pNoteState->GetPBIList().End(); ++it )
	{
		MidiActionStruct<CAkPBI*>& midiAction= *it;

		CAkPBI* pPbi = midiAction.data;
		AKASSERT(pPbi != NULL);

		TransParams transParams;
		transParams.eFadeCurve = AkCurveInterpolation_Linear;
		transParams.TransitionTime = 0;

		pPbi->_Pause( transParams );
	}

	for( AkMidiNoteActionList::Iterator it = m_pNoteState->GetActionList().Begin(); it != m_pNoteState->GetActionList().End(); ++it )
	{
		MidiActionStruct<CAkActionPlayAndContinue*>& midiAction= *it;
		g_pAudioMgr->PausePendingAction( midiAction.data );
	}
}

//-----------------------------------------------------------------------------
// Name: _ExecuteResume
// Desc: Executes resume for a note.
//-----------------------------------------------------------------------------
void CAkMidiNoteEvent::_ExecuteResume() const
{
	AKASSERT( g_pAudioMgr );
	AKASSERT( GetTargetNode() );
	AKASSERT( m_pNoteState );

	for( AkMidiNotePBIList::Iterator it = m_pNoteState->GetPBIList().Begin(); it != m_pNoteState->GetPBIList().End(); ++it )
	{
		MidiActionStruct<CAkPBI*>& midiAction= *it;

		CAkPBI* pPbi = midiAction.data;
		AKASSERT(pPbi != NULL);

		TransParams transParams;
		transParams.eFadeCurve = AkCurveInterpolation_Linear;
		transParams.TransitionTime = 0;

		pPbi->_Resume( transParams, false );
	}

	for( AkMidiNoteActionList::Iterator it = m_pNoteState->GetActionList().Begin(); it != m_pNoteState->GetActionList().End(); ++it )
	{
		MidiActionStruct<CAkActionPlayAndContinue*>& midiAction= *it;
		g_pAudioMgr->ResumePausedPendingAction( midiAction.data );
	}
}
