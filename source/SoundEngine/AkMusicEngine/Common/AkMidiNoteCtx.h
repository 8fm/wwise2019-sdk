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
// AkMidiNoteCtx.h
//
// Manages the playout context of a midi source.
//
//////////////////////////////////////////////////////////////////////
#ifndef _MIDI_ACTION_H_
#define _MIDI_ACTION_H_

#include "AkMusicStructs.h"
#include "AkChildCtx.h"
#include "AkPBIAware.h"
#include "AkMusicTrack.h"
#include "AkMusicNode.h"
#include "AkMidiBaseCtx.h"
#include "AkActionPlayAndContinue.h"

#include "AkMidiParseSe.h"
#include "AkList2.h"

struct AkTrackSrc;
class CAkUsageSlot;
class CAkMidiNoteEvent;
class CAkMidiBaseCtx;

template< class T >
struct MidiActionStruct
{
	MidiEventActionType eAction;
	T data;
};

template< class T>
class MidiActionList : public CAkList2< MidiActionStruct<T>, const MidiActionStruct<T>&, AkAllocAndKeep >
{
public:
};

// A AkSound node, associated with a midi note and channel that was resolved when we walked down the hierarchy.
struct TransformedSound
{
	TransformedSound(): pSound(NULL) {}
	TransformedSound( CAkSoundBase* in_pSound, const AkMidiNoteChannelPair& in_noteAndChannel ): 
			pSound(in_pSound), noteAndChannel(in_noteAndChannel) {}
	bool operator==( const TransformedSound& rhs ) 	{ return pSound == rhs.pSound && noteAndChannel == rhs.noteAndChannel; }
	CAkSoundBase* pSound;
	AkMidiNoteChannelPair noteAndChannel;
};

typedef MidiActionList< TransformedSound >				AkMidiNoteSoundList;
typedef MidiActionList< CAkPBI* >						AkMidiNotePBIList;
typedef MidiActionList< CAkActionPlayAndContinue* >		AkMidiNoteActionList;

class CAkMidiNoteState;

class CAkMidiNoteStateAware
{
public:
	virtual void OnNoteStateFinished( CAkMidiNoteState* in_pNote ) = 0;
};

class CAkMidiNoteState
{
public:
	CAkMidiNoteState(AkMidiEventEx in_event, CAkRegisteredObj* in_pGameObj): m_lRef( 1 )
		, m_MidiEvent(in_event)
		, m_pGameObj(in_pGameObj)
		, m_pNoteAware(NULL) 
#ifndef AK_OPTIMIZED
		, m_pSourceCtx(NULL)
#endif
	{}

	~CAkMidiNoteState();

	AKRESULT Init();

	AkUInt32 AddRef();
	AkUInt32 Release();

	AKRESULT AttachSound( MidiEventActionType in_eNoteOffAction, CAkSoundBase* in_pSound, const AkMidiNoteChannelPair& in_noteAndChannel )		
	{
		AKRESULT res = Attach(in_eNoteOffAction, TransformedSound(in_pSound,in_noteAndChannel), m_sounds);
		if (res == AK_Success)
			in_pSound->AddRef();
		return res;
	}

	AKRESULT DetachSound( CAkSoundBase* in_pNode, const AkMidiNoteChannelPair& in_noteAndChannel  )		
	{
		AKRESULT res = Detach(TransformedSound(in_pNode,in_noteAndChannel), m_sounds);
		if (res == AK_Success)
			in_pNode->Release();
		return res;
	}

	AKRESULT AttachPBI( MidiEventActionType in_eNoteOffAction, CAkPBI* in_pPBI )						
	{
		return Attach(in_eNoteOffAction, in_pPBI, m_PBIs);
	}

	AKRESULT DetachPBI( CAkPBI* in_pPBI )																
	{
		AKRESULT res = Detach(in_pPBI, m_PBIs);
		NotifyNoteAware();
		return res;
	}

	AKRESULT AttachAction( MidiEventActionType in_eNoteOffAction, CAkActionPlayAndContinue* in_pAction )
	{
		return Attach(in_eNoteOffAction, in_pAction, m_actions);
	}

	AKRESULT DetachAction( CAkActionPlayAndContinue* in_pAction )														
	{
		AKRESULT res = Detach(in_pAction, m_actions);
		NotifyNoteAware();
		return res;
	}

	CAkRegisteredObj* GetGameObj() const {return m_pGameObj;}
	
	const AkMidiNoteSoundList&	GetSoundList() const	{return m_sounds;}
	const AkMidiNotePBIList&	GetPBIList() const		{return m_PBIs;}
	const AkMidiNoteActionList&	GetActionList() const	{return m_actions;}

	bool IsFinished() const { return m_PBIs.IsEmpty() && m_actions.IsEmpty(); }

	void SetNoteStateAware( CAkMidiNoteStateAware* in_pAware ) { m_pNoteAware = in_pAware; }

protected:
	AkInt32 m_lRef;

private:
	
	template< class T >
	AKRESULT Attach( MidiEventActionType in_eAtion, T in_data, 
		MidiActionList<T> & io_list )
	{
		MidiActionStruct<T>* pmas = io_list.AddLast();
		if (pmas)
		{
			pmas->eAction = in_eAtion;
			pmas->data = in_data;
			return AK_Success;
		}
		return AK_Fail;
	}

	template< class T >
	AKRESULT Detach( T in_data, MidiActionList<T> & io_list )
	{
		typename MidiActionList<T>::IteratorEx it = io_list.BeginEx();
		while ( it != io_list.End() )
		{
			if ( (*it).data == in_data )
			{
				it = io_list.Erase( it );
				return AK_Success;
			}
			++it;
		}

		return AK_Success;
	}

	void NotifyNoteAware()
	{
		if( m_pNoteAware && IsFinished() )
			m_pNoteAware->OnNoteStateFinished( this );
	}

	AkMidiEventEx			m_MidiEvent;
	CAkRegisteredObj*		m_pGameObj;
	CAkMidiNoteStateAware*	m_pNoteAware;

	AkMidiNoteSoundList			m_sounds;
	AkMidiNotePBIList			m_PBIs;
	AkMidiNoteActionList		m_actions;


#ifndef AK_OPTIMIZED
public:
	//Backwards dependency - Only used for retrieving monitoring mute/solo state in the PBI.
	CAkMidiBaseCtx*  m_pSourceCtx;	
	void ReleaseMidiCtx();
#endif
};

class CAkMidiNoteEvent
{
public:

	CAkMidiNoteEvent( CAkMidiBaseCtx* in_pMidiCtx, CAkParameterNodeBase * in_pTargetNode );
	~CAkMidiNoteEvent();

	bool ScheduleMidiEvent(	const AkMidiEventEx& in_midiEvent, AkUInt32 in_uFrameOffset );

	inline void Stop( CAkRegisteredObj*, const UserParams& )	{ _ExecuteNoteOffAction();}
	inline void Pause( CAkRegisteredObj*, const UserParams& )	{ _ExecutePause();}
	inline void Resume( CAkRegisteredObj*, const UserParams& )	{ _ExecuteResume();}

	AkForceInline CAkParameterNodeBase * GetTargetNode() const { return m_pTargetNode; }
	AkForceInline CAkMidiBaseCtx* GetMidiCtx() const { return m_pMidiCtx; }

	AkUInt32 AddRef();
	AkUInt32 Release();

	CAkMidiNoteEvent*	pNextItem;	// For AkList

	inline const AkMidiEventEx& GetMidiEvent() const {return m_MidiEvent;}

	inline bool IsCcEvent() const { return m_MidiEvent.IsCcEvent(); }
	inline bool IsNoteOn() const { return m_MidiEvent.IsNoteOn(); }
	inline bool IsNoteOff() const { return m_MidiEvent.IsNoteOff(); }
	inline bool IsPitchBend() const { return m_MidiEvent.IsPitchBend(); }
	inline bool PlayExecuted() const { return m_bPlayExecuted; }
	inline bool ActionExecuted() const { return m_bActionExecuted; }
	inline bool NoteOffScheduled() const { return m_bNoteOffScheduled; }

	inline AkInt32 GetFrameOffset() { return m_iFrameOffset; }
	inline void SetFrameOffset( AkInt32 in_iFrameOffset ) { m_iFrameOffset = in_iFrameOffset; }

	//Returns true if event is complete and can be cleaned up.
	bool Update( AkUInt32 uElapsedFrames, bool in_bExecute );

	bool PairWithExistingNote( CAkMidiNoteEvent* pOldCtx );

	CAkRegisteredObj* GetGameObj() const { return m_pMidiCtx ? m_pMidiCtx->GetGameObj() : NULL; }
	CAkMidiNoteState* GetNoteState() const { return m_pNoteState; }

	void StopPBIsNoFade() const;

protected:
	AkInt32 m_lRef;

private:
	
	void _ExecutePlay( CAkRegisteredObj* in_pGameObj, const UserParams& in_UserParams );
	void _ExecuteNoteOffAction() const;
	void _ExecutePause() const;
	void _ExecuteResume() const;
	void PlayNode( CAkParameterNodeBase* in_pNodeToPlay, CAkParameterNodeBase* in_pMidiTarget, CAkRegisteredObj* in_pGameObj, const UserParams& in_UserParams, const AkMidiEventEx& in_midiEvent );
	void TriggerNoteOffMods( CAkRegisteredObj* in_pGameObj, const UserParams& in_userParams, CAkPBI* in_pCtx );

	CAkMidiNoteEvent();

	CAkMidiNoteState*		m_pNoteState;

	// Private to hide code for addref of target
	CAkMidiBaseCtx*			m_pMidiCtx;
	CAkParameterNodeBase*	m_pTargetNode;
	
	AkMidiEventEx			m_MidiEvent;
	AkInt32					m_iFrameOffset;
	
	bool					m_bPlayExecuted : 1;
	bool					m_bActionExecuted : 1;
	bool					m_bNoteOffScheduled : 1;
};



#endif
