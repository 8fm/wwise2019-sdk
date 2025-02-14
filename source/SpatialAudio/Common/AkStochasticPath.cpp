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
#include "AkStochasticPath.h"

#include "AkImageSource.h"
#include "AkDiffractionEdge.h"

bool StochasticSource::ComputeImage(const AKSIMD_V4F32& in_emitter,
									const AKSIMD_V4F32& in_listener, 
									const AKSIMD_V4F32& in_source,
									AKSIMD_V4F32& out_imageSource) const
{
	if (m_reflector)
	{
		Ak3DVector source(in_source);

		Ak3DVector wallNorm = m_reflector->Normal();
		Ak3DVector wallPos = m_reflector->Position();
		Ak3DVector toWall = wallPos - source;
		AkReal32 proj = toWall.Dot(wallNorm);
		Ak3DVector imageSource = wallNorm * (2.f * proj) + source;

		out_imageSource = imageSource.PointV4F32();

		return true;
	}
	else
	{
		Ak3DVector diffractionPt;
		if (m_diffractionEdge->FindDiffractionPt(Ak3DVector(in_listener), Ak3DVector(in_emitter)- Ak3DVector(in_listener), diffractionPt))
		{
			out_imageSource = diffractionPt.PointV4F32();
			return true;
		}

		return false;
	}
}
