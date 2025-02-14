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

//////////////////////////////////////////////////////////////////////
//
// AkResampler.cpp
// 
// Combines software resampling and pitch shifting opreation in one algorithm 
// using linear interpolation.
// Assumes same thread will call both SetPitch and Execute (not locking).
// There is some interpolation done on the pitch control parameter do avoid stepping behavior in transition
// or fast parameter changes.
// 
// We can think of latency / stepping problem by looking at rates at which different thinks occur:
// Control rate: rate at which SetPitch are called -> 1x per buffer (linked with buffer time)
// Interpolation rate: rate at which transitional pitch values are changed 
// -> NUMBLOCKTOREACHTARGET per buffer, necessary to avoid stepping while introduces up to 1 buffer latency
// Audio rate: rate at which samples are calculated == sample rate
// Simplifying assumption -> if its bypassed, its bypassed for the whole buffer
// It is possible to run the pitch algorithm with pitch 0.
//
/////////////////////////////////////////////////////////////////////

#include "stdafx.h" 
#include <AK/Tools/Common/AkPlatformFuncs.h>
#include "AkCommon.h"
#include "AkMath.h"
#include "AkResampler.h"
#include "AkResamplerCommon.h"
#include "AkRuntimeEnvironmentMgr.h"
#include "AkSettings.h"
#include <math.h>

#ifdef PERFORMANCE_BENCHMARK
#include "AkMonitor.h"
#endif

typedef AKRESULT (*PitchDSPFuncPtr) (	AkAudioBuffer * io_pInBuffer, 
										AkAudioBuffer * io_pOutBuffer,
										AkUInt32 uRequestedFrames,
										AkInternalPitchState * io_pPitchState );

static PitchDSPFuncPtr PitchDSPFuncTable[NumPitchOperatingMode][NumInputDataType];

void CAkResampler::InitDSPFunctTable()
{
	//////////////////////////////////////////////////////////////////////////
	// Bypass-I16 setup
#if defined ( AK_CPU_ARM_NEON )
	PitchDSPFuncTable[PitchOperatingMode_Bypass][I16_1Chan] =				Bypass_I16_1ChanV4F32;
	PitchDSPFuncTable[PitchOperatingMode_Bypass][I16_2Chan] =				Bypass_I16_2ChanV4F32;
	PitchDSPFuncTable[PitchOperatingMode_Bypass][I16_NChan] =				Bypass_I16_NChanV4F32;
#else
	PitchDSPFuncTable[PitchOperatingMode_Bypass][I16_1Chan] =				Bypass_I16_NChan;
	PitchDSPFuncTable[PitchOperatingMode_Bypass][I16_2Chan] =				Bypass_I16_NChan;
	PitchDSPFuncTable[PitchOperatingMode_Bypass][I16_NChan] =				Bypass_I16_NChan;
#endif

#if defined( AK_CPU_X86 )
	if (AK::AkRuntimeEnvironmentMgr::Instance()->GetSIMDSupport(AK::AK_SIMD_SSE))
	{
		PitchDSPFuncTable[PitchOperatingMode_Bypass][I16_1Chan] =			Bypass_I16_1ChanSSE;
		PitchDSPFuncTable[PitchOperatingMode_Bypass][I16_2Chan] =			Bypass_I16_2ChanSSE;
		PitchDSPFuncTable[PitchOperatingMode_Bypass][I16_NChan] =			Bypass_I16_NChanSSE;
	}
#endif
#if defined ( AKSIMD_SSE2_SUPPORTED )
	if (AK::AkRuntimeEnvironmentMgr::Instance()->GetSIMDSupport(AK::AK_SIMD_SSE2))
	{
		PitchDSPFuncTable[PitchOperatingMode_Bypass][I16_1Chan] =			Bypass_I16_1ChanV4F32;
		PitchDSPFuncTable[PitchOperatingMode_Bypass][I16_2Chan] =			Bypass_I16_2ChanV4F32;
		PitchDSPFuncTable[PitchOperatingMode_Bypass][I16_NChan] =			Bypass_I16_NChanV4F32;
	}
#endif
#if defined( AKSIMD_AVX2_SUPPORTED )
	if (AK::AkRuntimeEnvironmentMgr::Instance()->GetSIMDSupport(AK::AK_SIMD_AVX2))
	{
		PitchDSPFuncTable[PitchOperatingMode_Bypass][I16_1Chan] =			Bypass_I16_1ChanAVX2;
		PitchDSPFuncTable[PitchOperatingMode_Bypass][I16_2Chan] =			Bypass_I16_2ChanAVX2;
		PitchDSPFuncTable[PitchOperatingMode_Bypass][I16_NChan] =			Bypass_I16_NChanAVX2;
	}
#endif

	//////////////////////////////////////////////////////////////////////////
	// Bypass-native setup
	// Note: 1Chan and 2 channel cases handled by N routine
	PitchDSPFuncTable[PitchOperatingMode_Bypass][Native_1Chan] =			Bypass_Native_NChan;
	PitchDSPFuncTable[PitchOperatingMode_Bypass][Native_2Chan] =			Bypass_Native_NChan;
	PitchDSPFuncTable[PitchOperatingMode_Bypass][Native_NChan] =			Bypass_Native_NChan;

	//////////////////////////////////////////////////////////////////////////
	// Fixed-I16 setup
	PitchDSPFuncTable[PitchOperatingMode_Fixed][I16_1Chan] =				Fixed_I16_1Chan;
	PitchDSPFuncTable[PitchOperatingMode_Fixed][I16_2Chan] =				Fixed_I16_2Chan;
	PitchDSPFuncTable[PitchOperatingMode_Fixed][I16_NChan] =				Fixed_I16_NChan;

#if defined ( AK_CPU_ARM_NEON )
	PitchDSPFuncTable[PitchOperatingMode_Fixed][I16_1Chan] =				Fixed_I16_1ChanV4F32;
	PitchDSPFuncTable[PitchOperatingMode_Fixed][I16_2Chan] =				Fixed_I16_2ChanV4F32;
	PitchDSPFuncTable[PitchOperatingMode_Fixed][I16_NChan] =				Fixed_I16_NChanV4F32;
#endif
#if defined ( AKSIMD_SSE2_SUPPORTED )
	if (AK::AkRuntimeEnvironmentMgr::Instance()->GetSIMDSupport(AK::AK_SIMD_SSE2))
	{
		PitchDSPFuncTable[PitchOperatingMode_Fixed][I16_1Chan] =			Fixed_I16_1ChanV4F32;
		PitchDSPFuncTable[PitchOperatingMode_Fixed][I16_2Chan] =			Fixed_I16_2ChanV4F32;
		PitchDSPFuncTable[PitchOperatingMode_Fixed][I16_NChan] =			Fixed_I16_NChanV4F32;
	}
#endif
#if defined( AKSIMD_AVX2_SUPPORTED )
	if (AK::AkRuntimeEnvironmentMgr::Instance()->GetSIMDSupport(AK::AK_SIMD_AVX2))
	{
		PitchDSPFuncTable[PitchOperatingMode_Fixed][I16_1Chan] =			Fixed_I16_1ChanAVX2;
		PitchDSPFuncTable[PitchOperatingMode_Fixed][I16_2Chan] =			Fixed_I16_2ChanAVX2;
		PitchDSPFuncTable[PitchOperatingMode_Fixed][I16_NChan] =			Fixed_I16_NChanAVX2;
	}
#endif

	//////////////////////////////////////////////////////////////////////////
	// Fixed-native setup
	PitchDSPFuncTable[PitchOperatingMode_Fixed][Native_1Chan] =				Fixed_Native_1Chan;
	PitchDSPFuncTable[PitchOperatingMode_Fixed][Native_2Chan] =				Fixed_Native_2Chan;
	PitchDSPFuncTable[PitchOperatingMode_Fixed][Native_NChan] =				Fixed_Native_NChan;

	//////////////////////////////////////////////////////////////////////////
	// Interpolating setup
	PitchDSPFuncTable[PitchOperatingMode_Interpolating][I16_1Chan] =		Interpolating_I16_1Chan;
	PitchDSPFuncTable[PitchOperatingMode_Interpolating][I16_2Chan] =		Interpolating_I16_2Chan;
	PitchDSPFuncTable[PitchOperatingMode_Interpolating][I16_NChan] =		Interpolating_I16_NChan;
	PitchDSPFuncTable[PitchOperatingMode_Interpolating][Native_1Chan] =		Interpolating_Native_1Chan;
	PitchDSPFuncTable[PitchOperatingMode_Interpolating][Native_2Chan] =		Interpolating_Native_2Chan;
	PitchDSPFuncTable[PitchOperatingMode_Interpolating][Native_NChan] =		Interpolating_Native_NChan;
}


// Constructor
CAkResampler::CAkResampler( )
	: m_PitchOperationMode(PitchOperatingMode_Bypass) // Will be set every time by SetPitch()
	, m_fSampleRateConvertRatio(1.f) // placeholder value until we receive sample rate in ::Init
	, m_fTargetPitchVal(0.f)
	, m_bFirstSetPitch(true)

{
	m_InternalPitchState.iLastValue = NULL;
	m_InternalPitchState.bLastValuesAllocated = false;
	m_InternalPitchState.uOutFrameOffset = 0;
	m_InternalPitchState.uInFrameOffset = 0;

	m_InternalPitchState.uFloatIndex = SINGLEFRAMEDISTANCE; // Initial index set to 1 -> 0 == previous buffer
	// Note: No need to set last values since float index initial value is 1 

	// Pitch interpolation variables
	m_InternalPitchState.uCurrentFrameSkip = 0;				// Current frame skip
	m_InternalPitchState.uTargetFrameSkip = 0;				// Target frame skip
	m_InternalPitchState.uInterpolationRampCount = 0;		// Sample count for pitch parameter interpolation 
}

// Destructor
CAkResampler::~CAkResampler( )
{

}

// Helper for computing frame skip. Pads against 0 (which would result in div by 0).
AkUInt32 CAkResampler::PitchToFrameSkip(AkReal32 in_fPitchVal)
{
	AkReal32 fFrameSkip = (m_fSampleRateConvertRatio * powf(2.f, in_fPitchVal / 1200.f)) * FPMUL + 0.5f;

	// Clamp to [1,UINT_MAX/2] - to avoid risk of int overflow, esp. when combined with casts to AkInt32
	// This restricts the effective range of in_fPitchVal to an upper limit of ~18,000 cents * m_fSampleRateConvertRatio,
	// compared with the UI-suggested limit of 2,400 cents
	if (fFrameSkip >= 2147483648.f) // compare fp values because casting large float values to int truncates values
	{
		return 0x7FFFFFFF;
	}
	else
	{
		return AkMax(1, (AkUInt32)(fFrameSkip));
	}
}

// Frame skip includes resampling ratio frameskip = 1 / (pitchratio * (target sr)/(src sr))
// 1 / pitch ratio = frameskip / ( (src sr) / (target sr) )
AkReal32 CAkResampler::GetLastRate() const
{ 
	return ((AkReal32)m_InternalPitchState.uCurrentFrameSkip / FPMUL ) / m_fSampleRateConvertRatio;
}

// Pass on internal pitch state (must be called after Init)
void CAkResampler::SetLastValues( AkReal32 * in_pfLastValues )
{
	if  ( ISI16TYPE( m_DSPFunctionIndex ) )
	{
		for ( AkUInt32 i = 0; i < m_uNumChannels; ++i )
		{
			m_InternalPitchState.iLastValue[i] = FLOAT_TO_INT16( in_pfLastValues[i] );
		}
	}
	else if  ( ISNATIVETYPE( m_DSPFunctionIndex ) )
	{
		for ( AkUInt32 i = 0; i < m_uNumChannels; ++i )
		{
			m_InternalPitchState.fLastValue[i] = in_pfLastValues[i];
		}
	}
	else
	{
		AKASSERT( !"Unsupported format." );
	}
}

// Retrieve internal pitch state
void CAkResampler::GetLastValues( AkReal32 * out_pfLastValues )
{
	if  ( ISI16TYPE( m_DSPFunctionIndex ) )
	{
		for ( AkUInt32 i = 0; i < m_uNumChannels; ++i )
		{
			out_pfLastValues[i] = INT16_TO_FLOAT( m_InternalPitchState.iLastValue[i] );
		}
	}
	else if  ( ISNATIVETYPE( m_DSPFunctionIndex ) )
	{
		for ( AkUInt32 i = 0; i < m_uNumChannels; ++i )
		{
			out_pfLastValues[i] = m_InternalPitchState.fLastValue[i];
		}
	}
	else
	{
		AKASSERT( !"Unsupported format." );
	}
}

void CAkResampler::SwitchTo( const AkAudioFormat & in_fmt, AkReal32 in_fPitch, AkAudioBuffer * io_pIOBuffer, AkUInt32 in_uSampleRate )
{
	AKASSERT( m_uNumChannels == in_fmt.GetNumChannels() );

	AkReal32 * fLastValues = (AkReal32*)AkAlloca( m_uNumChannels * sizeof( AkReal32 ) );
	GetLastValues( fLastValues );

	AkReal32 fNewSampleRateConvertRatio = (AkReal32) in_fmt.uSampleRate / in_uSampleRate;
	if ( m_fSampleRateConvertRatio != fNewSampleRateConvertRatio )
	{
		m_fSampleRateConvertRatio = fNewSampleRateConvertRatio;
		m_bFirstSetPitch = true;
	}

	SetPitch( in_fPitch, true );	// Sample-accurate transitions using "SwitchTo" are always in the actor-mixer hierarchy and thus use interpolation.

	m_DSPFunctionIndex = GetDSPFunctionIndex( in_fmt );

	SetLastValues( fLastValues );
}

AkUInt8 CAkResampler::GetDSPFunctionIndex( const AkAudioFormat & in_fmt ) const
{
	AkUInt8 uDSPFunctionIndex = 0;

	switch ( in_fmt.GetBitsPerSample() )
	{
	case 16:
		AKASSERT( in_fmt.GetInterleaveID() == AK_INTERLEAVED );
		switch ( m_uNumChannels )
		{
		case 1:
			uDSPFunctionIndex = I16_1Chan;
			break;
		case 2:
			uDSPFunctionIndex = I16_2Chan;
			break;
		default:
			uDSPFunctionIndex = I16_NChan;
			break;
		}
		break;
	case 32:
		AKASSERT( in_fmt.GetInterleaveID() == AK_NONINTERLEAVED );
		switch ( m_uNumChannels )
		{
		case 1:
			uDSPFunctionIndex = Native_1Chan;
			break;
		case 2:
			uDSPFunctionIndex = Native_2Chan;
			break;
		default:
			uDSPFunctionIndex = Native_NChan;
			break;
		}
		break;
	default:
		AKASSERT( !"Invalid sample resolution." );
		return 0xFF;
	}

	return uDSPFunctionIndex;
}

//-----------------------------------------------------------------------------
// Name: Init
// Desc: Setup converter to use appropriate conversion routine
//-----------------------------------------------------------------------------
AKRESULT CAkResampler::Init( AkAudioFormat * io_pFormat, AkUInt32 in_uSampleRate )
{
	// Allocate "last values" if required.
	AkUInt32 uNumChannels = io_pFormat->GetNumChannels();
	bool bIsNative = io_pFormat->GetBitsPerSample() == 32;
	if ( uNumChannels > AK_STANDARD_MAX_NUM_CHANNELS )
	{
		if ( bIsNative )
		{
			// Float.
			m_InternalPitchState.fLastValue = (AkReal32*)AkMalign( AkMemID_Processing, uNumChannels * sizeof(AkReal32), AK_SIMD_ALIGNMENT );
			if ( !m_InternalPitchState.fLastValue )
				return AK_Fail;

			for (AkUInt32 i = 0; i < uNumChannels; ++i)
				m_InternalPitchState.fLastValue[i] = 0.f;
		}
		else
		{
			// Int 16
			AKASSERT( io_pFormat->GetBitsPerSample() == 16 );
			m_InternalPitchState.iLastValue = (AkInt16*)AkMalign( AkMemID_Processing, uNumChannels * sizeof(AkInt16), AK_SIMD_ALIGNMENT );
			if ( !m_InternalPitchState.iLastValue )
				return AK_Fail;

			for (AkUInt32 i = 0; i < uNumChannels; ++i)
				m_InternalPitchState.iLastValue[i] = 0;
		}
		m_InternalPitchState.uChannelMapping = (AkUInt8*)AkAlloc(AkMemID_Processing, uNumChannels);
		m_InternalPitchState.bLastValuesAllocated = true;
	}
	else
	{
		m_InternalPitchState.iLastValue = m_InternalPitchState.iLastValueStatic;
		m_InternalPitchState.uChannelMapping = m_InternalPitchState.uChannelMappingStatic;
		m_InternalPitchState.bLastValuesAllocated = false;

		for (AkUInt32 i = 0; i < AK_STANDARD_MAX_NUM_CHANNELS; ++i)
			m_InternalPitchState.fLastValue[i] = 0.f;
	}

	// The interpolation must be resized
	// sample rate at 48000 khz -- interpolation over 1024	samples // Normal / high quality
	// sample rate at 24000 khz -- interpolation over 512	samples // Custom low PC
	// sample rate at 375 khz	-- interpolation over 8		samples // Motion
	m_InternalPitchState.uInterpolationRampInc = PITCHRAMPBASERATE / in_uSampleRate;

	// Set resampling ratio
	m_fSampleRateConvertRatio = (AkReal32) io_pFormat->uSampleRate / in_uSampleRate;
	m_uNumChannels = (AkUInt8)uNumChannels;
	
	m_DSPFunctionIndex = GetDSPFunctionIndex( *io_pFormat );

#ifdef PERFORMANCE_BENCHMARK
	m_fTotalTime = 0.f;
	m_uNumberCalls = 0;
#endif
	SetChannelMapping(io_pFormat->channelConfig, bIsNative ? AkPipelineChannelMappingFunc : AkWaveChannelMappingFunc);

	return m_DSPFunctionIndex < NumInputDataType ? AK_Success : AK_Fail;
}

//-----------------------------------------------------------------------------
// Name: Execute
//-----------------------------------------------------------------------------
AKRESULT CAkResampler::Execute(	AkAudioBuffer * io_pInBuffer, 
								AkAudioBuffer * io_pOutBuffer )
{
	AKASSERT( io_pInBuffer != NULL );
	AKASSERT( io_pOutBuffer != NULL );
	AKASSERT( m_InternalPitchState.uRequestedFrames <= io_pOutBuffer->MaxFrames() );

#ifdef PERFORMANCE_BENCHMARK
	AkInt64 TimeBefore;
	AKPLATFORM::PerformanceCounter( &TimeBefore ); 
#endif

	// Call appropriate DSP function

	if ( io_pInBuffer->uValidFrames == 0 )
		return AK_NoMoreData; // WG-15893

	AKRESULT eResult;
	do
	{
		eResult = (PitchDSPFuncTable[m_PitchOperationMode][m_DSPFunctionIndex])( io_pInBuffer, io_pOutBuffer, m_InternalPitchState.uRequestedFrames, &m_InternalPitchState );
		if ( m_PitchOperationMode == PitchOperatingMode_Interpolating && m_InternalPitchState.uInterpolationRampCount >= PITCHRAMPLENGTH )
		{
			m_InternalPitchState.uCurrentFrameSkip = m_InternalPitchState.uTargetFrameSkip;
			m_PitchOperationMode = PitchOperatingMode_Fixed;
			// Note: It is ok to go to fixed mode (even if it should have gone to bypass mode) for the remainder of this buffer
			// It will go back to bypass mode after next SetPitch() is called
		}
		
	} 
	while( io_pInBuffer->uValidFrames > 0 && 
		   io_pOutBuffer->uValidFrames < m_InternalPitchState.uRequestedFrames );

#ifdef PERFORMANCE_BENCHMARK
	AkInt64 TimeAfter;
	AKPLATFORM::PerformanceCounter( &TimeAfter );
	AkReal32 fElapsed = AKPLATFORM::Elapsed( TimeAfter, TimeBefore );
	m_fTotalTime += fElapsed;
	++m_uNumberCalls;
#endif

	return eResult;
}

AKRESULT CAkResampler::DeinterleaveAndSwapOutput( AkPipelineBufferBase * io_pIOBuffer )
{
	// Do this in a temporary allocated buffer and release the one provided (buffer swap) once its done).
	AkPipelineBufferBase DeinterleavedBuffer = *io_pIOBuffer;
	DeinterleavedBuffer.ClearData();
	AKRESULT eStatus = DeinterleavedBuffer.GetCachedBuffer( );
	if ( eStatus != AK_Success )
		return eStatus;

	DeinterleavedBuffer.uValidFrames = io_pIOBuffer->uValidFrames;
	AKASSERT(DeinterleavedBuffer.uValidFrames <= DeinterleavedBuffer.MaxFrames());

	Deinterleave_Native_NChan( io_pIOBuffer, &DeinterleavedBuffer );

	// Release pipeline buffer and swap for the deinterleaved buffer
	io_pIOBuffer->ReleaseCachedBuffer();
	static_cast<AkAudioBuffer&>(*io_pIOBuffer) = static_cast<const AkAudioBuffer&>(DeinterleavedBuffer); //Just overwrite AkAudioBuffer part

	return AK_Success;
}

AKRESULT CAkResampler::InterleaveAndSwapOutput( AkPipelineBufferBase* io_pIOBuffer )
{
	// Do this in a temporary allocated buffer and release the one provided (buffer swap) once its done).
	AkPipelineBufferBase InterleavedBuffer = *io_pIOBuffer;
	InterleavedBuffer.ClearData();
	AKRESULT eStatus = InterleavedBuffer.GetCachedBuffer( );
	if ( eStatus != AK_Success )
		return eStatus;

	InterleavedBuffer.uValidFrames = io_pIOBuffer->uValidFrames;
	AKASSERT(InterleavedBuffer.uValidFrames <= InterleavedBuffer.MaxFrames());

	Interleave_Native_NChan( io_pIOBuffer, &InterleavedBuffer );

	// Release pipeline buffer and swap for the deinterleaved buffer
	io_pIOBuffer->ReleaseCachedBuffer();
	static_cast<AkAudioBuffer&>(*io_pIOBuffer) = static_cast<const AkAudioBuffer&>(InterleavedBuffer); //Just overwrite AkAudioBuffer part

	return AK_Success;
}

bool CAkResampler::HasOffsets()
{
	return ( m_InternalPitchState.uOutFrameOffset || m_InternalPitchState.uInFrameOffset );
}

void CAkResampler::SetChannelMapping(const AkChannelConfig &config, AkChannelMappingFunc pfMapping)
{
	AKASSERT(pfMapping != nullptr);
	for (int i = 0; i < (int)config.uNumChannels; i++)
	{
		m_InternalPitchState.uChannelMapping[i] = pfMapping(config, i);
	}
}
void CAkResampler::ResetOffsets()
{
	m_InternalPitchState.uOutFrameOffset = 0;
	m_InternalPitchState.uInFrameOffset = 0;
}

//-----------------------------------------------------------------------------
// Name: Term
// Desc: Terminate conversion
//-----------------------------------------------------------------------------
void CAkResampler::Term()
{
	if ( m_InternalPitchState.bLastValuesAllocated )
	{
		AKASSERT( m_InternalPitchState.iLastValue );
		AkFalign( AkMemID_Processing, m_InternalPitchState.iLastValue );
		AkFree( AkMemID_Processing, m_InternalPitchState.uChannelMapping );
	}
	ResetOffsets();
#ifdef PERFORMANCE_BENCHMARK
	AkOSChar szString[64];
	AK_OSPRINTF( szString, 64, AKTEXT("%f\n"), m_fTotalTime/m_uNumberCalls );
	AKPLATFORM::OutputDebugMsg( szString );
	MONITOR_MSG( szString );
#endif
}

//-----------------------------------------------------------------------------
// Name: SetPitch
// Desc: Change pitch shift (value provided in cents)
//-----------------------------------------------------------------------------
void CAkResampler::SetPitch( AkReal32 in_fPitchVal, bool in_bInterpolate )
{
	if ( AK_EXPECT_FALSE( m_bFirstSetPitch ) )
	{	
		// No interpolation required
		AkUInt32 uFrameSkip = PitchToFrameSkip(in_fPitchVal);
		m_InternalPitchState.uTargetFrameSkip = m_InternalPitchState.uCurrentFrameSkip = uFrameSkip;
		m_InternalPitchState.uInterpolationRampCount = PITCHRAMPLENGTH;
		m_fTargetPitchVal = in_fPitchVal;
		m_bFirstSetPitch = false;
	}

	if ( in_fPitchVal != m_fTargetPitchVal )
	{
		if ( m_PitchOperationMode == PitchOperatingMode_Interpolating )
		{
			m_InternalPitchState.uCurrentFrameSkip = (AkInt32) m_InternalPitchState.uCurrentFrameSkip + 
				( (AkInt32) m_InternalPitchState.uTargetFrameSkip - (AkInt32) m_InternalPitchState.uCurrentFrameSkip ) *
				(AkInt32) m_InternalPitchState.uInterpolationRampCount / (AkInt32) PITCHRAMPLENGTH;

			if (m_InternalPitchState.uCurrentFrameSkip == 0)
				m_InternalPitchState.uCurrentFrameSkip = 1;
			else if (m_InternalPitchState.uCurrentFrameSkip > 0x7FFFFFFF)
				m_InternalPitchState.uCurrentFrameSkip = 0x7FFFFFFF;
		}

		// New pitch interpolation is required
		m_InternalPitchState.uInterpolationRampCount = 0;	
		m_InternalPitchState.uTargetFrameSkip = PitchToFrameSkip(in_fPitchVal);
		m_fTargetPitchVal = in_fPitchVal;

		if ( !in_bInterpolate )
			m_InternalPitchState.uCurrentFrameSkip = m_InternalPitchState.uTargetFrameSkip;
	}

	if ( m_InternalPitchState.uCurrentFrameSkip != m_InternalPitchState.uTargetFrameSkip )
	{	
		// Route to appropriate pitch interpolating DSP
		m_PitchOperationMode = PitchOperatingMode_Interpolating;
	}
	else
	{
		// No interpolation required.
		// Bypass if effective resampling is within our fixed point precision
		if ( m_InternalPitchState.uCurrentFrameSkip == SINGLEFRAMEDISTANCE )
		{
			// Note: route the next execute dereference to appropriate DSP
			m_PitchOperationMode = PitchOperatingMode_Bypass;
		}
		else
		{
			// Done with the pitch change but need to resample at constant ratio
			m_PitchOperationMode = PitchOperatingMode_Fixed;
		}
	}
}

void CAkResampler::SetPitchForTimeSkip( AkReal32 in_fPitchVal )
{
	if ( m_bFirstSetPitch || in_fPitchVal != m_fTargetPitchVal )
	{
		AkUInt32 uFrameSkip = PitchToFrameSkip(in_fPitchVal);
		m_InternalPitchState.uTargetFrameSkip = m_InternalPitchState.uCurrentFrameSkip = uFrameSkip;
		m_InternalPitchState.uInterpolationRampCount = PITCHRAMPLENGTH;
		m_fTargetPitchVal = in_fPitchVal;
		m_bFirstSetPitch = false;
	}
}


