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
// AkVPLFilterNode.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _AK_VPL_FILTER_NODE_H_
#define _AK_VPL_FILTER_NODE_H_

#include "AkVPLFilterNodeBase.h"

// Used for in place effects
class CAkVPLFilterNode : public CAkVPLFilterNodeBase
{
public:
	virtual void		VirtualOn( AkVirtualQueueBehavior eBehavior );
	virtual void		GetBuffer( AkVPLState & io_state );
	virtual void		ConsumeBuffer( AkVPLState & io_state );
	virtual void		ReleaseBuffer();

	virtual bool		ReleaseInputBuffer(){ return false; }

	virtual AKRESULT	Seek();

	virtual AKRESULT	Init(
		IAkPlugin * in_pPlugin,
		const AkFXDesc & in_fxDesc,
		AkUInt32 in_uFXIndex,
		CAkVPLSrcCbxNode * in_pCbx,
		AkAudioFormat &	in_format );
	virtual void		Term();
	virtual void		ReleaseMemory();
	virtual AKRESULT	TimeSkip( AkUInt32 & io_uFrames );

	virtual AK::IAkPlugin* GetPlugin(){return m_pEffect;}

	virtual AkChannelConfig GetOutputConfig();

private:
	AK::IAkInPlaceEffectPlugin *	m_pEffect;		// Pointer to Fx.
	AkUInt8 *						m_pAllocatedBuffer;
	AkChannelConfig					m_channelConfig;
};

#endif //_AK_VPL_FILTER_NODE_H_
