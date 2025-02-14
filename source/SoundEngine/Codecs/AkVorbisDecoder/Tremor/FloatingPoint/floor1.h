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
#pragma once

#include "codec_internal.h"

typedef int(*floor1_inverse2_fn) (vorbis_dsp_state *, vorbis_info_floor1 *, ogg_int32_t *buffer, ogg_int32_t *, int);
extern floor1_inverse2_fn floor1_inverse2;

template<class RENDERLINE>
int floor1_inverse2_template(struct vorbis_dsp_state*	vd,
	vorbis_info_floor1*	info,
	ogg_int32_t*		fit_value,
	ogg_int32_t*		out,
	int in_end)
{
	codec_setup_info* ci = vd->csi;
	int n = ci->blocksizes[vd->state.W] / 2;

	if (!fit_value)
	{
		AkZeroMemLarge(out, sizeof(*out)*n);
		return(0);
	}

	ogg_int32_t* pEnd = out + in_end;

	// render the lines
	int hx = 0;
	int lx = 0;
	int ly = fit_value[0] * info->mult;
	for (int j = 1;j < info->posts && out < pEnd;j++)
	{
		int current = info->forward_index[j];
		int hy = fit_value[current];
		if (hy < 0x8000)
		{
			hy *= info->mult;
			hx = info->postlist[current];
			RENDERLINE::render_line(lx, hx, ly, hy, out, pEnd);

			ly = hy;
			lx = hx;
		}
	}

	return(1);
}
