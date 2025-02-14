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
// AkVPLVolAutmNode.h.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_VPL_VOL_AUTM_NODE_H_
#define _AK_VPL_VOL_AUTM_NODE_H_

#include "AkVPLNode.h"

class CAkVPLVolAutmNode : public CAkVPLNode
{
public:
	CAkVPLVolAutmNode(): m_pPBI(NULL) {}
	~CAkVPLVolAutmNode();

	/*virtual*/ void	ConsumeBuffer( AkVPLState & io_state );
	/*virtual*/ void	ProcessDone( AkVPLState & io_state );
	virtual void		ReleaseBuffer();

	AKRESULT			TimeSkip( AkUInt32 & io_uFrames );
	AKRESULT Init( CAkPBI* in_pPBI );
	void	Term(){};

	virtual void		VirtualOn( AkVirtualQueueBehavior eBehavior );

	//For sample accurate transitions.
	inline void SetPBI(CAkPBI* in_pPbi){m_pPBI = in_pPbi;}

private:
	void Execute(AkVPLState & io_state);

	void ApplyModsToVolume(
		AkVPLState& io_state,
		AkModulatorXfrm in_xfrms[],
		AkModulatorGain in_gains[],
		AkReal32* in_pBufs[],
		AkUInt32 in_uNumBufs,
		AkUInt32 in_uNumGains
		);

	CAkPBI* m_pPBI;
};

#endif //_AK_VPL_VOL_AUTM_NODE_H_
