/***********************************************************************
  The content of this file includes source code for the sound engine
  portion of the AUDIOKINETIC Wwise Technology and constitutes "Level
  Two Source Code" as defined in the Source Code Addendum attached
  with this file.  Any use of the Level Two Source Code shall be
  subject to the terms and conditions outlined in the Source Code
  Addendum and the End User License Agreement for Wwise(R).

  Version:  Build: 
  Copyright (c) 2006-2020 Audiokinetic Inc.
 ***********************************************************************/

/////////////////////////////////////////////////////////////////////
//
// AkVPLFilterNode.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkVPLFilterNodeBase.h"
#include "AkAudioLibTimer.h"
#include "AkVPLSrcCbxNode.h"

void CAkVPLFilterNodeBase::VirtualOn( AkVirtualQueueBehavior eBehavior )
{
	if( eBehavior == AkVirtualQueueBehavior_FromBeginning )
	{
		// WG-5935 
		// If the last buffer was drained and we restart from beginning, the flag must be resetted.
		m_bLast = false;
	}	
	
    if ( !m_bLast )
    {
		m_pInput->VirtualOn( eBehavior );
	}
}

AKRESULT CAkVPLFilterNodeBase::VirtualOff( AkVirtualQueueBehavior eBehavior, bool in_bUseSourceOffset )
{
	if ( !m_bLast )
	    return m_pInput->VirtualOff( eBehavior, in_bUseSourceOffset );

	return AK_Success;
}

AKRESULT CAkVPLFilterNodeBase::Init( 
		IAkPlugin * in_pPlugin,
		const AkFXDesc & in_fxDesc,
		AkUInt32 in_uFXIndex,
		CAkVPLSrcCbxNode * in_pCbx,
		AkAudioFormat & )
{
	AKASSERT( in_pPlugin != NULL );
	AKASSERT( in_pCbx != NULL );

	m_pCbx					= in_pCbx;
	m_pInsertFXContext		= NULL;
	m_bLast					= false;
	m_iBypassed				= 0;
	m_bLastBypassed			= false;
	m_uFXIndex				= in_uFXIndex;

#ifndef AK_OPTIMIZED
	// Context must be set before clone since it will trigger
	// RTPC subscriptions and we need the Context for profiling
	if (CAkBehavioralCtx *pCtx = in_pCbx->GetPBI())
	{
		m_pluginParams.UpdateContextID(pCtx->GetSoundID(), pCtx->GetPipelineID());
	}
#endif

	if (!m_pluginParams.Clone(in_fxDesc.pFx, in_pCbx->GetPBI()))
		return AK_Fail;

	m_pluginParams.fxID = in_fxDesc.pFx->GetFXID(); // Cached copy of fx id for profiling.
	
	m_pInsertFXContext = AkNew( AkMemID_Processing, CAkInsertFXContext( in_pCbx, in_uFXIndex ) );
	if ( m_pInsertFXContext == NULL )
	{
		MONITOR_PLUGIN_ERROR(AK::Monitor::ErrorCode_PluginAllocationFailed, in_pCbx->GetPBI(), m_pluginParams.fxID);
		return AK_Fail;
	}

	return AK_Success;
} // Init

void CAkVPLFilterNodeBase::Term()
{
	m_pluginParams.Term();

	if ( m_pInsertFXContext )
	{
		AkDelete( AkMemID_Processing, m_pInsertFXContext );
		m_pInsertFXContext = NULL;
	}
} // Term
