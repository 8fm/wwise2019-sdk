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
// AkLEngineCmds.h
//
// Cross-platform lower engine commands management.
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_LENGINE_CMDS_H_
#define _AK_LENGINE_CMDS_H_

#include "AkList2.h"
#include <AK/Tools/Common/AkListBare.h>
// Warning: all platforms must implement a CAkVPLSrcCbxNode and a static CAkLEngine
// with the methods used in this class. TODO Move to a separate, cross-platform interface.
#include "AkVPLSrcCbxNode.h"

//-----------------------------------------------------------------------------
// Structs.
//-----------------------------------------------------------------------------
struct AkLECmd
{
	enum Type
	{
		Type_Play		= 0,
		Type_PlayPause	= 1,
		Type_Pause		= 2,
		Type_Resume		= 3,
		Type_StopLooping= 4,
		Type_Seek		= 5
	};

	CAkPBI *	m_pCtx;
    AkUInt32	m_ulSequenceNumber;		// For playback start synchronization; see CAkLEngineCmds::m_ulPlayEventID
	AkUInt32	m_eType : 8;			// AkLECmd::Type
	AkUInt32	m_bSourceConnected : 1;
};

typedef AkListBare<CAkVPLSrcCbxNode> AkListVPLSrcs;

//-----------------------------------------------------------------------------
// CAkLEngineCmds static class.
//-----------------------------------------------------------------------------

class CAkLEngineCmds
{
public:
	static AKRESULT Init();
	static void Term();

	// Behavioral engine interface
	static inline bool ProcessPlayCmdsNeeded() { return m_bProcessPlayCmdsNeeded; }
	static void ProcessPlayCommands();
	static void IncrementSyncCount(){ ++m_ulPlayEventID; };

	// Interface for CAkLEngine.
	static void ProcessAllCommands();
	static void ProcessDisconnectedSources( AkUInt32 in_uFrames );
	static void DestroyDisconnectedSources();
	static inline void AddDisconnectedSource(CAkVPLSrcCbxNode * in_pCbx) { m_listSrcsNotConnected.AddLast(in_pCbx); }
	static inline void RemoveDisconnectedSource(CAkVPLSrcCbxNode * in_pCbx) { AKVERIFY(m_listSrcsNotConnected.Remove(in_pCbx) == AK_Success); }

	// Interface for PBI/URenderer.
	static void DequeuePBI( CAkPBI* in_pPBI );
	static AKRESULT	EnqueueAction( AkLECmd::Type in_eType, CAkPBI * in_pContext );

private:
    static void ProcessPendingCommands();
	
private: 
	typedef CAkList2<AkLECmd, const AkLECmd&, AkAllocAndFree, AkMemID_Processing> AkListCmd;
	static AkListCmd			m_listCmd;					// List of command posted by the upper engine.
    static AkListVPLSrcs		m_listSrcsNotConnected;		// List of sounds not yet connected to a bus.	
	static AkUInt32				m_ulPlayEventID;			// Play event id.
	static bool					m_bProcessPlayCmdsNeeded;
};
#endif	//_AK_LENGINE_CMDS_H_
