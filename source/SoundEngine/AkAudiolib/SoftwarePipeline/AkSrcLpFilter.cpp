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
// AkSrcLpFilter.cpp
//
// Single Butterworth low pass filter section (IIR). 
// The input control parameter in range (0,100) is non -linearly mapped to cutoff frequency (Hz)
// Assumes same thread will call both SetLPFPar and Execute (not locking)
// There is some interpolation done on the LPF control parameter do avoid stepping behavior in transition
// or fast parameter changes.
// 
// We can think of latency / stepping problem by looking at rates at which different thinks occur:
// Control rate: rate at which SetLPFPar are called -> 1x per buffer (linked with buffer time)
// Interpolation rate: rate at which transitional LPFParam values are changed ( coefficient recalculation needed)
// -> NUMBLOCKTOREACHTARGET per buffer, necessary to avoid stepping while introduces up to 1 buffer latency
// Audio rate: rate at which samples are calculated == sample rate
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h" 
#include "AkSrcLpFilter.h"
#include "AkFXMemAlloc.h"
#include "AkSettings.h"
#ifdef PERFORMANCE_BENCHMARK
#include "AkMonitor.h"
#endif


CAkSrcLpHpFilter::CAkSrcLpHpFilter()
{
}

CAkSrcLpHpFilter::~CAkSrcLpHpFilter()
{
}

AKRESULT CAkSrcLpHpFilter::Init(AkChannelConfig in_channelConfig)
{
	// Note: for the moment, the LFE channel is a a full band channel treated no differently than others
	// This means that what really matters here is only the number of channel and not the input channel configuration
	// The number set here may be offset by half the DSP function table size to get the corresponding interpolating routine

	m_InternalBQFState.channelConfig = in_channelConfig;

#ifdef PERFORMANCE_BENCHMARK
	m_fTotalTime = 0.f;
	m_uNumberCalls = 0;
#endif 

	if(m_InternalBQFState.m_LPF.Init(AkFXMemAlloc::GetLower(), in_channelConfig.uNumChannels) == AK_Success &&
		m_InternalBQFState.m_HPF.Init(AkFXMemAlloc::GetLower(), in_channelConfig.uNumChannels) == AK_Success)
	{
		return AK_Success;
	}
	else
	{
		Term();
		return AK_Fail;
	}
}

//-----------------------------------------------------------------------------
// Name: Term
// Desc: Terminates.
//-----------------------------------------------------------------------------
void CAkSrcLpHpFilter::Term( )
{
	m_InternalBQFState.m_LPF.Term( AkFXMemAlloc::GetLower() );
	m_InternalBQFState.m_HPF.Term( AkFXMemAlloc::GetLower() );

#ifdef PERFORMANCE_BENCHMARK
	if ( m_uNumberCalls )
	{
		AkOSChar szString[64];
		AK_OSPRINTF( szString, 64, AKTEXT("%f\n"), m_fTotalTime/m_uNumberCalls );
		AKPLATFORM::OutputDebugMsg( szString );
		MONITOR_MSG( szString );
	}
#endif 
}

AKRESULT CAkSrcLpHpFilter::ChangeChannelConfig(AkChannelConfig in_channelConfig)
{
	//We need to reallocate buffers, but keep the same filter parameters.
	m_InternalBQFState.m_LPF.Term(AkFXMemAlloc::GetLower());
	m_InternalBQFState.m_HPF.Term(AkFXMemAlloc::GetLower());

	m_InternalBQFState.channelConfig = in_channelConfig;
	m_InternalBQFState.m_LPFParams.Reset();
	m_InternalBQFState.m_HPFParams.Reset();
	if (m_InternalBQFState.m_LPF.Init(AkFXMemAlloc::GetLower(), in_channelConfig.uNumChannels) == AK_Success &&
		m_InternalBQFState.m_HPF.Init(AkFXMemAlloc::GetLower(), in_channelConfig.uNumChannels) == AK_Success)
	{
		return AK_Success;
	}
	else
	{
		Term();
		return AK_Fail;
	}
}

void CAkSrcLpHpFilter::ResetRamp()
{
	m_InternalBQFState.m_LPFParams.bFirstSet = true;
	m_InternalBQFState.m_LPF.Reset();

	m_InternalBQFState.m_HPFParams.bFirstSet = true;
	m_InternalBQFState.m_HPF.Reset();
}

template< typename EvalBqfParams > 
static inline bool _ManageBQFChange( AkBQFParams& io_params, DSP::BiquadFilterMultiSIMD& in_Bqf )
{
	if ( AK_EXPECT_FALSE( io_params.bTargetDirty ) )
	{
		io_params.bTargetDirty = false;

		if ( AK_EXPECT_FALSE( io_params.bFirstSet ) )
		{
			io_params.bFirstSet = false;

			// No interpolation required upon first SetLPF call (current == target)
			io_params.fCurrentPar = io_params.fTargetPar;
			io_params.uNumInterBlocks = NUMBLOCKTOREACHTARGET;
			io_params.SetBypassed(EvalBqfParams::EvalBypass( io_params.fTargetPar ));
			if ( !io_params.IsBypassed() )
			{
				AkReal32 fCutFreq = EvalBqfParams::EvalCutoff( io_params.fTargetPar, AK_NUM_VOICE_REFILL_FRAMES );
				EvalBqfParams::ComputeCoefs(in_Bqf, fCutFreq);
			}
		}
		else
		{
			// New LPF interpolation is required (set target)
			bool bBypass =  EvalBqfParams::EvalBypass( io_params.fCurrentPar ) && EvalBqfParams::EvalBypass( io_params.fTargetPar );
			io_params.SetBypassed(bBypass);
			io_params.uNumInterBlocks = bBypass ? NUMBLOCKTOREACHTARGET : 0;
		}
	}
	return io_params.IsBypassed();
}

template< typename EvalBqfParams > 
void _Execute( AkAudioBuffer * io_pBuffer, AkBQFParams& io_params, DSP::BiquadFilterMultiSIMD& in_Bqf  )
{
	AKASSERT( io_pBuffer != NULL && io_pBuffer->GetChannel( 0 ) != NULL );
	AKASSERT( io_pBuffer->MaxFrames() != 0 && io_pBuffer->uValidFrames <= io_pBuffer->MaxFrames() );

	const AkUInt32 uChannels = io_pBuffer->NumChannels();
	const AkUInt32 uNumFrames = io_pBuffer->uValidFrames;

	bool isBypassed = _ManageBQFChange<EvalBqfParams>( io_params, in_Bqf );

	// Call appropriate DSP function
	if ( AK_EXPECT_FALSE( !isBypassed ) )
	{
		if (io_params.IsInterpolating())
		{
			//Interpolation required.
			const AkReal32 fParamStart = io_params.fCurrentPar;
			const AkReal32 fParamDiff = io_params.fTargetPar - fParamStart;
			
			AkUInt32 uFramesProduced = 0;
			while ( uFramesProduced < uNumFrames )
			{
				AkUInt32 uFramesInBlock = AkMin(LPFPARAMUPDATEPERIOD, uNumFrames-uFramesProduced);

				if ( io_params.IsInterpolating() )
				{
					++io_params.uNumInterBlocks;
					AkReal32 fCurrentPar = fParamStart + (fParamDiff*io_params.uNumInterBlocks)/NUMBLOCKTOREACHTARGET;
					AkReal32 fCutFreq = EvalBqfParams::EvalCutoff( fCurrentPar, AK_NUM_VOICE_REFILL_FRAMES );
					EvalBqfParams::ComputeCoefs(in_Bqf, fCutFreq);
				}

				in_Bqf.ProcessBuffer((AkReal32*)io_pBuffer->GetInterleavedData() + uFramesProduced, uFramesInBlock, io_pBuffer->MaxFrames());
				
				uFramesProduced += uFramesInBlock;
			}

			if ( !io_params.IsInterpolating() )	
			{
				io_params.fCurrentPar = io_params.fTargetPar;
				if (EvalBqfParams::EvalBypass( io_params.fTargetPar ))
					io_params.iNextBypass = AkBQFParams::kFramesToBypass;
			}
		}
		else
		{
			in_Bqf.ProcessBuffer((AkReal32*)io_pBuffer->GetInterleavedData(), uNumFrames, io_pBuffer->MaxFrames());
			if (AK_EXPECT_FALSE( io_params.iNextBypass > 0 ) )
			{
				if ( --io_params.iNextBypass == 0 )
					io_params.SetBypassed(true);
			}
		}
	}
	else
	{
		if (uNumFrames >= 2 )
		{
			// We must remove the DC offset introduced by transition from non-bypass to bypass
			if ( AK_EXPECT_FALSE( ! io_params.IsDcRemoved() ) )
			{
				io_params.SetDcRemoved();

				for (AkUInt16 c = 0; c < uChannels; ++c)
				{
					AkReal32 * pfBuf = io_pBuffer->GetChannel(c);
									
					::DSP::Memories filterMem;
					in_Bqf.GetMemories(c, filterMem.fFFwd1, filterMem.fFFwd2, filterMem.fFFbk1, filterMem.fFFbk2);
										
					AkReal32 fSampleOffset = (filterMem.fFFbk1 - filterMem.fFFwd1);
					AkReal32 fOffsetDelta = fSampleOffset / uNumFrames;
					
					for (AkUInt16 s = 0; s < uNumFrames; ++s)
					{	
						fSampleOffset -= fOffsetDelta;
						pfBuf[s] += fSampleOffset;												
					}
				}
			}

			// We must maintain the memory of the filter buffers so that we are ready to process values when we result Filtering (unbypassing)
			for (AkUInt16 c = 0; c < uChannels; ++c)
			{
				//Reset the memory of the filter to the current signal.  Better than starting from 0.
				AkReal32 * pfBuf = io_pBuffer->GetChannel(c);
	
				in_Bqf.SetMemories(c, pfBuf[uNumFrames - 1], pfBuf[uNumFrames - 2], pfBuf[uNumFrames - 1], pfBuf[uNumFrames - 2]);//sending 2 last samples
			}
		}
	}
}

// This is just a copy of _Execute using the out of place logic from the mono code
template< typename EvalBqfParams >
void _ExecuteOutOfPlace(AkAudioBuffer * in_pBuffer, AkAudioBuffer * out_pBuffer, AkBQFParams& io_params, DSP::BiquadFilterMultiSIMD& in_Bqf)
{
	AKASSERT(in_pBuffer != NULL && in_pBuffer->GetChannel(0) != NULL);
	AKASSERT(in_pBuffer->MaxFrames() != 0 && in_pBuffer->uValidFrames <= in_pBuffer->MaxFrames());
	AKASSERT(out_pBuffer != NULL && out_pBuffer->GetChannel(0) != NULL);
	AKASSERT(out_pBuffer->MaxFrames() != 0 && out_pBuffer->uValidFrames <= out_pBuffer->MaxFrames());

	const AkUInt32 uChannels = in_pBuffer->NumChannels();
	const AkUInt32 uNumFrames = in_pBuffer->uValidFrames;

	bool isBypassed = _ManageBQFChange<EvalBqfParams>(io_params, in_Bqf);

	// Call appropriate DSP function
	if (AK_EXPECT_FALSE(!isBypassed))
	{
		if (io_params.IsInterpolating())
		{
			//Interpolation required.
			const AkReal32 fParamStart = io_params.fCurrentPar;
			const AkReal32 fParamDiff = io_params.fTargetPar - fParamStart;

			AkUInt32 uFramesProduced = 0;
			while (uFramesProduced < uNumFrames)
			{
				AkUInt32 uFramesInBlock = AkMin(LPFPARAMUPDATEPERIOD, uNumFrames - uFramesProduced);

				if (io_params.IsInterpolating())
				{
					++io_params.uNumInterBlocks;
					AkReal32 fCurrentPar = fParamStart + (fParamDiff*io_params.uNumInterBlocks) / NUMBLOCKTOREACHTARGET;
					AkReal32 fCutFreq = EvalBqfParams::EvalCutoff(fCurrentPar, AK_NUM_VOICE_REFILL_FRAMES);
					EvalBqfParams::ComputeCoefs(in_Bqf, fCutFreq);
				}

				AkReal32 * AK_RESTRICT in_pfBuf = (AkReal32 *)in_pBuffer->GetInterleavedData() + uFramesProduced;
				AkReal32 * AK_RESTRICT out_pfBuf = (AkReal32 *)out_pBuffer->GetInterleavedData() + uFramesProduced;
				in_Bqf.ProcessBuffer(in_pfBuf, uFramesInBlock, in_pBuffer->MaxFrames(), out_pfBuf);
				uFramesProduced += uFramesInBlock;
			}

			if (!io_params.IsInterpolating())
			{
				io_params.fCurrentPar = io_params.fTargetPar;
				if (EvalBqfParams::EvalBypass(io_params.fTargetPar))
					io_params.iNextBypass = AkBQFParams::kFramesToBypass;
			}
		}
		else
		{
			in_Bqf.ProcessBuffer((AkReal32*)in_pBuffer->GetInterleavedData(), uNumFrames, in_pBuffer->MaxFrames(), (AkReal32*)out_pBuffer->GetInterleavedData());

			if (AK_EXPECT_FALSE(io_params.iNextBypass > 0))
			{
				if (--io_params.iNextBypass == 0)
					io_params.SetBypassed(true);
			}
		}
	}
	else
	{
		if (uNumFrames >= 2)
		{
			// We must remove the DC offset introduced by transition from non-bypass to bypass
			if (AK_EXPECT_FALSE(!io_params.IsDcRemoved()))
			{
				io_params.SetDcRemoved();

				for (AkUInt16 c = 0; c < uChannels; ++c)
				{
					AkReal32 * in_pfBuf = in_pBuffer->GetChannel(c);
					AkReal32 * out_pfBuf = out_pBuffer->GetChannel(c);

					::DSP::Memories filterMem;
					in_Bqf.GetMemories(c, filterMem.fFFwd1, filterMem.fFFwd2, filterMem.fFFbk1, filterMem.fFFbk2);

					AkReal32 fSampleOffset = (filterMem.fFFbk1 - filterMem.fFFwd1);
					AkReal32 fOffsetDelta = fSampleOffset / uNumFrames;

					for (AkUInt16 s = 0; s < uNumFrames; ++s)
					{
						// Copy forward the input buffer
						out_pfBuf[s] = in_pfBuf[s] - fSampleOffset;
						fSampleOffset += fOffsetDelta;
					}
				}
			}
			else
			{
				for (AkUInt16 c = 0; c < uChannels; ++c)
				{
					AkReal32 * in_pfBuf = in_pBuffer->GetChannel(c);
					AkReal32 * out_pfBuf = out_pBuffer->GetChannel(c);

					AKPLATFORM::AkMemCpy(out_pfBuf, in_pfBuf, uNumFrames * sizeof(AkReal32));
				}
			}

			// We must maintain the memory of the filter buffers so that we are ready to process values when we result Filtering (unbypassing)
			for (AkUInt16 c = 0; c < uChannels; ++c)
			{
				//Reset the memory of the filter to the current signal.  Better than starting from 0.
				AkReal32 * in_pfBuf = in_pBuffer->GetChannel(c);

				in_Bqf.SetMemories(c, in_pfBuf[uNumFrames - 1], in_pfBuf[uNumFrames - 2], in_pfBuf[uNumFrames - 1], in_pfBuf[uNumFrames - 2]);//sending 2 last samples
			}
		}
	}
}

void _BypassMonoOutOfPlace( AkAudioBuffer * in_pBuffer, AkAudioBuffer * out_pBuffer, AkBQFParams& io_params, DSP::BiquadFilterMultiSIMD& in_Bqf, AkUInt32 in_filterChannel )
{
	AKASSERT( in_pBuffer != NULL && in_pBuffer->GetChannel( 0 ) != NULL );
	AKASSERT( in_pBuffer->MaxFrames() != 0 && in_pBuffer->uValidFrames <= in_pBuffer->MaxFrames() );

	// The out buffer is only used when removing dc
	AKASSERT( out_pBuffer != NULL && out_pBuffer->GetChannel( 0 ) != NULL );
	AKASSERT( out_pBuffer->MaxFrames() != 0 && out_pBuffer->uValidFrames <= out_pBuffer->MaxFrames() );
	AKASSERT( out_pBuffer->NumChannels() == 1 );

	const AkUInt32 uNumFrames = in_pBuffer->uValidFrames;

	if ( uNumFrames >= 2 )
	{
		// We must remove the DC offset introduced by transition from non-bypass to bypass
		if ( AK_EXPECT_FALSE( !io_params.IsDcRemoved() ) )
		{
			io_params.SetDcRemoved();

			AkReal32 * in_pfBuf = in_pBuffer->GetChannel( in_filterChannel );
			AkReal32 * out_pfBuf = out_pBuffer->GetChannel( 0 );
			
			::DSP::Memories memories;
			in_Bqf.GetMemories(in_filterChannel, memories.fFFwd1, memories.fFFwd2, memories.fFFbk1, memories.fFFbk2);

			AkReal32 fSampleOffset = (memories.fFFbk1 - memories.fFFwd1);
			AkReal32 fOffsetDelta = fSampleOffset / uNumFrames;

			for ( AkUInt16 s = 0; s < uNumFrames; ++s )
			{
				// Copy forward the input buffer
				out_pfBuf[s] = in_pfBuf[s] - fSampleOffset;
				fSampleOffset += fOffsetDelta;
			}
		}
		else
		{
			AkReal32 * in_pfBuf = in_pBuffer->GetChannel( in_filterChannel );
			AkReal32 * out_pfBuf = out_pBuffer->GetChannel( 0 );

			AKPLATFORM::AkMemCpy( out_pfBuf, in_pfBuf, uNumFrames * sizeof( AkReal32 ) );
		}

		// We must maintain the memory of the filter buffers so that we are ready to process values when we resule Filtering (unbypassing)
		//Reset the memory of the filter to the current signal.  Better than starting from 0.
		AkReal32 * in_pfBuf = in_pBuffer->GetChannel( in_filterChannel );
		in_Bqf.SetMemories(in_filterChannel, in_pfBuf[uNumFrames - 1], in_pfBuf[uNumFrames - 2], in_pfBuf[uNumFrames - 1], in_pfBuf[uNumFrames - 2]);//sending 2 last samples		
	}
}

void _BypassMonoInPlace( AkAudioBuffer * io_pBuffer, AkBQFParams& io_params, DSP::BiquadFilterMultiSIMD& in_Bqf, AkUInt32 in_filterChannel )
{
	AKASSERT( io_pBuffer != NULL && io_pBuffer->GetChannel( 0 ) != NULL );
	AKASSERT( io_pBuffer->MaxFrames() != 0 && io_pBuffer->uValidFrames <= io_pBuffer->MaxFrames() );
	AKASSERT( io_pBuffer->NumChannels() == 1 );

	const AkUInt32 uNumFrames = io_pBuffer->uValidFrames;

	if ( uNumFrames >= 2 )
	{
		// We must remove the DC offset introduced by transition from non-bypass to bypass
		if ( AK_EXPECT_FALSE( !io_params.IsDcRemoved() ) )
		{
			io_params.SetDcRemoved();

			AkReal32 * pfBuf = io_pBuffer->GetChannel( 0 );

			::DSP::Memories memories;
			in_Bqf.GetMemories(in_filterChannel, memories.fFFwd1, memories.fFFwd2, memories.fFFbk1, memories.fFFbk2);
			
			AkReal32 fSampleOffset = (memories.fFFbk1 - memories.fFFwd1);
			AkReal32 fOffsetDelta = fSampleOffset / uNumFrames;

			for ( AkUInt16 s = 0; s < uNumFrames; ++s )
			{
				pfBuf[s] += fSampleOffset;
				fSampleOffset -= fOffsetDelta;
			}
		}

		// We must maintain the memory of the filter buffers so that we are ready to process values when we resule Filtering (unbypassing)
		//Reset the memory of the filter to the current signal.  Better than starting from 0.
		AkReal32 * pfBuf = io_pBuffer->GetChannel( 0 );		
		in_Bqf.SetMemories(in_filterChannel, pfBuf[uNumFrames - 1], pfBuf[uNumFrames - 2], pfBuf[uNumFrames - 1], pfBuf[uNumFrames - 2]);//sending 2 last samples		
	}
}

// out of place for first filter
// Return the number of interpolation blocks to be incrememnted in the outer channel loop
template< typename EvalBqfParams >
AkUInt16 _ExecuteMonoOutOfPlace( AkAudioBuffer * in_pBuffer, AkAudioBuffer * out_pBuffer, AkBQFParams& io_params, DSP::BiquadFilterMultiSIMD& in_Bqf, AkUInt32 in_filterChannel )
{
	AKASSERT( in_pBuffer != NULL && in_pBuffer->GetChannel( 0 ) != NULL );
	AKASSERT( out_pBuffer != NULL && out_pBuffer->GetChannel( 0 ) != NULL );
	AKASSERT( in_pBuffer->MaxFrames() != 0 && in_pBuffer->uValidFrames <= in_pBuffer->MaxFrames() );
	AKASSERT( out_pBuffer->MaxFrames() != 0 && out_pBuffer->uValidFrames <= out_pBuffer->MaxFrames() );
	AKASSERT( out_pBuffer->NumChannels() == 1 );

	const AkUInt32 uNumFrames = in_pBuffer->uValidFrames;
	AkUInt16 uLocalNumInterBlocks = io_params.uNumInterBlocks;

	if (uLocalNumInterBlocks < NUMBLOCKTOREACHTARGET)
	{
		//Interpolation required.
		const AkReal32 fParamStart = io_params.fCurrentPar;
		const AkReal32 fParamDiff = io_params.fTargetPar - fParamStart;

		AkUInt32 uFramesProduced = 0;
		while ( uFramesProduced < uNumFrames )
		{
			AkUInt32 uFramesInBlock = AkMin( LPFPARAMUPDATEPERIOD, uNumFrames - uFramesProduced );

			if (uLocalNumInterBlocks < NUMBLOCKTOREACHTARGET)
			{
				++uLocalNumInterBlocks;
				AkReal32 fCurrentPar = fParamStart + (fParamDiff*uLocalNumInterBlocks) / NUMBLOCKTOREACHTARGET;
				AkReal32 fCutFreq = EvalBqfParams::EvalCutoff( fCurrentPar, AK_NUM_VOICE_REFILL_FRAMES );
				EvalBqfParams::ComputeCoefs( in_Bqf, fCutFreq );
			}

			AkReal32 * AK_RESTRICT in_pfBuf = in_pBuffer->GetChannel( in_filterChannel ) + uFramesProduced;
			AkReal32 * AK_RESTRICT out_pfBuf = out_pBuffer->GetChannel( 0 ) + uFramesProduced;
			in_Bqf.ProcessBufferMono( in_pfBuf, uFramesInBlock, in_filterChannel, out_pfBuf );

			uFramesProduced += uFramesInBlock;
		}
	}
	else
	{

		in_Bqf.ProcessBufferMono( in_pBuffer->GetChannel( in_filterChannel ), uNumFrames, in_filterChannel, out_pBuffer->GetChannel( 0 ) );
	}
	return uLocalNumInterBlocks;
}


// In place for cascaded filter
// Return the number of interpolation blocks to be incrememnted in the outer channel loop
template< typename EvalBqfParams >
AkUInt16 _ExecuteMonoInPlace( AkAudioBuffer * io_pBuffer, AkBQFParams& io_params, DSP::BiquadFilterMultiSIMD& in_Bqf, AkUInt32 in_filterChannel )
{
	AKASSERT( io_pBuffer != NULL && io_pBuffer->GetChannel( 0 ) != NULL );
	AKASSERT( io_pBuffer->MaxFrames() != 0 && io_pBuffer->uValidFrames <= io_pBuffer->MaxFrames() );
	AKASSERT( io_pBuffer->NumChannels() == 1 );

	const AkUInt32 uNumFrames = io_pBuffer->uValidFrames;
	AkUInt16 uLocalNumInterBlocks = io_params.uNumInterBlocks;

	if (uLocalNumInterBlocks < NUMBLOCKTOREACHTARGET)
	{
		//Interpolation required.
		const AkReal32 fParamStart = io_params.fCurrentPar;
		const AkReal32 fParamDiff = io_params.fTargetPar - fParamStart;

		AkUInt32 uFramesProduced = 0;
		while ( uFramesProduced < uNumFrames )
		{
			AkUInt32 uFramesInBlock = AkMin( LPFPARAMUPDATEPERIOD, uNumFrames - uFramesProduced );

			if (uLocalNumInterBlocks < NUMBLOCKTOREACHTARGET)
			{
				++uLocalNumInterBlocks;
				AkReal32 fCurrentPar = fParamStart + (fParamDiff*uLocalNumInterBlocks) / NUMBLOCKTOREACHTARGET;
				AkReal32 fCutFreq = EvalBqfParams::EvalCutoff( fCurrentPar, AK_NUM_VOICE_REFILL_FRAMES );
				EvalBqfParams::ComputeCoefs( in_Bqf, fCutFreq );
			}

			AkReal32 * AK_RESTRICT pfBuf = io_pBuffer->GetChannel( 0 ) + uFramesProduced;
			in_Bqf.ProcessBufferMono( pfBuf, uFramesInBlock, in_filterChannel );

			uFramesProduced += uFramesInBlock;
		}
	}
	else
	{
		in_Bqf.ProcessBufferMono( io_pBuffer->GetChannel( 0 ), uNumFrames, in_filterChannel );
	}
	return uLocalNumInterBlocks;
}

void CAkSrcLpHpFilter::Execute( AkAudioBuffer * io_pBuffer )
{
#ifdef PERFORMANCE_BENCHMARK
	AkInt64 TimeBefore;
	AKPLATFORM::PerformanceCounter( &TimeBefore ); 
#endif

	if ( m_InternalBQFState.m_LPF.IsInitialized() )
		_Execute<AkLpfParamEval>( io_pBuffer, m_InternalBQFState.m_LPFParams, m_InternalBQFState.m_LPF );
	if ( m_InternalBQFState.m_HPF.IsInitialized() )
		_Execute<AkHpfParamEval>( io_pBuffer, m_InternalBQFState.m_HPFParams, m_InternalBQFState.m_HPF );

#ifdef PERFORMANCE_BENCHMARK
	AkInt64 TimeAfter;
	AKPLATFORM::PerformanceCounter( &TimeAfter );
	AkReal32 fElapsed = AKPLATFORM::Elapsed( TimeAfter, TimeBefore );
	m_fTotalTime += fElapsed;
	++m_uNumberCalls;
#endif
}

void CAkSrcLpHpFilter::ExecuteOutOfPlace(AkAudioBuffer * in_pBuffer, AkAudioBuffer * out_pBuffer)
{
#ifdef PERFORMANCE_BENCHMARK
	AkInt64 TimeBefore;
	AKPLATFORM::PerformanceCounter(&TimeBefore);
#endif

	if (m_InternalBQFState.m_LPF.IsInitialized())
	{
		_ExecuteOutOfPlace<AkLpfParamEval>(in_pBuffer, out_pBuffer, m_InternalBQFState.m_LPFParams, m_InternalBQFState.m_LPF);
		if (m_InternalBQFState.m_HPF.IsInitialized())
			_Execute<AkHpfParamEval>(out_pBuffer, m_InternalBQFState.m_HPFParams, m_InternalBQFState.m_HPF);
	}
	else if (m_InternalBQFState.m_HPF.IsInitialized())
		_ExecuteOutOfPlace<AkHpfParamEval>(in_pBuffer, out_pBuffer, m_InternalBQFState.m_HPFParams, m_InternalBQFState.m_HPF);

#ifdef PERFORMANCE_BENCHMARK
	AkInt64 TimeAfter;
	AKPLATFORM::PerformanceCounter(&TimeAfter);
	AkReal32 fElapsed = AKPLATFORM::Elapsed(TimeAfter, TimeBefore);
	m_fTotalTime += fElapsed;
	++m_uNumberCalls;
#endif
}

// Bypass call for both filters, to be called when an temp stack buffer is not allocated
void CAkSrcLpHpFilter::BypassMono( AkAudioBuffer * in_pBuffer, AkUInt32 in_filterChannel )
{
#ifdef PERFORMANCE_BENCHMARK
	AkInt64 TimeBefore;
	AKPLATFORM::PerformanceCounter( &TimeBefore );
#endif
	// Only call this after dc is removed - both filters are initialized or neither
	if ( IsInitialized() )
	{
		AKASSERT(m_InternalBQFState.m_LPFParams.IsDcRemoved() && m_InternalBQFState.m_HPFParams.IsDcRemoved());
		AKASSERT(in_pBuffer != NULL && in_pBuffer->GetChannel(0) != NULL);
		AKASSERT(in_pBuffer->MaxFrames() != 0 && in_pBuffer->uValidFrames <= in_pBuffer->MaxFrames());

		const AkUInt32 uNumFrames = in_pBuffer->uValidFrames;
		if (uNumFrames >= 2)
		{
			// We must maintain the memory of the filter buffers so that we are ready to process values when we resule Filtering (unbypassing)
			//Reset the memory of the filter to the current signal.  Better than starting from 0.
			AkReal32 * pfBuf = in_pBuffer->GetChannel(in_filterChannel);

			AkReal32 fMem1 = pfBuf[uNumFrames - 1];
			AkReal32 fMem2 = pfBuf[uNumFrames - 2];

			m_InternalBQFState.m_LPF.SetMemories(in_filterChannel, fMem1, fMem2, fMem1, fMem2);//sending 2 last samples
			m_InternalBQFState.m_HPF.SetMemories(in_filterChannel, fMem1, fMem2, fMem1, fMem2);//sending 2 last samples
		}
	}

#ifdef PERFORMANCE_BENCHMARK
	AkInt64 TimeAfter;
	AKPLATFORM::PerformanceCounter( &TimeAfter );
	AkReal32 fElapsed = AKPLATFORM::Elapsed( TimeAfter, TimeBefore );
	m_fTotalTime += fElapsed;
	++m_uNumberCalls;
#endif
}

// Regular process called, to be called when a temp stack buffer is allocated
AkUInt16 CAkSrcLpHpFilter::ExecuteMono( AkAudioBuffer * in_pBuffer, AkAudioBuffer * out_pBuffer, AkUInt32 in_filterChannel )
{
#ifdef PERFORMANCE_BENCHMARK
	AkInt64 TimeBefore;
	AKPLATFORM::PerformanceCounter( &TimeBefore );
#endif
	AkUInt16 uLocalNumInterBlocks = 0;

	// CAkSrcLpHpFilter::Init() enforces that both filters should be initialized to run audio
	if ( IsInitialized() )
	{
		// Out of place
		if ( AK_EXPECT_FALSE( !m_InternalBQFState.m_LPFParams.IsBypassed() ) )
		{
			uLocalNumInterBlocks = _ExecuteMonoOutOfPlace<AkLpfParamEval>( in_pBuffer, out_pBuffer, m_InternalBQFState.m_LPFParams, m_InternalBQFState.m_LPF, in_filterChannel );
		}
		else
		{
			_BypassMonoOutOfPlace( in_pBuffer, out_pBuffer, m_InternalBQFState.m_LPFParams, m_InternalBQFState.m_LPF, in_filterChannel );
		}

		// In place
		if (AK_EXPECT_FALSE( !m_InternalBQFState.m_HPFParams.IsBypassed() ))
		{
			uLocalNumInterBlocks = _ExecuteMonoInPlace<AkHpfParamEval>( out_pBuffer, m_InternalBQFState.m_HPFParams, m_InternalBQFState.m_HPF, in_filterChannel );
		}
		else
		{
			_BypassMonoInPlace( out_pBuffer, m_InternalBQFState.m_HPFParams, m_InternalBQFState.m_HPF, in_filterChannel );
		}
	}
	else
	{
		// ExecuteMono should only be called if filters are init, but copy to the output buffer to avoid garbage
		const AkUInt32 uNumFrames = in_pBuffer->uValidFrames;

		AkReal32 * in_pfBuf = in_pBuffer->GetChannel( in_filterChannel );
		AkReal32 * out_pfBuf = out_pBuffer->GetChannel( 0 );

		AKPLATFORM::AkMemCpy( out_pfBuf, in_pfBuf, uNumFrames * sizeof( AkReal32 ) );
	}
#ifdef PERFORMANCE_BENCHMARK
	AkInt64 TimeAfter;
	AKPLATFORM::PerformanceCounter( &TimeAfter );
	AkReal32 fElapsed = AKPLATFORM::Elapsed( TimeAfter, TimeBefore );
	m_fTotalTime += fElapsed;
	++m_uNumberCalls;
#endif

	return uLocalNumInterBlocks;
}

bool CAkSrcLpHpFilter::ManageBQFChange()
{
	// If either filter is not initialized here, return bypass = true
	bool res = true;
	if ( IsInitialized() )
	{
		res = _ManageBQFChange<AkLpfParamEval>( m_InternalBQFState.m_LPFParams, m_InternalBQFState.m_LPF ) && res;
		res = _ManageBQFChange<AkHpfParamEval>( m_InternalBQFState.m_HPFParams, m_InternalBQFState.m_HPF ) && res;
	}

	return res;
}
