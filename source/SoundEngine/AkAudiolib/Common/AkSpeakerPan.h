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
// AkSpeakerPan.h
//
//////////////////////////////////////////////////////////////////////
#ifndef _SPEAKER_PAN_H_
#define _SPEAKER_PAN_H_

#include "AkMath.h"
#include <AK/Tools/Common/AkKeyArray.h>
#include <AK/SoundEngine/Common/AkSimd.h>
#include <AK/SoundEngine/Common/AkSpeakerConfig.h>
#include <AK/SoundEngine/Common/AkSpeakerVolumes.h>

class AkDevice;

//====================================================================================================
// speaker pan
//====================================================================================================
class CAkSpeakerPan
{
public:
	static void Init();
	static void Term();

	static void GetSpeakerVolumesDirect(
		AkChannelConfig		in_inputConfig,
		AkChannelConfig		in_outputConfig,
		AkReal32			in_fCenterPerc,
		AK::SpeakerVolumes::MatrixPtr out_pVolumes
		);

	// Compute speaker matrix for 3D panning with spread.
	static void GetSpeakerVolumes(
		const AkTransform & in_emitter,
		AkReal32			in_fDivergenceCenter,
		AkReal32			in_fSpread,
		AkReal32			in_fFocus,
		AK::SpeakerVolumes::MatrixPtr out_pVolumes,
		AkChannelConfig		in_supportedInputConfig, // LFE and height channels not supported with standard configs.
		AkChannelMask		in_uSelInputChannelMask, // Mask of selected input channels
		AkChannelConfig		in_outputConfig,	// config of bus to which this signal is routed.
		const AkVector &	in_listPosition,	// listener position
		const AkReal32		in_listRot[3][3],	// listener rotation
		AkDevice *			in_pDevice
		);

	static void GetSpeakerVolumes2DPan(
		AkReal32			in_fX,			// [0..1] // 0 = full left, 1 = full right
		AkReal32			in_fY,			// [0..1] // 0 = full rear, 1 = full front
		AkReal32			in_fCenterPct,	// [0..1]
		AkSpeakerPanningType	in_ePannerType,
		AkChannelConfig		in_inputConfig,
		AkChannelConfig		in_outputConfig,
		AK::SpeakerVolumes::MatrixPtr out_pVolumes,
		AkDevice *			in_pDevice
		);

	static void GetDefaultSpeakerAngles(
		AkChannelConfig	in_channelConfig,	// Desired channel config.
		AkReal32 		out_angles[],		// Returned angles for given channel config. Must be preallocated (use AK::GetNumberOfAnglesForConfig()).
		AkReal32 &		out_fHeightAngle	// Returned height in degrees.
		);

	static AKRESULT SetSpeakerAngles(
		const AkReal32 *	in_pfSpeakerAngles,			// Array of loudspeaker pair angles, expressed in degrees relative to azimuth ([0,180]).
		AkUInt32			in_uNumAngles,				// Number of loudspeaker pair angles.
		AkReal32 *			out_fSpeakerAngles,			// Pointer to buffer filled with speaker angles (rad).
		AkReal32 &			out_fMinAngleBetweenSpeakers// Returned minimum angle between speakers (rad).
		);

	// Convert angles stored into device (as integers) to SDK-friendly speaker angles (in radians)
	static void ConvertSpeakerAngles(
		AkReal32			in_fRadians[],				// Speaker angles stored in device (rad).
		AkUInt32			in_uNumAngles,				// Number of angle values.
		AkReal32			out_arAngles[]				// Returned speaker angles, in degrees (see AkOutputSettings::fSpeakerAngles). Pass in an array of size in_uNumAngles.
		);

	typedef CAkKeyArray<AkChannelConfig, void*> MapConfig2PanCache;

	struct ConfigInOut
	{
		AkChannelConfig in;
		AkChannelConfig out;

		bool operator==(const ConfigInOut & in_rOther) const
		{
			return (in == in_rOther.in && out == in_rOther.out);
		}
	};
	typedef CAkKeyArray<ConfigInOut, AK::SpeakerVolumes::MatrixPtr> MapConfig2DecodeMx;

	static void CreatePanCache(
		AkChannelConfig		in_outputConfig,	// config of bus to which this signal is routed.
		AkReal32			in_fSpeakerAngles[],// Speaker angles (rad).
		AkReal32			in_fHeightAngle,	// Height speaker angle (rad).
		void *&				io_pPanCache		// Allocated or unallocated array of opaque data for various panners, allocated herein if needed and returned filled with values.
		);
	static void DestroyPanCache(
		AkChannelConfig		in_outputConfig,	// config of bus to which this signal is routed.
		void *&				io_pPanCache		// Allocated or unallocated array of opaque data for various panners, allocated herein if needed and returned filled with values.
		);

	struct ConfigIn2d3d
	{
		AkUInt32 uNumChansIn	:31;
		AkUInt32 b3D			:1;

		bool operator==(const ConfigIn2d3d & in_rOther) const
		{
			return (uNumChansIn == in_rOther.uNumChansIn && b3D == in_rOther.b3D);
		}
	};
	typedef AkArray< AKSIMD_V4F32, const AKSIMD_V4F32 &, AkArrayAllocatorAlignedSimd< AkMemID_Object >, AkGrowByPolicy_NoGrow > ArraySimdVector;
	typedef MapStruct< ConfigIn2d3d, ArraySimdVector > CacheItem;
	struct AkCacheItemMovePolicy
	{
		static AkForceInline void Move(CacheItem& in_Dest, CacheItem& in_Src)
		{
			in_Dest.key = in_Src.key;
			in_Dest.item.Transfer(in_Src.item);
		}

		static AkForceInline bool IsTrivial() { return false; }
	};
	typedef CAkKeyArray<ConfigIn2d3d, ArraySimdVector, ArrayPoolDefault, AkGrowByPolicy_DEFAULT, AkCacheItemMovePolicy > MapConfig2Spread;

	static AKRESULT CreateSpreadCache(
		ConfigIn2d3d		key,				// Plane or 3D config
		AkReal32			in_fOneOverMinAngleBetweenSpeakers,		// 1 / min angle between speakers ("PAN_CIRCLE" degrees).
		ArraySimdVector &	out_arSimdVectors	// Returned array filled with spread points
		);

	// Compute vector-based amplitude gains on the plane.
	static AKRESULT ComputePlanarVBAPGains(
		AkDevice *			in_pDevice,					// Output device.
		AkReal32			in_fAngle,					// Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are clockwise)
		AkChannelConfig		in_outputConfig,			// Desired output configuration. 
		AkReal32			in_fCenterPerc,				// Center percentage. Only applies to mono inputs to outputs that have no center.
		AK::SpeakerVolumes::VectorPtr out_vVolumes		// Returned volumes.
		);

	/// Not used.
	//Returns an angle in range [-PI, PI]
	static AkForceInline AkReal32 CartesianToPolar(AkReal32 in_fX, AkReal32 in_fY)
	{
		AkReal32 fAngle;
		//convert to polar coordinates
		if (in_fX == 0.0f)
		{
			if (in_fY > 0.0f)
				fAngle = PIOVERTWO;
			else if (in_fY == 0.0f)
				fAngle = 0; // Speaker-coordinate extension: we want x=y=0 case to be like a source straight ahead.
			else
				fAngle = -PIOVERTWO;
		}
		else
		{
			fAngle = atan2f(in_fY, in_fX);
		}
		return fAngle;
	}

	// Compute angles from left-handed rectangular coordinates, where xz is the plane (phi=0) and y is the elevation axis.
	// Distance should have been precomputed, and be strictly above 0.
	static AkForceInline void CartesianToSpherical(
		AkReal32 x,
		AkReal32 z,
		AkReal32 y,
		AkReal32 fDistance,
		AkReal32 & out_fAzimuth,
		AkReal32 & out_fElevation)
	{
		AKASSERT(fDistance > 0.f);

		//convert to polar coordinates
		if (z == 0.0f)
		{
			if (x == 0.0f)
			{
				out_fAzimuth = 0; // Speaker-coordinate extension: we want x=y=0 case to be like a source straight ahead.
				if (y == 0.0f)
					out_fElevation = 0;
				else if (y > 0.0f)
					out_fElevation = PIOVERTWO;
				else
					out_fElevation = -PIOVERTWO;
			}
			else
			{
				if (x > 0.0f)
					out_fAzimuth = PIOVERTWO;
				else
					out_fAzimuth = -PIOVERTWO;
				AkReal32 fSinElev = y / fDistance;
				fSinElev = AkClamp(fSinElev, -1.f, 1.f);	// Clamp: abs(fDistance) may be slightly larger due to floating point error.
				out_fElevation = AkMath::FastASin( fSinElev );
			}
		}
		else
		{
			out_fAzimuth = AkMath::FastAtan2f( x, z );
			AkReal32 fSinElev = y / fDistance;
			fSinElev = AkClamp(fSinElev, -1.f, 1.f);	// Clamp: abs(fDistance) may be slightly larger due to floating point error.
			out_fElevation = AkMath::FastASin( fSinElev );
		}
	}

	static const AkReal32 k_sn3d[AK_MAX_AMBISONICS_ORDER][AK_MAX_AMBISONICS_ORDER];

	static void AddHeadphonePower(AkReal32 x, AK::SpeakerVolumes::VectorPtr pChanIn);

#ifdef AK_71AUDIO
	#define AK_NUM_CHANNELS_FOR_SPREAD		(7)
#else
	#define AK_NUM_CHANNELS_FOR_SPREAD		(5)
#endif
	static void AddPowerVbap2d(void* in_pPanCache, AkUInt32 in_uNumArcs, const AkVector& point, AkReal32 in_fGain, AkUInt32 indexMap[AK_NUM_CHANNELS_FOR_SPREAD + 1], AK::SpeakerVolumes::VectorPtr pChanIn);

	// IMPORTANT: x,y,z in right-handed coordinate system with x pointing to the front, y to the left and z to the top.
	static inline void _ComputeSN3DphericalHarmonics(
		AkReal32 x,
		AkReal32 y,
		AkReal32 z,
		AkUInt32 in_order,
		AkUInt32 in_uStrideOut,
		AkReal32 * out_pVolumes
	)
	{
		AkReal32 * pHarmonic = out_pVolumes;
		*pHarmonic = 1.f;	// W = 1
		pHarmonic += in_uStrideOut;
		*pHarmonic = y;
		pHarmonic += in_uStrideOut;
		*pHarmonic = z;
		pHarmonic += in_uStrideOut;
		*pHarmonic = x;
		pHarmonic += in_uStrideOut;

		if (in_order > 1)
		{
			AkReal32 z2 = z*z;
			if (z2 < 0.99f)
			{
				AkReal32 cosphi = sqrtf(1.f - z2);
				AkReal32 oneovercosphi = 1.f / cosphi;
				AkReal32 costheta = x * oneovercosphi;
				AkReal32 sintheta = y * oneovercosphi;

				// Use hardcoded implementation for lower orders as it is more efficient.
				if (in_order <= 3)
				{
					// 2nd order
					const AkReal32 fSN3D_2 = 0.86602540378443864676372317075294f;	// sqrtf(3.f) / 2.f;

					AkReal32 U = fSN3D_2 * (costheta*costheta - sintheta*sintheta) * cosphi * cosphi;
					AkReal32 V = fSN3D_2 * (2 * costheta*sintheta * cosphi * cosphi);
					AkReal32 R = 0.5f * (3.f * z * z - 1.f);
					AkReal32 S = fSN3D_2 * (costheta * 2 * z * cosphi);
					AkReal32 T = fSN3D_2 * (sintheta * 2 * z * cosphi);

					*pHarmonic = V;
					pHarmonic += in_uStrideOut;
					*pHarmonic = T;
					pHarmonic += in_uStrideOut;
					*pHarmonic = R;
					pHarmonic += in_uStrideOut;
					*pHarmonic = S;
					pHarmonic += in_uStrideOut;
					*pHarmonic = U;
					pHarmonic += in_uStrideOut;

					if (in_order == 3)
					{
						AkReal32 fNormPQ = 0.79056941504209483299972338610818f;	// sqrtf(5.f / 8.f);
						AkReal32 P = fNormPQ * (costheta*costheta*costheta - 3 * sintheta*sintheta*costheta) * (cosphi * cosphi * cosphi);
						AkReal32 Q = fNormPQ * (3 * costheta*costheta*sintheta - sintheta*sintheta*sintheta) * (cosphi * cosphi * cosphi);
						AkReal32 K = 0.5f * z * (5.f * z * z - 3.f);
						const AkReal32 fNormLM = 0.61237243569579452454932101867647f;	// sqrtf(3.f / 8.f);
						AkReal32 f5sin2phiminus1 = (5 * z * z - 1.f) * cosphi;
						AkReal32 L = fNormLM * costheta * f5sin2phiminus1;
						AkReal32 M = fNormLM * sintheta * f5sin2phiminus1;
						const AkReal32 fNormNO = 1.9364916731037084425896326998912f;	// sqrtf(15.f) / 2.f;
						AkReal32 N = fNormNO * (costheta*costheta - sintheta*sintheta) * z * cosphi * cosphi;
						AkReal32 O = fNormNO * (2 * costheta*sintheta) * z * cosphi * cosphi;

						*pHarmonic = Q;
						pHarmonic += in_uStrideOut;
						*pHarmonic = O;
						pHarmonic += in_uStrideOut;
						*pHarmonic = M;
						pHarmonic += in_uStrideOut;
						*pHarmonic = K;
						pHarmonic += in_uStrideOut;
						*pHarmonic = L;
						pHarmonic += in_uStrideOut;
						*pHarmonic = N;
						pHarmonic += in_uStrideOut;
						*pHarmonic = P;
					}
				}
				else
				{
					// Generic algorithm for higher orders (>3) using recurrence equations.

					// Cache n degrees cos(n*theta) and sin(n*theta)
					// 
					// Init recurrent variables with results of degrees n - 1 = 0 and n = 1
					AkUInt32 sizeofOrderPlusOne = (in_order + 1) * sizeof(AkReal32);
					AkReal32 * cosn = (AkReal32*)AkAlloca(sizeofOrderPlusOne);
					AkReal32 * sinn = (AkReal32*)AkAlloca(sizeofOrderPlusOne);
					cosn[0] = 1.f;
					cosn[1] = costheta;
					sinn[0] = 0.f;
					sinn[1] = sintheta;
					for (AkUInt32 n = 2; n < in_order + 1; n++)
					{
						cosn[n] = 2 * costheta * cosn[n - 1] - cosn[n - 2];
						sinn[n] = sinn[n - 1] * costheta + cosn[n - 1] * sintheta;
					}

					// Order recurrence
					//
					// allocate scratch buffers for storing Pm-1,n and Pm,n (n = [0, order])
					// init with zeros because each new order overshoots 2 orders before by 2 degrees
					AkReal32 * Pm_minus1 = (AkReal32*)AkAlloca(sizeofOrderPlusOne);
					AkReal32 * Pm = (AkReal32*)AkAlloca(sizeofOrderPlusOne);
					memset(Pm_minus1, 0, sizeofOrderPlusOne);
					memset(Pm, 0, sizeofOrderPlusOne);

					// work buffer for inside loop, used up to mplus1+1
					AkReal32 * Pm_plus1 = (AkReal32*)AkAlloca((in_order + 2) * sizeof(AkReal32));

					// init recurrent variables with results of orders m - 1 = 0 and m = 1
					Pm_minus1[0] = 1;	//Pm_minus1_0 = 1
					Pm[0] = z;			//Pm_0 = uz;
					Pm[1] = cosphi;

					// for m (current order), compute order m+1
					for (AkUInt32 m = 1; m < in_order; m++)
					{
						AkUInt32 m_plus1 = m + 1;

						// Compute n = 0 component and write it in its rightful place
						Pm_plus1[0] = ((2 * m + 1) * z * Pm[0] - m * Pm_minus1[0]) / (m + 1);
						AkUInt32 acn = m_plus1 * m_plus1 + m_plus1;
						out_pVolumes[acn * in_uStrideOut] = Pm_plus1[0];

						// Compute n != 0 components
						for (AkUInt32 n_plus1 = 1; n_plus1 < m_plus1 + 1; n_plus1++)
						{
							AkReal32 Pm_minus1_n_plus1 = Pm_minus1[n_plus1];
							AkReal32 Pm_n = Pm[n_plus1 - 1];
							AkReal32 Pm_plus1_n_plus1 = Pm_minus1_n_plus1 + (2 * m + 1) * cosphi * Pm_n;
							Pm_plus1[n_plus1] = Pm_plus1_n_plus1;

							AkReal32 SN3D = k_sn3d[m_plus1 - 1][n_plus1 - 1];	// sqrt(2 * factorial(m_plus1 - n_plus1) / factorial(m_plus1 + n_plus1))

							out_pVolumes[(acn + n_plus1)* in_uStrideOut] = SN3D * Pm_plus1[n_plus1] * cosn[n_plus1];
							out_pVolumes[(acn - n_plus1)* in_uStrideOut] = SN3D * Pm_plus1[n_plus1] * sinn[n_plus1];
						}

						// update recurrent variables (just the non - zero part to avoid useless copies).
						memcpy(Pm_minus1, Pm, (m_plus1 + 1) * sizeof(AkReal32));
						memcpy(Pm, Pm_plus1, (m_plus1 + 1) * sizeof(AkReal32));
					}
				}
			}
			else
			{
				// Degenerate case z ~ +/-1: cosphi ~ 0, x & y don't care.
				AkReal32 Pm = z;
				for (AkUInt32 m = 2; m < in_order + 1; m++)
				{
					Pm *= z; // Pm_plus1 = ((2 * m + 1) * z * Pm - m * Pm_minus1) / (m + 1) #P2 = 1, P3 = z, P4 = 1, P5 = z, ... Pm = z**m

					// Write n = 0 component in its rightful place, 0 elsewhere
					AkUInt32 acn = m * m + m;
					const AkReal32 * pAcn = out_pVolumes + acn * in_uStrideOut;
					do
					{
						*pHarmonic = 0;
						pHarmonic += in_uStrideOut;
					} while (pHarmonic < pAcn);
					*pHarmonic = Pm;
					pHarmonic += in_uStrideOut;
				}

				// Trailing zeros.
				AkUInt32 numHarmonics = (in_order + 1) * (in_order + 1);
				const AkReal32 * pEnd = out_pVolumes + numHarmonics * in_uStrideOut;
				do
				{
					*pHarmonic = 0;
					pHarmonic += in_uStrideOut;
				} while (pHarmonic < pEnd);
			}
		}
	}

	// By angle. az clockwise.
	static void EncodeToAmbisonics(
		AkReal32 az,
		AkReal32 el,
		AK::SpeakerVolumes::VectorPtr in_pVolumes,
		AkUInt32 in_numHarmonics
	);

	// Compute spherical harmonic vector with SN3D normalization, ACN ordering, for given orientation (x,y,z).
	// Here, (x,y,z) are in a left-handed coordinate system with z pointing to the front, and are assumed normalized.
	// Note: Does not support points at (0,1,0).
	static AkForceInline void ComputeSN3DphericalHarmonics(
		AkReal32 x,
		AkReal32 y,
		AkReal32 z,
		AkUInt32 in_order,
		AkUInt32 in_uStrideOut,
		AkReal32 * out_pVolumes
		)
	{
		_ComputeSN3DphericalHarmonics(z, -x, y, in_order, in_uStrideOut, out_pVolumes);
	}

	// Select decoder
	static inline AkUInt32 OrderMinusOneToNumHarmonics(int orderidx)
	{
		return (orderidx + 2) * (orderidx + 2);
	}
	static const AkReal32 * GetSampledHarmonics3D(int in_order);
	static void ComputeDecoderMatrix(int in_orderidx, const AkReal32 * samples, AkUInt32 numSamples, AkReal32 * out_decoder);
	static const AkReal32 * GetReWeights(int in_orderidx);
	
	static void MultiDirectionAmbisonicPan(
		AkChannelConfig		in_inputConfig,
		AkChannelConfig		in_outputConfig,
		AK::SpeakerVolumes::MatrixPtr in_pVolumeMatrices,
		AkUInt32			in_matricesSize,
		AkReal32 *			in_mxWeights,
		AkUInt32			in_numMatrices,
		AK::SpeakerVolumes::MatrixPtr io_mxResult
	);

    static void DecodeAmbisonicsBasic(
        AkChannelConfig		in_inputConfig,
        AkChannelConfig		in_outputConfig,
        AK::SpeakerVolumes::MatrixPtr in_pVolumeMatrix,
        AkDevice *			in_pDevice
    );

	AKSOUNDENGINE_API static void GetDecodingMatrixShape(
		AkChannelConfig		in_inputConfig,			// ambisonic config
		AkUInt32 &			out_numPoints,			// Number of samples for given ambisonic order/config
		int &				out_orderidx			// Ambisonic order (index - 0 == 1st order)
	);

	static AKRESULT CopyDecoderInto(
		int					in_orderidx,			// Ambisonic order (index - 0 == 1st order)
		AK::SpeakerVolumes::MatrixPtr out_mx		// Preallocated matrix
	);

	AKSOUNDENGINE_API static void EncodeVector(
		int					in_orderidx,			// Ambisonic order (index - 0 == 1st order)
		AK::SpeakerVolumes::VectorPtr in_volumes,	// Number of samples must correspond to internal sphere sampling for given order (obtainable via GetDecodingMatrixShape)
		AK::SpeakerVolumes::VectorPtr out_volumes	// Encoded vector (length == number of harmonics for given order)
	);

//----------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------

	static void CreatePanCache2D(
		AkChannelConfig		in_outputConfig,	// config of bus to which this signal is routed.
		AkReal32			in_fSpeakerAngles[],// Speaker angles (rad).
		void *&				io_pPanCache		// Allocated or unallocated array of pan gain pairs, allocated herein if needed and returned filled with values.
		);
	static void CreatePanCache3D(
		AkChannelConfig		in_outputConfig,	// config of bus to which this signal is routed.
		AkReal32			in_fSpeakerAngles[],// Speaker angles (rad).
		AkReal32			in_fHeightAngle,	// Height speaker angle (rad).
		void *&				io_pPanPairs		// Allocated or unallocated array of pan gain pairs, allocated herein if needed and returned filled with values.
		);

	// Makes identity matrix to up to min(numchannels_in,numchannels_out), zeros the additional rows/columns.
	static void GetSpeakerVolumesDirectNoMask(
		AkChannelConfig		in_inputConfig,
		AkChannelConfig		in_outputConfig,
		AK::SpeakerVolumes::MatrixPtr out_pVolumes
		);

	template< class Contract >
	static void ContractPoints(
		const AkReal32 * AK_RESTRICT in_points,
		AkUInt32 in_numPoints,
		AkReal32 * AK_RESTRICT out_points,
		AkReal32 in_spread);
	template< class Contract >
	static void ContractPointsScalar(
		const AkReal32* AK_RESTRICT in_points,
		AkUInt32 in_numPoints,
		AkReal32* AK_RESTRICT out_points,
		AkReal32 in_spread);
};


class ContractInside;
class ContractOutside;
class ContractInsideScalar;
class ContractOutsideScalar;

#endif
