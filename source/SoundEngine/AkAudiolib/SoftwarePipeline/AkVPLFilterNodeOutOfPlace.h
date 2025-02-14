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

#ifndef _AK_VPL_FILTER_NODE_EX_H_
#define _AK_VPL_FILTER_NODE_EX_H_

#include "AkVPLFilterNodeBase.h"

class CAkVPLFilterNodeOutOfPlace : public CAkVPLFilterNodeBase
{
public:
	virtual void		VirtualOn( AkVirtualQueueBehavior eBehavior );
	virtual void		GetBuffer( AkVPLState & io_state );
	virtual void		ConsumeBuffer( AkVPLState & io_state );

	virtual void		ReleaseBuffer();
	virtual bool		ReleaseInputBuffer();

	virtual void		ProcessDone( AkVPLState & io_state );

	virtual AKRESULT	Seek();

	virtual void		PopMarkers(AkUInt32 uNumMarkers);

	virtual AKRESULT	Init(
		IAkPlugin * in_pPlugin,
		const AkFXDesc & in_fxDesc,
		AkUInt32 in_uFXIndex,
		CAkVPLSrcCbxNode * in_pCbx,
		AkAudioFormat &	io_format );
	virtual void		Term();
	virtual void		ReleaseMemory();

	virtual AKRESULT	TimeSkip( AkUInt32 & io_uFrames );

	virtual AK::IAkPlugin* GetPlugin(){ return m_pEffect; }
	virtual AkChannelConfig GetOutputConfig();

private:

	void InitInputBuffer(AkVPLState &in_buffer);

	AK::IAkOutOfPlaceEffectPlugin *	m_pEffect;		// Pointer to Fx.
	AkUInt16						m_usRequestedFrames;
	AkVPLState						m_BufferIn;			// Input buffer.
	AkVPLState						m_BufferOut;		// Output buffer.
	AkUInt32                		m_uInOffset;
	AkUInt32						m_InputFramesBeforeExec;
	AkUInt32						m_uConsumedInputFrames;
	AkUInt32						m_uRequestedInputFrames;
	AkReal32						m_fAveragedInput;	//For rate estimation
	AkReal32						m_fAveragedOutput;	//For rate estimation
	AkUInt32						m_uConsumedSinceLastOutput;
};

#endif //_AK_VPL_FILTER_NODE_EX_H_
