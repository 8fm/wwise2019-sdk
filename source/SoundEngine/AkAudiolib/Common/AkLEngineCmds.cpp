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

#include <AK/SoundEngine/Common/AkSoundEngine.h>

#include "AkLEngineCmds.h"
#include "AkMonitor.h"
#include "AkPBI.h"
#include "AkLEngine.h"

//-----------------------------------------------------------------------------
// Defines.
//-----------------------------------------------------------------------------

#define MIN_NUM_LE_COMMANDS         512 // Commands may hang in the list for a while.

//-----------------------------------------------------------------------------
//Static variables.
//-----------------------------------------------------------------------------
AkListVPLSrcs				CAkLEngineCmds::m_listSrcsNotConnected;
AkUInt32					CAkLEngineCmds::m_ulPlayEventID		= 0;
CAkLEngineCmds::AkListCmd	CAkLEngineCmds::m_listCmd;
bool						CAkLEngineCmds::m_bProcessPlayCmdsNeeded = false;

AKRESULT CAkLEngineCmds::Init()
{
	m_ulPlayEventID	= 0;
	return m_listCmd.Reserve( MIN_NUM_LE_COMMANDS );
}

void CAkLEngineCmds::Term()
{
	m_listSrcsNotConnected.Term();
	m_listCmd.Term();
}

//-----------------------------------------------------------------------------
// Name: DequeuePBI
// Desc: Destroy the specified PBI.
//-----------------------------------------------------------------------------
void CAkLEngineCmds::DequeuePBI( CAkPBI* in_pPBI )
{
	AkListCmd::IteratorEx iter = m_listCmd.BeginEx();
	while ( iter != m_listCmd.End() )
	{
		AkLECmd & event = *iter;
		if( event.m_pCtx == in_pPBI )
		{
			// Remove all events that are associated with this context
			iter = m_listCmd.Erase( iter );
		}
		else
		{
			++iter;
		}
	}
} // DequeuePBI

AKRESULT CAkLEngineCmds::EnqueueAction( AkLECmd::Type in_eType, CAkPBI * in_pContext )
{
	AkLECmd * pCmd = m_listCmd.AddLast();
	if ( !pCmd )
	{
		MONITOR_ERROR( AK::Monitor::ErrorCode_LowerEngineCommandListFull );
		return AK_Fail;
	}

	pCmd->m_pCtx = in_pContext;
	pCmd->m_ulSequenceNumber = m_ulPlayEventID;
	pCmd->m_eType = in_eType;
	pCmd->m_bSourceConnected = false;

	if ( in_eType == AkLECmd::Type_Play || in_eType == AkLECmd::Type_PlayPause )
		m_bProcessPlayCmdsNeeded = true;

	return AK_Success;
}

//-----------------------------------------------------------------------------
// Name: ProcessAllCommands
// Desc: Execute the list of commands posted by the game.
//-----------------------------------------------------------------------------
void CAkLEngineCmds::ProcessAllCommands()
{
	// First pass. Add sounds for Play|PlayPaused commands newly added.
	if ( m_bProcessPlayCmdsNeeded )
		ProcessPlayCommands();
	
	// Now, execute all pending commands
	ProcessPendingCommands();
}

//-----------------------------------------------------------------------------
// Name: ProcessPlayCommands
// Desc: Process only play commands of the command list.
//-----------------------------------------------------------------------------
void CAkLEngineCmds::ProcessPlayCommands()
{
	WWISE_SCOPED_PROFILE_MARKER("CAkLEngineCmds::ProcessPlayCommands");

	AKASSERT( ProcessPlayCmdsNeeded() );

	// NOTE: Commands are enqueued with m_bSourceConnected = false
	// Then, for Play|PlayPaused commands, AddSound() is called, which creates the cbx,
	// and IF THE SOURCE IS READY, connects it to a bus and sets m_bSourceConnected.
	// ProcessCommands uses these fields to see if commands can be executed:
	// If cbx is created but !m_bSourceConnected, it tries to connect the source.
	// If it is a pause/resume/stop command, it tries to find the connected source
	// and resolves the command if it is present.
	// In all cases, commands are executed if and only if m_bSourceConnected is set.

	AkListCmd::IteratorEx iter = m_listCmd.BeginEx();
	while( iter != m_listCmd.End() )
	{
		AkLECmd&  l_rCmd = *iter;

		// Add sound if it does not exist and a Play or PlayPaused was queued.
		if( !l_rCmd.m_pCtx->GetCbx() &&
			( l_rCmd.m_eType == AkLECmd::Type_Play ||
			l_rCmd.m_eType == AkLECmd::Type_PlayPause ) )
		{
			if ( l_rCmd.m_pCtx->WasStopped() )
			{
				// play+stopped sound: cancel the whole thing immediately.
				CAkPBI * pCtx = l_rCmd.m_pCtx;
				iter = m_listCmd.Erase( iter );
				pCtx->Destroy( CtxDestroyReasonPlayFailed );	
			}
			else if ( CAkLEngine::AddSound( l_rCmd ) != AK_Success )
			{
				// Add sound failed. Source was destroyed. Remove from list.
				iter = m_listCmd.Erase( iter );
			}
			else
				++iter;
		}
		else
			++iter;
	}

	m_bProcessPlayCmdsNeeded = false;
}

//-----------------------------------------------------------------------------
// Name: ProcessPendingCommands
// Desc: Process the commands of the commands list.
// Note: When this function is called, commands in the list must be sorted by 
//       their sequence number.
//       Play and PlayPaused commands must have their cbx set with a previous 
//       AddSound(). However, they might not be connected yet (m_bSourceConnected == false).
//       Connection will be attempted for unconnected Play|Play_Paused commands.
//       The source of all Play|Play_Paused commands that have the same sequence 
//       number must be connected before the command is actually executed.
//       All other commands must have their source connected before being 
//       executed. These commands are resolved to their VPL and VPLSrc herein,
//       when it is appropriate to do so.       
//       Order of commands with the same sequence number matters.
// Note: It is not possible that other commands be enqueued before the 
//       play, because the upper engine does not send them when the PBI does
//       not exist. However, it should not be assumed at this level. Fix.
//-----------------------------------------------------------------------------
void CAkLEngineCmds::ProcessPendingCommands()
{
	WWISE_SCOPED_PROFILE_MARKER("CAkLEngineCmds::ProcessPendingCommands");

StartOverProcCmds:
	AkListCmd::IteratorEx iter = m_listCmd.BeginEx();
	while( iter != m_listCmd.End() )
	{
		// Verify that all VPLs with this sequence number are ready before executing any Play|Play_paused command.
		bool bDoSkipSeqNumber = false;
		AkUInt32 ulCurSeqNumber = (*iter).m_ulSequenceNumber;
		AkListCmd::IteratorEx iterProc = iter;

		while ( iterProc != m_listCmd.End( ) &&
			ulCurSeqNumber == (*iterProc).m_ulSequenceNumber )
		{
			AkLECmd& l_rItemCheck = *iterProc;

			// Check for Play commands with sources not ready.
			if ( !l_rItemCheck.m_bSourceConnected &&
				( l_rItemCheck.m_eType == AkLECmd::Type_Play ||
				l_rItemCheck.m_eType == AkLECmd::Type_PlayPause ) )
			{
				// Not ready.
				// Try connect source now. If source becomes ready, it will be connected therein and
				// l_rItemCheck.m_bSourceConnected will be set.
				// DO NOT try to connect source if it was stopped.
				if ( !l_rItemCheck.m_pCtx->WasStopped() )
				{
					AKRESULT eResult = AK_Fail;
					CAkVPLSrcCbxNode * pCbx = l_rItemCheck.m_pCtx->GetCbx();
					AKASSERT( pCbx );
					if (pCbx)// WG-31674 - Bug reported, we never reproed but we decided to add this if() since 2 users reported the same unexplainable callstack leading to pCbx being NULL here.
					{
						eResult = CAkLEngine::VPLTryConnectSource(l_rItemCheck.m_pCtx, pCbx);
					}
					if ( eResult == AK_Fail )
					{
						// Failure: Source was destroyed. Remove from event list.
						// Clean command lists for that context.
						m_listCmd.Erase( iterProc );
						// Start over.
						goto StartOverProcCmds;
					}
					else if ( eResult == AK_FormatNotReady )
					{
						// Found a source not ready to play. Skip that sequence number.
						bDoSkipSeqNumber = true;
					}
					else // source is ready
					{
						l_rItemCheck.m_bSourceConnected = true;
					}
				}
				// Else source is stopped and is about to be cleaned out. Ignore.
			}
			++iterProc;
 		}

		if ( bDoSkipSeqNumber )
		{
			// The sequence number must be skipped. Find next command that has a different sequence number.
			do
			{
				++iter;
			}
			while ( iter != m_listCmd.End( ) &&
				ulCurSeqNumber == (*iter).m_ulSequenceNumber );
		}
		else
		{
			// Execute command if the source is connected.
			AkLECmd & l_rItem = *iter;
			bool l_bIsCmdValid = true;

			CAkVPLSrcCbxNode * pCbx = NULL;

			// Resolve stop, pause and resume commands that are not yet identified to a VPL source.
			if ( !l_rItem.m_bSourceConnected )
			{
				if ( l_rItem.m_eType != AkLECmd::Type_Play 
					&& l_rItem.m_eType != AkLECmd::Type_PlayPause )
				{
					pCbx = CAkLEngine::ResolveCommandVPL( l_rItem );
					if ( !pCbx )
					{
						iter = m_listCmd.Erase( iter );
						l_bIsCmdValid = false;
					}
				}
			}
			else
			{
				pCbx = l_rItem.m_pCtx->GetCbx();
			}

			// Do not consider the command if !l_bIsCmdValid; it was removed
			if ( l_bIsCmdValid )
			{
				// Execute command if source is connected. Otherwise, skip it.
				if ( l_rItem.m_bSourceConnected )
				{
					// Source is connected. Execute command.
					switch( l_rItem.m_eType )
					{
					case AkLECmd::Type_Play:
						pCbx->Start();
						break;

					case AkLECmd::Type_PlayPause:
						pCbx->Start();
						pCbx->Pause();
						break;

					case AkLECmd::Type_Pause:
						pCbx->Pause();
						break;

					case AkLECmd::Type_Resume:
						pCbx->Resume();
						break;

					case AkLECmd::Type_StopLooping:
						pCbx->StopLooping( l_rItem.m_pCtx );
						break;

					case AkLECmd::Type_Seek:
						pCbx->SeekSource();
						break;

					default:
						AKASSERT(!"Unsupported action type");
						break;
					}

					// Command executed: dequeue.
					iter = m_listCmd.Erase( iter );
				}
				else
				{
					// Source is not ready. Skip command (leave it in the queue).
					++iter;
				} // if source ready/connected.
			}
		} // if do skip seq number.
	} // for all commands.
} // PerformCommands

void CAkLEngineCmds::ProcessDisconnectedSources( AkUInt32 in_uFrames )
{
	// Consume one look-ahead frame of each source that is not yet connected.
	AkListVPLSrcs::IteratorEx it = m_listSrcsNotConnected.BeginEx();
	while ( it != m_listSrcsNotConnected.End() )
	{
		CAkVPLSrcCbxNode * pCbx = *it;
		CAkPBI *pPBI = pCbx->GetPBI();

		if( pPBI->WasStopped() )
		{
			// Delete any pending commands that refer to this VPL source.
			AkListCmd::IteratorEx iterCmd = m_listCmd.BeginEx();
			while( iterCmd != m_listCmd.End() )
			{
				AkLECmd& l_rCmd = *iterCmd;

				if( l_rCmd.m_pCtx == pCbx->GetContext() )
					iterCmd = m_listCmd.Erase( iterCmd );
				else
					++iterCmd;
			}

			it = m_listSrcsNotConnected.Erase( it );

			// Destroy the source.
			CAkLEngine::VPLDestroySource( pCbx );
		}
		else
		{
			if( !pPBI->WasPaused() )
			{
				pPBI->ConsumeFrameOffset( pPBI->ScaleFrameSize( in_uFrames ) );
			}
			++it;
		}
	}
}

void CAkLEngineCmds::DestroyDisconnectedSources()
{
	for (AkListVPLSrcs::IteratorEx it = m_listSrcsNotConnected.BeginEx(); it != m_listSrcsNotConnected.End() ; )
	{
		CAkVPLSrcCbxNode * pCbx = *it;
		it = m_listSrcsNotConnected.Erase( it );
		CAkLEngine::VPLDestroySource( pCbx );
	}
}
