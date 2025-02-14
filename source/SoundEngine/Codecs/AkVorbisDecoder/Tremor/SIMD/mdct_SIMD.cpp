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
// mdct_SIMD.cpp
//
// MDCT implementation in SIMD
//
//
// By mjean
//////////////////////////////////////////////////////////////////////

#include <AK/AkPlatforms.h>
 
#include <AK/SoundEngine/Common/AkSimd.h>
#include <AK/Tools/Common/AkAssert.h>
#include <AK/Tools/Common/AkPlatformFuncs.h>

#if TARGET_IPHONE_SIMULATOR
#include "../FloatingPoint/mdct.cpp"
#elif defined( AKSIMD_V4F32_SUPPORTED )

#include "SinCosGen.h"

// #define cPI3_8 (0x30fbc54d) -> 0.38268343237353648635257803199467 = cos(3 * pi / 8)
#define CosThreePiOverEight	0.38268343237353648635257803199467f

// #define cPI2_8 (0x5a82799a) -> 0.70710678152139614407038136574923 = cos(2 * pi / 8)
#define CosPiOverFour	0.70710678152139614407038136574923f

// #define cPI1_8 (0x7641af3d) -> 0.92387953303934984516322139891014 = cos(pi / 8)
#define CosPiOverEight	0.92387953303934984516322139891014f

//9 bits is the maximum we'll get
/*
//8 bit numbers bit-reversed, shifted by 1.
static unsigned short g_rev8[256] = {0,256,128,384,64,320,192,448,32,288,160,416,96,352,224,480,16,272,144,400,80,336,208,464,48,304,176,432,112,368,240,496,8,264,136,392,72,328,200,456,40,296,168,424,104,360,232,488,24,280,152,408,88,344,216,472,56,312,184,440,120,376,248,504,4,260,132,388,68,324,196,452,36,292,164,420,100,356,228,484,20,276,148,404,84,340,212,468,52,308,180,436,116,372,244,500,12,268,140,396,76,332,204,460,44,300,172,428,108,364,236,492,28,284,156,412,92,348,220,476,60,316,188,444,124,380,252,508,2,258,130,386,66,322,194,450,34,290,162,418,98,354,226,482,18,274,146,402,82,338,210,466,50,306,178,434,114,370,242,498,10,266,138,394,74,330,202,458,42,298,170,426,106,362,234,490,26,282,154,410,90,346,218,474,58,314,186,442,122,378,250,506,6,262,134,390,70,326,198,454,38,294,166,422,102,358,230,486,22,278,150,406,86,342,214,470,54,310,182,438,118,374,246,502,14,270,142,398,78,334,206,462,46,302,174,430,110,366,238,494,30,286,158,414,94,350,222,478,62,318,190,446,126,382,254,510};
AkForceInline int bitrev9(int x)
{
	return g_rev8[x&0xFF] | (x >> 8);
}*/

//This is the most optimal version, but the gain over the version above is minimal, but cost 1024 bytes instead of 512 bytes
const unsigned short g_rev9[512] = {0,256,128,384,64,320,192,448,32,288,160,416,96,352,224,480,16,272,144,400,80,336,208,464,48,304,176,432,112,368,240,496,8,264,136,392,72,328,200,456,40,296,168,424,104,360,232,488,24,280,152,408,88,344,216,472,56,312,184,440,120,376,248,504,4,260,132,388,68,324,196,452,36,292,164,420,100,356,228,484,20,276,148,404,84,340,212,468,52,308,180,436,116,372,244,500,12,268,140,396,76,332,204,460,44,300,172,428,108,364,236,492,28,284,156,412,92,348,220,476,60,316,188,444,124,380,252,508,2,258,130,386,66,322,194,450,34,290,162,418,98,354,226,482,18,274,146,402,82,338,210,466,50,306,178,434,114,370,242,498,10,266,138,394,74,330,202,458,42,298,170,426,106,362,234,490,26,282,154,410,90,346,218,474,58,314,186,442,122,378,250,506,6,262,134,390,70,326,198,454,38,294,166,422,102,358,230,486,22,278,150,406,86,342,214,470,54,310,182,438,118,374,246,502,14,270,142,398,78,334,206,462,46,302,174,430,110,366,238,494,30,286,158,414,94,350,222,478,62,318,190,446,126,382,254,510,1,257,129,385,65,321,193,449,33,289,161,417,97,353,225,481,17,273,145,401,81,337,209,465,49,305,177,433,113,369,241,497,9,265,137,393,73,329,201,457,41,297,169,425,105,361,233,489,25,281,153,409,89,345,217,473,57,313,185,441,121,377,249,505,5,261,133,389,69,325,197,453,37,293,165,421,101,357,229,485,21,277,149,405,85,341,213,469,53,309,181,437,117,373,245,501,13,269,141,397,77,333,205,461,45,301,173,429,109,365,237,493,29,285,157,413,93,349,221,477,61,317,189,445,125,381,253,509,3,259,131,387,67,323,195,451,35,291,163,419,99,355,227,483,19,275,147,403,83,339,211,467,51,307,179,435,115,371,243,499,11,267,139,395,75,331,203,459,43,299,171,427,107,363,235,491,27,283,155,411,91,347,219,475,59,315,187,443,123,379,251,507,7,263,135,391,71,327,199,455,39,295,167,423,103,359,231,487,23,279,151,407,87,343,215,471,55,311,183,439,119,375,247,503,15,271,143,399,79,335,207,463,47,303,175,431,111,367,239,495,31,287,159,415,95,351,223,479,63,319,191,447,127,383,255,511};
AkForceInline int bitrev9(const int x)
{
	return g_rev9[x];
}

//Redefine this on some platforms that don't perform well with MADD.
#define AK_MADD(A, B, C) AKSIMD_MADD_V4F32(A, B, C)
#define AK_MSUB(A, B, C) AKSIMD_MSUB_V4F32(A, B, C)

//#define AK_MADD(A, B, C) AKSIMD_ADD_V4F32(AKSIMD_MUL_V4F32(A, B), C)
//#define AK_MSUB(A, B, C) AKSIMD_SUB_V4F32(AKSIMD_MUL_V4F32(A, B), C)

//SIMDRotate takes a 4x4 matrix (4 independent vectors) and rotates it 90 degrees.
#if defined AK_CPU_ARM_NEON
AkForceInline void SIMDRotate(AKSIMD_V4F32 &A, AKSIMD_V4F32 &B, AKSIMD_V4F32 &C, AKSIMD_V4F32 &D)
{
	float32x4x2_t vfTemp0 = vtrnq_f32( A, B );
	float32x4x2_t vfTemp1 = vtrnq_f32( C, D );	

	A = AKSIMD_SHUFFLE_V4F32( vfTemp0.val[0], vfTemp1.val[0], AKSIMD_SHUFFLE(1,0,1,0) );
	B = AKSIMD_SHUFFLE_V4F32( vfTemp0.val[1], vfTemp1.val[1], AKSIMD_SHUFFLE(1,0,1,0) );
	C = AKSIMD_SHUFFLE_V4F32( vfTemp0.val[0], vfTemp1.val[0], AKSIMD_SHUFFLE(3,2,3,2) );
	D = AKSIMD_SHUFFLE_V4F32( vfTemp0.val[1], vfTemp1.val[1], AKSIMD_SHUFFLE(3,2,3,2) );
}
#else //#if defined( AK_CPU_X86 ) || defined( AK_CPU_X86_64 )
AkForceInline void SIMDRotate(AKSIMD_V4F32 &A, AKSIMD_V4F32 &B, AKSIMD_V4F32 &C, AKSIMD_V4F32 &D)
{
	AKSIMD_V4F32 tmp4, tmp5, tmp6, tmp7;
	//Actarus transform...
	tmp4 = AKSIMD_MOVELH_V4F32(A, B);
	tmp5 = AKSIMD_MOVEHL_V4F32(B, A);
	tmp6 = AKSIMD_MOVELH_V4F32(C, D);
	tmp7 = AKSIMD_MOVEHL_V4F32(D, C);

	A = AKSIMD_SHUFFLE_V4F32(tmp4, tmp6, AKSIMD_SHUFFLE(2,0,2,0));
	B = AKSIMD_SHUFFLE_V4F32(tmp4, tmp6, AKSIMD_SHUFFLE(3,1,3,1));
	C = AKSIMD_SHUFFLE_V4F32(tmp5, tmp7, AKSIMD_SHUFFLE(2,0,2,0));
	D = AKSIMD_SHUFFLE_V4F32(tmp5, tmp7, AKSIMD_SHUFFLE(3,1,3,1));
}
#endif

#define SIMDRead4(p, step, A, B, C, D)\
	A = AKSIMD_LOAD_V4F32(p);	\
	B = AKSIMD_LOAD_V4F32(p+step);	\
	C = AKSIMD_LOAD_V4F32(p+2*step);	\
	D = AKSIMD_LOAD_V4F32(p+3*step);

#define SIMD_ffXPROD31(a, b, t, v, x, y) \
	x = AK_MADD(a, t, AKSIMD_MUL_V4F32(b, v));	\
	y = AK_MSUB(b, t, AKSIMD_MUL_V4F32(a, v));	

#define SIMD_ffXNPROD31(a, b, t, v, x, y) \
	x = AK_MSUB(a, t, AKSIMD_MUL_V4F32(b, v)); \
	y = AK_MADD(b, t, AKSIMD_MUL_V4F32(a, v));

#define BUTTERFLY_STAGE_XN(_s1ar, _s1ai, _s1br, _s1bi, _symIncAr, _symIncAi, _symIncBr, _symIncBi, _y1ar, _y1ai, _y1br, _y1bi, _y3ar, _y3ai, _y3br, _y3bi)\
	_y3ar	= AKSIMD_ADD_V4F32(_symIncAr, _symIncAi);\
	r0		= AKSIMD_SUB_V4F32(_symIncAr, _symIncAi);\
	_y3br	= AKSIMD_ADD_V4F32(_symIncBr, _symIncBi);\
	r1		= AKSIMD_SUB_V4F32(_symIncBr, _symIncBi);\
	_y3ai	= AKSIMD_ADD_V4F32(_s1ai, _s1ar);\
	r2		= AKSIMD_SUB_V4F32(_s1ar, _s1ai);\
	_y3bi	= AKSIMD_ADD_V4F32(_s1bi, _s1br);\
	r3		= AKSIMD_SUB_V4F32(_s1bi, _s1br);\
	SIMD_ffXNPROD31(r0, r1, Tr, Ti, _y1ar, _y1br);\
	SIMD_ffXNPROD31(r3, r2, Tr, Ti, _y1ai, _y1bi);

#define BUTTERFLY_STAGE_X(_s2ar, _s2ai, _s2br, _s2bi, _symIncAr, _symIncAi, _symIncBr, _symIncBi, _y2ar, _y2ai, _y2br, _y2bi, _y4ar, _y4ai, _y4br, _y4bi)\
	_y4ar	= AKSIMD_ADD_V4F32(_symIncAr, _symIncAi);\
	r0		= AKSIMD_SUB_V4F32(_symIncAr, _symIncAi);\
	_y4br	= AKSIMD_ADD_V4F32(_symIncBr, _symIncBi);\
	r1		= AKSIMD_SUB_V4F32(_symIncBi, _symIncBr);\
	_y4ai	= AKSIMD_ADD_V4F32(_s2ai, _s2ar);\
	r2		= AKSIMD_SUB_V4F32(_s2ai, _s2ar);\
	_y4bi	= AKSIMD_ADD_V4F32(_s2bi, _s2br);\
	r3		= AKSIMD_SUB_V4F32(_s2bi, _s2br);\
	SIMD_ffXPROD31(r1, r0, TEr, TEi, _y2ar, _y2br);\
	SIMD_ffXPROD31(r2, r3, TEr, TEi, _y2ai, _y2bi);


#define LoadAndSplitPair(_p, A, B) \
	A = AKSIMD_LOAD_V4F32(_p); \
	B = AKSIMD_SPLAT_V4F32(A, 1); \
	A = AKSIMD_SPLAT_V4F32(A, 0);

/* N/stage point generic N stage butterfly */
AkForceInline void mdct_butterfly_stage_SIMD_Ex(float *x, int points, int shift)
{
	AKSIMD_V4F32	*x1 = (AKSIMD_V4F32 *)(x + points) - 4;	// Starting from the end
	AKSIMD_V4F32	*x2 = (AKSIMD_V4F32 *)(x + points / 2) - 4;	// Starting from the middle

	// We simultaneously navigate the 1st 1/4 of a sine and cosine during the loop below.
	// Sine:   | S--> | ---- | ---- | ---- |
	// Cosine: | C--> | ---- | ---- | ---- |
	// each with a period of 4 * ((N/2) / 4)
	AKSIMD_V4F32 TEr, TEi;
	SinCosGenerator sincos;
	sincos.InitFwdSingle(shift);

	AKSIMD_V4F32 r0, r1, r2, r3;

	// One loop computes 4 values per half
	// | ------ | ------ |
	// | <---x2 | <---x1 |
	do // (N/2) / 4 iterations
	{
		sincos.Next(TEr, TEi);

		r0		= AKSIMD_SUB_V4F32(x1[0], x1[1]);
		x1[0]	= AKSIMD_ADD_V4F32(x1[0], x1[1]);
		r1		= AKSIMD_SUB_V4F32(x1[3], x1[2]);
		x1[2]	= AKSIMD_ADD_V4F32(x1[2], x1[3]);
		r2		= AKSIMD_SUB_V4F32(x2[1], x2[0]);
		x1[1]	= AKSIMD_ADD_V4F32(x2[1], x2[0]);
		r3		= AKSIMD_SUB_V4F32(x2[3], x2[2]);
		x1[3]	= AKSIMD_ADD_V4F32(x2[3], x2[2]);

		SIMD_ffXPROD31(r1, r0, TEr, TEi, x2[0], x2[2]);
		SIMD_ffXPROD31(r2, r3, TEr, TEi, x2[1], x2[3]);

		x1 -= 4;
		x2 -= 4;
	} while (x2 >= (AKSIMD_V4F32*)x);
}


/*
This does the same thing as bitreverse and also ungroup the SIMD grouping done in mdct_presymmetry_plus_2stages_group.
There is a nice side effect of grouping independent blocks that are n/4 away.  It does already a part of the bit reversal.
Here is an example on a 32 point data set.  This is the first grouped block of 4 is comprised from the first item from each sub n/4 block (index 0, 8, 16, 24)
Check the difference, after the bit reverse operation on an ungrouped input, only the middle two are swapped.  This is true for ALL groups of 4.
Then the WHOLE group needs to be moved at the right place, bit-reversed on the remaining bits (log(n/4))
On top of that, since the grouping is done on independent blocks, we can order them in anyway we want at the beginning.  So we can swap the middle 2 at the grouping stage.
Input		Reversed	Grouped	Input
0	00000	0	00000	0	00000
1	00001	16	10000	8	01000
2	00010	8	01000	16	10000
3	00011	24	11000	24	11000
...			...			...
In this example, only 0 and 8 need to be considered for bit reversal, the other two should be put straight to their normal position.
*/

//Platform specific shuffles.  These should be redefined for each platform.
#if defined( AK_CPU_X86 ) || defined( AK_CPU_X86_64 )
#define AKSIMD_SHUFFLE_DBCA( __a__ ) _mm_shuffle_ps( (__a__), (__a__), _MM_SHUFFLE(0,2,1,3))
#define AKSIMD_SHUFFLE_ACBD( __a__ ) _mm_shuffle_ps( (__a__), (__a__), _MM_SHUFFLE(3,1,2,0))
#define AKSIMD_SHUFFLE_DCBA( __a__ ) _mm_shuffle_ps( (__a__), (__a__), _MM_SHUFFLE(0,1,2,3))

#elif defined AK_CPU_ARM_NEON

AkForceInline AKSIMD_V4F32 AKSIMD_SHUFFLE_ACBD(const AKSIMD_V4F32& rA)
{
	AKSIMD_V4F32 A = rA;
	float t = AKSIMD_GETELEMENT_V4F32(A,2);
	AKSIMD_GETELEMENT_V4F32(A,2) = AKSIMD_GETELEMENT_V4F32(A,1);
	AKSIMD_GETELEMENT_V4F32(A,1) = t;
	return A;
}

/*AkForceInline AKSIMD_V4F32 AKSIMD_SHUFFLE_ACBD(const AKSIMD_V4F32& A)
{
	AKSIMD_V2F32 ab = vget_low_f32( A ); 
	AKSIMD_V2F32 cd = vget_high_f32( A );

	float32x2x2_t acbd = vzip_f32(ab, cd);
	return vcombine_f32(acbd.val[0], acbd.val[1]);
}*/

AkForceInline AKSIMD_V4F32 AKSIMD_SHUFFLE_DBCA(const AKSIMD_V4F32& rA)
{
	AKSIMD_V4F32 A = rA;
	float t = AKSIMD_GETELEMENT_V4F32(A,0);
	AKSIMD_GETELEMENT_V4F32(A,0) = AKSIMD_GETELEMENT_V4F32(A,3);
	AKSIMD_GETELEMENT_V4F32(A,3) = t;
	return A;
}

/*
AkForceInline AKSIMD_V4F32 AKSIMD_SHUFFLE_DBCA(const AKSIMD_V4F32& A)
{
	AKSIMD_V4F32 rev = vrev64q_f32( A);
	AKSIMD_V2F32 ba = vget_low_f32( rev ); 
	AKSIMD_V2F32 dc = vget_high_f32( rev );

	float32x2x2_t dbca = vzip_f32(dc, ba);
	return vcombine_f32(dbca.val[0], dbca.val[1]);
}*/

AkForceInline AKSIMD_V4F32 AKSIMD_SHUFFLE_DCBA(const AKSIMD_V4F32& A)
{
	AKSIMD_V4F32 rev = vrev64q_f32( A);
	return vcombine_f32(vget_high_f32( rev ), vget_low_f32( rev ));
}
#endif

void mdct_reverse_step7_step8(float *x,int n,int shift, float *pOut)
{
	AKSIMD_V4F32 w0r;	//Real parts from beginning
	AKSIMD_V4F32 w0i;	//Imaginary parts from beginning
	AKSIMD_V4F32 w1r;	//Real parts from end
	AKSIMD_V4F32 w1i;	//Imaginary parts from end

	n = n >>1; 

	SinCosGenerator sincos;
	sincos.InitStep7(shift+1);

	SinCosGenerator sincosStep8;
	sincosStep8.InitStep7(shift-1);

	AKSIMD_V4F32 *pIn = (AKSIMD_V4F32*)x;
	AKSIMD_V4F32 *pO = (AKSIMD_V4F32*)pOut;
	int n2 = n / 8;
	int idx0 = 0;			//Index from the beginning
	int idx1 = n2 - 1;	//Index from the end (in blocks to process, therefore blocks of 4, but only half of them)
	int idxOut = n /8 -1;

	AKSIMD_V4F32 fr0,fr1;
	AKSIMD_V4F32 fr2,fr3;
	AKSIMD_V4F32 Tr, Ti, T8r, T8i;
	const AKSIMD_DECLARE_V4F32(vHalf, 0.5f, 0.5f, 0.5f, 0.5f);
	const AKSIMD_DECLARE_V4F32(vSin45, CosPiOverFour, CosPiOverFour, CosPiOverFour, CosPiOverFour);	

	//Compared to the scalar version, we output between 1 and -1.  This 256 factor comes from the q_del factor in the codebook vectors expansion.  
	//It was necessary because of fixed-point processing (allowed 8 bits of decimals)... might not be anymore.
	const  AKSIMD_DECLARE_V4F32(vQuantizeFactor, 1.f/256.f, 1.f / 256.f, 1.f / 256.f, 1.f / 256.f);

	int significant = shift-1;	//"shift" is the number of bits remaining from the max supported in this algo.  
	//We support windows of 2048.  Divided by blocks of 4, it means only 512 (9 bits)

	do 
	{
		//Read w0 from its reversed position
		int src0 = bitrev9(idx1) >> significant;
		w0r = pIn[src0];
		w0i = pIn[src0+1];

		//Swp the w1 items since reading from the end should make the w0[0] interact with w1[1]
		w0r = AKSIMD_SHUFFLE_DBCA(w0r);
		w0i = AKSIMD_SHUFFLE_DBCA(w0i);		

		//Read w1 from its reversed position
		int src1 = bitrev9(idx0) >> significant;
		w1r = pIn[src1];
		w1i = pIn[src1+1];

		w1r = AKSIMD_SHUFFLE_ACBD(w1r);
		w1i = AKSIMD_SHUFFLE_ACBD(w1i);

		sincos.Next(Tr, Ti);

		fr0 = AKSIMD_ADD_V4F32(w0r, w1r);	//fr0		= (w0[0] + w1[0]);
		fr1 = AKSIMD_SUB_V4F32(w1i, w0i);	//fr1		= (w1[1] - w0[1]);
		fr2 = AKSIMD_MUL_V4F32(AK_MADD(fr0, Ti, AKSIMD_MUL_V4F32(fr1, Tr)), vHalf);	//fr2		= (fr0 * T[1] + fr1 * T[0]) * 0.5f;
		fr3 = AKSIMD_MUL_V4F32(AK_MSUB(fr1, Ti, AKSIMD_MUL_V4F32(fr0, Tr)), vHalf);	//fr3		= (fr1 * T[1] - fr0 * T[0]) * 0.5f;

		fr0 = AKSIMD_MUL_V4F32(AKSIMD_ADD_V4F32(w0i, w1i), vHalf); //fr0     = (w0[1] + w1[1]) * 0.5f;
		fr1 = AKSIMD_MUL_V4F32(AKSIMD_SUB_V4F32(w0r, w1r), vHalf); //fr1     = (w0[0] - w1[0]) * 0.5f;

		w0r = AKSIMD_ADD_V4F32(fr0, fr2);	//w0[0]  = (fr0 + fr2);
		w0i = AKSIMD_ADD_V4F32(fr1, fr3);   //w0[1]  = (fr1 + fr3);
		w1r = AKSIMD_SUB_V4F32(fr0, fr2);   //w1[0]  = (fr0 - fr2);
		w1i = AKSIMD_SUB_V4F32(fr3, fr1);   //w1[1]  = (fr3 - fr1);	

		//Step 8
		sincosStep8.Next(Tr, Ti);
		w0i = AKSIMD_NEG_V4F32(w0i);	//float	fr1	= -x[1];
		SIMD_ffXPROD31(w0r, w0i, Tr, Ti, fr0, fr1);			//ffXPROD31( fr0, fr1, T[0], T[1], x, x+1);

		//Rescale (previously done in mdct_unroll_lap)
		fr0 = AKSIMD_MUL_V4F32(fr0, vQuantizeFactor);
		fr1 = AKSIMD_MUL_V4F32(fr1, vQuantizeFactor);

		//swap back the A & B complex.
		AKSIMD_STORE_V4F32(pO+idx0, fr0);
		AKSIMD_STORE_V4F32(pO+idx0+n/8, fr1);

		w1i = AKSIMD_NEG_V4F32(w1i);	//float	fr1	= -x[1];
		//Compute mirrored sin and cos from other (saves 4 instruction compared to using a reversed sincos generator)
		T8r = AKSIMD_MUL_V4F32(AKSIMD_SUB_V4F32(Ti, Tr), vSin45);
		T8i = AKSIMD_MUL_V4F32(AKSIMD_ADD_V4F32(Ti, Tr), vSin45);
		SIMD_ffXPROD31(w1r, w1i, T8r, T8i, fr0, fr1);			//ffXPROD31( fr0, fr1, T[0], T[1], x, x+1);		

		fr0 = AKSIMD_SHUFFLE_DCBA(fr0);
		fr1 = AKSIMD_SHUFFLE_DCBA(fr1);

		//Rescale (previously done in mdct_unroll_lap)
		fr0 = AKSIMD_MUL_V4F32(fr0, vQuantizeFactor);
		fr1 = AKSIMD_MUL_V4F32(fr1, vQuantizeFactor);

		AKSIMD_STORE_V4F32(pO+idxOut, fr0);
		AKSIMD_STORE_V4F32(pO+idxOut+n/8, fr1);

		idx0++;
		idx1--;	
		idxOut--;
	} while (idx0 < idx1);	
}

#define READ_CPLX(_i, _j) \
	AKSIMD_V4F32 x##_i = AKSIMD_LOAD_V4F32(x+_i);	\
	AKSIMD_V4F32 x##_j = AKSIMD_LOAD_V4F32(x+_i+1);
	
#define COMPUTE_SUM_AND_DIFF(_i, _j, _name)	\
	AKSIMD_V4F32 s##_name = AKSIMD_ADD_V4F32(_i,_j);	\
	AKSIMD_V4F32 d##_name = AKSIMD_SUB_V4F32(_i,_j);	\

#define SPLIT_AND_STORE16() \
	AKSIMD_STORE_V4F32(x++, evenA); \
	AKSIMD_STORE_V4F32(x++, oddA); \
    AKSIMD_STORE_V4F32(x++, evenB); \
	AKSIMD_STORE_V4F32(x++, oddB); 

//This step does what butterfly16 and butterfly8 does in the original code.
AkForceInline void mdct_butterfly_16_combinedx4(float * pIn, int base, int n)
{
	AKSIMD_V4F32 * AK_RESTRICT x = (AKSIMD_V4F32 * AK_RESTRICT)pIn;
	READ_CPLX(0, 1);
	READ_CPLX(2, 3);
	READ_CPLX(4, 5);
	READ_CPLX(6, 7);
	READ_CPLX(8, 9);
	READ_CPLX(10, 11);
	READ_CPLX(12, 13);
	READ_CPLX(14, 15);	

	const  AKSIMD_DECLARE_V4F32(K, CosPiOverFour, CosPiOverFour, CosPiOverFour, CosPiOverFour);
	COMPUTE_SUM_AND_DIFF(x0, x1, 0);
	COMPUTE_SUM_AND_DIFF(x2, x3, 2);
	COMPUTE_SUM_AND_DIFF(x4, x5, 4);
	COMPUTE_SUM_AND_DIFF(x6, x7, 6);
	COMPUTE_SUM_AND_DIFF(x8, x9, 8);
	COMPUTE_SUM_AND_DIFF(x10, x11, 10);
	COMPUTE_SUM_AND_DIFF(x12, x13, 12);
	COMPUTE_SUM_AND_DIFF(x14, x15, 14);

	COMPUTE_SUM_AND_DIFF(d8, d10, d8d10);
	COMPUTE_SUM_AND_DIFF(d0, d2, d0d2);

	AKSIMD_V4F32 K0 = AKSIMD_MUL_V4F32(AKSIMD_SUB_V4F32(sd8d10, dd0d2), K);
	AKSIMD_V4F32 K1 = AKSIMD_MUL_V4F32(AKSIMD_SUB_V4F32(AKSIMD_NEG_V4F32(dd8d10), sd0d2), K);
	AKSIMD_V4F32 K2 = AKSIMD_MUL_V4F32(AKSIMD_SUB_V4F32(sd0d2, dd8d10), K);
	AKSIMD_V4F32 K3 = AKSIMD_MUL_V4F32(AKSIMD_SUB_V4F32(AKSIMD_NEG_V4F32(sd8d10), dd0d2), K);

	COMPUTE_SUM_AND_DIFF(d12, d6, d12d6);	
	COMPUTE_SUM_AND_DIFF(d14, d4, d14d4);	
	COMPUTE_SUM_AND_DIFF(s12, s4, s12s4);
	COMPUTE_SUM_AND_DIFF(s14, s6, s14s6);
	COMPUTE_SUM_AND_DIFF(s8, s0, s8s0);
	COMPUTE_SUM_AND_DIFF(s10, s2, s10s2);

	AKSIMD_V4F32 evenA, oddA, evenB, oddB;
	evenA = AKSIMD_ADD_V4F32(sd12d6, K0);//x0
	oddA = AKSIMD_ADD_V4F32(dd14d4, K1);	//x1
	evenB = AKSIMD_SUB_V4F32(sd12d6, K0);//x2
	oddB = AKSIMD_SUB_V4F32(dd14d4, K1);	//x3
	SPLIT_AND_STORE16();

	evenA = AKSIMD_ADD_V4F32(dd12d6, K2);//x4
	oddA = AKSIMD_ADD_V4F32(sd14d4, K3);	//x5
	evenB = AKSIMD_SUB_V4F32(dd12d6, K2);//x6
	oddB = AKSIMD_SUB_V4F32(sd14d4, K3);	//x7
	SPLIT_AND_STORE16();

	evenA = AKSIMD_ADD_V4F32(ds12s4, ds10s2);//x8
	oddA = AKSIMD_SUB_V4F32(ds14s6, ds8s0);	//x9
	evenB = AKSIMD_SUB_V4F32(ds12s4, ds10s2);//x10
	oddB = AKSIMD_ADD_V4F32(ds14s6, ds8s0);	//x11
	SPLIT_AND_STORE16();

	evenA = AKSIMD_SUB_V4F32(ss12s4, ss8s0);//x12
	oddA = AKSIMD_SUB_V4F32(ss14s6, ss10s2);	//x13
	evenB = AKSIMD_ADD_V4F32(ss12s4, ss8s0);//x14
	oddB = AKSIMD_ADD_V4F32(ss14s6, ss10s2);	//x15
	SPLIT_AND_STORE16();
}

#define SPLIT_AND_STORE32(offset) \
	AKSIMD_STORE_V4F32(x + offset, even);	\
	AKSIMD_STORE_V4F32(x + offset+1, odd);

AkForceInline void mdct_butterfly_32x4(float *pIn, int base, int n)
{
	AKSIMD_V4F32 * AK_RESTRICT x = (AKSIMD_V4F32 * AK_RESTRICT)pIn;
	AKSIMD_V4F32 r0, r1, r2, r3, even, odd;
	const AKSIMD_DECLARE_V4F32(K3_8, CosThreePiOverEight, CosThreePiOverEight, CosThreePiOverEight, CosThreePiOverEight);
	const AKSIMD_DECLARE_V4F32(K1_8, CosPiOverEight, CosPiOverEight, CosPiOverEight, CosPiOverEight);
	const AKSIMD_DECLARE_V4F32(K1_4, CosPiOverFour, CosPiOverFour, CosPiOverFour, CosPiOverFour);

	READ_CPLX(16, 17);
	READ_CPLX(18, 19);
	READ_CPLX(0, 1);
	READ_CPLX(2, 3);

	r0 = AKSIMD_SUB_V4F32(x16, x17);
	r1 = AKSIMD_SUB_V4F32(x18, x19);
	r2 = AKSIMD_SUB_V4F32(x1, x0);
	r3 = AKSIMD_SUB_V4F32(x3, x2);

	/*r0		= x[16] - x[17];	sr0
	r1		= x[18] - x[19];		sr1
	r2		= x[ 1] - x[ 0];		sr2
	r3		= x[ 3] - x[ 2];		sr3

	x[16]	= x[16] + x[17];
	x[17]	= x[ 1] + x[ 0];
	x[18]	= x[18] + x[19];
	x[19]	= x[ 3] + x[ 2];
	*/
	even = AKSIMD_ADD_V4F32(x16, x17);	//x16
	odd = AKSIMD_ADD_V4F32(x1, x0);		//x17
	SPLIT_AND_STORE32(16);

	even = AKSIMD_ADD_V4F32(x18, x19);	//x18
	odd = AKSIMD_ADD_V4F32(x3, x2);		//x19
	SPLIT_AND_STORE32(18);

	//ffXNPROD31( r0, r1, CosThreePiOverEight, CosPiOverEight, &x[ 0], &x[ 2] );
	//ffXPROD31 ( r2, r3, CosPiOverEight, CosThreePiOverEight, &x[ 1], &x[ 3] );
//	x[ 0] = r0 * CosThreePiOverEight - r1 * CosPiOverEight;
//	x[ 1] = r2 * CosPiOverEight + r3 * CosThreePiOverEight;
//	x[ 2] = r1 * CosThreePiOverEight + r0 * CosPiOverEight;
//  x[ 3] = r3 * CosPiOverEight - r2 * CosThreePiOverEight;
	even = AK_MSUB(r0, K3_8, AKSIMD_MUL_V4F32(r1, K1_8));	//x0
	odd = AK_MADD(r2, K1_8, AKSIMD_MUL_V4F32(r3, K3_8));		//x1
	SPLIT_AND_STORE32(0);

	even = AK_MADD(r1, K3_8, AKSIMD_MUL_V4F32(r0, K1_8));	//x2
	odd = AK_MSUB(r3, K1_8, AKSIMD_MUL_V4F32(r2, K3_8));		//x3
	SPLIT_AND_STORE32(2);

// 	r0		= x[20] - x[21];
// 	r1		= x[22] - x[23];
// 	r2		= x[ 5] - x[ 4];
// 	r3		= x[ 7] - x[ 6];

	READ_CPLX(20, 21);
	READ_CPLX(22, 23);
	READ_CPLX(4, 5);
	READ_CPLX(6, 7);

	r0 = AKSIMD_SUB_V4F32(x20, x21);
	r1 = AKSIMD_SUB_V4F32(x22, x23);
	r2 = AKSIMD_SUB_V4F32(x5, x4);
	r3 = AKSIMD_SUB_V4F32(x7, x6);

// 	x[20]	= x[20] + x[21];
// 	x[21]	= x[ 5] + x[ 4];
// 	x[22]	= x[22] + x[23];
// 	x[23]	= x[ 7] + x[ 6];

	even = AKSIMD_ADD_V4F32(x20, x21);	//x20
	odd = AKSIMD_ADD_V4F32(x5, x4);	//x21
	SPLIT_AND_STORE32(20);

	even = AKSIMD_ADD_V4F32(x22, x23);	//x22
	odd = AKSIMD_ADD_V4F32(x7, x6);		//x23
	SPLIT_AND_STORE32(22);

// 	x[ 4] = (r0 - r1) * CosPiOverFour;
// 	x[ 5] = (r3 + r2) * CosPiOverFour;
// 	x[ 6] = (r0 + r1) * CosPiOverFour;
// 	x[ 7] = (r3 - r2) * CosPiOverFour;

	even = AKSIMD_MUL_V4F32(AKSIMD_SUB_V4F32(r0, r1), K1_4);	//x4
	odd = AKSIMD_MUL_V4F32(AKSIMD_ADD_V4F32(r3, r2), K1_4);		//x5
	SPLIT_AND_STORE32(4);

	even = AKSIMD_MUL_V4F32(AKSIMD_ADD_V4F32(r0, r1), K1_4);	//x6
	odd = AKSIMD_MUL_V4F32(AKSIMD_SUB_V4F32(r3, r2), K1_4);		//x7
	SPLIT_AND_STORE32(6);

// 	r0		= x[24] - x[25];
// 	r1		= x[26] - x[27];
// 	r2		= x[ 9] - x[ 8];
// 	r3		= x[11] - x[10];

	READ_CPLX(24, 25);
	READ_CPLX(26, 27);
	READ_CPLX(8, 9);
	READ_CPLX(10, 11);

	r0 = AKSIMD_SUB_V4F32(x24, x25);
	r1 = AKSIMD_SUB_V4F32(x26, x27);
	r2 = AKSIMD_SUB_V4F32(x9, x8);
	r3 = AKSIMD_SUB_V4F32(x11, x10);

// 	x[24]	= x[24] + x[25];           
// 	x[25]	= x[ 9] + x[ 8];
// 	x[26]	= x[26] + x[27];
// 	x[27]	= x[11] + x[10];

	even = AKSIMD_ADD_V4F32(x24, x25);	//x24
	odd = AKSIMD_ADD_V4F32(x9, x8);		//x25
	SPLIT_AND_STORE32(24);

	even = AKSIMD_ADD_V4F32(x26, x27);	//x26
	odd = AKSIMD_ADD_V4F32(x11, x10);	//x27
	SPLIT_AND_STORE32(26);

	//ffXNPROD31( r0, r1, CosPiOverEight, CosThreePiOverEight, &x[ 8], &x[10] );
	//ffXPROD31 ( r2, r3, CosThreePiOverEight, CosPiOverEight, &x[ 9], &x[11] );
// 	x[ 8] = r0 * CosPiOverEight - r1 * CosThreePiOverEight;
// 	x[ 9] = r2 * CosThreePiOverEight + r3 * CosPiOverEight;
// 	x[10] = r1 * CosPiOverEight + r0 * CosThreePiOverEight;
// 	x[11] = r3 * CosThreePiOverEight - r2 * CosPiOverEight;
	even = AK_MSUB(r0, K1_8, AKSIMD_MUL_V4F32(r1, K3_8));	//x8
	odd = AK_MADD(r2, K3_8, AKSIMD_MUL_V4F32(r3, K1_8));		//x9
	SPLIT_AND_STORE32(8);

	even = AK_MADD(r1, K1_8, AKSIMD_MUL_V4F32(r0, K3_8));	//x10
	odd = AK_MSUB(r3, K3_8, AKSIMD_MUL_V4F32(r2, K1_8));		//x11
	SPLIT_AND_STORE32(10);

// 	r0		= x[28] - x[29];
// 	r1		= x[30] - x[31];
// 	r2		= x[12] - x[13];
// 	r3		= x[15] - x[14];

	READ_CPLX(28, 29);
	READ_CPLX(30, 31);
	READ_CPLX(12, 13);
	READ_CPLX(14, 15);

	r0 = AKSIMD_SUB_V4F32(x28, x29);
	r1 = AKSIMD_SUB_V4F32(x30, x31);
	r2 = AKSIMD_SUB_V4F32(x12, x13);
	r3 = AKSIMD_SUB_V4F32(x15, x14);

// 	x[28]	= x[28] + x[29];           
// 	x[29]	= x[13] + x[12];
// 	x[30]	= x[30] + x[31];
// 	x[31]	= x[15] + x[14];

	even = AKSIMD_ADD_V4F32(x28, x29);	//x28
	odd = AKSIMD_ADD_V4F32(x13, x12);	//x29
	SPLIT_AND_STORE32(28);

	even = AKSIMD_ADD_V4F32(x30, x31);	//x30
	odd = AKSIMD_ADD_V4F32(x15, x14);	//x31
	SPLIT_AND_STORE32(30);

// 	x[12]	= r0;
// 	x[13]	= r3; 
// 	x[14]	= r1;
// 	x[15]	= r2;

	even = r0;	//x12
	odd = r3;	//x13
	SPLIT_AND_STORE32(12);

	even = r1;	//x14
	odd = r2;	//x15
	SPLIT_AND_STORE32(14);

	mdct_butterfly_16_combinedx4(pIn, base, n);
	mdct_butterfly_16_combinedx4(pIn+64, base+16, n);
}

/*
Read 4 vectors and rotate, return in 4 vectors
A0 B0 C0 D0       A0 A1 A2 A3
A1 B1 C1 D1   =\  B0 B1 B2 B3
A2 B2 C2 D2   =/  C0 C1 C2 C3
A3 B3 C3 D3       D0 D1 D2 D3
*/
#ifdef AK_CPU_ARM_NEON
//On ARM, we can use the deinterleaving load instructions to avoid costly shuffles.
#define symIncAr v4Inc.val[0]
#define symIncAi v4Inc.val[1]
#define symIncBr v4Inc.val[2]
#define symIncBi v4Inc.val[3]

#define symDecAr v4Dec.val[0]
#define symDecAi v4Dec.val[1]
#define symDecBr v4Dec.val[2]
#define symDecBi v4Dec.val[3]
#else
AkForceInline void SIMDReadRotate(AKSIMD_V4F32* p, const int step, AKSIMD_V4F32 &A, AKSIMD_V4F32 &B, AKSIMD_V4F32 &C, AKSIMD_V4F32 &D)
{
	SIMDRead4(p, step, A, B, C, D);
	SIMDRotate(A, B, C, D);
}
#endif

#if defined( AK_CPU_X86 ) || defined( AK_CPU_X86_64 )
AkForceInline void SIMDShiftRightAndInsert(AKSIMD_V4F32& A, const AKSIMD_V4F32&B)
{
	AKSIMD_V4F32 TC = AKSIMD_SHUFFLE_V4F32(B, A, AKSIMD_SHUFFLE(3,3,0,0));
	A = AKSIMD_SHUFFLE_V4F32(A, TC, AKSIMD_SHUFFLE(1,2,2,1));
}
#elif defined AK_CPU_ARM_NEON
AkForceInline void SIMDShiftRightAndInsert(AKSIMD_V4F32& abcd, AKSIMD_V4F32& efgh)
{
	//There must be a better way, this is slow...
	/*float32x4_t dcba = vrev64q_f32(abcd);
	float32x2_t dc = vget_high_f32(dcba);
	float32x2x2_t decf = vzip_f32(dc, vget_low_f32(efgh));
	float32x2x2_t bcad = vzip_f32(vget_high_f32(dcba), vget_high_f32(abcd));
	abcd = vcombine_f32(bcad.val[0], decf.val[0]);*/

	/*nt8x8_t maskBC = { 4,5,6,7, 8,9,10,11 };
	uint8x8_t maskDE = { 12,13,14,15, 16,17,18,19 };
	uint8x8x3_t abcdef;
	abcdef.val[0] = vreinterpret_u8_f32(vget_low_f32(abcd));
	abcdef.val[1] = vreinterpret_u8_f32(vget_high_f32(abcd));
	abcdef.val[2] = vreinterpret_u8_f32(vget_low_f32(efgh));
	uint8x8_t bc = vtbl3_u8(abcdef, maskBC);
	uint8x8_t de = vtbl3_u8(abcdef, maskDE);

	abcd = vcombine_f32(vreinterpret_f32_u8(bc), vreinterpret_f32_u8(de));*/

	AKSIMD_GETELEMENT_V4F32(abcd,0) = AKSIMD_GETELEMENT_V4F32(abcd,1);
	AKSIMD_GETELEMENT_V4F32(abcd,1) = AKSIMD_GETELEMENT_V4F32(abcd,2);
	AKSIMD_GETELEMENT_V4F32(abcd,2) = AKSIMD_GETELEMENT_V4F32(abcd,3);
	AKSIMD_GETELEMENT_V4F32(abcd,3) = AKSIMD_GETELEMENT_V4F32(efgh,0);
}
#endif

void mdct_presymmetry_SIMD_Ex(float *x,const int points, const int shift, float *in_floor, const int end)
{
	float *pOut = x;
	float *pRevOut = x + points - 16;	

	AKSIMD_V4F32* pC = (AKSIMD_V4F32*)x;
	AKSIMD_V4F32* pSym = (AKSIMD_V4F32*)(x + points);
	AKSIMD_V4F32* pFloor = (AKSIMD_V4F32*)in_floor;
	AKSIMD_V4F32* pFloorSym = (AKSIMD_V4F32*)(in_floor+points);	

#ifdef AK_CPU_ARM_NEON
	float32x4x4_t v4Inc;
	float32x4x4_t v4Dec;
	float32x4x4_t v4Floor;	
#else
	AKSIMD_V4F32 symIncAr, symIncAi, symIncBr, symIncBi, symDecAr, symDecAi, symDecBr, symDecBi;	
#endif
	AKSIMD_V4F32 Tr, Ti;
	//Symmetry output
	AKSIMD_V4F32 s1ar,s1ai;
	AKSIMD_V4F32 s1br,s1bi;	
	AKSIMD_V4F32 s4ar,s4ai;
	AKSIMD_V4F32 s4br,s4bi; 	

	SinCosGenerator sincos;
	sincos.InitFwd(shift, 0);

	AKSIMD_V4F32 tmpR, tmpI;
	sincos.Next(tmpR, tmpI);	

#ifdef AK_CPU_ARM_NEON
	pSym -= 4;
	pFloorSym -= 4;
#else
	pSym -= 1;
	pFloorSym -= 1;
#endif

	//The spectrum is often truncated at the end.  This because the buffers need to be powers of 2, but the actual spectrum is smaller.
	//The encoder doesn't even bother encoding the last empty partitions.
	//Use this fact to save on the pre-symmetry step, knowing that the right part is made of zeros.
	if (end < points)
	{
		AKASSERT((points - end) % 16 == 0);
		float *pOutZero = x + (points - end);	
		const AKSIMD_V4F32 zero = AKSIMD_SET_V4F32(0.f);
		
		while (pOut < pOutZero)
		{
			Tr = tmpR;
			Ti = tmpI;

#ifdef AK_CPU_ARM_NEON			
			int32x4x4_t v4s32Inc;			
			v4s32Inc = vld4q_s32((int*)pC);			
			v4Floor = vld4q_f32((float*)pFloor);
			v4Inc.val[0] = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(v4s32Inc.val[0]), v4Floor.val[0]);
			v4Inc.val[1] = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(v4s32Inc.val[1]), v4Floor.val[1]);
			v4Inc.val[2] = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(v4s32Inc.val[2]), v4Floor.val[2]);
			v4Inc.val[3] = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(v4s32Inc.val[3]), v4Floor.val[3]);

			//Rightmost part is zeros, don't load.		
#else
			AKSIMD_V4F32 f0, f1, f2, f3;

			//Read, A and B values.  (Non-zero)		
			SIMDRead4(pC, 1, symIncAr, symIncAi, symIncBr, symIncBi);

			//Apply floor
			SIMDRead4(pFloor, 1, f0, f1, f2, f3);
			symIncAr = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(_mm_castps_si128(symIncAr)), f0);
			symIncAi = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(_mm_castps_si128(symIncAi)), f1);
			symIncBr = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(_mm_castps_si128(symIncBr)), f2);
			symIncBi = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(_mm_castps_si128(symIncBi)), f3);

			//Transpose
			SIMDRotate(symIncAr, symIncAi, symIncBr, symIncBi);

			//Do not read from the end, we know they're zeros.			
#endif			
			//s4ai = 0;
			//s4bi = 0;
			SIMD_ffXNPROD31(symIncBr, symIncAr, Ti, Tr, s4ar, s4br);

			//Read twiddles one step further.  We keep 3 of them, and add a new one.		
			sincos.Next(tmpR, tmpI);

			SIMDShiftRightAndInsert(Tr, tmpR);
			SIMDShiftRightAndInsert(Ti, tmpI);

			SIMD_ffXPROD31(symIncAi, symIncBi, Ti, Tr, s1ai, s1bi);			
			//s1ar = 0;
			//s1br = 0;	

			//Store
			AKSIMD_STORE_V4F32(pOut + 0, zero);
			AKSIMD_STORE_V4F32(pOut + 4, s1ai);
			AKSIMD_STORE_V4F32(pOut + 8, zero);
			AKSIMD_STORE_V4F32(pOut + 12, s1bi);
			pOut += 16;

			//Store reversed
			AKSIMD_STORE_V4F32(pRevOut + 0, AKSIMD_SHUFFLE_DCBA(s4ar));
			AKASSERT(pRevOut[4] == 0.f && pRevOut[5] == 0.f && pRevOut[6] == 0.f && pRevOut[7] == 0.f);			
			AKSIMD_STORE_V4F32(pRevOut + 8, AKSIMD_SHUFFLE_DCBA(s4br));
			AKASSERT(pRevOut[12] == 0.f && pRevOut[13] == 0.f && pRevOut[14] == 0.f && pRevOut[15] == 0.f);
			pRevOut -= 16;

			pC += 4;
			pFloor += 4;
		}
		pSym -= (points - end)/4;
		pFloorSym -= (points - end)/4;
	}

	//Do the rest of the symmetry process normally.
	while(pOut < pRevOut)
	{		
		Tr = tmpR;
		Ti = tmpI;

#ifdef AK_CPU_ARM_NEON
		int32x4x4_t v4s32Inc;
		int32x4x4_t v4s32Dec;
		v4s32Inc = vld4q_s32((int*)pC);
		v4s32Dec = vld4q_s32((int*)pSym);
		v4Floor = vld4q_f32((float*)pFloor);
		v4Inc.val[0] = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(v4s32Inc.val[0]), v4Floor.val[0]);
		v4Inc.val[1] = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(v4s32Inc.val[1]), v4Floor.val[1]);
		v4Inc.val[2] = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(v4s32Inc.val[2]), v4Floor.val[2]);
		v4Inc.val[3] = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(v4s32Inc.val[3]), v4Floor.val[3]);

		v4Floor = vld4q_f32((float*)pFloorSym);
		v4Dec.val[0] = AKSIMD_CONVERT_V4I32_TO_V4F32(v4s32Dec.val[0]);
		v4Dec.val[1] = AKSIMD_CONVERT_V4I32_TO_V4F32(v4s32Dec.val[1]);
		v4Dec.val[2] = AKSIMD_CONVERT_V4I32_TO_V4F32(v4s32Dec.val[2]);
		v4Dec.val[3] = AKSIMD_CONVERT_V4I32_TO_V4F32(v4s32Dec.val[3]);
		v4Dec.val[0] = AKSIMD_MUL_V4F32(v4Dec.val[0], v4Floor.val[0]);
		v4Dec.val[1] = AKSIMD_MUL_V4F32(v4Dec.val[1], v4Floor.val[1]);
		v4Dec.val[2] = AKSIMD_MUL_V4F32(v4Dec.val[2], v4Floor.val[2]);
		v4Dec.val[3] = AKSIMD_MUL_V4F32(v4Dec.val[3], v4Floor.val[3]);
		v4Dec.val[0] = AKSIMD_SHUFFLE_DCBA(v4Dec.val[0]);
		v4Dec.val[1] = AKSIMD_SHUFFLE_DCBA(v4Dec.val[1]);
		v4Dec.val[2] = AKSIMD_SHUFFLE_DCBA(v4Dec.val[2]);
		v4Dec.val[3] = AKSIMD_SHUFFLE_DCBA(v4Dec.val[3]);
#else
		AKSIMD_V4F32 f0, f1, f2, f3;

		//Read, A and B values.  				
		SIMDRead4(pC, 1, symIncAr, symIncAi, symIncBr, symIncBi);

		//Apply floor
		SIMDRead4(pFloor, 1, f0, f1, f2, f3);
		symIncAr = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(_mm_castps_si128(symIncAr)), f0);
		symIncAi = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(_mm_castps_si128(symIncAi)), f1);
		symIncBr = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(_mm_castps_si128(symIncBr)), f2);
		symIncBi = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(_mm_castps_si128(symIncBi)), f3);

		//Transpose
		SIMDRotate(symIncAr, symIncAi, symIncBr, symIncBi);		

		//Read in reversed position, A and B values		
		SIMDRead4(pSym, -1, symDecAr, symDecAi, symDecBr, symDecBi);

		//Apply floor
		SIMDRead4(pFloorSym, -1, f0, f1, f2, f3);
		symDecAr = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(_mm_castps_si128(symDecAr)), f0);
		symDecAi = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(_mm_castps_si128(symDecAi)), f1);
		symDecBr = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(_mm_castps_si128(symDecBr)), f2);
		symDecBi = AKSIMD_MUL_V4F32(AKSIMD_CONVERT_V4I32_TO_V4F32(_mm_castps_si128(symDecBi)), f3);

		//Transpose
		SIMDRotate(symDecAr, symDecAi, symDecBr, symDecBi);		
#endif

		SIMD_ffXPROD31(symDecAi, symDecBi, Tr, Ti, s4ai, s4bi);		
		SIMD_ffXNPROD31(symIncBr, symIncAr, Ti, Tr, s4ar, s4br);

		//Read twiddles one step further.  We keep 3 of them, and add a new one.		
		sincos.Next(tmpR, tmpI);

		SIMDShiftRightAndInsert(Tr, tmpR);
		SIMDShiftRightAndInsert(Ti, tmpI);

		SIMD_ffXPROD31(symIncAi, symIncBi, Ti, Tr, s1ai, s1bi);		
		SIMD_ffXNPROD31(symDecBr, symDecAr, Tr, Ti, s1ar, s1br);

		//Store
		AKSIMD_STORE_V4F32(pOut +  0, s1ar);
		AKSIMD_STORE_V4F32(pOut +  4, s1ai);
		AKSIMD_STORE_V4F32(pOut +  8, s1br);
		AKSIMD_STORE_V4F32(pOut +  12, s1bi);
		pOut +=16;

		//Store reversed
		AKSIMD_STORE_V4F32(pRevOut +  0, AKSIMD_SHUFFLE_DCBA(s4ar));
		AKSIMD_STORE_V4F32(pRevOut +  4, AKSIMD_SHUFFLE_DCBA(s4ai));
		AKSIMD_STORE_V4F32(pRevOut +  8, AKSIMD_SHUFFLE_DCBA(s4br));	
		AKSIMD_STORE_V4F32(pRevOut +  12, AKSIMD_SHUFFLE_DCBA(s4bi));
		pRevOut -=16;

		pSym-=4;
		pC+=4;
		pFloor += 4;
		pFloorSym -= 4;
	}	
}

//It also groups the data per group of 4 similar data for SIMD processing.  
//This means regrouping all the first real numbers of each 4 quadrants, followed by all the imaginary parts of the 4 quadrants
//Ex:
//Original distribution (for a 128 float input): r0 i0 r1 i1 ... r32 i32 r33 i33 ... r64 i64 r65 i65 ... r96 i96 r97 i97
//Grouped distribution						   : r0 r32 r64 r96 i0 i32 i64 i96 r1 r33 r65 r97 i1 i33 i65 i97 ...
//This grouping allows the following butterfly steps to work independently.

//This function can't have more than 2048 points as input (or it needs unaligned reads)
void mdct_first_2stages_SIMD_Ex(float *x,const int points, const int shift, float* pOut)
{
	AKSIMD_V4F32 r0, r1, r2, r3;
	int n4 = points / 16;	
	int x1a,x2a,x3a,x4a;
	x1a = 0;

	SinCosGenerator sincos;
	sincos.InitFwd(shift+1, 1);
	SinCosGenerator sincos2;
	sincos2.InitReverse(shift+1);
	SinCosGenerator sincos3;
	sincos3.InitFwd(shift+2, 1);

	AKSIMD_V4F32* pC = (AKSIMD_V4F32*)x;
	AKSIMD_V4F32* pSym = (AKSIMD_V4F32*)(x + points);

	AKSIMD_V4F32 Tr, Ti, TEr, TEi;
	//Symmetry output
	AKSIMD_V4F32 s1ar,s1ai;
	AKSIMD_V4F32 s1br,s1bi;
	AKSIMD_V4F32 s2ar,s2ai;
	AKSIMD_V4F32 s2br,s2bi; 	
	AKSIMD_V4F32 s3ar,s3ai;
	AKSIMD_V4F32 s3br,s3bi;
	AKSIMD_V4F32 s4ar,s4ai;
	AKSIMD_V4F32 s4br,s4bi;

	//Stage 1 output
	AKSIMD_V4F32 y1ar,y1ai;
	AKSIMD_V4F32 y1br,y1bi;
	AKSIMD_V4F32 y2ar,y2ai;
	AKSIMD_V4F32 y2br,y2bi;
	AKSIMD_V4F32 y3ar,y3ai;
	AKSIMD_V4F32 y3br,y3bi;
	AKSIMD_V4F32 y4ar,y4ai;
	AKSIMD_V4F32 y4br,y4bi;

	//Stage 2 output
	AKSIMD_V4F32 x1ar,x1ai;
	AKSIMD_V4F32 x1br,x1bi;
	AKSIMD_V4F32 x2ar,x2ai;
	AKSIMD_V4F32 x2br,x2bi;
	AKSIMD_V4F32 x3ar,x3ai;
	AKSIMD_V4F32 x3br,x3bi;
	AKSIMD_V4F32 x4ar,x4ai;
	AKSIMD_V4F32 x4br,x4bi;	

	pSym -= 1;

	//One loop will write 64 values.
	for(int i = 0; i < points/64;i++)
	{	
		x2a = x1a+n4;
		x3a = x2a+n4;
		x4a = x3a+n4;

		//Read values from presym step
		SIMDRead4(pC+x1a, 1, s1ar, s1ai, s1br, s1bi);
		SIMDRead4(pC+x3a, 1, s3ar, s3ai, s3br, s3bi);

		sincos.Next(Tr, Ti);

		//Butterfly stage 1 (first/third quarters)
		BUTTERFLY_STAGE_XN(s1ar, s1ai, s1br, s1bi,		//First quarter
			s3ar, s3ai, s3br, s3bi,						//Third quarter
			y1ar, y1ai, y1br, y1bi,						//Output first quarter
			y3ar, y3ai, y3br, y3bi);					//Output third quarter

		sincos2.Next(TEr, TEi);

		//Second/Fourth quarter	
		//Read values from presym step
		SIMDRead4(pC+x2a, 1, s2ar, s2ai, s2br, s2bi);
		SIMDRead4(pC+x4a, 1, s4ar, s4ai, s4br, s4bi);

		BUTTERFLY_STAGE_X(s2ar, s2ai, s2br, s2bi,		//Second quarter
			s4ar, s4ai, s4br, s4bi,						//Fourth quarter
			y2ar, y2ai, y2br, y2bi,						//Output Second quarter
			y4ar, y4ai, y4br, y4bi);					//Output Fourth quarter;

		//Compute stage 2 from the intermediate output.
		sincos3.Next(Tr, Ti);

		BUTTERFLY_STAGE_XN(y1ar, y1ai, y1br, y1bi, //First quarter
			y2ar, y2ai, y2br, y2bi, //Second quarter
			x1ar, x1ai, x1br, x1bi, //Output first quarter
			x2ar, x2ai, x2br, x2bi);//Output second quarter		

		BUTTERFLY_STAGE_XN(y3ar, y3ai, y3br, y3bi, //Third quarter
			y4ar, y4ai, y4br, y4bi, //Fourth quarter
			x3ar, x3ai, x3br, x3bi, //Output third quarter
			x4ar, x4ai, x4br, x4bi);//Output Fourth quarter		

		//Rotate data back.  The final memory layout for the rest of the mdct algo looks like this: Values for each quarter are interleaved IN a vector and the 4 values making pairs are grouped.
		//Example:
		//x1ar x2ar x3ar x4ar
		//x1ai x2ai x3ai x4ai
		//x1br x2br x3br x4br
		//x1bi x2bi x3bi x4bi

		//The data we currently have at this point is
		//x1ar0 x1ar1 x1ar2 x1ar3	... where the last index is the iteration number.  
		//x1ai0 x1ai1 x1ai2 x1ai3
		//x1br0 x1br1 x1br2 x1bi3
		//x1bi0 x1bi1 x1bi2 x1bi3
		// ... same for x2, x3, x4 quarters.

		//Rotating the x1ar values
		SIMDRotate(x1ar, x2ar, x3ar, x4ar);		//Now x1ar contains the x*ar (all quadrants) of the first iteration, etc.
		AKSIMD_STORE_V4F32(pOut +  0, x1ar);	//First iteration
		AKSIMD_STORE_V4F32(pOut + 16, x2ar);	//Second iteration
		AKSIMD_STORE_V4F32(pOut + 32, x3ar);	//Third iteration
		AKSIMD_STORE_V4F32(pOut + 48, x4ar);	//Fourth iteration
		pOut += 4;

		SIMDRotate(x1ai, x2ai, x3ai, x4ai);	
		AKSIMD_STORE_V4F32(pOut +  0, x1ai);
		AKSIMD_STORE_V4F32(pOut + 16, x2ai);
		AKSIMD_STORE_V4F32(pOut + 32, x3ai);
		AKSIMD_STORE_V4F32(pOut + 48, x4ai);
		pOut += 4;

		SIMDRotate(x1br, x2br, x3br, x4br);
		AKSIMD_STORE_V4F32(pOut +  0, x1br);
		AKSIMD_STORE_V4F32(pOut + 16, x2br);
		AKSIMD_STORE_V4F32(pOut + 32, x3br);
		AKSIMD_STORE_V4F32(pOut + 48, x4br);
		pOut += 4;

		SIMDRotate(x1bi, x2bi, x3bi, x4bi);	
		AKSIMD_STORE_V4F32(pOut +  0, x1bi);
		AKSIMD_STORE_V4F32(pOut + 16, x2bi);
		AKSIMD_STORE_V4F32(pOut + 32, x3bi);
		AKSIMD_STORE_V4F32(pOut + 48, x4bi);

		pOut += 64-12;
		x1a += 4;
		pSym -= 4;
	}
}

/* Doesn't work yet.  Needs to be debugged and switched to SinCosGenerator usage
Would only be useful for the 512 and 2048 window sizes.
void mdct_butterfly_2stages_SIMD(float *x,int points,int step)
{
	float *T_s1Start = fsincos_lookup0;
	float *T_s1End = fsincos_lookup0 + 1024;
	float *T_s2 = fsincos_lookup0;

	AKSIMD_V4F32 r0, r1, r2, r3;
	const int n4 = points / 16;	
	const int x2a = n4;
	const int x3a = 2*n4;
	const int x4a = 3*n4;

	AKSIMD_V4F32* AK_RESTRICT pC = (AKSIMD_V4F32*)x;

	AKSIMD_V4F32 Tr, Ti, TEr, TEi;
	//Symmetry output
	AKSIMD_V4F32 s1ar,s1ai;
	AKSIMD_V4F32 s1br,s1bi;
	AKSIMD_V4F32 s2ar,s2ai;
	AKSIMD_V4F32 s2br,s2bi; 	

	//Stage 1 output
	AKSIMD_V4F32 y1ar,y1ai;
	AKSIMD_V4F32 y1br,y1bi;
	AKSIMD_V4F32 y2ar,y2ai;
	AKSIMD_V4F32 y2br,y2bi;
	AKSIMD_V4F32 y3ar,y3ai;
	AKSIMD_V4F32 y3br,y3bi;
	AKSIMD_V4F32 y4ar,y4ai;
	AKSIMD_V4F32 y4br,y4bi;

	//This function follows the same logic as the scalar version.  The vectorization is done by executing 4 iterations the same time.
	//Each scalar iteration depends on 16 floats grouped by 4 (2 pairs of complex in each quadrant).  
	//In this SIMD version, for each of the 16 floats we'll read 4 vectors and recombine them so the content of the resulting vectors contain
	//only values that are semantically the same.  After that, all maths operations can then follow the exact same flow as the scalar version of the function.

	T_s1Start += step;
	T_s1End -= step;	
	T_s2 += step * 2;

	//One loop will write 64 values.
	for(int i = 0; i < points/128;i++)
	{
		//First/Third quarters
		SIMDRead4(pC, 1, s1ar, s1ai, s1br, s1bi);
		SIMDRead4(pC+x3a, 1, s2ar, s2ai, s2br, s2bi);
		LoadAndSplitPair((AKSIMD_V4F32*)T_s1Start, Tr, Ti);		
		T_s1Start += step;

		//Butterfly stage 1 (first/third quarters)
		BUTTERFLY_STAGE_XN(s1ar, s1ai, s1br, s1bi,						//First quarter
			s2ar, s2ai, s2br, s2bi,		//Third quarter
			y1ar, y1ai, y1br, y1bi,						//Output first quarter
			y3ar, y3ai, y3br, y3bi);					//Output third quarter

		//Second/Fourth quarter	
		SIMDRead4(pC+x2a, 1, s1ar, s1ai, s1br, s1bi);
		SIMDRead4(pC+x4a, 1, s2ar, s2ai, s2br, s2bi);
		LoadAndSplitPair((AKSIMD_V4F32*)T_s1End, TEr, TEi);
		T_s1End -= step;	

		BUTTERFLY_STAGE_X(s1ar, s1ai, s1br, s1bi,						//Second quarter
			s2ar, s2ai, s2br, s2bi,		//Fourth quarter
			y2ar, y2ai, y2br, y2bi,						//Output Second quarter
			y4ar, y4ai, y4br, y4bi);					//Output Fourth quarter;

		//Compute stage 2 from the intermediate output.
		LoadAndSplitPair((AKSIMD_V4F32*)T_s2, Tr, Ti);
		T_s2 += step*2;

		BUTTERFLY_STAGE_XN(y1ar, y1ai, y1br, y1bi, //First quarter
			y2ar, y2ai, y2br, y2bi, //Second quarter
			*(pC + 0), *(pC + 1), *(pC + 2), *(pC + 3), //Output first quarter
			*(pC + x2a + 0), *(pC+ x2a + 1), *(pC+ x2a + 2), *(pC+ x2a + 3));//Output second quarter

		BUTTERFLY_STAGE_XN(y3ar, y3ai, y3br, y3bi, //Third quarter
			y4ar, y4ai, y4br, y4bi, //Fourth quarter
			*(pC + x3a + 0), *(pC+ x3a + 1), *(pC+ x3a + 2), *(pC+ x3a + 3), //Output third quarter
			*(pC + x4a + 0), *(pC+ x4a + 1), *(pC+ x4a + 2), *(pC+ x4a + 3));//Output Fourth quarter		

		pC+=4;
	}

	T_s2 = fsincos_lookup0 + 1024 - step*2;

	for(int i = 0; i < points/128;i++)
	{
		SIMDRead4(pC, 1, s1ar, s1ai, s1br, s1bi);
		SIMDRead4(pC+x3a, 1, s2ar, s2ai, s2br, s2bi);
		LoadAndSplitPair((AKSIMD_V4F32*)T_s1Start, Tr, Ti);
		T_s1Start += step;

		//Butterfly stage 1 (first/third quarters)
		BUTTERFLY_STAGE_XN(s1ar, s1ai, s1br, s1bi,						//First quarter
			s2ar, s2ai, s2br, s2bi,		//Third quarter
			y1ar, y1ai, y1br, y1bi,						//Output first quarter
			y3ar, y3ai, y3br, y3bi);					//Output third quarter

		//Second/Fourth quarter	
		SIMDRead4(pC+x2a, 1, s1ar, s1ai, s1br, s1bi);
		SIMDRead4(pC+x4a, 1, s2ar, s2ai, s2br, s2bi);
		LoadAndSplitPair((AKSIMD_V4F32*)T_s1End, TEr, TEi);
		T_s1End -= step;	

		BUTTERFLY_STAGE_X(s1ar, s1ai, s1br, s1bi,						//Second quarter
			s2ar, s2ai, s2br, s2bi,		//Fourth quarter
			y2ar, y2ai, y2br, y2bi,						//Output Second quarter
			y4ar, y4ai, y4br, y4bi);					//Output Fourth quarter;

		//Compute stage 2 from the intermediate output.
		LoadAndSplitPair((AKSIMD_V4F32*)T_s2, TEr, TEi);
		T_s2 -= step*2;

		BUTTERFLY_STAGE_X(y1ar, y1ai, y1br, y1bi,					
			y2ar, y2ai, y2br, y2bi,				
			*(pC + 0), *(pC + 1), *(pC + 2), *(pC + 3), //Output first quarter
			*(pC + x2a + 0), *(pC+ x2a + 1), *(pC+ x2a + 2), *(pC+ x2a + 3));//Output second quarter

		BUTTERFLY_STAGE_X(y3ar, y3ai, y3br, y3bi,					
			y4ar, y4ai, y4br, y4bi,				
			*(pC + x3a + 0), *(pC+ x3a + 1), *(pC+ x3a + 2), *(pC+ x3a + 3), //Output third quarter
			*(pC + x4a + 0), *(pC+ x4a + 1), *(pC+ x4a + 2), *(pC+ x4a + 3));//Output Fourth quarter		

		pC+=4;
	}
}
*/

/* 

Further possible optimizations:
A) The 2-stage combined generic pass is coded, but not used yet. This is only useful for window size of 512, 1024 and 2048.
B) Merging presymmetry with the first two stages of butterflies is still possible, we'd save 50% of the writes and reads between the 2 steps.  
   We can process the first half directly, keeping everything in register.  But the second half (processed in reverse) will still need to be temporarily written and processed in the second loop.
*/

AkForceInline void mdct_backward_SIMD(int n, float *in, float *in_floor, int end)
{	
	int shift;
	int step;

	for (shift=4; !(n&(1<<shift)) ;shift++);
	shift=13-shift;
	step=2<<shift;

	// MDCT uses half the input (for 50% overlap)
	n /= 2;

	float *mdctBuffer = in_floor;

	// For the butterflies, the total number of stages is: log_2(N).
	mdct_presymmetry_SIMD_Ex(in, n, shift, in_floor, end);
	// Each stage has N/2 butterflies.

	// First 2 stages
	mdct_first_2stages_SIMD_Ex(in, n, shift, mdctBuffer);

	// Next (5 - 2) stages here of the log_2(N) stages.
	int stages=8-shift-2;
	int i,j;
	int bitshift = 2+shift;

	for(i = 0; --stages > 0; i++)
	{
		// For each stage we divide the stride by two: N = N/(2^i)
		for(j = 0 ; j < (1<<i) ; j++)
		{
			// For each pass in a stage we go to the next stride: j*N/(2^i)
			mdct_butterfly_stage_SIMD_Ex(mdctBuffer + (n >> i)*j, n >> i, bitshift);
		}
		bitshift++;
	}

	// Last 5 stages recursively with strides: { 32, { 16, {8, 4, 2} } }
	for (int i = 0; i < n; i += 32 * 4)
	{
		mdct_butterfly_32x4(mdctBuffer + i, i / 4, n);
	}

	n *= 2;

	mdct_reverse_step7_step8(mdctBuffer, n, shift, in);
}

void mdct_backward(int n, float *in, float* floor, int end)
{
	mdct_backward_SIMD( n, in, floor, end);
}

void mdct_unroll_lap(	int			n0,
						   int			n1,
						   int			lW,
						   int			W,
						   float		*in,
						   float		*right,
						   const float	*w0,
						   const float	*w1,
						   float	*out,
						   int			step,
						   int			start, /* samples, this frame */
						   int			end    /* samples, this frame */)
{
	float *l=in+(W&&lW ? n1>>2 : n0>>2);
	float *r=right+(lW ? n1>>2 : n0>>2);
	float *post;
	const float *wR=(W && lW ? w1+(n1>>1) : w0+(n0>>1));
	const float *wL=(W && lW ? w1         : w0        );

	int preLap=(lW && !W ? (n1>>2)-(n0>>2) : 0 );
	int halfLap=(lW && W ? (n1>>2) : (n0>>2) );
	int postLap=(!lW && W ? (n1>>2)-(n0>>2) : 0 );
	int n,off;

	/* preceeding direct-copy lapping from previous frame, if any */
	if(preLap)
	{
		n      = (end<preLap?end:preLap);
		off    = (start<preLap?start:preLap);
		post   = r-n;
		r     -= off;
		start -= off;
		end   -= n;

		/*AKASSERT(((AkUIntPtr)out % AK_SIMD_ALIGNMENT) == 0);
		AKASSERT(((AkUIntPtr)r % AK_SIMD_ALIGNMENT) == 0);*/

		if (((AkUIntPtr)out % AK_SIMD_ALIGNMENT) == 0 && ((AkUIntPtr)r % AK_SIMD_ALIGNMENT) == 0 && ((AkUIntPtr)post % AK_SIMD_ALIGNMENT) == 0)
		{
			while(r>post)
			{
				r -= 4;
				AKSIMD_V4F32 a = AKSIMD_LOAD_V4F32(r);
				AKSIMD_STORE_V4F32(out, AKSIMD_SHUFFLE_DCBA(a));
				out += 4;
			}
		}
 		else
 		{
 			while(r>post)
 			{
 				--r;
 				*out = *r;
 				out++;				
 			}
 		}
	}

	/* cross-lap; two halves due to wrap-around */
	n      = (end<halfLap?end:halfLap);
	off    = (start<halfLap?start:halfLap);
	post   = r-n;
	r     -= off;
	l     -= off;
	start -= off;
	wR    -= off;
	wL    += off;
	end   -= n;

	if (((AkUIntPtr)out % AK_SIMD_ALIGNMENT) == 0 && ((AkUIntPtr)r % AK_SIMD_ALIGNMENT) == 0 && ((AkUIntPtr)post % AK_SIMD_ALIGNMENT) == 0)
	{
		AKASSERT(((AkUIntPtr)l % AK_SIMD_ALIGNMENT) == 0);

		while(r>post)
		{
			l -= 4;
			r -= 4;
			wR -= 4;		
			AKSIMD_V4F32 fr = AKSIMD_LOAD_V4F32(r);
			AKSIMD_V4F32 fwR = AKSIMD_LOAD_V4F32(wR);
			AKSIMD_V4F32 fl = AKSIMD_LOAD_V4F32(l);
			AKSIMD_V4F32 fwL = AKSIMD_SHUFFLE_DCBA(AKSIMD_LOAD_V4F32(wL));
			wL += 4;

			AKSIMD_V4F32 rev = AK_MADD(fr, fwR, AKSIMD_MUL_V4F32(fl, fwL));

			AKSIMD_STORE_V4F32(out, AKSIMD_SHUFFLE_DCBA(rev));
			out+=4;
		}
	}
	else
	{
		while(r>post)
		{
			l--;
			float fr = *--r;
			float fwR = *--wR;
			float fl = *l;
			float fwL = *wL++;

			float Prod	= (fr * fwR) + (fl * fwL);
			*out = Prod;
			out++;
		}
	}

	n      = (end<halfLap?end:halfLap);
	off    = (start<halfLap?start:halfLap);
	post   = r+n;
	r     += off;
	l     += off;
	start -= off;
	end   -= n;
	wR    -= off;
	wL    += off;

	if (((AkUIntPtr)out % AK_SIMD_ALIGNMENT) == 0 && ((AkUIntPtr)r % AK_SIMD_ALIGNMENT) == 0 && ((AkUIntPtr)post % AK_SIMD_ALIGNMENT) == 0)
	{
		AKASSERT(((AkUIntPtr)l % AK_SIMD_ALIGNMENT) == 0);

		while(r<post)
		{
			wR -= 4;		
			AKSIMD_V4F32 fr = AKSIMD_LOAD_V4F32(r);
			AKSIMD_V4F32 fwR = AKSIMD_SHUFFLE_DCBA(AKSIMD_LOAD_V4F32(wR));
			AKSIMD_V4F32 fl = AKSIMD_LOAD_V4F32(l);
			AKSIMD_V4F32 fwL = AKSIMD_LOAD_V4F32(wL);
			wL += 4;
			r += 4;
			l += 4;

			AKSIMD_STORE_V4F32(out, AK_MSUB(fr, fwR, AKSIMD_MUL_V4F32(fl, fwL)));
			out+=4;
		}
	}
	else
	{
		while(r<post)
		{
			float fr = *r++;
			float fwR = (float)*--wR;
			float fl = *l;
			float fwL = (float)*wL++;

			float Prod	= (fr * fwR) - (fl * fwL);
			*out = Prod;
			out++;
			l++;
		}
	}

	/* preceeding direct-copy lapping from previous frame, if any */
	if(postLap)
	{
		n      = (end<postLap?end:postLap);
		off    = (start<postLap?start:postLap);
		post   = l+n;
		l     += off;

		if (((AkUIntPtr)out % AK_SIMD_ALIGNMENT) == 0 && ((AkUIntPtr)l % AK_SIMD_ALIGNMENT) == 0 && ((AkUIntPtr)post % AK_SIMD_ALIGNMENT) == 0)
		{
			while(l<post)
			{
				AKSIMD_V4F32 a = AKSIMD_LOAD_V4F32(l);
				AKSIMD_STORE_V4F32(out, AKSIMD_NEG_V4F32(a));
				out += 4;
				l += 4;
			}
		}
		else
		{
			while(l<post)
			{
				*out = *l * -1.f;
				out++;
				l++;
			}
		}
	}
}


#endif
