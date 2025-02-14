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

 function: channel mapping 0 implementation

 ********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ogg.h"
#include "os.h"
#include "ivorbiscodec.h"
#include "mdct.h"
#include "floor1.h"
#include "codebook.h"

static int ilog(unsigned int v){
  int ret=0;
  if(v)--v;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

/* also responsible for range checking */
int mapping_info_unpack(vorbis_info_mapping*	info,
						codec_setup_info*		ci,
						int channels,
						oggpack_buffer*			opb,
						CAkVorbisAllocator&		VorbisAllocator)
{
	int i;
	int AllocSize;
	memset(info,0,sizeof(*info));

	if(oggpack_read(opb,1))
		info->submaps=oggpack_read(opb,4)+1;
	else
		info->submaps=1;

	if(oggpack_read(opb,1))
	{
		info->coupling_steps=oggpack_read(opb,8)+1;
		int AllocSize = info->coupling_steps*sizeof(*info->coupling);
		info->coupling = (coupling_step*)VorbisAllocator.Alloc(AllocSize);

		for(i=0;i<info->coupling_steps;i++)
		{
			int testM=info->coupling[i].mag=oggpack_read_var(opb,ilog(channels));
			int testA=info->coupling[i].ang=oggpack_read_var(opb,ilog(channels));

			if(	testM<0 || 
				testA<0 || 
				testM==testA || 
				testM>=channels ||
				testA>=channels)
			{
				goto err_out;
			}
		}
	}

	if(oggpack_read(opb,2)>0)
	{
		goto err_out; /* 2,3:reserved */
	}

	if(info->submaps>1)
	{
		int AllocSize = sizeof(*info->chmuxlist)*channels;
		info->chmuxlist = (unsigned char*)VorbisAllocator.Alloc(AllocSize);

		for(i=0;i<channels;i++)
		{
			info->chmuxlist[i]=oggpack_read(opb,4);
			if(info->chmuxlist[i]>=info->submaps)
			{
				goto err_out;
			}
		}
	}

	AllocSize = sizeof(*info->submaplist)*info->submaps;
	info->submaplist = (submap*)VorbisAllocator.Alloc(AllocSize);

	for(i=0;i<info->submaps;i++)
	{
		oggpack_read(opb,8);
		info->submaplist[i].floor=oggpack_read(opb,8);
		if(info->submaplist[i].floor>=ci->floors)
		{
			goto err_out;
		}
		info->submaplist[i].residue=oggpack_read(opb,8);
		if(info->submaplist[i].residue>=ci->residues)
		{
			goto err_out;
		}
  }

  return 0;

 err_out:
  return -1;
}

int mapping_inverse(vorbis_dsp_state *vd,vorbis_info_mapping *info)
{
	codec_setup_info     *ci= vd->csi;

	int                   i,j;
	ogg_int32_t           n=ci->blocksizes[vd->state.W];

	ogg_int32_t **pcmbundle = (ogg_int32_t**)alloca(sizeof(*pcmbundle)*vd->channels);
	int          *zerobundle = (int*)alloca(sizeof(*zerobundle)*vd->channels);
	int          *nonzero	= (int*)alloca(sizeof(*nonzero)*vd->channels);
	ogg_int32_t **floormemo = (ogg_int32_t**)alloca(sizeof(*floormemo)*vd->channels);

	/* recover the spectral envelope; store it in the PCM vector for now */
	for(i=0;i<vd->channels;i++)
	{
		int submap=0;
		int floorno;

		if(info->submaps>1)
		{
			submap=info->chmuxlist[i];
		}
		floorno=info->submaplist[submap].floor;

		/* floor 1 */
		floormemo[i] = (ogg_int32_t*)alloca(sizeof(*floormemo[i])*floor1_memosize(&ci->floor_param[floorno]));

		floormemo[i]=floor1_inverse1(vd,&ci->floor_param[floorno],floormemo[i]);

		nonzero[i] = (floormemo[i] != 0) & 1;	
		AkZeroMemLarge(vd->work[i],sizeof(*vd->work[i])*n/2);		
	}

	/* channel coupling can 'dirty' the nonzero listing */
	for(i=0;i<info->coupling_steps;i++)
	{
		if(nonzero[info->coupling[i].mag] ||
			nonzero[info->coupling[i].ang])
		{
				nonzero[info->coupling[i].mag]=1; 
				nonzero[info->coupling[i].ang]=1; 
		}
	}

	/* recover the residue into our working vectors */
	for(i=0;i<info->submaps;i++)
	{
		int ch_in_bundle	= 0;

		for(j = 0 ; j < vd->channels ; j++)
		{
			if(!info->chmuxlist || info->chmuxlist[j]==i)
			{			
				if(nonzero[j])
				{
					zerobundle[ch_in_bundle]=1;
				}
				else
				{
					zerobundle[ch_in_bundle]=0;
				}
				pcmbundle[ch_in_bundle++]=vd->work[j];
			}
		}

		res_inverse(vd,ci->residue_param+info->submaplist[i].residue,pcmbundle,zerobundle,ch_in_bundle);
	}

	//for(j=0;j<vd->channels;j++)
	//_analysis_output("coupled",seq+j,vb->pcm[j],-8,n/2,0,0);

	/* channel coupling */
#if defined AKSIMD_V4F32_SUPPORTED

	/*static const AKSIMD_V4I32 kvZero = { 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0 };
	static const AKSIMD_V4I32 kvNeg1 = { -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1,  -1, -1, -1, -1 };*/
	static const AKSIMD_V4I32 kvZero = AKSIMD_SET_V4I32(0);
	static const AKSIMD_V4I32 kvNeg1 = AKSIMD_SET_V4I32(-1);
	
	/*
	Formula table:
	Mag	Ang	New_Mag	New_Ang
	+	+	Mag		Mag-Ang
	+	-	Mag+Ang Mag
	-	+	Mag		Mag+Ang
	-	-	Mag-Ang Mag

	We'll always add Ang and sub Ang to each term, depending on the result of the conditions.  
	Note that there are 2 possibilities when the result is just Mag: you can not add and not sub, 
	or add and sub.  This simplifies the conditions.
			
				Magnitude		Angle
			AddToM	SubToM	AddToM	SubToM
	+	+	0		0		0		1
	+	-	1		0		0		0
	-	+	1		1		1		0
	-	-	0		1		1		1

	Simple truth table!	
	*/
	for (i = info->coupling_steps - 1;i >= 0;i--)
	{
		ogg_int32_t *pcmM = vd->work[info->coupling[i].mag];
		ogg_int32_t *pcmA = vd->work[info->coupling[i].ang];
		AKSIMD_V4I32 *vPM = (AKSIMD_V4I32 *)pcmM;
		AKSIMD_V4I32 *vPA = (AKSIMD_V4I32 *)pcmA;
		for (j = 0; j < n / 8; j++)
		{
			AKSIMD_V4I32 vMag = vPM[j];
			AKSIMD_V4I32 vAng = vPA[j];

			AKSIMD_V4I32 vMagNegative = AKSIMD_CMPLT_V4I32(vMag, kvZero);				
			AKSIMD_V4I32 vAngPositive = AKSIMD_CMPGT_V4I32(vAng, kvNeg1);	//Including 0

			AKSIMD_V4I32 AngleSub = AKSIMD_XOR_V4I32(vMagNegative, vAngPositive);
			AKSIMD_V4I32 MagAdd = AKSIMD_XOR_V4I32(AngleSub, kvNeg1);	//NOT
			
			AKSIMD_V4I32 M = AKSIMD_ADD_V4I32(vMag, AKSIMD_AND_V4I32(MagAdd, vAng));
			M = AKSIMD_SUB_V4I32(M, AKSIMD_AND_V4I32(vMagNegative, vAng));

			AKSIMD_V4I32 A = AKSIMD_ADD_V4I32(vMag, AKSIMD_AND_V4I32(vMagNegative, vAng));
			A = AKSIMD_SUB_V4I32(A, AKSIMD_AND_V4I32(AngleSub, vAng));

			AKSIMD_STORE_V4I32(vPM + j, M);
			AKSIMD_STORE_V4I32(vPA + j, A);
		}
	}
#else
	for(i=info->coupling_steps-1;i>=0;i--)
	{
		ogg_int32_t *pcmM=vd->work[info->coupling[i].mag];
		ogg_int32_t *pcmA=vd->work[info->coupling[i].ang];

		for(j=0;j<n/2;j++)
		{
			ogg_int32_t mag=pcmM[j];
			ogg_int32_t ang=pcmA[j];	

			if(mag>0)
			{
				if(ang>0)
				{
					pcmM[j]=mag;
					pcmA[j]=mag-ang;
				}
				else
				{
					pcmA[j]=mag;
					pcmM[j] = mag + ang;
				}
			}
			else
			{
				if(ang>0)
				{
					pcmM[j]=mag;
					pcmA[j]=mag+ang;
				}
				else
				{
					pcmA[j]=mag;
					pcmM[j] = mag - ang;
				}
			}
		}
	}
#endif

	//for(j=0;j<vd->channels;j++)
	//_analysis_output("residue",seq+j,vb->pcm[j],-8,n/2,0,0);

	AK_ALIGN_SIMD(float mdctBuffer[2048+8]);	//Used for the MDCT and the Floor curve.  (Floor could overshoot by 8 because of AVX)		

	/* compute and apply spectral envelope */
	for(i=0;i<vd->channels;i++)
	{		
		int submap=0;
		int floorno;

		if(info->submaps>1)
		{
			submap=info->chmuxlist[i];
		}
		floorno=info->submaplist[submap].floor;

		vorbis_info_residue *res_info = ci->residue_param + info->submaplist[submap].residue;
		int pcmend = ci->blocksizes[vd->state.W];
		int max = pcmend >> 1;
		int end = res_info->end <= max ? res_info->end : max;
		
		/* floor 1 */
		floor1_inverse2(vd,&ci->floor_param[floorno],floormemo[i], (ogg_int32_t*) mdctBuffer, end);
		
		mdct_backward(n, (float *)vd->work[i] , mdctBuffer, end);
	}

	vd->state.bShiftedDCT = false;

	//for(j=0;j<vd->channels;j++)
	//_analysis_output("imdct",seq+j,vb->pcm[j],-24,n,0,0);

	/* all done! */

	return(0);
}
