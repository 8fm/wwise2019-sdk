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

/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis 'TREMOR' CODEC SOURCE CODE.   *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2003    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: modified discrete cosine transform prototypes

 ********************************************************************/

#ifndef _OGG_mdct_H_
#define _OGG_mdct_H_

#include "ivorbiscodec.h"
#include "misc.h"

#define DATA_TYPE ogg_int32_t
#define REG_TYPE  register ogg_int32_t

#ifdef _LOW_ACCURACY_
#define cPI3_8 (0x0062)
#define cPI2_8 (0x00b5)
#define cPI1_8 (0x00ed)
#else
#define cPI3_8 (0x30fbc54d)
#define cPI2_8 (0x5a82799a)
#define cPI1_8 (0x7641af3d)
#endif

extern void mdct_backward(int n, float *in, float *floor);

extern void mdct_unroll_lap(int n0,int n1,
				int lW,int W,
				float *in,float *right,
			    LOOKUP_T *w0,LOOKUP_T *w1,
				float *out,
				int step,
				int start,int end /* samples, this frame */);

//====================================================================================================
//====================================================================================================
inline void mdct_shift_right(int n, DATA_TYPE *in, DATA_TYPE *right)
{
	int i;
	n>>=2;
	in+=1;

	AkPrefetchZero(right, n *sizeof(DATA_TYPE));

	for(i=0;i<n;i++)
	{
		right[i]=in[i<<1];
	}
}

#endif












