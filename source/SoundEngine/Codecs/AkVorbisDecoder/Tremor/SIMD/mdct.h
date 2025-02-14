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

#ifndef _OGG_SIMD_mdct_H_
#define _OGG_SIMD_mdct_H_

#include <AK/AkPlatforms.h>
 
#include "ivorbiscodec.h"

#define DATA_TYPE ogg_int32_t
#define REG_TYPE  register ogg_int32_t

extern void mdct_backward(int n, float *in, float* floor, int end);

AkForceInline void mdct_shift_right(int n, DATA_TYPE *in, DATA_TYPE *right)
{
	AKPLATFORM::AkMemCpy(right, in + n/4, n/4*sizeof(float));	
}

#endif //_OGG_SIMD_mdct_H_












