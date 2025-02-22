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
// AkContextualMusicSequencer.cpp
//
// Action sequencer for music contexts.
// Holds a list of pending musical actions, stamped with sample-based
// timing. 
// For example, a sequence context would enqueue actions on children 
// nodes that are scheduled to play or stop in the near future.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AkContextualMusicSequencer.h"

CAkMusicActionSequencer::CAkMusicActionSequencer()
{
}

CAkMusicActionSequencer::~CAkMusicActionSequencer()
{
	AKASSERT( IsEmpty() );
	Term();
}

// Returns a pointer to the action enqueued if successful, NULL otherwise.
void CAkMusicActionSequencer::ScheduleAction( 
    AkMusicAction * in_pAction			// Action to be scheduled. Allocated by user.
    )
{
    // Insert action chronologically.
    AkInt32 iTime = in_pAction->Time();
    CAkMusicActionSequencer::IteratorEx it = BeginEx();
    while ( it != End() )
    {
        if ( iTime < (*it)->Time() )
        {
            Insert( it, in_pAction );
			return;
        }
        ++it;
    }
	// Add at the end of the list.
    AddLast( in_pAction );
}

// Returns AK_NoMoreData when there is no action to be executed in next frame (out_action is invalid).
// Otherwise, returns AK_DataReady.
AKRESULT CAkMusicActionSequencer::PopImminentAction(
	AkInt32 in_iNow,					// Current time.
    AkInt32 in_iFrameDuration,			// Number of samples to process.
	AkMusicAction *& out_pAction		// Returned action. Freed by user.
    )
{
	if ( First() )
    {
        AKASSERT( First()->Time() >= in_iNow ||
            !"Action should have been executed in the past" );
        if ( First()->Time() <= in_iNow + in_iFrameDuration )
        {
			out_pAction = First();
            RemoveFirst();
            return AK_DataReady;
        }
    }
	out_pAction = NULL;
    return AK_NoMoreData;
}

// Remove all actions from sequencer (ref counted targets are released).
void CAkMusicActionSequencer::Flush()
{
    // Remove and delete all actions.
    while ( !IsEmpty() )
    {
		AkMusicAction * pAction = First();
		RemoveFirst();
		AkDelete(AkMemID_Object, pAction );
    }
}

//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-

// Removes from sequencer and frees all actions that reference the specified PBI. 
void CAkMusicSubTrackSequencer::ClearSubTrackCtx( CAkSubTrackCtx* in_pSubTrackCtx )
{
	CAkMusicSubTrackSequencer::IteratorEx it = BeginEx();
	while ( it != End() )
	{
		if ( (*it)->Type() == MusicActionTypeStop 
			&& (static_cast<AkMusicActionStopSubTrack*>(*it))->SubTrackCtx() == in_pSubTrackCtx )
		{
			AkMusicAction* pAction = (*it);
			it = Erase( it );
			AkDelete(AkMemID_Object, pAction );
		}
		else
			++it;
	}
}

//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-

// Removes from sequencer and frees all actions that reference the specified PBI. 
void CAkMusicClipSequencer::ClearTargetCtx( CAkChildCtx* in_pTarget )
{
	CAkMusicClipSequencer::IteratorEx it = BeginEx();
	while ( it != End() )
	{
		if ( (*it)->Type() == MusicActionTypeStop 
			&& (static_cast<AkMusicActionStopClip*>(*it))->pTargetPBI == in_pTarget )
		{
			AkMusicAction* pAction = (*it);
			it = Erase( it );
			AkDelete(AkMemID_Object, pAction );
		}
		else
			++it;
	}
}
