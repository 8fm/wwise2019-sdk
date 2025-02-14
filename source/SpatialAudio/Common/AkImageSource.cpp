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

#include "stdafx.h"
#include "AkImageSource.h"
#include "AkReflectInstance.h"
#include "AkGeometrySet.h"
#include "AkOcclusionChecker.h"
#include "AkSpatialAudioCompts.h"
#include "AkDiffractionPath.h"
#include "AkSoundGeometry.h"

const AkAcousticSurface& AkImageSourceTriangle::GetAcousticSurface() const
{
	return pGeoSet != NULL ? pGeoSet->GetAcousticSurface(this) : AkGeometrySet::sDefaultAcousticSurface;
}

AkSurfIdx AkImageSourceTriangle::GetSurfIdx() const
{
	return pGeoSet != NULL ? pGeoSet->GetSurfIdx(this) : AK_INVALID_SURFACE;
}

AkReal32 CAkReflectionPaths::GetPortalFade(AkUInt32 in_reflectionIdx, const CAkSpatialAudioEmitter* in_pEmitter) const
{
	AkReal32 fadeEmitter = 1.0f;
	if (in_reflectionIdx < transitionIdx)
	{
		fadeEmitter *= in_pEmitter->GetSpatialAudioComponent()->GetTransitionRatio();
	}
	else // in_reflectionIdx >= transitionIdx
	{
		fadeEmitter *= 1.f - in_pEmitter->GetSpatialAudioComponent()->GetTransitionRatio();
	}
	
	return fadeEmitter;
}

void CAkReflectionPaths::GetVirtualSources(CAkReflectInstance& out_reflect, const CAkSpatialAudioListener* in_pListener, const CAkSpatialAudioEmitter* in_pEmitter) const
{
#if defined(AK_ENABLE_ASSERTS) && !defined(MSTC_SYSTEMATIC_MEMORY_STRESS) // a memory failure might trigger this assert. In that case, the assert can be ignored.
	// A duplicate path ID means there was an error in the hash calculation (maybe a reflector/edge/portal was skipped?), or an actual duplicate path that should not exist.
	for (AkUInt32 i = 0; i < Length(); ++i)
		for (AkUInt32 j = 0; j < Length(); ++j)
			AKASSERT(i == j || (*this)[i].pathID != (*this)[j].pathID);
#endif

	if (out_reflect.Reserve(Length() + out_reflect.NumActiveImageSources()))
	{
		for (AkUInt32 pathIdx = 0; pathIdx < Length(); ++pathIdx)
		{
			AkReflectionPath& src = (*this)[pathIdx];

			AkReflectImageSource* pVirtSrc = out_reflect.AddVirtualSource();

			pVirtSrc->params.sourcePosition = src.CalcImageSource(in_pEmitter->GetPosition(), in_pListener->GetPosition());
			pVirtSrc->params.fLevel = 1.0f;
			pVirtSrc->params.fDistanceScalingFactor = 1.f;
			pVirtSrc->params.fDiffraction = src.totalDiffraction;
			pVirtSrc->name.pName = NULL;
			pVirtSrc->name.uNumChar = 0;

			for (AkInt32 i = (AkInt32)src.numPathPoints - 1; i >= 0; --i)
			{
				if (src.surfaces[i] != NULL)
				{
					// Name the image source after the first valid acoustic surface on the listener side.
					pVirtSrc->SetName(src.surfaces[i]->strName);
					break;
				}
			}

			// Assign acoustic textures. 
			{
				AkUInt32 textureIdx = 0;
				AkUInt32 surfaceIdx = 0;
				
				// Count the diffractions on the emitter side. 
				while (surfaceIdx < src.numPathPoints && 
					src.surfaces[surfaceIdx] == NULL )
				{
					surfaceIdx++;
				}

				pVirtSrc->params.uDiffractionEmitterSide = (AkUInt8)surfaceIdx;

				// Count the reflections, indicated by a valid surface.
				while (	surfaceIdx < src.numPathPoints &&
						textureIdx < AK_MAX_NUM_TEXTURE &&
						src.surfaces[surfaceIdx] != NULL)
				{
					pVirtSrc->texture.arTextureID[textureIdx] = src.surfaces[surfaceIdx]->textureID;

					++textureIdx;
					++surfaceIdx;
				}

				pVirtSrc->texture.uNumTexture = textureIdx;

				// The remainder indicates the diffractions on the listener side.
				pVirtSrc->params.uDiffractionListenerSide = (AkUInt8)(src.numPathPoints - surfaceIdx);

				AKASSERT(pVirtSrc->params.uDiffractionEmitterSide + pVirtSrc->params.uDiffractionListenerSide + pVirtSrc->texture.uNumTexture == src.numPathPoints);
			}

			pVirtSrc->uID = src.pathID;
			pVirtSrc->params.fLevel = src.level * GetPortalFade(pathIdx, in_pEmitter);
		}
	}
}

Ak3DVector AkReflectionPath::CalcImageSource(const Ak3DVector& in_emitter, const Ak3DVector& in_listener)
{
	Ak3DVector v = in_emitter - Ak3DVector(pathPoint[0]);
	AkReal32 dist = v.Length();

	AkUInt32 i = 1;
	while (i < numPathPoints)
	{
		v = Ak3DVector(pathPoint[i - 1]) - Ak3DVector(pathPoint[i]);
		dist += v.Length();
		++i;
	}
		
	Ak3DVector toListener = Ak3DVector(pathPoint[i - 1]) - in_listener;
	AkReal32 l = toListener.Length();
	dist += l;

	if (l != 0.f)
		return in_listener + (toListener / l) * dist;
	else
		return in_listener;
}

bool AkReflectionPath::AddPoint(Ak3DVector in_diffractionPt, const Ak3DVector& in_listener)
{
	Ak3DVector prevPt(pathPoint[numPathPoints - 1]);

	Ak3DVector incident = prevPt - in_diffractionPt;
	Ak3DVector diffracted = in_diffractionPt - in_listener;
	AkReal32 lengthIncident = incident.Length();
	AkReal32 lengthDiffracted = diffracted.Length();
	AkReal32 denom = (lengthIncident*lengthDiffracted);
	if (denom > AK_SA_EPSILON)
	{
		AkReal32 diffraction_rad = AK::SpatialAudio::ACos((incident.Dot(diffracted) / denom)) / (PI / 2.f);
		if (diffraction_rad > AK_SA_DIFFRACTION_EPSILON)
		{
			if (AccumulateDiffraction(diffraction_rad))
			{
				if (numPathPoints < AK_MAX_REFLECTION_PATH_LENGTH)
				{
					diffraction[numPathPoints] = diffraction_rad;
					pathPoint[numPathPoints] = in_diffractionPt.PointV4F32();
					numPathPoints++;
				}
				else
				{
					// We cannot add the path because it will lead to incorrect length computation and display artifacts (path going through walls for intance)
					return false;
				}

				return true;
			}
			else
			{
				// bail on path due to too much diffraction.
				return false;
			}
		}
	}

	// We didn't add a point, but path overall is still valid.
	return true;
}

bool AkReflectionPath::AddPointFromEmitter(Ak3DVector in_diffractionPt, const Ak3DVector& in_emitter)
{
	Ak3DVector prevPt(pathPoint[numPathPoints - 1]);

	Ak3DVector incident = prevPt - in_emitter;
	Ak3DVector diffracted = in_diffractionPt - prevPt;
	AkReal32 lengthIncident = incident.Length();
	AkReal32 lengthDiffracted = diffracted.Length();
	AkReal32 denom = (lengthIncident*lengthDiffracted);
	if (denom > AK_SA_EPSILON)
	{
		AkReal32 diffraction_rad = AK::SpatialAudio::ACos((incident.Dot(diffracted) / denom)) / (PI / 2.f);
		if (diffraction_rad > AK_SA_DIFFRACTION_EPSILON)
		{
			if (AccumulateDiffraction(diffraction_rad))
			{
				if (numPathPoints < AK_MAX_REFLECTION_PATH_LENGTH)
				{
					diffraction[numPathPoints] = diffraction_rad;
					pathPoint[numPathPoints] = in_diffractionPt.PointV4F32();
					numPathPoints++;
				}

				return true;
			}
			else
			{
				// bail on path due to too much diffraction.
				return false;
			}
		}
	}

	// We didn't add a point, but path overall is still valid.
	return true;
}

bool AkReflectionPath::AppendPathSegment(AkAcousticPortal* pPortal, const AkDiffractionPathSegment& diffractionPath, const Ak3DVector& listenerPos, bool bReversePath)
{
	bool bPathValid = true;

	Ak3DVector ptDiffraction;
	if (bReversePath)
	{
		pPortal->CalcIntersectPoint(PointClosestToListener(), diffractionPath.nodeCount > 0 ? diffractionPath.nodes[0] : diffractionPath.emitterPos, ptDiffraction);

		for (AkUInt32 i = 0; bPathValid && i < diffractionPath.nodeCount; ++i)
		{
			Ak3DVector ptTo = diffractionPath.nodes[i];
			bPathValid = AddPoint(ptDiffraction, ptTo);
			ptDiffraction = ptTo;
		}
	}
	else
	{
		pPortal->CalcIntersectPoint( PointClosestToListener(), diffractionPath.nodeCount > 0 ? diffractionPath.nodes[diffractionPath.nodeCount - 1] : listenerPos, ptDiffraction);

		for (AkInt32 i = diffractionPath.nodeCount - 1; bPathValid && i >= 0; --i)
		{
			Ak3DVector ptTo = diffractionPath.nodes[i];
			bPathValid = AddPoint(ptDiffraction, ptTo);
			ptDiffraction = ptTo;
		}
	}

	// Add final segment to listener
	bPathValid = bPathValid && AddPoint(ptDiffraction, listenerPos);

	AugmentPathID(pPortal);
	AugmentPathID(diffractionPath.pathID);

	return bPathValid;
}

AkReal32 AkReflectionPath::ComputePathLength(const Ak3DVector& i_listener, const Ak3DVector& i_emitter) const
{
	AkReal32 length = 0.0f;
	
	AKASSERT(numPathPoints < AK_MAX_REFLECTION_PATH_LENGTH);

	if (numPathPoints > 0)
	{
		Ak3DVector previousPoint = i_emitter;		
		for (AkUInt32 i = 0; i < numPathPoints; ++i)
		{
			length += (Ak3DVector(pathPoint[i]) - previousPoint).Length();

			previousPoint = Ak3DVector(pathPoint[i]);
		}

		length += (i_listener - previousPoint).Length();
	}

	return length;
}

AkUInt32 AkReflectionPath::GetNumReflections() const
{
	AkUInt32 numReflections = 0;

	for (AkUInt32 i = 0; i < numPathPoints; ++i)
	{
		if (surfaces[i] != NULL)
		{
			++numReflections;
		}
	}

	return numReflections;
}