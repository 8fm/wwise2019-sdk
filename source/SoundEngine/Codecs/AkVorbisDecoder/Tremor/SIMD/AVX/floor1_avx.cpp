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

#include "../FloatingPoint/floor1.h"
#include <AK/SoundEngine/Platforms/SSE/AkSimdAvx.h>
struct floor_AVX
{
	static AkForceInline void render_line(int x0, int x1, int y0, int y1, ogg_int32_t *&out, ogg_int32_t *in_pEnd)
	{
		int len = x1 - x0;
		const float dy = y1 - y0;
		const float adx = x1 - x0;
		const float slope = dy / adx;
		const float fy0 = y0;

		ogg_int32_t* pEnd = AkMin(out + len, in_pEnd);

		const AKSIMD_V8F32 eight = AKSIMD_SET_V8F32(8.f);
		AKSIMD_V8F32 fx = { 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f };

		const AKSIMD_V8F32 a = AKSIMD_SET_V8F32(slope*0.0908339713445762);
		const AKSIMD_V8F32 b = AKSIMD_SET_V8F32(-23.16266269286690 + fy0*0.0908339713445762);
		const AKSIMD_V8F32 c = AKSIMD_SET_V8F32(slope*-373463.931103);
		const AKSIMD_V8F32 d = AKSIMD_SET_V8F32(1.1000539433507213e9 + fy0*-373463.9311030);
		const AKSIMD_V8F32 e = AKSIMD_SET_V8F32(slope *-3.90516593047587e-10);
		const AKSIMD_V8F32 f = AKSIMD_SET_V8F32(1.161016522989446e-7 - 3.90516593047587e-10*fy0);
		const AKSIMD_V8F32 g = AKSIMD_SET_V8F32(4.299235046832561e-9);	
		const AKSIMD_V8F32 h = AKSIMD_SET_V8F32(1.250010863763456e7);
#ifdef __clang__
#pragma unroll (2)
#endif
		while (out < pEnd)
		{
			//float w = (float)(int)(fx*a + b);
			AKSIMD_V8F32 w = AKSIMD_MADD_V8F32(fx, a, b);
			w = AKSIMD_CEIL_V8F32(w);

			//float y = c*fx + d + 1.0 / (g* w + e*fx + f) + h*w;
			AKSIMD_V8F32 tmp1 = AKSIMD_MADD_V8F32(fx, e, f);
			AKSIMD_V8F32 denum = AKSIMD_MADD_V8F32(w, g, tmp1);	// g* w + (e*fx + f)
			AKSIMD_V8F32 tmp2 = AKSIMD_MADD_V8F32(fx, c, d);
			AKSIMD_V8F32 y = AKSIMD_MADD_V8F32(w, h, tmp2); // h*w + (c*fx + d)
			y = AKSIMD_ADD_V8F32(y, AKSIMD_RECIP_V8F32(denum));

			AKSIMD_STORE_V8I32((AKSIMD_V8I32*)out, AKSIMD_CONVERT_V8F32_TO_V8I32(y));

			fx = AKSIMD_ADD_V8F32(fx, eight);
			out += 8;
		}
		out = pEnd;
	}
};

void test_render_line_AVX(int x0, int x1, int y0, int y1, ogg_int32_t *&out, ogg_int32_t *in_pEnd)
{
	return floor_AVX::render_line(x0, x1, y0, y1, out, in_pEnd);
};


int floor1_inverse2_AVX(struct vorbis_dsp_state* vd,
	vorbis_info_floor1*	info,
	ogg_int32_t*		fit_value,
	ogg_int32_t*		out,
	int in_end)
{
	return floor1_inverse2_template<floor_AVX>(vd,info,fit_value,out, in_end);
}
