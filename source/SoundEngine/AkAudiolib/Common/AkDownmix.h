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
// AkDownmix.h
// 
// Downmix recipes.
//
//////////////////////////////////////////////////////////////////////
#pragma once

#include <AK/SoundEngine/Common/AkTypes.h>
#include "AkSpeakerVolumesEx.h"

// Channel downmix equations.
namespace AkDownmix
{
/// LX Move to SDK?
#define AK_ALL_SUPPORTED_SPEAKERS_DOWNMIX	( AK_SPEAKER_SETUP_7 | AK_SPEAKER_BACK_CENTER )
#define AK_ALL_SUPPORTED_SPEAKERS			( AK_ALL_SUPPORTED_SPEAKERS_DOWNMIX | AK_SPEAKER_TOP |AK_SPEAKER_HEIGHT_FRONT_LEFT | AK_SPEAKER_HEIGHT_FRONT_CENTER	| AK_SPEAKER_HEIGHT_FRONT_RIGHT	| AK_SPEAKER_HEIGHT_BACK_LEFT | AK_SPEAKER_HEIGHT_BACK_CENTER | AK_SPEAKER_HEIGHT_BACK_RIGHT )

	// Downmix matrices.
	// IMPORTANT: non-7.1 platforms must have values for both side and back channels, 
	// and they must be equal, because of the historical confusion between surround and back channels.

	// Note: Mono downmixing is scaled by ONE_OVER_SQRT_OF_TWO in order to have 
	// constant power stereo and full scale mono (center) yielding the same value.
	static const AkReal32 s_fToMono[][1] = 
	{
		{ ONE_OVER_SQRT_OF_TWO },	// FL	( ONE_OVER_SQRT_OF_TWO * 1 )
		{ ONE_OVER_SQRT_OF_TWO },	// FR
#ifdef AK_LFECENTER
		{ 1.f },					// FC	( ONE_OVER_SQRT_OF_TWO * 2 * ONE_OVER_SQRT_OF_TWO )
		{ 0.f },					// LFE
#endif	
#ifdef AK_REARCHANNELS
		{ 0.5f },					// BL	( ONE_OVER_SQRT_OF_TWO * ONE_OVER_SQRT_OF_TWO )
		{ 0.5f },					// BR
		{ 0.f },					// N/A
		{ 0.f },					// N/A
		{ 0.5f },					// BC
		{ 0.5f },					// SL
		{ 0.5f },					// SR
#endif	// AK_REARCHANNELS
#ifdef AK_71AUDIO
		{ 1.f },					// Top	( ONE_OVER_SQRT_OF_TWO * ONE_OVER_SQRT_OF_TWO * 2 )
		{ 0.5f },					// HFL	( ONE_OVER_SQRT_OF_TWO * ONE_OVER_SQRT_OF_TWO )
		{ ONE_OVER_SQRT_OF_TWO },	// HFC	( ONE_OVER_SQRT_OF_TWO * 0.5 * 2 )
		{ 0.5f },					// HFR	( ONE_OVER_SQRT_OF_TWO * ONE_OVER_SQRT_OF_TWO )
		{ 0.5f * ONE_OVER_SQRT_OF_TWO },// HBL	( ONE_OVER_SQRT_OF_TWO * 0.5 )
		{ 0.f },						// HBC
		{ 0.5f * ONE_OVER_SQRT_OF_TWO }	// HBR
#endif	// AK_71AUDIO
	};
	
	static const AkReal32 s_fToStereo[][2] = 
	{
		{ 1.f, 0.f },									// FL
		{ 0.f, 1.f },									// FR
#ifdef AK_LFECENTER
		{ONE_OVER_SQRT_OF_TWO, ONE_OVER_SQRT_OF_TWO },	// FC
		{ 0.f, 0.f },									// LFE
#endif
#ifdef AK_REARCHANNELS
		{ONE_OVER_SQRT_OF_TWO, 0.f },					// BL
		{0.f , ONE_OVER_SQRT_OF_TWO },					// BR
		{ 0.f, 0.f },									// N/A
		{ 0.f, 0.f },									// N/A
		{ONE_OVER_SQRT_OF_TWO, ONE_OVER_SQRT_OF_TWO },	// BC
		{ONE_OVER_SQRT_OF_TWO, 0.f },					// SL
		{0.f , ONE_OVER_SQRT_OF_TWO },					// SR
#ifdef AK_71AUDIO
		{ ONE_OVER_SQRT_OF_TWO, ONE_OVER_SQRT_OF_TWO },	// Top
		{ ONE_OVER_SQRT_OF_TWO, 0.f },					// HFL
		{ 0.5f, 0.5f },									// HFC
		{ 0.f, ONE_OVER_SQRT_OF_TWO },					// HFR
		{ 0.5f, 0.f },									// HBL
		{ 0.f, 0.f },									// HBC
		{ 0.f, 0.5f }									// HBR
#endif	// AK_71AUDIO
#endif	// AK_REARCHANNELS
	};

#ifdef AK_LFECENTER
#define ONE_OVER_SQRT_OF_THREE	(0.57735026918962576450914878050196)
	static const AkReal32 s_fToThree[][3] = 
	{
		{ 1.f, 0.f, 0.f },								// FL
		{ 0.f, 1.f, 0.f },								// FR
		{ 0.f, 0.f, 1.f },								// FC
		{ 0.f, 0.f, 0.f },								// LFE
#ifdef AK_REARCHANNELS
		{ONE_OVER_SQRT_OF_TWO, 0.f, 0.f },				// BL
		{0.f , ONE_OVER_SQRT_OF_TWO, 0.f },				// BR
		{ 0.f, 0.f, 0.f },								// N/A
		{ 0.f, 0.f, 0.f },								// N/A
		{0.f, 0.f, ONE_OVER_SQRT_OF_TWO },				// BC
		{ONE_OVER_SQRT_OF_TWO, 0.f, 0.f },				// SL
		{0.f , ONE_OVER_SQRT_OF_TWO, 0.f },				// SR
#ifdef AK_71AUDIO
		{ ONE_OVER_SQRT_OF_THREE, ONE_OVER_SQRT_OF_THREE, ONE_OVER_SQRT_OF_THREE },	// Top
		{ ONE_OVER_SQRT_OF_TWO, 0.f, 0.f },				// HFL
		{ 0.f, 0.f, ONE_OVER_SQRT_OF_TWO },				// HFC
		{ 0.f, ONE_OVER_SQRT_OF_TWO, 0.f },				// HFR
		{ 0.5f, 0.f, 0.f },								// HBL
		{ 0.f, 0.f, 0.f },								// HBC
		{ 0.f, 0.5f, 0.f }								// HBR
#endif	// AK_71AUDIO
#endif	// AK_REARCHANNELS
	};

#endif	// AK_LFECENTER

#ifdef AK_REARCHANNELS
	static const AkReal32 s_fToFour[][4] = 
	{
		{ 1.f, 0.f, 0.f, 0.f },							// FL
		{ 0.f, 1.f, 0.f, 0.f },							// FR
		{ ONE_OVER_SQRT_OF_TWO, ONE_OVER_SQRT_OF_TWO, 0.f, 0.f },	// FC
		{ 0.f, 0.f, 0.f, 0.f },							// LFE
		{ 0.f, 0.f, 1.f, 0.f },							// BL
		{ 0.f, 0.f, 0.f, 1.f },							// BR
		{ 0.f, 0.f, 0.f, 0.f },							// N/A
		{ 0.f, 0.f, 0.f, 0.f },							// N/A
		{ ONE_OVER_SQRT_OF_TWO, ONE_OVER_SQRT_OF_TWO, 0.f, 0.f },	// BC
		{ 0.f, 0.f, 1.f, 0.f },							// SL
		{ 0.f, 0.f, 0.f, 1.f },							// SR
#ifdef AK_71AUDIO
		{ 0.5f, 0.5f, 0.5f, 0.5f },						// Top
		{ ONE_OVER_SQRT_OF_TWO, 0.f, 0.f, 0.f },		// HFL
		{ 0.5f, 0.5f, 0.f, 0.f },						// HFC
		{ 0.f, ONE_OVER_SQRT_OF_TWO, 0.f, 0.f },		// HFR
		{ 0.f, 0.f, ONE_OVER_SQRT_OF_TWO, 0.f },		// HBL
		{ 0.f, 0.f, 0.f, 0.f },							// HBC
		{ 0.f, 0.f, 0.f, ONE_OVER_SQRT_OF_TWO }			// HBR
#endif	// #ifdef AK_71AUDIO
	};

#ifdef AK_LFECENTER
#define ONE_OVER_SQRT_OF_FIVE	(0.44721359549995793928183473374626f)
	static const AkReal32 s_fToFive[][5] = 
	{
		{ 1.f, 0.f, 0.f, 0.f, 0.f },					// FL
		{ 0.f, 1.f, 0.f, 0.f, 0.f },					// FR
		{ 0.f, 0.f, 1.f, 0.f, 0.f },					// FC
		{ 0.f, 0.f, 0.f, 0.f, 0.f },					// LFE
		{ 0.f, 0.f, 0.f, 1.f, 0.f },					// BL
		{ 0.f, 0.f, 0.f, 0.f, 1.f },					// BR
		{ 0.f, 0.f, 0.f, 0.f, 0.f },					// N/A
		{ 0.f, 0.f, 0.f, 0.f, 0.f },					// N/A
		{ 0.f, 0.f, ONE_OVER_SQRT_OF_TWO, 1.f, 0.f },	// BC
		{ 0.f, 0.f, 0.f, 1.f, 0.f },					// SL
		{ 0.f, 0.f, 0.f, 0.f, 1.f },					// SR
#ifdef AK_71AUDIO
		{ ONE_OVER_SQRT_OF_FIVE, ONE_OVER_SQRT_OF_FIVE, ONE_OVER_SQRT_OF_FIVE, ONE_OVER_SQRT_OF_FIVE, ONE_OVER_SQRT_OF_FIVE },	// Top
		{ ONE_OVER_SQRT_OF_TWO, 0.f, 0.f, 0.f, 0.f },	// HFL
		{ 0.f, 0.f, ONE_OVER_SQRT_OF_TWO, 0.f, 0.f },	// HFC
		{ 0.f, ONE_OVER_SQRT_OF_TWO, 0.f, 0.f, 0.f },	// HFR
		{ 0.f, 0.f, 0.f, ONE_OVER_SQRT_OF_TWO, 0.f },	// HBL
		{ 0.f, 0.f, 0.f, 0.f, 0.f },					// HBC
		{ 0.f, 0.f, 0.f, 0.f, ONE_OVER_SQRT_OF_TWO }	// HBR
#endif	// #ifdef AK_71AUDIO
	};
#endif	// AK_LFECENTER
#endif	// AK_REARCHANNELS

#ifdef AK_71AUDIO
#define ONE_OVER_SQRT_OF_SIX	(0.40824829046386301636621401245098f)
	static const AkReal32 s_fToSix[][6] = 
	{
		{ 1.f, 0.f, 0.f, 0.f, 0.f, 0.f },				// FL
		{ 0.f, 1.f, 0.f, 0.f, 0.f, 0.f },				// FR
		{ ONE_OVER_SQRT_OF_TWO, ONE_OVER_SQRT_OF_TWO, 0.f, 0.f, 0.f, 0.f },	// FC
		{ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f },				// LFE
		{ 0.f, 0.f, 1.f, 0.f, 0.f, 0.f },				// BL
		{ 0.f, 0.f, 0.f, 1.f, 0.f, 0.f },				// BR
		{ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f },				// N/A
		{ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f },				// N/A
		{ ONE_OVER_SQRT_OF_TWO, ONE_OVER_SQRT_OF_TWO, 0.f, 0.f, 0.f, 0.f },	// BC
		{ 0.f, 0.f, 0.f, 0.f, 1.f, 0.f },				// SL
		{ 0.f, 0.f, 0.f, 0.f, 0.f, 1.f },				// SR
		{ ONE_OVER_SQRT_OF_SIX, ONE_OVER_SQRT_OF_SIX, ONE_OVER_SQRT_OF_SIX, ONE_OVER_SQRT_OF_SIX, ONE_OVER_SQRT_OF_SIX, ONE_OVER_SQRT_OF_SIX },	// Top
		{ ONE_OVER_SQRT_OF_TWO, 0.f, 0.f, 0.f, 0.f, 0.f },	// HFL
		{ 0.5f, 0.5f, 0.f, 0.f, 0.f, 0.f },					// HFC
		{ 0.f, ONE_OVER_SQRT_OF_TWO, 0.f, 0.f, 0.f, 0.f },	// HFR
		{ 0.f, 0.f, 0.f, ONE_OVER_SQRT_OF_TWO, 0.f, 0.f },	// HBL
		{ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f },					// HBC
		{ 0.f, 0.f, 0.f, 0.f, ONE_OVER_SQRT_OF_TWO, 0.f }	// HBR		
	};

#define ONE_OVER_SQRT_OF_SEVEN	(0.37796447300922722721451653623418f)
	static const AkReal32 s_fToSeven[][7] = 
	{
		{ 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f },			// FL
		{ 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f },			// FR
		{ 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f },			// FC
		{ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f },			// LFE
		{ 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f },			// BL
		{ 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f },			// BR
		{ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f },			// N/A
		{ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f },			// N/A
		{ 0.f, 0.f, ONE_OVER_SQRT_OF_TWO, 0.f, 0.f, 0.f, 0.f },	// FC
		{ 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f },			// SL
		{ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f },			// SR
		{ ONE_OVER_SQRT_OF_SEVEN, ONE_OVER_SQRT_OF_SEVEN, ONE_OVER_SQRT_OF_SEVEN, ONE_OVER_SQRT_OF_SEVEN, ONE_OVER_SQRT_OF_SEVEN, ONE_OVER_SQRT_OF_SEVEN, ONE_OVER_SQRT_OF_SEVEN },	// Top
		{ ONE_OVER_SQRT_OF_TWO, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f },	// HFL
		{ 0.f, 0.f, ONE_OVER_SQRT_OF_TWO, 0.f, 0.f, 0.f, 0.f },	// HFC
		{ 0.f, ONE_OVER_SQRT_OF_TWO, 0.f, 0.f, 0.f, 0.f, 0.f },	// HFR
		{ 0.f, 0.f, 0.f, ONE_OVER_SQRT_OF_TWO, 0.f, 0.f, 0.f },	// HBL
		{ 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f },					// HBC
		{ 0.f, 0.f, 0.f, 0.f, ONE_OVER_SQRT_OF_TWO, 0.f, 0.f }	// HBR
	};
#endif	// AK_71AUDIO

#define AK_TRANSFORM_VOLUME_MX( _num_chan_proc_in_, _num_chan_proc_out_, _downmix_mx_ )									\
{																														\
	for ( AkUInt32 uIn = 0; uIn < _num_chan_proc_in_; uIn++ )															\
	{																													\
		AK::SpeakerVolumes::VectorPtr pInRow = AK::SpeakerVolumes::Matrix::GetChannel( in_pInputMx, uIn, in_configOutOrig.uNumChannels );	\
		AK::SpeakerVolumes::VectorPtr pOutRow = AK::SpeakerVolumes::Matrix::GetChannel( in_pOutputMx, uIn, in_uNumChannelsOut );			\
		AK::SpeakerVolumes::Vector::Zero( pOutRow, in_uNumChannelsOut );												\
		AkUInt32 uIdxDownMixRow = 0;																					\
		AkUInt32 uOutOrig = 0;																							\
		AkChannelMask uOutOrigBit = AK_SPEAKER_FRONT_LEFT;																\
		while ( uOutOrigBit <= in_configOutOrig.uChannelMask )															\
		{																												\
			if ( uOutOrigBit & in_configOutOrig.uChannelMask )															\
			{																											\
				for ( AkUInt32 uOut = 0; uOut < _num_chan_proc_out_; uOut++ )											\
					pOutRow[ uOut ] += pInRow[ uOutOrig ] * _downmix_mx_[uIdxDownMixRow][uOut];							\
				++uOutOrig;																								\
			}																											\
			++uIdxDownMixRow;																							\
			uOutOrigBit <<= 1;																							\
		}																												\
	}																													\
}

	// Transforms an INxOUT1 volume matrix into an INxOUT2 volume matrix by applying standard AC3 downmix recipes.
	// Use with standard configs only.
	// Configurations should not contain the LFE channel, as it is always processed separately.
	static inline void Transform( 
		AkChannelConfig in_configIn,		// Input configuration.
		AkChannelConfig in_configOutOrig,	// Original output configuration.
		AkChannelConfig in_configOutNoLFE,	// Desired output configuration with LFE stripped out.
		AkUInt32 in_uNumChannelsOut,		// Number of output channels in desired output configuration. May differ from in_configOutNoLFE because of LFE. Required to properly address in_pOutputMx.
		AK::SpeakerVolumes::MatrixPtr in_pInputMx,	// Volume matrix.
		AK::SpeakerVolumes::MatrixPtr in_pOutputMx	// Transformed volume matrix.
		)
	{
		AKASSERT( in_configIn.eConfigType == AK_ChannelConfigType_Standard
				&& in_configOutOrig.eConfigType == AK_ChannelConfigType_Standard
				&& in_configOutNoLFE.eConfigType == AK_ChannelConfigType_Standard );
		AKASSERT( ( in_configOutNoLFE.uChannelMask & ~AK_ALL_SUPPORTED_SPEAKERS ) == 0 );
		AKASSERT( !in_configIn.HasLFE() && !in_configOutNoLFE.HasLFE() );

#ifdef AK_71AUDIO
		AkChannelMask uMaskOutPlane = in_configOutNoLFE.uChannelMask & AK_ALL_SUPPORTED_SPEAKERS_DOWNMIX;
		AkChannelMask uMaskInPlane = in_configIn.uChannelMask;
		AkUInt32 uNumChannelsIn = ( uMaskInPlane != in_configIn.uChannelMask ) ? AK::GetNumNonZeroBits( uMaskInPlane ) : in_configIn.uNumChannels;
#else
		AkChannelMask uMaskOutPlane = in_configOutNoLFE.uChannelMask;
		AkUInt32 uNumChannelsIn = in_configIn.uNumChannels;
#endif

		switch ( uMaskOutPlane )
		{
		case AK_SPEAKER_SETUP_STEREO:
			AK_TRANSFORM_VOLUME_MX( uNumChannelsIn, 2, s_fToStereo );
			break;
#ifdef AK_LFECENTER
		case AK_SPEAKER_SETUP_3STEREO:
			AK_TRANSFORM_VOLUME_MX( uNumChannelsIn, 3, s_fToThree );
			break;
#endif // AK_LFECENTER
#ifdef AK_REARCHANNELS
		case AK_SPEAKER_SETUP_4:
			AK_TRANSFORM_VOLUME_MX( uNumChannelsIn, 4, s_fToFour );
			break;
#endif // AK_REARCHANNELS
#if defined(AK_REARCHANNELS) && defined(AK_LFECENTER)
		case AK_SPEAKER_SETUP_5:
			AK_TRANSFORM_VOLUME_MX( uNumChannelsIn, 5, s_fToFive );
			break;
#endif // defined(AK_REARCHANNELS) && defined(AK_LFECENTER)
#ifdef AK_71AUDIO
		case AK_SPEAKER_SETUP_6:
			AK_TRANSFORM_VOLUME_MX( uNumChannelsIn, 6, s_fToSix );
			break;
		case AK_SPEAKER_SETUP_7:
			AK_TRANSFORM_VOLUME_MX( uNumChannelsIn, 7, s_fToSeven );
			break;
#endif // AK_71AUDIO
		case AK_SPEAKER_SETUP_MONO:
			AK_TRANSFORM_VOLUME_MX( uNumChannelsIn, 1, s_fToMono );		// Note: not used with 2D panning.
			break;
		default:
			// Unhandled channel configuration, or empty or LFE-only. Silent.
			break;
		}

		/// TODO Downmix "height" layer.
	}

#define AK_GEN_DIRECT_VOLUME_MX( _num_chan_proc_out_, _downmix_mx_ )												\
{																													\
	AkUInt32 uIn = 0;																								\
	AkUInt32 uIdxDownMixRow = 0;																					\
	AkChannelMask uChannelBit = AK_SPEAKER_FRONT_LEFT;																\
	while ( uChannelBit <= uMaskIn )																				\
	{																												\
		if ( uChannelBit & uMaskIn )																				\
		{																											\
			AK::SpeakerVolumes::VectorPtr pOutRow = AK::SpeakerVolumes::Matrix::GetChannel( in_pOutputMx, uIn, in_uNumChannelsOut );	\
			AK::SpeakerVolumes::Vector::Zero( pOutRow, in_uNumChannelsOut );										\
			AkUInt32 uOut = 0;																						\
			while ( uOut < _num_chan_proc_out_ )																	\
			{																										\
				pOutRow[ uOut ] = _downmix_mx_[uIdxDownMixRow][uOut];												\
				++uOut;																								\
			}																										\
			while ( uOut < in_uNumChannelsOut )																		\
			{																										\
				pOutRow[ uOut ] = 0.f;																				\
				++uOut;																								\
			}																										\
			++uIn;																									\
		}																											\
		++uIdxDownMixRow;																							\
		uChannelBit <<= 1;																							\
	}																												\
}

	// Computes values of an INxOUT volume matrix according to input and output channels and AC3 downmix recipes.
	// Use with standard configs only.
	// Configurations should not contain the LFE channel, as it is always processed separately.
	static inline void ComputeDirectSpeakerAssignment( 
		AkChannelConfig in_configIn,		// Input configuration.
		AkChannelConfig in_configOutNoLfe,	// Desired output configuration with LFE stripped out.
		AkUInt32 in_uNumChannelsOut,		// Number of output channels in desired output configuration. May differ from in_configOutNoLFE because of LFE. Required to properly address in_pOutputMx.
		AK::SpeakerVolumes::MatrixPtr in_pOutputMx	// Returned volume matrix.
		)
	{
		AKASSERT( in_configIn.eConfigType == AK_ChannelConfigType_Standard
				&& in_configOutNoLfe.eConfigType == AK_ChannelConfigType_Standard );
		AKASSERT( ( in_configOutNoLfe.uChannelMask & ~AK_ALL_SUPPORTED_SPEAKERS ) == 0 );
		AKASSERT( !in_configIn.HasLFE() && !in_configOutNoLfe.HasLFE() );

#ifdef AK_71AUDIO
		AkChannelMask uMask2DOut = in_configOutNoLfe.uChannelMask & AK_ALL_SUPPORTED_SPEAKERS_DOWNMIX;
		AkChannelMask uMaskIn = in_configIn.uChannelMask;
#else
		AkChannelMask uMask2DOut = in_configOutNoLfe.uChannelMask;
		AkChannelMask uMaskIn = in_configIn.uChannelMask;
#endif

		switch ( uMask2DOut )
		{
		case AK_SPEAKER_SETUP_MONO:
			// Note: stereo to mono mixdown needs to be scaled in order to ensure that 
			// constant power stereo and full scale mono (center) yield the same value.
			AK_GEN_DIRECT_VOLUME_MX( 1, s_fToMono );
			break;
		case AK_SPEAKER_SETUP_STEREO:
			AK_GEN_DIRECT_VOLUME_MX( 2, s_fToStereo );
			break;
#ifdef AK_LFECENTER
		case AK_SPEAKER_SETUP_3STEREO:
			AK_GEN_DIRECT_VOLUME_MX( 3, s_fToThree );
			break;
#endif // AK_LFECENTER
#ifdef AK_REARCHANNELS
		case AK_SPEAKER_SETUP_4:
			AK_GEN_DIRECT_VOLUME_MX( 4, s_fToFour );
			break;
#endif // AK_REARCHANNELS
#if defined(AK_REARCHANNELS) && defined(AK_LFECENTER)
		case AK_SPEAKER_SETUP_5:
			AK_GEN_DIRECT_VOLUME_MX( 5, s_fToFive );
			break;
#endif // defined(AK_REARCHANNELS) && defined(AK_LFECENTER)
#ifdef AK_71AUDIO
		case AK_SPEAKER_SETUP_6:
			AK_GEN_DIRECT_VOLUME_MX( 6, s_fToSix );
			break;
		case AK_SPEAKER_SETUP_7:
			AK_GEN_DIRECT_VOLUME_MX( 7, s_fToSeven );
			break;
#endif // AK_71AUDIO
		default:
			// Unhandled channel configuration, or empty or LFE-only. Silent.
			break;
		}

		// Route additional input channels directly to their corresponding output channels.
		/// TODO Downmix "height" layer.
#ifdef AK_71AUDIO
		if (AK::HasHeightChannels(in_configOutNoLfe.uChannelMask)
			&& AK::HasHeightChannels(in_configIn.uChannelMask))
		{
			AkUInt32 uOut = 0;
			AkChannelMask uChannelBitOut = 1;
			// Start with first height channel.
			AkUInt32 uIn = AK::GetNumNonZeroBits(uMaskIn & AK_SPEAKER_SETUP_DEFAULT_PLANE);
			AkChannelMask uChannelBit = AK_SPEAKER_TOP;
			while ( uChannelBit <= in_configIn.uChannelMask )
			{
				if ( uChannelBit & in_configIn.uChannelMask )
				{
					AK::SpeakerVolumes::VectorPtr pOutRow = AK::SpeakerVolumes::Matrix::GetChannel( in_pOutputMx, uIn, in_uNumChannelsOut );
					AK::SpeakerVolumes::Vector::Zero( pOutRow, in_uNumChannelsOut );
					if ( uChannelBit & in_configOutNoLfe.uChannelMask )
					{
						// Channel present in output too. Find it.
						while ( !( uChannelBitOut & uChannelBit ) )
						{
							if ( uChannelBitOut & in_configOutNoLfe.uChannelMask )
								++uOut;
							uChannelBitOut <<= 1;
						}
						pOutRow[ uOut ] = 1.f;
					}
					++uIn;
				}
				uChannelBit <<= 1;
			}
		}
#endif
	}
}
