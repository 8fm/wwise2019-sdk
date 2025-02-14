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
#include "SinCosGen.h"
#if defined( AKSIMD_V4F32_SUPPORTED )
#include <AK/Tools/Common/AkPlatformFuncs.h>

//Use the following functions to re-generate the precomputed starting points tables for sines and cosines.
#if 0
#include <Windows.h>
#include <math.h>
void PrintVectors(float  *x, int N)
{
	char  msg[100];
	for (int i = 0; i < N-1; i++)
	{		
		sprintf(msg, "{%.10ff, %.10ff, %.10ff, %.10ff},\n", x[i][0], x[i][1], x[i][2], x[i][3]);
		AKPLATFORM::OutputDebugMsg(msg);
	}
	sprintf(msg, "{%.10ff, %.10ff, %.10ff, %.10ff}};\n\n", x[N-1][0], x[N-1][1], x[N-1][2], x[N-1][3]);
	AKPLATFORM::OutputDebugMsg(msg);
}

void GenStep7Factors()
{
	float  s1[STEP7_TABLE_LEN];
	float  s2[STEP7_TABLE_LEN];
	float  c1[STEP7_TABLE_LEN];
	float  c2[STEP7_TABLE_LEN];
	int i = 0;
	for (int step = 1; step < (1 << STEP7_TABLE_LEN); step <<= 1)
	{
		const int N = 1024*8/step;		//Generate 1/8 of cycle.
		// step = N steps over 2*PI radians
		const float internal_b = 2.0f*PI/N;
			
		//Recompute for SIMD steps (4 times less).
		const float b =  2.0f*PI/(N/4);
		const float origin = 2.0f*PI/(N*2);

		for(int j = 0; j < 4; j++)
		{
			// minus three steps from origin (because it is a second order filter)
			
			float a = origin+(j*internal_b) - 3.0f * b;

			//Replace with FastSin/Cos
			s2[i][j] = sin(a + b);	
			s1[i][j] = sin(a + 2*b);
			c2[i][j] = cos(a + b);
			c1[i][j] = cos(a + 2*b);
		}
		i++;
	}

	AKPLATFORM::OutputDebugMsg("const AK_ALIGN_SIMD(float  SinCosGenerator::s_step7_s1[STEP7_TABLE_LEN]) = {\n");
	PrintVectors(s1, STEP7_TABLE_LEN);
	AKPLATFORM::OutputDebugMsg("const AK_ALIGN_SIMD(float  SinCosGenerator::s_step7_s2[STEP7_TABLE_LEN]) = {\n");
	PrintVectors(s2, STEP7_TABLE_LEN);
	AKPLATFORM::OutputDebugMsg("const AK_ALIGN_SIMD(float  SinCosGenerator::s_step7_c1[STEP7_TABLE_LEN]) = {\n");
	PrintVectors(c1, STEP7_TABLE_LEN);
	AKPLATFORM::OutputDebugMsg("const AK_ALIGN_SIMD(float  SinCosGenerator::s_step7_c2[STEP7_TABLE_LEN]) = {\n");
	PrintVectors(c2, STEP7_TABLE_LEN);
}


void GenReverseFactors()
{
	float  _s1[SINCOS_TABLE_LEN*2][4];
	float  _s2[SINCOS_TABLE_LEN*2][4];
	float  _c1[SINCOS_TABLE_LEN*2][4];
	float  _c2[SINCOS_TABLE_LEN*2][4];
	int i = 0;
	
	for (int step = 1; step < (1 << SINCOS_TABLE_LEN); step <<= 1)
	{
		int N = 512*8/step;		//Generate 1/8 of cycle.
		// step = N steps over 2*PI radians
		float internal_b = 2.0f*PI/N;
		const float internal_cb = 2 * cos(internal_b);
			
		//Recompute for SIMD steps (4 times less).
		float b =  2.0f*PI/(N/4);
		const float cb = 2 * cos(b);			

		for(int j = 0; j < 4; j++)
		{
			//Start from 45 degrees, compute the next two steps. (Always an offset of 1)
			float a = PI/4 - internal_b*(j+1) + 3.0f * b;
			
			float s2 = sin(a - b);
			float s1 = sin(a - 2*b);
			float c2 = cos(a - b);
			float c1 = cos(a - 2*b);

			_s1[i][j] = s1;
			_c1[i][j] = c1;
			_s2[i][j] = s2;	
			_c2[i][j] = c2;	

			float s = cb * s1 - s2;
			float c = cb * c1 - c2;
			s2 = s1;
			c2 = c1;
			s1 = s;
			c1 = c;
		}

		i++;
	}

	AKPLATFORM::OutputDebugMsg("const AK_ALIGN_SIMD(float  SinCosGenerator::s_s1Rev[SINCOS_TABLE_LEN]) = {\n");
	PrintVectors(_s1, SINCOS_TABLE_LEN);

	AKPLATFORM::OutputDebugMsg("const AK_ALIGN_SIMD(float  SinCosGenerator::s_s2Rev[SINCOS_TABLE_LEN]) = {\n");
	PrintVectors(_s2, SINCOS_TABLE_LEN);

	AKPLATFORM::OutputDebugMsg("const AK_ALIGN_SIMD(float  SinCosGenerator::s_c1Rev[SINCOS_TABLE_LEN]) = {\n");
	PrintVectors(_c1, SINCOS_TABLE_LEN);

	AKPLATFORM::OutputDebugMsg("const AK_ALIGN_SIMD(float  SinCosGenerator::s_c2Rev[SINCOS_TABLE_LEN]) = {\n");
	PrintVectors(_c2, SINCOS_TABLE_LEN);
}

void GenSinCosFactors()
{
	float  _s1[SINCOS_TABLE_LEN*2][4];
	float  _s2[SINCOS_TABLE_LEN*2][4];
	float  _c1[SINCOS_TABLE_LEN*2][4];
	float  _c2[SINCOS_TABLE_LEN*2][4];
	float  _cb[SINCOS_TABLE_LEN*2][4];
	int i = 0;
	
	for (int offset = 0; offset < 2; offset++)
	{
		for (int step = 1; step < (1 << SINCOS_TABLE_LEN); step <<= 1)
		{
			int N = 512*8/step;		//Generate 1/8 of cycle.
			// step = N steps over 2*PI radians
			float internal_b = 2.0f*PI/N;
			const float internal_cb = 2 * cos(internal_b);
			
			//Recompute for SIMD steps (4 times less).
			float b =  2.0f*PI/(N/4);
			const float cb = 2 * cos(b);			

			for(int j = 0; j < 4; j++)
			{
				// minus three steps from origin (because it is a second order filter)
				float a = internal_b*(j+offset) - 3.0f * b;
			
				float s2 = sin(a + b);
				float s1 = sin(a + 2*b);
				float c2 = cos(a + b);
				float c1 = cos(a + 2*b);

				_s1[i][j] = s1;
				_c1[i][j] = c1;
				_s2[i][j] = s2;	
				_c2[i][j] = c2;	
			}

			//_cb[i][0] = _cb[i][1] = _cb[i][2] = _cb[i][3] = cb;

			i++;
		}
	}

	//Generate CB table.  Need one extra entry for "mdct_step8".
	i = 0;
	for (int step = 1; step < (1 << (SINCOS_TABLE_LEN+1)); step <<= 1)
	{
		int N = 1024*8/step;		//Generate 1/8 of cycle.	Use 1024-steps cycle as base, contary to other methods ( this adds the missing entry for step8).
		float b =  2.0f*PI/(N/4);
		const float cb = 2 * cos(b);
		_cb[i][0] = _cb[i][1] = _cb[i][2] = _cb[i][3] = cb;
		i++;
	}

	AKPLATFORM::OutputDebugMsg("const AK_ALIGN_SIMD(float  SinCosGenerator::s_s1[SINCOS_TABLE_LEN*2]) = {\n");
	PrintVectors(_s1, SINCOS_TABLE_LEN*2);

	AKPLATFORM::OutputDebugMsg("const AK_ALIGN_SIMD(float  SinCosGenerator::s_s2[SINCOS_TABLE_LEN*2]) = {\n");
	PrintVectors(_s2, SINCOS_TABLE_LEN*2);

	AKPLATFORM::OutputDebugMsg("const AK_ALIGN_SIMD(float  SinCosGenerator::s_c1[SINCOS_TABLE_LEN*2]) = {\n");
	PrintVectors(_c1, SINCOS_TABLE_LEN*2);

	AKPLATFORM::OutputDebugMsg("const AK_ALIGN_SIMD(float  SinCosGenerator::s_c2[SINCOS_TABLE_LEN*2]) = {\n");
	PrintVectors(_c2, SINCOS_TABLE_LEN*2);

	AKPLATFORM::OutputDebugMsg("const AK_ALIGN_SIMD(float  SinCosGenerator::s_cb[SINCOS_TABLE_LEN+1]) = {\n");
	PrintVectors(_cb, SINCOS_TABLE_LEN+1);

	GenStep7Factors();
	GenReverseFactors();
}
#endif

const AK_ALIGN_SIMD(float  SinCosGenerator::s_s1[SINCOS_TABLE_LEN * 2][4]) = {
{-0.0061358842f, -0.0046019270f, -0.0030679570f, -0.0015339799f},
{-0.0122715374f, -0.0092037562f, -0.0061358851f, -0.0030679561f},
{-0.0245412271f, -0.0184067339f, -0.0122715393f, -0.0061358833f},
{-0.0490676723f, -0.0368072316f, -0.0245412309f, -0.0122715356f},
{-0.0980171338f, -0.0735645816f, -0.0490676798f, -0.0245412234f},
{-0.1950903088f, -0.1467304975f, -0.0980171487f, -0.0490676649f},
{-0.3826834261f, -0.2902847230f, -0.1950903386f, -0.0980171189f},
{-0.7071067691f, -0.5555703044f, -0.3826834559f, -0.1950902790f},
{-1.0000000000f, -0.9238796234f, -0.7071068287f, -0.3826833665f},
{-0.0046019270f, -0.0030679570f, -0.0015339799f, 0.0000000000f},
{-0.0092037562f, -0.0061358851f, -0.0030679561f, 0.0000000000f},
{-0.0184067339f, -0.0122715393f, -0.0061358833f, 0.0000000000f},
{-0.0368072316f, -0.0245412309f, -0.0122715356f, 0.0000000000f},
{-0.0735645816f, -0.0490676798f, -0.0245412234f, 0.0000000000f},
{-0.1467304975f, -0.0980171487f, -0.0490676649f, 0.0000000000f},
{-0.2902847230f, -0.1950903386f, -0.0980171189f, 0.0000000000f},
{-0.5555703044f, -0.3826834559f, -0.1950902790f, 0.0000000000f},
{-0.9238796234f, -0.7071068287f, -0.3826833665f, 0.0000000000f}};

const AK_ALIGN_SIMD(float  SinCosGenerator::s_s2[SINCOS_TABLE_LEN * 2][4]) = {
{-0.0122715374f, -0.0107376594f, -0.0092037544f, -0.0076698288f},
{-0.0245412271f, -0.0214740820f, -0.0184067301f, -0.0153392060f},
{-0.0490676723f, -0.0429382585f, -0.0368072242f, -0.0306748021f},
{-0.0980171338f, -0.0857973173f, -0.0735645667f, -0.0613207370f},
{-0.1950903088f, -0.1709619015f, -0.1467304677f, -0.1224106699f},
{-0.3826834261f, -0.3368898630f, -0.2902846634f, -0.2429801822f},
{-0.7071067691f, -0.6343933344f, -0.5555702448f, -0.4713967144f},
{-1.0000000000f, -0.9807853103f, -0.9238795042f, -0.8314695954f},
{-0.0000001510f, -0.3826832771f, -0.7071067691f, -0.9238795638f},
{-0.0107376594f, -0.0092037544f, -0.0076698288f, -0.0061358847f},
{-0.0214740820f, -0.0184067301f, -0.0153392060f, -0.0122715384f},
{-0.0429382585f, -0.0368072242f, -0.0306748021f, -0.0245412290f},
{-0.0857973173f, -0.0735645667f, -0.0613207370f, -0.0490676761f},
{-0.1709619015f, -0.1467304677f, -0.1224106699f, -0.0980171412f},
{-0.3368898630f, -0.2902846634f, -0.2429801822f, -0.1950903237f},
{-0.6343933344f, -0.5555702448f, -0.4713967144f, -0.3826834559f},
{-0.9807853103f, -0.9238795042f, -0.8314695954f, -0.7071067691f},
{-0.3826832771f, -0.7071067691f, -0.9238795638f, -1.0000000000f}};

const AK_ALIGN_SIMD(float  SinCosGenerator::s_c1[SINCOS_TABLE_LEN * 2][4]) = {
{0.9999811649f, 0.9999893904f, 0.9999952912f, 0.9999988079f},
{0.9999247193f, 0.9999576211f, 0.9999811649f, 0.9999952912f},
{0.9996988177f, 0.9998306036f, 0.9999247193f, 0.9999811649f},
{0.9987954497f, 0.9993223548f, 0.9996988177f, 0.9999247193f},
{0.9951847196f, 0.9972904325f, 0.9987954497f, 0.9996988177f},
{0.9807853103f, 0.9891765118f, 0.9951847196f, 0.9987954497f},
{0.9238795638f, 0.9569402933f, 0.9807852507f, 0.9951847196f},
{0.7071068287f, 0.8314695358f, 0.9238795042f, 0.9807853103f},
{0.0000000755f, 0.3826832175f, 0.7071067095f, 0.9238795638f},
{0.9999893904f, 0.9999952912f, 0.9999988079f, 1.0000000000f},
{0.9999576211f, 0.9999811649f, 0.9999952912f, 1.0000000000f},
{0.9998306036f, 0.9999247193f, 0.9999811649f, 1.0000000000f},
{0.9993223548f, 0.9996988177f, 0.9999247193f, 1.0000000000f},
{0.9972904325f, 0.9987954497f, 0.9996988177f, 1.0000000000f},
{0.9891765118f, 0.9951847196f, 0.9987954497f, 1.0000000000f},
{0.9569402933f, 0.9807852507f, 0.9951847196f, 1.0000000000f},
{0.8314695358f, 0.9238795042f, 0.9807853103f, 1.0000000000f},
{0.3826832175f, 0.7071067095f, 0.9238795638f, 1.0000000000f}};

const AK_ALIGN_SIMD(float  SinCosGenerator::s_c2[SINCOS_TABLE_LEN * 2][4]) = {
{0.9999247193f, 0.9999423623f, 0.9999576211f, 0.9999706149f},
{0.9996988177f, 0.9997693896f, 0.9998306036f, 0.9998823404f},
{0.9987954497f, 0.9990777373f, 0.9993223548f, 0.9995294213f},
{0.9951847196f, 0.9963126183f, 0.9972904325f, 0.9981181026f},
{0.9807853103f, 0.9852776527f, 0.9891765118f, 0.9924795628f},
{0.9238795638f, 0.9415440559f, 0.9569403529f, 0.9700312614f},
{0.7071068287f, 0.7730104327f, 0.8314695954f, 0.8819212914f},
{0.0000000755f, 0.1950902343f, 0.3826834261f, 0.5555702448f},
{-1.0000000000f, -0.9238796234f, -0.7071067691f, -0.3826833963f},
{0.9999423623f, 0.9999576211f, 0.9999706149f, 0.9999811649f},
{0.9997693896f, 0.9998306036f, 0.9998823404f, 0.9999247193f},
{0.9990777373f, 0.9993223548f, 0.9995294213f, 0.9996988177f},
{0.9963126183f, 0.9972904325f, 0.9981181026f, 0.9987954497f},
{0.9852776527f, 0.9891765118f, 0.9924795628f, 0.9951847196f},
{0.9415440559f, 0.9569403529f, 0.9700312614f, 0.9807852507f},
{0.7730104327f, 0.8314695954f, 0.8819212914f, 0.9238795042f},
{0.1950902343f, 0.3826834261f, 0.5555702448f, 0.7071067691f},
{-0.9238796234f, -0.7071067691f, -0.3826833963f, -0.0000000437f}};

const AK_ALIGN_SIMD(float  SinCosGenerator::s_cb[SINCOS_TABLE_LEN + 1][4]) = {
{1.9999905825f, 1.9999905825f, 1.9999905825f, 1.9999905825f},
{1.9999623299f, 1.9999623299f, 1.9999623299f, 1.9999623299f},
{1.9998494387f, 1.9998494387f, 1.9998494387f, 1.9998494387f},
{1.9993976355f, 1.9993976355f, 1.9993976355f, 1.9993976355f},
{1.9975908995f, 1.9975908995f, 1.9975908995f, 1.9975908995f},
{1.9903694391f, 1.9903694391f, 1.9903694391f, 1.9903694391f},
{1.9615705013f, 1.9615705013f, 1.9615705013f, 1.9615705013f},
{1.8477590084f, 1.8477590084f, 1.8477590084f, 1.8477590084f},
{1.4142135382f, 1.4142135382f, 1.4142135382f, 1.4142135382f},
{-0.0000000874f, -0.0000000874f, -0.0000000874f, -0.0000000874f}};

const AK_ALIGN_SIMD(float  SinCosGenerator::s_step7_s1[STEP7_TABLE_LEN][4]) = {
{-0.0026844628f, -0.0019174751f, -0.0011504854f, -0.0003834954f},
{-0.0053689065f, -0.0038349433f, -0.0023009691f, -0.0007669906f},
{-0.0107376575f, -0.0076698302f, -0.0046019261f, -0.0015339808f},
{-0.0214740783f, -0.0153392088f, -0.0092037544f, -0.0030679579f},
{-0.0429382510f, -0.0306748077f, -0.0184067301f, -0.0061358870f},
{-0.0857973024f, -0.0613207482f, -0.0368072242f, -0.0122715430f},
{-0.1709618717f, -0.1224106923f, -0.0735645667f, -0.0245412383f}};

const AK_ALIGN_SIMD(float  SinCosGenerator::s_step7_s2[STEP7_TABLE_LEN][4]) = {
{-0.0057523958f, -0.0049854172f, -0.0042184344f, -0.0034514503f},
{-0.0115046008f, -0.0099707106f, -0.0084367944f, -0.0069028591f},
{-0.0230076797f, -0.0199404284f, -0.0168729872f, -0.0138053894f},
{-0.0460031778f, -0.0398729295f, -0.0337411724f, -0.0276081469f},
{-0.0919089466f, -0.0796824396f, -0.0674439147f, -0.0551952496f},
{-0.1830398738f, -0.1588581502f, -0.1345807016f, -0.1102222130f},
{-0.3598950207f, -0.3136817515f, -0.2667127550f, -0.2191012502f}};

const AK_ALIGN_SIMD(float  SinCosGenerator::s_step7_c1[STEP7_TABLE_LEN][4]) = {
{0.9999964237f, 0.9999981523f, 0.9999993443f, 0.9999999404f},
{0.9999855757f, 0.9999926686f, 0.9999973774f, 0.9999997020f},
{0.9999423623f, 0.9999706149f, 0.9999893904f, 0.9999988079f},
{0.9997693896f, 0.9998823404f, 0.9999576211f, 0.9999952912f},
{0.9990777373f, 0.9995294213f, 0.9998306036f, 0.9999811649f},
{0.9963126183f, 0.9981181026f, 0.9993223548f, 0.9999247193f},
{0.9852776527f, 0.9924795032f, 0.9972904325f, 0.9996988177f}};

const AK_ALIGN_SIMD(float  SinCosGenerator::s_step7_c2[STEP7_TABLE_LEN][4]) = {
{0.9999834299f, 0.9999876022f, 0.9999911189f, 0.9999940395f},
{0.9999338388f, 0.9999502897f, 0.9999644160f, 0.9999761581f},
{0.9997352958f, 0.9998011589f, 0.9998576641f, 0.9999046922f},
{0.9989413023f, 0.9992047548f, 0.9994305968f, 0.9996188283f},
{0.9957674146f, 0.9968202710f, 0.9977230430f, 0.9984755516f},
{0.9831054807f, 0.9873014092f, 0.9909026623f, 0.9939069748f},
{0.9329928160f, 0.9495281577f, 0.9637760520f, 0.9757021070f}};

const AK_ALIGN_SIMD(float  SinCosGenerator::s_s1Rev[SINCOS_TABLE_LEN][4]) = {
{0.7103533745f, 0.7092728615f, 0.7081906796f, 0.7071067691f},
{0.7135848403f, 0.7114321589f, 0.7092728019f, 0.7071067691f},
{0.7200024724f, 0.7157308459f, 0.7114321589f, 0.7071067691f},
{0.7326542735f, 0.7242470980f, 0.7157308459f, 0.7071067691f},
{0.7572088242f, 0.7409511805f, 0.7242470384f, 0.7071067691f},
{0.8032075167f, 0.7730104327f, 0.7409511209f, 0.7071067691f},
{0.8819212317f, 0.8314695954f, 0.7730104327f, 0.7071067691f},
{0.9807852507f, 0.9238795638f, 0.8314696550f, 0.7071067691f},
{0.9238795042f, 1.0000000000f, 0.9238796234f, 0.7071068287f}};

const AK_ALIGN_SIMD(float  SinCosGenerator::s_s2Rev[SINCOS_TABLE_LEN][4]) = {
{0.7146586776f, 0.7135848403f, 0.7125093341f, 0.7114321589f},
{0.7221282125f, 0.7200025320f, 0.7178700566f, 0.7157308459f},
{0.7368165255f, 0.7326542735f, 0.7284643650f, 0.7242470384f},
{0.7651672363f, 0.7572088242f, 0.7491363883f, 0.7409511209f},
{0.8175848126f, 0.8032075763f, 0.7883464098f, 0.7730104327f},
{0.9039893150f, 0.8819212914f, 0.8577286601f, 0.8314695954f},
{0.9951847196f, 0.9807852507f, 0.9569402933f, 0.9238795042f},
{0.8314696550f, 0.9238795042f, 0.9807852507f, 1.0000000000f},
{-0.3826834261f, 0.0000001510f, 0.3826832771f, 0.7071067691f}};

const AK_ALIGN_SIMD(float  SinCosGenerator::s_c1Rev[SINCOS_TABLE_LEN][4]) = {
{0.7038452029f, 0.7049340606f, 0.7060212493f, 0.7071067691f},
{0.7005687952f, 0.7027547359f, 0.7049341202f, 0.7071068287f},
{0.6939715147f, 0.6983762383f, 0.7027547359f, 0.7071068287f},
{0.6806010008f, 0.6895405054f, 0.6983762383f, 0.7071067691f},
{0.6531728506f, 0.6715589166f, 0.6895405650f, 0.7071068287f},
{0.5956993103f, 0.6343932748f, 0.6715589762f, 0.7071068287f},
{0.4713967741f, 0.5555702448f, 0.6343933344f, 0.7071067691f},
{0.1950903535f, 0.3826833069f, 0.5555701852f, 0.7071068287f},
{-0.3826835155f, 0.0000000755f, 0.3826832175f, 0.7071067095f}};

const AK_ALIGN_SIMD(float  SinCosGenerator::s_c2Rev[SINCOS_TABLE_LEN][4]) = {
{0.6994733214f, 0.7005687952f, 0.7016626000f, 0.7027547359f},
{0.6917592287f, 0.6939714551f, 0.6961771250f, 0.6983762383f},
{0.6760927439f, 0.6806010008f, 0.6850836873f, 0.6895405650f},
{0.6438315511f, 0.6531728506f, 0.6624158025f, 0.6715589762f},
{0.5758081675f, 0.5956992507f, 0.6152315736f, 0.6343932748f},
{0.4275549948f, 0.4713966548f, 0.5141026974f, 0.5555702448f},
{0.0980171338f, 0.1950903535f, 0.2902847528f, 0.3826834261f},
{-0.5555701852f, -0.3826835155f, -0.1950903237f, 0.0000000755f},
{-0.9238795042f, -1.0000000000f, -0.9238796234f, -0.7071067691f}};
#endif