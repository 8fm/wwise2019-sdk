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
// AkVPLLPFNode.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_VPL_LPF_NODE_H_
#define _AK_VPL_LPF_NODE_H_

#include "AkSrcLpFilter.h"
#include "AkVPLNode.h"

class CAkVPLBQFNode : public CAkVPLNode
{
public:
	/*virtual*/ void	ConsumeBuffer( AkVPLState & io_state );
	/*virtual*/ void	ProcessDone( AkVPLState & io_state );
	virtual void		ReleaseBuffer();

	AKRESULT			TimeSkip( AkUInt32 & io_uFrames );
	AkForceInline AKRESULT Init( AkChannelConfig in_channelConfig ) { return m_LpHpFilter.Init( in_channelConfig ); }
	AkForceInline void	Term() { m_LpHpFilter.Term(); }

	AkForceInline void SetLPF( AkReal32 in_fLPF ) { m_LpHpFilter.SetLPFPar( in_fLPF ); }
	AkForceInline AkReal32 GetLPF() const { return m_LpHpFilter.GetLPFPar(); }

	AkForceInline void SetHPF( AkReal32 in_fLPF ) { m_LpHpFilter.SetHPFPar( in_fLPF ); }
	AkForceInline AkReal32 GetHPF() const { return m_LpHpFilter.GetHPFPar(); }

	virtual void		VirtualOn( AkVirtualQueueBehavior eBehavior );

private:
	CAkSrcLpHpFilter		m_LpHpFilter;			// Pointer to lpf/hpf object.
};

#endif //_AK_VPL_LPF_NODE_H_
