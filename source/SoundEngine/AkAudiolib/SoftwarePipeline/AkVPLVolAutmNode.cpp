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
// CAkVPLVolAutmNode.cpp
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "AkPBI.h"
#include "AkVPLVolAutmNode.h"
#include "AkModulatorPBIData.h"

#include <AK/DSP/AkVolAutomation.h>
#include <AK/DSP/AkApplyGain.h>


CAkVPLVolAutmNode::~CAkVPLVolAutmNode()
{
}

AKRESULT CAkVPLVolAutmNode::Init( CAkPBI* in_pPBI )
{
	m_pPBI = in_pPBI;
	return AK_Success;
}

void CAkVPLVolAutmNode::ConsumeBuffer( AkVPLState & io_state )
{
	// this could be no more data if we're getting the last buffer
	if ( io_state.HasData() && !m_pPBI->GetModulatorData().IsEmpty() )
	{
		Execute( io_state );
	}
}

void CAkVPLVolAutmNode::ProcessDone( AkVPLState & io_state )
{
}

void CAkVPLVolAutmNode::ReleaseBuffer()
{
	if ( m_pInput ) // Can be NULL when voice starvation occurs in sample-accurate sequences
		m_pInput->ReleaseBuffer();	 
} // ReleaseBuffer

AKRESULT CAkVPLVolAutmNode::TimeSkip( AkUInt32 & io_uFrames )
{
	return m_pInput->TimeSkip( io_uFrames );
}

void CAkVPLVolAutmNode::VirtualOn( AkVirtualQueueBehavior eBehavior )
{
	if ( m_pInput ) // Could be NULL when voice starvation occurs in sample-accurate sequences
		m_pInput->VirtualOn( eBehavior );
}

void CAkVPLVolAutmNode::Execute(AkVPLState & io_state)
{
	static const AkRTPCBitArray kVolAndMakeUpGain( (1ULL << RTPC_Volume) | (1ULL << RTPC_MakeUpGain) );

	CAkModulatorData& modData = m_pPBI->GetModulatorData();

	AkUInt32 uNumBufMods = 0, uNumGainMods = 0;
	
	modData.GetNumAutomatedModulators( kVolAndMakeUpGain, uNumBufMods, uNumGainMods );
	if ( uNumBufMods == 0 && uNumGainMods == 0 )
		return;

	if (!m_pPBI->WasStopped())
	{
		AkModulatorXfrm* xfrms = (AkModulatorXfrm*)AkAlloca( uNumBufMods*sizeof(AkModulatorXfrm) );
		AkModulatorGain* gains = (AkModulatorGain*)AkAlloca( uNumGainMods*sizeof(AkModulatorGain) );
		AkReal32** pBufs = (AkReal32**)AkAlloca( uNumBufMods*sizeof(AkReal32*) );

		modData.GetBufferList( kVolAndMakeUpGain, xfrms, gains, pBufs, uNumBufMods, uNumGainMods );

		ApplyModsToVolume( io_state, xfrms, gains, pBufs, uNumBufMods, uNumGainMods );
	}
	else
	{
		//If the PBI was stopped, we apply a constant volume scale to the final buffer.
		AkModulatorGain gain( modData.GetCtrlRateOutput( kVolAndMakeUpGain ) );
		ApplyModsToVolume( io_state, NULL, &gain, NULL, 0, 1 );
	}
}

void CAkVPLVolAutmNode::ApplyModsToVolume(
				AkVPLState& io_state,
				AkModulatorXfrm in_xfrms[],
				AkModulatorGain in_gains[],
				AkReal32* in_pBufs[],
				AkUInt32 in_uNumBufs,
				AkUInt32 in_uNumGains
				)
{
	// First apply the modulators which have buffers
	AK::DSP::ApplyVolAutomation( io_state, in_xfrms, in_pBufs, in_uNumBufs );

	// Next apply the bufferless modulators
	for( AkUInt32 i = 0; i < in_uNumGains; ++i )
	{
		AK::DSP::ApplyGain( &io_state, in_gains[i].m_fStart, in_gains[i].m_fEnd );
	}
}

