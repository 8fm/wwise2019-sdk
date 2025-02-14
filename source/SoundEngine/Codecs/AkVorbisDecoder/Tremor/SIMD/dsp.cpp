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

 function: PCM data vector blocking, windowing and dis/reassembly

 ********************************************************************/
#include <AK/AkPlatforms.h>
 
#include <stdlib.h> 
#include "ogg.h"
#include "mdct.h"
#include "ivorbiscodec.h"
#include "codec_internal.h"
#include "window_lookup.h"
#include <AK/Tools/Common/AkLock.h>
#include <AK/Tools/Common/AkAutoLock.h>

extern void InitFloorFunc();

int vorbis_dsp_restart(vorbis_dsp_state *v, ogg_uint32_t in_uExtraSamplesBegin, ogg_uint32_t in_uExtraSamplesEnd )
{
	v->state.out_end		= -1;
	v->state.out_begin		= -1;
	v->state.extra_samples_begin = in_uExtraSamplesBegin;
	v->state.extra_samples_end = in_uExtraSamplesEnd;

	return 0;
}

int vorbis_dsp_init(vorbis_dsp_state* v, int channels )
{
	int i;

	v->channels = channels;

#ifndef AK_VOICE_MAX_NUM_CHANNELS
	v->work = (ogg_int32_t **)AkMalign(AkMemID_Processing, sizeof(ogg_int32_t *) * channels * 2, AK_SIMD_ALIGNMENT);
	if (!v->work)
		return -1;

	v->mdctright = v->work + channels;
#endif

	InitFloorFunc();

	// otherwise vorbis_dsp_clear will take it for some valid address
	v->work[0]		= NULL;
	v->mdctright[0]	= NULL;

	// allocate the arrays
	int DctSize		= v->GetDctSize();

	// Do not count in UVM, do not alloc in UVM memory
	char* pDct		= (char*)AkMalign(AkMemID_Processing, DctSize, AK_SIMD_ALIGNMENT);
	v->mdctright[0] = (ogg_int32_t *)pDct;	// In case of error, this needs to be set so that vorbis_dsp_clear releases mem.

	// out of memory
	if(pDct == NULL)
	{
		return -1;
	}
		
	AkZeroMemLarge(pDct, DctSize);
	v->state.bShiftedDCT = true;

	// we need the size per channel now
	DctSize /= channels;
	for(i = 0 ; i < channels ; ++i)
	{
		v->mdctright[i]	= (ogg_int32_t *)pDct;
		pDct += DctSize;
	}
		
	v->state.lW=0; /* previous window size */
	v->state.W=0;  /* current window size */

	return 0;
}

void vorbis_dsp_clear(vorbis_dsp_state *v)
{
#ifndef AK_VOICE_MAX_NUM_CHANNELS
	if (v->work)
#endif
	{
		if (v->mdctright[0])
		{
			// Do not count in UVM, do not alloc in UVM memory
			AkFalign(AkMemID_Processing, v->mdctright[0]);
			v->mdctright[0] = NULL;			
		}

#ifndef AK_VOICE_MAX_NUM_CHANNELS
		AkFalign(AkMemID_Processing, v->work);
		v->work = NULL;
		v->mdctright = NULL;
#endif
	}	
}

static LOOKUP_T *_vorbis_window(int left){
  switch(left){
/*
  case 32:
    return vwin64;
  case 64:
    return vwin128;
*/
  case 128:
    return vwin256;
  case 256:
    return vwin512;
  case 512:
    return vwin1024;
  case 1024:
    return vwin2048;
  case 2048:
    return vwin4096;
/*
#ifndef LIMIT_TO_64kHz
  case 4096:
    return vwin8192;
#endif
*/
  default:
    return(0);
  }
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
						   int			end    /* samples, this frame */);

/* pcm==0 indicates we just want the pending samples, no more */
int vorbis_dsp_pcmout(vorbis_dsp_state *v,float *pcm, int samples, int idxLFE)
{
	if(v->state.out_begin < v->state.out_end)
	{
		AKASSERT( v->state.out_begin > -1 );

		int n = v->state.out_end - v->state.out_begin;
		if(pcm)
		{
			//AkPrefetchZero(pcm, samples*sizeof(ogg_int16_t));

			codec_setup_info *ci = v->csi;
			if(n > samples)
			{
				n = samples;
			}
			LOOKUP_T *Window0 = _vorbis_window(ci->blocksizes[0]>>1);
			LOOKUP_T *Window1 = _vorbis_window(ci->blocksizes[1]>>1);
			int i = 0;

			i = 0;
			do
			{
				//Reorder LFE channel to last position
				int c = i;
				if (i > idxLFE)
					c = i - 1;
				else if (i == idxLFE)
					c = v->channels - 1;

				float * pDst = pcm + c*samples;
			
				mdct_unroll_lap(ci->blocksizes[0],
					ci->blocksizes[1],
					v->state.lW,
					v->state.W,
					(float *) v->work[i],
					(float *) v->mdctright[i],
					Window0,
					Window1,
					pDst,
					v->channels,
					v->state.out_begin,
					v->state.out_begin + n);

				//At this point, we can already move the imaginary part and free a big part of the buffer.  
				//This is more optimal than waiting for the next frame in vorbis_dsp_synthesis because both source and dest buffers are still in L1 cache.
				mdct_shift_right(ci->blocksizes[v->state.W], (ogg_int32_t *) v->work[i], v->mdctright[i]);				
			}
			while ( ++i < v->channels );			
			v->state.out_begin += n;	
			v->state.bShiftedDCT = true;
		}

		return(n);
	}
	return(0);
}

void vorbis_dsp_synthesis(vorbis_dsp_state *vd,ogg_packet *op, char *pWork, int WorkSize)
{
	//Reset the work pointer area because the static buffer might have been re-allocated.
	int channelSize = WorkSize / vd->channels;
	for(int i = 0 ; i < vd->channels ; ++i)
	{
		vd->work[i] = (ogg_int32_t *)pWork;
		pWork += channelSize;
	}
	
	codec_setup_info* ci = vd->csi;

	oggpack_readinit(&vd->opb,&(op->buffer));	

	/* read our mode */
	int mode=oggpack_read(&vd->opb,1);
	AKASSERT( mode < ci->modes );

	/* shift information we still need from last window */
	vd->state.lW=vd->state.W;
	vd->state.W=ci->mode_param[mode].blockflag;

 	int iBlockSizeLW = ci->blocksizes[vd->state.lW];

	//If no frames were output in last synthesis, it means we also didn't shift the previous DCT imaginary part.  Do it now.
	if (!vd->state.bShiftedDCT)
	{ 
 		int i=0;
 		do
 		{
 			mdct_shift_right(iBlockSizeLW,vd->work[i],vd->mdctright[i]);
 		}
 		while( ++i<vd->channels );
		vd->state.bShiftedDCT = true;
	}

	if( vd->state.out_begin != -1)
	{
		vd->state.out_begin=0;
		vd->state.out_end=iBlockSizeLW/4+ci->blocksizes[vd->state.W]/4;

		if ( AK_EXPECT_FALSE( vd->state.extra_samples_begin ) )
		{
			/* partial first frame, or seek table overshoot. Strip the extra samples off */
			vd->state.out_begin = vd->state.extra_samples_begin;
			if ( vd->state.out_begin > vd->state.out_end )
			{
				vd->state.out_begin = vd->state.out_end;
				vd->state.extra_samples_begin -= vd->state.out_end;

				// If we are more than 2 windows away from target start, no
				// need to synthesize this packet.
				if ( vd->state.extra_samples_begin >= ( ogg_uint32_t )( ci->blocksizes[ 1 ] / 2 ) )
					return;
			}
			else
			{
				vd->state.extra_samples_begin = 0;
			}
		}

		if ( AK_EXPECT_FALSE( op->e_o_s ) )
		{
			/* partial last frame.  Strip the extra samples off */
			vd->state.out_end -= vd->state.extra_samples_end;
			if ( vd->state.out_end < vd->state.out_begin )
				vd->state.out_end = vd->state.out_begin;
		}
	}
	else
	{
		vd->state.out_begin=0;
		vd->state.out_end=0;

		// If we are more than 2 windows away from target start, no
		// need to synthesize this packet.
		if ( vd->state.extra_samples_begin >= ( ogg_uint32_t )( ci->blocksizes[ 1 ] / 2 ) )
			return;
	}

	/* packet decode and portions of synthesis that rely on only this block */
	mapping_inverse(vd,ci->map_param+ci->mode_param[mode].mapping);
}

void vorbis_shift_dct(vorbis_dsp_state *v)
{
	int i=0;
	do
	{
		mdct_shift_right(v->csi->blocksizes[v->state.W], v->work[i],v->mdctright[i]);
	}
	while( ++i < v->channels );
	v->state.bShiftedDCT = true;
}
