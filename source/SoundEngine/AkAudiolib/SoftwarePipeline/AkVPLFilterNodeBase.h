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

#ifndef _AK_VPL_FILTER_NODE_BASE_H_
#define _AK_VPL_FILTER_NODE_BASE_H_

#include "AkFXContext.h"
#include "AkVPLNode.h"
#include "AkFxBase.h"

class CAkVPLFilterNodeBase : public CAkVPLNode
{
public:
	
	virtual void		VirtualOn( AkVirtualQueueBehavior eBehavior );
    virtual AKRESULT	VirtualOff( AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset );

	virtual AKRESULT Init(
		IAkPlugin * in_pPlugin,
		const AkFXDesc & in_fxDesc,
		AkUInt32 in_uFXIndex,
		CAkVPLSrcCbxNode * in_pCbx,
		AkAudioFormat &	in_format );
	virtual void		Term() = 0;
	virtual void		ReleaseMemory() = 0;
	virtual bool		ReleaseInputBuffer() = 0;

	virtual void		GetBuffer( AkVPLState & io_state ) = 0;
	virtual void		ConsumeBuffer( AkVPLState & io_state ) = 0;

	AkInt16 GetBypassed() { return m_iBypassed; }
	void SetBypassed( AkInt16 in_iBypassed ) { m_iBypassed = in_iBypassed; }
	AkPluginID GetFXID() const { return m_pluginParams.fxID; }

	bool IsUsingThisSlot( const CAkUsageSlot* in_pUsageSlot )
	{
		return m_pInsertFXContext && m_pInsertFXContext->IsUsingThisSlot( in_pUsageSlot, GetPlugin() );
	}

	bool IsUsingThisSlot( const AkUInt8* in_pData )
	{
		return m_pInsertFXContext && m_pInsertFXContext->IsUsingThisSlot( in_pData );
	}

	virtual AK::IAkPlugin* GetPlugin() = 0;
	virtual AkChannelConfig GetOutputConfig() = 0;

#ifndef AK_OPTIMIZED
	void RecapParamDelta()
	{
		m_pluginParams.RecapRTPCParams();
	}
#endif

protected:

	struct PluginParams : public PluginRTPCSub
	{
		AkPluginID				fxID;				// Effect unique type ID. 

		PluginParams()
			: fxID( AK_INVALID_PLUGINID )
		{}
	};

	CAkVPLSrcCbxNode *	m_pCbx;				// Pointer to Cbx node.
	CAkInsertFXContext *	m_pInsertFXContext;	// FX context.
	PluginParams			m_pluginParams;

	bool					m_bLast;			// True=was the last input buffer.
	AkInt16					m_iBypassed;
	bool					m_bLastBypassed;	// FX bypassed last buffer (determine whether Reset necessary)
	AkUInt32                m_uFXIndex;
};

#endif //_AK_VPL_FILTER_NODE_BASE_H_
