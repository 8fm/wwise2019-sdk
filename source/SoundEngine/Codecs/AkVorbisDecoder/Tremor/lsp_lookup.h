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
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2002    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function: lookup data

 ********************************************************************/

#ifndef _V_LOOKUP_DATA_H_
#define _V_LOOKUP_DATA_H_

#include "os_types.h"

#define INVSQ_LOOKUP_I_SHIFT 10
#define INVSQ_LOOKUP_I_MASK 1023
static const long INVSQ_LOOKUP_I[64+1]={
	   92682,   91966,   91267,   90583,
	   89915,   89261,   88621,   87995,
	   87381,   86781,   86192,   85616,
	   85051,   84497,   83953,   83420,
	   82897,   82384,   81880,   81385,
	   80899,   80422,   79953,   79492,
	   79039,   78594,   78156,   77726,
	   77302,   76885,   76475,   76072,
	   75674,   75283,   74898,   74519,
	   74146,   73778,   73415,   73058,
	   72706,   72359,   72016,   71679,
	   71347,   71019,   70695,   70376,
	   70061,   69750,   69444,   69141,
	   68842,   68548,   68256,   67969,
	   67685,   67405,   67128,   66855,
	   66585,   66318,   66054,   65794,
	   65536,
};

static const long INVSQ_LOOKUP_IDel[64]={
             716,     699,     684,     668,
             654,     640,     626,     614,
             600,     589,     576,     565,
             554,     544,     533,     523,
             513,     504,     495,     486,
             477,     469,     461,     453,
             445,     438,     430,     424,
             417,     410,     403,     398,
             391,     385,     379,     373,
             368,     363,     357,     352,
             347,     343,     337,     332,
             328,     324,     319,     315,
             311,     306,     303,     299,
             294,     292,     287,     284,
             280,     277,     273,     270,
             267,     264,     260,     258,
};

#define COS_LOOKUP_I_SHIFT 9
#define COS_LOOKUP_I_MASK 511
#define COS_LOOKUP_I_SZ 128
static const ogg_int32_t COS_LOOKUP_I[COS_LOOKUP_I_SZ+1]={
	   16384,   16379,   16364,   16340,
	   16305,   16261,   16207,   16143,
	   16069,   15986,   15893,   15791,
	   15679,   15557,   15426,   15286,
	   15137,   14978,   14811,   14635,
	   14449,   14256,   14053,   13842,
	   13623,   13395,   13160,   12916,
	   12665,   12406,   12140,   11866,
	   11585,   11297,   11003,   10702,
	   10394,   10080,    9760,    9434,
	    9102,    8765,    8423,    8076,
	    7723,    7366,    7005,    6639,
	    6270,    5897,    5520,    5139,
	    4756,    4370,    3981,    3590,
	    3196,    2801,    2404,    2006,
	    1606,    1205,     804,     402,
	       0,    -401,    -803,   -1204,
	   -1605,   -2005,   -2403,   -2800,
	   -3195,   -3589,   -3980,   -4369,
	   -4755,   -5138,   -5519,   -5896,
	   -6269,   -6638,   -7004,   -7365,
	   -7722,   -8075,   -8422,   -8764,
	   -9101,   -9433,   -9759,  -10079,
	  -10393,  -10701,  -11002,  -11296,
	  -11584,  -11865,  -12139,  -12405,
	  -12664,  -12915,  -13159,  -13394,
	  -13622,  -13841,  -14052,  -14255,
	  -14448,  -14634,  -14810,  -14977,
	  -15136,  -15285,  -15425,  -15556,
	  -15678,  -15790,  -15892,  -15985,
	  -16068,  -16142,  -16206,  -16260,
	  -16304,  -16339,  -16363,  -16378,
	  -16383,
};

#endif





