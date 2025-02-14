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
// AkSrcLpFilter.h
//
// Single Butterworth low pass filter section (IIR). 
// The input control parameter in range (0,100) is non -linearly mapped to cutoff frequency (Hz)
// Assumes same thread will call both SetLPFPar and Execute (not locking)
//
//////////////////////////////////////////////////////////////////////

#ifndef _AK_SRCLPFILTER_H_
#define _AK_SRCLPFILTER_H_

#include "AkInternalLPFState.h"
#include "AkLPFCommon.h"

//#define PERFORMANCE_BENCHMARK


class CAkSrcLpHpFilter
{
public:  
	CAkSrcLpHpFilter();
    ~CAkSrcLpHpFilter();

    AKRESULT Init( AkChannelConfig in_channelConfig );
	void Term();

	AKRESULT ChangeChannelConfig(AkChannelConfig in_channelConfig);

	AkForceInline void SetLPFPar( AkReal32 in_fTargetLPFPar )
	{
		SetTargetParam(m_InternalBQFState.m_LPFParams, in_fTargetLPFPar);
	}

	AkForceInline void SetHPFPar( AkReal32 in_fTargetHPFPar )
	{
		SetTargetParam(m_InternalBQFState.m_HPFParams, in_fTargetHPFPar);
	}

	AkForceInline void CheckBypass()
	{
		// This is moved from the _Execute to outside the channel loop
		if (AK_EXPECT_FALSE( !m_InternalBQFState.m_LPFParams.IsBypassed() ))
		{
			if (AK_EXPECT_FALSE( !m_InternalBQFState.m_LPFParams.IsInterpolating() ))
			{
				m_InternalBQFState.m_LPFParams.fCurrentPar = m_InternalBQFState.m_LPFParams.fTargetPar;

				if (AK_EXPECT_FALSE( m_InternalBQFState.m_LPFParams.iNextBypass > 0 ))
				{
					if (--m_InternalBQFState.m_LPFParams.iNextBypass == 0)
					{
						m_InternalBQFState.m_LPFParams.SetBypassed( true );
					}
				}
				// First frame to turn bypass on
				else if (AkLpfParamEval::EvalBypass( m_InternalBQFState.m_LPFParams.fTargetPar ))
				{
					m_InternalBQFState.m_LPFParams.iNextBypass = AkBQFParams::kFramesToBypass;
				}
			}
		}

		if (AK_EXPECT_FALSE( !m_InternalBQFState.m_HPFParams.IsBypassed() ))
		{
			if (AK_EXPECT_FALSE( !m_InternalBQFState.m_HPFParams.IsInterpolating() ))
			{
				m_InternalBQFState.m_HPFParams.fCurrentPar = m_InternalBQFState.m_HPFParams.fTargetPar;

				if (AK_EXPECT_FALSE( m_InternalBQFState.m_HPFParams.iNextBypass > 0 ))
				{
					if (--m_InternalBQFState.m_HPFParams.iNextBypass == 0)
					{
						m_InternalBQFState.m_HPFParams.SetBypassed( true );
					}
				}
				// First frame to turn bypass on
				else if (AkHpfParamEval::EvalBypass( m_InternalBQFState.m_HPFParams.fTargetPar ))
				{
					m_InternalBQFState.m_HPFParams.iNextBypass = AkBQFParams::kFramesToBypass;
				}
			}
		}
	}

	AkForceInline void UpdateNumInterBlocks( AkUInt16 in_uNumInterBlocks ) 
	{
		m_InternalBQFState.m_LPFParams.uNumInterBlocks = in_uNumInterBlocks;
		m_InternalBQFState.m_HPFParams.uNumInterBlocks = in_uNumInterBlocks;
	}

	void ResetRamp();

	AkForceInline AkReal32 GetLPFPar() const { return m_InternalBQFState.m_LPFParams.fTargetPar; }
	AkForceInline AkReal32 GetHPFPar() const { return m_InternalBQFState.m_HPFParams.fTargetPar; }

	AkForceInline bool IsInitialized() { return m_InternalBQFState.m_LPF.IsInitialized() &&
												m_InternalBQFState.m_HPF.IsInitialized(); }

	AkForceInline bool IsDcRemoved() {	return m_InternalBQFState.m_LPFParams.IsDcRemoved() &&
											   m_InternalBQFState.m_HPFParams.IsDcRemoved(); }
	
	AkForceInline AkChannelConfig GetChannelConfig() const { return m_InternalBQFState.channelConfig; }	

    void Execute( AkAudioBuffer * io_pBuffer ); 	// Input/output audio buffer	
	void ExecuteOutOfPlace(AkAudioBuffer * in_pBuffer, AkAudioBuffer * out_pBuffer);	
	AkUInt16 ExecuteMono( AkAudioBuffer * in_pBuffer, AkAudioBuffer * out_pBuffer, AkUInt32 in_filterChannel ); // Input/output mono audio buffer, return the interpolation frame increment
	void BypassMono( AkAudioBuffer * in_pBuffer, AkUInt32 in_filterChannel ); // Input/output mono audio buffer

	bool ManageBQFChange();

private:

	AkInternalBQFState m_InternalBQFState;

#ifdef PERFORMANCE_BENCHMARK
	AkUInt32			m_uNumberCalls;
	AkReal32			m_fTotalTime;
#endif

	AkForceInline void SetTargetParam( AkBQFParams& in_params, AkReal32 in_fTargetPar )
	{
		in_fTargetPar = AkClamp( in_fTargetPar, 0.0f, 100.0f );
		if (in_fTargetPar != in_params.fTargetPar)
		{
			in_params.bTargetDirty = true;
			in_params.fCurrentPar += ( (in_params.fTargetPar - in_params.fCurrentPar)/NUMBLOCKTOREACHTARGET ) * in_params.uNumInterBlocks;
			in_params.fTargetPar = in_fTargetPar;
		}
	}
};

#endif  // _AK_SRCLPFILTER_H_
