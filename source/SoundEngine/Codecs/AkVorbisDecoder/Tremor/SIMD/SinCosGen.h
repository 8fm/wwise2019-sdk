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


#include <AK/SoundEngine/Common/AkSimd.h>
#if defined( AKSIMD_V4F32_SUPPORTED )

#define PI  3.14159265f
#define SINCOS_TABLE_LEN 9 
#define STEP7_TABLE_LEN 7

//Redefine this on some platforms that don't perform well with MADD.
#define SINCOS_MADD(A, B, C) AKSIMD_MADD_V4F32(A, B, C)
#define SINCOS_MSUB(A, B, C) AKSIMD_MSUB_V4F32(A, B, C)


//SIMD Sine and Cosine generator.  Used to replace fsincos_lookup in the original Tremor mdct.
//This is using the second order resonant filter method described by in "Faster Math Functions" of Robin Green @ Sony Computer Entertainment America
class SinCosGenerator
{
public:
	SinCosGenerator(){};
	AkForceInline void InitFwd(const int bitstep, int const offset)
	{
		cb = ((const AKSIMD_V4F32*)s_cb)[bitstep+1];
		s1 = ((const AKSIMD_V4F32*)s_s1)[offset*SINCOS_TABLE_LEN + bitstep];
		s2 = ((const AKSIMD_V4F32*)s_s2)[offset*SINCOS_TABLE_LEN + bitstep];
		c1 = ((const AKSIMD_V4F32*)s_c1)[offset*SINCOS_TABLE_LEN + bitstep];
		c2 = ((const AKSIMD_V4F32*)s_c2)[offset*SINCOS_TABLE_LEN + bitstep];
	}

	AkForceInline void InitStep7(const int step7_bitstep)
	{
		cb = ((const AKSIMD_V4F32*)s_cb)[step7_bitstep];
		s1 = ((const AKSIMD_V4F32*)s_step7_s1)[step7_bitstep];
		s2 = ((const AKSIMD_V4F32*)s_step7_s2)[step7_bitstep];
		c1 = ((const AKSIMD_V4F32*)s_step7_c1)[step7_bitstep];
		c2 = ((const AKSIMD_V4F32*)s_step7_c2)[step7_bitstep];
	}

	AkForceInline void InitReverse(const int bitstep)
	{		
		cb = ((const AKSIMD_V4F32*)s_cb)[bitstep + 1];
		s1 = ((const AKSIMD_V4F32*)s_s1Rev)[bitstep];
		s2 = ((const AKSIMD_V4F32*)s_s2Rev)[bitstep];
		c1 = ((const AKSIMD_V4F32*)s_c1Rev)[bitstep];
		c2 = ((const AKSIMD_V4F32*)s_c2Rev)[bitstep];
	}

	AkForceInline void InitFwdSingle(int bitstep)
	{		
		cb = ((const AKSIMD_V4F32*)s_cb)[bitstep];
		bitstep--;
		s1 = AKSIMD_LOAD1_V4F32(((float*)s_s1)[bitstep*4]);
		s2 = AKSIMD_LOAD1_V4F32(((float*)s_s2)[bitstep*4]);
		c1 = AKSIMD_LOAD1_V4F32(((float*)s_c1)[bitstep*4]);
		c2 = AKSIMD_LOAD1_V4F32(((float*)s_c2)[bitstep*4]);
	}

	AkForceInline void Next(AKSIMD_V4F32 &s, AKSIMD_V4F32 &c)
	{
		s = SINCOS_MSUB(cb, s1, s2);
		c = SINCOS_MSUB(cb, c1, c2);
		s2 = s1;
		c2 = c1;
		s1 = s;
		c1 = c;
	}	

private:
	AKSIMD_V4F32 cb;	//Filter coefficient
	AKSIMD_V4F32 s2;	//Second previous sine value
	AKSIMD_V4F32 s1;	//Previous sine value
	AKSIMD_V4F32 c2;	//Second previous cosine value
	AKSIMD_V4F32 c1;	//Previous cosine value

	//All tables indexed by bit depth of the number of iterations (i.e. if you divide the 1/8 cycle in 16 points, the bit depth is 4 (1 << 4 = 16)
	static const AK_ALIGN_SIMD(float s_cb[SINCOS_TABLE_LEN + 1][4]);	//Filter coefficient
	static const AK_ALIGN_SIMD(float s_s1[SINCOS_TABLE_LEN * 2][4]);	//Starting points for previous sine iteration.  Second half of table is with offset of one iteration.
	static const AK_ALIGN_SIMD(float s_s2[SINCOS_TABLE_LEN * 2][4]);	//Starting points for second previous sine iteration.  Second half of table is with offset of one iteration.
	static const AK_ALIGN_SIMD(float s_c1[SINCOS_TABLE_LEN * 2][4]); //Starting points for previous sine iteration.  Second half of table is with offset of one iteration.
	static const AK_ALIGN_SIMD(float s_c2[SINCOS_TABLE_LEN * 2][4]); //Starting points for second previous sine iteration.  Second half of table is with offset of one iteration.
	
	//Step 7 and Step 8 start at an offset of half the delta between each iteration.
	static const AK_ALIGN_SIMD(float s_step7_s1[STEP7_TABLE_LEN][4]);
	static const AK_ALIGN_SIMD(float s_step7_s2[STEP7_TABLE_LEN][4]);
	static const AK_ALIGN_SIMD(float s_step7_c1[STEP7_TABLE_LEN][4]);
	static const AK_ALIGN_SIMD(float s_step7_c2[STEP7_TABLE_LEN][4]);

	//Starting points to go in reverse from 45 degrees to 0.
	static const AK_ALIGN_SIMD(float s_s1Rev[SINCOS_TABLE_LEN][4]);
	static const AK_ALIGN_SIMD(float s_s2Rev[SINCOS_TABLE_LEN][4]);
	static const AK_ALIGN_SIMD(float s_c1Rev[SINCOS_TABLE_LEN][4]);
	static const AK_ALIGN_SIMD(float s_c2Rev[SINCOS_TABLE_LEN][4]);
};
#endif