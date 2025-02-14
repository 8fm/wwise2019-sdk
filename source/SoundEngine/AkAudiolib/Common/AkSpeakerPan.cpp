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
// AkSpeakerPan.cpp
//
//////////////////////////////////////////////////////////////////////


#include "stdafx.h"

#include "AkSpeakerPan.h"
#include <AK/SoundEngine/Common/AkCommonDefs.h>
#include <AK/SoundEngine/Common/AkSimdMath.h>
#include "AkOutputMgr.h"
#include "AkDownmix.h"
#include "AkMath.h"
#include "AkVBAP.h"
#include "AkFXMemAlloc.h"
#include "Ak3DListener.h"

//#include <tchar.h> //test traces

//====================================================================================================
//====================================================================================================

void CAkSpeakerPan::CreatePanCache(
	AkChannelConfig		in_outputConfig,	// config of bus to which this signal is routed.
	AkReal32			in_fSpeakerAngles[],// Speaker angles (rad).
	AkReal32			in_fHeightAngle,	// Height speaker angle (rad).
	void *&				io_pPanCache		// Allocated or unallocated array of opaque data for various panners, allocated herein if needed and returned filled with values.
	)
{
	AKASSERT(in_outputConfig.eConfigType == AK_ChannelConfigType_Standard);
	if (!AK::HasHeightChannels(in_outputConfig.uChannelMask))
		CreatePanCache2D(in_outputConfig, in_fSpeakerAngles, io_pPanCache);
	else
		CreatePanCache3D(in_outputConfig, in_fSpeakerAngles, in_fHeightAngle, io_pPanCache);
}

void CAkSpeakerPan::DestroyPanCache(
	AkChannelConfig		in_outputConfig,	// config of bus to which this signal is routed.
	void *&				io_pPanCache		// Pan cache to free.
	)
{
	if (!AK::HasHeightChannels(in_outputConfig.uChannelMask))
	{
		AkFree(AkMemID_Object, io_pPanCache);
	}
	else
	{
		AkVBAP::Term(AkFXMemAlloc::GetUpper(), io_pPanCache);
	}
}

// Important: in_uNumArcs not necessarily equal to in_uNumChannels (like with center% < 100, or the stereo case)
static inline AkUInt32 GetNum2dArcs(AkUInt32 in_uNumChannelsNoLfe)
{
	return (in_uNumChannelsNoLfe < 4) ? in_uNumChannelsNoLfe + 2 : in_uNumChannelsNoLfe;
}

// Matrix2x2:
// |a b|
// |c d|
struct Matrix2x2
{
	AkReal32 a;
	AkReal32 b;
	AkReal32 c;
	AkReal32 d;
};

void CAkSpeakerPan::CreatePanCache2D( 
	AkChannelConfig		in_outputConfig,	// config of bus to which this signal is routed.
	AkReal32			in_fSpeakerAngles[],// Speaker angles (rad)
	void *&				io_pPanCache		// Allocated or unallocated array of opaque data for various panners, allocated herein if needed and returned filled with values.
	)
{
	// Important: in_uNumArcs not necessarily equal to in_uNumChannels (like with center% < 100, or the stereo case)
	AkUInt32 uNumArcs = GetNum2dArcs(in_outputConfig.uNumChannels);

	if (!io_pPanCache)
	{
		io_pPanCache = AkAlloc(AkMemID_Object, uNumArcs * sizeof(Matrix2x2));
		if (!io_pPanCache)
			return;
	}

	Matrix2x2 * out_arArcs = (Matrix2x2*)io_pPanCache;

	AKASSERT(uNumArcs > 2);
	AKASSERT(in_outputConfig.uNumChannels >= 2);

	// Create pan cache.

	AkReal32 cos0 = cosf(in_fSpeakerAngles[0]);
	AkReal32 cos1 = cosf(in_fSpeakerAngles[1]);
	AkReal32 surrAng = in_fSpeakerAngles[1] + (in_fSpeakerAngles[2] - in_fSpeakerAngles[1]) / 2;
	AkReal32 coss = cosf(surrAng);
	AkReal32 sins = sinf(surrAng);
	AkReal32 cos2 = cosf(in_fSpeakerAngles[2]);
	AkReal32 sin0 = sinf(in_fSpeakerAngles[0]);
	AkReal32 sin1 = sinf(in_fSpeakerAngles[1]);
	AkReal32 sin2 = sinf(in_fSpeakerAngles[2]);

	/// REVIEW Building the whole table while we could build only the desired row.

	// +1 for wrap around
	AkVector outChanDir[][AK_NUM_CHANNELS_FOR_SPREAD + 1] =
	{
		//{ { 0, 0, 1 } }, // mono unused.
		{ { -sin0, 0, cos0 },  { sin0, 0, cos0 }, { sin2, 0, cos2 }, { -sin2, 0, cos2 }, { -sin0, 0, cos0 } },	// stereo: FL, FR, (B)R, (B)L, FL
		{ { -sin0, 0, cos0 },        { 0, 0, 1 }, { sin0, 0, cos0 },  { sin2, 0, cos2 }, { -sin2, 0, cos2 }, { -sin0, 0, cos0 } }, // 3.x: FL, C, FR, (B)R, (B)L, FL
		{ { -sins, 0, coss }, { -sin0, 0, cos0 }, { sin0, 0, cos0 },  { sins, 0, coss }, { -sins, 0, coss } },	// 4.x: SL, FL, FR, SR, SL
		{ { -sins, 0, coss }, { -sin0, 0, cos0 },       { 0, 0, 1 },  { sin0, 0, cos0 },  { sins, 0, coss }, { -sins, 0, coss } },	// 5.x: SL, FL, C, FR, SR, SL
#ifdef AK_71AUDIO
		{ { -sin2, 0, cos2 }, { -sin1, 0, cos1 }, { -sin0, 0, cos0 }, { sin0, 0, cos0 },  { sin1, 0, cos1 },  { sin2, 0, cos2 }, { -sin2, 0, cos2 } },	// 6.x: BL, SL, FL, FR, SR, BR, BL
		{ { -sin2, 0, cos2 }, { -sin1, 0, cos1 }, { -sin0, 0, cos0 },       { 0, 0, 1 },  { sin0, 0, cos0 },  { sin1, 0, cos1 },  { sin2, 0, cos2 }, { -sin2, 0, cos2 } },	// 7.x: BL, SL, FL, C, FR, SR, BR, BL
#endif
	};

	// Compute (inverse) matrix for each arc.
	for (AkUInt32 uArc = 0; uArc < uNumArcs; uArc++)
	{
		// [[lix,liz], [ljx,ljz]]
		// inv = 1 / (lix*ljz - liz*ljx) [[ljz,-liz],[-ljx,lix]]
		const AkVector & li = outChanDir[in_outputConfig.uNumChannels - 2][uArc];
		const AkVector & lj = outChanDir[in_outputConfig.uNumChannels - 2][uArc + 1];
		AkReal32 det = 1 / (li.X*lj.Z - li.Z*lj.X);	// Note: determinant really just needed for sign
		out_arArcs[uArc].a = det * lj.Z;
		out_arArcs[uArc].b = det * -lj.X;
		out_arArcs[uArc].c = det * -li.Z;
		out_arArcs[uArc].d = det * li.X;
	}
}

void CAkSpeakerPan::CreatePanCache3D(
	AkChannelConfig		in_outputConfig,	// config of bus to which this signal is routed.
	AkReal32			in_fSpeakerAngles[],// Speaker angles (rad).
	AkReal32			in_fHeightAngle,	// Height speaker angle (rad).
	void *&				io_pPanCache		// Allocated or unallocated array of pan gain pairs, allocated herein if needed and returned filled with values.
	)
{
	// Note: 3D pan cache is meant to be used with angles whose direction is counterclockwise, contrary to angles of emitter-listener pairs in Wwise.

	// Hijack PanPair with blob used for 3D VBAP.
	AkUInt32 uNumChannels = in_outputConfig.uNumChannels;
	AkSphericalCoord * arSphericalPositions = (AkSphericalCoord*)AkAlloca(uNumChannels * sizeof(AkSphericalCoord));
	AkReal32 fFrontAngle = in_fSpeakerAngles[0];
	arSphericalPositions[0].theta = fFrontAngle; // FL
	arSphericalPositions[0].phi = 0.f;
	arSphericalPositions[0].r = 1.f;
	arSphericalPositions[1].theta = -fFrontAngle; // FR
	arSphericalPositions[1].phi = 0.f;
	arSphericalPositions[1].r = 1.f;
	AkUInt32 uNextIdx = 2;
	AkChannelMask in_uOutputConfig = in_outputConfig.uChannelMask;
	if (AK::HasCenter(in_uOutputConfig))
	{
		arSphericalPositions[2].theta = 0.f; // C
		arSphericalPositions[2].phi = 0.f;
		arSphericalPositions[2].r = 1.f;
		uNextIdx = 3;
	}
	if (AK::HasSurroundChannels(in_uOutputConfig))
	{
		AkReal32 fSideOrSurroundAngles;

		if (AK::HasSideAndRearChannels(in_uOutputConfig))
		{
			AkReal32 fBackAngle = in_fSpeakerAngles[2];
			arSphericalPositions[uNextIdx].theta = fBackAngle; // BL
			arSphericalPositions[uNextIdx].phi = 0.f;
			arSphericalPositions[uNextIdx].r = 1.f;
			++uNextIdx;
			arSphericalPositions[uNextIdx].theta = -fBackAngle; // BR
			arSphericalPositions[uNextIdx].phi = 0.f;
			arSphericalPositions[uNextIdx].r = 1.f;
			++uNextIdx;

			// Side angle.
			fSideOrSurroundAngles = in_fSpeakerAngles[1];
		}
		else
		{
			// Surround angle.
			// Use mean angle to minimize error with case where 5.1 is downmixed 7.1. 
			fSideOrSurroundAngles = in_fSpeakerAngles[1] + (in_fSpeakerAngles[2] - in_fSpeakerAngles[1]) / 2;
		}

		arSphericalPositions[uNextIdx].theta = fSideOrSurroundAngles; // SL
		arSphericalPositions[uNextIdx].phi = 0.f;
		arSphericalPositions[uNextIdx].r = 1.f;
		++uNextIdx;
		arSphericalPositions[uNextIdx].theta = -fSideOrSurroundAngles; // SR
		arSphericalPositions[uNextIdx].phi = 0.f;
		arSphericalPositions[uNextIdx].r = 1.f;
		++uNextIdx;
	}
	
	if (in_uOutputConfig & AK_SPEAKER_TOP)
	{
		arSphericalPositions[uNextIdx].theta = 0.f;
		arSphericalPositions[uNextIdx].phi = PIOVERTWO;
		arSphericalPositions[uNextIdx].r = 1.f;
		++uNextIdx;
	}
	if (in_uOutputConfig & AK_SPEAKER_HEIGHT_FRONT_LEFT)
	{
		arSphericalPositions[uNextIdx].theta = fFrontAngle;
		arSphericalPositions[uNextIdx].phi = in_fHeightAngle;
		arSphericalPositions[uNextIdx].r = 1.f;
		++uNextIdx;
	}
	if (in_uOutputConfig & AK_SPEAKER_HEIGHT_FRONT_CENTER)
	{
		arSphericalPositions[uNextIdx].theta = 0.f;
		arSphericalPositions[uNextIdx].phi = in_fHeightAngle;
		arSphericalPositions[uNextIdx].r = 1.f;
		++uNextIdx;
	}
	if (in_uOutputConfig & AK_SPEAKER_HEIGHT_FRONT_RIGHT)
	{
		arSphericalPositions[uNextIdx].theta = -fFrontAngle;
		arSphericalPositions[uNextIdx].phi = in_fHeightAngle;
		arSphericalPositions[uNextIdx].r = 1.f;
		++uNextIdx;
	}
	AkReal32 fBackAngle = in_fSpeakerAngles[2];
	if (in_uOutputConfig & AK_SPEAKER_HEIGHT_BACK_LEFT)
	{
		arSphericalPositions[uNextIdx].theta = fBackAngle;
		arSphericalPositions[uNextIdx].phi = in_fHeightAngle;
		arSphericalPositions[uNextIdx].r = 1.f;
		++uNextIdx;
	}
	if (in_uOutputConfig & AK_SPEAKER_HEIGHT_BACK_CENTER)
	{
		arSphericalPositions[uNextIdx].theta = 0.f;
		arSphericalPositions[uNextIdx].phi = in_fHeightAngle;
		arSphericalPositions[uNextIdx].r = 1.f;
	}
	if (in_uOutputConfig & AK_SPEAKER_HEIGHT_BACK_RIGHT)
	{
		arSphericalPositions[uNextIdx].theta = -fBackAngle;
		arSphericalPositions[uNextIdx].phi = in_fHeightAngle;
		arSphericalPositions[uNextIdx].r = 1.f;
	}

	void * pOldCache = io_pPanCache;
	io_pPanCache = NULL;
	AkVBAP::PushVertices(AkFXMemAlloc::GetUpper(), arSphericalPositions, uNumChannels, io_pPanCache);
	if (pOldCache && io_pPanCache)
	{
		// Free old cache (unless new cache failed to initialize).
		AkVBAP::Term(AkFXMemAlloc::GetUpper(), pOldCache);
	}
}

AKRESULT CAkSpeakerPan::CreateSpreadCache(
	ConfigIn2d3d		key,				// Plane or 3D config
	AkReal32			in_fOneOverMinAngleBetweenSpeakers,		// 1 / min angle between speakers ("PAN_CIRCLE" degrees).
	ArraySimdVector &	out_arSimdVectors	// Returned array filled with spread points
	)
{
	AkReal32 fOneOverNumChannels = 1.f / key.uNumChansIn;
	AkReal32 fHalfChannelSpreadAngle = PI * fOneOverNumChannels;

	if (!key.b3D)
	{
		// Plane config

		// Compute arcs with desired sampling.

		//Calculate the number of virtual points needed for each input channel (so the channels have a width to fill all speakers)
		AkReal32 fNbVirtualPointsH = (fHalfChannelSpreadAngle * in_fOneOverMinAngleBetweenSpeakers); //min 2 points
		AkUInt32 uNbVirtualPointsH = (AkUInt32)(fNbVirtualPointsH + 1.01f);	// + 1 + "epsilon" big enough to yield the same results on all pf/fpus.
		if (uNbVirtualPointsH & 0x3)
			uNbVirtualPointsH += (4 - (uNbVirtualPointsH & 0x3));	//mul 4 points

		if (out_arSimdVectors.Resize(uNbVirtualPointsH) != AK_Success)
			return AK_Fail;

		AkReal32 fHalfVirtPtSpreadAngle = fHalfChannelSpreadAngle / (AkReal32)uNbVirtualPointsH;

		// Compute prototype arc.
		AkReal32 * pCurVect = (AkReal32 *)(&out_arSimdVectors[0]);

		AkReal32 fCurrVirtPtAngle = ((uNbVirtualPointsH-1) * fHalfVirtPtSpreadAngle);

		AkUInt32 uVirtualPoint = 0;
		do
		{
			// Convert in cartesian coordinates.
			pCurVect[0] = (AkReal32)sin(-fCurrVirtPtAngle);
			pCurVect[1] = 0.f;
			pCurVect[2] = (AkReal32)cos(-fCurrVirtPtAngle);
			pCurVect[3] = 0.f;
			pCurVect += 4;

			fCurrVirtPtAngle -= 2.f * fHalfVirtPtSpreadAngle;
		} while (++uVirtualPoint < uNbVirtualPointsH);
	}
	else
	{
		// Compute prototype spherical cap with desired sampling
		/// REVIEW This is almost OUT channel config agnostic, but not quite.
		/// Not IN channel config agnostic though.

		AkReal32 fNbVirtualPointsOnMeridian = (fHalfChannelSpreadAngle * in_fOneOverMinAngleBetweenSpeakers);
		AkUInt32 uNbVirtualPointsOnMeridian = (AkUInt32)(fNbVirtualPointsOnMeridian + 1.01f);	// + 1 + "epsilon" big enough to yield the same results on all pf/fpus.
		// Make even
		uNbVirtualPointsOnMeridian += (uNbVirtualPointsOnMeridian & 0x1);

		const AkUInt32 uNbVirtualPointsOnParallel = 8;

		AkUInt32 uNumPtsCap = uNbVirtualPointsOnMeridian * uNbVirtualPointsOnParallel;
		
		if (out_arSimdVectors.Resize(uNumPtsCap) != AK_Success)
			return AK_Fail;

		// Compute prototype cap.
		AkReal32 * pCurVect = (AkReal32 *)(&out_arSimdVectors[0]);
		

		AkReal32 lowpar = cosf(fOneOverNumChannels * PI);
		AkReal32 merstep = (1.f - lowpar) / (AkReal32)(uNbVirtualPointsOnMeridian);

		lowpar += merstep / 2.f;

		AkReal32 fOneOverNbVirtualPointsOnParallel = 1.f / (AkReal32)uNbVirtualPointsOnParallel;
		AkReal32 angstep = 2 * PI * fOneOverNbVirtualPointsOnParallel;

		AkReal32 z = lowpar;
		for (AkUInt32 uPar = 0; uPar < uNbVirtualPointsOnMeridian; uPar++)
		{
			AkReal32 r = sqrtf(1 - z*z);
			AkReal32 p = 0.f;
			for (AkUInt32 uMer = 0; uMer < uNbVirtualPointsOnParallel; uMer++)
			{
				pCurVect[0] = r * cosf(p);
				pCurVect[1] = r * sinf(p);
				pCurVect[2]= z;
				pCurVect[3] = 0.f;
				pCurVect += 4;
				p += angstep;
			}
			z += merstep;
		}
	}

	return AK_Success;
}

// index <-> order-1, degree-1
// sqrt(2 * (m-n)! / (m+n)!)
const AkReal32 CAkSpeakerPan::k_sn3d[AK_MAX_AMBISONICS_ORDER][AK_MAX_AMBISONICS_ORDER] =
{
	{ 1.0 },	// order 1
	{ 5.77350269e-01f, 2.88675135e-01f }, // order 2 ...
	{ 4.08248290e-01f, 1.29099445e-01f, 5.27046277e-02 },
	{ 3.16227766e-01f, 7.45355992e-02f, 1.99204768e-02f, 7.04295212e-03f },
	{ 2.58198890e-01f, 4.87950036e-02f, 9.96023841e-03f, 2.34765071e-03f, 7.42392339e-04f }
};

// Ambisonics convention conversions
// Per order m: sqrt(2*m+1)
static const AkReal32 k_fSn3dToN3d[] =
{
	1.f,	// order 0
	1.7320508075688772935274463415059f,	// order 1
	2.2360679774997896964091736687313f,	// ...
	2.6457513110645905905016157536393f,
	3.f,
	3.3166247903553998491149327366707f
};

// maxRE weights
static const AkReal32 s_weights_1[] = {
	0.707106781186547f,
	0.408000612744638f,
	0.408000612744638f,
	0.408000612744638f
};

static const AkReal32 s_weights_2[] = {
	0.527046276694730f,
	0.408460864438416f,
	0.408460864438416f,
	0.408460864438416f,
	0.210818510677892f,
	0.210818510677892f,
	0.210818510677892f,
	0.210818510677892f,
	0.210818510677892f
};

static const AkReal32 s_weights_3[] = {
	0.417028828114150f,
	0.359061821006283f,
	0.359061821006283f,
	0.359061821006283f,
	0.255221642805860f,
	0.255221642805860f,
	0.255221642805860f,
	0.255221642805860f,
	0.255221642805860f,
	0.127193792574816f,
	0.127193792574816f,
	0.127193792574816f,
	0.127193792574816f,
	0.127193792574816f,
	0.127193792574816f,
	0.127193792574816f
};

static const AkReal32 s_weights_4[] = {
	0.34418519f,
	0.31189368f,
	0.31189368f,
	0.31189368f,
	0.25185506f,
	0.25185506f,
	0.25185506f,
	0.25185506f,
	0.25185506f,
	0.17244751f,
	0.17244751f,
	0.17244751f,
	0.17244751f,
	0.17244751f,
	0.17244751f,
	0.17244751f,
	0.0845785f,
	0.0845785f,
	0.0845785f,
	0.0845785f,
	0.0845785f,
	0.0845785f,
	0.0845785f,
	0.0845785f,
	0.0845785f
};

static const AkReal32 s_weights_5[] = {
	0.29268113f,
	0.27291623f,
	0.27291623f,
	0.27291623f,
	0.23538854f,
	0.23538854f,
	0.23538854f,
	0.23538854f,
	0.23538854f,
	0.1838769f,
	0.1838769f,
	0.1838769f,
	0.1838769f,
	0.1838769f,
	0.1838769f,
	0.1838769f,
	0.1235129f,
	0.1235129f,
	0.1235129f,
	0.1235129f,
	0.1235129f,
	0.1235129f,
	0.1235129f,
	0.1235129f,
	0.1235129f,
	0.06020811f,
	0.06020811f,
	0.06020811f,
	0.06020811f,
	0.06020811f,
	0.06020811f,
	0.06020811f,
	0.06020811f,
	0.06020811f,
	0.06020811f,
	0.06020811f
};

static const AkReal32 s_weights2D_1[] = {
	0.707106781186547f,
	0.499924494298889f,
	0.499924494298889f,
	0.499924494298889f
};

static const AkReal32 s_weights2D_2[] = {
	0.577350269189626f,
	0.499985333118216f,
	0.499985333118216f,
	0.499985333118216f,
	0.288675134594813f,
	0.288675134594813f,
	0.288675134594813f,
	0.288675134594813f,
	0.288675134594813f
};

static const AkReal32 s_weights2D_3[] = {
	0.500000000000000f,
	0.462000000000000f,
	0.462000000000000f,
	0.462000000000000f,
	0.353500000000000f,
	0.353500000000000f,
	0.353500000000000f,
	0.353500000000000f,
	0.353500000000000f,
	0.191500000000000f,
	0.191500000000000f,
	0.191500000000000f,
	0.191500000000000f,
	0.191500000000000f,
	0.191500000000000f,
	0.191500000000000f
};

static const AkReal32 s_weights2D_4[] = {
	0.4472136f,
	0.4253254f,
	0.4253254f,
	0.4253254f,
	0.3618034f,
	0.3618034f,
	0.3618034f,
	0.3618034f,
	0.3618034f,
	0.26286556f,
	0.26286556f,
	0.26286556f,
	0.26286556f,
	0.26286556f,
	0.26286556f,
	0.26286556f,
	0.1381966f,
	0.1381966f,
	0.1381966f,
	0.1381966f,
	0.1381966f,
	0.1381966f,
	0.1381966f,
	0.1381966f,
	0.1381966f
};

static const AkReal32 s_weights2D_5[] = {
	0.40824829f,
	0.39433757f,
	0.39433757f,
	0.39433757f,
	0.35355339f,
	0.35355339f,
	0.35355339f,
	0.35355339f,
	0.35355339f,
	0.28867513f,
	0.28867513f,
	0.28867513f,
	0.28867513f,
	0.28867513f,
	0.28867513f,
	0.28867513f,
	0.20412415f,
	0.20412415f,
	0.20412415f,
	0.20412415f,
	0.20412415f,
	0.20412415f,
	0.20412415f,
	0.20412415f,
	0.20412415f,
	0.10566243f,
	0.10566243f,
	0.10566243f,
	0.10566243f,
	0.10566243f,
	0.10566243f,
	0.10566243f,
	0.10566243f,
	0.10566243f,
	0.10566243f,
	0.10566243f
};

static const AkReal32 * s_weightsRE_3D[] = {
	s_weights_1,
	s_weights_2,
	s_weights_3,
	s_weights_4,
	s_weights_5
};

const AkReal32* CAkSpeakerPan::GetReWeights(int in_orderidx)
{
	return s_weightsRE_3D[in_orderidx];
}

static const AkReal32 * s_weightsRE_2D[] = {
	s_weights2D_1,
	s_weights2D_2,
	s_weights2D_3,
	s_weights2D_4,
	s_weights2D_5
};

static const AkReal32 k_surfSphere = 4.f * PI;
static const AkReal32 k_periCircle = 2.f * PI;

// Static sampling of sphere, expressed in left handed coordinate system.
AK_ALIGN_SIMD(static const AkReal32) s_sampling_32[32][4] =
{
	{ -0.35836795, 0.93358043, 0.00000000 },
	{ -0.35836795, -0.93358043, 0.00000000 },
	{ 0.35836795, 0.93358043, 0.00000000 },
	{ 0.35836795, -0.93358043, 0.00000000 },
	{ -0.00000000, 0.35836795, 0.93358043 },
	{ -0.00000000, -0.35836795, 0.93358043 },
	{ -0.00000000, 0.35836795, -0.93358043 },
	{ -0.00000000, -0.35836795, -0.93358043 },
	{ -0.93358043, 0.00000000, 0.35836795 },
	{ 0.93358043, 0.00000000, 0.35836795 },
	{ -0.93358043, 0.00000000, -0.35836795 },
	{ 0.93358043, 0.00000000, -0.35836795 },
	{ -0.00000000, 0.84804810, 0.52991926 },
	{ -0.00000000, -0.84804810, 0.52991926 },
	{ -0.00000000, 0.84804810, -0.52991926 },
	{ -0.00000000, -0.84804810, -0.52991926 },
	{ -0.52991926, 0.00000000, 0.84804810 },
	{ 0.52991926, 0.00000000, 0.84804810 },
	{ -0.52991926, 0.00000000, -0.84804810 },
	{ 0.52991926, 0.00000000, -0.84804810 },
	{ -0.84804810, 0.52991926, 0.00000000 },
	{ -0.84804810, -0.52991926, 0.00000000 },
	{ 0.84804810, 0.52991926, 0.00000000 },
	{ 0.84804810, -0.52991926, 0.00000000 },
	{ -0.57922797, 0.57357644, 0.57922797 },
	{ -0.57922797, -0.57357644, 0.57922797 },
	{ 0.57922797, 0.57357644, 0.57922797 },
	{ 0.57922797, -0.57357644, 0.57922797 },
	{ -0.57922797, 0.57357644, -0.57922797 },
	{ -0.57922797, -0.57357644, -0.57922797 },
	{ 0.57922797, 0.57357644, -0.57922797 },
	{ 0.57922797, -0.57357644, -0.57922797 }
};

AK_ALIGN_SIMD(static const AkReal32) s_sampling_64[64][4] =
{
	{ 0.2795085023843567f, -0.5282561687800567f, -0.8017608229644738f },
	{ -0.8502344734887628f, -0.46053147353741714f, 0.25497470849623827f },
	{ -0.12035042957115419f, -0.9800033933808433f, -0.15845858469666624f },
	{ -0.7384544159076097f, 0.5417347855222537f, -0.40150778047468605f },
	{ -0.9023086943336909f, 0.4303313534732594f, 0.025572374698152586f },
	{ 0.047978186500540476f, 0.2587638055948044f, -0.9647483539940421f },
	{ 0.8576571134250162f, 0.019706492014795608f, -0.513844266255779f },
	{ 0.5069750463207845f, -0.3047043639965583f, 0.8063073563905337f },
	{ 0.9399815104155f, 0.2546550605648621f, 0.22712454778315272f },
	{ -0.13328836298376312f, -0.7502513721686398f, 0.6475778647021389f },
	{ -0.35447040056892704f, 0.8935650491541904f, 0.27548545887319265f },
	{ 0.22415075497005427f, 0.6471534751285397f, 0.728662348880062f },
	{ 0.13191205409230705f, -0.8763074670884176f, -0.46334051529110887f },
	{ -0.9785125042547358f, 0.05487020545814117f, 0.198752458022805f },
	{ -0.17088868898367007f, -0.6507279277852598f, -0.7398312104647548f },
	{ -0.33010896884839513f, 0.8225311608892928f, -0.4631096609356813f },
	{ -0.9657953358062479f, -0.2366975009294911f, -0.1058945814885284f },
	{ -0.32781901560960175f, 0.5511792043375731f, -0.7672914555177528f },
	{ 0.9226821436993045f, 0.29510754321698907f, -0.2481314160578804f },
	{ 0.08991843769739725f, -0.38106480489312855f, 0.9201653596141408f },
	{ 0.986639037400764f, -0.15763723075559277f, -0.04115717867640994f },
	{ 0.29234572013074805f, -0.7284935280554434f, 0.6195410878243405f },
	{ -0.15620844409699247f, 0.747057770018828f, 0.6461451928531952f },
	{ 0.4734530569700871f, 0.7959409169543181f, 0.3772535746200408f },
	{ 0.4459695309718168f, 0.8903078636551701f, -0.09199502898822752f },
	{ 0.8417169730129586f, -0.4565916635899127f, -0.28816070183510195f },
	{ -0.1058183729581746f, 0.3262821648839505f, 0.9393308367253399f },
	{ -0.3231275179209919f, -0.3314430149428089f, 0.8864164568688015f },
	{ 0.76920720114347f, 0.6382846217335253f, -0.030216276533019115f },
	{ 0.6632890802733076f, -0.5856146198176896f, 0.46594325088574096f },
	{ -0.752537868044841f, -0.5776177718800288f, -0.316298066334394f },
	{ 0.12857102079111057f, 0.6120359932027286f, -0.780308551559626f },
	{ -0.7988984346591672f, 0.1699038812549158f, 0.5769696371852231f },
	{ -0.6179211266088764f, 0.24046043505109518f, -0.748566804276983f },
	{ -0.48632828486704854f, -0.3889905967062063f, -0.7824136470003731f },
	{ 0.41794444304567124f, -0.907671893204277f, -0.038133670347964235f },
	{ -0.43330045271610246f, -0.8006545848890619f, -0.4137667862119248f },
	{ -0.9192056731273198f, 0.14419535157049343f, -0.36642684273400694f },
	{ 0.5337015982896157f, -0.7185628534169453f, -0.44590361029308495f },
	{ -0.3337323174208779f, 0.013182674630154465f, -0.9425757038023449f },
	{ -0.7506388026425374f, -0.2382967860016695f, 0.616243482520223f },
	{ 0.18479920653226084f, 0.8649218268771788f, -0.46664706862519795f },
	{ 0.910896387469669f, -0.2038278648823192f, 0.3587784452725967f },
	{ 0.19327280968895375f, -0.9459199609876408f, 0.2605399171721632f },
	{ 0.5756507029965032f, 0.3537643174906068f, -0.7372089770275703f },
	{ 0.16571540884247268f, 0.033834467069261416f, 0.9855930357456424f },
	{ 0.09904267009038839f, 0.9601974928688736f, 0.26117297752580737f },
	{ -0.5044532103355767f, 0.4887736022753057f, 0.7117775806394521f },
	{ -0.2777514110750853f, -0.932324416412886f, 0.2315714494625158f },
	{ -0.7099009228898816f, 0.6093390491984683f, 0.35319485104117954f },
	{ 0.0176814394204141f, -0.2462186571575567f, -0.9690530117426764f },
	{ 0.4153315225900387f, 0.011297498399834943f, -0.9095999631007831f },
	{ -0.6248530727062379f, -0.7775912026002082f, 0.07007538204201731f },
	{ -0.5721708292247515f, 0.8156420488020453f, -0.08572391970892494f },
	{ 0.7333052360400311f, 0.4835528545378343f, 0.4779540434650452f },
	{ -0.5236117901817078f, -0.6352589263855711f, 0.5676942747907174f },
	{ 0.6486111756428468f, -0.2958010027474403f, -0.7012883213092977f },
	{ 0.7316587157128583f, 0.011851188360553457f, 0.6815680986195388f },
	{ 0.46401322074956836f, 0.3147364622783838f, 0.8280294018222419f },
	{ -0.07113597772647035f, 0.9882436521598338f, -0.13532980691145852f },
	{ -0.7823148961386536f, -0.20286469503538943f, -0.588922167003218f },
	{ 0.7471864123197824f, -0.6576661440513069f, 0.09585253368261983f },
	{ -0.42754704821762124f, 0.04567796323568558f, 0.9028383272962212f },
	{ 0.6048906133814166f, 0.6283342920812205f, -0.4891864299404115f }
};


// 2D: use azimuth only.
const AkUInt32 k_uNumSamples2D = 12;
AK_ALIGN_SIMD(static const AkReal32) s_sampling2D_12[k_uNumSamples2D][4] =
{
	// 12 point circle
	{ 0.00000000e+00f, 0.f, 1.00000000e+00f },
	{ -5.03538384e-01f, 0.f, 8.63972856e-01f },
	{ -8.63972856e-01f, 0.f, 5.03538384e-01f },
	{ -1.00000000e+00f, 0.f, 6.12323400e-17f },
	{ -8.63972856e-01f, 0.f, -5.03538384e-01f },
	{ -5.03538384e-01f, 0.f, -8.63972856e-01f },
	{ 0.00000000e+00f, 0.f, -1.00000000e+00f },
	{ 5.03538384e-01f, 0.f, -8.63972856e-01f },
	{ 8.63972856e-01f, 0.f, -5.03538384e-01f },
	{ 1.00000000e+00f, 0.f, -1.83697020e-16f },
	{ 8.63972856e-01f, 0.f, 5.03538384e-01f },
	{ 5.03538384e-01f, 0.f, 8.63972856e-01f }
};

// IMPORTANT: Update in sync with s_numSamples3Dsampling
static const AkReal32 * s_sampling3D[AK_MAX_AMBISONICS_ORDER] =
{
	&(s_sampling_32[0][0]),	// 1st order
	&(s_sampling_32[0][0]),
	&(s_sampling_32[0][0]),
	&(s_sampling_64[0][0]),
	&(s_sampling_64[0][0])
};

// IMPORTANT: Update in sync with s_sampling3D
static AkUInt32 s_numSamples3Dsampling[AK_MAX_AMBISONICS_ORDER] =
{
	32, // 1st order
	32,
	32,
	64,
	64
};

static const AkReal32 * s_sampling2D[AK_MAX_AMBISONICS_ORDER] =
{
	&(s_sampling2D_12[0][0]),	// 1st order
	&(s_sampling2D_12[0][0]),
	&(s_sampling2D_12[0][0]),
	&(s_sampling2D_12[0][0]),
	&(s_sampling2D_12[0][0])
};

static AkUInt32 s_numSamples2Dsampling[AK_MAX_AMBISONICS_ORDER] =
{
	k_uNumSamples2D, // 1st order
	k_uNumSamples2D,
	k_uNumSamples2D,
	k_uNumSamples2D,
	k_uNumSamples2D
};

/// REVIEW Consider replacing this with function. Supported incomplete configs hurt...
static const AkInt8 s_numChanToOrderMinusOne[] =
{
	-1,	// 0 channel
	0,	// 1 channel -> order 1
	0,	// 2 channels -> order 1
	0,
	0,
	1,	// 5 channels -> order 2
	1,
	2,	// 7 channels -> order 3
	2,	// 8 channels -> order 3
	1,	// 9 channels -> order 2
	-1,	// 10 channels -> not supported
	2,	// 11 channels -> order 3
	-1,	// 12 channels -> not supported
	-1,	// 13 channels -> not supported
	-1,	// 14 channels -> not supported
	-1,	// 15 channels -> not supported
	2,	// 16 channels -> order 3
	-1,	// 17 channels -> not supported
	-1,	// 18 channels -> not supported
	-1,	// 19 channels -> not supported
	-1,	// 20 channels -> not supported
	-1,	// 21 channels -> not supported
	-1,	// 22 channels -> not supported
	-1,	// 23 channels -> not supported
	-1,	// 24 channels -> not supported
	3,	// 25 channels -> order 4
	-1,	// 26 channels -> not supported
	-1,	// 27 channels -> not supported
	-1,	// 28 channels -> not supported
	-1,	// 29 channels -> not supported
	-1,	// 30 channels -> not supported
	-1,	// 31 channels -> not supported
	-1,	// 32 channels -> not supported
	-1,	// 33 channels -> not supported
	-1,	// 34 channels -> not supported
	-1,	// 35 channels -> not supported
	4,	// 36 channels -> order 5
};

static AkReal32 * s_decoder3D[AK_MAX_AMBISONICS_ORDER+1] =
{
	NULL, // Invalid: remains NULL to catch unsupported configurations
	NULL, // Order 1
	NULL, // Order 2
	NULL, // ...
	NULL,
	NULL
};

static AkReal32 * s_decoder2D[AK_MAX_AMBISONICS_ORDER+1] =
{
	NULL, // Invalid: remains NULL to catch unsupported configurations
	NULL, // Order 1
	NULL, // Order 2
	NULL, // ...
	NULL,
	NULL
};

static AkReal32 * s_encoder_t[AK_MAX_AMBISONICS_ORDER+1] =
{
	NULL, // Invalid: remains NULL to catch unsupported configurations
	NULL, // Order 1
	NULL, // Order 2
	NULL, // ...
	NULL,
	NULL
};

#define DECODER_IDX(row,col,NCOL) ((col)+(NCOL*(row)))

void CAkSpeakerPan::Init()
{
	for (AkUInt32 order = 0; order < AK_MAX_AMBISONICS_ORDER+1; order++)
	{
		s_decoder3D[order] = NULL;
		s_decoder2D[order] = NULL;
		s_encoder_t[order] = NULL;
	}
}

void CAkSpeakerPan::Term()
{
	for (AkUInt32 order = 0; order < AK_MAX_AMBISONICS_ORDER+1; order++)
	{
		if (s_decoder3D[order])
		{
			AkFree(AkMemID_Object, s_decoder3D[order]);
		}
		if (s_decoder2D[order])
		{
			AkFree(AkMemID_Object, s_decoder2D[order]);
		}
		if (s_encoder_t[order])
		{
			AkFree(AkMemID_Object, s_encoder_t[order]);
		}
	}
}

void CAkSpeakerPan::ComputeDecoderMatrix(int in_orderidx, const AkReal32 * samples, AkUInt32 numSamples, AkReal32 * out_decoder)
{
	// Compute decoder matrix.
	for (AkUInt32 point = 0; point < numSamples; point++)
	{
		AkVector * sample = (AkVector*)((AKSIMD_V4F32*)samples + point);
		ComputeSN3DphericalHarmonics(sample->X, sample->Y, sample->Z, in_orderidx + 1, numSamples, out_decoder + point);
	}

	// Apply SN3D to N3D conversion twice.
	// The unweighted unnormalized decoding matrix D = C' in the SN3D convention, with C' the transpose of the encoding matrix, is obtained like so: 
	// D(SN3D) = D(N3D) * a(N3D)SN3D = C(N3D)' * a(N3D)SN3D = (a(N3D)SN3D * C(SN3D))' * a(N3D)SN3D = C(SN3D)' * a(N3D)SN3D * a(N3D)SN3D
	// a being the diagonal matrix of Sn3dToN3d conversion gains below.
	AkUInt32 uOrder = 1;
	AkUInt32 numHarmonics = OrderMinusOneToNumHarmonics(in_orderidx);
	for (AkUInt32 harm = 0; harm < numHarmonics; harm++)
	{
		for (AkUInt32 point = 0; point < numSamples; point++)
		{
			out_decoder[DECODER_IDX(harm, point, numSamples)] *= k_fSn3dToN3d[uOrder - 1] * k_fSn3dToN3d[uOrder - 1];
		}
		uOrder += ((harm + 1) / (uOrder * uOrder));
	}
}

// Returns pre-baked set of orthogonal (SN3D) spherical harmonics computed on sampled sphere.
const AkReal32 * CAkSpeakerPan::GetSampledHarmonics3D(int in_orderidx)
{
	AkReal32 * decoder = s_decoder3D[in_orderidx+1];
	if (!decoder && in_orderidx >= 0) // s_decoder[0] is invalid, always NULL
	{
		// Construct decoder matrix the first time.
		const AkReal32 * samples = s_sampling3D[in_orderidx];
		AkUInt32 numSamples = s_numSamples3Dsampling[in_orderidx];
		AkUInt32 numHarmonics = OrderMinusOneToNumHarmonics(in_orderidx);
		decoder = (AkReal32 *)AkAlloc(AkMemID_Object, numHarmonics * numSamples * sizeof(AkReal32));
		if (decoder)
		{
			ComputeDecoderMatrix(in_orderidx, samples, numSamples, decoder);
			s_decoder3D[in_orderidx+1] = decoder;
		}
	}

	return decoder;
}

static const AkReal32 * GetSampledHarmonics2D(int in_orderidx)
{
	AkReal32 * decoder = s_decoder2D[in_orderidx+1];
	if (!decoder && in_orderidx >= 0)	// s_decoder[0] is invalid, always NULL
	{
		const AkReal32 * samples = s_sampling2D[in_orderidx];
		AkUInt32 numSamples = s_numSamples2Dsampling[in_orderidx];
		AkUInt32 numHarmonics = CAkSpeakerPan::OrderMinusOneToNumHarmonics(in_orderidx);
		decoder = (AkReal32 *)AkAlloc(AkMemID_Object, numHarmonics * numSamples * sizeof(AkReal32));
		if (decoder)
		{
			// Compute decoder matrix.
			for (AkUInt32 point = 0; point < numSamples; point++)
			{
				AkVector * sample = (AkVector*)((AKSIMD_V4F32*)samples + point);
				CAkSpeakerPan::ComputeSN3DphericalHarmonics(sample->X, sample->Y, sample->Z, in_orderidx + 1, numSamples, decoder + point);
			}

			// Normalize and apply RE weights.

			// Also, convert to N2D
			const AkReal32 k_fN3dToN2d[] =
			{
				1.0f,
				0.816496580928f,
				0.73029674334f,
				0.676123403783f,
				0.637455258312f,
				0.607789741118f
			};

			// Also, "convert to SN3D" (since it is meant to _decode from_ SN3D, we actually perform the opposite conversion)
			for (int order = 0; order <= in_orderidx + 1; order++)
			{
				AkUInt32 harm = order * order;
				AKASSERT(harm < numHarmonics);
				AkReal32 scaling = k_fSn3dToN3d[order] * k_fSn3dToN3d[order] * k_fN3dToN2d[order] * k_fN3dToN2d[order];
				for (AkUInt32 point = 0; point < numSamples; point++)
				{
					decoder[DECODER_IDX(harm, point, numSamples)] *= scaling;
				}
				int degree = -order + 1;
				for (; degree < order; degree++)
				{
					harm = order * (order + 1) + degree;
					AKASSERT(harm < numHarmonics);
					// clear non horizontal components
					for (AkUInt32 point = 0; point < numSamples; point++)
					{
						decoder[DECODER_IDX(harm, point, numSamples)] = 0.f;
					}
				}
				if (degree == order)	// skip order 0
				{
					harm = order * (order + 1) + degree;
					AKASSERT(harm < numHarmonics);
					for (AkUInt32 point = 0; point < numSamples; point++)
					{
						decoder[DECODER_IDX(harm, point, numSamples)] *= scaling;
					}
				}
			}

			s_decoder2D[in_orderidx+1] = decoder;
		}
	}
	return decoder;
}

// Skip harmonics for incomplete channel configs.
static const AkUInt32 MixedAmbisonicsSkipIndices[] =
{
	0xE,	// 1: Skip YZX
	0xC,	// 2: Skip ZX
	0x4,	// 3: skip Z
	0,	// 4: full
	0xE4,	// 5: skip ZTRS
	0xE0,	// 6: skip TRS
	0x7CE4,	// 7: skip ZTRSOMKLN
	0x7CE0,	// 8: skip ZTRSOMKLN
	0,	// 9: full
	0,	// 10: not supported
	0x7C00	// 11: skip OMKLN
	// higher counts either full or mixed config not supported.
};

static bool SkipHarmonic(AkUInt32 in_numHarmonics, AkUInt32 in_chanIdx)
{
	return in_numHarmonics <= 11 && MixedAmbisonicsSkipIndices[in_numHarmonics - 1] & (1 << in_chanIdx);
}

void CAkSpeakerPan::EncodeToAmbisonics(
	AkReal32 az,
	AkReal32 el,
	AK::SpeakerVolumes::VectorPtr in_pVolumes,
	AkUInt32 in_numHarmonics
)
{
	AkUInt32 order = s_numChanToOrderMinusOne[in_numHarmonics] + 1;
	// convert to spherical harmonic convention
	AkReal32 cosElev = cosf(el);
	_ComputeSN3DphericalHarmonics(cosf(az) * cosElev, -sinf(az) * cosElev, sinf(el), order, 1, in_pVolumes);
}

static void BuildSphereTransformationsPositionOnly(
	const AkVector & in_emitterPosition,
	const AkReal32 mxListRot[3][3],	// Unitary rotation matrix (row vectors in row major order).
	const AkVector & in_vListPosition,
	AkReal32 out_mxPointRot[3][3]
)
{
	// Compute rotation matrix for point source warping: from z to incident vector.
	{
		AkVector vPointEmitter;
		vPointEmitter.X = in_emitterPosition.X - in_vListPosition.X;
		vPointEmitter.Y = in_emitterPosition.Y - in_vListPosition.Y;
		vPointEmitter.Z = in_emitterPosition.Z - in_vListPosition.Z;

		AkReal32 * pFloat = &(out_mxPointRot[0][0]);

		if ((vPointEmitter.X * vPointEmitter.X + vPointEmitter.Y * vPointEmitter.Y + vPointEmitter.Z * vPointEmitter.Z) > 0.f)
		{
			const AkReal32 * AK_RESTRICT pListRotation = &(mxListRot[0][0]);
			AkVector vRelPointEmitter;
			AkMath::UnrotateVector(vPointEmitter, pListRotation, vRelPointEmitter);


			AkVector vListFront = { 0, 0, 1 };	// { mxListRot[2][0], mxListRot[2][1], mxListRot[2][2] };
			AkReal32 fNorm = 1.f / AkMath::Magnitude(vRelPointEmitter);
			AkVector vEmitFront = { fNorm * vRelPointEmitter.X, fNorm * vRelPointEmitter.Y, fNorm * vRelPointEmitter.Z };
			AkVector vAxis = AkMath::CrossProduct(vListFront, vEmitFront);
			if ((vAxis.X * vAxis.X + vAxis.Y * vAxis.Y + vAxis.Z * vAxis.Z) > AK_FLT_EPSILON)
			{
				AkReal32 cosa = AkMath::DotProduct(vListFront, vEmitFront);
				AkReal32 sina = AkMath::Magnitude(vAxis);
				AkReal32 oneminuscosa = 1.f - cosa;
				fNorm = 1.f / sina;
				vAxis.X *= fNorm;
				vAxis.Y *= fNorm;
				vAxis.Z *= fNorm;

				*pFloat++ = cosa + vAxis.X*vAxis.X*oneminuscosa;
				*pFloat++ = vAxis.Z * sina + vAxis.Y*vAxis.X*oneminuscosa;
				*pFloat++ = -vAxis.Y * sina + vAxis.Z*vAxis.X*oneminuscosa;
				*pFloat++ = -vAxis.Z * sina + vAxis.X*vAxis.Y*oneminuscosa;
				*pFloat++ = cosa + vAxis.Y*vAxis.Y*oneminuscosa;
				*pFloat++ = vAxis.X * sina + vAxis.Z*vAxis.Y*oneminuscosa;
				*pFloat++ = vAxis.Y * sina + vAxis.X*vAxis.Z*oneminuscosa;
				*pFloat++ = -vAxis.X * sina + vAxis.Y*vAxis.Z*oneminuscosa;
				*pFloat++ = cosa + vAxis.Z*vAxis.Z*oneminuscosa;
			}
			else
			{
				// Colinear.
				if ((vListFront.X * vEmitFront.X) + (vListFront.Y * vEmitFront.Y) + (vListFront.Z * vEmitFront.Z) < 0)
				{
					// Opposite directions.
					if (vListFront.X == vListFront.Y)
					{
						vAxis.X = -vListFront.Z;
						vAxis.Y = 0.f;
						vAxis.Z = vListFront.X;
					}
					else
					{
						vAxis.X = -vListFront.Y;
						vAxis.Y = vListFront.X;
						vAxis.Z = 0.f;
					}

					fNorm = 1.f / AkMath::Magnitude(vAxis);
					vAxis.X *= fNorm;
					vAxis.Y *= fNorm;
					vAxis.Z *= fNorm;

					AkReal32 cosa = -1.f;
					AkReal32 sina = 0.f;
					AkReal32 oneminuscosa = 2.f;
					*pFloat++ = cosa + vAxis.X*vAxis.X*oneminuscosa;
					*pFloat++ = vAxis.Z * sina + vAxis.Y*vAxis.X*oneminuscosa;
					*pFloat++ = -vAxis.Y * sina + vAxis.Z*vAxis.X*oneminuscosa;
					*pFloat++ = -vAxis.Z * sina + vAxis.X*vAxis.Y*oneminuscosa;
					*pFloat++ = cosa + vAxis.Y*vAxis.Y*oneminuscosa;
					*pFloat++ = vAxis.X * sina + vAxis.Z*vAxis.Y*oneminuscosa;
					*pFloat++ = vAxis.Y * sina + vAxis.X*vAxis.Z*oneminuscosa;
					*pFloat++ = -vAxis.X * sina + vAxis.Y*vAxis.Z*oneminuscosa;
					*pFloat++ = cosa + vAxis.Z*vAxis.Z*oneminuscosa;
				}
				else
				{
					// Same direction
					*pFloat++ = 1.f;
					*pFloat++ = 0.f;
					*pFloat++ = 0.f;
					*pFloat++ = 0.f;
					*pFloat++ = 1.f;
					*pFloat++ = 0.f;
					*pFloat++ = 0.f;
					*pFloat++ = 0.f;
					*pFloat++ = 1.f;
				}
			}
		}
		else
		{
			// 0 vector.
			*pFloat++ = 1.f;
			*pFloat++ = 0.f;
			*pFloat++ = 0.f;
			*pFloat++ = 0.f;
			*pFloat++ = 1.f;
			*pFloat++ = 0.f;
			*pFloat++ = 0.f;
			*pFloat++ = 0.f;
			*pFloat++ = 1.f;
		}
	}
}

static void BuildSphereTransformations(
	const AkTransform & in_emitter,
	const AkReal32 mxListRot[3][3],	// Unitary rotation matrix (row vectors in row major order).
	const AkVector & in_vListPosition,
	AkReal32 out_mxPointRot[3][3],
	AkReal32 out_mxCbxRot[3][3]	// Combined rotation 
	)
{
	// Compute rotation matrix for point source warping: from z to incident vector.
	BuildSphereTransformationsPositionOnly(in_emitter.Position(), mxListRot, in_vListPosition, out_mxPointRot);

	// Compute emitter matrix
	AkVector OrientationSide = AkMath::CrossProduct(in_emitter.OrientationTop(), in_emitter.OrientationFront());
	AkReal32 mxEmitRot[3][3];
	{
		AkReal32* pFloat = &(mxEmitRot[0][0]);

		// Build up rotation matrix out of our 3 basic row vectors, stored in row major order.
		*pFloat++ = OrientationSide.X;
		*pFloat++ = OrientationSide.Y;
		*pFloat++ = OrientationSide.Z;

		*pFloat++ = in_emitter.OrientationTop().X;
		*pFloat++ = in_emitter.OrientationTop().Y;
		*pFloat++ = in_emitter.OrientationTop().Z;

		*pFloat++ = in_emitter.OrientationFront().X;
		*pFloat++ = in_emitter.OrientationFront().Y;
		*pFloat++ = in_emitter.OrientationFront().Z;
	}

	// Combine all rotations
	{
		AkReal32 mxRelEmitRot[3][3];
		AkReal32 * pFloat = &(mxRelEmitRot[0][0]);
		*pFloat++ = mxEmitRot[0][0] * mxListRot[0][0] + mxEmitRot[0][1] * mxListRot[0][1] + mxEmitRot[0][2] * mxListRot[0][2];
		*pFloat++ = mxEmitRot[0][0] * mxListRot[1][0] + mxEmitRot[0][1] * mxListRot[1][1] + mxEmitRot[0][2] * mxListRot[1][2];
		*pFloat++ = mxEmitRot[0][0] * mxListRot[2][0] + mxEmitRot[0][1] * mxListRot[2][1] + mxEmitRot[0][2] * mxListRot[2][2];
		*pFloat++ = mxEmitRot[1][0] * mxListRot[0][0] + mxEmitRot[1][1] * mxListRot[0][1] + mxEmitRot[1][2] * mxListRot[0][2];
		*pFloat++ = mxEmitRot[1][0] * mxListRot[1][0] + mxEmitRot[1][1] * mxListRot[1][1] + mxEmitRot[1][2] * mxListRot[1][2];
		*pFloat++ = mxEmitRot[1][0] * mxListRot[2][0] + mxEmitRot[1][1] * mxListRot[2][1] + mxEmitRot[1][2] * mxListRot[2][2];
		*pFloat++ = mxEmitRot[2][0] * mxListRot[0][0] + mxEmitRot[2][1] * mxListRot[0][1] + mxEmitRot[2][2] * mxListRot[0][2];
		*pFloat++ = mxEmitRot[2][0] * mxListRot[1][0] + mxEmitRot[2][1] * mxListRot[1][1] + mxEmitRot[2][2] * mxListRot[1][2];
		*pFloat++ = mxEmitRot[2][0] * mxListRot[2][0] + mxEmitRot[2][1] * mxListRot[2][1] + mxEmitRot[2][2] * mxListRot[2][2];

		pFloat = &(out_mxCbxRot[0][0]);
		*pFloat++ = mxRelEmitRot[0][0] * out_mxPointRot[0][0] + mxRelEmitRot[0][1] * out_mxPointRot[0][1] + mxRelEmitRot[0][2] * out_mxPointRot[0][2];
		*pFloat++ = mxRelEmitRot[0][0] * out_mxPointRot[1][0] + mxRelEmitRot[0][1] * out_mxPointRot[1][1] + mxRelEmitRot[0][2] * out_mxPointRot[1][2];
		*pFloat++ = mxRelEmitRot[0][0] * out_mxPointRot[2][0] + mxRelEmitRot[0][1] * out_mxPointRot[2][1] + mxRelEmitRot[0][2] * out_mxPointRot[2][2];
		*pFloat++ = mxRelEmitRot[1][0] * out_mxPointRot[0][0] + mxRelEmitRot[1][1] * out_mxPointRot[0][1] + mxRelEmitRot[1][2] * out_mxPointRot[0][2];
		*pFloat++ = mxRelEmitRot[1][0] * out_mxPointRot[1][0] + mxRelEmitRot[1][1] * out_mxPointRot[1][1] + mxRelEmitRot[1][2] * out_mxPointRot[1][2];
		*pFloat++ = mxRelEmitRot[1][0] * out_mxPointRot[2][0] + mxRelEmitRot[1][1] * out_mxPointRot[2][1] + mxRelEmitRot[1][2] * out_mxPointRot[2][2];
		*pFloat++ = mxRelEmitRot[2][0] * out_mxPointRot[0][0] + mxRelEmitRot[2][1] * out_mxPointRot[0][1] + mxRelEmitRot[2][2] * out_mxPointRot[0][2];
		*pFloat++ = mxRelEmitRot[2][0] * out_mxPointRot[1][0] + mxRelEmitRot[2][1] * out_mxPointRot[1][1] + mxRelEmitRot[2][2] * out_mxPointRot[1][2];
		*pFloat++ = mxRelEmitRot[2][0] * out_mxPointRot[2][0] + mxRelEmitRot[2][1] * out_mxPointRot[2][1] + mxRelEmitRot[2][2] * out_mxPointRot[2][2];
	}
}

static void TransformSampledSphereNoSpread(
	const AkReal32 in_sampling[],	// expected: 4 floats (x,y,z,0) per vector
	AkUInt32 in_uNumSamples,
	const AkTransform & in_emitter,
	const AkReal32 mxListRot[3][3],	// Unitary rotation matrix (row vectors in row major order).
	const AkVector & in_vListPosition,
	AkReal32 out_arXformedSamples[]	// Returned transformed vectors // expected: 4 floats (x,y,z,0) per vector
	)
{
	//AK_INSTRUMENT_SCOPE("TransformSampledSphere");

	AkReal32 mxPointRot[3][3];

	BuildSphereTransformationsPositionOnly(
		in_emitter.Position(),
		mxListRot,
		in_vListPosition,
		mxPointRot
		);

	const AkReal32 * AK_RESTRICT pSamples = in_sampling;
	const AkReal32 * AK_RESTRICT pSamplesEnd = pSamples + 4 * in_uNumSamples;
	AkReal32 * AK_RESTRICT pXformedSamples = out_arXformedSamples;

	while (pSamples < pSamplesEnd)
	{
		AkVector vIn;
		vIn.X = pSamples[0];
		vIn.Y = pSamples[1];
		vIn.Z = pSamples[2];

		pSamples += 4;

		// Rotate field to desired orientation.
		AkVector vRotated;
		const AkReal32 * AK_RESTRICT pPointRotation = &(mxPointRot[0][0]);
		AkMath::RotateVector(vIn, pPointRotation, vRotated);
		pXformedSamples[0] = vRotated.X;
		pXformedSamples[1] = vRotated.Y;
		pXformedSamples[2] = vRotated.Z;

		pXformedSamples += 4;
	}
}

// rotate (pPreRotation), contract (spread [0,1] <-> [full, no] contraction), rotate (pPostRotation)
template< class Contract >
static void RotateAndContractPoints(
	const AkReal32 * AK_RESTRICT pSamples,
	AkUInt32 in_uNumSamples,
	AkReal32 in_spread,
	const AkReal32 * AK_RESTRICT pPreRotation,
	const AkReal32 * AK_RESTRICT pPostRotation,
	AkReal32 * AK_RESTRICT pXformedSamples)
{
	AKSIMD_V4F32 mr1_11 = AKSIMD_LOAD1_V4F32(pPreRotation[0]);
	AKSIMD_V4F32 mr1_12 = AKSIMD_LOAD1_V4F32(pPreRotation[1]);
	AKSIMD_V4F32 mr1_13 = AKSIMD_LOAD1_V4F32(pPreRotation[2]);
	AKSIMD_V4F32 mr1_21 = AKSIMD_LOAD1_V4F32(pPreRotation[3]);
	AKSIMD_V4F32 mr1_22 = AKSIMD_LOAD1_V4F32(pPreRotation[4]);
	AKSIMD_V4F32 mr1_23 = AKSIMD_LOAD1_V4F32(pPreRotation[5]);
	AKSIMD_V4F32 mr1_31 = AKSIMD_LOAD1_V4F32(pPreRotation[6]);
	AKSIMD_V4F32 mr1_32 = AKSIMD_LOAD1_V4F32(pPreRotation[7]);
	AKSIMD_V4F32 mr1_33 = AKSIMD_LOAD1_V4F32(pPreRotation[8]);

	AKSIMD_V4F32 mr2_11 = AKSIMD_LOAD1_V4F32(pPostRotation[0]);
	AKSIMD_V4F32 mr2_12 = AKSIMD_LOAD1_V4F32(pPostRotation[1]);
	AKSIMD_V4F32 mr2_13 = AKSIMD_LOAD1_V4F32(pPostRotation[2]);
	AKSIMD_V4F32 mr2_21 = AKSIMD_LOAD1_V4F32(pPostRotation[3]);
	AKSIMD_V4F32 mr2_22 = AKSIMD_LOAD1_V4F32(pPostRotation[4]);
	AKSIMD_V4F32 mr2_23 = AKSIMD_LOAD1_V4F32(pPostRotation[5]);
	AKSIMD_V4F32 mr2_31 = AKSIMD_LOAD1_V4F32(pPostRotation[6]);
	AKSIMD_V4F32 mr2_32 = AKSIMD_LOAD1_V4F32(pPostRotation[7]);
	AKSIMD_V4F32 mr2_33 = AKSIMD_LOAD1_V4F32(pPostRotation[8]);

	AKASSERT((in_uNumSamples % 4) == 0);
	const AkReal32 * AK_RESTRICT pSamplesEnd = pSamples + 4 * in_uNumSamples;

	Contract contract;
	contract.Setup(in_spread);

	while (pSamples < pSamplesEnd)
	{
		// Load 4 vectors, and group the x y and z
		AKSIMD_V4F32 v0 = AKSIMD_LOAD_V4F32(pSamples);
		AKSIMD_V4F32 v1 = AKSIMD_LOAD_V4F32(pSamples+4);
		AKSIMD_V4F32 v2 = AKSIMD_LOAD_V4F32(pSamples+8);
		AKSIMD_V4F32 v3 = AKSIMD_LOAD_V4F32(pSamples+12);

		pSamples += 16;

		AKSIMD_V4F32 xxxx, yyyy, zzzz;
		AkMath::PermuteVectors3(v0, v1, v2, v3, xxxx, yyyy, zzzz);


		// Rotate

		// x' = x*m11 + y*m21 + z*m31
		AKSIMD_V4F32 xxxx_out = AKSIMD_MUL_V4F32(xxxx, mr1_11);
		xxxx_out = AKSIMD_MADD_V4F32(yyyy, mr1_21, xxxx_out);
		xxxx_out = AKSIMD_MADD_V4F32(zzzz, mr1_31, xxxx_out);

		// y' = x*m12 + y*m22 + z*m32
		AKSIMD_V4F32 yyyy_out = AKSIMD_MUL_V4F32(xxxx, mr1_12);
		yyyy_out = AKSIMD_MADD_V4F32(yyyy, mr1_22, yyyy_out);
		yyyy_out = AKSIMD_MADD_V4F32(zzzz, mr1_32, yyyy_out);

		// z' = x*m13 + y*m23 + z*m33
		AKSIMD_V4F32 zzzz_out = AKSIMD_MUL_V4F32(xxxx, mr1_13);
		zzzz_out = AKSIMD_MADD_V4F32(yyyy, mr1_23, zzzz_out);
		zzzz_out = AKSIMD_MADD_V4F32(zzzz, mr1_33, zzzz_out);

		// Contract
		contract.Contract(xxxx_out, yyyy_out, zzzz_out);

		// Restore field to desired orientation.
		//AkMath::RotateVector(vContracted, pPointRotation, vRestored);
		xxxx = xxxx_out;
		yyyy = yyyy_out;
		zzzz = zzzz_out;

		// x' = x*m11 + y*m21 + z*m31
		xxxx_out = AKSIMD_MUL_V4F32(xxxx, mr2_11);
		xxxx_out = AKSIMD_MADD_V4F32(yyyy, mr2_21, xxxx_out);
		xxxx_out = AKSIMD_MADD_V4F32(zzzz, mr2_31, xxxx_out);

		// y' = x*m12 + y*m22 + z*m32
		yyyy_out = AKSIMD_MUL_V4F32(xxxx, mr2_12);
		yyyy_out = AKSIMD_MADD_V4F32(yyyy, mr2_22, yyyy_out);
		yyyy_out = AKSIMD_MADD_V4F32(zzzz, mr2_32, yyyy_out);

		// z' = x*m13 + y*m23 + z*m33
		zzzz_out = AKSIMD_MUL_V4F32(xxxx, mr2_13);
		zzzz_out = AKSIMD_MADD_V4F32(yyyy, mr2_23, zzzz_out);
		zzzz_out = AKSIMD_MADD_V4F32(zzzz, mr2_33, zzzz_out);

		// Store
		AkMath::UnpermuteVectors3(xxxx_out, yyyy_out, zzzz_out, v0, v1, v2, v3);
		AKSIMD_STORE_V4F32(pXformedSamples, v0);
		AKSIMD_STORE_V4F32(pXformedSamples+4, v1);
		AKSIMD_STORE_V4F32(pXformedSamples+8, v2);
		AKSIMD_STORE_V4F32(pXformedSamples+12, v3);

		pXformedSamples += 16;
	}
}

static void TransformSampledSphereSIMD(
	const AkReal32 in_sampling[],	// expected: 4 floats (x,y,z,0) per vector
	AkUInt32 in_uNumSamples,
	const AkTransform & in_emitter,
	const AkReal32 mxListRot[3][3],	// Unitary rotation matrix (row vectors in row major order).
	const AkVector & in_vListPosition,
	AkReal32 in_fSpread,
	AkReal32 out_arXformedSamples[]	// expected: 4 floats (x,y,z,0) per vector
)
{
	AKASSERT(in_fSpread >= 0);	// in_fSpread should be [0,1], but ContractInside() is not too sensitive to spread that slightly overshoots 1, as opposed to ContractOutside() which is sensitive to spread slightly below 0.
	//AK_INSTRUMENT_SCOPE("TransformSampledSphere");

	AkReal32 mxPointRot[3][3];
	AkReal32 mxCbxRot[3][3];

	BuildSphereTransformations(
		in_emitter,
		mxListRot,
		in_vListPosition,
		mxPointRot,	// point rotation (z to contraction direction)
		mxCbxRot	// combined rotation (emitter-listener rotation * contraction direction to z)
	);

	if (in_fSpread <= 0.5f)
	{
		RotateAndContractPoints<ContractOutside>(
			in_sampling,
			in_uNumSamples,
			in_fSpread,
			&(mxCbxRot[0][0]),
			&(mxPointRot[0][0]),
			out_arXformedSamples);
	}
	else
	{
		RotateAndContractPoints<ContractInside>(
			in_sampling,
			in_uNumSamples,
			in_fSpread,
			&(mxCbxRot[0][0]),
			&(mxPointRot[0][0]),
			out_arXformedSamples);
	}
}

void ComputeEncoderTransposed(AkUInt32 in_uNumHarmonics, AkUInt32 in_order, AkReal32 * encode_t, const AkReal32 * in_samples, AkUInt32 in_numSamples)
{
	for (AkUInt32 uSample = 0; uSample < in_numSamples; uSample++)
	{
		AkVector * sample = (AkVector*)((AKSIMD_V4F32*)in_samples + uSample);
		CAkSpeakerPan::ComputeSN3DphericalHarmonics(sample->X, sample->Y, sample->Z, in_order, in_numSamples, (encode_t + DECODER_IDX(0, uSample, in_numSamples)));
	}
}

static const AkReal32 * GetEncoderTransposed(
	int					in_orderidx,			// Ambisonic order (index - 0 == 1st order)
	AkUInt32 & out_numSamples)
{
	out_numSamples = s_numSamples3Dsampling[in_orderidx];
	AkReal32 * encode_t = s_encoder_t[in_orderidx];
	if (!encode_t)
	{
		AkUInt32 numHarmonics = CAkSpeakerPan::OrderMinusOneToNumHarmonics(in_orderidx);
		encode_t = (AkReal32 *)AkAlloc(AkMemID_Object, out_numSamples * numHarmonics * sizeof(AkReal32));
		if (encode_t)
		{
			const AkReal32 * samples = s_sampling3D[in_orderidx];
			ComputeEncoderTransposed(numHarmonics, in_orderidx + 1, encode_t, samples, out_numSamples);
			s_encoder_t[in_orderidx] = encode_t;
		}
	}
	return encode_t;
}

void CAkSpeakerPan::EncodeVector(
	int					in_orderidx,			// Ambisonic order (index - 0 == 1st order)
	AK::SpeakerVolumes::VectorPtr in_volumes,	// Number of samples must correspond to internal sphere sampling for given order (obtainable via GetDecodingMatrixShape)
	AK::SpeakerVolumes::VectorPtr out_volumes	// Encoded vector (length == number of harmonics for given order)
	)
{
	// Create encoding matrix.
	/// REVIEW bake?
	AkUInt32 numSamples;
	AkUInt32 numHarmonics = OrderMinusOneToNumHarmonics(in_orderidx);
	const AkReal32 * encoder = GetEncoderTransposed(in_orderidx, numSamples);
	if (encoder)
	{
		for (AkUInt32 uHarm = 0; uHarm < numHarmonics; uHarm++)
		{
			AkReal32 out = 0.f;
			for (AkUInt32 uSample = 0; uSample < numSamples; uSample++)
			{
				out += encoder[DECODER_IDX(uHarm, uSample, numSamples)] * in_volumes[uSample];
			}
			out_volumes[uHarm] = out;
		}
	}
	else
	{
		AK::SpeakerVolumes::Vector::Zero(out_volumes, numHarmonics);
	}
}

#define MAKE_MASK_AND_IDX(__mask__,__idx__) \
	((__idx__ << 16) | __mask__)
#define TAKE_IDX(__mask_and_idx__) \
	((__mask_and_idx__ >> 16) & 0xFFFF)
#define TAKE_MASK(__mask_and_idx__) \
	((__mask_and_idx__) & 0xFFFF)
// +1 for the the arc that wraps around.
static AkUInt32 ChannelMasksAndIndicesForSpread[][AK_NUM_CHANNELS_FOR_SPREAD+1] =
{
	{ MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_CENTER, 0) },	// mono
	// stereo: L, R, R, L, L
	{ MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT) },	
	// 3.x: L, C, R, R, L, L
	{ MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_CENTER, AK_IDX_SETUP_CENTER), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT) },
	// 4.x: SL, FL, FR, SR, SL
	{ MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_LEFT, AK_IDX_SETUP_NOCENTER_BACK_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_RIGHT, AK_IDX_SETUP_NOCENTER_BACK_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_LEFT, AK_IDX_SETUP_NOCENTER_BACK_LEFT) },
	// 5.x: SL, FL, C, FR, SR, SL
	{ MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_LEFT, AK_IDX_SETUP_WITHCENTER_BACK_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_CENTER, AK_IDX_SETUP_CENTER), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_RIGHT, AK_IDX_SETUP_WITHCENTER_BACK_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_LEFT, AK_IDX_SETUP_WITHCENTER_BACK_LEFT) }
#ifdef AK_71AUDIO
	// 6.x: BL, SL, FL, FR, SR, BR, BL
	, { MAKE_MASK_AND_IDX(AK_SPEAKER_BACK_LEFT, AK_IDX_SETUP_NOCENTER_BACK_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_LEFT, AK_IDX_SETUP_NOCENTER_SIDE_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_RIGHT, AK_IDX_SETUP_NOCENTER_SIDE_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_BACK_RIGHT, AK_IDX_SETUP_NOCENTER_BACK_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_BACK_LEFT, AK_IDX_SETUP_NOCENTER_BACK_LEFT) },
	// 7.x: BL, SL, FL, C, FR, SR, BR, BL
	{ MAKE_MASK_AND_IDX(AK_SPEAKER_BACK_LEFT, AK_IDX_SETUP_WITHCENTER_BACK_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_LEFT, AK_IDX_SETUP_WITHCENTER_SIDE_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_CENTER, AK_IDX_SETUP_CENTER), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_RIGHT, AK_IDX_SETUP_WITHCENTER_SIDE_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_BACK_RIGHT, AK_IDX_SETUP_WITHCENTER_BACK_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_BACK_LEFT, AK_IDX_SETUP_WITHCENTER_BACK_LEFT) }
#endif
};

static void CopyRotatedCopiesOntoChannels(AkUInt32 in_uChannels, AkUInt32 in_uNumPoints, const AkReal32 * in_arPoints, AkReal32 * out_arXformedPoints)
{
	static const AkVector s_chanDir[][AK_NUM_CHANNELS_FOR_SPREAD] =
	{
		{ { 0, 0, 1 } }, // mono
		{ { -1, 0, 0 }, { 1, 0, 0 } },	// stereo: L, R
		{ { -1, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0 } },	// 3.x: L, C, R
		{ { -ONE_OVER_SQRT_OF_TWO, 0, -ONE_OVER_SQRT_OF_TWO }, { -ONE_OVER_SQRT_OF_TWO, 0, ONE_OVER_SQRT_OF_TWO }, { ONE_OVER_SQRT_OF_TWO, 0, ONE_OVER_SQRT_OF_TWO }, { ONE_OVER_SQRT_OF_TWO, 0, -ONE_OVER_SQRT_OF_TWO } },	// 4.x: SL, FL, FR, SR
		{ { -0.58778525, 0, -0.80901699 }, { -0.95105652, 0, 0.30901699 }, { 0, 0, 1 }, { 0.95105652, 0, 0.30901699 }, { 0.58778525, 0, -0.80901699 } },	// 5.x: SL, FL, C, FR, SR
#ifdef AK_71AUDIO
		{ { -0.5, 0, -8.66025404e-01 }, { -1, 0, 0 }, { -0.5, 0, 8.66025404e-01 }, { 0.5, 0, 8.66025404e-01 }, { 1, 0, 0 }, { 0.5, 0, -8.66025404e-01 } },	// 6.x: BL, SL, FL, FR, SR, BR
		{ { -0.43388374, 0, -0.90096887 }, { -0.97492791, 0, -0.22252093 }, { -0.78183148, 0, 0.6234898 }, { 0, 0, 1 }, { 0.78183148, 0, 0.6234898 }, { 0.97492791, 0, -0.22252093 }, { 0.43388374, 0, -0.90096887 } },	// 7.x: BL, SL, FL, C, FR, SR, BR
#endif
	};

	AkReal32 * AK_RESTRICT pXformedPoints = out_arXformedPoints;
	for (AkUInt32 uChan = 0; uChan < in_uChannels; uChan++)
	{
		// Build up rotation matrix out of our 3 basic row vectors, stored in row major order.
		const AkVector top = { 0.f, 1.f, 0.f };
		const AkVector & front = s_chanDir[in_uChannels - 1][uChan];
		AkVector OrientationSide = AkMath::CrossProduct(top, front);

		/// TODO Reduce as a 2D rotation.
		AKSIMD_V4F32 mr1_11 = AKSIMD_LOAD1_V4F32(OrientationSide.X);
		AKSIMD_V4F32 mr1_12 = AKSIMD_LOAD1_V4F32(OrientationSide.Y);
		AKSIMD_V4F32 mr1_13 = AKSIMD_LOAD1_V4F32(OrientationSide.Z);
		AKSIMD_V4F32 mr1_21 = AKSIMD_SET_V4F32(0.f);
		AKSIMD_V4F32 mr1_22 = AKSIMD_SET_V4F32(1.f);
		AKSIMD_V4F32 mr1_23 = AKSIMD_SET_V4F32(0.f);
		AKSIMD_V4F32 mr1_31 = AKSIMD_LOAD1_V4F32(front.X);
		AKSIMD_V4F32 mr1_32 = AKSIMD_LOAD1_V4F32(front.Y);
		AKSIMD_V4F32 mr1_33 = AKSIMD_LOAD1_V4F32(front.Z);

		/// Matrix is built; rotate and store samples
		AKASSERT((in_uNumPoints % 4) == 0);
		const AkReal32 * AK_RESTRICT pPoints = in_arPoints;
		const AkReal32 * AK_RESTRICT pPointsEnd = pPoints + 4 * in_uNumPoints;
		
		do
		{
			// Load 4 vectors, and group the x y and z
			AKSIMD_V4F32 v0 = AKSIMD_LOAD_V4F32(pPoints);
			AKSIMD_V4F32 v1 = AKSIMD_LOAD_V4F32(pPoints + 4);
			AKSIMD_V4F32 v2 = AKSIMD_LOAD_V4F32(pPoints + 8);
			AKSIMD_V4F32 v3 = AKSIMD_LOAD_V4F32(pPoints + 12);

			pPoints += 16;

			AKSIMD_V4F32 xxxx, yyyy, zzzz;
			AkMath::PermuteVectors3(v0, v1, v2, v3, xxxx, yyyy, zzzz);


			// Rotate

			// x' = x*m11 + y*m21 + z*m31
			AKSIMD_V4F32 xxxx_out = AKSIMD_MUL_V4F32(xxxx, mr1_11);
			xxxx_out = AKSIMD_MADD_V4F32(yyyy, mr1_21, xxxx_out);
			xxxx_out = AKSIMD_MADD_V4F32(zzzz, mr1_31, xxxx_out);

			// y' = x*m12 + y*m22 + z*m32
			AKSIMD_V4F32 yyyy_out = AKSIMD_MUL_V4F32(xxxx, mr1_12);
			yyyy_out = AKSIMD_MADD_V4F32(yyyy, mr1_22, yyyy_out);
			yyyy_out = AKSIMD_MADD_V4F32(zzzz, mr1_32, yyyy_out);

			// z' = x*m13 + y*m23 + z*m33
			AKSIMD_V4F32 zzzz_out = AKSIMD_MUL_V4F32(xxxx, mr1_13);
			zzzz_out = AKSIMD_MADD_V4F32(yyyy, mr1_23, zzzz_out);
			zzzz_out = AKSIMD_MADD_V4F32(zzzz, mr1_33, zzzz_out);

			// Store
			AkMath::UnpermuteVectors3(xxxx_out, yyyy_out, zzzz_out, v0, v1, v2, v3);
			AKSIMD_STORE_V4F32(pXformedPoints, v0);
			AKSIMD_STORE_V4F32(pXformedPoints + 4, v1);
			AKSIMD_STORE_V4F32(pXformedPoints + 8, v2);
			AKSIMD_STORE_V4F32(pXformedPoints + 12, v3);

			pXformedPoints += 16;

		} while (pPoints < pPointsEnd);
	}
}

/// REVIEW split masks and indices in 2 tables?
static AkUInt32 ChannelMasksAndIndicesNoCenter[][AK_NUM_CHANNELS_FOR_SPREAD + 1] =
{
	{ MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_CENTER, 0) },	// mono
	// stereo: L, R, R, L, L
	{ MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT) },
	// UNUSED 3.x: L, C, R, R, L, L
	{ MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_CENTER, AK_IDX_SETUP_CENTER), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT) },
	// 4.x: SL, FL, FR, SR, SL
	{ MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_LEFT, AK_IDX_SETUP_WITHCENTER_BACK_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_RIGHT, AK_IDX_SETUP_WITHCENTER_BACK_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_LEFT, AK_IDX_SETUP_WITHCENTER_BACK_LEFT) },
	// UNUSED 5.x: SL, FL, C, FR, SR, SL
	{ MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_LEFT, AK_IDX_SETUP_WITHCENTER_BACK_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_CENTER, AK_IDX_SETUP_CENTER), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_RIGHT, AK_IDX_SETUP_WITHCENTER_BACK_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_LEFT, AK_IDX_SETUP_WITHCENTER_BACK_LEFT) }
#ifdef AK_71AUDIO
	// 6.x: BL, SL, FL, FR, SR, BR, BL
	, { MAKE_MASK_AND_IDX(AK_SPEAKER_BACK_LEFT, AK_IDX_SETUP_WITHCENTER_BACK_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_LEFT, AK_IDX_SETUP_WITHCENTER_SIDE_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_RIGHT, AK_IDX_SETUP_WITHCENTER_SIDE_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_BACK_RIGHT, AK_IDX_SETUP_WITHCENTER_BACK_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_BACK_LEFT, AK_IDX_SETUP_WITHCENTER_BACK_LEFT) },
	// UNUSED 7.x: BL, SL, FL, C, FR, SR, BR, BL
	{ MAKE_MASK_AND_IDX(AK_SPEAKER_BACK_LEFT, AK_IDX_SETUP_WITHCENTER_BACK_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_LEFT, AK_IDX_SETUP_WITHCENTER_SIDE_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_LEFT, AK_IDX_SETUP_FRONT_LEFT), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_CENTER, AK_IDX_SETUP_CENTER), MAKE_MASK_AND_IDX(AK_SPEAKER_FRONT_RIGHT, AK_IDX_SETUP_FRONT_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_SIDE_RIGHT, AK_IDX_SETUP_WITHCENTER_SIDE_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_BACK_RIGHT, AK_IDX_SETUP_WITHCENTER_BACK_RIGHT), MAKE_MASK_AND_IDX(AK_SPEAKER_BACK_LEFT, AK_IDX_SETUP_WITHCENTER_BACK_LEFT) }
#endif
};

// Important: in_uNumArcs not necessarily equal to in_uNumChannels, for handling panning on subset of the channel config used (like with center% < 100)
void CAkSpeakerPan::AddPowerVbap2d(void * in_pPanCache, AkUInt32 in_uNumArcs, const AkVector & point, AkReal32 in_fGain, AkUInt32 indexMap[AK_NUM_CHANNELS_FOR_SPREAD + 1], AK::SpeakerVolumes::VectorPtr pChanIn)
{
	Matrix2x2 * arArcs = (Matrix2x2 *)in_pPanCache;
	AkUInt32 uArc = 0;
	AkReal32 components[4];

	// Attempt to find the best "arc" for the point provided
	// Best is defined as, the uArc where the point transformed by the 2x2 matrix
	// has both x and z components in the domain [0,+inf)
	// e.g. Vals[2] = { arArcs.a * pointX + arArcs.b * pointZ, arArcs.c * pointX + arArcs.d * pointZ}
	// and Vals[0] and Vals[1] are BOTH >= 0
	do
	{
		components[0] = arArcs[uArc].a * point.X;
		components[1] = arArcs[uArc].b * point.Z;
		components[2] = arArcs[uArc].c * point.X;
		components[3] = arArcs[uArc].d * point.Z;
		uArc++;
	} while (components[0] < -components[1] || components[2] < -components[3]);

	// Compute channel gains.
	uArc--;
	AKASSERT(uArc < in_uNumArcs);
	AkReal32 vals[2] = {
		components[0] + components[1] + AK_FLT_EPSILON,
		components[2] + components[3] + AK_FLT_EPSILON,
	};
	AkReal32 valsSquared[2] = {
		vals[0] * vals[0],
		vals[1] * vals[1]
	};
	AkReal32 normFactor = in_fGain / (valsSquared[0] + valsSquared[1]);
	pChanIn[TAKE_IDX(indexMap[uArc])] += valsSquared[0] * normFactor;
	// Note: Maps have an extra column to allow wrap arounds.
	pChanIn[TAKE_IDX(indexMap[(uArc + 1)])] += valsSquared[1] * normFactor;
}

void CAkSpeakerPan::AddHeadphonePower(AkReal32 x, AK::SpeakerVolumes::VectorPtr pChanIn)
{
	// A quadratic interpolation is good enough for headphone panning.
	const AkReal32 a = ONE_OVER_SQRT_OF_TWO;
	const AkReal32 b = 0.5f;
	const AkReal32 c = 0.5f - ONE_OVER_SQRT_OF_TWO;
	AkReal32 right = a + b*x + c*x*x;
	right *= right;
	AkReal32 left = (right <= 1.f) ? 1.f - right : 0.f;
	pChanIn[0] += left;
	pChanIn[1] += right;
}

static inline void GenPointsForZeroSpread(
	AkUInt32 in_uNumSamples,
	const AkTransform & in_emitter,
	const AkReal32 in_mxListRot[3][3],	// Unitary rotation matrix (row vectors in row major order).
	const AkVector & in_vListPosition,
	AkReal32 out_arXformedSamples[]	// Returned transformed vectors // expected: 4 floats (x,y,z,0) per vector, in_uNumSamples vectors
	)
{
	AkReal32 * arSamples = (AkReal32*)AkAlloca(in_uNumSamples * sizeof(AKSIMD_V4F32));
	
	//AK_INSTRUMENT_SCOPE("Prepare virtual points");

	AkReal32 * pPoints = arSamples;
	AkReal32 * pPointsEnd = pPoints + 4 * in_uNumSamples;
	do
	{
		// { 0, 0, 1 };
		pPoints[0] = 0.f;
		pPoints[1] = 0.f;
		pPoints[3] = pPoints[2] = 1.f;
		pPoints += 4;
	} while (pPoints < pPointsEnd);
	
	// Perform field transformation: rotation and spread.
	TransformSampledSphereNoSpread(
		arSamples,
		in_uNumSamples,
		in_emitter,			// Emitter transform
		in_mxListRot,		// Listener rotation
		in_vListPosition,	// Listener position
		out_arXformedSamples	// Returned transformed vectors
		);
}

// Compute speaker matrix for 3D panning with spread.
void CAkSpeakerPan::GetSpeakerVolumes( 
	const AkTransform & in_emitter, 
	AkReal32			in_fDivergenceCenter,
	AkReal32			in_fSpread,
	AkReal32			in_fFocus,
	AK::SpeakerVolumes::MatrixPtr out_pVolumes,	// Returned volume matrix. Assumes it is preinitialized with zeros.
	AkChannelConfig		in_supportedInputConfig, // LFE and height channels not supported with standard configs.
	AkChannelMask		in_uSelInputChannelMask, // Mask of selected input channels
	AkChannelConfig		in_outputConfig,	// config of bus to which this signal is routed.
	const AkVector &	in_vListPosition,	// listener position
	const AkReal32		in_mxListRot[3][3],
	AkDevice *			in_pDevice
	)
{
	AKASSERT((in_supportedInputConfig.uNumChannels > 0 && !in_supportedInputConfig.HasLFE())
		|| !"Input config not supported for 3D panning");

	// Ignore LFE.
#ifdef AK_71AUDIO
	AkChannelConfig	outputConfigNoLfe;
	if (in_outputConfig.eConfigType == AK_ChannelConfigType_Standard)
		outputConfigNoLfe.SetStandard((in_outputConfig.uChannelMask) & ~AK_SPEAKER_LOW_FREQUENCY);
	else
		outputConfigNoLfe = in_outputConfig;
#else
	AkChannelConfig	outputConfigNoLfe = in_outputConfig.RemoveLFE();
#endif

	AkUInt32 uMaskExisting = in_supportedInputConfig.uChannelMask;	// Mask of existing channels in the input. Will be shifted to the right.
	AkUInt32 uMaskDesired = uMaskExisting & in_uSelInputChannelMask;	// Mask of desired input channels, stripped of non existing channels.
	AkUInt32 uNumInputChannels = in_supportedInputConfig.uNumChannels;	// Real number of input channels.
	
	// Handle mono and LFE output.
	if (outputConfigNoLfe.uNumChannels <= 1)
	{
		if (outputConfigNoLfe.uNumChannels == 1)
		{
			if (in_supportedInputConfig.eConfigType == AK_ChannelConfigType_Ambisonic)
			{
				AK::SpeakerVolumes::Matrix::GetChannel(out_pVolumes, 0, in_outputConfig.uNumChannels)[0] = 1.f;
			}
			else
			{
				uMaskDesired = in_uSelInputChannelMask;
				AkUInt32 iChannel = 0;
				do
				{
					AkUInt32 uMaskAndIdx = ChannelMasksAndIndicesForSpread[uNumInputChannels - 1][iChannel];
					if (TAKE_MASK(uMaskAndIdx) & uMaskDesired)
					{
						AK::SpeakerVolumes::Matrix::GetChannel(out_pVolumes, TAKE_IDX(uMaskAndIdx), in_outputConfig.uNumChannels)[0] = 1.f;
					}
				} while (++iChannel < uNumInputChannels);
			}
		}
		return;
	}
	else if (in_supportedInputConfig.eConfigType == AK_ChannelConfigType_Anonymous || in_outputConfig.eConfigType == AK_ChannelConfigType_Anonymous)
	{
		// 3D positioning not supported for anonymous configurations. 
		GetSpeakerVolumesDirectNoMask(in_supportedInputConfig, in_outputConfig, out_pVolumes);
		return;
	}

	// Convert spread [0,100] to [0,1]
	in_fSpread *= 0.01f;
	
	// Input
	if (in_supportedInputConfig.eConfigType == AK_ChannelConfigType_Ambisonic)
	{
		/**
		AkInt64 iStart, iEnd;
		AKPLATFORM::PerformanceCounter(&iStart);
		**/

		// Ambisonics input:
		AkUInt32 uNumAmbisonicsChannels = in_supportedInputConfig.uNumChannels;
		if (in_outputConfig.eConfigType == AK_ChannelConfigType_Ambisonic && in_outputConfig.uNumChannels < uNumAmbisonicsChannels)	// With ambi->ambi, truncate to lower order.
			uNumAmbisonicsChannels = in_outputConfig.uNumChannels;

		// Transform sampling, compute transform matrix if output is ambisonics, pan each sample if it is standard (~AllRAD decoding)
		int orderidx = s_numChanToOrderMinusOne[uNumAmbisonicsChannels];
		AkUInt32 uNumHarmonics = OrderMinusOneToNumHarmonics(orderidx);
		AkUInt32 uNumSamples = s_numSamples3Dsampling[orderidx];
		const AkReal32 * samples = s_sampling3D[orderidx];

		if (in_fSpread < 0.f)
			in_fSpread = 0.f;

		AkReal32 * arXformedSamples = (AkReal32*)AkAlloca(uNumSamples * sizeof(AKSIMD_V4F32));
		TransformSampledSphereSIMD(
			samples,
			uNumSamples,
			in_emitter,			// Emitter transform
			in_mxListRot,		// Listener rotation
			in_vListPosition,	// Listener position
			in_fSpread,
			arXformedSamples	// Returned transformed vectors
			);

		AkReal32 fSquaredGain = 0.5f + 0.5f * in_fSpread * in_fSpread;

		const AkReal32 * decoder = GetSampledHarmonics3D(orderidx);
		if (!decoder)
			return; // unsupported config or oom
		
		if (in_outputConfig.eConfigType == AK_ChannelConfigType_Ambisonic)
		{
			// Allocate working ambisonics matrix.
			// Write it in transposed form to speed up multiply below.
			AK::SpeakerVolumes::MatrixPtr mxTransformedHarmonicsT = (AK::SpeakerVolumes::MatrixPtr)AkAlloca(AK::SpeakerVolumes::Matrix::GetRequiredSize(uNumHarmonics, uNumSamples));
			AkReal32 fGain = sqrtf(fSquaredGain);
			{
				//AK_INSTRUMENT_SCOPE("ComputeSN3DphericalHarmonics");
				for (AkUInt32 s = 0; s < uNumSamples; s++)
				{
					AkVector * pxformedSample = (AkVector *)((AKSIMD_V4F32*)arXformedSamples + s);
					ComputeSN3DphericalHarmonics(pxformedSample->X, pxformedSample->Y, pxformedSample->Z, orderidx + 1, uNumSamples, mxTransformedHarmonicsT + s);
				}
			}

			//AK_INSTRUMENT_SCOPE("Matrix Multiply");

			// Multiply onto mixing matrix (represented in transposed form: T'=Yrot'*Y). Handle mixed-order configs.
			const AkUInt32 uRowSize = AK::SpeakerVolumes::Vector::GetRequiredSize(uNumSamples);
			AkUInt32 uChanIn = 0;
			for (AkUInt32 in = 0; in < uNumHarmonics; in++)
			{
				// Skip harmonics for incomplete channel configs.
				if (SkipHarmonic(in_supportedInputConfig.uNumChannels, in))
					continue;

				AkReal32 normalize = fGain / uNumSamples;

				AK::SpeakerVolumes::ConstVectorPtr pDecoderFetch = &(decoder[in * uNumSamples]);
				AK::SpeakerVolumes::VectorPtr pDecoderRow = (AK::SpeakerVolumes::VectorPtr)AkAlloca(uRowSize);
				AK::SpeakerVolumes::Vector::Copy(pDecoderRow, pDecoderFetch, uNumSamples, normalize);
				AK::SpeakerVolumes::VectorPtr pChanIn = AK::SpeakerVolumes::Matrix::GetChannel(out_pVolumes, uChanIn, in_outputConfig.uNumChannels);
				for (AkUInt32 out = 0; out < uNumHarmonics; out++)
				{
					AK::SpeakerVolumes::ConstVectorPtr pHarmonic = AK::SpeakerVolumes::Matrix::GetChannel(mxTransformedHarmonicsT, out, uNumSamples);
					AK::SpeakerVolumes::ConstVectorPtr pDecoder = pDecoderRow;
					AK::SpeakerVolumes::ConstVectorPtr pHarmonicEnd = pHarmonic + uNumSamples;
					AKSIMD_V4F32 vOut = AKSIMD_SET_V4F32(0);
					do
					{
						AKSIMD_V4F32 vHarmonics = AKSIMD_LOAD_V4F32(pHarmonic);
						AKSIMD_V4F32 vDecoder = AKSIMD_LOAD_V4F32(pDecoder);
						pHarmonic += 4;
						pDecoder += 4;
						vOut = AKSIMD_MADD_V4F32(vHarmonics, vDecoder, vOut);
					} while (pHarmonic < pHarmonicEnd);
					vOut = AKSIMD_HORIZONTALADD_V4F32(vOut);
					AKSIMD_STORE1_V4F32(pChanIn+out, vOut);
				}
				uChanIn++;
			}
		}
		else if (in_pDevice)
		{
			void * pPanCache = in_pDevice->GetPanCache(outputConfigNoLfe);
			if (pPanCache)
			{
				bool b2D = !AK::HasHeightChannels(in_outputConfig.uChannelMask);

				// Compute mixing matrix for sampled sphere.
				AkUInt32 uVbapSize = AK::SpeakerVolumes::Matrix::GetRequiredSize(uNumSamples, outputConfigNoLfe.uNumChannels);
				AK::SpeakerVolumes::MatrixPtr pMxVBAP = (AK::SpeakerVolumes::MatrixPtr)AkAlloca(uVbapSize);
				AK::SpeakerVolumes::Matrix::Zero(pMxVBAP, uNumSamples, outputConfigNoLfe.uNumChannels);
				AkReal32 fOneOverNumSamples = 1.f / uNumSamples;
				AkReal32 fSquaredGainNum = fSquaredGain * fOneOverNumSamples;
				if (!b2D)
				{
					for (AkUInt32 uIn = 0; uIn < uNumSamples; uIn++)
					{
						AK::SpeakerVolumes::VectorPtr pIn = AK::SpeakerVolumes::Matrix::GetChannel(pMxVBAP, uIn, outputConfigNoLfe.uNumChannels);
						/// OPTI: Normalization / vector ops computed on sparse vector... ComputeVBAPSquared should return the gain sum out of the chosen triplet(s)
						AkVector * pxformedSample = (AkVector*)((AKSIMD_V4F32*)arXformedSamples + uIn);
						AkVBAP::ComputeVBAPSquared(pPanCache, pxformedSample->Z, -pxformedSample->X, pxformedSample->Y, outputConfigNoLfe.uNumChannels, pIn);
						AkReal32 fGainSum = AK::SpeakerVolumes::Vector::L1Norm(pIn, outputConfigNoLfe.uNumChannels);
						if (fGainSum > 0.f)
						{
							AK::SpeakerVolumes::Vector::Mul(pIn, fSquaredGainNum / fGainSum, outputConfigNoLfe.uNumChannels);
							AK::SpeakerVolumes::Vector::Sqrt(pIn, outputConfigNoLfe.uNumChannels); //convert power back to a gain
						}
					}
				}
				else
				{
					if (outputConfigNoLfe.uChannelMask == AK_SPEAKER_SETUP_STEREO && in_pDevice->ePanningRule == AkPanningRule_Headphones)
					{
						for (AkUInt32 uIn = 0; uIn < uNumSamples; uIn++)
						{
							AK::SpeakerVolumes::VectorPtr pChanIn = AK::SpeakerVolumes::Matrix::GetChannel(pMxVBAP, uIn, outputConfigNoLfe.uNumChannels);
							// Headphone panning.
							AkVector * pxformedSample = (AkVector*)((AKSIMD_V4F32*)arXformedSamples + uIn);
							AddHeadphonePower(pxformedSample->X, pChanIn);
							AK::SpeakerVolumes::Vector::Mul(pChanIn, fSquaredGainNum, in_outputConfig.uNumChannels);
							AK::SpeakerVolumes::Vector::Sqrt(pChanIn, in_outputConfig.uNumChannels); //convert power back to a gain
						}
					}
					else
					{
						AkUInt32 uNumArcs = GetNum2dArcs(outputConfigNoLfe.uNumChannels);
						for (AkUInt32 uIn = 0; uIn < uNumSamples; uIn++)
						{
							AK::SpeakerVolumes::VectorPtr pChanIn = AK::SpeakerVolumes::Matrix::GetChannel(pMxVBAP, uIn, outputConfigNoLfe.uNumChannels);
							AkVector * pxformedSample = (AkVector*)((AKSIMD_V4F32*)arXformedSamples + uIn);
							AddPowerVbap2d(pPanCache, uNumArcs, *pxformedSample, fSquaredGainNum, ChannelMasksAndIndicesForSpread[outputConfigNoLfe.uNumChannels - 1], pChanIn);
							AK::SpeakerVolumes::Vector::Sqrt(pChanIn, outputConfigNoLfe.uNumChannels);
						}
					}
				}

				// Mixing matrix is represented in transposed form: D'=Dopt*G'.
				AkReal32 normalize = k_surfSphere * fOneOverNumSamples;
				const AkReal32 * weights = s_weightsRE_3D[orderidx];	// whether panning occurs on a 2D or 3D config, decoding is made on a 3D sampling.
				
				AkUInt32 uChanIn = 0;
				for (AkUInt32 row = 0; row < uNumHarmonics && uChanIn < in_supportedInputConfig.uNumChannels; row++)
				{
					// Skip harmonics for incomplete channel configs.
					if (SkipHarmonic(in_supportedInputConfig.uNumChannels, row))
						continue;
					AK::SpeakerVolumes::VectorPtr pMxDecodeRow = AK::SpeakerVolumes::Matrix::GetChannel(out_pVolumes, uChanIn, in_outputConfig.uNumChannels);
					AK::SpeakerVolumes::Vector::Zero(pMxDecodeRow, in_outputConfig.uNumChannels);
					for (AkUInt32 col = 0; col < outputConfigNoLfe.uNumChannels; col++)
					{
						AkReal32 fValue = 0;
						for (AkUInt32 s = 0; s < uNumSamples; s++)
						{
							fValue += normalize * weights[row] * decoder[DECODER_IDX(row, s, uNumSamples)] * AK::SpeakerVolumes::Matrix::GetChannel(pMxVBAP, s, outputConfigNoLfe.uNumChannels)[col];
						}
						pMxDecodeRow[col] = fValue;
					}
					++uChanIn;
				}
			}
		}

		/**
		AKPLATFORM::PerformanceCounter(&iEnd);
		AkReal32 fDuration = AKPLATFORM::Elapsed(iEnd, iStart);
		char msg[16];
		sprintf(msg, "%f ms\n", fDuration);
		AKPLATFORM::OutputDebugMsg(msg);
		**/
	}
	else
	{
		// Standard configs.
		if(!in_pDevice)	//No signal path to device, therefore, no output.  
			return;
		
		// Clamp input channels to 7
		uNumInputChannels &= 0x7;

		AkUInt32 uUsedInputChannels = (uMaskDesired == uMaskExisting) ? uNumInputChannels : AK::GetNumNonZeroBits(uMaskDesired) & 0x7;
		if (uUsedInputChannels == 0)
			return;
		AkReal32 fOneOverNumChannels = 1.f / uUsedInputChannels;

		if (outputConfigNoLfe.eConfigType == AK_ChannelConfigType_Standard && !AK::HasHeightChannels(outputConfigNoLfe.uChannelMask))
		{
			AkUInt32 uNbVirtualPoints;
			AkReal32 *arXformedSamples;

			{
				// Optimized case for spread 0.
				if (in_fSpread <= 0.f)
				{
					uNbVirtualPoints = 1;
					arXformedSamples = (AkReal32*)AkAlloca(uUsedInputChannels * sizeof(AKSIMD_V4F32));
					GenPointsForZeroSpread(
						uUsedInputChannels,
						in_emitter,
						in_mxListRot,
						in_vListPosition,
						arXformedSamples
						);
				}
				else
				{
					//AK_INSTRUMENT_SCOPE("Prepare virtual points");

					// Get a spread arc prototype and contract it according to focus.

					const ArraySimdVector * pSpreadCache = in_pDevice->GetSpreadCache(uUsedInputChannels, false);
					if (!pSpreadCache)
						return;

					uNbVirtualPoints = pSpreadCache->Length();

					AkReal32 * arFocussedArc = (AkReal32*)AkAlloca(uNbVirtualPoints * sizeof(AKSIMD_V4F32));
					{
						const AkReal32 fFocusFactor = (1.f - in_fFocus * 0.01f); 
						if (fFocusFactor == 1.f)
						{
							// Special path for common case in_fFocus == 0, especially since ContractInside is more costly.
							memcpy(arFocussedArc, &((*pSpreadCache)[0]), uNbVirtualPoints * sizeof(AKSIMD_V4F32));
						}
						else if (fFocusFactor <= 0.5f)
						{
							ContractPoints<ContractOutside>(
								(AkReal32 *)&((*pSpreadCache)[0]),
								uNbVirtualPoints,
								arFocussedArc,
								fFocusFactor);
						}
						else
						{
							ContractPoints<ContractInside>(
								(AkReal32 *)&((*pSpreadCache)[0]),
								uNbVirtualPoints,
								arFocussedArc,
								fFocusFactor);
						}
					}

					// Rotate and copy focussed arc towards each _effectively used_ input channel.
					AkReal32 * arSamples = (AkReal32*)AkAlloca(uNbVirtualPoints * uUsedInputChannels * sizeof(AKSIMD_V4F32));
					CopyRotatedCopiesOntoChannels(uUsedInputChannels, uNbVirtualPoints, arFocussedArc, arSamples);

					// Perform field transformation: rotation and spread.
					arXformedSamples = (AkReal32*)AkAlloca(uNbVirtualPoints * uUsedInputChannels * sizeof(AKSIMD_V4F32));
					TransformSampledSphereSIMD(
						arSamples,
						uNbVirtualPoints * uUsedInputChannels,
						in_emitter,			// Emitter transform
						in_mxListRot,		// Listener rotation
						in_vListPosition,	// Listener position
						in_fSpread,
						arXformedSamples	// Returned transformed vectors
						);
				}
			}

			
			// 2D VBAP

			// Compute (inverse) matrix for each arc.
			void * pPanCache = in_pDevice->GetPanCache(outputConfigNoLfe);
			if (!pPanCache)
				return;
			
			//AK_INSTRUMENT_SCOPE("2D VBAP");

			AkUInt32 uNumArcs = GetNum2dArcs(outputConfigNoLfe.uNumChannels);

			// Get pan cache without center.
			AkChannelConfig outputConfigNoCenter = outputConfigNoLfe.RemoveCenter();

			AkUInt32 uNumArcsNoCenter = uNumArcs - 1;	/// Not used if outputConfigNoLfe == uOutputConfigNoCenter
			/// REVIEW Consider optimizing for center%=0
			void * pPanCacheNoCenter = NULL;
			if (outputConfigNoLfe != outputConfigNoCenter && in_fDivergenceCenter < 1.f)
				pPanCacheNoCenter = in_pDevice->GetPanCache(outputConfigNoCenter);


			AkReal32 fTotalAttenuationFactor = fOneOverNumChannels / uNbVirtualPoints;

			AkUInt32 uChannelIn = 0;
			AkUInt32 uUsedChannelIn = 0;
			do
			{
				AkUInt32 uMaskAndIdx = ChannelMasksAndIndicesForSpread[uNumInputChannels - 1][uChannelIn];
				if (TAKE_MASK(uMaskAndIdx) & uMaskDesired)
				{
					AK::SpeakerVolumes::VectorPtr pChanIn = AK::SpeakerVolumes::Matrix::GetChannel(out_pVolumes, TAKE_IDX(uMaskAndIdx), in_outputConfig.uNumChannels);

					const AkReal32 * pPoints = arXformedSamples + 4 * uNbVirtualPoints * uUsedChannelIn;
					const AkReal32 * pPointsEnd = pPoints + 4 * uNbVirtualPoints;
					do
					{
						AkVector point;
						point.X = pPoints[0];
						point.Y = pPoints[1];
						point.Z = pPoints[2];
						pPoints += 4;

						if (outputConfigNoLfe.uChannelMask == AK_SPEAKER_SETUP_STEREO && in_pDevice->ePanningRule == AkPanningRule_Headphones)
						{
							// Headphone panning.
							AddHeadphonePower(point.X, pChanIn);
						}
						else if (pPanCacheNoCenter)
						{
							// Speaker panning with center%
							AddPowerVbap2d(pPanCache, uNumArcs, point, in_fDivergenceCenter, ChannelMasksAndIndicesForSpread[outputConfigNoLfe.uNumChannels - 1], pChanIn);
							AddPowerVbap2d(pPanCacheNoCenter, uNumArcsNoCenter, point, (1.0f - in_fDivergenceCenter), ChannelMasksAndIndicesNoCenter[outputConfigNoLfe.uNumChannels - 2], pChanIn);
						}
						else
						{
							// Speaker panning
							AddPowerVbap2d(pPanCache, uNumArcs, point, 1.f, ChannelMasksAndIndicesForSpread[outputConfigNoLfe.uNumChannels - 1], pChanIn);
						}

					} while (pPoints < pPointsEnd);

					AK::SpeakerVolumes::Vector::Mul(pChanIn, fTotalAttenuationFactor, in_outputConfig.uNumChannels);
					AK::SpeakerVolumes::Vector::Sqrt(pChanIn, in_outputConfig.uNumChannels); //convert power back to a gain

					++uUsedChannelIn;
				}
			} while (++uChannelIn < uNumInputChannels);
		}
		else
		{
			// 3D output

			AkUInt32 uNbVirtualPoints;
			AkReal32 * arXformedSamples;

			{
				// Optimized case for spread 0.
				if (in_fSpread <= 0.f)
				{
					uNbVirtualPoints = 1;
					arXformedSamples = (AkReal32*)AkAlloca(uUsedInputChannels * sizeof(AKSIMD_V4F32));
					GenPointsForZeroSpread(
						uUsedInputChannels,
						in_emitter,
						in_mxListRot,
						in_vListPosition,
						arXformedSamples
						);
				}
				else
				{
					//AK_INSTRUMENT_SCOPE("Prepare virtual points");

					// Get a spread cap prototype and contract it according to focus.

					const ArraySimdVector * pSpreadCache = in_pDevice->GetSpreadCache(uUsedInputChannels, true);
					if (!pSpreadCache)
						return;

					uNbVirtualPoints = pSpreadCache->Length();

					AkReal32 * arFocussedCap = (AkReal32*)AkAlloca(uNbVirtualPoints * sizeof(AKSIMD_V4F32));
					{
						const AkReal32 fFocusFactor = (1.f - in_fFocus * 0.01f);
						if (fFocusFactor == 1.f)
						{
							// Special path for common case in_fFocus == 0, especially since ContractInside is more costly.
							memcpy(arFocussedCap, &((*pSpreadCache)[0]), uNbVirtualPoints * sizeof(AKSIMD_V4F32));
						}
						else if (fFocusFactor <= 0.5f)
						{
							ContractPoints<ContractOutside>(
								(AkReal32 *)&((*pSpreadCache)[0]),
								uNbVirtualPoints,
								arFocussedCap,
								fFocusFactor);
						}
						else
						{
							ContractPoints<ContractInside>(
								(AkReal32 *)&((*pSpreadCache)[0]),
								uNbVirtualPoints,
								arFocussedCap,
								fFocusFactor);
						}
					}
					
					// Rotate and copy focussed spherical cap towards each _effectively used_ input channel.
					AkReal32 * arSamples = (AkReal32*)AkAlloca(uNbVirtualPoints * uUsedInputChannels * sizeof(AKSIMD_V4F32));
					CopyRotatedCopiesOntoChannels(uUsedInputChannels, uNbVirtualPoints, arFocussedCap, arSamples);

					// Perform field transformation: rotation and spread.
					arXformedSamples = (AkReal32*)AkAlloca(uNbVirtualPoints * uUsedInputChannels * sizeof(AKSIMD_V4F32));
					TransformSampledSphereSIMD(
						arSamples,
						uNbVirtualPoints * uUsedInputChannels,
						in_emitter,			// Emitter transform
						in_mxListRot,		// Listener rotation
						in_vListPosition,	// Listener position
						in_fSpread,
						arXformedSamples	// Returned transformed vectors
						);
				}
			}
			
			// Pan to std config or encode to ambisonics.
			if (outputConfigNoLfe.eConfigType == AK_ChannelConfigType_Standard)
			{
				//AK_INSTRUMENT_SCOPE("3D VBAP");

				AKASSERT(AK::HasHeightChannels(outputConfigNoLfe.uChannelMask));

				// 3D VBAP
				
				// Center%: optimizing the 0% and 100% cases.
				// "Default" pan table is "no center" unless output config has a center AND center% is 1. 
				// "Alternate" pan table is "center" if output config has a center AND center% is neither 0 or 1, otherwise it is not set.
				AkChannelConfig outputCfgNoCenter = outputConfigNoLfe.RemoveCenter();
				AkUInt32 uNumChannelsDefaultTable = outputConfigNoLfe.uNumChannels;
				void * pPanTable = NULL;
				void * pAlternatePanTable = NULL;
				if (outputConfigNoLfe.uChannelMask & AK_SPEAKER_FRONT_CENTER)
				{
					// Has center.
					if (in_fDivergenceCenter == 1.f)
						pPanTable = in_pDevice->GetPanCache(outputConfigNoLfe);	// "with center"
					else
					{
						pPanTable = in_pDevice->GetPanCache(outputCfgNoCenter);				// "no center"
						--uNumChannelsDefaultTable;
						if (in_fDivergenceCenter != 0.f)
							pAlternatePanTable = in_pDevice->GetPanCache(outputConfigNoLfe);	// "with center". Result is a combination of both.
					}
				}
				else
				{
					pPanTable = in_pDevice->GetPanCache(outputCfgNoCenter);					// "no center"
				}
				if (!pPanTable)
					return;

				size_t uVectorSize = AK::SpeakerVolumes::Vector::GetRequiredSize(outputConfigNoLfe.uNumChannels);
				AkUInt32 uChannelIn = 0;
				AkUInt32 uUsedChannelIn = 0;
				if (pAlternatePanTable == NULL)
				{
					do
					{
						AkUInt32 uMaskAndIdx = ChannelMasksAndIndicesForSpread[uNumInputChannels - 1][uChannelIn];
						if (TAKE_MASK(uMaskAndIdx) & uMaskDesired)
						{
							// Pan input channel:
							AK::SpeakerVolumes::VectorPtr pChanIn = AK::SpeakerVolumes::Matrix::GetChannel(out_pVolumes, TAKE_IDX(uMaskAndIdx), in_outputConfig.uNumChannels);

							// For each input point:
							const AkReal32 * pPoints = arXformedSamples + 4 * uNbVirtualPoints * uUsedChannelIn;
							const AkReal32 * pPointsEnd = pPoints + 4 * uNbVirtualPoints;
							do
							{
								AkVector point;
								point.X = pPoints[0];
								point.Y = pPoints[1];
								point.Z = pPoints[2];
								pPoints += 4;

								AK::SpeakerVolumes::VectorPtr pChanTemp = (AK::SpeakerVolumes::VectorPtr)AkAlloca(uVectorSize);
								AkVBAP::ComputeVBAPSquared(pPanTable, point.Z, -point.X, point.Y, uNumChannelsDefaultTable, pChanTemp);

								// Sum. Skip center channel if needed.
								AkReal32 * AK_RESTRICT pChan = pChanIn;
								const AkReal32 * pEnd = pChan + outputConfigNoLfe.uNumChannels;
								AkReal32 * AK_RESTRICT pTemp = pChanTemp;
								*pChan++ += (*pTemp++);
								*pChan++ += (*pTemp++);
								pChan += (outputConfigNoLfe.uNumChannels - uNumChannelsDefaultTable);	// +1 if output bus has center and VBAP was computed for centerless config (center%=0).
								do
								{
									*pChan++ += (*pTemp++);
								} while (pChan < pEnd);

							} while (pPoints < pPointsEnd);

							AkReal32 fGainSum = AK::SpeakerVolumes::Vector::L1Norm(pChanIn, outputConfigNoLfe.uNumChannels);
							if (fGainSum > 0.f)
							{
								AkReal32 fTotalAttenuationFactor = fOneOverNumChannels / fGainSum;
								AK::SpeakerVolumes::Vector::Mul(pChanIn, fTotalAttenuationFactor, outputConfigNoLfe.uNumChannels);
								AK::SpeakerVolumes::Vector::Sqrt(pChanIn, outputConfigNoLfe.uNumChannels); //convert power back to a gain
							}

							++uUsedChannelIn;
						}
					} while (++uChannelIn < uNumInputChannels);
				}
				else
				{
					// Algorithm with center% weighting (more costly).
					size_t uVectorSizeNoCenter = AK::SpeakerVolumes::Vector::GetRequiredSize(uNumChannelsDefaultTable);

					do
					{
						AkUInt32 uMaskAndIdx = ChannelMasksAndIndicesForSpread[uNumInputChannels - 1][uChannelIn];
						if (TAKE_MASK(uMaskAndIdx) & uMaskDesired)
						{
							AK::SpeakerVolumes::VectorPtr pChanIn = AK::SpeakerVolumes::Matrix::GetChannel(out_pVolumes, TAKE_IDX(uMaskAndIdx), in_outputConfig.uNumChannels);

							// For each input point:
							const AkReal32 * pPoints = arXformedSamples + 4 * uNbVirtualPoints * uUsedChannelIn;
							const AkReal32 * pPointsEnd = pPoints + 4 * uNbVirtualPoints;
							do
							{
								AkVector point;
								point.X = pPoints[0];
								point.Y = pPoints[1];
								point.Z = pPoints[2];
								pPoints += 4;
							
								// No center.
								{
									AK::SpeakerVolumes::VectorPtr pChanTempNoCenter = (AK::SpeakerVolumes::VectorPtr)AkAlloca(uVectorSizeNoCenter);
									AkVBAP::ComputeVBAPSquared(pPanTable, point.Z, -point.X, point.Y, uNumChannelsDefaultTable, pChanTempNoCenter);

									// MADD with divergence. Offset returned channels after center.
									AkReal32 fWeightNoCenter = 1.f - in_fDivergenceCenter;
									AKASSERT(outputConfigNoLfe.uNumChannels > 3);
									AkReal32 * AK_RESTRICT pChan = pChanIn;
									const AkReal32 * pEnd = pChan + outputConfigNoLfe.uNumChannels;
									AkReal32 * AK_RESTRICT pTemp = pChanTempNoCenter;
									*pChan++ += (*pTemp++ * fWeightNoCenter);
									*pChan++ += (*pTemp++ * fWeightNoCenter);
									pChan++;
									do
									{
										*pChan++ += (*pTemp++ * fWeightNoCenter);
									} while (pChan < pEnd);
								}

								// With center.
								{
									AK::SpeakerVolumes::VectorPtr pChanTempWithCenter = (AK::SpeakerVolumes::VectorPtr)AkAlloca(uVectorSize);
									AkVBAP::ComputeVBAPSquared(pAlternatePanTable, point.Z, -point.X, point.Y, outputConfigNoLfe.uNumChannels, pChanTempWithCenter);

									// MADD with divergence. 
									AkReal32 * AK_RESTRICT pChan = pChanIn;
									const AkReal32 * pEnd = pChan + outputConfigNoLfe.uNumChannels;
									AkReal32 * AK_RESTRICT pTemp = pChanTempWithCenter;
									do
									{
										*pChan++ += (*pTemp++ * in_fDivergenceCenter);
									} while (pChan < pEnd);
								}

							} while (pPoints < pPointsEnd);

							AkReal32 fGainSum = AK::SpeakerVolumes::Vector::L1Norm(pChanIn, outputConfigNoLfe.uNumChannels);
							if (fGainSum > 0.f)
							{
								AkReal32 fTotalAttenuationFactor = fOneOverNumChannels / fGainSum;
								AK::SpeakerVolumes::Vector::Mul(pChanIn, fTotalAttenuationFactor, outputConfigNoLfe.uNumChannels);
								AK::SpeakerVolumes::Vector::Sqrt(pChanIn, outputConfigNoLfe.uNumChannels); //convert power back to a gain
							}

							++uUsedChannelIn;
						}

					} while (++uChannelIn < uNumInputChannels);
				}
			}
			else if (outputConfigNoLfe.eConfigType == AK_ChannelConfigType_Ambisonic)
			{
				AkUInt32 order = s_numChanToOrderMinusOne[in_outputConfig.uNumChannels] + 1;
				AkUInt32 uChannelIn = 0;
				AkUInt32 uUsedChannelIn = 0;
				AKASSERT(in_outputConfig.uNumChannels == ((order + 1) * (order + 1)));
				AK::SpeakerVolumes::VectorPtr pHarmonics = (AK::SpeakerVolumes::VectorPtr)AkAlloca(AK::SpeakerVolumes::Vector::GetRequiredSize(in_outputConfig.uNumChannels));
				do
				{
					AkUInt32 uMaskAndIdx = ChannelMasksAndIndicesForSpread[uNumInputChannels - 1][uChannelIn];
					if (TAKE_MASK(uMaskAndIdx) & uMaskDesired)
					{
						AK::SpeakerVolumes::VectorPtr pChanIn = AK::SpeakerVolumes::Matrix::GetChannel(out_pVolumes, TAKE_IDX(uMaskAndIdx), in_outputConfig.uNumChannels);

						// For each input point:
						const AkReal32 * pPoints = arXformedSamples + 4 * uNbVirtualPoints * uUsedChannelIn;
						const AkReal32 * pPointsEnd = pPoints + 4 * uNbVirtualPoints;
						do
						{
							AkVector point;
							point.X = pPoints[0];
							point.Y = pPoints[1];
							point.Z = pPoints[2];
							pPoints += 4;

							// With Ambisonics, amplitude gains are summed instead of power:
							// - virtual sources represent a superposition of plane waves;
							// - negative polarity is possible;
							// - encoding of a single virtual source is not L2-normalized by definition, so adding up power and normalizing prior to converting to amplitude is incorrect.
							// Virtual source contributions are simply divided by the number of virtual sources. As the number of virtual sources coming from all directions (spread=100% in 3D) 
							// tends to infinity, the L1 norm of W should thus converge towards 1, while that of other components tends towards 0.
							
							// convert to spherical harmonic convention
							_ComputeSN3DphericalHarmonics(point.Z, -point.X, point.Y, order, 1, pHarmonics);
							AK::SpeakerVolumes::Vector::Add(pChanIn, pHarmonics, in_outputConfig.uNumChannels);

						} while (pPoints < pPointsEnd);

						// Normalize.
						AkReal32 fTotalAttenuationFactor = fOneOverNumChannels / (uNbVirtualPoints);
						AK::SpeakerVolumes::Vector::Mul(pChanIn, fTotalAttenuationFactor, in_outputConfig.uNumChannels);

						++uUsedChannelIn;
					}
				} while (++uChannelIn < uNumInputChannels);
			}
		}
	}
}

//
// Panning functions.
// 

// 1-position fade.
static inline void _GetSpeakerVolumes2DPan1(
	AkReal32			in_fX,			// [0..1]
	AK::SpeakerVolumes::VectorPtr out_pVolumes,
	AkChannelMask
#ifdef AK_ENABLE_ASSERTS
		in_uOutputConfig
#endif
	)
{
	AKASSERT( in_uOutputConfig == AK_SPEAKER_SETUP_STEREO || in_uOutputConfig == AK_SPEAKER_SETUP_3STEREO );
	out_pVolumes[AK_IDX_SETUP_FRONT_LEFT]	= AkSqrtEstimate( 1.0f - in_fX );
	out_pVolumes[AK_IDX_SETUP_FRONT_RIGHT]	= AkSqrtEstimate( in_fX );
}

// 1-position fade with routing of a portion of the signal (L-R) to center channel.
#ifdef AK_LFECENTER
static inline void _GetSpeakerVolumes2DPan1RouteToCenter(
	AkReal32			in_fX,			// [0..1]
	AkReal32			in_fCenterPct,	// [0..1]
	AK::SpeakerVolumes::VectorPtr out_pVolumes,
	AkChannelMask
#ifdef AK_ENABLE_ASSERTS
		in_uOutputConfig
#endif
	)
{
	AKASSERT( in_uOutputConfig == AK_SPEAKER_SETUP_3STEREO );
	AkReal32 fCenter;
	AkReal32 fRight;
	if ( in_fX <= 0.5f )
	{
		fCenter = 2.f * in_fCenterPct * in_fX;
		fRight = in_fX * ( 1.f - in_fCenterPct );
	}
	else
	{
		fCenter = 2.f * in_fCenterPct * ( 1.f - in_fX );
		fRight = in_fX + in_fCenterPct * ( in_fX - 1.f );
	}
	AkReal32 fLeft = 1.f - fRight - fCenter;

	out_pVolumes[AK_IDX_SETUP_FRONT_LEFT]	= AkSqrtEstimate( fLeft );
	out_pVolumes[AK_IDX_SETUP_FRONT_RIGHT]	= AkSqrtEstimate( fRight );
	out_pVolumes[AK_IDX_SETUP_CENTER]		= AkSqrtEstimate( fCenter );
}
#endif // AK_LFECENTER

#if defined( AK_LFECENTER )
// 1-position fade of source that has a center channel (3-stereo). 
static inline void _GetSpeakerVolumes2DPan1HasCenter(
	AkReal32			in_fX,			// [0..1]
	AK::SpeakerVolumes::VectorPtr out_pVolumes,
	AkChannelMask		
#ifdef AK_ENABLE_ASSERTS
		in_uOutputConfig
#endif
	)
{
	AKASSERT( in_uOutputConfig == AK_SPEAKER_SETUP_3STEREO );

	// Balance left-right.
	AkReal32 fLeft		= ( 2 - 2*in_fX ) / 3.f;
	AkReal32 fCenter	= 1/3.f;
	AkReal32 fRight		= ( 2 * in_fX ) / 3.f;

	out_pVolumes[AK_IDX_SETUP_FRONT_LEFT]	= AkSqrtEstimate( fLeft );
	out_pVolumes[AK_IDX_SETUP_FRONT_RIGHT]	= AkSqrtEstimate( fRight );
	out_pVolumes[AK_IDX_SETUP_CENTER]		= AkSqrtEstimate( fCenter );
}
#endif // AK_LFECENTER

// 2-position fade family.
#ifdef AK_REARCHANNELS

// 2-position fade.
static inline void _GetSpeakerVolumes2DPan2(
	AkReal32			in_fX,			// [0..1]
	AkReal32			in_fY,			// [0..1]
	AK::SpeakerVolumes::VectorPtr out_pVolumes,
	AkChannelMask		in_uOutputConfig
	)
{
	AKASSERT( in_uOutputConfig == AK_SPEAKER_SETUP_4 || in_uOutputConfig == AK_SPEAKER_SETUP_5 );

	AkReal32 fLeft = 1.f - in_fX;
	out_pVolumes[AK_IDX_SETUP_FRONT_LEFT] = AkSqrtEstimate( fLeft * in_fY );
	out_pVolumes[AK_IDX_SETUP_FRONT_RIGHT] = AkSqrtEstimate( in_fX * in_fY );	
	AkUInt32 uIdxOffset = 0;
#if defined( AK_LFECENTER )
	if ( in_uOutputConfig & AK_SPEAKER_FRONT_CENTER )
	{
		out_pVolumes[AK_IDX_SETUP_CENTER] = 0.f;
		uIdxOffset = 1;
	}
#endif // AK_LFECENTER

	AkReal32 fRearBalance = ( 1.0f - in_fY );
	out_pVolumes[AK_IDX_SETUP_NOCENTER_BACK_LEFT+uIdxOffset]	= AkSqrtEstimate( fLeft * fRearBalance );
	out_pVolumes[AK_IDX_SETUP_NOCENTER_BACK_RIGHT+uIdxOffset]	= AkSqrtEstimate( in_fX * fRearBalance );
}

#if defined( AK_LFECENTER )
// 2-position fade with routing of a portion of the signal (FL-FR) to center channel.
static inline void _GetSpeakerVolumes2DPan2RouteToCenter(
	AkReal32			in_fX,			// [0..1]
	AkReal32			in_fY,			// [0..1]
	AkReal32			in_fCenterPct,	// [0..1]
	AK::SpeakerVolumes::VectorPtr out_pVolumes,
	AkChannelMask
#ifdef AK_ENABLE_ASSERTS
		in_uOutputConfig
#endif
	)
{
	AKASSERT( in_uOutputConfig == AK_SPEAKER_SETUP_5 );
	AkReal32 fCenter;
	AkReal32 fRight;
	if ( in_fX <= 0.5f )
	{
		fCenter = 2.f * in_fCenterPct * in_fX;
		fRight = in_fX * ( 1.f - in_fCenterPct );
	}
	else
	{
		fCenter = 2.f * in_fCenterPct * ( 1.f - in_fX );
		fRight = in_fX + in_fCenterPct * ( in_fX - 1.f );
	}
	AkReal32 fLeft = 1.f - fRight - fCenter;

	out_pVolumes[AK_IDX_SETUP_FRONT_LEFT]	= AkSqrtEstimate( fLeft * in_fY );
	out_pVolumes[AK_IDX_SETUP_FRONT_RIGHT]	= AkSqrtEstimate( fRight * in_fY );
	out_pVolumes[AK_IDX_SETUP_CENTER]		= AkSqrtEstimate( fCenter * in_fY );

	AkReal32 fRearBalance = ( 1.0f - in_fY );
	out_pVolumes[AK_IDX_SETUP_WITHCENTER_BACK_LEFT]		= AkSqrtEstimate( ( 1.0f - in_fX ) * fRearBalance );
	out_pVolumes[AK_IDX_SETUP_WITHCENTER_BACK_RIGHT]	= AkSqrtEstimate( in_fX * fRearBalance );
}
#endif

// 2-position fade of source that has a center channel (3-stereo). 
static inline void _GetSpeakerVolumes2DPan2HasCenter(
	AkReal32			in_fX,			// [0..1]
	AkReal32			in_fY,			// [0..1]
	AK::SpeakerVolumes::VectorPtr out_pVolumes,
	AkChannelMask
#ifdef AK_ENABLE_ASSERTS
		in_uOutputConfig
#endif
	)
{
	AKASSERT( in_uOutputConfig == AK_SPEAKER_SETUP_5 );

	// Balance left-right.
	AkReal32 fFrontLeft = ( 2 - 2*in_fX ) / 3.f;
	AkReal32 fFrontCenter = 1/3.f;
	AkReal32 fFrontRight = ( 2 * in_fX ) / 3.f;
	AkReal32 fRearRight = in_fX;
	AkReal32 fRearLeft = 1 - in_fX;

	// Balance front-rear.
	// Note: Because our pad is square but the channels are not symmetric, the linear interpolation
	// has a different slope whether it is in the top or bottom half. At y = 0.5, power is evenly distributed.
	AkReal32 fRearBalance = ( in_fY >= 0.5f ) ? ( ( 4 - 4 * in_fY ) / 5.f ) : ( 1 - 6 * in_fY / 5.f );
	AkReal32 fFrontBalance = 1 - fRearBalance;

	fFrontLeft *= fFrontBalance;
	fFrontCenter *= fFrontBalance;
	fFrontRight *= fFrontBalance;	

	fRearLeft *= fRearBalance;
	fRearRight *= fRearBalance;

	AKASSERT( fFrontLeft + fFrontCenter + fFrontRight + fRearLeft + fRearRight > 1 - 0.00001 
			&& fFrontLeft + fFrontCenter + fFrontRight + fRearLeft + fRearRight < 1 + 0.00001 );

	//convert power to speaker gains
	out_pVolumes[AK_IDX_SETUP_FRONT_LEFT]				= AkSqrtEstimate( fFrontLeft );
	out_pVolumes[AK_IDX_SETUP_FRONT_RIGHT]				= AkSqrtEstimate( fFrontRight );
	out_pVolumes[AK_IDX_SETUP_CENTER]					= AkSqrtEstimate( fFrontCenter );
	out_pVolumes[AK_IDX_SETUP_WITHCENTER_BACK_LEFT]		= AkSqrtEstimate( fRearLeft );
	out_pVolumes[AK_IDX_SETUP_WITHCENTER_BACK_RIGHT]	= AkSqrtEstimate( fRearRight );
}
#endif	// AK_REARCHANNELS	// End of 2-position fade family.


// 2D panning rules extended for 6/7.1 (sides and backs) (3-position fade). 
#ifdef AK_71AUDIO
// 3-position fade.
static inline void _GetSpeakerVolumes2DPan3(
	AkReal32			in_fX,			// [0..1]
	AkReal32			in_fY,			// [0..1]
	AK::SpeakerVolumes::VectorPtr out_pVolumes,
	AkChannelMask		in_uOutputConfig
	)
{
	AKASSERT( in_uOutputConfig == AK_SPEAKER_SETUP_6 || in_uOutputConfig == AK_SPEAKER_SETUP_7 );

	// Balance front channels: center + left-right
	AkReal32 fFrontRight = in_fX;
	AkReal32 fFrontLeft = 1.f - fFrontRight;

	// Balance surround channels (side and back pairs).
	AkReal32 fSurroundRight = in_fX;
	AkReal32 fSurroundLeft = 1.f - in_fX;

	// Balance front-rear.
	AkReal32 fFrontBalance = ( 4.f * in_fY - 1.f ) / 3.f;
	if ( fFrontBalance < 0.f ) fFrontBalance = 0.f;
	AkReal32 fBackBalance = ( 3.f - 4.f * in_fY ) / 3.f;
	if ( fBackBalance < 0.f ) fBackBalance = 0.f;
	AkReal32 fSideBalance = 1 - ( fFrontBalance + fBackBalance );

	AKASSERT( fFrontLeft * fFrontBalance + fFrontRight * fFrontBalance + fSurroundLeft * fBackBalance + fSurroundLeft * fSideBalance + fSurroundRight * fBackBalance + fSurroundRight * fSideBalance > 1 - 0.00001f 
			&& fFrontLeft * fFrontBalance + fFrontRight * fFrontBalance + fSurroundLeft * fBackBalance + fSurroundLeft * fSideBalance + fSurroundRight * fBackBalance + fSurroundRight * fSideBalance < 1 + 0.00001f );

	//convert power to speaker gains
	out_pVolumes[AK_IDX_SETUP_FRONT_LEFT]	= AkSqrtEstimate( fFrontLeft * fFrontBalance );
	out_pVolumes[AK_IDX_SETUP_FRONT_RIGHT]	= AkSqrtEstimate( fFrontRight * fFrontBalance );
	AkUInt32 uIdxOffset = 0;
	if ( in_uOutputConfig & AK_SPEAKER_FRONT_CENTER )
	{
		out_pVolumes[AK_IDX_SETUP_CENTER] = 0.f;
		uIdxOffset = 1;
	}
	out_pVolumes[AK_IDX_SETUP_NOCENTER_BACK_LEFT+uIdxOffset]	= AkSqrtEstimate( fSurroundLeft * fBackBalance );
	out_pVolumes[AK_IDX_SETUP_NOCENTER_BACK_RIGHT+uIdxOffset]	= AkSqrtEstimate( fSurroundRight * fBackBalance );
	out_pVolumes[AK_IDX_SETUP_NOCENTER_SIDE_LEFT+uIdxOffset]	= AkSqrtEstimate( fSurroundLeft * fSideBalance );
	out_pVolumes[AK_IDX_SETUP_NOCENTER_SIDE_RIGHT+uIdxOffset]	= AkSqrtEstimate( fSurroundRight * fSideBalance );
}

// 3-position fade with routing of a portion of the signal (FL-FR) to center channel.
static inline void _GetSpeakerVolumes2DPan3RouteToCenter(
	AkReal32			in_fX,			// [0..1]
	AkReal32			in_fY,			// [0..1]
	AkReal32			in_fCenterPct,	// [0..1]
	AK::SpeakerVolumes::VectorPtr out_pVolumes,
	AkChannelMask
#ifdef AK_ENABLE_ASSERTS
		in_uOutputConfig
#endif
	)
{
	AKASSERT( in_uOutputConfig == AK_SPEAKER_SETUP_7 );

	// Balance front channels: center + left-right
	AkReal32 fFrontCenter;
	AkReal32 fFrontRight;
	if ( in_fX <= 0.5f )
	{
		fFrontCenter = 2.f * in_fCenterPct * in_fX;
		fFrontRight = in_fX * ( 1.f - in_fCenterPct );
	}
	else
	{
		fFrontCenter = 2.f * in_fCenterPct * ( 1.f - in_fX );
		fFrontRight = in_fX + in_fCenterPct * ( in_fX - 1.f );
	}
	AkReal32 fFrontLeft = 1.f - fFrontRight - fFrontCenter;

	// Balance surround channels (side and back pairs).
	AkReal32 fSurroundRight = in_fX;
	AkReal32 fSurroundLeft = 1.f - in_fX;

	// Balance front-rear.
	AkReal32 fFrontBalance = ( 4.f * in_fY - 1.f ) / 3.f;
	if ( fFrontBalance < 0.f ) fFrontBalance = 0.f;
	AkReal32 fBackBalance = ( 3.f - 4.f * in_fY ) / 3.f;
	if ( fBackBalance < 0.f ) fBackBalance = 0.f;
	AkReal32 fSideBalance = 1 - ( fFrontBalance + fBackBalance );

	AKASSERT( fFrontLeft * fFrontBalance + fFrontRight * fFrontBalance + fFrontCenter * fFrontBalance + fSurroundLeft * fBackBalance + fSurroundLeft * fSideBalance + fSurroundRight * fBackBalance + fSurroundRight * fSideBalance > 1 - 0.00001f 
			&& fFrontLeft * fFrontBalance + fFrontRight * fFrontBalance + fFrontCenter * fFrontBalance + fSurroundLeft * fBackBalance + fSurroundLeft * fSideBalance + fSurroundRight * fBackBalance + fSurroundRight * fSideBalance < 1 + 0.00001f );

	//convert power to speaker gains
	out_pVolumes[AK_IDX_SETUP_FRONT_LEFT]			= AkSqrtEstimate( fFrontLeft * fFrontBalance );
	out_pVolumes[AK_IDX_SETUP_FRONT_RIGHT]			= AkSqrtEstimate( fFrontRight * fFrontBalance );
	out_pVolumes[AK_IDX_SETUP_CENTER]				= AkSqrtEstimate( fFrontCenter * fFrontBalance );
	out_pVolumes[AK_IDX_SETUP_WITHCENTER_BACK_LEFT] = AkSqrtEstimate( fSurroundLeft * fBackBalance );
	out_pVolumes[AK_IDX_SETUP_WITHCENTER_BACK_RIGHT]= AkSqrtEstimate( fSurroundRight * fBackBalance );
	out_pVolumes[AK_IDX_SETUP_WITHCENTER_SIDE_LEFT] = AkSqrtEstimate( fSurroundLeft * fSideBalance );
	out_pVolumes[AK_IDX_SETUP_WITHCENTER_SIDE_RIGHT]= AkSqrtEstimate( fSurroundRight * fSideBalance );
}

// 3-position fade with front center channel present in both the input and the output.
static inline void _GetSpeakerVolumes2DPan3HasCenter(
	AkReal32			in_fX,			// [0..1]
	AkReal32			in_fY,			// [0..1]
	AK::SpeakerVolumes::VectorPtr out_pVolumes,
	AkChannelMask		
#ifdef AK_ENABLE_ASSERTS
		in_uOutputConfig
#endif
	)
{
	AKASSERT( in_uOutputConfig == AK_SPEAKER_SETUP_7 );

	// Balance left-right.
	AkReal32 fFrontLeft = ( 2.f - 2.f * in_fX )/3.f;
	AkReal32 fFrontCenter = 1.f / 3.f;
	AkReal32 fFrontRight = 1.f - ( fFrontLeft + fFrontCenter );
	if (fFrontRight < 0.f) fFrontRight = 0.f;

	AkReal32 fRearRight = in_fX;
	AkReal32 fRearLeft = 1.f - in_fX;

	// Balance front-rear.
	// Note: Because our pad is square but the number of channels is not symmetric in the front and in the back, 
	// the linear interpolation has a different slope whether it is in the top or bottom half. 
	// At y = 0.5, power is evenly distributed.
	AkReal32 fFrontBalance = ( 8.f * in_fY - 1.f ) / 7.f;
	if ( fFrontBalance < 0.f ) fFrontBalance = 0.f;
	AkReal32 fBackBalance = ( 7.f - 10.f * in_fY ) / 7.f;
	if ( fBackBalance < 0.f ) fBackBalance = 0.f;
	AkReal32 fSideBalance = 1.f - ( fFrontBalance + fBackBalance );

	//convert power to speaker gains
	out_pVolumes[AK_IDX_SETUP_FRONT_LEFT]			= AkSqrtEstimate( fFrontLeft * fFrontBalance );
	out_pVolumes[AK_IDX_SETUP_FRONT_RIGHT]			= AkSqrtEstimate( fFrontRight * fFrontBalance );
	out_pVolumes[AK_IDX_SETUP_CENTER]				= AkSqrtEstimate( fFrontCenter * fFrontBalance );
	out_pVolumes[AK_IDX_SETUP_WITHCENTER_BACK_LEFT] = AkSqrtEstimate( fRearLeft * fBackBalance );
	out_pVolumes[AK_IDX_SETUP_WITHCENTER_BACK_RIGHT]= AkSqrtEstimate( fRearRight * fBackBalance );
	out_pVolumes[AK_IDX_SETUP_WITHCENTER_SIDE_LEFT] = AkSqrtEstimate( fRearLeft * fSideBalance );
	out_pVolumes[AK_IDX_SETUP_WITHCENTER_SIDE_RIGHT]= AkSqrtEstimate( fRearRight * fSideBalance );
}
#endif // AK_71AUDIO	// End of 3-position fade family

static inline void HandleLfe( 
	AkChannelConfig		in_inputConfig,
	AkChannelConfig		in_outputConfig,
	AK::SpeakerVolumes::MatrixPtr out_pVolumes
	)
{
#ifdef AK_LFECENTER
	// LFE.
	if ( in_inputConfig.HasLFE() )
	{
		// LFE always last
		AK::SpeakerVolumes::VectorPtr pLfeIn = AK::SpeakerVolumes::Matrix::GetChannel( out_pVolumes, in_inputConfig.uNumChannels-1, in_outputConfig.uNumChannels );
		AK::SpeakerVolumes::Vector::Zero( pLfeIn, in_outputConfig.uNumChannels );
		if ( in_outputConfig.HasLFE() )
			pLfeIn[ in_outputConfig.uNumChannels - 1 ] = 1.f;
	}
#endif
}

static inline void GetSpeakerVolumesDirectStd(
	AkChannelConfig		in_inputConfig,
	AkChannelConfig		in_inputConfigNoLfe,	// Pass in in_inputConfig with LFE stripped out.
	AkChannelConfig		in_outputConfig,
	AkChannelConfig		in_outputConfigNoLfe,	// Pass in in_outputConfig with LFE stripped out.
	AkReal32			in_fCenterPct,
	AK::SpeakerVolumes::MatrixPtr out_pVolumes
	)
{
	AKASSERT( in_inputConfigNoLfe == in_inputConfig.RemoveLFE() 
			&& in_outputConfigNoLfe == in_outputConfig.RemoveLFE() );

	// Handle special mono input case separately for center% support.
	if ( in_inputConfigNoLfe.uNumChannels == 1 )
	{
		AK::SpeakerVolumes::VectorPtr pVolumes = AK::SpeakerVolumes::Matrix::GetChannel( out_pVolumes, 0, in_outputConfig.uNumChannels );
		
		// Zero-out in case there is an LFE, or other unhandled channels.
#ifdef AK_LFECENTER
		AK::SpeakerVolumes::Vector::Zero( pVolumes, in_outputConfig.uNumChannels );
#endif
		if ( in_outputConfigNoLfe.uNumChannels > 1 )
		{
			// Output is multichannel.
#ifdef AK_LFECENTER
			// Ignore center% if output config does not have 3 front channels.
			if ( ( in_outputConfigNoLfe.uChannelMask & AK_SPEAKER_SETUP_3STEREO ) == AK_SPEAKER_SETUP_3STEREO )
			{
				// Handle mono case with center %.
				// Note: Mono->mono case does not need to be handled separately. "To mono" downmix is such that 
				// we may freely swap fCenter = 1 with constant power distribution across the front channels.
				AkReal32 fSides = AkSqrtEstimate( (1.f - in_fCenterPct) * 0.5f );
				pVolumes[0] = fSides;
				pVolumes[1] = fSides;
				pVolumes[2] = AkSqrtEstimate( in_fCenterPct );
			}
			else
#endif
			{
				// Cannot have multichannel bus without front left-right
				AKASSERT( in_outputConfigNoLfe.uChannelMask & AK_SPEAKER_SETUP_STEREO );
				// No center; we express mono signal paths by distributing power equally onto left and right
				// channels. This is perfectly invertible with "to mono" AC3 downmix recipe with -3dB normalization.
				pVolumes[0] = ONE_OVER_SQRT_OF_TWO;
				pVolumes[1] = ONE_OVER_SQRT_OF_TWO;
			}
		}
		else
		{
			// Output is mono.
			pVolumes[0] = 1.f;
		}

		HandleLfe( in_inputConfig, in_outputConfig, out_pVolumes );
	}
	else
	{
		// Multichannel input.
		// Make identity matrix, apply downmix rules.

		/// LX OPTI Consider optimized path if input config is equal or is a subset of the output config.

		// Compute panning on fullband channels only.		
		AkDownmix::ComputeDirectSpeakerAssignment( 
			in_inputConfigNoLfe,			// Input configuration.
			in_outputConfigNoLfe,			// Desired output configuration with LFE stripped out.
			in_outputConfig.uNumChannels,	// Number of output channels in desired output configuration. May differ from in_configOutNoLFE because of LFE. Required to properly address in_pOutputMx.
			out_pVolumes					// Returned volume matrix.
			);
	}

	// Handle LFE.
	HandleLfe( in_inputConfig, in_outputConfig, out_pVolumes );
}

// Makes identity matrix to up to min(numchannels_in,numchannels_out), zeros the additional rows/columns.
void CAkSpeakerPan::GetSpeakerVolumesDirectNoMask(
	AkChannelConfig		in_inputConfig,
	AkChannelConfig		in_outputConfig,
	AK::SpeakerVolumes::MatrixPtr out_pVolumes
	)
{
	AK::SpeakerVolumes::Matrix::Zero( out_pVolumes, in_inputConfig.uNumChannels, in_outputConfig.uNumChannels );
	if (in_inputConfig.eConfigType == AK_ChannelConfigType_Ambisonic)
	{
		AkUInt32 uChanIn = 0;
		for (AkUInt32 uChanOut = 0; uChanOut < in_outputConfig.uNumChannels && uChanIn < in_inputConfig.uNumChannels; uChanOut++)
		{
			// Skip harmonics for incomplete channel configs.
			if (SkipHarmonic(in_inputConfig.uNumChannels, uChanOut))
				continue;

			AK::SpeakerVolumes::VectorPtr pChanIn = AK::SpeakerVolumes::Matrix::GetChannel(out_pVolumes, uChanIn, in_outputConfig.uNumChannels);
			pChanIn[uChanOut] = 1.f;
			++uChanIn;
		}
	}
	else
	{
		AkUInt32 uNumChannels = AkMin(in_inputConfig.uNumChannels, in_outputConfig.uNumChannels);
		for (AkUInt32 uChan = 0; uChan < uNumChannels; uChan++)
		{
			AK::SpeakerVolumes::VectorPtr pChanIn = AK::SpeakerVolumes::Matrix::GetChannel(out_pVolumes, uChan, in_outputConfig.uNumChannels);
			pChanIn[uChan] = 1.f;
		}
	}
}

template< class Contract >
void CAkSpeakerPan::ContractPoints(const AkReal32* AK_RESTRICT in_points, AkUInt32 in_numPoints, AkReal32* AK_RESTRICT out_points, AkReal32 in_spread)
{
	Contract contract;
	contract.Setup(in_spread);

	AKASSERT((in_numPoints % 4) == 0);
	const AkReal32* AK_RESTRICT pPointsEnd = in_points + 4 * in_numPoints;
	do
	{
		// Load 4 vectors, and group the x y and z
		AKSIMD_V4F32 v0 = AKSIMD_LOAD_V4F32(in_points);
		AKSIMD_V4F32 v1 = AKSIMD_LOAD_V4F32(in_points + 4);
		AKSIMD_V4F32 v2 = AKSIMD_LOAD_V4F32(in_points + 8);
		AKSIMD_V4F32 v3 = AKSIMD_LOAD_V4F32(in_points + 12);

		in_points += 16;

		AKSIMD_V4F32 xxxx, yyyy, zzzz;
		AkMath::PermuteVectors3(v0, v1, v2, v3, xxxx, yyyy, zzzz);
		contract.Contract(xxxx, yyyy, zzzz);

		AkMath::UnpermuteVectors3(xxxx, yyyy, zzzz, v0, v1, v2, v3);
		AKSIMD_STORE_V4F32(out_points, v0);
		AKSIMD_STORE_V4F32(out_points + 4, v1);
		AKSIMD_STORE_V4F32(out_points + 8, v2);
		AKSIMD_STORE_V4F32(out_points + 12, v3);

		out_points += 16;

	} while (in_points < pPointsEnd);
}

template< class Contract >
void CAkSpeakerPan::ContractPointsScalar(
	const AkReal32* AK_RESTRICT in_points,
	AkUInt32 in_numPoints,
	AkReal32* AK_RESTRICT out_points,
	AkReal32 in_spread)
{
	Contract contract;
	contract.Setup(in_spread);

	const AkReal32* AK_RESTRICT pPointsEnd = in_points + 4 * in_numPoints;
	do
	{
		// Load 4 vectors, and group the x y and z
		AkReal32 x = *in_points++;
		AkReal32 y = *in_points++;
		AkReal32 z = *in_points++;
		in_points++;

		contract.Contract(x, y, z);

		*out_points++ = x;
		*out_points++ = y;
		*out_points++ = z;
		out_points++;
	} while (in_points < pPointsEnd);
}



void CAkSpeakerPan::GetSpeakerVolumesDirect(
	AkChannelConfig		in_inputConfig,
	AkChannelConfig		in_outputConfig,
	AkReal32			in_fCenterPerc,
	AK::SpeakerVolumes::MatrixPtr out_pVolumes
	)
{
	GetSpeakerVolumes2DPan(0.f, 0.f, in_fCenterPerc, AK_DirectSpeakerAssignment, in_inputConfig, in_outputConfig, out_pVolumes, NULL);
}

static void GetSpeakerVolumes2DPanStd(
	AkReal32			in_fX,			// [0..1]
	AkReal32			in_fY,			// [0..1]
	AkReal32			in_fCenterPct,	// [0..1]
	AkSpeakerPanningType	in_ePannerType,
	AkChannelConfig		in_inputConfig,
	AkChannelConfig		in_outputConfig,
	AK::SpeakerVolumes::MatrixPtr out_pVolumes
	)
{
	AkChannelConfig configInNoLfe = in_inputConfig.RemoveLFE();
	AkChannelConfig configOutNoLfe = in_outputConfig.RemoveLFE();

	if (in_ePannerType == AK_DirectSpeakerAssignment || configOutNoLfe.uNumChannels == 1)
	{
		// Direct speaker assignment (or we are routed to a mono bus, in which case 2D panning is useless).
		GetSpeakerVolumesDirectStd( in_inputConfig, configInNoLfe, in_outputConfig, configOutNoLfe, in_fCenterPct, out_pVolumes );
	}
	else
	{
		// Handle mono input case separately.
		if ( configInNoLfe.uNumChannels == 1 )
		{
			// 2D panning of a mono input:
			// Result of _GetSpeakerVolumes2DPan() is used directly as the distribution of our mono input to each output channel.

			AK::SpeakerVolumes::VectorPtr pVolumes = AK::SpeakerVolumes::Matrix::GetChannel( out_pVolumes, 0, in_outputConfig.uNumChannels );
#ifdef AK_LFECENTER
			AK::SpeakerVolumes::Vector::Zero( pVolumes, in_outputConfig.uNumChannels );
#endif

#ifndef AK_71AUDIO
			AkChannelMask uMaskPlaneOut2D = configOutNoLfe.uChannelMask;	// No 7.1: no extended configs,
#else
			// Handle extended output configs
			AkChannelMask uMaskPlaneOut2D = configOutNoLfe.uChannelMask & AK_SPEAKER_SETUP_DEFAULT_PLANE;

#ifdef AK_LFECENTER
			if(uMaskPlaneOut2D == 0)
			{
				// WG-34210
				// No channels eligible for 2D panning. Bail out now.
				AK::SpeakerVolumes::Matrix::Zero(out_pVolumes, in_inputConfig.uNumChannels, in_outputConfig.uNumChannels);
				HandleLfe(in_inputConfig, in_outputConfig, out_pVolumes);
				return;
			}
#endif
			if ( uMaskPlaneOut2D == AK_SPEAKER_SETUP_7 )
				_GetSpeakerVolumes2DPan3RouteToCenter( in_fX, in_fY, in_fCenterPct, pVolumes, uMaskPlaneOut2D );
			else if ( uMaskPlaneOut2D == AK_SPEAKER_SETUP_6 )
				_GetSpeakerVolumes2DPan3( in_fX, in_fY, pVolumes, uMaskPlaneOut2D );
			else 
#endif
#ifdef AK_REARCHANNELS
#ifdef AK_LFECENTER
			if ( uMaskPlaneOut2D == AK_SPEAKER_SETUP_5 )
				_GetSpeakerVolumes2DPan2RouteToCenter( in_fX, in_fY, in_fCenterPct, pVolumes, uMaskPlaneOut2D );
			else 
#endif
			if ( uMaskPlaneOut2D == AK_SPEAKER_SETUP_4 )
				_GetSpeakerVolumes2DPan2( in_fX, in_fY, pVolumes, uMaskPlaneOut2D );
			else 
#endif
#ifdef AK_LFECENTER
			if ( uMaskPlaneOut2D == AK_SPEAKER_SETUP_3STEREO )
				_GetSpeakerVolumes2DPan1RouteToCenter( in_fX, in_fCenterPct, pVolumes, uMaskPlaneOut2D );
			else
#endif
				_GetSpeakerVolumes2DPan1( in_fX, pVolumes, uMaskPlaneOut2D );
		}
		else
		{
			// 2D panning of a multichannel input.

			/// LX TODO Pan height layer?
			// TODO Handle input and output configs with BACK CENTER. 
#ifdef AK_71AUDIO
			AkChannelMask uInputMask2D = configInNoLfe.uChannelMask & AK_SPEAKER_SETUP_DEFAULT_PLANE;
			uInputMask2D = AK::BackToSideChannels( uInputMask2D );	// Fix "back only" to sides.
			AkChannelMask uOutputMask2D = configOutNoLfe.uChannelMask & AK_SPEAKER_SETUP_DEFAULT_PLANE;
#else
			AkChannelMask uInputMask2D = configInNoLfe.uChannelMask;
			AkChannelMask uOutputMask2D = configOutNoLfe.uChannelMask;
#endif
#ifdef AK_LFECENTER
			if ( uInputMask2D == 0 )
			{
				/// LX TEST this path. Is it used?
				// No channels eligible for 2D panning. Bail out now.
				AK::SpeakerVolumes::Matrix::Zero( out_pVolumes, in_inputConfig.uNumChannels, in_outputConfig.uNumChannels );
				HandleLfe( in_inputConfig, in_outputConfig, out_pVolumes );
				return;
			}
#endif
			AkUInt32 uOredInOutMask = uInputMask2D | uOutputMask2D;

			// Prepare matrix IN x IN|OUT for routing of input channels.
			/// LX OPTI stereo platforms?
			AkChannelConfig configIn2D;
			configIn2D.SetStandard( uInputMask2D );
			AkChannelConfig configPanInOut;
			configPanInOut.SetStandard( uOredInOutMask );
			AkUInt32 uRoutingMxSize = AK::SpeakerVolumes::Matrix::GetRequiredSize( configIn2D.uNumChannels, configPanInOut.uNumChannels );
			AK::SpeakerVolumes::MatrixPtr pRoutingMx = (AK::SpeakerVolumes::MatrixPtr)AkAlloca( uRoutingMxSize );
			AK::SpeakerVolumes::Matrix::Zero( pRoutingMx, configIn2D.uNumChannels, configPanInOut.uNumChannels );

			// Prepare vector of panning weights.
			AkUInt32 uPanningVectorSize = AK::SpeakerVolumes::Vector::GetRequiredSize( configPanInOut.uNumChannels );
			AK::SpeakerVolumes::VectorPtr pPanningWeights = (AK::SpeakerVolumes::VectorPtr)AkAlloca( uPanningVectorSize );

			/// LX REVIEW Required to handle higher-order configurations, and LFE (by silencing them out).
			AK::SpeakerVolumes::Vector::Zero( pPanningWeights, uPanningVectorSize / sizeof(AkReal32) );

			// Route input channels into output channels according to in/in-out configuration.
#ifdef AK_REARCHANNELS
			AkUInt32 uIdxInOutBackOffset = ( uOredInOutMask & AK_SPEAKER_FRONT_CENTER ) >> 2;
#endif
			switch ( uInputMask2D )
			{
				case AK_SPEAKER_SETUP_STEREO:
				{
					AK::SpeakerVolumes::VectorPtr pChanL = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_FRONT_LEFT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanR = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_FRONT_RIGHT, configPanInOut.uNumChannels ); 

					// Bleed L and R to corresponding rear and side channels.
					pChanL[ AK_IDX_SETUP_FRONT_LEFT ] = 1.f;
					pChanR[ AK_IDX_SETUP_FRONT_RIGHT ] = 1.f;
#ifdef AK_71AUDIO
					if ( uOredInOutMask == AK_SPEAKER_SETUP_7 || uOredInOutMask == AK_SPEAKER_SETUP_6 )
					{
						pChanL[ AK_IDX_SETUP_NOCENTER_BACK_LEFT + uIdxInOutBackOffset ] = 1.f;
						pChanL[ AK_IDX_SETUP_NOCENTER_SIDE_LEFT + uIdxInOutBackOffset ] = 1.f;
						
						pChanR[ AK_IDX_SETUP_NOCENTER_BACK_RIGHT + uIdxInOutBackOffset ] = 1.f;
						pChanR[ AK_IDX_SETUP_NOCENTER_SIDE_RIGHT + uIdxInOutBackOffset ] = 1.f;

						_GetSpeakerVolumes2DPan3( in_fX, in_fY, (AkReal32*)pPanningWeights, uOredInOutMask );
					}
					else 
#endif
#ifdef AK_REARCHANNELS
					if ( uOredInOutMask == AK_SPEAKER_SETUP_5 || uOredInOutMask == AK_SPEAKER_SETUP_4 )
					{
						pChanL[ AK_IDX_SETUP_NOCENTER_BACK_LEFT + uIdxInOutBackOffset ] = 1.f;
						
						pChanR[ AK_IDX_SETUP_NOCENTER_BACK_RIGHT + uIdxInOutBackOffset ] = 1.f;

						_GetSpeakerVolumes2DPan2( in_fX, in_fY, (AkReal32*)pPanningWeights, uOredInOutMask );
					}
					else
#endif
					{
						_GetSpeakerVolumes2DPan1( in_fX, (AkReal32*)pPanningWeights, uOredInOutMask );
					}
				}
				break;
#ifdef AK_LFECENTER
				case AK_SPEAKER_SETUP_3STEREO:
				{
					AK::SpeakerVolumes::VectorPtr pChanL = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_FRONT_LEFT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanR = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_FRONT_RIGHT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanC = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_CENTER, configPanInOut.uNumChannels ); 

					// Bleed L and R to corresponding rear and side channels, and C in both
					pChanL[ AK_IDX_SETUP_FRONT_LEFT ] = 1.f;
					pChanR[ AK_IDX_SETUP_FRONT_RIGHT ] = 1.f;
					pChanC[ AK_IDX_SETUP_CENTER ] = 1.f;
#ifdef AK_71AUDIO
					if ( uOredInOutMask == AK_SPEAKER_SETUP_7 || uOredInOutMask == AK_SPEAKER_SETUP_6 )
					{
						pChanL[ AK_IDX_SETUP_NOCENTER_BACK_LEFT + uIdxInOutBackOffset ] = 1.f;
						pChanL[ AK_IDX_SETUP_NOCENTER_SIDE_LEFT + uIdxInOutBackOffset ] = 1.f;
						
						pChanR[ AK_IDX_SETUP_NOCENTER_BACK_RIGHT + uIdxInOutBackOffset ] = 1.f;
						pChanR[ AK_IDX_SETUP_NOCENTER_SIDE_RIGHT + uIdxInOutBackOffset ] = 1.f;

						pChanC[ AK_IDX_SETUP_NOCENTER_BACK_LEFT + uIdxInOutBackOffset ] = 0.5f;
						pChanC[ AK_IDX_SETUP_NOCENTER_SIDE_LEFT + uIdxInOutBackOffset ] = 0.5f;
						pChanC[ AK_IDX_SETUP_NOCENTER_BACK_RIGHT + uIdxInOutBackOffset ] = 0.5f;
						pChanC[ AK_IDX_SETUP_NOCENTER_SIDE_RIGHT + uIdxInOutBackOffset ] = 0.5f;

						_GetSpeakerVolumes2DPan3HasCenter( in_fX, in_fY, (AkReal32*)pPanningWeights, uOredInOutMask );
					}
					else 
#endif
#ifdef AK_REARCHANNELS
					if ( uOredInOutMask == AK_SPEAKER_SETUP_5 || uOredInOutMask == AK_SPEAKER_SETUP_4 )
					{
						pChanL[ AK_IDX_SETUP_NOCENTER_BACK_LEFT + uIdxInOutBackOffset ] = 1.f;
						
						pChanR[ AK_IDX_SETUP_NOCENTER_BACK_RIGHT + uIdxInOutBackOffset ] = 1.f;

						pChanC[ AK_IDX_SETUP_NOCENTER_BACK_LEFT + uIdxInOutBackOffset ] = 0.5f;
						pChanC[ AK_IDX_SETUP_NOCENTER_BACK_RIGHT + uIdxInOutBackOffset ] = 0.5f;

						_GetSpeakerVolumes2DPan2HasCenter( in_fX, in_fY, (AkReal32*)pPanningWeights, uOredInOutMask );
					}
					else
#endif
					{
						_GetSpeakerVolumes2DPan1HasCenter( in_fX, (AkReal32*)pPanningWeights, uOredInOutMask );
					}
				}
				break;
#endif
#ifdef AK_REARCHANNELS
				case AK_SPEAKER_SETUP_4:
				{
					AK::SpeakerVolumes::VectorPtr pChanL = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_FRONT_LEFT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanR = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_FRONT_RIGHT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanBL = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_NOCENTER_BACK_LEFT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanBR = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_NOCENTER_BACK_RIGHT, configPanInOut.uNumChannels );

					pChanL[ AK_IDX_SETUP_FRONT_LEFT ] = 1.f;
					pChanR[ AK_IDX_SETUP_FRONT_RIGHT ] =1.f;
					pChanBL[ AK_IDX_SETUP_NOCENTER_BACK_LEFT + uIdxInOutBackOffset ] = 1.f;
					pChanBR[ AK_IDX_SETUP_NOCENTER_BACK_RIGHT + uIdxInOutBackOffset ] = 1.f;
#ifdef AK_71AUDIO
					if ( uOredInOutMask == AK_SPEAKER_SETUP_7 || uOredInOutMask == AK_SPEAKER_SETUP_6 )
					{
						// Bleed L and R to corresponding side channels.
						pChanBL[ AK_IDX_SETUP_NOCENTER_SIDE_LEFT + uIdxInOutBackOffset ] = 1.f;
						pChanBR[ AK_IDX_SETUP_NOCENTER_SIDE_RIGHT + uIdxInOutBackOffset ] = 1.f;

						_GetSpeakerVolumes2DPan3( in_fX, in_fY, (AkReal32*)pPanningWeights, uOredInOutMask );
					}
					else
#endif
					{
						_GetSpeakerVolumes2DPan2( in_fX, in_fY, (AkReal32*)pPanningWeights, uOredInOutMask );
					}
				}
				break;
#ifdef AK_LFECENTER
				case AK_SPEAKER_SETUP_5:
				{
					AK::SpeakerVolumes::VectorPtr pChanL = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_FRONT_LEFT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanR = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_FRONT_RIGHT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanC = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_CENTER, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanBL = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_WITHCENTER_BACK_LEFT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanBR = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_WITHCENTER_BACK_RIGHT, configPanInOut.uNumChannels );

					pChanL[ AK_IDX_SETUP_FRONT_LEFT ] = 1.f;
					pChanR[ AK_IDX_SETUP_FRONT_RIGHT ] = 1.f;
					pChanC[ AK_IDX_SETUP_CENTER ] = 1.f;
					pChanBL[ AK_IDX_SETUP_NOCENTER_BACK_LEFT + uIdxInOutBackOffset ] = 1.f;
					pChanBR[ AK_IDX_SETUP_NOCENTER_BACK_RIGHT + uIdxInOutBackOffset ] = 1.f;
#ifdef AK_71AUDIO
					if ( uOredInOutMask == AK_SPEAKER_SETUP_7 || uOredInOutMask == AK_SPEAKER_SETUP_6 )
					{
						// Bleed L and R to corresponding side channels.
						pChanBL[ AK_IDX_SETUP_NOCENTER_SIDE_LEFT + uIdxInOutBackOffset ] = 1.f;
						pChanBR[ AK_IDX_SETUP_NOCENTER_SIDE_RIGHT + uIdxInOutBackOffset ] = 1.f;

						_GetSpeakerVolumes2DPan3HasCenter( in_fX, in_fY, (AkReal32*)pPanningWeights, uOredInOutMask );
					}
					else
#endif	// 71
					{
						_GetSpeakerVolumes2DPan2HasCenter( in_fX, in_fY, (AkReal32*)pPanningWeights, uOredInOutMask );
					}
				}
				break;
#endif	// AK_LFECENTER
#endif	// AK_REARCHANNELS
#ifdef AK_71AUDIO
				case AK_SPEAKER_SETUP_6:
				{
					AK::SpeakerVolumes::VectorPtr pChanL = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_FRONT_LEFT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanR = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_FRONT_RIGHT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanBL = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_NOCENTER_BACK_LEFT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanBR = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_NOCENTER_BACK_RIGHT, configPanInOut.uNumChannels );
					AK::SpeakerVolumes::VectorPtr pChanSL = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_NOCENTER_SIDE_LEFT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanSR = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_NOCENTER_SIDE_RIGHT, configPanInOut.uNumChannels );

					pChanL[ AK_IDX_SETUP_FRONT_LEFT ] = 1.f;
					pChanR[ AK_IDX_SETUP_FRONT_RIGHT ] = 1.f;
					pChanBL[ AK_IDX_SETUP_NOCENTER_BACK_LEFT + uIdxInOutBackOffset ] = 1.f;
					pChanBR[ AK_IDX_SETUP_NOCENTER_BACK_RIGHT + uIdxInOutBackOffset ] = 1.f;
					pChanSL[ AK_IDX_SETUP_NOCENTER_SIDE_LEFT + uIdxInOutBackOffset ] = 1.f;
					pChanSR[ AK_IDX_SETUP_NOCENTER_SIDE_RIGHT + uIdxInOutBackOffset ] = 1.f;

					_GetSpeakerVolumes2DPan3( in_fX, in_fY, (AkReal32*)pPanningWeights, uOredInOutMask );
				}
				break;

				case AK_SPEAKER_SETUP_7:
				{
					AK::SpeakerVolumes::VectorPtr pChanL = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_FRONT_LEFT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanR = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_FRONT_RIGHT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanC = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_CENTER, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanBL = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_WITHCENTER_BACK_LEFT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanBR = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_WITHCENTER_BACK_RIGHT, configPanInOut.uNumChannels );
					AK::SpeakerVolumes::VectorPtr pChanSL = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_WITHCENTER_SIDE_LEFT, configPanInOut.uNumChannels ); 
					AK::SpeakerVolumes::VectorPtr pChanSR = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, AK_IDX_SETUP_WITHCENTER_SIDE_RIGHT, configPanInOut.uNumChannels );

					pChanL[ AK_IDX_SETUP_FRONT_LEFT ] = 1.f;
					pChanR[ AK_IDX_SETUP_FRONT_RIGHT ] = 1.f;
					pChanC[ AK_IDX_SETUP_CENTER ] = 1.f;
					pChanBL[ AK_IDX_SETUP_NOCENTER_BACK_LEFT + uIdxInOutBackOffset ] = 1.f;
					pChanBR[ AK_IDX_SETUP_NOCENTER_BACK_RIGHT + uIdxInOutBackOffset ] = 1.f;
					pChanSL[ AK_IDX_SETUP_NOCENTER_SIDE_LEFT + uIdxInOutBackOffset ] = 1.f;
					pChanSR[ AK_IDX_SETUP_NOCENTER_SIDE_RIGHT + uIdxInOutBackOffset ] = 1.f;

					_GetSpeakerVolumes2DPan3HasCenter( in_fX, in_fY, (AkReal32*)pPanningWeights, uOredInOutMask );
				}
				break;
#endif
			}

			// Apply panning weights onto each fullband input channel's output distribution.
			AKASSERT( configIn2D.uNumChannels > 0 );
			AkUInt32 uChannel = 0;
			do
			{
				AK::SpeakerVolumes::VectorPtr pChanIn = AK::SpeakerVolumes::Matrix::GetChannel( pRoutingMx, uChannel, configPanInOut.uNumChannels ); 
				AK::SpeakerVolumes::Vector::Mul( pChanIn, pPanningWeights, configPanInOut.uNumChannels );
			}
			while ( ++uChannel < configIn2D.uNumChannels );

			// Downmix to proper output configuration.
			/// LX OPTI Optimized path for config INOUT == config IN
			AkDownmix::Transform( configIn2D, configPanInOut, configOutNoLfe, in_outputConfig.uNumChannels, pRoutingMx, out_pVolumes );
		}	// if ( in_bIsPannerEnabled )

		// Handle LFE.
		HandleLfe( in_inputConfig, in_outputConfig, out_pVolumes );

	}	// if input not mono
}

void CAkSpeakerPan::DecodeAmbisonicsBasic(
	AkChannelConfig		in_inputConfig,
	AkChannelConfig		in_outputConfig,
	AK::SpeakerVolumes::MatrixPtr in_pVolumeMatrix,
	AkDevice *			in_pDevice
	)
{
	AkChannelConfig outputConfigNoLfe = in_outputConfig.RemoveLFE();
	if (outputConfigNoLfe.uNumChannels == 1)
	{
		// Mono = W
		in_pVolumeMatrix[0] = 1.f;
	}
	else if (outputConfigNoLfe.uChannelMask == AK_SPEAKER_SETUP_2_0 || outputConfigNoLfe.uChannelMask == AK_SPEAKER_SETUP_3_0)	// Stereo and 3-stereo.
	{
		// Mid-Side
		// Left = W + Y
		// Right = W - Y
		// N = 2;
		const AkReal32 g = 0.5f;
		AK::SpeakerVolumes::VectorPtr pW = AK::SpeakerVolumes::Matrix::GetChannel(in_pVolumeMatrix, 0, in_outputConfig.uNumChannels);
		pW[0] = pW[1] = g;
		if (in_inputConfig.uNumChannels > 1)
		{
			// Y
			AK::SpeakerVolumes::VectorPtr pY = AK::SpeakerVolumes::Matrix::GetChannel(in_pVolumeMatrix, 1, in_outputConfig.uNumChannels);
			pY[0] = g;
			pY[1] = -g;
		}
	}
	else if (outputConfigNoLfe.uNumChannels > 0 && in_pDevice ) // Ensure the decode matrices exist. Cannot decode if we don't know the layout (endpoint).
	{
		CAkSpeakerPan::ConfigInOut key = { in_inputConfig, in_outputConfig };
		AK::SpeakerVolumes::MatrixPtr * ppMxDecode = in_pDevice->m_mapConfig2DecodeMx.Exists(key);
		if (!ppMxDecode)
		{
			AK::SpeakerVolumes::MatrixPtr pMxDecode = NULL;

			// Use returned uNumSamples samples for ambisonics -> standard decoding. When decoding to anonymous config, use number of channels.
			bool b2D = (in_outputConfig.eConfigType == AK_ChannelConfigType_Standard) && !AK::HasHeightChannels(in_outputConfig.uChannelMask);
						
			int orderidx = s_numChanToOrderMinusOne[in_inputConfig.uNumChannels];
			AkUInt32 uNumHarmonics = OrderMinusOneToNumHarmonics(orderidx);
			const AkReal32 *weights, *decoder = NULL;
			AkUInt32 uNumSamples;

			if (outputConfigNoLfe.eConfigType == AK_ChannelConfigType_Standard)
			{
				void * pPanCache = in_pDevice->GetPanCache(outputConfigNoLfe);
				if (pPanCache)
				{
					// Compute mixing matrix for sampled sphere.
					AK::SpeakerVolumes::MatrixPtr pMxVBAP;
					
					AkReal32 fOneOverNumSamples, normalize;
					
					if (!b2D)
					{
						uNumSamples = s_numSamples3Dsampling[orderidx];
						fOneOverNumSamples = 1.f / uNumSamples;
						normalize = k_surfSphere * fOneOverNumSamples;
												
						AkUInt32 uVbapSize = AK::SpeakerVolumes::Matrix::GetRequiredSize(uNumSamples, outputConfigNoLfe.uNumChannels);
						pMxVBAP = (AK::SpeakerVolumes::MatrixPtr)AkAlloca(uVbapSize);
						AK::SpeakerVolumes::Matrix::Zero(pMxVBAP, uNumSamples, outputConfigNoLfe.uNumChannels);
						
						// 3D
						const AkReal32 * samples = s_sampling3D[orderidx];
						
						for (AkUInt32 uIn = 0; uIn < uNumSamples; uIn++)
						{
							AK::SpeakerVolumes::VectorPtr pIn = AK::SpeakerVolumes::Matrix::GetChannel(pMxVBAP, uIn, outputConfigNoLfe.uNumChannels);
							/// OPTI: Normalization / vector ops computed on sparse vector...
							// Transform left-handed-z-front samples' system into AkVBAP's right-handed-x-front system.
							AkVector * sample = (AkVector*)((AKSIMD_V4F32*)samples + uIn);
							AkVBAP::ComputeVBAPSquared(pPanCache, sample->Z, -sample->X, sample->Y, outputConfigNoLfe.uNumChannels, pIn);
							AkReal32 fGainSum = AK::SpeakerVolumes::Vector::L1Norm(pIn, outputConfigNoLfe.uNumChannels);
							if (fGainSum > 0.f)
							{
								AK::SpeakerVolumes::Vector::Mul(pIn, fOneOverNumSamples / fGainSum, outputConfigNoLfe.uNumChannels);
								AK::SpeakerVolumes::Vector::Sqrt(pIn, outputConfigNoLfe.uNumChannels); //convert power back to a gain
							}
						}
						
						decoder = GetSampledHarmonics3D(orderidx);
						weights = s_weightsRE_3D[orderidx];
					}
					else
					{
						uNumSamples = s_numSamples2Dsampling[orderidx];
						fOneOverNumSamples = 1.f / uNumSamples; 
						normalize = k_periCircle * fOneOverNumSamples;
						
						AkUInt32 uVbapSize = AK::SpeakerVolumes::Matrix::GetRequiredSize(uNumSamples, outputConfigNoLfe.uNumChannels);
						pMxVBAP = (AK::SpeakerVolumes::MatrixPtr)AkAlloca(uVbapSize);
						AK::SpeakerVolumes::Matrix::Zero(pMxVBAP, uNumSamples, outputConfigNoLfe.uNumChannels);
						
						const AkReal32 * samples = s_sampling2D[orderidx];

						AkUInt32 uNumArcs = GetNum2dArcs(outputConfigNoLfe.uNumChannels);
						for (AkUInt32 uIn = 0; uIn < uNumSamples; uIn++)
						{
							AK::SpeakerVolumes::VectorPtr pChanIn = AK::SpeakerVolumes::Matrix::GetChannel(pMxVBAP, uIn, outputConfigNoLfe.uNumChannels);
							AkVector * sample = (AkVector*)((AKSIMD_V4F32*)samples + uIn);
							AddPowerVbap2d(pPanCache, uNumArcs, *sample, fOneOverNumSamples, ChannelMasksAndIndicesForSpread[outputConfigNoLfe.uNumChannels - 1], pChanIn);
							AK::SpeakerVolumes::Vector::Sqrt(pChanIn, outputConfigNoLfe.uNumChannels);
						}

						decoder = GetSampledHarmonics2D(orderidx);
						weights = s_weightsRE_2D[orderidx];
					}

					if (!decoder)
						return; // unsupported config or oom

					// Mixing matrix is represented in transposed form: D'=Dopt*G'.
					AkUInt32 uDecodeSize = AK::SpeakerVolumes::Matrix::GetRequiredSize(in_inputConfig.uNumChannels, in_outputConfig.uNumChannels); // allocate full size (lfe) for fast copy.
					pMxDecode = (AK::SpeakerVolumes::MatrixPtr)AkAlloc(AkMemID_Object, uDecodeSize);
					if (pMxDecode)
					{
						AkUInt32 uChanIn = 0;
						for (AkUInt32 row = 0; row < uNumHarmonics && uChanIn < in_inputConfig.uNumChannels; row++)
						{
							// Skip harmonics for incomplete channel configs.
							if (SkipHarmonic(in_inputConfig.uNumChannels, row))
								continue;
							AK::SpeakerVolumes::VectorPtr pMxDecodeRow = AK::SpeakerVolumes::Matrix::GetChannel(pMxDecode, uChanIn, in_outputConfig.uNumChannels);
							AK::SpeakerVolumes::Vector::Zero(pMxDecodeRow, in_outputConfig.uNumChannels);
							for (AkUInt32 col = 0; col < outputConfigNoLfe.uNumChannels; col++)
							{
								AkReal32 fValue = 0;
								for (AkUInt32 s = 0; s < uNumSamples; s++)
								{
									fValue += normalize * weights[row] * decoder[DECODER_IDX(row, s, uNumSamples)] * AK::SpeakerVolumes::Matrix::GetChannel(pMxVBAP, s, outputConfigNoLfe.uNumChannels)[col];
								}
								pMxDecodeRow[col] = fValue;
							}
							++uChanIn;
						}
					}
				}
			}
			else
			{
				// Over anonymous config.
				uNumSamples = in_outputConfig.uNumChannels;
				decoder = GetSampledHarmonics3D(orderidx);

				AkUInt32 uDecodeSize = AK::SpeakerVolumes::Matrix::GetRequiredSize(in_inputConfig.uNumChannels, in_outputConfig.uNumChannels);
				pMxDecode = (AK::SpeakerVolumes::MatrixPtr)AkAlloc(AkMemID_Object, uDecodeSize);
				if (pMxDecode)
				{
					for (AkUInt32 row = 0; row < in_inputConfig.uNumChannels; row++)
					{
						AK::SpeakerVolumes::VectorPtr pMxDecodeRow = AK::SpeakerVolumes::Matrix::GetChannel(pMxDecode, row, in_outputConfig.uNumChannels);
						for (AkUInt32 col = 0; col < outputConfigNoLfe.uNumChannels; col++)
						{
							pMxDecodeRow[col] = decoder[DECODER_IDX(row, col, uNumSamples)];
						}
					}
				}
			}

			if (pMxDecode)
			{
				CAkSpeakerPan::ConfigInOut key2 = { in_inputConfig, in_outputConfig };
				ppMxDecode = in_pDevice->m_mapConfig2DecodeMx.Set(key2, pMxDecode);
				if (!ppMxDecode)
				{
					// Failed to store.
					AkFree(AkMemID_Object, pMxDecode);
				}
			}
		}
		
		if (ppMxDecode)
		{
			AK::SpeakerVolumes::Matrix::Copy(in_pVolumeMatrix, *ppMxDecode, in_inputConfig.uNumChannels, in_outputConfig.uNumChannels);
		}
	}
}

void CAkSpeakerPan::GetDecodingMatrixShape(
	AkChannelConfig		in_inputConfig,	// ambisonic config
	AkUInt32 &			out_numPoints,
	int &				out_orderidx
)
{
	AKASSERT(in_inputConfig.eConfigType == AK_ChannelConfigType_Ambisonic);
	out_orderidx = s_numChanToOrderMinusOne[in_inputConfig.uNumChannels];
	out_numPoints = s_numSamples3Dsampling[out_orderidx];
}

AKRESULT CAkSpeakerPan::CopyDecoderInto(
	int					in_orderidx,
	AK::SpeakerVolumes::MatrixPtr out_mx // Preallocated matrix
)
{
	const AkReal32 * decoder = GetSampledHarmonics3D(in_orderidx);
	if (decoder)
	{
		AkUInt32 numPoints = s_numSamples3Dsampling[in_orderidx];
		AkUInt32 numHarmonics = OrderMinusOneToNumHarmonics(in_orderidx);
		for (AkUInt32 row = 0; row < numHarmonics; row++)
		{
			AK::SpeakerVolumes::VectorPtr pMxDecodeRow = AK::SpeakerVolumes::Matrix::GetChannel(out_mx, row, numPoints);
			for (AkUInt32 col = 0; col < numPoints; col++)
			{
				pMxDecodeRow[col] = decoder[DECODER_IDX(row, col, numPoints)];
			}
		}
		return AK_Success;
	}
	return AK_Fail;
}

void CAkSpeakerPan::GetSpeakerVolumes2DPan(
	AkReal32			in_fX,			// [0..1] // 0 = full left, 1 = full right
	AkReal32			in_fY,			// [0..1] // 0 = full rear, 1 = full front
	AkReal32			in_fCenterPct,	// [0..1]
	AkSpeakerPanningType	in_ePannerType,
	AkChannelConfig		in_inputConfig,
	AkChannelConfig		in_outputConfig,
	AK::SpeakerVolumes::MatrixPtr in_pVolumeMatrix,
	AkDevice *			in_pDevice
	)
{
	// Clear.
	AK::SpeakerVolumes::Matrix::Zero( in_pVolumeMatrix, in_inputConfig.uNumChannels, in_outputConfig.uNumChannels );

	if (in_inputConfig.eConfigType == in_outputConfig.eConfigType)
	{
		if (in_outputConfig.eConfigType == AK_ChannelConfigType_Standard)
		{
			GetSpeakerVolumes2DPanStd(in_fX, in_fY, in_fCenterPct, in_ePannerType, in_inputConfig, in_outputConfig, in_pVolumeMatrix);
		}
		else
		{
			AKASSERT(in_outputConfig.eConfigType == AK_ChannelConfigType_Ambisonic
				|| in_outputConfig.eConfigType == AK_ChannelConfigType_Anonymous);

			GetSpeakerVolumesDirectNoMask(in_inputConfig, in_outputConfig, in_pVolumeMatrix);
		}
	}
	else
	{
		/// TODO 2D panning.

		/// Ambisonics conversions.
		if (in_inputConfig.eConfigType == AK_ChannelConfigType_Ambisonic
			&& in_outputConfig.eConfigType != AK_ChannelConfigType_Ambisonic)
		{
			// Decoding.
			DecodeAmbisonicsBasic(in_inputConfig, in_outputConfig, in_pVolumeMatrix, in_pDevice);
		}
		else if (in_inputConfig.eConfigType == AK_ChannelConfigType_Standard
			&& in_outputConfig.eConfigType == AK_ChannelConfigType_Ambisonic)
		{
			// Encoding.
			
			AkChannelConfig inputConfigNoLFE = in_inputConfig.RemoveLFE();

			if (inputConfigNoLFE.uNumChannels == 1)
			{
				// Mono: To W 
				in_pVolumeMatrix[0] = 1.f;
			}
			else
			{
				// Azimuthal channels
				static AkReal32 PlaneLayerAmbisonicsEncodingAngles[AK_NUM_CHANNELS_FOR_SPREAD][AK_NUM_CHANNELS_FOR_SPREAD] =
				{
					{ 0 },	// mono
					{ PIOVERTWO, -PIOVERTWO },	// stereo
					{ PIOVERTWO, -PIOVERTWO, 0 },	// 3.x
					{ PIOVERFOUR, -PIOVERFOUR, THREEPIOVERFOUR, -THREEPIOVERFOUR },	// 4.x
					{ PIOVERFOUR, -PIOVERFOUR, 0, THREEPIOVERFOUR, -THREEPIOVERFOUR }	// 5.x
#ifdef AK_71AUDIO
					, { PIOVERSIX, -PIOVERSIX, 5 * PIOVERSIX, -5 * PIOVERSIX, PIOVERTWO, -PIOVERTWO }	// 6.x
					, { PIOVERSIX, -PIOVERSIX, 0, 5 * PIOVERSIX, -5 * PIOVERSIX, PIOVERTWO, -PIOVERTWO }	// 7.x
#endif
				};

				AkChannelConfig planeLayer;
				planeLayer.SetStandard(inputConfigNoLFE.uChannelMask & AK_SPEAKER_SETUP_DEFAULT_PLANE);
				for (AkUInt32 uChan = 0; uChan < planeLayer.uNumChannels; uChan++)
				{
					AK::SpeakerVolumes::VectorPtr pChan = AK::SpeakerVolumes::Matrix::GetChannel(in_pVolumeMatrix, uChan, in_outputConfig.uNumChannels);
					EncodeToAmbisonics(-PlaneLayerAmbisonicsEncodingAngles[planeLayer.uNumChannels - 1][uChan], 0.f, pChan, in_outputConfig.uNumChannels);
				}

#ifdef AK_71AUDIO
				// Height channels
				static AkReal32 HeightLayerAmbisonicsEncodingAzimuth[][6] =
				{
					{ 0 },	// HFC
					{ PIOVERTWO, -PIOVERTWO },	// HFL and HFR
					{ PIOVERTWO, 0, -PIOVERTWO },	// HFL, HFC, HFR
					{ PIOVERFOUR, -PIOVERFOUR, THREEPIOVERFOUR, -THREEPIOVERFOUR },	// HFL, HFR, HBL, HBR
					{ 0, PIOVERFOUR, -PIOVERFOUR, THREEPIOVERFOUR, -THREEPIOVERFOUR },	// T, HFL, HFR, HBL, HBR
					{ 0, PIOVERFOUR, 0, -PIOVERFOUR, THREEPIOVERFOUR, -THREEPIOVERFOUR },	// T, HFL, HFC, HFR, HBL, HBR
				};
				static AkReal32 HeightLayerAmbisonicsEncodingElevation[][6] =
				{
					{ PIOVERFOUR },	// HFC
					{ PIOVERFOUR, PIOVERFOUR },	// HFL and HFR
					{ PIOVERFOUR, PIOVERFOUR, PIOVERFOUR },	// HFL, HFC, HFR
					{ PIOVERFOUR, PIOVERFOUR, PIOVERFOUR, PIOVERFOUR  },	// HFL, HFR, HBL, HBR
					{ PIOVERTWO, PIOVERFOUR, PIOVERFOUR, PIOVERFOUR, PIOVERFOUR },	// T, HFL, HFR, HBL, HBR
					{ PIOVERTWO, PIOVERFOUR, PIOVERFOUR, PIOVERFOUR, PIOVERFOUR, PIOVERFOUR },	// T, HFL, HFC, HFR, HBL, HBR
				};

				AkChannelConfig heightLayer;
				heightLayer.SetStandard(inputConfigNoLFE.uChannelMask & ~AK_SPEAKER_SETUP_DEFAULT_PLANE);
				for (AkUInt32 uChan = 0; uChan < heightLayer.uNumChannels; uChan++)
				{
					AK::SpeakerVolumes::VectorPtr pChan = AK::SpeakerVolumes::Matrix::GetChannel(in_pVolumeMatrix, planeLayer.uNumChannels + uChan, in_outputConfig.uNumChannels);
					EncodeToAmbisonics(-HeightLayerAmbisonicsEncodingAzimuth[heightLayer.uNumChannels - 1][uChan], HeightLayerAmbisonicsEncodingElevation[heightLayer.uNumChannels - 1][uChan], pChan, in_outputConfig.uNumChannels);
				}
#endif
			}
		}
		else if ((in_inputConfig.eConfigType == AK_ChannelConfigType_Anonymous
			&& in_outputConfig.eConfigType == AK_ChannelConfigType_Standard) ||
			( in_inputConfig.eConfigType == AK_ChannelConfigType_Standard
			&& in_outputConfig.eConfigType == AK_ChannelConfigType_Anonymous))
		{
			// Direct speaker assignment by index
			AkUInt32 uMinChan = AkMin(in_inputConfig.uNumChannels, in_outputConfig.uNumChannels);
			for (AkUInt32 uChan = 0; uChan < uMinChan; uChan++)
			{
				AK::SpeakerVolumes::VectorPtr pChan = AK::SpeakerVolumes::Matrix::GetChannel(in_pVolumeMatrix, uChan, in_outputConfig.uNumChannels);
				pChan[uChan] = 1.0f;
			}
		}
		else
			AKASSERT(!"Mixing rule not implemented");/// TODO Implement anonymous to mono and vice-versa	
	}
}

void CAkSpeakerPan::GetDefaultSpeakerAngles( 
	AkChannelConfig	in_channelConfig,	// Desired channel config.
	AkReal32 		out_angles[],		// Returned angles for given channel config. Must be preallocated (use AK::GetNumberOfAnglesForConfig()).
	AkReal32 &		out_fHeightAngle	// Returned height in degrees.
	)
{
	// Defaults.
	out_angles[0] = 45.f;	// Front channels.
#ifdef AK_71AUDIO
	out_angles[1] = 90.f;	// Side channels of 7.1 setup
	out_angles[2] = 135.f;	// Rear channels of 7.1 setup	
#elif defined(AK_REARCHANNELS)
	out_angles[1] = 112.5f;	// Surround channels of 5.1 setup
#endif
	out_fHeightAngle = 54.7f;
}

AKRESULT CAkSpeakerPan::SetSpeakerAngles(
	const AkReal32 *	in_pfSpeakerAngles,			// Array of loudspeaker pair angles, expressed in degrees relative to azimuth ([0,180]).
	AkUInt32			in_uNumAngles,				// Number of loudspeaker pair angles.
	AkReal32 *			out_fSpeakerAngles,			// Pointer to buffer filled with speaker angles (rad).
	AkReal32 &			out_fMinAngleBetweenSpeakers// Returned minimum angle between speakers (rad).
	)
{
	AKASSERT( in_uNumAngles > 0 );

	// Convert speaker angles to uint 0-PAN_CIRCLE. 
	for ( AkUInt32 uAngle = 0; uAngle < in_uNumAngles; uAngle++ )
	{
		out_fSpeakerAngles[uAngle] = TWOPI * in_pfSpeakerAngles[uAngle] / 360.f;
		if (out_fSpeakerAngles[uAngle] >= PI)
		{
			AKASSERT( !"Angle out of range" );
			return AK_Fail;
		}
	}

	// Find and store the minimum angle. 
	// At the same time, ensure that they respect the following constraints: 
	// - Values [0,180]
	// - Any angle smaller than 180 degrees (speaker 0 must be smaller than 90 degrees).
	// - Increasing order
	AkReal32 fMinAngleBetweenSpeakers = out_fSpeakerAngles[0];
	if (fMinAngleBetweenSpeakers >= PIOVERTWO)
	{
		AKASSERT( !"fSpeakerAngles[0] must be smaller than 90 degrees" );
		return AK_Fail;
	}
	AkUInt32 uSpeaker = 1;
	while ( uSpeaker < in_uNumAngles )
	{
		// Check each interval.
		if (out_fSpeakerAngles[uSpeaker] < out_fSpeakerAngles[uSpeaker - 1])
		{
			AKASSERT( !"Angles need to be in increasing order" );
			return AK_Fail;
		}
		AkReal32 fInterval = out_fSpeakerAngles[uSpeaker] - out_fSpeakerAngles[uSpeaker - 1];
		if (fInterval == 0 || fInterval >= PI)
		{
			AKASSERT( !"Speaker interval out of range ]0,180[" );
			return AK_Fail;
		}
		if ( fInterval < fMinAngleBetweenSpeakers )
			fMinAngleBetweenSpeakers = out_fSpeakerAngles[uSpeaker] - out_fSpeakerAngles[uSpeaker - 1];
		++uSpeaker;
	}
	// Check interval between last speaker and its image on the other side, unless there is only 2 channels.
	if ( uSpeaker > 1 )	// not stereo.
	{
		AkReal32 fInterval = TWOPI - 2 * out_fSpeakerAngles[uSpeaker - 1];
		if (fInterval < fMinAngleBetweenSpeakers)
			fMinAngleBetweenSpeakers = fInterval;
	}
	
	out_fMinAngleBetweenSpeakers = fMinAngleBetweenSpeakers;

	return AK_Success;
}

// Compute 3D speaker gains for one point source.
AKRESULT CAkSpeakerPan::ComputePlanarVBAPGains( 
	AkDevice *		in_pDevice,				// Output device.
	AkReal32		in_fAngle,				// Incident angle, in radians [-pi,pi], where 0 is the azimuth (positive values are clockwise)
	AkChannelConfig	in_outputConfig,		// Desired output configuration. 
	AkReal32		in_fCenterPerc,			// Center percentage. Only applies to mono inputs to outputs that have no center.
	AK::SpeakerVolumes::VectorPtr out_vVolumes	// Returned volumes.
	)
{
	AKASSERT( in_outputConfig.uNumChannels > 0 );
	AKRESULT eResult = in_pDevice->EnsurePanCacheExists( in_outputConfig );
	if ( eResult == AK_Success )
	{
		AkChannelConfig	outputConfigNoLfe = in_outputConfig.RemoveLFE();
		AK::SpeakerVolumes::Vector::Zero( out_vVolumes, in_outputConfig.uNumChannels );

		if (outputConfigNoLfe.uChannelMask == AK_SPEAKER_SETUP_STEREO && in_pDevice->ePanningRule == AkPanningRule_Headphones)
		{
			// Headphone panning.
			AddHeadphonePower(sinf(in_fAngle), out_vVolumes);
		}
		else
		{
			void * pPanCache = in_pDevice->GetPanCache(outputConfigNoLfe);
			if (!pPanCache)
				return AK_Fail;
			AkUInt32 uNumArcs = GetNum2dArcs(outputConfigNoLfe.uNumChannels);
			AkVector in = { sinf(in_fAngle), 0.f, cosf(in_fAngle) };
			AddPowerVbap2d(pPanCache, uNumArcs, in, 1.f, ChannelMasksAndIndicesForSpread[outputConfigNoLfe.uNumChannels - 1], out_vVolumes);
		}
		AK::SpeakerVolumes::Vector::Sqrt( out_vVolumes, in_outputConfig.uNumChannels ); //convert power back to a gain
	}
	return eResult;
}

// Convert radians to SDK-friendly speaker angles (degrees)
void CAkSpeakerPan::ConvertSpeakerAngles(
	AkReal32		in_fRadians[],				// Speaker angles stored in device (rad).
	AkUInt32		in_uNumAngles,				// Number of angle values.
	AkReal32		out_arAngles[]				// Returned speaker angles, in degrees (see AkOutputSettings::fSpeakerAngles). Pass in an array of size in_uNumAngles.
	)
{
	for ( AkUInt32 uAngle = 0; uAngle < in_uNumAngles; uAngle++ )
	{
		out_arAngles[uAngle] = 360.f * in_fRadians[uAngle] / TWOPI;
	}
}

void CAkSpeakerPan::MultiDirectionAmbisonicPan(
	AkChannelConfig		in_inputConfig,
	AkChannelConfig		in_outputConfig,
	AK::SpeakerVolumes::MatrixPtr in_pVolumeMatrices,
	AkUInt32			in_matricesSize,
	AkReal32 *			in_mxWeights,
	AkUInt32			in_numMatrices,
	AK::SpeakerVolumes::MatrixPtr io_mxResult
)
{
	// Get decoder.
	int orderidx = s_numChanToOrderMinusOne[in_outputConfig.uNumChannels];
	
	const AkReal32 * decoder = CAkSpeakerPan::GetSampledHarmonics3D(orderidx);
	if (!decoder)
		return; // unsupported config

	AkUInt32 uNumAngles = s_numSamples3Dsampling[orderidx];

	// Let's allocate a vector for keeping the max contribution of each ray to each virtual speaker.
	AK::SpeakerVolumes::VectorPtr vMax = (AK::SpeakerVolumes::VectorPtr)AkAlloca(AK::SpeakerVolumes::Vector::GetRequiredSize(uNumAngles));

	// Let's also allocate a temporary vector for each weighted ray's input channel
	AK::SpeakerVolumes::VectorPtr vWeightRayChannel = (AK::SpeakerVolumes::VectorPtr)AkAlloca(AK::SpeakerVolumes::Vector::GetRequiredSize(in_outputConfig.uNumChannels));

	AkReal32 normalization = 1.f / uNumAngles;

	// Go one input channel at a time.
	for (AkUInt32 uChanIn = 0; uChanIn < in_inputConfig.uNumChannels; uChanIn++)
	{
		AK::SpeakerVolumes::Vector::Zero(vMax, uNumAngles);

		// Loop on relevant rays.
		for (AkUInt32 uVolume = 0; uVolume < in_numMatrices; uVolume++)
		{
			// Take the decoding matrix
			AK::SpeakerVolumes::MatrixPtr volumes = in_pVolumeMatrices + in_matricesSize * uVolume;

			// Take the gain vector for this input
			AK::SpeakerVolumes::ConstVectorPtr vRayChannel = AK::SpeakerVolumes::Matrix::GetChannel(volumes, uChanIn, in_outputConfig.uNumChannels);

			// Copy in working vector and multiply by this ray's weight.
			AK::SpeakerVolumes::Vector::Copy(vWeightRayChannel, vRayChannel, in_outputConfig.uNumChannels, in_mxWeights[uVolume]);

			// Decode vWeightRayChannel to virtual speakers; for each decoded virtual speaker, get the max with other rays.
			for (AkUInt32 uAngle = 0; uAngle < uNumAngles; uAngle++)
			{
				AkReal32 gain = 0;
				for (AkUInt32 uHarm = 0; uHarm < in_outputConfig.uNumChannels; uHarm++)
				{
					gain += decoder[DECODER_IDX(uHarm, uAngle, uNumAngles)] * vWeightRayChannel[uHarm] * normalization;
				}
				if (fabsf(gain) > fabsf(vMax[uAngle]))
					vMax[uAngle] = gain;
			}
		}

		// Encode vector into output matrix for this input channel.
		AK::SpeakerVolumes::VectorPtr vOut = AK::SpeakerVolumes::Matrix::GetChannel(io_mxResult, uChanIn, in_outputConfig.uNumChannels);
		EncodeVector(orderidx, vMax, vOut);
	}
}

//////////////////////////////////////////////////////////////////////////
// Definitions of ContractInside and ContractOutside (and template
// instantiations for ContractPoints)

class ContractInside
{
public:
	AkForceInline void Setup(AkReal32 in_fSpread)
	{
		spread = AKSIMD_SET_V4F32(in_fSpread);
		spreadBias = AKSIMD_SET_V4F32(PIOVERTWO * (1.f - in_fSpread));
	}

	// Make spreadbias = PIOVERTWO * (1.f - spread)
	AkForceInline void Contract(AKSIMD_V4F32& io_xxxx, AKSIMD_V4F32& io_yyyy, AKSIMD_V4F32& io_zzzz)
	{
		// Clamp |z| to epsilon below 1 (i.e. |asin(z)| epsilon below pi/2), to avoid singularity when expanding x/y, and to pad against z having gone
		// slightly out of bounds due to previous transformations (which may result in slight dilation instead of contraction).
		// Epsilon should also be large enough so that z**2 is also strictly smaller than 1.
		const AKSIMD_V4F32 eps = AKSIMD_SET_V4F32(0.0001f);
		const AKSIMD_V4F32 one = AKSIMD_SET_V4F32(1.f);

		// Fix input and deal with singularities
		AKSIMD_V4F32 zzzz = AKSIMD_MAX_V4F32(io_zzzz, AKSIMD_NEG_V4F32(one));
		AKSIMD_V4F32 xysquarenorm = AKSIMD_ADD_V4F32(AKSIMD_MUL_V4F32(io_xxxx, io_xxxx), AKSIMD_MUL_V4F32(io_yyyy, io_yyyy));
		zzzz = AKSIMD_SEL_GTEZ_V4F32(AKSIMD_SUB_V4F32(xysquarenorm, eps), zzzz, one); // a >= 0 ? b : c 

		// phi = arcsin(z) ~=
		// z2 = z * z
		// phi = ((-0.35786 + 0.719088 * z2) * z2 + 1.085865) * z
		AKSIMD_V4F32 zzzz2 = AKSIMD_MUL_V4F32(zzzz, zzzz);
		AKSIMD_V4F32 phi = AKSIMD_MUL_V4F32(AKSIMD_MADD_V4F32(AKSIMD_MADD_V4F32(AKSIMD_SET_V4F32(0.719088f), zzzz2, AKSIMD_SET_V4F32(-0.35786f)), zzzz2, AKSIMD_SET_V4F32(1.085865f)), zzzz);
		// phiPrime = spread * phi + pi / 2. * (1. - spread)
		AKSIMD_V4F32 phiPrime = AKSIMD_MADD_V4F32(spread, phi, spreadBias);
		// zprime = np.sin(phiPrime) ~= ((phiPrime^2 * 0.00833216076 + -0.166666546) * phiPrime^2 + 1.) * phiPrime
		AKSIMD_V4F32 phiPrime2 = AKSIMD_MUL_V4F32(phiPrime, phiPrime);
		io_zzzz = AKSIMD_MUL_V4F32(phiPrime, AKSIMD_MADD_V4F32(AKSIMD_MADD_V4F32(phiPrime2, AKSIMD_SET_V4F32(0.00833216076f), AKSIMD_SET_V4F32(-0.166666546f)), phiPrime2, one));

		// xprime = fix * x
		// fix = cos(asin(zprime))/cos(asin(z)) = sqrt(1-zprime**2/1-z**2) = sqrt(1-zprime**2/(x**2+y**2)) = // (x**2+y**2) preferable over 1-z^2 for xy projection norm overshooting 1
		// (1-zprime**2) / sqrt((1-zprime**2)*(x**2+y**2))
		AKSIMD_V4F32 num = AKSIMD_MADD_V4F32(io_zzzz, AKSIMD_NEG_V4F32(io_zzzz), one);
		AKSIMD_V4F32 den = AKSIMD_MUL_V4F32(num, xysquarenorm);
		den = AKSIMD_MAX_V4F32(den, eps);
		AKSIMD_V4F32 fix = AKSIMD_MUL_V4F32(num, AKSIMD_RSQRT_V4F32(den));

		io_xxxx = AKSIMD_MUL_V4F32(io_xxxx, fix);
		io_yyyy = AKSIMD_MUL_V4F32(io_yyyy, fix);
	}
private:
	AKSIMD_V4F32 spread;
	AKSIMD_V4F32 spreadBias;
};

class ContractInsideScalar
{
public:
	AkForceInline void Setup(AkReal32 in_fSpread)
	{
		spread = in_fSpread;
		spreadBias = PIOVERTWO * (1.f - in_fSpread);
	}

	// Make spreadbias = PIOVERTWO * (1.f - spread)
	AkForceInline void Contract(AkReal32& x, AkReal32& y, AkReal32& z)
	{
		// Clamp |z| to epsilon below 1 (i.e. |asin(z)| epsilon below pi/2), to avoid singularity when expanding x/y, and to pad against z having gone
		// slightly out of bounds due to previous transformations (which may result in slight dilation instead of contraction).
		// Epsilon should also be large enough so that z**2 is also strictly smaller than 1.
		const AkReal32 eps = 0.0001f;

		// Fix input and deal with singularities
		AkReal32 tempz = AkMax(z, -1.f);
		AkReal32 xysquarenorm = x * x + y * y;
		tempz = (xysquarenorm - eps) >= 0 ? tempz : 1.f; // a >= 0 ? b : c 

																					  // phi = arcsin(z) ~=
																					  // z2 = z * z
																					  // phi = ((-0.35786 + 0.719088 * z2) * z2 + 1.085865) * z
		AkReal32 z2 = tempz * tempz;
		AkReal32 phi = ((0.719088f * z2 - 0.35786f) * z2 + 1.085865f) * tempz;
		// phiPrime = spread * phi + pi / 2. * (1. - spread)
		AkReal32 phiPrime = spread * phi + spreadBias;
		// zprime = np.sin(phiPrime) ~= ((phiPrime^2 * 0.00833216076 + -0.166666546) * phiPrime^2 + 1.) * phiPrime
		AkReal32 phiPrime2 = phiPrime * phiPrime;
		z = phiPrime * ((phiPrime2 * 0.00833216076f - 0.166666546f) * phiPrime2 + 1.f);

		// xprime = fix * x
		// fix = cos(asin(zprime))/cos(asin(z)) = sqrt(1-zprime**2/1-z**2) = sqrt(1-zprime**2/(x**2+y**2)) = // (x**2+y**2) preferable over 1-z^2 for xy projection norm overshooting 1
		// (1-zprime**2) / sqrt((1-zprime**2)*(x**2+y**2))
		AkReal32 num = z * -z + 1.f;
		AkReal32 den = num * xysquarenorm;
		den = AkMax(den, eps);
		AkReal32 fix = num / sqrtf(den);

		x *= fix;
		y *= fix;
	}
private:
	AkReal32 spread;
	AkReal32 spreadBias;
};

// Algorithm for spread <= 0.5f
class ContractOutside
{
public:
	AkForceInline void Setup(AkReal32 in_fSpread)
	{
		AKASSERT(in_fSpread <= 0.5f);
		AkReal32 d = 1.f / (AkMath::FastSin(in_fSpread * PI) + 0.0001f);
		dddd = AKSIMD_SET_V4F32(d);
	}

	AkForceInline void Contract(AKSIMD_V4F32& io_xxxx, AKSIMD_V4F32& io_yyyy, AKSIMD_V4F32& io_zzzz)
	{
		const AKSIMD_V4F32 zmin = AKSIMD_SET_V4F32(0.001f - 1.f);

		io_zzzz = AKSIMD_ADD_V4F32(AKSIMD_MAX_V4F32(io_zzzz, zmin), dddd);
		AKSIMD_V4F32 oneovernorm = AKSIMD_RSQRT_V4F32(AKSIMD_ADD_V4F32(AKSIMD_ADD_V4F32(AKSIMD_MUL_V4F32(io_xxxx, io_xxxx), AKSIMD_MUL_V4F32(io_yyyy, io_yyyy)), AKSIMD_MUL_V4F32(io_zzzz, io_zzzz)));
		io_xxxx = AKSIMD_MUL_V4F32(io_xxxx, oneovernorm);
		io_yyyy = AKSIMD_MUL_V4F32(io_yyyy, oneovernorm);
		io_zzzz = AKSIMD_MUL_V4F32(io_zzzz, oneovernorm);
	}

private:
	AKSIMD_V4F32 dddd;
};

class ContractOutsideScalar
{
public:
	AkForceInline void Setup(AkReal32 in_fSpread)
	{
		AKASSERT(in_fSpread <= 0.5f);
		d = 1.f / (AkMath::FastSin(in_fSpread * PI) + 0.0001f);
	}

	AkForceInline void Contract(AkReal32& x, AkReal32& y, AkReal32& z)
	{
		const AkReal32 zmin = 0.001f - 1.f;

		z = AkMax(z, zmin) + d;
		AkReal32 oneovernorm = 1.f / sqrtf(x * x + y * y + z * z);
		x *= oneovernorm;
		y *= oneovernorm;
		z *= oneovernorm;
	}

private:
	AkReal32 d;
};

template void CAkSpeakerPan::ContractPoints<ContractInside>(const AkReal32* AK_RESTRICT in_points, AkUInt32 in_numPoints, AkReal32* AK_RESTRICT out_points, AkReal32 in_spread);
template void CAkSpeakerPan::ContractPoints<ContractOutside>(const AkReal32* AK_RESTRICT in_points, AkUInt32 in_numPoints, AkReal32* AK_RESTRICT out_points, AkReal32 in_spread);
template void CAkSpeakerPan::ContractPointsScalar<ContractInsideScalar>(const AkReal32* AK_RESTRICT in_points, AkUInt32 in_numPoints, AkReal32* AK_RESTRICT out_points, AkReal32 in_spread);
template void CAkSpeakerPan::ContractPointsScalar<ContractOutsideScalar>(const AkReal32* AK_RESTRICT in_points, AkUInt32 in_numPoints, AkReal32* AK_RESTRICT out_points, AkReal32 in_spread);
