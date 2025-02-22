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
// AkPathManager.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _PATH_MANAGER_H_
#define _PATH_MANAGER_H_

#include <AK/Tools/Common/AkArray.h>
#include "AkSharedEnum.h"

class CAkPath;
struct AkPathListItem;
struct AkPathState;
class CAkBehavioralCtx;

class CAkPathManager
{
public:
	CAkPathManager();
	~CAkPathManager();

	AKRESULT Init( AkUInt32 in_uMaxNumPath );
	void Term();

	// adds a path to the active list
	CAkPath* AddPathToList();

	// removes a path from the active list
	AKRESULT RemovePathFromList(
		CAkPath*	in_pPath
		);

	// adds a sound to the list
	AKRESULT AddPathUser(
		CAkPath*	in_pPath,
		CAkBehavioralCtx*		in_pNode
		);

	// removes an ITransitionable from the list
	AKRESULT RemovePathUser(
		CAkPath*	in_pPath,
		CAkBehavioralCtx*		in_pNode
		);

	// increments the number of potential users
	AKRESULT AddPotentialUser(
		CAkPath* in_pPath
		);

	// decrements the number of potential users
	AKRESULT RemovePotentialUser(
		CAkPath* in_pPath
		);

	// sets the play list
	AKRESULT SetPathsList(
		CAkPath*		in_pPath,
		AkPathListItem*	in_pPathList,
		AkUInt32		in_ulListSize,
		AkPathMode		in_PathMode,
		bool			in_bIsLooping,
		AkPathState*	in_pState
		);

	void ProcessPathsList(
		AkUInt32 in_uCurrentBufferTick
		);

	// start it
	AKRESULT Start(
		CAkPath*	in_pPath,
		AkPathState* in_pState
		);

	// stop it
	AKRESULT Stop(
		CAkPath*	in_pPath
		);

	// pause it
	AKRESULT Pause(
		CAkPath*	in_pPath
		);

	// resume it
	AKRESULT Resume(
		CAkPath*	in_pPath
		);

private:
	// the paths we manage
	typedef AkArray<CAkPath*, CAkPath*, ArrayPoolDefault, AkGrowByPolicy_NoGrow> AkPathList;
	AkPathList			m_ActivePathsList;
	AkUInt32			m_uMaxPathNumber;
};

extern CAkPathManager*	g_pPathManager;
#endif
