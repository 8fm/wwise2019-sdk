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

/////////////////////////////////////////////////////////////////////
//
// AkVPLLPFNode.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkVPLLPFNode.h"

void CAkVPLBQFNode::ConsumeBuffer( AkVPLState & io_state )
{
	// this could be no more data if we're getting the last buffer
	if ( io_state.HasData() )
	{
		m_LpHpFilter.Execute( &io_state );
	}
}

void CAkVPLBQFNode::ProcessDone( AkVPLState & io_state )
{
}

void CAkVPLBQFNode::ReleaseBuffer()
{
	if ( m_pInput ) // Can be NULL when voice starvation occurs in sample-accurate sequences
		m_pInput->ReleaseBuffer();	 
} // ReleaseBuffer

AKRESULT CAkVPLBQFNode::TimeSkip( AkUInt32 & io_uFrames )
{
	// no need to do anything while skipping time here.
	// this is assuming that the output volume at boundaries of 'skipped time' is equivalent
	// to zero, masking any artefact that might arise from not updating the coefficients.

	return m_pInput->TimeSkip( io_uFrames );
}

void CAkVPLBQFNode::VirtualOn( AkVirtualQueueBehavior eBehavior )
{
	m_LpHpFilter.ResetRamp();

	if ( m_pInput ) // Could be NULL when voice starvation occurs in sample-accurate sequences
		m_pInput->VirtualOn( eBehavior );
}
